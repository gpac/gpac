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


#ifndef _GF_EVG_DEV_H_
#define _GF_EVG_DEV_H_

#include <gpac/evg.h>

/*base stencil stack*/
#define EVGBASESTENCIL	\
	u32 type;	\
	void (*fill_run)(GF_EVGStencil *p, GF_EVGSurface *surf, s32 x, s32 y, u32 count);	\
	GF_Matrix2D pmat;					\
	GF_Matrix2D smat;					\
	GF_Rect frame;					\
	GF_ColorMatrix cmat;



struct _gf_evg_base_stencil
{
	EVGBASESTENCIL
};

/*define this macro to convert points in the outline decomposition rather than when attaching path
to surface. When point conversion is inlined in the raster, memory usage is lower (no need for temp storage)*/
#define INLINE_POINT_CONVERSION

#ifdef INLINE_POINT_CONVERSION
typedef struct __vec2f EVG_Vector;
#else
typedef struct
{
	s32 x;
	s32 y;
} EVG_Vector;
#endif

typedef struct
{
	s32 n_contours;
	s32 n_points;
	EVG_Vector *points;
	u8 *tags;
	s32 *contours;
	s32 flags; /*same as path flags*/
} EVG_Outline;

typedef struct TRaster_ *EVG_Raster;

typedef struct EVG_Span_
{
	short           x;
	unsigned short  len;
	unsigned char   coverage;
} EVG_Span;

typedef void (*EVG_SpanFunc)(int y, int count, EVG_Span *spans, void *user);
#define EVG_Raster_Span_Func  EVG_SpanFunc

typedef struct EVG_Raster_Params_
{
	EVG_Outline *source;
#ifdef INLINE_POINT_CONVERSION
	GF_Matrix2D *mx;
#endif
	EVG_SpanFunc gray_spans;

	s32 clip_xMin, clip_yMin, clip_xMax, clip_yMax;

	void *user;
} EVG_Raster_Params;


EVG_Raster evg_raster_new();
void evg_raster_del(EVG_Raster raster);
int evg_raster_render(EVG_Raster raster, EVG_Raster_Params *params);

/*the surface object - currently only ARGB/RGB32, RGB/BGR and RGB555/RGB565 supported*/
struct _gf_evg_surface
{
	/*surface info*/
	char *pixels;
	u32 pixelFormat, BPP;
	u32 width, height;
	s32 pitch_x, pitch_y;
	Bool center_coords;

	/*color buffer for variable stencils - size of width * 32 bit for 8 bit surface or width * 64 otherwise*/
	void *stencil_pix_run;

	/*default texture filter level*/
	u32 texture_filter;

	u32 useClipper;
	GF_IRect clipper;

	GF_Rect path_bounds;

	/*complete path transform (including page flipping/recenter)*/
	GF_Matrix2D mat;

	/*stencil currently used*/
	GF_EVGStencil *sten;

	void (*yuv_flush_uv)(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);


	/*in solid color mode to speed things*/
	u32 fill_col;
	u64 fill_col_wide;
	u32 grey_type;

	/*FreeType raster*/
	EVG_Raster raster;

	/*FreeType outline (path converted to ft)*/
	EVG_Outline ftoutline;
	EVG_Raster_Params ftparams;

	u8 *uv_alpha;
	u32 uv_alpha_alloc;

	Bool swap_uv, is_422, is_yuv, not_8bits;
	u32 yuv_prof;

	u32 idx_y1, idx_u, idx_v, idx_a, idx_g, idx_r, idx_b;

#ifndef INLINE_POINT_CONVERSION
	/*transformed point list*/
	u32 pointlen;
	EVG_Vector *points;
#endif
};

/*solid color brush*/
typedef struct
{
	EVGBASESTENCIL
	GF_Color color;
} EVG_Brush;

/*max number of interpolation points*/
#define EVGGRADIENTSLOTS		12

/*(2^EVGGRADIENTBITS)-1 colors in gradient. Below 6 bits the gradient is crappy */
#define EVGGRADIENTBITS		10


/*base gradient stencil*/
#define EVGGRADIENT	\
		s32	mod;	\
		u32	precomputed_argb[(1<<EVGGRADIENTBITS)];	\
		u32	col[EVGGRADIENTSLOTS];		\
		Fixed pos[EVGGRADIENTSLOTS];	\
		u32	precomputed_dest[(1<<EVGGRADIENTBITS)];	\
		u32 yuv_prof; \
		u8 alpha;		\
		u8 updated;		\

typedef struct
{
	EVGBASESTENCIL
	EVGGRADIENT
} EVG_BaseGradient;

/*linear gradient*/
typedef struct
{
	EVGBASESTENCIL
	EVGGRADIENT

	GF_Point2D start, end;
	GF_Matrix2D vecmat;
	s32	curp;
	Fixed pos_ft;
} EVG_LinearGradient;

/*radial gradient*/
typedef struct
{
	EVGBASESTENCIL
	EVGGRADIENT

	GF_Point2D center, focus, radius;
	/*drawing state vars*/
	GF_Point2D cur_p, d_f, d_i;
	Fixed rad;
} EVG_RadialGradient;

/*texture stencil - should be reworked, is very slow*/
typedef struct __evg_texture
{
	EVGBASESTENCIL
	u32 width, height, stride, stride_uv;
	u32 pixel_format, Bpp;
	char *pixels;
	char *pix_u, *pix_v, *pix_a;

	GF_Point2D cur_pt;
	Fixed cur_y, inc_x, inc_y;

	u32 mod, filter;
	u32 replace_col;
	Bool cmat_is_replace;

	u8 alpha;
	Bool is_yuv;

	Bool owns_texture;

	GF_ColorMatrix yuv_cmat;

	u32 (*tx_get_pixel)(struct __evg_texture *_this, u32 x, u32 y);
	u64 (*tx_get_pixel_wide)(struct __evg_texture *_this, u32 x, u32 y);
} EVG_Texture;

#define GF_TEXTURE_FILTER_DEFAULT	GF_TEXTURE_FILTER_HIGH_SPEED


u64 evg_col_to_wide( u32 col);

void evg_gradient_precompute(EVG_BaseGradient *grad, GF_EVGSurface *surf);
void evg_radial_init(EVG_RadialGradient *_this);
void evg_texture_init(GF_EVGStencil *p, GF_EVGSurface *surf);

void evg_argb_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_argb_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_argb_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_argb_fill_erase(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_argb(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_rgbx_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_rgbx_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_rgbx_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_rgbx(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_rgb_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_rgb_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_rgb_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_rgb(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_grey_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_grey_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_grey_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_grey(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_alphagrey_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_alphagrey_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_alphagrey_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_alphagrey(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_rgb_to_yuv(GF_EVGSurface *surf, GF_Color col, u8*y, u8*cb, u8*cr);
GF_Color evg_argb_to_ayuv(GF_EVGSurface *surf, GF_Color col);
GF_Color evg_ayuv_to_argb(GF_EVGSurface *surf, GF_Color col);
u64 evg_argb_to_ayuv_wide(GF_EVGSurface *surf, u64 col);
u64 evg_ayuv_to_argb_wide(GF_EVGSurface *surf, u64 col);
void evg_make_ayuv_color_mx(GF_ColorMatrix *cmat, GF_ColorMatrix *yuv_cmat);

u64 evg_make_col_wide(u16 a, u16 r, u16 g, u16 b);

void evg_yuv420p_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv420p_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv420p_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_yuv420p(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);
void evg_yuv420p_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_yuv420p_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);

GF_Err evg_surface_clear_nv12(GF_EVGSurface *surf, GF_IRect rc, GF_Color col, Bool swap_uv);
void evg_nv12_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_nv12_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);

GF_Err evg_surface_clear_yuv422p(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);
void evg_yuv422p_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_yuv422p_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);

void evg_yuv444p_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv444p_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv444p_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_yuv444p(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_565_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_565_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_565_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_565(GF_EVGSurface *_this, GF_IRect rc, GF_Color col);


void evg_444_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_444_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_444_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_444(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_555_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_555_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_555_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_555(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

void evg_user_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_user_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_user_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);


void evg_yuyv_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuyv_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuyv_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_yuyv(GF_EVGSurface *_surf, GF_IRect rc, GF_Color col);

void evg_yuv420p_10_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv420p_10_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv420p_10_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_yuv420p_10(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);
void evg_yuv420p_10_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_yuv420p_10_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);

GF_Err evg_surface_clear_nv12_10(GF_EVGSurface *surf, GF_IRect rc, GF_Color col, Bool swap_uv);
void evg_nv12_10_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_nv12_10_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);


GF_Err evg_surface_clear_yuv422p_10(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);
void evg_yuv422p_10_flush_uv_const(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);
void evg_yuv422p_10_flush_uv_var(GF_EVGSurface *surf, u8 *surf_uv_alpha, s32 cu, s32 cv, s32 y);

void evg_yuv444p_10_fill_const(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv444p_10_fill_const_a(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
void evg_yuv444p_10_fill_var(s32 y, s32 count, EVG_Span *spans, GF_EVGSurface *surf);
GF_Err evg_surface_clear_yuv444p_10(GF_EVGSurface *surf, GF_IRect rc, GF_Color col);

#endif

