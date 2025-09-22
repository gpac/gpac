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

static void ils2d_check_changes(GF_Node *node, Drawable *stack, GF_TraverseState *tr_state)
{
	u32 i;
	Bool started;
	SFVec2f *pts;
	M_IndexedLineSet2D *ils2D;
	M_Coordinate2D *coord;

	if (! gf_node_dirty_get(node)) return;

	drawable_reset_path(stack);
	gf_node_dirty_clear(node, 0);
	drawable_mark_modified(stack, tr_state);

	ils2D = (M_IndexedLineSet2D *)node;
	coord = (M_Coordinate2D *)ils2D->coord;

	pts = coord->point.vals;
	if (ils2D->coordIndex.count > 0) {
		started = 0;
		for (i=0; i < ils2D->coordIndex.count; i++) {
			/*NO close on ILS2D*/
			if (ils2D->coordIndex.vals[i] == -1) {
				started = 0;
			} else if (!started) {
				started = 1;
				gf_path_add_move_to(stack->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			} else {
				gf_path_add_line_to(stack->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			}
		}
	} else if (coord->point.count) {
		gf_path_add_move_to(stack->path, pts[0].x, pts[0].y);
		for (i=1; i < coord->point.count; i++) {
			gf_path_add_line_to(stack->path, pts[i].x, pts[i].y);
		}
	}
	stack->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
}

static void ILS2D_Draw(GF_Node *node, GF_TraverseState *tr_state)
{
	GF_Path *path;
	SFVec2f *pts;
	SFColor col;
	Fixed alpha;
	u32 i, count, col_ind, ind, end_at;
	u32 linear[2];
#if 0 //unused
	u32 *colors, j;
#endif
	SFVec2f start, end;
	u32 num_col;
	GF_EVGStencil *grad;
	DrawableContext *ctx = tr_state->ctx;
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ils2D->coord;
	M_Color *color = (M_Color *) ils2D->color;

	end.x = end.y = 0;
	if (!coord->point.count) return;

	if (! ils2D->color) {
		/*no texturing*/
		visual_2d_draw_path(tr_state->visual, ctx->drawable->path, ctx, NULL, NULL, tr_state);
		return;
	}

	alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
	pts = coord->point.vals;

	if (!ils2D->colorPerVertex || (color->color.count<2) ) {
		count = 0;
		end_at = ils2D->coordIndex.count;
		if (!end_at) end_at = coord->point.count;
		ind = ils2D->coordIndex.count ? ils2D->coordIndex.vals[0] : 0;
		i=1;
		path = gf_path_new();
		gf_path_add_move_to(path, pts[ind].x, pts[ind].y);

		for (; i<=end_at; i++) {
			if ((i==end_at) || (ils2D->coordIndex.count && ils2D->coordIndex.vals[i] == -1)) {

				/*draw current*/
				col_ind = (ils2D->colorIndex.count && (ils2D->colorIndex.vals[count]>=0) ) ? (u32) ils2D->colorIndex.vals[count] : count;
				if (col_ind>=color->color.count) col_ind=color->color.count-1;
				col = color->color.vals[col_ind];
				ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);

				visual_2d_draw_path(tr_state->visual, path, ctx, NULL, NULL, tr_state);

				i++;
				if (i>=end_at) break;
				gf_path_reset(path);

				ind = (ils2D->coordIndex.count && (ils2D->coordIndex.vals[i]>=0)) ? (u32) ils2D->coordIndex.vals[i] : i;
				gf_path_add_move_to(path, pts[ind].x, pts[ind].y);

				if (ils2D->coordIndex.count) count++;
				continue;
			} else {
				ind = (ils2D->coordIndex.count && (ils2D->coordIndex.vals[i]>=0) ) ? (u32) ils2D->coordIndex.vals[i] : i;
				gf_path_add_line_to(path, pts[ind].x, pts[ind].y);
			}
		}
		gf_path_del(path);
		return;
	}

	end_at = ils2D->coordIndex.count;
	if (!end_at) end_at = coord->point.count;

	col_ind = 0;
	i=0;
	path = gf_path_new();
	while (1) {
		gf_path_reset(path);
		ind = (ils2D->coordIndex.count && (ils2D->coordIndex.vals[i]>=0)) ? (u32) ils2D->coordIndex.vals[i] : i;
		start = pts[ind];
		num_col = 1;
		i++;
		gf_path_add_move_to(path, start.x, start.y);

		if (ils2D->coordIndex.count) {
			while (ils2D->coordIndex.vals[i] != -1) {
				end = pts[ils2D->coordIndex.vals[i]];
				gf_path_add_line_to(path, end.x, end.y);
				i++;
				num_col++;
				if (i >= ils2D->coordIndex.count) break;
			}
		} else {
			while (i<end_at) {
				end = pts[i];
				gf_path_add_line_to(path, end.x, end.y);
				i++;
				num_col++;
			}
		}

		/*use linear gradient*/
		if (num_col==2) {
			Fixed pos[2];
			grad = gf_evg_stencil_new(GF_STENCIL_LINEAR_GRADIENT);
			if (ils2D->colorIndex.count) {
				col = color->color.vals[ils2D->colorIndex.vals[col_ind]];
				linear[0] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
				col = color->color.vals[ils2D->colorIndex.vals[col_ind+1]];
				linear[1] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			} else if (ils2D->coordIndex.count) {
				col = color->color.vals[ils2D->coordIndex.vals[col_ind]];
				linear[0] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
				col = color->color.vals[ils2D->coordIndex.vals[col_ind+1]];
				linear[1] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			} else {
				col = color->color.vals[col_ind];
				linear[0] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
				col = color->color.vals[col_ind+1];
				linear[1] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
			}
			pos[0] = 0;
			pos[1] = FIX_ONE;
			gf_evg_stencil_set_linear_gradient(grad, start.x, start.y, end.x, end.y);
			gf_evg_stencil_set_gradient_interpolation(grad, pos, linear, 2);
		} else {
            grad = NULL;
#if 0 //unused
			grad = gf_evg_stencil_new(GF_STENCIL_VERTEX_GRADIENT);
			if (grad) {
				gf_evg_stencil_set_vertex_path(grad, path);

				colors = (u32*)gf_malloc(sizeof(u32) * num_col);
				for (j=0; j<num_col; j++) {
					if (ils2D->colorIndex.count>0) {
						col = color->color.vals[ils2D->colorIndex.vals[col_ind+j]];
					} else if (ils2D->coordIndex.count) {
						col = color->color.vals[ils2D->coordIndex.vals[col_ind+j]];
					} else {
						col = color->color.vals[col_ind+j];
					}
					colors[j] = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
				}
				gf_evg_stencil_set_vertex_colors(grad, colors, num_col);
				gf_free(colors);
			}
#endif

		}
		gf_evg_stencil_set_matrix(grad, &ctx->transform);
		gf_evg_stencil_set_auto_matrix(grad, GF_FALSE);
		visual_2d_draw_path(tr_state->visual, path, ctx, NULL, grad, tr_state);
		if (grad) gf_evg_stencil_delete(grad);

		i ++;
		col_ind += num_col + 1;
		if (i >= ils2D->coordIndex.count) break;
		ctx->flags &= ~CTX_PATH_STROKE;
	}
	gf_path_del(path);
}


static void TraverseILS2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_node_del(node);
		return;
	}

	if (!ils2D->coord) return;

	ils2d_check_changes(node, stack, tr_state);

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		ILS2D_Draw(node, tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		if (!stack->mesh) {
			stack->mesh = new_mesh();
			mesh_new_ils(stack->mesh, ils2D->coord, &ils2D->coordIndex, ils2D->color, &ils2D->colorIndex, ils2D->colorPerVertex, 0);
		}
		if (ils2D->color) {
			DrawAspect2D asp;
			memset(&asp, 0, sizeof(DrawAspect2D));
			drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);

			visual_3d_mesh_strike(tr_state, stack->mesh, asp.pen_props.width, asp.line_scale, asp.pen_props.dash);
		} else {
			visual_3d_draw_2d(stack, tr_state);
		}
		return;
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
		/*ILS2D are NEVER filled*/
		ctx->aspect.fill_color &= 0x00FFFFFF;
		drawable_finalize_sort(ctx, tr_state, NULL);
		return;
	default:
		return;
	}
}

static void ILS2D_SetColorIndex(GF_Node *node, GF_Route *route)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	if (node) {
		gf_sg_vrml_field_copy(&ils2D->colorIndex, &ils2D->set_colorIndex, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&ils2D->set_colorIndex, GF_SG_VRML_MFINT32);
	}
}

static void ILS2D_SetCoordIndex(GF_Node *node, GF_Route *route)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	if (node) {
		gf_sg_vrml_field_copy(&ils2D->coordIndex, &ils2D->set_coordIndex, GF_SG_VRML_MFINT32);
		gf_sg_vrml_mf_reset(&ils2D->set_coordIndex, GF_SG_VRML_MFINT32);
	}
}

void compositor_init_indexed_line_set2d(GF_Compositor *compositor, GF_Node *node)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	Drawable *stack = drawable_stack_new(compositor, node);
	stack->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	gf_node_set_callback_function(node, TraverseILS2D);
	ils2D->on_set_colorIndex = ILS2D_SetColorIndex;
	ils2D->on_set_coordIndex = ILS2D_SetCoordIndex;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		ILS2D_SetCoordIndex(NULL, NULL);
		ILS2D_SetColorIndex(NULL, NULL);
	}
#endif

}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
