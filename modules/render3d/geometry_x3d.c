/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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


#include "visual_surface.h"

Bool Stack2DIntersectWithRay(GF_Node *owner, GF_Ray *r, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords);

static Bool R3D_NoIntersectionWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *vec, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	return 0;
}

static Bool Disk2DIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Fixed r, sqr, sqdist;
	Bool inside;
	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;
	sqdist = gf_mulfix(outPoint->x, outPoint->x) + gf_mulfix(outPoint->y, outPoint->y);
	r = ((X_Disk2D *)owner)->outerRadius;
	sqr = gf_mulfix(r, r);
	inside = (sqr >= sqdist) ? 1 : 0;
	if (!inside) return 0;
	r = ((X_Disk2D *)owner)->innerRadius;
	sqr = r*r;
	inside = (sqr <= sqdist) ? 1 : 0;
	if (!inside) return 0;

	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = FIX_ONE; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, r) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, r) + FIX_ONE/2;
	}
	return 1;
}

static void RenderDisk2D(GF_Node *node, void *rs)
{
	stack2D *st = (stack2D *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		Fixed a = ((X_Disk2D *) node)->outerRadius * 2;
		stack2D_reset(st);
		/*FIXME - move to 3D stack and skip path stuff to enable it with OpenGL-ES*/
		gf_path_add_ellipse(st->path, 0, 0, a, a);
		a = ((X_Disk2D *) node)->innerRadius * 2;
		if (a) gf_path_add_ellipse(st->path, 0, 0, a, a);
		mesh_from_path(st->mesh, st->path);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		stack2D_draw(st, eff);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitDisk2D(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderDisk2D);
	st->IntersectWithRay = Disk2DIntersectWithRay;
}

static void RenderArc2D(GF_Node *node, void *rs)
{
	stack2D *st = (stack2D *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		stack2D_reset(st);
		if (gf_node_get_tag(node)==TAG_X3D_Arc2D) {
			X_Arc2D *a = (X_Arc2D *) node;
			gf_path_add_arc(st->path, a->radius, a->startAngle, a->endAngle, 0);
			mesh_get_outline(st->mesh, st->path);
		} else {
			X_ArcClose2D *a = (X_ArcClose2D *) node;
			gf_path_add_arc(st->path, a->radius, a->startAngle, a->endAngle, !stricmp(a->closureType.buffer, "PIE") ? 2 : 1);
			mesh_from_path(st->mesh, st->path);
		}
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		stack2D_draw(st, eff);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitArc2D(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderArc2D);
	if (gf_node_get_tag(node)==TAG_X3D_Arc2D) {
		st->IntersectWithRay = R3D_NoIntersectionWithRay;
	} else {
		st->IntersectWithRay = Stack2DIntersectWithRay;
	}
}

static void RenderPolyline2D(GF_Node *node, void *rs)
{
	stack2D *st = (stack2D *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		u32 i;
		X_Polyline2D *a = (X_Polyline2D *) node;
		stack2D_reset(st);
		for (i=0; i<a->lineSegments.count; i++) {
			if (i) {
				gf_path_add_line_to(st->path, a->lineSegments.vals[i].x, a->lineSegments.vals[i].y);
			} else {
				gf_path_add_move_to(st->path, a->lineSegments.vals[i].x, a->lineSegments.vals[i].y);
			}
		}
		mesh_get_outline(st->mesh, st->path);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		stack2D_draw(st, eff);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitPolyline2D(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderPolyline2D);
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderPolypoint2D(GF_Node *node, void *rs)
{
	SFColorRGBA col;
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		u32 i;
		X_Polypoint2D *p = (X_Polypoint2D *)node;
		mesh_reset(st->mesh);
		st->mesh->mesh_type = MESH_POINTSET;
		col.red = col.green = col.blue = 0; col.alpha = FIX_ONE;
		for (i=0; i<p->point.count; i++) {
			mesh_set_point(st->mesh, p->point.vals[i].x, p->point.vals[i].y, 0, col);
			mesh_set_index(st->mesh, st->mesh->v_count-1);
		}
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitPolypoint2D(Render3D *sr, GF_Node *node)
{
	DrawableStack *st = BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderPolypoint2D);
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderLineSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		u32 i, j, c_idx;
		GenMFField *cols;
		GF_Vertex vx;
		Bool rgba_col;
		X_LineSet *p = (X_LineSet *)node;
		X_Coordinate *c = (X_Coordinate *) p->coord;

		gf_node_dirty_clear(node, 0);

		mesh_reset(st->mesh);
		st->mesh->mesh_type = MESH_LINESET;

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
						vx.color = ((MFColorRGBA *)cols)->vals[c_idx];
					} else {
						vx.color = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[c_idx]);
					}
				}
				mesh_set_vertex_vx(st->mesh, &vx);
				if (j) {
					mesh_set_index(st->mesh, st->mesh->v_count-2);
					mesh_set_index(st->mesh, st->mesh->v_count-1);
				}
				c_idx++;
				if (c_idx==c->point.count) break;
			}
		}
		if (cols) st->mesh->flags |= MESH_HAS_COLOR;
		mesh_update_bounds(st->mesh);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitLineSet(Render3D *sr, GF_Node *node)
{
	DrawableStack *st = BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderLineSet);
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderTriangleSet2D(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		u32 i, count, idx;
		GF_Vertex v1, v2, v3;
		X_TriangleSet2D *p = (X_TriangleSet2D *)node;
		mesh_reset(st->mesh);
		st->mesh->mesh_type = MESH_TRIANGLES;
		v1.color.red = v1.color.green = v1.color.blue = 0;
		v1.normal.x = v1.normal.y = 0; v1.normal.z = FIX_ONE;
		v1.pos.z = 0;
		v3 = v2 = v1;
		count = p->vertices.count;
		while (count%3) count--;
		for (i=0; i<count; i+=3) {
			idx = st->mesh->v_count;
			v1.pos.x = p->vertices.vals[i].x;
			v1.pos.y = p->vertices.vals[i].y;
			v2.pos.x = p->vertices.vals[i+1].x;
			v2.pos.y = p->vertices.vals[i+1].y;
			v3.pos.x = p->vertices.vals[i+2].x;
			v3.pos.y = p->vertices.vals[i+2].y;
			mesh_set_vertex_vx(st->mesh, &v1);
			mesh_set_vertex_vx(st->mesh, &v2);
			mesh_set_vertex_vx(st->mesh, &v3);
			gf_vec_diff(v2.pos, v2.pos, v1.pos);
			gf_vec_diff(v3.pos, v3.pos, v1.pos);
			v1.pos = gf_vec_cross(v2.pos, v3.pos);
			if (v1.pos.z<0) {
				mesh_set_triangle(st->mesh, idx, idx+2, idx+1);
			} else {
				mesh_set_triangle(st->mesh, idx, idx+1, idx+2);
			}
		}
		st->mesh->flags |= MESH_IS_2D;
		mesh_update_bounds(st->mesh);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitTriangleSet2D(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderTriangleSet2D);
}

static void BuildTriangleSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 i, count, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
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
				vx.color = ((MFColorRGBA *)cols)->vals[idx];
			} else {
				vx.color = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
			}
		}
		if (norms && (norms->count>idx)) {
			vx.normal = norms->vals[idx];
			gf_vec_norm(&vx.normal);
		}
		if (txcoords) {
			if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
		} 
		/*X3D says nothing about default texture mapping here...*/
		else if (!generate_tx) {
			switch (i%3) {
			case 2: vx.texcoords.x = FIX_ONE; vx.texcoords.y = 0; break;
			case 1: vx.texcoords.x = FIX_ONE/2; vx.texcoords.y = FIX_ONE; break;
			case 0: vx.texcoords.x = 0; vx.texcoords.y = 0; break;
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

static void RenderTriangleSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;


	if (gf_node_dirty_get(node)) {
		X_TriangleSet *ts = (X_TriangleSet *)node;
		gf_node_dirty_clear(node, 0);
		if (!ts->coord) return;
		BuildTriangleSet(st->mesh, ts->coord, ts->color, ts->texCoord, ts->normal, NULL, ts->normalPerVertex, ts->ccw, ts->solid);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitTriangleSet(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderTriangleSet);
}


static void RenderIndexedTriangleSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;


	if (gf_node_dirty_get(node)) {
		X_IndexedTriangleSet *its = (X_IndexedTriangleSet *)node;
		gf_node_dirty_clear(node, 0);
		if (!its->coord) return;
		BuildTriangleSet(st->mesh, its->coord, its->color, its->texCoord, its->normal, &its->index, its->normalPerVertex, its->ccw, its->solid);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void ITS_SetIndex(GF_Node *node)
{
	X_IndexedTriangleSet *its = (X_IndexedTriangleSet*)node;
	gf_sg_vrml_field_copy(&its->index, &its->set_index, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&its->set_index, GF_SG_VRML_MFINT32);
}

void R3D_InitIndexedTriangleSet(Render3D *sr, GF_Node *node)
{
	X_IndexedTriangleSet *its = (X_IndexedTriangleSet*)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderIndexedTriangleSet);
	its->on_set_index = ITS_SetIndex;
}


static void BuildTriangleStripSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *stripList, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 strip, i, cur_idx, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
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
					GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render 3D] bad formatted X3D triangle strip\n"));
					return;
				}
				idx = indices->vals[cur_idx];
			} else {
				idx = cur_idx;
			}

			vx.pos = c->point.vals[idx];

			if (cols && (cols->count>idx)) {
				if (rgba_col) {
					vx.color = ((MFColorRGBA *)cols)->vals[idx];
				} else {
					vx.color = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
				}
			}
			/*according to X3D spec, if normal field is set, it is ALWAYS as normal per vertex*/
			if (norms && (norms->count>idx)) {
				vx.normal = norms->vals[idx];
				gf_vec_norm(&vx.normal);
			}
			if (txcoords) {
				if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
			} 
			/*X3D says nothing about default texture mapping here...*/
			else if (!generate_tx) {
				switch (idx%3) {
				case 2: vx.texcoords.x = FIX_ONE; vx.texcoords.y = 0; break;
				case 1: vx.texcoords.x = FIX_ONE/2; vx.texcoords.y = FIX_ONE; break;
				case 0: vx.texcoords.x = 0; vx.texcoords.y = 0; break;
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
				v = gf_vec_scale(mesh->vertices[mesh->indices[i]].normal,-1);
				mesh->vertices[mesh->indices[i]].normal = v;
				mesh->vertices[mesh->indices[i+1]].normal = v;
				mesh->vertices[mesh->indices[i+2]].normal = v;
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
				if (stripList->vals[strip] <= 3) { cur_face ++; continue; }

				/*first face normal*/
				n_0 = mesh->vertices[mesh->indices[3*cur_face]].normal;
				/*second face normal*/
				n_1 = mesh->vertices[mesh->indices[3*(cur_face+1)]].normal;
				gf_vec_add(n_avg, n_0, n_1);
				gf_vec_norm(&n_avg);
				/*assign to second point of first face and first of second face*/
				mesh->vertices[mesh->indices[3*cur_face+1]].normal = n_avg;
				mesh->vertices[mesh->indices[3*(cur_face+1)]].normal = n_avg;
				nb_face = stripList->vals[strip] - 2;
				cur_face++;
				for (i=1; i<nb_face-1; i++) {
					/*get normal (use second pt of current face since first has been updated)*/
					n_2 = mesh->vertices[mesh->indices[3*cur_face + 1]].normal;
					gf_vec_add(n_avg, n_0, n_1);
					gf_vec_add(n_avg, n_avg, n_2);
					gf_vec_norm(&n_avg);
					/*last of prev face*/
					mesh->vertices[mesh->indices[3*cur_face - 1]].normal = n_avg;
					/*second of current face*/
					mesh->vertices[mesh->indices[3*cur_face + 1]].normal = n_avg;
					/*first of next face*/
					mesh->vertices[mesh->indices[3*cur_face + 3]].normal = n_avg;
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

static void RenderTriangleStripSet(GF_Node *node, void *rs)
{
	X_TriangleStripSet *tss = (X_TriangleStripSet *)node;
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (!tss->coord) return;

	if (gf_node_dirty_get(node)) {
		gf_node_dirty_clear(node, 0);
		BuildTriangleStripSet(st->mesh, tss->coord, tss->color, tss->texCoord, tss->normal, &tss->stripCount, NULL, tss->normalPerVertex, tss->ccw, tss->solid);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitTriangleStripSet(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderTriangleStripSet);
}

static void RenderIndexedTriangleStripSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		MFInt32 stripList;
		u32 i, nb_strips;
		X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet *)node;
		gf_node_dirty_clear(node, 0);
		if (!itss->coord) return;

		stripList.count = 0; stripList.vals = NULL;
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
		BuildTriangleStripSet(st->mesh, itss->coord, itss->color, itss->texCoord, itss->normal, &stripList, &itss->index, itss->normalPerVertex, itss->ccw, itss->solid);
		gf_sg_vrml_mf_reset(&stripList, GF_SG_VRML_MFINT32);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void ITSS_SetIndex(GF_Node *node)
{
	X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet*)node;
	gf_sg_vrml_field_copy(&itss->index, &itss->set_index, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&itss->set_index, GF_SG_VRML_MFINT32);
}

void R3D_InitIndexedTriangleStripSet(Render3D *sr, GF_Node *node)
{
	X_IndexedTriangleStripSet *itss = (X_IndexedTriangleStripSet*)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderIndexedTriangleStripSet);
	itss->on_set_index = ITSS_SetIndex;
}


static void BuildTriangleFanSet(GF_Mesh *mesh, GF_Node *_coords, GF_Node *_color, GF_Node *_txcoords, GF_Node *_normal, MFInt32 *fanList, MFInt32 *indices, Bool normalPerVertex, Bool ccw, Bool solid)
{
	u32 fan, i, cur_idx, generate_tx;
	GF_Vertex vx;
	GenMFField *cols;
	MFVec3f *norms;
	MFVec2f *txcoords;
	Bool rgba_col;
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
					GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render 3D] bad formatted X3D triangle set\n"));
					return;
				}
				idx = indices->vals[cur_idx];
			} else {
				idx = cur_idx;
			}
			vx.pos = c->point.vals[idx];

			if (cols && (cols->count>idx)) {
				if (rgba_col) {
					vx.color = ((MFColorRGBA *)cols)->vals[idx];
				} else {
					vx.color = gf_sg_sfcolor_to_rgba( ((MFColor *)cols)->vals[idx]);
				}
			}
			/*according to X3D spec, if normal field is set, it is ALWAYS as normal per vertex*/
			if (norms && (norms->count>idx)) {
				vx.normal = norms->vals[idx];
				gf_vec_norm(&vx.normal);
			}
			if (txcoords) {
				if (txcoords->count>idx) vx.texcoords = txcoords->vals[idx];
			} 
			/*X3D says nothing about default texture mapping here...*/
			else if (!generate_tx) {
				switch (idx%3) {
				case 2: vx.texcoords.x = FIX_ONE; vx.texcoords.y = 0; break;
				case 1: vx.texcoords.x = FIX_ONE/2; vx.texcoords.y = FIX_ONE; break;
				case 0: vx.texcoords.x = 0; vx.texcoords.y = 0; break;
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
				if (fanList->vals[fan] == 3) { cur_face++; continue; }
	
				start_face = cur_face;

				/*first face normal*/
				n_0 = mesh->vertices[mesh->indices[3*cur_face]].normal;
				n_tot = n_0;
				cur_face++;
				nb_face = fanList->vals[fan] - 2;
				for (i=1; i<nb_face; i++) {
					n_1 = mesh->vertices[mesh->indices[3*cur_face + 1]].normal;
					gf_vec_add(n_avg, n_0, n_1);
					gf_vec_norm(&n_avg);
					mesh->vertices[mesh->indices[3*cur_face + 1]].normal = n_avg;
					gf_vec_add(n_tot, n_tot, n_1);
					n_0 = n_1;
					cur_face++;
				}
				/*and assign center normal*/
				gf_vec_norm(&n_tot);
				for (i=0; i<nb_face; i++) {
					mesh->vertices[mesh->indices[3*(i+start_face)]].normal = n_tot;
				}
			}
		}
	}
	if (solid) mesh->flags |= MESH_IS_SOLID;
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void RenderTriangleFanSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		X_TriangleFanSet *tfs = (X_TriangleFanSet *)node;
		gf_node_dirty_clear(node, 0);
		if (!tfs->coord) return;
		BuildTriangleFanSet(st->mesh, tfs->coord, tfs->color, tfs->texCoord, tfs->normal, &tfs->fanCount, NULL, tfs->normalPerVertex, tfs->ccw, tfs->solid);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitTriangleFanSet(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderTriangleFanSet);
}

static void RenderIndexedTriangleFanSet(GF_Node *node, void *rs)
{
	DrawableStack *st = (DrawableStack *)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (gf_node_dirty_get(node)) {
		MFInt32 fanList;
		u32 i, nb_fans;
		X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
		gf_node_dirty_clear(node, 0);
		if (!itfs->coord) return;

		fanList.count = 0; fanList.vals = NULL;
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
		BuildTriangleFanSet(st->mesh, itfs->coord, itfs->color, itfs->texCoord, itfs->normal, &fanList, &itfs->index, itfs->normalPerVertex, itfs->ccw, itfs->solid);
		gf_sg_vrml_mf_reset(&fanList, GF_SG_VRML_MFINT32);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void ITFS_SetIndex(GF_Node *node)
{
	X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
	gf_sg_vrml_field_copy(&itfs->index, &itfs->set_index, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&itfs->set_index, GF_SG_VRML_MFINT32);
}

void R3D_InitIndexedTriangleFanSet(Render3D *sr, GF_Node *node)
{
	X_IndexedTriangleFanSet *itfs = (X_IndexedTriangleFanSet *)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderIndexedTriangleFanSet);
	itfs->on_set_index = ITFS_SetIndex;
}

