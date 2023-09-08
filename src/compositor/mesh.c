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

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
#include <gpac/color.h>

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)

void compositor_get_srdmap_size(const GF_PropertyValue *srd_map, u32 *width, u32 *height, u32 *min_x, u32 *min_y);

/*for GPAC_HAS_GLU and glDeleteBuffersARB */
#include "gl_inc.h"

/*when highspeeds, amount of subdivisions for bezier and ellipse will be divided by this factor*/
#define HIGH_SPEED_RATIO	2


/*size alloc for meshes doubles memory at each gf_realloc rather than using a fix-size increment
 (this really speeds up large meshes constructing). Final memory usage is adjusted when updating mesh bounds
*/
#define MESH_CHECK_VERTEX(m)		\
	if (m->v_count == m->v_alloc) {	\
		m->v_alloc *= 2;	\
		m->vertices = (GF_Vertex *)gf_realloc(m->vertices, sizeof(GF_Vertex)*m->v_alloc);	\
	}	\
 
#define MESH_CHECK_IDX(m)		\
	if (m->i_count == m->i_alloc) {	\
		m->i_alloc *= 2;	\
		m->indices = (IDX_TYPE*)gf_realloc(m->indices, sizeof(IDX_TYPE)*m->i_alloc);	\
	}	\
 


static void del_aabb_node(AABBNode *node)
{
	if (node->pos) del_aabb_node(node->pos);
	if (node->neg) del_aabb_node(node->neg);
	gf_free(node);
}

void mesh_reset(GF_Mesh *mesh)
{
	mesh->v_count = 0;
	mesh->i_count = 0;
	mesh->flags = 0;
	mesh->mesh_type = 0;
	memset(&mesh->bounds.min_edge, 0, sizeof(SFVec3f));
	memset(&mesh->bounds.max_edge, 0, sizeof(SFVec3f));
	if (mesh->aabb_root) del_aabb_node(mesh->aabb_root);
	mesh->aabb_root = NULL;
	if (mesh->aabb_indices) gf_free(mesh->aabb_indices);
	mesh->aabb_indices = NULL;


	if (mesh->vbo) {
		glDeleteBuffers(1, &mesh->vbo);
		mesh->vbo = 0;
	}
	if (mesh->vbo_idx) {
		glDeleteBuffers(1, &mesh->vbo_idx);
		mesh->vbo_idx = 0;
	}
}

void mesh_free(GF_Mesh *mesh)
{
	if (mesh->vertices) gf_free(mesh->vertices);
	if (mesh->indices) gf_free(mesh->indices);
	if (mesh->aabb_root) del_aabb_node(mesh->aabb_root);
	mesh->aabb_root = NULL;
	if (mesh->aabb_indices) gf_free(mesh->aabb_indices);
	gf_free(mesh);
}

GF_Mesh *new_mesh()
{
	GF_Mesh *mesh = (GF_Mesh *)gf_malloc(sizeof(GF_Mesh));
	if (mesh) {
		memset(mesh, 0, sizeof(GF_Mesh));
		mesh->v_alloc = 8;
		mesh->vertices = (GF_Vertex*)gf_malloc(sizeof(GF_Vertex)*mesh->v_alloc);
		mesh->i_alloc = 8;
		mesh->indices = (IDX_TYPE*)gf_malloc(sizeof(IDX_TYPE)*mesh->i_alloc);
	}
	return mesh;
}

static void mesh_fit_alloc(GF_Mesh *m)
{
	if (m->v_count && (m->v_count < m->v_alloc)) {
		m->v_alloc = m->v_count;
		m->vertices = (GF_Vertex *)gf_realloc(m->vertices, sizeof(GF_Vertex)*m->v_alloc);
	}
	if (m->i_count && (m->i_count  < m->i_alloc)) {
		m->i_alloc = m->i_count;
		m->indices = (IDX_TYPE*)gf_realloc(m->indices, sizeof(IDX_TYPE)*m->i_alloc);
	}
}


void mesh_update_bounds(GF_Mesh *mesh)
{
	u32 i;
	Fixed mx, my, mz, Mx, My, Mz;
	mx = my = mz = FIX_MAX;
	Mx = My = Mz = FIX_MIN;

	mesh_fit_alloc(mesh);

	for (i=0; i<mesh->v_count; i++) {
		SFVec3f *v = &mesh->vertices[i].pos;
		if (mx>v->x) mx=v->x;
		if (my>v->y) my=v->y;
		if (mz>v->z) mz=v->z;
		if (Mx<v->x) Mx=v->x;
		if (My<v->y) My=v->y;
		if (Mz<v->z) Mz=v->z;
	}
	mesh->bounds.min_edge.x = mx;
	mesh->bounds.min_edge.y = my;
	mesh->bounds.min_edge.z = mz;
	mesh->bounds.max_edge.x = Mx;
	mesh->bounds.max_edge.y = My;
	mesh->bounds.max_edge.z = Mz;
	gf_bbox_refresh(&mesh->bounds);
}

void mesh_clone(GF_Mesh *dest, GF_Mesh *orig)
{
	if (dest->v_alloc<orig->v_alloc) {
		dest->v_alloc = orig->v_alloc;
		dest->vertices = (GF_Vertex *)gf_realloc(dest->vertices, sizeof(GF_Vertex)*dest->v_alloc);
	}
	dest->v_count = orig->v_count;
	memcpy(dest->vertices, orig->vertices, sizeof(GF_Vertex)*dest->v_count);

	if (dest->i_alloc < orig->i_alloc) {
		dest->i_alloc = orig->i_alloc;
		dest->indices = (IDX_TYPE*)gf_realloc(dest->indices, sizeof(IDX_TYPE)*dest->i_alloc);
	}
	dest->i_count = orig->i_count;
	memcpy(dest->indices, orig->indices, sizeof(IDX_TYPE)*dest->i_count);

	dest->mesh_type = orig->mesh_type;
	dest->flags = orig->flags;
	dest->bounds = orig->bounds;
	/*and reset AABB*/
	if (dest->aabb_root) del_aabb_node(dest->aabb_root);
	dest->aabb_root = NULL;
	if (dest->aabb_indices) gf_free(dest->aabb_indices);
	dest->aabb_indices = NULL;
}


static GFINLINE GF_Vertex set_vertex(Fixed x, Fixed y, Fixed z, Fixed nx, Fixed ny, Fixed nz, Fixed u, Fixed v)
{
	SFVec3f nor;
	GF_Vertex res;
	res.pos.x = x;
	res.pos.y = y;
	res.pos.z = z;
	nor.x = nx;
	nor.y = ny;
	nor.z = nz;
	gf_vec_norm(&nor);
	MESH_SET_NORMAL(res, nor);

	res.texcoords.x = u;
	res.texcoords.y = v;
#ifdef MESH_USE_SFCOLOR
	res.color.blue = res.color.red = res.color.green = res.color.alpha = FIX_ONE;
#else
	res.color = 0xFFFFFFFF;
#endif
	return res;
}
void mesh_set_vertex(GF_Mesh *mesh, Fixed x, Fixed y, Fixed z, Fixed nx, Fixed ny, Fixed nz, Fixed u, Fixed v)
{
	MESH_CHECK_VERTEX(mesh);
	mesh->vertices[mesh->v_count] = set_vertex(x, y, z, nx, ny, nz, u, v);
	mesh->v_count++;
}

void mesh_set_vertex_v(GF_Mesh *mesh, SFVec3f pt, SFVec3f nor, SFVec2f tx, SFColorRGBA col)
{
	if (!mesh) return;
	MESH_CHECK_VERTEX(mesh);
	if (!mesh->vertices) return;
	mesh->vertices[mesh->v_count].pos = pt;
	mesh->vertices[mesh->v_count].texcoords = tx;
	mesh->vertices[mesh->v_count].color = MESH_MAKE_COL(col);
	gf_vec_norm(&nor);
	MESH_SET_NORMAL(mesh->vertices[mesh->v_count], nor);
	mesh->v_count++;
}

void mesh_set_vertex_vx(GF_Mesh *mesh, GF_Vertex *vx)
{
	MESH_CHECK_VERTEX(mesh);
	mesh->vertices[mesh->v_count] = *vx;
	mesh->v_count++;
}

void mesh_set_point(GF_Mesh *mesh, Fixed x, Fixed y, Fixed z, SFColorRGBA col)
{
	MESH_CHECK_VERTEX(mesh);
	mesh->vertices[mesh->v_count].pos.x = x;
	mesh->vertices[mesh->v_count].pos.y = y;
	mesh->vertices[mesh->v_count].pos.z = z;
	mesh->vertices[mesh->v_count].normal.x = mesh->vertices[mesh->v_count].normal.y = mesh->vertices[mesh->v_count].normal.z = 0;
	mesh->vertices[mesh->v_count].texcoords.x = mesh->vertices[mesh->v_count].texcoords.y = 0;
	mesh->vertices[mesh->v_count].color = MESH_MAKE_COL(col);
	mesh->v_count++;
}
void mesh_set_index(GF_Mesh *mesh, u32 idx)
{
	MESH_CHECK_IDX(mesh);
	mesh->indices[mesh->i_count] = (IDX_TYPE) idx;
	mesh->i_count++;
}
void mesh_set_triangle(GF_Mesh *mesh, u32 v1_idx, u32 v2_idx, u32 v3_idx)
{
	mesh_set_index(mesh, v1_idx);
	mesh_set_index(mesh, v2_idx);
	mesh_set_index(mesh, v3_idx);
}
void mesh_set_line(GF_Mesh *mesh, u32 v1_idx, u32 v2_idx)
{
	mesh_set_index(mesh, v1_idx);
	mesh_set_index(mesh, v2_idx);
}

void mesh_recompute_normals(GF_Mesh *mesh)
{
	u32 i;
	if (mesh->mesh_type) return;

	for (i=0; i<mesh->i_count; i+=3) {
		SFVec3f v1, v2, v3;
		gf_vec_diff(v1, mesh->vertices[mesh->indices[i+1]].pos, mesh->vertices[mesh->indices[i]].pos);
		gf_vec_diff(v2, mesh->vertices[mesh->indices[i+2]].pos, mesh->vertices[mesh->indices[i]].pos);
		v3 = gf_vec_cross(v1, v2);
		gf_vec_norm(&v3);
		MESH_SET_NORMAL(mesh->vertices[mesh->indices[i]], v3);
		MESH_SET_NORMAL(mesh->vertices[mesh->indices[i+1]], v3);
		MESH_SET_NORMAL(mesh->vertices[mesh->indices[i+2]], v3);
	}
}

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_X3D)
void mesh_generate_tex_coords(GF_Mesh *mesh, GF_Node *__texCoords)
{
	u32 i;
	X_TextureCoordinateGenerator *txgen = (X_TextureCoordinateGenerator *)__texCoords;

	if (!strcmp(txgen->mode.buffer, "SPHERE-LOCAL")) {
		for (i=0; i<mesh->v_count; i++) {
			GF_Vertex *vx = &mesh->vertices[i];
			vx->texcoords.x = (vx->normal.x+FIX_ONE) / 2;
			vx->texcoords.y = (vx->normal.y+FIX_ONE) / 2;
		}
	}
	else if (!strcmp(txgen->mode.buffer, "COORD")) {
		for (i=0; i<mesh->v_count; i++) {
			GF_Vertex *vx = &mesh->vertices[i];
			vx->texcoords.x = vx->pos.x;
			vx->texcoords.y = vx->pos.y;
		}
	}
}
#endif /*GPAC_DISABLE_VRML*/


void mesh_new_box(GF_Mesh *mesh, SFVec3f size)
{
	Fixed hx = size.x / 2;
	Fixed hy = size.y / 2;
	Fixed hz = size.z / 2;

	mesh_reset(mesh);
	/*back face (horiz flip of texcoords)*/
	mesh_set_vertex(mesh,  hx, -hy, -hz,  0,  0, -FIX_ONE, 0, 0);
	mesh_set_vertex(mesh, -hx, -hy, -hz,  0,  0, -FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(mesh, -hx,  hy, -hz,  0,  0, -FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh,  hx,  hy, -hz,  0,  0, -FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(mesh, 0, 1, 2);
	mesh_set_triangle(mesh, 0, 2, 3);
	/*top face*/
	mesh_set_vertex(mesh, -hx,  hy,  hz,  0,  FIX_ONE,  0, 0, 0);
	mesh_set_vertex(mesh,  hx,  hy,  hz,  0,  FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_vertex(mesh,  hx,  hy, -hz,  0,  FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh, -hx,  hy, -hz,  0,  FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_triangle(mesh, 4, 5, 6);
	mesh_set_triangle(mesh, 4, 6, 7);
	/*front face*/
	mesh_set_vertex(mesh, -hx, -hy,  hz,  0,  0,  FIX_ONE, 0, 0);
	mesh_set_vertex(mesh,  hx, -hy,  hz,  0,  0,  FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(mesh,  hx,  hy,  hz,  0,  0,  FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh, -hx,  hy,  hz,  0,  0,  FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(mesh, 8, 9, 10);
	mesh_set_triangle(mesh, 8, 10, 11);
	/*left face*/
	mesh_set_vertex(mesh, -hx, -hy, -hz, -FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(mesh, -hx, -hy,  hz, -FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(mesh, -hx,  hy,  hz, -FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh, -hx,  hy, -hz, -FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_triangle(mesh, 12, 13, 14);
	mesh_set_triangle(mesh, 12, 14, 15);
	/*bottom face*/
	mesh_set_vertex(mesh, -hx, -hy, -hz,  0, -FIX_ONE,  0, 0, 0);
	mesh_set_vertex(mesh,  hx, -hy, -hz,  0, -FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_vertex(mesh,  hx, -hy,  hz,  0, -FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh, -hx, -hy,  hz,  0, -FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_triangle(mesh, 16, 17, 18);
	mesh_set_triangle(mesh, 16, 18, 19);
	/*right face*/
	mesh_set_vertex(mesh,  hx, -hy,  hz,  FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(mesh,  hx, -hy, -hz,  FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(mesh,  hx,  hy, -hz,  FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(mesh,  hx,  hy,  hz,  FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_triangle(mesh, 20, 21, 22);
	mesh_set_triangle(mesh, 20, 22, 23);


	mesh->flags |= MESH_IS_SOLID;
	mesh->bounds.min_edge.x = -hx;
	mesh->bounds.min_edge.y = -hy;
	mesh->bounds.min_edge.z = -hz;
	mesh->bounds.max_edge.x = hx;
	mesh->bounds.max_edge.y = hy;
	mesh->bounds.max_edge.z = hz;
	gf_bbox_refresh(&mesh->bounds);
	gf_mesh_build_aabbtree(mesh);
}

void mesh_new_unit_bbox(GF_Mesh *mesh)
{
	SFColorRGBA col;
	Fixed s = FIX_ONE/2;

	memset(&col, 0, sizeof(SFColor));
	col.alpha = 1;
	mesh_reset(mesh);
	mesh->mesh_type = MESH_LINESET;
	mesh_set_point(mesh, -s, -s, -s,  col);
	mesh_set_point(mesh, s, -s, -s,  col);
	mesh_set_point(mesh, s, s, -s,  col);
	mesh_set_point(mesh, -s, s, -s,  col);
	mesh_set_point(mesh, -s, -s, s,  col);
	mesh_set_point(mesh, s, -s, s,  col);
	mesh_set_point(mesh, s, s, s,  col);
	mesh_set_point(mesh, -s, s, s,  col);

	mesh_set_line(mesh, 0, 1);
	mesh_set_line(mesh, 1, 2);
	mesh_set_line(mesh, 2, 3);
	mesh_set_line(mesh, 3, 0);
	mesh_set_line(mesh, 4, 5);
	mesh_set_line(mesh, 5, 6);
	mesh_set_line(mesh, 6, 7);
	mesh_set_line(mesh, 7, 4);
	mesh_set_line(mesh, 0, 4);
	mesh_set_line(mesh, 1, 5);
	mesh_set_line(mesh, 2, 6);
	mesh_set_line(mesh, 3, 7);
	gf_bbox_refresh(&mesh->bounds);
}


static void compute_cylinder(Fixed height, Fixed radius, s32 numFacets, SFVec3f *coords, SFVec2f *texcoords)
{
	Fixed angle, t, u;
	s32 i;
	t = height / 2;
	for (i=0; i<numFacets; ++i) {
		angle = i*GF_2PI / numFacets - GF_PI2;
		coords[i].y = t;
		coords[i].x = gf_mulfix(radius, gf_cos(angle));
		coords[i].z = gf_mulfix(radius , gf_sin(angle));
		u = FIX_ONE - i*FIX_ONE/numFacets;
		texcoords[i].x = u;
		texcoords[i].y = FIX_ONE;
	}
}

#define CYLINDER_SUBDIV	24
void mesh_new_cylinder(GF_Mesh *mesh, Fixed height, Fixed radius, Bool bottom, Bool side, Bool top, Bool low_res)
{
	u32 nfacets, i, c_idx;
	SFVec3f *coords;
	SFVec2f *texcoords;

	mesh_reset(mesh);
	if (!bottom && !side && !top) return;

	nfacets = CYLINDER_SUBDIV;
	if (low_res) nfacets /= HIGH_SPEED_RATIO;
	coords = (SFVec3f*) gf_malloc(sizeof(SFVec3f) * nfacets);
	texcoords = (SFVec2f*)gf_malloc(sizeof(SFVec2f) * nfacets);

	compute_cylinder(height, radius, nfacets, coords, texcoords);

	if (side) {
		for (i=0; i<nfacets; ++i) {
			/*top*/
			mesh_set_vertex(mesh, coords[i].x, coords[i].y, coords[i].z,
			                coords[i].x, 0, coords[i].z,
			                texcoords[i].x, FIX_ONE);

			/*bottom*/
			mesh_set_vertex(mesh, coords[i].x, -1*coords[i].y, coords[i].z,
			                coords[i].x, 0, coords[i].z,
			                texcoords[i].x, 0);


			/*top circle is counterclockwise, reverse coords*/
			if (i) {
				mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-1, mesh->v_count-3);
				mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-2, mesh->v_count-1);
			}
		}

		/*top*/
		mesh_set_vertex(mesh, coords[0].x, coords[0].y, coords[0].z,
		                coords[0].x, 0, coords[0].z,
		                texcoords[0].x - FIX_ONE, FIX_ONE);

		/*bottom*/
		mesh_set_vertex(mesh, coords[0].x, -1*coords[0].y, coords[0].z,
		                coords[0].x, 0, coords[0].z,
		                texcoords[0].x - FIX_ONE, 0);

		mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-1, mesh->v_count-3);
		mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-2, mesh->v_count-1);
	}

	if (bottom) {
		Fixed angle = 0;
		Fixed aincr = GF_2PI / nfacets;

		mesh_set_vertex(mesh, 0, -height/2, 0, 0, -FIX_ONE, 0, FIX_ONE/2, FIX_ONE/2);
		c_idx = mesh->v_count-1;
		for (i=0; i<nfacets; ++i, angle += aincr) {
			mesh_set_vertex(mesh, coords[i].x, -1*coords[i].y, coords[i].z,
			                0, -FIX_ONE, 0,
			                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);
			if (i) mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
		}

		mesh_set_vertex(mesh, coords[0].x, -1*coords[0].y, coords[0].z,
		                0, -FIX_ONE, 0,
		                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);
		mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
	}

	if (top) {
		Fixed aincr = GF_2PI / nfacets;
		Fixed angle = GF_PI + aincr;

		mesh_set_vertex(mesh, 0, height/2, 0, 0, FIX_ONE, 0, FIX_ONE/2, FIX_ONE/2);
		c_idx = mesh->v_count-1;
		for (i=nfacets; i>0; --i, angle += aincr) {

			mesh_set_vertex(mesh, coords[i - 1].x, coords[i - 1].y, coords[i - 1].z,
			                0, FIX_ONE, 0,
			                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);
			if (i) mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
		}
		mesh_set_vertex(mesh, coords[nfacets - 1].x, coords[nfacets - 1].y, coords[nfacets - 1].z,
		                0, FIX_ONE, 0,
		                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);
		mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
	}
	gf_free(texcoords);
	gf_free(coords);

	if (top && bottom && side) mesh->flags |= MESH_IS_SOLID;

	mesh->bounds.min_edge.x = mesh->bounds.min_edge.z = -radius;
	mesh->bounds.max_edge.x = mesh->bounds.max_edge.z = radius;
	mesh->bounds.max_edge.y = (side || (top && bottom)) ? height/2 : 0;
	mesh->bounds.min_edge.y = -mesh->bounds.max_edge.y;
	gf_bbox_refresh(&mesh->bounds);

	gf_mesh_build_aabbtree(mesh);
}

#define CONE_SUBDIV	24
void mesh_new_cone(GF_Mesh *mesh, Fixed height, Fixed radius, Bool bottom, Bool side, Bool low_res)
{
	u32 nfacets, i, c_idx;
	SFVec3f *coords;
	SFVec2f *texcoords;

	mesh_reset(mesh);
	if (!bottom && !side) return;

	nfacets = CONE_SUBDIV;
	if (low_res) nfacets /= HIGH_SPEED_RATIO;
	coords = (SFVec3f*)gf_malloc(sizeof(SFVec3f) * nfacets);
	texcoords = (SFVec2f*)gf_malloc(sizeof(SFVec2f) * nfacets);

	compute_cylinder(height, radius, nfacets, coords, texcoords);

	if (side) {
		Fixed Ny = gf_muldiv(radius, radius, height);

		for (i = 0; i < nfacets; ++i) {
			/*top*/
			mesh_set_vertex(mesh, 0, coords[i].y, 0,
			                coords[i].x, Ny, coords[i].z,
			                texcoords[i].x, FIX_ONE);

			/*base*/
			mesh_set_vertex(mesh, coords[i].x, -1*coords[i].y, coords[i].z,
			                coords[i].x, Ny, coords[i].z,
			                texcoords[i].x, 0);
			if (i) {
				mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-1, mesh->v_count-3);
			}
		}
		/*top*/
		mesh_set_vertex(mesh, 0, coords[0].y, 0, coords[0].x, Ny, coords[0].z, texcoords[0].x - FIX_ONE, FIX_ONE);
		/*base*/
		mesh_set_vertex(mesh, coords[0].x, -1*coords[0].y, coords[0].z,
		                coords[0].x, Ny, coords[0].z,
		                texcoords[0].x - FIX_ONE, 0);

		mesh_set_triangle(mesh, mesh->v_count-4, mesh->v_count-1, mesh->v_count-3);
	}

	if (bottom) {
		Fixed angle = 0;
		Fixed aincr = GF_2PI / nfacets;

		mesh_set_vertex(mesh, 0, -height/2, 0, 0, -FIX_ONE, 0, FIX_ONE/2, FIX_ONE/2);
		c_idx = mesh->v_count - 1;
		for (i=0; i<nfacets; ++i, angle += aincr) {
			mesh_set_vertex(mesh, coords[i].x, -1*coords[i].y, coords[i].z,
			                0, -FIX_ONE, 0,
			                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);

			if (i)
				mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
		}
		mesh_set_vertex(mesh, coords[0].x, -1*coords[0].y, coords[0].z,
		                0, -FIX_ONE, 0,
		                (FIX_ONE + gf_sin(angle))/2, FIX_ONE - (FIX_ONE + gf_cos(angle))/2);
		mesh_set_triangle(mesh, c_idx, mesh->v_count-2, mesh->v_count-1);
	}
	gf_free(texcoords);
	gf_free(coords);

	if (bottom && side) mesh->flags |= MESH_IS_SOLID;

	mesh->bounds.min_edge.x = mesh->bounds.min_edge.z = -radius;
	mesh->bounds.max_edge.x = mesh->bounds.max_edge.z = radius;
	mesh->bounds.max_edge.y = height/2;
	mesh->bounds.min_edge.y = -mesh->bounds.max_edge.y;
	gf_bbox_refresh(&mesh->bounds);

	gf_mesh_build_aabbtree(mesh);

}

void compute_sphere(Fixed radius, SFVec3f *coords, SFVec2f *texcoords, u32 num_steps, GF_MeshSphereAngles *sphere_angles)
{
	Fixed r, angle, x, y, z;
	u32 i, j;

	for (i=0; i<num_steps; i++) {
		if (!sphere_angles) {
			angle = (i * GF_PI / (num_steps-1) ) - GF_PI2;
		} else {
			angle = (i * (sphere_angles->max_phi - sphere_angles->min_phi) / (num_steps-1) ) + sphere_angles->min_phi;
		}
		y = gf_sin(angle);
		r = gf_sqrt(FIX_ONE - gf_mulfix(y, y));
		for (j = 0; j<num_steps; j++) {
			if (!sphere_angles) {
				angle = GF_2PI * j / num_steps - GF_PI2;
			} else {
				angle = (j * (sphere_angles->max_theta - sphere_angles->min_theta) / (num_steps-1) ) + sphere_angles->min_theta;
			}
			x = gf_mulfix(gf_cos(angle), r);
			z = gf_mulfix(gf_sin(angle), r);
			coords[i * num_steps + j].x = gf_mulfix(radius, x);
			coords[i * num_steps + j].y = gf_mulfix(radius, y);
			coords[i * num_steps + j].z = gf_mulfix(radius, z);
			if (radius>0) {
				if (!sphere_angles){
					texcoords[i * num_steps + j].x = FIX_ONE - j*FIX_ONE/num_steps;
					texcoords[i * num_steps + j].y = i*FIX_ONE/num_steps;
				}else{
					texcoords[i * num_steps + j].x = j*FIX_ONE/(num_steps-1);
					texcoords[i * num_steps + j].y = FIX_ONE - i*FIX_ONE/(num_steps-1);

				}
			}else {
				if (!sphere_angles){
					texcoords[i * num_steps + j].x = j*FIX_ONE/(num_steps);
					texcoords[i * num_steps + j].y = FIX_ONE - i*FIX_ONE/(num_steps);
				}else{
					texcoords[i * num_steps + j].x = j*FIX_ONE/(num_steps-1);
					texcoords[i * num_steps + j].y = FIX_ONE - i*FIX_ONE/(num_steps-1);
				}
			}
		}
	}

}

void mesh_new_sphere(GF_Mesh *mesh, Fixed radius, Bool low_res, GF_MeshSphereAngles *sphere_angles)
{
	u32 i, j, num_steps, npts;
	SFVec3f *coords;
	SFVec2f *texcoords;

	num_steps = 48;
	//this is 360 VR, use a large number of subdivisions (1 seg for 5 degrees should be enough )
	if (radius<0) num_steps = 72;

	if (low_res) num_steps /= 2;
	if (sphere_angles) {
		Fixed min_subd1, min_subd2;
		min_subd1 = gf_divfix(sphere_angles->max_phi - sphere_angles->min_phi, GF_PI);
		min_subd2 = gf_divfix(sphere_angles->max_theta - sphere_angles->min_theta, GF_2PI);
		if (min_subd1<0) min_subd1 = -min_subd1;
		if (min_subd2<0) min_subd2 = -min_subd2;
		if (min_subd2<min_subd1) min_subd1=min_subd2;
		num_steps = FIX2INT(num_steps * min_subd1);
	}
	npts = num_steps * num_steps;

	coords = (SFVec3f*)gf_malloc(sizeof(SFVec3f)*npts);
	texcoords = (SFVec2f*)gf_malloc(sizeof(SFVec2f)*npts);
	compute_sphere(radius, coords, texcoords, num_steps, sphere_angles);

	for (i=0; i<num_steps-1; i++) {
		u32 n = i * num_steps;
		Fixed last_tx_coord;
		for (j=0; j<num_steps; j++) {
			mesh_set_vertex(mesh, coords[n + j + num_steps].x, coords[n + j + num_steps].y, coords[n + j + num_steps].z,
			                coords[n + j + num_steps].x, coords[n + j + num_steps].y, coords[n + j + num_steps].z,
			                texcoords[n + j + num_steps].x, texcoords[n + j + num_steps].y);

			mesh_set_vertex(mesh, coords[n + j].x, coords[n + j].y, coords[n + j].z,
			                coords[n + j].x, coords[n + j].y, coords[n + j].z,
			                texcoords[n + j].x, texcoords[n + j].y);

			if (j) {
				mesh_set_triangle(mesh, mesh->v_count-3, mesh->v_count-4, mesh->v_count-2);
				mesh_set_triangle(mesh, mesh->v_count-3, mesh->v_count-2, mesh->v_count-1);
			}
		
		}
		if (!sphere_angles) {
			last_tx_coord = (radius>0) ? 0 : FIX_ONE;
			mesh_set_vertex(mesh, coords[n + num_steps].x, coords[n + num_steps].y, coords[n + num_steps].z,
					coords[n + num_steps].x, coords[n + num_steps].y, coords[n  + num_steps].z,
					last_tx_coord, texcoords[n + num_steps].y);
			mesh_set_vertex(mesh, coords[n].x, coords[n].y, coords[n].z,
					coords[n].x, coords[n].y, coords[n].z,
					last_tx_coord, texcoords[n].y);

			mesh_set_triangle(mesh, mesh->v_count-3, mesh->v_count-4, mesh->v_count-2);
			mesh_set_triangle(mesh, mesh->v_count-3, mesh->v_count-2, mesh->v_count-1);
		}
	}

	gf_free(coords);
	gf_free(texcoords);
	if (!sphere_angles) {
		mesh->flags |= MESH_IS_SOLID;
	}
	if (radius<0) radius = -radius;

	mesh->bounds.min_edge.x = mesh->bounds.min_edge.y = mesh->bounds.min_edge.z = -radius;
	mesh->bounds.max_edge.x = mesh->bounds.max_edge.y = mesh->bounds.max_edge.z = radius;

	gf_bbox_refresh(&mesh->bounds);

	if (radius != FIX_ONE) gf_mesh_build_aabbtree(mesh);
}

void mesh_new_spherical_srd(GF_Mesh *mesh, Fixed radius, const GF_PropertyValue *srd_map, const GF_PropertyValue *srd_ref)
{
	u32 i, nb_items = srd_map->value.uint_list.nb_items / 8;
	u32 *vals = srd_map->value.uint_list.vals;
	u32 width, height;

	mesh_reset(mesh);
	if (!nb_items) return;
	compositor_get_srdmap_size(srd_map, &width, &height, NULL, NULL);

	u32 num_steps = 8;
	if (radius<0) radius = -radius;

	u32 nb_vx=0;
	for (i=0; i<nb_items; i++) {
		//get rect coords
		Fixed s_x = INT2FIX(vals[8*i]);
		Fixed s_y = INT2FIX(vals[8*i+1]);
		Fixed s_w = INT2FIX(vals[8*i+2]);
		Fixed s_h = INT2FIX(vals[8*i+3]);
		//normalize
		s_x = gf_divfix(s_x, width);
		s_y = gf_divfix(s_y, height);
		s_w = gf_divfix(s_w, width);
		s_h = gf_divfix(s_h, height);

		//get texture coords
		Fixed tx_tl = INT2FIX(vals[8*i+4]);
		Fixed ty_tl = INT2FIX(vals[8*i+5]);
		Fixed tx_br = INT2FIX(vals[8*i+4] + vals[8*i+6]);
		Fixed ty_br = INT2FIX(vals[8*i+5] + vals[8*i+7]);
		tx_tl /= srd_ref->value.vec2i.x;
		tx_br /= srd_ref->value.vec2i.x;
		ty_tl /= srd_ref->value.vec2i.y;
		ty_br /= srd_ref->value.vec2i.y;
		ty_tl = FIX_ONE - ty_tl;
		ty_br = FIX_ONE - ty_br;

		//build sphere x=0 is top , y=0 is left
		Fixed angle_top = GF_PI2 - GF_PI * s_y;
		Fixed angle_bottom = GF_PI2 - GF_PI * (s_y+s_h);

		Fixed angle_left = GF_2PI * s_x + GF_PI2;
		Fixed angle_right = GF_2PI * (s_x+s_w) + GF_PI2;

		u32 idx_h, idx_v;
		for (idx_v=0; idx_v<num_steps-1; idx_v++) {
			Fixed h1_angle, h2_angle;
			Fixed ty_top, ty_bottom;
			h1_angle = angle_top + idx_v * (angle_bottom-angle_top) / (num_steps-1);

			ty_top = ty_tl + idx_v * (ty_br-ty_tl) / (num_steps-1);
			if (idx_v+1<num_steps-1) {
				h2_angle = angle_top + (idx_v+1) * (angle_bottom-angle_top) / (num_steps-1);
				ty_bottom = ty_tl + (idx_v+1) * (ty_br-ty_tl) / (num_steps-1);
			}
			//avoid potential rounding for last step
			else {
				h2_angle = angle_bottom;
				ty_bottom = ty_br;
			}

			Fixed y_top = gf_sin(h1_angle);
			Fixed y_bottom = gf_sin(h2_angle);
			Fixed r_top = gf_sqrt(FIX_ONE - gf_mulfix(y_top, y_top));
			Fixed r_bottom = gf_sqrt(FIX_ONE - gf_mulfix(y_bottom, y_bottom));

			r_top = gf_mulfix(r_top, radius);
			r_bottom = gf_mulfix(r_bottom, radius);
			y_top = gf_mulfix(y_top, radius);
			y_bottom = gf_mulfix(y_bottom, radius);

			for (idx_h=0; idx_h<num_steps-1; idx_h++) {
				Fixed v1_angle, v2_angle;
				Fixed tx_left, tx_right;
				v1_angle = angle_left + idx_h * (angle_right-angle_left) / (num_steps-1);
				tx_left = tx_tl + idx_h * (tx_br-tx_tl) / (num_steps-1);
				if (idx_h+1<num_steps-1) {
					v2_angle = angle_left + (idx_h+1) * (angle_right-angle_left) / (num_steps-1);
					tx_right = tx_tl + (idx_h+1) * (tx_br-tx_tl) / (num_steps-1);
				}
				//avoid potential rounding for last step
				else {
					v2_angle = angle_right;
					tx_right = tx_br;
				}
				Fixed x_tl = gf_mulfix(gf_cos(v1_angle), r_top);
				Fixed z_tl = gf_mulfix(gf_sin(v1_angle), r_top);

				Fixed x_tr = gf_mulfix(gf_cos(v2_angle), r_top);
				Fixed z_tr = gf_mulfix(gf_sin(v2_angle), r_top);

				Fixed x_bl = gf_mulfix(gf_cos(v1_angle), r_bottom);
				Fixed z_bl = gf_mulfix(gf_sin(v1_angle), r_bottom);

				Fixed x_br = gf_mulfix(gf_cos(v2_angle), r_bottom);
				Fixed z_br = gf_mulfix(gf_sin(v2_angle), r_bottom);

				mesh_set_vertex(mesh, x_tl, y_top, z_tl, x_tl, y_top,  z_tl, tx_left, ty_top);
				mesh_set_vertex(mesh, x_bl, y_bottom, z_bl, x_bl, y_bottom, z_bl, tx_left, ty_bottom);
				mesh_set_vertex(mesh, x_br, y_bottom, z_br, x_br, y_bottom, z_br, tx_right, ty_bottom);
				mesh_set_vertex(mesh, x_tr, y_top, z_tr, x_tr, y_top, z_tr, tx_right, ty_top);

				mesh_set_triangle(mesh, nb_vx, nb_vx+1, nb_vx+2);
				mesh_set_triangle(mesh, nb_vx, nb_vx+2, nb_vx+3);
				nb_vx+=4;
			}
		}
	}
	mesh_update_bounds(mesh);
}
void mesh_new_rectangle_ex(GF_Mesh *mesh, SFVec2f size, SFVec2f *orig, u32 flip, u32 rotate)
{
	Fixed x = - size.x / 2;
	Fixed y = size.y / 2;
	u32 i;
	Bool flip_h = GF_FALSE;
	Bool flip_v = GF_FALSE;

	Fixed textureCoords[] = {
		0, 0,
		FIX_ONE, 0,
		FIX_ONE, FIX_ONE,
		0, FIX_ONE
	};

	if (orig) {
		x = orig->x;
		y = orig->y;
	}
	mesh_reset(mesh);

	switch (flip) {
	case 2:
		flip_v = GF_TRUE;
		break;
	case 1:
		flip_h = GF_TRUE;
		break;
	case 3:
		flip_v = GF_TRUE;
		flip_h = GF_TRUE;
		break;
	}

	if (flip_h) {
		Fixed v = textureCoords[0];
		textureCoords[0] = textureCoords[2] = textureCoords[4];
		textureCoords[4] = textureCoords[6] = v;
	}
	if (flip_v) {
		Fixed v = textureCoords[1];
		textureCoords[1] = textureCoords[3] = textureCoords[5];
		textureCoords[5] = textureCoords[7] = v;
	}

	for (i=0; i < rotate; i++)  {
		Fixed vx = textureCoords[4];
		Fixed vy = textureCoords[5];

		textureCoords[4] = textureCoords[2];
		textureCoords[5] = textureCoords[3];
		textureCoords[2] = textureCoords[0];
		textureCoords[3] = textureCoords[1];
		textureCoords[0] = textureCoords[6];
		textureCoords[1] = textureCoords[7];
		textureCoords[6] = vx;
		textureCoords[7] = vy;
	}


	mesh_set_vertex(mesh, x, y-size.y,  0,  0,  0,  FIX_ONE, textureCoords[0], textureCoords[1]);
	mesh_set_vertex(mesh, x+size.x, y-size.y,  0,  0,  0,  FIX_ONE, textureCoords[2], textureCoords[3]);
	mesh_set_vertex(mesh, x+size.x, y,  0,  0,  0,  FIX_ONE, textureCoords[4], textureCoords[5]);
	mesh_set_vertex(mesh, x,  y,  0,  0,  0,  FIX_ONE, textureCoords[6], textureCoords[7]);

	mesh_set_triangle(mesh, 0, 1, 2);
	mesh_set_triangle(mesh, 0, 2, 3);

	mesh->flags |= MESH_IS_2D;

	mesh->bounds.min_edge.x = x;
	mesh->bounds.min_edge.y = y-size.y;
	mesh->bounds.min_edge.z = 0;
	mesh->bounds.max_edge.x = x+size.x;
	mesh->bounds.max_edge.y = y;
	mesh->bounds.max_edge.z = 0;
	gf_bbox_refresh(&mesh->bounds);
}

void mesh_new_rectangle(GF_Mesh *mesh, SFVec2f size, SFVec2f *orig, Bool flip)
{
	mesh_new_rectangle_ex(mesh, size, orig, flip ? 2 : 0, 0);
}

void mesh_new_planar_srd(GF_Mesh *mesh, SFVec2f size, u32 flip, u32 rotate, const GF_PropertyValue *srd_map, const GF_PropertyValue *srd_ref)
{
	Bool flip_h = GF_FALSE;
	Bool flip_v = GF_FALSE;
	u32 i, nb_items = srd_map->value.uint_list.nb_items / 8;
	u32 *vals = srd_map->value.uint_list.vals;
	u32 width, height;
	compositor_get_srdmap_size(srd_map, &width, &height, NULL, NULL);

	Fixed ox = - size.x / 2;
	Fixed oy = size.y / 2;
	switch (flip) {
	case 2:
		flip_v = GF_TRUE;
		break;
	case 1:
		flip_h = GF_TRUE;
		break;
	case 3:
		flip_v = GF_TRUE;
		flip_h = GF_TRUE;
		break;
	}

	u32 nb_vx=0;
	mesh_reset(mesh);
	for (i=0; i<nb_items; i++) {
		Fixed x = INT2FIX(vals[8*i]);
		Fixed y = INT2FIX(vals[8*i+1]);
		Fixed w = INT2FIX(vals[8*i+2]);
		Fixed h = INT2FIX(vals[8*i+3]);

		x = gf_mulfix(x, size.x) / width;
		y = gf_mulfix(y, size.y) / height;
		w = gf_mulfix(w, size.x) / width;
		h = gf_mulfix(h, size.y) / height;

		Fixed tx_tl = INT2FIX(vals[8*i+4]);
		Fixed ty_tl = INT2FIX(vals[8*i+5]);
		Fixed tx_br = INT2FIX(vals[8*i+4] + vals[8*i+6]);
		Fixed ty_br = INT2FIX(vals[8*i+5] + vals[8*i+7]);
		tx_tl /= srd_ref->value.vec2i.x;
		tx_br /= srd_ref->value.vec2i.x;
		ty_tl /= srd_ref->value.vec2i.y;
		ty_br /= srd_ref->value.vec2i.y;
		ty_tl = FIX_ONE - ty_tl;
		ty_br = FIX_ONE - ty_br;

		if (flip_h) {
			Fixed v = tx_tl;
			tx_tl = tx_br;
			tx_br = v;
		}
		if (flip_v) {
			Fixed v = ty_tl;
			ty_tl = ty_br;
			ty_br = v;
		}

		mesh_set_vertex(mesh, ox + x, oy - y - h,  0,  0,  0,  FIX_ONE, tx_tl, ty_br);
		mesh_set_vertex(mesh, ox + x + w, oy - y - h,  0,  0,  0,  FIX_ONE, tx_br, ty_br);
		mesh_set_vertex(mesh, ox + x + w, oy - y,  0,  0,  0,  FIX_ONE, tx_br, ty_tl);
		mesh_set_vertex(mesh, ox + x, oy - y,  0,  0,  0,  FIX_ONE, tx_tl, ty_tl);

		mesh_set_triangle(mesh, nb_vx, nb_vx+1, nb_vx+2);
		mesh_set_triangle(mesh, nb_vx, nb_vx+2, nb_vx+3);
		nb_vx+=4;
	}

	mesh->flags |= MESH_IS_2D;
	mesh_update_bounds(mesh);
}


#define ELLIPSE_SUBDIV		32
void mesh_new_ellipse(GF_Mesh *mesh, Fixed a_dia, Fixed b_dia, Bool low_res)
{
	Fixed step, cur, end, cosa, sina;
	a_dia /= 2;
	b_dia /= 2;

	/*no begin/end draw since we always use generic 2D node drawing methods*/
	end = GF_2PI;
	step = end / ELLIPSE_SUBDIV;
	if (low_res) step *= HIGH_SPEED_RATIO;

	mesh_reset(mesh);

	/*center*/
	mesh_set_vertex(mesh, 0, 0, 0, 0, 0, FIX_ONE, FIX_ONE/2, FIX_ONE/2);
	for (cur=0; cur<end; cur += step) {
		cosa = gf_cos(cur);
		sina = gf_sin(cur);

		mesh_set_vertex(mesh, gf_mulfix(a_dia, cosa), gf_mulfix(b_dia, sina), 0,
		                0, 0, FIX_ONE,
		                (FIX_ONE + cosa)/2, (FIX_ONE + sina)/2);

		if (cur) mesh_set_triangle(mesh, 0, mesh->v_count-2, mesh->v_count-1);
	}
	mesh_set_vertex(mesh, a_dia, 0, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE/2);
	mesh_set_triangle(mesh, 0, mesh->v_count-2, mesh->v_count-1);

	mesh->flags |= MESH_IS_2D;
	mesh->bounds.min_edge.x = -a_dia;
	mesh->bounds.min_edge.y = -b_dia;
	mesh->bounds.min_edge.z = 0;
	mesh->bounds.max_edge.x = a_dia;
	mesh->bounds.max_edge.y = b_dia;
	mesh->bounds.max_edge.z = 0;
	gf_bbox_refresh(&mesh->bounds);
}

void mesh_from_path_intern(GF_Mesh *mesh, GF_Path *path, Bool make_ccw)
{
	u32 i, nbPts;
	Fixed w, h;
	GF_Rect bounds;
	Bool isCW = 0;

	gf_path_flatten(path);
	gf_path_get_bounds(path, &bounds);

	mesh_reset(mesh);
	if (path->n_contours==1) {
		u32 type = gf_polygone2d_get_convexity(path->points, path->n_points);
		switch (type) {
		/*degenrated polygon - skip*/
		case GF_POLYGON_CONVEX_LINE:
			return;
		case GF_POLYGON_CONVEX_CW:
			isCW = make_ccw;
		case GF_POLYGON_CONVEX_CCW:
			w = bounds.width;
			h = bounds.height;

			/*add all vertices*/
			for (i=0; i<path->n_points-1; i++) {
				GF_Point2D pt = path->points[i];
				mesh_set_vertex(mesh, pt.x, pt.y, 0, 0, 0, FIX_ONE, gf_divfix(pt.x - bounds.x, w), gf_divfix(bounds.y - pt.y, h));
			}
			nbPts = path->n_points - 1;
			/*take care of already closed path*/
			if ( (path->points[i].x != path->points[0].x) || (path->points[i].y != path->points[0].y)) {
				GF_Point2D pt = path->points[i];
				mesh_set_vertex(mesh, pt.x, pt.y, 0, 0, 0, FIX_ONE, gf_divfix(pt.x - bounds.x, w), gf_divfix(bounds.y - pt.y, h));
				nbPts = path->n_points;
			}
			/*make it CCW*/
			for (i=1; i<nbPts-1; i++) {
				if (isCW) {
					mesh_set_triangle(mesh, 0, nbPts-i, nbPts-i-1);
				} else {
					mesh_set_triangle(mesh, 0, i, i+1);
				}
			}
			mesh->bounds.min_edge.x = bounds.x;
			mesh->bounds.min_edge.y = bounds.y-bounds.height;
			mesh->bounds.min_edge.z = 0;
			mesh->bounds.max_edge.x = bounds.x+bounds.width;
			mesh->bounds.max_edge.y = bounds.y;
			mesh->bounds.max_edge.z = 0;
			gf_bbox_refresh(&mesh->bounds);
			return;
		default:
			break;
		}
	}
	/*we need to tesselate the path*/
#ifdef GPAC_HAS_GLU
	gf_mesh_tesselate_path(mesh, path, 0);
#endif
}

void mesh_from_path(GF_Mesh *mesh, GF_Path *path)
{
	mesh_from_path_intern(mesh, path, 1);
}


void mesh_get_outline(GF_Mesh *mesh, GF_Path *path)
{
	u32 i, j, cur, nb_pts;
	mesh_reset(mesh);

	mesh->mesh_type = MESH_LINESET;
	mesh->flags |= (MESH_IS_2D | MESH_NO_TEXTURE);

	gf_path_flatten(path);

	cur = 0;
	for (i=0; i<path->n_contours; i++) {
		nb_pts = 1+path->contours[i] - cur;
		for (j=0; j<nb_pts; j++) {
			GF_Point2D pt = path->points[j+cur];
			if (j) mesh_set_line(mesh, mesh->v_count-1, mesh->v_count);
			mesh_set_vertex(mesh, pt.x, pt.y, 0, 0, 0, FIX_ONE, 0, 0);
		}
		cur += nb_pts;
	}
	mesh_update_bounds(mesh);
}


#ifndef GPAC_DISABLE_VRML


#define COL_TO_RGBA(res, col) { res.red = col.red; res.green = col.green; res.blue = col.blue; res.alpha = FIX_ONE; }


#define MESH_GET_COL(thecol, index)	{\
	if (colorRGB && ((u32) index < colorRGB->color.count) ) COL_TO_RGBA(thecol, colorRGB->color.vals[index])	\
	else if (colorRGBA && (u32) index < colorRGBA->color.count) thecol = colorRGBA->color.vals[index]; \
	} \
 
void mesh_new_ils(GF_Mesh *mesh, GF_Node *__coord, MFInt32 *coordIndex, GF_Node *__color, MFInt32 *colorIndex, Bool colorPerVertex, Bool do_close)
{
	u32 i, n, count, c_count, col_count;
	u32 index;
	u32 first_idx, last_idx;
	Bool move_to;
	SFVec3f pt;
	SFColorRGBA colRGBA;
	Bool has_color, has_coord;
	M_Coordinate2D *coord2D = (M_Coordinate2D*) __coord;
	M_Coordinate *coord = (M_Coordinate*) __coord;
	M_Color *colorRGB = (M_Color *) __color;
#ifndef GPAC_DISABLE_X3D
	X_ColorRGBA *colorRGBA = (X_ColorRGBA *) __color;
#endif

	if (__coord && (gf_node_get_tag(__coord) == TAG_MPEG4_Coordinate2D)) {
		coord = NULL;
	} else {
		coord2D = NULL;
	}

	colRGBA.red = colRGBA.green = colRGBA.blue = colRGBA.alpha = 0;

	if (!coord2D && !coord) return;
	c_count = coord2D ? coord2D->point.count : coord->point.count;
	if (!c_count) return;

	count = coordIndex->count;
	has_coord = count ? 1 : 0;
	if (!has_coord) count = c_count;

	if (!colorIndex->vals) colorIndex = coordIndex;
	col_count = colorIndex->count ? colorIndex->count : c_count;
	/*not enough color indices, use coord ones*/
	if (colorPerVertex && (col_count<count) ) {
		colorIndex = coordIndex;
		col_count = count;
	}
	has_color = 0;
	if (__color) {
#ifndef GPAC_DISABLE_X3D
		if (gf_node_get_tag(__color)==TAG_X3D_ColorRGBA) {
			colorRGB = NULL;
			has_color = (colorRGBA->color.count) ? 1 : 0;
		} else
#endif
		{
#ifndef GPAC_DISABLE_X3D
			colorRGBA = NULL;
#endif
			has_color = (colorRGB->color.count) ? 1 : 0;
		}
	}

	mesh_reset(mesh);
	mesh->mesh_type = MESH_LINESET;
	if (has_color) mesh->flags |= MESH_HAS_COLOR;

	n = 0;
	if (has_color && !colorPerVertex) {
		index = colorIndex->count ? colorIndex->vals[0] : 0;
#ifndef GPAC_DISABLE_X3D
		if ((u32) index < col_count) MESH_GET_COL(colRGBA, index);
#endif
	}
	move_to = 1;

	first_idx = last_idx = 0;
	for (i=0; i<count; i++) {
		if (has_coord && coordIndex->vals[i] == -1) {
			/*close color per vertex*/
			if (!move_to && do_close && !gf_vec_equal(mesh->vertices[first_idx].pos, mesh->vertices[last_idx].pos) ) {
				mesh_set_line(mesh, last_idx, first_idx);
			}
			move_to = 1;
			n++;
			if (has_color && !colorPerVertex) {
				if (n<colorIndex->count) index = colorIndex->vals[n];
				else if (n<col_count) index = n;
				else index = 0;
#ifndef GPAC_DISABLE_X3D
				MESH_GET_COL(colRGBA, index);
#endif
			}
		} else {
			if (has_color && colorPerVertex) {
				if (i<colorIndex->count) index = colorIndex->vals[i];
				else if (i<col_count) index = i;
				else index=0;
#ifndef GPAC_DISABLE_X3D
				MESH_GET_COL(colRGBA, index);
#endif
			}
			if (has_coord) index = coordIndex->vals[i];
			else index = i;
			if (index < c_count) {
				if (coord2D) {
					pt.x = coord2D->point.vals[index].x;
					pt.y = coord2D->point.vals[index].y;
					pt.z = 0;
				} else {
					pt = coord->point.vals[index];
				}
				mesh_set_point(mesh, pt.x, pt.y, pt.z, colRGBA);
				last_idx = mesh->v_count - 1;
				if (move_to) {
					first_idx = last_idx;
					move_to = 0;
				} else {
					mesh_set_line(mesh, last_idx-1, last_idx);
				}
			}
		}
	}
	/*close color per vertex*/
	if (do_close && !gf_vec_equal(mesh->vertices[first_idx].pos, mesh->vertices[last_idx].pos) ) {
		mesh_set_line(mesh, last_idx, first_idx);
	}
	if (coord2D) mesh->flags |= MESH_IS_2D;
#ifndef GPAC_DISABLE_X3D
	if (colorRGBA) mesh->flags |= MESH_HAS_ALPHA;
#endif
	mesh_update_bounds(mesh);
}

void mesh_new_ps(GF_Mesh *mesh, GF_Node *__coord, GF_Node *__color)
{
	u32 c_count, i;
	Bool has_color;
	SFVec3f pt;
	SFColorRGBA colRGBA;
	M_Coordinate2D *coord2D = (M_Coordinate2D*) __coord;
	M_Coordinate *coord = (M_Coordinate*) __coord;
	M_Color *colorRGB = (M_Color *) __color;
#ifndef GPAC_DISABLE_X3D
	X_ColorRGBA *colorRGBA = (X_ColorRGBA *) __color;
#endif

	if (__coord && (gf_node_get_tag(__coord) == TAG_MPEG4_Coordinate2D)) {
		coord = NULL;
	} else {
		coord2D = NULL;
	}

	if (!coord2D && !coord) return;
	c_count = coord2D ? coord2D->point.count : coord->point.count;
	if (!c_count) return;

	mesh_reset(mesh);
	mesh->mesh_type = MESH_POINTSET;

	has_color = 0;
	if (__color) {
#ifndef GPAC_DISABLE_X3D
		if (gf_node_get_tag(__color)==TAG_X3D_ColorRGBA) {
			colorRGB = NULL;
			has_color = (colorRGBA->color.count) ? 1 : 0;
		} else
#endif
		{
#ifndef GPAC_DISABLE_X3D
			colorRGBA = NULL;
#endif
			has_color = (colorRGB->color.count) ? 1 : 0;
		}
	}
	if (has_color) mesh->flags |= MESH_HAS_COLOR;

	colRGBA.red = colRGBA.green = colRGBA.blue = colRGBA.alpha = FIX_ONE;

	for (i=0; i<c_count; ++i) {
#ifndef GPAC_DISABLE_X3D
		if (has_color) MESH_GET_COL(colRGBA, i);
#endif
		if (coord2D) {
			pt.x = coord2D->point.vals[i].x;
			pt.y = coord2D->point.vals[i].y;
			pt.z = 0;
		} else {
			pt = coord->point.vals[i];
		}
		mesh_set_point(mesh, pt.x, pt.y, pt.z, colRGBA);
		mesh_set_index(mesh, mesh->v_count-1);
	}
	mesh_update_bounds(mesh);
}

/*structures used for normal smoothing*/
struct face_info
{
	/*face normal*/
	SFVec3f nor;
	u32 idx_alloc;
	/*nb pts in face*/
	u32 idx_count;
	/*indexes of face vertices in the pt_info structure*/
	u32 *idx;
};
/*for each pt in the mesh*/
struct pt_info
{
	u32 face_alloc;
	/*number of faces this point belongs to*/
	u32 face_count;
	/*idx of face in face_info structure*/
	u32 *faces;
};

void register_point_in_face(struct face_info *fi, u32 pt_index)
{
	if (fi->idx_count==fi->idx_alloc) {
		fi->idx_alloc += 10;
		fi->idx = (u32*)gf_realloc(fi->idx, sizeof(u32)*fi->idx_alloc);
	}
	fi->idx[fi->idx_count] = pt_index;
	fi->idx_count++;
}

void register_face_in_point(struct pt_info *pi, u32 face_index)
{
	if (pi->face_count==pi->face_alloc) {
		pi->face_alloc += 10;
		pi->faces = (u32*)gf_realloc(pi->faces, sizeof(u32)*pi->face_alloc);
	}
	pi->faces[pi->face_count] = face_index;
	pi->face_count++;
}

static GFINLINE SFVec3f smooth_face_normals(struct pt_info *pts, u32 nb_pts, struct face_info *faces, u32 nb_faces,
        u32 pt_idx_in_face, u32 face_idx, Fixed creaseAngleCos)
{
	u32 i=0;
	SFVec3f nor;
	struct face_info *fi = &faces[face_idx];
	struct pt_info *pi = &pts[fi->idx[pt_idx_in_face]];

	nor = fi->nor;
	/*for each face adjacent this point/face*/
	for (i=0; i<pi->face_count; i++) {
		/*current face, skip*/
		if (pi->faces[i]==face_idx) continue;
		if (gf_vec_dot(fi->nor, faces[pi->faces[i]].nor) > creaseAngleCos) {
			gf_vec_add(nor, nor, faces[pi->faces[i]].nor);
		}

	}
	gf_vec_norm(&nor);
	return nor;
}


#define GET_IDX(idx, array) \
		if (idx<array->count && (array->vals[idx]>=0) ) index = array->vals[idx];	\
		else if (idx<c_count) index = idx;	\
		else index = 0;	\
 
void mesh_new_ifs_intern(GF_Mesh *mesh, GF_Node *__coord, MFInt32 *coordIndex,
                         GF_Node *__color, MFInt32 *colorIndex, Bool colorPerVertex,
                         GF_Node *__normal, MFInt32 *normalIndex, Bool normalPerVertex,
                         GF_Node *__texCoords, MFInt32 *texCoordIndex,
                         Fixed creaseAngle)
{
	u32 i, idx, count, c_count, nor_count;
	u32 index;
	Bool smooth_normals;
	SFVec2f tx;
	u32 s_axis, t_axis;
	SFVec3f pt, nor, bounds, center;
	SFColorRGBA colRGBA;
	Bool has_color, has_coord, has_normal, has_tex, gen_tex_coords;
	GF_Mesh **faces;
	struct face_info *faces_info;
	struct pt_info *pts_info;
	u32 face_count, cur_face;
	M_Coordinate2D *coord2D = (M_Coordinate2D*) __coord;
	M_Coordinate *coord = (M_Coordinate*) __coord;
	M_Color *colorRGB = (M_Color *) __color;
#ifndef GPAC_DISABLE_X3D
	X_ColorRGBA *colorRGBA = (X_ColorRGBA *) __color;
#endif
	M_Normal *normal = (M_Normal*) __normal;
	M_TextureCoordinate *txcoord = (M_TextureCoordinate*) __texCoords;

	nor.x = nor.y = nor.z = 0;
	center = pt = bounds = nor;
	tx.x = tx.y = 0;

	if (__coord && (gf_node_get_tag(__coord) == TAG_MPEG4_Coordinate2D)) {
		coord = NULL;
	} else {
		coord2D = NULL;
		if (!__coord)
			return;
		/*not supported yet*/
#ifndef GPAC_DISABLE_X3D
		if (gf_node_get_tag(__coord) == TAG_X3D_CoordinateDouble)
			return;
#endif
	}
	gen_tex_coords = 0;
#ifndef GPAC_DISABLE_X3D
	if (__texCoords && (gf_node_get_tag(__texCoords)==TAG_X3D_TextureCoordinateGenerator)) {
		gen_tex_coords = 1;
		txcoord = NULL;
	}
#endif

	if (!coord2D && !coord) return;
	c_count = coord2D ? coord2D->point.count : coord->point.count;
	if (!c_count) return;

	if (normal && normalIndex) {
		if (!normalIndex->vals) normalIndex = coordIndex;
		nor_count = normalIndex->count ? normalIndex->count : c_count;
		has_normal = normal->vector.count ? 1 : 0;
	} else {
		nor_count = 0;
		nor.x = nor.y = 0;
		nor.z = FIX_ONE;
		has_normal = 0;
	}

	has_tex = txcoord ? 1 : 0;
	if (has_tex && !texCoordIndex->vals) texCoordIndex = coordIndex;

	mesh_reset(mesh);
	memset(&colRGBA, 0, sizeof(SFColorRGBA));
	/*compute bounds - note we assume all points in the IFS coordinate are used*/
	s_axis = t_axis = 0;
	if (!has_tex) {
		for (i=0; i<c_count; i++) {
			if (coord2D) mesh_set_point(mesh, coord2D->point.vals[i].x, coord2D->point.vals[i].y, 0, colRGBA);
			else mesh_set_point(mesh, coord->point.vals[i].x, coord->point.vals[i].y, coord->point.vals[i].z, colRGBA);
		}
		mesh_update_bounds(mesh);
		gf_vec_diff(bounds, mesh->bounds.max_edge, mesh->bounds.min_edge);
		center = mesh->bounds.min_edge;
		if ( (bounds.x >= bounds.y) && (bounds.x >= bounds.z) ) {
			s_axis = 0;
			t_axis = (bounds.y >= bounds.z) ? 1 : 2;
		}
		else if ( (bounds.y >= bounds.x) && (bounds.y >= bounds.z) ) {
			s_axis = 1;
			t_axis = (bounds.x >= bounds.z) ? 0 : 2;
		}
		else {
			s_axis = 2;
			t_axis = (bounds.x >= bounds.y) ? 0 : 1;
		}
	}

	has_color = 0;
	if (__color) {
#ifndef GPAC_DISABLE_X3D
		if (gf_node_get_tag(__color)==TAG_X3D_ColorRGBA) {
			colorRGB = NULL;
			has_color = (colorRGBA->color.count) ? 1 : 0;
		} else
#endif
		{
#ifndef GPAC_DISABLE_X3D
			colorRGBA = NULL;
#endif
			has_color = (colorRGB->color.count) ? 1 : 0;
		}
	}
	idx = 0;
	if (has_color) {
		if (!colorPerVertex) {
			index = colorIndex->count ? colorIndex->vals[0] : 0;
#ifndef GPAC_DISABLE_X3D
			MESH_GET_COL(colRGBA, index);
#endif
		} else {
			if (!colorIndex->vals) colorIndex = coordIndex;
		}
	}

	if (has_normal && !normalPerVertex) {
		index = normalIndex->count ? normalIndex->vals[0] : 0;
		if (index < nor_count) nor = normal->vector.vals[index];
	}

	count = coordIndex->count;
	has_coord = count ? 1 : 0;
	if (!has_coord) count = c_count;

	smooth_normals = (!has_normal && coord && (creaseAngle > FIX_EPSILON)) ? 1 : 0;

	/*build face list*/
	if (!has_coord) {
		face_count = 1;
	} else {
		face_count = 0;
		for (i=0; i<count; i++) {
			if (coordIndex->vals[i] == -1) face_count++;
		}
		/*don't forget last face*/
		if (coordIndex->vals[count-1] != -1) face_count++;
	}

	faces = (GF_Mesh**)gf_malloc(sizeof(GF_Mesh *)*face_count);
	for (i=0; i<face_count; i++) {
		faces[i] = new_mesh();
		if (coord2D) faces[i]->flags = MESH_IS_2D;
	}
	faces_info = NULL;
	pts_info = NULL;

	/*alloc face & normals tables*/
	if (smooth_normals) {
		faces_info = (struct face_info*)gf_malloc(sizeof(struct face_info)*face_count);
		memset(faces_info, 0, sizeof(struct face_info)*face_count);
		pts_info = (struct pt_info*)gf_malloc(sizeof(struct pt_info)*c_count);
		memset(pts_info, 0, sizeof(struct pt_info)*c_count);
	}

	cur_face = 0;
	for (i=0; i<count; i++) {
		if (has_coord && coordIndex->vals[i] == -1) {
			idx++;
			if (has_color && !colorPerVertex) {
				GET_IDX(idx, colorIndex);
#ifndef GPAC_DISABLE_X3D
				MESH_GET_COL(colRGBA, index);
#endif
			}
			if (has_normal && !normalPerVertex) {
				GET_IDX(idx, normalIndex);
				if (index < nor_count) nor = normal->vector.vals[index];
			}
			if (faces[cur_face]->v_count<3) faces[cur_face]->v_count=0;
			/*compute face normal - watchout for colinear vectors*/
			else if (smooth_normals) {
				SFVec3f v1, v2, fn;
				u32 k=2;
				gf_vec_diff(v1, faces[cur_face]->vertices[1].pos, faces[cur_face]->vertices[0].pos);
				while (k<faces[cur_face]->v_count) {
					gf_vec_diff(v2, faces[cur_face]->vertices[k].pos, faces[cur_face]->vertices[0].pos);
					fn = gf_vec_cross(v1, v2);
					if (gf_vec_len(fn)) {
						gf_vec_norm(&fn);
						faces_info[cur_face].nor = fn;
						break;
					}
					k++;
				}
			}
			cur_face++;
		} else {
			if (has_color && colorPerVertex) {
				GET_IDX(i, colorIndex);
#ifndef GPAC_DISABLE_X3D
				MESH_GET_COL(colRGBA, index);
#endif
			}
			if (has_normal && normalPerVertex) {
				GET_IDX(i, normalIndex);
				if (index < normal->vector.count) {
					nor = normal->vector.vals[index];
				}
			}

			if (has_coord) index = coordIndex->vals[i];
			else index = i;
			if (index < c_count) {
				if (coord2D) {
					pt.x = coord2D->point.vals[index].x;
					pt.y = coord2D->point.vals[index].y;
					pt.z = 0;
				} else {
					pt = coord->point.vals[index];
				}
				/*update face to point and point to face structures*/
				if (smooth_normals) {
					register_point_in_face(&faces_info[cur_face], index);
					register_face_in_point(&pts_info[index], cur_face);
				}
			}

			if (has_tex) {
				GET_IDX(i, texCoordIndex);
				if (index < txcoord->point.count) tx = txcoord->point.vals[index];
			} else {
				SFVec3f v;
				gf_vec_diff(v, pt, center);
				tx.x = tx.y = 0;
				if (s_axis==0) tx.x = gf_divfix(v.x, bounds.x);
				else if (s_axis==1) tx.x = gf_divfix(v.y, bounds.y);
				else if (s_axis==2) tx.x = gf_divfix(v.z, bounds.z);
				if (t_axis==0) tx.y = gf_divfix(v.x, bounds.x);
				else if (t_axis==1) tx.y = gf_divfix(v.y, bounds.y);
				else if (t_axis==2) tx.y = gf_divfix(v.z, bounds.z);
			}

			mesh_set_vertex_v(faces[cur_face], pt, nor, tx, colRGBA);
		}
	}

	/*generate normals*/
	if (!has_normal && coord) {
		u32 j;
		if (smooth_normals) {
			Fixed cosCrease;
			/*we only support 0->PI, whatever exceeds is smoothest*/
			if (creaseAngle>GF_PI) cosCrease = -FIX_ONE;
			else cosCrease = gf_cos(creaseAngle);

			for (i=0; i<face_count; i++) {
				for (j=0; j<faces[i]->v_count; j++) {
					SFVec3f n = smooth_face_normals(pts_info, c_count, faces_info, face_count, j, i, cosCrease);
					MESH_SET_NORMAL(faces[i]->vertices[j], n);
				}
			}

			if (faces_info) {
				for (i=0; i<face_count; i++) if (faces_info[i].idx) gf_free(faces_info[i].idx);
				gf_free(faces_info);
			}
			if (pts_info) {
				for (i=0; i<c_count; i++) if (pts_info[i].faces) gf_free(pts_info[i].faces);
				gf_free(pts_info);
			}
			mesh->flags |= MESH_IS_SMOOTHED;
		} else {
			for (i=0; i<face_count; i++) {
				SFVec3f v1, v2, n;
				if (! faces[i] || ! faces[i]->vertices) continue;
				gf_vec_diff(v1, faces[i]->vertices[1].pos, faces[i]->vertices[0].pos);
				gf_vec_diff(v2, faces[i]->vertices[2].pos, faces[i]->vertices[0].pos);
				n = gf_vec_cross(v1, v2);
				if (!n.x && !n.y && !n.z) n.z = FIX_ONE;
				else gf_vec_norm(&n);
				for (j=0; j<faces[i]->v_count; j++)
					MESH_SET_NORMAL(faces[i]->vertices[j], n);
			}
		}
	}

	mesh_reset(mesh);
	mesh->mesh_type = MESH_TRIANGLES;
	if (has_color) mesh->flags |= MESH_HAS_COLOR;

	for (i=0; i<face_count; i++) {
		if (! faces[i]) continue;
		if (faces[i]->v_count) TesselateFaceMesh(mesh, faces[i]);
		mesh_free(faces[i]);
	}
	gf_free(faces);
	mesh_update_bounds(mesh);

	if (!coord2D) gf_mesh_build_aabbtree(mesh);

#ifndef GPAC_DISABLE_X3D
	if (colorRGBA) mesh->flags |= MESH_HAS_ALPHA;
	if (gen_tex_coords) mesh_generate_tex_coords(mesh, __texCoords);
#endif
}

void mesh_new_ifs2d(GF_Mesh *mesh, GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	mesh_new_ifs_intern(mesh, ifs2D->coord, &ifs2D->coordIndex,
	                    ifs2D->color, &ifs2D->colorIndex, ifs2D->colorPerVertex,
	                    NULL, NULL, 0, ifs2D->texCoord, &ifs2D->texCoordIndex, 0);

	mesh->flags |= MESH_IS_2D;
}

void mesh_new_ifs(GF_Mesh *mesh, GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	mesh_new_ifs_intern(mesh, ifs->coord, &ifs->coordIndex, ifs->color, &ifs->colorIndex, ifs->colorPerVertex,
	                    ifs->normal, &ifs->normalIndex, ifs->normalPerVertex, ifs->texCoord, &ifs->texCoordIndex, ifs->creaseAngle);

	if (ifs->solid) mesh->flags |= MESH_IS_SOLID;
	if (!ifs->ccw) mesh->flags |= MESH_IS_CW;
}

void mesh_new_elevation_grid(GF_Mesh *mesh, GF_Node *node)
{
	u32 i, j, face_count, pt_count, zDimension, xDimension, cur_face, idx, pt_idx;
	Bool has_normal, has_txcoord, has_color, smooth_normals;
	GF_Mesh **faces;
	GF_Vertex vx;
	SFVec3f v1, v2, n;
	struct face_info *faces_info;
	struct pt_info *pts_info;
	M_ElevationGrid *eg = (M_ElevationGrid *) node;
	M_Normal *norm = (M_Normal *)eg->normal;
	M_Color *colorRGB = (M_Color *)eg->color;
#ifndef GPAC_DISABLE_X3D
	X_ColorRGBA *colorRGBA = (X_ColorRGBA *)eg->color;
#endif
	SFColorRGBA rgba;
	M_TextureCoordinate *txc = (M_TextureCoordinate *)eg->texCoord;

	mesh_reset(mesh);
	if (!eg->height.count || (eg->xDimension<2) || (eg->zDimension<2)) return;

	memset(&vx, 0, sizeof(GF_Vertex));
	memset(&rgba, 0, sizeof(SFColorRGBA));
	has_txcoord = txc ? txc->point.count : 0;
	has_normal = norm ? norm->vector.count : 0;
	has_color = 0;
	if (eg->color) {
#ifndef GPAC_DISABLE_X3D
		if (gf_node_get_tag(eg->color)==TAG_X3D_ColorRGBA) {
			colorRGB = NULL;
			has_color = colorRGBA->color.count ? 1 : 0;
		} else
#endif
		{
#ifndef GPAC_DISABLE_X3D
			colorRGBA = NULL;
#endif
			has_color = colorRGB->color.count ? 1 : 0;
		}
	}
	face_count = (eg->xDimension-1) * (eg->zDimension-1);
	pt_count = eg->xDimension * eg->zDimension;
	if (pt_count>eg->height.count) return;

	smooth_normals = (!has_normal && (eg->creaseAngle > FIX_EPSILON)) ? 1 : 0;

	faces = NULL;
	faces_info = NULL;
	pts_info = NULL;

	zDimension = (u32) eg->zDimension;
	xDimension = (u32) eg->xDimension;
	cur_face = 0;
	pt_idx = 0;

	/*basic case: nothing specified but the points*/
	if (!smooth_normals && !has_color && !has_normal && !has_txcoord) {
		for (j=0; j<zDimension; j++) {
			for (i=0; i<xDimension; i++) {
				vx.pos.z = eg->zSpacing * j;
				vx.pos.y = eg->height.vals[i +j*xDimension];
				vx.pos.x = eg->xSpacing * i;
				vx.texcoords.x = INT2FIX(i) / (xDimension - 1);
				vx.texcoords.y = INT2FIX(j) / (zDimension - 1);

				mesh_set_vertex_vx(mesh, &vx);
			}
		}
		for (j=0; j<zDimension-1; j++) {
			u32 z0 = (j)*xDimension;
			u32 z1 = (j+1)*xDimension;
			for (i=0; i<xDimension-1; i++) {
				mesh_set_triangle(mesh, i+z0, i+z1, i+z1+1);
				mesh_set_triangle(mesh, i+z0, i+z1+1, i+z0+1);

				gf_vec_diff(v1, mesh->vertices[i+z0].pos, mesh->vertices[i+z0+1].pos);
				gf_vec_diff(v2, mesh->vertices[i+z1+1].pos, mesh->vertices[i+z0+1].pos);
				n = gf_vec_cross(v1, v2);
				gf_vec_norm(&n);
				MESH_SET_NORMAL(mesh->vertices[i+z0], n);
				MESH_SET_NORMAL(mesh->vertices[i+z0+1], n);
				MESH_SET_NORMAL(mesh->vertices[i+z1], n);
				MESH_SET_NORMAL(mesh->vertices[i+z1+1], n);
			}
		}
		mesh->mesh_type = MESH_TRIANGLES;
		mesh_update_bounds(mesh);
		if (!eg->ccw) mesh->flags |= MESH_IS_CW;
		if (eg->solid) mesh->flags |= MESH_IS_SOLID;
		gf_mesh_build_aabbtree(mesh);
		return;
	}

	/*alloc face & normals tables*/
	if (smooth_normals) {
		faces = (GF_Mesh **)gf_malloc(sizeof(GF_Mesh *)*face_count);
		if (!faces) return;
		faces_info = (struct face_info*)gf_malloc(sizeof(struct face_info)*face_count);
		if (!faces_info) return;
		memset(faces_info, 0, sizeof(struct face_info)*face_count);
		pts_info = (struct pt_info*)gf_malloc(sizeof(struct pt_info)*pt_count);
		if (!pts_info) return;
		memset(pts_info, 0, sizeof(struct pt_info)*pt_count);
		faces[cur_face] = new_mesh();
		if (!faces[cur_face]) return;
	}

	for (j=0; j<zDimension-1; j++) {
		for (i = 0; i < xDimension-1; i++) {
			u32 k, l;
			/*get face color*/
			if (has_color && !eg->colorPerVertex) {
				idx = i + j * (xDimension-1);
#ifndef GPAC_DISABLE_X3D
				MESH_GET_COL(rgba, idx);
#endif
				vx.color = MESH_MAKE_COL(rgba);
			}
			/*get face normal*/
			if (has_normal && !eg->normalPerVertex) {
				idx = i + j * (xDimension-1);
				if (idx<norm->vector.count) {
					n = norm->vector.vals[idx];
					gf_vec_norm(&n);
					MESH_SET_NORMAL(vx, n);
				}
			}

			for (k=0; k<2; k++) {
				vx.pos.z = eg->zSpacing * (j+k);
				for (l=0; l<2; l++) {

					vx.pos.y = eg->height.vals[(i+l) +(j+k)*xDimension];
					vx.pos.x = eg->xSpacing * (i+l);

					/*get color per vertex*/
					if (has_color && eg->colorPerVertex) {
						idx = i+l + (j+k) * xDimension;
#ifndef GPAC_DISABLE_X3D
						MESH_GET_COL(rgba, idx);
#endif
						vx.color = MESH_MAKE_COL(rgba);
					}
					/*get tex coord*/
					if (!has_txcoord) {
						vx.texcoords.x = INT2FIX(i+l) / (xDimension - 1);
						vx.texcoords.y = INT2FIX(j+k) / (zDimension - 1);
					} else {
						idx = (i+l) +(j+k)*xDimension;
						if (idx<txc->point.count) vx.texcoords = txc->point.vals[idx];
					}
					/*get normal per vertex*/
					if (has_normal && eg->normalPerVertex) {
						idx = (i+l) + (j+k) * xDimension;
						if (idx<norm->vector.count) {
							n = norm->vector.vals[idx];
							gf_vec_norm(&n);
							MESH_SET_NORMAL(vx, n);
						}
					}
					/*update face to point and point to face structures*/
					if (smooth_normals) {
						mesh_set_vertex_vx(faces[cur_face], &vx);
						register_point_in_face(&faces_info[cur_face], (i+l) + (j+k)*xDimension);
						register_face_in_point(&pts_info[(i+l) + (j+k)*xDimension], cur_face);
					} else {
						mesh_set_vertex_vx(mesh, &vx);
					}
				}

			}

			/*compute face normal*/
			if (smooth_normals) {
				mesh_set_triangle(faces[cur_face], 0, 2, 3);
				mesh_set_triangle(faces[cur_face], 0, 3, 1);
				gf_vec_diff(v1, faces[cur_face]->vertices[0].pos, faces[cur_face]->vertices[1].pos);
				gf_vec_diff(v2, faces[cur_face]->vertices[3].pos, faces[cur_face]->vertices[1].pos);
				faces_info[cur_face].nor = gf_vec_cross(v1, v2);
				gf_vec_norm(&faces_info[cur_face].nor);
				/*done with face*/
				cur_face++;
				if (cur_face<face_count) {
					faces[cur_face] = new_mesh();
					if (!faces[cur_face]) return;
				}
			} else {
				mesh_set_triangle(mesh, pt_idx+0, pt_idx+2, pt_idx+3);
				mesh_set_triangle(mesh, pt_idx+0, pt_idx+3, pt_idx+1);

				if (!has_normal) {
					gf_vec_diff(v1, mesh->vertices[pt_idx+0].pos, mesh->vertices[pt_idx+1].pos);
					gf_vec_diff(v2, mesh->vertices[pt_idx+3].pos, mesh->vertices[pt_idx+1].pos);
					n = gf_vec_cross(v1, v2);
					gf_vec_norm(&n);
					MESH_SET_NORMAL(mesh->vertices[pt_idx+0], n);
					MESH_SET_NORMAL(mesh->vertices[pt_idx+1], n);
					MESH_SET_NORMAL(mesh->vertices[pt_idx+2], n);
					MESH_SET_NORMAL(mesh->vertices[pt_idx+3], n);
				}
				pt_idx+=4;
			}
		}
	}

	/*generate normals*/
	if (smooth_normals) {
		Fixed cosCrease;
		/*we only support 0->PI, whatever exceeds is smoothest*/
		if (eg->creaseAngle>GF_PI) cosCrease = -FIX_ONE;
		else cosCrease = gf_cos(eg->creaseAngle);

		for (i=0; i<face_count; i++) {
			for (j=0; j<faces[i]->v_count; j++) {
				n = smooth_face_normals(pts_info, pt_count, faces_info, face_count, j, i, cosCrease);
				MESH_SET_NORMAL(faces[i]->vertices[j], n);
			}
		}

		if (faces_info) {
			for (i=0; i<face_count; i++) if (faces_info[i].idx) gf_free(faces_info[i].idx);
			gf_free(faces_info);
		}
		if (pts_info) {
			for (i=0; i<pt_count; i++) if (pts_info[i].faces) gf_free(pts_info[i].faces);
			gf_free(pts_info);
		}
		mesh->flags |= MESH_IS_SMOOTHED;


		for (i=0; i<face_count; i++) {
			if (faces[i]->v_count) {
				u32 init_idx;
				GF_Mesh *face = faces[i];
				init_idx = mesh->v_count;
				/*quads only*/
				mesh_set_vertex_vx(mesh, &face->vertices[0]);
				mesh_set_vertex_vx(mesh, &face->vertices[1]);
				mesh_set_vertex_vx(mesh, &face->vertices[2]);
				mesh_set_vertex_vx(mesh, &face->vertices[3]);
				mesh_set_triangle(mesh, init_idx, init_idx + 2, init_idx + 3);
				mesh_set_triangle(mesh, init_idx, init_idx + 3, init_idx + 1);
			}
			mesh_free(faces[i]);
		}

		/*destroy faces*/
		gf_free(faces);
	}

	mesh->mesh_type = MESH_TRIANGLES;
	if (has_color) mesh->flags |= MESH_HAS_COLOR;
	mesh_update_bounds(mesh);
	if (!eg->ccw) mesh->flags |= MESH_IS_CW;
	if (eg->solid) mesh->flags |= MESH_IS_SOLID;
#ifndef GPAC_DISABLE_X3D
	if (colorRGBA) mesh->flags |= MESH_HAS_ALPHA;
#endif
	gf_mesh_build_aabbtree(mesh);
}


typedef struct
{
	SFVec3f yaxis, zaxis, xaxis;
} SCP;

typedef struct
{
	SFVec3f pt, yaxis, zaxis, xaxis;
	u32 max_idx;
} SCPInfo;

#define REGISTER_POINT_FACE(FACE_IDX) \
	{	u32 fidx;	\
		fidx = FACE_IDX; \
		mesh_set_vertex_vx(faces[fidx], &vx);	\
		if (smooth_normals) {	\
			register_point_in_face(&faces_info[fidx], pidx);	\
			register_face_in_point(&pts_info[pidx], fidx);	\
		} \
	} \
 

#define NEAR_ZERO(__x) (ABS(__x)<=FIX_EPSILON)

static void mesh_extrude_path_intern(GF_Mesh *mesh, GF_Path *path, MFVec3f *thespine, Fixed creaseAngle, Fixed min_cx, Fixed min_cy, Fixed width_cx, Fixed width_cy, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool tx_along_spine)
{
	GF_Mesh **faces;
	GF_Vertex vx;
	struct face_info *faces_info;
	struct pt_info *pts_info;
	GF_Matrix mx;
	SCP *SCPs, SCPbegin, SCPend;
	SCPInfo *SCPi;
	Bool smooth_normals, spine_closed, check_first_spine_vec, do_close;
	u32 i, j, k, nb_scp, nb_spine, face_count, pt_count, faces_per_cross, begin_face, end_face, face_spines, pts_per_cross, cur_pts_in_cross,cur, nb_pts, convexity;
	SFVec3f *spine, v1, v2, n, spine_vec;
	Fixed cross_len, spine_len, cur_cross, cur_spine;
	SFRotation r;
	SFVec2f scale;

	if (!path->n_contours) return;
	if (path->n_points<2) return;
	if (thespine->count<2) return;

	spine_closed = 0;
	if (gf_vec_equal(thespine->vals[0], thespine->vals[thespine->count-1])) spine_closed = 1;
	if (spine_closed && (thespine->count==2)) return;

	gf_path_flatten(path);

	memset(&vx, 0, sizeof(GF_Vertex));
	pts_per_cross = 0;
	cross_len = 0;
	cur = 0;
	for (i=0; i<path->n_contours; i++) {
		nb_pts = 1 + path->contours[i] - cur;
		pts_per_cross += nb_pts;
		v1.z = 0;
		for (j=1; j<nb_pts; j++) {
			v1.x = path->points[j+cur].x - path->points[j-1+cur].x;
			v1.y = path->points[j+cur].y - path->points[j-1+cur].y;
			cross_len += gf_vec_len(v1);
		}
	}

	faces_per_cross	= pts_per_cross - path->n_contours;
	begin_face = end_face = 0;
	face_spines = face_count = (thespine->count-1)*faces_per_cross;
	if (begin_cap) {
		begin_face = face_count;
		face_count ++;
	}
	if (end_cap) {
		end_face = face_count;
		face_count ++;
	}

	pt_count = pts_per_cross * thespine->count;
	smooth_normals = NEAR_ZERO(creaseAngle) ? 0 : 1;

	faces = (GF_Mesh**)gf_malloc(sizeof(GF_Mesh *)*face_count);
	for (i=0; i<face_count; i++) faces[i] = new_mesh();
	faces_info = NULL;
	pts_info = NULL;

	/*alloc face & normals tables*/
	if (smooth_normals) {
		faces_info = (struct face_info*)gf_malloc(sizeof(struct face_info)*face_count);
		memset(faces_info, 0, sizeof(struct face_info)*face_count);
		pts_info = (struct pt_info*)gf_malloc(sizeof(struct pt_info)*pt_count);
		memset(pts_info, 0, sizeof(struct pt_info)*pt_count);
	}


	spine = thespine->vals;
	nb_spine = thespine->count;
	SCPs = (SCP *)gf_malloc(sizeof(SCP) * nb_spine);
	memset(SCPs, 0, sizeof(SCP) * nb_spine);
	SCPi = (SCPInfo *) gf_malloc(sizeof(SCPInfo) * nb_spine);
	memset(SCPi, 0, sizeof(SCPInfo) * nb_spine);

	/*collect all # SCPs:
	1- if a spine has identical consecutive points with # orientation, these points use the same SCPs
	2- if 2 segs of the spine are colinear, they also use the same SCP
	*/
	SCPi[0].pt = spine[0];
	SCPi[0].max_idx = 0;
	nb_scp=1;
	spine_len = 0;
	check_first_spine_vec = 1;
	for (i=1; i<nb_spine; i++) {
		Fixed len;
		/*also get spine length for txcoord*/
		gf_vec_diff(v2, spine[i], spine[i-1]);
		len = gf_vec_len(v2);
		spine_len += len;
		if (check_first_spine_vec && len) {
			check_first_spine_vec = 0;
			spine_vec = v2;
		}

		/*case 1: same point, same SCP*/
		if (gf_vec_equal(SCPi[nb_scp-1].pt, spine[i])) {
			SCPi[nb_scp-1].max_idx = i;
			continue;
		}
		/*last point in spine*/
		if (i+1 == nb_spine) {
			nb_scp++;
			SCPi[nb_scp-1].pt = spine[i];
			SCPi[nb_scp-1].max_idx = i;
			break;
		}

		gf_vec_diff(v1, spine[i+1], spine[i]);
		gf_vec_diff(v2, SCPi[nb_scp-1].pt, spine[i]);
		n = gf_vec_cross(v1, v2);
		/*case 2: spine segs are colinear*/
		if (!n.x && !n.y && !n.z) {
			SCPi[nb_scp-1].max_idx = i;
		}
		/*OK new SCP*/
		else {
			nb_scp++;
			SCPi[nb_scp-1].pt = spine[i];
			SCPi[nb_scp-1].max_idx = i;
		}
	}

	/*all colinear!!*/
	if (nb_scp<=2) {
		SCPi[0].xaxis.x = FIX_ONE;
		SCPi[0].xaxis.y = 0;
		SCPi[0].xaxis.z = 0;
		SCPi[0].yaxis.x = 0;
		SCPi[0].yaxis.y = FIX_ONE;
		SCPi[0].yaxis.z = 0;
		SCPi[0].zaxis.x = 0;
		SCPi[0].zaxis.y = 0;
		SCPi[0].zaxis.z = FIX_ONE;
		/*compute rotation from (0, 1, 0) to spine_vec*/
		if (!check_first_spine_vec) {
			Fixed alpha, gamma;
			Fixed cos_a, sin_a, sin_g, cos_g;

			gf_vec_norm(&spine_vec);
			if (! NEAR_ZERO(spine_vec.x) ) {
				if (spine_vec.x >= FIX_ONE-FIX_EPSILON) alpha = GF_PI2;
				else if (spine_vec.x <= -FIX_ONE+FIX_EPSILON) alpha = -GF_PI2;
				else alpha = gf_asin(spine_vec.x);
				cos_a = gf_cos(alpha);
				sin_a = spine_vec.x;

				if (NEAR_ZERO(cos_a)) gamma = 0;
				else {
					Fixed __abs;
					gamma = gf_acos(gf_divfix(spine_vec.y, cos_a));
					sin_g = gf_sin(gamma);
					__abs = gf_divfix(spine_vec.z, cos_a) + sin_g;
					if (ABS(__abs) > ABS(sin_g) ) gamma *= -1;
				}
				cos_g = gf_cos(gamma);
				if (NEAR_ZERO(cos_g)) {
					cos_g = 0;
					sin_g = FIX_ONE;
				} else {
					sin_g = gf_sin(gamma);
				}
				SCPi[0].yaxis.y = gf_mulfix(cos_a, sin_g);
				SCPi[0].yaxis.z = gf_mulfix(cos_a, cos_g);
				SCPi[0].yaxis.x = sin_a;
				SCPi[0].zaxis.y = -gf_mulfix(sin_a, sin_g);
				SCPi[0].zaxis.z = -gf_mulfix(sin_a, cos_g);
				SCPi[0].zaxis.x = cos_a;
			}
			if (! NEAR_ZERO(spine_vec.z) ) {
				if (spine_vec.z >= FIX_ONE-FIX_EPSILON) alpha = GF_PI2;
				else if (spine_vec.z <= -FIX_ONE+FIX_EPSILON) alpha = -GF_PI2;
				else alpha = gf_asin(spine_vec.z);
				cos_a = gf_cos(alpha);
				sin_a = spine_vec.z;

				if (NEAR_ZERO(cos_a) ) gamma = 0;
				else {
					Fixed __abs;
					gamma = gf_acos(gf_divfix(spine_vec.y, cos_a));
					sin_g = gf_sin(gamma);
					__abs = gf_divfix(spine_vec.x, cos_a) + sin_g;
					if (ABS(__abs) > ABS(sin_g) ) gamma *= -1;
				}
				cos_g = gf_cos(gamma);
				sin_g = gf_sin(gamma);
				SCPi[0].yaxis.y = gf_mulfix(cos_a, sin_g);
				SCPi[0].yaxis.x = gf_mulfix(cos_a, cos_g);
				SCPi[0].yaxis.z = sin_a;
				SCPi[0].zaxis.y = -gf_mulfix(sin_a, sin_g);
				SCPi[0].zaxis.x = -gf_mulfix(sin_a, cos_g);
				SCPi[0].zaxis.z = cos_a;
			}
		}
		for (i=0; i<nb_spine; i++) {
			SCPs[i].xaxis = SCPi[0].xaxis;
			SCPs[i].yaxis = SCPi[0].yaxis;
			SCPs[i].zaxis = SCPi[0].zaxis;
		}
	}
	/*not colinear*/
	else {
		assert(nb_scp<=nb_spine);

		/*now non-cap SCPs axis*/
		for (i=1; i<nb_scp-1; i++) {
			/*get Y axis*/
			gf_vec_diff(SCPi[i].yaxis, SCPi[i+1].pt, SCPi[i-1].pt);
			/*get Z axis*/
			gf_vec_diff(v1, SCPi[i+1].pt, SCPi[i].pt);
			gf_vec_diff(v2, SCPi[i-1].pt, SCPi[i].pt);
			SCPi[i].zaxis = gf_vec_cross(v1, v2);
		}
		/*compute head and tail*/
		if (spine_closed) {
			gf_vec_diff(SCPi[0].yaxis, SCPi[1].pt, SCPi[nb_scp-2].pt);
			gf_vec_diff(v1, SCPi[1].pt, SCPi[0].pt);
			gf_vec_diff(v2, SCPi[nb_scp-2].pt, SCPi[0].pt);
			SCPi[0].zaxis = gf_vec_cross(v1, v2);
			SCPi[nb_scp-1].yaxis = SCPi[0].yaxis;
			SCPi[nb_scp-1].zaxis = SCPi[0].zaxis;
		} else {
			gf_vec_diff(SCPi[0].yaxis, SCPi[1].pt, SCPi[0].pt);
			SCPi[0].zaxis = SCPi[1].zaxis;
			gf_vec_diff(SCPi[nb_scp-1].yaxis, SCPi[nb_scp-1].pt, SCPi[nb_scp-2].pt);
			SCPi[nb_scp-1].zaxis = SCPi[nb_scp-2].zaxis;
		}
		/*check orientation*/
		for (i=1; i<nb_scp; i++) {
			Fixed res = gf_vec_dot(SCPi[i].zaxis, SCPi[i-1].zaxis);
			if (res<0) gf_vec_rev(SCPi[i].zaxis);
		}
		/*and assign SCPs*/
		j=0;
		for (i=0; i<nb_scp; i++) {
			/*compute X, norm vectors*/
			SCPi[i].xaxis = gf_vec_cross(SCPi[i].yaxis, SCPi[i].zaxis);
			gf_vec_norm(&SCPi[i].xaxis);
			gf_vec_norm(&SCPi[i].yaxis);
			gf_vec_norm(&SCPi[i].zaxis);
			/*assign SCPs*/
			while (j<=SCPi[i].max_idx) {
				SCPs[j].xaxis = SCPi[i].xaxis;
				SCPs[j].yaxis = SCPi[i].yaxis;
				SCPs[j].zaxis = SCPi[i].zaxis;
				j++;
			}
		}
	}
	gf_free(SCPi);

	r.x = r.q = r.z = 0;
	r.y = FIX_ONE;
	scale.x = scale.y = FIX_ONE;

	cur_spine = 0;
	/*insert all points of the extrusion*/
	for (i=0; i<nb_spine; i++) {
		u32 cur_face_in_cross;
		SCP *curSCP;

		if (spine_closed && (i+1==nb_spine)) {
			curSCP = &SCPs[0];
			do_close =  1;
		} else {
			curSCP = &SCPs[i];
			do_close =  0;

			/*compute X*/
			curSCP->xaxis = gf_vec_cross(curSCP->yaxis, curSCP->zaxis);
			gf_vec_norm(&curSCP->xaxis);
			gf_vec_norm(&curSCP->yaxis);
			gf_vec_norm(&curSCP->zaxis);

			if (spine_ori && (i<spine_ori->count)) r = spine_ori->vals[i];
			if (spine_scale && (i<spine_scale->count)) scale = spine_scale->vals[i];

			gf_mx_init(mx);
			gf_mx_add_rotation(&mx, r.q, r.x, r.y, r.z);
			gf_mx_apply_vec(&mx, &curSCP->xaxis);
			gf_mx_apply_vec(&mx, &curSCP->yaxis);
			gf_mx_apply_vec(&mx, &curSCP->zaxis);
		}

		vx.texcoords.y = gf_divfix(cur_spine, spine_len);

		cur_pts_in_cross = 0;
		cur_face_in_cross = 0;
		cur = 0;
		for (j=0; j<path->n_contours; j++) {
			Bool subpath_closed;
			nb_pts = 1+path->contours[j] - cur;
			cur_cross = 0;
			subpath_closed = 0;
			if ((path->points[cur].x==path->points[path->contours[j]].x) && (path->points[cur].y==path->points[path->contours[j]].y))
				subpath_closed = 1;

			for (k=0; k<nb_pts; k++) {
				u32 pidx = k + cur_pts_in_cross + i*pts_per_cross;
				if (do_close) pidx = k + cur_pts_in_cross;

				v1.x = path->points[k+cur].x;
				v1.z = path->points[k+cur].y;

				if (tx_along_spine) {
					vx.texcoords.x = gf_divfix(cur_cross, cross_len);
				} else {
					vx.texcoords.x = gf_divfix(v1.x - min_cx, width_cx);
					vx.texcoords.y = gf_divfix(v1.z - min_cy, width_cy);
				}

				/*handle closed cross-section*/
				if (subpath_closed && (k+1==nb_pts)) {
					pidx = cur_pts_in_cross + i*pts_per_cross;
					if (do_close) pidx = cur_pts_in_cross;

					v1.x = path->points[cur].x;
					v1.z = path->points[cur].y;
				}
				v1.x = gf_mulfix(v1.x, scale.x);
				v1.z = gf_mulfix(v1.z, scale.y);
				v1.y = 0;
				vx.pos.x = gf_mulfix(v1.x, curSCP->xaxis.x) + gf_mulfix(v1.y, curSCP->yaxis.x) + gf_mulfix(v1.z, curSCP->zaxis.x) + spine[i].x;
				vx.pos.y = gf_mulfix(v1.x, curSCP->xaxis.y) + gf_mulfix(v1.y, curSCP->yaxis.y) + gf_mulfix(v1.z, curSCP->zaxis.y) + spine[i].y;
				vx.pos.z = gf_mulfix(v1.x, curSCP->xaxis.z) + gf_mulfix(v1.y, curSCP->yaxis.z) + gf_mulfix(v1.z, curSCP->zaxis.z) + spine[i].z;

				/*in current spine*/
				if (i+1<nb_spine) {
					/*current face*/
					if (k<nb_pts-1) REGISTER_POINT_FACE(k + cur_face_in_cross + i*faces_per_cross);
					/*previous face*/
					if (k) REGISTER_POINT_FACE(k-1 + cur_face_in_cross + i*faces_per_cross);
				}
				/*first spine*/
				else if (smooth_normals && do_close) {
					if (k<nb_pts-1) {
						register_point_in_face(&faces_info[k + cur_face_in_cross], pidx);
						register_face_in_point(&pts_info[pidx], k + cur_face_in_cross);
					}
					/*previous face*/
					if (k) {
						register_point_in_face(&faces_info[k-1 + cur_face_in_cross], pidx);
						register_face_in_point(&pts_info[pidx], k-1 + cur_face_in_cross);
					}
				}
				/*in previous spine*/
				if (i) {
					/*face "below" face*/
					if (k<nb_pts-1) REGISTER_POINT_FACE(k + cur_face_in_cross + (i-1)*faces_per_cross);
					/*previous face "below" face*/
					if (k) REGISTER_POINT_FACE(k-1 + cur_face_in_cross + (i-1)*faces_per_cross);
				}

				if (k+1<nb_pts) {
					v1.z = 0;
					v1.x = path->points[k+1+cur].x - path->points[k+cur].x;
					v1.y = path->points[k+1+cur].y - path->points[k+cur].y;
					cur_cross += gf_vec_len(v1);
				}

			}
			cur_face_in_cross += nb_pts-1;
			cur_pts_in_cross += nb_pts;
			cur += nb_pts;
		}
		if (i+1<nb_spine) {
			gf_vec_diff(v1, spine[i+1], spine[i]);
			cur_spine += gf_vec_len(v1);
		}
	}

	/*generate triangles & normals*/
	for (i=0; i<face_spines; i++) {
		mesh_set_triangle(faces[i], 0, 1, 3);
		mesh_set_triangle(faces[i], 0, 3, 2);
		gf_vec_diff(v1, faces[i]->vertices[1].pos, faces[i]->vertices[0].pos);
		gf_vec_diff(v2, faces[i]->vertices[3].pos, faces[i]->vertices[0].pos);
		n = gf_vec_cross(v1, v2);
		gf_vec_norm(&n);
		for (j=0; j<faces[i]->v_count; j++) {
			MESH_SET_NORMAL(faces[i]->vertices[j], n);
			if (smooth_normals) faces_info[i].nor = n;
		}
	}


	/*generate begin cap*/
	if (begin_face) {
		scale.x = scale.y = FIX_ONE;
		if (spine_scale && spine_scale->count) scale = spine_scale->vals[0];

		/*get first SCP after rotation*/
		SCPbegin = SCPs[0];

		n = SCPbegin.yaxis;
		gf_vec_norm(&n);

		convexity = gf_polygone2d_get_convexity(path->points, path->n_points);
		switch (convexity) {
		case GF_POLYGON_CONVEX_CCW:
		case GF_POLYGON_COMPLEX_CCW:
			break;
		default:
			gf_vec_rev(n);
			break;
		}

		MESH_SET_NORMAL(vx, n);


		if (smooth_normals) {
			faces_info[begin_face].nor = n;
			assert(gf_vec_len(n));
		}
		cur_pts_in_cross = 0;
		cur = 0;
		for (i=0; i<path->n_contours; i++) {
			Bool subpath_closed = 0;
			nb_pts = 1 + path->contours[i] - cur;
			if ((path->points[cur].x==path->points[path->contours[i]].x) && (path->points[cur].y==path->points[path->contours[i]].y))
				subpath_closed = 1;

			for (j=0; j<nb_pts; j++) {
				u32 pidx = j + (pts_per_cross-1-cur_pts_in_cross);
				v1.x = path->points[j+cur].x;
				v1.z = path->points[j+cur].y;
				vx.texcoords.x = gf_divfix(v1.x - min_cx, width_cx);
				vx.texcoords.y = gf_divfix(v1.z - min_cy, width_cy);
				/*handle closed cross-section*/
				if (subpath_closed  && (j+1==nb_pts)) {
					pidx = (pts_per_cross-1-cur_pts_in_cross);
					v1.x = path->points[cur].x;
					v1.z = path->points[cur].y;
				}
				v1.x = gf_mulfix(v1.x , scale.x);
				v1.z = gf_mulfix(v1.z , scale.y);
				v1.y = 0;
				vx.pos.x = gf_mulfix(v1.x, SCPbegin.xaxis.x) + gf_mulfix(v1.y, SCPbegin.yaxis.x) + gf_mulfix(v1.z, SCPbegin.zaxis.x) + spine[0].x;
				vx.pos.y = gf_mulfix(v1.x, SCPbegin.xaxis.y) + gf_mulfix(v1.y, SCPbegin.yaxis.y) + gf_mulfix(v1.z, SCPbegin.zaxis.y) + spine[0].y;
				vx.pos.z = gf_mulfix(v1.x, SCPbegin.xaxis.z) + gf_mulfix(v1.y, SCPbegin.yaxis.z) + gf_mulfix(v1.z, SCPbegin.zaxis.z) + spine[0].z;
				REGISTER_POINT_FACE(begin_face);
			}
			cur_pts_in_cross += nb_pts;
			cur += nb_pts;
		}
	}

	/*generate end cap*/
	if (end_face) {
		scale.x = scale.y = FIX_ONE;
		if (spine_scale && (nb_spine-1<spine_scale->count)) scale = spine_scale->vals[nb_spine-1];
		/*get last SCP after rotation*/
		SCPend = SCPs[nb_spine-1];

		n = SCPend.yaxis;
		gf_vec_norm(&n);
		MESH_SET_NORMAL(vx, n);
		convexity = gf_polygone2d_get_convexity(path->points, path->n_points);
		switch (convexity) {
		case GF_POLYGON_CONVEX_CCW:
		case GF_POLYGON_COMPLEX_CCW:
			gf_vec_rev(vx.normal);
			gf_vec_rev(n);
			break;
		}

		if (smooth_normals) {
			faces_info[end_face].nor = n;
			assert(gf_vec_len(n));
		}
		cur_pts_in_cross = 0;

		cur = 0;
		for (i=0; i<path->n_contours; i++) {
			Bool subpath_closed = 0;
			nb_pts = 1 + path->contours[i] - cur;
			if ((path->points[cur].x==path->points[path->contours[i]].x) && (path->points[cur].y==path->points[path->contours[i]].y))
				subpath_closed = 1;

			for (j=0; j<nb_pts; j++) {
				u32 pidx = j + cur_pts_in_cross + (nb_spine-1)*pts_per_cross;
				v1.x = path->points[j+cur].x;
				v1.z = path->points[j+cur].y;
				vx.texcoords.x = gf_divfix(v1.x - min_cx, width_cx);
				vx.texcoords.y = gf_divfix(v1.z - min_cy, width_cy);
				/*handle closed cross-section*/
				if (subpath_closed && (j+1==nb_pts)) {
					pidx = cur_pts_in_cross + (nb_spine-1)*pts_per_cross;
					v1.x = path->points[cur].x;
					v1.z = path->points[cur].y;
				}
				v1.x = gf_mulfix(v1.x, scale.x);
				v1.z = gf_mulfix(v1.z, scale.y);
				v1.y = 0;
				vx.pos.x = gf_mulfix(v1.x, SCPend.xaxis.x) + gf_mulfix(v1.y, SCPend.yaxis.x) + gf_mulfix(v1.z, SCPend.zaxis.x) + spine[nb_spine-1].x;
				vx.pos.y = gf_mulfix(v1.x, SCPend.xaxis.y) + gf_mulfix(v1.y, SCPend.yaxis.y) + gf_mulfix(v1.z, SCPend.zaxis.y) + spine[nb_spine-1].y;
				vx.pos.z = gf_mulfix(v1.x, SCPend.xaxis.z) + gf_mulfix(v1.y, SCPend.yaxis.z) + gf_mulfix(v1.z, SCPend.zaxis.z) + spine[nb_spine-1].z;

				REGISTER_POINT_FACE(end_face);
			}
			cur_pts_in_cross += nb_pts;
			cur += nb_pts;
		}
	}


	if (smooth_normals) {
		Fixed cosCrease;
		/*we only support 0->PI, whatever exceeds is smoothest*/
		if (creaseAngle>GF_PI) cosCrease = -FIX_ONE;
		else cosCrease = gf_cos(creaseAngle);

		for (i=0; i<face_count; i++) {
			for (j=0; j<faces[i]->v_count; j++) {
				n = smooth_face_normals(pts_info, pt_count, faces_info, face_count, j, i, cosCrease);
				MESH_SET_NORMAL(faces[i]->vertices[j], n);
			}
		}

		if (faces_info) {
			for (i=0; i<face_count; i++) if (faces_info[i].idx) gf_free(faces_info[i].idx);
			gf_free(faces_info);
		}
		if (pts_info) {
			for (i=0; i<pt_count; i++) if (pts_info[i].faces) gf_free(pts_info[i].faces);
			gf_free(pts_info);
		}
		mesh->flags |= MESH_IS_SMOOTHED;
	}

	mesh->mesh_type = MESH_TRIANGLES;

	for (i=0; i<face_spines; i++) {
		if (faces[i]->v_count) {
			u32 init_idx;
			GF_Mesh *face = faces[i];
			init_idx = mesh->v_count;
			/*quads only*/
			mesh_set_vertex_vx(mesh, &face->vertices[0]);
			mesh_set_vertex_vx(mesh, &face->vertices[1]);
			mesh_set_vertex_vx(mesh, &face->vertices[2]);
			mesh_set_vertex_vx(mesh, &face->vertices[3]);
			mesh_set_triangle(mesh, init_idx + 0, init_idx + 1, init_idx + 3);
			mesh_set_triangle(mesh, init_idx + 0, init_idx + 3, init_idx + 2);
		}
		mesh_free(faces[i]);
	}
	if (begin_face) {
		if (path->n_contours>1) {
#ifdef GPAC_HAS_GLU
			u32 *ptsPerFace = gf_malloc(sizeof(u32)*path->n_contours);
			/*we reversed begin cap!!!*/
			cur = 0;
			for (i=0; i<path->n_contours; i++) {
				nb_pts = 1+path->contours[i] - cur;
				cur+=nb_pts;
				ptsPerFace[i] = nb_pts;
			}
			TesselateFaceMeshComplex(mesh, faces[begin_face], path->n_contours, ptsPerFace);
			gf_free(ptsPerFace);
#endif
		} else {
			TesselateFaceMesh(mesh, faces[begin_face]);
		}
		mesh_free(faces[begin_face]);
	}
	if (end_face) {
		if (path->n_contours>1) {
#ifdef GPAC_HAS_GLU
			u32 *ptsPerFace = gf_malloc(sizeof(u32)*path->n_contours);
			cur = 0;
			for (i=0; i<path->n_contours; i++) {
				nb_pts = 1+path->contours[i] - cur;
				cur+=nb_pts;
				ptsPerFace[i] = nb_pts;
			}
			TesselateFaceMeshComplex(mesh, faces[end_face], path->n_contours, ptsPerFace);
			gf_free(ptsPerFace);
#endif
		} else {
			TesselateFaceMesh(mesh, faces[end_face]);
		}
		mesh_free(faces[end_face]);
	}
	gf_free(faces);
	gf_free(SCPs);

	/*FIXME: this is correct except we need to handle path cw/ccw - until then no possibility
	to get correct lighting*/
	/*
		if (path->subpathlen && path->subpath[0]->closed && ((begin_face && end_face) || spine_closed))
			mesh->flags |= MESH_IS_SOLID;
		else
			mesh->flags &= ~MESH_IS_SOLID;
	*/
}

void mesh_extrude_path_ext(GF_Mesh *mesh, GF_Path *path, MFVec3f *thespine, Fixed creaseAngle, Fixed min_cx, Fixed min_cy, Fixed width_cx, Fixed width_cy, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool tx_along_spine)
{
	mesh_extrude_path_intern(mesh, path, thespine, creaseAngle, min_cx, min_cy, width_cx, width_cy, begin_cap, end_cap, spine_ori, spine_scale, tx_along_spine);
}

void mesh_extrude_path(GF_Mesh *mesh, GF_Path *path, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool tx_along_spine)
{
	GF_Rect rc;
	gf_path_get_bounds(path, &rc);
	mesh_extrude_path_intern(mesh, path, thespine, creaseAngle, rc.x, rc.y-rc.height, rc.width, rc.height, begin_cap, end_cap, spine_ori, spine_scale, tx_along_spine);
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

void mesh_new_extrusion(GF_Mesh *mesh, GF_Node *node)
{
	GF_Path *path;
	u32 i;
	M_Extrusion *ext = (M_Extrusion *)node;

	mesh_reset(mesh);
	path = gf_path_new();
	gf_path_add_move_to(path, ext->crossSection.vals[0].x, ext->crossSection.vals[0].y);
	for (i=1; i<ext->crossSection.count; i++) {
		gf_path_add_line_to(path, ext->crossSection.vals[i].x, ext->crossSection.vals[i].y);
	}

	mesh_extrude_path(mesh, path, &ext->spine, ext->creaseAngle, ext->beginCap, ext->endCap, &ext->orientation, &ext->scale, 1);
	gf_path_del(path);

	mesh_update_bounds(mesh);
	if (!ext->ccw) mesh->flags |= MESH_IS_CW;
}


#endif /*GPAC_DISABLE_VRML*/

#endif //!defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_EVG)
