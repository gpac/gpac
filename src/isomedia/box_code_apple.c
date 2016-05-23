/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2012
 *				All rights reserved
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

void ilst_del(GF_Box *s)
{
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err ilst_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	while (ptr->size) {
		/*if no ilst type coded, break*/
		sub_type = gf_bs_peek_bits(bs, 32, 0);
		if (sub_type) {
			e = gf_isom_parse_box(&a, bs);
			if (e) return e;
			if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
			ptr->size -= a->size;
			gf_list_add(ptr->other_boxes, a);
		} else {
			gf_bs_read_u32(bs);
			ptr->size -= 4;
		}
	}
	return GF_OK;
}

GF_Box *ilst_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemListBox, GF_ISOM_BOX_TYPE_ILST);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ilst_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
//	GF_ItemListBox *ptr = (GF_ItemListBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return GF_OK;
}


GF_Err ilst_Size(GF_Box *s)
{
	GF_Err e;
//	GF_ItemListBox *ptr = (GF_ItemListBox *)s;

	e = gf_isom_box_get_size(s);
	if (e) return e;

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ListItem_del(GF_Box *s)
{
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;
	if (ptr == NULL) return;
	if (ptr->data != NULL) {
		if (ptr->data->data) gf_free(ptr->data->data);
		gf_free(ptr->data);
	}
	gf_free(ptr);
}

GF_Err ListItem_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a = NULL;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

	/*iTunes way: there's a data atom containing the data*/
	sub_type = gf_bs_peek_bits(bs, 32, 4);
	if (sub_type == GF_ISOM_BOX_TYPE_DATA ) {
		e = gf_isom_parse_box(&a, bs);
		if (e) return e;
		if (ptr->size<a->size) return GF_ISOM_INVALID_FILE;
		ptr->size -= a->size;

		if (a && ptr->data) gf_isom_box_del((GF_Box *) ptr->data);
		ptr->data = (GF_DataBox *)a;
	}
	/*QT way*/
	else {
		ptr->data->type = 0;
		ptr->data->dataSize = gf_bs_read_u16(bs);
		gf_bs_read_u16(bs);
		ptr->data->data = (char *) gf_malloc(sizeof(char)*(ptr->data->dataSize + 1));
		gf_bs_read_data(bs, ptr->data->data, ptr->data->dataSize);
		ptr->data->data[ptr->data->dataSize] = 0;
		ptr->size -= ptr->data->dataSize;
	}
	return GF_OK;
}

GF_Box *ListItem_New(u32 type)
{
	ISOM_DECL_BOX_ALLOC(GF_ListItemBox, type);

	tmp->data = (GF_DataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DATA);

	if (tmp->data == NULL) {
		gf_free(tmp);
		return NULL;
	}

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ListItem_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	/*iTune way*/
	if (ptr->data->type) return gf_isom_box_write((GF_Box* )ptr->data, bs);
	/*QT way*/
	gf_bs_write_u16(bs, ptr->data->dataSize);
	gf_bs_write_u16(bs, 0);
	gf_bs_write_data(bs, ptr->data->data, ptr->data->dataSize);
	return GF_OK;
}

GF_Err ListItem_Size(GF_Box *s)
{
	GF_Err e;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

	e = gf_isom_box_get_size(s);
	if (e) return e;

	/*iTune way*/
	if (ptr->data && ptr->data->type) {
		e = gf_isom_box_size((GF_Box *)ptr->data);
		if (e) return e;
		ptr->size += ptr->data->size;
	}
	/*QT way*/
	else if (ptr->data) {
		ptr->size += ptr->data->dataSize + 4;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void data_del(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *) s;
	if (ptr == NULL) return;
	if (ptr->data)
		gf_free(ptr->data);
	gf_free(ptr);

}

GF_Err data_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	GF_DataBox *ptr = (GF_DataBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;
	ptr->reserved = gf_bs_read_int(bs, 32);
	ptr->size -= 4;
	if (ptr->size) {
		ptr->dataSize = (u32) ptr->size;
		ptr->data = (char*)gf_malloc(ptr->dataSize * sizeof(ptr->data[0]) + 1);
		if (ptr->data == NULL) return GF_OUT_OF_MEM;
		ptr->data[ptr->dataSize] = 0;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}

	return GF_OK;
}

GF_Box *data_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataBox, GF_ISOM_BOX_TYPE_DATA);

	gf_isom_full_box_init((GF_Box *)tmp);

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err data_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_DataBox *ptr = (GF_DataBox *) s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_int(bs, ptr->reserved, 32);
	if(ptr->data != NULL && ptr->dataSize > 0) {
		gf_bs_write_data(bs, ptr->data, ptr->dataSize);
	}
	return GF_OK;
}

GF_Err data_Size(GF_Box *s)
{
	GF_Err e;
	GF_DataBox *ptr = (GF_DataBox *)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	ptr->size += 4;
	if(ptr->data != NULL && ptr->dataSize > 0) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_MetaBox *gf_isom_apple_get_meta_extensions(GF_ISOFile *mov)
{
	u32 i;
	GF_MetaBox *meta;
	GF_UserDataMap *map;

	if (!mov || !mov->moov) return NULL;

	if (!mov->moov->udta) return NULL;
	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_META, NULL);
	if (!map) return NULL;

	for(i = 0; i < gf_list_count(map->other_boxes); i++) {
		meta = (GF_MetaBox*)gf_list_get(map->other_boxes, i);

		if(meta != NULL && meta->handler != NULL && meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDIR) return meta;
	}

	return NULL;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_MetaBox *gf_isom_apple_create_meta_extensions(GF_ISOFile *mov)
{
	GF_Err e;
	u32 i;
	GF_MetaBox *meta;
	GF_UserDataMap *map;

	if (!mov || !mov->moov) return NULL;

	if (!mov->moov->udta) {
		e = moov_AddBox((GF_Box*)mov->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return NULL;
	}

	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_META, NULL);
	if (map) {
		for(i = 0; i < gf_list_count(map->other_boxes); i++) {
			meta = (GF_MetaBox*)gf_list_get(map->other_boxes, i);

			if(meta != NULL && meta->handler != NULL && meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDIR) return meta;
		}
	}

	meta = (GF_MetaBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_META);

	if(meta != NULL) {
		meta->handler = (GF_HandlerBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_HDLR);
		if(meta->handler == NULL) {
			gf_isom_box_del((GF_Box *)meta);
			return NULL;
		}
		meta->handler->handlerType = GF_ISOM_HANDLER_TYPE_MDIR;
		if (!meta->other_boxes) meta->other_boxes = gf_list_new();
		gf_list_add(meta->other_boxes, gf_isom_box_new(GF_ISOM_BOX_TYPE_ILST));
		udta_AddBox(mov->moov->udta, (GF_Box *)meta);
	}

	return meta;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
