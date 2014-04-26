/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - MPEG4 Quantization H263 implementation -
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
 * $Id: quant_h263.cpp,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#include "global.h"
#include "quant.h"

//----------------------------
/* dequantize intra-block & clamp to [-2048,2047]
 */
void dequant_h263_intra(int *data, const int *coeff, dword quant, dword dcscalar, const dword *mpeg_quant_matrices) {

	const int quant_m_2 = quant << 1;
	const int quant_add = (quant & 1 ? quant : quant - 1);

	data[0] = *coeff++ * dcscalar;
	if(data[0] < -2048) {
		data[0] = -2048;
	}//else
	if(data[0] > 2047) {
		data[0] = 2047;
	}
	for(int i = 1; i < 64; i++) {
		int acLevel = *coeff++;

		if(acLevel == 0) {
			data[i] = 0;
		} else if(acLevel < 0) {
			acLevel = quant_m_2 * -acLevel + quant_add;
			data[i] = (acLevel <= 2048 ? -acLevel : -2048);
		} else {
			acLevel = quant_m_2 * acLevel + quant_add;
			data[i] = (acLevel <= 2047 ? acLevel : 2047);
		}
	}
}

//----------------------------
/* dequantize inter-block & clamp to [-2048,2047]
 */

void dequant_h263_inter(int *data, const int *coeff, dword quant, const dword *mpeg_quant_matrices) {

	const dword quant_m_2 = quant << 1;
	const dword quant_add = (quant & 1 ? quant : quant - 1);

	for(int i = 0; i < 64; i++) {
		int acLevel = *coeff++;

		if(acLevel == 0) {
			data[i] = 0;
		} else if(acLevel < 0) {
			acLevel = acLevel * quant_m_2 - quant_add;
			data[i] = (acLevel >= -2048 ? acLevel : -2048);
		} else {
			acLevel = acLevel * quant_m_2 + quant_add;
			data[i] = (acLevel <= 2047 ? acLevel : 2047);
		}
	}
}

//----------------------------
