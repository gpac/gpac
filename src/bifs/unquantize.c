/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#include "quant.h"

#ifndef GPAC_DISABLE_BIFS

#include <math.h>

u32 gf_bifs_dec_qp14_get_bits(GF_BifsDecoder *codec)
{
	if (!codec->ActiveQP || !codec->coord_stored) return 0;
	return (u32) ceil(log1p(codec->NumCoord) / log(2) );
}

void gf_bifs_dec_qp14_enter(GF_BifsDecoder * codec, Bool Enter)
{
	if (!codec->ActiveQP) return;
	if (Enter) codec->storing_coord = GF_TRUE;
	else {
		if (codec->storing_coord) codec->coord_stored = GF_TRUE;
		codec->storing_coord = GF_FALSE;
	}
}

void gf_bifs_dec_qp14_reset(GF_BifsDecoder * codec)
{
	codec->coord_stored = GF_FALSE;
	codec->storing_coord = GF_FALSE;
	codec->NumCoord = 0;
}

void gf_bifs_dec_qp14_set_length(GF_BifsDecoder * codec, u32 NbElements)
{
	if (!codec->ActiveQP || !codec->storing_coord || codec->coord_stored) return;
	codec->NumCoord = NbElements;
}

GF_Err gf_bifs_dec_qp_set(GF_BifsDecoder *codec, GF_Node *qp)
{
	assert(gf_node_get_tag(qp) == TAG_MPEG4_QuantizationParameter);

	/*if we have an active QP, push it into the stack*/
	if (codec->ActiveQP && ((GF_Node*)codec->ActiveQP != codec->scenegraph->global_qp) )
		gf_list_insert(codec->QPs, codec->ActiveQP, 0);

	codec->ActiveQP = (M_QuantizationParameter *)qp;
	return GF_OK;
}

GF_Err gf_bifs_dec_qp_remove(GF_BifsDecoder *codec, Bool ActivatePrev)
{
	if (!codec->force_keep_qp && codec->ActiveQP && ((GF_Node*)codec->ActiveQP != codec->scenegraph->global_qp) ) {
		gf_node_unregister((GF_Node *) codec->ActiveQP, NULL);
	}
	codec->ActiveQP = NULL;
	if (!ActivatePrev) return GF_OK;

	if (gf_list_count(codec->QPs)) {
		codec->ActiveQP = (M_QuantizationParameter*)gf_list_get(codec->QPs, 0);
		gf_list_rem(codec->QPs, 0);
	} else if (codec->scenegraph->global_qp) {
		codec->ActiveQP = (M_QuantizationParameter *)codec->scenegraph->global_qp;
	}
	return GF_OK;
}

//parses efficient float
Fixed gf_bifs_dec_mantissa_float(GF_BifsDecoder *codec, GF_BitStream *bs)
{
	u32 mantLength, expLength, mantSign, mantissa;
	unsigned char exp;

	union {
		Float f;
		long l;
	} ft_value;

	mantLength = gf_bs_read_int(bs, 4);
	if (!mantLength) return 0;

	expLength = gf_bs_read_int(bs, 3);
	mantSign = gf_bs_read_int(bs, 1);
	mantissa = gf_bs_read_int(bs, mantLength - 1);

	exp = 127;
	if (expLength) {
		u32 expSign = gf_bs_read_int(bs, 1);
		u32 exponent = gf_bs_read_int(bs, expLength-1);
		exp += (1-2*expSign)*( (1 << (expLength-1) ) + exponent);
	}

	ft_value.l = mantSign << 31;
	ft_value.l |= (exp & 0xff) << 23;
	ft_value.l |= mantissa << 9;
	return FLT2FIX(ft_value.f);
}

//check if the quant type is on in the QP, and if so retrieves NbBits and Min Max
//specified for the field
Bool Q_IsTypeOn(M_QuantizationParameter *qp, u32 q_type, u32 *NbBits, SFVec3f *b_min, SFVec3f *b_max)
{
	switch (q_type) {
	case QC_3DPOS:
		if (!qp->position3DQuant) return GF_FALSE;
		*NbBits = qp->position3DNbBits;
		b_min->x = MAX(b_min->x, qp->position3DMin.x);
		b_min->y = MAX(b_min->y, qp->position3DMin.y);
		b_min->z = MAX(b_min->z, qp->position3DMin.z);
		b_max->x = MIN(b_max->x, qp->position3DMax.x);
		b_max->y = MIN(b_max->y, qp->position3DMax.y);
		b_max->z = MIN(b_max->z, qp->position3DMax.z);
		return GF_TRUE;
	case QC_2DPOS:
		if (!qp->position2DQuant) return GF_FALSE;
		*NbBits = qp->position2DNbBits;
		b_min->x = MAX(b_min->x, qp->position2DMin.x);
		b_min->y = MAX(b_min->y, qp->position2DMin.y);
		b_max->x = MIN(b_max->x, qp->position2DMax.x);
		b_max->y = MIN(b_max->y, qp->position2DMax.y);
		return GF_TRUE;
	case QC_ORDER:
		if (!qp->drawOrderQuant) return GF_FALSE;
		*NbBits = qp->drawOrderNbBits;
		b_min->x = MAX(b_min->x, qp->drawOrderMin);
		b_max->x = MIN(b_max->x, qp->drawOrderMax);
		return GF_TRUE;
	case QC_COLOR:
		if (!qp->colorQuant) return GF_FALSE;
		*NbBits = qp->colorNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->colorMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->colorMax);
		return GF_TRUE;
	case QC_TEXTURE_COORD:
		if (!qp->textureCoordinateQuant) return GF_FALSE;
		*NbBits = qp->textureCoordinateNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->textureCoordinateMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->textureCoordinateMax);
		return GF_TRUE;
	case QC_ANGLE:
		if (!qp->angleQuant) return GF_FALSE;
		*NbBits = qp->angleNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->angleMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->angleMax);
		return GF_TRUE;
	case QC_SCALE:
		if (!qp->scaleQuant) return GF_FALSE;
		*NbBits = qp->scaleNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->scaleMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->scaleMax);
		return GF_TRUE;
	case QC_INTERPOL_KEYS:
		if (!qp->keyQuant) return GF_FALSE;
		*NbBits = qp->keyNbBits;
		b_min->x = MAX(b_min->x, qp->keyMin);
		b_min->y = MAX(b_min->y, qp->keyMin);
		b_min->z = MAX(b_min->z, qp->keyMin);
		b_max->x = MIN(b_max->x, qp->keyMax);
		b_max->y = MIN(b_max->y, qp->keyMax);
		b_max->z = MIN(b_max->z, qp->keyMax);
		return GF_TRUE;
	case QC_NORMALS:
		if (!qp->normalQuant) return GF_FALSE;
		*NbBits = qp->normalNbBits;
		b_min->x = b_min->y = b_min->z = 0;
		b_max->x = b_max->y = b_max->z = FIX_ONE;
		return GF_TRUE;
	case QC_ROTATION:
		if (!qp->normalQuant) return GF_FALSE;
		*NbBits = qp->normalNbBits;
		b_min->x = b_min->y = b_min->z = 0;
		b_max->x = b_max->y = b_max->z = FIX_ONE;
		return GF_TRUE;
	case QC_SIZE_3D:
		if (!qp->sizeQuant) return GF_FALSE;
		*NbBits = qp->sizeNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->sizeMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->sizeMax);
		return GF_TRUE;
	case QC_SIZE_2D:
		if (!qp->sizeQuant) return GF_FALSE;
		*NbBits = qp->sizeNbBits;
		b_min->x = b_min->y = b_min->z = MAX(b_min->x, qp->sizeMin);
		b_max->x = b_max->y = b_max->z = MIN(b_max->x, qp->sizeMax);
		return GF_TRUE;

	//cf specs, from here ALWAYS ON
	case QC_LINEAR_SCALAR:
		//nbBits is the one from the FCT - DO NOT CHANGE IT
		return GF_TRUE;
	case QC_COORD_INDEX:
		//nbBits has to be recomputed on the fly
		return GF_TRUE;
	case QC_RESERVED:
		*NbBits = 0;
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}


//Linear inverse Quantization for floats
Fixed Q_InverseQuantize(Fixed Min, Fixed Max, u32 NbBits, u32 value)
{
	if (!value) return Min;
	if (value == (u32) ((1 << NbBits) - 1) ) return Max;
	return Min + gf_muldiv(Max - Min, INT2FIX(value), INT2FIX( (1 << NbBits) - 1) );
}


GF_Err Q_DecFloat(GF_BifsDecoder *codec, GF_BitStream *bs, u32 FieldType, SFVec3f BMin, SFVec3f BMax, u32 NbBits, void *field_ptr)
{
	switch (FieldType) {
	case GF_SG_VRML_SFINT32:
		return GF_NON_COMPLIANT_BITSTREAM;
	case GF_SG_VRML_SFFLOAT:
		*((SFFloat *)field_ptr) = Q_InverseQuantize(BMin.x, BMax.x, NbBits, gf_bs_read_int(bs, NbBits));
		return GF_OK;
	case GF_SG_VRML_SFVEC2F:
		((SFVec2f *)field_ptr)->x = Q_InverseQuantize(BMin.x, BMax.x, NbBits, gf_bs_read_int(bs, NbBits));
		((SFVec2f *)field_ptr)->y = Q_InverseQuantize(BMin.y, BMax.y, NbBits, gf_bs_read_int(bs, NbBits));
		return GF_OK;
	case GF_SG_VRML_SFVEC3F:
		((SFVec3f *)field_ptr)->x = Q_InverseQuantize(BMin.x, BMax.x, NbBits, gf_bs_read_int(bs, NbBits));
		((SFVec3f *)field_ptr)->y = Q_InverseQuantize(BMin.y, BMax.y, NbBits, gf_bs_read_int(bs, NbBits));
		((SFVec3f *)field_ptr)->z = Q_InverseQuantize(BMin.z, BMax.z, NbBits, gf_bs_read_int(bs, NbBits));
		return GF_OK;
	case GF_SG_VRML_SFCOLOR:
		((SFColor *)field_ptr)->red = Q_InverseQuantize(BMin.x, BMax.x, NbBits, gf_bs_read_int(bs, NbBits));
		((SFColor *)field_ptr)->green = Q_InverseQuantize(BMin.y, BMax.y, NbBits, gf_bs_read_int(bs, NbBits));
		((SFColor *)field_ptr)->blue = Q_InverseQuantize(BMin.z, BMax.z, NbBits, gf_bs_read_int(bs, NbBits));
		return GF_OK;

	case GF_SG_VRML_SFROTATION:
		//forbidden in this Q mode
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}

//int in quant are either Linear Scalar fields or CoordIndex
//the quant is just a bitshifting into [0, 2^NbBits-1]
//so IntMin + ReadBit(NbBits) = value
GF_Err Q_DecInt(GF_BifsDecoder *codec, GF_BitStream *bs, u32 QType, SFInt32 b_min, u32 NbBits, void *field_ptr)
{
	switch (QType) {
	case QC_LINEAR_SCALAR:
	case QC_COORD_INDEX:
		*((SFInt32 *)field_ptr) = gf_bs_read_int(bs, NbBits) + b_min;
		return GF_OK;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
}

//SFRotation and SFVec3f are quantized as normalized vectors ,mapped on a cube
//in the UnitSphere (R=1.0)
GF_Err Q_DecCoordOnUnitSphere(GF_BifsDecoder *codec, GF_BitStream *bs, u32 NbBits, u32 NbComp, Fixed *m_ft)
{
	u32 i, orient, sign;
	s32 value;
	Fixed tang[4], delta;
	s32 dir;
	if (NbBits>32) return GF_NON_COMPLIANT_BITSTREAM;
	if (NbComp != 2 && NbComp != 3) return GF_BAD_PARAM;

	//only 2 or 3 comp in the quantized version
	dir = 1;
	if(NbComp == 2) dir -= 2 * gf_bs_read_int(bs, 1);

	orient = gf_bs_read_int(bs, 2);
	if ((orient==3) && (NbComp==2)) return GF_NON_COMPLIANT_BITSTREAM;

	for(i=0; i<NbComp; i++) {
		value = gf_bs_read_int(bs, NbBits) - (1 << (NbBits-1) );
		sign = (value >= 0) ? 1 : -1;
		m_ft[i] = sign * Q_InverseQuantize(0, 1, NbBits-1, sign*value);
	}
	delta = 1;
	for (i=0; i<NbComp; i++) {
		tang[i] = gf_tan(gf_mulfix(GF_PI/4, m_ft[i]) );
		delta += gf_mulfix(tang[i], tang[i]);
	}
	delta = gf_divfix(INT2FIX(dir), gf_sqrt(delta) );
	m_ft[orient] = delta;

	for (i=0; i<NbComp; i++) {
		m_ft[ (orient + i+1) % (NbComp+1) ] = gf_mulfix(tang[i], delta);
	}
	return GF_OK;
}

//parses a rotation
GF_Err Q_DecRotation(GF_BifsDecoder *codec, GF_BitStream *bs, u32 NbBits, void *field_ptr)
{
	u32 i;
	Fixed q, sin2, comp[4];
	GF_Err e;

	e = Q_DecCoordOnUnitSphere(codec, bs, NbBits, 3, comp);
	if (e) return e;

	q = 2 * gf_acos(comp[0]);
	sin2 = gf_sin(q / 2);

	if (ABS(sin2) <= FIX_EPSILON) {
		for (i=1; i<4; i++) comp[i] = 0;
		comp[3] = FIX_ONE;
	} else {
		for (i=1; i<4; i++) comp[i] = gf_divfix(comp[i], sin2);
	}
	((SFRotation *)field_ptr)->x = comp[1];
	((SFRotation *)field_ptr)->y = comp[2];
	((SFRotation *)field_ptr)->z = comp[3];
	((SFRotation *)field_ptr)->q = q;
	return GF_OK;
}

//parses a Normal vec
GF_Err Q_DecNormal(GF_BifsDecoder *codec, GF_BitStream *bs, u32 NbBits, void *field_ptr)
{
	Fixed comp[4];
	SFVec3f v;
	GF_Err e;
	e = Q_DecCoordOnUnitSphere(codec, bs, NbBits, 2, comp);
	if (e) return e;
	v.x = comp[0];
	v.y = comp[1];
	v.z = comp[2];
	gf_vec_norm(&v);
	*((SFVec3f *)field_ptr) = v;
	return GF_OK;
}

GF_Err gf_bifs_dec_unquant_field(GF_BifsDecoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
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
		NbBits = gf_bifs_dec_qp14_get_bits(codec);
		/*QP14 is always on, not having NbBits set means the coord field is set after the index field, hence not decodable*/
		if (!NbBits) return GF_NON_COMPLIANT_BITSTREAM;
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
		e = Q_DecFloat(codec, bs, field->fieldType, BMin, BMax, NbBits, field->far_ptr);
		break;
	//SFInt types
	case QC_LINEAR_SCALAR:
	case QC_COORD_INDEX:
		e = Q_DecInt(codec, bs, QType, (SFInt32) b_min, NbBits, field->far_ptr);
		break;
	//normalized fields (normals and vectors)
	case QC_NORMALS:
		//normal quant is only for SFVec3F
		if (field->fieldType != GF_SG_VRML_SFVEC3F) return GF_NON_COMPLIANT_BITSTREAM;
		e = Q_DecNormal(codec, bs, NbBits, field->far_ptr);
		break;
	case QC_ROTATION:
		//normal quant is only for SFRotation
		if (field->fieldType != GF_SG_VRML_SFROTATION) return GF_NON_COMPLIANT_BITSTREAM;
		e = Q_DecRotation(codec, bs, NbBits, field->far_ptr);
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (e) return e;
	return GF_OK;
}

#endif /*GPAC_DISABLE_BIFS*/
