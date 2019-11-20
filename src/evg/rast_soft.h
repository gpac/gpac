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

typedef struct __vec2f EVG_Vector;

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
	unsigned short x;
	unsigned short len;
	unsigned char coverage;
	u32 idx1, idx2;
} EVG_Span;

typedef void (*EVG_SpanFunc)(int y, int count, EVG_Span *spans, void *user);
#define EVG_Raster_Span_Func  EVG_SpanFunc


EVG_Raster evg_raster_new();
void evg_raster_del(EVG_Raster raster);
int evg_raster_render(GF_EVGSurface *surf);

GF_Err evg_raster_render_path_3d(GF_EVGSurface *surf);
GF_Err evg_raster_render3d(GF_EVGSurface *surf, u32 *indices, u32 nb_idx, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type);

typedef struct
{
	GF_Matrix proj;
	GF_Matrix modelview;
	//depth buffer
	Float *depth_buffer;

	gf_evg_fragment_shader frag_shader;
	void *frag_shader_udta;
	gf_evg_vertex_shader vert_shader;
	void *vert_shader_udta;
	/*render state*/
	u32 vp_x, vp_y, vp_w, vp_h;
	Bool is_ccw;
	Bool backface_cull;
	Float max_depth;
	Float min_depth;
	Float depth_range;
	Float point_size, line_size;
	Bool smooth_points;
	Bool disable_aa, write_depth, early_depth_test, run_write_depth;
	Bool (*depth_test)(Float depth_buf_value, Float frag_value);
	Bool mode2d;
	Bool clip_zero;
	Float zw_factor, zw_offset;
	/*internal variables for triangle rasterization*/
	GF_EVGPrimitiveType prim_type;
	GF_Vec4 s_v1, s_v2, s_v3;
	Float area;
	
	Float s3_m_s2_x, s3_m_s2_y;
	Float s1_m_s3_x, s1_m_s3_y;
	Float s2_m_s1_x, s2_m_s1_y;
	Float pt_radius;
	Float v1v2_length, v1v3_length, v2v3_length;
	GF_Vec v1v2, v1v3, v2v3;

	struct _gf_evg_base_stencil yuv_sten;

	u32 *pix_vals;
} EVG_Surface3DExt;

typedef enum
{
	EVG_YUV_NONE=0,
	EVG_YUV_444,
	EVG_YUV
} EVG_YUVType;

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

	GF_EVGCompositeMode comp_mode;

	/*in solid color mode to speed things*/
	u32 fill_col;
	u64 fill_col_wide;
	u32 grey_type;

	/*FreeType raster*/
	EVG_Raster raster;

	/*FreeType outline (path converted to ft)*/
	EVG_Outline ftoutline;

	GF_Matrix2D *mx;
	EVG_SpanFunc gray_spans;

	void (*fill_single)(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
	void (*fill_single_a)(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);

	s32 clip_xMin, clip_yMin, clip_xMax, clip_yMax;


	u8 *uv_alpha;
	u32 uv_alpha_alloc;

	Bool swap_uv, not_8bits, is_422;
	EVG_YUVType yuv_type;
	u32 yuv_prof;

	u32 idx_y1, idx_u, idx_v, idx_a, idx_g, idx_r, idx_b;

	u8 (*get_alpha)(void *udta, u8 src_alpha, s32 x, s32 y);
	void *get_alpha_udta;

	Bool is_3d_matrix;
	GF_Matrix mx3d;
	EVG_Surface3DExt *ext3d;
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
	u32 width, height, stride, stride_uv, stride_alpha;
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

	gf_evg_texture_callback tx_callback;
	void *tx_callback_udta;
	Bool tx_callback_screen_coords;
} EVG_Texture;

#define GF_TEXTURE_FILTER_DEFAULT	GF_TEXTURE_FILTER_HIGH_SPEED

void evg_fill_run(GF_EVGStencil *p, GF_EVGSurface *surf, s32 x, s32 y, u32 count);

#define evg_make_col_wide(_a, _r, _g, _b)\
	((u64)(_a)) << 48 | ((u64)(_r))<<32 | ((u64)(_g))<<16 | ((u64)(_b))

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

void evg_make_ayuv_color_mx(GF_ColorMatrix *cmat, GF_ColorMatrix *yuv_cmat);

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


void evg_grey_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_grey_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_alphagrey_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_alphagrey_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_rgb_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_rgb_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_rgbx_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_rgbx_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_argb_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_argb_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_565_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_565_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_555_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_555_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);
void evg_444_fill_single_a(s32 y, s32 x, u8 coverage, u32 col, GF_EVGSurface *surf);
void evg_444_fill_single(s32 y, s32 x, u32 col, GF_EVGSurface *surf);

#include <limits.h>

#define ErrRaster_MemoryOverflow   -4
#define ErrRaster_Invalid_Mode     -2
#define ErrRaster_Invalid_Outline  -1


#define GPAC_FIX_BITS		16
#define PIXEL_BITS  8
#define PIXEL_BITS_DIFF 8	/*GPAC_FIX_BITS - PIXEL_BITS*/

#define ONE_PIXEL       ( 1L << PIXEL_BITS )
#define PIXEL_MASK      ( -1L << PIXEL_BITS )
#define TRUNC( x )      ( (TCoord)((x) >> PIXEL_BITS) )
#define SUBPIXELS( x )  ( (TPos)(x) << PIXEL_BITS )
#define FLOOR( x )      ( (x) & -ONE_PIXEL )
#define CEILING( x )    ( ( (x) + ONE_PIXEL - 1 ) & -ONE_PIXEL )
#define ROUND( x )      ( ( (x) + ONE_PIXEL / 2 ) & -ONE_PIXEL )
#define UPSCALE( x )    ( (x) >> ( PIXEL_BITS_DIFF) )
#define DOWNSCALE( x )  ( (x) << ( PIXEL_BITS_DIFF) )


/*************************************************************************/
/*                                                                       */
/*   TYPE DEFINITIONS                                                    */
/*                                                                       */

/* don't change the following types to FT_Int or FT_Pos, since we might */
/* need to define them to "float" or "double" when experimenting with   */
/* new algorithms                                                       */

typedef int   TCoord;   /* integer scanline/pixel coordinate */
typedef long  TPos;     /* sub-pixel coordinate              */

/* determine the type used to store cell areas.  This normally takes at */
/* least PIXEL_BYTES*2 + 1.  On 16-bit systems, we need to use `long'   */
/* instead of `int', otherwise bad things happen                        */

/* approximately determine the size of integers using an ANSI-C header */
#if UINT_MAX == 0xFFFFU
typedef long  TArea;
#else
typedef int  TArea;
#endif


/* maximal number of gray spans in a call to the span callback */
#define FT_MAX_GRAY_SPANS  64

#define AA_CELL_STEP_ALLOC	8

typedef struct  TCell_
{
	TCoord  x;
	int     cover;
	TArea   area;
	u32 idx1, idx2;
} AACell;

typedef struct
{
	TCoord  x;
	u32 color;
	int cover;
	Float depth;
	Float write_depth;
	u32 idx1, idx2;
} PatchPixel;

typedef struct
{
	//current cells in sweep
	AACell *cells;
	int alloc, num;

	/*list of candidates for border pixels (visible edges), flushed at end of shape
	 this ensures that a pixel is draw once and only once
	*/
	PatchPixel *pixels;
	int palloc, pnum;
} AAScanline;


typedef struct  TRaster_
{
	AAScanline *scanlines;
	int max_lines;
	TPos min_ex, max_ex, min_ey, max_ey;
	TCoord ex, ey;
	TPos x,  y, last_ey;
	TArea area;
	int cover;
	u32 idx1, idx2;

	EVG_Span *gray_spans;
	int num_gray_spans;
	u32 max_gray_spans, alloc_gray_spans;
	EVG_Raster_Span_Func  render_span;
	void *render_span_data;

	u32 first_scanline;

	GF_Matrix2D *mx;
} TRaster;

void gray_record_cell( TRaster *raster );
void gray_set_cell( TRaster *raster, TCoord  ex, TCoord  ey );
void gray_render_line(TRaster *raster, TPos  to_x, TPos  to_y);
void gray_quick_sort( AACell *cells, int count);
void gray_sweep_line( TRaster *raster, AAScanline *sl, int y, Bool zero_non_zero_rule);

#endif

