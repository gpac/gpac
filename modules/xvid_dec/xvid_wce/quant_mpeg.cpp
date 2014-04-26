/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MPEG4 Quantization related header  -
 *
 *  Copyright(C) 2001-2003 Peter Ross <pross@xvid.org>
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
 * $Id: quant_mpeg.cpp,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#include "global.h"
#include "quant.h"
#include "quant_matrix.h"

//----------------------------
/* dequantize intra-block & clamp to [-2048,2047]
 *
 * data[i] = (coeff[i] * default_intra_matrix[i] * quant2) >> 4;
 */
void dequant_mpeg_intra(int *data, const int *coeff, dword quant, dword dcscalar, const dword *mpeg_quant_matrices) {

	const dword *intra_matrix = get_intra_matrix(mpeg_quant_matrices);

	data[0] = coeff[0] * dcscalar;
	if(data[0] < -2048) {
		data[0] = -2048;
	} else if(data[0] > 2047) {
		data[0] = 2047;
	}

	for(int i = 1; i < 64; i++) {
		if(coeff[i] == 0) {
			data[i] = 0;
		} else if(coeff[i] < 0) {
			int level = -coeff[i];

			level = (level * intra_matrix[i] * quant) >> 3;
			data[i] = (level <= 2048 ? -level : -2048);
		} else {
			dword level = coeff[i];

			level = (level * intra_matrix[i] * quant) >> 3;
			data[i] = (level <= 2047 ? level : 2047);
		}
	}
}

//----------------------------
/* dequantize inter-block & clamp to [-2048,2047]
 * data = ((2 * coeff + SIGN(coeff)) * inter_matrix[i] * quant) / 16
 */

void dequant_mpeg_inter(int *data, const int *coeff, dword quant, const dword *mpeg_quant_matrices) {

	dword sum = 0;
	const dword *inter_matrix = get_inter_matrix(mpeg_quant_matrices);

	for(int i = 0; i < 64; i++) {
		if(coeff[i] == 0) {
			data[i] = 0;
		} else if(coeff[i] < 0) {
			int level = -coeff[i];

			level = ((2 * level + 1) * inter_matrix[i] * quant) >> 4;
			data[i] = (level <= 2048 ? -level : -2048);
		} else {
			dword level = coeff[i];
			level = ((2 * level + 1) * inter_matrix[i] * quant) >> 4;
			data[i] = (level <= 2047 ? level : 2047);
		}
		sum ^= data[i];
	}
	/* mismatch control */
	if ((sum & 1) == 0) {
		data[63] ^= 1;
	}
}

//----------------------------

