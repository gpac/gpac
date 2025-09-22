/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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


/*!
\file "gpac/evg.h"
\brief 2D vector graphics rasterizer

This file contains all defined functions for 2D vector graphics of the GPAC framework.
*/

/*!
\addtogroup evg_grp
\brief Vector Graphics rendering of GPAC.

GPAC uses a software rasterizer for vector graphics based on FreeType's "ftgray" anti-aliased renderer.

The rasterizer supports
- drawing to RGB, YUV and grayscale surfaces.
- Texture mapping using RGB, YUV and grayscale textures
- Linear Gradients with opacity
- Radial Gradients with opacity
- Solid brush with opacity
- axis-aligned clipping

The rasterizer is only capable of filling a given GF_Path object
- striking (outlining) of a path must be done by using \ref gf_path_get_outline to the the outline path
- text must be first converted to GF_Path

The rasterizer also supports experimental drawing of 3D primitives (point, lines, triangles). This is obviously not intended to replace GPU rendering, but can
be useful to draw simple primitives over an existing video source. The following constraints apply in 3D mode:
- stencils are not used
- path can only be drawn using \ref gf_evg_surface_draw_path

@{
*/

/*! stencil type used for all stencils*/
typedef struct _gf_evg_base_stencil GF_EVGStencil;
/*! surface type*/
typedef struct _gf_evg_surface GF_EVGSurface;


/*! stencil types*/
typedef enum
{
	/*! solid color stencil*/
	GF_STENCIL_SOLID = 0,
	/*! linear color gradient stencil*/
	GF_STENCIL_LINEAR_GRADIENT,
	/*! radial color gradient stencil*/
	GF_STENCIL_RADIAL_GRADIENT,
	/*! texture stencil*/
	GF_STENCIL_VERTEX_GRADIENT,
	/*! texture stencil*/
	GF_STENCIL_TEXTURE,
} GF_StencilType;

/*! gradient filling modes*/
typedef enum
{
	/*! edge colors are repeated until path is filled*/
	GF_GRADIENT_MODE_PAD,
	/*! pattern is inversed each time it's repeated*/
	GF_GRADIENT_MODE_SPREAD,
	/*! pattern is repeated to fill path*/
	GF_GRADIENT_MODE_REPEAT
} GF_GradientMode;

/*! texture map flags*/
typedef enum
{
	/*! texture is repeated in its horizontal direction*/
	GF_TEXTURE_REPEAT_S = (1<<1),
	/*! texture is repeated in its horizontal direction*/
	GF_TEXTURE_REPEAT_T = (1<<2),
	/*! texture is fliped horizontally*/
	GF_TEXTURE_FLIP_X = (1<<3),
	/*! texture is fliped vertically*/
	GF_TEXTURE_FLIP_Y = (1<<4),
} GF_TextureMapFlags;

/*! filter levels for texturing - up to the graphics engine but the following levels are used by
the client*/
typedef enum
{
	/*! high speed mapping (ex, no filtering applied)*/
	GF_TEXTURE_FILTER_HIGH_SPEED,
	/*! compromise between speed and quality (ex, filter to nearest pixel)*/
	GF_TEXTURE_FILTER_MID,
	/*! high quality mapping (ex, bi-linear/bi-cubic interpolation)*/
	GF_TEXTURE_FILTER_HIGH_QUALITY
} GF_TextureFilter;

/*! rasterizer antialiasing depending on the graphics engine*/
typedef enum
{
	/*! raster shall use no antialiasing */
	GF_RASTER_HIGH_SPEED,
	/*! raster should use fast mode and good quality if possible*/
	GF_RASTER_MID,
	/*! raster should use full antialiasing - this is the default for all new surfaces*/
	GF_RASTER_HIGH_QUALITY
} GF_RasterQuality;

#ifndef GPAC_DISABLE_EVG


/*! common constructor for all stencil types
\param type the stencil type
\return a new stencil object*/
GF_EVGStencil *gf_evg_stencil_new(GF_StencilType type);

/*! common destructor for all stencils
\param stencil the target stencil
*/
void gf_evg_stencil_delete(GF_EVGStencil *stencil);

/*! sets stencil transformation matrix
\param stencil the target stencil
\param mat the 2D matrix to set
\return error if any
*/
GF_Err gf_evg_stencil_set_matrix(GF_EVGStencil *stencil, GF_Matrix2D *mat);

/*! gets stencil transformation matrix
\param stencil the target stencil
\param mat the 2D matrix to fetch
\return TRUE if resulting matrix is fetched, false otherwise (matrix is identity or does not apply to this stencil class)
*/
Bool gf_evg_stencil_get_matrix(GF_EVGStencil * stencil, GF_Matrix2D *mat);

/*! sets stencil transformation matrix auto mode.

When auto matrix mode is activated, the stencil matrix only describes texture mapping transform in uv space (normalized UV coordinates, OpenGL like),
and final transformation to raster coordinates is done internally.
When auto matrix mode is not activated,, the stencil matrix must describe the complete transformation from uv space to raster coordinates (consequently, the matrix usually includes the final path transformation).

Stencils are by default created in auto matrix mode.

\param stencil the target stencil
\param auto_on  If true,  the surface current matrix will be added to the stencil matrix when drawing, otherwise the stencil matrix is in final surface coordinates
\return error if any
*/
GF_Err gf_evg_stencil_set_auto_matrix(GF_EVGStencil * stencil, Bool auto_on);

/*! sets stencil transformation matrix auto mode.
\param stencil the target stencil
\return true if auto mode is enabled, false otherwise
*/
Bool gf_evg_stencil_get_auto_matrix(GF_EVGStencil * stencil);

/*! gets stencil type
\param stencil the target stencil
\return stencil type
*/
GF_StencilType gf_evg_stencil_type(GF_EVGStencil *stencil);

/*! sets color for solid brush stencil
\param stencil the target stencil
\param color the color to set
\return error if any
*/
GF_Err gf_evg_stencil_set_brush_color(GF_EVGStencil *stencil, GF_Color color);

/*! gets color for solid brush stencil
\param stencil the target stencil
\return the stencil color
*/
GF_Color gf_evg_stencil_get_brush_color(GF_EVGStencil * stencil);

/*! sets gradient repeat mode for a gradient stencil
\note this may be called before the gradient is setup
\param stencil the target stencil
\param mode the gradient mode
\return error if any
*/
GF_Err gf_evg_stencil_set_gradient_mode(GF_EVGStencil *stencil, GF_GradientMode mode);
/*! sets linear gradient direction for a gradient stencil
Gradient line is defined by start and end. For example, {0,0}, {1,0} defines an horizontal gradient
\param stencil the target stencil
\param start_x horizontal coordinate of starting point
\param start_y vertical coordinate of starting point
\param end_x horizontal coordinate of ending point
\param end_y vertical coordinate of ending point
\return error if any
*/
GF_Err gf_evg_stencil_set_linear_gradient(GF_EVGStencil *stencil, Fixed start_x, Fixed start_y, Fixed end_x, Fixed end_y);
/*! sets radial gradient center point, focal point and radius for a gradient stencil
\param stencil the target stencil
\param cx horizontal coordinate of center
\param cy vertical coordinate of center
\param fx horizontal coordinate of focal point
\param fy vertical coordinate of focal point
\param x_radius horizontal radius
\param y_radius vertical radius
\return error if any
*/
GF_Err gf_evg_stencil_set_radial_gradient(GF_EVGStencil *stencil, Fixed cx, Fixed cy, Fixed fx, Fixed fy, Fixed x_radius, Fixed y_radius);

/*! sets color interpolation for a gradient stencil
\note the colors at 0 and 1.0 MUST be provided
\note colors shall be fed in order from 0 to 1

\param stencil the target stencil
\param pos interpolation positions. Each position gives the distance from (center for radial, start for linear) expressed between 0 and 1 (1 being the gradient bounds) - may be NULL when count is 0
\param col colors at the given position - may be NULL when count is 0
\param count number of colors and position. A value of 0 resets the gradient colors
\return error if any
*/
GF_Err gf_evg_stencil_set_gradient_interpolation(GF_EVGStencil *stencil, Fixed *pos, GF_Color *col, u32 count);

/*! pushes a single color interpolation for a gradient stencil
\note the colors at 0 and 1.0 MUST be provided
\note colors shall be fed in order from 0 to 1

\param stencil the target stencil
\param pos interpolation position. A position gives the distance from (center for radial, start for linear) expressed between 0 and 1 (1 being the gradient bounds)
\param col color at the given position
\return error if any
*/
GF_Err gf_evg_stencil_push_gradient_interpolation(GF_EVGStencil *stencil, Fixed pos, GF_Color col);

/*! sets global alpha blending level for a stencil
The alpha channel will be combined with the color matrix if any
\warning do not use with solid brush stencil
\param stencil the target stencil
\param alpha the alpha value between 0 (full transparency) and 255 (full opacityy)
\return error if any
*/
GF_Err gf_evg_stencil_set_alpha(GF_EVGStencil *stencil, u8 alpha);

/*! gets global alpha blending level of a stencil
\param stencil the target stencil
\return the alpha value between 0 (full transparency) and 255 (full opacityy)
*/
u8 gf_evg_stencil_get_alpha(GF_EVGStencil *stencil);

/*! checks if pixel format is supported for a texture stencil
\param pixelFormat the target pixel format
\return GF_TRUE if supported, GF_FALSE otherwise
*/
Bool gf_evg_texture_format_ok(GF_PixelFormat pixelFormat);


/*! sets pixel data for a texture stencil
\param stencil the target stencil
\param pixels texture data starting from top row
\param width texture width in pixels
\param height texture height in pixels
\param stride texture horizontal pitch (bytes to skip to get to next row)
\param pixelFormat texture pixel format
\note this stencil acts as a data wrapper, the pixel data is not locally copied
\return error if any
*/
GF_Err gf_evg_stencil_set_texture(GF_EVGStencil *stencil, u8 *pixels, u32 width, u32 height, u32 stride, GF_PixelFormat pixelFormat);

/*! sets pixel planes data for a texture stencil
\param stencil the target stencil
\param width texture width in pixels
\param height texture height in pixels
\param pixelFormat texture pixel format
\param y_or_rgb Y plane or RGB plane
\param stride stride of the Y or RGB plane
\param u_plane U plane or UV plane for semi-planar YUV formats
\param v_plane V plane for planar YUV format, NULL for semi-planar YUV formats
\param uv_stride stride of the U and V or UV plane
\param alpha_plane alpha plane
\param alpha_stride stride of the alpha plane; if 0 and alpha plane is not NULL, the stride is assumed to be the same as the Y plane stride
\note this stencil acts as a data wrapper, the pixel data is not locally copied
\return error if any
*/
GF_Err gf_evg_stencil_set_texture_planes(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, const u8 *y_or_rgb, u32 stride, const u8 *u_plane, const u8 *v_plane, u32 uv_stride, const u8 *alpha_plane, u32 alpha_stride);


/*! sets texture palette, only for greyscale textures.
\param stencil the target stencil
\param palette palette data
\param pix_fmt palette pixel format, must be RGB/BGR or any RGBA format
\param nb_cols number of colors in palette
\return error if any
*/
GF_Err gf_evg_stencil_set_palette(GF_EVGStencil *stencil, const u8 *palette, u32 pix_fmt, u32 nb_cols);

/*! callback function prototype for parametric textures
\param cbk user data callback
\param x horizontal coordinate in texture, ranging between 0 and width-1, (0,0) being top-left pixel, or horizontal coordinate of destination pixel on surface
\param y vertical coordinate in texture, ranging between 0 and height-1, (0,0) being top-left pixel, or vertical coordinate of destination pixel on surface
\param r set to the red value (not initialized before call)
\param g set to the green value (not initialized before call)
\param b set to the blue value (not initialized before call)
\param a set to the alpha value (not initialized before call)
*/
typedef void (*gf_evg_texture_callback)(void *cbk, u32 x, u32 y, Float *r, Float *g, Float *b, Float *a);

/*! sets parametric callback on a texture stencil. A parametric texture gets its pixel values from a callback function, the resulting value being blended according to the antialiasing level of the pixel. This allows creating rather complex custom textures, in a fashion similar to fragment shaders.
\param stencil the target stencil
\param width texture width in pixels
\param height texture height in pixels
\param pixelFormat texture pixel format, indicates if the returned values are in RGB or YUV
\param callback the callback function to use
\param cbk_udta user data to pass to the callback function
\param use_screen_coords if set, the coordinates passed to the callback function are screen coordinates rather than texture coordinates
\return error if any
*/
GF_Err gf_evg_stencil_set_texture_parametric(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, gf_evg_texture_callback callback, void *cbk_udta, Bool use_screen_coords);

/*! sets texture mapping options for a texture stencil
\param stencil the target stencil
\param map_mode texture mapping flags to set
\return error if any
*/
GF_Err gf_evg_stencil_set_mapping(GF_EVGStencil *stencil, GF_TextureMapFlags map_mode);

/*! sets padding color in RGBA
 If texture repeat is disabled and pad color is set, this color is used when outside of image, otherwise pixel is fetch from image border
\param stencil the target stencil
\param pad_color color to use , 0 disables color padding
\return error if any
*/
GF_Err gf_evg_stencil_set_pad_color(GF_EVGStencil * stencil, GF_Color pad_color);

/*! gets padding color in RGBA
\param stencil the target stencil
\return color  used , 0 if error
*/
u32 gf_evg_stencil_get_pad_color(GF_EVGStencil * stencil);

/*! sets filtering mode for a texture stencil
\param stencil the target stencil
\param filter_mode texture filtering mode to set
\return error if any
*/
GF_Err gf_evg_stencil_set_filter(GF_EVGStencil *stencil, GF_TextureFilter filter_mode);

/*! sets color matrix of a stencil
\param stencil the target stencil
\param cmat the color matrix to use. If NULL, resets current color matrix
\return error if any
 */
GF_Err gf_evg_stencil_set_color_matrix(GF_EVGStencil *stencil, GF_ColorMatrix *cmat);

/*! gets color matrix of a stencil
\param stencil the target stencil
\param cmat filled with current color matrix of stencil
\return error if any
 */
GF_Err gf_evg_stencil_get_color_matrix(GF_EVGStencil *stencil, GF_ColorMatrix *cmat);

/*! gets pixel at given position in ARGB format
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return pixel value
 */
u32 gf_evg_stencil_get_pixel(GF_EVGStencil *stencil, s32 x, s32 y);

/*! gets pixel at given position in AYUV format
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return pixel value
 */
u32 gf_evg_stencil_get_pixel_yuv(GF_EVGStencil *stencil, s32 x, s32 y);

/*! gets pixel at given position in ARGB format for more than 8 bits textures
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return pixel value
 */
u64 gf_evg_stencil_get_pixel_wide(GF_EVGStencil *stencil, s32 x, s32 y);

/*! gets pixel at given position in AYUV format for more than 8 bits textures
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return pixel value
 */
u64 gf_evg_stencil_get_pixel_yuv_wide(GF_EVGStencil *stencil, s32 x, s32 y);

/*! gets ARGB pixel at a given position
 The position is given as normalize coordinates (between 0.0 and 1.0), {0.0,0.0} is top-left, {1.0,1.0} is bottom-right
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return output r, r, g, a components
 */
GF_Vec4 gf_evg_stencil_get_pixel_f(GF_EVGStencil *stencil, Float x, Float y);

/*! gets AYUV pixel at a given position
  The position is given as normalize coordinates (between 0.0 and 1.0), {0.0,0.0} is top-left, {1.0,1.0} is bottom-right
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return output y, u, v, a components
 */
GF_Vec4 gf_evg_stencil_get_pixel_yuv_f(GF_EVGStencil *stencil, Float x, Float y);

/*! creates a canvas surface object
\param center_coords if GF_TRUE, indicates mathematical-like coord system (0,0) at the center of the canvas, otherwise indicates computer-like coord system (0,0) top-left corner
\return a new surface*/
GF_EVGSurface *gf_evg_surface_new(Bool center_coords);

/*! deletes a surface object
\param surf the surface object
*/
void gf_evg_surface_delete(GF_EVGSurface *surf);

/*! enables threading on a canvas surface object (can only be called once per surface)
\param surf the target surface
\param nb_threads number of additional threads to use for rendering. -1 use all availables core
\return error if any, GF_BAD_PARAM  if threading was already allocated, GF_IO_ERR if only one core available and nb_threads not assigned to a positive value*/
GF_Err gf_evg_enable_threading(GF_EVGSurface *surf, s32 nb_threads);

/*! attaches a surface object to a texture stencil object
\param surf the surface object
\param sten the stencil object (shll be a texture stencil)
\return error if any
*/
GF_Err gf_evg_surface_attach_to_texture(GF_EVGSurface *surf, GF_EVGStencil *sten);

/*! checks if pixel format is supported for a surface
\param pixelFormat the target pixel format
\return GF_TRUE if supported, GF_FALSE otherwise
*/
Bool gf_evg_surface_format_ok(GF_PixelFormat pixelFormat);

/*! attaches a surface object to a memory buffer
\param surf the surface object
\param pixels texture data
\param width texture width in pixels
\param height texture height in pixels
\param pitch_x texture horizontal pitch (bytes to skip to get to next pixel). O means linear frame buffer (eg pitch_x==bytes per pixel)
\param pitch_y texture vertical pitch (bytes to skip to get to next line)
\param pixelFormat texture pixel format
\return error if any
*/
GF_Err gf_evg_surface_attach_to_buffer(GF_EVGSurface *surf, u8 *pixels, u32 width, u32 height, s32 pitch_x, s32 pitch_y, GF_PixelFormat pixelFormat);

/*! sets rasterizer precision
\param surf the surface object
\param level the raster quality level
\return error if any
*/
GF_Err gf_evg_surface_set_raster_level(GF_EVGSurface *surf, GF_RasterQuality level);

/*! gets rasterizer precision
\param surf the surface object
\return the raster quality level
*/
GF_RasterQuality gf_evg_surface_get_raster_level(GF_EVGSurface *surf);

/*! Force next path fill to use antialias, reset at each  \ref gf_evg_surface_set_path
\param surf the surface object
\return error if any
*/
GF_Err gf_evg_surface_force_aa(GF_EVGSurface *surf);

/*! sets the given matrix as the current transformations for all drawn paths
\note this is only used for 2D rasterizer, and ignored in 3D mode
\param surf the surface object
\param mat the matrix to set; if NULL, resets the current transformation
\return error if any
*/
GF_Err gf_evg_surface_set_matrix(GF_EVGSurface *surf, GF_Matrix2D *mat);

/*! sets the given matrix as the current transformations for all drawn paths. The matrix shall be a projection matrix (ortho or perspective)
with normalized coordinates in [-1,1]. It may also contain a modelview part.

Perspective correct mapping is supported for textures and gradients:
- the bottom-left corner of the path bounds is texture coordinate 0,0
- the top-right corner of the path bounds is texture coordinate 1,1

\warning When 3D matrices are used, filling a path shall be done with a stencil in auto matrix mode.

\note this is only used for 2D rasterizer, and ignored in 3D mode
\param surf the surface object
\param mat the matrix to set; if NULL, resets the current transformation
\return error if any
*/
GF_Err gf_evg_surface_set_matrix_3d(GF_EVGSurface *surf, GF_Matrix *mat);

/*! sets the given rectangle as a clipper
When a clipper is enabled, nothing is drawn outside of the clipper. The clipper is not affected by the surface matrix
\param surf the surface object
\param rc the clipper to set, in pixel coordinates of the surface; if NULL, disables clipper
\return error if any
*/
GF_Err gf_evg_surface_set_clipper(GF_EVGSurface *surf, GF_IRect *rc);

/*! checks if clipper is active
\param surf the surface object
\return GF_TRUE if clipper is active, GF_FALSE otherwise
*/
Bool gf_evg_surface_use_clipper(GF_EVGSurface *surf);

/*! sets the given path as the current one for drawing
\warning This will internally copy the path after transformation with the current transfom. Changing the surface transform will have no effect unless calling this function again.
The clipper and stencil mode may be changed at will

\note this is only used for 2D rasterizer, and ignored in 3D mode
\param surf the surface object
\param path the target path to rasterize
\return error if any
*/
GF_Err gf_evg_surface_set_path(GF_EVGSurface *surf, GF_Path *path);

/*! draw (filling) the current path on a surface using the given stencil and current clipper if any.
If the stencil is a solid brush and its alpha value is 0, the surface is cleared if it has an alpha component, otherwise the call is ignored.
\note this can be called several times with the same current path
\note this is only used for 2D rasterizer, and ignored in 3D mode
\param surf the surface object
\param stencil the stencil to use to fill a path, if NULL uses 2D shader if setup
\return error if any
*/
GF_Err gf_evg_surface_fill(GF_EVGSurface *surf, GF_EVGStencil *stencil);


/*! Operands for multitexture operations*/
typedef enum {
	/*! no multitexture*/
	GF_EVG_OPERAND_NONE = 0,
	/*! mix texture 1 and texure 2 using coefficient in first param, returning tx1*coef + tx2*(1-coef) but forcing alpha to full opacity*/
	GF_EVG_OPERAND_MIX,
	/*! mix texture 1 and texure 2 using coefficient in first param, returning tx1*coef + tx2*(1-coef), including alpha channel */
	GF_EVG_OPERAND_MIX_ALPHA,
	/*! replace alpha of texture 1 with alpha of texture 2
		if first param is 0 or not set, use alpha channel from texture 2
		if first param is 1, use red channel from texture 2
		if first param is 2, use green/Cb/U channel from texture 2
		if first param is 3, use blue/Cr/V channel from texture 2
	*/
	GF_EVG_OPERAND_REPLACE_ALPHA,
	/*! replace alpha of texture 1 with (1-alpha) of texture 2
		if first param is 0 or not set, use alpha channel from texture 2
		if first param is 1, use red/Y channel from texture 2
		if first param is 2, use green/Cb/U channel from texture 2
		if first param is 3, use blue/Cr/V channel from texture 2
	*/
	GF_EVG_OPERAND_REPLACE_ONE_MINUS_ALPHA,
	/*! mix texture 1 and texure 2 using alpha coef of texture 3 but forcing alpha to full opacity
		if first param is 0 or not set, use alpha channel from texture 3
		if first param is 1, use red channel from texture 3
		if first param is 2, use green/Cb/U channel from texture 3
		if first param is 3, use blue/Cr/V channel from texture 3
	*/
	GF_EVG_OPERAND_MIX_DYN,
	/*! mix texture 1 and texure 2 using alpha coef of texture 3, including alpha channel.
		if first param is 0 or not set, use alpha channel from texture 3
		if first param is 1, use red channel from texture 3
		if first param is 2, use green/Cb/U channel from texture 3
		if first param is 3, use blue/Cr/V channel from texture 3
	*/
	GF_EVG_OPERAND_MIX_DYN_ALPHA,
	/*! use texture 1 for odd fill and texture 2 for even fill*/
	GF_EVG_OPERAND_ODD_FILL,
} GF_EVGMultiTextureMode;

/*! draw (filling) the current path on a surface using a compinaison of  stencils and current clipper if any.
\param surf the surface object
\param operand stencil combine effect
\param sten1 the first stencil to use, if NULL assumes shader and ignores operand
\param sten2 the second stencil to use, may be NULL depending on operand
\param sten3 the third stencil to use, may be NULL depending on operand
\param params the parameters to control the operand
\return error if any
*/
GF_Err gf_evg_surface_multi_fill(GF_EVGSurface *surf, GF_EVGMultiTextureMode operand, GF_EVGStencil *sten1, GF_EVGStencil *sten2, GF_EVGStencil *sten3, Float params[4]);

/*! clears given pixel rectangle on a surface with the given color
\warning this ignores any clipper set on the surface
\param surf the surface object
\param rc the rectangle in pixel coordinates to clear. This may lay outside the surface, and shall not be NULL
\param col the color used to clear the canvas. The alpha component is discarded if the surface does not have alpha, otherwise it is used
\return error if any
*/
GF_Err gf_evg_surface_clear(GF_EVGSurface *surf, GF_IRect *rc, GF_Color col);

/*! sets center coord mode of a surface
\note this is only used for 2D rasterizer, and ignored in 3D mode
\param surf the surface object
\param center_coords if GF_TRUE, indicates mathematical-like coord system (0,0) at the center of the canvas, otherwise indicates computer-like coord system (0,0) top-left corner
*/
void gf_evg_surface_set_center_coords(GF_EVGSurface *surf, Bool center_coords);

/*! Composition mode used for ARGB surfaces - cf Canvas2D modes*/
typedef enum
{
	/*! source over*/
	GF_EVG_SRC_OVER = 0,
	/*! source atop*/
	GF_EVG_SRC_ATOP,
	/*! source in*/
	GF_EVG_SRC_IN,
	/*! source out*/
	GF_EVG_SRC_OUT,
	/*! destination atop*/
	GF_EVG_DST_ATOP,
	/*! destination in*/
	GF_EVG_DST_IN,
	/*! destination out*/
	GF_EVG_DST_OUT,
	/*! destination over*/
	GF_EVG_DST_OVER,
	/*! destination * source*/
	GF_EVG_LIGHTER,
	/*! source copy*/
	GF_EVG_COPY,
	/*! source XOR destination*/
	GF_EVG_XOR,
} GF_EVGCompositeMode;

/*! sets surface composite mode, as defined in Canvas2D - only used for ARGB surfaces
 \warning this is still experimental
\param surf the surface object
\param comp_mode the composition mode to use
*/
void gf_evg_surface_set_composite_mode(GF_EVGSurface *surf, GF_EVGCompositeMode comp_mode);

/*! callback type for alpha override
\param udta opaque data passed back to caller
\param src_alpha alpha value about to be blended
\param x horizontal coordinate of pixel to be drawn, {0,0} being top-left, positive Y go down
\param y vertical coordinate of pixel to be drawn, {0,0} being top-left, positive Y go down
\return final alpha value to use*/
typedef u8 (*gf_evg_get_alpha)(void *udta, u8 src_alpha, s32 x, s32 y);

/*! sets alpha callback function. This allows finer control over the blending, but does not allow for pixel color modification
\param surf the surface object
\param get_alpha the callback function to alter the alpha value
\param cbk opaque data for the callback function
*/
void gf_evg_surface_set_alpha_callback(GF_EVGSurface *surf, gf_evg_get_alpha get_alpha, void *cbk);


/*! Primitive types for 3D software rasterize - see OpenGL terminology*/
typedef enum
{
	//do NOT modify order

	/*! points, 1 vertex index per primitive */
	GF_EVG_POINTS=1,
	/*! polygon, all vertex indices in array are used for a single face*/
	GF_EVG_POLYGON,
	/*! lines, 2 vertex indices per primitive*/
	GF_EVG_LINES,
	/*! triangles, 3 vertex indices per primitive*/
	GF_EVG_TRIANGLES,
	/*! quads, 4 vertex indices per primitive*/
	GF_EVG_QUADS,

	/*! line strip, 2 vertex indices for first primitive, then one for subsequent ones*/
	GF_EVG_LINE_STRIP,
	/*! triangle strip, 3 vertex indices for first primitive, then one for subsequent ones*/
	GF_EVG_TRIANGLE_STRIP,
	/*! triangle fan, 3 vertex indices for first primitive, then one for subsequent ones*/
	GF_EVG_TRIANGLE_FAN,
	/*! quad strip, 4 vertex indices for first primitive, then one for subsequent ones*/
	GF_EVG_QUAD_STRIP,
} GF_EVGPrimitiveType;

/*! enables  3D software rasterizer for surface
 \param surf the target 3D surface
 \return error if any*/
GF_Err gf_evg_surface_enable_3d(GF_EVGSurface *surf);

/*! sets projection matrix
 \note this is only used for 3D rasterizer, and fails 2D mode
 \param surf the target 3D surface
 \param mx the target projection matrix
 \return error if any
 */
GF_Err gf_evg_surface_set_projection(GF_EVGSurface *surf, GF_Matrix *mx);
/*! sets modelview matrix
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param mx the target modelview matrix
\return error if any
*/
GF_Err gf_evg_surface_set_modelview(GF_EVGSurface *surf, GF_Matrix *mx);
/*! draws a set of primitive using vertices and an index buffer modelview matrix
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param indices the array of indices to use in the vertex buffer
\param nb_indices the number of indices
\param vertices the array of vertices to use
\param nb_vertices the number of vertices
\param nb_comp the number of component per vertices (eg, 2, 3)
\param prim_type the primitive type to use
\return error if any
*/
GF_Err gf_evg_surface_draw_array(GF_EVGSurface *surf, u32 *indices, u32 nb_indices, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type);

/*! clears the depth buffer
 \note the depth buffer is set by the caller. If no depth buffer is assigned, this function returns GF_OK
 \note the color buffer is cleared using \ref gf_evg_surface_clear
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param depth the depth value to use
\return error if any
*/
GF_Err gf_evg_surface_clear_depth(GF_EVGSurface *surf, Float depth);
/*! sets the surface viewport to convert from projected coordiantes to screen coordinates
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param x the top-left coordinate of the viewport in pixels
\param y the top-left coordinate of the viewport in pixels
\param w the width  of the viewport in pixels
\param h the height of the viewport in pixels
\return error if any
*/
GF_Err gf_evg_surface_viewport(GF_EVGSurface *surf, u32 x, u32 y, u32 w, u32 h);
/*! draws path on a 3D surface with the given z.

The vertex shader is ignored (surface matrix must be set), and the fragment shader is called using perspective interpolation weights derived from the path bounds,
using path bounds (top-left, top-right, bottom-right) vertices.
 \note this is only used for 3D rasterizer, and fails 2D mode

\param surf the target 3D surface
\param path the path to draw
\param z the z value to assign to the path in local coordinate system
\return error if any
*/
GF_Err gf_evg_surface_draw_path(GF_EVGSurface *surf, GF_Path *path, Float z);

/*! Depth test modes*/
typedef enum
{
	/*! depth test is disabled*/
	GF_EVGDEPTH_DISABLE,
	/*! depth test always fails*/
	GF_EVGDEPTH_NEVER,
	/*! depth test always succeeds*/
	GF_EVGDEPTH_ALWAYS,
	/*! depth test succeeds if fragment depth is == than depth buffer value*/
	GF_EVGDEPTH_EQUAL,
	/*! depth test succeeds if fragment depth is != than depth buffer value*/
	GF_EVGDEPTH_NEQUAL,
	/*! depth test succeeds if fragment depth is < than depth buffer value*/
	GF_EVGDEPTH_LESS,
	/*! depth test succeeds if fragment depth is <= than depth buffer value*/
	GF_EVGDEPTH_LESS_EQUAL,
	/*! depth test succeeds if fragment depth is > than depth buffer value*/
	GF_EVGDEPTH_GREATER,
	/*! depth test succeeds if fragment depth is >= than depth buffer value*/
	GF_EVGDEPTH_GREATER_EQUAL
} GF_EVGDepthTest;

/*! sets depth buffer test
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param mode the desired depth test mode
\return error if any
*/
GF_Err gf_evg_set_depth_test(GF_EVGSurface *surf, GF_EVGDepthTest mode);

/*! fragment color type */
typedef enum
{
	/*! fragment is invalid (discarded or error) */
	GF_EVG_FRAG_INVALID = 0,
	/*! fragment is RGB float*/
	GF_EVG_FRAG_RGB,
	GF_EVG_FRAG_RGB_PACK,
	/*! fragment is YUV */
	GF_EVG_FRAG_YUV,
	GF_EVG_FRAG_YUV_PACK,
} GF_EVGFragmentType;

/*! Parameters for the fragment callback */
typedef struct
{
	/*! screen x in pixels - input param*/
	Float screen_x;
	/*! screen y in pixels - input param*/
	Float screen_y;
	/*! screen z in NDC - input param*/
	Float screen_z;
	/*! depth - input and output param - 3D shaders only*/
	Float depth;
	/*! primitive index in the current \ref gf_evg_surface_draw_array call, 0 being the first primitive  - input param - 3D shaders only*/
	u32 prim_index;
	/*! index of first vertex in the current primitive  - input param - 3D shaders only*/
	u32 idx1;
	/*! index of second vertex in the current primitive (for lines or triangles/quads)  - input param - 3D shaders only*/
	u32 idx2;
	/*! index of third vertex in the current primitive (for triangles/quads)  - input param - 3D shaders only*/
	u32 idx3;

	/*! primitive type  - input param - 3D shaders only*/
	GF_EVGPrimitiveType ptype;

	/*! fragment color, must be written if fragment is not discarded - output value*/
	GF_Vec4 color;
	/*! fragment color in pack 32 bit ARGB/AYUV*/
	u32 color_pack;
	/*! fragment color in pack 64 bit ARGB/AYUV*/
	u64 color_pack_wide;
	/*! fragment valid state - output value*/
	GF_EVGFragmentType frag_valid;

	/*vars for lerp*/
	/*perspective correct interpolation is done according to OpenGL eq 14.9
		f = (a*fa/wa + b*fb/wb + c*fc/wc) / (a/w_a + b/w_b + c/w_c)
	*/
	/*! perspective corrected barycentric, eg bc1/q1, bc2/q2, bc3/q3  - 3D shaders only*/
	Float pbc1, pbc2, pbc3;
	/*! perspective divider - 3D shaders only
	\note this is also 1/W of the fragment, eg opengl gl_fragCoord.w
	*/
	Float persp_denum;
	/* private for shader, valid between \ref gf_evg_fragment_shader_init (init, cleanup) calls */
	void *shader_udta;

	/*! horizontal texture coordinate, 0 is left of image -   2D shaders only */
	u32 tx_x;
	/*! vertical texture coordinate, 0 is top of image -   2D shaders only */
	u32 tx_y;
	/*! texture width -  2D shaders only */
	u32 tx_width;
	/*! texture height -  2D shaders only */
	u32 tx_height;

	/*! coverage value between 0 and 0xFF -  2D shaders only */
	u8 coverage;
	/*! odd / even flag for path drawn with zero/non-zero fill rule -  2D shaders only */
	u8 odd_flag;

} GF_EVGFragmentParam;


/*! Parameters for the vertex callback */
typedef struct
{
	/*! input vertex - input param*/
	GF_Vec4 in_vertex;
	/*! primitive index in the current \ref gf_evg_surface_draw_array call, 0 being the first primitive  - input param*/
	u32 prim_index;
	/*! index of the vertex  - input param*/
	u32 vertex_idx;
	/*! index of the vertex in the current primitive  - input param*/
	u32 vertex_idx_in_prim;
	/*! primitive type  - input param*/
	u32 ptype;

	/*! transformed vertex to use, must be written  - output values*/
	GF_Vec4 out_vertex;
} GF_EVGVertexParam;

/*! callback type for fragment shader
 \param udta opaque data passed back to caller
 \param fragp fragment paramters
 \return GF_TRUE if success; GF_FALSE if error*/
typedef Bool (*gf_evg_fragment_shader)(void *udta, GF_EVGFragmentParam *fragp);

/*! callback type for fragment shader init, called once before each primitive - may be NULL
 \param udta opaque data passed back to caller
 \param fragp fragment paramters
 \param th_id index of thread context
 \param is_cleanup if TRUE indicates and of primitive rendering, otherwise begin
 \return GF_TRUE if success; GF_FALSE if error*/
typedef Bool (*gf_evg_fragment_shader_init)(void *udta, GF_EVGFragmentParam *fragp, u32 th_id, Bool is_cleanup);


/*! assigns fragment shader to the rasterizer.
\warning If multithread is enabled, the shader must be thread-safe
This can be used for 3D and 2D modes. In 2D mode, texture coordinates are derived from path bounds
\param surf the target 3D surface
\param shader the fragment shader callback to use
\param shader_init the fragment shader init callback to use
\param shader_udta opaque data to pass to the callback function
\return error if any
 */
GF_Err gf_evg_surface_set_fragment_shader(GF_EVGSurface *surf, gf_evg_fragment_shader shader, gf_evg_fragment_shader_init shader_init, void *shader_udta);

/*! callback type for vertex shader
\param udta opaque data passed back to caller
\param vertp vertex paramters
\return GF_TRUE if success; GF_FALSE if error*/
typedef Bool (*gf_evg_vertex_shader)(void *udta, GF_EVGVertexParam *vertp);
/*! assigns vertex shader to the rasterizer
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param shader the vertex shader callback to use
\param shader_udta opaque data to pass to the callback function
\return error if any
 */
GF_Err gf_evg_surface_set_vertex_shader(GF_EVGSurface *surf, gf_evg_vertex_shader shader, void *shader_udta);

/*! sets face orientation for the primitives
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param is_ccw if GF_TRUE, face are given in counter-clockwise order, otherwise in clockwise order
\return error if any
 */
GF_Err gf_evg_surface_set_ccw(GF_EVGSurface *surf, Bool is_ccw);
/*! enables/disables backface culling
 \warning not enabling backface culling when anti-aliasing is on may result in visual artefacts
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param backcull if GF_TRUE, faces with normals poiting away from camera will not be drawn
\return error if any
 */
GF_Err gf_evg_surface_set_backcull(GF_EVGSurface *surf, Bool backcull);
/*! enables/disables anti-aliased rendering
 \warning not enabling backface culling when anti-aliasing is on may result in visual artefacts
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param antialias if GF_TRUE, edges will be anti-aliased
\return error if any
 */
GF_Err gf_evg_surface_set_antialias(GF_EVGSurface *surf, Bool antialias);
/*! sets minimum depth value for coordinates normalization - see OpenGL glDepthRange
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param min_depth the minimum depth value
\return error if any
 */
GF_Err gf_evg_surface_set_min_depth(GF_EVGSurface *surf, Float min_depth);
/*! sets maximum depth value for coordinates normalization - see OpenGL glDepthRange
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param max_depth the maximum depth value
\return error if any
 */
GF_Err gf_evg_surface_set_max_depth(GF_EVGSurface *surf, Float max_depth);
/*! indicates that the clip space after projection is [0,1] and not [-1, 1]
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param clip_zero if GF_TRUE, the projected vertices depth are in [0, 1], otherwise in [-1, 1]
\return error if any
 */
GF_Err gf_evg_surface_set_clip_zero(GF_EVGSurface *surf, Bool clip_zero);
/*! sets point size value for points primitives
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param size  the desired point size in pixels
\return error if any
 */
GF_Err gf_evg_surface_set_point_size(GF_EVGSurface *surf, Float size);
/*! sets line size (width) value for points primitives
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param size  the desired line size in pixels
\return error if any
 */
GF_Err gf_evg_surface_set_line_size(GF_EVGSurface *surf, Float size);
/*! enables point smoothing.
 When point smoothing is enabled, the geometry drawn is a circle, otherwise a square
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param smooth if GF_TRUE, point smoothing is enabled
\return error if any
 */
GF_Err gf_evg_surface_set_point_smooth(GF_EVGSurface *surf, Bool smooth);
/*! disables early depth test
If your fragment shader modifies the depth of the fragment, you must call this function to avoid early depth test (fragment discarded before calling the shader)
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param disable if GF_TRUE, early depth test is disabled
\return error if any
 */
GF_Err gf_evg_surface_disable_early_depth(GF_EVGSurface *surf, Bool disable);
/*! disables depth buffer write
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param do_write if GF_TRUE,  depth values are written to the depth buffer (default when creating the rasterizer)
\return error if any
 */
GF_Err gf_evg_surface_write_depth(GF_EVGSurface *surf, Bool do_write);
/*! sets  depth buffer
 The 3D rasterizer does not allocate any surface (depth or color buffers), it is the user responsibility to set these buffers
 \warning do NOT forget to resize your depth buffer when resizing the canvas !
\note this is only used for 3D rasterizer, and fails 2D mode
\param surf the target 3D surface
\param depth the depth buffer to use. Its size shall be Float*width*height
\return error if any
 */
GF_Err gf_evg_surface_set_depth_buffer(GF_EVGSurface *surf, Float *depth);


/*! performs RGB to YUV conversion
\param surf the target  surface
\param r the source red component
\param g the source green component
\param b the source blue component
\param y the output Y component
\param cb the output U/Cb component
\param cr the output V/Cr component
\return error if any
 */
GF_Err gf_gf_evg_rgb_to_yuv_f(GF_EVGSurface *surf, Float r, Float g, Float b, Float *y, Float *cb, Float *cr);

/*! performs YUV to RGB conversion
\param surf the target  surface
\param y the source Y component
\param cb the source U/Cb component
\param cr the source V/Cr component
\param r the output red component
\param g the output green component
\param b the output blue component
\return error if any
 */
GF_Err gf_evg_yuv_to_rgb_f(GF_EVGSurface *surf, Float y, Float cb, Float cr, Float *r, Float *g, Float *b);

/*! converts RGB to YUV
 \param surf the target surface
 \param col the source RGBA color
 \param y the output Y value
 \param cb the output Cb/U value
 \param cr the output Cr/V value
 */
void gf_evg_rgb_to_yuv(GF_EVGSurface *surf, GF_Color col, u8*y, u8*cb, u8*cr);

/*! converts ARGB to AYUV
\param surf the target surface
\param col the source ARGB color
\return the result AYUV color
*/
GF_Color gf_evg_argb_to_ayuv(GF_EVGSurface *surf, GF_Color col);

/*! converts AYUV to ARGB
\param surf the target surface
\param col the source AYUV  color
\return the result ARGB color
*/
GF_Color gf_evg_ayuv_to_argb(GF_EVGSurface *surf, GF_Color col);

/*! converts ARGB to AYUV for wide colors
\param surf the target surface
\param col the source ARGB color
\return the result AYUV color
*/
u64 gf_evg_argb_to_ayuv_wide(GF_EVGSurface *surf, u64 col);

/*! converts AYUV to ARGB for wide color
\param surf the target surface
\param col the source AYUV  color
\return the result ARGB color
*/
u64 gf_evg_ayuv_to_argb_wide(GF_EVGSurface *surf, u64 col);

/*! global alpha mask values */
typedef enum
{
	/*! global alpha mask not used */
	GF_EVGMASK_NONE = 0,
	/*! subsequent draw operations will target the global alpha mask */
	GF_EVGMASK_DRAW,
	/*! subsequent draw operations will target the global alpha mask, but alpha mask is not cleared */
	GF_EVGMASK_DRAW_NO_CLEAR,
	/*! subsequent draw operations will be filtered with the global alpha mask */
	GF_EVGMASK_USE,
	/*! subsequent draw operations will be filtered with 1 minus the global alpha mask */
	GF_EVGMASK_USE_INV,
	/*! combine draw and use: the mask is set to 0xFF, each pixel drawn turns the mask value to 0 */
	GF_EVGMASK_RECORD,
} GF_EVGMaskMode;

/*! sets global alpha mask mode

The global alpha mask is an 8-bit alpha channel the size of the surface. Any resize operation on the surface will reset the alpha mask
Typcial usage for the alpha mask is to setup the surface, use GF_EVGMASK_DRAW to draw your mask then GF_EVGMASK_USE to apply the mask on your draw calls

Whenever the mask mode is changed to GF_EVGMASK_DRAW, the alpha masked is cleared
\param surf the target surface
\param mask_mode the current mask mode
\return error if any
*/
GF_Err gf_evg_surface_set_mask_mode(GF_EVGSurface *surf, GF_EVGMaskMode mask_mode);

/*! gets global alpha mask mode
\param surf the target surface
\return the current mask mode
*/
GF_EVGMaskMode gf_evg_surface_get_mask_mode(GF_EVGSurface *surf);

/*! @} */

#endif //GPAC_DISABLE_EVG

#ifdef __cplusplus
}
#endif

#endif	/*_GF_EVG_H_*/

