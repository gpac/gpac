/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - (de)Quantization related header  -
 *
 *  Copyright(C) 2003 Edouard Gomez <ed.gomez@free.fr>
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
 * $Id: quant.h,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _QUANT_H_
#define _QUANT_H_

#include "portab.h"

/*****************************************************************************
 * Common API for Intra (de)Quant functions
 ****************************************************************************/

typedef void (quant_intraFunc)(int *coeff, const int *data, dword quant, dword dcscalar, const dword *mpeg_quant_matrices);

typedef quant_intraFunc *quant_intraFuncPtr;

/* DeQuant functions */
quant_intraFunc dequant_h263_intra;
quant_intraFunc dequant_mpeg_intra;

/*****************************************************************************
 * Common API for Inter (de)Quant functions
 ****************************************************************************/

typedef void (quant_interFunc)(int *coeff, const int* data, dword quant, const dword *mpeg_quant_matrices);

typedef quant_interFunc *quant_interFuncPtr;

quant_interFunc dequant_h263_inter;
quant_interFunc dequant_mpeg_inter;

#endif /* _QUANT_H_ */
