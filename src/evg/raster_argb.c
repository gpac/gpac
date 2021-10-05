/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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


static GFINLINE s32
mul255(s32 a, s32 b)
{
	return ((a+1) * b) >> 8;
}


/*
		32 bit ARGB/BGRA/RGBA
*/

u32 do_composite_mode(GF_EVGCompositeMode comp_mode, s32 *srca, s32 *dsta)
{
	switch (comp_mode) {
	case GF_EVG_SRC_ATOP:
		if (*srca && *dsta) {}
		else if (*dsta) { *srca = 0; }
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_SRC_IN:
		if (*srca && *dsta) {}
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_SRC_OUT:
		if (*srca && !*dsta) {}
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_DST_ATOP:
		if (*srca && *dsta) { *srca = 0;}
		else if (*srca) { *dsta = 0; }
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_DST_IN:
		if (*srca && *dsta) { *srca = 0; }
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_DST_OUT:
		if (!*srca && *dsta) {*srca = 0;}
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_DST_OVER:
		if (*dsta) {*srca = 0;}
		else { *dsta = *srca = 0; }
		break;
	case GF_EVG_LIGHTER:
		return 1;
	case GF_EVG_COPY:
		*dsta = 0;
		break;
	case GF_EVG_XOR:
		return 2;
	case GF_EVG_SRC_OVER:
		return 0;
	}
	return 0;
}

GFINLINE static void overmask_argb(u32 src, u8 *dst, u32 alpha, GF_EVGSurface *surf)
{
	u32 cmode;
	s32 srca = GF_COL_A(src);
	s32 srcr = GF_COL_R(src);
	s32 srcg = GF_COL_G(src);
	s32 srcb = GF_COL_B(src);
	s32 dsta = dst[surf->idx_a];

	srca = mul255(srca, alpha);

	cmode = do_composite_mode(surf->comp_mode, &srca, &dsta);
	if (cmode==1) {
		u8 dstr = dst[surf->idx_r];
		u8 dstg = dst[surf->idx_g];
		u8 dstb = dst[surf->idx_b];
//		dsta += srca;
//		if (srca>0xFF) srca = 0xFF;
		dstr += srcr;
		if (dstr>0xFF) dstr = 0xFF;
		dstg += srcg;
		if (dstg>0xFF) dstg = 0xFF;
		dstb += srcb;
		if (dstb>0xFF) dstb = 0xFF;
		dst[surf->idx_a] = (u8) srca;
		dst[surf->idx_r] = (u8) dstr;
		dst[surf->idx_g] = (u8) dstg;
		dst[surf->idx_b] = (u8) dstb;
		return;
	}
	if (cmode==2) {
		u8 dstr = dst[surf->idx_r];
		u8 dstg = dst[surf->idx_g];
		u8 dstb = dst[surf->idx_b];
		dst[surf->idx_a] = (u8) srca;
		dst[surf->idx_r] = (u8) (dstr^srcr);
		dst[surf->idx_g] = (u8) (dstg^srcg);
		dst[surf->idx_b] = (u8) (dstb^srcb);
		return;
	}

	/*special case for RGBA:
		if dst alpha is 0, consider the surface is empty and copy pixel
		if source alpha is 0xFF erase the entire pixel
	*/
	if (dsta && (srca!=0xFF) ) {
		s32 final_a;
		s32 dstr = dst[surf->idx_r];
		s32 dstg = dst[surf->idx_g];
		s32 dstb = dst[surf->idx_b];

		//do the maths , so that the result of the blend follows the same DST = SRC*apha + DST(1-alpha)
		//it gives a transform alpha of Fa = SRCa + DSTa - SRCa*DSTa
		//and an RGB Fc = (SRCa*SRCc + DSTa*DSTc - DSTc*(DSTa-SRCa)) / Fa
		final_a = dsta + srca - mul255(dsta, srca);
		if (final_a) {
			s32 res;
			dst[surf->idx_a] = final_a;
			res = (srcr*srca + dstr*(dsta-srca)) / final_a;
			if (res<0) res=0;
			dst[surf->idx_r] = (u8) (res);
			res = (srcg*srca + dstg*(dsta-srca)) / final_a;
			if (res<0) res=0;
			dst[surf->idx_g] = (u8) (res);
			res = (srcb*srca + dstb*(dsta-srca)) / final_a;
			if (res<0) res=0;
			dst[surf->idx_b] = (u8) (res);
		}
	} else {
		dst[surf->idx_a] = (u8) srca;
		dst[surf->idx_r] = (u8) srcr;
		dst[surf->idx_g] = (u8) srcg;
		dst[surf->idx_b] = (u8) srcb;
	}
}

GFINLINE static void overmask_argb_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count, GF_EVGSurface *surf)
{
	u8 const_srca = GF_COL_A(src);
	s32 srcr = GF_COL_R(src);
	s32 srcg = GF_COL_G(src);
	s32 srcb = GF_COL_B(src);

	while (count) {
		s32 srca = const_srca;
		s32 dsta = dst[surf->idx_a];

		do_composite_mode(surf->comp_mode, &srca, &dsta);

		/*special case for RGBA:
			if dst alpha is 0, consider the surface is empty and copy pixel
			if source alpha is 0xFF erase the entire pixel
		*/
		if ((dsta != 0) && (srca != 0xFF)) {
			s32 final_a;
			s32 dstr = dst[surf->idx_r];
			s32 dstg = dst[surf->idx_g];
			s32 dstb = dst[surf->idx_b];

			//do the maths , so that the result of the blend follows the same DST = SRC*apha + DST(1-alpha)
			//it gives a transform alpha of Fa = SRCa + DSTa - SRCa*DSTa
			//and an RGB Fc = (SRCa*SRCc + DSTa*DSTc - DSTc*(DSTa-SRCa)) / Fa
			final_a = dsta + srca - mul255(dsta, srca);
			if (final_a) {
				s32 res;
				dst[surf->idx_a] = final_a;
				res = (srcr*srca + dstr*(dsta-srca)) / final_a;
				if (res<0) res=0;
				dst[surf->idx_r] = (u8) (res);
				res = (srcg*srca + dstg*(dsta-srca)) / final_a;
				if (res<0) res=0;
				dst[surf->idx_g] = (u8) (res);
				res = (srcb*srca + dstb*(dsta-srca)) / final_a;
				if (res<0) res=0;
				dst[surf->idx_b] = (u8) (res);
			}
		} else {
			dst[surf->idx_a] = (u8) srca;
			dst[surf->idx_r] = (u8) srcr;
			dst[surf->idx_g] = (u8) srcg;
			dst[surf->idx_b] = (u8) srcb;
		}
		dst+=dst_pitch_x;
		count--;
	}
}

void evg_argb_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_argb(col, dst, 0xFF, surf);
}

void evg_argb_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_argb(col, dst, coverage, surf);
}


void evg_argb_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 col = surf->fill_col;
	u32 col_no_a;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i;

	col_no_a = col & 0x00FFFFFF;

	for (i=0; i<count; i++) {
		u32 new_a, fin;
		u8 *p;
		u32 len;
		p = dst + spans[i].x*surf->pitch_x;
		len = spans[i].len;

		new_a = spans[i].coverage;
		fin = (new_a<<24) | col_no_a;
		//we must blend in all cases since we have to merge with the dst alpha
		overmask_argb_const_run(fin, p, surf->pitch_x, len, surf);
	}
}

void evg_argb_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 a, fin, col_no_a;
	s32 i;

	a = GF_COL_A(surf->fill_col);
	col_no_a = surf->fill_col & 0x00FFFFFF;
	if (surf->get_alpha) {
		u32 j;
		for (i=0; i<count; i++) {
			for (j=0; j<spans[i].len; j++) {
				s32 x = spans[i].x+j;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);
				fin = (fin<<24) | col_no_a;
				overmask_argb_const_run(fin, dst + x * surf->pitch_x, surf->pitch_x, 1, surf);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			fin = (fin<<24) | col_no_a;
			overmask_argb_const_run(fin, dst + spans[i].x*surf->pitch_x, surf->pitch_x, spans[i].len, surf);
		}
	}
}

void evg_argb_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 *p;
		u8 spanalpha;
		u32 len;
		u32 *col;
		p = dst + spans[i].x * surf->pitch_x;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		while (len--) {
			//we must blend in all cases since we have to merge with the dst alpha
			overmask_argb(*col, p, spanalpha, surf);
			col++;
			p += surf->pitch_x;
		}
	}
}


void evg_argb_fill_erase(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 *p;
		u32 len;
		p = dst + spans[i].x*surf->pitch_x;
		len = spans[i].len;
		if (spans[i].coverage != 0xFF) {
		} else {
			while (len--) {
				*(u32 *)p = 0;
				p += surf->pitch_x;
			}
		}
	}
}

GF_Err evg_surface_clear_argb(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u8 *data, *o_data;
	u8 a, r, g, b;
	u32 x, y, w, h, sy;
	s32 st;
	GF_EVGSurface *_this = (GF_EVGSurface *)surf;
	st = _this->pitch_y;

	h = rc.height;
	w = rc.width;
	sy = rc.y;

	if (sy+h > _this->height) {
		h = _this->height - sy;
	}
	if (rc.x + w > _this->width) {
		w = _this->width - rc.x;
	}

	a = GF_COL_A(col);
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	o_data = NULL;
	for (y = 0; y < h; y++) {
		data = (u8 *) _this ->pixels + (sy+y)* st + _this->pitch_x * rc.x;
		if (!y) {
			o_data = data;
			for (x = 0; x < w; x++) {
				data[surf->idx_a] = a;
				data[surf->idx_r] = r;
				data[surf->idx_g] = g;
				data[surf->idx_b] = b;
				data += 4;
			}
		} else {
			memcpy(data, o_data, w*4);
		}
	}
	return GF_OK;
}

/*
		32 bit RGBX/XRGB/BGRX/XBGR
*/

static void overmask_rgbx(u32 src, u8 *dst, u32 alpha, GF_EVGSurface *surf)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcr = (src >> 16) & 0xff;
	s32 srcg = (src >> 8) & 0xff;
	s32 srcb = (src >> 0) & 0xff;

	s32 dstr = dst[surf->idx_r];
	s32 dstg = dst[surf->idx_g];
	s32 dstb = dst[surf->idx_b];

	srca = mul255(srca, alpha);
	dst[surf->idx_r] = mul255(srca, srcr - dstr) + dstr;
	dst[surf->idx_g] = mul255(srca, srcg - dstg) + dstg;
	dst[surf->idx_b] = mul255(srca, srcb - dstb) + dstb;
}

GFINLINE static void overmask_rgbx_const_run(u32 src, u8 *dst, s32 dst_pitch_x, u32 count, GF_EVGSurface *surf)
{
	s32 srca = (src>>24) & 0xff;
	u32 srcr = mul255(srca, ((src >> 16) & 0xff)) ;
	u32 srcg = mul255(srca, ((src >> 8) & 0xff)) ;
	u32 srcb = mul255(srca, ((src) & 0xff)) ;
	while (count) {
		u32 dstc;
		dstc = dst[surf->idx_r];
		dst[surf->idx_r] = mul255(srca, srcr - dstc) + dstc;
		dstc = dst[surf->idx_g];
		dst[surf->idx_g] = mul255(srca, srcg - dstc) + dstc;
		dstc = dst[surf->idx_b];
		dst[surf->idx_b] = mul255(srca, srcb - dstc) + dstc;
		dst += dst_pitch_x;
		count--;
	}
}


void evg_rgbx_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	u8 r = GF_COL_R(col);
	u8 g = GF_COL_G(col);
	u8 b = GF_COL_B(col);
	dst[surf->idx_r] = r;
	dst[surf->idx_g] = g;
	dst[surf->idx_b] = b;
}

void evg_rgbx_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_rgbx(col, dst, coverage, surf);
}

void evg_rgbx_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
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
			overmask_rgbx_const_run(fin, dst + x, surf->pitch_x, len, surf);
		} else {
			while (len--) {
				dst[x+surf->idx_r] = r;
				dst[x+surf->idx_g] = g;
				dst[x+surf->idx_b] = b;
				x += surf->pitch_x;
			}
		}
	}
}

void evg_rgbx_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
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
				overmask_rgbx_const_run(fin, dst + surf->pitch_x * x, surf->pitch_x, 1, surf);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			fin = (fin<<24) | col_no_a;
			overmask_rgbx_const_run(fin, dst + surf->pitch_x*spans[i].x, surf->pitch_x, spans[i].len, surf);
		}
	}
}


void evg_rgbx_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		s32 x;
		u32 len;
		u32 *col, _col;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		x = spans[i].x * surf->pitch_x;
		
		while (len--) {
			_col = *col;
			col_a = GF_COL_A(_col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_rgbx(*col, dst+x, spanalpha, surf);
				} else {
					dst[x+surf->idx_r] = GF_COL_R(_col);
					dst[x+surf->idx_g] = GF_COL_G(_col);
					dst[x+surf->idx_b] = GF_COL_B(_col);
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_rgbx(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u32 x, y, w, h, sx, sy;
	u8 r,g,b;
	s32 st;
	char *o_data;
	GF_EVGSurface *_this = (GF_EVGSurface *)surf;
	st = _this->pitch_x;

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);

	o_data = NULL;
	for (y = 0; y < h; y++) {
		u8 *data = (u8 *) _this ->pixels + (y + sy) * _this->pitch_y + st*sx;
		if (!y) {
			o_data = data;
			for (x = 0; x < w; x++) {
				data[surf->idx_r] = r;
				data[surf->idx_g] = g;
				data[surf->idx_b] = b;
				data[surf->idx_a] = 0xFF;
				data += st;
			}
		} else {
			memcpy(data, o_data, w*4);
		}
	}
	return GF_OK;
}




/*
		alpha grey
*/

static void overmask_alphagrey(u32 src, u8 *dst, u32 alpha, u32 grey_type, u32 idx_g, u32 idx_a)
{
	s32 srca = (src >> 24) & 0xff;
	s32 srcc;
	s32 dsta = dst[idx_a];

	if (grey_type==0) srcc = (src >> 16) & 0xff;
	else if (grey_type==1) srcc = (src >> 8) & 0xff;
	else srcc = (src >> 0) & 0xff;
	srca = mul255(srca, alpha);

	if (dsta) {
		s32 dstc = dst[idx_g];
		dst[idx_g] = mul255(srca, srcc - dstc) + dstc;
		dst[idx_a] = mul255(srca, srca) + mul255(255-srca, dsta);
	} else {
		dst[idx_g] = srcc;
		dst[idx_a] = srca;
	}
}

static void overmask_alphagrey_const_run(u32 src_a, u32 src_c, u8 *dst, s32 dst_pitch_x, u32 count, u32 idx_g, u32 idx_a)
{
	while (count) {
		s32 dsta = dst[idx_a];
		/*special case : if dst alpha is 0, consider the surface is empty and copy pixel*/
		if (dsta) {
			s32 dstc = dst[idx_g];
			dst[idx_g] = mul255(src_a, src_c - dstc) + dstc;
			dst[idx_a] = mul255(src_a, src_a) + mul255(255-src_a, dsta);
		} else {
			dst[idx_g] = src_c;
			dst[idx_a] = src_a;
		}
		dst += dst_pitch_x;
		count--;
	}
}


void evg_alphagrey_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	u8 col_c;

	if (surf->grey_type==0) col_c = GF_COL_R(col);
	else if (surf->grey_type==1) col_c = GF_COL_G(col);
	else col_c = GF_COL_B(col);
	dst[surf->idx_g] = col_c;
}

void evg_alphagrey_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf)
{
	u8 *dst = surf->pixels + y * surf->pitch_y + x * surf->pitch_x;
	overmask_alphagrey(col, dst, coverage, surf->grey_type, surf->idx_g, surf->idx_a);
}

void evg_alphagrey_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 col = surf->fill_col;
	u32 a;
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i, x;
	u32 len;
	u8 col_a, col_c;

	col_a = GF_COL_A(col);
	if (surf->grey_type==0) col_c = GF_COL_R(col);
	else if (surf->grey_type==1) col_c = GF_COL_G(col);
	else col_c = GF_COL_B(col);

	for (i=0; i<count; i++) {
		x = spans[i].x * surf->pitch_x;
		len = spans[i].len;

		if (spans[i].coverage != 0xFF) {
			a = mul255(0xFF, spans[i].coverage);
			overmask_alphagrey_const_run(a, col_c, dst + x, surf->pitch_x, len, surf->idx_g, surf->idx_a);
		} else {
			while (len--) {
				dst[x+surf->idx_g] = col_c;
				dst[x+surf->idx_a] = col_a;
				x += surf->pitch_x;
			}
		}
	}
}

void evg_alphagrey_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	u32 col = surf->fill_col;
	u32 a, fin, col_c;
	s32 i;

	a = GF_COL_A(col);
	if (surf->grey_type==0) col_c = GF_COL_R(col);
	else if (surf->grey_type==1) col_c = GF_COL_G(col);
	else col_c = GF_COL_B(col);

	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; i++) {
				s32 x = spans[i].x+j;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);
				overmask_alphagrey_const_run(fin, col_c, dst + surf->pitch_x * x, surf->pitch_x, 1, surf->idx_g, surf->idx_a);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			fin = mul255(a, spans[i].coverage);
			overmask_alphagrey_const_run(fin, col_c, dst + surf->pitch_x*spans[i].x, surf->pitch_x, spans[i].len, surf->idx_g, surf->idx_a);
		}
	}
}


void evg_alphagrey_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u8 *dst = (u8 *) surf->pixels + y * surf->pitch_y;
	s32 i;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		s32 x;
		u32 len;
		u32 *col, _col;
		len = spans[i].len;
		spanalpha = spans[i].coverage;
		col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		x = spans[i].x * surf->pitch_x;
		while (len--) {
			_col = *col;
			col_a = GF_COL_A(_col);
			if (col_a) {
				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_alphagrey(*col, (dst + x) , spanalpha, surf->grey_type, surf->idx_g, surf->idx_a);
				} else {
					u8 dstc;

					if (surf->grey_type==0) dstc = GF_COL_R(_col);
					else if (surf->grey_type==1) dstc = GF_COL_G(_col);
					else dstc = GF_COL_B(_col);

					dst[x+surf->idx_g] = dstc;
					dst[x+surf->idx_a] = col_a;
				}
			}
			col++;
			x += surf->pitch_x;
		}
	}
}

GF_Err evg_surface_clear_alphagrey(GF_EVGSurface *surf, GF_IRect rc, GF_Color col)
{
	u8 *data, *data_o;
	u8 col_a, col_c;
	u32 x, y, w, h, sx, sy;
	s32 st;
	GF_EVGSurface *_this = (GF_EVGSurface *)surf;
	st = _this->pitch_y;

	col_a = GF_COL_A(col);
	if (surf->grey_type==0) col_c = GF_COL_R(col);
	else if (surf->grey_type==1) col_c = GF_COL_G(col);
	else col_c = GF_COL_B(col);

	h = rc.height;
	w = rc.width;
	sx = rc.x;
	sy = rc.y;

	data_o = NULL;

	for (y = 0; y < h; y++) {
		data = (u8 *) _this ->pixels + (sy+y)* st + _this->pitch_x*sx;
		if (!y) {
			data_o = data;
			for (x = 0; x < w; x++) {
				data[surf->idx_a] = col_c;
				data[surf->idx_g] = col_a;
				data += _this->pitch_x;
			}
		} else {
			memcpy(data, data_o, w*_this->BPP);
		}
	}
	return GF_OK;
}
