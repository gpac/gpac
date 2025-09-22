/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / software 2D rasterizer
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
 *
 */


#include "rast_soft.h"

#ifndef GPAC_DISABLE_EVG

/*
	FIXME / WARNING - this code only works for little endian platfoms
		not seen any big endian platform using RGB565/RGB555 output
*/


static GFINLINE s32
mul255(s32 a, s32 b)
{
	return ((a + 1) * b) >> 8;
}


/*
			RGB 565 part
*/

static void write_565(u8 *dst, u8 r, u8 g, u8 b)
{
	r>>=3;
	g>>=2;
	b>>=3;

	dst[0] = r;
	dst[0] <<= 3;
	dst[0] |= ((g >> 3) & 0x7);
	dst[1] = (g & 0x7);
	dst[1] <<= 5;
	dst[1] |= b;
}

static void overmask_565(u32 src, u8 *dst, u32 alpha)
{
	u32 resr, resg, resb;
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	s32 dstr = (dst[0] >> 3) & 0x1f;
	s32 dstg = (dst[0]) & 0x7;
	dstg <<= 3;
	dstg |= (dst[1]>>3) & 0x7;
	s32 dstb = (dst[1]) & 0x1f;

	dstr<<=3;
	dstg<<=2;
	dstb<<=3;

	srca = mul255(srca, alpha);
	resr = mul255(srca, srcr - dstr) + dstr;
	resg = mul255(srca, srcg - dstg) + dstg;
	resb = mul255(srca, srcb - dstb) + dstb;
	write_565(dst, resr, resg, resb);
}

void overmask_565_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	u32 resr, resg, resb;
	u8 srca = (src >> 24) & 0xff;
	u8 srcr = (src >> 16) & 0xff;
	u8 srcg = (src >> 8) & 0xff;
	u8 srcb = (src >> 0) & 0xff;

	while (count) {
		s32 dstr = (dst[0] >> 3) & 0x1f;
		s32 dstg = (dst[0]) & 0x7;
		dstg <<= 3;
		dstg |= (dst[1]>>3) & 0x7;
		s32 dstb = (dst[1]) & 0x1f;

		dstr<<=3;
		dstg<<=2;
		dstb<<=3;

		resr = mul255(srca, srcr - dstr) + dstr;
		resg = mul255(srca, srcg - dstg) + dstg;
		resb = mul255(srca, srcb - dstb) + dstb;
		write_565(dst, resr, resg, resb);
		dst += dst_pitch_x;
		count--;
	}
}


void evg_565_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	u8 r = GF_COL_R(col);
	u8 g = GF_COL_G(col);
	u8 b = GF_COL_B(col);
	write_565(dst, r, g, b);
}

void evg_565_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_565(col, dst, coverage);
}


void evg_565_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	u8 r, g, b;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	register s32 i;
	u32 len;
	s32 x;

	col_no_a = col&0x00FFFFFF;
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	for (i=0; i<count; i++) {
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | (col_no_a);
			overmask_565_const_run(fin, dst+x, surf->pitch_x, len);
		} else {
			while (len--) {
				write_565(dst+x, r, g, b);
				x+=surf->pitch_x;
			}
		}
	}
}

void evg_565_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	register u32 a, fin, col_no_a;
	register s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col&0x00FFFFFF;
	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				s32 x = spans[i].x + j;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);
				fin = (fin<<24) | col_no_a;
				overmask_565_const_run(fin, dst + x * surf->pitch_x, surf->pitch_x, 1);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			fin = (fin<<24) | col_no_a;
			overmask_565_const_run(fin, dst + spans[i].x * surf->pitch_x, surf->pitch_x, spans[i].len);
		}
	}
}


void evg_565_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	register s32 i;

	for (i=0; i<count; i++) {
		register u8 spanalpha, col_a;
		register s32 x;
		register u32 len;
		register u32 *col;
		len = spans[i].len;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				u32 _col = *col;
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_565(_col, dst+x, spanalpha);
				} else {
					write_565(dst+x, GF_COL_R(_col), GF_COL_G(_col), GF_COL_B(_col));
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_565(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	register u32 x, y, w, h, sx, sy;
	register u8 r, g, b;
	u8 *data_o;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	/*convert to 565*/
	r = GF_COL_R(col);
	g = GF_COL_R(col);
	b = GF_COL_R(col);

	data_o = NULL;
	for (y=0; y<h; y++) {
		u8 *data = (u8 *) surf->pixels + (sy+y) * surf->pitch_y + surf->pitch_x*sx;
		if (!y) {
			data_o = data;
			for (x=0; x<w; x++)  {
				write_565(data, r, g, b);
				data += surf->pitch_x;
			}
		} else {
			memcpy(data, data_o, w*surf->pitch_x);
		}
	}
	return GF_OK;
}



/*
			RGB 555 part
*/

static void write_555(u8 *dst, u8 r, u8 g, u8 b)
{
	r>>=3;
	g>>=3;
	b>>=3;

	dst[0] = r;
	dst[0] <<= 2;
	dst[0] |= ((g >> 3) & 0x3);
	dst[1] = (g & 0x7);
	dst[1] <<= 5;
	dst[1] |= b;
}

static void overmask_555(u32 src, u8 *dst, u32 alpha)
{
	u32 resr, resg, resb;
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	s32 dstr = (dst[0] >> 2) & 0x1f;
	s32 dstg = (dst[0] ) & 0x3;
	dstg <<= 3;
	dstg |= (dst[1]>>5 ) & 0x7;
	s32 dstb = (dst[1] ) & 0x1f;
	dstr<<=3;
	dstg<<=3;
	dstb<<=3;

	srca = mul255(srca, alpha);
	resr = mul255(srca, srcr - dstr) + dstr;
	resg = mul255(srca, srcg - dstg) + dstg;
	resb = mul255(srca, srcb - dstb) + dstb;
	write_555(dst, resr, resg, resb);
}

static void overmask_555_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	u32 resr, resg, resb;
	u8 srca = (src >> 24) & 0xff;
	u8 srcr = (src >> 16) & 0xff;
	u8 srcg = (src >> 8) & 0xff;
	u8 srcb = (src >> 0) & 0xff;

	while (count) {
		s32 dstr = (dst[0] >> 2) & 0x1f;
		s32 dstg = (dst[0] ) & 0x3;
		dstg <<= 3;
		dstg |= (dst[1]>>5 ) & 0x7;
		s32 dstb = (dst[1] ) & 0x1f;
		dstr<<=3;
		dstg<<=3;
		dstb<<=3;

		resr = mul255(srca, srcr - dstr) + dstr;
		resg = mul255(srca, srcg - dstg) + dstg;
		resb = mul255(srca, srcb - dstb) + dstb;
		write_555(dst, resr, resg, resb);
		dst += dst_pitch_x;
		count--;
	}
}


void evg_555_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	u8 r = GF_COL_R(col);
	u8 g = GF_COL_G(col);
	u8 b = GF_COL_B(col);
	write_555(dst, r, g, b);
}

void evg_555_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_555(col, dst, coverage);
}

void evg_555_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	u8 *dst = surf->pixels + y * surf->pitch_y;
	u8 r, g, b;
	s32 i, x;
	u32 len;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	col_no_a = col&0x00FFFFFF;
	for (i=0; i<count; i++) {
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_555_const_run(fin, dst+x, surf->pitch_x, len);
		} else {
			while (len--) {
				write_555(dst+x, r, g, b);
				x+=surf->pitch_x;
			}
		}
	}
}

void evg_555_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				s32 x = spans[i].x + j;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);
				fin = (fin<<24) | col_no_a;
				overmask_555_const_run(fin, dst + x*surf->pitch_x, surf->pitch_x, 1);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			fin = (fin<<24) | col_no_a;
			overmask_555_const_run(fin, dst + spans[i].x*surf->pitch_x, surf->pitch_x, spans[i].len);
		}
	}
}


void evg_555_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		s32 x;
		u32 len;
		u32 *col;
		len = spans[i].len;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				u32 _col = *col;
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_555(*col, dst+x, spanalpha);
				} else {
					write_555(dst+x, GF_COL_R(_col), GF_COL_G(_col), GF_COL_B(_col) );
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_555(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, sx, sy;
	u8 r, g, b;
	u8 *data_o;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	data_o = NULL;
	for (y=0; y<h; y++) {
		u8 *data = surf->pixels + (sy+y) * surf->pitch_y + surf->pitch_x*sx;
		if (!y) {
			data_o = data;
			for (x=0; x<w; x++)  {
				write_555(data, r, g, b);
				data += surf->pitch_x;
			}
		} else {
			memcpy(data, data_o, w*surf->pitch_x);
		}
	}
	return GF_OK;
}


/*
			RGB 444 part
*/

static void write_444(u8 *dst, u8 r, u8 g, u8 b)
{
	dst[0] = r >> 4;
	dst[1] = g >> 4;
	dst[1]<<=4;
	dst[1] |= b >> 4;
}

static void overmask_444(u8 *dst, u32 src, u32 alpha)
{
	u32 resr, resg, resb;
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	s32 dstr = (dst[0] & 0x0f) << 4;
	s32 dstg = ((dst[1] >> 4) & 0x0f) << 4;
	s32 dstb = (dst[1] & 0x0f) << 4;

	srca = mul255(srca, alpha);
	resr = mul255(srca, srcr - dstr) + dstr;
	resg = mul255(srca, srcg - dstg) + dstg;
	resb = mul255(srca, srcb - dstb) + dstb;
	write_444(dst, resr, resg, resb);
}

static void overmask_444_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	u32 resr, resg, resb;
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	while (count) {
		s32 dstr = dst[0] & 0x0f;
		s32 dstg = (dst[1]>>4) & 0x0f;
		s32 dstb = (dst[1]) & 0x0f;

		dstr<<=4;
		dstg<<=4;
		dstb<<=4;

		resr = mul255(srca, srcr - dstr) + dstr;
		resg = mul255(srca, srcg - dstg) + dstg;
		resb = mul255(srca, srcb - dstb) + dstb;
		write_444(dst, resr, resg, resb);

		dst += dst_pitch_x;
		count--;
	}
}


void evg_444_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	u8 r = GF_COL_R(col);
	u8 g = GF_COL_G(col);
	u8 b = GF_COL_B(col);
	write_444(dst, r, g, b);
}

void evg_444_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_444(dst, col, coverage);
}

void evg_444_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	u8 *dst = surf->pixels + y * surf->pitch_y;
	u8 r, g, b;
	s32 i, x;
	u32 len;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	col_no_a = col&0x00FFFFFF;
	for (i=0; i<count; i++) {
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_444_const_run(fin, dst+x, surf->pitch_x, len);
		} else {
			while (len--) {
				write_444(dst+x, r, g, b);
				x+=surf->pitch_x;
			}
		}
	}
}

void evg_444_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;


	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				s32 x = spans[i].x + j;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);
				fin = (fin<<24) | col_no_a;
				overmask_444_const_run(fin, dst + x*surf->pitch_x, surf->pitch_x, 1);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			fin = (fin<<24) | col_no_a;
			overmask_444_const_run(fin, dst + spans[i].x*surf->pitch_x, surf->pitch_x, spans[i].len);
		}
	}
}


void evg_444_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		s32 x;
		u32 len;
		u32 *col;
		len = spans[i].len;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				u32 _col = *col;
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_444(dst+x, _col, spanalpha);
				} else {
					write_444(dst+x, GF_COL_R(_col), GF_COL_G(_col), GF_COL_B(_col) );
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_444(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, sx, sy;
	u8 r, g, b;
	u8 *data_o;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	data_o = NULL;
	for (y=0; y<h; y++) {
		u8 *data = surf->pixels + (sy+y) * surf->pitch_y + surf->pitch_x*sx;
		if (!y) {
			data_o = data;
			for (x=0; x<w; x++)  {
				write_444(data, r, g, b);
				data += surf->pitch_x;
			}
		} else {
			memcpy(data, data_o, w * surf->pitch_x);
		}
	}
	return GF_OK;
}

#endif // GPAC_DISABLE_EVG
