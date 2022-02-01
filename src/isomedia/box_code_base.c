/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

void co64_box_del(GF_Box *s)
{
	GF_ChunkLargeOffsetBox *ptr;
	ptr = (GF_ChunkLargeOffsetBox *) s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	gf_free(ptr);
}

GF_Err co64_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 entries;
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;
	ptr->nb_entries = gf_bs_read_u32(bs);

	ISOM_DECREASE_SIZE(ptr, 4)

	if ((u64)ptr->nb_entries > ptr->size / 8 || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(u64)) {
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

GF_Box *co64_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChunkLargeOffsetBox, GF_ISOM_BOX_TYPE_CO64);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err co64_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err co64_box_size(GF_Box *s)
{
	GF_ChunkLargeOffsetBox *ptr = (GF_ChunkLargeOffsetBox *) s;

	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void cprt_box_del(GF_Box *s)
{
	GF_CopyrightBox *ptr = (GF_CopyrightBox *) s;
	if (ptr == NULL) return;
	if (ptr->notice)
		gf_free(ptr->notice);
	gf_free(ptr);
}


GF_Box *chpl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChapterListBox, GF_ISOM_BOX_TYPE_CHPL);
	tmp->list = gf_list_new();
	tmp->version = 1;
	return (GF_Box *)tmp;
}

void chpl_box_del(GF_Box *s)
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
GF_Err chpl_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_ChapterEntry *ce;
	u32 nb_chaps, len, i, count;
	GF_ChapterListBox *ptr = (GF_ChapterListBox *)s;

	ISOM_DECREASE_SIZE(ptr, 5)
	/*reserved or ???*/
	gf_bs_read_u32(bs);
	nb_chaps = gf_bs_read_u8(bs);

	count = 0;
	while (nb_chaps) {
		GF_SAFEALLOC(ce, GF_ChapterEntry);
		if (!ce) return GF_OUT_OF_MEM;
		ISOM_DECREASE_SIZE(ptr, 9)
		ce->start_time = gf_bs_read_u64(bs);
		len = gf_bs_read_u8(bs);
		if (ptr->size<len) return GF_ISOM_INVALID_FILE;
		if (len) {
			ce->name = (char *)gf_malloc(sizeof(char)*(len+1));
			if (!ce->name) return GF_OUT_OF_MEM;
			ISOM_DECREASE_SIZE(ptr, len)
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

GF_Err chpl_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err chpl_box_size(GF_Box *s)
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


GF_Err cprt_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	gf_bs_read_int(bs, 1);
	//the spec is unclear here, just says "the value 0 is interpreted as undetermined"
	ptr->packedLanguageCode[0] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[1] = gf_bs_read_int(bs, 5);
	ptr->packedLanguageCode[2] = gf_bs_read_int(bs, 5);

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
		ptr->notice = (char*)gf_malloc((bytesToRead+1) * sizeof(char));
		if (ptr->notice == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->notice, bytesToRead);
		ptr->notice[bytesToRead] = 0;
	}
	return GF_OK;
}

GF_Box *cprt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CopyrightBox, GF_ISOM_BOX_TYPE_CPRT);
	tmp->packedLanguageCode[0] = 'u';
	tmp->packedLanguageCode[1] = 'n';
	tmp->packedLanguageCode[2] = 'd';

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err cprt_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err cprt_box_size(GF_Box *s)
{
	GF_CopyrightBox *ptr = (GF_CopyrightBox *)s;

	ptr->size += 2;
	if (ptr->notice)
		ptr->size += strlen(ptr->notice) + 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void kind_box_del(GF_Box *s)
{
	GF_KindBox *ptr = (GF_KindBox *) s;
	if (ptr == NULL) return;
	if (ptr->schemeURI) gf_free(ptr->schemeURI);
	if (ptr->value) gf_free(ptr->value);
	gf_free(ptr);
}

GF_Err kind_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_KindBox *ptr = (GF_KindBox *)s;

	if (ptr->size) {
		u32 bytesToRead = (u32) ptr->size;
		char *data;
		u32 schemeURIlen;
		data = (char*)gf_malloc(bytesToRead * sizeof(char));
		if (!data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, data, bytesToRead);
		/*safety check in case the string is not null-terminated*/
		if (data[bytesToRead-1]) {
			data = (char*)gf_realloc(data, sizeof(char)*(bytesToRead + 1));
			if (!data) return GF_OUT_OF_MEM;
			data[bytesToRead] = 0;
			bytesToRead++;
		}
		ptr->schemeURI = gf_strdup(data);
		if (!ptr->schemeURI) return GF_OUT_OF_MEM;
		schemeURIlen = (u32) strlen(data);
		if (bytesToRead > schemeURIlen+1) {
			/* read the value */
			char *data_value = data + schemeURIlen +1;
			ptr->value = gf_strdup(data_value);
			if (!ptr->value) return GF_OUT_OF_MEM;
		}
		gf_free(data);
	}
	return GF_OK;
}

GF_Box *kind_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_KindBox, GF_ISOM_BOX_TYPE_KIND);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err kind_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_KindBox *ptr = (GF_KindBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
    if (ptr->schemeURI)
        gf_bs_write_data(bs, ptr->schemeURI, (u32) (strlen(ptr->schemeURI) + 1 ));
    else
        gf_bs_write_u8(bs, 0);

    if (ptr->value) {
		gf_bs_write_data(bs, ptr->value, (u32) (strlen(ptr->value) + 1) );
	}
	return GF_OK;
}

GF_Err kind_box_size(GF_Box *s)
{
	GF_KindBox *ptr = (GF_KindBox *)s;

    ptr->size += (ptr->schemeURI ? strlen(ptr->schemeURI) : 0) + 1;
	if (ptr->value) {
		ptr->size += strlen(ptr->value) + 1;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ctts_box_del(GF_Box *s)
{
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}



GF_Err ctts_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	u32 sampleCount;
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nb_entries = gf_bs_read_u32(bs);

	if (ptr->nb_entries > ptr->size / 8 || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(GF_DttsEntry) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in ctts\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->alloc_size = ptr->nb_entries;
	ptr->entries = (GF_DttsEntry *)gf_malloc(sizeof(GF_DttsEntry)*ptr->alloc_size);
	if (!ptr->entries) return GF_OUT_OF_MEM;
	sampleCount = 0;
	for (i=0; i<ptr->nb_entries; i++) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->entries[i].sampleCount = gf_bs_read_u32(bs);
		if (ptr->version)
			ptr->entries[i].decodingOffset = gf_bs_read_int(bs, 32);
		else
			ptr->entries[i].decodingOffset = (s32) gf_bs_read_u32(bs);

		if (ptr->max_cts_delta <= ABS(ptr->entries[i].decodingOffset)) {
			ptr->max_cts_delta = ABS(ptr->entries[i].decodingOffset);
			ptr->sample_num_max_cts_delta = sampleCount;
		}
		sampleCount += ptr->entries[i].sampleCount;
	}
#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_LastSampleNumber = sampleCount;
#endif
	return GF_OK;
}

GF_Box *ctts_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CompositionOffsetBox, GF_ISOM_BOX_TYPE_CTTS);
	return (GF_Box *) tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ctts_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err ctts_box_size(GF_Box *s)
{
	GF_CompositionOffsetBox *ptr = (GF_CompositionOffsetBox *) s;

	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void cslg_box_del(GF_Box *s)
{
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
	return;
}

GF_Err cslg_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	ISOM_DECREASE_SIZE(ptr, 20);
	ptr->compositionToDTSShift = gf_bs_read_int(bs, 32);
	ptr->leastDecodeToDisplayDelta = gf_bs_read_int(bs, 32);
	ptr->greatestDecodeToDisplayDelta = gf_bs_read_int(bs, 32);
	ptr->compositionStartTime = gf_bs_read_int(bs, 32);
	ptr->compositionEndTime = gf_bs_read_int(bs, 32);
	return GF_OK;
}

GF_Box *cslg_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CompositionToDecodeBox, GF_ISOM_BOX_TYPE_CSLG);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err cslg_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err cslg_box_size(GF_Box *s)
{
	GF_CompositionToDecodeBox *ptr = (GF_CompositionToDecodeBox *)s;

	ptr->size += 20;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ccst_box_del(GF_Box *s)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;
	if (ptr) gf_free(ptr);
	return;
}

GF_Err ccst_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->all_ref_pics_intra = gf_bs_read_int(bs, 1);
	ptr->intra_pred_used = gf_bs_read_int(bs, 1);
	ptr->max_ref_per_pic = gf_bs_read_int(bs, 4);
	ptr->reserved = gf_bs_read_int(bs, 26);
	return GF_OK;
}

GF_Box *ccst_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CodingConstraintsBox, GF_ISOM_BOX_TYPE_CCST);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ccst_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err ccst_box_size(GF_Box *s)
{
	GF_CodingConstraintsBox *ptr = (GF_CodingConstraintsBox *)s;
	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void url_box_del(GF_Box *s)
{
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) gf_free(ptr->location);
	gf_free(ptr);
	return;
}


GF_Err url_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

	if (ptr->size) {
		u32 location_size = (u32) ptr->size;
		if (location_size < 1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in svhd box\n", ptr->size));
			return GF_ISOM_INVALID_FILE;
		}
		ptr->location = (char*)gf_malloc(location_size);
		if (! ptr->location) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->location, location_size);
		if (ptr->location[location_size-1]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] url box location is not 0-terminated\n" ));
			return GF_ISOM_INVALID_FILE;
		}
	}
	return GF_OK;
}

GF_Box *url_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryURLBox, GF_ISOM_BOX_TYPE_URL);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err url_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err url_box_size(GF_Box *s)
{
	GF_DataEntryURLBox *ptr = (GF_DataEntryURLBox *)s;

	if ( !(ptr->flags & 1)) {
		if (ptr->location) ptr->size += 1 + strlen(ptr->location);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void urn_box_del(GF_Box *s)
{
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;
	if (ptr == NULL) return;
	if (ptr->location) gf_free(ptr->location);
	if (ptr->nameURN) gf_free(ptr->nameURN);
	gf_free(ptr);
}


GF_Err urn_box_read(GF_Box *s, GF_BitStream *bs)
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

GF_Box *urn_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryURNBox, GF_ISOM_BOX_TYPE_URN);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err urn_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err urn_box_size(GF_Box *s)
{
	GF_DataEntryURNBox *ptr = (GF_DataEntryURNBox *)s;

	if ( !(ptr->flags & 1)) {
		if (ptr->nameURN) ptr->size += 1 + strlen(ptr->nameURN);
		if (ptr->location) ptr->size += 1 + strlen(ptr->location);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void unkn_box_del(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *) s;
	if (!s) return;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err unkn_box_read(GF_Box *s, GF_BitStream *bs)
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
		gf_bs_set_cookie(sub_bs, GF_ISOM_BS_COOKIE_NO_LOGS);
		e = gf_isom_box_array_read(s, sub_bs);
	}
	gf_bs_del(sub_bs);
	if (e==GF_OK) {
		gf_free(ptr->data);
		ptr->data = NULL;
		ptr->dataSize = 0;
	} else if (s->child_boxes) {
		gf_isom_box_array_del(s->child_boxes);
		s->child_boxes=NULL;
	}

	return GF_OK;
}

GF_Box *unkn_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_UnknownBox, GF_ISOM_BOX_TYPE_UNKNOWN);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err unkn_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 type;
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;
	if (!s) return GF_BAD_PARAM;
	type = s->type;
	ptr->type = ptr->original_4cc;
	e = gf_isom_box_write_header(s, bs);
	ptr->type = type;
	if (e) return e;

	if (ptr->dataSize && ptr->data) {
		gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Err unkn_box_size(GF_Box *s)
{
	GF_UnknownBox *ptr = (GF_UnknownBox *)s;

	if (ptr->dataSize && ptr->data) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void def_parent_box_del(GF_Box *s)
{
	if (s) gf_free(s);
}


GF_Err def_parent_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *def_parent_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_Box, 0);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITEHintSa

GF_Err def_parent_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err def_parent_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void def_parent_full_box_del(GF_Box *s)
{
	if (s) gf_free(s);
}


GF_Err def_parent_full_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *def_parent_full_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_Box, 0);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITEHintSa

GF_Err def_parent_full_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err def_parent_full_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void uuid_box_del(GF_Box *s)
{
	GF_UnknownUUIDBox *ptr = (GF_UnknownUUIDBox *) s;
	if (!s) return;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err uuid_box_read(GF_Box *s, GF_BitStream *bs)
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

GF_Box *uuid_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_UnknownUUIDBox, GF_ISOM_BOX_TYPE_UUID);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err uuid_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err uuid_box_size(GF_Box *s)
{
	GF_UnknownUUIDBox*ptr = (GF_UnknownUUIDBox*)s;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void dinf_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err dinf_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_DataInformationBox *ptr = (GF_DataInformationBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_DREF:
		BOX_FIELD_ASSIGN(dref, GF_DataReferenceBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err dinf_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DataInformationBox *dinf;
	GF_Err e = gf_isom_box_array_read(s, bs);
	if (e) {
		return e;
	}
	dinf = (GF_DataInformationBox *)s;
	if (!dinf->dref) {
		if (! (gf_bs_get_cookie(bs) & GF_ISOM_BS_COOKIE_NO_LOGS) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing dref box in dinf\n"));
		}
		dinf->dref = (GF_DataReferenceBox *) gf_isom_box_new_parent(&dinf->child_boxes, GF_ISOM_BOX_TYPE_DREF);
	}
	return GF_OK;
}

GF_Box *dinf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataInformationBox, GF_ISOM_BOX_TYPE_DINF);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dinf_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err dinf_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void dref_box_del(GF_Box *s)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *) s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err dref_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	gf_bs_read_u32(bs);
	return gf_isom_box_array_read(s, bs);
}

GF_Box *dref_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataReferenceBox, GF_ISOM_BOX_TYPE_DREF);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dref_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count;
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = ptr->child_boxes ? gf_list_count(ptr->child_boxes) : 0;
	gf_bs_write_u32(bs, count);
	return GF_OK;
}

GF_Err dref_box_size(GF_Box *s)
{
	GF_DataReferenceBox *ptr = (GF_DataReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;

	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void edts_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err edts_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_EditBox *ptr = (GF_EditBox *)s;
	if (a->type == GF_ISOM_BOX_TYPE_ELST) {
		BOX_FIELD_ASSIGN(editList, GF_EditListBox)
		return GF_OK;
	} else {
		return GF_OK;
	}
	return GF_OK;
}


GF_Err edts_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *edts_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_EditBox, GF_ISOM_BOX_TYPE_EDTS);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err edts_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_EditBox *ptr = (GF_EditBox *)s;

	//here we have a trick: if editList is empty, skip the box
	if (ptr->editList && gf_list_count(ptr->editList->entryList)) {
		return gf_isom_box_write_header(s, bs);
	} else {
		s->size = 0;
	}
	return GF_OK;
}

GF_Err edts_box_size(GF_Box *s)
{
	GF_EditBox *ptr = (GF_EditBox *)s;

	//here we have a trick: if editList is empty, skip the box
	if (!ptr->editList || ! gf_list_count(ptr->editList->entryList)) {
		ptr->size = 0;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void elst_box_del(GF_Box *s)
{
	GF_EditListBox *ptr;
	u32 nb_entries;
	u32 i;

	ptr = (GF_EditListBox *)s;
	if (ptr == NULL) return;
	nb_entries = gf_list_count(ptr->entryList);
	for (i = 0; i < nb_entries; i++) {
		GF_EdtsEntry *p = (GF_EdtsEntry*)gf_list_get(ptr->entryList, i);
		if (p) gf_free(p);
	}
	gf_list_del(ptr->entryList);
	gf_free(ptr);
}

GF_Err elst_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 entries;
	s32 tr;
	u32 nb_entries;
	GF_EditListBox *ptr = (GF_EditListBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	nb_entries = gf_bs_read_u32(bs);

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
		GF_EdtsEntry *p;
		GF_SAFEALLOC(p, GF_EdtsEntry);
		if (!p) return GF_OUT_OF_MEM;
		if (ptr->version == 1) {
			ISOM_DECREASE_SIZE(ptr, 16);
			p->segmentDuration = gf_bs_read_u64(bs);
			p->mediaTime = (s64) gf_bs_read_u64(bs);
		} else {
			ISOM_DECREASE_SIZE(ptr, 8);
			p->segmentDuration = gf_bs_read_u32(bs);
			tr = gf_bs_read_u32(bs);
			p->mediaTime = (s64) tr;
		}
		ISOM_DECREASE_SIZE(ptr, 4);
		p->mediaRate = gf_bs_read_u32(bs);
		gf_list_add(ptr->entryList, p);
	}
	return GF_OK;
}

GF_Box *elst_box_new()
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

GF_Err elst_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	u32 nb_entries;
	GF_EditListBox *ptr = (GF_EditListBox *)s;
	if (!ptr) return GF_BAD_PARAM;

	nb_entries = gf_list_count(ptr->entryList);
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, nb_entries);
	for (i = 0; i < nb_entries; i++ ) {
		GF_EdtsEntry *p = (GF_EdtsEntry*)gf_list_get(ptr->entryList, i);
		if (ptr->version == 1) {
			gf_bs_write_u64(bs, p->segmentDuration);
			gf_bs_write_u64(bs, p->mediaTime);
		} else {
			gf_bs_write_u32(bs, (u32) p->segmentDuration);
			gf_bs_write_u32(bs, (s32) p->mediaTime);
		}
		gf_bs_write_u32(bs, p->mediaRate);
	}
	return GF_OK;
}

GF_Err elst_box_size(GF_Box *s)
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

void esds_box_del(GF_Box *s)
{
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	if (ptr == NULL)	return;
	if (ptr->desc) gf_odf_desc_del((GF_Descriptor *)ptr->desc);
	gf_free(ptr);
}


GF_Err esds_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e=GF_OK;
	u32 descSize;
	GF_ESDBox *ptr = (GF_ESDBox *)s;

	descSize = (u32) (ptr->size);

	if (descSize) {
		char *enc_desc = (char*)gf_malloc(sizeof(char) * descSize);
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
			if (ptr->desc && !ptr->desc->URLString) {
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

GF_Box *esds_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ESDBox, GF_ISOM_BOX_TYPE_ESDS);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err esds_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u8 *enc_desc;
	u32 descSize = 0;
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	//make sure we write with no ESID and no OCRESID
    if (ptr->desc) {
        ptr->desc->ESID = 0;
        ptr->desc->OCRESID = 0;
    }
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	e = gf_odf_desc_write((GF_Descriptor *)ptr->desc, &enc_desc, &descSize);
	if (e) return e;
	gf_bs_write_data(bs, enc_desc, descSize);
	//free our buffer
	gf_free(enc_desc);
	return GF_OK;
}

GF_Err esds_box_size(GF_Box *s)
{
	u32 descSize = 0;
	GF_ESDBox *ptr = (GF_ESDBox *)s;
	//make sure we write with no ESID and no OCRESID
    if (ptr->desc) {
        ptr->desc->ESID = 0;
        ptr->desc->OCRESID = 0;
    }
	descSize = gf_odf_desc_size((GF_Descriptor *)ptr->desc);
	ptr->size += descSize;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void free_box_del(GF_Box *s)
{
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err free_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;

	if (ptr->size > 0xFFFFFFFF) return GF_IO_ERR;

	bytesToRead = (u32) (ptr->size);

	if (bytesToRead) {
		ptr->data = (char*)gf_malloc(bytesToRead * sizeof(char));
		if (!ptr->data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->data, bytesToRead);
		ptr->dataSize = bytesToRead;
	}
	return GF_OK;
}

GF_Box *free_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FreeSpaceBox, GF_ISOM_BOX_TYPE_FREE);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err free_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err free_box_size(GF_Box *s)
{
	GF_FreeSpaceBox *ptr = (GF_FreeSpaceBox *)s;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ftyp_box_del(GF_Box *s)
{
	GF_FileTypeBox *ptr = (GF_FileTypeBox *) s;
	if (ptr->altBrand) gf_free(ptr->altBrand);
	gf_free(ptr);
}

GF_Box *ftyp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FileTypeBox, GF_ISOM_BOX_TYPE_FTYP);
	return (GF_Box *)tmp;
}

GF_Err ftyp_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->majorBrand = gf_bs_read_u32(bs);
	ptr->minorVersion = gf_bs_read_u32(bs);

	if (ptr->size % 4) return GF_ISOM_INVALID_FILE;
	ptr->altCount = ( (u32) (ptr->size)) / 4;
	if (!ptr->altCount) return GF_OK;

	ptr->altBrand = (u32*)gf_malloc(sizeof(u32)*ptr->altCount);
	if (!ptr->altBrand) return GF_OUT_OF_MEM;

	for (i = 0; i<ptr->altCount; i++) {
		ptr->altBrand[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ftyp_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err ftyp_box_size(GF_Box *s)
{
	GF_FileTypeBox *ptr = (GF_FileTypeBox *)s;

	ptr->size += 8 + ptr->altCount * 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gnrm_box_del(GF_Box *s)
{
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnrm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericSampleEntryBox, GF_ISOM_BOX_TYPE_GNRM);

	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

//dummy
GF_Err gnrm_box_read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gnrm_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;

	//careful we are not writing the box type but the entry type so switch for write
	ptr->type = ptr->EntryType;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GNRM;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	return GF_OK;
}

GF_Err gnrm_box_size(GF_Box *s)
{
	GF_GenericSampleEntryBox *ptr = (GF_GenericSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRM;
	ptr->size += 8+ptr->data_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gnrv_box_del(GF_Box *s)
{
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnrv_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericVisualSampleEntryBox, GF_ISOM_BOX_TYPE_GNRV);
	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}
//dummy
GF_Err gnrv_box_read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gnrv_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;

	//careful we are not writing the box type but the entry type so switch for write
	ptr->type = ptr->EntryType;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GNRV;

	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)ptr, bs);
	gf_bs_write_data(bs,  ptr->data, ptr->data_size);
	return GF_OK;
}

GF_Err gnrv_box_size(GF_Box *s)
{
	GF_GenericVisualSampleEntryBox *ptr = (GF_GenericVisualSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRV;
	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);
	ptr->size += ptr->data_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gnra_box_del(GF_Box *s)
{
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)ptr);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Box *gnra_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericAudioSampleEntryBox, GF_ISOM_BOX_TYPE_GNRA);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*) tmp);
	return (GF_Box *)tmp;
}
//dummy
GF_Err gnra_box_read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
}
#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err gnra_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;

	//careful we are not writing the box type but the entry type so switch for write
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

GF_Err gnra_box_size(GF_Box *s)
{
	GF_GenericAudioSampleEntryBox *ptr = (GF_GenericAudioSampleEntryBox *)s;
	s->type = GF_ISOM_BOX_TYPE_GNRA;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox *)s);
	ptr->size += ptr->data_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void hdlr_box_del(GF_Box *s)
{
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	if (ptr == NULL) return;
	if (ptr->nameUTF8) gf_free(ptr->nameUTF8);
	gf_free(ptr);
}


GF_Err hdlr_box_read(GF_Box *s, GF_BitStream *bs)
{
	u64 cookie;
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;

	ISOM_DECREASE_SIZE(ptr, 20);
	ptr->reserved1 = gf_bs_read_u32(bs);
	ptr->handlerType = gf_bs_read_u32(bs);
	gf_bs_read_data(bs, (char*)ptr->reserved2, 12);

	cookie = gf_bs_get_cookie(bs);
	if (ptr->handlerType==GF_ISOM_MEDIA_VISUAL)
		cookie |= GF_ISOM_BS_COOKIE_VISUAL_TRACK;
	else
		cookie &= ~GF_ISOM_BS_COOKIE_VISUAL_TRACK;
	gf_bs_set_cookie(bs, cookie);

	if (ptr->size) {
		u32 name_size = (u32) ptr->size;
		if (name_size < 1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in hdlr\n", ptr->size));
			return GF_ISOM_INVALID_FILE;
		}
		ptr->nameUTF8 = (char*)gf_malloc(name_size);
		if (!ptr->nameUTF8) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->nameUTF8, name_size);

		//patch for old QT files - we cannot rely on checking if str[0]==len(str+1) since we may have
		//cases where the first character of the string decimal value is indeed the same as the string length!!
		//we had this issue with encryption_import test
		//we therefore only check if last char is null, and if not so assume old QT style
		if (ptr->nameUTF8[name_size-1]) {
			if (name_size > 1)
				memmove(ptr->nameUTF8, ptr->nameUTF8+1, sizeof(char) * (u32) (name_size-1) );
			ptr->nameUTF8[name_size-1] = 0;
			ptr->store_counted_string = GF_TRUE;
		}
	}
	return GF_OK;
}

GF_Box *hdlr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_HandlerBox, GF_ISOM_BOX_TYPE_HDLR);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err hdlr_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err hdlr_box_size(GF_Box *s)
{
	GF_HandlerBox *ptr = (GF_HandlerBox *)s;
	ptr->size += 20 + 1; //null term or counted string
	if (ptr->nameUTF8) {
		ptr->size += strlen(ptr->nameUTF8);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void hinf_box_del(GF_Box *s)
{
	GF_HintInfoBox *hinf = (GF_HintInfoBox *)s;
	gf_free(hinf);
}

GF_Box *hinf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_HintInfoBox, GF_ISOM_BOX_TYPE_HINF);

	tmp->child_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

GF_Err hinf_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_HintInfoBox *hinf = (GF_HintInfoBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MAXR:
		if (!is_rem) {
			u32 i=0;
			GF_MAXRBox *maxR;
			while ((maxR = (GF_MAXRBox *)gf_list_enum(hinf->child_boxes, &i))) {
				if ((maxR->type==GF_ISOM_BOX_TYPE_MAXR) && (maxR->granularity == ((GF_MAXRBox *)a)->granularity))
					ERROR_ON_DUPLICATED_BOX(a, s)
			}
		}
		break;
	}
	return GF_OK;
}


GF_Err hinf_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err hinf_box_write(GF_Box *s, GF_BitStream *bs)
{
//	GF_HintInfoBox *ptr = (GF_HintInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err hinf_box_size(GF_Box *s)
{
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void hmhd_box_del(GF_Box *s)
{
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err hmhd_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;

	ISOM_DECREASE_SIZE(ptr, 16);
	ptr->maxPDUSize = gf_bs_read_u16(bs);
	ptr->avgPDUSize = gf_bs_read_u16(bs);
	ptr->maxBitrate = gf_bs_read_u32(bs);
	ptr->avgBitrate = gf_bs_read_u32(bs);
	ptr->slidingAverageBitrate = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *hmhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_HintMediaHeaderBox, GF_ISOM_BOX_TYPE_HMHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err hmhd_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err hmhd_box_size(GF_Box *s)
{
	GF_HintMediaHeaderBox *ptr = (GF_HintMediaHeaderBox *)s;
	ptr->size += 16;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *hnti_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_HintTrackInfoBox, GF_ISOM_BOX_TYPE_HNTI);
	return (GF_Box *)tmp;
}

void hnti_box_del(GF_Box *a)
{
	gf_free(a);
}

GF_Err hnti_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_HintTrackInfoBox *ptr = (GF_HintTrackInfoBox *)s;
	if (!ptr || !a) return GF_BAD_PARAM;

	switch (a->type) {
	//this is the value for GF_RTPBox - same as HintSampleEntry for RTP !!!
	case GF_ISOM_BOX_TYPE_RTP:
	case GF_ISOM_BOX_TYPE_SDP:
		BOX_FIELD_ASSIGN(SDP, GF_Box)
		break;
	default:
		break;
	}
	return GF_OK;
}

GF_Err hnti_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, s->type);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hnti_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}


GF_Err hnti_box_size(GF_Box *s)
{
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		GF_SDPBox
**********************************************************/

void sdp_box_del(GF_Box *s)
{
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr->sdpText) gf_free(ptr->sdpText);
	gf_free(ptr);

}
GF_Err sdp_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	length = (u32) (ptr->size);

	if (length >= (u32)0xFFFFFFFF) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid length %lu in sdp box\n", length));
		return GF_ISOM_INVALID_FILE;
	}

	//sdp text has no delimiter !!!
	ptr->sdpText = (char*)gf_malloc(sizeof(char) * (length+1));
	if (!ptr->sdpText) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->sdpText, length);
	ptr->sdpText[length] = 0;
	return GF_OK;
}
GF_Box *sdp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SDPBox, GF_ISOM_BOX_TYPE_SDP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sdp_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	//don't write the NULL char!!!
	if (ptr->sdpText)
		gf_bs_write_data(bs, ptr->sdpText, (u32) strlen(ptr->sdpText));
	return GF_OK;
}
GF_Err sdp_box_size(GF_Box *s)
{
	GF_SDPBox *ptr = (GF_SDPBox *)s;
	//don't count the NULL char!!!
	if (ptr->sdpText)
		ptr->size += strlen(ptr->sdpText);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




void rtp_hnti_box_del(GF_Box *s)
{
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	if (ptr->sdpText) gf_free(ptr->sdpText);
	gf_free(ptr);

}
GF_Err rtp_hnti_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->subType = gf_bs_read_u32(bs);

	length = (u32) (ptr->size);

	if (length >= (u32)0xFFFFFFFF) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid length %lu in rtp_hnti box\n", length));
		return GF_ISOM_INVALID_FILE;
	}

	//sdp text has no delimiter !!!
	ptr->sdpText = (char*)gf_malloc(sizeof(char) * (length+1));
	if (!ptr->sdpText) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->sdpText, length);
	ptr->sdpText[length] = 0;
	return GF_OK;
}

GF_Box *rtp_hnti_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_RTPBox, GF_ISOM_BOX_TYPE_RTP);
	tmp->subType = GF_ISOM_BOX_TYPE_SDP;
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rtp_hnti_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err rtp_hnti_box_size(GF_Box *s)
{
	GF_RTPBox *ptr = (GF_RTPBox *)s;
	ptr->size += 4 + strlen(ptr->sdpText);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TRPY GF_Box
**********************************************************/

void trpy_box_del(GF_Box *s)
{
	gf_free((GF_TRPYBox *)s);
}
GF_Err trpy_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TRPYBox *ptr = (GF_TRPYBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *trpy_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TRPYBox, GF_ISOM_BOX_TYPE_TRPY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trpy_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TRPYBox *ptr = (GF_TRPYBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err trpy_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		TOTL GF_Box
**********************************************************/

void totl_box_del(GF_Box *s)
{
	gf_free((GF_TRPYBox *)s);
}
GF_Err totl_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TOTLBox *ptr = (GF_TOTLBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *totl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TOTLBox, GF_ISOM_BOX_TYPE_TOTL);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err totl_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TOTLBox *ptr = (GF_TOTLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err totl_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		NUMP GF_Box
**********************************************************/

void nump_box_del(GF_Box *s)
{
	gf_free((GF_NUMPBox *)s);
}
GF_Err nump_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_NUMPBox *ptr = (GF_NUMPBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->nbPackets = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *nump_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_NUMPBox, GF_ISOM_BOX_TYPE_NUMP);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err nump_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NUMPBox *ptr = (GF_NUMPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbPackets);
	return GF_OK;
}
GF_Err nump_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		NPCK GF_Box
**********************************************************/

void npck_box_del(GF_Box *s)
{
	gf_free((GF_NPCKBox *)s);
}
GF_Err npck_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_NPCKBox *ptr = (GF_NPCKBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nbPackets = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *npck_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_NPCKBox, GF_ISOM_BOX_TYPE_NPCK);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err npck_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NPCKBox *ptr = (GF_NPCKBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbPackets);
	return GF_OK;
}
GF_Err npck_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TPYL GF_Box
**********************************************************/

void tpyl_box_del(GF_Box *s)
{
	gf_free((GF_NTYLBox *)s);
}
GF_Err tpyl_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_NTYLBox *ptr = (GF_NTYLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *tpyl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_NTYLBox, GF_ISOM_BOX_TYPE_TPYL);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tpyl_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_NTYLBox *ptr = (GF_NTYLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err tpyl_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		TPAY GF_Box
**********************************************************/

void tpay_box_del(GF_Box *s)
{
	gf_free((GF_TPAYBox *)s);
}
GF_Err tpay_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TPAYBox *ptr = (GF_TPAYBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nbBytes = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tpay_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TPAYBox, GF_ISOM_BOX_TYPE_TPAY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tpay_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TPAYBox *ptr = (GF_TPAYBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err tpay_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		MAXR GF_Box
**********************************************************/

void maxr_box_del(GF_Box *s)
{
	gf_free((GF_MAXRBox *)s);
}
GF_Err maxr_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MAXRBox *ptr = (GF_MAXRBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->granularity = gf_bs_read_u32(bs);
	ptr->maxDataRate = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *maxr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MAXRBox, GF_ISOM_BOX_TYPE_MAXR);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err maxr_box_write(GF_Box *s, GF_BitStream *bs)
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
GF_Err maxr_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		DMED GF_Box
**********************************************************/

void dmed_box_del(GF_Box *s)
{
	gf_free((GF_DMEDBox *)s);
}
GF_Err dmed_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMEDBox *ptr = (GF_DMEDBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dmed_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DMEDBox, GF_ISOM_BOX_TYPE_DMED);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dmed_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DMEDBox *ptr = (GF_DMEDBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err dmed_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		DIMM GF_Box
**********************************************************/

void dimm_box_del(GF_Box *s)
{
	gf_free((GF_DIMMBox *)s);
}
GF_Err dimm_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DIMMBox *ptr = (GF_DIMMBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8)
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *dimm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DIMMBox, GF_ISOM_BOX_TYPE_DIMM);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dimm_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DIMMBox *ptr = (GF_DIMMBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err dimm_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/**********************************************************
		DREP GF_Box
**********************************************************/

void drep_box_del(GF_Box *s)
{
	gf_free((GF_DREPBox *)s);
}
GF_Err drep_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DREPBox *ptr = (GF_DREPBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8)
	ptr->nbBytes = gf_bs_read_u64(bs);
	return GF_OK;
}
GF_Box *drep_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DREPBox, GF_ISOM_BOX_TYPE_DREP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err drep_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DREPBox *ptr = (GF_DREPBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->nbBytes);
	return GF_OK;
}
GF_Err drep_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



/**********************************************************
		TMIN GF_Box
**********************************************************/

void tmin_box_del(GF_Box *s)
{
	gf_free((GF_TMINBox *)s);
}
GF_Err tmin_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMINBox *ptr = (GF_TMINBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->minTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmin_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TMINBox, GF_ISOM_BOX_TYPE_TMIN);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tmin_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TMINBox *ptr = (GF_TMINBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->minTime);
	return GF_OK;
}
GF_Err tmin_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		TMAX GF_Box
**********************************************************/

void tmax_box_del(GF_Box *s)
{
	gf_free((GF_TMAXBox *)s);
}
GF_Err tmax_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TMAXBox *ptr = (GF_TMAXBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->maxTime = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *tmax_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TMAXBox, GF_ISOM_BOX_TYPE_TMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tmax_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TMAXBox *ptr = (GF_TMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxTime);
	return GF_OK;
}
GF_Err tmax_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		PMAX GF_Box
**********************************************************/

void pmax_box_del(GF_Box *s)
{
	gf_free((GF_PMAXBox *)s);
}
GF_Err pmax_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_PMAXBox *ptr = (GF_PMAXBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->maxSize = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *pmax_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PMAXBox, GF_ISOM_BOX_TYPE_PMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err pmax_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PMAXBox *ptr = (GF_PMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxSize);
	return GF_OK;
}
GF_Err pmax_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		DMAX GF_Box
**********************************************************/

void dmax_box_del(GF_Box *s)
{
	gf_free((GF_DMAXBox *)s);
}
GF_Err dmax_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_DMAXBox *ptr = (GF_DMAXBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->maxDur = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *dmax_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DMAXBox, GF_ISOM_BOX_TYPE_DMAX);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dmax_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DMAXBox *ptr = (GF_DMAXBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->maxDur);
	return GF_OK;
}
GF_Err dmax_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/**********************************************************
		PAYT GF_Box
**********************************************************/

void payt_box_del(GF_Box *s)
{
	GF_PAYTBox *payt = (GF_PAYTBox *)s;
	if (payt->payloadString) gf_free(payt->payloadString);
	gf_free(payt);
}
GF_Err payt_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;

	ISOM_DECREASE_SIZE(ptr, 5 );
	ptr->payloadCode = gf_bs_read_u32(bs);
	length = gf_bs_read_u8(bs);
	ISOM_DECREASE_SIZE(ptr, length);
	ptr->payloadString = (char*)gf_malloc(sizeof(char) * (length+1) );
	if (! ptr->payloadString) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->payloadString, length);
	ptr->payloadString[length] = 0;

	return GF_OK;
}
GF_Box *payt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PAYTBox, GF_ISOM_BOX_TYPE_PAYT);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err payt_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_Err e;
	GF_PAYTBox *ptr = (GF_PAYTBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->payloadCode);
    len = ptr->payloadString ? (u32) strlen(ptr->payloadString) : 0;
	gf_bs_write_u8(bs, len);
	if (len) gf_bs_write_data(bs, ptr->payloadString, len);
	return GF_OK;
}
GF_Err payt_box_size(GF_Box *s)
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

void name_box_del(GF_Box *s)
{
	GF_NameBox *name = (GF_NameBox *)s;
	if (name->string) gf_free(name->string);
	gf_free(name);
}
GF_Err name_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 length;
	GF_NameBox *ptr = (GF_NameBox *)s;

	length = (u32) (ptr->size);

	if (length >= (u32)0xFFFFFFFF) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid length %lu in name box\n", length));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->string = (char*)gf_malloc(sizeof(char) * (length+1));
	if (! ptr->string) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->string, length);
	ptr->string[length] = 0;
	return GF_OK;
}
GF_Box *name_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_NameBox, GF_ISOM_BOX_TYPE_NAME);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err name_box_write(GF_Box *s, GF_BitStream *bs)
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
GF_Err name_box_size(GF_Box *s)
{
	GF_NameBox *ptr = (GF_NameBox *)s;
	if (ptr->string) ptr->size += strlen(ptr->string) + 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tssy_box_del(GF_Box *s)
{
	gf_free(s);
}
GF_Err tssy_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeStampSynchronyBox *ptr = (GF_TimeStampSynchronyBox *)s;
	ISOM_DECREASE_SIZE(ptr, 1)
	gf_bs_read_int(bs, 6);
	ptr->timestamp_sync = gf_bs_read_int(bs, 2);
	return GF_OK;
}
GF_Box *tssy_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeStampSynchronyBox, GF_ISOM_BOX_TYPE_TSSY);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tssy_box_write(GF_Box *s, GF_BitStream *bs)
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
GF_Err tssy_box_size(GF_Box *s)
{
	s->size += 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void srpp_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err srpp_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_SCHI:
		BOX_FIELD_ASSIGN(info, GF_SchemeInformationBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SCHM:
		BOX_FIELD_ASSIGN(scheme_type, GF_SchemeTypeBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err srpp_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;

	ISOM_DECREASE_SIZE(s, 16)
	ptr->encryption_algorithm_rtp = gf_bs_read_u32(bs);
	ptr->encryption_algorithm_rtcp = gf_bs_read_u32(bs);
	ptr->integrity_algorithm_rtp = gf_bs_read_u32(bs);
	ptr->integrity_algorithm_rtcp = gf_bs_read_u32(bs);
	return gf_isom_box_array_read(s, bs);
}
GF_Box *srpp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SRTPProcessBox, GF_ISOM_BOX_TYPE_SRPP);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err srpp_box_write(GF_Box *s, GF_BitStream *bs)
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

	return GF_OK;
}
GF_Err srpp_box_size(GF_Box *s)
{
	u32 pos = 0;
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *)s;
	s->size += 16;
	gf_isom_check_position(s, (GF_Box*)ptr->info, &pos);
	gf_isom_check_position(s, (GF_Box*)ptr->scheme_type, &pos);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void rssr_box_del(GF_Box *s)
{
	gf_free(s);
}
GF_Err rssr_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ReceivedSsrcBox *ptr = (GF_ReceivedSsrcBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->ssrc = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *rssr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ReceivedSsrcBox, GF_ISOM_BOX_TYPE_RSSR);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rssr_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ReceivedSsrcBox *ptr = (GF_ReceivedSsrcBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->ssrc);
	return GF_OK;
}
GF_Err rssr_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/




void iods_box_del(GF_Box *s)
{
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;
	if (ptr == NULL) return;
	if (ptr->descriptor) gf_odf_desc_del(ptr->descriptor);
	gf_free(ptr);
}


GF_Err iods_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 descSize;
	char *desc;
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;

	//use the OD codec...
	descSize = (u32) (ptr->size);
	desc = (char*)gf_malloc(sizeof(char) * descSize);
	if (!desc) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, desc, descSize);
	e = gf_odf_desc_read(desc, descSize, &ptr->descriptor);
	//OK, free our desc
	gf_free(desc);

	if (e) return e;
	switch (ptr->descriptor->tag) {
	case GF_ODF_ISOM_OD_TAG:
	case GF_ODF_ISOM_IOD_TAG:
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid descriptor in iods, tag %u found but only %u or %u allowed\n", ptr->descriptor->tag, GF_ODF_ISOM_IOD_TAG, GF_ODF_ISOM_OD_TAG ));
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}

GF_Box *iods_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ObjectDescriptorBox, GF_ISOM_BOX_TYPE_IODS);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err iods_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 descSize;
	u8 *desc;
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

GF_Err iods_box_size(GF_Box *s)
{
	GF_ObjectDescriptorBox *ptr = (GF_ObjectDescriptorBox *)s;

	ptr->size += gf_odf_desc_size(ptr->descriptor);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mdat_box_del(GF_Box *s)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	if (!s) return;

	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}


GF_Err mdat_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	ptr->dataSize = s->size;
	ptr->bsOffset = gf_bs_get_position(bs);

	//store idat for rewrite
	if (ptr->type==GF_ISOM_BOX_TYPE_IDAT) {
		ptr->data = gf_malloc(sizeof(u8) * (size_t)ptr->dataSize);
		if (!ptr->data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->data, (u32) ptr->dataSize);
		ptr->size = 0;
		return GF_OK;
	}

	//then skip these bytes
	gf_bs_skip_bytes(bs, ptr->dataSize);
	return GF_OK;
}

GF_Box *mdat_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaDataBox, GF_ISOM_BOX_TYPE_MDAT);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mdat_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	//make sure we have some data ...
	//if not, we handle that independently (edit files)
	if (ptr->data) {
		gf_bs_write_data(bs, ptr->data, (u32) ptr->dataSize);
	}
	return GF_OK;
}

GF_Err mdat_box_size(GF_Box *s)
{
	GF_MediaDataBox *ptr = (GF_MediaDataBox *)s;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mdhd_box_del(GF_Box *s)
{
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err mdhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;

	if (ptr->version == 1) {
		ISOM_DECREASE_SIZE(ptr, 28)
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 16)
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

	ISOM_DECREASE_SIZE(ptr, 4)
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

GF_Box *mdhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaHeaderBox, GF_ISOM_BOX_TYPE_MDHD);

	tmp->packedLanguage[0] = 'u';
	tmp->packedLanguage[1] = 'n';
	tmp->packedLanguage[2] = 'd';
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mdhd_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err mdhd_box_size(GF_Box *s)
{
	GF_MediaHeaderBox *ptr = (GF_MediaHeaderBox *)s;
	ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;

	ptr->size += 4;
	ptr->size += (ptr->version == 1) ? 28 : 16;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void mdia_box_del(GF_Box *s)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	if (ptr == NULL) return;
	if (ptr->nalu_parser) gf_bs_del(ptr->nalu_parser);
	if (ptr->nalu_out_bs) gf_bs_del(ptr->nalu_out_bs);
	if (ptr->nalu_ps_bs) gf_bs_del(ptr->nalu_ps_bs);
	if (ptr->extracted_bs) gf_bs_del(ptr->extracted_bs);
	if (ptr->extracted_samp) gf_isom_sample_del(&ptr->extracted_samp);
	if (ptr->in_sample_buffer) gf_free(ptr->in_sample_buffer);
	if (ptr->tmp_nal_copy_buffer) gf_free(ptr->tmp_nal_copy_buffer);
	gf_free(ptr);
}


GF_Err mdia_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_MDHD:
		BOX_FIELD_ASSIGN(mediaHeader, GF_MediaHeaderBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_HDLR:
		BOX_FIELD_ASSIGN(handler, GF_HandlerBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_MINF:
		BOX_FIELD_ASSIGN(information, GF_MediaInformationBox)
		return GF_OK;
	}
	return GF_OK;
}


GF_Err mdia_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u64 cookie = gf_bs_get_cookie(bs);
	cookie &= ~GF_ISOM_BS_COOKIE_VISUAL_TRACK;
	gf_bs_set_cookie(bs, cookie);
	e = gf_isom_box_array_read(s, bs);
	gf_bs_set_cookie(bs, cookie);

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

GF_Box *mdia_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaBox, GF_ISOM_BOX_TYPE_MDIA);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mdia_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err mdia_box_size(GF_Box *s)
{
	u32 pos = 0;
	GF_MediaBox *ptr = (GF_MediaBox *)s;
	//Header first
	gf_isom_check_position(s, (GF_Box*)ptr->mediaHeader, &pos);
	//then handler
	gf_isom_check_position(s, (GF_Box*)ptr->handler, &pos);

#if 0
	//elng before info for CMAF info - we deactiveate for now, no specific errors raised and CMAF should not impose any order
	GF_Box *elng = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_ELNG);
	if (elng)
		gf_isom_check_position(s, elng, &pos);
#endif

	//then info
	gf_isom_check_position(s, (GF_Box*)ptr->information, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mfra_box_del(GF_Box *s)
{
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;
	if (ptr == NULL) return;
	gf_list_del(ptr->tfra_list);
	gf_free(ptr);
}

GF_Box *mfra_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentRandomAccessBox, GF_ISOM_BOX_TYPE_MFRA);
	tmp->tfra_list = gf_list_new();
	return (GF_Box *)tmp;
}

GF_Err mfra_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_TFRA:
		BOX_FIELD_LIST_ASSIGN(tfra_list);
		return GF_OK;
	case GF_ISOM_BOX_TYPE_MFRO:
		BOX_FIELD_ASSIGN(mfro, GF_MovieFragmentRandomAccessOffsetBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err mfra_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mfra_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err mfra_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MovieFragmentRandomAccessBox *ptr = (GF_MovieFragmentRandomAccessBox *)s;
	gf_isom_check_position_list(s, ptr->tfra_list, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->mfro, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tfra_box_del(GF_Box *s)
{
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Box *tfra_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentRandomAccessBox, GF_ISOM_BOX_TYPE_TFRA);
	return (GF_Box *)tmp;
}

GF_Err tfra_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_RandomAccessEntry *p = 0;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	ISOM_DECREASE_SIZE(ptr, 12);

	ptr->track_id = gf_bs_read_u32(bs);
	if (gf_bs_read_int(bs, 26) != 0)
		return GF_ISOM_INVALID_FILE;

	ptr->traf_bits = (gf_bs_read_int(bs, 2) + 1) * 8;
	ptr->trun_bits = (gf_bs_read_int(bs, 2) + 1) * 8;
	ptr->sample_bits = (gf_bs_read_int(bs, 2) + 1) * 8;

	ptr->nb_entries = gf_bs_read_u32(bs);

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
		if ((u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(GF_RandomAccessEntry)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in traf\n", ptr->nb_entries));
			return GF_ISOM_INVALID_FILE;
		}
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

GF_Err tfra_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, sap_nb_entries;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->track_id);
	gf_bs_write_int(bs, 0, 26);

	gf_bs_write_int(bs, ptr->traf_bits/8 - 1, 2);
	gf_bs_write_int(bs, ptr->trun_bits/8 - 1, 2);
	gf_bs_write_int(bs, ptr->sample_bits/8 - 1, 2);

	sap_nb_entries = 0;
	for (i=0; i<ptr->nb_entries; i++) {
		GF_RandomAccessEntry *p = &ptr->entries[i];
		//no sap found, do not store
		if (p->trun_number) sap_nb_entries++;
	}

	gf_bs_write_u32(bs, sap_nb_entries);

	for (i=0; i<ptr->nb_entries; i++) {
		GF_RandomAccessEntry *p = &ptr->entries[i];
		//no sap found, do not store
		if (!p->trun_number) continue;
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

GF_Err tfra_box_size(GF_Box *s)
{
	u32 i;
	GF_TrackFragmentRandomAccessBox *ptr = (GF_TrackFragmentRandomAccessBox *)s;
	ptr->size += 12;

	for (i=0; i<ptr->nb_entries; i++) {
		GF_RandomAccessEntry *p = &ptr->entries[i];
		//no sap found, do not store
		if (!p->trun_number) continue;
		ptr->size +=  ((ptr->version==1) ? 16 : 8 ) + ptr->traf_bits/8 + ptr->trun_bits/8 + ptr->sample_bits/8;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void mfro_box_del(GF_Box *s)
{
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Box *mfro_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentRandomAccessOffsetBox, GF_ISOM_BOX_TYPE_MFRO);
	return (GF_Box *)tmp;
}

GF_Err mfro_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->container_size = gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mfro_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentRandomAccessOffsetBox *ptr = (GF_MovieFragmentRandomAccessOffsetBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->container_size);
	return GF_OK;
}

GF_Err mfro_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void elng_box_del(GF_Box *s)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;
	if (ptr == NULL) return;
	if (ptr->extended_language) gf_free(ptr->extended_language);
	gf_free(ptr);
}

GF_Err elng_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;

	if (ptr->size) {
		ptr->extended_language = (char*)gf_malloc((u32) ptr->size);
		if (ptr->extended_language == NULL) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->extended_language, (u32) ptr->size);
		/*safety check in case the string is not null-terminated*/
		if (ptr->extended_language[ptr->size-1]) {
			char *str = (char*)gf_malloc((u32) ptr->size + 1);
			if (!str) return GF_OUT_OF_MEM;
			memcpy(str, ptr->extended_language, (u32) ptr->size);
			str[ptr->size] = 0;
			gf_free(ptr->extended_language);
			ptr->extended_language = str;
		}
	}
	return GF_OK;
}

GF_Box *elng_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaBox, GF_ISOM_BOX_TYPE_ELNG);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err elng_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err elng_box_size(GF_Box *s)
{
	GF_ExtendedLanguageBox *ptr = (GF_ExtendedLanguageBox *)s;

	if (ptr->extended_language) {
		ptr->size += strlen(ptr->extended_language)+1;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void mfhd_box_del(GF_Box *s)
{
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err mfhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->sequence_number = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *mfhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentHeaderBox, GF_ISOM_BOX_TYPE_MFHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err mfhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MovieFragmentHeaderBox *ptr = (GF_MovieFragmentHeaderBox *) s;
	if (!s) return GF_BAD_PARAM;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->sequence_number);
	return GF_OK;
}

GF_Err mfhd_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void minf_box_del(GF_Box *s)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	if (ptr == NULL) return;

	//if we have a Handler not self-contained, delete it (the self-contained belongs to the movie)
	if (ptr->dataHandler) {
		gf_isom_datamap_close(ptr);
	}
	gf_free(ptr);
}

GF_Err minf_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_NMHD:
	case GF_ISOM_BOX_TYPE_STHD:
	case GF_ISOM_BOX_TYPE_VMHD:
	case GF_ISOM_BOX_TYPE_SMHD:
	case GF_ISOM_BOX_TYPE_HMHD:
	case GF_ISOM_BOX_TYPE_GMHD:
		BOX_FIELD_ASSIGN(InfoHeader, GF_Box)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_DINF:
		BOX_FIELD_ASSIGN(dataInformation, GF_DataInformationBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_STBL:
		BOX_FIELD_ASSIGN(sampleTable, GF_SampleTableBox)
		return GF_OK;
	}
	return GF_OK;
}


GF_Err minf_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	GF_Err e;

	e = gf_isom_box_array_read(s, bs);

	if (!e && ! ptr->dataInformation) {
		GF_Box *url;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing DataInformationBox\n"));
		//commented on purpose, we are still able to handle the file, we only throw an error but keep processing
//		e = GF_ISOM_INVALID_FILE;

		//add a dinf box to avoid any access to a null dinf
		ptr->dataInformation = (GF_DataInformationBox *) gf_isom_box_new_parent(&ptr->child_boxes, GF_ISOM_BOX_TYPE_DINF);
		if (!ptr->dataInformation) return GF_OUT_OF_MEM;

		ptr->dataInformation->dref = (GF_DataReferenceBox *) gf_isom_box_new_parent(&ptr->dataInformation->child_boxes, GF_ISOM_BOX_TYPE_DREF);
		if (!ptr->dataInformation->dref) return GF_OUT_OF_MEM;

		url = gf_isom_box_new_parent(&ptr->dataInformation->dref->child_boxes, GF_ISOM_BOX_TYPE_URL);
		if (!url) return GF_OUT_OF_MEM;
		((GF_FullBox*)url)->flags = 1;
	}
	return e;
}

GF_Box *minf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MediaInformationBox, GF_ISOM_BOX_TYPE_MINF);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err minf_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err minf_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MediaInformationBox *ptr = (GF_MediaInformationBox *)s;
	//Header first
	gf_isom_check_position(s, (GF_Box *)ptr->InfoHeader, &pos);
	//then dataInfo
	gf_isom_check_position(s, (GF_Box *)ptr->dataInformation, &pos);
	gf_isom_check_position(s, gf_isom_box_find_child(s->child_boxes, GF_ISOM_BOX_TYPE_MVCI), &pos);
	//then sampleTable
	gf_isom_check_position(s, (GF_Box *)ptr->sampleTable, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void moof_box_del(GF_Box *s)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	if (ptr == NULL) return;

	gf_list_del(ptr->TrackList);
	if (ptr->PSSHs) gf_list_del(ptr->PSSHs);
	if (ptr->mdat) gf_free(ptr->mdat);
	gf_free(ptr);
}

GF_Err moof_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MFHD:
		BOX_FIELD_ASSIGN(mfhd, GF_MovieFragmentHeaderBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRAF:
		BOX_FIELD_LIST_ASSIGN(TrackList)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_PSSH:
		BOX_FIELD_LIST_ASSIGN(PSSHs)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err moof_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *moof_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieFragmentBox, GF_ISOM_BOX_TYPE_MOOF);
	tmp->TrackList = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err moof_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err moof_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MovieFragmentBox *ptr = (GF_MovieFragmentBox *) s;
	if (!s) return GF_BAD_PARAM;
	//Header First
	gf_isom_check_position(s, (GF_Box *)ptr->mfhd, &pos);
	//then PSSH
	gf_isom_check_position_list(s, ptr->PSSHs, &pos);
	//then the track list
	gf_isom_check_position_list(s, ptr->TrackList, &pos);
	return GF_OK;
}


#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

void moov_box_del(GF_Box *s)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	if (ptr == NULL) return;
	gf_list_del(ptr->trackList);
	gf_free(ptr);
}

GF_Err moov_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MovieBox *ptr = (GF_MovieBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IODS:
		BOX_FIELD_ASSIGN(iods, GF_ObjectDescriptorBox)
		//if no IOD, delete the box
		if (ptr->iods && !ptr->iods->descriptor) {
			ptr->iods = NULL;
			gf_isom_box_del_parent(&s->child_boxes, a);
		}
		return GF_OK;

	case GF_ISOM_BOX_TYPE_MVHD:
		BOX_FIELD_ASSIGN(mvhd, GF_MovieHeaderBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_UDTA:
		BOX_FIELD_ASSIGN(udta, GF_UserDataBox)
		return GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MVEX:
		BOX_FIELD_ASSIGN(mvex, GF_MovieExtendsBox)
		if (ptr->mvex)
			ptr->mvex->mov = ptr->mov;
		return GF_OK;
#endif

	case GF_ISOM_BOX_TYPE_META:
		BOX_FIELD_ASSIGN(meta, GF_MetaBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_TRAK:
		if (is_rem) {
			gf_list_del_item(ptr->trackList, a);
			return GF_OK;
		}
		{
			GF_TrackBox *tk = (GF_TrackBox *)a;
			//set our pointer to this obj
			tk->moov = ptr;
			tk->index = gf_list_count(ptr->trackList);
			if (tk->References) {
				GF_TrackReferenceTypeBox *dpnd=NULL;
				Track_FindRef(tk, GF_ISOM_REF_BASE, &dpnd);
				if (dpnd)
					tk->nb_base_refs = dpnd->trackIDCount;
			}
		}
		return gf_list_add(ptr->trackList, a);
	}
	return GF_OK;
}


GF_Err moov_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *moov_box_new()
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


GF_Err moov_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err moov_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MovieBox *ptr = (GF_MovieBox *)s;

	gf_isom_check_position(s, (GF_Box *) ptr->mvhd, &pos);
	gf_isom_check_position(s, (GF_Box *) ptr->iods, &pos);
	gf_isom_check_position(s, (GF_Box *) ptr->meta, &pos);
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->mvex && !ptr->mvex_after_traks) {
		gf_isom_check_position(s, (GF_Box *) ptr->mvex, &pos);
	}
#endif
	gf_isom_check_position_list(s, ptr->trackList, &pos);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (ptr->mvex && ptr->mvex_after_traks) {
		gf_isom_check_position(s, (GF_Box *) ptr->mvex, &pos);
	}
#endif
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void audio_sample_entry_box_del(GF_Box *s)
{
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	gf_free(ptr);
}

GF_Err audio_sample_entry_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_UnknownBox *wave = NULL;
	Bool drop_wave=GF_FALSE;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		BOX_FIELD_ASSIGN(esd, GF_ESDBox)
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;

	case GF_ISOM_BOX_TYPE_DAMR:
	case GF_ISOM_BOX_TYPE_DEVC:
	case GF_ISOM_BOX_TYPE_DQCP:
	case GF_ISOM_BOX_TYPE_DSMV:
		BOX_FIELD_ASSIGN(cfg_3gpp, GF_3GPPConfigBox)
		/*for 3GP config, remember sample entry type in config*/
		ptr->cfg_3gpp->cfg.type = ptr->type;
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;

	case GF_ISOM_BOX_TYPE_DOPS:
		BOX_FIELD_ASSIGN(cfg_opus, GF_OpusSpecificBox)
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;
	case GF_ISOM_BOX_TYPE_DAC3:
		BOX_FIELD_ASSIGN(cfg_ac3, GF_AC3ConfigBox)
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;
	case GF_ISOM_BOX_TYPE_DEC3:
		BOX_FIELD_ASSIGN(cfg_ac3, GF_AC3ConfigBox)
		break;
	case GF_ISOM_BOX_TYPE_DMLP:
		BOX_FIELD_ASSIGN(cfg_mlp, GF_TrueHDConfigBox)
		break;
	case GF_ISOM_BOX_TYPE_MHAC:
		BOX_FIELD_ASSIGN(cfg_mha, GF_MHAConfigBox)
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;
	case GF_ISOM_BOX_TYPE_DFLA:
		BOX_FIELD_ASSIGN(cfg_flac, GF_FLACConfigBox)
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		break;

	case GF_ISOM_BOX_TYPE_UNKNOWN:
		wave = (GF_UnknownBox *)a;
		/*HACK for QT files: get the esds box from the track*/
		if (s->type == GF_ISOM_BOX_TYPE_MP4A) {
			if (is_rem) {
				return GF_OK;
			}
			if (ptr->esd) ERROR_ON_DUPLICATED_BOX(a, ptr)
			//wave subboxes may have been properly parsed
 			if ((wave->original_4cc == GF_QT_BOX_TYPE_WAVE) && gf_list_count(wave->child_boxes)) {
 				u32 i;
                for (i =0; i<gf_list_count(wave->child_boxes); i++) {
                    GF_Box *inner_box = (GF_Box *)gf_list_get(wave->child_boxes, i);
                    if (inner_box->type == GF_ISOM_BOX_TYPE_ESDS) {
                        ptr->esd = (GF_ESDBox *)inner_box;
 						if (ptr->qtff_mode & GF_ISOM_AUDIO_QTFF_CONVERT_FLAG) {
                        	gf_list_rem(a->child_boxes, i);
                        	drop_wave=GF_TRUE;
                        	ptr->compression_id = 0;
                        	gf_list_add(ptr->child_boxes, inner_box);
						}
                    }
                }
				if (drop_wave) {
					gf_isom_box_del_parent(&ptr->child_boxes, a);
                	ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
					ptr->version = 0;
					return GF_OK;
				}
                ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_EXT_VALID;
                return GF_OK;
            }
            gf_isom_box_del_parent(&ptr->child_boxes, a);
            return GF_ISOM_INVALID_MEDIA;

		}
 		ptr->qtff_mode &= ~GF_ISOM_AUDIO_QTFF_CONVERT_FLAG;

 		if ((wave->original_4cc == GF_QT_BOX_TYPE_WAVE) && gf_list_count(wave->child_boxes)) {
			ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_NOEXT;
		}
		return GF_OK;
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
		else if ((s->type == GF_ISOM_BOX_TYPE_MHA1)
			|| (s->type == GF_ISOM_BOX_TYPE_MHA2)
			|| (s->type == GF_ISOM_BOX_TYPE_MHM1)
			|| (s->type == GF_ISOM_BOX_TYPE_MHM2)
		) {
			cfg_ptr = (GF_Box **) &ptr->cfg_mha;
			subtype = GF_ISOM_BOX_TYPE_MHAC;
		}
		else if (s->type == GF_ISOM_BOX_TYPE_MLPA) {
			cfg_ptr = (GF_Box **) &ptr->cfg_mlp;
			subtype = GF_ISOM_BOX_TYPE_DMLP;
		}

		if (cfg_ptr) {
			if (is_rem) {
				*cfg_ptr = NULL;
				return GF_OK;
			}
			if (*cfg_ptr) ERROR_ON_DUPLICATED_BOX(a, ptr)

			//wave subboxes may have been properly parsed
 			if (gf_list_count(a->child_boxes)) {
 				u32 i;
                for (i =0; i<gf_list_count(a->child_boxes); i++) {
                    GF_Box *inner_box = (GF_Box *)gf_list_get(a->child_boxes, i);
                    if (inner_box->type == subtype) {
                        *cfg_ptr = inner_box;
 						if (ptr->qtff_mode & GF_ISOM_AUDIO_QTFF_CONVERT_FLAG) {
                        	gf_list_rem(a->child_boxes, i);
                        	drop_wave=GF_TRUE;
                        	gf_list_add(ptr->child_boxes, inner_box);
						}
						break;
                    }
                }
				if (drop_wave) {
					gf_isom_box_del_parent(&ptr->child_boxes, a);
                	ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
					ptr->compression_id = 0;
					ptr->version = 0;
					return GF_OK;
				}
                ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_EXT_VALID;
                return GF_OK;
            }
		}
	}
 		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_EXT_VALID;
		return GF_OK;
	}
	return GF_OK;
}
GF_Err audio_sample_entry_box_read(GF_Box *s, GF_BitStream *bs)
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
		ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_NOEXT;

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
		if (nb_alnum>2) ptr->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
	}

	gf_bs_seek(bs, start);
	e = gf_isom_audio_sample_entry_read((GF_AudioSampleEntryBox*)s, bs);
	if (e) return e;
	pos = gf_bs_get_position(bs);
	size = (u32) s->size;

	//when cookie is set on bs, always convert qtff-style mp4a to isobmff-style
	//since the conversion is done in addBox and we don't have the bitstream there (arg...), flag the box
 	if (gf_bs_get_cookie(bs) & GF_ISOM_BS_COOKIE_QT_CONV) {
 		ptr->qtff_mode |= GF_ISOM_AUDIO_QTFF_CONVERT_FLAG;
 	}

	e = gf_isom_box_array_read(s, bs);
	if (!e) {
		if (s->type==GF_ISOM_BOX_TYPE_ENCA) {
			GF_ProtectionSchemeInfoBox *sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(s->child_boxes, GF_ISOM_BOX_TYPE_SINF);

			if (sinf && sinf->original_format) {
				u32 type = sinf->original_format->data_format;
				switch (type) {
				case GF_ISOM_SUBTYPE_3GP_AMR:
				case GF_ISOM_SUBTYPE_3GP_AMR_WB:
				case GF_ISOM_SUBTYPE_3GP_EVRC:
				case GF_ISOM_SUBTYPE_3GP_QCELP:
				case GF_ISOM_SUBTYPE_3GP_SMV:
					if (ptr->cfg_3gpp) ptr->cfg_3gpp->cfg.type = type;
					break;
				}
			}
		}
		return GF_OK;
	}
	if (size<8) return GF_ISOM_INVALID_FILE;


	/*hack for some weird files (possibly recorded with live.com tools, needs further investigations)*/
	gf_bs_seek(bs, pos);
	data = (char*)gf_malloc(sizeof(char) * size);
	if (!data) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, data, size);
	for (i=0; i<size-8; i++) {
		if (GF_4CC((u32)data[i+4], (u8)data[i+5], (u8)data[i+6], (u8)data[i+7]) == GF_ISOM_BOX_TYPE_ESDS) {
			GF_BitStream *mybs = gf_bs_new(data + i, size - i, GF_BITSTREAM_READ);
			if (ptr->esd) gf_isom_box_del_parent(&ptr->child_boxes, (GF_Box *)ptr->esd);
			ptr->esd = NULL;
			e = gf_isom_box_parse((GF_Box **)&ptr->esd, mybs);
			gf_bs_del(mybs);

			if ((e==GF_OK) && (ptr->esd->type == GF_ISOM_BOX_TYPE_ESDS)) {
				if (!ptr->child_boxes) ptr->child_boxes = gf_list_new();
				gf_list_add(ptr->child_boxes, ptr->esd);
			} else if (ptr->esd) {
				gf_isom_box_del((GF_Box *)ptr->esd);
				ptr->esd = NULL;
			}
			break;
		}
	}
	gf_free(data);
	return e;
}

GF_Box *audio_sample_entry_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGAudioSampleEntryBox, GF_ISOM_BOX_TYPE_MP4A);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}

GF_Box *enca_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGAudioSampleEntryBox, GF_ISOM_BOX_TYPE_ENCA);
	gf_isom_audio_sample_entry_init((GF_AudioSampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err audio_sample_entry_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_audio_sample_entry_write((GF_AudioSampleEntryBox*)s, bs);
	return GF_OK;
}

GF_Err audio_sample_entry_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MPEGAudioSampleEntryBox *ptr = (GF_MPEGAudioSampleEntryBox *)s;
	gf_isom_audio_sample_entry_size((GF_AudioSampleEntryBox*)s);
	if (ptr->qtff_mode)
		return GF_OK;

	gf_isom_check_position(s, (GF_Box *)ptr->esd, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_3gpp, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_opus, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_ac3, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_flac, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_mlp, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gen_sample_entry_box_del(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(ptr);
}


GF_Err gen_sample_entry_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)s, bs);
	if (e) return e;
	ISOM_DECREASE_SIZE(s, 8);
	return gf_isom_box_array_read(s, bs);
}

GF_Box *gen_sample_entry_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleEntryBox, GF_QT_SUBTYPE_C608);//type will be overriten
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gen_sample_entry_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	return GF_OK;
}

GF_Err gen_sample_entry_box_size(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	ptr->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mp4s_box_del(GF_Box *s)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	gf_free(ptr);
}

GF_Err mp4s_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		BOX_FIELD_ASSIGN(esd, GF_ESDBox)
		break;
	}
	return GF_OK;
}

GF_Err mp4s_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(ptr, 8);
	return gf_isom_box_array_read(s, bs);
}

GF_Box *mp4s_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGSampleEntryBox, GF_ISOM_BOX_TYPE_MP4S);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	tmp->internal_type = GF_ISOM_SAMPLE_ENTRY_MP4S;
	return (GF_Box *)tmp;
}

GF_Box *encs_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGSampleEntryBox, GF_ISOM_BOX_TYPE_ENCS);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	tmp->internal_type = GF_ISOM_SAMPLE_ENTRY_MP4S;
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mp4s_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
    return GF_OK;
}

GF_Err mp4s_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MPEGSampleEntryBox *ptr = (GF_MPEGSampleEntryBox *)s;
	s->size += 8;
	gf_isom_check_position(s, (GF_Box *)ptr->esd, &pos);
    return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void video_sample_entry_box_del(GF_Box *s)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	/*for publishing*/
	if (ptr->emul_esd) gf_odf_desc_del((GF_Descriptor *)ptr->emul_esd);
	gf_free(ptr);
}

GF_Err video_sample_entry_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_ESDS:
		BOX_FIELD_ASSIGN(esd, GF_ESDBox)
		break;
	case GF_ISOM_BOX_TYPE_RINF:
		BOX_FIELD_ASSIGN(rinf, GF_RestrictedSchemeInfoBox)
		break;
	case GF_ISOM_BOX_TYPE_AVCC:
		BOX_FIELD_ASSIGN(avc_config, GF_AVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_HVCC:
		BOX_FIELD_ASSIGN(hevc_config, GF_HEVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_VVCC:
		BOX_FIELD_ASSIGN(vvc_config, GF_VVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_SVCC:
		BOX_FIELD_ASSIGN(svc_config, GF_AVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_MVCC:
		BOX_FIELD_ASSIGN(mvc_config, GF_AVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_LHVC:
		BOX_FIELD_ASSIGN(lhvc_config, GF_HEVCConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_AV1C:
		BOX_FIELD_ASSIGN(av1_config, GF_AV1ConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_VPCC:
		BOX_FIELD_ASSIGN(vp_config, GF_VPConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_DVCC:
	case GF_ISOM_BOX_TYPE_DVVC:
		BOX_FIELD_ASSIGN(dovi_config, GF_DOVIConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_UUID:
		if (! memcmp(((GF_UnknownUUIDBox*)a)->uuid, GF_ISOM_IPOD_EXT, 16)) {
			BOX_FIELD_ASSIGN(ipod_ext, GF_UnknownUUIDBox)
		} else {
			return GF_OK;
		}
		break;
	case GF_ISOM_BOX_TYPE_D263:
		BOX_FIELD_ASSIGN(cfg_3gpp, GF_3GPPConfigBox)
		/*for 3GP config, remember sample entry type in config*/
		if (ptr->cfg_3gpp)
			ptr->cfg_3gpp->cfg.type = ptr->type;
		break;

	case GF_ISOM_BOX_TYPE_JP2H:
		BOX_FIELD_ASSIGN(jp2h, GF_J2KHeaderBox)
		return GF_OK;

	case GF_ISOM_BOX_TYPE_PASP:
	case GF_ISOM_BOX_TYPE_CLAP:
	case GF_ISOM_BOX_TYPE_COLR:
	case GF_ISOM_BOX_TYPE_MDCV:
	case GF_ISOM_BOX_TYPE_CLLI:
	case GF_ISOM_BOX_TYPE_CCST:
	case GF_ISOM_BOX_TYPE_AUXI:
	case GF_ISOM_BOX_TYPE_RVCC:
	case GF_ISOM_BOX_TYPE_M4DS:
		if (!is_rem && !gf_isom_box_check_unique(s->child_boxes, a)) {
			ERROR_ON_DUPLICATED_BOX(a, ptr)
		}
		return GF_OK;
	}
	return GF_OK;
}

GF_Err video_sample_entry_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEGVisualSampleEntryBox *mp4v = (GF_MPEGVisualSampleEntryBox*)s;
	GF_Err e;
	e = gf_isom_video_sample_entry_read((GF_VisualSampleEntryBox *)s, bs);
	if (e) return e;
	e = gf_isom_box_array_read(s, bs);
	if (e) return e;
	/*this is an AVC sample desc*/
	if (mp4v->avc_config || mp4v->svc_config || mp4v->mvc_config)
		AVC_RewriteESDescriptor(mp4v);
	/*this is an HEVC sample desc*/
	if (mp4v->hevc_config || mp4v->lhvc_config || (mp4v->type==GF_ISOM_BOX_TYPE_HVT1))
		HEVC_RewriteESDescriptor(mp4v);
	/*this is an AV1 sample desc*/
	if (mp4v->av1_config)
		AV1_RewriteESDescriptor(mp4v);
	/*this is a VP8-9 sample desc*/
	if (mp4v->vp_config)
		VP9_RewriteESDescriptor(mp4v);

	if (s->type==GF_ISOM_BOX_TYPE_ENCV) {
		GF_ProtectionSchemeInfoBox *sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(s->child_boxes, GF_ISOM_BOX_TYPE_SINF);

		if (sinf && sinf->original_format) {
			u32 type = sinf->original_format->data_format;
			switch (type) {
			case GF_ISOM_SUBTYPE_3GP_H263:
				if (mp4v->cfg_3gpp) mp4v->cfg_3gpp->cfg.type = type;
				break;
			}
		}
	}
	return GF_OK;
}

GF_Box *video_sample_entry_box_new()
{
	GF_MPEGVisualSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_MPEGVisualSampleEntryBox);
	if (tmp == NULL) return NULL;

	gf_isom_video_sample_entry_init((GF_VisualSampleEntryBox *)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err video_sample_entry_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_isom_video_sample_entry_write((GF_VisualSampleEntryBox *)s, bs);
	return GF_OK;
}

GF_Err video_sample_entry_box_size(GF_Box *s)
{
	GF_Box *b;
	u32 pos=0;
	GF_MPEGVisualSampleEntryBox *ptr = (GF_MPEGVisualSampleEntryBox *)s;
	gf_isom_video_sample_entry_size((GF_VisualSampleEntryBox *)s);

	/*make sure we write the config box first, we don't care about the rest*/

	/*mp4v*/
	gf_isom_check_position(s, (GF_Box *)ptr->esd, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->cfg_3gpp, &pos);
	/*avc / SVC + MVC*/
	gf_isom_check_position(s, (GF_Box *)ptr->avc_config, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->svc_config, &pos);
	if (ptr->mvc_config) {
		gf_isom_check_position(s, gf_isom_box_find_child(s->child_boxes, GF_ISOM_BOX_TYPE_VWID), &pos);
		gf_isom_check_position(s, (GF_Box *)ptr->mvc_config, &pos);
	}

	/*HEVC*/
	gf_isom_check_position(s, (GF_Box *)ptr->hevc_config, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->lhvc_config, &pos);

	/*VVC*/
	gf_isom_check_position(s, (GF_Box *)ptr->vvc_config, &pos);
	
	/*AV1*/
	gf_isom_check_position(s, (GF_Box *)ptr->av1_config, &pos);

	/*VPx*/
	gf_isom_check_position(s, (GF_Box *)ptr->vp_config, &pos);

	/*JP2H*/
	gf_isom_check_position(s, (GF_Box *)ptr->jp2h, &pos);

	/*DolbyVision*/
	gf_isom_check_position(s, (GF_Box *)ptr->dovi_config, &pos);

	b = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_ST3D);
	if (b) gf_isom_check_position(s, b, &pos);

	b = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_SV3D);
	if (b) gf_isom_check_position(s, b, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void mvex_box_del(GF_Box *s)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;
	if (ptr == NULL) return;
	gf_list_del(ptr->TrackExList);
	gf_list_del(ptr->TrackExPropList);
	gf_free(ptr);
}


GF_Err mvex_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *)s;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TREX:
		BOX_FIELD_LIST_ASSIGN(TrackExList)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TREP:
		BOX_FIELD_LIST_ASSIGN(TrackExPropList)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_MEHD:
		BOX_FIELD_ASSIGN(mehd, GF_MovieExtendsHeaderBox)
		return GF_OK;
	}
	return GF_OK;
}



GF_Err mvex_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

GF_Box *mvex_box_new()
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


GF_Err mvex_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err mvex_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_MovieExtendsBox *ptr = (GF_MovieExtendsBox *) s;
	gf_isom_check_position(s, (GF_Box *)ptr->mehd, &pos);
	gf_isom_check_position_list(s, ptr->TrackExList, &pos);
	gf_isom_check_position_list(s, ptr->TrackExPropList, &pos);
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *mehd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MovieExtendsHeaderBox, GF_ISOM_BOX_TYPE_MEHD);
	return (GF_Box *)tmp;
}
void mehd_box_del(GF_Box *s)
{
	gf_free(s);
}
GF_Err mehd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;

	if (ptr->version==1) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->fragment_duration = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->fragment_duration = (u64) gf_bs_read_u32(bs);
	}
	return GF_OK;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err mehd_box_write(GF_Box *s, GF_BitStream *bs)
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
GF_Err mehd_box_size(GF_Box *s)
{
	GF_MovieExtendsHeaderBox *ptr = (GF_MovieExtendsHeaderBox *)s;
	ptr->version = (ptr->fragment_duration>0xFFFFFFFF) ? 1 : 0;
	s->size += (ptr->version == 1) ? 8 : 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void mvhd_box_del(GF_Box *s)
{
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err mvhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	if (ptr->version == 1) {
		ISOM_DECREASE_SIZE(ptr, 28);
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 16);
		ptr->creationTime = gf_bs_read_u32(bs);
		ptr->modificationTime = gf_bs_read_u32(bs);
		ptr->timeScale = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u32(bs);
	}
	if (!ptr->timeScale) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Movie header timescale is invalid (0) - defaulting to 600\n" ));
		ptr->timeScale = 600;
	}
	ISOM_DECREASE_SIZE(ptr, 80);
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

GF_Box *mvhd_box_new()
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

GF_Err mvhd_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err mvhd_box_size(GF_Box *s)
{
	GF_MovieHeaderBox *ptr = (GF_MovieHeaderBox *)s;
	if (ptr->duration==(u64) -1) ptr->version = 0;
	else ptr->version = (ptr->duration>0xFFFFFFFF) ? 1 : 0;

	ptr->size += (ptr->version == 1) ? 28 : 16;
	ptr->size += 80;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void nmhd_box_del(GF_Box *s)
{
	GF_MPEGMediaHeaderBox *ptr = (GF_MPEGMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}



GF_Err nmhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	return GF_OK;
}

GF_Box *nmhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MPEGMediaHeaderBox, GF_ISOM_BOX_TYPE_NMHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err nmhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_full_box_write(s, bs);
}

GF_Err nmhd_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void padb_box_del(GF_Box *s)
{
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *) s;
	if (ptr == NULL) return;
	if (ptr->padbits) gf_free(ptr->padbits);
	gf_free(ptr);
}


GF_Err padb_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->SampleCount = gf_bs_read_u32(bs);
	if (ptr->size < ptr->SampleCount/2) //half byte per sample
		return GF_ISOM_INVALID_FILE;

	ptr->padbits = (u8 *)gf_malloc(sizeof(u8)*ptr->SampleCount);
	if (!ptr->padbits) return GF_OUT_OF_MEM;

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

GF_Box *padb_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PaddingBitsBox, GF_ISOM_BOX_TYPE_PADB);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err padb_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err padb_box_size(GF_Box *s)
{
	GF_PaddingBitsBox *ptr = (GF_PaddingBitsBox *)s;
	ptr->size += 4;
	if (ptr->SampleCount) ptr->size += (ptr->SampleCount + 1) / 2;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void rely_box_del(GF_Box *s)
{
	GF_RelyHintBox *rely = (GF_RelyHintBox *)s;
	gf_free(rely);
}

GF_Err rely_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_RelyHintBox *ptr = (GF_RelyHintBox *)s;
	ISOM_DECREASE_SIZE(ptr, 1);
	ptr->reserved = gf_bs_read_int(bs, 6);
	ptr->preferred = gf_bs_read_int(bs, 1);
	ptr->required = gf_bs_read_int(bs, 1);
	return GF_OK;
}

GF_Box *rely_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_RelyHintBox, GF_ISOM_BOX_TYPE_RELY);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rely_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RelyHintBox *ptr = (GF_RelyHintBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->reserved, 6);
	gf_bs_write_int(bs, ptr->preferred, 1);
	gf_bs_write_int(bs, ptr->required, 1);
	return GF_OK;
}

GF_Err rely_box_size(GF_Box *s)
{
	s->size += 1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void rtpo_box_del(GF_Box *s)
{
	GF_RTPOBox *rtpo = (GF_RTPOBox *)s;
	gf_free(rtpo);
}

GF_Err rtpo_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_RTPOBox *ptr = (GF_RTPOBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->timeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *rtpo_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_RTPOBox, GF_ISOM_BOX_TYPE_RTPO);
	return (GF_Box *)tmp;
}
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rtpo_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err rtpo_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

void smhd_box_del(GF_Box *s)
{
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	if (ptr == NULL ) return;
	gf_free(ptr);
}


GF_Err smhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->balance = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *smhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SoundMediaHeaderBox, GF_ISOM_BOX_TYPE_SMHD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err smhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->balance);
	gf_bs_write_u16(bs, ptr->reserved);
	return GF_OK;
}

GF_Err smhd_box_size(GF_Box *s)
{
	GF_SoundMediaHeaderBox *ptr = (GF_SoundMediaHeaderBox *)s;

	ptr->reserved = 0;
	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void snro_box_del(GF_Box *s)
{
	GF_SeqOffHintEntryBox *snro = (GF_SeqOffHintEntryBox *)s;
	gf_free(snro);
}

GF_Err snro_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SeqOffHintEntryBox *ptr = (GF_SeqOffHintEntryBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->SeqOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *snro_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SeqOffHintEntryBox, GF_ISOM_BOX_TYPE_SNRO);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err snro_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SeqOffHintEntryBox *ptr = (GF_SeqOffHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->SeqOffset);
	return GF_OK;
}

GF_Err snro_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stbl_box_del(GF_Box *s)
{
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	if (ptr == NULL) return;

	if (ptr->sub_samples) gf_list_del(ptr->sub_samples);
	if (ptr->sampleGroups) gf_list_del(ptr->sampleGroups);
	if (ptr->sampleGroupsDescription) gf_list_del(ptr->sampleGroupsDescription);
	if (ptr->sai_sizes) gf_list_del(ptr->sai_sizes);
	if (ptr->sai_offsets) gf_list_del(ptr->sai_offsets);
	if (ptr->traf_map) {
		if (ptr->traf_map->frag_starts) {
			u32 i;
			for (i=0; i<ptr->traf_map->nb_entries; i++) {
				if (ptr->traf_map->frag_starts[i].moof_template)
					gf_free(ptr->traf_map->frag_starts[i].moof_template);
			}
			gf_free(ptr->traf_map->frag_starts);
		}
		gf_free(ptr->traf_map);
	}
	gf_free(ptr);
}

GF_Err stbl_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;
	if (!a) return GF_OK;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_STTS:
		BOX_FIELD_ASSIGN(TimeToSample, GF_TimeToSampleBox)
		break;
	case GF_ISOM_BOX_TYPE_CTTS:
		BOX_FIELD_ASSIGN(CompositionOffset, GF_CompositionOffsetBox)
		break;
	case GF_ISOM_BOX_TYPE_CSLG:
		BOX_FIELD_ASSIGN(CompositionToDecode, GF_CompositionToDecodeBox)
		break;
	case GF_ISOM_BOX_TYPE_STSS:
		BOX_FIELD_ASSIGN(SyncSample, GF_SyncSampleBox)
		break;
	case GF_ISOM_BOX_TYPE_STSD:
		BOX_FIELD_ASSIGN(SampleDescription, GF_SampleDescriptionBox)
		break;
	case GF_ISOM_BOX_TYPE_STZ2:
	case GF_ISOM_BOX_TYPE_STSZ:
		BOX_FIELD_ASSIGN(SampleSize, GF_SampleSizeBox)
		break;
	case GF_ISOM_BOX_TYPE_STSC:
		BOX_FIELD_ASSIGN(SampleToChunk, GF_SampleToChunkBox)
		break;
	case GF_ISOM_BOX_TYPE_PADB:
		BOX_FIELD_ASSIGN(PaddingBits, GF_PaddingBitsBox)
		break;

	//WARNING: AS THIS MAY CHANGE DYNAMICALLY DURING EDIT,
	case GF_ISOM_BOX_TYPE_CO64:
	case GF_ISOM_BOX_TYPE_STCO:
		BOX_FIELD_ASSIGN(ChunkOffset, GF_Box)
		break;
	case GF_ISOM_BOX_TYPE_STSH:
		BOX_FIELD_ASSIGN(ShadowSync, GF_ShadowSyncBox)
		break;
	case GF_ISOM_BOX_TYPE_STDP:
		BOX_FIELD_ASSIGN(DegradationPriority, GF_DegradationPriorityBox)
		break;
	case GF_ISOM_BOX_TYPE_SDTP:
		BOX_FIELD_ASSIGN(SampleDep, GF_SampleDependencyTypeBox)
		break;

	case GF_ISOM_BOX_TYPE_SUBS:
		BOX_FIELD_LIST_ASSIGN(sub_samples)
		//check subsample box
		if (!is_rem) {
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
	case GF_ISOM_BOX_TYPE_CSGP:
		BOX_FIELD_LIST_ASSIGN(sampleGroups)
		break;
	case GF_ISOM_BOX_TYPE_SGPD:
		BOX_FIELD_LIST_ASSIGN(sampleGroupsDescription)
		break;
	case GF_ISOM_BOX_TYPE_SAIZ:
		BOX_FIELD_LIST_ASSIGN(sai_sizes)
		break;
	case GF_ISOM_BOX_TYPE_SAIO:
		BOX_FIELD_LIST_ASSIGN(sai_offsets)
		break;
	}
	return GF_OK;
}




GF_Err stbl_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	//we need to parse DegPrior in a special way
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;

	e = gf_isom_box_array_read(s, bs);
	if (e) return e;

	if (!ptr->SyncSample)
		ptr->no_sync_found = 1;

	ptr->nb_sgpd_in_stbl = gf_list_count(ptr->sampleGroupsDescription);
	ptr->nb_stbl_boxes = gf_list_count(ptr->child_boxes);

	if (gf_bs_get_cookie(bs) & GF_ISOM_BS_COOKIE_CLONE_TRACK)
		return GF_OK;
//	return GF_OK;

#define CHECK_BOX(_name) \
	if (!ptr->_name) {\
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Mandatory box %s is missing\n", #_name)); \
		return GF_ISOM_INVALID_FILE; \
	}

	CHECK_BOX(SampleToChunk)
	CHECK_BOX(SampleSize)
	CHECK_BOX(ChunkOffset)
	CHECK_BOX(TimeToSample)

	//sanity check
	if (ptr->SampleSize->sampleCount) {
		if (!ptr->TimeToSample->nb_entries || !ptr->SampleToChunk->nb_entries)
			return GF_ISOM_INVALID_FILE;
	}
	u32 i, max_chunks=0;
	if (ptr->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		max_chunks = ((GF_ChunkOffsetBox *)ptr->ChunkOffset)->nb_entries;
	}
	else if (ptr->ChunkOffset->type == GF_ISOM_BOX_TYPE_CO64) {
		max_chunks = ((GF_ChunkOffsetBox *)ptr->ChunkOffset)->nb_entries;
	}

	//sanity check on stsc vs chunk offset tables
	for (i=0; i<ptr->SampleToChunk->nb_entries; i++) {
		GF_StscEntry *ent = &ptr->SampleToChunk->entries[i];
		if (!i && (ent->firstChunk!=1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] first_chunk of first entry shall be 1 but is %u\n", ent->firstChunk));
			return GF_ISOM_INVALID_FILE;
		}
		if (ptr->SampleToChunk->entries[i].firstChunk > max_chunks) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] first_chunk is %u but number of chunks defined %u\n", ptr->SampleToChunk->entries[i].firstChunk, max_chunks));
			return GF_ISOM_INVALID_FILE;
		}
		if (i+1 == ptr->SampleToChunk->nb_entries) break;
		GF_StscEntry *next_ent = &ptr->SampleToChunk->entries[i+1];
		if (next_ent->firstChunk < ent->firstChunk) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] first_chunk (%u) for entry %u is greater than first_chunk (%u) for entry %u\n", i+1, ent->firstChunk, i+2, next_ent->firstChunk));
			return GF_ISOM_INVALID_FILE;
		}
	}
	return GF_OK;
}

GF_Box *stbl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleTableBox, GF_ISOM_BOX_TYPE_STBL);
	//maxSamplePer chunk is 10 by default
	tmp->MaxSamplePerChunk = 10;
	tmp->groupID = 1;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stbl_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err stbl_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_SampleTableBox *ptr = (GF_SampleTableBox *)s;

	gf_isom_check_position(s, (GF_Box *)ptr->SampleDescription, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->TimeToSample, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->CompositionOffset, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->CompositionToDecode, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->SyncSample, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->ShadowSync, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->SampleToChunk, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->SampleSize, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->ChunkOffset, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->DegradationPriority, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->SampleDep, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->PaddingBits, &pos);

	if (ptr->sub_samples) {
		gf_isom_check_position_list(s, ptr->sub_samples, &pos);
	}
	if (ptr->sampleGroupsDescription) {
		gf_isom_check_position_list(s, ptr->sampleGroupsDescription, &pos);
	}
	if (ptr->sampleGroups) {
		gf_isom_check_position_list(s, ptr->sampleGroups, &pos);
	}
	if (ptr->sai_sizes) {
		gf_isom_check_position_list(s, ptr->sai_sizes, &pos);
	}
	if (ptr->sai_offsets) {
		gf_isom_check_position_list(s, ptr->sai_offsets, &pos);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stco_box_del(GF_Box *s)
{
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	gf_free(ptr);
}


GF_Err stco_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 entries;
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nb_entries = gf_bs_read_u32(bs);
	if (ptr->nb_entries > ptr->size / 4 || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(u32)) {
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

GF_Box *stco_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChunkOffsetBox, GF_ISOM_BOX_TYPE_STCO);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stco_box_write(GF_Box *s, GF_BitStream *bs)
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


GF_Err stco_box_size(GF_Box *s)
{
	GF_ChunkOffsetBox *ptr = (GF_ChunkOffsetBox *)s;

	ptr->size += 4 + (4 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void stdp_box_del(GF_Box *s)
{
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;
	if (ptr == NULL ) return;
	if (ptr->priorities) gf_free(ptr->priorities);
	gf_free(ptr);
}

//this is called through stbl_read...
GF_Err stdp_box_read(GF_Box *s, GF_BitStream *bs)
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

GF_Box *stdp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DegradationPriorityBox, GF_ISOM_BOX_TYPE_STDP);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stdp_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stdp_box_size(GF_Box *s)
{
	GF_DegradationPriorityBox *ptr = (GF_DegradationPriorityBox *)s;

	ptr->size += (2 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stsc_box_del(GF_Box *s)
{
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}


GF_Err stsc_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nb_entries = gf_bs_read_u32(bs);

	if (ptr->nb_entries > ptr->size / 12 || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(GF_StscEntry)) {
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

GF_Box *stsc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleToChunkBox, GF_ISOM_BOX_TYPE_STSC);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsc_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stsc_box_size(GF_Box *s)
{
	GF_SampleToChunkBox *ptr = (GF_SampleToChunkBox *)s;

	ptr->size += 4 + (12 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stsd_box_del(GF_Box *s)
{
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err stsd_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_UnknownBox *def;
	if (!a) return GF_OK;

	if (is_rem || gf_box_valid_in_parent(a, "stsd")) {
		return GF_OK;
	}
	switch (a->type) {
	//unknown sample description: we need a specific box to handle the data ref index
	//rather than a default box ...
	case GF_ISOM_BOX_TYPE_UNKNOWN:
		def = (GF_UnknownBox *)a;
		/*we need at least 8 bytes for unknown sample entries*/
		if (def->dataSize < 8) {
			gf_isom_box_del_parent(&s->child_boxes, a);
			return GF_ISOM_INVALID_MEDIA;
		}
		return GF_OK;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Cannot process box of type %s\n", gf_4cc_to_str(a->type)));
		return GF_ISOM_INVALID_FILE;
	}
}


GF_Err stsd_box_read(GF_Box *s, GF_BitStream *bs)
{
	ISOM_DECREASE_SIZE(s, 4)
	gf_bs_read_u32(bs);

	return gf_isom_box_array_read_ex(s, bs, GF_ISOM_BOX_TYPE_STSD);
}

GF_Box *stsd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleDescriptionBox, GF_ISOM_BOX_TYPE_STSD);
	tmp->child_boxes = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 nb_entries;
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	nb_entries = gf_list_count(ptr->child_boxes);
	gf_bs_write_u32(bs, nb_entries);
	return GF_OK;
}

GF_Err stsd_box_size(GF_Box *s)
{
	GF_SampleDescriptionBox *ptr = (GF_SampleDescriptionBox *)s;
	ptr->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stsh_box_del(GF_Box *s)
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



GF_Err stsh_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count, i;
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;

	ISOM_DECREASE_SIZE(s, 4)
	count = gf_bs_read_u32(bs);
	if (ptr->size / 8 < count)
		return GF_ISOM_INVALID_FILE;

	for (i = 0; i < count; i++) {
		GF_StshEntry *ent = (GF_StshEntry *) gf_malloc(sizeof(GF_StshEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->shadowedSampleNumber = gf_bs_read_u32(bs);
		ent->syncSampleNumber = gf_bs_read_u32(bs);
		e = gf_list_add(ptr->entries, ent);
		if (e) return e;
	}
	return GF_OK;
}

GF_Box *stsh_box_new()
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

GF_Err stsh_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stsh_box_size(GF_Box *s)
{
	GF_ShadowSyncBox *ptr = (GF_ShadowSyncBox *)s;
	ptr->size += 4 + (8 * gf_list_count(ptr->entries));
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void stss_box_del(GF_Box *s)
{
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
	if (ptr == NULL) return;
	if (ptr->sampleNumbers) gf_free(ptr->sampleNumbers);
	gf_free(ptr);
}

GF_Err stss_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nb_entries = gf_bs_read_u32(bs);
	if (ptr->size / 4 <  ptr->nb_entries || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(u32)) {
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

GF_Box *stss_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SyncSampleBox, GF_ISOM_BOX_TYPE_STSS);
	return (GF_Box*)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stss_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stss_box_size(GF_Box *s)
{
	GF_SyncSampleBox *ptr = (GF_SyncSampleBox *)s;
	ptr->size += 4 + (4 * ptr->nb_entries);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void stsz_box_del(GF_Box *s)
{
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return;
	if (ptr->sizes) gf_free(ptr->sizes);
	gf_free(ptr);
}


GF_Err stsz_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, estSize;
	GF_SampleSizeBox *ptr = (GF_SampleSizeBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	//support for CompactSizes
	if (s->type == GF_ISOM_BOX_TYPE_STSZ) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->sampleSize = gf_bs_read_u32(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);
	} else {
		//24-reserved
		ISOM_DECREASE_SIZE(ptr, 8);
		gf_bs_read_int(bs, 24);
		i = gf_bs_read_u8(bs);
		ptr->sampleCount = gf_bs_read_u32(bs);
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
	if (ptr->sampleCount && (u64)ptr->sampleCount > (u64)SIZE_MAX/sizeof(u32)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsz\n", ptr->sampleCount));
		return GF_ISOM_INVALID_FILE;
	}
	if (s->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (! ptr->sampleSize && ptr->sampleCount) {
			if (ptr->sampleCount > ptr->size / 4) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in stsz\n", ptr->sampleCount));
				return GF_ISOM_INVALID_FILE;
			}
			ptr->sizes = (u32 *) gf_malloc(ptr->sampleCount * sizeof(u32));
			if (! ptr->sizes) return GF_OUT_OF_MEM;
			ptr->alloc_size = ptr->sampleCount;
			for (i = 0; i < ptr->sampleCount; i++) {
				ptr->sizes[i] = gf_bs_read_u32(bs);
				if (ptr->max_size < ptr->sizes[i])
					ptr->max_size = ptr->sizes[i];
				ptr->total_size += ptr->sizes[i];
				ptr->total_samples++;
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
			u32 s_size;
			switch (ptr->sampleSize) {
			case 4:
				s_size = ptr->sizes[i] = gf_bs_read_int(bs, 4);
				if (ptr->max_size < s_size)
					ptr->max_size = s_size;
				ptr->total_size += s_size;
				ptr->total_samples++;
				if (i+1 < ptr->sampleCount) {
					s_size = ptr->sizes[i+1] = gf_bs_read_int(bs, 4);
					if (ptr->max_size < s_size)
						ptr->max_size = s_size;
					ptr->total_size += s_size;
					ptr->total_samples++;
				} else {
					//0 padding in odd sample count
					gf_bs_read_int(bs, 4);
				}
				i += 2;
				break;
			default:
				s_size = ptr->sizes[i] = gf_bs_read_int(bs, ptr->sampleSize);
				if (ptr->max_size < s_size)
					ptr->max_size = s_size;
				ptr->total_size += s_size;
				ptr->total_samples++;
				i += 1;
				break;
			}
		}
	}
	return GF_OK;
}

GF_Box *stsz_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleSizeBox, 0);

	//type is unknown here, can be regular or compact table
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsz_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stsz_box_size(GF_Box *s)
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


void stts_box_del(GF_Box *s)
{
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}


GF_Err stts_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;

#ifndef GPAC_DISABLE_ISOM_WRITE
	ptr->w_LastDTS = 0;
#endif

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->nb_entries = gf_bs_read_u32(bs);
	if (ptr->size / 8 < ptr->nb_entries || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(GF_SttsEntry)) {
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
		if (ptr->max_ts_delta<ptr->entries[i].sampleDelta)
			ptr->max_ts_delta = ptr->entries[i].sampleDelta;

		if (!ptr->entries[i].sampleDelta) {
			if ((i+1<ptr->nb_entries) ) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Found stts entry with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			} else if (ptr->entries[i].sampleCount>1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] more than one stts entry at the end of the track with sample_delta=0 - forbidden ! Fixing to 1\n" ));
				ptr->entries[i].sampleDelta = 1;
			}
		}
		//cf issue 1644: some media streams may have sample duration > 2^31 (ttml mostly), we cannot patch this
		//for now we disable the check, one opt could be to have the check only for some media types, or only for the first entry
#if 0
		else if ((s32) ptr->entries[i].sampleDelta < 0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] stts entry %d has negative duration %d - forbidden ! Fixing to 1, sync may get lost (consider reimport raw media)\n", i, (s32) ptr->entries[i].sampleDelta ));
			ptr->entries[i].sampleDelta = 1;
		}
#endif

	}
	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries*8);

	//remove the last sample delta.
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (ptr->nb_entries) ptr->w_LastDTS -= ptr->entries[ptr->nb_entries-1].sampleDelta;
#endif
	return GF_OK;
}

GF_Box *stts_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeToSampleBox, GF_ISOM_BOX_TYPE_STTS);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stts_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stts_box_size(GF_Box *s)
{
	GF_TimeToSampleBox *ptr = (GF_TimeToSampleBox *)s;
	ptr->size += 4 + (8 * ptr->nb_entries);
	return GF_OK;
}


#endif /*GPAC_DISABLE_ISOM_WRITE*/


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void tfhd_box_del(GF_Box *s)
{
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err tfhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackFragmentHeaderBox *ptr = (GF_TrackFragmentHeaderBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->trackID = gf_bs_read_u32(bs);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRAF_BASE_OFFSET) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->base_data_offset = gf_bs_read_u64(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DESC) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->sample_desc_index = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_DUR) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->def_sample_duration = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_SIZE) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->def_sample_size = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->def_sample_flags = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *tfhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentHeaderBox, GF_ISOM_BOX_TYPE_TFHD);
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err tfhd_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tfhd_box_size(GF_Box *s)
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


void tims_box_del(GF_Box *s)
{
	GF_TSHintEntryBox *tims = (GF_TSHintEntryBox *)s;
	gf_free(tims);
}

GF_Err tims_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TSHintEntryBox *ptr = (GF_TSHintEntryBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->timeScale = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tims_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TSHintEntryBox, GF_ISOM_BOX_TYPE_TIMS);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tims_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TSHintEntryBox *ptr = (GF_TSHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->timeScale);
	return GF_OK;
}

GF_Err tims_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tkhd_box_del(GF_Box *s)
{
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
	return;
}


GF_Err tkhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackHeaderBox *ptr = (GF_TrackHeaderBox *)s;

	if (ptr->version == 1) {
		ISOM_DECREASE_SIZE(ptr, 32);
		ptr->creationTime = gf_bs_read_u64(bs);
		ptr->modificationTime = gf_bs_read_u64(bs);
		ptr->trackID = gf_bs_read_u32(bs);
		ptr->reserved1 = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 20);
		ptr->creationTime = gf_bs_read_u32(bs);
		ptr->modificationTime = gf_bs_read_u32(bs);
		ptr->trackID = gf_bs_read_u32(bs);
		ptr->reserved1 = gf_bs_read_u32(bs);
		ptr->duration = gf_bs_read_u32(bs);
	}
	ptr->initial_duration = ptr->duration;

	ISOM_DECREASE_SIZE(ptr, 60);
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

GF_Box *tkhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackHeaderBox, GF_ISOM_BOX_TYPE_TKHD);
	tmp->matrix[0] = 0x00010000;
	tmp->matrix[4] = 0x00010000;
	tmp->matrix[8] = 0x40000000;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tkhd_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tkhd_box_size(GF_Box *s)
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

void traf_box_del(GF_Box *s)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	if (ptr == NULL) return;
	if (ptr->sub_samples) gf_list_del(ptr->sub_samples);
	gf_list_del(ptr->TrackRuns);
	if (ptr->sampleGroups) gf_list_del(ptr->sampleGroups);
	if (ptr->sampleGroupsDescription) gf_list_del(ptr->sampleGroupsDescription);
	if (ptr->sai_sizes) gf_list_del(ptr->sai_sizes);
	if (ptr->sai_offsets) gf_list_del(ptr->sai_offsets);
	gf_free(ptr);
}

GF_Err traf_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TFHD:
		BOX_FIELD_ASSIGN(tfhd, GF_TrackFragmentHeaderBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRUN:
		BOX_FIELD_LIST_ASSIGN(TrackRuns)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SDTP:
		BOX_FIELD_ASSIGN(sdtp, GF_SampleDependencyTypeBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TFDT:
		BOX_FIELD_ASSIGN(tfdt, GF_TFBaseMediaDecodeTimeBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SUBS:
		BOX_FIELD_LIST_ASSIGN(sub_samples)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SBGP:
	case GF_ISOM_BOX_TYPE_CSGP:
		BOX_FIELD_LIST_ASSIGN(sampleGroups)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SGPD:
		BOX_FIELD_LIST_ASSIGN(sampleGroupsDescription)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SAIZ:
		BOX_FIELD_LIST_ASSIGN(sai_sizes)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SAIO:
		BOX_FIELD_LIST_ASSIGN(sai_offsets)
		return GF_OK;
	//we will throw an error if both PIFF_PSEC and SENC are found. Not such files seen yet
	case GF_ISOM_BOX_TYPE_UUID:
		if ( ((GF_UUIDBox *)a)->internal_4cc==GF_ISOM_BOX_UUID_PSEC) {
			BOX_FIELD_ASSIGN(sample_encryption, GF_SampleEncryptionBox)
			if (!is_rem)
				ptr->sample_encryption->traf = ptr;
			return GF_OK;
		} else if ( ((GF_UUIDBox *)a)->internal_4cc==GF_ISOM_BOX_UUID_TFXD) {
			BOX_FIELD_ASSIGN(tfxd, GF_MSSTimeExtBox)
			return GF_OK;
		} else if ( ((GF_UUIDBox *)a)->internal_4cc==GF_ISOM_BOX_UUID_TFRF) {
			BOX_FIELD_ASSIGN(tfrf, GF_MSSTimeRefBox)
			return GF_OK;
		} else {
			return GF_OK;
		}
	case GF_ISOM_BOX_TYPE_SENC:
		BOX_FIELD_ASSIGN(sample_encryption, GF_SampleEncryptionBox)
		if (!is_rem)
			ptr->sample_encryption->traf = ptr;
		return GF_OK;
	}
	return GF_OK;
}


GF_Err traf_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *)s;
	GF_Err e = gf_isom_box_array_read(s, bs);
	if (e) return e;

	if (!ptr->tfhd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing TrackFragmentHeaderBox \n"));
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}

GF_Box *traf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentBox, GF_ISOM_BOX_TYPE_TRAF);
	tmp->TrackRuns = gf_list_new();
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err traf_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err traf_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_TrackFragmentBox *ptr = (GF_TrackFragmentBox *) s;

	//Header first
	gf_isom_check_position(s, (GF_Box *)ptr->tfhd, &pos);
	gf_isom_check_position_list(s, ptr->sub_samples, &pos);

	gf_isom_check_position(s, (GF_Box *)ptr->tfdt, &pos);

	//cmaf-like
	if (ptr->truns_first) {
		gf_isom_check_position_list(s, ptr->TrackRuns, &pos);
		gf_isom_check_position_list(s, ptr->sai_sizes, &pos);
		gf_isom_check_position_list(s, ptr->sai_offsets, &pos);
		//senc MUST be after saio in GPAC, as senc writing uses info from saio writing
		gf_isom_check_position(s, (GF_Box *)ptr->sample_encryption, &pos);
		gf_isom_check_position_list(s, ptr->sampleGroupsDescription, &pos);
		gf_isom_check_position_list(s, ptr->sampleGroups, &pos);
		//subsamples will be last
	} else {
		gf_isom_check_position_list(s, ptr->sampleGroupsDescription, &pos);
		gf_isom_check_position_list(s, ptr->sampleGroups, &pos);
		gf_isom_check_position_list(s, ptr->sai_sizes, &pos);
		gf_isom_check_position_list(s, ptr->sai_offsets, &pos);
		gf_isom_check_position(s, (GF_Box *)ptr->sample_encryption, &pos);
		gf_isom_check_position_list(s, ptr->TrackRuns, &pos);
	}

	//when sdtp is present (smooth-like) write it after the trun box
	gf_isom_check_position(s, (GF_Box *)ptr->sdtp, &pos);

	//tfxd should be last ...
	if (ptr->tfxd)
		gf_isom_check_position(s, (GF_Box *)ptr->tfxd, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *tfxd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MSSTimeExtBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_TFXD;
	tmp->version = 1;
	return (GF_Box *)tmp;
}

void tfxd_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err tfxd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_MSSTimeExtBox *ptr = (GF_MSSTimeExtBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);

	if (ptr->version == 0x01) {
		ISOM_DECREASE_SIZE(ptr, 16);
		ptr->absolute_time_in_track_timescale = gf_bs_read_u64(bs);
		ptr->fragment_duration_in_track_timescale = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->absolute_time_in_track_timescale = gf_bs_read_u32(bs);
		ptr->fragment_duration_in_track_timescale = gf_bs_read_u32(bs);
	}

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tfxd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MSSTimeExtBox *ptr = (GF_MSSTimeExtBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, 0);
	if (ptr->version) {
		gf_bs_write_u64(bs, ptr->absolute_time_in_track_timescale);
		gf_bs_write_u64(bs, ptr->fragment_duration_in_track_timescale);
	} else {
		gf_bs_write_u32(bs, (u32) ptr->absolute_time_in_track_timescale);
		gf_bs_write_u32(bs, (u32) ptr->fragment_duration_in_track_timescale);
	}
	return GF_OK;
}

GF_Err tfxd_box_size(GF_Box *s)
{
	GF_MSSTimeExtBox *ptr = (GF_MSSTimeExtBox*)s;
	s->size += 4 + (ptr->version ? 16 : 8);
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE



GF_Box *tfrf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MSSTimeRefBox, GF_ISOM_BOX_TYPE_UUID);
	tmp->internal_4cc = GF_ISOM_BOX_UUID_TFRF;
	return (GF_Box *)tmp;
}

void tfrf_box_del(GF_Box *s)
{
	GF_MSSTimeRefBox *ptr = (GF_MSSTimeRefBox *)s;
	if (ptr->frags) gf_free(ptr->frags);
	gf_free(s);
}


GF_Err tfrf_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_MSSTimeRefBox *ptr = (GF_MSSTimeRefBox *)s;
	ISOM_DECREASE_SIZE(ptr, 5);
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);
	ptr->frags_count = gf_bs_read_u8(bs);
	ptr->frags = gf_malloc(sizeof(GF_MSSTimeEntry) * ptr->frags_count);
	if (!ptr->frags) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->frags_count; i++) {
		if (ptr->version == 0x01) {
			ISOM_DECREASE_SIZE(ptr, 16);
			ptr->frags[i].absolute_time_in_track_timescale = gf_bs_read_u64(bs);
			ptr->frags[i].fragment_duration_in_track_timescale = gf_bs_read_u64(bs);
		} else {
			ISOM_DECREASE_SIZE(ptr, 8);
			ptr->frags[i].absolute_time_in_track_timescale = gf_bs_read_u32(bs);
			ptr->frags[i].fragment_duration_in_track_timescale = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tfrf_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_MSSTimeRefBox *ptr = (GF_MSSTimeRefBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u24(bs, 0);
	gf_bs_write_u8(bs, ptr->frags_count);
	for (i=0; i<ptr->frags_count; i++) {
		if (ptr->version==0x01) {
			gf_bs_write_u64(bs, ptr->frags[i].absolute_time_in_track_timescale);
			gf_bs_write_u64(bs, ptr->frags[i].fragment_duration_in_track_timescale);
		} else {
			gf_bs_write_u32(bs, (u32) ptr->frags[i].absolute_time_in_track_timescale);
			gf_bs_write_u32(bs, (u32) ptr->frags[i].fragment_duration_in_track_timescale);
		}
	}
	return GF_OK;
}

GF_Err tfrf_box_size(GF_Box *s)
{
	GF_MSSTimeRefBox *ptr = (GF_MSSTimeRefBox*)s;
	s->size += 5;
	if (ptr->version) s->size += 16 * ptr->frags_count;
	else s->size += 8 * ptr->frags_count;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void trak_box_del(GF_Box *s)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	if (ptr->chunk_cache)
		gf_bs_del(ptr->chunk_cache);
#endif
	gf_free(s);
}

static GF_Err gf_isom_check_sample_desc(GF_TrackBox *trak)
{
	GF_BitStream *bs;
	GF_UnknownBox *a;
	u32 i;
	GF_Err e;
	GF_SampleTableBox *stbl;

	if (!trak->Media || !trak->Media->information) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no media box !\n" ));
		return GF_OK;
	}
	if (!trak->Media->information->sampleTable) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no sample table !\n" ));
		trak->Media->information->sampleTable = (GF_SampleTableBox *) gf_isom_box_new_parent(&trak->Media->information->child_boxes, GF_ISOM_BOX_TYPE_STBL);
	}
	stbl = trak->Media->information->sampleTable;

	if (!stbl->SampleDescription) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Track with no sample description box !\n" ));
		stbl->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSD);
		return GF_OK;
	}

	i=0;
	while ((a = (GF_UnknownBox*)gf_list_enum(stbl->SampleDescription->child_boxes, &i))) {
		GF_ProtectionSchemeInfoBox *sinf;
		u32 base_ent_type = 0;
		u32 type = a->type;
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_ENCS:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_RESV:
		case GF_ISOM_BOX_TYPE_ENCT:
			sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(a->child_boxes, GF_ISOM_BOX_TYPE_SINF);
			if (!sinf || !sinf->original_format) return GF_ISOM_INVALID_FILE;
			type = sinf->original_format->data_format;
			base_ent_type = ((GF_SampleEntryBox *)a)->internal_type;
			break;
		}

		switch (type) {
		case GF_ISOM_BOX_TYPE_MP4S:
			if (base_ent_type && (base_ent_type != GF_ISOM_SAMPLE_ENTRY_MP4S)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Protected sample entry %s uses incompatible sample description %s\n", gf_4cc_to_str(a->type), gf_4cc_to_str(type) ));

				return GF_ISOM_INVALID_FILE;
			}
			continue;

		case GF_ISOM_SUBTYPE_3GP_AMR:
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_QCELP:
		case GF_ISOM_SUBTYPE_3GP_SMV:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_MP3:
		case GF_ISOM_BOX_TYPE_MHA1:
		case GF_ISOM_BOX_TYPE_MHA2:
		case GF_ISOM_BOX_TYPE_MHM1:
		case GF_ISOM_BOX_TYPE_MHM2:
		case GF_ISOM_BOX_TYPE_OPUS:
		case GF_ISOM_BOX_TYPE_AC3:
		case GF_ISOM_BOX_TYPE_EC3:
		case GF_QT_SUBTYPE_RAW_AUD:
		case GF_QT_SUBTYPE_TWOS:
		case GF_QT_SUBTYPE_SOWT:
		case GF_QT_SUBTYPE_FL32:
		case GF_QT_SUBTYPE_FL64:
		case GF_QT_SUBTYPE_IN24:
		case GF_QT_SUBTYPE_IN32:
		case GF_QT_SUBTYPE_ULAW:
		case GF_QT_SUBTYPE_ALAW:
		case GF_QT_SUBTYPE_ADPCM:
		case GF_QT_SUBTYPE_IMA_ADPCM:
		case GF_QT_SUBTYPE_DVCA:
		case GF_QT_SUBTYPE_QDMC:
		case GF_QT_SUBTYPE_QDMC2:
		case GF_QT_SUBTYPE_QCELP:
		case GF_QT_SUBTYPE_kMP3:
		case GF_ISOM_BOX_TYPE_IPCM:
		case GF_ISOM_BOX_TYPE_FPCM:
			if (base_ent_type && (base_ent_type != GF_ISOM_SAMPLE_ENTRY_AUDIO))
				return GF_ISOM_INVALID_FILE;
			continue;

		case GF_ISOM_BOX_TYPE_MP4V:
		case GF_ISOM_SUBTYPE_3GP_H263:
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
		case GF_ISOM_BOX_TYPE_JPEG:
		case GF_ISOM_BOX_TYPE_PNG:
		case GF_ISOM_BOX_TYPE_JP2K:
		case GF_ISOM_BOX_TYPE_MJP2:
		case GF_QT_SUBTYPE_APCH:
		case GF_QT_SUBTYPE_APCO:
		case GF_QT_SUBTYPE_APCN:
		case GF_QT_SUBTYPE_APCS:
		case GF_QT_SUBTYPE_AP4X:
		case GF_QT_SUBTYPE_AP4H:
		case GF_ISOM_BOX_TYPE_VVC1:
		case GF_ISOM_BOX_TYPE_VVI1:
		case GF_QT_SUBTYPE_RAW_VID:
		case GF_QT_SUBTYPE_YUYV:
		case GF_QT_SUBTYPE_UYVY:
		case GF_QT_SUBTYPE_YUV444:
		case GF_QT_SUBTYPE_YUVA444:
		case GF_QT_SUBTYPE_YUV422_10:
		case GF_QT_SUBTYPE_YUV444_10:
		case GF_QT_SUBTYPE_YUV422_16:
		case GF_QT_SUBTYPE_YUV420:
		case GF_QT_SUBTYPE_I420:
		case GF_QT_SUBTYPE_IYUV:
		case GF_QT_SUBTYPE_YV12:
		case GF_QT_SUBTYPE_YVYU:
		case GF_QT_SUBTYPE_RGBA:
		case GF_QT_SUBTYPE_ABGR:
		case GF_ISOM_BOX_TYPE_DVHE:
		case GF_ISOM_BOX_TYPE_DVH1:
		case GF_ISOM_BOX_TYPE_DVA1:
		case GF_ISOM_BOX_TYPE_DVAV:
		case GF_ISOM_BOX_TYPE_DAV1:
			if (base_ent_type && (base_ent_type != GF_ISOM_SAMPLE_ENTRY_VIDEO))
				return GF_ISOM_INVALID_FILE;
			continue;


		case GF_ISOM_BOX_TYPE_METX:
		case GF_ISOM_BOX_TYPE_METT:
		case GF_ISOM_BOX_TYPE_STXT:
		case GF_ISOM_BOX_TYPE_TX3G:
		case GF_ISOM_BOX_TYPE_TEXT:
		case GF_ISOM_BOX_TYPE_GHNT:
		case GF_ISOM_BOX_TYPE_RTP_STSD:
		case GF_ISOM_BOX_TYPE_SRTP_STSD:
		case GF_ISOM_BOX_TYPE_FDP_STSD:
		case GF_ISOM_BOX_TYPE_RRTP_STSD:
		case GF_ISOM_BOX_TYPE_RTCP_STSD:
		case GF_ISOM_BOX_TYPE_DIMS:
		case GF_ISOM_BOX_TYPE_LSR1:
		case GF_ISOM_BOX_TYPE_WVTT:
		case GF_ISOM_BOX_TYPE_STPP:
		case GF_ISOM_BOX_TYPE_SBTT:
			if (base_ent_type && (base_ent_type != GF_ISOM_SAMPLE_ENTRY_GENERIC))
				return GF_ISOM_INVALID_FILE;
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
			gf_bs_set_cookie(bs, GF_ISOM_BS_COOKIE_NO_LOGS);\
			e = gf_isom_box_array_read((GF_Box *) _box, bs); \
			count_subb = _box->child_boxes ? gf_list_count(_box->child_boxes) : 0; \
			if (count_subb && !e) { \
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
		gf_list_rem(trak->Media->information->sampleTable->SampleDescription->child_boxes, i-1); \
		gf_isom_box_del((GF_Box *)a); \
		gf_list_insert(trak->Media->information->sampleTable->SampleDescription->child_boxes, _box, i-1); \


		/*only process visual or audio
		note: no need for new_box_parent here since we always store sample descriptions in child_boxes*/
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
			GF_GenericSampleEntryBox *genm = (GF_GenericSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
			genm->size = a->size-8;
			bs = gf_bs_new(a->data, a->dataSize, GF_BITSTREAM_READ);

			e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)genm, bs);
			if (e) return e;

			STSD_SWITCH_BOX(genm)
		}
		break;
		}
	}
	return GF_OK;
}


GF_Err trak_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	if (!a) return GF_OK;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_TKHD:
		BOX_FIELD_ASSIGN(Header, GF_TrackHeaderBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_EDTS:
		BOX_FIELD_ASSIGN(editBox, GF_EditBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UDTA:
		BOX_FIELD_ASSIGN(udta, GF_UserDataBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_META:
		BOX_FIELD_ASSIGN(meta, GF_MetaBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TREF:
		BOX_FIELD_ASSIGN(References, GF_TrackReferenceBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_MDIA:
		BOX_FIELD_ASSIGN(Media, GF_MediaBox)
		if (!is_rem)
			((GF_MediaBox *)a)->mediaTrack = ptr;
		return GF_OK;
	case GF_ISOM_BOX_TYPE_TRGR:
		BOX_FIELD_ASSIGN(groups, GF_TrackGroupBox)
		return GF_OK;
	case GF_QT_BOX_TYPE_TAPT:
		BOX_FIELD_ASSIGN(Aperture, GF_Box)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SENC:
		BOX_FIELD_ASSIGN(sample_encryption, GF_SampleEncryptionBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_UUID:
		if (((GF_UnknownUUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC) {
			BOX_FIELD_ASSIGN(sample_encryption, GF_SampleEncryptionBox)
			return GF_OK;
		}
	}
	return GF_OK;
}


GF_Err trak_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackBox *ptr = (GF_TrackBox *)s;
	e = gf_isom_box_array_read(s, bs);
	if (e) return e;
	e = gf_isom_check_sample_desc(ptr);
	if (e) return e;

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
	if (!ptr->Media->information->sampleTable->SampleSize || (ptr->Media->information->sampleTable->SampleSize->sampleCount==0)) {
		if (ptr->Header->initial_duration) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Track with no samples but duration defined, ignoring duration\n"));
			ptr->Header->initial_duration = 0;
		}
	}

	for (i=0; i<gf_list_count(ptr->Media->information->sampleTable->child_boxes); i++) {
		GF_Box *a = gf_list_get(ptr->Media->information->sampleTable->child_boxes, i);
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

GF_Box *trak_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackBox, GF_ISOM_BOX_TYPE_TRAK);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trak_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err trak_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_TrackBox *ptr = (GF_TrackBox *)s;

	if (ptr->sample_encryption && ptr->sample_encryption->load_needed) {
		if (!ptr->moov || !ptr->moov->mov || !ptr->moov->mov->movieFileMap)
			return GF_ISOM_INVALID_FILE;
		GF_Err e = senc_Parse(ptr->moov->mov->movieFileMap->bs, ptr, NULL, ptr->sample_encryption);
		if (e) return e;
	}

	gf_isom_check_position(s, (GF_Box *)ptr->Header, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->Aperture, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->References, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->editBox, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->Media, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->meta, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->groups, &pos);
	gf_isom_check_position(s, (GF_Box *)ptr->udta, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stri_box_del(GF_Box *s)
{
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->attribute_list) gf_free(ptr->attribute_list);
	gf_free(ptr);
}

GF_Err stri_box_read(GF_Box *s, GF_BitStream *bs)
{
	size_t i;
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8)
	ptr->switch_group = gf_bs_read_u16(bs);
	ptr->alternate_group = gf_bs_read_u16(bs);
	ptr->sub_track_id = gf_bs_read_u32(bs);
	ptr->attribute_count = ptr->size / 4;
	if ((u64)ptr->attribute_count > (u64)SIZE_MAX/sizeof(u32)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in stri\n", ptr->size));
		return GF_ISOM_INVALID_FILE;
	}
	GF_SAFE_ALLOC_N(ptr->attribute_list, (size_t)ptr->attribute_count, u32);
	if (!ptr->attribute_list) return GF_OUT_OF_MEM;
	for (i = 0; i < ptr->attribute_count; i++) {
		ISOM_DECREASE_SIZE(ptr, 4)
		ptr->attribute_list[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *stri_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackInformationBox, GF_ISOM_BOX_TYPE_STRI);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stri_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stri_box_size(GF_Box *s)
{
	GF_SubTrackInformationBox *ptr = (GF_SubTrackInformationBox *)s;

	ptr->size += 8 + 4 * ptr->attribute_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void stsg_box_del(GF_Box *s)
{
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	if (ptr == NULL) return;
	if (ptr->group_description_index) gf_free(ptr->group_description_index);
	gf_free(ptr);
}

GF_Err stsg_box_read(GF_Box *s, GF_BitStream *bs)
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

GF_Box *stsg_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackSampleGroupBox, GF_ISOM_BOX_TYPE_STSG);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stsg_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stsg_box_size(GF_Box *s)
{
	GF_SubTrackSampleGroupBox *ptr = (GF_SubTrackSampleGroupBox *)s;
	ptr->size += 6 + 4 * ptr->nb_groups;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void strk_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err strk_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	if (!a) return GF_OK;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_STRI:
		BOX_FIELD_ASSIGN(info, GF_SubTrackInformationBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_STRD:
		BOX_FIELD_ASSIGN(strd, GF_Box)
		return GF_OK;
	}
	return GF_OK;
}


GF_Err strk_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SubTrackBox *ptr = (GF_SubTrackBox *)s;
	e = gf_isom_box_array_read(s, bs);
	if (e) return e;

	if (!ptr->info) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing SubTrackInformationBox\n"));
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}

GF_Box *strk_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SubTrackBox, GF_ISOM_BOX_TYPE_STRK);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err strk_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err strk_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void tref_box_del(GF_Box *s)
{
	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err tref_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, s->type);
}

GF_Box *tref_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackReferenceBox, GF_ISOM_BOX_TYPE_TREF);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tref_box_write(GF_Box *s, GF_BitStream *bs)
{
//	GF_TrackReferenceBox *ptr = (GF_TrackReferenceBox *)s;
	return gf_isom_box_write_header(s, bs);
}

GF_Err tref_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void reftype_box_del(GF_Box *s)
{
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	if (!ptr) return;
	if (ptr->trackIDs) gf_free(ptr->trackIDs);
	gf_free(ptr);
}


GF_Err reftype_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	u32 i;
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;

	bytesToRead = (u32) (ptr->size);
	if (!bytesToRead) return GF_OK;

	ptr->trackIDCount = (u32) (bytesToRead) / sizeof(u32);
	ptr->trackIDs = (GF_ISOTrackID *) gf_malloc(ptr->trackIDCount * sizeof(GF_ISOTrackID));
	if (!ptr->trackIDs) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->trackIDCount; i++) {
		ptr->trackIDs[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *reftype_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackReferenceTypeBox, GF_ISOM_BOX_TYPE_REFT);
	return (GF_Box *)tmp;
}


GF_Err reftype_AddRefTrack(GF_TrackReferenceTypeBox *ref, GF_ISOTrackID trackID, u16 *outRefIndex)
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

	ref->trackIDs = (GF_ISOTrackID *) gf_realloc(ref->trackIDs, (ref->trackIDCount + 1) * sizeof(GF_ISOTrackID) );
	if (!ref->trackIDs) return GF_OUT_OF_MEM;
	ref->trackIDs[ref->trackIDCount] = trackID;
	ref->trackIDCount++;
	if (outRefIndex) *outRefIndex = ref->trackIDCount;
	return GF_OK;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err reftype_box_write(GF_Box *s, GF_BitStream *bs)
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


GF_Err reftype_box_size(GF_Box *s)
{
	GF_TrackReferenceTypeBox *ptr = (GF_TrackReferenceTypeBox *)s;
	if (ptr->trackIDCount)
		ptr->size += (ptr->trackIDCount * sizeof(u32));
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void trex_box_del(GF_Box *s)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err trex_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;

	ISOM_DECREASE_SIZE(ptr, 20);
	ptr->trackID = gf_bs_read_u32(bs);
	ptr->def_sample_desc_index = gf_bs_read_u32(bs);
	ptr->def_sample_duration = gf_bs_read_u32(bs);
	ptr->def_sample_size = gf_bs_read_u32(bs);
	ptr->def_sample_flags = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *trex_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackExtendsBox, GF_ISOM_BOX_TYPE_TREX);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err trex_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err trex_box_size(GF_Box *s)
{
	GF_TrackExtendsBox *ptr = (GF_TrackExtendsBox *)s;
	ptr->size += 20;
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/



void trep_box_del(GF_Box *s)
{
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err trep_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->trackID = gf_bs_read_u32(bs);

	return gf_isom_box_array_read(s, bs);
}

GF_Box *trep_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackExtensionPropertiesBox, GF_ISOM_BOX_TYPE_TREP);
	tmp->child_boxes = gf_list_new();
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err trep_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->trackID);
	return GF_OK;
}

GF_Err trep_box_size(GF_Box *s)
{
	GF_TrackExtensionPropertiesBox *ptr = (GF_TrackExtensionPropertiesBox *)s;
	ptr->size += 4;
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/



#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

void trun_box_del(GF_Box *s)
{
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;
	if (ptr == NULL) return;

	if (ptr->samples) gf_free(ptr->samples);
	if (ptr->cache) gf_bs_del(ptr->cache);
	if (ptr->sample_order) gf_free(ptr->sample_order);
	gf_free(ptr);
}

#ifdef GF_ENABLE_CTRN

static u32 ctrn_field_size(u32 field_idx)
{
	if (field_idx==3) return 4;
	return field_idx;
}

u32 gf_isom_ctrn_field_size_bits(u32 field_idx)
{
	if (field_idx==3) return 32;
	return field_idx*8;
}
static u32 ctrn_read_flags(GF_BitStream *bs, u32 nbbits)
{
	u32 val = gf_bs_read_int(bs, nbbits);
	if (nbbits==16) val <<= 16;
	else if (nbbits==8) val <<= 24;
	return val;
}

static GF_Err ctrn_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, count, flags, first_idx=0;
	Bool inherit_dur, inherit_size, inherit_flags, inherit_ctso;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;
	flags = ptr->flags;
	ptr->ctrn_flags = flags;
	ptr->flags = 0;

	ptr->sample_count = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(ptr, 2);

	if (flags & GF_ISOM_TRUN_DATA_OFFSET) {
		if (flags & GF_ISOM_CTRN_DATAOFFSET_16) {
			ptr->data_offset = gf_bs_read_u16(bs);
			ISOM_DECREASE_SIZE(ptr, 2);
		} else {
			ptr->data_offset = gf_bs_read_u32(bs);
			ISOM_DECREASE_SIZE(ptr, 4);
		}
		ptr->flags |= GF_ISOM_TRUN_DATA_OFFSET;
	}
	if (flags & GF_ISOM_CTRN_CTSO_MULTIPLIER) {
		ptr->ctso_multiplier = gf_bs_read_u16(bs);
		ISOM_DECREASE_SIZE(ptr, 2);
	}
	/*no sample dur/sample_flag/size/ctso for first or following, create a pack sample */
	if (! (flags & 0x00FFFF00)) {
		GF_SAFEALLOC(ent, GF_TrunEntry);
		if (!ent) return GF_OUT_OF_MEM;
		ent->nb_pack = ptr->sample_count;
		gf_list_add(ptr->entries, ent);
		return GF_OK;
	}
	/*allocate all entries*/
	for (i=0; i<ptr->sample_count; i++) {
		GF_SAFEALLOC(ent, GF_TrunEntry);
		if (!ent) return GF_OUT_OF_MEM;
		gf_list_add(ptr->entries, ent);
	}
	//unpack flags
	ptr->ctrn_first_dur = (flags>>22) & 0x3;
	ptr->ctrn_first_size = (flags>>20) & 0x3;
	ptr->ctrn_first_sample_flags = (flags>>18) & 0x3;
	ptr->ctrn_first_ctts = (flags>>16) & 0x3;
	ptr->ctrn_dur = (flags>>14) & 0x3;
	ptr->ctrn_size = (flags>>12) & 0x3;
	ptr->ctrn_sample_flags = (flags>>10) & 0x3;
	ptr->ctrn_ctts = (flags>>8) & 0x3;

	inherit_dur = flags & GF_ISOM_CTRN_INHERIT_DUR;
	inherit_size = flags & GF_ISOM_CTRN_INHERIT_SIZE;
	inherit_flags = flags & GF_ISOM_CTRN_INHERIT_FLAGS;
	inherit_ctso = flags & GF_ISOM_CTRN_INHERIT_CTSO;

	if (flags & GF_ISOM_CTRN_FIRST_SAMPLE) {
		ent = gf_list_get(ptr->entries, 0);
		first_idx = 1;
		if (!inherit_dur && ptr->ctrn_first_dur) {
			ent->Duration = gf_bs_read_int(bs, gf_isom_ctrn_field_size_bits(ptr->ctrn_first_dur) );
			ISOM_DECREASE_SIZE(ptr, ctrn_field_size(ptr->ctrn_first_dur) );
		}
		if (!inherit_size && ptr->ctrn_first_size) {
			ent->size = gf_bs_read_int(bs, gf_isom_ctrn_field_size_bits(ptr->ctrn_first_size) );
			ISOM_DECREASE_SIZE(ptr, ctrn_field_size(ptr->ctrn_first_size) );
		}
		if (!inherit_flags && ptr->ctrn_first_sample_flags) {
			ent->flags = ctrn_read_flags(bs, gf_isom_ctrn_field_size_bits(ptr->ctrn_first_sample_flags) );
			ISOM_DECREASE_SIZE(ptr, ctrn_field_size(ptr->ctrn_first_sample_flags) );
		}
		if (!inherit_ctso && ptr->ctrn_first_ctts) {
			ent->CTS_Offset = gf_bs_read_int(bs, gf_isom_ctrn_field_size_bits(ptr->ctrn_first_ctts) );
			ISOM_DECREASE_SIZE(ptr, ctrn_field_size(ptr->ctrn_first_ctts) );
			if (ptr->ctso_multiplier)
				ent->CTS_Offset *= (s32) ptr->ctso_multiplier;
		}
	}
	count = ptr->sample_count - first_idx;
	if (!inherit_dur && ptr->ctrn_dur) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ptr->ctrn_dur);
		ISOM_DECREASE_SIZE(ptr, count * nbbits / 8);
		for (i=first_idx; i<ptr->sample_count; i++) {
			ent = gf_list_get(ptr->entries, i);
			ent->Duration = gf_bs_read_int(bs, nbbits);
		}
	}
	if (!inherit_size && ptr->ctrn_size) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ptr->ctrn_size);
		ISOM_DECREASE_SIZE(ptr, count * nbbits / 8);
		for (i=first_idx; i<ptr->sample_count; i++) {
			ent = gf_list_get(ptr->entries, i);
			ent->size = gf_bs_read_int(bs, nbbits);
		}
	}
	if (!inherit_flags && ptr->ctrn_sample_flags) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ptr->ctrn_sample_flags);
		ISOM_DECREASE_SIZE(ptr, count * nbbits / 8);
		for (i=first_idx; i<ptr->sample_count; i++) {
			ent = gf_list_get(ptr->entries, i);
			ent->flags = ctrn_read_flags(bs, nbbits);
		}
	}
	if (!inherit_ctso && ptr->ctrn_ctts) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ptr->ctrn_ctts);
		ISOM_DECREASE_SIZE(ptr, count * nbbits / 8);
		for (i=first_idx; i<ptr->sample_count; i++) {
			ent = gf_list_get(ptr->entries, i);
			ent->CTS_Offset = gf_bs_read_int(bs, nbbits);
			if (ptr->ctso_multiplier)
				ent->CTS_Offset *= (s32) ptr->ctso_multiplier;
		}
	}

	return GF_OK;
}
#endif

GF_Err trun_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;

#ifdef GF_ENABLE_CTRN
	if (ptr->type == GF_ISOM_BOX_TYPE_CTRN) {
		ptr->type = GF_ISOM_BOX_TYPE_TRUN;
		ptr->use_ctrn = GF_TRUE;
		return ctrn_box_read(s, bs);
	}
#endif

	//check this is a good file
	if ((ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) && (ptr->flags & GF_ISOM_TRUN_FLAGS))
		return GF_ISOM_INVALID_FILE;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->sample_count = gf_bs_read_u32(bs);

	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->data_offset = gf_bs_read_u32(bs);
	}
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->first_sample_flags = gf_bs_read_u32(bs);
	}
	if (! (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) ) ) {
		ptr->samples = gf_malloc(sizeof(GF_TrunEntry));
		if (!ptr->samples) return GF_OUT_OF_MEM;
		//memset to 0 !!
		memset(ptr->samples, 0, sizeof(GF_TrunEntry));
		ptr->sample_alloc = ptr->nb_samples = 1;
		ptr->samples[0].nb_pack = ptr->sample_count;
	} else {
		//if we get here, at least one flag (so at least 4 bytes) is set, check size
		if (ptr->sample_count * 4 > ptr->size) {
			ISOM_DECREASE_SIZE(ptr, ptr->sample_count*4);
		}
		if ((u64)ptr->sample_count > (u64)SIZE_MAX/sizeof(GF_TrunEntry)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of samples %d in trun\n", ptr->sample_count));
			return GF_ISOM_INVALID_FILE;
		}
		ptr->samples = gf_malloc(sizeof(GF_TrunEntry) * ptr->sample_count);
		if (!ptr->samples) return GF_OUT_OF_MEM;
		ptr->sample_alloc = ptr->nb_samples = ptr->sample_count;
		//memset to 0 upfront
		memset(ptr->samples, 0, ptr->sample_count * sizeof(GF_TrunEntry));

		//read each entry (even though nothing may be written)
		for (i=0; i<ptr->sample_count; i++) {
			u32 trun_size = 0;
			GF_TrunEntry *p = &ptr->samples[i];

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
				trun_size += 4;
			}
			ISOM_DECREASE_SIZE(ptr, trun_size);
		}
	}
	/*todo parse sample reorder*/
	if (ptr->size) {
		gf_bs_skip_bytes(bs, ptr->size);
		ptr->size = 0;
	}
	return GF_OK;
}

GF_Box *trun_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackFragmentRunBox, GF_ISOM_BOX_TYPE_TRUN);
	//NO FLAGS SET BY DEFAULT
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

#ifdef GF_ENABLE_CTRN
static void ctrn_write_sample_flags(GF_BitStream *bs, u32 flags, u32 field_size)
{
	if (!field_size) return;

	if (field_size==8) flags = flags>>24;
	else if (field_size==16) flags = flags>>16;
	gf_bs_write_int(bs, flags, field_size);
}


static void ctrn_write_ctso(GF_TrackFragmentRunBox *ctrn, GF_BitStream *bs, u32 ctso, u32 field_size)
{
	if (!field_size) return;

	if (ctrn->ctso_multiplier) {
		gf_bs_write_int(bs, ctso / ctrn->ctso_multiplier, field_size);
	} else {
		gf_bs_write_int(bs, ctso, field_size);
	}
}

GF_Err ctrn_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, count, flags;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *ctrn = (GF_TrackFragmentRunBox *) s;
	if (!s) return GF_BAD_PARAM;
	flags = ctrn->flags;
	ctrn->flags = ctrn->ctrn_flags;
	ctrn->type = GF_ISOM_BOX_TYPE_CTRN;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	ctrn->flags = flags;
	ctrn->type = GF_ISOM_BOX_TYPE_TRUN;

	gf_bs_write_u16(bs, ctrn->sample_count);
	if (ctrn->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		if (ctrn->ctrn_flags & GF_ISOM_CTRN_DATAOFFSET_16) {
			gf_bs_write_u16(bs, ctrn->data_offset);
		} else {
			gf_bs_write_u32(bs, ctrn->data_offset);
		}
	}
	if (ctrn->ctso_multiplier) {
		gf_bs_write_u16(bs, ctrn->ctso_multiplier);
	}
	/*we always write first sample using first flags*/
	ent = gf_list_get(ctrn->entries, 0);
	gf_bs_write_int(bs, ent->Duration, gf_isom_ctrn_field_size_bits(ctrn->ctrn_first_dur) );
	gf_bs_write_int(bs, ent->size, gf_isom_ctrn_field_size_bits(ctrn->ctrn_first_size) );
	ctrn_write_sample_flags(bs, ent->flags, gf_isom_ctrn_field_size_bits(ctrn->ctrn_first_sample_flags) );
	ctrn_write_ctso(ctrn,bs, ent->CTS_Offset, gf_isom_ctrn_field_size_bits(ctrn->ctrn_first_ctts) );

	count = gf_list_count(ctrn->entries);
	if (ctrn->ctrn_dur) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ctrn->ctrn_dur);
		for (i=1; i<count; i++) {
			GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);
			gf_bs_write_int(bs, a_ent->Duration, nbbits);
		}
	}
	if (ctrn->ctrn_size) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ctrn->ctrn_size);
		for (i=1; i<count; i++) {
			GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);
			gf_bs_write_int(bs, a_ent->size, nbbits);
		}
	}
	if (ctrn->ctrn_sample_flags) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ctrn->ctrn_sample_flags);
		for (i=1; i<count; i++) {
			GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);
			ctrn_write_sample_flags(bs, a_ent->flags, nbbits);
		}
	}
	if (ctrn->ctrn_ctts) {
		u32 nbbits = gf_isom_ctrn_field_size_bits(ctrn->ctrn_ctts);
		for (i=1; i<count; i++) {
			GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);
			ctrn_write_ctso(ctrn, bs, a_ent->CTS_Offset, nbbits);
		}
	}

	return GF_OK;
}
#endif

GF_Err trun_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *) s;
	if (!s) return GF_BAD_PARAM;

#ifdef GF_ENABLE_CTRN
	if (ptr->use_ctrn)
		return ctrn_box_write(s, bs);
#endif

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

	if (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) )  {
		for (i=0; i<ptr->nb_samples; i++) {
			GF_TrunEntry *p = &ptr->samples[i];

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
	}

	if (ptr->sample_order) {
		u32 nb_bits = 8;
		if (ptr->sample_count>0xFFFFFF) nb_bits = 32;
		else if (ptr->sample_count>0xFFFF) nb_bits = 24;
		else if (ptr->sample_count>0xFF) nb_bits = 16;

		for (i=0; i<ptr->sample_count; i++) {
			gf_bs_write_int(bs, ptr->sample_order[i], nb_bits);
		}
	}
	return GF_OK;
}

#ifdef GF_ENABLE_CTRN
static u32 ctrn_sample_flags_to_index(u32 val)
{
	if (!val) return 0;
	if (val & 0x0000FFFF)
		return 3;
	if (val & 0x00FF0000)
		return 2;
	return 1;
}
static u32 ctrn_u32_to_index(u32 val)
{
	if (!val) return 0;
	if (val<=255) return 1;
	if (val<=65535) return 2;
	return 3;
}
static u32 ctrn_s32_to_index(s32 val)
{
	if (!val) return 0;
	if (ABS(val)<=127) return 1;
	if (ABS(val)<=32767) return 2;
	return 3;
}
static u32 ctrn_ctts_to_index(GF_TrackFragmentRunBox *ctrn, s32 ctts)
{
	if (!(ctrn->flags & GF_ISOM_TRUN_CTS_OFFSET))
		return 0;

	if (!ctts) return 0;

	if (ctrn->version) {
		if (ctrn->ctso_multiplier) return ctrn_s32_to_index(ctts / ctrn->ctso_multiplier);
		return ctrn_s32_to_index(ctts);
	}
	assert(ctts>0);
	if (ctrn->ctso_multiplier) return ctrn_u32_to_index((u32)ctts / ctrn->ctso_multiplier);
	return ctrn_s32_to_index((u32)ctts);
}

static GF_Err ctrn_box_size(GF_TrackFragmentRunBox *ctrn)
{
	Bool use_ctso_multi = GF_TRUE;
	u32 i, count;
	GF_TrunEntry *ent;

	ctrn->ctrn_flags = 0;
	ctrn->ctrn_first_dur = ctrn->ctrn_first_size = ctrn->ctrn_first_sample_flags = ctrn->ctrn_first_ctts = 0;
	ctrn->ctrn_dur = ctrn->ctrn_size = ctrn->ctrn_sample_flags = ctrn->ctrn_ctts = 0;

	ctrn->size += 2; //16 bits for sample count
	if (ctrn->flags & GF_ISOM_TRUN_DATA_OFFSET) {
		ctrn->ctrn_flags |= GF_ISOM_TRUN_DATA_OFFSET;
		if (ABS(ctrn->data_offset) < 32767) {
			ctrn->size += 2;
			ctrn->ctrn_flags |= GF_ISOM_CTRN_DATAOFFSET_16;
		} else
			ctrn->size += 4;
	}

	count = gf_list_count(ctrn->entries);
	if (ctrn->ctso_multiplier && (ctrn->flags & GF_ISOM_TRUN_CTS_OFFSET) && (ctrn->ctso_multiplier<=0xFFFF) ) {
		for (i=0; i<count; i++) {
			GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);
			if (a_ent->CTS_Offset % ctrn->ctso_multiplier) {
				use_ctso_multi = GF_FALSE;
				break;
			}
		}
	} else {
		use_ctso_multi = GF_FALSE;
	}
	if (ctrn->use_inherit) {
		use_ctso_multi = GF_FALSE;
		ctrn->ctrn_flags |= 0xB0; //duration=1,size=0,flags=1,cts=1 << 4
	}

	if (use_ctso_multi) {
		ctrn->size += 2;
		ctrn->ctrn_flags |= GF_ISOM_CTRN_CTSO_MULTIPLIER;
	} else {
		ctrn->ctso_multiplier = 0;
	}

	/*we always write first sample using first flags*/
	ent = gf_list_get(ctrn->entries, 0);
	ctrn->ctrn_flags |= GF_ISOM_CTRN_FIRST_SAMPLE;

	if (!ctrn->use_inherit && (ctrn->flags & GF_ISOM_TRUN_DURATION)) {
		ctrn->ctrn_first_dur = ctrn_u32_to_index(ent->Duration);
		if (ctrn->ctrn_first_dur) {
			ctrn->size += ctrn_field_size(ctrn->ctrn_first_dur);
			ctrn->ctrn_flags |= ctrn->ctrn_first_dur<<22;
		}
	}

	if (ctrn->flags & GF_ISOM_TRUN_SIZE) {
		ctrn->ctrn_first_size = ctrn_u32_to_index(ent->size);
		if (ctrn->ctrn_first_size) {
			ctrn->size += ctrn_field_size(ctrn->ctrn_first_size);
			ctrn->ctrn_flags |= ctrn->ctrn_first_size<<20;
		}
	}

	if (!ctrn->use_inherit && (ctrn->flags & GF_ISOM_TRUN_FLAGS)) {
		ctrn->ctrn_first_sample_flags = ctrn_sample_flags_to_index(ent->flags);
		if (ctrn->ctrn_first_sample_flags) {
			ctrn->size += ctrn_field_size(ctrn->ctrn_first_sample_flags);
			ctrn->ctrn_flags |= ctrn->ctrn_first_sample_flags<<18;
		}
	}
	if (!ctrn->use_inherit && (ctrn->flags & GF_ISOM_TRUN_CTS_OFFSET)) {
		ctrn->ctrn_first_ctts = ctrn_ctts_to_index(ctrn, ent->CTS_Offset);
		if (ctrn->ctrn_first_ctts) {
			ctrn->size += ctrn_field_size(ctrn->ctrn_first_ctts);
			ctrn->ctrn_flags |= ctrn->ctrn_first_ctts<<16;
		}
	}

	for (i=1; i<count; i++) {
		u8 field_idx;
		GF_TrunEntry *a_ent = gf_list_get(ctrn->entries, i);

		if (!ctrn->use_inherit && (ctrn->flags & GF_ISOM_TRUN_DURATION)) {
			field_idx = ctrn_u32_to_index(a_ent->Duration);
			if (ctrn->ctrn_dur < field_idx)
				ctrn->ctrn_dur = field_idx;
		}
		if (ctrn->flags & GF_ISOM_TRUN_SIZE) {
			field_idx = ctrn_u32_to_index(a_ent->size);
			if (ctrn->ctrn_size < field_idx)
				ctrn->ctrn_size = field_idx;
		}
		if (!ctrn->use_inherit && (ctrn->flags & GF_ISOM_TRUN_FLAGS)) {
			field_idx = ctrn_sample_flags_to_index(a_ent->flags);
			if (ctrn->ctrn_sample_flags < field_idx)
				ctrn->ctrn_sample_flags = field_idx;
		}
		if (!ctrn->use_inherit) {
			field_idx = ctrn_ctts_to_index(ctrn, a_ent->CTS_Offset);
			if (ctrn->ctrn_ctts < field_idx)
				ctrn->ctrn_ctts = field_idx;
		}
	}
	count-=1;
	if (ctrn->ctrn_dur) {
		ctrn->size += count * ctrn_field_size(ctrn->ctrn_dur);
		ctrn->ctrn_flags |= ctrn->ctrn_dur<<14;
	}
	if (ctrn->ctrn_size) {
		ctrn->size += count * ctrn_field_size(ctrn->ctrn_size);
		ctrn->ctrn_flags |= ctrn->ctrn_size<<12;
	}
	if (ctrn->ctrn_sample_flags) {
		ctrn->size += count * ctrn_field_size(ctrn->ctrn_sample_flags);
		ctrn->ctrn_flags |= ctrn->ctrn_sample_flags<<10;
	}
	if (ctrn->ctrn_ctts) {
		ctrn->size += count * ctrn_field_size(ctrn->ctrn_ctts);
		ctrn->ctrn_flags |= ctrn->ctrn_ctts<<8;
	}
	return GF_OK;
}
#endif

GF_Err trun_box_size(GF_Box *s)
{
	GF_TrackFragmentRunBox *ptr = (GF_TrackFragmentRunBox *)s;

#ifdef GF_ENABLE_CTRN
	if (ptr->use_ctrn)
		return ctrn_box_size(ptr);
#endif

	ptr->size += 4;
	//The rest depends on the flags
	if (ptr->flags & GF_ISOM_TRUN_DATA_OFFSET) ptr->size += 4;
	if (ptr->flags & GF_ISOM_TRUN_FIRST_FLAG) ptr->size += 4;

	if (ptr->sample_order) {
		u32 nb_bytes = 1;
		if (ptr->sample_count>0xFFFFFF) nb_bytes = 4;
		else if (ptr->sample_count>0xFFFF) nb_bytes = 3;
		else if (ptr->sample_count>0xFF) nb_bytes = 2;
		ptr->size += ptr->sample_count*nb_bytes;
	}

	if (! (ptr->flags & (GF_ISOM_TRUN_DURATION | GF_ISOM_TRUN_SIZE | GF_ISOM_TRUN_FLAGS | GF_ISOM_TRUN_CTS_OFFSET) ) ) {
		return GF_OK;
	}

	//if nothing to do, this will be skipped automatically
	if (ptr->flags & GF_ISOM_TRUN_DURATION) ptr->size += 4*ptr->nb_samples;
	if (ptr->flags & GF_ISOM_TRUN_SIZE) ptr->size += 4*ptr->nb_samples;
	//SHOULDN'T BE USED IF GF_ISOM_TRUN_FIRST_FLAG IS DEFINED
	if (ptr->flags & GF_ISOM_TRUN_FLAGS) ptr->size += 4*ptr->nb_samples;
	if (ptr->flags & GF_ISOM_TRUN_CTS_OFFSET) ptr->size += 4*ptr->nb_samples;

	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


void tsro_box_del(GF_Box *s)
{
	GF_TimeOffHintEntryBox *tsro = (GF_TimeOffHintEntryBox *)s;
	gf_free(tsro);
}

GF_Err tsro_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeOffHintEntryBox *ptr = (GF_TimeOffHintEntryBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->TimeOffset = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *tsro_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeOffHintEntryBox, GF_ISOM_BOX_TYPE_TSRO);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tsro_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TimeOffHintEntryBox *ptr = (GF_TimeOffHintEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->TimeOffset);
	return GF_OK;
}

GF_Err tsro_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void udta_box_del(GF_Box *s)
{
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	if (ptr == NULL) return;
	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		gf_isom_box_array_del(map->boxes);
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

GF_Err udta_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_Err e;
	u32 box_type;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;
	if (!ptr) return GF_BAD_PARAM;
	if (!a) return GF_OK;

	//detach from parent list if any
	gf_list_del_item(ptr->child_boxes, a);

	/* for unknown udta boxes, we reference them by their original box type */
	box_type = a->type;
	if (box_type == GF_ISOM_BOX_TYPE_UNKNOWN) {
		GF_UnknownBox* unkn = (GF_UnknownBox *)a;
		box_type = unkn->original_4cc;
	}

	map = udta_getEntry(ptr, box_type, (a->type==GF_ISOM_BOX_TYPE_UUID) ? & ((GF_UUIDBox *)a)->uuid : NULL);
	if (map == NULL) {
		if (is_rem) return GF_OK;

		map = (GF_UserDataMap *) gf_malloc(sizeof(GF_UserDataMap));
		if (map == NULL) return GF_OUT_OF_MEM;
		memset(map, 0, sizeof(GF_UserDataMap));

		map->boxType = box_type;
		if (a->type == GF_ISOM_BOX_TYPE_UUID)
			memcpy(map->uuid, ((GF_UUIDBox *)a)->uuid, 16);
		map->boxes = gf_list_new();
		if (!map->boxes) {
			gf_free(map);
			return GF_OUT_OF_MEM;
		}
		e = gf_list_add(ptr->recordList, map);
		if (e) return e;
	}
	if (is_rem) {
		gf_list_del_item(map->boxes, a);
		return GF_OK;
	}
	return gf_list_add(map->boxes, a);
}


GF_Err udta_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_box_array_read(s, bs);
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

GF_Box *udta_box_new()
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

GF_Err udta_box_write(GF_Box *s, GF_BitStream *bs)
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
		e = gf_isom_box_array_write(s, map->boxes, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err udta_box_size(GF_Box *s)
{
	GF_Err e;
	u32 i;
	GF_UserDataMap *map;
	GF_UserDataBox *ptr = (GF_UserDataBox *)s;

	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(ptr->recordList, &i))) {
		//warning: here we are not passing the actual "parent" of the list
		//but the UDTA box. The parent itself is not an box, we don't care about it
		e = gf_isom_box_array_size(s, map->boxes);
		if (e) return e;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void vmhd_box_del(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err vmhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->reserved = gf_bs_read_u64(bs);
	return GF_OK;
}

GF_Box *vmhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_VideoMediaHeaderBox, GF_ISOM_BOX_TYPE_VMHD);
	tmp->flags = 1;
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err vmhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u64(bs, ptr->reserved);
	return GF_OK;
}

GF_Err vmhd_box_size(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	ptr->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void void_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err void_box_read(GF_Box *s, GF_BitStream *bs)
{
	if (s->size) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

GF_Box *void_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_Box, GF_ISOM_BOX_TYPE_VOID);
	return tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err void_box_write(GF_Box *s, GF_BitStream *bs)
{
	gf_bs_write_u32(bs, 0);
	return GF_OK;
}

GF_Err void_box_size(GF_Box *s)
{
	s->size = 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *pdin_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProgressiveDownloadBox, GF_ISOM_BOX_TYPE_PDIN);
	return (GF_Box *)tmp;
}


void pdin_box_del(GF_Box *s)
{
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;
	if (ptr == NULL) return;
	if (ptr->rates) gf_free(ptr->rates);
	if (ptr->times) gf_free(ptr->times);
	gf_free(ptr);
}


GF_Err pdin_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox*)s;

	ptr->count = (u32) (ptr->size) / 8;
	ptr->rates = (u32*)gf_malloc(sizeof(u32)*ptr->count);
	if (!ptr->rates) return GF_OUT_OF_MEM;
	ptr->times = (u32*)gf_malloc(sizeof(u32)*ptr->count);
	if (!ptr->times) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->count; i++) {
		ptr->rates[i] = gf_bs_read_u32(bs);
		ptr->times[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pdin_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err pdin_box_size(GF_Box *s)
{
	GF_ProgressiveDownloadBox *ptr = (GF_ProgressiveDownloadBox *)s;
	ptr->size += 8*ptr->count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *sdtp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleDependencyTypeBox, GF_ISOM_BOX_TYPE_SDTP);
	return (GF_Box *)tmp;
}


void sdtp_box_del(GF_Box *s)
{
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;
	if (ptr == NULL) return;
	if (ptr->sample_info) gf_free(ptr->sample_info);
	gf_free(ptr);
}


GF_Err sdtp_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox*)s;

	/*out-of-order sdtp, assume no padding at the end*/
	if (!ptr->sampleCount) ptr->sampleCount = (u32) ptr->size;
	else if (ptr->sampleCount > (u32) ptr->size) return GF_ISOM_INVALID_FILE;

	ptr->sample_info = (u8 *) gf_malloc(sizeof(u8)*ptr->sampleCount);
	if (!ptr->sample_info) return GF_OUT_OF_MEM;
	ptr->sample_alloc = ptr->sampleCount;
	gf_bs_read_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	ISOM_DECREASE_SIZE(ptr, ptr->sampleCount);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err sdtp_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox *)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, (char*)ptr->sample_info, ptr->sampleCount);
	return GF_OK;
}

GF_Err sdtp_box_size(GF_Box *s)
{
	GF_SampleDependencyTypeBox *ptr = (GF_SampleDependencyTypeBox *)s;
	ptr->size += ptr->sampleCount;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *pasp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PixelAspectRatioBox, GF_ISOM_BOX_TYPE_PASP);
	return (GF_Box *)tmp;
}


void pasp_box_del(GF_Box *s)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err pasp_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->hSpacing = gf_bs_read_u32(bs);
	ptr->vSpacing = gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pasp_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->hSpacing);
	gf_bs_write_u32(bs, ptr->vSpacing);
	return GF_OK;
}

GF_Err pasp_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *clap_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CleanApertureBox, GF_ISOM_BOX_TYPE_CLAP);
	return (GF_Box *)tmp;
}


void clap_box_del(GF_Box *s)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox*)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}


GF_Err clap_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox*)s;
	ISOM_DECREASE_SIZE(ptr, 32);
	ptr->cleanApertureWidthN = gf_bs_read_u32(bs);
	ptr->cleanApertureWidthD = gf_bs_read_u32(bs);
	ptr->cleanApertureHeightN = gf_bs_read_u32(bs);
	ptr->cleanApertureHeightD = gf_bs_read_u32(bs);
	ptr->horizOffN = (s32) gf_bs_read_u32(bs);
	ptr->horizOffD = gf_bs_read_u32(bs);
	ptr->vertOffN = (s32) gf_bs_read_u32(bs);
	ptr->vertOffD = gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clap_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->cleanApertureWidthN);
	gf_bs_write_u32(bs, ptr->cleanApertureWidthD);
	gf_bs_write_u32(bs, ptr->cleanApertureHeightN);
	gf_bs_write_u32(bs, ptr->cleanApertureHeightD);
	gf_bs_write_u32(bs, (u32) ptr->horizOffN);
	gf_bs_write_u32(bs, ptr->horizOffD);
	gf_bs_write_u32(bs, (u32) ptr->vertOffN);
	gf_bs_write_u32(bs, ptr->vertOffD);
	return GF_OK;
}

GF_Err clap_box_size(GF_Box *s)
{
	s->size += 32;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *metx_box_new()
{
	//type is overridden by the box constructor
	ISOM_DECL_BOX_ALLOC(GF_MetaDataSampleEntryBox, GF_ISOM_BOX_TYPE_METX);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


void metx_box_del(GF_Box *s)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);

	if (ptr->content_encoding) gf_free(ptr->content_encoding);
	if (ptr->xml_namespace) gf_free(ptr->xml_namespace);
	if (ptr->xml_schema_loc) gf_free(ptr->xml_schema_loc);
	if (ptr->mime_type) gf_free(ptr->mime_type);
	gf_free(ptr);
}


GF_Err metx_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_TXTC:
		//we allow the config box on metx
		BOX_FIELD_ASSIGN(config, GF_TextConfigBox)
		break;
	}
	return GF_OK;
}

GF_Err metx_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 size, i;
	GF_Err e;
	char *str;
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;
	ISOM_DECREASE_SIZE(ptr, 8);

	size = (u32) ptr->size;
	str = gf_malloc(sizeof(char)*size);
	if (!str) return GF_OUT_OF_MEM;

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
	return gf_isom_box_array_read(s, bs);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err metx_box_write(GF_Box *s, GF_BitStream *bs)
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
	}

	return GF_OK;
}

GF_Err metx_box_size(GF_Box *s)
{
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
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* SimpleTextSampleEntry */
GF_Box *txtc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TextConfigBox, GF_ISOM_BOX_TYPE_TXTC);
	return (GF_Box *)tmp;
}


void txtc_box_del(GF_Box *s)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)s;
	if (ptr == NULL) return;

	if (ptr->config) gf_free(ptr->config);
	gf_free(ptr);
}

GF_Err txtc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)s;
	if ((u32)ptr->size >= (u32)0xFFFFFFFF) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in txtc box\n", ptr->size));
		return GF_ISOM_INVALID_FILE;
	}
	ptr->config = (char *)gf_malloc(sizeof(char)*((u32) ptr->size+1));
	if (!ptr->config) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->config, (u32) ptr->size);
	ptr->config[ptr->size] = 0;
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err txtc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox *)s;
	GF_Err e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->config)
		gf_bs_write_data(bs, ptr->config, (u32) strlen(ptr->config));
	gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err txtc_box_size(GF_Box *s)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox *)s;
	if (ptr->config)
		ptr->size += strlen(ptr->config);
	ptr->size++;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dac3_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AC3ConfigBox, GF_ISOM_BOX_TYPE_DAC3);
	return (GF_Box *)tmp;
}

GF_Box *dec3_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AC3ConfigBox, GF_ISOM_BOX_TYPE_DAC3);
	tmp->cfg.is_ec3 = 1;
	return (GF_Box *)tmp;
}

void dac3_box_del(GF_Box *s)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	gf_free(ptr);
}


GF_Err dac3_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	return gf_odf_ac3_config_parse_bs(bs, ptr->cfg.is_ec3, &ptr->cfg);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dac3_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AC3ConfigBox *ptr = (GF_AC3ConfigBox *)s;

	if (ptr->cfg.is_ec3) s->type = GF_ISOM_BOX_TYPE_DEC3;
	e = gf_isom_box_write_header(s, bs);
	if (ptr->cfg.is_ec3) s->type = GF_ISOM_BOX_TYPE_DAC3;
	if (e) return e;
	
	return gf_odf_ac3_cfg_write_bs(&ptr->cfg, bs);
}

GF_Err dac3_box_size(GF_Box *s)
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



void lsrc_box_del(GF_Box *s)
{
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	if (ptr == NULL) return;
	if (ptr->hdr) gf_free(ptr->hdr);
	gf_free(ptr);
}


GF_Err lsrc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	ptr->hdr_size = (u32) ptr->size;
	ptr->hdr = gf_malloc(sizeof(char)*ptr->hdr_size);
	if (!ptr->hdr) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->hdr, ptr->hdr_size);
	return GF_OK;
}

GF_Box *lsrc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_LASERConfigurationBox, GF_ISOM_BOX_TYPE_LSRC);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err lsrc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->hdr, ptr->hdr_size);
	return GF_OK;
}

GF_Err lsrc_box_size(GF_Box *s)
{
	GF_LASERConfigurationBox *ptr = (GF_LASERConfigurationBox *)s;
	ptr->size += ptr->hdr_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void lsr1_box_del(GF_Box *s)
{
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	if (ptr == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	if (ptr->slc) gf_odf_desc_del((GF_Descriptor *)ptr->slc);
	gf_free(ptr);
}

GF_Err lsr1_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_LSRC:
		BOX_FIELD_ASSIGN(lsr_config, GF_LASERConfigurationBox)
		break;
	case GF_ISOM_BOX_TYPE_M4DS:
		BOX_FIELD_ASSIGN(descr, GF_MPEG4ExtensionDescriptorsBox)
		break;
	}
	return GF_OK;
}

GF_Err lsr1_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox*)s;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(ptr, 8);

	return gf_isom_box_array_read(s, bs);
}

GF_Box *lsr1_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_LASeRSampleEntryBox, GF_ISOM_BOX_TYPE_LSR1);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err lsr1_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	return GF_OK;
}

GF_Err lsr1_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_LASeRSampleEntryBox *ptr = (GF_LASeRSampleEntryBox *)s;
	s->size += 8;
	gf_isom_check_position(s, (GF_Box *)ptr->lsr_config, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void sidx_box_del(GF_Box *s)
{
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox *) s;
	if (ptr == NULL) return;
	if (ptr->refs) gf_free(ptr->refs);
	gf_free(ptr);
}

GF_Err sidx_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_SegmentIndexBox *ptr = (GF_SegmentIndexBox*) s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->reference_ID = gf_bs_read_u32(bs);
	ptr->timescale = gf_bs_read_u32(bs);

	if (ptr->version==0) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->earliest_presentation_time = gf_bs_read_u32(bs);
		ptr->first_offset = gf_bs_read_u32(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 16);
		ptr->earliest_presentation_time = gf_bs_read_u64(bs);
		ptr->first_offset = gf_bs_read_u64(bs);
	}
	ISOM_DECREASE_SIZE(ptr, 4);
	gf_bs_read_u16(bs); /* reserved */
	ptr->nb_refs = gf_bs_read_u16(bs);

	ptr->refs = gf_malloc(sizeof(GF_SIDXReference)*ptr->nb_refs);
	if (!ptr->refs) return GF_OUT_OF_MEM;
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

GF_Box *sidx_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SegmentIndexBox, GF_ISOM_BOX_TYPE_SIDX);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err sidx_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err sidx_box_size(GF_Box *s)
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

void ssix_box_del(GF_Box *s)
{
	u32 i;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox *)s;
	if (ptr == NULL) return;
	if (ptr->subsegments) {
		for (i = 0; i < ptr->subsegment_alloc; i++) {
			GF_SubsegmentInfo *subsegment = &ptr->subsegments[i];
			if (subsegment->ranges) gf_free(subsegment->ranges);
		}
		gf_free(ptr->subsegments);
	}
	gf_free(ptr);
}

GF_Err ssix_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i,j;
	GF_SubsegmentIndexBox *ptr = (GF_SubsegmentIndexBox*)s;

	ISOM_DECREASE_SIZE(ptr, 4)
	ptr->subsegment_count = gf_bs_read_u32(bs);
	//each subseg has at least one range_count (4 bytes), abort if not enough bytes (broken box)
	if (ptr->size / 4 < ptr->subsegment_count || (u64)ptr->subsegment_count > (u64)SIZE_MAX/sizeof(GF_SubsegmentInfo))
		return GF_ISOM_INVALID_FILE;

	ptr->subsegment_alloc = ptr->subsegment_count;
	GF_SAFE_ALLOC_N(ptr->subsegments, ptr->subsegment_count, GF_SubsegmentInfo);
	if (!ptr->subsegments)
	    return GF_OUT_OF_MEM;
	for (i = 0; i < ptr->subsegment_count; i++) {
		GF_SubsegmentInfo *subseg = &ptr->subsegments[i];
		ISOM_DECREASE_SIZE(ptr, 4)
		subseg->range_count = gf_bs_read_u32(bs);
		//each range is 4 bytes, abort if not enough bytes
		if (ptr->size / 4 < subseg->range_count || (u64)subseg->range_count > (u64)SIZE_MAX/sizeof(GF_SubsegmentRangeInfo))
			return GF_ISOM_INVALID_FILE;
		subseg->ranges = (GF_SubsegmentRangeInfo*) gf_malloc(sizeof(GF_SubsegmentRangeInfo) * subseg->range_count);
		if (!subseg->ranges) return GF_OUT_OF_MEM;
		for (j = 0; j < subseg->range_count; j++) {
			ISOM_DECREASE_SIZE(ptr, 4)
			subseg->ranges[j].level = gf_bs_read_u8(bs);
			subseg->ranges[j].range_size = gf_bs_read_u24(bs);
		}
	}
	return GF_OK;
}

GF_Box *ssix_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SubsegmentIndexBox, GF_ISOM_BOX_TYPE_SSIX);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ssix_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err ssix_box_size(GF_Box *s)
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

void leva_box_del(GF_Box *s)
{
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox *)s;
	if (ptr == NULL) return;
	if (ptr->levels) gf_free(ptr->levels);
	gf_free(ptr);
}

GF_Err leva_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_LevelAssignmentBox *ptr = (GF_LevelAssignmentBox*)s;

	ISOM_DECREASE_SIZE(ptr, 1)
	ptr->level_count = gf_bs_read_u8(bs);
	//each level is at least 5 bytes
	if (ptr->size / 5 < ptr->level_count)
		return GF_ISOM_INVALID_FILE;

	GF_SAFE_ALLOC_N(ptr->levels, ptr->level_count, GF_LevelAssignment);
	if (!ptr->levels) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->level_count; i++) {
		GF_LevelAssignment *level = &ptr->levels[i];
		u8 tmp;
		if (!level || ptr->size < 5) return GF_BAD_PARAM;
		ISOM_DECREASE_SIZE(ptr, 5)

		level->track_id = gf_bs_read_u32(bs);
		tmp = gf_bs_read_u8(bs);
		level->padding_flag = tmp >> 7;
		level->type = tmp & 0x7F;
		if (level->type == 0) {
			ISOM_DECREASE_SIZE(ptr, 4)
			level->grouping_type = gf_bs_read_u32(bs);
		}
		else if (level->type == 1) {
			ISOM_DECREASE_SIZE(ptr, 8)
			level->grouping_type = gf_bs_read_u32(bs);
			level->grouping_type_parameter = gf_bs_read_u32(bs);
		}
		else if (level->type == 4) {
			ISOM_DECREASE_SIZE(ptr, 4)
			level->sub_track_id = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}

GF_Box *leva_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_LevelAssignmentBox, GF_ISOM_BOX_TYPE_LEVA);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err leva_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err leva_box_size(GF_Box *s)
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

GF_Box *pcrb_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PcrInfoBox, GF_ISOM_BOX_TYPE_PCRB);
	return (GF_Box *)tmp;
}

void pcrb_box_del(GF_Box *s)
{
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox *) s;
	if (ptr == NULL) return;
	if (ptr->pcr_values) gf_free(ptr->pcr_values);
	gf_free(ptr);
}

GF_Err pcrb_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->subsegment_count = gf_bs_read_u32(bs);

	if ((u64)ptr->subsegment_count > ptr->size / 8 || (u64)ptr->subsegment_count > (u64)SIZE_MAX/sizeof(u64)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of subsegment %d in pcrb\n", ptr->subsegment_count));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->pcr_values = gf_malloc(sizeof(u64)*ptr->subsegment_count);
	if (!ptr->pcr_values) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->subsegment_count; i++) {
		u64 data1 = gf_bs_read_u32(bs);
		u64 data2 = gf_bs_read_u16(bs);
		ISOM_DECREASE_SIZE(ptr, 6);
		ptr->pcr_values[i] = (data1 << 10) | (data2 >> 6);

	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pcrb_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err pcrb_box_size(GF_Box *s)
{
	GF_PcrInfoBox *ptr = (GF_PcrInfoBox*) s;

	ptr->size += 4;
	ptr->size += ptr->subsegment_count * 6;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *subs_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SubSampleInformationBox, GF_ISOM_BOX_TYPE_SUBS);
	tmp->Samples = gf_list_new();
	return (GF_Box *)tmp;
}

void subs_box_del(GF_Box *s)
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

GF_Err subs_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j, entry_count;
	u16 subsample_count;
	GF_SubSampleEntry *pSubSamp;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) s;

	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	entry_count = gf_list_count(ptr->Samples);
	gf_bs_write_u32(bs, entry_count);

	for (i=0; i<entry_count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);
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

GF_Err subs_box_size(GF_Box *s)
{
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) s;
	u32 entry_count, i;
	u16 subsample_count;

	// add 4 byte for entry_count
	ptr->size += 4;
	entry_count = gf_list_count(ptr->Samples);
	for (i=0; i<entry_count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);
		subsample_count = gf_list_count(pSamp->SubSamples);
		// 4 byte for sample_delta, 2 byte for subsample_count
		// and 6 + (4 or 2) bytes for each subsample
		ptr->size += 4 + 2 + subsample_count * (6 + (ptr->version==1 ? 4 : 2));
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Err subs_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *)s;
	u32 entry_count, i, j;
	u16 subsample_count;

	ISOM_DECREASE_SIZE(ptr, 4);
	entry_count = gf_bs_read_u32(bs);

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

GF_Box *tfdt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TFBaseMediaDecodeTimeBox, GF_ISOM_BOX_TYPE_TFDT);
	return (GF_Box *)tmp;
}

void tfdt_box_del(GF_Box *s)
{
	gf_free(s);
}

/*this is using chpl format according to some NeroRecode samples*/
GF_Err tfdt_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;

	if (ptr->version==1) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->baseMediaDecodeTime = gf_bs_read_u64(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->baseMediaDecodeTime = (u32) gf_bs_read_u32(bs);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tfdt_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tfdt_box_size(GF_Box *s)
{
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox *)s;

	if (!ptr->version && (ptr->baseMediaDecodeTime<=0xFFFFFFFF)) {
		//ptr->version = 0;
		ptr->size += 4;
	} else {
		ptr->version = 1;
		ptr->size += 8;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


GF_Box *rvcc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_RVCConfigurationBox, GF_ISOM_BOX_TYPE_RVCC);
	return (GF_Box *)tmp;
}

void rvcc_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err rvcc_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox*)s;
	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->predefined_rvc_config = gf_bs_read_u16(bs);
	if (!ptr->predefined_rvc_config) {
		ISOM_DECREASE_SIZE(ptr, 2);
		ptr->rvc_meta_idx = gf_bs_read_u16(bs);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err rvcc_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err rvcc_box_size(GF_Box *s)
{
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox *)s;
	ptr->size += 2;
	if (! ptr->predefined_rvc_config) ptr->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *sbgp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleGroupBox, GF_ISOM_BOX_TYPE_SBGP);
	return (GF_Box *)tmp;
}
void sbgp_box_del(GF_Box *a)
{
	GF_SampleGroupBox *p = (GF_SampleGroupBox *)a;
	if (p->sample_entries) gf_free(p->sample_entries);
	gf_free(p);
}

GF_Err sbgp_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_SampleGroupBox *ptr = (GF_SampleGroupBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->grouping_type = gf_bs_read_u32(bs);

	if (ptr->version==1) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->grouping_type_parameter = gf_bs_read_u32(bs);
	}
	ptr->entry_count = gf_bs_read_u32(bs);

	if (ptr->size < sizeof(GF_SampleGroupEntry)*ptr->entry_count || (u64)ptr->entry_count > (u64)SIZE_MAX/sizeof(GF_SampleGroupEntry))
	    return GF_ISOM_INVALID_FILE;

	ptr->sample_entries = gf_malloc(sizeof(GF_SampleGroupEntry)*ptr->entry_count);
	if (!ptr->sample_entries) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->entry_count; i++) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->sample_entries[i].sample_count = gf_bs_read_u32(bs);
		ptr->sample_entries[i].group_description_index = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sbgp_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err sbgp_box_size(GF_Box *s)
{
	GF_SampleGroupBox *p = (GF_SampleGroupBox*)s;

	p->size += 8;
	if (p->grouping_type_parameter) p->version=1;

	if (p->version==1) p->size += 4;
	p->size += 8*p->entry_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

static void *sgpd_parse_entry(u32 grouping_type, GF_BitStream *bs, s32 bytes_in_box, u32 entry_size, u32 *total_bytes)
{
	Bool null_size_ok = GF_FALSE;
	GF_DefaultSampleGroupDescriptionEntry *def_ptr;

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
		if (bytes_in_box<3) return NULL;
		GF_SAFEALLOC(ptr, GF_CENCSampleEncryptionGroupEntry);
		if (!ptr) return NULL;
		Bool use_mkey = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 7); //reserved
		ptr->crypt_byte_block = gf_bs_read_int(bs, 4);
		ptr->skip_byte_block = gf_bs_read_int(bs, 4);
		ptr->IsProtected = gf_bs_read_u8(bs);
		bytes_in_box -= 3;
		if (use_mkey) {
			u64 pos = gf_bs_get_position(bs);
			u32 i, count = gf_bs_read_u16(bs);
			bytes_in_box -= 2;
			if (bytes_in_box<0) {
				gf_free(ptr);
				return NULL;
			}
			for (i=0; i<count; i++) {
				u8 ivsize = gf_bs_read_u8(bs);
				gf_bs_skip_bytes(bs, 16);
				bytes_in_box -= 17;
				if (!ivsize) {
					//const IV
					ivsize = gf_bs_read_u8(bs);
					gf_bs_skip_bytes(bs, ivsize);
					bytes_in_box -= 1 + ivsize;
				}
				if (bytes_in_box<0) {
					gf_free(ptr);
					return NULL;
				}
			}
			ptr->key_info_size = 1 + (u32) (gf_bs_get_position(bs) - pos);
			ptr->key_info = gf_malloc(sizeof(u8) * ptr->key_info_size);
			if (!ptr->key_info) {
				gf_free(ptr);
				return NULL;
			}
			gf_bs_seek(bs, pos);
			ptr->key_info[0] = 1;
			gf_bs_read_data(bs, ptr->key_info + 1, ptr->key_info_size - 1);
			*total_bytes = 3 + ptr->key_info_size - 1;

			if (!gf_cenc_validate_key_info(ptr->key_info, ptr->key_info_size)) {
				gf_free(ptr->key_info);
				gf_free(ptr);
				return NULL;
			}
		} else {
			bin128 kid;
			u8 const_iv_size = 0;
			u8 iv_size = gf_bs_read_u8(bs);
			gf_bs_read_data(bs, kid, 16);
			bytes_in_box -= 17;
			if (bytes_in_box<0) {
				gf_free(ptr);
				return NULL;
			}

			*total_bytes = 20;
			if ((ptr->IsProtected == 1) && !iv_size) {
				const_iv_size = gf_bs_read_u8(bs);
				if ((const_iv_size != 8) && (const_iv_size != 16)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] seig sample group have invalid constant_IV size\n"));
					gf_free(ptr);
					return NULL;
				}
			}
			ptr->key_info_size = 20;
			if (!iv_size && ptr->IsProtected) {
				ptr->key_info_size += 1 + const_iv_size;
			}
			ptr->key_info = gf_malloc(sizeof(u8) * ptr->key_info_size);
			if (!ptr->key_info) {
				gf_free(ptr);
				return NULL;
			}
			ptr->key_info[0] = 0;
			ptr->key_info[1] = 0;
			ptr->key_info[2] = 0;
			ptr->key_info[3] = iv_size;
			memcpy(ptr->key_info+4, kid, 16);
			if (!iv_size && ptr->IsProtected) {
				ptr->key_info[20] = const_iv_size;
				gf_bs_read_data(bs, (char *)ptr->key_info+21, const_iv_size);
				*total_bytes += 1 + const_iv_size;
			}
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
			entry_size = 1 + (large_size ? 2 : 1);
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
	case GF_ISOM_SAMPLE_GROUP_SPOR:
	{
		u32 i;
		GF_SubpictureOrderEntry *ptr;
		GF_SAFEALLOC(ptr, GF_SubpictureOrderEntry);
		if (!ptr) return NULL;
		ptr->subpic_id_info_flag = gf_bs_read_int(bs, 1);
		ptr->num_subpic_ref_idx = gf_bs_read_int(bs, 15);
		*total_bytes = 2;
		ptr->subp_track_ref_idx = gf_malloc(sizeof(u16) * ptr->num_subpic_ref_idx);
		if (!ptr->subp_track_ref_idx) {
			gf_free(ptr);
			return NULL;
		}
		for (i=0; i<ptr->num_subpic_ref_idx; i++) {
			ptr->subp_track_ref_idx[i] = gf_bs_read_u16(bs);
			*total_bytes += 2;
		}
		if (ptr->subpic_id_info_flag) {
			ptr->spinfo.subpic_id_len_minus1 = gf_bs_read_int(bs, 4);
			ptr->spinfo.subpic_id_bit_pos = gf_bs_read_int(bs, 12);
			ptr->spinfo.start_code_emul_flag = gf_bs_read_int(bs, 1);
			ptr->spinfo.pps_sps_subpic_id_flag = gf_bs_read_int(bs, 1);
			if (ptr->spinfo.pps_sps_subpic_id_flag) {
				ptr->spinfo.xps_id = gf_bs_read_int(bs, 6);
			} else {
				ptr->spinfo.xps_id = gf_bs_read_int(bs, 4);
				gf_bs_read_int(bs, 2);
			}
			*total_bytes += 3;
		}
		return ptr;
	}
	case GF_ISOM_SAMPLE_GROUP_SULM:
	{
		u32 i;
		GF_SubpictureLayoutMapEntry *ptr;
		GF_SAFEALLOC(ptr, GF_SubpictureLayoutMapEntry);
		if (!ptr) return NULL;
		ptr->groupID_info_4cc = gf_bs_read_u32(bs);
		ptr->nb_entries = 1 + gf_bs_read_u16(bs);
		*total_bytes = 6;
		ptr->groupIDs = gf_malloc(sizeof(u16) * ptr->nb_entries);
		if (!ptr->groupIDs) {
			gf_free(ptr);
			return NULL;
		}
		for (i=0; i<ptr->nb_entries; i++) {
			ptr->groupIDs[i] = gf_bs_read_u16(bs);
			*total_bytes += 2;
		}
		return ptr;
	}
	default:
		break;
	}

	if (!entry_size && !null_size_ok) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] %s sample group does not indicate entry size and is not implemented, cannot parse!\n", gf_4cc_to_str( grouping_type) ));
		return NULL;
	}
	GF_SAFEALLOC(def_ptr, GF_DefaultSampleGroupDescriptionEntry);
	if (!def_ptr) return NULL;
	if (entry_size) {
		def_ptr->length = entry_size;
		def_ptr->data = (u8 *) gf_malloc(sizeof(u8)*def_ptr->length);
		if (!def_ptr->data) {
			gf_free(def_ptr);
			return NULL;
		}
		gf_bs_read_data(bs, (char *) def_ptr->data, def_ptr->length);
		*total_bytes = entry_size;
	}
	return def_ptr;
}

void sgpd_del_entry(u32 grouping_type, void *entry)
{
	switch (grouping_type) {
	case GF_ISOM_SAMPLE_GROUP_SYNC:
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_PROL:
	case GF_ISOM_SAMPLE_GROUP_RAP:
	case GF_ISOM_SAMPLE_GROUP_TELE:
	case GF_ISOM_SAMPLE_GROUP_SAP:
		gf_free(entry);
		return;
	case GF_ISOM_SAMPLE_GROUP_SEIG:
	{
		GF_CENCSampleEncryptionGroupEntry *seig = (GF_CENCSampleEncryptionGroupEntry *)entry;
		if (seig->key_info) gf_free(seig->key_info);
		gf_free(entry);
	}
		return;
	case GF_ISOM_SAMPLE_GROUP_OINF:
		gf_isom_oinf_del_entry(entry);
		return;
	case GF_ISOM_SAMPLE_GROUP_LINF:
		gf_isom_linf_del_entry(entry);
		return;
	case GF_ISOM_SAMPLE_GROUP_SPOR:
	{
		GF_SubpictureOrderEntry *spor = (GF_SubpictureOrderEntry *)entry;
		if (spor->subp_track_ref_idx) gf_free(spor->subp_track_ref_idx);
		gf_free(spor);
	}
		return;

	case GF_ISOM_SAMPLE_GROUP_SULM:
	{
		GF_SubpictureLayoutMapEntry *sulm = (GF_SubpictureLayoutMapEntry *) entry;
		if (sulm->groupIDs) gf_free(sulm->groupIDs);
		gf_free(sulm);
		return;
	}

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
	{
		GF_CENCSampleEncryptionGroupEntry *seig = (GF_CENCSampleEncryptionGroupEntry *)entry;
		Bool use_mkey = seig->key_info[0];
		u32 nb_keys = 1;
		if (use_mkey) {
			nb_keys = seig->key_info[1];
			nb_keys<<=8;
			nb_keys |= seig->key_info[2];
		}
		gf_bs_write_int(bs, use_mkey ? 1 : 0, 1);
		gf_bs_write_int(bs, 0, 7);
		gf_bs_write_int(bs, seig->crypt_byte_block, 4);
		gf_bs_write_int(bs, seig->skip_byte_block, 4);
		gf_bs_write_u8(bs, seig->IsProtected);
		if (nb_keys>1) {
			gf_bs_write_data(bs, seig->key_info+1, seig->key_info_size-1);
		} else {
			gf_bs_write_data(bs, seig->key_info+3, seig->key_info_size - 3);
		}
	}
		return;
	case GF_ISOM_SAMPLE_GROUP_OINF:
		gf_isom_oinf_write_entry(entry, bs);
		return;
	case GF_ISOM_SAMPLE_GROUP_LINF:
		gf_isom_linf_write_entry(entry, bs);
		return;

	case GF_ISOM_SAMPLE_GROUP_SPOR:
	{
		u32 i;
		GF_SubpictureOrderEntry *spor = (GF_SubpictureOrderEntry *) entry;
		gf_bs_write_int(bs, spor->subpic_id_info_flag, 1);
		gf_bs_write_int(bs, spor->num_subpic_ref_idx, 15);
		for (i=0; i<spor->num_subpic_ref_idx; i++) {
			gf_bs_write_u16(bs, spor->subp_track_ref_idx[i]);
		}
		if (spor->subpic_id_info_flag) {
			gf_bs_write_int(bs, spor->spinfo.subpic_id_len_minus1, 4);
			gf_bs_write_int(bs, spor->spinfo.subpic_id_bit_pos, 12);
			gf_bs_write_int(bs, spor->spinfo.start_code_emul_flag, 1);
			gf_bs_write_int(bs, spor->spinfo.pps_sps_subpic_id_flag, 1);
			if (spor->spinfo.pps_sps_subpic_id_flag) {
				gf_bs_write_int(bs, spor->spinfo.xps_id, 6);
			} else {
				gf_bs_write_int(bs, spor->spinfo.xps_id, 4);
				gf_bs_write_int(bs, 0, 2);
			}
		}
		return;
	}

	case GF_ISOM_SAMPLE_GROUP_SULM:
	{
		u32 i;
		GF_SubpictureLayoutMapEntry *sulm = (GF_SubpictureLayoutMapEntry *) entry;
		gf_bs_write_u32(bs, sulm->groupID_info_4cc);
		gf_bs_write_u16(bs, sulm->nb_entries - 1);
		for (i=0; i<sulm->nb_entries; i++) {
			gf_bs_write_u16(bs, sulm->groupIDs[i]);
		}
		return;
	}

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
	{
		GF_CENCSampleEncryptionGroupEntry *seig = (GF_CENCSampleEncryptionGroupEntry *)entry;
		Bool use_mkey = seig->key_info[0] ? GF_TRUE : GF_FALSE;
		if (use_mkey) {
			return 3 + seig->key_info_size-1;
		}
		return seig->key_info_size; //== 3 + (seig->key_info_size-3);
	}
	case GF_ISOM_SAMPLE_GROUP_OINF:
		return gf_isom_oinf_size_entry(entry);
	case GF_ISOM_SAMPLE_GROUP_LINF:
		return gf_isom_linf_size_entry(entry);
	case GF_ISOM_SAMPLE_GROUP_SPOR:
	{
		GF_SubpictureOrderEntry *spor = (GF_SubpictureOrderEntry *)entry;
		u32 s = 2 + 2*spor->num_subpic_ref_idx;
		if (spor->subpic_id_info_flag) {
			s += 3;
		}
		return s;
	}
	case GF_ISOM_SAMPLE_GROUP_SULM:
	{
		GF_SubpictureLayoutMapEntry *sulm = (GF_SubpictureLayoutMapEntry *) entry;
		return 6 + 2*sulm->nb_entries;
	}

	default:
		return ((GF_DefaultSampleGroupDescriptionEntry *)entry)->length;
	}
}
#endif

GF_Box *sgpd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleGroupDescriptionBox, GF_ISOM_BOX_TYPE_SGPD);
	/*version 0 is deprecated, use v1 by default*/
	tmp->version = 1;
	tmp->group_descriptions = gf_list_new();
	return (GF_Box *)tmp;
}

void sgpd_box_del(GF_Box *a)
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

GF_Err sgpd_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 entry_count;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;

	ISOM_DECREASE_SIZE(p, 8);
	p->grouping_type = gf_bs_read_u32(bs);

	if (p->version>=1) {
		ISOM_DECREASE_SIZE(p, 4);
		p->default_length = gf_bs_read_u32(bs);
	}
	if (p->version>=2) {
		ISOM_DECREASE_SIZE(p, 4);
		p->default_description_index = gf_bs_read_u32(bs);
	}
	entry_count = gf_bs_read_u32(bs);

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
		ptr = sgpd_parse_entry(p->grouping_type, bs, (s32) p->size, size, &parsed_bytes);
		//don't return an error, just stop parsing so that we skip over the sgpd box
		if (!ptr) return GF_OK;
		gf_list_add(p->group_descriptions, ptr);

		ISOM_DECREASE_SIZE(p, parsed_bytes);
		entry_count--;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err sgpd_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i, nb_descs;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;
	GF_Err e;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, p->grouping_type);
	if (p->version>=1) gf_bs_write_u32(bs, p->default_length);
	if (p->version>=2) gf_bs_write_u32(bs, p->default_description_index);
	nb_descs = gf_list_count(p->group_descriptions);
	gf_bs_write_u32(bs, nb_descs);

	for (i=0; i<nb_descs; i++) {
		void *ptr = gf_list_get(p->group_descriptions, i);
		if ((p->version >= 1) && !p->default_length) {
			u32 size = sgpd_size_entry(p->grouping_type, ptr);
			gf_bs_write_u32(bs, size);
		}
		sgpd_write_entry(p->grouping_type, ptr, bs);
	}
	return GF_OK;
}

GF_Err sgpd_box_size(GF_Box *s)
{
	u32 i, nb_descs;
	Bool use_def_size = GF_TRUE;
	GF_SampleGroupDescriptionBox *p = (GF_SampleGroupDescriptionBox *)s;

	p->size += 8;

	//we force all sample groups to version 1, v0 being deprecated
	if (!p->version)
		p->version = 1;
	p->size += 4;

	if (p->version>=2)
		p->size += 4;
	p->default_length = 0;

	nb_descs = gf_list_count(p->group_descriptions);
	for (i=0; i<nb_descs; i++) {
		void *ptr = gf_list_get(p->group_descriptions, i);
		u32 size = sgpd_size_entry(p->grouping_type, ptr);
		p->size += size;
		if (use_def_size && !p->default_length) {
			p->default_length = size;
		} else if (p->default_length != size) {
			use_def_size = GF_FALSE;
			p->default_length = 0;
		}
	}
	if (p->version>=1) {
		if (!p->default_length) p->size += nb_descs * 4;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void saiz_box_del(GF_Box *s)
{
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;
	if (ptr == NULL) return;
	if (ptr->sample_info_size) gf_free(ptr->sample_info_size);
	gf_free(ptr);
}


GF_Err saiz_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleAuxiliaryInfoSizeBox*ptr = (GF_SampleAuxiliaryInfoSizeBox*)s;

	if (ptr->flags & 1) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);
	}
	ISOM_DECREASE_SIZE(ptr, 5);
	ptr->default_sample_info_size = gf_bs_read_u8(bs);
	ptr->sample_count = gf_bs_read_u32(bs);

	if (ptr->default_sample_info_size == 0) {
		if (ptr->size < ptr->sample_count)
		    return GF_ISOM_INVALID_FILE;

		ptr->sample_info_size = gf_malloc(sizeof(u8)*ptr->sample_count);
		ptr->sample_alloc = ptr->sample_count;
		if (!ptr->sample_info_size)
		    return GF_OUT_OF_MEM;

		ISOM_DECREASE_SIZE(ptr, ptr->sample_count);
		gf_bs_read_data(bs, (char *) ptr->sample_info_size, ptr->sample_count);
	}
	return GF_OK;
}

GF_Box *saiz_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleAuxiliaryInfoSizeBox, GF_ISOM_BOX_TYPE_SAIZ);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err saiz_box_write(GF_Box *s, GF_BitStream *bs)
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
		if (!ptr->sample_info_size)
			gf_bs_write_u8(bs, 0);
		else
			gf_bs_write_data(bs, (char *) ptr->sample_info_size, ptr->sample_count);
	}
	return GF_OK;
}

GF_Err saiz_box_size(GF_Box *s)
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

void saio_box_del(GF_Box *s)
{
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*)s;
	if (ptr == NULL) return;
	if (ptr->offsets) gf_free(ptr->offsets);
	if (ptr->cached_data) gf_free(ptr->cached_data);
	gf_free(ptr);
}


GF_Err saio_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox *)s;

	if (ptr->flags & 1) {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->aux_info_type = gf_bs_read_u32(bs);
		ptr->aux_info_type_parameter = gf_bs_read_u32(bs);
	}
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->entry_count = gf_bs_read_u32(bs);

	if (ptr->entry_count) {
		u32 i;
		if (ptr->size / (ptr->version == 0 ? 4 : 8) < ptr->entry_count || (u64)ptr->entry_count > (u64)SIZE_MAX/sizeof(u64))
			return GF_ISOM_INVALID_FILE;
		ptr->offsets = gf_malloc(sizeof(u64)*ptr->entry_count);
		if (!ptr->offsets)
			return GF_OUT_OF_MEM;
		ptr->entry_alloc = ptr->entry_count;
		if (ptr->version==0) {
			ISOM_DECREASE_SIZE(ptr, 4*ptr->entry_count);
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets[i] = gf_bs_read_u32(bs);
		} else {
			ISOM_DECREASE_SIZE(ptr, 8*ptr->entry_count);
			for (i=0; i<ptr->entry_count; i++)
				ptr->offsets[i] = gf_bs_read_u64(bs);
		}
	}
	return GF_OK;
}

GF_Box *saio_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SampleAuxiliaryInfoOffsetBox, GF_ISOM_BOX_TYPE_SAIO);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err saio_box_write(GF_Box *s, GF_BitStream *bs)
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
		//store position in bitstream before writing data - offsets can be NULL if a single offset is rewritten later on (cf senc_box_write)
		ptr->offset_first_offset_field = gf_bs_get_position(bs);
		if (ptr->version==0) {
			if (!ptr->offsets) {
				gf_bs_write_u32(bs, 0);
			} else {
				for (i=0; i<ptr->entry_count; i++)
					gf_bs_write_u32(bs, (u32) ptr->offsets[i]);
			}
		} else {
			if (!ptr->offsets) {
				gf_bs_write_u64(bs, 0);
			} else {
				for (i=0; i<ptr->entry_count; i++)
					gf_bs_write_u64(bs, ptr->offsets[i]);
			}
		}
	}
	return GF_OK;
}

GF_Err saio_box_size(GF_Box *s)
{
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*)s;

	if (ptr->aux_info_type || ptr->aux_info_type_parameter) {
		ptr->flags |= 1;
	}

	if (ptr->flags & 1) ptr->size += 8;
	ptr->size += 4;
	//a little optim here: in cenc, the saio always points to a single data block, only one entry is needed
	switch (ptr->aux_info_type) {
	case GF_ISOM_CENC_SCHEME:
	case GF_ISOM_CBC_SCHEME:
	case GF_ISOM_CENS_SCHEME:
	case GF_ISOM_CBCS_SCHEME:
		if (ptr->offsets) gf_free(ptr->offsets);
		ptr->offsets = NULL;
		ptr->entry_alloc = 0;
		ptr->entry_count = 1;
		break;
	}

	ptr->size += ((ptr->version==1) ? 8 : 4) * ptr->entry_count;
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE


void prft_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err prft_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) s;

	ISOM_DECREASE_SIZE(ptr, 12);
	ptr->refTrackID = gf_bs_read_u32(bs);
	ptr->ntp = gf_bs_read_u64(bs);
	if (ptr->version==0) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->timestamp = gf_bs_read_u32(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 8);
		ptr->timestamp = gf_bs_read_u64(bs);
	}
	return GF_OK;
}

GF_Box *prft_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProducerReferenceTimeBox, GF_ISOM_BOX_TYPE_PRFT);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err prft_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err prft_box_size(GF_Box *s)
{
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox*)s;

	ptr->size += 4+8+ (ptr->version ? 8 : 4);
	return GF_OK;
}
#endif //GPAC_DISABLE_ISOM_WRITE

GF_Box *trgr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackGroupBox, GF_ISOM_BOX_TYPE_TRGR);
	tmp->groups = gf_list_new();
	if (!tmp->groups) {
		gf_free(tmp);
		return NULL;
	}
	return (GF_Box *)tmp;
}

void trgr_box_del(GF_Box *s)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;
	if (ptr == NULL) return;
	gf_list_del(ptr->groups);
	gf_free(ptr);
}


GF_Err trgr_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;

	BOX_FIELD_LIST_ASSIGN(groups)
	return gf_list_add(ptr->groups, a);
}


GF_Err trgr_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, s->type);
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_Err trgr_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err trgr_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *) s;
	gf_isom_check_position_list(s, ptr->groups, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *trgt_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackGroupTypeBox, GF_ISOM_BOX_TYPE_TRGT);
	return (GF_Box *)tmp;
}

void trgt_box_del(GF_Box *s)
{
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err trgt_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *)s;
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->track_group_id = gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trgt_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err trgt_box_size(GF_Box *s)
{
	GF_TrackGroupBox *ptr = (GF_TrackGroupBox *)s;

	ptr->size+= 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *stvi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_StereoVideoBox, GF_ISOM_BOX_TYPE_STVI);
	return (GF_Box *)tmp;
}

void stvi_box_del(GF_Box *s)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;
	if (ptr == NULL) return;
	if (ptr->stereo_indication_type) gf_free(ptr->stereo_indication_type);
	gf_free(ptr);
}

GF_Err stvi_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;

	ISOM_DECREASE_SIZE(ptr, 12);
	gf_bs_read_int(bs, 30);
	ptr->single_view_allowed = gf_bs_read_int(bs, 2);
	ptr->stereo_scheme = gf_bs_read_u32(bs);
	ptr->sit_len = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, ptr->sit_len);

	ptr->stereo_indication_type = gf_malloc(sizeof(char)*ptr->sit_len);
	if (!ptr->stereo_indication_type) return GF_OUT_OF_MEM;

	gf_bs_read_data(bs, ptr->stereo_indication_type, ptr->sit_len);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err stvi_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err stvi_box_size(GF_Box *s)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *)s;

	ptr->size+= 12 + ptr->sit_len;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *fiin_box_new()
{
	ISOM_DECL_BOX_ALLOC(FDItemInformationBox, GF_ISOM_BOX_TYPE_FIIN);
	return (GF_Box *)tmp;
}

void fiin_box_del(GF_Box *s)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;
	if (ptr == NULL) return;
	if (ptr->partition_entries) gf_list_del(ptr->partition_entries);
	gf_free(ptr);
}


GF_Err fiin_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_PAEN:
		BOX_FIELD_LIST_ASSIGN(partition_entries)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_SEGR:
		BOX_FIELD_ASSIGN(session_info, FDSessionGroupBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_GITN:
		BOX_FIELD_ASSIGN(group_id_to_name, GroupIdToNameBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err fiin_box_read(GF_Box *s, GF_BitStream *bs)
{
	FDItemInformationBox *ptr = (FDItemInformationBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	gf_bs_read_u16(bs);
	return gf_isom_box_array_read(s, bs);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fiin_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	FDItemInformationBox *ptr = (FDItemInformationBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, gf_list_count(ptr->partition_entries) );
	return GF_OK;
}

GF_Err fiin_box_size(GF_Box *s)
{
	u32 pos=0;
	FDItemInformationBox *ptr = (FDItemInformationBox *) s;
	s->size+= 2;
	gf_isom_check_position_list(s, ptr->partition_entries, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *paen_box_new()
{
	ISOM_DECL_BOX_ALLOC(FDPartitionEntryBox, GF_ISOM_BOX_TYPE_PAEN);
	return (GF_Box *)tmp;
}

void paen_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err paen_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	FDPartitionEntryBox *ptr = (FDPartitionEntryBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_FPAR:
		BOX_FIELD_ASSIGN(blocks_and_symbols, FilePartitionBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_FECR:
		BOX_FIELD_ASSIGN(FEC_symbol_locations, FECReservoirBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_FIRE:
		BOX_FIELD_ASSIGN(File_symbol_locations, FileReservoirBox)
		return GF_OK;
	}
	return GF_OK;
}

GF_Err paen_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err paen_box_write(GF_Box *s, GF_BitStream *bs)
{
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err paen_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




GF_Box *fpar_box_new()
{
	ISOM_DECL_BOX_ALLOC(FilePartitionBox, GF_ISOM_BOX_TYPE_FPAR);
	return (GF_Box *)tmp;
}

void fpar_box_del(GF_Box *s)
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
	if (! *out_str) return GF_OUT_OF_MEM;

	if (!s->size) {
		*out_str[0] = 0;
		return GF_OK;
	}

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

GF_Err fpar_box_read(GF_Box *s, GF_BitStream *bs)
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
	if (ptr->nb_entries > ptr->size / 6 || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(FilePartitionEntry))
		return GF_ISOM_INVALID_FILE;

	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries * 6 );
	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, FilePartitionEntry);
	if (!ptr->entries) return GF_OUT_OF_MEM;

	for (i=0;i < ptr->nb_entries; i++) {
		ptr->entries[i].block_count = gf_bs_read_u16(bs);
		ptr->entries[i].block_size = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fpar_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err fpar_box_size(GF_Box *s)
{
	FilePartitionBox *ptr = (FilePartitionBox *)s;

	ptr->size += 13 + (ptr->version ? 8 : 4);
	if (ptr->scheme_specific_info)
		ptr->size += strlen(ptr->scheme_specific_info);

	ptr->size+= ptr->nb_entries * 6;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *fecr_box_new()
{
	ISOM_DECL_BOX_ALLOC(FECReservoirBox, GF_ISOM_BOX_TYPE_FECR);
	return (GF_Box *)tmp;
}

void fecr_box_del(GF_Box *s)
{
	FECReservoirBox *ptr = (FECReservoirBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err fecr_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	FECReservoirBox *ptr = (FECReservoirBox *)s;

	ISOM_DECREASE_SIZE(ptr, (ptr->version ? 4 : 2) );
	ptr->nb_entries = gf_bs_read_int(bs, ptr->version ? 32 : 16);

	if (ptr->nb_entries > ptr->size / (ptr->version ? 8 : 6) || (u64)ptr->nb_entries > (u64)SIZE_MAX/sizeof(FECReservoirEntry) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in fecr\n", ptr->nb_entries));
		return GF_ISOM_INVALID_FILE;
	}

	ISOM_DECREASE_SIZE(ptr, ptr->nb_entries * (ptr->version ? 8 : 6) );
	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, FECReservoirEntry);
	if (!ptr->entries) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->nb_entries; i++) {
		ptr->entries[i].item_id = gf_bs_read_int(bs, ptr->version ? 32 : 16);
		ptr->entries[i].symbol_count = gf_bs_read_u32(bs);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fecr_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err fecr_box_size(GF_Box *s)
{
	FECReservoirBox *ptr = (FECReservoirBox *)s;
	ptr->size += (ptr->version ? 4 : 2) +  ptr->nb_entries * (ptr->version ? 8 : 6);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *segr_box_new()
{
	ISOM_DECL_BOX_ALLOC(FDSessionGroupBox, GF_ISOM_BOX_TYPE_SEGR);
	return (GF_Box *)tmp;
}

void segr_box_del(GF_Box *s)
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

GF_Err segr_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, k;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->num_session_groups = gf_bs_read_u16(bs);
	if (ptr->size < ptr->num_session_groups) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in segr\n", ptr->num_session_groups));
		ptr->num_session_groups = 0;
		return GF_ISOM_INVALID_FILE;
	}

	GF_SAFE_ALLOC_N(ptr->session_groups, ptr->num_session_groups, SessionGroupEntry);
	if (!ptr->session_groups) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->num_session_groups; i++) {
		ptr->session_groups[i].nb_groups = gf_bs_read_u8(bs);
		ISOM_DECREASE_SIZE(ptr, 1);

		ISOM_DECREASE_SIZE(ptr, ptr->session_groups[i].nb_groups*4);

		GF_SAFE_ALLOC_N(ptr->session_groups[i].group_ids, ptr->session_groups[i].nb_groups, u32);
		if (!ptr->session_groups[i].group_ids) return GF_OUT_OF_MEM;

		for (k=0; k<ptr->session_groups[i].nb_groups; k++) {
			ptr->session_groups[i].group_ids[k] = gf_bs_read_u32(bs);
		}

		ptr->session_groups[i].nb_channels = gf_bs_read_u16(bs);
		ISOM_DECREASE_SIZE(ptr, ptr->session_groups[i].nb_channels*4);

		GF_SAFE_ALLOC_N(ptr->session_groups[i].channels, ptr->session_groups[i].nb_channels, u32);
		if (!ptr->session_groups[i].channels) return GF_OUT_OF_MEM;

		for (k=0; k<ptr->session_groups[i].nb_channels; k++) {
			ptr->session_groups[i].channels[k] = gf_bs_read_u32(bs);
		}
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err segr_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err segr_box_size(GF_Box *s)
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


GF_Box *gitn_box_new()
{
	ISOM_DECL_BOX_ALLOC(GroupIdToNameBox, GF_ISOM_BOX_TYPE_GITN);
	return (GF_Box *)tmp;
}

void gitn_box_del(GF_Box *s)
{
	u32 i;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *)s;
	if (ptr == NULL) return;
	if (ptr->entries) {
		for (i=0; i<ptr->nb_entries; i++) {
			if (ptr->entries[i].name) gf_free(ptr->entries[i].name);
		}
		gf_free(ptr->entries);
	}
	gf_free(ptr);
}

GF_Err gitn_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *)s;

	ISOM_DECREASE_SIZE(ptr, 2);
	ptr->nb_entries = gf_bs_read_u16(bs);
	if (ptr->size / 4 < ptr->nb_entries)
		return GF_ISOM_INVALID_FILE;

	GF_SAFE_ALLOC_N(ptr->entries, ptr->nb_entries, GroupIdNameEntry);
	if (!ptr->entries) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->nb_entries; i++) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->entries[i].group_id = gf_bs_read_u32(bs);

		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->entries[i].name);
		if (e) return e;
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gitn_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err gitn_box_size(GF_Box *s)
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

GF_Box *fdpa_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FDpacketBox, GF_ISOM_BOX_TYPE_FDPA);
	return (GF_Box *)tmp;
}

void fdpa_box_del(GF_Box *s)
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

GF_Err fdpa_box_read(GF_Box *s, GF_BitStream *bs)
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
	if (ptr->size / 2 < ptr->header_ext_count) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid number of entries %d in fdpa\n", ptr->header_ext_count));
		return GF_ISOM_INVALID_FILE;
	}

	GF_SAFE_ALLOC_N(ptr->headers, ptr->header_ext_count, GF_LCTheaderExtension);
	if (!ptr->headers) return GF_OUT_OF_MEM;

	for (i=0; i<ptr->header_ext_count; i++) {
		ptr->headers[i].header_extension_type = gf_bs_read_u8(bs);
		ISOM_DECREASE_SIZE(ptr, 1);

		if (ptr->headers[i].header_extension_type > 127) {
			ISOM_DECREASE_SIZE(ptr, 3);
			gf_bs_read_data(bs, (char *) ptr->headers[i].content, 3);
		} else {
			ISOM_DECREASE_SIZE(ptr, 1);
			ptr->headers[i].data_length = gf_bs_read_u8(bs);
			if (ptr->headers[i].data_length) {
				ptr->headers[i].data_length = 4*ptr->headers[i].data_length - 2;
				if (ptr->size < sizeof(char) * ptr->headers[i].data_length)
				    return GF_ISOM_INVALID_FILE;
				ptr->headers[i].data = gf_malloc(sizeof(char) * ptr->headers[i].data_length);
				if (!ptr->headers[i].data) return GF_OUT_OF_MEM;

				ISOM_DECREASE_SIZE(ptr, ptr->headers[i].data_length);
				gf_bs_read_data(bs, ptr->headers[i].data, ptr->headers[i].data_length);
			}
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fdpa_box_write(GF_Box *s, GF_BitStream *bs)
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
	gf_bs_write_u16(bs, ptr->info.transport_object_identifier);
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

GF_Err fdpa_box_size(GF_Box *s)
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


GF_Box *extr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ExtraDataBox, GF_ISOM_BOX_TYPE_EXTR);
	return (GF_Box *)tmp;
}

void extr_box_del(GF_Box *s)
{
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *)s;
	if (ptr == NULL) return;
	if (ptr->feci) gf_isom_box_del((GF_Box*)ptr->feci);
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Err extr_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *)s;

	e = gf_isom_box_parse((GF_Box**) &ptr->feci, bs);
	if (e) return e;
	if (!ptr->feci || ptr->feci->size > ptr->size) return GF_ISOM_INVALID_MEDIA;
	ptr->data_length = (u32) (ptr->size - ptr->feci->size);
	ptr->data = gf_malloc(sizeof(char)*ptr->data_length);
	if (!ptr->data) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->data, ptr->data_length);

	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err extr_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err extr_box_size(GF_Box *s)
{
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *) s;
	ptr->size += ptr->data_length;
	if (ptr->feci) {
		GF_Err e = gf_isom_box_size((GF_Box*)ptr->feci);
		if (e) return e;
		ptr->size += ptr->feci->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *fdsa_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_HintSample, GF_ISOM_BOX_TYPE_FDSA);
	if (!tmp) return NULL;
	tmp->packetTable = gf_list_new();
	tmp->hint_subtype = GF_ISOM_BOX_TYPE_FDP_STSD;
	return (GF_Box*)tmp;
}

void fdsa_box_del(GF_Box *s)
{
	GF_HintSample *ptr = (GF_HintSample *)s;
	gf_list_del(ptr->packetTable);
	gf_free(ptr);
}

GF_Err fdsa_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_HintSample *ptr = (GF_HintSample *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_FDPA:
		BOX_FIELD_LIST_ASSIGN(packetTable)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_EXTR:
		BOX_FIELD_ASSIGN(extra_data, GF_ExtraDataBox)
		break;
	}
	return GF_OK;
}
GF_Err fdsa_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fdsa_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err fdsa_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM_HINTING*/


void trik_box_del(GF_Box *s)
{
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	if (ptr == NULL) return;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err trik_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	ptr->entry_count = (u32) ptr->size;
	if ((u64)ptr->entry_count > (u64)SIZE_MAX/sizeof(GF_TrickPlayBoxEntry)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in trik\n", ptr->size));
		return GF_ISOM_INVALID_FILE;
	}
	ptr->entries = (GF_TrickPlayBoxEntry *) gf_malloc(ptr->entry_count * sizeof(GF_TrickPlayBoxEntry) );
	if (!ptr->entries) return GF_OUT_OF_MEM;

	for (i=0; i< ptr->entry_count; i++) {
		ptr->entries[i].pic_type = gf_bs_read_int(bs, 2);
		ptr->entries[i].dependency_level = gf_bs_read_int(bs, 6);
	}
	return GF_OK;
}

GF_Box *trik_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrickPlayBox, GF_ISOM_BOX_TYPE_TRIK);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err trik_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err trik_box_size(GF_Box *s)
{
	GF_TrickPlayBox *ptr = (GF_TrickPlayBox *) s;
	ptr->size += 8 * ptr->entry_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void bloc_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err bloc_box_read(GF_Box *s,GF_BitStream *bs)
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

GF_Box *bloc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_BaseLocationBox, GF_ISOM_BOX_TYPE_TRIK);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err bloc_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err bloc_box_size(GF_Box *s)
{
	s->size += 1024;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ainf_box_del(GF_Box *s)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;
	if (ptr->APID) gf_free(ptr->APID);
	gf_free(s);
}

GF_Err ainf_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;

	ISOM_DECREASE_SIZE(s, 4)
	ptr->profile_version = gf_bs_read_u32(bs);
	return gf_isom_read_null_terminated_string(s, bs, s->size, &ptr->APID);
}

GF_Box *ainf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AssetInformationBox, GF_ISOM_BOX_TYPE_AINF);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ainf_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->profile_version);
    if (ptr->APID)
        gf_bs_write_data(bs, ptr->APID, (u32) strlen(ptr->APID) );
    gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err ainf_box_size(GF_Box *s)
{
	GF_AssetInformationBox *ptr = (GF_AssetInformationBox *) s;
    s->size += 4 + (ptr->APID ? strlen(ptr->APID) : 0 ) + 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void mhac_box_del(GF_Box *s)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;
	if (ptr->mha_config) gf_free(ptr->mha_config);
	gf_free(s);
}

GF_Err mhac_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;

	ISOM_DECREASE_SIZE(s, 5)
	ptr->configuration_version = gf_bs_read_u8(bs);
	ptr->mha_pl_indication = gf_bs_read_u8(bs);
	ptr->reference_channel_layout = gf_bs_read_u8(bs);
	ptr->mha_config_size = gf_bs_read_u16(bs);
	if (ptr->mha_config_size) {
		ISOM_DECREASE_SIZE(s, ptr->mha_config_size)

		ptr->mha_config = gf_malloc(sizeof(char)*ptr->mha_config_size);
		if (!ptr->mha_config) return GF_OUT_OF_MEM;

		gf_bs_read_data(bs, ptr->mha_config, ptr->mha_config_size);
	}
	return GF_OK;
}

GF_Box *mhac_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MHAConfigBox, GF_ISOM_BOX_TYPE_MHAC);
	tmp->configuration_version = 1;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mhac_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->configuration_version);
	gf_bs_write_u8(bs, ptr->mha_pl_indication);
	gf_bs_write_u8(bs, ptr->reference_channel_layout);
	gf_bs_write_u16(bs, ptr->mha_config ? ptr->mha_config_size : 0);
	if (ptr->mha_config && ptr->mha_config_size)
		gf_bs_write_data(bs, ptr->mha_config, ptr->mha_config_size);

	return GF_OK;
}

GF_Err mhac_box_size(GF_Box *s)
{
	GF_MHAConfigBox *ptr = (GF_MHAConfigBox *) s;
	s->size += 5;
	if (ptr->mha_config_size && ptr->mha_config) s->size += ptr->mha_config_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void mhap_box_del(GF_Box *s)
{
	GF_MHACompatibleProfilesBox *ptr = (GF_MHACompatibleProfilesBox *) s;
	if (ptr->compat_profiles) gf_free(ptr->compat_profiles);
	gf_free(s);
}

GF_Err mhap_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_MHACompatibleProfilesBox *ptr = (GF_MHACompatibleProfilesBox *) s;

	ISOM_DECREASE_SIZE(s, 1)
	ptr->num_profiles = gf_bs_read_u8(bs);
	if (!ptr->num_profiles) return GF_OK;

	ISOM_DECREASE_SIZE(s, ptr->num_profiles)
	ptr->compat_profiles = gf_malloc(sizeof(u8) * ptr->num_profiles);
	if (!ptr->compat_profiles) return GF_OUT_OF_MEM;
	for (i=0; i<ptr->num_profiles; i++) {
		ptr->compat_profiles[i] = gf_bs_read_u8(bs);
	}
	return GF_OK;
}

GF_Box *mhap_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MHACompatibleProfilesBox, GF_ISOM_BOX_TYPE_MHAP);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mhap_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_MHACompatibleProfilesBox *ptr = (GF_MHACompatibleProfilesBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->num_profiles);
	for (i=0; i<ptr->num_profiles; i++) {
		gf_bs_write_u8(bs, ptr->compat_profiles[i]);
	}
	return GF_OK;
}

GF_Err mhap_box_size(GF_Box *s)
{
	GF_MHACompatibleProfilesBox *ptr = (GF_MHACompatibleProfilesBox *) s;
	s->size += 1 + ptr->num_profiles;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void jp2h_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err jp2h_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_J2KHeaderBox *ptr = (GF_J2KHeaderBox *)s;
	switch(a->type) {
	case GF_ISOM_BOX_TYPE_IHDR:
		BOX_FIELD_ASSIGN(ihdr, GF_J2KImageHeaderBox)
		return GF_OK;
	case GF_ISOM_BOX_TYPE_COLR:
		BOX_FIELD_ASSIGN(colr, GF_ColourInformationBox)
		return GF_OK;
	}
	return GF_OK;
}
GF_Err jp2h_box_read(GF_Box *s,GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, s->type);
}

GF_Box *jp2h_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_J2KHeaderBox, GF_ISOM_BOX_TYPE_JP2H);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err jp2h_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err jp2h_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void ihdr_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err ihdr_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_J2KImageHeaderBox *ptr = (GF_J2KImageHeaderBox *) s;

	ISOM_DECREASE_SIZE(s, 14)

	ptr->height = gf_bs_read_u32(bs);
	ptr->width = gf_bs_read_u32(bs);
	ptr->nb_comp = gf_bs_read_u16(bs);
	ptr->bpc = gf_bs_read_u8(bs);
	ptr->Comp = gf_bs_read_u8(bs);
	ptr->UnkC = gf_bs_read_u8(bs);
	ptr->IPR = gf_bs_read_u8(bs);

	return GF_OK;
}

GF_Box *ihdr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_J2KImageHeaderBox, GF_ISOM_BOX_TYPE_IHDR);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ihdr_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_J2KImageHeaderBox *ptr = (GF_J2KImageHeaderBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->height);
	gf_bs_write_u32(bs, ptr->width);
	gf_bs_write_u16(bs, ptr->nb_comp);
	gf_bs_write_u8(bs, ptr->bpc);
	gf_bs_write_u8(bs, ptr->Comp);
	gf_bs_write_u8(bs, ptr->UnkC);
	gf_bs_write_u8(bs, ptr->IPR);
	return GF_OK;
}

GF_Err ihdr_box_size(GF_Box *s)
{
	s->size += 14;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* Dolby Vision */

GF_Box *dvcC_box_new()
{
	GF_DOVIConfigurationBox *tmp = (GF_DOVIConfigurationBox *)gf_malloc(sizeof(GF_DOVIConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DOVIConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_DVCC;
	return (GF_Box *)tmp;
}

void dvcC_box_del(GF_Box *s)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox*)s;
	gf_free(ptr);
}

GF_Err dvcC_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox *)s;

	//GF_DOVIDecoderConfigurationRecord
	ISOM_DECREASE_SIZE(ptr, 24)
	ptr->DOVIConfig.dv_version_major = gf_bs_read_u8(bs);
	ptr->DOVIConfig.dv_version_minor = gf_bs_read_u8(bs);
	ptr->DOVIConfig.dv_profile = gf_bs_read_int(bs, 7);
	ptr->DOVIConfig.dv_level = gf_bs_read_int(bs, 6);
	ptr->DOVIConfig.rpu_present_flag = gf_bs_read_int(bs, 1);
	ptr->DOVIConfig.el_present_flag = gf_bs_read_int(bs, 1);
	ptr->DOVIConfig.bl_present_flag = gf_bs_read_int(bs, 1);
	ptr->DOVIConfig.dv_bl_signal_compatibility_id = gf_bs_read_int(bs, 4);
	if (gf_bs_read_int(bs, 28) != 0)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] dvcC reserved bits are not zero\n"));

	for (i = 0; i < 4; i++) {
		if (gf_bs_read_u32(bs) != 0) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] dvcC reserved bits are not zero\n"));
		}
	}
	if (ptr->DOVIConfig.dv_profile==8) {
		if (!ptr->DOVIConfig.dv_bl_signal_compatibility_id || (ptr->DOVIConfig.dv_bl_signal_compatibility_id>2) ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] dvcC profile 8 but compatibility ID %d is not 1 or 2, patching to 2\n", ptr->DOVIConfig.dv_bl_signal_compatibility_id));
			ptr->DOVIConfig.dv_bl_signal_compatibility_id = 2;
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dvcC_box_write(GF_Box *s, GF_BitStream *bs)
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
	gf_bs_write_int(bs, ptr->DOVIConfig.dv_bl_signal_compatibility_id, 4);
	gf_bs_write_int(bs, 0, 28);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);

	return GF_OK;
}

GF_Err dvcC_box_size(GF_Box *s)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox *)s;

	ptr->size += 24;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dvvC_box_new()
{
	GF_DOVIConfigurationBox *tmp = (GF_DOVIConfigurationBox *)gf_malloc(sizeof(GF_DOVIConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DOVIConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_DVVC;
	return (GF_Box *)tmp;
}

void dvvC_box_del(GF_Box *s)
{
	GF_DOVIConfigurationBox *ptr = (GF_DOVIConfigurationBox*)s;
	gf_free(ptr);
}

GF_Err dvvC_box_read(GF_Box *s, GF_BitStream *bs)
{
	return dvcC_box_read(s, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err dvvC_box_write(GF_Box *s, GF_BitStream *bs)
{
	return dvcC_box_write(s, bs);
}

GF_Err dvvC_box_size(GF_Box *s)
{
	return dvcC_box_size(s);
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *dOps_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OpusSpecificBox, GF_ISOM_BOX_TYPE_DOPS);
	return (GF_Box *)tmp;
}

void dOps_box_del(GF_Box *s)
{
	GF_OpusSpecificBox *ptr = (GF_OpusSpecificBox *)s;
	if (ptr) gf_free(ptr);
}

GF_Err dOps_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_OpusSpecificBox *ptr = (GF_OpusSpecificBox *)s;
	ptr->version = gf_bs_read_u8(bs);
	ptr->OutputChannelCount = gf_bs_read_u8(bs);
	ptr->PreSkip = gf_bs_read_u16(bs);
	ptr->InputSampleRate = gf_bs_read_u32(bs);
	ptr->OutputGain = gf_bs_read_u16(bs);
	ptr->ChannelMappingFamily = gf_bs_read_u8(bs);
	ISOM_DECREASE_SIZE(ptr, 11)
	if (ptr->size) {
		ISOM_DECREASE_SIZE(ptr, 2+ptr->OutputChannelCount);
		ptr->StreamCount = gf_bs_read_u8(bs);
		ptr->CoupledCount = gf_bs_read_u8(bs);
		gf_bs_read_data(bs, (char *) ptr->ChannelMapping, ptr->OutputChannelCount);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dOps_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OpusSpecificBox *ptr = (GF_OpusSpecificBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_u8(bs, ptr->OutputChannelCount);
	gf_bs_write_u16(bs, ptr->PreSkip);
	gf_bs_write_u32(bs, ptr->InputSampleRate);
	gf_bs_write_u16(bs, ptr->OutputGain);
	gf_bs_write_u8(bs, ptr->ChannelMappingFamily);
	if (ptr->ChannelMappingFamily) {
		gf_bs_write_u8(bs, ptr->StreamCount);
		gf_bs_write_u8(bs, ptr->CoupledCount);
		gf_bs_write_data(bs, (char *) ptr->ChannelMapping, ptr->OutputChannelCount);
	}
	return GF_OK;
}

GF_Err dOps_box_size(GF_Box *s)
{
	GF_OpusSpecificBox *ptr = (GF_OpusSpecificBox *)s;
	ptr->size += 11;
	if (ptr->ChannelMappingFamily)
		ptr->size += 2 + ptr->OutputChannelCount;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void dfla_box_del(GF_Box *s)
{
	GF_FLACConfigBox *ptr = (GF_FLACConfigBox *) s;
	if (ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Err dfla_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_FLACConfigBox *ptr = (GF_FLACConfigBox *) s;
	ptr->dataSize = (u32) ptr->size;
	ptr->size=0;
	ptr->data = gf_malloc(ptr->dataSize);
	gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	return GF_OK;
}

GF_Box *dfla_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FLACConfigBox, GF_ISOM_BOX_TYPE_DFLA);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dfla_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FLACConfigBox *ptr = (GF_FLACConfigBox *) s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	return GF_OK;
}

GF_Err dfla_box_size(GF_Box *s)
{
	GF_FLACConfigBox *ptr = (GF_FLACConfigBox *) s;
	ptr->size += ptr->dataSize;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void mvcg_box_del(GF_Box *s)
{
	GF_MultiviewGroupBox *ptr = (GF_MultiviewGroupBox *) s;
	if (ptr->entries) gf_free(ptr->entries);
	gf_free(ptr);
}

GF_Err mvcg_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_MultiviewGroupBox *ptr = (GF_MultiviewGroupBox *) s;
	ISOM_DECREASE_SIZE(s, 7)
	ptr->multiview_group_id = gf_bs_read_u32(bs);
	ptr->num_entries = gf_bs_read_u16(bs);
	gf_bs_read_u8(bs);
	ptr->entries = gf_malloc(ptr->num_entries * sizeof(MVCIEntry));
	memset(ptr->entries, 0, ptr->num_entries * sizeof(MVCIEntry));
	for (i=0; i<ptr->num_entries; i++) {
		ISOM_DECREASE_SIZE(s, 1)
		ptr->entries[i].entry_type = gf_bs_read_u8(bs);
		switch (ptr->entries[i].entry_type) {
		case 0:
			ISOM_DECREASE_SIZE(s, 4)
			ptr->entries[i].trackID = gf_bs_read_u32(bs);
			break;
		case 1:
			ISOM_DECREASE_SIZE(s, 6)
			ptr->entries[i].trackID = gf_bs_read_u32(bs);
			ptr->entries[i].tierID = gf_bs_read_u16(bs);
			break;
		case 2:
			ISOM_DECREASE_SIZE(s, 2)
			gf_bs_read_int(bs, 6);
			ptr->entries[i].output_view_id = gf_bs_read_int(bs, 10);
			break;
		case 3:
			ISOM_DECREASE_SIZE(s, 4)
			gf_bs_read_int(bs, 6)	;
			ptr->entries[i].start_view_id = gf_bs_read_int(bs, 10);
			ptr->entries[i].view_count = gf_bs_read_u16(bs);
			break;
		}
	}
	return gf_isom_box_array_read(s, bs);
}

GF_Box *mvcg_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MultiviewGroupBox, GF_ISOM_BOX_TYPE_MVCG);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mvcg_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_MultiviewGroupBox *ptr = (GF_MultiviewGroupBox *) s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;


	gf_bs_write_u32(bs, ptr->multiview_group_id);
	gf_bs_write_u16(bs, ptr->num_entries);
	gf_bs_write_u8(bs, 0);

	for (i=0; i<ptr->num_entries; i++) {
		gf_bs_write_u8(bs, ptr->entries[i].entry_type);
		switch (ptr->entries[i].entry_type) {
		case 0:
			gf_bs_write_u32(bs, ptr->entries[i].trackID);
			break;
		case 1:
			gf_bs_write_u32(bs, ptr->entries[i].trackID);
			gf_bs_write_u16(bs, ptr->entries[i].tierID);
			break;
		case 2:
			gf_bs_write_int(bs, 0, 6);
			gf_bs_write_int(bs, ptr->entries[i].output_view_id, 10);
			break;
		case 3:
			gf_bs_write_int(bs, 0, 6)	;
			gf_bs_write_int(bs, ptr->entries[i].start_view_id, 10);
			gf_bs_write_u16(bs, ptr->entries[i].view_count);
			break;
		}
	}
	return GF_OK;
}

GF_Err mvcg_box_size(GF_Box *s)
{
	u32 i;
	GF_MultiviewGroupBox *ptr = (GF_MultiviewGroupBox *) s;

	ptr->size += 7;
	for (i=0; i<ptr->num_entries; i++) {
		switch (ptr->entries[i].entry_type) {
		case 0:
			ptr->size += 1 + 4;
			break;
		case 1:
			ptr->size += 1 + 6;
			break;
		case 2:
			ptr->size += 1 + 2;
			break;
		case 3:
			ptr->size += 1 + 4;
			break;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void vwid_box_del(GF_Box *s)
{
	u32 i;
	GF_ViewIdentifierBox *ptr = (GF_ViewIdentifierBox *) s;
	if (ptr->views) {
		for (i=0; i<ptr->num_views; i++) {
			if (ptr->views[i].view_refs)
				gf_free(ptr->views[i].view_refs);
		}
		gf_free(ptr->views);
	}
	gf_free(ptr);
}

GF_Err vwid_box_read(GF_Box *s,GF_BitStream *bs)
{
	u32 i;
	GF_ViewIdentifierBox *ptr = (GF_ViewIdentifierBox *) s;
	ISOM_DECREASE_SIZE(s, 3)
	gf_bs_read_int(bs, 2);
	ptr->min_temporal_id = gf_bs_read_int(bs, 3);
	ptr->max_temporal_id = gf_bs_read_int(bs, 3);
	ptr->num_views = gf_bs_read_u16(bs);
	if (ptr->num_views > ptr->size / 6)
		return GF_ISOM_INVALID_FILE;

	ptr->views = gf_malloc(sizeof(ViewIDEntry)*ptr->num_views);
	memset(ptr->views, 0, sizeof(ViewIDEntry)*ptr->num_views);
	for (i=0; i<ptr->num_views; i++) {
		u32 j;
		ISOM_DECREASE_SIZE(s, 6)

		gf_bs_read_int(bs, 6);
		ptr->views[i].view_id = gf_bs_read_int(bs, 10);
		gf_bs_read_int(bs, 6);
		ptr->views[i].view_order_index = gf_bs_read_int(bs, 10);
		ptr->views[i].texture_in_stream = gf_bs_read_int(bs, 1);
		ptr->views[i].texture_in_track = gf_bs_read_int(bs, 1);
		ptr->views[i].depth_in_stream = gf_bs_read_int(bs, 1);
		ptr->views[i].depth_in_track = gf_bs_read_int(bs, 1);
		ptr->views[i].base_view_type = gf_bs_read_int(bs, 2);
		ptr->views[i].num_ref_views = gf_bs_read_int(bs, 10);

		if (ptr->views[i].num_ref_views > ptr->size / 2)
			return GF_ISOM_INVALID_FILE;

		ptr->views[i].view_refs = gf_malloc(sizeof(ViewIDRefViewEntry)*ptr->views[i].num_ref_views);
		for (j=0; j<ptr->views[i].num_ref_views; j++) {
			ISOM_DECREASE_SIZE(s, 2)
			gf_bs_read_int(bs, 4);
			ptr->views[i].view_refs[j].dep_comp_idc = gf_bs_read_int(bs, 2);
			ptr->views[i].view_refs[j].ref_view_id = gf_bs_read_int(bs, 10);
		}
	}
	return GF_OK;
}

GF_Box *vwid_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ViewIdentifierBox, GF_ISOM_BOX_TYPE_VWID);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err vwid_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j;
	GF_ViewIdentifierBox *ptr = (GF_ViewIdentifierBox *) s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, 0, 2);
	gf_bs_write_int(bs, ptr->min_temporal_id, 3);
	gf_bs_write_int(bs, ptr->max_temporal_id, 3);
	gf_bs_write_u16(bs, ptr->num_views);

	for (i=0; i<ptr->num_views; i++) {
		gf_bs_write_int(bs, 0, 6);
		gf_bs_write_int(bs, ptr->views[i].view_id, 10);
		gf_bs_write_int(bs, 0, 6);
		gf_bs_write_int(bs, ptr->views[i].view_order_index, 10);

		gf_bs_write_int(bs, ptr->views[i].texture_in_stream, 1);
		gf_bs_write_int(bs, ptr->views[i].texture_in_track, 1);
		gf_bs_write_int(bs, ptr->views[i].depth_in_stream, 1);
		gf_bs_write_int(bs, ptr->views[i].depth_in_track, 1);
		gf_bs_write_int(bs, ptr->views[i].base_view_type, 2);
		gf_bs_write_int(bs, ptr->views[i].num_ref_views, 10);

		for (j=0; j<ptr->views[i].num_ref_views; j++) {
			gf_bs_write_int(bs, 0, 4);
			gf_bs_write_int(bs, ptr->views[i].view_refs[j].dep_comp_idc, 2);
			gf_bs_write_int(bs, ptr->views[i].view_refs[j].ref_view_id, 10);
		}
	}
	return GF_OK;
}

GF_Err vwid_box_size(GF_Box *s)
{
	u32 i;
	GF_ViewIdentifierBox *ptr = (GF_ViewIdentifierBox *) s;
	ptr->size += 3;
	for (i=0; i<ptr->num_views; i++) {
		ptr->size += 6 + 2 * ptr->views[i].num_ref_views;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void pcmC_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err pcmC_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_PCMConfigBox *ptr = (GF_PCMConfigBox *) s;

	ISOM_DECREASE_SIZE(s, 2)
	ptr->format_flags = gf_bs_read_u8(bs);
	ptr->PCM_sample_size = gf_bs_read_u8(bs);
	return GF_OK;
}

GF_Box *pcmC_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PCMConfigBox, GF_ISOM_BOX_TYPE_PCMC);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err pcmC_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PCMConfigBox *ptr = (GF_PCMConfigBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->format_flags);
	gf_bs_write_u8(bs, ptr->PCM_sample_size);
	return GF_OK;
}

GF_Err pcmC_box_size(GF_Box *s)
{
	s->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void chnl_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err chnl_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_ChannelLayoutBox *ptr = (GF_ChannelLayoutBox *) s;

	ISOM_DECREASE_SIZE(s, 1)
	ptr->layout.stream_structure = gf_bs_read_u8(bs);
	if (ptr->layout.stream_structure & 1) {
		ISOM_DECREASE_SIZE(s, 1)
		ptr->layout.definedLayout = gf_bs_read_u8(bs);
		if (ptr->layout.definedLayout) {
			u32 remain = (u32) ptr->size;
			if (ptr->layout.stream_structure & 2) remain--;
			ptr->layout.channels_count = 0;
			while (remain) {
				ISOM_DECREASE_SIZE(s, 1)
				ptr->layout.layouts[ptr->layout.channels_count].position = gf_bs_read_u8(bs);
				remain--;
				if (ptr->layout.layouts[ptr->layout.channels_count].position == 126) {
					ISOM_DECREASE_SIZE(s, 3)
					ptr->layout.layouts[ptr->layout.channels_count].azimuth = gf_bs_read_int(bs, 16);
					ptr->layout.layouts[ptr->layout.channels_count].elevation = gf_bs_read_int(bs, 8);
					remain-=3;
				}
			}
		} else {
			ISOM_DECREASE_SIZE(s, 8)
			ptr->layout.omittedChannelsMap = gf_bs_read_u64(bs);
		}
	}
	if (ptr->layout.stream_structure & 2) {
		ISOM_DECREASE_SIZE(s, 1)
		ptr->layout.object_count = gf_bs_read_u8(bs);
	}
	return GF_OK;
}

GF_Box *chnl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChannelLayoutBox, GF_ISOM_BOX_TYPE_CHNL);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err chnl_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ChannelLayoutBox *ptr = (GF_ChannelLayoutBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->layout.stream_structure);
	if (ptr->layout.stream_structure & 1) {
		gf_bs_write_u8(bs, ptr->layout.definedLayout);
		if (ptr->layout.definedLayout==0) {
			u32 i;
			for (i=0; i<ptr->layout.channels_count; i++) {
				gf_bs_write_u8(bs, ptr->layout.layouts[i].position);
				if (ptr->layout.layouts[i].position==126) {
					gf_bs_write_int(bs, ptr->layout.layouts[i].azimuth, 16);
					gf_bs_write_int(bs, ptr->layout.layouts[i].elevation, 8);
				}
			}
		} else {
			gf_bs_write_u64(bs, ptr->layout.omittedChannelsMap);
		}
	}
	if (ptr->layout.stream_structure & 2) {
		gf_bs_write_u8(bs, ptr->layout.object_count);
	}
	return GF_OK;
}

GF_Err chnl_box_size(GF_Box *s)
{
	GF_ChannelLayoutBox *ptr = (GF_ChannelLayoutBox *) s;
	s->size += 1;
	if (ptr->layout.stream_structure & 1) {
		s->size += 1;
		if (ptr->layout.definedLayout==0) {
			u32 i;
			for (i=0; i<ptr->layout.channels_count; i++) {
				s->size+=1;
				if (ptr->layout.layouts[i].position==126)
					s->size+=3;
			}
		} else {
			s->size += 8;
		}
	}
	if (ptr->layout.stream_structure & 2) {
		s->size += 1;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *emsg_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_EventMessageBox, GF_ISOM_BOX_TYPE_EMSG);
	return (GF_Box *)tmp;
}

void emsg_box_del(GF_Box *s)
{
	GF_EventMessageBox *ptr = (GF_EventMessageBox *) s;
	if (ptr == NULL) return;
	if (ptr->scheme_id_uri) gf_free(ptr->scheme_id_uri);
	if (ptr->value) gf_free(ptr->value);
	if (ptr->message_data) gf_free(ptr->message_data);
	gf_free(ptr);
}

GF_Err emsg_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_EventMessageBox *ptr = (GF_EventMessageBox*) s;

	if (ptr->version==0) {
		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->scheme_id_uri);
		if (e) return e;
		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->value);
		if (e) return e;

		ISOM_DECREASE_SIZE(ptr, 16);
		ptr->timescale = gf_bs_read_u32(bs);
		ptr->presentation_time_delta = gf_bs_read_u32(bs);
		ptr->event_duration = gf_bs_read_u32(bs);
		ptr->event_id = gf_bs_read_u32(bs);
	} else if (ptr->version==1) {
		ISOM_DECREASE_SIZE(ptr, 20);
		ptr->timescale = gf_bs_read_u32(bs);
		ptr->presentation_time_delta = gf_bs_read_u64(bs);
		ptr->event_duration = gf_bs_read_u32(bs);
		ptr->event_id = gf_bs_read_u32(bs);

		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->scheme_id_uri);
		if (e) return e;
		e = gf_isom_read_null_terminated_string(s, bs, ptr->size, &ptr->value);
		if (e) return e;
	} else {
		return GF_OK;
	}
	if (ptr->size) {
		if (ptr->size>0xFFFFFFFUL) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] emsg message data size too big ("LLU") to be loaded\n", ptr->size));
			return GF_OUT_OF_MEM;
		}
		ptr->message_data_size = (u32) ptr->size;
		ptr->message_data = gf_malloc(ptr->message_data_size);
		if (!ptr->message_data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->message_data, ptr->message_data_size);
		ptr->size = 0;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err emsg_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 len;
	GF_EventMessageBox *ptr = (GF_EventMessageBox*) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	if (ptr->version==1) {
		gf_bs_write_u32(bs, ptr->timescale);
		gf_bs_write_u64(bs, ptr->presentation_time_delta);
		gf_bs_write_u32(bs, ptr->event_duration);
		gf_bs_write_u32(bs, ptr->event_id);
	}

	len = ptr->scheme_id_uri ? (u32) strlen(ptr->scheme_id_uri) : 0;
	if (len) gf_bs_write_data(bs, ptr->scheme_id_uri, len);
	gf_bs_write_u8(bs, 0);

	len = ptr->value ? (u32) strlen(ptr->value) : 0;
	if (len) gf_bs_write_data(bs, ptr->value, len);
	gf_bs_write_u8(bs, 0);

	if (ptr->version==0) {
		gf_bs_write_u32(bs, ptr->timescale);
		gf_bs_write_u32(bs, (u32) ptr->presentation_time_delta);
		gf_bs_write_u32(bs, ptr->event_duration);
		gf_bs_write_u32(bs, ptr->event_id);
	}
	if (ptr->message_data)
		gf_bs_write_data(bs, ptr->message_data, ptr->message_data_size);
	return GF_OK;
}

GF_Err emsg_box_size(GF_Box *s)
{
	GF_EventMessageBox *ptr = (GF_EventMessageBox*) s;

	ptr->size += 4;
	if (ptr->version) {
		ptr->size += 20;
	} else {
		ptr->size += 16;
	}
	ptr->size+=2; //1 NULL-terminated strings
	if (ptr->scheme_id_uri) ptr->size += strlen(ptr->scheme_id_uri);
	if (ptr->value) ptr->size += strlen(ptr->value);
	if (ptr->message_data)
		ptr->size += ptr->message_data_size;

	return GF_OK;
}
#endif // GPAC_DISABLE_ISOM_WRITE




GF_Box *csgp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_CompactSampleGroupBox, GF_ISOM_BOX_TYPE_CSGP);
	return (GF_Box *)tmp;
}
void csgp_box_del(GF_Box *a)
{
	GF_CompactSampleGroupBox *p = (GF_CompactSampleGroupBox *)a;
	if (p->patterns) {
		u32 i;
		for (i=0; i<p->pattern_count; i++) {
			gf_free(p->patterns[i].sample_group_description_indices);
		}
		gf_free(p->patterns);
	}
	gf_free(p);
}

u32 get_size_by_code(u32 code)
{
	if (code==0) return 4;
	if (code==1) return 8;
	if (code==2) return 16;
	return 32;
}
GF_Err csgp_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, bits, gidx_mask;
	Bool index_msb_indicates_fragment_local_description, grouping_type_parameter_present;
	u32 pattern_size, scount_size, index_size;
	GF_CompactSampleGroupBox *ptr = (GF_CompactSampleGroupBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->version = gf_bs_read_u8(bs);
	ptr->flags = gf_bs_read_u24(bs);

	index_msb_indicates_fragment_local_description = (ptr->flags & (1<<7)) ? GF_TRUE : GF_FALSE;
	grouping_type_parameter_present = (ptr->flags & (1<<6)) ? GF_TRUE : GF_FALSE;

	pattern_size = get_size_by_code( ((ptr->flags>>4) & 0x3) );
	scount_size = get_size_by_code( ((ptr->flags>>2) & 0x3) );
	index_size = get_size_by_code( (ptr->flags & 0x3) );

	if (((pattern_size==4) && (scount_size!=4)) || ((pattern_size!=4) && (scount_size==4))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] compact sample gorup pattern_size and sample_count_size mare not both 4 bits\n"));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->grouping_type = gf_bs_read_u32(bs);
	if (grouping_type_parameter_present) {
		ISOM_DECREASE_SIZE(ptr, 4);
		ptr->grouping_type_parameter = gf_bs_read_u32(bs);
	}
	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->pattern_count = gf_bs_read_u32(bs);


	if ( (ptr->size / ( (pattern_size + scount_size) / 8 ) < ptr->pattern_count) || (u64)ptr->pattern_count > (u64)SIZE_MAX/sizeof(GF_CompactSampleGroupPattern) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] compact sample gorup pattern_count value (%lu) invalid\n", ptr->pattern_count));
		return GF_ISOM_INVALID_FILE;
	}

	ptr->patterns = gf_malloc(sizeof(GF_CompactSampleGroupPattern) * ptr->pattern_count);
	if (!ptr->patterns) return GF_OUT_OF_MEM;

	bits = 0;
	for (i=0; i<ptr->pattern_count; i++) {
		ptr->patterns[i].length = gf_bs_read_int(bs, pattern_size);
		ptr->patterns[i].sample_count = gf_bs_read_int(bs, scount_size);
		bits += pattern_size + scount_size;
		if (! (bits % 8)) {
			bits/=8;
			ISOM_DECREASE_SIZE(ptr, bits);
			bits=0;
		}
		if ( (u64)ptr->patterns[i].length > (u64)SIZE_MAX/sizeof(u32) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] compact sample gorup pattern #%d value (%lu) invalid\n", i, ptr->patterns[i].length));
			ptr->patterns[i].sample_group_description_indices = NULL;
			return GF_ISOM_INVALID_FILE;
		}
		ptr->patterns[i].sample_group_description_indices = gf_malloc(sizeof(u32) * ptr->patterns[i].length);
		if (!ptr->patterns[i].sample_group_description_indices) return GF_OUT_OF_MEM;
	}
	bits = 0;
	gidx_mask = ((u32)1) << (index_size-1);
	for (i=0; i<ptr->pattern_count; i++) {
		u32 j;
		for (j=0; j<ptr->patterns[i].length; j++) {
			u32 idx = gf_bs_read_int(bs, index_size);
			if (index_msb_indicates_fragment_local_description) {
				//MSB set, this is a index of a group described in the fragment
				if (idx & gidx_mask) {
					idx += 0x10000;
					idx &= ~gidx_mask;
				}
			}
			ptr->patterns[i].sample_group_description_indices[j] = idx;
			bits += index_size;

			if (! (bits % 8)) {
				bits/=8;
				ISOM_DECREASE_SIZE(ptr, bits);
				bits=0;
			}
		}
	}
	if (bits)
		gf_bs_align(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err csgp_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_CompactSampleGroupBox *ptr = (GF_CompactSampleGroupBox*)s;
	u32 pattern_size = get_size_by_code( ((ptr->flags>>4) & 0x3) );
	u32 scount_size = get_size_by_code( ((ptr->flags>>2) & 0x3) );
	u32 index_size = get_size_by_code( (ptr->flags & 0x3) );

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->version);
	gf_bs_write_int(bs, ptr->flags, 24);
	gf_bs_write_u32(bs, ptr->grouping_type);

	if (ptr->flags & (1<<6))
		gf_bs_write_u32(bs, ptr->grouping_type_parameter);

	gf_bs_write_u32(bs, ptr->pattern_count);

	for (i = 0; i<ptr->pattern_count; i++ ) {
		gf_bs_write_int(bs, ptr->patterns[i].length, pattern_size);
		gf_bs_write_int(bs, ptr->patterns[i].sample_count, scount_size);
	}

	for (i = 0; i<ptr->pattern_count; i++ ) {
		u32 j;
		for (j=0; j<ptr->patterns[i].length; j++) {
			u32 idx = ptr->patterns[i].sample_group_description_indices[j];
			if (idx > 0x10000) {
				idx -= 0x10000;
				gf_bs_write_int(bs, 1, 1);
				gf_bs_write_int(bs, idx, index_size-1);
			} else {
				gf_bs_write_int(bs, idx, index_size);
			}
		}
	}
	gf_bs_align(bs);
	return GF_OK;
}

GF_Err csgp_box_size(GF_Box *s)
{
	u32 i, bits;
	GF_CompactSampleGroupBox *ptr = (GF_CompactSampleGroupBox*)s;
	u32 pattern_size = get_size_by_code( ((ptr->flags>>4) & 0x3) );
	u32 scount_size = get_size_by_code( ((ptr->flags>>2) & 0x3) );
	u32 index_size = get_size_by_code( (ptr->flags & 0x3) );

	ptr->size += 12; //v, flags , grouping_type, pattern_length
	if (ptr->flags & (1<<6))
		ptr->size+=4;

	ptr->size += ptr->pattern_count * (pattern_size + scount_size) / 8;
	bits=0;
	for (i=0; i<ptr->pattern_count; i++)
		bits += ptr->patterns[i].length * index_size;
	ptr->size += bits/8;
	if (bits % 8) ptr->size++;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *dmlp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrueHDConfigBox, GF_ISOM_BOX_TYPE_DMLP);
	return (GF_Box *)tmp;
}

void dmlp_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err dmlp_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrueHDConfigBox *ptr = (GF_TrueHDConfigBox *)s;
	ISOM_DECREASE_SIZE(ptr, 10)
	ptr->format_info = gf_bs_read_u32(bs);
	ptr->peak_data_rate = gf_bs_read_int(bs, 15);
	gf_bs_read_int(bs, 1);
	gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err dmlp_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TrueHDConfigBox *ptr = (GF_TrueHDConfigBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->format_info);
	gf_bs_write_int(bs, ptr->peak_data_rate, 15);
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_u32(bs, 0);
	return GF_OK;
}

GF_Err dmlp_box_size(GF_Box *s)
{
	s->size += 10;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *xtra_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_XtraBox, GF_ISOM_BOX_TYPE_XTRA);
	tmp->tags = gf_list_new();
	return (GF_Box *)tmp;
}

void xtra_box_del(GF_Box *s)
{
	GF_XtraBox *ptr = (GF_XtraBox *)s;
	while (gf_list_count(ptr->tags)) {
		GF_XtraTag *tag = gf_list_pop_back(ptr->tags);
		if (tag->name) gf_free(tag->name);
		if (tag->prop_value) gf_free(tag->prop_value);
		gf_free(tag);
	}
	gf_list_del(ptr->tags);
	gf_free(s);
}

GF_Err xtra_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_XtraBox *ptr = (GF_XtraBox *)s;
	while (ptr->size) {
		GF_XtraTag *tag;
		u32 prop_type = 0;

		char *data=NULL, *data2=NULL;
		ISOM_DECREASE_SIZE_NO_ERR(ptr, 8)
		s32 tag_size = gf_bs_read_u32(bs);
		u32 name_size = gf_bs_read_u32(bs);
		if (tag_size < 8) return GF_ISOM_INVALID_FILE;

		tag_size -= 8;
		if ((tag_size>ptr->size) || (name_size>ptr->size)) {
			return GF_ISOM_INVALID_FILE;
		}
		ISOM_DECREASE_SIZE_NO_ERR(ptr, 10)

		ISOM_DECREASE_SIZE_NO_ERR(ptr, name_size)
		data = gf_malloc(sizeof(char) * (name_size+1));
		gf_bs_read_data(bs, data, name_size);
		data[name_size] = 0;
		tag_size-=name_size;

		u32 flags = gf_bs_read_u32(bs);
		u32 prop_size = gf_bs_read_u32(bs);
		tag_size-=8;

		if (prop_size>4) {
			tag_size-=2;
			prop_type = gf_bs_read_u16(bs);
			prop_size -= 6;
			ISOM_DECREASE_SIZE_NO_ERR(ptr, prop_size)
			data2 = gf_malloc(sizeof(char) * (prop_size));
			gf_bs_read_data(bs, data2, prop_size);
			tag_size-=prop_size;
		} else {
			prop_size = 0;
		}
		GF_SAFEALLOC(tag, GF_XtraTag)
		tag->flags = flags;
		tag->name = data;
		tag->prop_size = prop_size;
		tag->prop_value = data2;
		tag->prop_type = prop_type;
		gf_list_add(ptr->tags, tag);

		if (tag_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[isom] invalid tag size in Xtra !\n"));
		}
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err xtra_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_XtraBox *ptr = (GF_XtraBox *)s;
	u32 i, count = gf_list_count(ptr->tags);

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	for (i=0; i<count; i++) {
		GF_XtraTag *tag = gf_list_get(ptr->tags, i);
		u32 tag_size = 16;
		u32 name_len = tag->name ? (u32) strlen(tag->name) : 0;
		tag_size += name_len;
		if (tag->prop_value) {
			tag_size += 2 + tag->prop_size;
		}
		gf_bs_write_u32(bs, tag_size);
		gf_bs_write_u32(bs, name_len);
		gf_bs_write_data(bs, tag->name, name_len);
		gf_bs_write_u32(bs, tag->flags);
		gf_bs_write_u32(bs, 6 + tag->prop_size);
		gf_bs_write_u16(bs, tag->prop_type);
		gf_bs_write_data(bs, tag->prop_value, tag->prop_size);
	}
	return GF_OK;
}

GF_Err xtra_box_size(GF_Box *s)
{
	GF_XtraBox *ptr = (GF_XtraBox *)s;
	u32 i, count = gf_list_count(ptr->tags);
	for (i=0; i<count; i++) {
		GF_XtraTag *tag = gf_list_get(ptr->tags, i);
		ptr->size += 18 + (u32) strlen(tag->name) + tag->prop_size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *st3d_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_Stereo3DBox, GF_ISOM_BOX_TYPE_ST3D);
	return (GF_Box *)tmp;
}

void st3d_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err st3d_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Stereo3DBox *ptr = (GF_Stereo3DBox *)s;
	ISOM_DECREASE_SIZE(ptr, 1)
	ptr->stereo_type = gf_bs_read_u8(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err st3d_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Stereo3DBox *ptr = (GF_Stereo3DBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->stereo_type);
	return GF_OK;
}

GF_Err st3d_box_size(GF_Box *s)
{
	s->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_Box *svhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SphericalVideoInfoBox, GF_ISOM_BOX_TYPE_SVHD);
	return (GF_Box *)tmp;
}

void svhd_box_del(GF_Box *s)
{
	GF_SphericalVideoInfoBox *ptr = (GF_SphericalVideoInfoBox *)s;
	if (ptr->string) gf_free(ptr->string);
	gf_free(s);
}


GF_Err svhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SphericalVideoInfoBox *ptr = (GF_SphericalVideoInfoBox *)s;
	if ((u32)ptr->size >= (u32)0xFFFFFFFF) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid size %llu in svhd box\n", ptr->size));
		return GF_ISOM_INVALID_FILE;
	}
	ptr->string = gf_malloc(sizeof(char) * ((u32) ptr->size+1));
	if (!ptr->string) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->string, (u32) ptr->size);
	ptr->string[ptr->size] = 0;
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err svhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SphericalVideoInfoBox *ptr = (GF_SphericalVideoInfoBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->string)
		gf_bs_write_data(bs, ptr->string, (u32) strlen(ptr->string));
	gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err svhd_box_size(GF_Box *s)
{
	GF_SphericalVideoInfoBox *ptr = (GF_SphericalVideoInfoBox *)s;
	if (ptr->string)
		s->size += (u32) strlen(ptr->string);
	s->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *prhd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProjectionHeaderBox, GF_ISOM_BOX_TYPE_PRHD);
	return (GF_Box *)tmp;
}

void prhd_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err prhd_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ProjectionHeaderBox *ptr = (GF_ProjectionHeaderBox *)s;
	ISOM_DECREASE_SIZE(ptr, 12)
	ptr->yaw = (s32) gf_bs_read_u32(bs);
	ptr->pitch = (s32) gf_bs_read_u32(bs);
	ptr->roll = (s32) gf_bs_read_u32(bs);
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err prhd_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProjectionHeaderBox *ptr = (GF_ProjectionHeaderBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->yaw);
	gf_bs_write_u32(bs, ptr->pitch);
	gf_bs_write_u32(bs, ptr->roll);
	return GF_OK;
}

GF_Err prhd_box_size(GF_Box *s)
{
	s->size += 12;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *proj_type_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ProjectionTypeBox, GF_ISOM_BOX_TYPE_EQUI); //will be overwritten
	return (GF_Box *)tmp;
}

void proj_type_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err proj_type_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ProjectionTypeBox *ptr = (GF_ProjectionTypeBox *)s;

	if (ptr->type==GF_ISOM_BOX_TYPE_CBMP) {
		ISOM_DECREASE_SIZE(ptr, 8)
		ptr->layout = gf_bs_read_u32(bs);
		ptr->padding = gf_bs_read_u32(bs);
	}
	else if (ptr->type==GF_ISOM_BOX_TYPE_EQUI) {
		ISOM_DECREASE_SIZE(ptr, 16)
		ptr->bounds_top = gf_bs_read_u32(bs);
		ptr->bounds_bottom = gf_bs_read_u32(bs);
		ptr->bounds_left = gf_bs_read_u32(bs);
		ptr->bounds_right = gf_bs_read_u32(bs);
	} else {
		ISOM_DECREASE_SIZE(ptr, 8)
		ptr->crc = gf_bs_read_u32(bs);
		ptr->encoding_4cc = gf_bs_read_u32(bs);
	}
	return gf_isom_box_array_read(s, bs);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err proj_type_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ProjectionTypeBox *ptr = (GF_ProjectionTypeBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->type==GF_ISOM_BOX_TYPE_CBMP) {
		gf_bs_write_u32(bs, ptr->layout);
		gf_bs_write_u32(bs, ptr->padding);
	}
	else if (ptr->type==GF_ISOM_BOX_TYPE_EQUI) {
		gf_bs_write_u32(bs, ptr->bounds_top);
		gf_bs_write_u32(bs, ptr->bounds_bottom);
		gf_bs_write_u32(bs, ptr->bounds_left);
		gf_bs_write_u32(bs, ptr->bounds_right);
	} else {
		gf_bs_write_u32(bs, ptr->crc);
		gf_bs_write_u32(bs, ptr->encoding_4cc);
	}
	return GF_OK;
}

GF_Err proj_type_box_size(GF_Box *s)
{
	GF_ProjectionTypeBox *ptr = (GF_ProjectionTypeBox *)s;
	if (ptr->type==GF_ISOM_BOX_TYPE_CBMP)
		s->size += 8;
	else if (ptr->type==GF_ISOM_BOX_TYPE_EQUI)
		s->size += 16;
	else
		s->size += 8;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
