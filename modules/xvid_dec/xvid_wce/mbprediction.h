/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Prediction header -
 *
 *  Copyright(C) 2002-2003 xvid team <xvid-devel@xvid.org>
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
 * $Id: mbprediction.h,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/


#ifndef _MBPREDICTION_H_
#define _MBPREDICTION_H_

#include "portab.h"
#include "decoder.h"
#include "global.h"

#define MIN(X, Y) ((X)<(Y)?(X):(Y))
#define MAX(X, Y) ((X)>(Y)?(X):(Y))

/* very large value */
#define MV_MAX_ERROR	(4096 * 256)

#define MVequal(A,B) ( ((A).x)==((B).x) && ((A).y)==((B).y) )

void add_acdc(MACROBLOCK *pMB, dword block, int dct_codes[64], dword iDcScaler, int predictors[8]);

void predict_acdc(MACROBLOCK * pMBs, dword x, dword y, dword mb_width, dword block, int qcoeff[64], dword current_quant, int iDcScaler, int predictors[8], int bound);

VECTOR
get_pmv2(const MACROBLOCK * const mbs,
         const int mb_width,
         const int bound,
         const int x,
         const int y,
         const int block);

#endif							/* _MBPREDICTION_H_ */
