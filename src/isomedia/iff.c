/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef GPAC_DISABLE_ISOM

GF_Box *ispe_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ImageSpatialExtentsPropertyBox, GF_ISOM_BOX_TYPE_ISPE);
	return (GF_Box *)tmp;
}

void ispe_del(GF_Box *a)
{
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox *)a;
	gf_free(p);
}

GF_Err ispe_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

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
GF_Err ispe_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err ispe_Size(GF_Box *s)
{
	GF_Err e;
	GF_ImageSpatialExtentsPropertyBox *p = (GF_ImageSpatialExtentsPropertyBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (p->version == 0 && p->flags == 0) {
		p->size += 8;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for ispe box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *colr_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ColourInformationBox, GF_ISOM_BOX_TYPE_COLR);
	return (GF_Box *)tmp;
}

void colr_del(GF_Box *a)
{
	GF_ColourInformationBox *p = (GF_ColourInformationBox *)a;
	if (p->opaque) gf_free(p->opaque);
	gf_free(p);
}

GF_Err colr_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ColourInformationBox *p = (GF_ColourInformationBox *)s;

	p->colour_type = gf_bs_read_u32(bs);
	p->size -= 4;
	if (p->colour_type == GF_4CC('n','c','l','x')) {
		p->colour_primaries = gf_bs_read_u16(bs);
		p->transfer_characteristics = gf_bs_read_u16(bs);
		p->matrix_coefficients = gf_bs_read_u16(bs);
		p->full_range_flag = (gf_bs_read_u8(bs) & 0x80 ? GF_TRUE : GF_FALSE);
	} else {
		p->opaque = gf_malloc(sizeof(u8)* (u32) p->size);
		p->opaque_size = (u32) p->size;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("ICC colour profile not supported \n" ));
		gf_bs_read_data(bs, (char *) p->opaque, p->opaque_size);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err colr_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ColourInformationBox *p = (GF_ColourInformationBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	if (p->colour_type != GF_4CC('n','c','l','x')) {
		gf_bs_write_u32(bs, p->colour_type);
		gf_bs_write_data(bs, (char *)p->opaque, p->opaque_size);
	} else {
		gf_bs_write_u32(bs, p->colour_type);
		gf_bs_write_u16(bs, p->colour_primaries);
		gf_bs_write_u16(bs, p->transfer_characteristics);
		gf_bs_write_u16(bs, p->matrix_coefficients);
		gf_bs_write_u8(bs, (p->full_range_flag == GF_TRUE ? 0x80 : 0));
	}
	return GF_OK;
}

GF_Err colr_Size(GF_Box *s)
{
	GF_Err e;
	GF_ColourInformationBox *p = (GF_ColourInformationBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (p->colour_type != GF_4CC('n','c','l','x')) {
		p->size += 4 + p->opaque_size;
	} else {
		p->size += 11;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *pixi_New()
{
	ISOM_DECL_BOX_ALLOC(GF_PixelInformationPropertyBox, GF_ISOM_BOX_TYPE_PIXI);
	return (GF_Box *)tmp;
}

void pixi_del(GF_Box *a)
{
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox *)a;
	if (p->bits_per_channel) gf_free(p->bits_per_channel);
	gf_free(p);
}

GF_Err pixi_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i;
	GF_Err e;
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

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
GF_Err pixi_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err pixi_Size(GF_Box *s)
{
	GF_Err e;
	GF_PixelInformationPropertyBox *p = (GF_PixelInformationPropertyBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (p->version == 0 && p->flags == 0) {
		p->size += 1 + p->num_channels;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for pixi box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *rloc_New()
{
	ISOM_DECL_BOX_ALLOC(GF_RelativeLocationPropertyBox, GF_ISOM_BOX_TYPE_RLOC);
	return (GF_Box *)tmp;
}

void rloc_del(GF_Box *a)
{
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox *)a;
	gf_free(p);
}

GF_Err rloc_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox *)s;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

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
GF_Err rloc_Write(GF_Box *s, GF_BitStream *bs)
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

GF_Err rloc_Size(GF_Box *s)
{
	GF_Err e;
	GF_RelativeLocationPropertyBox *p = (GF_RelativeLocationPropertyBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
	if (p->version == 0 && p->flags == 0) {
		p->size += 8;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("version and flags for rloc box not supported" ));
		return GF_NOT_SUPPORTED;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *irot_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ImageRotationBox, GF_ISOM_BOX_TYPE_IROT);
	return (GF_Box *)tmp;
}

void irot_del(GF_Box *a)
{
	GF_ImageRotationBox *p = (GF_ImageRotationBox *)a;
	gf_free(p);
}

GF_Err irot_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_ImageRotationBox *p = (GF_ImageRotationBox *)s;
	p->angle = gf_bs_read_u8(bs) & 0x3;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err irot_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ImageRotationBox *p = (GF_ImageRotationBox*)s;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u8(bs, p->angle);
	return GF_OK;
}

GF_Err irot_Size(GF_Box *s)
{
	GF_Err e;
	GF_ImageRotationBox *p = (GF_ImageRotationBox*)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	p->size += 1;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipco_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertyContainerBox, GF_ISOM_BOX_TYPE_IPCO);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void ipco_del(GF_Box *s)
{
	GF_ItemPropertyContainerBox *p = (GF_ItemPropertyContainerBox *)s;
	gf_free(p);
}

GF_Err ipco_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, gf_isom_box_add_default);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ipco_Write(GF_Box *s, GF_BitStream *bs)
{
	if (!s) return GF_BAD_PARAM;
	return gf_isom_box_write_header(s, bs);
}

GF_Err ipco_Size(GF_Box *s)
{
	return gf_isom_box_get_size(s);
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *iprp_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertiesBox, GF_ISOM_BOX_TYPE_IPRP);
	tmp->other_boxes = gf_list_new();
	return (GF_Box *)tmp;
}

void iprp_del(GF_Box *s)
{
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	if (p->property_container) gf_isom_box_del((GF_Box *)p->property_container);
	gf_free(p);
}

static GF_Err iprp_AddBox(GF_Box *s, GF_Box *a)
{
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_IPCO:
		if (p->property_container) {
			return GF_ISOM_INVALID_FILE;
		}
		p->property_container = (GF_ItemPropertyContainerBox*)a;
		break;
	default:
		return gf_isom_box_add_default(s, a);
	}
	return GF_OK;
}

GF_Err iprp_Read(GF_Box *s, GF_BitStream *bs)
{
	return gf_isom_read_box_list(s, bs, iprp_AddBox);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err iprp_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	if (p->property_container) {
		e = gf_isom_box_write((GF_Box *) p->property_container, bs);
		if (e) return e;
	}
	return GF_OK;
}

GF_Err iprp_Size(GF_Box *s)
{
	GF_Err e;
	GF_ItemPropertiesBox *p = (GF_ItemPropertiesBox *)s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (p->property_container) {
		e = gf_isom_box_size((GF_Box *) p->property_container);
		if (e) return e;
		p->size += p->property_container->size;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Box *ipma_New()
{
	ISOM_DECL_BOX_ALLOC(GF_ItemPropertyAssociationBox, GF_ISOM_BOX_TYPE_IPMA);
	tmp->entries = gf_list_new();
	return (GF_Box *)tmp;
}

void ipma_del(GF_Box *a)
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

GF_Err ipma_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, j;
	GF_Err e;
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox *)s;
	u32 entry_count, association_count;

	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

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
GF_Err ipma_Write(GF_Box *s, GF_BitStream *bs)
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
				gf_bs_write_u16(bs, *ess << 15 | *prop_index);
			} else {
				gf_bs_write_u8(bs, *ess << 7 | *prop_index);
			}
		}
	}
	return GF_OK;
}

GF_Err ipma_Size(GF_Box *s)
{
	u32 i;
	GF_Err e;
	u32 entry_count, association_count;
	GF_ItemPropertyAssociationBox *p = (GF_ItemPropertyAssociationBox*)s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;
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

#endif /*GPAC_DISABLE_ISOM*/
