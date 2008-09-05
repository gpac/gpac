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


#include <gpac/user.h>
#include <gpac/constants.h>
#include <gpac/color.h>


/* original YUV table code from XviD colorspace module */

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



#define col_clip(a) MAX(0, MIN(255, a))

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

static void gf_yuv_load_lines(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 y_stride, s32 uv_stride, s32 width)
{
	u32 x, hw;
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



static void gf_yuva_load_lines(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char *v_src, unsigned char *a_src,
				 s32 y_stride, s32 uv_stride, s32 width)
{
	u32 x, hw;
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
typedef void (*load_line_proto)(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits);

static void copy_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	s32 pos;
	u16 *dst = (u16 *)_dst;
	u8 a, r, g, b;
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
	u8 a, r, g, b;
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
	u8 a, r, g, b;

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
	u8 a, r, g, b;

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

static void copy_row_rgb_32(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u8 a, r, g, b;
	s32 pos = 0x10000L;
	u32 *dst = (u32 *)_dst;
	
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_ARGB(a, r, g, b);
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_bgr_32(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u8 a, r, g, b;
	s32 pos = 0x10000L;
	u32 *dst = (u32 *)_dst;
	
	while ( dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_ARGB(b, g, r, a);
		dst+=x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a, r, g, b;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;

	
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
	u32 _r, _g, _b, a, r, g, b;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;

	
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
	u32 _r, _g, _b, a, r, g, b;
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
	u32 _r, _g, _b, a, r, g, b;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
		}

		if (a && alpha) {
			_r = dst[0]; _g = dst[0]; _b = dst[0];
			a = mul255(a, alpha);
			dst[2] = mul255(a, r - _r) + _r;
			dst[1] = mul255(a, g - _g) + _g;
			dst[0] = mul255(a, b - _b) + _b;

		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_rgb_32(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _r, _g, _b, a, r, g, b, col;
	s32 pos;
	u32 *dst = (u32 *)_dst;
	
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			a = mul255(a, alpha);
			pos -= 0x10000L;
		}

		if (a) {
			col = *dst;
			_r = GF_COL_R(col);
			_g = GF_COL_G(col);
			_b = GF_COL_B(col);
			_r = mul255(a, r - _r) + _r;
			_g = mul255(a, g - _g) + _g;
			_b = mul255(a, b - _b) + _b;
			*dst = GF_COL_ARGB(0xFF, _r, _g, _b);
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void merge_row_argb_32(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha)
{
	u32 _a, _r, _g, _b, a, r, g, b, col;
	s32 pos;
	u32 *dst = (u32 *)_dst;
	
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++; g = *src++; b = *src++; a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}

		if (a) {
			col = *dst;
			_r = GF_COL_R(col);
			_g = GF_COL_G(col);
			_b = GF_COL_B(col);
			if (GF_COL_A(col)) {
				_a = mul255(a, a) + mul255(0xFF-a, 0xFF);
				_r = mul255(a, r - _r) + _r;
				_g = mul255(a, g - _g) + _g;
				_b = mul255(a, b - _b) + _b;
				*dst = GF_COL_ARGB(_a, _r, _g, _b);
			} else {
				*dst = GF_COL_ARGB(a, r, g, b);
			}
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static GFINLINE u8 colmask(s32 a, s32 n)
{
    s32 mask = (1 << n) - 1;
    return (u8) (a & (0xff & ~mask)) | ((-((a >> n) & 1)) & mask);
}

static void load_line_rgb_555(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_rgb_565(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_rgb_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_bgr_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_rgb_32(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_argb(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
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

static void load_line_bgr_32(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u8 *dst_bits)
{
	src_bits += x_offset*4 + y_offset*y_pitch;
	memcpy(dst_bits, src_bits, sizeof(char)*4*width);
}

static void load_line_yv12(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u8 *pY, *pU, *pV;
	pY = (u8 *)src_bits;
	pU = (u8 *)src_bits + y_pitch*height;
	pV = (u8 *)src_bits + 5*y_pitch*height/4;
	pY += x_offset + y_offset*y_pitch;
	pU += x_offset/2 + y_offset*y_pitch/4;
	pV += x_offset/2 + y_offset*y_pitch/4;
	gf_yuv_load_lines((unsigned char*)dst_bits, 4*width, pY, pU, pV, y_pitch, y_pitch/2, width);
}

static void load_line_yuva(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits)
{
	u8 *pY, *pU, *pV, *pA;
	pY = (u8*)src_bits;
	pU = (u8*)src_bits + y_pitch*height;
	pV = (u8*)src_bits + 5*y_pitch*height/4;
	pA = (u8*)src_bits + 3*y_pitch*height/2;

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset/2 + y_offset*y_pitch/4;
	pV += x_offset/2 + y_offset*y_pitch/4;
	pA += x_offset + y_offset*y_pitch;
	gf_yuva_load_lines(dst_bits, 4*width, pY, pU, pV, pA, y_pitch, y_pitch/2, width);
}

static void gf_cmx_apply_argb(GF_ColorMatrix *_this, u8 *a_, u8 *r_, u8 *g_, u8 *b_);

GF_EXPORT
GF_Err gf_stretch_bits(GF_VideoSurface *dst, GF_VideoSurface *src, GF_Window *dst_wnd, GF_Window *src_wnd, s32 dst_x_pitch, u8 alpha, Bool flip, GF_ColorKey *key, GF_ColorMatrix *cmat)
{
	u8 *tmp, *rows;
	u8 ka, kr, kg, kb, kl, kh;
	s32 src_row;
	u32 i, yuv_type = 0;
	Bool yuv_init = 0;
	Bool has_alpha = (alpha!=0xFF) ? 1 : 0;
	u32 dst_bpp, dst_w_size;
	s32 pos_y, inc_y, inc_x, prev_row, x_off;
	u32 src_w, src_h, dst_w, dst_h;
	u8 *src_bits = NULL, *dst_bits = NULL, *dst_bits_prev = NULL, *dst_temp_bits = NULL;
	copy_row_proto copy_row = NULL;
	load_line_proto load_line = NULL;

	if (cmat && (cmat->m[15] || cmat->m[16] || cmat->m[17] || (cmat->m[18]!=FIX_ONE) || cmat->m[19] )) has_alpha = 1;
	else if (key && (key->alpha<0xFF)) has_alpha = 1;

	switch (src->pixel_format) {
	case GF_PIXEL_RGB_555:
		load_line = load_line_rgb_555;
		break;
	case GF_PIXEL_RGB_565:
		load_line = load_line_rgb_565;
		break;
	case GF_PIXEL_RGB_24:
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
		has_alpha = 1;
	case GF_PIXEL_RGB_32:
		load_line = load_line_rgb_32;
		break;
	case GF_PIXEL_BGR_32:
		load_line = load_line_bgr_32;
		break;
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		yuv2rgb_init();
		yuv_type = 1;
		break;
	case GF_PIXEL_YUVA:
		has_alpha = 1;
		yuv_type = 2;
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
		copy_row = has_alpha ? merge_row_rgb_32: copy_row_rgb_32;
		break;
	case GF_PIXEL_ARGB:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_argb_32: copy_row_rgb_32;
		break;
	case GF_PIXEL_RGBA:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_argb_32: copy_row_bgr_32;
		break;
	case GF_PIXEL_BGR_32:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_rgb_32: copy_row_bgr_32;
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

	if (yuv_type && (src_w%2)) src_w++;

	tmp = (u8 *) malloc(sizeof(u8) * src_w * (yuv_type ? 8 : 4) );
	rows = tmp;

	src_bits = (u8 *) src->video_buffer;
	dst_bits = (u8 *) dst->video_buffer;

	pos_y = 0x10000;
	inc_y = (src_h << 16) / dst_h;
	inc_x = (src_w << 16) / dst_w;
	x_off = src_wnd ? src_wnd->x : 0;
	src_row = src_wnd ? src_wnd->y : 0;

	prev_row = -1;

	dst_bits = (u8 *) dst->video_buffer;
	if (dst_wnd) dst_bits += ((s32)dst_wnd->x) * dst_x_pitch + ((s32)dst_wnd->y) * dst->pitch;

	dst_w_size = dst_bpp*dst_w;

	/*small opt here: if we need to fetch data from destination, and if destination is 
	hardware memory, we work on a copy of the destination line*/
	if (has_alpha && dst->is_hardware_memory)
		dst_temp_bits = (u8 *) malloc(sizeof(u8) * dst_bpp * dst_w);


	/*for 2 and 4 bytes colors, precompute pitch for u16 and u32 type casting*/
	if ((dst_bpp==2) || (dst_bpp==4) ) dst_x_pitch /= (s32) dst_bpp;

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

	while (dst_h) {
		while ( pos_y >= 0x10000L ) {
			src_row++;
			pos_y -= 0x10000L;
		}
		/*new row, check if conversion is needed*/
		if (prev_row != src_row) {
			u32 the_row = src_row - 1;
			if (yuv_type) {
				if ( the_row % 2) {
					if (!yuv_init) {
						yuv_init = 1;
						the_row --;
						if (flip) the_row = src->height-2 - the_row;
						if (yuv_type==1) {
							load_line_yv12(src->video_buffer, x_off, the_row, src->pitch, src_w, src->height, tmp);
						} else {
							load_line_yuva(src->video_buffer, x_off, the_row, src->pitch, src_w, src->height, tmp);
						}
						the_row = src_row - 1;
					}
					rows = flip ? tmp : tmp + src_w * 4;
				} else {
					if (flip) the_row = src->height-2 - the_row;
					if (yuv_type==1) {
						load_line_yv12(src->video_buffer, x_off, the_row, src->pitch, src_w, src->height, tmp);
					} else {
						load_line_yuva(src->video_buffer, x_off, the_row, src->pitch, src_w, src->height, tmp);
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
#if 0
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
				load_line((u8*)src->video_buffer, x_off, the_row, src->pitch, src_w, tmp);
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
						s32 thres;
						thres = (255 + (tmp[idx]-kr)) % 255;
						thres += (255 + (tmp[idx+1]-kg)) % 255;
						thres += (255 + (tmp[idx+2]-kb)) % 255;
						if (thres > 3*0xFE) tmp[idx+3] = ka;
//						if ( (tmp[idx]==kr) && (tmp[idx+1]==kg) && (tmp[idx+2]==kb)) tmp[idx+3] = ka;
					}
				}
			}
			if (dst_temp_bits) {
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
		else if (has_alpha || dst->is_hardware_memory) {
			copy_row(rows, src_w, dst_bits, dst_w, inc_x, dst_x_pitch, alpha);
		} else {
			memcpy(dst_bits, dst_bits_prev, dst_w_size);
		}
		
		pos_y += inc_y;
		prev_row = src_row;

		dst_bits_prev = dst_bits;
		dst_bits += dst->pitch;
		dst_h--;
	}
	if (dst_temp_bits) free(dst_temp_bits);
	free(tmp);
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
