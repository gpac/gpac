/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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


#ifndef _GF_PATH2D_H_
#define _GF_PATH2D_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/math.h>
#include <gpac/constants.h>


typedef struct
{
	/*number of subpath*/
	u32 n_contours;
	/*number of points in path and alloc size*/
	u32 n_points, n_alloc_points;
	/*points */
	GF_Point2D *points;
	/*point tags (one per point)*/
	u8 *tags;
	/*subpath end points*/
	u32 *contours;
	/*path bbox - NEVER USE WITHOUT FIRST CALLING gf_path_get_bounds*/
	GF_Rect bbox;
	/* path flags*/
	s32 flags;
	/*fineness to use whenever flattening the path - default is FIX_ONE*/
	Fixed fineness;
} GF_Path;


GF_Path *gf_path_new();
void gf_path_del(GF_Path *gp);
void gf_path_reset(GF_Path *gp);
GF_Path *gf_path_clone(GF_Path *gp);
GF_Err gf_path_close(GF_Path *gp);
GF_Err gf_path_add_move_to(GF_Path *gp, Fixed x, Fixed y);
GF_Err gf_path_add_move_to_vec(GF_Path *gp, GF_Point2D *pt);
GF_Err gf_path_add_line_to(GF_Path *gp, Fixed x, Fixed y);
GF_Err gf_path_add_line_to_vec(GF_Path *gp, GF_Point2D *pt);
GF_Err gf_path_add_cubic_to(GF_Path *gp, Fixed c1_x, Fixed c1_y, Fixed c2_x, Fixed c2_y, Fixed x, Fixed y);
GF_Err gf_path_add_cubic_to_vec(GF_Path *gp, GF_Point2D *c1, GF_Point2D *c2, GF_Point2D *pt);
GF_Err gf_path_add_quadratic_to(GF_Path *gp, Fixed c_x, Fixed c_y, Fixed x, Fixed y);
GF_Err gf_path_add_quadratic_to_vec(GF_Path *gp, GF_Point2D *c, GF_Point2D *pt);
GF_Err gf_path_add_rect_center(GF_Path *gp, Fixed cx, Fixed cy, Fixed w, Fixed h);
GF_Err gf_path_add_rect(GF_Path *gp, Fixed ox, Fixed oy, Fixed w, Fixed h);
GF_Err gf_path_add_ellipse(GF_Path *gp, Fixed cx, Fixed cy, Fixed a_axis, Fixed b_axis);
/*add N-bezier to path - path.fineness must be set before*/
GF_Err gf_path_add_bezier(GF_Path *gp, GF_Point2D *pts, u32 nbPoints);
GF_Err gf_path_add_arc_to(GF_Path *gp, Fixed end_x, Fixed end_y, Fixed fa_x, Fixed fa_y, Fixed fb_x, Fixed fb_y, Bool cw);
GF_Err gf_path_add_arc(GF_Path *gp, Fixed radius, Fixed start_angle, Fixed end_angle, u32 close_type);
GF_Err gf_path_get_control_bounds(GF_Path *gp, GF_Rect *rc);
GF_Err gf_path_get_bounds(GF_Path *gp, GF_Rect *rc);
void gf_path_flatten(GF_Path *gp);
GF_Path *gf_path_get_flatten(GF_Path *gp);
Bool gf_path_point_over(GF_Path *gp, Fixed x, Fixed y);

typedef struct _path_iterator GF_PathIterator;

/*inits path iteration*/
GF_PathIterator *gf_path_iterator_new(GF_Path *gp);
void gf_path_iterator_del(GF_PathIterator *it);

Fixed gf_path_iterator_get_length(GF_PathIterator *it);
/*gets transformation matrix at given point (offset) 
the transform is so that a local system is translated to the given point, its x-axis tangent to the path and in 
the same direction (path direction is from first point to last point)
@offset: length on the path in local system unit
@follow_tangent: indicates if transformation shall be computed if @offset indicates a point outside 
the path (<0 or >path_length). In which case the path shall be virtually extended by the tangent at 
origin (@offset <0) or at end (@offset>path_length). Otherwise the transformation is not computed 
and 0 is returned
@mat: matrix to be transformed (transformation shall be appended) - the matrix shall not be initialized
@smooth_edges: indicates if discontinuities shall be smoothed. 
	If not set, the rotation angle THETA is
the slope (DX/DY) of the current segment found.
	if set, the amount of the object that lies on next segment shall be computed according to 
@length_after_point . let:
	len_last: length of current checked segment
	len1: length of all previous segments so that len1 + len_last >= offset
then if (offset + length_after_point > len1 + len_last) {
		ratio = (len1 + len_last - offset) / length_after_point;

	then THETA = ratio * slope(L1) + (1-ratio) * slope(L2)
(of course care must be taken for PI/2 angles & the like)

returns 1 if matrix has been updated, 0 otherwise (fail or point out of path without tangent extension)
*/
Bool gf_path_iterator_get_transform(GF_PathIterator *it, Fixed offset, Bool follow_tangent, GF_Matrix2D *mat, Bool smooth_edges, Fixed length_after_point);



/*get convexity info for this 2D polygon*/
u32 gf_polygone2d_get_convexity(GF_Point2D *pts, u32 nb_pts);


/* 2D Path constants */

/*point tags*/
enum
{
	/*point is on curve (moveTo, lineTo, end of splines)*/
	GF_PATH_CURVE_ON = 1,
	/*point is a subpath close*/
	GF_PATH_CLOSE	= 5,
	/*point is conic control point*/
	GF_PATH_CURVE_CONIC = 0,
	/*point is cubic control point*/
	GF_PATH_CURVE_CUBIC = 2,
};


/*path flags*/
enum
{
	/*zero-nonzero rule - if not set, uses odd/even rule*/
	GF_PATH_FILL_ZERO_NONZERO = 1,
	/*when set bbox must be recomputed - used to avoid wasting time on bounds calculation*/
	GF_PATH_BBOX_DIRTY = 2,
	/*nidicates path has been flattened*/
	GF_PATH_FLATTENED = 4,
};

/* 2D Polygon convexity checking */
enum
{
	/*complex/unknown*/
	GF_POLYGON_COMPLEX,
	/*counter-clockwise convex*/
	GF_POLYGON_CONVEX_CCW,
	/*clockwise convex*/
	GF_POLYGON_CONVEX_CW,
	/*convex line (degenerated path - all segments aligned)*/
	GF_POLYGON_CONVEX_LINE
};

/*stencil alignment*/
typedef enum
{	
	/*line is centered on path (-width/2, +width/2)*/
	GF_PATH_LINE_CENTER = 0,
	/*line is inside path*/
	GF_PATH_LINE_INSIDE,
	/*line is outside path*/
	GF_PATH_LINE_OUTSIDE,
} GF_LineAlignement;

/*line cap*/
typedef enum
{
	/*end of line is flat*/
	GF_LINE_CAP_FLAT = 0,
	/*end of line is round*/
	GF_LINE_CAP_ROUND,
	/*end of line is square*/
	GF_LINE_CAP_SQUARE,
	/*end of line is triangle*/
	GF_LINE_CAP_TRIANGLE,
} GF_LineCap;

/*line join - match XLineProperties values*/
typedef enum
{
	/*miter join*/
	GF_LINE_JOIN_MITER = 0,
	/*round join*/
	GF_LINE_JOIN_ROUND,
	/*bevel join*/
	GF_LINE_JOIN_BEVEL,
} GF_LineJoin;

/*dash styles*/
typedef enum
{
	/*no dash - default*/
	GF_DASH_STYLE_PLAIN = 0,
	GF_DASH_STYLE_DASH,
	GF_DASH_STYLE_DOT,
	GF_DASH_STYLE_DASH_DOT,
	GF_DASH_STYLE_DASH_DASH_DOT,
	GF_DASH_STYLE_DASH_DOT_DOT,
	GF_DASH_STYLE_CUSTOM,
} GF_DashStyle;




/*when dash style is custom, structure indicates dash pattern */
typedef struct
{
	u32 num_dash;
	Fixed *dashes;
} GF_DashSettings;

typedef struct
{
	Fixed width;
	GF_LineCap cap;
	GF_LineJoin join;
	GF_LineAlignement align;
	Fixed miterLimit;
	GF_DashStyle dash;
	Fixed dash_offset;
	/*valid only when dash style is GF_DASH_STYLE_CUSTOM*/
	GF_DashSettings *dash_set;
} GF_PenSettings;

GF_Path *gf_path_get_outline(GF_Path *path, GF_PenSettings pen);



#ifdef __cplusplus
}
#endif


#endif	/*_GF_PATH2D_H_*/

