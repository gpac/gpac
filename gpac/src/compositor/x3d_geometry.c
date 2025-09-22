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


#include "nodes_stacks.h"
#include "visual_manager.h"
#include "drawable.h"

#if !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_COMPOSITOR)

static void disk2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		Fixed a = ((X_Disk2D *) node)->outerRadius * 2;
		drawable_reset_path(stack);
		gf_path_add_ellipse(stack->path, 0, 0, a, a);
		a = ((X_Disk2D *) node)->innerRadius * 2;
		if (a) gf_path_add_ellipse(stack->path, 0, 0, a, a);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}

static void TraverseDisk2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	disk2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			/*FIXME - enable it with OpenGL-ES*/
			mesh_from_path(stack->mesh, stack->path);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	}
}

void compositor_init_disk2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseDisk2D);
}

static void arc2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		drawable_reset_path(stack);
		if (gf_node_get_tag(node)==TAG_X3D_Arc2D) {
			X_Arc2D *a = (X_Arc2D *) node;
			gf_path_add_arc(stack->path, a->radius, a->startAngle, a->endAngle, 0);
		} else {
			X_ArcClose2D *a = (X_ArcClose2D *) node;
			gf_path_add_arc(stack->path, a->radius, a->startAngle, a->endAngle, !stricmp(a->closureType.buffer, "PIE") ? 2 : 1);
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}

static void TraverseArc2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	arc2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			if (gf_node_get_tag(node)==TAG_X3D_Arc2D) {
				mesh_get_outline(stack->mesh, stack->path);
			} else {
				mesh_from_path(stack->mesh, stack->path);
			}
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
#ifndef GPAC_DISABLE_3D
		gf_bbox_from_rect(&tr_state->bbox, &tr_state->bounds);
#endif
		return;
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	}
}

void compositor_init_arc2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseArc2D);
}

static void polyline2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		u32 i;
		X_Polyline2D *a = (X_Polyline2D *) node;
		drawable_reset_path(stack);
		for (i=0; i<a->lineSegments.count; i++) {
			if (i) {
				gf_path_add_line_to(stack->path, a->lineSegments.vals[i].x, a->lineSegments.vals[i].y);
			} else {
				gf_path_add_move_to(stack->path, a->lineSegments.vals[i].x, a->lineSegments.vals[i].y);
			}
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}

static void TraversePolyline2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	polyline2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_get_outline(stack->mesh, stack->path);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	}
}

void compositor_init_polyline2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraversePolyline2D);
}

static void triangleset2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		u32 i, count;
		X_TriangleSet2D *p = (X_TriangleSet2D *)node;
		drawable_reset_path(stack);
		count = p->vertices.count;
		while (count%3) count--;
		for (i=0; i<count; i+=3) {
			gf_path_add_move_to(stack->path, p->vertices.vals[i].x, p->vertices.vals[i].y);
			gf_path_add_line_to(stack->path, p->vertices.vals[i+1].x, p->vertices.vals[i+1].y);
			gf_path_add_line_to(stack->path, p->vertices.vals[i+2].x, p->vertices.vals[i+2].y);
			gf_path_close(stack->path);
		}
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}

static void TraverseTriangleSet2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	triangleset2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			SFColorRGBA col;
			u32 i, count, idx;
			GF_Vertex v1, v2, v3;
			X_TriangleSet2D *p = (X_TriangleSet2D *)node;

			stack->mesh = new_mesh();
			stack->mesh->mesh_type = MESH_TRIANGLES;
			col.red = col.green = col.blue = 0;
			col.alpha = FIX_ONE;
			v1.color = MESH_MAKE_COL(col);
			v1.normal.x = v1.normal.y = 0;
			v1.normal.z = MESH_NORMAL_UNIT;
			v1.pos.z = 0;
			v3 = v2 = v1;
			count = p->vertices.count;
			while (count%3) count--;
			for (i=0; i<count; i+=3) {
				idx = stack->mesh->v_count;
				v1.pos.x = p->vertices.vals[i].x;
				v1.pos.y = p->vertices.vals[i].y;
				v2.pos.x = p->vertices.vals[i+1].x;
				v2.pos.y = p->vertices.vals[i+1].y;
				v3.pos.x = p->vertices.vals[i+2].x;
				v3.pos.y = p->vertices.vals[i+2].y;
				mesh_set_vertex_vx(stack->mesh, &v1);
				mesh_set_vertex_vx(stack->mesh, &v2);
				mesh_set_vertex_vx(stack->mesh, &v3);
				gf_vec_diff(v2.pos, v2.pos, v1.pos);
				gf_vec_diff(v3.pos, v3.pos, v1.pos);
				v1.pos = gf_vec_cross(v2.pos, v3.pos);
				if (v1.pos.z<0) {
					mesh_set_triangle(stack->mesh, idx, idx+2, idx+1);
				} else {
					mesh_set_triangle(stack->mesh, idx, idx+1, idx+2);
				}
			}
			stack->mesh->flags |= MESH_IS_2D;
			mesh_update_bounds(stack->mesh);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	}
}

void compositor_init_triangle_set2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseTriangleSet2D);
}

#ifndef GPAC_DISABLE_3D

static void build_polypoint2d(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	u32 i;
	SFColorRGBA col;
	X_Polypoint2D *p = (X_Polypoint2D *)node;

	stack->mesh->mesh_type = MESH_POINTSET;
	col.red = col.green = col.blue = 0;
	col.alpha = FIX_ONE;
	for (i=0; i<p->point.count; i++) {
		mesh_set_point(stack->mesh, p->point.vals[i].x, p->point.vals[i].y, 0, col);
		mesh_set_index(stack->mesh, stack->mesh->v_count-1);
	}
}

static void TraversePolypoint2D(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_polypoint2d);
}

void compositor_init_polypoint2d(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraversePolypoint2D);
}

static void build_line_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	u32 i, j, c_idx;
	GenMFField *cols;
	GF_Vertex vx;
	Bool rgba_col;
	SFColorRGBA rgba;
	X_LineSet *p = (X_LineSet *)node;
	X_Coordinate *c = (X_Coordinate *) p->coord;

	stack->mesh->mesh_type = MESH_LINESET;

	cols = NULL;
	rgba_col = 0;
	if (p->color) {
		if (gf_node_get_tag(p->color)==TAG_X3D_ColorRGBA) {
			rgba_col = 1;
			cols = (GenMFField *) & ((X_ColorRGBA *) p->color)->color;
		} else {
			cols = (GenMFField *) & ((M_Color *) p->color)->color;
		}
	}
	c_idx = 0;
	memset(&vx, 0, sizeof(GF_Vertex));
	for (i=0; i<p->vertexCount.count; i++) {
		if (p->vertexCount.vals[i]<2) continue;

		for (j=0; j<(u32) p->vertexCount.vals[i]; j++) {
			vx.pos = c->point.vals[c_idx];
			if (cols && (cols->count>c_idx)) {
				if (rgba_col) {
					rgba = ((MFColorRGBA *)cols)->vals[c_idx];
				} else {
					rgba = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[c_idx]);
				}
				vx.color = MESH_MAKE_COL(rgba);
			}
			mesh_set_vertex_vx(stack->mesh, &vx);
			if (j) {
				mesh_set_index(stack->mesh, stack->mesh->v_count-2);
				mesh_set_index(stack->mesh, stack->mesh->v_count-1);
			}
			c_idx++;
			if (c_idx==c->point.count) break;
		}
	}
	if (cols) stack->mesh->flags |= MESH_HAS_COLOR;
	mesh_update_bounds(stack->mesh);
}

static void TraverseLineSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_line_set);
}


void compositor_init_lineset(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseLineSet);
}

static void BuildTriangleSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 i, count, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
	SFColorRGBA rgba;
	X_Coordinate *c = (X_Coordinate *) _coords;

	mesh_reset(mesh);

	cols = NULL;
	rgba_col = 0;
	if (_color) {
		if (gf_node_get_tag(_color)==TAG_X3D_ColorRGBA) {
			rgba_col = 1;
			cols = (GenMFField *) & ((X_ColorRGBA *) _color)->color;
		} else {
			cols = (GenMFField *) & ((M_Color *) _color)->color;
		}
	}
	norms = NULL;
	if (_normal) norms = & ((M_Normal *)_normal)->vector;
	txcoords = NULL;
	generate_tx = 0;
	/*FIXME - this can't work with multitexturing*/
	if (_txcoords) {
		switch (gf_node_get_tag(_txcoords)) {
		case TAG_X3D_TextureCoordinate:
		case TAG_MPEG4_TextureCoordinate:
			txcoords = & ((M_TextureCoordinate *)_txcoords)->point;
			break;
		case TAG_X3D_TextureCoordinateGenerator:
			generate_tx = 1;
			break;
		}
	}

	if (indices) {
		count = indices->count;
	} else {
		count = c->point.count;
	}
	while (count%3) count--;
	memset(&vx, 0, sizeof(GF_Vertex));
	for (i=0; i<count; i++) {
		u32 idx;
		if (indices) {
			if (indices->count<=i) return;
			idx = indices->vals[i];
		} else {
			idx = i;
		}
		vx.pos = c->point.vals[idx];
		if (cols && (cols->count>idx)) {
			if (rgba_col) {
				rgba = ((MFColorRGBA *)cols)->vals[idx];
			} else {
				rgba = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
			}
			vx.color = MESH_MAKE_COL(rgba);
		}
		if (norms && (norms->count>idx)) {
			SFVec3f n = norms->vals[idx];
			gf_vec_norm(&n);
			MESH_SET_NORMAL(vx, n);
		}
		if (txcoords) {
			if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
		}
		/*X3D says nothing about default texture mapping here...*/
		else if (!generate_tx) {
			switch (i%3) {
			case 2:
				vx.texcoords.x = FIX_ONE;
				vx.texcoords.y = 0;
				break;
			case 1:
				vx.texcoords.x = FIX_ONE/2;
				vx.texcoords.y = FIX_ONE;
				break;
			case 0:
				vx.texcoords.x = 0;
				vx.texcoords.y = 0;
				break;
			}
		}
		mesh_set_vertex_vx(mesh, &vx);
	}
	for (i=0; i<mesh->v_count; i+=3) {
		mesh_set_triangle(mesh, i, i+1, i+2);
	}
	if (generate_tx) mesh_generate_tex_coords(mesh, _txcoords);

	if (!ccw) mesh->flags |= MESH_IS_CW;
	if (cols) mesh->flags |= MESH_HAS_COLOR;
	if (rgba_col) mesh->flags |= MESH_HAS_ALPHA;
	if (!_normal) mesh_recompute_normals(mesh);
	if (solid) mesh->flags |= MESH_IS_SOLID;
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void build_triangle_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	X_TriangleSet *ts = (X_TriangleSet *)node;
	if (!ts->coord) return;
	BuildTriangleSet(stack->mesh, ts->coord, ts->color, ts->texCoord, ts->normal, NULL, ts->normalPerVertex, ts->ccw, ts->solid);
}

static void TraverseTriangleSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_triangle_set);
}


void compositor_init_triangle_set(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseTriangleSet);
}


static void build_indexed_triangle_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	X_IndexedTriangleSet *its = (X_IndexedTriangleSet*)node;
	if (!its->coord) return;
	BuildTriangleSet(stack->mesh, its->coord, its->color, its->texCoord, its->normal, &its->index, its->normalPerVertex, its->ccw, its->solid);
}

static void TraverseIndexedTriangleSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_indexed_triangle_set);
}

static void ITS_SetIndex(GF_Node *node, GF_Route *route)
{
	if (node) {
		X_IndexedTriangleSet *its = (X_IndexedTriangleSet*)node;
		gf_sg_vrml_field_copy(&its->index, &its->set_index, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&its->set_index, GF_SG_VRML_MFINT32);
	}
}

void compositor_init_indexed_triangle_set(GF_Compositor *compositor, GF_Node *node)
{
	X_IndexedTriangleSet *its = (X_IndexedTriangleSet*)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseIndexedTriangleSet);
	its->on_set_index = ITS_SetIndex;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		ITS_SetIndex(NULL, NULL);
	}
#endif

}


static void BuildTriangleStripSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *stripList, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 strip, i, cur_idx, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
	SFColorRGBA rgba;
	X_Coordinate *c = (X_Coordinate *) _coords;

	mesh_reset(mesh);

	cols = NULL;
	rgba_col = 0;
	if (_color) {
		if (gf_node_get_tag(_color)==TAG_X3D_ColorRGBA) {
			rgba_col = 1;
			cols = (GenMFField *) & ((X_ColorRGBA *) _color)->color;
		} else {
			cols = (GenMFField *) & ((M_Color *) _color)->color;
		}
	}
	norms = NULL;
	if (_normal) norms = & ((M_Normal *)_normal)->vector;
	txcoords = NULL;
	generate_tx = 0;
	/*FIXME - this can't work with multitexturing*/
	if (_txcoords) {
		switch (gf_node_get_tag(_txcoords)) {
		case TAG_X3D_TextureCoordinate:
		case TAG_MPEG4_TextureCoordinate:
			txcoords = & ((M_TextureCoordinate *)_txcoords)->point;
			break;
		case TAG_X3D_TextureCoordinateGenerator:
			generate_tx = 1;
			break;
		}
	}
	memset(&vx, 0, sizeof(GF_Vertex));
	cur_idx = 0;
	for (strip= 0; strip<stripList->count; strip++) {
		u32 start_idx = mesh->v_count;
		if (stripList->vals[strip] < 3) continue;

		for (i=0; i<(u32) stripList->vals[strip]; i++) {
			u32 idx;
			if (indices) {
				if (indices->count<=cur_idx) return;
				if (indices->vals[cur_idx] == -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[X3D] bad formatted X3D triangle strip\n"));
					return;
				}
				idx = indices->vals[cur_idx];
			} else {
				idx = cur_idx;
			}

			vx.pos = c->point.vals[idx];

			if (cols && (cols->count>idx)) {
				if (rgba_col) {
					rgba = ((MFColorRGBA *)cols)->vals[idx];
				} else {
					rgba = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
				}
				vx.color = MESH_MAKE_COL(rgba);
			}
			/*according to X3D spec, if normal field is set, it is ALWAYS as normal per vertex*/
			if (norms && (norms->count>idx)) {
				SFVec3f n = norms->vals[idx];
				gf_vec_norm(&n);
				MESH_SET_NORMAL(vx, n);
			}
			if (txcoords) {
				if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
			}
			/*X3D says nothing about default texture mapping here...*/
			else if (!generate_tx) {
				switch (idx%3) {
				case 2:
					vx.texcoords.x = FIX_ONE;
					vx.texcoords.y = 0;
					break;
				case 1:
					vx.texcoords.x = FIX_ONE/2;
					vx.texcoords.y = FIX_ONE;
					break;
				case 0:
					vx.texcoords.x = 0;
					vx.texcoords.y = 0;
					break;
				}
			}
			if (i>2) {
				/*duplicate last 2 vertices (we really need independent vertices to handle normals per face)*/
				mesh_set_vertex_vx(mesh, &mesh->vertices[mesh->v_count-2]);
				mesh_set_vertex_vx(mesh, &mesh->vertices[mesh->v_count-2]);
			}
			mesh_set_vertex_vx(mesh, &vx);

			cur_idx ++;
			if (indices) {
				if (cur_idx>=indices->count) break;
			} else if (cur_idx==c->point.count) break;

		}
		for (i=start_idx; i<mesh->v_count; i+=3) {
			mesh_set_triangle(mesh, i, i+1, i+2);
		}
		/*get rid of -1*/
		if (indices && (cur_idx<indices->count) && (indices->vals[cur_idx]==-1)) cur_idx++;
	}
	if (generate_tx) mesh_generate_tex_coords(mesh, _txcoords);

	if (cols) mesh->flags |= MESH_HAS_COLOR;
	if (rgba_col) mesh->flags |= MESH_HAS_ALPHA;
	if (_normal) {
		if (!ccw) mesh->flags |= MESH_IS_CW;
	}
	/*reorder everything to CCW*/
	else {
		u32 cur_face = 0;
		mesh_recompute_normals(mesh);
		for (i=0; i<mesh->i_count; i+= 3) {
			if ((ccw && (cur_face%2)) || (!ccw && !(cur_face%2))) {
				SFVec3f v;
				u32 idx;
				MESH_GET_NORMAL(v, mesh->vertices[mesh->indices[i]]);
				v = gf_vec_scale(v,-1);
				MESH_SET_NORMAL(mesh->vertices[mesh->indices[i]], v);
				mesh->vertices[mesh->indices[i+1]].normal = mesh->vertices[mesh->indices[i]].normal;
				mesh->vertices[mesh->indices[i+2]].normal = mesh->vertices[mesh->indices[i]].normal;
				idx = mesh->indices[i+2];
				mesh->indices[i+2] = mesh->indices[i+1];
				mesh->indices[i+1] = idx;
			}
			cur_face++;
		}
		if (normalPerVertex) {
			cur_face = 0;
			for (strip=0; strip<stripList->count; strip++) {
				SFVec3f n_0, n_1, n_2, n_avg;
				u32 nb_face;
				if (stripList->vals[strip] < 3) continue;
				if (stripList->vals[strip] <= 3) {
					cur_face ++;
					continue;
				}

				/*first face normal*/
				MESH_GET_NORMAL(n_0, mesh->vertices[mesh->indices[3*cur_face]]);
				/*second face normal*/
				MESH_GET_NORMAL(n_1, mesh->vertices[mesh->indices[3*(cur_face+1)]]);

				gf_vec_add(n_avg, n_0, n_1);
				gf_vec_norm(&n_avg);
				/*assign to second point of first face and first of second face*/
				MESH_SET_NORMAL(mesh->vertices[mesh->indices[3*cur_face+1]], n_avg);
				MESH_SET_NORMAL(mesh->vertices[mesh->indices[3*(cur_face+1)]], n_avg);
				nb_face = stripList->vals[strip] - 2;
				cur_face++;
				for (i=1; i<nb_face-1; i++) {
					/*get normal (use second pt of current face since first has been updated)*/
					MESH_GET_NORMAL(n_2, mesh->vertices[mesh->indices[3*cur_face + 1]]);
					gf_vec_add(n_avg, n_0, n_1);
					gf_vec_add(n_avg, n_avg, n_2);
					gf_vec_norm(&n_avg);
					/*last of prev face*/
					MESH_SET_NORMAL(mesh->vertices[mesh->indices[3*cur_face - 1]], n_avg);
					/*second of current face*/
					mesh->vertices[mesh->indices[3*cur_face + 1]].normal = mesh->vertices[mesh->indices[3*cur_face - 1]].normal;
					/*first of next face*/
					mesh->vertices[mesh->indices[3*cur_face + 3]].normal = mesh->vertices[mesh->indices[3*cur_face - 1]].normal;
					n_0 = n_1;
					n_1 = n_2;
					cur_face++;
				}
			}
		}
	}
	if (solid) mesh->flags |= MESH_IS_SOLID;
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void build_triangle_strip_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	X_TriangleStripSet *tss = (X_TriangleStripSet *)node;
	if (!tss->coord) return;
	BuildTriangleStripSet(stack->mesh, tss->coord, tss->color, tss->texCoord, tss->normal, &tss->stripCount, NULL, tss->normalPerVertex, tss->ccw, tss->solid);
}

static void TraverseTriangleStripSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_triangle_strip_set);
}

void compositor_init_triangle_strip_set(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseTriangleStripSet);
}

static void build_indexed_triangle_strip_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	MFInt32 stripList;
	u32 i, nb_strips;
	X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet *)node;

	if (!itss->coord) return;

	stripList.count = 0;
	stripList.vals = NULL;
	nb_strips = 0;
	for (i=0; i<itss->index.count; i++) {
		if (itss->index.vals[i]==-1) {
			if (nb_strips>=3) {
				u32 *out_nb;
				gf_sg_vrml_mf_append(&stripList, GF_SG_VRML_MFINT32, (void **) &out_nb);
				*out_nb = nb_strips;
			}
			nb_strips = 0;
		} else {
			nb_strips++;
		}
	}
	if (nb_strips>=3) {
		u32 *out_nb;
		gf_sg_vrml_mf_append(&stripList, GF_SG_VRML_MFINT32, (void **) &out_nb);
		*out_nb = nb_strips;
	}
	BuildTriangleStripSet(stack->mesh, itss->coord, itss->color, itss->texCoord, itss->normal, &stripList, &itss->index, itss->normalPerVertex, itss->ccw, itss->solid);
	gf_sg_vrml_mf_reset(&stripList, GF_SG_VRML_MFINT32);
}

static void TraverseIndexedTriangleStripSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_indexed_triangle_strip_set);
}

static void ITSS_SetIndex(GF_Node *node, GF_Route *route)
{
	if (node) {
		X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet*)node;
		gf_sg_vrml_field_copy(&itss->index, &itss->set_index, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&itss->set_index, GF_SG_VRML_MFINT32);
	}
}

void compositor_init_indexed_triangle_strip_set(GF_Compositor *compositor, GF_Node *node)
{
	X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet*)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseIndexedTriangleStripSet);
	itss->on_set_index = ITSS_SetIndex;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		ITSS_SetIndex(NULL, NULL);
	}
#endif
}


static void BuildTriangleFanSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *fanList, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 fan, i, cur_idx, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
	SFColorRGBA rgba;
	X_Coordinate *c = (X_Coordinate *) _coords;
	mesh_reset(mesh);

	cols = NULL;
	rgba_col = 0;
	if (_color) {
		if (gf_node_get_tag(_color)==TAG_X3D_ColorRGBA) {
			rgba_col = 1;
			cols = (GenMFField *) & ((X_ColorRGBA *) _color)->color;
		} else {
			cols = (GenMFField *) & ((M_Color *) _color)->color;
		}
	}
	norms = NULL;
	if (_normal) norms = & ((M_Normal *)_normal)->vector;
	txcoords = NULL;
	generate_tx = 0;
	/*FIXME - this can't work with multitexturing*/
	if (_txcoords) {
		switch (gf_node_get_tag(_txcoords)) {
		case TAG_X3D_TextureCoordinate:
		case TAG_MPEG4_TextureCoordinate:
			txcoords = & ((M_TextureCoordinate *)_txcoords)->point;
			break;
		case TAG_X3D_TextureCoordinateGenerator:
			generate_tx = 1;
			break;
		}
	}

	memset(&vx, 0, sizeof(GF_Vertex));
	cur_idx = 0;
	for (fan= 0; fan<fanList->count; fan++) {
		u32 start_idx = mesh->v_count;
		if (fanList->vals[fan] < 3) continue;

		for (i=0; i<(u32) fanList->vals[fan]; i++) {
			u32 idx;
			if (indices) {
				if (indices->count<=cur_idx) return;
				if (indices->vals[cur_idx] == -1) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[X3D] bad formatted X3D triangle set\n"));
					return;
				}
				idx = indices->vals[cur_idx];
			} else {
				idx = cur_idx;
			}
			vx.pos = c->point.vals[idx];

			if (cols && (cols->count>idx)) {
				if (rgba_col) {
					rgba = ((MFColorRGBA *)cols)->vals[idx];
				} else {
					rgba = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
				}
				vx.color = MESH_MAKE_COL(rgba);
			}
			/*according to X3D spec, if normal field is set, it is ALWAYS as normal per vertex*/
			if (norms && (norms->count>idx)) {
				SFVec3f n = norms->vals[idx];
				gf_vec_norm(&n);
				MESH_SET_NORMAL(vx, n);
			}
			if (txcoords) {
				if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
			}
			/*X3D says nothing about default texture mapping here...*/
			else if (!generate_tx) {
				switch (idx%3) {
				case 2:
					vx.texcoords.x = FIX_ONE;
					vx.texcoords.y = 0;
					break;
				case 1:
					vx.texcoords.x = FIX_ONE/2;
					vx.texcoords.y = FIX_ONE;
					break;
				case 0:
					vx.texcoords.x = 0;
					vx.texcoords.y = 0;
					break;
				}
			}
			mesh_set_vertex_vx(mesh, &vx);

			cur_idx ++;
			if (indices) {
				if (cur_idx>=indices->count) break;
			} else if (cur_idx==c->point.count) break;

			if (i>1) {
				mesh_set_vertex_vx(mesh, &mesh->vertices[start_idx]);
				mesh_set_vertex_vx(mesh, &vx);
			}
		}
		for (i=start_idx; i<mesh->v_count; i+=3) {
			mesh_set_triangle(mesh, i, i+1, i+2);
		}
		/*get rid of -1*/
		if (indices && (cur_idx<indices->count) && (indices->vals[cur_idx]==-1)) cur_idx++;
	}
	if (generate_tx) mesh_generate_tex_coords(mesh, _txcoords);

	if (cols) mesh->flags |= MESH_HAS_COLOR;
	if (rgba_col) mesh->flags |= MESH_HAS_ALPHA;
	if (!ccw) mesh->flags |= MESH_IS_CW;
	if (!_normal) {
		mesh_recompute_normals(mesh);
		if (normalPerVertex) {
			u32 cur_face = 0;
			for (fan=0; fan<fanList->count; fan++) {
				SFVec3f n_0, n_1, n_avg, n_tot;
				u32 nb_face, start_face;
				if (fanList->vals[fan] < 3) continue;
				if (fanList->vals[fan] == 3) {
					cur_face++;
					continue;
				}

				start_face = cur_face;

				/*first face normal*/
				MESH_GET_NORMAL(n_0, mesh->vertices[mesh->indices[3*cur_face]]);
				n_tot = n_0;
				cur_face++;
				nb_face = fanList->vals[fan] - 2;
				for (i=1; i<nb_face; i++) {
					MESH_GET_NORMAL(n_1, mesh->vertices[mesh->indices[3*cur_face + 1]]);
					gf_vec_add(n_avg, n_0, n_1);
					gf_vec_norm(&n_avg);
					MESH_SET_NORMAL(mesh->vertices[mesh->indices[3*cur_face + 1]], n_avg);
					gf_vec_add(n_tot, n_tot, n_1);
					n_0 = n_1;
					cur_face++;
				}
				/*and assign center normal*/
				gf_vec_norm(&n_tot);
				for (i=0; i<nb_face; i++) {
					MESH_SET_NORMAL(mesh->vertices[mesh->indices[3*(i+start_face)]], n_tot);
				}
			}
		}
	}
	if (solid) mesh->flags |= MESH_IS_SOLID;
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void build_triangle_fan_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	X_TriangleFanSet *tfs = (X_TriangleFanSet *)node;
	if (!tfs->coord) return;
	BuildTriangleFanSet(stack->mesh, tfs->coord, tfs->color, tfs->texCoord, tfs->normal, &tfs->fanCount, NULL, tfs->normalPerVertex, tfs->ccw, tfs->solid);
}

static void TraverseTriangleFanSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_triangle_fan_set);
}

void compositor_init_triangle_fan_set(GF_Compositor *compositor, GF_Node *node)
{
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseTriangleFanSet);
}

static void build_indexed_triangle_fan_set(GF_Node *node, Drawable3D *stack, GF_TraverseState *tr_state)
{
	MFInt32 fanList;
	u32 i, nb_fans;
	X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
	gf_node_dirty_clear(node, 0);
	if (!itfs->coord) return;

	fanList.count = 0;
	fanList.vals = NULL;
	nb_fans = 0;
	for (i=0; i<itfs->index.count; i++) {
		if (itfs->index.vals[i]==-1) {
			if (nb_fans>=3) {
				u32 *out_nb;
				gf_sg_vrml_mf_append(&fanList, GF_SG_VRML_MFINT32, (void **) &out_nb);
				*out_nb = nb_fans;
			}
			nb_fans = 0;
		} else {
			nb_fans++;
		}
	}
	if (nb_fans>=3) {
		u32 *out_nb;
		gf_sg_vrml_mf_append(&fanList, GF_SG_VRML_MFINT32, (void **) &out_nb);
		*out_nb = nb_fans;
	}
	BuildTriangleFanSet(stack->mesh, itfs->coord, itfs->color, itfs->texCoord, itfs->normal, &fanList, &itfs->index, itfs->normalPerVertex, itfs->ccw, itfs->solid);
	gf_sg_vrml_mf_reset(&fanList, GF_SG_VRML_MFINT32);
}

static void TraverseIndexedTriangleFanSet(GF_Node *node, void *rs, Bool is_destroy)
{
	drawable_3d_base_traverse(node, rs, is_destroy, build_indexed_triangle_fan_set);
}

static void ITFS_SetIndex(GF_Node *node, GF_Route *route)
{
	if (node) {
		X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
		gf_sg_vrml_field_copy(&itfs->index, &itfs->set_index, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&itfs->set_index, GF_SG_VRML_MFINT32);
	}
}

void compositor_init_indexed_triangle_fan_set(GF_Compositor *compositor, GF_Node *node)
{
	X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
	drawable_3d_new(node);
	gf_node_set_callback_function(node, TraverseIndexedTriangleFanSet);
	itfs->on_set_index = ITFS_SetIndex;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		ITFS_SetIndex(NULL, NULL);
	}
#endif
}

#endif /*GPAC_DISABLE_3D*/

#endif // !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_COMPOSITOR)
