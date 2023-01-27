/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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


#include <gpac/internal/mesh.h>
#include <gpac/color.h>

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)

/*for GPAC_HAS_GLU*/
#include "gl_inc.h"

#ifndef CALLBACK
#define CALLBACK
#endif


#ifdef GPAC_HAS_GLU

#ifdef GPAC_CONFIG_IOS
#define GLdouble GLfloat
#endif


typedef struct
{
	/*for tesselation*/
	GLUtesselator *tess_obj;
	GF_Mesh *mesh;

	/*vertex indices: we cannot use a static array because reallocating the array will likely change memory
	address of indices, hence break triangulator*/
	GF_List *vertex_index;
} MeshTess;

static void CALLBACK mesh_tess_begin(GLenum which) {
	assert(which==GL_TRIANGLES);
}
static void CALLBACK mesh_tess_end(void) {
}
static void CALLBACK mesh_tess_error(GLenum error_code)
{
	if (error_code)
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Mesh] Tesselate error %s\n", gluErrorString(error_code)));
}

/*only needed to force GL_TRIANGLES*/
static void CALLBACK mesh_tess_edgeflag(GLenum flag) { }

static void CALLBACK mesh_tess_vertex(void *vertexData, void *user_data)
{
	MeshTess *tess = (MeshTess *) user_data;
	mesh_set_index(tess->mesh, *(u32*)vertexData);
}

static void CALLBACK mesh_tess_combine(GLdouble coords[3], void* vertex_data[4], GLfloat weight[4], void** out_data, void *user_data)
{
	u32 i, idx;
	u32 *new_idx;
	SFVec3f n;
	SFVec2f tx;
	SFColor col;

	MeshTess *tess = (MeshTess *) user_data;

	col.red = col.green = col.blue = 0;
	if (tess->mesh->flags & MESH_HAS_COLOR) {
		for (i=0; i<4; i++) {
			if (weight[i]) {
				SFColorRGBA rgba;
				Fixed _weight = FLT2FIX(weight[i]);
				idx = * (u32 *) vertex_data[i];

				MESH_GET_COLOR(rgba, tess->mesh->vertices[idx]);

				col.red += gf_mulfix(_weight, rgba.red);
				col.green += gf_mulfix(_weight, rgba.green);
				col.blue += gf_mulfix(_weight, rgba.blue);
			}
		}
	}

	n.x = n.y = n.z = 0;
	if (tess->mesh->flags & MESH_IS_2D) {
		n.z = FIX_ONE;
	} else {
		for (i=0; i<4; i++) {
			if (weight[i]) {
				Fixed _weight = FLT2FIX(weight[i]);
				SFVec3f _n;
				idx = * (u32 *) vertex_data[i];
				MESH_GET_NORMAL(_n, tess->mesh->vertices[idx]);
				n.x += gf_mulfix(_weight, _n.x);
				n.y += gf_mulfix(_weight, _n.y);
				n.z += gf_mulfix(_weight, _n.z);
			}
		}
	}
	tx.x = tx.y = 0;
	if (!(tess->mesh->flags & MESH_NO_TEXTURE)) {
		for (i=0; i<4; i++) {
			if (weight[i]) {
				Fixed _weight = FLT2FIX(weight[i]);
				idx = * (u32 *) vertex_data[i];
				tx.x += gf_mulfix(_weight, tess->mesh->vertices[idx].texcoords.x);
				tx.y += gf_mulfix(_weight, tess->mesh->vertices[idx].texcoords.y);
			}
		}
	}

	new_idx = (u32 *) gf_malloc(sizeof(u32));
	gf_list_add(tess->vertex_index, new_idx);
	*new_idx = tess->mesh->v_count;
	mesh_set_vertex(tess->mesh, FLT2FIX( (Float) coords[0]), FLT2FIX( (Float) coords[1]), FLT2FIX( (Float) coords[2]), n.x, n.y, n.z, tx.x, tx.y);
	*out_data = new_idx;
}

void gf_mesh_tesselate_path(GF_Mesh *mesh, GF_Path *path, u32 outline_style)
{
	u32 i, j, cur, nb_pts;
	u32 *idx;
	Fixed w, h, min_y;
	GF_Rect rc;
	GLdouble vertex[3];
	MeshTess *tess;
	if (!mesh || !path || !path->n_contours) return;
	tess = gf_malloc(sizeof(MeshTess));
	if (!tess) return;
	memset(tess, 0, sizeof(MeshTess));
	tess->tess_obj = gluNewTess();
	if (!tess->tess_obj) {
		gf_free(tess);
		return;
	}
	tess->vertex_index = gf_list_new();

	mesh_reset(mesh);
	mesh->flags |= MESH_IS_2D;
	if (outline_style==1) mesh->flags |= MESH_NO_TEXTURE;

	tess->mesh = mesh;
	gluTessCallback(tess->tess_obj, GLU_TESS_VERTEX_DATA, (void (CALLBACK*)()) &mesh_tess_vertex);
	gluTessCallback(tess->tess_obj, GLU_TESS_BEGIN, (void (CALLBACK*)()) &mesh_tess_begin);
	gluTessCallback(tess->tess_obj, GLU_TESS_END, (void (CALLBACK*)()) &mesh_tess_end);
	gluTessCallback(tess->tess_obj, GLU_TESS_COMBINE_DATA, (void (CALLBACK*)()) &mesh_tess_combine);
	gluTessCallback(tess->tess_obj, GLU_TESS_ERROR, (void (CALLBACK*)()) &mesh_tess_error);
	gluTessCallback(tess->tess_obj, GLU_EDGE_FLAG,(void (CALLBACK*)()) &mesh_tess_edgeflag);

	if (path->flags & GF_PATH_FILL_ZERO_NONZERO) gluTessProperty(tess->tess_obj, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_NONZERO);

	gluTessBeginPolygon(tess->tess_obj, tess);
	gluTessNormal(tess->tess_obj, 0, 0, 1);

	gf_path_flatten(path);
	gf_path_get_bounds(path, &rc);

	w = rc.width;
	h = rc.height;
	min_y = rc.y - h;
	vertex[2] = 0;
	/*since we're not sure whether subpaths overlaps or not, tesselate everything*/
	cur = 0;
	for (i=0; i<path->n_contours; i++) {
		nb_pts = 1+path->contours[i]-cur;

		gluTessBeginContour(tess->tess_obj);

		for (j=0; j<nb_pts; j++) {
			GF_Point2D pt = path->points[cur+j];
			Fixed u = gf_divfix(pt.x - rc.x, w);
			Fixed v = gf_divfix(pt.y - min_y, h);

			idx = (u32 *) gf_malloc(sizeof(u32));
			*idx = mesh->v_count;
			gf_list_add(tess->vertex_index, idx);
			mesh_set_vertex(mesh, pt.x, pt.y, 0, 0, 0, FIX_ONE, u, v);

			vertex[0] = (Double) FIX2FLT(pt.x);
			vertex[1] = (Double) FIX2FLT(pt.y);
			gluTessVertex(tess->tess_obj, vertex, idx);
		}
		gluTessEndContour(tess->tess_obj);
		cur+=nb_pts;
	}

	gluTessEndPolygon(tess->tess_obj);

	gluDeleteTess(tess->tess_obj);

	while (gf_list_count(tess->vertex_index)) {
		u32 *idx = gf_list_get(tess->vertex_index, 0);
		gf_list_rem(tess->vertex_index, 0);
		gf_free(idx);
	}
	gf_list_del(tess->vertex_index);
	gf_free(tess);

	mesh->bounds.min_edge.x = rc.x;
	mesh->bounds.min_edge.y = rc.y-rc.height;
	mesh->bounds.max_edge.x = rc.x+rc.width;
	mesh->bounds.max_edge.y = rc.y;
	mesh->bounds.min_edge.z = mesh->bounds.max_edge.z = 0;
	gf_bbox_refresh(&mesh->bounds);

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		mesh_tess_error(0);
	}
#endif

}

#else

void gf_mesh_tesselate_path(GF_Mesh *mesh, GF_Path *path, u32 outline_style) { }

#endif



#define GetPoint2D(pt, apt) \
	if (!direction) { pt.x = - apt.pos.z; pt.y = apt.pos.y;	}	\
	else if (direction==1) { pt.x = apt.pos.z; pt.y = apt.pos.x; }	\
	else if (direction==2) { pt.x = apt.pos.x; pt.y = apt.pos.y; } \
 

#define ConvCompare(delta)	\
    ( (delta.x > 0) ? -1 :		\
      (delta.x < 0) ?	1 :		\
      (delta.y > 0) ? -1 :		\
      (delta.y < 0) ?	1 :	\
      0 )

#define ConvGetPointDelta(delta, pprev, pcur )			\
    /* Given a previous point 'pprev', read a new point into 'pcur' */	\
    /* and return delta in 'delta'.				    */	\
    GetPoint2D(pcur, pts[iread]); iread++;			\
    delta.x = pcur.x - pprev.x;					\
    delta.y = pcur.y - pprev.y;					\
 
#define ConvCross(p, q) gf_mulfix(p.x,q.y) - gf_mulfix(p.y,q.x);

#define ConvCheckTriple						\
    if ( (thisDir = ConvCompare(dcur)) == -curDir ) {			\
	  ++dirChanges;							\
	  /* if ( dirChanges > 2 ) return NotConvex;		     */ \
    }									\
    curDir = thisDir;							\
    cross = ConvCross(dprev, dcur);					\
    if ( cross > 0 ) { \
		if ( angleSign == -1 ) return GF_POLYGON_COMPLEX;		\
		angleSign = 1;					\
	}							\
    else if (cross < 0) {	\
		if (angleSign == 1) return GF_POLYGON_COMPLEX;		\
		angleSign = -1;				\
	}						\
    pSecond = pThird;		\
    dprev.x = dcur.x;		\
    dprev.y = dcur.y;							\
 
u32 polygon_check_convexity(GF_Vertex *pts, u32 len, u32 direction)
{
	s32 curDir, thisDir = 0, dirChanges = 0, angleSign = 0;
	u32 iread;
	Fixed cross;
	GF_Point2D pSecond, pThird, pSaveSecond;
	GF_Point2D dprev, dcur;

	if (len<3) return GF_POLYGON_CONVEX_LINE;

	pSecond.x = pSecond.y = 0;
	pThird = pSecond;

	GetPoint2D(pThird, pts[0]);
	iread = 1;
	ConvGetPointDelta(dprev, pThird, pSecond);
	pSaveSecond = pSecond;
	/*initial direction */
	curDir = ConvCompare(dprev);
	while ( iread < len) {
		/* Get different point, break if no more points */
		ConvGetPointDelta(dcur, pSecond, pThird );
		if ( (dcur.x == 0) && (dcur.y == 0) ) continue;
		/* Check current three points */
		ConvCheckTriple;
	}

	/* Must check for direction changes from last vertex back to first */
	/* Prepare for 'ConvexCheckTriple' */
	GetPoint2D(pThird, pts[0]);
	dcur.x = pThird.x - pSecond.x;
	dcur.y = pThird.y - pSecond.y;
	if ( ConvCompare(dcur) ) {
		ConvCheckTriple;
	}
	/* and check for direction changes back to second vertex */
	dcur.x = pSaveSecond.x - pSecond.x;
	dcur.y = pSaveSecond.y - pSecond.y;
	/* Don't care about 'pThird' now */
	ConvCheckTriple;

	/* Decide on polygon type given accumulated status */
	if ( dirChanges > 2 ) return GF_POLYGON_COMPLEX;
	if ( angleSign > 0 ) return GF_POLYGON_CONVEX_CCW;
	if ( angleSign < 0 ) return GF_POLYGON_CONVEX_CW;
	return GF_POLYGON_CONVEX_LINE;
}


void TesselateFaceMesh(GF_Mesh *dest, GF_Mesh *orig)
{
	u32 poly_type, i, nb_pts, init_idx, direction;
	Fixed max_nor_coord, c;
	SFVec3f nor;
#ifdef GPAC_HAS_GLU
	u32 *idx;
	GLdouble vertex[3];
	MeshTess *tess;
#endif

	/*get normal*/
	if (orig->flags & MESH_IS_2D) {
		nor.x = nor.y = 0;
		nor.z = FIX_ONE;
	} else {
		MESH_GET_NORMAL(nor, orig->vertices[0]);
	}

	/*select projection direction*/
	direction = 0;
	max_nor_coord = ABS(nor.x);
	c = ABS(nor.y);
	if (c>max_nor_coord) {
		direction = 1;
		max_nor_coord = c;
	}
	c = ABS(nor.z);
	if (c>max_nor_coord) direction = 2;

	/*if this is a convex polygone don't triangulate*/
	poly_type = polygon_check_convexity(orig->vertices, orig->v_count, direction);
	switch (poly_type) {
	case GF_POLYGON_CONVEX_LINE:
	/*do NOT try to make face CCW otherwise we loose front/back faces...*/
	case GF_POLYGON_CONVEX_CW:
	case GF_POLYGON_CONVEX_CCW:
		init_idx = dest->v_count;
		nb_pts = orig->v_count;
		for (i=0; i<nb_pts; i++) {
			mesh_set_vertex_vx(dest, &orig->vertices[i]);
		}
		nb_pts -= 1;
		for (i=1; i<nb_pts; i++) {
			mesh_set_triangle(dest, init_idx, init_idx + i, init_idx + i+1);
		}
		return;
	default:
		break;
	}

#ifdef GPAC_HAS_GLU

	/*tesselate it*/
	tess = gf_malloc(sizeof(MeshTess));
	if (!tess) return;
	memset(tess, 0, sizeof(MeshTess));
	tess->tess_obj = gluNewTess();
	if (!tess->tess_obj) {
		gf_free(tess);
		return;
	}
	tess->vertex_index = gf_list_new();

	tess->mesh = dest;
	gluTessCallback(tess->tess_obj, GLU_TESS_VERTEX_DATA, (void (CALLBACK*)()) &mesh_tess_vertex);
	gluTessCallback(tess->tess_obj, GLU_TESS_BEGIN, (void (CALLBACK*)()) &mesh_tess_begin);
	gluTessCallback(tess->tess_obj, GLU_TESS_END, (void (CALLBACK*)()) &mesh_tess_end);
	gluTessCallback(tess->tess_obj, GLU_TESS_COMBINE_DATA, (void (CALLBACK*)()) &mesh_tess_combine);
	gluTessCallback(tess->tess_obj, GLU_TESS_ERROR, (void (CALLBACK*)()) &mesh_tess_error);
	gluTessCallback(tess->tess_obj, GLU_EDGE_FLAG,(void (CALLBACK*)()) &mesh_tess_edgeflag);

	gluTessBeginPolygon(tess->tess_obj, tess);
	gluTessBeginContour(tess->tess_obj);


	for (i=0; i<orig->v_count; i++) {
		idx = (u32 *) gf_malloc(sizeof(u32));
		*idx = dest->v_count;
		gf_list_add(tess->vertex_index, idx);
		mesh_set_vertex_vx(dest, &orig->vertices[i]);

		vertex[0] = (Double) FIX2FLT(orig->vertices[i].pos.x);
		vertex[1] = (Double) FIX2FLT(orig->vertices[i].pos.y);
		vertex[2] = (Double) FIX2FLT(orig->vertices[i].pos.z);
		gluTessVertex(tess->tess_obj, vertex, idx);
	}

	gluTessEndContour(tess->tess_obj);
	gluTessEndPolygon(tess->tess_obj);
	gluDeleteTess(tess->tess_obj);

	while (gf_list_count(tess->vertex_index)) {
		u32 *idx = gf_list_get(tess->vertex_index, 0);
		gf_list_rem(tess->vertex_index, 0);
		gf_free(idx);
	}
	gf_list_del(tess->vertex_index);
	gf_free(tess);
#endif
}



#ifdef GPAC_HAS_GLU

void TesselateFaceMeshComplex(GF_Mesh *dest, GF_Mesh *orig, u32 nbFaces, u32 *ptsPerFaces)
{
	u32 i, cur_pt_faces, cur_face;
	u32 *idx;
	GLdouble vertex[3];
	MeshTess *tess;

	/*tesselate it*/
	tess = gf_malloc(sizeof(MeshTess));
	if (!tess) return;
	memset(tess, 0, sizeof(MeshTess));
	tess->tess_obj = gluNewTess();
	if (!tess->tess_obj) {
		gf_free(tess);
		return;
	}
	tess->vertex_index = gf_list_new();

	tess->mesh = dest;
	gluTessCallback(tess->tess_obj, GLU_TESS_VERTEX_DATA, (void (CALLBACK*)()) &mesh_tess_vertex);
	gluTessCallback(tess->tess_obj, GLU_TESS_BEGIN, (void (CALLBACK*)()) &mesh_tess_begin);
	gluTessCallback(tess->tess_obj, GLU_TESS_END, (void (CALLBACK*)()) &mesh_tess_end);
	gluTessCallback(tess->tess_obj, GLU_TESS_COMBINE_DATA, (void (CALLBACK*)()) &mesh_tess_combine);
	gluTessCallback(tess->tess_obj, GLU_TESS_ERROR, (void (CALLBACK*)()) &mesh_tess_error);
	gluTessCallback(tess->tess_obj, GLU_EDGE_FLAG,(void (CALLBACK*)()) &mesh_tess_edgeflag);

	gluTessBeginPolygon(tess->tess_obj, tess);
	gluTessBeginContour(tess->tess_obj);


	cur_pt_faces = 0;
	cur_face = 0;
	for (i=0; i<orig->v_count; i++) {

		if (i>= cur_pt_faces + ptsPerFaces[cur_face]) {
			cur_pt_faces += ptsPerFaces[cur_face];
			cur_face++;
			if (cur_face>=nbFaces) break;
			gluTessEndContour(tess->tess_obj);
			gluTessBeginContour(tess->tess_obj);
		}

		idx = (u32 *) gf_malloc(sizeof(u32));
		*idx = dest->v_count;
		gf_list_add(tess->vertex_index, idx);
		mesh_set_vertex_vx(dest, &orig->vertices[i]);

		vertex[0] = (Double) FIX2FLT(orig->vertices[i].pos.x);
		vertex[1] = (Double) FIX2FLT(orig->vertices[i].pos.y);
		vertex[2] = (Double) FIX2FLT(orig->vertices[i].pos.z);
		gluTessVertex(tess->tess_obj, vertex, idx);
	}

	gluTessEndContour(tess->tess_obj);
	gluTessEndPolygon(tess->tess_obj);
	gluDeleteTess(tess->tess_obj);

	while (gf_list_count(tess->vertex_index)) {
		u32 *idx = gf_list_get(tess->vertex_index, 0);
		gf_list_rem(tess->vertex_index, 0);
		gf_free(idx);
	}
	gf_list_del(tess->vertex_index);
	gf_free(tess);
}
#endif

#endif // !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)
