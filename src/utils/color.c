/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#include <gpac/tools.h>
#include <gpac/constants.h>
#include <gpac/color.h>


/* YUV -> RGB conversion loading two lines at each call */

#define col_clip(a) MAX(0, MIN(255, a))
#define SCALEBITS_OUT	13
#define FIX_OUT(x)		((unsigned short) ((x) * (1L<<SCALEBITS_OUT) + 0.5))

static s32 RGB_Y[256];
static s32 B_U[256];
static s32 G_U[256];
static s32 G_V[256];
static s32 R_V[256];


static s32 yuv2rgb_is_init = 0;
static void yuv2rgb_init(void)
{
	s32 i;
	if (yuv2rgb_is_init) return;
	yuv2rgb_is_init = 1;

	for(i = 0; i < 256; i++) {
		RGB_Y[i] = FIX_OUT(1.164) * (i - 16);
		B_U[i] = FIX_OUT(2.018) * (i - 128);
		G_U[i] = FIX_OUT(0.391) * (i - 128);
		G_V[i] = FIX_OUT(0.813) * (i - 128);
		R_V[i] = FIX_OUT(1.596) * (i - 128);
	}
}

static void gf_yuv_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 y_stride, s32 uv_stride, s32 width)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *) dst + dststride;
	unsigned char *y_src2 = (unsigned char *) y_src + y_stride;

	hw = width / 2;
	for (x = 0; x < hw; x++) {
		s32 u, v;
		s32 b_u, g_uv, r_v, rgb_y;

		u = u_src[x];
		v = v_src[x];

		b_u = B_U[u];
		g_uv = G_U[u] + G_V[v];
		r_v = R_V[v];

		rgb_y = RGB_Y[*y_src];
		dst[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;

		rgb_y = RGB_Y[*y_src];
		dst[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;

		rgb_y = RGB_Y[*y_src2];
		dst2[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;

		rgb_y = RGB_Y[*y_src2];
		dst2[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;

		dst += 8;
		dst2 += 8;
	}
}

static void gf_yuv_10_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *_y_src, unsigned char *_u_src, unsigned char *_v_src, s32 y_stride, s32 uv_stride, s32 width)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *) dst + dststride;
	unsigned short *y_src2 = (unsigned short *) (_y_src + y_stride);
	unsigned short *y_src = (unsigned short *)_y_src;
	unsigned short *u_src = (unsigned short *)_u_src;
	unsigned short *v_src = (unsigned short *)_v_src;


	hw = width / 2;
	for (x = 0; x < hw; x++) {
		s32 u, v;
		s32 b_u, g_uv, r_v, rgb_y;

		u = u_src[x] >> 2;
		v = v_src[x] >> 2;

		b_u = B_U[u];
		g_uv = G_U[u] + G_V[v];
		r_v = R_V[v];

		rgb_y = RGB_Y[*y_src >> 2];
		dst[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;

		rgb_y = RGB_Y[*y_src >> 2];
		dst[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;

		rgb_y = RGB_Y[*y_src2 >> 2];
		dst2[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;

		rgb_y = RGB_Y[*y_src2 >> 2];
		dst2[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;

		dst += 8;
		dst2 += 8;
	}
}


static void gf_yuv_load_lines_packed(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 width)
{
	u32 hw, x;

	hw = width / 2;
	for (x = 0; x < hw; x++) {
		s32 u, v;
		s32 b_u, g_uv, r_v, rgb_y;

		u = *u_src;
		v = *v_src;

		b_u = B_U[u];
		g_uv = G_U[u] + G_V[v];
		r_v = R_V[v];

		rgb_y = RGB_Y[*y_src];
		dst[0] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;

		rgb_y = RGB_Y[*(y_src+2)];
		dst[4] = col_clip( (rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip( (rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip( (rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;

		dst += 8;
		y_src += 4;
		u_src += 4;
		v_src += 4;
	}
}


static void gf_yuva_load_lines(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char *v_src, unsigned char *a_src,
				 s32 y_stride, s32 uv_stride, s32 width)
{
	u32 hw, x;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;
	unsigned char *a_src2 = a_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
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
		dst[7] = *a_src;
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
}

static s32 mul255(s32 a, s32 b)
{
	return ((a+1) * b) >> 8;
}

typedef void (*copy_row_proto)(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha);
typedef void (*load_line_proto)(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 src_width, u32 src_height, u8 *dst_bits);

static void copy_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	s32 pos;
	u16 *dst = (u16 *)_dst;
	u8 a=0, r=0, g=0, b=0;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_555(r, g, b);
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_rgb_565(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	s32 pos;
	u16 *dst = (u16 *)_dst;
	u8 a=0, r=0, g=0, b=0;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_565(r, g, b);
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void copy_row_rgb_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	s32 pos;
	u8 a=0, r=0, g=0, b=0;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) { dst[0] = r; dst[1] = g; dst[2] = b; }
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_bgr_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	s32 pos;
	u8 a=0, r=0, g=0, b=0;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) { dst[0] = b; dst[1] = g; dst[2] = r; }
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_bgrx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			dst[0] = b;
			dst[1] = g;
			dst[2] = r;
			dst[3] = 0xFF;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_rgbx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while ( dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			dst[0] = r;
			dst[1] = g;
			dst[2] = b;
			dst[3] = 0xFF;
		}
		dst+=x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_rgbd(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while ( dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		dst[0] = r;
		dst[1] = g;
		dst[2] = b;
		dst[3] = a;

		dst+=x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a && alpha) {
			col = *dst;
			_r = (col >> 7) & 0xf8;
			_g = (col >> 2) & 0xf8;
			_b = (col << 3) & 0xf8;
			_r = mul255(a, r - _r) + _r;
			_g = mul255(a, g - _g) + _g;
			_b = mul255(a, b - _b) + _b;
			*dst = GF_COL_555(_r, _g, _b);
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void merge_row_rgb_565(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a) {
			col = *dst;
			_r = (col >> 8) & 0xf8;
			_g = (col >> 3) & 0xfc;
			_b = (col << 3) & 0xf8;
			_r = mul255(a, r - _r) + _r;
			_g = mul255(a, g - _g) + _g;
			_b = mul255(a, b - _b) + _b;
			*dst = GF_COL_565(_r, _g, _b);
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_rgb_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a) {
			_r = dst[0]; _g = dst[0]; _b = dst[0];
			dst[0] = mul255(a, r - _r) + _r;
			dst[1] = mul255(a, g - _g) + _g;
			dst[2] = mul255(a, b - _b) + _b;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void merge_row_bgr_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}

		if (a && alpha) {
			_b = dst[0];
			_g = dst[1];
			_r = dst[2]; 
			a = mul255(a, alpha);
			dst[0] = mul255(a, b - _b) + _b;
			dst[1] = mul255(a, g - _g) + _g;
			dst[2] = mul255(a, r - _r) + _r;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_bgrx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			a = mul255(a, alpha);
			pos -= 0x10000L;
		}

		if (a) {
			_b = dst[0];
			_g = dst[1];
			_r = dst[2];

			_r = mul255(a, r - _r) + _r;
			_g = mul255(a, g - _g) + _g;
			_b = mul255(a, b - _b) + _b;
			
			dst[0] = _b;
			dst[1] = _g;
			dst[2] = _r;
			dst[3] = 0xFF;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void merge_row_rgbx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			a = mul255(a, alpha);
			pos -= 0x10000L;
		}

		if (a) {
			_r = dst[0];
			_g = dst[1];
			_b = dst[2];
			_r = mul255(a, r - _r) + _r;
			_g = mul255(a, g - _g) + _g;
			_b = mul255(a, b - _b) + _b;
			dst[0] = _r;
			dst[1] = _g;
			dst[2] = _b;
			dst[3] = 0xFF;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_bgra(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _a, _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}

		if (a) {
			_b = dst[0];
			_g = dst[1];
			_r = dst[2];
			if (dst[3]) {
				_a = mul255(a, a) + mul255(0xFF-a, 0xFF);
				_r = mul255(a, r - _r) + _r;
				_g = mul255(a, g - _g) + _g;
				_b = mul255(a, b - _b) + _b;
				dst[0] = _b;
				dst[1] = _g;
				dst[2] = _r;
				dst[3] = _a;
			} else {
				dst[0] = b;
				dst[1] = g;
				dst[2] = r;
				dst[3] = a;
			}
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void merge_row_rgba(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _a, _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}

		if (a) {
			_r = dst[0];
			_g = dst[1];
			_b = dst[2];
			if (dst[3]) {
				_a = mul255(a, a) + mul255(0xFF-a, 0xFF);
				_r = mul255(a, r - _r) + _r;
				_g = mul255(a, g - _g) + _g;
				_b = mul255(a, b - _b) + _b;
				dst[0] = _r;
				dst[1] = _g;
				dst[2] = _b;
				dst[3] = _a;
			} else {
				dst[0] = r;
				dst[1] = g;
				dst[2] = b;
				dst[3] = a;
			}
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void load_line_grey(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = dst_bits[1] = dst_bits[2] = *src_bits++;
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_alpha_grey(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*2 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = dst_bits[1] = dst_bits[2] = *src_bits++;
		dst_bits[3] = *src_bits++;
		dst_bits+=4;
	}
}

static GFINLINE u8 colmask(s32 a, s32 n)
{
    s32 mask = (1 << n) - 1;
    return (u8) (a & (0xff & ~mask)) | ((-((a >> n) & 1)) & mask);
}

static void load_line_rgb_555(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*3 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		u16 c = *((u16*)src_bits + i);
		dst_bits[0] = colmask(c >> (10 - 3), 3);
		dst_bits[1] = colmask(c >> (5 - 3), 3);
		dst_bits[2] = colmask(c << 3, 3);
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_rgb_565(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*3 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		u16 c = *((u16*)src_bits + i);
		dst_bits[0] = colmask(c >> (11 - 3), 3);
		dst_bits[1] = colmask(c >> (5 - 2), 2);
		dst_bits[2] = colmask(c << 3, 3);
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_rgb_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*3 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_bgr_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*3 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[2] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_rgb_32(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = *src_bits++;
		dst_bits += 4;
	}
}

static void load_line_rgbd(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = 0xFF;
		src_bits++;
		dst_bits += 4;
	}
}

static void load_line_rgbds(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = ( *src_bits++) & 0x80 ? 255 : 0;
		dst_bits += 4;
	}
}

static void load_line_argb(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[2] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[3] = *src_bits++;
		dst_bits += 4;
	}
}

static void load_line_bgr_32(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[2] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[3] = *src_bits++;
		dst_bits += 4;
	}
}

static void load_line_yv12(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV)
{
	u8 *pY;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 5*y_pitch*height/4;
	}

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset/2 + y_offset*y_pitch/4;
	pV += x_offset/2 + y_offset*y_pitch/4;
	gf_yuv_load_lines_planar((unsigned char*)dst_bits, 4*width, pY, pU, pV, y_pitch, y_pitch/2, width);
}

static void load_line_yv12_10(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV)
{
	u8 *pY;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 5*y_pitch*height/4;
	}

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset/2 + y_offset*y_pitch/4;
	pV += x_offset/2 + y_offset*y_pitch/4;
	gf_yuv_10_load_lines_planar((unsigned char*)dst_bits, 4*width, pY, pU, pV, y_pitch, y_pitch/2, width);
}

static void load_line_yuva(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, u8 *pA)
{
	u8 *pY;
	pY = (u8*)src_bits;
	if (!pU) {
		pU = (u8*)src_bits + y_pitch*height;
		pV = (u8*)src_bits + 5*y_pitch*height/4;
		pA = (u8*)src_bits + 3*y_pitch*height/2;
	}

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset/2 + y_offset*y_pitch/4;
	pV += x_offset/2 + y_offset*y_pitch/4;
	pA += x_offset + y_offset*y_pitch;
	gf_yuva_load_lines(dst_bits, 4*width, pY, pU, pV, pA, y_pitch, y_pitch/2, width);
}

static void load_line_yuyv(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u8 *pY, *pU, *pV;
	pY = (u8 *)src_bits + x_offset + y_offset*y_pitch;
	pU = (u8 *)pY + 1;
	pV = (u8 *)pY + 3;
	gf_yuv_load_lines_packed((unsigned char*)dst_bits, 4*width, pY, pU, pV, width);
}

/*Ivica patch - todo, align it with other YUV loader (2 lines at a time) to avoid fetching twice U and V*/
static void load_line_YUV420SP(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	s32 frameSize = width * height;
	s32 j, yp, uvp, y, y1192, r, g, b, u, v;
	u32 i;

	yp = (s32)(x_offset + y_offset * y_pitch / 1.5f);
	j = y_offset;

	uvp = frameSize + (j >> 1) * width, u = 0, v = 0;

	for (i=0; i<width; i++, yp++) {

		y = (0xff & ((int) src_bits[yp])) - 16;
		if (y < 0) y = 0;
		if ((i & 1) == 0)
		{
			v = (0xff & src_bits[uvp++]) - 128;
			u = (0xff & src_bits[uvp++]) - 128;
		}

		y1192 = 1192 * y;
		r = (y1192 + 1634 * v);
		g = (y1192 - 833 * v - 400 * u);
		b = (y1192 + 2066 * u);

		if (r < 0)
			r = 0;
		else if (r > 262143)
			r = 262143;
		if (g < 0)
			g = 0;
		else if (g > 262143)
			g = 262143;
		if (b < 0)
			b = 0;
		else if (b > 262143)
			b = 262143;

		*((u32*)dst_bits) = 0xff000000 | ((b << 6) & 0xff0000)
					| ((g >> 2) & 0xff00) | ((r >> 10) & 0xff);
		dst_bits+=4;
	}
}


static void gf_cmx_apply_argb(GF_ColorMatrix *_this, u8 *a_, u8 *r_, u8 *g_, u8 *b_);

//#define COLORKEY_MPEG4_STRICT

GF_EXPORT
GF_Err gf_stretch_bits(GF_VideoSurface *dst, GF_VideoSurface *src, GF_Window *dst_wnd, GF_Window *src_wnd, u8 alpha, Bool flip, GF_ColorKey *key, GF_ColorMatrix *cmat)
{
	u8 *tmp, *rows;
	u8 ka, kr, kg, kb, kl, kh;
	s32 src_row;
	u32 i, yuv_planar_type = 0;
	Bool no_memcpy;
	Bool force_load_odd_yuv_lines = 0;
	Bool yuv_init = 0;
	Bool has_alpha = (alpha!=0xFF) ? 1 : 0;
	u32 dst_bpp, dst_w_size;
	s32 pos_y, inc_y, inc_x, prev_row, x_off;
	u32 src_w, src_h, dst_w, dst_h;
	u8 *dst_bits = NULL, *dst_bits_prev = NULL, *dst_temp_bits = NULL;
	s32 dst_x_pitch = dst->pitch_x;

	copy_row_proto copy_row = NULL;
	load_line_proto load_line = NULL;

	if (cmat && (cmat->m[15] || cmat->m[16] || cmat->m[17] || (cmat->m[18]!=FIX_ONE) || cmat->m[19] )) has_alpha = 1;
	else if (key && (key->alpha<0xFF)) has_alpha = 1;

	switch (src->pixel_format) {
	case GF_PIXEL_GREYSCALE:
		load_line = load_line_grey;
		break;
	case GF_PIXEL_ALPHAGREY:
		load_line = load_line_alpha_grey;
		has_alpha = 1;
		break;
	case GF_PIXEL_RGB_555:
		load_line = load_line_rgb_555;
		break;
	case GF_PIXEL_RGB_565:
		load_line = load_line_rgb_565;
		break;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_RGBS:
		load_line = load_line_rgb_24;
		break;
	case GF_PIXEL_BGR_24:
		load_line = load_line_bgr_24;
		break;
	case GF_PIXEL_ARGB:
		has_alpha = 1;
		load_line = load_line_argb;
		break;
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGBAS:
		has_alpha = 1;
	case GF_PIXEL_RGB_32:
		load_line = load_line_rgb_32;
		break;
	case GF_PIXEL_RGBDS:
		load_line = load_line_rgbds;
		has_alpha = 1;
		break;
	case GF_PIXEL_RGBD:
		load_line = load_line_rgbd;
		break;
	case GF_PIXEL_BGR_32:
		load_line = load_line_bgr_32;
		break;
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		yuv2rgb_init();
		yuv_planar_type = 1;
		break;
	case GF_PIXEL_YV12_10:
		yuv2rgb_init();
		yuv_planar_type = 3;
		break;
	case GF_PIXEL_NV21:
		load_line = load_line_YUV420SP;
		break;
	case GF_PIXEL_YUVA:
		has_alpha = 1;
	case GF_PIXEL_YUVD:
		yuv_planar_type = 2;
		yuv2rgb_init();
		break;
	case GF_PIXEL_YUY2:
		yuv_planar_type = 0;
		yuv2rgb_init();
		load_line = load_line_yuyv;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	/*only RGB output supported*/
	switch (dst->pixel_format) {
	case GF_PIXEL_RGB_555:
		dst_bpp = sizeof(unsigned char)*2;
		copy_row = has_alpha ? merge_row_rgb_555 : copy_row_rgb_555;
		break;
	case GF_PIXEL_RGB_565:
		dst_bpp = sizeof(unsigned char)*2;
		copy_row = has_alpha ? merge_row_rgb_565 : copy_row_rgb_565;
		break;
	case GF_PIXEL_RGB_24:
		dst_bpp = sizeof(unsigned char)*3;
		copy_row = has_alpha ? merge_row_rgb_24 : copy_row_rgb_24;
		break;
	case GF_PIXEL_BGR_24:
		dst_bpp = sizeof(unsigned char)*3;
		copy_row = has_alpha ? merge_row_bgr_24 : copy_row_bgr_24;
		break;
	case GF_PIXEL_RGB_32:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_bgrx : copy_row_bgrx;
		break;
	case GF_PIXEL_ARGB:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_bgra : copy_row_bgrx;
		break;
	case GF_PIXEL_RGBD:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_bgrx : copy_row_rgbd;
		break;
	case GF_PIXEL_RGBA:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_rgba : copy_row_rgbx;
		break;
	case GF_PIXEL_BGR_32:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_rgbx : copy_row_rgbx;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	/*x_pitch 0 means linear framebuffer*/
	if (!dst_x_pitch) dst_x_pitch = dst_bpp;


	src_w = src_wnd ? src_wnd->w : src->width;
	src_h = src_wnd ? src_wnd->h : src->height;
	dst_w = dst_wnd ? dst_wnd->w : dst->width;
	dst_h = dst_wnd ? dst_wnd->h : dst->height;

	if (yuv_planar_type && (src_w%2)) src_w++;

	tmp = (u8 *) gf_malloc(sizeof(u8) * src_w * (yuv_planar_type ? 8 : 4) );
	rows = tmp;

	if ( (src_h / dst_h) * dst_h != src_h) force_load_odd_yuv_lines = 1;

	dst_bits = (u8 *) dst->video_buffer;

	pos_y = 0x10000;
	inc_y = (src_h << 16) / dst_h;
	inc_x = (src_w << 16) / dst_w;
	x_off = src_wnd ? src_wnd->x : 0;
	src_row = src_wnd ? src_wnd->y : 0;

	prev_row = -1;

	dst_bits = (u8 *) dst->video_buffer;
	if (dst_wnd) dst_bits += ((s32)dst_wnd->x) * dst_x_pitch + ((s32)dst_wnd->y) * dst->pitch_y;

	dst_w_size = dst_bpp*dst_w;

	/*small opt here: if we need to fetch data from destination, and if destination is
	hardware memory, we work on a copy of the destination line*/
	if (has_alpha && dst->is_hardware_memory)
		dst_temp_bits = (u8 *) gf_malloc(sizeof(u8) * dst_bpp * dst_w);

	kl = kh = ka = kr = kg = kb = 0;
	if (key) {
		ka = key->alpha;
		kr = key->r;
		kg = key->g;
		kb = key->b;
		kl = key->low;
		kh = key->high;
		if (kh==kl) kh++;
	}

	/*do NOT use memcpy if the target buffer is not in systems memory*/
	no_memcpy = (has_alpha || dst->is_hardware_memory || (dst_bpp!=dst_x_pitch)) ? 1 : 0;

	while (dst_h) {
		while ( pos_y >= 0x10000L ) {
			src_row++;
			pos_y -= 0x10000L;
		}
		/*new row, check if conversion is needed*/
		if (prev_row != src_row) {
			u32 the_row = src_row - 1;
			if (yuv_planar_type) {
				if (the_row % 2) {
					if (!yuv_init || force_load_odd_yuv_lines) {
						yuv_init = 1;
						the_row --;
						if (flip) the_row = src->height-2 - the_row;
						if (yuv_planar_type==1) {
							load_line_yv12(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *) src->u_ptr, (u8 *) src->v_ptr);
						} else if (yuv_planar_type==3) {
							load_line_yv12_10((char *) src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp,(u8 *) src->u_ptr, (u8 *) src->v_ptr);
						} else {
							load_line_yuva(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *) src->u_ptr, (u8 *) src->v_ptr, (u8 *) src->a_ptr);
						}
						the_row = src_row - 1;

						if (cmat) {
							for (i=0; i<2*src_w; i++) {
								u32 idx = 4*i;
								gf_cmx_apply_argb(cmat, (u8 *) &tmp[idx+3], (u8 *) &tmp[idx], (u8 *) &tmp[idx+1], (u8 *) &tmp[idx+2]);
							}
						}
						if (key) {
							for (i=0; i<2*src_w; i++) {
								u32 idx = 4*i;
								s32 thres, v;
								v = tmp[idx]-kr; thres = ABS(v);
								v = tmp[idx+1]-kg; thres += ABS(v);
								v = tmp[idx+2]-kb; thres += ABS(v);
								thres/=3;
#ifdef COLORKEY_MPEG4_STRICT
								if (thres < kl) tmp[idx+3] = 0;
								else if (thres <= kh) tmp[idx+3] = (thres-kl)*ka / (kh-kl);
#else
								if (thres < kh) tmp[idx+3] = 0;
#endif
								else tmp[idx+3] = ka;
							}
						}
					}
					rows = flip ? tmp : tmp + src_w * 4;
				} else {
					if (flip) the_row = src->height-2 - the_row;
					if (yuv_planar_type==1) {
						load_line_yv12(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *) src->u_ptr, (u8 *) src->v_ptr);
					} else if (yuv_planar_type==3) {
						load_line_yv12_10(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *) src->u_ptr, (u8 *) src->v_ptr);
					} else {
						load_line_yuva(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *) src->u_ptr,(u8 *)  src->v_ptr, (u8 *) src->a_ptr);
					}
					yuv_init = 1;
					rows = flip ? tmp + src_w * 4 : tmp;

					if (cmat) {
						for (i=0; i<2*src_w; i++) {
							u32 idx = 4*i;
							gf_cmx_apply_argb(cmat, &tmp[idx+3], &tmp[idx], &tmp[idx+1], &tmp[idx+2]);
						}
					}
					if (key) {
						for (i=0; i<2*src_w; i++) {
							u32 idx = 4*i;
							s32 thres, v;
							v = tmp[idx]-kr; thres = ABS(v);
							v = tmp[idx+1]-kg; thres += ABS(v);
							v = tmp[idx+2]-kb; thres += ABS(v);
							thres/=3;
#ifdef COLORKEY_MPEG4_STRICT
							if (thres < kl) tmp[idx+3] = 0;
							else if (thres <= kh) tmp[idx+3] = (thres-kl)*ka / (kh-kl);
#else
							if (thres < kh) tmp[idx+3] = 0;
#endif
							else tmp[idx+3] = ka;
						}
					}
				}
			} else {
				if (flip) the_row = src->height-1 - the_row;
				load_line((u8*)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp);
				rows = tmp;
				if (cmat) {
					for (i=0; i<src_w; i++) {
						u32 idx = 4*i;
						gf_cmx_apply_argb(cmat, &tmp[idx+3], &tmp[idx], &tmp[idx+1], &tmp[idx+2]);
					}
				}
				if (key) {
					for (i=0; i<src_w; i++) {
						u32 idx = 4*i;
						s32 thres, v;
						v = tmp[idx]-kr; thres = ABS(v);
						v = tmp[idx+1]-kg; thres += ABS(v);
						v = tmp[idx+2]-kb; thres += ABS(v);
						thres/=3;
#ifdef COLORKEY_MPEG4_STRICT
						if (thres < kl) tmp[idx+3] = 0;
						else if (thres <= kh) tmp[idx+3] = (thres-kl)*ka / (kh-kl);
#else
						if (thres < kh) tmp[idx+3] = 0;
#endif
						else tmp[idx+3] = ka;
					}
				}
			}
			/*FIXME - this should be configurable, and tested against each graphics card*/
			if (0&&dst_temp_bits) {
				/*load from video memory*/
				memcpy(dst_temp_bits, dst_bits, dst_w_size);
				/*merge*/
				copy_row(rows, src_w, dst_temp_bits, dst_w, inc_x, dst_x_pitch, alpha);
				/*copy to video memory*/
				memcpy(dst_bits, dst_temp_bits, dst_w_size);
			} else {
				copy_row(rows, src_w, dst_bits, dst_w, inc_x, dst_x_pitch, alpha);
			}
		}
		/*do NOT use memcpy if the target buffer is not in systems memory*/
		else if (no_memcpy) {
			copy_row(rows, src_w, dst_bits, dst_w, inc_x, dst_x_pitch, alpha);
		} else {
			memcpy(dst_bits, dst_bits_prev, dst_w_size);
		}

		pos_y += inc_y;
		prev_row = src_row;

		dst_bits_prev = dst_bits;
		dst_bits += dst->pitch_y;
		dst_h--;
	}
	if (dst_temp_bits) gf_free(dst_temp_bits);
	gf_free(tmp);
	return GF_OK;
}



/*
	COLOR MATRIX TOOLS
 */

GF_EXPORT
void gf_cmx_init(GF_ColorMatrix *_this)
{
	if (!_this) return;
	memset(_this->m, 0, sizeof(Fixed)*20);
	_this->m[0] = _this->m[6] = _this->m[12] = _this->m[18] = FIX_ONE;
	_this->identity = 1;
}


static void gf_cmx_identity(GF_ColorMatrix *_this)
{
	GF_ColorMatrix mat;
	gf_cmx_init(&mat);
	_this->identity = memcmp(_this->m, mat.m, sizeof(Fixed)*20) ? 0 : 1;
}

GF_EXPORT
void gf_cmx_set_all(GF_ColorMatrix *_this, Fixed *coefs)
{
	if (!_this || !coefs) return;
}

GF_EXPORT
void gf_cmx_set(GF_ColorMatrix *_this,
				 Fixed c1, Fixed c2, Fixed c3, Fixed c4, Fixed c5,
				 Fixed c6, Fixed c7, Fixed c8, Fixed c9, Fixed c10,
				 Fixed c11, Fixed c12, Fixed c13, Fixed c14, Fixed c15,
				 Fixed c16, Fixed c17, Fixed c18, Fixed c19, Fixed c20)
{
	if (!_this) return;
	_this->m[0] = c1; _this->m[1] = c2; _this->m[2] = c3; _this->m[3] = c4; _this->m[4] = c5;
	_this->m[5] = c6; _this->m[6] = c7; _this->m[7] = c8; _this->m[8] = c9; _this->m[9] = c10;
	_this->m[10] = c11; _this->m[11] = c12; _this->m[12] = c13; _this->m[13] = c14; _this->m[14] = c15;
	_this->m[15] = c16; _this->m[16] = c17; _this->m[17] = c18; _this->m[18] = c19; _this->m[19] = c20;
	gf_cmx_identity(_this);
}

GF_EXPORT
void gf_cmx_copy(GF_ColorMatrix *_this, GF_ColorMatrix *from)
{
	if (!_this || !from) return;
	memcpy(_this->m, from->m, sizeof(Fixed)*20);
	gf_cmx_identity(_this);
}


GF_EXPORT
void gf_cmx_multiply(GF_ColorMatrix *_this, GF_ColorMatrix *w)
{
	Fixed res[20];
	if (!_this || !w || w->identity) return;
	if (_this->identity) {
		gf_cmx_copy(_this, w);
		return;
	}

	res[0] = gf_mulfix(_this->m[0], w->m[0]) + gf_mulfix(_this->m[1], w->m[5]) + gf_mulfix(_this->m[2], w->m[10]) + gf_mulfix(_this->m[3], w->m[15]);
	res[1] = gf_mulfix(_this->m[0], w->m[1]) + gf_mulfix(_this->m[1], w->m[6]) + gf_mulfix(_this->m[2], w->m[11]) + gf_mulfix(_this->m[3], w->m[16]);
	res[2] = gf_mulfix(_this->m[0], w->m[2]) + gf_mulfix(_this->m[1], w->m[7]) + gf_mulfix(_this->m[2], w->m[12]) + gf_mulfix(_this->m[3], w->m[17]);
	res[3] = gf_mulfix(_this->m[0], w->m[3]) + gf_mulfix(_this->m[1], w->m[8]) + gf_mulfix(_this->m[2], w->m[13]) + gf_mulfix(_this->m[3], w->m[18]);
	res[4] = gf_mulfix(_this->m[0], w->m[4]) + gf_mulfix(_this->m[1], w->m[9]) + gf_mulfix(_this->m[2], w->m[14]) + gf_mulfix(_this->m[3], w->m[19]) + _this->m[4];

	res[5] = gf_mulfix(_this->m[5], w->m[0]) + gf_mulfix(_this->m[6], w->m[5]) + gf_mulfix(_this->m[7], w->m[10]) + gf_mulfix(_this->m[8], w->m[15]);
	res[6] = gf_mulfix(_this->m[5], w->m[1]) + gf_mulfix(_this->m[6], w->m[6]) + gf_mulfix(_this->m[7], w->m[11]) + gf_mulfix(_this->m[8], w->m[16]);
	res[7] = gf_mulfix(_this->m[5], w->m[2]) + gf_mulfix(_this->m[6], w->m[7]) + gf_mulfix(_this->m[7], w->m[12]) + gf_mulfix(_this->m[8], w->m[17]);
	res[8] = gf_mulfix(_this->m[5], w->m[3]) + gf_mulfix(_this->m[6], w->m[8]) + gf_mulfix(_this->m[7], w->m[13]) + gf_mulfix(_this->m[8], w->m[18]);
	res[9] = gf_mulfix(_this->m[5], w->m[4]) + gf_mulfix(_this->m[6], w->m[9]) + gf_mulfix(_this->m[7], w->m[14]) + gf_mulfix(_this->m[8], w->m[19]) + _this->m[9];

	res[10] = gf_mulfix(_this->m[10], w->m[0]) + gf_mulfix(_this->m[11], w->m[5]) + gf_mulfix(_this->m[12], w->m[10]) + gf_mulfix(_this->m[13], w->m[15]);
	res[11] = gf_mulfix(_this->m[10], w->m[1]) + gf_mulfix(_this->m[11], w->m[6]) + gf_mulfix(_this->m[12], w->m[11]) + gf_mulfix(_this->m[13], w->m[16]);
	res[12] = gf_mulfix(_this->m[10], w->m[2]) + gf_mulfix(_this->m[11], w->m[7]) + gf_mulfix(_this->m[12], w->m[12]) + gf_mulfix(_this->m[13], w->m[17]);
	res[13] = gf_mulfix(_this->m[10], w->m[3]) + gf_mulfix(_this->m[11], w->m[8]) + gf_mulfix(_this->m[12], w->m[13]) + gf_mulfix(_this->m[13], w->m[18]);
	res[14] = gf_mulfix(_this->m[10], w->m[4]) + gf_mulfix(_this->m[11], w->m[9]) + gf_mulfix(_this->m[12], w->m[14]) + gf_mulfix(_this->m[13], w->m[19]) + _this->m[14];

	res[15] = gf_mulfix(_this->m[15], w->m[0]) + gf_mulfix(_this->m[16], w->m[5]) + gf_mulfix(_this->m[17], w->m[10]) + gf_mulfix(_this->m[18], w->m[15]);
	res[16] = gf_mulfix(_this->m[15], w->m[1]) + gf_mulfix(_this->m[16], w->m[6]) + gf_mulfix(_this->m[17], w->m[11]) + gf_mulfix(_this->m[18], w->m[16]);
	res[17] = gf_mulfix(_this->m[15], w->m[2]) + gf_mulfix(_this->m[16], w->m[7]) + gf_mulfix(_this->m[17], w->m[12]) + gf_mulfix(_this->m[18], w->m[17]);
	res[18] = gf_mulfix(_this->m[15], w->m[3]) + gf_mulfix(_this->m[16], w->m[8]) + gf_mulfix(_this->m[17], w->m[13]) + gf_mulfix(_this->m[18], w->m[18]);
	res[19] = gf_mulfix(_this->m[15], w->m[4]) + gf_mulfix(_this->m[16], w->m[9]) + gf_mulfix(_this->m[17], w->m[14]) + gf_mulfix(_this->m[18], w->m[19]) + _this->m[19];
	memcpy(_this->m, res, sizeof(Fixed)*20);
	gf_cmx_identity(_this);
}

#define CLIP_COMP(val)	{ if (val<0) { val=0; } else if (val>FIX_ONE) { val=FIX_ONE;} }

static void gf_cmx_apply_argb(GF_ColorMatrix *_this, u8 *a_, u8 *r_, u8 *g_, u8 *b_)
{
	Fixed _a, _r, _g, _b, a, r, g, b;
	if (!_this || _this->identity) return;

	a = INT2FIX(*a_)/255;
	r = INT2FIX(*r_)/255;
	g = INT2FIX(*g_)/255;
	b = INT2FIX(*b_)/255;
	_r = gf_mulfix(r, _this->m[0]) + gf_mulfix(g, _this->m[1]) + gf_mulfix(b, _this->m[2]) + gf_mulfix(a, _this->m[3]) + _this->m[4];
	_g = gf_mulfix(r, _this->m[5]) + gf_mulfix(g, _this->m[6]) + gf_mulfix(b, _this->m[7]) + gf_mulfix(a, _this->m[8]) + _this->m[9];
	_b = gf_mulfix(r, _this->m[10]) + gf_mulfix(g, _this->m[11]) + gf_mulfix(b, _this->m[12]) + gf_mulfix(a, _this->m[13]) + _this->m[14];
	_a = gf_mulfix(r, _this->m[15]) + gf_mulfix(g, _this->m[16]) + gf_mulfix(b, _this->m[17]) + gf_mulfix(a, _this->m[18]) + _this->m[19];
	CLIP_COMP(_a);
	CLIP_COMP(_r);
	CLIP_COMP(_g);
	CLIP_COMP(_b);

	*a_ = FIX2INT(_a*255);
	*r_ = FIX2INT(_r*255);
	*g_ = FIX2INT(_g*255);
	*b_ = FIX2INT(_b*255);
}


GF_EXPORT
GF_Color gf_cmx_apply(GF_ColorMatrix *_this, GF_Color col)
{
	Fixed _a, _r, _g, _b, a, r, g, b;
	if (!_this || _this->identity) return col;

	a = INT2FIX(col>>24); a /= 255;
	r = INT2FIX((col>>16)&0xFF); r /= 255;
	g = INT2FIX((col>>8)&0xFF); g /= 255;
	b = INT2FIX((col)&0xFF); b /= 255;
	_r = gf_mulfix(r, _this->m[0]) + gf_mulfix(g, _this->m[1]) + gf_mulfix(b, _this->m[2]) + gf_mulfix(a, _this->m[3]) + _this->m[4];
	_g = gf_mulfix(r, _this->m[5]) + gf_mulfix(g, _this->m[6]) + gf_mulfix(b, _this->m[7]) + gf_mulfix(a, _this->m[8]) + _this->m[9];
	_b = gf_mulfix(r, _this->m[10]) + gf_mulfix(g, _this->m[11]) + gf_mulfix(b, _this->m[12]) + gf_mulfix(a, _this->m[13]) + _this->m[14];
	_a = gf_mulfix(r, _this->m[15]) + gf_mulfix(g, _this->m[16]) + gf_mulfix(b, _this->m[17]) + gf_mulfix(a, _this->m[18]) + _this->m[19];
	CLIP_COMP(_a);
	CLIP_COMP(_r);
	CLIP_COMP(_g);
	CLIP_COMP(_b);
	return GF_COL_ARGB(FIX2INT(_a*255),FIX2INT(_r*255),FIX2INT(_g*255),FIX2INT(_b*255));
}

GF_EXPORT
void gf_cmx_apply_fixed(GF_ColorMatrix *_this, Fixed *a, Fixed *r, Fixed *g, Fixed *b)
{
	u32 col = GF_COL_ARGB_FIXED(*a, *r, *g, *b);
	col = gf_cmx_apply(_this, col);
	*a = INT2FIX(GF_COL_A(col)) / 255;
	*r = INT2FIX(GF_COL_R(col)) / 255;
	*g = INT2FIX(GF_COL_G(col)) / 255;
	*b = INT2FIX(GF_COL_B(col)) / 255;
}



#ifdef WIN32
# include <intrin.h>
# define GPAC_HAS_SSE2
#else
# ifdef __SSE2__
#  include <emmintrin.h>
#  define GPAC_HAS_SSE2
# endif
#endif

#ifdef GPAC_HAS_SSE2

static GF_Err gf_color_write_yv12_10_to_yuv_intrin(GF_VideoSurface *vs_dst,  unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd)
{
	u32 i, j, w, h;
	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 5*src_stride * src_height/4;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y + _src_wnd->x;
		/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
		therefore force an even Y offset for U and V planes.*/
		pU = pU + (src_stride * (_src_wnd->y / 2) + _src_wnd->x) / 2;
		pV = pV + (src_stride * (_src_wnd->y / 2) + _src_wnd->x) / 2;
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = src_width;
		h = src_height;
	}

	if (vs_dst->pixel_format == GF_PIXEL_YV12) {
		__m128i val1, val2, val_dst, *src1, *src2, *dst;
		for (i=0; i<h; i++) {
			src1 = (__m128i *)(pY + i*src_stride);
			src2 = src1+1;
			dst = (__m128i *)(vs_dst->video_buffer + i*vs_dst->pitch_y);
			
			for (j=0; j<w/16; j++, src1+=2, src2+=2, dst++) {
				val1 = _mm_load_si128(src1);
				val1 = _mm_srli_epi16(val1, 2);				
				val2 = _mm_load_si128(src2);
				val2 = _mm_srli_epi16(val2, 2);
				val_dst = _mm_packus_epi16(val1, val2);
				_mm_store_si128(dst, val_dst);
			}
		}
		
		for (i=0; i<h/2; i++) {
			src1 = (__m128i *) (pU + i*src_stride/2);
			src2 = src1+1;
			dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2);

			for (j=0; j<w/32; j++, src1+=2, src2+=2, dst++) {
				val1 = _mm_load_si128(src1);
				val1 = _mm_srli_epi16(val1, 2);				
				val2 = _mm_load_si128(src2);
				val2 = _mm_srli_epi16(val2, 2);
				val_dst = _mm_packus_epi16(val1, val2);
				_mm_store_si128(dst, val_dst);
			}
		}

		for (i=0; i<h/2; i++) {
			src1 = (__m128i *) (pV + i*src_stride/2);
			src2 = src1+1;
			dst = (__m128i *)(vs_dst->video_buffer + 5*vs_dst->pitch_y * vs_dst->height/4  + i*vs_dst->pitch_y/2);
			
			for (j=0; j<w/32; j++, src1+=2, src2+=2, dst++) {
				val1 = _mm_load_si128(src1);
				val1 = _mm_srli_epi16(val1, 2);				
				val2 = _mm_load_si128(src2);
				val2 = _mm_srli_epi16(val2, 2);
				val_dst = _mm_packus_epi16(val1, val2);
				_mm_store_si128(dst, val_dst);
			}
		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}
#endif


GF_EXPORT
GF_Err gf_color_write_yv12_10_to_yuv(GF_VideoSurface *vs_dst,  unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd)
{
	u32 i, j, w, h;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = src_width;
		h = src_height;
	}


#ifdef GPAC_HAS_SSE2

#ifdef GPAC_64_BITS
#define GFINTCAST  (u64)
#else
#define GFINTCAST  (u32)
#endif

	if ( (w%32 == 0)
		&& (GFINTCAST (vs_dst->video_buffer + vs_dst->pitch_y)%8 == 0)
		&& (GFINTCAST (vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + vs_dst->pitch_y/2)%8 == 0)
		&& (GFINTCAST (pU + src_stride/2)%8 == 0)
		&& (GFINTCAST (pV + src_stride/2)%8 == 0)
	) {
		return gf_color_write_yv12_10_to_yuv_intrin(vs_dst, pY, pU, pV, src_stride, src_width, src_height, _src_wnd);
	}
#endif

	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 5*src_stride * src_height/4;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y + _src_wnd->x;
		/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
		therefore force an even Y offset for U and V planes.*/
		pU = pU + (src_stride * (_src_wnd->y / 2) + _src_wnd->x) / 2;
		pV = pV + (src_stride * (_src_wnd->y / 2) + _src_wnd->x) / 2;
	}

	if (vs_dst->pixel_format == GF_PIXEL_YV12) {
		for (i=0; i<h; i++) {
			u16 *src = (u16 *) (pY + i*src_stride);
			u8 *dst = (u8 *) vs_dst->video_buffer + i*vs_dst->pitch_y;

			for (j=0; j<w;j++) {
				*dst = (*src) >> 2;
				dst++;
				src++;
			}
		}

		for (i=0; i<h/2; i++) {
			u16 *src = (u16 *) (pU + i*src_stride/2);
			u8 *dst = (u8 *) vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2;

			for (j=0; j<w/2;j++) {
				*dst = (*src) >> 2;
				dst++;
				src++;
			}
		}

		for (i=0; i<h/2; i++) {
			u16 *src = (u16 *) (pV + i*src_stride/2);
			u8 *dst = (u8 *) vs_dst->video_buffer + 5*vs_dst->pitch_y * vs_dst->height/4  + i*vs_dst->pitch_y/2;

			for (j=0; j<w/2;j++) {
				*dst = (*src) >> 2;
				dst++;
				src++;
			}
		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

