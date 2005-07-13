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

#ifndef _GF_MATH_H_
#define _GF_MATH_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/setup.h>
	
/*
	NOTE: there is a conflict on Win32 VC6 with C++ and gpac headers when including <math.h>
*/
#ifndef __cplusplus
#include <math.h>
#endif

/*winCE always uses fixed-point version*/
#ifndef GPAC_FIXED_POINT
/*note: 
		to turn fp on, change to GPAC_FIXED_POINT
		to turn fp off, change to GPAC_NO_FIXED_POINT
	this is needed by configure+sed to modify this file directly
*/
#define GPAC_NO_FIXED_POINT
#endif


/*****************************************************************************************
			FIXED-POINT SUPPORT - HARDCODED FOR 16.16 representation
	the software rasterizer also use a 16.16 representation even in non-fixed version
******************************************************************************************/

#ifdef GPAC_FIXED_POINT

typedef s32 Fixed;

#define FIX_ONE			0x10000L
#define INT2FIX(v)		((Fixed)( ((s32) (v) ) << 16))
#define FLT2FIX(v)		((Fixed) ((v) * FIX_ONE))
#define FIX2INT(v)		((s32)(((v)+((FIX_ONE>>1)))>>16))
#define FIX2FLT(v)		((Float)( ((Float)(v)) / ((Float) FIX_ONE)))
#define FIX_EPSILON		2
#define FIX_MAX			0x7FFFFFFF
#define FIX_MIN			-FIX_MAX
#define GF_PI2		102944
#define GF_PI		205887
#define GF_2PI		411774

Fixed gf_mulfix(Fixed a, Fixed b);
Fixed gf_muldiv(Fixed a, Fixed b, Fixed c);
Fixed gf_divfix(Fixed a, Fixed b);

Fixed gf_cos(Fixed angle);
Fixed gf_sin(Fixed angle);
Fixed gf_tan(Fixed angle);
Fixed gf_acos(Fixed angle);
Fixed gf_asin(Fixed angle);
Fixed gf_atan2(Fixed x, Fixed y);
Fixed gf_sqrt(Fixed x);
Fixed gf_ceil(Fixed a);
Fixed gf_floor(Fixed a);

#else


typedef Float Fixed;

#define FIX_ONE			1.0f
#define INT2FIX(v)		((Float) (v))
#define FLT2FIX(v)		((Float) (v))
#define FIX2INT(v)		((s32)(v))
#define FIX2FLT(v)		((Float) (v))
#define FIX_EPSILON		GF_EPSILON_FLOAT
#define FIX_MAX			GF_MAX_FLOAT
#define FIX_MIN			-GF_MAX_FLOAT

#define GF_PI2		1.5707963267949f
#define GF_PI		3.1415926535898f
#define GF_2PI		6.2831853071796f

#define gf_mulfix(_a, _b)		((_a)*(_b))
#define gf_muldiv(_a, _b, _c)	((_c) ? (_a)*(_b)/(_c) : GF_MAX_FLOAT)
#define gf_divfix(_a, _b)		((_b) ? (_a)/(_b) : GF_MAX_FLOAT)


#define gf_cos(_a) ((Float) cos(_a))
#define gf_sin(_a) ((Float) sin(_a))
#define gf_tan(_a) ((Float) tan(_a))
#define gf_atan2(_x, _y) ((Float) atan2(_y, _x))
#define gf_sqrt(_x) ((Float) sqrt(_x))
#define gf_ceil(_a) ((Float) ceil(_a))
#define gf_floor(_a) ((Float) floor(_a))
#define gf_acos(_a) ((Float) acos(_a))
#define gf_asin(_a) ((Float) asin(_a))

#endif

Fixed gf_angle_diff(Fixed a, Fixed b);


typedef struct __vec2f
{
	Fixed x;
	Fixed y;
} GF_Point2D;

Fixed gf_v2d_len(GF_Point2D *vec);
void gf_v2d_from_polar(GF_Point2D*  vec, Fixed length, Fixed angle);

/*rectangle*/
typedef struct
{
	Fixed x, y;
	Fixed width, height;
} GF_Rect;

void gf_rect_union(GF_Rect *rc1, GF_Rect *rc2);
void gf_rect_center(GF_Rect *rc, Fixed w, Fixed h);
Bool gf_rect_overlaps(GF_Rect rc1, GF_Rect rc2);
Bool gf_rect_equal(GF_Rect rc1, GF_Rect rc2);

/*pixel-aligned rectangle*/
typedef struct
{
	s32 x, y;
	s32 width, height;
} GF_IRect;

GF_IRect gf_rect_pixelize(GF_Rect *r);



/* matrix object used by the renderer - transformation x,y in X, Y is:
	X = m[0]*x + m[1]*y + m[2];
	Y = m[3]*x + m[4]*y + m[5];
*/
typedef struct
{
	Fixed m[6];
} GF_Matrix2D;


#define gf_mx2d_init(_obj) { memset((_obj).m, 0, sizeof(Fixed)*6); (_obj).m[0] = (_obj).m[4] = FIX_ONE; }
#define gf_mx2d_copy(_obj, from) memcpy((_obj).m, (from).m, sizeof(Fixed)*6);
#define gf_mx2d_is_identity(_obj) ((!(_obj).m[1] && !(_obj).m[2] && !(_obj).m[3] && !(_obj).m[5] && ((_obj).m[0]==FIX_ONE) && ((_obj).m[4]==FIX_ONE)) ? 1 : 0)

void gf_mx2d_add_matrix(GF_Matrix2D *_this, GF_Matrix2D *from);
void gf_mx2d_add_translation(GF_Matrix2D *_this, Fixed cx, Fixed cy);
void gf_mx2d_add_rotation(GF_Matrix2D *_this, Fixed cx, Fixed cy, Fixed angle);
void gf_mx2d_add_scale(GF_Matrix2D *_this, Fixed scale_x, Fixed scale_y);
void gf_mx2d_add_scale_at(GF_Matrix2D *_this, Fixed scale_x, Fixed scale_y, Fixed cx, Fixed cy, Fixed angle);
void gf_mx2d_add_skew(GF_Matrix2D *_this, Fixed skew_x, Fixed skew_y);
void gf_mx2d_add_skewX(GF_Matrix2D *_this, Fixed angle);
void gf_mx2d_add_skewY(GF_Matrix2D *_this, Fixed angle);
void gf_mx2d_inverse(GF_Matrix2D *_this);
void gf_mx2d_apply_coords(GF_Matrix2D *_this, Fixed *x, Fixed *y);
void gf_mx2d_apply_point(GF_Matrix2D *_this, GF_Point2D *pt);
/*gets enclosing rect of rect after transformed*/
void gf_mx2d_apply_rect(GF_Matrix2D *_this, GF_Rect *rc);


/*color matrix object - can be used by any stencil type and performs complete
color space transformation (shift, rotate, skew, add)*/
typedef struct
{
/*the matrix coefs are in rgba order eg the color RGBA is multiplied by:
	R'		m0	m1	m2	m3	m4			R
	G'		m5	m6	m7	m8	m9			G
	B'	=	m10	m11	m12	m13	m14		x	B
	A'		m15	m16	m17	m18	m19			A
	0		0	0	0	0	1			0
	Coeficients are in intensity scale (range [0, 1])
*/
	Fixed m[20];
	/*internal flag to speed up things when matrix is identity READ ONLY - DO NOT MODIFY*/
	u32 identity;
} GF_ColorMatrix;


/*base color tools - color is of the form 0xAARRGGBB*/
typedef u32 GF_Color;
#define GF_COL_ARGB(a, r, g, b) ((a)<<24 | (r)<<16 | (g)<<8 | (b))

#define GF_COL_A(c) (u8) ((c)>>24)
#define GF_COL_R(c) (u8) ( ((c)>>16) & 0xFF)
#define GF_COL_G(c) (u8) ( ((c)>>8) & 0xFF)
#define GF_COL_B(c) (u8) ( (c) & 0xFF)
#define GF_COL_565(r, g, b) (u16) (((r & 248)<<8) + ((g & 252)<<3)  + (b>>3))
#define GF_COL_555(r, g, b) (u16) (((r & 248)<<7) + ((g & 248)<<2)  + (b>>3))
#define GF_COL_AG(a, g) (u16) ( (a << 8) | g)
#define GF_COL_TO_565(c) (((GF_COL_R(c) & 248)<<8) + ((GF_COL_G(c) & 252)<<3)  + (GF_COL_B(c)>>3))
#define GF_COL_TO_555(c) (((GF_COL_R(c) & 248)<<7) + ((GF_COL_G(c) & 248)<<2)  + (GF_COL_B(c)>>3))
#define GF_COL_TO_AG(c) ( (GF_COL_A(c) << 8) | GF_COL_R(c))


void gf_cmx_init(GF_ColorMatrix *_this);
void gf_cmx_set_all(GF_ColorMatrix *_this, Fixed *coefs);
void gf_cmx_set(GF_ColorMatrix *_this, 
				 Fixed mrr, Fixed mrg, Fixed mrb, Fixed mra, Fixed tr,
				 Fixed mgr, Fixed mgg, Fixed mgb, Fixed mga, Fixed tg,
				 Fixed mbr, Fixed mbg, Fixed mbb, Fixed mba, Fixed tb,
				 Fixed mar, Fixed mag, Fixed mab, Fixed maa, Fixed ta);
void gf_cmx_copy(GF_ColorMatrix *_this, GF_ColorMatrix *from);
void gf_cmx_multiply(GF_ColorMatrix *_this, GF_ColorMatrix *w);
GF_Color gf_cmx_apply(GF_ColorMatrix *_this, GF_Color col);
void gf_cmx_apply_fixed(GF_ColorMatrix *_this, Fixed *a, Fixed *r, Fixed *g, Fixed *b);

#define GF_COL_ARGB_FIXED(_a, _r, _g, _b) GF_COL_ARGB(FIX2INT(255*(_a)), FIX2INT(255*(_r)), FIX2INT(255*(_g)), FIX2INT(255*(_b)))

/*
		3D tools

*/

/*vector/point*/
typedef struct __vec3f
{
	Fixed x;
	Fixed y;
	Fixed z;
} GF_Vec;

/*base vector operations are MACROs for faster access*/
#define gf_vec_equal(v1, v2) (((v1).x == (v2).x) && ((v1).y == (v2).y) && ((v1).z == (v2).z))
#define gf_vec_diff(res, v1, v2) { (res).x = (v1).x - (v2).x; (res).y = (v1).y - (v2).y; (res).z = (v1).z - (v2).z; }
#define gf_vec_rev(v) { (v).x = -(v).x; (v).y = -(v).y; (v).z = -(v).z; }
#define gf_vec_add(res, v1, v2) { (res).x = (v1).x + (v2).x; (res).y = (v1).y + (v2).y; (res).z = (v1).z + (v2).z; }

Fixed gf_vec_len(GF_Vec v);
Fixed gf_vec_lensq(GF_Vec v);
Fixed gf_vec_dot(GF_Vec v1, GF_Vec v2);
void gf_vec_norm(GF_Vec *v);
GF_Vec gf_vec_scale(GF_Vec v, Fixed f);
GF_Vec gf_vec_cross(GF_Vec v1, GF_Vec v2);


/*vector4/quat/rotation*/
typedef struct __vec4f
{
	Fixed x;
	Fixed y;
	Fixed z;
	Fixed q;
} GF_Vec4;


/*matrix*/
typedef struct
{
	Fixed m[16];
} GF_Matrix;


/*quaternions tools*/
#define gf_quat_len(v) gf_sqrt(gf_mulfix((v).q,(v).q) + gf_mulfix((v).x,(v).x) + gf_mulfix((v).y,(v).y) + gf_mulfix((v).z,(v).z))
#define gf_quat_norm(v) { \
	Fixed __mag = gf_quat_len(v);	\
	(v).x = gf_divfix((v).x, __mag); (v).y = gf_divfix((v).y, __mag); (v).z = gf_divfix((v).z, __mag); (v).q = gf_divfix((v).q, __mag);	\
	}	\


GF_Vec4 gf_quat_to_rotation(GF_Vec4 *quat);
GF_Vec4 gf_quat_from_matrix(GF_Matrix *mx);
GF_Vec4 gf_quat_from_rotation(GF_Vec4 rot);
GF_Vec4 gf_quat_get_inv(GF_Vec4 *quat);
GF_Vec4 gf_quat_multiply(GF_Vec4 *q1, GF_Vec4 *q2);
GF_Vec gf_quat_rotate(GF_Vec4 *quat, GF_Vec *vec);
GF_Vec4 gf_quat_from_axis_cos(GF_Vec axis, Fixed cos_a);
GF_Vec4 gf_quat_slerp(GF_Vec4 q1, GF_Vec4 q2, Fixed frac);

/*Axis-Aligned Bounding-Box*/
typedef struct
{
	/*min_x, min_y, min_z*/
	GF_Vec min_edge;
	/*max_x, max_y, max_z*/
	GF_Vec max_edge;

	/*center of bbox*/
	GF_Vec center;
	/*radius bbox bounding sphere*/
	Fixed radius;
	/*bbox content is valid*/
	Bool is_set;
} GF_BBox;

void gf_bbox_refresh(GF_BBox *b);
void gf_bbox_from_rect(GF_BBox *box, GF_Rect *rc);
void gf_rect_from_bbox(GF_Rect *rc, GF_BBox *box);
void gf_bbox_grow_point(GF_BBox *box, GF_Vec pt);
void gf_bbox_union(GF_BBox *b1, GF_BBox *b2);
Bool gf_bbox_equal(GF_BBox *b1, GF_BBox *b2);
Bool gf_bbox_point_inside(GF_BBox *box, GF_Vec *p);
/*vertices are ordered to respect p vertex indexes (vertex from bbox closer to plane)
and so that n-vertex (vertex from bbox farther from plane) is 7-p_vx_idx*/
void gf_bbox_get_vertices(GF_Vec bmin, GF_Vec bmax, GF_Vec *vecs);


/*matrix tools - all tools assume affine matrices, except when _4x4 is specified*/
#define gf_mx_init(_obj) { memset((_obj).m, 0, sizeof(Fixed)*16); (_obj).m[0] = (_obj).m[5] = (_obj).m[10] = (_obj).m[15] = FIX_ONE; }
#define gf_mx_copy(_obj, from) memcpy(&(_obj), &(from), sizeof(GF_Matrix));
/*converts 2D matrix to 3D matrix*/
void gf_mx_from_mx2d(GF_Matrix *mat, GF_Matrix2D *mat2D);
/*returns 1 if matrices are same*/
Bool gf_mx_equal(GF_Matrix *mx1, GF_Matrix *mx2);
/*translates matrix*/
void gf_mx_add_translation(GF_Matrix *mat, Fixed tx, Fixed ty, Fixed tz);
/*scales matrix*/
void gf_mx_add_scale(GF_Matrix *mat, Fixed sx, Fixed sy, Fixed sz);
/*rotates matrix*/
void gf_mx_add_rotation(GF_Matrix *mat, Fixed angle, Fixed x, Fixed y, Fixed z);
/*multiply: mat = mat*mul*/
void gf_mx_add_matrix(GF_Matrix *mat, GF_Matrix *mul);
/*affine matrix inversion*/
void gf_mx_inverse(GF_Matrix *mx);
void gf_mx_apply_vec(GF_Matrix *mx, GF_Vec *pt);
/*gets enclosing rect of rect after transformed (matrix is considered as 2D)*/
void gf_mx_apply_rect(GF_Matrix *_this, GF_Rect *rc);
/*creates ortho matrix*/
void gf_mx_ortho(GF_Matrix *mx, Fixed left, Fixed right, Fixed bottom, Fixed top, Fixed z_near, Fixed z_far);
/*creates perspective matrix*/
void gf_mx_perspective(GF_Matrix *mx, Fixed fieldOfView, Fixed aspectRatio, Fixed z_ear, Fixed z_far);
/*creates look matrix*/
void gf_mx_lookat(GF_Matrix *mx, GF_Vec position, GF_Vec target, GF_Vec upVector);
/*gets enclosing box of box after transformed*/
void gf_mx_apply_bbox(GF_Matrix *mx, GF_BBox *b);
/*multiply: mat = mat*mul as full 4x4 matrices*/
void gf_mx_add_matrix_4x4(GF_Matrix *mat, GF_Matrix *mul);
/*generic 4x4 matrix inversion - returns 0 if failure*/
Bool gf_mx_inverse_4x4(GF_Matrix *mx);
/*apply 4th dim vector*/
void gf_mx_apply_vec_4x4(GF_Matrix *mx, GF_Vec4 *vec);
/*decomposes matrix into translation, scale, shear and rotate - only use with affine matrix */
void gf_mx_decompose(GF_Matrix *mx, GF_Vec *translate, GF_Vec *scale, GF_Vec4 *rotate, GF_Vec *shear);
/*only rotates the vector*/
void gf_mx_rotate_vector(GF_Matrix *mx, GF_Vec *pt);
/*rotation matrix transforming local axis in given norm vectors*/
void gf_mx_rotation_matrix_from_vectors(GF_Matrix *mx, GF_Vec x_axis, GF_Vec y_axis, GF_Vec z_axis);
/*trash all z info and gets a 2D matrix*/
void gf_mx2d_from_mx(GF_Matrix2D *mat2D, GF_Matrix *mat);


typedef struct
{
	GF_Vec normal;
	Fixed d;
} GF_Plane;

void gf_mx_apply_plane(GF_Matrix *mx, GF_Plane *plane);
Fixed gf_plane_get_distance(GF_Plane *plane, GF_Vec *p);
/*returns closest point on line from a given point in space*/
GF_Vec gf_closest_point_to_line(GF_Vec line_pt, GF_Vec line_vec, GF_Vec pt);
/*return p-vertex index (vertex from bbox closer to plane) - index range from 0 to 8*/
u32 gf_plane_get_p_vertex_idx(GF_Plane *p);
Bool gf_plane_intersect_line(GF_Plane *plane, GF_Vec *linepoint, GF_Vec *linevec, GF_Vec *outPoint);

/*classify box/plane position. Retturns one of the following*/
enum 
{	
	GF_BBOX_FRONT, /*box is in front of the plane*/
	GF_BBOX_INTER, /*box intersects the plane*/
	GF_BBOX_BACK /*box is back of the plane*/
};

u32 gf_bbox_plane_relation(GF_BBox *box, GF_Plane *p);




typedef struct
{
	GF_Vec orig;
	GF_Vec dir;
} GF_Ray;

GF_Ray gf_ray(GF_Vec start, GF_Vec end);
void gf_mx_apply_ray(GF_Matrix *mx, GF_Ray *r);
/*retuns 1 if intersection and stores value in outPoint*/
Bool gf_ray_hit_box(GF_Ray *ray, GF_Vec min_edge, GF_Vec max_edge, GF_Vec *outPoint);
/*retuns 1 if intersection and stores value in outPoint - if @center is NULL, assumes center is origin(0, 0, 0)*/
Bool gf_ray_hit_sphere(GF_Ray *ray, GF_Vec *center, Fixed radius, GF_Vec *outPoint);
/*retuns 1 if intersection with triangle and stores distance on ray value */
Bool gf_ray_hit_triangle(GF_Ray *ray, GF_Vec *v0, GF_Vec *v1, GF_Vec *v2, Fixed *dist);
/*same as above and performs backface cull (solid meshes)*/
Bool gf_ray_hit_triangle_backcull(GF_Ray *ray, GF_Vec *v0, GF_Vec *v1, GF_Vec *v2, Fixed *dist);


#ifdef __cplusplus
}
#endif


#endif		/*_GF_MATH_H_*/

