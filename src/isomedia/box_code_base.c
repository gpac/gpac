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

void co64_del(GF_Box *s)
{
	GF_ChunkLargeOffsetBox *ptr;
	ptr = (GF_ChunkLargeOffsetBox *) s;
	if (ptr == NULL) return;
	if (ptr->offsets) free(ptr->offsets);
	free(ptr);
}

GF_Err co64_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 entries;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->offsets = (u64 *) malloc(ptr->nb_entries * sizeof(u64) );
	if (ptr->offsets == NULL) return GF_OUT_OF_MEM;
	ptr->alloc_size = ptr->nb_entries;
	for (entries = 0; entries < ptr->nb_entries; entries++) {
		ptr->offsets[entries] = gf_bs_read_u64(bs);
	}
	return GF_OK;
}

GF_Box *co64_New()
{
	GF_ChunkLargeOffsetBox *tmp;
	
	tmp = (GF_ChunkLargeOffsetBox *) malloc(sizeof(GF_ChunkLargeOffsetBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ChunkLargeOffsetBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_CO64;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err co64_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i = 0; i < ptr->nb_entries; i++ ) {
		gf_bs_write_u64(bs, ptr->offsets[i]);
	}
	return GF_OK;
}

GF_Err co64_Size(GF_Box *s)
{
	GF_Err e;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void cprt_del(GF_Box *s)
{
	GF_CopyrightBox *ptr = (GF_CopyrightBox *) s;
	if (ptr == NULL) return;
	if (ptr->notice)
		free(ptr->notice);
	free(ptr);
}


GF_Box *chpl_New()
{
	GF_ChapterListBox *tmp;
	
	tmp = (GF_ChapterListBox *) malloc(sizeof(GF_ChapterListBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_CopyrightBox));
	tmp->list = gf_list_new();
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_CHPL;
    tmp->version = 1;
	return (GF_Box *)tmp;
}

void chpl_del(GF_Box *s)
{
	GF_ChapterListBox *ptr = (GF_ChapterListBox *) s;
	if (ptr == NULL) return;
	while (gf_list_count(ptr->list)) {
		GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(ptr->list, 0);
		if (ce->name) free(ce->name);
		free(ce);
		gf_list_rem(ptr->list, 0);
	}
	gf_list_del(ptr->list);
	free(ptr);
}

/*this is using chpl format according to some NeroRecode samples*/
GF_Err chpl_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_ChapterEntry *ce;
	u32 nb_chaps, len, i, count;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	/*reserved or ???*/
	gf_bs_read_u32(bs);
	nb_chaps = gf_bs_read_u8(bs);

	count = 0;
	while (nb_chaps) {
		GF_SAFEALLOC(ce, GF_ChapterEntry);
		ce->start_time = gf_bs_read_u64(bs);
		len = gf_bs_read_u8(bs);
		if (len) {
			ce->name = (char *)malloc(sizeof(char)*(len+1));
			gf_bs_read_data(bs, ce->name, len);
			ce->name[len] = 0;
		} else {
			ce->name = strdup("");
		}

		for (i=0;i<count; i++) {
			GF_ChapterEntry *ace = (GF_ChapterEntry *) gf_list_get(ptr->list, i);
			if (ace->start_time >= ce->start_time) {
				gf_list_insert(ptr->list, ce, i);
				ce = NULL;
				break;
			}
		}
		if (ce) gf_list_add(ptr->list, ce);
		count++;
		nb_chaps--;
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY

GF_Err chpl_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count, i;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *) s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	count = gf_list_count(ptr->list);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u8(bs, count);
	for (i=0; i<count; i++) {
		u32 len;
		GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(ptr->list, i);
		gf_bs_write_u64(bs, ce->start_time);
		if (ce->name) {
			len = strlen(ce->name); if (len>255) len = 255;
			gf_bs_write_u8(bs, len);
			gf_bs_write_data(bs, ce->name, len);
		} else {
			gf_bs_write_u8(bs, 0);
		}
	}
	return GF_OK;
}

GF_Err chpl_Size(GF_Box *s)
{
	GF_Err e;
	u32 count, i;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 5;

	count = gf_list_count(ptr->list);
	for (i=0; i<count; i++) {
		GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(ptr->list, i);
		ptr->size += 9; /*64bit time stamp + 8bit str len*/
		if (ce->name) ptr->size += strlen(ce->name);
	}
	return GF_OK;
}

#endif


GF_Err cprt_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	gf_bs_read_int(bs, 1);
	//the spec is unclear here, just says "the value 0 is interpreted as undetermined"
	ptr->packedLanguageCode[0] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[1] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[2] = gf_bs_read_int(bs, 5);
	ptr->size -= 2;
	//but before or after compaction ?? We assume before
	if (ptr->packedLanguageCode[0] || ptr->packedLanguageCode[1] || ptr->packedLanguageCode[2]) {
		ptr->packedLanguageCode[0] += 0x60;
		ptr->packedLanguageCode[1] += 0x60;
		ptr->packedLanguageCode[2] += 0x60;
	} else {
		ptr->packedLanguageCode[0] = 'u';
		ptr->packedLanguageCode[1] = 'n';
		ptr->packedLanguageCode[2] = 'd';
	}
	if (ptr->size) {
		u32 bytesToRead = (u32) ptr->size;
		ptr->notice = (char*)malloc(bytesToRead * sizeof(char));
		if (ptr->notice == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->notice, bytesToRead);
	}
	return GF_OK;
}

GF_Box *cprt_New()
{
	GF_CopyrightBox *tmp;
	
	tmp = (GF_CopyrightBox *) malloc(sizeof(GF_CopyrightBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_CopyrightBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_CPRT;
	tmp->packedLanguageCode[0] = 'u';
	tmp->packedLanguageCode[1] = 'n';
	tmp->packedLanguageCode[2] = 'd';

	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err cprt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_CopyrightBox *ptr = (GF_CopyrightBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, 0, 1);
	if (ptr->packedLanguageCode[0]) {
		gf_bs_write_int(bs, ptr->packedLanguageCode[0] - 0x60, 5);
		gf_bs_write_int(bs, ptr->packedLanguageCode[1] - 0x60, 5);
		gf_bs_write_int(bs, ptr->packedLanguageCode[2] - 0x60, 5);
	} else {
		gf_bs_write_int(bs, 0, 15);
	}
	if (ptr->notice) {
		gf_bs_write_data(bs, ptr->notice, (unsigned long)strlen(ptr->notice) + 1);
	}
	return GF_OK;
}

GF_Err cprt_Size(GF_Box *s)
{
	GF_Err e;
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 2;
	if (ptr->notice)
		ptr->size += strlen(ptr->notice) + 1;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void ctts_del(GF_Box *s)
{
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;
	if (ptr->entries) free(ptr->entries);
	free(ptr);
}



GF_Err ctts_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	u32 sampleCount;
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;
	
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = malloc(sizeof(GF_DttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	sampleCount = 0;
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		ptr->entries[i].decodingOffset = gf_bs_read_u32(bs);
		sampleCount += ptr->entries[i].sampleCount;
	}
#ifndef GPAC_READ_ONLY
	ptr->w_LastSampleNumber = sampleCount;
#endif
	return GF_OK;
}

GF_Box *ctts_New()
{
	GF_CompositionOffsetBox *tmp;
	
	tmp = (GF_CompositionOffsetBox *) malloc(sizeof(GF_CompositionOffsetBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_CompositionOffsetBox));

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->type = GF_ISOM_BOX_TYPE_CTTS;
	return (GF_Box *) tmp;
}



//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err ctts_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries; i++ ) {
		gf_bs_write_u32(bs, ptr->entries[i].sampleCount);
		gf_bs_write_u32(bs, ptr->entries[i].decodingOffset);
	}
	return GF_OK;
}

GF_Err ctts_Size(GF_Box *s)
{
	GF_Err e;
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *) s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void url_del(GF_Box *s)
{
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) free(ptr->location);
	free(ptr);
	return;
}


GF_Err url_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->size) {
		ptr->location = (char*)malloc((u32) ptr->size);
		if (! ptr->location) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->location, (u32)ptr->size);
	}
	return GF_OK;
}

GF_Box *url_New()
{
	GF_DataEntryURLBox *tmp = (GF_DataEntryURLBox *) malloc(sizeof(GF_DataEntryURLBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataEntryURLBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_URL;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err url_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//the flag set indicates we have a string (WE HAVE TO for URLs)
    if ( !(ptr->flags & 1)) {
		if (ptr->location) {
			gf_bs_write_data(bs, ptr->location, (u32)strlen(ptr->location) + 1);
		}
	}
	return GF_OK;
}

GF_Err url_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;
	
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if ( !(ptr->flags & 1)) {
		if (ptr->location) ptr->size += 1 + strlen(ptr->location);
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void urn_del(GF_Box *s)
{
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) free(ptr->location);
	if (ptr->nameURN) free(ptr->nameURN);
	free(ptr);
}


GF_Err urn_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, to_read;
	char *tmpName;
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (! ptr->size ) return GF_OK;

	//here we have to handle that in a clever way
	to_read = (u32) ptr->size;
	tmpName = (char*)malloc(sizeof(char) * to_read);
	if (!tmpName) return GF_OUT_OF_MEM;
	//get the data
	gf_bs_read_data(bs, tmpName, to_read);

	//then get the break
	i = 0;
	while ( (tmpName[i] != 0) && (i < to_read) ) {
		i++;
	}
	//check the data is consistent
	if (i == to_read) {
		free(tmpName);
		return GF_ISOM_INVALID_FILE;
	}
	//no NULL char, URL is not specified
	if (i == to_read - 1) {
		ptr->nameURN = tmpName;
		ptr->location = NULL;
		return GF_OK;
	}
	//OK, this has both URN and URL
	ptr->nameURN = (char*)malloc(sizeof(char) * (i+1));
	if (!ptr->nameURN) {
		free(tmpName);
		return GF_OUT_OF_MEM;
	}
	ptr->location = (char*)malloc(sizeof(char) * (to_read - i - 1));
	if (!ptr->location) {
		free(tmpName);
		free(ptr->nameURN);
		ptr->nameURN = NULL;
		return GF_OUT_OF_MEM;
	}
	memcpy(ptr->nameURN, tmpName, i + 1);
	memcpy(ptr->location, tmpName + i + 1, (to_read - i - 1));
	free(tmpName);
	return GF_OK;
}

GF_Box *urn_New()
{
	GF_DataEntryURNBox *tmp = (GF_DataEntryURNBox *) malloc(sizeof(GF_DataEntryURNBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataEntryURNBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_URN;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err urn_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//the flag set indicates we have a string (WE HAVE TO for URLs)
    if ( !(ptr->flags & 1)) {
		//to check, the spec says: First name, then location
		if (ptr->nameURN) {
			gf_bs_write_data(bs, ptr->nameURN, (u32)strlen(ptr->nameURN) + 1);
		}
		if (ptr->location) {
			gf_bs_write_data(bs, ptr->location, (u32)strlen(ptr->location) + 1);
		}
	}
	return GF_OK;
}

GF_Err urn_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if ( !(ptr->flags & 1)) {
		if (ptr->nameURN) ptr->size += 1 + strlen(ptr->nameURN);
		if (ptr->location) ptr->size += 1 + strlen(ptr->location);
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void defa_del(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *) s;
	if (!s) return;
	if (ptr->data) free(ptr->data);
	free(ptr);
}


GF_Err defa_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (ptr->size > 0xFFFFFFFF) return GF_ISOM_INVALID_FILE;
	bytesToRead = (u32) (ptr->size);

	if (bytesToRead) {
		ptr->data = (char*)malloc(bytesToRead);
		if (ptr->data == NULL ) return GF_OUT_OF_MEM;
		ptr->dataSize = bytesToRead;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

//warning: we don't have any boxType, trick has to be done while creating..
GF_Box *defa_New()
{
	GF_UnknownBox *tmp = (GF_UnknownBox *) malloc(sizeof(GF_UnknownBox));
	memset(tmp, 0, sizeof(GF_UnknownBox));
	return (GF_Box *) tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err defa_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
    if (ptr->data) {
		gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Err defa_Size(GF_Box *s)
{
	GF_Err e;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void uuid_del(GF_Box *s)
{
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox *) s;
	if (!s) return;
	if (ptr->data) free(ptr->data);
	free(ptr);
}


GF_Err uuid_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox *)s;
	if (ptr->size > 0xFFFFFFFF) return GF_ISOM_INVALID_FILE;
	bytesToRead = (u32) (ptr->size);

	if (bytesToRead) {
		ptr->data = (char*)malloc(bytesToRead);
		if (ptr->data == NULL ) return GF_OUT_OF_MEM;
		ptr->dataSize = bytesToRead;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

//warning: we don't have any boxType, trick has to be done while creating..
GF_Box *uuid_New()
{
	GF_UnknownUUIDBox *tmp = (GF_UnknownUUIDBox *) malloc(sizeof(GF_UnknownUUIDBox));
	memset(tmp, 0, sizeof(GF_UnknownUUIDBox));
	tmp->type = GF_ISOM_BOX_TYPE_UUID;
	return (GF_Box *) tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err uuid_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox*)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
    if (ptr->data) {
		gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Err uuid_Size(GF_Box *s)
{
	GF_Err e;
	GF_UnknownUUIDBox*ptr = (GF_UnknownUUIDBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void dinf_del(GF_Box *s)
{
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_del((GF_Box *)ptr->dref);
	free(ptr);
}



GF_Err dinf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_DREF:
		if (ptr->dref) return GF_ISOM_INVALID_FILE;
		ptr->dref = (GF_DataReferenceBox *)a;
		return GF_OK;		
	}
	gf_isom_box_del(a);
	return GF_OK;		
}

GF_Err dinf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, dinf_AddBox);
}

GF_Box *dinf_New()
{
	GF_DataInformationBox *tmp = (GF_DataInformationBox *) malloc(sizeof(GF_DataInformationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataInformationBox));
	tmp->type = GF_ISOM_BOX_TYPE_DINF;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err dinf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->dref) {
		e = gf_isom_box_write((GF_Box *)ptr->dref, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err dinf_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	e = gf_isom_box_get_size(s);
	if (ptr->dref) {
		e = gf_isom_box_size((GF_Box *) ptr->dref);
		if (e) return e;
		ptr->size += ptr->dref->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void dref_del(GF_Box *s)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *) s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->boxList);
	free(ptr);
}


GF_Err dref_AddDataEntry(GF_DataReferenceBox *ptr, GF_Box *entry)
{
	return gf_list_add(ptr->boxList, entry);
}

GF_Err dref_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count, i;
	GF_Box *a;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	count = gf_bs_read_u32(bs);

	for ( i = 0; i < count; i++ ) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
		e = gf_list_add(ptr->boxList, a);
		if (e) return e;
		ptr->size -= a->size;
	}
	return GF_OK;
}

GF_Box *dref_New()
{
	GF_DataReferenceBox *tmp = (GF_DataReferenceBox *) malloc(sizeof(GF_DataReferenceBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataReferenceBox));

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->boxList = gf_list_new();
	if (!tmp->boxList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_DREF;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err dref_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = gf_list_count(ptr->boxList);
	gf_bs_write_u32(bs, count);
	return gf_isom_box_array_write(s, ptr->boxList, bs);
}

GF_Err dref_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	e = gf_isom_box_array_size(s, ptr->boxList);
	if (e) return e;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void edts_del(GF_Box *s)
{
	GF_EditBox *ptr = (GF_EditBox *) s;
	gf_isom_box_del((GF_Box *)ptr->editList);
	free(ptr);
}


GF_Err edts_AddBox(GF_Box *s, GF_Box *a)
{
	GF_EditBox *ptr = (GF_EditBox *)s;
	if (a->type == GF_ISOM_BOX_TYPE_ELST) {
		if (ptr->editList) return GF_BAD_PARAM;
		ptr->editList = (GF_EditListBox *)a;
		return GF_OK;
	}
	gf_isom_box_del(a);
	return GF_OK;
}


GF_Err edts_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, edts_AddBox);
}

GF_Box *edts_New()
{
	GF_EditBox *tmp;
	GF_SAFEALLOC(tmp, GF_EditBox);
	if (tmp == NULL) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_EDTS;
	return (GF_Box *) tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err edts_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_EditBox *ptr = (GF_EditBox *)s;

	//here we have a trick: if editList is empty, skip the box
	if (gf_list_count(ptr->editList->entryList)) {
		e = gf_isom_box_write_header(s, bs);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *) ptr->editList, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err edts_Size(GF_Box *s)
{
	GF_Err e;
	GF_EditBox *ptr = (GF_EditBox *)s;

	//here we have a trick: if editList is empty, skip the box
	if (! gf_list_count(ptr->editList->entryList)) {
		ptr->size = 0;
	} else {
		e = gf_isom_box_get_size(s);
		if (e) return e;
		e = gf_isom_box_size((GF_Box *)ptr->editList);
		if (e) return e;
		ptr->size += ptr->editList->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void elst_del(GF_Box *s)
{
	GF_EditListBox *ptr;
	GF_EdtsEntry *p;
	u32 nb_entries;
	u32 i;

	ptr = (GF_EditListBox *)s;
	if (ptr == NULL) return;
	nb_entries = gf_list_count(ptr->entryList);
	for (i = 0; i < nb_entries; i++) {
		p = (GF_EdtsEntry*)gf_list_get(ptr->entryList, i);
		if (p) free(p);
	}
	gf_list_del(ptr->entryList);
	free(ptr);
}




GF_Err elst_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 entries;
	s32 tr;
	u32 nb_entries;
	GF_EdtsEntry *p;
	GF_EditListBox *ptr = (GF_EditListBox *)s;
	
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	nb_entries = gf_bs_read_u32(bs);

	for (entries = 0; entries < nb_entries; entries++ ) {
		p = (GF_EdtsEntry *) malloc(sizeof(GF_EdtsEntry));
		if (!p) return GF_OUT_OF_MEM;
		if (ptr->version == 1) {
			p->segmentDuration = gf_bs_read_u64(bs);
			p->mediaTime = (s64) gf_bs_read_u64(bs);
		} else {
			p->segmentDuration = gf_bs_read_u32(bs);
			tr = gf_bs_read_u32(bs);
			p->mediaTime = (s64) tr;
		}
		p->mediaRate = gf_bs_read_u16(bs);
		gf_bs_read_u16(bs);
		gf_list_add(ptr->entryList, p);
	}
	return GF_OK;
}

GF_Box *elst_New()
{
	GF_EditListBox *tmp;
	
	tmp = (GF_EditListBox *) malloc(sizeof(GF_EditListBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_EditListBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->entryList = gf_list_new();
	if (!tmp->entryList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_ELST;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err elst_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	u32 nb_entries;
	GF_EdtsEntry *p;
	GF_EditListBox *ptr = (GF_EditListBox *)s;
	if (!ptr) return GF_BAD_PARAM;

	nb_entries = gf_list_count(ptr->entryList);
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, nb_entries);
	for (i = 0; i < nb_entries; i++ ) {
		p = (GF_EdtsEntry*)gf_list_get(ptr->entryList, i);
		if (ptr->version == 1) {
			gf_bs_write_u64(bs, p->segmentDuration);
			gf_bs_write_u64(bs, p->mediaTime);
		} else {
			gf_bs_write_u32(bs, (u32) p->segmentDuration);
			gf_bs_write_u32(bs, (s32) p->mediaTime);
		}
		gf_bs_write_u16(bs, p->mediaRate);
		gf_bs_write_u16(bs, 0);
	}
	return GF_OK;
}

GF_Err elst_Size(GF_Box *s)
{
	GF_Err e;
	u32 durtimebytes;
	u32 i, nb_entries;
	GF_EditListBox *ptr = (GF_EditListBox *)s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	//entry count
	ptr->size += 4;
	nb_entries = gf_list_count(ptr->entryList);
	ptr->version = 0;
	for (i=0; i<nb_entries; i++) {
		GF_EdtsEntry *p = (GF_EdtsEntry*)gf_list_get(ptr->entryList, i);
		if ((p->segmentDuration>0xFFFFFFFF) || (p->mediaTime>0xFFFFFFFF)) {
			ptr->version = 1;
			break;
		}
	}
	durtimebytes = (ptr->version == 1 ? 16 : 8) + 4;
	ptr->size += (nb_entries * durtimebytes);
	return GF_OK;
}


#endif //GPAC_READ_ONLY

void esds_del(GF_Box *s)
{
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	if (ptr == NULL)	return;
	if (ptr->desc) gf_odf_desc_del((GF_Descriptor *)ptr->desc);
	free(ptr);
}


GF_Err esds_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 descSize;
	char *enc_desc;
	u32 SLIsPredefined(GF_SLConfig *sl);
	GF_ESDBox *ptr = (GF_ESDBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	descSize = (u32) (ptr->size);

	if (descSize) {
		enc_desc = (char*)malloc(sizeof(char) * descSize);
		if (!enc_desc) return GF_OUT_OF_MEM;
		//get the payload
		gf_bs_read_data(bs, enc_desc, descSize);
		//send it to the OD Codec
		e = gf_odf_desc_read(enc_desc, descSize, (GF_Descriptor **) &ptr->desc);
		//OK, free our desc
		free(enc_desc);
		//we do not abbort on error, but skip the descritpor
		if (e) {
			ptr->desc = NULL;
		} else {
			/*fix broken files*/
			if (!ptr->desc->URLString) {
				if (!ptr->desc->slConfig) {
					ptr->desc->slConfig = (GF_SLConfig *) gf_odf_desc_new(GF_ODF_SLC_TAG);
					ptr->desc->slConfig->predefined = SLPredef_MP4;
				} else if (ptr->desc->slConfig->predefined != SLPredef_MP4) {
					ptr->desc->slConfig->predefined = SLPredef_MP4;
					gf_odf_slc_set_pref(ptr->desc->slConfig);
				}
			}
		}
	}
	return GF_OK;
}

GF_Box *esds_New()
{
	GF_ESDBox *tmp = (GF_ESDBox *) malloc(sizeof(GF_ESDBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ESDBox));

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ESDS;

	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err esds_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	char *enc_desc;
	u32 descSize = 0;
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	e = gf_odf_desc_write((GF_Descriptor *)ptr->desc, &enc_desc, &descSize);
	if (e) return e;
	gf_bs_write_data(bs, enc_desc, descSize);
	//free our buffer
	free(enc_desc);
	return GF_OK;
}

GF_Err esds_Size(GF_Box *s)
{
	GF_Err e;
	u32 descSize = 0;
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	descSize = gf_odf_desc_size((GF_Descriptor *)ptr->desc);
	ptr->size += descSize;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void free_del(GF_Box *s)
{
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	if (ptr->data) free(ptr->data);
	free(ptr);
}


GF_Err free_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;

	if (ptr->size > 0xFFFFFFFF) return GF_IO_ERR;
	
	bytesToRead = (u32) (ptr->size);
	
	if (bytesToRead) {
		ptr->data = (char*)malloc(bytesToRead * sizeof(char));
		gf_bs_read_data(bs, ptr->data, bytesToRead);
		ptr->dataSize = bytesToRead;
	}
	return GF_OK;
}

GF_Box *free_New()
{
	GF_FreeSpaceBox *tmp = (GF_FreeSpaceBox *) malloc(sizeof(GF_FreeSpaceBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_FreeSpaceBox));
	tmp->type = GF_ISOM_BOX_TYPE_FREE;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err free_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->dataSize)	gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	return GF_OK;
}

GF_Err free_Size(GF_Box *s)
{
	GF_Err e;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void ftyp_del(GF_Box *s)
{
	GF_FileTypeBox *ptr = (GF_FileTypeBox *) s;
	if (ptr->altBrand) free(ptr->altBrand);
	free(ptr);
}

GF_Box *ftyp_New()
{
	GF_FileTypeBox *tmp;
	
	tmp = (GF_FileTypeBox *) malloc(sizeof(GF_FileTypeBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_FileTypeBox));

	tmp->type = GF_ISOM_BOX_TYPE_FTYP;
	return (GF_Box *)tmp;
}

GF_Err ftyp_Read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;

	ptr->majorBrand = gf_bs_read_u32(bs);
	ptr->minorVersion = gf_bs_read_u32(bs);
	ptr->size -= 8;

	ptr->altCount = ( (u32) (ptr->size)) / 4;
	if (!ptr->altCount) return GF_OK;
	if (ptr->altCount * 4 != (u32) (ptr->size)) return GF_ISOM_INVALID_FILE;

	ptr->altBrand = (u32*)malloc(sizeof(u32)*ptr->altCount);
	for (i = 0; i<ptr->altCount; i++) {
		ptr->altBrand[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}



//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err ftyp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_FileTypeBox *ptr = (GF_FileTypeBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->majorBrand);
	gf_bs_write_u32(bs, ptr->minorVersion);
	for (i=0; i<ptr->altCount; i++) {
		gf_bs_write_u32(bs, ptr->altBrand[i]);
	}
	return GF_OK;
}

GF_Err ftyp_Size(GF_Box *s)
{
	GF_Err e;
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 8 + ptr->altCount * 4;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void gnrm_del(GF_Box *s)
{
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	if (ptr->data) free(ptr->data);
	free(ptr);
}

GF_Box *gnrm_New()
{
	GF_GenericSampleEntryBox *tmp = (GF_GenericSampleEntryBox *) malloc(sizeof(GF_GenericSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_GenericSampleEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_GNRM;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err gnrm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	
	//carefull we are not writing the box type but the entry type so switch for write
	ptr->type = ptr->EntryType;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GNRM;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	return GF_OK;
}

GF_Err gnrm_Size(GF_Box *s)
{
	GF_Err e;
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	s->type = ptr->EntryType;
	e = gf_isom_box_get_size(s);
	s->type = GF_ISOM_BOX_TYPE_GNRM;
	if (e) return e;
	ptr->size += 8+ptr->data_size;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void gnrv_del(GF_Box *s)
{
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	if (ptr->data) free(ptr->data);
	free(ptr);
}

GF_Box *gnrv_New()
{
	GF_GenericVisualSampleEntryBox *tmp = (GF_GenericVisualSampleEntryBox *) malloc(sizeof(GF_GenericVisualSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_GenericVisualSampleEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_GNRV;
	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err gnrv_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	
	//carefull we are not writing the box type but the entry type so switch for write
	ptr->type = ptr->EntryType;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GNRV;

	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)ptr, bs);
	gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	return GF_OK;
}

GF_Err gnrv_Size(GF_Box *s)
{
	GF_Err e;
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	s->type = ptr->EntryType;
	e = gf_isom_box_get_size(s);
	s->type = GF_ISOM_BOX_TYPE_GNRV;
	if (e) return e;
	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);
	ptr->size += ptr->data_size;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void gnra_del(GF_Box *s)
{
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	if (ptr->data) free(ptr->data);
	free(ptr);
}

GF_Box *gnra_New()
{
	GF_GenericAudioSampleEntryBox *tmp = (GF_GenericAudioSampleEntryBox *) malloc(sizeof(GF_GenericAudioSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_GenericAudioSampleEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_GNRA;
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err gnra_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	
	//carefull we are not writing the box type but the entry type so switch for write
	ptr->type = ptr->EntryType;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GNRA;

	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox *)ptr, bs);
	gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	return GF_OK;
}

GF_Err gnra_Size(GF_Box *s)
{
	GF_Err e;
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	s->type = ptr->EntryType;
	e = gf_isom_box_get_size(s);
	s->type = GF_ISOM_BOX_TYPE_GNRA;
	if (e) return e;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox *)s);
	ptr->size += ptr->data_size;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void hdlr_del(GF_Box *s)
{
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	if (ptr == NULL) return;
	if (ptr->nameUTF8) free(ptr->nameUTF8);
	free(ptr);
}


GF_Err hdlr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->reserved1 = gf_bs_read_u32(bs);
	ptr->handlerType = gf_bs_read_u32(bs);
	gf_bs_read_data(bs, (char*)ptr->reserved2, 12);
	ptr->size -= 20;
	if (ptr->size) {
		ptr->nameUTF8 = (char*)malloc((u32) ptr->size);
		if (ptr->nameUTF8 == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->nameUTF8, (u32) ptr->size);
		/*safety check in case the string is not null-terminated*/
		if (ptr->nameUTF8[ptr->size-1]) {
			char *str = (char*)malloc((u32) ptr->size + 1);
			memcpy(str, ptr->nameUTF8, (u32) ptr->size);
			str[ptr->size] = 0;
			free(ptr->nameUTF8);
			ptr->nameUTF8 = str;
		}
	}
	return GF_OK;
}

GF_Box *hdlr_New()
{
	GF_HandlerBox *tmp = (GF_HandlerBox *) malloc(sizeof(GF_HandlerBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_HandlerBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_HDLR;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err hdlr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->reserved1);
	gf_bs_write_u32(bs, ptr->handlerType);
	gf_bs_write_data(bs, (char*)ptr->reserved2, 12);
	if (ptr->nameUTF8) gf_bs_write_data(bs, ptr->nameUTF8, strlen(ptr->nameUTF8));
	/*NULL-terminated string is written*/
	gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err hdlr_Size(GF_Box *s)
{
	GF_Err e;
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 20 + 1;
	if (ptr->nameUTF8) ptr->size += strlen(ptr->nameUTF8);
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void hinf_del(GF_Box *s)
{
	GF_HintInfoBox *hinf = (GF_HintInfoBox *)s;
	gf_isom_box_array_del(hinf->boxList);
	gf_list_del(hinf->dataRates);
	free(hinf);
}

GF_Box *hinf_New()
{
	GF_HintInfoBox *tmp = (GF_HintInfoBox *)malloc(sizeof(GF_HintInfoBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_HintInfoBox));

	tmp->boxList = gf_list_new();
	if (!tmp->boxList) {
		free(tmp);
		return NULL;
	}
	tmp->dataRates = gf_list_new();
	if (!tmp->dataRates) {
		gf_list_del(tmp->boxList);
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_HINF;
	return (GF_Box *)tmp;
}

GF_Err hinf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MAXRBox *maxR;
	GF_HintInfoBox *hinf = (GF_HintInfoBox *)s;
	u32 i;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MAXR:
		i=0;
		while ((maxR = (GF_MAXRBox *)gf_list_enum(hinf->dataRates, &i))) {
			if (maxR->granularity == ((GF_MAXRBox *)a)->granularity) return GF_ISOM_INVALID_FILE;
		}
		gf_list_add(hinf->dataRates, a);
		break;
	default:
		break;
	}
	return gf_list_add(hinf->boxList, a);
}


GF_Err hinf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, hinf_AddBox);
}

#ifndef GPAC_READ_ONLY

GF_Err hinf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HintInfoBox *ptr = (GF_HintInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->boxList, bs);
}

GF_Err hinf_Size(GF_Box *s)
{
	GF_Err e;
	GF_HintInfoBox *ptr = (GF_HintInfoBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	return gf_isom_box_array_size(s, ptr->boxList);
}
#endif	

void hmhd_del(GF_Box *s)
{
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}


GF_Err hmhd_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->maxPDUSize = gf_bs_read_u16(bs);
	ptr->avgPDUSize = gf_bs_read_u16(bs);
	ptr->maxBitrate = gf_bs_read_u32(bs);
	ptr->avgBitrate = gf_bs_read_u32(bs);
	ptr->slidingAverageBitrate = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *hmhd_New()
{
	GF_HintMediaHeaderBox *tmp = (GF_HintMediaHeaderBox *) malloc(sizeof(GF_HintMediaHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_HintMediaHeaderBox));
	
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_HMHD;

	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err hmhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->maxPDUSize);
	gf_bs_write_u16(bs, ptr->avgPDUSize);
	gf_bs_write_u32(bs, ptr->maxBitrate);
	gf_bs_write_u32(bs, ptr->avgBitrate);
	gf_bs_write_u32(bs, ptr->slidingAverageBitrate);
	return GF_OK;
}

GF_Err hmhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 16;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

GF_Box *hnti_New()
{
	GF_HintTrackInfoBox *tmp = (GF_HintTrackInfoBox *)malloc(sizeof(GF_HintTrackInfoBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_HintTrackInfoBox));
	tmp->boxList = gf_list_new();
	if (!tmp->boxList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_HNTI;
	return (GF_Box *)tmp;
}

void hnti_del(GF_Box *a)
{
	GF_Box *t;
	GF_RTPBox *rtp;
	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)a;
	while (gf_list_count(ptr->boxList)) {
		t = (GF_Box*)gf_list_get(ptr->boxList, 0);
		if (t->type != GF_ISOM_BOX_TYPE_RTP) {
			gf_isom_box_del(t);
		} else {
			rtp = (GF_RTPBox *)t;
			if (rtp->sdpText) free(rtp->sdpText);
			free(rtp);
		}
		gf_list_rem(ptr->boxList, 0);
	}
	gf_list_del(ptr->boxList);
	free(ptr);
}

GF_Err hnti_AddBox(GF_HintTrackInfoBox *hnti, GF_Box *a)
{
	if (!hnti || !a) return GF_BAD_PARAM;

	switch (a->type) {
	//this is the value for GF_RTPBox - same as HintSampleEntry for RTP !!!
	case GF_ISOM_BOX_TYPE_RTP:
	case GF_ISOM_BOX_TYPE_SDP:
		if (hnti->SDP) return GF_BAD_PARAM;
		hnti->SDP = a;
		break;
	default:
		break;
	}
	return gf_list_add(hnti->boxList, a);
}

GF_Err hnti_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 type;
	u32 length;
	GF_Err e;
	GF_Box *a;
	GF_RTPBox *rtp;

	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	//WARNING: because of the HNTI at movie level, we cannot use the generic parsing scheme!
	//this because the child SDP box at the movie level has a type of RTP, used for
	//the HintSampleEntry !
	while (ptr->size) {
		//get the type of the box (4 bytes after our current position in the bitstream)
		//before parsing...
		type = gf_bs_peek_bits(bs, 32, 4);
		if (type != GF_ISOM_BOX_TYPE_RTP) {
			e = gf_isom_parse_box(&a, bs);
			if (e) return e;
			e = hnti_AddBox(ptr, a);
			if (e) return e;
			if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
			ptr->size-=a->size;
		} else {
			u32 sr;
			rtp = (GF_RTPBox*)malloc(sizeof(GF_RTPBox));
			if (!rtp) return GF_OUT_OF_MEM;
			rtp->size = gf_bs_read_u32(bs);
			rtp->type = gf_bs_read_u32(bs);
			sr = 8;
			//"ITS LENGTH IS CALCULATED BY SUBSTRACTING 8 (or 12) from the box size" - QT specs
			//this means that we don't have any NULL char as a delimiter in QT ...
			if (rtp->size == 1) return GF_BAD_PARAM;
			rtp->subType = gf_bs_read_u32(bs);
			sr += 4;
			if (rtp->subType != GF_ISOM_BOX_TYPE_SDP) return GF_NOT_SUPPORTED;
			if (rtp->size < sr) return GF_ISOM_INVALID_FILE;
			length = (u32) (rtp->size - sr);
			rtp->sdpText = (char*)malloc(sizeof(char) * (length + 1));
			if (!rtp->sdpText) {
				free(rtp);
				return GF_OUT_OF_MEM;
			}
			gf_bs_read_data(bs, rtp->sdpText, length);
			rtp->sdpText[length] = 0;
			sr += length;
			e = hnti_AddBox(ptr, (GF_Box *)rtp);
			if (e) return e;
			if (ptr->size<rtp->size) return GF_ISOM_INVALID_FILE;
			ptr->size -= rtp->size;
		}
	}
	return GF_OK;
}

#ifndef GPAC_READ_ONLY
GF_Err hnti_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, count;
	GF_Box *a;
	GF_RTPBox *rtp;

	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	count = gf_list_count(ptr->boxList);
	for (i = 0; i < count; i ++) {
		a = (GF_Box*)gf_list_get(ptr->boxList, i);
		if (a->type != GF_ISOM_BOX_TYPE_RTP) {
			e = gf_isom_box_write(a, bs);
			if (e) return e;
		} else {
			//write the GF_RTPBox by hand
			rtp = (GF_RTPBox *)a;
			e = gf_isom_box_write_header(a, bs);
			if (e) return e;
			gf_bs_write_u32(bs, rtp->subType);
			//don't write the NULL char
			gf_bs_write_data(bs, rtp->sdpText, strlen(rtp->sdpText));
		}
	}
	return GF_OK;
}


GF_Err hnti_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, count;
	GF_Box *a;
	GF_RTPBox *rtp;

	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_get_size(s);
	if (e) return e;

	count = gf_list_count(ptr->boxList);
	for (i = 0; i < count; i ++) {
		a = (GF_Box*)gf_list_get(ptr->boxList, i);
		if (a->type != GF_ISOM_BOX_TYPE_RTP) {
			e = gf_isom_box_size(a);
			if (e) return e;
		} else {
			//get the GF_RTPBox size by hand
			rtp = (GF_RTPBox *)a;
			e = gf_isom_box_get_size(a);
			if (e) return e;
			//don't count the NULL char...
			rtp->size += 4 + strlen(rtp->sdpText);
		}
		ptr->size += a->size;
	}
	return GF_OK;
}
#endif

/**********************************************************
		GF_SDPBox
**********************************************************/

void sdp_del(GF_Box *s)
{
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr->sdpText) free(ptr->sdpText);
	free(ptr);

}
GF_Err sdp_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	length = (u32) (ptr->size);
	//sdp text has no delimiter !!!
	ptr->sdpText = (char*)malloc(sizeof(char) * (length+1));
	if (!ptr->sdpText) return GF_OUT_OF_MEM;
	
	gf_bs_read_data(bs, ptr->sdpText, length);
	ptr->sdpText[length] = 0;
	return GF_OK;
}
GF_Box *sdp_New()
{
	GF_SDPBox *tmp = (GF_SDPBox *) malloc(sizeof(GF_SDPBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_SDPBox));
	tmp->type = GF_ISOM_BOX_TYPE_SDP;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err sdp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//don't write the NULL char!!!
	gf_bs_write_data(bs, ptr->sdpText, strlen(ptr->sdpText));
	return GF_OK;
}
GF_Err sdp_Size(GF_Box *s)
{
	GF_Err e;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	//don't count the NULL char!!!
	ptr->size += strlen(ptr->sdpText);
	return GF_OK;
}

#endif


/**********************************************************
		TRPY GF_Box
**********************************************************/

void trpy_del(GF_Box *s)
{
	free((GF_TRPYBox *)s);
}
GF_Err trpy_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TRPYBox *ptr = (GF_TRPYBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *trpy_New()
{
	GF_TRPYBox *tmp = (GF_TRPYBox *) malloc(sizeof(GF_TRPYBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TRPY;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY

GF_Err trpy_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TRPYBox *ptr = (GF_TRPYBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err trpy_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif

/**********************************************************
		TOTL GF_Box
**********************************************************/

void totl_del(GF_Box *s)
{
	free((GF_TRPYBox *)s);
}
GF_Err totl_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TOTLBox *ptr = (GF_TOTLBox *)s;
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *totl_New()
{
	GF_TOTLBox *tmp = (GF_TOTLBox *) malloc(sizeof(GF_TOTLBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TOTL;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}

#ifndef GPAC_READ_ONLY

GF_Err totl_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TOTLBox *ptr = (GF_TOTLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err totl_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		NUMP GF_Box
**********************************************************/

void nump_del(GF_Box *s)
{
	free((GF_NUMPBox *)s);
}
GF_Err nump_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_NUMPBox *ptr = (GF_NUMPBox *)s;
	ptr->nbPackets = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *nump_New()
{
	GF_NUMPBox *tmp = (GF_NUMPBox *) malloc(sizeof(GF_NUMPBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_NUMP;
	tmp->nbPackets = 0;
	return (GF_Box *)tmp;
}

#ifndef GPAC_READ_ONLY
GF_Err nump_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NUMPBox *ptr = (GF_NUMPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbPackets);
	return GF_OK;
}
GF_Err nump_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif


/**********************************************************
		NPCK GF_Box
**********************************************************/

void npck_del(GF_Box *s)
{
	free((GF_NPCKBox *)s);
}
GF_Err npck_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_NPCKBox *ptr = (GF_NPCKBox *)s;
	ptr->nbPackets = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *npck_New()
{
	GF_NPCKBox *tmp = (GF_NPCKBox *) malloc(sizeof(GF_NPCKBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_NPCK;
	tmp->nbPackets = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err npck_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NPCKBox *ptr = (GF_NPCKBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbPackets);
	return GF_OK;
}
GF_Err npck_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		TPYL GF_Box
**********************************************************/

void tpyl_del(GF_Box *s)
{
	free((GF_NTYLBox *)s);
}
GF_Err tpyl_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_NTYLBox *ptr = (GF_NTYLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *tpyl_New()
{
	GF_NTYLBox *tmp = (GF_NTYLBox *) malloc(sizeof(GF_NTYLBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TPYL;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err tpyl_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NTYLBox *ptr = (GF_NTYLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err tpyl_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif

/**********************************************************
		TPAY GF_Box
**********************************************************/

void tpay_del(GF_Box *s)
{
	free((GF_TPAYBox *)s);
}
GF_Err tpay_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TPAYBox *ptr = (GF_TPAYBox *)s;
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tpay_New()
{
	GF_TPAYBox *tmp = (GF_TPAYBox *) malloc(sizeof(GF_TPAYBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TPAY;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err tpay_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TPAYBox *ptr = (GF_TPAYBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err tpay_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		MAXR GF_Box
**********************************************************/

void maxr_del(GF_Box *s)
{
	free((GF_MAXRBox *)s);
}
GF_Err maxr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MAXRBox *ptr = (GF_MAXRBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	ptr->granularity = gf_bs_read_u32(bs);
	ptr->maxDataRate = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *maxr_New()
{
	GF_MAXRBox *tmp = (GF_MAXRBox *) malloc(sizeof(GF_MAXRBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_MAXR;
	tmp->granularity = tmp->maxDataRate = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err maxr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MAXRBox *ptr = (GF_MAXRBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->granularity);
	gf_bs_write_u32(bs, ptr->maxDataRate);
	return GF_OK;
}
GF_Err maxr_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif


/**********************************************************
		DMED GF_Box
**********************************************************/

void dmed_del(GF_Box *s)
{
	free((GF_DMEDBox *)s);
}
GF_Err dmed_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMEDBox *ptr = (GF_DMEDBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dmed_New()
{
	GF_DMEDBox *tmp = (GF_DMEDBox *) malloc(sizeof(GF_DMEDBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_DMED;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err dmed_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DMEDBox *ptr = (GF_DMEDBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err dmed_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif

/**********************************************************
		DIMM GF_Box
**********************************************************/

void dimm_del(GF_Box *s)
{
	free((GF_DIMMBox *)s);
}
GF_Err dimm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMMBox *ptr = (GF_DIMMBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dimm_New()
{
	GF_DIMMBox *tmp = (GF_DIMMBox *) malloc(sizeof(GF_DIMMBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_DIMM;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err dimm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DIMMBox *ptr = (GF_DIMMBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err dimm_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif

/**********************************************************
		DREP GF_Box
**********************************************************/

void drep_del(GF_Box *s)
{
	free((GF_DREPBox *)s);
}
GF_Err drep_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DREPBox *ptr = (GF_DREPBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *drep_New()
{
	GF_DREPBox *tmp = (GF_DREPBox *) malloc(sizeof(GF_DREPBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_DREP;
	tmp->nbBytes = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err drep_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DREPBox *ptr = (GF_DREPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err drep_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}
#endif



/**********************************************************
		TMIN GF_Box
**********************************************************/

void tmin_del(GF_Box *s)
{
	free((GF_TMINBox *)s);
}
GF_Err tmin_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMINBox *ptr = (GF_TMINBox *)s;
	ptr->minTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmin_New()
{
	GF_TMINBox *tmp = (GF_TMINBox *) malloc(sizeof(GF_TMINBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TMIN;
	tmp->minTime = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err tmin_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TMINBox *ptr = (GF_TMINBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->minTime);
	return GF_OK;
}
GF_Err tmin_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		TMAX GF_Box
**********************************************************/

void tmax_del(GF_Box *s)
{
	free((GF_TMAXBox *)s);
}
GF_Err tmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMAXBox *ptr = (GF_TMAXBox *)s;
	ptr->maxTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmax_New()
{
	GF_TMAXBox *tmp = (GF_TMAXBox *) malloc(sizeof(GF_TMAXBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_TMAX;
	tmp->maxTime = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err tmax_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TMAXBox *ptr = (GF_TMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxTime);
	return GF_OK;
}
GF_Err tmax_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		PMAX GF_Box
**********************************************************/

void pmax_del(GF_Box *s)
{
	free((GF_PMAXBox *)s);
}
GF_Err pmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PMAXBox *ptr = (GF_PMAXBox *)s;
	ptr->maxSize = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *pmax_New()
{
	GF_PMAXBox *tmp = (GF_PMAXBox *) malloc(sizeof(GF_PMAXBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_PMAX;
	tmp->maxSize = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err pmax_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PMAXBox *ptr = (GF_PMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxSize);
	return GF_OK;
}
GF_Err pmax_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		DMAX GF_Box
**********************************************************/

void dmax_del(GF_Box *s)
{
	free((GF_DMAXBox *)s);
}
GF_Err dmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMAXBox *ptr = (GF_DMAXBox *)s;
	ptr->maxDur = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *dmax_New()
{
	GF_DMAXBox *tmp = (GF_DMAXBox *) malloc(sizeof(GF_DMAXBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_DMAX;
	tmp->maxDur = 0;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err dmax_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DMAXBox *ptr = (GF_DMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxDur);
	return GF_OK;
}
GF_Err dmax_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


/**********************************************************
		PAYT GF_Box
**********************************************************/

void payt_del(GF_Box *s)
{
	GF_PAYTBox *payt = (GF_PAYTBox *)s;
	if (payt->payloadString) free(payt->payloadString);
	free(payt);
}
GF_Err payt_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;

	ptr->payloadCode = gf_bs_read_u32(bs);
	length = gf_bs_read_u8(bs);
	ptr->payloadString = (char*)malloc(sizeof(char) * (length+1) );
	if (! ptr->payloadString) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->payloadString, length);
	ptr->payloadString[length] = 0;
	ptr->size -= 4+length+1;
	return GF_OK;
}
GF_Box *payt_New()
{
	GF_PAYTBox *tmp = (GF_PAYTBox *) malloc(sizeof(GF_PAYTBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_PAYT;
	tmp->payloadCode = 0;
	tmp->payloadString = NULL;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err payt_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_Err e;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->payloadCode);
	len = strlen(ptr->payloadString);
	gf_bs_write_u8(bs, len);
	if (len) gf_bs_write_data(bs, ptr->payloadString, len);
	return GF_OK;
}
GF_Err payt_Size(GF_Box *s)
{
	GF_Err e;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	if (ptr->payloadString) ptr->size += strlen(ptr->payloadString) + 1;
	return GF_OK;
}
#endif


/**********************************************************
		PAYT GF_Box
**********************************************************/

void name_del(GF_Box *s)
{
	GF_NameBox *name = (GF_NameBox *)s;
	if (name->string) free(name->string);
	free(name);
}
GF_Err name_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_NameBox *ptr = (GF_NameBox *)s;

	length = (u32) (ptr->size);
	ptr->string = (char*)malloc(sizeof(char) * length);
	if (! ptr->string) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->string, length);
	return GF_OK;
}
GF_Box *name_New()
{
	GF_NameBox *tmp = (GF_NameBox *) malloc(sizeof(GF_NameBox));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_NAME;
	tmp->string = NULL;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err name_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NameBox *ptr = (GF_NameBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->string) {
		gf_bs_write_data(bs, ptr->string, strlen(ptr->string) + 1);
	}
	return GF_OK;
}
GF_Err name_Size(GF_Box *s)
{
	GF_Err e;
	GF_NameBox *ptr = (GF_NameBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (ptr->string) ptr->size += strlen(ptr->string) + 1;
	return GF_OK;
}
#endif

void iods_del(GF_Box *s)
{
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;
	if (ptr == NULL) return;
	if (ptr->descriptor) gf_odf_desc_del(ptr->descriptor);
	free(ptr);
}


GF_Err iods_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 descSize;
	char *desc;
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	//use the OD codec...
	descSize = (u32) (ptr->size);
	desc = (char*)malloc(sizeof(char) * descSize);
	gf_bs_read_data(bs, desc, descSize);
	e = gf_odf_desc_read(desc, descSize, &ptr->descriptor);
	//OK, free our desc
	free(desc);
	return GF_OK;
}

GF_Box *iods_New()
{
	GF_ObjectDescriptorBox *tmp = (GF_ObjectDescriptorBox *) malloc(sizeof(GF_ObjectDescriptorBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ObjectDescriptorBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_IODS;
	return (GF_Box *)tmp;
}



//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err iods_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 descSize;
	char *desc;
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//call our OD codec
	e = gf_odf_desc_write(ptr->descriptor, &desc, &descSize);
	if (e) return e;
	gf_bs_write_data(bs, desc, descSize);
	//and free our stuff maybe!!
	free(desc);
	return GF_OK;
}

GF_Err iods_Size(GF_Box *s)
{
	GF_Err e;
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += gf_odf_desc_size(ptr->descriptor);
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void mdat_del(GF_Box *s)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	if (!s) return;
	
	if (ptr->data) free(ptr->data);
	free(ptr);
}


GF_Err mdat_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	ptr->dataSize = s->size;
	//then skip these bytes
	gf_bs_skip_bytes(bs, ptr->dataSize);
	return GF_OK;
}

GF_Box *mdat_New()
{
	GF_MediaDataBox *tmp = (GF_MediaDataBox *) malloc(sizeof(GF_MediaDataBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MediaDataBox));
	tmp->type = GF_ISOM_BOX_TYPE_MDAT;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mdat_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	//make sure we have some data ...
	//if not, we handle that independantly (edit files)
	if (ptr->data) {
		gf_bs_write_data(bs, ptr->data, (u32) ptr->dataSize);
	}
	return GF_OK;
}

GF_Err mdat_Size(GF_Box *s)
{
	GF_Err e;
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void mdhd_del(GF_Box *s)
{
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}

GF_Err mdhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ptr->creationTime = gf_bs_read_u32(bs);
		ptr->modificationTime = gf_bs_read_u32(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u32(bs);
	}
	//our padding bit
	gf_bs_read_int(bs, 1);
	//the spec is unclear here, just says "the value 0 is interpreted as undetermined"
	ptr->packedLanguage[0] = gf_bs_read_int(bs, 5);
	ptr->packedLanguage[1] = gf_bs_read_int(bs, 5);
	ptr->packedLanguage[2] = gf_bs_read_int(bs, 5);
	//but before or after compaction ?? We assume before
	if (ptr->packedLanguage[0] || ptr->packedLanguage[1] || ptr->packedLanguage[2]) {
		ptr->packedLanguage[0] += 0x60;
		ptr->packedLanguage[1] += 0x60;
		ptr->packedLanguage[2] += 0x60;
	} else {
		ptr->packedLanguage[0] = 'u';
		ptr->packedLanguage[1] = 'n';
		ptr->packedLanguage[2] = 'd';
	}
	ptr->reserved = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *mdhd_New()
{
	GF_MediaHeaderBox *tmp = (GF_MediaHeaderBox *) malloc(sizeof(GF_MediaHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MediaHeaderBox));

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->type = GF_ISOM_BOX_TYPE_MDHD;

	tmp->packedLanguage[0] = 'u';
	tmp->packedLanguage[1] = 'n';
	tmp->packedLanguage[2] = 'd';
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mdhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
    if (ptr->version == 1) {
		gf_bs_write_u64(bs, ptr->creationTime);
		gf_bs_write_u64(bs, ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->timeScale);
		gf_bs_write_u64(bs, ptr->duration);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->creationTime);
		gf_bs_write_u32(bs, (u32) ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->timeScale);
		gf_bs_write_u32(bs, (u32) ptr->duration);
	}
	//SPECS: BIT(1) of padding
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, ptr->packedLanguage[0] - 0x60, 5);
	gf_bs_write_int(bs, ptr->packedLanguage[1] - 0x60, 5);
	gf_bs_write_int(bs, ptr->packedLanguage[2] - 0x60, 5);
	gf_bs_write_u16(bs, ptr->reserved);
	return GF_OK;
}

GF_Err mdhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	ptr->size += (ptr->version == 1) ? 28 : 16;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void mdia_del(GF_Box *s)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	if (ptr == NULL) return;
	if (ptr->mediaHeader) gf_isom_box_del((GF_Box *)ptr->mediaHeader);
	if (ptr->information) gf_isom_box_del((GF_Box *)ptr->information);
	if (ptr->handler) gf_isom_box_del((GF_Box *)ptr->handler);
	free(ptr);
}


GF_Err mdia_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_MDHD:
	   if (ptr->mediaHeader) return GF_ISOM_INVALID_FILE;
	   ptr->mediaHeader = (GF_MediaHeaderBox *)a;
	   return GF_OK;
   
	case GF_ISOM_BOX_TYPE_HDLR:
	   if (ptr->handler) return GF_ISOM_INVALID_FILE;
	   ptr->handler = (GF_HandlerBox *)a;
	   return GF_OK;
   
	case GF_ISOM_BOX_TYPE_MINF:
	   if (ptr->information) return GF_ISOM_INVALID_FILE;
	   ptr->information = (GF_MediaInformationBox *)a;
	   return GF_OK;
	}
	gf_isom_box_del(a);
	return GF_OK;
}


GF_Err mdia_Read(GF_Box *s, GF_BitStream *bs)
{	
	return gf_isom_read_box_list(s, bs, mdia_AddBox);
}

GF_Box *mdia_New()
{   
	GF_MediaBox *tmp = (GF_MediaBox *) malloc(sizeof(GF_MediaBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MediaBox));
	tmp->type = GF_ISOM_BOX_TYPE_MDIA;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mdia_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//Header first
	if (ptr->mediaHeader) {
		e = gf_isom_box_write((GF_Box *) ptr->mediaHeader, bs);
		if (e) return e;
	}
	//then handler
	if (ptr->handler) {
		e = gf_isom_box_write((GF_Box *) ptr->handler, bs);
		if (e) return e;
	}
	if (ptr->information) {
		e = gf_isom_box_write((GF_Box *) ptr->information, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err mdia_Size(GF_Box *s)
{
	GF_Err e;
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	if (ptr->mediaHeader) {
		e = gf_isom_box_size((GF_Box *) ptr->mediaHeader);
		if (e) return e;
		ptr->size += ptr->mediaHeader->size;
	}	
	if (ptr->handler) {
		e = gf_isom_box_size((GF_Box *) ptr->handler);
		if (e) return e;
		ptr->size += ptr->handler->size;
	}
	if (ptr->information) {
		e = gf_isom_box_size((GF_Box *) ptr->information);
		if (e) return e;
		ptr->size += ptr->information->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY


#ifndef	GPAC_ISOM_NO_FRAGMENTS

void mfhd_del(GF_Box *s)
{
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}

GF_Err mfhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->sequence_number = gf_bs_read_u32(bs);
	if (!ptr->sequence_number) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

GF_Box *mfhd_New()
{
	GF_MovieFragmentHeaderBox *tmp = (GF_MovieFragmentHeaderBox *) malloc(sizeof(GF_MovieFragmentHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MovieFragmentHeaderBox));
	tmp->type = GF_ISOM_BOX_TYPE_MFHD;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err mfhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->sequence_number);
	return GF_OK;
}

GF_Err mfhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}



#endif /*GPAC_READ_ONLY*/

#endif /*GPAC_ISOM_NO_FRAGMENTS*/


void minf_del(GF_Box *s)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	if (ptr == NULL) return;

	//if we have a Handler not self-contained, delete it (the self-contained belongs to the movie)
	if (ptr->dataHandler) {
		gf_isom_datamap_close(ptr);
	}
	if (ptr->InfoHeader) gf_isom_box_del((GF_Box *)ptr->InfoHeader);
	if (ptr->dataInformation) gf_isom_box_del((GF_Box *)ptr->dataInformation);
	if (ptr->sampleTable) gf_isom_box_del((GF_Box *)ptr->sampleTable);
	gf_isom_box_array_del(ptr->boxes);
	free(ptr);
}

GF_Err minf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_NMHD:
	case GF_ISOM_BOX_TYPE_VMHD:
	case GF_ISOM_BOX_TYPE_SMHD:
	case GF_ISOM_BOX_TYPE_HMHD:
	case GF_ISOM_BOX_TYPE_GMHD:
		if (ptr->InfoHeader) return GF_ISOM_INVALID_FILE;
		ptr->InfoHeader = a;
		return GF_OK;
	
	case GF_ISOM_BOX_TYPE_DINF:
		if (ptr->dataInformation ) return GF_ISOM_INVALID_FILE;
		ptr->dataInformation = (GF_DataInformationBox *)a;
		return GF_OK;
		
	case GF_ISOM_BOX_TYPE_STBL:
		if (ptr->sampleTable ) return GF_ISOM_INVALID_FILE;
		ptr->sampleTable = (GF_SampleTableBox *)a;
		return GF_OK;
	default:
		return gf_list_add(ptr->boxes, a);
	}
	return GF_OK;
}


GF_Err minf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, minf_AddBox);
}

GF_Box *minf_New()
{
	GF_MediaInformationBox *tmp = (GF_MediaInformationBox *) malloc(sizeof(GF_MediaInformationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MediaInformationBox));
	tmp->type = GF_ISOM_BOX_TYPE_MINF;
	tmp->boxes = gf_list_new();
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err minf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	if (!s) return GF_BAD_PARAM;
	
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	//Header first
	if (ptr->InfoHeader) {
		e = gf_isom_box_write((GF_Box *) ptr->InfoHeader, bs);
		if (e) return e;
	}	
	//then dataInfo
	if (ptr->dataInformation) {
		e = gf_isom_box_write((GF_Box *) ptr->dataInformation, bs);
		if (e) return e;
	}	
	//then sampleTable
	if (ptr->sampleTable) {
		e = gf_isom_box_write((GF_Box *) ptr->sampleTable, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->boxes, bs);
}

GF_Err minf_Size(GF_Box *s)
{
	GF_Err e;
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (ptr->InfoHeader) {
		e = gf_isom_box_size((GF_Box *) ptr->InfoHeader);
		if (e) return e;
		ptr->size += ptr->InfoHeader->size;
	}	
	if (ptr->dataInformation) {
		e = gf_isom_box_size((GF_Box *) ptr->dataInformation);
		if (e) return e;
		ptr->size += ptr->dataInformation->size;
	}	
	if (ptr->sampleTable) {
		e = gf_isom_box_size((GF_Box *) ptr->sampleTable);
		if (e) return e;
		ptr->size += ptr->sampleTable->size;
	}	
	return gf_isom_box_array_size(s, ptr->boxes);
}

#endif //GPAC_READ_ONLY

#ifndef	GPAC_ISOM_NO_FRAGMENTS

void moof_del(GF_Box *s)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	if (ptr == NULL) return;

	if (ptr->mfhd) gf_isom_box_del((GF_Box *) ptr->mfhd);
	gf_isom_box_array_del(ptr->TrackList);
	free(ptr);
}

GF_Err moof_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MFHD:
		if (ptr->mfhd) return GF_ISOM_INVALID_FILE;
		ptr->mfhd = (GF_MovieFragmentHeaderBox *) a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRAF:
		return gf_list_add(ptr->TrackList, a);
	default:
		return GF_ISOM_INVALID_FILE;
	}
}

GF_Err moof_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, moof_AddBox);
}

GF_Box *moof_New()
{
	GF_MovieFragmentBox *tmp = (GF_MovieFragmentBox *) malloc(sizeof(GF_MovieFragmentBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MovieFragmentBox));
	tmp->type = GF_ISOM_BOX_TYPE_MOOF;
	tmp->TrackList = gf_list_new();
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err moof_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *) s;
	if (!s) return GF_BAD_PARAM;
	
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//Header First
	if (ptr->mfhd) {
		e = gf_isom_box_write((GF_Box *) ptr->mfhd, bs);
		if (e) return e;
	}
	//then the track list
	return gf_isom_box_array_write(s, ptr->TrackList, bs);
}

GF_Err moof_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	
	if (ptr->mfhd) {
		e = gf_isom_box_size((GF_Box *) ptr->mfhd);
		if (e) return e;
		ptr->size += ptr->mfhd->size;
	}
	return gf_isom_box_array_size(s, ptr->TrackList);
}



#endif //GPAC_READ_ONLY


#endif 

void moov_del(GF_Box *s)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	if (ptr == NULL) return;

	if (ptr->mvhd) gf_isom_box_del((GF_Box *)ptr->mvhd);
	if (ptr->meta) gf_isom_box_del((GF_Box *)ptr->meta);
	if (ptr->iods) gf_isom_box_del((GF_Box *)ptr->iods);
	if (ptr->udta) gf_isom_box_del((GF_Box *)ptr->udta);
#ifndef	GPAC_ISOM_NO_FRAGMENTS
	if (ptr->mvex) gf_isom_box_del((GF_Box *)ptr->mvex);
#endif

	gf_isom_box_array_del(ptr->trackList);
	gf_isom_box_array_del(ptr->boxes);
	free(ptr);
}


GF_Err moov_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	switch (a->type ) {
	case GF_ISOM_BOX_TYPE_IODS:
		if (ptr->iods) return GF_ISOM_INVALID_FILE;
		ptr->iods = (GF_ObjectDescriptorBox *)a;
		//if no IOD, delete the box
		if (!ptr->iods->descriptor) {
			ptr->iods = NULL;
			gf_isom_box_del(a);
		}
		return GF_OK;
		
	case GF_ISOM_BOX_TYPE_MVHD:
		if (ptr->mvhd) return GF_ISOM_INVALID_FILE;
		ptr->mvhd = (GF_MovieHeaderBox *)a;
		return GF_OK;
		
	case GF_ISOM_BOX_TYPE_UDTA:
		if (ptr->udta) return GF_ISOM_INVALID_FILE;
		ptr->udta = (GF_UserDataBox *)a;
		return GF_OK;

#ifndef	GPAC_ISOM_NO_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX:
		if (ptr->mvex) return GF_ISOM_INVALID_FILE;
		ptr->mvex = (GF_MovieExtendsBox *)a;
		ptr->mvex->mov = ptr->mov;
		return GF_OK;
#endif

	case GF_ISOM_BOX_TYPE_META:
		if (ptr->meta) return GF_ISOM_INVALID_FILE;
		ptr->meta = (GF_MetaBox *)a;
		return GF_OK;
		
	case GF_ISOM_BOX_TYPE_TRAK:
		//set our pointer to this obj
		((GF_TrackBox *)a)->moov = ptr;
		return gf_list_add(ptr->trackList, a);
	default:
		return gf_list_add(ptr->boxes, a);
	}
}



GF_Err moov_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, moov_AddBox);
}

GF_Box *moov_New()
{
	GF_MovieBox *tmp = (GF_MovieBox *) malloc(sizeof(GF_MovieBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MovieBox));
	tmp->trackList = gf_list_new();
	if (!tmp->trackList) {
		free(tmp);
		return NULL;
	}
	tmp->boxes = gf_list_new();
	if (!tmp->boxes) {
		gf_list_del(tmp->trackList);
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_MOOV;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err moov_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->mvhd) {
		e = gf_isom_box_write((GF_Box *) ptr->mvhd, bs);
		if (e) return e;
	}
	if (ptr->iods) {
		e = gf_isom_box_write((GF_Box *) ptr->iods, bs);
		if (e) return e;
	}
	if (ptr->meta) {
		e = gf_isom_box_write((GF_Box *) ptr->meta, bs);
		if (e) return e;
	}
#ifndef	GPAC_ISOM_NO_FRAGMENTS
	if (ptr->mvex) {
		e = gf_isom_box_write((GF_Box *) ptr->mvex, bs);
		if (e) return e;
	}
#endif 

	e = gf_isom_box_array_write(s, ptr->trackList, bs);
	if (e) return e;

	if (ptr->udta) {
		e = gf_isom_box_write((GF_Box *) ptr->udta, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->boxes, bs);
}

GF_Err moov_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	if (ptr->mvhd) {
		e = gf_isom_box_size((GF_Box *) ptr->mvhd);
		if (e) return e;
		ptr->size += ptr->mvhd->size;
	}
	if (ptr->iods) {
		e = gf_isom_box_size((GF_Box *) ptr->iods);
		if (e) return e;
		ptr->size += ptr->iods->size;
	}
	if (ptr->udta) {
		e = gf_isom_box_size((GF_Box *) ptr->udta);
		if (e) return e;
		ptr->size += ptr->udta->size;
	}
	if (ptr->meta) {
		e = gf_isom_box_size((GF_Box *) ptr->meta);
		if (e) return e;
		ptr->size += ptr->meta->size;
	}
#ifndef	GPAC_ISOM_NO_FRAGMENTS
	if (ptr->mvex) {
		e = gf_isom_box_size((GF_Box *) ptr->mvex);
		if (e) return e;
		ptr->size += ptr->mvex->size;
	}
#endif

	e = gf_isom_box_array_size(s, ptr->trackList);
	if (e) return e;
	return gf_isom_box_array_size(s, ptr->boxes);
}

#endif //GPAC_READ_ONLY

void mp4a_del(GF_Box *s)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
}

GF_Err mp4a_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) return GF_ISOM_INVALID_FILE;
		ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		if (ptr->protection_info) return GF_ISOM_INVALID_FILE;
		ptr->protection_info = (GF_ProtectionInfoBox*)a;
		break;
	case GF_4CC('w','a','v','e'):
		if (ptr->esd) return GF_ISOM_INVALID_FILE;
		/*HACK for QT files: get the esds box from the track*/
		{
			GF_UnknownBox *wave = (GF_UnknownBox *)a;
			u32 offset = 0;
			while ((wave->data[offset+4]!='e') && (wave->data[offset+5]!='s')) {
				offset++;
				if (offset == wave->dataSize) break;
			}
			if (offset < wave->dataSize) {
				GF_Box *a;
				GF_Err e;
				GF_BitStream *bs = gf_bs_new(wave->data+offset, wave->dataSize-offset, GF_BITSTREAM_READ);
				e = gf_isom_parse_box(&a, bs);
				gf_bs_del(bs);
				ptr->esd = (GF_ESDBox *)a;
			}
			gf_isom_box_del(a);
		}
		break;
	default:
		gf_isom_box_del(a);
		break;
	}
	return GF_OK;
}
GF_Err mp4a_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGAudioSampleEntryBox *ptr;
	char *data;
	u32 i, size;
	GF_Err e;
	u64 pos;

	e = gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox*)s, bs);
	if (e) return e;
	pos = gf_bs_get_position(bs);
	size = (u32) s->size;

	e = gf_isom_read_box_list(s, bs, mp4a_AddBox);
	if (!e) return GF_OK;

	/*hack for some weird files (possibly recorded with live.com tools, needs further investigations)*/	
	ptr = (GF_MPEGAudioSampleEntryBox *)s;
	gf_bs_seek(bs, pos);
	data = (char*)malloc(sizeof(char) * size);
	gf_bs_read_data(bs, data, size);
	for (i=0; i<size-8; i++) {
		if (GF_4CC(data[i+4], data[i+5], data[i+6], data[i+7]) == GF_ISOM_BOX_TYPE_ESDS) {
			GF_BitStream *mybs = gf_bs_new(data + i, size - i, GF_BITSTREAM_READ);
			e = gf_isom_parse_box((GF_Box **)&ptr->esd, mybs);
			gf_bs_del(mybs);
			break;
		}
	}
	free(data);
	return e;
}

GF_Box *mp4a_New()
{
	GF_MPEGAudioSampleEntryBox *tmp = (GF_MPEGAudioSampleEntryBox *)malloc(sizeof(GF_MPEGAudioSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MPEGAudioSampleEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_MP4A;
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

GF_Box *enca_New() 
{
	GF_Box *tmp = mp4a_New();
	tmp->type = GF_ISOM_BOX_TYPE_ENCA;
	return tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mp4a_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
	if (e) return e;
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCA)) {
		e = gf_isom_box_write((GF_Box *)ptr->protection_info, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err mp4a_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);

	e = gf_isom_box_size((GF_Box *)ptr->esd);
	if (e) return e;
	ptr->size += ptr->esd->size;
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCA)) {
		e = gf_isom_box_size((GF_Box *)ptr->protection_info);
		if (e) return e;
		ptr->size += ptr->protection_info->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void mp4s_del(GF_Box *s)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
}

GF_Err mp4s_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) return GF_ISOM_INVALID_FILE;
		ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		if (ptr->protection_info) return GF_ISOM_INVALID_FILE;
		ptr->protection_info = (GF_ProtectionInfoBox*)a;
		break;
	default:
		gf_isom_box_del(a);
		break;
	}
	return GF_OK;
}

GF_Err mp4s_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	ptr->size -= 8;
	return gf_isom_read_box_list(s, bs, mp4s_AddBox);
}

GF_Box *mp4s_New()
{
	GF_MPEGSampleEntryBox *tmp = (GF_MPEGSampleEntryBox *) malloc(sizeof(GF_MPEGSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MPEGSampleEntryBox));

	tmp->type = GF_ISOM_BOX_TYPE_MP4S;
	return (GF_Box *)tmp;
}

GF_Box *encs_New()
{
	GF_Box *tmp = mp4s_New();
	tmp->type = GF_ISOM_BOX_TYPE_ENCS;
	return tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mp4s_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
	if (e) return e;
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCS)) {
		e = gf_isom_box_write((GF_Box *)ptr->protection_info, bs);
	}
	return e;
}

GF_Err mp4s_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 8;
	e = gf_isom_box_size((GF_Box *)ptr->esd);
	if (e) return e;
	ptr->size += ptr->esd->size;
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCS)) {
		e = gf_isom_box_size((GF_Box *)ptr->protection_info);
		if (e) return e;
		ptr->size += ptr->protection_info->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void mp4v_del(GF_Box *s)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->avc_config) gf_isom_box_del((GF_Box *) ptr->avc_config);
	if (ptr->bitrate) gf_isom_box_del((GF_Box *) ptr->bitrate);
	if (ptr->descr) gf_isom_box_del((GF_Box *) ptr->descr);
	if (ptr->ipod_ext) gf_isom_box_del((GF_Box *)ptr->ipod_ext);
	/*for publishing*/
	if (ptr->emul_esd) gf_odf_desc_del((GF_Descriptor *)ptr->emul_esd);

	if (ptr->pasp) gf_isom_box_del((GF_Box *)ptr->pasp);

	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
}

GF_Err mp4v_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) return GF_ISOM_INVALID_FILE;
		ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		if (ptr->protection_info) return GF_ISOM_INVALID_FILE;
		ptr->protection_info = (GF_ProtectionInfoBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_AVCC:
		if (ptr->avc_config) return GF_ISOM_INVALID_FILE;
		ptr->avc_config = (GF_AVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_BTRT:
		if (ptr->bitrate) return GF_ISOM_INVALID_FILE;
		ptr->bitrate = (GF_MPEG4BitRateBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_M4DS:
		if (ptr->descr) return GF_ISOM_INVALID_FILE;
		ptr->descr = (GF_MPEG4ExtensionDescriptorsBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_UUID:
		if (ptr->ipod_ext) return GF_ISOM_INVALID_FILE;
		ptr->ipod_ext = (GF_UnknownUUIDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_PASP:
		if (ptr->pasp) return GF_ISOM_INVALID_FILE;
		ptr->pasp = (GF_PixelAspectRatioBox *)a;
		break;
	default:
		gf_isom_box_del(a);
		break;
	}
	return GF_OK;
}

GF_Err mp4v_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGVisualSampleEntryBox *mp4v = (GF_MPEGVisualSampleEntryBox*)s;
	GF_Err e;
	e = gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *)s, bs);
	if (e) return e;
	e = gf_isom_read_box_list(s, bs, mp4v_AddBox);
	if (e) return e;
	/*this is an AVC sample desc*/
	if (mp4v->avc_config) AVC_RewriteESDescriptor(mp4v);
	return GF_OK;
}

static GF_Box *mp4v_encv_avc1_new(u32 type)
{
	GF_MPEGVisualSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_MPEGVisualSampleEntryBox);
	if (tmp == NULL) return NULL;

	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	tmp->type = type;
	return (GF_Box *)tmp;
}
GF_Box *mp4v_New()
{
	return mp4v_encv_avc1_new(GF_ISOM_BOX_TYPE_MP4V);
}

GF_Box *avc1_New()
{
	return mp4v_encv_avc1_new(GF_ISOM_BOX_TYPE_AVC1);
}

GF_Box *encv_New()
{
	return mp4v_encv_avc1_new(GF_ISOM_BOX_TYPE_ENCV);
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mp4v_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)s, bs);

	if (ptr->pasp) {
		e = gf_isom_box_write((GF_Box *)ptr->pasp, bs);
		if (e) return e;
	}

	/*mp4v*/
	if (ptr->esd) {
		e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
		if (e) return e;
	}
	/*avc1*/
	else {
		if (ptr->avc_config && ptr->avc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->avc_config, bs);
			if (e) return e;
		}
		if (ptr->ipod_ext)	{
			e = gf_isom_box_write((GF_Box *) ptr->ipod_ext, bs);
			if (e) return e;
		}
		if (ptr->bitrate) {
			e = gf_isom_box_write((GF_Box *) ptr->bitrate, bs);
			if (e) return e;
		}
		if (ptr->descr)	{
			e = gf_isom_box_write((GF_Box *) ptr->descr, bs);
			if (e) return e;
		}
	}
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCV)) {
		e = gf_isom_box_write((GF_Box *)ptr->protection_info, bs);
		if (e) return e;
	}
	return e;
}

GF_Err mp4v_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);

	if (ptr->esd) {
		e = gf_isom_box_size((GF_Box *)ptr->esd);
		if (e) return e;
		ptr->size += ptr->esd->size;
	} else if (ptr->avc_config) {
		if (ptr->avc_config && ptr->avc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->avc_config); 
			if (e) return e;
			ptr->size += ptr->avc_config->size;
		}
		if (ptr->ipod_ext) {
			e = gf_isom_box_size((GF_Box *) ptr->ipod_ext);
			if (e) return e;
			ptr->size += ptr->ipod_ext->size;
		}
		if (ptr->bitrate) {
			e = gf_isom_box_size((GF_Box *) ptr->bitrate);
			if (e) return e;
			ptr->size += ptr->bitrate->size;
		}
		if (ptr->descr) {
			e = gf_isom_box_size((GF_Box *) ptr->descr);
			if (e) return e;
			ptr->size += ptr->descr->size;
		}
	} else {
		return GF_ISOM_INVALID_FILE;
	}
	if (ptr->pasp) {
		e = gf_isom_box_size((GF_Box *)ptr->pasp);
		if (e) return e;
		ptr->size += ptr->pasp->size;
	}
	if (ptr->protection_info && (ptr->type == GF_ISOM_BOX_TYPE_ENCV)) {
		e = gf_isom_box_size((GF_Box *)ptr->protection_info);
		if (e) return e;
		ptr->size += ptr->protection_info->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY



#ifndef	GPAC_ISOM_NO_FRAGMENTS

void mvex_del(GF_Box *s)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	if (ptr == NULL) return;
	if (ptr->mehd) gf_isom_box_del((GF_Box*)ptr->mehd);
	gf_isom_box_array_del(ptr->TrackExList);
	free(ptr);
}


GF_Err mvex_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TREX:
		return gf_list_add(ptr->TrackExList, a);
	case GF_ISOM_BOX_TYPE_MEHD:
		if (ptr->mehd) break;
		ptr->mehd = (GF_MovieExtendsHeaderBox*)a;
		return GF_OK;
	}
	gf_isom_box_del(a);
	return GF_OK;
}



GF_Err mvex_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, mvex_AddBox);
}

GF_Box *mvex_New()
{
	GF_MovieExtendsBox *tmp = (GF_MovieExtendsBox *) malloc(sizeof(GF_MovieExtendsBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MovieExtendsBox));
	tmp->TrackExList = gf_list_new();
	if (!tmp->TrackExList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_MVEX;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err mvex_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->TrackExList, bs);
}

GF_Err mvex_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	return gf_isom_box_array_size(s, ptr->TrackExList);
}



#endif //GPAC_READ_ONLY

GF_Box *mehd_New()
{
	GF_MovieExtendsHeaderBox *tmp;
	GF_SAFEALLOC(tmp, GF_MovieExtendsHeaderBox);
	if (tmp == NULL) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_MEHD;
	return (GF_Box *)tmp;
}
void mehd_del(GF_Box *s)
{
	free(s);
}
GF_Err mehd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->version==1) {
		ptr->fragment_duration = gf_bs_read_u64(bs);
	} else {
		ptr->fragment_duration = (u64) gf_bs_read_u32(bs);
	}
	return GF_OK;
}
#ifndef GPAC_READ_ONLY
GF_Err mehd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;
	GF_Err e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		gf_bs_write_u64(bs, ptr->fragment_duration);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->fragment_duration);
	}
	return GF_OK;
}
GF_Err mehd_Size(GF_Box *s)
{
	GF_Err e = gf_isom_full_box_get_size(s);
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;
	if (e) return e;
	ptr->version = (ptr->fragment_duration>0xFFFFFFFF) ? 1 : 0;
	s->size += (ptr->version == 1) ? 8 : 4;
	return GF_OK;
}
#endif

#endif 


void mvhd_del(GF_Box *s)
{
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}


GF_Err mvhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ptr->creationTime = gf_bs_read_u32(bs);
		ptr->modificationTime = gf_bs_read_u32(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u32(bs);
	}
	ptr->preferredRate = gf_bs_read_u32(bs);
	ptr->preferredVolume = gf_bs_read_u16(bs);
	gf_bs_read_data(bs, ptr->reserved, 10);
	ptr->matrixA = gf_bs_read_u32(bs);
	ptr->matrixB = gf_bs_read_u32(bs);
	ptr->matrixU = gf_bs_read_u32(bs);
	ptr->matrixC = gf_bs_read_u32(bs);
	ptr->matrixD = gf_bs_read_u32(bs);
	ptr->matrixV = gf_bs_read_u32(bs);
	ptr->matrixX = gf_bs_read_u32(bs);
	ptr->matrixY = gf_bs_read_u32(bs);
	ptr->matrixW = gf_bs_read_u32(bs);
	ptr->previewTime = gf_bs_read_u32(bs);
	ptr->previewDuration = gf_bs_read_u32(bs);
	ptr->posterTime = gf_bs_read_u32(bs);
	ptr->selectionTime = gf_bs_read_u32(bs);
	ptr->selectionDuration = gf_bs_read_u32(bs);
	ptr->currentTime = gf_bs_read_u32(bs);
	ptr->nextTrackID = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *mvhd_New()
{
	GF_MovieHeaderBox *tmp = (GF_MovieHeaderBox *) malloc(sizeof(GF_MovieHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MovieHeaderBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_MVHD;
	tmp->preferredRate = (1<<16);
	tmp->preferredVolume = (1<<8);

	tmp->matrixA = (1<<16);
	tmp->matrixD = (1<<16);
	tmp->matrixW = (1<<30);

	tmp->nextTrackID = 1;

	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err mvhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		gf_bs_write_u64(bs, ptr->creationTime);
		gf_bs_write_u64(bs, ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->timeScale);
		gf_bs_write_u64(bs, ptr->duration);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->creationTime);
		gf_bs_write_u32(bs, (u32) ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->timeScale);
		gf_bs_write_u32(bs, (u32) ptr->duration);
	}
	gf_bs_write_u32(bs, ptr->preferredRate);
	gf_bs_write_u16(bs, ptr->preferredVolume);
	gf_bs_write_data(bs, ptr->reserved, 10);
	gf_bs_write_u32(bs, ptr->matrixA);
	gf_bs_write_u32(bs, ptr->matrixB);
	gf_bs_write_u32(bs, ptr->matrixU);
	gf_bs_write_u32(bs, ptr->matrixC);
	gf_bs_write_u32(bs, ptr->matrixD);
	gf_bs_write_u32(bs, ptr->matrixV);
	gf_bs_write_u32(bs, ptr->matrixX);
	gf_bs_write_u32(bs, ptr->matrixY);
	gf_bs_write_u32(bs, ptr->matrixW);
	gf_bs_write_u32(bs, ptr->previewTime);
	gf_bs_write_u32(bs, ptr->previewDuration);
	gf_bs_write_u32(bs, ptr->posterTime);
	gf_bs_write_u32(bs, ptr->selectionTime);
	gf_bs_write_u32(bs, ptr->selectionDuration);
	gf_bs_write_u32(bs, ptr->currentTime);
	gf_bs_write_u32(bs, ptr->nextTrackID);
	return GF_OK;
}

GF_Err mvhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += (ptr->version == 1) ? 28 : 16;
	ptr->size += 80;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void nmhd_del(GF_Box *s)
{
	GF_MPEGMediaHeaderBox *ptr = (GF_MPEGMediaHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}



GF_Err nmhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Box *nmhd_New()
{
	GF_MPEGMediaHeaderBox *tmp = (GF_MPEGMediaHeaderBox *) malloc(sizeof(GF_MPEGMediaHeaderBox));
	if (tmp == NULL) return NULL;
	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->type = GF_ISOM_BOX_TYPE_NMHD;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err nmhd_Write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err nmhd_Size(GF_Box *s)
{
	return gf_isom_full_box_get_size(s);
}

#endif //GPAC_READ_ONLY



void padb_del(GF_Box *s)
{
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *) s;
	if (ptr == NULL) return;
	if (ptr->padbits) free(ptr->padbits);
	free(ptr);
}


GF_Err padb_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;

	e = gf_isom_full_box_read( s, bs);
	if (e) return e;

	ptr->SampleCount = gf_bs_read_u32(bs);

	ptr->padbits = (u8 *)malloc(sizeof(u8)*ptr->SampleCount);
	for (i=0; i<ptr->SampleCount; i += 2) {
		gf_bs_read_int(bs, 1);
		if (i+1 < ptr->SampleCount) {
			ptr->padbits[i+1] = gf_bs_read_int(bs, 3);
		} else {
			gf_bs_read_int(bs, 3);
		}
		gf_bs_read_int(bs, 1);
		ptr->padbits[i] = gf_bs_read_int(bs, 3);
	}
	return GF_OK;
}

GF_Box *padb_New()
{
	GF_PaddingBitsBox *tmp;
	
	tmp = (GF_PaddingBitsBox *) malloc(sizeof(GF_PaddingBitsBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_PaddingBitsBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_FADB;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err padb_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->SampleCount, 32);
	
	for (i=0 ; i<ptr->SampleCount; i += 2) {
		gf_bs_write_int(bs, 0, 1);
		if (i+1 < ptr->SampleCount) {
			gf_bs_write_int(bs, ptr->padbits[i+1], 3);
		} else {
			gf_bs_write_int(bs, 0, 3);
		}
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, ptr->padbits[i], 3);
	}
	return GF_OK;
}

GF_Err padb_Size(GF_Box *s)
{
	GF_Err e;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if (ptr->SampleCount) ptr->size += (ptr->SampleCount + 1) / 2;


	return GF_OK;
}

#endif //GPAC_READ_ONLY


void rely_del(GF_Box *s)
{
	GF_RelyHintBox *rely = (GF_RelyHintBox *)s;
	free(rely);
}

GF_Err rely_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_RelyHintBox *ptr = (GF_RelyHintBox *)s;
	ptr->reserved = gf_bs_read_int(bs, 6);
	ptr->prefered = gf_bs_read_int(bs, 1);
	ptr->required = gf_bs_read_int(bs, 1);
	return GF_OK;
}

GF_Box *rely_New()
{
	GF_RelyHintBox *tmp = (GF_RelyHintBox *)malloc(sizeof(GF_RelyHintBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_RelyHintBox));
	tmp->type = GF_ISOM_BOX_TYPE_RELY;

	return (GF_Box *)tmp;
}


#ifndef GPAC_READ_ONLY
GF_Err rely_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RelyHintBox *ptr = (GF_RelyHintBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->reserved, 6);
	gf_bs_write_int(bs, ptr->prefered, 1);
	gf_bs_write_int(bs, ptr->required, 1);
	return GF_OK;
}

GF_Err rely_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 1;
	return GF_OK;
}
#endif


void rtpo_del(GF_Box *s)
{
	GF_RTPOBox *rtpo = (GF_RTPOBox *)s;
	free(rtpo);
}

GF_Err rtpo_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_RTPOBox *ptr = (GF_RTPOBox *)s;
	ptr->timeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *rtpo_New()
{
	GF_RTPOBox *tmp = (GF_RTPOBox *) malloc(sizeof(GF_RTPOBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_RTPOBox));
	tmp->type = GF_ISOM_BOX_TYPE_RTPO;
	return (GF_Box *)tmp;
}
#ifndef GPAC_READ_ONLY
GF_Err rtpo_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RTPOBox *ptr = (GF_RTPOBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	//here we have no pb, just remembed that some entries will have to
	//be 4-bytes aligned ...
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->timeOffset);
	return GF_OK;
}

GF_Err rtpo_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif

void smhd_del(GF_Box *s)
{
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	if (ptr == NULL ) return;
	free(ptr);
}


GF_Err smhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->reserved = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *smhd_New()
{
	GF_SoundMediaHeaderBox *tmp = (GF_SoundMediaHeaderBox *) malloc(sizeof(GF_SoundMediaHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SoundMediaHeaderBox));
	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->type = GF_ISOM_BOX_TYPE_SMHD;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err smhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->reserved);
	return GF_OK;
}

GF_Err smhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->reserved = 0;
	ptr->size += 4;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void snro_del(GF_Box *s)
{
	GF_SeqOffHintEntryBox *snro = (GF_SeqOffHintEntryBox *)s;
	free(snro);
}

GF_Err snro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_SeqOffHintEntryBox *ptr = (GF_SeqOffHintEntryBox *)s;
	ptr->SeqOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *snro_New()
{
	GF_SeqOffHintEntryBox *tmp = (GF_SeqOffHintEntryBox *) malloc(sizeof(GF_SeqOffHintEntryBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_SeqOffHintEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_SNRO;
	return (GF_Box *)tmp;
}


#ifndef GPAC_READ_ONLY
GF_Err snro_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SeqOffHintEntryBox *ptr = (GF_SeqOffHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->SeqOffset);
	return GF_OK;
}

GF_Err snro_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


#define WRITE_SAMPLE_FRAGMENTS		1

void stbl_del(GF_Box *s)
{
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	if (ptr == NULL) return;

	if (ptr->ChunkOffset) gf_isom_box_del(ptr->ChunkOffset);
	if (ptr->CompositionOffset) gf_isom_box_del((GF_Box *) ptr->CompositionOffset);
	if (ptr->DegradationPriority) gf_isom_box_del((GF_Box *) ptr->DegradationPriority);
	if (ptr->SampleDescription) gf_isom_box_del((GF_Box *) ptr->SampleDescription);
	if (ptr->SampleSize) gf_isom_box_del((GF_Box *) ptr->SampleSize);
	if (ptr->SampleToChunk) gf_isom_box_del((GF_Box *) ptr->SampleToChunk);
	if (ptr->ShadowSync) gf_isom_box_del((GF_Box *) ptr->ShadowSync);
	if (ptr->SyncSample) gf_isom_box_del((GF_Box *) ptr->SyncSample);
	if (ptr->TimeToSample) gf_isom_box_del((GF_Box *) ptr->TimeToSample);
	if (ptr->SampleDep) gf_isom_box_del((GF_Box *) ptr->SampleDep);
	if (ptr->PaddingBits) gf_isom_box_del((GF_Box *) ptr->PaddingBits);
	if (ptr->Fragments) gf_isom_box_del((GF_Box *) ptr->Fragments);

	free(ptr);
}

GF_Err stbl_AddBox(GF_SampleTableBox *ptr, GF_Box *a)
{
	if (!a) return GF_OK;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_STTS:
		if (ptr->TimeToSample) return GF_ISOM_INVALID_FILE;
		ptr->TimeToSample = (GF_TimeToSampleBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_CTTS:
		if (ptr->CompositionOffset) return GF_ISOM_INVALID_FILE;
		ptr->CompositionOffset = (GF_CompositionOffsetBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSS:
		if (ptr->SyncSample) return GF_ISOM_INVALID_FILE;
		ptr->SyncSample = (GF_SyncSampleBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSD:
		if (ptr->SampleDescription) return GF_ISOM_INVALID_FILE;
		ptr->SampleDescription  =(GF_SampleDescriptionBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		if (ptr->SampleSize) return GF_ISOM_INVALID_FILE;
		ptr->SampleSize = (GF_SampleSizeBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSC:
		if (ptr->SampleToChunk) return GF_ISOM_INVALID_FILE;
		ptr->SampleToChunk = (GF_SampleToChunkBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_FADB:
		if (ptr->PaddingBits) return GF_ISOM_INVALID_FILE;
		ptr->PaddingBits = (GF_PaddingBitsBox *) a;
		break;

	//WARNING: AS THIS MAY CHANGE DYNAMICALLY DURING EDIT, 
	case GF_ISOM_BOX_TYPE_CO64:
	case GF_ISOM_BOX_TYPE_STCO:
		if (ptr->ChunkOffset) {
			gf_isom_box_del(ptr->ChunkOffset);
		}
		ptr->ChunkOffset = a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_STSH:
		if (ptr->ShadowSync) return GF_ISOM_INVALID_FILE;
		ptr->ShadowSync = (GF_ShadowSyncBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STDP:
		if (ptr->DegradationPriority) return GF_ISOM_INVALID_FILE;
		ptr->DegradationPriority = (GF_DegradationPriorityBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SDTP:
		if (ptr->SampleDep) return GF_ISOM_INVALID_FILE;
		ptr->SampleDep= (GF_SampleDependencyTypeBox *)a;
		break;

	case GF_ISOM_BOX_TYPE_STSF:
		if (ptr->Fragments) return GF_ISOM_INVALID_FILE;
		ptr->Fragments = (GF_SampleFragmentBox *)a;
		break;

	//what's this box ??? delete it
	default:
		gf_isom_box_del(a);
	}
	return GF_OK;
}




GF_Err stbl_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Box *a;
	//we need to parse DegPrior in a special way
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;

	while (ptr->size) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		//we need to read the DegPriority in a different way...
		if ((a->type == GF_ISOM_BOX_TYPE_STDP) || (a->type == GF_ISOM_BOX_TYPE_SDTP)) { 
			u64 s = a->size;
/*
			if (!ptr->SampleSize) {
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
*/
			if (a->type == GF_ISOM_BOX_TYPE_STDP) {
				if (ptr->SampleSize) ((GF_DegradationPriorityBox *)a)->nb_entries = ptr->SampleSize->sampleCount;
				e = stdp_Read(a, bs);
			} else {
				if (ptr->SampleSize) ((GF_SampleDependencyTypeBox *)a)->sampleCount = ptr->SampleSize->sampleCount;
				e = sdtp_Read(a, bs);
			}
			if (e) {
				gf_isom_box_del(a);
				return e;
			}
			a->size = s;
		}
		if (ptr->size<a->size) {
			gf_isom_box_del(a);
			return GF_ISOM_INVALID_FILE;
		}
		ptr->size -= a->size;
		e = stbl_AddBox(ptr, a);
		if (e) return e;
	}
	return GF_OK;
}

GF_Box *stbl_New()
{
	GF_SampleTableBox *tmp = (GF_SampleTableBox *) malloc(sizeof(GF_SampleTableBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleTableBox));

	tmp->type = GF_ISOM_BOX_TYPE_STBL;
	//maxSamplePer chunk is 10 by default
	tmp->MaxSamplePerChunk = 10;
	tmp->groupID = 1;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stbl_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->SampleDescription) {
		e = gf_isom_box_write((GF_Box *) ptr->SampleDescription, bs);
		if (e) return e;
	}
	if (ptr->TimeToSample) {
		e = gf_isom_box_write((GF_Box *) ptr->TimeToSample, bs);
		if (e) return e;
	}
	if (ptr->CompositionOffset)	{
		e = gf_isom_box_write((GF_Box *) ptr->CompositionOffset, bs);
		if (e) return e;
	}
	if (ptr->SyncSample) {
		e = gf_isom_box_write((GF_Box *) ptr->SyncSample, bs);
		if (e) return e;
	}
	if (ptr->ShadowSync) {
		e = gf_isom_box_write((GF_Box *) ptr->ShadowSync, bs);
		if (e) return e;
	}
	if (ptr->SampleToChunk) {
		e = gf_isom_box_write((GF_Box *) ptr->SampleToChunk, bs);
		if (e) return e;
	}
	if (ptr->SampleSize) {
		e = gf_isom_box_write((GF_Box *) ptr->SampleSize, bs);
		if (e) return e;
	}
	if (ptr->ChunkOffset) {
		e = gf_isom_box_write(ptr->ChunkOffset, bs);
		if (e) return e;
	}
	if (ptr->DegradationPriority) {
		e = gf_isom_box_write((GF_Box *) ptr->DegradationPriority, bs);
		if (e) return e;
	}
	if (ptr->SampleDep && ptr->SampleDep->sampleCount) {
		e = gf_isom_box_write((GF_Box *) ptr->SampleDep, bs);
		if (e) return e;
	}
	if (ptr->PaddingBits) {
		e = gf_isom_box_write((GF_Box *) ptr->PaddingBits, bs);
		if (e) return e;
	}

#if WRITE_SAMPLE_FRAGMENTS
	//sampleFragments
	if (ptr->Fragments) {
		e = gf_isom_box_write((GF_Box *) ptr->Fragments, bs);
		if (e) return e;
	}
#endif
	return GF_OK;
}

GF_Err stbl_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	//Mandatory boxs (but not internally :)
	if (ptr->SampleDescription) {
		e = gf_isom_box_size((GF_Box *) ptr->SampleDescription);
		if (e) return e;
		ptr->size += ptr->SampleDescription->size;
	}
	if (ptr->SampleSize) {
		e = gf_isom_box_size((GF_Box *) ptr->SampleSize);
		if (e) return e;
		ptr->size += ptr->SampleSize->size;
	}
	if (ptr->SampleToChunk) {
		e = gf_isom_box_size((GF_Box *) ptr->SampleToChunk);
		if (e) return e;
		ptr->size += ptr->SampleToChunk->size;
	} 
	if (ptr->TimeToSample) {
		e = gf_isom_box_size((GF_Box *) ptr->TimeToSample);
		if (e) return e;
		ptr->size += ptr->TimeToSample->size;
	}
	if (ptr->ChunkOffset) {
		e = gf_isom_box_size(ptr->ChunkOffset);
		if (e) return e;
		ptr->size += ptr->ChunkOffset->size;
	}

	//optional boxs
	if (ptr->CompositionOffset)	{
		e = gf_isom_box_size((GF_Box *) ptr->CompositionOffset);
		if (e) return e;
		ptr->size += ptr->CompositionOffset->size;
	}
	if (ptr->DegradationPriority) {
		e = gf_isom_box_size((GF_Box *) ptr->DegradationPriority);
		if (e) return e;
		ptr->size += ptr->DegradationPriority->size;
	}
	if (ptr->ShadowSync) {
		e = gf_isom_box_size((GF_Box *) ptr->ShadowSync);
		if (e) return e;
		ptr->size += ptr->ShadowSync->size;
	}
	if (ptr->SyncSample) {
		e = gf_isom_box_size((GF_Box *) ptr->SyncSample);
		if (e) return e;
		ptr->size += ptr->SyncSample->size;
	}
	if (ptr->SampleDep && ptr->SampleDep->sampleCount) {
		e = gf_isom_box_size((GF_Box *) ptr->SampleDep);
		if (e) return e;
		ptr->size += ptr->SampleDep->size;
	}
	//padb
	if (ptr->PaddingBits) {
		e = gf_isom_box_size((GF_Box *) ptr->PaddingBits);
		if (e) return e;
		ptr->size += ptr->PaddingBits->size;
	}
#if WRITE_SAMPLE_FRAGMENTS
	//sample fragments
	if (ptr->Fragments) {
		e = gf_isom_box_size((GF_Box *) ptr->Fragments);
		if (e) return e;
		ptr->size += ptr->Fragments->size;
	}
#endif
	
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void stco_del(GF_Box *s)
{
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;
	if (ptr == NULL) return;
	if (ptr->offsets) free(ptr->offsets);
	free(ptr);
}


GF_Err stco_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 entries;
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);

	if (ptr->nb_entries) {
		ptr->offsets = (u32 *) malloc(ptr->nb_entries * sizeof(u32) );
		if (ptr->offsets == NULL) return GF_OUT_OF_MEM;
		ptr->alloc_size = ptr->nb_entries;

		for (entries = 0; entries < ptr->nb_entries; entries++) {
			ptr->offsets[entries] = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}

GF_Box *stco_New()
{
	GF_ChunkOffsetBox *tmp = (GF_ChunkOffsetBox *) malloc(sizeof(GF_ChunkOffsetBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ChunkOffsetBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_STCO;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stco_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i = 0; i < ptr->nb_entries; i++) {
		gf_bs_write_u32(bs, ptr->offsets[i]);
	}
	return GF_OK;
}


GF_Err stco_Size(GF_Box *s)
{
	GF_Err e;
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (4 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void stdp_del(GF_Box *s)
{
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	if (ptr == NULL ) return;
	if (ptr->priorities) free(ptr->priorities);
	free(ptr);
}

//this is called through stbl_read...
GF_Err stdp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 entry;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	/*out-of-order stdp, assume no padding at the end*/
	if (!ptr->nb_entries) ptr->nb_entries = (u32) (ptr->size-8) / 2;
	ptr->priorities = (u16 *) malloc(ptr->nb_entries * sizeof(u16));
	if (ptr->priorities == NULL) return GF_OUT_OF_MEM;
	for (entry = 0; entry < ptr->nb_entries; entry++) {
		//we have a bit for padding
		gf_bs_read_int(bs, 1);
		ptr->priorities[entry] = gf_bs_read_int(bs, 15);
	}
	return GF_OK;
}

GF_Box *stdp_New()
{
	GF_DegradationPriorityBox *tmp = (GF_DegradationPriorityBox *) malloc(sizeof(GF_DegradationPriorityBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DegradationPriorityBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_STDP;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stdp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	for (i = 0; i < ptr->nb_entries; i++) {
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, ptr->priorities[i], 15);
	}
	return GF_OK;
}

GF_Err stdp_Size(GF_Box *s)
{
	GF_Err e;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += (2 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void stsc_del(GF_Box *s)
{
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) free(ptr->entries);
	free(ptr);
}


GF_Err stsc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = malloc(sizeof(GF_StscEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	
	for (i = 0; i < ptr->nb_entries; i++) {
		ptr->entries[i].firstChunk = gf_bs_read_u32(bs);
		ptr->entries[i].samplesPerChunk = gf_bs_read_u32(bs);
		ptr->entries[i].sampleDescriptionIndex = gf_bs_read_u32(bs);
		ptr->entries[i].isEdited = 0;
		ptr->entries[i].nextChunk = 0;

		//update the next chunk in the previous entry
		if (i) ptr->entries[i-1].nextChunk = ptr->entries[i].firstChunk;
	}
	ptr->currentIndex = 0;
	ptr->firstSampleInCurrentChunk = 0;
	ptr->currentChunk = 0;
	ptr->ghostNumber = 0;
	return GF_OK;
}

GF_Box *stsc_New()
{
	GF_SampleToChunkBox *tmp = (GF_SampleToChunkBox *) malloc(sizeof(GF_SampleToChunkBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleToChunkBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_STSC;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stsc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;	
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries; i++) {
		gf_bs_write_u32(bs, ptr->entries[i].firstChunk);
		gf_bs_write_u32(bs, ptr->entries[i].samplesPerChunk);
		gf_bs_write_u32(bs, ptr->entries[i].sampleDescriptionIndex);
	}
	return GF_OK;
}

GF_Err stsc_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (12 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void stsd_del(GF_Box *s)
{
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->boxList);
	free(ptr);
}

GF_Err stsd_AddBox(GF_SampleDescriptionBox *ptr, GF_Box *a)
{
	GF_UnknownBox *def;
	if (!a) return GF_OK;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_ENCS:
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_BOX_TYPE_ENCA:
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_GHNT:
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_TX3G:
	case GF_ISOM_BOX_TYPE_ENCT:
	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
	case GF_ISOM_BOX_TYPE_DIMS:
	case GF_ISOM_BOX_TYPE_AC3:
		return gf_list_add(ptr->boxList, a);
	/*for 3GP config, we must set the type*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
	{
		((GF_3GPPAudioSampleEntryBox *)a)->info->cfg.type = a->type;
		return gf_list_add(ptr->boxList, a);
	}
	case GF_ISOM_SUBTYPE_3GP_H263:
	{
		((GF_3GPPVisualSampleEntryBox *)a)->info->cfg.type = a->type;
		return gf_list_add(ptr->boxList, a);
	}

	//unknown sample description: we need a specific box to handle the data ref index
	//rather than a default box ...
	default:
		def = (GF_UnknownBox *)a;
		/*we need at least 8 bytes for unknown sample entries*/
		if (def->dataSize < 8) {
			gf_isom_box_del(a);
			return GF_OK;
		}
		return gf_list_add(ptr->boxList, a);
	}
}


GF_Err stsd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 nb_entries;
	u32 i;
	GF_Box *a;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	nb_entries = gf_bs_read_u32(bs);
	for (i = 0; i < nb_entries; i++) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		e = stsd_AddBox(ptr, a);
		if (e) return e;
	}
	return GF_OK;
}

GF_Box *stsd_New()
{
	GF_SampleDescriptionBox *tmp = (GF_SampleDescriptionBox *) malloc(sizeof(GF_SampleDescriptionBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleDescriptionBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->boxList = gf_list_new();
	if (! tmp->boxList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_STSD;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stsd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 nb_entries;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	nb_entries = gf_list_count(ptr->boxList);
	gf_bs_write_u32(bs, nb_entries);
	return gf_isom_box_array_write(s, ptr->boxList, bs);
}

GF_Err stsd_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return gf_isom_box_array_size(s, ptr->boxList);
}

#endif //GPAC_READ_ONLY

void stsf_del(GF_Box *s)
{
	u32 nb_entries;
	u32 i;
	GF_StsfEntry *pe;
	GF_SampleFragmentBox *ptr = (GF_SampleFragmentBox *)s;
	if (ptr == NULL) return;
	
	if (ptr->entryList) {
		nb_entries = gf_list_count(ptr->entryList);
		for ( i = 0; i < nb_entries; i++ ) {
			pe = (GF_StsfEntry*)gf_list_get(ptr->entryList, i);
			if (pe->fragmentSizes) free(pe->fragmentSizes);
			free(pe);	
		}
		gf_list_del(ptr->entryList);
	}
	free(ptr);
}



GF_Err stsf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 entries, i;
	u32 nb_entries;
	GF_StsfEntry *p;
	GF_SampleFragmentBox *ptr = (GF_SampleFragmentBox *)s;
	
	p = NULL;
	if (!ptr) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	nb_entries = gf_bs_read_u32(bs);

	p = NULL;
	for ( entries = 0; entries < nb_entries; entries++ ) {
		p = (GF_StsfEntry *) malloc(sizeof(GF_StsfEntry));
		if (!p) return GF_OUT_OF_MEM;
		p->SampleNumber = gf_bs_read_u32(bs);
		p->fragmentCount = gf_bs_read_u32(bs);
		p->fragmentSizes = (u16*)malloc(sizeof(GF_StsfEntry) * p->fragmentCount);
		for (i=0; i<p->fragmentCount; i++) {
			p->fragmentSizes[i] = gf_bs_read_u16(bs);
		}
		gf_list_add(ptr->entryList, p);
	}
#ifndef GPAC_READ_ONLY
	ptr->w_currentEntry = p;
	ptr->w_currentEntryIndex = nb_entries-1;
#endif
	return GF_OK;
}

GF_Box *stsf_New()
{
	GF_SampleFragmentBox *tmp;
	
	tmp = (GF_SampleFragmentBox *) malloc(sizeof(GF_SampleFragmentBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleFragmentBox));

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->entryList = gf_list_new();
	if (! tmp->entryList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_STSF;
	return (GF_Box *) tmp;
}



//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stsf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j;
	u32 nb_entries;
	GF_StsfEntry *p;
	GF_SampleFragmentBox *ptr = (GF_SampleFragmentBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
    nb_entries = gf_list_count(ptr->entryList);
	gf_bs_write_u32(bs, nb_entries);
	for ( i = 0; i < nb_entries; i++ ) {
		p = (GF_StsfEntry*)gf_list_get(ptr->entryList, i);
		gf_bs_write_u32(bs, p->SampleNumber);
		gf_bs_write_u32(bs, p->fragmentCount);
		for (j=0;j<p->fragmentCount;j++) {
			gf_bs_write_u16(bs, p->fragmentSizes[j]);
		}
	}
	return GF_OK;
}

GF_Err stsf_Size(GF_Box *s)
{
	GF_Err e;
	GF_StsfEntry *p;
	u32 nb_entries, i;
	GF_SampleFragmentBox *ptr = (GF_SampleFragmentBox *) s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	nb_entries = gf_list_count(ptr->entryList);
	ptr->size += 4;
	for (i=0;i<nb_entries; i++) {
		p = (GF_StsfEntry *)gf_list_get(ptr->entryList, i);
		ptr->size += 8 + 2*p->fragmentCount;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void stsh_del(GF_Box *s)
{
	u32 i = 0;
	GF_StshEntry *ent;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;
	if (ptr == NULL) return;
	while ( (ent = (GF_StshEntry *)gf_list_enum(ptr->entries, &i)) ) {
		free(ent);
	}
	gf_list_del(ptr->entries);
	free(ptr);
}



GF_Err stsh_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count, i;
	GF_StshEntry *ent;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	count = gf_bs_read_u32(bs);

	for (i = 0; i < count; i++) {
		ent = (GF_StshEntry *) malloc(sizeof(GF_StshEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->shadowedSampleNumber = gf_bs_read_u32(bs);
		ent->syncSampleNumber = gf_bs_read_u32(bs);
		e = gf_list_add(ptr->entries, ent);
		if (e) return e;
	}
	return GF_OK;
}

GF_Box *stsh_New()
{
	GF_ShadowSyncBox *tmp = (GF_ShadowSyncBox *) malloc(sizeof(GF_ShadowSyncBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ShadowSyncBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->entries = gf_list_new();
	if (!tmp->entries) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_STSH;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stsh_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_StshEntry *ent;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, gf_list_count(ptr->entries));
	i=0;
	while ((ent = (GF_StshEntry *)gf_list_enum(ptr->entries, &i))) {
		gf_bs_write_u32(bs, ent->shadowedSampleNumber);
		gf_bs_write_u32(bs, ent->syncSampleNumber);
	}
	return GF_OK;
}

GF_Err stsh_Size(GF_Box *s)
{
	GF_Err e;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (8 * gf_list_count(ptr->entries));
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void stss_del(GF_Box *s)
{
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
	if (ptr == NULL) return;
	if (ptr->sampleNumbers) free(ptr->sampleNumbers);
	free(ptr);
}

GF_Err stss_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->alloc_size = ptr->nb_entries;
	ptr->sampleNumbers = (u32 *) malloc( ptr->alloc_size * sizeof(u32));
	if (ptr->sampleNumbers == NULL) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->nb_entries; i++) {
		ptr->sampleNumbers[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *stss_New()
{
	GF_SyncSampleBox *tmp = (GF_SyncSampleBox *) malloc(sizeof(GF_SyncSampleBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SyncSampleBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_STSS;
	return (GF_Box*)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stss_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i = 0; i < ptr->nb_entries; i++) {
		gf_bs_write_u32(bs, ptr->sampleNumbers[i]);
	}
	return GF_OK;
}

GF_Err stss_Size(GF_Box *s)
{
	GF_Err e;
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (4 * ptr->nb_entries);
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void stsz_del(GF_Box *s)
{
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return;
	if (ptr->sizes) free(ptr->sizes);
	free(ptr);
}


GF_Err stsz_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, estSize;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	//support for CompactSizes
	if (s->type == GF_ISOM_BOX_TYPE_STSZ) {
		ptr->sampleSize = gf_bs_read_u32(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);
		ptr->size -= 8;
	} else {
		//24-reserved
		gf_bs_read_int(bs, 24);
		i = gf_bs_read_u8(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);
		ptr->size -= 8;
		switch (i) {
		case 4:
		case 8:
		case 16:
			ptr->sampleSize = i;
			break;
		default:
			//try to fix the file
			//no samples, no parsing pb
			if (!ptr->sampleCount) {
				ptr->sampleSize = 16;
				return GF_OK;
			}			
			estSize = (u32) (ptr->size) / ptr->sampleCount;
			if (!estSize && ((ptr->sampleCount+1)/2 == (ptr->size)) ) {
				ptr->sampleSize = 4;
				break;
			} else if (estSize == 1 || estSize == 2) {
				ptr->sampleSize = 8 * estSize;
			} else {
				return GF_ISOM_INVALID_FILE;
			}
		}
	}
	if (s->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (! ptr->sampleSize && ptr->sampleCount) {
			ptr->sizes = (u32 *) malloc(ptr->sampleCount * sizeof(u32));
			if (! ptr->sizes) return GF_OUT_OF_MEM;
			for (i = 0; i < ptr->sampleCount; i++) {
				ptr->sizes[i] = gf_bs_read_u32(bs);
			}
		}
	} else {
		//note we could optimize the mem usage by keeping the table compact
		//in memory. But that would complicate both caching and editing
		//we therefore keep all sizes as u32 and uncompress the table
		ptr->sizes = (u32 *) malloc(ptr->sampleCount * sizeof(u32));
		if (! ptr->sizes) return GF_OUT_OF_MEM;

		for (i = 0; i < ptr->sampleCount; ) {
			switch (ptr->sampleSize) {
			case 4:
				ptr->sizes[i] = gf_bs_read_int(bs, 4);
				if (i+1 < ptr->sampleCount) {
					ptr->sizes[i+1] = gf_bs_read_int(bs, 4);
				} else {
					//0 padding in odd sample count
					gf_bs_read_int(bs, 4);
				}
				i += 2;
				break;
			default:
				ptr->sizes[i] = gf_bs_read_int(bs, ptr->sampleSize);
				i += 1;
				break;
			}
		}
	}
	return GF_OK;
}

GF_Box *stsz_New()
{
	GF_SampleSizeBox *tmp = (GF_SampleSizeBox *) malloc(sizeof(GF_SampleSizeBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleSizeBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	//type is unknown here, can be regular or compact table
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stsz_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//in both versions this is still valid
	if (ptr->type == GF_ISOM_BOX_TYPE_STSZ) {
		gf_bs_write_u32(bs, ptr->sampleSize);
	} else {
		gf_bs_write_u24(bs, 0);
		gf_bs_write_u8(bs, ptr->sampleSize);
	}
	gf_bs_write_u32(bs, ptr->sampleCount);

	if (ptr->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (! ptr->sampleSize) {
			for (i = 0; i < ptr->sampleCount; i++) {
				gf_bs_write_u32(bs, ptr->sizes[i]);
			}
		}
	} else {
		for (i = 0; i < ptr->sampleCount; ) {
			switch (ptr->sampleSize) {
			case 4:
				gf_bs_write_int(bs, ptr->sizes[i], 4);
				if (i+1 < ptr->sampleCount) {
					gf_bs_write_int(bs, ptr->sizes[i+1], 4);
				} else {
					//0 padding in odd sample count
					gf_bs_write_int(bs, 0, 4);
				}
				i += 2;
				break;
			default:
				gf_bs_write_int(bs, ptr->sizes[i], ptr->sampleSize);
				i += 1;
				break;
			}
		}
	}
	return GF_OK;
}

GF_Err stsz_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, fieldSize, size;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	ptr->size += 8;
	if (!ptr->sampleCount) return GF_OK;

	//regular table
	if (ptr->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (ptr->sampleSize) return GF_OK;
		ptr->size += (4 * ptr->sampleCount);
		return GF_OK;
	}

	fieldSize = 4;
	size = ptr->sizes[0];

	for (i=0; i < ptr->sampleCount; i++) {
		if (ptr->sizes[i] <= 0xF) continue;
		//switch to 8-bit table
		else if (ptr->sizes[i] <= 0xFF) {
			fieldSize = 8;
		}
		//switch to 16-bit table
		else if (ptr->sizes[i] <= 0xFFFF) {
			fieldSize = 16;
		}
		//switch to 32-bit table
		else {
			fieldSize = 32;
		}

		//check the size
		if (size != ptr->sizes[i]) size = 0;
	}
	//if all samples are of the same size, switch to regular (more compact)
	if (size) {
		ptr->type = GF_ISOM_BOX_TYPE_STSZ;
		ptr->sampleSize = size;
		free(ptr->sizes);
		ptr->sizes = NULL;
	}

	if (fieldSize == 32) {
		//oops, doesn't fit in a compact table
		ptr->type = GF_ISOM_BOX_TYPE_STSZ;
		ptr->size += (4 * ptr->sampleCount);
		return GF_OK;
	}
	
	//make sure we are a compact table (no need to change the mem representation)
	ptr->type = GF_ISOM_BOX_TYPE_STZ2;
	ptr->sampleSize = fieldSize;
	if (fieldSize == 4) {
		//do not forget the 0 padding field for odd count
		ptr->size += (ptr->sampleCount + 1) / 2;
	} else {
		ptr->size += (ptr->sampleCount) * (fieldSize/8);
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void stts_del(GF_Box *s)
{
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	if (ptr->entries) free(ptr->entries);
	free(ptr);
}


GF_Err stts_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

#ifndef GPAC_READ_ONLY
	ptr->w_LastDTS = 0;
#endif
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = malloc(sizeof(GF_SttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		ptr->entries[i].sampleDelta = gf_bs_read_u32(bs);
#ifndef GPAC_READ_ONLY
		ptr->w_currentSampleNum += ptr->entries[i].sampleCount;
		ptr->w_LastDTS += ptr->entries[i].sampleCount * ptr->entries[i].sampleDelta;
#endif
	}
	//remove the last sample delta.
#ifndef GPAC_READ_ONLY
	if (ptr->nb_entries) ptr->w_LastDTS -= ptr->entries[ptr->nb_entries-1].sampleDelta;
#endif
	return GF_OK;
}

GF_Box *stts_New()
{
	GF_TimeToSampleBox *tmp = (GF_TimeToSampleBox *) malloc(sizeof(GF_TimeToSampleBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TimeToSampleBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_STTS;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err stts_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries;i++) {
		gf_bs_write_u32(bs, ptr->entries[i].sampleCount);
		gf_bs_write_u32(bs, ptr->entries[i].sampleDelta);
	}
	return GF_OK;
}

GF_Err stts_Size(GF_Box *s)
{
	GF_Err e;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}


#endif //GPAC_READ_ONLY


#ifndef	GPAC_ISOM_NO_FRAGMENTS

void tfhd_del(GF_Box *s)
{
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}

GF_Err tfhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
	
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	
	ptr->trackID = gf_bs_read_u32(bs);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRAF_BASE_OFFSET) {
		ptr->base_data_offset = gf_bs_read_u64(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DESC) {
		ptr->sample_desc_index = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DUR) {
		ptr->def_sample_duration = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_SIZE) {
		ptr->def_sample_size = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) {
		ptr->def_sample_flags = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *tfhd_New()
{
	GF_TrackFragmentHeaderBox *tmp = (GF_TrackFragmentHeaderBox *) malloc(sizeof(GF_TrackFragmentHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackFragmentHeaderBox));
	tmp->type = GF_ISOM_BOX_TYPE_TFHD;
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err tfhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->trackID);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRAF_BASE_OFFSET) {
		gf_bs_write_u64(bs, ptr->base_data_offset);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DESC) {
		gf_bs_write_u32(bs, ptr->sample_desc_index);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DUR) {
		gf_bs_write_u32(bs, ptr->def_sample_duration);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_SIZE) {
		gf_bs_write_u32(bs, ptr->def_sample_size);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) {
		gf_bs_write_u32(bs, ptr->def_sample_flags);
	}
	return GF_OK;
}

GF_Err tfhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRAF_BASE_OFFSET) ptr->size += 8;
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DESC) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DUR) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_SIZE) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) ptr->size += 4;
	return GF_OK;
}



#endif //GPAC_READ_ONLY

#endif 

void tims_del(GF_Box *s)
{
	GF_TSHintEntryBox *tims = (GF_TSHintEntryBox *)s;
	free(tims);
}

GF_Err tims_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TSHintEntryBox *ptr = (GF_TSHintEntryBox *)s;
	ptr->timeScale = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tims_New()
{
	GF_TSHintEntryBox *tmp = (GF_TSHintEntryBox *) malloc(sizeof(GF_TSHintEntryBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_TSHintEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_TIMS;
	return (GF_Box *)tmp;
}

#ifndef GPAC_READ_ONLY

GF_Err tims_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TSHintEntryBox *ptr = (GF_TSHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->timeScale);
	return GF_OK;
}

GF_Err tims_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}

#endif	


void tkhd_del(GF_Box *s)
{
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
	return;
}


GF_Err tkhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->trackID = gf_bs_read_u32(bs);
		ptr->reserved1 = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ptr->creationTime = gf_bs_read_u32(bs);
		ptr->modificationTime = gf_bs_read_u32(bs);
		ptr->trackID = gf_bs_read_u32(bs);
		ptr->reserved1 = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u32(bs);
	}
	ptr->reserved2[0] = gf_bs_read_u32(bs);
	ptr->reserved2[1] = gf_bs_read_u32(bs);
	ptr->layer = gf_bs_read_u16(bs);
	ptr->alternate_group = gf_bs_read_u16(bs);
	ptr->volume = gf_bs_read_u16(bs);
	ptr->reserved3 = gf_bs_read_u16(bs);
	ptr->matrix[0] = gf_bs_read_u32(bs);
	ptr->matrix[1] = gf_bs_read_u32(bs);
	ptr->matrix[2] = gf_bs_read_u32(bs);
	ptr->matrix[3] = gf_bs_read_u32(bs);
	ptr->matrix[4] = gf_bs_read_u32(bs);
	ptr->matrix[5] = gf_bs_read_u32(bs);
	ptr->matrix[6] = gf_bs_read_u32(bs);
	ptr->matrix[7] = gf_bs_read_u32(bs);
	ptr->matrix[8] = gf_bs_read_u32(bs);
	ptr->width = gf_bs_read_u32(bs);
	ptr->height = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tkhd_New()
{
	GF_TrackHeaderBox *tmp = (GF_TrackHeaderBox *) malloc(sizeof(GF_TrackHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackHeaderBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_TKHD;
	tmp->matrix[0] = 0x00010000;
	tmp->matrix[4] = 0x00010000;
	tmp->matrix[8] = 0x40000000;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err tkhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->version == 1) {
		gf_bs_write_u64(bs, ptr->creationTime);
		gf_bs_write_u64(bs, ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->trackID);
		gf_bs_write_u32(bs, ptr->reserved1);
		gf_bs_write_u64(bs, ptr->duration);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->creationTime);
		gf_bs_write_u32(bs, (u32) ptr->modificationTime);
		gf_bs_write_u32(bs, ptr->trackID);
		gf_bs_write_u32(bs, ptr->reserved1);
		gf_bs_write_u32(bs, (u32) ptr->duration);
	}
	gf_bs_write_u32(bs, ptr->reserved2[0]);
	gf_bs_write_u32(bs, ptr->reserved2[1]);
	gf_bs_write_u16(bs, ptr->layer);
	gf_bs_write_u16(bs, ptr->alternate_group);
	gf_bs_write_u16(bs, ptr->volume);
	gf_bs_write_u16(bs, ptr->reserved3);
	gf_bs_write_u32(bs, ptr->matrix[0]);
	gf_bs_write_u32(bs, ptr->matrix[1]);
	gf_bs_write_u32(bs, ptr->matrix[2]);
	gf_bs_write_u32(bs, ptr->matrix[3]);
	gf_bs_write_u32(bs, ptr->matrix[4]);
	gf_bs_write_u32(bs, ptr->matrix[5]);
	gf_bs_write_u32(bs, ptr->matrix[6]);
	gf_bs_write_u32(bs, ptr->matrix[7]);
	gf_bs_write_u32(bs, ptr->matrix[8]);
	gf_bs_write_u32(bs, ptr->width);
	gf_bs_write_u32(bs, ptr->height);
	return GF_OK;
}

GF_Err tkhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;
	ptr->size += (ptr->version == 1) ? 32 : 20;
	ptr->size += 60;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



#ifndef	GPAC_ISOM_NO_FRAGMENTS

void traf_del(GF_Box *s)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	if (ptr == NULL) return;
	if (ptr->tfhd) gf_isom_box_del((GF_Box *) ptr->tfhd);
	gf_isom_box_array_del(ptr->TrackRuns);
	free(ptr);
}

GF_Err traf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TFHD:
		if (ptr->tfhd) return GF_ISOM_INVALID_FILE;
		ptr->tfhd = (GF_TrackFragmentHeaderBox *) a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRUN:
		return gf_list_add(ptr->TrackRuns, a);

	default:
		return GF_ISOM_INVALID_FILE;
	}
}


GF_Err traf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, traf_AddBox);
}

GF_Box *traf_New()
{
	GF_TrackFragmentBox *tmp = (GF_TrackFragmentBox *) malloc(sizeof(GF_TrackFragmentBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackFragmentBox));
	tmp->type = GF_ISOM_BOX_TYPE_TRAF;
	tmp->TrackRuns = gf_list_new();
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err traf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	//Header first
	if (ptr->tfhd) {
		e = gf_isom_box_write((GF_Box *) ptr->tfhd, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->TrackRuns, bs);
}

GF_Err traf_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;

	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (ptr->tfhd) {
		e = gf_isom_box_size((GF_Box *) ptr->tfhd);
		if (e) return e;
		ptr->size += ptr->tfhd->size;
	}
	return gf_isom_box_array_size(s, ptr->TrackRuns);
}



#endif //GPAC_READ_ONLY

#endif 

void trak_del(GF_Box *s)
{
	GF_TrackBox *ptr = (GF_TrackBox *) s;
	if (ptr == NULL) return;

	if (ptr->Header) gf_isom_box_del((GF_Box *)ptr->Header);
	if (ptr->udta) gf_isom_box_del((GF_Box *)ptr->udta);
	if (ptr->Media) gf_isom_box_del((GF_Box *)ptr->Media);
	if (ptr->References) gf_isom_box_del((GF_Box *)ptr->References);
	if (ptr->editBox) gf_isom_box_del((GF_Box *)ptr->editBox);
	if (ptr->meta) gf_isom_box_del((GF_Box *)ptr->meta);
	gf_isom_box_array_del(ptr->boxes);
	if (ptr->name) free(ptr->name); 
	free(ptr);
}

static void gf_isom_check_sample_desc(GF_TrackBox *trak)
{
	GF_BitStream *bs;
	GF_GenericVisualSampleEntryBox *genv;
	GF_GenericAudioSampleEntryBox *gena;
	GF_GenericSampleEntryBox *genm;
	GF_UnknownBox *a;
	u32 i;
	u64 read;

	i=0;
	while ((a = (GF_UnknownBox*)gf_list_enum(trak->Media->information->sampleTable->SampleDescription->boxList, &i))) {
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_MP4S:
		case GF_ISOM_BOX_TYPE_ENCS:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_MP4V:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_SUBTYPE_3GP_AMR:
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_QCELP:
		case GF_ISOM_SUBTYPE_3GP_SMV:
		case GF_ISOM_SUBTYPE_3GP_H263:
		case GF_ISOM_BOX_TYPE_GHNT:
		case GF_ISOM_BOX_TYPE_RTP_STSD:
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_TX3G:
		case GF_ISOM_BOX_TYPE_ENCT:
		case GF_ISOM_BOX_TYPE_DIMS:
		case GF_ISOM_BOX_TYPE_AC3:
			continue;
		default:
			break;
		}
		/*only process visual or audio*/
		switch (trak->Media->handler->handlerType) {
		case GF_ISOM_MEDIA_VISUAL:
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->boxList, i-1);
			genv = (GF_GenericVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRV);
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			genv->size = a->size;
			gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *) genv, bs);
			genv->data_size = (u32) gf_bs_available(bs);
			if (genv->data_size) {
				genv->data = (char*)malloc(sizeof(char) * genv->data_size);
				gf_bs_read_data(bs, genv->data, genv->data_size);
			}
			gf_bs_del(bs);
			genv->size = a->size;
			genv->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->boxList, genv, i-1);
			break;
		case GF_ISOM_MEDIA_AUDIO:
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->boxList, i-1);
			gena = (GF_GenericAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRA);
			gena->size = a->size;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox *) gena, bs);
			gena->data_size = (u32) gf_bs_available(bs);
			if (gena->data_size) {
				gena->data = (char*)malloc(sizeof(char) * gena->data_size);
				gf_bs_read_data(bs, gena->data, gena->data_size);
			}
			gf_bs_del(bs);
			gena->size = a->size;
			gena->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->boxList, gena, i-1);
			break;

		default:
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->boxList, i-1);
			genm = (GF_GenericSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
			genm->size = a->size;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			read = 0;
			gf_bs_read_data(bs, genm->reserved, 6);
			genm->dataReferenceIndex = gf_bs_read_u16(bs);
			genm->data_size = (u32) gf_bs_available(bs);
			if (genm->data_size) {
				genm->data = (char*)malloc(sizeof(char) * genm->data_size);
				gf_bs_read_data(bs, genm->data, genm->data_size);
			}
			gf_bs_del(bs);
			genm->size = a->size;
			genm->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->boxList, genm, i-1);
			break;
		}

	}
}


GF_Err trak_AddBox(GF_Box *s, GF_Box *a)
{
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	if (!a) return GF_OK;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_TKHD:
		if (ptr->Header) return GF_ISOM_INVALID_FILE;
		ptr->Header = (GF_TrackHeaderBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_EDTS:
		if (ptr->editBox) return GF_ISOM_INVALID_FILE;
		ptr->editBox = (GF_EditBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UDTA:
		if (ptr->udta) return GF_ISOM_INVALID_FILE;
		ptr->udta = (GF_UserDataBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_META:
		if (ptr->meta) return GF_ISOM_INVALID_FILE;
		ptr->meta = (GF_MetaBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TREF:
		if (ptr->References) return GF_ISOM_INVALID_FILE;
		ptr->References = (GF_TrackReferenceBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_MDIA:
		if (ptr->Media) return GF_ISOM_INVALID_FILE;
		ptr->Media = (GF_MediaBox *)a;
		((GF_MediaBox *)a)->mediaTrack = ptr;
		return GF_OK;
	default:
		gf_list_add(ptr->boxes, a);
		return GF_OK;
	}
	return GF_OK;
}


GF_Err trak_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	e = gf_isom_read_box_list(s, bs, trak_AddBox);
	if (e) return e;
	gf_isom_check_sample_desc(ptr);
	return GF_OK;
}

GF_Box *trak_New()
{
	GF_TrackBox *tmp = (GF_TrackBox *) malloc(sizeof(GF_TrackBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackBox));
	tmp->type = GF_ISOM_BOX_TYPE_TRAK;
	tmp->boxes = gf_list_new();
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err trak_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->Header) {
		e = gf_isom_box_write((GF_Box *) ptr->Header, bs);
		if (e) return e;
	}
	if (ptr->References) {
		e = gf_isom_box_write((GF_Box *) ptr->References, bs);
		if (e) return e;
	}
	if (ptr->editBox) {
		e = gf_isom_box_write((GF_Box *) ptr->editBox, bs);
		if (e) return e;
	}
	if (ptr->Media) {
		e = gf_isom_box_write((GF_Box *) ptr->Media, bs);
		if (e) return e;
	}
	if (ptr->meta) {
		e = gf_isom_box_write((GF_Box *) ptr->meta, bs);
		if (e) return e;
	}
	if (ptr->udta) {
		e = gf_isom_box_write((GF_Box *) ptr->udta, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->boxes, bs);
}

GF_Err trak_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;

	if (ptr->Header) {
		e = gf_isom_box_size((GF_Box *) ptr->Header);
		if (e) return e;
		ptr->size += ptr->Header->size;
	}
	if (ptr->udta) {
		e = gf_isom_box_size((GF_Box *) ptr->udta);
		if (e) return e;
		ptr->size += ptr->udta->size;
	}
	if (ptr->References) {
		e = gf_isom_box_size((GF_Box *) ptr->References);
		if (e) return e;
		ptr->size += ptr->References->size;
	}
	if (ptr->editBox) {
		e = gf_isom_box_size((GF_Box *) ptr->editBox);
		if (e) return e;
		ptr->size += ptr->editBox->size;
	}
	if (ptr->Media) {
		e = gf_isom_box_size((GF_Box *) ptr->Media);
		if (e) return e;
		ptr->size += ptr->Media->size;
	}
	if (ptr->meta) {
		e = gf_isom_box_size((GF_Box *) ptr->meta);
		if (e) return e;
		ptr->size += ptr->meta->size;
	}
	return gf_isom_box_array_size(s, ptr->boxes);
}

#endif //GPAC_READ_ONLY


void tref_del(GF_Box *s)
{
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->boxList);
	free(ptr);
}


GF_Err tref_AddBox(GF_Box *s, GF_Box *a)
{
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	return gf_list_add(ptr->boxList, a);
}

GF_Err tref_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list_ex(s, bs, tref_AddBox, s->type);
}

GF_Box *tref_New()
{
	GF_TrackReferenceBox *tmp = (GF_TrackReferenceBox *) malloc(sizeof(GF_TrackReferenceBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackReferenceBox));
	tmp->boxList = gf_list_new();
	if (!tmp->boxList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_TREF;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err tref_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->boxList, bs);
}

GF_Err tref_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	return gf_isom_box_array_size(s, ptr->boxList);
}

#endif //GPAC_READ_ONLY

void reftype_del(GF_Box *s)
{
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	if (!ptr) return;
	if (ptr->trackIDs) free(ptr->trackIDs);
	free(ptr);
}


GF_Err reftype_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	u32 i;
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;

	bytesToRead = (u32) (ptr->size);
	if (!bytesToRead) return GF_OK;

	ptr->trackIDCount = (u32) (bytesToRead) / sizeof(u32);
	ptr->trackIDs = (u32 *) malloc(ptr->trackIDCount * sizeof(u32));
	if (!ptr->trackIDs) return GF_OUT_OF_MEM;
	
	for (i = 0; i < ptr->trackIDCount; i++) {
		ptr->trackIDs[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *reftype_New()
{
	GF_TrackReferenceTypeBox *tmp = (GF_TrackReferenceTypeBox *) malloc(sizeof(GF_TrackReferenceTypeBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackReferenceTypeBox));
	tmp->type = GF_ISOM_BOX_TYPE_REFT;
	return (GF_Box *)tmp;
}


GF_Err reftype_AddRefTrack(GF_TrackReferenceTypeBox *ref, u32 trackID, u16 *outRefIndex)
{
	u32 i;
	if (!ref || !trackID) return GF_BAD_PARAM;

	if (outRefIndex) *outRefIndex = 0;
	//don't add a dep if already here !!
	for (i = 0; i < ref->trackIDCount; i++) {
		if (ref->trackIDs[i] == trackID) {
			if (outRefIndex) *outRefIndex = i+1;
			return GF_OK;
		}
	}

	ref->trackIDs = (u32 *) realloc(ref->trackIDs, (ref->trackIDCount + 1) * sizeof(u32) );
	if (!ref->trackIDs) return GF_OUT_OF_MEM;
	ref->trackIDs[ref->trackIDCount] = trackID;
	ref->trackIDCount++;
	if (outRefIndex) *outRefIndex = ref->trackIDCount;
	return GF_OK;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err reftype_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	ptr->type = ptr->reference_type;
	e = gf_isom_box_write_header(s, bs);
	ptr->type = GF_ISOM_BOX_TYPE_REFT;
	if (e) return e;
	for (i = 0; i < ptr->trackIDCount; i++) {
		gf_bs_write_u32(bs, ptr->trackIDs[i]);
	}
	return GF_OK;
}


GF_Err reftype_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += (ptr->trackIDCount * sizeof(u32));
	return GF_OK;
}

#endif //GPAC_READ_ONLY



#ifndef	GPAC_ISOM_NO_FRAGMENTS

void trex_del(GF_Box *s)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}


GF_Err trex_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->trackID = gf_bs_read_u32(bs);
	ptr->def_sample_desc_index = gf_bs_read_u32(bs);
	ptr->def_sample_duration = gf_bs_read_u32(bs);
	ptr->def_sample_size = gf_bs_read_u32(bs);
	ptr->def_sample_flags = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *trex_New()
{
	GF_TrackExtendsBox *tmp = (GF_TrackExtendsBox *) malloc(sizeof(GF_TrackExtendsBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackExtendsBox));
	tmp->type = GF_ISOM_BOX_TYPE_TREX;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err trex_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->trackID);
	gf_bs_write_u32(bs, ptr->def_sample_desc_index);
	gf_bs_write_u32(bs, ptr->def_sample_duration);
	gf_bs_write_u32(bs, ptr->def_sample_size);
	gf_bs_write_u32(bs, ptr->def_sample_flags);
	return GF_OK;
}

GF_Err trex_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 20;
	return GF_OK;
}



#endif //GPAC_READ_ONLY

#endif


#ifndef	GPAC_ISOM_NO_FRAGMENTS

void trun_del(GF_Box *s)
{
	GF_TrunEntry *p;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;
	if (ptr == NULL) return;
	
	while (gf_list_count(ptr->entries)) {
		p = (GF_TrunEntry*)gf_list_get(ptr->entries, 0);
		gf_list_rem(ptr->entries, 0);
		free(p);
	}
	gf_list_del(ptr->entries);
	if (ptr->cache) gf_bs_del(ptr->cache);
	free(ptr);
}

GF_Err trun_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrunEntry *p;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;
	
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	//check this is a good file
	if ((ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) && (ptr->flags & GF_ISOM_TRUN_FLAGS)) 
		return GF_ISOM_INVALID_FILE;

	ptr->sample_count = gf_bs_read_u32(bs);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		ptr->data_offset = gf_bs_read_u32(bs);
		ptr->size -= 4;
	}
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) {
		ptr->first_sample_flags = gf_bs_read_u32(bs);
		ptr->size -= 4;
	}

	//read each entry (even though nothing may be written)
	for (i=0; i<ptr->sample_count; i++) {
		u32 trun_size = 0;
		p = (GF_TrunEntry *) malloc(sizeof(GF_TrunEntry));
		memset(p, 0, sizeof(GF_TrunEntry));

		if (ptr->flags & GF_ISOM_TRUN_DURATION) {
			p->Duration = gf_bs_read_u32(bs);
			trun_size += 4;
		}
		if (ptr->flags & GF_ISOM_TRUN_SIZE) {
			p->size = gf_bs_read_u32(bs);
			trun_size += 4;
		}
		//SHOULDN'T BE USED IF GF_ISOM_TRUN_FIRST_FLAG IS DEFINED
		if (ptr->flags & GF_ISOM_TRUN_FLAGS) {
			p->flags = gf_bs_read_u32(bs);
			trun_size += 4;
		}
		if (ptr->flags & GF_ISOM_TRUN_CTS_OFFSET) {
			p->CTS_Offset = gf_bs_read_u32(bs);
		}
		gf_list_add(ptr->entries, p);
		if (ptr->size<trun_size) return GF_ISOM_INVALID_FILE;
		ptr->size-=trun_size;
	}	
	return GF_OK;
}

GF_Box *trun_New()
{
	GF_TrackFragmentRunBox *tmp = (GF_TrackFragmentRunBox *) malloc(sizeof(GF_TrackFragmentRunBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_TrackFragmentRunBox));
	tmp->type = GF_ISOM_BOX_TYPE_TRUN;
	tmp->entries = gf_list_new();
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY


GF_Err trun_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_TrunEntry *p;
	GF_Err e;
	u32 i, count;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->sample_count);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		gf_bs_write_u32(bs, ptr->data_offset);
	}
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) {
		gf_bs_write_u32(bs, ptr->first_sample_flags);
	}

	//if nothing to do, this will be skipped automatically
	count = gf_list_count(ptr->entries);
	for (i=0; i<count; i++) {
		p = (GF_TrunEntry*)gf_list_get(ptr->entries, i);

		if (ptr->flags & GF_ISOM_TRUN_DURATION) {
			gf_bs_write_u32(bs, p->Duration);
		}
		if (ptr->flags & GF_ISOM_TRUN_SIZE) {
			gf_bs_write_u32(bs, p->size);
		}
		//SHOULDN'T BE USED IF GF_ISOM_TRUN_FIRST_FLAG IS DEFINED
		if (ptr->flags & GF_ISOM_TRUN_FLAGS) {
			gf_bs_write_u32(bs, p->flags);
		}
		if (ptr->flags & GF_ISOM_TRUN_CTS_OFFSET) {
			gf_bs_write_u32(bs, p->CTS_Offset);
		}
	}	
	return GF_OK;
}

GF_Err trun_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, count;
	GF_TrunEntry *p;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	
	ptr->size += 4;
	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) ptr->size += 4;

	//if nothing to do, this will be skipped automatically
	count = gf_list_count(ptr->entries);
	for (i=0; i<count; i++) {
		p = (GF_TrunEntry*)gf_list_get(ptr->entries, i);
		if (ptr->flags & GF_ISOM_TRUN_DURATION) ptr->size += 4;
		if (ptr->flags & GF_ISOM_TRUN_SIZE) ptr->size += 4;
		//SHOULDN'T BE USED IF GF_ISOM_TRUN_FIRST_FLAG IS DEFINED
		if (ptr->flags & GF_ISOM_TRUN_FLAGS) ptr->size += 4;
		if (ptr->flags & GF_ISOM_TRUN_CTS_OFFSET) ptr->size += 4;
	}
	
	return GF_OK;
}



#endif //GPAC_READ_ONLY

#endif 

void tsro_del(GF_Box *s)
{
	GF_TimeOffHintEntryBox *tsro = (GF_TimeOffHintEntryBox *)s;
	free(tsro);
}

GF_Err tsro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeOffHintEntryBox *ptr = (GF_TimeOffHintEntryBox *)s;
	ptr->TimeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tsro_New()
{
	GF_TimeOffHintEntryBox *tmp = (GF_TimeOffHintEntryBox *) malloc(sizeof(GF_TimeOffHintEntryBox));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_TimeOffHintEntryBox));
	tmp->type = GF_ISOM_BOX_TYPE_TSRO;
	return (GF_Box *)tmp;
}


#ifndef GPAC_READ_ONLY
GF_Err tsro_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TimeOffHintEntryBox *ptr = (GF_TimeOffHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->TimeOffset);
	return GF_OK;
}

GF_Err tsro_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 4;
	return GF_OK;
}
#endif


void udta_del(GF_Box *s)
{
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	if (ptr == NULL) return;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		gf_isom_box_array_del(map->boxList);
		free(map);
	}
	gf_list_del(ptr->recordList);
	free(ptr);
}

GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 box_type, bin128 *uuid)
{
	u32 i;
	GF_UserDataMap *map;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		if (map->boxType == box_type) {
			if ((box_type != GF_ISOM_BOX_TYPE_UUID) || !uuid) return map;
			if (!memcmp(map->uuid, *uuid, 16)) return map;
		}
	}
	return NULL;
}

GF_Err udta_AddBox(GF_UserDataBox *ptr, GF_Box *a)
{
	GF_Err e;
	GF_UserDataMap *map;
	if (!ptr) return GF_BAD_PARAM;
	if (!a) return GF_OK;

	map = udta_getEntry(ptr, a->type, (a->type==GF_ISOM_BOX_TYPE_UUID) ? & ((GF_UUIDBox *)a)->uuid : NULL);
	if (map == NULL) {
		map = (GF_UserDataMap *) malloc(sizeof(GF_UserDataMap));
		if (map == NULL) return GF_OUT_OF_MEM;
		memset(map, 0, sizeof(GF_UserDataMap));
		
		map->boxType = a->type;
		if (a->type == GF_ISOM_BOX_TYPE_UUID) 
			memcpy(map->uuid, ((GF_UUIDBox *)a)->uuid, 16);
		map->boxList = gf_list_new();
		if (!map->boxList) {
			free(map);
			return GF_OUT_OF_MEM;
		}
		e = gf_list_add(ptr->recordList, map);
		if (e) return e;
	}
	return gf_list_add(map->boxList, a);
}


GF_Err udta_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	while (ptr->size) {
		/*if no udta type coded, break*/
		sub_type = gf_bs_peek_bits(bs, 32, 0);
		if (sub_type) {
			e = gf_isom_parse_box(&a, bs);
			if (e) return e;
			e = udta_AddBox(ptr, a);
			if (e) return e;
			if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
			ptr->size -= a->size;
		} else {
			gf_bs_read_u32(bs);
			ptr->size -= 4;
		}
	}
	return GF_OK;
}

GF_Box *udta_New()
{
	GF_UserDataBox *tmp = (GF_UserDataBox *) malloc(sizeof(GF_UserDataBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_UserDataBox));
	tmp->recordList = gf_list_new();
	if (!tmp->recordList) {
		free(tmp);
		return NULL;
	}
	tmp->type = GF_ISOM_BOX_TYPE_UDTA;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err udta_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		//warning: here we are not passing the actual "parent" of the list
		//but the UDTA box. The parent itself is not an box, we don't care about it
		e = gf_isom_box_array_write(s, map->boxList, bs);
		if (e) return e;
    }
	return GF_OK;
}

GF_Err udta_Size(GF_Box *s)
{
	GF_Err e;
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		//warning: here we are not passing the actual "parent" of the list
		//but the UDTA box. The parent itself is not an box, we don't care about it
		e = gf_isom_box_array_size(s, map->boxList);
		if (e) return e;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void vmhd_del(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	if (ptr == NULL) return;
	free(ptr);
}


GF_Err vmhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->reserved = gf_bs_read_u64(bs);
	return GF_OK;
}

GF_Box *vmhd_New()
{
	GF_VideoMediaHeaderBox *tmp = (GF_VideoMediaHeaderBox *) malloc(sizeof(GF_VideoMediaHeaderBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_VideoMediaHeaderBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	tmp->type = GF_ISOM_BOX_TYPE_VMHD;
	return (GF_Box *)tmp;
}


//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err vmhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->reserved);
	return GF_OK;
}

GF_Err vmhd_Size(GF_Box *s)
{
	GF_Err e;
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 8;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


void void_del(GF_Box *s)
{
	free(s);
}


GF_Err void_Read(GF_Box *s, GF_BitStream *bs)
{
	if (s->size) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

GF_Box *void_New()
{
	GF_Box *tmp = (GF_Box *) malloc(sizeof(GF_Box));
	if (!tmp) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_VOID;
	return tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err void_Write(GF_Box *s, GF_BitStream *bs)
{
	gf_bs_write_u32(bs, 0);
	return GF_OK;
}

GF_Err void_Size(GF_Box *s)
{
	s->size = 4;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



GF_Box *pdin_New()
{
	GF_ProgressiveDownloadBox *tmp = (GF_ProgressiveDownloadBox*) malloc(sizeof(GF_ProgressiveDownloadBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ProgressiveDownloadBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	tmp->type = GF_ISOM_BOX_TYPE_PDIN;
	return (GF_Box *)tmp;
}


void pdin_del(GF_Box *s)
{
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;
	if (ptr == NULL) return;
	if (ptr->rates) free(ptr->rates);
	if (ptr->times) free(ptr->times);
	free(ptr);
}


GF_Err pdin_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->count = (u32) (ptr->size) / 8;
	ptr->rates = (u32*)malloc(sizeof(u32)*ptr->count);
	ptr->times = (u32*)malloc(sizeof(u32)*ptr->count);
	for (i=0; i<ptr->count; i++) {
		ptr->rates[i] = gf_bs_read_u32(bs);
		ptr->times[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err pdin_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	for (i=0; i<ptr->count; i++) {
		gf_bs_write_u32(bs, ptr->rates[i]);
		gf_bs_write_u32(bs, ptr->times[i]);
	}
	return GF_OK;
}

GF_Err pdin_Size(GF_Box *s)
{
	GF_Err e;
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 8*ptr->count;
	return GF_OK;
}

#endif //GPAC_READ_ONLY




GF_Box *sdtp_New()
{
	GF_SampleDependencyTypeBox *tmp = (GF_SampleDependencyTypeBox*) malloc(sizeof(GF_SampleDependencyTypeBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_SampleDependencyTypeBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	tmp->type = GF_ISOM_BOX_TYPE_SDTP;
	return (GF_Box *)tmp;
}


void sdtp_del(GF_Box *s)
{
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;
	if (ptr == NULL) return;
	if (ptr->sample_info) free(ptr->sample_info);
	free(ptr);
}


GF_Err sdtp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	/*out-of-order sdtp, assume no padding at the end*/
	if (!ptr->sampleCount) ptr->sampleCount = (u32) (ptr->size - 8);
	ptr->sample_info = (u8 *) malloc(sizeof(u8)*ptr->sampleCount);
	gf_bs_read_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	ptr->size -= ptr->sampleCount;
	return GF_OK;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err sdtp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	return GF_OK;
}

GF_Err sdtp_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->sampleCount;
	return GF_OK;
}

#endif //GPAC_READ_ONLY


GF_Box *pasp_New()
{
	GF_PixelAspectRatioBox *tmp;
	GF_SAFEALLOC(tmp, GF_PixelAspectRatioBox);
	if (tmp == NULL) return NULL;
	tmp->type = GF_ISOM_BOX_TYPE_PASP;
	return (GF_Box *)tmp;
}


void pasp_del(GF_Box *s)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	if (ptr == NULL) return;
	free(ptr);
}


GF_Err pasp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	ptr->hSpacing = gf_bs_read_u32(bs);
	ptr->vSpacing = gf_bs_read_u32(bs);
	ptr->size -= 8;
	return GF_OK;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err pasp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->hSpacing);
	gf_bs_write_u32(bs, ptr->vSpacing);
	return GF_OK;
}

GF_Err pasp_Size(GF_Box *s)
{
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	return GF_OK;
}

#endif //GPAC_READ_ONLY




GF_Box *metx_New(u32 type)
{
	GF_MetaDataSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_MetaDataSampleEntryBox);
	if (tmp == NULL) return NULL;
	tmp->type = type;
	return (GF_Box *)tmp;
}


void metx_del(GF_Box *s)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;
	if (ptr == NULL) return;
	if (ptr->content_encoding) free(ptr->content_encoding);
	if (ptr->mime_type_or_namespace) free(ptr->mime_type_or_namespace);
	if (ptr->xml_schema_loc) free(ptr->xml_schema_loc);
	if (ptr->bitrate) gf_isom_box_del((GF_Box *) ptr->bitrate);
	free(ptr);
}


GF_Err metx_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_SINF:
		if (ptr->protection_info) return GF_ISOM_INVALID_FILE;
		ptr->protection_info = (GF_ProtectionInfoBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_BTRT:
		if (ptr->bitrate) return GF_ISOM_INVALID_FILE;
		ptr->bitrate = (GF_MPEG4BitRateBox *)a;
		break;
	default:
		gf_isom_box_del(a);
		break;
	}
	return GF_OK;
}

GF_Err metx_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 size, i;
	char *str;
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;
	size = (u32) ptr->size;
	str = malloc(sizeof(char)*size);
	i=0;
	while (i<size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) ptr->content_encoding = strdup(str);

	i=0;
	while (i<size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) ptr->mime_type_or_namespace = strdup(str);

	if (ptr->type==GF_ISOM_BOX_TYPE_METX) {
		i=0;
		while (i<size) {
			str[i] = gf_bs_read_u8(bs);
			size--;
			if (!str[i])
				break;
			i++;
		}
		if (i) ptr->xml_schema_loc = strdup(str);
	}
	ptr->size = size;
	return gf_isom_read_box_list(s, bs, metx_AddBox);
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err metx_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);

	if (ptr->content_encoding) 
		gf_bs_write_data(bs, ptr->content_encoding, strlen(ptr->content_encoding));
	gf_bs_write_u8(bs, 0);

	if (ptr->mime_type_or_namespace) 
		gf_bs_write_data(bs, ptr->mime_type_or_namespace, strlen(ptr->mime_type_or_namespace));
	gf_bs_write_u8(bs, 0);
	
	if (ptr->xml_schema_loc) 
		gf_bs_write_data(bs, ptr->xml_schema_loc, strlen(ptr->xml_schema_loc));
	gf_bs_write_u8(bs, 0);

	if (ptr->bitrate) {
		e = gf_isom_box_write((GF_Box *)ptr->bitrate, bs);
		if (e) return e;
	}
	if (ptr->protection_info) {
		e = gf_isom_box_write((GF_Box *)ptr->protection_info, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err metx_Size(GF_Box *s)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 8;

	if (ptr->content_encoding)
		ptr->size += strlen(ptr->content_encoding);
	ptr->size++;
	if (ptr->mime_type_or_namespace)
		ptr->size += strlen(ptr->mime_type_or_namespace);
	ptr->size++;
	if (ptr->xml_schema_loc)
		ptr->size += strlen(ptr->xml_schema_loc);
	ptr->size++;

	if (ptr->bitrate) {
		e = gf_isom_box_size((GF_Box *)ptr->bitrate);
		if (e) return e;
		ptr->size += ptr->bitrate->size;
	}
	if (ptr->protection_info) {
		e = gf_isom_box_size((GF_Box *)ptr->protection_info);
		if (e) return e;
		ptr->size += ptr->protection_info->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY



GF_Box *dac3_New()
{
	GF_AC3ConfigBox *tmp = (GF_AC3ConfigBox *) malloc(sizeof(GF_AC3ConfigBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_AC3ConfigBox));
	tmp->type = GF_ISOM_BOX_TYPE_DAC3;
	return (GF_Box *)tmp;
}

void dac3_del(GF_Box *s)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	free(ptr);
}


GF_Err dac3_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	ptr->cfg.fscod = gf_bs_read_int(bs, 2);
	ptr->cfg.bsid = gf_bs_read_int(bs, 5);
	ptr->cfg.bsmod = gf_bs_read_int(bs, 3);
	ptr->cfg.acmod = gf_bs_read_int(bs, 3);
	ptr->cfg.lfon = gf_bs_read_int(bs, 1);
	ptr->cfg.brcode = gf_bs_read_int(bs, 5);
	gf_bs_read_int(bs, 5);
	return GF_OK;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err dac3_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->cfg.fscod, 2);
	gf_bs_write_int(bs, ptr->cfg.bsid, 5);
	gf_bs_write_int(bs, ptr->cfg.bsmod, 3);
	gf_bs_write_int(bs, ptr->cfg.acmod, 3);
	gf_bs_write_int(bs, ptr->cfg.lfon, 1);
	gf_bs_write_int(bs, ptr->cfg.brcode, 5);
	gf_bs_write_int(bs, 0, 5);
	return GF_OK;
}

GF_Err dac3_Size(GF_Box *s)
{
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 3;
	return GF_OK;
}

#endif //GPAC_READ_ONLY



void ac3_del(GF_Box *s)
{
	GF_AC3SampleEntryBox *ptr = (GF_AC3SampleEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	if (ptr->protection_info) gf_isom_box_del((GF_Box *)ptr->protection_info);
	free(ptr);
}


GF_Err ac3_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AC3SampleEntryBox *ptr = (GF_AC3SampleEntryBox *)s;
	e = gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox*)s, bs);
	if (e) return e;
	e = gf_isom_parse_box((GF_Box **)&ptr->info, bs);
	if (e) return e;
	return GF_OK;
}

GF_Box *ac3_New()
{
	GF_AC3SampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_AC3SampleEntryBox);
	if (tmp == NULL) return NULL;
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_AC3;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err ac3_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	return gf_isom_box_write((GF_Box *)ptr->info, bs);
}

GF_Err ac3_Size(GF_Box *s)
{
	GF_Err e;
	GF_3GPPAudioSampleEntryBox *ptr = (GF_3GPPAudioSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);
	e = gf_isom_box_size((GF_Box *)ptr->info);
	if (e) return e;
	ptr->size += ptr->info->size;
	return GF_OK;
}

#endif //GPAC_READ_ONLY
