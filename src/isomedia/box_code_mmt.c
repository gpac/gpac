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

void mmpu_del(GF_Box *s)
{
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	if (ptr == NULL) return;
	if(ptr->asset_id_value)gf_free(ptr->asset_id_value);
	gf_free(ptr);
}

GF_Box *mmpu_New()
{
	ISOM_DECL_BOX_ALLOC(GF_MmtMmpuBox, GF_ISOM_BOX_TYPE_MMPU);
	return (GF_Box *)tmp;
}

GF_Err mmpu_Read(GF_Box *s,GF_BitStream *bs)
{
	GF_Err e;
	u32 p;

	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	e = gf_isom_full_box_read(s, bs);
	if (e) return e;

	ptr->is_complete=gf_bs_read_int(bs,1);
	ptr->is_adc_present=gf_bs_read_int(bs, 1);
	ptr->reserved=gf_bs_read_int(bs,6);
	ptr->mpu_sequence_number=gf_bs_read_u32(bs);
	ptr->asset_id_scheme=gf_bs_read_u32(bs);
	ptr->asset_id_length=gf_bs_read_u32(bs);
	for(p=0;p<ptr->asset_id_length;p++)
		gf_bs_write_u8(bs, ptr->asset_id_value[p]);

	return GF_OK;
}

GF_Err mmpu_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	int p;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_int(bs, ptr->is_complete, 1);
	gf_bs_write_int(bs, ptr->is_adc_present, 1);
	gf_bs_write_int(bs, ptr->reserved, 6);

	gf_bs_write_u32(bs, ptr->mpu_sequence_number);
	gf_bs_write_u32(bs, ptr->asset_id_scheme);
	gf_bs_write_u32(bs, ptr->asset_id_length);
	for(p=0;p<ptr->asset_id_length;p++)
		gf_bs_write_u8(bs, ptr->asset_id_value[p]);

	return GF_OK;
}

GF_Err mmpu_Size(GF_Box *s)
{
	GF_Err e;
	u32 size;
	GF_MmtMmpuBox *ptr = (GF_MmtMmpuBox*) s;
	e = gf_isom_full_box_get_size(s);
	if (e) return e;

	ptr->size += 13;
	for (size=0; size<ptr->asset_id_length; size++) {
		ptr->size++;
	}

	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM*/

