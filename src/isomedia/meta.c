/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2020
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

	count = gf_list_count(meta->child_boxes);
	for (i=0; i<count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->child_boxes, i);
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
	count = gf_list_count(meta->child_boxes);
	for (i = 0; i <count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->child_boxes, i);
		if ((a->type == GF_ISOM_BOX_TYPE_XML) || (a->type == GF_ISOM_BOX_TYPE_BXML) ) {
			xml = (GF_XMLBox *)a;
			break;
		}
	}
	if (!xml || !xml->xml) return GF_BAD_PARAM;

	didfile = gf_fopen(outName, "wb");
	if (!didfile) return GF_IO_ERR;
	gf_fwrite(xml->xml, strlen(xml->xml), didfile);
	gf_fclose(didfile);

	if (is_binary) *is_binary = (xml->type==GF_ISOM_BOX_TYPE_BXML) ? 1 : 0;
	return GF_OK;
}

#if 0 //unused
GF_XMLBox *gf_isom_get_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool *is_binary)
{
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return NULL;

	/*Find XMLBox*/
	count = gf_list_count(meta->child_boxes);
	for (i = 0; i <count; i++) {
		GF_Box *a = (GF_Box *)gf_list_get(meta->child_boxes, i);
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
#endif


GF_EXPORT
u32 gf_isom_get_meta_item_count(GF_ISOFile *file, Bool root_meta, u32 track_num)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return 0;
	return gf_list_count(meta->item_infos->item_infos);
}

GF_EXPORT
GF_Err gf_isom_get_meta_item_info(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num,
                                  u32 *itemID, u32 *type, u32 *protection_scheme, u32 *protection_scheme_version, Bool *is_self_reference,
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
	if (item_name) (*item_name) = iinf->item_name;
	if (item_mime_type) (*item_mime_type) = iinf->content_type;
	if (item_encoding) (*item_encoding) = iinf->content_encoding;
	if (is_self_reference) *is_self_reference = 0;
	if (type) *type = iinf->item_type;

	if (item_url) (*item_url) = NULL;
	if (item_urn) (*item_urn) = NULL;


	if (iinf->item_protection_index) {
		GF_ProtectionSchemeInfoBox *sinf;
		if (!meta->protections) return GF_ISOM_INVALID_FILE;
		sinf = gf_list_get(meta->protections->protection_information, iinf->item_protection_index-1);
		if (!sinf) return GF_ISOM_INVALID_FILE;

		if (sinf->scheme_type) {
			if (protection_scheme) *protection_scheme = sinf->scheme_type->scheme_type;
			if (protection_scheme_version) *protection_scheme_version = sinf->scheme_type->scheme_version;
		} else {
			if (protection_scheme) *protection_scheme = GF_4CC('u','k','n','w');
			if (protection_scheme_version) *protection_scheme_version = 0;

		}
	} else {
		if (protection_scheme) *protection_scheme = 0;
		if (protection_scheme_version) *protection_scheme_version = 0;
	}

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (iloc->item_ID==iinf->item_ID) {
			if (iloc->data_reference_index) {
				GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->child_boxes, iloc->data_reference_index-1);
				if (!a) return GF_ISOM_INVALID_FILE;
				if (a->type==GF_ISOM_BOX_TYPE_URL) {
					if (item_url) (*item_url) = ((GF_DataEntryURLBox*)a)->location;
				} else if (a->type==GF_ISOM_BOX_TYPE_URN) {
					if (item_url) (*item_url) = ((GF_DataEntryURNBox*)a)->location;
					if (item_urn) (*item_urn) = ((GF_DataEntryURNBox*)a)->nameURN;
				}
				break;
			} else if (is_self_reference && !iloc->base_offset) {
				GF_ItemExtentEntry *entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
				if (!entry) return GF_ISOM_INVALID_FILE;

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
GF_Err gf_isom_get_meta_item_flags(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num)
{
	GF_ItemInfoEntryBox *iinf;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return 0;

	iinf = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, item_num-1);
	if (!iinf) return 0;
	return iinf->flags;
}

GF_EXPORT
u32 gf_isom_get_meta_item_by_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_ID)
{
	u32 i, count;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return 0;
	count = gf_list_count(meta->item_infos->item_infos);
	for (i=0; i<count; i++) {
		GF_ItemInfoEntryBox *iinf = (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
		if (iinf->item_ID==item_ID) return i+1;
	}
	return 0;
}

static GF_Err gf_isom_extract_meta_item_intern(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, const char *dump_file_name, u8 **out_data, u32 *out_size, u32 *out_alloc_size, const char **out_mime, Bool use_annex_b)
{
	GF_BitStream *item_bs;
	char szPath[1024];
	FILE *resource = NULL;
	u32 i, count;
	GF_Err e;
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

	if (!location_entry) return GF_BAD_PARAM;

	/* offsets are expressed from the start of the idat box instead of the start of the file */
	if (location_entry->construction_method == 1) {
		Bool found = GF_FALSE;

		count = gf_list_count(meta->child_boxes);
		for (i = 0; i <count; i++) {
			GF_Box *a = (GF_Box *)gf_list_get(meta->child_boxes, i);

			if (a->type == GF_ISOM_BOX_TYPE_IDAT) {
				GF_MediaDataBox *p = (GF_MediaDataBox *)a;
				idat_offset = p->bsOffset;
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[IsoMedia] Item %d references an inexistant idat box\n", item_num));
			return GF_BAD_PARAM;
		}
	}
	/* when construction_method==1, data_reference_index is ignored */
	/*FIXME*/
	else if (location_entry->data_reference_index) {
		char *item_url = NULL, *item_urn = NULL;
		GF_Box *a = (GF_Box *)gf_list_get(meta->file_locations->dref->child_boxes, location_entry->data_reference_index-1);
		if (!a) return GF_ISOM_INVALID_FILE;
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
		GF_ItemExtentEntry *extent_entry = (GF_ItemExtentEntry *)gf_list_get(location_entry->extent_entries, 0);
		if (!extent_entry->extent_length
#ifndef GPAC_DISABLE_ISOM_WRITE
		        && !extent_entry->original_extent_offset
#endif
		   ) return GF_BAD_PARAM;
	}

	item_bs = NULL;


	if (out_data) {
		item_bs = gf_bs_new(*out_data, *out_size, GF_BITSTREAM_WRITE_DYN);
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
		u32 j, nb_assoc;
		GF_HEVCConfigurationBox *hvcc = NULL;
		GF_AVCConfigurationBox *avcc = NULL;
		if (! meta->item_props) return GF_NON_COMPLIANT_BITSTREAM;
		if (! meta->item_props->property_container) {
			if (item_bs) gf_bs_del(item_bs);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (! meta->item_props->property_association) {
			if (item_bs) gf_bs_del(item_bs);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		nb_assoc = gf_list_count(meta->item_props->property_association->entries);
		for (i=0; i<nb_assoc; i++) {
			GF_ItemPropertyAssociationEntry *e = gf_list_get(meta->item_props->property_association->entries, i);
			if (e->item_id!=item_id) continue;
			for (j=0; j<e->nb_associations; j++) {
				u32 idx = e->associations[j].index;
				if (! idx) continue;
				hvcc = gf_list_get(meta->item_props->property_container->child_boxes, idx - 1);
				if (!hvcc) {
					if (item_bs) gf_bs_del(item_bs);
					return GF_NON_COMPLIANT_BITSTREAM;
				}
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

	e = GF_OK;
	for (i=0; i<count; i++) {
		char buf_cache[4096];
		u64 remain;
		GF_ItemExtentEntry *extent_entry = (GF_ItemExtentEntry *)gf_list_get(location_entry->extent_entries, i);
		gf_bs_seek(file->movieFileMap->bs, idat_offset + location_entry->base_offset + extent_entry->extent_offset);

		remain = extent_entry->extent_length;
		while (remain) {
			if (nalu_size_length) {
				if (remain < nalu_size_length) {
					e = GF_ISOM_INVALID_FILE;
					break;
				}

				u32 nal_size = gf_bs_read_int(file->movieFileMap->bs, 8*nalu_size_length);
				if (remain - nalu_size_length < nal_size) {
					e = GF_ISOM_INVALID_FILE;
					break;
				}

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
		gf_bs_get_content_no_truncate(item_bs, out_data, out_size, out_alloc_size);
	}
	gf_bs_del(item_bs);
	if (resource) {
		gf_fclose(resource);
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_extract_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, const char *dump_file_name)
{
	return gf_isom_extract_meta_item_intern(file, root_meta, track_num, item_id, dump_file_name, NULL, NULL, NULL, NULL, GF_TRUE);
}

GF_EXPORT
GF_Err gf_isom_extract_meta_item_mem(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, u8 **out_data, u32 *out_size, u32 *out_alloc_size, const char **out_mime, Bool use_annex_b)
{
	return gf_isom_extract_meta_item_intern(file, root_meta, track_num, item_id, NULL, out_data, out_size, out_alloc_size, out_mime, use_annex_b);
}

GF_EXPORT
GF_Err gf_isom_extract_meta_item_get_cenc_info(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, Bool *is_protected,
	u8 *skip_byte_block, u8 *crypt_byte_block, const u8 **key_info, u32 *key_info_size, u32 *aux_info_type_param,
	u8 **cenc_sai_data, u32 *cenc_sai_data_size, u32 *cenc_sai_alloc_size)
{
	u32 count, i;
	u32 j, sai_item_id;
	Bool found = GF_FALSE;
	GF_ItemPropertyAssociationBox *ipma = NULL;
	GF_ItemPropertyContainerBox *ipco = NULL;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	ipma = meta->item_props->property_association;
	ipco = meta->item_props->property_container;

	count = gf_list_count(ipma->entries);
	for (i = 0; i < count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
		if (entry->item_id != item_id) continue;
		for (j = 0; j < entry->nb_associations; j++) {
			GF_ItemEncryptionPropertyBox *ienc;
			u32 index = entry->associations[j].index;
			ienc = index ? (GF_ItemEncryptionPropertyBox *)gf_list_get(ipco->child_boxes, index - 1) : NULL;
			if (!ienc) continue;

			if (ienc->type!=GF_ISOM_BOX_TYPE_IENC) continue;
			if (ienc->key_info_size<19) return GF_ISOM_INVALID_FILE;
			
			*skip_byte_block = ienc->skip_byte_block;
			*crypt_byte_block = ienc->crypt_byte_block;
			*key_info = ienc->key_info;
			*key_info_size = ienc->key_info_size;
			found = GF_TRUE;
			break;
		}
		if (found) break;
	}
	if (!found) {
		*is_protected = GF_FALSE;
		return GF_OK;
	}
	*is_protected = GF_TRUE;
	sai_item_id = 0;

	//look for item reference
	if (!meta->item_refs)
		return GF_ISOM_INVALID_FILE;
	count = gf_list_count(meta->item_refs->references);
	for (i=0; i<count; i++) {
		GF_ItemReferenceTypeBox *iref = gf_list_get(meta->item_refs->references, i);
		if (iref->reference_type!=GF_ISOM_REF_AUXR) continue;
		sai_item_id = iref->to_item_IDs[0];
		break;
	}
	if (!sai_item_id) return GF_ISOM_INVALID_FILE;

	if (aux_info_type_param) {
		count = gf_list_count(ipma->entries);
		found = GF_FALSE;
		for (i = 0; i < count; i++) {
			GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
			if (entry->item_id != sai_item_id) continue;
			for (j = 0; j < entry->nb_associations; j++) {
				GF_AuxiliaryInfoPropertyBox *iaux;
				u32 index = entry->associations[j].index;
				iaux = index ? (GF_AuxiliaryInfoPropertyBox *)gf_list_get(ipco->child_boxes, index - 1) : NULL;
				if (!iaux) continue;

				if (iaux->type!=GF_ISOM_BOX_TYPE_IAUX) continue;
				switch (iaux->aux_info_type) {
				case GF_ISOM_CENC_SCHEME:
				case GF_ISOM_CENS_SCHEME:
				case GF_ISOM_CBC_SCHEME:
				case GF_ISOM_CBCS_SCHEME:
				case 0:
					break;
				default:
					continue;
				}
				*aux_info_type_param = iaux->aux_info_parameter;
				found = GF_TRUE;
				break;
			}
			if (found) break;
		}
	}
	if (!cenc_sai_data)
		return GF_OK;

	return gf_isom_extract_meta_item_mem(file, root_meta, track_num, sai_item_id, cenc_sai_data, cenc_sai_data_size, cenc_sai_alloc_size, NULL, GF_FALSE);
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
	char szName[40];
	GF_MetaBox *meta;

	GF_Err e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) {
		if (!metaType) return GF_OK;
		meta = (GF_MetaBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_META);
		if (!meta) return GF_OUT_OF_MEM;
		if (root_meta) {
			file->meta = meta;
			gf_list_add(file->TopBoxes, meta);
		} else {
			e = gf_isom_insert_moov(file);
			if (e) return e;
			if (!track_num) {
				file->moov->meta = meta;
				if (!file->moov->child_boxes) file->moov->child_boxes = gf_list_new();
				gf_list_add(file->moov->child_boxes, meta);
			} else {
				GF_TrackBox *tk = (GF_TrackBox *)gf_list_get(file->moov->trackList, track_num-1);
				if (!tk) {
					gf_isom_box_del((GF_Box *)meta);
					return GF_BAD_PARAM;
				}
				if (!tk->child_boxes) tk->child_boxes = gf_list_new();
				gf_list_add(tk->child_boxes, meta);
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
				gf_isom_box_del_parent(&file->moov->child_boxes, (GF_Box *)file->moov->meta);
				file->moov->meta = NULL;
			} else {
				GF_TrackBox *tk = (GF_TrackBox *)gf_list_get(file->moov->trackList, track_num-1);
				if (!tk) return GF_BAD_PARAM;
				gf_isom_box_del_parent(&tk->child_boxes, (GF_Box *)tk->meta);
				tk->meta = NULL;
			}
		}
		return GF_OK;
	}

	if (!meta->handler) {
		meta->handler = (GF_HandlerBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_HDLR);
		if (!meta->handler) return GF_OUT_OF_MEM;
	}
	if (meta->handler->nameUTF8) gf_free(meta->handler->nameUTF8);
	meta->handler->handlerType = metaType;
	sprintf(szName, "GPAC %s Handler", gf_4cc_to_str(metaType));
	meta->handler->nameUTF8 = gf_strdup(szName);
	if (!meta->handler->nameUTF8) return GF_OUT_OF_MEM;
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
	while ((a = (GF_Box*)gf_list_enum(meta->child_boxes, &i))) {
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_XML:
		case GF_ISOM_BOX_TYPE_BXML:
			gf_isom_box_del_parent(&meta->child_boxes, a);
			i--;
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *XMLFileName, unsigned char *data, u32 data_size, Bool IsBinaryXML)
{
	GF_Err e;
	GF_XMLBox *xml;
	GF_MetaBox *meta;
	u32 length;
	if (!XMLFileName && !data)
		return GF_BAD_PARAM;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	e = gf_isom_remove_meta_xml(file, root_meta, track_num);
	if (e) return e;

	xml = (GF_XMLBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_XML);
	if (!xml) return GF_OUT_OF_MEM;
	if (IsBinaryXML) xml->type = GF_ISOM_BOX_TYPE_BXML;

	if (XMLFileName) {
		/*assume 32bit max size = 4Go should be sufficient for a DID!!*/
		return gf_file_load_data(XMLFileName, (u8 **) &xml->xml, &length);
	}

	/*assume 32bit max size = 4Go should be sufficient for a DID!!*/
	xml->xml = (char*)gf_malloc(sizeof(unsigned char)*data_size);
	if (!xml->xml) return GF_OUT_OF_MEM;
	memcpy(xml->xml, data, sizeof(unsigned char)*data_size);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_get_meta_image_props(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, GF_ImageItemProperties *prop) {
	u32 count, i, inum;
	u32 j;
	GF_ItemPropertyAssociationBox *ipma = NULL;
	GF_ItemPropertyContainerBox *ipco = NULL;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;

	memset(prop, 0, sizeof(GF_ImageItemProperties));
	if (!meta->item_props) return GF_OK;

	ipma = meta->item_props->property_association;
	ipco = meta->item_props->property_container;

	inum = gf_isom_get_meta_item_by_id(file, root_meta, track_num, item_id);
	i = gf_isom_get_meta_item_flags(file, root_meta, track_num, inum);
	if (i & 0x1)
		prop->hidden = GF_TRUE;

	count = gf_list_count(ipma->entries);
	for (i = 0; i < count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
		if (entry->item_id != item_id) continue;
		for (j = 0; j < entry->nb_associations; j++) {
			GF_Box *b;
			u32 index = entry->associations[j].index;
			b = index ? (GF_Box *)gf_list_get(ipco->child_boxes, index - 1) : NULL;
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
				u32 k;
				GF_PixelInformationPropertyBox *pixi = (GF_PixelInformationPropertyBox *)b;
				if (pixi->num_channels > 3) {
					return GF_BAD_PARAM;
				}
				prop->num_channels = pixi->num_channels;
				for(k = 0; k < 3; k++) {
					if (k < pixi->num_channels) {
						prop->bits_per_channel[k] = pixi->bits_per_channel[k];
					} else {
						prop->bits_per_channel[k] = 0;
					}
				}
			}
			break;
			case GF_ISOM_BOX_TYPE_IROT:
			{
				GF_ImageRotationBox *irot = (GF_ImageRotationBox *)b;
				prop->angle = irot->angle * 90;
			}
			break;
			case GF_ISOM_BOX_TYPE_IMIR:
			{
				GF_ImageMirrorBox *imir = (GF_ImageMirrorBox *)b;
				prop->mirror = imir->axis+1;
			}
			break;
			case GF_ISOM_BOX_TYPE_CLAP:
			{
				GF_CleanApertureBox *clap = (GF_CleanApertureBox *)b;
				prop->clap_hden = clap->cleanApertureHeightD;
				prop->clap_hnum = clap->cleanApertureHeightN;
				prop->clap_wden = clap->cleanApertureWidthD;
				prop->clap_wnum = clap->cleanApertureWidthN;
				prop->clap_hoden = clap->horizOffD;
				prop->clap_honum = clap->horizOffN;
				prop->clap_voden = clap->vertOffD;
				prop->clap_vonum = clap->vertOffN;
			}
			break;
			case GF_ISOM_BOX_TYPE_A1LX:
			{
				GF_AV1LayeredImageIndexingPropertyBox *a1lx = (GF_AV1LayeredImageIndexingPropertyBox *)b;
				memcpy(prop->av1_layer_size, a1lx->layer_size, sizeof(prop->av1_layer_size));
			}
			break;
			case GF_ISOM_BOX_TYPE_A1OP:
			{
				GF_AV1OperatingPointSelectorPropertyBox *a1op = (GF_AV1OperatingPointSelectorPropertyBox *)b;
				prop->av1_op_index = a1op->op_index;
			}
			case GF_ISOM_BOX_TYPE_HVCC:
			case GF_ISOM_BOX_TYPE_AVCC:
			case GF_ISOM_BOX_TYPE_AV1C:
			case GF_ISOM_BOX_TYPE_VVCC:
				prop->config = b;
				break;
			}
		}
	}
	return GF_OK;
}

static s32 meta_find_prop(GF_ItemPropertyContainerBox *boxes, GF_ImageItemProperties *prop) {
	u32 i;
	u32 count;
	count = gf_list_count(boxes->child_boxes);
	for (i = 0; i < count; i++) {
		GF_Box *b = (GF_Box *)gf_list_get(boxes->child_boxes, i);
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
		case GF_ISOM_BOX_TYPE_IMIR:
		{
			GF_ImageMirrorBox *imir = (GF_ImageMirrorBox *)b;
			if (prop->mirror && imir->axis == prop->mirror-1) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_CLAP:
		{
			GF_CleanApertureBox *clap = (GF_CleanApertureBox *)b;
			if (prop->clap_hden == clap->cleanApertureHeightD &&
				prop->clap_hnum == clap->cleanApertureHeightN &&
				prop->clap_wden == clap->cleanApertureWidthD &&
				prop->clap_wnum == clap->cleanApertureWidthN &&
				prop->clap_hoden == clap->horizOffD &&
				prop->clap_honum == clap->horizOffN &&
				prop->clap_voden == clap->vertOffD &&
				prop->clap_vonum == clap->vertOffN) {
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
		case GF_ISOM_BOX_TYPE_IENC:
		{
			GF_ItemEncryptionPropertyBox *ienc = (GF_ItemEncryptionPropertyBox *)b;
			if (prop->cenc_info
				&& (prop->cenc_info->skip_byte_block == ienc->skip_byte_block)
				&& (prop->cenc_info->crypt_byte_block == ienc->crypt_byte_block)
				&& (prop->cenc_info->key_info_size == ienc->key_info_size)
				&& prop->cenc_info->key_info && ienc->key_info
				&& !memcmp(prop->cenc_info->key_info, ienc->key_info, ienc->key_info_size)
			) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_A1LX:
		{
			GF_AV1LayeredImageIndexingPropertyBox *a1lx = (GF_AV1LayeredImageIndexingPropertyBox *)b;
			if (memcmp(prop->av1_layer_size, a1lx->layer_size, sizeof(prop->av1_layer_size)) == 0) {
				return i;
			}
		}
		break;
		case GF_ISOM_BOX_TYPE_A1OP:
		{
			GF_AV1OperatingPointSelectorPropertyBox *a1op = (GF_AV1OperatingPointSelectorPropertyBox *)b;
			if (prop->av1_op_index == a1op->op_index) {
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

static GF_Err meta_add_item_property_association(GF_ItemPropertyAssociationBox *ipma, u32 item_ID, u32 prop_index, Bool essential) {
	u32 i, count;
	GF_ItemPropertyAssociationEntry *found_entry = NULL;

	count = gf_list_count(ipma->entries);
	for (i = 0; i < count; i++) {
		found_entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ipma->entries, i);
		if (found_entry->item_id == item_ID) break;
		found_entry = NULL;
	}
	if (!found_entry) {
		GF_SAFEALLOC(found_entry, GF_ItemPropertyAssociationEntry);
		if (!found_entry) return GF_OUT_OF_MEM;
		gf_list_add(ipma->entries, found_entry);
		found_entry->item_id = item_ID;
	}
	found_entry->associations = gf_realloc(found_entry->associations, sizeof(GF_ItemPropertyAssociationSlot) * (found_entry->nb_associations+1));
	if (!found_entry->associations) return GF_OUT_OF_MEM;

	found_entry->associations[found_entry->nb_associations].essential = essential;
	found_entry->associations[found_entry->nb_associations].index = prop_index;
	found_entry->nb_associations++;
	return GF_OK;
}

static GF_Err meta_process_image_properties(GF_MetaBox *meta, u32 item_ID, GF_ImageItemProperties *image_props)
{
	GF_ImageItemProperties searchprop;
	GF_ItemPropertyAssociationBox *ipma;
	GF_ItemPropertyContainerBox *ipco;
	s32 prop_index;
	GF_Err e;
	memset(&searchprop, 0, sizeof(GF_ImageItemProperties));

	if (!meta->item_props) {
		meta->item_props = (GF_ItemPropertiesBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_IPRP);
		if (!meta->item_props) return GF_OUT_OF_MEM;
		meta->item_props->property_container = (GF_ItemPropertyContainerBox *)gf_isom_box_new_parent(&meta->item_props->child_boxes, GF_ISOM_BOX_TYPE_IPCO);
		if (!meta->item_props->property_container) return GF_OUT_OF_MEM;
		ipco = meta->item_props->property_container;
		ipma = (GF_ItemPropertyAssociationBox *)gf_isom_box_new_parent(&meta->item_props->child_boxes, GF_ISOM_BOX_TYPE_IPMA);
		if (!ipma) return GF_OUT_OF_MEM;
		meta->item_props->property_association = ipma;
	} else {
		ipco = meta->item_props->property_container;
		ipma = meta->item_props->property_association;
	}
	if (!ipco->child_boxes)
		ipco->child_boxes = gf_list_new();

	if (strlen(image_props->iccPath) > 0) {
		u32 size;
		u8 *data;
		e = gf_file_load_data(image_props->iccPath, &data, &size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error opening ICC colour profile file at %s\n", &image_props->iccPath));

		} else {

			GF_ColourInformationBox *colr = (GF_ColourInformationBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_COLR);
			if (!colr) return GF_OUT_OF_MEM;
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] reading ICC colour profile from file %s\n", &image_props->iccPath));
			colr->colour_type = GF_ISOM_SUBTYPE_PROF;
			colr->opaque_size = size;
			colr->opaque = data;

			prop_index = gf_list_count(ipco->child_boxes) - 1;
			meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		}
	}

	if (image_props->width || image_props->height) {
		searchprop.width = image_props->width;
		searchprop.height = image_props->height;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_ImageSpatialExtentsPropertyBox *ispe = (GF_ImageSpatialExtentsPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_ISPE);
			if (!ispe) return GF_OUT_OF_MEM;
			ispe->image_width = image_props->width;
			ispe->image_height = image_props->height;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		if (e) return e;
		searchprop.width = 0;
		searchprop.height = 0;
	}
	if (image_props->hOffset || image_props->vOffset) {
		searchprop.hOffset = image_props->hOffset;
		searchprop.vOffset = image_props->vOffset;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_RelativeLocationPropertyBox *rloc = (GF_RelativeLocationPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_RLOC);
			if (!rloc) return GF_OUT_OF_MEM;
			rloc->horizontal_offset = image_props->hOffset;
			rloc->vertical_offset = image_props->vOffset;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.hOffset = 0;
		searchprop.vOffset = 0;
	}
	if (image_props->hSpacing || image_props->vSpacing) {
		searchprop.hSpacing = image_props->hSpacing;
		searchprop.vSpacing = image_props->vSpacing;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_PASP);
			if (!pasp) return GF_OUT_OF_MEM;
			pasp->hSpacing = image_props->hSpacing;
			pasp->vSpacing = image_props->vSpacing;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		if (e) return e;
		searchprop.hSpacing = 0;
		searchprop.vSpacing = 0;
	}
	if (image_props->clap_wnum || image_props->clap_wden || image_props->clap_hnum || image_props->clap_hden || image_props->clap_honum || image_props->clap_hoden || image_props->clap_vonum || image_props->clap_voden) {
		searchprop.clap_wnum = image_props->clap_wnum;
		searchprop.clap_wden = image_props->clap_wden;
		searchprop.clap_hnum = image_props->clap_hnum;
		searchprop.clap_hden = image_props->clap_hden;
		searchprop.clap_honum = image_props->clap_honum;
		searchprop.clap_hoden = image_props->clap_hoden;
		searchprop.clap_vonum = image_props->clap_vonum;
		searchprop.clap_voden = image_props->clap_voden;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_CleanApertureBox *clap = (GF_CleanApertureBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_CLAP);
			if (!clap) return GF_OUT_OF_MEM;
			clap->cleanApertureHeightD = image_props->clap_hden;
			clap->cleanApertureHeightN = image_props->clap_hnum;
			clap->cleanApertureWidthD = image_props->clap_wden;
			clap->cleanApertureWidthN = image_props->clap_wnum;
			clap->horizOffD = image_props->clap_hoden;
			clap->horizOffN = image_props->clap_honum;
			clap->vertOffD = image_props->clap_voden;
			clap->vertOffN = image_props->clap_vonum;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.clap_wnum = searchprop.clap_wden = searchprop.clap_hnum = searchprop.clap_hden = searchprop.clap_honum = searchprop.clap_hoden = searchprop.clap_vonum = searchprop.clap_voden = 0;
	}
	if (image_props->angle) {
		searchprop.angle = image_props->angle;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_ImageRotationBox *irot = (GF_ImageRotationBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_IROT);
			if (!irot) return GF_OUT_OF_MEM;
			irot->angle = image_props->angle/90;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.angle = 0;
	}
	if (image_props->mirror) {
		searchprop.mirror = image_props->mirror;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_ImageMirrorBox *imir = (GF_ImageMirrorBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_IMIR);
			if (!imir) return GF_OUT_OF_MEM;
			imir->axis = image_props->mirror-1;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.mirror = 0;
	}
	if (image_props->config) {
		searchprop.config = image_props->config;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			gf_list_add(ipco->child_boxes, gf_isom_clone_config_box(image_props->config));
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.config = NULL;
	}
	if (image_props->alpha) {
		searchprop.alpha = image_props->alpha;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_AuxiliaryTypePropertyBox *auxC = (GF_AuxiliaryTypePropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_AUXC);
			if (!auxC) return GF_OUT_OF_MEM;
			auxC->aux_urn = gf_strdup("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha");
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.alpha = GF_FALSE;
	}
	if (image_props->depth) {
		searchprop.depth = image_props->depth;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_AuxiliaryTypePropertyBox *auxC = (GF_AuxiliaryTypePropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_AUXC);
			if (!auxC) return GF_OUT_OF_MEM;
			auxC->aux_urn = gf_strdup("urn:mpeg:mpegB:cicp:systems:auxiliary:depth");
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.alpha = GF_FALSE;
	}
	if (image_props->num_channels) {
		u32 k;
		searchprop.num_channels = image_props->num_channels;
		for (k=0; k<3; k++) {
			searchprop.bits_per_channel[k] = image_props->bits_per_channel[k];
		}
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_PixelInformationPropertyBox *pixi = (GF_PixelInformationPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_PIXI);
			if (!pixi) return GF_OUT_OF_MEM;
			pixi->num_channels = image_props->num_channels;
			pixi->bits_per_channel = gf_malloc(pixi->num_channels);
			if (!pixi->bits_per_channel) return GF_OUT_OF_MEM;
			for (k=0; k<pixi->num_channels; k++) {
				pixi->bits_per_channel[k] = image_props->bits_per_channel[k];
			}
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.num_channels = 0;
	}
	if ((image_props->av1_layer_size[0] != image_props->av1_layer_size[1]) ||
		(image_props->av1_layer_size[1] != image_props->av1_layer_size[2]) ||
		(image_props->av1_layer_size[0] != image_props->av1_layer_size[2])) {
		memcpy(searchprop.av1_layer_size, image_props->av1_layer_size, sizeof(searchprop.av1_layer_size));
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_AV1LayeredImageIndexingPropertyBox *a1lx = (GF_AV1LayeredImageIndexingPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_A1LX);
			if (!a1lx) return GF_OUT_OF_MEM;
			a1lx->large_size = 1;
			memcpy(a1lx->layer_size, image_props->av1_layer_size, sizeof(a1lx->layer_size));
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_FALSE);
		if (e) return e;
		memset(searchprop.av1_layer_size, 0, sizeof(searchprop.av1_layer_size));
	}
	if (image_props->av1_op_index) {
		searchprop.av1_op_index = image_props->av1_op_index;
		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			GF_AV1OperatingPointSelectorPropertyBox *a1op = (GF_AV1OperatingPointSelectorPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_A1OP);
			if (!a1op) return GF_OUT_OF_MEM;
			a1op->op_index = image_props->av1_op_index;
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.av1_op_index = 0;
	}

	if (image_props->cenc_info) {
		GF_ItemEncryptionPropertyBox *ienc = NULL;

		if (!gf_cenc_validate_key_info(image_props->cenc_info->key_info, image_props->cenc_info->key_info_size))
			return GF_BAD_PARAM;

		searchprop.cenc_info = image_props->cenc_info;

		prop_index = meta_find_prop(ipco, &searchprop);
		if (prop_index < 0) {
			ienc = (GF_ItemEncryptionPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_IENC);
			if (!ienc) return GF_OUT_OF_MEM;
			ienc->skip_byte_block = image_props->cenc_info->skip_byte_block;
			ienc->crypt_byte_block = image_props->cenc_info->crypt_byte_block;
			ienc->key_info_size = image_props->cenc_info->key_info_size;
			ienc->key_info = gf_malloc(sizeof(u8) * image_props->cenc_info->key_info_size);
			if (!ienc->key_info) {
				gf_free(ienc);
				return GF_OUT_OF_MEM;
			}
			memcpy(ienc->key_info, image_props->cenc_info->key_info, image_props->cenc_info->key_info_size);
			prop_index = gf_list_count(ipco->child_boxes) - 1;
		}

		//add property
		e = meta_add_item_property_association(ipma, item_ID, prop_index + 1, GF_TRUE);
		if (e) return e;
		searchprop.cenc_info = NULL;
	}

	return GF_OK;
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
	if (meta->groups_list) {
		u32 i;
		u32 groups_count = gf_list_count(meta->groups_list->child_boxes);
		for (i = 0; i < groups_count; i++) {
			GF_EntityToGroupTypeBox *g = (GF_EntityToGroupTypeBox *)gf_list_get(meta->groups_list->child_boxes, i);
			if (g->group_id > lastItemID) lastItemID = g->group_id;
		}
		*item_id = lastItemID+1;
	}
	return GF_OK;
}

GF_Err gf_isom_add_meta_item_extended(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path,
                                      const char *item_name, u32 *io_item_id, u32 item_type, const char *mime_type, const char *content_encoding,
                                      GF_ImageItemProperties *image_props,
                                      const char *URL, const char *URN,
                                      char *data, u32 data_len, GF_List *item_extent_refs, u32 tk_id, u32 sample_num)
{
	u32 i;
	GF_Err e;
	GF_ItemLocationEntry *location_entry;
	GF_ItemInfoEntryBox *infe;
	GF_MetaBox *meta;
	u32 lastItemID = 0;
	u32 item_id = io_item_id ? *io_item_id : 0;

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
			GF_ItemInfoEntryBox *iinf_e= (GF_ItemInfoEntryBox *)gf_list_get(meta->item_infos->item_infos, i);
			if (iinf_e->item_ID > lastItemID) lastItemID = iinf_e->item_ID;
			if (item_id == iinf_e->item_ID) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[IsoMedia] Item with id %d already exists, ignoring id\n", item_id));
				item_id = 0;
			}
		}
	} else {
		meta->item_infos = (GF_ItemInfoBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_IINF);
	}

	/*Creation an ItemLocation Box if it does not exist*/
	if (!meta->item_locations)
		meta->item_locations = (GF_ItemLocationBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_ILOC);

	infe = (GF_ItemInfoEntryBox *)gf_isom_box_new_parent(&meta->item_infos->child_boxes, GF_ISOM_BOX_TYPE_INFE);
	if (!infe) return GF_OUT_OF_MEM;

	if (item_id) {
		infe->item_ID = item_id;
	} else {
		infe->item_ID = ++lastItemID;
	}
	if (io_item_id) *io_item_id = infe->item_ID;
	
	if (tk_id && sample_num) {
		data_len = gf_isom_get_sample_size(file, tk_id, sample_num);
		if (item_name)
			infe->item_name = gf_strdup(item_name);
	}
	else if (!tk_id && sample_num) {
		data_len = 0;
		if (item_name)
			infe->item_name = gf_strdup(item_name);
	}
	/*get relative name*/
	else if (item_name) {
		infe->item_name = gf_strdup(item_name);
		file->no_inplace_rewrite = GF_TRUE;
	} else if (resource_path) {
		infe->item_name = gf_strdup(gf_file_basename( resource_path ));
		file->no_inplace_rewrite = GF_TRUE;
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
		gf_isom_box_del_parent(&meta->item_infos->child_boxes, (GF_Box *)infe);
		return GF_OUT_OF_MEM;
	}
	memset(location_entry, 0, sizeof(GF_ItemLocationEntry));
	location_entry->extent_entries = gf_list_new();

	/*Creates an mdat if it does not exist*/
	if (!file->mdat) {
		file->mdat = (GF_MediaDataBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
		if (!file->mdat) return GF_OUT_OF_MEM;
		gf_list_add(file->TopBoxes, file->mdat);
	}

	gf_list_add(meta->item_locations->location_entries, location_entry);
	location_entry->item_ID = infe->item_ID;

	e = gf_list_add(meta->item_infos->item_infos, infe);
	if (e) return e;

	if (image_props) {
		if (image_props->hidden) {
			infe->flags = 0x1;
		}
		e = meta_process_image_properties(meta, infe->item_ID, image_props);
		if (e) return e;

		if (image_props->cenc_info) {
			GF_ProtectionSchemeInfoBox *sinf;
			u32 cenc_item_id = infe->item_ID + 1;

			//create a new auxi item
			e = gf_isom_add_meta_item_memory(file, root_meta, track_num, NULL, &cenc_item_id, GF_ISOM_ITEM_TYPE_AUXI, NULL, NULL, NULL, (u8 *) image_props->cenc_info->sai_data, image_props->cenc_info->sai_data_size, NULL);
			if (e) return e;
			//add item reference
			e = gf_isom_meta_add_item_ref(file, root_meta, track_num, infe->item_ID, cenc_item_id, GF_ISOM_REF_AUXR, NULL);
			if (e) return e;

			//multikey, we MUST have a 'iaux' prop with aux_info_type_param=1 associated
			if (image_props->cenc_info->key_info[0]) {
				GF_ItemPropertyContainerBox *ipco = meta->item_props->property_container;
				GF_ItemPropertyAssociationBox *ipma = meta->item_props->property_association;
				u32 k, pcount = gf_list_count(ipco->child_boxes);
				s32 prop_index = -1;
				for (k=0; k<pcount; k++) {
					GF_AuxiliaryInfoPropertyBox *b = (GF_AuxiliaryInfoPropertyBox*)gf_list_get(ipco->child_boxes, k);
					if (b->type != GF_ISOM_BOX_TYPE_IAUX) continue;
					switch (b->aux_info_type) {
					case GF_ISOM_CENC_SCHEME:
					case GF_ISOM_CENS_SCHEME:
					case GF_ISOM_CBC_SCHEME:
					case GF_ISOM_CBCS_SCHEME:
					case 0:
						break;
					default:
						continue;
					}
					if (b->aux_info_parameter!=1) continue;
					prop_index = k;
					break;
				}

				if (prop_index < 0) {
					GF_AuxiliaryInfoPropertyBox *iaux = (GF_AuxiliaryInfoPropertyBox *)gf_isom_box_new_parent(&ipco->child_boxes, GF_ISOM_BOX_TYPE_IAUX);
					if (!iaux) return GF_OUT_OF_MEM;
					iaux->aux_info_parameter = 1;
					prop_index = gf_list_count(ipco->child_boxes) - 1;
				}

				//add property
				e = meta_add_item_property_association(ipma, cenc_item_id, prop_index + 1, GF_TRUE);
				if (e) return e;
			}

			//look for scheme in ipro
			if (!meta->protections) {
				meta->protections = (GF_ItemProtectionBox *) gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_IPRO);
				if (!meta->protections) return GF_OUT_OF_MEM;
			}
			sinf = NULL;
			for (i=0; i<gf_list_count(meta->protections->protection_information); i++) {
				sinf = gf_list_get(meta->protections->protection_information, i);
				if (sinf->scheme_type && (sinf->scheme_type->scheme_type==image_props->cenc_info->scheme_type)
					&& (sinf->scheme_type->scheme_version==image_props->cenc_info->scheme_version))
					break;
				sinf = NULL;
			}
			if (!sinf) {
				sinf = (GF_ProtectionSchemeInfoBox *)gf_isom_box_new_parent(&meta->protections->child_boxes, GF_ISOM_BOX_TYPE_SINF);
				if (!sinf) return GF_OUT_OF_MEM;
				gf_list_add(meta->protections->protection_information, sinf);
				sinf->scheme_type = (GF_SchemeTypeBox *)gf_isom_box_new_parent(&sinf->child_boxes, GF_ISOM_BOX_TYPE_SCHM);
				if (!sinf->scheme_type) return GF_OUT_OF_MEM;
				sinf->scheme_type->scheme_type = image_props->cenc_info->scheme_type;
				sinf->scheme_type->scheme_version = image_props->cenc_info->scheme_version;
			}
			infe->item_protection_index = 1 + gf_list_find(meta->protections->protection_information, sinf);
		}
	}

	/*0: the current file*/
	location_entry->data_reference_index = 0;
	if (self_reference) {
		GF_ItemExtentEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemExtentEntry);
		if (!entry) return GF_OUT_OF_MEM;
		gf_list_add(location_entry->extent_entries, entry);
		if (!infe->item_name) infe->item_name = gf_strdup("");
		return GF_OK;
	}

	/*file not copied, just referenced*/
	if (URL || URN) {
		u32 dataRefIndex;
		if (!meta->file_locations) {
			meta->file_locations = (GF_DataInformationBox *) gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_DINF);
			if (!meta->file_locations) return GF_OUT_OF_MEM;
		}
		if (!meta->file_locations->dref) {
			meta->file_locations->dref = (GF_DataReferenceBox *) gf_isom_box_new_parent(&meta->file_locations->child_boxes, GF_ISOM_BOX_TYPE_DREF);
			if (!meta->file_locations->dref) return GF_OUT_OF_MEM;
		}

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
			if (!entry) return GF_OUT_OF_MEM;
			gf_list_add(location_entry->extent_entries, entry);
			item_index = (u32 *)gf_list_get(item_extent_refs, i);
			gf_isom_meta_add_item_ref(file, root_meta, track_num, infe->item_ID, *item_index, GF_ISOM_REF_ILOC, &(entry->extent_index));
		}
	}
	else if (tk_id && sample_num) {
		if ((file->openMode == GF_ISOM_OPEN_WRITE) || (file->openMode == GF_ISOM_OPEN_EDIT)) {
			GF_ItemExtentEntry *entry;
			GF_SAFEALLOC(entry, GF_ItemExtentEntry);
			if (!entry) return GF_OUT_OF_MEM;

			entry->extent_length = data_len;
			location_entry->base_offset = 0;
			GF_ISOSample *samp = gf_isom_get_sample_info(file, tk_id, sample_num, NULL, &entry->extent_offset);
			if (samp) gf_isom_sample_del(&samp);
			gf_list_add(location_entry->extent_entries, entry);

			if (data_len>0xFFFFFFFF) meta->item_locations->length_size = 8;
			else if (! meta->item_locations->base_offset_size) meta->item_locations->length_size = 4;

			//for in-place rewrite + add-image
			if (file->openMode == GF_ISOM_OPEN_EDIT) {
				location_entry->base_offset = 0;
				infe->tk_id = tk_id;
				infe->sample_num = sample_num;
				infe->data_len = data_len;
			}
		} else {
			infe->tk_id = tk_id;
			infe->sample_num = sample_num;
			infe->data_len = data_len;
			file->no_inplace_rewrite = GF_TRUE;
		}
		meta->use_item_sample_sharing = 1;
	}
	else if (!tk_id && sample_num) {
		GF_ItemExtentEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemExtentEntry);
		if (!entry) return GF_OUT_OF_MEM;

		entry->extent_length = data_len;
		gf_list_add(location_entry->extent_entries, entry);
		infe->ref_it_id = sample_num;
		infe->data_len = data_len;
		meta->use_item_item_sharing = 1;
	}
	else if (data || resource_path){
		/*capture mode, write to disk*/
		if ((file->openMode == GF_ISOM_OPEN_WRITE) && !location_entry->data_reference_index) {
			FILE *src;
			GF_ItemExtentEntry *entry;
			GF_SAFEALLOC(entry, GF_ItemExtentEntry);
			if (!entry) return GF_OUT_OF_MEM;

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
					entry->extent_length = gf_fsize(src);

					remain = entry->extent_length;
					while (remain) {
						u32 size_cache = (remain > 4096) ? 4096 : (u32)remain;
						size_t read = gf_fread(cache_data, size_cache, src);
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
				if (!infe->full_path) return GF_OUT_OF_MEM;
				
				memcpy(infe->full_path, data, sizeof(char) * data_len);
				infe->data_len = data_len;
			}
			else if (resource_path) {
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
	return gf_isom_add_meta_item_extended(file, root_meta, track_num, self_reference, resource_path, item_name, &item_id, item_type, mime_type, content_encoding, image_props, URL, URN, NULL, 0, NULL, 0, 0);
}

GF_EXPORT
GF_Err gf_isom_add_meta_item_memory(GF_ISOFile *file, Bool root_meta, u32 track_num, const char *item_name, u32 *item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, char *data, u32 data_len, GF_List *item_extent_refs)
{
	return gf_isom_add_meta_item_extended(file, root_meta, track_num, GF_FALSE, NULL, item_name, item_id, item_type, mime_type, content_encoding, image_props, NULL, NULL, data, data_len, item_extent_refs, 0, 0);
}

GF_EXPORT
GF_Err gf_isom_add_meta_item_sample_ref(GF_ISOFile *file, Bool root_meta, u32 track_num, const char *item_name, u32 *item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, u32 tk_id, u32 sample_num)
{
	return gf_isom_add_meta_item_extended(file, root_meta, track_num, GF_FALSE, NULL, item_name, item_id, item_type, mime_type, content_encoding, image_props, NULL, NULL, NULL, 0, NULL, tk_id, sample_num);
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
	if (!item_num) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: No item with ID %d in file, cannnot remove\n"));
		return GF_NOT_FOUND;
	}
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

	if (meta->item_props && meta->item_props->property_association) {
		GF_ItemPropertyAssociationBox *ipma = meta->item_props->property_association;
		count = gf_list_count(ipma->entries);
		for (i=0; i<count; i++) {
			GF_ItemPropertyAssociationEntry *pa_ent = gf_list_get(ipma->entries, i);
			if (pa_ent->item_id == iinf->item_ID) {
				gf_free(pa_ent->associations);
				gf_free(pa_ent);
				gf_list_rem(ipma->entries, i);
				break;
			}
		}
	}

	//rewrite item sharing ids
	if (meta->use_item_item_sharing) {
		u32 new_ref_id = 0;
		count = gf_list_count(meta->item_infos->item_infos);
		for (i=0; i<count; i++) {
			GF_ItemInfoEntryBox *iinf2 = (GF_ItemInfoEntryBox *) gf_list_get(meta->item_infos->item_infos, i);
			if (iinf2->ref_it_id != iinf->item_ID) continue;

			if (new_ref_id) {
				iinf2->ref_it_id = new_ref_id;
			} else {
				new_ref_id = iinf2->item_ID;
				iinf2->ref_it_id = 0;
				if (iinf->tk_id) {
					iinf2->tk_id = iinf->tk_id;
					iinf2->sample_num = iinf->sample_num;
					iinf2->data_len = iinf->data_len;
				}
			}
		}
	}

	gf_isom_box_del_parent(&meta->item_infos->child_boxes, (GF_Box *)iinf);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_meta_primary_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id)
{
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !meta->item_infos || !meta->item_locations) return GF_BAD_PARAM;
	/*either one or the other*/
	if (gf_isom_has_meta_xml(file, root_meta, track_num)) return GF_BAD_PARAM;

	if (meta->primary_resource) gf_isom_box_del_parent(&meta->child_boxes, (GF_Box*)meta->primary_resource);
	meta->primary_resource = (GF_PrimaryItemBox*) gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_PITM);
	if (!meta->primary_resource) return GF_OUT_OF_MEM;
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
		meta->item_refs = (GF_ItemReferenceBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_IREF);
		if (!meta->item_refs) return GF_OUT_OF_MEM;
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
		ref = (GF_ItemReferenceTypeBox *)gf_isom_box_new_parent(&meta->item_refs->child_boxes, GF_ISOM_BOX_TYPE_REFI);
		if (!ref) return GF_OUT_OF_MEM;
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

void gf_isom_meta_restore_items_ref(GF_ISOFile *movie, GF_MetaBox *meta)
{
	u32 i, nb_items, nb_tracks;
	if (!meta->item_locations || !meta->item_infos) return;
	nb_tracks = movie->moov ? gf_list_count(movie->moov->trackList) : 0;
	nb_items = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<nb_items; i++) {
		u32 j;
		u64 item_offset;
		GF_ItemExtentEntry *entry;
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		/*get item info*/
		GF_ItemInfoEntryBox *iinf = NULL;
		j=0;
		while ((iinf = (GF_ItemInfoEntryBox *)gf_list_enum(meta->item_infos->item_infos, &j))) {
			if (iinf->item_ID==iloc->item_ID) break;
			iinf = NULL;
		}
		if (!iinf) continue;
		if (iinf->ref_it_id) continue;

		if (gf_list_count(iloc->extent_entries) != 1) continue;
		entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
		if (!entry) continue;
		item_offset = iloc->base_offset + entry->extent_offset;

		for (j=0;j<nb_tracks; j++) {
			GF_TrackBox *trak;
			GF_SampleSizeBox *stsz;
			u32 k;
			trak = gf_list_get(movie->moov->trackList, j);
			if (! gf_isom_is_video_handler_type(trak->Media->handler->handlerType))
				continue;

			stsz = trak->Media->information->sampleTable->SampleSize;
			if (!stsz->sampleCount) continue;
			for (k=0; k<stsz->sampleCount; k++) {
				GF_Err e;
				u32 chunk, di, samp_size;
				u64 samp_offset;
				samp_size = stsz->sampleSize ? stsz->sampleSize : stsz->sizes[k];
				if (samp_size != entry->extent_length)
					continue;

				e = stbl_GetSampleInfos(trak->Media->information->sampleTable, k+1, &samp_offset, &chunk, &di, NULL);
				if (e) continue;
				if (samp_offset == item_offset) {
					iinf->tk_id = trak->Header->trackID;
					iinf->sample_num = k+1;
					iinf->data_len = (u32) entry->extent_length;
					meta->use_item_sample_sharing = 1;
					break;
				}
			}
			if (iinf->tk_id) break;
		}

		//restore all item refs
		for (j=i+1;j<nb_items; j++) {
			u32 k;
			u64 item_offset2;
			GF_ItemExtentEntry *entry2;
			GF_ItemLocationEntry *iloc2 = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, j);
			/*get item info*/
			GF_ItemInfoEntryBox *iinf2 = NULL;
			k=0;
			while ((iinf2 = (GF_ItemInfoEntryBox *)gf_list_enum(meta->item_infos->item_infos, &k))) {
				if (iinf2->item_ID==iloc2->item_ID) break;
				iinf2 = NULL;
			}
			if (!iinf2) continue;
			if (gf_list_count(iloc2->extent_entries) != 1) continue;
			entry2 = (GF_ItemExtentEntry *)gf_list_get(iloc2->extent_entries, 0);
			if (!entry2) continue;
			item_offset2 = iloc2->base_offset + entry2->extent_offset;

			if (item_offset == item_offset2) {
				iinf2->ref_it_id = iinf->item_ID;
				meta->use_item_item_sharing = 1;
			}
		}
	}
}

GF_EXPORT
GF_Err gf_isom_meta_add_item_group(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, u32 group_id, u32 group_type)
{
	u32 i, count;
	GF_EntityToGroupTypeBox *group;
	s32 index = -1;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta) return GF_BAD_PARAM;
	if (!group_type) return GF_BAD_PARAM;
	if (!group_id) {
		GF_Err e = gf_isom_meta_get_next_item_id(file, root_meta, track_num, &group_id);
		if (e != GF_OK) return e;
	}
	if (!meta->groups_list) {
		meta->groups_list = (GF_GroupListBox *)gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_GRPL);
		if (!meta->groups_list) return GF_OUT_OF_MEM;
		meta->groups_list->child_boxes = gf_list_new();
		if (!meta->groups_list->child_boxes) return GF_OUT_OF_MEM;
	}
	count = gf_list_count(meta->groups_list->child_boxes);
	for (i = 0; i < count; i++) {
		group = (GF_EntityToGroupTypeBox *)gf_list_get(meta->groups_list->child_boxes, i);
		if (group->grouping_type == group_type && group->group_id == group_id) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		group = (GF_EntityToGroupTypeBox *)gf_isom_box_new_parent(&meta->groups_list->child_boxes, GF_ISOM_BOX_TYPE_GRPT);
		if (!group) return GF_OUT_OF_MEM;
		group->grouping_type = group_type;
		group->group_id = group_id;
		group->entity_ids = NULL;
		group->entity_id_count = 0;
	} else {
		group = (GF_EntityToGroupTypeBox *)gf_list_get(meta->groups_list->child_boxes, index);
	}

	group->entity_ids = (u32 *)gf_realloc(group->entity_ids, (group->entity_id_count + 1) * sizeof(u32));
	if (!group->entity_ids) return GF_OUT_OF_MEM;
	group->entity_ids[group->entity_id_count] = item_id;
	group->entity_id_count++;

	return GF_OK;
}

GF_EXPORT
u32 gf_isom_meta_get_item_ref_count(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 from_id, u32 type)
{
	u32 i, count;
	GF_ItemReferenceTypeBox *ref;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !type || !from_id) return 0;
	if (!meta->item_refs) return 0;

	count = gf_list_count(meta->item_refs->references);
	for (i = 0; i < count; i++) {
		ref = (GF_ItemReferenceTypeBox *)gf_list_get(meta->item_refs->references, i);
		if (ref->from_item_id == from_id && ref->reference_type == type) {
			return ref->reference_count;
		}
	}
	return 0;
}

GF_EXPORT
u32 gf_isom_meta_get_item_ref_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 from_id, u32 type, u32 ref_idx)
{
	u32 i, count;
	GF_ItemReferenceTypeBox *ref;
	GF_MetaBox *meta = gf_isom_get_meta(file, root_meta, track_num);
	if (!meta || !type || !from_id || !ref_idx) return 0;
	if (!meta->item_refs) return 0;

	count = gf_list_count(meta->item_refs->references);
	for (i = 0; i < count; i++) {
		ref = (GF_ItemReferenceTypeBox *)gf_list_get(meta->item_refs->references, i);
		if (ref->from_item_id == from_id && ref->reference_type == type) {
			if (ref_idx>ref->reference_count) return 0;
			return ref->to_item_IDs[ref_idx-1];
		}
	}
	return 0;
}

#endif /*GPAC_DISABLE_ISOM*/
