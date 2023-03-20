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

#ifndef GPAC_DISABLE_3D
#include <gpac/internal/mesh.h>
#endif

#if defined(GPAC_ENABLE_FLASHSHAPE) && !defined(GPAC_DISABLE_COMPOSITOR)


typedef struct
{
	GF_Path *path;
	Fixed width;
	u32 fill_col, line_col;
#ifndef GPAC_DISABLE_3D
	GF_Mesh *mesh;
#endif
} FSItem;


typedef struct
{
	GF_Compositor *compositor;
	Drawable *drawable;
	GF_Rect bounds;
	GF_List *items;
	Fixed max_width;
} FSStack;


static void clean_paths(FSStack *stack)
{
	/*delete all path objects*/
	while (gf_list_count(stack->items)) {
		FSItem *it = (FSItem*)gf_list_get(stack->items, 0);
		gf_list_rem(stack->items, 0);
		if (it->path) gf_path_del(it->path);
#ifndef GPAC_DISABLE_3D
		if (it->mesh) mesh_free(it->mesh);
#endif
		gf_free(it);
	}
}

static FSItem *new_fs_item(FSStack *st, u32 line_col, u32 fill_col, Fixed width)
{
	FSItem *item;
	GF_SAFEALLOC(item, FSItem);
	if (!item) return NULL;
	gf_list_add(st->items, item);
	item->fill_col = fill_col;
	item->width = width;
	item->line_col = line_col;
	item->path = gf_path_new();
	return item;
}

#define SFCOL_MAKE_ARGB(c) GF_COL_ARGB_FIXED(FIX_ONE, c.red, c.green, c.blue);

static void build_shape(FSStack *st, GF_Node *node)
{
	GF_FieldInfo field;
	MFVec2f *coords;
	MFInt32 *com, *widthIndex, *lineIndex, *fillIndex, *coordIndex;
	MFFloat *widths;
	MFColor *colors;
	u32 wi, li, fi, ci, command, i, has_ci;
	FSItem *fill_item, *line_item;
	Fixed w;
	SFVec2f cur, pt, ct1= {0,0}, ct2, *pts;
	GF_Rect rc;
	u32 line_col, fill_col;
	Bool need_line, need_fill;

	/*get all fields*/
	gf_node_get_field(node, 0, &field);
	coords = (MFVec2f*)field.far_ptr;
	gf_node_get_field(node, 1, &field);
	com = (MFInt32*)field.far_ptr;
	gf_node_get_field(node, 2, &field);
	widths = (MFFloat*)field.far_ptr;
	gf_node_get_field(node, 3, &field);
	colors = (MFColor*)field.far_ptr;
	gf_node_get_field(node, 4, &field);
	widthIndex = (MFInt32*)field.far_ptr;
	gf_node_get_field(node, 5, &field);
	lineIndex = (MFInt32*)field.far_ptr;
	gf_node_get_field(node, 6, &field);
	fillIndex = (MFInt32*)field.far_ptr;
	gf_node_get_field(node, 7, &field);
	coordIndex = (MFInt32*)field.far_ptr;

	wi = li = fi = ci = 0;
	w = 0;

	st->max_width = 0;
	cur.x = cur.y = 0;
	fill_item = line_item = NULL;
	need_line = need_fill = GF_FALSE;
	has_ci = coordIndex->count ? 1 : 0;
	pts = coords->vals;
	line_col = fill_col = 0;

	/*implicit commands: 0 1 2 3*/

	/*
		if (widthIndex->count) {
			w = (widthIndex->vals[wi]==-1) ? 0 : widths->vals[widthIndex->vals[wi]];
			if (!w) {
				need_line = 0;
				line_item = NULL;
			} else {
				need_line = 1;
				if (st->max_width<w) st->max_width = w;
			}
			wi++;
		}
		if (lineIndex->count) {
			if (w) {
				line_col = SFCOL_MAKE_ARGB(colors->vals[lineIndex->vals[li]]);
				need_line = 1;
			}
			li++;
		}
		if (fillIndex->count) {
			if (fillIndex->vals[fi]==-1) {
				fill_col = 0;
				fill_item = NULL;
			} else {
				fill_col = SFCOL_MAKE_ARGB(colors->vals[fillIndex->vals[fi]]);
				need_fill = 1;
			}
			fi++;
		}
		if (!coordIndex->count) return;
		cur = coords->vals[coordIndex->vals[ci]];
		ci++;
	*/

	for (command=0; command<com->count; command++) {
		switch (com->vals[command]) {
		/*set line width*/
		case 0:
			if (wi >= widthIndex->count) return;
			w = (widthIndex->vals[wi]==-1) ? 0 : widths->vals[widthIndex->vals[wi]];
			if (!w)
				line_item = NULL;
			else {
				need_line = GF_TRUE;
				if (st->max_width<w) st->max_width = w;
			}
			wi++;
			break;
		/*set line color*/
		case 1:
			if (li > lineIndex->count) return;
			if (w) {
				line_col = SFCOL_MAKE_ARGB(colors->vals[lineIndex->vals[li]]);
				need_line = GF_TRUE;
			}
			li++;
			break;
		/*set fill color*/
		case 2:
			if (fi >= fillIndex->count) return;
			if (fillIndex->vals[fi]==-1) {
				fill_col = 0;
				fill_item = NULL;
			} else {
				fill_col = SFCOL_MAKE_ARGB(colors->vals[fillIndex->vals[fi]]);
				need_fill = GF_TRUE;
			}
			fi++;
			break;
		/*moveTo*/
		case 3:
			if ((has_ci && ci >= coordIndex->count) || (!has_ci && ci >= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				need_line = GF_FALSE;
			}
			if (has_ci) {
				pt = pts[coordIndex->vals[ci]];
			} else {
				pt = pts[ci];
			}
			if (fill_item) gf_path_add_move_to(fill_item->path, pt.x, pt.y);
			if (line_item) gf_path_add_move_to(line_item->path, pt.x, pt.y);
			ct1 = pt;
			cur = pt;
			ci++;
			break;
		/*lineTo*/
		case 4:
			if ((has_ci && ci >= coordIndex->count) || (!has_ci && ci >= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				gf_path_add_move_to(fill_item->path, cur.x, cur.y);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				gf_path_add_move_to(line_item->path, cur.x, cur.y);
				need_line = GF_FALSE;
			}
			if (has_ci) {
				pt = pts[coordIndex->vals[ci]];
			} else {
				pt = pts[ci];
			}
			if (fill_item) gf_path_add_line_to(fill_item->path, pt.x, pt.y);
			if (line_item) gf_path_add_line_to(line_item->path, pt.x, pt.y);
			cur = pt;
			ci++;
			break;
		/*cubic curveTo*/
		case 5:
			if ((has_ci && ci + 2 >= coordIndex->count) || (!has_ci && ci + 2>= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				gf_path_add_move_to(fill_item->path, cur.x, cur.y);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				gf_path_add_move_to(line_item->path, cur.x, cur.y);
				need_line = GF_FALSE;
			}
			if (has_ci) {
				ct1 = pts[coordIndex->vals[ci]];
				ct2 = pts[coordIndex->vals[ci+1]];
				pt = pts[coordIndex->vals[ci+2]];
			} else {
				ct1 = pts[ci];
				ct2 = pts[ci+1];
				pt = pts[ci+2];
			}
			if (fill_item) gf_path_add_cubic_to(fill_item->path, ct1.x, ct1.y, ct2.x, ct2.y, pt.x, pt.y);
			if (line_item) gf_path_add_cubic_to(line_item->path, ct1.x, ct1.y, ct2.x, ct2.y, pt.x, pt.y);
			ct1 = ct2;
			cur = pt;
			ci += 3;
			break;
		/*cubic nextCurveTo*/
		case 6:
			if ((has_ci && ci + 1 >= coordIndex->count) || (!has_ci && ci + 1>= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				gf_path_add_move_to(fill_item->path, cur.x, cur.y);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				gf_path_add_move_to(line_item->path, cur.x, cur.y);
				need_line = GF_FALSE;
			}
			ct1.x = 2*cur.x - ct1.x;
			ct1.y = 2*cur.y - ct1.y;
			if (has_ci) {
				ct2 = pts[coordIndex->vals[ci]];
				pt = pts[coordIndex->vals[ci+1]];
			} else {
				ct2 = pts[ci];
				pt = pts[ci+1];
			}
			if (fill_item) gf_path_add_cubic_to(fill_item->path, ct1.x, ct1.y, ct2.x, ct2.y, pt.x, pt.y);
			if (line_item) gf_path_add_cubic_to(line_item->path, ct1.x, ct1.y, ct2.x, ct2.y, pt.x, pt.y);
			ct1 = ct2;
			cur = pt;
			ci += 2;
			break;
		/*quadratic CurveTo*/
		case 7:
			if ((has_ci && ci + 1 >= coordIndex->count) || (!has_ci && ci + 1>= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				gf_path_add_move_to(fill_item->path, cur.x, cur.y);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				gf_path_add_move_to(line_item->path, cur.x, cur.y);
				need_line = GF_FALSE;
			}
			if (has_ci) {
				ct1 = pts[coordIndex->vals[ci]];
				pt = pts[coordIndex->vals[ci+1]];
			} else {
				ct1 = pts[ci];
				pt = pts[ci+1];
			}
			if (fill_item) gf_path_add_quadratic_to(fill_item->path, ct1.x, ct1.y, pt.x, pt.y);
			if (line_item) gf_path_add_quadratic_to(line_item->path, ct1.x, ct1.y, pt.x, pt.y);
			cur = pt;
			ci += 2;
			break;
		/*quadratic nextCurveTo*/
		case 8:
			if ((has_ci && ci >= coordIndex->count) || (!has_ci && ci >= coords->count) ) return;
			if (need_fill) {
				fill_item = new_fs_item(st, 0, fill_col, 0);
				gf_path_add_move_to(fill_item->path, cur.x, cur.y);
				need_fill = GF_FALSE;
			}
			if (need_line) {
				line_item = new_fs_item(st, line_col, 0, w);
				gf_path_add_move_to(line_item->path, cur.x, cur.y);
				need_line = GF_FALSE;
			}
			ct1.x = 2*cur.x - ct1.x;
			ct1.y = 2*cur.y - ct1.y;
			if (has_ci) {
				pt = pts[coordIndex->vals[ci]];
			} else {
				pt = pts[ci];
			}
			if (fill_item) gf_path_add_quadratic_to(fill_item->path, ct1.x, ct1.y, pt.x, pt.y);
			if (line_item) gf_path_add_quadratic_to(line_item->path, ct1.x, ct1.y, pt.x, pt.y);
			cur = pt;
			ci += 2;
			break;
		/*close*/
		case 9:
			if (fill_item) gf_path_close(fill_item->path);
			if (line_item) gf_path_close(line_item->path);
			break;
		}
	}

	/*compute bounds*/
	st->bounds.width = st->bounds.height = 0;
	for (i=0; i<gf_list_count(st->items); i++) {
		line_item = (FSItem*)gf_list_get(st->items, i);
		gf_path_get_bounds(line_item->path, &rc);
		gf_rect_union(&st->bounds, &rc);
	}
}

static void fs_traverse(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 i;
	DrawableContext *ctx;
	FSStack *st = (FSStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState*)rs;

	if (is_destroy) {
		clean_paths(st);
		drawable_del(st->drawable);
		gf_list_del(st->items);
		gf_free(st);
		return;
	}
	/*check for geometry change*/
	if (gf_node_dirty_get(node)) {
		gf_node_dirty_clear(node, 0);
		/*build*/
		clean_paths(st);
		build_shape(st, node);
	}

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		ctx = tr_state->ctx;
		for (i=0; i<gf_list_count(st->items); i++) {
			FSItem *item = (FSItem*)gf_list_get(st->items, i);
			ctx->flags &= ~(CTX_PATH_FILLED | CTX_PATH_STROKE);
			memset(&ctx->aspect, 0, sizeof(DrawAspect2D));
			if (item->fill_col) {
				ctx->aspect.fill_color = item->fill_col;
			}
			if (item->width) {
				ctx->aspect.line_color = item->line_col;
				ctx->aspect.pen_props.width = item->width;
			}
			visual_2d_draw_path(tr_state->visual, item->path, ctx, NULL, NULL, tr_state);
		}
		return;

#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		ctx = tr_state->ctx;
		for (i=0; i<gf_list_count(st->items); i++) {
			FSItem *item = (FSItem*)gf_list_get(st->items, i);
			memset(&ctx->aspect, 0, sizeof(DrawAspect2D));
			if (item->fill_col) {
				ctx->aspect.fill_color = item->fill_col;
			}
			if (item->width) {
				ctx->aspect.line_color = item->line_col;
				ctx->aspect.pen_props.width = item->width;
			}
			if (!item->mesh) {
				item->mesh = new_mesh();
				mesh_from_path(item->mesh, item->path);
			}
			st->drawable->mesh = item->mesh;
			visual_3d_draw_2d_with_aspect(st->drawable, tr_state, &ctx->aspect);
			st->drawable->mesh = NULL;
		}
		return;
#endif
	case TRAVERSE_PICK:
		/*todo*/
		return;
	case TRAVERSE_GET_BOUNDS:
		tr_state->bounds = st->bounds;
		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		/*finalize*/
#ifndef GPAC_DISABLE_VRML
		ctx = drawable_init_context_mpeg4(st->drawable, tr_state);
		if (!ctx) return;

		/*force width to max width used for clipper compute*/
		if (st->max_width) {
			ctx->aspect.pen_props.width = st->max_width;
		}
		drawable_finalize_sort(ctx, tr_state, &st->bounds);
		break;
#else
		return;
#endif
	}
}


void compositor_init_hc_flashshape(GF_Compositor *compositor, GF_Node *node)
{
	FSStack *stack;

	GF_SAFEALLOC(stack, FSStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate flashshape proto stack\n"));
		return;
	}
	stack->drawable = drawable_new();
	stack->drawable->node = node;
	stack->drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->items = gf_list_new();

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, fs_traverse);
}

#endif // defined(GPAC_ENABLE_FLASHSHAPE) && !defined(GPAC_DISABLE_COMPOSITOR)
