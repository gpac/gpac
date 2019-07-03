/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / Embedded Vector Graphics engine
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


#ifndef _GF_EVG_H_
#define _GF_EVG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/path2d.h>
#include <gpac/module.h>
#include <gpac/color.h>

/*stencil type used for all stencils*/
typedef struct _gf_evg_base_stencil GF_EVGStencil;
/*surface type*/
typedef struct _gf_evg_surface GF_EVGSurface;


/*stencil types*/
typedef enum
{
	/*solid color stencil*/
	GF_STENCIL_SOLID = 0,
	/*linear color gradient stencil*/
	GF_STENCIL_LINEAR_GRADIENT,
	/*radial color gradient stencil*/
	GF_STENCIL_RADIAL_GRADIENT,
	/*texture stencil*/
	GF_STENCIL_VERTEX_GRADIENT,
	/*texture stencil*/
	GF_STENCIL_TEXTURE,
} GF_StencilType;


/*gradient filling modes*/
typedef enum
{
	/*edge colors are repeated until path is filled*/
	GF_GRADIENT_MODE_PAD,
	/*pattern is inversed each time it's repeated*/
	GF_GRADIENT_MODE_SPREAD,
	/*pattern is repeated to fill path*/
	GF_GRADIENT_MODE_REPEAT
} GF_GradientMode;


/*texture tiling flags*/
typedef enum
{
	/*texture is repeated in its horizontal direction*/
	GF_TEXTURE_REPEAT_S = (1<<1),
	/*texture is repeated in its horizontal direction*/
	GF_TEXTURE_REPEAT_T = (1<<2),
	/*texture is fliped vertically*/
	GF_TEXTURE_FLIP = (1<<3),
} GF_TextureTiling;

/*filter levels for texturing - up to the graphics engine but the following levels are used by
the client*/
typedef enum
{
	/*high speed mapping (ex, no filtering applied)*/
	GF_TEXTURE_FILTER_HIGH_SPEED,
	/*compromise between speed and quality (ex, filter to nearest pixel)*/
	GF_TEXTURE_FILTER_MID,
	/*high quality mapping (ex, bi-linear/bi-cubic interpolation)*/
	GF_TEXTURE_FILTER_HIGH_QUALITY
} GF_TextureFilter;

/* rasterizer antialiasing depending on the graphics engine*/
typedef enum
{
	/*raster shall use no antialiasing */
	GF_RASTER_HIGH_SPEED,
	/*raster should use fast mode and good quality if possible*/
	GF_RASTER_MID,
	/*raster should use full antialiasing*/
	GF_RASTER_HIGH_QUALITY
} GF_RasterLevel;


/*common constructor for all stencil types*/
GF_EVGStencil *gf_evg_stencil_new(GF_StencilType type);

/*common destructor for all stencils*/
void gf_evg_stencil_delete(GF_EVGStencil *stencil);

/*set stencil transformation matrix*/
GF_Err gf_evg_stencil_set_matrix(GF_EVGStencil *stencil, GF_Matrix2D *mat);

/*solid brush - set brush color*/
GF_Err gf_evg_stencil_set_brush_color(GF_EVGStencil *stencil, GF_Color c);
/*gradient brushes*/
/*sets gradient repeat mode - return GF_NOT_SUPPORTED if driver doesn't support this to let the app compute repeat patterns
this may be called before the gradient is setup*/
GF_Err gf_evg_stencil_set_gradient_mode(GF_EVGStencil *stencil, GF_GradientMode mode);
/*set linear gradient.  line is defined by start and end, and you can give interpolation colors at specified positions*/
GF_Err gf_evg_stencil_set_linear_gradient(GF_EVGStencil *stencil, Fixed start_x, Fixed start_y, Fixed end_x, Fixed end_y);
/*radial gradient brush center point, focal point and radius - colors can only be set through set_interpolation */
GF_Err gf_evg_stencil_set_radial_gradient(GF_EVGStencil *stencil, Fixed cx, Fixed cy, Fixed fx, Fixed fy, Fixed x_radius, Fixed y_radius);
/*radial and linear gradient (not used with vertex) - set color interpolation at given points,
	@pos[i]: distance from (center for radial, start for linear) expressed between 0 and 1 (1 being the gradient bounds)
	@col[i]: associated color
NOTE 1: the colors at 0 and 1.0 MUST be provided
NOTE 2: colors shall be fed in order from 0 to 1
NOTE 3: this overrides the colors provided for linear gradient
*/
GF_Err gf_evg_stencil_set_gradient_interpolation(GF_EVGStencil *stencil, Fixed *pos, GF_Color *col, u32 count);

/*sets global alpha blending level for stencil (texture and gradients)
the alpha channel shall be combined with the color matrix if any*/
GF_Err gf_evg_stencil_set_alpha(GF_EVGStencil *stencil, u8 alpha);

/*set stencil texture
	@pixels: texture data, from top to bottom
	@width, @height: texture size
	@stride: texture horizontal pitch (bytes to skip to get to next row)
	@pixelFormat: texture pixel format as defined in file constants.h
	@destination_format_hint: this is the current pixel format of the destination surface, and is given
	as a hint in case the texture needs to be converted by the stencil
	@no_copy: if set, specifies the texture data shall not be cached by the module (eg it must be able
	to directly modify the given memory
NOTE: this stencil acts as a data wrapper, the pixel data is not required to be locally copied
data is not required to be available for texturing until the stencil is used in a draw operation
*/
GF_Err gf_evg_stencil_set_texture(GF_EVGStencil *stencil, u8 *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat);

/*set stencil texture
	@pixels: texture data, from top to bottom
	@width, @height: texture size
	@stride: texture horizontal pitch (bytes to skip to get to next row)
	@pixelFormat: texture pixel format as defined in file constants.h
	@destination_format_hint: this is the current pixel format of the destination surface, and is given
	as a hint in case the texture needs to be converted by the stencil
	@no_copy: if set, specifies the texture data shall not be cached by the module (eg it must be able
	to directly modify the given memory
NOTE: this stencil acts as a data wrapper, the pixel data is not required to be locally copied
data is not required to be available for texturing until the stencil is used in a draw operation
*/
GF_Err gf_evg_stencil_set_texture_planes(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, const u8 *y_or_rgb, u32 stride, const u8 *u_plane, const u8 *v_plane, u32 uv_stride, const u8 *alpha_plane);


/*sets texture tile mode*/
GF_Err gf_evg_stencil_set_tiling(GF_EVGStencil *stencil, GF_TextureTiling mode);
/*sets texture filtering mode*/
GF_Err gf_evg_stencil_set_filter(GF_EVGStencil *stencil, GF_TextureFilter filter_mode);
/*set stencil color matrix - texture stencils only. If matrix is NULL, resets current color matrix*/
GF_Err gf_evg_stencil_set_color_matrix(GF_EVGStencil *stencil, GF_ColorMatrix *cmat);

/*creates surface object*/
/* @center_coords: true indicates mathematical-like coord system,
				   false indicates computer-like coord system */
GF_EVGSurface *gf_evg_surface_new(Bool center_coords);

/* delete surface object */
void gf_evg_surface_delete(GF_EVGSurface *_this);

/* attach surface object to device object (Win32: HDC) width and height are target surface size*/
GF_Err gf_evg_surface_attach_to_device(GF_EVGSurface *_this, void *os_handle, u32 width, u32 height);
/* attach surface object to stencil object*/
GF_Err gf_evg_surface_attach_to_texture(GF_EVGSurface *_this, GF_EVGStencil *sten);
/* attach surface object to memory buffer if supported
	@pixels: texture data
	@width, @height: texture size
	@pitch_x: texture horizontal pitch (bytes to skip to get to next pixel). O means linear frame buffer (eg pitch_x==bytes per pixel)
	@pitch_y: texture vertical pitch (bytes to skip to get to next line)
	@pixelFormat: texture pixel format
*/
GF_Err gf_evg_surface_attach_to_buffer(GF_EVGSurface *_this, u8 *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat);

/*sets rasterizer precision */
GF_Err gf_evg_surface_set_raster_level(GF_EVGSurface *_this, GF_RasterLevel RasterSetting);
/* set the given matrix as the current transformations for all drawn paths
if NULL reset the current transformation */
GF_Err gf_evg_surface_set_matrix(GF_EVGSurface *_this, GF_Matrix2D *mat);
/* set the given rectangle as a clipper - nothing will be drawn outside this clipper
if the clipper is NULL then no clipper is set
NB: the clipper is not affected by the surface matrix and is given in pixels
CF ABOVE NOTE ON CLIPPERS*/
GF_Err gf_evg_surface_set_clipper(GF_EVGSurface *_this, GF_IRect *rc);

/*sets the given path as the current one for drawing - the surface transform is NEVER changed between
setting the path and filling, only the clipper may change*/
GF_Err gf_evg_surface_set_path(GF_EVGSurface *_this, GF_Path *path);
/*fills the current path using the given stencil - can be called several times with the same current path*/
GF_Err gf_evg_surface_fill(GF_EVGSurface *_this, GF_EVGStencil *stencil);

/*clears given pixel rect on the surface with the given color - REQUIRED
the given rect is formatted as a clipper - CF ABOVE NOTE ON CLIPPERS*/
GF_Err gf_evg_surface_clear(GF_EVGSurface *_this, GF_IRect *rc, GF_Color col);


#ifdef __cplusplus
}
#endif


#endif	/*_GF_EVG_H_*/

