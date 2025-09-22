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

#include "visual_manager.h"
#include "nodes_stacks.h"

#if !defined(GPAC_DISABLE_COMPOSITOR)

#include <gpac/nodes_mpeg4.h>
#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg.h>
#endif

static Bool visual_draw_bitmap_stub(GF_VisualManager *visual, GF_TraverseState *tr_state, struct _drawable_context *ctx)
{
	return 0;
}


GF_VisualManager *visual_new(GF_Compositor *compositor)
{
	GF_VisualManager *tmp;
	GF_SAFEALLOC(tmp, GF_VisualManager);
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate new visual\n"));
		return NULL;
	}

	tmp->center_coords = 1;
	tmp->compositor = compositor;
	ra_init(&tmp->to_redraw);
#ifndef GPAC_DISABLE_VRML
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();
#endif

	tmp->raster_brush = gf_evg_stencil_new(GF_STENCIL_SOLID);

	tmp->DrawBitmap = visual_draw_bitmap_stub;
	tmp->ClearSurface = visual_2d_clear_surface;

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		visual_draw_bitmap_stub(tmp, NULL, NULL);
		visual_reset_graphics(NULL);
	}
#endif

#ifndef GPAC_DISABLE_3D

#ifndef GPAC_DISABLE_VRML
	tmp->navigation_stack = gf_list_new();
	tmp->fog_stack = gf_list_new();
#endif /*GPAC_DISABLE_VRML*/
	tmp->alpha_nodes_to_draw = gf_list_new();
	tmp->compiled_programs = gf_list_new();
#endif

	return tmp;
}

void visual_del(GF_VisualManager *visual)
{
	ra_del(&visual->to_redraw);

	if (visual->raster_surface) gf_evg_surface_delete(visual->raster_surface);
	visual->raster_surface = NULL;
	if (visual->raster_brush) gf_evg_stencil_delete(visual->raster_brush);
	visual->raster_brush = NULL;

	while (visual->context) {
		DrawableContext *ctx = visual->context;
		visual->context = ctx->next;
		DeleteDrawableContext(ctx);
	}
	while (visual->prev_nodes) {
		struct _drawable_store *cur = visual->prev_nodes;
		visual->prev_nodes = cur->next;
		gf_free(cur);
	}

#ifndef GPAC_DISABLE_VRML
	if (visual->back_stack) BindableStackDelete(visual->back_stack);
	if (visual->view_stack) BindableStackDelete(visual->view_stack);
#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_3D
	visual_3d_reset_graphics(visual);
	ra_del(&visual->hybgl_drawn);

#ifndef GPAC_DISABLE_VRML
	if (visual->navigation_stack) BindableStackDelete(visual->navigation_stack);
	if (visual->fog_stack) BindableStackDelete(visual->fog_stack);
#endif /*GPAC_DISABLE_VRML*/

	gf_list_del(visual->alpha_nodes_to_draw);
	gf_list_del(visual->compiled_programs);
#endif
	gf_free(visual);
}

Bool visual_get_size_info(GF_TraverseState *tr_state, Fixed *surf_width, Fixed *surf_height)
{
	Fixed w, h;
//	w = tr_state->visual->width;
//	h = tr_state->visual->height;
	w = tr_state->vp_size.x;
	h = tr_state->vp_size.y;
	/*no size info, use main compositor output size*/
	if (!w || !h) {
		w = INT2FIX(tr_state->visual->compositor->vp_width);
		h = INT2FIX(tr_state->visual->compositor->vp_height);
	}
	if (tr_state->pixel_metrics) {
		*surf_width = w;
		*surf_height = h;
		return 1;
	}
	if (tr_state->min_hsize) {
		*surf_width = gf_divfix(w, tr_state->min_hsize);
		*surf_height = gf_divfix(h, tr_state->min_hsize);
		return 0;
	}
	if (h > w) {
		*surf_width = 2*FIX_ONE;
		*surf_height = gf_divfix(2*h, w);
	} else {
		*surf_width = gf_divfix(2*w, h);
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
			gf_node_dirty_reset(ctx->appear, 0);

#ifndef GPAC_DISABLE_3D
		/*this may happen when switching a visual from 2D to 3D - discard context*/
		if (visual->type_3d) ctx->drawable=NULL;
#endif
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
				gf_node_dirty_reset(ctx->appear, 0);

			ctx->drawable = NULL;
			ctx = ctx->next;
		}
	}
}

Bool visual_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d) {
		Bool res = visual_3d_draw_frame(visual, root, tr_state, is_root_visual);
		visual_clean_contexts(visual);
		return res;
	}
	if (visual->compositor->hybrid_opengl)
		visual_3d_clean_state(visual);
#endif
	return visual_2d_draw_frame(visual, root, tr_state, is_root_visual);
}

void gf_sc_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state, s32 *child_idx)
{
	u32 i;
	SFVec2f size;
	GF_Rect rc;
	GF_Matrix2D cur_mx;

	if (tr_state->abort_bounds_traverse) {
		if (self == tr_state->for_node) {
			gf_mx2d_pre_multiply(&tr_state->mx_at_node, &tr_state->transform);
		}
		tr_state->abort_bounds_traverse=0;
		gf_sc_get_nodes_bounds(self, children, tr_state, child_idx);
		tr_state->abort_bounds_traverse=1;
		return;
	}
	if (!children) return;

	size.x = size.y = -FIX_ONE;
#ifndef GPAC_DISABLE_VRML
	switch (gf_node_get_tag(self)) {
	case TAG_MPEG4_Layer2D:
		size = ((M_Layer2D *)self)->size;
		break;
	case TAG_MPEG4_Layer3D:
		size = ((M_Layer3D *)self)->size;
		break;
	case TAG_MPEG4_Form:
		size = ((M_Form *)self)->size;
		break;
	}
#endif
	if ((size.x>=0) && (size.y>=0)) {
		tr_state->bounds = gf_rect_center(size.x, size.y);
		return;
	}

	gf_mx2d_copy(cur_mx, tr_state->transform);
	rc = gf_rect_center(0,0);

	i = 0;
	while (children) {
		if (child_idx && (i != (u32) *child_idx)) {
			children = children->next;
			continue;
		}
		gf_mx2d_init(tr_state->transform);
		tr_state->bounds = gf_rect_center(0,0);

		/*we hit the target node*/
		if (children->node == tr_state->for_node)
			tr_state->abort_bounds_traverse = 1;

		gf_node_traverse(children->node, tr_state);

		if (tr_state->abort_bounds_traverse) {
			gf_mx2d_add_matrix(&tr_state->mx_at_node, &cur_mx);
			return;
		}

		gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
		gf_rect_union(&rc, &tr_state->bounds);
		children = children->next;
		if (child_idx)
			break;
	}

#ifndef GPAC_DISABLE_SVG
	if (gf_node_get_tag(self)==TAG_SVG_use) {
		GF_FieldInfo info;
		if (gf_node_get_attribute_by_tag(self, TAG_XLINK_ATT_href, 0, 0, &info)==GF_OK) {
			GF_Node *iri = ((XMLRI*)info.far_ptr)->target;
			if (iri) {
				gf_mx2d_init(tr_state->transform);
				tr_state->bounds = gf_rect_center(0,0);

				/*we hit the target node*/
				if (iri == tr_state->for_node)
					tr_state->abort_bounds_traverse = 1;

				gf_node_traverse(iri, tr_state);

				if (tr_state->abort_bounds_traverse) {
					gf_mx2d_pre_multiply(&tr_state->mx_at_node, &cur_mx);
					return;
				}

				gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
				gf_rect_union(&rc, &tr_state->bounds);
			}
		}
	}
#endif

	gf_mx2d_copy(tr_state->transform, cur_mx);
	if (self != tr_state->for_node) {
		gf_mx2d_apply_rect(&tr_state->transform, &rc);
	}
	tr_state->bounds = rc;
}

void visual_reset_graphics(GF_VisualManager *visual)
{
	if (!visual) return;
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d) {
		visual_3d_reset_graphics(visual);
	}
	compositor_2d_reset_gl_auto(visual->compositor);
#endif
}

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
