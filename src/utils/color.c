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

#ifndef GPAC_DISABLE_PLAYER

static GF_Err color_write_nv12_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yv12_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, const GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yuv422_10_to_yuv422(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yuv422_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yuv444_10_to_yuv444(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yuv444_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_up);
static GF_Err color_write_yuv420_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv);
static GF_Err color_write_yuv422_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv);
static GF_Err color_write_yuv444_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv);
static GF_Err color_write_yvyu_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv);
static GF_Err color_write_rgb_to_24(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd);
static GF_Err color_write_rgb_to_32(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd);


static GFINLINE u8 colmask(s32 a, s32 n)
{
	s32 mask = (1 << n) - 1;
	return (u8) (a & (0xff & ~mask)) | ((-((a >> n) & 1)) & mask);
}


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

static void yuv_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *) dst + dststride;
	unsigned char *y_src2 = (unsigned char *) y_src + y_stride;

	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = dst[4] = dst2[0] = dst2[4] = v_src[x];
			dst[1] = dst[5] = dst2[1] = dst2[5] = u_src[x];

			dst[2] = *y_src;
			dst[3] = 0xFF;
			y_src++;

			dst[6] = *y_src;
			dst[7] = 0xFF;
			y_src++;

			dst2[2] = *y_src2;
			dst2[3] = 0xFF;
			y_src2++;

			dst2[6] = *y_src2;
			dst2[7] = 0xFF;
			y_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
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
static void yuv422_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *)dst + dststride;
	unsigned char *y_src2 = (unsigned char *)y_src + y_stride;
	unsigned char *u_src2 = (unsigned char *)u_src + uv_stride;
	unsigned char *v_src2 = (unsigned char *)v_src + uv_stride;

	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = dst[4] = *v_src;
			dst[1] = dst[5] = *u_src;
			dst[2] = *y_src;
			y_src++;
			dst[3] = 0xFF;
			dst[6] = *y_src;
			y_src++;
			dst[7] = 0xFF;

			u_src++;
			v_src++;

			dst2[0] = dst2[4] = *v_src2;
			dst2[1] = dst2[5] = *u_src2;
			dst2[2] = *y_src;
			y_src2++;
			dst2[3] = 0xFF;
			dst2[6] = *y_src;
			y_src2++;
			dst2[7] = 0xFF;

			u_src2++;
			v_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}

	for (x = 0; x < hw; x++) {
		s32 b_u, g_uv, r_v, rgb_y;

		b_u = B_U[*u_src];
		g_uv = G_U[*u_src] + G_V[*v_src];
		r_v = R_V[*v_src];
		rgb_y = RGB_Y[*y_src];
		dst[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;

		rgb_y = RGB_Y[*y_src];
		dst[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;
		u_src++;
		v_src++;

		b_u = B_U[*u_src2];
		g_uv = G_U[*u_src2] + G_V[*v_src2];
		r_v = R_V[*v_src2];
		rgb_y = RGB_Y[*y_src2];
		dst2[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;

		rgb_y = RGB_Y[*y_src2];
		dst2[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;

		dst += 8;
		dst2 += 8;
	}
}
static void yuv444_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *)dst + dststride;
	unsigned char *y_src2 = (unsigned char *)y_src + y_stride;
	unsigned char *u_src2 = (unsigned char *)u_src + uv_stride;
	unsigned char *v_src2 = (unsigned char *)v_src + uv_stride;

	hw = width / 2;

	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = *v_src;
			dst[1] = *u_src;
			dst[2] = *y_src;
			dst[3] = 0xFF;

			y_src++;
			u_src++;
			v_src++;

			dst[4] = *v_src;
			dst[5] = *u_src;
			dst[6] = *y_src;
			dst[7] = 0xFF;

			y_src++;
			u_src++;
			v_src++;

			dst2[0] = *v_src2;
			dst2[1] = *u_src2;
			dst2[2] = *y_src2;
			dst2[3] = 0xFF;

			y_src2++;
			u_src2++;
			v_src2++;

			dst2[4] = *v_src2;
			dst2[5] = *u_src2;
			dst2[6] = *y_src2;
			dst2[7] = 0xFF;

			y_src2++;
			u_src2++;
			v_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}

	for (x = 0; x < hw; x++) {
		s32 b_u, g_uv, r_v, rgb_y;


		b_u = B_U[*u_src];
		g_uv = G_U[*u_src] + G_V[*v_src];
		r_v = R_V[*v_src];
		rgb_y = RGB_Y[*y_src];
		dst[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;
		u_src++;
		v_src++;


		b_u = B_U[*u_src];
		g_uv = G_U[*u_src] + G_V[*v_src];
		r_v = R_V[*v_src];
		rgb_y = RGB_Y[*y_src];
		dst[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;
		u_src++;
		v_src++;


		b_u = B_U[*u_src2];
		g_uv = G_U[*u_src2] + G_V[*v_src2];
		r_v = R_V[*v_src2];
		rgb_y = RGB_Y[*y_src2];
		dst2[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;


		b_u = B_U[*u_src2];
		g_uv = G_U[*u_src2] + G_V[*v_src2];
		r_v = R_V[*v_src2];
		rgb_y = RGB_Y[*y_src2];

		dst2[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;

		dst += 8;
		dst2 += 8;
	}
}

static void yuv_10_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *_y_src, unsigned char *_u_src, unsigned char *_v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *) dst + dststride;
	unsigned short *y_src2 = (unsigned short *) (_y_src + y_stride);
	unsigned short *y_src = (unsigned short *)_y_src;
	unsigned short *u_src = (unsigned short *)_u_src;
	unsigned short *v_src = (unsigned short *)_v_src;


	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = dst[4] = dst2[0] = dst2[4] = v_src[x] >> 2;
			dst[1] = dst[5] = dst2[1] = dst2[5] = u_src[x] >> 2;
			dst[2] = *y_src >> 2;
			y_src++;
			dst[3] = 0xFF;

			dst[6] = *y_src >> 2;
			y_src++;
			dst[7] = 0xFF;

			dst2[2] = *y_src2 >> 2;
			y_src2++;
			dst2[3] = 0xFF;

			dst2[6] = *y_src2 >> 2;
			y_src2++;
			dst2[7] = 0xFF;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
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
static void yuv422_10_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *_y_src, unsigned char *_u_src, unsigned char *_v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *)dst + dststride;
	unsigned short *y_src2 = (unsigned short *)(_y_src + y_stride);
	unsigned short *u_src2 = (unsigned short *)(_u_src + uv_stride);
	unsigned short *v_src2 = (unsigned short *)(_v_src + uv_stride);
	unsigned short *y_src = (unsigned short *)_y_src;
	unsigned short *u_src = (unsigned short *)_u_src;
	unsigned short *v_src = (unsigned short *)_v_src;

	hw = width / 2;

	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = dst[4] = *v_src >> 2;
			dst[1] = dst[5] = *u_src >> 2;
			dst[2] = *y_src >> 2;
			y_src++;
			dst[3] = 0xFF;

			dst[6] = *y_src >> 2;
			y_src++;
			dst[7] = 0xFF;

			dst2[0] = dst2[4] = *v_src2 >> 2;
			dst2[1] = dst2[5] = *u_src2 >> 2;
			dst2[2] = *y_src2 >> 2;
			y_src2++;
			dst2[3] = 0xFF;

			dst2[6] = *y_src2 >> 2;
			y_src2++;
			dst2[7] = 0xFF;

			y_src2++;
			u_src2++;
			v_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
	for (x = 0; x < hw; x++) {
		s32 b_u, g_uv, r_v, rgb_y;

		b_u = B_U[*u_src >> 2];
		g_uv = G_U[*u_src >> 2] + G_V[*v_src >> 2];
		r_v = R_V[*v_src >> 2];
		rgb_y = RGB_Y[*y_src >> 2];
		dst[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;




		rgb_y = RGB_Y[*y_src >> 2];
		dst[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;
		u_src++;
		v_src++;


		b_u = B_U[*u_src2 >> 2];
		g_uv = G_U[*u_src2 >> 2] + G_V[*v_src2 >> 2];
		r_v = R_V[*v_src2 >> 2];
		rgb_y = RGB_Y[*y_src2 >> 2];
		dst2[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;

		rgb_y = RGB_Y[*y_src2 >> 2];
		dst2[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;

		dst += 8;
		dst2 += 8;
	}
}
static void yuv444_10_load_lines_planar(unsigned char *dst, s32 dststride, unsigned char *_y_src, unsigned char *_u_src, unsigned char *_v_src, s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *)dst + dststride;
	unsigned short * y_src2 = (unsigned short *)(_y_src + y_stride);
	unsigned short * u_src2 = (unsigned short *)(_u_src + uv_stride);
	unsigned short * v_src2 = (unsigned short *)(_v_src + uv_stride);
	unsigned short * y_src = (unsigned short *)_y_src;
	unsigned short * u_src = (unsigned short *)_u_src;
	unsigned short * v_src = (unsigned short *)_v_src;

	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {
			dst[0] = *v_src >> 2;
			dst[1] = *u_src >> 2;
			dst[2] = *y_src >> 2;
			dst[3] = 0xFF;
			y_src++;
			u_src++;
			v_src++;

			dst[4] = *v_src >> 2;
			dst[5] = *u_src >> 2;
			dst[6] = *y_src >> 2;
			dst[7] = 0xFF;
			y_src++;
			u_src++;
			v_src++;

			dst2[0] = *v_src2 >> 2;
			dst2[1] = *u_src2 >> 2;
			dst2[2] = *y_src2 >> 2;
			dst2[3] = 0xFF;
			y_src2++;
			u_src2++;
			v_src2++;

			dst2[4] = *v_src2 >> 2;
			dst2[5] = *u_src2 >> 2;
			dst2[6] = *y_src2 >> 2;
			dst2[7] = 0xFF;
			y_src2++;
			u_src2++;
			v_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
	for (x = 0; x < hw; x++) {
		s32 b_u, g_uv, r_v, rgb_y;


		b_u = B_U[*u_src >> 2];
		g_uv = G_U[*u_src >> 2] + G_V[*v_src >> 2];
		r_v = R_V[*v_src >> 2];
		rgb_y = RGB_Y[*y_src >> 2];
		dst[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[3] = 0xFF;
		y_src++;
		u_src++;
		v_src++;


		b_u = B_U[*u_src >> 2];
		g_uv = G_U[*u_src >> 2] + G_V[*v_src >> 2];
		r_v = R_V[*v_src >> 2];
		rgb_y = RGB_Y[*y_src >> 2];
		dst[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst[7] = 0xFF;
		y_src++;
		u_src++;
		v_src++;


		b_u = B_U[*u_src2 >> 2];
		g_uv = G_U[*u_src2 >> 2] + G_V[*v_src2 >> 2];
		r_v = R_V[*v_src2 >> 2];
		rgb_y = RGB_Y[*y_src2 >> 2];
		dst2[0] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[1] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[2] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[3] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;


		b_u = B_U[*u_src2 >> 2];
		g_uv = G_U[*u_src2 >> 2] + G_V[*v_src2 >> 2];
		r_v = R_V[*v_src2 >> 2];
		rgb_y = RGB_Y[*y_src2 >> 2];

		dst2[4] = col_clip((rgb_y + r_v) >> SCALEBITS_OUT);
		dst2[5] = col_clip((rgb_y - g_uv) >> SCALEBITS_OUT);
		dst2[6] = col_clip((rgb_y + b_u) >> SCALEBITS_OUT);
		dst2[7] = 0xFF;
		y_src2++;
		u_src2++;
		v_src2++;

		dst += 8;
		dst2 += 8;
	}
}

static void yuv_load_lines_packed(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char * v_src, s32 width, Bool dst_yuv)
{
	u32 hw;

	hw = width / 2;
	if (dst_yuv) {
		while (hw) {
			hw--;

			dst[0] = dst[4] = *u_src;
			dst[1] = dst[5] = *v_src;
			dst[2] = *y_src;
			dst[3] = 0xFF;
			dst[6] = *(y_src+2);
			dst[7] = 0xFF;

			dst += 8;
			y_src += 4;
			u_src += 4;
			v_src += 4;
		}
		return;
	}
	while (hw) {
		s32 b_u, g_uv, r_v, rgb_y;
		hw--;

		b_u = B_U[*u_src];
		g_uv = G_U[*u_src] + G_V[*v_src];
		r_v = R_V[*v_src];

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


static void yuva_load_lines(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char *v_src, unsigned char *a_src,
                               s32 y_stride, s32 uv_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = dst + dststride;
	unsigned char *y_src2 = y_src + y_stride;
	unsigned char *a_src2 = a_src + y_stride;

	yuv2rgb_init();

	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {

			dst[0] = dst[4] = dst2[0] = dst2[4] = v_src[x];
			dst[1] = dst[5] = dst2[1] = dst2[5] = u_src[x];

			dst[2] = *y_src;
			dst[3] = *a_src;
			y_src++;
			a_src++;

			dst[6] = *y_src;
			dst[7] = *a_src;
			y_src++;
			a_src++;

			dst2[2] = *y_src2;
			dst2[3] = *a_src2;
			y_src2++;
			a_src2++;

			dst2[6] = *y_src2;
			dst2[7] = *a_src2;
			y_src2++;
			a_src2++;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
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

typedef void (*copy_row_proto)(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height);
typedef void (*load_line_proto)(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 src_width, u32 src_height, u8 *dst_bits, Bool dst_yuv);

static void copy_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	s32 pos;
	u16 *dst = (u16 *)_dst;
	u8 a=0, r=0, g=0, b=0;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_555(r, g, b);
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_rgb_565(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	s32 pos;
	u16 *dst = (u16 *)_dst;
	u8 a=0, r=0, g=0, b=0;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) *dst = GF_COL_565(r, g, b);
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


static void copy_row_rgb_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	s32 pos;
	u8 a=0, r=0, g=0, b=0;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			dst[0] = r;
			dst[1] = g;
			dst[2] = b;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_bgr_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	s32 pos;
	u8 a=0, r=0, g=0, b=0;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			dst[0] = b;
			dst[1] = g;
			dst[2] = r;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_bgrx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void copy_row_argb(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			dst[0] = 0xFF;
			dst[1] = r;
			dst[2] = g;
			dst[3] = b;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void copy_row_rgbx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while ( dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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

static void copy_row_rgbd(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u8 a=0, r=0, g=0, b=0;
	s32 pos = 0x10000L;

	while ( dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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
#if 0
static void copy_row_yuv444(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	s32 pos;
	u8 *dY, *dU, *dV;
	u8 a=0, y=0, u=0, v=0;

	dY = dst;
	dU = dst + dst_height * dst_pitch;
	dV = dU + dst_height * dst_pitch;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			v = *src++;
			u = *src++;
			y = *src++;
			a = *src++;
			pos -= 0x10000L;
		}
		if (a) {
			*dV = v;
			*dU = u;
			*dY = y;
		}
		dY++;
		dU++;
		dV++;

		pos += h_inc;
		dst_w--;
	}
}
#endif

static void merge_row_rgb_555(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a && alpha) {
			col = *dst;
			_r = colmask(col >> (10 - 3), 3);
			_g = colmask(col >> (5 - 3), 3);
			_b = colmask(col << 3, 3);

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

static void merge_row_rgb_565(u8 *src, u32 src_w, u8 *_dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	u16 col, *dst = (u16 *)_dst;
	x_pitch /= 2;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void merge_row_rgb_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a) {
			_r = dst[0];
			_g = dst[0];
			_b = dst[0];
			dst[0] = mul255(a, r - _r) + _r;
			dst[1] = mul255(a, g - _g) + _g;
			dst[2] = mul255(a, b - _b) + _b;
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}


#if 0
static void merge_row_yuv444(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	u8 *dY, *dU, *dV;

	dY = dst;
	dU = dst + dst_height * dst_pitch;
	dV = dU + dst_height * dst_pitch;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}
		if (a) {
			_r = dY[0];
			_g = dU[0];
			_b = dV[0];
			dY[0] = mul255(a, r - _r) + _r;
			dU[1] = mul255(a, g - _g) + _g;
			dV[2] = mul255(a, b - _b) + _b;
		}
		dY++;
		dU++;
		dV++;

		pos += h_inc;
		dst_w--;
	}
}
#endif

static void merge_row_bgr_24(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void merge_row_bgrx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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

static void merge_row_rgbx(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void merge_row_bgra(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _a, _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void merge_row_argb(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _a, _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;

	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
			pos -= 0x10000L;
			a = mul255(a, alpha);
		}

		if (a) {
			_r = dst[1];
			_g = dst[2];
			_b = dst[3];
			if (dst[0]) {
				_a = mul255(a, a) + mul255(0xFF-a, 0xFF);
				_r = mul255(a, r - _r) + _r;
				_g = mul255(a, g - _g) + _g;
				_b = mul255(a, b - _b) + _b;
				dst[0] = _a;
				dst[1] = _r;
				dst[2] = _g;
				dst[3] = _b;
			} else {
				dst[0] = a;
				dst[1] = b;
				dst[2] = g;
				dst[3] = r;
			}
		}
		dst += x_pitch;
		pos += h_inc;
		dst_w--;
	}
}

static void merge_row_rgba(u8 *src, u32 src_w, u8 *dst, u32 dst_w, s32 h_inc, s32 x_pitch, u8 alpha, u32 dst_pitch, u32 dst_height)
{
	u32 _a, _r, _g, _b, a=0, r=0, g=0, b=0;
	s32 pos;
	pos = 0x10000;
	while (dst_w) {
		while ( pos >= 0x10000L ) {
			r = *src++;
			g = *src++;
			b = *src++;
			a = *src++;
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


static void load_line_grey(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = dst_bits[1] = dst_bits[2] = *src_bits++;
		dst_bits[3] = 0xFF;
		dst_bits+=4;
	}
}

static void load_line_alpha_grey(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*2 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = dst_bits[1] = dst_bits[2] = *src_bits++;
		dst_bits[3] = *src_bits++;
		dst_bits+=4;
	}
}

static void load_line_grey_alpha(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*2 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[3] = *src_bits++;
		dst_bits[0] = dst_bits[1] = dst_bits[2] = *src_bits++;
		dst_bits+=4;
	}
}

static void load_line_rgb_555(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_rgb_565(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_rgb_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_bgr_24(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_rgb_32(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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
static void load_line_xrgb(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = 0xFF;
		dst_bits += 4;
	}
}
static void load_line_bgrx(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[2] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[3] = 0xFF;
		src_bits++;
		dst_bits += 4;
	}
}

static void load_line_rgbd(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_rgbds(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits[3] = (( *src_bits++) & 0x80) ? 255 : 0;
		dst_bits += 4;
	}
}

static void load_line_bgra(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
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

static void load_line_argb(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u32 i;
	src_bits += x_offset*4 + y_offset*y_pitch;
	for (i=0; i<width; i++) {
		dst_bits[3] = *src_bits++;
		dst_bits[0] = *src_bits++;
		dst_bits[1] = *src_bits++;
		dst_bits[2] = *src_bits++;
		dst_bits += 4;
	}
}
static void load_line_yv12(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
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
	yuv_load_lines_planar((unsigned char*)dst_bits, 4*width, pY, pU, pV, y_pitch, y_pitch/2, width, dst_yuv);
}
static void load_line_yuv422(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
{
	u8 *pY;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 3 * y_pitch*height / 2;
	}
	
	pY += x_offset + y_offset*y_pitch;
	pU += x_offset / 2 + y_offset*y_pitch / 2;
	pV += x_offset / 2 + y_offset*y_pitch / 2;
	yuv422_load_lines_planar((unsigned char*)dst_bits, 4 * width, pY, pU, pV, y_pitch, y_pitch / 2, width, dst_yuv);
}
static void load_line_yuv444(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
{
	u8 *pY;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 2 * y_pitch*height;
	}

	
	pY += x_offset + y_offset*y_pitch;
	pU += x_offset + y_offset*y_pitch;
	pV += x_offset + y_offset*y_pitch;
	yuv444_load_lines_planar((unsigned char*)dst_bits, 4 * width, pY, pU, pV, y_pitch, y_pitch, width, dst_yuv);
}
static void load_line_yv12_10(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
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
	yuv_10_load_lines_planar((unsigned char*)dst_bits, 4*width, pY, pU, pV, y_pitch, y_pitch/2, width, dst_yuv);
}
static void load_line_yuv422_10(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
{
	u8 *pY;
	u16  *src_y, *src_u, *src_v;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 3 * y_pitch*height / 2;
	}
	src_y = (u16 *)pY + x_offset;
	src_u = (u16 *)pU + x_offset / 2;
	src_v = (u16 *)pV + x_offset / 2;
	

	pY = (u8 *)src_y + y_offset*y_pitch;
	pU = (u8 *)src_u + y_offset*y_pitch / 2;
	pV = (u8 *)src_v + y_offset*y_pitch / 2;
	yuv422_10_load_lines_planar((unsigned char*)dst_bits, 4 * width, pY, pU, pV, y_pitch, y_pitch / 2, width, dst_yuv);
}
static void load_line_yuv444_10(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, Bool dst_yuv)
{
	u8 *pY;
	u16  *src_y, *src_u, *src_v;
	pY = (u8 *)src_bits;
	if (!pU) {
		pU = (u8 *)src_bits + y_pitch*height;
		pV = (u8 *)src_bits + 2 * y_pitch*height;
	}
	 src_y = (u16 *)pY + x_offset;
	 src_u = (u16 *)pU + x_offset;
	 src_v = (u16 *)pV + x_offset;
	

	pY = (u8 *)src_y + y_offset*y_pitch;
	pU = (u8 *)src_u + y_offset*y_pitch;
	pV = (u8 *)src_v + y_offset*y_pitch;
	yuv444_10_load_lines_planar((unsigned char*)dst_bits, 4 * width, pY, pU, pV, y_pitch, y_pitch, width, dst_yuv);
}
static void load_line_yuva(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, u8 *pV, u8 *pA, Bool dst_yuv)
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
	yuva_load_lines(dst_bits, 4*width, pY, pU, pV, pA, y_pitch, y_pitch/2, width, dst_yuv);
}

static void load_line_yuyv(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u8 *pY, *pU, *pV;
	pY = (u8 *)src_bits + x_offset + y_offset*y_pitch;
	pU = (u8 *)pY + 1;
	pV = (u8 *)pY + 3;
	yuv_load_lines_packed((unsigned char*)dst_bits, 4*width, pY, pU, pV, width, dst_yuv);
}
static void load_line_uyvy(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u8 *pY, *pU, *pV;
	pU = (u8 *)src_bits + x_offset + y_offset*y_pitch;
	pY = (u8 *)pU + 1;
	pV = (u8 *)pU + 2;
	yuv_load_lines_packed((unsigned char*)dst_bits, 4*width, pY, pU, pV, width, dst_yuv);
}
static void load_line_yvyu(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u8 *pY, *pU, *pV;
	pY = (u8 *)src_bits + x_offset + y_offset*y_pitch;
	pV = (u8 *)pY + 1;
	pU = (u8 *)pY + 3;
	yuv_load_lines_packed((unsigned char*)dst_bits, 4*width, pY, pU, pV, width, dst_yuv);
}
static void load_line_vyuy(u8 *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, Bool dst_yuv)
{
	u8 *pY, *pU, *pV;
	pV = (u8 *)src_bits + x_offset + y_offset*y_pitch;
	pY = (u8 *)pV + 1;
	pU = (u8 *)pV + 2;
	yuv_load_lines_packed((unsigned char*)dst_bits, 4*width, pY, pU, pV, width, dst_yuv);
}


static void gf_yuv_load_lines_nv12_nv21(unsigned char *dst, s32 dststride, unsigned char *y_src, unsigned char *u_src, unsigned char *v_src, s32 y_stride, s32 width, Bool dst_yuv)
{
	u32 hw, x;
	unsigned char *dst2 = (unsigned char *) dst + dststride;
	unsigned char *y_src2 = (unsigned char *) y_src + y_stride;

	hw = width / 2;
	if (dst_yuv) {
		for (x = 0; x < hw; x++) {

			dst[0] = dst[4] = dst2[0] = dst2[4] = v_src[2*x];
			dst[1] = dst[5] = dst2[1] = dst2[5] = u_src[2*x];
			dst[2] = *y_src;
			y_src++;
			dst[3] = 0xFF;

			dst[6] = *y_src;
			y_src++;
			dst[7] = 0xFF;

			dst2[2] = *y_src2;
			y_src2++;
			dst2[3] = 0xFF;

			dst2[6] = *y_src2;
			y_src2++;
			dst2[7] = 0xFF;

			dst += 8;
			dst2 += 8;
		}
		return;
	}
	for (x = 0; x < hw; x++) {
		s32 u, v;
		s32 b_u, g_uv, r_v, rgb_y;

		u = u_src[2*x];
		v = v_src[2*x];

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

static void load_line_nv12(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, Bool dst_yuv)
{
	u8 *pY = (u8*)src_bits;
	if (!pU) {
		pU = (u8*)src_bits + y_pitch*height;
	}

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset + y_offset*y_pitch/2; //half vertical sampling
	gf_yuv_load_lines_nv12_nv21(dst_bits, 4*width, pY, pU, pU + 1, y_pitch, width, dst_yuv);
}
static void load_line_nv21(char *src_bits, u32 x_offset, u32 y_offset, u32 y_pitch, u32 width, u32 height, u8 *dst_bits, u8 *pU, Bool dst_yuv)
{
	u8 *pY = (u8*)src_bits;
	if (!pU) {
		pU = (u8*)src_bits + y_pitch*height;
	}

	pY += x_offset + y_offset*y_pitch;
	pU += x_offset + y_offset*y_pitch/2; //half vertical sampling
	gf_yuv_load_lines_nv12_nv21(dst_bits, 4*width, pY, pU+1, pU, y_pitch, width, dst_yuv);
}

//#define COLORKEY_MPEG4_STRICT

static Bool format_is_yuv(u32 in_pf)
{
	switch (in_pf) {
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		return GF_TRUE;
		/*not supported yet*/
	case GF_PIXEL_YUVA:
	default:
		return GF_FALSE;
	}
}

GF_EXPORT
GF_Err gf_stretch_bits(GF_VideoSurface *dst, GF_VideoSurface *src, GF_Window *dst_wnd, GF_Window *src_wnd, u8 alpha, Bool flip, GF_ColorKey *key, GF_ColorMatrix *cmat)
{
	u8 *tmp, *rows;
	u8 ka=0, kr=0, kg=0, kb=0, kl=0, kh=0;
	s32 src_row;
	u32 i, yuv_planar_type = 0;
	Bool no_memcpy, dst_yuv = GF_FALSE;
	Bool force_load_odd_yuv_lines = GF_FALSE;
	Bool yuv_init = GF_FALSE;
	Bool has_alpha = (alpha!=0xFF) ? GF_TRUE : GF_FALSE;
	u32 dst_bpp, dst_w_size;
	s32 pos_y, inc_y, inc_x, prev_row, x_off;
	u32 src_w, src_h, dst_w, dst_h;
	u8 *dst_bits = NULL, *dst_bits_prev = NULL, *dst_temp_bits = NULL;
	s32 dst_x_pitch = dst->pitch_x;
	copy_row_proto copy_row = NULL;
	load_line_proto load_line = NULL;

	if (cmat && (cmat->m[15] || cmat->m[16] || cmat->m[17] || (cmat->m[18]!=FIX_ONE) || cmat->m[19] )) has_alpha = GF_TRUE;
	else if (key && (key->alpha<0xFF)) has_alpha = GF_TRUE;

	//check if we have a dedicated copy/conv when no stretch nor blending (avoids line load while copying)
	if ((alpha==0xFF) && !flip && !key && !cmat) {
		Bool no_stretch = GF_FALSE;
		Bool output_yuv = format_is_yuv(dst->pixel_format);
		GF_Err e = GF_NOT_SUPPORTED;

		if (!dst_wnd) no_stretch = GF_TRUE;
		else if (src_wnd) {
			if (!dst_wnd->x && !dst_wnd->y && (dst_wnd->w==src_wnd->w) && (dst_wnd->h==src_wnd->h))
				no_stretch = GF_TRUE;
		} else {
			if ((dst_wnd->w==src->width) && (dst_wnd->h==src->height))
				no_stretch = GF_TRUE;
		}
		if (no_stretch && output_yuv) {
			//check YUV10->8, YUV->YUV
			switch (src->pixel_format) {
			case GF_PIXEL_NV12_10:
				e = color_write_nv12_10_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			case GF_PIXEL_NV21_10:
				e = color_write_nv12_10_to_yuv(dst, src, src_wnd, GF_TRUE);
				break;
			case GF_PIXEL_YUV_10:
				e = color_write_yv12_10_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			case GF_PIXEL_YUV422_10:
				if (dst->pixel_format == GF_PIXEL_YUV422)
					e = color_write_yuv422_10_to_yuv422(dst, src, src_wnd, GF_FALSE);
				else if (dst->pixel_format == GF_PIXEL_YUV)
					e = color_write_yuv422_10_to_yuv(dst, src, src_wnd, GF_FALSE);
				else if (dst->pixel_format == GF_PIXEL_YVU)
					e = color_write_yuv422_10_to_yuv(dst, src, src_wnd, GF_TRUE);
				break;
			case GF_PIXEL_YUV444_10:
				if (dst->pixel_format == GF_PIXEL_YUV444)
					e = color_write_yuv444_10_to_yuv444(dst, src, src_wnd, GF_FALSE);
				else if (dst->pixel_format == GF_PIXEL_YUV)
					e = color_write_yuv444_10_to_yuv(dst, src, src_wnd, GF_FALSE);
				else if (dst->pixel_format == GF_PIXEL_YVU)
					e = color_write_yuv444_10_to_yuv(dst, src, src_wnd, GF_TRUE);
				break;
			case GF_PIXEL_YUV:
				e = color_write_yuv420_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			case GF_PIXEL_YVU:
				e = color_write_yuv420_to_yuv(dst, src, src_wnd, GF_TRUE);
				break;
			case GF_PIXEL_YUV422:
				e = color_write_yuv422_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			case GF_PIXEL_YUV444:
				e = color_write_yuv444_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			case GF_PIXEL_YUYV:
			case GF_PIXEL_YVYU:
			case GF_PIXEL_UYVY:
			case GF_PIXEL_VYUY:
				e = color_write_yvyu_to_yuv(dst, src, src_wnd, GF_FALSE);
				break;
			}
		}
		else if (no_stretch && !output_yuv) {
			//check rgb->rgb copy
			switch (dst->pixel_format) {
			case GF_PIXEL_RGB:
			case GF_PIXEL_RGBS:
			case GF_PIXEL_BGR:
				e = color_write_rgb_to_24(dst, src, src_wnd);
				break;
			case GF_PIXEL_RGBX:
			case GF_PIXEL_XRGB:
			case GF_PIXEL_RGBD:
			case GF_PIXEL_RGBDS:
			case GF_PIXEL_BGRX:
			case GF_PIXEL_XBGR:
				e = color_write_rgb_to_32(dst, src, src_wnd);
				break;
			default:
				break;
			}
		}
		if (e == GF_OK) return GF_OK;
	}
	
	
	switch (src->pixel_format) {
	case GF_PIXEL_GREYSCALE:
		load_line = load_line_grey;
		break;
	case GF_PIXEL_ALPHAGREY:
		load_line = load_line_alpha_grey;
		has_alpha = GF_TRUE;
		break;
	case GF_PIXEL_GREYALPHA:
		load_line = load_line_grey_alpha;
		has_alpha = GF_TRUE;
		break;
	case GF_PIXEL_RGB_555:
		load_line = load_line_rgb_555;
		break;
	case GF_PIXEL_RGB_565:
		load_line = load_line_rgb_565;
		break;
	case GF_PIXEL_RGB:
	case GF_PIXEL_RGBS:
		load_line = load_line_rgb_24;
		break;
	case GF_PIXEL_BGR:
		load_line = load_line_bgr_24;
		break;
	case GF_PIXEL_ARGB:
		has_alpha = GF_TRUE;
		load_line = load_line_argb;
		break;
	case GF_PIXEL_BGRA:
		has_alpha = GF_TRUE;
		load_line = load_line_bgra;
		break;
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGBAS:
		has_alpha = GF_TRUE;
	case GF_PIXEL_RGBX:
		load_line = load_line_rgb_32;
		break;
	case GF_PIXEL_XRGB:
		load_line = load_line_xrgb;
		break;
	case GF_PIXEL_BGRX:
		load_line = load_line_bgrx;
		break;
	case GF_PIXEL_RGBDS:
		load_line = load_line_rgbds;
		has_alpha = GF_TRUE;
		break;
	case GF_PIXEL_RGBD:
		load_line = load_line_rgbd;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
		yuv2rgb_init();
		yuv_planar_type = 1;
		break;
	case GF_PIXEL_YUV422:
		yuv2rgb_init();
		yuv_planar_type = 4;
		break;
	case GF_PIXEL_YUV444:
		yuv2rgb_init();
		yuv_planar_type = 5;
		break;

	case GF_PIXEL_YUV_10:
		yuv2rgb_init();
		yuv_planar_type = 3;
		break;
	case GF_PIXEL_YUV422_10:
		yuv2rgb_init();
		yuv_planar_type = 6;
		break;
	case GF_PIXEL_YUV444_10:
		yuv2rgb_init();
		yuv_planar_type = 7;
		break;
	case GF_PIXEL_NV21:
		yuv2rgb_init();
		yuv_planar_type = 8;
		break;
	case GF_PIXEL_NV12:
		yuv2rgb_init();
		yuv_planar_type = 9;
		break;
	case GF_PIXEL_YUVA:
		has_alpha = GF_TRUE;
	case GF_PIXEL_YUVD:
		yuv_planar_type = 2;
		yuv2rgb_init();
		break;
	case GF_PIXEL_YUYV:
		yuv_planar_type = 0;
		yuv2rgb_init();
		load_line = load_line_yuyv;
		break;
	case GF_PIXEL_UYVY:
		yuv_planar_type = 0;
		yuv2rgb_init();
		load_line = load_line_uyvy;
		break;
	case GF_PIXEL_YVYU:
		yuv_planar_type = 0;
		yuv2rgb_init();
		load_line = load_line_yvyu;
		break;
	case GF_PIXEL_VYUY:
		yuv_planar_type = 0;
		yuv2rgb_init();
		load_line = load_line_vyuy;
		break;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Source pixel format %s not supported by gf_stretch_bits\n", gf_pixel_fmt_name(src->pixel_format) ));
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
	case GF_PIXEL_RGB:
		dst_bpp = sizeof(unsigned char)*3;
		copy_row = has_alpha ? merge_row_rgb_24 : copy_row_rgb_24;
		break;
	case GF_PIXEL_BGR:
		dst_bpp = sizeof(unsigned char)*3;
		copy_row = has_alpha ? merge_row_bgr_24 : copy_row_bgr_24;
		break;
	case GF_PIXEL_RGBX:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_bgrx : copy_row_bgrx;
		break;
	case GF_PIXEL_ARGB:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_argb : copy_row_argb;
		break;
	case GF_PIXEL_BGRA:
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
	case GF_PIXEL_BGRX:
		dst_bpp = sizeof(unsigned char)*4;
		copy_row = has_alpha ? merge_row_rgbx : copy_row_rgbx;
		break;
#if 0
	//yuv dest not yet supported
	case GF_PIXEL_YUV444:
		dst_bpp = sizeof(unsigned char)*3;
		copy_row = has_alpha ? merge_row_yuv444 : copy_row_yuv444;
		dst_yuv = GF_TRUE;
		break;
#endif
	default:
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Destination pixel format %s not supported by gf_stretch_bits, patch welcome\n", gf_pixel_fmt_name(dst->pixel_format) ));
		return GF_NOT_SUPPORTED;
	}
	/*x_pitch 0 means linear framebuffer*/
	if (!dst_x_pitch) dst_x_pitch = dst_bpp;


	src_w = src_wnd ? src_wnd->w : src->width;
	src_h = src_wnd ? src_wnd->h : src->height;
	dst_w = dst_wnd ? dst_wnd->w : dst->width;
	dst_h = dst_wnd ? dst_wnd->h : dst->height;

	if (!src_w || !src_h || !dst_w || !dst_h)
		return GF_OK;
		
	if (yuv_planar_type && (src_w%2)) src_w++;

	tmp = (u8 *) gf_malloc(sizeof(u8) * src_w * (yuv_planar_type ? 8 : 4) );
	rows = tmp;

	if ( (src_h / dst_h) * dst_h != src_h) force_load_odd_yuv_lines = GF_TRUE;

	pos_y = 0x10000;
	inc_y = (src_h << 16) / dst_h;
	inc_x = (src_w << 16) / dst_w;
	x_off = src_wnd ? src_wnd->x : 0;
	src_row = src_wnd ? src_wnd->y : 0;

	prev_row = -1;

	dst_bits = (u8 *) dst->video_buffer;
	if (dst_wnd) dst_bits += ((s32)dst_wnd->x) * dst_x_pitch + ((s32)dst_wnd->y) * dst->pitch_y;

	dst_w_size = dst_bpp*dst_w;

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
	no_memcpy = (has_alpha || dst->is_hardware_memory || (dst_bpp!=dst_x_pitch)) ? GF_TRUE : GF_FALSE;

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
						yuv_init = GF_TRUE;
						the_row--;
						if (flip) the_row = src->height - 2 - the_row;
						if (yuv_planar_type == 1) {
							load_line_yv12(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 4) {
							load_line_yuv422(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 5) {
							load_line_yuv444(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 3) {
							load_line_yv12_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 6) {
							load_line_yuv422_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 7) {
							load_line_yuv444_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 8) {
							load_line_nv21((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, dst_yuv);
						}
						else if (yuv_planar_type == 9) {
							load_line_nv12((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, dst_yuv);
						}
						else {
							load_line_yuva(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, (u8 *)src->a_ptr, dst_yuv);
						}

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
								v = tmp[idx]-kr;
								thres = ABS(v);
								v = tmp[idx+1]-kg;
								thres += ABS(v);
								v = tmp[idx+2]-kb;
								thres += ABS(v);
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
				}
				else {
					if (flip) the_row = src->height - 2 - the_row;
					if (yuv_planar_type == 1) {
						load_line_yv12(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 4) {
						load_line_yuv422(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 5) {
						load_line_yuv444(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 3) {
						load_line_yv12_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 6) {
						load_line_yuv422_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 7) {
						load_line_yuv444_10((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 8) {
						load_line_nv21((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, dst_yuv);
					}
					else if (yuv_planar_type == 9) {
						load_line_nv12((char *)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, dst_yuv);
					}
					else {
						load_line_yuva(src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, (u8 *)src->u_ptr, (u8 *)src->v_ptr, (u8 *)src->a_ptr, dst_yuv);
					}
					yuv_init = GF_TRUE;
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
							v = tmp[idx]-kr;
							thres = ABS(v);
							v = tmp[idx+1]-kg;
							thres += ABS(v);
							v = tmp[idx+2]-kb;
							thres += ABS(v);
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
				load_line((u8*)src->video_buffer, x_off, the_row, src->pitch_y, src_w, src->height, tmp, dst_yuv);
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
						v = tmp[idx]-kr;
						thres = ABS(v);
						v = tmp[idx+1]-kg;
						thres += ABS(v);
						v = tmp[idx+2]-kb;
						thres += ABS(v);
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
			copy_row(rows, src_w, dst_bits, dst_w, inc_x, dst_x_pitch, alpha, dst->pitch_y, dst->height);
		}
		/*do NOT use memcpy if the target buffer is not in systems memory*/
		else if (no_memcpy) {
			copy_row(rows, src_w, dst_bits, dst_w, inc_x, dst_x_pitch, alpha, dst->pitch_y, dst->height);
		} else if (dst_bits && dst_bits_prev) {
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

#endif // GPAC_DISABLE_PLAYER


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
void gf_cmx_set(GF_ColorMatrix *_this,
                Fixed c1, Fixed c2, Fixed c3, Fixed c4, Fixed c5,
                Fixed c6, Fixed c7, Fixed c8, Fixed c9, Fixed c10,
                Fixed c11, Fixed c12, Fixed c13, Fixed c14, Fixed c15,
                Fixed c16, Fixed c17, Fixed c18, Fixed c19, Fixed c20)
{
	if (!_this) return;
	_this->m[0] = c1;
	_this->m[1] = c2;
	_this->m[2] = c3;
	_this->m[3] = c4;
	_this->m[4] = c5;
	_this->m[5] = c6;
	_this->m[6] = c7;
	_this->m[7] = c8;
	_this->m[8] = c9;
	_this->m[9] = c10;
	_this->m[10] = c11;
	_this->m[11] = c12;
	_this->m[12] = c13;
	_this->m[13] = c14;
	_this->m[14] = c15;
	_this->m[15] = c16;
	_this->m[16] = c17;
	_this->m[17] = c18;
	_this->m[18] = c19;
	_this->m[19] = c20;
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

GF_EXPORT
void gf_cmx_apply_argb(GF_ColorMatrix *_this, u8 *a_, u8 *r_, u8 *g_, u8 *b_)
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

	a = INT2FIX(col>>24);
	a /= 255;
	r = INT2FIX((col>>16)&0xFF);
	r /= 255;
	g = INT2FIX((col>>8)&0xFF);
	g /= 255;
	b = INT2FIX((col)&0xFF);
	b /= 255;
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
u64 gf_cmx_apply_wide(GF_ColorMatrix *_this, u64 col)
{
	u64 res;
	Fixed _a, _r, _g, _b, a, r, g, b;
	if (!_this || _this->identity) return col;

	a = INT2FIX(col>>48);
	a /= 0xFFFF;
	r = INT2FIX((col>>32)&0xFFFF);
	r /= 0xFFFF;
	g = INT2FIX((col>>16)&0xFFFF);
	g /= 0xFFFF;
	b = INT2FIX((col)&0xFFFF);
	b /= 0xFFFF;
	_r = gf_mulfix(r, _this->m[0]) + gf_mulfix(g, _this->m[1]) + gf_mulfix(b, _this->m[2]) + gf_mulfix(a, _this->m[3]) + _this->m[4];
	_g = gf_mulfix(r, _this->m[5]) + gf_mulfix(g, _this->m[6]) + gf_mulfix(b, _this->m[7]) + gf_mulfix(a, _this->m[8]) + _this->m[9];
	_b = gf_mulfix(r, _this->m[10]) + gf_mulfix(g, _this->m[11]) + gf_mulfix(b, _this->m[12]) + gf_mulfix(a, _this->m[13]) + _this->m[14];
	_a = gf_mulfix(r, _this->m[15]) + gf_mulfix(g, _this->m[16]) + gf_mulfix(b, _this->m[17]) + gf_mulfix(a, _this->m[18]) + _this->m[19];
	CLIP_COMP(_a);
	CLIP_COMP(_r);
	CLIP_COMP(_g);
	CLIP_COMP(_b);
	res = (u32) (_a*0xFFFF)&0xFFFF;
	res<<=16;
	res |= (u32) (_r*0xFFFF)&0xFFFF;
	res<<=16;
	res |= (u32) (_g*0xFFFF)&0xFFFF;
	res<<=16;
	res |= (u32) (_b*0xFFFF)&0xFFFF;
	return res;
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


#ifndef GPAC_DISABLE_PLAYER


//intrinsic code segfaults on 32 bit, need to check why
#if defined(GPAC_64_BITS)
# if defined(WIN32) && !defined(__GNUC__)
#  include <intrin.h>
#  define GPAC_HAS_SSE2
# else
#  ifdef __SSE2__
#   include <emmintrin.h>
#   define GPAC_HAS_SSE2
#  endif
# endif
#endif

#ifdef GPAC_HAS_SSE2

static GF_Err color_write_yv12_10_to_yuv_intrin(GF_VideoSurface *vs_dst, unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	__m128i val1, val2, val_dst, *src1, *src2, *dst;
	if (!pY) return GF_BAD_PARAM;
	
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

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}


	
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
			if (vs_dst->u_ptr) dst = (__m128i *) (vs_dst->u_ptr + i*vs_dst->pitch_y/2);
			else dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2);

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
			if (vs_dst->v_ptr) dst = (__m128i *) (vs_dst->v_ptr + i*vs_dst->pitch_y/2);
			else dst = (__m128i *)(vs_dst->video_buffer + 5*vs_dst->pitch_y * vs_dst->height/4  + i*vs_dst->pitch_y/2);

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

static GF_Err color_write_yuv422_10_to_yuv422_intrin(GF_VideoSurface *vs_dst,  unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	__m128i val1, val2, val_dst, *src1, *src2, *dst;
	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 3*src_stride * src_height/2;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y + _src_wnd->x;
		pU = pU + (src_stride * _src_wnd->y  + _src_wnd->x) / 2;
		pV = pV + (src_stride * _src_wnd->y  + _src_wnd->x) / 2;
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = src_width;
		h = src_height;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}


	
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

	for (i=0; i<h; i++) {
		src1 = (__m128i *) (pU + i*src_stride/2);
		src2 = src1+1;
		if (vs_dst->u_ptr) dst = (__m128i *) (vs_dst->u_ptr + i*vs_dst->pitch_y/2);
		else dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2);

		for (j=0; j<w/32; j++, src1+=2, src2+=2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i=0; i<h; i++) {
		src1 = (__m128i *) (pV + i*src_stride/2);
		src2 = src1+1;
		if (vs_dst->v_ptr) dst = (__m128i *) (vs_dst->v_ptr + i*vs_dst->pitch_y/2);
		else dst = (__m128i *)(vs_dst->video_buffer + 3*vs_dst->pitch_y * vs_dst->height/2  + i*vs_dst->pitch_y/2);

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

static GF_Err color_write_yuv444_10_to_yuv444_intrin(GF_VideoSurface *vs_dst, unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	__m128i val1, val2, val_dst, *src1, *src2, *dst;
	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 2 * src_stride * src_height ;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y  + _src_wnd->x;
		pU = pU + src_stride * _src_wnd->y  + _src_wnd->x;
		pV = pV + src_stride * _src_wnd->y  + _src_wnd->x;
		w = _src_wnd->w;
		h = _src_wnd->h;
	}
	else {
		w = src_width;
		h = src_height;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}


	for (i = 0; i<h; i++) {
		src1 = (__m128i *)(pY + i*src_stride);
		src2 = src1 + 1;
		dst = (__m128i *)(vs_dst->video_buffer + i*vs_dst->pitch_y);

		for (j = 0; j<w / 16; j++, src1 += 2, src2 += 2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i = 0; i<h; i++) {
		src1 = (__m128i *) (pU + i*src_stride );
		src2 = src1 + 1;
		if (vs_dst->u_ptr) dst = (__m128i *) (vs_dst->u_ptr + i*vs_dst->pitch_y );
		else dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y );

		for (j = 0; j<w / 16; j++, src1 += 2, src2 += 2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i = 0; i<h ; i++) {
		src1 = (__m128i *) (pV + i*src_stride );
		src2 = src1 + 1;
		if (vs_dst->v_ptr) dst = (__m128i *) (vs_dst->v_ptr + i*vs_dst->pitch_y);
		else dst = (__m128i *)(vs_dst->video_buffer + 2 * vs_dst->pitch_y * vs_dst->height  + i*vs_dst->pitch_y );

		for (j = 0; j<w / 16; j++, src1 += 2, src2 += 2, dst++) {
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
static GF_Err color_write_yuv422_10_to_yuv_intrin(GF_VideoSurface *vs_dst, unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	__m128i val1, val2, val_dst, *src1, *src2, *dst;
	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 3 * src_stride * src_height / 2;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y + _src_wnd->x;
		pU = pU + (src_stride * _src_wnd->y + _src_wnd->x) / 2;
		pV = pV + (src_stride * _src_wnd->y + _src_wnd->x) / 2;
		w = _src_wnd->w;
		h = _src_wnd->h;
	}
	else {
		w = src_width;
		h = src_height;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}



	for (i = 0; i<h; i++) {
		src1 = (__m128i *)(pY + i*src_stride);
		src2 = src1 + 1;
		dst = (__m128i *)(vs_dst->video_buffer + i*vs_dst->pitch_y);

		for (j = 0; j<w / 16; j++, src1 += 2, src2 += 2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i = 0; i<h / 2; i++) {
		src1 = (__m128i *) (pU +  i*src_stride);
		src2 = src1 + 1;
		if (vs_dst->u_ptr) dst = (__m128i *) (vs_dst->u_ptr + i*vs_dst->pitch_y / 2);
		else dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y / 2);

		for (j = 0; j<w / 32; j++, src1 += 2, src2 += 2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i = 0; i<h / 2; i++) {
		src1 = (__m128i *) (pV + i*src_stride);
		src2 = src1 + 1;
		if (vs_dst->v_ptr) dst = (__m128i *) (vs_dst->v_ptr + i*vs_dst->pitch_y / 2);
		else dst = (__m128i *)(vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4 + i*vs_dst->pitch_y / 2);

		for (j = 0; j<w / 32; j++, src1 += 2, src2 += 2, dst++) {
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
static GF_Err color_write_yuv444_10_to_yuv_intrin(GF_VideoSurface *vs_dst, unsigned char *pY, unsigned char *pU, unsigned char*pV, u32 src_stride, u32 src_width, u32 src_height, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	__m128i val1, val2,val3,val4, val12, val34, val_dst, *src1, *src2,*src3,*src4, *dst;
	
	if (!pU) {
		pU = pY + src_stride * src_height;
		pV = pY + 2 * src_stride * src_height;
	}

	if (_src_wnd) {
		pY = pY + src_stride * _src_wnd->y + _src_wnd->x;
		pU = pU + src_stride * _src_wnd->y + _src_wnd->x;
		pV = pV + src_stride * _src_wnd->y + _src_wnd->x;
		w = _src_wnd->w;
		h = _src_wnd->h;
	}
	else {
		w = src_width;
		h = src_height;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}


	for (i = 0; i<h; i++) {
		src1 = (__m128i *)(pY + i*src_stride);
		src2 = src1 + 1;
		dst = (__m128i *)(vs_dst->video_buffer + i*vs_dst->pitch_y);

		for (j = 0; j<w / 16; j++, src1 += 2, src2 += 2, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi16(val1, 2);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi16(val2, 2);
			val_dst = _mm_packus_epi16(val1, val2);
			_mm_store_si128(dst, val_dst);
		}
	}

	for (i = 0; i<h / 2; i++) {
		src1 = (__m128i *) (pU + 2*i*src_stride);
		src2 = src1 + 1;
		src3 = src2 + 1;
		src4 = src3 + 1;
		if (vs_dst->u_ptr) dst = (__m128i *) (vs_dst->u_ptr + i*vs_dst->pitch_y / 2);
		else dst = (__m128i *)(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y / 2);

		for (j = 0; j<w /32; j++, src1 += 4, src2 += 4,src3 +=4, src4+=4, dst++) {
			val1 = _mm_load_si128(src1);
			val1 = _mm_srli_epi32(val1, 16);
			val2 = _mm_load_si128(src2);
			val2 = _mm_srli_epi32(val2, 16);
			val12 = _mm_packs_epi32(val1, val2); 
			val12 = _mm_srli_epi16(val12, 2);

			val3 = _mm_load_si128(src3);
			
			val3 = _mm_srli_epi32(val3, 16);
			val4 = _mm_load_si128(src4);
		
			val4 = _mm_srli_epi32(val4, 16);
			val34 = _mm_packs_epi32(val3, val4); 
			val34 = _mm_srli_epi16(val34, 2);

			val_dst = _mm_packus_epi16(val12, val34);
			_mm_store_si128(dst, val_dst);
			
		}
	}

	for (i = 0; i<h / 2; i++) {
		src1 = (__m128i *) (pV + 2*i*src_stride );
		src2 = src1 + 1;
		src3 = src1 + 2;
		src4 = src1 + 3;
		if (vs_dst->v_ptr) dst = (__m128i *) (vs_dst->v_ptr + i*vs_dst->pitch_y / 2);
		else dst = (__m128i *)(vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4 + i*vs_dst->pitch_y / 2);

		for (j = 0; j<w / 32; j++, src1 += 4, src2 += 4, src3 += 4, src4 += 4, dst++) {
			val1 = _mm_load_si128(src1);
			
			val1 = _mm_srli_epi32(val1, 16);
			val2 = _mm_load_si128(src2);
			
			val2 = _mm_srli_epi32(val2, 16);
			val12 = _mm_packs_epi32(val1, val2); 
			val12 = _mm_srli_epi16(val12, 2);

			val3 = _mm_load_si128(src3);
			
			val3 = _mm_srli_epi32(val3, 16);
			val4 = _mm_load_si128(src4);
			
			val4 = _mm_srli_epi32(val4, 16);
			val34 = _mm_packs_epi32(val3, val4); 
			val34 = _mm_srli_epi16(val34, 2);

			val_dst = _mm_packus_epi16(val12, val34);
			_mm_store_si128(dst, val_dst);
		}
	}
	
	return GF_OK;

}
#endif


static GF_Err color_write_yv12_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, const GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
	}

	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 5 * vs_src->pitch_y * vs_src->height/4;
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
	        && (GFINTCAST (pU + vs_src->pitch_y/2)%8 == 0)
	        && (GFINTCAST (pV + vs_src->pitch_y/2)%8 == 0)
	   ) {
		return color_write_yv12_10_to_yuv_intrin(vs_dst, pY, pU, pV, vs_src->pitch_y, vs_src->width, vs_src->height, _src_wnd, swap_uv);
	}
#endif

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
		therefore force an even Y offset for U and V planes.*/
		pU = pU + (vs_src->pitch_y * (_src_wnd->y / 2) + _src_wnd->x) / 2;
		pV = pV + (vs_src->pitch_y * (_src_wnd->y / 2) + _src_wnd->x) / 2;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}

	
	for (i=0; i<h; i++) {
		u16 *src = (u16 *) (pY + i*vs_src->pitch_y);
		u8 *dst = (u8 *) vs_dst->video_buffer + i*vs_dst->pitch_y;

		for (j=0; j<w; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i=0; i<h/2; i++) {
		u16 *src = (u16 *) (pU + i*vs_src->pitch_y/2);
		u8 *dst = (u8 *) vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2;
		if (vs_dst->u_ptr) dst = (u8 *) (vs_dst->u_ptr + i*vs_dst->pitch_y/2);

		for (j=0; j<w/2; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i=0; i<h/2; i++) {
		u16 *src = (u16 *) (pV + i*vs_src->pitch_y/2);
		u8 *dst = (u8 *) vs_dst->video_buffer + 5*vs_dst->pitch_y * vs_dst->height/4  + i*vs_dst->pitch_y/2;
		if (vs_dst->v_ptr) dst = (u8 *) (vs_dst->v_ptr + i*vs_dst->pitch_y/2);

		for (j=0; j<w/2; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}
	return GF_OK;
}

static GF_Err color_write_nv12_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pUV = vs_src->u_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
	}


//#ifdef GPAC_HAS_SSE2
#if 0

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
		return gf_color_write_yv12_10_to_yuv_intrin(vs_dst, pY, pU, pV, src_stride, src_width, src_height, _src_wnd, swap_uv);
	}
#endif

	if (!pUV) {
		pUV = pY + vs_src->pitch_y * vs_src->height;
	}

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
		therefore force an even Y offset for U and V planes.*/
		pUV = pUV + (vs_src->pitch_y * (_src_wnd->y / 2) + _src_wnd->x) / 2;
	}

	for (i=0; i<h; i++) {
		u16 *src = (u16 *) (pY + i*vs_src->pitch_y);
		u8 *dst = (u8 *) vs_dst->video_buffer + i*vs_dst->pitch_y;

		for (j=0; j<w; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i=0; i<h/2; i++) {
		u16 *src = (u16 *) (pUV + i*vs_src->pitch_y/2);
		u8 *dst = (u8 *) vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y/2;
		if (vs_dst->u_ptr) dst = (u8 *) (vs_dst->u_ptr + i*vs_dst->pitch_y/2);
		if (swap_uv) src += 1;

		for (j=0; j<w/2; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i=0; i<h/2; i++) {
		u16 *src = (u16 *) (pUV + i*vs_src->pitch_y/2);
		u8 *dst = (u8 *) vs_dst->video_buffer + 5*vs_dst->pitch_y * vs_dst->height/4  + i*vs_dst->pitch_y/2;
		if (vs_dst->v_ptr) dst = (u8 *) (vs_dst->v_ptr + i*vs_dst->pitch_y/2);
		if (!swap_uv) src += 1;

		for (j=0; j<w/2; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}
	return GF_OK;

}

static GF_Err color_write_yuv422_10_to_yuv422(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
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
	        && (GFINTCAST (pU + vs_src->pitch_y/2)%8 == 0)
	        && (GFINTCAST (pV + vs_src->pitch_y/2)%8 == 0)
	   ) {
		return color_write_yuv422_10_to_yuv422_intrin(vs_dst, pY, pU, pV, vs_src->pitch_y, vs_src->width, vs_src->height, _src_wnd, swap_uv);
	}
#endif

	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 3*vs_src->pitch_y * vs_src->height/2;
	}

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		pU = pU + (vs_src->pitch_y * _src_wnd->y + _src_wnd->x) / 2;
		pV = pV + (vs_src->pitch_y * _src_wnd->y + _src_wnd->x) / 2;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}
	
	for (i=0; i<h; i++) {
		u16 *src_y = (u16 *) (pY + i*vs_src->pitch_y);
		u8 *dst_y = (u8 *) vs_dst->video_buffer + i*vs_dst->pitch_y;

		for (j=0; j<w; j++) {
			*dst_y = (*src_y) >> 2;
			dst_y++;
			src_y++;
		}
	}
	for (i=0; i<h; i++) {
		u16 *src_u = (u16 *) (pU + i*vs_src->pitch_y/2);
		u16 *src_v = (u16 *) (pV + i*vs_src->pitch_y/2);
		u8 *dst_u = (u8 *) vs_dst->video_buffer + vs_dst->width * vs_dst->height + i*vs_dst->pitch_y/2;
		u8 *dst_v = (u8 *) vs_dst->video_buffer + 3*vs_dst->pitch_y * vs_dst->height/2  + i*vs_dst->pitch_y/2;
		if (vs_dst->u_ptr) dst_u = (u8 *) (vs_dst->u_ptr + i*vs_dst->pitch_y/2);
		if (vs_dst->v_ptr) dst_v = (u8 *) (vs_dst->v_ptr + i*vs_dst->pitch_y/2);
		
		for (j=0; j<w/2; j++) {
			*dst_u = (*src_u) >> 2;
			dst_u++;
			src_u++;
			
			*dst_v = (*src_v) >> 2;
			dst_v++;
			src_v++;
		}
	}
	return GF_OK;
}

static GF_Err color_write_yuv444_10_to_yuv444(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
	}


#ifdef GPAC_HAS_SSE2

#ifdef GPAC_64_BITS
#define GFINTCAST  (u64)
#else
#define GFINTCAST  (u32)
#endif

	if ( (w%32 == 0)
	        && (GFINTCAST (vs_dst->video_buffer + vs_dst->pitch_y)%8 == 0)
	        && (GFINTCAST (vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + vs_dst->pitch_y)%8 == 0)
	        && (GFINTCAST (pU + vs_src->pitch_y)%8 == 0)
	        && (GFINTCAST (pV + vs_src->pitch_y)%8 == 0)
	   ) {
		return color_write_yuv444_10_to_yuv444_intrin(vs_dst, pY, pU, pV, vs_src->pitch_y, vs_src->width, vs_src->height, _src_wnd, swap_uv);
	}
#endif

	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 2*vs_src->pitch_y * vs_src->height;
	}

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y  + _src_wnd->x;
		pU = pU + vs_src->pitch_y * _src_wnd->y  + _src_wnd->x ;
		pV = pV + vs_src->pitch_y * _src_wnd->y  + _src_wnd->x;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}

	for (i=0; i<h; i++) {
		u16 *src_y = (u16 *) (pY + i*vs_src->pitch_y);
		u8 *dst_y = (u8 *) vs_dst->video_buffer + i*vs_dst->pitch_y;
		
		u16 *src_u= (u16 *) (pU + i*vs_src->pitch_y);
		u8 *dst_u = (u8 *) vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height+ i*vs_dst->pitch_y;
		
		u16 *src_v = (u16 *) (pV + i*vs_src->pitch_y);
		u8 *dst_v = (u8 *) vs_dst->video_buffer + 2*vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y;

		if (vs_dst->u_ptr) dst_u = (u8 *)(vs_dst->u_ptr + i*vs_dst->pitch_y);
		if (vs_dst->v_ptr) dst_v = (u8 *)(vs_dst->v_ptr + i*vs_dst->pitch_y);

		for (j=0; j<w; j++) {
			*dst_y = (*src_y) >> 2;
			dst_y++;
			src_y++;
			
			*dst_u = (*src_u) >> 2;
			dst_u++;
			src_u++;
			
		   *dst_v= (*src_v) >> 2;
			dst_v++;
			src_v++;
		}
	}
	return GF_OK;
}

static GF_Err color_write_yuv422_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
	}


#ifdef GPAC_HAS_SSE2

#ifdef GPAC_64_BITS
#define GFINTCAST  (u64)
#else
#define GFINTCAST  (u32)
#endif

	if ((w % 32 == 0)
		&& (GFINTCAST(vs_dst->video_buffer + vs_dst->pitch_y) % 8 == 0)
		&& (GFINTCAST(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + vs_dst->pitch_y / 2) % 8 == 0)
		&& (GFINTCAST(pU + vs_src->pitch_y / 2) % 8 == 0)
		&& (GFINTCAST(pV + vs_src->pitch_y / 2) % 8 == 0)
		) {
		return color_write_yuv422_10_to_yuv_intrin(vs_dst, pY, pU, pV, vs_src->pitch_y, vs_src->width, vs_src->height, _src_wnd, swap_uv);
	}
#endif
	
	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 3 * vs_src->pitch_y * vs_src->height/2;
	}

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		pU = pU + (vs_src->pitch_y * _src_wnd->y + _src_wnd->x) / 2;
		pV = pV + (vs_src->pitch_y * _src_wnd->y + _src_wnd->x) / 2;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}


	for (i = 0; i<h; i++) {
		u16 *src = (u16 *)(pY + i*vs_src->pitch_y);
		u8 *dst = (u8 *)vs_dst->video_buffer + i*vs_dst->pitch_y;

		for (j = 0; j<w; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i = 0; i<h/2; i++) {
		u16 *srcu = (u16 *)(pU +  i*vs_src->pitch_y);
		u16 *srcv = (u16 *)(pV +  i*vs_src->pitch_y);
		u8 *dstu, *dstv;

		if (vs_dst->u_ptr)
			dstu = (u8 *)(vs_dst->u_ptr + i*vs_dst->pitch_y / 2);
		else
			dstu = (u8 *)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y / 2;

		if (vs_dst->v_ptr)
			dstv = (u8 *)(vs_dst->v_ptr + i*vs_dst->pitch_y / 2);
		else
			dstv = (u8 *)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4 + i*vs_dst->pitch_y / 2;

		for (j = 0; j<w / 2; j++) {
			*dstu = ( (srcu[0] + srcu[1]) / 2) >> 2;
			dstu++;
			srcu+=2;

			*dstv = ( (srcv[0] + srcv[1]) / 2) >> 2;
			dstv++;
			srcv+=2;
		}
	}

	return GF_OK;
}

static GF_Err color_write_yuv444_10_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j, w, h;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
	} else {
		w = vs_src->width;
		h = vs_src->height;
	}
	

#ifdef GPAC_HAS_SSE2

#ifdef GPAC_64_BITS
#define GFINTCAST  (u64)
#else
#define GFINTCAST  (u32)
#endif

	if ( (w % 32 == 0)
		&& (GFINTCAST(vs_dst->video_buffer + vs_dst->pitch_y) % 8 == 0)
		&& (GFINTCAST(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + vs_dst->pitch_y) % 8 == 0)
		&& (GFINTCAST(pU + vs_src->pitch_y) % 8 == 0)
		&& (GFINTCAST(pV + vs_src->pitch_y) % 8 == 0)
		) {
		return color_write_yuv444_10_to_yuv_intrin(vs_dst, pY, pU, pV, vs_src->pitch_y, vs_src->width, vs_src->height, _src_wnd, swap_uv);
	}
#endif


	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 2 * vs_src->pitch_y * vs_src->height;
	}

	if (_src_wnd) {
		pY = pY + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		pU = pU + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
		pV = pV + vs_src->pitch_y * _src_wnd->y + _src_wnd->x;
	}

	if (swap_uv) {
		u8 *t = pV;
		pV = pU;
		pU = t;
	}
	
	for (i = 0; i<h; i++) {
		u16 *src = (u16 *)(pY + i*vs_src->pitch_y);
		u8 *dst = (u8 *)vs_dst->video_buffer + i*vs_dst->pitch_y;

		for (j = 0; j<w; j++) {
			*dst = (*src) >> 2;
			dst++;
			src++;
		}
	}

	for (i = 0; i<h/2; i++) {
		u16 *srcu1 = (u16 *)(pU + 2*i*vs_src->pitch_y );
		u16 *srcu2 = (u16 *)(pU + 2*(i+1)*vs_src->pitch_y );
		u16 *srcv1 = (u16 *)(pV + 2*i*vs_src->pitch_y );
		u16 *srcv2 = (u16 *)(pV + 2*(i+1)*vs_src->pitch_y );
		u8 *dstu, *dstv;

		if (vs_dst->u_ptr)
			dstu = (u8 *)(vs_dst->u_ptr + i*vs_dst->pitch_y / 2);
		else
			dstu = (u8 *)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i*vs_dst->pitch_y / 2;

		if (vs_dst->v_ptr)
			dstv = (u8 *)(vs_dst->v_ptr + i*vs_dst->pitch_y / 2);
		else
			dstv = (u8 *)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4 + i*vs_dst->pitch_y / 2;

		for (j = 0; j<w/2 ;j++) {
			u32 u, v;
			u = (srcu1[0] + srcu1[1] + srcu2[0] + srcu2[1] ) / 4;
			*dstu = u>>2;
			dstu++;
			srcu1+=2;
			srcu2+=2;

			v = (srcv1[0] + srcv1[1] + srcv2[0] + srcv2[1] ) / 4;
			*dstv = v>>2;
			dstv++;
			srcv1+=2;
			srcv2+=2;
		}
	}
	return GF_OK;
}

static Bool is_planar_yuv(u32 pf)
{
	switch (pf) {
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		return GF_TRUE;
	}
	return GF_FALSE;
}


static GF_Err color_write_yuv420_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 w, h, ox, oy;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	} else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 5 * vs_src->pitch_y * vs_src->height / 4;
	}


	pY = pY + vs_src->pitch_y * oy + ox;
	/*because of U and V downsampling by 2x2, working with odd Y offset will lead to a half-line shift between Y and UV components. We
	therefore force an even Y offset for U and V planes.*/
	pU = pU + (vs_src->pitch_y * (oy / 2) + ox) / 2;
	pV = pV + (vs_src->pitch_y * (oy / 2) + ox) / 2;


	if (is_planar_yuv(vs_dst->pixel_format)) {
		/*complete source copy*/
		if ((vs_dst->pitch_y == (s32)vs_src->pitch_y) && (w == vs_src->width) && (h == vs_src->height)) {
			assert(!ox);
			assert(!oy);
			memcpy(vs_dst->video_buffer, pY, sizeof(u8)*w*h);
			if (vs_dst->pixel_format == GF_PIXEL_YUV) {
				memcpy(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height, pV, sizeof(u8)*w*h/ 4);
				memcpy(vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4, pU, sizeof(u8)*w*h/ 4);
			}
			else {
				memcpy(vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height, pU, sizeof(u8)*w*h / 4);
				memcpy(vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4, pV, sizeof(u8)*w*h/ 4);
			}
		} else {
			u32 i;
			u8 *dst, *src, *dst2, *src2, *dst3, *src3;

			src = pY;
			dst = (u8*)vs_dst->video_buffer;

			src2 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pU : pV;
			dst2 = (u8*)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height;
			src3 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pV : pU;
			dst3 = (u8*)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4;
			for (i = 0; i<h; i++) {
				memcpy(dst, src, w);
				src += vs_src->pitch_y;
				dst += vs_dst->pitch_y;
				if (i<h / 2) {
					memcpy(dst2, src2, w / 2);
					src2 += vs_src->pitch_y/ 2;
					dst2 += vs_dst->pitch_y / 2;
					memcpy(dst3, src3, w / 2);
					src3 += vs_src->pitch_y / 2;
					dst3 += vs_dst->pitch_y / 2;
				}
			}
		}
	}
	else if (vs_dst->pixel_format == GF_PIXEL_UYVY) {
		u32 i, j;
		for (i = 0; i<h; i++) {
			u8 *dst, *y, *u, *v;
			y = pY + i*vs_src->pitch_y;
			u = pU + (i / 2) * vs_src->pitch_y  / 2;
			v = pV + (i / 2) * vs_src->pitch_y / 2;
			dst = (u8 *)vs_dst->video_buffer + i*vs_dst->pitch_y;

			for (j = 0; j<w / 2; j++) {
				*dst = *u;
				dst++;
				u++;
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
				*dst = *y;
				dst++;
				y++;
			}
		}
	}
	else if (vs_dst->pixel_format == GF_PIXEL_VYUY) {
		u32 i, j;
		for (i = 0; i<h; i++) {
			u8 *dst, *y, *u, *v;
			y = pY + i*vs_src->pitch_y;
			u = pU + (i / 2) * vs_src->pitch_y / 2;
			v = pV + (i / 2) * vs_src->pitch_y / 2;
			dst = (u8 *)vs_dst->video_buffer + i*vs_dst->pitch_y;

			for (j = 0; j<w / 2; j++) {
				*dst = *v;
				dst++;
				v++;
				*dst = *y;
				dst++;
				y++;
				*dst = *u;
				dst++;
				u++;
				*dst = *y;
				dst++;
				y++;
			}
		}
	}
	else if (vs_dst->pixel_format == GF_PIXEL_YUYV) {
		u32 i, j;
		for (i = 0; i<h; i++) {
			u8 *dst, *y, *u, *v;
			y = pY + i*vs_src->pitch_y;
			u = pU + (i / 2) * vs_src->pitch_y / 2;
			v = pV + (i / 2) * vs_src->pitch_y / 2;
			dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;

			for (j = 0; j<w / 2; j++) {
				*dst = *y;
				dst++;
				y++;
				*dst = *u;
				dst++;
				u++;
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
			}
		}
	}
	else if (vs_dst->pixel_format == GF_PIXEL_YVYU) {
		u32 i, j;
		for (i = 0; i<h; i++) {
			u8 *dst, *y, *u, *v;
			y = pY + i*vs_src->pitch_y;
			u = pU + (i / 2) * vs_src->pitch_y / 2;
			v = pV + (i / 2) * vs_src->pitch_y / 2;
			dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;

			for (j = 0; j<w / 2; j++) {
				*dst = *y;
				dst++;
				y++;
				*dst = *v;
				dst++;
				v++;
				*dst = *y;
				dst++;
				y++;
				*dst = *u;
				dst++;
				u++;
			}
		}
	}
	else {
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static GF_Err color_write_yuv422_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 w, h, ox, oy;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	}
	else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	if (!pU) {
		pU = pY + vs_src->pitch_y * vs_src->height;
		pV = pY + 3 * vs_src->pitch_y * vs_src->height / 2;
	}


	pY = pY + vs_src->pitch_y * oy + ox;
	pU = pU + (vs_src->pitch_y * oy + ox) / 2;
	pV = pV + (vs_src->pitch_y * oy + ox) / 2;


	if (is_planar_yuv(vs_dst->pixel_format)) {
		/*complete source copy*/
		u32 i;
		u8 *dst, *src, *dst2, *src2, *dst3, *src3, *_src2, *_src3;

		src = pY;
		_src2 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pU : pV;
		_src3 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pV : pU;
		dst = (u8*)vs_dst->video_buffer;
		dst2 = (u8*)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height;
		dst3 = (u8*)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4;
		for (i = 0; i<h; i++) {
			memcpy(dst, src, w);
			src += vs_src->pitch_y;
			dst += vs_dst->pitch_y;
			if (i < h / 2) {
				src2 = _src2 + i*vs_src->pitch_y;
				src3 = _src3 + i*vs_src->pitch_y;
				memcpy(dst2, src2, w / 2);
				memcpy(dst3, src3, w / 2);
				dst2 += vs_dst->pitch_y / 2;
				dst3 += vs_dst->pitch_y / 2;
			}
		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}


static GF_Err color_write_yuv444_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 w, h, ox, oy;
	u8 *pY = vs_src->video_buffer;
	u8 *pU = vs_src->u_ptr;
	u8 *pV = vs_src->v_ptr;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	}
	else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	if (!pU) {
		pU = pY + vs_src->pitch_y* vs_src->height;
		pV = pY + 2 * vs_src->pitch_y * vs_src->height;
	}

	pY = pY + vs_src->pitch_y * oy + ox;
	pU = pU + vs_src->pitch_y * oy + ox;
	pV = pV + vs_src->pitch_y * oy + ox;

	if (is_planar_yuv(vs_dst->pixel_format)) {
		/*complete source copy*/
		u32 i, j;
		u8 *dst, *src, *_src2, *_src3;

		src = pY;
		_src2 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pU : pV;
		_src3 = (vs_dst->pixel_format != GF_PIXEL_YUV) ? pV : pU;
		dst = (u8*)vs_dst->video_buffer;

		for (i = 0; i<h; i++) {
			memcpy(dst, src, w);
			src += vs_src->pitch_y;
			dst += vs_dst->pitch_y;

		}
		for (i = 0; i < h / 2; i++) {
			u8 *dst2, *src2, *dst3, *src3;
			src2 = _src2 + 2 * i*vs_src->pitch_y;
			dst2 = (u8*)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height + i* vs_dst->pitch_y / 2;
			src3 = _src3 + 2 * i*vs_src->pitch_y;
			dst3 = (u8*)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4 + i* vs_dst->pitch_y / 2;
			for (j = 0; j<w / 2; j++) {
				*dst2 = *src2;
				dst2++;
				src2 += 2;

				*dst3 = *src3;
				dst3++;
				src3 += 2;
			}
		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static GF_Err color_write_yvyu_to_yuv(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd, Bool swap_uv)
{
	u32 i, j;
	u32 w, h, ox, oy;
	u8 *pY, *pU, *pV;
	
	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	}
	else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	switch (vs_src->pixel_format) {
	case GF_PIXEL_UYVY:
		pU = vs_src->video_buffer + vs_src->pitch_y* oy + ox;
		pY = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 1;
		pV = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 3;
		break;
	case GF_PIXEL_YUYV:
		pY = vs_src->video_buffer + vs_src->pitch_y * oy + ox;
		pU = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 1;
		pV = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 3;
		break;
	case GF_PIXEL_YVYU:
		pY = vs_src->video_buffer + vs_src->pitch_y * oy + ox;
		pV = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 1;
		pU = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 3;
		break;
	case GF_PIXEL_VYUY:
		pV = vs_src->video_buffer + vs_src->pitch_y* oy + ox;
		pY = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 1;
		pU = vs_src->video_buffer + vs_src->pitch_y * oy + ox + 3;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	if (is_planar_yuv(vs_dst->pixel_format)) {
		u8 *dst_y, *dst_u, *dst_v;

		dst_y = (u8*)vs_dst->video_buffer;
		if (vs_dst->pixel_format == GF_PIXEL_YUV) {
			dst_v = (u8*)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height;
			dst_u = (u8*)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4;
		}
		else {
			dst_u = (u8*)vs_dst->video_buffer + vs_dst->pitch_y * vs_dst->height;
			dst_v = (u8*)vs_dst->video_buffer + 5 * vs_dst->pitch_y * vs_dst->height / 4;
		}
		for (i = 0; i<h; i++) {
			for (j = 0; j<w; j += 2) {
				*dst_y = *pY;
				*(dst_y + 1) = *(pY + 2);
				dst_y += 2;
				pY += 4;
				if (i % 2) continue;

				*dst_u = (*pU + *(pU + vs_src->pitch_y)) / 2;
				*dst_v = (*pV + *(pV + vs_src->pitch_y)) / 2;
				dst_u++;
				dst_v++;
				pU += 4;
				pV += 4;
			}
			if (i % 2) {
				pU += vs_src->pitch_y;
				pV += vs_src->pitch_y;
			}
		}
		return GF_OK;
	}

	if (vs_src->pixel_format == vs_dst->pixel_format) {
		for (i = 0; i<h; i++) {
			char *dst = vs_dst->video_buffer + i*vs_dst->pitch_y;
			pY = vs_src->video_buffer + vs_src->pitch_y * (i + oy) + ox;
			memcpy(dst, pY, sizeof(char) * 2 * w);
		}
		return GF_OK;
	}

	for (i = 0; i<h; i++) {
		u8 *dst = vs_dst->video_buffer + i*vs_dst->pitch_y;
		u8 *y = pY + vs_src->pitch_y * i;
		u8 *u = pU + vs_src->pitch_y * i;
		u8 *v = pV + vs_src->pitch_y * i;
		switch (vs_dst->pixel_format) {
		case GF_PIXEL_UYVY:
			for (j = 0; j<w; j += 2) {
				dst[0] = *u;
				dst[1] = *y;
				dst[2] = *v;
				dst[3] = *(y + 2);
				dst += 4;
				y += 4;
				u += 4;
				v += 4;
			}
			break;
		case GF_PIXEL_YVYU:
			for (j = 0; j<w; j += 2) {
				dst[0] = *y;
				dst[1] = *v;
				dst[2] = *(y + 2);
				dst[3] = *u;
				dst += 4;
				y += 4;
				u += 4;
				v += 4;
			}
			break;
		case GF_PIXEL_YUYV:
			for (j = 0; j<w; j += 2) {
				dst[0] = *y;
				dst[1] = *u;
				dst[2] = *(y + 2);
				dst[3] = *v;
				dst += 4;
				y += 4;
				u += 4;
				v += 4;
			}
			break;
		case GF_PIXEL_VYUY:
			for (j = 0; j<w; j += 2) {
				dst[0] = *v;
				dst[1] = *y;
				dst[2] = *u;
				dst[3] = *(y + 2);
				dst += 4;
				y += 4;
				u += 4;
				v += 4;
			}
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
	}
	return GF_OK;
}


u32 get_bpp(u32 pf)
{
	switch (pf) {
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		return 2;
	case GF_PIXEL_RGB:
	case GF_PIXEL_RGBS:
	case GF_PIXEL_BGR:
		return 3;
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBAS:
	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGBDS:
		return 4;
	}
	return 0;
}

static GF_Err color_write_rgb_to_24(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd)
{
	u32 i;
	u32 w, h, ox, oy;
	u8 *src;
	u32 BPP;

	if (vs_src->pixel_format != vs_dst->pixel_format) return GF_NOT_SUPPORTED;
	BPP = get_bpp(vs_src->pixel_format);
	if (!BPP) return GF_NOT_SUPPORTED;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	} else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	/*go to start of src*/
	src = vs_src->video_buffer + vs_src->pitch_x * oy + BPP * ox;

	for (i = 0; i<h; i++) {
		memcpy(vs_dst->video_buffer + i*vs_dst->pitch_y, src, sizeof(u8) * BPP * w);
		src += vs_src->pitch_y;
	}
	return GF_OK;
}


static GF_Err color_write_rgb_to_32(GF_VideoSurface *vs_dst, GF_VideoSurface *vs_src, GF_Window *_src_wnd)
{
	u32 i, j, w, h, ox, oy;
	u8 *src;
	Bool isBGR;
	u8 *dst, *cur;
	u32 BPP = get_bpp(vs_src->pixel_format);
	if (!BPP) return GF_NOT_SUPPORTED;

	if (_src_wnd) {
		w = _src_wnd->w;
		h = _src_wnd->h;
		ox = _src_wnd->x;
		oy = _src_wnd->y;
	}
	else {
		w = vs_src->width;
		h = vs_src->height;
		ox = oy = 0;
	}

	/*go to start of src*/
	src = vs_src->video_buffer + vs_src->pitch_y * oy + BPP * ox;

	if (vs_src->pixel_format == vs_dst->pixel_format) {
		for (i = 0; i<h; i++) {
			memcpy(vs_dst->video_buffer + i*vs_dst->pitch_y, src, sizeof(u8) * BPP * w);
		}
		return GF_OK;
	}
	/*get all pixels*/
	isBGR = (vs_dst->pixel_format == GF_PIXEL_BGRX) ? GF_TRUE : GF_FALSE;
	if (isBGR) {
		switch (vs_src->pixel_format) {
		case GF_PIXEL_RGB:
		case GF_PIXEL_RGBS:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_RGBDS:
		case GF_PIXEL_RGBD:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_BGR:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
				}
			}
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
	}
	else {
		switch (vs_src->pixel_format) {
		case GF_PIXEL_RGB:
		case GF_PIXEL_RGBS:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_RGBD:
		case GF_PIXEL_RGBDS:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[2] = *cur++;
					dst[1] = *cur++;
					dst[0] = *cur++;
					cur++;
					dst += 4;
				}
			}
			break;
		case GF_PIXEL_BGR:
			for (i = 0; i<h; i++) {
				dst = (u8*)vs_dst->video_buffer + i*vs_dst->pitch_y;
				cur = src + i*vs_src->pitch_y;
				for (j = 0; j<w; j++) {
					dst[0] = *cur++;
					dst[1] = *cur++;
					dst[2] = *cur++;
					dst += 4;
				}
			}
			break;
		default:
			return GF_NOT_SUPPORTED;
		}
	}
	return GF_OK;
}

#endif


/* Basic SVG datatype parsing functions */
static const struct predef_col {
	const char *name;
	u8 r;
	u8 g;
	u8 b;
} predefined_colors[] =
{
	{"aliceblue",240, 248, 255},
	{"antiquewhite",250, 235, 215},
	{"aquamarine",127, 255, 212},
	{"azure",240, 255, 255},
	{"beige",245, 245, 220},
	{"bisque",255, 228, 196},
	{"black", 0, 0, 0},
	{"blanchedalmond",255, 235, 205},
	{"blue", 0, 0, 255},
	{"blueviolet",138, 43, 226},
	{"brown",165, 42, 42},
	{"burlywood",222, 184, 135},
	{"cadetblue", 95, 158, 160},
	{"chartreuse",127, 255, 0},
	{"chocolate",210, 105, 30},
	{"coral",255, 127, 80},
	{"lightpink",255, 182, 193},
	{"lightsalmon",255, 160, 122},
	{"lightseagreen", 32, 178, 170},
	{"lightskyblue",135, 206, 250},
	{"lightslategray",119, 136, 153},
	{"lightslategrey",119, 136, 153},
	{"lightsteelblue",176, 196, 222},
	{"lightyellow",255, 255, 224},
	{"lime", 0, 255, 0},
	{"limegreen", 50, 205, 50},
	{"linen",250, 240, 230},
	{"magenta",255, 0, 255},
	{"maroon",128, 0, 0},
	{"mediumaquamarine",102, 205, 170},
	{"mediumblue", 0, 0, 205},
	{"mediumorchid",186, 85, 211},
	{"cornflowerblue",100, 149, 237},
	{"cornsilk",255, 248, 220},
	{"crimson",220, 20, 60},
	{"cyan", 0, 255, 255},
	{"darkblue", 0, 0, 139},
	{"darkcyan", 0, 139, 139},
	{"darkgoldenrod",184, 134, 11},
	{"darkgray",169, 169, 169},
	{"darkgreen", 0, 100, 0},
	{"darkgrey",169, 169, 169},
	{"darkkhaki",189, 183, 107},
	{"darkmagenta",139, 0, 139},
	{"darkolivegreen", 85, 107, 47},
	{"darkorange",255, 140, 0},
	{"darkorchid",153, 50, 204},
	{"darkred",139, 0, 0},
	{"darksalmon",233, 150, 122},
	{"darkseagreen",143, 188, 143},
	{"darkslateblue", 72, 61, 139},
	{"darkslategray", 47, 79, 79},
	{"darkslategrey", 47, 79, 79},
	{"darkturquoise", 0, 206, 209},
	{"darkviolet",148, 0, 211},
	{"deeppink",255, 20, 147},
	{"deepskyblue", 0, 191, 255},
	{"dimgray",105, 105, 105},
	{"dimgrey",105, 105, 105},
	{"dodgerblue", 30, 144, 255},
	{"firebrick",178, 34, 34},
	{"floralwhite",255, 250, 240},
	{"forestgreen", 34, 139, 34},
	{"fuchsia",255, 0, 255},
	{"gainsboro",220, 220, 220},
	{"ghostwhite",248, 248, 255},
	{"gold",255, 215, 0},
	{"goldenrod",218, 165, 32},
	{"gray",128, 128, 128},
	{"grey",128, 128, 128},
	{"green", 0, 128, 0},
	{"greenyellow",173, 255, 47},
	{"honeydew",240, 255, 240},
	{"hotpink",255, 105, 180},
	{"indianred",205, 92, 92},
	{"indigo", 75, 0, 130},
	{"ivory",255, 255, 240},
	{"khaki",240, 230, 140},
	{"lavender",230, 230, 25},
	{"lavenderblush",255, 240, 245},
	{"mediumpurple",147, 112, 219},
	{"mediumseagreen", 60, 179, 113},
	{"mediumslateblue",123, 104, 238},
	{"mediumspringgreen", 0, 250, 154},
	{"mediumturquoise", 72, 209, 204},
	{"mediumvioletred",199, 21, 133},
	{"midnightblue", 25, 25, 112},
	{"mintcream",245, 255, 250},
	{"mistyrose",255, 228, 225},
	{"moccasin",255, 228, 181},
	{"navajowhite",255, 222, 173},
	{"navy", 0, 0, 128},
	{"oldlace",253, 245, 230},
	{"olive",128, 128, 0},
	{"olivedrab",107, 142, 35},
	{"orange",255, 165, 0},
	{"orangered",255, 69, 0},
	{"orchid",218, 112, 214},
	{"palegoldenrod",238, 232, 170},
	{"palegreen",152, 251, 152},
	{"paleturquoise",175, 238, 238},
	{"palevioletred",219, 112, 147},
	{"papayawhip",255, 239, 213},
	{"peachpuff",255, 218, 185},
	{"peru",205, 133, 63},
	{"pink",255, 192, 203},
	{"plum",221, 160, 221},
	{"powderblue",176, 224, 230},
	{"purple",128, 0, 128},
	{"red",255, 0, 0},
	{"rosybrown",188, 143, 143},
	{"royalblue", 65, 105, 225},
	{"saddlebrown",139, 69, 19},
	{"salmon",250, 128, 114},
	{"sandybrown",244, 164, 96},
	{"seagreen", 46, 139, 87},
	{"seashell",255, 245, 238},
	{"sienna",160, 82, 45},
	{"silver",192, 192, 192},
	{"skyblue",135, 206, 235},
	{"slateblue",106, 90, 205},
	{"slategray",112, 128, 144},
	{"slategrey",112, 128, 144},
	{"snow",255, 250, 250},
	{"springgreen", 0, 255, 127},
	{"steelblue", 70, 130, 180},
	{"tan",210, 180, 140},
	{"teal", 0, 128, 128},
	{"lawngreen",124, 252, 0},
	{"lemonchiffon",255, 250, 205},
	{"lightblue",173, 216, 230},
	{"lightcoral",240, 128, 128},
	{"lightcyan",224, 255, 255},
	{"lightgoldenrodyellow",250, 250, 210},
	{"lightgray",211, 211, 211},
	{"lightgreen",144, 238, 144},
	{"lightgrey",211, 211, 211},
	{"thistle",216, 191, 216},
	{"tomato",255, 99, 71},
	{"turquoise", 64, 224, 208},
	{"violet",238, 130, 238},
	{"wheat",245, 222, 179},
	{"white",255, 255, 255},
	{"whitesmoke",245, 245, 245},
	{"yellow",255, 255, 0},
	{"yellowgreen",154, 205, 50},
	{"aqua", 0, 255, 255},

};


GF_EXPORT
GF_Color gf_color_parse(const char *name)
{
	u32 i, count;
	u32 res;
	if (!strcmp(name, "none") )
		return 0;
	if ((name[0]=='$') || (name[0]=='#')) {
		sscanf(name+1, "%x", &res);
		if (strlen(name+1) == 8) return res;
		return res | 0xFF000000;
	}
	if (!strnicmp(name, "0x", 2) ) {
		sscanf(name+2, "%x", &res);
		if (strlen(name+2) == 8) return res;
		return res | 0xFF000000;
	}

	count = sizeof(predefined_colors) / sizeof(struct predef_col);
	for (i=0; i<count; i++) {
		if (!strcmp(name, predefined_colors[i].name)) {
			res = GF_COL_ARGB(0xFF, predefined_colors[i].r, predefined_colors[i].g, predefined_colors[i].b);
			return res;
		}
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknown color code %s, using 0x00000000\n", name));
	return 0;

}

GF_EXPORT
const char *gf_color_get_name(GF_Color col)
{
	u32 i, count;
	u8 r, g, b;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	count = sizeof(predefined_colors) / sizeof(struct predef_col);
	for (i=0; i<count; i++) {
		if (predefined_colors[i].r != r) continue;
		if (predefined_colors[i].g != g) continue;
		if (predefined_colors[i].b != b) continue;
		return predefined_colors[i].name;
	}
	return NULL;
}


GF_EXPORT
Bool gf_color_enum(u32 *idx, GF_Color *col, const char **color_name)
{
	u32 count = sizeof(predefined_colors) / sizeof(struct predef_col);
	if (*idx>=count) return GF_FALSE;
	if (col) *col = GF_COL_ARGB(0xFF, predefined_colors[*idx].r, predefined_colors[*idx].g, predefined_colors[*idx].b);
	if (color_name) *color_name = predefined_colors[*idx].name;
	(*idx)++;
	return GF_TRUE;
}
