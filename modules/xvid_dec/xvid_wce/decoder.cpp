/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Decoder Module -
 *
 *  Copyright(C) 2002      MinChen <chenm001@163.com>
 *               2002-2003 Peter Ross <pross@xvid.org>
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id: decoder.cpp,v 1.4 2008-08-20 13:57:54 jeanlf Exp $
 *
 ****************************************************************************/

#include "quant.h"
#include "quant_matrix.h"
#include "interpolate8x8.h"
#include "reduced.h"
#include "mbprediction.h"
#include "gmc.h"
#include "mem_align.h"

#ifdef __SYMBIAN32__
#include <e32base.h>
#endif

//----------------------------

# define DECLARE_ALIGNED_MATRIX(name,sizex,sizey,type,alignment) \
                type name##_storage[(sizex)*(sizey)+(alignment)-1]; \
                type * name = (type *) (((int) name##_storage+(alignment - 1)) & ~((int)(alignment)-1))

//----------------------------
//----------------------------
/* K = 4 */
static const dword roundtab_76[16] = { 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1 };

/* K = 1 */
const dword roundtab_79[4] = { 0, 1, 0, 0 };

//----------------------------

int S_decoder::Resize() {
	//free existing
	image_destroy(&cur, edged_width, edged_height);
	image_destroy(&refn[0], edged_width, edged_height);
	image_destroy(&refn[1], edged_width, edged_height);
	image_destroy(&tmp, edged_width, edged_height);
	image_destroy(&qtmp, edged_width, edged_height);

	image_destroy(&gmc, edged_width, edged_height);

	if(last_mbs)
		xvid_free(last_mbs);
	if(mbs)
		xvid_free(mbs);

	//realloc
	mb_width = (width + 15) / 16;
	mb_height = (height + 15) / 16;

	edged_width = 16 * mb_width + 2 * EDGE_SIZE;
	edged_height = 16 * mb_height + 2 * EDGE_SIZE;

	if(image_create(&cur, edged_width, edged_height)) {
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}

	if(image_create(&refn[0], edged_width, edged_height)) {
		image_destroy(&cur, edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}

	//support B-frame to reference last 2 frame
	if(image_create(&refn[1], edged_width, edged_height)) {
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}
	if(image_create(&tmp, edged_width, edged_height)) {
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		image_destroy(&refn[1], edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}

	if(image_create(&qtmp, edged_width, edged_height)) {
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		image_destroy(&refn[1], edged_width, edged_height);
		image_destroy(&tmp, edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}

	if(image_create(&gmc, edged_width, edged_height)) {
		image_destroy(&qtmp, edged_width, edged_height);
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		image_destroy(&refn[1], edged_width, edged_height);
		image_destroy(&tmp, edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}

	mbs = (MACROBLOCK*)xvid_malloc(sizeof(MACROBLOCK) * mb_width * mb_height, CACHE_LINE);
	if(mbs == NULL) {
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		image_destroy(&refn[1], edged_width, edged_height);
		image_destroy(&tmp, edged_width, edged_height);
		image_destroy(&qtmp, edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}
	MemSet(mbs, 0, sizeof(MACROBLOCK) * mb_width * mb_height);

	//for skip MB flag
	last_mbs = (MACROBLOCK*)xvid_malloc(sizeof(MACROBLOCK) * mb_width * mb_height, CACHE_LINE);
	if(!last_mbs) {
		xvid_free(mbs);
		image_destroy(&cur, edged_width, edged_height);
		image_destroy(&refn[0], edged_width, edged_height);
		image_destroy(&refn[1], edged_width, edged_height);
		image_destroy(&tmp, edged_width, edged_height);
		image_destroy(&qtmp, edged_width, edged_height);
		xvid_free(this);
		return XVID_ERR_MEMORY;
	}
	MemSet(last_mbs, 0, sizeof(MACROBLOCK) * mb_width * mb_height);

	return 0;
}

//----------------------------

int decoder_create(xvid_dec_create_t *create) {

	if(XVID_VERSION_MAJOR(create->version) != 1)   /* v1.x.x */
		return XVID_ERR_VERSION;

	S_decoder *dec = (S_decoder*)xvid_malloc(sizeof(S_decoder), CACHE_LINE);
	//S_decoder *dec = new(ELeave) S_decoder(create);
	if(!dec)
		return XVID_ERR_MEMORY;

	return dec->Init(create);
}

//----------------------------

S_decoder::S_decoder(xvid_dec_create_t *create):
#ifdef PROFILE
	prof(*create->prof),
#endif
	time_inc_resolution(0),
	fixed_time_inc(0),
	time_inc_bits(0),
	shape(0),
	quant_bits(0),
	quant_type(0),
	mpeg_quant_matrices(0),
	quarterpel(0),
	complexity_estimation_disable(0),
	interlacing(false),
	top_field_first(0),
	alternate_vertical_scan(0),
	aspect_ratio(0),
	par_width(0),
	par_height(0),
	sprite_enable(0),
	sprite_warping_points(0),
	sprite_warping_accuracy(0),
	sprite_brightness_change(0),
	newpred_enable(0),
	reduced_resolution_enable(false),
	bs_version(0),
	width(0),
	height(0),
	edged_width(0),
	edged_height(0),
	mb_width(0),
	mb_height(0),
	mbs(0),
	last_mbs(0),
	last_coding_type(0),
	frames(0),
	time(0),
	time_base(0),
	last_time_base(0),
	last_non_b_time(0),
	time_pp(0),
	time_bp(0),
	fixed_dimensions(false),
	scalability(false),
	low_delay(false),
	low_delay_default(false),
	last_reduced_resolution(false),
	packed_mode(false)
{
	MemSet(&p_fmv, 0, sizeof(p_fmv));
	MemSet(&p_bmv, 0, sizeof(p_bmv));
}

//----------------------------

S_decoder::~S_decoder() {

	xvid_free(last_mbs);
	xvid_free(mbs);

	//image based GMC
	image_destroy(&gmc, edged_width, edged_height);

	image_destroy(&refn[0], edged_width, edged_height);
	image_destroy(&refn[1], edged_width, edged_height);
	image_destroy(&tmp, edged_width, edged_height);
	image_destroy(&qtmp, edged_width, edged_height);
	image_destroy(&cur, edged_width, edged_height);
	xvid_free(mpeg_quant_matrices);
}

//----------------------------

int S_decoder::Init(xvid_dec_create_t *create) {

	mpeg_quant_matrices = (dword*)xvid_malloc(sizeof(dword) * 64 * 8, CACHE_LINE);
	if(!mpeg_quant_matrices) {
		delete this;
		return XVID_ERR_MEMORY;
	}

	create->handle = this;

	width = create->width;
	height = create->height;

	cur.Null();
	refn[0].Null();
	refn[1].Null();
	tmp.Null();
	qtmp.Null();

	//image based GMC
	gmc.Null();


	mbs = NULL;
	last_mbs = NULL;

	init_mpeg_matrix(mpeg_quant_matrices);

	//For B-frame support (used to save reference frame's time
	frames = 0;
	time = time_base = last_time_base = 0;
	low_delay = false;
	packed_mode = false;

	fixed_dimensions = (width > 0 && height > 0);

	init_vlc_tables();
	idct_int32_init();

	if(fixed_dimensions)
		return Resize();
	return 0;
}

//----------------------------

static const int dquant_table[4] = {
	-1, -2, 1, 2
};

//----------------------------

static dword get_dc_scaler(dword quant, dword lum) {

	if(quant < 5)
		return 8;
	if(quant < 25 && !lum)
		return (quant + 13) / 2;
	if(quant < 9)
		return 2 * quant;
	if(quant < 25)
		return quant + 8;
	if(lum)
		return 2 * quant - 16;
	return quant - 6;
}

//---------------------------

#ifdef _ARM_
extern"C"
void XVID_ClearMatrix(void *dst);

#else
void XVID_ClearMatrix(void *dst);
#ifndef USE_ARM_ASM
inline void XVID_ClearMatrix(void *dst) {
	MemSet(dst, 0, 64 * sizeof(int));
}
#endif
#endif

//--------------------------
/* decode an intra macroblock */
void S_decoder::MBIntra(MACROBLOCK *pMB, dword x_pos, dword y_pos, dword acpred_flag, dword cbp, Bitstream *bs,
                        dword quant, dword intra_dc_threshold, dword bound, bool reduced_resolution) {

	DECLARE_ALIGNED_MATRIX(block, 6, 64, int, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int, CACHE_LINE);

	int i;

	dword stride = edged_width;
	const dword stride2 = stride / 2;
	byte *pY_Cur, *pU_Cur, *pV_Cur;

	if(reduced_resolution) {
		pY_Cur = cur.y + (y_pos << 5) * stride + (x_pos << 5);
		pU_Cur = cur.u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = cur.v + (y_pos << 4) * stride2 + (x_pos << 4);
	} else {
		pY_Cur = cur.y + (y_pos << 4) * stride + (x_pos << 4);
		pU_Cur = cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = cur.v + (y_pos << 3) * stride2 + (x_pos << 3);
	}

	MemSet(block, 0, 6 * 64 * sizeof(int));  //clear

	const dword iQuant = pMB->quant;
	for(i = 0; i < 6; i++) {
		dword iDcScaler = get_dc_scaler(iQuant, i < 4);
		int predictors[8];
		int start_coeff;

		predict_acdc(mbs, x_pos, y_pos, mb_width, i, &block[i * 64], iQuant, iDcScaler, predictors, bound);
		if(!acpred_flag)
			pMB->acpred_directions[i] = 0;

		if(quant < intra_dc_threshold) {
			PROF_S(PROF_1);
			int dc_size;
			int dc_dif;

			dc_size = i < 4 ? bs->get_dc_size_lum() : bs->get_dc_size_chrom();
			dc_dif = dc_size ? bs->get_dc_dif(dc_size) : 0;

			if (dc_size > 8) {
				bs->Skip(1);   /* marker */
			}

			block[i * 64 + 0] = dc_dif;
			start_coeff = 1;

			DPRINTF(XVID_DEBUG_COEFF,"block[0] %i\n", dc_dif);
			PROF_E(PROF_1);
		} else {
			start_coeff = 0;
		}
		if(cbp & (1 << (5 - i))) {
			//coded
			int direction = alternate_vertical_scan ? 2 : pMB->acpred_directions[i];
			get_intra_block(bs, &block[i * 64], direction, start_coeff);
		}
		add_acdc(pMB, i, &block[i * 64], iDcScaler, predictors);
		if(quant_type == 0) {
			dequant_h263_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler, mpeg_quant_matrices);
		} else {
			dequant_mpeg_intra(&data[i * 64], &block[i * 64], iQuant, iDcScaler, mpeg_quant_matrices);
		}
		InverseDiscreteCosineTransform(&data[i * 64]);
	}

	dword next_block = stride * 8;
	if(interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}
	if(reduced_resolution) {
		next_block*=2;
		copy_upsampled_8x8_16to8(pY_Cur, &data[0 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + 16, &data[1 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + next_block, &data[2 * 64], stride);
		copy_upsampled_8x8_16to8(pY_Cur + 16 + next_block, &data[3 * 64], stride);
		copy_upsampled_8x8_16to8(pU_Cur, &data[4 * 64], stride2);
		copy_upsampled_8x8_16to8(pV_Cur, &data[5 * 64], stride2);
	} else {
		transfer_16to8copy(pY_Cur, &data[0 * 64], stride);
		transfer_16to8copy(pY_Cur + 8, &data[1 * 64], stride);
		transfer_16to8copy(pY_Cur + next_block, &data[2 * 64], stride);
		transfer_16to8copy(pY_Cur + 8 + next_block, &data[3 * 64], stride);
		transfer_16to8copy(pU_Cur, &data[4 * 64], stride2);
		transfer_16to8copy(pV_Cur, &data[5 * 64], stride2);
	}
}

//----------------------------

void S_decoder::mb_decode(const dword cbp, Bitstream * bs, byte * pY_Cur, byte * pU_Cur, byte * pV_Cur, bool reduced_resolution, const MACROBLOCK *pMB) {

	DECLARE_ALIGNED_MATRIX(block, 1, 64, int, CACHE_LINE);
	DECLARE_ALIGNED_MATRIX(data, 6, 64, int, CACHE_LINE);

	int i;
	int stride = edged_width;
	const int stride2 = stride/2;

	const dword iQuant = pMB->quant;
	const int direction = alternate_vertical_scan ? 2 : 0;
	const quant_interFuncPtr dequant = quant_type == 0 ? dequant_h263_inter : dequant_mpeg_inter;
	for(i = 0; i < 6; i++) {
		if(cbp & (1 << (5 - i))) {
			//coded
			XVID_ClearMatrix(block);
			get_inter_block(bs, block, direction);
			dequant(&data[i * 64], block, iQuant, mpeg_quant_matrices);
			InverseDiscreteCosineTransform(&data[i * 64]);
		}
	}

	int next_block = stride * (reduced_resolution ? 16 : 8);
	if(interlacing && pMB->field_dct) {
		next_block = stride;
		stride *= 2;
	}

	if(reduced_resolution) {
		if(cbp & 32)
			add_upsampled_8x8_16to8(pY_Cur, &data[0 * 64], stride);
		if(cbp & 16)
			add_upsampled_8x8_16to8(pY_Cur + 16, &data[1 * 64], stride);
		if(cbp & 8)
			add_upsampled_8x8_16to8(pY_Cur + next_block, &data[2 * 64], stride);
		if(cbp & 4)
			add_upsampled_8x8_16to8(pY_Cur + 16 + next_block, &data[3 * 64], stride);
		if(cbp & 2)
			add_upsampled_8x8_16to8(pU_Cur, &data[4 * 64], stride2);
		if(cbp & 1)
			add_upsampled_8x8_16to8(pV_Cur, &data[5 * 64], stride2);
	} else {
		if(cbp & 32)
			transfer_16to8add(pY_Cur, &data[0 * 64], stride);
		if(cbp & 16)
			transfer_16to8add(pY_Cur + 8, &data[1 * 64], stride);
		if(cbp & 8)
			transfer_16to8add(pY_Cur + next_block, &data[2 * 64], stride);
		if(cbp & 4)
			transfer_16to8add(pY_Cur + 8 + next_block, &data[3 * 64], stride);
		if(cbp & 2)
			transfer_16to8add(pU_Cur, &data[4 * 64], stride2);
		if(cbp & 1)
			transfer_16to8add(pV_Cur, &data[5 * 64], stride2);
	}
}

//----------------------------

void S_decoder::DecodeInterMacroBlock(const MACROBLOCK * pMB, dword x_pos, dword y_pos, dword cbp, Bitstream *bs,
                                      bool rounding, bool reduced_resolution, int ref) {

	dword stride = edged_width;
	dword stride2 = stride / 2;
	dword i;

	byte *pY_Cur, *pU_Cur, *pV_Cur;

	int uv_dx, uv_dy;
	VECTOR mv[4];              //local copy of mvs

	if(reduced_resolution) {
		pY_Cur = cur.y + (y_pos << 5) * stride + (x_pos << 5);
		pU_Cur = cur.u + (y_pos << 4) * stride2 + (x_pos << 4);
		pV_Cur = cur.v + (y_pos << 4) * stride2 + (x_pos << 4);
		for (i = 0; i < 4; i++) {
			mv[i].x = RRV_MV_SCALEUP(pMB->mvs[i].x);
			mv[i].y = RRV_MV_SCALEUP(pMB->mvs[i].y);
		}
	} else {
		pY_Cur = cur.y + (y_pos << 4) * stride + (x_pos << 4);
		pU_Cur = cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
		pV_Cur = cur.v + (y_pos << 3) * stride2 + (x_pos << 3);
		for(i = 0; i < 4; i++)
			mv[i] = pMB->mvs[i];
	}

	if(pMB->mode != MODE_INTER4V) {
		//INTER, INTER_Q, NOT_CODED, FORWARD, BACKWARD

		uv_dx = mv[0].x;
		uv_dy = mv[0].y;
		if (quarterpel) {
			uv_dx /= 2;
			uv_dy /= 2;
		}
		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];

		if(reduced_resolution)
			interpolate32x32_switch(cur.y, refn[0].y, 32*x_pos, 32*y_pos, mv[0].x, mv[0].y, stride, rounding);
		else if(quarterpel)
			interpolate16x16_quarterpel(cur.y, refn[ref].y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, mv[0].x, mv[0].y, stride, rounding);
		else
			interpolate16x16_switch(cur.y, refn[ref].y, 16*x_pos, 16*y_pos, mv[0].x, mv[0].y, stride, rounding);
	} else {
		//MODE_INTER4V

		if(quarterpel) {
			uv_dx = (mv[0].x / 2) + (mv[1].x / 2) + (mv[2].x / 2) + (mv[3].x / 2);
			uv_dy = (mv[0].y / 2) + (mv[1].y / 2) + (mv[2].y / 2) + (mv[3].y / 2);
		} else {
			uv_dx = mv[0].x + mv[1].x + mv[2].x + mv[3].x;
			uv_dy = mv[0].y + mv[1].y + mv[2].y + mv[3].y;
		}

		uv_dx = (uv_dx >> 3) + roundtab_76[uv_dx & 0xf];
		uv_dy = (uv_dy >> 3) + roundtab_76[uv_dy & 0xf];

		if(reduced_resolution) {
			interpolate16x16_switch(cur.y, refn[0].y, 32*x_pos,      32*y_pos,      mv[0].x, mv[0].y, stride, rounding);
			interpolate16x16_switch(cur.y, refn[0].y, 32*x_pos + 16, 32*y_pos,      mv[1].x, mv[1].y, stride, rounding);
			interpolate16x16_switch(cur.y, refn[0].y, 32*x_pos,      32*y_pos + 16, mv[2].x, mv[2].y, stride, rounding);
			interpolate16x16_switch(cur.y, refn[0].y, 32*x_pos + 16, 32*y_pos + 16, mv[3].x, mv[3].y, stride, rounding);
			interpolate16x16_switch(cur.u, refn[0].u, 16*x_pos,      16*y_pos,      uv_dx,   uv_dy,   stride2, rounding);
			interpolate16x16_switch(cur.v, refn[0].v, 16*x_pos,      16*y_pos,      uv_dx,   uv_dy,   stride2, rounding);
		} else if(quarterpel) {
			interpolate8x8_quarterpel(cur.y, refn[0].y , qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, mv[0].x, mv[0].y, stride, rounding);
			interpolate8x8_quarterpel(cur.y, refn[0].y , qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos, mv[1].x, mv[1].y, stride, rounding);
			interpolate8x8_quarterpel(cur.y, refn[0].y , qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos + 8, mv[2].x, mv[2].y, stride, rounding);
			interpolate8x8_quarterpel(cur.y, refn[0].y , qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8, mv[3].x, mv[3].y, stride, rounding);
		} else {
			interpolate8x8_switch(cur.y, refn[0].y , 16*x_pos, 16*y_pos, mv[0].x, mv[0].y, stride, rounding);
			interpolate8x8_switch(cur.y, refn[0].y , 16*x_pos + 8, 16*y_pos, mv[1].x, mv[1].y, stride, rounding);
			interpolate8x8_switch(cur.y, refn[0].y , 16*x_pos, 16*y_pos + 8, mv[2].x, mv[2].y, stride, rounding);
			interpolate8x8_switch(cur.y, refn[0].y , 16*x_pos + 8, 16*y_pos + 8, mv[3].x, mv[3].y, stride, rounding);
		}
	}
	//chroma
	if(reduced_resolution) {
		interpolate16x16_switch(cur.u, refn[0].u, 16*x_pos, 16*y_pos, uv_dx, uv_dy, stride2, rounding);
		interpolate16x16_switch(cur.v, refn[0].v, 16*x_pos, 16*y_pos, uv_dx, uv_dy, stride2, rounding);
	} else {
		interpolate8x8_switch(cur.u, refn[ref].u,  8*x_pos,  8*y_pos, uv_dx, uv_dy, stride2, rounding);
		interpolate8x8_switch(cur.v, refn[ref].v,  8*x_pos,  8*y_pos, uv_dx, uv_dy, stride2, rounding);
	}
	if(cbp)
		mb_decode(cbp, bs, pY_Cur, pU_Cur, pV_Cur, reduced_resolution, pMB);
}

//----------------------------

inline int gmc_sanitize(int value, int quarterpel, int fcode) {

	int length = 1 << (fcode+4);

#if 0
	if (quarterpel) value *= 2;
#endif

	if (value < -length)
		return -length;
	else if (value >= length)
		return length-1;
	else return value;
}

//----------------------------

void S_decoder::mbgmc(MACROBLOCK *pMB, dword x_pos, dword y_pos, dword fcode, dword cbp, Bitstream * bs, bool rounding) {

	const dword stride = edged_width;
	const dword stride2 = stride / 2;

	byte *const pY_Cur=cur.y + (y_pos << 4) * stride + (x_pos << 4);
	byte *const pU_Cur=cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	byte *const pV_Cur=cur.v + (y_pos << 3) * stride2 + (x_pos << 3);

	NEW_GMC_DATA *gmc_data = &new_gmc_data;

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

	//this is where the calculations are done
	gmc_data->predict_16x16(gmc_data, cur.y + y_pos*16*stride + x_pos*16, refn[0].y, stride, stride, x_pos, y_pos, rounding);
	gmc_data->predict_8x8(gmc_data, cur.u + y_pos*8*stride2 + x_pos*8, refn[0].u, cur.v + y_pos*8*stride2 + x_pos*8, refn[0].v, stride2, stride2, x_pos, y_pos, rounding);
	gmc_data->get_average_mv(gmc_data, &pMB->amv, x_pos, y_pos, quarterpel);

	pMB->amv.x = gmc_sanitize(pMB->amv.x, quarterpel, fcode);
	pMB->amv.y = gmc_sanitize(pMB->amv.y, quarterpel, fcode);

	pMB->mvs[0] = pMB->mvs[1] = pMB->mvs[2] = pMB->mvs[3] = pMB->amv;

	if(cbp)
		mb_decode(cbp, bs, pY_Cur, pU_Cur, pV_Cur, 0, pMB);
}

//----------------------------

void S_decoder::I_Frame(Bitstream * bs, bool reduced_resolution, int quant, int intra_dc_threshold) {

	dword bound;
	dword x, y;
	//dword mb_width = mb_width;
	//dword mb_height = mb_height;

	if (reduced_resolution) {
		mb_width = (width + 31) / 32;
		mb_height = (height + 31) / 32;
	}

	bound = 0;

	for (y = 0; y < mb_height; y++) {
		for (x = 0; x < mb_width; x++) {
			dword mcbpc;
			dword cbpc;
			dword acpred_flag;
			dword cbpy;
			dword cbp;

			while(bs->ShowBits(9) == 1)
				bs->Skip(9);

			if(bs->check_resync_marker(0)) {
				bound = read_video_packet_header(bs, 0, &quant, NULL, NULL, &intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
			}
			MACROBLOCK *mb = &mbs[y * mb_width + x];

			//DPRINTF(XVID_DEBUG_MB, "macroblock (%i,%i) %08x\n", x, y, bs->ShowBits(32));

			mcbpc = bs->get_mcbpc_intra();
			mb->mode = mcbpc & 7;
			cbpc = (mcbpc >> 4);

			acpred_flag = bs->GetBit();

			cbpy = bs->GetCbpy(1);
			cbp = (cbpy << 2) | cbpc;

			if (mb->mode == MODE_INTRA_Q) {
				quant += dquant_table[bs->GetBits(2)];
				if (quant > 31) {
					quant = 31;
				} else if (quant < 1) {
					quant = 1;
				}
			}
			mb->quant = quant;
			mb->mvs[0].x = mb->mvs[0].y =
			                   mb->mvs[1].x = mb->mvs[1].y =
			                                      mb->mvs[2].x = mb->mvs[2].y =
			                                              mb->mvs[3].x = mb->mvs[3].y =0;

			if(interlacing) {
				mb->field_dct = bs->GetBit();
				DPRINTF(XVID_DEBUG_MB,"deci: field_dct: %i\n", mb->field_dct);
			}
			MBIntra(mb, x, y, acpred_flag, cbp, bs, quant, intra_dc_threshold, bound, reduced_resolution);
		}
		/*
		#ifdef XVID_CSP_SLICE
		if(out_frm)
		   output_slice(&cur, edged_width,width,out_frm,0,y,mb_width);
		#endif
		   */
	}
}

//----------------------------

void S_decoder::GetMotionVector(Bitstream * bs, int x, int y, int k, VECTOR *ret_mv, int fcode, int bound) {

	const int scale_fac = 1 << (fcode - 1);
	const int high = (32 * scale_fac) - 1;
	const int low = ((-32) * scale_fac);
	const int range = (64 * scale_fac);

	const VECTOR pmv = get_pmv2(mbs, mb_width, bound, x, y, k);
	VECTOR mv;

	mv.x = bs->GetMoveVector(fcode);
	mv.y = bs->GetMoveVector(fcode);

	//DPRINTF(XVID_DEBUG_MV,"mv_diff (%i,%i) pred (%i,%i) result (%i,%i)\n", mv.x, mv.y, pmv.x, pmv.y, mv.x+pmv.x, mv.y+pmv.y);

	mv.x += pmv.x;
	mv.y += pmv.y;

	if(mv.x < low) {
		mv.x += range;
	} else if(mv.x > high) {
		mv.x -= range;
	}

	if(mv.y < low) {
		mv.y += range;
	} else if(mv.y > high) {
		mv.y -= range;
	}

	ret_mv->x = mv.x;
	ret_mv->y = mv.y;
}

//----------------------------
// for P_VOP set gmc_warp to NULL
void S_decoder::P_Frame(Bitstream *bs, int rounding, bool reduced_resolution, int quant, int fcode,
                        int intra_dc_threshold, const WARPPOINTS *gmc_warp) {

	//dword mb_width = mb_width;
	//dword mb_height = mb_height;

	if(reduced_resolution) {
		mb_width = (width + 31) / 32;
		mb_height = (height + 31) / 32;
	}

	SetEdges(refn[0]);

	if(gmc_warp) {
		//accuracy: 0==1/2, 1=1/4, 2=1/8, 3=1/16
		generate_GMCparameters(sprite_warping_points, sprite_warping_accuracy, gmc_warp, width, height, &new_gmc_data);
		//image warping is done block-based in decoder_mbgmc(), now
	}

	PROF_S(PROF_FRM_P);
	dword bound = 0;

	for(dword y = 0; y < mb_height; y++) {
		//int cp_mb = 0, st_mb = 0;
		for(dword x = 0; x < mb_width; x++) {
			//skip stuffing
			while(bs->ShowBits(10) == 1)
				bs->Skip(10);

			if(bs->check_resync_marker(fcode - 1)) {
				bound = read_video_packet_header(bs, fcode - 1, &quant, &fcode, NULL, &intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
			}
			MACROBLOCK *mb = &mbs[y * mb_width + x];

			//DPRINTF(XVID_DEBUG_MB, "macroblock (%i,%i) %08x\n", x, y, bs->ShowBits(32));

			if(!(bs->GetBit())) {
				//block _is_ coded
				int mcsel = 0;    //mcsel: '0'=local motion, '1'=GMC

				//cp_mb++;
				dword mcbpc = bs->GetMcbpcInter();
				mb->mode = mcbpc & 7;
				dword cbpc = (mcbpc >> 4);

				DPRINTF(XVID_DEBUG_MB, "mode %i\n", mb->mode);
				DPRINTF(XVID_DEBUG_MB, "cbpc %i\n", cbpc);

				dword intra = (mb->mode == MODE_INTRA || mb->mode == MODE_INTRA_Q);

				dword acpred_flag = 0;
				if(gmc_warp && (mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q))
					mcsel = bs->GetBit();
				else if(intra)
					acpred_flag = bs->GetBit();

				dword cbpy = bs->GetCbpy(intra);
				//DPRINTF(XVID_DEBUG_MB, "cbpy %i mcsel %i \n", cbpy,mcsel);

				dword cbp = (cbpy << 2) | cbpc;

				if(mb->mode == MODE_INTER_Q || mb->mode == MODE_INTRA_Q) {
					int dquant = dquant_table[bs->GetBits(2)];
					DPRINTF(XVID_DEBUG_MB, "dquant %i\n", dquant);
					quant += dquant;
					if (quant > 31) {
						quant = 31;
					} else if (quant < 1) {
						quant = 1;
					}
					DPRINTF(XVID_DEBUG_MB, "quant %i\n", quant);
				}
				mb->quant = quant;

				if(interlacing) {
					if((cbp || intra) && !mcsel) {
						mb->field_dct = bs->GetBit();
						DPRINTF(XVID_DEBUG_MB,"decp: field_dct: %i\n", mb->field_dct);
					}

					if(mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {
						mb->field_pred = bs->GetBit();
						DPRINTF(XVID_DEBUG_MB, "decp: field_pred: %i\n", mb->field_pred);

						if(mb->field_pred) {
							mb->field_for_top = bs->GetBit();
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_top: %i\n", mb->field_for_top);
							mb->field_for_bot = bs->GetBit();
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_bot: %i\n", mb->field_for_bot);
						}
					}
				}
				if(mcsel) {
					mbgmc(mb, x, y, fcode, cbp, bs, rounding);
					PROF_E(PROF_0);
					continue;

				} else if(mb->mode == MODE_INTER || mb->mode == MODE_INTER_Q) {
					if(interlacing && mb->field_pred) {
						GetMotionVector(bs, x, y, 0, &mb->mvs[0], fcode, bound);
						GetMotionVector(bs, x, y, 0, &mb->mvs[1], fcode, bound);
					} else {
						GetMotionVector(bs, x, y, 0, &mb->mvs[0], fcode, bound);
						mb->mvs[1] = mb->mvs[2] = mb->mvs[3] = mb->mvs[0];
					}
				} else if(mb->mode == MODE_INTER4V) {
					GetMotionVector(bs, x, y, 0, &mb->mvs[0], fcode, bound);
					GetMotionVector(bs, x, y, 1, &mb->mvs[1], fcode, bound);
					GetMotionVector(bs, x, y, 2, &mb->mvs[2], fcode, bound);
					GetMotionVector(bs, x, y, 3, &mb->mvs[3], fcode, bound);
				} else {   /* MODE_INTRA, MODE_INTRA_Q */
					mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = 0;
					mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = 0;
					MBIntra(mb, x, y, acpred_flag, cbp, bs, quant, intra_dc_threshold, bound, reduced_resolution);
					continue;
				}
				PROF_S(PROF_5);
				DecodeInterMacroBlock(mb, x, y, cbp, bs, rounding, reduced_resolution, 0);
				PROF_E(PROF_5);
			} else if(gmc_warp) {
				//a not coded S(GMC)-VOP macroblock
				mb->mode = MODE_NOT_CODED_GMC;
				mbgmc(mb, x, y, fcode, 0x00, bs, rounding);

				/*
				#ifdef XVID_CSP_SLICE
				if(out_frm && cp_mb > 0) {
				   output_slice(&cur, edged_width,width,out_frm,st_mb,y,cp_mb);
				   cp_mb = 0;
				}
				st_mb = x+1;
				#endif
				*/
			} else {
				//not coded P_VOP macroblock
				mb->mode = MODE_NOT_CODED;

				mb->mvs[0].x = mb->mvs[1].x = mb->mvs[2].x = mb->mvs[3].x = 0;
				mb->mvs[0].y = mb->mvs[1].y = mb->mvs[2].y = mb->mvs[3].y = 0;

				DecodeInterMacroBlock(mb, x, y, 0, bs, rounding, reduced_resolution, 0);
				/*
				#ifdef XVID_CSP_SLICE
				if(out_frm && cp_mb > 0) {
				   output_slice(&cur, edged_width,width,out_frm,st_mb,y,cp_mb);
				   cp_mb = 0;
				}
				st_mb = x+1;
				#endif
				*/
			}
		}
		/*
		#ifdef XVID_CSP_SLICE
		if(out_frm && cp_mb > 0)
		   output_slice(&cur, edged_width,width,out_frm,st_mb,y,cp_mb);
		#endif
		   */
	}
	PROF_E(PROF_FRM_P);
}

//----------------------------
/* decode B-frame motion vector */
static void get_b_motion_vector(Bitstream * bs, VECTOR * mv, int fcode, const VECTOR pmv) {

	const int scale_fac = 1 << (fcode - 1);
	const int high = (32 * scale_fac) - 1;
	const int low = ((-32) * scale_fac);
	const int range = (64 * scale_fac);

	int mv_x = bs->GetMoveVector(fcode);
	int mv_y = bs->GetMoveVector(fcode);

	mv_x += pmv.x;
	mv_y += pmv.y;

	if (mv_x < low)
		mv_x += range;
	else if (mv_x > high)
		mv_x -= range;

	if (mv_y < low)
		mv_y += range;
	else if (mv_y > high)
		mv_y -= range;

	mv->x = mv_x;
	mv->y = mv_y;
}

//----------------------------
/* decode an B-frame direct & interpolate macroblock */
void S_decoder::BFrameInterpolateMBInter(const IMAGE &forward, const IMAGE &backward, const MACROBLOCK *pMB, dword x_pos, dword y_pos, Bitstream *bs, int direct) {

	PROF_S(PROF_BINT_MBI);
	dword stride = edged_width;
	dword stride2 = stride / 2;
	int uv_dx, uv_dy;
	int b_uv_dx, b_uv_dy;
	byte *pY_Cur, *pU_Cur, *pV_Cur;
	const dword cbp = pMB->cbp;

	pY_Cur = cur.y + (y_pos << 4) * stride + (x_pos << 4);
	pU_Cur = cur.u + (y_pos << 3) * stride2 + (x_pos << 3);
	pV_Cur = cur.v + (y_pos << 3) * stride2 + (x_pos << 3);

	if(!direct) {
		uv_dx = pMB->mvs[0].x;
		uv_dy = pMB->mvs[0].y;

		b_uv_dx = pMB->b_mvs[0].x;
		b_uv_dy = pMB->b_mvs[0].y;

		if(quarterpel) {
			uv_dx /= 2;
			uv_dy /= 2;
			b_uv_dx /= 2;
			b_uv_dy /= 2;
		}
		uv_dx = (uv_dx >> 1) + roundtab_79[uv_dx & 0x3];
		uv_dy = (uv_dy >> 1) + roundtab_79[uv_dy & 0x3];

		b_uv_dx = (b_uv_dx >> 1) + roundtab_79[b_uv_dx & 0x3];
		b_uv_dy = (b_uv_dy >> 1) + roundtab_79[b_uv_dy & 0x3];
	} else {
		if(quarterpel) {
			uv_dx = (pMB->mvs[0].x / 2) + (pMB->mvs[1].x / 2) + (pMB->mvs[2].x / 2) + (pMB->mvs[3].x / 2);
			uv_dy = (pMB->mvs[0].y / 2) + (pMB->mvs[1].y / 2) + (pMB->mvs[2].y / 2) + (pMB->mvs[3].y / 2);
			b_uv_dx = (pMB->b_mvs[0].x / 2) + (pMB->b_mvs[1].x / 2) + (pMB->b_mvs[2].x / 2) + (pMB->b_mvs[3].x / 2);
			b_uv_dy = (pMB->b_mvs[0].y / 2) + (pMB->b_mvs[1].y / 2) + (pMB->b_mvs[2].y / 2) + (pMB->b_mvs[3].y / 2);
		} else {
			uv_dx = pMB->mvs[0].x + pMB->mvs[1].x + pMB->mvs[2].x + pMB->mvs[3].x;
			uv_dy = pMB->mvs[0].y + pMB->mvs[1].y + pMB->mvs[2].y + pMB->mvs[3].y;
			b_uv_dx = pMB->b_mvs[0].x + pMB->b_mvs[1].x + pMB->b_mvs[2].x + pMB->b_mvs[3].x;
			b_uv_dy = pMB->b_mvs[0].y + pMB->b_mvs[1].y + pMB->b_mvs[2].y + pMB->b_mvs[3].y;
		}

		uv_dx = (uv_dx >> 3) + roundtab_76[uv_dx & 0xf];
		uv_dy = (uv_dy >> 3) + roundtab_76[uv_dy & 0xf];
		b_uv_dx = (b_uv_dx >> 3) + roundtab_76[b_uv_dx & 0xf];
		b_uv_dy = (b_uv_dy >> 3) + roundtab_76[b_uv_dy & 0xf];
	}

	if(quarterpel) {
		if(!direct) {
			interpolate16x16_quarterpel(cur.y, forward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		} else {
			interpolate8x8_quarterpel(cur.y, forward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
			interpolate8x8_quarterpel(cur.y, forward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos, pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
			interpolate8x8_quarterpel(cur.y, forward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos + 8, pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
			interpolate8x8_quarterpel(cur.y, forward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8, pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
		}
	} else {
		interpolate8x8_switch(cur.y, forward.y, 16 * x_pos, 16 * y_pos, pMB->mvs[0].x, pMB->mvs[0].y, stride, 0);
		interpolate8x8_switch(cur.y, forward.y, 16 * x_pos + 8, 16 * y_pos, pMB->mvs[1].x, pMB->mvs[1].y, stride, 0);
		interpolate8x8_switch(cur.y, forward.y, 16 * x_pos, 16 * y_pos + 8, pMB->mvs[2].x, pMB->mvs[2].y, stride, 0);
		interpolate8x8_switch(cur.y, forward.y, 16 * x_pos + 8, 16 * y_pos + 8, pMB->mvs[3].x, pMB->mvs[3].y, stride, 0);
	}

	interpolate8x8_switch(cur.u, forward.u, 8 * x_pos, 8 * y_pos, uv_dx, uv_dy, stride2, 0);
	interpolate8x8_switch(cur.v, forward.v, 8 * x_pos, 8 * y_pos, uv_dx, uv_dy, stride2, 0);

	if(quarterpel) {
		if(!direct) {
			interpolate16x16_quarterpel(tmp.y, backward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		} else {
			interpolate8x8_quarterpel(tmp.y, backward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos, pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
			interpolate8x8_quarterpel(tmp.y, backward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos, pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride, 0);
			interpolate8x8_quarterpel(tmp.y, backward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos, 16*y_pos + 8, pMB->b_mvs[2].x, pMB->b_mvs[2].y, stride, 0);
			interpolate8x8_quarterpel(tmp.y, backward.y, qtmp.y, qtmp.y + 64, qtmp.y + 128, 16*x_pos + 8, 16*y_pos + 8, pMB->b_mvs[3].x, pMB->b_mvs[3].y, stride, 0);
		}
	} else {
		interpolate8x8_switch(tmp.y, backward.y, 16 * x_pos, 16 * y_pos, pMB->b_mvs[0].x, pMB->b_mvs[0].y, stride, 0);
		interpolate8x8_switch(tmp.y, backward.y, 16 * x_pos + 8, 16 * y_pos, pMB->b_mvs[1].x, pMB->b_mvs[1].y, stride, 0);
		interpolate8x8_switch(tmp.y, backward.y, 16 * x_pos, 16 * y_pos + 8, pMB->b_mvs[2].x, pMB->b_mvs[2].y, stride, 0);
		interpolate8x8_switch(tmp.y, backward.y, 16 * x_pos + 8, 16 * y_pos + 8, pMB->b_mvs[3].x, pMB->b_mvs[3].y, stride, 0);
	}

	interpolate8x8_switch(tmp.u, backward.u, 8 * x_pos, 8 * y_pos, b_uv_dx, b_uv_dy, stride2, 0);
	interpolate8x8_switch(tmp.v, backward.v, 8 * x_pos, 8 * y_pos, b_uv_dx, b_uv_dy, stride2, 0);

	interpolate8x8_avg2(cur.y + (16 * y_pos * stride) + 16 * x_pos, cur.y + (16 * y_pos * stride) + 16 * x_pos,
	                    tmp.y + (16 * y_pos * stride) + 16 * x_pos, stride, 1, 8);

	interpolate8x8_avg2(cur.y + (16 * y_pos * stride) + 16 * x_pos + 8, cur.y + (16 * y_pos * stride) + 16 * x_pos + 8,
	                    tmp.y + (16 * y_pos * stride) + 16 * x_pos + 8, stride, 1, 8);

	interpolate8x8_avg2(cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos, cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos,
	                    tmp.y + ((16 * y_pos + 8) * stride) + 16 * x_pos, stride, 1, 8);

	interpolate8x8_avg2(cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8, cur.y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8,
	                    tmp.y + ((16 * y_pos + 8) * stride) + 16 * x_pos + 8, stride, 1, 8);

	interpolate8x8_avg2(cur.u + (8 * y_pos * stride2) + 8 * x_pos, cur.u + (8 * y_pos * stride2) + 8 * x_pos,
	                    tmp.u + (8 * y_pos * stride2) + 8 * x_pos, stride2, 1, 8);

	interpolate8x8_avg2(cur.v + (8 * y_pos * stride2) + 8 * x_pos, cur.v + (8 * y_pos * stride2) + 8 * x_pos,
	                    tmp.v + (8 * y_pos * stride2) + 8 * x_pos, stride2, 1, 8);

	if(cbp)
		mb_decode(cbp, bs, pY_Cur, pU_Cur, pV_Cur, 0, pMB);
	PROF_E(PROF_BINT_MBI);
}

//----------------------------
/* for decode B-frame dbquant */
static int get_dbquant(Bitstream * bs) {
	if (!bs->GetBit())     /*  '0' */
		return (0);
	else if (!bs->GetBit())   /* '10' */
		return (-2);
	else                    /* '11' */
		return (2);
}

//----------------------------
/*
 * decode B-frame mb_type
 * bit      ret_value
 * 1     0
 * 01    1
 * 001      2
 * 0001     3
 */
static int get_mbtype(Bitstream * bs) {
	int mb_type;

	for (mb_type = 0; mb_type <= 3; mb_type++)
		if (bs->GetBit())
			return (mb_type);

	return -1;
}

//----------------------------

void S_decoder::B_Frame(Bitstream *bs, int quant, int fcode_forward, int fcode_backward) {

	dword x, y;
	VECTOR mv;
	const VECTOR zeromv = {0,0};
	const int64_t TRB = time_pp - time_bp, TRD = time_pp;
	int i;

	PROF_S(PROF_FRM_B);
	SetEdges(refn[0]);
	SetEdges(refn[1]);

	for(y = 0; y < mb_height; y++) {
		//initialize Pred Motion Vector
		p_fmv = p_bmv = zeromv;
		for(x = 0; x < mb_width; x++) {
			MACROBLOCK *mb = &mbs[y * mb_width + x];
			MACROBLOCK *last_mb = &last_mbs[y * mb_width + x];
			const int fcode_max = (fcode_forward>fcode_backward) ? fcode_forward : fcode_backward;

			if(bs->check_resync_marker(fcode_max  - 1)) {
				dword intra_dc_threshold; /* fake variable */
				int bound = read_video_packet_header(bs, fcode_max - 1, &quant, &fcode_forward, &fcode_backward, (int*)&intra_dc_threshold);
				x = bound % mb_width;
				y = bound / mb_width;
				//reset predicted macroblocks
				p_fmv = p_bmv = zeromv;
			}

			mv = mb->b_mvs[0] = mb->b_mvs[1] = mb->b_mvs[2] = mb->b_mvs[3] = mb->mvs[0] = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] = zeromv;
			mb->quant = quant;

			/*
			 * skip if the co-located P_VOP macroblock is not coded
			 * if not codec in co-located S_VOP macroblock is _not_
			 * automatically skipped
			 */

			if(last_mb->mode == MODE_NOT_CODED) {
				mb->cbp = 0;
				mb->mode = MODE_FORWARD;
				DecodeInterMacroBlock(mb, x, y, mb->cbp, bs, 0, 0, 1);
				continue;
			}

			if(!bs->GetBit()) {
				//modb=='0'
				const byte modb2 = bs->GetBit();

				mb->mode = get_mbtype(bs);

				if (!modb2)    /* modb=='00' */
					mb->cbp = bs->GetBits(6);
				else
					mb->cbp = 0;

				if (mb->mode && mb->cbp) {
					quant += get_dbquant(bs);
					if (quant > 31)
						quant = 31;
					else if (quant < 1)
						quant = 1;
				}
				mb->quant = quant;

				if(interlacing) {
					if (mb->cbp) {
						mb->field_dct = bs->GetBit();
						DPRINTF(XVID_DEBUG_MB,"decp: field_dct: %i\n", mb->field_dct);
					}

					if (mb->mode) {
						mb->field_pred = bs->GetBit();
						DPRINTF(XVID_DEBUG_MB, "decp: field_pred: %i\n", mb->field_pred);

						if (mb->field_pred) {
							mb->field_for_top = bs->GetBit();
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_top: %i\n", mb->field_for_top);
							mb->field_for_bot = bs->GetBit();
							DPRINTF(XVID_DEBUG_MB,"decp: field_for_bot: %i\n", mb->field_for_bot);
						}
					}
				}

			} else {
				mb->mode = MODE_DIRECT_NONE_MV;
				mb->cbp = 0;
			}

			switch(mb->mode) {
			case MODE_DIRECT:
				get_b_motion_vector(bs, &mv, 1, zeromv);
			//flow...
			case MODE_DIRECT_NONE_MV:
				for(i=0; i<4; i++) {
					mb->mvs[i].x = (int)((int(TRB) * last_mb->mvs[i].x) / (int)TRD + mv.x);
					mb->b_mvs[i].x = (int) ((mv.x == 0) ?
					                        (int(TRB - TRD) * last_mb->mvs[i].x) / (int)TRD :
					                        mb->mvs[i].x - last_mb->mvs[i].x);
					mb->mvs[i].y = (int) ((int(TRB) * last_mb->mvs[i].y) / (int)TRD + mv.y);
					mb->b_mvs[i].y = (int) ((mv.y == 0) ?
					                        (int(TRB - TRD) * last_mb->mvs[i].y) / int(TRD) :
					                        int(mb->mvs[i].y - last_mb->mvs[i].y));
				}
				BFrameInterpolateMBInter(refn[1], refn[0], mb, x, y, bs, 1);
				break;

			case MODE_INTERPOLATE:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_forward, p_fmv);
				p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =   mb->mvs[0];

				get_b_motion_vector(bs, &mb->b_mvs[0], fcode_backward, p_bmv);
				p_bmv = mb->b_mvs[1] = mb->b_mvs[2] = mb->b_mvs[3] = mb->b_mvs[0];

				BFrameInterpolateMBInter(refn[1], refn[0], mb, x, y, bs, 0);
				break;

			case MODE_BACKWARD:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_backward, p_bmv);
				p_bmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =   mb->mvs[0];

				DecodeInterMacroBlock(mb, x, y, mb->cbp, bs, 0, 0, 0);
				break;

			case MODE_FORWARD:
				get_b_motion_vector(bs, &mb->mvs[0], fcode_forward, p_fmv);
				p_fmv = mb->mvs[1] = mb->mvs[2] = mb->mvs[3] =   mb->mvs[0];

				DecodeInterMacroBlock(mb, x, y, mb->cbp, bs, 0, 0, 1);
				break;

			default:
				DPRINTF(XVID_DEBUG_ERROR,"Not supported B-frame mb_type = %i\n", mb->mode);
			}
		}
	}
	PROF_E(PROF_FRM_B);
}

//----------------------------
/* perform post processing if necessary, and output the image */
/*
void S_decoder::Output(IMAGE *img, xvid_dec_frame_t *frame, xvid_dec_stats_t *stats, int coding_type){

   //image_output(img, width, height, edged_width, (byte**)frame->output.plane, (unsigned int*)frame->output.stride, frame->output.csp, interlacing);
   yv12_to_bgr((byte*)frame->output.plane, frame->output.stride, img->y, img->u, img->v, edged_width, edged_width/2, width, height, false);

   if(stats){
      stats->type = coding2type(coding_type);
      stats->data.vop.time_base = (int)time_base;
      stats->data.vop.time_increment = 0; //XXX: todo
   }
}
*/

//----------------------------

int S_decoder::Decode(xvid_dec_frame_t *frame, xvid_dec_stats_t *stats) {

	PROF_S(PROF_DECODE);

	Bitstream bs;
	bool rounding;
	bool reduced_resolution;
	dword quant;
	dword fcode_forward;
	dword fcode_backward;
	dword intra_dc_threshold;
	WARPPOINTS gmc_warp;
	int coding_type;

	frame->img_out = NULL;

	if(XVID_VERSION_MAJOR(frame->version) != 1 || (stats && XVID_VERSION_MAJOR(stats->version) != 1)) { //v1.x.x
		PROF_E(PROF_DECODE);
		return XVID_ERR_VERSION;
	}

	//start_global_timer();

	low_delay_default = (frame->general & XVID_LOWDELAY);
	if((frame->general & XVID_DISCONTINUITY))
		frames = 0;
	/*
	#ifdef XVID_CSP_SLICE
	out_frm = (frame->output.csp == XVID_CSP_SLICE) ? &frame->output : NULL;
	#endif
	*/

	if(frame->length < 0) {
		//decoder flush
		int ret;
		//if not decoding "low_delay/packed", and this isn't low_delay and
		// we have a reference frame, then outout the reference frame
		if(!(low_delay_default && packed_mode) && !low_delay && frames>0) {
			//Output(&refn[0], frame, stats, last_coding_type);
			frame->img_out = &refn[0];
			frames = 0;
			ret = 0;
		} else {
			if (stats) stats->type = XVID_TYPE_NOTHING;
			ret = XVID_ERR_END;
		}
		//stop_global_timer();
		PROF_E(PROF_DECODE);
		return ret;
	}

	bs.Init(frame->bitstream, frame->length);

	//XXX: 0x7f is only valid whilst decoding vfw xvid/divx5 avi's
	if(low_delay_default && frame->length == 1 && bs.ShowBits(8) == 0x7f) {
		//image_output(&refn[0], width, height, edged_width, (byte**)frame->output.plane, (unsigned int*)frame->output.stride, frame->output.csp, interlacing);
		if(stats)
			stats->type = XVID_TYPE_NOTHING;
		PROF_E(PROF_DECODE);
		return 1;               //one byte consumed
	}

	bool success = false;
	bool output = false;
	bool seen_something = false;

	/*
	cur.Clear(width, height, edged_width, 0, 0, 0);
	refn[0].Clear(width, height, edged_width, 0, 0, 0);
	refn[1].Clear(width, height, edged_width, 0, 0, 0);
	*/

repeat:

	coding_type = BitstreamReadHeaders(&bs, rounding, &reduced_resolution, &quant, &fcode_forward, &fcode_backward, &intra_dc_threshold, &gmc_warp);

	//DPRINTF(XVID_DEBUG_HEADER, "coding_type=%i, packed=%i, time=%lli, time_pp=%i, time_bp=%i\n", coding_type, packed_mode, time, time_pp, time_bp);

	if(coding_type == -1) {
		//nothing
		if(success)
			goto done;
		if(stats)
			stats->type = XVID_TYPE_NOTHING;
		PROF_E(PROF_DECODE);
		return bs.Pos()/8;
	}

	if(coding_type == -2 || coding_type == -3) {
		//vol and/or resize

		if(coding_type == -3)
			Resize();
		/*
		if(stats){
		   stats->type = XVID_TYPE_VOL;
		   stats->data.vol.general = 0;
		   //XXX: if (interlacing) stats->data.vol.general |= ++INTERLACING;
		   stats->data.vol.width = width;
		   stats->data.vol.height = height;
		   stats->data.vol.par = aspect_ratio;
		   stats->data.vol.par_width = par_width;
		   stats->data.vol.par_height = par_height;
		   return bs.Pos()/8;   //number of bytes consumed
		}
		*/
		goto repeat;
	}
	p_bmv.x = p_bmv.y = p_fmv.y = p_fmv.y = 0;

	//packed_mode: special-N_VOP treament
	if(packed_mode && coding_type == N_VOP) {
		if(low_delay_default && frames > 0) {
			//Output(&refn[0], frame, stats, last_coding_type);
			frame->img_out = &refn[0];
			output = true;
		}
		//ignore otherwise
	} else if(coding_type != B_VOP) {
		switch(coding_type) {
		case I_VOP:
			PROF_S(PROF_FRM_I);
			I_Frame(&bs, reduced_resolution, quant, intra_dc_threshold);
			PROF_E(PROF_FRM_I);
			break;
		case P_VOP:
			P_Frame(&bs, rounding, reduced_resolution, quant, fcode_forward, intra_dc_threshold, NULL);
			break;
		case S_VOP:
			//not called (or very rare)
			P_Frame(&bs, rounding, reduced_resolution, quant, fcode_forward, intra_dc_threshold, &gmc_warp);
			break;
		case N_VOP:
			//XXX: not_coded vops are not used for forward prediction we should not swap(last_mbs,mbs)
			cur.Copy(&refn[0], edged_width, height);
			break;
		}
		if(reduced_resolution)
			cur.deblock_rrv(edged_width, mbs, (width + 31) / 32, (height + 31) / 32, mb_width, 16, 0);

		//note: for packed_mode, output is performed when the special-N_VOP is decoded
		if(!(low_delay_default && packed_mode)) {
			if(low_delay) {
				//Output(&cur, frame, stats, coding_type);
				frame->img_out = &cur;
				output = true;
			} else if(frames > 0) {
				//is the reference frame valid?
				// output the reference frame
				//Output(&refn[0], frame, stats, last_coding_type);
				frame->img_out = &refn[1];
				output = true;
			}
		}
		refn[0].Swap(&refn[1]);
		cur.Swap(&refn[0]);
		Swap(mbs, last_mbs);
		last_reduced_resolution = reduced_resolution;
		last_coding_type = coding_type;

		frames++;
		seen_something = true;
	} else {
		//B_VOP
		if(low_delay) {
			DPRINTF(XVID_DEBUG_ERROR, "warning: bvop found in low_delay==1 stream\n");
			low_delay = true;
		}

		if(frames < 2) {
			/* attemping to decode a bvop without atleast 2 reference frames */
			cur.Print(edged_width, height, 16, 16, "broken b-frame, mising ref frames");
		} else if(time_pp <= time_bp) {
			//this occurs when dx50_bvop_compatibility==0 sequences are decoded in vfw
			cur.Print(edged_width, height, 16, 16, "broken b-frame");
		} else {
			B_Frame(&bs, quant, fcode_forward, fcode_backward);
		}
		//Output(&cur, frame, stats, coding_type);
		frame->img_out = &cur;
		output = true;
		frames++;
	}

	bs.ByteAlign();

	//low_delay_default mode: repeat in packed_mode
	if(low_delay_default && packed_mode && !output && !success) {
		success = true;
		goto repeat;
	}
done:
	//low_delay_default mode: if we've gotten here without outputting anything,
	// then output the recently decoded frame, or print an error message
	if(low_delay_default && !output) {
		if(packed_mode && seen_something) {
			//output the recently decoded frame
			//Output(&refn[0], frame, stats, last_coding_type);
			frame->img_out = &refn[0];
		} else {
			cur.Clear(width, height, edged_width, 0, 128, 128);
			cur.Print(edged_width, height, 16, 16, "warning: nothing to output");
			cur.Print(edged_width, height, 16, 64, "bframe decoder lag");

			//Output(&cur, frame, stats, P_VOP);
			frame->img_out = &cur;
			if(stats)
				stats->type = XVID_TYPE_NOTHING;
		}
	}

	PROF_E(PROF_DECODE);
	//number of bytes consumed
	return bs.Pos() / 8;
}

//----------------------------

