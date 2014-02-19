/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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
 */

#include "rast_soft.h"


static GFINLINE s32
mul255(s32 a, s32 b)
{
	return ((a+1) * b) >> 8;
}

/*
		32 bit ARGB
*/

static void overmask_bgra(u32 src, u8 *dst, u32 alpha)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;
	s32 dsta = dst[3];
	srca = mul255(srca, alpha);
	if (dsta) {
		s32 dstr = dst[1];
		s32 dstg = dst[1];
		s32 dstb = dst[0];
		dst[0] = mul255(srca, srcb - dstb) + dstb;
		dst[1] = mul255(srca, srcg - dstg) + dstg;
		dst[2] = mul255(srca, srcr - dstr) + dstr;
		dst[3] = mul255(srca, srca) + mul255(255-srca, dsta);
	} else {
		dst[0] = srcb;
		dst[1] = srcg;
		dst[2] = srcr;
		dst[3] = srca;
	}
}

static void overmask_bgra_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;


	while (count) {
		s32 dsta = dst[3];
		/*special case for ARGB: if dst alpha is 0, consider the surface is empty and copy pixel*/
		if (dsta) {
			s32 dstb = dst[0];
			s32 dstg = dst[1];
			s32 dstr = dst[2];

			dst[0] = mul255(srca, srcb - dstb) + dstb;
			dst[1] = mul255(srca, srcg - dstg) + dstg;
			dst[2] = mul255(srca, srcr - dstr) + dstr;
			dst[3] = mul255(srca, srca) + mul255(255-srca, dsta);
		} else {
			dst[0] = srcr;
			dst[1] = srcg;
			dst[2] = srcr;
			dst[3] = srca;
		}
		dst += dst_pitch_x;
		count--;
	}
}

void evg_bgra_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i, x;
	u32 len;
	u8 col_a, col_r, col_g, col_b;

	col_a = GF_COL_A(col);
	col_r = GF_COL_R(col);
	col_g = GF_COL_G(col);
	col_b = GF_COL_B(col);
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;
	
		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			fin = (a<<24) | col_no_a;
			overmask_bgra_const_run(fin, dst + x, surf->pitch_x, len);
		} else {
			while (len--) {
				dst[x] = col_b;
				dst[x+1] = col_g;
				dst[x+2] = col_r;
				dst[x+3] = col_a;
				x += surf->pitch_x;
			}
		}
	}
}

void evg_bgra_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | col_no_a;
		overmask_bgra_const_run(fin, dst + surf->pitch_x*spans[i].x, surf->pitch_x, spans[i].len);
	}
}


void evg_bgra_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 spanalpha, col_a;
	s32 i, x;
	u32 len;
	u32 *col, _col;

	for (i=0; i<count; i++) {
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		x = spans[i].x * surf->pitch_x;
		col = surf->stencil_pix_run;
		while (len--) {
			_col = *col;
			col_a = GF_COL_A(_col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_bgra(*col, (dst + x) , spanalpha);
				} else {
					dst[x] = GF_COL_B(_col);
					dst[x+1] = GF_COL_G(_col);
					dst[x+2] = GF_COL_R(_col);
					dst[x+3] = col_a;
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_bgra(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u8 *data;
	u8 col_a, col_b, col_g, col_r;
	u32 x, y, w, h, sx, sy;
	s32 st;
	Bool use_memset;
	EVGSurface *_this = (EVGSurface *)surf;
	st = _this->pitch_y;

	col_a = GF_COL_A(col);
	col_r = GF_COL_R(col);
	col_g = GF_COL_G(col);
	col_b = GF_COL_B(col);

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;
	
	use_memset = 0;
	if (_this->pitch_x !=4) use_memset = 0;
	else if (!col_a) use_memset = 1;
	else if ((col_a==col_r) && (col_a==col_g) && (col_a==col_b)) use_memset = 1;

	if (!use_memset) {
		for (y = 0; y < h; y++) {
			data = (u8 *) _this ->pixels + (sy+y)* st + _this->pitch_x*sx;
			for (x = 0; x < w; x++) {
				data[0] = col_b;
				data[1] = col_g;
				data[2] = col_r;
				data[3] = col_a;
				data += _this->pitch_x;
			}
		}
	} else {
		u32 sw = 4*w;
		for (y = 0; y < h; y++) {
			memset(_this ->pixels + (sy+y)* st + _this->pitch_x * sx, col_a, sizeof(char)*sw);
		}
	}
	return GF_OK;
}


/*
		32 bit RGB
*/

static void overmask_bgrx(u32 src, u8 *dst, u32 alpha)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	s32 dstb = dst[0];
	s32 dstg = dst[1];
	s32 dstr = dst[2];

	srca = mul255(srca, alpha);
	dst[0] = mul255(srca, srcb - dstb) + dstb;
	dst[1] = mul255(srca, srcg - dstg) + dstg;
	dst[2] = mul255(srca, srcr - dstr) + dstr;
	dst[3] = 0xFF;
}

GFINLINE static void overmask_bgrx_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	s32 srca = (src>>24) & 0xff;
	u32 srcr = mul255(srca, ((src >> 16) & 0xff)) ;
	u32 srcg = mul255(srca, ((src >> 8) & 0xff)) ;
	u32 srcb = mul255(srca, ((src) & 0xff)) ;
	u32 inva = 1 + 0xFF - srca;

	while (count) {
		dst[0] = srcb + ((inva*dst[0])>>8);
		dst[1] = srcg + ((inva*dst[1])>>8);
		dst[2] = srcr + ((inva*dst[2])>>8);
		dst[3] = 0xFF;
		dst += dst_pitch_x;
		count--;
	}
}

void evg_bgrx_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 fin, col_no_a, spana;
	u8 col_r, col_g, col_b;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i, x;
	u32 len;

	col_no_a = col & 0x00FFFFFF;
	col_r = GF_COL_R(col);
	col_g = GF_COL_G(col);
	col_b = GF_COL_B(col);
	for (i=0; i<count; i++) {
		spana = spans[i].coverage;
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;

		if (spana != 0xFF) {
			fin = (spana<<24) | col_no_a;
			overmask_bgrx_const_run(fin, dst + x, surf->pitch_x, len);
		} else {
			while (len--) {
				dst[x] = col_b;
				dst[x+1] = col_g;
				dst[x+2] = col_r;
				dst[x+3] = 0xFF;
				x += surf->pitch_x;
			}
		}
	}
}

void evg_bgrx_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | col_no_a;
		overmask_bgrx_const_run(fin, dst + surf->pitch_x*spans[i].x, surf->pitch_x, spans[i].len);
	}
}


void evg_bgrx_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 spanalpha, col_a;
	s32 i, x;
	u32 len;
	u32 *col;

	for (i=0; i<count; i++) {
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			u32 _col = *col;
			col_a = GF_COL_A(_col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_bgrx(*col, dst+x, spanalpha);
				} else {
					dst[x] = GF_COL_B(_col);
					dst[x+1] = GF_COL_G(_col);
					dst[x+2] = GF_COL_R(_col);
					dst[x+3] = 0xFF;
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

/*
		32 bit BGR
*/

static void overmask_rgbx(u32 src, u8 *dst, u32 alpha)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;
	
	s32 dstr = dst[0];
	s32 dstg = dst[1];
	s32 dstb = dst[2];

	srca = mul255(srca, alpha);
	dst[0] = mul255(srca, srcr - dstr) + dstr;
	dst[1] = mul255(srca, srcg - dstg) + dstg;
	dst[2] = mul255(srca, srcb - dstb) + dstb;
	dst[3] = 0xFF;
}

GFINLINE static void overmask_rgbx_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count)
{
	s32 srca = (src>>24) & 0xff;
	u32 srcr = mul255(srca, ((src >> 16) & 0xff)) ;
	u32 srcg = mul255(srca, ((src >> 8) & 0xff)) ;
	u32 srcb = mul255(srca, ((src) & 0xff)) ;
	u32 inva = 1 + 0xFF - srca;

	while (count) {
		dst[0] = srcr + ((inva*dst[0])>>8);
		dst[1] = srcg + ((inva*dst[1])>>8);
		dst[2] = srcb + ((inva*dst[2])>>8);
		dst += dst_pitch_x;
		count--;
	}
}

void evg_rgbx_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 fin, col_no_a, spana;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 r, g, b;
	s32 i, x;
	u32 len;

	col_no_a = col & 0x00FFFFFF;
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	for (i=0; i<count; i++) {
		spana = spans[i].coverage;
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;

		if (spana != 0xFF) {
			fin = (spana<<24) | col_no_a;
			overmask_rgbx_const_run(fin, dst + x, surf->pitch_x, len);
		} else {
			while (len--) {
				dst[x] = r;
				dst[x+1] = g;
				dst[x+2] = b;
				dst[x+3] = 0xFF;
				x += surf->pitch_x;
			}
		}
	}
}

void evg_rgbx_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_no_a;
	s32 i;

	a = (col>>24)&0xFF;
	col_no_a = col & 0x00FFFFFF;
	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | col_no_a;
		overmask_rgbx_const_run(fin, dst + surf->pitch_x*spans[i].x, surf->pitch_x, spans[i].len);
	}
}


void evg_rgbx_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 spanalpha, col_a;
	s32 i, x;
	u32 len;
	u32 *col, _col;

	for (i=0; i<count; i++) {
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			_col = *col;
			col_a = GF_COL_A(_col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_rgbx(*col, dst+x, spanalpha);
				} else {
					dst[x] = GF_COL_R(_col);
					dst[x+1] = GF_COL_G(_col);
					dst[x+2] = GF_COL_B(_col);
					dst[x+3] = 0xFF;
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_rgbx(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, sx, sy;
	u8 r,g,b;
	s32 st;
	EVGSurface *_this = (EVGSurface *)surf;
	st = _this->pitch_x;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);
	col = GF_COL_ARGB(0xFF, b, g, r);
	for (y = 0; y < h; y++) {
		u8 *data = (u8 *) _this ->pixels + (y + sy) * _this->pitch_y + st*sx;
		for (x = 0; x < w; x++) {
			data[0] = r;
			data[1] = g;
			data[2] = b;
			data[3] = 0xFF;
			data += st;
		}
	}
	return GF_OK;
}



/*
		32 bit RGBA
*/

GFINLINE static void overmask_rgba(u32 src, u8 *dst, u32 alpha)
{
	u8 srca = GF_COL_A(src);
	u8 srcr = GF_COL_R(src);
	u8 srcg = GF_COL_G(src);
	u8 srcb = GF_COL_B(src);
	u8 dsta = dst[3];
	srca = mul255(srca, alpha);
	if (dsta) {
		u8 dstr = dst[0];
		u8 dstg = dst[1];
		u8 dstb = dst[2];
		dst[0] = mul255(srca, srcr - dstr) + dstr;
		dst[1] = mul255(srca, srcg - dstg) + dstg;
		dst[2] = mul255(srca, srcb - dstb) + dstb;
		if (dsta==0xFF) dst[3] = (u8)0xFF;
		else dst[3] = mul255(srca, srca) + mul255(255-srca, dsta);
	} else {
		dst[0] = srcr;
		dst[1] = srcg;
		dst[2] = srcb;
		dst[3] = srca;
	}
}

GFINLINE static void overmask_rgba_const_run(u32 src, u8 *dst, s32 dst_pitch_x,  u32 count)
{
	u8 srca = GF_COL_A(src);
	u8 srcr = GF_COL_R(src);
	u8 srcg = GF_COL_G(src);
	u8 srcb = GF_COL_B(src);

	while (count) {
		u8 dsta = dst[3];
		/*special case for RGBA: if dst alpha is 0, consider the surface is empty and copy pixel*/
		if (dsta) {
			u8 dstr = dst[0];
			u8 dstg = dst[1];
			u8 dstb = dst[2];
			dst[0] = (u8) mul255(srca, srcr - dstr) + dstr;
			dst[1] = (u8) mul255(srca, srcg - dstg) + dstg;
			dst[2] = (u8) mul255(srca, srcb - dstb) + dstb;
			if (dsta==0xFF) dst[3] = (u8)0xFF;
			else dst[3] = (u8) mul255(srca, srca) + mul255(255-srca, dsta);
		} else {
			dst[0] = srcr;
			dst[1] = srcg;
			dst[2] = srcb;
			dst[3] = srca;
		}
		dst+=dst_pitch_x;
		count--;
	}
}

void evg_rgba_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u32 col = surf->fill_col;
	u32 new_a, fin, col_no_a;
	u8 a, r, g, b;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 *p;
	s32 i;
	u32 len;

	a = GF_COL_A(col);
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);
	col_no_a = col & 0x00FFFFFF;

	for (i=0; i<count; i++) {
		p = dst + spans[i].x*surf->pitch_x;
		len = spans[i].len;
	
		if (spans[i].coverage != 0xFF) {
			new_a = spans[i].coverage;
			fin = (new_a<<24) | col_no_a;
			overmask_rgba_const_run(fin, p, surf->pitch_x, len);
		} else {
			while (len--) {
				*(p) = r;
				*(p+1) = g;
				*(p+2) = b;
				*(p+3) = a;
				p += surf->pitch_x;
			}
		}
	}
}

void evg_rgba_fill_erase(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 *p;
	s32 i;
	u32 len;

	for (i=0; i<count; i++) {
		p = dst + spans[i].x*surf->pitch_x;
		len = spans[i].len;
	
		if (spans[i].coverage != 0xFF) {
/*			while (len--) {
				*p = 0xFF-spans[i].coverage;
				p += surf->pitch_x;
			}
*/		} else {
			while (len--) {
				*(u32 *)p = 0;
				p += surf->pitch_x;
			}
		}
	}
}

void evg_rgba_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 a, fin, col_no_a;
	s32 i;

	a = GF_COL_A(surf->fill_col);
	col_no_a = surf->fill_col & 0x00FFFFFF;

	for (i=0; i<count; i++) {
		fin = mul255(a, spans[i].coverage);
		fin = (fin<<24) | col_no_a;
		overmask_rgba_const_run(fin, dst + spans[i].x*surf->pitch_x, surf->pitch_x, spans[i].len);
	}
}


void evg_rgba_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u8 *p;
	u8 spanalpha, col_a;
	s32 i;
	u32 len;
	u32 *col;

	for (i=0; i<count; i++) {
		p = dst + spans[i].x * surf->pitch_x;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		surf->sten->fill_run(surf->sten, surf, spans[i].x, y, len);
		col = surf->stencil_pix_run;
		while (len--) {
			col_a = GF_COL_A(*col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_rgba(*col, p, spanalpha);
				} else {
					u32 _col = *col;
					p[0] = GF_COL_R(_col);
					p[1] = GF_COL_G(_col);
					p[2] = GF_COL_B(_col);
					p[3] = GF_COL_A(_col);
				}
			}
			col++;
			p += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_rgba(GF_SURFACE surf, GF_IRect rc, GF_Color col)
{
	u8 *data;
	u8 a, r, g, b;
	u32 x, y, w, h, sy;
	s32 st;
	Bool use_memset;
	EVGSurface *_this = (EVGSurface *)surf;
	st = _this->pitch_y;

	h = rc.height;
	w = rc.width;
	sy = rc.y;

	a = GF_COL_A(col);
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	use_memset = 0;
	if (_this->pitch_x !=4) use_memset = 0;
	else if (!a) use_memset = 1;
	else if ((a==r) && (a==g) && (a==b)) use_memset = 1;


	if (!use_memset) {
		for (y = 0; y < h; y++) {
			data = (u8 *) _this ->pixels + (sy+y)* st + _this->pitch_x * rc.x;
			for (x = 0; x < w; x++) {
				*(data) = r;
				*(data+1) = g;
				*(data+2) = b;
				*(data+3) = a;
				data += 4;
			}
		}
	} else {
		u32 sw = 4*w;
		for (y = 0; y < h; y++) {
			memset(_this ->pixels + (sy+y)* st + _this->pitch_x * rc.x, a, sizeof(char)*sw);
		}
	}
	return GF_OK;
}

