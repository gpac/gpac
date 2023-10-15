/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2023
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

#ifndef GPAC_DISABLE_EVG

//TODO: support for color spaces, support for more than 10 bits and BigEndian format ?

#if 1

#define mul255(_a, _b)  ( (((u32)(_a) + 1) * (u32)(_b) ) >> 8)

#else
static s32
mul255(s32 a, s32 b)
{
	return ((a + 1) * b) >> 8;
}
#endif

static s32
mul255_zero(s32 a, s32 b)
{
	if (!a) return 0;
	return ((a + 1) * b) >> 8;
}

//RGB <-> YUV full range conversion, using integer (1024 factor)
#define YUV_USE_INT

void gf_evg_rgb_to_yuv(GF_EVGSurface *surf, GF_Color col, u8*y, u8*cb, u8*cr)
{
	u32 r = GF_COL_R(col);
	u32 g = GF_COL_G(col);
	u32 b = GF_COL_B(col);

#ifndef YUV_USE_INT
	*y = (u8) (0.299*r + 0.587 * g + 0.114 * b);
	*cb = (u8) (-0.169*(s32)r - 0.331*(s32)g + 0.499*b + 128);
	*cr = (u8) (0.499 * r - 0.418*(s32)g - 0.0813*(s32)b + 128);
#else
	u32 _v = 306*r + 601 * g + 117 * b;
	*y = (u8) (_v >> 10);
	_v = (-173*(s32)r - 339*(s32)g + 511*b + 131072);
	*cb = (u8) (_v >> 10);
	_v = (511 * r - 428*(s32)g - 83*(s32)b + 131072);
	*cr = (u8) (_v >> 10);
#endif
}
GF_Color gf_evg_argb_to_ayuv(GF_EVGSurface *surf, GF_Color col)
{
	u8 a, y, cb, cr;
	a = GF_COL_A(col);
	u32 r = GF_COL_R(col);
	u32 g = GF_COL_G(col);
	u32 b = GF_COL_B(col);

#ifndef YUV_USE_INT
	y = (u8) (0.299*r + 0.587 * g + 0.114 * b);
	cb = (u8) (-0.169*(s32)r - 0.331*(s32)g + 0.499*b + 128);
	cr = (u8) (0.499 * r - 0.418*(s32)g - 0.0813*(s32)b + 128);
#else
	u32 _v = 306*r + 601 * g + 117 * b;
	y = (u8) (_v >> 10);
	_v = (-173*(s32)r - 339*(s32)g + 511*b + 131072);
	cb = (u8) (_v >> 10);
	_v = (511 * r - 428*(s32)g - 83*(s32)b + 131072);
	cr = (u8) (_v >> 10);
#endif
	return GF_COL_ARGB(a, y, cb, cr);
}

GF_Err gf_gf_evg_rgb_to_yuv_f(GF_EVGSurface *surf, Float r, Float g, Float b, Float *y, Float *cb, Float *cr)
{
	*y = (0.299f * r + 0.587f * g + 0.114f * b);
	*cb = (-0.169f * (s32)r - 0.331f * (s32)g + 0.499f * b + 128.0f);
	*cr = (0.499f * r - 0.418f * (s32)g - 0.0813f * (s32)b + 128.0f);
	return GF_OK;
}
GF_Err gf_evg_yuv_to_rgb_f(GF_EVGSurface *surf, Float y, Float cb, Float cr, Float *r, Float *g, Float *b)
{
	*r = (y + 1.402f * (cr - 128.0f));
	*g = (y - 0.344136f * (cb - 128.0f) - 0.714136f * (cr-128.0f) );
	*b = (y + 1.772f * (cb - 128.0f) );
	return GF_OK;
}

GF_Color gf_evg_ayuv_to_argb(GF_EVGSurface *surf, GF_Color col)
{
	u32 a;
	s32 y, cb, cr;
	s32 r, g, b;
	a = GF_COL_A(col);
	y = GF_COL_R(col);
	cb = GF_COL_G(col);
	cr = GF_COL_B(col);

#ifndef YUV_USE_INT
	r = (s32) (y + 1.402 * (cr - 128));
	g = (s32) (y - 0.344136 * (cb - 128) - 0.714136*(cr-128) );
	b = (s32) (y + 1.772 * (cb - 128) );

#define TRUNC_8BIT(_a) if (_a<0) {_a = 0;} else if (_a>255) {_a=255;}
	TRUNC_8BIT(r)
	TRUNC_8BIT(g)
	TRUNC_8BIT(b)

#else
	y *= 1024;
	r = (s32) (y + 1436 * (cr - 128));
	g = (s32) (y - 352 * (cb - 128) - 731*(cr-128) );
	b = (s32) (y + 1814 * (cb - 128) );

#define TRUNC_8BIT(_a) if (_a<0) {_a = 0;} else { u32 __a = (u32) _a; __a>>=10; if (__a>255) {__a=255;} _a = __a; }
	TRUNC_8BIT(r)
	TRUNC_8BIT(g)
	TRUNC_8BIT(b)

#endif


	return GF_COL_ARGB(a, r, g, b);
}

u64 gf_evg_argb_to_ayuv_wide(GF_EVGSurface *surf, u64 col)
{
	u16 a, y, cb, cr;
	u32 r, g, b;

	a = (col>>48)&0xFFFF;
	r = (col>>32)&0xFFFF;
	g = (col>>16)&0xFFFF;
	b = (col)&0xFFFF;

#ifndef YUV_USE_INT
	y = (u16) (0.299*r + 0.587 * g + 0.114 * b);
	cb = (u16) (-0.169*(s32)r - 0.331*(s32)g + 0.499*b + 32768);
	cr = (u16) (0.499 * r - 0.418*(s32)g - 0.0813*(s32)b + 32768);
#else
	u32 _v = 306*r + 601 * g + 117 * b;
	y = (u16) (_v >> 10);
	_v = (-173*(s32)r - 339*(s32)g + 511*b + 33554432);
	cb = (u16) (_v >> 10);
	_v = (511 * r - 428*(s32)g - 83*(s32)b + 33554432);
	cr = (u16) (_v >> 10);
#endif


	return GF_COLW_ARGB(a, y, cb, cr);
}
u64 gf_evg_ayuv_to_argb_wide(GF_EVGSurface *surf, u64 col)
{
	u32 a;
	s64 y, cb, cr;
	s32 r, g, b;
	a = (col>>48)&0xFFFF;
	y = (col>>32)&0xFFFF;
	cb = (col>>16)&0xFFFF;
	cr = (col)&0xFFFF;

#ifndef YUV_USE_INT
	r = (s32) (y + 1.402 * (cr - 32768));
	g = (s32) (y - 0.344136 * (cb - 32768) - 0.714136*(cr-32768) );
	b = (s32) (y + 1.772 * (cb - 32768) );
#define TRUNC_16BIT(_a) if (_a<0) {_a = 0;} else if (_a>32768) {_a=32768;}
	TRUNC_16BIT(r)
	TRUNC_16BIT(g)
	TRUNC_16BIT(b)

#else
	y *= 1024;
	r = (s32) (y + 1436 * (cr - 32768));
	g = (s32) (y - 352 * (cb - 32768) - 731*(cr-32768) );
	b = (s32) (y + 1814 * (cb - 32768) );

#define TRUNC_16BIT(_a) if (_a<0) {_a = 0;} else { u32 __a = (u32) _a; __a>>=10; if (__a>32768) {__a=32768;} _a = __a; }
	TRUNC_16BIT(r)
	TRUNC_16BIT(g)
	TRUNC_16BIT(b)

#endif

	return GF_COLW_ARGB(a, r, g, b);
}

#if 0 //unused
void evg_make_ayuv_color_mx(GF_ColorMatrix *cmat, GF_ColorMatrix *yuv_cmat)
{
	GF_ColorMatrix cmx_y2r, cmx_r2y;
	gf_cmx_init(&cmx_r2y);
	gf_cmx_init(&cmx_y2r);
	cmx_r2y.identity = cmx_y2r.identity = GF_FALSE;

	//r = (s32) (y + 1.402 * (cr - 128));
	cmx_y2r.m[0] = FIX_ONE; //y
	cmx_y2r.m[2] = FLT2FIX(1.402); //cr
	cmx_y2r.m[4] = FLT2FIX(-1.402 * 0.504); //tr

	//g = (s32) (y - 0.344136 * (cb - 128) - 0.714136*(cr-128) );
	cmx_y2r.m[5] = FIX_ONE; //y
	cmx_y2r.m[6] = FLT2FIX(- 0.344136); //cb
	cmx_y2r.m[7] = FLT2FIX(- 0.714136); //cr
	cmx_y2r.m[9] = FLT2FIX(0.344136 * 0.504 + 0.714136*0.504); //tr

	//b = (s32) (y + 1.772 * (cb - 128) );
	cmx_y2r.m[10] = FIX_ONE; //y
	cmx_y2r.m[11] = FLT2FIX(1.772); //cb
	cmx_y2r.m[14] = FLT2FIX(-1.772 * 0.504); //tr


	//y = (u8) (0.299*r + 0.587 * g + 0.114 * b);
	cmx_r2y.m[0] = FLT2FIX(0.299); //r
	cmx_r2y.m[1] = FLT2FIX(0.587); //g
	cmx_r2y.m[2] = FLT2FIX(0.114); //b


	//cb = (u8) (-0.169*(s32)r - 0.331*(s32)g + 0.499*b + 128);
	cmx_r2y.m[5] = FLT2FIX(-0.169); //r
	cmx_r2y.m[6] = FLT2FIX(-0.331); //g
	cmx_r2y.m[7] = FLT2FIX(0.499); //b
	cmx_r2y.m[9] = FLT2FIX(0.504); //tr

	//cr = (u8) (0.499 * r - 0.418*(s32)g - 0.0813*(s32)b + 128);
	cmx_r2y.m[10] = FLT2FIX(0.499); //r
	cmx_r2y.m[11] = FLT2FIX(-0.418); //g
	cmx_r2y.m[12] = FLT2FIX(-0.0813); //b
	cmx_r2y.m[14] = FLT2FIX(0.504); //tr

	gf_cmx_copy(yuv_cmat, &cmx_r2y);
	gf_cmx_multiply(yuv_cmat, cmat);
	gf_cmx_multiply(yuv_cmat, &cmx_y2r);
}
#endif

/*
			YUV420p part
*/

static void overmask_yuv420p(u8 col_a, u8 cy, char *dst, u32 alpha)
{
	s32 srca = col_a;
	u32 srcc = cy;
	s32 dstc = (*dst) & 0xFF;

	srca = mul255(srca, alpha);
	*dst = mul255(srca, srcc - dstc) + dstc;
}

static void overmask_yuv420p_const_run(u8 a, u8 val, u8 *ptr, u32 count, short x)
{
	while (count) {
		u8 dst = *(ptr);
		*ptr = (u8) mul255(a, val - dst) + dst;
		ptr ++;
		count--;
	}
}
void evg_yuv420p_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	u8 *pU = rctx->surf->pixels + rctx->surf->height * rctx->surf->pitch_y;
	u8 *pV;
	pU +=  y/2 * rctx->surf->pitch_y/2;
	pV = pU + rctx->surf->height/2 * rctx->surf->pitch_y/2;

	//no need to swap u and V in const flush, they have been swaped when setting up the brush

	//we are at an odd line, write uv
	for (i=0; i<rctx->surf->width; i+=2) {
		u8 dst;

		//even line
		a = rctx->uv_alpha[i] + rctx->uv_alpha[i+1];
		//odd line
		a += surf_uv_alpha[i] + surf_uv_alpha[i+1];

		if (a) {
			char *s_ptr_u, *s_ptr_v;

			a /= 4;

			s_ptr_u = pU + i / 2;
			s_ptr_v = pV + i / 2;
			if (a==0xFF) {
				*s_ptr_u = (u8) cu;
				*s_ptr_v = (u8) cv;
			} else {
				dst = *(s_ptr_u);
				*s_ptr_u = (u8) mul255(a, cu - dst) + dst;

				dst = *(s_ptr_v);
				*s_ptr_v = (u8) mul255(a, cv - dst) + dst;
			}
		}
	}
	memset(rctx->uv_alpha, 0, rctx->surf->uv_alpha_alloc);
}

void evg_yuv420p_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	char *pY = surf->pixels;
	u8 *surf_uv_alpha;
	s32 i;
	u8 cy, cu, cv;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = rctx->uv_alpha;
	}
	else if (write_uv) {
		surf_uv_alpha = rctx->uv_alpha + surf->width;
	} else {
		surf_uv_alpha = rctx->uv_alpha;
	}

	pY +=  y * surf->pitch_y;

	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);

	for (i=0; i<count; i++) {
		u32 a;
		char *s_pY;
		u32 len;
		len = spans[i].len;
		s_pY = pY + spans[i].x;

		a = spans[i].coverage;
		if (a != 0xFF) {
			overmask_yuv420p_const_run((u8)a, cy, s_pY, len, 0);
			memset(surf_uv_alpha + spans[i].x, (u8)a, len);
		} else  {
			while (len--) {
				*(s_pY) = cy;
				s_pY ++;
			}
			memset(surf_uv_alpha + spans[i].x, 0xFF, spans[i].len);
		}
	}
	if (write_uv && !rctx->no_yuv_flush) {
		surf->yuv_flush_uv(surf, rctx, surf_uv_alpha, cu, cv, y);
	}

}

void evg_yuv420p_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 a;
	char *pY = surf->pixels;
	u8 *surf_uv_alpha;
	s32 i;
	u8 cy, cu, cv;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = rctx->uv_alpha;
	} else if (write_uv) {
		surf_uv_alpha = rctx->uv_alpha + surf->width;
	} else {
		surf_uv_alpha = rctx->uv_alpha;
	}

	pY +=  y * surf->pitch_y;

	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);
	a = GF_COL_A(surf->fill_col);

	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 fin, j;
			for (j=0; j<spans[i].len; j++) {
				s32 x = spans[i].x + j;
				char *s_pY = pY + x;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);

				overmask_yuv420p_const_run((u8)fin, cy, s_pY, 1, 0);

				memset(surf_uv_alpha + x, (u8)fin, 1);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			char *s_pY;
			u32 fin, len;
			len = spans[i].len;
			s_pY = pY + spans[i].x;
			fin = mul255(a, spans[i].coverage);

			overmask_yuv420p_const_run((u8)fin, cy, s_pY, len, 0);

			memset(surf_uv_alpha + spans[i].x, (u8)fin, len);
		}
	}
	//we are at an odd line, write uv
	if (write_uv && !rctx->no_yuv_flush)
		surf->yuv_flush_uv(surf, rctx, surf_uv_alpha, cu, cv, y);
}


void evg_yuv420p_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 _cu, s32 _cv,  s32 y)
{
	u32 i;
	u8 *pU, *pV;
	pU = surf->pixels + surf->height *surf->pitch_y;
	pU += y/2 * surf->pitch_y/2;
	pV = pU + surf->height/2 * surf->pitch_y/2;

	if (surf->swap_uv) {
		u8 *tmp = pU;
		pU = pV;
		pV = tmp;
	}

	for (i=0; i<surf->width; i+=2) {
		u32 a, a11, a12, a21, a22;
		u32 idx1 = 3*i;
		u32 idx2 = idx1 + 3;
		assert(idx1 < surf->uv_alpha_alloc );
		assert(idx2 < surf->uv_alpha_alloc );
		//get alpha
		a11 = (u32)rctx->uv_alpha[idx1];
		a12 = (u32)rctx->uv_alpha[idx2];
		a21 = (u32)surf_uv_alpha[idx1];
		a22 = (u32)surf_uv_alpha[idx2];

		a = a11 + a12 + a21 + a22;
		if (a) {
			u8 cdst=0;
			u32 chroma_u, chroma_v, c11, c12, c21, c22;

			a /= 4;
			//get cb
			idx1 += 1;
			idx2 += 1;

			if (a!=0xFF) {
 				cdst = *pU;
			}
			c11 = (u32)rctx->uv_alpha[idx1];
			if (a11!=0xFF) c11 = mul255_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)rctx->uv_alpha[idx2];
			if (a12!=0xFF) c12 = mul255_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha[idx1];
			if (a21!=0xFF) c21 = mul255_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha[idx2];
			if (a22!=0xFF) c22 = mul255_zero(a22, c22 - cdst) + cdst;

			chroma_u = c11 + c12 + c21 + c22;
			chroma_u /= 4;

			//get cr
			idx1 += 1;
			idx2 += 1;

			if (a!=0xFF) {
 				cdst = *pV;
			}
			c11 = (u32)rctx->uv_alpha[idx1];
			if (a11!=0xFF) c11 = mul255_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)rctx->uv_alpha[idx2];
			if (a12!=0xFF) c12 = mul255_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha[idx1];
			if (a21!=0xFF) c21 = mul255_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha[idx2];
			if (a22!=0xFF) c22 = mul255_zero(a22, c22 - cdst) + cdst;

			chroma_v = c11 + c12 + c21 + c22;
			chroma_v /= 4;

			*pU = chroma_u;
			*pV = chroma_v;
		}
		pU++;
		pV++;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);

}

void evg_yuv420p_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	s32 i;
	char *pY = surf->pixels;
	u8 *surf_uv_alpha;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = rctx->uv_alpha;
	} else if (write_uv) {
		//second line of storage (we store alpha, cr, cb for each pixel)
		surf_uv_alpha = rctx->uv_alpha + 3*surf->width;
	} else {
		//first line of storage
		surf_uv_alpha = rctx->uv_alpha;
	}

	pY +=  y * surf->pitch_y;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		char *s_pY;
		short x;
		u32 *p_col;
		u32 len;
		len = spans[i].len;
		p_col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;

		s_pY = pY + spans[i].x;
		x = spans[i].x;

		while (len--) {
			u32 col = *p_col;
			col_a = GF_COL_A(col);
			if (col_a) {
				u8 cy, cb, cr;
				u32 idx=3*x;
				//col is directly packed as AYCbCr
				cy = GF_COL_R(col);
				cb = GF_COL_G(col);
				cr = GF_COL_B(col);

				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_yuv420p(col_a, cy, s_pY, spanalpha);

					u8 a = mul255(col_a, spanalpha);
					surf_uv_alpha[idx] = a;
				} else {
					*s_pY = cy;
					surf_uv_alpha[idx] = 0xFF;
				}
				surf_uv_alpha[idx+1] = cb;
				surf_uv_alpha[idx+2] = cr;
			}
			s_pY++;
			p_col++;
			x++;
		}
	}
	//compute final u,v for both lines
	if (write_uv && !rctx->no_yuv_flush) {
		surf->yuv_flush_uv(surf, rctx, surf_uv_alpha, 0, 0, y);
	}
}

GF_Err evg_surface_clear_yuv420p(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i;
	u8 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	u8 *pY, *pU, *pV;

	pY = surf->pixels + rc.y * surf->pitch_y + rc.x;
	pU = surf->pixels + surf->height * surf->pitch_y + rc.y/2 * surf->pitch_y/2 + rc.x/2;
	pV = surf->pixels + 5*surf->height * surf->pitch_y/4 + rc.y/2 * surf->pitch_y/2 + rc.x/2;
	if (surf->swap_uv) {
		u8 *tmp = pU;
		pU = pV;
		pV = tmp;
	}

	gf_evg_rgb_to_yuv(surf, col, &cy, &cb, &cr);

	if (!rc.x && !rc.y && ((u32) rc.width==_surf->width) && ((u32) rc.height==_surf->height)) {
		memset(pY, cy, _surf->pitch_y * _surf->height);
		memset(pU, cb, _surf->pitch_y/2 * _surf->height/2);
		memset(pV, cr, _surf->pitch_y/2 * _surf->height/2);
		return GF_OK;
	}

	for (i = 0; i < rc.height; i++) {
		memset(pY, cy, rc.width);
		pY += surf->pitch_y;
		if (i%2) {
			memset(pU, cb, rc.width/2);
			pU += surf->pitch_y/2;
			memset(pV, cr, rc.width/2);
			pV += surf->pitch_y/2;
		}
	}
	return GF_OK;
}


/*
			NV12 / NV21 part
*/


void evg_nv12_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	pU +=  y/2 * surf->pitch_y;

	for (i=0; i<surf->width; i+=2) {
		u8 dst;

		//even line
		a = rctx->uv_alpha[i] + rctx->uv_alpha[i+1];
		//odd line
		a += surf_uv_alpha[i] + surf_uv_alpha[i+1];

		if (a) {
			char *s_ptr;
			a /= 4;

			s_ptr = pU + i;
			if (a==0xFF) {
				*s_ptr = cu;

				s_ptr++;
				*s_ptr = cv;
			} else {
				dst = *(s_ptr);
				*s_ptr = (u8) mul255(a, cu - dst) + dst;

				s_ptr++;
				dst = *(s_ptr);
				*s_ptr = (u8) mul255(a, cv - dst) + dst;
			}
		}
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}

void evg_nv12_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i;
	char *pU;
	pU = surf->pixels + surf->height *surf->pitch_y;
	pU += y/2 * surf->pitch_y;

	for (i=0; i<surf->width; i+=2) {
		u32 a, a11, a12, a21, a22;

		u32 idx1=3*i;
		u32 idx2=3*i + 3;

		//get alpha
		a11 = (u32)rctx->uv_alpha[idx1];
		a12 = (u32)rctx->uv_alpha[idx2];
		a21 = (u32)surf_uv_alpha[idx1];
		a22 = (u32)surf_uv_alpha[idx2];
		a = a11+a12+a21+a22;

		if (a) {
			u8 cdst=0;
			u32 chroma_u, chroma_v, c11, c12, c21, c22;

			a /= 4;

			//get cb
			idx1 += 1;
			idx2 += 1;
			if (a!=0xFF)
				cdst = pU[surf->idx_u];

			c11 = (u32)rctx->uv_alpha[idx1];
			if (a11!=0xFF) c11 = mul255_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)rctx->uv_alpha[idx2];
			if (a12!=0xFF) c12 = mul255_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha[idx1];
			if (a21!=0xFF) c21 = mul255_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha[idx2];
			if (a22!=0xFF) c22 = mul255_zero(a22, c22 - cdst) + cdst;

			chroma_u = c11 + c12 + c21 + c22;
			chroma_u /= 4;

			//get cr
			idx1 += 1;
			idx2 += 1;

			if (a!=0xFF)
				cdst = pU[surf->idx_v];

			c11 = (u32)rctx->uv_alpha[idx1];
			if (a11!=0xFF) c11 = mul255_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)rctx->uv_alpha[idx2];
			if (a12!=0xFF) c12 = mul255_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha[idx1];
			if (a21!=0xFF) c21 = mul255_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha[idx2];
			if (a22!=0xFF) c22 = mul255_zero(a22, c22 - cdst) + cdst;

			chroma_v = c11 + c12 + c21 + c22;
			chroma_v /= 4;

			pU[surf->idx_u] = chroma_u;
			pU[surf->idx_v] = chroma_v;
		}
		pU+=2;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}


GF_Err evg_surface_clear_nv12(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col, Bool swap_uv)
{
	s32 i;
	u8 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pU_first;

	pY += rc.y * surf->pitch_y;
	pU +=  rc.y/2 * surf->pitch_y;

	pY += rc.x;
	pU += rc.x/2;
	pU_first = pU;

	gf_evg_rgb_to_yuv(surf, col, &cy, &cb, &cr);

	if (swap_uv) {
		u8 t = cb;
		cb = cr;
		cr = t;
	}
	for (i = 0; i < rc.height; i++) {
		memset(pY, cy, rc.width);
		pY += surf->pitch_y;
		if (i%2) {
			//first uv line, build it
			if (i==1) {
				s32 j;
				for (j=0; j<rc.width/2; j++) {
					*pU = cb;
					pU++;
					*pU = cr;
					pU++;
				}
			}
			//non-first uv line recopy from first
			else {
				memcpy(pU, pU_first, rc.width);
				pU += surf->pitch_y;
			}
		}
	}
	return GF_OK;
}


/*
			YUV422 part
*/

void evg_yuv422p_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;
	pU +=  y * surf->pitch_y/2;
	pV = pU + surf->height * surf->pitch_y/2;

	for (i=0; i<surf->width; i+=2) {
		u8 dst;

		a = rctx->uv_alpha[i] + rctx->uv_alpha[i+1];

		if (a) {
			char *s_ptr_u, *s_ptr_v;

			a /= 2;
			s_ptr_u = pU + i / 2;
			s_ptr_v = pV + i / 2;
			if (a==0xFF) {
				*s_ptr_u = (u8) cu;
				*s_ptr_v = (u8) cv;
			} else {
				dst = *(s_ptr_u);
				*s_ptr_u = (u8) mul255(a, cu - dst) + dst;

				dst = *(s_ptr_v);
				*s_ptr_v = (u8) mul255(a, cv - dst) + dst;
			}
		}
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}

void evg_yuv422p_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *surf_uv_alpha, s32 _cu, s32 _cv,  s32 y)
{
	u32 i;
	char *pU, *pV;
	pU = surf->pixels + surf->height *surf->pitch_y;
	pU += y * surf->pitch_y/2;
	pV = pU + surf->height * surf->pitch_y/2;

	for (i=0; i<surf->width; i+=2) {
		u32 a, a1, a2;
		u32 idx1=3*i;
		u32 idx2=3*i + 3;
		//get alpha
		a1 = (u32)rctx->uv_alpha[idx1];
		a2 = (u32)rctx->uv_alpha[idx2];
		a = a1+a2;
		if (a) {
			u8 cdst=0;
			u32 chroma_u, chroma_v, c1, c2;

			a /= 2;

			//get cb
			if (a != 0xFF)
				cdst = *pU;

			c1 = (u32)rctx->uv_alpha[idx1+1];
			if (a1!=0xFF) c1 = mul255_zero(a1, c1 - cdst) + cdst;
			c2 = (u32)rctx->uv_alpha[idx2+1];
			if (a2!=0xFF) c2 = mul255_zero(a2, c1 - cdst) + cdst;
			chroma_u = c1 + c2;
			chroma_u /= 2;

			//get cb
			if (a != 0xFF)
				cdst = *pV;

			c1 = (u32)rctx->uv_alpha[idx1+2];
			if (a1!=0xFF) c1 = mul255_zero(a1, c1 - cdst) + cdst;
			c2 = (u32)rctx->uv_alpha[idx2+2];
			if (a2!=0xFF) c2 = mul255_zero(a2, c1 - cdst) + cdst;
			chroma_v = c1 + c2;
			chroma_v /= 2;


			*pU = chroma_u;
			*pV = chroma_v;
		}
		pU++;
		pV++;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}


GF_Err evg_surface_clear_yuv422p(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i;
	u8 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;

	pY += rc.y * surf->pitch_y;
	pU +=  rc.y/2 * surf->pitch_y/2;
	pV = pU + surf->height/2 * surf->pitch_y/2;

	pY += rc.x;
	pU += rc.x/2;
	pV += rc.x/2;

	gf_evg_rgb_to_yuv(surf, col, &cy, &cb, &cr);

	if (!rc.x && !rc.y && ((u32) rc.width==_surf->width) && ((u32) rc.height==_surf->height)) {
		memset(pY, cy, _surf->pitch_y * _surf->height);
		memset(pU, cb, _surf->pitch_y/2 * _surf->height);
		memset(pV, cr, _surf->pitch_y/2 * _surf->height);
		return GF_OK;
	}


	for (i = 0; i < rc.height; i++) {
		memset(pY, cy, rc.width);
		pY += surf->pitch_y;
		memset(pU, cb, rc.width/2);
		pU += surf->pitch_y/2;
		memset(pV, cr, rc.width/2);
		pV += surf->pitch_y/2;
	}
	return GF_OK;
}

/*
			YUV444 part
*/

void evg_yuv444p_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	char *pY, *pU, *pV;
	s32 i;
	u8 cy, cu, cv;

	pY = surf->pixels + y * surf->pitch_y;
	pU = surf->pixels + surf->height*surf->pitch_y + y * surf->pitch_y;
	pV = surf->pixels + 2*surf->height*surf->pitch_y + y * surf->pitch_y;

	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);

	for (i=0; i<count; i++) {
		u32 a;
		char *s_pY, *s_pU, *s_pV;
		u32 len;
		len = spans[i].len;
		s_pY = pY + spans[i].x;
		s_pU = pU + spans[i].x;
		s_pV = pV + spans[i].x;

		a = spans[i].coverage;
		if (a != 0xFF) {
			overmask_yuv420p_const_run((u8)a, cy, s_pY, len, 0);
			overmask_yuv420p_const_run((u8)a, cu, s_pU, len, 0);
			overmask_yuv420p_const_run((u8)a, cv, s_pV, len, 0);
		} else  {
			while (len--) {
				*(s_pY) = cy;
				s_pY ++;
				*(s_pU) = cu;
				s_pU ++;
				*(s_pV) = cv;
				s_pV ++;
			}
		}
	}
}

void evg_yuv444p_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 a;
	char *pY, *pU, *pV;
	s32 i;
	u8 cy, cu, cv;

	pY = surf->pixels + y * surf->pitch_y;
	pU = surf->pixels + surf->height*surf->pitch_y + y * surf->pitch_y;
	pV = surf->pixels + 2*surf->height*surf->pitch_y + y * surf->pitch_y;

	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);
	a = GF_COL_A(surf->fill_col);

	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				char *s_pY, *s_pU, *s_pV;
				u32 fin;
				s32 x = spans[i].x + j;
				s_pY = pY + x;
				s_pU = pU + x;
				s_pV = pV + x;
				u8 aa = surf->get_alpha(surf->get_alpha_udta, a, x, y);
				fin = mul255(aa, spans[i].coverage);

				overmask_yuv420p_const_run((u8)fin, cy, s_pY, 1, 0);
				overmask_yuv420p_const_run((u8)fin, cu, s_pU, 1, 0);
				overmask_yuv420p_const_run((u8)fin, cv, s_pV, 1, 0);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			char *s_pY, *s_pU, *s_pV;
			u32 fin, len;
			len = spans[i].len;
			s_pY = pY + spans[i].x;
			s_pU = pU + spans[i].x;
			s_pV = pV + spans[i].x;
			fin = mul255(a, spans[i].coverage);

			overmask_yuv420p_const_run((u8)fin, cy, s_pY, len, 0);
			overmask_yuv420p_const_run((u8)fin, cu, s_pU, len, 0);
			overmask_yuv420p_const_run((u8)fin, cv, s_pV, len, 0);
		}
	}
}

void evg_yuv444p_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	s32 i;
	char *pY, *pU, *pV;

	pY = surf->pixels + y * surf->pitch_y;
	pU = pY + surf->height*surf->pitch_y;
	pV = pU + surf->height*surf->pitch_y;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		u32 len;
		u32 *p_col;
		char *s_pY, *s_pU, *s_pV;
		len = spans[i].len;
		p_col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;

		s_pY = pY + spans[i].x;
		s_pU = pU + spans[i].x;
		s_pV = pV + spans[i].x;

		while (len--) {
			u32 col = *p_col;
			col_a = GF_COL_A(col);
			if (col_a) {
				u8 cy, cb, cr;
				cy = GF_COL_R(col);
				cb = GF_COL_G(col);
				cr = GF_COL_B(col);

				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_yuv420p(col_a, cy, s_pY, spanalpha);
					overmask_yuv420p(col_a, cb, s_pU, spanalpha);
					overmask_yuv420p(col_a, cr, s_pV, spanalpha);
				} else {
					*s_pY = cy;
					*s_pU = cb;
					*s_pV = cr;
				}
			}
			s_pY++;
			s_pU++;
			s_pV++;
			p_col++;
		}
	}
}

GF_Err evg_surface_clear_yuv444p(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i;
	u8 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;

	pY += rc.y * surf->pitch_y;
	pU += rc.y * surf->pitch_y;
	pV = pU + surf->height * surf->pitch_y;

	pY += rc.x;
	pU += rc.x;
	pV += rc.x;

	gf_evg_rgb_to_yuv(surf, col, &cy, &cb, &cr);

	if (!rc.x && !rc.y && ((u32) rc.width==_surf->width) && ((u32) rc.height==_surf->height)) {
		memset(pY, cy, _surf->pitch_y * _surf->height);
		memset(pU, cb, _surf->pitch_y * _surf->height);
		memset(pV, cr, _surf->pitch_y * _surf->height);
		return GF_OK;
	}

	for (i = 0; i < rc.height; i++) {
		memset(pY, cy, rc.width);
		pY += surf->pitch_y;
		memset(pU, cb, rc.width);
		pU += surf->pitch_y;
		memset(pV, cr, rc.width);
		pV += surf->pitch_y;
	}
	return GF_OK;
}


/*
			YUYV part
*/

static void overmask_yuvy(char *dst, u8 c, u32 alpha)
{
	s32 srca = alpha;
	u32 srcc = c;
	s32 dstc = (*dst) & 0xFF;
	*dst = mul255(srca, srcc - dstc) + dstc;
}
void evg_yuyv_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	char *pY;
	s32 i;
	u8 cy, cu, cv;

	if (!count) return;

	pY = surf->pixels + y * surf->pitch_y;
	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);

	for (i=0; i<count; i++) {
		char *s_pY;
		u32 len;
		len = spans[i].len;
		//get start of yuyv block: devide x by 2, multiply by 4 (two Y pix packed in 4 bytes)
		s_pY = pY + (spans[i].x/2) * 4;
		//move to first Y in block
		s_pY += surf->idx_y1;
		//if odd pixel move to next one
		if (spans[i].x%2) s_pY += 2;

		if (spans[i].coverage != 0xFF) {
			memset(rctx->uv_alpha + spans[i].x, spans[i].coverage, len);
			while (len--) {
				overmask_yuvy(s_pY, cy, spans[i].coverage);
				s_pY += 2;
			}
		} else  {
			memset(rctx->uv_alpha + spans[i].x, 0xFF, len);

			while (len--) {
				*s_pY = cy;
				s_pY += 2;
			}
		}
	}

	for (i=0; i<(s32) surf->width; i+=2) {
		u32 a = (u32)rctx->uv_alpha[i];
		a += (u32)rctx->uv_alpha[i + 1];
		if (a) {
			a /=2;
			if (a==0xFF) {
				pY[surf->idx_u] = cu;
				pY[surf->idx_v] = cv;
			} else if (a) {
				overmask_yuvy(pY + surf->idx_u, cu, a);
				overmask_yuvy(pY + surf->idx_v, cv, a);
			}
		}
		pY+=4;
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}

void evg_yuyv_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	char *pY;
	s32 i;
	u8 cy, cu, cv, a;

	pY = surf->pixels + y * surf->pitch_y;
	cy = GF_COL_R(surf->fill_col);
	cu = GF_COL_G(surf->fill_col);
	cv = GF_COL_B(surf->fill_col);
	a = GF_COL_A(surf->fill_col);

	for (i=0; i<count; i++) {
		char *s_pY;
		u32 fin, len;
		len = spans[i].len;
		s_pY = pY + (spans[i].x/2) * 4;
		if (spans[i].x%2) s_pY += 2;

		fin = mul255(a, spans[i].coverage);

		memset(rctx->uv_alpha + spans[i].x, (u8)fin, len);
		while (len--) {
			overmask_yuvy(s_pY + surf->idx_y1, cy, fin);
			s_pY += 2;
		}
	}
	pY = surf->pixels + y * surf->pitch_y;
	for (i=0; i<(s32) surf->width; i+=2) {
		u32 p_a = rctx->uv_alpha[i];
		p_a += rctx->uv_alpha[i + 1];
		p_a /=2;
		if (p_a==0xFF) {
			pY[surf->idx_u] = cu;
			pY[surf->idx_v] = cv;
		} else if (p_a) {
			overmask_yuvy(pY + surf->idx_u, cu, p_a);
			overmask_yuvy(pY + surf->idx_v, cv, p_a);
		}
		pY+=4;
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);

}

void evg_yuyv_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	s32 i;
	char *pY;

	pY = surf->pixels + y * surf->pitch_y;

	for (i=0; i<count; i++) {
		u8 spanalpha, col_a;
		u32 *p_col;
		char *s_pY;
		u32 len, x;
		len = spans[i].len;
		s_pY = pY + (spans[i].x/2) * 4;
		if (spans[i].x%2) s_pY += 2;

		p_col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;

		x = spans[i].x;

		while (len--) {
			u32 col = *p_col;
			col_a = GF_COL_A(col);
			if (col_a) {
				u32 idx=3*x;
				u8 cy, cb, cr;
				cy = GF_COL_R(col);
				cb = GF_COL_G(col);
				cr = GF_COL_B(col);

				if ((spanalpha!=0xFF) || (col_a != 0xFF)) {
					overmask_yuv420p(col_a, cy, s_pY + surf->idx_y1, spanalpha);

					u8 a = mul255(col_a, spanalpha);
					rctx->uv_alpha[idx] = a;
				} else {
					s_pY[surf->idx_y1] = cy;
					rctx->uv_alpha[idx] = 0xFF;
				}
				rctx->uv_alpha[idx+1] = cb;
				rctx->uv_alpha[idx+2] = cr;
			}
			s_pY+=2;
			p_col++;
			x++;
		}
	}
	pY = surf->pixels + y * surf->pitch_y;
	for (i=0; i<(s32)surf->width; i+=2) {
		u32 a;
		u32 idx1=3*i;
		u32 idx2=3*i + 3;
		//get alpha
		a = (u32)rctx->uv_alpha[idx1] + (u32)rctx->uv_alpha[idx2];
		if (a) {
			u32 chroma;

			a /= 2;

			//get cb
			chroma = (u32)rctx->uv_alpha[idx1+1] + (u32)rctx->uv_alpha[idx2+1];
			chroma /= 2;
			if (a==0xFF) {
				pY[surf->idx_u] = chroma;
			} else {
				overmask_yuvy(pY + surf->idx_u, chroma, a);
			}
			//get cr
			chroma = (u32)rctx->uv_alpha[idx1+2] + (u32)rctx->uv_alpha[idx2+2];
			chroma /= 2;
			if (a==0xFF) {
				pY[surf->idx_v] = chroma;
			} else {
				overmask_yuvy(pY + surf->idx_v, chroma, a);
			}
		}
		pY+=4;
	}

	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);

}

GF_Err evg_surface_clear_yuyv(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	u32 i;
	s32 j;
	u8 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *o_pY;
	char *pY = surf->pixels;
	pY += rc.y * surf->pitch_y;
	pY += (rc.x/2) * 4;

	gf_evg_rgb_to_yuv(surf, col, &cy, &cb, &cr);
	o_pY = pY;
	for (i = 0; i <(u32)rc.height; i++) {
		if (!i) {
			char *dst = pY;
			for (j = 0; j < rc.width/2; j++) {
				dst[surf->idx_y1] = cy;
				dst[surf->idx_u] = cb;
				dst[surf->idx_y1 + 2] = cy;
				dst[surf->idx_v] = cr;
				dst += 4;
			}
		} else {
			memcpy(pY, o_pY, rc.width*2);
		}
		pY += surf->pitch_y;
	}
	return GF_OK;
}


#ifdef GPAC_BIG_ENDIAN

#define set_u16_le(_ptr, val) { ((u8 *)_ptr)[0] = (val>>8)&0xFF;  ((u8 *)_ptr)[1] = (val&0xFF); }
#define set_u16_be(_ptr, val) { *(u16 *) _ptr = (u16) val; }

#define get_u16_le(val, _ptr) { val = ((u32) (*(u8 *) _ptr+1)<< 8) | *(u8 *) _ptr; }
#define get_u16_be(val, _ptr) { val = *(u16 *) (_ptr); }

#else

#define set_u16_le(_ptr, val) { (*(u16 *) _ptr) = (u16) val; }
#define set_u16_be(_ptr, val) { ((u8 *)_ptr)[0] = (val>>8)&0xFF;  ((u8 *)_ptr)[1] = (val&0xFF); }

#define get_u16_le(val, _ptr) { val = *(u16 *) (_ptr); }
#define get_u16_be(val, _ptr) { val = ((u32) (*(u8 *) _ptr)<< 8) | *(u8 *) _ptr+1; }

#endif


/*
			YUV420p-10 part
*/
static s32
mul_10(s64 a, s64 b)
{
	return (s32) ( ((a + 1) * b) >> 16);
}

static s32
mul_10_zero(s64 a, s64 b)
{
	if (!a) return 0;
	return (s32) ( ((a + 1) * b) >> 16);
}

static void overmask_yuv420p_10(u16 col_a, u16 cy, u16 *dst, u32 alpha)
{
	s32 srca = col_a;
	u32 srcc = cy;
	s32 dstc;
	get_u16_le(dstc, dst);

	srca = mul_10(srca, alpha);
	set_u16_le(dst, (mul_10(srca, srcc - dstc) + dstc) );
}

static void overmask_yuv420p_10_const_run(u16 a, u16 val, u16 *ptr, u32 count, short x)
{
	while (count) {
		u16 dst;
		get_u16_le(dst, ptr);
		set_u16_le(ptr, (u16) (mul_10(a, val - dst) + dst));
		ptr ++;
		count--;
	}
}

void evg_yuv420p_10_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	u16 *surf_uv_alpha_even = (u16 *) rctx->uv_alpha;
	u16 *surf_uv_alpha_odd = (u16 *) _surf_uv_alpha;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;
	pU +=  y/2 * surf->pitch_y/2;
	pV = pU + surf->height/2 * surf->pitch_y/2;

	//we are at an odd line, write uv
	for (i=0; i<surf->width; i+=2) {
		u16 dst;

		//even line
		a = surf_uv_alpha_even[i] + surf_uv_alpha_even[i+1];
		//odd line
		a += surf_uv_alpha_odd[i] + surf_uv_alpha_odd[i+1];

		if (a) {
			u16 *s_ptr_u, *s_ptr_v;

			a /= 4;

			s_ptr_u = ((u16 *) pU) + i/2;
			s_ptr_v = ((u16 *) pV) + i/2;
			if (a==0xFFFF) {
				set_u16_le(s_ptr_u, cu);
				set_u16_le(s_ptr_v, cv);
			} else {
				get_u16_le(dst, s_ptr_u);
				set_u16_le(s_ptr_u, (u16) (mul_10(a, cu - dst) + dst) );
				get_u16_le(dst, s_ptr_v);
				set_u16_le(s_ptr_v, (u16) (mul_10(a, cv - dst) + dst) );
			}
		}
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}

void evg_yuv420p_10_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u16 *pY;
	u16 *surf_uv_alpha;
	s32 i;
	u16 cy, cu, cv;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = (u16 *)rctx->uv_alpha;
	}
	else if (write_uv) {
		surf_uv_alpha = ((u16 *)rctx->uv_alpha) + surf->width;
	} else {
		surf_uv_alpha = (u16 *)rctx->uv_alpha;
	}

	pY =  (u16 *) (surf->pixels + y * surf->pitch_y);

	cy = (surf->fill_col_wide>>32) & 0xFFFF;
	cy >>=6;
	cu = (surf->fill_col_wide>>16) & 0xFFFF;
	cu >>=6;
	cv = (surf->fill_col_wide) & 0xFFFF;
	cv >>=6;

	for (i=0; i<count; i++) {
		u32 a;
		u16 *s_pY;
		u32 j, len;
		len = spans[i].len;
		s_pY = pY + spans[i].x;

		a = 0xFF * spans[i].coverage;
		for (j=0; j<len; j++) {
			surf_uv_alpha[spans[i].x + j] = a;
		}

		if (spans[i].coverage != 0xFF) {
			overmask_yuv420p_10_const_run(a, cy, s_pY, len, 0);
		} else  {
			while (len--) {
				set_u16_le(s_pY, cy);
				s_pY ++;
			}
		}
	}
	if (write_uv && !rctx->no_yuv_flush) {
		surf->yuv_flush_uv(surf, rctx, (u8 *)surf_uv_alpha, cu, cv, y);
	}

}

void evg_yuv420p_10_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 a;
	u16 *pY;
	u16 *surf_uv_alpha;
	s32 i;
	u16 cy, cu, cv;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = (u16 *)rctx->uv_alpha;
	} else if (write_uv) {
		surf_uv_alpha = (u16 *)rctx->uv_alpha + surf->width;
	} else {
		surf_uv_alpha = (u16 *)rctx->uv_alpha;
	}

	pY = (u16 *) (surf->pixels + y * surf->pitch_y);


	a = (surf->fill_col_wide>>48) & 0xFFFF;
	cy = (surf->fill_col_wide>>32) & 0xFFFF;
	cy >>=6;
	cu = (surf->fill_col_wide>>16) & 0xFFFF;
	cu >>=6;
	cv = (surf->fill_col_wide) & 0xFFFF;
	cv >>=6;

	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				u16 *s_pY;
				u32 fin;
				s32 x = spans[i].x + j;
				s_pY = pY + x;
				fin = surf->get_alpha(surf->get_alpha_udta, a, x, y) * spans[i].coverage;
				fin /= 0xFF;

				overmask_yuv420p_10_const_run(fin, cy, s_pY, 1, 0);
				surf_uv_alpha[x] = fin;
			}
		}
	} else {
		for (i=0; i<count; i++) {
			u16 *s_pY;
			u32 fin, len, j;
			len = spans[i].len;
			s_pY = pY + spans[i].x;
			fin = a * spans[i].coverage;
			fin /= 0xFF;

			overmask_yuv420p_10_const_run(fin, cy, s_pY, len, 0);

			for (j=0; j<len; j++) {
				surf_uv_alpha[spans[i].x + j] = fin;
			}
		}
	}
	//we are at an odd line, write uv
	if (write_uv && !rctx->no_yuv_flush)
		surf->yuv_flush_uv(surf, rctx, (u8*)surf_uv_alpha, cu, cv, y);
}

void evg_yuv420p_10_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 _cu, s32 _cv,  s32 y)
{
	u32 i;
	u16 *surf_uv_alpha_even = (u16 *) rctx->uv_alpha;
	u16 *surf_uv_alpha_odd = (u16 *) _surf_uv_alpha;
	u16 *pU, *pV;
	pU = (u16 *) (surf->pixels + surf->height *surf->pitch_y + y/2 * surf->pitch_y/2);
	pV = (u16 *) (surf->pixels + 5*surf->height *surf->pitch_y/4 + y/2 * surf->pitch_y/2);

	for (i=0; i<surf->width; i+=2) {
		u32 a, a11, a12, a21, a22;
		u32 idx1=3*i;
		u32 idx2=3*i + 3;

		//get alpha
		a11 = (u32)surf_uv_alpha_even[idx1];
		a12 = (u32)surf_uv_alpha_even[idx2];
		a21 = (u32)surf_uv_alpha_odd[idx1];
		a22 = (u32)surf_uv_alpha_odd[idx2];

		a = a11+a12+a21+a22;
		if (a) {
			s32 cdst=0;
			s32 chroma_u, chroma_v, c11, c12, c21, c22;

			a /= 4;

			//get cb
			idx1 += 1;
			idx2 += 1;

			if (a!=0xFFFF) {
				get_u16_le(cdst, pU);
			}
			c11 = (u32)surf_uv_alpha_even[idx1];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha_even[idx2];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha_odd[idx1];
			if (a21!=0xFFFF) c21 = mul_10_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha_odd[idx2];
			if (a22!=0xFFFF) c22 = mul_10_zero(a22, c22 - cdst) + cdst;

			chroma_u = c11 + c12 + c21 + c22;
			chroma_u /= 4;
			set_u16_le(pU, (u16) chroma_u);

			//get cr
			idx1 += 1;
			idx2 += 1;

			if (a!=0xFFFF) {
				get_u16_le(cdst, pV);
			}
			c11 = (u32)surf_uv_alpha_even[idx1];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha_even[idx2];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha_odd[idx1];
			if (a21!=0xFFFF) c21 = mul_10_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha_odd[idx2];
			if (a22!=0xFFFF) c22 = mul_10_zero(a22, c22 - cdst) + cdst;

			chroma_v = c11 + c12 + c21 + c22;
			chroma_v /= 4;
			set_u16_le(pV, (u16) chroma_v);
		}
		pU++;
		pV++;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);

}

void evg_yuv420p_10_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	s32 i;
	u16 *pY;
	u16 *surf_uv_alpha;
	Bool write_uv = (y%2) ? GF_TRUE : GF_FALSE;

	if (surf->is_422) {
		write_uv = GF_TRUE;
		surf_uv_alpha = (u16 *) rctx->uv_alpha;
	} else if (write_uv) {
		//second line of storage (we store alpha, cr, cb for each pixel)
		surf_uv_alpha =  ((u16 *) rctx->uv_alpha) + 3*surf->width;
	} else {
		//first line of storage
		surf_uv_alpha =  (u16 *) rctx->uv_alpha;
	}

	pY = (u16 *) (surf->pixels + y * surf->pitch_y);

	for (i=0; i<count; i++) {
		u8 spanalpha;
		u32 len;
		u64 *p_col;
		u16 *s_pY;
		short x;
		len = spans[i].len;
		p_col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;

		s_pY = pY + spans[i].x;
		x = spans[i].x;

		while (len--) {
			u64 col = *p_col;
			u32 col_a = (col>>48)&0xFFFF;
			if (col_a) {
				u16 cy, cb, cr;
				u32 idx=3*x;
				//col is directly packed as AYCbCr
				cy = ((col>>32) & 0xFFFF) >> 6;
				cb = ((col>>16) & 0xFFFF) >> 6;
				cr = ((col) & 0xFFFF) >> 6;

				if ((spanalpha!=0xFF) || (col_a != 0xFFFF)) {
					u16 spana = spanalpha;
					spana <<= 8;
					overmask_yuv420p_10(col_a, cy, s_pY, spana);
					surf_uv_alpha[idx] = mul_10(col_a, spana);
				} else {
					set_u16_le(s_pY, cy);
					surf_uv_alpha[idx] = 0xFFFF;
				}
				surf_uv_alpha[idx+1] = cb;
				surf_uv_alpha[idx+2] = cr;
			}
			s_pY++;
			p_col++;
			x++;
		}
	}
	//compute final u,v for both lines
	if (write_uv && !rctx->no_yuv_flush) {
		surf->yuv_flush_uv(surf, rctx, (u8 *)surf_uv_alpha, 0, 0, y);
	}
}

GF_Err evg_surface_clear_yuv420p_10(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i, j;
	u8 _cy, _cb, _cr;
	u16 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	u16 *pY, *pU, *pV, *o_pY, *o_pU, *o_pV;

	pY = (u16 *) (surf->pixels + rc.y * surf->pitch_y + 2*rc.x);
	pU = (u16 *) (surf->pixels + surf->height * surf->pitch_y + rc.y/2 * surf->pitch_y/2 + rc.x);
	pV = (u16 *) (surf->pixels + 5*surf->height * surf->pitch_y/4 + rc.y/2 * surf->pitch_y/2 + rc.x);

	gf_evg_rgb_to_yuv(surf, col, &_cy, &_cb, &_cr);
	cy = ((u16)_cy) << 2;
	cb = ((u16)_cb) << 2;
	cr = ((u16)_cr) << 2;
	o_pY = pY;
	o_pU = pU;
	o_pV = pV;
	for (i = 0; i <rc.height; i++) {
		if (!i) {
			if (cy) {
				for (j=0; j<rc.width; j++)
					set_u16_le(&pY[j] , cy);
			} else {
				memset(pY, 0, rc.width*2);
			}
		} else {
			memcpy(pY, o_pY, rc.width*2);
		}
		//pitch is in bytes, we are in short
		pY += surf->pitch_y/2;

		if (i%2) continue;

		if (!i) {
			for (j=0; j<rc.width/2; j++) {
				set_u16_le(&pU[j], cb);
				set_u16_le(&pV[j], cr);
			}
		} else {
			memcpy(pU, o_pU, rc.width);//half width at 2 bytes per short
			memcpy(pV, o_pV, rc.width);
		}
		//pitch is in bytes, we are in short
		pU += surf->pitch_y/4;
		pV += surf->pitch_y/4;
	}
	return GF_OK;
}

/*
			NV12 / NV21 10 bit part
*/


void evg_nv12_10_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	u16 *surf_uv_alpha_even = (u16 *) rctx->uv_alpha;
	u16 *surf_uv_alpha_odd = (u16 *)_surf_uv_alpha;
	u8 *pUV = surf->pixels + surf->height * surf->pitch_y + y/2 * surf->pitch_y;
	u8 *pU = pUV + 2*surf->idx_u;
	u8 *pV = pUV + 2*surf->idx_v;

	for (i=0; i<surf->width; i+=2) {
		//even line
		a = surf_uv_alpha_even[i] + surf_uv_alpha_even[i+1];
		//odd line
		a += surf_uv_alpha_odd[i] + surf_uv_alpha_odd[i+1];

		if (a) {
			s32 dst;
			a /= 4;

			if (a==0xFFFF) {
				set_u16_le(pU, cu);
				set_u16_le(pV, cv);
			} else {
				get_u16_le(dst, pU);
				set_u16_le(pU, (u16) (mul_10(a, cu - dst) + dst) );
				get_u16_le(dst, pV);
				set_u16_le(pV, (u16) (mul_10(a, cv - dst) + dst) );
			}
		}
		pU += 4;
		pV += 4;
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}


void evg_nv12_10_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i;
	u16 *surf_uv_alpha_even = (u16 *) rctx->uv_alpha;
	u16 *surf_uv_alpha_odd = (u16 *) _surf_uv_alpha;
	u8 *pUV = surf->pixels + surf->height *surf->pitch_y + y/2 * surf->pitch_y;
	u8 *pU = pUV + 2*surf->idx_u;
	u8 *pV = pUV + 2*surf->idx_v;

	for (i=0; i<surf->width; i+=2) {
		u32 a, a11, a12, a21, a22;
		u32 idx1=3*i;
		u32 idx2=3*i + 3;
		//get alpha
		a11 = (u32)surf_uv_alpha_even[idx1];
		a12 = (u32)surf_uv_alpha_even[idx2];
		a21 = (u32)surf_uv_alpha_odd[idx1];
		a22 = (u32)surf_uv_alpha_odd[idx2];
		a = a11+a12+a21+a22;

		if (a) {
			s32 cdst=0, chroma_u, chroma_v, c11, c12, c21, c22;

			a /= 4;

			//get cb
			idx1 += 1;
			idx2 += 1;
			if (a!=0xFFFF)
				get_u16_le(cdst, pU);

			c11 = (u32)surf_uv_alpha_even[idx1];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha_even[idx2];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha_odd[idx1];
			if (a21!=0xFFFF) c21 = mul_10_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha_odd[idx2];
			if (a22!=0xFFFF) c22 = mul_10_zero(a22, c22 - cdst) + cdst;

			chroma_u = c11+c12+c21+c22;
			chroma_u /= 4;

			//get cr
			idx1 += 1;
			idx2 += 1;
			if (a!=0xFFFF)
				get_u16_le(cdst, pV);

			c11 = (u32)surf_uv_alpha_even[idx1];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha_even[idx2];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;
			c21 = (u32)surf_uv_alpha_odd[idx1];
			if (a21!=0xFFFF) c21 = mul_10_zero(a21, c21 - cdst) + cdst;
			c22 = (u32)surf_uv_alpha_odd[idx2];
			if (a22!=0xFFFF) c22 = mul_10_zero(a22, c22 - cdst) + cdst;

			chroma_v = c11+c12+c21+c22;
			chroma_v /= 4;


			set_u16_le(pU, chroma_u);
			set_u16_le(pV, chroma_v);
		}
		pU += 4;
		pV += 4;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}


GF_Err evg_surface_clear_nv12_10(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col, Bool swap_uv)
{
	s32 i, j;
	u8 _cy, _cb, _cr;
	u16 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	u16 *s_pY, *s_pU;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pU_first, *pY_first;

	pY += rc.y * surf->pitch_y;
	pU +=  rc.y/2 * surf->pitch_y;

	pY += 2*rc.x;
	pU += 2*rc.x;
	pU_first = pU;
	pY_first = pY;

	gf_evg_rgb_to_yuv(surf, col, &_cy, &_cb, &_cr);

	if (swap_uv) {
		u8 t = _cb;
		_cb = _cr;
		_cr = t;
	}
	cy = ((u16)_cy) << 2;
	cb = ((u16)_cb) << 2;
	cr = ((u16)_cr) << 2;

	s_pY = (u16 *) pY;
	s_pU = (u16 *) pU;

	//init first lines
	if (cy) {
		for (j=0; j<rc.width;j++) {
			set_u16_le(&s_pY[j], cy);
		}
	} else {
		memset(s_pY, 0, 2*rc.width);
	}
	s_pY += surf->pitch_y/2;

	for (j=0; j<rc.width/2; j++) {
		set_u16_le(&s_pU[2*j], cb);
		set_u16_le(&s_pU[2*j + 1], cr);
	}
	s_pU += surf->pitch_y/2;

	for (i = 1; i < rc.height; i++) {

		memcpy(s_pY, pY_first, 2*rc.width);
		s_pY += surf->pitch_y/2;

		if (i%2) continue;

		//non-first uv line recopy from first (half width, U and V mix, 2 bytes = width*2)
		memcpy(s_pU, pU_first, 2*rc.width);
		s_pU += surf->pitch_y/2;
	}
	return GF_OK;
}



/*
			YUV422 10 part
*/

void evg_yuv422p_10_flush_uv_const(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 cu, s32 cv, s32 y)
{
	u32 i, a;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;
	u16 *surf_uv_alpha = (u16 *) rctx->uv_alpha;
	pU +=  y * surf->pitch_y/2;
	pV = pU + surf->height * surf->pitch_y/2;

	for (i=0; i<surf->width; i+=2) {
		a = surf_uv_alpha[i] + surf_uv_alpha[i+1];

		if (a) {
			s32 dst;
			u16 *s_ptr_u, *s_ptr_v;

			a /= 2;

			s_ptr_u = (u16*)pU + i / 2;
			s_ptr_v = (u16*)pV + i / 2;
			if (a==0xFFFF) {
				set_u16_le(s_ptr_u, (u16) cu);
				set_u16_le(s_ptr_v, (u16) cv);

			} else {
				get_u16_le(dst, s_ptr_u);
				set_u16_le(s_ptr_u, (u16) (mul_10(a, cu - dst) + dst) );

				get_u16_le(dst, s_ptr_v);
				set_u16_le(s_ptr_v, (u16) (mul_10(a, cv - dst) + dst));
			}
		}
	}
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}

void evg_yuv422p_10_flush_uv_var(GF_EVGSurface *surf, EVGRasterCtx *rctx, u8 *_surf_uv_alpha, s32 _cu, s32 _cv,  s32 y)
{
	u32 i;
	u16 *surf_uv_alpha = (u16 *) rctx->uv_alpha;
	u16 *pU = (u16 *) (surf->pixels + surf->height *surf->pitch_y + y * surf->pitch_y/2);
	u16 *pV = (u16 *) (surf->pixels + surf->height *surf->pitch_y + surf->height * surf->pitch_y/2 + y * surf->pitch_y/2);

	for (i=0; i<surf->width; i+=2) {
		u32 a, a11, a12;
		u32 idx1=3*i;
		u32 idx2=3*i + 3;
		//get alpha
		a11 = surf_uv_alpha[idx1];
		a12 = surf_uv_alpha[idx2];
		a=a11+a12;

		if (a) {
			s32 cdst=0, chroma_u, chroma_v, c11, c12;

			a /= 2;

			//get cb
			if (a!=0xFFFF)
				get_u16_le(cdst, pU);

			c11 = (u32)surf_uv_alpha[idx1+1];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha[idx2+1];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;

			chroma_u = c11 + c12;
			chroma_u /= 2;
			set_u16_le(pU, (u16) chroma_u);

			//get cr
			if (a!=0xFFFF)
				get_u16_le(cdst, pV);

			c11 = (u32)surf_uv_alpha[idx1+2];
			if (a11!=0xFFFF) c11 = mul_10_zero(a11, c11 - cdst) + cdst;
			c12 = (u32)surf_uv_alpha[idx2+2];
			if (a12!=0xFFFF) c12 = mul_10_zero(a12, c12 - cdst) + cdst;

			chroma_v = c11 + c12;
			chroma_v /= 2;
			set_u16_le(pV, (u16) chroma_v);
		}
		pU++;
		pV++;
	}
	//reset for next pass
	memset(rctx->uv_alpha, 0, surf->uv_alpha_alloc);
}


GF_Err evg_surface_clear_yuv422p_10(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i, j;
	u8 _cy, _cb, _cr;
	u16 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV;
	char *o_pY, *o_pU, *o_pV;

	pY += rc.y * surf->pitch_y;
	pU +=  rc.y/2 * surf->pitch_y/2;
	pV = pU + surf->height/2 * surf->pitch_y/2;

	pY += 2*rc.x;
	pU += rc.x;
	pV += rc.x;

	o_pY = pY;
	o_pU = pU;
	o_pV = pV;
	gf_evg_rgb_to_yuv(surf, col, &_cy, &_cb, &_cr);

	cy = ((u16)_cy) << 2;
	cb = ((u16)_cb) << 2;
	cr = ((u16)_cr) << 2;

	for (i = 0; i < rc.height; i++) {
		if (!i) {
			for (j=0; j<rc.width; j++) {
				set_u16_le(&((u16 *)pY)[j], cy);
				if (j%2) {
					set_u16_le(&((u16 *)pU)[j/2], cb);
					set_u16_le(& ((u16 *)pV)[j/2], cr);
				}
			}
		} else {
			memcpy(pY, o_pY, 2*rc.width);
			memcpy(pU, o_pU, rc.width);
			memcpy(pV, o_pV, rc.width);
		}
		pY += surf->pitch_y;
		pU += surf->pitch_y/2;
		pV += surf->pitch_y/2;
	}
	return GF_OK;
}


/*
			YUV444 10bits part
*/

void evg_yuv444p_10_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u16 *pY, *pU, *pV;
	s32 i;
	u32 cy, cu, cv;

	pY = (u16 *) (surf->pixels + y * surf->pitch_y);
	pU = (u16 *) (surf->pixels + surf->height*surf->pitch_y + y * surf->pitch_y);
	pV = (u16 *) (surf->pixels + 2*surf->height*surf->pitch_y + y * surf->pitch_y);

	cy = (surf->fill_col_wide>>32) & 0xFFFF;
	cy >>=6;
	cu = (surf->fill_col_wide>>16) & 0xFFFF;
	cu >>=6;
	cv = (surf->fill_col_wide) & 0xFFFF;
	cv >>=6;

	for (i=0; i<count; i++) {
		u32 a;
		u16 *s_pY, *s_pU, *s_pV;
		u32 len;
		len = spans[i].len;
		s_pY = pY + spans[i].x;
		s_pU = pU + spans[i].x;
		s_pV = pV + spans[i].x;

		if (spans[i].coverage != 0xFF) {
			a = 0xFF * spans[i].coverage;

			overmask_yuv420p_10_const_run(a, cy, s_pY, len, 0);
			overmask_yuv420p_10_const_run(a, cu, s_pU, len, 0);
			overmask_yuv420p_10_const_run(a, cv, s_pV, len, 0);
		} else {
			while (len--) {
				set_u16_le(s_pY, cy);
				s_pY ++;
				set_u16_le(s_pU, cu);
				s_pU ++;
				set_u16_le(s_pV, cv);
				s_pV ++;
			}
		}
	}
}

void evg_yuv444p_10_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	u32 a;
	u16 *pY, *pU, *pV;
	s32 i;
	u32 cy, cu, cv;

	pY = (u16 *) (surf->pixels + y * surf->pitch_y);
	pU = (u16 *) (surf->pixels + surf->height*surf->pitch_y + y * surf->pitch_y);
	pV = (u16 *) (surf->pixels + 2*surf->height*surf->pitch_y + y * surf->pitch_y);

	a = (surf->fill_col_wide>>48) & 0xFFFF;
	cy = (surf->fill_col_wide>>32) & 0xFFFF;
	cy >>=6;
	cu = (surf->fill_col_wide>>16) & 0xFFFF;
	cu >>=6;
	cv = (surf->fill_col_wide) & 0xFFFF;
	cv >>=6;

	if (surf->get_alpha) {
		for (i=0; i<count; i++) {
			u32 j;
			for (j=0; j<spans[i].len; j++) {
				u16 *s_pY, *s_pU, *s_pV;
				u32 fin;
				s32 x = spans[i].x + j;
				s_pY = pY + x;
				s_pU = pU + x;
				s_pV = pV + x;

				fin = surf->get_alpha(surf->get_alpha_udta, a, x, y) * spans[i].coverage;
				fin /= 0xFF;

				overmask_yuv420p_10_const_run(fin, cy, s_pY, 1, 0);
				overmask_yuv420p_10_const_run(fin, cu, s_pU, 1, 0);
				overmask_yuv420p_10_const_run(fin, cv, s_pV, 1, 0);
			}
		}
	} else {
		for (i=0; i<count; i++) {
			u16 *s_pY, *s_pU, *s_pV;
			u32 fin, len;
			len = spans[i].len;
			s_pY = pY + spans[i].x;
			s_pU = pU + spans[i].x;
			s_pV = pV + spans[i].x;

			fin = a * spans[i].coverage;
			fin /= 0xFF;

			overmask_yuv420p_10_const_run(fin, cy, s_pY, len, 0);
			overmask_yuv420p_10_const_run(fin, cu, s_pU, len, 0);
			overmask_yuv420p_10_const_run(fin, cv, s_pV, len, 0);
		}
	}
}

void evg_yuv444p_10_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf, EVGRasterCtx *rctx)
{
	s32 i;
	u16 *pY, *pU, *pV;

	pY = (u16 *) (surf->pixels + y * surf->pitch_y);
	pU = (u16 *) (surf->pixels + surf->height*surf->pitch_y + y * surf->pitch_y);
	pV = (u16 *) (surf->pixels + 2*surf->height*surf->pitch_y + y * surf->pitch_y);

	for (i=0; i<count; i++) {
		u8 spanalpha;
		u32 len;
		u64 *p_col;
		u16 *s_pY, *s_pU, *s_pV;
		len = spans[i].len;
		p_col = surf->fill_run(surf->sten, rctx, &spans[i], y);
		spanalpha = spans[i].coverage;

		s_pY = pY + spans[i].x;
		s_pU = pU + spans[i].x;
		s_pV = pV + spans[i].x;

		while (len--) {
			u64 col = *p_col;
			u32 col_a = (col>>48)&0xFFFF;
			if (col_a) {
				u16 cy, cb, cr;
				//col is directly packed as AYCbCr
				cy = ((col>>32) & 0xFFFF) >> 6;
				cb = ((col>>16) & 0xFFFF) >> 6;
				cr = ((col) & 0xFFFF) >> 6;

				if ((spanalpha!=0xFF) || (col_a != 0xFFFF)) {
					u16 spana = spanalpha;
					spana <<= 8;
					overmask_yuv420p_10(col_a, cy, s_pY, spana);
					overmask_yuv420p_10(col_a, cb, s_pU, spana);
					overmask_yuv420p_10(col_a, cr, s_pV, spana);
				} else {
					set_u16_le(s_pY, cy);
					set_u16_le(s_pU, cb);
					set_u16_le(s_pV, cr);
				}
			}
			s_pY++;
			s_pU++;
			s_pV++;
			p_col++;
		}
	}
}

GF_Err evg_surface_clear_yuv444p_10(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col)
{
	s32 i;
	u8 _cy, _cb, _cr;
	u16 cy, cb, cr;
	GF_EVGSurface *surf = (GF_EVGSurface *)_surf;
	char *pY = surf->pixels;
	char *pU = surf->pixels + surf->height *surf->pitch_y;
	char *pV, *o_pY, *o_pU, *o_pV;

	pY += rc.y * surf->pitch_y;
	pU += rc.y * surf->pitch_y;
	pV = pU + surf->height * surf->pitch_y;

	pY += 2*rc.x;
	pU += 2*rc.x;
	pV += 2*rc.x;

	gf_evg_rgb_to_yuv(surf, col, &_cy, &_cb, &_cr);

	cy = ((u16)_cy) << 2;
	cb = ((u16)_cb) << 2;
	cr = ((u16)_cr) << 2;

	o_pY = pY;
	o_pU = pU;
	o_pV = pV;

	for (i = 0; i < rc.height; i++) {
		if (!i) {
			s32 j;
			for (j=0; j<rc.width; j++) {
				set_u16_le(& ((u16 *)pY)[j], cy);
				set_u16_le(& ((u16 *)pU)[j], cb);
				set_u16_le(& ((u16 *)pV)[j], cr);
			}
		} else {
			memcpy(pY, o_pY, 2*rc.width);
			memcpy(pU, o_pU, 2*rc.width);
			memcpy(pV, o_pV, 2*rc.width);
		}
		pY += surf->pitch_y;
		pU += surf->pitch_y;
		pV += surf->pitch_y;
	}
	return GF_OK;
}

#endif //GPAC_DISABLE_EVG
