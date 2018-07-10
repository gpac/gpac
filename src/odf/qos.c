/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef GPAC_MINIMAL_ODF

/************************************************************
		QoSQualifiers Functions
************************************************************/

GF_EXPORT
GF_QoS_Default *gf_odf_qos_new(u8 tag)
{

	GF_QoS_Default *NewQoS(u8 tag);

	GF_QoS_Default *qos;

	qos = NewQoS(tag);
	return qos;
}

GF_EXPORT
GF_Err gf_odf_qos_del(GF_QoS_Default **qos)
{
	if (*qos) gf_odf_delete_qos_qual(*qos);
	*qos = NULL;
	return GF_OK;
}


//same function, but for QoS, as a Qualifier IS NOT a descriptor
GF_EXPORT
GF_Err gf_odf_qos_add_qualif(GF_QoS_Descriptor *desc, GF_QoS_Default *qualif)
{
	u32 i;
	GF_QoS_Default *def;

	if (desc->tag != GF_ODF_QOS_TAG) return GF_BAD_PARAM;
	if (desc->predefined) return GF_ODF_FORBIDDEN_DESCRIPTOR;

	i=0;
	while ((def = (GF_QoS_Default *)gf_list_enum(desc->QoS_Qualifiers, &i))) {
		//if same Qualifier, not allowed...
		if (def->tag == qualif->tag) return GF_ODF_FORBIDDEN_DESCRIPTOR;
	}
	return gf_list_add(desc->QoS_Qualifiers, qualif);
}

void gf_odf_delete_qos_qual(GF_QoS_Default *qos)
{
	switch (qos->tag) {
	case QoSMaxDelayTag :
	case QoSPrefMaxDelayTag:
	case QoSLossProbTag:
	case QoSMaxGapLossTag:
	case QoSMaxAUSizeTag:
	case QoSAvgAUSizeTag:
	case QoSMaxAURateTag:
		gf_free(qos);
		return;

	default:
		if ( ((GF_QoS_Private *)qos)->DataLength)
			gf_free(((GF_QoS_Private *)qos)->Data);
		gf_free( (GF_QoS_Private *) qos);
		return;
	}
}


GF_Err gf_odf_size_qos_qual(GF_QoS_Default *qos)
{
	if (! qos) return GF_BAD_PARAM;
	qos->size = 0;

	switch (qos->tag) {
	case QoSMaxDelayTag:
	case QoSPrefMaxDelayTag:
	case QoSLossProbTag:
	case QoSMaxGapLossTag:
	case QoSMaxAUSizeTag:
	case QoSAvgAUSizeTag:
	case QoSMaxAURateTag:
		qos->size += 4;
		return GF_OK;

	case 0x00:
	case 0xFF:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	default :
		qos->size += ((GF_QoS_Private *)qos)->DataLength;
	}
	return GF_OK;
}

GF_Err gf_odf_write_qos_qual(GF_BitStream *bs, GF_QoS_Default *qos)
{
	GF_Err e;
	if (!bs || !qos) return GF_BAD_PARAM;

	e = gf_odf_size_qos_qual(qos);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, qos->tag, qos->size);
	if (e) return e;

	switch (qos->tag) {
	case QoSMaxDelayTag:
		gf_bs_write_int(bs, ((GF_QoS_MaxDelay *)qos)->MaxDelay, 32);
		break;

	case QoSPrefMaxDelayTag:
		gf_bs_write_int(bs, ((GF_QoS_PrefMaxDelay *)qos)->PrefMaxDelay, 32);
		break;

	case QoSLossProbTag:
		//FLOAT (double on 4 bytes)
		gf_bs_write_float(bs, ((GF_QoS_LossProb *)qos)->LossProb);
		break;

	case QoSMaxGapLossTag:
		gf_bs_write_int(bs, ((GF_QoS_MaxGapLoss *)qos)->MaxGapLoss, 32);
		break;

	case QoSMaxAUSizeTag:
		gf_bs_write_int(bs, ((GF_QoS_MaxAUSize *)qos)->MaxAUSize, 32);
		break;

	case QoSAvgAUSizeTag:
		gf_bs_write_int(bs, ((GF_QoS_AvgAUSize *)qos)->AvgAUSize, 32);
		break;

	case QoSMaxAURateTag:
		gf_bs_write_int(bs, ((GF_QoS_MaxAURate *)qos)->MaxAURate, 32);
		break;

	case 0x00:
	case 0xFF:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	default:
		//we defined the private qos...
		gf_bs_write_data(bs, ((GF_QoS_Private *)qos)->Data, ((GF_QoS_Private *)qos)->DataLength);
		break;
	}
	return GF_OK;
}



GF_Err gf_odf_parse_qos(GF_BitStream *bs, GF_QoS_Default **qos_qual, u32 *qual_size)
{
	u32 tag, qos_size, val, bytesParsed, sizeHeader;
	GF_QoS_Default *newQoS;

	//tag
	tag = gf_bs_read_int(bs, 8);
	bytesParsed = 1;
	//size of instance
	qos_size = 0;
	sizeHeader = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		sizeHeader++;
		if (sizeHeader > 5) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[ODF] Descriptor size on more than 4 bytes\n"));
			return GF_ODF_INVALID_DESCRIPTOR;
		}
		qos_size <<= 7;
		qos_size |= val & 0x7F;
	} while ( val & 0x80 );
	if (gf_bs_available(bs) < qos_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[ODF] Not enough bytes (%d) to read descriptor (size=%d)\n", gf_bs_available(bs), qos_size));
		return GF_ODF_INVALID_DESCRIPTOR;
	}
	bytesParsed += sizeHeader;

	//Payload
	switch (tag) {
	case QoSMaxDelayTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxDelay));
		((GF_QoS_MaxDelay *)newQoS)->MaxDelay = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case QoSPrefMaxDelayTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_PrefMaxDelay));
		((GF_QoS_PrefMaxDelay *)newQoS)->PrefMaxDelay = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case QoSLossProbTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_LossProb));
		((GF_QoS_LossProb *)newQoS)->LossProb = gf_bs_read_float(bs);
		bytesParsed += 4;
		break;

	case QoSMaxGapLossTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxGapLoss));
		((GF_QoS_MaxGapLoss *)newQoS)->MaxGapLoss = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case QoSMaxAUSizeTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxAUSize));
		((GF_QoS_MaxAUSize *)newQoS)->MaxAUSize = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case QoSAvgAUSizeTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_AvgAUSize));
		((GF_QoS_AvgAUSize *)newQoS)->AvgAUSize = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case QoSMaxAURateTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxAURate));
		((GF_QoS_MaxAURate *)newQoS)->MaxAURate = gf_bs_read_int(bs, 32);
		bytesParsed += 4;
		break;

	case 0x00:
	case 0xFF:
		return GF_ODF_FORBIDDEN_DESCRIPTOR;

	default:
		//we defined the private qos...
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_Private));
		((GF_QoS_Private *)newQoS)->DataLength = qos_size;
		((GF_QoS_Private *)newQoS)->Data = (char *) gf_malloc( qos_size );
		gf_bs_read_data(bs, ((GF_QoS_Private *)newQoS)->Data, ((GF_QoS_Private *)newQoS)->DataLength);
		bytesParsed += ((GF_QoS_Private *)newQoS)->DataLength;
		break;
	}
	newQoS->size = qos_size;
	newQoS->tag = tag;
	if (bytesParsed != 1 + qos_size + sizeHeader) {
		gf_odf_delete_qos_qual(newQoS);
		return GF_ODF_INVALID_DESCRIPTOR;
	}
	*qos_qual = newQoS;
	*qual_size = bytesParsed;
	return GF_OK;
}


GF_QoS_Default *NewQoS(u8 tag)
{
	GF_QoS_Default *newQoS;

	switch (tag) {
	case QoSMaxDelayTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxDelay));
		((GF_QoS_MaxDelay *)newQoS)->MaxDelay = 0;
		((GF_QoS_MaxDelay *)newQoS)->size = 4;
		break;

	case QoSPrefMaxDelayTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_PrefMaxDelay));
		((GF_QoS_PrefMaxDelay *)newQoS)->PrefMaxDelay = 0;
		((GF_QoS_PrefMaxDelay *)newQoS)->size = 4;
		break;

	case QoSLossProbTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_LossProb));
		((GF_QoS_LossProb *)newQoS)->LossProb = 0;
		((GF_QoS_LossProb *)newQoS)->size = 4;
		break;

	case QoSMaxGapLossTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxGapLoss));
		((GF_QoS_MaxGapLoss *)newQoS)->MaxGapLoss = 0;
		((GF_QoS_MaxGapLoss *)newQoS)->size = 4;
		break;

	case QoSMaxAUSizeTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxAUSize));
		((GF_QoS_MaxAUSize *)newQoS)->MaxAUSize = 0;
		((GF_QoS_MaxAUSize *)newQoS)->size = 0;
		break;

	case QoSAvgAUSizeTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_AvgAUSize));
		((GF_QoS_AvgAUSize *)newQoS)->AvgAUSize = 0;
		((GF_QoS_AvgAUSize *)newQoS)->size = 4;
		break;

	case QoSMaxAURateTag:
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_MaxAURate));
		((GF_QoS_MaxAURate *)newQoS)->MaxAURate = 0;
		((GF_QoS_MaxAURate *)newQoS)->size = 4;
		break;

	case 0x00:
	case 0xFF:
		return NULL;

	default:
		//we defined the private qos...
		newQoS = (GF_QoS_Default *) gf_malloc(sizeof(GF_QoS_Private));
		((GF_QoS_Private *)newQoS)->DataLength = 0;
		((GF_QoS_Private *)newQoS)->Data = NULL;
		break;
	}
	newQoS->tag = tag;
	return newQoS;
}

//
//	Constructor
//
GF_Descriptor *gf_odf_new_qos()
{
	GF_QoS_Descriptor *newDesc = (GF_QoS_Descriptor *) gf_malloc(sizeof(GF_QoS_Descriptor));
	if (!newDesc) return NULL;
	newDesc->QoS_Qualifiers = gf_list_new();
	newDesc->predefined = 0;
	newDesc->tag = GF_ODF_QOS_TAG;
	return (GF_Descriptor *) newDesc;
}

//
//	Desctructor
//
GF_Err gf_odf_del_qos(GF_QoS_Descriptor *qos)
{
	if (!qos) return GF_BAD_PARAM;

	while (gf_list_count(qos->QoS_Qualifiers)) {
		GF_QoS_Default *tmp = (GF_QoS_Default*)gf_list_get(qos->QoS_Qualifiers, 0);
		gf_odf_delete_qos_qual(tmp);
		gf_list_rem(qos->QoS_Qualifiers, 0);
	}
	gf_list_del(qos->QoS_Qualifiers);
	gf_free(qos);
	return GF_OK;
}


//
//		Reader
//
GF_Err gf_odf_read_qos(GF_BitStream *bs, GF_QoS_Descriptor *qos, u32 DescSize)
{
	GF_Err e;
	GF_QoS_Default *tmp;
	u32 tmp_size, nbBytes = 0;
	if (!qos) return GF_BAD_PARAM;

	qos->predefined = gf_bs_read_int(bs, 8);
	nbBytes += 1;

	if (qos->predefined) {
		if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
		return GF_OK;
	}

	while (nbBytes < DescSize) {
		tmp = NULL;
		e = gf_odf_parse_qos(bs, &tmp, &tmp_size);
		if (e) return e;
		if (!tmp) return GF_ODF_INVALID_DESCRIPTOR;
		e = gf_list_add(qos->QoS_Qualifiers, tmp);
		if (e) return e;
		nbBytes += tmp_size;
	}
	if (nbBytes != DescSize) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}




//
//		Size
//
GF_Err gf_odf_size_qos(GF_QoS_Descriptor *qos, u32 *outSize)
{
	GF_Err e;
	u32 i;
	GF_QoS_Default *tmp;

	if (!qos) return GF_BAD_PARAM;

	*outSize = 1;

	i=0;
	while ((tmp = (GF_QoS_Default *)gf_list_enum(qos->QoS_Qualifiers, &i))) {
		e = gf_odf_size_qos_qual(tmp);
		if (e) return e;
		*outSize += tmp->size + gf_odf_size_field_size(tmp->size);
	}
	return GF_OK;
}

//
//		Writer
//
GF_Err gf_odf_write_qos(GF_BitStream *bs, GF_QoS_Descriptor *qos)
{
	GF_Err e;
	u32 size, i;
	GF_QoS_Default *tmp;
	if (!qos) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)qos, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, qos->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, qos->predefined, 8);

	if (! qos->predefined) {
		i=0;
		while ((tmp = (GF_QoS_Default *)gf_list_enum(qos->QoS_Qualifiers, &i))) {
			e = gf_odf_write_qos_qual(bs, tmp);
			if (e) return e;
		}
	}
	return GF_OK;
}


#endif /*GPAC_MINIMAL_ODF*/

