/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Interpolation related header  -
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
 * $Id: interpolate8x8.h,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/

#ifndef _INTERPOLATE8X8_H_
#define _INTERPOLATE8X8_H_

#include "mem_transfer.h"

typedef void (INTERPOLATE8X8)(byte *dst, const byte *src, dword stride, bool rounding);
typedef INTERPOLATE8X8 *INTERPOLATE8X8_PTR;

typedef void (INTERPOLATE8X8_AVG2)(byte *dst, const byte *src1, const byte *src2, dword stride, bool rounding, dword height);
typedef INTERPOLATE8X8_AVG2 *INTERPOLATE8X8_AVG2_PTR;

typedef void (INTERPOLATE8X8_AVG4)(byte *dst, const byte *src1, const byte *src2, const byte *src3, const byte *src4, dword stride, bool rounding);
typedef INTERPOLATE8X8_AVG4 *INTERPOLATE8X8_AVG4_PTR;

typedef void (INTERPOLATE_LOWPASS) (byte *dst,
                                    byte *src,
                                    int stride,
                                    int rounding);

typedef INTERPOLATE_LOWPASS *INTERPOLATE_LOWPASS_PTR;

typedef void (INTERPOLATE_LOWPASS_HV) (byte *dst1,
                                       byte *dst2,
                                       byte *src,
                                       int stride,
                                       int rounding);

typedef INTERPOLATE_LOWPASS_HV *INTERPOLATE_LOWPASS_HV_PTR;

typedef void (INTERPOLATE8X8_6TAP_LOWPASS)(byte *dst, byte *src, int stride, bool rounding);

typedef INTERPOLATE8X8_6TAP_LOWPASS *INTERPOLATE8X8_6TAP_LOWPASS_PTR;

#ifdef _ARM_                  //PPC
extern"C" {
#endif

	INTERPOLATE8X8 interpolate8x8_halfpel_h;
	INTERPOLATE8X8 interpolate8x8_halfpel_hv;
	INTERPOLATE8X8 interpolate8x8_halfpel_v;

#ifdef _ARM_
}
#endif


INTERPOLATE8X8_AVG2 interpolate8x8_avg2;

INTERPOLATE_LOWPASS interpolate8x8_lowpass_h;
INTERPOLATE_LOWPASS interpolate8x8_lowpass_v;

INTERPOLATE_LOWPASS interpolate16x16_lowpass_h;
INTERPOLATE_LOWPASS interpolate16x16_lowpass_v;

INTERPOLATE_LOWPASS_HV interpolate8x8_lowpass_hv;
INTERPOLATE_LOWPASS_HV interpolate16x16_lowpass_hv;

//----------------------------
// Notes:
//    x, y is always multiply of 4, so writing to 'cur' is always dword aligned
//    stride is always multiply of 4
void interpolate8x8_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding);

inline void interpolate16x16_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding) {

	interpolate8x8_switch(cur, refn, x,   y,   dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+8, y,   dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x,   y+8, dx, dy, stride, rounding);
	interpolate8x8_switch(cur, refn, x+8, y+8, dx, dy, stride, rounding);
}

inline void interpolate32x32_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding) {
	interpolate16x16_switch(cur, refn, x,    y,    dx, dy, stride, rounding);
	interpolate16x16_switch(cur, refn, x+16, y,    dx, dy, stride, rounding);
	interpolate16x16_switch(cur, refn, x,    y+16, dx, dy, stride, rounding);
	interpolate16x16_switch(cur, refn, x+16, y+16, dx, dy, stride, rounding);
}

void interpolate8x8_quarterpel(byte *cur, byte *refn, byte *refh, byte *refv, byte *refhv, dword x, dword y, int dx, int dy, dword stride, bool rounding);
void interpolate16x16_quarterpel(byte *cur, byte *refn, byte *refh, byte *refv, byte *refhv, dword x, dword y, int dx, int dy, dword stride, bool rounding);

#endif
