/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato / Jean Le Feuvre 2005
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
 *
 *  GPAC is free software you can redistribute it and/or modify
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
	GF_MetaBox *tmp = (GF_MetaBox *) gf_malloc(sizeof(GF_MetaBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MetaBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_META;
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void meta_del(GF_Box *s)
{
	u32 count, i;
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_del((GF_Box *)ptr->handler);
	if (ptr->primary_resource) gf_isom_box_del((GF_Box *)ptr->primary_resource);
	if (ptr->file_locations) gf_isom_box_del((GF_Box *)ptr->file_locations);
	if (ptr->item_locations) gf_isom_box_del((GF_Box *)ptr->item_locations);
	if (ptr->protections) gf_isom_box_del((GF_Box *)ptr->protections);
	if (ptr->item_infos) gf_isom_box_del((GF_Box *)ptr->item_infos);
	if (ptr->IPMP_control) gf_isom_box_del((GF_Box *)ptr->IPMP_control);
	count = gf_list_count(ptr->other_boxes);
	for (i = 0; i < count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(ptr->other_boxes, i);
		gf_isom_box_del(a);
	}
	gf_list_del(ptr->other_boxes);
	gf_free(ptr);
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
	//case ???: ptr->IPMP_control = (???*)a; break;
	case GF_ISOM_BOX_TYPE_XML: 
	case GF_ISOM_BOX_TYPE_BXML: 
	case GF_ISOM_BOX_TYPE_ILST: 
		gf_list_add(ptr->other_boxes, a); break;
	default: 
		gf_isom_box_del(a); 
		break;
	}
	return GF_OK;
}

GF_Err meta_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 next_size = gf_bs_peek_bits(bs, 32, 4);
	GF_Err e;
	/*try to hack around QT files which don't use a full box for meta*/
	if (next_size<s->size) {
		e = gf_isom_full_box_read(s, bs);
		if (e) return e;
	}
	return gf_isom_read_box_list(s, bs, meta_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err meta_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 count, i;
	GF_Err e;
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) ptr->handler, bs);
	if (e) return e;
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
	if ((count = gf_list_count(ptr->other_boxes))) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->other_boxes, i);
			e = gf_isom_box_write(a, bs);
			if (e) return e;
		}
	}
	return GF_OK;
}

GF_Err meta_Size(GF_Box *s)
{
	u32 i, count;
	GF_Err e;
	GF_MetaBox *ptr = (GF_MetaBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	e = gf_isom_box_size((GF_Box *) ptr->handler);
	if (e) return e;
	ptr->size += ptr->handler->size;
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
	if ((count = gf_list_count(ptr->other_boxes))) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->other_boxes, i);
			e = gf_isom_box_size(a);
			if (e) return e;
			ptr->size += a->size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *xml_New()
{
	GF_XMLBox *tmp = (GF_XMLBox *) gf_malloc(sizeof(GF_XMLBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_XMLBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_XML;
	return (GF_Box *)tmp;
}

void xml_del(GF_Box *s)
{
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (ptr == NULL) return;
	if (ptr->xml_length && ptr->xml) gf_free(ptr->xml);
	gf_free(ptr);
}

GF_Err xml_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->xml_length = (u32)(ptr->size);
	ptr->xml = (char *)gf_malloc(sizeof(char)*ptr->xml_length);
	if (!ptr->xml) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ptr->xml, ptr->xml_length);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err xml_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	if (ptr->xml_length) gf_bs_write_data(bs, ptr->xml, ptr->xml_length);
	return GF_OK;
}

GF_Err xml_Size(GF_Box *s)
{
	GF_Err e;
	GF_XMLBox *ptr = (GF_XMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->xml_length;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *bxml_New()
{
	GF_BinaryXMLBox *tmp = (GF_BinaryXMLBox *) gf_malloc(sizeof(GF_BinaryXMLBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_BinaryXMLBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_BXML;
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
	GF_Err e;
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
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
	GF_Err e;
	GF_BinaryXMLBox *ptr = (GF_BinaryXMLBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += ptr->data_length;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iloc_New()
{
	GF_ItemLocationBox *tmp = (GF_ItemLocationBox *) gf_malloc(sizeof(GF_ItemLocationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemLocationBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_ILOC;
	tmp->location_entries = gf_list_new();
	return (GF_Box *)tmp;
}

void iloc_del(GF_Box *s)
{
	u32 i, j, item_count, extent_count;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (ptr == NULL) return;
	item_count = gf_list_count(ptr->location_entries);
	if (item_count) {
		for (i = 0; i < item_count; i++) {
			GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
			extent_count = gf_list_count(location->extent_entries);
			for (j = 0; j < extent_count; j++) {
				GF_ItemExtentEntry *extent = (GF_ItemExtentEntry *)gf_list_get(location->extent_entries, j);
				gf_free(extent);
			}
			gf_list_del(location->extent_entries);
			gf_free(location);
		}
		gf_list_del(ptr->location_entries);
	}
	gf_free(ptr);
}

GF_Err iloc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 item_count, extent_count, i, j;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->offset_size = gf_bs_read_int(bs, 4);
	ptr->length_size = gf_bs_read_int(bs, 4);
	ptr->base_offset_size = gf_bs_read_int(bs, 4);
	gf_bs_read_int(bs, 4);
	item_count = gf_bs_read_u16(bs);
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location_entry = (GF_ItemLocationEntry *)gf_malloc(sizeof(GF_ItemLocationEntry));
		gf_list_add(ptr->location_entries, location_entry);
		location_entry->item_ID = gf_bs_read_u16(bs);
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
	gf_bs_write_int(bs, 0, 4);
	item_count = gf_list_count(ptr->location_entries);
	gf_bs_write_u16(bs, item_count);
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		gf_bs_write_u16(bs, location->item_ID);
		gf_bs_write_u16(bs, location->data_reference_index);
		gf_bs_write_long_int(bs, location->base_offset, 8*ptr->base_offset_size);
		extent_count = gf_list_count(location->extent_entries);
		gf_bs_write_u16(bs, extent_count);
		for (j=0; j<extent_count; j++) {
			GF_ItemExtentEntry *extent = (GF_ItemExtentEntry *)gf_list_get(location->extent_entries, j);
			gf_bs_write_long_int(bs, extent->extent_offset, 8*ptr->offset_size);
			gf_bs_write_long_int(bs, extent->extent_length, 8*ptr->length_size);
		}
	}
	return GF_OK;
}

GF_Err iloc_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, item_count, extent_count;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	item_count = gf_list_count(ptr->location_entries);
	for (i = 0; i < item_count; i++) {
		GF_ItemLocationEntry *location = (GF_ItemLocationEntry *)gf_list_get(ptr->location_entries, i);
		extent_count = gf_list_count(location->extent_entries);
		ptr->size += 6 + ptr->base_offset_size + extent_count*(ptr->offset_size + ptr->length_size);
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *pitm_New()
{
	GF_PrimaryItemBox *tmp = (GF_PrimaryItemBox *) gf_malloc(sizeof(GF_PrimaryItemBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_PrimaryItemBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_PITM;
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
	GF_Err e;
	GF_PrimaryItemBox *ptr = (GF_PrimaryItemBox *)s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;	
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
	GF_Err e;
	GF_ItemLocationBox *ptr = (GF_ItemLocationBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 16;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipro_New()
{
	GF_ItemProtectionBox *tmp = (GF_ItemProtectionBox *) gf_malloc(sizeof(GF_ItemProtectionBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemProtectionBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_IPRO;
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
		gf_list_add(ptr->protection_information, a);
	else 
		gf_isom_box_del(a);
	return GF_OK;
}
GF_Err ipro_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	return gf_isom_read_box_list(s, bs, ipro_AddBox);
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
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
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
	GF_ItemInfoEntryBox *tmp = (GF_ItemInfoEntryBox *) gf_malloc(sizeof(GF_ItemInfoEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemInfoEntryBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_INFE;
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
	GF_Err e;
	char *buf;
	u32 buf_len, i, string_len, string_start;
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	
	ptr->item_ID = gf_bs_read_u16(bs);
	ptr->item_protection_index = gf_bs_read_u16(bs);
	ptr->size -= 4;
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
		}
		string_len++;
	}
	gf_free(buf);
	if (!ptr->item_name || !ptr->content_type) return GF_ISOM_INVALID_FILE;
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
	gf_bs_write_u16(bs, ptr->item_ID);
	gf_bs_write_u16(bs, ptr->item_protection_index);
	if (ptr->item_name) {
		len = strlen(ptr->item_name)+1;
		gf_bs_write_data(bs, ptr->item_name, len);
	}
	if (ptr->content_type) {
		len = strlen(ptr->content_type)+1;
		gf_bs_write_data(bs, ptr->content_type, len);
	}
	if (ptr->content_encoding) {
		len = strlen(ptr->content_encoding)+1;
		gf_bs_write_data(bs, ptr->content_encoding, len);
	}
	return GF_OK;
}

GF_Err infe_Size(GF_Box *s)
{
	GF_Err e;
	GF_ItemInfoEntryBox *ptr = (GF_ItemInfoEntryBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if (ptr->item_name) ptr->size += strlen(ptr->item_name)+1;
	if (ptr->content_type) ptr->size += strlen(ptr->content_type)+1;
	if (ptr->content_encoding) ptr->size += strlen(ptr->content_encoding)+1;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iinf_New()
{
	GF_ItemInfoBox *tmp = (GF_ItemInfoBox *) gf_malloc(sizeof(GF_ItemInfoBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemInfoBox));
	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_IINF;
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

GF_Err iinf_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_Box *a;
	u32 count;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	count = gf_bs_read_u16(bs);

	while (count) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;

		if (a->type == GF_ISOM_BOX_TYPE_INFE)
			gf_list_add(ptr->item_infos, a); 
		else 
			gf_isom_box_del(a);
		count --;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iinf_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 count, i;
	GF_Err e;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	count = gf_list_count(ptr->item_infos);
	gf_bs_write_u16(bs, count);
	if (count) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->item_infos, i);
			e = gf_isom_box_write(a, bs);
			if (e) return e;
		}
	}
	return GF_OK;
}

GF_Err iinf_Size(GF_Box *s)
{
	u32 i, count;
	GF_Err e;
	GF_ItemInfoBox *ptr = (GF_ItemInfoBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 2;
	if ((count = gf_list_count(ptr->item_infos))) {
		for (i = 0; i < count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(ptr->item_infos, i);
			e = gf_isom_box_size(a);
			if (e) return e;
			ptr->size += a->size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/

