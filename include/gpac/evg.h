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

/*!
\file "gpac/evg.h"
\brief 2D vector graphics rasterizer

This file contains all defined functions for 2D vector graphics of the GPAC framework.
*/

/*!
\addtogroup evg_grp
\brief 2D Vector Graphics rendering of GPAC.

GPAC uses a software rasterizer for 2D graphics based on FreeType's "ftgray" anti-aliased renderer.

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
	/*! raster should use full antialiasing*/
	GF_RASTER_HIGH_QUALITY
} GF_RasterQuality;


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

/*! sets global alpha blending level for a texture or gradient stencil
The alpha channel will be combined with the color matrix if any
\warning do not use with solid brush stencil
\param stencil the target stencil
\param alpha the alpha value between 0 (full transparency) and 255 (full opacityy)
\return error if any
*/
GF_Err gf_evg_stencil_set_alpha(GF_EVGStencil *stencil, u8 alpha);

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

/*! sets parametric callback a texture stencil. A parametric texture gets its pixel values from a callback function, the resulting value being blended according to the antialiasing level of the pixel. This allows creating rather complex custom textures, in a fashion similar to fragment shaders.
\param stencil the target stencil
\param width texture width in pixels
\param height texture height in pixels
\param pixelFormat texture pixel format, indicates if the returned values are in RGB or YUV
\param callback the callback function to use
\param cbk_udta user data to pass to the callback function
\param use_screen_coords if set, the coordinates passed to the callback function are screen coordinates rather than texture coordinates
\return error if any
*/
GF_Err gf_evg_stencil_set_texture_parametric(GF_EVGStencil *stencil, u32 width, u32 height, GF_PixelFormat pixelFormat, gf_evg_texture_callback callback, void *cbk_data, Bool use_screen_coords);

/*! sets texture mapping options for a texture stencil
\param stencil the target stencil
\param map_mode texture mapping flags to set
\return error if any
*/
GF_Err gf_evg_stencil_set_mapping(GF_EVGStencil *stencil, GF_TextureMapFlags map_mode);

/*! sets filtering mode for a texture stencil
\param stencil the target stencil
\param filter_mode texture filtering mode to set
\return error if any
*/
GF_Err gf_evg_stencil_set_filter(GF_EVGStencil *stencil, GF_TextureFilter filter_mode);

/*! sets color matrix of a stencil
\warning ignored for solid brush stencil
\param stencil the target stencil
\param cmat the color matrix to use. If NULL, resets current color matrix
\return error if any
 */
GF_Err gf_evg_stencil_set_color_matrix(GF_EVGStencil *stencil, GF_ColorMatrix *cmat);

/*! gets pixel at given position (still experimental)
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\return pixel value
 */
u32 gf_evg_stencil_get_pixel(GF_EVGStencil *stencil, s32 x, s32 y);

/*! gets ARGB pixel at given position as normalize coordinates (between 0 and 1), {0,0} is top-left, {1,1} is bottom-right (still experimental)
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\param r output r component
\param g output g component
\param b output b component
\param a output a component
\return error if any
 */
GF_Err gf_evg_stencil_get_pixel_f(GF_EVGStencil *st, Float x, Float y, Float *r, Float *g, Float *b, Float *a);

/*! gets AYUV pixel at given position as normalize coordinates (between 0 and 1), {0,0} is top-left, {1,1} is bottom-right (still experimental)
\param stencil the target stencil
\param x horizontal coord
\param y vertical coord
\param Y output y component
\param U output u component
\param V output v component
\param A output a component
\return error if any
 */
GF_Err gf_evg_stencil_get_pixel_yuv_f(GF_EVGStencil *st, Float x, Float y, Float *Y, Float *U, Float *V, Float *A);

/*! creates a canvas surface object
\param center_coords if GF_TRUE, indicates mathematical-like coord system (0,0) at the center of the canvas, otherwise indicates computer-like coord system (0,0) top-left corner
\return a new surface*/
GF_EVGSurface *gf_evg_surface_new(Bool center_coords);

/*! deletes a surface object
\param surf the surface object
*/
void gf_evg_surface_delete(GF_EVGSurface *surf);

/*! attaches a surface object to a texture stencil object
\param surf the surface object
\param sten the stencil object (shll be a texture stencil)
\return error if any
*/
GF_Err gf_evg_surface_attach_to_texture(GF_EVGSurface *surf, GF_EVGStencil *sten);

/*! attaches a surface object to a memory buffer
\param surf the surface object
\param pixels: texture data
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

/*! sets the given matrix as the current transformations for all drawn paths
\param surf the surface object
\param mat the matrix to set; if NULL, resets the current transformation
\return error if any
*/
GF_Err gf_evg_surface_set_matrix(GF_EVGSurface *surf, GF_Matrix2D *mat);

/*! sets the given matrix as the current transformations for all drawn paths. The matrix shall be a projection matrix (ortho or perspective)
wuth normalized coordinates in [-1,1]. It may also contain a modelview part
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

/*! sets the given path as the current one for drawing
\warning This will internally copy the path after transformation with the current transfom. Changing the surface transform will have no effect unless calling this function again.
The clipper and stencil mode may be changed at will

\param surf the surface object
\param path the target path to rasterize
\return error if any
*/
GF_Err gf_evg_surface_set_path(GF_EVGSurface *surf, GF_Path *path);

/*! draw (filling) the current path on a surface using the given stencil and current clipper if any
\note this can be called several times with the same current path
\param surf the surface object
\param stencil the stencil to use to fill a path
\return error if any
*/
GF_Err gf_evg_surface_fill(GF_EVGSurface *surf, GF_EVGStencil *stencil);

/*! clears given pixel rectangle on a surface with the given color
\warning this ignores any clipper set on the surface
\param surf the surface object
\param rc the rectangle in pixel coordinates to clear. This may lay outside the surface, and shall not be NULL
\param col the color used to clear the canvas. The alpha component is discarded if the surface does not have alpha, otherwise it is used
\return error if any
*/
GF_Err gf_evg_surface_clear(GF_EVGSurface *surf, GF_IRect *rc, GF_Color col);

/*! sets center coord mode of a surface
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
\param surf the surface object
\param comp_mode the composition mode to use
*/
void gf_evg_surface_set_composite_mode(GF_EVGSurface *surf, GF_EVGCompositeMode comp_mode);

/*! sets alpha callback function
\param surf the surface object
\param get_alpha the callback function to alter the alpha value. x and y are in pixel coordinates, non-centered mode (0,0) is topleft, positive Y go down
\param cbk opaque data for the callback function
*/
void gf_evg_surface_set_alpha_callback(GF_EVGSurface *surf, u8 (*get_alpha)(void *udta, u8 src_alpha, s32 x, s32 y), void *cbk);



typedef enum
{
	//do NOT modify order
	GF_EVG_POINTS=1,
	GF_EVG_POLYGON,
	GF_EVG_LINES,
	GF_EVG_TRIANGLES,
	GF_EVG_QUADS,

	GF_EVG_LINE_STRIP,
	GF_EVG_TRIANGLE_STRIP,
	GF_EVG_TRIANGLE_FAN,
	GF_EVG_QUAD_STRIP,
} GF_EVGPrimitiveType;

GF_EVGSurface *gf_evg_surface3d_new();
GF_Err gf_evg_surface_set_projection(GF_EVGSurface *surf, GF_Matrix *mx);
GF_Err gf_evg_surface_set_modelview(GF_EVGSurface *surf, GF_Matrix *mx);
GF_Err gf_evg_surface_draw_array(GF_EVGSurface *surf, u32 *indices, u32 nb_idx, Float *vertices, u32 nb_vertices, u32 nb_comp, GF_EVGPrimitiveType prim_type);
GF_Err gf_evg_surface_clear_depth(GF_EVGSurface *surf, Float depth);
GF_Err gf_evg_surface_viewport(GF_EVGSurface *surf, u32 x, u32 y, u32 w, u32 h);
/*draws path on a 3D surface with the given z. The vertex shader is ignored (surface matrix must be set),
and the fragment shader is called using perspective interpolation weights derived from the path bounds,
using path bounds (top-left, top-right, bottom-right) vertices*/
GF_Err gf_evg_surface_draw_path(GF_EVGSurface *surf, GF_Path *path, Float z);

typedef enum
{
	GF_EVGDEPTH_DISABLE,
	GF_EVGDEPTH_NEVER,
	GF_EVGDEPTH_ALWAYS,
	GF_EVGDEPTH_EQUAL,
	GF_EVGDEPTH_NEQUAL,
	GF_EVGDEPTH_LESS,
	GF_EVGDEPTH_LESS_EQUAL,
	GF_EVGDEPTH_GREATER,
	GF_EVGDEPTH_GREATER_EQUAL
} GF_EVGDepthTest;

GF_Err gf_evg_set_depth_test(GF_EVGSurface *surf, GF_EVGDepthTest mode);

typedef enum
{
	GF_EVG_FRAG_INVALID = 0,
	GF_EVG_FRAG_RGB,
	GF_EVG_FRAG_YUV,
} GF_EVGFragmentType;

typedef struct
{
	/*input params*/
	Float screen_x;
	Float screen_y;
	Float screen_z;
	Float depth;

	u32 prim_index;
	/*index of vertices for the primitive in the vertex buffer*/
	u32 idx1, idx2, idx3;

	GF_EVGPrimitiveType ptype;

	/*output values*/
	GF_Vec4 color;
	GF_EVGFragmentType frag_valid;

	/*vars for lerp*/
	/*perspective correct interpolation is done according to openGL eq 14.9
		f = (a*fa/wa + b*fb/wb + c*fc/wc) / (a/w_a + b/w_b + c/w_c)
	*/
	/*perspective corrected barycentric, eg bc1/q1, bc2/q2, bc3/q3
		these are constant throughout the fragment*/
	Float pbc1, pbc2, pbc3;
	/* this is alsp 1/W of the fragment, eg opengl gl_fragCoord.w*/
	Float persp_denum;
} GF_EVGFragmentParam;


typedef struct
{
	/*input params*/
	GF_Vec4 in_vertex;
	u32 prim_index;
	u32 vertex_idx;
	u32 vertex_idx_in_prim;
	u32 ptype;

	/*output values*/
	GF_Vec4 out_vertex;
} GF_EVGVertexParam;

typedef Bool (*gf_evg_fragment_shader)(void *udta, GF_EVGFragmentParam *frag);
GF_Err gf_evg_surface_set_fragment_shader(GF_EVGSurface *surf, gf_evg_fragment_shader shader, void *shader_udta);

typedef Bool (*gf_evg_vertex_shader)(void *udta, GF_EVGVertexParam *frag);
GF_Err gf_evg_surface_set_vertex_shader(GF_EVGSurface *surf, gf_evg_vertex_shader shader, void *shader_udta);

GF_Err gf_evg_surface_set_ccw(GF_EVGSurface *surf, Bool is_ccw);
GF_Err gf_evg_surface_set_backcull(GF_EVGSurface *surf, Bool backcull);
GF_Err gf_evg_surface_set_antialias(GF_EVGSurface *surf, Bool antialias);
GF_Err gf_evg_surface_set_min_depth(GF_EVGSurface *surf, Float min_depth);
GF_Err gf_evg_surface_set_max_depth(GF_EVGSurface *surf, Float max_depth);
GF_Err gf_evg_surface_set_clip_zero(GF_EVGSurface *surf, Bool clip_zero);
GF_Err gf_evg_surface_set_point_size(GF_EVGSurface *surf, Float size);
GF_Err gf_evg_surface_set_line_size(GF_EVGSurface *surf, Float size);
GF_Err gf_evg_surface_set_point_smooth(GF_EVGSurface *surf, Bool smooth);
GF_Err gf_evg_surface_disable_early_depth(GF_EVGSurface *surf, Bool disable);
GF_Err gf_evg_surface_write_depth(GF_EVGSurface *surf, Bool do_write);
GF_Err gf_evg_surface_set_depth_buffer(GF_EVGSurface *surf, Float *depth);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_EVG_H_*/

