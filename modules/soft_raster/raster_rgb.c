/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / software 2D rasterizer module
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
 *		
 */

#include "rast_soft.h"

static s32
mul255(s32 a, s32 b)
{
	return ((a + 1) * b) >> 8;
}


/*
			RGB part
*/

static void overmask_rgb(u32 src, char *dst, u32 alpha)
{
	s32 srca = (src >> 24) & 0xff;
	u32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src) & 0xff;

	s32 dstr = (*dst) & 0xFF;
	s32 dstg = *(dst+1) & 0xFF;
	s32 dstb = *(dst+2) & 0xFF;

	srca = mul255(srca, alpha);
	*dst = mul255(srca, srcr - dstr) + dstr;
	*(dst+1) = mul255(srca, srcg - dstg) + dstg;
	*(dst+2) = mul255(srca, srcb - dstb) + dstb;
}

static void overmask_rgb_const_run(u32 src, char *dst, u32 count)
{
	u8 srca = (src >> 24) & 0xff;
	u8 srcr = (src >> 16) & 0xff;
	u8 srcg = (src >> 8) & 0xff;
	u8 srcb = (src) & 0xff;

	while (count) {
		u8 dstr = *(dst);
		u8 dstg = *(dst+1);
		u8 dstb = *(dst+2);
		*dst = (u8) mul255(srca, srcr - dstr) + dstr;
		*(dst+1) = (u8) mul255(srca, srcg - dstg) + dstg;
		*(dst+2) = (u8) mul255(srca, srcb - dstb) + dstb;
		dst += 3;
		count--;
	}
}

void evg_rgb_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	char *dst = surf->pixels + y * surf->stride;
	char *p;
	s32 i;
	u32 x, len, r, g, b;
	u8 aa_lev = surf->AALevel;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		x = spans[i].x * 3;
		len = spans[i].len;
		p = dst + x;
	
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_rgb_const_run(fin, p, len);
		} else {
			while (len--) {
				*(p) = r;
				*(p + 1) = g;
				*(p + 2) = b;
				p += 3;
			}
		}
	}
}

void evg_rgb_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->stride;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;
	u8 aa_lev = surf->AALevel;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | (col&0x00FFFFFF);
		overmask_rgb_const_run(fin, dst + 3 * spans[i].x, spans[i].len);
	}
}


void evg_rgb_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->stride;
	u8 spanalpha, col_a;
	s32 i;
	u32 x, len, bpp;
	u32 *col;
	u8 aa_lev = surf->AALevel;
	bpp = surf->BPP;

	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		x = 3*spans[i].x;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_rgb(*col, dst + x, spanalpha);
				} else {
					dst[x] = GF_COL_R(*col);
					dst[x+1] = GF_COL_G(*col);
					dst[x+2] = GF_COL_B(*col);
				}
			}
			col++;
			x += 3;
		}
	}
}

GF_Err evg_surface_clear_rgb(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, st, sx, sy;
	u8 r, g, b;
	EVGSurface *_this = (EVGSurface *)surf;
	st = _this->stride;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	for (y = 0; y < h; y++) {
		char *data = _this ->pixels + (y + sy) * st + 3*sx;
		for (x = 0; x < w; x++) {
			*(data) = r;
			*(data+1) = g;
			*(data+2) = b;
			data += 3;
		}
	}
	return GF_OK;
}


/*

			BGR part
*/

static void overmask_bgr(u32 src, char *dst, u32 alpha)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src) & 0xff;

	s32 dstb = *dst & 0xFF;
	s32 dstg = *(dst+1) & 0xFF;
	s32 dstr = *(dst+2) & 0xFF;

	srca = mul255(srca, alpha);
	*(dst) = mul255(srca, srcb - dstb) + dstb;
	*(dst+1) = mul255(srca, srcg - dstg) + dstg;
	*(dst+2) = mul255(srca, srcr - dstr) + dstr;
}

static void overmask_bgr_const_run(u32 src, char *dst, u32 count)
{
	u8 srca = (src >> 24) & 0xff;
	u8 srcr = (src >> 16) & 0xff;
	u8 srcg = (src >> 8) & 0xff;
	u8 srcb = (src) & 0xff;

	while (count) {
		u8 dstb = *(dst);
		u8 dstg = *(dst+1);
		u8 dstr = *(dst+2);
		*dst = (u8) mul255(srca, srcb - dstb) + dstb;
		*(dst+1) = (u8) mul255(srca, srcg - dstg) + dstg;
		*(dst+2) = (u8) mul255(srca, srcr - dstr) + dstr;
		dst += 3;
		count--;
	}
}

void evg_bgr_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	char *dst = surf->pixels + y * surf->stride;
	char *p;
	s32 i;
	u32 x, len, r, g, b;
	u8 aa_lev = surf->AALevel;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		x = spans[i].x * 3;
		len = spans[i].len;
		p = dst + x;
	
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_bgr_const_run(fin, p, len);
		} else {
			while (len--) {
				*(p) = b;
				*(p + 1) = g;
				*(p + 2) = r;
				p += 3;
			}
		}
	}
}

void evg_bgr_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->stride;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;
	u8 aa_lev = surf->AALevel;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | col_no_a;
		overmask_bgr_const_run(fin, dst + 3 * spans[i].x, spans[i].len);
	}
}


void evg_bgr_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	char *dst = surf->pixels + y * surf->stride;
	u8 spanalpha, col_a;
	s32 i;
	u32 x, len, bpp;
	u32 *col;
	u8 aa_lev = surf->AALevel;
	bpp = surf->BPP;

	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		x = spans[i].x * bpp;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, x, y, len);
		col = surf->stencil_pix_run;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_bgr(*col, dst + x, spanalpha);
				} else {
					dst[x] = GF_COL_B(*col);
					dst[x+1] = GF_COL_G(*col);
					dst[x+2] = GF_COL_R(*col);
				}
			}
			col++;
			x += 3;
		}
	}
}

GF_Err evg_surface_clear_bgr(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, st, sx, sy;
	u8 r, g, b;
	EVGSurface *_this = (EVGSurface *)surf;
	st = _this->stride;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	for (y = 0; y < h; y++) {
		char *data = _this ->pixels + (y+sy) * st + 3*sx;
		for (x = 0; x < w; x++) {
			*(data) = b;
			*(data+1) = g;
			*(data+2) = r;
			data += 3;
		}
	}
	return GF_OK;
}


/*
	user-defined callbacks
*/

void evg_user_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col_no_a;
	s32 i;
	u8 aa_lev = surf->AALevel;

	col_no_a = surf->fill_col;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
	
		if (spans[i].coverage != 0xFF) {
			u32 a = mul255(0xFF, spans[i].coverage);
			surf->raster_fill_run_alpha(surf->raster_cbk, spans[i].x, y, spans[i].len, col_no_a, a);
		} else {
			surf->raster_fill_run_no_alpha(surf->raster_cbk, spans[i].x, y, spans[i].len, col_no_a);
		}
	}
}

void evg_user_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col, a, col_no_a;
	s32 i;
	u8 aa_lev = surf->AALevel;

	a = (surf->fill_col>>24)&0xFF;
	col_no_a = surf->fill_col | 0xFF000000;
	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		col = mul255(a, spans[i].coverage);
		surf->raster_fill_run_alpha(surf->raster_cbk, spans[i].x, y, spans[i].len, col_no_a, col);
	}
}

void evg_user_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 spanalpha, col_a, a;
	s32 i;
	u32 x, len, bpp;
	u32 *col;
	u8 aa_lev = surf->AALevel;
	bpp = surf->BPP;

	for (i=0; i<count; i++) {
		if (spans[i].coverage<aa_lev) continue;
		len = spans[i].len;
		x = spans[i].x;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, x, y, len);
		col = surf->stencil_pix_run;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					a = mul255(col_a, spans[i].coverage);
					surf->raster_fill_run_alpha(surf->raster_cbk, x, y, 1, *col, a);
				} else {
					surf->raster_fill_run_no_alpha(surf->raster_cbk, x, y, 1, *col);
				}
			}
			col++;
			x ++;
		}
	}
}

GF_Err evg_surface_clear_user(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u32 y, w, h, sx, sy, a;
	EVGSurface *_this = (EVGSurface *)surf;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;
	a = GF_COL_A(col);
	if (a==0xFF) {
		for (y = 0; y < h; y++) {
			_this->raster_fill_run_no_alpha(_this->raster_cbk, sx, y+sy, w, col);
		}
	} else {
		col |= 0xFF000000;
		for (y = 0; y < h; y++) {
			_this->raster_fill_run_alpha(_this->raster_cbk, sx, y+sy, w, col, a);
		}
	}
	return GF_OK;
}
