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



#ifndef GPAC_DISABLE_ISOM

void co64_del(GF_Box *s)
{
	GF_ChunkLargeOffsetBox *ptr;
	ptr = (GF_ChunkLargeOffsetBox *) s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	gf_free(ptr);
}

GF_Err co64_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 entries;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->offsets = (u64 *) gf_malloc(ptr->nb_entries * sizeof(u64) );
	if (ptr->offsets == NULL) return GF_OUT_OF_MEM;
	ptr->alloc_size = ptr->nb_entries;
	for (entries = 0; entries < ptr->nb_entries; entries++) {
		ptr->offsets[entries] = gf_bs_read_u64(bs);
	}
	return GF_OK;
}

GF_Box *co64_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ChunkLargeOffsetBox, GF_ISOM_BOX_TYPE_CO64);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void cprt_del(GF_Box *s)
{
	GF_CopyrightBox *ptr = (GF_CopyrightBox *) s;
	if (ptr == NULL) return;
	if (ptr->notice)
		gf_free(ptr->notice);
	gf_free(ptr);
}


GF_Box *chpl_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ChapterListBox, GF_ISOM_BOX_TYPE_CHPL);
	tmp->list = gf_list_new();
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->version = 1;
	return (GF_Box *)tmp;
}

void chpl_del(GF_Box *s)
{
	GF_ChapterListBox *ptr = (GF_ChapterListBox *) s;
	if (ptr == NULL) return;
	while (gf_list_count(ptr->list)) {
		GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(ptr->list, 0);
		if (ce->name) gf_free(ce->name);
		gf_free(ce);
		gf_list_rem(ptr->list, 0);
	}
	gf_list_del(ptr->list);
	gf_free(ptr);
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
		if (!ce) return GF_OUT_OF_MEM;
		ce->start_time = gf_bs_read_u64(bs);
		len = gf_bs_read_u8(bs);
		if (len) {
			ce->name = (char *)gf_malloc(sizeof(char)*(len+1));
			gf_bs_read_data(bs, ce->name, len);
			ce->name[len] = 0;
		} else {
			ce->name = gf_strdup("");
		}

		for (i=0; i<count; i++) {
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

#ifndef GPAC_DISABLE_ISOM_WRITE

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
			len = (u32) strlen(ce->name);
			if (len>255) len = 255;
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


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
		ptr->notice = (char*)gf_malloc(bytesToRead * sizeof(char));
		if (ptr->notice == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->notice, bytesToRead);
	}
	return GF_OK;
}

GF_Box *cprt_New()
{
	ISOM_DECL_BOX_ALLOC(GF_CopyrightBox, GF_ISOM_BOX_TYPE_CPRT);

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->packedLanguageCode[0] = 'u';
	tmp->packedLanguageCode[1] = 'n';
	tmp->packedLanguageCode[2] = 'd';

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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
		gf_bs_write_data(bs, ptr->notice, (u32) (strlen(ptr->notice) + 1) );
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void kind_del(GF_Box *s)
{
	GF_KindBox *ptr = (GF_KindBox *) s;
	if (ptr == NULL) return;
	if (ptr->schemeURI) gf_free(ptr->schemeURI);
	if (ptr->value) gf_free(ptr->value);
	gf_free(ptr);
}

GF_Err kind_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_KindBox *ptr = (GF_KindBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	if (ptr->size) {
		u32 bytesToRead = (u32) ptr->size;
		char *data;
		u32 schemeURIlen;
		data = (char*)gf_malloc(bytesToRead * sizeof(char));
		if (data == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, data, bytesToRead);
		/*safety check in case the string is not null-terminated*/
		if (data[bytesToRead-1]) {
			char *str = (char*)gf_malloc((u32) bytesToRead + 1);
			memcpy(str, data, (u32) bytesToRead);
			str[ptr->size] = 0;
			gf_free(data);
			data = str;
			bytesToRead++;
		}
		ptr->schemeURI = gf_strdup(data);
		schemeURIlen = (u32) strlen(data);
		if (bytesToRead > schemeURIlen+1) {
			/* read the value */
			char *data_value = data + schemeURIlen +1;
			ptr->value = gf_strdup(data_value);
		}
		gf_free(data);
	}
	return GF_OK;
}

GF_Box *kind_New()
{
	ISOM_DECL_BOX_ALLOC(GF_KindBox, GF_ISOM_BOX_TYPE_KIND);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err kind_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_KindBox *ptr = (GF_KindBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->schemeURI, (u32) (strlen(ptr->schemeURI) + 1 ));
	if (ptr->value) {
		gf_bs_write_data(bs, ptr->value, (u32) (strlen(ptr->value) + 1) );
	}
	return GF_OK;
}

GF_Err kind_Size(GF_Box *s)
{
	GF_Err e;
	GF_KindBox *ptr = (GF_KindBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += strlen(ptr->schemeURI) + 1;
	if (ptr->value) {
		ptr->size += strlen(ptr->value) + 1;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ctts_del(GF_Box *s)
{
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
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
	ptr->entries = (GF_DttsEntry *)gf_malloc(sizeof(GF_DttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	sampleCount = 0;
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		if (ptr->version)
			ptr->entries[i].decodingOffset = gf_bs_read_int(bs, 32);
		else
			ptr->entries[i].decodingOffset = (s32) gf_bs_read_u32(bs);
		sampleCount += ptr->entries[i].sampleCount;
	}
#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_LastSampleNumber = sampleCount;
#endif
	return GF_OK;
}

GF_Box *ctts_New()
{
	ISOM_DECL_BOX_ALLOC(GF_CompositionOffsetBox, GF_ISOM_BOX_TYPE_CTTS);

	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *) tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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
		if (ptr->version) {
			gf_bs_write_int(bs, ptr->entries[i].decodingOffset, 32);
		} else {
			gf_bs_write_u32(bs, (u32) ptr->entries[i].decodingOffset);
		}
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void cslg_del(GF_Box *s)
{
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
	return;
}

GF_Err cslg_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->compositionToDTSShift = gf_bs_read_int(bs, 32);
	ptr->leastDecodeToDisplayDelta = gf_bs_read_int(bs, 32);
	ptr->greatestDecodeToDisplayDelta = gf_bs_read_int(bs, 32);
	ptr->compositionStartTime = gf_bs_read_int(bs, 32);
	ptr->compositionEndTime = gf_bs_read_int(bs, 32);
	return GF_OK;
}

GF_Box *cslg_New()
{
	ISOM_DECL_BOX_ALLOC(GF_CompositionToDecodeBox, GF_ISOM_BOX_TYPE_CSLG);

	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err cslg_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->compositionToDTSShift, 32);
	gf_bs_write_int(bs, ptr->leastDecodeToDisplayDelta, 32);
	gf_bs_write_int(bs, ptr->greatestDecodeToDisplayDelta, 32);
	gf_bs_write_int(bs, ptr->compositionStartTime, 32);
	gf_bs_write_int(bs, ptr->compositionEndTime, 32);
	return GF_OK;
}

GF_Err cslg_Size(GF_Box *s)
{
	GF_Err e;
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 20;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void url_del(GF_Box *s)
{
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) gf_free(ptr->location);
	gf_free(ptr);
	return;
}


GF_Err url_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->size) {
		ptr->location = (char*)gf_malloc((u32) ptr->size);
		if (! ptr->location) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->location, (u32)ptr->size);
	}
	return GF_OK;
}

GF_Box *url_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryURLBox, GF_ISOM_BOX_TYPE_URL);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void urn_del(GF_Box *s)
{
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) gf_free(ptr->location);
	if (ptr->nameURN) gf_free(ptr->nameURN);
	gf_free(ptr);
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
	tmpName = (char*)gf_malloc(sizeof(char) * to_read);
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
		gf_free(tmpName);
		return GF_ISOM_INVALID_FILE;
	}
	//no NULL char, URL is not specified
	if (i == to_read - 1) {
		ptr->nameURN = tmpName;
		ptr->location = NULL;
		return GF_OK;
	}
	//OK, this has both URN and URL
	ptr->nameURN = (char*)gf_malloc(sizeof(char) * (i+1));
	if (!ptr->nameURN) {
		gf_free(tmpName);
		return GF_OUT_OF_MEM;
	}
	ptr->location = (char*)gf_malloc(sizeof(char) * (to_read - i - 1));
	if (!ptr->location) {
		gf_free(tmpName);
		gf_free(ptr->nameURN);
		ptr->nameURN = NULL;
		return GF_OUT_OF_MEM;
	}
	memcpy(ptr->nameURN, tmpName, i + 1);
	memcpy(ptr->location, tmpName + i + 1, (to_read - i - 1));
	gf_free(tmpName);
	return GF_OK;
}

GF_Box *urn_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryURNBox, GF_ISOM_BOX_TYPE_URN);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE


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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void defa_del(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *) s;
	if (!s) return;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err defa_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (ptr->size > 0xFFFFFFFF) return GF_ISOM_INVALID_FILE;
	bytesToRead = (u32) (ptr->size);

	if (!bytesToRead) return GF_OK;
	if (bytesToRead>1000000) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Unknown box %s (0x%08X) with payload larger than 1 MBytes, ignoring\n", gf_4cc_to_str(ptr->type), ptr->type ));
		gf_bs_skip_bytes(bs, ptr->dataSize);
		return GF_OK;
	}

	ptr->data = (char*)gf_malloc(bytesToRead);
	if (ptr->data == NULL ) return GF_OUT_OF_MEM;
	ptr->dataSize = bytesToRead;
	gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	return GF_OK;
}

//warning: we don't have any boxType, trick has to be done while creating..
GF_Box *defa_New()
{
	ISOM_DECL_BOX_ALLOC(GF_UnknownBox, 0);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err defa_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->dataSize && ptr->data) {
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

	if (ptr->dataSize && ptr->data) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void uuid_del(GF_Box *s)
{
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox *) s;
	if (!s) return;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err uuid_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox *)s;
	if (ptr->size > 0xFFFFFFFF) return GF_ISOM_INVALID_FILE;
	bytesToRead = (u32) (ptr->size);

	if (bytesToRead) {
		ptr->data = (char*)gf_malloc(bytesToRead);
		if (ptr->data == NULL ) return GF_OUT_OF_MEM;
		ptr->dataSize = bytesToRead;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Box *uuid_New()
{
	ISOM_DECL_BOX_ALLOC(GF_UnknownUUIDBox, GF_ISOM_BOX_TYPE_UUID);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void dinf_del(GF_Box *s)
{
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_del((GF_Box *)ptr->dref);
	gf_free(ptr);
}


GF_Err dinf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_DREF:
		if (ptr->dref) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->dref = (GF_DataReferenceBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err dinf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_read_box_list(s, bs, dinf_AddBox);
	if (e) {
		return e;
	}
	if (!((GF_DataInformationBox *)s)->dref) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Missing dref box in dinf\n"));
		((GF_DataInformationBox *)s)->dref = (GF_DataReferenceBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DREF);
	}
	return GF_OK;
}

GF_Box *dinf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataInformationBox, GF_ISOM_BOX_TYPE_DINF);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (e) return e;
	if (ptr->dref) {
		e = gf_isom_box_size((GF_Box *) ptr->dref);
		if (e) return e;
		ptr->size += ptr->dref->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void dref_del(GF_Box *s)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *) s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err dref_AddDataEntry(GF_Box *ptr, GF_Box *entry)
{
	if (entry->type==GF_4CC('a','l','i','s')) {
		GF_DataEntryURLBox *urle = (GF_DataEntryURLBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_URL);
		urle->flags = 1;
		gf_isom_box_del(entry);
		gf_isom_box_add_default(ptr, (GF_Box *)urle);
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] Apple \'alis\' box found, not supported - converting to self-pointing \'url \' \n" ));
	} else {
		return gf_isom_box_add_default(ptr, entry);
	}
	return GF_OK;
}

GF_Err dref_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	//u32 count;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;

	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	//count =
	gf_bs_read_u32(bs);
	ptr->size -= 4;

	return gf_isom_read_box_list(s, bs, dref_AddDataEntry);
}

GF_Box *dref_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataReferenceBox, GF_ISOM_BOX_TYPE_DREF);
	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dref_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = ptr->other_boxes ? gf_list_count(ptr->other_boxes) : 0;
	gf_bs_write_u32(bs, count);
	return GF_OK;
}

GF_Err dref_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void edts_del(GF_Box *s)
{
	GF_EditBox *ptr = (GF_EditBox *) s;
	gf_isom_box_del((GF_Box *)ptr->editList);
	gf_free(ptr);
}


GF_Err edts_AddBox(GF_Box *s, GF_Box *a)
{
	GF_EditBox *ptr = (GF_EditBox *)s;
	if (a->type == GF_ISOM_BOX_TYPE_ELST) {
		if (ptr->editList) return GF_BAD_PARAM;
		ptr->editList = (GF_EditListBox *)a;
		return GF_OK;
	} else {
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err edts_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, edts_AddBox);
}

GF_Box *edts_New()
{
	ISOM_DECL_BOX_ALLOC(GF_EditBox, GF_ISOM_BOX_TYPE_EDTS);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err edts_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_EditBox *ptr = (GF_EditBox *)s;

	//here we have a trick: if editList is empty, skip the box
	if (ptr->editList && gf_list_count(ptr->editList->entryList)) {
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
	if (!ptr->editList || ! gf_list_count(ptr->editList->entryList)) {
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

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
		if (p) gf_free(p);
	}
	gf_list_del(ptr->entryList);
	gf_free(ptr);
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
		p = (GF_EdtsEntry *) gf_malloc(sizeof(GF_EdtsEntry));
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
	ISOM_DECL_BOX_ALLOC(GF_EditListBox, GF_ISOM_BOX_TYPE_ELST);

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->entryList = gf_list_new();
	if (!tmp->entryList) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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


#endif /*GPAC_DISABLE_ISOM_WRITE*/

void esds_del(GF_Box *s)
{
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	if (ptr == NULL)	return;
	if (ptr->desc) gf_odf_desc_del((GF_Descriptor *)ptr->desc);
	gf_free(ptr);
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
		enc_desc = (char*)gf_malloc(sizeof(char) * descSize);
		if (!enc_desc) return GF_OUT_OF_MEM;
		//get the payload
		gf_bs_read_data(bs, enc_desc, descSize);
		//send it to the OD Codec
		e = gf_odf_desc_read(enc_desc, descSize, (GF_Descriptor **) &ptr->desc);
		//OK, free our desc
		gf_free(enc_desc);
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
	ISOM_DECL_BOX_ALLOC(GF_ESDBox, GF_ISOM_BOX_TYPE_ESDS);

	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	gf_free(enc_desc);
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void free_del(GF_Box *s)
{
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err free_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;

	if (ptr->size > 0xFFFFFFFF) return GF_IO_ERR;

	bytesToRead = (u32) (ptr->size);

	if (bytesToRead) {
		ptr->data = (char*)gf_malloc(bytesToRead * sizeof(char));
		gf_bs_read_data(bs, ptr->data, bytesToRead);
		ptr->dataSize = bytesToRead;
	}
	return GF_OK;
}

GF_Box *free_New()
{
	ISOM_DECL_BOX_ALLOC(GF_FreeSpaceBox, GF_ISOM_BOX_TYPE_FREE);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err free_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	if (ptr->original_4cc) {
		u32 t = s->type;
		s->type=ptr->original_4cc;
		e = gf_isom_box_write_header(s, bs);
		s->type=t;
	} else {
		e = gf_isom_box_write_header(s, bs);
	}
	if (e) return e;
	if (ptr->dataSize)	{
		if (ptr->data) {
			gf_bs_write_data(bs, ptr->data, ptr->dataSize);
		} else {
			u32 i = 0;
			while (i<ptr->dataSize) {
				gf_bs_write_u8(bs, 0);
				i++;
			}
		}
	}
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ftyp_del(GF_Box *s)
{
	GF_FileTypeBox *ptr = (GF_FileTypeBox *) s;
	if (ptr->altBrand) gf_free(ptr->altBrand);
	gf_free(ptr);
}

GF_Box *ftyp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_FileTypeBox, GF_ISOM_BOX_TYPE_FTYP);
	return (GF_Box *)tmp;
}

GF_Err ftyp_Read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;

	if (ptr->size < 8) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Found ftyp with size < 8, likely broken!\n"));
		return GF_BAD_PARAM;
	}
	ptr->majorBrand = gf_bs_read_u32(bs);
	ptr->minorVersion = gf_bs_read_u32(bs);
	ptr->size -= 8;

	ptr->altCount = ( (u32) (ptr->size)) / 4;
	if (!ptr->altCount) return GF_OK;
	if (ptr->altCount * 4 != (u32) (ptr->size)) return GF_ISOM_INVALID_FILE;

	ptr->altBrand = (u32*)gf_malloc(sizeof(u32)*ptr->altCount);
	for (i = 0; i<ptr->altCount; i++) {
		ptr->altBrand[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gnrm_del(GF_Box *s)
{
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnrm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericSampleEntryBox, GF_ISOM_BOX_TYPE_GNRM);

	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gnrv_del(GF_Box *s)
{
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnrv_New()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericVisualSampleEntryBox, GF_ISOM_BOX_TYPE_GNRV);
	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gnra_del(GF_Box *s)
{
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnra_New()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericAudioSampleEntryBox, GF_ISOM_BOX_TYPE_GNRA);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


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
	if (ptr->data) {
		gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	}
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void hdlr_del(GF_Box *s)
{
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	if (ptr == NULL) return;
	if (ptr->nameUTF8) gf_free(ptr->nameUTF8);
	gf_free(ptr);
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
		ptr->nameUTF8 = (char*)gf_malloc((u32) ptr->size);
		if (ptr->nameUTF8 == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->nameUTF8, (u32) ptr->size);
		/*safety check in case the string is not null-terminated*/
		if (ptr->nameUTF8[ptr->size-1]) {
			char *str = (char*)gf_malloc((u32) ptr->size + 1);
			memcpy(str, ptr->nameUTF8, (u32) ptr->size);
			str[ptr->size] = 0;
			gf_free(ptr->nameUTF8);
			ptr->nameUTF8 = str;
		}
	}
	return GF_OK;
}

GF_Box *hdlr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HandlerBox, GF_ISOM_BOX_TYPE_HDLR);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err hdlr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->reserved1);
	gf_bs_write_u32(bs, ptr->handlerType);
	gf_bs_write_data(bs, (char*)ptr->reserved2, 12);
	if (ptr->nameUTF8) gf_bs_write_data(bs, ptr->nameUTF8, (u32) strlen(ptr->nameUTF8));
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void hinf_del(GF_Box *s)
{
	GF_HintInfoBox *hinf = (GF_HintInfoBox *)s;
	gf_free(hinf);
}

GF_Box *hinf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HintInfoBox, GF_ISOM_BOX_TYPE_HINF);

	tmp->other_boxes = gf_list_new();
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
		while ((maxR = (GF_MAXRBox *)gf_list_enum(hinf->other_boxes, &i))) {
			if ((maxR->type==GF_ISOM_BOX_TYPE_MAXR) && (maxR->granularity == ((GF_MAXRBox *)a)->granularity))
				return GF_ISOM_INVALID_FILE;
		}
		break;
	}
	return gf_isom_box_add_default(s, a);
}


GF_Err hinf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, hinf_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err hinf_Write(GF_Box *s, GF_BitStream *bs)
{
//	GF_HintInfoBox *ptr = (GF_HintInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err hinf_Size(GF_Box *s)
{
//	GF_HintInfoBox *ptr = (GF_HintInfoBox *)s;
	return gf_isom_box_get_size(s);
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void hmhd_del(GF_Box *s)
{
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	ISOM_DECL_BOX_ALLOC(GF_HintMediaHeaderBox, GF_ISOM_BOX_TYPE_HMHD);

	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hnti_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HintTrackInfoBox, GF_ISOM_BOX_TYPE_HNTI);

	tmp->hints = gf_list_new();
	if (!tmp->hints) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}

void hnti_del(GF_Box *a)
{
	GF_Box *t;
	GF_RTPBox *rtp;
	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)a;
	while (gf_list_count(ptr->hints)) {
		t = (GF_Box*)gf_list_get(ptr->hints, 0);
		if (t->type != GF_ISOM_BOX_TYPE_RTP) {
			gf_isom_box_del(t);
		} else {
			rtp = (GF_RTPBox *)t;
			if (rtp->sdpText) gf_free(rtp->sdpText);
			gf_free(rtp);
		}
		gf_list_rem(ptr->hints, 0);
	}
	gf_list_del(ptr->hints);
	gf_free(ptr);
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
	return gf_list_add(hnti->hints, a);
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
			rtp = (GF_RTPBox*)gf_malloc(sizeof(GF_RTPBox));
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
			rtp->sdpText = (char*)gf_malloc(sizeof(char) * (length + 1));
			if (!rtp->sdpText) {
				gf_free(rtp);
				return GF_OUT_OF_MEM;
			}
			gf_bs_read_data(bs, rtp->sdpText, length);
			rtp->sdpText[length] = 0;
//			sr += length;
			e = hnti_AddBox(ptr, (GF_Box *)rtp);
			if (e) return e;
			if (ptr->size<rtp->size) return GF_ISOM_INVALID_FILE;
			ptr->size -= rtp->size;
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
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

	count = gf_list_count(ptr->hints);
	for (i = 0; i < count; i ++) {
		a = (GF_Box*)gf_list_get(ptr->hints, i);
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
			gf_bs_write_data(bs, rtp->sdpText, (u32) strlen(rtp->sdpText));
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

	count = gf_list_count(ptr->hints);
	for (i = 0; i < count; i ++) {
		a = (GF_Box*)gf_list_get(ptr->hints, i);
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		GF_SDPBox
**********************************************************/

void sdp_del(GF_Box *s)
{
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr->sdpText) gf_free(ptr->sdpText);
	gf_free(ptr);

}
GF_Err sdp_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	length = (u32) (ptr->size);
	//sdp text has no delimiter !!!
	ptr->sdpText = (char*)gf_malloc(sizeof(char) * (length+1));
	if (!ptr->sdpText) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->sdpText, length);
	ptr->sdpText[length] = 0;
	return GF_OK;
}
GF_Box *sdp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SDPBox, GF_ISOM_BOX_TYPE_SDP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sdp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//don't write the NULL char!!!
	gf_bs_write_data(bs, ptr->sdpText, (u32) strlen(ptr->sdpText));
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TRPY GF_Box
**********************************************************/

void trpy_del(GF_Box *s)
{
	gf_free((GF_TRPYBox *)s);
}
GF_Err trpy_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TRPYBox *ptr = (GF_TRPYBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *trpy_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TRPYBox, GF_ISOM_BOX_TYPE_TRPY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE

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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		TOTL GF_Box
**********************************************************/

void totl_del(GF_Box *s)
{
	gf_free((GF_TRPYBox *)s);
}
GF_Err totl_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TOTLBox *ptr = (GF_TOTLBox *)s;
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *totl_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TOTLBox, GF_ISOM_BOX_TYPE_TOTL);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		NUMP GF_Box
**********************************************************/

void nump_del(GF_Box *s)
{
	gf_free((GF_NUMPBox *)s);
}
GF_Err nump_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_NUMPBox *ptr = (GF_NUMPBox *)s;
	ptr->nbPackets = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *nump_New()
{
	ISOM_DECL_BOX_ALLOC(GF_NUMPBox, GF_ISOM_BOX_TYPE_NUMP);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		NPCK GF_Box
**********************************************************/

void npck_del(GF_Box *s)
{
	gf_free((GF_NPCKBox *)s);
}
GF_Err npck_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_NPCKBox *ptr = (GF_NPCKBox *)s;
	ptr->nbPackets = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *npck_New()
{
	ISOM_DECL_BOX_ALLOC(GF_NPCKBox, GF_ISOM_BOX_TYPE_NPCK);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TPYL GF_Box
**********************************************************/

void tpyl_del(GF_Box *s)
{
	gf_free((GF_NTYLBox *)s);
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
	ISOM_DECL_BOX_ALLOC(GF_NTYLBox, GF_ISOM_BOX_TYPE_TPYL);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		TPAY GF_Box
**********************************************************/

void tpay_del(GF_Box *s)
{
	gf_free((GF_TPAYBox *)s);
}
GF_Err tpay_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TPAYBox *ptr = (GF_TPAYBox *)s;
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tpay_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TPAYBox, GF_ISOM_BOX_TYPE_TPAY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		MAXR GF_Box
**********************************************************/

void maxr_del(GF_Box *s)
{
	gf_free((GF_MAXRBox *)s);
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
	ISOM_DECL_BOX_ALLOC(GF_MAXRBox, GF_ISOM_BOX_TYPE_MAXR);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		DMED GF_Box
**********************************************************/

void dmed_del(GF_Box *s)
{
	gf_free((GF_DMEDBox *)s);
}
GF_Err dmed_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMEDBox *ptr = (GF_DMEDBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dmed_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DMEDBox, GF_ISOM_BOX_TYPE_DMED);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		DIMM GF_Box
**********************************************************/

void dimm_del(GF_Box *s)
{
	gf_free((GF_DIMMBox *)s);
}
GF_Err dimm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMMBox *ptr = (GF_DIMMBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dimm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMMBox, GF_ISOM_BOX_TYPE_DIMM);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		DREP GF_Box
**********************************************************/

void drep_del(GF_Box *s)
{
	gf_free((GF_DREPBox *)s);
}
GF_Err drep_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DREPBox *ptr = (GF_DREPBox *)s;
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *drep_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DREPBox, GF_ISOM_BOX_TYPE_DREP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/**********************************************************
		TMIN GF_Box
**********************************************************/

void tmin_del(GF_Box *s)
{
	gf_free((GF_TMINBox *)s);
}
GF_Err tmin_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMINBox *ptr = (GF_TMINBox *)s;
	ptr->minTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmin_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TMINBox, GF_ISOM_BOX_TYPE_TMIN);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TMAX GF_Box
**********************************************************/

void tmax_del(GF_Box *s)
{
	gf_free((GF_TMAXBox *)s);
}
GF_Err tmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMAXBox *ptr = (GF_TMAXBox *)s;
	ptr->maxTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmax_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TMAXBox, GF_ISOM_BOX_TYPE_TMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		PMAX GF_Box
**********************************************************/

void pmax_del(GF_Box *s)
{
	gf_free((GF_PMAXBox *)s);
}
GF_Err pmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PMAXBox *ptr = (GF_PMAXBox *)s;
	ptr->maxSize = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *pmax_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PMAXBox, GF_ISOM_BOX_TYPE_PMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		DMAX GF_Box
**********************************************************/

void dmax_del(GF_Box *s)
{
	gf_free((GF_DMAXBox *)s);
}
GF_Err dmax_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMAXBox *ptr = (GF_DMAXBox *)s;
	ptr->maxDur = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *dmax_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DMAXBox, GF_ISOM_BOX_TYPE_DMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		PAYT GF_Box
**********************************************************/

void payt_del(GF_Box *s)
{
	GF_PAYTBox *payt = (GF_PAYTBox *)s;
	if (payt->payloadString) gf_free(payt->payloadString);
	gf_free(payt);
}
GF_Err payt_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;

	ptr->payloadCode = gf_bs_read_u32(bs);
	length = gf_bs_read_u8(bs);
	ptr->payloadString = (char*)gf_malloc(sizeof(char) * (length+1) );
	if (! ptr->payloadString) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->payloadString, length);
	ptr->payloadString[length] = 0;
	ptr->size -= 4+length+1;
	return GF_OK;
}
GF_Box *payt_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PAYTBox, GF_ISOM_BOX_TYPE_PAYT);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err payt_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_Err e;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->payloadCode);
	len = (u32) strlen(ptr->payloadString);
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		PAYT GF_Box
**********************************************************/

void name_del(GF_Box *s)
{
	GF_NameBox *name = (GF_NameBox *)s;
	if (name->string) gf_free(name->string);
	gf_free(name);
}
GF_Err name_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_NameBox *ptr = (GF_NameBox *)s;

	length = (u32) (ptr->size);
	ptr->string = (char*)gf_malloc(sizeof(char) * length);
	if (! ptr->string) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->string, length);
	return GF_OK;
}
GF_Box *name_New()
{
	ISOM_DECL_BOX_ALLOC(GF_NameBox, GF_ISOM_BOX_TYPE_NAME);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err name_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NameBox *ptr = (GF_NameBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->string) {
		gf_bs_write_data(bs, ptr->string, (u32) strlen(ptr->string) + 1);
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void iods_del(GF_Box *s)
{
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;
	if (ptr == NULL) return;
	if (ptr->descriptor) gf_odf_desc_del(ptr->descriptor);
	gf_free(ptr);
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
	desc = (char*)gf_malloc(sizeof(char) * descSize);
	gf_bs_read_data(bs, desc, descSize);
	e = gf_odf_desc_read(desc, descSize, &ptr->descriptor);
	//OK, free our desc
	gf_free(desc);
	return e;
}

GF_Box *iods_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ObjectDescriptorBox, GF_ISOM_BOX_TYPE_IODS);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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
	gf_free(desc);
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mdat_del(GF_Box *s)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	if (!s) return;

	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
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
	ISOM_DECL_BOX_ALLOC(GF_MediaDataBox, GF_ISOM_BOX_TYPE_MDAT);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mdhd_del(GF_Box *s)
{
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	if (!ptr->timeScale) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Media header timescale is invalid (0) - defaulting to 90000\n" ));
		ptr->timeScale = 90000;
	}

	ptr->original_duration = ptr->duration;

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
	ISOM_DECL_BOX_ALLOC(GF_MediaHeaderBox, GF_ISOM_BOX_TYPE_MDHD);

	gf_isom_full_box_init((GF_Box *) tmp);

	tmp->packedLanguage[0] = 'u';
	tmp->packedLanguage[1] = 'n';
	tmp->packedLanguage[2] = 'd';
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void mdia_del(GF_Box *s)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	if (ptr == NULL) return;
	if (ptr->mediaHeader) gf_isom_box_del((GF_Box *)ptr->mediaHeader);
	if (ptr->information) gf_isom_box_del((GF_Box *)ptr->information);
	if (ptr->handler) gf_isom_box_del((GF_Box *)ptr->handler);
	gf_free(ptr);
}


GF_Err mdia_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_MDHD:
		if (ptr->mediaHeader) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->mediaHeader = (GF_MediaHeaderBox *)a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_HDLR:
		if (ptr->handler) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->handler = (GF_HandlerBox *)a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_MINF:
		if (ptr->information) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->information = (GF_MediaInformationBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err mdia_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, mdia_AddBox);
}

GF_Box *mdia_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaBox, GF_ISOM_BOX_TYPE_MDIA);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mfra_del(GF_Box *s)
{
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->tfra_list);
	gf_free(ptr);
}

GF_Box *mfra_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentRandomAccessBox, GF_ISOM_BOX_TYPE_MFRA);
	tmp->tfra_list = gf_list_new();
	return (GF_Box *)tmp;
}

GF_Err mfra_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_TFRA:
		return gf_list_add(ptr->tfra_list, a);
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err mfra_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, mfra_AddBox);
}

void tfra_del(GF_Box *s)
{
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Box *tfra_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentRandomAccessBox, GF_ISOM_BOX_TYPE_TFRA);
	return (GF_Box *)tmp;
}

GF_Err tfra_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_RandomAccessEntry *p = 0;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->track_id = gf_bs_read_u32(bs);

	if (gf_bs_read_int(bs, 26) !=0) return GF_ISOM_INVALID_FILE;
	ptr->traf_bits = (gf_bs_read_int(bs, 2)+1)*8;
	ptr->trun_bits = (gf_bs_read_int(bs, 2)+1)*8;
	ptr->sample_bits = (gf_bs_read_int(bs, 2)+1)*8;
	ptr->nb_entries = gf_bs_read_u32(bs);

	if (ptr->nb_entries)
	{
		p = (GF_RandomAccessEntry *) gf_malloc(sizeof(GF_RandomAccessEntry) * ptr->nb_entries);
		if (!p) return GF_OUT_OF_MEM;
	}

	ptr->entries = p;

	for (i=0; i<ptr->nb_entries; i++) {
		memset(p, 0, sizeof(GF_RandomAccessEntry));

		if (ptr->version==1) {
			p->time = gf_bs_read_u64(bs);
			p->moof_offset = gf_bs_read_u64(bs);
		}
		else
		{
			p->time = gf_bs_read_u32(bs);
			p->moof_offset = gf_bs_read_u32(bs);
		}
		p->traf_number = gf_bs_read_int(bs, ptr->traf_bits);
		p->trun_number = gf_bs_read_int(bs, ptr->trun_bits);
		p->sample_number = gf_bs_read_int(bs, ptr->sample_bits);

		++p;
	}
	return GF_OK;
}

void elng_del(GF_Box *s)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;
	if (ptr == NULL) return;
	if (ptr->extended_language) gf_free(ptr->extended_language);
	gf_free(ptr);
}

GF_Err elng_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->size) {
		ptr->extended_language = (char*)gf_malloc((u32) ptr->size);
		if (ptr->extended_language == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->extended_language, (u32) ptr->size);
		/*safety check in case the string is not null-terminated*/
		if (ptr->extended_language[ptr->size-1]) {
			char *str = (char*)gf_malloc((u32) ptr->size + 1);
			memcpy(str, ptr->extended_language, (u32) ptr->size);
			str[ptr->size] = 0;
			gf_free(ptr->extended_language);
			ptr->extended_language = str;
		}
	}
	return GF_OK;
}

GF_Box *elng_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaBox, GF_ISOM_BOX_TYPE_ELNG);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err elng_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->extended_language) {
		gf_bs_write_data(bs, ptr->extended_language, (u32)(strlen(ptr->extended_language)+1));
	}
	return GF_OK;
}

GF_Err elng_Size(GF_Box *s)
{
	GF_Err e;
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if (ptr->extended_language) {
		ptr->size += strlen(ptr->extended_language)+1;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void mfhd_del(GF_Box *s)
{
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err mfhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->sequence_number = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *mfhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentHeaderBox, GF_ISOM_BOX_TYPE_MFHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


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



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


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
	gf_free(ptr);
}

GF_Err minf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_NMHD:
	case GF_ISOM_BOX_TYPE_STHD:
	case GF_ISOM_BOX_TYPE_VMHD:
	case GF_ISOM_BOX_TYPE_SMHD:
	case GF_ISOM_BOX_TYPE_HMHD:
	case GF_ISOM_BOX_TYPE_GMHD:
		if (ptr->InfoHeader) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->InfoHeader = a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_DINF:
		if (ptr->dataInformation) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->dataInformation = (GF_DataInformationBox *)a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_STBL:
		if (ptr->sampleTable ) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->sampleTable = (GF_SampleTableBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err minf_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, minf_AddBox);
}

GF_Box *minf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaInformationBox, GF_ISOM_BOX_TYPE_MINF);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	return GF_OK;
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
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void moof_del(GF_Box *s)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	if (ptr == NULL) return;

	if (ptr->mfhd) gf_isom_box_del((GF_Box *) ptr->mfhd);
	gf_isom_box_array_del(ptr->TrackList);
	if (ptr->mdat) gf_free(ptr->mdat);
	gf_free(ptr);
}

GF_Err moof_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MFHD:
		if (ptr->mfhd) ERROR_ON_DUPLICATED_BOX(a, ptr)

			ptr->mfhd = (GF_MovieFragmentHeaderBox *) a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRAF:
		return gf_list_add(ptr->TrackList, a);
	case GF_ISOM_BOX_TYPE_PSSH:
	default:
		return gf_isom_box_add_default(s, a);
	}
}

GF_Err moof_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, moof_AddBox);
}

GF_Box *moof_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentBox, GF_ISOM_BOX_TYPE_MOOF);
	tmp->TrackList = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

void moov_del(GF_Box *s)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	if (ptr == NULL) return;

	if (ptr->mvhd) gf_isom_box_del((GF_Box *)ptr->mvhd);
	if (ptr->meta) gf_isom_box_del((GF_Box *)ptr->meta);
	if (ptr->iods) gf_isom_box_del((GF_Box *)ptr->iods);
	if (ptr->udta) gf_isom_box_del((GF_Box *)ptr->udta);
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->mvex) gf_isom_box_del((GF_Box *)ptr->mvex);
#endif

	gf_isom_box_array_del(ptr->trackList);
	gf_free(ptr);
}


GF_Err moov_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	switch (a->type ) {
	case GF_ISOM_BOX_TYPE_IODS:
		if (ptr->iods) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->iods = (GF_ObjectDescriptorBox *)a;
		//if no IOD, delete the box
		if (!ptr->iods->descriptor) {
			ptr->iods = NULL;
			gf_isom_box_del(a);
		}
		return GF_OK;

	case GF_ISOM_BOX_TYPE_MVHD:
		if (ptr->mvhd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->mvhd = (GF_MovieHeaderBox *)a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_UDTA:
		if (ptr->udta) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->udta = (GF_UserDataBox *)a;
		return GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX:
		if (ptr->mvex) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->mvex = (GF_MovieExtendsBox *)a;
		ptr->mvex->mov = ptr->mov;
		return GF_OK;
#endif

	case GF_ISOM_BOX_TYPE_META:
		if (ptr->meta) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->meta = (GF_MetaBox *)a;
		return GF_OK;

	case GF_ISOM_BOX_TYPE_TRAK:
		//set our pointer to this obj
		((GF_TrackBox *)a)->moov = ptr;
		return gf_list_add(ptr->trackList, a);
	case GF_ISOM_BOX_TYPE_PSSH:
	default:
		return gf_isom_box_add_default(s, a);
	}
}


GF_Err moov_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, moov_AddBox);
}

GF_Box *moov_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieBox, GF_ISOM_BOX_TYPE_MOOV);
	tmp->trackList = gf_list_new();
	if (!tmp->trackList) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


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
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
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
	return GF_OK;
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
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->mvex) {
		e = gf_isom_box_size((GF_Box *) ptr->mvex);
		if (e) return e;
		ptr->size += ptr->mvex->size;
	}
#endif

	return gf_isom_box_array_size(s, ptr->trackList);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mp4a_del(GF_Box *s)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	gf_free(ptr);
}

GF_Err mp4a_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	case GF_4CC('w','a','v','e'):
		if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
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
				if (e) return e;
				ptr->esd = (GF_ESDBox *)a;
			}
			gf_isom_box_del(a);
		}
		break;
	default:
		return gf_isom_box_add_default(s, a);
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
	data = (char*)gf_malloc(sizeof(char) * size);
	gf_bs_read_data(bs, data, size);
	for (i=0; i<size-8; i++) {
		if (GF_4CC(data[i+4], data[i+5], data[i+6], data[i+7]) == GF_ISOM_BOX_TYPE_ESDS) {
			GF_BitStream *mybs = gf_bs_new(data + i, size - i, GF_BITSTREAM_READ);
			e = gf_isom_parse_box((GF_Box **)&ptr->esd, mybs);
			gf_bs_del(mybs);
			break;
		}
	}
	gf_free(data);
	return e;
}

GF_Box *mp4a_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGAudioSampleEntryBox, GF_ISOM_BOX_TYPE_MP4A);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

GF_Box *enca_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGAudioSampleEntryBox, GF_ISOM_BOX_TYPE_ENCA);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mp4a_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
	if (e) return e;

	return gf_isom_box_array_write(s, ptr->protections, bs);
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
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void mp4s_del(GF_Box *s)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	gf_free(ptr);
}

GF_Err mp4s_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	default:
		return gf_isom_box_add_default(s, a);
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
	ISOM_DECL_BOX_ALLOC(GF_MPEGSampleEntryBox, GF_ISOM_BOX_TYPE_MP4S);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

GF_Box *encs_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGSampleEntryBox, GF_ISOM_BOX_TYPE_ENCS);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	return gf_isom_box_array_write(s, ptr->protections, bs);
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
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mp4v_del(GF_Box *s)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->avc_config) gf_isom_box_del((GF_Box *) ptr->avc_config);
	if (ptr->svc_config) gf_isom_box_del((GF_Box *) ptr->svc_config);
	if (ptr->hevc_config) gf_isom_box_del((GF_Box *) ptr->hevc_config);
	if (ptr->lhvc_config) gf_isom_box_del((GF_Box *) ptr->lhvc_config);
	if (ptr->descr) gf_isom_box_del((GF_Box *) ptr->descr);
	if (ptr->ipod_ext) gf_isom_box_del((GF_Box *)ptr->ipod_ext);
	/*for publishing*/
	if (ptr->emul_esd) gf_odf_desc_del((GF_Descriptor *)ptr->emul_esd);

	if (ptr->pasp) gf_isom_box_del((GF_Box *)ptr->pasp);
	if (ptr->rvcc) gf_isom_box_del((GF_Box *)ptr->rvcc);

	gf_free(ptr);
}

GF_Err mp4v_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->esd = (GF_ESDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	case GF_ISOM_BOX_TYPE_AVCC:
		if (ptr->avc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->avc_config = (GF_AVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_HVCC:
		if (ptr->hevc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->hevc_config = (GF_HEVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SVCC:
		if (ptr->svc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->svc_config = (GF_AVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_LHVC:
		if (ptr->lhvc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->lhvc_config = (GF_HEVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_M4DS:
		if (ptr->descr) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->descr = (GF_MPEG4ExtensionDescriptorsBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_UUID:
		if (ptr->ipod_ext) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->ipod_ext = (GF_UnknownUUIDBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_PASP:
		if (ptr->pasp) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->pasp = (GF_PixelAspectRatioBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_RVCC:
		if (ptr->rvcc) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->rvcc = (GF_RVCConfigurationBox *)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
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
	if (mp4v->avc_config || mp4v->svc_config) AVC_RewriteESDescriptor(mp4v);
	/*this is an HEVC sample desc*/
	if (mp4v->hevc_config || mp4v->lhvc_config || (mp4v->type==GF_ISOM_BOX_TYPE_HVT1))
		HEVC_RewriteESDescriptor(mp4v);
	return GF_OK;
}

GF_Box *mp4v_encv_avc_hevc_new(u32 type)
{
	GF_MPEGVisualSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_MPEGVisualSampleEntryBox);
	if (tmp == NULL) return NULL;

	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	tmp->type = type;
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	/*avc or hevc*/
	else {
		if (ptr->avc_config && ptr->avc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->avc_config, bs);
			if (e) return e;
		}
		if (ptr->hevc_config && ptr->hevc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->hevc_config, bs);
			if (e) return e;
		}
		if (ptr->ipod_ext)	{
			e = gf_isom_box_write((GF_Box *) ptr->ipod_ext, bs);
			if (e) return e;
		}
		if (ptr->descr)	{
			e = gf_isom_box_write((GF_Box *) ptr->descr, bs);
			if (e) return e;
		}
		if (ptr->svc_config && ptr->svc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->svc_config, bs);
			if (e) return e;
		}
		if (ptr->lhvc_config && ptr->lhvc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->lhvc_config, bs);
			if (e) return e;
		}
	}
	if (ptr->rvcc) {
		e = gf_isom_box_write((GF_Box *)ptr->rvcc, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->protections, bs);
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
	} else {
		if (!ptr->avc_config && !ptr->svc_config && !ptr->hevc_config && !ptr->lhvc_config && (ptr->type!=GF_ISOM_BOX_TYPE_HVT1) ) {
			return GF_ISOM_INVALID_FILE;
		}

		if (ptr->hevc_config && ptr->hevc_config->config) {
			e = gf_isom_box_size((GF_Box *)ptr->hevc_config);
			if (e) return e;
			ptr->size += ptr->hevc_config->size;
		}

		if (ptr->avc_config && ptr->avc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->avc_config);
			if (e) return e;
			ptr->size += ptr->avc_config->size;
		}

		if (ptr->svc_config && ptr->svc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->svc_config);
			if (e) return e;
			ptr->size += ptr->svc_config->size;
		}

		if (ptr->lhvc_config && ptr->lhvc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->lhvc_config);
			if (e) return e;
			ptr->size += ptr->lhvc_config->size;
		}

		if (ptr->ipod_ext) {
			e = gf_isom_box_size((GF_Box *) ptr->ipod_ext);
			if (e) return e;
			ptr->size += ptr->ipod_ext->size;
		}
		if (ptr->descr) {
			e = gf_isom_box_size((GF_Box *) ptr->descr);
			if (e) return e;
			ptr->size += ptr->descr->size;
		}
	}
	if (ptr->pasp) {
		e = gf_isom_box_size((GF_Box *)ptr->pasp);
		if (e) return e;
		ptr->size += ptr->pasp->size;
	}
	if (ptr->rvcc) {
		e = gf_isom_box_size((GF_Box *)ptr->rvcc);
		if (e) return e;
		ptr->size += ptr->rvcc->size;
	}
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void mvex_del(GF_Box *s)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	if (ptr == NULL) return;
	if (ptr->mehd) gf_isom_box_del((GF_Box*)ptr->mehd);
	gf_isom_box_array_del(ptr->TrackExList);
	gf_isom_box_array_del(ptr->TrackExPropList);
	gf_free(ptr);
}


GF_Err mvex_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TREX:
		return gf_list_add(ptr->TrackExList, a);
	case GF_ISOM_BOX_TYPE_TREP:
		return gf_list_add(ptr->TrackExPropList, a);
	case GF_ISOM_BOX_TYPE_MEHD:
		if (ptr->mehd) break;
		ptr->mehd = (GF_MovieExtendsHeaderBox*)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}



GF_Err mvex_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, mvex_AddBox);
}

GF_Box *mvex_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieExtendsBox, GF_ISOM_BOX_TYPE_MVEX);
	tmp->TrackExList = gf_list_new();
	if (!tmp->TrackExList) {
		gf_free(tmp);
		return NULL;
	}
	tmp->TrackExPropList = gf_list_new();
	if (!tmp->TrackExPropList) {
		gf_list_del(tmp->TrackExList);
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err mvex_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (ptr->mehd) {
		e = gf_isom_box_write((GF_Box *)ptr->mehd, bs);
		if (e) return e;
	}
	e = gf_isom_box_array_write(s, ptr->TrackExList, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->TrackExPropList, bs);
}

GF_Err mvex_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (ptr->mehd) {
		e = gf_isom_box_size((GF_Box *)ptr->mehd);
		if (e) return e;
		ptr->size += ptr->mehd->size;
	}
	e = gf_isom_box_array_size(s, ptr->TrackExList);
	if (e) return e;
	return gf_isom_box_array_size(s, ptr->TrackExPropList);
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *mehd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieExtendsHeaderBox, GF_ISOM_BOX_TYPE_MEHD);
	return (GF_Box *)tmp;
}
void mehd_del(GF_Box *s)
{
	gf_free(s);
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
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void mvhd_del(GF_Box *s)
{
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	if (!ptr->timeScale) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Movie header timescale is invalid (0) - defaulting to 600\n" ));
		ptr->timeScale = 600;
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
	ptr->original_duration = ptr->duration;
	return GF_OK;
}

GF_Box *mvhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieHeaderBox, GF_ISOM_BOX_TYPE_MVHD);

	gf_isom_full_box_init((GF_Box *)tmp);

	tmp->preferredRate = (1<<16);
	tmp->preferredVolume = (1<<8);

	tmp->matrixA = (1<<16);
	tmp->matrixD = (1<<16);
	tmp->matrixW = (1<<30);

	tmp->nextTrackID = 1;

	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (ptr->duration==(u64) -1) ptr->version = 0;
	else ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += (ptr->version == 1) ? 28 : 16;
	ptr->size += 80;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void nmhd_del(GF_Box *s)
{
	GF_MPEGMediaHeaderBox *ptr = (GF_MPEGMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}



GF_Err nmhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Box *nmhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGMediaHeaderBox, GF_ISOM_BOX_TYPE_NMHD);
	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err nmhd_Write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err nmhd_Size(GF_Box *s)
{
	return gf_isom_full_box_get_size(s);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void padb_del(GF_Box *s)
{
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *) s;
	if (ptr == NULL) return;
	if (ptr->padbits) gf_free(ptr->padbits);
	gf_free(ptr);
}


GF_Err padb_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;

	e = gf_isom_full_box_read( s, bs);
	if (e) return e;

	ptr->SampleCount = gf_bs_read_u32(bs);

	ptr->padbits = (u8 *)gf_malloc(sizeof(u8)*ptr->SampleCount);
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
	ISOM_DECL_BOX_ALLOC(GF_PaddingBitsBox, GF_ISOM_BOX_TYPE_PADB);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void rely_del(GF_Box *s)
{
	GF_RelyHintBox *rely = (GF_RelyHintBox *)s;
	gf_free(rely);
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
	ISOM_DECL_BOX_ALLOC(GF_RelyHintBox, GF_ISOM_BOX_TYPE_RELY);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void rtpo_del(GF_Box *s)
{
	GF_RTPOBox *rtpo = (GF_RTPOBox *)s;
	gf_free(rtpo);
}

GF_Err rtpo_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_RTPOBox *ptr = (GF_RTPOBox *)s;
	ptr->timeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *rtpo_New()
{
	ISOM_DECL_BOX_ALLOC(GF_RTPOBox, GF_ISOM_BOX_TYPE_RTPO);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void smhd_del(GF_Box *s)
{
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	if (ptr == NULL ) return;
	gf_free(ptr);
}


GF_Err smhd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->balance = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *smhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SoundMediaHeaderBox, GF_ISOM_BOX_TYPE_SMHD);
	gf_isom_full_box_init((GF_Box *) tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err smhd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->balance);
	gf_bs_write_u16(bs, ptr->reserved);
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void snro_del(GF_Box *s)
{
	GF_SeqOffHintEntryBox *snro = (GF_SeqOffHintEntryBox *)s;
	gf_free(snro);
}

GF_Err snro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_SeqOffHintEntryBox *ptr = (GF_SeqOffHintEntryBox *)s;
	ptr->SeqOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *snro_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SeqOffHintEntryBox, GF_ISOM_BOX_TYPE_SNRO);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#define WRITE_SAMPLE_FRAGMENTS		1

void stbl_del(GF_Box *s)
{
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	if (ptr == NULL) return;

	if (ptr->ChunkOffset) gf_isom_box_del(ptr->ChunkOffset);
	if (ptr->CompositionOffset) gf_isom_box_del((GF_Box *) ptr->CompositionOffset);
	if (ptr->CompositionToDecode) gf_isom_box_del((GF_Box *) ptr->CompositionToDecode);
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
	if (ptr->SubSamples) gf_isom_box_del((GF_Box *) ptr->SubSamples);
	if (ptr->sampleGroups) gf_isom_box_array_del(ptr->sampleGroups);
	if (ptr->sampleGroupsDescription) gf_isom_box_array_del(ptr->sampleGroupsDescription);

	if (ptr->sai_sizes) gf_isom_box_array_del(ptr->sai_sizes);
	if (ptr->sai_offsets) gf_isom_box_array_del(ptr->sai_offsets);

	gf_free(ptr);
}

GF_Err stbl_AddBox(GF_SampleTableBox *ptr, GF_Box *a)
{
	if (!a) return GF_OK;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_STTS:
		if (ptr->TimeToSample) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->TimeToSample = (GF_TimeToSampleBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_CTTS:
		if (ptr->CompositionOffset) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->CompositionOffset = (GF_CompositionOffsetBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_CSLG:
		if (ptr->CompositionToDecode) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->CompositionToDecode = (GF_CompositionToDecodeBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSS:
		if (ptr->SyncSample) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SyncSample = (GF_SyncSampleBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSD:
		if (ptr->SampleDescription) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SampleDescription  =(GF_SampleDescriptionBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		if (ptr->SampleSize) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SampleSize = (GF_SampleSizeBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STSC:
		if (ptr->SampleToChunk) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SampleToChunk = (GF_SampleToChunkBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_PADB:
		if (ptr->PaddingBits) ERROR_ON_DUPLICATED_BOX(a, ptr)
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
		if (ptr->ShadowSync) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->ShadowSync = (GF_ShadowSyncBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_STDP:
		if (ptr->DegradationPriority) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->DegradationPriority = (GF_DegradationPriorityBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_SDTP:
		if (ptr->SampleDep) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SampleDep= (GF_SampleDependencyTypeBox *)a;
		break;

	case GF_ISOM_BOX_TYPE_STSF:
		if (ptr->Fragments) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->Fragments = (GF_SampleFragmentBox *)a;
		break;

	case GF_ISOM_BOX_TYPE_SUBS:
		if (ptr->SubSamples) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->SubSamples = (GF_SubSampleInformationBox *)a;
		break;

	case GF_ISOM_BOX_TYPE_SBGP:
		if (!ptr->sampleGroups) ptr->sampleGroups = gf_list_new();
		gf_list_add(ptr->sampleGroups, a);
		break;
	case GF_ISOM_BOX_TYPE_SGPD:
		if (!ptr->sampleGroupsDescription) ptr->sampleGroupsDescription = gf_list_new();
		gf_list_add(ptr->sampleGroupsDescription, a);
		break;

	case GF_ISOM_BOX_TYPE_SAIZ:
		if (!ptr->sai_sizes) ptr->sai_sizes = gf_list_new();
		gf_list_add(ptr->sai_sizes, a);
		break;
	case GF_ISOM_BOX_TYPE_SAIO:
		if (!ptr->sai_offsets) ptr->sai_offsets = gf_list_new();
		gf_list_add(ptr->sai_offsets, a);
		break;

	case GF_ISOM_BOX_TYPE_SENC:
		ptr->senc = a;
		return gf_isom_box_add_default((GF_Box *)ptr, a);
	case GF_ISOM_BOX_UUID_PSEC:
		ptr->piff_psec = a;
		return gf_isom_box_add_default((GF_Box *)ptr, a);
	case GF_ISOM_BOX_TYPE_UUID:
		if (((GF_UnknownUUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC) {
			ptr->piff_psec = a;
			return gf_isom_box_add_default((GF_Box *)ptr, a);
		}

	default:
		return gf_isom_box_add_default((GF_Box *)ptr, a);
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

			if (a->size>8) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" has %d extra bytes\n", gf_4cc_to_str(a->type), a->size));
				gf_bs_skip_bytes(bs, a->size-8);
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
	if (!ptr->SyncSample)
		ptr->no_sync_found = 1;

	ptr->nb_sgpd_in_stbl = gf_list_count(ptr->sampleGroupsDescription);
	ptr->nb_other_boxes_in_stbl = gf_list_count(ptr->other_boxes);

	return GF_OK;
}

GF_Box *stbl_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleTableBox, GF_ISOM_BOX_TYPE_STBL);
	//maxSamplePer chunk is 10 by default
	tmp->MaxSamplePerChunk = 10;
	tmp->groupID = 1;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (ptr->CompositionToDecode)	{
		e = gf_isom_box_write((GF_Box *) ptr->CompositionToDecode, bs);
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
	if (ptr->SubSamples) {
		e = gf_isom_box_write((GF_Box *) ptr->SubSamples, bs);
		if (e) return e;
	}
	if (ptr->sampleGroupsDescription) {
		e = gf_isom_box_array_write(s, ptr->sampleGroupsDescription, bs);
		if (e) return e;
	}
	if (ptr->sampleGroups) {
		e = gf_isom_box_array_write(s, ptr->sampleGroups, bs);
		if (e) return e;
	}
	if (ptr->sai_sizes) {
		e = gf_isom_box_array_write(s, ptr->sai_sizes, bs);
		if (e) return e;
	}
	if (ptr->sai_offsets) {
		e = gf_isom_box_array_write(s, ptr->sai_offsets, bs);
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
	if (ptr->CompositionToDecode)	{
		e = gf_isom_box_size((GF_Box *) ptr->CompositionToDecode);
		if (e) return e;
		ptr->size += ptr->CompositionToDecode->size;
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

	if (ptr->SubSamples) {
		e = gf_isom_box_size((GF_Box *) ptr->SubSamples);
		if (e) return e;
		ptr->size += ptr->SubSamples->size;
	}
	if (ptr->sampleGroups) {
		e = gf_isom_box_array_size(s, ptr->sampleGroups);
		if (e) return e;
	}
	if (ptr->sampleGroupsDescription) {
		e = gf_isom_box_array_size(s, ptr->sampleGroupsDescription);
		if (e) return e;
	}
	if (ptr->sai_sizes) {
		e = gf_isom_box_array_size(s, ptr->sai_sizes);
		if (e) return e;
	}
	if (ptr->sai_offsets) {
		e = gf_isom_box_array_size(s, ptr->sai_offsets);
		if (e) return e;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stco_del(GF_Box *s)
{
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	gf_free(ptr);
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
		ptr->offsets = (u32 *) gf_malloc(ptr->nb_entries * sizeof(u32) );
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
	ISOM_DECL_BOX_ALLOC(GF_ChunkOffsetBox, GF_ISOM_BOX_TYPE_STCO);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void stdp_del(GF_Box *s)
{
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	if (ptr == NULL ) return;
	if (ptr->priorities) gf_free(ptr->priorities);
	gf_free(ptr);
}

//this is called through stbl_read...
GF_Err stdp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 entry;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	/*out-of-order stdp, assume no padding at the end and take the entire remaining data for entries*/
	if (!ptr->nb_entries) ptr->nb_entries = (u32) ptr->size / 2;

	ptr->priorities = (u16 *) gf_malloc(ptr->nb_entries * sizeof(u16));
	if (ptr->priorities == NULL) return GF_OUT_OF_MEM;
	for (entry = 0; entry < ptr->nb_entries; entry++) {
		ptr->priorities[entry] = gf_bs_read_u16(bs);
	}
	ptr->size -= 2*ptr->nb_entries;
	return GF_OK;
}

GF_Box *stdp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DegradationPriorityBox, GF_ISOM_BOX_TYPE_STDP);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stdp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	for (i = 0; i < ptr->nb_entries; i++) {
		gf_bs_write_u16(bs, ptr->priorities[i]);
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stsc_del(GF_Box *s)
{
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
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
	ptr->entries = gf_malloc(sizeof(GF_StscEntry)*ptr->alloc_size);
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
	ISOM_DECL_BOX_ALLOC(GF_SampleToChunkBox, GF_ISOM_BOX_TYPE_STSC);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stsd_del(GF_Box *s)
{
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_HVT1:
	case GF_ISOM_BOX_TYPE_LHV1:
	case GF_ISOM_BOX_TYPE_LHE1:
	case GF_ISOM_BOX_TYPE_TX3G:
	case GF_ISOM_BOX_TYPE_TEXT:
	case GF_ISOM_BOX_TYPE_ENCT:
	case GF_ISOM_BOX_TYPE_METX:
	case GF_ISOM_BOX_TYPE_METT:
	case GF_ISOM_BOX_TYPE_STXT:
	case GF_ISOM_BOX_TYPE_DIMS:
	case GF_ISOM_BOX_TYPE_AC3:
	case GF_ISOM_BOX_TYPE_LSR1:
	case GF_ISOM_BOX_TYPE_WVTT:
	case GF_ISOM_BOX_TYPE_STPP:
	case GF_ISOM_BOX_TYPE_SBTT:
	case GF_ISOM_BOX_TYPE_ELNG:
	case GF_ISOM_BOX_TYPE_MP3:
		return gf_isom_box_add_default((GF_Box*)ptr, a);
	/*for 3GP config, we must set the type*/
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
	{
		((GF_3GPPAudioSampleEntryBox *)a)->info->cfg.type = a->type;
		return gf_isom_box_add_default((GF_Box*)ptr, a);
	}
	case GF_ISOM_SUBTYPE_3GP_H263:
	{
		((GF_3GPPVisualSampleEntryBox *)a)->info->cfg.type = a->type;
		return gf_isom_box_add_default((GF_Box*)ptr, a);
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
		return gf_isom_box_add_default((GF_Box*)ptr, a);
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
	ISOM_DECL_BOX_ALLOC(GF_SampleDescriptionBox, GF_ISOM_BOX_TYPE_STSD);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 nb_entries;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	nb_entries = gf_list_count(ptr->other_boxes);
	gf_bs_write_u32(bs, nb_entries);
	return GF_OK;
}

GF_Err stsd_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

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
			if (pe->fragmentSizes) gf_free(pe->fragmentSizes);
			gf_free(pe);
		}
		gf_list_del(ptr->entryList);
	}
	gf_free(ptr);
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
		p = (GF_StsfEntry *) gf_malloc(sizeof(GF_StsfEntry));
		if (!p) return GF_OUT_OF_MEM;
		p->SampleNumber = gf_bs_read_u32(bs);
		p->fragmentCount = gf_bs_read_u32(bs);
		p->fragmentSizes = (u16*)gf_malloc(sizeof(GF_StsfEntry) * p->fragmentCount);
		for (i=0; i<p->fragmentCount; i++) {
			p->fragmentSizes[i] = gf_bs_read_u16(bs);
		}
		gf_list_add(ptr->entryList, p);
	}
#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_currentEntry = p;
	ptr->w_currentEntryIndex = nb_entries-1;
#endif
	return GF_OK;
}

GF_Box *stsf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleFragmentBox, GF_ISOM_BOX_TYPE_STSF);

	gf_isom_full_box_init((GF_Box *) tmp);
	tmp->entryList = gf_list_new();
	if (! tmp->entryList) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *) tmp;
}




#ifndef GPAC_DISABLE_ISOM_WRITE

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
		for (j=0; j<p->fragmentCount; j++) {
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
	for (i=0; i<nb_entries; i++) {
		p = (GF_StsfEntry *)gf_list_get(ptr->entryList, i);
		ptr->size += 8 + 2*p->fragmentCount;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stsh_del(GF_Box *s)
{
	u32 i = 0;
	GF_StshEntry *ent;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;
	if (ptr == NULL) return;
	while ( (ent = (GF_StshEntry *)gf_list_enum(ptr->entries, &i)) ) {
		gf_free(ent);
	}
	gf_list_del(ptr->entries);
	gf_free(ptr);
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
		ent = (GF_StshEntry *) gf_malloc(sizeof(GF_StshEntry));
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
	ISOM_DECL_BOX_ALLOC(GF_ShadowSyncBox, GF_ISOM_BOX_TYPE_STSH);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->entries = gf_list_new();
	if (!tmp->entries) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void stss_del(GF_Box *s)
{
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
	if (ptr == NULL) return;
	if (ptr->sampleNumbers) gf_free(ptr->sampleNumbers);
	gf_free(ptr);
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
	ptr->sampleNumbers = (u32 *) gf_malloc( ptr->alloc_size * sizeof(u32));
	if (ptr->sampleNumbers == NULL) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->nb_entries; i++) {
		ptr->sampleNumbers[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *stss_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SyncSampleBox, GF_ISOM_BOX_TYPE_STSS);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box*)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stsz_del(GF_Box *s)
{
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return;
	if (ptr->sizes) gf_free(ptr->sizes);
	gf_free(ptr);
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
			ptr->sizes = (u32 *) gf_malloc(ptr->sampleCount * sizeof(u32));
			ptr->alloc_size = ptr->sampleCount;
			if (! ptr->sizes) return GF_OUT_OF_MEM;
			for (i = 0; i < ptr->sampleCount; i++) {
				ptr->sizes[i] = gf_bs_read_u32(bs);
			}
		}
	} else {
		//note we could optimize the mem usage by keeping the table compact
		//in memory. But that would complicate both caching and editing
		//we therefore keep all sizes as u32 and uncompress the table
		ptr->sizes = (u32 *) gf_malloc(ptr->sampleCount * sizeof(u32));
		if (! ptr->sizes) return GF_OUT_OF_MEM;
		ptr->alloc_size = ptr->sampleCount;

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
	ISOM_DECL_BOX_ALLOC(GF_SampleSizeBox, 0);

	gf_isom_full_box_init((GF_Box *)tmp);
	//type is unknown here, can be regular or compact table
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
				gf_bs_write_u32(bs, ptr->sizes ? ptr->sizes[i] : 0);
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
		gf_free(ptr->sizes);
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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stts_del(GF_Box *s)
{
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}


GF_Err stts_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_LastDTS = 0;
#endif
	ptr->nb_entries = gf_bs_read_u32(bs);
	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = gf_malloc(sizeof(GF_SttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		ptr->entries[i].sampleDelta = gf_bs_read_u32(bs);
#ifndef GPAC_DISABLE_ISOM_WRITE
		ptr->w_currentSampleNum += ptr->entries[i].sampleCount;
		ptr->w_LastDTS += ptr->entries[i].sampleCount * ptr->entries[i].sampleDelta;
#endif

		if (!ptr->entries[i].sampleDelta) {
			if ((i+1<ptr->nb_entries) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Found stss entry with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			} else if (ptr->entries[i].sampleCount>1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] more than one sample at the end of the track with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			}
		} else if ((s32) ptr->entries[i].sampleDelta < 0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] stts entry %d has negative duration %d - forbidden ! Fixing to 1, sync may get lost (consider reimport raw media)\n", i, (s32) ptr->entries[i].sampleDelta ));
			ptr->entries[i].sampleDelta = 1;
		}
	}
	//remove the last sample delta.
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (ptr->nb_entries) ptr->w_LastDTS -= ptr->entries[ptr->nb_entries-1].sampleDelta;
#endif
	return GF_OK;
}

GF_Box *stts_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeToSampleBox, GF_ISOM_BOX_TYPE_STTS);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stts_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries; i++) {
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


#endif /*GPAC_DISABLE_ISOM_WRITE*/


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void tfhd_del(GF_Box *s)
{
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentHeaderBox, GF_ISOM_BOX_TYPE_TFHD);
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


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

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void tims_del(GF_Box *s)
{
	GF_TSHintEntryBox *tims = (GF_TSHintEntryBox *)s;
	gf_free(tims);
}

GF_Err tims_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TSHintEntryBox *ptr = (GF_TSHintEntryBox *)s;
	ptr->timeScale = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tims_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TSHintEntryBox, GF_ISOM_BOX_TYPE_TIMS);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tkhd_del(GF_Box *s)
{
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	ISOM_DECL_BOX_ALLOC(GF_TrackHeaderBox, GF_ISOM_BOX_TYPE_TKHD);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->matrix[0] = 0x00010000;
	tmp->matrix[4] = 0x00010000;
	tmp->matrix[8] = 0x40000000;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (ptr->duration==(u64) -1) ptr->version = 0;
	else ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;
	ptr->size += (ptr->version == 1) ? 32 : 20;
	ptr->size += 60;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void traf_del(GF_Box *s)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	if (ptr == NULL) return;
	if (ptr->tfhd) gf_isom_box_del((GF_Box *) ptr->tfhd);
	if (ptr->sdtp) gf_isom_box_del((GF_Box *) ptr->sdtp);
	if (ptr->subs) gf_isom_box_del((GF_Box *) ptr->subs);
	if (ptr->tfdt) gf_isom_box_del((GF_Box *) ptr->tfdt);
	if (ptr->piff_sample_encryption) gf_isom_box_del((GF_Box *) ptr->piff_sample_encryption);
	if (ptr->sample_encryption) gf_isom_box_del((GF_Box *) ptr->sample_encryption);
	gf_isom_box_array_del(ptr->TrackRuns);
	if (ptr->sampleGroups) gf_isom_box_array_del(ptr->sampleGroups);
	if (ptr->sampleGroupsDescription) gf_isom_box_array_del(ptr->sampleGroupsDescription);
	if (ptr->sai_sizes) gf_isom_box_array_del(ptr->sai_sizes);
	if (ptr->sai_offsets) gf_isom_box_array_del(ptr->sai_offsets);
	gf_free(ptr);
}

GF_Err traf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TFHD:
		if (ptr->tfhd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->tfhd = (GF_TrackFragmentHeaderBox *) a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRUN:
		return gf_list_add(ptr->TrackRuns, a);
	case GF_ISOM_BOX_TYPE_SDTP:
		if (ptr->sdtp) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->sdtp = (GF_SampleDependencyTypeBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TFDT:
		if (ptr->tfdt) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->tfdt = (GF_TFBaseMediaDecodeTimeBox*) a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SUBS:
		if (ptr->subs) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->subs = (GF_SubSampleInformationBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SBGP:
		if (!ptr->sampleGroups) ptr->sampleGroups = gf_list_new();
		gf_list_add(ptr->sampleGroups, a);
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SGPD:
		if (!ptr->sampleGroupsDescription) ptr->sampleGroupsDescription = gf_list_new();
		gf_list_add(ptr->sampleGroupsDescription, a);
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SAIZ:
		if (!ptr->sai_sizes) ptr->sai_sizes = gf_list_new();
		gf_list_add(ptr->sai_sizes, a);
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SAIO:
		if (!ptr->sai_offsets) ptr->sai_offsets = gf_list_new();
		gf_list_add(ptr->sai_offsets, a);
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UUID:
		if ( ((GF_UUIDBox *)a)->internal_4cc==GF_ISOM_BOX_UUID_PSEC) {
			if (ptr->piff_sample_encryption) ERROR_ON_DUPLICATED_BOX(a, ptr)
				ptr->piff_sample_encryption = (GF_PIFFSampleEncryptionBox *)a;
			ptr->piff_sample_encryption->traf = ptr;
			return GF_OK;
		} else {
			return gf_isom_box_add_default(s, a);
		}
	case GF_ISOM_BOX_TYPE_SENC:
		if (ptr->sample_encryption) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->sample_encryption = (GF_SampleEncryptionBox *)a;
		ptr->sample_encryption->traf = ptr;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err traf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Box *a;

	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;

	while (ptr->size) {
		GF_Err e = gf_isom_parse_box(&a, bs);
		if (e) return e;


		//we need to read the DegPriority in a different way...
		if ((a->type == GF_ISOM_BOX_TYPE_STDP) || (a->type == GF_ISOM_BOX_TYPE_SDTP)) {
			u32 nb_samples=0, i=0;
			u64 s = a->size;
			for (i=0; i<gf_list_count(ptr->TrackRuns); i++) {
				GF_TrackFragmentRunBox *trun = gf_list_get(ptr->TrackRuns, i);
				nb_samples+=trun->sample_count;
			}
			if (a->type == GF_ISOM_BOX_TYPE_STDP) {
				if (nb_samples) ((GF_DegradationPriorityBox *)a)->nb_entries = nb_samples;
				e = stdp_Read(a, bs);
			} else {
				if (nb_samples) ((GF_SampleDependencyTypeBox *)a)->sampleCount = nb_samples;
				e = sdtp_Read(a, bs);
			}
			if (e) {
				gf_isom_box_del(a);
				return e;
			}
			a->size = s;
		}
		if (ptr->size<a->size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Box \"%s\" is larger than container box\n", gf_4cc_to_str(a->type)));
			ptr->size = 0;
		} else {
			ptr->size -= a->size;
		}
		e = traf_AddBox((GF_Box*)ptr, a);
		if (e) return e;
	}
	return GF_OK;
}

GF_Box *traf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentBox, GF_ISOM_BOX_TYPE_TRAF);
	tmp->TrackRuns = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (ptr->subs) {
		e = gf_isom_box_write((GF_Box *) ptr->subs, bs);
		if (e) return e;
	}
	if (ptr->tfdt) {
		e = gf_isom_box_write((GF_Box *) ptr->tfdt, bs);
		if (e) return e;
	}
	if (ptr->sdtp) {
		e = gf_isom_box_write((GF_Box *) ptr->sdtp, bs);
		if (e) return e;
	}
	if (ptr->sampleGroupsDescription) {
		e = gf_isom_box_array_write(s, ptr->sampleGroupsDescription, bs);
		if (e) return e;
	}
	if (ptr->sampleGroups) {
		e = gf_isom_box_array_write(s, ptr->sampleGroups, bs);
		if (e) return e;
	}
	if (ptr->sai_sizes) {
		e = gf_isom_box_array_write(s, ptr->sai_sizes, bs);
		if (e) return e;
	}
	if (ptr->sai_offsets) {
		e = gf_isom_box_array_write(s, ptr->sai_offsets, bs);
		if (e) return e;
	}
	e = gf_isom_box_array_write(s, ptr->TrackRuns, bs);
	if (e) return e;

	if (ptr->piff_sample_encryption) {
		e = gf_isom_box_write((GF_Box *) ptr->piff_sample_encryption, bs);
		if (e) return e;
	}

	if (ptr->sample_encryption) {
		e = gf_isom_box_write((GF_Box *) ptr->sample_encryption, bs);
		if (e) return e;
	}
	return GF_OK;
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
	if (ptr->piff_sample_encryption) {
		e = gf_isom_box_size((GF_Box *) ptr->piff_sample_encryption);
		if (e) return e;
		ptr->size += ptr->piff_sample_encryption->size;
	}
	if (ptr->subs) {
		e = gf_isom_box_size((GF_Box *) ptr->subs);
		if (e) return e;
		ptr->size += ptr->subs->size;
	}
	if (ptr->sdtp) {
		e = gf_isom_box_size((GF_Box *) ptr->sdtp);
		if (e) return e;
		ptr->size += ptr->sdtp->size;
	}
	if (ptr->tfdt) {
		e = gf_isom_box_size((GF_Box *) ptr->tfdt);
		if (e) return e;
		ptr->size += ptr->tfdt->size;
	}

	if (ptr->sampleGroups) {
		e = gf_isom_box_array_size(s, ptr->sampleGroups);
		if (e) return e;
	}
	if (ptr->sampleGroupsDescription) {
		e = gf_isom_box_array_size(s, ptr->sampleGroupsDescription);
		if (e) return e;
	}
	if (ptr->sai_sizes) {
		e = gf_isom_box_array_size(s, ptr->sai_sizes);
		if (e) return e;
	}
	if (ptr->sai_offsets) {
		e = gf_isom_box_array_size(s, ptr->sai_offsets);
		if (e) return e;
	}
	if (ptr->sample_encryption) {
		e = gf_isom_box_size((GF_Box *) ptr->sample_encryption);
		if (e) return e;
		ptr->size += ptr->sample_encryption->size;
	}
	return gf_isom_box_array_size(s, ptr->TrackRuns);
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


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
	if (ptr->name) gf_free(ptr->name);
	gf_free(ptr);
}

static void gf_isom_check_sample_desc(GF_TrackBox *trak)
{
	GF_BitStream *bs;
	GF_UnknownBox *a;
	u32 i;

	if (!trak->Media || !trak->Media->information) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no media box !\n" ));
		return;
	}
	if (!trak->Media->information->sampleTable) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no sample table !\n" ));
		trak->Media->information->sampleTable = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	}

	if (!trak->Media->information->sampleTable->SampleDescription) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no sample description box !\n" ));
		trak->Media->information->sampleTable->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSD);
		return;
	}

	i=0;
	while ((a = (GF_UnknownBox*)gf_list_enum(trak->Media->information->sampleTable->SampleDescription->other_boxes, &i))) {
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
		case GF_ISOM_BOX_TYPE_METX:
		case GF_ISOM_BOX_TYPE_METT:
		case GF_ISOM_BOX_TYPE_STXT:
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_AVC2:
		case GF_ISOM_BOX_TYPE_AVC3:
		case GF_ISOM_BOX_TYPE_AVC4:
		case GF_ISOM_BOX_TYPE_SVC1:
		case GF_ISOM_BOX_TYPE_HVC1:
		case GF_ISOM_BOX_TYPE_HEV1:
		case GF_ISOM_BOX_TYPE_HVC2:
		case GF_ISOM_BOX_TYPE_HEV2:
		case GF_ISOM_BOX_TYPE_HVT1:
		case GF_ISOM_BOX_TYPE_LHV1:
		case GF_ISOM_BOX_TYPE_LHE1:
		case GF_ISOM_BOX_TYPE_TX3G:
		case GF_ISOM_BOX_TYPE_TEXT:
		case GF_ISOM_BOX_TYPE_ENCT:
		case GF_ISOM_BOX_TYPE_DIMS:
		case GF_ISOM_BOX_TYPE_AC3:
		case GF_ISOM_BOX_TYPE_LSR1:
		case GF_ISOM_BOX_TYPE_WVTT:
		case GF_ISOM_BOX_TYPE_STPP:
		case GF_ISOM_BOX_TYPE_SBTT:
			continue;
		default:
			break;
		}
		if (!a->data || (a->dataSize<8) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Sample description %s does not have at least 8 bytes!\n", gf_4cc_to_str(a->type) ));
			continue;
		}

		/*only process visual or audio*/
		switch (trak->Media->handler->handlerType) {
		case GF_ISOM_MEDIA_VISUAL:
		{
			GF_GenericVisualSampleEntryBox *genv;
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->other_boxes, i-1);
			genv = (GF_GenericVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRV);
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			genv->size = a->size-8;
			gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *) genv, bs);

			if (gf_bs_available(bs)) {
				u64 pos = gf_bs_get_position(bs);
				//try to parse as boxes
				GF_Err e = gf_isom_read_box_list((GF_Box *) genv, bs, mp4v_AddBox);
				if (e) {
					gf_bs_seek(bs, pos);
					genv->data_size = (u32) gf_bs_available(bs);
					if (genv->data_size) {
						genv->data = a->data;
						a->data = NULL;
						memmove(genv->data, genv->data + pos, genv->data_size);
					}
				} else {
					genv->data_size = 0;
				}
			}
			gf_bs_del(bs);
			if (!genv->data_size && genv->data) {
				gf_free(genv->data);
				genv->data = NULL;
			}

			genv->size = 0;
			genv->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->other_boxes, genv, i-1);
		}
		break;
		case GF_ISOM_MEDIA_AUDIO:
		{
			GF_GenericAudioSampleEntryBox *gena;
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->other_boxes, i-1);
			gena = (GF_GenericAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRA);
			gena->size = a->size-8;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox *) gena, bs);

			if (gf_bs_available(bs)) {
				u64 pos = gf_bs_get_position(bs);
				//try to parse as boxes
				GF_Err e = gf_isom_read_box_list((GF_Box *) gena, bs, mp4a_AddBox);
				if (e) {
					gf_bs_seek(bs, pos);
					gena->data_size = (u32) gf_bs_available(bs);
					if (gena->data_size) {
						gena->data = a->data;
						a->data = NULL;
						memmove(gena->data, gena->data + pos, gena->data_size);
					}
				} else {
					gena->data_size = 0;
				}
			}
			gf_bs_del(bs);
			if (!gena->data_size && gena->data) {
				gf_free(gena->data);
				gena->data = NULL;
			}
			gena->size = 0;
			gena->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->other_boxes, gena, i-1);
		}
		break;

		default:
		{
			GF_GenericSampleEntryBox *genm;
			/*remove entry*/
			gf_list_rem(trak->Media->information->sampleTable->SampleDescription->other_boxes, i-1);
			genm = (GF_GenericSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
			genm->size = a->size-8;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			gf_bs_read_data(bs, genm->reserved, 6);
			genm->dataReferenceIndex = gf_bs_read_u16(bs);
			genm->size -= 8;

			if (gf_bs_available(bs)) {
				u64 pos = gf_bs_get_position(bs);
				//try to parse as boxes
				GF_Err e = gf_isom_read_box_list((GF_Box *) genm, bs, mp4s_AddBox);
				if (e) {
					gf_bs_seek(bs, pos);
					genm->data_size = (u32) gf_bs_available(bs);
					if (genm->data_size) {
						genm->data = a->data;
						a->data = NULL;
						memmove(genm->data, genm->data + pos, genm->data_size);
					}
				} else {
					genm->data_size = 0;
				}
			}
			gf_bs_del(bs);
			if (!genm->data_size && genm->data) {
				gf_free(genm->data);
				genm->data = NULL;
			}
			genm->size = 0;

			genm->EntryType = a->type;
			gf_isom_box_del((GF_Box *)a);
			gf_list_insert(trak->Media->information->sampleTable->SampleDescription->other_boxes, genm, i-1);
		}
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
		if (ptr->Header) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->Header = (GF_TrackHeaderBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_EDTS:
		if (ptr->editBox) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->editBox = (GF_EditBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UDTA:
		if (ptr->udta) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->udta = (GF_UserDataBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_META:
		if (ptr->meta) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->meta = (GF_MetaBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TREF:
		if (ptr->References) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->References = (GF_TrackReferenceBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_MDIA:
		if (ptr->Media) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->Media = (GF_MediaBox *)a;
		((GF_MediaBox *)a)->mediaTrack = ptr;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
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

	//we should only parse senc/psec when no saiz/saio is present, otherwise we fetch the info directly
	if (ptr->Media && ptr->Media->information && ptr->Media->information->sampleTable /*&& !ptr->Media->information->sampleTable->sai_sizes*/) {
		if (ptr->Media->information->sampleTable->senc) {
			e = senc_Parse(bs, ptr, NULL, (GF_SampleEncryptionBox *)ptr->Media->information->sampleTable->senc);
		}
		else if (ptr->Media->information->sampleTable->piff_psec) {
			e = senc_Parse(bs, ptr, NULL, (GF_SampleEncryptionBox *) ptr->Media->information->sampleTable->piff_psec);
		}
	}
	return e;
}

GF_Box *trak_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackBox, GF_ISOM_BOX_TYPE_TRAK);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
	return GF_OK;
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
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Err tref_AddBox(GF_Box *ptr, GF_Box *a)
{
	return gf_isom_box_add_default(ptr, a);
}

void tref_del(GF_Box *s)
{
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err tref_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list_ex(s, bs, gf_isom_box_add_default, s->type);
}

GF_Box *tref_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackReferenceBox, GF_ISOM_BOX_TYPE_TREF);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tref_Write(GF_Box *s, GF_BitStream *bs)
{
//	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	return gf_isom_box_write_header(s, bs);
}

GF_Err tref_Size(GF_Box *s)
{
//	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	return gf_isom_box_get_size(s);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void reftype_del(GF_Box *s)
{
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	if (!ptr) return;
	if (ptr->trackIDs) gf_free(ptr->trackIDs);
	gf_free(ptr);
}


GF_Err reftype_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	u32 i;
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;

	bytesToRead = (u32) (ptr->size);
	if (!bytesToRead) return GF_OK;

	ptr->trackIDCount = (u32) (bytesToRead) / sizeof(u32);
	ptr->trackIDs = (u32 *) gf_malloc(ptr->trackIDCount * sizeof(u32));
	if (!ptr->trackIDs) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->trackIDCount; i++) {
		ptr->trackIDs[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *reftype_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackReferenceTypeBox, GF_ISOM_BOX_TYPE_REFT);
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

	ref->trackIDs = (u32 *) gf_realloc(ref->trackIDs, (ref->trackIDCount + 1) * sizeof(u32) );
	if (!ref->trackIDs) return GF_OUT_OF_MEM;
	ref->trackIDs[ref->trackIDCount] = trackID;
	ref->trackIDCount++;
	if (outRefIndex) *outRefIndex = ref->trackIDCount;
	return GF_OK;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void trex_del(GF_Box *s)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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

	if (!ptr->def_sample_desc_index) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] TREX with default sample description set to 0, likely broken ! Fixing to 1\n" ));
		ptr->def_sample_desc_index = 1;
	}
	return GF_OK;
}

GF_Box *trex_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackExtendsBox, GF_ISOM_BOX_TYPE_TREX);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


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



#endif /*GPAC_DISABLE_ISOM_WRITE*/



void trep_del(GF_Box *s)
{
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err trep_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->trackID = gf_bs_read_u32(bs);
	ptr->size-=4;

	return gf_isom_read_box_list(s, bs, gf_isom_box_add_default);
}

GF_Box *trep_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackExtensionPropertiesBox, GF_ISOM_BOX_TYPE_TREP);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err trep_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->trackID);
	return GF_OK;
}

GF_Err trep_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void trun_del(GF_Box *s)
{
	GF_TrunEntry *p;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;
	if (ptr == NULL) return;

	while (gf_list_count(ptr->entries)) {
		p = (GF_TrunEntry*)gf_list_get(ptr->entries, 0);
		gf_list_rem(ptr->entries, 0);
		gf_free(p);
	}
	gf_list_del(ptr->entries);
	if (ptr->cache) gf_bs_del(ptr->cache);
	gf_free(ptr);
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
		p = (GF_TrunEntry *) gf_malloc(sizeof(GF_TrunEntry));
		if (!p) return GF_OUT_OF_MEM;
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
			if (ptr->version==0) {
				p->CTS_Offset = (u32) gf_bs_read_u32(bs);
			} else {
				p->CTS_Offset = (s32) gf_bs_read_u32(bs);
			}
		}
		gf_list_add(ptr->entries, p);
		if (ptr->size<trun_size) return GF_ISOM_INVALID_FILE;
		ptr->size-=trun_size;
	}
	return GF_OK;
}

GF_Box *trun_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentRunBox, GF_ISOM_BOX_TYPE_TRUN);
	tmp->entries = gf_list_new();
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


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
			if (ptr->version==0) {
				gf_bs_write_u32(bs, p->CTS_Offset);
			} else {
				gf_bs_write_u32(bs, (u32) p->CTS_Offset);
			}
		}
	}
	return GF_OK;
}

GF_Err trun_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, count;
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
		if (ptr->flags & GF_ISOM_TRUN_DURATION) ptr->size += 4;
		if (ptr->flags & GF_ISOM_TRUN_SIZE) ptr->size += 4;
		//SHOULDN'T BE USED IF GF_ISOM_TRUN_FIRST_FLAG IS DEFINED
		if (ptr->flags & GF_ISOM_TRUN_FLAGS) ptr->size += 4;
		if (ptr->flags & GF_ISOM_TRUN_CTS_OFFSET) ptr->size += 4;
	}

	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void tsro_del(GF_Box *s)
{
	GF_TimeOffHintEntryBox *tsro = (GF_TimeOffHintEntryBox *)s;
	gf_free(tsro);
}

GF_Err tsro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeOffHintEntryBox *ptr = (GF_TimeOffHintEntryBox *)s;
	ptr->TimeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tsro_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeOffHintEntryBox, GF_ISOM_BOX_TYPE_TSRO);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
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
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void udta_del(GF_Box *s)
{
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	if (ptr == NULL) return;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		gf_isom_box_array_del(map->other_boxes);
		gf_free(map);
	}
	gf_list_del(ptr->recordList);
	gf_free(ptr);
}

GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 box_type, bin128 *uuid)
{
	u32 i;
	GF_UserDataMap *map;
	if (ptr == NULL) return NULL;
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
		map = (GF_UserDataMap *) gf_malloc(sizeof(GF_UserDataMap));
		if (map == NULL) return GF_OUT_OF_MEM;
		memset(map, 0, sizeof(GF_UserDataMap));

		map->boxType = a->type;
		if (a->type == GF_ISOM_BOX_TYPE_UUID)
			memcpy(map->uuid, ((GF_UUIDBox *)a)->uuid, 16);
		map->other_boxes = gf_list_new();
		if (!map->other_boxes) {
			gf_free(map);
			return GF_OUT_OF_MEM;
		}
		e = gf_list_add(ptr->recordList, map);
		if (e) return e;
	}
	return gf_list_add(map->other_boxes, a);
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
	ISOM_DECL_BOX_ALLOC(GF_UserDataBox, GF_ISOM_BOX_TYPE_UDTA);
	tmp->recordList = gf_list_new();
	if (!tmp->recordList) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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
		e = gf_isom_box_array_write(s, map->other_boxes, bs);
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
		e = gf_isom_box_array_size(s, map->other_boxes);
		if (e) return e;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void vmhd_del(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
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
	ISOM_DECL_BOX_ALLOC(GF_VideoMediaHeaderBox, GF_ISOM_BOX_TYPE_VMHD);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void void_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err void_Read(GF_Box *s, GF_BitStream *bs)
{
	if (s->size) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

GF_Box *void_New()
{
	ISOM_DECL_BOX_ALLOC(GF_Box, GF_ISOM_BOX_TYPE_VOID);
	return tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *pdin_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ProgressiveDownloadBox, GF_ISOM_BOX_TYPE_PDIN);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	return (GF_Box *)tmp;
}


void pdin_del(GF_Box *s)
{
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;
	if (ptr == NULL) return;
	if (ptr->rates) gf_free(ptr->rates);
	if (ptr->times) gf_free(ptr->times);
	gf_free(ptr);
}


GF_Err pdin_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->count = (u32) (ptr->size) / 8;
	ptr->rates = (u32*)gf_malloc(sizeof(u32)*ptr->count);
	ptr->times = (u32*)gf_malloc(sizeof(u32)*ptr->count);
	for (i=0; i<ptr->count; i++) {
		ptr->rates[i] = gf_bs_read_u32(bs);
		ptr->times[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *sdtp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleDependencyTypeBox, GF_ISOM_BOX_TYPE_SDTP);
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->flags = 1;
	return (GF_Box *)tmp;
}


void sdtp_del(GF_Box *s)
{
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;
	if (ptr == NULL) return;
	if (ptr->sample_info) gf_free(ptr->sample_info);
	gf_free(ptr);
}


GF_Err sdtp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	/*out-of-order sdtp, assume no padding at the end*/
	if (!ptr->sampleCount) ptr->sampleCount = (u32) (ptr->size - 8);
	ptr->sample_info = (u8 *) gf_malloc(sizeof(u8)*ptr->sampleCount);
	gf_bs_read_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	ptr->size -= ptr->sampleCount;
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *pasp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PixelAspectRatioBox, GF_ISOM_BOX_TYPE_PASP);
	return (GF_Box *)tmp;
}


void pasp_del(GF_Box *s)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err pasp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	ptr->hSpacing = gf_bs_read_u32(bs);
	ptr->vSpacing = gf_bs_read_u32(bs);
	ptr->size -= 8;
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

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

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *metx_New(u32 type)
{
	ISOM_DECL_BOX_ALLOC(GF_MetaDataSampleEntryBox, type);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


void metx_del(GF_Box *s)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->content_encoding) gf_free(ptr->content_encoding);
	if (ptr->xml_namespace) gf_free(ptr->xml_namespace);
	if (ptr->xml_schema_loc) gf_free(ptr->xml_schema_loc);
	if (ptr->mime_type) gf_free(ptr->mime_type);
	if (ptr->config) gf_isom_box_del((GF_Box *)ptr->config);
	gf_free(ptr);
}


GF_Err metx_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	case GF_ISOM_BOX_TYPE_TXTC:
		//we allow the config box on metx
		if (ptr->config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->config = (GF_TextConfigBox *)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err metx_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 size, i;
	char *str;
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;

	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);

	size = (u32) ptr->size - 8;
	str = gf_malloc(sizeof(char)*size);

	i=0;

	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) {
		if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
			ptr->xml_namespace = gf_strdup(str);
		} else {
			ptr->content_encoding = gf_strdup(str);
		}
	}

	i=0;
	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if ((ptr->type==GF_ISOM_BOX_TYPE_METX) || (ptr->type==GF_ISOM_BOX_TYPE_STPP)) {
		if (i) {
			if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
				ptr->xml_schema_loc = gf_strdup(str);
			} else {
				ptr->xml_namespace = gf_strdup(str);
			}
		}

		i=0;
		while (size) {
			str[i] = gf_bs_read_u8(bs);
			size--;
			if (!str[i])
				break;
			i++;
		}
		if (i) {
			if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
				ptr->mime_type = gf_strdup(str);
			} else {
				ptr->xml_schema_loc = gf_strdup(str);
			}
		}
	}
	//mett, sbtt, stxt, stpp
	else {
		if (i) ptr->mime_type = gf_strdup(str);
	}
	ptr->size = size;
	gf_free(str);
	return gf_isom_read_box_list(s, bs, metx_AddBox);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err metx_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);

	if (ptr->type!=GF_ISOM_BOX_TYPE_STPP) {
		if (ptr->content_encoding)
			gf_bs_write_data(bs, ptr->content_encoding, (u32) strlen(ptr->content_encoding));
		gf_bs_write_u8(bs, 0);
	}

	if ((ptr->type==GF_ISOM_BOX_TYPE_METX) || (ptr->type==GF_ISOM_BOX_TYPE_STPP)) {
		if (ptr->xml_namespace)
			gf_bs_write_data(bs, ptr->xml_namespace, (u32) strlen(ptr->xml_namespace));

		gf_bs_write_u8(bs, 0);

		if (ptr->xml_schema_loc)
			gf_bs_write_data(bs, ptr->xml_schema_loc, (u32) strlen(ptr->xml_schema_loc));
		gf_bs_write_u8(bs, 0);

		if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
			if (ptr->mime_type)
				gf_bs_write_data(bs, ptr->mime_type, (u32) strlen(ptr->mime_type));

			gf_bs_write_u8(bs, 0);
		}
	}
	//mett, sbtt, stxt
	else {
		if (ptr->mime_type)
			gf_bs_write_data(bs, ptr->mime_type, (u32) strlen(ptr->mime_type));

		gf_bs_write_u8(bs, 0);

		if (ptr->config) {
			gf_isom_box_write((GF_Box *)ptr->config, bs);
		}
	}

	return gf_isom_box_array_write(s, ptr->protections, bs);
}

GF_Err metx_Size(GF_Box *s)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	GF_Err e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 8;

	if (ptr->type!=GF_ISOM_BOX_TYPE_STPP) {
		if (ptr->content_encoding)
			ptr->size += strlen(ptr->content_encoding);
		ptr->size++;
	}

	if ((ptr->type==GF_ISOM_BOX_TYPE_METX) || (ptr->type==GF_ISOM_BOX_TYPE_STPP)) {

		if (ptr->xml_namespace)
			ptr->size += strlen(ptr->xml_namespace);
		ptr->size++;

		if (ptr->xml_schema_loc)
			ptr->size += strlen(ptr->xml_schema_loc);
		ptr->size++;

		if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
			if (ptr->mime_type)
				ptr->size += strlen(ptr->mime_type);
			ptr->size++;
		}

	}
	//mett, sbtt, stxt
	else {
		if (ptr->mime_type)
			ptr->size += strlen(ptr->mime_type);
		ptr->size++;

		if (ptr->config) {
			e = gf_isom_box_size((GF_Box *)ptr->config);
			if (e) return e;
			ptr->size += ptr->config->size;
		}
	}

	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* SimpleTextSampleEntry */
GF_Box *txtc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TextConfigBox, GF_ISOM_BOX_TYPE_TXTC);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


void txtc_del(GF_Box *s)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)s;
	if (ptr == NULL) return;

	if (ptr->config) gf_free(ptr->config);
	gf_free(ptr);
}

GF_Err txtc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 size, i;
	char *str;
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	size = (u32) ptr->size;
	str = (char *)gf_malloc(sizeof(char)*size);

	i=0;

	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i])
			break;
		i++;
	}
	if (i) ptr->config = gf_strdup(str);
	gf_free(str);

	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err txtc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox *)s;
	GF_Err e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->config)
		gf_bs_write_data(bs, ptr->config, (u32) strlen(ptr->config));
	gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err txtc_Size(GF_Box *s)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox *)s;
	GF_Err e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->config)
		ptr->size += strlen(ptr->config);
	ptr->size++;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dac3_New(u32 boxType)
{
	ISOM_DECL_BOX_ALLOC(GF_AC3ConfigBox, GF_ISOM_BOX_TYPE_DAC3);
	if (boxType==GF_ISOM_BOX_TYPE_DEC3)
		tmp->cfg.is_ec3 = 1;
	return (GF_Box *)tmp;
}

void dac3_del(GF_Box *s)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	gf_free(ptr);
}


GF_Err dac3_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	if (ptr->cfg.is_ec3) {
		u32 i;
		ptr->cfg.brcode = gf_bs_read_int(bs, 13);
		ptr->cfg.nb_streams = gf_bs_read_int(bs, 3) + 1;
		for (i=0; i<ptr->cfg.nb_streams; i++) {
			ptr->cfg.streams[i].fscod = gf_bs_read_int(bs, 2);
			ptr->cfg.streams[i].bsid = gf_bs_read_int(bs, 5);
			ptr->cfg.streams[i].bsmod = gf_bs_read_int(bs, 5);
			ptr->cfg.streams[i].acmod = gf_bs_read_int(bs, 3);
			ptr->cfg.streams[i].lfon = gf_bs_read_int(bs, 1);
			gf_bs_read_int(bs, 3);
			ptr->cfg.streams[i].nb_dep_sub = gf_bs_read_int(bs, 4);
			if (ptr->cfg.streams[i].nb_dep_sub) {
				ptr->cfg.streams[i].chan_loc = gf_bs_read_int(bs, 9);
			} else {
				gf_bs_read_int(bs, 1);
			}
		}
	} else {
		ptr->cfg.nb_streams = 1;
		ptr->cfg.streams[0].fscod = gf_bs_read_int(bs, 2);
		ptr->cfg.streams[0].bsid = gf_bs_read_int(bs, 5);
		ptr->cfg.streams[0].bsmod = gf_bs_read_int(bs, 3);
		ptr->cfg.streams[0].acmod = gf_bs_read_int(bs, 3);
		ptr->cfg.streams[0].lfon = gf_bs_read_int(bs, 1);
		ptr->cfg.brcode = gf_bs_read_int(bs, 5);
		gf_bs_read_int(bs, 5);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dac3_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;

	if (ptr->cfg.is_ec3) s->type = GF_ISOM_BOX_TYPE_DEC3;
	e = gf_isom_box_write_header(s, bs);
	if (ptr->cfg.is_ec3) s->type = GF_ISOM_BOX_TYPE_DAC3;
	if (e) return e;

	if (ptr->cfg.is_ec3) {
		u32 i;
		gf_bs_write_int(bs, ptr->cfg.brcode, 13);
		gf_bs_write_int(bs, ptr->cfg.nb_streams - 1, 3);
		for (i=0; i<ptr->cfg.nb_streams; i++) {
			gf_bs_write_int(bs, ptr->cfg.streams[i].fscod, 2);
			gf_bs_write_int(bs, ptr->cfg.streams[i].bsid, 5);
			gf_bs_write_int(bs, ptr->cfg.streams[i].bsmod, 5);
			gf_bs_write_int(bs, ptr->cfg.streams[i].acmod, 3);
			gf_bs_write_int(bs, ptr->cfg.streams[i].lfon, 1);
			gf_bs_write_int(bs, 0, 3);
			gf_bs_write_int(bs, ptr->cfg.streams[i].nb_dep_sub, 4);
			if (ptr->cfg.streams[i].nb_dep_sub) {
				gf_bs_write_int(bs, ptr->cfg.streams[i].chan_loc, 9);
			} else {
				gf_bs_write_int(bs, 0, 1);
			}
		}
	} else {
		gf_bs_write_int(bs, ptr->cfg.streams[0].fscod, 2);
		gf_bs_write_int(bs, ptr->cfg.streams[0].bsid, 5);
		gf_bs_write_int(bs, ptr->cfg.streams[0].bsmod, 3);
		gf_bs_write_int(bs, ptr->cfg.streams[0].acmod, 3);
		gf_bs_write_int(bs, ptr->cfg.streams[0].lfon, 1);
		gf_bs_write_int(bs, ptr->cfg.brcode, 5);
		gf_bs_write_int(bs, 0, 5);
	}
	return GF_OK;
}

GF_Err dac3_Size(GF_Box *s)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	GF_Err e;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	if (ptr->cfg.is_ec3) {
		u32 i;
		s->size += 2;
		for (i=0; i<ptr->cfg.nb_streams; i++) {
			s->size += 3;
			if (ptr->cfg.streams[i].nb_dep_sub)
				s->size += 1;
		}
	} else {
		s->size += 3;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void ac3_del(GF_Box *s)
{
	GF_AC3SampleEntryBox *ptr = (GF_AC3SampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	gf_free(ptr);
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

GF_Box *ac3_New(u32 boxType)
{
	ISOM_DECL_BOX_ALLOC(GF_AC3SampleEntryBox, GF_ISOM_BOX_TYPE_AC3);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	if (boxType==GF_ISOM_BOX_TYPE_EC3)
		tmp->is_ec3 = 1;
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ac3_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AC3SampleEntryBox *ptr = (GF_AC3SampleEntryBox *)s;
	if (ptr->is_ec3) s->type = GF_ISOM_BOX_TYPE_EC3;
	e = gf_isom_box_write_header(s, bs);
	if (ptr->is_ec3) s->type = GF_ISOM_BOX_TYPE_AC3;
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	return gf_isom_box_write((GF_Box *)ptr->info, bs);
}

GF_Err ac3_Size(GF_Box *s)
{
	GF_Err e;
	GF_AC3SampleEntryBox *ptr = (GF_AC3SampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);
	e = gf_isom_box_size((GF_Box *)ptr->info);
	if (e) return e;
	ptr->size += ptr->info->size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void lsrc_del(GF_Box *s)
{
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	if (ptr == NULL) return;
	if (ptr->hdr) gf_free(ptr->hdr);
	gf_free(ptr);
}


GF_Err lsrc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	ptr->hdr_size = (u32) ptr->size;
	ptr->hdr = gf_malloc(sizeof(char)*ptr->hdr_size);
	gf_bs_read_data(bs, ptr->hdr, ptr->hdr_size);
	return GF_OK;
}

GF_Box *lsrc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_LASERConfigurationBox, GF_ISOM_BOX_TYPE_LSRC);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err lsrc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->hdr, ptr->hdr_size);
	return GF_OK;
}

GF_Err lsrc_Size(GF_Box *s)
{
	GF_Err e;
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->hdr_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void lsr1_del(GF_Box *s)
{
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->lsr_config) gf_isom_box_del((GF_Box *) ptr->lsr_config);
	if (ptr->descr) gf_isom_box_del((GF_Box *) ptr->descr);
	gf_free(ptr);
}

GF_Err lsr1_AddBox(GF_Box *s, GF_Box *a)
{
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_LSRC:
		if (ptr->lsr_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->lsr_config = (GF_LASERConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_M4DS:
		if (ptr->descr) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->descr = (GF_MPEG4ExtensionDescriptorsBox *)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err lsr1_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox*)s;
	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	ptr->size -= 8;
	return gf_isom_read_box_list(s, bs, lsr1_AddBox);
}

GF_Box *lsr1_New()
{
	ISOM_DECL_BOX_ALLOC(GF_LASeRSampleEntryBox, GF_ISOM_BOX_TYPE_LSR1);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err lsr1_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	if (ptr->lsr_config) {
		e = gf_isom_box_write((GF_Box *)ptr->lsr_config, bs);
		if (e) return e;
	}
	if (ptr->descr) {
		e = gf_isom_box_write((GF_Box *)ptr->descr, bs);
		if (e) return e;
	}
	return e;
}

GF_Err lsr1_Size(GF_Box *s)
{
	GF_Err e;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	s->size += 8;
	if (ptr->lsr_config) {
		e = gf_isom_box_size((GF_Box *)ptr->lsr_config);
		if (e) return e;
		ptr->size += ptr->lsr_config->size;
	}
	if (ptr->descr) {
		e = gf_isom_box_size((GF_Box *)ptr->descr);
		if (e) return e;
		ptr->size += ptr->descr->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void sidx_del(GF_Box *s)
{
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox *) s;
	if (ptr == NULL) return;
	if (ptr->refs) gf_free(ptr->refs);
	gf_free(ptr);
}

GF_Err sidx_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->reference_ID = gf_bs_read_u32(bs);
	ptr->timescale = gf_bs_read_u32(bs);
	ptr->size -= 8;
	if (ptr->version==0) {
		ptr->earliest_presentation_time = gf_bs_read_u32(bs);
		ptr->first_offset = gf_bs_read_u32(bs);
		ptr->size -= 8;
	} else {
		ptr->earliest_presentation_time = gf_bs_read_u64(bs);
		ptr->first_offset = gf_bs_read_u64(bs);
		ptr->size -= 16;
	}
	gf_bs_read_u16(bs); /* reserved */
	ptr->nb_refs = gf_bs_read_u16(bs);
	ptr->size -= 4;
	ptr->refs = gf_malloc(sizeof(GF_SIDXReference)*ptr->nb_refs);
	for (i=0; i<ptr->nb_refs; i++) {
		ptr->refs[i].reference_type = gf_bs_read_int(bs, 1);
		ptr->refs[i].reference_size = gf_bs_read_int(bs, 31);
		ptr->refs[i].subsegment_duration = gf_bs_read_u32(bs);
		ptr->refs[i].starts_with_SAP = gf_bs_read_int(bs, 1);
		ptr->refs[i].SAP_type = gf_bs_read_int(bs, 3);
		ptr->refs[i].SAP_delta_time = gf_bs_read_int(bs, 28);
		ptr->size -= 12;
	}
	return GF_OK;
}

GF_Box *sidx_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SegmentIndexBox, GF_ISOM_BOX_TYPE_SIDX);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err sidx_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->reference_ID);
	gf_bs_write_u32(bs, ptr->timescale);
	if (ptr->version==0) {
		gf_bs_write_u32(bs, (u32) ptr->earliest_presentation_time);
		gf_bs_write_u32(bs, (u32) ptr->first_offset);
	} else {
		gf_bs_write_u64(bs, ptr->earliest_presentation_time);
		gf_bs_write_u64(bs, ptr->first_offset);
	}
	gf_bs_write_u16(bs, 0);
	gf_bs_write_u16(bs, ptr->nb_refs);
	for (i=0; i<ptr->nb_refs; i++ ) {
		gf_bs_write_int(bs, ptr->refs[i].reference_type, 1);
		gf_bs_write_int(bs, ptr->refs[i].reference_size, 31);
		gf_bs_write_u32(bs, ptr->refs[i].subsegment_duration);
		gf_bs_write_int(bs, ptr->refs[i].starts_with_SAP, 1);
		gf_bs_write_int(bs, ptr->refs[i].SAP_type, 3);
		gf_bs_write_int(bs, ptr->refs[i].SAP_delta_time, 28);
	}
	return GF_OK;
}

GF_Err sidx_Size(GF_Box *s)
{
	GF_Err e;
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	ptr->size += 12;
	if (ptr->version==0) {
		ptr->size += 8;
	} else {
		ptr->size += 16;
	}
	ptr->size += ptr->nb_refs * 12;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *pcrb_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PcrInfoBox, GF_ISOM_BOX_TYPE_PCRB);
	return (GF_Box *)tmp;
}

void pcrb_del(GF_Box *s)
{
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox *) s;
	if (ptr == NULL) return;
	if (ptr->pcr_values) gf_free(ptr->pcr_values);
	gf_free(ptr);
}

GF_Err pcrb_Read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;

	ptr->subsegment_count = gf_bs_read_u32(bs);
	ptr->size -= 4;

	ptr->pcr_values = gf_malloc(sizeof(u64)*ptr->subsegment_count);
	for (i=0; i<ptr->subsegment_count; i++) {
		u64 data1 = gf_bs_read_u32(bs);
		u64 data2 = gf_bs_read_u16(bs);
		ptr->size -= 6;
		ptr->pcr_values[i] = (data1 << 10) | (data2 >> 6);

	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pcrb_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->subsegment_count);

	for (i=0; i<ptr->subsegment_count; i++ ) {
		u32 data1 = (u32) (ptr->pcr_values[i] >> 10);
		u16 data2 = (u16) (ptr->pcr_values[i] << 6);

		gf_bs_write_u32(bs, data1);
		gf_bs_write_u16(bs, data2);
	}
	return GF_OK;
}

GF_Err pcrb_Size(GF_Box *s)
{
	GF_Err e;
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;
	e = gf_isom_box_get_size(s);
	if (e) return e;

	ptr->size += 4;
	ptr->size += ptr->subsegment_count * 6;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *subs_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SubSampleInformationBox, GF_ISOM_BOX_TYPE_SUBS);
	tmp->Samples = gf_list_new();
	return (GF_Box *)tmp;
}

void subs_del(GF_Box *s)
{
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *)s;
	if (ptr == NULL) return;

	while (gf_list_count(ptr->Samples)) {
		GF_SubSampleInfoEntry *pSamp;
		pSamp = (GF_SubSampleInfoEntry*)gf_list_get(ptr->Samples, 0);
		while (gf_list_count(pSamp->SubSamples)) {
			GF_SubSampleEntry *pSubSamp;
			pSubSamp = (GF_SubSampleEntry*) gf_list_get(pSamp->SubSamples, 0);
			gf_free(pSubSamp);
			gf_list_rem(pSamp->SubSamples, 0);
		}
		gf_list_del(pSamp->SubSamples);
		gf_free(pSamp);
		gf_list_rem(ptr->Samples, 0);
	}
	gf_list_del(ptr->Samples);
	gf_free(ptr);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err subs_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j, entry_count;
	u16 subsample_count;
	GF_SubSampleInfoEntry *pSamp;
	GF_SubSampleEntry *pSubSamp;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) s;

	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	entry_count = gf_list_count(ptr->Samples);
	gf_bs_write_u32(bs, entry_count);

	for (i=0; i<entry_count; i++) {
		pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);
		subsample_count = gf_list_count(pSamp->SubSamples);
		gf_bs_write_u32(bs, pSamp->sample_delta);
		gf_bs_write_u16(bs, subsample_count);

		for (j=0; j<subsample_count; j++) {
			pSubSamp = (GF_SubSampleEntry*) gf_list_get(pSamp->SubSamples, j);
			if (ptr->version == 1) {
				gf_bs_write_u32(bs, pSubSamp->subsample_size);
			} else {
				gf_bs_write_u16(bs, pSubSamp->subsample_size);
			}
			gf_bs_write_u8(bs, pSubSamp->subsample_priority);
			gf_bs_write_u8(bs, pSubSamp->discardable);
			gf_bs_write_u32(bs, pSubSamp->reserved);
		}
	}
	return e;
}

GF_Err subs_Size(GF_Box *s)
{
	GF_Err e;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) s;
	GF_SubSampleInfoEntry *pSamp;
	u32 entry_count, i;
	u16 subsample_count;

	// determine the size of the full box
	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	// add 4 byte for entry_count
	ptr->size += 4;
	entry_count = gf_list_count(ptr->Samples);
	for (i=0; i<entry_count; i++) {
		pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);
		subsample_count = gf_list_count(pSamp->SubSamples);
		// 4 byte for sample_delta, 2 byte for subsample_count
		// and 6 + (4 or 2) bytes for each subsample
		ptr->size += 4 + 2 + subsample_count * (6 + (ptr->version==1 ? 4 : 2));
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Err subs_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *)s;
	u32 entry_count, i, j;
	u16 subsample_count;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	entry_count = gf_bs_read_u32(bs);
	ptr->size -= 4;

	for (i=0; i<entry_count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry*) gf_malloc(sizeof(GF_SubSampleInfoEntry));
		if (!pSamp) return GF_OUT_OF_MEM;

		memset(pSamp, 0, sizeof(GF_SubSampleInfoEntry));
		pSamp->SubSamples = gf_list_new();
		pSamp->sample_delta = gf_bs_read_u32(bs);
		subsample_count = gf_bs_read_u16(bs);

		for (j=0; j<subsample_count; j++) {
			GF_SubSampleEntry *pSubSamp = (GF_SubSampleEntry*) gf_malloc(sizeof(GF_SubSampleEntry));
			if (!pSubSamp) return GF_OUT_OF_MEM;

			memset(pSubSamp, 0, sizeof(GF_SubSampleEntry));
			if (ptr->version==1) {
				pSubSamp->subsample_size = gf_bs_read_u32(bs);
			} else {
				pSubSamp->subsample_size = gf_bs_read_u16(bs);
			}
			pSubSamp->subsample_priority = gf_bs_read_u8(bs);
			pSubSamp->discardable = gf_bs_read_u8(bs);
			pSubSamp->reserved = gf_bs_read_u32(bs);

			gf_list_add(pSamp->SubSamples, pSubSamp);
		}
		gf_list_add(ptr->Samples, pSamp);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

GF_Box *tfdt_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TFBaseMediaDecodeTimeBox, GF_ISOM_BOX_TYPE_TFDT);
	return (GF_Box *)tmp;
}

void tfdt_del(GF_Box *s)
{
	gf_free(s);
}

/*this is using chpl format according to some NeroRecode samples*/
GF_Err tfdt_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	if (ptr->version==1) {
		ptr->baseMediaDecodeTime = gf_bs_read_u64(bs);
		ptr->size -= 8;
	} else {
		ptr->baseMediaDecodeTime = (u32) gf_bs_read_u32(bs);
		ptr->size -= 4;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tfdt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *) s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->version==1) {
		gf_bs_write_u64(bs, ptr->baseMediaDecodeTime);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->baseMediaDecodeTime);
	}
	return GF_OK;
}

GF_Err tfdt_Size(GF_Box *s)
{
	GF_Err e;
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->baseMediaDecodeTime<=0xFFFFFFFF) {
		ptr->version = 0;
		ptr->size += 4;
	} else {
		ptr->version = 1;
		ptr->size += 8;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


GF_Box *rvcc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_RVCConfigurationBox, GF_ISOM_BOX_TYPE_RVCC);
	return (GF_Box *)tmp;
}

void rvcc_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err rvcc_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox*)s;
	ptr->predefined_rvc_config = gf_bs_read_u16(bs);
	ptr->size -= 2;
	if (!ptr->predefined_rvc_config) {
		ptr->rvc_meta_idx = gf_bs_read_u16(bs);
		ptr->size -= 2;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err rvcc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox*) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->predefined_rvc_config);
	if (!ptr->predefined_rvc_config) {
		gf_bs_write_u16(bs, ptr->rvc_meta_idx);
	}
	return GF_OK;
}

GF_Err rvcc_Size(GF_Box *s)
{
	GF_Err e;
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 2;
	if (! ptr->predefined_rvc_config) ptr->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *sbgp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleGroupBox, GF_ISOM_BOX_TYPE_SBGP);
	return (GF_Box *)tmp;
}
void sbgp_del(GF_Box *a)
{
	GF_SampleGroupBox *p = (GF_SampleGroupBox *)a;
	if (p->sample_entries) gf_free(p->sample_entries);
	gf_free(p);
}

GF_Err sbgp_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SampleGroupBox *p = (GF_SampleGroupBox *)s;
	GF_Err e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	p->grouping_type = gf_bs_read_u32(bs);
	p->size -= 4;
	if (p->version==1) {
		p->grouping_type_parameter = gf_bs_read_u32(bs);
		p->size -= 4;
	}
	p->entry_count = gf_bs_read_u32(bs);
	p->size -= 4;
	p->sample_entries = gf_malloc(sizeof(GF_SampleGroupEntry)*p->entry_count);
	if (!p->sample_entries) return GF_IO_ERR;
	for (i=0; i<p->entry_count; i++) {
		p->sample_entries[i].sample_count = gf_bs_read_u32(bs);
		p->sample_entries[i].group_description_index = gf_bs_read_u32(bs);
		p->size -= 8;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sbgp_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_SampleGroupBox *p = (GF_SampleGroupBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, p->grouping_type);
	if (p->version==1)
		gf_bs_write_u32(bs, p->grouping_type_parameter);

	gf_bs_write_u32(bs, p->entry_count);
	for (i = 0; i<p->entry_count; i++ ) {
		gf_bs_write_u32(bs, p->sample_entries[i].sample_count);
		gf_bs_write_u32(bs, p->sample_entries[i].group_description_index);
	}
	return GF_OK;
}

GF_Err sbgp_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleGroupBox *p = (GF_SampleGroupBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	p->size += 8;
	if (p->grouping_type_parameter) p->version=1;
	
	if (p->version==1) p->size += 4;
	p->size += 8*p->entry_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

static void *sgpd_parse_entry(u32 grouping_type, GF_BitStream *bs, u32 entry_size, u32 *total_bytes)
{
	GF_DefaultSampleGroupDescriptionEntry *ptr;
	switch (grouping_type) {
	case GF_4CC( 'r', 'o', 'l', 'l' ):
	{
		GF_RollRecoveryEntry *ptr;
		GF_SAFEALLOC(ptr, GF_RollRecoveryEntry);
		if (!ptr) return NULL;
		ptr->roll_distance = gf_bs_read_int(bs, 16);
		*total_bytes = 2;
		return ptr;
	}
	case GF_4CC( 'r', 'a', 'p', ' ' ):
	{
		GF_VisualRandomAccessEntry *ptr;
		GF_SAFEALLOC(ptr, GF_VisualRandomAccessEntry);
		if (!ptr) return NULL;
		ptr->num_leading_samples_known = gf_bs_read_int(bs, 1);
		ptr->num_leading_samples = gf_bs_read_int(bs, 7);
		*total_bytes = 1;
		return ptr;
	}
	case GF_4CC( 's', 'e', 'i', 'g' ):
	{
		GF_CENCSampleEncryptionGroupEntry *ptr;
		GF_SAFEALLOC(ptr, GF_CENCSampleEncryptionGroupEntry);
		if (!ptr) return NULL;
		gf_bs_read_u8(bs); //reserved
		ptr->crypt_byte_block = gf_bs_read_int(bs, 4);
		ptr->skip_byte_block = gf_bs_read_int(bs, 4);
		ptr->IsProtected = gf_bs_read_u8(bs);
		ptr->Per_Sample_IV_size = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *)ptr->KID, 16);
		*total_bytes = 20;
		if ((ptr->IsProtected == 1) && !ptr->Per_Sample_IV_size) {
			ptr->constant_IV_size = gf_bs_read_u8(bs);
			assert((ptr->constant_IV_size == 8) || (ptr->constant_IV_size == 16));
			gf_bs_read_data(bs, (char *)ptr->constant_IV, ptr->constant_IV_size);
			*total_bytes += 1 + ptr->constant_IV_size;
		}
		if (!entry_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] seig sample group does not indicate entry size, deprecated in spec\n"));
		}
		return ptr;
	}
	case GF_4CC('o','i','n','f'):
	{
		GF_OperatingPointsInformation *ptr = gf_isom_oinf_new_entry();
		u32 s = (u32) gf_bs_get_position(bs);
		gf_isom_oinf_read_entry(ptr, bs);
		*total_bytes = (u32) gf_bs_get_position(bs) - s;
		if (!entry_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] oinf sample group does not indicate entry size, deprecated in spec\n"));
		}
		return ptr;
	}
	case GF_4CC('l','i','n','f'):
	{
		GF_LHVCLayerInformation *ptr = gf_isom_linf_new_entry();
		u32 s = (u32) gf_bs_get_position(bs);
		gf_isom_linf_read_entry(ptr, bs);
		*total_bytes = (u32) gf_bs_get_position(bs) - s;
		if (!entry_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] linf sample group does not indicate entry size, deprecated in spec\n"));
		}
		return ptr;
	}
		
	case GF_4CC( 't', 'r', 'i', 'f' ):
		if (! entry_size) {
			u32 flags = gf_bs_peek_bits(bs, 24, 0);
			if (flags & 0x10000) entry_size=3;
			else {
				if (flags & 0x80000) entry_size=7;
				else entry_size=11;
				//have dependency list
				if (flags & 0x200000) {
					u32 nb_entries = gf_bs_peek_bits(bs, 16, entry_size);
					entry_size += 2 + 2*nb_entries;
				}
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] trif sample group does not indicate entry size, deprecated in spec\n"));
		}
		//fallthrough
		break;
	case GF_4CC( 'n', 'a', 'l', 'm' ):
		if (! entry_size) {
			u64 start = gf_bs_get_position(bs);
			Bool rle, large_size;
			u32 entry_count;
			gf_bs_read_int(bs, 6);
			large_size = gf_bs_read_int(bs, 1);
			rle = gf_bs_read_int(bs, 1);
			entry_count = gf_bs_read_int(bs, large_size ? 16 : 8);
			gf_bs_seek(bs, start);
			entry_size = 1 + large_size ? 2 : 1;
			entry_size += entry_count * 2;
			if (rle) entry_size += entry_count * (large_size ? 2 : 1);
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] nalm sample group does not indicate entry size, deprecated in spec\n"));
		}
		break;
	default:
		break;
	}

	if (!entry_size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] %s sample group does not indicate entry size, cannot parse!\n", gf_4cc_to_str( grouping_type) ));
		return NULL;
	}
	GF_SAFEALLOC(ptr, GF_DefaultSampleGroupDescriptionEntry);
	if (!ptr) return NULL;
	ptr->length = entry_size;
	ptr->data = (u8 *) gf_malloc(sizeof(u8)*ptr->length);
	gf_bs_read_data(bs, (char *) ptr->data, ptr->length);
	*total_bytes = entry_size;
	return ptr;
}

static void	sgpd_del_entry(u32 grouping_type, void *entry)
{
	switch (grouping_type) {
	case GF_4CC( 'r', 'o', 'l', 'l' ):
	case GF_4CC( 'r', 'a', 'p', ' ' ):
	case GF_4CC( 's', 'e', 'i', 'g' ):
		gf_free(entry);
		return;
	case GF_4CC( 'o', 'i', 'n', 'f' ):
		gf_isom_oinf_del_entry(entry);
		return;
	case GF_4CC( 'l', 'i', 'n', 'f' ):
		gf_isom_linf_del_entry(entry);
		return;
	default:
	{
		GF_DefaultSampleGroupDescriptionEntry *ptr = (GF_DefaultSampleGroupDescriptionEntry *)entry;
		if (ptr->data) gf_free(ptr->data);
		gf_free(ptr);
	}

	}
}

void sgpd_write_entry(u32 grouping_type, void *entry, GF_BitStream *bs)
{
	switch (grouping_type) {
	case GF_4CC( 'r', 'o', 'l', 'l' ):
		gf_bs_write_int(bs, ((GF_RollRecoveryEntry*)entry)->roll_distance, 16);
		return;
	case GF_4CC( 'r', 'a', 'p', ' ' ):
		gf_bs_write_int(bs, ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known, 1);
		gf_bs_write_int(bs, ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples, 7);
		return;
	case GF_4CC( 's', 'e', 'i', 'g' ):
		gf_bs_write_u8(bs, 0x0);
		gf_bs_write_int(bs, ((GF_CENCSampleEncryptionGroupEntry*)entry)->crypt_byte_block, 4);
		gf_bs_write_int(bs, ((GF_CENCSampleEncryptionGroupEntry*)entry)->skip_byte_block, 4);
		gf_bs_write_u8(bs, ((GF_CENCSampleEncryptionGroupEntry *)entry)->IsProtected);
		gf_bs_write_u8(bs, ((GF_CENCSampleEncryptionGroupEntry *)entry)->Per_Sample_IV_size);
		gf_bs_write_data(bs, (char *)((GF_CENCSampleEncryptionGroupEntry *)entry)->KID, 16);
		if ((((GF_CENCSampleEncryptionGroupEntry *)entry)->IsProtected == 1) && !((GF_CENCSampleEncryptionGroupEntry *)entry)->Per_Sample_IV_size) {
			gf_bs_write_u8(bs, ((GF_CENCSampleEncryptionGroupEntry *)entry)->constant_IV_size);
			gf_bs_write_data(bs, (char *)((GF_CENCSampleEncryptionGroupEntry *)entry)->constant_IV, ((GF_CENCSampleEncryptionGroupEntry *)entry)->constant_IV_size);
		}
		return;
	case GF_4CC( 'o', 'i', 'n', 'f' ):
		gf_isom_oinf_write_entry(entry, bs);
		return;
	case GF_4CC( 'l', 'i', 'n', 'f' ):
		gf_isom_linf_write_entry(entry, bs);
		return;
	default:
	{
		GF_DefaultSampleGroupDescriptionEntry *ptr = (GF_DefaultSampleGroupDescriptionEntry *)entry;
		gf_bs_write_data(bs, (char *) ptr->data, ptr->length);
	}
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE
static u32 sgpd_size_entry(u32 grouping_type, void *entry)
{
	switch (grouping_type) {
	case GF_4CC( 'r', 'o', 'l', 'l' ):
		return 2;
	case GF_4CC( 'r', 'a', 'p', ' ' ):
		return 1;
	case GF_4CC( 's', 'e', 'i', 'g' ):
		return ((((GF_CENCSampleEncryptionGroupEntry *)entry)->IsProtected == 1) && !((GF_CENCSampleEncryptionGroupEntry *)entry)->Per_Sample_IV_size) ? 21 + ((GF_CENCSampleEncryptionGroupEntry *)entry)->constant_IV_size : 20;
	case GF_4CC( 'o', 'i', 'n', 'f' ):
		return gf_isom_oinf_size_entry(entry);
	case GF_4CC( 'l', 'i', 'n', 'f' ):
		return gf_isom_linf_size_entry(entry);
	default:
		return ((GF_DefaultSampleGroupDescriptionEntry *)entry)->length;
	}
}
#endif

GF_Box *sgpd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleGroupDescriptionBox, GF_ISOM_BOX_TYPE_SGPD);
	/*version 0 is deprecated, use v1 by default*/
	tmp->version = 1;
	tmp->group_descriptions = gf_list_new();
	return (GF_Box *)tmp;
}

void sgpd_del(GF_Box *a)
{
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)a;
	while (gf_list_count(p->group_descriptions)) {
		void *ptr = gf_list_last(p->group_descriptions);
		sgpd_del_entry(p->grouping_type, ptr);
		gf_list_rem_last(p->group_descriptions);
	}
	gf_list_del(p->group_descriptions);
	gf_free(p);
}

GF_Err sgpd_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 entry_count;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;
	GF_Err e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	p->grouping_type = gf_bs_read_u32(bs);
	p->size -= 4;

	if (p->version>=1) {
		p->default_length = gf_bs_read_u32(bs);
		p->size -= 4;
	}
	if (p->version>=2) {
		p->default_description_index = gf_bs_read_u32(bs);
		p->size -= 4;
	}
	entry_count = gf_bs_read_u32(bs);
	p->size -= 4;

	while (entry_count) {
		void *ptr;
		u32 parsed_bytes;
		u32 size = p->default_length;
		if ((p->version>=1) && !size) {
			size = gf_bs_read_u32(bs);
			p->size -= 4;
		}
		ptr = sgpd_parse_entry(p->grouping_type, bs, size, &parsed_bytes);
		if (!ptr) return GF_ISOM_INVALID_FILE;
		if (p->size < parsed_bytes) return GF_ISOM_INVALID_FILE;
		p->size -= parsed_bytes;
		gf_list_add(p->group_descriptions, ptr);
		entry_count--;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sgpd_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;
	GF_Err e;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, p->grouping_type);
	if (p->version>=1) gf_bs_write_u32(bs, p->default_length);
	if (p->version>=2) gf_bs_write_u32(bs, p->default_description_index);
	gf_bs_write_u32(bs, gf_list_count(p->group_descriptions) );

	for (i=0; i<gf_list_count(p->group_descriptions); i++) {
		void *ptr = gf_list_get(p->group_descriptions, i);
		if ((p->version >= 1) && !p->default_length) {
			u32 size = sgpd_size_entry(p->grouping_type, ptr);
			gf_bs_write_u32(bs, size);
		}
		sgpd_write_entry(p->grouping_type, ptr, bs);
	}
	return GF_OK;
}

GF_Err sgpd_Size(GF_Box *s)
{
	u32 i;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;
	GF_Err e;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	p->size += 8;

	//we force all sample groups to version 1, v0 being deprecated
	p->version=1;
	p->size += 4;
	
	if (p->version>=2) p->size += 4;
	p->default_length = 0;

	for (i=0; i<gf_list_count(p->group_descriptions); i++) {
		void *ptr = gf_list_get(p->group_descriptions, i);
		u32 size = sgpd_size_entry(p->grouping_type, ptr);
		p->size += size;
		if (!p->default_length) {
			p->default_length = size;
		} else if (p->default_length != size) {
			p->default_length = 0;
		}
	}
	if (p->version>=1) {
		if (!p->default_length) p->size += gf_list_count(p->group_descriptions)*4;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void saiz_del(GF_Box *s)
{
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;
	if (ptr == NULL) return;
	if (ptr->sample_info_size) gf_free(ptr->sample_info_size);
	gf_free(ptr);
}


GF_Err saiz_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->flags & 1) {
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);
		ptr->size -= 8;
	}
	ptr->default_sample_info_size = gf_bs_read_u8(bs);
	ptr->sample_count = gf_bs_read_u32(bs);
	ptr->size -= 5;
	if (ptr->default_sample_info_size == 0) {
		ptr->sample_info_size = gf_malloc(sizeof(u8)*ptr->sample_count);
		gf_bs_read_data(bs, (char *) ptr->sample_info_size, ptr->sample_count);
		ptr->size -= ptr->sample_count;
	}
	return GF_OK;
}

GF_Box *saiz_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleAuxiliaryInfoSizeBox, GF_ISOM_BOX_TYPE_SAIZ);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err saiz_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->flags & 1) {
		gf_bs_write_u32(bs, ptr->aux_info_type);
		gf_bs_write_u32(bs, ptr->aux_info_type_parameter);
	}
	gf_bs_write_u8(bs, ptr->default_sample_info_size);
	gf_bs_write_u32(bs, ptr->sample_count);
	if (!ptr->default_sample_info_size) {
		gf_bs_write_data(bs, (char *) ptr->sample_info_size, ptr->sample_count);
	}
	return GF_OK;
}

GF_Err saiz_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoSizeBox *ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;

	if (ptr->aux_info_type || ptr->aux_info_type_parameter) {
		ptr->flags |= 1;
	}

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->flags & 1) ptr->size += 8;
	ptr->size += 5;
	if (ptr->default_sample_info_size==0)  ptr->size += ptr->sample_count;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

void saio_del(GF_Box *s)
{
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*)s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	if (ptr->offsets_large) gf_free(ptr->offsets_large);
	gf_free(ptr);
}


GF_Err saio_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	if (ptr->flags & 1) {
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);
		ptr->size -= 8;
	}
	ptr->entry_count = gf_bs_read_u32(bs);
	ptr->size -= 4;
	if (ptr->entry_count) {
		u32 i;
		if (ptr->version==0) {
			ptr->offsets = gf_malloc(sizeof(u32)*ptr->entry_count);
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets[i] = gf_bs_read_u32(bs);
			ptr->size -= 4*ptr->entry_count;
		} else {
			ptr->offsets_large = gf_malloc(sizeof(u64)*ptr->entry_count);
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets_large[i] = gf_bs_read_u64(bs);
			ptr->size -= 8*ptr->entry_count;
		}
	}
	return GF_OK;
}

GF_Box *saio_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleAuxiliaryInfoOffsetBox, GF_ISOM_BOX_TYPE_SAIO);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err saio_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->flags & 1) {
		gf_bs_write_u32(bs, ptr->aux_info_type);
		gf_bs_write_u32(bs, ptr->aux_info_type_parameter);
	}
	gf_bs_write_u32(bs, ptr->entry_count);
	if (ptr->entry_count) {
		u32 i;
		//store position in bitstream before writing data - offsets can be NULL if a single offset is rewritten later on (cf senc_write)
		ptr->offset_first_offset_field = gf_bs_get_position(bs);
		if (ptr->version==0) {
			if (!ptr->offsets) {
				gf_bs_write_u32(bs, 0);
			} else {
				for (i=0; i<ptr->entry_count; i++)
					gf_bs_write_u32(bs, ptr->offsets[i]);
			}
		} else {
			if (!ptr->offsets_large) {
				gf_bs_write_u64(bs, 0);
			} else {
				for (i=0; i<ptr->entry_count; i++)
					gf_bs_write_u64(bs, ptr->offsets_large[i]);
			}
		}
	}
	return GF_OK;
}

GF_Err saio_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*)s;

	if (ptr->aux_info_type || ptr->aux_info_type_parameter) {
		ptr->flags |= 1;
	}
	if (ptr->offsets_large) {
		ptr->version = 1;
	}

	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (ptr->flags & 1) ptr->size += 8;
	ptr->size += 4;
	ptr->size += ((ptr->version==1) ? 8 : 4) * ptr->entry_count;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


void prft_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err prft_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->refTrackID = gf_bs_read_u32(bs);
	ptr->ntp = gf_bs_read_u64(bs);
	if (ptr->version==0) {
		ptr->timestamp = gf_bs_read_u32(bs);
	} else {
		ptr->timestamp = gf_bs_read_u64(bs);
	}
	return GF_OK;
}

GF_Box *prft_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ProducerReferenceTimeBox, GF_ISOM_BOX_TYPE_PRFT);
	gf_isom_full_box_init((GF_Box *)tmp);
	return (GF_Box *)tmp;
}

void mmpu_del(GF_Box *s)
{
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	if (ptr == NULL) return;
	if(ptr->asset_id_value)gf_free(ptr->asset_id_value);
	gf_free(ptr);
}

GF_Box *mmpu_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MmtMmpuBox, GF_ISOM_BOX_TYPE_MMPU);
	return (GF_Box *)tmp;
}

GF_Err mmpu_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 p;

	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->is_complete=gf_bs_read_int(bs,1);
	ptr->is_adc_present=gf_bs_read_int(bs, 1);
	ptr->reserved=gf_bs_read_int(bs,6);
	ptr->mpu_sequence_number=gf_bs_read_u32(bs);
	ptr->asset_id_scheme=gf_bs_read_u32(bs);
	ptr->asset_id_length=gf_bs_read_u32(bs);
	if(ptr->asset_id_length>0)
		ptr->asset_id_value=(u8 *)gf_malloc(sizeof(u8)*(ptr->asset_id_length));

	for(p=0;p<ptr->asset_id_length;p++)
		ptr->asset_id_value[p]=gf_bs_read_u8(bs);

	return GF_OK;
}

GF_Err mmpu_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	int p;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->is_complete, 1);
	gf_bs_write_int(bs, ptr->is_adc_present, 1);
	gf_bs_write_int(bs, ptr->reserved, 6);

	gf_bs_write_u32(bs, ptr->mpu_sequence_number);
	gf_bs_write_u32(bs, ptr->asset_id_scheme);
	gf_bs_write_u32(bs, ptr->asset_id_length);

	for(p=0;p<ptr->asset_id_length;p++)
		gf_bs_write_u8(bs, ptr->asset_id_value[p]);

	return GF_OK;
}

GF_Err mmpu_Size(GF_Box *s)
{
	GF_Err e;
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	ptr->size += 13;
	ptr->size += ptr->asset_id_length;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM*/
