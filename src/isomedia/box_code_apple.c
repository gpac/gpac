/*
Author: Andrew Voznytsa <av@polynet.lviv.ua>

Project: GPAC - Multimedia Framework C SDK
Module: ISO Media File Format sub-project

Copyright: (c) 2006, Andrew Voznytsa
License: see License.txt in the top level directory.
*/

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM

void ilst_del(GF_Box *s)
{
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	if (ptr == NULL) return;
	gf_isom_box_array_del(ptr->tags);
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
			gf_list_add(ptr->tags, a);
		} else {
			gf_bs_read_u32(bs);
			ptr->size -= 4;
		}
	}
	return GF_OK;
}

GF_Box *ilst_New()
{
	GF_ItemListBox *tmp = (GF_ItemListBox *) gf_malloc(sizeof(GF_ItemListBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemListBox));
	tmp->type = GF_ISOM_BOX_TYPE_ILST;
	tmp->tags = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ilst_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return gf_isom_box_array_write(s, ptr->tags, bs);
}


GF_Err ilst_Size(GF_Box *s)
{
	GF_Err e;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;

	return gf_isom_box_array_size(s, ptr->tags);
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
	GF_ListItemBox *tmp;
	
	tmp = (GF_ListItemBox *) gf_malloc(sizeof(GF_ListItemBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ListItemBox));

	tmp->type = type;

	tmp->data = (GF_DataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DATA);

	if (tmp->data == NULL){
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
	else {
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
	GF_DataBox *tmp;
	
	tmp = (GF_DataBox *) gf_malloc(sizeof(GF_DataBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_DATA;

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
	if(ptr->data != NULL && ptr->dataSize > 0){
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
	if(ptr->data != NULL && ptr->dataSize > 0){
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

	for(i = 0; i < gf_list_count(map->boxList); i++){
		meta = (GF_MetaBox*)gf_list_get(map->boxList, i);

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

	if (!mov->moov->udta){
		e = moov_AddBox((GF_Box*)mov->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return NULL;
	}

	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_META, NULL);
	if (map){
		for(i = 0; i < gf_list_count(map->boxList); i++){
			meta = (GF_MetaBox*)gf_list_get(map->boxList, i);

			if(meta != NULL && meta->handler != NULL && meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDIR) return meta;
		}
	}

	meta = (GF_MetaBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_META);

	if(meta != NULL){
		meta->handler = (GF_HandlerBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_HDLR);
		if(meta->handler == NULL){
			gf_isom_box_del((GF_Box *)meta);
			return NULL;
		}
		meta->handler->handlerType = GF_ISOM_HANDLER_TYPE_MDIR;
		gf_list_add(meta->other_boxes, gf_isom_box_new(GF_ISOM_BOX_TYPE_ILST));
		udta_AddBox(mov->moov->udta, (GF_Box *)meta);
	}

	return meta;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
