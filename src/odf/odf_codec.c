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

/************************************************************
		Object GF_Descriptor Codec Functions
************************************************************/

GF_EXPORT
GF_ODCodec *gf_odf_codec_new()
{
	GF_ODCodec *codec;
	GF_List *comList;

	comList = gf_list_new();
	if (!comList) return NULL;
	
	codec = (GF_ODCodec *) gf_malloc(sizeof(GF_ODCodec));
	if (!codec) {
		gf_list_del(comList);
		return NULL;
	}
	//the bitstream is always NULL. It is created on the fly for access unit processing only
	codec->bs = NULL;
	codec->CommandList = comList;
	return codec;
}

GF_EXPORT
void gf_odf_codec_del(GF_ODCodec *codec)
{
	if (!codec) return;

	while (gf_list_count(codec->CommandList)) {
		GF_ODCom *com = (GF_ODCom *)gf_list_get(codec->CommandList, 0);
		gf_odf_delete_command(com);
		gf_list_rem(codec->CommandList, 0);
	}
	gf_list_del(codec->CommandList);
	if (codec->bs) gf_bs_del(codec->bs);
	gf_free(codec);
}


/************************************************************
		Codec Encoder Functions
************************************************************/

GF_EXPORT
GF_Err gf_odf_codec_add_com(GF_ODCodec *codec, GF_ODCom *command)
{
	if (!codec || !command) return GF_BAD_PARAM;
	return gf_list_add(codec->CommandList, command);
}

GF_EXPORT
GF_Err gf_odf_codec_encode(GF_ODCodec *codec, u32 cleanup_type)
{
	GF_ODCom *com;
	GF_Err e = GF_OK;
	u32 i;

	if (!codec) return GF_BAD_PARAM;
	
	//check our bitstream: if existing, this means the previous encoded AU was not retrieved
	//we DON'T allow that
	if (codec->bs) return GF_BAD_PARAM;
	codec->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!codec->bs) return GF_OUT_OF_MEM;

	/*encode each command*/
	i = 0;
	while ((com = (GF_ODCom *)gf_list_enum(codec->CommandList, &i))) {
		e = gf_odf_write_command(codec->bs, com); if (e) goto err_exit;
		//don't forget OD Commands are aligned...
		gf_bs_align(codec->bs);
	}

//if an error occurs, delete the GF_BitStream and empty the codec
err_exit:
	if (e) {
		gf_bs_del(codec->bs);
		codec->bs = NULL;
	}
	if (cleanup_type==1) {
		while (gf_list_count(codec->CommandList)) {
			com = (GF_ODCom *)gf_list_get(codec->CommandList, 0);
			gf_odf_delete_command(com);
			gf_list_rem(codec->CommandList, 0);
		}
	}
	if (cleanup_type==0) {
		gf_list_reset(codec->CommandList);
	}
	return e;
}

GF_EXPORT
GF_Err gf_odf_codec_get_au(GF_ODCodec *codec, char **outAU, u32 *au_length)
{
	if (!codec || !codec->bs || !outAU || *outAU) return GF_BAD_PARAM;
	gf_bs_get_content(codec->bs, outAU, au_length);
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	return GF_OK;
}



/************************************************************
		Codec Decoder Functions
************************************************************/

GF_EXPORT
GF_Err gf_odf_codec_set_au(GF_ODCodec *codec, const char *au, u32 au_length)
{
	if (!codec ) return GF_BAD_PARAM;
	if (!au || !au_length) return GF_OK;

	//if the command list is not empty, this is an error
	if (gf_list_count(codec->CommandList)) return GF_BAD_PARAM;

	//the bitStream should not be here
	if (codec->bs) return GF_BAD_PARAM;
	
	codec->bs = gf_bs_new(au, (u64) au_length, (unsigned char)GF_BITSTREAM_READ);
	if (!codec->bs) return GF_OUT_OF_MEM;
	return GF_OK;	
}


GF_EXPORT
GF_Err gf_odf_codec_decode(GF_ODCodec *codec)
{
	GF_Err e = GF_OK;
	u32 size = 0, comSize, bufSize;
	GF_ODCom *com;

	if (!codec || !codec->bs) return GF_BAD_PARAM;

	bufSize = (u32) gf_bs_available(codec->bs);
	while (size < bufSize) {
		e =	gf_odf_parse_command(codec->bs, &com, &comSize); if (e) goto err_exit;
		gf_list_add(codec->CommandList, com);
		size += comSize + gf_odf_size_field_size(comSize);
		//OD Commands are aligned
		gf_bs_align(codec->bs);
	}
	//then delete our bitstream
	gf_bs_del(codec->bs);
	codec->bs = NULL;
	if (size != bufSize) {
		e = GF_ODF_INVALID_COMMAND;
		goto err_exit;
	}
	return e;

err_exit:
	if (codec->bs) {
		gf_bs_del(codec->bs);
		codec->bs = NULL;
	}
	while (gf_list_count(codec->CommandList)) {
		com = (GF_ODCom*)gf_list_get(codec->CommandList, 0);
		gf_odf_delete_command(com);
		gf_list_rem(codec->CommandList, 0);
	}
	return e;
}

//get the first command in the codec and remove the entry
GF_EXPORT
GF_ODCom *gf_odf_codec_get_com(GF_ODCodec *codec)
{
	GF_ODCom *com;
	if (!codec || codec->bs) return NULL;
	com = (GF_ODCom*)gf_list_get(codec->CommandList, 0);
	if (com) gf_list_rem(codec->CommandList, 0);
	return com;
}



/************************************************************
		OD Commands Functions
************************************************************/

//some easy way to get an OD GF_ODCom...
GF_EXPORT
GF_ODCom *gf_odf_com_new(u8 tag)
{
	GF_ODCom *newcom;

	newcom = gf_odf_create_command(tag);
	newcom->tag = tag;
	return (GF_ODCom *)newcom;
}

//	... and to delete it
GF_EXPORT
GF_Err gf_odf_com_del(GF_ODCom **com)
{
	GF_Err e;
	e = gf_odf_delete_command(*com);
	*com = NULL;
	return e;
}


/************************************************************
		Object Descriptors Functions
************************************************************/

//some easy way to get an mpeg4 descriptor ...
GF_EXPORT
GF_Descriptor *gf_odf_desc_new(u8 tag)
{
	GF_Descriptor *newdesc;
	newdesc = gf_odf_create_descriptor(tag);
	newdesc->tag = tag;
	return (GF_Descriptor *)newdesc;
}

//	... and to delete it
GF_EXPORT
void gf_odf_desc_del(GF_Descriptor *desc)
{
	if (desc) gf_odf_delete_descriptor(desc);
}

//this functions will destroy the descriptors in a list but not the list
GF_EXPORT
GF_Err gf_odf_desc_list_del(GF_List *descList)
{
	GF_Err e;
	GF_Descriptor *tmp;

	if (! descList) return GF_BAD_PARAM;

	while (gf_list_count(descList)) {
		tmp = (GF_Descriptor*)gf_list_get(descList, 0);
		gf_list_rem(descList, 0);
		e = gf_odf_delete_descriptor(tmp);
		if (e) return e;
	}
	return GF_OK;
}



GF_EXPORT
GF_ESD *gf_odf_desc_esd_new(u32 sl_predefined)
{
	GF_ESD *esd;
	esd = (GF_ESD *) gf_odf_desc_new(GF_ODF_ESD_TAG);
	esd->decoderConfig = (GF_DecoderConfig *) gf_odf_desc_new(GF_ODF_DCD_TAG);
	esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	esd->slConfig = (GF_SLConfig *) gf_odf_new_slc((u8) sl_predefined);
	return esd;
}


//use this function to decode a standalone descriptor
//the desc MUST be formatted with tag and size field!!!
GF_EXPORT
GF_Err gf_odf_desc_read(char *raw_desc, u32 descSize, GF_Descriptor * *outDesc)
{
	GF_Err e;
	u32 size;
	GF_BitStream *bs;

	if (!raw_desc || !descSize) return GF_BAD_PARAM;

	bs = gf_bs_new(raw_desc, (u64) descSize, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;

	size = 0;
	e = gf_odf_parse_descriptor(bs, outDesc, &size);
	//the size dosn't have the header in it
	size += gf_odf_size_field_size(size);
/*
	if (size != descSize) {
		if (*outDesc) gf_odf_delete_descriptor(*outDesc);
		*outDesc = NULL;
		e = GF_ODF_INVALID_DESCRIPTOR;
	}
*/

	gf_bs_del(bs);
	return e;
}

//use this function to encode a standalone descriptor
//the desc will be formatted with tag and size field
GF_EXPORT
GF_Err gf_odf_desc_write(GF_Descriptor *desc, char **outEncDesc, u32 *outSize)
{
	GF_Err e;
	GF_BitStream *bs;

	if (!desc || !outEncDesc || !outSize) return GF_BAD_PARAM;

	*outEncDesc = NULL;
	*outSize = 0;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_OUT_OF_MEM;
	//then encode our desc...
	e = gf_odf_write_descriptor(bs, desc);
	if (e) {
		gf_bs_del(bs);
		return e;
	}
	//then get the content from our bitstream
	gf_bs_get_content(bs, outEncDesc, outSize);
	gf_bs_del(bs);
	return GF_OK;
}

//use this function to get the size of a standalone descriptor
GF_EXPORT
u32 gf_odf_desc_size(GF_Descriptor *desc)
{
	u32 descSize;
	GF_Err e;
	
	if (!desc) return GF_BAD_PARAM;
	//get the descriptor length
	e = gf_odf_size_descriptor(desc, &descSize);
	if (e) return 0;
	//add the header length
	descSize += gf_odf_size_field_size(descSize);
	return descSize;

}

//this is usefull to duplicate on the fly a descriptor (mainly for authoring purposes) 
GF_EXPORT
GF_Err gf_odf_desc_copy(GF_Descriptor *inDesc, GF_Descriptor **outDesc)
{
	GF_Err e;
	char *desc;
	u32 size;
	
	//warning: here we get some data allocated
	e = gf_odf_desc_write(inDesc, &desc, &size);
	if (e) return e;
	e = gf_odf_desc_read(desc, size, outDesc);
	gf_free(desc);
	return e;
}

/************************************************************
		Object Descriptors Edit Functions
************************************************************/

//This functions handles internally what desc can be added to another desc
//and adds it. NO DUPLICATION of the descriptor, so
//once a desc is added to its parent, destroying the parent WILL destroy this desc
GF_EXPORT
GF_Err gf_odf_desc_add_desc(GF_Descriptor *parentDesc, GF_Descriptor *newDesc)
{
	GF_DecoderConfig *dcd;

	//our ADD definition
	GF_Err AddDescriptorToOD(GF_ObjectDescriptor *od, GF_Descriptor *desc);
	GF_Err AddDescriptorToIOD(GF_InitialObjectDescriptor *iod, GF_Descriptor *desc);
	GF_Err AddDescriptorToESD(GF_ESD *esd, GF_Descriptor *desc);
	GF_Err AddDescriptorToIsomIOD(GF_IsomInitialObjectDescriptor *iod, GF_Descriptor *desc);
	GF_Err AddDescriptorToIsomOD(GF_IsomObjectDescriptor *od, GF_Descriptor *desc);
	
	if (!parentDesc || !newDesc) return GF_BAD_PARAM;

	switch (parentDesc->tag) {
	//these are container descriptors
	case GF_ODF_OD_TAG:
		return AddDescriptorToOD((GF_ObjectDescriptor *)parentDesc, newDesc);
	case GF_ODF_IOD_TAG:
		return AddDescriptorToIOD((GF_InitialObjectDescriptor *)parentDesc, newDesc);
	case GF_ODF_ESD_TAG:
		return AddDescriptorToESD((GF_ESD *)parentDesc, newDesc);
	case GF_ODF_DCD_TAG:
		dcd = (GF_DecoderConfig *)parentDesc;
		if ((newDesc->tag == GF_ODF_DSI_TAG) 
			|| (newDesc->tag == GF_ODF_BIFS_CFG_TAG)
			|| (newDesc->tag == GF_ODF_UI_CFG_TAG)
			|| (newDesc->tag == GF_ODF_TEXT_CFG_TAG)
			) {
			if (dcd->decoderSpecificInfo) return GF_ODF_FORBIDDEN_DESCRIPTOR;
			dcd->decoderSpecificInfo = (GF_DefaultDescriptor *) newDesc;
			return GF_OK;
		} else if (newDesc->tag == GF_ODF_EXT_PL_TAG) {
			return gf_list_add(dcd->profileLevelIndicationIndexDescriptor, newDesc);
		}
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	case GF_ODF_TEXT_CFG_TAG:
		if (newDesc->tag != GF_ODF_TX3G_TAG) return GF_ODF_FORBIDDEN_DESCRIPTOR;
		return gf_list_add(((GF_TextConfig *)parentDesc)->sample_descriptions, newDesc);

	case GF_ODF_QOS_TAG:
		return GF_BAD_PARAM;

	//MP4 File Format tags
	case GF_ODF_ISOM_IOD_TAG:
		return AddDescriptorToIsomIOD((GF_IsomInitialObjectDescriptor *)parentDesc, newDesc);
	case GF_ODF_ISOM_OD_TAG:
		return AddDescriptorToIsomOD((GF_IsomObjectDescriptor *)parentDesc, newDesc);

	case GF_ODF_IPMP_TL_TAG:
		if (newDesc->tag!=GF_ODF_IPMP_TOOL_TAG) return GF_BAD_PARAM;
		return gf_list_add(((GF_IPMP_ToolList *)parentDesc)->ipmp_tools, newDesc);

	case GF_ODF_BIFS_CFG_TAG:
	{
		GF_BIFSConfig *cfg = (GF_BIFSConfig *)parentDesc;
		if (newDesc->tag!=GF_ODF_ELEM_MASK_TAG) return GF_BAD_PARAM;
		if (!cfg->elementaryMasks) cfg->elementaryMasks = gf_list_new();
		return gf_list_add(cfg->elementaryMasks, newDesc);
	}
	default:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;
	}
}



/*****************************************************************************************
		Since IPMP V2, we introduce a new set of functions to read / write a list of 
	descriptors that have no containers (a bit like an OD command, but for descriptors)
		This is usefull for IPMPv2 DecoderSpecificInfo which contains a set of 
	IPMP_Declarators.
		As it could be used for other purposes we keep it generic
	You must create the list yourself, the functions just encode/decode from/to the list
*****************************************************************************************/

GF_EXPORT
GF_Err gf_odf_desc_list_read(char *raw_list, u32 raw_size, GF_List *descList)
{
	GF_BitStream *bs;
	u32 size, desc_size;
	GF_Descriptor *desc;
	GF_Err e = GF_OK;

	if (!descList || !raw_list || !raw_size) return GF_BAD_PARAM;

	bs = gf_bs_new(raw_list, raw_size, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;

	size = 0;
	while (size < raw_size) {
		e =	gf_odf_parse_descriptor(bs, &desc, &desc_size); 
		if (e) goto exit;
		gf_list_add(descList, desc);
		size += desc_size + gf_odf_size_field_size(desc_size);
	}

exit:
	//then delete our bitstream
	gf_bs_del(bs);
	if (size != raw_size) e = GF_ODF_INVALID_DESCRIPTOR;
	return e;
}


GF_EXPORT
GF_Err gf_odf_desc_list_write(GF_List *descList, char **outEncList, u32 *outSize)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!descList || !outEncList || *outEncList || !outSize) return GF_BAD_PARAM;

	*outSize = 0;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_OUT_OF_MEM;

	e = gf_odf_write_descriptor_list(bs, descList);
	if (e) {
		gf_bs_del(bs);
		return e;
	}

	gf_bs_get_content(bs, outEncList, outSize);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_desc_list_size(GF_List *descList, u32 *outSize)
{
	return gf_odf_size_descriptor_list(descList, outSize);
}



GF_Err gf_odf_codec_apply_com(GF_ODCodec *codec, GF_ODCom *command)
{
	GF_ODCom *com;
	GF_ODUpdate *odU, *odU_o;
	u32 i, count;
	count = gf_list_count(codec->CommandList);

	switch (command->tag) {
	case GF_ODF_OD_REMOVE_TAG:
		for (i=0; i<count; i++) {
			com = (GF_ODCom *)gf_list_get(codec->CommandList, i);
			/*process OD updates*/
			if (com->tag==GF_ODF_OD_UPDATE_TAG) {
				u32 count, j, k;
				GF_ODRemove *odR = (GF_ODRemove *) command;
				odU = (GF_ODUpdate *)com;
				count = gf_list_count(odU->objectDescriptors);
				/*remove all descs*/
				for (k=0; k<count; k++) {
					GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, k);
					for (j=0; j<odR->NbODs; j++) {
						if (od->objectDescriptorID==odR->OD_ID[j]) {
							gf_list_rem(odU->objectDescriptors, k);
							k--; count--;
							gf_odf_desc_del((GF_Descriptor *)od);
							break;
						}
					}
				}
				if (!gf_list_count(odU->objectDescriptors)) {
					gf_list_rem(codec->CommandList, i);
					i--;
					count--;
				}
			}
			/*process ESD updates*/
			else if (com->tag==GF_ODF_ESD_UPDATE_TAG) {
				u32 j;
				GF_ODRemove *odR = (GF_ODRemove *) command;
				GF_ESDUpdate *esdU = (GF_ESDUpdate*)com;
				for (j=0; j<odR->NbODs; j++) {
					if (esdU->ODID==odR->OD_ID[j]) {
						gf_list_rem(codec->CommandList, i);
						i--;
						count--;
						gf_odf_com_del((GF_ODCom**)&esdU);
						break;
					}
				}
			}
		}
		return GF_OK;
	case GF_ODF_OD_UPDATE_TAG:
		odU_o = NULL;
		for (i=0; i<count; i++) {
			odU_o = (GF_ODUpdate*)gf_list_get(codec->CommandList, i);
			/*process OD updates*/
			if (odU_o->tag==GF_ODF_OD_UPDATE_TAG) break;
			odU_o = NULL;
		}
		if (!odU_o) {
			odU_o = (GF_ODUpdate *)gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			gf_list_add(codec->CommandList, odU_o);
		}
		odU = (GF_ODUpdate*)command;
		count = gf_list_count(odU->objectDescriptors);
		for (i=0; i<count; i++) {
			Bool found = 0;
			GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, i);
			u32 j, count2 = gf_list_count(odU_o->objectDescriptors);
			for (j=0; j<count2; j++) {
				GF_ObjectDescriptor *od2 = (GF_ObjectDescriptor *)gf_list_get(odU_o->objectDescriptors, j);
				if (od2->objectDescriptorID==od->objectDescriptorID) {
					found = 1; 
					break;
				}
			}
			if (!found) {
				GF_ObjectDescriptor *od_new;
				gf_odf_desc_copy((GF_Descriptor*)od, (GF_Descriptor**)&od_new);
				gf_list_add(odU_o->objectDescriptors, od_new);
			}

		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}
