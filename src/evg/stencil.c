/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

static GF_EVGStencil *evg_solid_brush();
static GF_EVGStencil *evg_texture_brush();
static GF_EVGStencil *evg_linear_gradient_brush();
static GF_EVGStencil *evg_radial_gradient_brush();

u64 evg_make_col_wide(u16 a, u16 r, u16 g, u16 b)
{
	u64 res;
	res = a;
	res <<= 16;
	res |= r;
	res <<= 16;
	res |= g;
	res <<= 16;
	res |= b;
	return res;
}

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
				start = FIX2INT(gf_mulfix(_this->pos[c], maxPos));
				end = FIX2INT(gf_mulfix(_this->pos[c+1], maxPos));
				diff = end-start;

				if (diff) {
					for (i=start; i<=end; i++) {
						_this->precomputed_argb[i] = color_interpolate(_this->col[c], _this->col[c+1],
						                                  (u8) ( ( (i-start) * 255) / diff) );
					}
				}
			} else {
				start = FIX2INT(gf_mulfix(_this->pos[c+0], maxPos));
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
	memcpy(_this->col, col, sizeof(GF_Color) * count);
	memcpy(_this->pos, pos, sizeof(Fixed) * count);
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
		return 0L;
	}
	if (st) {
		gf_mx2d_init(st->pmat);
		gf_mx2d_init(st->smat);
		gf_cmx_init(&st->cmat);
	}
	return st;
}

GF_EXPORT
void gf_evg_stencil_delete(GF_EVGStencil * st)
{
	GF_EVGStencil *_this = (GF_EVGStencil *) st;
	switch(_this->type) {
	case GF_STENCIL_SOLID:
	case GF_STENCIL_LINEAR_GRADIENT:
	case GF_STENCIL_RADIAL_GRADIENT:
		gf_free(_this);
		return;
	case GF_STENCIL_TEXTURE:
	{
		EVG_Texture *tx = (EVG_Texture *)_this;
		/*destroy local texture iof any*/
		if (tx->owns_texture && tx->pixels) gf_free(tx->pixels);
		gf_free(_this);
	}
	return;
	}
}

GF_EXPORT
GF_Err gf_evg_stencil_set_matrix(GF_EVGStencil * st, GF_Matrix2D *mx)
{
	GF_EVGStencil *_this = (GF_EVGStencil *)st;
	if (!_this || _this->type>GF_STENCIL_TEXTURE) return GF_BAD_PARAM;
	if (mx) {
		gf_mx2d_copy(_this->smat, *mx);
	} else {
		gf_mx2d_init(_this->smat);
	}
	return GF_OK;
}


/*
	Solid color stencil
*/

static GF_EVGStencil *evg_solid_brush()
{
	EVG_Brush *tmp;
	GF_SAFEALLOC(tmp, EVG_Brush);
	if (!tmp) return 0L;
	tmp->fill_run = NULL;
	tmp->color = 0xFF000000;
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


/*
	linear gradient stencil
*/

static void lg_fill_run(GF_EVGStencil *p, GF_EVGSurface *surf, s32 x, s32 y, u32 count)
{
	Fixed _res;
	s32 val;
	u32 col;
	u32 *data = surf->stencil_pix_run;
	u64 *data_wide = surf->not_8bits ? surf->stencil_pix_run : NULL;
	EVG_LinearGradient *_this = (EVG_LinearGradient *) p;

	/*no need to move x & y to fixed*/
	_res = (Fixed) (x * _this->smat.m[0] + y * _this->smat.m[1] + _this->smat.m[2]);
	while (count) {
		val = FIX2INT(_res);
		_res += _this->smat.m[0];

		col = gradient_get_color((EVG_BaseGradient *)_this, val );
		if (data_wide) *data_wide++ = evg_col_to_wide(col);
		else *data++ = col;

		count--;
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

static void rg_fill_run(GF_EVGStencil *p, GF_EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	Fixed x, y, dx, dy, b, val;
	s32 pos;
	u32 col;
	u32 *data = surf->stencil_pix_run;
	u64 *data_wide = surf->not_8bits ? surf->stencil_pix_run : NULL;
	EVG_RadialGradient *_this = (EVG_RadialGradient *) p;

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

	do_yuv = GF_FALSE;

	if (surf->is_yuv) {
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

	do_yuv = surf->is_yuv;

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
			argb = evg_argb_to_ayuv(surf, argb);

		grad->precomputed_dest[i] = argb;
	}
}


/*
	Texture stencil
	 	FIXME: add filtering , check bilinear
*/

#define USE_BILINEAR	0

#if USE_BILINEAR
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
static u64 EVG_LERP(u64 c0, u64 c1, u8 t)
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

	return evg_make_col_wide(a2, r2, g2, b2);
}
#endif

static void tex_untransform_coord(EVG_Texture *_this, s32 _x, s32 _y, Fixed *outx, Fixed *outy)
{
	u32 checkx, checky;
	Fixed x, y, dim;

	/* reverse to texture coords*/
	x = INT2FIX(_x);
	y = INT2FIX(_y);
	gf_mx2d_apply_coords(&_this->smat, &x, &y);

	checkx = checky = 0;
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

	dim = INT2FIX(_this->width);
	if (_this->mod & GF_TEXTURE_REPEAT_S) {
		if (x<0) {
			while (x<0) x += dim;
		} else {
			while (x>dim) x -= dim;
		}
	} else {
		if (x<-dim) {
			x = 0;
		} else if (x>dim) {
			x = dim;
		}
		while (x<0) x+=dim;
	}

	dim = INT2FIX(_this->height);
	if (_this->mod & GF_TEXTURE_REPEAT_T) {
		if (y<0) {
			while (y<0) y += dim;
		} else {
			while (y>dim) y -= dim;
		}
	} else {
		if (y<-dim) {
			y = 0;
		} else if (y>dim) {
			y = dim;
		}
		while (y<0) y+=dim;
	}

	*outx=x;
	*outy=y;
}

static void tex_fill_run(GF_EVGStencil *p, GF_EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	s32 cx, x0, y0;
	u32 pix, replace_col;
	Bool has_alpha, has_replace_cmat, has_cmat, repeat_s, repeat_t;
	Fixed x, y, _fd;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	u32 *data = surf->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	tex_untransform_coord(_this, _x, _y, &x, &y);

#if USE_BILINEAR
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	_fd = INT2FIX(_this->width);
	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	if (!repeat_s && (x < - _fd)) x = 0;
	while (x<0) x += _fd;

	_fd = INT2FIX(_this->height);
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;
	if (!repeat_t && (y < - _fd)) y = 0;
	while (y<0) y += _fd;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_replace_cmat = _this->cmat_is_replace ? GF_TRUE : GF_FALSE;
	if (has_replace_cmat) has_cmat = GF_FALSE;
	else has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	replace_col = _this->replace_col;

	while (count) {
		x0 = FIX2INT(x);
		assert((s32)x0 >=0);
		if (repeat_s) {
			x0 = (x0) % _this->width;
		} else {
			x0 = MIN(x0, (s32) _this->width - 1);
		}

		y0 = FIX2INT(y);
		assert((s32)y0 >=0);
		if (repeat_t) {
			y0 = (y0) % _this->height;
		} else if (y0 >= (s32) _this->height) {
			y0 = _this->height-1;
		}

		pix = _this->tx_get_pixel(_this, x0, y0);

		_x++;
		tex_untransform_coord(_this, _x, _y, &x, &y);

		if (x<0) x+=INT2FIX(_this->width);
		if (y<0) y+=INT2FIX(_this->height);

		/*bilinear filtering - disabled (too slow and not precise enough)*/
#if USE_BILINEAR
		if (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) {
			u32 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;

			tx = FIX2INT(gf_muldiv(x, 255, _this->width) );
			ty = FIX2INT(gf_muldiv(y, 255, _this->height) );

			if (tx>120 || ty>120) {
				x1 = (x0+incx);
				if (x1<0) {
					while (x1<0) x1 += _this->width;
				} else {
					x1 = x1 % _this->width;
				}
				y1 = (y0+incy);
				if (y1<0) {
					while (y1<0) y1+=_this->height;
				} else {
					y1 = y1 % _this->height;
				}
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
				p01 = _this->tx_get_pixel(_this, x1, y0);
				p10 = _this->tx_get_pixel(_this, x0, y1);
				p11 = _this->tx_get_pixel(_this, x1, y1);

				p00 = EVG_LERP(p00, p01, tx);
				p10 = EVG_LERP(p10, p11, tx);
				pix = EVG_LERP(p00, p10, ty);
			}
		}
#endif

		if (has_alpha) {
			cx = ((GF_COL_A(pix) + 1) * _this->alpha) >> 8;
			pix = ( ((cx<<24) & 0xFF000000) ) | (pix & 0x00FFFFFF);
		}
		if (has_replace_cmat) {
			u32 __a;
			__a = GF_COL_A(pix);
			__a = (u32) (_this->cmat.m[18] * __a);
			//replace col is in target pixel format
			pix = ((__a<<24) | (replace_col & 0x00FFFFFF));
		}
		//move pixel to target pixel format, applying color transform matrix
		else if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!surf->is_yuv) {
				pix = evg_ayuv_to_argb(surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), evg_argb_to_ayuv
				pix = gf_cmx_apply(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else if (!_this->is_yuv) {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply(&_this->cmat, pix);

			//dest is yuv, transform
			if (surf->is_yuv)
				pix = evg_argb_to_ayuv(surf, pix);
		}

		*data++ = pix;
		count--;
	}
}


/*just a little faster...*/
static void tex_fill_run_straight(GF_EVGStencil *p, GF_EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0;
	u32 pix;
	u32 __a;
	Bool repeat_s = GF_FALSE;
	Fixed x, y, _fdim;
	u32 *data = surf->stencil_pix_run;
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
	_fdim = INT2FIX(_this->width);
	repeat_s = (_this->mod & GF_TEXTURE_REPEAT_S);
	if (!repeat_s && (x <- _fdim)) x=0;
	while (x<0) x += _fdim;

	_fdim = INT2FIX(_this->height);
	if (!(_this->mod & GF_TEXTURE_REPEAT_T) && (y <- _fdim)) y = 0;
	while (y<0) y += _fdim;

	y0 = FIX2INT(y);
	y0 = y0 % _this->height;
	
	while (count) {
		x0 = FIX2INT(x);
		if (repeat_s) {
			x0 = (x0) % _this->width;
		} else if (x0 >= (s32) _this->width) x0 = _this->width-1;

		x += _this->inc_x;
		pix = _this->tx_get_pixel(_this, x0, y0);

		//replace_col is in destination format
		if (_this->replace_col) {
			__a = GF_COL_A(pix);
			pix = ((__a<<24) | (_this->replace_col & 0x00FFFFFF));
		}
		//move pixel to target pixel format
		else if (_this->is_yuv && !surf->is_yuv) {
			pix = evg_ayuv_to_argb(surf, pix);
		}
		else if (!_this->is_yuv && surf->is_yuv) {
			pix = evg_argb_to_ayuv(surf, pix);
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
	return evg_make_col_wide(a, r, g, b);
}

static void tex_fill_run_wide(GF_EVGStencil *p, GF_EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0;
	u64 pix, replace_col;
	Bool has_alpha, has_replace_cmat, has_cmat, repeat_s, repeat_t;
	Fixed x, y, _fd;
#if USE_BILINEAR
	s32 incx, incy;
#endif
	u64 *data = surf->stencil_pix_run;
	EVG_Texture *_this = (EVG_Texture *) p;

	tex_untransform_coord(_this, _x, _y, &x, &y);

#if USE_BILINEAR
	incx = (_this->inc_x>0) ? 1 : -1;
	incy = (_this->inc_y>0) ? 1 : -1;
#endif

	_fd = INT2FIX(_this->width);
	repeat_s = _this->mod & GF_TEXTURE_REPEAT_S;
	if (!repeat_s && (x < - _fd)) x = 0;
	while (x<0) x += _fd;

	_fd = INT2FIX(_this->height);
	repeat_t = _this->mod & GF_TEXTURE_REPEAT_T;
	if (!repeat_t && (y < - _fd)) y = 0;
	while (y<0) y += _fd;

	has_alpha = (_this->alpha != 255) ? GF_TRUE : GF_FALSE;
	has_replace_cmat = _this->cmat_is_replace ? GF_TRUE : GF_FALSE;
	if (has_replace_cmat) has_cmat = GF_FALSE;
	else has_cmat = _this->cmat.identity ? GF_FALSE : GF_TRUE;

	replace_col = evg_col_to_wide(_this->replace_col);

	while (count) {
		x0 = FIX2INT(x);
		assert((s32)x0 >=0);
		if (repeat_s) {
			x0 = (x0) % _this->width;
		} else {
			x0 = MIN(x0, (s32) _this->width - 1);
		}

		y0 = FIX2INT(y);
		assert((s32)y0 >=0);
		if (repeat_t) {
			y0 = (y0) % _this->height;
		} else if (y0 >= (s32) _this->height) {
			y0 = _this->height-1;
		}

		if (_this->tx_get_pixel_wide) {
			pix = _this->tx_get_pixel_wide(_this, x0, y0);
		} else {
			pix = evg_col_to_wide( _this->tx_get_pixel(_this, x0, y0) );
		}
		_x++;
		tex_untransform_coord(_this, _x, _y, &x, &y);

		if (x<0) x+=INT2FIX(_this->width);
		if (y<0) y+=INT2FIX(_this->height);

		/*bilinear filtering - disabled (too slow and not precise enough)*/
#if USE_BILINEAR
		if (_this->filter==GF_TEXTURE_FILTER_HIGH_QUALITY) {
			u64 p00, p01, p10, p11;
			s32 x1, y1;
			u8 tx, ty;

			tx = FIX2INT(gf_muldiv(x, 255, _this->width) );
			ty = FIX2INT(gf_muldiv(y, 255, _this->height) );

			if (tx>120 || ty>120) {
				x1 = (x0+incx);
				if (x1<0) {
					while (x1<0) x1 += _this->width;
				} else {
					x1 = x1 % _this->width;
				}
				y1 = (y0+incy);
				if (y1<0) {
					while (y1<0) y1+=_this->height;
				} else {
					y1 = y1 % _this->height;
				}
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
				if (_this->tx_get_pixel_wide) {
					p01 = _this->tx_get_pixel_wide(_this, x1, y0);
					p10 = _this->tx_get_pixel_wide(_this, x0, y1);
					p11 = _this->tx_get_pixel_wide(_this, x1, y1);
				} else {
					p01 = evg_col_to_wide(_this->tx_get_pixel(_this, x1, y0) );
					p10 = evg_col_to_wide(_this->tx_get_pixel(_this, x0, y1) );
					p11 = evg_col_to_wide(_this->tx_get_pixel(_this, x1, y1) );
				}
				p00 = EVG_LERP_WIDE(p00, p01, tx);
				p10 = EVG_LERP_WIDE(p10, p11, tx);
				pix = EVG_LERP_WIDE(p00, p10, ty);
			}
		}
#endif

		if (has_alpha) {
			u64 _a = (pix>>48)&0xFF;
			_a = (_a * _this->alpha) >> 8;
			_a<<=48;
			pix = ( (_a & 0xFFFF000000000000UL) ) | (pix & 0x0000FFFFFFFFFFFFUL);
		}
		if (has_replace_cmat) {
			u64 _a = (pix>>48)&0xFF;
			_a = (_a * _this->alpha) >> 8;
			_a = (u64) (_this->cmat.m[18] * _a);
			_a<<=48;
			pix = ( (_a & 0xFFFF000000000000UL) ) | (replace_col & 0x0000FFFFFFFFFFFFUL);
		}
		//move pixel to target pixel format, applying color transform matrix
		else if (_this->is_yuv) {
			//if surf is rgb, transform
			if (!surf->is_yuv) {
				pix = evg_ayuv_to_argb_wide(surf, pix);
				//apply cmat
				if (has_cmat)
					pix = gf_cmx_apply_wide(&_this->cmat, pix);
			} else if (has_cmat) {
				//yuv->yuv , use color matrix in yuv domain
				//this is equivalent to  evg_ayuv_to_argb, gf_cmx_apply(&_this->cmat, pix), evg_argb_to_ayuv
				pix = gf_cmx_apply_wide(&_this->yuv_cmat, pix);
			}
		}
		//texture is RGB
		else if (!_this->is_yuv) {
			//apply cmat
			if (has_cmat)
				pix = gf_cmx_apply_wide(&_this->cmat, pix);

			//dest is yuv, transform
			if (surf->is_yuv)
				pix = evg_argb_to_ayuv_wide(surf, pix);
		}

		*data++ = pix;
		count--;
	}
}


/*just a little faster...*/
static void tex_fill_run_straight_wide(GF_EVGStencil *p, GF_EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0;
	u64 pix;
	Bool repeat_s = GF_FALSE;
	Fixed x, y, _fdim;
	u64 *data = surf->stencil_pix_run;
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
	_fdim = INT2FIX(_this->width);
	repeat_s = (_this->mod & GF_TEXTURE_REPEAT_S);
	if (!repeat_s && (x <- _fdim)) x=0;
	while (x<0) x += _fdim;

	_fdim = INT2FIX(_this->height);
	if (!(_this->mod & GF_TEXTURE_REPEAT_T) && (y <- _fdim)) y = 0;
	while (y<0) y += _fdim;

	y0 = FIX2INT(y);
	y0 = y0 % _this->height;

	while (count) {
		x0 = FIX2INT(x);
		if (repeat_s) {
			x0 = (x0) % _this->width;
		} else if (x0 >= (s32) _this->width) x0 = _this->width-1;

		x += _this->inc_x;

		if (_this->tx_get_pixel_wide) {
			pix = _this->tx_get_pixel_wide(_this, x0, y0);
		} else {
			pix = evg_col_to_wide( _this->tx_get_pixel(_this, x0, y0) );
		}

		//replace_col is in destination format
		if (_this->replace_col) {
			u64 _a = (pix>>48)&0xFF;
			_a = (_a * _this->alpha) >> 8;
			_a<<=48;
			pix = ( (_a & 0xFFFF000000000000UL) ) | (_this->replace_col & 0x0000FFFFFFFFFFFFUL);
		}
		//move pixel to target pixel format
		else if (_this->is_yuv && !surf->is_yuv) {
			pix = evg_ayuv_to_argb_wide(surf, pix);
		}
		else if (!_this->is_yuv && surf->is_yuv) {
			pix = evg_argb_to_ayuv_wide(surf, pix);
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
	gf_evg_stencil_set_filter( (GF_EVGStencil *) tmp, GF_TEXTURE_FILTER_DEFAULT);
	tmp->mod = 0;
	gf_cmx_init(&tmp->cmat);
	tmp->alpha = 255;
	return (GF_EVGStencil *) tmp;
}


u32 get_pix_argb(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF, *(pix+3) & 0xFF);
}
u32 get_pix_rgba(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_abgr(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix) & 0xFF, *(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF);
}
u32 get_pix_bgra(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *(pix) & 0xFF);
}
u32 get_pix_rgbx(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix) & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_xrgb(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF, *(pix+3) & 0xFF);
}
u32 get_pix_xbgr(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF);
}
u32 get_pix_bgrx(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *(pix) & 0xFF);
}
u32 get_pix_rgb_24(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *pix & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_bgr_24(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, * (pix+1) & 0xFF, *pix & 0xFF);
}
u32 get_pix_444(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = pix[0]&0x0f;
	u32 g = (pix[1]>>4)&0x0f;
	u32 b = pix[1]&0x0f;
	return GF_COL_ARGB(0xFF, (r << 4), (g << 4), (b << 4));
}
u32 get_pix_555(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = (pix[0]>>2) & 0x1f;
	u32 g = (pix[0])&0x3;
	g<<=3;
	g |= (pix[1]>>5) & 0x7;
	u32 b = pix[1] & 0x1f;
	return GF_COL_ARGB(0xFF, (r << 3), (g << 3), (b << 3));
}
u32 get_pix_565(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u32 r = (pix[0]>>3) & 0x1f;
	u32 g = (pix[0])&0x7;
	g<<=3;
	g |= (pix[1]>>5) & 0x7;
	u32 b = pix[1] & 0x1f;
	return GF_COL_ARGB(0xFF, (r << 3), (g << 2), (b << 3));
}
u32 get_pix_grey(EVG_Texture *_this, u32 x, u32 y)
{
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	u8 val = *pix;
	return GF_COL_ARGB(0xFF, val, val, val);
}
u32 get_pix_alphagrey(EVG_Texture *_this, u32 x, u32 y)
{
	u8 a, g;
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	a = *pix;
	g = *(pix+1);
	return GF_COL_ARGB(a, g, g, g);
}
u32 get_pix_greyalpha(EVG_Texture *_this, u32 x, u32 y)
{
	u8 a, g;
	char *pix = _this->pixels + y * _this->stride + _this->Bpp*x;
	g = *pix;
	a = *(pix+1);
	return GF_COL_ARGB(a, g, g, g);
}
u32 get_pix_yuv420p(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + x/2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}

#ifdef GPAC_BIG_ENDIAN

#define GET_LE_10BIT_AS_8(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) >> 2 )
#define GET_LE_10BIT_AS_16(_ptr) ( (((u16)(_ptr)[1])<<8 | (u16)(_ptr)[0] ) << 6 )

#define GET_BE_10BIT_AS_8(_ptr) ( (*(u16 *)(_ptr)) >> 2 )
#define GET_BE_10BIT_AS_16(_ptr) ( (*(u16 *)(_ptr)) << 6 )

#else

#define GET_LE_10BIT_AS_8(_ptr) ( (*(u16 *)(_ptr)) >> 2 )
#define GET_LE_10BIT_AS_16(_ptr) ( (*(u16 *)(_ptr)) << 6 )

#define GET_BE_10BIT_AS_8(_ptr) ( (((u16)(_ptr)[0])<<8 | (u16)(_ptr)[1] ) >> 2 )
#define GET_BE_10BIT_AS_16(_ptr) ( (((u16)(_ptr)[0])<<8 | (u16)(_ptr)[1] ) << 6 )

#endif


u32 get_pix_yuv420p_10(EVG_Texture *_this, u32 x, u32 y)
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

u64 get_pix_yuv420p_10_wide(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);

	return evg_make_col_wide(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv420p_a(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y/2 * _this->stride/2 + x/2;
	u8 *pA = _this->pix_a  + y * _this->stride + x;

	return GF_COL_ARGB(*pA, *pY, *pU, *pV);
}
u32 get_pix_yuv422p(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride/2 + x/2;
	u8 *pV = _this->pix_v + y * _this->stride/2 + x/2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}
u32 get_pix_yuv422p_10(EVG_Texture *_this, u32 x, u32 y)
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

u64 get_pix_yuv422p_10_wide(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride/2 + (x/2)*2;
	u8 *pV = _this->pix_v + y * _this->stride/2 + (x/2)*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);
	return evg_make_col_wide(0xFFFF, vy, vu, vv);
}

u32 get_pix_yuv444p(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride + x;
	u8 *pV = _this->pix_v + y * _this->stride + x;
	return GF_COL_ARGB(0xFF, *pY, *pU, *pV);
}
u32 get_pix_yuv444p_10(EVG_Texture *_this, u32 x, u32 y)
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
u64 get_pix_yuv444p_10_wide(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y * _this->stride + x*2;
	u8 *pV = _this->pix_v + y * _this->stride + x*2;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pV);
	return evg_make_col_wide(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv444p_a(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y * _this->stride + x;
	u8 *pV = _this->pix_v + y * _this->stride + x;
	u8 *pA = _this->pix_a + y * _this->stride + x;
	return GF_COL_ARGB(*pA, *pY, *pU, *pV);
}
u32 get_pix_yuv_nv12(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*2;
	return GF_COL_ARGB(0xFF, *pY, *pU, *(pU+1));
}

u32 get_pix_yuv_nv12_10(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u  + y/2 * _this->stride + (x/2)*4;
	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pU+2);

	return GF_COL_ARGB(0xFF, vy, vu, vv);
}
u64 get_pix_yuv_nv12_10_wide(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pU+2);

	return evg_make_col_wide(0xFFFF, vy, vu, vv);
}
u32 get_pix_yuv_nv21(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + x;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*2;
	return GF_COL_ARGB(0xFF, *pY, *(pU+1), *pU);
}
u32 get_pix_yuv_nv21_10(EVG_Texture *_this, u32 x, u32 y)
{
	u8 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_8(pY);
	vu = GET_LE_10BIT_AS_8(pU);
	vv = GET_LE_10BIT_AS_8(pU+2);

	return GF_COL_ARGB(0xFF, vy, vv, vu);
}
u64 get_pix_yuv_nv21_10_wide(EVG_Texture *_this, u32 x, u32 y)
{
	u16 vy, vu, vv;
	u8 *pY = _this->pixels + y * _this->stride + x*2;
	u8 *pU = _this->pix_u + y/2 * _this->stride + (x/2)*4;

	vy = GET_LE_10BIT_AS_16(pY);
	vu = GET_LE_10BIT_AS_16(pU);
	vv = GET_LE_10BIT_AS_16(pU+2);
	return evg_make_col_wide(0xFFFF, vy, vv, vu);
}
u32 get_pix_yuyv(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[1];
	u8 v = pY[3];
	u8 luma = (x%2) ? pY[2] : pY[0];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_yvyu(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[3];
	u8 v = pY[1];
	u8 luma = (x%2) ? pY[2] : pY[0];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_uyvy(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[0];
	u8 v = pY[2];
	u8 luma = (x%2) ? pY[3] : pY[1];
	return GF_COL_ARGB(0xFF, luma, u, v);
}
u32 get_pix_vyuy(EVG_Texture *_this, u32 x, u32 y)
{
	u8 *pY = _this->pixels + y * _this->stride + (x/2)*4;
	u8 u = pY[2];
	u8 v = pY[0];
	u8 luma = (x%2) ? pY[3] : pY[1];
	return GF_COL_ARGB(0xFF, luma, u, v);
}

static void texture_set_callbacks(EVG_Texture *_this)
{
	switch (_this->pixel_format) {
	case GF_PIXEL_RGBA:
		_this->tx_get_pixel = get_pix_rgba;
		return;
	case GF_PIXEL_ARGB:
		_this->tx_get_pixel = get_pix_argb;
		return;
	case GF_PIXEL_ABGR:
		_this->tx_get_pixel = get_pix_abgr;
		return;
	case GF_PIXEL_BGRA:
		_this->tx_get_pixel = get_pix_bgra;
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
		_this->tx_get_pixel = get_pix_grey;
		return;
	case GF_PIXEL_ALPHAGREY:
		_this->tx_get_pixel = get_pix_alphagrey;
		return;
	case GF_PIXEL_GREYALPHA:
		_this->tx_get_pixel = get_pix_greyalpha;
		return;
	case GF_PIXEL_YUV:
		_this->tx_get_pixel = get_pix_yuv420p;
		break;
	case GF_PIXEL_YUVA:
		_this->tx_get_pixel = get_pix_yuv420p_a;
		break;
	case GF_PIXEL_YUV422:
		_this->tx_get_pixel = get_pix_yuv422p;
		break;
	case GF_PIXEL_YUV444:
		_this->tx_get_pixel = get_pix_yuv444p;
		break;
	case GF_PIXEL_YUVA444:
		_this->tx_get_pixel = get_pix_yuv444p_a;
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
		break;
	case GF_PIXEL_YUV422_10:
		_this->tx_get_pixel = get_pix_yuv422p_10;
		_this->tx_get_pixel_wide = get_pix_yuv422p_10_wide;
		break;
	case GF_PIXEL_YUV444_10:
		_this->tx_get_pixel = get_pix_yuv444p_10;
		_this->tx_get_pixel_wide = get_pix_yuv444p_10_wide;
		break;
	case GF_PIXEL_NV12_10:
		_this->tx_get_pixel = get_pix_yuv_nv12_10;
		_this->tx_get_pixel_wide = get_pix_yuv_nv12_10_wide;
		break;
	case GF_PIXEL_NV21_10:
		_this->tx_get_pixel = get_pix_yuv_nv21_10;
		_this->tx_get_pixel_wide = get_pix_yuv_nv21_10_wide;
		break;
	default:
		return;
	}
	//assign image planes
	if (_this->pix_u) return;

	switch (_this->pixel_format) {
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV:
		if (!_this->stride_uv) _this->stride_uv = _this->stride/2;
		_this->pix_u = _this->pixels + _this->stride*_this->height;
		_this->pix_v = _this->pix_u + _this->stride_uv * _this->height/2;
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

static GF_Err gf_evg_stencil_set_texture_internal(GF_EVGStencil * st, u32 width, u32 height, GF_PixelFormat pixelFormat, const char *pixels, u32 stride, const char *u_plane, const char *v_plane, u32 uv_stride, const char *alpha_plane)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !pixels || !width || !height || !stride || _this->owns_texture)
		return GF_BAD_PARAM;

	_this->pixels = NULL;
	_this->is_yuv = GF_FALSE;

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
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUVA:
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
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	_this->pixel_format = pixelFormat;
	_this->width = width;
	_this->height = height;
	_this->stride = stride;
	_this->stride_uv = uv_stride;
	_this->pixels = (char *) pixels;
	_this->pix_u = (char *) u_plane;
	_this->pix_v = (char *) v_plane;
	texture_set_callbacks(_this);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_texture_planes(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, const u8 *y_or_rgb, u32 stride, const u8 *u_plane, const u8 *v_plane, u32 uv_stride, const u8 *alpha_plane)
{
 	return gf_evg_stencil_set_texture_internal(stencil, width, height, pixelFormat, y_or_rgb, stride, u_plane, v_plane, uv_stride, alpha_plane);
}
GF_EXPORT
GF_Err gf_evg_stencil_set_texture(GF_EVGStencil *stencil, u8 *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat)
{
	return gf_evg_stencil_set_texture_internal(stencil, width, height, pixelFormat, pixels, stride, NULL, NULL, 0, NULL);
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

	_this->replace_col = 0;
	_this->cmat_is_replace = GF_FALSE;
	if (!_this->cmat.identity
	        && !_this->cmat.m[0] && !_this->cmat.m[1] && !_this->cmat.m[2] && !_this->cmat.m[3]
	        && !_this->cmat.m[5] && !_this->cmat.m[6] && !_this->cmat.m[7] && !_this->cmat.m[8]
	        && !_this->cmat.m[10] && !_this->cmat.m[11] && !_this->cmat.m[12] && !_this->cmat.m[13]
	        && !_this->cmat.m[15] && !_this->cmat.m[16] && !_this->cmat.m[17] && !_this->cmat.m[19]) {
		_this->cmat_is_replace = GF_TRUE;
		_this->replace_col = GF_COL_ARGB(FIX2INT(_this->cmat.m[18]*255), FIX2INT(_this->cmat.m[4]*255), FIX2INT(_this->cmat.m[9]*255), FIX2INT(_this->cmat.m[14]*255));

		if (surf->is_yuv) {
			_this->replace_col = evg_argb_to_ayuv(surf, _this->replace_col);
		}
	}

	if ((_this->alpha == 255) && !_this->smat.m[1] && !_this->smat.m[3] && (_this->cmat.identity || _this->cmat_is_replace)) {
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_straight_wide;
		} else {
			_this->fill_run = tex_fill_run_straight;
		}
	} else {
		if (!_this->cmat.identity && _this->is_yuv && surf->is_yuv) {
			evg_make_ayuv_color_mx(&_this->cmat, &_this->yuv_cmat);
		}
		if (surf->not_8bits) {
			_this->fill_run = tex_fill_run_wide;
		} else {
			_this->fill_run = tex_fill_run;
		}
	}

	texture_set_callbacks(_this);
}



GF_EXPORT
GF_Err gf_evg_stencil_set_tiling(GF_EVGStencil * st, GF_TextureTiling mode)
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
	GF_EVGStencil *_this = (GF_EVGStencil *)st;
	if (!_this) return GF_BAD_PARAM;
	is_grad = ((_this->type==GF_STENCIL_LINEAR_GRADIENT) || (_this->type==GF_STENCIL_RADIAL_GRADIENT)) ? GF_TRUE : GF_FALSE;


	if (!cmat) {
		if (is_grad && !_this->cmat.identity)
			((EVG_BaseGradient *) _this)->updated = 1;
		gf_cmx_init(&_this->cmat);
	} else {
		if (is_grad && memcmp(&_this->cmat.m, &cmat->m, sizeof(Fixed)*20))
			((EVG_BaseGradient *) _this)->updated = 1;
		gf_cmx_copy(&_this->cmat, cmat);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_evg_stencil_set_alpha(GF_EVGStencil * st, u8 alpha)
{
	EVG_Texture *_this = (EVG_Texture *)st;
	if (!_this) return GF_BAD_PARAM;
	if (_this->type==GF_STENCIL_SOLID) return GF_BAD_PARAM;
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
