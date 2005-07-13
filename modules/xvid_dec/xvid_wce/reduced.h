/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Reduced VOP header  -
 *
 *  Copyright(C) 2002 Pascal Massimino <skal@planet-d.net>
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
 * $Id: reduced.h,v 1.1.1.1 2005-07-13 14:36:16 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _REDUCED_H_
#define _REDUCED_H_

#include "portab.h"

/* decoding */
typedef void (COPY_UPSAMPLED_8X8_16TO8)(byte *Dst, const int *Src, int BpS);
typedef void (ADD_UPSAMPLED_8X8_16TO8)(byte *Dst, const int *Src, int BpS);

/* deblocking: Note: "Nb"_Blks is the number of 8-pixels blocks to process */
typedef void HFILTER_31(byte *Src1, byte *Src2, int Nb_Blks);
typedef void VFILTER_31(byte *Src1, byte *Src2, int BpS, int Nb_Blks);

/* encoding: WARNING! These read 1 pixel outside of the input 16x16 block! */
typedef void FILTER_18X18_TO_8X8(int *Dst, const byte *Src, int BpS);
typedef void FILTER_DIFF_18X18_TO_8X8(int *Dst, const byte *Src, int BpS);


extern COPY_UPSAMPLED_8X8_16TO8 copy_upsampled_8x8_16to8;
extern ADD_UPSAMPLED_8X8_16TO8 add_upsampled_8x8_16to8;

extern VFILTER_31 vfilter_31;

extern HFILTER_31 hfilter_31;

/* rrv motion vector scale-up */
#define RRV_MV_SCALEUP(a)	( (a)>0 ? 2*(a)-1 : (a)<0 ? 2*(a)+1 : (a) )

#endif /* _REDUCED_H_ */
