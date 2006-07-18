/*
Author: Andrew Voznytsa <av@polynet.lviv.ua>

Project: GPAC - Multimedia Framework C SDK
Module: ISO Media File Format sub-project

Copyright: (c) 2006, Andrew Voznytsa
License: see License.txt in the top level directory.
*/

#include <gpac/internal/isomedia_dev.h>

void ilst_del(GF_Box *s)
{
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	if (ptr == NULL) return;
	if(ptr->iTunesSpecificInfo != NULL) gf_isom_box_del((GF_Box *)ptr->iTunesSpecificInfo);
	if(ptr->coverArt != NULL) gf_isom_box_del((GF_Box *)ptr->coverArt);
	if(ptr->compilation != NULL) gf_isom_box_del((GF_Box *)ptr->compilation);
	if(ptr->tempo != NULL) gf_isom_box_del((GF_Box *)ptr->tempo);
	if(ptr->trackNumber != NULL) gf_isom_box_del((GF_Box *)ptr->trackNumber);
	if(ptr->disk != NULL) gf_isom_box_del((GF_Box *)ptr->disk);
	if(ptr->genre != NULL) gf_isom_box_del((GF_Box *)ptr->genre);
	if(ptr->encoder != NULL) gf_isom_box_del((GF_Box *)ptr->encoder);
	if(ptr->writer != NULL) gf_isom_box_del((GF_Box *)ptr->writer);
	if(ptr->composer != NULL) gf_isom_box_del((GF_Box *)ptr->composer);
	if(ptr->album != NULL) gf_isom_box_del((GF_Box *)ptr->album);
	if(ptr->track != NULL) gf_isom_box_del((GF_Box *)ptr->track);
	if(ptr->artist != NULL) gf_isom_box_del((GF_Box *)ptr->artist);
	if(ptr->created != NULL) gf_isom_box_del((GF_Box *)ptr->created);
	if(ptr->comment != NULL) gf_isom_box_del((GF_Box *)ptr->comment);
	if(ptr->name != NULL) gf_isom_box_del((GF_Box *)ptr->name);
	free(ptr);
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

			switch(a->type){
			case GF_ISOM_BOX_TYPE_0xA9NAM:
				assert(ptr->name == NULL);
				if(ptr->name != NULL) gf_isom_box_del((GF_Box *)ptr->name);
				ptr->name = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9CMT:
				assert(ptr->comment == NULL);
				if(ptr->comment != NULL) gf_isom_box_del((GF_Box *)ptr->comment);
				ptr->comment = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9DAY:
				assert(ptr->created == NULL);
				if(ptr->created != NULL) gf_isom_box_del((GF_Box *)ptr->created);
				ptr->created = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9ART:
				assert(ptr->artist == NULL);
				if(ptr->artist != NULL) gf_isom_box_del((GF_Box *)ptr->artist);
				ptr->artist = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9TRK:
			case GF_ISOM_BOX_TYPE_TRKN:
				assert(ptr->track == NULL);
				if(ptr->track != NULL) gf_isom_box_del((GF_Box *)ptr->track);
				ptr->track = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9ALB:
				assert(ptr->album == NULL);
				if(ptr->album != NULL) gf_isom_box_del((GF_Box *)ptr->album);
				ptr->album = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9COM:
				assert(ptr->composer == NULL);
				if(ptr->composer != NULL) gf_isom_box_del((GF_Box *)ptr->composer);
				ptr->composer = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9WRT:
				assert(ptr->writer == NULL);
				if(ptr->writer != NULL) gf_isom_box_del((GF_Box *)ptr->writer);
				ptr->writer = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_0xA9TOO:
				assert(ptr->encoder == NULL);
				if(ptr->encoder != NULL) gf_isom_box_del((GF_Box *)ptr->encoder);
				ptr->encoder = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_GNRE:
				assert(ptr->genre == NULL);
				if(ptr->genre != NULL) gf_isom_box_del((GF_Box *)ptr->genre);
				ptr->genre = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_DISK:
				assert(ptr->disk == NULL);
				if(ptr->disk != NULL) gf_isom_box_del((GF_Box *)ptr->disk);
				ptr->disk = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_TMPO:
				assert(ptr->tempo == NULL);
				if(ptr->tempo != NULL) gf_isom_box_del((GF_Box *)ptr->tempo);
				ptr->tempo = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_CPIL:
				assert(ptr->compilation == NULL);
				if(ptr->compilation != NULL) gf_isom_box_del((GF_Box *)ptr->compilation);
				ptr->compilation = (GF_ListItemBox *)a;
				break;
			case GF_ISOM_BOX_TYPE_COVR:
				assert(ptr->coverArt == NULL);
				if(ptr->coverArt != NULL) gf_isom_box_del((GF_Box *)ptr->coverArt);
				ptr->coverArt = (GF_ListItemBox *)a;
				break;
			default:
				gf_isom_box_del(a);
				a = NULL;
			}
		} else {
			gf_bs_read_u32(bs);
			ptr->size -= 4;
		}
	}
	return GF_OK;
}

GF_Box *ilst_New()
{
	GF_ItemListBox *tmp = (GF_ItemListBox *) malloc(sizeof(GF_ItemListBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ItemListBox));
	tmp->type = GF_ISOM_BOX_TYPE_ILST;
	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

GF_Err ilst_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if(ptr->name != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->name, bs);
		if(e) return e;
	}
	if(ptr->comment != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->comment, bs);
		if(e) return e;
	}
	if(ptr->created != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->created, bs);
		if(e) return e;
	}
	if(ptr->artist != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->artist, bs);
		if(e) return e;
	}
	if(ptr->album != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->album, bs);
		if(e) return e;
	}
	if(ptr->track != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->track, bs);
		if(e) return e;
	}
	if(ptr->composer != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->composer, bs);
		if(e) return e;
	}
	if(ptr->writer != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->writer, bs);
		if(e) return e;
	}
	if(ptr->encoder != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->encoder, bs);
		if(e) return e;
	}
	if(ptr->genre != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->genre, bs);
		if(e) return e;
	}
	if(ptr->disk != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->disk, bs);
		if(e) return e;
	}
	if(ptr->trackNumber != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->trackNumber, bs);
		if(e) return e;
	}
	if(ptr->compilation != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->compilation, bs);
		if(e) return e;
	}
	if(ptr->tempo != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->tempo, bs);
		if(e) return e;
	}
	if(ptr->coverArt != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->coverArt, bs);
		if(e) return e;
	}
	if(ptr->iTunesSpecificInfo != NULL){
		e = gf_isom_box_write((GF_Box *)ptr->iTunesSpecificInfo, bs);
		if(e) return e;
	}
	return GF_OK;
}

GF_Err ilst_Size(GF_Box *s)
{
	GF_Err e;
	GF_ItemListBox *ptr = (GF_ItemListBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if(ptr->name != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->name);
		if(e) return e;
		ptr->size += ptr->name->size;
	}
	if(ptr->comment != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->comment);
		if(e) return e;
		ptr->size += ptr->comment->size;
	}
	if(ptr->created != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->created);
		if(e) return e;
		ptr->size += ptr->created->size;
	}
	if(ptr->artist != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->artist);
		if(e) return e;
		ptr->size += ptr->artist->size;
	}
	if(ptr->album != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->album);
		if(e) return e;
		ptr->size += ptr->album->size;
	}
	if(ptr->track != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->track);
		if(e) return e;
		ptr->size += ptr->track->size;
	}
	if(ptr->composer != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->composer);
		if(e) return e;
		ptr->size += ptr->composer->size;
	}
	if(ptr->writer != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->writer);
		if(e) return e;
		ptr->size += ptr->writer->size;
	}
	if(ptr->encoder != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->encoder);
		if(e) return e;
		ptr->size += ptr->encoder->size;
	}
	if(ptr->genre != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->genre);
		if(e) return e;
		ptr->size += ptr->genre->size;
	}
	if(ptr->disk != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->disk);
		if(e) return e;
		ptr->size += ptr->disk->size;
	}
	if(ptr->trackNumber != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->trackNumber);
		if(e) return e;
		ptr->size += ptr->trackNumber->size;
	}
	if(ptr->compilation != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->compilation);
		if(e) return e;
		ptr->size += ptr->compilation->size;
	}
	if(ptr->tempo != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->tempo);
		if(e) return e;
		ptr->size += ptr->tempo->size;
	}
	if(ptr->coverArt != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->coverArt);
		if(e) return e;
		ptr->size += ptr->coverArt->size;
	}
	if(ptr->iTunesSpecificInfo != NULL){
		e = gf_isom_box_size((GF_Box *)ptr->iTunesSpecificInfo);
		if(e) return e;
		ptr->size += ptr->iTunesSpecificInfo->size;
	}
	return GF_OK;
}

#endif //GPAC_READ_ONLY

void ListItem_del(GF_Box *s)
{
	GF_ListItemBox *ptr = (GF_ListItemBox *) s;
	if (ptr == NULL) return;
	if (ptr->data != NULL) {
		if (ptr->data->data) free(ptr->data->data);
		free(ptr->data);
	}
	free(ptr);
}

GF_Err ListItem_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 sub_type;
	GF_Box *a = NULL;
	GF_ListItemBox *ptr = (GF_ListItemBox *)s;

	sub_type = gf_bs_peek_bits(bs, 32, 0);
	/*iTunes way*/
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
		ptr->data->data = malloc(sizeof(char)*(ptr->data->dataSize + 1));
		gf_bs_read_data(bs, ptr->data->data, ptr->data->dataSize);
		ptr->data->data[ptr->data->dataSize] = 0;
		ptr->size -= ptr->data->dataSize;
	}
	return GF_OK;
}

GF_Box *ListItem_New(u32 type)
{
	GF_ListItemBox *tmp;
	
	tmp = (GF_ListItemBox *) malloc(sizeof(GF_ListItemBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_ListItemBox));

	tmp->type = type;

	tmp->data = (GF_DataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_DATA);

	if (tmp->data == NULL){
		free(tmp);
		return NULL;
	}

	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

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

#endif //GPAC_READ_ONLY

void data_del(GF_Box *s)
{
	GF_DataBox *ptr = (GF_DataBox *) s;
	if (ptr == NULL) return;
	if (ptr->data)
		free(ptr->data);
	free(ptr);

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
		ptr->data = (u8*)malloc(ptr->dataSize * sizeof(ptr->data[0]) + 1);
		if (ptr->data == NULL) return GF_OUT_OF_MEM;
		ptr->data[ptr->dataSize] = 0;
		gf_bs_read_data(bs, ptr->data, ptr->dataSize);
	}
	
	return GF_OK;
}

GF_Box *data_New()
{
	GF_DataBox *tmp;
	
	tmp = (GF_DataBox *) malloc(sizeof(GF_DataBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_DataBox));

	gf_isom_full_box_init((GF_Box *)tmp);
	tmp->type = GF_ISOM_BOX_TYPE_DATA;

	return (GF_Box *)tmp;
}

//from here, for write/edit versions
#ifndef GPAC_READ_ONLY

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

#endif //GPAC_READ_ONLY

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

#ifndef GPAC_READ_ONLY
GF_MetaBox *gf_isom_apple_create_meta_extensions(GF_ISOFile *mov)
{
	GF_Err e;
	u32 i;
	GF_MetaBox *meta;
	GF_UserDataMap *map;
	GF_Err moov_AddBox(GF_MovieBox *ptr, GF_Box *a);
	GF_Err udta_AddBox (GF_UserDataBox *ptr, GF_Box *a);

	if (!mov || !mov->moov) return NULL;

	if (!mov->moov->udta){
		e = moov_AddBox(mov->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
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
#endif