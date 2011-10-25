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
#include <gpac/utf.h>

#define DATE_CODING_BIT_LEN	40


static GFINLINE GF_Err OD_ReadUTF8String(GF_BitStream *bs, char **string, Bool isUTF8, u32 *read)
{
	u32 len;
	*read = 1;
	len = gf_bs_read_int(bs, 8) + 1;
	if (!isUTF8) len *= 2;
	(*string) = (char *) gf_malloc(sizeof(char)*len);
	if (! (*string) ) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, (*string), len);
	*read += len;
	return GF_OK;
}

static GFINLINE u32 OD_SizeUTF8String(char *string, Bool isUTF8)
{
	if (isUTF8) return 1 + strlen(string);
	return 1 + 2*gf_utf8_wcslen((const unsigned short *)string);
}

static GFINLINE void OD_WriteUTF8String(GF_BitStream *bs, char *string, Bool isUTF8)
{
	u32 len;
	if (isUTF8) {
		len = strlen(string);
		gf_bs_write_int(bs, len, 8);
		gf_bs_write_data(bs, string, len);
	} else {
		len = gf_utf8_wcslen((const unsigned short *)string);
		gf_bs_write_int(bs, len, 8);
		gf_bs_write_data(bs, string, len*2);
	}
}

/*use to parse strings read the length as well - Warning : the alloc is done here !!*/
GF_Err gf_odf_read_url_string(GF_BitStream *bs, char **string, u32 *readBytes)
{
	u32 length;
	*readBytes = 0;

	/*if the string is not NULL, return an error...*/
	if (*string != NULL) return GF_BAD_PARAM;

	/*the len is always on 8 bits*/
	length = gf_bs_read_int(bs, 8);
	*readBytes = 1;
	/*JLF AMD to MPEG-4 systems :) - This is not conformant at all, just hoping MPEG will accept it soon
	since 255bytes URL is a real pain in the neck*/
	if (!length) {
		length = gf_bs_read_int(bs, 32);
		*readBytes += 4;
	}

	/*we want to use strlen to get rid of "stringLength" => we need an extra 0*/
	(*string) = (char *) gf_malloc(length + 1);
	if (! string) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, (*string), length);
	*readBytes += length;
	(*string)[length] = 0;
	return GF_OK;
}

/*writes string*/
GF_Err gf_odf_write_url_string(GF_BitStream *bs, char *string)
{
	u32 len;
	/*we accept NULL strings now*/
	if (!string) {
		gf_bs_write_int(bs, 0, 8);
		return GF_OK;
	}		
	len = strlen(string);
	if (len > 255) {
		gf_bs_write_int(bs, 0, 8);
		gf_bs_write_int(bs, len, 32);
	} else {
		gf_bs_write_int(bs, len, 8);
	}
	gf_bs_write_data(bs, string, len);
	return GF_OK;
}

u32 gf_odf_size_url_string(char *string)
{
	u32 len = strlen(string);
	if (len>255) return len+5;
	return len+1;
}

GF_Descriptor *gf_odf_new_esd()
{
	GF_ESD *newDesc = (GF_ESD *) gf_malloc(sizeof(GF_ESD));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_ESD));
	newDesc->IPIDataSet = gf_list_new();
	newDesc->IPMPDescriptorPointers = gf_list_new();
	newDesc->extensionDescriptors = gf_list_new();
	newDesc->tag = GF_ODF_ESD_TAG;
	return (GF_Descriptor *) newDesc;
}


GF_Err gf_odf_del_esd(GF_ESD *esd)
{
	GF_Err e;
	if (!esd) return GF_BAD_PARAM;
	if (esd->URLString)	gf_free(esd->URLString);

	if (esd->decoderConfig)	{
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->decoderConfig);
		if (e) return e;
	}
	if (esd->slConfig) {
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->slConfig);
		if (e) return e;
	}
	if (esd->ipiPtr) {
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->ipiPtr);
		if (e) return e;
	}
	if (esd->qos) {
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->qos);
		if (e) return e;
	}
	if (esd->RegDescriptor)	{
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->RegDescriptor);
		if (e) return e;
	}
	if (esd->langDesc)	{
		e = gf_odf_delete_descriptor((GF_Descriptor *) esd->langDesc);
		if (e) return e;
	}

	e = gf_odf_delete_descriptor_list(esd->IPIDataSet);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(esd->IPMPDescriptorPointers);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(esd->extensionDescriptors);
	if (e) return e;
	gf_free(esd);
	return GF_OK;
}


GF_Err AddDescriptorToESD(GF_ESD *esd, GF_Descriptor *desc)
{
	if (!esd || !desc) return GF_BAD_PARAM;

	switch (desc->tag) {
	case GF_ODF_DCD_TAG:
		if (esd->decoderConfig) return GF_ODF_INVALID_DESCRIPTOR;
		esd->decoderConfig = (GF_DecoderConfig *) desc;
		break;

	case GF_ODF_SLC_TAG:
		if (esd->slConfig) return GF_ODF_INVALID_DESCRIPTOR;
		esd->slConfig = (GF_SLConfig *) desc;
		break;

	case GF_ODF_MUXINFO_TAG:
		gf_list_add(esd->extensionDescriptors, desc);
		break;

	case GF_ODF_LANG_TAG:
		if (esd->langDesc) return GF_ODF_INVALID_DESCRIPTOR;
		esd->langDesc = (GF_Language *) desc;
		break;

#ifndef GPAC_MINIMAL_ODF
	//the GF_ODF_ISOM_IPI_PTR_TAG is only used in the file format and replaces GF_ODF_IPI_PTR_TAG...
	case GF_ODF_ISOM_IPI_PTR_TAG:
	case GF_ODF_IPI_PTR_TAG:
		if (esd->ipiPtr) return GF_ODF_INVALID_DESCRIPTOR;
		esd->ipiPtr = (GF_IPIPtr *) desc;
		break;

	case GF_ODF_QOS_TAG:
		if (esd->qos) return GF_ODF_INVALID_DESCRIPTOR;
		esd->qos  =(GF_QoS_Descriptor *) desc;
		break;

	case GF_ODF_CI_TAG:
	case GF_ODF_SCI_TAG:
		return gf_list_add(esd->IPIDataSet, desc);

	//we use the same struct for v1 and v2 IPMP DPs
	case GF_ODF_IPMP_PTR_TAG:
		return gf_list_add(esd->IPMPDescriptorPointers, desc);

	case GF_ODF_REG_TAG:
		if (esd->RegDescriptor) return GF_ODF_INVALID_DESCRIPTOR;
		esd->RegDescriptor =(GF_Registration *) desc;
		break;
#endif

	default:
		if ( (desc->tag >= GF_ODF_EXT_BEGIN_TAG) &&
			(desc->tag <= GF_ODF_EXT_END_TAG) ) {
			return gf_list_add(esd->extensionDescriptors, desc);
		}
		gf_odf_delete_descriptor(desc);
		return GF_OK;
	}

	return GF_OK;
}

GF_Err gf_odf_read_esd(GF_BitStream *bs, GF_ESD *esd, u32 DescSize)
{
	GF_Err e = GF_OK;
	u32 ocrflag, urlflag, streamdependflag, tmp_size, nbBytes, read;

	if (! esd) return GF_BAD_PARAM;

	nbBytes = 0;

	esd->ESID = gf_bs_read_int(bs, 16);
	streamdependflag = gf_bs_read_int(bs, 1);
	urlflag = gf_bs_read_int(bs, 1);
	ocrflag = gf_bs_read_int(bs, 1);
	esd->streamPriority = gf_bs_read_int(bs, 5);
	nbBytes += 3;
	
	if (streamdependflag) {
		esd->dependsOnESID = gf_bs_read_int(bs, 16);
		nbBytes += 2;
	}

	if (urlflag) {
		e = gf_odf_read_url_string(bs, & esd->URLString, &read);
		if (e) return e;
		nbBytes += read;
	}
	if (ocrflag) {
		esd->OCRESID = gf_bs_read_int(bs, 16);
		nbBytes += 2;
	}
	/*fix broken sync*/
//	if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmp_size);
		/*fix for iPod files*/
		if (e==GF_ODF_INVALID_DESCRIPTOR) {
			nbBytes += tmp_size;
			if (nbBytes>DescSize) return e;
			gf_bs_read_int(bs, DescSize-nbBytes);
			return GF_OK;
		}
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = AddDescriptorToESD(esd, tmp);
		if (e) return e;
		nbBytes += tmp_size + gf_odf_size_field_size(tmp_size);
		
		//apple fix
		if (!tmp_size) nbBytes = DescSize;

	}
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return e;

}

GF_Err gf_odf_size_esd(GF_ESD *esd, u32 *outSize)
{
	GF_Err e;
	u32 tmpSize;
	if (! esd) return GF_BAD_PARAM;

	*outSize = 0;
	*outSize += 3;

	/*this helps keeping proper sync: some people argue that OCR_ES_ID == ES_ID is a circular reference
	of streams. Since this is equivalent to no OCR_ES_ID, keep it that way*/
//	if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;

	if (esd->dependsOnESID) *outSize += 2;
	if (esd->URLString) *outSize += gf_odf_size_url_string(esd->URLString);
	if (esd->OCRESID) *outSize += 2;

	if (esd->decoderConfig) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->decoderConfig, &tmpSize);
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (esd->slConfig) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->slConfig, &tmpSize);
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (esd->ipiPtr) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->ipiPtr, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (esd->langDesc) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->langDesc, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}

	e = gf_odf_size_descriptor_list(esd->IPIDataSet, outSize);
	if (e) return e;
	e = gf_odf_size_descriptor_list(esd->IPMPDescriptorPointers, outSize);
	if (e) return e;
	if (esd->qos) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->qos, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (esd->RegDescriptor) {
		e = gf_odf_size_descriptor((GF_Descriptor *) esd->RegDescriptor, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	return gf_odf_size_descriptor_list(esd->extensionDescriptors, outSize);
}

GF_Err gf_odf_write_esd(GF_BitStream *bs, GF_ESD *esd)
{
	GF_Err e;
	u32 size;
	if (! esd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)esd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, esd->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, esd->ESID, 16);
	gf_bs_write_int(bs, esd->dependsOnESID ? 1 : 0, 1);
	gf_bs_write_int(bs, esd->URLString != NULL ? 1 : 0, 1);
	gf_bs_write_int(bs, esd->OCRESID ? 1 : 0, 1);
	gf_bs_write_int(bs, esd->streamPriority, 5);

	if (esd->dependsOnESID) {
		gf_bs_write_int(bs, esd->dependsOnESID, 16);
	}
	if (esd->URLString) {
		e = gf_odf_write_url_string(bs, esd->URLString);
		if (e) return e;
	}


	if (esd->OCRESID) {
		gf_bs_write_int(bs, esd->OCRESID, 16);
	}
	if (esd->decoderConfig) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->decoderConfig);
		if (e) return e;
	}
	if (esd->slConfig) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->slConfig);
		if (e) return e;
	}
	if (esd->ipiPtr) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->ipiPtr);
		if (e) return e;
	}
	if (esd->langDesc) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->langDesc);
		if (e) return e;
	}

	e = gf_odf_write_descriptor_list(bs, esd->IPIDataSet);
	if (e) return e;
	e = gf_odf_write_descriptor_list(bs, esd->IPMPDescriptorPointers);
	if (e) return e;
	if (esd->qos) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->qos);
		if (e) return e;
	}
	if (esd->RegDescriptor) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) esd->RegDescriptor);
		if (e) return e;
	}
	return gf_odf_write_descriptor_list(bs, esd->extensionDescriptors);
}

GF_Descriptor *gf_odf_new_iod()
{
	GF_InitialObjectDescriptor *newDesc = (GF_InitialObjectDescriptor *) gf_malloc(sizeof(GF_InitialObjectDescriptor));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_InitialObjectDescriptor));

	newDesc->ESDescriptors = gf_list_new();
	newDesc->OCIDescriptors = gf_list_new();
	newDesc->IPMP_Descriptors = gf_list_new();

	newDesc->extensionDescriptors = gf_list_new();
	newDesc->tag = GF_ODF_IOD_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_iod(GF_InitialObjectDescriptor *iod)
{
	GF_Err e;
	if (!iod) return GF_BAD_PARAM;
	if (iod->URLString)	gf_free(iod->URLString);
	e = gf_odf_delete_descriptor_list(iod->ESDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->OCIDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->IPMP_Descriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->extensionDescriptors);
	if (e) return e;
	if (iod->IPMPToolList) gf_odf_delete_descriptor((GF_Descriptor *) iod->IPMPToolList);
	gf_free(iod);
	return GF_OK;
}

GF_Err AddDescriptorToIOD(GF_InitialObjectDescriptor *iod, GF_Descriptor *desc)
{
	if (!iod || !desc) return GF_BAD_PARAM;

	switch (desc->tag) {
	case GF_ODF_ESD_TAG:
		return gf_list_add(iod->ESDescriptors, desc);

	//we use the same struct for v1 and v2 IPMP DPs
	case GF_ODF_IPMP_PTR_TAG:
	/*IPMPX*/
	case GF_ODF_IPMP_TAG:
		return gf_list_add(iod->IPMP_Descriptors, desc);
	
	/*IPMPX*/
	case GF_ODF_IPMP_TL_TAG:
		if (iod->IPMPToolList) gf_odf_desc_del((GF_Descriptor *)iod->IPMPToolList);
		iod->IPMPToolList = (GF_IPMP_ToolList *)desc;
		return GF_OK;

	default:
		break;
	}
	if ( (desc->tag >= GF_ODF_OCI_BEGIN_TAG) && (desc->tag <= GF_ODF_OCI_END_TAG) ) return gf_list_add(iod->OCIDescriptors, desc);
	if ( (desc->tag >= GF_ODF_EXT_BEGIN_TAG) && (desc->tag <= GF_ODF_EXT_END_TAG) ) return gf_list_add(iod->extensionDescriptors, desc);
	return GF_BAD_PARAM;
}

GF_Err gf_odf_read_iod(GF_BitStream *bs, GF_InitialObjectDescriptor *iod, u32 DescSize)
{
	GF_Err e;
	u32 urlflag, read;
	u32 tmp_size, nbBytes = 0;
	if (! iod) return GF_BAD_PARAM;

	iod->objectDescriptorID = gf_bs_read_int(bs, 10);
	urlflag = gf_bs_read_int(bs, 1);
	iod->inlineProfileFlag = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 4);
	nbBytes += 2;
	
	if (urlflag) {
		e = gf_odf_read_url_string(bs, & iod->URLString, &read);
		if (e) return e;
		nbBytes += read;
	} else {
		iod->OD_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->scene_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->audio_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->visual_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->graphics_profileAndLevel = gf_bs_read_int(bs, 8);
		nbBytes += 5;
	}

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmp_size);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = AddDescriptorToIOD(iod, tmp);
		if (e) return e;
		nbBytes += tmp_size + gf_odf_size_field_size(tmp_size);
	}
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_iod(GF_InitialObjectDescriptor *iod, u32 *outSize)
{
	GF_Err e;
	if (! iod) return GF_BAD_PARAM;

	*outSize = 0;
	*outSize += 2;
	if (iod->URLString) {
		*outSize += gf_odf_size_url_string(iod->URLString);
	} else {
		*outSize += 5;
		e = gf_odf_size_descriptor_list(iod->ESDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(iod->OCIDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(iod->IPMP_Descriptors, outSize);
		if (e) return e;

	}
	e = gf_odf_size_descriptor_list(iod->extensionDescriptors, outSize);
	if (e) return e;
	if (iod->IPMPToolList) {
		u32 tmpSize;
		e = gf_odf_size_descriptor((GF_Descriptor *) iod->IPMPToolList, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	return GF_OK;
}

GF_Err gf_odf_write_iod(GF_BitStream *bs, GF_InitialObjectDescriptor *iod)
{
	GF_Err e;
	u32 size;
	if (! iod) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)iod, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, iod->tag, size);
	if (e) return e;
	
	gf_bs_write_int(bs, iod->objectDescriptorID, 10);
	gf_bs_write_int(bs, iod->URLString != NULL ? 1 : 0, 1);
	gf_bs_write_int(bs, iod->inlineProfileFlag, 1);
	gf_bs_write_int(bs, 15, 4);		//reserved: 0b1111 == 15

	if (iod->URLString) {
		gf_odf_write_url_string(bs, iod->URLString);
	} else {
		gf_bs_write_int(bs, iod->OD_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->scene_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->audio_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->visual_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->graphics_profileAndLevel, 8);
		e = gf_odf_write_descriptor_list(bs, iod->ESDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, iod->OCIDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, iod->IPMP_Descriptors, GF_ODF_IPMP_PTR_TAG);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, iod->IPMP_Descriptors, GF_ODF_IPMP_TAG);
		if (e) return e;
		if (iod->IPMPToolList) {
			e = gf_odf_write_descriptor(bs, (GF_Descriptor *) iod->IPMPToolList);
			if (e) return e;
		}
	}
	e = gf_odf_write_descriptor_list(bs, iod->extensionDescriptors);
	return GF_OK;
}



GF_Descriptor *gf_odf_new_od()
{
	GF_ObjectDescriptor *newDesc;
	GF_SAFEALLOC(newDesc, GF_ObjectDescriptor);
	if (!newDesc) return NULL;

	newDesc->URLString = NULL;
	newDesc->ESDescriptors = gf_list_new();
	newDesc->OCIDescriptors = gf_list_new();
	newDesc->IPMP_Descriptors = gf_list_new();
	newDesc->extensionDescriptors = gf_list_new();
	newDesc->objectDescriptorID = 0;
	newDesc->tag = GF_ODF_OD_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_od(GF_ObjectDescriptor *od)
{
	GF_Err e;
	if (!od) return GF_BAD_PARAM;
	if (od->URLString)	gf_free(od->URLString);
	e = gf_odf_delete_descriptor_list(od->ESDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->OCIDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->IPMP_Descriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->extensionDescriptors);
	if (e) return e;
	gf_free(od);
	return GF_OK;
}

GF_Err AddDescriptorToOD(GF_ObjectDescriptor *od, GF_Descriptor *desc)
{
	if (!od || !desc) return GF_BAD_PARAM;

	//check if we can handle ContentClassif tags
	if ( (desc->tag >= GF_ODF_OCI_BEGIN_TAG) &&
		(desc->tag <= GF_ODF_OCI_END_TAG) ) {
		return gf_list_add(od->OCIDescriptors, desc);
	}

	//or extensions
	if ( (desc->tag >= GF_ODF_EXT_BEGIN_TAG) &&
		(desc->tag <= GF_ODF_EXT_END_TAG) ) {
		return gf_list_add(od->extensionDescriptors, desc);
	}

	//to cope with envivio
	switch (desc->tag) {
	case GF_ODF_ESD_TAG:
	case GF_ODF_ESD_REF_TAG:
		return gf_list_add(od->ESDescriptors, desc);

	//we use the same struct for v1 and v2 IPMP DPs
	case GF_ODF_IPMP_PTR_TAG:
	case GF_ODF_IPMP_TAG:
		return gf_list_add(od->IPMP_Descriptors, desc);

	default:
		return GF_BAD_PARAM;
	}
}

GF_Err gf_odf_read_od(GF_BitStream *bs, GF_ObjectDescriptor *od, u32 DescSize)
{
	GF_Err e;
	u32 urlflag;
	u32 tmpSize, nbBytes = 0;
	if (! od) return GF_BAD_PARAM;

	od->objectDescriptorID = gf_bs_read_int(bs, 10);
	urlflag = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 5);
	nbBytes += 2;
	
	if (urlflag) {
		u32 read;
		e = gf_odf_read_url_string(bs, & od->URLString, &read);
		if (e) return e;
		nbBytes += read;
	}

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = AddDescriptorToOD(od, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_od(GF_ObjectDescriptor *od, u32 *outSize)
{
	GF_Err e;
	if (! od) return GF_BAD_PARAM;

	*outSize = 2;
	if (od->URLString) {
		*outSize += gf_odf_size_url_string(od->URLString);
	} else {
		e = gf_odf_size_descriptor_list(od->ESDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(od->OCIDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(od->IPMP_Descriptors, outSize);
		if (e) return e;
	}
	return gf_odf_size_descriptor_list(od->extensionDescriptors, outSize);
}

GF_Err gf_odf_write_od(GF_BitStream *bs, GF_ObjectDescriptor *od)
{
	GF_Err e;
	u32 size;
	if (! od) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)od, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, od->tag, size);
	if (e) return e;
	
	gf_bs_write_int(bs, od->objectDescriptorID, 10);
	gf_bs_write_int(bs, od->URLString != NULL ? 1 : 0, 1);
	gf_bs_write_int(bs, 31, 5);		//reserved: 0b1111.1 == 31

	if (od->URLString) {
		gf_odf_write_url_string(bs, od->URLString);
	} else {
		e = gf_odf_write_descriptor_list(bs, od->ESDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, od->OCIDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, od->IPMP_Descriptors, GF_ODF_IPMP_PTR_TAG);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, od->IPMP_Descriptors, GF_ODF_IPMP_TAG);
		if (e) return e;
	}
	e = gf_odf_write_descriptor_list(bs, od->extensionDescriptors);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_isom_iod()
{
	GF_IsomInitialObjectDescriptor *newDesc = (GF_IsomInitialObjectDescriptor *) gf_malloc(sizeof(GF_IsomInitialObjectDescriptor));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_IsomInitialObjectDescriptor));

	newDesc->ES_ID_IncDescriptors = gf_list_new();
	newDesc->ES_ID_RefDescriptors = gf_list_new();
	newDesc->OCIDescriptors = gf_list_new();
	newDesc->IPMP_Descriptors = gf_list_new();
	newDesc->extensionDescriptors = gf_list_new();
	newDesc->tag = GF_ODF_ISOM_IOD_TAG;

	//by default create an IOD with no inline and no capabilities
	newDesc->audio_profileAndLevel = 0xFF;
	newDesc->graphics_profileAndLevel = 0xFF;
	newDesc->scene_profileAndLevel = 0xFF;
	newDesc->OD_profileAndLevel = 0xFF;
	newDesc->visual_profileAndLevel = 0xFF;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_isom_iod(GF_IsomInitialObjectDescriptor *iod)
{
	GF_Err e;
	if (!iod) return GF_BAD_PARAM;
	if (iod->URLString)	gf_free(iod->URLString);
	e = gf_odf_delete_descriptor_list(iod->ES_ID_IncDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->ES_ID_RefDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->OCIDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->IPMP_Descriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(iod->extensionDescriptors);
	if (e) return e;
	if (iod->IPMPToolList) gf_odf_delete_descriptor((GF_Descriptor *) iod->IPMPToolList);
	gf_free(iod);
	return GF_OK;
}

GF_Err AddDescriptorToIsomIOD(GF_IsomInitialObjectDescriptor *iod, GF_Descriptor *desc)
{
	if (!iod || !desc) return GF_BAD_PARAM;

	switch (desc->tag) {
	case GF_ODF_ESD_TAG:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	case GF_ODF_ESD_INC_TAG:
		//there shouldn't be ref if inc
		if (gf_list_count(iod->ES_ID_RefDescriptors)) return GF_ODF_FORBIDDEN_DESCRIPTOR;
		return gf_list_add(iod->ES_ID_IncDescriptors, desc);

	case GF_ODF_ESD_REF_TAG:
		//there shouldn't be inc if ref
		if (gf_list_count(iod->ES_ID_IncDescriptors)) return GF_ODF_FORBIDDEN_DESCRIPTOR;
		return gf_list_add(iod->ES_ID_RefDescriptors, desc);

	//we use the same struct for v1 and v2 IPMP DPs
	case GF_ODF_IPMP_PTR_TAG:
	case GF_ODF_IPMP_TAG:
		return gf_list_add(iod->IPMP_Descriptors, desc);

	/*IPMPX*/
	case GF_ODF_IPMP_TL_TAG:
		if (iod->IPMPToolList) gf_odf_desc_del((GF_Descriptor *)iod->IPMPToolList);
		iod->IPMPToolList = (GF_IPMP_ToolList *)desc;
		return GF_OK;

	default:
		break;
	}
	//check if we can handle ContentClassif tags
	if ( (desc->tag >= GF_ODF_OCI_BEGIN_TAG) && (desc->tag <= GF_ODF_OCI_END_TAG) ) return gf_list_add(iod->OCIDescriptors, desc);
	//or extensions
	if ( (desc->tag >= GF_ODF_EXT_BEGIN_TAG) && (desc->tag <= GF_ODF_EXT_END_TAG) ) return gf_list_add(iod->extensionDescriptors, desc);
	return GF_BAD_PARAM;
}

GF_Err gf_odf_read_isom_iod(GF_BitStream *bs, GF_IsomInitialObjectDescriptor *iod, u32 DescSize)
{
	u32 nbBytes = 0, tmpSize;
	u32 urlflag;
	GF_Err e;
	if (! iod) return GF_BAD_PARAM;

	iod->objectDescriptorID = gf_bs_read_int(bs, 10);
	urlflag = gf_bs_read_int(bs, 1);
	iod->inlineProfileFlag = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 4);
	nbBytes += 2;
	
	if (urlflag) {
		u32 read;
		e = gf_odf_read_url_string(bs, & iod->URLString, &read);
		if (e) return e;
		nbBytes += read;
	} else {
		iod->OD_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->scene_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->audio_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->visual_profileAndLevel = gf_bs_read_int(bs, 8);
		iod->graphics_profileAndLevel = gf_bs_read_int(bs, 8);
		nbBytes += 5;
	}

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = AddDescriptorToIsomIOD(iod, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_isom_iod(GF_IsomInitialObjectDescriptor *iod, u32 *outSize)
{
	GF_Err e;
	if (! iod) return GF_BAD_PARAM;

	*outSize = 2;
	if (iod->URLString) {
		*outSize += gf_odf_size_url_string(iod->URLString);
	} else {
		*outSize += 5;
		e = gf_odf_size_descriptor_list(iod->ES_ID_IncDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(iod->ES_ID_RefDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(iod->OCIDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(iod->IPMP_Descriptors, outSize);
		if (e) return e;
	}
	if (iod->IPMPToolList) {
		u32 tmpSize;
		e = gf_odf_size_descriptor((GF_Descriptor *) iod->IPMPToolList, &tmpSize);	
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	return gf_odf_size_descriptor_list(iod->extensionDescriptors, outSize);
}

GF_Err gf_odf_write_isom_iod(GF_BitStream *bs, GF_IsomInitialObjectDescriptor *iod)
{
	GF_Err e;
	u32 size;
	if (! iod) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)iod, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, iod->tag, size);
	if (e) return e;
	
	gf_bs_write_int(bs, iod->objectDescriptorID, 10);
	gf_bs_write_int(bs, iod->URLString != NULL ? 1 : 0, 1);
	gf_bs_write_int(bs, iod->inlineProfileFlag, 1);
	gf_bs_write_int(bs, 15, 4);		//reserved: 0b1111 == 15

	if (iod->URLString) {
		gf_odf_write_url_string(bs, iod->URLString);
	} else {
		gf_bs_write_int(bs, iod->OD_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->scene_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->audio_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->visual_profileAndLevel, 8);
		gf_bs_write_int(bs, iod->graphics_profileAndLevel, 8);
		e = gf_odf_write_descriptor_list(bs, iod->ES_ID_IncDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, iod->ES_ID_RefDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, iod->OCIDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, iod->IPMP_Descriptors, GF_ODF_IPMP_PTR_TAG);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, iod->IPMP_Descriptors, GF_ODF_IPMP_TAG);
		if (e) return e;
		if (iod->IPMPToolList) {
			e = gf_odf_write_descriptor(bs, (GF_Descriptor *) iod->IPMPToolList);
			if (e) return e;
		}
	}
	e = gf_odf_write_descriptor_list(bs, iod->extensionDescriptors);
	if (e) return e;
	return GF_OK;
}


GF_Descriptor *gf_odf_new_isom_od()
{
	GF_IsomObjectDescriptor *newDesc = (GF_IsomObjectDescriptor *) gf_malloc(sizeof(GF_IsomObjectDescriptor));
	if (!newDesc) return NULL;

	newDesc->URLString = NULL;
	newDesc->ES_ID_IncDescriptors = gf_list_new();
	newDesc->ES_ID_RefDescriptors = gf_list_new();
	newDesc->OCIDescriptors = gf_list_new();
	newDesc->IPMP_Descriptors = gf_list_new();
	newDesc->extensionDescriptors = gf_list_new();
	newDesc->objectDescriptorID = 0;
	newDesc->tag = GF_ODF_ISOM_OD_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_isom_od(GF_IsomObjectDescriptor *od)
{
	GF_Err e;
	if (!od) return GF_BAD_PARAM;
	if (od->URLString)	gf_free(od->URLString);
	e = gf_odf_delete_descriptor_list(od->ES_ID_IncDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->ES_ID_RefDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->OCIDescriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->IPMP_Descriptors);
	if (e) return e;
	e = gf_odf_delete_descriptor_list(od->extensionDescriptors);
	if (e) return e;
	gf_free(od);
	return GF_OK;
}

GF_Err AddDescriptorToIsomOD(GF_IsomObjectDescriptor *od, GF_Descriptor *desc)
{
	if (!od || !desc) return GF_BAD_PARAM;

	//check if we can handle ContentClassif tags
	if ( (desc->tag >= GF_ODF_OCI_BEGIN_TAG) &&
		(desc->tag <= GF_ODF_OCI_END_TAG) ) {
		return gf_list_add(od->OCIDescriptors, desc);
	}

	//or extension ...
	if ( (desc->tag >= GF_ODF_EXT_BEGIN_TAG) &&
		(desc->tag <= GF_ODF_EXT_END_TAG) ) {
		return gf_list_add(od->extensionDescriptors, desc);
	}

	switch (desc->tag) {
	case GF_ODF_ESD_TAG:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	case GF_ODF_ESD_INC_TAG:
		//there shouldn't be ref if inc
		if (gf_list_count(od->ES_ID_RefDescriptors)) return GF_ODF_FORBIDDEN_DESCRIPTOR;
		return gf_list_add(od->ES_ID_IncDescriptors, desc);

	case GF_ODF_ESD_REF_TAG:
		//there shouldn't be inc if ref
		if (gf_list_count(od->ES_ID_IncDescriptors)) return GF_ODF_FORBIDDEN_DESCRIPTOR;
		return gf_list_add(od->ES_ID_RefDescriptors, desc);

	//we use the same struct for v1 and v2 IPMP DPs
	case GF_ODF_IPMP_PTR_TAG:
	case GF_ODF_IPMP_TAG:
		return gf_list_add(od->IPMP_Descriptors, desc);

	default:
		return GF_BAD_PARAM;
	}
}

GF_Err gf_odf_read_isom_od(GF_BitStream *bs, GF_IsomObjectDescriptor *od, u32 DescSize)
{
	GF_Err e;
	u32 urlflag;
	u32 tmpSize, nbBytes = 0;
	if (! od) return GF_BAD_PARAM;

	od->objectDescriptorID = gf_bs_read_int(bs, 10);
	urlflag = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 5);
	nbBytes += 2;
	
	if (urlflag) {
		u32 read;
		e = gf_odf_read_url_string(bs, & od->URLString, &read);
		if (e) return e;
		nbBytes += read;
	}

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = AddDescriptorToIsomOD(od, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_isom_od(GF_IsomObjectDescriptor *od, u32 *outSize)
{
	GF_Err e;
	if (! od) return GF_BAD_PARAM;

	*outSize = 2;
	if (od->URLString) {
		*outSize += gf_odf_size_url_string(od->URLString);
	} else {
		e = gf_odf_size_descriptor_list(od->ES_ID_IncDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(od->ES_ID_RefDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(od->OCIDescriptors, outSize);
		if (e) return e;
		e = gf_odf_size_descriptor_list(od->IPMP_Descriptors, outSize);
		if (e) return e;
	}
	return gf_odf_size_descriptor_list(od->extensionDescriptors, outSize);
}

GF_Err gf_odf_write_isom_od(GF_BitStream *bs, GF_IsomObjectDescriptor *od)
{
	GF_Err e;
	u32 size;
	if (! od) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)od, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, od->tag, size);
	if (e) return e;
	
	gf_bs_write_int(bs, od->objectDescriptorID, 10);
	gf_bs_write_int(bs, od->URLString != NULL ? 1 : 0, 1);
	gf_bs_write_int(bs, 31, 5);		//reserved: 0b1111.1 == 31

	if (od->URLString) {
		gf_odf_write_url_string(bs, od->URLString);
	} else {
		e = gf_odf_write_descriptor_list(bs, od->ES_ID_IncDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, od->ES_ID_RefDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list(bs, od->OCIDescriptors);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, od->IPMP_Descriptors, GF_ODF_IPMP_PTR_TAG);
		if (e) return e;
		e = gf_odf_write_descriptor_list_filter(bs, od->IPMP_Descriptors, GF_ODF_IPMP_TAG);
		if (e) return e;
	}
	e = gf_odf_write_descriptor_list(bs, od->extensionDescriptors);
	if (e) return e;
	return GF_OK;
}



GF_Descriptor *gf_odf_new_dcd()
{
	GF_DecoderConfig *newDesc;
	GF_SAFEALLOC(newDesc, GF_DecoderConfig);
	if (!newDesc) return NULL;

	newDesc->profileLevelIndicationIndexDescriptor = gf_list_new();
	newDesc->tag = GF_ODF_DCD_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_dcd(GF_DecoderConfig *dcd)
{
	GF_Err e;
	if (!dcd) return GF_BAD_PARAM;

	if (dcd->decoderSpecificInfo) {
		e = gf_odf_delete_descriptor((GF_Descriptor *) dcd->decoderSpecificInfo);
		if (e) return e;
	}
	if (dcd->rvc_config) {
		e = gf_odf_delete_descriptor((GF_Descriptor *) dcd->rvc_config);
		if (e) return e;
	}
	e = gf_odf_delete_descriptor_list(dcd->profileLevelIndicationIndexDescriptor);
	if (e) return e;
	gf_free(dcd);
	return GF_OK;
}

GF_Err gf_odf_read_dcd(GF_BitStream *bs, GF_DecoderConfig *dcd, u32 DescSize)
{
	GF_Err e;
	u32 /*reserved, */tmp_size, nbBytes = 0;
	if (! dcd) return GF_BAD_PARAM;

	dcd->objectTypeIndication = gf_bs_read_int(bs, 8);
	dcd->streamType = gf_bs_read_int(bs, 6);
	dcd->upstream = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 1);
	dcd->bufferSizeDB = gf_bs_read_int(bs, 24);
	dcd->maxBitrate = gf_bs_read_int(bs, 32);
	dcd->avgBitrate = gf_bs_read_int(bs, 32);
	nbBytes += 13;

	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmp_size);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		switch (tmp->tag) {
		case GF_ODF_DSI_TAG:
			if (dcd->decoderSpecificInfo) {
				gf_odf_delete_descriptor(tmp);
				return GF_ODF_INVALID_DESCRIPTOR;
			}
			dcd->decoderSpecificInfo = (GF_DefaultDescriptor *) tmp;
			break;

		case GF_ODF_EXT_PL_TAG:
			e = gf_list_add(dcd->profileLevelIndicationIndexDescriptor, tmp);
			if (e < GF_OK) {
				gf_odf_delete_descriptor(tmp);
				return e;
			}
			break;

		/*iPod fix: delete and aborts, this will create an InvalidDescriptor at the ESD level with a loaded DSI,
		laoding will abort with a partially valid ESD which is all the matters*/
		case GF_ODF_SLC_TAG:
			gf_odf_delete_descriptor(tmp);
			return GF_OK;

		//what the hell is this descriptor ?? Don't know, so delete it !
		default:
			gf_odf_delete_descriptor(tmp);
			break;
		}
		nbBytes += tmp_size + gf_odf_size_field_size(tmp_size);
	}
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_dcd(GF_DecoderConfig *dcd, u32 *outSize)
{
	GF_Err e;
	u32 tmpSize;
	if (! dcd) return GF_BAD_PARAM;

	*outSize = 0;
	*outSize += 13;
	if (dcd->decoderSpecificInfo) {
		//warning: we don't know anything about the structure of a generic DecSpecInfo
		//we check the tag and size of the descriptor, but we most ofthe time can't parse it
		//the decSpecInfo is handle as a defaultDescriptor (opaque data, but same structure....)
		e = gf_odf_size_descriptor((GF_Descriptor *) dcd->decoderSpecificInfo , &tmpSize);
		if (e) return e;
		*outSize += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	e = gf_odf_size_descriptor_list(dcd->profileLevelIndicationIndexDescriptor, outSize);
	if (e) return e;
	return GF_OK;
}

GF_Err gf_odf_write_dcd(GF_BitStream *bs, GF_DecoderConfig *dcd)
{
	GF_Err e;
	u32 size;
	if (! dcd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)dcd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, dcd->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, dcd->objectTypeIndication, 8);
	gf_bs_write_int(bs, dcd->streamType, 6);
	gf_bs_write_int(bs, dcd->upstream, 1);
	gf_bs_write_int(bs, 1, 1);	//reserved field...
	gf_bs_write_int(bs, dcd->bufferSizeDB, 24);
	gf_bs_write_int(bs, dcd->maxBitrate, 32);
	gf_bs_write_int(bs, dcd->avgBitrate, 32);

	if (dcd->decoderSpecificInfo) {
		e = gf_odf_write_descriptor(bs, (GF_Descriptor *) dcd->decoderSpecificInfo);
		if (e) return e;
	}
	e = gf_odf_write_descriptor_list(bs, dcd->profileLevelIndicationIndexDescriptor);
	return e;
}


GF_Descriptor *gf_odf_new_default()
{
	GF_DefaultDescriptor *newDesc = (GF_DefaultDescriptor *) gf_malloc(sizeof(GF_DefaultDescriptor));
	if (!newDesc) return NULL;

	newDesc->dataLength = 0;
	newDesc->data = NULL;
	//set it to the Max allowed
	newDesc->tag = GF_ODF_USER_END_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_default(GF_DefaultDescriptor *dd)
{
	if (!dd) return GF_BAD_PARAM;

	if (dd->data) gf_free(dd->data);
	gf_free(dd);
	return GF_OK;
}

GF_Err gf_odf_read_default(GF_BitStream *bs, GF_DefaultDescriptor *dd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! dd) return GF_BAD_PARAM;

	dd->dataLength = DescSize;
	dd->data = NULL;
	if (DescSize) {
		dd->data = (char*)gf_malloc(dd->dataLength);
		if (! dd->data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, dd->data, dd->dataLength);
		nbBytes += dd->dataLength;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_default(GF_DefaultDescriptor *dd, u32 *outSize)
{
	if (! dd) return GF_BAD_PARAM;
	*outSize  = dd->dataLength;
	return GF_OK;
}

GF_Err gf_odf_write_default(GF_BitStream *bs, GF_DefaultDescriptor *dd)
{
	GF_Err e;
	u32 size;
	if (! dd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)dd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, dd->tag, size);
	if (e) return e;

	if (dd->data) {
		gf_bs_write_data(bs, dd->data, dd->dataLength);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_esd_inc()
{
	GF_ES_ID_Inc *newDesc = (GF_ES_ID_Inc *) gf_malloc(sizeof(GF_ES_ID_Inc));
	if (!newDesc) return NULL;
	newDesc->tag = GF_ODF_ESD_INC_TAG;
	newDesc->trackID = 0;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_esd_inc(GF_ES_ID_Inc *esd_inc)
{
	if (!esd_inc) return GF_BAD_PARAM;
	gf_free(esd_inc);
	return GF_OK;
}
GF_Err gf_odf_read_esd_inc(GF_BitStream *bs, GF_ES_ID_Inc *esd_inc, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! esd_inc) return GF_BAD_PARAM;

	esd_inc->trackID = gf_bs_read_int(bs, 32);
	nbBytes += 4;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}
GF_Err gf_odf_size_esd_inc(GF_ES_ID_Inc *esd_inc, u32 *outSize)
{
	if (! esd_inc) return GF_BAD_PARAM;
	*outSize = 4;
	return GF_OK;
}
GF_Err gf_odf_write_esd_inc(GF_BitStream *bs, GF_ES_ID_Inc *esd_inc)
{
	GF_Err e;
	u32 size;
	if (! esd_inc) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)esd_inc, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, esd_inc->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, esd_inc->trackID, 32);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_esd_ref()
{
	GF_ES_ID_Ref *newDesc = (GF_ES_ID_Ref *) gf_malloc(sizeof(GF_ES_ID_Ref));
	if (!newDesc) return NULL;
	newDesc->tag = GF_ODF_ESD_REF_TAG;
	newDesc->trackRef = 0;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_esd_ref(GF_ES_ID_Ref *esd_ref)
{
	if (!esd_ref) return GF_BAD_PARAM;
	gf_free(esd_ref);
	return GF_OK;
}
GF_Err gf_odf_read_esd_ref(GF_BitStream *bs, GF_ES_ID_Ref *esd_ref, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! esd_ref) return GF_BAD_PARAM;

	esd_ref->trackRef = gf_bs_read_int(bs, 16);
	nbBytes += 2;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_esd_ref(GF_ES_ID_Ref *esd_ref, u32 *outSize)
{
	if (! esd_ref) return GF_BAD_PARAM;
	*outSize = 2;
	return GF_OK;
}
GF_Err gf_odf_write_esd_ref(GF_BitStream *bs, GF_ES_ID_Ref *esd_ref)
{
	GF_Err e;
	u32 size;
	if (! esd_ref) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)esd_ref, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, esd_ref->tag, size);
	if (e) return e;
	
	gf_bs_write_int(bs, esd_ref->trackRef, 16);
	return GF_OK;
}



GF_Descriptor *gf_odf_new_segment()
{
	GF_Segment *newDesc = (GF_Segment *) gf_malloc(sizeof(GF_Segment));
	if (!newDesc) return NULL;

	memset(newDesc, 0, sizeof(GF_Segment));
	newDesc->tag = GF_ODF_SEGMENT_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_segment(GF_Segment *sd)
{
	if (!sd) return GF_BAD_PARAM;

	if (sd->SegmentName) gf_free(sd->SegmentName);
	gf_free(sd);
	return GF_OK;
}

GF_Err gf_odf_read_segment(GF_BitStream *bs, GF_Segment *sd, u32 DescSize)
{
	u32 size, nbBytes = 0;
	if (!sd) return GF_BAD_PARAM;

	sd->startTime = gf_bs_read_double(bs);
	sd->Duration = gf_bs_read_double(bs);
	nbBytes += 16;
	size = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	if (size) {
		sd->SegmentName = (char*) gf_malloc(sizeof(char)*(size+1));
		if (!sd->SegmentName) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, sd->SegmentName, size);
		sd->SegmentName[size] = 0;
		nbBytes += size;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_segment(GF_Segment *sd, u32 *outSize)
{
	if (!sd) return GF_BAD_PARAM;
	*outSize = 17;
	if (sd->SegmentName) *outSize += strlen(sd->SegmentName);
	return GF_OK;
}

GF_Err gf_odf_write_segment(GF_BitStream *bs, GF_Segment *sd)
{
	GF_Err e;
	u32 size;
	if (!sd) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)sd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, sd->tag, size);
	if (e) return e;
	gf_bs_write_double(bs, sd->startTime);
	gf_bs_write_double(bs, sd->Duration);
	if (sd->SegmentName) {
		gf_bs_write_int(bs, strlen(sd->SegmentName), 8);
		gf_bs_write_data(bs, sd->SegmentName, strlen(sd->SegmentName));
	} else {
		gf_bs_write_int(bs, 0, 8);
	}
	return GF_OK;
}
GF_Descriptor *gf_odf_new_mediatime()
{
	GF_MediaTime *newDesc = (GF_MediaTime *) gf_malloc(sizeof(GF_MediaTime));
	if (!newDesc) return NULL;

	memset(newDesc, 0, sizeof(GF_MediaTime));
	newDesc->tag = GF_ODF_MEDIATIME_TAG;
	return (GF_Descriptor *) newDesc;
}
GF_Err gf_odf_del_mediatime(GF_MediaTime *mt)
{
	if (!mt) return GF_BAD_PARAM;
	gf_free(mt);
	return GF_OK;
}
GF_Err gf_odf_read_mediatime(GF_BitStream *bs, GF_MediaTime *mt, u32 DescSize)
{
	if (!mt) return GF_BAD_PARAM;
	mt->mediaTimeStamp = gf_bs_read_double(bs);
	return GF_OK;
}
GF_Err gf_odf_size_mediatime(GF_MediaTime *mt, u32 *outSize)
{
	if (!mt) return GF_BAD_PARAM;
	*outSize = 8;
	return GF_OK;
}
GF_Err gf_odf_write_mediatime(GF_BitStream *bs, GF_MediaTime *mt)
{
	GF_Err e;
	u32 size;
	if (!mt) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)mt, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, mt->tag, size);
	if (e) return e;
	gf_bs_write_double(bs, mt->mediaTimeStamp);
	return GF_OK;
}


GF_Descriptor *gf_odf_new_lang()
{
	GF_Language *newDesc = (GF_Language *) gf_malloc(sizeof(GF_Language));
	if (!newDesc) return NULL;
	newDesc->langCode = 0;
	newDesc->tag = GF_ODF_LANG_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_lang(GF_Language *ld)
{
	if (!ld) return GF_BAD_PARAM;
	gf_free(ld);
	return GF_OK;
}

GF_Err gf_odf_read_lang(GF_BitStream *bs, GF_Language *ld, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!ld) return GF_BAD_PARAM;

	ld->langCode = gf_bs_read_int(bs, 24);
	nbBytes += 3;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_lang(GF_Language *ld, u32 *outSize)
{
	if (!ld) return GF_BAD_PARAM;
	*outSize = 3;
	return GF_OK;
}

GF_Err gf_odf_write_lang(GF_BitStream *bs, GF_Language *ld)
{
	GF_Err e;
	u32 size;
	if (!ld) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ld, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ld->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, ld->langCode, 24);
	return GF_OK;
}



GF_Descriptor *gf_odf_new_auxvid()
{
	GF_AuxVideoDescriptor *newDesc;
	GF_SAFEALLOC(newDesc, GF_AuxVideoDescriptor);
	if (!newDesc) return NULL;
	newDesc->tag = GF_ODF_AUX_VIDEO_DATA;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_auxvid(GF_AuxVideoDescriptor *ld)
{
	if (!ld) return GF_BAD_PARAM;
	gf_free(ld);
	return GF_OK;
}

GF_Err gf_odf_read_auxvid(GF_BitStream *bs, GF_AuxVideoDescriptor *ld, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!ld) return GF_BAD_PARAM;

	ld->aux_video_type = gf_bs_read_int(bs, 8);
	ld->position_offset_h = gf_bs_read_int(bs, 8);
	ld->position_offset_v = gf_bs_read_int(bs, 8);
	nbBytes += 3;
	switch (ld->aux_video_type) {
	case 0x10:
		ld->kfar = gf_bs_read_int(bs, 8);
		ld->knear = gf_bs_read_int(bs, 8);
		nbBytes += 2;
		break;
	case 0x11:
		ld->parallax_zero = gf_bs_read_int(bs, 16);
		ld->parallax_scale = gf_bs_read_int(bs, 16);
		ld->dref = gf_bs_read_int(bs, 16);
		ld->wref = gf_bs_read_int(bs, 16);
		nbBytes += 8;
		break;
	}
	while (nbBytes < DescSize) {
		gf_bs_read_int(bs, 8);
		nbBytes ++;
	}
	return GF_OK;
}

GF_Err gf_odf_size_auxvid(GF_AuxVideoDescriptor *ld, u32 *outSize)
{
	if (!ld) return GF_BAD_PARAM;
	switch (ld->aux_video_type) {
	case 0x10:
		*outSize = 5;
		break;
	case 0x11:
		*outSize = 11;
		break;
	default:
		*outSize = 3;
		break;
	}
	return GF_OK;
}

GF_Err gf_odf_write_auxvid(GF_BitStream *bs, GF_AuxVideoDescriptor *ld)
{
	GF_Err e;
	u32 size;
	if (!ld) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)ld, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ld->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, ld->aux_video_type, 8);
	gf_bs_write_int(bs, ld->position_offset_h, 8);
	gf_bs_write_int(bs, ld->position_offset_v, 8);
	switch (ld->aux_video_type) {
	case 0x10:
		gf_bs_write_int(bs, ld->kfar, 8);
		gf_bs_write_int(bs, ld->knear, 8);
		break;
	case 0x11:
		gf_bs_write_int(bs, ld->parallax_zero, 16);
		gf_bs_write_int(bs, ld->parallax_scale, 16);
		gf_bs_write_int(bs, ld->dref, 16);
		gf_bs_write_int(bs, ld->wref, 16);
		break;
	}
	return GF_OK;
}



GF_Descriptor *gf_odf_new_muxinfo()
{
	GF_MuxInfo *newDesc = (GF_MuxInfo *) gf_malloc(sizeof(GF_MuxInfo));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_MuxInfo));
	newDesc->tag = GF_ODF_MUXINFO_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_muxinfo(GF_MuxInfo *mi)
{
	if (!mi) return GF_BAD_PARAM;
	if (mi->file_name) gf_free(mi->file_name);
	if (mi->streamFormat) gf_free(mi->streamFormat);
	if (mi->textNode) gf_free(mi->textNode);
	if (mi->fontNode) gf_free(mi->fontNode);
	gf_free(mi);
	return GF_OK;
}

GF_Err gf_odf_read_muxinfo(GF_BitStream *bs, GF_MuxInfo *mi, u32 DescSize)
{
	return GF_OK;
}
GF_Err gf_odf_size_muxinfo(GF_MuxInfo *mi, u32 *outSize)
{
	*outSize = 0;
	return GF_OK;
}
GF_Err gf_odf_write_muxinfo(GF_BitStream *bs, GF_MuxInfo *mi)
{
	return GF_OK;
}

GF_Descriptor *gf_odf_New_ElemMask()
{
	GF_ElementaryMask *newDesc = (GF_ElementaryMask*) gf_malloc (sizeof(GF_ElementaryMask));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_ElementaryMask));
	newDesc->tag = GF_ODF_ELEM_MASK_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_ElemMask(GF_ElementaryMask *desc)
{
	if (desc->node_name) gf_free(desc->node_name);
	gf_free(desc);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_bifs_cfg()
{
	GF_BIFSConfig *newDesc = (GF_BIFSConfig *) gf_malloc(sizeof(GF_BIFSConfig));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_BIFSConfig));
	newDesc->tag = GF_ODF_BIFS_CFG_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_bifs_cfg(GF_BIFSConfig *desc)
{
	if (desc->elementaryMasks) {
		u32 i, count = gf_list_count(desc->elementaryMasks);
		for (i=0; i<count; i++) {
			GF_ElementaryMask *tmp = (GF_ElementaryMask *)gf_list_get(desc->elementaryMasks, i);
			if (tmp->node_name) gf_free(tmp->node_name);
			gf_free(tmp);
		}
		gf_list_del(desc->elementaryMasks);
	}
	gf_free(desc);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_laser_cfg()
{
	GF_LASERConfig *newDesc = (GF_LASERConfig *) gf_malloc(sizeof(GF_LASERConfig));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_LASERConfig));
	newDesc->tag = GF_ODF_LASER_CFG_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_laser_cfg(GF_LASERConfig *desc)
{
	gf_free(desc);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_ui_cfg()
{
	GF_UIConfig *newDesc = (GF_UIConfig *) gf_malloc(sizeof(GF_UIConfig));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_UIConfig));
	newDesc->tag = GF_ODF_UI_CFG_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_ui_cfg(GF_UIConfig *desc)
{
	if (desc->deviceName) gf_free(desc->deviceName);
	if (desc->ui_data) gf_free(desc->ui_data);
	gf_free(desc);
	return GF_OK;
}

#ifndef GPAC_MINIMAL_ODF






GF_Descriptor *gf_odf_new_cc()
{
	GF_CCDescriptor *newDesc = (GF_CCDescriptor *) gf_malloc(sizeof(GF_CCDescriptor));
	if (!newDesc) return NULL;

	newDesc->contentClassificationData = NULL;
	newDesc->dataLength = 0;
	newDesc->classificationEntity = 0;
	newDesc->classificationTable = 0;
	newDesc->tag = GF_ODF_CC_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_cc(GF_CCDescriptor *ccd)
{
	if (!ccd) return GF_BAD_PARAM;
	if (ccd->contentClassificationData) gf_free(ccd->contentClassificationData);
	gf_free(ccd);
	return GF_OK;
}

GF_Err gf_odf_read_cc(GF_BitStream *bs, GF_CCDescriptor *ccd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!ccd) return GF_BAD_PARAM;

	ccd->classificationEntity = gf_bs_read_int(bs, 32);
	ccd->classificationTable = gf_bs_read_int(bs, 16);
	nbBytes += 6;
	ccd->dataLength = DescSize - 6;
	ccd->contentClassificationData = (char*)gf_malloc(sizeof(char) * ccd->dataLength);
	if (!ccd->contentClassificationData) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, ccd->contentClassificationData, ccd->dataLength);
	nbBytes += ccd->dataLength;

	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_cc(GF_CCDescriptor *ccd, u32 *outSize)
{
	if (!ccd) return GF_BAD_PARAM;
	*outSize = 6 + ccd->dataLength;
	return GF_OK;
}

GF_Err gf_odf_write_cc(GF_BitStream *bs, GF_CCDescriptor *ccd)
{
	u32 size;
	GF_Err e;
	if (!ccd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ccd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ccd->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, ccd->classificationEntity, 32);
	gf_bs_write_int(bs, ccd->classificationTable, 16);
	gf_bs_write_data(bs, ccd->contentClassificationData, ccd->dataLength);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_cc_date()
{
	GF_CC_Date *newDesc = (GF_CC_Date *) gf_malloc(sizeof(GF_CC_Date));
	if (!newDesc) return NULL;
	memset(newDesc->contentCreationDate, 0, 5);
	newDesc->tag = GF_ODF_CC_DATE_TAG;
	return (GF_Descriptor *) newDesc;
}


GF_Err gf_odf_del_cc_date(GF_CC_Date *cdd)
{
	if (!cdd) return GF_BAD_PARAM;
	gf_free(cdd);
	return GF_OK;
}

GF_Err gf_odf_read_cc_date(GF_BitStream *bs, GF_CC_Date *cdd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!cdd) return GF_BAD_PARAM;

	gf_bs_read_data(bs, cdd->contentCreationDate, DATE_CODING_BIT_LEN);
	nbBytes += DATE_CODING_BIT_LEN / 8;
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_cc_date(GF_CC_Date *cdd, u32 *outSize)
{
	if (!cdd) return GF_BAD_PARAM;
	*outSize = (DATE_CODING_BIT_LEN / 8);
	return GF_OK;
}

GF_Err gf_odf_write_cc_date(GF_BitStream *bs, GF_CC_Date *cdd)
{
	u32 size;
	GF_Err e;
	if (!cdd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)cdd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, cdd->tag, size);
	if (e) return e;

	gf_bs_write_data(bs, cdd->contentCreationDate , DATE_CODING_BIT_LEN);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_cc_name()
{
	GF_CC_Name *newDesc = (GF_CC_Name *) gf_malloc(sizeof(GF_CC_Name));
	if (!newDesc) return NULL;

	newDesc->ContentCreators = gf_list_new();
	if (! newDesc->ContentCreators) {
		gf_free(newDesc);
		return NULL;
	}
	newDesc->tag = GF_ODF_CC_NAME_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_cc_name(GF_CC_Name *cnd)
{
	u32 i;
	GF_ContentCreatorInfo *tmp;
	if (!cnd) return GF_BAD_PARAM;

	i=0;
	while ((tmp = (GF_ContentCreatorInfo *)gf_list_enum(cnd->ContentCreators, &i))) {
		if (tmp->contentCreatorName) gf_free(tmp->contentCreatorName);
		gf_free(tmp);
	}
	gf_list_del(cnd->ContentCreators);
	gf_free(cnd);
	return GF_OK;
}

GF_Err gf_odf_read_cc_name(GF_BitStream *bs, GF_CC_Name *cnd, u32 DescSize)
{
	GF_Err e;
	u32 i, count, len, nbBytes = 0;
	if (!cnd) return GF_BAD_PARAM;

	count = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	for (i = 0; i< count; i++) {
		GF_ContentCreatorInfo *tmp = (GF_ContentCreatorInfo*)gf_malloc(sizeof(GF_ContentCreatorInfo));
		if (! tmp) return GF_OUT_OF_MEM;
		memset(tmp , 0, sizeof(GF_ContentCreatorInfo));
		tmp->langCode = gf_bs_read_int(bs, 24);
		tmp->isUTF8 = gf_bs_read_int(bs, 1);
		/*aligned = */gf_bs_read_int(bs, 7);
		nbBytes += 4;

		e = OD_ReadUTF8String(bs, & tmp->contentCreatorName, tmp->isUTF8, &len);
		if (e) return e;
		nbBytes += len;
		e = gf_list_add(cnd->ContentCreators, tmp);
	}
	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_cc_name(GF_CC_Name *cnd, u32 *outSize)
{
	u32 i;
	GF_ContentCreatorInfo *tmp;
	if (!cnd) return GF_BAD_PARAM;

	*outSize = 1;
	i=0;
	while ((tmp = (GF_ContentCreatorInfo *)gf_list_enum(cnd->ContentCreators, &i))) {
		*outSize += 4 + OD_SizeUTF8String(tmp->contentCreatorName, tmp->isUTF8);
	}
	return GF_OK;
}

GF_Err gf_odf_write_cc_name(GF_BitStream *bs, GF_CC_Name *cnd)
{
	GF_Err e;
	GF_ContentCreatorInfo *tmp;
	u32 i, size;
	if (!cnd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)cnd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, cnd->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, gf_list_count(cnd->ContentCreators), 8);

	i=0;
	while ((tmp = (GF_ContentCreatorInfo *)gf_list_enum(cnd->ContentCreators, &i))) {
		gf_bs_write_int(bs, tmp->langCode, 24);
		gf_bs_write_int(bs, tmp->isUTF8, 1);
		gf_bs_write_int(bs, 0, 7);		//aligned
		OD_WriteUTF8String(bs, tmp->contentCreatorName, tmp->isUTF8);
	}
	return GF_OK;
}
	

GF_Descriptor *gf_odf_new_ci()
{
	GF_CIDesc *newDesc = (GF_CIDesc *) gf_malloc(sizeof(GF_CIDesc));
	if (!newDesc) return NULL;

	newDesc->compatibility = 0;
	newDesc->contentIdentifier = NULL;
	newDesc->tag = GF_ODF_CI_TAG;
	newDesc->contentIdentifierFlag = 0;
	newDesc->contentIdentifierType = 0;
	newDesc->contentType = 0;
	newDesc->contentTypeFlag = 0;
	newDesc->protectedContent = 0;
	return (GF_Descriptor *) newDesc;
}


GF_Err gf_odf_del_ci(GF_CIDesc *cid)
{
	if (!cid) return GF_BAD_PARAM;

	if (cid->contentIdentifier) gf_free(cid->contentIdentifier);
	gf_free(cid);
	return GF_OK;
}


GF_Err gf_odf_read_ci(GF_BitStream *bs, GF_CIDesc *cid, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! cid) return GF_BAD_PARAM;

	cid->compatibility = gf_bs_read_int(bs, 2);	//MUST BE NULL
	if (cid->compatibility) return GF_ODF_INVALID_DESCRIPTOR;

	cid->contentTypeFlag = gf_bs_read_int(bs, 1);
	cid->contentIdentifierFlag = gf_bs_read_int(bs, 1);
	cid->protectedContent = gf_bs_read_int(bs, 1);
	/*reserved = */gf_bs_read_int(bs, 3);
	nbBytes += 1;

	if (cid->contentTypeFlag) {
		cid->contentType = gf_bs_read_int(bs, 8);
		nbBytes += 1;
	}
	if (cid->contentIdentifierFlag) {
		cid->contentIdentifierType = gf_bs_read_int(bs, 8);
		cid->contentIdentifier = (char*)gf_malloc(DescSize - 2 - cid->contentTypeFlag);
		if (! cid->contentIdentifier) return GF_OUT_OF_MEM;

		gf_bs_read_data(bs, cid->contentIdentifier, DescSize - 2 - cid->contentTypeFlag);
		nbBytes += DescSize - 1 - cid->contentTypeFlag;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_ci(GF_CIDesc *cid, u32 *outSize)
{
	if (! cid) return GF_BAD_PARAM;

	*outSize = 1;
	if (cid->contentTypeFlag) *outSize += 1;

	if (cid->contentIdentifierFlag) 
		*outSize += strlen((const char*)cid->contentIdentifier) - 1 - cid->contentTypeFlag;
	return GF_OK;
}

GF_Err gf_odf_write_ci(GF_BitStream *bs, GF_CIDesc *cid)
{
	GF_Err e;
	u32 size;
	if (! cid) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)cid, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, cid->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, cid->compatibility, 2);
	gf_bs_write_int(bs, cid->contentTypeFlag, 1);
	gf_bs_write_int(bs, cid->contentIdentifierFlag, 1);
	gf_bs_write_int(bs, cid->protectedContent, 1);
	gf_bs_write_int(bs, 7, 3);		//reserved 0b111 = 7

	if (cid->contentTypeFlag) {
		gf_bs_write_int(bs, cid->contentType, 8);
	}

	if (cid->contentIdentifierFlag) {
		gf_bs_write_int(bs, cid->contentIdentifierType, 8);
		gf_bs_write_data(bs, cid->contentIdentifier, size - 2 - cid->contentTypeFlag);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_exp_text()
{
	GF_ExpandedTextual *newDesc = (GF_ExpandedTextual *) gf_malloc(sizeof(GF_ExpandedTextual));
	if (!newDesc) return NULL;

	newDesc->itemDescriptionList = gf_list_new();
	if (! newDesc->itemDescriptionList) {
		gf_free(newDesc);
		return NULL;
	}
	newDesc->itemTextList = gf_list_new();
	if (! newDesc->itemTextList) {
		gf_free(newDesc->itemDescriptionList);
		gf_free(newDesc);
		return NULL;
	}
	newDesc->isUTF8 = 0;
	newDesc->langCode = 0;
	newDesc->NonItemText = NULL;
	newDesc->tag = GF_ODF_TEXT_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_exp_text(GF_ExpandedTextual *etd)
{
	if (!etd) return GF_BAD_PARAM;

	while (gf_list_count(etd->itemDescriptionList)) {
		GF_ETD_ItemText *tmp = (GF_ETD_ItemText*)gf_list_get(etd->itemDescriptionList, 0);
		if (tmp) {
			if (tmp->text) gf_free(tmp->text);
			gf_free(tmp);
		}
		gf_list_rem(etd->itemDescriptionList, 0);
	}
	gf_list_del(etd->itemDescriptionList);

	while (gf_list_count(etd->itemTextList)) {
		GF_ETD_ItemText *tmp = (GF_ETD_ItemText*)gf_list_get(etd->itemTextList, 0);
		if (tmp) {
			if (tmp->text) gf_free(tmp->text);
			gf_free(tmp);
		}
		gf_list_rem(etd->itemTextList, 0);
	}
	gf_list_del(etd->itemTextList);

	if (etd->NonItemText) gf_free(etd->NonItemText);
	gf_free(etd);
	return GF_OK;
}

GF_Err gf_odf_read_exp_text(GF_BitStream *bs, GF_ExpandedTextual *etd, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0;
	u32 i, len, nonLen, count;
	if (!etd) return GF_BAD_PARAM;

	etd->langCode = gf_bs_read_int(bs, 24);
	etd->isUTF8 = gf_bs_read_int(bs, 1);
	/*aligned = */gf_bs_read_int(bs, 7);
	count = gf_bs_read_int(bs, 8);
	nbBytes += 5;

	for (i = 0; i< count; i++) {
		//description
		GF_ETD_ItemText *description, *Text;
		description = (GF_ETD_ItemText*)gf_malloc(sizeof(GF_ETD_ItemText));
		if (! description) return GF_OUT_OF_MEM;
		description->text = NULL;
		e = OD_ReadUTF8String(bs, & description->text, etd->isUTF8, &len); 
		if (e) return e;
		e = gf_list_add(etd->itemDescriptionList, description);
		if (e) return e;
		nbBytes += len;

		//text
		Text = (GF_ETD_ItemText*)gf_malloc(sizeof(GF_ETD_ItemText));
		if (! Text) return GF_OUT_OF_MEM;
		Text->text = NULL;
		e = OD_ReadUTF8String(bs, & Text->text, etd->isUTF8, &len);
		if (e) return e;
		e = gf_list_add(etd->itemTextList, Text);
		if (e) return e;
		nbBytes += len;
	}
	len = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	nonLen = 0;
	while (len == 255) {
		nonLen += len;
		len = gf_bs_read_int(bs, 8);
		nbBytes += 1;
	}
	nonLen += len;
	if (nonLen) {
		//here we have no choice but do the job ourselves
		//because the length is not encoded on 8 bits
		etd->NonItemText = (char *) gf_malloc(sizeof(char) * (1+nonLen) * (etd->isUTF8 ? 1 : 2));
		if (! etd->NonItemText) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, etd->NonItemText, nonLen * (etd->isUTF8 ? 1 : 2));
		nbBytes += nonLen * (etd->isUTF8 ? 1 : 2);
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


GF_Err gf_odf_size_exp_text(GF_ExpandedTextual *etd, u32 *outSize)
{
	u32 i, len, nonLen, lentmp, count;
	GF_ETD_ItemText *tmp;
	if (!etd) return GF_BAD_PARAM;

	*outSize = 5;
	if (gf_list_count(etd->itemDescriptionList) != gf_list_count(etd->itemTextList)) return GF_ODF_INVALID_DESCRIPTOR;

	count = gf_list_count(etd->itemDescriptionList);
	for (i=0; i<count; i++) {
		tmp = (GF_ETD_ItemText *)gf_list_get(etd->itemDescriptionList, i);
		*outSize += OD_SizeUTF8String(tmp->text, etd->isUTF8);
		tmp = (GF_ETD_ItemText*)gf_list_get(etd->itemTextList, i);
		*outSize += OD_SizeUTF8String(tmp->text, etd->isUTF8);
	}
	*outSize += 1;
	if (etd->NonItemText) {
		if (etd->isUTF8) {
			nonLen = strlen((const char*)etd->NonItemText);
		} else {
			nonLen = gf_utf8_wcslen((const unsigned short*)etd->NonItemText);
		}
	} else {
		nonLen = 0;
	}
	len = 255;
	lentmp = nonLen;
	if (lentmp < 255) { 
		len = lentmp;
	}
	while (len == 255) {
		*outSize += 1;
		lentmp -= 255;
		if (lentmp < 255) { 
			len = lentmp;
		}
	}
	*outSize += nonLen * (etd->isUTF8 ? 1 : 2);
	return GF_OK;
}

GF_Err gf_odf_write_exp_text(GF_BitStream *bs, GF_ExpandedTextual *etd)
{
	GF_Err e;
	u32 size, i, len, nonLen, lentmp, count;
	GF_ETD_ItemText *tmp;
	if (!etd) return GF_BAD_PARAM;

	if (gf_list_count(etd->itemDescriptionList) != gf_list_count(etd->itemTextList)) return GF_ODF_INVALID_DESCRIPTOR;

	e = gf_odf_size_descriptor((GF_Descriptor *)etd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, etd->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, etd->langCode, 24);
	gf_bs_write_int(bs, etd->isUTF8, 1);
	gf_bs_write_int(bs, 0, 7);		//aligned
	gf_bs_write_int(bs, gf_list_count(etd->itemDescriptionList), 8);

	count = gf_list_count(etd->itemDescriptionList);
	for (i=0; i<count; i++) {
		tmp = (GF_ETD_ItemText *)gf_list_get(etd->itemDescriptionList, i);
		OD_WriteUTF8String(bs, tmp->text, etd->isUTF8);
		tmp = (GF_ETD_ItemText*)gf_list_get(etd->itemTextList, i);
		OD_WriteUTF8String(bs, tmp->text, etd->isUTF8);
	}
	if (etd->NonItemText) {
		nonLen = strlen((const char*)etd->NonItemText) + 1;
		if (etd->isUTF8) {
			nonLen = strlen((const char*)etd->NonItemText);
		} else {
			nonLen = gf_utf8_wcslen((const unsigned short*)etd->NonItemText);
		}
	} else {
		nonLen = 0;
	}
	lentmp = nonLen;
	len = lentmp < 255 ? lentmp : 255;
	while (len == 255) {
		gf_bs_write_int(bs, len, 8);
		lentmp -= len;
		len = lentmp < 255 ? lentmp : 255;
	}
	gf_bs_write_int(bs, len, 8);
	gf_bs_write_data(bs, etd->NonItemText, nonLen * (etd->isUTF8 ? 1 : 2));
	return GF_OK;
}

GF_Descriptor *gf_odf_new_pl_ext()
{
	GF_PLExt *newDesc = (GF_PLExt *) gf_malloc(sizeof(GF_PLExt));
	if (!newDesc) return NULL;
	newDesc->AudioProfileLevelIndication = 0;
	newDesc->GraphicsProfileLevelIndication = 0;
	newDesc->MPEGJProfileLevelIndication = 0;
	newDesc->ODProfileLevelIndication = 0;
	newDesc->profileLevelIndicationIndex = 0;
	newDesc->SceneProfileLevelIndication = 0;
	newDesc->VisualProfileLevelIndication = 0;
	newDesc->tag = GF_ODF_EXT_PL_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_pl_ext(GF_PLExt *pld)
{
	if (!pld) return GF_BAD_PARAM;

	gf_free(pld);
	return GF_OK;
}

GF_Err gf_odf_read_pl_ext(GF_BitStream *bs, GF_PLExt *pld, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!pld) return GF_BAD_PARAM;

	pld->profileLevelIndicationIndex = gf_bs_read_int(bs, 8);
	pld->ODProfileLevelIndication = gf_bs_read_int(bs, 8);
	pld->SceneProfileLevelIndication = gf_bs_read_int(bs, 8);
	pld->AudioProfileLevelIndication = gf_bs_read_int(bs, 8);
	pld->VisualProfileLevelIndication = gf_bs_read_int(bs, 8);
	pld->GraphicsProfileLevelIndication = gf_bs_read_int(bs, 8);
	pld->MPEGJProfileLevelIndication = gf_bs_read_int(bs, 8);

	nbBytes += 7;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_pl_ext(GF_PLExt *pld, u32 *outSize)
{
	if (!pld) return GF_BAD_PARAM;

	*outSize = 7;
	return GF_OK;
}

GF_Err gf_odf_write_pl_ext(GF_BitStream *bs, GF_PLExt *pld)
{
	GF_Err e;
	u32 size;
	if (!pld) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)pld, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, pld->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, pld->profileLevelIndicationIndex, 8);
	gf_bs_write_int(bs, pld->ODProfileLevelIndication, 8);
	gf_bs_write_int(bs, pld->SceneProfileLevelIndication, 8);
	gf_bs_write_int(bs, pld->AudioProfileLevelIndication, 8);
	gf_bs_write_int(bs, pld->VisualProfileLevelIndication, 8);
	gf_bs_write_int(bs, pld->GraphicsProfileLevelIndication, 8);
	gf_bs_write_int(bs, pld->MPEGJProfileLevelIndication, 8);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_ipi_ptr()
{
	GF_IPIPtr *newDesc = (GF_IPIPtr *) gf_malloc(sizeof(GF_IPIPtr));
	if (!newDesc) return NULL;
	newDesc->IPI_ES_Id = 0;
	newDesc->tag = GF_ODF_IPI_PTR_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_ipi_ptr(GF_IPIPtr *ipid)
{
	if (!ipid) return GF_BAD_PARAM;
	gf_free(ipid);
	return GF_OK;
}

GF_Err gf_odf_read_ipi_ptr(GF_BitStream *bs, GF_IPIPtr *ipid, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! ipid) return GF_BAD_PARAM;

	ipid->IPI_ES_Id = gf_bs_read_int(bs, 16);
	nbBytes += 2;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_ipi_ptr(GF_IPIPtr *ipid, u32 *outSize)
{
	if (! ipid) return GF_BAD_PARAM;
	*outSize = 2;
	return GF_OK;
}

GF_Err gf_odf_write_ipi_ptr(GF_BitStream *bs, GF_IPIPtr *ipid)
{
	GF_Err e;
	u32 size;
	if (! ipid) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)ipid, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipid->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, ipid->IPI_ES_Id, 16);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_ipmp()
{
	GF_IPMP_Descriptor *newDesc;
	GF_SAFEALLOC(newDesc, GF_IPMP_Descriptor);
	if (!newDesc) return NULL;

	newDesc->ipmpx_data = gf_list_new();
	newDesc->tag = GF_ODF_IPMP_TAG;
	return (GF_Descriptor *) newDesc;
}
GF_Err gf_odf_del_ipmp(GF_IPMP_Descriptor *ipmp)
{
	if (!ipmp) return GF_BAD_PARAM;
	if (ipmp->opaque_data) gf_free(ipmp->opaque_data);
	/*TODO DELETE IPMPX*/
	while (gf_list_count(ipmp->ipmpx_data)) {
		GF_IPMPX_Data *p = (GF_IPMPX_Data *)gf_list_get(ipmp->ipmpx_data, 0);
		gf_list_rem(ipmp->ipmpx_data, 0);
		gf_ipmpx_data_del(p);
	}
	gf_list_del(ipmp->ipmpx_data);
	gf_free(ipmp);
	return GF_OK;
}

GF_Err gf_odf_read_ipmp(GF_BitStream *bs, GF_IPMP_Descriptor *ipmp, u32 DescSize)
{
	u32 size;
	u64 nbBytes = 0;
	if (!ipmp) return GF_BAD_PARAM;

	ipmp->IPMP_DescriptorID = gf_bs_read_int(bs, 8);
	ipmp->IPMPS_Type = gf_bs_read_int(bs, 16);
	nbBytes += 3;
	size = DescSize - 3;

	/*IPMPX escape*/
	if ((ipmp->IPMP_DescriptorID==0xFF) && (ipmp->IPMPS_Type==0xFFFF)) {
		ipmp->IPMP_DescriptorIDEx = gf_bs_read_int(bs, 16);
		gf_bs_read_data(bs, (char*)ipmp->IPMP_ToolID, 16);
		ipmp->control_point = gf_bs_read_int(bs, 8);
		nbBytes += 19;
		if (ipmp->control_point) {
			ipmp->cp_sequence_code = gf_bs_read_int(bs, 8);
			nbBytes += 1;
		}
		while (nbBytes<DescSize) {
			u64 pos;
			GF_Err e;
			GF_IPMPX_Data *p;
			pos = gf_bs_get_position(bs);
			e = gf_ipmpx_data_parse(bs, &p);
			if (e) return e;
			gf_list_add(ipmp->ipmpx_data, p);
			nbBytes += gf_bs_get_position(bs) - pos;
		}
	}
	/*URL*/
	else if (! ipmp->IPMPS_Type) {
		ipmp->opaque_data = (char*)gf_malloc(size + 1);
		if (! ipmp->opaque_data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ipmp->opaque_data, size);
		nbBytes += size;
		ipmp->opaque_data[size] = 0;
		ipmp->opaque_data_size = size;
		
	}
	/*data*/
	else {
		ipmp->opaque_data_size = size;
		ipmp->opaque_data = (char*)gf_malloc(size);
		if (! ipmp->opaque_data) return GF_OUT_OF_MEM;
		gf_bs_read_data(bs, ipmp->opaque_data, size);
		nbBytes += size;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_ipmp(GF_IPMP_Descriptor *ipmp, u32 *outSize)
{
	u32 i, s;
	if (!ipmp) return GF_BAD_PARAM;

	*outSize = 3;
	/*IPMPX escape*/
	if ((ipmp->IPMP_DescriptorID==0xFF) && (ipmp->IPMPS_Type==0xFFFF)) {
		GF_IPMPX_Data *p;
		*outSize += 19;
		if (ipmp->control_point) *outSize += 1;
		s = 0;
		i=0;
		while ((p = (GF_IPMPX_Data *)gf_list_enum(ipmp->ipmpx_data, &i))) {
			s += gf_ipmpx_data_full_size(p);
		}
		(*outSize) += s;
	}
	else if (! ipmp->IPMPS_Type) {
		if (!ipmp->opaque_data) return GF_ODF_INVALID_DESCRIPTOR;
		*outSize += strlen(ipmp->opaque_data);
	} else {
		*outSize += ipmp->opaque_data_size;
	}
	return GF_OK;
}

GF_Err gf_odf_write_ipmp(GF_BitStream *bs, GF_IPMP_Descriptor *ipmp)
{
	GF_Err e;
	u32 size, i;
	if (!ipmp) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ipmp, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmp->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, ipmp->IPMP_DescriptorID, 8);
	gf_bs_write_int(bs, ipmp->IPMPS_Type, 16);

	if ((ipmp->IPMP_DescriptorID==0xFF) && (ipmp->IPMPS_Type==0xFFFF)) {
		GF_IPMPX_Data *p;
		gf_bs_write_int(bs, ipmp->IPMP_DescriptorIDEx, 16);
		gf_bs_write_data(bs, (char*)ipmp->IPMP_ToolID, 16);
		gf_bs_write_int(bs, ipmp->control_point, 8);
		if (ipmp->control_point) gf_bs_write_int(bs, ipmp->cp_sequence_code, 8);

		i=0;
		while ((p = (GF_IPMPX_Data *) gf_list_enum(ipmp->ipmpx_data, &i))) {
			gf_ipmpx_data_write(bs, p);
		}
	}
	else if (!ipmp->IPMPS_Type) {
		if (!ipmp->opaque_data) return GF_ODF_INVALID_DESCRIPTOR;
		gf_bs_write_data(bs, ipmp->opaque_data, strlen(ipmp->opaque_data));
	} else {
		gf_bs_write_data(bs, ipmp->opaque_data, ipmp->opaque_data_size);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_ipmp_ptr()
{
	GF_IPMPPtr *newDesc;
	GF_SAFEALLOC(newDesc, GF_IPMPPtr);
	if (!newDesc) return NULL;
	newDesc->tag = GF_ODF_IPMP_PTR_TAG;
	return (GF_Descriptor *) newDesc;
}
GF_Err gf_odf_del_ipmp_ptr(GF_IPMPPtr *ipmpd)
{
	if (!ipmpd) return GF_BAD_PARAM;
	gf_free(ipmpd);
	return GF_OK;
}

GF_Err gf_odf_read_ipmp_ptr(GF_BitStream *bs, GF_IPMPPtr *ipmpd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (! ipmpd) return GF_BAD_PARAM;
	ipmpd->IPMP_DescriptorID = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	if (ipmpd->IPMP_DescriptorID == 0xFF) {
		ipmpd->IPMP_DescriptorIDEx = gf_bs_read_int(bs, 16);
		ipmpd->IPMP_ES_ID = gf_bs_read_int(bs, 16);
		nbBytes += 4;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}
GF_Err gf_odf_size_ipmp_ptr(GF_IPMPPtr *ipmpd, u32 *outSize)
{
	if (! ipmpd) return GF_BAD_PARAM;
	*outSize = 1;
	if (ipmpd->IPMP_DescriptorID == 0xFF) *outSize += 4;
	return GF_OK;
}
GF_Err gf_odf_write_ipmp_ptr(GF_BitStream *bs, GF_IPMPPtr *ipmpd)
{
	GF_Err e;
	u32 size;
	if (! ipmpd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ipmpd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmpd->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, ipmpd->IPMP_DescriptorID, 8);
	if (ipmpd->IPMP_DescriptorID == 0xFF) {
		gf_bs_write_int(bs, ipmpd->IPMP_DescriptorIDEx, 16);
		gf_bs_write_int(bs, ipmpd->IPMP_ES_ID, 16);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_kw()
{
	GF_KeyWord *newDesc = (GF_KeyWord *) gf_malloc(sizeof(GF_KeyWord));
	if (!newDesc) return NULL;

	newDesc->keyWordsList = gf_list_new();
	if (! newDesc->keyWordsList) {
		gf_free(newDesc);
		return NULL;
	}
	newDesc->isUTF8 = 0;
	newDesc->languageCode = 0;
	newDesc->tag = GF_ODF_KW_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_kw(GF_KeyWord *kwd)
{
	if (!kwd) return GF_BAD_PARAM;

	while (gf_list_count(kwd->keyWordsList)) {
		GF_KeyWordItem *tmp = (GF_KeyWordItem*)gf_list_get(kwd->keyWordsList, 0);
		if (tmp) {
			if (tmp->keyWord) gf_free(tmp->keyWord);
			gf_free(tmp);
		}
	}
	gf_list_del(kwd->keyWordsList);
	gf_free(kwd);
	return GF_OK;
}

GF_Err gf_odf_read_kw(GF_BitStream *bs, GF_KeyWord *kwd, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0, i, kwcount, len;
	if (!kwd) return GF_BAD_PARAM;

	kwd->languageCode = gf_bs_read_int(bs, 24);
	kwd->isUTF8 = gf_bs_read_int(bs, 1);
	/*aligned = */gf_bs_read_int(bs, 7);
	kwcount = gf_bs_read_int(bs, 8);
	nbBytes += 5;

	for (i = 0 ; i < kwcount; i++) {
		GF_KeyWordItem *tmp = (GF_KeyWordItem*)gf_malloc(sizeof(GF_KeyWordItem));
		if (! tmp) return GF_OUT_OF_MEM;
		e = OD_ReadUTF8String(bs, & tmp->keyWord, kwd->isUTF8, &len);
		if (e) return e;
		e = gf_list_add(kwd->keyWordsList, tmp);
		if (e) return e;
		nbBytes  += len;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


GF_Err gf_odf_size_kw(GF_KeyWord *kwd, u32 *outSize)
{
	u32 i;
	GF_KeyWordItem *tmp;
	if (!kwd) return GF_BAD_PARAM;

	*outSize = 5;
	i=0;
	while ((tmp = (GF_KeyWordItem *)gf_list_enum(kwd->keyWordsList, &i))) {
		*outSize += OD_SizeUTF8String(tmp->keyWord, kwd->isUTF8);
	}
	return GF_OK;
}
GF_Err gf_odf_write_kw(GF_BitStream *bs, GF_KeyWord *kwd)
{
	GF_Err e;
	u32 size, i;
	GF_KeyWordItem *tmp;
	if (!kwd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)kwd, &size);
	assert(e == GF_OK);
	e = gf_odf_write_base_descriptor(bs, kwd->tag, size);
	assert(e == GF_OK);

	gf_bs_write_int(bs, kwd->languageCode, 24);
	gf_bs_write_int(bs, kwd->isUTF8, 1);
	gf_bs_write_int(bs, 0, 7);		//aligned(8)
	gf_bs_write_int(bs, gf_list_count(kwd->keyWordsList), 8);

	i=0;
	while ((tmp = (GF_KeyWordItem *)gf_list_enum(kwd->keyWordsList, &i))) {
		OD_WriteUTF8String(bs, tmp->keyWord, kwd->isUTF8);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_oci_date()
{
	GF_OCI_Data *newDesc = (GF_OCI_Data *) gf_malloc(sizeof(GF_OCI_Data));
	if (!newDesc) return NULL;
	memset(newDesc->OCICreationDate, 0, 5);
	newDesc->tag = GF_ODF_OCI_DATE_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_oci_date(GF_OCI_Data *ocd)
{
	if (!ocd) return GF_BAD_PARAM;
	gf_free(ocd);
	return GF_OK;
}

GF_Err gf_odf_read_oci_date(GF_BitStream *bs, GF_OCI_Data *ocd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!ocd) return GF_BAD_PARAM;

	gf_bs_read_data(bs, ocd->OCICreationDate, DATE_CODING_BIT_LEN);
	nbBytes += DATE_CODING_BIT_LEN / 8;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_oci_date(GF_OCI_Data *ocd, u32 *outSize)
{
	if (!ocd) return GF_BAD_PARAM;
	*outSize = (DATE_CODING_BIT_LEN / 8);
	return GF_OK;
}

GF_Err gf_odf_write_oci_date(GF_BitStream *bs, GF_OCI_Data *ocd)
{
	GF_Err e;
	u32 size;
	if (!ocd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ocd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ocd->tag, size);
	if (e) return e;
	gf_bs_write_data(bs, ocd->OCICreationDate , DATE_CODING_BIT_LEN);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_oci_name()
{
	GF_OCICreators *newDesc = (GF_OCICreators *) gf_malloc(sizeof(GF_OCICreators));
	if (!newDesc) return NULL;

	newDesc->OCICreators = gf_list_new();
	if (! newDesc->OCICreators) {
		gf_free(newDesc);
		return NULL;
	}
	newDesc->tag = GF_ODF_OCI_NAME_TAG;
	return (GF_Descriptor *) newDesc;
}
GF_Err gf_odf_del_oci_name(GF_OCICreators *ocn)
{
	u32 i;
	GF_OCICreator_item *tmp;
	if (!ocn) return GF_BAD_PARAM;
	
	i=0;
	while ((tmp = (GF_OCICreator_item *)gf_list_enum(ocn->OCICreators, &i))) {
		if (tmp->OCICreatorName) gf_free(tmp->OCICreatorName);
		gf_free(tmp);
	}
	gf_list_del(ocn->OCICreators);
	gf_free(ocn);
	return GF_OK;
}

GF_Err gf_odf_read_oci_name(GF_BitStream *bs, GF_OCICreators *ocn, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0;
	u32 i, count, len;
	if (!ocn) return GF_BAD_PARAM;

	count = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	for (i = 0; i< count; i++) {
		GF_OCICreator_item *tmp = (GF_OCICreator_item*)gf_malloc(sizeof(GF_OCICreator_item));
		if (! tmp) return GF_OUT_OF_MEM;
		tmp->langCode = gf_bs_read_int(bs, 24);
		tmp->isUTF8 = gf_bs_read_int(bs, 1);
		/*aligned = */gf_bs_read_int(bs, 7);
		nbBytes += 4;
		e = OD_ReadUTF8String(bs, & tmp->OCICreatorName, tmp->isUTF8, &len);
		if (e) return e;
		nbBytes += len;
		e = gf_list_add(ocn->OCICreators, tmp);
		if (e) return e;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_oci_name(GF_OCICreators *ocn, u32 *outSize)
{
	u32 i;
	GF_OCICreator_item *tmp;
	if (!ocn) return GF_BAD_PARAM;

	*outSize = 1;
	i=0;
	while ((tmp = (GF_OCICreator_item *)gf_list_enum(ocn->OCICreators, &i))) {
		*outSize += 4 + OD_SizeUTF8String(tmp->OCICreatorName, tmp->isUTF8);
	}
	return GF_OK;
}

GF_Err gf_odf_write_oci_name(GF_BitStream *bs, GF_OCICreators *ocn)
{
	GF_Err e;
	u32 size;
	u32 i;
	GF_OCICreator_item *tmp;
	if (!ocn) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)ocn, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ocn->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, gf_list_count(ocn->OCICreators), 8);

	i=0;
	while ((tmp = (GF_OCICreator_item *)gf_list_enum(ocn->OCICreators, &i))) {
		gf_bs_write_int(bs, tmp->langCode, 24);
		gf_bs_write_int(bs, tmp->isUTF8, 1);
		gf_bs_write_int(bs, 0, 7);		//aligned
		gf_bs_write_int(bs, strlen(tmp->OCICreatorName) , 8);
		OD_WriteUTF8String(bs, tmp->OCICreatorName, tmp->isUTF8);
	}
	return GF_OK;
}


GF_Descriptor *gf_odf_new_pl_idx()
{
	GF_PL_IDX *newDesc = (GF_PL_IDX *) gf_malloc(sizeof(GF_PL_IDX));
	if (!newDesc) return NULL;
	newDesc->profileLevelIndicationIndex = 0;
	newDesc->tag = GF_ODF_PL_IDX_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_pl_idx(GF_PL_IDX *plid)
{
	if (!plid) return GF_BAD_PARAM;
	gf_free(plid);
	return GF_OK;
}

GF_Err gf_odf_read_pl_idx(GF_BitStream *bs, GF_PL_IDX *plid, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!plid) return GF_BAD_PARAM;

	plid->profileLevelIndicationIndex = gf_bs_read_int(bs, 8);
	nbBytes += 1;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}
GF_Err gf_odf_size_pl_idx(GF_PL_IDX *plid, u32 *outSize)
{
	if (!plid) return GF_BAD_PARAM;
	*outSize = 1;
	return GF_OK;
}
GF_Err gf_odf_write_pl_idx(GF_BitStream *bs, GF_PL_IDX *plid)
{
	GF_Err e;
	u32 size;
	if (!plid) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)plid, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, plid->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, plid->profileLevelIndicationIndex, 8);
	return GF_OK;
}


GF_Descriptor *gf_odf_new_rating()
{
	GF_Rating *newDesc = (GF_Rating *) gf_malloc(sizeof(GF_Rating));
	if (!newDesc) return NULL;

	newDesc->infoLength = 0;
	newDesc->ratingInfo = NULL;
	newDesc->ratingCriteria = 0;
	newDesc->ratingEntity = 0;
	newDesc->tag = GF_ODF_RATING_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_rating(GF_Rating *rd)
{
	if (!rd) return GF_BAD_PARAM;

	if (rd->ratingInfo) gf_free(rd->ratingInfo);
	gf_free(rd);
	return GF_OK;
}

GF_Err gf_odf_read_rating(GF_BitStream *bs, GF_Rating *rd, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!rd) return GF_BAD_PARAM;

	rd->ratingEntity = gf_bs_read_int(bs, 32);
	rd->ratingCriteria = gf_bs_read_int(bs, 16);
	rd->infoLength = DescSize - 6;
	nbBytes += 6;
	
	rd->ratingInfo = (char*)gf_malloc(rd->infoLength);
	if (! rd->ratingInfo) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, rd->ratingInfo, rd->infoLength);
	nbBytes += rd->infoLength;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_rating(GF_Rating *rd, u32 *outSize)
{
	if (!rd) return GF_BAD_PARAM;

	*outSize = 6 + rd->infoLength;
	return GF_OK;
}

GF_Err gf_odf_write_rating(GF_BitStream *bs, GF_Rating *rd)
{
	GF_Err e;
	u32 size;
	if (!rd) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)rd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, rd->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, rd->ratingEntity, 32);
	gf_bs_write_int(bs, rd->ratingCriteria, 16);
	gf_bs_write_data(bs, rd->ratingInfo, rd->infoLength);
	return GF_OK;
}


GF_Descriptor *gf_odf_new_reg()
{
	GF_Registration *newDesc = (GF_Registration *) gf_malloc(sizeof(GF_Registration));
	if (!newDesc) return NULL;
	newDesc->additionalIdentificationInfo = NULL;
	newDesc->dataLength = 0;
	newDesc->formatIdentifier = 0;
	newDesc->tag = GF_ODF_REG_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_reg(GF_Registration *reg)
{
	if (!reg) return GF_BAD_PARAM;

	if (reg->additionalIdentificationInfo) gf_free(reg->additionalIdentificationInfo);
	gf_free(reg);
	return GF_OK;
}

GF_Err gf_odf_read_reg(GF_BitStream *bs, GF_Registration *reg, u32 DescSize)
{
	u32 nbBytes = 0;
	if (!reg) return GF_BAD_PARAM;

	reg->formatIdentifier = gf_bs_read_int(bs, 32);
	reg->dataLength = DescSize - 4;
	reg->additionalIdentificationInfo = (char*)gf_malloc(reg->dataLength);
	if (! reg->additionalIdentificationInfo) return GF_OUT_OF_MEM;
	gf_bs_read_data(bs, reg->additionalIdentificationInfo, reg->dataLength);
	nbBytes += reg->dataLength + 4;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


GF_Err gf_odf_size_reg(GF_Registration *reg, u32 *outSize)
{
	if (!reg) return GF_BAD_PARAM;

	*outSize = 4 + reg->dataLength;
	return GF_OK;
}

GF_Err gf_odf_write_reg(GF_BitStream *bs, GF_Registration *reg)
{
	GF_Err e;
	u32 size;
	if (!reg) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)reg, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, reg->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, reg->formatIdentifier, 32);
	gf_bs_write_data(bs, reg->additionalIdentificationInfo, reg->dataLength);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_short_text()
{
	GF_ShortTextual *newDesc = (GF_ShortTextual *) gf_malloc(sizeof(GF_ShortTextual));
	if (!newDesc) return NULL;

	newDesc->eventName = NULL;
	newDesc->eventText = NULL;
	newDesc->isUTF8 = 0;
	newDesc->langCode = 0;
	newDesc->tag = GF_ODF_SHORT_TEXT_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_short_text(GF_ShortTextual *std)
{
	if (!std) return GF_BAD_PARAM;

	if (std->eventName) gf_free(std->eventName);
	if (std->eventText) gf_free(std->eventText);
	gf_free(std);
	return GF_OK;
}

GF_Err gf_odf_read_short_text(GF_BitStream *bs, GF_ShortTextual *std, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0, len;
	if (!std) return GF_BAD_PARAM;

	std->langCode = gf_bs_read_int(bs, 24);
	std->isUTF8 = gf_bs_read_int(bs, 1);
	/*aligned = */gf_bs_read_int(bs, 7);
	nbBytes += 4;

	e = OD_ReadUTF8String(bs, & std->eventName, std->isUTF8, &len);
	if (e) return e;
	nbBytes += len;
	e = OD_ReadUTF8String(bs, & std->eventText, std->isUTF8, &len);
	if (e) return e;
	nbBytes += len;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_short_text(GF_ShortTextual *std, u32 *outSize)
{
	if (!std) return GF_BAD_PARAM;
	*outSize = 4;
	*outSize += OD_SizeUTF8String(std->eventName, std->isUTF8) + OD_SizeUTF8String(std->eventText, std->isUTF8);
	return GF_OK;
}

GF_Err gf_odf_write_short_text(GF_BitStream *bs, GF_ShortTextual *std)
{
	GF_Err e;
	u32 size;
	if (!std) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)std, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, std->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, std->langCode, 24);
	gf_bs_write_int(bs, std->isUTF8, 1);
	gf_bs_write_int(bs, 0, 7);
	OD_WriteUTF8String(bs, std->eventName, std->isUTF8);
	OD_WriteUTF8String(bs, std->eventText, std->isUTF8);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_smpte_camera()
{
	GF_SMPTECamera *newDesc = (GF_SMPTECamera *) gf_malloc(sizeof(GF_SMPTECamera));
	if (!newDesc) return NULL;

	newDesc->ParamList = gf_list_new();
	if (! newDesc->ParamList) {
		gf_free(newDesc);
		return NULL;
	}
	newDesc->cameraID = 0;
	newDesc->tag = GF_ODF_SMPTE_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_smpte_camera(GF_SMPTECamera *cpd)
{
	u32 i;
	GF_SmpteParam *tmp; 
	if (!cpd) return GF_BAD_PARAM;

	i=0;
	while ((tmp = (GF_SmpteParam *)gf_list_enum(cpd->ParamList, &i))) {
		gf_free(tmp);
	}
	gf_list_del(cpd->ParamList);
	gf_free(cpd);
	return GF_OK;
}
GF_Err gf_odf_read_smpte_camera(GF_BitStream *bs, GF_SMPTECamera *cpd, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0, i, count;
	if (!cpd) return GF_BAD_PARAM;

	cpd->cameraID = gf_bs_read_int(bs, 8);
	count = gf_bs_read_int(bs, 8);
	nbBytes += 2;

	for (i=0; i< count ; i++) {
		GF_SmpteParam *tmp = (GF_SmpteParam*)gf_malloc(sizeof(GF_SmpteParam));
		if (! tmp) return GF_OUT_OF_MEM;
		tmp->paramID = gf_bs_read_int(bs, 8);
		tmp->param = gf_bs_read_int(bs, 32);
		nbBytes += 5;
		e = gf_list_add(cpd->ParamList, tmp);
		if (e) return e;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}

GF_Err gf_odf_size_smpte_camera(GF_SMPTECamera *cpd, u32 *outSize)
{
	if (!cpd) return GF_BAD_PARAM;
	*outSize = 2 + 5 * gf_list_count(cpd->ParamList);
	return GF_OK;
}

GF_Err gf_odf_write_smpte_camera(GF_BitStream *bs, GF_SMPTECamera *cpd)
{
	GF_Err e;
	GF_SmpteParam *tmp;
	u32 size, i;
	if (!cpd) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)cpd, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, cpd->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, cpd->cameraID, 8);
	gf_bs_write_int(bs, gf_list_count(cpd->ParamList), 8);

	i=0;
	while ((tmp = (GF_SmpteParam *)gf_list_enum(cpd->ParamList, &i))) {
		gf_bs_write_int(bs, tmp->paramID, 8);
		gf_bs_write_int(bs, tmp->param, 32);
	}
	return GF_OK;
}

GF_Descriptor *gf_odf_new_sup_cid()
{
	GF_SCIDesc *newDesc = (GF_SCIDesc *) gf_malloc(sizeof(GF_SCIDesc));
	if (!newDesc) return NULL;
	newDesc->supplContentIdentifierTitle = NULL;
	newDesc->supplContentIdentifierValue  =NULL;
	newDesc->languageCode = 0;
	newDesc->tag = GF_ODF_SCI_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_sup_cid(GF_SCIDesc *scid)
{
	if (!scid) return GF_BAD_PARAM;

	if (scid->supplContentIdentifierTitle) gf_free(scid->supplContentIdentifierTitle);
	if (scid->supplContentIdentifierValue) gf_free(scid->supplContentIdentifierValue);
	gf_free(scid);
	return GF_OK;
}

GF_Err gf_odf_read_sup_cid(GF_BitStream *bs, GF_SCIDesc *scid, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0, len;
	if (! scid) return GF_BAD_PARAM;

	scid->languageCode = gf_bs_read_int(bs, 24);
	nbBytes += 3;
	e = OD_ReadUTF8String(bs, & scid->supplContentIdentifierTitle, 1, &len);
	if (e) return e;
	nbBytes += len;
	e = OD_ReadUTF8String(bs, & scid->supplContentIdentifierValue, 1, &len);
	if (e) return e;
	nbBytes += len;
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


GF_Err gf_odf_size_sup_cid(GF_SCIDesc *scid, u32 *outSize)
{
	if (! scid) return GF_BAD_PARAM;
	*outSize = 3 + OD_SizeUTF8String(scid->supplContentIdentifierTitle, 1) + OD_SizeUTF8String(scid->supplContentIdentifierValue, 1);
	return GF_OK;
}
GF_Err gf_odf_write_sup_cid(GF_BitStream *bs, GF_SCIDesc *scid)
{
	GF_Err e;
	u32 size;
	if (! scid) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)scid, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, scid->tag, size);
	if (e) return e;
	gf_bs_write_int(bs, scid->languageCode, 24);
	OD_WriteUTF8String(bs, scid->supplContentIdentifierTitle, 1);
	OD_WriteUTF8String(bs, scid->supplContentIdentifierValue, 1);
	return GF_OK;
}


/*IPMPX stuff*/
GF_Descriptor *gf_odf_new_ipmp_tool_list()
{
	GF_IPMP_ToolList*newDesc = (GF_IPMP_ToolList*) gf_malloc(sizeof(GF_IPMP_ToolList));
	if (!newDesc) return NULL;
	newDesc->ipmp_tools = gf_list_new();
	newDesc->tag = GF_ODF_IPMP_TL_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_ipmp_tool_list(GF_IPMP_ToolList *ipmptl)
{
	if (!ipmptl) return GF_BAD_PARAM;

	while (gf_list_count(ipmptl->ipmp_tools)) {
		GF_IPMP_Tool *t = (GF_IPMP_Tool *) gf_list_get(ipmptl->ipmp_tools, 0);
		gf_list_rem(ipmptl->ipmp_tools, 0);
		if (t->tool_url) gf_free(t->tool_url);
		gf_free(t);
	}
	gf_list_del(ipmptl->ipmp_tools);
	gf_free(ipmptl);
	return GF_OK;
}

GF_Err gf_odf_read_ipmp_tool_list(GF_BitStream *bs, GF_IPMP_ToolList *ipmptl, u32 DescSize)
{
	GF_Err e;
	u32 tmpSize;
	u32 nbBytes = 0;
	if (! ipmptl) return GF_BAD_PARAM;
	
	while (nbBytes < DescSize) {
		GF_Descriptor *tmp = NULL;
		e = gf_odf_parse_descriptor(bs, &tmp, &tmpSize);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = gf_list_add(ipmptl->ipmp_tools, tmp);
		if (e) return e;
		nbBytes += tmpSize + gf_odf_size_field_size(tmpSize);
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


GF_Err gf_odf_size_ipmp_tool_list(GF_IPMP_ToolList *ipmptl, u32 *outSize)
{
	if (!ipmptl) return GF_BAD_PARAM;
	*outSize = 0;
	return gf_odf_size_descriptor_list(ipmptl->ipmp_tools, outSize);
}

GF_Err gf_odf_write_ipmp_tool_list(GF_BitStream *bs, GF_IPMP_ToolList *ipmptl)
{
	GF_Err e;
	u32 size;
	if (!ipmptl) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)ipmptl, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmptl->tag, size);
	if (e) return e;
	e = gf_odf_write_descriptor_list(bs, ipmptl->ipmp_tools);
	return GF_OK;
}

GF_Descriptor *gf_odf_new_ipmp_tool()
{
	GF_IPMP_Tool *newDesc = (GF_IPMP_Tool*) gf_malloc(sizeof(GF_IPMP_Tool));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_IPMP_Tool));
	newDesc->tag = GF_ODF_IPMP_TL_TAG;
	return (GF_Descriptor *) newDesc;
}

GF_Err gf_odf_del_ipmp_tool(GF_IPMP_Tool *ipmpt)
{
	if (!ipmpt) return GF_BAD_PARAM;
	if (ipmpt->tool_url) gf_free(ipmpt->tool_url);
	gf_free(ipmpt);
	return GF_OK;
}

GF_Err gf_odf_read_ipmp_tool(GF_BitStream *bs, GF_IPMP_Tool *ipmpt, u32 DescSize)
{
	Bool is_alt, is_param;
	u32 nbBytes = 0;
	if (! ipmpt) return GF_BAD_PARAM;
	gf_bs_read_data(bs, (char*) ipmpt->IPMP_ToolID, 16);
	is_alt = gf_bs_read_int(bs, 1);
	is_param = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 6);
	nbBytes = 17;

	if (is_alt) {
		u32 i;
		ipmpt->num_alternate = gf_bs_read_int(bs, 8);
		nbBytes += 1;
		for (i=0; i<ipmpt->num_alternate; i++) {
			gf_bs_read_data(bs, (char*)ipmpt->specificToolID[i], 16);
			nbBytes += 16;
			if (nbBytes>DescSize) break;
		}
	}
	if (nbBytes>DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	
	if (is_param) { }

	if (nbBytes<DescSize) {
		u32 s;
		nbBytes += gf_ipmpx_array_size(bs, &s);
		if (s) {
			ipmpt->tool_url = (char*)gf_malloc(sizeof(char)*(s+1));
			gf_bs_read_data(bs, ipmpt->tool_url, s);
			ipmpt->tool_url[s] = 0;
			nbBytes += s;
		}
	}

	if (nbBytes!=DescSize) return GF_NON_COMPLIANT_BITSTREAM;
	return GF_OK;
}


GF_Err gf_odf_size_ipmp_tool(GF_IPMP_Tool *ipmpt, u32 *outSize)
{
	if (!ipmpt) return GF_BAD_PARAM;
	*outSize = 17;
	if (ipmpt->num_alternate) *outSize += 1 + 16*ipmpt->num_alternate;

	if (ipmpt->tool_url) {
		u32 s = strlen(ipmpt->tool_url);
		*outSize += gf_odf_size_field_size(s) - 1 + s;
	}
	return GF_OK;
}

GF_Err gf_odf_write_ipmp_tool(GF_BitStream *bs, GF_IPMP_Tool *ipmpt)
{
	GF_Err e;
	u32 size;
	if (!ipmpt) return GF_BAD_PARAM;
	e = gf_odf_size_descriptor((GF_Descriptor *)ipmpt, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, ipmpt->tag, size);
	if (e) return e;

	gf_bs_write_data(bs, (char*)ipmpt->IPMP_ToolID, 16);
	gf_bs_write_int(bs, ipmpt->num_alternate ? 1 : 0, 1);
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, 0, 6);

	if (ipmpt->num_alternate) {
		u32 i;
		gf_bs_write_int(bs, ipmpt->num_alternate, 8);
		for (i=0;i<ipmpt->num_alternate; i++) gf_bs_write_data(bs, (char*)ipmpt->specificToolID[i], 16);
	}
	if (ipmpt->tool_url) gf_ipmpx_write_array(bs, ipmpt->tool_url, strlen(ipmpt->tool_url));
	return GF_OK;
}

#endif /*GPAC_MINIMAL_ODF*/
