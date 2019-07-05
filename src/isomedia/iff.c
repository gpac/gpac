/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2019
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
#include <gpac/constants.h>
#include <gpac/media_tools.h>

#ifndef GPAC_DISABLE_ISOM

GF_Box *ispe_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ImageSpatialExtentsPropertyBox, GF_ISOM_BOX_TYPE_ISPE);
	return (GF_Box *)tmp;
}

void ispe_box_del(GF_Box *a)
{
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox *)a;
	gf_free(p);
}

GF_Err ispe_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox *)s;

	if (p->version == 0 && p->flags == 0) {
		p->image_width = gf_bs_read_u32(bs);
		p->image_height = gf_bs_read_u32(bs);
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for ispe box not supported" ));
		gf_bs_skip_bytes(bs, p->size);
		return GF_NOT_SUPPORTED;
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ispe_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox*)s;

	p->version = 0;
	p->flags = 0;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, p->image_width);
	gf_bs_write_u32(bs, p->image_height);
	return GF_OK;
}

GF_Err ispe_box_size(GF_Box *s)
{
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox*)s;
	if (p->version == 0 && p->flags == 0) {
		p->size += 8;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for ispe box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *colr_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ColourInformationBox, GF_ISOM_BOX_TYPE_COLR);
	return (GF_Box *)tmp;
}

void colr_box_del(GF_Box *a)
{
	GF_ColourInformationBox *p = (GF_ColourInformationBox *)a;
	if (p->opaque) gf_free(p->opaque);
	gf_free(p);
}

GF_Err colr_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ColourInformationBox *p = (GF_ColourInformationBox *)s;

	if (p->is_jp2) {
		ISOM_DECREASE_SIZE(p, 3);
		p->method = gf_bs_read_u8(bs);
		p->precedence = gf_bs_read_u8(bs);
		p->approx = gf_bs_read_u8(bs);
		if (p->size) {
			p->opaque = gf_malloc(sizeof(u8)*(size_t)p->size);
			p->opaque_size = (u32) p->size;
			gf_bs_read_data(bs, (char *) p->opaque, p->opaque_size);
		}
	} else {
		ISOM_DECREASE_SIZE(p, 4);
		p->colour_type = gf_bs_read_u32(bs);
		switch (p->colour_type) {
		case GF_ISOM_SUBTYPE_NCLX:
			ISOM_DECREASE_SIZE(p, 7);
			p->colour_primaries = gf_bs_read_u16(bs);
			p->transfer_characteristics = gf_bs_read_u16(bs);
			p->matrix_coefficients = gf_bs_read_u16(bs);
			p->full_range_flag = (gf_bs_read_u8(bs) & 0x80 ? GF_TRUE : GF_FALSE);
			break;
		case GF_ISOM_SUBTYPE_NCLC:
			ISOM_DECREASE_SIZE(p, 6);
			p->colour_primaries = gf_bs_read_u16(bs);
			p->transfer_characteristics = gf_bs_read_u16(bs);
			p->matrix_coefficients = gf_bs_read_u16(bs);
			break;
		default:
			p->opaque = gf_malloc(sizeof(u8)*(size_t)p->size);
			p->opaque_size = (u32) p->size;
			gf_bs_read_data(bs, (char *) p->opaque, p->opaque_size);
			break;
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err colr_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ColourInformationBox *p = (GF_ColourInformationBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (p->is_jp2) {
		gf_bs_write_u8(bs, p->method);
		gf_bs_write_u8(bs, p->precedence);
		gf_bs_write_u8(bs, p->approx);
		if (p->opaque_size)
			gf_bs_write_data(bs, (char *)p->opaque, p->opaque_size);
	} else {
		switch (p->colour_type) {
		case GF_ISOM_SUBTYPE_NCLX:
			gf_bs_write_u32(bs, p->colour_type);
			gf_bs_write_u16(bs, p->colour_primaries);
			gf_bs_write_u16(bs, p->transfer_characteristics);
			gf_bs_write_u16(bs, p->matrix_coefficients);
			gf_bs_write_u8(bs, (p->full_range_flag == GF_TRUE ? 0x80 : 0));
			break;
		case GF_ISOM_SUBTYPE_NCLC:
			gf_bs_write_u32(bs, p->colour_type);
			gf_bs_write_u16(bs, p->colour_primaries);
			gf_bs_write_u16(bs, p->transfer_characteristics);
			gf_bs_write_u16(bs, p->matrix_coefficients);
			break;
		default:
			gf_bs_write_u32(bs, p->colour_type);
			gf_bs_write_data(bs, (char *)p->opaque, p->opaque_size);
			break;
		}
	}
	return GF_OK;
}

GF_Err colr_box_size(GF_Box *s)
{
	GF_ColourInformationBox *p = (GF_ColourInformationBox*)s;

	if (p->is_jp2) {
		p->size += 3 + p->opaque_size;
	} else {
		switch (p->colour_type) {
		case GF_ISOM_SUBTYPE_NCLX:
			p->size += 11;
			break;
		case GF_ISOM_SUBTYPE_NCLC:
			p->size += 10;
			break;
		default:
			p->size += 4 + p->opaque_size;
			break;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *pixi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_PixelInformationPropertyBox, GF_ISOM_BOX_TYPE_PIXI);
	return (GF_Box *)tmp;
}

void pixi_box_del(GF_Box *a)
{
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox *)a;
	if (p->bits_per_channel) gf_free(p->bits_per_channel);
	gf_free(p);
}

GF_Err pixi_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox *)s;

	if (p->version == 0 && p->flags == 0) {
		p->num_channels = gf_bs_read_u8(bs);
		p->bits_per_channel = (u8 *)gf_malloc(p->num_channels);
		for (i = 0; i < p->num_channels; i++) {
			p->bits_per_channel[i] = gf_bs_read_u8(bs);
		}
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for pixi box not supported" ));
		gf_bs_skip_bytes(bs, p->size);
		return GF_NOT_SUPPORTED;
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err pixi_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox*)s;

	p->version = 0;
	p->flags = 0;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->num_channels);
	for (i = 0; i < p->num_channels; i++) {
		gf_bs_write_u8(bs, p->bits_per_channel[i]);
	}
	return GF_OK;
}

GF_Err pixi_box_size(GF_Box *s)
{
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox*)s;
	if (p->version == 0 && p->flags == 0) {
		p->size += 1 + p->num_channels;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for pixi box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *rloc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_RelativeLocationPropertyBox, GF_ISOM_BOX_TYPE_RLOC);
	return (GF_Box *)tmp;
}

void rloc_box_del(GF_Box *a)
{
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox *)a;
	gf_free(p);
}

GF_Err rloc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox *)s;

	if (p->version == 0 && p->flags == 0) {
		p->horizontal_offset = gf_bs_read_u32(bs);
		p->vertical_offset = gf_bs_read_u32(bs);
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for rloc box not supported" ));
		gf_bs_skip_bytes(bs, p->size);
		return GF_NOT_SUPPORTED;
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err rloc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox*)s;

	p->version = 0;
	p->flags = 0;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, p->horizontal_offset);
	gf_bs_write_u32(bs, p->vertical_offset);
	return GF_OK;
}

GF_Err rloc_box_size(GF_Box *s)
{
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox*)s;
	if (p->version == 0 && p->flags == 0) {
		p->size += 8;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for rloc box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *irot_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ImageRotationBox, GF_ISOM_BOX_TYPE_IROT);
	return (GF_Box *)tmp;
}

void irot_box_del(GF_Box *a)
{
	GF_ImageRotationBox *p = (GF_ImageRotationBox *)a;
	gf_free(p);
}

GF_Err irot_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ImageRotationBox *p = (GF_ImageRotationBox *)s;
	p->angle = gf_bs_read_u8(bs) & 0x3;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err irot_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ImageRotationBox *p = (GF_ImageRotationBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->angle);
	return GF_OK;
}

GF_Err irot_box_size(GF_Box *s)
{
	GF_ImageRotationBox *p = (GF_ImageRotationBox*)s;
	p->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipco_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertyContainerBox, GF_ISOM_BOX_TYPE_IPCO);
	return (GF_Box *)tmp;
}

void ipco_box_del(GF_Box *s)
{
	GF_ItemPropertyContainerBox *p = (GF_ItemPropertyContainerBox *)s;
	gf_free(p);
}

GF_Err ipco_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, NULL);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ipco_box_write(GF_Box *s, GF_BitStream *bs)
{
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err ipco_box_size(GF_Box *s)
{
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iprp_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertiesBox, GF_ISOM_BOX_TYPE_IPRP);
	return (GF_Box *)tmp;
}

void iprp_box_del(GF_Box *s)
{
	gf_free(s);
}

static GF_Err iprp_on_child_box(GF_Box *s, GF_Box *a)
{
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IPCO:
		if (p->property_container) ERROR_ON_DUPLICATED_BOX(a, p)
		p->property_container = (GF_ItemPropertyContainerBox*)a;
		break;
	case GF_ISOM_BOX_TYPE_IPMA:
		if (p->property_association) ERROR_ON_DUPLICATED_BOX(a, p)
		p->property_association = (GF_ItemPropertyAssociationBox*)a;
		break;
	default:
		return GF_OK;
	}
	return GF_OK;
}

GF_Err iprp_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs, iprp_on_child_box);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err iprp_box_write(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_write_header(s, bs);
}

GF_Err iprp_box_size(GF_Box *s)
{
	u32 pos=0;
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	gf_isom_check_position(s, (GF_Box*)p->property_container, &pos);
	gf_isom_check_position(s, (GF_Box*)p->property_association, &pos);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipma_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertyAssociationBox, GF_ISOM_BOX_TYPE_IPMA);
	tmp->entries = gf_list_new();
	return (GF_Box *)tmp;
}

void ipma_box_del(GF_Box *a)
{
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox *)a;
	if (p->entries) {
		u32 i, j;
		u32 count, count2;
		count = gf_list_count(p->entries);

		for (i = 0; i < count; i++) {
			GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(p->entries, i);
			if (entry) {
				count2 = gf_list_count(entry->essential);
				for (j = 0; j < count2; j++) {
					Bool *essential = (Bool *)gf_list_get(entry->essential, j);
					Bool *prop_index = (Bool *)gf_list_get(entry->property_index, j);
					gf_free(essential);
					gf_free(prop_index);
				}
				gf_list_del(entry->essential);
				gf_list_del(entry->property_index);
				gf_free(entry);
			}
		}
		gf_list_del(p->entries);
	}
	gf_free(p);
}

GF_Err ipma_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, j;
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox *)s;
	u32 entry_count, association_count;

	entry_count = gf_bs_read_u32(bs);
	for (i = 0; i < entry_count; i++) {
		GF_ItemPropertyAssociationEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemPropertyAssociationEntry);
		if (!entry) return GF_OUT_OF_MEM;
		entry->essential = gf_list_new();
		entry->property_index = gf_list_new();
		gf_list_add(p->entries, entry);
		if (p->version == 0) {
			entry->item_id = gf_bs_read_u16(bs);
		} else {
			entry->item_id = gf_bs_read_u32(bs);
		}
		association_count = gf_bs_read_u8(bs);
		for (j = 0; j < association_count; j++) {
			Bool *ess = (Bool *)gf_malloc(sizeof(Bool));
			u32 *prop_index = (u32 *)gf_malloc(sizeof(u32));
			if (p->flags & 1) {
				u16 tmp = gf_bs_read_u16(bs);
				*ess = (tmp >> 15 ? GF_TRUE: GF_FALSE);
				*prop_index = (tmp & 0x7FFF);
			} else {
				u8 tmp = gf_bs_read_u8(bs);
				*ess = (tmp >> 7 ? GF_TRUE: GF_FALSE);
				*prop_index = (tmp & 0x7F);
			}
			gf_list_add(entry->essential, ess);
			gf_list_add(entry->property_index, prop_index);
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ipma_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i, j;
	GF_Err e;
	u32 entry_count, association_count;
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	entry_count = gf_list_count(p->entries);
	gf_bs_write_u32(bs, entry_count);
	for (i = 0; i < entry_count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(p->entries, i);
		if (p->version == 0) {
			gf_bs_write_u16(bs, entry->item_id);
		} else {
			gf_bs_write_u32(bs, entry->item_id);
		}
		association_count = gf_list_count(entry->essential);
		gf_bs_write_u8(bs, association_count);
		for (j = 0; j < association_count; j++) {
			Bool *ess = (Bool *)gf_list_get(entry->essential, j);
			u32 *prop_index = (u32 *)gf_list_get(entry->property_index, j);
			if (p->flags & 1) {
				gf_bs_write_u16(bs, (u16)( ( (*ess ? 1 : 0) << 15) | (*prop_index & 0x7F) ) );
			} else {
				gf_bs_write_u8(bs, (u32)(( (*ess ? 1 : 0) << 7) | *prop_index));
			}
		}
	}
	return GF_OK;
}

GF_Err ipma_box_size(GF_Box *s)
{
	u32 i;
	u32 entry_count, association_count;
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox*)s;

	entry_count = gf_list_count(p->entries);
	p->size += 4;
	if (p->version == 0) {
		p->size += entry_count * 2;
	} else {
		p->size += entry_count * 4;
	}
	p->size += entry_count;
	for (i = 0; i < entry_count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(p->entries, i);
		association_count = gf_list_count(entry->essential);
		if (p->flags & 1) {
			p->size += association_count * 2;
		} else {
			p->size += association_count;
		}
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *grpl_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_GroupListBox, GF_ISOM_BOX_TYPE_GRPL);
	return (GF_Box *)tmp;
}

void grpl_box_del(GF_Box *s)
{
	GF_GroupListBox *p = (GF_GroupListBox *)s;
	gf_free(p);
}

GF_Err grpl_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read_ex(s, bs, NULL, s->type);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err grpl_box_write(GF_Box *s, GF_BitStream *bs)
{
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err grpl_box_size(GF_Box *s)
{
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


void grptype_box_del(GF_Box *s)
{
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *)s;
	if (!ptr) return;
	if (ptr->entity_ids) gf_free(ptr->entity_ids);
	gf_free(ptr);
}


GF_Err grptype_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 bytesToRead;
	u32 i;
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *)s;

	bytesToRead = (u32) (ptr->size);
	if (!bytesToRead) return GF_OK;
	ptr->group_id = gf_bs_read_u32(bs);
	ptr->entity_id_count = gf_bs_read_u32(bs);
	ISOM_DECREASE_SIZE(ptr, 8)
	if (ptr->entity_id_count*4 > ptr->size) return GF_ISOM_INVALID_FILE;

	ptr->entity_ids = (u32 *) gf_malloc(ptr->entity_id_count * sizeof(u32));
	if (!ptr->entity_ids) return GF_OUT_OF_MEM;

	for (i = 0; i < ptr->entity_id_count; i++) {
		ptr->entity_ids[i] = gf_bs_read_u32(bs);
	}
	return GF_OK;
}

GF_Box *grptype_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_EntityToGroupTypeBox, GF_ISOM_BOX_TYPE_GRPT);
	//the group type code is assign in gf_isom_box_parse_ex
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err grptype_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *)s;
	ptr->type = ptr->grouping_type;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	ptr->type = GF_ISOM_BOX_TYPE_GRPT;

	gf_bs_write_u32(bs, ptr->group_id);
	gf_bs_write_u32(bs, ptr->entity_id_count);

	for (i = 0; i < ptr->entity_id_count; i++) {
		gf_bs_write_u32(bs, ptr->entity_ids[i]);
	}
	return GF_OK;
}


GF_Err grptype_box_size(GF_Box *s)
{
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *)s;
	ptr->size += 8 + 4*ptr->entity_id_count;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *auxc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AuxiliaryTypePropertyBox, GF_ISOM_BOX_TYPE_AUXC);
	return (GF_Box *)tmp;
}

void auxc_box_del(GF_Box *a)
{
	GF_AuxiliaryTypePropertyBox *p = (GF_AuxiliaryTypePropertyBox *)a;
	if (p->aux_urn) gf_free(p->aux_urn);
	if (p->data) gf_free(p->data);
	gf_free(p);
}

GF_Err auxc_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AuxiliaryTypePropertyBox *p = (GF_AuxiliaryTypePropertyBox *)s;
	GF_Err e;

	e = gf_isom_read_null_terminated_string(s, bs, s->size, &p->aux_urn);
	if (e) return e;
	p->data_size = (u32) p->size;
	p->data = gf_malloc(sizeof(char) * p->data_size);
	gf_bs_read_data(bs, p->data, p->data_size);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err auxc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AuxiliaryTypePropertyBox *p = (GF_AuxiliaryTypePropertyBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//with terminating 0
    if (p->aux_urn)
        gf_bs_write_data(bs, p->aux_urn, (u32) strlen(p->aux_urn) );
    gf_bs_write_u8(bs, 0);
	gf_bs_write_data(bs, p->data, p->data_size);

	return GF_OK;
}

GF_Err auxc_box_size(GF_Box *s)
{
	GF_AuxiliaryTypePropertyBox *p = (GF_AuxiliaryTypePropertyBox*)s;
    p->size += (p->aux_urn ? strlen(p->aux_urn) : 0) + 1 + p->data_size;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

void auxi_box_del(GF_Box *s)
{
	GF_AuxiliaryTypeInfoBox *ptr = (GF_AuxiliaryTypeInfoBox *)s;
	if (ptr->aux_track_type) gf_free(ptr->aux_track_type);
	if (ptr) gf_free(ptr);
	return;
}

GF_Err auxi_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AuxiliaryTypeInfoBox *ptr = (GF_AuxiliaryTypeInfoBox *)s;
	return gf_isom_read_null_terminated_string(s, bs, s->size, &ptr->aux_track_type);
}

GF_Box *auxi_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AuxiliaryTypeInfoBox, GF_ISOM_BOX_TYPE_AUXI);
	return (GF_Box *) tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err auxi_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AuxiliaryTypeInfoBox *ptr = (GF_AuxiliaryTypeInfoBox *)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	//with terminating 0
    if (ptr->aux_track_type)
        gf_bs_write_data(bs, ptr->aux_track_type, (u32) strlen(ptr->aux_track_type) );
    gf_bs_write_u8(bs, 0);
	return GF_OK;
}

GF_Err auxi_box_size(GF_Box *s)
{
	GF_AuxiliaryTypeInfoBox *ptr = (GF_AuxiliaryTypeInfoBox *)s;
    ptr->size += (ptr->aux_track_type ? strlen(ptr->aux_track_type) : 0 )+ 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/
GF_Box *oinf_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_OINFPropertyBox, GF_ISOM_BOX_TYPE_OINF);
	tmp->oinf = gf_isom_oinf_new_entry();
	return (GF_Box *)tmp;
}

void oinf_box_del(GF_Box *a)
{
	GF_OINFPropertyBox *p = (GF_OINFPropertyBox *)a;
	if (p->oinf) gf_isom_oinf_del_entry(p->oinf);
	gf_free(p);
}

GF_Err oinf_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_OINFPropertyBox *p = (GF_OINFPropertyBox *)s;
	return gf_isom_oinf_read_entry(p->oinf, bs);
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err oinf_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_OINFPropertyBox *p = (GF_OINFPropertyBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	return gf_isom_oinf_write_entry(p->oinf, bs);
}

GF_Err oinf_box_size(GF_Box *s)
{
	GF_OINFPropertyBox *p = (GF_OINFPropertyBox*)s;
	if (!p->oinf) return GF_BAD_PARAM;
	p->size += gf_isom_oinf_size_entry(p->oinf);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *tols_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_TargetOLSPropertyBox, GF_ISOM_BOX_TYPE_TOLS);
	return (GF_Box *)tmp;
}

void tols_box_del(GF_Box *a)
{
	gf_free(a);
}

GF_Err tols_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_TargetOLSPropertyBox *p = (GF_TargetOLSPropertyBox *)s;

	ISOM_DECREASE_SIZE(p, 2)
	p->target_ols_index = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err tols_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_TargetOLSPropertyBox *p = (GF_TargetOLSPropertyBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, p->target_ols_index);
	return GF_OK;
}

GF_Err tols_box_size(GF_Box *s)
{
	GF_TargetOLSPropertyBox *p = (GF_TargetOLSPropertyBox*)s;
	p->size += 2;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *clli_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ContentLightLevelBox, GF_ISOM_BOX_TYPE_CLLI);
	return (GF_Box *)tmp;
}

void clli_box_del(GF_Box *a)
{
	GF_ContentLightLevelBox *p = (GF_ContentLightLevelBox *)a;
	gf_free(p);
}

GF_Err clli_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ContentLightLevelBox *p = (GF_ContentLightLevelBox *)s;
	p->max_content_light_level = gf_bs_read_u16(bs);
	p->max_pic_average_light_level = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clli_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ContentLightLevelBox *p = (GF_ContentLightLevelBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, p->max_content_light_level);
	gf_bs_write_u16(bs, p->max_pic_average_light_level);
	return GF_OK;
}

GF_Err clli_box_size(GF_Box *s)
{
	GF_ContentLightLevelBox *p = (GF_ContentLightLevelBox*)s;
	p->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *mdcv_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_MasteringDisplayColourVolumeBox, GF_ISOM_BOX_TYPE_MDCV);
	return (GF_Box *)tmp;
}

void mdcv_box_del(GF_Box *a)
{
	GF_MasteringDisplayColourVolumeBox *p = (GF_MasteringDisplayColourVolumeBox *)a;
	gf_free(p);
}

GF_Err mdcv_box_read(GF_Box *s, GF_BitStream *bs)
{
	int c = 0;
	GF_MasteringDisplayColourVolumeBox *p = (GF_MasteringDisplayColourVolumeBox *)s;
	for (c = 0; c<3; c++) {
		p->display_primaries[c].x = gf_bs_read_u16(bs);
		p->display_primaries[c].y = gf_bs_read_u16(bs);
	}
	p->white_point_x = gf_bs_read_u16(bs);
	p->white_point_y = gf_bs_read_u16(bs);
	p->max_display_mastering_luminance = gf_bs_read_u32(bs);
	p->min_display_mastering_luminance = gf_bs_read_u32(bs);

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err mdcv_box_write(GF_Box *s, GF_BitStream *bs)
{
	int c = 0;
	GF_Err e;
	GF_MasteringDisplayColourVolumeBox *p = (GF_MasteringDisplayColourVolumeBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	for (c = 0; c<3; c++) {
		gf_bs_write_u16(bs, p->display_primaries[c].x);
		gf_bs_write_u16(bs, p->display_primaries[c].y);
	}
	gf_bs_write_u16(bs, p->white_point_x);
	gf_bs_write_u16(bs, p->white_point_y);
	gf_bs_write_u32(bs, p->max_display_mastering_luminance);
	gf_bs_write_u32(bs, p->min_display_mastering_luminance);
	
	return GF_OK;
}

GF_Err mdcv_box_size(GF_Box *s)
{
	GF_MasteringDisplayColourVolumeBox *p = (GF_MasteringDisplayColourVolumeBox*)s;
	p->size += 24;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



static GF_Err gf_isom_iff_create_image_item_from_track_internal(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, u32 imported_track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs, u32 sample_number) {
	GF_Err e;
	u32 imported_sample_desc_index = 1;
//	u32 sample_index = 1;
	u32 w, h, hSpacing, vSpacing;
	u8 num_channels;
	u8 bits_per_channel[3];
	u32 subtype;
	GF_ISOSample *sample = NULL;
	u32 timescale;
	u32 item_type = 0;
	GF_ImageItemProperties local_image_props;
	Bool config_needed = 0;
	GF_Box *config_box = NULL;
	//u32 media_brand = 0;
	u32 sample_desc_index = 0;

	if (image_props && image_props->tile_mode != TILE_ITEM_NONE) {
		/* Processing the input file in Tiled mode:
		   The single track is split into multiple tracks
		   and each track is processed to create an item */
		u32 i, count;
		u32 tile_track;
		GF_List *tile_item_ids;
		char sz_item_name[256];
		GF_TileItemMode orig_tile_mode;
#ifndef GPAC_DISABLE_MEDIA_IMPORT
		e = gf_media_split_hevc_tiles(movie, 0);
#else
		e = GF_NOT_SUPPORTED;
#endif
		if (e) return e;
		tile_item_ids = gf_list_new();
		orig_tile_mode = image_props->tile_mode;
		image_props->tile_mode = TILE_ITEM_NONE;
		count = gf_isom_get_reference_count(movie, imported_track, GF_ISOM_REF_SABT);
		for (i = 0; i < count; i++) {
			u32 *tile_item_id = gf_malloc(sizeof(u32));
			*tile_item_id = item_id + i+1;
			gf_list_add(tile_item_ids, tile_item_id);
			e = gf_isom_get_reference(movie, imported_track, GF_ISOM_REF_SABT, 1, &tile_track);
			if (e) return e;
			sprintf(sz_item_name, "%s-Tile%d", (item_name ? item_name : "Image"), i + 1);
			if (orig_tile_mode != TILE_ITEM_SINGLE || image_props->single_tile_number == i + 1) {
				e = gf_isom_iff_create_image_item_from_track(movie, root_meta, meta_track_number, tile_track, sz_item_name, *tile_item_id, NULL, NULL);
			}
			if (e) return e;
			gf_isom_remove_track(movie, tile_track);
			if (orig_tile_mode == TILE_ITEM_ALL_BASE) {
				e = gf_isom_meta_add_item_ref(movie, root_meta, meta_track_number, *tile_item_id, item_id, GF_ISOM_REF_TBAS, NULL);
			}
			if (e) return e;
		}
		sprintf(sz_item_name, "%s-TileBase", (item_name ? item_name : "Image"));
		if (orig_tile_mode == TILE_ITEM_ALL_BASE) {
			gf_isom_iff_create_image_item_from_track(movie, root_meta, meta_track_number, imported_track, sz_item_name, item_id, image_props, tile_item_ids);
		}
		else if (orig_tile_mode == TILE_ITEM_ALL_GRID) {
			// TODO
		}
		for (i = 0; i < count; i++) {
			u32 *tile_item_id = gf_list_get(tile_item_ids, i);
			gf_free(tile_item_id);
		}
		gf_list_del(tile_item_ids);
		return GF_OK;
	}

import_next_sample:

	/* Check if the track type is supported as item type */
	/* Get the config box if needed */
	subtype = gf_isom_get_media_subtype(movie, imported_track, imported_sample_desc_index);
	switch (subtype) {
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		//FIXME: in avc1 with multiple descriptor, we should take the right description index
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_avc_config_get(movie, imported_track, imported_sample_desc_index);
		item_type = GF_ISOM_SUBTYPE_AVC_H264;
		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;
	case GF_ISOM_SUBTYPE_SVC_H264:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_svc_config_get(movie, imported_track, imported_sample_desc_index);
		item_type = GF_ISOM_SUBTYPE_SVC_H264;
		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;
	case GF_ISOM_SUBTYPE_MVC_H264:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_MVCC);
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_mvc_config_get(movie, imported_track, imported_sample_desc_index);
		item_type = GF_ISOM_SUBTYPE_MVC_H264;
		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;
	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC2:
	case GF_ISOM_SUBTYPE_HEV2:
	case GF_ISOM_SUBTYPE_HVT1:
	case GF_ISOM_SUBTYPE_LHV1:
	case GF_ISOM_SUBTYPE_LHE1:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_HVCC);
		((GF_HEVCConfigurationBox *)config_box)->config = gf_isom_hevc_config_get(movie, imported_track, imported_sample_desc_index);
		if (subtype == GF_ISOM_SUBTYPE_HVT1) {
			item_type = GF_ISOM_SUBTYPE_HVT1;
		}
		else {
			item_type = GF_ISOM_SUBTYPE_HVC1;
		}
		config_needed = 1;
		if (!((GF_HEVCConfigurationBox *)config_box)->config) {
			((GF_HEVCConfigurationBox *)config_box)->config = gf_isom_lhvc_config_get(movie, imported_track, imported_sample_desc_index);
			item_type = GF_ISOM_SUBTYPE_LHV1;
		}
		num_channels = 3;
		bits_per_channel[0] = ((GF_HEVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_HEVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		//media_brand = GF_ISOM_BRAND_HEIC;
		break;
	case GF_ISOM_SUBTYPE_AV01:
		{
			config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_AV1C);
			((GF_AV1ConfigurationBox *)config_box)->config = gf_isom_av1_config_get(movie, imported_track, imported_sample_desc_index);
			item_type = GF_ISOM_SUBTYPE_AV01;
			config_needed = 1;
			u8 depth = ((GF_AV1ConfigurationBox *)config_box)->config->high_bitdepth ? (((GF_AV1ConfigurationBox *)config_box)->config->twelve_bit ? 12 : 10 ) : 8;
			if (((GF_AV1ConfigurationBox *)config_box)->config->monochrome) {
				num_channels = 1;
				bits_per_channel[0] = depth;
				bits_per_channel[1] = 0;
				bits_per_channel[2] = 0;
			} else {
				num_channels = 3;
				bits_per_channel[0] = depth;
				bits_per_channel[1] = depth;
				bits_per_channel[2] = depth;
			}
			//media_brand = GF_ISOM_BRAND_AVIF;
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Codec not supported to create HEIF image items\n"));
		return GF_NOT_SUPPORTED;
	}
	if (config_needed && !config_box && !((GF_AVCConfigurationBox *)config_box)->config) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Image type %s requires a missing configuration box\n", gf_4cc_to_str(item_type)));
		return GF_BAD_PARAM;
	}
	/* Get some images properties from the track data */
	e = gf_isom_get_visual_info(movie, imported_track, imported_sample_desc_index, &w, &h);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error determining image size\n"));
		if (config_box) gf_isom_box_del(config_box);
		return e;
	}
	e = gf_isom_get_pixel_aspect_ratio(movie, imported_track, imported_sample_desc_index, &hSpacing, &vSpacing);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error determining image aspect ratio\n"));
		if (config_box) gf_isom_box_del(config_box);
		return e;
	}
	if (!image_props) {
		image_props = &local_image_props;
		memset(image_props, 0, sizeof(GF_ImageItemProperties));
	}
	if (!image_props->width && !image_props->height) {
		image_props->width = w;
		image_props->height = h;
	}
	if (!image_props->hSpacing && !image_props->vSpacing) {
		image_props->hSpacing = hSpacing;
		image_props->vSpacing = vSpacing;
	}
	image_props->config = config_box;
	if (!image_props->num_channels) {
		image_props->num_channels = num_channels;
		image_props->bits_per_channel[0] = bits_per_channel[0];
		image_props->bits_per_channel[1] = bits_per_channel[1];
		image_props->bits_per_channel[2] = bits_per_channel[2];
	}

	timescale = gf_isom_get_media_timescale(movie, imported_track);
	if (image_props->time<0) {
		sample = gf_isom_get_sample(movie, imported_track, sample_number, &sample_desc_index);
	} else {
		e = gf_isom_get_sample_for_media_time(movie, imported_track, (u64)(image_props->time*timescale), &sample_desc_index, GF_ISOM_SEARCH_SYNC_FORWARD, &sample, NULL, NULL);
	}
	if (sample_desc_index != imported_sample_desc_index) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Warning sample description index for sync sample differ from config\n"));
	}
	if (e || !sample || !sample->IsRAP) {
		if (image_props->time<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error some imported samples are not sync samples\n", image_props->time));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error no sync sample found after time %g\n", image_props->time));
		}
		if (sample) gf_isom_sample_del(&sample);
		if (config_box) gf_isom_box_del(config_box);
		return GF_BAD_PARAM;
	}

	if (!item_id) {
		e = gf_isom_meta_get_next_item_id(movie, root_meta, meta_track_number, &item_id);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Adding sample from time %.3f as item %d\n", sample->DTS*1.0/timescale, item_id));
	e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, (!item_name || !strlen(item_name) ? "Image" : item_name), item_id, item_type, NULL, NULL, image_props, sample->data, sample->dataLength, item_extent_refs);

	gf_isom_set_brand_info(movie, GF_ISOM_BRAND_MIF1, 0);
	gf_isom_reset_alt_brands(movie);
	// TODO Analyze configuration to determine the brand */
	//if (media_brand) {
	//	gf_isom_modify_alternate_brand(movie, media_brand, 1);
	//}
	gf_isom_sample_del(&sample);
	if (config_box) gf_isom_box_del(config_box);

	if (image_props->time<0) {
		if (sample_number >= gf_isom_get_sample_count(movie, imported_track)) return e;
		sample_number++;
		item_id=0;
		//avoid recursion this could get quite big
		goto import_next_sample;
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_iff_create_image_item_from_track(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, u32 imported_track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs)
{

 	return gf_isom_iff_create_image_item_from_track_internal(movie, root_meta, meta_track_number, imported_track, item_name, item_id, image_props, item_extent_refs, 1);
}
#endif /*GPAC_DISABLE_ISOM*/
