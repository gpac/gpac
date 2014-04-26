/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Prediction module -
 *
 *  Copyright (C) 2001-2003 Michael Militzer <isibaar@xvid.org>
 *                2001-2003 Peter Ross <pross@xvid.org>
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
 * $Id: mbprediction.cpp,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/

#include "global.h"
#include "mbprediction.h"

//----------------------------

#if 0

#define Divide(a,b)    (((a)>0) ? ((a)+((b)>>1))/(b) : ((a)-((b)>>1))/(b))

#else

#define RET_DIV(a, N) return (a*N+32768) >> 16;
#define RET_SHR(a, N, S) return (a+N) >> S;

static int Divide(int a, int b) {
	switch(b) {
	case 1:
		return a;
	case 2:
		return a >> 1;
	case 4:
		RET_SHR(a, 2, 2);
	case 8:
		RET_SHR(a, 4, 3);
	case 16:
		RET_SHR(a, 8, 4);
	case 32:
		RET_SHR(a, 16, 5);
	case 3:
		RET_DIV(a, 21845);
	case 5:
		RET_DIV(a, 13107);
	case 6:
		RET_DIV(a, 10923);
	case 7:
		RET_DIV(a, 9362);
	case 9:
		RET_DIV(a, 7282);
	case 10:
		RET_DIV(a, 6554);
	case 11:
		RET_DIV(a, 5958);
	case 12:
		RET_DIV(a, 5461);
	case 13:
		RET_DIV(a, 5041);
	case 14:
		RET_DIV(a, 4681);
	case 15:
		RET_DIV(a, 4369);
	case 17:
		RET_DIV(a, 3855);
	case 18:
		RET_DIV(a, 3641);
	case 19:
		RET_DIV(a, 3449);
	case 20:
		RET_DIV(a, 3277);
	case 21:
		RET_DIV(a, 3121);
	case 22:
		RET_DIV(a, 2979);
	case 23:
		RET_DIV(a, 2849);
	case 24:
		RET_DIV(a, 2731);
	case 25:
		RET_DIV(a, 2621);
	case 26:
		RET_DIV(a, 2521);
	case 27:
		RET_DIV(a, 2427);
	case 28:
		RET_DIV(a, 2341);
	case 29:
		RET_DIV(a, 2260);
	case 30:
		RET_DIV(a, 2185);
	case 31:
		RET_DIV(a, 2114);
	}
	return ((a>0) ? (a+(b>>1))/b : (a-(b>>1))/b);
}
#endif

//----------------------------

inline int rescale(int predict_quant, int current_quant, int coeff) {

	if(!coeff)
		return 0;
	return Divide(coeff * predict_quant, current_quant);
}

//----------------------------

static const int default_acdc_values[15] = {
	1024,
	0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0
};

//----------------------------
/* get dc/ac prediction direction for a single block and place
   predictor values into MB->pred_values[j][..]
*/
void predict_acdc(MACROBLOCK *pMBs, dword x, dword y, dword mb_width, dword block, int qcoeff[64], dword current_quant, int iDcScaler, int predictors[8], int bound) {

	const int mbpos = (y * mb_width) + x;
	int *left, *top, *diag, *current;

	int left_quant = current_quant;
	int top_quant = current_quant;

	const int *pLeft = default_acdc_values;
	const int *pTop = default_acdc_values;
	const int *pDiag = default_acdc_values;

	dword index = x + y * mb_width;  /* current macroblock */
	int *acpred_direction = &pMBs[index].acpred_directions[block];
	dword i;

	left = top = diag = current = 0;

	/* grab left,top and diag macroblocks */

	/* left macroblock */

	if (x && mbpos >= bound + 1  &&
	        (pMBs[index - 1].mode == MODE_INTRA ||
	         pMBs[index - 1].mode == MODE_INTRA_Q)) {

		left = pMBs[index - 1].pred_values[0];
		left_quant = pMBs[index - 1].quant;
	}
	/* top macroblock */

	if (mbpos >= bound + (int)mb_width &&
	        (pMBs[index - mb_width].mode == MODE_INTRA ||
	         pMBs[index - mb_width].mode == MODE_INTRA_Q)) {

		top = pMBs[index - mb_width].pred_values[0];
		top_quant = pMBs[index - mb_width].quant;
	}
	/* diag macroblock */

	if (x && mbpos >= bound + (int)mb_width + 1 &&
	        (pMBs[index - 1 - mb_width].mode == MODE_INTRA ||
	         pMBs[index - 1 - mb_width].mode == MODE_INTRA_Q)) {

		diag = pMBs[index - 1 - mb_width].pred_values[0];
	}

	current = pMBs[index].pred_values[0];

	/* now grab pLeft, pTop, pDiag _blocks_ */

	switch(block) {
	case 0:
		if(left)
			pLeft = left + MBPRED_SIZE;
		if(top)
			pTop = top + (MBPRED_SIZE << 1);
		if(diag)
			pDiag = diag + 3 * MBPRED_SIZE;
		break;

	case 1:
		pLeft = current;
		left_quant = current_quant;
		if(top) {
			pTop = top + 3 * MBPRED_SIZE;
			pDiag = top + (MBPRED_SIZE << 1);
		}
		break;

	case 2:
		if(left) {
			pLeft = left + 3 * MBPRED_SIZE;
			pDiag = left + MBPRED_SIZE;
		}
		pTop = current;
		top_quant = current_quant;
		break;

	case 3:
		pLeft = current + (MBPRED_SIZE << 1);
		left_quant = current_quant;
		pTop = current + MBPRED_SIZE;
		top_quant = current_quant;
		pDiag = current;
		break;

	case 4:
		if(left)
			pLeft = left + (MBPRED_SIZE << 2);
		if(top)
			pTop = top + (MBPRED_SIZE << 2);
		if(diag)
			pDiag = diag + (MBPRED_SIZE << 2);
		break;

	case 5:
		if(left)
			pLeft = left + 5 * MBPRED_SIZE;
		if(top)
			pTop = top + 5 * MBPRED_SIZE;
		if(diag)
			pDiag = diag + 5 * MBPRED_SIZE;
		break;
	}

	/*
	 * determine ac prediction direction & ac/dc predictor place rescaled ac/dc
	 * predictions into predictors[] for later use
	 */

	if(ABS(pLeft[0] - pDiag[0]) < ABS(pDiag[0] - pTop[0])) {
		//vertical
		*acpred_direction = 1;
		predictors[0] = Divide(pTop[0], iDcScaler);
		for(i = 1; i < 8; i++)
			predictors[i] = rescale(top_quant, current_quant, pTop[i]);
	} else {
		//horizontal
		*acpred_direction = 2;
		predictors[0] = Divide(pLeft[0], iDcScaler);
		for(i = 1; i < 8; i++)
			predictors[i] = rescale(left_quant, current_quant, pLeft[i + 7]);
	}
}

//----------------------------
/* decoder: add predictors to dct_codes[] and
   store current coeffs to pred_values[] for future prediction
*/
void add_acdc(MACROBLOCK *pMB, dword block, int dct_codes[64], dword iDcScaler, int predictors[8]) {

	byte acpred_direction = pMB->acpred_directions[block];
	int *pCurrent = pMB->pred_values[block];

	DPRINTF(XVID_DEBUG_COEFF,"predictor[0] %i\n", predictors[0]);

	dct_codes[0] += predictors[0];   /* dc prediction */
	pCurrent[0] = dct_codes[0] * iDcScaler;

	if(acpred_direction == 1) {
		for(int i = 1; i < 8; i++) {
			int level = dct_codes[i] + predictors[i];
			//DPRINTF(XVID_DEBUG_COEFF,"predictor[%i] %i\n",i, predictors[i]);
			dct_codes[i] = level;
			pCurrent[i] = level;
			pCurrent[i + 7] = dct_codes[i * 8];
		}
	} else if(acpred_direction == 2) {
		for(int i = 1; i < 8; i++) {
			int level = dct_codes[i * 8] + predictors[i];
			//DPRINTF(XVID_DEBUG_COEFF,"predictor[%i] %i\n",i*8, predictors[i]);
			dct_codes[i * 8] = level;
			pCurrent[i + 7] = level;
			pCurrent[i] = dct_codes[i];
		}
	} else {
		for(int i = 1; i < 8; i++) {
			pCurrent[i] = dct_codes[i];
			pCurrent[i + 7] = dct_codes[i * 8];
		}
	}
}

//----------------------------

static const VECTOR zeroMV = { 0, 0 };

VECTOR get_pmv2(const MACROBLOCK * const mbs, const int mb_width, const int bound, const int x, const int y, const int block) {

	int lx, ly, lz;      /* left */
	int tx, ty, tz;      /* top */
	int rx, ry, rz;      /* top-right */
	int lpos, tpos, rpos;
	int num_cand = 0, last_cand = 1;

	VECTOR pmv[4]; /* left neighbour, top neighbour, top-right neighbour */

	switch(block) {
	case 0:
		lx = x - 1;
		ly = y;
		lz = 1;
		tx = x;
		ty = y - 1;
		tz = 2;
		rx = x + 1;
		ry = y - 1;
		rz = 2;
		break;
	case 1:
		lx = x;
		ly = y;
		lz = 0;
		tx = x;
		ty = y - 1;
		tz = 3;
		rx = x + 1;
		ry = y - 1;
		rz = 2;
		break;
	case 2:
		lx = x - 1;
		ly = y;
		lz = 3;
		tx = x;
		ty = y;
		tz = 0;
		rx = x;
		ry = y;
		rz = 1;
		break;
	default:
		lx = x;
		ly = y;
		lz = 2;
		tx = x;
		ty = y;
		tz = 0;
		rx = x;
		ry = y;
		rz = 1;
	}

	lpos = lx + ly * mb_width;
	rpos = rx + ry * mb_width;
	tpos = tx + ty * mb_width;

	if(lpos >= bound && lx >= 0) {
		num_cand++;
		pmv[1] = mbs[lpos].mvs[lz];
	} else
		pmv[1] = zeroMV;

	if(tpos >= bound) {
		num_cand++;
		last_cand = 2;
		pmv[2] = mbs[tpos].mvs[tz];
	} else
		pmv[2] = zeroMV;

	if(rpos >= bound && rx < mb_width) {
		num_cand++;
		last_cand = 3;
		pmv[3] = mbs[rpos].mvs[rz];
	} else
		pmv[3] = zeroMV;

	//if there're more than one candidate, we return the median vector
	if(num_cand > 1) {
		//set median
		pmv[0].x =
		    MIN(MAX(pmv[1].x, pmv[2].x),
		        MIN(MAX(pmv[2].x, pmv[3].x), MAX(pmv[1].x, pmv[3].x)));
		pmv[0].y =
		    MIN(MAX(pmv[1].y, pmv[2].y),
		        MIN(MAX(pmv[2].y, pmv[3].y), MAX(pmv[1].y, pmv[3].y)));
		return pmv[0];
	}

	return pmv[last_cand];  //no point calculating median mv
}

//----------------------------
