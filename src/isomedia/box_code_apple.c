/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2021
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

void ilst_box_del(GF_Box *s)
{
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err ilst_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	while (ptr->size) {
		/*if no ilst type coded, break*/
		sub_type = gf_bs_peek_bits(bs, 32, 0);
		if (sub_type) {
			e = gf_isom_box_parse_ex(&a, bs, s->type, GF_FALSE, s->size);

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
			gf_list_add(ptr->child_boxes, a);
		} else {
			gf_bs_read_u32(bs);
			ISOM_DECREASE_SIZE(ptr, 4);
		}
	}
	return GF_OK;
}

GF_Box *ilst_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemListBox, GF_ISOM_BOX_TYPE_ILST);
	tmp->child_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ilst_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
//	GF_ItemListBox *ptr = (GF_ItemListBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return GF_OK;
}


GF_Err ilst_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void ilst_item_box_del(GF_Box *s)
{
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err ilst_item_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a = NULL;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

	/*iTunes way: there's a data atom containing the data*/
	sub_type = gf_bs_peek_bits(bs, 32, 4);
	if (sub_type == GF_ISOM_BOX_TYPE_DATA ) {
		e = gf_isom_box_parse(&a, bs);

		if (!e && a && (ptr->size < a->size)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isom] not enough bytes in box %s: %d left, reading %d (file %s, line %d)\n", gf_4cc_to_str(ptr->type), ptr->size, a->size, __FILE__, __LINE__ )); \
			e = GF_ISOM_INVALID_FILE;
		}
		if (!a) e = GF_ISOM_INVALID_FILE;

		if (e) {
			if (a) gf_isom_box_del(a);
			return e;
		}
		if (!a) return GF_NON_COMPLIANT_BITSTREAM;

		ISOM_DECREASE_SIZE(ptr, a->size);

		if (ptr->data) gf_isom_box_del_parent(&ptr->child_boxes, (GF_Box *) ptr->data);

		/* otherwise a->data will always overflow */
		if (a->size > 4 && (a->type != GF_ISOM_BOX_TYPE_VOID)) {
			ptr->data = (GF_DataBox *)a;
			if (!ptr->child_boxes) ptr->child_boxes = gf_list_new();
			gf_list_add(ptr->child_boxes, ptr->data);
		} else {
			ptr->data = NULL;
			gf_isom_box_del(a);
		}
	}
	/*QT way*/
	else {
		u64 pos = gf_bs_get_position(bs);
		u64 prev_size = s->size;
		/*try parsing as generic box list*/
		e = gf_isom_box_array_read(s, bs);
		if (e==GF_OK) return GF_OK;
		//reset content and retry - this deletes ptr->data !!
		gf_isom_box_array_del(s->child_boxes);
		s->child_boxes=NULL;
		gf_bs_seek(bs, pos);
		s->size = prev_size;

		ptr->data = (GF_DataBox *)gf_isom_box_new_parent(&ptr->child_boxes, GF_ISOM_BOX_TYPE_DATA);
		//nope, check qt-style
		ptr->data->qt_style = GF_TRUE;
		ISOM_DECREASE_SIZE(ptr, 2);
		ptr->data->dataSize = gf_bs_read_u16(bs);
		gf_bs_read_u16(bs);

		ptr->data->data = (char *) gf_malloc(sizeof(char)*(ptr->data->dataSize + 1));
		if (!ptr->data->data) return GF_OUT_OF_MEM;

		gf_bs_read_data(bs, ptr->data->data, ptr->data->dataSize);
		ptr->data->data[ptr->data->dataSize] = 0;
		ISOM_DECREASE_SIZE(ptr, ptr->data->dataSize);
	}
	return GF_OK;
}

GF_Box *ilst_item_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ListItemBox, GF_ISOM_ITUNE_NAME); //type will be overwrite
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ilst_item_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	/*generic box list*/
	if (ptr->child_boxes && !ptr->data) {
	}
	//empty ilst
	else if (!ptr->data) {
	}
	/*iTune way: data-box-encapsulated box list*/
	else if (!ptr->data->qt_style) {
	}
	/*QT way: raw data*/
	else {
		gf_bs_write_u16(bs, ptr->data->dataSize);
		gf_bs_write_u16(bs, 0);
		gf_bs_write_data(bs, ptr->data->data, ptr->data->dataSize);
		ptr->size = 0; //abort
	}
	return GF_OK;
}

GF_Err ilst_item_box_size(GF_Box *s)
{
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

	/*generic box list*/
	if (ptr->child_boxes && !ptr->data) {
	}
	/*iTune way: data-box-encapsulated box list*/
	else if (ptr->data && !ptr->data->qt_style) {
		u32 pos=0;
		gf_isom_check_position(s, (GF_Box* ) ptr->data, &pos);
	}
	/*QT way: raw data*/
	else if (ptr->data) {
		ptr->size += ptr->data->dataSize + 4;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void databox_box_del(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *) s;
	if (ptr == NULL) return;
	if (ptr->data)
		gf_free(ptr->data);
	gf_free(ptr);

}

GF_Err databox_box_read(GF_Box *s,GF_BitStream *bs)
{
	GF_DataBox *ptr = (GF_DataBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	ptr->reserved = gf_bs_read_u32(bs);

	if (ptr->size) {
		ptr->dataSize = (u32) ptr->size;
		ptr->data = (char*)gf_malloc(ptr->dataSize * sizeof(ptr->data[0]) + 1);
		if (!ptr->data) return GF_OUT_OF_MEM;
		ptr->data[ptr->dataSize] = 0;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}

	return GF_OK;
}

GF_Box *databox_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataBox, GF_ISOM_BOX_TYPE_DATA);

	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err databox_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err databox_box_size(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *)s;

	ptr->size += 4;
	if(ptr->data != NULL && ptr->dataSize > 0) {
		ptr->size += ptr->dataSize;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void alis_box_del(GF_Box *s)
{
	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;
	if (ptr == NULL) return;
	gf_free(ptr);
}

GF_Err alis_box_read(GF_Box *s, GF_BitStream *bs)
{
//	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;
	return GF_OK;
}

GF_Box *alis_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_DataEntryAliasBox, GF_QT_BOX_TYPE_ALIS);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err alis_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
//	GF_DataEntryAliasBox *ptr = (GF_DataEntryAliasBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err alis_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void wide_box_del(GF_Box *s)
{
	if (s == NULL) return;
	gf_free(s);
}


GF_Err wide_box_read(GF_Box *s, GF_BitStream *bs)
{
	gf_bs_skip_bytes(bs, s->size);
	s->size = 0;
	return GF_OK;
}

GF_Box *wide_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_WideBox, GF_QT_BOX_TYPE_WIDE);
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err wide_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	return GF_OK;
}

GF_Err wide_box_size(GF_Box *s)
{
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *gf_isom_get_meta_extensions(GF_ISOFile *mov, u32 meta_type)
{
	u32 i;
	GF_UserDataMap *map;

	if (!mov || !mov->moov) return NULL;

	if (!mov->moov->udta) return NULL;
	map = udta_getEntry(mov->moov->udta, (meta_type==1) ? GF_ISOM_BOX_TYPE_XTRA : GF_ISOM_BOX_TYPE_META, NULL);
	if (!map) return NULL;

	for(i = 0; i < gf_list_count(map->boxes); i++) {
		GF_MetaBox *meta = (GF_MetaBox*)gf_list_get(map->boxes, i);
		if ((meta_type==1) && (meta->type==GF_ISOM_BOX_TYPE_XTRA)) return (GF_Box *) meta;

		if (meta && meta->handler) {
			if ( (meta_type==0) && (meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDIR))
				return (GF_Box *) meta;
			if ( (meta_type==2) && (meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDTA))
				return (GF_Box *) meta;
		}
	}
	return NULL;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Box *gf_isom_create_meta_extensions(GF_ISOFile *mov, u32 meta_type)
{
	GF_Err e;
	u32 i;
	GF_MetaBox *meta;
	GF_UserDataMap *map;
	u32 udta_subtype = (meta_type==1) ? GF_ISOM_BOX_TYPE_XTRA : GF_ISOM_BOX_TYPE_META;

	if (!mov || !mov->moov) return NULL;

	if (!mov->moov->udta) {
		e = moov_on_child_box((GF_Box*)mov->moov, gf_isom_box_new_parent(&mov->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		if (e) return NULL;
	}

	map = udta_getEntry(mov->moov->udta, udta_subtype, NULL);
	if (map) {
		for (i=0; i<gf_list_count(map->boxes); i++) {
			meta = (GF_MetaBox*)gf_list_get(map->boxes, i);
			if (meta_type==1) return (GF_Box *) meta;

			if (meta && meta->handler) {
				if ((meta_type==0) && (meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDIR)) return (GF_Box *) meta;
				if ((meta_type==2) && (meta->handler->handlerType == GF_ISOM_HANDLER_TYPE_MDTA)) return (GF_Box *) meta;
			}
		}
	}

	//udta handles children boxes through maps
	meta = (GF_MetaBox *)gf_isom_box_new(udta_subtype);

	if (meta) {
		udta_on_child_box((GF_Box *)mov->moov->udta, (GF_Box *)meta, GF_FALSE);
		if (meta_type!=1) {
			meta->handler = (GF_HandlerBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_HDLR);
			if(meta->handler == NULL) {
				gf_isom_box_del((GF_Box *)meta);
				return NULL;
			}
			meta->handler->handlerType = (meta_type==2) ? GF_ISOM_HANDLER_TYPE_MDTA : GF_ISOM_HANDLER_TYPE_MDIR;
			gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_ILST);
		}
	}
	return (GF_Box *) meta;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gmin_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err gmin_box_read(GF_Box *s, GF_BitStream *bs)
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

GF_Box *gmin_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GenericMediaHeaderInfoBox, GF_QT_BOX_TYPE_GMIN);
	return (GF_Box *)tmp;
}



#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gmin_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err gmin_box_size(GF_Box *s)
{
	GF_VideoMediaHeaderBox *ptr = (GF_VideoMediaHeaderBox *)s;
	ptr->size += 12;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




void clef_box_del(GF_Box *s)
{
	gf_free(s);
}

GF_Err clef_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ApertureBox *ptr = (GF_ApertureBox *)s;
	ISOM_DECREASE_SIZE(ptr, 8);
	ptr->width = gf_bs_read_u32(bs);
	ptr->height = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *clef_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ApertureBox, GF_QT_BOX_TYPE_CLEF);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clef_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ApertureBox *ptr = (GF_ApertureBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->width);
	gf_bs_write_u32(bs, ptr->height);
	return GF_OK;
}

GF_Err clef_box_size(GF_Box *s)
{
	s->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tmcd_box_del(GF_Box *s)
{
	if (s == NULL) return;
	gf_isom_sample_entry_predestroy((GF_SampleEntryBox *)s);
	gf_free(s);
}


GF_Err tmcd_box_read(GF_Box *s, GF_BitStream *bs)
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

	return gf_isom_box_array_read(s, bs);
}

GF_Box *tmcd_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeCodeSampleEntryBox, GF_QT_BOX_TYPE_TMCD);
	gf_isom_sample_entry_init((GF_SampleEntryBox*)tmp);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tmcd_box_write(GF_Box *s, GF_BitStream *bs)
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

	return GF_OK;
}

GF_Err tmcd_box_size(GF_Box *s)
{
	GF_SampleEntryBox *ptr = (GF_SampleEntryBox *)s;
	ptr->size += 8 + 18;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void tcmi_box_del(GF_Box *s)
{
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;
	if (ptr->font) gf_free(ptr->font);
	gf_free(s);
}


GF_Err tcmi_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 len;
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;

	//don't remove font name len field, some writers just skip it if no font ...
	ISOM_DECREASE_SIZE(s, 20);

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
	if (!ptr->size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] broken tmci box, missing font name length field\n" ));
		return GF_OK;
	}
	ISOM_DECREASE_SIZE(s, 1);

	len = gf_bs_read_u8(bs);
	if (len > ptr->size)
		len = (u32) ptr->size;
	if (len) {
		ptr->font = gf_malloc(len+1);
		if (!ptr->font) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ptr->font, len);
		ptr->size -= len;
		ptr->font[len]=0;
	}
	return GF_OK;
}

GF_Box *tcmi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TimeCodeMediaInformationBox, GF_QT_BOX_TYPE_TCMI);
	tmp->text_size = 12;
	tmp->text_color_red = 0xFFFF;
	tmp->text_color_green = 0xFFFF;
	tmp->text_color_blue = 0xFFFF;
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err tcmi_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err tcmi_box_size(GF_Box *s)
{
	GF_TimeCodeMediaInformationBox *ptr = (GF_TimeCodeMediaInformationBox *)s;
	ptr->size += 21;
	if (ptr->font)
    	ptr->size += strlen(ptr->font);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void fiel_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err fiel_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_FieldInfoBox *ptr = (GF_FieldInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 2);

    ptr->field_count = gf_bs_read_u8(bs);
    ptr->field_order = gf_bs_read_u8(bs);
	return GF_OK;
}

GF_Box *fiel_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_FieldInfoBox, GF_QT_BOX_TYPE_FIEL);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err fiel_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_FieldInfoBox *ptr = (GF_FieldInfoBox *)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, ptr->field_count);
	gf_bs_write_u8(bs, ptr->field_order);
	return GF_OK;
}

GF_Err fiel_box_size(GF_Box *s)
{
	s->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void gama_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err gama_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_GamaInfoBox *ptr = (GF_GamaInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 4);

    ptr->gama = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *gama_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GamaInfoBox, GF_QT_BOX_TYPE_GAMA);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gama_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_GamaInfoBox *ptr = (GF_GamaInfoBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->gama);
	return GF_OK;
}

GF_Err gama_box_size(GF_Box *s)
{
	s->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void chrm_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err chrm_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ChromaInfoBox *ptr = (GF_ChromaInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 2);

    ptr->chroma = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Box *chrm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChromaInfoBox, GF_QT_BOX_TYPE_CHRM);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err chrm_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_ChromaInfoBox *ptr = (GF_ChromaInfoBox *)s;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, ptr->chroma);
	return GF_OK;
}

GF_Err chrm_box_size(GF_Box *s)
{
	s->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void chan_box_del(GF_Box *s)
{
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;
	if (ptr->audio_descs) gf_free(ptr->audio_descs);
	if (ptr->ext_data) gf_free(ptr->ext_data);
	gf_free(s);
}


GF_Err chan_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;

	ISOM_DECREASE_SIZE(s, 12);
	ptr->layout_tag = gf_bs_read_u32(bs);
	ptr->bitmap = gf_bs_read_u32(bs);
	ptr->num_audio_description = gf_bs_read_u32(bs);

	if (ptr->size / 20 < ptr->num_audio_description)
		return GF_ISOM_INVALID_FILE;

	ptr->audio_descs = gf_malloc(sizeof(GF_AudioChannelDescription) * ptr->num_audio_description);
	if (!ptr->audio_descs) return GF_OUT_OF_MEM;
	
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
		ptr->ext_data = gf_malloc(sizeof(u8) * ptr->ext_data_size);
		if (!ptr->ext_data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, (char *)ptr->ext_data, (u32) ptr->size);
		ptr->size = 0;
	}
	return GF_OK;
}

GF_Box *chan_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ChannelLayoutInfoBox, GF_QT_BOX_TYPE_CHAN);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err chan_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err chan_box_size(GF_Box *s)
{
	GF_ChannelLayoutInfoBox *ptr = (GF_ChannelLayoutInfoBox *)s;
	s->size += 12 + 20 * ptr->num_audio_description;
	if (ptr->ext_data) {
		s->size += ptr->ext_data_size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void load_box_del(GF_Box *s)
{
	gf_free(s);
}


GF_Err load_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TrackLoadBox *ptr = (GF_TrackLoadBox *)s;
	ISOM_DECREASE_SIZE(s, 16);
	ptr->preload_start_time = gf_bs_read_u32(bs);
	ptr->preload_duration = gf_bs_read_u32(bs);
	ptr->preload_flags = gf_bs_read_u32(bs);
	ptr->default_hints = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Box *load_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TrackLoadBox, GF_QT_BOX_TYPE_LOAD);
	return (GF_Box *)tmp;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err load_box_write(GF_Box *s, GF_BitStream *bs)
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

GF_Err load_box_size(GF_Box *s)
{
	s->size += 16;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


void keys_box_del(GF_Box *s)
{
	GF_MetaKeysBox *ptr = (GF_MetaKeysBox *)s;
	if (ptr == NULL) return;
	while (gf_list_count(ptr->keys)) {
		GF_MetaKey *k = gf_list_pop_back(ptr->keys);
		if (k->data) gf_free(k->data);
		gf_free(k);
	}
	gf_list_del(ptr->keys);
	gf_free(ptr);
}

GF_Err keys_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, nb_keys;
	GF_MetaKeysBox *ptr = (GF_MetaKeysBox *)s;

	ISOM_DECREASE_SIZE(ptr, 4);
	nb_keys = gf_bs_read_u32(bs);
	for (i=0; i<nb_keys; i++) {
		GF_MetaKey *k;
		ISOM_DECREASE_SIZE(ptr, 8);
		u32 ksize = gf_bs_read_u32(bs);
		if (ksize<8) return GF_ISOM_INVALID_FILE;
		u32 ns = gf_bs_read_u32(bs);
		ISOM_DECREASE_SIZE(ptr, ksize-8);
		GF_SAFEALLOC(k, GF_MetaKey);
		if (!k) return GF_OUT_OF_MEM;
		gf_list_add(ptr->keys, k);
		k->ns = ns;
		k->size = ksize-8;
		k->data = gf_malloc(k->size+1);
		if (!k->data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, k->data, k->size);
		k->data[k->size]=0;
	}
	return GF_OK;
}

GF_Box *keys_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MetaKeysBox, GF_ISOM_BOX_TYPE_KEYS);
	tmp->keys = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err keys_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i, nb_keys;
	GF_MetaKeysBox *ptr = (GF_MetaKeysBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	nb_keys = gf_list_count(ptr->keys);
	gf_bs_write_u32(bs, nb_keys);
	for (i=0; i<nb_keys; i++) {
		GF_MetaKey *k = gf_list_get(ptr->keys, i);
		gf_bs_write_u32(bs, k->size+8);
		gf_bs_write_u32(bs, k->ns);
		if (k->data)
			gf_bs_write_data(bs, k->data, k->size);
	}
	return GF_OK;
}


GF_Err keys_box_size(GF_Box *s)
{
	u32 i, nb_keys;
	GF_MetaKeysBox *ptr = (GF_MetaKeysBox *)s;
	ptr->size += 4;
	nb_keys = gf_list_count(ptr->keys);
	for (i=0; i<nb_keys; i++) {
		GF_MetaKey *k = gf_list_get(ptr->keys, i);
		if (!k->data) k->size = 0;
		ptr->size += 8 + k->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
