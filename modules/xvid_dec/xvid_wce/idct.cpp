/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Inverse DCT  -
 *
 *  These routines are from Independent JPEG Group's free JPEG software
 *  Copyright (C) 1991-1998, Thomas G. Lane (see the file README.IJG)
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
 * $Id: idct.cpp,v 1.2 2006-12-13 15:12:27 jeanlf Exp $
 *
 ****************************************************************************/

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 * MPEG2AVI
 * --------
 * v0.16B33 renamed the initialization function to init_idct_int32()
 * v0.16B32 removed the unused idct_row() and idct_col() functions
 * v0.16B3  changed var declarations to static, to enforce data align
 * v0.16B22  idct_FAST() renamed to idct_int32()
 *        also merged idct_FAST() into a single function, to help VC++
 *        optimize it.
 *
 * v0.14  changed int to long, to avoid confusion when compiling on x86
 *        platform ( in VC++ "int" -> 32bits )
 */

/**********************************************************/
/* inverse two dimensional DCT, Chen-Wang algorithm       */
/* (cf. IEEE ASSP-32, pp. 803-816, Aug. 1984)             */
/* 32-bit integer arithmetic (8 bit coefficients)         */
/* 11 mults, 29 adds per DCT                              */
/*                                      sE, 18.8.91       */
/**********************************************************/
/* coefficients extended to 12 bit for IEEE1180-1990      */
/* compliance                           sE,  2.1.94       */
/**********************************************************/

/* this code assumes >> to be a two's-complement arithmetic */
/* right shift: (-2)>>1 == -1 , (-3)>>1 == -2               */

#include "Decoder.h"

const int __W1 = 2841,        //2048*sqrt(2)*cos(1*pi/16)
          __W2 = 2676,               // 2048*sqrt(2)*cos(2*pi/16)
          __W3 = 2408,               // 2048*sqrt(2)*cos(3*pi/16)
          __W5 = 1609,               // 2048*sqrt(2)*cos(5*pi/16)
          __W6 = 1108,               // 2048*sqrt(2)*cos(6*pi/16)
          __W7 = 565;                // 2048*sqrt(2)*cos(7*pi/16)

//----------------------------
/* two dimensional inverse discrete cosine transform */
//idct_int32_init() must be called before the first call to this function!
void S_decoder::InverseDiscreteCosineTransform(int *block) const {

	const t_clip_val *iclp = iclip + 512;
#if defined USE_ARM_ASM
	void InverseDiscreteCosineTransform_ARM(int *block, const int *iclip);
	InverseDiscreteCosineTransform_ARM(block, iclp);
#else
	int i;
	//idct rows
	for(i = 8; i--; block += 8) {
		int X0 = block[0];
		int X1, X2, X3, X4, X5, X6, X7;
		if(!((X1 = block[4]) | (X2 = block[6]) | (X3 = block[2]) | (X4 = block[1]) | (X5 = block[7]) | (X6 = block[5]) | (X7 = block[3]))) {
			block[0] = block[1] = block[2] = block[3] = block[4] = block[5] = block[6] = block[7] = X0 << 3;
			continue;
		}
		//for proper rounding in the fourth stage
		X0 = (X0 << 11) + 128;
		X1 <<= 11;

		//first stage
		int X8 = __W7 * (X4 + X5);
		X4 = X8 + (__W1 - __W7) * X4;
		X5 = X8 - (__W1 + __W7) * X5;
		X8 = __W3 * (X6 + X7);
		X6 = X8 - (__W3 - __W5) * X6;
		X7 = X8 - (__W3 + __W5) * X7;

		//second stage
		X8 = X0 + X1;
		X0 -= X1;
		X1 = __W6 * (X3 + X2);
		X2 = X1 - (__W2 + __W6) * X2;
		X3 = X1 + (__W2 - __W6) * X3;
		X1 = X4 + X6;
		X4 -= X6;
		X6 = X5 + X7;
		X5 -= X7;

		//third stage
		X7 = X8 + X3;
		X8 -= X3;
		X3 = X0 + X2;
		X0 -= X2;
		X2 = (181 * (X4 + X5) + 128) >> 8;
		X4 = (181 * (X4 - X5) + 128) >> 8;

		//fourth stage
		block[0] = ((X7 + X1) >> 8);
		block[1] = ((X3 + X2) >> 8);
		block[2] = ((X0 + X4) >> 8);
		block[3] = ((X8 + X6) >> 8);
		block[4] = ((X8 - X6) >> 8);
		block[5] = ((X0 - X4) >> 8);
		block[6] = ((X3 - X2) >> 8);
		block[7] = ((X7 - X1) >> 8);
	}
	block -= 8*8;
	//idct columns
	for(i = 8; i--; ++block) {
		int X0 = block[8 * 0];
		int X1, X2, X3, X4, X5, X6, X7;
		//shortcut
		if(!((X1 = block[8 * 4]) | (X2 = block[8 * 6]) | (X3 = block[8 * 2]) | (X4 = block[8 * 1]) | (X5 = block[8 * 7]) | (X6 = block[8 * 5]) | (X7 = block[8 * 3]))) {
			block[8 * 0] = block[8 * 1] = block[8 * 2] = block[8 * 3] = block[8 * 4] = block[8 * 5] = block[8 * 6] = block[8 * 7] = iclp[(X0 + 32) >> 6];
			continue;
		}
		X0 = (X0 << 8) + 8192;
		X1 <<= 8;

		//first stage
		int X8 = __W7 * (X4 + X5) + 4;
		X4 = (X8 + (__W1 - __W7) * X4) >> 3;
		X5 = (X8 - (__W1 + __W7) * X5) >> 3;
		X8 = __W3 * (X6 + X7) + 4;
		X6 = (X8 - (__W3 - __W5) * X6) >> 3;
		X7 = (X8 - (__W3 + __W5) * X7) >> 3;

		//second stage
		X8 = X0 + X1;
		X0 -= X1;
		X1 = __W6 * (X3 + X2) + 4;
		X2 = (X1 - (__W2 + __W6) * X2) >> 3;
		X3 = (X1 + (__W2 - __W6) * X3) >> 3;
		X1 = X4 + X6;
		X4 -= X6;
		X6 = X5 + X7;
		X5 -= X7;

		//third stage
		X7 = X8 + X3;
		X8 -= X3;
		X3 = X0 + X2;
		X0 -= X2;
		X2 = (181 * (X4 + X5) + 128) >> 8;
		X4 = (181 * (X4 - X5) + 128) >> 8;

		//fourth stage
		block[8 * 0] = iclp[(X7 + X1) >> 14];
		block[8 * 1] = iclp[(X3 + X2) >> 14];
		block[8 * 2] = iclp[(X0 + X4) >> 14];
		block[8 * 3] = iclp[(X8 + X6) >> 14];
		block[8 * 4] = iclp[(X8 - X6) >> 14];
		block[8 * 5] = iclp[(X0 - X4) >> 14];
		block[8 * 6] = iclp[(X3 - X2) >> 14];
		block[8 * 7] = iclp[(X7 - X1) >> 14];
	}
#endif
}

//----------------------------

void S_decoder::idct_int32_init() {

	t_clip_val *iclp = iclip + 512;
	for(int i = -512; i < 512; i++)
		iclp[i] = (i < -256) ? -256 : ((i > 255) ? 255 : i);
}

//----------------------------
