/*****************************************************************************
 *
 * XVID MPEG-4 VIDEO CODEC
 * - 8x8 block-based halfpel interpolation -
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
 * $Id: interpolate8x8.cpp,v 1.1.1.1 2005-07-13 14:36:15 jeanlf Exp $
 *
 ****************************************************************************/

#include "portab.h"
#include "global.h"
#include "interpolate8x8.h"

//----------------------------

void interpolate8x8_switch(byte *cur, const byte *refn, dword x, dword y, int dx, int dy, dword stride, bool rounding) {

	const byte *src = refn + ((y + (dy>>1)) * stride + x + (dx>>1));
	byte *dst = cur + (y * stride + x);

	switch(((dx & 1) << 1) + (dy & 1)) {
	case 0:
		transfer8x8_copy(dst, src, stride);
		break;
	case 1:
		interpolate8x8_halfpel_v(dst, src, stride, rounding);
		break;
	case 2:
		interpolate8x8_halfpel_h(dst, src, stride, rounding);
		break;
	default:
		interpolate8x8_halfpel_hv(dst, src, stride, rounding);
		break;
	}
}

//----------------------------

void interpolate8x8_quarterpel(byte *cur, byte *refn, byte *refh, byte *refv, byte *refhv, dword x, dword y, int dx, int dy, dword stride, bool rounding) {

	const int xRef = x*4 + dx;
	const int yRef = y*4 + dy;

	byte *src, *dst;
	byte *halfpel_h, *halfpel_v, *halfpel_hv;
	int x_int, y_int, x_frac, y_frac;

	x_int = xRef/4;
	if(xRef < 0 && xRef % 4)
		x_int--;

	x_frac = xRef - (4*x_int);

	y_int  = yRef/4;
	if (yRef < 0 && yRef % 4)
		y_int--;

	y_frac = yRef - (4*y_int);

	src = refn + y_int * stride + x_int;
	halfpel_h = refh;
	halfpel_v = refv;
	halfpel_hv = refhv;

	dst = cur + y * stride + x;

	switch((y_frac << 2) | (x_frac)) {

	case 0:
		transfer8x8_copy(dst, src, stride);
		break;

	case 1:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, src, halfpel_h, stride, rounding, 8);
		break;

	case 2:
		interpolate8x8_lowpass_h(dst, src, stride, rounding);
		break;

	case 3:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, src + 1, halfpel_h, stride, rounding, 8);
		break;

	case 4:
		interpolate8x8_lowpass_v(halfpel_v, src, stride, rounding);
		interpolate8x8_avg2(dst, src, halfpel_v, stride, rounding, 8);
		break;

	case 5:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_v, halfpel_hv, stride, rounding, 8);
		break;

	case 6:
		interpolate8x8_lowpass_hv(halfpel_hv, halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_h, halfpel_hv, stride, rounding, 8);
		break;

	case 7:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src + 1, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_v, halfpel_hv, stride, rounding, 8);
		break;

	case 8:
		interpolate8x8_lowpass_v(dst, src, stride, rounding);
		break;

	case 9:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(dst, halfpel_v, stride, rounding);
		break;

	case 10:
		interpolate8x8_lowpass_hv(dst, halfpel_h, src, stride, rounding);
		break;

	case 11:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src + 1, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(dst, halfpel_v, stride, rounding);
		break;

	case 12:
		interpolate8x8_lowpass_v(halfpel_v, src, stride, rounding);
		interpolate8x8_avg2(dst, src+stride, halfpel_v, stride, rounding, 8);
		break;

	case 13:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_v+stride, halfpel_hv, stride, rounding, 8);
		break;

	case 14:
		interpolate8x8_lowpass_hv(halfpel_hv, halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_h+stride, halfpel_hv, stride, rounding, 8);
		break;

	case 15:
		interpolate8x8_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src + 1, halfpel_h, stride, rounding, 9);
		interpolate8x8_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_hv, halfpel_v + stride, stride, rounding, 8);
		break;
	}
}

//----------------------------

void interpolate16x16_quarterpel(byte *cur, byte *refn, byte *refh, byte *refv, byte *refhv, dword x, dword y, int dx, int dy, dword stride, bool rounding) {

	const int xRef = x*4 + dx;
	const int yRef = y*4 + dy;

	byte *src, *dst;
	byte *halfpel_h, *halfpel_v, *halfpel_hv;
	int x_int, y_int, x_frac, y_frac;

	x_int = xRef/4;
	if (xRef < 0 && xRef % 4)
		x_int--;

	x_frac = xRef - (4*x_int);

	y_int  = yRef/4;
	if (yRef < 0 && yRef % 4)
		y_int--;

	y_frac = yRef - (4*y_int);

	src = refn + y_int * stride + x_int;
	halfpel_h = refh;
	halfpel_v = refv;
	halfpel_hv = refhv;

	dst = cur + y * stride + x;

	switch((y_frac << 2) | (x_frac)) {
	case 0:
		transfer16x16_copy(dst, src, stride);
		break;

	case 1:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, src, halfpel_h, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, src+8, halfpel_h+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, src+8*stride, halfpel_h+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, src+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 8);
		break;

	case 2:
		interpolate16x16_lowpass_h(dst, src, stride, rounding);
		break;

	case 3:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, src + 1, halfpel_h, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, src + 8 + 1, halfpel_h+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, src + 8*stride + 1, halfpel_h+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, src+8*stride+8 + 1, halfpel_h+8*stride+8, stride, rounding, 8);
		break;

	case 4:
		interpolate16x16_lowpass_v(halfpel_v, src, stride, rounding);
		interpolate8x8_avg2(dst, src, halfpel_v, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, src+8, halfpel_v+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, src+8*stride, halfpel_v+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, src+8*stride+8, halfpel_v+8*stride+8, stride, rounding, 8);
		break;

	case 5:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);

		interpolate16x16_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_hv, halfpel_v, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_hv+8, halfpel_v+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_hv+8*stride, halfpel_v+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_hv+8*stride+8, halfpel_v+8*stride+8, stride, rounding, 8);
		break;

	case 6:
		interpolate16x16_lowpass_hv(halfpel_hv, halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_h, halfpel_hv, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_h+8, halfpel_hv+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_h+8*stride, halfpel_hv+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_h+8*stride+8, halfpel_hv+8*stride+8, stride, rounding, 8);
		break;

	case 7:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src+1, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src+1 + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src+1 + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+1+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);

		interpolate16x16_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_hv, halfpel_v, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_hv+8, halfpel_v+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_hv+8*stride, halfpel_v+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_hv+8*stride+8, halfpel_v+8*stride+8, stride, rounding, 8);
		break;

	case 8:
		interpolate16x16_lowpass_v(dst, src, stride, rounding);
		break;

	case 9:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);
		interpolate16x16_lowpass_v(dst, halfpel_v, stride, rounding);
		break;

	case 10:
		interpolate16x16_lowpass_hv(dst, halfpel_h, src, stride, rounding);
		break;

	case 11:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src+1, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src+1 + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src+1 + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+1+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);
		interpolate16x16_lowpass_v(dst, halfpel_v, stride, rounding);
		break;

	case 12:
		interpolate16x16_lowpass_v(halfpel_v, src, stride, rounding);
		interpolate8x8_avg2(dst, src+stride, halfpel_v, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, src+stride+8, halfpel_v+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, src+stride+8*stride, halfpel_v+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, src+stride+8*stride+8, halfpel_v+8*stride+8, stride, rounding, 8);
		break;

	case 13:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);

		interpolate16x16_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_hv, halfpel_v+stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_hv+8, halfpel_v+stride+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_hv+8*stride, halfpel_v+stride+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_hv+8*stride+8, halfpel_v+stride+8*stride+8, stride, rounding, 8);
		break;

	case 14:
		interpolate16x16_lowpass_hv(halfpel_hv, halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_h+stride, halfpel_hv, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_h+stride+8, halfpel_hv+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_h+stride+8*stride, halfpel_hv+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_h+stride+8*stride+8, halfpel_hv+8*stride+8, stride, rounding, 8);
		break;

	case 15:
		interpolate16x16_lowpass_h(halfpel_h, src, stride, rounding);
		interpolate8x8_avg2(halfpel_v, src+1, halfpel_h, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8, src+1 + 8, halfpel_h+8, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride, src+1 + 8*stride, halfpel_h+8*stride, stride, rounding, 9);
		interpolate8x8_avg2(halfpel_v+8*stride+8, src+1+8*stride+8, halfpel_h+8*stride+8, stride, rounding, 9);

		interpolate16x16_lowpass_v(halfpel_hv, halfpel_v, stride, rounding);
		interpolate8x8_avg2(dst, halfpel_hv, halfpel_v+stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8, halfpel_hv+8, halfpel_v+stride+8, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride, halfpel_hv+8*stride, halfpel_v+stride+8*stride, stride, rounding, 8);
		interpolate8x8_avg2(dst+8*stride+8, halfpel_hv+8*stride+8, halfpel_v+stride+8*stride+8, stride, rounding, 8);
		break;
	}
}

//----------------------------

void interpolate8x8_avg2(byte *dst, const byte *src1, const byte *src2, dword stride, bool rounding, dword height) {

	const int round = 1 - rounding;
	for(dword i = 0; i < height; i++) {
		dst[0] = (src1[0] + src2[0] + round) >> 1;
		dst[1] = (src1[1] + src2[1] + round) >> 1;
		dst[2] = (src1[2] + src2[2] + round) >> 1;
		dst[3] = (src1[3] + src2[3] + round) >> 1;
		dst[4] = (src1[4] + src2[4] + round) >> 1;
		dst[5] = (src1[5] + src2[5] + round) >> 1;
		dst[6] = (src1[6] + src2[6] + round) >> 1;
		dst[7] = (src1[7] + src2[7] + round) >> 1;

		dst += stride;
		src1 += stride;
		src2 += stride;
	}
}

//----------------------------

#ifndef _ARM_

void interpolate8x8_halfpel_h(byte *dst, const byte *src, dword stride, bool rounding) {

#if defined USE_ARM_ASM && 1
	int y, tmp, tmp1, tmp2, tmp3;
	if(rounding) {
		asm volatile(
		    "mov %7, #8\n\t"
		    "\n.ihh_loop:\n\t"
		    "ldrb %2, [%1, #8]\n\t"
		    //7+8
		    "ldrb %3, [%1, #7]\n\t"
		    "add %4, %2, %3\n\t"
		    "mov %4, %4, asr #1\n\t"
		    //6+7
		    "ldrb %2, [%1, #6]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //5+6
		    "ldrb %3, [%1, #5]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //4+5
		    "ldrb %2, [%1, #4]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"

		    "str %4, [%0, #4]\n\t"

		    //3+4
		    "ldrb %3, [%1, #3]\n\t"
		    "add %4, %2, %3\n\t"
		    "mov %4, %4, asr #1\n\t"
		    //2+3
		    "ldrb %2, [%1, #2]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //1+2
		    "ldrb %3, [%1, #1]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //0+1
		    "ldrb %2, [%1, #0]\n\t"
		    "add %5, %2, %3\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"

		    "str %4, [%0, #0]\n\t"

		    "add %0, %0, %6\n   add %1, %1, %6\n\t"
		    "subs %7, %7, #1\n   bne .ihh_loop\n\t"
		    : "+r"(dst), "+r"(src), "&=r"(tmp), "&=r"(tmp1), "&=r"(tmp2), "&=r"(tmp3)
		    : "r"(stride), "%r"(y)
		);
	} else {
		asm volatile(
		    "mov %7, #8\n\t"
		    "\n.ihh_loop1:\n\t"
		    "ldrb %2, [%1, #8]\n\t"
		    //7+8
		    "ldrb %3, [%1, #7]\n\t"
		    "add %4, %2, %3\n\t"
		    "add %4, %4, #1\n\t"
		    "mov %4, %4, asr #1\n\t"
		    //6+7
		    "ldrb %2, [%1, #6]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //5+6
		    "ldrb %3, [%1, #5]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //4+5
		    "ldrb %2, [%1, #4]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"

		    "str %4, [%0, #4]\n\t"

		    //3+4
		    "ldrb %3, [%1, #3]\n\t"
		    "add %4, %2, %3\n\t"
		    "add %4, %4, #1\n\t"
		    "mov %4, %4, asr #1\n\t"
		    //2+3
		    "ldrb %2, [%1, #2]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //1+2
		    "ldrb %3, [%1, #1]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"
		    //0+1
		    "ldrb %2, [%1, #0]\n\t"
		    "add %5, %2, %3\n\t"
		    "add %5, %5, #1\n\t"
		    "mov %5, %5, asr #1\n\t"
		    "orr %4, %5, %4, asl #8\n\t"

		    "str %4, [%0, #0]\n\t"

		    "add %0, %0, %6\n   add %1, %1, %6\n\t"
		    "subs %7, %7, #1\n   bne .ihh_loop1\n\t"
		    : "+r"(dst), "+r"(src), "&=r"(tmp), "&=r"(tmp1), "&=r"(tmp2), "&=r"(tmp3)
		    : "r"(stride), "%r"(y)
		);
	}
#else
	if(rounding) {
		for(dword j = 0; j < 8*stride; j+=stride) {
			dst[j + 0] = byte((src[j + 0] + src[j + 1] )>>1);
			dst[j + 1] = byte((src[j + 1] + src[j + 2] )>>1);
			dst[j + 2] = byte((src[j + 2] + src[j + 3] )>>1);
			dst[j + 3] = byte((src[j + 3] + src[j + 4] )>>1);
			dst[j + 4] = byte((src[j + 4] + src[j + 5] )>>1);
			dst[j + 5] = byte((src[j + 5] + src[j + 6] )>>1);
			dst[j + 6] = byte((src[j + 6] + src[j + 7] )>>1);
			dst[j + 7] = byte((src[j + 7] + src[j + 8] )>>1);
		}
	} else {
		for(dword j = 0; j < 8*stride; j+=stride) {
			//forward or backwards? Who knows ...
			dst[j + 0] = byte((src[j + 0] + src[j + 1] + 1)>>1);
			dst[j + 1] = byte((src[j + 1] + src[j + 2] + 1)>>1);
			dst[j + 2] = byte((src[j + 2] + src[j + 3] + 1)>>1);
			dst[j + 3] = byte((src[j + 3] + src[j + 4] + 1)>>1);
			dst[j + 4] = byte((src[j + 4] + src[j + 5] + 1)>>1);
			dst[j + 5] = byte((src[j + 5] + src[j + 6] + 1)>>1);
			dst[j + 6] = byte((src[j + 6] + src[j + 7] + 1)>>1);
			dst[j + 7] = byte((src[j + 7] + src[j + 8] + 1)>>1);
		}
	}
#endif
}

#endif

//----------------------------

#ifndef _ARM_

void interpolate8x8_halfpel_v(byte *dst, const byte *src, dword stride, bool rounding) {

#if defined USE_ARM_ASM && 1
	int y, tmp, tmp1, tmp2, tmp3, tmp4;
	if(rounding) {
		asm volatile(
		    "mov %7, #8\n\t"
		    "\n.ihv_loop:\n\t"
		    "add %5, %1, %6\n\t"
		    //3
		    "ldrb %2, [%1, #3]\n   ldrb %3, [%5, #3]\n\t"
		    "add %4, %2, %3\n   mov %4, %4, asr #1\n\t"
		    //2
		    "ldrb %2, [%1, #2]\n   ldrb %3, [%5, #2]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //1
		    "ldrb %2, [%1, #1]\n   ldrb %3, [%5, #1]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //0
		    "ldrb %2, [%1, #0]\n   ldrb %3, [%5, #0]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"

		    "str %4, [%0, #0]\n\t"

		    //7
		    "ldrb %2, [%1, #7]\n   ldrb %3, [%5, #7]\n\t"
		    "add %4, %2, %3\n   mov %4, %4, asr #1\n\t"
		    //6
		    "ldrb %2, [%1, #6]\n   ldrb %3, [%5, #6]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //5
		    "ldrb %2, [%1, #5]\n   ldrb %3, [%5, #5]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //4
		    "ldrb %2, [%1, #4]\n   ldrb %3, [%5, #4]\n\t"
		    "add %2, %2, %3\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"

		    "str %4, [%0, #4]\n\t"

		    "add %0, %0, %6\n   add %1, %1, %6\n\t"
		    "subs %7, %7, #1\n   bne .ihv_loop\n\t"
		    : "+r"(dst), "+r"(src), "&=r"(tmp), "&=r"(tmp1), "&=r"(tmp2), "&=r"(tmp3)
		    : "r"(stride), "%r"(y)
		);
	} else {
		asm volatile(
		    "mov %7, #8\n\t"
		    "\n.ihv_loop1:\n\t"
		    "add %5, %1, %6\n\t"
		    //3
		    "ldrb %2, [%1, #3]\n   ldrb %3, [%5, #3]\n\t"
		    "add %4, %2, %3\n   add %4, %4, #1\n   mov %4, %4, asr #1\n\t"
		    //2
		    "ldrb %2, [%1, #2]\n   ldrb %3, [%5, #2]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //1
		    "ldrb %2, [%1, #1]\n   ldrb %3, [%5, #1]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //0
		    "ldrb %2, [%1, #0]\n   ldrb %3, [%5, #0]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"

		    "str %4, [%0, #0]\n\t"

		    //7
		    "ldrb %2, [%1, #7]\n   ldrb %3, [%5, #7]\n\t"
		    "add %4, %2, %3\n   add %4, %4, #1\n   mov %4, %4, asr #1\n\t"
		    //6
		    "ldrb %2, [%1, #6]\n   ldrb %3, [%5, #6]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //5
		    "ldrb %2, [%1, #5]\n   ldrb %3, [%5, #5]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"
		    //4
		    "ldrb %2, [%1, #4]\n   ldrb %3, [%5, #4]\n\t"
		    "add %2, %2, %3\n   add %2, %2, #1\n   mov %2, %2, asr #1\n\t"
		    "orr %4, %2, %4, asl #8\n\t"

		    "str %4, [%0, #4]\n\t"

		    "add %0, %0, %6\n   add %1, %1, %6\n\t"
		    "subs %7, %7, #1\n   bne .ihv_loop1\n\t"
		    : "+r"(dst), "+r"(src), "&=r"(tmp), "&=r"(tmp1), "&=r"(tmp2), "&=r"(tmp3)
		    : "r"(stride), "%r"(y)
		);
	}
#else
	if(rounding) {
		for(dword j = 0; j < 8*stride; j+=stride) {     /* forward is better. Some automatic prefetch perhaps. */
			dst[j + 0] = byte((src[j + 0] + src[j + stride + 0] )>>1);
			dst[j + 1] = byte((src[j + 1] + src[j + stride + 1] )>>1);
			dst[j + 2] = byte((src[j + 2] + src[j + stride + 2] )>>1);
			dst[j + 3] = byte((src[j + 3] + src[j + stride + 3] )>>1);
			dst[j + 4] = byte((src[j + 4] + src[j + stride + 4] )>>1);
			dst[j + 5] = byte((src[j + 5] + src[j + stride + 5] )>>1);
			dst[j + 6] = byte((src[j + 6] + src[j + stride + 6] )>>1);
			dst[j + 7] = byte((src[j + 7] + src[j + stride + 7] )>>1);
		}
	} else {
		for(dword j = 0; j < 8*stride; j+=stride) {
			dst[j + 0] = byte((src[j + 0] + src[j + stride + 0] + 1)>>1);
			dst[j + 1] = byte((src[j + 1] + src[j + stride + 1] + 1)>>1);
			dst[j + 2] = byte((src[j + 2] + src[j + stride + 2] + 1)>>1);
			dst[j + 3] = byte((src[j + 3] + src[j + stride + 3] + 1)>>1);
			dst[j + 4] = byte((src[j + 4] + src[j + stride + 4] + 1)>>1);
			dst[j + 5] = byte((src[j + 5] + src[j + stride + 5] + 1)>>1);
			dst[j + 6] = byte((src[j + 6] + src[j + stride + 6] + 1)>>1);
			dst[j + 7] = byte((src[j + 7] + src[j + stride + 7] + 1)>>1);
		}
	}
#endif
}
#endif

//----------------------------

#ifndef _ARM_                  //PPC

#ifndef USE_ARM_ASM           //implemented in asm for ARM

void interpolate8x8_halfpel_hv(byte *dst, const byte *src, dword stride, bool rounding) {

	if(rounding) {
		for(dword j = 0; j < 8*stride; j+=stride) {
			dst[j + 0] = (byte)((src[j+0] + src[j+1] + src[j+stride+0] + src[j+stride+1] +1)>>2);
			dst[j + 1] = (byte)((src[j+1] + src[j+2] + src[j+stride+1] + src[j+stride+2] +1)>>2);
			dst[j + 2] = (byte)((src[j+2] + src[j+3] + src[j+stride+2] + src[j+stride+3] +1)>>2);
			dst[j + 3] = (byte)((src[j+3] + src[j+4] + src[j+stride+3] + src[j+stride+4] +1)>>2);
			dst[j + 4] = (byte)((src[j+4] + src[j+5] + src[j+stride+4] + src[j+stride+5] +1)>>2);
			dst[j + 5] = (byte)((src[j+5] + src[j+6] + src[j+stride+5] + src[j+stride+6] +1)>>2);
			dst[j + 6] = (byte)((src[j+6] + src[j+7] + src[j+stride+6] + src[j+stride+7] +1)>>2);
			dst[j + 7] = (byte)((src[j+7] + src[j+8] + src[j+stride+7] + src[j+stride+8] +1)>>2);
		}
	} else {
		for(dword j = 0; j < 8*stride; j+=stride) {
			dst[j + 0] = (byte)((src[j+0] + src[j+1] + src[j+stride+0] + src[j+stride+1] +2)>>2);
			dst[j + 1] = (byte)((src[j+1] + src[j+2] + src[j+stride+1] + src[j+stride+2] +2)>>2);
			dst[j + 2] = (byte)((src[j+2] + src[j+3] + src[j+stride+2] + src[j+stride+3] +2)>>2);
			dst[j + 3] = (byte)((src[j+3] + src[j+4] + src[j+stride+3] + src[j+stride+4] +2)>>2);
			dst[j + 4] = (byte)((src[j+4] + src[j+5] + src[j+stride+4] + src[j+stride+5] +2)>>2);
			dst[j + 5] = (byte)((src[j+5] + src[j+6] + src[j+stride+5] + src[j+stride+6] +2)>>2);
			dst[j + 6] = (byte)((src[j+6] + src[j+7] + src[j+stride+6] + src[j+stride+7] +2)>>2);
			dst[j + 7] = (byte)((src[j+7] + src[j+8] + src[j+stride+7] + src[j+stride+8] +2)>>2);
		}
	}
}
#endif
#endif

//----------------------------

void interpolate16x16_lowpass_h(byte *dst, byte *src, int stride, int rounding) {

	int round_add = 16 - rounding;
	for(int i = 0; i < 17; i++) {

		dst[0] = CLIP(((7 * ((src[0]<<1) - src[2]) +  23 * src[1] + 3 * src[3] - src[4] + round_add) >> 5), 0, 255);
		dst[1] = CLIP(((19 * src[1] + 20 * src[2] - src[5] + 3 * (src[4] - src[0] - (src[3]<<1)) + round_add) >> 5), 0, 255);
		dst[2] = CLIP(((20 * (src[2] + src[3]) + (src[0]<<1) + 3 * (src[5] - ((src[1] + src[4])<<1)) - src[6] + round_add) >> 5), 0, 255);

		dst[3] = CLIP(((20 * (src[3] + src[4]) + 3 * ((src[6] + src[1]) - ((src[2] + src[5])<<1)) - (src[0] + src[7]) + round_add) >> 5), 0, 255);
		dst[4] = CLIP(((20 * (src[4] + src[5]) - 3 * (((src[3] + src[6])<<1) - (src[2] + src[7])) - (src[1] + src[8]) + round_add) >> 5), 0, 255);
		dst[5] = CLIP(((20 * (src[5] + src[6]) - 3 * (((src[4] + src[7])<<1) - (src[3] + src[8])) - (src[2] + src[9]) + round_add) >> 5), 0, 255);
		dst[6] = CLIP(((20 * (src[6] + src[7]) - 3 * (((src[5] + src[8])<<1) - (src[4] + src[9])) - (src[3] + src[10]) + round_add) >> 5), 0, 255);
		dst[7] = CLIP(((20 * (src[7] + src[8]) - 3 * (((src[6] + src[9])<<1) - (src[5] + src[10])) - (src[4] + src[11]) + round_add) >> 5), 0, 255);
		dst[8] = CLIP(((20 * (src[8] + src[9]) - 3 * (((src[7] + src[10])<<1) - (src[6] + src[11])) - (src[5] + src[12]) + round_add) >> 5), 0, 255);
		dst[9] = CLIP(((20 * (src[9] + src[10]) - 3 * (((src[8] + src[11])<<1) - (src[7] + src[12])) - (src[6] + src[13]) + round_add) >> 5), 0, 255);
		dst[10] = CLIP(((20 * (src[10] + src[11]) - 3 * (((src[9] + src[12])<<1) - (src[8] + src[13])) - (src[7] + src[14]) + round_add) >> 5), 0, 255);
		dst[11] = CLIP(((20 * (src[11] + src[12]) - 3 * (((src[10] + src[13])<<1) - (src[9] + src[14])) - (src[8] + src[15]) + round_add) >> 5), 0, 255);
		dst[12] = CLIP(((20 * (src[12] + src[13]) - 3 * (((src[11] + src[14])<<1) - (src[10] + src[15])) - (src[9] + src[16]) + round_add) >> 5), 0, 255);

		dst[13] = CLIP(((20 * (src[13] + src[14]) + (src[16]<<1) + 3 * (src[11] - ((src[12] + src[15]) << 1)) - src[10] + round_add) >> 5), 0, 255);
		dst[14] = CLIP(((19 * src[15] + 20 * src[14] + 3 * (src[12] - src[16] - (src[13] << 1)) - src[11] + round_add) >> 5), 0, 255);
		dst[15] = CLIP(((23 * src[15] + 7 * ((src[16]<<1) - src[14]) + 3 * src[13] - src[12] + round_add) >> 5), 0, 255);

		dst += stride;
		src += stride;
	}
}

//----------------------------

void interpolate8x8_lowpass_h(byte *dst, byte *src, int stride, int rounding) {

	int round_add = 16 - rounding;
	for(int i = 0; i < 9; i++) {

		dst[0] = CLIP(((7 * ((src[0]<<1) - src[2]) + 23 * src[1] + 3 * src[3] - src[4] + round_add) >> 5), 0, 255);
		dst[1] = CLIP(((19 * src[1] + 20 * src[2] - src[5] + 3 * (src[4] - src[0] - (src[3]<<1)) + round_add) >> 5), 0, 255);
		dst[2] = CLIP(((20 * (src[2] + src[3]) + (src[0]<<1) + 3 * (src[5] - ((src[1] + src[4])<<1)) - src[6] + round_add) >> 5), 0, 255);
		dst[3] = CLIP(((20 * (src[3] + src[4]) + 3 * ((src[6] + src[1]) - ((src[2] + src[5])<<1)) - (src[0] + src[7]) + round_add) >> 5), 0, 255);
		dst[4] = CLIP(((20 * (src[4] + src[5]) - 3 * (((src[3] + src[6])<<1) - (src[2] + src[7])) - (src[1] + src[8]) + round_add) >> 5), 0, 255);
		dst[5] = CLIP(((20 * (src[5] + src[6]) + (src[8]<<1) + 3 * (src[3] - ((src[4] + src[7]) << 1)) - src[2] + round_add) >> 5), 0, 255);
		dst[6] = CLIP(((19 * src[7] + 20 * src[6] + 3 * (src[4] - src[8] - (src[5] << 1)) - src[3] + round_add) >> 5), 0, 255);
		dst[7] = CLIP(((23 * src[7] + 7 * ((src[8]<<1) - src[6]) + 3 * src[5] - src[4] + round_add) >> 5), 0, 255);

		dst += stride;
		src += stride;
	}
}

//----------------------------

void interpolate16x16_lowpass_v(byte *dst, byte *src, int stride, int rounding) {

	int round_add = 16 - rounding;
	for(int i = 0; i < 17; i++) {
		int src0 = src[0];
		int src1 = src[stride];
		int src2 = src[2 * stride];
		int src3 = src[3 * stride];
		int src4 = src[4 * stride];
		int src5 = src[5 * stride];
		int src6 = src[6 * stride];
		int src7 = src[7 * stride];
		int src8 = src[8 * stride];
		int src9 = src[9 * stride];
		int src10 = src[10 * stride];
		int src11 = src[11 * stride];
		int src12 = src[12 * stride];
		int src13 = src[13 * stride];
		int src14 = src[14 * stride];
		int src15 = src[15 * stride];
		int src16 = src[16 * stride];


		dst[0] = CLIP(((7 * ((src0<<1) - src2) +  23 * src1 + 3 * src3 - src4 + round_add) >> 5), 0, 255);
		dst[stride] = CLIP(((19 * src1 + 20 * src2 - src5 + 3 * (src4 - src0 - (src3<<1)) + round_add) >> 5), 0, 255);
		dst[2*stride] = CLIP(((20 * (src2 + src3) + (src0<<1) + 3 * (src5 - ((src1 + src4)<<1)) - src6 + round_add) >> 5), 0, 255);

		dst[3*stride] = CLIP(((20 * (src3 + src4) + 3 * ((src6 + src1) - ((src2 + src5)<<1)) - (src0 + src7) + round_add) >> 5), 0, 255);
		dst[4*stride] = CLIP(((20 * (src4 + src5) - 3 * (((src3 + src6)<<1) - (src2 + src7)) - (src1 + src8) + round_add) >> 5), 0, 255);
		dst[5*stride] = CLIP(((20 * (src5 + src6) - 3 * (((src4 + src7)<<1) - (src3 + src8)) - (src2 + src9) + round_add) >> 5), 0, 255);
		dst[6*stride] = CLIP(((20 * (src6 + src7) - 3 * (((src5 + src8)<<1) - (src4 + src9)) - (src3 + src10) + round_add) >> 5), 0, 255);
		dst[7*stride] = CLIP(((20 * (src7 + src8) - 3 * (((src6 + src9)<<1) - (src5 + src10)) - (src4 + src11) + round_add) >> 5), 0, 255);
		dst[8*stride] = CLIP(((20 * (src8 + src9) - 3 * (((src7 + src10)<<1) - (src6 + src11)) - (src5 + src12) + round_add) >> 5), 0, 255);
		dst[9*stride] = CLIP(((20 * (src9 + src10) - 3 * (((src8 + src11)<<1) - (src7 + src12)) - (src6 + src13) + round_add) >> 5), 0, 255);
		dst[10*stride] = CLIP(((20 * (src10 + src11) - 3 * (((src9 + src12)<<1) - (src8 + src13)) - (src7 + src14) + round_add) >> 5), 0, 255);
		dst[11*stride] = CLIP(((20 * (src11 + src12) - 3 * (((src10 + src13)<<1) - (src9 + src14)) - (src8 + src15) + round_add) >> 5), 0, 255);
		dst[12*stride] = CLIP(((20 * (src12 + src13) - 3 * (((src11 + src14)<<1) - (src10 + src15)) - (src9 + src16) + round_add) >> 5), 0, 255);

		dst[13*stride] = CLIP(((20 * (src13 + src14) + (src16<<1) + 3 * (src11 - ((src12 + src15) << 1)) - src10 + round_add) >> 5), 0, 255);
		dst[14*stride] = CLIP(((19 * src15 + 20 * src14 + 3 * (src12 - src16 - (src13 << 1)) - src11 + round_add) >> 5), 0, 255);
		dst[15*stride] = CLIP(((23 * src15 + 7 * ((src16<<1) - src14) + 3 * src13 - src12 + round_add) >> 5), 0, 255);

		dst++;
		src++;
	}
}

//----------------------------

void interpolate8x8_lowpass_v(byte *dst, byte *src, int stride, int rounding) {

	int round_add = 16 - rounding;
	for(int i = 0; i < 9; i++) {
		int src0 = src[0];
		int src1 = src[stride];
		int src2 = src[2 * stride];
		int src3 = src[3 * stride];
		int src4 = src[4 * stride];
		int src5 = src[5 * stride];
		int src6 = src[6 * stride];
		int src7 = src[7 * stride];
		int src8 = src[8 * stride];

		dst[0]       = CLIP(((7 * ((src0<<1) - src2) + 23 * src1 + 3 * src3 - src4 + round_add) >> 5), 0, 255);
		dst[stride]     = CLIP(((19 * src1 + 20 * src2 - src5 + 3 * (src4 - src0 - (src3 << 1)) + round_add) >> 5), 0, 255);
		dst[2 * stride] = CLIP(((20 * (src2 + src3) + (src0<<1) + 3 * (src5 - ((src1 + src4) <<1 )) - src6 + round_add) >> 5), 0, 255);
		dst[3 * stride] = CLIP(((20 * (src3 + src4) + 3 * ((src6 + src1) - ((src2 + src5)<<1)) - (src0 + src7) + round_add) >> 5), 0, 255);
		dst[4 * stride] = CLIP(((20 * (src4 + src5) + 3 * ((src2 + src7) - ((src3 + src6)<<1)) - (src1 + src8) + round_add) >> 5), 0, 255);
		dst[5 * stride] = CLIP(((20 * (src5 + src6) + (src8<<1) + 3 * (src3 - ((src4 + src7) << 1)) - src2 + round_add) >> 5), 0, 255);
		dst[6 * stride] = CLIP(((19 * src7 + 20 * src6 - src3 + 3 * (src4 - src8 - (src5 << 1)) + round_add) >> 5), 0, 255);
		dst[7 * stride] = CLIP(((7 * ((src8<<1) - src6) + 23 * src7 + 3 * src5 - src4 + round_add) >> 5), 0, 255);

		dst++;
		src++;
	}
}

//----------------------------

void interpolate16x16_lowpass_hv(byte *dst1, byte *dst2, byte *src, int stride, int rounding) {

	byte round_add = 16 - rounding;
	byte *h_ptr = dst2;
	for(int i = 0; i < 17; i++) {

		h_ptr[0] = CLIP(((7 * ((src[0]<<1) - src[2]) +  23 * src[1] + 3 * src[3] - src[4] + round_add) >> 5), 0, 255);
		h_ptr[1] = CLIP(((19 * src[1] + 20 * src[2] - src[5] + 3 * (src[4] - src[0] - (src[3]<<1)) + round_add) >> 5), 0, 255);
		h_ptr[2] = CLIP(((20 * (src[2] + src[3]) + (src[0]<<1) + 3 * (src[5] - ((src[1] + src[4])<<1)) - src[6] + round_add) >> 5), 0, 255);

		h_ptr[3] = CLIP(((20 * (src[3] + src[4]) + 3 * ((src[6] + src[1]) - ((src[2] + src[5])<<1)) - (src[0] + src[7]) + round_add) >> 5), 0, 255);
		h_ptr[4] = CLIP(((20 * (src[4] + src[5]) - 3 * (((src[3] + src[6])<<1) - (src[2] + src[7])) - (src[1] + src[8]) + round_add) >> 5), 0, 255);
		h_ptr[5] = CLIP(((20 * (src[5] + src[6]) - 3 * (((src[4] + src[7])<<1) - (src[3] + src[8])) - (src[2] + src[9]) + round_add) >> 5), 0, 255);
		h_ptr[6] = CLIP(((20 * (src[6] + src[7]) - 3 * (((src[5] + src[8])<<1) - (src[4] + src[9])) - (src[3] + src[10]) + round_add) >> 5), 0, 255);
		h_ptr[7] = CLIP(((20 * (src[7] + src[8]) - 3 * (((src[6] + src[9])<<1) - (src[5] + src[10])) - (src[4] + src[11]) + round_add) >> 5), 0, 255);
		h_ptr[8] = CLIP(((20 * (src[8] + src[9]) - 3 * (((src[7] + src[10])<<1) - (src[6] + src[11])) - (src[5] + src[12]) + round_add) >> 5), 0, 255);
		h_ptr[9] = CLIP(((20 * (src[9] + src[10]) - 3 * (((src[8] + src[11])<<1) - (src[7] + src[12])) - (src[6] + src[13]) + round_add) >> 5), 0, 255);
		h_ptr[10] = CLIP(((20 * (src[10] + src[11]) - 3 * (((src[9] + src[12])<<1) - (src[8] + src[13])) - (src[7] + src[14]) + round_add) >> 5), 0, 255);
		h_ptr[11] = CLIP(((20 * (src[11] + src[12]) - 3 * (((src[10] + src[13])<<1) - (src[9] + src[14])) - (src[8] + src[15]) + round_add) >> 5), 0, 255);
		h_ptr[12] = CLIP(((20 * (src[12] + src[13]) - 3 * (((src[11] + src[14])<<1) - (src[10] + src[15])) - (src[9] + src[16]) + round_add) >> 5), 0, 255);

		h_ptr[13] = CLIP(((20 * (src[13] + src[14]) + (src[16]<<1) + 3 * (src[11] - ((src[12] + src[15]) << 1)) - src[10] + round_add) >> 5), 0, 255);
		h_ptr[14] = CLIP(((19 * src[15] + 20 * src[14] + 3 * (src[12] - src[16] - (src[13] << 1)) - src[11] + round_add) >> 5), 0, 255);
		h_ptr[15] = CLIP(((23 * src[15] + 7 * ((src[16]<<1) - src[14]) + 3 * src[13] - src[12] + round_add) >> 5), 0, 255);

		h_ptr += stride;
		src += stride;
	}
	interpolate16x16_lowpass_v(dst1, dst2, stride, rounding);

}

//----------------------------

void interpolate8x8_lowpass_hv(byte *dst1, byte *dst2, byte *src, int stride, int rounding) {

	byte round_add = 16 - rounding;
	byte *h_ptr = dst2;
	for(int i = 0; i < 9; i++) {

		h_ptr[0] = CLIP(((7 * ((src[0]<<1) - src[2]) + 23 * src[1] + 3 * src[3] - src[4] + round_add) >> 5), 0, 255);
		h_ptr[1] = CLIP(((19 * src[1] + 20 * src[2] - src[5] + 3 * (src[4] - src[0] - (src[3]<<1)) + round_add) >> 5), 0, 255);
		h_ptr[2] = CLIP(((20 * (src[2] + src[3]) + (src[0]<<1) + 3 * (src[5] - ((src[1] + src[4])<<1)) - src[6] + round_add) >> 5), 0, 255);
		h_ptr[3] = CLIP(((20 * (src[3] + src[4]) + 3 * ((src[6] + src[1]) - ((src[2] + src[5])<<1)) - (src[0] + src[7]) + round_add) >> 5), 0, 255);
		h_ptr[4] = CLIP(((20 * (src[4] + src[5]) - 3 * (((src[3] + src[6])<<1) - (src[2] + src[7])) - (src[1] + src[8]) + round_add) >> 5), 0, 255);
		h_ptr[5] = CLIP(((20 * (src[5] + src[6]) + (src[8]<<1) + 3 * (src[3] - ((src[4] + src[7]) << 1)) - src[2] + round_add) >> 5), 0, 255);
		h_ptr[6] = CLIP(((19 * src[7] + 20 * src[6] + 3 * (src[4] - src[8] - (src[5] << 1)) - src[3] + round_add) >> 5), 0, 255);
		h_ptr[7] = CLIP(((23 * src[7] + 7 * ((src[8]<<1) - src[6]) + 3 * src[5] - src[4] + round_add) >> 5), 0, 255);

		h_ptr += stride;
		src += stride;
	}
	interpolate8x8_lowpass_v(dst1, dst2, stride, rounding);
}

//----------------------------
