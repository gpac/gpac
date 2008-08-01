/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


u32 gf_bifs_dec_qp14_get_bits(GF_BifsDecoder *codec);

typedef struct
{
	s32 comp_min[3];
	s32 previous_val[3];
	s32 current_val[3];
	s32 m_delta[3];

	u32 intra_mode, intra_inter, compNbBits, num_bounds, num_comp, num_fields, QNbBits;
	u8 QType;
	Bool use_default;
	SFVec3f BMin, BMax;

	s32 direction, orientation, inverse;
	
	u32 cur_field;

	GF_AAModel *models[3];
	GF_AAModel *dir_model;
	GF_AADecoder *dec;
} PredMF;


void PMF_ResetModels(PredMF *pmf)
{
	u32 i;
	for (i=0; i<pmf->num_bounds; i++) {
		gp_bifs_aa_model_init(pmf->models[i], pmf->compNbBits);
	}
	gp_bifs_aa_model_init(pmf->dir_model, 1);
}


Fixed PMF_UnquantizeFloat(s32 vq, Fixed BMin, Fixed BMax, u32 NbBits, Bool unit_vector)
{
	Fixed scale = 0;
	Fixed width = BMax - BMin;
	if (unit_vector) NbBits -= 1;
	if (width > FIX_EPSILON) {
		if (NbBits) {
			scale = gf_divfix(width , INT2FIX( (1<<NbBits) - 1) );
		} else {
			scale = width/2;
		}
	}
    return BMin + scale * vq;
}

GF_Err PMF_UnquantizeNormal(PredMF *pmf, GF_FieldInfo *field)
{
	void *slot;
	Fixed comp[3];
	Fixed tang[2];
 	u32 i;
	Fixed delta=FIX_ONE;
    for (i=0; i<2; i++) {
		Fixed v = PMF_UnquantizeFloat(pmf->current_val[i] - (1<< (pmf->QNbBits -1) ), 0 , FIX_ONE, pmf->QNbBits, 1);
		tang[i]= gf_tan(gf_mulfix(GF_PI * 4, v));
		delta += gf_mulfix(tang[i], tang[i]);
	}
    delta = gf_divfix(pmf->direction, gf_sqrt(delta) );

    comp[(pmf->orientation) % 3] = delta;
    for (i=0; i<2; i++) 
		comp[(pmf->orientation + i+1)%3] = gf_mulfix(tang[i], delta);

	gf_sg_vrml_mf_get_item(field->far_ptr, field->fieldType, &slot, pmf->cur_field);
	((SFVec3f *)slot)->x = comp[0];
	((SFVec3f *)slot)->y = comp[1];
	((SFVec3f *)slot)->z = comp[2];
	return GF_OK;
}

GF_Err PMF_UnquantizeRotation(PredMF *pmf, GF_FieldInfo *field)
{
	u32 i;
	void *slot;
	Fixed comp[4];
	Fixed tang[3];
	Fixed sine, delta = FIX_ONE;

	for (i=0; i<3; i++) {
		Fixed v = PMF_UnquantizeFloat(pmf->current_val[i] - (1<<(pmf->QNbBits - 1)), 0, FIX_ONE, pmf->QNbBits, 1);
		tang[i] = gf_tan(gf_mulfix(GF_PI / 4, v));
		delta += gf_mulfix(tang[i], tang[i]);
	}
    delta = gf_divfix(pmf->direction , gf_sqrt(delta) );

    comp[(pmf->orientation)%4] = delta;
    for (i=0; i<3; i++) 
		comp[(pmf->orientation + i+1)%4] = gf_mulfix(tang[i], delta);
  
	gf_sg_vrml_mf_get_item(field->far_ptr, field->fieldType, &slot, pmf->cur_field);
	delta = 2 * gf_acos(comp[0]);
	sine = gf_sin(delta / 2);
	if (sine != 0) {
		for(i=1; i<4; i++)
			comp[i] = gf_divfix(comp[i], sine);

		((SFRotation *)slot)->x = comp[1];
		((SFRotation *)slot)->y = comp[2];
		((SFRotation *)slot)->z = comp[3];
	} else {
		((SFRotation *)slot)->x = FIX_ONE;
		((SFRotation *)slot)->y = 0;
		((SFRotation *)slot)->z = 0;
	}
	((SFRotation *)slot)->q = delta;
	return GF_OK;
}

GF_Err PMF_Unquantize(PredMF *pmf, GF_FieldInfo *field)
{
	void *slot;
	if (pmf->QType == QC_NORMALS) {
		return PMF_UnquantizeNormal(pmf, field);
	}
	if (pmf->QType == QC_ROTATION) {
		return PMF_UnquantizeRotation(pmf, field);
	}
	/*regular*/
	gf_sg_vrml_mf_get_item(field->far_ptr, field->fieldType, &slot, pmf->cur_field);
	switch (field->fieldType) {
	case GF_SG_VRML_MFVEC3F:
		((SFVec3f *) slot)->x = PMF_UnquantizeFloat(pmf->current_val[0], pmf->BMin.x, pmf->BMax.x, pmf->QNbBits, 0);
		((SFVec3f *) slot)->y = PMF_UnquantizeFloat(pmf->current_val[1], pmf->BMin.y, pmf->BMax.y, pmf->QNbBits, 0);
		((SFVec3f *) slot)->z = PMF_UnquantizeFloat(pmf->current_val[2], pmf->BMin.z, pmf->BMax.z, pmf->QNbBits, 0);
		break;
	case GF_SG_VRML_MFVEC2F:
		((SFVec2f *) slot)->x = PMF_UnquantizeFloat(pmf->current_val[0], pmf->BMin.x, pmf->BMax.x, pmf->QNbBits, 0);
		((SFVec2f *) slot)->y = PMF_UnquantizeFloat(pmf->current_val[1], pmf->BMin.y, pmf->BMax.y, pmf->QNbBits, 0);
		break;
	case GF_SG_VRML_MFFLOAT:
		*((SFFloat *) slot) = PMF_UnquantizeFloat(pmf->current_val[0], pmf->BMin.x, pmf->BMax.x, pmf->QNbBits, 0);
		break;
	case GF_SG_VRML_MFCOLOR:
		((SFColor *) slot)->red = PMF_UnquantizeFloat(pmf->current_val[0], pmf->BMin.x, pmf->BMax.x, pmf->QNbBits, 0);
		((SFColor *) slot)->green = PMF_UnquantizeFloat(pmf->current_val[1], pmf->BMin.y, pmf->BMax.y, pmf->QNbBits, 0);
		((SFColor *) slot)->blue = PMF_UnquantizeFloat(pmf->current_val[2], pmf->BMin.z, pmf->BMax.z, pmf->QNbBits, 0);
		break;
	case GF_SG_VRML_MFINT32:
		switch (pmf->QType) {
		case QC_LINEAR_SCALAR:
		case QC_COORD_INDEX:
			*((SFInt32 *) slot) = pmf->current_val[0] + (s32) pmf->BMin.x;
			break;
		}
		break;
	}
	return GF_OK;
}


GF_Err PMF_ParsePValue(PredMF *pmf, GF_BitStream *bs, GF_FieldInfo *field)
{
	u32 i, numModel;
	s32 prev_dir = 0;
	switch (pmf->QType) {
	case QC_NORMALS:
		prev_dir = pmf->direction;
		pmf->direction = gp_bifs_aa_decode(pmf->dec, pmf->dir_model);
		break;
	}
	/*decode (one model per component)*/
	numModel = 0;
	for (i=0; i<pmf->num_comp; i++) {
		pmf->previous_val[i]= pmf->current_val[i];
		pmf->current_val[i] =  gp_bifs_aa_decode(pmf->dec, pmf->models[numModel]) + pmf->comp_min[numModel];
		numModel += (pmf->num_bounds==1) ? 0 : 1;
	}

	/*compensate values*/
	switch (pmf->QType) {
	case QC_NORMALS:
	case QC_ROTATION:
		/*NOT TESTED*/
		{
			s32 temp_val[3];
			s32 diff_dir = prev_dir * (pmf->direction ? -1 : 1);
			s32 inv=1;
			s32 diff_ori = 0;
			s32 shift = 1 << (pmf->QNbBits - 1);

			for (i=0; i<3; i++) {
				pmf->previous_val[i] -= shift;
				pmf->current_val[i] -= shift;
			}
			for (i=0; i< pmf->num_comp; i++) {
				temp_val[i] = pmf->previous_val[i] + pmf->current_val[i];
				if ( abs(temp_val[i]) > shift - 1) {
					diff_ori = i+1;
					inv = ( temp_val[i] > 0) ? 1 : -1;
					break;
				}
			}
			if (diff_ori != 0) {
				s32 k=0;
				for (i=0; i< pmf->num_comp - diff_ori; i++) {
					k = (i + diff_ori) % pmf->num_comp;
					temp_val[i] = inv * ( pmf->previous_val[i] + pmf->current_val[i]);
				}
				k = diff_ori - 1;
				temp_val[pmf->num_comp - diff_ori] = inv * 2 * (shift - 1) - (pmf->previous_val[k] + pmf->current_val[k]) ; 
				for (i = pmf->num_comp - diff_ori + 1; i<pmf->num_comp; i++) {
					k = (i+diff_ori-1) % pmf->num_comp;
					temp_val[i] = inv * (pmf->previous_val[k] + pmf->current_val[k]);
				}
			}
			pmf->orientation = (pmf->orientation + diff_ori) % (pmf->num_comp + 1);
			pmf->direction = diff_dir * inv;
			for (i=0; i< pmf->num_comp; i++) 
				pmf->current_val[i]= temp_val[i] + shift;
		}
		break;
	default:
		for (i=0; i< pmf->num_comp; i++)
			pmf->current_val[i] += pmf->previous_val[i];
	}
	/*unquantize*/
	return PMF_Unquantize(pmf, field);
}

GF_Err PMF_ParseIValue(PredMF *pmf, GF_BitStream *bs, GF_FieldInfo *field)
{
	u32 i;
	switch (pmf->QType) {
	case QC_NORMALS:
		i = gf_bs_read_int(bs, 1);
		pmf->direction = i ? -1 : 1;
	case QC_ROTATION:
		pmf->orientation = gf_bs_read_int(bs, 2);
		break;
	}
	/*read all vals*/
	for (i=0; i<pmf->num_comp; i++) {
		pmf->current_val[i] = gf_bs_read_int(bs, pmf->QNbBits);
	}
	/*reset after each I*/
	if (pmf->cur_field + 1<pmf->num_fields) gp_bifs_aa_dec_reset(pmf->dec);

	return PMF_Unquantize(pmf, field);
}

/*bit access shall be done through the AA decoder since bits may be cached there*/
GF_Err PMF_UpdateArrayQP(PredMF *pmf, GF_BitStream *bs)
{
	u32 flag, i;
	switch (pmf->intra_mode) {
	case 1:
		flag = gf_bs_read_int(bs, 5);
		pmf->intra_inter = gf_bs_read_int(bs, flag);
	case 2:
	case 0:
		flag = gf_bs_read_int(bs, 1);
		if (flag) {
			pmf->compNbBits = gf_bs_read_int(bs, 5);
		}
		flag = gf_bs_read_int(bs, 1);
		if (flag) {
			for (i=0; i<pmf->num_bounds; i++) {
				flag = gf_bs_read_int(bs, pmf->QNbBits + 1);
				pmf->comp_min[i] = flag - (1<<pmf->QNbBits);
			}
		}
		break;
	}
	/*reset all models when new settings are received*/
	PMF_ResetModels(pmf);
	return GF_OK;
}



GF_Err gf_bifs_dec_pred_mf_field(GF_BifsDecoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
{
	GF_Err e;
	Bool HasQ;
	u8 AType;
	Fixed b_min, b_max;
	u32 i, flag;
	PredMF pmf;
	
	memset(&pmf, 0, sizeof(PredMF));
	
	HasQ = gf_bifs_get_aq_info(node, field->fieldIndex, &pmf.QType, &AType, &b_min, &b_max, &pmf.QNbBits);
	if (!HasQ || !pmf.QType) return GF_EOS;

	/*get NbBits for QP14 (QC_COORD_INDEX)*/
	if (pmf.QType == QC_COORD_INDEX) 
		pmf.QNbBits = gf_bifs_dec_qp14_get_bits(codec);

	pmf.BMin.x = pmf.BMin.y = pmf.BMin.z = b_min;
	pmf.BMax.x = pmf.BMax.y = pmf.BMax.z = b_max;

	/*check is the QP is on and retrieves the bounds*/
	if (!Q_IsTypeOn(codec->ActiveQP, pmf.QType, &pmf.QNbBits, &pmf.BMin, &pmf.BMax)) return GF_EOS;
	
	switch (field->fieldType) {
	case GF_SG_VRML_MFCOLOR:
	case GF_SG_VRML_MFVEC3F:
		if (pmf.QType==QC_NORMALS) {
			pmf.num_comp = 2;
			break;
		}
	case GF_SG_VRML_MFROTATION:
		pmf.num_comp = 3;
		break;
	case GF_SG_VRML_MFVEC2F:
		pmf.num_comp = 2;
		break;
	case GF_SG_VRML_MFFLOAT:
	case GF_SG_VRML_MFINT32:
		pmf.num_comp = 1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}


	/*parse array header*/
	flag = gf_bs_read_int(bs, 5);
	pmf.num_fields = gf_bs_read_int(bs, flag);
	pmf.intra_mode = gf_bs_read_int(bs, 2);
	switch (pmf.intra_mode) {
	case 1:
		flag = gf_bs_read_int(bs, 5);
		pmf.intra_inter = gf_bs_read_int(bs, flag);
		/*no break*/
	case 2:
	case 0:
		pmf.compNbBits = gf_bs_read_int(bs, 5);
		if (pmf.QType==1) pmf.num_bounds = 3;
		else if (pmf.QType==2) pmf.num_bounds = 2;
		else pmf.num_bounds = 1;
		for (i=0; i<pmf.num_bounds; i++) {
			flag = gf_bs_read_int(bs, pmf.QNbBits + 1);
			pmf.comp_min[i] = flag - (1<<pmf.QNbBits);
		}
		break;
	case 3:
		break;
	}


	pmf.dec = gp_bifs_aa_dec_new(bs);
	pmf.models[0] = gp_bifs_aa_model_new();
	pmf.models[1] = gp_bifs_aa_model_new();
	pmf.models[2] = gp_bifs_aa_model_new();
	pmf.dir_model = gp_bifs_aa_model_new();

	PMF_ResetModels(&pmf);

	gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, pmf.num_fields);
	pmf.cur_field = 0;
	/*parse initial I*/
	e = PMF_ParseIValue(&pmf, bs, field);
	if (e) return e;

	for (pmf.cur_field=1; pmf.cur_field<pmf.num_fields; pmf.cur_field++) {
		switch (pmf.intra_mode) {
		case 0:
			e = PMF_ParsePValue(&pmf, bs, field);
			break;

		/*NOT TESTED*/
		case 1:
			if (!(pmf.cur_field % pmf.intra_inter)) {
				/*resync bitstream*/
				gp_bifs_aa_dec_resync(pmf.dec);
				flag = gf_bs_read_int(bs, 1);
				/*update settings ?*/
				if (flag) {
					e = PMF_UpdateArrayQP(&pmf, bs);
					if (e) goto err_exit;
				}
				e = PMF_ParseIValue(&pmf, bs, field);
			} else {
				e = PMF_ParsePValue(&pmf, bs, field);
			}
			break;

		/*NOT TESTED*/
		case 2:
			/*is intra ? - WARNING: this is from the arithmetic context !!*/
			flag = gp_bifs_aa_dec_get_bit(pmf.dec);
			if (flag) {
				/*resync bitstream*/
				gp_bifs_aa_dec_resync_bit(pmf.dec);
				flag = gf_bs_read_int(bs, 1);
				/*update settings ?*/
				if (flag) {
					e = PMF_UpdateArrayQP(&pmf, bs);
					if (e) goto err_exit;
				}
				e = PMF_ParseIValue(&pmf, bs, field);
			} else {
				e = PMF_ParsePValue(&pmf, bs, field);
				gp_bifs_aa_dec_flush(pmf.dec);
			}
			break;
		}
		if (e) goto err_exit;
	}


	if (pmf.intra_mode==2) {
		gp_bifs_aa_dec_resync_bit(pmf.dec);
	} else {
		gp_bifs_aa_dec_resync(pmf.dec);
	}

err_exit:
	gp_bifs_aa_model_del(pmf.models[0]);
	gp_bifs_aa_model_del(pmf.models[1]);
	gp_bifs_aa_model_del(pmf.models[2]);
	gp_bifs_aa_model_del(pmf.dir_model);
	gp_bifs_aa_dec_del(pmf.dec);
	return e;
}

