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

#include "quant.h"

#ifdef GPAC_ENABLE_BIFS_PMF

/*
	Adaptive Arithmethic Decoder from Annex G (Normative)

	 Original code:
		   sac.c, part of tmndecode (H.263 decoder)

	Copyright (C) 1996  Telenor R&D, Norway
         Karl Olav Lillevold <Karl.Lillevold@nta.no>

	Karl Olav Lillevold               <Karl.Lillevold@nta.no>
		Telenor Research and Development
		P.O.Box 83                        tel.:   +47 63 84 84 00
		N-2007 Kjeller, Norway            fax.:   +47 63 81 00 76

    Robert Danielsen                  e-mail: Robert.Danielsen@nta.no
		Telenor Research and Development  www:    http://www.nta.no/brukere/DVC/
		P.O.Box 83                        tel.:   +47 63 84 84 00
		N-2007 Kjeller, Norway            fax.:   +47 63 81 00 76

	 Original license: GNU Lesser General Public License

	This is basically the code from IM1 (the spec is not up-to-date with ref soft)
*/




#define AAM_Q1 16384
#define AAM_Q2 32768
#define AAM_Q3 49152
#define AAM_TOP 65535
#define AAM_ZEROMAX 22
#define AAM_MAX_FREQ 16383

struct _aamodel
{
	s32 nb_symb;
	s32	*cumul_freq;
	s32 *freq;
};

struct _aadecoder
{
	u32 low, high, code_value;
	s32 zero_run;
	Bool Bit;
	GF_BitStream *bs;
	Bool needs_flush;
	u32 read_bits, used_bits;
	/*amount of fake bits (bit read after end of stream)*/
	u32 skip_bits;
};



GF_AAModel *gp_bifs_aa_model_new()
{
	GF_AAModel *tmp;
	GF_SAFEALLOC(tmp, GF_AAModel);
	return tmp;
}

void gp_bifs_aa_model_del(GF_AAModel *model)
{
	if (model->cumul_freq) gf_free(model->cumul_freq);
	if (model->freq) gf_free(model->freq);
	gf_free(model);
}

void gp_bifs_aa_model_init(GF_AAModel *model, u32 nbBits)
{
	s32 i;
	model->nb_symb = 1<<nbBits;
	if (model->cumul_freq) gf_free(model->cumul_freq);
	if (model->freq) gf_free(model->freq);
	model->freq = (s32*)gf_malloc(sizeof(s32) * model->nb_symb);
	model->cumul_freq = (s32*)gf_malloc(sizeof(s32) * (model->nb_symb+1));

	for(i=0; i<model->nb_symb; i++) {
		model->freq[i]=1;
		model->cumul_freq[i] = model->nb_symb - i;
	}
	model->cumul_freq[model->nb_symb] = 0;
}

void UpdateAAModel(GF_AAModel *model, s32 symbol)
{
	s32 sum, i;

	/*The model is rescaled to avoid overflow*/
	if (model->cumul_freq[0] == AAM_MAX_FREQ) {
		sum = 0;
		for (i=model->nb_symb-1; i>=0; i--) {
			model->freq[i] = (model->freq[i]+1)/2;
			sum += model->freq[i];
			model->cumul_freq[i] = sum;
		}
		model->cumul_freq[model->nb_symb] = 0;
	}
	model->freq[symbol] ++;
	model->cumul_freq[symbol] ++;
	while (symbol>0) model->cumul_freq[--symbol] ++;
}


void gf_bs_rewind_bits(GF_BitStream *bs, u64 nbBits);

GF_AADecoder *gp_bifs_aa_dec_new(GF_BitStream *bs)
{
	GF_AADecoder *tmp;
	GF_SAFEALLOC(tmp, GF_AADecoder);
	if (tmp)
		tmp->bs = bs;
	return tmp;
}
void gp_bifs_aa_dec_del(GF_AADecoder *dec)
{
	gf_free(dec);
}

static Bool bit_out_psc_layer(GF_AADecoder *dec)
{
	s32 v;
	if (gf_bs_bits_available(dec->bs) ) {
		v = gf_bs_read_int(dec->bs, 1) ? 1 : 0;
		dec->read_bits++;
	} else {
		v = (dec->zero_run == AAM_MAX_FREQ) ? 1 : 0;
		dec->skip_bits++;
	}

	if (dec->zero_run == AAM_ZEROMAX) {
		if (!v) return 0;
		v = gf_bs_read_int(dec->bs, 1) ? 1 : 0;
		dec->zero_run = 0;
		dec->read_bits++;
		dec->used_bits++;
	}
	dec->Bit = v;
	dec->needs_flush = 1;

	if (!dec->Bit)
		dec->zero_run++;
	else
		dec->zero_run = 0;

	return 1;
}

void gp_bifs_aa_dec_resync(GF_AADecoder *dec)
{
	u32 rewind;
	if (!dec->needs_flush) return;
	/*magic number from IM1 (spec is wrong there)*/
	rewind = 14;
	//spec: rewind = dec->read_bits - 1 - (dec->used_bits+1);
	if (dec->skip_bits < rewind) gf_bs_rewind_bits(dec->bs, rewind - dec->skip_bits);

	dec->needs_flush = 0;
	dec->code_value = 0;
	dec->low = 0;
	dec->high = AAM_TOP;
	dec->zero_run = 0;
	dec->used_bits = dec->read_bits = 0;
}

void gp_bifs_aa_dec_flush(GF_AADecoder *dec)
{
	if (!bit_out_psc_layer(dec)) return;
	dec->code_value = 2 * dec->code_value + dec->Bit;
	if (!bit_out_psc_layer(dec)) return;
	dec->code_value = (2 * dec->code_value + dec->Bit) & AAM_TOP;

	dec->low = 0;
	dec->high = AAM_TOP;
	dec->zero_run = 0;
}

void gp_bifs_aa_dec_resync_bit(GF_AADecoder *dec)
{
	if (dec->needs_flush && (dec->skip_bits < 16))
		gf_bs_rewind_bits(dec->bs, 16 - dec->skip_bits);

	dec->needs_flush = 0;
}

s32 gp_bifs_aa_dec_get_bit(GF_AADecoder *dec)
{
	/*get flag*/
	s32 res = dec->code_value >> 15;
	/*get next bit*/
	bit_out_psc_layer(dec);
	dec->code_value = (2 * dec->code_value + dec->Bit) & AAM_TOP;
	return res;
}

void gp_bifs_aa_dec_reset(GF_AADecoder *dec)
{
	s32 i;
	dec->code_value = 0;
	dec->low = 0;
	dec->high = AAM_TOP;
	dec->zero_run = 0;
	for (i=0; i < 16; i++) {
		if (!bit_out_psc_layer(dec)) return;
		dec->code_value = 2 * dec->code_value + dec->Bit;
	}
	dec->used_bits = 0;
}


static s32 AADec_Dec(GF_AADecoder *dec, GF_AAModel *model)
{
	u32 sac_index, len;
	s32 sum;

	len = dec->high - dec->low + 1;
	sum = (-1 + (dec->code_value - dec->low + 1) * model->cumul_freq[0]) / len;

	for (sac_index = 1; model->cumul_freq[sac_index] > sum; sac_index++) {
	}
	dec->high = dec->low - 1 + (len * model->cumul_freq[sac_index-1]) / model->cumul_freq[0];
	dec->low += (len * model->cumul_freq[sac_index]) / model->cumul_freq[0];

	for ( ; ; ) {
		if (dec->high < AAM_Q2) {
		} else if (dec->low >= AAM_Q2) {
			dec->code_value -= AAM_Q2;
			dec->low -= AAM_Q2;
			dec->high -= AAM_Q2;
		}
		else if (dec->low >= AAM_Q1 && dec->high < AAM_Q3) {
			dec->code_value -= AAM_Q1;
			dec->low -= AAM_Q1;
			dec->high -= AAM_Q1;
		} else {
			break;
		}

		dec->low *= 2;
		dec->high = 2 * dec->high + 1;
		if (!bit_out_psc_layer(dec)) return -1;
		dec->code_value = 2 * dec->code_value + dec->Bit;
		dec->used_bits++;
	}
	return (sac_index - 1);
}

s32 gp_bifs_aa_decode(GF_AADecoder *dec, GF_AAModel *model)
{
	s32 v;
	v = AADec_Dec(dec, model);
	UpdateAAModel(model, v);
	return v;
}

#endif /*GPAC_ENABLE_BIFS_PMF*/

