/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
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


GF_MetaBox *gf_isom_get_meta(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_TrackBox *tk;
	if (!file) return NULL;
	if (root_meta) return file->meta;
	if (!track_num) return file->moov ? file->moov->meta : NULL;

	tk = (GF_TrackBox*)gf_list_get(file->moov->trackList, track_num-1);
	return tk ? tk->meta : NULL;
}

GF_EXPORT
u32 gf_isom_get_meta_type(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return 0;
	if (!meta->handler) return 0;
	return meta->handler->handlerType;
}

GF_EXPORT
u32 gf_isom_has_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return 0;

	count = gf_list_count(meta->other_boxes);
	for (i=0; i<count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->other_boxes, i);
		if (a->type == GF_ISOM_BOX_TYPE_XML) return 1;
		if (a->type == GF_ISOM_BOX_TYPE_BXML) return 2;
	}
	return 0;
}

GF_Err gf_isom_extract_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *outName, Bool *is_binary)
{
	u32 i, count;
	FILE *didfile;
	GF_XMLBox *xml = NULL;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	/*Find XMLBox*/
	count = gf_list_count(meta->other_boxes);
	for (i = 0; i <count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->other_boxes, i);
		if ((a->type == GF_ISOM_BOX_TYPE_XML) || (a->type == GF_ISOM_BOX_TYPE_BXML) ) {
			xml = (GF_XMLBox *)a;
			break;
		}
	}
	if (!xml || !xml->xml || !xml->xml_length) return GF_BAD_PARAM;
	
	didfile = gf_f64_open(outName, "wt");
	if (!didfile) return GF_IO_ERR;
	fwrite(xml->xml, xml->xml_length, 1, didfile);
	fclose(didfile);

	if (is_binary) *is_binary = (xml->type==GF_ISOM_BOX_TYPE_BXML) ? 1 : 0;
	return GF_OK;
}



GF_EXPORT
u32 gf_isom_get_meta_item_count(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return 0;
	return gf_list_count(meta->item_infos->item_infos);
}

GF_EXPORT
GF_Err gf_isom_get_meta_item_info(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num, 
							u32 *itemID, u32 *protection_idx, Bool *is_self_reference,
							const char **item_name, const char **item_mime_type, const char **item_encoding,
							const char **item_url, const char **item_urn)
{
	GF_ItemInfoEntryBox *iinf;
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;

	iinf = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, item_num-1);
	if (!iinf) return GF_BAD_PARAM;
	
	if (itemID) (*itemID) = iinf->item_ID;
	if (protection_idx) (*protection_idx) = iinf->item_protection_index;
	if (item_name) (*item_name) = iinf->item_name;
	if (item_mime_type) (*item_mime_type) = iinf->content_type;
	if (item_encoding) (*item_encoding) = iinf->content_encoding;
	if (is_self_reference) *is_self_reference = 0;

	if (item_url) (*item_url) = NULL;
	if (item_urn) (*item_urn) = NULL;

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (iloc->item_ID==iinf->item_ID) {
			if (iloc->data_reference_index) {
				GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->boxList, iloc->data_reference_index-1);
				if (a->type==GF_ISOM_BOX_TYPE_URL) {
					if (item_url) (*item_url) = ((GF_DataEntryURLBox*)a)->location;
				} else if (a->type==GF_ISOM_BOX_TYPE_URN) {
					if (item_url) (*item_url) = ((GF_DataEntryURNBox*)a)->location;
					if (item_urn) (*item_urn) = ((GF_DataEntryURNBox*)a)->nameURN;
				}
				break;
			} else if (is_self_reference && !iloc->base_offset) {
				GF_ItemExtentEntry *entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
				if (!entry->extent_length && !entry->original_extent_offset)
					*is_self_reference = 1;
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
u32 gf_isom_get_meta_item_by_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_ID)
{
	GF_ItemInfoEntryBox *iinf;
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return 0;
	count = gf_list_count(meta->item_infos->item_infos);
	for (i=0; i<count; i++) {
		iinf = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
		if (iinf->item_ID==item_ID) return i+1;
	}
	return 0;
}

GF_Err gf_isom_extract_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, const char *dump_file_name)
{
	char szPath[1024];
	GF_ItemExtentEntry *extent_entry;
	FILE *resource = NULL;
	u32 i, count;
	GF_ItemLocationEntry *location_entry;
	u32 item_num;
	char *item_name = NULL;

	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;

	item_num = gf_isom_get_meta_item_by_id(file, root_meta, track_num, item_id);
	if (item_num) {
		GF_ItemInfoEntryBox *item_entry = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, item_num-1);
		item_name = item_entry->item_name;
	} 

	location_entry = NULL;
	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		location_entry = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (location_entry->item_ID == item_id) break;
		location_entry = NULL;
	}

	if (!location_entry) return GF_BAD_PARAM;
	/*FIXME*/
	if (location_entry->data_reference_index) {
		char *item_url = NULL, *item_urn = NULL;
		GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->boxList, location_entry->data_reference_index-1);
		if (a->type==GF_ISOM_BOX_TYPE_URL) {
			item_url = ((GF_DataEntryURLBox*)a)->location;
		} else if (a->type==GF_ISOM_BOX_TYPE_URN) {
			item_url = ((GF_DataEntryURNBox*)a)->location;
			item_urn = ((GF_DataEntryURNBox*)a)->nameURN;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[IsoMedia] Item already outside the ISO file at URL: %s, URN: %s\n", (item_url?item_url:"N/A"), (item_urn?item_urn:"N/A") ));
		return GF_OK;
	}

	/*don't extract self-reference item*/
	count = gf_list_count(location_entry->extent_entries);
	if (!location_entry->base_offset && (count==1)) {
		extent_entry = (GF_ItemExtentEntry *)gf_list_get(location_entry->extent_entries, 0);
		if (!extent_entry->extent_length && !extent_entry->original_extent_offset) return GF_BAD_PARAM;
	}

	if (dump_file_name) {
		strcpy(szPath, dump_file_name);
	} else {
		if (item_name) strcpy(szPath, item_name);
		else sprintf(szPath, "item_id%02d", item_id);
	}
	resource = gf_f64_open(szPath, "wb");
		
	for (i=0; i<count; i++) {
		char buf_cache[4096];
		u64 remain;
		GF_ItemExtentEntry *extent_entry = (GF_ItemExtentEntry *)gf_list_get(location_entry->extent_entries, i);
		gf_bs_seek(file->movieFileMap->bs, /*location_entry->base_offset +*/ extent_entry->extent_offset);

		remain = extent_entry->extent_length;
		while (remain) {
			u32 cache_size = (remain>4096) ? 4096 : (u32) remain;
			gf_bs_read_data(file->movieFileMap->bs, buf_cache, cache_size);
			fwrite(buf_cache, 1, cache_size, resource);
			remain -= cache_size;
		}
	}
	fclose(resource);
	return GF_OK;
}

u32 gf_isom_get_meta_primary_item_id(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->primary_resource) return 0;
	return meta->primary_resource->item_ID;
}


#ifndef GPAC_READ_ONLY


GF_Err gf_isom_set_meta_type(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 metaType)
{
	char szName[20];
	GF_MetaBox *meta;

	GF_Err e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) {
		if (!metaType) return GF_OK;
		meta = (GF_MetaBox *) meta_New();
		if (root_meta) {
			file->meta = meta;
			gf_list_add(file->TopBoxes, meta);
		} else {
			gf_isom_insert_moov(file);
			if (!track_num) {
				file->moov->meta = meta;
			} else {
				GF_TrackBox *tk = (GF_TrackBox *)gf_list_get(file->moov->trackList, track_num-1);
				if (!tk) {
					gf_isom_box_del((GF_Box *)meta);
					return GF_BAD_PARAM;
				}
				tk->meta = meta;
			}
		}
	} else if (!metaType) {
		if (root_meta) {
			gf_list_del_item(file->TopBoxes, meta);
			gf_isom_box_del((GF_Box *)file->meta);
			file->meta = NULL;
		} else if (file->moov) {
			if (!track_num) {
				gf_isom_box_del((GF_Box *)file->moov->meta);
				file->moov->meta = NULL;
			} else {
				GF_TrackBox *tk = (GF_TrackBox *)gf_list_get(file->moov->trackList, track_num-1);
				if (!tk) return GF_BAD_PARAM;
				gf_isom_box_del((GF_Box *)tk->meta);
				tk->meta = NULL;
			}
		}
		return GF_OK;
	}

	if (!meta->handler) 
		meta->handler = (GF_HandlerBox *)hdlr_New();

	if (meta->handler->nameUTF8) free(meta->handler->nameUTF8);
	meta->handler->handlerType = metaType;
	sprintf(szName, "GPAC %s Handler", gf_4cc_to_str(metaType));
	meta->handler->nameUTF8 = strdup(szName);
	return GF_OK;
}

GF_Err gf_isom_remove_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	u32 i;
	GF_Box *a;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;
	i=0;
	while ((a = (GF_Box*)gf_list_enum(meta->other_boxes, &i))) {
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_XML:
		case GF_ISOM_BOX_TYPE_BXML:
			gf_list_rem(meta->other_boxes, i-1);
			gf_isom_box_del(a);
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err gf_isom_set_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *XMLFileName, Bool IsBinaryXML)
{
	GF_Err e;
	FILE *xmlfile;
	GF_XMLBox *xml;
	GF_MetaBox *meta;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	e = gf_isom_remove_meta_xml(file, root_meta, track_num);
	if (e) return e;

	xml = (GF_XMLBox *)xml_New();
	if (!xml) return GF_OUT_OF_MEM;
	gf_list_add(meta->other_boxes, xml);
	if (IsBinaryXML) xml->type = GF_ISOM_BOX_TYPE_BXML;


	/*assume 32bit max size = 4Go should be sufficient for a DID!!*/
	xmlfile = fopen(XMLFileName, "rb");
	if (!xmlfile) return GF_URL_ERROR;
	fseek(xmlfile, 0, SEEK_END);
	xml->xml_length = ftell(xmlfile);
	fseek(xmlfile, 0, SEEK_SET);
	xml->xml = (char*)malloc(sizeof(unsigned char)*xml->xml_length);
	xml->xml_length = fread(xml->xml, 1, sizeof(unsigned char)*xml->xml_length, xmlfile);
	if (ferror(xmlfile)) {
		free(xml->xml);
		xml->xml = NULL;
		return GF_BAD_PARAM;
	}
	fclose(xmlfile);
	return GF_OK;
}


GF_Err gf_isom_add_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path, const char *item_name, const char *mime_type, const char *content_encoding, const char *URL, const char *URN)
{
	GF_Err e;
	GF_ItemLocationEntry *location_entry;
	GF_ItemInfoEntryBox *infe;
	GF_MetaBox *meta;
	u32 lastItemID = 0;

	if (!self_reference && !item_name && !resource_path) return GF_BAD_PARAM;
	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	e = FlushCaptureMode(file);
	if (e) return e;

	/*check file exists */
	if (!URN && !URL && !self_reference) {
		FILE *src = fopen(resource_path, "rb");
		if (!src) return GF_URL_ERROR;
		fclose(src);
	}

	if (meta->item_infos) {
		u32 i;
		u32 item_count = gf_list_count(meta->item_infos->item_infos);
		for (i = 0; i < item_count; i++) {
			GF_ItemInfoEntryBox *e= (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
			if (e->item_ID > lastItemID) lastItemID = e->item_ID;
		}
	}

	infe = (GF_ItemInfoEntryBox *)infe_New();
	infe->item_ID = ++lastItemID;

	/*get relative name*/
	if (item_name) {
		infe->item_name = strdup(item_name);
	} else if (resource_path) {
		if (strrchr(resource_path, GF_PATH_SEPARATOR)) {
			infe->item_name = strdup(strrchr(resource_path, GF_PATH_SEPARATOR) + 1);
		} else {
			infe->item_name = strdup(resource_path);
		}
	}

	if (mime_type) {
		infe->content_type = strdup(mime_type);
	} else {
		infe->content_type = strdup("application/octet-stream");
	}
	if (content_encoding) infe->content_encoding = strdup(content_encoding);

	/*Creation of the ItemLocation */
	location_entry = (GF_ItemLocationEntry*)malloc(sizeof(GF_ItemLocationEntry));
	if (!location_entry) {
		gf_isom_box_del((GF_Box *)infe);
		return GF_OUT_OF_MEM;
	}
	memset(location_entry, 0, sizeof(GF_ItemLocationEntry));
	location_entry->extent_entries = gf_list_new();
		
	/*Creates an mdat if it does not exist*/
	if (!file->mdat) {
		file->mdat = (GF_MediaDataBox *)mdat_New();
		gf_list_add(file->TopBoxes, file->mdat);
	}

	/*Creation an ItemLocation Box if it does not exist*/
	if (!meta->item_locations) meta->item_locations = (GF_ItemLocationBox *)iloc_New();
	gf_list_add(meta->item_locations->location_entries, location_entry);
	location_entry->item_ID = lastItemID;

	if (!meta->item_infos) meta->item_infos = (GF_ItemInfoBox *) iinf_New();
	e = gf_list_add(meta->item_infos->item_infos, infe);
	if (e) return e;

	/*0: the current file*/
	location_entry->data_reference_index = 0;
	if (self_reference) {
		GF_ItemExtentEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemExtentEntry);
		gf_list_add(location_entry->extent_entries, entry);
		if (!infe->item_name) infe->item_name = strdup("");
		return GF_OK;
	}

	/*file not copied, just referenced*/
	if (URL || URN) {
		u32 dataRefIndex;
		if (!meta->file_locations) meta->file_locations = (GF_DataInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DINF);
		if (!meta->file_locations->dref) meta->file_locations->dref = (GF_DataReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DREF);
		e = Media_FindDataRef(meta->file_locations->dref, (char *) URL, (char *) URN, &dataRefIndex);
		if (e) return e;
		if (!dataRefIndex) {
			e = Media_CreateDataRef(meta->file_locations->dref, (char *) URL, (char *) URN, &dataRefIndex);
			if (e) return e;
		}
		location_entry->data_reference_index = dataRefIndex;
	}

	/*capture mode, write to disk*/
	if ((file->openMode == GF_ISOM_OPEN_WRITE) && !location_entry->data_reference_index) {
		FILE *src;
		GF_ItemExtentEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemExtentEntry);

		location_entry->base_offset = gf_bs_get_position(file->editFileMap->bs);

		/*update base offset size*/
		if (location_entry->base_offset>0xFFFFFFFF) meta->item_locations->base_offset_size = 8;
		else if (location_entry->base_offset && !meta->item_locations->base_offset_size) meta->item_locations->base_offset_size = 4;

		entry->extent_length = 0;
		entry->extent_offset = 0;
		gf_list_add(location_entry->extent_entries, entry);

		src = gf_f64_open(resource_path, "rb");
		if (src) {
			char cache_data[4096];
			u64 remain;
			gf_f64_seek(src, 0, SEEK_END);
			entry->extent_length = gf_f64_tell(src);
			gf_f64_seek(src, 0, SEEK_SET);

			remain = entry->extent_length;
			while (remain) {
				u32 size_cache = (remain>4096) ? 4096 : (u32) remain;
				fread(cache_data, 1, size_cache, src);
				gf_bs_write_data(file->editFileMap->bs, cache_data, size_cache);
				remain -= size_cache;
			}
			fclose(src);

			/*update length size*/
			if (entry->extent_length>0xFFFFFFFF) meta->item_locations->length_size = 8;
			else if (entry->extent_length && !meta->item_locations->length_size) meta->item_locations->length_size = 4;
		}
	}
	/*store full path for info*/
	else if (!location_entry->data_reference_index) {
		infe->full_path = strdup(resource_path);
	}
	return GF_OK;
}


GF_Err gf_isom_remove_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id)
{
	GF_ItemInfoEntryBox *iinf;
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	u32 item_num;
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;

	item_num = gf_isom_get_meta_item_by_id(file, root_meta, track_num, item_id);
	if (!item_num) return GF_BAD_PARAM;
	iinf = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, item_num-1);
	gf_list_rem(meta->item_infos->item_infos, item_num-1);

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (iloc->item_ID==iinf->item_ID) {
			/*FIXME: remove data ref...*/
			if (iloc->data_reference_index) { }

			gf_list_rem(meta->item_locations->location_entries, i);
			gf_isom_box_del((GF_Box *)iloc);
			break;
		}
	}
	gf_isom_box_del((GF_Box *)iinf);
	return GF_OK;
}

GF_Err gf_isom_set_meta_primary_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;
	/*either one or the other*/
	if (gf_isom_has_meta_xml(file, root_meta, track_num)) return GF_BAD_PARAM;

	if (meta->primary_resource) gf_isom_box_del((GF_Box*)meta->primary_resource);
	meta->primary_resource = (GF_PrimaryItemBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_PITM);
	meta->primary_resource->item_ID = item_id;
	return GF_OK;
}


#endif	//GPAC_READ_ONLY


