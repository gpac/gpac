/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

Bool compositor_get_2d_plane_intersection(GF_Ray *ray, SFVec3f *res)
{
	GF_Plane p;
	Fixed t, t2;
	if (!ray->dir.x && !ray->dir.y) {
		res->x = ray->orig.x;
		res->y = ray->orig.y;
		return 1;
	}
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

/*
		Shape 
*/
static void TraverseShape(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state;
	M_Shape *shape;
	if (is_destroy ) return;

	tr_state = (GF_TraverseState *)rs;
#ifndef GPAC_DISABLE_3D
	if (tr_state->traversing_mode==TRAVERSE_LIGHTING) return;
#endif

	shape = (M_Shape *) node;
	if (!shape->geometry) return;

	/*reset this node dirty flag (because bitmap may trigger bounds invalidation on the fly)*/
	gf_node_dirty_clear(node, 0);

	
	/*check traverse mode, and take care of switch-off flag*/
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		GF_Node *m;
		tr_state->appear = (GF_Node *) shape->appearance;
		
		/*this is done regardless of switch flag*/
		gf_node_traverse((GF_Node *) shape->geometry, tr_state);

		if (tr_state->appear) {
			/*apply line width*/
			m = ((M_Appearance *)tr_state->appear)->material;
			if (m && (gf_node_get_tag(m)==TAG_MPEG4_Material2D) ) {
				DrawAspect2D asp;
				Fixed width = 0;
				asp.line_scale = FIX_ONE;
				m = ((M_Material2D *)m)->lineProps;
				if (m) {
					switch (gf_node_get_tag(m)) {
					case TAG_MPEG4_LineProperties:
						width = ((M_LineProperties *) m)->width;
						drawable_compute_line_scale(tr_state, &asp);
						break;
					case TAG_MPEG4_XLineProperties:
						if ( ((M_XLineProperties *) m)->isCenterAligned)
							width = ((M_XLineProperties *) m)->width;
						if ( ((M_XLineProperties *) m)->isScalable)
							drawable_compute_line_scale(tr_state, &asp);
						break;
					}
					width = gf_mulfix(width, asp.line_scale);
					tr_state->bounds.width += width;
					tr_state->bounds.height += width;
					tr_state->bounds.y += width/2;
					tr_state->bounds.x -= width/2;
				}
			}
			tr_state->appear = NULL;
		}
	} else {
		if (tr_state->switched_off) return;

		tr_state->appear = (GF_Node *) shape->appearance;

		switch (tr_state->traversing_mode) {
		case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
			if (tr_state->visual->type_3d)
				visual_3d_register_context(tr_state, shape->geometry);
			else
#endif
				gf_node_traverse((GF_Node *) shape->geometry, tr_state);
			break;
		case TRAVERSE_PICK:
			gf_node_traverse((GF_Node *) shape->geometry, tr_state);
			break;
#ifndef GPAC_DISABLE_3D
		/*if we're here we passed culler already*/
		case TRAVERSE_DRAW_3D: 
			gf_node_traverse((GF_Node *) shape->geometry, tr_state);
			break;
		case TRAVERSE_COLLIDE:
			visual_3d_drawable_collide(shape->geometry, tr_state);
			break;
#endif
		}

		tr_state->appear = NULL;
	}
}

void compositor_init_shape(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, TraverseShape);
}

static void circle_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		Fixed a = ((M_Circle *) node)->radius * 2;
		drawable_reset_path(stack);
		gf_path_add_ellipse(stack->path, 0, 0, a, a);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}
static void TraverseCircle(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	circle_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			Fixed a = ((M_Circle *) node)->radius * 2;
			stack->mesh = new_mesh();
			mesh_new_ellipse(stack->mesh, a, a, tr_state->visual->compositor->high_speed);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_PICK:
		drawable_pick(stack, tr_state);
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

void compositor_init_circle(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseCircle);
}

static void ellipse_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		drawable_reset_path(stack);
		gf_path_add_ellipse(stack->path, 0, 0, ((M_Ellipse *) node)->radius.x*2, ((M_Ellipse *) node)->radius.y*2);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}
static void TraverseEllipse(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	ellipse_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_new_ellipse(stack->mesh, ((M_Ellipse *) node)->radius.x * 2, ((M_Ellipse *) node)->radius.y * 2, tr_state->visual->compositor->high_speed);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_PICK:
		drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		break;
	}
}

void compositor_init_ellipse(GF_Compositor  *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseEllipse);
}

static void compositor_2d_draw_rectangle(GF_TraverseState *tr_state)
{
	DrawableContext *ctx = tr_state->ctx;

	if (ctx->aspect.fill_texture) {
		Bool res;

		/*get image size WITHOUT line size*/
		if (ctx->aspect.pen_props.width) {
			GF_Rect orig_unclip;
			GF_IRect orig_clip;
			orig_unclip = ctx->bi->unclip;
			orig_clip = ctx->bi->clip;

			gf_path_get_bounds(ctx->drawable->path, &ctx->bi->unclip);
			gf_mx2d_apply_rect(&ctx->transform, &ctx->bi->unclip);
			ctx->bi->clip = gf_rect_pixelize(&ctx->bi->unclip);
			gf_irect_intersect(&ctx->bi->clip, &orig_clip);

			res = tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx, NULL);

			/*strike path*/
			ctx->bi->unclip = orig_unclip;
			ctx->bi->clip = orig_clip;
			if (res) {
				ctx->flags |= CTX_PATH_FILLED;
				visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
			}
		} else {
			res = tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx, NULL);
		}
		/*if failure retry with raster*/
		if (res) return;
	} 

	visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
	visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
}

static void rectangle_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	/*if modified update node - we don't update for other traversing mode in order not to mess up the dirty
	rect tracking (otherwise we would miss geometry changes with same bounds)*/
	if (gf_node_dirty_get(node)) {
		drawable_reset_path(stack);
		gf_path_add_rect_center(stack->path, 0, 0, ((M_Rectangle *) node)->size.x, ((M_Rectangle *) node)->size.y);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack, tr_state);
	}
}
static void TraverseRectangle(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	rectangle_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		compositor_2d_draw_rectangle(tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_new_rectangle(stack->mesh, ((M_Rectangle *) node)->size);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_PICK:
		drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		break;
	default:
		return;
	}

	ctx = drawable_init_context_mpeg4(stack, tr_state);
	if (!ctx) return;

	/*if alpha or not filled, transparent*/
	if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {
	} 
	/*if texture transparent, transparent*/
	else if (ctx->aspect.fill_texture && ctx->aspect.fill_texture->transparent) {
	} 
	/*if rotated, transparent (doesn't fill bounds)*/
	else if (ctx->transform.m[1] || ctx->transform.m[3]) {
	}
	/*TODO check matrix for alpha*/
	else if (!tr_state->color_mat.identity) {
	}
	/*otherwsie, not transparent*/
	else {
		ctx->flags &= ~CTX_IS_TRANSPARENT;	
	}
	drawable_finalize_sort(ctx, tr_state, NULL);
}

void compositor_init_rectangle(GF_Compositor  *compositor, GF_Node *node)
{
	Drawable *stack = drawable_stack_new(compositor, node);
	stack->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	gf_node_set_callback_function(node, TraverseRectangle);
}


#define CHECK_VALID_C2D(nbPts) if (!idx && cur_index+nbPts>=pt_count) { gf_path_reset(stack->path); return; }
//#define CHECK_VALID_C2D(nbPts)
#define GET_IDX(_i)	((idx && idx->count>_i) ? idx->vals[_i] : _i)

void curve2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state, MFInt32 *idx)
{
	M_Curve2D *c2D;
	SFVec2f orig, ct_orig, ct_end, end;
	u32 cur_index, i, remain, type_count, pt_count;
	SFVec2f *pts;
	M_Coordinate2D *coord;

	c2D = (M_Curve2D *)node;
	coord = (M_Coordinate2D *)c2D->point;
	drawable_reset_path(stack);
	if (!coord) return;

	stack->path->fineness = c2D->fineness;
	if (tr_state->visual->compositor->high_speed)  stack->path->fineness /= 2;


	pts = coord->point.vals;
	if (!pts) 
		return;

	cur_index = c2D->type.count ? 1 : 0;
	/*if the first type is a moveTo skip initial moveTo*/
	i=0;
	if (cur_index) {
		while (c2D->type.vals[i]==0) i++;
	}
	ct_orig = orig = pts[ GET_IDX(i) ];

	gf_path_add_move_to(stack->path, orig.x, orig.y);

	pt_count = coord->point.count;
	type_count = c2D->type.count;
	for (; i<type_count; i++) {

		switch (c2D->type.vals[i]) {
		/*moveTo, 1 point*/
		case 0:
			CHECK_VALID_C2D(0);
			orig = pts[ GET_IDX(cur_index) ];
			if (i) gf_path_add_move_to(stack->path, orig.x, orig.y);
			cur_index += 1;
			break;
		/*lineTo, 1 point*/
		case 1:
			CHECK_VALID_C2D(0);
			end = pts[ GET_IDX(cur_index) ];
			gf_path_add_line_to(stack->path, end.x, end.y);
			orig = end;
			cur_index += 1;
			break;
		/*curveTo, 3 points*/
		case 2:
			CHECK_VALID_C2D(2);
			ct_orig = pts[ GET_IDX(cur_index) ];
			ct_end = pts[ GET_IDX(cur_index+1) ];
			end = pts[ GET_IDX(cur_index+2) ];
			gf_path_add_cubic_to(stack->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*nextCurveTo, 2 points (cf spec)*/
		case 3:
			CHECK_VALID_C2D(1);
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			ct_end = pts[ GET_IDX(cur_index) ];
			end = pts[ GET_IDX(cur_index+1) ];
			gf_path_add_cubic_to(stack->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;
		
		/*all XCurve2D specific*/

		/*CW and CCW ArcTo*/
		case 4:
		case 5:
			CHECK_VALID_C2D(2);
			ct_orig = pts[ GET_IDX(cur_index) ];
			ct_end = pts[ GET_IDX(cur_index+1) ];
			end = pts[ GET_IDX(cur_index+2) ];
			gf_path_add_arc_to(stack->path, end.x, end.y, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, (c2D->type.vals[i]==5) ? 1 : 0);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*ClosePath*/
		case 6:
			gf_path_close(stack->path);
			break;
		/*quadratic CurveTo, 2 points*/
		case 7:
			CHECK_VALID_C2D(1);
			ct_end = pts[ GET_IDX(cur_index) ];
			end = pts[ GET_IDX(cur_index+1) ];
			gf_path_add_quadratic_to(stack->path, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;
		}
	}

	/*what's left is an N-bezier spline*/
	if (!idx && (pt_count > cur_index) ) {
		/*first moveto*/
		if (!cur_index) cur_index++;

		remain = pt_count - cur_index;

		if (remain>1)
			gf_path_add_bezier(stack->path, &pts[cur_index], remain);
	}
	
	gf_node_dirty_clear(node, 0);
	drawable_mark_modified(stack, tr_state);
}

static void TraverseCurve2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}
	if (gf_node_dirty_get(node)) {
		curve2d_check_changes(node, stack, tr_state, NULL);
	}

	switch (tr_state->traversing_mode) {
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_from_path(stack->mesh, stack->path);
		}
		visual_3d_draw_2d(stack, tr_state);
		return;
#endif
	case TRAVERSE_PICK:
		drawable_pick(stack, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
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

void compositor_init_curve2d(GF_Compositor  *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, TraverseCurve2D);
}



/*
	Note on point set 2D: this is a very bad node and should be avoided in DEF/USE, since the size 
	of the rectangle representing the pixel shall always be 1 pixel w/h, therefore
	the path object is likely not the same depending on transformation context...

*/

static void get_point_size(GF_Matrix2D *mat, Fixed *w, Fixed *h)
{
	GF_Point2D pt;
	pt.x = mat->m[0] + mat->m[1];
	pt.y = mat->m[3] + mat->m[4];
	*w = *h = gf_divfix(FLT2FIX(1.41421356f) , gf_v2d_len(&pt));
}

static void pointset2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	u32 i;
	Fixed w, h;
	M_Coordinate2D *coord;

	if (!gf_node_dirty_get(node)) return;
	coord = (M_Coordinate2D *) ((M_PointSet2D *)node)->coord;

	drawable_reset_path(stack);

	get_point_size(&tr_state->transform, &w, &h);
	/*for PS2D don't add to avoid too  much antialiasing, just try to fill the given pixel*/
	for (i=0; i < coord->point.count; i++) 
		gf_path_add_rect(stack->path, coord->point.vals[i].x, coord->point.vals[i].y, w, h);

	stack->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
	
	gf_node_dirty_clear(node, 0);
	drawable_mark_modified(stack, tr_state);
}

static void PointSet2D_Draw(GF_Node *node, GF_TraverseState *tr_state)
{
	GF_Path *path;
	Fixed alpha, w, h;
	u32 i;
	SFColor col;
	DrawableContext *ctx = tr_state->ctx;
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ps2D->coord;
	M_Color *color = (M_Color *) ps2D->color;

	/*never outline PS2D*/
	ctx->flags |= CTX_PATH_STROKE;
	if (!color || color->color.count<coord->point.count) {
		/*no texturing*/
		visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
		return;
	}

	get_point_size(&ctx->transform, &w, &h);

	path = gf_path_new();
	alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
	for (i = 0; i < coord->point.count; i++) {
		col = color->color.vals[i];
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
		gf_path_add_rect_center(path, coord->point.vals[i].x, coord->point.vals[i].y, w, h);
		visual_2d_draw_path(tr_state->visual, path, ctx, NULL, NULL, tr_state);
		gf_path_reset(path);
		ctx->flags &= ~CTX_PATH_FILLED;
	}
	gf_path_del(path);
}

static void TraversePointSet2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	
	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	if (!ps2D->coord) return;

	pointset2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		PointSet2D_Draw(node, tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
	{
		DrawAspect2D asp;
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_new_ps(stack->mesh, ps2D->coord, ps2D->color);
		}
		memset(&asp, 0, sizeof(DrawAspect2D));
		drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);
		visual_3d_set_material_2d_argb(tr_state->visual, asp.fill_color);
		visual_3d_mesh_paint(tr_state, stack->mesh);
		return;
	} 
#endif
	case TRAVERSE_GET_BOUNDS:
		gf_path_get_bounds(stack->path, &tr_state->bounds);
		return;
	case TRAVERSE_PICK:
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		ctx = drawable_init_context_mpeg4(stack, tr_state);
		if (!ctx) return;
		drawable_finalize_sort(ctx, tr_state, NULL);
		break;
	default:
		return;
	}
}


void compositor_init_pointset2d(GF_Compositor  *compositor, GF_Node *node)
{
	Drawable *stack = drawable_stack_new(compositor, node);
	stack->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	gf_node_set_callback_function(node, TraversePointSet2D);
}
