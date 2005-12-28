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
#include <gpac/options.h>


/*
		Shape 
*/
static void RenderShape(GF_Node *node, void *rs)
{
	DrawableStack *st;
	u32 cull_bckup;
	RenderEffect3D *eff; 
	M_Shape *shape;
	
	eff = (RenderEffect3D *) rs;
	if (eff->traversing_mode==TRAVERSE_LIGHTING) return;

	shape = (M_Shape *) node;
	if (!shape->geometry) return;
	eff->appear = (GF_Node *) shape->appearance;

	/*reset this node dirty flag (because bitmap may trigger bounds invalidation on the fly)*/
	gf_node_dirty_clear(node, 0);

	/*check traverse mode, and take care of switch-off flag*/
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*this is done regardless of switch flag*/
		gf_node_render((GF_Node *) shape->geometry, eff);
		/*reset appearance dirty flag to get sure size changes in textures invalidates the parent graph(s)*/
		gf_node_dirty_clear(eff->appear, 0);
		eff->appear = NULL;
	} else {
		if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return;
		/*if we're here we passed culler already*/
		if (eff->traversing_mode==TRAVERSE_RENDER) 
			gf_node_render((GF_Node *) shape->geometry, eff);
		else if (eff->traversing_mode==TRAVERSE_SORT) {
			st = gf_node_get_private(shape->geometry);
			cull_bckup = eff->cull_flag;
			if (st && node_cull(eff, &st->mesh->bounds, 0)) {
				VS_RegisterContext(eff, node, &st->mesh->bounds, 1);
			}
			eff->cull_flag = cull_bckup;
		}
		else if (eff->traversing_mode==TRAVERSE_PICK) drawable_do_pick(shape->geometry, eff);
		else if (eff->traversing_mode==TRAVERSE_COLLIDE) drawable_do_collide(shape->geometry, eff);

	}
}


Bool R3D_Get2DPlaneIntersection(GF_Ray *ray, SFVec3f *res)
{
	GF_Plane p;
	Fixed t, t2;
	p.normal.x = p.normal.y = 0; p.normal.z = FIX_ONE;
	p.d = 0;
	t2 = gf_vec_dot(p.normal, ray->dir);
	if (t2 == 0) return 0;
	t = - gf_divfix(gf_vec_dot(p.normal, ray->orig) + p.d, t2);
	if (t<0) return 0;
	*res = gf_vec_scale(ray->dir, t);
	gf_vec_add(*res, ray->orig, *res);
	return 1;
}

Bool R3D_PickInClipper(RenderEffect3D *eff, GF_Rect *clip)
{
	SFVec3f pos;
	GF_Matrix mx;
	GF_Ray r;
	gf_mx_copy(mx, eff->model_matrix);
	gf_mx_inverse(&mx);
	r = eff->ray;
	gf_mx_apply_ray(&mx, &r);
	if (!R3D_Get2DPlaneIntersection(&r, &pos)) return 0;
	if ( (pos.x < clip->x) || (pos.y > clip->y) 
		|| (pos.x > clip->x + clip->width) || (pos.y < clip->y - clip->height) ) return 0;
	return 1;
}


void R3D_InitShape(Render3D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderShape);
}

static Bool R3D_NoIntersectionWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *vec, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	return 0;
}

static void RenderBox(GF_Node *n, void *rs)
{
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = gf_node_get_private(n);
	M_Box *box = (M_Box *)n;

	if (gf_node_dirty_get(n)) {
		mesh_new_box(st->mesh, box->size);
		gf_node_dirty_clear(n, 0);
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitBox(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderBox);
}

static void RenderCone(GF_Node *n, void *rs)
{
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = gf_node_get_private(n);
	M_Cone *co = (M_Cone *)n;

	if (gf_node_dirty_get(n)) {
		mesh_new_cone(st->mesh, co->height, co->bottomRadius, co->bottom, co->side, eff->surface->render->compositor->high_speed);
		gf_node_dirty_clear(n, 0);
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitCone(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderCone);
}

static void RenderCylinder(GF_Node *n, void *rs)
{
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = gf_node_get_private(n);
	M_Cylinder *cy = (M_Cylinder *)n;

	if (gf_node_dirty_get(n) ) {
		mesh_new_cylinder(st->mesh, cy->height, cy->radius, cy->bottom, cy->side, cy->top, eff->surface->render->compositor->high_speed);
		gf_node_dirty_clear(n, 0);
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitCylinder(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderCylinder);
}

static void RenderSphere(GF_Node *n, void *rs)
{
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	DrawableStack *st = gf_node_get_private(n);
	M_Sphere *sp = (M_Sphere *)n;

	if (gf_node_dirty_get(n)) {
		mesh_new_sphere(st->mesh, sp->radius, eff->surface->render->compositor->high_speed);
		gf_node_dirty_clear(n, 0);
	}

	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitSphere(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderSphere);
}

static Bool CircleIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Fixed r, sqr;
	Bool inside;
	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;
	r = ((M_Circle *)owner)->radius;
	sqr = gf_mulfix(r, r);
	inside = (sqr >= gf_mulfix(outPoint->x, outPoint->x) + gf_mulfix(outPoint->y, outPoint->y)) ? 1 : 0;
	if (!inside) return 0;
	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = FIX_ONE; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, r) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, r) + FIX_ONE/2;
	}
	return 1;
}

static void RenderCircle(GF_Node *node, void *rs)
{
	stack2D *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		Fixed a = ((M_Circle *) node)->radius * 2;
		stack2D_reset(st);
		gf_path_add_ellipse(st->path, 0, 0, a, a);
		mesh_new_ellipse(st->mesh, a, a, eff->surface->render->compositor->high_speed);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		stack2D_draw(st, eff);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitCircle(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderCircle);
	st->IntersectWithRay = CircleIntersectWithRay;
}

static Bool EllipseIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Fixed a, b;
	Bool inside;
	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;
	a = ((M_Ellipse *)owner)->radius.x; a = gf_mulfix(a, a);
	b = ((M_Ellipse *)owner)->radius.y; b = gf_mulfix(b, b);
	inside = (gf_muldiv(outPoint->x, outPoint->x, a) + gf_muldiv(outPoint->y, outPoint->y, b) <= 1) ? 1 : 0;
	if (!inside) return 0;
	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = FIX_ONE; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, a) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, b) + FIX_ONE/2;
	}
	return 1;
}

static void RenderEllipse(GF_Node *node, void *rs)
{
	stack2D *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		Fixed a = ((M_Ellipse *) node)->radius.x;
		Fixed b = ((M_Ellipse *) node)->radius.y;
		stack2D_reset(st);
		gf_path_add_ellipse(st->path, 0, 0, a, b);
		mesh_new_ellipse(st->mesh, a, b, eff->surface->render->compositor->high_speed);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) 
		stack2D_draw(st, eff);
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitEllipse(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderEllipse);
	st->IntersectWithRay = EllipseIntersectWithRay;
}

static Bool RectangleIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	stack2D *st;
	Bool inside;
	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;
	st = (stack2D *) gf_node_get_private(owner);
	inside = ( (outPoint->x >= st->mesh->bounds.min_edge.x) && (outPoint->y >= st->mesh->bounds.min_edge.y) 
			&& (outPoint->x <= st->mesh->bounds.max_edge.x) && (outPoint->y <= st->mesh->bounds.max_edge.y) 
			) ? 1 : 0;

	if (!inside) return 0;
	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = FIX_ONE; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, ((M_Rectangle*)owner)->size.x) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, ((M_Rectangle*)owner)->size.y) + FIX_ONE/2;
	}
	return 1;
}

static void RenderRectangle(GF_Node *node, void *rs)
{
	stack2D *st = (stack2D *) gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	/*build vec path*/
	if (gf_node_dirty_get(node)) {
		SFVec2f s = ((M_Rectangle *) node)->size;
		stack2D_reset(st);
		gf_path_add_rect_center(st->path, 0, 0, s.x, s.y);
		mesh_new_rectangle(st->mesh, s);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) 
		stack2D_draw(st, eff);
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitRectangle(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderRectangle);
	st->IntersectWithRay = RectangleIntersectWithRay;
}


Bool Stack2DIntersectWithRay(GF_Node *owner, GF_Ray *r, SFVec3f *outPoint, SFVec3f *outNormal, SFVec2f *outTexCoords)
{
	Bool inside;
	stack2D *st;
	if (!R3D_Get2DPlaneIntersection(r, outPoint)) return 0;
	st = (stack2D *) gf_node_get_private(owner);

	if ( (outPoint->x < st->mesh->bounds.min_edge.x) || (outPoint->y < st->mesh->bounds.min_edge.y) 
			|| (outPoint->x > st->mesh->bounds.max_edge.x) || (outPoint->y > st->mesh->bounds.max_edge.y) ) return 0;

	inside = gf_path_point_over(st->path, outPoint->x, outPoint->y);
	if (!inside) return 0;
	if (outNormal) { outNormal->x = outNormal->y = 0; outNormal->z = FIX_ONE; }
	if (outTexCoords) { 
		outTexCoords->x = gf_divfix(outPoint->x, (st->mesh->bounds.max_edge.x - st->mesh->bounds.min_edge.x)) + FIX_ONE/2;
		outTexCoords->y = gf_divfix(outPoint->y, (st->mesh->bounds.max_edge.y - st->mesh->bounds.min_edge.y)) + FIX_ONE/2;
	}
	return 1;
}


//#define CHECK_VALID_C2D(nbPts) if (cur_index+nbPts>=pt_count) { gf_path_reset(st->path); return; }
#define CHECK_VALID_C2D(nbPts)

static void build_curve2D(stack2D *st, M_Curve2D *c2D)
{
	SFVec2f orig, ct_orig, ct_end, end;
	u32 cur_index, i, remain, type_count, pt_count;
	SFVec2f *pts;
	M_Coordinate2D *coord = (M_Coordinate2D *)c2D->point;

	pts = coord->point.vals;
	if (!pts) 
		return;

	cur_index = c2D->type.count ? 1 : 0;
	/*if the first type is a moveTo skip initial moveTo*/
	i=0;
	if (cur_index) {
		while (c2D->type.vals[i]==0) i++;
	}
	ct_orig = orig = pts[i];

	gf_path_add_move_to(st->path, orig.x, orig.y);

	pt_count = coord->point.count;
	type_count = c2D->type.count;
	for (; i<type_count; i++) {

		switch (c2D->type.vals[i]) {
		/*moveTo, 1 point*/
		case 0:
			CHECK_VALID_C2D(0);
			orig = pts[cur_index];
			if (i) gf_path_add_move_to(st->path, orig.x, orig.y);
			cur_index += 1;
			break;
		/*lineTo, 1 point*/
		case 1:
			CHECK_VALID_C2D(0);
			end = pts[cur_index];
			gf_path_add_line_to(st->path, end.x, end.y);
			orig = end;
			cur_index += 1;
			break;
		/*curveTo, 3 points*/
		case 2:
			CHECK_VALID_C2D(2);
			ct_orig = pts[cur_index];
			ct_end = pts[cur_index+1];
			end = pts[cur_index+2];
			gf_path_add_cubic_to(st->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*nextCurveTo, 2 points (cf spec)*/
		case 3:
			CHECK_VALID_C2D(1);
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			ct_end = pts[cur_index];
			end = pts[cur_index+1];
			gf_path_add_cubic_to(st->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;

		/*all XCurve2D specific*/

		/*CW and CCW ArcTo*/
		case 4:
		case 5:
			CHECK_VALID_C2D(2);
			ct_orig = pts[cur_index];
			ct_end = pts[cur_index+1];
			end = pts[cur_index+2];
			gf_path_add_arc_to(st->path, end.x, end.y, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, (c2D->type.vals[i]==5) ? 1 : 0);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*ClosePath*/
		case 6:
			gf_path_close(st->path);
			break;
		/*quadratic CurveTo, 2 points*/
		case 7:
			CHECK_VALID_C2D(1);
			ct_end = pts[cur_index];
			end = pts[cur_index+1];
			gf_path_add_quadratic_to(st->path, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;
		}
	}

	/*what's left is an N-bezier spline*/
	if (pt_count > cur_index) {
		/*first moveto*/
		if (!cur_index) cur_index++;

		remain = pt_count - cur_index;

		if (remain>1)
			gf_path_add_bezier(st->path, &pts[cur_index], remain);
	}
}

static void RenderCurve2D(GF_Node *node, void *rs)
{
	M_Curve2D *c2D = (M_Curve2D *)node;
	stack2D *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (!c2D->point) return;

	if (gf_node_dirty_get(node)) {
		stack2D_reset(st);
		st->path->fineness = c2D->fineness;
		if (st->compositor->high_speed) st->path->fineness /= 2;
		build_curve2D(st, c2D);
		mesh_from_path(st->mesh, st->path);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) 
		stack2D_draw(st, eff);
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		eff->bbox = st->mesh->bounds;
}

void R3D_InitCurve2D(Render3D *sr, GF_Node *node)
{
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderCurve2D);
	st->IntersectWithRay = Stack2DIntersectWithRay;
}

static void build_ils2d(stack2D *st, M_IndexedLineSet2D *ils2D)
{
	u32 i;
	Bool started;
	SFVec2f *pts;
	M_Coordinate2D *coord = (M_Coordinate2D *)ils2D->coord;

	pts = coord->point.vals;
	if (ils2D->coordIndex.count > 0) {
		started = 0;
		for (i=0; i < ils2D->coordIndex.count; i++) {
			/*NO close on ILS2D*/
			if (ils2D->coordIndex.vals[i] == -1) {
				started = 0;
			} else if (!started) {
				started = 1;
				gf_path_add_move_to(st->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			} else {
				gf_path_add_line_to(st->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			}
		}
	} else if (coord->point.count) {
		gf_path_add_move_to(st->path, pts[0].x, pts[0].y);
		for (i=1; i < coord->point.count; i++) {
			gf_path_add_line_to(st->path, pts[i].x, pts[i].y);
		}
	}
}

static void RenderILS2D(GF_Node *node, void *rs)
{
	Aspect2D asp;
	StrikeInfo *si;
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	stack2D *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		stack2D_reset(st);
		build_ils2d(st, ils2D);
		mesh_new_ils(st->mesh, ils2D->coord, &ils2D->coordIndex, ils2D->color, &ils2D->colorIndex, ils2D->colorPerVertex, 0);
		gf_node_dirty_clear(node, 0);
	}


	if (!eff->traversing_mode) {
		VS_GetAspect2D(eff, &asp);

		VS3D_SetAntiAlias(eff->surface, (eff->surface->render->compositor->antiAlias==GF_ANTIALIAS_FULL) ? 1 : 0);
		if (ils2D->color) {
			VS3D_StrikeMesh(eff, st->mesh, Aspect_GetLineWidth(&asp), asp.pen_props.dash);
		} else {
			si = VS_GetStrikeInfo(st, &asp, eff);
			if (si) {
				VS_Set2DStrikeAspect(eff->surface, &asp);
				if (!si->is_vectorial) {
					VS3D_StrikeMesh(eff, si->outline, Aspect_GetLineWidth(&asp), asp.pen_props.dash);
				} else {
					VS3D_DrawMesh(eff, si->outline);
				}
			}
		}
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
	
}

static void ILS2D_SetColorIndex(GF_Node *node)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	gf_sg_vrml_field_copy(&ils2D->colorIndex, &ils2D->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils2D->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void ILS2D_SetCoordIndex(GF_Node *node)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	gf_sg_vrml_field_copy(&ils2D->coordIndex, &ils2D->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils2D->set_coordIndex, GF_SG_VRML_MFINT32);
}

void R3D_InitILS2D(Render3D *sr, GF_Node *node)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderILS2D);
	ils2D->on_set_colorIndex = ILS2D_SetColorIndex;
	ils2D->on_set_coordIndex = ILS2D_SetCoordIndex;
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderPointSet2D(GF_Node *node, void *rs)
{
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;
	if (!ps2D->coord) return;

	if (gf_node_dirty_get(node)) {
		mesh_new_ps(st->mesh, ps2D->coord, ps2D->color);
		gf_node_dirty_clear(node, 0);
	}
	
	if (!eff->traversing_mode) {
		Aspect2D asp;
		VS_GetAspect2D(eff, &asp);
		VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);
		VS3D_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}


void R3D_InitPointSet2D(Render3D *sr, GF_Node *node)
{
	DrawableStack *st = BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderPointSet2D);
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void build_ifs2d(stack2D *st, M_IndexedFaceSet2D *ifs2D)
{
	u32 i;
	SFVec2f *pts;
	u32 ci_count, c_count;
	Bool started;
	M_Coordinate2D *coord = (M_Coordinate2D *)ifs2D->coord;

	c_count = coord->point.count;
	ci_count = ifs2D->coordIndex.count;
	pts = coord->point.vals;

	if (ci_count > 0) {
		started = 0;
		for (i=0; i < ci_count; i++) {
			if (ifs2D->coordIndex.vals[i] == -1) {
				gf_path_close(st->path);
				started = 0;
			} else if (!started) {
				started = 1;
				gf_path_add_move_to(st->path, pts[ifs2D->coordIndex.vals[i]].x, pts[ifs2D->coordIndex.vals[i]].y);
			} else {
				gf_path_add_line_to(st->path, pts[ifs2D->coordIndex.vals[i]].x, pts[ifs2D->coordIndex.vals[i]].y);
			}
		}
		if (started) gf_path_close(st->path);
	} else if (c_count) {
		gf_path_add_move_to(st->path, pts[0].x, pts[0].y);
		for (i=1; i < c_count; i++) {
			gf_path_add_line_to(st->path, pts[i].x, pts[i].y);
		}
		gf_path_close(st->path);
	}
}

static void RenderIFS2D(GF_Node *node, void *rs)
{
	Aspect2D asp;
	StrikeInfo *si;
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	stack2D *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		stack2D_reset(st);
		build_ifs2d(st, ifs2D);
		mesh_new_ifs2d(st->mesh, node);
		gf_node_dirty_clear(node, 0);
	}

	if (!eff->traversing_mode) {
		VS_GetAspect2D(eff, &asp);
		if (ifs2D->color && !asp.filled) {
			/*use special func to disable outline recompute and handle recompute ourselves*/
			si = VS_GetStrikeInfoIFS(st, &asp, eff);
			if (!si->outline) {
				si->outline = new_mesh();
				mesh_new_ils(si->outline, ifs2D->coord, &ifs2D->coordIndex, ifs2D->color, &ifs2D->colorIndex, ifs2D->colorPerVertex, 1);
			}
			VS3D_StrikeMesh(eff, si->outline, Aspect_GetLineWidth(&asp), asp.pen_props.dash);
		} else {
			stack2D_draw(st, eff);
		}
	} 
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void IFS2D_SetColorIndex(GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	gf_sg_vrml_field_copy(&ifs2D->colorIndex, &ifs2D->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs2D->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void IFS2D_SetCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	gf_sg_vrml_field_copy(&ifs2D->coordIndex, &ifs2D->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs2D->set_coordIndex, GF_SG_VRML_MFINT32);
}

static void IFS2D_SetTexCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	gf_sg_vrml_field_copy(&ifs2D->texCoordIndex, &ifs2D->set_texCoordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs2D->set_texCoordIndex, GF_SG_VRML_MFINT32);
}

void R3D_InitIFS2D(Render3D *sr, GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	stack2D *st = BaseStack2D(sr->compositor, node);
	gf_node_set_render_function(node, RenderIFS2D);
	ifs2D->on_set_colorIndex = IFS2D_SetColorIndex;
	ifs2D->on_set_coordIndex = IFS2D_SetCoordIndex;
	ifs2D->on_set_texCoordIndex = IFS2D_SetTexCoordIndex;
	st->IntersectWithRay = Stack2DIntersectWithRay;
}

static void RenderPointSet(GF_Node *node, void *rs)
{
	M_PointSet *ps = (M_PointSet *)node;
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;
	if (!ps->coord) return;

	if (gf_node_dirty_get(node)) {
		mesh_new_ps(st->mesh, ps->coord, ps->color);
		gf_node_dirty_clear(node, 0);
	}
	
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	}
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

void R3D_InitPointSet(Render3D *sr, GF_Node *node)
{
	DrawableStack *st = BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderPointSet);
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderIFS(GF_Node *node, void *rs)
{
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		mesh_new_ifs(st->mesh, node);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void IFS_SetColorIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->colorIndex, &ifs->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->coordIndex, &ifs->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_coordIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetNormalIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->normalIndex, &ifs->set_normalIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_normalIndex, GF_SG_VRML_MFINT32);
}

static void IFS_SetTexCoordIndex(GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	gf_sg_vrml_field_copy(&ifs->texCoordIndex, &ifs->set_texCoordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ifs->set_texCoordIndex, GF_SG_VRML_MFINT32);
}

void R3D_InitIFS(Render3D *sr, GF_Node *node)
{
	M_IndexedFaceSet *ifs = (M_IndexedFaceSet *)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderIFS);
	ifs->on_set_colorIndex = IFS_SetColorIndex;
	ifs->on_set_coordIndex = IFS_SetCoordIndex;
	ifs->on_set_normalIndex = IFS_SetNormalIndex;
	ifs->on_set_texCoordIndex = IFS_SetTexCoordIndex;
}

static void RenderILS(GF_Node *node, void *rs)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (!ils->coord) return;

	if (gf_node_dirty_get(node)) {
		mesh_new_ils(st->mesh, ils->coord, &ils->coordIndex, ils->color, &ils->colorIndex, ils->colorPerVertex, 0);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS3D_SetAntiAlias(eff->surface, (eff->surface->render->compositor->antiAlias==GF_ANTIALIAS_FULL) ? 1 : 0);
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void ILS_SetColorIndex(GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	gf_sg_vrml_field_copy(&ils->colorIndex, &ils->set_colorIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils->set_colorIndex, GF_SG_VRML_MFINT32);
}

static void ILS_SetCoordIndex(GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	gf_sg_vrml_field_copy(&ils->coordIndex, &ils->set_coordIndex, GF_SG_VRML_MFINT32);
	gf_sg_vrml_mf_reset(&ils->set_coordIndex, GF_SG_VRML_MFINT32);
}

void R3D_InitILS(Render3D *sr, GF_Node *node)
{
	M_IndexedLineSet *ils = (M_IndexedLineSet *)node;
	DrawableStack *st = BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderILS);
	ils->on_set_colorIndex = ILS_SetColorIndex;
	ils->on_set_coordIndex = ILS_SetCoordIndex;
	st->IntersectWithRay = R3D_NoIntersectionWithRay;
}

static void RenderElevationGrid(GF_Node *node, void *rs)
{
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		mesh_new_elevation_grid(st->mesh, node);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void ElevationGrid_SetHeight(GF_Node *node)
{
	M_ElevationGrid *eg = (M_ElevationGrid *)node;
	gf_sg_vrml_field_copy(&eg->height, &eg->set_height, GF_SG_VRML_MFFLOAT);
	gf_sg_vrml_mf_reset(&eg->set_height, GF_SG_VRML_MFFLOAT);
}

void R3D_InitElevationGrid(Render3D *sr, GF_Node *node)
{
	M_ElevationGrid *eg = (M_ElevationGrid *)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderElevationGrid);
	eg->on_set_height = ElevationGrid_SetHeight;
}


static void RenderExtrusion(GF_Node *node, void *rs)
{
	DrawableStack *st = gf_node_get_private(node);
	RenderEffect3D *eff = rs;

	if (gf_node_dirty_get(node)) {
		mesh_new_extrusion(st->mesh, node);
		gf_node_dirty_clear(node, 0);
	}
	if (!eff->traversing_mode) {
		VS_DrawMesh(eff, st->mesh);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
}

static void Extrusion_SetCrossSection(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->crossSection, &eg->set_crossSection, GF_SG_VRML_MFVEC2F);
	gf_sg_vrml_mf_reset(&eg->set_crossSection, GF_SG_VRML_MFVEC2F);
}
static void Extrusion_SetOrientation(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->orientation, &eg->set_orientation, GF_SG_VRML_MFROTATION);
	gf_sg_vrml_mf_reset(&eg->set_orientation, GF_SG_VRML_MFROTATION);
}
static void Extrusion_SetScale(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->scale, &eg->set_scale, GF_SG_VRML_MFVEC2F);
	gf_sg_vrml_mf_reset(&eg->set_scale, GF_SG_VRML_MFVEC2F);
}
static void Extrusion_SetSpine(GF_Node *node)
{
	M_Extrusion *eg = (M_Extrusion *)node;
	gf_sg_vrml_field_copy(&eg->spine, &eg->set_spine, GF_SG_VRML_MFVEC3F);
	gf_sg_vrml_mf_reset(&eg->set_spine, GF_SG_VRML_MFVEC3F);
}
void R3D_InitExtrusion(Render3D *sr, GF_Node *node)
{
	M_Extrusion *ext = (M_Extrusion *)node;
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderExtrusion);
	ext->on_set_crossSection = Extrusion_SetCrossSection;
	ext->on_set_orientation = Extrusion_SetOrientation;
	ext->on_set_scale = Extrusion_SetScale;
	ext->on_set_spine = Extrusion_SetSpine;
}


/*
			NonLinearDeformer

	NOTE: AFX spec is just hmm, well, hmm. NonLinearDeformer.extend interpretation differ from type to 
	type within the spec, and between the spec and the ref soft. This is GPAC interpretation
	* all params are specified with default transform axis (Z axis)
	* NLD.type = 0 (taper):
		* taping radius = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along taper axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base taper radius
		extend works like key/keyValue for a scalar interpolator
		final radius: r(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

	* NLD.type = 1 (twister):
		* twisting angle = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along twister axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base twister angle
		extend works like key/keyValue for a scalar interpolator
		final angle: a(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

	* NLD.type = 2 (bender):
		* bending curvature = NLD.param
		* extend = N * [diff, perc] with 
			- diff : relative position along bender axis (for default, diff=0: z min, diff=1: z max)
			- perc: mult ratio for base bender curvature
		extend works like key/keyValue for a scalar interpolator
		final curvature: c(z) = LinearInterp(extend[min key], extend[min key + 1]) * param

  Another pb of NLD is that the spec says nothing about object/axis alignment: should we center
  the object at 0,0,0 (local coords) or not? the results are quite different. Here we don't 
  recenter the object before transform
*/

static Bool NLD_GetMatrix(M_NonLinearDeformer *nld, GF_Matrix *mx)
{
	SFVec3f v1, v2;
	SFRotation r;
	Fixed l1, l2, dot;

	/*compute rotation matrix from NLD axis to 0 0 1*/
	v1 = nld->axis;
	gf_vec_norm(&v1);
	v2.x = v2.y = 0; v2.z = FIX_ONE;
	if (gf_vec_equal(v1, v2)) return 0;

	l1 = gf_vec_len(v1);
	l2 = gf_vec_len(v2);
	dot = gf_divfix(gf_vec_dot(v1, v2), gf_mulfix(l1, l2));

	r.x = gf_mulfix(v1.y, v2.z) - gf_mulfix(v2.y, v1.z);
	r.y = gf_mulfix(v1.z, v2.x) - gf_mulfix(v2.z, v1.x);
	r.z = gf_mulfix(v1.x, v2.y) - gf_mulfix(v2.x, v1.y);
	r.q = gf_atan2(gf_sqrt(FIX_ONE - gf_mulfix(dot, dot)), dot);
	gf_mx_init(*mx);
	gf_mx_add_rotation(mx, r.q, r.x, r.y, r.z);
	return 1;
}

static GFINLINE void NLD_GetKey(M_NonLinearDeformer *nld, Fixed frac, Fixed *f_min, Fixed *min, Fixed *f_max, Fixed *max)
{
	u32 i, count;
	count = nld->extend.count;
	if (count%2) count--;

	*f_min = 0;
	*min = 0;
	for (i=0; i<count; i+=2) {
		if (frac>=nld->extend.vals[i]) {
			*f_min = nld->extend.vals[i];
			*min = nld->extend.vals[i+1];
		} 
		if ((i+2<count) && (frac<=nld->extend.vals[i+2])) {
			*f_max = nld->extend.vals[i+2];
			*max = nld->extend.vals[i+3];
			return;
		}
	}
	if (count) {
		*f_max = nld->extend.vals[count-2];
		*max = nld->extend.vals[count-1];
	} else {
		*f_max = FIX_ONE;
		*max = GF_PI;
	}
}

static void NLD_Apply(M_NonLinearDeformer *nld, GF_Mesh *mesh)
{
	u32 i;
	GF_Matrix mx;
	Fixed param, z_min, z_max, v_min, v_max, f_min, f_max, frac, val, a_cos, a_sin;
	Bool needs_transform = NLD_GetMatrix(nld, &mx);

	param = nld->param;
	if (!param) param = 1;
	
	if (mesh->bounds.min_edge.z == mesh->bounds.max_edge.z) return;

	z_min = FIX_MAX;
	z_max = -FIX_MAX;
	if (needs_transform) {
		for (i=0; i<mesh->v_count; i++) {
			gf_mx_apply_vec(&mx, &mesh->vertices[i].pos);
			gf_mx_rotate_vector(&mx, &mesh->vertices[i].normal);
			if (mesh->vertices[i].pos.z<z_min) z_min = mesh->vertices[i].pos.z;
			if (mesh->vertices[i].pos.z>z_max) z_max = mesh->vertices[i].pos.z;
		}
	} else {
		z_min = mesh->bounds.min_edge.z;
		z_max = mesh->bounds.max_edge.z;
	}
	
	for (i=0; i<mesh->v_count; i++) {
		SFVec3f old = mesh->vertices[i].pos;
		frac = gf_divfix(old.z - z_min, z_max - z_min);
		NLD_GetKey(nld, frac, &f_min, &v_min, &f_max, &v_max);
		if (f_max == f_min) {
			val = v_min;
		} else {
			frac = gf_divfix(frac-f_min, f_max - f_min);
			val = gf_mulfix(v_max - v_min, frac) + v_min;
		}
		val = gf_mulfix(val, param);

		switch (nld->type) {
		/*taper*/
		case 0:
			mesh->vertices[i].pos.x = gf_mulfix(mesh->vertices[i].pos.x, val);
			mesh->vertices[i].pos.y = gf_mulfix(mesh->vertices[i].pos.y, val);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(mesh->vertices[i].normal.x, val);
			mesh->vertices[i].normal.y = gf_mulfix(mesh->vertices[i].normal.y, val);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*twist*/
		case 1:
			a_cos = gf_cos(val);
			a_sin = gf_sin(val);
			mesh->vertices[i].pos.x = gf_mulfix(a_cos, old.x) - gf_mulfix(a_sin, old.y);
			mesh->vertices[i].pos.y = gf_mulfix(a_sin, old.x) + gf_mulfix(a_cos, old.y);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(a_cos, old.x) -  gf_mulfix(a_sin, old.y);
			mesh->vertices[i].normal.y = gf_mulfix(a_sin, old.x) + gf_mulfix(a_cos, old.y);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*bend*/
		case 2:
			a_cos = gf_cos(val);
			a_sin = gf_sin(val);
			mesh->vertices[i].pos.x = gf_mulfix(a_sin, old.z) + gf_mulfix(a_cos, old.x);
			mesh->vertices[i].pos.z = gf_mulfix(a_cos, old.z) - gf_mulfix(a_sin, old.x);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(a_sin, old.z) +  gf_mulfix(a_cos, old.x);
			mesh->vertices[i].normal.z = gf_mulfix(a_cos, old.z) - gf_mulfix(a_sin, old.x);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		/*pinch, not standard  (taper on X dim only)*/
		case 3:
			mesh->vertices[i].pos.x = gf_mulfix(mesh->vertices[i].pos.x, val);
			old = mesh->vertices[i].normal;
			mesh->vertices[i].normal.x = gf_mulfix(mesh->vertices[i].normal.x, val);
			gf_vec_norm(&mesh->vertices[i].normal);
			break;
		}
	}
	if (needs_transform) {
		gf_mx_inverse(&mx);
		for (i=0; i<mesh->v_count; i++) {
			gf_mx_apply_vec(&mx, &mesh->vertices[i].pos);
			gf_mx_rotate_vector(&mx, &mesh->vertices[i].normal);
		}
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void RenderNonLinearDeformer(GF_Node *n, void *rs)
{
	DrawableStack *st = (DrawableStack *) gf_node_get_private(n);
	M_NonLinearDeformer *nld = (M_NonLinearDeformer*)n;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (!nld->geometry) return;

	/*call render for get_bounds to make sure geometry is up to date*/
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		gf_node_render(nld->geometry, eff);
	}
	/*invalidated*/
	if (gf_node_dirty_get(n)) {
		DrawableStack *geo_st = gf_node_get_private(nld->geometry);
		if (!geo_st) return;
		gf_node_dirty_clear(n, 0);
		mesh_clone(st->mesh, geo_st->mesh);
		/*apply deforms*/
		NLD_Apply(nld, st->mesh);
	}
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->mesh->bounds;
	}
	/*in case there's a cascading of NLDs*/
	else if (!eff->traversing_mode && eff->appear) {
		VS_DrawMesh(eff, st->mesh);
	}
}

void R3D_InitNonLinearDeformer(Render3D *sr, GF_Node *node)
{
	BaseDrawableStack(sr->compositor, node);
	gf_node_set_render_function(node, RenderNonLinearDeformer);
}
