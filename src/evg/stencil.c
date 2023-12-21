/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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
 */

#include "rast_soft.h"

#ifndef GPAC_DISABLE_EVG

static GF_EVGStencil *evg_solid_brush();
static GF_EVGStencil *evg_texture_brush();
static GF_EVGStencil *evg_linear_gradient_brush();
static GF_EVGStencil *evg_radial_gradient_brush();



GF_Color color_interpolate(u32 a, u32 b, u8 pos)
{
	u32 ca = ((a>>24)     )*(u32)(0xFF-pos)+((b>>24)     )*(u32)pos;
	u32 cr = ((a>>16)&0xFF)*(u32)(0xFF-pos)+((b>>16)&0xFF)*(u32)pos;
	u32 cg = ((a>> 8)&0xFF)*(u32)(0xFF-pos)+((b>> 8)&0xFF)*(u32)pos;
	u32 cb = ((a    )&0xFF)*(u32)(0xFF-pos)+((b    )&0xFF)*(u32)pos;
	return	(((ca+(ca>>8)+1)>>8)<<24)|
	        (((cr+(cr>>8)+1)>>8)<<16)|
	        (((cg+(cg>>8)+1)>>8)<< 8)|
	        (((cb+(cb>>8)+1)>>8)    );
}




/*
	Generic gradient tools
*/

#define EVGGRADIENTBUFFERSIZE	(1<<EVGGRADIENTBITS)
#define EVGGRADIENTMAXINTPOS		EVGGRADIENTBUFFERSIZE - 1

static void gradient_update(EVG_BaseGradient *_this)
{
	s32 i, c;
	s32 start, end, diff;
	Fixed maxPos = INT2FIX(EVGGRADIENTMAXINTPOS);

	_this->updated = 1;

	if (_this->pos[0]>=0) {
		if(_this->pos[0]>0) {
			end = FIX2INT(gf_mulfix(_this->pos[0], maxPos));
			for (i=0; i<= end; i++) {
				_this->precomputed_argb[i] = _this->col[0];
			}
		}
		for (c=0; c<EVGGRADIENTSLOTS; c++) {
			if (_this->pos[c]<0) break;
			if (_this->pos[c+1]>=0) {
				Fixed pos = MIN(_this->pos[c], FIX_ONE);
				start = FIX2INT(gf_mulfix(pos, maxPos));
				pos = MIN(_this->pos[c+1], FIX_ONE);
				end = FIX2INT(gf_mulfix(pos, maxPos));
				diff = end-start;

				if (diff) {
					for (i=start; i<=end; i++) {
						_this->precomputed_argb[i] = color_interpolate(_this->col[c], _this->col[c+1],
						                                  (u8) ( ( (i-start) * 255) / diff) );
					}
				}
			} else {
				Fixed pos = MIN(_this->pos[c+0], FIX_ONE);
				start = FIX2INT(gf_mulfix(pos, maxPos));
				for(i=start; i<=EVGGRADIENTMAXINTPOS; i++) {
					_this->precomputed_argb[i] = _this->col[c];
				}
			}
		}
	}
}

static u32 gradient_get_color(EVG_BaseGradient *_this, s32 pos)
{
	s32 max_pos = 1 << EVGGRADIENTBITS;

	switch (_this->mod) {
	case GF_GRADIENT_MODE_SPREAD:
		if (pos<0) pos = -pos;
		return _this->precomputed_dest[(pos & max_pos) ?  EVGGRADIENTMAXINTPOS - (pos % max_pos) :  pos % max_pos];

	case GF_GRADIENT_MODE_REPEAT:
		while (pos < 0) pos += max_pos;
		return _this->precomputed_dest[pos % max_pos];

	case GF_GRADIENT_MODE_PAD:
	default:
		return _this->precomputed_dest[ MIN(EVGGRADIENTMAXINTPOS, MAX((s32) 0, pos))];
	}
}

GF_EXPORT
GF_Err gf_evg_stencil_set_gradient_interpolation(GF_EVGStencil * p, Fixed *pos, GF_Color *col, u32 count)
{
	EVG_BaseGradient *_this = (EVG_BaseGradient *) p;
	if ( (_this->type != GF_STENCIL_LINEAR_GRADIENT) && (_this->type != GF_STENCIL_RADIAL_GRADIENT) ) return GF_BAD_PARAM;
	if (count>=EVGGRADIENTSLOTS-1) return GF_OUT_OF_MEM;
	if (count) {
		memcpy(_this->col, col, sizeof(GF_Color) * count);
		memcpy(_this->pos, pos, sizeof(Fixed) * count);
	}
	_this->col[count] = 0;
	_this->pos[count] = -FIX_ONE;
	gradient_update(_this);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_evg_stencil_push_gradient_interpolation(GF_EVGStencil * p, Fixed pos, GF_Color col)
{
	u32 count=0;
	EVG_BaseGradient *_this = (EVG_BaseGradient *) p;
	if ( (_this->type != GF_STENCIL_LINEAR_GRADIENT) && (_this->type != GF_STENCIL_RADIAL_GRADIENT) ) return GF_BAD_PARAM;
	while (count<EVGGRADIENTSLOTS-1) {
		if (_this->pos[count]==-FIX_ONE) break;
		count++;
	}
	if (count>=EVGGRADIENTSLOTS-1) return GF_OUT_OF_MEM;
	_this->col[count] = col;
	_this->pos[count] = pos;
	count++;
	_this->col[count] = 0;
	_this->pos[count] = -FIX_ONE;
	gradient_update(_this);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_gradient_mode(GF_EVGStencil * p, GF_GradientMode mode)
{
	EVG_BaseGradient *_this = (EVG_BaseGradient *) p;
	if ( (_this->type != GF_STENCIL_LINEAR_GRADIENT) && (_this->type != GF_STENCIL_RADIAL_GRADIENT) ) return GF_BAD_PARAM;
	_this->mod = mode;
	return GF_OK;
}

/*
	Generic stencil
*/

GF_EXPORT
GF_EVGStencil * gf_evg_stencil_new(GF_StencilType type)
{
	GF_EVGStencil *st;
	switch (type) {
	case GF_STENCIL_SOLID:
		st = evg_solid_brush();
		break;
	case GF_STENCIL_LINEAR_GRADIENT:
		st = evg_linear_gradient_brush();
		break;
	case GF_STENCIL_RADIAL_GRADIENT:
		st = evg_radial_gradient_brush();
		break;
	case GF_STENCIL_TEXTURE:
		st = evg_texture_brush();
		break;
	default:
		return NULL;
	}
	if (st) {
		gf_mx2d_init(st->pmat);
		gf_mx2d_init(st->smat);
		gf_cmx_init(&st->cmat);
		st->auto_mx = 1;
	}
	return st;
}

GF_EXPORT
void gf_evg_stencil_delete(GF_EVGStencil * st)
{
	switch(st->type) {
	case GF_STENCIL_SOLID:
	case GF_STENCIL_LINEAR_GRADIENT:
	case GF_STENCIL_RADIAL_GRADIENT:
		gf_free(st);
		return;
	case GF_STENCIL_TEXTURE:
	{
		EVG_Texture *tx = (EVG_Texture *)st;
		/*destroy local texture iof any*/
		if (tx->owns_texture && tx->pixels) gf_free(tx->pixels);
		gf_free(st);
	}
	return;
	}
}

GF_EXPORT
GF_Err gf_evg_stencil_set_auto_matrix(GF_EVGStencil * st, Bool auto_on)
{
       if (!st) return GF_BAD_PARAM;
       st->auto_mx = auto_on ? 1 : 0;
       return GF_OK;
}

GF_EXPORT
Bool gf_evg_stencil_get_auto_matrix(GF_EVGStencil * st)
{
       return st ? st->auto_mx : GF_FALSE;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_matrix(GF_EVGStencil * st, GF_Matrix2D *mx)
{
	if (!st || (st->type>GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	if (mx) {
		gf_mx2d_copy(st->smat, *mx);
	} else {
		gf_mx2d_init(st->smat);
	}
	return GF_OK;
}

GF_EXPORT
Bool gf_evg_stencil_get_matrix(GF_EVGStencil * st, GF_Matrix2D *mx)
{
	if (!st || !mx || (st->type>GF_STENCIL_TEXTURE)) return GF_FALSE;
	gf_mx2d_copy(*mx, st->smat);
	return GF_TRUE;
}


GF_EXPORT
GF_StencilType gf_evg_stencil_type(GF_EVGStencil *sten)
{
	return sten ? sten->type : 0;
}

/*
	Solid color stencil
*/
static void sc_fill_run(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 x, s32 y, u32 count)
{
	EVG_Brush *sc = (EVG_Brush *)p;
	u32 *data = rctx->stencil_pix_run;
	u64 *data_wide = rctx->surf->not_8bits ? rctx->stencil_pix_run : NULL;
	while (count) {
		if (data) *data++ = sc->fill_col;
		else *data_wide++ = sc->fill_col_wide;

		count--;
	}

}
static GF_EVGStencil *evg_solid_brush()
{
	EVG_Brush *tmp;
	GF_SAFEALLOC(tmp, EVG_Brush);
	if (!tmp) return 0L;
	tmp->fill_run = sc_fill_run;
	tmp->color = 0xFF000000;
	tmp->alpha = 0xFF;
	tmp->type = GF_STENCIL_SOLID;
	return (GF_EVGStencil *) tmp;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_brush_color(GF_EVGStencil * st, GF_Color c)
{
	EVG_Brush *_this = (EVG_Brush *) st;
	if (!_this  || (_this ->type != GF_STENCIL_SOLID) ) return GF_BAD_PARAM;
	_this->color = c;
	return GF_OK;
}

GF_EXPORT
GF_Color gf_evg_stencil_get_brush_color(GF_EVGStencil * st)
{
	EVG_Brush *_this = (EVG_Brush *) st;
	if (!_this  || (_this ->type != GF_STENCIL_SOLID) ) return 0;
	return _this->color;
}

#define edgeFunction_pre2(a, b_minus_a_x, b_minus_a_y) \
	( (_x - a.x) * (b_minus_a_y) - (_y - a.y) * (b_minus_a_x) ) / surf->tri_area

#define PERSP_VARS_DECL \
	GF_EVGSurface *surf = rctx->surf; \
	Float bc1 = edgeFunction_pre2(surf->s_v2, surf->s3_m_s2_x, surf->s3_m_s2_y); \
	Float bc3 = edgeFunction_pre2(surf->s_v1, surf->s2_m_s1_x, surf->s2_m_s1_y); \
	Float bc1_inc = surf->s3_m_s2_y / surf->tri_area; \
	Float bc3_inc = surf->s2_m_s1_y / surf->tri_area; \
	Float pbc1 = bc1 * surf->s_v1.q; \
	Float pbc3 = bc3 * surf->s_v3.q; \
	Float pbc2 = (1.0f - bc1 - bc3) * surf->s_v2.q; \
	Float pbc1_inc = bc1_inc * surf->s_v1.q; \
	Float pbc3_inc = bc3_inc * surf->s_v3.q; \
	Float pbc2_inc = - (bc1_inc + bc3_inc) * surf->s_v2.q; \
	Float persp_denum = pbc1 + pbc2 + pbc3; \
	Float pers_denum_inc = pbc1_inc + pbc2_inc + pbc3_inc;



//we map texture coords, pt1 is {0,0} pt2 is {1,0} pt3 is {0,1}
//y flip is done by switching the points in evg_raster_render_path_3d
#define PERSP_APPLY \
	Fixed ix = FLT2FIX(pbc2 / persp_denum);\
	Fixed iy = FLT2FIX(pbc3 / persp_denum);\
	pbc2 += pbc2_inc;\
	pbc3 += pbc3_inc;\
	persp_denum += pers_denum_inc;

/*
	linear gradient stencil
*/

static void lg_fill_run(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	Fixed _res;
	s32 val;
	u32 col;
	u32 *data = rctx->stencil_pix_run;
	u64 *data_wide = rctx->surf->not_8bits ? rctx->stencil_pix_run : NULL;
	EVG_LinearGradient *_this = (EVG_LinearGradient *) p;

	gf_assert(data);

	if (rctx->surf->is_3d_matrix) {
		PERSP_VARS_DECL

		while (count) {

			PERSP_APPLY

			gf_mx2d_apply_coords(&_this->smat, &ix, &iy);
			val = FIX2INT(ix);

			col = gradient_get_color((EVG_BaseGradient *)_this, val );
			if (data_wide) *data_wide++ = evg_col_to_wide(col);
			else *data++ = col;

			count--;
		}
	} else {
		/*no need to move x & y to fixed*/
		_res = (Fixed) (_x * _this->smat.m[0] + _y * _this->smat.m[1] + _this->smat.m[2]);
		while (count) {
			val = FIX2INT(_res);
			_res += _this->smat.m[0];

			col = gradient_get_color((EVG_BaseGradient *)_this, val );
			if (data_wide) *data_wide++ = evg_col_to_wide(col);
			else *data++ = col;

			count--;
		}
	}
}

GF_EXPORT
GF_Err gf_evg_stencil_set_linear_gradient(GF_EVGStencil * st, Fixed start_x, Fixed start_y, Fixed end_x, Fixed end_y)
{
	GF_Matrix2D mtx;
	GF_Point2D s;
	Fixed f;
	EVG_LinearGradient *_this = (EVG_LinearGradient *) st;
	if (_this->type != GF_STENCIL_LINEAR_GRADIENT) return GF_BAD_PARAM;

	_this->start.x = start_x;
	_this->start.y = start_y;
	_this->end.x = end_x;
	_this->end.y = end_y;
	s.x = end_x - start_x;
	s.y = end_y - start_y;
	f = gf_v2d_len(&s);
	if (f) f = gf_invfix(f);

	gf_mx2d_init(mtx);
	mtx.m[2] = - _this->start.x;
	mtx.m[5] = - _this->start.y;
	_this->vecmat = mtx;

	gf_mx2d_init(mtx);
	gf_mx2d_add_rotation(&mtx, 0, 0, - gf_atan2(s.y, s.x));
	gf_mx2d_add_matrix(&_this->vecmat, &mtx);

	gf_mx2d_init(mtx);
	gf_mx2d_add_scale(&mtx, f, f);
	gf_mx2d_add_matrix(&_this->vecmat, &mtx);
	return GF_OK;
}

static GF_EVGStencil *evg_linear_gradient_brush()
{
	s32 i;
	EVG_LinearGradient *tmp;
	GF_SAFEALLOC(tmp, EVG_LinearGradient);
	if (!tmp) return 0L;
	gf_mx2d_init(tmp->vecmat);
	tmp->fill_run = lg_fill_run;
	tmp->type = GF_STENCIL_LINEAR_GRADIENT;
	for(i=0; i<EVGGRADIENTSLOTS; i++) tmp->pos[i]=-1;

	tmp->alpha = 0xFF;
	gf_evg_stencil_set_linear_gradient((GF_EVGStencil *)tmp, 0, 0, FIX_ONE, 0);
	return (GF_EVGStencil *) tmp;
}


/*
	radial gradient stencil
*/

static void rg_fill_run(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	Fixed x, y, dx, dy, b, val;
	s32 pos;
	u32 col;
	u32 *data = rctx->stencil_pix_run;
	u64 *data_wide = rctx->surf->not_8bits ? rctx->stencil_pix_run : NULL;
	EVG_RadialGradient *_this = (EVG_RadialGradient *) p;

	gf_assert(data);

	if (rctx->surf->is_3d_matrix) {
		PERSP_VARS_DECL

		while (count) {

			PERSP_APPLY

			gf_mx2d_apply_coords(&_this->smat, &ix, &iy);

			dx = ix - _this->d_f.x;
			dy = iy - _this->d_f.y;
			b = gf_mulfix(_this->rad, gf_mulfix(dx, _this->d_f.x) + gf_mulfix(dy,  _this->d_f.y));
			val = gf_mulfix(b, b) + gf_mulfix(_this->rad, gf_mulfix(dx, dx)+gf_mulfix(dy, dy));
			b += gf_sqrt(val);
			pos = FIX2INT(EVGGRADIENTBUFFERSIZE*b);

			col = gradient_get_color((EVG_BaseGradient *)_this, pos );
			if (data_wide) *data_wide++ = evg_col_to_wide(col);
			else *data++ = col;

			count--;
		}
	} else {
		x = INT2FIX(_x);
		y = INT2FIX(_y);
		gf_mx2d_apply_coords(&_this->smat, &x, &y);

		dx = x - _this->d_f.x;
		dy = y - _this->d_f.y;
		while (count) {
			b = gf_mulfix(_this->rad, gf_mulfix(dx, _this->d_f.x) + gf_mulfix(dy,  _this->d_f.y));
			val = gf_mulfix(b, b) + gf_mulfix(_this->rad, gf_mulfix(dx, dx)+gf_mulfix(dy, dy));
			b += gf_sqrt(val);
			pos = FIX2INT(EVGGRADIENTBUFFERSIZE*b);

			col = gradient_get_color((EVG_BaseGradient *)_this, pos);
			if (data_wide) *data_wide++ = evg_col_to_wide(col);
			else *data++ = col;

			dx += _this->d_i.x;
			dy += _this->d_i.y;
			count--;
		}
	}
}

void evg_radial_init(EVG_RadialGradient *_this)
{
	GF_Point2D p0, p1;
	p0.x = p0.y = p1.y = 0;
	p1.x = FIX_ONE;

	gf_mx2d_apply_point(&_this->smat, &p0);
	gf_mx2d_apply_point(&_this->smat, &p1);
	_this->d_i.x = p1.x - p0.x;
	_this->d_i.y = p1.y - p0.y;

	_this->rad = FIX_ONE - gf_mulfix(_this->d_f.x, _this->d_f.x) - gf_mulfix(_this->d_f.y, _this->d_f.y);
	if (_this->rad) {
		_this->rad = gf_invfix(_this->rad);
	} else {
		_this->rad = EVGGRADIENTBUFFERSIZE;
	}
}

static GF_EVGStencil *evg_radial_gradient_brush()
{
	s32 i;
	EVG_RadialGradient *tmp;
	GF_SAFEALLOC(tmp, EVG_RadialGradient);
	if (!tmp) return 0L;

	tmp->fill_run = rg_fill_run;
	tmp->type = GF_STENCIL_RADIAL_GRADIENT;
	for(i=0; i<EVGGRADIENTSLOTS; i++) tmp->pos[i]=-1;

	tmp->center.x = tmp->center.y = FIX_ONE/2;
	tmp->focus = tmp->center;
	tmp->radius = tmp->center;
	tmp->alpha = 0xFF;
	return (GF_EVGStencil *) tmp;
}


GF_EXPORT
GF_Err gf_evg_stencil_set_radial_gradient(GF_EVGStencil * st, Fixed cx, Fixed cy, Fixed fx, Fixed fy, Fixed x_radius, Fixed y_radius)
{
	EVG_RadialGradient *_this = (EVG_RadialGradient *) st;
	if (_this->type != GF_STENCIL_RADIAL_GRADIENT) return GF_BAD_PARAM;

	_this->center.x = cx;
	_this->center.y = cy;
	_this->focus.x = fx;
	_this->focus.y = fy;
	_this->radius.x = x_radius;
	_this->radius.y = y_radius;
	return GF_OK;
}


void evg_gradient_precompute(EVG_BaseGradient *grad, GF_EVGSurface *surf)
{
	Bool do_cmat, do_yuv, has_a, has_changed;
	u32 i, nb_col;

	has_changed = grad->updated;

	if (surf->yuv_type) {
		if (grad->yuv_prof != surf->yuv_prof) {
			grad->yuv_prof = surf->yuv_prof;
			has_changed = GF_TRUE;
		}
	} else {
		if (grad->yuv_prof) {
			grad->yuv_prof = 0;
			has_changed = GF_TRUE;
		}
	}
	if (!has_changed) {
		return;
	}
	grad->updated = 0;

	do_yuv = (surf->yuv_type==EVG_YUV_NONE) ? GF_FALSE : GF_TRUE;

	do_cmat = (grad->cmat.identity) ? GF_FALSE : GF_TRUE;
	has_a = (grad->alpha==0xFF) ? GF_FALSE : GF_TRUE;
	nb_col = (1<<EVGGRADIENTBITS);

	for (i=0; i<nb_col; i++) {
		u32 argb = grad->precomputed_argb[i];
		if (has_a) {
			u32 ca = ((u32) (GF_COL_A(argb) + 1) * grad->alpha) >> 8;
			argb = ( ((ca<<24) & 0xFF000000) ) | (argb & 0x00FFFFFF);
		}
		if (do_cmat)
			argb = gf_cmx_apply(&grad->cmat, argb);

		if (do_yuv)
			argb = gf_evg_argb_to_ayuv(surf, argb);

		grad->precomputed_dest[i] = argb;
	}
}


/*
	Texture stencil
	 	FIXME: add filtering , check bilinear
*/

static GFINLINE s32 mul255(s32 a, s32 b)
{
	return ((a+1) * b) >> 8;
}
static u32 EVG_LERP(u32 c0, u32 c1, u8 t)
{
	s32 a0, r0, g0, b0;
	s32 a1, r1, g1, b1;
	s32 a2, r2, g2, b2;

	if (!t) return c0;

	a0 = GF_COL_A(c0);
	r0 = GF_COL_R(c0);
	g0 = GF_COL_G(c0);
	b0 = GF_COL_B(c0);
	a1 = GF_COL_A(c1);
	r1 = GF_COL_R(c1);
	g1 = GF_COL_G(c1);
	b1 = GF_COL_B(c1);

	a2 = a0 + mul255(t, (a1 - a0));
	r2 = r0 + mul255(t, (r1 - r0));
	g2 = g0 + mul255(t, (g1 - g0));
	b2 = b0 + mul255(t, (b1 - b0));
	return GF_COL_ARGB(a2, r2, g2, b2);
}

static GFINLINE s64 mul_wide(s64 a, s64 b)
{
	return ((a+1) * b) >> 16;
}
static u64 EVG_LERP_WIDE(u64 c0, u64 c1, u8 t)
{
	s64 a0, r0, g0, b0;
	s64 a1, r1, g1, b1;
	s64 a2, r2, g2, b2;

	if (!t) return c0;

	a0 = (c0>>48) & 0xFFFF;
	r0 = (c0>>32) & 0xFFFF;
	g0 = (c0>>16) & 0xFFFF;
	b0 = (c0) & 0xFFFF;

	a1 = (c1>>48) & 0xFFFF;
	r1 = (c1>>32) & 0xFFFF;
	g1 = (c1>>16) & 0xFFFF;
	b1 = (c1) & 0xFFFF;

	a2 = a0 + mul_wide(t, (a1 - a0));
	r2 = r0 + mul_wide(t, (r1 - r0));
	g2 = g0 + mul_wide(t, (g1 - g0));
	b2 = b0 + mul_wide(t, (b1 - b0));

	return GF_COLW_ARGB(a2, r2, g2, b2);
}

//slightly more precise but way too expensive as it uses mx2d apply
//#define EXACT_TX_COORDS

static void tex_untransform_coord(EVG_Texture *_this, s32 _x, s32 _y, Fixed *outx, Fixed *outy)
{
	Fixed x, y;

	/* reverse to texture coords*/
	x = INT2FIX(_x);
	y = INT2FIX(_y);
	gf_mx2d_apply_coords(&_this->smat, &x, &y);

#ifdef EXACT_TX_COORDS
	u32 checkx = 0;
	u32 checky = 0;
	if (ABS(x)< FIX_ONE/20) checkx = 1;
	if (ABS(y)< FIX_ONE/20) checky = 1;

	/*we may have a numerical stability issues, try to figure out whether we are close from 0 or width/height*/
	if ( checkx || checky) {
		Fixed tx, ty;
		tx = INT2FIX(_x+1);
		ty = INT2FIX(_y+1);
		gf_mx2d_apply_coords(&_this->smat, &tx, &ty);

		if (checkx) {
			if (tx<0) x = INT2FIX(_this->width - 1);
			else x = 0;
		}
		if (checky) {
			if (ty<0) y = INT2FIX(_this->height - 1);
			else y = 0;
		}
	}
#endif

	*outx=x;
	*outy=y;
}

#ifdef EXACT_TX_COORDS

#define TEX_UNTRANSFORM_COORDS \
	_x++; \
	tex_untransform_coord(_this, _x, _y, &x, &y);

#else

#define TEX_UNTRANSFORM_COORDS \
		x += _this->inc_x; \
		y += _this->inc_y; \

#endif

static u32 evg_paramtx_get_pixel(struct __evg_texture *_this, u32 x, u32 y, EVGRasterCtx *rctx)
{
	Float a, r, g, b;
	_this->tx_callback(_this->tx_callback_udta, x, y, &r, &g, &b, &a);
	r*=255;
	g*=255;
	b*=255;
	a*=255;
	return GF_COL_ARGB(a, r, g, b);
}
u64 evg_paramtx_get_pixel_wide(struct __evg_texture *_this, u32 x, u32 y, EVGRasterCtx *rctx)
{
	Float a, r, g, b;
	_this->tx_callback(_this->tx_callback_udta, x, y, &r, &g, &b, &a);
	r*=0xFFFF;
	g*=0xFFFF;
	b*=0xFFFF;
	a*=0xFFFF;
	return GF_COLW_ARGB(a, r, g, b);
}

static void tex_fill_run_callback(GF_EVGStencil *_p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	EVG_Texture *p = (EVG_Texture *)_p;
	u32 *data = rctx->stencil_pix_run;
	while (count) {
		*data = evg_paramtx_get_pixel(p, _x, _y, NULL);
		data++;
		count--;
		_x++;
	}
}

static void tex_fill_run_callback_wide(GF_EVGStencil *_p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	EVG_Texture *p = (EVG_Texture *)_p;
	u64 *data = rctx->stencil_pix_run;
	while (count) {
		*data = evg_paramtx_get_pixel_wide(p, _x, _y, NULL);
		data++;
		count--;
	}
}

//bilinear used fo 2D graphics ?
#define USE_BILINEAR	1

static void tex_fill_run(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 cx, x0, y0, m_width, m_height;
	u32 pix;
	Bool has_alpha, has_cmat, repeat_s, repeat_t;
	Fixed x, y;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u32 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	tex_untransform_coord(_this, _x, _y, &x, &y);

#if USE_BILINEAR
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	m_width = _this->width-1;
	m_height = _this->height-1;

	while (count) {
		x0 = FIX2INT(x);

		if (x0 > m_width) {
			if (repeat_s) {
				x0 = (x0) % _this->width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += _this->width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		y0 = FIX2INT(y);
		if (y0 > m_height) {
			if (repeat_t) {
				y0 = (y0) % _this->height;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				y0 = m_height;
			}
		}
		else if (y0 < 0) {
			if (repeat_t) {
				while (y0<0) y0 += _this->height;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				y0 = 0;
			}
		}

		pix = _this->tx_get_pixel(_this, x0, y0, rctx);

		TEX_UNTRANSFORM_COORDS

		/*bilinear filtering*/
#if USE_BILINEAR
		if (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) {
			Bool do_lerp=GF_TRUE;
			u32 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;

			tx = FIX2INT(gf_muldiv(x, 255, _this->width) );
			ty = FIX2INT(gf_muldiv(y, 255, _this->height) );

			x1 = (x0+incx);
			if (x1<0) {
				if (!repeat_s)
					do_lerp=GF_FALSE;
				else
					while (x1<0) x1 += _this->width;
			} else {
				if (!repeat_s && ((u32) x1 > _this->width))
					do_lerp=GF_FALSE;
				else
					x1 = x1 % _this->width;
			}
			y1 = (y0+incy);
			if (y1<0) {
				if (!repeat_t)
					do_lerp=GF_FALSE;
				else
					while (y1<0) y1+=_this->height;
			} else {
				if (!repeat_t && ((u32) y1 > _this->height))
					do_lerp=GF_FALSE;
				else
					y1 = y1 % _this->height;
			}

			if (!do_lerp) goto write_pix;

			if (incx>0) {
				if (x1<x0) tx = 255-tx;
			} else {
				if (x1>x0) tx = 255-tx;
			}
			if (incy>0) {
				if (y1<y0) ty = 255-ty;
			} else {
				if (y1>y0) ty = 255-ty;
			}

			p00 = pix;
			p01 = _this->tx_get_pixel(_this, x1, y0, rctx);
			p10 = _this->tx_get_pixel(_this, x0, y1, rctx);
			p11 = _this->tx_get_pixel(_this, x1, y1, rctx);

			p00 = EVG_LERP(p00, p01, tx);
			p10 = EVG_LERP(p10, p11, tx);
			pix = EVG_LERP(p00, p10, ty);

		}
#endif

write_pix:
		if (has_alpha) {
			cx = ((GF_COL_A(pix) + 1) * _this->alpha) >> 8;
			pix = ( (((u32)cx<<24) & 0xFF000000) ) | (pix & 0x00FFFFFF);
		}
		//move pixel to target pixel format, applying color transform matrix
		if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!yuv_type) {
				pix = gf_evg_ayuv_to_argb(rctx->surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  gf_evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), gf_evg_argb_to_ayuv
				pix = gf_cmx_apply(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply(&_this->cmat, pix);

			//dest is yuv, transform
			if (yuv_type)
				pix = gf_evg_argb_to_ayuv(rctx->surf, pix);
		}

		*data++ = pix;
		count--;
	}
}


/*just a little faster...*/
static void tex_fill_run_straight(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0, m_width;
	u32 pix;
	u32 conv_type=0;
	Bool repeat_s = GF_FALSE;
	u32 pad_y = 0;
	Fixed x, y;
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u32 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	/*get texture coords in FIXED - offset*/
	x = _this->smat.m[0]*_x + _this->smat.m[2];
	y = _this->smat.m[4]*_y + _this->smat.m[5];

	/*we may have a numerical stability issues, try to figure out whether we are close from 0 or width/height*/
	if (ABS(x)< FIX_ONE/10) {
		Fixed test = _this->smat.m[0]*(_x+1) + _this->smat.m[2];
		if (test<0)
			x = INT2FIX(_this->width - 1);
		else x = 0;
	}
	if (ABS(y)< FIX_ONE/10) {
		Fixed test = _this->smat.m[4]*(_y+1) + _this->smat.m[5];
		if (test<0)
			y = INT2FIX(_this->height - 1);
		else y = 0;
	}

	repeat_s = (_this->mod & GF_TEXTURE_REPEAT_S);

	y0 = FIX2INT(y);
	if (y0<0) {
		if (_this->mod & GF_TEXTURE_REPEAT_T) {
			while (y0<0) y0 += (s32) _this->height;
		} else if (_this->fill_pad_color)
			pad_y = _this->fill_pad_color;
		else
			y0 = 0;
	} else if (y0 >= (s32) _this->height) {
		if (_this->mod & GF_TEXTURE_REPEAT_T)
			while (y0 >= (s32) _this->height) y0 -= (s32) _this->height;
		else if (_this->fill_pad_color)
			pad_y = _this->fill_pad_color;
		else
			y0 = _this->height-1;
	}
	if (pad_y) {
		while (count) {
			*data++ = pad_y;
			count--;
		}
		return;
	}

	m_width = _this->width - 1;

	if (_this->is_yuv && !yuv_type) conv_type = 1;
	else if (!_this->is_yuv && yuv_type) conv_type = 2;


	while (count) {
		x0 = FIX2INT(x);
		if (x0 > m_width) {
			if (repeat_s) {
				while ((u32) x0 > _this->width) x0 -= _this->width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += _this->width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		pix = _this->tx_get_pixel(_this, x0, y0, rctx);

write_pix:
		x += _this->inc_x;
		//move pixel to target pixel format
		switch (conv_type) {
		case 1:
			pix = gf_evg_ayuv_to_argb(rctx->surf, pix);
			break;
		case 2:
			pix = gf_evg_argb_to_ayuv(rctx->surf, pix);
			break;
		}

		*data++ = pix;
		count--;
	}
}

u64 evg_col_to_wide( u32 col)
{
	u32 a = GF_COL_A(col) << 8 | 0xFF;
	u32 r = GF_COL_R(col) << 8 | 0xFF;
	u32 g = GF_COL_G(col) << 8 | 0xFF;
	u32 b = GF_COL_B(col) << 8 | 0xFF;
	return GF_COLW_ARGB(a, r, g, b);
}

static void tex_fill_run_wide(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0, m_width, m_height;
	u64 pix;
	Bool has_alpha, has_cmat, repeat_s, repeat_t;
	Fixed x, y;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u64 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	tex_untransform_coord(_this, _x, _y, &x, &y);

#if USE_BILINEAR
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	m_width = _this->width - 1;
	m_height = _this->height - 1;

	while (count) {
		x0 = FIX2INT(x);
		if (x0 > m_width) {
			if (repeat_s) {
				x0 = (x0) % _this->width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += _this->width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		y0 = FIX2INT(y);
		if (y0 > m_height) {
			if (repeat_t) {
				y0 = (y0) % _this->height;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				y0 = m_height;
			}
		}
		else if (y0 < 0) {
			if (repeat_t) {
				while (y0<0) y0 += _this->height;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				TEX_UNTRANSFORM_COORDS
				goto write_pix;
			} else {
				y0 = 0;
			}
		}

		pix = _this->tx_get_pixel_wide(_this, x0, y0, rctx);

		TEX_UNTRANSFORM_COORDS

		/*bilinear filtering*/
#if USE_BILINEAR
		if (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) {
			u64 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;
			Bool do_lerp=GF_TRUE;

			tx = FIX2INT(gf_muldiv(x, 255, _this->width) );
			ty = FIX2INT(gf_muldiv(y, 255, _this->height) );

			x1 = (x0+incx);
			if (x1<0) {
				if (!repeat_s)
					do_lerp=GF_FALSE;
				else
					while (x1<0) x1 += _this->width;
			} else {
				if (!repeat_s && ((u32) x1 > _this->width))
					do_lerp=GF_FALSE;
				else
					x1 = x1 % _this->width;
			}
			y1 = (y0+incy);
			if (y1<0) {
				if (!repeat_t)
					do_lerp=GF_FALSE;
				else
					while (y1<0) y1+=_this->height;
			} else {
				if (!repeat_t && ((u32) y1 > _this->height))
					do_lerp=GF_FALSE;
				else
					y1 = y1 % _this->height;
			}

			if (!do_lerp) goto write_pix;


			if (incx>0) {
				if (x1<x0) tx = 255-tx;
			} else {
				if (x1>x0) tx = 255-tx;
			}
			if (incy>0) {
				if (y1<y0) ty = 255-ty;
			} else {
				if (y1>y0) ty = 255-ty;
			}

			p00 = pix;

			p01 = _this->tx_get_pixel_wide(_this, x1, y0, rctx);
			p10 = _this->tx_get_pixel_wide(_this, x0, y1, rctx);
			p11 = _this->tx_get_pixel_wide(_this, x1, y1, rctx);

			p00 = EVG_LERP_WIDE(p00, p01, tx);
			p10 = EVG_LERP_WIDE(p10, p11, tx);
			pix = EVG_LERP_WIDE(p00, p10, ty);
		}
#endif

write_pix:

		if (has_alpha) {
			u64 _a = (pix>>48)&0xFF;
			_a = (_a * _this->alpha) >> 8;
			_a<<=48;
			pix = ( (_a & 0xFFFF000000000000UL) ) | (pix & 0x0000FFFFFFFFFFFFUL);
		}

		//move pixel to target pixel format, applying color transform matrix
		if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!yuv_type) {
				pix = gf_evg_ayuv_to_argb_wide(rctx->surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply_wide(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  gf_evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), gf_evg_argb_to_ayuv
				pix = gf_cmx_apply_wide(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply_wide(&_this->cmat, pix);

			//dest is yuv, transform
			if (yuv_type)
				pix = gf_evg_argb_to_ayuv_wide(rctx->surf, pix);
		}

		*data++ = pix;
		count--;
	}
}


/*just a little faster...*/
static void tex_fill_run_straight_wide(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0, m_width;
	u64 pix, pad_y=0;
	u32 conv_type=0;
	Bool repeat_s = GF_FALSE;
	Fixed x, y;
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u64 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	/*get texture coords in FIXED - offset*/
	x = _this->smat.m[0]*_x + _this->smat.m[2];
	y = _this->smat.m[4]*_y + _this->smat.m[5];

	/*we may have a numerical stability issues, try to figure out whether we are close from 0 or width/height*/
	if (ABS(x)< FIX_ONE/10) {
		Fixed test = _this->smat.m[0]*(_x+1) + _this->smat.m[2];
		if (test<0) x = INT2FIX(_this->width - 1);
		else x = 0;
	}
	if (ABS(y)< FIX_ONE/10) {
		Fixed test = _this->smat.m[4]*(_y+1) + _this->smat.m[5];
		if (test<0) y = INT2FIX(_this->height - 1);
		else y = 0;
	}

	/* and move in absolute coords*/
	repeat_s = (_this->mod & GF_TEXTURE_REPEAT_S);

	y0 = FIX2INT(y);

	if (y0<0) {
		if (_this->mod & GF_TEXTURE_REPEAT_T)
			while (y0<0) y0 += _this->height;
		else if (_this->fill_pad_color_wide)
			pad_y = _this->fill_pad_color_wide;
		else
			y0 = 0;
	} else if (y0>=(s32) _this->height) {
		if (_this->mod & GF_TEXTURE_REPEAT_T)
			while ((u32) y0 > _this->height) y0 -= _this->height;
		else if (_this->fill_pad_color_wide)
			pad_y = _this->fill_pad_color_wide;
		else
			y0 = _this->height-1;
	}
	if (pad_y) {
		while (count) {
			*data++ = pad_y;
			count--;
		}
		return;
	}

	m_width = _this->width - 1;

	if (_this->is_yuv && !yuv_type) conv_type = 1;
	else if (!_this->is_yuv && yuv_type) conv_type = 2;

	while (count) {
		x0 = FIX2INT(x);
		if (x0 > m_width) {
			if (repeat_s) {
				x0 = (x0) % _this->width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += _this->width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		pix = _this->tx_get_pixel_wide(_this, x0, y0, rctx);

write_pix:
		x += _this->inc_x;
		//move pixel to target pixel format
		switch (conv_type) {
		case 1:
			pix = gf_evg_ayuv_to_argb_wide(rctx->surf, pix);
			break;
		case 2:
			pix = gf_evg_argb_to_ayuv_wide(rctx->surf, pix);
			break;
		}

		*data++ = pix;
		count--;
	}
}

static void tex_fill_run_3d(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 cx, x0, y0, m_width, m_height;
	u32 pix;
	Bool has_alpha, has_cmat, repeat_s, repeat_t;
	Fixed x, y;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u32 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;


#if USE_BILINEAR
	Bool use_bili = (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) ? 1 : 0;
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	Bool do_mx = gf_mx2d_is_identity(_this->smat_bck) ? GF_FALSE : GF_TRUE;

	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	m_width = _this->width-1;
	m_height = _this->height-1;

	PERSP_VARS_DECL

	while (count) {
		PERSP_APPLY

		if (do_mx)
			gf_mx2d_apply_coords(&_this->smat_bck, &ix, &iy);

		x = m_width * ix;
		y = m_height * iy;

		x0 = FIX2INT(x);

		if (x0 > m_width) {
			if (repeat_s) {
				while (x0>m_width) x0 -= m_width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += m_width;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		y0 = FIX2INT(y);
		if (y0 > m_height) {
			if (repeat_t) {
				while (y0>m_height) y0 -= m_height;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				y0 = m_height;
			}
		}
		else if (y0 < 0) {
			if (repeat_t) {
				while (y0<0) y0 += m_height;
			} else if (_this->fill_pad_color) {
				pix = _this->fill_pad_color;
				goto write_pix;
			} else {
				y0 = 0;
			}
		}

		pix = _this->tx_get_pixel(_this, x0, y0, rctx);

		/*bilinear filtering*/
#if USE_BILINEAR
		if (use_bili) {
			Bool do_lerp=GF_TRUE;
			u32 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;

			tx = FIX2INT(gf_muldiv(x, 255, m_width) );
			ty = FIX2INT(gf_muldiv(y, 255, m_height) );

			x1 = (x0+incx);
			if (x1<0) {
				if (!repeat_s)
					do_lerp=GF_FALSE;
				else
					while (x1<0) x1 += m_width;
			} else {
				if (!repeat_s && (x1 > m_width))
					do_lerp=GF_FALSE;
				else
					x1 = x1 % m_width;
			}
			y1 = (y0+incy);
			if (y1<0) {
				if (!repeat_t)
					do_lerp=GF_FALSE;
				else
					while (y1<0) y1 += m_height;
			} else {
				if (!repeat_t && (y1 > m_height))
					do_lerp=GF_FALSE;
				else
					y1 = y1 % m_height;
			}

			if (!do_lerp) goto write_pix;

			if (incx>0) {
				if (x1<x0) tx = 255-tx;
			} else {
				if (x1>x0) tx = 255-tx;
			}
			if (incy>0) {
				if (y1<y0) ty = 255-ty;
			} else {
				if (y1>y0) ty = 255-ty;
			}

			p00 = pix;
			p01 = _this->tx_get_pixel(_this, x1, y0, rctx);
			p10 = _this->tx_get_pixel(_this, x0, y1, rctx);
			p11 = _this->tx_get_pixel(_this, x1, y1, rctx);

			p00 = EVG_LERP(p00, p01, tx);
			p10 = EVG_LERP(p10, p11, tx);
			pix = EVG_LERP(p00, p10, ty);

		}
#endif

write_pix:
		if (has_alpha) {
			cx = ((GF_COL_A(pix) + 1) * _this->alpha) >> 8;
			pix = ( (((u32)cx<<24) & 0xFF000000) ) | (pix & 0x00FFFFFF);
		}
		//move pixel to target pixel format, applying color transform matrix
		if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!yuv_type) {
				pix = gf_evg_ayuv_to_argb(rctx->surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  gf_evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), gf_evg_argb_to_ayuv
				pix = gf_cmx_apply(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply(&_this->cmat, pix);

			//dest is yuv, transform
			if (yuv_type)
				pix = gf_evg_argb_to_ayuv(rctx->surf, pix);
		}

		*data++ = pix;
		count--;
	}
}

static void tex_fill_run_3d_wide(GF_EVGStencil *p, EVGRasterCtx *rctx, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0, m_width, m_height;
	u64 pix;
	Bool has_alpha, has_cmat, repeat_s, repeat_t;
	Fixed x, y;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	EVG_YUVType yuv_type = rctx->surf->yuv_type;
	u64 *data = rctx->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;


#if USE_BILINEAR
	Bool use_bili = (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) ? 1 : 0;
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	Bool do_mx = gf_mx2d_is_identity(_this->smat_bck) ? GF_FALSE : GF_TRUE;

	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	m_width = _this->width-1;
	m_height = _this->height-1;

	PERSP_VARS_DECL

	while (count) {
		PERSP_APPLY

		if (do_mx)
			gf_mx2d_apply_coords(&_this->smat_bck, &ix, &iy);

		x = m_width * ix;
		y = m_height * iy;

		x0 = FIX2INT(x);

		if (x0 > m_width) {
			if (repeat_s) {
				while (x0>m_width) x0 -= m_width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				x0 = m_width;
			}
		}
		else if (x0 < 0) {
			if (repeat_s) {
				while (x0<0) x0 += m_width;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				x0 = 0;
			}
		}

		y0 = FIX2INT(y);
		if (y0 > m_height) {
			if (repeat_t) {
				while (y0>m_height) y0 -= m_height;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				y0 = m_height;
			}
		}
		else if (y0 < 0) {
			if (repeat_t) {
				while (y0<0) y0 += m_height;
			} else if (_this->fill_pad_color_wide) {
				pix = _this->fill_pad_color_wide;
				goto write_pix;
			} else {
				y0 = 0;
			}
		}

		pix = _this->tx_get_pixel_wide(_this, x0, y0, rctx);

		/*bilinear filtering*/
#if USE_BILINEAR
		if (use_bili) {
			Bool do_lerp=GF_TRUE;
			u64 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;

			tx = FIX2INT(gf_muldiv(x, 255, m_width) );
			ty = FIX2INT(gf_muldiv(y, 255, m_height) );

			x1 = (x0+incx);
			if (x1<0) {
				if (!repeat_s)
					do_lerp=GF_FALSE;
				else
					while (x1<0) x1 += m_width;
			} else {
				if (!repeat_s && (x1 > m_width))
					do_lerp=GF_FALSE;
				else
					x1 = x1 % m_width;
			}
			y1 = (y0+incy);
			if (y1<0) {
				if (!repeat_t)
					do_lerp=GF_FALSE;
				else
					while (y1<0) y1 += m_height;
			} else {
				if (!repeat_t && (y1 > m_height))
					do_lerp=GF_FALSE;
				else
					y1 = y1 % m_height;
			}

			if (!do_lerp) goto write_pix;

			if (incx>0) {
				if (x1<x0) tx = 255-tx;
			} else {
				if (x1>x0) tx = 255-tx;
			}
			if (incy>0) {
				if (y1<y0) ty = 255-ty;
			} else {
				if (y1>y0) ty = 255-ty;
			}

			p00 = pix;
			p01 = _this->tx_get_pixel_wide(_this, x1, y0, rctx);
			p10 = _this->tx_get_pixel_wide(_this, x0, y1, rctx);
			p11 = _this->tx_get_pixel_wide(_this, x1, y1, rctx);

			p00 = EVG_LERP_WIDE(p00, p01, tx);
			p10 = EVG_LERP_WIDE(p10, p11, tx);
			pix = EVG_LERP_WIDE(p00, p10, ty);

		}
#endif

write_pix:
		if (has_alpha) {
			u64 _a = (pix>>48)&0xFF;
			_a = (_a * _this->alpha) >> 8;
			_a<<=48;
			pix = ( (_a & 0xFFFF000000000000UL) ) | (pix & 0x0000FFFFFFFFFFFFUL);
		}
		//move pixel to target pixel format, applying color transform matrix
		if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!yuv_type) {
				pix = gf_evg_ayuv_to_argb_wide(rctx->surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply_wide(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  gf_evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), gf_evg_argb_to_ayuv
				pix = gf_cmx_apply_wide(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply_wide(&_this->cmat, pix);

			//dest is yuv, transform
			if (yuv_type)
				pix = gf_evg_argb_to_ayuv_wide(rctx->surf, pix);
		}

		*data++ = pix;
		count--;
	}
}

GF_EVGStencil *evg_texture_brush()
{
	EVG_Texture *tmp;
	GF_SAFEALLOC(tmp, EVG_Texture);
	if (!tmp) return 0L;

	tmp->fill_run = tex_fill_run;
	tmp->type = GF_STENCIL_TEXTURE;
	/*default is using the surface settings*/
	gf_evg_stencil_set_filter( (GF_EVGStencil *) tmp, GF_TEXTURE_FILTER_HIGH_SPEED);
	tmp->mod = 0;
	gf_cmx_init(&tmp->cmat);
	tmp->alpha = 255;
	return (GF_EVGStencil *) tmp;
}


u32 get_pix_argb(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF, *(pix+3) & 0xFF);
}
u32 get_pix_rgba(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_grba(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix+1) & 0xFF, *(pix) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_abgr(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix) & 0xFF, *(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF);
}
u32 get_pix_bgra(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *(pix) & 0xFF);
}
u32 get_pix_rgbx(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_xrgb(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF, *(pix+3) & 0xFF);
}
u32 get_pix_xbgr(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF);
}
u32 get_pix_bgrx(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *(pix) & 0xFF);
}
u32 get_pix_rgb_24(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *pix & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_gbr_24(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF, *pix & 0xFF);
}
u32 get_pix_bgr_24(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, * (pix+1) & 0xFF, *pix & 0xFF);
}
u32 get_pix_444(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = pix[0]&0x0f;
	u32 g = (pix[1]>>4)&0x0f;
	u32 b = pix[1]&0x0f;
	return GF_COL_ARGB(0xFF, (r << 4), (g << 4), (b << 4));
}
u32 get_pix_555(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = (pix[0]>>2) & 0x1f;
	u32 g = (pix[0])&0x3;
	g<<=3;
	g |= (pix[1]>>5) & 0x7;
	u32 b = pix[1] & 0x1f;
	return GF_COL_ARGB(0xFF, (r << 3), (g << 3), (b << 3));
}
u32 get_pix_565(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = (pix[0]>>3) & 0x1f;
	u32 g = (pix[0])&0x7;
	g<<=3;
	g |= (pix[1]>>5) & 0x7;
	u32 b = pix[1] & 0x1f;
	return GF_COL_ARGB(0xFF, (r << 3), (g << 2), (b << 3));
}
u32 get_pix_palette_alpha(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 val = *pix;
	if (val >= _this->palette_colors) return GF_COL_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	pix = (u8*)_this->palette + val*4;
	return GF_COL_ARGB(pix[_this->pidx_a], pix[_this->pidx_r], pix[_this->pidx_g], pix[_this->pidx_b]);
}
u32 get_pix_palette(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 val = *pix;
	if (val >= _this->palette_colors) return GF_COL_ARGB(0xFF, 0xFF, 0xFF, 0xFF);
	pix = (u8*)_this->palette + val*3;
	return GF_COL_ARGB(0xFF, pix[_this->pidx_r], pix[_this->pidx_g], pix[_this->pidx_b]);
}
u32 get_pix_grey(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u8 val = *pix;
	return GF_COL_ARGB(0xFF, val, val, val);
}
u32 get_pix_alphagrey(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 a, g;
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	a = *pix;
	g = *(pix+1);
	return GF_COL_ARGB(a, g, g, g);
}
u32 get_pix_greyalpha(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 a, g;
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	g = *pix;
	a = *(pix+1);
	return GF_COL_ARGB(a, g, g, g);
}
u32 get_pix_yuv420p(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + x/2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}

#ifdef GPAC_BIG_ENDIAN

#define GET_LE_10BIT_AS_8(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) >> 2 )
#define GET_LE_10BIT_AS_16(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) << 6 )

//#define GET_LE_10BIT_LEFT_AS_8(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) >> 8 )
#define GET_LE_10BIT_LEFT_AS_8(_ptr) ((_ptr)[1] )
#define GET_LE_10BIT_LEFT_AS_16(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) & 0xFFC0 )

#define GET_BE_10BIT_AS_8(_ptr) ( (*(u16 *)(_ptr)) >> 2 )
#define GET_BE_10BIT_AS_16(_ptr) ( (*(u16 *)(_ptr)) << 6 )

#else

#define GET_LE_10BIT_AS_8(_ptr) ( (*(u16 *)(_ptr)) >> 2 )
#define GET_LE_10BIT_AS_16(_ptr) ( (*(u16 *)(_ptr)) << 6 )

#define GET_LE_10BIT_LEFT_AS_8(_ptr) ( (*(u16 *)(_ptr)) >> 8 )
#define GET_LE_10BIT_LEFT_AS_16(_ptr) ( (*(u16 *)(_ptr)) & 0xFFC0 )


#define GET_BE_10BIT_AS_8(_ptr) ( (((u16)(_ptr)[0])<<8 | (u16)(_ptr)[1] ) >> 2 )
#define GET_BE_10BIT_AS_16(_ptr) ( (((u16)(_ptr)[0])<<8 | (u16)(_ptr)[1] ) << 6 )

#endif


u32 get_pix_yuv420p_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pV);

	return GF_COL_ARGB(0xFF, vy, vu, vv);
}

u64 get_pix_yuv420p_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);

	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv420p_a(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + x/2;
	u8 *pA = _this->pix_a  + y * _this->stride + x;

	return GF_COL_ARGB(*pA, *pY, *pU, *pV);
}
u32 get_pix_yuv422p(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y * _this->stride/2 + x/2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}
u32 get_pix_yuv422p_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pV);
	return GF_COL_ARGB(0xFF, vy, vu, vv);
}

u64 get_pix_yuv422p_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);
	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}

u32 get_pix_yuv444p(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride + x;
	u8 *pV = _this->pix_v + y * _this->stride + x;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}
u32 get_pix_yuv444p_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride + x*2;
	u8 *pV = _this->pix_v + y * _this->stride + x*2;

	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pV);
	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_yuv444p_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride + x*2;
	u8 *pV = _this->pix_v + y * _this->stride + x*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);
	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv444p_a(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride + x;
	u8 *pV = _this->pix_v + y * _this->stride + x;
	u8 *pA = _this->pix_a + y * _this->stride + x;
	return GF_COL_ARGB(*pA, *pY, *pU, *pV);
}
u32 get_pix_yuv_nv12(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *(pU+1));
}

u32 get_pix_yuv_nv12_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u  + y/2 * _this->stride + (x/2)*4;
	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pU+2);

	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_yuv_nv12_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pU+2);

	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv_nv21(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*2;
	return GF_COL_ARGB(0xFF, *pY, *(pU+1), *pU);
}
u32 get_pix_yuv_nv21_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pU+2);

	return GF_COL_ARGB(0xFF, vy, vv, vu);
}
u64 get_pix_yuv_nv21_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pU+2);
	return GF_COLW_ARGB(0xFFFF, vy, vv, vu);
}
u32 get_pix_yuyv(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[1];
	u8 v = pY[3];
	u8 luma = (x%2) ? pY[2] : pY[0];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_yvyu(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[3];
	u8 v = pY[1];
	u8 luma = (x%2) ? pY[2] : pY[0];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_uyvy(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[0];
	u8 v = pY[2];
	u8 luma = (x%2) ? pY[3] : pY[1];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_vyuy(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[2];
	u8 v = pY[0];
	u8 luma = (x%2) ? pY[3] : pY[1];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_yuv444_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u32 val;
	u8 *p_src = _this->pixels + y * _this->stride + x*4;
	val = p_src[3]; val<<=8;
	val |= p_src[2]; val<<=8;
	val |= p_src[1]; val<<=8;
	val |= p_src[0];

	vu = (u8) ( ((val>>2) & 0x3FF) >> 2);
	vy = (u8) ( ((val>>12) & 0x3FF) >> 2);
	vv = (u8) ( ((val>>22)) >> 2);

	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_yuv444_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u32 val;
	u8 *p_src = _this->pixels + y * _this->stride + x*4;
	val = p_src[3]; val<<=8;
	val |= p_src[2]; val<<=8;
	val |= p_src[1]; val<<=8;
	val |= p_src[0];

	vu = (u16) ( ((val>>2) & 0x3FF) << 6);
	vy = (u16) ( ((val>>12) & 0x3FF) << 6);
	vv = (u16) ( ((val>>22)) << 6);
	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}

u32 get_pix_v210(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	u32 val1, val2;
	u32 x_offset = x%6;
	x-=x_offset;
	u8 *p_src = _this->pixels + y * _this->stride + (x * 16) / 6;
	if (x_offset>=4) p_src += 8;
	else if (x_offset>=2) p_src += 4;

	val1 = p_src[3]; val1<<=8;
	val1 |= p_src[2]; val1<<=8;
	val1 |= p_src[1]; val1<<=8;
	val1 |= p_src[0];

	if (x_offset==0) {
		vu = (u8) ( ((val1) & 0x3FF) >> 2);
		vy = (u8) ( ((val1>>10) & 0x3FF) >> 2);
		vv = (u8) ( ((val1>>20) & 0x3FF) >> 2);
	} else {
		val2 = p_src[7]; val2<<=8;
		val2 |= p_src[6]; val2<<=8;
		val2 |= p_src[5]; val2<<=8;
		val2 |= p_src[4];

		if (x_offset==1) {
			vu = (u8) ( ((val1) & 0x3FF) >> 2);
			vv = (u8) ( ((val1>>20) & 0x3FF) >> 2);
			vy = (u8) ( ((val2) & 0x3FF) >> 2);
		} else if (x_offset==2) {
			vu = (u8) ( ((val1>>10) & 0x3FF) >> 2);
			vy = (u8) ( ((val1>>20) & 0x3FF) >> 2);
			vv = (u8) ( ((val2) & 0x3FF) >> 2);
		} else if (x_offset==3) {
			vu = (u8) ( ((val1>>10) & 0x3FF) >> 2);
			vv = (u8) ( ((val2) & 0x3FF) >> 2);
			vy = (u8) ( ((val2>>10) & 0x3FF) >> 2);
		} else if (x_offset==4) {
			vu = (u8) ( ((val1>>20) & 0x3FF) >> 2);
			vy = (u8) ( ((val2) & 0x3FF) >> 2);
			vv = (u8) ( ((val2>>10) & 0x3FF) >> 2);
		} else {
			vu = (u8) ( ((val1>>20) & 0x3FF) >> 2);
			vv = (u8) ( ((val2>>10) & 0x3FF) >> 2);
			vy = (u8) ( ((val2>>20) & 0x3FF) >> 2);
		}
	}
	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_v210_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u32 val1, val2;
	u32 x_offset = x%6;
	x-=x_offset;
	u8 *p_src = _this->pixels + y * _this->stride + (x * 16) / 6;
	if (x_offset>=4) p_src += 8;
	else if (x_offset>=2) p_src += 4;

	val1 = p_src[3]; val1<<=8;
	val1 |= p_src[2]; val1<<=8;
	val1 |= p_src[1]; val1<<=8;
	val1 |= p_src[0];

	if (x_offset==0) {
		vu = (u16) ( ((val1) & 0x3FF) << 6);
		vy = (u16) ( ((val1>>10) & 0x3FF) << 6);
		vv = (u16) ( ((val1>>20) & 0x3FF) << 6);
	} else {
		val2 = p_src[7]; val2<<=8;
		val2 |= p_src[6]; val2<<=8;
		val2 |= p_src[5]; val2<<=8;
		val2 |= p_src[4];

		if (x_offset==1) {
			vu = (u16) ( ((val1) & 0x3FF) << 6);
			vv = (u16) ( ((val1>>20) & 0x3FF) << 6);
			vy = (u16) ( ((val2) & 0x3FF) << 6);
		} else if (x_offset==2) {
			vu = (u16) ( ((val1>>10) & 0x3FF) << 6);
			vy = (u16) ( ((val1>>20) & 0x3FF) << 6);
			vv = (u16) ( ((val2) & 0x3FF) << 6);
		} else if (x_offset==3) {
			vu = (u16) ( ((val1>>10) & 0x3FF) << 6);
			vv = (u16) ( ((val2) & 0x3FF) << 6);
			vy = (u16) ( ((val2>>10) & 0x3FF) << 6);
		} else if (x_offset==4) {
			vu = (u16) ( ((val1>>20) & 0x3FF) << 6);
			vy = (u16) ( ((val2) & 0x3FF) << 6);
			vv = (u16) ( ((val2>>10) & 0x3FF) << 6);
		} else {
			vu = (u16) ( ((val1>>20) & 0x3FF) << 6);
			vv = (u16) ( ((val2>>10) & 0x3FF) << 6);
			vy = (u16) ( ((val2>>20) & 0x3FF) << 6);
		}
	}
	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}

u32 get_pix_yuyv_10(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u8 vy, vu, vv;
	Bool odd = x%2;
	u8 *p_src = _this->pixels + y * _this->stride + (x/2)*8;

	vu = GET_LE_10BIT_LEFT_AS_8(p_src + _this->off_u);
	if (odd)
		vy = GET_LE_10BIT_LEFT_AS_8(p_src + _this->off_y+4);
	else
		vy = GET_LE_10BIT_LEFT_AS_8(p_src + _this->off_y);
	vv = GET_LE_10BIT_LEFT_AS_8(p_src + _this->off_v);
	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_yuyv_10_wide(EVG_Texture *_this, u32 x, u32 y, EVGRasterCtx *ctx)
{
	u16 vy, vu, vv;
	u32 odd = x%2;
	u8 *p_src = _this->pixels + y * _this->stride + (x/2)*8;

	vu = GET_LE_10BIT_LEFT_AS_16(p_src + _this->off_u);
	if (odd)
		vy = GET_LE_10BIT_LEFT_AS_16(p_src + _this->off_y+4);
	else
		vy = GET_LE_10BIT_LEFT_AS_16(p_src + _this->off_y);
	vv = GET_LE_10BIT_LEFT_AS_16(p_src + _this->off_v);
	return GF_COLW_ARGB(0xFFFF, vy, vu, vv);
}
u64 default_get_pixel_wide(struct __evg_texture *_this, u32 x, u32 y, EVGRasterCtx *rctx)
{
	return evg_col_to_wide( _this->tx_get_pixel(_this, x, y, rctx) );
}

static void texture_set_callbacks(EVG_Texture *_this)
{
	Bool swap_uv = GF_FALSE;
	if (_this->tx_callback)
		return;

	_this->is_wide = 0;
	_this->is_transparent = 0;
	_this->tx_get_pixel = NULL;
	_this->tx_get_pixel_wide = default_get_pixel_wide;

	switch (_this->pixel_format) {
	case GF_PIXEL_RGBA:
		_this->tx_get_pixel = get_pix_rgba;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_ARGB:
		_this->tx_get_pixel = get_pix_argb;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_ABGR:
		_this->tx_get_pixel = get_pix_abgr;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_BGRA:
		_this->tx_get_pixel = get_pix_bgra;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_RGBX:
		_this->tx_get_pixel = get_pix_rgbx;
		return;
	case GF_PIXEL_BGRX:
		_this->tx_get_pixel = get_pix_bgrx;
		return;
	case GF_PIXEL_XRGB:
		_this->tx_get_pixel = get_pix_xrgb;
		return;
	case GF_PIXEL_XBGR:
		_this->tx_get_pixel = get_pix_xbgr;
		return;
	case GF_PIXEL_RGB:
		_this->tx_get_pixel = get_pix_rgb_24;
		return;
	case GF_PIXEL_BGR:
		_this->tx_get_pixel = get_pix_bgr_24;
		return;
	case GF_PIXEL_RGB_444:
		_this->tx_get_pixel = get_pix_444;
		return;
	case GF_PIXEL_RGB_555:
		_this->tx_get_pixel = get_pix_555;
		return;
	case GF_PIXEL_RGB_565:
		_this->tx_get_pixel = get_pix_565;
		return;
	case GF_PIXEL_GREYSCALE:
		if (_this->palette && _this->palette_colors) {
			if (_this->palette_comp==4)
				_this->tx_get_pixel = get_pix_palette_alpha;
			else
				_this->tx_get_pixel = get_pix_palette;
		} else
			_this->tx_get_pixel = get_pix_grey;
		return;
	case GF_PIXEL_ALPHAGREY:
		_this->tx_get_pixel = get_pix_alphagrey;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_GREYALPHA:
		_this->tx_get_pixel = get_pix_greyalpha;
		_this->is_transparent = 1;
		return;
	case GF_PIXEL_YUV:
	//we swap pU and pV at setup, use the same function
	case GF_PIXEL_YVU:
		_this->tx_get_pixel = get_pix_yuv420p;
		break;
	case GF_PIXEL_YUVA:
		_this->tx_get_pixel = get_pix_yuv420p_a;
		_this->is_transparent = 1;
		break;
	case GF_PIXEL_YUV422:
		_this->tx_get_pixel = get_pix_yuv422p;
		break;
	case GF_PIXEL_YUV444:
		_this->tx_get_pixel = get_pix_yuv444p;
		break;
	case GF_PIXEL_YUVA444:
		_this->tx_get_pixel = get_pix_yuv444p_a;
		_this->is_transparent = 1;
		break;
	case GF_PIXEL_NV12:
		_this->tx_get_pixel = get_pix_yuv_nv12;
		break;
	case GF_PIXEL_NV21:
		_this->tx_get_pixel = get_pix_yuv_nv21;
		break;
	case GF_PIXEL_YUYV:
		_this->tx_get_pixel = get_pix_yuyv;
		return;
	case GF_PIXEL_YVYU:
		_this->tx_get_pixel = get_pix_yvyu;
		return;
	case GF_PIXEL_UYVY:
		_this->tx_get_pixel = get_pix_uyvy;
		return;
	case GF_PIXEL_VYUY:
		_this->tx_get_pixel = get_pix_vyuy;
		return;
	case GF_PIXEL_YUV_10:
		_this->tx_get_pixel = get_pix_yuv420p_10;
		_this->tx_get_pixel_wide = get_pix_yuv420p_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_YUV422_10:
		_this->tx_get_pixel = get_pix_yuv422p_10;
		_this->tx_get_pixel_wide = get_pix_yuv422p_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_YUV444_10:
		_this->tx_get_pixel = get_pix_yuv444p_10;
		_this->tx_get_pixel_wide = get_pix_yuv444p_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_NV12_10:
		_this->tx_get_pixel = get_pix_yuv_nv12_10;
		_this->tx_get_pixel_wide = get_pix_yuv_nv12_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_NV21_10:
		_this->tx_get_pixel = get_pix_yuv_nv21_10;
		_this->tx_get_pixel_wide = get_pix_yuv_nv21_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_YUVA444_PACK:
		_this->tx_get_pixel = get_pix_rgba;
		_this->is_transparent = 1;
		break;
	case GF_PIXEL_UYVA444_PACK:
		_this->tx_get_pixel = get_pix_grba;
		_this->is_transparent = 1;
		break;
	case GF_PIXEL_YUV444_PACK:
		_this->tx_get_pixel = get_pix_rgb_24;
		break;
	case GF_PIXEL_VYU444_PACK:
		_this->tx_get_pixel = get_pix_gbr_24;
		break;
	case GF_PIXEL_YUV444_10_PACK:
		_this->tx_get_pixel = get_pix_yuv444_10;
		_this->tx_get_pixel_wide = get_pix_yuv444_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_V210:
		_this->tx_get_pixel = get_pix_v210;
		_this->tx_get_pixel_wide = get_pix_v210_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_YUYV_10:
		_this->off_y=0;
		_this->off_u=2;
		_this->off_v=4;
		_this->tx_get_pixel = get_pix_yuyv_10;
		_this->tx_get_pixel_wide = get_pix_yuyv_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_YVYU_10:
		_this->off_y=0;
		_this->off_u=4;
		_this->off_v=2;
		_this->tx_get_pixel = get_pix_yuyv_10;
		_this->tx_get_pixel_wide = get_pix_yuyv_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_UYVY_10:
		_this->off_y=2;
		_this->off_u=0;
		_this->off_v=4;
		_this->tx_get_pixel = get_pix_yuyv_10;
		_this->tx_get_pixel_wide = get_pix_yuyv_10_wide;
		_this->is_wide = 1;
		break;
	case GF_PIXEL_VYUY_10:
		_this->off_y=2;
		_this->off_u=4;
		_this->off_v=2;
		_this->tx_get_pixel = get_pix_yuyv_10;
		_this->tx_get_pixel_wide = get_pix_yuyv_10_wide;
		_this->is_wide = 1;
		break;
	default:
		return;
	}
	//assign image planes
	if (_this->pix_u) return;

	switch (_this->pixel_format) {
	case GF_PIXEL_YVU:
		swap_uv = GF_TRUE;
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV:
		if (!_this->stride_uv) _this->stride_uv = _this->stride/2;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height/2;
		if (swap_uv) {
			u8 *tmp = _this->pix_u;
			_this->pix_u = _this->pix_v;
			_this->pix_v = tmp;
		}
		return;
	case GF_PIXEL_YUVA:
		if (!_this->stride_uv) _this->stride_uv = _this->stride/2;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height/2;
		_this->pix_a = _this->pix_v + _this->stride_uv * _this->height/2;
		return;
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV422:
		if (!_this->stride_uv) _this->stride_uv = _this->stride/2;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height;
		return;
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_YUV444:
		if (!_this->stride_uv) _this->stride_uv = _this->stride;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height;
		return;
	case GF_PIXEL_YUVA444:
		if (!_this->stride_uv) _this->stride_uv = _this->stride;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height;
		_this->pix_a = _this->pix_v + _this->stride_uv * _this->height;
		return;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		if (!_this->stride_uv) _this->stride_uv = _this->stride;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		return;
	}
}

static GF_Err gf_evg_stencil_set_texture_internal(GF_EVGStencil * st, u32 width, u32 height, GF_PixelFormat pixelFormat, const char *pixels, u32 stride, const char *u_plane, const char *v_plane, u32 uv_stride, const char *alpha_plane, u32 alpha_stride)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !pixels || !width || !height || _this->owns_texture)
		return GF_BAD_PARAM;

	_this->pixels = NULL;
	_this->is_yuv = GF_FALSE;
	_this->palette = NULL;
	_this->palette_colors = 0;

	switch (pixelFormat) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
	case GF_PIXEL_BGRX:
		_this->Bpp = 4;
		break;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
		_this->Bpp = 3;
		break;
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		_this->Bpp = 2;
		break;
	case GF_PIXEL_GREYSCALE:
		_this->Bpp = 1;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
		_this->is_yuv = GF_TRUE;
		_this->Bpp = 1;
		break;
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		_this->is_yuv = GF_TRUE;
		_this->Bpp = 2;
		break;
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
		_this->is_yuv = GF_TRUE;
		_this->Bpp = 1;
		if (!stride)
			stride = 4 * width;
		break;

	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
		_this->is_yuv = GF_TRUE;
		_this->Bpp = 2;
		if (!stride)
			stride = 4 * width;
		break;
	case GF_PIXEL_YUVA444_PACK:
	case GF_PIXEL_UYVA444_PACK:
		_this->Bpp = 4;
		_this->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YUV444_10_PACK:
		_this->Bpp = 4;
		_this->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YUV444_PACK:
	case GF_PIXEL_VYU444_PACK:
		_this->Bpp = 3;
		_this->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_V210:
		_this->Bpp = 2;
		_this->is_yuv = GF_TRUE;
		if (!stride) {
			stride = width;
			while (stride % 48) stride++;
			stride = stride * 16 / 6; //4 x 32 bits to represent 6 pixels
		}
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	if (!stride)
		stride = _this->Bpp * width;

	_this->pixel_format = pixelFormat;
	_this->width = width;
	_this->height = height;
	_this->stride = stride;
	_this->stride_uv = uv_stride;
	_this->stride_alpha = alpha_stride ? alpha_stride : stride;
	_this->pixels = (char *) pixels;
	_this->pix_u = (char *) u_plane;
	_this->pix_v = (char *) v_plane;
	texture_set_callbacks(_this);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_texture_planes(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, const u8 *y_or_rgb, u32 stride, const u8 *u_plane, const u8 *v_plane, u32 uv_stride, const u8 *alpha_plane, u32 stride_alpha)
{
 	return gf_evg_stencil_set_texture_internal(stencil, width, height, pixelFormat, y_or_rgb, stride, u_plane, v_plane, uv_stride, alpha_plane, stride_alpha);
}
GF_EXPORT
GF_Err gf_evg_stencil_set_texture(GF_EVGStencil *stencil, u8 *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat)
{
	return gf_evg_stencil_set_texture_internal(stencil, width, height, pixelFormat, pixels, stride, NULL, NULL, 0, NULL, 0);
}

GF_EXPORT
Bool gf_evg_texture_format_ok(GF_PixelFormat pixelFormat)
{
	EVG_Texture _tx;
	memset(&_tx, 0, sizeof(EVG_Texture));
	_tx.type = GF_STENCIL_TEXTURE;
	GF_Err e = gf_evg_stencil_set_texture_internal((GF_EVGStencil *) &_tx, 2, 2, pixelFormat, (const u8 *) &_tx, 0, NULL, NULL, 0, NULL, 0);
	if (e==GF_OK) return GF_TRUE;
	return GF_FALSE;
}


GF_EXPORT
GF_Err gf_evg_stencil_set_palette(GF_EVGStencil *stencil, const u8 *palette, u32 pix_fmt, u32 nb_cols)
{
	EVG_Texture *_this = (EVG_Texture *) stencil;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !palette || !nb_cols)
		return GF_BAD_PARAM;
	if (_this->pixel_format != GF_PIXEL_GREYSCALE) return GF_BAD_PARAM;
	_this->palette = palette;
	_this->palette_colors = nb_cols;
	_this->palette_pfmt = pix_fmt;
	switch (pix_fmt) {
	case GF_PIXEL_RGB:
		_this->palette_comp = 3;
		_this->pidx_r=0;
		_this->pidx_g=1;
		_this->pidx_b=2;
		break;
	case GF_PIXEL_BGR:
		_this->palette_comp = 3;
		_this->pidx_r=2;
		_this->pidx_g=1;
		_this->pidx_b=0;
		break;
	case GF_PIXEL_RGBA:
		_this->palette_comp = 4;
		_this->pidx_r=0;
		_this->pidx_g=1;
		_this->pidx_b=2;
		_this->pidx_a=3;
		break;
	case GF_PIXEL_ARGB:
		_this->palette_comp = 4;
		_this->pidx_r=1;
		_this->pidx_g=2;
		_this->pidx_b=3;
		_this->pidx_a=0;
		break;
	case GF_PIXEL_ABGR:
		_this->palette_comp = 4;
		_this->pidx_r=3;
		_this->pidx_g=2;
		_this->pidx_b=1;
		_this->pidx_a=0;
		break;
	case GF_PIXEL_BGRA:
		_this->palette_comp = 4;
		_this->pidx_r=2;
		_this->pidx_g=1;
		_this->pidx_b=0;
		_this->pidx_a=3;
		break;
	default:
		_this->palette = NULL;
		_this->palette_colors = 0;
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}
GF_EXPORT
GF_Err gf_evg_stencil_get_texture_planes(GF_EVGStencil *stencil, u8 **pY_or_RGB, u8 **pU, u8 **pV, u8 **pA, u32 *stride, u32 *stride_uv)
{
	EVG_Texture *_this = (EVG_Texture *) stencil;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) )
		return GF_BAD_PARAM;


	if (pY_or_RGB) *pY_or_RGB = _this->pixels;
	if (pU) *pU = _this->pix_u;
	if (pV) *pV = _this->pix_v;
	if (pA) *pA = _this->pix_a;
	if (stride) *stride = _this->stride;
	if (stride_uv) *stride_uv = _this->stride_uv;

	return GF_OK;
}


GF_EXPORT
GF_Err gf_evg_stencil_set_texture_parametric(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, gf_evg_texture_callback callback, void *cbk_data, Bool use_screen_coords)
{
	EVG_Texture *_this = (EVG_Texture *) stencil;
	u8 data=0;
	GF_Err e;
	if (!callback) return GF_BAD_PARAM;
	e = gf_evg_stencil_set_texture_internal(stencil, width, height, pixelFormat, &data, width, NULL, NULL, 0, NULL, 0);
	if (e) return e;
	_this->pixels = NULL;
	_this->tx_get_pixel = evg_paramtx_get_pixel;
	_this->tx_get_pixel_wide = evg_paramtx_get_pixel_wide;
	_this->is_wide = 1;

	_this->tx_callback = callback;
	_this->tx_callback_udta = cbk_data;
	_this->tx_callback_screen_coords = use_screen_coords;
	return GF_OK;
}

void evg_texture_init(GF_EVGStencil *p, GF_EVGSurface *surf)
{
	GF_Point2D p0, p1;
	EVG_Texture *_this = (EVG_Texture *) p;

	p0.x = p0.y = p1.y = 0;
	p1.x = FIX_ONE;
	gf_mx2d_apply_point(&_this->smat, &p0);
	gf_mx2d_apply_point(&_this->smat, &p1);
	_this->inc_x = p1.x - p0.x;
	_this->inc_y = p1.y - p0.y;

	if (surf->is_3d_matrix) {
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_3d_wide;
		} else {
			_this->fill_run = tex_fill_run_3d;
		}
	} else if (_this->tx_callback && _this->tx_callback_screen_coords) {
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_callback_wide;
		} else {
			_this->fill_run = tex_fill_run_callback;
		}
	}
	else if ((_this->filter != GF_TEXTURE_FILTER_HIGH_QUALITY) && (_this->alpha == 255)
		&& !_this->smat.m[1] && !_this->smat.m[3]
		&& _this->cmat.identity
	) {
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_straight_wide;
		} else {
			_this->fill_run = tex_fill_run_straight;
		}
	} else {
		if (!_this->cmat.identity && _this->is_yuv && surf->yuv_type) {
//			evg_make_ayuv_color_mx(&_this->cmat, &_this->yuv_cmat);
			_this->yuv_cmat = _this->cmat;
		}
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_wide;
		} else {
			_this->fill_run = tex_fill_run;
		}
	}

	if (!_this->pad_rbg) {
		_this->fill_pad_color = 0;
	} else if (_this->is_transparent) {
		_this->fill_pad_color = GF_COL_ARGB(0, 0xFF, 0xFF, 0xFF);
	} else if (_this->is_yuv) {
		_this->fill_pad_color = gf_evg_argb_to_ayuv(surf, _this->pad_rbg);
	} else {
		_this->fill_pad_color = _this->pad_rbg;
	}
	_this->fill_pad_color_wide = evg_col_to_wide(_this->fill_pad_color);

	texture_set_callbacks(_this);
}

GF_EXPORT
GF_Err gf_evg_stencil_set_pad_color(GF_EVGStencil * st, GF_Color pad_color)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	_this->pad_rbg = pad_color;
	return GF_OK;
}

GF_EXPORT
u32 gf_evg_stencil_get_pad_color(GF_EVGStencil * st)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return 0;
	return _this->pad_rbg;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_mapping(GF_EVGStencil * st, GF_TextureMapFlags mode)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	_this->mod = mode;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_filter(GF_EVGStencil * st, GF_TextureFilter filter_mode)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	_this->filter = filter_mode;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_color_matrix(GF_EVGStencil * st, GF_ColorMatrix *cmat)
{
	Bool is_grad;
	EVG_BaseGradient *_this = (EVG_BaseGradient *)st;
	if (!_this) return GF_BAD_PARAM;
	is_grad = ((_this->type==GF_STENCIL_LINEAR_GRADIENT) || (_this->type==GF_STENCIL_RADIAL_GRADIENT)) ? GF_TRUE : GF_FALSE;


	if (!cmat) {
		if (is_grad && !_this->cmat.identity)
			_this->updated = 1;
		gf_cmx_init(&_this->cmat);
	} else {
		if (is_grad && memcmp(&_this->cmat.m, &cmat->m, sizeof(Fixed)*20))
			_this->updated = 1;
		gf_cmx_copy(&_this->cmat, cmat);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_get_color_matrix(GF_EVGStencil * st, GF_ColorMatrix *cmat)
{
	EVG_BaseGradient *_this = (EVG_BaseGradient *)st;
	if (!_this || !cmat) return GF_BAD_PARAM;
	gf_cmx_copy(cmat, &_this->cmat);
	return GF_OK;
}
static u32 gf_evg_stencil_get_pixel_intern(EVG_Texture *_this, s32 x, s32 y, Bool want_yuv)
{
	u32 col;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !_this->tx_get_pixel) return 0;
	if (x<0) x=0;
	else if ((u32) x>=_this->width) x = _this->width-1;

	if (y<0) y=0;
	else if ((u32) y>=_this->height) y = _this->height-1;

	col = _this->tx_get_pixel(_this, x, y, NULL);
	if (_this->is_yuv) {
		if (!want_yuv) return gf_evg_ayuv_to_argb(NULL, col);
	} else {
		if (want_yuv) return gf_evg_argb_to_ayuv(NULL, col);
	}
	return col;
}

GF_EXPORT
u32 gf_evg_stencil_get_pixel(GF_EVGStencil *st, s32 x, s32 y)
{
	return gf_evg_stencil_get_pixel_intern((EVG_Texture *)st, x, y, GF_FALSE);
}

GF_EXPORT
u32 gf_evg_stencil_get_pixel_yuv(GF_EVGStencil *st, s32 x, s32 y)
{
	return gf_evg_stencil_get_pixel_intern((EVG_Texture *)st, x, y, GF_TRUE);
}

u32 gf_evg_stencil_get_pixel_fast(GF_EVGStencil *st, s32 x, s32 y)
{
	return ((EVG_Texture *)st)->tx_get_pixel((EVG_Texture *)st, x, y, NULL);
}
u64 gf_evg_stencil_get_pixel_wide_fast(GF_EVGStencil *st, s32 x, s32 y)
{
	return ((EVG_Texture *)st)->tx_get_pixel_wide((EVG_Texture *)st, x, y, NULL);
}



static u64 gf_evg_stencil_get_pixel_wide_intern(EVG_Texture *_this, s32 x, s32 y, Bool want_yuv)
{
	u64 col;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !_this->tx_get_pixel_wide) return 0;
	if (x<0) x=0;
	else if ((u32) x>=_this->width) x = _this->width-1;

	if (y<0) y=0;
	else if ((u32) y>=_this->height) y = _this->height-1;

	col = _this->tx_get_pixel_wide(_this, x, y, NULL);
	if (_this->is_yuv) {
		if (!want_yuv) return gf_evg_ayuv_to_argb_wide(NULL, col);
	} else {
		if (want_yuv) return gf_evg_argb_to_ayuv_wide(NULL, col);
	}
	return col;
}

GF_EXPORT
u64 gf_evg_stencil_get_pixel_wide(GF_EVGStencil *st, s32 x, s32 y)
{
	return gf_evg_stencil_get_pixel_wide_intern((EVG_Texture *)st, x, y, GF_FALSE);
}

GF_EXPORT
u64 gf_evg_stencil_get_pixel_yuv_wide(GF_EVGStencil *st, s32 x, s32 y)
{
	return gf_evg_stencil_get_pixel_intern((EVG_Texture *)st, x, y, GF_TRUE);
}
static GF_Vec4 gf_evg_stencil_get_pixel_f_intern(EVG_Texture *_this, Float x, Float y, Bool want_yuv)
{
	u32 col;
	GF_Vec4 res;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || (!_this->tx_get_pixel && !_this->tx_get_pixel_wide) ) {
		memset(&res, 0, sizeof(GF_Vec4));
		return res;
	}

	if (_this->mod & GF_TEXTURE_FLIP_X) x = -x;
	if (_this->mod & GF_TEXTURE_FLIP_Y) y = -y;

	x*=_this->width;
	y*=_this->height;
	if (_this->mod & GF_TEXTURE_REPEAT_S) {
		while (x<0) x += _this->width;
		while (x>=_this->width) x -= _this->width;
	} else {
		if (x<0) x=0;
		else if (x>=_this->width) x = (Float)_this->width-1;
	}

	if (_this->mod & GF_TEXTURE_REPEAT_T) {
		while (y<0) y += _this->height;
		while (y>=_this->height) y -= _this->height;
	} else {
		if (y<0) y=0;
		else if (y>=_this->height) y = (Float)_this->height-1;
	}

	//10-bit or more texture, use wide and convert to float
	if (_this->is_wide) {
		u64 colw;
		if (_this->filter==GF_TEXTURE_FILTER_HIGH_SPEED) {
			colw = _this->tx_get_pixel_wide(_this, (s32) x, (s32) y, NULL);
		} else {
			u32 _x = (u32) floor(x);
			u32 _y = (u32) floor(y);
			if (_this->filter==GF_TEXTURE_FILTER_MID) {
				if ((x - _x > 0.5) && _x+1<_this->width) _x++;
				if ((y - _y > 0.5) && _y+1<_this->height) _y++;
				colw = _this->tx_get_pixel_wide(_this, _x, _y, NULL);
			} else {
				u64 col01, col11, col10;
				s32 _x1 = _x+1;
				s32 _y1 = _y+1;
				u8 diff_x = (u8) (255 * (x - _x));
				u8 diff_y = (u8) (255 * (y - _y));

				if ((u32)_x1>=_this->width) _x1 = _this->width-1;
				if ((u32)_y1>=_this->height) _y1 = _this->height-1;
				colw = _this->tx_get_pixel_wide(_this, _x, _y, NULL);
				col10 = _this->tx_get_pixel_wide(_this, _x1, _y, NULL);
				col01 = _this->tx_get_pixel_wide(_this, _x, _y1, NULL);
				col11 = _this->tx_get_pixel_wide(_this, _x1, _y1, NULL);
				colw = EVG_LERP_WIDE(colw, col10, diff_x);
				col11 = EVG_LERP_WIDE(col01, col11, diff_x);
				colw = EVG_LERP_WIDE(colw, col11, diff_y);
			}
		}
		if (_this->is_yuv) {
			if (!want_yuv) colw = gf_evg_ayuv_to_argb_wide(NULL, colw);
		} else {
			if (want_yuv) colw = gf_evg_argb_to_ayuv_wide(NULL, colw);
		}

		res.x = GF_COLW_R(colw); res.x /= 0xFFFF;
		res.y = GF_COLW_G(colw); res.y /= 0xFFFF;
		res.z = GF_COLW_B(colw); res.z /= 0xFFFF;
		res.q = GF_COLW_A(colw); res.q /= 0xFFFF;
		return res;
	}

	//8-bit texture, use regular and convert to float
	if (_this->filter==GF_TEXTURE_FILTER_HIGH_SPEED) {
		col = _this->tx_get_pixel(_this, (s32) x, (s32) y, NULL);
	} else {
		u32 _x = (u32) floor(x);
		u32 _y = (u32) floor(y);
		if (_this->filter==GF_TEXTURE_FILTER_MID) {
			if ((x - _x > 0.5) && _x+1<_this->width) _x++;
			if ((y - _y > 0.5) && _y+1<_this->height) _y++;
			col = _this->tx_get_pixel(_this, _x, _y, NULL);
		} else {
			u32 col01, col11, col10;
			s32 _x1 = _x+1;
			s32 _y1 = _y+1;
			u8 diff_x = (u8) (255 * (x - _x));
			u8 diff_y = (u8) (255 * (y - _y));

			if ((u32)_x1>=_this->width) _x1 = _this->width-1;
			if ((u32)_y1>=_this->height) _y1 = _this->height-1;
			col = _this->tx_get_pixel(_this, _x, _y, NULL);
			col10 = _this->tx_get_pixel(_this, _x1, _y, NULL);
			col01 = _this->tx_get_pixel(_this, _x, _y1, NULL);
			col11 = _this->tx_get_pixel(_this, _x1, _y1, NULL);
			col = EVG_LERP(col, col10, diff_x);
			col11 = EVG_LERP(col01, col11, diff_x);
			col = EVG_LERP(col, col11, diff_y);
		}
	}
	if (_this->is_yuv) {
		if (!want_yuv) col = gf_evg_ayuv_to_argb(NULL, col);
	} else {
		if (want_yuv) col = gf_evg_argb_to_ayuv(NULL, col);
	}
	res.x = (Float) ( GF_COL_R(col) / 255.0 );
	res.y = (Float)( GF_COL_G(col) / 255.0 );
	res.z = (Float)( GF_COL_B(col) / 255.0 );
	res.q = (Float)( GF_COL_A(col) / 255.0 );
	return res;
}

GF_EXPORT
GF_Vec4 gf_evg_stencil_get_pixel_f(GF_EVGStencil *st, Float x, Float y)
{
	return gf_evg_stencil_get_pixel_f_intern((EVG_Texture *)st, x, y, GF_FALSE);
}

GF_EXPORT
GF_Vec4 gf_evg_stencil_get_pixel_yuv_f(GF_EVGStencil *st, Float x, Float y)
{
	return gf_evg_stencil_get_pixel_f_intern((EVG_Texture *)st, x, y, GF_TRUE);
}

GF_EXPORT
GF_Err gf_evg_stencil_set_alpha(GF_EVGStencil * st, u8 alpha)
{
	EVG_Texture *_this = (EVG_Texture *)st;
	if (!_this) return GF_BAD_PARAM;
	if (_this->type==GF_STENCIL_SOLID) {
		((EVG_Brush *)st)->alpha = alpha;
		return GF_OK;
	}
	if (_this->type==GF_STENCIL_TEXTURE)
		_this->alpha = alpha;
	else {
		if ( ((EVG_BaseGradient*)st)->alpha != alpha) {
			((EVG_BaseGradient*)st)->updated = 1;
		}
		((EVG_BaseGradient*)st)->alpha = alpha;
	}
	return GF_OK;
}

GF_EXPORT
u8 gf_evg_stencil_get_alpha(GF_EVGStencil * st)
{
	EVG_Texture *_this = (EVG_Texture *)st;
	if (!_this) return 0;
	if (_this->type==GF_STENCIL_SOLID) {
		return ((EVG_Brush *)st)->alpha;
	}
	if (_this->type==GF_STENCIL_TEXTURE)
		return _this->alpha;

	return ((EVG_BaseGradient*)st)->alpha;
}

void *evg_fill_run(GF_EVGStencil *p, EVGRasterCtx *rctx, EVG_Span *span, s32 y)
{

	if (rctx->surf->direct_yuv_3d) {
		u32 *src = rctx->stencil_pix_run;
		return src + span->x;
	}
	if (rctx->surf->is_shader) {
		rctx->frag_param.coverage = span->coverage;
		rctx->frag_param.odd_flag = span->odd_flag;
		rctx->frag_param.screen_x = (Float) span->x;
		rctx->frag_param.screen_y = (Float) y;
	}
	if (rctx->surf->odd_fill) {
		if (!span->odd_flag && rctx->surf->sten2) {
			rctx->surf->sten2->fill_run(rctx->surf->sten2, rctx, span->x, y, span->len);
		} else {
			p->fill_run(p, rctx, span->x, y, span->len);
		}
		return rctx->stencil_pix_run;
	}

	if (p)
		p->fill_run(p, rctx, span->x, y, span->len);
	if (rctx->surf->update_run) {
		void *bck = rctx->stencil_pix_run;
		if (rctx->surf->sten2) {
			rctx->stencil_pix_run = rctx->stencil_pix_run2;
			rctx->surf->sten2->fill_run(rctx->surf->sten2, rctx, span->x, y, span->len);
		}
		if (rctx->surf->sten3) {
			rctx->stencil_pix_run = rctx->stencil_pix_run3;
			rctx->surf->sten3->fill_run(rctx->surf->sten3, rctx, span->x, y, span->len);
		}
		rctx->stencil_pix_run = bck;
		rctx->surf->update_run(rctx, span->len);
		return rctx->stencil_pix_run;
	}

	if (rctx->surf->get_alpha) {
		u32 i;
		GF_EVGSurface *surf = rctx->surf;
		EVG_Texture *_p = (EVG_Texture *)p;
		if (_p->Bpp>8) {
			u64 *coll = (u64 *)rctx->stencil_pix_run;
			for (i=0; i<span->len; i++) {
				u64 a = (*coll>>48)&0xFFFF;
				a = 0xFF * surf->get_alpha(surf->get_alpha_udta, (u8) (a/0xFF), span->x+i, y);
				*coll = (a<<48) | ((*coll) & 0x0000FFFFFFFFFFFFUL);
				coll ++;
			}
		} else {
			u32 *col = (u32 *)rctx->stencil_pix_run;
			for (i=0; i<span->len; i++) {
				u32 a = GF_COL_A(*col);
				a = surf->get_alpha(surf->get_alpha_udta, a, span->len+i, y);
				*col = (a<<24) | ((*col) & 0x00FFFFFF);
				col ++;
			}
		}
	}
	return rctx->stencil_pix_run;
}
void *evg_fill_run_mask(GF_EVGStencil *p, EVGRasterCtx *rctx, EVG_Span *span, s32 y)
{
	void *res = evg_fill_run(p, rctx, span, y);
	u8 *mask = rctx->surf->internal_mask + y*rctx->surf->width + span->x;
	u32 i;
	if (rctx->surf->not_8bits) {
		u64 *wcols = res;
		for (i=0; i<span->len; i++) {
			*wcols = (*wcols & 0x0000FFFFFFFFFFFFUL) | (((u64) *mask)*256)<<48;
			wcols++;
			mask++;
		}
	} else {
		u32 * cols = res;
		for (i=0; i<span->len; i++) {
			*cols = (*cols & 0x00FFFFFF) | ((u32)*mask)<<24;
			cols++;
			mask++;
		}
	}
	return res;
}
void *evg_fill_run_mask_inv(GF_EVGStencil *p, EVGRasterCtx *rctx, EVG_Span *span, s32 y)
{
	void *res = evg_fill_run(p, rctx, span, y);
	u8 *mask = rctx->surf->internal_mask + y*rctx->surf->width + span->x;
	u32 i;
	if (rctx->surf->not_8bits) {
		u64 *wcols = res;
		for (i=0; i<span->len; i++) {
			*wcols = (*wcols & 0x0000FFFFFFFFFFFFUL) | (((u64) (0xFF- *mask))*256)<<48;
			wcols++;
			mask++;
		}
	} else {
		u32 * cols = res;
		for (i=0; i<span->len; i++) {
			*cols = (*cols & 0x00FFFFFF) | ((u32) (0xFF- *mask) )<<24;
			cols++;
			mask++;
		}
	}
	return res;
}

void evg_fill_span_mask(int y, int count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	EVG_Span aspan, *ospan;
	int i;
	aspan.idx1 = aspan.idx2 = 0;

	//disable yuv flush for 420 and 422
	rctx->no_yuv_flush = 1;
	for (i=0; i<count; i++) {
		u32 len, o_x;
		ospan = &spans[i];

		o_x = ospan->x;
		len = ospan->len;
		aspan.len = 0;
		u8 *mask = surf->internal_mask + surf->width * y + o_x;
		while (len) {
			u32 mv = *mask;

			if (mv) {
				if (!aspan.len) {
					aspan.x = o_x;
					aspan.coverage = ospan->coverage;
					aspan.odd_flag = ospan->odd_flag;
				}
				aspan.len++;
				if (mv == 0xFF)
					*mask = 0xFF - aspan.coverage;
				else {
					*mask = 0;
					aspan.coverage = 0xFF;
				}
			} else if (aspan.len) {
				surf->fill_spans(y, 1, &aspan, surf, rctx);
				aspan.len = 0;
			}
			mask++;
			o_x++;
			len--;
		}
		//flush
		if (aspan.len) {
			//last span, enable yuv flush for 420 and 422
			//we can do it now because we know we get called only once per y for YUV 420/422
			if (i+1==count)
				rctx->no_yuv_flush = 0;

			surf->fill_spans(y, 1, &aspan, surf, rctx);
		}
	}
}


#define mix_run_func(_type, _val, _shift, _R, _G, _B, _ARGB) \
	u32 r1, g1, b1, r2, g2, b2; \
	u32 mix = rctx->surf->mix_val; \
	u32 imix = _val - mix; \
	u32 i=0; \
	_type col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2; \
	if (!mix) return; \
	if (!imix) return;\
	\
	while (i<count) { \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		r1 =  _R(col1); \
		g1 =  _G(col1); \
		b1 =  _B(col1); \
		r2 =  _R(col2); \
		g2 =  _G(col2); \
		b2 =  _B(col2); \
		r1 = (r1 * imix + r2 * mix) >> _shift; \
		g1 = (g1 * imix + g2 * mix) >> _shift; \
		b1 = (b1 * imix + b2 * mix) >> _shift; \
		col1p[i] = _ARGB(_val, r1, g1, b1); \
		i++; \
	} \

static void mix_run(EVGRasterCtx *rctx, u32 count)
{
	mix_run_func(u32, 0xFF, 8, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mix_run_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_run_func(u64, 0xFFFF, 16, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}


#define mixa_run_func(_type, _val, _shift, _A, _R, _G, _B, _ARGB) \
	u32 a1, r1, g1, b1, a2, r2, g2, b2; \
	u32 mix = rctx->surf->mix_val; \
	u32 imix = _val - mix; \
	u32 i=0; \
	_type col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2; \
	if (!mix) return; \
	if (!imix) return; \
 \
	while (i<count) { \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		a1 =  _A(col1); \
		r1 =  _R(col1); \
		g1 =  _G(col1); \
		b1 =  _B(col1); \
		a2 =  _A(col2); \
		r2 =  _R(col2); \
		g2 =  _G(col2); \
		b2 =  _B(col2); \
		a1 = (a1 * imix + a2 * mix) >> _shift; \
		r1 = (r1 * imix + r2 * mix) >> _shift; \
		g1 = (g1 * imix + g2 * mix) >> _shift; \
		b1 = (b1 * imix + b2 * mix) >> _shift; \
		col1p[i] = _ARGB(a1, r1, g1, b1); \
		i++; \
	} \

static void mixa_run(EVGRasterCtx *rctx, u32 count)
{
	mixa_run_func(u32, 0xFF, 8, GF_COL_A, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mixa_run_wide(EVGRasterCtx *rctx, u32 count)
{
	mixa_run_func(u64, 0xFFFF, 16, GF_COLW_A, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}


#define repa_run_func(_type, _shift, _mask, _A) \
	u32 i=0; \
	_type a2, col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2; \
 \
	while (i<count) { \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		a2 = _A(col2);\
		col1p[i] = (a2<<_shift) | (col1 & _mask);\
		i++; \
	} \

static void replace_alpha_run_a(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u32, 24, 0x00FFFFFF, GF_COL_A)
}
static void replace_alpha_run_a_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u64, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_A)
}
static void replace_alpha_run_r(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u32, 24, 0x00FFFFFF, GF_COL_R)
}
static void replace_alpha_run_r_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u64, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_R)
}
static void replace_alpha_run_g(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u32, 24, 0x00FFFFFF, GF_COL_G)
}
static void replace_alpha_run_g_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u64, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_G)
}
static void replace_alpha_run_b(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u32, 24, 0x00FFFFFF, GF_COL_B)
}
static void replace_alpha_run_b_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_run_func(u64, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_B)
}


#define repa_m1_run_func(_type, _val, _shift, _mask, _A) \
	u32 i=0; \
	_type a2, col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2; \
 \
	while (i<count) { \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		a2 = _val - _A(col2);\
		col1p[i] = (a2<<_shift) | (col1 & _mask);\
		i++; \
	} \

static void replace_alpha_m1_run_a(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u32, 0xFF, 24, 0x00FFFFFF, GF_COL_A)
}
static void replace_alpha_m1_run_a_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u64, 0xFFFF, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_A)
}
static void replace_alpha_m1_run_r(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u32, 0xFF, 24, 0x00FFFFFF, GF_COL_R)
}
static void replace_alpha_m1_run_r_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u64, 0xFFFF, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_R)
}
static void replace_alpha_m1_run_g(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u32, 0xFF, 24, 0x00FFFFFF, GF_COL_G)
}
static void replace_alpha_m1_run_g_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u64, 0xFFFF, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_G)
}
static void replace_alpha_m1_run_b(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u32, 0xFF, 24, 0x00FFFFFF, GF_COL_B)
}
static void replace_alpha_m1_run_b_wide(EVGRasterCtx *rctx, u32 count)
{
	repa_m1_run_func(u64, 0xFFFF, 56, 0x0000FFFFFFFFFFFFUL, GF_COLW_B)
}


#define mix_dyn_run_func(_type, _val, _shift, _A, _R, _G, _B, _ARGB) \
	u32 r1, g1, b1, r2, g2, b2; \
	u32 i=0; \
	_type col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2, *col3p = rctx->stencil_pix_run3; \
 \
	while (i<count) { \
		u32 mix = _A(col3p[i]); \
		u32 imix = _val - mix; \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		r1 =  _R(col1); \
		g1 =  _G(col1); \
		b1 =  _B(col1); \
		r2 =  _R(col2); \
		g2 =  _G(col2); \
		b2 =  _B(col2); \
		r1 = (r1 * imix + r2 * mix) >> _shift; \
		g1 = (g1 * imix + g2 * mix) >> _shift; \
		b1 = (b1 * imix + b2 * mix) >> _shift; \
		col1p[i] = _ARGB(_val, r1, g1, b1); \
		i++; \
	} \

static void mix_dyn_run_a(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u32, 0xFF, 8, GF_COL_A, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mix_dyn_run_a_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u64, 0xFFFF, 16, GF_COLW_A, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}

static void mix_dyn_run_r(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u32, 0xFF, 8, GF_COL_R, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mix_dyn_run_r_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u64, 0xFFFF, 16, GF_COLW_R, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}

static void mix_dyn_run_g(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u32, 0xFF, 8, GF_COL_G, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mix_dyn_run_g_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u64, 0xFFFF, 16, GF_COLW_G, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}
static void mix_dyn_run_b(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u32, 0xFF, 8, GF_COL_B, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}

static void mix_dyn_run_b_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyn_run_func(u64, 0xFFFF, 16, GF_COLW_B, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}


#define mix_dyna_run_func(_type, _val, _shift, _A, _R, _G, _B, _ARGB) \
	u32 a1, r1, g1, b1, a2, r2, g2, b2; \
	u32 i=0; \
	_type col1, col2, *col1p = rctx->stencil_pix_run, *col2p = rctx->stencil_pix_run2, *col3p = rctx->stencil_pix_run3; \
 \
	while (i<count) { \
		u32 mix = _A(col3p[i]); \
		u32 imix = _val - mix; \
		col1 = col1p[i]; \
		col2 = col2p[i]; \
		a1 =  _A(col1); \
		r1 =  _R(col1); \
		g1 =  _G(col1); \
		b1 =  _B(col1); \
		a2 =  _A(col2); \
		r2 =  _R(col2); \
		g2 =  _G(col2); \
		b2 =  _B(col2); \
		a1 = (a1 * imix + a2 * mix) >> _shift; \
		r1 = (r1 * imix + r2 * mix) >> _shift; \
		g1 = (g1 * imix + g2 * mix) >> _shift; \
		b1 = (b1 * imix + b2 * mix) >> _shift; \
		col1p[i] = _ARGB(a1, r1, g1, b1); \
		i++; \
	} \

static void mix_dyna_run_a(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u32, 0xFF, 8, GF_COL_A, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}
static void mix_dyna_run_a_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u64, 0xFFFF, 16, GF_COLW_A, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}
static void mix_dyna_run_r(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u32, 0xFF, 8, GF_COL_R, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}
static void mix_dyna_run_r_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u64, 0xFFFF, 16, GF_COLW_R, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}
static void mix_dyna_run_g(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u32, 0xFF, 8, GF_COL_G, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}
static void mix_dyna_run_g_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u64, 0xFFFF, 16, GF_COLW_G, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}
static void mix_dyna_run_b(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u32, 0xFF, 8, GF_COL_B, GF_COL_R, GF_COL_G, GF_COL_B, GF_COL_ARGB)
}
static void mix_dyna_run_b_wide(EVGRasterCtx *rctx, u32 count)
{
	mix_dyna_run_func(u64, 0xFFFF, 16, GF_COLW_B, GF_COLW_R, GF_COLW_G, GF_COLW_B, GF_COLW_ARGB)
}


GF_Err gf_evg_setup_multi_texture(GF_EVGSurface *surf, GF_EVGMultiTextureMode operand, GF_EVGStencil *sten2, GF_EVGStencil *sten3, Float *params)
{
	Float param1 = params ? params[0] : 0;
	surf->sten2 = surf->sten3 = NULL;
	surf->odd_fill = 0;

	switch (operand) {
	case GF_EVG_OPERAND_MIX:
		if (!sten2 || !params) return GF_BAD_PARAM;
		surf->sten2 = sten2;
		if (surf->not_8bits) {
			surf->mix_val = (u32) (params[0] * 0xFFFF);
			surf->update_run = mix_run_wide;
			if (!surf->mix_val) surf->sten2 = NULL;
			else if (surf->mix_val==0xFFFF) {
				surf->sten2 = NULL;
				surf->sten = sten2;
			}
		} else {
			surf->mix_val = (u32) (params[0] * 255);
			surf->update_run = mix_run;
			if (!surf->mix_val) surf->sten2 = NULL;
			else if (surf->mix_val==0xFF) {
				surf->sten2 = NULL;
				surf->sten = sten2;
			}
		}
		break;
	case GF_EVG_OPERAND_MIX_ALPHA:
		if (!sten2 || !params) return GF_BAD_PARAM;
		surf->sten2 = sten2;
		if (surf->not_8bits) {
			surf->mix_val = (u32) (params[0] * 0xFFFF);
			surf->update_run = mixa_run_wide;
			if (!surf->mix_val) surf->sten2 = NULL;
			else if (surf->mix_val==0xFFFF) {
				surf->sten2 = NULL;
				surf->sten = sten2;
			}
		} else {
			surf->mix_val = (u32) (params[0] * 255);
			surf->update_run = mixa_run;
			if (!surf->mix_val) surf->sten2 = NULL;
			else if (surf->mix_val==0xFF) {
				surf->sten2 = NULL;
				surf->sten = sten2;
			}
		}
		break;
	case GF_EVG_OPERAND_REPLACE_ALPHA:
		if (!sten2) return GF_BAD_PARAM;
		if (param1>=3)
			surf->update_run = surf->not_8bits ? replace_alpha_run_b_wide : replace_alpha_run_b;
		else if (param1>=2)
			surf->update_run = surf->not_8bits ? replace_alpha_run_g_wide : replace_alpha_run_g;
		else if (param1>=1)
			surf->update_run = surf->not_8bits ? replace_alpha_run_r_wide : replace_alpha_run_r;
		else
			surf->update_run = surf->not_8bits ? replace_alpha_run_a_wide : replace_alpha_run_a;
		surf->sten2 = sten2;
		break;
	case GF_EVG_OPERAND_REPLACE_ONE_MINUS_ALPHA:
		if (!sten2) return GF_BAD_PARAM;
		if (param1>=3)
			surf->update_run = surf->not_8bits ? replace_alpha_m1_run_b_wide : replace_alpha_m1_run_b;
		else if (param1>=2)
			surf->update_run = surf->not_8bits ? replace_alpha_m1_run_g_wide : replace_alpha_m1_run_g;
		else if (param1>=1)
			surf->update_run = surf->not_8bits ? replace_alpha_m1_run_r_wide : replace_alpha_m1_run_r;
		else
			surf->update_run = surf->not_8bits ? replace_alpha_m1_run_a_wide : replace_alpha_m1_run_a;
		surf->sten2 = sten2;
		break;
	case GF_EVG_OPERAND_MIX_DYN:
		if (!sten2 || !sten3) return GF_BAD_PARAM;
		if (param1>=3)
			surf->update_run = surf->not_8bits ? mix_dyn_run_b_wide : mix_dyn_run_b;
		else if (param1>=2)
			surf->update_run = surf->not_8bits ? mix_dyn_run_g_wide : mix_dyn_run_g;
		else if (param1>=1)
			surf->update_run = surf->not_8bits ? mix_dyn_run_r_wide : mix_dyn_run_r;
		else
			surf->update_run = surf->not_8bits ? mix_dyn_run_a_wide : mix_dyn_run_a;
		surf->sten2 = sten2;
		surf->sten3 = sten3;
		break;
	case GF_EVG_OPERAND_MIX_DYN_ALPHA:
		if (!sten2 || !sten3 || !params) return GF_BAD_PARAM;
		if (param1>=3)
			surf->update_run = surf->not_8bits ? mix_dyna_run_b_wide : mix_dyna_run_b;
		else if (param1>=2)
			surf->update_run = surf->not_8bits ? mix_dyna_run_g_wide : mix_dyna_run_g;
		else if (param1>=1)
			surf->update_run = surf->not_8bits ? mix_dyna_run_r_wide : mix_dyna_run_r;
		else
			surf->update_run = surf->not_8bits ? mix_dyna_run_a_wide : mix_dyna_run_a;

		surf->sten2 = sten2;
		surf->sten3 = sten3;
		break;
	case GF_EVG_OPERAND_ODD_FILL:
		if (!sten2) return GF_BAD_PARAM;
		surf->sten2 = sten2;
		surf->odd_fill = 1;
		break;

	default:
		return GF_BAD_PARAM;
	}


	surf->run_size = sizeof(u32) * (surf->width+2);
	if (surf->not_8bits) surf->run_size *= 2;

	if (surf->sten2 && !surf->raster_ctx.stencil_pix_run2) {
		surf->raster_ctx.stencil_pix_run2 = gf_malloc(sizeof(u8) * surf->run_size);
		if (!surf->raster_ctx.stencil_pix_run2) return GF_OUT_OF_MEM;
	}
	if (surf->sten3 && !surf->raster_ctx.stencil_pix_run3) {
		surf->raster_ctx.stencil_pix_run3 = gf_malloc(sizeof(u8) * surf->run_size);
		if (!surf->raster_ctx.stencil_pix_run3) return GF_OUT_OF_MEM;
	}

#ifndef GPAC_DISABLE_THREADS
	u32 i;
	for (i=0; i<surf->nb_threads; i++) {
		EVGRasterCtx *rctx = &surf->th_raster_ctx[i];
		if (surf->sten2 && !rctx->stencil_pix_run2) {
			rctx->stencil_pix_run2 = gf_malloc(sizeof(u8) * surf->run_size);
			if (!rctx->stencil_pix_run2) return GF_OUT_OF_MEM;
		}
		if (surf->sten3 && !rctx->stencil_pix_run3) {
			rctx->stencil_pix_run3 = gf_malloc(sizeof(u8) * surf->run_size);
			if (!rctx->stencil_pix_run3) return GF_OUT_OF_MEM;
		}
	}
#endif
	return GF_OK;
}

#endif //GPAC_DISABLE_EVG
