/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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

#include "math.h"
#include "quant.h"

#ifndef GPAC_DISABLE_BIFS_ENC

GF_Err gf_bifs_enc_qp_set(GF_BifsEncoder *codec, GF_Node *qp)
{
	if (gf_node_get_tag(qp) != TAG_MPEG4_QuantizationParameter) return GF_BAD_PARAM;

	/*if we have an active QP, push it into the stack*/
	if (codec->ActiveQP && ((GF_Node*)codec->ActiveQP != codec->scene_graph->global_qp) ) 
		gf_list_insert(codec->QPs, codec->ActiveQP, 0);
	
	codec->ActiveQP = (M_QuantizationParameter *)qp;
	return GF_OK;
}

GF_Err gf_bifs_enc_qp_remove(GF_BifsEncoder *codec, Bool ActivatePrev)
{
	codec->ActiveQP = NULL;
	if (!ActivatePrev) return GF_OK;

	if (gf_list_count(codec->QPs)) {
		codec->ActiveQP = (M_QuantizationParameter*)gf_list_get(codec->QPs, 0);
		gf_list_rem(codec->QPs, 0);
	} else if (codec->scene_graph->global_qp) {
		codec->ActiveQP = (M_QuantizationParameter *)codec->scene_graph->global_qp;
	}
	return GF_OK;
}


u32 gf_bifs_enc_qp14_get_bits(GF_BifsEncoder *codec)
{
	if (!codec->ActiveQP || !codec->coord_stored) return 0;
	return (u32) ceil(log(codec->NumCoord+1) / log(2) ); 
}

void gf_bifs_enc_qp14_enter(GF_BifsEncoder *codec, Bool Enter)
{
	if (!codec->ActiveQP) return;
	if (Enter) codec->storing_coord = 1;
	else {
		if (codec->storing_coord) codec->coord_stored = 1;
		codec->storing_coord = 0;
	}
}

void gf_bifs_enc_qp14_reset(GF_BifsEncoder *codec)
{
	codec->coord_stored = 0;
	codec->storing_coord = 0;
	codec->NumCoord = 0;
}

void gf_bifs_enc_qp14_set_length(GF_BifsEncoder *codec, u32 NbElements)
{
	if (!codec->ActiveQP || !codec->storing_coord || codec->coord_stored) return;
	codec->NumCoord = NbElements;
}

void gf_bifs_enc_mantissa_float(GF_BifsEncoder *codec, Fixed ft, GF_BitStream *bs)
{
	u32 mantLength, expLength, mantSign, mantissa, expSign, i, nbBits;
	s32 exp;

	union
	{	
		Float f;
		s32 l;
	} ft_val;

	if (ft == 0) {
	    gf_bs_write_int(bs, 0, 4);
		return;
	}
	ft_val.f = FIX2FLT(ft);
  
	mantSign = ((ft_val.l & 0x80000000) >> 31) & 0x1;
	mantissa = (ft_val.l & 0x007FFFFF) >> 9;
	mantLength = 15;
	expSign=0;
	exp =(((ft_val.l & 0x7F800000) >> 23)-127);
	expLength = 8;
  
	if (mantissa == 0) mantLength = 1;
  

	if (exp) {
		if (exp< 0) {
			expSign = 1;
			exp = -exp;	    
		}
		while ((exp & (1<<(--expLength)))==0) { }
		exp &= ~(1<<expLength);
		expLength++;
	} else {
		expLength=0;
	}
  
	nbBits=0;
	for(i = mantissa; i>0; ++nbBits) i >>= 1;

	gf_bs_write_int(bs, nbBits+1, 4);
	if (mantLength) {
		gf_bs_write_int(bs, expLength, 3);
		gf_bs_write_int(bs, mantSign, 1);
		gf_bs_write_int(bs, mantissa, nbBits);
		if(expLength) {
			gf_bs_write_int(bs, expSign, 1);
			gf_bs_write_int(bs, exp, expLength - 1);
		}
	}
}


//Linear Quantization for floats - go back to float to avoid overflow if nbBits more than 15...
s32 Q_Quantize(Fixed Min, Fixed Max, u32 NbBits, Fixed value)
{
	Float _v;
	if (value <= Min) return 0;
	if (value >= Max) return (1<<NbBits)-1;
	_v = FIX2FLT(value - Min);
	_v *= (1 << NbBits) - 1;
	_v /= FIX2FLT(Max - Min);
	return FIX2INT(gf_floor( FLT2FIX(_v+0.5) ) );
}


GF_Err Q_EncFloat(GF_BifsEncoder *codec, GF_BitStream *bs, u32 FieldType, SFVec3f BMin, SFVec3f BMax, u32 NbBits, void *field_ptr)
{
	s32 newVal;
	switch (FieldType) {
	case GF_SG_VRML_SFINT32:
		return GF_NON_COMPLIANT_BITSTREAM;
	case GF_SG_VRML_SFFLOAT:
		newVal = Q_Quantize(BMin.x, BMax.x, NbBits, *((SFFloat *)field_ptr));
		gf_bs_write_int(bs, newVal, NbBits);
		return GF_OK;
	case GF_SG_VRML_SFVEC2F:
		newVal = Q_Quantize(BMin.x, BMax.x, NbBits, ((SFVec2f *)field_ptr)->x);
		gf_bs_write_int(bs, newVal, NbBits);
		newVal = Q_Quantize(BMin.y, BMax.y, NbBits, ((SFVec2f *)field_ptr)->y);
		gf_bs_write_int(bs, newVal, NbBits);
		return GF_OK;
	case GF_SG_VRML_SFVEC3F:
		newVal = Q_Quantize(BMin.x, BMax.x, NbBits, ((SFVec3f *)field_ptr)->x);
		gf_bs_write_int(bs, newVal, NbBits);
		newVal = Q_Quantize(BMin.y, BMax.y, NbBits, ((SFVec3f *)field_ptr)->y);
		gf_bs_write_int(bs, newVal, NbBits);
		newVal = Q_Quantize(BMin.z, BMax.z, NbBits, ((SFVec3f *)field_ptr)->z);
		gf_bs_write_int(bs, newVal, NbBits);
		return GF_OK;
	case GF_SG_VRML_SFCOLOR:
		newVal = Q_Quantize(BMin.x, BMax.x, NbBits, ((SFColor *)field_ptr)->red);
		gf_bs_write_int(bs, newVal, NbBits);
		newVal = Q_Quantize(BMin.y, BMax.y, NbBits, ((SFColor *)field_ptr)->green);
		gf_bs_write_int(bs, newVal, NbBits);
		newVal = Q_Quantize(BMin.z, BMax.z, NbBits, ((SFColor *)field_ptr)->blue);
		gf_bs_write_int(bs, newVal, NbBits);
		return GF_OK;

	case GF_SG_VRML_SFROTATION:
		//forbidden in this Q mode
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

//int in quant are either Linear Scalar fields or CoordIndex
//the quant is just a bitshifting into [0, 2^NbBits-1]
//so v = value - b_min
GF_Err Q_EncInt(GF_BifsEncoder *codec, GF_BitStream *bs, u32 QType, SFInt32 b_min, u32 NbBits, void *field_ptr)
{
	switch (QType) {
	case QC_LINEAR_SCALAR:
	case QC_COORD_INDEX:
		gf_bs_write_int(bs, *((SFInt32 *)field_ptr) - b_min, NbBits);
		return GF_OK;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
}

GF_Err Q_EncCoordOnUnitSphere(GF_BifsEncoder *codec, GF_BitStream *bs, u32 NbBits, u32 NbComp, Fixed *m_ft) 
{
	u32 i;
	u32 len = NbComp+1;
	s32 orientation =-1;
	Fixed maxTmp = - FIX_MAX;
	for (i=0; i<len; i++) {
		if (ABS(m_ft[i]) > maxTmp) {
			maxTmp = ABS(m_ft[i]);
			orientation = i;
		}
	}
	if(NbComp==2) gf_bs_write_int(bs, ((m_ft[orientation]>0) ? 0 : 1), 1); 
	gf_bs_write_int(bs, orientation, 2);
	for (i=0; i<NbComp; i++) {
		Fixed v = gf_mulfix(gf_divfix(INT2FIX(4), GF_PI) , gf_atan2(m_ft[orientation], m_ft[(orientation+i+1) % len]));
		s32 qdt = Q_Quantize(0, 1, NbBits-1, (v>=0 ? v : -v));
		s32 qv = (1<<(NbBits-1)) + (v>=0 ? 1 : -1) * qdt;
		gf_bs_write_int(bs, qv, NbBits);
	}
	return GF_OK;
}

GF_Err Q_EncNormal(GF_BifsEncoder *codec, GF_BitStream *bs, u32 NbBits, void *field_ptr)
{
	Fixed comp[3];
	SFVec3f v =  * (SFVec3f *)field_ptr;
	gf_vec_norm(&v);
	comp[0] = v.x;
	comp[1] = v.y;
	comp[2] = v.z;
	return Q_EncCoordOnUnitSphere(codec, bs, NbBits, 2, comp);
}

GF_Err Q_EncRotation(GF_BifsEncoder *codec, GF_BitStream *bs, u32 NbBits, void *field_ptr)
{
	GF_Vec4 quat;
	Fixed comp[4];
	
	/*get quaternion*/
	quat = gf_quat_from_rotation(*(SFRotation *)field_ptr);
	comp[0] = quat.q;
	comp[1] = quat.x;
	comp[2] = quat.y;
	comp[3] = quat.z;
	return Q_EncCoordOnUnitSphere(codec, bs, NbBits, 3, comp);
}

GF_Err gf_bifs_enc_quant_field(GF_BifsEncoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
{
	Bool HasQ;
	u8 QType, AType;
	u32 NbBits;
	Fixed b_min, b_max;
	SFVec3f BMin, BMax;
	GF_Err e;

	/*check QP*/
	if (!codec->ActiveQP) return GF_EOS;
	/*check FieldType*/
	switch (field->fieldType) {
	case GF_SG_VRML_SFINT32:
	case GF_SG_VRML_SFFLOAT:
	case GF_SG_VRML_SFROTATION:
	case GF_SG_VRML_SFVEC2F:
	case GF_SG_VRML_SFVEC3F:
		break;
	case GF_SG_VRML_SFCOLOR:
		break;
	default:
		return GF_EOS;
	}
	
	/*check NDT*/
	HasQ = gf_bifs_get_aq_info(node, field->fieldIndex, &QType, &AType, &b_min, &b_max, &NbBits);
	if (!HasQ || !QType) return GF_EOS;

	/*get NbBits for QP14 (QC_COORD_INDEX)*/
	if (QType == QC_COORD_INDEX) {
		NbBits = gf_bifs_enc_qp14_get_bits(codec);
		/*QP14 is always on, not having NbBits set means the coord field is set after the index field, hence not decodable*/
		if (!NbBits) 
			return GF_NON_COMPLIANT_BITSTREAM;
	}

	BMin.x = BMin.y = BMin.z = b_min;
	BMax.x = BMax.y = BMax.z = b_max;

	/*check is the QP is on and retrieves the bounds*/
	if (!Q_IsTypeOn(codec->ActiveQP, QType, &NbBits, &BMin, &BMax)) return GF_EOS;

	/*ok the field is Quantized, dequantize*/
	switch (QType) {
	//these are all SFFloat quantized on n fields
	case QC_3DPOS:
	case QC_2DPOS:
	case QC_ORDER:
	case QC_COLOR:
	case QC_TEXTURE_COORD:
	case QC_ANGLE:
	case QC_SCALE:
	case QC_INTERPOL_KEYS:
	case QC_SIZE_3D:
	case QC_SIZE_2D:
		e = Q_EncFloat(codec, bs, field->fieldType, BMin, BMax, NbBits, field->far_ptr);
		break;
	//SFInt types
	case QC_LINEAR_SCALAR:
	case QC_COORD_INDEX:
		e = Q_EncInt(codec, bs, QType, (SFInt32) b_min, NbBits, field->far_ptr);
		break;
	//normalized fields (normals and vectors)
	case QC_NORMALS:
		//normal quant is only for SFVec3F
		if (field->fieldType != GF_SG_VRML_SFVEC3F) return GF_NON_COMPLIANT_BITSTREAM;
		e = Q_EncNormal(codec, bs, NbBits, field->far_ptr);
		break;
	case QC_ROTATION:
		//normal quant is only for SFVec3F
		if (field->fieldType != GF_SG_VRML_SFROTATION) return GF_NON_COMPLIANT_BITSTREAM;
		e = Q_EncRotation(codec, bs, NbBits, field->far_ptr);
		break;
	default:
		return GF_BAD_PARAM;
	}
	return e;
}

#endif	/*GPAC_DISABLE_BIFS_ENC*/
