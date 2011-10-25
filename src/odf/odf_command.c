/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
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


#include <gpac/internal/odf_dev.h>


GF_Err gf_odf_parse_command(GF_BitStream *bs, GF_ODCom **com, u32 *com_size)
{
	u32 val, size, sizeHeader;
	u8 tag;
	GF_Err err;
	GF_ODCom *newCom;
	if (!bs) return GF_BAD_PARAM;

	*com_size = 0;

	//tag
	tag = gf_bs_read_int(bs, 8);
	sizeHeader = 1;
	
	//size
	size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		sizeHeader++;
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80 );
	*com_size = size;

	newCom = gf_odf_create_command(tag);
	if (! newCom) {
		*com = NULL;
		return GF_OUT_OF_MEM;
	}

	newCom->tag = tag;

	err = gf_odf_read_command(bs, newCom, *com_size);
	//little trick to handle lazy bitstreams that encode 
	//SizeOfInstance on a fix number of bytes
	//This nb of bytes is added in Read methods
	*com_size += sizeHeader - gf_odf_size_field_size(*com_size);
	*com = newCom;
	if (err) {
		gf_odf_delete_command(newCom);
		*com = NULL;
	}
	return err;
}


GF_ODCom *gf_odf_new_base_command()
{
	GF_BaseODCom *newCom = (GF_BaseODCom *) gf_malloc(sizeof(GF_BaseODCom));
	if (!newCom) return NULL;
	newCom->dataSize = 0;
	newCom->data = NULL;
	return (GF_ODCom *)newCom;
}
GF_Err gf_odf_del_base_command(GF_BaseODCom *bcRemove)
{
	if (! bcRemove) return GF_BAD_PARAM;
	if (bcRemove->data) gf_free(bcRemove->data);
	gf_free(bcRemove);
	return GF_OK;
}

GF_Err gf_odf_read_base_command(GF_BitStream *bs, GF_BaseODCom *bcRem, u32 gf_odf_size_command)
{
	if (! bcRem) return GF_BAD_PARAM;

	bcRem->dataSize = gf_odf_size_command;
	bcRem->data = (char *) gf_malloc(sizeof(char) * bcRem->dataSize);
	if (! bcRem->data) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, bcRem->data, bcRem->dataSize);
	return GF_OK;
}
GF_Err gf_odf_size_base_command(GF_BaseODCom *bcRem, u32 *outSize)
{
	if (!bcRem) return GF_BAD_PARAM;
	*outSize = bcRem->dataSize;
	return GF_OK;
}
GF_Err gf_odf_write_base_command(GF_BitStream *bs, GF_BaseODCom *bcRem)
{
	u32 size;
	GF_Err e;
	if (! bcRem) return GF_BAD_PARAM;

	e = gf_odf_size_base_command(bcRem, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, bcRem->tag, size);
	if (e) return e;
	gf_bs_write_data(bs, bcRem->data, bcRem->dataSize);
	return GF_OK;
}

GF_ODCom *gf_odf_new_od_remove()
{
	GF_ODRemove *newCom = (GF_ODRemove *) gf_malloc(sizeof(GF_ODRemove));
	if (!newCom) return NULL;
	newCom->NbODs = 0;
	newCom->OD_ID = NULL;
	newCom->tag = GF_ODF_OD_REMOVE_TAG;
	return (GF_ODCom *)newCom;
}
GF_Err gf_odf_del_od_remove(GF_ODRemove *ODRemove)
{
	if (! ODRemove) return GF_BAD_PARAM;
	if (ODRemove->OD_ID) gf_free(ODRemove->OD_ID);
	gf_free(ODRemove);
	return GF_OK;
}
GF_Err gf_odf_read_od_remove(GF_BitStream *bs, GF_ODRemove *odRem, u32 gf_odf_size_command)
{
	u32 i = 0, nbBits;
	if (! odRem) return GF_BAD_PARAM;

	odRem->NbODs = (u32 ) (gf_odf_size_command * 8) / 10;
	odRem->OD_ID = (u16 *) gf_malloc(sizeof(u16) * odRem->NbODs);
	if (! odRem->OD_ID) return GF_OUT_OF_MEM;

	for (i = 0; i < odRem->NbODs ; i++) {
		odRem->OD_ID[i] = gf_bs_read_int(bs, 10);
	}
	nbBits = odRem->NbODs * 10;
	//now we need to align !!!
	nbBits += gf_bs_align(bs);
	if (nbBits != (gf_odf_size_command * 8)) return GF_ODF_INVALID_COMMAND;
	return GF_OK;
}

GF_Err gf_odf_size_od_remove(GF_ODRemove *odRem, u32 *outSize)
{
	u32 size;
	if (!odRem) return GF_BAD_PARAM;
	
	size = 10 * odRem->NbODs;
	*outSize = 0;
	*outSize = size/8;
	if (*outSize * 8 != size) *outSize += 1;
	return GF_OK;
}

GF_Err gf_odf_write_od_remove(GF_BitStream *bs, GF_ODRemove *odRem)
{
	GF_Err e;
	u32 size, i;
	if (! odRem) return GF_BAD_PARAM;

	e = gf_odf_size_od_remove(odRem, &size);
	assert(e == GF_OK);
	e = gf_odf_write_base_descriptor(bs, odRem->tag, size);
	assert(e == GF_OK);

	for (i = 0; i < odRem->NbODs; i++) {
		gf_bs_write_int(bs, odRem->OD_ID[i], 10);
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}



GF_ODCom *gf_odf_new_od_update()
{
	GF_ODUpdate *newCom = (GF_ODUpdate *) gf_malloc(sizeof(GF_ODUpdate));
	if (!newCom) return NULL;
	
	newCom->objectDescriptors = gf_list_new();
	if (! newCom->objectDescriptors) {
		gf_free(newCom);
		return NULL;
	}
	newCom->tag = GF_ODF_OD_UPDATE_TAG;
	return (GF_ODCom *)newCom;
}

GF_Err gf_odf_del_od_update(GF_ODUpdate *ODUpdate)
{
	GF_Err e;
	if (! ODUpdate) return GF_BAD_PARAM;
	while (gf_list_count(ODUpdate->objectDescriptors)) {
		GF_Descriptor *tmp = (GF_Descriptor*)gf_list_get(ODUpdate->objectDescriptors, 0);
		e = gf_odf_delete_descriptor(tmp);
		if (e) return e;
		e = gf_list_rem(ODUpdate->objectDescriptors, 0);
		if (e) return e;
	}
	gf_list_del(ODUpdate->objectDescriptors);
	gf_free(ODUpdate);
	return GF_OK;
}



GF_Err AddToODUpdate(GF_ODUpdate *odUp, GF_Descriptor *desc)
{
	if (! odUp) return GF_BAD_PARAM;
	if (! desc) return GF_OK;

	switch (desc->tag) {
	case GF_ODF_OD_TAG:
	case GF_ODF_IOD_TAG:
	case GF_ODF_ISOM_IOD_TAG:
	case GF_ODF_ISOM_OD_TAG:
		return gf_list_add(odUp->objectDescriptors, desc);

	default:
		gf_odf_delete_descriptor(desc);
		return GF_OK;
	}
}

GF_Err gf_odf_read_od_update(GF_BitStream *bs, GF_ODUpdate *odUp, u32 gf_odf_size_command)
{
	GF_Descriptor *tmp;
	GF_Err e = GF_OK;
	u32 tmpSize = 0, nbBytes = 0;
	if (! odUp) return GF_BAD_PARAM;

	while (nbBytes < gf_odf_size_command) {
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		e = AddToODUpdate(odUp, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	//OD commands are aligned
	gf_bs_align(bs);
	if (nbBytes != gf_odf_size_command) return GF_ODF_INVALID_COMMAND;
	return e;
}
GF_Err gf_odf_size_od_update(GF_ODUpdate *odUp, u32 *outSize)
{
	GF_Descriptor *tmp;
	u32 i, tmpSize;
	if (!odUp) return GF_BAD_PARAM;

	*outSize = 0;
	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(odUp->objectDescriptors, &i))) {
		gf_odf_size_descriptor(tmp, &tmpSize);
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	return GF_OK;
}
GF_Err gf_odf_write_od_update(GF_BitStream *bs, GF_ODUpdate *odUp)
{
	GF_Err e;
	GF_Descriptor *tmp;
	u32 size, i;
	if (! odUp) return GF_BAD_PARAM;

	e = gf_odf_size_od_update(odUp, &size);
	if (e) return e;
	gf_odf_write_base_descriptor(bs, odUp->tag, size);
	if (e) return e;

	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(odUp->objectDescriptors, &i))) {
		e = gf_odf_write_descriptor(bs, tmp);
		if (e) return e;
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}


GF_ODCom *gf_odf_new_esd_update()
{
	GF_ESDUpdate *newCom = (GF_ESDUpdate *) gf_malloc(sizeof(GF_ESDUpdate));
	if (!newCom) return NULL;
	
	newCom->ESDescriptors = gf_list_new();
	if (! newCom->ESDescriptors) {
		gf_free(newCom);
		return NULL;
	}
	newCom->tag = GF_ODF_ESD_UPDATE_TAG;
	return (GF_ODCom *)newCom;
}

GF_Err gf_odf_del_esd_update(GF_ESDUpdate *ESDUpdate)
{
	GF_Err e;
	if (! ESDUpdate) return GF_BAD_PARAM;
	while (gf_list_count(ESDUpdate->ESDescriptors)) {
		GF_Descriptor *tmp = (GF_Descriptor*)gf_list_get(ESDUpdate->ESDescriptors, 0);
		e = gf_odf_delete_descriptor(tmp);
		if (e) return e;
		e = gf_list_rem(ESDUpdate->ESDescriptors, 0);
		if (e) return e;
	}
	gf_list_del(ESDUpdate->ESDescriptors);
	gf_free(ESDUpdate);
	return GF_OK;
}

GF_Err AddToESDUpdate(GF_ESDUpdate *esdUp, GF_Descriptor *desc)
{
	if (! esdUp) return GF_BAD_PARAM;
	if (!desc) return GF_OK;

	switch (desc->tag) {
	case GF_ODF_ESD_TAG:
	case GF_ODF_ESD_REF_TAG:
		return gf_list_add(esdUp->ESDescriptors, desc);

	default:
		gf_odf_delete_descriptor(desc);
		return GF_OK;
	}
}

GF_Err gf_odf_read_esd_update(GF_BitStream *bs, GF_ESDUpdate *esdUp, u32 gf_odf_size_command)
{
	GF_Descriptor *tmp;
	u32 tmpSize = 0, nbBits = 0;
	GF_Err e = GF_OK;
	if (! esdUp) return GF_BAD_PARAM;

	esdUp->ODID = gf_bs_read_int(bs, 10);
	nbBits += 10;
	//very tricky, we're at the bit level here...
	while (1) {
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		e = AddToESDUpdate(esdUp, tmp);
		if (e) return e;
		nbBits += ( tmpSize + gf_odf_size_field_size(tmpSize) ) * 8;
		//our com is aligned, so nbBits is between (gf_odf_size_command-1)*8 and gf_odf_size_command*8
		if ( ( (nbBits >(gf_odf_size_command-1)*8) && (nbBits <= gf_odf_size_command * 8)) 
			|| (nbBits > gf_odf_size_command*8) ) {	//this one is a security break
			break;
		}
	}
	if (nbBits > gf_odf_size_command * 8) return GF_ODF_INVALID_COMMAND;
	//Align our bitstream
	nbBits += gf_bs_align(bs);
	if (nbBits != gf_odf_size_command *8) return GF_ODF_INVALID_COMMAND;
	return e;
}



GF_Err gf_odf_size_esd_update(GF_ESDUpdate *esdUp, u32 *outSize)
{
	u32 i, BitSize, tmpSize;
	GF_Descriptor *tmp;
	if (!esdUp) return GF_BAD_PARAM;

	*outSize = 0;
	BitSize = 10;
	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(esdUp->ESDescriptors, &i))) {
		gf_odf_size_descriptor(tmp, &tmpSize);
		BitSize += ( tmpSize + gf_odf_size_field_size(tmpSize) ) * 8;
	}
	while ((s32) BitSize > 0) {
		BitSize -= 8;
		*outSize += 1;
	}
	return GF_OK;
}
GF_Err gf_odf_write_esd_update(GF_BitStream *bs, GF_ESDUpdate *esdUp)
{
	GF_Descriptor *tmp;
	GF_Err e;
	u32 size, i;
	if (! esdUp) return GF_BAD_PARAM;

	e = gf_odf_size_esd_update(esdUp, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, esdUp->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, esdUp->ODID, 10);
	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(esdUp->ESDescriptors, &i))) {
		e = gf_odf_write_descriptor(bs, tmp);
		if (e) return e;
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}


GF_ODCom *gf_odf_new_esd_remove()
{
	GF_ESDRemove *newCom = (GF_ESDRemove *) gf_malloc(sizeof(GF_ESDRemove));
	if (!newCom) return NULL;
	newCom->NbESDs = 0;
	newCom->ES_ID = NULL;
	newCom->tag = GF_ODF_ESD_REMOVE_TAG;
	return (GF_ODCom *)newCom;
}

GF_Err gf_odf_del_esd_remove(GF_ESDRemove *ESDRemove)
{
	if (! ESDRemove) return GF_BAD_PARAM;
	if (ESDRemove->ES_ID) gf_free(ESDRemove->ES_ID);
	gf_free(ESDRemove);
	return GF_OK;
}


GF_Err gf_odf_read_esd_remove(GF_BitStream *bs, GF_ESDRemove *esdRem, u32 gf_odf_size_command)
{
	u32 i = 0;
	if (! esdRem) return GF_BAD_PARAM;

	esdRem->ODID = gf_bs_read_int(bs, 10);
	/*aligned = */gf_bs_read_int(bs, 6);		//aligned

	//we have gf_odf_size_command - 2 bytes left, and this is our ES_ID[1...255]
	//this works because OD commands are aligned
	if (gf_odf_size_command < 2) return GF_ODF_INVALID_DESCRIPTOR;
	if (gf_odf_size_command == 2) {
		esdRem->NbESDs = 0;
		esdRem->ES_ID = NULL;
		return GF_OK;
	}
	esdRem->NbESDs = (gf_odf_size_command - 2) / 2;
	esdRem->ES_ID = (u16 *) gf_malloc(sizeof(u16) * esdRem->NbESDs);
	if (! esdRem->ES_ID) return GF_OUT_OF_MEM;
	for (i = 0; i < esdRem->NbESDs ; i++) {
		esdRem->ES_ID[i] = gf_bs_read_int(bs, 16);
	}
	//OD commands are aligned (but we should already be aligned....
	/*nbBits = */gf_bs_align(bs);
	return GF_OK;
}

GF_Err gf_odf_size_esd_remove(GF_ESDRemove *esdRem, u32 *outSize)
{
	if (!esdRem) return GF_BAD_PARAM;
	*outSize = 2 + 2 * esdRem->NbESDs;
	return GF_OK;
}
GF_Err gf_odf_write_esd_remove(GF_BitStream *bs, GF_ESDRemove *esdRem)
{
	GF_Err e;
	u32 size, i;
	if (! esdRem) return GF_BAD_PARAM;

	e = gf_odf_size_esd_remove(esdRem, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, esdRem->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, esdRem->ODID, 10);
	gf_bs_write_int(bs, 0, 6);		//aligned
	for (i = 0; i < esdRem->NbESDs ; i++) {
		gf_bs_write_int(bs, esdRem->ES_ID[i], 16);
	}
	//OD commands are aligned (but we are already aligned....
	gf_bs_align(bs);
	return GF_OK;
}


GF_ODCom *gf_odf_new_ipmp_remove()
{
	GF_IPMPRemove *newCom = (GF_IPMPRemove *) gf_malloc(sizeof(GF_IPMPRemove));
	if (!newCom) return NULL;
	newCom->IPMPDescID  =NULL;
	newCom->NbIPMPDs = 0;
	newCom->tag = GF_ODF_IPMP_REMOVE_TAG;
	return (GF_ODCom *)newCom;
}

GF_Err gf_odf_del_ipmp_remove(GF_IPMPRemove *IPMPDRemove)
{
	if (! IPMPDRemove) return GF_BAD_PARAM;
	if (IPMPDRemove->IPMPDescID) gf_free(IPMPDRemove->IPMPDescID);
	gf_free(IPMPDRemove);
	return GF_OK;
}

GF_Err gf_odf_read_ipmp_remove(GF_BitStream *bs, GF_IPMPRemove *ipmpRem, u32 gf_odf_size_command)
{
	u32 i;
	if (! ipmpRem) return GF_BAD_PARAM;

	//we have gf_odf_size_command bytes left, and this is our IPMPD_ID[1...255]
	//this works because OD commands are aligned
	if (!gf_odf_size_command) return GF_OK;

	ipmpRem->NbIPMPDs = gf_odf_size_command;
	ipmpRem->IPMPDescID = (u8 *) gf_malloc(sizeof(u8) * ipmpRem->NbIPMPDs);
	if (! ipmpRem->IPMPDescID) return GF_OUT_OF_MEM;

	for (i = 0; i < ipmpRem->NbIPMPDs; i++) {
		ipmpRem->IPMPDescID[i] = gf_bs_read_int(bs, 8);
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}

GF_Err gf_odf_size_ipmp_remove(GF_IPMPRemove *ipmpRem, u32 *outSize)
{
	if (!ipmpRem) return GF_BAD_PARAM;

	*outSize = ipmpRem->NbIPMPDs;
	return GF_OK;
}
GF_Err gf_odf_write_ipmp_remove(GF_BitStream *bs, GF_IPMPRemove *ipmpRem)
{
	GF_Err e;
	u32 size, i;
	if (! ipmpRem) return GF_BAD_PARAM;

	e = gf_odf_size_ipmp_remove(ipmpRem, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmpRem->tag, size);
	if (e) return e;

	for (i = 0; i < ipmpRem->NbIPMPDs; i++) {
		gf_bs_write_int(bs, ipmpRem->IPMPDescID[i], 8);
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}

GF_ODCom *gf_odf_new_ipmp_update()
{
	GF_IPMPUpdate *newCom = (GF_IPMPUpdate *) gf_malloc(sizeof(GF_IPMPUpdate));
	if (!newCom) return NULL;
	newCom->IPMPDescList = gf_list_new();
	if (! newCom->IPMPDescList) {
		gf_free(newCom);
		return NULL;
	}
	newCom->tag = GF_ODF_IPMP_UPDATE_TAG;
	return (GF_ODCom *)newCom;
}

GF_Err gf_odf_del_ipmp_update(GF_IPMPUpdate *IPMPDUpdate)
{
	GF_Err e;
	if (! IPMPDUpdate) return GF_BAD_PARAM;
	while (gf_list_count(IPMPDUpdate->IPMPDescList)) {
		GF_Descriptor *tmp = (GF_Descriptor*)gf_list_get(IPMPDUpdate->IPMPDescList, 0);
		e = gf_odf_delete_descriptor(tmp);
		assert(e == GF_OK);
		e = gf_list_rem(IPMPDUpdate->IPMPDescList, 0);
		assert(e == GF_OK);
	}
	gf_list_del(IPMPDUpdate->IPMPDescList);
	gf_free(IPMPDUpdate);
	return GF_OK;
}

GF_Err AddToIPMPDUpdate(GF_IPMPUpdate *ipmpUp, GF_Descriptor *desc)
{
	if (! ipmpUp) return GF_BAD_PARAM;
	if (!desc) return GF_OK;

	switch (desc->tag) {
	case GF_ODF_IPMP_TAG:
		return gf_list_add(ipmpUp->IPMPDescList, desc);
	default:
		gf_odf_delete_descriptor(desc);
		return GF_OK;
	}
}

GF_Err gf_odf_read_ipmp_update(GF_BitStream *bs, GF_IPMPUpdate *ipmpUp, u32 gf_odf_size_command)
{
	GF_Descriptor *tmp;
	u32 tmpSize = 0, nbBytes = 0;
	GF_Err e = GF_OK;
	if (! ipmpUp) return GF_BAD_PARAM;

	while (nbBytes < gf_odf_size_command) {
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		e = AddToIPMPDUpdate(ipmpUp, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	//OD commands are aligned
	gf_bs_align(bs);
	if (nbBytes != gf_odf_size_command) return GF_ODF_INVALID_COMMAND;
	return e;
}


GF_Err gf_odf_size_ipmp_update(GF_IPMPUpdate *ipmpUp, u32 *outSize)
{
	GF_Descriptor *tmp;
	u32 i, tmpSize;
	if (!ipmpUp) return GF_BAD_PARAM;

	*outSize = 0;
	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(ipmpUp->IPMPDescList, &i))) {
		gf_odf_size_descriptor(tmp, &tmpSize);
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	return GF_OK;
}
GF_Err gf_odf_write_ipmp_update(GF_BitStream *bs, GF_IPMPUpdate *ipmpUp)
{
	GF_Err e;
	GF_Descriptor *tmp;
	u32 size, i;
	if (! ipmpUp) return GF_BAD_PARAM;

	e = gf_odf_size_ipmp_update(ipmpUp, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmpUp->tag, size);
	if (e) return e;

	i=0;
	while ((tmp = (GF_Descriptor *)gf_list_enum(ipmpUp->IPMPDescList, &i))) {
		e = gf_odf_write_descriptor(bs, tmp);
		if (e) return e;
	}
	//OD commands are aligned
	gf_bs_align(bs);
	return GF_OK;
}
