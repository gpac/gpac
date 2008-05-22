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

#include "visual_manager.h"
#include <gpac/nodes_mpeg4.h>


static Bool visual_draw_bitmap_stub(GF_VisualManager *visual, GF_TraverseState *tr_state, struct _drawable_context *ctx, GF_ColorKey *col_key)
{
	return 0;
}

GF_VisualManager *visual_new(GF_Compositor *compositor)
{
	GF_VisualManager *tmp;
	GF_SAFEALLOC(tmp, GF_VisualManager);

	tmp->center_coords = 1;
	tmp->compositor = compositor;
	ra_init(&tmp->to_redraw);
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();
	tmp->raster_brush = compositor->rasterizer->stencil_new(compositor->rasterizer, GF_STENCIL_SOLID);

	tmp->DrawBitmap = visual_draw_bitmap_stub;

#ifndef GPAC_DISABLE_3D
	tmp->navigation_stack = gf_list_new();
	tmp->fog_stack = gf_list_new();
	tmp->alpha_nodes_to_draw = gf_list_new();
#endif

	return tmp;
}

void visual_del(GF_VisualManager *visual)
{
	ra_del(&visual->to_redraw);

	if (visual->raster_surface) visual->compositor->rasterizer->surface_delete(visual->raster_surface);
	visual->raster_surface = NULL;
	if (visual->raster_brush) visual->compositor->rasterizer->stencil_delete(visual->raster_brush);
	visual->raster_brush = NULL;

	while (visual->context) {
		DrawableContext *ctx = visual->context;
		visual->context = ctx->next;
		DeleteDrawableContext(ctx);
	}
	while (visual->prev_nodes) {
		struct _drawable_store *cur = visual->prev_nodes;
		visual->prev_nodes = cur->next;
		free(cur);
	}

	if (visual->back_stack) gf_list_del(visual->back_stack);
	if (visual->view_stack) gf_list_del(visual->view_stack);

#ifndef GPAC_DISABLE_3D
	if (visual->navigation_stack) gf_list_del(visual->navigation_stack);
	if (visual->fog_stack) gf_list_del(visual->fog_stack);
	gf_list_del(visual->alpha_nodes_to_draw);
#endif
	free(visual);
}

Bool visual_get_size_info(GF_TraverseState *tr_state, Fixed *surf_width, Fixed *surf_height)
{
	u32 w, h;
	w = tr_state->visual->width;
	h = tr_state->visual->height;
	/*no size info, use main compositor output size*/
	if (!w || !h) {
		w = tr_state->visual->compositor->vp_width;
		h = tr_state->visual->compositor->vp_height;
	}
	if (tr_state->pixel_metrics) {
		*surf_width = INT2FIX(w);
		*surf_height = INT2FIX(h);
		return 1;
	}
	if (h > w) {
		*surf_width = 2*FIX_ONE;
		*surf_height = gf_divfix(2*INT2FIX(h), INT2FIX(w));
	} else {
		*surf_width = gf_divfix(2*INT2FIX(w), INT2FIX(h));
		*surf_height = 2*FIX_ONE;
	}
	return 0;
}

void visual_clean_contexts(GF_VisualManager *visual)
{
	u32 i, count;
	Bool is_root_visual = (visual->compositor->visual==visual) ? 1 : 0;
	DrawableContext *ctx = visual->context;
	while (ctx && ctx->drawable) {
		/*remove visual registration flag*/
		ctx->drawable->flags &= ~DRAWABLE_REGISTERED_WITH_VISUAL;
		if (is_root_visual && (ctx->flags & CTX_HAS_APPEARANCE)) 
			gf_node_dirty_reset(ctx->appear);
		ctx = ctx->next;
	}

	/*composite visual, cannot reset flags until root is done*/
	if (!is_root_visual) return;

	/*reset all flags of all appearance nodes registered on all visuals but main one (done above)
	this must be done once all visuals have been drawn, otherwise we won't detect the changes 
	for nodes drawn on several visuals*/
	count = gf_list_count(visual->compositor->visuals);
	for (i=1; i<count; i++) {
		GF_VisualManager *a_vis = gf_list_get(visual->compositor->visuals, i);
		ctx = a_vis->context;
		while (ctx && ctx->drawable) {
			if (ctx->flags & CTX_HAS_APPEARANCE) 
				gf_node_dirty_reset(ctx->appear);
			ctx = ctx->next;
		}
	}
}

Bool visual_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d) {
		GF_Err e = visual_3d_draw_frame(visual, root, tr_state, is_root_visual);
		visual_clean_contexts(visual);
		return e;
	}
#endif
	return visual_2d_draw_frame(visual, root, tr_state, is_root_visual);
}

void gf_sc_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	SFVec2f size;
	GF_Rect rc;
	GF_Matrix2D cur_mx;

	if (tr_state->abort_bounds_traverse) return;

	size.x = size.y = -FIX_ONE;
	switch (gf_node_get_tag(self)) {
	case TAG_MPEG4_Layer2D: size = ((M_Layer2D *)self)->size; break;
	case TAG_MPEG4_Layer3D: size = ((M_Layer3D *)self)->size; break;
	case TAG_MPEG4_Form: size = ((M_Form *)self)->size; break;
	}
	if ((size.x>=0) && (size.y>=0)) {
		tr_state->bounds = gf_rect_center(size.x, size.y);
		return;
	}

	gf_mx2d_copy(cur_mx, tr_state->transform);
	rc = gf_rect_center(0,0);
	while (children) {
		gf_mx2d_init(tr_state->transform);
		tr_state->bounds = gf_rect_center(0,0);

		gf_node_traverse(children->node, tr_state);
		/*we hit the target node*/
		if (children->node == tr_state->for_node) {
			tr_state->abort_bounds_traverse = 1;
			gf_mx_from_mx2d(&tr_state->visual->compositor->hit_world_to_local, &tr_state->transform);
			return;
		}
		if (tr_state->abort_bounds_traverse) return;

		gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
		gf_rect_union(&rc, &tr_state->bounds);
		children = children->next;
	}
	gf_mx2d_copy(tr_state->transform, cur_mx);
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	tr_state->bounds = rc;
}

