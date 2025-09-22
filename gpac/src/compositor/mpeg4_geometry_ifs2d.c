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

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

static void ifs2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	u32 i;
	SFVec2f *pts;
	u32 ci_count, c_count;
	Bool started;
	M_IndexedFaceSet2D *ifs2D;
	M_Coordinate2D *coord;

	if (! gf_node_dirty_get(node)) return;

	ifs2D = (M_IndexedFaceSet2D *)node;
	coord = (M_Coordinate2D *)ifs2D->coord;
	drawable_reset_path(stack);
	gf_node_dirty_clear(node, 0);
	drawable_mark_modified(stack, tr_state);


	c_count = coord->point.count;
	ci_count = ifs2D->coordIndex.count;
	pts = coord->point.vals;

	if (ci_count > 0) {
		started = 0;
		for (i=0; i < ci_count; i++) {
			if (ifs2D->coordIndex.vals[i] == -1) {
				gf_path_close(stack->path);
				started = 0;
			} else if (!started) {
				started = 1;
				gf_path_add_move_to_vec(stack->path, &pts[ifs2D->coordIndex.vals[i]]);
			} else {
				gf_path_add_line_to_vec(stack->path, &pts[ifs2D->coordIndex.vals[i]]);
			}
		}
		if (started) gf_path_close(stack->path);
	} else if (c_count) {
		gf_path_add_move_to_vec(stack->path, &pts[0]);
		for (i=1; i < c_count; i++) {
			gf_path_add_line_to_vec(stack->path, &pts[i]);
		}
		gf_path_close(stack->path);
	}
}


static void IFS2D_Draw(GF_Node *node, GF_TraverseState *tr_state)
{
	u32 i, count, ci_count;
#if 0 //unused
	u32 j, ind_col, num_col;
	SFVec2f center, end;
	SFColor col_cen;
	GF_EVGStencil *grad;
	u32 *colors;
#endif
	SFVec2f start;
	SFVec2f *pts;
	SFColor col;
	Fixed alpha;
	DrawableContext *ctx = tr_state->ctx;
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ifs2D->coord;
	M_Color *color = (M_Color *) ifs2D->color;

	col.red = col.green = col.blue = 0;
	/*simple case, no color specified*/
	if (!ifs2D->color) {
		visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
		visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
		return;
	}

	/*if default face use first color*/
	ci_count = ifs2D->coordIndex.count;
	pts = coord->point.vals;

	if (ci_count == 0) {
		col = (ifs2D->colorIndex.count > 0) ? color->color.vals[ifs2D->colorIndex.vals[0]] : color->color.vals[0];

		alpha = INT2FIX(GF_COL_A(ctx->aspect.fill_color)) / 255;
		if (!alpha || !ctx->aspect.pen_props.width) {
			alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
			ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
		} else {
			ctx->aspect.fill_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
		}
		visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
		visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
		return;
	}

	/*we have color per faces so we need N path :(*/
	if (! ifs2D->colorPerVertex) {
		GF_Path *path = gf_path_new();

		count = 0;
		i = 0;
		while (1) {
			gf_path_reset(path);
			start = pts[ifs2D->coordIndex.vals[i]];
			gf_path_add_move_to(path, start.x, start.y);
			i++;

			while (ifs2D->coordIndex.vals[i] != -1) {
				start = pts[ifs2D->coordIndex.vals[i]];
				gf_path_add_line_to(path, start.x, start.y);
				i++;
				if (i >= ci_count) break;
			}
			/*close in ALL cases because even if the start/end points are the same the line join needs to be present*/
			gf_path_close(path);

			col = (ifs2D->colorIndex.count > 0) ? color->color.vals[ifs2D->colorIndex.vals[count]] : color->color.vals[count];

			alpha = INT2FIX(GF_COL_A(ctx->aspect.fill_color)) / 255;
			if (!alpha) {
				alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
				ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			} else {
				ctx->aspect.fill_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			}

			visual_2d_texture_path(tr_state->visual, path, ctx, tr_state);
			visual_2d_draw_path(tr_state->visual, path, ctx, NULL, NULL, tr_state);
			count++;
			i++;
			if (i >= ci_count) break;
			ctx->flags &= ~CTX_PATH_FILLED;
			ctx->flags &= ~CTX_PATH_STROKE;
		}
		gf_path_del(path);
		return;
	}

	/*final case, color per vertex means gradient fill/strike*/
	/*not supported, fill default*/
	visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
	visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
	return;


#if 0 //deprecated
	path = gf_path_new();

	ind_col = 0;
	i = 0;
	while (1) {
		gf_path_reset(path);
		start = pts[ifs2D->coordIndex.vals[i]];
		center = start;
		gf_path_add_move_to(path, start.x, start.y);
		num_col = 1;
		i+=1;
		while (ifs2D->coordIndex.vals[i] != -1) {
			end = pts[ifs2D->coordIndex.vals[i]];
			gf_path_add_line_to(path, end.x, end.y);
			i++;
			center.x += end.x;
			center.y += end.y;
			num_col ++;
			if (i >= ci_count) break;
		}
		gf_path_close(path);
		num_col++;

		alpha = INT2FIX(GF_COL_A(ctx->aspect.fill_color) ) / 255;

		colors = (u32*)gf_malloc(sizeof(u32) * num_col);
		col_cen.blue = col_cen.red = col_cen.green = 0;
		for (j=0; j<num_col-1; j++) {
			if (ifs2D->colorIndex.count > ind_col + j) {
				col = color->color.vals[ifs2D->colorIndex.vals[ind_col + j]];
			} else if (ci_count > ind_col + j) {
				col = color->color.vals[ifs2D->coordIndex.vals[ind_col + j]];
			}
			colors[j] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			col_cen.blue += col.blue;
			col_cen.green += col.green;
			col_cen.red += col.red;
		}
		colors[num_col-1] = colors[0];

		if (ifs2D->colorIndex.count > ind_col) {
			col = color->color.vals[ifs2D->colorIndex.vals[ind_col]];
		} else if (ci_count > ind_col) {
			col = color->color.vals[ifs2D->coordIndex.vals[ind_col]];
		}
		col_cen.blue += col.blue;
		col_cen.green += col.green;
		col_cen.red += col.red;

		gf_evg_stencil_set_vertex_path(grad, path);
		gf_evg_stencil_set_vertex_colors(grad, colors, num_col);

		gf_free(colors);

		col_cen.blue /= num_col;
		col_cen.green /= num_col;
		col_cen.red /= num_col;
		center.x /= num_col;
		center.y /= num_col;
		gf_evg_stencil_set_vertex_center(grad, center.x, center.y, GF_COL_ARGB_FIXED(alpha, col_cen.red, col_cen.green, col_cen.blue) );

		gf_evg_stencil_set_matrix(grad, &ctx->transform);

		/*draw*/
		visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, grad, grad, tr_state);

		gf_evg_stencil_delete(grad);

		//goto next point
		i++;
		ind_col += num_col + 1;
		if (i >= ci_count) break;
		grad = gf_evg_stencil_new(GF_STENCIL_VERTEX_GRADIENT);
		ctx->flags &= ~CTX_PATH_FILLED;
		ctx->flags &= ~CTX_PATH_STROKE;
	}
	gf_path_del(path);
#endif


}

static void TraverseIFS2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}
	if (!ifs2D->coord) return;

	ifs2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		IFS2D_Draw(node, tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
	{
		DrawAspect2D asp;

		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_new_ifs2d(stack->mesh, node);
		}

		memset(&asp, 0, sizeof(DrawAspect2D));
		drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);
		if (ifs2D->color && !GF_COL_A(asp.fill_color) ) {
			/*use special func to disable outline recompute and handle recompute ourselves*/
			StrikeInfo2D *si = drawable_get_strikeinfo(tr_state->visual->compositor, stack, &asp, tr_state->appear, NULL, 0, tr_state);
			if (!si->mesh_outline) {
				si->mesh_outline = new_mesh();
				mesh_new_ils(si->mesh_outline, ifs2D->coord, &ifs2D->coordIndex, ifs2D->color, &ifs2D->colorIndex, ifs2D->colorPerVertex, 1);
			}
			visual_3d_mesh_strike(tr_state, si->mesh_outline, asp.pen_props.width, asp.line_scale, asp.pen_props.dash);
		} else {
			visual_3d_draw_2d_with_aspect(stack, tr_state, &asp);
		}
		return;
	}
#endif
	case TRAVERSE_PICK:
		vrml_drawable_pick(stack, tr_state);
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

static void IFS2D_SetColorIndex(GF_Node *node, GF_Route *route)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	if (node) {
		gf_sg_vrml_field_copy(&ifs2D->colorIndex, &ifs2D->set_colorIndex, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&ifs2D->set_colorIndex, GF_SG_VRML_MFINT32);
	}
}

static void IFS2D_SetCoordIndex(GF_Node *node, GF_Route *route)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	if (node) {
		gf_sg_vrml_field_copy(&ifs2D->coordIndex, &ifs2D->set_coordIndex, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&ifs2D->set_coordIndex, GF_SG_VRML_MFINT32);
	}
}

void compositor_init_indexed_face_set2d(GF_Compositor *compositor, GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	Drawable *stack = drawable_stack_new(compositor, node);
	stack->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	gf_node_set_callback_function(node, TraverseIFS2D);
	ifs2D->on_set_colorIndex = IFS2D_SetColorIndex;
	ifs2D->on_set_coordIndex = IFS2D_SetCoordIndex;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		IFS2D_SetCoordIndex(NULL, NULL);
		IFS2D_SetColorIndex(NULL, NULL);
	}
#endif

}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
