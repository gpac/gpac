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



#include "node_stacks.h"
#include "grouping.h"
#include "visual_manager.h"


typedef struct
{
	GROUPING_NODE_STACK_2D
	GF_List *backs;
	GF_List *views;
	Bool first;
	GF_Rect clip;
} Layer2DStack;


Bool render_pick_in_clipper(GF_TraverseState *tr_state, GF_Rect *clip)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		SFVec3f pos;
		GF_Matrix mx;
		GF_Ray r;
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);
		r = tr_state->ray;
		gf_mx_apply_ray(&mx, &r);
		if (!render_get_2d_plane_intersection(&r, &pos)) return 0;
		if ( (pos.x < clip->x) || (pos.y > clip->y) 
			|| (pos.x > clip->x + clip->width) || (pos.y < clip->y - clip->height) ) return 0;

	} else 
#endif
	{
		GF_Rect rc = *clip;
		GF_Point2D pt;
		gf_mx2d_apply_rect(&tr_state->transform, &rc);
		pt.x = tr_state->ray.orig.x;
		pt.y = tr_state->ray.orig.y;

		if ( (pt.x < rc.x) || (pt.y > rc.y) 
			|| (pt.x > rc.x + rc.width) || (pt.y < rc.y - rc.height) ) return 0;
	}
	return 1;
}

static void l2d_CheckBindables(GF_Node *n, GF_TraverseState *tr_state, Bool force_render)
{
	GF_Node *btop;
	M_Layer2D *l2d;
	l2d = (M_Layer2D *)n;
	if (force_render) gf_node_render(l2d->background, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
	if (btop != l2d->background) { 
		gf_node_unregister(l2d->background, n);
		gf_node_register(btop, n); 
		l2d->background = btop;
		gf_node_event_out_str(n, "background");
	}
	if (force_render) gf_node_render(l2d->viewport, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
	if (btop != l2d->viewport) { 
		gf_node_unregister(l2d->viewport, n);
		gf_node_register(btop, n); 
		l2d->viewport = btop;
		gf_node_event_out_str(n, "viewport");
	}
}

static void RenderLayer2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_List *oldb, *oldv;
	GF_Node *viewport;
	GF_Node *back;

#ifndef GPAC_DISABLE_3D
	GF_List *oldf, *oldn;
	GF_List *node_list_backup;
	GF_Rect prev_clipper;
	Bool had_clip;
#endif
	
	M_Layer2D *l = (M_Layer2D *)node;
	Layer2DStack *st = (Layer2DStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	
	if (is_destroy) {
		gf_list_del(st->backs);
		gf_list_del(st->views);
		free(st);
		return;
	}

	/*layers can only be rendered in a 2D context*/
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && tr_state->camera->is_3D) return;
#endif

	/*layer2D maintains its own stacks*/
	oldb = tr_state->backgrounds;
	oldv = tr_state->viewpoints;
	tr_state->backgrounds = st->backs;
	tr_state->viewpoints = st->views;
#ifndef GPAC_DISABLE_3D
	oldf = tr_state->fogs;
	oldn = tr_state->navigations;
	tr_state->fogs = tr_state->navigations = NULL;
#endif

	l2d_CheckBindables(node, tr_state, st->first);

	back = (GF_Node*)gf_list_get(st->backs, 0);

	viewport = (GF_Node*)gf_list_get(st->views, 0);

	if (gf_node_dirty_get(node)) {
		/*recompute grouping node state*/
		if (gf_node_dirty_get(node) && GF_SG_CHILD_DIRTY) 
			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
		
		/*override group bounds*/
		visual_get_size_info(tr_state, &st->clip.width, &st->clip.height);
		/*setup bounds in local coord system*/
		if (l->size.x>=0) st->clip.width = l->size.x;
		if (l->size.y>=0) st->clip.height = l->size.y;
		st->clip = gf_rect_center(st->clip.width, st->clip.height);
		st->bounds = st->clip;
	}
	

	switch (tr_state->traversing_mode) {
	case TRAVERSE_RENDER:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			tr_state->layer_clipper = render_2d_update_clipper(tr_state, st->clip, &had_clip, &prev_clipper, 1);

			visual_3d_matrix_push(tr_state->visual);
			/*setup clipping*/
			visual_3d_set_clipper_2d(tr_state->visual, tr_state->layer_clipper);
			
			/*apply background BEFORE viewport*/
			if (back) {
				tr_state->traversing_mode = TRAVERSE_RENDER_BINDABLE;
				gf_bbox_from_rect(&tr_state->bbox, &st->clip);
				gf_node_render(back, tr_state);
			}

			/*sort all children without transform, and use current transform when flushing contexts*/
			gf_mx_init(tr_state->model_matrix);

			/*apply viewport*/
			if (viewport) {
				tr_state->traversing_mode = TRAVERSE_RENDER_BINDABLE;
				gf_bbox_from_rect(&tr_state->bbox, &st->clip);
				gf_node_render(viewport, tr_state);
				visual_3d_matrix_add(tr_state->visual, tr_state->model_matrix.m);
			}

			node_list_backup = tr_state->visual->alpha_nodes_to_draw;
			tr_state->visual->alpha_nodes_to_draw = gf_list_new();
			tr_state->traversing_mode = TRAVERSE_RENDER;
			/*reset cull flag*/
			tr_state->cull_flag = 0;
			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);

			visual_3d_flush_contexts(tr_state->visual, tr_state);

			assert(!gf_list_count(tr_state->visual->alpha_nodes_to_draw));
			gf_list_del(tr_state->visual->alpha_nodes_to_draw);
			tr_state->visual->alpha_nodes_to_draw = node_list_backup;

			
			visual_3d_matrix_pop(tr_state->visual);

			visual_3d_reset_clipper_2d(tr_state->visual);

			tr_state->has_layer_clip = had_clip;
			if (had_clip) {
				tr_state->layer_clipper = prev_clipper;
				visual_3d_set_clipper_2d(tr_state->visual, tr_state->layer_clipper);
			}
		} else 
#endif
		{
			GF_IRect prev_clip;
			GF_Rect rc;
			if (viewport) {
				tr_state->traversing_mode = TRAVERSE_RENDER_BINDABLE;
				tr_state->bounds = st->clip;
				gf_node_render(viewport, tr_state);
			}

			prev_clip = tr_state->visual->top_clipper;
			rc = st->clip;
			gf_mx2d_apply_rect(&tr_state->transform, &rc);
			tr_state->visual->top_clipper = gf_rect_pixelize(&rc);
			gf_irect_intersect(&tr_state->visual->top_clipper, &prev_clip);
			
			tr_state->traversing_mode = TRAVERSE_RENDER;
			if (back && Bindable_GetIsBound(back) ) {
				DrawableContext *ctx;

				ctx = b2d_get_context((M_Background2D*) back, st->backs);
				gf_mx2d_init(ctx->transform);
				ctx->bi->clip = tr_state->visual->top_clipper;
				ctx->bi->unclip = rc;

				if (tr_state->trav_flags & TF_RENDER_DIRECT) {
					tr_state->ctx = ctx;
					tr_state->traversing_mode = TRAVERSE_DRAW_2D;
					gf_node_render(back, tr_state);
					tr_state->traversing_mode = TRAVERSE_RENDER;
					tr_state->ctx = NULL;
				} else {
					DrawableContext *back_ctx = visual_2d_get_drawable_context(tr_state->visual);

					gf_node_render(back, tr_state);

					back_ctx->flags = ctx->flags;
					back_ctx->flags &= ~CTX_IS_TRANSPARENT;
					back_ctx->flags |= CTX_IS_BACKGROUND;
					back_ctx->aspect = ctx->aspect;
					back_ctx->drawable = ctx->drawable;
					drawable_check_bounds(back_ctx, tr_state->visual);
					back_ctx->bi->clip = ctx->bi->clip;
					back_ctx->bi->unclip = ctx->bi->unclip;
				}
				/*keep track of node drawn, whether direct or indirect rendering*/
				if (!(ctx->drawable->flags & DRAWABLE_REGISTERED_WITH_VISUAL) ) {
					struct _drawable_store *it;
					GF_SAFEALLOC(it, struct _drawable_store);
					it->drawable = ctx->drawable;
					if (tr_state->visual->last_prev_entry) {
						tr_state->visual->last_prev_entry->next = it;
						tr_state->visual->last_prev_entry = it;
					} else {
						tr_state->visual->prev_nodes = tr_state->visual->last_prev_entry = it;
					}
					GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Registering new drawn node %s on visual\n", gf_node_get_class_name(it->drawable->node)));
					ctx->drawable->flags |= DRAWABLE_REGISTERED_WITH_VISUAL;
				}
			}

			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
			tr_state->visual->top_clipper = prev_clip;
		}
		break;
		
		/*check picking - we must fall in our 2D clipper*/
	case TRAVERSE_PICK:
		if (render_pick_in_clipper(tr_state, &st->clip)) 
			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
		break;
	case TRAVERSE_GET_BOUNDS:
		tr_state->bounds = st->clip;
#ifndef GPAC_DISABLE_3D
		gf_bbox_from_rect(&tr_state->bbox, &st->clip);
#endif
		break;

#ifndef GPAC_DISABLE_3D
	/*drawing a layer means drawing all sub-elements as a whole (no depth sorting with parents)*/
	case TRAVERSE_DRAW_3D:
		assert(0);
		break;
#endif
	}
	
	/*restore traversing state*/
	tr_state->backgrounds = oldb;
	tr_state->viewpoints = oldv;
#ifndef GPAC_DISABLE_3D
	tr_state->fogs = oldf;
	tr_state->navigations = oldn;
#endif

	/*in case we missed bindables*/
	if (st->first) {
		st->first = 0;
		gf_sr_invalidate(tr_state->visual->render->compositor, NULL);
	}
}

void render_init_layer2d(Render *sr, GF_Node *node)
{
	Layer2DStack *stack;
	GF_SAFEALLOC(stack, Layer2DStack);

	stack->backs = gf_list_new();
	stack->views = gf_list_new();
	stack->first = 1;

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderLayer2D);
}


