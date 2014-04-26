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
 */

#include "rast_soft.h"

EVGStencil *evg_solid_brush();
EVGStencil *evg_texture_brush();
EVGStencil *evg_linear_gradient_brush();
EVGStencil *evg_radial_gradient_brush();



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

	if (_this->pos[0]>=0) {
		if(_this->pos[0]>0) {
			end = FIX2INT(gf_mulfix(_this->pos[0], maxPos));
			for (i=0; i<= end; i++) {
				_this->pre[i] = _this->col[0];
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
						_this->pre[i] = color_interpolate(_this->col[c], _this->col[c+1],
						                                  (u8) ( ( (i-start) * 255) / diff) );
					}
				}
			} else {
				start = FIX2INT(gf_mulfix(_this->pos[c+0], maxPos));
				for(i=start; i<=EVGGRADIENTMAXINTPOS; i++) {
					_this->pre[i] = _this->col[c];
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
		return _this->pre[(pos & max_pos) ?  EVGGRADIENTMAXINTPOS - (pos % max_pos) :  pos % max_pos];

	case GF_GRADIENT_MODE_REPEAT:
		while (pos < 0) pos += max_pos;
		return _this->pre[pos % max_pos];

	case GF_GRADIENT_MODE_PAD:
	default:
		return _this->pre[ MIN(EVGGRADIENTMAXINTPOS, MAX((s32) 0, pos))];
	}
}

GF_Err evg_stencil_set_gradient_interpolation(GF_STENCIL p, Fixed *pos, GF_Color *col, u32 count)
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

GF_Err evg_stencil_set_gradient_mode(GF_STENCIL p, GF_GradientMode mode)
{
	EVG_BaseGradient *_this = (EVG_BaseGradient *) p;
	if ( (_this->type != GF_STENCIL_LINEAR_GRADIENT) && (_this->type != GF_STENCIL_RADIAL_GRADIENT) ) return GF_BAD_PARAM;
	_this->mod = mode;
	return GF_OK;
}

/*
	Generic stencil
*/

GF_STENCIL evg_stencil_new(GF_Raster2D *_dr, GF_StencilType type)
{
	EVGStencil *st;
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

void evg_stencil_delete(GF_STENCIL st)
{
	EVGStencil *_this = (EVGStencil *) st;
	switch(_this->type) {
	case GF_STENCIL_SOLID:
	case GF_STENCIL_LINEAR_GRADIENT:
	case GF_STENCIL_RADIAL_GRADIENT:
		gf_free(_this);
		return;
	case GF_STENCIL_TEXTURE:
	{
		EVG_Texture *tx = (EVG_Texture *)_this;
		/*destroy conversion buffer if any*/
		if ( tx->conv_buf) gf_free( tx->conv_buf );
		/*destroy local texture iof any*/
		if (tx->owns_texture && tx->pixels) gf_free(tx->pixels);
		gf_free(_this);
	}
	return;
	}
}

GF_Err evg_stencil_set_matrix(GF_STENCIL st, GF_Matrix2D *mx)
{
	EVGStencil *_this = (EVGStencil *)st;
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

EVGStencil *evg_solid_brush()
{
	EVG_Brush *tmp;
	GF_SAFEALLOC(tmp, EVG_Brush);
	if (!tmp) return 0L;
	tmp->fill_run = NULL;
	tmp->color = 0xFF000000;
	tmp->type = GF_STENCIL_SOLID;
	return (EVGStencil *) tmp;
}

GF_Err evg_stencil_set_brush_color(GF_STENCIL st, GF_Color c)
{
	EVG_Brush *_this = (EVG_Brush *) st;
	if (!_this  || (_this ->type != GF_STENCIL_SOLID) ) return GF_BAD_PARAM;
	_this->color = c;
	return GF_OK;
}


/*
	linear gradient stencil
*/

static void lgb_fill_run(EVGStencil *p, EVGSurface *surf, s32 x, s32 y, u32 count)
{
	Bool has_cmat, has_a;
	Fixed _res;
	s32 val;
	u32 col, ca;
	u32 *data = surf->stencil_pix_run;
	EVG_LinearGradient *_this = (EVG_LinearGradient *) p;

	has_cmat = _this->cmat.identity ? 0 : 1;
	has_a = (_this->alpha==0xFF) ? 0 : 1;

	/*no need to move x & y to fixed*/
	_res = (Fixed) (x * _this->smat.m[0] + y * _this->smat.m[1] + _this->smat.m[2]);
	while (count) {
		val = FIX2INT(_res);
		_res += _this->smat.m[0];

		col = gradient_get_color((EVG_BaseGradient *)_this, val );
		if (has_a) {
			ca = ((GF_COL_A(col) + 1) * _this->alpha) >> 8;
			col = ( ((ca<<24) & 0xFF000000) ) | (col & 0x00FFFFFF);
		}
		if (has_cmat) col = gf_cmx_apply(&p->cmat, col);
		*data++ = col;
		count--;
	}
}

GF_Err evg_stencil_set_linear_gradient(GF_STENCIL st, Fixed start_x, Fixed start_y, Fixed end_x, Fixed end_y)
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

EVGStencil *evg_linear_gradient_brush()
{
	s32 i;
	EVG_LinearGradient *tmp;
	GF_SAFEALLOC(tmp, EVG_LinearGradient);
	if (!tmp) return 0L;
	gf_mx2d_init(tmp->vecmat);
	tmp->fill_run = lgb_fill_run;
	tmp->type = GF_STENCIL_LINEAR_GRADIENT;
	for(i=0; i<EVGGRADIENTSLOTS; i++) tmp->pos[i]=-1;

	tmp->alpha = 0xFF;
	evg_stencil_set_linear_gradient((EVGStencil *)tmp, 0, 0, FIX_ONE, 0);
	return (EVGStencil *) tmp;
}


/*
	radial gradient stencil
*/

static void rg_fill_run(EVGStencil *p, EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	Fixed x, y, dx, dy, b, val;
	Bool has_cmat, has_a;
	s32 pos;
	u32 col, ca;
	u32 *data = surf->stencil_pix_run;
	EVG_RadialGradient *_this = (EVG_RadialGradient *) p;

	x = INT2FIX(_x);
	y = INT2FIX(_y);
	gf_mx2d_apply_coords(&_this->smat, &x, &y);

	has_cmat = _this->cmat.identity ? 0 : 1;
	has_a = (_this->alpha==0xFF) ? 0 : 1;

	dx = x - _this->d_f.x;
	dy = y - _this->d_f.y;
	while (count) {
		b = gf_mulfix(_this->rad, gf_mulfix(dx, _this->d_f.x) + gf_mulfix(dy,  _this->d_f.y));
		val = gf_mulfix(b, b) + gf_mulfix(_this->rad, gf_mulfix(dx, dx)+gf_mulfix(dy, dy));
		b += gf_sqrt(val);
		pos = FIX2INT(EVGGRADIENTBUFFERSIZE*b);

		col = gradient_get_color((EVG_BaseGradient *)_this, pos);
		if (has_a) {
			ca = ((GF_COL_A(col) + 1) * _this->alpha) >> 8;
			col = ( ((ca<<24) & 0xFF000000) ) | (col & 0x00FFFFFF);
		}
		if (has_cmat) col = gf_cmx_apply(&p->cmat, col);
		*data++ = col;

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

EVGStencil *evg_radial_gradient_brush()
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
	return (EVGStencil *) tmp;
}


GF_Err evg_stencil_set_radial_gradient(GF_STENCIL st, Fixed cx, Fixed cy, Fixed fx, Fixed fy, Fixed x_radius, Fixed y_radius)
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

/*
	Texture stencil - this is just a crude texture mapper, it lacks precision and filtering ...
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
	return (a2<<24) | (r2<<16) | (g2<<8) | b2;
}
#endif

static void bmp_untransform_coord(EVG_Texture *_this, s32 _x, s32 _y, Fixed *outx, Fixed *outy)
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

static void bmp_fill_run(EVGStencil *p, EVGSurface *surf, s32 _x, s32 _y, u32 count)
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

	bmp_untransform_coord(_this, _x, _y, &x, &y);

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

	y0 = (s32) FIX2INT(y);
	has_alpha = (_this->alpha != 255) ? 1 : 0;
	has_replace_cmat = _this->cmat_is_replace ? 1 : 0;
	has_cmat = _this->cmat.identity ? 0 : 1;
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

		pix = _this->tx_get_pixel(_this->pixels + _this->stride*y0 + _this->Bpp*x0);

		_x++;
		bmp_untransform_coord(_this, _x, _y, &x, &y);

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
				p01 = _this->tx_get_pixel(_this->pixels + _this->stride*y0 + _this->Bpp*x1);
				p10 = _this->tx_get_pixel(_this->pixels + _this->stride*y1 + _this->Bpp*x0);
				p11 = _this->tx_get_pixel(_this->pixels + _this->stride*y1 + _this->Bpp*x1);

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
			pix = ((__a<<24) | (replace_col & 0x00FFFFFF));
		} else if (has_cmat) {
			pix = gf_cmx_apply(&_this->cmat, pix);
		}
		*data++ = pix;
		count--;
	}
}


/*just a little faster...*/
static void bmp_fill_run_straight(EVGStencil *p, EVGSurface *surf, s32 _x, s32 _y, u32 count)
{
	s32 x0, y0;
	u32 pix;
	u32 __a;
	Bool repeat_s = 0;
	Fixed x, y, _fdim;
	char *pix_line;
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
	pix_line = _this->pixels + _this->stride*y0;

	while (count) {
		x0 = FIX2INT(x);
		if (repeat_s) {
			x0 = (x0) % _this->width;
		} else if (x0 >= (s32) _this->width) x0 = _this->width-1;

		x += _this->inc_x;
		pix = _this->tx_get_pixel(pix_line + _this->Bpp*x0);

		if (_this->replace_col) {
			__a = GF_COL_A(pix);
			pix = ((__a<<24) | (_this->replace_col & 0x00FFFFFF));
		}
		*data++ = pix;
		count--;
	}
}

void evg_bmp_init(EVGStencil *p)
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
	_this->cmat_is_replace = 0;
	if (!_this->cmat.identity
	        && !_this->cmat.m[0] && !_this->cmat.m[1] && !_this->cmat.m[2] && !_this->cmat.m[3]
	        && !_this->cmat.m[5] && !_this->cmat.m[6] && !_this->cmat.m[7] && !_this->cmat.m[8]
	        && !_this->cmat.m[10] && !_this->cmat.m[11] && !_this->cmat.m[12] && !_this->cmat.m[13]
	        && !_this->cmat.m[15] && !_this->cmat.m[16] && !_this->cmat.m[17] && !_this->cmat.m[19]) {
		_this->cmat_is_replace = 1;
		_this->replace_col = GF_COL_ARGB(FIX2INT(_this->cmat.m[18]*255), FIX2INT(_this->cmat.m[4]*255), FIX2INT(_this->cmat.m[9]*255), FIX2INT(_this->cmat.m[14]*255));
	}

	if ((_this->alpha == 255) && !_this->smat.m[1] && !_this->smat.m[3] && (_this->cmat.identity || _this->cmat_is_replace)) {
		_this->fill_run = bmp_fill_run_straight;
	} else {
		_this->fill_run = bmp_fill_run;
	}
}


EVGStencil *evg_texture_brush()
{
	EVG_Texture *tmp;
	GF_SAFEALLOC(tmp, EVG_Texture);
	if (!tmp) return 0L;

	tmp->fill_run = bmp_fill_run;
	tmp->type = GF_STENCIL_TEXTURE;
	/*default is using the surface settings*/
	tmp->filter = GF_TEXTURE_FILTER_DEFAULT;
	tmp->mod = 0;
	gf_cmx_init(&tmp->cmat);
	tmp->alpha = 255;
	return (EVGStencil *) tmp;
}


u32 get_pix_argb(char *pix) {
	return GF_COL_ARGB(*(pix+3) & 0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *pix & 0xFF);
}
u32 get_pix_rgba(char *pix) {
	return GF_COL_ARGB(*(pix+3) & 0xFF, *pix & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_rgb_32(char *pix) {
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, *(pix+1) & 0xFF, *pix & 0xFF);
}
u32 get_pix_rgb_24(char *pix) {
	return GF_COL_ARGB(0xFF, *pix & 0xFF, *(pix+1) & 0xFF, *(pix+2) & 0xFF);
}
u32 get_pix_bgr_24(char *pix) {
	return GF_COL_ARGB(0xFF, *(pix+2) & 0xFF, * (pix+1) & 0xFF, *pix & 0xFF);
}
u32 get_pix_444(char *pix) {
	u16 val = *(u16*)pix;
	return GF_COL_ARGB(0xFF,  (u8) ( (val >> 4) & 0xf0), (u8) ( (val) & 0xf0),  (u8) ( (val << 4) & 0xf0)	);
}
u32 get_pix_555(char *pix) {
	u16 val = *(u16*)pix;
	return GF_COL_ARGB(0xFF, (u8) ( (val >> 7) & 0xf8), (u8) ( (val >> 2) & 0xf8), (u8) ( (val << 3) & 0xf8) );
}
u32 get_pix_565(char *pix) {
	u16 val = *(u16*)pix;
	return GF_COL_ARGB(0xFF,  (u8) ( (val >> 8) & 0xf8), (u8) ( (val >> 3) & 0xfc),  (u8) ( (val << 3) & 0xf8)	);
}
u32 get_pix_grey(char *pix) {
	u8 val = *pix;
	return GF_COL_ARGB(0xFF, val, val, val);
}
u32 get_pix_alphagrey(char *pix) {
	return GF_COL_ARGB((u8) *(pix+1), (u8) *pix, (u8) *pix, (u8) *pix);
}

static void texture_set_callback(EVG_Texture *_this)
{
	switch (_this->pixel_format) {
	case GF_PIXEL_RGBA:
		_this->tx_get_pixel = get_pix_rgba;
		return;
	case GF_PIXEL_ARGB:
		_this->tx_get_pixel = get_pix_argb;
		return;
	case GF_PIXEL_RGB_32:
		_this->tx_get_pixel = get_pix_rgb_32;
		return;
	case GF_PIXEL_RGB_24:
		_this->tx_get_pixel = get_pix_rgb_24;
		return;
	case GF_PIXEL_BGR_24:
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
	}
}

GF_Err evg_stencil_set_texture(GF_STENCIL st, char *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat, GF_PixelFormat destination_format_hint, Bool no_copy)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE) || !pixels || !width || !height || !stride || _this->owns_texture)
		return GF_BAD_PARAM;

	_this->pixels = 0L;
	_this->is_converted = 1;

	switch (pixelFormat) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGB_32:
		_this->Bpp = 4;
		break;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
		_this->Bpp = 3;
		break;
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_ALPHAGREY:
		_this->Bpp = 2;
		break;
	case GF_PIXEL_GREYSCALE:
		_this->Bpp = 1;
		break;
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		_this->orig_format = GF_PIXEL_YV12;
		_this->orig_buf = (u8 *) pixels;
		_this->orig_stride = stride;
		_this->is_converted = 0;
		break;
	case GF_PIXEL_YUVA:
		_this->orig_format = GF_PIXEL_YUVA;
		_this->orig_buf = (u8 *) pixels;
		_this->orig_stride = stride;
		_this->is_converted = 0;
		break;
	default:
		/*the rest is not supported (eg BGR32)*/
		return GF_NOT_SUPPORTED;
	}
	_this->pixel_format = pixelFormat;
	_this->width = width;
	_this->height = height;
	_this->stride = stride;
	_this->pixels = (char *) pixels;
	texture_set_callback(_this);
	return GF_OK;
}


GF_Err evg_stencil_set_tiling(GF_STENCIL st, GF_TextureTiling mode)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	_this->mod = mode;
	return GF_OK;
}

GF_Err evg_stencil_set_filter(GF_STENCIL st, GF_TextureFilter filter_mode)
{
	EVG_Texture *_this = (EVG_Texture *) st;
	if (!_this || (_this->type != GF_STENCIL_TEXTURE)) return GF_BAD_PARAM;
	_this->filter = filter_mode;
	return GF_OK;
}

GF_Err evg_stencil_set_color_matrix(GF_STENCIL st, GF_ColorMatrix *cmat)
{
	EVG_Texture *_this = (EVG_Texture *)st;
	if (!_this) return GF_BAD_PARAM;
	if (!cmat) gf_cmx_init(&_this->cmat);
	else gf_cmx_copy(&_this->cmat, cmat);
	return GF_OK;
}

GF_Err evg_stencil_set_alpha(GF_STENCIL st, u8 alpha)
{
	EVG_Texture *_this = (EVG_Texture *)st;
	if (!_this) return GF_BAD_PARAM;
	if (_this->type==GF_STENCIL_SOLID) return GF_BAD_PARAM;
	if (_this->type==GF_STENCIL_TEXTURE)
		_this->alpha = alpha;
	else
		((EVG_BaseGradient*)st)->alpha = alpha;
	return GF_OK;
}


/*internal*/
void evg_set_texture_active(EVGStencil *st)
{
	GF_VideoSurface src, dst;
	EVG_Texture *_this = (EVG_Texture *)st;
	if (_this->is_converted) return;

	/*perform YUV->RGB*/

	if (_this->orig_format == GF_PIXEL_YV12) {
		_this->Bpp = 3;
		_this->pixel_format = GF_PIXEL_RGB_24;
	} else {
		_this->Bpp = 4;
		_this->pixel_format = GF_PIXEL_ARGB;
	}
	if (_this->Bpp * _this->width * _this->height > _this->conv_size) {
		if (_this->conv_buf) gf_free(_this->conv_buf);
		_this->conv_size = _this->Bpp * _this->width * _this->height;
		_this->conv_buf = (unsigned char *) gf_malloc(sizeof(unsigned char)*_this->conv_size);
	}

	memset(&src, 0, sizeof(GF_VideoSurface));
	src.height = _this->height;
	src.width = _this->width;
	src.pitch_x = 0;
	src.pitch_y = _this->orig_stride;
	src.pixel_format = _this->orig_format;
	src.video_buffer = (char *)_this->orig_buf;

	memset(&dst, 0, sizeof(GF_VideoSurface));
	dst.width = _this->width;
	dst.height = _this->height;
	dst.pitch_x = _this->Bpp;
	dst.pitch_y = _this->Bpp * _this->width;
	dst.pixel_format = _this->pixel_format;
	dst.video_buffer = (char *) _this->conv_buf;

	gf_stretch_bits(&dst, &src, NULL, NULL, 0xFF, 0, NULL, NULL);

	_this->is_converted = 1;
	_this->pixels = (char *) _this->conv_buf;
	_this->stride = _this->Bpp * _this->width;
	texture_set_callback(_this);
}

GF_Err evg_stencil_create_texture(GF_STENCIL st, u32 width, u32 height, GF_PixelFormat pixelFormat)
{
	EVG_Texture *_this = 	(EVG_Texture *)st;
	if (_this->orig_buf) return GF_BAD_PARAM;
	_this->pixels = 0L;
	_this->is_converted = 1;

	switch (pixelFormat) {
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGB_32:
		_this->Bpp = 4;
		break;
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
		_this->Bpp = 3;
		break;
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_ALPHAGREY:
		_this->Bpp = 2;
		break;
	case GF_PIXEL_GREYSCALE:
		_this->Bpp = 1;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	_this->pixel_format = pixelFormat;
	_this->width = width;
	_this->height = height;
	_this->stride = width*_this->Bpp;

	if (_this->pixels) gf_free(_this->pixels);
	_this->pixels = (char *) gf_malloc(sizeof(char) * _this->stride * _this->height);
	memset(_this->pixels, 0, sizeof(char) * _this->stride * _this->height);
	_this->owns_texture = 1;
	texture_set_callback(_this);
	return GF_OK;
}
