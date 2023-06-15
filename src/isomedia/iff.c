/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2023
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
#endif

GF_Box *a1lx_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AV1LayeredImageIndexingPropertyBox, GF_ISOM_BOX_TYPE_A1LX);
	return (GF_Box *)tmp;
}

void a1lx_box_del(GF_Box *a)
{
	GF_AV1LayeredImageIndexingPropertyBox *p = (GF_AV1LayeredImageIndexingPropertyBox *)a;
	gf_free(p);
}

GF_Err a1lx_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_AV1LayeredImageIndexingPropertyBox *p = (GF_AV1LayeredImageIndexingPropertyBox *)s;
	
	ISOM_DECREASE_SIZE(p, 1);
	gf_bs_read_int(bs, 7);
	p->large_size = gf_bs_read_int(bs, 1);
	for (i=0; i<3; i++) {
		if (p->large_size) {
			ISOM_DECREASE_SIZE(p, 4);
			p->layer_size[i] = gf_bs_read_u32(bs);
		} else {
			ISOM_DECREASE_SIZE(p, 2);
			p->layer_size[i] = gf_bs_read_u16(bs);
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err a1lx_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	GF_AV1LayeredImageIndexingPropertyBox *p = (GF_AV1LayeredImageIndexingPropertyBox*)s;

	gf_bs_write_int(bs, 0, 7);
	gf_bs_write_int(bs, p->large_size ? 1 : 0, 1);
	for (i=0; i<3; i++) {
		if (p->large_size) {
			gf_bs_write_u32(bs, p->layer_size[i]);
		} else {
			gf_bs_write_u16(bs, p->layer_size[i]);
		}
	}
	return GF_OK;
}

GF_Err a1lx_box_size(GF_Box *s)
{
	GF_AV1LayeredImageIndexingPropertyBox *p = (GF_AV1LayeredImageIndexingPropertyBox*)s;

	//if large was set, do not override
	if (! p->large_size) {
		if (p->layer_size[0]>0xFFFF) p->large_size = 1;
		else if (p->layer_size[1]>0xFFFF) p->large_size = 1;
		else if (p->layer_size[2]>0xFFFF) p->large_size = 1;
	}

	p->size += (p->large_size ? 4 : 2) * 3 + 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *a1op_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AV1OperatingPointSelectorPropertyBox, GF_ISOM_BOX_TYPE_A1OP);
	return (GF_Box *)tmp;
}

void a1op_box_del(GF_Box *a)
{
	GF_AV1OperatingPointSelectorPropertyBox *p = (GF_AV1OperatingPointSelectorPropertyBox *)a;
	gf_free(p);
}

GF_Err a1op_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AV1OperatingPointSelectorPropertyBox *p = (GF_AV1OperatingPointSelectorPropertyBox *)s;
	p->op_index = gf_bs_read_u8(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err a1op_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	GF_AV1OperatingPointSelectorPropertyBox *p = (GF_AV1OperatingPointSelectorPropertyBox*)s;
	gf_bs_write_u8(bs, p->op_index);
	return GF_OK;
}

GF_Err a1op_box_size(GF_Box *s)
{
	GF_AV1OperatingPointSelectorPropertyBox *p = (GF_AV1OperatingPointSelectorPropertyBox*)s;
	p->size += 1;
	return GF_OK;
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
			p->full_range_flag = (gf_bs_read_u8(bs) & 0x80) ? GF_TRUE : GF_FALSE;
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
			ISOM_DECREASE_SIZE(p, 1)
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

GF_Box *imir_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ImageMirrorBox, GF_ISOM_BOX_TYPE_IMIR);
	return (GF_Box *)tmp;
}

void imir_box_del(GF_Box *a)
{
	GF_ImageMirrorBox *p = (GF_ImageMirrorBox *)a;
	gf_free(p);
}

GF_Err imir_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_ImageMirrorBox *p = (GF_ImageMirrorBox *)s;
	p->axis = gf_bs_read_u8(bs) & 0x1;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err imir_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ImageMirrorBox *p = (GF_ImageMirrorBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->axis);
	return GF_OK;
}

GF_Err imir_box_size(GF_Box *s)
{
	GF_ImageMirrorBox *p = (GF_ImageMirrorBox*)s;
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
	return gf_isom_box_array_read(s, bs);
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

GF_Err iprp_on_child_box(GF_Box *s, GF_Box *a, Bool is_rem)
{
	GF_ItemPropertiesBox *ptr = (GF_ItemPropertiesBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IPCO:
		BOX_FIELD_ASSIGN(property_container, GF_ItemPropertyContainerBox)
		break;
	case GF_ISOM_BOX_TYPE_IPMA:
		BOX_FIELD_ASSIGN(property_association, GF_ItemPropertyAssociationBox)
		break;
	default:
		return GF_OK;
	}
	return GF_OK;
}

GF_Err iprp_box_read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_box_array_read(s, bs);
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
		u32 i;
		u32 count;
		count = gf_list_count(p->entries);

		for (i = 0; i < count; i++) {
			GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(p->entries, i);
			if (entry) {
				gf_free(entry->associations);
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
	u32 entry_count;

	ISOM_DECREASE_SIZE(p, 4)
	entry_count = gf_bs_read_u32(bs);
	for (i = 0; i < entry_count; i++) {
		GF_ItemPropertyAssociationEntry *entry;
		GF_SAFEALLOC(entry, GF_ItemPropertyAssociationEntry);
		if (!entry) return GF_OUT_OF_MEM;
		gf_list_add(p->entries, entry);
		if (p->version == 0) {
			ISOM_DECREASE_SIZE(p, 3)
			entry->item_id = gf_bs_read_u16(bs);
		} else {
			ISOM_DECREASE_SIZE(p, 5)
			entry->item_id = gf_bs_read_u32(bs);
		}
		entry->nb_associations = gf_bs_read_u8(bs);
		entry->associations = gf_malloc(sizeof(GF_ItemPropertyAssociationSlot) * entry->nb_associations);
		if (!entry->associations) return GF_OUT_OF_MEM;
		for (j = 0; j < entry->nb_associations; j++) {
			if (p->flags & 1) {
				u16 tmp = gf_bs_read_u16(bs);
				entry->associations[j].essential = (tmp >> 15) ? GF_TRUE : GF_FALSE;
				entry->associations[j].index = (tmp & 0x7FFF);
			} else {
				u8 tmp = gf_bs_read_u8(bs);
				entry->associations[j].essential = (tmp >> 7) ? GF_TRUE : GF_FALSE;
				entry->associations[j].index = (tmp & 0x7F);
			}
		}
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ipma_box_write(GF_Box *s, GF_BitStream *bs)
{
	u32 i, j;
	GF_Err e;
	u32 entry_count;
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
		gf_bs_write_u8(bs, entry->nb_associations);
		for (j = 0; j < entry->nb_associations; j++) {
			if (p->flags & 1) {
				gf_bs_write_u16(bs, (u16)( ( (entry->associations[j].essential ? 1 : 0) << 15) | (entry->associations[j].index & 0x7F) ) );
			} else {
				gf_bs_write_u8(bs, (u32)(( (entry->associations[j].essential ? 1 : 0) << 7) | entry->associations[j].index));
			}
		}
	}
	return GF_OK;
}

GF_Err ipma_box_size(GF_Box *s)
{
	u32 i;
	u32 entry_count;
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
		if (p->flags & 1) {
			p->size += entry->nb_associations * 2;
		} else {
			p->size += entry->nb_associations;
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
	return gf_isom_box_array_read(s, bs);
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
	u32 i;
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *)s;

	ISOM_DECREASE_SIZE(ptr, 8)
	ptr->group_id = gf_bs_read_u32(bs);
	ptr->entity_id_count = gf_bs_read_u32(bs);

	if (ptr->entity_id_count > ptr->size / 4) return GF_ISOM_INVALID_FILE;

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
	ISOM_DECREASE_SIZE(p, 4)
	p->clli.max_content_light_level = gf_bs_read_u16(bs);
	p->clli.max_pic_average_light_level = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err clli_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ContentLightLevelBox *p = (GF_ContentLightLevelBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u16(bs, p->clli.max_content_light_level);
	gf_bs_write_u16(bs, p->clli.max_pic_average_light_level);
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
	ISOM_DECREASE_SIZE(p, 24)
	for (c = 0; c<3; c++) {
		p->mdcv.display_primaries[c].x = gf_bs_read_u16(bs);
		p->mdcv.display_primaries[c].y = gf_bs_read_u16(bs);
	}
	p->mdcv.white_point_x = gf_bs_read_u16(bs);
	p->mdcv.white_point_y = gf_bs_read_u16(bs);
	p->mdcv.max_display_mastering_luminance = gf_bs_read_u32(bs);
	p->mdcv.min_display_mastering_luminance = gf_bs_read_u32(bs);

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
		gf_bs_write_u16(bs, p->mdcv.display_primaries[c].x);
		gf_bs_write_u16(bs, p->mdcv.display_primaries[c].y);
	}
	gf_bs_write_u16(bs, p->mdcv.white_point_x);
	gf_bs_write_u16(bs, p->mdcv.white_point_y);
	gf_bs_write_u32(bs, p->mdcv.max_display_mastering_luminance);
	gf_bs_write_u32(bs, p->mdcv.min_display_mastering_luminance);
	
	return GF_OK;
}

GF_Err mdcv_box_size(GF_Box *s)
{
	GF_MasteringDisplayColourVolumeBox *p = (GF_MasteringDisplayColourVolumeBox*)s;
	p->size += 24;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *ienc_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemEncryptionPropertyBox, GF_ISOM_BOX_TYPE_IENC);
	return (GF_Box *)tmp;
}

void ienc_box_del(GF_Box *a)
{
	GF_ItemEncryptionPropertyBox *p = (GF_ItemEncryptionPropertyBox *)a;
	if (p->key_info) gf_free(p->key_info);
	gf_free(p);
}

GF_Err ienc_box_read(GF_Box *s, GF_BitStream *bs)
{
	u32 nb_keys;
	GF_ItemEncryptionPropertyBox *p = (GF_ItemEncryptionPropertyBox *)s;

	ISOM_DECREASE_SIZE(p, 3)
	gf_bs_read_u8(bs);
	if (p->version == 0) {
		gf_bs_read_u8(bs);
	} else {
		p->skip_byte_block = gf_bs_read_int(bs, 4);
		p->crypt_byte_block = gf_bs_read_int(bs, 4);
	}
	nb_keys = gf_bs_read_u8(bs);
	if (nb_keys * (sizeof(bin128)+1) > p->size)
		return GF_NON_COMPLIANT_BITSTREAM;
	p->key_info_size = (u32) (3+p->size);
	p->key_info = gf_malloc(sizeof(u8) * p->key_info_size);
	if (!p->key_info) return GF_OUT_OF_MEM;
	p->key_info[0] = 1;
	p->key_info[1] = 0;
	p->key_info[2] = nb_keys;
	gf_bs_read_data(bs, p->key_info+3, (u32) p->size);
	p->size = 0;
	if (!gf_cenc_validate_key_info(p->key_info, p->key_info_size))
		return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err ienc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u32 nb_keys;
	GF_ItemEncryptionPropertyBox *p = (GF_ItemEncryptionPropertyBox*)s;
	if (p->skip_byte_block || p->crypt_byte_block)
		p->version = 1;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, 0);
	if (p->version) {
		gf_bs_write_int(bs, p->skip_byte_block, 4);
		gf_bs_write_int(bs, p->crypt_byte_block, 4);
	} else {
		gf_bs_write_u8(bs, 0);
	}
	if (p->key_info[0]) {
		if (p->key_info[1]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Too many keys for ienc box, max is 255)\n"))
			return GF_BAD_PARAM;
		}
		nb_keys = p->key_info[2];
	} else {
		nb_keys = 1;
	}
	gf_bs_write_u8(bs, nb_keys);
	gf_bs_write_data(bs, p->key_info+3, p->key_info_size-3);
	return GF_OK;
}

GF_Err ienc_box_size(GF_Box *s)
{
	GF_ItemEncryptionPropertyBox *p = (GF_ItemEncryptionPropertyBox*)s;
	if (!p->key_info || (p->key_info_size<19))
		return GF_BAD_PARAM;
	p->size += 3 + p->key_info_size-3;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *iaux_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_AuxiliaryInfoPropertyBox, GF_ISOM_BOX_TYPE_IAUX);
	return (GF_Box *)tmp;
}

void iaux_box_del(GF_Box *a)
{
	gf_free(a);
}

GF_Err iaux_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_AuxiliaryInfoPropertyBox *p = (GF_AuxiliaryInfoPropertyBox *)s;

	ISOM_DECREASE_SIZE(p, 8)
	p->aux_info_type = gf_bs_read_u32(bs);
	p->aux_info_parameter = gf_bs_read_u32(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err iaux_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_AuxiliaryInfoPropertyBox *p = (GF_AuxiliaryInfoPropertyBox*)s;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, p->aux_info_type);
	gf_bs_write_u32(bs, p->aux_info_parameter);
	return GF_OK;
}

GF_Err iaux_box_size(GF_Box *s)
{
	GF_AuxiliaryInfoPropertyBox *p = (GF_AuxiliaryInfoPropertyBox*)s;
	p->size += 8;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


#ifndef GPAC_DISABLE_ISOM_WRITE

static GF_Err gf_isom_iff_create_image_item_from_track_internal(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, u32 imported_track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs, u32 sample_number) {
	GF_Err e;
	u32 w, h, hSpacing, vSpacing;
	u8 num_channels;
	u8 bits_per_channel[3];
	u32 subtype;
	GF_ISOSample *sample = NULL;
	u32 timescale;
	u32 item_type = 0;
	GF_ImageItemProperties local_image_props;
	GF_ImageItemProtection ipro, *orig_ipro = NULL;
	Bool config_needed = 0;
	GF_Box *config_box = NULL;
	Bool is_cenc = GF_FALSE;
	Bool is_first = GF_TRUE;
	Bool neg_time = (image_props && image_props->time<0) ? GF_TRUE : GF_FALSE;
	u8 *sai = NULL;
	u32 sai_size = 0, sai_alloc_size = 0;
	u32 sample_desc_index = 0;
	GF_ISOFile *fsrc = movie;
	Bool reset_brands = GF_FALSE;

	//only reset brands if first item import
	if (!gf_isom_get_meta_item_count(movie, root_meta, meta_track_number))
		reset_brands = GF_TRUE;

	if (image_props && image_props->src_file)
		fsrc = image_props->src_file;

	if (image_props && image_props->tile_mode != TILE_ITEM_NONE) {
		/* Processing the input file in Tiled mode:
		   The single track is split into multiple tracks
		   and each track is processed to create an item */
		u32 i, count;
		u32 tile_track;
		GF_List *tile_item_ids;
		char sz_item_name[256];
		GF_TileItemMode orig_tile_mode;

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_MEDIA_IMPORT)
		if (image_props->src_file)
			e = GF_SERVICE_ERROR;
		else
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
			if (!tile_item_id) return GF_OUT_OF_MEM;

			*tile_item_id = item_id + i+1;
			gf_list_add(tile_item_ids, tile_item_id);
			e = gf_isom_get_reference(movie, imported_track, GF_ISOM_REF_SABT, 1, &tile_track);
			if (e) return e;
			if (item_name)
				sprintf(sz_item_name, "%s-Tile%d", item_name, i + 1);
			if (orig_tile_mode != TILE_ITEM_SINGLE || image_props->single_tile_number == i + 1) {
				e = gf_isom_iff_create_image_item_from_track(movie, root_meta, meta_track_number, tile_track, item_name ? sz_item_name : NULL, *tile_item_id, NULL, NULL);
			}
			if (e) return e;
			gf_isom_remove_track(movie, tile_track);
			if (orig_tile_mode == TILE_ITEM_ALL_BASE) {
				e = gf_isom_meta_add_item_ref(movie, root_meta, meta_track_number, *tile_item_id, item_id, GF_ISOM_REF_TBAS, NULL);
			}
			if (e) return e;
		}
		if (item_name)
			sprintf(sz_item_name, "%s-TileBase", item_name);
		if (orig_tile_mode == TILE_ITEM_ALL_BASE) {
			gf_isom_iff_create_image_item_from_track(movie, root_meta, meta_track_number, imported_track, item_name ? sz_item_name : NULL, item_id, image_props, tile_item_ids);
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

	if (!image_props) {
		image_props = &local_image_props;
		memset(image_props, 0, sizeof(GF_ImageItemProperties));
	} else {
		orig_ipro = image_props->cenc_info;
		image_props->cenc_info = NULL;
	}

	if (!imported_track) {
		GF_ImageItemProperties src_props;
		u32 item_idx, ref_id;
		u32 scheme_type=0, scheme_version=0;
		const char *orig_item_name, *orig_item_mime_type, *orig_item_encoding;
		if (!image_props->item_ref_id) return GF_BAD_PARAM;

		if (gf_isom_meta_get_item_ref_count(fsrc, GF_TRUE, 0, image_props->item_ref_id, GF_4CC('d','i','m','g')) > 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Cannnot import derived image, only native image import is supported\n"));
			return GF_NOT_SUPPORTED;
		}

		item_idx = gf_isom_get_meta_item_by_id(fsrc, GF_TRUE, 0, image_props->item_ref_id);
		if (!item_idx) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: No item with ID %d, cannnot import\n", image_props->item_ref_id));
			return GF_BAD_PARAM;
		}
		orig_item_name = orig_item_mime_type = orig_item_encoding = NULL;
		gf_isom_get_meta_item_info(fsrc, GF_TRUE, 0, item_idx, &ref_id, &item_type, &scheme_type, &scheme_version, NULL, NULL, NULL, &orig_item_name, &orig_item_mime_type, &orig_item_encoding);

		if (!ref_id) return GF_BAD_PARAM;
		if (ref_id != image_props->item_ref_id) return GF_ISOM_INVALID_FILE;

		gf_isom_get_meta_image_props(fsrc, GF_TRUE, 0, ref_id, &src_props, NULL);

		image_props->config = src_props.config;
		image_props->width = src_props.width;
		image_props->height = src_props.height;
		image_props->num_channels = src_props.num_channels;
		memcpy(image_props->av1_layer_size, src_props.av1_layer_size, sizeof(u32)*3);
		memcpy(image_props->bits_per_channel, src_props.bits_per_channel, sizeof(u32)*3);
		if (!image_props->hSpacing && !image_props->vSpacing) {
			image_props->hSpacing = src_props.hSpacing;
			image_props->vSpacing = src_props.vSpacing;
		}
		if (image_props->copy_props) {
			if (!image_props->hOffset && !image_props->vOffset) {
				image_props->hOffset = src_props.hOffset;
				image_props->vOffset = src_props.vOffset;
			}
			if (!image_props->clap_wden) {
				image_props->clap_wnum = src_props.clap_wnum;
				image_props->clap_wden = src_props.clap_wden;
				image_props->clap_hnum = src_props.clap_hnum;
				image_props->clap_hden = src_props.clap_hden;
				image_props->clap_honum = src_props.clap_honum;
				image_props->clap_hoden = src_props.clap_hoden;
				image_props->clap_vonum = src_props.clap_vonum;
				image_props->clap_voden = src_props.clap_voden;
			}
			if (!image_props->alpha) image_props->alpha = src_props.alpha;
			if (!image_props->depth) image_props->depth = src_props.depth;
			if (!image_props->hidden) image_props->hidden = src_props.hidden;
			if (!image_props->angle) image_props->angle = src_props.angle;
			if (!image_props->mirror) image_props->mirror = src_props.mirror;
			if (!image_props->av1_op_index) image_props->av1_op_index = src_props.av1_op_index;
		}
		if (!item_name) item_name = orig_item_name;

		if (!image_props->use_reference || (fsrc == image_props->src_file)) {
			u8 *data = NULL;
			u32 size=0;
			e = gf_isom_extract_meta_item_mem(fsrc, GF_TRUE, 0, ref_id, &data, &size, &size, NULL, GF_FALSE);
			if (e) return GF_BAD_PARAM;

			e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, item_name, &item_id, item_type, NULL, NULL, image_props, data, size, NULL);
			if (data) gf_free(data);
		} else {
			e = gf_isom_add_meta_item_sample_ref(movie, root_meta, meta_track_number, item_name, &item_id, item_type, NULL, NULL, image_props, 0, ref_id);
		}
		return e;
	}

import_next_sample:

	timescale = gf_isom_get_media_timescale(fsrc, imported_track);
	if (image_props->sample_num) {
		sample_number = image_props->sample_num;
		sample = gf_isom_get_sample(fsrc, imported_track, sample_number, &sample_desc_index);
		e = gf_isom_last_error(fsrc);
	} else if (image_props->time<0) {
		sample = gf_isom_get_sample(fsrc, imported_track, sample_number, &sample_desc_index);
		e = gf_isom_last_error(fsrc);
	} else {
		e = gf_isom_get_sample_for_media_time(fsrc, imported_track, (u64)(image_props->time*timescale), &sample_desc_index, GF_ISOM_SEARCH_SYNC_FORWARD, &sample, &sample_number, NULL);
	}
	if (e || !sample || !sample->IsRAP) {
		if (!sample) {
			if (is_first) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("No sample found%s\n", (image_props->time<0) ? "" : " for requested time"));
			} else {
				e = GF_OK;
				goto exit;
			}
		} else if ((image_props->time<0) || (image_props->step_time)) {
			if (image_props->sample_num) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: imported sample %d (DTS "LLU") is not a sync sample (RAP %d size %d)\n", sample_number, sample->DTS, sample->IsRAP, sample->dataLength));
			} else if (image_props->step_time) {
				gf_isom_sample_del(&sample);
				e = GF_OK;
				goto exit;
			} else {
				gf_isom_sample_del(&sample);
				sample_number++;
				if (sample_number == gf_isom_get_sample_count(fsrc, imported_track)) {
					e = GF_OK;
					goto exit;
				}
				goto import_next_sample;
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error no sync sample found after time %g\n", image_props->time));
		}
		if (!e) e = GF_BAD_PARAM;
		goto exit;
	}

	/* Check if the track type is supported as item type */
	/* Get the config box if needed */
	subtype = gf_isom_get_media_subtype(fsrc, imported_track, sample_desc_index);
	if (gf_isom_is_media_encrypted(fsrc, imported_track, sample_desc_index)) {
		if (gf_isom_is_cenc_media(fsrc, imported_track, sample_desc_index)) {
			e = gf_isom_get_original_format_type(fsrc, imported_track, sample_desc_index, &subtype);
			if (e) goto exit;
			is_cenc = GF_TRUE;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Protected sample not using CENC, cannot add as item\n"));
			e = GF_BAD_PARAM;
			goto exit;
		}
	}


	switch (subtype) {
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		//FIXME: in avc1 with multiple descriptor, we should take the right description index
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
		if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_avc_config_get(fsrc, imported_track, sample_desc_index);
		if (! ((GF_AVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
		item_type = GF_ISOM_SUBTYPE_AVC_H264;
		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;
	case GF_ISOM_SUBTYPE_SVC_H264:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
		if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_svc_config_get(fsrc, imported_track, sample_desc_index);
		if (! ((GF_AVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
		item_type = GF_ISOM_SUBTYPE_SVC_H264;
		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_AVCConfigurationBox *)config_box)->config->luma_bit_depth;
		bits_per_channel[1] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		bits_per_channel[2] = ((GF_AVCConfigurationBox *)config_box)->config->chroma_bit_depth;
		break;
	case GF_ISOM_SUBTYPE_MVC_H264:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_MVCC);
		if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
		((GF_AVCConfigurationBox *)config_box)->config = gf_isom_mvc_config_get(fsrc, imported_track, sample_desc_index);
		if (! ((GF_AVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
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
		if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
		((GF_HEVCConfigurationBox *)config_box)->config = gf_isom_hevc_config_get(fsrc, imported_track, sample_desc_index);
		if (! ((GF_HEVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
		if (subtype == GF_ISOM_SUBTYPE_HVT1) {
			item_type = GF_ISOM_SUBTYPE_HVT1;
		}
		else {
			item_type = GF_ISOM_SUBTYPE_HVC1;
		}
		config_needed = 1;
		if (!((GF_HEVCConfigurationBox *)config_box)->config) {
			((GF_HEVCConfigurationBox *)config_box)->config = gf_isom_lhvc_config_get(fsrc, imported_track, sample_desc_index);
			if (! ((GF_HEVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
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
			if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
			((GF_AV1ConfigurationBox *)config_box)->config = gf_isom_av1_config_get(fsrc, imported_track, sample_desc_index);
			if (! ((GF_AV1ConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
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
			// presence of OBU SH in config is not recommended and properties should be used instead of metadata OBUs
			while (gf_list_count(((GF_AV1ConfigurationBox *)config_box)->config->obu_array)) {
				GF_AV1_OBUArrayEntry *obu = gf_list_pop_back(((GF_AV1ConfigurationBox *)config_box)->config->obu_array);
				if (obu) {
					if (obu->obu) gf_free(obu->obu);
					gf_free(obu);
				}
			}
			gf_list_del(((GF_AV1ConfigurationBox *)config_box)->config->obu_array);
			((GF_AV1ConfigurationBox *)config_box)->config->obu_array = NULL;
			e = gf_media_av1_layer_size_get(fsrc, imported_track, sample_number, image_props->av1_op_index, image_props->av1_layer_size);
      if (e) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("AV1 operating point index out of range for stream\n"));
        goto exit;
      }
			//media_brand = GF_ISOM_BRAND_AVIF;
		}
		break;

	case GF_ISOM_SUBTYPE_VVC1:
		config_box = gf_isom_box_new(GF_ISOM_BOX_TYPE_VVCC);
		if (!config_box) { e = GF_OUT_OF_MEM; goto exit; }
		((GF_VVCConfigurationBox *)config_box)->config = gf_isom_vvc_config_get(fsrc, imported_track, sample_desc_index);
		if (! ((GF_VVCConfigurationBox *)config_box)->config) { e = GF_OUT_OF_MEM; goto exit; }
		item_type = GF_ISOM_SUBTYPE_VVC1;

		config_needed = 1;
		num_channels = 3;
		bits_per_channel[0] = ((GF_VVCConfigurationBox *)config_box)->config->bit_depth;
		bits_per_channel[1] = bits_per_channel[2] = bits_per_channel[0];
		//media_brand = GF_ISOM_BRAND_HEIC;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Codec not supported to create HEIF image items\n"));
		e = GF_NOT_SUPPORTED;
		goto exit;
	}
	if (config_needed && !config_box && !((GF_AVCConfigurationBox *)config_box)->config) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error: Image type %s requires a missing configuration box\n", gf_4cc_to_str(item_type)));
		e = GF_BAD_PARAM;
		goto exit;
	}
	/* Get some images properties from the track data */
	e = gf_isom_get_visual_info(fsrc, imported_track, sample_desc_index, &w, &h);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error determining image size\n"));
		goto exit;
	}
	e = gf_isom_get_pixel_aspect_ratio(fsrc, imported_track, sample_desc_index, &hSpacing, &vSpacing);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Error determining image aspect ratio\n"));
		goto exit;
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
	if (is_cenc) {
		Bool Is_Encrypted;

		memset(&ipro, 0, sizeof(GF_ImageItemProtection));
		gf_isom_get_cenc_info(fsrc, imported_track, sample_desc_index, NULL, &ipro.scheme_type, &ipro.scheme_version);
		e = gf_isom_get_sample_cenc_info(fsrc, imported_track, sample_number, &Is_Encrypted, &ipro.crypt_byte_block, &ipro.skip_byte_block, &ipro.key_info, &ipro.key_info_size);
		if (e) goto exit;

		if (Is_Encrypted) {
			sai_size = sai_alloc_size;
			e = gf_isom_cenc_get_sample_aux_info(fsrc, imported_track, sample_number, sample_desc_index, NULL, &sai, &sai_size);
			if (e) goto exit;

			if (sai_size > sai_alloc_size)
				sai_alloc_size = sai_size;

			ipro.sai_data = sai;
			ipro.sai_data_size = sai_size;
			image_props->cenc_info = &ipro;

			if (is_first) {
				u32 i, nb_pssh = gf_isom_get_pssh_count(fsrc);
				for (i=0; i<nb_pssh; i++) {
					bin128 SystemID;
					u32 version;
					u32 KID_count;
					const bin128 *KIDs;
					const u8 *private_data;
					u32 private_data_size;

					gf_isom_get_pssh_info(fsrc, i+1, SystemID, &version, &KID_count, &KIDs, &private_data, &private_data_size);
					
					gf_cenc_set_pssh(movie, SystemID, version, KID_count, (bin128 *) KIDs, (u8 *) private_data, private_data_size, 2);
				}
			}

		} else {
			image_props->cenc_info = NULL;
		}
	}
	if (!item_id) {
		e = gf_isom_meta_get_next_item_id(movie, root_meta, meta_track_number, &item_id);
		if (e) goto exit;
	}
	if (image_props->use_reference) {
		if (image_props->sample_num) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("referring trackID %d sample %d as item %d\n", imported_track, sample_number, item_id));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("referring trackID %d sample at time %.3f as item %d\n", imported_track, (sample->DTS+sample->CTS_Offset)*1.0/timescale, item_id));
		}
		e = gf_isom_add_meta_item_sample_ref(movie, root_meta, meta_track_number, item_name, &item_id, item_type, NULL, NULL, image_props, imported_track, sample_number);
	} else {

		if (image_props->sample_num) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Adding sample %d as item %d\n", sample_number, item_id));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Adding sample at time %.3f as item %d\n", (sample->DTS+sample->CTS_Offset)*1.0/timescale, item_id));
		}
		e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, item_name, &item_id, item_type, NULL, NULL, image_props, sample->data, sample->dataLength, item_extent_refs);
	}

	image_props->cenc_info = NULL;

	if (reset_brands) {
		gf_isom_set_brand_info(movie, GF_ISOM_BRAND_MIF1, 0);
		gf_isom_reset_alt_brands(movie);

		// TODO Analyze configuration to determine the brand */
		//if (media_brand) {
		//	gf_isom_modify_alternate_brand(movie, media_brand, GF_TRUE);
		//}
	}

	
	if (neg_time)
		image_props->time = -1;

	if (!e && !image_props->sample_num && ((image_props->time<0) || image_props->end_time || image_props->step_time)) {
		if (image_props->end_time || image_props->step_time) {
			Double t = (Double) (sample->DTS + sample->CTS_Offset);
			t /= timescale;
			if (image_props->step_time) {
				t += image_props->step_time;
			} else {
				//step 1ms
				t += 0.001;
			}

			if ((image_props->end_time>0) && (t>image_props->end_time)) {
				goto exit;
			}
			image_props->time = t;
		}

		item_id=0;
		gf_isom_sample_del(&sample);
		if (config_box) {
			gf_isom_box_del(config_box);
			config_box = NULL;
		}
		is_first = GF_FALSE;
		if (sample_number >= gf_isom_get_sample_count(fsrc, imported_track)) return e;
		sample_number++;
		//avoid recursion this could get quite big
		goto import_next_sample;
	}

exit:
	if (sai) gf_free(sai);
	gf_isom_sample_del(&sample);
	if (config_box) gf_isom_box_del(config_box);
	image_props->cenc_info = orig_ipro;
	return e;
}

static
GF_Err gf_isom_iff_create_image_grid_item_internal(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, const char *item_name, u32 *item_id, GF_ImageItemProperties *image_props) {
	GF_Err e = GF_OK;
	u32 grid4cc = GF_4CC('g', 'r', 'i', 'd');
	GF_BitStream *grid_bs;
	if (image_props->num_grid_rows < 1 || image_props->num_grid_columns < 1 || image_props->num_grid_rows > 256 || image_props->num_grid_columns > 256) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Wrong grid parameters: %d, %d\n", image_props->num_grid_rows, image_props->num_grid_columns));
		return GF_BAD_PARAM;
	}
	if (image_props->width == 0 || image_props->height == 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("At least one grid dimension set to 0: %d, %d\n", image_props->width, image_props->height));
		return GF_BAD_PARAM;
	}
	grid_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!grid_bs) return GF_OUT_OF_MEM;
	gf_bs_write_u8(grid_bs, 0); //version
	gf_bs_write_u8(grid_bs, (image_props->width > 1<<16 || image_props->height > 1<<16) ? 1 : 0); // flags
	gf_bs_write_u8(grid_bs, image_props->num_grid_rows-1);
	gf_bs_write_u8(grid_bs, image_props->num_grid_columns-1);
	gf_bs_write_u16(grid_bs, image_props->width);
	gf_bs_write_u16(grid_bs, image_props->height);
	u8 *grid_data;
	u32 grid_data_size;
	gf_bs_get_content(grid_bs, &grid_data, &grid_data_size);
	e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, item_name, item_id, grid4cc, NULL, NULL, image_props, grid_data, grid_data_size, NULL);
	gf_free(grid_data);
	gf_bs_del(grid_bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_iff_create_image_grid_item(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props)
{
	return gf_isom_iff_create_image_grid_item_internal(movie, root_meta, meta_track_number, item_name, &item_id, image_props);
}

static GF_Err iff_create_auto_grid(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props)
{
	GF_Err e;
	GF_ImageItemProperties props;
	u32 w=0, h=0;
	u32 *imgs_ids=NULL;
	u32 nb_imgs=0, nb_src_imgs;
	u32 nb_cols, nb_rows;
	u32 last_valid_nb_cols;
	Bool first_pass = GF_TRUE;
	Double ar;
	u32 i, nb_items = gf_isom_get_meta_item_count(movie, root_meta, meta_track_number);
	for (i=0; i<nb_items; i++) {
		u32 an_item_id, item_type;
		gf_isom_get_meta_item_info(movie, root_meta, meta_track_number, i+1, &an_item_id, &item_type, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

		if (!item_id) continue;
		if (item_type==GF_ISOM_ITEM_TYPE_AUXI) continue;

		memset(&props, 0, sizeof(props));
		gf_isom_get_meta_image_props(movie, root_meta, meta_track_number, an_item_id, &props, NULL);
		if (!props.width || !props.height) continue;

		if (!w) w = props.width;
		if (!h) h = props.height;

		if ((w != props.width) || (h != props.height)) {
			if (imgs_ids) gf_free(imgs_ids);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("Auto grid can only be generated for images of the same size - try using `-add-image-grid`\n"));
			return GF_NOT_SUPPORTED;
		}
		imgs_ids = gf_realloc(imgs_ids, sizeof(u32) * (nb_imgs+1));
		if (!imgs_ids) return GF_OUT_OF_MEM;
		imgs_ids[nb_imgs] = an_item_id;
		nb_imgs++;
	}

	if (!nb_imgs || !w || !h) {
		if (imgs_ids) gf_free(imgs_ids);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("No image items found\n"));
		return GF_BAD_PARAM;
	}

	//try roughly the same aspect ratio
	if (image_props->auto_grid_ratio) {
		ar = image_props->auto_grid_ratio;
	} else {
		ar = w;
		ar /= h;
	}
	nb_src_imgs = nb_imgs;

recompute_grid:
	nb_cols=1;
	nb_rows=1;
	last_valid_nb_cols = 0;

	if (nb_imgs>1) {
		while (nb_cols < nb_imgs) {
			Double target_ar = ((Double) nb_cols) * w;
			nb_rows = nb_imgs / nb_cols;
			if (nb_rows * nb_cols != nb_imgs) {
				nb_cols++;
				continue;
			}
			target_ar /= ((Double)nb_rows) * h;
			if (target_ar >= ar) break;
			last_valid_nb_cols = nb_cols;
			nb_cols++;
		}
	}
	if ((nb_cols==nb_imgs) && last_valid_nb_cols) {
		nb_cols = last_valid_nb_cols;
		nb_rows = nb_imgs / nb_cols;
	}

	if (first_pass && (nb_imgs>1) && ((nb_cols==1) || (nb_rows==1) ) ) {
		if (nb_imgs % 2) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Not an even number of images, removing last item from grid\n"));
			nb_imgs--;
			first_pass = GF_FALSE;
			goto recompute_grid;
		}
	}

	memset(&props, 0, sizeof(props));
	if (image_props)
		memcpy(&props, image_props, sizeof(props));
	props.hidden = GF_FALSE;
	props.width = nb_cols * w;
	props.height = nb_rows * h;
	props.num_grid_rows = nb_rows;
	props.num_grid_columns = nb_cols;
	GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Grid: %u rows %u cols - size %ux%u pixels\n", nb_cols, nb_rows, props.width, props.height));

	e = gf_isom_iff_create_image_grid_item_internal(movie, root_meta, meta_track_number, item_name, &item_id, &props);
	if (e) {
		gf_free(imgs_ids);
		return e;
	}

	for (i=0; i<nb_imgs; i++) {
		e = gf_isom_meta_add_item_ref(movie, root_meta, meta_track_number, item_id, imgs_ids[i], GF_4CC('d','i','m','g'), NULL);
		if (e) goto exit;
	}

	//we might want an option to avoid this?
	//if (image_props->hidden)
	{
		GF_MetaBox *meta = gf_isom_get_meta(movie, root_meta, meta_track_number);
		for (i=0; i<nb_src_imgs; i++) {
			GF_ItemInfoEntryBox *iinf;
			u32 j=0;
			while ((iinf = (GF_ItemInfoEntryBox *)gf_list_enum(meta->item_infos->item_infos, &j))) {
				if (iinf->item_ID == imgs_ids[i]) {
					iinf->flags |= 0x1;
					break;
				}
			}
		}
		e = gf_isom_set_meta_primary_item(movie, root_meta, meta_track_number, item_id);
	}

exit:
	gf_free(imgs_ids);
	return e;
}

GF_EXPORT
GF_Err gf_isom_iff_create_image_overlay_item(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props) {
	u32 i;
	Bool use32bitFields = GF_FALSE;
	GF_Err e = GF_OK;
	u32 overlay4cc = GF_4CC('i', 'o', 'v', 'l');
	GF_BitStream *overlay_bs;
	if (image_props->overlay_count == 0 || image_props->width == 0 || image_props->height == 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Unusual overlay parameters: %d, %d, %d\n", image_props->overlay_count, image_props->width, image_props->height));
	}
	if (image_props->width <= 1<<16 || image_props->height <= 1<<16) {
		for (i=0; i< image_props->overlay_count; i++) {
			if ( image_props->overlay_offsets[i].horizontal > 1<<16 ||
				image_props->overlay_offsets[i].vertical > 1<<16) {
				use32bitFields = GF_TRUE;
				break;
			}
		}
	} else {
		use32bitFields = GF_TRUE;
	}
	overlay_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!overlay_bs) return GF_OUT_OF_MEM;
	gf_bs_write_u8(overlay_bs, 0); // version
	gf_bs_write_u8(overlay_bs, use32bitFields ? 1 : 0); // flags
	gf_bs_write_u16(overlay_bs, image_props->overlay_canvas_fill_value_r);
	gf_bs_write_u16(overlay_bs, image_props->overlay_canvas_fill_value_g);
	gf_bs_write_u16(overlay_bs, image_props->overlay_canvas_fill_value_b);
	gf_bs_write_u16(overlay_bs, image_props->overlay_canvas_fill_value_a);
	gf_bs_write_u16(overlay_bs, image_props->width);
	gf_bs_write_u16(overlay_bs, image_props->height);
	for (i = 0; i <image_props->overlay_count; i++) {
		gf_bs_write_u16(overlay_bs, image_props->overlay_offsets[i].horizontal);
		gf_bs_write_u16(overlay_bs, image_props->overlay_offsets[i].vertical);
	}
	u8 *overlay_data;
	u32 overlay_data_size;
	gf_bs_get_content(overlay_bs, &overlay_data, &overlay_data_size);
	e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, item_name, &item_id, overlay4cc, NULL, NULL, image_props, overlay_data, overlay_data_size, NULL);
	gf_free(overlay_data);
	gf_bs_del(overlay_bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_iff_create_image_identity_item(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props) {
	GF_Err e = GF_OK;
	u32 identity4cc = GF_4CC('i', 'd', 'e', 'n');
	if (image_props->width == 0 || image_props->height == 0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("At least one identity dimension set to 0: %d, %d\n", image_props->width, image_props->height));
	}
	e = gf_isom_add_meta_item_memory(movie, root_meta, meta_track_number, item_name, &item_id, identity4cc, NULL, NULL, image_props, NULL, 0, NULL);
	return e;
}

GF_EXPORT
GF_Err gf_isom_iff_create_image_item_from_track(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, u32 imported_track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs)
{

	if (image_props && image_props->auto_grid)
		return iff_create_auto_grid(movie, root_meta, meta_track_number, item_name, item_id, image_props);

 	return gf_isom_iff_create_image_item_from_track_internal(movie, root_meta, meta_track_number, imported_track, item_name, item_id, image_props, item_extent_refs, 1);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
