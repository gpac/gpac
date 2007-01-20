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

static void build_graph(Drawable *cs, M_IndexedFaceSet2D *ifs2D)
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
				gf_path_close(cs->path);
				started = 0;
			} else if (!started) {
				started = 1;
				gf_path_add_move_to_vec(cs->path, &pts[ifs2D->coordIndex.vals[i]]);
			} else {
				gf_path_add_line_to_vec(cs->path, &pts[ifs2D->coordIndex.vals[i]]);
			}
		}
		if (started) gf_path_close(cs->path);
	} else if (c_count) {
		gf_path_add_move_to_vec(cs->path, &pts[0]);
		for (i=1; i < c_count; i++) {
			gf_path_add_line_to_vec(cs->path, &pts[i]);
		}
		gf_path_close(cs->path);
	}
}


static void IFS2D_Draw(GF_Node *node, RenderEffect2D *eff)
{
	u32 i, count, ci_count;
	u32 start_pts, j, ind_col, num_col;
	SFVec2f center, end;
	SFColor col_cen;
	GF_STENCIL grad;
	u32 *colors;
	GF_Path *path;
	SFVec2f start;
	SFVec2f *pts;
	SFColor col;
	Fixed alpha;
	GF_Raster2D *r2d;
	DrawableContext *ctx = eff->ctx;
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ifs2D->coord;
	M_Color *color = (M_Color *) ifs2D->color;
		
	col.red = col.green = col.blue = 0;
	/*simple case, no color specified*/
	if (!ifs2D->color) {
		VS2D_TexturePath(eff->surface, ctx->drawable->path, ctx);
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
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
		VS2D_TexturePath(eff->surface, ctx->drawable->path, ctx);
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
		return;
	}

	/*we have color per faces so we need N path :(*/
	if (! ifs2D->colorPerVertex) {
		path = gf_path_new();

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

			VS2D_TexturePath(eff->surface, path, ctx);
			VS2D_DrawPath(eff->surface, path, ctx, NULL, NULL);
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
	r2d = eff->surface->render->compositor->r2d;
	grad = r2d->stencil_new(r2d, GF_STENCIL_VERTEX_GRADIENT);
	/*not supported, fill default*/
	if (!grad) {
		VS2D_TexturePath(eff->surface, ctx->drawable->path, ctx);
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
		return;
	}


	path = gf_path_new();

	ind_col = 0;
	i = 0;
	while (1) {
		gf_path_reset(path);
		start = pts[ifs2D->coordIndex.vals[i]];
		center = start;
		gf_path_add_move_to(path, start.x, start.y);
		start_pts = i;
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

		colors = (u32*)malloc(sizeof(u32) * num_col);
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

		r2d->stencil_set_vertex_path(grad, path);
		r2d->stencil_set_vertex_colors(grad, colors, num_col);

		free(colors);
		
		col_cen.blue /= num_col;
		col_cen.green /= num_col;
		col_cen.red /= num_col;
		center.x /= num_col;
		center.y /= num_col;
		r2d->stencil_set_vertex_center(grad, center.x, center.y, GF_COL_ARGB_FIXED(alpha, col_cen.red, col_cen.green, col_cen.blue) );

		r2d->stencil_set_matrix(grad, &ctx->transform);

		/*draw*/
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, grad, grad);

		r2d->stencil_delete(grad);

		//goto next point
		i++;
		ind_col += num_col + 1;	
		if (i >= ci_count) break;
		grad = r2d->stencil_new(r2d, GF_STENCIL_VERTEX_GRADIENT);
		ctx->flags &= ~CTX_PATH_FILLED;
		ctx->flags &= ~CTX_PATH_STROKE;
	}
	gf_path_del(path);
}

static void RenderIFS2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		IFS2D_Draw(node, eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	if (!ifs2D->coord) return;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		build_graph(cs, ifs2D);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	
	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	drawable_finalize_render(ctx, eff, NULL);
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

void R2D_InitIFS2D(Render2D *sr, GF_Node *node)
{
	M_IndexedFaceSet2D *ifs2D = (M_IndexedFaceSet2D *)node;
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, RenderIFS2D);
	ifs2D->on_set_colorIndex = IFS2D_SetColorIndex;
	ifs2D->on_set_coordIndex = IFS2D_SetCoordIndex;
}

