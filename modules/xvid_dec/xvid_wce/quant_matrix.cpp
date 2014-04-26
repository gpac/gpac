/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Quantization matrix management code  -
 *
 *  Copyright(C) 2002 Michael Militzer <isibaar@xvid.org>
 *               2002 Peter Ross <pross@xvid.org>
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
 * $Id: quant_matrix.cpp,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#include "quant_matrix.h"

#define FIX(X)   (((X)==1) ? 0xFFFF : ((1UL << 16) / (X) + 1))
#define FIXL(X)    ((1UL << 16) / (X) - 1)

/*****************************************************************************
 * Default matrices
 ****************************************************************************/

static const byte default_intra_matrix[64] = {
	8, 17, 18, 19, 21, 23, 25, 27,
	17, 18, 19, 21, 23, 25, 27, 28,
	20, 21, 22, 23, 24, 26, 28, 30,
	21, 22, 23, 24, 26, 28, 30, 32,
	22, 23, 24, 26, 28, 30, 32, 35,
	23, 24, 26, 28, 30, 32, 35, 38,
	25, 26, 28, 30, 32, 35, 38, 41,
	27, 28, 30, 32, 35, 38, 41, 45
};

//----------------------------

static const byte default_inter_matrix[64] = {
	16, 17, 18, 19, 20, 21, 22, 23,
	17, 18, 19, 20, 21, 22, 23, 24,
	18, 19, 20, 21, 22, 23, 24, 25,
	19, 20, 21, 22, 23, 24, 26, 27,
	20, 21, 22, 23, 25, 26, 27, 28,
	21, 22, 23, 24, 26, 27, 28, 30,
	22, 23, 24, 26, 27, 28, 30, 31,
	23, 24, 25, 27, 28, 30, 31, 33
};

//----------------------------

const byte *get_default_intra_matrix() {
	return default_intra_matrix;
}

//----------------------------

const byte *get_default_inter_matrix() {
	return default_inter_matrix;
}

//----------------------------

void set_intra_matrix(dword *mpeg_quant_matrices, const byte *matrix) {

	dword *intra_matrix = mpeg_quant_matrices + 0*64;
	dword *intra_matrix1 = mpeg_quant_matrices + 1*64;
	dword *intra_matrix_fix = mpeg_quant_matrices + 2*64;
	dword *intra_matrix_fixl = mpeg_quant_matrices + 3*64;

	for(int i = 0; i < 64; i++) {
		intra_matrix[i] = (!i) ? 8 : matrix[i];
		intra_matrix1[i] = (intra_matrix[i]>>1);
		intra_matrix1[i] += ((intra_matrix[i] == 1) ? 1: 0);
		intra_matrix_fix[i] = FIX(intra_matrix[i]);
		intra_matrix_fixl[i] = FIXL(intra_matrix[i]);
	}
}

//----------------------------

void set_inter_matrix(dword *mpeg_quant_matrices, const byte *matrix) {

	dword *inter_matrix = mpeg_quant_matrices + 4*64;
	dword *inter_matrix1 = mpeg_quant_matrices + 5*64;
	dword *inter_matrix_fix = mpeg_quant_matrices + 6*64;
	dword *inter_matrix_fixl = mpeg_quant_matrices + 7*64;

	for(int i = 0; i < 64; i++) {
		inter_matrix1[i] = ((inter_matrix[i] = matrix[i])>>1);
		inter_matrix1[i] += ((inter_matrix[i] == 1) ? 1: 0);
		inter_matrix_fix[i] = FIX(inter_matrix[i]);
		inter_matrix_fixl[i] = FIXL(inter_matrix[i]);
	}
}

//----------------------------

void init_mpeg_matrix(dword *mpeg_quant_matrices) {

	set_intra_matrix(mpeg_quant_matrices, default_intra_matrix);
	set_inter_matrix(mpeg_quant_matrices, default_inter_matrix);
}

//----------------------------
