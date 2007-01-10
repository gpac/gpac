/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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



#include "stacks2d.h"
#include "visualsurface2d.h"

static void build_graph(Drawable *cs, M_IndexedLineSet2D *ils2D)
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
				gf_path_add_move_to(cs->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			} else {
				gf_path_add_line_to(cs->path, pts[ils2D->coordIndex.vals[i]].x, pts[ils2D->coordIndex.vals[i]].y);
			}
		}
	} else if (coord->point.count) {
		gf_path_add_move_to(cs->path, pts[0].x, pts[0].y);
		for (i=1; i < coord->point.count; i++) {
			gf_path_add_line_to(cs->path, pts[i].x, pts[i].y);
		}
	}
	cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
}

static void ILS2D_Draw(GF_Node *node, RenderEffect2D *eff)
{
	GF_Path *path;
	SFVec2f *pts;
	SFColor col;
	Fixed alpha;
	u32 i, count, col_ind, ind, end_at;
	u32 linear[2], *colors;
	SFVec2f start, end;
	u32 j, num_col;
	GF_STENCIL grad;
	GF_Raster2D *r2d;
	DrawableContext *ctx = eff->ctx;
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ils2D->coord;
	M_Color *color = (M_Color *) ils2D->color;

	end.x = end.y = 0;
	if (!coord->point.count) return;

	if (! ils2D->color) {
		/*no texturing*/
		VS2D_DrawPath(eff->surface, ctx->node->path, ctx, NULL, NULL);
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
				col_ind = (ils2D->colorIndex.count) ? ils2D->colorIndex.vals[count] : count;
				if (col_ind>=color->color.count) col_ind=color->color.count-1;
				col = color->color.vals[col_ind];
				ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);

				VS2D_DrawPath(eff->surface, path, ctx, NULL, NULL);

				i++;
				if (i>=end_at) break;
				gf_path_reset(path);

				ind = ils2D->coordIndex.count ? ils2D->coordIndex.vals[i] : i;
				gf_path_add_move_to(path, pts[ind].x, pts[ind].y);

				if (ils2D->coordIndex.count) count++;
				continue;
			} else {
				ind = ils2D->coordIndex.count ? ils2D->coordIndex.vals[i] : i;
				gf_path_add_line_to(path, pts[ind].x, pts[ind].y);
			}
		}
		gf_path_del(path);
		return;
	}
	
	r2d = NULL;
	end_at = ils2D->coordIndex.count;
	if (!end_at) end_at = coord->point.count;
	count = 0;
	col_ind = 0;
	ind = 0;
	i=0;
	path = gf_path_new();
	while (1) {
		gf_path_reset(path);
		ind = ils2D->coordIndex.count ? ils2D->coordIndex.vals[i] : i;
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

		r2d = eff->surface->render->compositor->r2d;
		/*use linear gradient*/
		if (num_col==2) {
			Fixed pos[2];
			grad = r2d->stencil_new(r2d, GF_STENCIL_LINEAR_GRADIENT);
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
			pos[0] = 0; pos[1] = FIX_ONE;
			r2d->stencil_set_linear_gradient(grad, start.x, start.y, end.x, end.y);
			r2d->stencil_set_gradient_interpolation(grad, pos, linear, 2);
		} else {
			grad = r2d->stencil_new(r2d, GF_STENCIL_VERTEX_GRADIENT);
			if (grad) {
				r2d->stencil_set_vertex_path(grad, path);

				colors = (u32*)malloc(sizeof(u32) * num_col);
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
				r2d->stencil_set_vertex_colors(grad, colors, num_col);
				free(colors);
			}
		}
		r2d->stencil_set_matrix(grad, &ctx->transform);
		VS2D_DrawPath(eff->surface, path, ctx, NULL, grad);
		if (grad) r2d->stencil_delete(grad);

		i ++;
		col_ind += num_col + 1;
		if (i >= ils2D->coordIndex.count) break;
		ctx->flags &= ~CTX_PATH_STROKE;
	}
	gf_path_del(path);
}


static void RenderILS2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		ILS2D_Draw(node, eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		return;
	}

	if (!ils2D->coord) return;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		build_graph(cs, ils2D);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}

	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	/*ILS2D are NEVER filled*/
	ctx->aspect.fill_color &= 0x00FFFFFF;
	drawable_finalize_render(ctx, eff, NULL);
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

void R2D_InitILS2D(Render2D *sr, GF_Node *node)
{
	M_IndexedLineSet2D *ils2D = (M_IndexedLineSet2D *)node;
	Drawable * stack = drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, RenderILS2D);
	ils2D->on_set_colorIndex = ILS2D_SetColorIndex;
	ils2D->on_set_coordIndex = ILS2D_SetCoordIndex;
}
