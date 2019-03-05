/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2019
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

GF_EXPORT
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
	if (!xml || !xml->xml) return GF_BAD_PARAM;

	didfile = gf_fopen(outName, "wb");
	if (!didfile) return GF_IO_ERR;
	gf_fwrite(xml->xml, strlen(xml->xml), 1, didfile);
	gf_fclose(didfile);

	if (is_binary) *is_binary = (xml->type==GF_ISOM_BOX_TYPE_BXML) ? 1 : 0;
	return GF_OK;
}

GF_XMLBox *gf_isom_get_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool *is_binary)
{
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return NULL;

	/*Find XMLBox*/
	count = gf_list_count(meta->other_boxes);
	for (i = 0; i <count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->other_boxes, i);
		if (a->type == GF_ISOM_BOX_TYPE_XML) {
			*is_binary = 0;
			return (GF_XMLBox *)a;
		} else if (a->type == GF_ISOM_BOX_TYPE_BXML) {
			*is_binary = 1;
			return (GF_XMLBox *)a;
		}
	}
	return NULL;
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
                                  u32 *itemID, u32 *type, u32 *protection_idx, Bool *is_self_reference,
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
	if (type) *type = iinf->item_type;

	if (item_url) (*item_url) = NULL;
	if (item_urn) (*item_urn) = NULL;

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (iloc->item_ID==iinf->item_ID) {
			if (iloc->data_reference_index) {
				GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->other_boxes, iloc->data_reference_index-1);
				if (a->type==GF_ISOM_BOX_TYPE_URL) {
					if (item_url) (*item_url) = ((GF_DataEntryURLBox*)a)->location;
				} else if (a->type==GF_ISOM_BOX_TYPE_URN) {
					if (item_url) (*item_url) = ((GF_DataEntryURNBox*)a)->location;
					if (item_urn) (*item_urn) = ((GF_DataEntryURNBox*)a)->nameURN;
				}
				break;
			} else if (is_self_reference && !iloc->base_offset) {
				GF_ItemExtentEntry *entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
				if (!entry->extent_length
#ifndef GPAC_DISABLE_ISOM_WRITE
				        && !entry->original_extent_offset
#endif
				   )
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

GF_EXPORT
GF_Err gf_isom_extract_meta_item_extended(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, const char *dump_file_name, char **out_data, u32 *out_size, const char **out_mime, Bool use_annex_b)
{
	GF_BitStream *item_bs;
	char szPath[1024];
	GF_ItemExtentEntry *extent_entry;
	FILE *resource = NULL;
	u32 i, count;
	GF_ItemLocationEntry *location_entry;
	u32 item_num;
	u32 item_type = 0;
	u32 nalu_size_length = 0;
	u64 idat_offset = 0;
	char *item_name = NULL;

	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;

	if (out_mime) *out_mime = NULL;

	item_num = gf_isom_get_meta_item_by_id(file, root_meta, track_num, item_id);
	if (item_num) {
		GF_ItemInfoEntryBox *item_entry = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, item_num-1);
		item_name = item_entry->item_name;
		if (out_mime) *out_mime = item_entry->content_type;

		item_type = item_entry->item_type;
	}

	location_entry = NULL;
	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		location_entry = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (location_entry->item_ID == item_id) break;
		location_entry = NULL;
	}

	// switch (item_type) {
	// case GF_ISOM_SUBTYPE_HVC1:
	// case GF_ISOM_SUBTYPE_AVC_H264:
	// case GF_ISOM_SUBTYPE_JPEG:
	// 	break;
	// default:
	// 	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[IsoMedia] Extracting item type %s not supported\n", gf_4cc_to_str(item_type) ));
	// 	return GF_NOT_SUPPORTED;
	// }

	if (!location_entry) return GF_BAD_PARAM;

	/* offsets are expressed from the start of the idat box instead of the start of the file */
	if (location_entry->construction_method == 1) {
		int found = 0;

		count = gf_list_count(meta->other_boxes);
		for (i = 0; i <count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(meta->other_boxes, i);

			if (a->type == GF_ISOM_BOX_TYPE_IDAT) {

				GF_MediaDataBox *p = (GF_MediaDataBox *)a;
				idat_offset = p->bsOffset;
				found = 1;
				break;

			}
			if (!found) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Item %d references an inexistant idat box\n", item_num));
				return GF_BAD_PARAM;
			}
		}
	}
	/* when construction_method==1, data_reference_index is ignored */
	/*FIXME*/
	else if (location_entry->data_reference_index) {
		char *item_url = NULL, *item_urn = NULL;
		GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->other_boxes, location_entry->data_reference_index-1);
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
		if (!extent_entry->extent_length
#ifndef GPAC_DISABLE_ISOM_WRITE
		        && !extent_entry->original_extent_offset
#endif
		   ) return GF_BAD_PARAM;
	}

	item_bs = NULL;


	if (out_data) {
		item_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	} else if (dump_file_name) {
		strcpy(szPath, dump_file_name);
		resource = gf_fopen(szPath, "wb");
		item_bs = gf_bs_from_file(resource, GF_BITSTREAM_WRITE);
	} else {
		if (item_name && strlen(item_name) > 0) strcpy(szPath, item_name);
		else sprintf(szPath, "item_id%02d", item_id);
		resource = gf_fopen(szPath, "wb");
		item_bs = gf_bs_from_file(resource, GF_BITSTREAM_WRITE);
	}

	if ((item_type == GF_ISOM_SUBTYPE_HVC1) || (item_type == GF_ISOM_SUBTYPE_AVC_H264) ) {
		u32 i, count, j, c2;
		GF_HEVCConfigurationBox *hvcc = NULL;
		GF_AVCConfigurationBox *avcc = NULL;
		if (! meta->item_props) return GF_NON_COMPLIANT_BITSTREAM;
		if (! meta->item_props->property_container) return GF_NON_COMPLIANT_BITSTREAM;
		if (! meta->item_props->property_association) return GF_NON_COMPLIANT_BITSTREAM;

		count = gf_list_count(meta->item_props->property_association->entries);
		for (i=0; i<count; i++) {
			GF_ItemPropertyAssociationEntry *e = gf_list_get(meta->item_props->property_association->entries, i);
			if (e->item_id!=item_id) continue;
			c2 = gf_list_count(e->property_index);
			for (j=0; j<c2; j++) {
				u32 *idx = gf_list_get(e->property_index, j);
				if (! (*idx) ) continue;
				hvcc = gf_list_get(meta->item_props->property_container->other_boxes, (*idx) - 1);
				if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
				if (hvcc->type == GF_ISOM_BOX_TYPE_HVCC) break;
				if (hvcc->type == GF_ISOM_BOX_TYPE_AVCC) {
					avcc = (GF_AVCConfigurationBox *) hvcc;
					hvcc = NULL;
					break;
				}
			}
			if (avcc || hvcc) break;
		}
		if (hvcc) {
			if (! hvcc->config) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Missing HEVC config in hvcC\n"));
			} else {
				if (use_annex_b) {
					hvcc->config->write_annex_b = GF_TRUE;
					gf_odf_hevc_cfg_write_bs(hvcc->config, item_bs);
					hvcc->config->write_annex_b = GF_FALSE;
				}
				nalu_size_length = hvcc->config->nal_unit_size;
			}
		} else if (avcc) {
			if (! avcc->config) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Missing AVC config in avcC\n"));
			} else {
				if (use_annex_b) {
					avcc->config->write_annex_b = GF_TRUE;
					gf_odf_avc_cfg_write_bs(avcc->config, item_bs);
					avcc->config->write_annex_b = GF_FALSE;
				}
				nalu_size_length = avcc->config->nal_unit_size;
			}
		}
	}

	for (i=0; i<count; i++) {
		char buf_cache[4096];
		u64 remain;
		GF_ItemExtentEntry *extent_entry = (GF_ItemExtentEntry *)gf_list_get(location_entry->extent_entries, i);
		gf_bs_seek(file->movieFileMap->bs, idat_offset + location_entry->base_offset + extent_entry->extent_offset);

		remain = extent_entry->extent_length;
		while (remain) {
			if (nalu_size_length) {
				u32 nal_size = gf_bs_read_int(file->movieFileMap->bs, 8*nalu_size_length);
				assert(remain>nalu_size_length);

				if (use_annex_b)
					gf_bs_write_u32(item_bs, 1);
				else
					gf_bs_write_int(item_bs, nal_size, 8*nalu_size_length);

				remain -= nalu_size_length + nal_size;
				while (nal_size) {
					u32 cache_size = (nal_size>4096) ? 4096 : (u32) nal_size;

					gf_bs_read_data(file->movieFileMap->bs, buf_cache, cache_size);
					gf_bs_write_data(item_bs, buf_cache, cache_size);
					nal_size -= cache_size;
				}
			} else {
				u32 cache_size = (remain>4096) ? 4096 : (u32) remain;
				gf_bs_read_data(file->movieFileMap->bs, buf_cache, cache_size);
				gf_bs_write_data(item_bs, buf_cache, cache_size);
				remain -= cache_size;
			}
		}
	}
	if (out_data) {
		gf_bs_get_content(item_bs, out_data, out_size);
	}
	if (resource) {
		gf_fclose(resource);
	}
	gf_bs_del(item_bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_extract_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, const char *dump_file_name)
{
	return gf_isom_extract_meta_item_extended(file, root_meta, track_num, item_id, dump_file_name, NULL, NULL, NULL, GF_TRUE);
}

GF_EXPORT
GF_Err gf_isom_extract_meta_item_mem(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, char **out_data, u32 *out_size, const char **out_mime, Bool use_annex_b)
{
	return gf_isom_extract_meta_item_extended(file, root_meta, track_num, item_id, NULL, out_data, out_size, out_mime, use_annex_b);
}

GF_EXPORT
u32 gf_isom_get_meta_primary_item_id(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->primary_resource) return 0;
	return meta->primary_resource->item_ID;
}


#ifndef GPAC_DISABLE_ISOM_WRITE


GF_EXPORT
GF_Err gf_isom_set_meta_type(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 metaType)
{
	char szName[20];
	GF_MetaBox *meta;

	GF_Err e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) {
		if (!metaType) return GF_OK;
		meta = (GF_MetaBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_META);
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
		meta->handler = (GF_HandlerBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_HDLR);

	if (meta->handler->nameUTF8) gf_free(meta->handler->nameUTF8);
	meta->handler->handlerType = metaType;
	sprintf(szName, "GPAC %s Handler", gf_4cc_to_str(metaType));
	meta->handler->nameUTF8 = gf_strdup(szName);
	return GF_OK;
}

GF_EXPORT
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

GF_EXPORT
GF_Err gf_isom_set_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *XMLFileName, Bool IsBinaryXML)
{
	GF_Err e;
	FILE *xmlfile;
	GF_XMLBox *xml;
	GF_MetaBox *meta;
	u32 length, bread;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	e = gf_isom_remove_meta_xml(file, root_meta, track_num);
	if (e) return e;

	xml = (GF_XMLBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_XML);
	if (!xml) return GF_OUT_OF_MEM;
	gf_list_add(meta->other_boxes, xml);
	if (IsBinaryXML) xml->type = GF_ISOM_BOX_TYPE_BXML;


	/*assume 32bit max size = 4Go should be sufficient for a DID!!*/
	xmlfile = gf_fopen(XMLFileName, "rb");
	if (!xmlfile) return GF_URL_ERROR;
	gf_fseek(xmlfile, 0, SEEK_END);
	assert(gf_ftell(xmlfile) < 1<<31);
	length = (u32) gf_ftell(xmlfile);
	gf_fseek(xmlfile, 0, SEEK_SET);
	xml->xml = (char*)gf_malloc(sizeof(unsigned char)*(length+1));
	bread =  (u32) fread(xml->xml, 1, sizeof(unsigned char)*length, xmlfile);
	if (ferror(xmlfile) || (bread != length)) {
		gf_free(xml->xml);
		xml->xml = NULL;
		return GF_BAD_PARAM;
	}
	else {
		xml->xml[length] = '\0';
	}
	gf_fclose(xmlfile);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_meta_xml_memory(GF_ISOFile *file, Bool root_meta, u32 track_num, unsigned char *data, u32 data_size, Bool IsBinaryXML)
{
	GF_Err e;
	GF_XMLBox *xml;
	GF_MetaBox *meta;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	e = gf_isom_remove_meta_xml(file, root_meta, track_num);
	if (e) return e;

	xml = (GF_XMLBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_XML);
	if (!xml) return GF_OUT_OF_MEM;
	gf_list_add(meta->other_boxes, xml);
	if (IsBinaryXML) xml->type = GF_ISOM_BOX_TYPE_BXML;


	/*assume 32bit max size = 4Go should be sufficient for a DID!!*/
	xml->xml = (char*)gf_malloc(sizeof(unsigned char)*data_size);
	memcpy(xml->xml, data, sizeof(unsigned char)*data_size);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_meta_image_props(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, GF_ImageItemProperties *prop) {
	u32 count, i;
	u32 count2, j;
	GF_ItemPropertyAssociationBox *ipma = NULL;
	GF_ItemPropertyContainerBox *ipco = NULL;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	memset(prop, 0, sizeof(GF_ImageItemProperties));
	if (!meta->item_props) return GF_OK;

	ipma = meta->item_props->property_association;
	ipco = meta->item_props->property_container;

	count = gf_list_count(ipma->entries);
	for (i = 0; i < count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
		if (entry->item_id == item_id) {
			count2 = gf_list_count(entry->property_index);
			for (j = 0; j < count2; j++) {
				u32 *index = (u32 *)gf_list_get(entry->property_index, j);
				GF_Box *b = (GF_Box *)gf_list_get(ipco->other_boxes, *index - 1);
				if (!b) continue;
				switch (b->type) {
				case GF_ISOM_BOX_TYPE_ISPE:
				{
					GF_ImageSpatialExtentsPropertyBox *ispe = (GF_ImageSpatialExtentsPropertyBox *)b;
					prop->width = ispe->image_width;
					prop->height = ispe->image_height;
				}
				break;
				case GF_ISOM_BOX_TYPE_RLOC:
				{
					GF_RelativeLocationPropertyBox *rloc = (GF_RelativeLocationPropertyBox *)b;
					prop->hOffset = rloc->horizontal_offset;
					prop->vOffset = rloc->vertical_offset;
				}
				break;
				case GF_ISOM_BOX_TYPE_PASP:
				{
					GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *)b;
					prop->hSpacing = pasp->hSpacing;
					prop->vSpacing = pasp->vSpacing;
				}
				break;
				case GF_ISOM_BOX_TYPE_PIXI:
				{
					GF_PixelInformationPropertyBox *pixi = (GF_PixelInformationPropertyBox *)b;
					if (pixi->num_channels > 3) {
						return GF_BAD_PARAM;
					}
					prop->num_channels = pixi->num_channels;
					memset(prop->bits_per_channel, 0, 3);
					memcpy(prop->bits_per_channel, pixi->bits_per_channel, pixi->num_channels);
				}
				break;
				case GF_ISOM_BOX_TYPE_IROT:
				{
					GF_ImageRotationBox *irot = (GF_ImageRotationBox *)b;
					prop->angle = irot->angle * 90;
				}
				break;
				case GF_ISOM_BOX_TYPE_HVCC:
				case GF_ISOM_BOX_TYPE_AVCC:
					prop->config = b;
					break;
				}
			}
		}
	}
	return GF_OK;
}

static s32 meta_find_prop(GF_ItemPropertyContainerBox *boxes, GF_ImageItemProperties *prop) {
	u32 i;
	u32 count;
	count = gf_list_count(boxes->other_boxes);
	for (i = 0; i < count; i++) {
		GF_Box *b = (GF_Box *)gf_list_get(boxes->other_boxes, i);
		switch(b->type) {
		case GF_ISOM_BOX_TYPE_ISPE:
		{
			GF_ImageSpatialExtentsPropertyBox *ispe = (GF_ImageSpatialExtentsPropertyBox *)b;
			if ((prop->width || prop->height) && ispe->image_width == prop->width && ispe->image_height == prop->height) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_RLOC:
		{
			GF_RelativeLocationPropertyBox *rloc = (GF_RelativeLocationPropertyBox *)b;
			if ((prop->hOffset || prop->vOffset) && rloc->horizontal_offset == prop->hOffset && rloc->vertical_offset == prop->vOffset) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_PASP:
		{
			GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *)b;
			if ((prop->hSpacing || prop->vSpacing) && pasp->hSpacing == prop->hSpacing && pasp->vSpacing == prop->vSpacing) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_IROT:
		{
			GF_ImageRotationBox *irot = (GF_ImageRotationBox *)b;
			if (prop->angle && irot->angle*90 == prop->angle) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_PIXI:
		{
			GF_PixelInformationPropertyBox *pixi = (GF_PixelInformationPropertyBox *)b;
			if (prop->num_channels && pixi->num_channels == prop->num_channels) {
				int j;
				for (j = 0; j < pixi->num_channels; j++) {
					if (pixi->bits_per_channel[j] != prop->bits_per_channel[j]) {
						break;
					}
				}
				return i;
			}
		}
		break;
		default:
			if (gf_isom_box_equal(prop->config, b)) {
				return i;
			}
		}
	}
	return -1;
}

static void meta_add_item_property_association(GF_ItemPropertyAssociationBox *ipma, u32 item_ID, u32 prop_index, Bool essential) {
	u32 i, count;
	GF_ItemPropertyAssociationEntry *found_entry = NULL;
	Bool *ess = (Bool *)gf_malloc(sizeof(Bool));
	u32 *index = (u32 *)gf_malloc(sizeof(u32));

	*ess = essential;
	*index = prop_index;
	count = gf_list_count(ipma->entries);
	for (i = 0; i < count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
		if (entry->item_id == item_ID) {
			found_entry = entry;
			break;
		}
	}
	if (!found_entry) {
		GF_SAFEALLOC(found_entry, GF_ItemPropertyAssociationEntry);
		if (!found_entry) return;

		gf_list_add(ipma->entries, found_entry);
		found_entry->item_id = item_ID;
		found_entry->essential = gf_list_new();
		found_entry->property_index= gf_list_new();
	}
	gf_list_add(found_entry->essential, ess);
	gf_list_add(found_entry->property_index, index);
}
	
static void meta_process_image_properties(GF_MetaBox *meta, u32 item_ID, GF_ImageItemProperties *image_props) {
	GF_ImageItemProperties searchprop;
	GF_ItemPropertyAssociationBox *ipma;
	GF_ItemPropertyContainerBox *ipco;
	s32 prop_index;
	memset(&searchprop, 0, sizeof(GF_ImageItemProperties));

	if (!meta->item_props) {
		meta->item_props = (GF_ItemPropertiesBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IPRP);
		meta->item_props->property_container = (GF_ItemPropertyContainerBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IPCO);
		ipco = meta->item_props->property_container;
		ipma = (GF_ItemPropertyAssociationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IPMA);
		meta->item_props->property_association = ipma;
	} else {
		ipco = meta->item_props->property_container;
		ipma = meta->item_props->property_association;
	}

	if (strlen(image_props->iccPath) > 0) {
		FILE *fp = gf_fopen(image_props->iccPath, "rb");
		if (fp) {
			size_t read;
			GF_ColourInformationBox *colr = (GF_ColourInformationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_COLR);
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] reading ICC colour profile from file %s\n", &image_props->iccPath));
			colr->colour_type = GF_ISOM_SUBTYPE_PROF;
			fseek(fp,0,SEEK_END);
			colr->opaque_size = (u32)ftell(fp);
			fseek(fp,0,SEEK_SET);
			colr->opaque = malloc(colr->opaque_size);
			read = gf_fread(colr->opaque, 1, colr->opaque_size, fp);
			gf_fclose(fp);
			if (ferror(fp) || read != colr->opaque_size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error reading ICC colour profile from file %s\n", &image_props->iccPath));
			} else {
				gf_list_add(ipco->other_boxes, colr);
				prop_index = gf_list_count(ipco->other_boxes) - 1;
				meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error opening ICC colour profile file at %s\n", &image_props->iccPath));
		}
	}

	if (image_props->width || image_props->height) {
		searchprop.width = image_props->width;
		searchprop.height = image_props->height;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_ImageSpatialExtentsPropertyBox *ispe = (GF_ImageSpatialExtentsPropertyBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_ISPE);
			ispe->image_width = image_props->width;
			ispe->image_height = image_props->height;
			gf_list_add(ipco->other_boxes, ispe);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		searchprop.width = 0;
		searchprop.height = 0;
	}
	if (image_props->hOffset || image_props->vOffset) {
		searchprop.hOffset = image_props->hOffset;
		searchprop.vOffset = image_props->vOffset;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_RelativeLocationPropertyBox *rloc = (GF_RelativeLocationPropertyBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_RLOC);
			rloc->horizontal_offset = image_props->hOffset;
			rloc->vertical_offset = image_props->vOffset;
			gf_list_add(ipco->other_boxes, rloc);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		searchprop.hOffset = 0;
		searchprop.vOffset = 0;
	}
	if (image_props->hSpacing || image_props->vSpacing) {
		searchprop.hSpacing = image_props->hSpacing;
		searchprop.vSpacing = image_props->vSpacing;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_PASP);
			pasp->hSpacing = image_props->hSpacing;
			pasp->vSpacing = image_props->vSpacing;
			gf_list_add(ipco->other_boxes, pasp);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		searchprop.hSpacing = 0;
		searchprop.vSpacing = 0;
	}
	if (image_props->angle) {
		searchprop.angle = image_props->angle;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_ImageRotationBox *irot = (GF_ImageRotationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IROT);
			irot->angle = image_props->angle/90;
			gf_list_add(ipco->other_boxes, irot);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		searchprop.angle = 0;
	}
	if (image_props->config) {
		searchprop.config = image_props->config;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			gf_list_add(ipco->other_boxes, gf_isom_clone_config_box(image_props->config));
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		searchprop.config = NULL;
	}
	if (image_props->alpha) {
		searchprop.alpha = image_props->alpha;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_AuxiliaryTypePropertyBox *auxC = (GF_AuxiliaryTypePropertyBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_AUXC);
			auxC->aux_urn = gf_strdup("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha");
			gf_list_add(ipco->other_boxes, auxC);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		searchprop.alpha = GF_FALSE;
	}
	if (image_props->num_channels) {
		searchprop.num_channels = image_props->num_channels;
		memcpy(searchprop.bits_per_channel, image_props->bits_per_channel, 3);
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_PixelInformationPropertyBox *pixi = (GF_PixelInformationPropertyBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_PIXI);
			pixi->num_channels = image_props->num_channels;
			pixi->bits_per_channel = gf_malloc(pixi->num_channels);
			memcpy(pixi->bits_per_channel, image_props->bits_per_channel, image_props->num_channels);
			gf_list_add(ipco->other_boxes, pixi);
			prop_index = gf_list_count(ipco->other_boxes) - 1;
		}
		meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		searchprop.num_channels = 0;
	}
}

GF_EXPORT
GF_Err gf_isom_meta_get_next_item_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 *item_id)
{
	GF_MetaBox *meta;
	u32 lastItemID = 0;

	if (!file || !item_id) return GF_BAD_PARAM;
	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Missing meta box"));
		return GF_BAD_PARAM;
	}

	if (meta->item_infos) {
		u32 i;
		u32 item_count = gf_list_count(meta->item_infos->item_infos);
		for (i = 0; i < item_count; i++) {
			GF_ItemInfoEntryBox *e = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
			if (e->item_ID > lastItemID) lastItemID = e->item_ID;
		}
		*item_id = lastItemID+1;
	}
	else {
		*item_id = lastItemID + 1;
	}
	return GF_OK;
}

GF_Err gf_isom_add_meta_item_extended(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path,
                                      const char *item_name, u32 item_id, u32 item_type, const char *mime_type, const char *content_encoding,
                                      GF_ImageItemProperties *image_props,
                                      const char *URL, const char *URN,
                                      char *data, u32 data_len, GF_List *item_extent_refs)
{
	u32 i;
	GF_Err e;
	GF_ItemLocationEntry *location_entry;
	GF_ItemInfoEntryBox *infe;
	GF_MetaBox *meta;
	u32 lastItemID = 0;

	if (!self_reference && !resource_path && !data && !item_extent_refs) return GF_BAD_PARAM;
	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Trying to add item, but missing meta box"));
		return GF_BAD_PARAM;
	}

	e = FlushCaptureMode(file);
	if (e) return e;

	/*check file exists */
	if (resource_path) {
		FILE *src = gf_fopen(resource_path, "rb");
		if (!src) return GF_URL_ERROR;
		gf_fclose(src);
	}

	if (meta->item_infos) {
		u32 item_count = gf_list_count(meta->item_infos->item_infos);
		for (i = 0; i < item_count; i++) {
			GF_ItemInfoEntryBox *e= (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
			if (e->item_ID > lastItemID) lastItemID = e->item_ID;
			if (item_id == e->item_ID) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[IsoMedia] Item with id %d already exists, ignoring id\n", item_id));
				item_id = 0;
			}
		}
	}

	infe = (GF_ItemInfoEntryBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_INFE);
	if (item_id) {
		infe->item_ID = item_id;
	} else {
		infe->item_ID = ++lastItemID;
	}

	/*get relative name*/
	if (item_name) {
		infe->item_name = gf_strdup(item_name);
	} else if (resource_path) {
		infe->item_name = gf_strdup(gf_file_basename( resource_path ));
	}

	infe->item_type = item_type;

	if (mime_type) {
		infe->content_type = gf_strdup(mime_type);
	} else {
		infe->content_type = gf_strdup("application/octet-stream");
	}
	if (content_encoding) infe->content_encoding = gf_strdup(content_encoding);

	/*Creation of the ItemLocation */
	location_entry = (GF_ItemLocationEntry*)gf_malloc(sizeof(GF_ItemLocationEntry));
	if (!location_entry) {
		gf_isom_box_del((GF_Box *)infe);
		return GF_OUT_OF_MEM;
	}
	memset(location_entry, 0, sizeof(GF_ItemLocationEntry));
	location_entry->extent_entries = gf_list_new();

	/*Creates an mdat if it does not exist*/
	if (!file->mdat) {
		file->mdat = (GF_MediaDataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
		gf_list_add(file->TopBoxes, file->mdat);
	}

	/*Creation an ItemLocation Box if it does not exist*/
	if (!meta->item_locations) meta->item_locations = (GF_ItemLocationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_ILOC);
	gf_list_add(meta->item_locations->location_entries, location_entry);
	location_entry->item_ID = infe->item_ID;

	if (!meta->item_infos) meta->item_infos = (GF_ItemInfoBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IINF);
	e = gf_list_add(meta->item_infos->item_infos, infe);
	if (e) return e;

	if (image_props) {
		if (image_props->hidden) {
			infe->flags = 0x1;
		}
		meta_process_image_properties(meta, infe->item_ID, image_props);
	}

	/*0: the current file*/
	location_entry->data_reference_index = 0;
	if (self_reference) {
		GF_ItemExtentEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemExtentEntry);
		gf_list_add(location_entry->extent_entries, entry);
		if (!infe->item_name) infe->item_name = gf_strdup("");
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
			e = Media_CreateDataRef(file, meta->file_locations->dref, (char *) URL, (char *) URN, &dataRefIndex);
			if (e) return e;
		}
		location_entry->data_reference_index = dataRefIndex;
	}

	if (item_extent_refs && gf_list_count(item_extent_refs)) {
		u32 refs_count;
		location_entry->construction_method = 2;
		meta->item_locations->index_size = 4;
		refs_count = gf_list_count(item_extent_refs);
		for (i = 0; i < refs_count; i++) {
			u32 *item_index;
			GF_ItemExtentEntry *entry;
			GF_SAFEALLOC(entry, GF_ItemExtentEntry);
			gf_list_add(location_entry->extent_entries, entry);
			item_index = (u32 *)gf_list_get(item_extent_refs, i);
			gf_isom_meta_add_item_ref(file, root_meta, track_num, infe->item_ID, *item_index, GF_ISOM_REF_ILOC, &(entry->extent_index));
		}
	}
	else {
		/*capture mode, write to disk*/
		if ((file->openMode == GF_ISOM_OPEN_WRITE) && !location_entry->data_reference_index) {
			FILE *src;
			GF_ItemExtentEntry *entry;
			GF_SAFEALLOC(entry, GF_ItemExtentEntry);

			location_entry->base_offset = gf_bs_get_position(file->editFileMap->bs);

			/*update base offset size*/
			if (location_entry->base_offset > 0xFFFFFFFF) meta->item_locations->base_offset_size = 8;
			else if (location_entry->base_offset && !meta->item_locations->base_offset_size) meta->item_locations->base_offset_size = 4;

			entry->extent_length = 0;
			entry->extent_offset = 0;
			gf_list_add(location_entry->extent_entries, entry);

			if (data) {
				gf_bs_write_data(file->editFileMap->bs, data, data_len);
				/*update length size*/
				if (entry->extent_length > 0xFFFFFFFF) meta->item_locations->length_size = 8;
				else if (entry->extent_length && !meta->item_locations->length_size) meta->item_locations->length_size = 4;
			}
			else if (resource_path) {
				src = gf_fopen(resource_path, "rb");
				if (src) {
					char cache_data[4096];
					u64 remain;
					gf_fseek(src, 0, SEEK_END);
					entry->extent_length = gf_ftell(src);
					gf_fseek(src, 0, SEEK_SET);

					remain = entry->extent_length;
					while (remain) {
						u32 size_cache = (remain > 4096) ? 4096 : (u32)remain;
						size_t read = fread(cache_data, 1, size_cache, src);
						if (read == (size_t)-1) break;
						gf_bs_write_data(file->editFileMap->bs, cache_data, (u32)read);
						remain -= (u32)read;
					}
					gf_fclose(src);

					/*update length size*/
					if (entry->extent_length > 0xFFFFFFFF) meta->item_locations->length_size = 8;
					else if (entry->extent_length && !meta->item_locations->length_size) meta->item_locations->length_size = 4;
				}
			}
		}
		/*store full path for info*/
		else if (!location_entry->data_reference_index) {
			if (data) {
				infe->full_path = (char *)gf_malloc(sizeof(char) * data_len);
				memcpy(infe->full_path, data, sizeof(char) * data_len);
				infe->data_len = data_len;
			}
			else {
				infe->full_path = gf_strdup(resource_path);
				infe->data_len = 0;
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path, const char *item_name, u32 item_id, u32 item_type,
                             const char *mime_type, const char *content_encoding, const char *URL, const char *URN,
                             GF_ImageItemProperties *image_props)
{
	return gf_isom_add_meta_item_extended(file, root_meta, track_num, self_reference, resource_path, item_name, item_id, item_type, mime_type, content_encoding, image_props, URL, URN, NULL, 0, NULL);
}

GF_EXPORT
GF_Err gf_isom_add_meta_item_memory(GF_ISOFile *file, Bool root_meta, u32 track_num, const char *item_name, u32 item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, char *data, u32 data_len, GF_List *item_extent_refs)
{
	return gf_isom_add_meta_item_extended(file, root_meta, track_num, GF_FALSE, NULL, item_name, item_id, item_type, mime_type, content_encoding, image_props, NULL, NULL, data, data_len, item_extent_refs);
}

GF_EXPORT
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
			iloc_entry_del(iloc);
			break;
		}
	}
	gf_isom_box_del((GF_Box *)iinf);
	return GF_OK;
}

GF_EXPORT
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


#endif	/*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_Err gf_isom_meta_add_item_ref(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 from_id, u32 to_id, u32 type, u64 *ref_index)
{
	u32 i, count;
	s32 index = -1;
	GF_ItemReferenceTypeBox *ref;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;
	if (!meta->item_refs) {
		meta->item_refs = (GF_ItemReferenceBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_IREF);
	}
	count = gf_list_count(meta->item_refs->references);
	for (i = 0; i < count; i++) {
		ref = (GF_ItemReferenceTypeBox *)gf_list_get(meta->item_refs->references, i);
		if (ref->from_item_id == from_id && ref->reference_type == type) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		ref = (GF_ItemReferenceTypeBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_REFI);
		gf_list_add(meta->item_refs->references, ref);
		ref->reference_type = type;
		ref->from_item_id = from_id;
	}
	else {
		for (i = 0; i < ref->reference_count; i++) {
			if (ref->to_item_IDs[i] == to_id) {
				return GF_OK;
			}
		}
	}

	ref->to_item_IDs = (u32 *)gf_realloc(ref->to_item_IDs, (ref->reference_count + 1) * sizeof(u32));
	if (!ref->to_item_IDs) return GF_OUT_OF_MEM;
	ref->to_item_IDs[ref->reference_count] = to_id;
	ref->reference_count++;
	if (ref_index) {
		*ref_index = ref->reference_count;
	}
	return GF_OK;

}

#endif /*GPAC_DISABLE_ISOM*/
