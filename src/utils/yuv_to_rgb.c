/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


/* original code from XviD colorspace module */

/*****************************************************************************
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - colorspace conversion module -
 *
 *  Copyright(C) 2002 Peter Ross <pross@xvid.org>
 *               2002 Michael Militzer <isibaar@xvid.org>
 *
 *  follows the usual GPL license terms
 ****************************************************************************/


#include <gpac/yuv.h>


#define col_clip(a) (a<0 ? 0 : (a>255 ? 255 : a) )

#define GF_COL_565(r, g, b) (u16) (((r & 248)<<8) + ((g & 252)<<3)  + (b>>3))
#define GF_COL_555(r, g, b) (u16) (((r & 248)<<7) + ((g & 248)<<2)  + (b>>3))

static s32 RGB_Y[256];
static s32 B_U[256];
static s32 G_U[256];
static s32 G_V[256];
static s32 R_V[256];

#define SCALEBITS_OUT	13
#define FIX_OUT(x)		((unsigned short) ((x) * (1L<<SCALEBITS_OUT) + 0.5))

static s32 is_init = 0;

/**/
static void yuv2rgb_init(void) 
{
	s32 i;
	if (!is_init) {
		is_init = 1;
		for(i = 0; i < 256; i++) {
			RGB_Y[i] = FIX_OUT(1.164) * (i - 16);
			B_U[i] = FIX_OUT(2.018) * (i - 128);
			G_U[i] = FIX_OUT(0.391) * (i - 128);
			G_V[i] = FIX_OUT(0.813) * (i - 128);
			R_V[i] = FIX_OUT(1.596) * (i - 128);
		}
	}
}


void gf_yuv_to_rgb_555(unsigned char *dst, s32 dst_stride,
				 unsigned char *y_src, unsigned char * u_src, unsigned char * v_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hh, hw;
	const u32 dst_dif = 2 * dst_stride - 2 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dst_stride;
	unsigned char *y_src2 = y_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		s32 r, g, b, r2, g2, b2;
		r = g = b = r2 = g2 = b2 = 0;

		for (x = 0; x < hw; x++) {
			s32 u, v, rgb;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			b = (b & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
			g = (g & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
			r = (r & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) dst = GF_COL_555(r, g, b);
			y_src++;

			rgb_y = RGB_Y[*y_src];
			b = (b & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
			g = (g & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
			r = (r & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
			rgb = (s32) *(unsigned short *) (dst + 2);
			*(unsigned short *) (dst + 2) = GF_COL_555(r, g, b);
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			b2 = (b2 & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
			g2 = (g2 & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
			r2 = (r2 & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst2) = GF_COL_555(r2, g2, b2);
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			b2 = (b2 & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
			g2 = (g2 & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
			r2 = (r2 & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst2 + 2) = GF_COL_555(r2, g2, b2);
			y_src2++;

			dst += 4;
			dst2 += 4;
		}

		dst += dst_dif;
		dst2 += dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;

		u_src += uv_stride;
		v_src += uv_stride;
	}
}


void gf_yuv_to_rgb_565(unsigned char * dst, s32 dst_stride,
				 unsigned char* y_src, unsigned char* u_src, unsigned char* v_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dst_stride - 2 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dst_stride;
	unsigned char *y_src2 = y_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		s32 r, g, b, r2, g2, b2;
		r = g = b = r2 = g2 = b2 = 0;

		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			b = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			g = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			r = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst) = GF_COL_565(r, g, b);
			y_src++;

			rgb_y = RGB_Y[*y_src];
			b = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			g = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			r = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst + 2) = GF_COL_565(r, g, b);
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			b2 = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			g2 = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			r2 = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst2) = GF_COL_565(r2, g2, b2);
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			b2 = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			g2 = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			r2 = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			*(unsigned short *) (dst2 + 2) = GF_COL_565(r2, g2, b2);
			y_src2++;

			dst += 4;
			dst2 += 4;
		}

		dst += dst_dif;
		dst2 += dst_dif;
		y_src += y_dif;
		y_src2 += y_dif;
		u_src += uv_stride;
		v_src += uv_stride;
	}
}

void gf_yuv_to_bgr_24(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dststride - 3 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			dst[0] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[2] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src];
			dst[3] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[5] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			dst2[0] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[2] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			dst2[3] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[5] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			dst += 6;
			dst2 += 6;
		}

		dst += dst_dif;
		dst2 += dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;

		u_src += uv_stride;
		v_src += uv_stride;
	}
}

void gf_yuv_to_rgb_24(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dststride - 3 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			dst[0] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[2] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src];
			dst[3] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[5] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			dst2[0] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[2] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			dst2[3] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[5] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			dst += 6;
			dst2 += 6;
		}

		dst += dst_dif;
		dst2 += dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;

		u_src += uv_stride;
		v_src += uv_stride;
	}
}


void gf_yuv_to_rgb_24_flip(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, 
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dststride + 3 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;
	dst2 = dst + (height-2)*dststride;
	dst = dst2 + dststride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			dst[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src];
			dst[5] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[3] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			dst2[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			dst2[5] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			dst2[4] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[3] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			y_src2++;

			dst += 6;
			dst2 += 6;
		}

		dst -= dst_dif;
		dst2 -= dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;

		u_src += uv_stride;
		v_src += uv_stride;
	}
}

void gf_yuv_to_rgb_32(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *v_src, unsigned char * u_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dststride - 4 * width;
	s32 y_dif = 2 * y_stride - width;
	
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			dst[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT );
			dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT );
			dst[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT );
			//dst[3] = 255;
			y_src++;

			rgb_y = RGB_Y[*y_src];
			dst[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			dst[5]= col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			//dst[7] = 255;
			y_src++;

			rgb_y = RGB_Y[*y_src2];
			dst2[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			//dst2[3] = 255;
			y_src2++;

			rgb_y = RGB_Y[*y_src2];
			dst2[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
			dst2[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
			dst2[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
			//dst2[7] = 255;
			y_src2++;

			dst += 8;
			dst2 += 8;
		}

		dst += dst_dif;
		dst2 += dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;
		u_src += uv_stride;
		v_src += uv_stride;
	}
}

void gf_yuva_to_rgb_32(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *v_src, unsigned char * u_src, unsigned char *a_src,
				 s32 y_stride, s32 uv_stride, s32 width, s32 height)
{
	u32 x, y, hw, hh;
	const u32 dst_dif = 2 * dststride - 4 * width;
	s32 y_dif = 2 * y_stride - width;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;
	unsigned char *a_src2 = a_src + y_stride;
	
	yuv2rgb_init();

	hw = width / 2;
	hh = height / 2;
	for (y = hh; y; y--) {
		for (x = 0; x < hw; x++) {
			s32 u, v;
			s32 b_u, g_uv, r_v, rgb_y;

			u = u_src[x];
			v = v_src[x];

			b_u = B_U[u];
			g_uv = G_U[u] + G_V[v];
			r_v = R_V[v];

			rgb_y = RGB_Y[*y_src];
			dst[0] = col_clip ( (rgb_y + r_v) >> SCALEBITS_OUT );
			dst[1] = col_clip ( (rgb_y - g_uv) >> SCALEBITS_OUT );
			dst[2] = col_clip ( (rgb_y + b_u) >> SCALEBITS_OUT );
			dst[3] = *a_src;
			y_src++;
			a_src++;

			rgb_y = RGB_Y[*y_src];
			dst[4] = col_clip ( (rgb_y + r_v) >> SCALEBITS_OUT );
			dst[5] = col_clip ( (rgb_y - g_uv) >> SCALEBITS_OUT );
			dst[6] = col_clip ( (rgb_y + b_u) >> SCALEBITS_OUT );
			y_src++;
			a_src++;

			rgb_y = RGB_Y[*y_src2];
			dst2[0] = col_clip ( (rgb_y + r_v) >> SCALEBITS_OUT );
			dst2[1] = col_clip ( (rgb_y - g_uv) >> SCALEBITS_OUT );
			dst2[2] = col_clip ( (rgb_y + b_u) >> SCALEBITS_OUT );
			dst2[3] = *a_src2;
			y_src2++;
			a_src2++;

			rgb_y = RGB_Y[*y_src2];
			dst2[4] = col_clip ( (rgb_y + r_v) >> SCALEBITS_OUT );
			dst2[5] = col_clip ( (rgb_y - g_uv) >> SCALEBITS_OUT );
			dst2[6] = col_clip ( (rgb_y + b_u) >> SCALEBITS_OUT );
			dst2[7] = *a_src2;
			y_src2++;
			a_src2++;

			dst += 8;
			dst2 += 8;
		}

		dst += dst_dif;
		dst2 += dst_dif;

		y_src += y_dif;
		y_src2 += y_dif;
		a_src += y_dif;
		a_src2 += y_dif;

		u_src += uv_stride;
		v_src += uv_stride;
	}
}
