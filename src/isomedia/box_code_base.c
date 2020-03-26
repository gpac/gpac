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
	u32 entries;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	ptr->nb_entries = gf_bs_read_u32(bs);

	ISOM_DECREASE_SIZE(ptr, 4)

	if (ptr->nb_entries > ptr->size / 8) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in co64\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

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
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;

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
	GF_ChapterEntry *ce;
	u32 nb_chaps, len, i, count;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *)s;

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
	u32 count, i;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *)s;

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
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;

	gf_bs_read_int(bs, 1);
	//the spec is unclear here, just says "the value 0 is interpreted as undetermined"
	ptr->packedLanguageCode[0] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[1] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[2] = gf_bs_read_int(bs, 5);
	ISOM_DECREASE_SIZE(ptr, 2);

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
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;

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
	GF_KindBox *ptr = (GF_KindBox *)s;

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
	GF_KindBox *ptr = (GF_KindBox *)s;

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
	u32 i;
	u32 sampleCount;
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;

	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->nb_entries > ptr->size / 8) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in ctts\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

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
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *) s;

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
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

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
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	ptr->size += 20;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ccst_del(GF_Box *s)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;
	if (ptr) gf_free(ptr);
	return;
}

GF_Err ccst_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->all_ref_pics_intra = gf_bs_read_int(bs, 1);
	ptr->intra_pred_used = gf_bs_read_int(bs, 1);
	ptr->max_ref_per_pic = gf_bs_read_int(bs, 4);
	ptr->reserved = gf_bs_read_int(bs, 26);
	return GF_OK;
}

GF_Box *ccst_New()
{
	ISOM_DECL_BOX_ALLOC(GF_CodingConstraintsBox, GF_ISOM_BOX_TYPE_CCST);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ccst_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->all_ref_pics_intra, 1);
	gf_bs_write_int(bs, ptr->intra_pred_used, 1);
	gf_bs_write_int(bs, ptr->max_ref_per_pic, 4);
	gf_bs_write_int(bs, 0, 26);
	return GF_OK;
}

GF_Err ccst_Size(GF_Box *s)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;
	ptr->size += 4;
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
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

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
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

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
	u32 i, to_read;
	char *tmpName;
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;
	if (! ptr->size ) return GF_OK;

	//here we have to handle that in a clever way
	to_read = (u32) ptr->size;
	tmpName = (char*)gf_malloc(sizeof(char) * to_read);
	if (!tmpName) return GF_OUT_OF_MEM;
	//get the data
	gf_bs_read_data(bs, tmpName, to_read);

	//then get the break
	i = 0;
	while ( (i < to_read) && (tmpName[i] != 0) ) {
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
	memcpy(ptr->nameURN, tmpName, i + 1);

	if (tmpName[to_read - 1] != 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] urn box contains invalid location field\n" ));
	}
	else {
		ptr->location = (char*)gf_malloc(sizeof(char) * (to_read - i - 1));
		if (!ptr->location) {
			gf_free(tmpName);
			gf_free(ptr->nameURN);
			ptr->nameURN = NULL;
			return GF_OUT_OF_MEM;
		}
		memcpy(ptr->location, tmpName + i + 1, (to_read - i - 1));
	}

	gf_free(tmpName);
	return GF_OK;
}

GF_Box *urn_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryURNBox, GF_ISOM_BOX_TYPE_URN);
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
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;

	if ( !(ptr->flags & 1)) {
		if (ptr->nameURN) ptr->size += 1 + strlen(ptr->nameURN);
		if (ptr->location) ptr->size += 1 + strlen(ptr->location);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void unkn_del(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *) s;
	if (!s) return;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err unkn_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 bytesToRead, sub_size, sub_a;
	GF_BitStream *sub_bs;
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

	//try to parse container boxes, check if next 8 bytes match a subbox
	sub_bs = gf_bs_new(ptr->data, ptr->dataSize, GF_BITSTREAM_READ);
	sub_size = gf_bs_read_u32(sub_bs);
	sub_a = gf_bs_read_u8(sub_bs);
	e = (sub_size && (sub_size <= ptr->dataSize)) ? GF_OK : GF_NOT_SUPPORTED;
	if (! isalnum(sub_a)) e = GF_NOT_SUPPORTED;
	sub_a = gf_bs_read_u8(sub_bs);
	if (! isalnum(sub_a)) e = GF_NOT_SUPPORTED;
	sub_a = gf_bs_read_u8(sub_bs);
	if (! isalnum(sub_a)) e = GF_NOT_SUPPORTED;
	sub_a = gf_bs_read_u8(sub_bs);
	if (! isalnum(sub_a)) e = GF_NOT_SUPPORTED;

	if (e == GF_OK) {
		gf_bs_seek(sub_bs, 0);
		e = gf_isom_box_array_read(s, sub_bs, gf_isom_box_add_default);
	}
	gf_bs_del(sub_bs);
	if (e==GF_OK) {
		gf_free(ptr->data);
		ptr->data = NULL;
		ptr->dataSize = 0;
	} else if (s->other_boxes) {
		gf_isom_box_array_del(s->other_boxes);
		s->other_boxes=NULL;
	}

	return GF_OK;
}

GF_Box *unkn_New()
{
	ISOM_DECL_BOX_ALLOC(GF_UnknownBox, GF_ISOM_BOX_TYPE_UNKNOWN);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err unkn_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 type = s->type;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->type = ptr->original_4cc;
	e = gf_isom_box_write_header(s, bs);
	ptr->type = type;
	if (e) return e;

	if (ptr->dataSize && ptr->data) {
		gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Err unkn_Size(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;

	if (ptr->dataSize && ptr->data) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void def_cont_box_del(GF_Box *s)
{
	if (s) gf_free(s);
}


GF_Err def_cont_box_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, gf_isom_box_add_default);
}

GF_Box *def_cont_box_New()
{
	ISOM_DECL_BOX_ALLOC(GF_Box, 0);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITEHintSa

GF_Err def_cont_box_Write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err def_cont_box_Size(GF_Box *s)
{
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
	GF_UnknownUUIDBox*ptr = (GF_UnknownUUIDBox*)s;
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
	GF_Err e = gf_isom_box_array_read(s, bs, dinf_AddBox);
	if (e) {
		return e;
	}
	if (!((GF_DataInformationBox *)s)->dref) {
		GF_Box* dref;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing dref box in dinf\n"));
		dref = gf_isom_box_new(GF_ISOM_BOX_TYPE_DREF);
		((GF_DataInformationBox *)s)->dref = (GF_DataReferenceBox *)dref;
		gf_isom_box_add_for_dump_mode(s, dref);
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
	return gf_isom_box_add_default(ptr, entry);
}

GF_Err dref_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;

	if (ptr == NULL) return GF_BAD_PARAM;
	gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	return gf_isom_box_array_read(s, bs, dref_AddDataEntry);
}

GF_Box *dref_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataReferenceBox, GF_ISOM_BOX_TYPE_DREF);
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
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

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
	return gf_isom_box_array_read(s, bs, edts_AddBox);
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
	u32 entries;
	s32 tr;
	u32 nb_entries;
	GF_EdtsEntry *p;
	GF_EditListBox *ptr = (GF_EditListBox *)s;

	nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->version == 1) {
		if (nb_entries > ptr->size / 20) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in ctts\n", nb_entries));
			return GF_ISOM_INVALID_FILE;
		}
	} else {
		if (nb_entries > ptr->size / 12) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in ctts\n", nb_entries));
			return GF_ISOM_INVALID_FILE;
		}
	}


	for (entries = 0; entries < nb_entries; entries++) {
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
	u32 durtimebytes;
	u32 i, nb_entries;
	GF_EditListBox *ptr = (GF_EditListBox *)s;

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
	GF_Err e=GF_OK;
	u32 descSize;
	char *enc_desc;
	u32 SLIsPredefined(GF_SLConfig *sl);
	GF_ESDBox *ptr = (GF_ESDBox *)s;

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

		if (ptr->desc && (ptr->desc->tag!=GF_ODF_ESD_TAG) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid descriptor tag 0x%x in esds\n", ptr->desc->tag));
			gf_odf_desc_del((GF_Descriptor*)ptr->desc);
			ptr->desc=NULL;
			return GF_ISOM_INVALID_FILE;
		}

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
	return e;
}

GF_Box *esds_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ESDBox, GF_ISOM_BOX_TYPE_ESDS);
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
	u32 descSize = 0;
	GF_ESDBox *ptr = (GF_ESDBox *)s;
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
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
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
	ISOM_DECREASE_SIZE(ptr, 8);

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
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;

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

//dummy
GF_Err gnrm_Read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
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
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRM;
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
//dummy
GF_Err gnrv_Read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
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
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRV;
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
//dummy
GF_Err gnra_Read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
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
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRA;
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
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;

	ptr->reserved1 = gf_bs_read_u32(bs);
	ptr->handlerType = gf_bs_read_u32(bs);
	gf_bs_read_data(bs, (char*)ptr->reserved2, 12);

	ISOM_DECREASE_SIZE(ptr, 20);

	if (ptr->size) {
		ptr->nameUTF8 = (char*)gf_malloc((u32) ptr->size);
		if (ptr->nameUTF8 == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->nameUTF8, (u32) ptr->size);

		//patch for old QT files - we cannot rely on checking if str[0]==len(str+1) since we may have
		//cases where the first character of the string decimal value is indeed the same as the string length!!
		//we had this issue with encryption_import test
		//we therefore only check if last char is null, and if not so assume old QT style
		if (ptr->nameUTF8[ptr->size-1]) {
			memmove(ptr->nameUTF8, ptr->nameUTF8+1, sizeof(char) * (u32) (ptr->size-1) );
			ptr->nameUTF8[ptr->size-1] = 0;
			ptr->store_counted_string = GF_TRUE;
		}
	}
	return GF_OK;
}

GF_Box *hdlr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HandlerBox, GF_ISOM_BOX_TYPE_HDLR);
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

	if (ptr->nameUTF8) {
		u32 len = (u32)strlen(ptr->nameUTF8);
		if (ptr->store_counted_string) {
			gf_bs_write_u8(bs, len);
			gf_bs_write_data(bs, ptr->nameUTF8, len);
		} else {
			gf_bs_write_data(bs, ptr->nameUTF8, len);
			gf_bs_write_u8(bs, 0);
		}
	} else {
		gf_bs_write_u8(bs, 0);
	}
	return GF_OK;
}

GF_Err hdlr_Size(GF_Box *s)
{
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	ptr->size += 20 + 1; //null term or counted string
	if (ptr->nameUTF8) {
		ptr->size += strlen(ptr->nameUTF8);
	}
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
	return gf_isom_box_array_read(s, bs, hinf_AddBox);
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
	return GF_OK;
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
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;

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
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	ptr->size += 16;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hnti_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HintTrackInfoBox, GF_ISOM_BOX_TYPE_HNTI);
	return (GF_Box *)tmp;
}

void hnti_del(GF_Box *a)
{
	gf_free(a);
}

GF_Err hnti_AddBox(GF_Box *s, GF_Box *a)
{
	GF_HintTrackInfoBox *hnti = (GF_HintTrackInfoBox *)s;
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
	return gf_isom_box_add_default(s, a);
}

GF_Err hnti_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, hnti_AddBox, s->type);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hnti_Write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}


GF_Err hnti_Size(GF_Box *s)
{
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
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	//don't count the NULL char!!!
	ptr->size += strlen(ptr->sdpText);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




void rtp_hnti_del(GF_Box *s)
{
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	if (ptr->sdpText) gf_free(ptr->sdpText);
	gf_free(ptr);

}
GF_Err rtp_hnti_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->subType = gf_bs_read_u32(bs);

	length = (u32) (ptr->size);
	//sdp text has no delimiter !!!
	ptr->sdpText = (char*)gf_malloc(sizeof(char) * (length+1));
	if (!ptr->sdpText) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->sdpText, length);
	ptr->sdpText[length] = 0;
	return GF_OK;
}

GF_Box *rtp_hnti_New()
{
	ISOM_DECL_BOX_ALLOC(GF_RTPBox, GF_ISOM_BOX_TYPE_RTP);
	tmp->subType = GF_ISOM_BOX_TYPE_SDP;
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rtp_hnti_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->subType);
	//don't write the NULL char!!!
	gf_bs_write_data(bs, ptr->sdpText, (u32) strlen(ptr->sdpText));
	return GF_OK;
}

GF_Err rtp_hnti_Size(GF_Box *s)
{
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	ptr->size += 4 + strlen(ptr->sdpText);
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

	ISOM_DECREASE_SIZE(ptr, (4+length+1) );
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
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;
	s->size += 4 + 1;
	if (ptr->payloadString) ptr->size += strlen(ptr->payloadString);
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
	ptr->string = (char*)gf_malloc(sizeof(char) * (length+1));
	if (! ptr->string) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->string, length);
	ptr->string[length] = 0;
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
	GF_NameBox *ptr = (GF_NameBox *)s;
	if (ptr->string) ptr->size += strlen(ptr->string) + 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tssy_del(GF_Box *s)
{
	gf_free(s);
}
GF_Err tssy_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeStampSynchronyBox *ptr = (GF_TimeStampSynchronyBox *)s;
	gf_bs_read_int(bs, 6);
	ptr->timestamp_sync = gf_bs_read_int(bs, 2);
	return GF_OK;
}
GF_Box *tssy_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeStampSynchronyBox, GF_ISOM_BOX_TYPE_TSSY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tssy_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TimeStampSynchronyBox *ptr = (GF_TimeStampSynchronyBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, 0, 6);
	gf_bs_write_int(bs, ptr->timestamp_sync, 2);
	return GF_OK;
}
GF_Err tssy_Size(GF_Box *s)
{
	s->size += 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void srpp_del(GF_Box *s)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;
	if (ptr->info) gf_isom_box_del((GF_Box*)ptr->info);
	if (ptr->scheme_type) gf_isom_box_del((GF_Box*)ptr->scheme_type);
	gf_free(s);
}

GF_Err srpp_AddBox(GF_Box *s, GF_Box *a)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_SCHI:
		if (ptr->info) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->info = (GF_SchemeInformationBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SCHM:
		if (ptr->scheme_type) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->scheme_type = (GF_SchemeTypeBox *)a;
		return GF_OK;
	}
	return gf_isom_box_add_default(s, a);
}

GF_Err srpp_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;

	ISOM_DECREASE_SIZE(s, 16)
	ptr->encryption_algorithm_rtp = gf_bs_read_u32(bs);
	ptr->encryption_algorithm_rtcp = gf_bs_read_u32(bs);
	ptr->integrity_algorithm_rtp = gf_bs_read_u32(bs);
	ptr->integrity_algorithm_rtp = gf_bs_read_u32(bs);
	return gf_isom_box_array_read(s, bs, gf_isom_box_add_default);
}
GF_Box *srpp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SRTPProcessBox, GF_ISOM_BOX_TYPE_SRPP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err srpp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->encryption_algorithm_rtp);
	gf_bs_write_u32(bs, ptr->encryption_algorithm_rtcp);
	gf_bs_write_u32(bs, ptr->integrity_algorithm_rtp);
	gf_bs_write_u32(bs, ptr->integrity_algorithm_rtcp);
	if (ptr->info) {
		e = gf_isom_box_write((GF_Box*)ptr->info, bs);
		if (e) return e;
	}
	if (ptr->scheme_type) {
		e = gf_isom_box_write((GF_Box*)ptr->scheme_type, bs);
		if (e) return e;
	}
	return GF_OK;
}
GF_Err srpp_Size(GF_Box *s)
{
	GF_Err e;
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;

	s->size += 16;
	if (ptr->info) {
		e = gf_isom_box_size((GF_Box*)ptr->info);
		if (e) return e;
		ptr->size += ptr->info->size;
	}
	if (ptr->scheme_type) {
		e = gf_isom_box_size((GF_Box*)ptr->scheme_type);
		if (e) return e;
		ptr->size += ptr->scheme_type->size;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void rssr_del(GF_Box *s)
{
	gf_free(s);
}
GF_Err rssr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ReceivedSsrcBox *ptr = (GF_ReceivedSsrcBox *)s;
	ptr->ssrc = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *rssr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ReceivedSsrcBox, GF_ISOM_BOX_TYPE_RSSR);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rssr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ReceivedSsrcBox *ptr = (GF_ReceivedSsrcBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->ssrc);
	return GF_OK;
}
GF_Err rssr_Size(GF_Box *s)
{
	s->size += 4;
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
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;

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
	ptr->bsOffset = gf_bs_get_position(bs);

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
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
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
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;

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
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Media header timescale is 0 - defaulting to 90000\n" ));
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
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;

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
	GF_Err e = gf_isom_box_array_read(s, bs, mdia_AddBox);
	if (e) return e;
	if (!((GF_MediaBox *)s)->information) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MediaInformationBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	if (!((GF_MediaBox *)s)->handler) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing HandlerBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	if (!((GF_MediaBox *)s)->mediaHeader) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MediaHeaderBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
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
	if (ptr->mfro) gf_isom_box_del((GF_Box*)ptr->mfro);
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
	case GF_ISOM_BOX_TYPE_MFRO:
		if (ptr->mfro) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->mfro = (GF_MovieFragmentRandomAccessOffsetBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err mfra_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, mfra_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mfra_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	e = gf_isom_box_array_write(s, ptr->tfra_list, bs);
	if (e) return e;
	if (ptr->mfro) {
		e = gf_isom_box_write((GF_Box *) ptr->mfro, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err mfra_Size(GF_Box *s)
{
	GF_Err e;
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;

	if (ptr->mfro) {
		e = gf_isom_box_size((GF_Box *)ptr->mfro);
		if (e) return e;
		ptr->size += ptr->mfro->size;
	}
	return gf_isom_box_array_size(s, ptr->tfra_list);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


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
	u32 i;
	GF_RandomAccessEntry *p = 0;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	if (ptr->size < 12)
		return GF_ISOM_INVALID_FILE;

	ptr->track_id = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (gf_bs_read_int(bs, 26) != 0)
		return GF_ISOM_INVALID_FILE;

	ptr->traf_bits = (gf_bs_read_int(bs, 2) + 1) * 8;
	ptr->trun_bits = (gf_bs_read_int(bs, 2) + 1) * 8;
	ptr->sample_bits = (gf_bs_read_int(bs, 2) + 1) * 8;
	ISOM_DECREASE_SIZE(ptr, 4);

	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->version == 1) {
		if (ptr->nb_entries > ptr->size / (16+(ptr->traf_bits+ptr->trun_bits+ptr->sample_bits)/8)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in traf\n", ptr->nb_entries));
			return GF_ISOM_INVALID_FILE;
		}
	} else {
		if (ptr->nb_entries > ptr->size / (8+(ptr->traf_bits+ptr->trun_bits+ptr->sample_bits)/8)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in traf\n", ptr->nb_entries));
			return GF_ISOM_INVALID_FILE;
		}
	}

	if (ptr->nb_entries) {
		p = (GF_RandomAccessEntry *) gf_malloc(sizeof(GF_RandomAccessEntry) * ptr->nb_entries);
		if (!p) return GF_OUT_OF_MEM;
	}

	ptr->entries = p;

	for (i=0; i<ptr->nb_entries; i++) {
		memset(p, 0, sizeof(GF_RandomAccessEntry));

		if (ptr->version == 1) {
			p->time = gf_bs_read_u64(bs);
			p->moof_offset = gf_bs_read_u64(bs);
		} else {
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


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tfra_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->track_id);
	gf_bs_write_int(bs, 0, 26);

	gf_bs_write_int(bs, ptr->traf_bits/8 - 1, 2);
	gf_bs_write_int(bs, ptr->trun_bits/8 - 1, 2);
	gf_bs_write_int(bs, ptr->sample_bits/8 - 1, 2);

	gf_bs_write_u32(bs, ptr->nb_entries);

	for (i=0; i<ptr->nb_entries; i++) {
		GF_RandomAccessEntry *p = &ptr->entries[i];
		if (ptr->version==1) {
			gf_bs_write_u64(bs, p->time);
			gf_bs_write_u64(bs, p->moof_offset);
		} else {
			gf_bs_write_u32(bs, (u32) p->time);
			gf_bs_write_u32(bs, (u32) p->moof_offset);
		}
		gf_bs_write_int(bs, p->traf_number, ptr->traf_bits);
		gf_bs_write_int(bs, p->trun_number, ptr->trun_bits);
		gf_bs_write_int(bs, p->sample_number, ptr->sample_bits);
	}
	return GF_OK;
}

GF_Err tfra_Size(GF_Box *s)
{
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	ptr->size += 12;

	ptr->size += ptr->nb_entries * ( ((ptr->version==1) ? 16 : 8 ) + ptr->traf_bits/8 + ptr->trun_bits/8 + ptr->sample_bits/8);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void mfro_del(GF_Box *s)
{
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Box *mfro_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentRandomAccessOffsetBox, GF_ISOM_BOX_TYPE_MFRO);
	return (GF_Box *)tmp;
}

GF_Err mfro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;

	ptr->container_size = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mfro_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->container_size);
	return GF_OK;
}

GF_Err mfro_Size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void elng_del(GF_Box *s)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;
	if (ptr == NULL) return;
	if (ptr->extended_language) gf_free(ptr->extended_language);
	gf_free(ptr);
}

GF_Err elng_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;

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
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;

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
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
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
	s->size += 4;
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
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	GF_Err e;
	e = gf_isom_box_array_read(s, bs, minf_AddBox);
	if (! ptr->dataInformation) {
		GF_Box *dinf, *dref, *url;
		extern Bool use_dump_mode;
		Bool dump_mode = use_dump_mode;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing DataInformationBox\n"));
		//commented on purpose, we are still able to handle the file, we only throw an error but keep processing
//		e = GF_ISOM_INVALID_FILE;

		//add a dinf box to avoid any access to a null dinf
		dinf = gf_isom_box_new(GF_ISOM_BOX_TYPE_DINF);
		if (!dinf) return GF_OUT_OF_MEM;
		if (ptr->InfoHeader && gf_list_find(ptr->other_boxes, ptr->InfoHeader)>=0) dump_mode = GF_TRUE;
		if (ptr->sampleTable && gf_list_find(ptr->other_boxes, ptr->sampleTable)>=0) dump_mode = GF_TRUE;

		ptr->dataInformation = (GF_DataInformationBox *)dinf;

		dref = gf_isom_box_new(GF_ISOM_BOX_TYPE_DREF);
		if (!dref) return GF_OUT_OF_MEM;
		e = dinf_AddBox(dinf, dref);

		url = gf_isom_box_new(GF_ISOM_BOX_TYPE_URL);
		if (!url) return GF_OUT_OF_MEM;
		((GF_FullBox*)url)->flags = 1;
		e = gf_isom_box_add_default(dref, url);

		if (dump_mode) {
			gf_isom_box_add_for_dump_mode((GF_Box*)ptr, (GF_Box*)ptr->dataInformation);
			gf_isom_box_add_for_dump_mode((GF_Box*)dinf, (GF_Box*)dref);
		}
	}
	return e;
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
	return gf_isom_box_array_read(s, bs, moof_AddBox);
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

	if (ptr->mfhd) {
		e = gf_isom_box_size((GF_Box *)ptr->mfhd);
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
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IODS:
		if (ptr->iods) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->iods = (GF_ObjectDescriptorBox *)a;
		//if no IOD, delete the box
		if (!ptr->iods->descriptor) {
			extern Bool use_dump_mode;
			ptr->iods = NULL;

			// don't actually delete in dump mode, it will be done in other_boxes
			if (!use_dump_mode)
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
	GF_Err e;
	e = gf_isom_box_array_read(s, bs, moov_AddBox);
	if (e) {
		return e;
	}
	else {
		if (!((GF_MovieBox *)s)->mvhd) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MovieHeaderBox\n"));
			return GF_ISOM_INVALID_FILE;
		}
	}
	return e;
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
	if (ptr->mvex && !ptr->mvex_after_traks) {
		e = gf_isom_box_write((GF_Box *) ptr->mvex, bs);
		if (e) return e;
	}
#endif

	e = gf_isom_box_array_write(s, ptr->trackList, bs);
	if (e) return e;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->mvex && ptr->mvex_after_traks) {
		e = gf_isom_box_write((GF_Box *) ptr->mvex, bs);
		if (e) return e;
	}
#endif

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

void audio_sample_entry_del(GF_Box *s)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->is_qtff!=2) {
		if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
		if (ptr->cfg_opus) gf_isom_box_del((GF_Box *)ptr->cfg_opus);
		if (ptr->cfg_ac3) gf_isom_box_del((GF_Box *)ptr->cfg_ac3);
		if (ptr->cfg_mha) gf_isom_box_del((GF_Box *)ptr->cfg_mha);
	}
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	if (ptr->cfg_3gpp) gf_isom_box_del((GF_Box *)ptr->cfg_3gpp);
	gf_free(ptr);
}

GF_Err audio_sample_entry_AddBox(GF_Box *s, GF_Box *a)
{
	GF_UnknownBox *wave = NULL;
	Bool drop_wave=GF_FALSE;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->esd = (GF_ESDBox *)a;
		ptr->is_qtff = 0;
		break;
	case GF_ISOM_BOX_TYPE_SINF:
		gf_list_add(ptr->protections, a);
		break;
	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
		ptr->cfg_3gpp = (GF_3GPPConfigBox *) a;
		/*for 3GP config, remember sample entry type in config*/
		ptr->cfg_3gpp->cfg.type = ptr->type;
		ptr->is_qtff = 0;
		break;

	case GF_ISOM_BOX_TYPE_DOPS:
		ptr->cfg_opus = (GF_OpusSpecificBox *)a;
		ptr->is_qtff = 0;
		break;
	case GF_ISOM_BOX_TYPE_DAC3:
		ptr->cfg_ac3 = (GF_AC3ConfigBox *) a;
		ptr->is_qtff = 0;
		break;
	case GF_ISOM_BOX_TYPE_DEC3:
		ptr->cfg_ac3 = (GF_AC3ConfigBox *) a;
		ptr->is_qtff = 0;
		break;
	case GF_ISOM_BOX_TYPE_MHA1:
	case GF_ISOM_BOX_TYPE_MHA2:
	case GF_ISOM_BOX_TYPE_MHM1:
	case GF_ISOM_BOX_TYPE_MHM2:
		ptr->cfg_mha = (GF_MHAConfigBox *) a;
		ptr->is_qtff = 0;
		break;

	case GF_ISOM_BOX_TYPE_UNKNOWN:
		wave = (GF_UnknownBox *)a;
		/*HACK for QT files: get the esds box from the track*/
		if (s->type == GF_ISOM_BOX_TYPE_MP4A) {
			if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)

			//wave subboxes may have been properly parsed
 			if ((wave->original_4cc == GF_QT_BOX_TYPE_WAVE) && gf_list_count(wave->other_boxes)) {
 				u32 i;
                for (i =0; i<gf_list_count(wave->other_boxes); i++) {
                    GF_Box *inner_box = (GF_Box *)gf_list_get(wave->other_boxes, i);
                    if (inner_box->type == GF_ISOM_BOX_TYPE_ESDS) {
                        ptr->esd = (GF_ESDBox *)inner_box;
 						if (ptr->is_qtff & 1<<16) {
                        	gf_list_rem(a->other_boxes, i);
                        	drop_wave=GF_TRUE;
						}
                    }
                }
				if (drop_wave) {
					gf_isom_box_del(a);
                	ptr->is_qtff = 0;
					return GF_OK;
				}
                ptr->is_qtff = 2;
                return gf_isom_box_add_default(s, a);
            }
            gf_isom_box_del(a);
            return GF_ISOM_INVALID_MEDIA;

		}
 		ptr->is_qtff &= ~(1<<16);

 		if ((wave->original_4cc == GF_QT_BOX_TYPE_WAVE) && gf_list_count(wave->other_boxes)) {
			ptr->is_qtff = 2;
		}
		return gf_isom_box_add_default(s, a);
	case GF_QT_BOX_TYPE_WAVE:
	{
		u32 subtype = 0;
		GF_Box **cfg_ptr = NULL;
		if (s->type == GF_ISOM_BOX_TYPE_MP4A) {
			cfg_ptr = (GF_Box **) &ptr->esd;
			subtype = GF_ISOM_BOX_TYPE_ESDS;
		}
		else if (s->type == GF_ISOM_BOX_TYPE_AC3) {
			cfg_ptr = (GF_Box **) &ptr->cfg_ac3;
			subtype = GF_ISOM_BOX_TYPE_DAC3;
		}
		else if (s->type == GF_ISOM_BOX_TYPE_EC3) {
			cfg_ptr = (GF_Box **) &ptr->cfg_ac3;
			subtype = GF_ISOM_BOX_TYPE_DEC3;
		}
		else if (s->type == GF_ISOM_BOX_TYPE_OPUS) {
			cfg_ptr = (GF_Box **) &ptr->cfg_opus;
			subtype = GF_ISOM_BOX_TYPE_DOPS;
		}
		else if ((s->type == GF_ISOM_BOX_TYPE_MHA1) || (s->type == GF_ISOM_BOX_TYPE_MHA2)) {
			cfg_ptr = (GF_Box **) &ptr->cfg_mha;
			subtype = GF_ISOM_BOX_TYPE_MHAC;
		}

		if (cfg_ptr) {
			if (*cfg_ptr) ERROR_ON_DUPLICATED_BOX(a, ptr)

			//wave subboxes may have been properly parsed
 			if (gf_list_count(a->other_boxes)) {
 				u32 i;
                for (i =0; i<gf_list_count(a->other_boxes); i++) {
                    GF_Box *inner_box = (GF_Box *)gf_list_get(a->other_boxes, i);
                    if (inner_box->type == subtype) {
						*cfg_ptr = inner_box;
 						if (ptr->is_qtff & 1<<16) {
                        	gf_list_rem(a->other_boxes, i);
                        	drop_wave=GF_TRUE;
						}
						break;
                    }
                }
				if (drop_wave) {
					gf_isom_box_del(a);
                	ptr->is_qtff = 0;
					return GF_OK;
				}
                ptr->is_qtff = 2;
                return gf_isom_box_add_default(s, a);
            }
		}
	}
 		ptr->is_qtff = 2;
		return gf_isom_box_add_default(s, a);
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}
GF_Err audio_sample_entry_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGAudioSampleEntryBox *ptr;
	char *data;
	u8 a, b, c, d;
	u32 i, size, v, nb_alnum;
	GF_Err e;
	u64 pos, start;

	ptr = (GF_MPEGAudioSampleEntryBox *)s;

	start = gf_bs_get_position(bs);
	gf_bs_seek(bs, start + 8);
	v = gf_bs_read_u16(bs);
	if (v)
		ptr->is_qtff = 1;

	//try to disambiguate QTFF v1 and MP4 v1 audio sample entries ...
	if (v==1) {
		//go to end of ISOM audio sample entry, skip 4 byte (box size field), read 4 bytes (box type) and check if this looks like a box
		gf_bs_seek(bs, start + 8 + 20  + 4);
		a = gf_bs_read_u8(bs);
		b = gf_bs_read_u8(bs);
		c = gf_bs_read_u8(bs);
		d = gf_bs_read_u8(bs);
		nb_alnum = 0;
		if (isalnum(a)) nb_alnum++;
		if (isalnum(b)) nb_alnum++;
		if (isalnum(c)) nb_alnum++;
		if (isalnum(d)) nb_alnum++;
		if (nb_alnum>2) ptr->is_qtff = 0;
	}

	gf_bs_seek(bs, start);
	e = gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox*)s, bs);
	if (e) return e;
	pos = gf_bs_get_position(bs);
	size = (u32) s->size;

	//when cookie is set on bs, always convert qtff-style mp4a to isobmff-style
	//since the conversion is done in addBox and we don't have the bitstream there (arg...), flag the box
 	if (gf_bs_get_cookie(bs)) {
 		ptr->is_qtff |= 1<<16;
 	}

	e = gf_isom_box_array_read(s, bs, audio_sample_entry_AddBox);
	if (!e) return GF_OK;
	if (size<8) return GF_ISOM_INVALID_FILE;

	/*hack for some weird files (possibly recorded with live.com tools, needs further investigations)*/
	gf_bs_seek(bs, pos);
	data = (char*)gf_malloc(sizeof(char) * size);
	gf_bs_read_data(bs, data, size);
	for (i=0; i<size-8; i++) {
		if (GF_4CC((u32)data[i+4], (u8)data[i+5], (u8)data[i+6], (u8)data[i+7]) == GF_ISOM_BOX_TYPE_ESDS) {
			extern Bool use_dump_mode;
			GF_BitStream *mybs = gf_bs_new(data + i, size - i, GF_BITSTREAM_READ);
			if (ptr->esd) {
				if (!use_dump_mode) gf_isom_box_del((GF_Box *)ptr->esd);
				ptr->esd=NULL;
			}

			e = gf_isom_box_parse((GF_Box **)&ptr->esd, mybs);

			if (e==GF_OK) {
				gf_isom_box_add_for_dump_mode((GF_Box*)ptr, (GF_Box*)ptr->esd);
			} else if (ptr->esd) {
				gf_isom_box_del((GF_Box *)ptr->esd);
				ptr->esd=NULL;
			}

			gf_bs_del(mybs);
			break;
		}
	}
	gf_free(data);
	return e;
}

GF_Box *audio_sample_entry_New()
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

GF_Err audio_sample_entry_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);

	if (ptr->is_qtff)
		return gf_isom_box_array_write(s, ptr->protections, bs);

	if (ptr->esd) {
		e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
		if (e) return e;
	}
	if (ptr->cfg_3gpp) {
		e = gf_isom_box_write((GF_Box *)ptr->cfg_3gpp, bs);
		if (e) return e;
	}
	if (ptr->cfg_opus) {
		e = gf_isom_box_write((GF_Box *)ptr->cfg_opus, bs);
		if (e) return e;
	}
	if (ptr->cfg_ac3) {
		e = gf_isom_box_write((GF_Box *)ptr->cfg_ac3, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->protections, bs);
}

GF_Err audio_sample_entry_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;

	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);

	if (ptr->is_qtff)
		return gf_isom_box_array_size(s, ptr->protections);

	if (ptr->esd) {
		e = gf_isom_box_size((GF_Box *)ptr->esd);
		if (e) return e;
		ptr->size += ptr->esd->size;
	}
	if (ptr->cfg_3gpp) {
		e = gf_isom_box_size((GF_Box *)ptr->cfg_3gpp);
		if (e) return e;
		ptr->size += ptr->cfg_3gpp->size;
	}
	if (ptr->cfg_opus) {
		e = gf_isom_box_size((GF_Box *)ptr->cfg_opus);
		if (e) return e;
		ptr->size += ptr->cfg_opus->size;
	}
	if (ptr->cfg_ac3) {
		e = gf_isom_box_size((GF_Box *)ptr->cfg_ac3);
		if (e) return e;
		ptr->size += ptr->cfg_ac3->size;
	}
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gen_sample_entry_del(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(ptr);
}


GF_Err gen_sample_entry_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)s, bs);
	if (e) return e;
	ISOM_DECREASE_SIZE(s, 8);
	return gf_isom_box_array_read(s, bs, gf_isom_box_add_default);
}

GF_Box *gen_sample_entry_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleEntryBox, GF_QT_BOX_TYPE_C608);//type will be overriten
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gen_sample_entry_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	return gf_isom_box_array_write(s, ptr->protections, bs);
}

GF_Err gen_sample_entry_Size(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	ptr->size += 8;
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
	GF_Err e;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(ptr, 8);
	return gf_isom_box_array_read(s, bs, mp4s_AddBox);
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

	ptr->size += 8;
	e = gf_isom_box_size((GF_Box *)ptr->esd);
	if (e) return e;
	ptr->size += ptr->esd->size;
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void video_sample_entry_del(GF_Box *s)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->esd) gf_isom_box_del((GF_Box *)ptr->esd);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	/*for publishing*/
	if (ptr->emul_esd) gf_odf_desc_del((GF_Descriptor *)ptr->emul_esd);

	if (ptr->avc_config) gf_isom_box_del((GF_Box *) ptr->avc_config);
	if (ptr->svc_config) gf_isom_box_del((GF_Box *) ptr->svc_config);
	if (ptr->mvc_config) gf_isom_box_del((GF_Box *) ptr->mvc_config);
	if (ptr->hevc_config) gf_isom_box_del((GF_Box *) ptr->hevc_config);
	if (ptr->lhvc_config) gf_isom_box_del((GF_Box *) ptr->lhvc_config);
	if (ptr->av1_config) gf_isom_box_del((GF_Box *)ptr->av1_config);
	if (ptr->vp_config) gf_isom_box_del((GF_Box *)ptr->vp_config);
	if (ptr->cfg_3gpp) gf_isom_box_del((GF_Box *)ptr->cfg_3gpp);

	if (ptr->descr) gf_isom_box_del((GF_Box *) ptr->descr);
	if (ptr->ipod_ext) gf_isom_box_del((GF_Box *)ptr->ipod_ext);

	if (ptr->pasp) gf_isom_box_del((GF_Box *)ptr->pasp);
	if (ptr->clap) gf_isom_box_del((GF_Box *)ptr->clap);
	if (ptr->colr) gf_isom_box_del((GF_Box*)ptr->colr);
	if (ptr->mdcv) gf_isom_box_del((GF_Box*)ptr->mdcv);
	if (ptr->clli) gf_isom_box_del((GF_Box*)ptr->clli);
	if (ptr->rinf) gf_isom_box_del((GF_Box *)ptr->rinf);
	if (ptr->ccst) gf_isom_box_del((GF_Box *)ptr->ccst);

	if (ptr->rvcc) gf_isom_box_del((GF_Box *)ptr->rvcc);
	if (ptr->auxi) gf_isom_box_del((GF_Box *)ptr->auxi);

	gf_free(ptr);
}

GF_Err video_sample_entry_AddBox(GF_Box *s, GF_Box *a)
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
	case GF_ISOM_BOX_TYPE_RINF:
		if (ptr->rinf) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->rinf = (GF_RestrictedSchemeInfoBox *) a;
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
	case GF_ISOM_BOX_TYPE_MVCC:
		if (ptr->mvc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->mvc_config = (GF_AVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_LHVC:
		if (ptr->lhvc_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->lhvc_config = (GF_HEVCConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_AV1C:
		if (ptr->av1_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->av1_config = (GF_AV1ConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_VPCC:
		if (ptr->vp_config) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->vp_config = (GF_VPConfigurationBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_M4DS:
		if (ptr->descr) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->descr = (GF_MPEG4ExtensionDescriptorsBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_UUID:
		if (! memcmp(((GF_UnknownUUIDBox*)a)->uuid, GF_ISOM_IPOD_EXT, 16)) {
			if (ptr->ipod_ext) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->ipod_ext = (GF_UnknownUUIDBox *)a;
		} else {
			return gf_isom_box_add_default(s, a);
		}
		break;
	case GF_ISOM_BOX_TYPE_D263:
		ptr->cfg_3gpp = (GF_3GPPConfigBox *)a;
		/*for 3GP config, remember sample entry type in config*/
		ptr->cfg_3gpp->cfg.type = ptr->type;
		break;

	case GF_ISOM_BOX_TYPE_PASP:
		if (ptr->pasp) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->pasp = (GF_PixelAspectRatioBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_CLAP:
		if (ptr->clap) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->clap = (GF_CleanApertureBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_COLR:
		if (ptr->colr) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->colr = (GF_ColourInformationBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_MDCV:
		if (ptr->mdcv) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->mdcv = (GF_MasteringDisplayColourVolumeBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_CLLI:
		if (ptr->clli) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->clli = (GF_ContentLightLevelBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_CCST:
		if (ptr->ccst) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->ccst = (GF_CodingConstraintsBox *)a;
		break;
	case GF_ISOM_BOX_TYPE_AUXI:
		if (ptr->auxi) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->auxi = (GF_AuxiliaryTypeInfoBox *)a;
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

GF_Err video_sample_entry_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGVisualSampleEntryBox *mp4v = (GF_MPEGVisualSampleEntryBox*)s;
	GF_Err e;
	e = gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *)s, bs);
	if (e) return e;
	e = gf_isom_box_array_read(s, bs, video_sample_entry_AddBox);
	if (e) return e;
	/*this is an AVC sample desc*/
	if (mp4v->avc_config || mp4v->svc_config || mp4v->mvc_config) AVC_RewriteESDescriptor(mp4v);
	/*this is an HEVC sample desc*/
	if (mp4v->hevc_config || mp4v->lhvc_config || (mp4v->type==GF_ISOM_BOX_TYPE_HVT1))
		HEVC_RewriteESDescriptor(mp4v);
	/*this is an AV1 sample desc*/
	if (mp4v->av1_config)
		AV1_RewriteESDescriptor(mp4v);
	return GF_OK;
}

GF_Box *video_sample_entry_New()
{
	GF_MPEGVisualSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_MPEGVisualSampleEntryBox);
	if (tmp == NULL) return NULL;

	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err video_sample_entry_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)s, bs);

	/*mp4v*/
	if (ptr->esd) {
		e = gf_isom_box_write((GF_Box *)ptr->esd, bs);
		if (e) return e;
	}
	/*mp4v*/
	else if (ptr->cfg_3gpp) {
		e = gf_isom_box_write((GF_Box *)ptr->cfg_3gpp, bs);
		if (e) return e;
	}
	/*avc or hevc or av1*/
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
		if (ptr->mvc_config && ptr->mvc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->mvc_config, bs);
			if (e) return e;
		}
		if (ptr->lhvc_config && ptr->lhvc_config->config) {
			e = gf_isom_box_write((GF_Box *) ptr->lhvc_config, bs);
			if (e) return e;
		}
		if (ptr->av1_config && ptr->av1_config->config) {
			e = gf_isom_box_write((GF_Box *)ptr->av1_config, bs);
			if (e) return e;
		}
		if (ptr->vp_config && ptr->vp_config->config) {
			e = gf_isom_box_write((GF_Box *)ptr->vp_config, bs);
			if (e) return e;
		}
	}
	if (ptr->clap) {
		e = gf_isom_box_write((GF_Box*)ptr->clap, bs);
		if (e) return e;
	}
	if (ptr->pasp) {
		e = gf_isom_box_write((GF_Box *)ptr->pasp, bs);
		if (e) return e;
	}
	if (ptr->colr) {
		e = gf_isom_box_write((GF_Box*)ptr->colr, bs);
		if (e) return e;
	}
	if (ptr->mdcv) {
		e = gf_isom_box_write((GF_Box*)ptr->mdcv, bs);
		if (e) return e;
	}
	if (ptr->clli) {
		e = gf_isom_box_write((GF_Box*)ptr->clli, bs);
		if (e) return e;
	}
	if (ptr->ccst) {
		e = gf_isom_box_write((GF_Box *)ptr->ccst, bs);
		if (e) return e;
	}
	if (ptr->auxi) {
		e = gf_isom_box_write((GF_Box *)ptr->auxi, bs);
		if (e) return e;
	}
	if (ptr->rvcc) {
		e = gf_isom_box_write((GF_Box *)ptr->rvcc, bs);
		if (e) return e;
	}
	if (ptr->rinf) {
		e = gf_isom_box_write((GF_Box *)ptr->rinf, bs);
		if (e) return e;
	}
	return gf_isom_box_array_write(s, ptr->protections, bs);
}

GF_Err video_sample_entry_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;

	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);

	if (ptr->esd) {
		e = gf_isom_box_size((GF_Box *)ptr->esd);
		if (e) return e;
		ptr->size += ptr->esd->size;
	} else if (ptr->cfg_3gpp) {
		e = gf_isom_box_size((GF_Box *)ptr->cfg_3gpp);
		if (e) return e;
		ptr->size += ptr->cfg_3gpp->size;
	} else {
		switch (ptr->type) {
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_AVC2:
		case GF_ISOM_BOX_TYPE_AVC3:
		case GF_ISOM_BOX_TYPE_AVC4:
		case GF_ISOM_BOX_TYPE_SVC1:
		case GF_ISOM_BOX_TYPE_SVC2:
		case GF_ISOM_BOX_TYPE_MVC1:
		case GF_ISOM_BOX_TYPE_MVC2:
			if (!ptr->avc_config && !ptr->svc_config  && !ptr->mvc_config)
				return GF_ISOM_INVALID_FILE;
			break;
		case GF_ISOM_BOX_TYPE_VP08:
		case GF_ISOM_BOX_TYPE_VP09:
			if (!ptr->vp_config) {
				return GF_ISOM_INVALID_FILE;
			}
			break;
		case GF_ISOM_BOX_TYPE_AV01:
			if (!ptr->av1_config) {
				return GF_ISOM_INVALID_FILE;
			}
			break;
		case GF_ISOM_BOX_TYPE_HVC1:
		case GF_ISOM_BOX_TYPE_HEV1:
		case GF_ISOM_BOX_TYPE_HVC2:
		case GF_ISOM_BOX_TYPE_HEV2:
		case GF_ISOM_BOX_TYPE_LHV1:
		case GF_ISOM_BOX_TYPE_LHE1:
		//commented on purpose, HVT1 tracks have no config associated
//		case GF_ISOM_BOX_TYPE_HVT1:
//		case GF_ISOM_BOX_TYPE_HVT2:
			if (!ptr->hevc_config && !ptr->lhvc_config) {
				return GF_ISOM_INVALID_FILE;
			}
			break;
		default:
			break;
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

		if (ptr->mvc_config && ptr->mvc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->mvc_config);
			if (e) return e;
			ptr->size += ptr->mvc_config->size;
		}

		if (ptr->lhvc_config && ptr->lhvc_config->config) {
			e = gf_isom_box_size((GF_Box *) ptr->lhvc_config);
			if (e) return e;
			ptr->size += ptr->lhvc_config->size;
		}

		if (ptr->av1_config && ptr->av1_config->config) {
			e = gf_isom_box_size((GF_Box *)ptr->av1_config);
			if (e) return e;
			ptr->size += ptr->av1_config->size;
		}

		if (ptr->vp_config && ptr->vp_config->config) {
			e = gf_isom_box_size((GF_Box *)ptr->vp_config);
			if (e) return e;
			ptr->size += ptr->vp_config->size;
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
	if (ptr->colr) {
		e = gf_isom_box_size((GF_Box*)ptr->colr);
		if (e) return e;
		ptr->size += ptr->colr->size;
	}
	if (ptr->mdcv) {
		e = gf_isom_box_size((GF_Box*)ptr->mdcv);
		if (e) return e;
		ptr->size += ptr->mdcv->size;
	}
	if (ptr->clli) {
		e = gf_isom_box_size((GF_Box*)ptr->clli);
		if (e) return e;
		ptr->size += ptr->clli->size;
	}
	if (ptr->clap) {
		e = gf_isom_box_size((GF_Box *)ptr->clap);
		if (e) return e;
		ptr->size += ptr->clap->size;
	}
	if (ptr->ccst) {
		e = gf_isom_box_size((GF_Box *)ptr->ccst);
		if (e) return e;
		ptr->size += ptr->ccst->size;
	}
	if (ptr->auxi) {
		e = gf_isom_box_size((GF_Box *)ptr->auxi);
		if (e) return e;
		ptr->size += ptr->auxi->size;
	}
	if (ptr->rvcc) {
		e = gf_isom_box_size((GF_Box *)ptr->rvcc);
		if (e) return e;
		ptr->size += ptr->rvcc->size;
	}
	if (ptr->rinf) {
		e = gf_isom_box_size((GF_Box *)ptr->rinf);
		if (e) return e;
		ptr->size += ptr->rinf->size;
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
	ptr->mehd = NULL;
	ptr->TrackExList = NULL;
	ptr->TrackExPropList = NULL;
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
	return gf_isom_box_array_read(s, bs, mvex_AddBox);
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
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;

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
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;
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
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
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
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr->duration==(u64) -1) ptr->version = 0;
	else ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;

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
	return GF_OK;
}

GF_Box *nmhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGMediaHeaderBox, GF_ISOM_BOX_TYPE_NMHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err nmhd_Write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err nmhd_Size(GF_Box *s)
{
	return GF_OK;
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
	u32 i;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;

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
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;
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
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	ptr->balance = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *smhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SoundMediaHeaderBox, GF_ISOM_BOX_TYPE_SMHD);
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
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;

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
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


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
	if (ptr->sub_samples) gf_isom_box_array_del(ptr->sub_samples);
	if (ptr->sampleGroups) gf_isom_box_array_del(ptr->sampleGroups);
	if (ptr->sampleGroupsDescription) gf_isom_box_array_del(ptr->sampleGroupsDescription);

	if (ptr->sai_sizes) gf_isom_box_array_del(ptr->sai_sizes);
	if (ptr->sai_offsets) gf_isom_box_array_del(ptr->sai_offsets);
	if (ptr->traf_map) {
		if (ptr->traf_map->sample_num) gf_free(ptr->traf_map->sample_num);
		gf_free(ptr->traf_map);
	}

	gf_free(ptr);
}

GF_Err stbl_AddBox(GF_Box *s, GF_Box *a)
{
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
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
			extern Bool use_dump_mode;
			if (!use_dump_mode)
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

	case GF_ISOM_BOX_TYPE_SUBS:
		if (!ptr->sub_samples) ptr->sub_samples = gf_list_new();
		gf_list_add(ptr->sub_samples, a);
		//check subsample box
		{
			GF_SubSampleInformationBox *subs = (GF_SubSampleInformationBox *)a;
			GF_SubSampleInfoEntry *ent = gf_list_get(subs->Samples, 0);
			if (!ent) {
				gf_list_rem(subs->Samples, 0);
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] first entry in SubSample in track SampleTable is invalid\n"));
			}
			else if (ent->sample_delta==0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] first entry in SubSample in track SampleTable has sample_delta of 0, should be one. Fixing\n"));
				ent->sample_delta = 1;
			}
		}
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
	default:
		return gf_isom_box_add_default((GF_Box *)ptr, a);
	}
	return GF_OK;
}




GF_Err stbl_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	//we need to parse DegPrior in a special way
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;

	e = gf_isom_box_array_read(s, bs, stbl_AddBox);
	if (e) return e;

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
	if (ptr->sub_samples) {
		e = gf_isom_box_array_write(s, ptr->sub_samples, bs);
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
	return GF_OK;
}

GF_Err stbl_Size(GF_Box *s)
{
	GF_Err e;
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;

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

	if (ptr->sub_samples) {
		e = gf_isom_box_array_size(s, ptr->sub_samples);
		if (e) return e;
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
	u32 entries;
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;

	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->nb_entries > ptr->size / 4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stco\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

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
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;

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
	u32 entry;
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;

	/*out-of-order stdp, assume no padding at the end and take the entire remaining data for entries*/
	if (!ptr->nb_entries) ptr->nb_entries = (u32) ptr->size / 2;
	else if (ptr->nb_entries > ptr->size / 2) return GF_ISOM_INVALID_FILE;

	ptr->priorities = (u16 *) gf_malloc(ptr->nb_entries * sizeof(u16));
	if (ptr->priorities == NULL) return GF_OUT_OF_MEM;
	for (entry = 0; entry < ptr->nb_entries; entry++) {
		ptr->priorities[entry] = gf_bs_read_u16(bs);
	}
	ISOM_DECREASE_SIZE(ptr, (2*ptr->nb_entries) );
	return GF_OK;
}

GF_Box *stdp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DegradationPriorityBox, GF_ISOM_BOX_TYPE_STDP);
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
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;

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
	u32 i;
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->nb_entries > ptr->size / 12) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsc\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = NULL;
	if (ptr->nb_entries) {
		ptr->entries = gf_malloc(sizeof(GF_StscEntry)*ptr->alloc_size);
		if (!ptr->entries) return GF_OUT_OF_MEM;
	}

	for (i = 0; i < ptr->nb_entries; i++) {
		ptr->entries[i].firstChunk = gf_bs_read_u32(bs);
		ptr->entries[i].samplesPerChunk = gf_bs_read_u32(bs);
		ptr->entries[i].sampleDescriptionIndex = gf_bs_read_u32(bs);
		ptr->entries[i].isEdited = 0;
		ptr->entries[i].nextChunk = 0;
		if (!ptr->entries[i].firstChunk) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] invalid first chunk 0 in stsc entry\n", ptr->nb_entries));
			return GF_ISOM_INVALID_FILE;
		}

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
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

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

GF_Err stsd_AddBox(GF_Box *s, GF_Box *a)
{
	GF_UnknownBox *def;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	if (!a) return GF_OK;

	if (gf_box_valid_in_parent(a, "stsd")) {
		return gf_isom_box_add_default((GF_Box*)ptr, a);
	}
	switch (a->type) {
	//unknown sample description: we need a specific box to handle the data ref index
	//rather than a default box ...
	case GF_ISOM_BOX_TYPE_UNKNOWN:
		def = (GF_UnknownBox *)a;
		/*we need at least 8 bytes for unknown sample entries*/
		if (def->dataSize < 8) {
			gf_isom_box_del(a);
			return GF_ISOM_INVALID_MEDIA;
		}
		return gf_isom_box_add_default((GF_Box*)ptr, a);

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Cannot process box of type %s\n", gf_4cc_to_str(a->type)));
		return GF_ISOM_INVALID_FILE;
	}
}


GF_Err stsd_Read(GF_Box *s, GF_BitStream *bs)
{
	gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(s, 4)

	return gf_isom_box_array_read_ex(s, bs, stsd_AddBox, GF_ISOM_BOX_TYPE_STSD);
}

GF_Box *stsd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleDescriptionBox, GF_ISOM_BOX_TYPE_STSD);
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
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	ptr->size += 4;
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
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;
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
	u32 i;
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;

	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->nb_entries > ptr->size / 4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stss\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

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
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
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
	u32 i, estSize;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	//support for CompactSizes
	if (s->type == GF_ISOM_BOX_TYPE_STSZ) {
		ptr->sampleSize = gf_bs_read_u32(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);

		ISOM_DECREASE_SIZE(ptr, 8);
	} else {
		//24-reserved
		gf_bs_read_int(bs, 24);
		i = gf_bs_read_u8(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 8);
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
			if (ptr->sampleCount > ptr->size / 4) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsz\n", ptr->sampleCount));
				return GF_ISOM_INVALID_FILE;
			}
			ptr->sizes = (u32 *) gf_malloc(ptr->sampleCount * sizeof(u32));
			ptr->alloc_size = ptr->sampleCount;
			if (! ptr->sizes) return GF_OUT_OF_MEM;
			for (i = 0; i < ptr->sampleCount; i++) {
				ptr->sizes[i] = gf_bs_read_u32(bs);
			}
		}
	} else {
		if (ptr->sampleSize==4) {
			if (ptr->sampleCount / 2 > ptr->size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsz\n", ptr->sampleCount));
				return GF_ISOM_INVALID_FILE;
			}
		} else {
			if (ptr->sampleCount > ptr->size / (ptr->sampleSize/8)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsz\n", ptr->sampleCount));
				return GF_ISOM_INVALID_FILE;
			}
		}
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
	u32 i, fieldSize, size;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;

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
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;

#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_LastDTS = 0;
#endif
	ptr->nb_entries = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->nb_entries > ptr->size / 8) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stts\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = gf_malloc(sizeof(GF_SttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		ptr->entries[i].sampleDelta = gf_bs_read_u32(bs);
#ifndef GPAC_DISABLE_ISOM_WRITE
		ptr->w_currentSampleNum += ptr->entries[i].sampleCount;
		ptr->w_LastDTS += (u64)ptr->entries[i].sampleCount * ptr->entries[i].sampleDelta;
#endif

		if (!ptr->entries[i].sampleDelta) {
			if ((i+1<ptr->nb_entries) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Found stts entry with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			} else if (ptr->entries[i].sampleCount>1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] more than one stts entry at the end of the track with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			}
		} else if ((s32) ptr->entries[i].sampleDelta < 0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] stts entry %d has negative duration %d - forbidden ! Fixing to 1, sync may get lost (consider reimport raw media)\n", i, (s32) ptr->entries[i].sampleDelta ));
			ptr->entries[i].sampleDelta = 1;
		}
	}
	if (ptr->size<(ptr->nb_entries*8)) return GF_ISOM_INVALID_FILE;
	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries*8);

	//remove the last sample delta.
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (ptr->nb_entries) ptr->w_LastDTS -= ptr->entries[ptr->nb_entries-1].sampleDelta;
#endif
	return GF_OK;
}

GF_Box *stts_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeToSampleBox, GF_ISOM_BOX_TYPE_STTS);
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
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
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
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;

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
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
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
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;

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
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;

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
	if (ptr->sub_samples) gf_isom_box_array_del(ptr->sub_samples);
	if (ptr->tfdt) gf_isom_box_del((GF_Box *) ptr->tfdt);
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
		if (!ptr->sub_samples) ptr->sub_samples = gf_list_new();
		return gf_list_add(ptr->sub_samples, a);
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
	//we will throw an error if both PIFF_PSEC and SENC are found. Not such files seen yet
	case GF_ISOM_BOX_TYPE_UUID:
		if ( ((GF_UUIDBox *)a)->internal_4cc==GF_ISOM_BOX_UUID_PSEC) {
			if (ptr->sample_encryption) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->sample_encryption = (GF_SampleEncryptionBox *)a;
			ptr->sample_encryption->traf = ptr;
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
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	GF_Err e = gf_isom_box_array_read(s, bs, traf_AddBox);
	if (e) return e;

	if (!ptr->tfhd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing TrackFragmentHeaderBox \n"));
		return GF_ISOM_INVALID_FILE;
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


GF_Box *tfxd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MSSTimeExtBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_TFXD;
	return (GF_Box *)tmp;
}

void tfxd_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err tfxd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MSSTimeExtBox *ptr = (GF_MSSTimeExtBox *)s;
	if (ptr->size<4) return GF_ISOM_INVALID_FILE;
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->version == 0x01) {
		ptr->absolute_time_in_track_timescale = gf_bs_read_u64(bs);
		ptr->fragment_duration_in_track_timescale = gf_bs_read_u64(bs);
	} else {
		ptr->absolute_time_in_track_timescale = gf_bs_read_u32(bs);
		ptr->fragment_duration_in_track_timescale = gf_bs_read_u32(bs);
	}

	return GF_OK;
}

GF_Err tfxd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = GF_OK;
	GF_MSSTimeExtBox *uuid = (GF_MSSTimeExtBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, 1);
	gf_bs_write_u24(bs, 0);
	gf_bs_write_u64(bs, uuid->absolute_time_in_track_timescale);
	gf_bs_write_u64(bs, uuid->fragment_duration_in_track_timescale);

	return GF_OK;
}

GF_Err tfxd_Size(GF_Box *s)
{
	s->size += 20;
	return GF_OK;
}

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
	if (ptr->sub_samples) {
		e = gf_isom_box_array_write(s, ptr->sub_samples, bs);
		if (e) return e;
	}
	if (ptr->tfdt) {
		e = gf_isom_box_write((GF_Box *) ptr->tfdt, bs);
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

	if (ptr->sample_encryption) {
		e = gf_isom_box_write((GF_Box *) ptr->sample_encryption, bs);
		if (e) return e;
	}

	e = gf_isom_box_array_write(s, ptr->TrackRuns, bs);
	if (e) return e;

	//when sdtp is present (smooth-like) write it after the trun box
	if (ptr->sdtp) {
		e = gf_isom_box_write((GF_Box *) ptr->sdtp, bs);
		if (e) return e;
	}
	//tfxd should be last ...
	if (ptr->tfxd) {
		e = gf_isom_box_write((GF_Box *) ptr->tfxd, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err traf_Size(GF_Box *s)
{
	GF_Err e;
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;

	if (ptr->tfhd) {
		e = gf_isom_box_size((GF_Box *) ptr->tfhd);
		if (e) return e;
		ptr->size += ptr->tfhd->size;
	}
	if (ptr->sub_samples) {
		e = gf_isom_box_array_size(s, ptr->sub_samples);
		if (e) return e;
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
	if (ptr->tfxd) {
		e = gf_isom_box_size((GF_Box *)ptr->tfxd);
		if (e) return e;
		s->size += ptr->tfxd->size;
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
	if (ptr->groups) gf_isom_box_del((GF_Box *)ptr->groups);
	if (ptr->Aperture) gf_isom_box_del((GF_Box *)ptr->Aperture);
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
		gf_isom_box_add_for_dump_mode((GF_Box *)trak->Media->information, (GF_Box *)trak->Media->information->sampleTable);
	}

	if (!trak->Media->information->sampleTable->SampleDescription) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no sample description box !\n" ));
		trak->Media->information->sampleTable->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSD);
		gf_isom_box_add_for_dump_mode((GF_Box *)trak->Media->information->sampleTable, (GF_Box *)trak->Media->information->sampleTable->SampleDescription);
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
		case GF_ISOM_BOX_TYPE_RESV:
		case GF_ISOM_SUBTYPE_3GP_AMR:
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_QCELP:
		case GF_ISOM_SUBTYPE_3GP_SMV:
		case GF_ISOM_SUBTYPE_3GP_H263:
		case GF_ISOM_BOX_TYPE_GHNT:
		case GF_ISOM_BOX_TYPE_RTP_STSD:
		case GF_ISOM_BOX_TYPE_SRTP_STSD:
		case GF_ISOM_BOX_TYPE_FDP_STSD:
		case GF_ISOM_BOX_TYPE_RRTP_STSD:
		case GF_ISOM_BOX_TYPE_RTCP_STSD:
		case GF_ISOM_BOX_TYPE_METX:
		case GF_ISOM_BOX_TYPE_METT:
		case GF_ISOM_BOX_TYPE_STXT:
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
		case GF_ISOM_BOX_TYPE_HVT1:
		case GF_ISOM_BOX_TYPE_LHV1:
		case GF_ISOM_BOX_TYPE_LHE1:
		case GF_ISOM_BOX_TYPE_AV01:
		case GF_ISOM_BOX_TYPE_VP08:
		case GF_ISOM_BOX_TYPE_VP09:
		case GF_ISOM_BOX_TYPE_AV1C:
		case GF_ISOM_BOX_TYPE_TX3G:
		case GF_ISOM_BOX_TYPE_TEXT:
		case GF_ISOM_BOX_TYPE_ENCT:
		case GF_ISOM_BOX_TYPE_DIMS:
		case GF_ISOM_BOX_TYPE_OPUS:
		case GF_ISOM_BOX_TYPE_AC3:
		case GF_ISOM_BOX_TYPE_EC3:
		case GF_ISOM_BOX_TYPE_LSR1:
		case GF_ISOM_BOX_TYPE_WVTT:
		case GF_ISOM_BOX_TYPE_STPP:
		case GF_ISOM_BOX_TYPE_SBTT:
		case GF_ISOM_BOX_TYPE_MP3:
		case GF_ISOM_BOX_TYPE_JPEG:
		case GF_ISOM_BOX_TYPE_PNG:
		case GF_ISOM_BOX_TYPE_JP2K:
		case GF_ISOM_BOX_TYPE_MHA1:
		case GF_ISOM_BOX_TYPE_MHA2:
		case GF_ISOM_BOX_TYPE_MHM1:
		case GF_ISOM_BOX_TYPE_MHM2:
		case GF_QT_BOX_TYPE_AUDIO_RAW:
		case GF_QT_BOX_TYPE_AUDIO_TWOS:
		case GF_QT_BOX_TYPE_AUDIO_SOWT:
		case GF_QT_BOX_TYPE_AUDIO_FL32:
		case GF_QT_BOX_TYPE_AUDIO_FL64:
		case GF_QT_BOX_TYPE_AUDIO_IN24:
		case GF_QT_BOX_TYPE_AUDIO_IN32:
		case GF_QT_BOX_TYPE_AUDIO_ULAW:
		case GF_QT_BOX_TYPE_AUDIO_ALAW:
		case GF_QT_BOX_TYPE_AUDIO_ADPCM:
		case GF_QT_BOX_TYPE_AUDIO_IMA_ADPCM:
		case GF_QT_BOX_TYPE_AUDIO_DVCA:
		case GF_QT_BOX_TYPE_AUDIO_QDMC:
		case GF_QT_BOX_TYPE_AUDIO_QDMC2:
		case GF_QT_BOX_TYPE_AUDIO_QCELP:
		case GF_QT_BOX_TYPE_AUDIO_kMP3:
			continue;

		case GF_ISOM_BOX_TYPE_UNKNOWN:
			break;
		default:
			if (gf_box_valid_in_parent((GF_Box *) a, "stsd")) {
				continue;
			}
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Unexpected box %s in stsd!\n", gf_4cc_to_str(a->type)));
			continue;
		}
		//we are sure to have an unknown box here
		assert(a->type==GF_ISOM_BOX_TYPE_UNKNOWN);

		if (!a->data || (a->dataSize<8) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Sample description %s does not have at least 8 bytes!\n", gf_4cc_to_str(a->original_4cc) ));
			continue;
		}
		else if (a->dataSize > a->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Sample description %s has wrong data size %d!\n", gf_4cc_to_str(a->original_4cc), a->dataSize));
			continue;
		}

#define STSD_SWITCH_BOX(_box) \
		if (gf_bs_available(bs)) { \
			u64 pos = gf_bs_get_position(bs); \
			u32 count_subb = 0; \
			GF_Err e;\
			gf_bs_set_cookie(bs, 1);\
			e = gf_isom_box_array_read((GF_Box *) _box, bs, gf_isom_box_add_default); \
			count_subb = _box->other_boxes ? gf_list_count(_box->other_boxes) : 0; \
			if (!count_subb || e) { \
				gf_bs_seek(bs, pos); \
				_box->data_size = (u32) gf_bs_available(bs); \
				if (_box->data_size) { \
					_box->data = a->data; \
					a->data = NULL; \
					memmove(_box->data, _box->data + pos, _box->data_size); \
				} \
			} else { \
				_box->data_size = 0; \
			} \
		} \
		gf_bs_del(bs); \
		if (!_box->data_size && _box->data) { \
			gf_free(_box->data); \
			_box->data = NULL; \
		} \
		_box->size = 0; \
		_box->EntryType = a->original_4cc; \
		gf_list_rem(trak->Media->information->sampleTable->SampleDescription->other_boxes, i-1); \
		gf_isom_box_del((GF_Box *)a); \
		gf_list_insert(trak->Media->information->sampleTable->SampleDescription->other_boxes, _box, i-1); \


		/*only process visual or audio*/
		switch (trak->Media->handler->handlerType) {
        case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		{
			GF_GenericVisualSampleEntryBox *genv = (GF_GenericVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRV);
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			genv->size = a->size-8;
			gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *) genv, bs);

			STSD_SWITCH_BOX(genv)

		}
		break;
		case GF_ISOM_MEDIA_AUDIO:
		{
			GF_GenericAudioSampleEntryBox *gena = (GF_GenericAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRA);
			gena->size = a->size-8;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);
			gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox *) gena, bs);

			STSD_SWITCH_BOX(gena)

		}
		break;

		default:
		{
			GF_Err e;
			GF_GenericSampleEntryBox *genm = (GF_GenericSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
			genm->size = a->size-8;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);

			e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)genm, bs);
			if (e) return;

			STSD_SWITCH_BOX(genm)
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
	case GF_ISOM_BOX_TYPE_TRGR:
		if (ptr->groups) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->groups = (GF_TrackGroupBox *)a;
		return GF_OK;
	case GF_QT_BOX_TYPE_TAPT:
		if (ptr->Aperture) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->Aperture = (GF_Box *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SENC:
		ptr->sample_encryption = (GF_SampleEncryptionBox*)a;
		return gf_isom_box_add_default((GF_Box *)ptr, a);
	case GF_ISOM_BOX_TYPE_UUID:
		if (((GF_UnknownUUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC) {
			ptr->sample_encryption = (GF_SampleEncryptionBox*) a;
			return gf_isom_box_add_default((GF_Box *)ptr, a);
		}

	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err trak_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	e = gf_isom_box_array_read(s, bs, trak_AddBox);
	if (e) return e;
	gf_isom_check_sample_desc(ptr);

	if (!ptr->Header) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing TrackHeaderBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	if (!ptr->Media) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MediaBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	if (!ptr->Media->information || !ptr->Media->information->sampleTable) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid MediaBox\n"));
		return GF_ISOM_INVALID_FILE;
	}

	for (i=0; i<gf_list_count(ptr->Media->information->sampleTable->other_boxes); i++) {
		GF_Box *a = gf_list_get(ptr->Media->information->sampleTable->other_boxes, i);
		if ((a->type ==GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
			ptr->sample_encryption = (struct __sample_encryption_box *) a;
			break;
		}
		else if (a->type == GF_ISOM_BOX_TYPE_SENC) {
			ptr->sample_encryption = (struct __sample_encryption_box *)a;
			break;
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
	if (ptr->Aperture) {
		e = gf_isom_box_write((GF_Box *) ptr->Aperture, bs);
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
	if (ptr->groups) {
		e = gf_isom_box_write((GF_Box *) ptr->groups, bs);
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

	if (ptr->Header) {
		e = gf_isom_box_size((GF_Box *) ptr->Header);
		if (e) return e;
		ptr->size += ptr->Header->size;
	}
	if (ptr->Aperture) {
		e = gf_isom_box_size((GF_Box *) ptr->Aperture);
		if (e) return e;
		ptr->size += ptr->Aperture->size;
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
	if (ptr->groups) {
		e = gf_isom_box_size((GF_Box *) ptr->groups);
		if (e) return e;
		ptr->size += ptr->groups->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stri_del(GF_Box *s)
{
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->attribute_list) gf_free(ptr->attribute_list);
	gf_free(ptr);
}

GF_Err stri_Read(GF_Box *s, GF_BitStream *bs)
{
	size_t i;
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;
	ptr->switch_group = gf_bs_read_u16(bs);
	ptr->alternate_group = gf_bs_read_u16(bs);
	ptr->sub_track_id = gf_bs_read_u32(bs);
	ptr->size -= 8;
	ptr->attribute_count = ptr->size / 4;
	GF_SAFE_ALLOC_N(ptr->attribute_list, (size_t)ptr->attribute_count, u32);
	if (!ptr->attribute_list) return GF_OUT_OF_MEM;
	for (i = 0; i < ptr->attribute_count; i++) {
		ptr->attribute_list[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *stri_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackInformationBox, GF_ISOM_BOX_TYPE_STRI);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stri_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->switch_group);
	gf_bs_write_u16(bs, ptr->alternate_group);
	gf_bs_write_u32(bs, ptr->sub_track_id);
	for (i = 0; i < ptr->attribute_count; i++) {
		gf_bs_write_u32(bs, ptr->attribute_list[i]);
	}
	return GF_OK;
}

GF_Err stri_Size(GF_Box *s)
{
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;

	ptr->size += 8 + 4 * ptr->attribute_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stsg_del(GF_Box *s)
{
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	if (ptr == NULL) return;
	if (ptr->group_description_index) gf_free(ptr->group_description_index);
	gf_free(ptr);
}

GF_Err stsg_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	ISOM_DECREASE_SIZE(s, 6);
	ptr->grouping_type = gf_bs_read_u32(bs);
	ptr->nb_groups = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(s, ptr->nb_groups*4);
	GF_SAFE_ALLOC_N(ptr->group_description_index, ptr->nb_groups, u32);
	if (!ptr->group_description_index) return GF_OUT_OF_MEM;
	for (i = 0; i < ptr->nb_groups; i++) {
		ptr->group_description_index[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *stsg_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackSampleGroupBox, GF_ISOM_BOX_TYPE_STSG);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsg_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->grouping_type);
	gf_bs_write_u16(bs, ptr->nb_groups);
	for (i = 0; i < ptr->nb_groups; i++) {
		gf_bs_write_u32(bs, ptr->group_description_index[i]);
	}
	return GF_OK;
}

GF_Err stsg_Size(GF_Box *s)
{
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	ptr->size += 6 + 4 * ptr->nb_groups;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void strk_del(GF_Box *s)
{
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	if (ptr == NULL) return;
	if (ptr->info) gf_isom_box_del((GF_Box *)ptr->info);
	gf_free(ptr);
}

GF_Err strk_AddBox(GF_Box *s, GF_Box *a)
{
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	if (!a) return GF_OK;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_STRI:
		if (ptr->info) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->info = (GF_SubTrackInformationBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_STRD:
		if (ptr->strd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			ptr->strd = a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}


GF_Err strk_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	e = gf_isom_box_array_read(s, bs, strk_AddBox);
	if (e) return e;

	if (!ptr->info) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing SubTrackInformationBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	return e;
}

GF_Box *strk_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackBox, GF_ISOM_BOX_TYPE_STRK);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err strk_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->info) {
		e = gf_isom_box_write((GF_Box *)ptr->info, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err strk_Size(GF_Box *s)
{
	GF_Err e;
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;

	if (ptr->info) {
		e = gf_isom_box_size((GF_Box *)ptr->info);
		if (e) return e;
		ptr->size += ptr->info->size;
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
	return gf_isom_box_array_read_ex(s, bs, gf_isom_box_add_default, s->type);
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
	return GF_OK;
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
	if (!ptr->trackIDCount) return GF_OK;

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
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	if (!ptr->trackIDCount)
		ptr->size=0;
	else
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
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;

	ptr->trackID = gf_bs_read_u32(bs);
	ptr->def_sample_desc_index = gf_bs_read_u32(bs);
	ptr->def_sample_duration = gf_bs_read_u32(bs);
	ptr->def_sample_size = gf_bs_read_u32(bs);
	ptr->def_sample_flags = gf_bs_read_u32(bs);
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
	//we always write 1 in trex default sample desc as using 0 breaks chrome/opera/...
	gf_bs_write_u32(bs, ptr->def_sample_desc_index ? ptr->def_sample_desc_index : 1);
	gf_bs_write_u32(bs, ptr->def_sample_duration);
	gf_bs_write_u32(bs, ptr->def_sample_size);
	gf_bs_write_u32(bs, ptr->def_sample_flags);
	return GF_OK;
}

GF_Err trex_Size(GF_Box *s)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
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
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;

	ptr->trackID = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	return gf_isom_box_array_read(s, bs, gf_isom_box_add_default);
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
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;
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
	u32 i;
	GF_TrunEntry *p;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;

	//check this is a good file
	if ((ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) && (ptr->flags & GF_ISOM_TRUN_FLAGS))
		return GF_ISOM_INVALID_FILE;

	ptr->sample_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		ptr->data_offset = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
	}
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) {
		ptr->first_sample_flags = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
	}
	if (! (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) ) ) {
		GF_SAFEALLOC(p, GF_TrunEntry);
		p->nb_pack = ptr->sample_count;
		gf_list_add(ptr->entries, p);
		return GF_OK;
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
		ISOM_DECREASE_SIZE(ptr, trun_size);
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

	if (! (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) ) ) {
		return GF_OK;
	}

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
	u32 i, count;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;

	ptr->size += 4;
	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) ptr->size += 4;

	if (! (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) ) ) {
		return GF_OK;
	}

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

GF_Err udta_AddBox(GF_Box *s, GF_Box *a)
{
	GF_Err e;
	u32 box_type;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	if (!ptr) return GF_BAD_PARAM;
	if (!a) return GF_OK;

	/* for unknown udta boxes, we reference them by their original box type */
	box_type = a->type;
	if (box_type == GF_ISOM_BOX_TYPE_UNKNOWN) {
		GF_UnknownBox* unkn = (GF_UnknownBox *)a;
		if (unkn)
			box_type = unkn->original_4cc;
	}

	map = udta_getEntry(ptr, box_type, (a->type==GF_ISOM_BOX_TYPE_UUID) ? & ((GF_UUIDBox *)a)->uuid : NULL);
	if (map == NULL) {
		map = (GF_UserDataMap *) gf_malloc(sizeof(GF_UserDataMap));
		if (map == NULL) return GF_OUT_OF_MEM;
		memset(map, 0, sizeof(GF_UserDataMap));

		map->boxType = box_type;
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
	GF_Err e = gf_isom_box_array_read(s, bs, udta_AddBox);
	if (e) return e;
	if (s->size==4) {
		u32 val = gf_bs_read_u32(bs);
		s->size = 0;
		if (val) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] udta has 4 remaining bytes set to %08X but they should be 0\n", val));
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
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;

	ptr->reserved = gf_bs_read_u64(bs);
	return GF_OK;
}

GF_Box *vmhd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_VideoMediaHeaderBox, GF_ISOM_BOX_TYPE_VMHD);
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
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
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
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;

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
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox *)s;
	ptr->size += 8*ptr->count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *sdtp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleDependencyTypeBox, GF_ISOM_BOX_TYPE_SDTP);
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
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;

	/*out-of-order sdtp, assume no padding at the end*/
	if (!ptr->sampleCount) ptr->sampleCount = (u32) ptr->size;
	else if (ptr->sampleCount > (u32) ptr->size) return GF_ISOM_INVALID_FILE;

	ptr->sample_info = (u8 *) gf_malloc(sizeof(u8)*ptr->sampleCount);
	gf_bs_read_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	ISOM_DECREASE_SIZE(ptr, ptr->sampleCount);
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
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox *)s;
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
	ISOM_DECREASE_SIZE(ptr, 8);
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
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *clap_New()
{
	ISOM_DECL_BOX_ALLOC(GF_CleanApertureBox, GF_ISOM_BOX_TYPE_CLAP);
	return (GF_Box *)tmp;
}


void clap_del(GF_Box *s)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err clap_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox*)s;
	ISOM_DECREASE_SIZE(ptr, 32);
	ptr->cleanApertureWidthN = gf_bs_read_u32(bs);
	ptr->cleanApertureWidthD = gf_bs_read_u32(bs);
	ptr->cleanApertureHeightN = gf_bs_read_u32(bs);
	ptr->cleanApertureHeightD = gf_bs_read_u32(bs);
	ptr->horizOffN = gf_bs_read_u32(bs);
	ptr->horizOffD = gf_bs_read_u32(bs);
	ptr->vertOffN = gf_bs_read_u32(bs);
	ptr->vertOffD = gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clap_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->cleanApertureWidthN);
	gf_bs_write_u32(bs, ptr->cleanApertureWidthD);
	gf_bs_write_u32(bs, ptr->cleanApertureHeightN);
	gf_bs_write_u32(bs, ptr->cleanApertureHeightD);
	gf_bs_write_u32(bs, ptr->horizOffN);
	gf_bs_write_u32(bs, ptr->horizOffD);
	gf_bs_write_u32(bs, ptr->vertOffN);
	gf_bs_write_u32(bs, ptr->vertOffD);
	return GF_OK;
}

GF_Err clap_Size(GF_Box *s)
{
	s->size += 32;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *metx_New()
{
	//type is overridden by the box constructor
	ISOM_DECL_BOX_ALLOC(GF_MetaDataSampleEntryBox, GF_ISOM_BOX_TYPE_METX);
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
	GF_Err e;
	char *str;
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	size = (u32) ptr->size - 8;
	str = gf_malloc(sizeof(char)*size);

	i=0;

	while (size) {
		str[i] = gf_bs_read_u8(bs);
		size--;
		if (!str[i]) {
			i++;
			break;
		}
		i++;
	}
	if (!size && i>1 && str[i-1]) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] metx read invalid string\n"));
		gf_free(str);
		return GF_ISOM_INVALID_FILE;
	}
	if (i>1) {
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
		if (!str[i]) {
			i++;
			break;
		}
		i++;
	}
	if (!size && i>1 && str[i-1]) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] metx read invalid string\n"));
		gf_free(str);
		return GF_ISOM_INVALID_FILE;
	}
	if ((ptr->type==GF_ISOM_BOX_TYPE_METX) || (ptr->type==GF_ISOM_BOX_TYPE_STPP)) {
		if (i>1) {
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
			if (!str[i]) {
				i++;
				break;
			}
			i++;
		}
		if (!size && i>1 && str[i-1]) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] metx read invalid string\n"));
			gf_free(str);
			return GF_ISOM_INVALID_FILE;
		}
		if (i>1) {
			if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
				ptr->mime_type = gf_strdup(str);
			} else {
				ptr->xml_schema_loc = gf_strdup(str);
			}
		}
	}
	//mett, sbtt, stxt, stpp
	else {
		if (i>1) ptr->mime_type = gf_strdup(str);
	}
	ptr->size = size;
	gf_free(str);
	return gf_isom_box_array_read(s, bs, metx_AddBox);
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
	GF_Err e;
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
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
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)s;
	ptr->config = (char *)gf_malloc(sizeof(char)*((u32) ptr->size+1));
	gf_bs_read_data(bs, ptr->config, (u32) ptr->size);
	ptr->config[ptr->size] = 0;
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
	if (ptr->config)
		ptr->size += strlen(ptr->config);
	ptr->size++;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dac3_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AC3ConfigBox, GF_ISOM_BOX_TYPE_DAC3);
	return (GF_Box *)tmp;
}

GF_Box *dec3_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AC3ConfigBox, GF_ISOM_BOX_TYPE_DAC3);
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
	if (!ptr->hdr)
	    return GF_OUT_OF_MEM;
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
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
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
	GF_Err e;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox*)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(ptr, 8);

	return gf_isom_box_array_read(s, bs, lsr1_AddBox);
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
	u32 i;
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;

	ptr->reference_ID = gf_bs_read_u32(bs);
	ptr->timescale = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 8);

	if (ptr->version==0) {
		ptr->earliest_presentation_time = gf_bs_read_u32(bs);
		ptr->first_offset = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 8);
	} else {
		ptr->earliest_presentation_time = gf_bs_read_u64(bs);
		ptr->first_offset = gf_bs_read_u64(bs);
		ISOM_DECREASE_SIZE(ptr, 16);
	}
	gf_bs_read_u16(bs); /* reserved */
	ptr->nb_refs = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	ptr->refs = gf_malloc(sizeof(GF_SIDXReference)*ptr->nb_refs);
	if (!ptr->refs)
	    return GF_OUT_OF_MEM;
	for (i=0; i<ptr->nb_refs; i++) {
		ptr->refs[i].reference_type = gf_bs_read_int(bs, 1);
		ptr->refs[i].reference_size = gf_bs_read_int(bs, 31);
		ptr->refs[i].subsegment_duration = gf_bs_read_u32(bs);
		ptr->refs[i].starts_with_SAP = gf_bs_read_int(bs, 1);
		ptr->refs[i].SAP_type = gf_bs_read_int(bs, 3);
		ptr->refs[i].SAP_delta_time = gf_bs_read_int(bs, 28);

		ISOM_DECREASE_SIZE(ptr, 12);
	}
	return GF_OK;
}

GF_Box *sidx_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SegmentIndexBox, GF_ISOM_BOX_TYPE_SIDX);
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
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;

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

void ssix_del(GF_Box *s)
{
	u32 i;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox *)s;
	if (ptr == NULL) return;
	if (ptr->subsegments) {
		for (i = 0; i < ptr->subsegment_count; i++) {
			GF_SubsegmentInfo *subsegment = &ptr->subsegments[i];
			if (subsegment->ranges) gf_free(subsegment->ranges);
		}
		gf_free(ptr->subsegments);
	}
	gf_free(ptr);
}

GF_Err ssix_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i,j;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox*)s;

	if (ptr->size < 4) return GF_BAD_PARAM;
	ptr->subsegment_count = gf_bs_read_u32(bs);
	ptr->size -= 4;

	if (ptr->subsegment_count > UINT_MAX / sizeof(GF_SubsegmentInfo))
		return GF_ISOM_INVALID_FILE;

	GF_SAFE_ALLOC_N(ptr->subsegments, ptr->subsegment_count, GF_SubsegmentInfo);
	if (!ptr->subsegments)
	    return GF_OUT_OF_MEM;
	for (i = 0; i < ptr->subsegment_count; i++) {
		GF_SubsegmentInfo *subseg = &ptr->subsegments[i];
		if (ptr->size < 4) return GF_BAD_PARAM;
		subseg->range_count = gf_bs_read_u32(bs);
		ptr->size -= 4;
		if (ptr->size < subseg->range_count*4) return GF_BAD_PARAM;
		subseg->ranges = (GF_SubsegmentRangeInfo*) gf_malloc(sizeof(GF_SubsegmentRangeInfo) * subseg->range_count);
		for (j = 0; j < subseg->range_count; j++) {
			subseg->ranges[j].level = gf_bs_read_u8(bs);
			subseg->ranges[j].range_size = gf_bs_read_u24(bs);
			ptr->size -= 4;
		}
	}
	return GF_OK;
}

GF_Box *ssix_New()
{
	ISOM_DECL_BOX_ALLOC(GF_SubsegmentIndexBox, GF_ISOM_BOX_TYPE_SSIX);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ssix_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->subsegment_count);
	for (i = 0; i<ptr->subsegment_count; i++) {
		gf_bs_write_u32(bs, ptr->subsegments[i].range_count);
		for (j = 0; j < ptr->subsegments[i].range_count; j++) {
			gf_bs_write_u8(bs, ptr->subsegments[i].ranges[j].level);
			gf_bs_write_u24(bs, ptr->subsegments[i].ranges[j].range_size);
		}
	}
	return GF_OK;
}

GF_Err ssix_Size(GF_Box *s)
{
	u32 i;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox*)s;

	ptr->size += 4;
	for (i = 0; i < ptr->subsegment_count; i++) {
		ptr->size += 4 + 4 * ptr->subsegments[i].range_count;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void leva_del(GF_Box *s)
{
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox *)s;
	if (ptr == NULL) return;
	if (ptr->levels) gf_free(ptr->levels);
	gf_free(ptr);
}

GF_Err leva_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox*)s;

	if (ptr->size < 4) return GF_BAD_PARAM;
	ptr->level_count = gf_bs_read_u8(bs);
	ptr->size -= 4;
	GF_SAFE_ALLOC_N(ptr->levels, ptr->level_count, GF_LevelAssignment);
	for (i = 0; i < ptr->level_count; i++) {
		GF_LevelAssignment *level = &ptr->levels[i];
		u8 tmp;
		if (ptr->size < 5) return GF_BAD_PARAM;
		level->track_id = gf_bs_read_u32(bs);
		tmp = gf_bs_read_u8(bs);
		level->padding_flag = tmp >> 7;
		level->type = tmp & 0x7F;
		if (level->type == 0) {
			level->grouping_type = gf_bs_read_u32(bs);
		}
		else if (level->type == 1) {
			level->grouping_type = gf_bs_read_u32(bs);
			level->grouping_type_parameter = gf_bs_read_u32(bs);
		}
		else if (level->type == 4) {
			level->sub_track_id = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}

GF_Box *leva_New()
{
	ISOM_DECL_BOX_ALLOC(GF_LevelAssignmentBox, GF_ISOM_BOX_TYPE_LEVA);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err leva_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->level_count);
	for (i = 0; i<ptr->level_count; i++) {
		gf_bs_write_u32(bs, ptr->levels[i].track_id);
		gf_bs_write_u8(bs, ptr->levels[i].padding_flag << 7 | (ptr->levels[i].type & 0x7F));
		if (ptr->levels[i].type == 0) {
			gf_bs_write_u32(bs, ptr->levels[i].grouping_type);
		}
		else if (ptr->levels[i].type == 1) {
			gf_bs_write_u32(bs, ptr->levels[i].grouping_type);
			gf_bs_write_u32(bs, ptr->levels[i].grouping_type_parameter);
		}
		else if (ptr->levels[i].type == 4) {
			gf_bs_write_u32(bs, ptr->levels[i].sub_track_id);
		}
	}
	return GF_OK;
}

GF_Err leva_Size(GF_Box *s)
{
	u32 i;
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox*)s;

	ptr->size += 1;
	for (i = 0; i < ptr->level_count; i++) {
		ptr->size += 5;
		if (ptr->levels[i].type == 0 || ptr->levels[i].type == 4) {
			ptr->size += 4;
		}
		else if (ptr->levels[i].type == 1) {
			ptr->size += 8;
		}
	}
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
	ISOM_DECREASE_SIZE(ptr, 4);

	ptr->pcr_values = gf_malloc(sizeof(u64)*ptr->subsegment_count);
	if (!ptr->pcr_values)
	    return GF_OUT_OF_MEM;
	for (i=0; i<ptr->subsegment_count; i++) {
		u64 data1 = gf_bs_read_u32(bs);
		u64 data2 = gf_bs_read_u16(bs);
		ISOM_DECREASE_SIZE(ptr, 6);
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
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;

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
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) s;
	GF_SubSampleInfoEntry *pSamp;
	u32 entry_count, i;
	u16 subsample_count;

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
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *)s;
	u32 entry_count, i, j;
	u16 subsample_count;

	entry_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	for (i=0; i<entry_count; i++) {
		u32 subs_size=0;
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry*) gf_malloc(sizeof(GF_SubSampleInfoEntry));
		if (!pSamp) return GF_OUT_OF_MEM;

		memset(pSamp, 0, sizeof(GF_SubSampleInfoEntry));
		pSamp->SubSamples = gf_list_new();
		pSamp->sample_delta = gf_bs_read_u32(bs);
		subsample_count = gf_bs_read_u16(bs);
		subs_size=6;

		for (j=0; j<subsample_count; j++) {
			GF_SubSampleEntry *pSubSamp = (GF_SubSampleEntry*) gf_malloc(sizeof(GF_SubSampleEntry));
			if (!pSubSamp) return GF_OUT_OF_MEM;

			memset(pSubSamp, 0, sizeof(GF_SubSampleEntry));
			if (ptr->version==1) {
				pSubSamp->subsample_size = gf_bs_read_u32(bs);
				subs_size+=4;
			} else {
				pSubSamp->subsample_size = gf_bs_read_u16(bs);
				subs_size+=2;
			}
			pSubSamp->subsample_priority = gf_bs_read_u8(bs);
			pSubSamp->discardable = gf_bs_read_u8(bs);
			pSubSamp->reserved = gf_bs_read_u32(bs);
			subs_size+=6;

			gf_list_add(pSamp->SubSamples, pSubSamp);
		}
		gf_list_add(ptr->Samples, pSamp);
		ISOM_DECREASE_SIZE(ptr, subs_size);
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
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;

	if (ptr->version==1) {
		ptr->baseMediaDecodeTime = gf_bs_read_u64(bs);
		ISOM_DECREASE_SIZE(ptr, 8);
	} else {
		ptr->baseMediaDecodeTime = (u32) gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
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
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;

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
	ISOM_DECREASE_SIZE(ptr, 2);
	if (!ptr->predefined_rvc_config) {
		ptr->rvc_meta_idx = gf_bs_read_u16(bs);
		ISOM_DECREASE_SIZE(ptr, 2);
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
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox *)s;
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
	GF_SampleGroupBox *ptr = (GF_SampleGroupBox *)s;

	ptr->grouping_type = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->version==1) {
		ptr->grouping_type_parameter = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
	}
	ptr->entry_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->size < sizeof(GF_SampleGroupEntry)*ptr->entry_count)
	    return GF_ISOM_INVALID_FILE;
	ptr->sample_entries = gf_malloc(sizeof(GF_SampleGroupEntry)*ptr->entry_count);
	if (!ptr->sample_entries)
	    return GF_OUT_OF_MEM;
	if (!ptr->sample_entries) return GF_IO_ERR;
	for (i=0; i<ptr->entry_count; i++) {
		ptr->sample_entries[i].sample_count = gf_bs_read_u32(bs);
		ptr->sample_entries[i].group_description_index = gf_bs_read_u32(bs);

		ISOM_DECREASE_SIZE(ptr, 8);
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
	GF_SampleGroupBox *p = (GF_SampleGroupBox*)s;

	p->size += 8;
	if (p->grouping_type_parameter) p->version=1;

	if (p->version==1) p->size += 4;
	p->size += 8*p->entry_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

static void *sgpd_parse_entry(u32 grouping_type, GF_BitStream *bs, u32 entry_size, u32 *total_bytes)
{
	Bool null_size_ok = GF_FALSE;

	GF_DefaultSampleGroupDescriptionEntry *ptr;
	switch (grouping_type) {
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_PROL:
	{
		GF_RollRecoveryEntry *ptr;
		GF_SAFEALLOC(ptr, GF_RollRecoveryEntry);
		if (!ptr) return NULL;
		ptr->roll_distance = gf_bs_read_int(bs, 16);
		*total_bytes = 2;
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_RAP:
	{
		GF_VisualRandomAccessEntry *ptr;
		GF_SAFEALLOC(ptr, GF_VisualRandomAccessEntry);
		if (!ptr) return NULL;
		ptr->num_leading_samples_known = gf_bs_read_int(bs, 1);
		ptr->num_leading_samples = gf_bs_read_int(bs, 7);
		*total_bytes = 1;
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_SAP:
	{
		GF_SAPEntry *ptr;
		GF_SAFEALLOC(ptr, GF_SAPEntry);
		if (!ptr) return NULL;
		ptr->dependent_flag = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 3);
		ptr->SAP_type = gf_bs_read_int(bs, 4);
		*total_bytes = 1;
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_SYNC:
	{
		GF_SYNCEntry *ptr;
		GF_SAFEALLOC(ptr, GF_SYNCEntry);
		if (!ptr) return NULL;
		gf_bs_read_int(bs, 2);
		ptr->NALU_type = gf_bs_read_int(bs, 6);
		*total_bytes = 1;
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_TELE:
	{
		GF_TemporalLevelEntry *ptr;
		GF_SAFEALLOC(ptr, GF_TemporalLevelEntry);
		if (!ptr) return NULL;
		ptr->level_independently_decodable = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 7);
		*total_bytes = 1;
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_SEIG:
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
			if ((ptr->constant_IV_size != 8) && (ptr->constant_IV_size != 16)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] seig sample group have invalid constant_IV size\n"));
				gf_free(ptr);
				return NULL;
			}
			gf_bs_read_data(bs, (char *)ptr->constant_IV, ptr->constant_IV_size);
			*total_bytes += 1 + ptr->constant_IV_size;
		}
		if (!entry_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] seig sample group does not indicate entry size, deprecated in spec\n"));
		}
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_OINF:
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
	case GF_ISOM_SAMPLE_GROUP_LINF:
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

	case GF_ISOM_SAMPLE_GROUP_TRIF:
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
		break;
	case GF_ISOM_SAMPLE_GROUP_NALM:
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

	case GF_ISOM_SAMPLE_GROUP_TSAS:
	case GF_ISOM_SAMPLE_GROUP_STSA:
		null_size_ok = GF_TRUE;
		break;
	//TODO, add support for these ones ?
	case GF_ISOM_SAMPLE_GROUP_TSCL:
		entry_size = 20;
		break;
	case GF_ISOM_SAMPLE_GROUP_LBLI:
		entry_size = 2;
		break;
	default:
		break;
	}

	if (!entry_size && !null_size_ok) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] %s sample group does not indicate entry size and is not implemented, cannot parse!\n", gf_4cc_to_str( grouping_type) ));
		return NULL;
	}
	GF_SAFEALLOC(ptr, GF_DefaultSampleGroupDescriptionEntry);
	if (!ptr) return NULL;
	if (entry_size) {
		ptr->length = entry_size;
		ptr->data = (u8 *) gf_malloc(sizeof(u8)*ptr->length);
		gf_bs_read_data(bs, (char *) ptr->data, ptr->length);
		*total_bytes = entry_size;
	}
	return ptr;
}

static void	sgpd_del_entry(u32 grouping_type, void *entry)
{
	switch (grouping_type) {
	case GF_ISOM_SAMPLE_GROUP_SYNC:
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_PROL:
	case GF_ISOM_SAMPLE_GROUP_RAP:
	case GF_ISOM_SAMPLE_GROUP_SEIG:
	case GF_ISOM_SAMPLE_GROUP_TELE:
	case GF_ISOM_SAMPLE_GROUP_SAP:
		gf_free(entry);
		return;
	case GF_ISOM_SAMPLE_GROUP_OINF:
		gf_isom_oinf_del_entry(entry);
		return;
	case GF_ISOM_SAMPLE_GROUP_LINF:
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
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_PROL:
		gf_bs_write_int(bs, ((GF_RollRecoveryEntry*)entry)->roll_distance, 16);
		return;
	case GF_ISOM_SAMPLE_GROUP_RAP:
		gf_bs_write_int(bs, ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known, 1);
		gf_bs_write_int(bs, ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples, 7);
		return;
	case GF_ISOM_SAMPLE_GROUP_SAP:
		gf_bs_write_int(bs, ((GF_SAPEntry*)entry)->dependent_flag, 1);
		gf_bs_write_int(bs, 0, 3);
		gf_bs_write_int(bs, ((GF_SAPEntry*)entry)->SAP_type, 4);
		return;
	case GF_ISOM_SAMPLE_GROUP_SYNC:
		gf_bs_write_int(bs, 0, 2);
		gf_bs_write_int(bs, ((GF_SYNCEntry*)entry)->NALU_type, 6);
		return;
	case GF_ISOM_SAMPLE_GROUP_TELE:
		gf_bs_write_int(bs, ((GF_TemporalLevelEntry*)entry)->level_independently_decodable, 1);
		gf_bs_write_int(bs, 0, 7);
		return;
	case GF_ISOM_SAMPLE_GROUP_SEIG:
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
	case GF_ISOM_SAMPLE_GROUP_OINF:
		gf_isom_oinf_write_entry(entry, bs);
		return;
	case GF_ISOM_SAMPLE_GROUP_LINF:
		gf_isom_linf_write_entry(entry, bs);
		return;
	default:
	{
		GF_DefaultSampleGroupDescriptionEntry *ptr = (GF_DefaultSampleGroupDescriptionEntry *)entry;
		if (ptr->length)
			gf_bs_write_data(bs, (char *) ptr->data, ptr->length);
	}
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE
static u32 sgpd_size_entry(u32 grouping_type, void *entry)
{
	switch (grouping_type) {
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_PROL:
		return 2;
	case GF_ISOM_SAMPLE_GROUP_TELE:
	case GF_ISOM_SAMPLE_GROUP_RAP:
	case GF_ISOM_SAMPLE_GROUP_SAP:
	case GF_ISOM_SAMPLE_GROUP_SYNC:
		return 1;
	case GF_ISOM_SAMPLE_GROUP_TSCL:
		return 20;
	case GF_ISOM_SAMPLE_GROUP_LBLI:
		return 2;
	case GF_ISOM_SAMPLE_GROUP_TSAS:
	case GF_ISOM_SAMPLE_GROUP_STSA:
		return 0;
	case GF_ISOM_SAMPLE_GROUP_SEIG:
		return ((((GF_CENCSampleEncryptionGroupEntry *)entry)->IsProtected == 1) && !((GF_CENCSampleEncryptionGroupEntry *)entry)->Per_Sample_IV_size) ? 21 + ((GF_CENCSampleEncryptionGroupEntry *)entry)->constant_IV_size : 20;
	case GF_ISOM_SAMPLE_GROUP_OINF:
		return gf_isom_oinf_size_entry(entry);
	case GF_ISOM_SAMPLE_GROUP_LINF:
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

	p->grouping_type = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(p, 4);

	if (p->version>=1) {
		p->default_length = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(p, 4);
	}
	if (p->version>=2) {
		p->default_description_index = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(p, 4);
	}
	entry_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(p, 4);

	if (entry_count>p->size)
		return GF_ISOM_INVALID_FILE;

	while (entry_count) {
		void *ptr;
		u32 parsed_bytes=0;
		u32 size = p->default_length;
		if ((p->version>=1) && !size) {
			size = gf_bs_read_u32(bs);
			ISOM_DECREASE_SIZE(p, 4);
		}
		ptr = sgpd_parse_entry(p->grouping_type, bs, size, &parsed_bytes);
		//don't return an error, just stop parsing so that we skip over the sgpd box
		if (!ptr) return GF_OK;

		ISOM_DECREASE_SIZE(p, parsed_bytes);

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
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;

	if (ptr->flags & 1) {
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);

		ISOM_DECREASE_SIZE(ptr, 8);
	}
	ptr->default_sample_info_size = gf_bs_read_u8(bs);
	ptr->sample_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 5);

	if (ptr->default_sample_info_size == 0) {
		if (ptr->size < sizeof(u8)*ptr->sample_count)
		    return GF_ISOM_INVALID_FILE;
		ptr->sample_info_size = gf_malloc(sizeof(u8)*ptr->sample_count);
		if (!ptr->sample_info_size)
		    return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, (char *) ptr->sample_info_size, ptr->sample_count);
		ISOM_DECREASE_SIZE(ptr, ptr->sample_count);
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
	GF_SampleAuxiliaryInfoSizeBox *ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;

	if (ptr->aux_info_type || ptr->aux_info_type_parameter) {
		ptr->flags |= 1;
	}
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
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox *)s;

	if (ptr->flags & 1) {
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 8);
	}
	ptr->entry_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->entry_count) {
		u32 i;
		if (ptr->version==0) {
			if (ptr->size < sizeof(u32)*ptr->entry_count)
			    return GF_ISOM_INVALID_FILE;
			ptr->offsets = gf_malloc(sizeof(u32)*ptr->entry_count);
			if (!ptr->offsets)
			    return GF_OUT_OF_MEM;
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets[i] = gf_bs_read_u32(bs);

			ISOM_DECREASE_SIZE(ptr, 4*ptr->entry_count);
		} else {
			if (ptr->size < sizeof(u64)*ptr->entry_count)
			    return GF_ISOM_INVALID_FILE;
			ptr->offsets_large = gf_malloc(sizeof(u64)*ptr->entry_count);
			if (!ptr->offsets_large)
			    return GF_OUT_OF_MEM;
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets_large[i] = gf_bs_read_u64(bs);
			ISOM_DECREASE_SIZE(ptr, 8*ptr->entry_count);
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
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*)s;

	if (ptr->aux_info_type || ptr->aux_info_type_parameter) {
		ptr->flags |= 1;
	}
	if (ptr->offsets_large) {
		ptr->version = 1;
	}

	if (ptr->flags & 1) ptr->size += 8;
	ptr->size += 4;
	//a little optim here: in cenc, the saio always points to a single data block, only one entry is needed
	switch (ptr->aux_info_type) {
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
		if (ptr->offsets_large) gf_free(ptr->offsets_large);
		if (ptr->offsets) gf_free(ptr->offsets);
		ptr->offsets_large = NULL;
		ptr->offsets = NULL;
		ptr->entry_count = 1;
		break;
	}

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
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) s;

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
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err prft_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->refTrackID);
	gf_bs_write_u64(bs, ptr->ntp);
	if (ptr->version==0) {
		gf_bs_write_u32(bs, (u32) ptr->timestamp);
	} else {
		gf_bs_write_u64(bs, ptr->timestamp);
	}

	return GF_OK;
}

GF_Err prft_Size(GF_Box *s)
{
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox*)s;

	ptr->size += 4+8+ (ptr->version ? 8 : 4);
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *trgr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackGroupBox, GF_ISOM_BOX_TYPE_TRGR);
	tmp->groups = gf_list_new();
	if (!tmp->groups) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}

void trgr_del(GF_Box *s)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->groups);
	gf_free(ptr);
}


GF_Err trgr_AddBox(GF_Box *s, GF_Box *a)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;
	return gf_list_add(ptr->groups, a);
}


GF_Err trgr_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, trgr_AddBox, s->type);
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err trgr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return gf_isom_box_array_write(s, ptr->groups, bs);
}

GF_Err trgr_Size(GF_Box *s)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;
	return gf_isom_box_array_size(s, ptr->groups);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *trgt_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackGroupTypeBox, GF_ISOM_BOX_TYPE_TRGT);
	return (GF_Box *)tmp;
}

void trgt_del(GF_Box *s)
{
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err trgt_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *)s;
	ptr->track_group_id = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trgt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *) s;
	if (!s) return GF_BAD_PARAM;
	s->type = ptr->group_type;
	e = gf_isom_full_box_write(s, bs);
	s->type = GF_ISOM_BOX_TYPE_TRGT;
	if (e) return e;
	gf_bs_write_u32(bs, ptr->track_group_id);
	return GF_OK;
}

GF_Err trgt_Size(GF_Box *s)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;

	ptr->size+= 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *stvi_New()
{
	ISOM_DECL_BOX_ALLOC(GF_StereoVideoBox, GF_ISOM_BOX_TYPE_STVI);
	return (GF_Box *)tmp;
}

void stvi_del(GF_Box *s)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;
	if (ptr == NULL) return;
	if (ptr->stereo_indication_type) gf_free(ptr->stereo_indication_type);
	gf_free(ptr);
}

GF_Err stvi_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;

	ISOM_DECREASE_SIZE(ptr, 12);
	gf_bs_read_int(bs, 30);
	ptr->single_view_allowed = gf_bs_read_int(bs, 2);
	ptr->stereo_scheme = gf_bs_read_u32(bs);
	ptr->sit_len = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, ptr->sit_len);
	if (ptr->size < sizeof(char)*ptr->sit_len)
	    return GF_ISOM_INVALID_FILE;
	ptr->stereo_indication_type = gf_malloc(sizeof(char)*ptr->sit_len);
	if (!ptr->stereo_indication_type)
	    return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->stereo_indication_type, ptr->sit_len);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stvi_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, 0, 30);
	gf_bs_write_int(bs, ptr->single_view_allowed, 2);
	gf_bs_write_u32(bs, ptr->stereo_scheme);
	gf_bs_write_u32(bs, ptr->sit_len);
	gf_bs_write_data(bs, ptr->stereo_indication_type, ptr->sit_len);

	return GF_OK;
}

GF_Err stvi_Size(GF_Box *s)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;

	ptr->size+= 12 + ptr->sit_len;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *fiin_New()
{
	ISOM_DECL_BOX_ALLOC(FDItemInformationBox, GF_ISOM_BOX_TYPE_FIIN);
	return (GF_Box *)tmp;
}

void fiin_del(GF_Box *s)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->partition_entries) gf_isom_box_array_del(ptr->partition_entries);
	if (ptr->session_info) gf_isom_box_del((GF_Box*)ptr->session_info);
	if (ptr->group_id_to_name) gf_isom_box_del((GF_Box*)ptr->group_id_to_name);
	gf_free(ptr);
}


GF_Err fiin_AddBox(GF_Box *s, GF_Box *a)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_PAEN:
		if (!ptr->partition_entries) ptr->partition_entries = gf_list_new();
		return gf_list_add(ptr->partition_entries, a);
	case GF_ISOM_BOX_TYPE_SEGR:
		if (ptr->session_info) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->session_info = (FDSessionGroupBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_GITN:
		if (ptr->group_id_to_name) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->group_id_to_name = (GroupIdToNameBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err fiin_Read(GF_Box *s, GF_BitStream *bs)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	gf_bs_read_u16(bs);
	return gf_isom_box_array_read(s, bs, fiin_AddBox);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fiin_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	FDItemInformationBox *ptr = (FDItemInformationBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, gf_list_count(ptr->partition_entries) );
	e = gf_isom_box_array_write(s, ptr->partition_entries, bs);
	if (e) return e;
	if (ptr->session_info) gf_isom_box_write((GF_Box*)ptr->session_info, bs);
	if (ptr->group_id_to_name) gf_isom_box_write((GF_Box*)ptr->group_id_to_name, bs);
	return GF_OK;
}

GF_Err fiin_Size(GF_Box *s)
{
	GF_Err e;
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;

	ptr->size+= 2;
	if (ptr->partition_entries) {
		e = gf_isom_box_array_size(s, ptr->partition_entries);
		if (e) return e;
	}
	if (ptr->session_info) {
		e = gf_isom_box_size((GF_Box *)ptr->session_info);
		if (e) return e;
		ptr->size += ptr->session_info->size;
	}
	if (ptr->group_id_to_name) {
		e = gf_isom_box_size((GF_Box *) ptr->group_id_to_name);
		if (e) return e;
		ptr->size += ptr->group_id_to_name->size;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *paen_New()
{
	ISOM_DECL_BOX_ALLOC(FDPartitionEntryBox, GF_ISOM_BOX_TYPE_PAEN);
	return (GF_Box *)tmp;
}

void paen_del(GF_Box *s)
{
	FDPartitionEntryBox *ptr = (FDPartitionEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->blocks_and_symbols) gf_isom_box_del((GF_Box*)ptr->blocks_and_symbols);
	if (ptr->FEC_symbol_locations) gf_isom_box_del((GF_Box*)ptr->FEC_symbol_locations);
	if (ptr->File_symbol_locations) gf_isom_box_del((GF_Box*)ptr->File_symbol_locations);
	gf_free(ptr);
}


GF_Err paen_AddBox(GF_Box *s, GF_Box *a)
{
	FDPartitionEntryBox *ptr = (FDPartitionEntryBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_FPAR:
		if (ptr->blocks_and_symbols) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->blocks_and_symbols = (FilePartitionBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_FECR:
		if (ptr->FEC_symbol_locations) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->FEC_symbol_locations = (FECReservoirBox *)a;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_FIRE:
		if (ptr->File_symbol_locations) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->File_symbol_locations = (FileReservoirBox *)a;
		return GF_OK;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err paen_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, fiin_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err paen_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	FDPartitionEntryBox *ptr = (FDPartitionEntryBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->blocks_and_symbols) {
		e = gf_isom_box_write((GF_Box *)ptr->blocks_and_symbols, bs);
		if (e) return e;
	}
	if (ptr->FEC_symbol_locations) {
		e = gf_isom_box_write((GF_Box *)ptr->FEC_symbol_locations, bs);
		if (e) return e;
	}
	if (ptr->File_symbol_locations) {
		e = gf_isom_box_write((GF_Box *)ptr->File_symbol_locations, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err paen_Size(GF_Box *s)
{
	GF_Err e;
	FDPartitionEntryBox *ptr = (FDPartitionEntryBox *)s;

	if (ptr->blocks_and_symbols) {
		e = gf_isom_box_size((GF_Box *)ptr->blocks_and_symbols);
		if (e) return e;
		ptr->size += ptr->blocks_and_symbols->size;
	}
	if (ptr->FEC_symbol_locations) {
		e = gf_isom_box_size((GF_Box *) ptr->FEC_symbol_locations);
		if (e) return e;
		ptr->size += ptr->FEC_symbol_locations->size;
	}
	if (ptr->File_symbol_locations) {
		e = gf_isom_box_size((GF_Box *) ptr->File_symbol_locations);
		if (e) return e;
		ptr->size += ptr->File_symbol_locations->size;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *fpar_New()
{
	ISOM_DECL_BOX_ALLOC(FilePartitionBox, GF_ISOM_BOX_TYPE_FPAR);
	return (GF_Box *)tmp;
}

void fpar_del(GF_Box *s)
{
	FilePartitionBox *ptr = (FilePartitionBox *)s;
	if (ptr == NULL) return;
	if (ptr->scheme_specific_info) gf_free(ptr->scheme_specific_info);
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err gf_isom_read_null_terminated_string(GF_Box *s, GF_BitStream *bs, u64 size, char **out_str)
{
	u32 len=10;
	u32 i=0;

	*out_str = gf_malloc(sizeof(char)*len);
	while (1) {
		ISOM_DECREASE_SIZE(s, 1 );
		(*out_str)[i] = gf_bs_read_u8(bs);
		if (!(*out_str)[i]) break;
		i++;
		if (i==len) {
			len += 10;
			*out_str = gf_realloc(*out_str, sizeof(char)*len);
		}
		if (gf_bs_available(bs) == 0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] missing null character in null terminated string\n"));
			(*out_str)[i] = 0;
			return GF_OK;
		}
		if (i >= size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] string bigger than container, probably missing null character\n"));
			(*out_str)[i] = 0;
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err fpar_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	FilePartitionBox *ptr = (FilePartitionBox *)s;

	ISOM_DECREASE_SIZE(ptr, ((ptr->version ? 4 : 2) + 12) );
	ptr->itemID = gf_bs_read_int(bs, ptr->version ? 32 : 16);
	ptr->packet_payload_size = gf_bs_read_u16(bs);
	gf_bs_read_u8(bs);
	ptr->FEC_encoding_ID = gf_bs_read_u8(bs);
	ptr->FEC_instance_ID = gf_bs_read_u16(bs);
	ptr->max_source_block_length = gf_bs_read_u16(bs);
	ptr->encoding_symbol_length = gf_bs_read_u16(bs);
	ptr->max_number_of_encoding_symbols = gf_bs_read_u16(bs);

	e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->scheme_specific_info);
	if (e) return e;

	ISOM_DECREASE_SIZE(ptr, (ptr->version ? 4 : 2) );
	ptr->nb_entries = gf_bs_read_int(bs, ptr->version ? 32 : 16);
	if (ptr->nb_entries > UINT_MAX / 6)
		return GF_ISOM_INVALID_FILE;
	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries * 6 );
	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, FilePartitionEntry);
	for (i=0;i < ptr->nb_entries; i++) {
		ptr->entries[i].block_count = gf_bs_read_u16(bs);
		ptr->entries[i].block_size = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fpar_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	FilePartitionBox *ptr = (FilePartitionBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->itemID, ptr->version ? 32 : 16);
	gf_bs_write_u16(bs, ptr->packet_payload_size);
	gf_bs_write_u8(bs, 0);
	gf_bs_write_u8(bs, ptr->FEC_encoding_ID);
	gf_bs_write_u16(bs, ptr->FEC_instance_ID);
	gf_bs_write_u16(bs, ptr->max_source_block_length);
	gf_bs_write_u16(bs, ptr->encoding_symbol_length);
	gf_bs_write_u16(bs, ptr->max_number_of_encoding_symbols);
	if (ptr->scheme_specific_info) {
		gf_bs_write_data(bs, ptr->scheme_specific_info, (u32)strlen(ptr->scheme_specific_info) );
	}
	//null terminated string
	gf_bs_write_u8(bs, 0);

	gf_bs_write_int(bs, ptr->nb_entries, ptr->version ? 32 : 16);

	for (i=0;i < ptr->nb_entries; i++) {
		gf_bs_write_u16(bs, ptr->entries[i].block_count);
		gf_bs_write_u32(bs, ptr->entries[i].block_size);
	}
	return GF_OK;
}

GF_Err fpar_Size(GF_Box *s)
{
	FilePartitionBox *ptr = (FilePartitionBox *)s;

	ptr->size += 13 + (ptr->version ? 8 : 4);
	if (ptr->scheme_specific_info)
		ptr->size += strlen(ptr->scheme_specific_info);

	ptr->size+= ptr->nb_entries * 6;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *fecr_New()
{
	ISOM_DECL_BOX_ALLOC(FECReservoirBox, GF_ISOM_BOX_TYPE_FECR);
	return (GF_Box *)tmp;
}

void fecr_del(GF_Box *s)
{
	FECReservoirBox *ptr = (FECReservoirBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err fecr_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	FECReservoirBox *ptr = (FECReservoirBox *)s;

	ISOM_DECREASE_SIZE(ptr, (ptr->version ? 4 : 2) );
	ptr->nb_entries = gf_bs_read_int(bs, ptr->version ? 32 : 16);

	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries * (ptr->version ? 8 : 6) );
	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, FECReservoirEntry);
	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].item_id = gf_bs_read_int(bs, ptr->version ? 32 : 16);
		ptr->entries[i].symbol_count = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fecr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	FECReservoirBox *ptr = (FECReservoirBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->nb_entries, ptr->version ? 32 : 16);
	for (i=0; i<ptr->nb_entries; i++) {
		gf_bs_write_int(bs, ptr->entries[i].item_id, ptr->version ? 32 : 16);
		gf_bs_write_u32(bs, ptr->entries[i].symbol_count);
	}
	return GF_OK;
}

GF_Err fecr_Size(GF_Box *s)
{
	FECReservoirBox *ptr = (FECReservoirBox *)s;
	ptr->size += (ptr->version ? 4 : 2) +  ptr->nb_entries * (ptr->version ? 8 : 6);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *segr_New()
{
	ISOM_DECL_BOX_ALLOC(FDSessionGroupBox, GF_ISOM_BOX_TYPE_SEGR);
	return (GF_Box *)tmp;
}

void segr_del(GF_Box *s)
{
	u32 i;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *)s;
	if (ptr == NULL) return;
	for (i=0; i<ptr->num_session_groups; i++) {
		if (ptr->session_groups[i].group_ids) gf_free(ptr->session_groups[i].group_ids);
		if (ptr->session_groups[i].channels) gf_free(ptr->session_groups[i].channels);
	}
	if (ptr->session_groups) gf_free(ptr->session_groups);
	gf_free(ptr);
}

GF_Err segr_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, k;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->num_session_groups = gf_bs_read_u16(bs);
	if (ptr->num_session_groups*3>ptr->size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in segr\n", ptr->num_session_groups));
		ptr->num_session_groups = 0;
		return GF_ISOM_INVALID_FILE;
	}

	GF_SAFE_ALLOC_N(ptr->session_groups, ptr->num_session_groups, SessionGroupEntry);
	for (i=0; i<ptr->num_session_groups; i++) {
		ptr->session_groups[i].nb_groups = gf_bs_read_u8(bs);
		ISOM_DECREASE_SIZE(ptr, 1);
		GF_SAFE_ALLOC_N(ptr->session_groups[i].group_ids, ptr->session_groups[i].nb_groups, u32);
		for (k=0; k<ptr->session_groups[i].nb_groups; k++) {
			ISOM_DECREASE_SIZE(ptr, 4);
			ptr->session_groups[i].group_ids[k] = gf_bs_read_u32(bs);
		}

		ptr->session_groups[i].nb_channels = gf_bs_read_u16(bs);
		GF_SAFE_ALLOC_N(ptr->session_groups[i].channels, ptr->session_groups[i].nb_channels, u32);
		for (k=0; k<ptr->session_groups[i].nb_channels; k++) {
			ISOM_DECREASE_SIZE(ptr, 4);
			ptr->session_groups[i].channels[k] = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err segr_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 i, k;
	GF_Err e;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->num_session_groups);
	for (i=0; i<ptr->num_session_groups; i++) {
		gf_bs_write_u8(bs, ptr->session_groups[i].nb_groups);
		for (k=0; k<ptr->session_groups[i].nb_groups; k++) {
			gf_bs_write_u32(bs, ptr->session_groups[i].group_ids[k]);
		}

		gf_bs_write_u16(bs, ptr->session_groups[i].nb_channels);
		for (k=0; k<ptr->session_groups[i].nb_channels; k++) {
			gf_bs_write_u32(bs, ptr->session_groups[i].channels[k]);
		}
	}
	return GF_OK;
}

GF_Err segr_Size(GF_Box *s)
{
	u32 i;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *)s;

	ptr->size += 2;

	for (i=0; i<ptr->num_session_groups; i++) {
		ptr->size += 1 + 4*ptr->session_groups[i].nb_groups;
		ptr->size += 2 + 4*ptr->session_groups[i].nb_channels;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *gitn_New()
{
	ISOM_DECL_BOX_ALLOC(GroupIdToNameBox, GF_ISOM_BOX_TYPE_GITN);
	return (GF_Box *)tmp;
}

void gitn_del(GF_Box *s)
{
	u32 i;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *)s;
	if (ptr == NULL) return;
	for (i=0; i<ptr->nb_entries; i++) {
		if (ptr->entries[i].name) gf_free(ptr->entries[i].name);
	}
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err gitn_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->nb_entries = gf_bs_read_u16(bs);

	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, GroupIdNameEntry);
	for (i=0; i<ptr->nb_entries; i++) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->entries[i].group_id = gf_bs_read_u32(bs);

		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->entries[i].name);
		if (e) return e;
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gitn_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->nb_entries);
	for (i=0; i<ptr->nb_entries; i++) {
		gf_bs_write_u32(bs, ptr->entries[i].group_id);
		if (ptr->entries[i].name) gf_bs_write_data(bs, ptr->entries[i].name, (u32)strlen(ptr->entries[i].name) );
		gf_bs_write_u8(bs, 0);
	}
	return GF_OK;
}

GF_Err gitn_Size(GF_Box *s)
{
	u32 i;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *)s;
	ptr->size += 2;

	for (i=0; i<ptr->nb_entries; i++) {
		ptr->size += 5;
		if (ptr->entries[i].name) ptr->size += strlen(ptr->entries[i].name);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_HINTING

GF_Box *fdpa_New()
{
	ISOM_DECL_BOX_ALLOC(GF_FDpacketBox, GF_ISOM_BOX_TYPE_FDPA);
	return (GF_Box *)tmp;
}

void fdpa_del(GF_Box *s)
{
	u32 i;
	GF_FDpacketBox *ptr = (GF_FDpacketBox *)s;
	if (ptr == NULL) return;

	if (ptr->headers) {
		for (i=0; i<ptr->header_ext_count; i++) {
			if (ptr->headers[i].data) gf_free(ptr->headers[i].data);
		}
		gf_free(ptr->headers);
	}
	gf_free(ptr);
}

GF_Err fdpa_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_FDpacketBox *ptr = (GF_FDpacketBox *)s;

	ISOM_DECREASE_SIZE(ptr, 3);
	ptr->info.sender_current_time_present = gf_bs_read_int(bs, 1);
	ptr->info.expected_residual_time_present = gf_bs_read_int(bs, 1);
	ptr->info.session_close_bit = gf_bs_read_int(bs, 1);
	ptr->info.object_close_bit = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 4);
	ptr->info.transport_object_identifier = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->header_ext_count = gf_bs_read_u16(bs);
	if (ptr->header_ext_count*2>ptr->size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in fdpa\n", ptr->header_ext_count));
		return GF_ISOM_INVALID_FILE;
	}

	GF_SAFE_ALLOC_N(ptr->headers, ptr->header_ext_count, GF_LCTheaderExtension);
	for (i=0; i<ptr->header_ext_count; i++) {
		ptr->headers[i].header_extension_type = gf_bs_read_u8(bs);
		ISOM_DECREASE_SIZE(ptr, 1);

		if (ptr->headers[i].header_extension_type > 127) {
			gf_bs_read_data(bs, (char *) ptr->headers[i].content, 3);
		} else {
			ISOM_DECREASE_SIZE(ptr, 1);
			ptr->headers[i].data_length = gf_bs_read_u8(bs);
			if (ptr->headers[i].data_length) {
				ptr->headers[i].data_length = 4*ptr->headers[i].data_length - 2;
				if (ptr->size < sizeof(char) * ptr->headers[i].data_length)
				    return GF_ISOM_INVALID_FILE;
				ptr->headers[i].data = gf_malloc(sizeof(char) * ptr->headers[i].data_length);
				if (!ptr->headers[i].data)
				    return GF_OUT_OF_MEM;
				gf_bs_read_data(bs, ptr->headers[i].data, ptr->headers[i].data_length);
			}
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fdpa_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_FDpacketBox *ptr = (GF_FDpacketBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->info.sender_current_time_present, 1);
	gf_bs_write_int(bs, ptr->info.expected_residual_time_present, 1);
	gf_bs_write_int(bs, ptr->info.session_close_bit, 1);
	gf_bs_write_int(bs, ptr->info.object_close_bit, 1);
	gf_bs_write_int(bs, 0, 4);
	ptr->info.transport_object_identifier = gf_bs_read_u16(bs);
	gf_bs_write_u16(bs, ptr->header_ext_count);
	for (i=0; i<ptr->header_ext_count; i++) {
		gf_bs_write_u8(bs, ptr->headers[i].header_extension_type);
		if (ptr->headers[i].header_extension_type > 127) {
			gf_bs_write_data(bs, (const char *) ptr->headers[i].content, 3);
		} else {
			gf_bs_write_u8(bs, ptr->headers[i].data_length ? (ptr->headers[i].data_length+2)/4 : 0);
			if (ptr->headers[i].data_length) {
				gf_bs_write_data(bs, ptr->headers[i].data, ptr->headers[i].data_length);
			}
		}
	}
	return GF_OK;
}

GF_Err fdpa_Size(GF_Box *s)
{
	u32 i;
	GF_FDpacketBox *ptr = (GF_FDpacketBox *)s;

	ptr->size += 5;

	for (i=0; i<ptr->header_ext_count; i++) {
		ptr->size += 1;
		if (ptr->headers[i].header_extension_type > 127) {
			ptr->size += 3;
		} else {
			ptr->size += 1 + ptr->headers[i].data_length;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *extr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ExtraDataBox, GF_ISOM_BOX_TYPE_EXTR);
	return (GF_Box *)tmp;
}

void extr_del(GF_Box *s)
{
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *)s;
	if (ptr == NULL) return;
	if (ptr->feci) gf_isom_box_del((GF_Box*)ptr->feci);

	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Err extr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *)s;

	e = gf_isom_box_parse((GF_Box**) &ptr->feci, bs);
	if (e) return e;
	if (!ptr->feci || ptr->feci->size > ptr->size) return GF_ISOM_INVALID_MEDIA;
	ptr->data_length = (u32) (ptr->size - ptr->feci->size);
	ptr->data = gf_malloc(sizeof(char)*ptr->data_length);
	if (!ptr->data)
	    return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->data, ptr->data_length);

	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err extr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (ptr->feci) {
		e = gf_isom_box_write((GF_Box *)ptr->feci, bs);
		if (e) return e;
	}
	gf_bs_write_data(bs, ptr->data, ptr->data_length);
	return GF_OK;
}

GF_Err extr_Size(GF_Box *s)
{
	GF_Err e;
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *) s;

	if (ptr->feci) {
		e = gf_isom_box_size((GF_Box *)ptr->feci);
		if (e) return e;
		ptr->size += ptr->feci->size;
	}
	ptr->size += ptr->data_length;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *fdsa_New()
{
	ISOM_DECL_BOX_ALLOC(GF_HintSample, GF_ISOM_BOX_TYPE_FDSA);
	if (!tmp) return NULL;
	tmp->packetTable = gf_list_new();
	tmp->hint_subtype = GF_ISOM_BOX_TYPE_FDP_STSD;
	return (GF_Box*)tmp;
}

void fdsa_del(GF_Box *s)
{
	GF_HintSample *ptr = (GF_HintSample *)s;
	gf_isom_box_array_del(ptr->packetTable);
	if (ptr->extra_data) gf_isom_box_del((GF_Box*)ptr->extra_data);
	gf_free(ptr);
}

GF_Err fdsa_AddBox(GF_Box *s, GF_Box *a)
{
	GF_HintSample *ptr = (GF_HintSample *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_FDPA:
		gf_list_add(ptr->packetTable, a);
		break;
	case GF_ISOM_BOX_TYPE_EXTR:
		if (ptr->extra_data) ERROR_ON_DUPLICATED_BOX(a, ptr)
		ptr->extra_data = (GF_ExtraDataBox*)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}
GF_Err fdsa_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, fdsa_AddBox);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fdsa_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HintSample *ptr = (GF_HintSample *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	e = gf_isom_box_array_write(s, ptr->packetTable, bs);
	if (e) return e;
	if (ptr->extra_data) {
		e = gf_isom_box_write((GF_Box *)ptr->extra_data, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err fdsa_Size(GF_Box *s)
{
	GF_HintSample *ptr = (GF_HintSample*)s;
	GF_Err e;

	 if (ptr->extra_data) {
		e = gf_isom_box_size((GF_Box *)ptr->extra_data);
		if (e) return e;
		ptr->size += ptr->extra_data->size;
	}
	return gf_isom_box_array_size(s, ptr->packetTable);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_HINTING*/


void trik_del(GF_Box *s)
{
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err trik_Read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	ptr->entry_count = (u32) ptr->size;
	ptr->entries = (GF_TrickPlayBoxEntry *) gf_malloc(ptr->entry_count * sizeof(GF_TrickPlayBoxEntry) );
	if (ptr->entries == NULL) return GF_OUT_OF_MEM;

	for (i=0; i< ptr->entry_count; i++) {
		ptr->entries[i].pic_type = gf_bs_read_int(bs, 2);
		ptr->entries[i].dependency_level = gf_bs_read_int(bs, 6);
	}
	return GF_OK;
}

GF_Box *trik_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrickPlayBox, GF_ISOM_BOX_TYPE_TRIK);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trik_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	for (i=0; i < ptr->entry_count; i++ ) {
		gf_bs_write_int(bs, ptr->entries[i].pic_type, 2);
		gf_bs_write_int(bs, ptr->entries[i].dependency_level, 6);
	}
	return GF_OK;
}

GF_Err trik_Size(GF_Box *s)
{
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	ptr->size += 8 * ptr->entry_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void bloc_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err bloc_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_BaseLocationBox *ptr = (GF_BaseLocationBox *) s;

	ISOM_DECREASE_SIZE(s, 256)
	gf_bs_read_data(bs, (char *) ptr->baseLocation, 256);
	ISOM_DECREASE_SIZE(s, 256)
	gf_bs_read_data(bs, (char *) ptr->basePurlLocation, 256);
	ISOM_DECREASE_SIZE(s, 512)
	gf_bs_skip_bytes(bs, 512);
	return GF_OK;
}

GF_Box *bloc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_BaseLocationBox, GF_ISOM_BOX_TYPE_TRIK);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err bloc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_BaseLocationBox *ptr = (GF_BaseLocationBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, (const char *) ptr->baseLocation, 256);
	gf_bs_write_data(bs, (const char *) ptr->basePurlLocation, 256);
	for (i=0; i < 64; i++ ) {
		gf_bs_write_u64(bs, 0);
	}
	return GF_OK;
}

GF_Err bloc_Size(GF_Box *s)
{
	s->size += 1024;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ainf_del(GF_Box *s)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;
	if (ptr->APID) gf_free(ptr->APID);
	gf_free(s);
}

GF_Err ainf_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;

	ISOM_DECREASE_SIZE(s, 4)
	ptr->profile_version = gf_bs_read_u32(bs);
	return gf_isom_read_null_terminated_string(s, bs, s->size, &ptr->APID);
}

GF_Box *ainf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_AssetInformationBox, GF_ISOM_BOX_TYPE_AINF);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ainf_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->profile_version);
	gf_bs_write_data(bs, ptr->APID, (u32) strlen(ptr->APID) + 1);
	return GF_OK;
}

GF_Err ainf_Size(GF_Box *s)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;
	s->size += 4 +  strlen(ptr->APID) + 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void mhac_del(GF_Box *s)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;
	if (ptr->mha_config) gf_free(ptr->mha_config);
	gf_free(s);
}

GF_Err mhac_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;

	ISOM_DECREASE_SIZE(s, 5)
	ptr->configuration_version = gf_bs_read_u8(bs);
	ptr->mha_pl_indication = gf_bs_read_u8(bs);
	ptr->reference_channel_layout = gf_bs_read_u8(bs);
	ptr->mha_config_size = gf_bs_read_u16(bs);
	if (ptr->mha_config_size) {
		ISOM_DECREASE_SIZE(s, ptr->mha_config_size)
		if (ptr->size < sizeof(char)*ptr->mha_config_size)
		    return GF_ISOM_INVALID_FILE;
		ptr->mha_config = gf_malloc(sizeof(char)*ptr->mha_config_size);
		if (!ptr->mha_config)
		    return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->mha_config, ptr->mha_config_size);
	}
	return GF_OK;
}

GF_Box *mhac_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MHAConfigBox, GF_ISOM_BOX_TYPE_MHAC);
	tmp->configuration_version = 1;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mhac_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;

	e = gf_isom_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->configuration_version);
	gf_bs_write_u8(bs, ptr->mha_pl_indication);
	gf_bs_write_u8(bs, ptr->reference_channel_layout);
	gf_bs_write_u16(bs, ptr->mha_config ? ptr->mha_config_size : 0);
	if (ptr->mha_config && ptr->mha_config_size)
		gf_bs_write_data(bs, ptr->mha_config, ptr->mha_config_size);

	return GF_OK;
}

GF_Err mhac_Size(GF_Box *s)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;
	s->size += 5;
	if (ptr->mha_config_size && ptr->mha_config) s->size += ptr->mha_config_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* Dolby Vision */

GF_Box *dvcC_New()
{
	GF_DOVIConfigurationBox *tmp = (GF_DOVIConfigurationBox *)gf_malloc(sizeof(GF_DOVIConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DOVIConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_DVCC;
	return (GF_Box *)tmp;
}

void dvcC_del(GF_Box *s)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox*)s;
	gf_free(ptr);
}

GF_Err dvcC_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox *)s;

	//GF_DOVIDecoderConfigurationRecord
	ptr->DOVIConfig.dv_version_major = gf_bs_read_u8(bs);
	ptr->DOVIConfig.dv_version_minor = gf_bs_read_u8(bs);
	ptr->DOVIConfig.dv_profile = gf_bs_read_int(bs, 7);
	ptr->DOVIConfig.dv_level = gf_bs_read_int(bs, 6);
	ptr->DOVIConfig.rpu_present_flag = gf_bs_read_int(bs, 1);
	ptr->DOVIConfig.el_present_flag = gf_bs_read_int(bs, 1);
	ptr->DOVIConfig.bl_present_flag = gf_bs_read_int(bs, 1);
	{
		int i = 0;
		u32 data[5];
		memset(data, 0, sizeof(data));
		gf_bs_read_data(bs, (char*)data, 20);
		for (i = 0; i < 5; ++i) {
			if (data[i] != 0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] dvcC reserved bytes are not zero\n"));
				//return GF_ISOM_INVALID_FILE;
			}
		}
	}

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dvcC_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	//GF_DOVIDecoderConfigurationRecord
	gf_bs_write_u8(bs,  ptr->DOVIConfig.dv_version_major);
	gf_bs_write_u8(bs,  ptr->DOVIConfig.dv_version_minor);
	gf_bs_write_int(bs, ptr->DOVIConfig.dv_profile, 7);
	gf_bs_write_int(bs, ptr->DOVIConfig.dv_level, 6);
	gf_bs_write_int(bs, ptr->DOVIConfig.rpu_present_flag, 1);
	gf_bs_write_int(bs, ptr->DOVIConfig.el_present_flag, 1);
	gf_bs_write_int(bs, ptr->DOVIConfig.bl_present_flag, 1);
	{
		u32 data[5];
		memset(data, 0, sizeof(data));
		gf_bs_write_data(bs, (char*)data, sizeof(data));
	}

	return GF_OK;
}

GF_Err dvcC_Size(GF_Box *s)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox *)s;

	ptr->size += 24;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
