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


#ifndef _RAST_SOFT_H_
#define _RAST_SOFT_H_

#include <gpac/modules/raster2d.h>

#ifdef __cplusplus
extern "C" {
#endif


/*RGB 555 support is disabled by default*/
//#define GF_RGB_555_SUPORT

/*for symbian enable 4k color depth (RGB444, 4096 colors) since a good amount of devices use that*/
#ifdef __SYMBIAN32__
#define GF_RGB_444_SUPORT
#endif


typedef struct _evg_surface EVGSurface;

/*base stencil stack*/
#define EVGBASESTENCIL	\
	u32 type;	\
	void (*fill_run)(struct _evg_base_stencil *p, EVGSurface *surf, s32 x, s32 y, u32 count);	\
	GF_Matrix2D pmat;					\
	GF_Matrix2D smat;					\
	GF_Rect frame;					\
	GF_ColorMatrix cmat;			\

typedef struct _evg_base_stencil
{
	EVGBASESTENCIL
} EVGStencil;

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
struct _evg_surface
{
	/*surface info*/
	char *pixels;
	u32 pixelFormat, BPP;
	u32 width, height;
	s32 pitch_x, pitch_y;
	Bool center_coords;

	/*color buffer for variable stencils - size of width*/
	u32 *stencil_pix_run;

	/*default texture filter level*/
	u32 texture_filter;

	u32 useClipper;
	GF_IRect clipper;

	GF_Rect path_bounds;

	/*complete path transform (including page flipping/recenter)*/
	GF_Matrix2D mat;

	/*stencil currently used*/
	EVGStencil *sten;

	void *raster_cbk;
	/*fills line pixels without any blending operation*/
	void (*raster_fill_run_no_alpha)(void *cbk, u32 x, u32 y, u32 run_h_len, GF_Color color);
	/*fills line pixels with blending operation - alpha combines both fill color and anti-aliasing blending*/
	void (*raster_fill_run_alpha)(void *cbk, u32 x, u32 y, u32 run_h_len, GF_Color color, u8 alpha);
	/*fills rectangle with or without blending operation - alpha combines both fill color and anti-aliasing blending*/
	void (*raster_fill_rectangle)(void *cbk, u32 x, u32 y, u32 width, u32 height, GF_Color color);


	/*in solid color mode to speed things*/
	u32 fill_col;
	u32 fill_565;

#ifdef GF_RGB_444_SUPORT
	u32 fill_444;
#endif

#ifdef GF_RGB_555_SUPORT
	u32 fill_555;
#endif

	/*FreeType raster*/
	EVG_Raster raster;

	/*FreeType outline (path converted to ft)*/
	EVG_Outline ftoutline;
	EVG_Raster_Params ftparams;

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
		u32	pre[(1<<EVGGRADIENTBITS)];	\
		u32	col[EVGGRADIENTSLOTS];		\
		Fixed pos[EVGGRADIENTSLOTS];	\
		u8 alpha;		\

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
typedef struct
{
	EVGBASESTENCIL
	u32 width, height, stride;
	u32 pixel_format, Bpp;
	char *pixels;

	GF_Point2D cur_pt;
	Fixed cur_y, inc_x, inc_y;

	u32 mod, filter;
	u32 replace_col;
	Bool cmat_is_replace;

	u8 alpha;

	/*YUV->RGB local buffer*/
	unsigned char *conv_buf;
	u32 conv_size;
	unsigned char *orig_buf;
	u32 orig_stride, orig_format;
	Bool is_converted;

	Bool owns_texture;

	u32 (*tx_get_pixel)(char *pix);
} EVG_Texture;

#define GF_TEXTURE_FILTER_DEFAULT	GF_TEXTURE_FILTER_HIGH_SPEED


void evg_radial_init(EVG_RadialGradient *_this) ;
void evg_bmp_init(EVGStencil *p);
void evg_set_texture_active(EVGStencil *st);


GF_STENCIL evg_stencil_new(GF_Raster2D *, GF_StencilType type);
void evg_stencil_delete(GF_STENCIL st);
GF_Err evg_stencil_set_matrix(GF_STENCIL st, GF_Matrix2D *mx);
GF_Err evg_stencil_set_brush_color(GF_STENCIL st, GF_Color c);
GF_Err evg_stencil_set_linear_gradient(GF_STENCIL st, Fixed start_x, Fixed start_y, Fixed end_x, Fixed end_y);
GF_Err evg_stencil_set_radial_gradient(GF_STENCIL st, Fixed cx, Fixed cy, Fixed fx, Fixed fy, Fixed x_radius, Fixed y_radius);
GF_Err evg_stencil_set_gradient_interpolation(GF_STENCIL p, Fixed *pos, GF_Color *col, u32 count);
GF_Err evg_stencil_set_gradient_mode(GF_STENCIL p, GF_GradientMode mode);

GF_Err evg_stencil_set_alpha(GF_STENCIL st, u8 alpha);

/*texture only*/
GF_Err evg_stencil_set_texture(GF_STENCIL st, char *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat, GF_PixelFormat destination_format_hint, Bool no_copy);
GF_Err evg_stencil_set_tiling(GF_STENCIL st, GF_TextureTiling mode);
GF_Err evg_stencil_set_filter(GF_STENCIL st, GF_TextureFilter filter_mode);
GF_Err evg_stencil_set_color_matrix(GF_STENCIL st, GF_ColorMatrix *cmat);
GF_Err evg_stencil_reset_color_matrix(GF_STENCIL st);
GF_Err evg_stencil_create_texture(GF_STENCIL st, u32 width, u32 height, GF_PixelFormat pixelFormat);



GF_SURFACE evg_surface_new(GF_Raster2D *, Bool center_coords);
void evg_surface_delete(GF_SURFACE _this);
void evg_surface_detach(GF_SURFACE _this);
GF_Err evg_surface_attach_to_buffer(GF_SURFACE _this, char *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat);
GF_Err evg_surface_attach_to_texture(GF_SURFACE _this, GF_STENCIL sten);
GF_Err evg_surface_attach_to_callbacks(GF_SURFACE _this, GF_RasterCallback *callbacks, u32 width, u32 height);

GF_Err evg_surface_set_matrix(GF_SURFACE surf, GF_Matrix2D *mat);
GF_Err evg_surface_set_raster_level(GF_SURFACE surf , GF_RasterLevel RasterSetting);
GF_Err evg_surface_set_clipper(GF_SURFACE surf, GF_IRect *rc);
GF_Err evg_surface_set_path(GF_SURFACE surf, GF_Path *gp);
GF_Err evg_surface_fill(GF_SURFACE surf, GF_STENCIL stencil);
GF_Err evg_surface_clear(GF_SURFACE surf, GF_IRect *rc, u32 color);


/*FT raster callbacks */
void evg_bgra_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgra_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgra_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_bgra(GF_SURFACE surf, GF_IRect rc, GF_Color col);

void evg_rgba_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgba_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgba_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgba_fill_erase(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_rgba(GF_SURFACE surf, GF_IRect rc, GF_Color col);

void evg_bgrx_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgrx_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgrx_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);

void evg_rgbx_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgbx_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgbx_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_rgbx(GF_SURFACE surf, GF_IRect rc, GF_Color col);

void evg_rgb_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgb_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_rgb_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_rgb(GF_SURFACE surf, GF_IRect rc, GF_Color col);

void evg_bgr_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgr_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_bgr_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_bgr(GF_SURFACE surf, GF_IRect rc, GF_Color col);

void evg_565_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_565_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_565_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_565(GF_SURFACE _this, GF_IRect rc, GF_Color col);

#ifdef GF_RGB_444_SUPORT
void evg_444_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_444_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_444_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_444(GF_SURFACE surf, GF_IRect rc, GF_Color col);
#endif

#ifdef GF_RGB_555_SUPORT
void evg_555_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_555_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_555_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_555(GF_SURFACE surf, GF_IRect rc, GF_Color col);
#endif

void evg_user_fill_const(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_user_fill_const_a(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
void evg_user_fill_var(s32 y, s32 count, EVG_Span *spans, EVGSurface *surf);
GF_Err evg_surface_clear_user(GF_SURFACE surf, GF_IRect rc, GF_Color col);




#ifdef __cplusplus
}
#endif

#endif 

