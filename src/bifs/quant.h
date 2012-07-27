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


#ifndef __QUANTIZE_H
#define __QUANTIZE_H

#include <gpac/internal/bifs_dev.h>

#ifndef GPAC_DISABLE_BIFS

/*Quantization Categories*/
enum
{
	QC_NONE				=	0,
	QC_3DPOS			=	1, 
	QC_2DPOS			=	2, 
	QC_ORDER			=	3, 
	QC_COLOR			=	4, 
	QC_TEXTURE_COORD	=	5, 
	QC_ANGLE			=	6, 
	QC_SCALE			=	7, 
	QC_INTERPOL_KEYS	=	8, 
	QC_NORMALS			=	9, 
	QC_ROTATION			=	10, 
	QC_SIZE_3D			=	11, 
	QC_SIZE_2D			=	12, 
	QC_LINEAR_SCALAR	=	13, 
	QC_COORD_INDEX		=	14, 
	QC_RESERVED			=	15,
	QC_NOTDEF			=	16,
};

#ifdef GPAC_ENABLE_BIFS_PMF
GF_Err gf_bifs_dec_pred_mf_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
#endif

Bool Q_IsTypeOn(M_QuantizationParameter *qp, u32 q_type, u32 *NbBits, SFVec3f *b_min, SFVec3f *b_max);


/*QP14 decode*/
u32 gf_bifs_dec_qp14_get_bits(GF_BifsDecoder *codec);
void gf_bifs_dec_qp14_enter(GF_BifsDecoder * codec, Bool Enter);
void gf_bifs_dec_qp14_reset(GF_BifsDecoder * codec);
void gf_bifs_dec_qp14_set_length(GF_BifsDecoder * codec, u32 NbElements);
/*QP decoder (un)registration*/
GF_Err gf_bifs_dec_qp_set(GF_BifsDecoder *codec, GF_Node *qp);
GF_Err gf_bifs_dec_qp_remove(GF_BifsDecoder *codec, Bool ActivatePrev);

SFFloat gf_bifs_dec_mantissa_float(GF_BifsDecoder * codec, GF_BitStream *bs);
GF_Err gf_bifs_dec_unquant_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);

#ifndef GPAC_DISABLE_BIFS_ENC

/*QP14 encode*/
u32 gf_bifs_enc_qp14_get_bits(GF_BifsEncoder *codec);
void gf_bifs_enc_qp14_enter(GF_BifsEncoder *codec, Bool Enter);
void gf_bifs_enc_qp14_reset(GF_BifsEncoder *codec);
void gf_bifs_enc_qp14_set_length(GF_BifsEncoder *codec, u32 NbElements);
/*QP encoder (un)registration*/
GF_Err gf_bifs_enc_qp_set(GF_BifsEncoder *codec, GF_Node *qp);
GF_Err gf_bifs_enc_qp_remove(GF_BifsEncoder *codec, Bool ActivatePrev);
void gf_bifs_enc_mantissa_float(GF_BifsEncoder * codec, SFFloat val, GF_BitStream *bs);
GF_Err gf_bifs_enc_quant_field(GF_BifsEncoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);

#endif /*GPAC_DISABLE_BIFS_ENC*/

#ifdef GPAC_ENABLE_BIFS_PMF

/*
		Predictive MFField decode - mainly IM1 code (GPL H263 AA coder used)
*/

typedef struct _aamodel GF_AAModel;
GF_AAModel *gp_bifs_aa_model_new();
void gp_bifs_aa_model_del(GF_AAModel *model);
void gp_bifs_aa_model_init(GF_AAModel *model, u32 nbBits);

typedef struct _aadecoder GF_AADecoder;
GF_AADecoder *gp_bifs_aa_dec_new(GF_BitStream *bs);
void gp_bifs_aa_dec_del(GF_AADecoder *dec);
void gp_bifs_aa_dec_flush(GF_AADecoder *dec);
/*get input bit*/
s32 gp_bifs_aa_dec_get_bit(GF_AADecoder *dec);
/*resync after input bit has been fetched (full buffer (16 bit) rewind in source stream)*/
void gp_bifs_aa_dec_resync_bit(GF_AADecoder *dec);
/*resync bitstream*/
void gp_bifs_aa_dec_resync(GF_AADecoder *dec);
/*decode symbol in given model*/
s32 gp_bifs_aa_decode(GF_AADecoder *dec, GF_AAModel *model);
/*reset decoder - called after each parsed I frame*/
void gp_bifs_aa_dec_reset(GF_AADecoder *dec);


#endif /*GPAC_ENABLE_BIFS_PMF*/


#endif /*GPAC_DISABLE_BIFS*/

#endif	
