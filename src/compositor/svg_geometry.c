/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
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

#include "visual_manager.h"
#include "nodes_stacks.h"

#if !defined(GPAC_DISABLE_COMPOSITOR)

#ifdef GPAC_DISABLE_SVG

Bool svg_drawable_is_over(Drawable *drawable, Fixed x, Fixed y, DrawAspect2D *asp, GF_TraverseState *tr_state, GF_Rect *glyph_rc)
{
	return 0;
}

#else

Bool svg_drawable_is_over(Drawable *drawable, Fixed x, Fixed y, DrawAspect2D *asp, GF_TraverseState *tr_state, GF_Rect *glyph_rc)
{
	u32 check_fill, check_stroke;
	Bool check_over, check_outline, check_vis, inside;
	GF_Rect rc;
	u8 ptr_evt;

	ptr_evt = *tr_state->svg_props->pointer_events;

	if (ptr_evt==SVG_POINTEREVENTS_NONE) {
		return 0;
	}

	if (glyph_rc) {
		rc = *glyph_rc;
	} else {
		gf_path_get_bounds(drawable->path, &rc);
	}
	inside = ( (x >= rc.x) && (y <= rc.y) && (x <= rc.x + rc.width) && (y >= rc.y - rc.height) ) ? 1 : 0;

	if (ptr_evt==SVG_POINTEREVENTS_BOUNDINGBOX) return inside;

	check_fill = check_stroke = check_over = check_outline = check_vis = 0;
	/*
	check_vis: if set, return FALSE when visible property is not "visible"
	check_fill:
		if 1, checks whether point is over path,
		if 2, checks if the path is painted (even with fill-opacity=0) before
	check_stroke:
		if 1, checks whether point is over path outline,
		if 2, checks if the path outline is painted (even with stroke-opacity=0) before
	*/
	switch (ptr_evt) {
	case SVG_POINTEREVENTS_VISIBLE:
		check_vis = 1;
		check_fill = 1;
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_VISIBLEFILL:
		check_vis = 1;
		check_fill = 1;
		break;
	case SVG_POINTEREVENTS_VISIBLESTROKE:
		check_vis = 1;
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_VISIBLEPAINTED:
		check_vis = 1;
		check_fill = 2;
		check_stroke = 2;
		break;
	case SVG_POINTEREVENTS_FILL:
		check_fill = 1;
		break;
	case SVG_POINTEREVENTS_STROKE:
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_ALL:
		check_fill = 1;
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_PAINTED:
		check_fill = 2;
		check_stroke = 2;
		break;
	default:
		return 0;
	}

	/*!!watchout!! asp2D.width is 0 if stroke not visible due to painting properties - we must override this
	for picking*/
	if (check_stroke==1) {
		asp->pen_props.width = tr_state->svg_props->stroke_width ? tr_state->svg_props->stroke_width->value : 0;
	}
	if (!asp->pen_props.width) check_stroke = 0;

	if (check_stroke) {
		/*rough estimation of stroke bounding box to avoid fetching the stroke each time*/
		if (!inside) {
			Fixed width = asp->pen_props.width;
			rc.x -= width;
			rc.y += width;
			rc.width += 2*width;
			rc.height += 2*width;
			inside = ( (x >= rc.x) && (y <= rc.y) && (x <= rc.x + rc.width) && (y >= rc.y - rc.height) ) ? 1 : 0;
			if (!inside) return 0;
		}
	} else if (!inside) {
		return 0;
	}

	if (check_vis) {
		if (*tr_state->svg_props->visibility!=SVG_VISIBILITY_VISIBLE) return 0;
	}

	if (check_fill) {
		/*painted or don't care about fill*/
		if ((check_fill!=2) || asp->fill_texture || asp->fill_color) {
			if (glyph_rc) return 1;
			/*point is over path*/
			if (gf_path_point_over(drawable->path, x, y)) return 1;
		}
	}
	if (check_stroke) {
		/*not painted or don't care about stroke*/
		if ((check_stroke!=2) || asp->line_texture || asp->line_color) {
			StrikeInfo2D *si;
			if (glyph_rc) return 1;
			si = drawable_get_strikeinfo(tr_state->visual->compositor, drawable, asp, tr_state->appear, NULL, 0, NULL);
			/*point is over outline*/
			if (si && si->outline && gf_path_point_over(si->outline, x, y))
				return 1;
		}
	}
	return 0;
}


void svg_clone_use_stack(GF_Compositor *compositor, GF_TraverseState *tr_state)
{
	u32 i, count;
	count = gf_list_count(tr_state->use_stack);
	gf_list_reset(compositor->hit_use_stack);
	for (i=0; i<count; i++) {
		GF_Node *node = gf_list_get(tr_state->use_stack, i);
		gf_list_add(compositor->hit_use_stack, node);
	}
}

#ifndef GPAC_DISABLE_3D

void svg_drawable_3d_pick(Drawable *drawable, GF_TraverseState *tr_state, DrawAspect2D *asp)
{
	SFVec3f local_pt, world_pt, vdiff;
	SFVec3f hit_normal;
	SFVec2f text_coords;
	u32 i, count;
	Fixed sqdist;
	Bool node_is_over;
	GF_Compositor *compositor;
	GF_Matrix mx;
	GF_Ray r;

	compositor = tr_state->visual->compositor;

	r = tr_state->ray;
	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_inverse(&mx);
	gf_mx_apply_ray(&mx, &r);

	/*if we already have a hit point don't check anything below...*/
	if (compositor->hit_square_dist && !compositor->grabbed_sensor && !tr_state->layer3d) {
		GF_Plane p;
		GF_BBox box;
		SFVec3f hit = compositor->hit_world_point;
		gf_mx_apply_vec(&mx, &hit);
		p.normal = r.dir;
		p.d = -1 * gf_vec_dot(p.normal, hit);
		gf_bbox_from_rect(&box, &drawable->path->bbox);

		if (gf_bbox_plane_relation(&box, &p) == GF_BBOX_FRONT) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Picking] bounding box of node %s (DEF %s) below current hit point - skipping\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node)));
			return;
		}
	}
	node_is_over = 0;
	if (compositor_get_2d_plane_intersection(&r, &local_pt)) {
		node_is_over = svg_drawable_is_over(drawable, local_pt.x, local_pt.y, asp, tr_state, NULL);
	}

	if (!node_is_over) return;

	hit_normal.x = hit_normal.y = 0;
	hit_normal.z = FIX_ONE;
	text_coords.x = gf_divfix(local_pt.x, drawable->path->bbox.width) + FIX_ONE/2;
	text_coords.y = gf_divfix(local_pt.y, drawable->path->bbox.height) + FIX_ONE/2;

	/*check distance from user and keep the closest hitpoint*/
	world_pt = local_pt;
	gf_mx_apply_vec(&tr_state->model_matrix, &world_pt);

	for (i=0; i<tr_state->num_clip_planes; i++) {
		if (gf_plane_get_distance(&tr_state->clip_planes[i], &world_pt) < 0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Picking] node %s (def %s) is not in clipper half space\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node)));
			return;
		}
	}

	gf_vec_diff(vdiff, world_pt, tr_state->ray.orig);
	sqdist = gf_vec_lensq(vdiff);
	if (compositor->hit_square_dist && (compositor->hit_square_dist+FIX_EPSILON<sqdist)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Picking] node %s (def %s) is farther (%g) than current pick (%g)\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node), FIX2FLT(sqdist), FIX2FLT(compositor->hit_square_dist)));
		return;
	}

	compositor->hit_square_dist = sqdist;

	/*also stack any VRML sensors present at the current level. If the event is not catched
	by a listener in the SVG tree, the event will be forwarded to the VRML tree*/
	gf_list_reset(compositor->sensors);
	count = gf_list_count(tr_state->vrml_sensors);
	for (i=0; i<count; i++) {
		gf_list_add(compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}

	gf_mx_copy(compositor->hit_world_to_local, tr_state->model_matrix);
	gf_mx_copy(compositor->hit_local_to_world, mx);
	compositor->hit_local_point = local_pt;
	compositor->hit_world_point = world_pt;
	compositor->hit_world_ray = tr_state->ray;
	compositor->hit_normal = hit_normal;
	compositor->hit_texcoords = text_coords;

	svg_clone_use_stack(compositor, tr_state);
	/*not use in SVG patterns*/
	compositor->hit_appear = NULL;
	compositor->hit_node = drawable->node;
	compositor->hit_text = NULL;
	compositor->hit_use_dom_events = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Picking] node %s (def %s) is under mouse - hit %g %g %g\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node),
	                                      FIX2FLT(world_pt.x), FIX2FLT(world_pt.y), FIX2FLT(world_pt.z)));
}

#endif

void svg_drawable_pick(GF_Node *node, Drawable *drawable, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;
	GF_Matrix2D inv_2d;
	Fixed x, y;
	Bool picked = 0;
	GF_Compositor *compositor = tr_state->visual->compositor;
	SVGPropertiesPointers backup_props;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGAllAttributes all_atts;

	if (!drawable->path) return;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	memcpy(&backup_props, tr_state->svg_props, sizeof(SVGPropertiesPointers));
	gf_svg_apply_inheritance(&all_atts, tr_state->svg_props);
	if (compositor_svg_is_display_off(tr_state->svg_props)) return;

	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_svg(node, &asp, tr_state);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		svg_drawable_3d_pick(drawable, tr_state, &asp);
		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		return;
	}
#endif
	gf_mx2d_copy(inv_2d, tr_state->transform);
	gf_mx2d_inverse(&inv_2d);
	x = tr_state->ray.orig.x;
	y = tr_state->ray.orig.y;
	gf_mx2d_apply_coords(&inv_2d, &x, &y);

	picked = svg_drawable_is_over(drawable, x, y, &asp, tr_state, NULL);

	if (picked) {
		u32 count, i;
		compositor->hit_local_point.x = x;
		compositor->hit_local_point.y = y;
		compositor->hit_local_point.z = 0;

		gf_mx_from_mx2d(&compositor->hit_world_to_local, &tr_state->transform);
		gf_mx_from_mx2d(&compositor->hit_local_to_world, &inv_2d);

		compositor->hit_node = drawable->node;
		compositor->hit_use_dom_events = 1;
		compositor->hit_normal.x = compositor->hit_normal.y = 0;
		compositor->hit_normal.z = FIX_ONE;
		compositor->hit_texcoords.x = gf_divfix(x, drawable->path->bbox.width) + FIX_ONE/2;
		compositor->hit_texcoords.y = gf_divfix(y, drawable->path->bbox.height) + FIX_ONE/2;
		svg_clone_use_stack(compositor, tr_state);
		/*not use in SVG patterns*/
		compositor->hit_appear = NULL;

		/*also stack any VRML sensors present at the current level. If the event is not catched
		by a listener in the SVG tree, the event will be forwarded to the VRML tree*/
		gf_list_reset(tr_state->visual->compositor->sensors);
		count = gf_list_count(tr_state->vrml_sensors);
		for (i=0; i<count; i++) {
			gf_list_add(tr_state->visual->compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Picking] node %s is under mouse - hit %g %g 0\n", gf_node_get_log_name(drawable->node), FIX2FLT(x), FIX2FLT(y)));
	}

	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

static void svg_drawable_traverse(GF_Node *node, void *rs, Bool is_destroy,
                                  void (*rebuild_path)(GF_Node *, Drawable *, SVGAllAttributes *),
                                  Bool is_svg_rect, Bool is_svg_path)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	DrawableContext *ctx;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *drawable = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
#if USE_GF_PATH
		/* The path is the same as the one in the SVG node, don't delete it here */
		if (is_svg_path) drawable->path = NULL;
#endif
		drawable_node_del(node);
		return;
	}
	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);


	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		svg_drawable_pick(node, drawable, tr_state);
		return;
	}

	/*flatten attributes and apply animations + inheritance*/
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!compositor_svg_traverse_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags))
		return;

	/* Recreates the path (i.e the shape) only if the node is dirty */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		/*the rebuild function is responsible for cleaning the path*/
		rebuild_path(node, drawable, &all_atts);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		drawable_mark_modified(drawable, tr_state);
	}
	if (drawable->path) {
		if (*(tr_state->svg_props->fill_rule)==GF_PATH_FILL_ZERO_NONZERO) {
			if (!(drawable->path->flags & GF_PATH_FILL_ZERO_NONZERO)) {
				drawable->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
				drawable_mark_modified(drawable, tr_state);
			}
		} else {
			if (drawable->path->flags & GF_PATH_FILL_ZERO_NONZERO) {
				drawable->path->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
				drawable_mark_modified(drawable, tr_state);
			}
		}
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (! compositor_svg_is_display_off(tr_state->svg_props)) {
			DrawAspect2D asp;
			gf_path_get_bounds(drawable->path, &tr_state->bounds);
			if (!tr_state->ignore_strike) {
				memset(&asp, 0, sizeof(DrawAspect2D));
				drawable_get_aspect_2d_svg(node, &asp, tr_state);
				if (asp.pen_props.width) {
					StrikeInfo2D *si = drawable_get_strikeinfo(tr_state->visual->compositor, drawable, &asp, NULL, drawable->path, 0, NULL);
					if (si && si->outline) {
						gf_path_get_bounds(si->outline, &tr_state->bounds);
					}
				}
			}
			compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, NULL);
			if (!tr_state->abort_bounds_traverse)
				gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
			gf_sc_get_nodes_bounds(node, NULL, tr_state, NULL);

			compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, NULL);
		}
	} else if (tr_state->traversing_mode == TRAVERSE_SORT) {
		/*reset our flags - this may break reuse of nodes and change-detection in dirty-rect algo */
		gf_node_dirty_clear(node, 0);

		if (!compositor_svg_is_display_off(tr_state->svg_props) &&
		        ( *(tr_state->svg_props->visibility) != SVG_VISIBILITY_HIDDEN) ) {

			compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

			ctx = drawable_init_context_svg(drawable, tr_state, all_atts.clip_path);
			if (ctx) {
				if (is_svg_rect) {
					if (ctx->aspect.fill_texture && ctx->aspect.fill_texture->transparent) {}
					else if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {}
					else if (ctx->transform.m[1] || ctx->transform.m[3]) {}
					else {
						ctx->flags &= ~CTX_IS_TRANSPARENT;
						if (!ctx->aspect.pen_props.width)
							ctx->flags |= CTX_NO_ANTIALIAS;
					}
				}

				if (all_atts.pathLength && all_atts.pathLength->type==SVG_NUMBER_VALUE)
					ctx->aspect.pen_props.path_length = all_atts.pathLength->value;

#ifndef GPAC_DISABLE_3D
				if (tr_state->visual->type_3d) {
					if (!drawable->mesh) {
						drawable->mesh = new_mesh();
						if (drawable->path) mesh_from_path(drawable->mesh, drawable->path);
					}
					visual_3d_draw_from_context(ctx, tr_state);
					ctx->drawable = NULL;
				} else
#endif
				{
					drawable_finalize_sort(ctx, tr_state, NULL);
				}
			}
			compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	}

	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

static GF_Err svg_rect_add_arc(GF_Path *gp, Fixed end_x, Fixed end_y, Fixed cx, Fixed cy, Fixed rx, Fixed ry)
{
	Fixed angle, start_angle, end_angle, sweep, _vx, _vy, start_x, start_y;
	s32 i, num_steps;

	if (!gp->n_points) return GF_BAD_PARAM;

	start_x = gp->points[gp->n_points-1].x;
	start_y = gp->points[gp->n_points-1].y;

	//start angle and end angle
	start_angle = gf_atan2(start_y-cy, start_x-cx);
	end_angle = gf_atan2(end_y-cy, end_x-cx);
	sweep = end_angle - start_angle;

	if (sweep<0) sweep += 2*GF_PI;

	num_steps = 16;
	for (i=1; i<=num_steps; i++) {
		angle = start_angle + sweep*i/num_steps;
		_vx = cx + gf_mulfix(rx, gf_cos(angle));
		_vy = cy + gf_mulfix(ry, gf_sin(angle));
		gf_path_add_line_to(gp, _vx, _vy);
	}
	return GF_OK;
}

static void svg_rect_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	Fixed rx = (atts->rx ? atts->rx->value : 0);
	Fixed ry = (atts->ry ? atts->ry->value : 0);
	Fixed x = (atts->x ? atts->x->value : 0);
	Fixed y = (atts->y ? atts->y->value : 0);
	Fixed width = (atts->width ? atts->width->value : 0);
	Fixed height = (atts->height ? atts->height->value : 0);

	drawable_reset_path(stack);
	if (!width || !height) return;

	/*we follow SVG 1.1 and not 1.2 !!*/
	if (rx || ry) {
		Fixed cx, cy;
		if (rx >= width/2) rx = width/2;
		if (ry >= height/2) ry = height/2;
		if (rx == 0) rx = ry;
		if (ry == 0) ry = rx;
		gf_path_add_move_to(stack->path, x+rx, y);

		if (width-rx!=rx)
			gf_path_add_line_to(stack->path, x+width-rx, y);

		cx = x+width-rx;
		cy = y+ry;
		svg_rect_add_arc(stack->path, x+width, y+ry, cx, cy, rx, ry);

		if (height-ry!=ry)
			gf_path_add_line_to(stack->path, x+width, y+height-ry);

		cx = x+width-rx;
		cy = y+height-ry;
		svg_rect_add_arc(stack->path, x+width-rx, y+height, cx, cy, rx, ry);

		if (width-rx!=rx)
			gf_path_add_line_to(stack->path, x+rx, y+height);

		cx = x+rx;
		cy = y+height-ry;
		svg_rect_add_arc(stack->path, x, y+height-ry, cx, cy, rx, ry);

		if (height-ry!=ry)
			gf_path_add_line_to(stack->path, x, y+ry);

		cx = x+rx;
		cy = y+ry;
		svg_rect_add_arc(stack->path, x+rx, y, cx, cy, rx, ry);

		gf_path_close(stack->path);
	} else {
		gf_path_add_move_to(stack->path, x, y);
		gf_path_add_line_to(stack->path, x+width, y);
		gf_path_add_line_to(stack->path, x+width, y+height);
		gf_path_add_line_to(stack->path, x, y+height);
		gf_path_close(stack->path);
	}
}

static void svg_traverse_rect(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_rect_rebuild, 1, 0);
}

void compositor_init_svg_rect(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_rect);
}

static void svg_circle_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	Fixed r = 2*(atts->r ? atts->r->value : 0);
	drawable_reset_path(stack);
	gf_path_add_ellipse(stack->path, (atts->cx ? atts->cx->value : 0), (atts->cy ? atts->cy->value : 0), r, r);
}

static void svg_traverse_circle(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_circle_rebuild, 0, 0);
}

void compositor_init_svg_circle(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_circle);
}

static void svg_ellipse_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	drawable_reset_path(stack);
	gf_path_add_ellipse(stack->path, (atts->cx ? atts->cx->value : 0),
	                    (atts->cy ? atts->cy->value : 0),
	                    (atts->rx ? 2*atts->rx->value : 0),
	                    (atts->ry ? 2*atts->ry->value : 0));
}
static void svg_traverse_ellipse(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_ellipse_rebuild, 0, 0);
}

void compositor_init_svg_ellipse(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_ellipse);
}

static void svg_line_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	drawable_reset_path(stack);
	gf_path_add_move_to(stack->path, (atts->x1 ? atts->x1->value : 0), (atts->y1 ? atts->y1->value : 0));
	gf_path_add_line_to(stack->path, (atts->x2 ? atts->x2->value : 0), (atts->y2 ? atts->y2->value : 0));
}
static void svg_traverse_line(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_line_rebuild, 0, 0);
}

void compositor_init_svg_line(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_line);
}

static void svg_polyline_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	u32 i, nbPoints;
	drawable_reset_path(stack);
	if (atts->points)
		nbPoints = gf_list_count(*atts->points);
	else
		nbPoints = 0;

	if (nbPoints) {
		SVG_Point *p = (SVG_Point *)gf_list_get(*atts->points, 0);
		gf_path_add_move_to(stack->path, p->x, p->y);
		for (i = 1; i < nbPoints; i++) {
			p = (SVG_Point *)gf_list_get(*atts->points, i);
			gf_path_add_line_to(stack->path, p->x, p->y);
		}
	} else {
		gf_path_add_move_to(stack->path, 0, 0);
	}
}
static void svg_traverse_polyline(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_polyline_rebuild, 0, 0);
}

void compositor_init_svg_polyline(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_polyline);
}

static void svg_polygon_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	u32 i, nbPoints;
	drawable_reset_path(stack);
	if (atts->points)
		nbPoints = gf_list_count(*atts->points);
	else
		nbPoints = 0;

	if (nbPoints) {
		SVG_Point *p = (SVG_Point *)gf_list_get(*atts->points, 0);
		gf_path_add_move_to(stack->path, p->x, p->y);
		for (i = 1; i < nbPoints; i++) {
			p = (SVG_Point *)gf_list_get(*atts->points, i);
			gf_path_add_line_to(stack->path, p->x, p->y);
		}
	} else {
		gf_path_add_move_to(stack->path, 0, 0);
	}
	/*according to the spec, the polygon path is closed*/
	gf_path_close(stack->path);
}
static void svg_traverse_polygon(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_polygon_rebuild, 0, 0);
}

void compositor_init_svg_polygon(GF_Compositor *compositor, GF_Node *node)
{
	drawable_stack_new(compositor, node);
	gf_node_set_callback_function(node, svg_traverse_polygon);
}


static void svg_path_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
#if USE_GF_PATH
	drawable_reset_path_outline(stack);
	stack->path = atts->d;
#else
	drawable_reset_path(stack);
	gf_svg_path_build(stack->path, atts->d->commands, atts->d->points);
#endif
}

static void svg_traverse_path(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_traverse(node, rs, is_destroy, svg_path_rebuild, 0, 1);
}

void compositor_init_svg_path(GF_Compositor *compositor, GF_Node *node)
{
	Drawable *dr = drawable_stack_new(compositor, node);
	gf_path_del(dr->path);
	dr->path = NULL;
	gf_node_set_callback_function(node, svg_traverse_path);
}

#endif //defined(GPAC_DISABLE_SVG)

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
