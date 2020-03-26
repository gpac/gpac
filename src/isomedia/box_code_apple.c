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
			e = gf_isom_box_parse_ex(&a, bs, s->type, GF_FALSE);

			/* the macro will return in this case before we can free */
			if (!e && ptr->size < a->size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isom] not enough bytes in box %s: %d left, reading %d (file %s, line %d)\n", gf_4cc_to_str(ptr->type), ptr->size, a->size, __FILE__, __LINE__ )); \
				e = GF_ISOM_INVALID_FILE;
			}
			if (e) {
				if (a) gf_isom_box_del(a);
				return e;
			}

			ISOM_DECREASE_SIZE(ptr, a->size);
			gf_list_add(ptr->other_boxes, a);
		} else {
			gf_bs_read_u32(bs);
			ISOM_DECREASE_SIZE(ptr, 4);
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
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ilst_item_del(GF_Box *s)
{
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;
	if (ptr == NULL) return;
	if (ptr->data != NULL) {
		if (ptr->data->data) gf_free(ptr->data->data);
		gf_free(ptr->data);
	}
	gf_free(ptr);
}

GF_Err ilst_item_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a = NULL;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;
	/*iTunes way: there's a data atom containing the data*/
	sub_type = gf_bs_peek_bits(bs, 32, 4);
	if (sub_type == GF_ISOM_BOX_TYPE_DATA ) {
		e = gf_isom_box_parse(&a, bs);

		if (!e && a && ptr->size < a->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isom] not enough bytes in box %s: %d left, reading %d (file %s, line %d)\n", gf_4cc_to_str(ptr->type), ptr->size, a->size, __FILE__, __LINE__ )); \
			e = GF_ISOM_INVALID_FILE;
		}
		if (!a) e = GF_ISOM_INVALID_FILE;

		if (e) {
			if (a) gf_isom_box_del(a);
			return e;
		}

		ISOM_DECREASE_SIZE(ptr, a->size);

		if (a && ptr->data) gf_isom_box_del((GF_Box *) ptr->data);

		/* otherwise a->data will always overflow */
		if (a && a->size > 4 && a->type != GF_ISOM_BOX_TYPE_VOID)
			ptr->data = (GF_DataBox *)a;
		else {
			ptr->data = NULL;
			gf_isom_box_del(a);
		}
	}
	/*QT way*/
	else {
		ptr->data->type = 0;
		ptr->data->dataSize = gf_bs_read_u16(bs);
		gf_bs_read_u16(bs);
		ptr->data->data = (char *) gf_malloc(sizeof(char)*(ptr->data->dataSize + 1));
		gf_bs_read_data(bs, ptr->data->data, ptr->data->dataSize);
		ptr->data->data[ptr->data->dataSize] = 0;
		ISOM_DECREASE_SIZE(ptr, ptr->data->dataSize);
	}
	return GF_OK;
}

GF_Box *ilst_item_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ListItemBox, GF_ISOM_BOX_TYPE_CPIL); //type will be overwrite

	tmp->data = (GF_DataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DATA);

	if (tmp->data == NULL) {
		gf_free(tmp);
		return NULL;
	}

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ilst_item_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (!ptr->data) return GF_BAD_PARAM;

	/*iTune way*/
	if (ptr->data->type) return gf_isom_box_write((GF_Box* )ptr->data, bs);
	/*QT way*/
	gf_bs_write_u16(bs, ptr->data->dataSize);
	gf_bs_write_u16(bs, 0);
	gf_bs_write_data(bs, ptr->data->data, ptr->data->dataSize);
	return GF_OK;
}

GF_Err ilst_item_Size(GF_Box *s)
{
	GF_Err e;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

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

void databox_del(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *) s;
	if (ptr == NULL) return;
	if (ptr->data)
		gf_free(ptr->data);
	gf_free(ptr);

}

GF_Err databox_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_DataBox *ptr = (GF_DataBox *)s;

	ptr->reserved = gf_bs_read_int(bs, 32);
	ISOM_DECREASE_SIZE(ptr, 4);

	if (ptr->size) {
		ptr->dataSize = (u32) ptr->size;
		ptr->data = (char*)gf_malloc(ptr->dataSize * sizeof(ptr->data[0]) + 1);
		if (ptr->data == NULL) return GF_OUT_OF_MEM;
		ptr->data[ptr->dataSize] = 0;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}

	return GF_OK;
}

GF_Box *databox_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataBox, GF_ISOM_BOX_TYPE_DATA);

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err databox_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err databox_Size(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *)s;

	ptr->size += 4;
	if(ptr->data != NULL && ptr->dataSize > 0) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void alis_del(GF_Box *s)
{
	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err alis_Read(GF_Box *s, GF_BitStream *bs)
{
//	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;
	return GF_OK;
}

GF_Box *alis_New()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryAliasBox, GF_QT_BOX_TYPE_ALIS);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err alis_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
//	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err alis_Size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void wide_del(GF_Box *s)
{
	if (s == NULL) return;
	gf_free(s);
}


GF_Err wide_Read(GF_Box *s, GF_BitStream *bs)
{
	gf_bs_skip_bytes(bs, s->size);
	s->size = 0;
	return GF_OK;
}

GF_Box *wide_New()
{
	ISOM_DECL_BOX_ALLOC(GF_WideBox, GF_QT_BOX_TYPE_WIDE);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err wide_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err wide_Size(GF_Box *s)
{
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
		udta_AddBox((GF_Box *)mov->moov->udta, (GF_Box *)meta);
	}

	return meta;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gmin_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err gmin_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_GenericMediaHeaderInfoBox *ptr = (GF_GenericMediaHeaderInfoBox *)s;
	ISOM_DECREASE_SIZE(ptr, 12);
	ptr->graphics_mode = gf_bs_read_u16(bs);
	ptr->op_color_red = gf_bs_read_u16(bs);
	ptr->op_color_green = gf_bs_read_u16(bs);
	ptr->op_color_blue = gf_bs_read_u16(bs);
	ptr->balance = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *gmin_New()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericMediaHeaderInfoBox, GF_QT_BOX_TYPE_GMIN);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gmin_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_GenericMediaHeaderInfoBox *ptr = (GF_GenericMediaHeaderInfoBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, ptr->graphics_mode);
	gf_bs_write_u16(bs, ptr->op_color_red);
	gf_bs_write_u16(bs, ptr->op_color_green);
	gf_bs_write_u16(bs, ptr->op_color_blue);
	gf_bs_write_u16(bs, ptr->balance);
	gf_bs_write_u16(bs, ptr->reserved);
	return GF_OK;
}

GF_Err gmin_Size(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	ptr->size += 12;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




void clef_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err clef_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ApertureBox *ptr = (GF_ApertureBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->width = gf_bs_read_u32(bs);
	ptr->height = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *clef_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ApertureBox, GF_QT_BOX_TYPE_CLEF);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clef_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ApertureBox *ptr = (GF_ApertureBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->width);
	gf_bs_write_u32(bs, ptr->height);
	return GF_OK;
}

GF_Err clef_Size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tmcd_del(GF_Box *s)
{
	if (s == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(s);
}


GF_Err tmcd_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TimeCodeSampleEntryBox *ptr = (GF_TimeCodeSampleEntryBox *)s;
	GF_Err e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)s, bs);
	if (e) return e;

	ISOM_DECREASE_SIZE(s, 26);
	gf_bs_read_u32(bs); //reserved
	ptr->flags = gf_bs_read_u32(bs);
	ptr->timescale = gf_bs_read_u32(bs);
	ptr->frame_duration = gf_bs_read_u32(bs);
	ptr->frames_per_counter_tick = gf_bs_read_u8(bs);
	gf_bs_read_u8(bs); //reserved

	return gf_isom_box_array_read(s, bs, gf_isom_box_add_default);
}

GF_Box *tmcd_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeCodeSampleEntryBox, GF_QT_BOX_TYPE_TMCD);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tmcd_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TimeCodeSampleEntryBox *ptr = (GF_TimeCodeSampleEntryBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);

	gf_bs_write_u32(bs, 0); //reserved
	gf_bs_write_u32(bs, ptr->flags);
	gf_bs_write_u32(bs, ptr->timescale);
	gf_bs_write_u32(bs, ptr->frame_duration);
	gf_bs_write_u8(bs, ptr->frames_per_counter_tick);
	gf_bs_write_u8(bs, 0); //reserved

	return gf_isom_box_array_write(s, ptr->protections, bs);
}

GF_Err tmcd_Size(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	ptr->size += 8 + 18;
	return gf_isom_box_array_size(s, ptr->protections);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tcmi_del(GF_Box *s)
{
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;
	if (ptr->font) gf_free(ptr->font);
	gf_free(s);
}


GF_Err tcmi_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;

	ISOM_DECREASE_SIZE(s, 21);

	ptr->text_font = gf_bs_read_u16(bs);
	ptr->text_face = gf_bs_read_u16(bs);
	ptr->text_size = gf_bs_read_u16(bs);
	gf_bs_read_u16(bs);
	ptr->text_color_red = gf_bs_read_u16(bs);
	ptr->text_color_green = gf_bs_read_u16(bs);
	ptr->text_color_blue = gf_bs_read_u16(bs);
	ptr->back_color_red = gf_bs_read_u16(bs);
	ptr->back_color_green = gf_bs_read_u16(bs);
	ptr->back_color_blue = gf_bs_read_u16(bs);
	len = gf_bs_read_u8(bs);
	if (len > ptr->size)
		len = (u32) ptr->size;
	if (len) {
		ptr->font = gf_malloc(len+1);
		gf_bs_read_data(bs, ptr->font, len);
		ptr->size -= len;
		ptr->font[len]=0;
	}
	return GF_OK;
}

GF_Box *tcmi_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeCodeMediaInformationBox, GF_QT_BOX_TYPE_TCMI);
	tmp->text_size = 12;
	tmp->text_color_red = 0xFFFF;
	tmp->text_color_green = 0xFFFF;
	tmp->text_color_blue = 0xFFFF;
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tcmi_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 len;
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->text_font);
	gf_bs_write_u16(bs, ptr->text_face);
	gf_bs_write_u16(bs, ptr->text_size);
	gf_bs_write_u16(bs, 0);
	gf_bs_write_u16(bs, ptr->text_color_red);
	gf_bs_write_u16(bs, ptr->text_color_green);
	gf_bs_write_u16(bs, ptr->text_color_blue);
	gf_bs_write_u16(bs, ptr->back_color_red);
	gf_bs_write_u16(bs, ptr->back_color_green);
	gf_bs_write_u16(bs, ptr->back_color_blue);
	len = ptr->font ? (u32) strlen(ptr->font) : 0;
	gf_bs_write_u8(bs, len);
	if (ptr->font)
		gf_bs_write_data(bs, ptr->font, len);

	return GF_OK;
}

GF_Err tcmi_Size(GF_Box *s)
{
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;
	ptr->size += 21;
	if (ptr->font)
    	ptr->size += strlen(ptr->font);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void fiel_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err fiel_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_FieldInfoBox *ptr = (GF_FieldInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 2);

    ptr->field_count = gf_bs_read_u8(bs);
    ptr->field_order = gf_bs_read_u8(bs);
	return GF_OK;
}

GF_Box *fiel_New()
{
	ISOM_DECL_BOX_ALLOC(GF_FieldInfoBox, GF_QT_BOX_TYPE_FIEL);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fiel_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FieldInfoBox *ptr = (GF_FieldInfoBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->field_count);
	gf_bs_write_u8(bs, ptr->field_order);
	return GF_OK;
}

GF_Err fiel_Size(GF_Box *s)
{
	s->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gama_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err gama_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_GamaInfoBox *ptr = (GF_GamaInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 4);

    ptr->gama = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *gama_New()
{
	ISOM_DECL_BOX_ALLOC(GF_GamaInfoBox, GF_QT_BOX_TYPE_GAMA);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gama_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_GamaInfoBox *ptr = (GF_GamaInfoBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->gama);
	return GF_OK;
}

GF_Err gama_Size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void chrm_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err chrm_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ChromaInfoBox *ptr = (GF_ChromaInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 2);

    ptr->chroma = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *chrm_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ChromaInfoBox, GF_QT_BOX_TYPE_CHRM);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err chrm_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_ChromaInfoBox *ptr = (GF_ChromaInfoBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->chroma);
	return GF_OK;
}

GF_Err chrm_Size(GF_Box *s)
{
	s->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void chan_del(GF_Box *s)
{
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;
	if (ptr->audio_descs) gf_free(ptr->audio_descs);
	if (ptr->ext_data) gf_free(ptr->ext_data);
	gf_free(s);
}


GF_Err chan_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 12);
	ptr->layout_tag = gf_bs_read_u32(bs);
	ptr->bitmap = gf_bs_read_u32(bs);
	ptr->num_audio_description = gf_bs_read_u32(bs);

	ptr->audio_descs = gf_malloc(sizeof(GF_AudioChannelDescription) * ptr->num_audio_description);
	if (!ptr->audio_descs)
	    return GF_OUT_OF_MEM;
	for (i=0; i<ptr->num_audio_description; i++) {
		GF_AudioChannelDescription *adesc = &ptr->audio_descs[i];
		ISOM_DECREASE_SIZE(s, 20);
		adesc->label = gf_bs_read_u32(bs);
		adesc->flags = gf_bs_read_u32(bs);
		adesc->coordinates[0] = gf_bs_read_float(bs);
		adesc->coordinates[1] = gf_bs_read_float(bs);
		adesc->coordinates[2] = gf_bs_read_float(bs);
	}
	//avoids warning on most files
	if (ptr->size==20) {
		ptr->size=0;
		gf_bs_skip_bytes(bs, 20);
	}
	if (ptr->size<10000) {
		ptr->ext_data_size = (u32) ptr->size;
		ptr->ext_data = gf_malloc((size_t)(sizeof(u8)*ptr->size));
		if (!ptr->ext_data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, (char *)ptr->ext_data, (u32) ptr->size);
		ptr->size = 0;
	}
	return GF_OK;
}

GF_Box *chan_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ChannelLayoutInfoBox, GF_QT_BOX_TYPE_CHAN);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err chan_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;


	gf_bs_write_u32(bs, ptr->layout_tag);
	gf_bs_write_u32(bs, ptr->bitmap);
	gf_bs_write_u32(bs, ptr->num_audio_description);
	for (i=0; i<ptr->num_audio_description; i++) {
		GF_AudioChannelDescription *adesc = &ptr->audio_descs[i];
		gf_bs_write_u32(bs, adesc->label);
		gf_bs_write_u32(bs, adesc->flags);
		gf_bs_write_float(bs, adesc->coordinates[0]);
		gf_bs_write_float(bs, adesc->coordinates[1]);
		gf_bs_write_float(bs, adesc->coordinates[2]);
	}
	if (ptr->ext_data) {
		gf_bs_write_data(bs, ptr->ext_data, ptr->ext_data_size);
	}
	return GF_OK;
}

GF_Err chan_Size(GF_Box *s)
{
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;
	s->size += 12 + 20 * ptr->num_audio_description;
	if (ptr->ext_data) {
		s->size += ptr->ext_data_size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void load_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err load_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackLoadBox *ptr = (GF_TrackLoadBox *)s;
	ISOM_DECREASE_SIZE(s, 16);
	ptr->preload_start_time = gf_bs_read_u32(bs);
	ptr->preload_duration = gf_bs_read_u32(bs);
	ptr->preload_flags = gf_bs_read_u32(bs);
	ptr->default_hints = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *load_New()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackLoadBox, GF_QT_BOX_TYPE_LOAD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err load_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackLoadBox *ptr = (GF_TrackLoadBox *)s;

	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u32(bs, ptr->preload_start_time);
	gf_bs_write_u32(bs, ptr->preload_duration);
	gf_bs_write_u32(bs, ptr->preload_flags);
	gf_bs_write_u32(bs, ptr->default_hints);
	return GF_OK;
}

GF_Err load_Size(GF_Box *s)
{
	s->size += 16;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
