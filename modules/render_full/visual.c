/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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

GF_VisualManager *visual_new(Render *sr)
{
	GF_VisualManager *tmp;
	GF_SAFEALLOC(tmp, GF_VisualManager);

	tmp->center_coords = 1;
	tmp->render = sr;
	ra_init(&tmp->to_redraw);
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();

#ifndef GPAC_DISABLE_3D
	tmp->navigation_stack = gf_list_new();
	tmp->fog_stack = gf_list_new();
	tmp->alpha_nodes_to_draw = gf_list_new();
#endif

	return tmp;
}

void visual_del(GF_VisualManager *vis)
{
	ra_del(&vis->to_redraw);
	visual_2d_reset_raster(vis);

	while (vis->context) {
		DrawableContext *ctx = vis->context;
		vis->context = ctx->next;
		DeleteDrawableContext(ctx);
	}
	while (vis->prev_nodes) {
		struct _drawable_store *cur = vis->prev_nodes;
		vis->prev_nodes = cur->next;
		free(cur);
	}

	if (vis->back_stack) gf_list_del(vis->back_stack);
	if (vis->view_stack) gf_list_del(vis->view_stack);

#ifndef GPAC_DISABLE_3D
	if (vis->navigation_stack) gf_list_del(vis->navigation_stack);
	if (vis->fog_stack) gf_list_del(vis->fog_stack);
	gf_list_del(vis->alpha_nodes_to_draw);
#endif
	free(vis);
}

Bool visual_get_size_info(GF_TraverseState *tr_state, Fixed *surf_width, Fixed *surf_height)
{
	u32 w, h;
	w = tr_state->visual->width;
	h = tr_state->visual->height;
	/*no size info, use main render output size*/
	if (!w || !h) {
		w = tr_state->visual->render->vp_width;
		h = tr_state->visual->render->vp_height;
	}
	if (tr_state->is_pixel_metrics) {
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


Bool visual_render_frame(GF_VisualManager *vis, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
#ifndef GPAC_DISABLE_3D
	if (vis->type_3d) return visual_3d_render_frame(vis, root, tr_state, is_root_visual);
#endif
	return visual_2d_render_frame(vis, root, tr_state, is_root_visual);
}

