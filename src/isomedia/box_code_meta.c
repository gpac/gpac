/*
 *			GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato / Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM

GF_Box *meta_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MetaBox, GF_ISOM_BOX_TYPE_META);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void meta_reset(GF_Box *s)
{
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (ptr == NULL) return;

	gf_isom_box_del((GF_Box *)ptr->handler);
	ptr->handler = NULL;
	if (ptr->primary_resource) gf_isom_box_del((GF_Box *)ptr->primary_resource);
	ptr->primary_resource = NULL;
	if (ptr->file_locations) gf_isom_box_del((GF_Box *)ptr->file_locations);
	ptr->file_locations = NULL;
	if (ptr->item_locations) gf_isom_box_del((GF_Box *)ptr->item_locations);
	ptr->item_locations = NULL;
	if (ptr->protections) gf_isom_box_del((GF_Box *)ptr->protections);
	ptr->protections = NULL;
	if (ptr->item_infos) gf_isom_box_del((GF_Box *)ptr->item_infos);
	ptr->item_infos = NULL;
	if (ptr->IPMP_control) gf_isom_box_del((GF_Box *)ptr->IPMP_control);
	ptr->IPMP_control = NULL;
	if (ptr->item_refs) gf_isom_box_del((GF_Box *)ptr->item_refs);
	ptr->item_refs = NULL;
	if (ptr->item_props) gf_isom_box_del((GF_Box *)ptr->item_props);
	ptr->item_props = NULL;
	if (ptr->other_boxes) gf_isom_box_array_del(ptr->other_boxes);
	ptr->other_boxes = NULL;
}

void meta_del(GF_Box *s)
{
	meta_reset(s);
	gf_free(s);
}


GF_Err meta_AddBox(GF_Box *s, GF_Box *a)
{
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_HDLR:
		if (ptr->handler) return GF_ISOM_INVALID_FILE;
		ptr->handler = (GF_HandlerBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_PITM:
		if (ptr->primary_resource) return GF_ISOM_INVALID_FILE;
		ptr->primary_resource = (GF_PrimaryItemBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_DINF:
		if (ptr->file_locations) return GF_ISOM_INVALID_FILE;
		ptr->file_locations = (GF_DataInformationBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_ILOC:
		if (ptr->item_locations) return GF_ISOM_INVALID_FILE;
		ptr->item_locations = (GF_ItemLocationBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_IPRO:
		if (ptr->protections) return GF_ISOM_INVALID_FILE;
		ptr->protections = (GF_ItemProtectionBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_IINF:
		if (ptr->item_infos) return GF_ISOM_INVALID_FILE;
		ptr->item_infos = (GF_ItemInfoBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_IREF:
		if (ptr->item_refs) return GF_ISOM_INVALID_FILE;
		ptr->item_refs = (GF_ItemReferenceBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_IPRP:
		if (ptr->item_props) return GF_ISOM_INVALID_FILE;
		ptr->item_props = (GF_ItemPropertiesBox*)a;
		break;
	//case ???: ptr->IPMP_control = (???*)a; break;
	case GF_ISOM_BOX_TYPE_XML:
	case GF_ISOM_BOX_TYPE_BXML:
	case GF_ISOM_BOX_TYPE_ILST:
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err meta_Read(GF_Box *s, GF_BitStream *bs)
{
	u64 pos = gf_bs_get_position(bs);
	u64 size = s->size;
	GF_Err e = gf_isom_box_array_read(s, bs, meta_AddBox);
	/*try to hack around QT files which don't use a full box for meta, rewind 4 bytes*/
	if (e && (pos>4) ) {
		gf_bs_seek(bs, pos-4);
		meta_reset(s);
		s->size = size+4;
		e = gf_isom_box_array_read(s, bs, meta_AddBox);
	}
	return e;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err meta_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->handler) {
		e = gf_isom_box_write((GF_Box *) ptr->handler, bs);
		if (e) return e;
	}
	if (ptr->primary_resource) {
		e = gf_isom_box_write((GF_Box *) ptr->primary_resource, bs);
		if (e) return e;
	}
	if (ptr->file_locations) {
		e = gf_isom_box_write((GF_Box *) ptr->file_locations, bs);
		if (e) return e;
	}
	if (ptr->item_locations) {
		e = gf_isom_box_write((GF_Box *) ptr->item_locations, bs);
		if (e) return e;
	}
	if (ptr->protections) {
		e = gf_isom_box_write((GF_Box *) ptr->protections, bs);
		if (e) return e;
	}
	if (ptr->item_infos) {
		e = gf_isom_box_write((GF_Box *) ptr->item_infos, bs);
		if (e) return e;
	}
	if (ptr->IPMP_control) {
		e = gf_isom_box_write((GF_Box *) ptr->IPMP_control, bs);
		if (e) return e;
	}
	if (ptr->item_refs) {
		e = gf_isom_box_write((GF_Box *)ptr->item_refs, bs);
		if (e) return e;
	}
	if (ptr->item_props) {
		e = gf_isom_box_write((GF_Box *) ptr->item_props, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err meta_Size(GF_Box *s)
{
	GF_Err e;
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (!s) return GF_BAD_PARAM;
	if (ptr->handler) {
		e = gf_isom_box_size((GF_Box *) ptr->handler);
		if (e) return e;
		ptr->size += ptr->handler->size;
	}
	if (ptr->primary_resource) {
		e = gf_isom_box_size((GF_Box *) ptr->primary_resource);
		if (e) return e;
		ptr->size += ptr->primary_resource->size;
	}
	if (ptr->file_locations) {
		e = gf_isom_box_size((GF_Box *) ptr->file_locations);
		if (e) return e;
		ptr->size += ptr->file_locations->size;
	}
	if (ptr->item_locations) {
		e = gf_isom_box_size((GF_Box *) ptr->item_locations);
		if (e) return e;
		ptr->size += ptr->item_locations->size;
	}
	if (ptr->protections) {
		e = gf_isom_box_size((GF_Box *) ptr->protections);
		if (e) return e;
		ptr->size += ptr->protections->size;
	}
	if (ptr->item_infos) {
		e = gf_isom_box_size((GF_Box *) ptr->item_infos);
		if (e) return e;
		ptr->size += ptr->item_infos->size;
	}
	if (ptr->IPMP_control) {
		e = gf_isom_box_size((GF_Box *) ptr->IPMP_control);
		if (e) return e;
		ptr->size += ptr->IPMP_control->size;
	}
	if (ptr->item_refs) {
		e = gf_isom_box_size((GF_Box *)ptr->item_refs);
		if (e) return e;
		ptr->size += ptr->item_refs->size;
	}
	if (ptr->item_props) {
		e = gf_isom_box_size((GF_Box *) ptr->item_props);
		if (e) return e;
		ptr->size += ptr->item_props->size;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *xml_New()
{
	ISOM_DECL_BOX_ALLOC(GF_XMLBox, GF_ISOM_BOX_TYPE_XML);
	return (GF_Box *)tmp;
}

void xml_del(GF_Box *s)
{
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (ptr == NULL) return;
	if (ptr->xml) gf_free(ptr->xml);
	gf_free(ptr);
}

GF_Err xml_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	return gf_isom_read_null_terminated_string(s, bs, s->size, &ptr->xml);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err xml_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->xml) gf_bs_write_data(bs, ptr->xml, (u32) strlen(ptr->xml));
	gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err xml_Size(GF_Box *s)
{
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += strlen(ptr->xml)+1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *bxml_New()
{
	ISOM_DECL_BOX_ALLOC(GF_BinaryXMLBox, GF_ISOM_BOX_TYPE_BXML);
	return (GF_Box *)tmp;
}

void bxml_del(GF_Box *s)
{
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;
	if (ptr == NULL) return;
	if (ptr->data_length && ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Err bxml_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;

	ptr->data_length = (u32)(ptr->size);
	ptr->data = (char*)gf_malloc(sizeof(char)*ptr->data_length);
	if (!ptr->data) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->data, ptr->data_length);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err bxml_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->data_length) gf_bs_write_data(bs, ptr->data, ptr->data_length);
	return GF_OK;
}

GF_Err bxml_Size(GF_Box *s)
{
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += ptr->data_length;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iloc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemLocationBox, GF_ISOM_BOX_TYPE_ILOC);
	tmp->location_entries = gf_list_new();
	return (GF_Box *)tmp;
}

void iloc_entry_del(GF_ItemLocationEntry *location)
{
	u32 j, extent_count;
	extent_count = gf_list_count(location->extent_entries);
	for (j = 0; j < extent_count; j++) {
		GF_ItemExtentEntry *extent = (GF_ItemExtentEntry *)gf_list_get(location->extent_entries, j);
		gf_free(extent);
	}
	gf_list_del(location->extent_entries);
	gf_free(location);
}

void iloc_del(GF_Box *s)
{
	u32 i, item_count;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (ptr == NULL) return;
	item_count = gf_list_count(ptr->location_entries);
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		iloc_entry_del(location);
	}
	gf_list_del(ptr->location_entries);
	gf_free(ptr);
}

GF_Err iloc_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 item_count, extent_count, i, j;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;

	ptr->offset_size = gf_bs_read_int(bs, 4);
	ptr->length_size = gf_bs_read_int(bs, 4);
	ptr->base_offset_size = gf_bs_read_int(bs, 4);
	if (ptr->version == 1 || ptr->version == 2) {
		ptr->index_size = gf_bs_read_int(bs, 4);
	}
	else {
		gf_bs_read_int(bs, 4);
	}
	if (ptr->version < 2) {
		item_count = gf_bs_read_u16(bs);
	}
	else {
		item_count = gf_bs_read_u32(bs);
	}
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location_entry = (GF_ItemLocationEntry *)gf_malloc(sizeof(GF_ItemLocationEntry));
		gf_list_add(ptr->location_entries, location_entry);
		if (ptr->version < 2) {
			location_entry->item_ID = gf_bs_read_u16(bs);
		}
		else {
			location_entry->item_ID = gf_bs_read_u32(bs);
		}
		if (ptr->version == 1 || ptr->version == 2) {
			location_entry->construction_method = gf_bs_read_u16(bs);
		}
		else {
			location_entry->construction_method = 0;
		}
		location_entry->data_reference_index = gf_bs_read_u16(bs);
		location_entry->base_offset = gf_bs_read_int(bs, 8*ptr->base_offset_size);
#ifndef GPAC_DISABLE_ISOM_WRITE
		location_entry->original_base_offset = location_entry->base_offset;
#endif

		extent_count = gf_bs_read_u16(bs);
		location_entry->extent_entries = gf_list_new();
		for (j = 0; j < extent_count; j++) {
			GF_ItemExtentEntry *extent_entry = (GF_ItemExtentEntry *)gf_malloc(sizeof(GF_ItemExtentEntry));
			gf_list_add(location_entry->extent_entries, extent_entry);
			if ((ptr->version == 1 || ptr->version == 2) && ptr->index_size > 0) {
				extent_entry->extent_index = gf_bs_read_int(bs, 8 * ptr->index_size);;
			}
			else {
				extent_entry->extent_index = 0;
			}
			extent_entry->extent_offset = gf_bs_read_int(bs, 8*ptr->offset_size);
			extent_entry->extent_length = gf_bs_read_int(bs, 8*ptr->length_size);
#ifndef GPAC_DISABLE_ISOM_WRITE
			extent_entry->original_extent_offset = extent_entry->extent_offset;
#endif
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iloc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, j, item_count, extent_count;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->offset_size, 4);
	gf_bs_write_int(bs, ptr->length_size, 4);
	gf_bs_write_int(bs, ptr->base_offset_size, 4);
	gf_bs_write_int(bs, ptr->index_size, 4);
	item_count = gf_list_count(ptr->location_entries);
	if (ptr->version < 2) {
		gf_bs_write_u16(bs, item_count);
	}
	else {
		gf_bs_write_u32(bs, item_count);
	}
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		if (ptr->version < 2) {
			gf_bs_write_u16(bs, location->item_ID);
		}
		else {
			gf_bs_write_u32(bs, location->item_ID);
		}
		if (ptr->version == 1 || ptr->version == 2) {
			gf_bs_write_u16(bs, location->construction_method);
		}
		gf_bs_write_u16(bs, location->data_reference_index);
		gf_bs_write_long_int(bs, location->base_offset, 8*ptr->base_offset_size);
		extent_count = gf_list_count(location->extent_entries);
		gf_bs_write_u16(bs, extent_count);
		for (j=0; j<extent_count; j++) {
			GF_ItemExtentEntry *extent = (GF_ItemExtentEntry *)gf_list_get(location->extent_entries, j);
			if ((ptr->version == 1 || ptr->version == 2) && ptr->index_size > 0) {
				gf_bs_write_long_int(bs, extent->extent_index, 8 * ptr->index_size);
			}
			gf_bs_write_long_int(bs, extent->extent_offset, 8*ptr->offset_size);
			gf_bs_write_long_int(bs, extent->extent_length, 8*ptr->length_size);
		}
	}
	return GF_OK;
}

GF_Err iloc_Size(GF_Box *s)
{
	u32 i, item_count, extent_count;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (!s) return GF_BAD_PARAM;
	if (ptr->index_size) {
		ptr->version = 1;
	}
	item_count = gf_list_count(ptr->location_entries);
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		if (location->construction_method) {
			ptr->version = 1;
		}
		if (location->item_ID > 0xFFFF) {
			ptr->version = 2;
		}
	}
	ptr->size += 4;
	if (ptr->version == 2) {
		ptr->size += 2; // 32 bits item count instead of 16 bits
	}
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		extent_count = gf_list_count(location->extent_entries);
		ptr->size += 6 + ptr->base_offset_size + extent_count*(ptr->offset_size + ptr->length_size);
		if (ptr->version == 2) {
			ptr->size += 2; //32 bits item ID instead of 16 bits
		}
		if (ptr->version == 1 || ptr->version == 2) {
			ptr->size += 2; // construction_method
			ptr->size += extent_count*ptr->index_size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *pitm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PrimaryItemBox, GF_ISOM_BOX_TYPE_PITM);
	return (GF_Box *)tmp;
}

void pitm_del(GF_Box *s)
{
	GF_PrimaryItemBox *ptr = (GF_PrimaryItemBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err pitm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_PrimaryItemBox *ptr = (GF_PrimaryItemBox *)s;

	ptr->item_ID = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err pitm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_PrimaryItemBox *ptr = (GF_PrimaryItemBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->item_ID);
	return GF_OK;
}

GF_Err pitm_Size(GF_Box *s)
{
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += 2;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipro_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemProtectionBox, GF_ISOM_BOX_TYPE_IPRO);
	tmp->protection_information = gf_list_new();
	return (GF_Box *)tmp;
}

void ipro_del(GF_Box *s)
{
	u32 count, i;
	GF_ItemProtectionBox *ptr = (GF_ItemProtectionBox *)s;
	if (ptr == NULL) return;
	count = gf_list_count(ptr->protection_information);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->protection_information, i);
		gf_isom_box_del(a);
	}
	gf_list_del(ptr->protection_information);
	gf_free(ptr);
}

GF_Err ipro_AddBox(GF_Box *s, GF_Box *a)
{
	GF_ItemProtectionBox *ptr = (GF_ItemProtectionBox *)s;
	if (a->type == GF_ISOM_BOX_TYPE_SINF)
		return gf_list_add(ptr->protection_information, a);
	else
		return gf_isom_box_add_default(s, a);
}
GF_Err ipro_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, ipro_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ipro_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 count, i;
	GF_Err e;
	GF_ItemProtectionBox *ptr = (GF_ItemProtectionBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = gf_list_count(ptr->protection_information);
	gf_bs_write_u16(bs, count);
	if (count) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->protection_information, i);
			e = gf_isom_box_write(a, bs);
			if (e) return e;
		}
	}
	return GF_OK;
}

GF_Err ipro_Size(GF_Box *s)
{
	u32 i, count;
	GF_Err e;
	GF_ItemProtectionBox *ptr = (GF_ItemProtectionBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += 2;
	if ((count = gf_list_count(ptr->protection_information))) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->protection_information, i);
			e = gf_isom_box_size(a);
			if (e) return e;
			ptr->size += a->size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *infe_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemInfoEntryBox, GF_ISOM_BOX_TYPE_INFE);
	return (GF_Box *)tmp;
}

void infe_del(GF_Box *s)
{
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;
	if (ptr == NULL) return;
	if (ptr->item_name) gf_free(ptr->item_name);
	if (ptr->full_path) gf_free(ptr->full_path);
	if (ptr->content_type) gf_free(ptr->content_type);
	if (ptr->content_encoding) gf_free(ptr->content_encoding);
	gf_free(ptr);
}

GF_Err infe_Read(GF_Box *s, GF_BitStream *bs)
{
	char *buf;
	u32 buf_len, i, string_len, string_start;
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;

	ptr->item_ID = gf_bs_read_u16(bs);
	ptr->item_protection_index = gf_bs_read_u16(bs);
	ISOM_DECREASE_SIZE(ptr, 4);
	if (ptr->version == 2) {
		ptr->item_type = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, 4);
	}
	buf_len = (u32) (ptr->size);
	buf = (char*)gf_malloc(buf_len);
	if (buf_len != gf_bs_read_data(bs, buf, buf_len)) {
		gf_free(buf);
		return GF_ISOM_INVALID_FILE;
	}
	string_len = 1;
	string_start = 0;
	for (i = 0; i < buf_len; i++) {
		if (buf[i] == 0) {
			if (!ptr->item_name) {
				ptr->item_name = (char*)gf_malloc(sizeof(char)*string_len);
				memcpy(ptr->item_name, buf+string_start, string_len);
			} else if (!ptr->content_type) {
				ptr->content_type = (char*)gf_malloc(sizeof(char)*string_len);
				memcpy(ptr->content_type, buf+string_start, string_len);
			} else {
				ptr->content_encoding = (char*)gf_malloc(sizeof(char)*string_len);
				memcpy(ptr->content_encoding, buf+string_start, string_len);
			}
			string_start += string_len;
			string_len = 0;
			if (ptr->content_encoding && ptr->version == 1) {
				break;
			}
		}
		string_len++;
	}
	gf_free(buf);
	if (!ptr->item_name || (!ptr->content_type && ptr->version < 2)) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err infe_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 len;
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->version == 3) {
		gf_bs_write_u32(bs, ptr->item_ID);
	}
	else {
		gf_bs_write_u16(bs, ptr->item_ID);
	}
	gf_bs_write_u16(bs, ptr->item_protection_index);
	if (ptr->version >= 2) {
		gf_bs_write_u32(bs, ptr->item_type);
	}
	if (ptr->item_name) {
		len = (u32) strlen(ptr->item_name)+1;
		gf_bs_write_data(bs, ptr->item_name, len);
	} else {
		gf_bs_write_byte(bs, 0, 1);
	}
	if (ptr->item_type == GF_4CC('m', 'i', 'm', 'e') || ptr->item_type == GF_4CC('u', 'r', 'i', ' ')) {
		if (ptr->content_type) {
			len = (u32)strlen(ptr->content_type) + 1;
			gf_bs_write_data(bs, ptr->content_type, len);
		}
		else {
			gf_bs_write_byte(bs, 0, 1);
		}
	}
	if (ptr->item_type == GF_4CC('m', 'i', 'm', 'e')) {
		if (ptr->content_encoding) {
			len = (u32)strlen(ptr->content_encoding) + 1;
			gf_bs_write_data(bs, ptr->content_encoding, len);
		}
		else {
			gf_bs_write_byte(bs, 0, 1);
		}
	}
	return GF_OK;
}

GF_Err infe_Size(GF_Box *s)
{
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;
	if (!s) return GF_BAD_PARAM;
	if (ptr->item_type) {
		ptr->version = 2;
		if (ptr->item_ID > 0xFFFF) {
			ptr->version = 3;
		}
	}
	else {
		ptr->version = 0;
	}
	ptr->size += 4;
	if (ptr->version == 3) {
		ptr->size += 2; // item_ID on 32 bits (+2 bytes)
	}
	if (ptr->version >= 2) {
		ptr->size += 4; // item_type size
	}
	if (ptr->item_name) ptr->size += strlen(ptr->item_name)+1;
	else ptr->size += 1;
	if (ptr->item_type == GF_4CC('m', 'i', 'm', 'e') || ptr->item_type == GF_4CC('u', 'r', 'i', ' ')) {
		if (ptr->content_type) ptr->size += strlen(ptr->content_type) + 1;
		else ptr->size += 1;
	}
	if (ptr->item_type == GF_4CC('m','i','m','e')) {
		if (ptr->content_encoding) ptr->size += strlen(ptr->content_encoding) + 1;
		else ptr->size += 1;
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iinf_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemInfoBox, GF_ISOM_BOX_TYPE_IINF);
	tmp->item_infos = gf_list_new();
	return (GF_Box *)tmp;
}

void iinf_del(GF_Box *s)
{
	u32 count, i;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;
	if (ptr == NULL) return;
	count = gf_list_count(ptr->item_infos);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->item_infos, i);
		gf_isom_box_del(a);
	}
	gf_list_del(ptr->item_infos);
	gf_free(ptr);
}

GF_Err iinf_AddBox(GF_Box *s, GF_Box *a)
{
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;

	if (a->type == GF_ISOM_BOX_TYPE_INFE) {
		return gf_list_add(ptr->item_infos, a);
	} else {
		return gf_isom_box_add_default(s, a);
	}
}

GF_Err iinf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;

	if (ptr->version == 0) {
		ISOM_DECREASE_SIZE(s, 2)
		gf_bs_read_u16(bs);
	} else {
		ISOM_DECREASE_SIZE(s, 4)
		gf_bs_read_u32(bs);
	}
	return gf_isom_box_array_read(s, bs, iinf_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iinf_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 count;
	GF_Err e;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = gf_list_count(ptr->item_infos);
	gf_bs_write_u16(bs, count);

	if (count) {
		gf_isom_box_array_write(s, ptr->item_infos, bs);
	}
	return GF_OK;
}

GF_Err iinf_Size(GF_Box *s)
{
	u32 count;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	ptr->size += 2;
	if ((count = gf_list_count(ptr->item_infos))) {
		gf_isom_box_array_size(s, ptr->item_infos);
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Err iref_AddBox(GF_Box *s, GF_Box *a)
{
	GF_ItemReferenceBox *ptr = (GF_ItemReferenceBox *)s;
	return gf_list_add(ptr->references, a);
}

void iref_del(GF_Box *s)
{
	u32 count, i;
	GF_ItemReferenceBox *ptr = (GF_ItemReferenceBox *)s;
	if (ptr == NULL) return;
	count = gf_list_count(ptr->references);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->references, i);
		gf_isom_box_del(a);
	}
	gf_list_del(ptr->references);
	gf_free(ptr);
}


GF_Err iref_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, iref_AddBox, s->type);
}

GF_Box *iref_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemReferenceBox, GF_ISOM_BOX_TYPE_IREF);
	tmp->references = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err iref_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 count, i;
	GF_ItemReferenceBox *ptr = (GF_ItemReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = gf_list_count(ptr->references);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->references, i);
		e = gf_isom_box_write(a, bs);
		if (e) return e;
	}
	return e;
}

GF_Err iref_Size(GF_Box *s)
{
	GF_Err e;
	u32 count, i;
	GF_ItemReferenceBox *ptr = (GF_ItemReferenceBox *)s;
	if (!s) return GF_BAD_PARAM;
	count = gf_list_count(ptr->references);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->references, i);
		e = gf_isom_box_size(a);		
		if (e) return e;
		s->size += a->size;
	}
	return e;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ireftype_del(GF_Box *s)
{
	GF_ItemReferenceTypeBox *ptr = (GF_ItemReferenceTypeBox *)s;
	if (!ptr) return;
	if (ptr->to_item_IDs) gf_free(ptr->to_item_IDs);
	gf_free(ptr);
}

GF_Err ireftype_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	u32 i;
	GF_ItemReferenceTypeBox *ptr = (GF_ItemReferenceTypeBox *)s;

	bytesToRead = (u32)(ptr->size);
	if (!bytesToRead) return GF_OK;

	ptr->from_item_id = gf_bs_read_u16(bs);
	ptr->reference_count = gf_bs_read_u16(bs);
	ptr->to_item_IDs = (u32 *)gf_malloc(ptr->reference_count * sizeof(u32));
	if (!ptr->to_item_IDs) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->reference_count; i++) {
		ptr->to_item_IDs[i] = gf_bs_read_u16(bs);
	}
	return GF_OK;
}

GF_Box *ireftype_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemReferenceTypeBox, GF_ISOM_BOX_TYPE_REFI);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ireftype_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_ItemReferenceTypeBox *ptr = (GF_ItemReferenceTypeBox *)s;
	ptr->type = ptr->reference_type;
	e = gf_isom_box_write_header(s, bs);
	ptr->type = GF_ISOM_BOX_TYPE_REFI;
	if (e) return e;
	gf_bs_write_u16(bs, ptr->from_item_id);
	gf_bs_write_u16(bs, ptr->reference_count);
	for (i = 0; i < ptr->reference_count; i++) {
		gf_bs_write_u16(bs, ptr->to_item_IDs[i]);
	}
	return GF_OK;
}


GF_Err ireftype_Size(GF_Box *s)
{
	GF_ItemReferenceTypeBox *ptr = (GF_ItemReferenceTypeBox *)s;
	ptr->size += 4 + (ptr->reference_count * sizeof(u16));
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/
#endif /*GPAC_DISABLE_ISOM*/

