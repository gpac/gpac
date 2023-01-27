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
#include "mpeg4_grouping.h"
#include "visual_manager.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)


typedef struct
{
	GROUPING_MPEG4_STACK_2D
	GF_List *backs;
	GF_List *views;
	Bool first;
	GF_Rect clip;
} Layer2DStack;



static void l2d_CheckBindables(GF_Node *n, GF_TraverseState *tr_state, Bool force_traverse)
{
	GF_Node *btop;
	M_Layer2D *l2d;
	l2d = (M_Layer2D *)n;
	if (force_traverse) gf_node_traverse(l2d->background, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
	if (btop != l2d->background) {
		gf_node_unregister(l2d->background, n);
		gf_node_register(btop, n);
		l2d->background = btop;
		gf_node_event_out(n, 4/*"background"*/);
	}
	if (force_traverse) gf_node_traverse(l2d->viewport, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
	if (btop != l2d->viewport) {
		gf_node_unregister(l2d->viewport, n);
		gf_node_register(btop, n);
		l2d->viewport = btop;
		gf_node_event_out(n, 5/*"viewport"*/);
	}
}

static void TraverseLayer2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_List *oldb, *oldv;
	GF_Node *viewport;
	GF_Node *back;
	Bool prev_layer;
	GF_Matrix2D backup;
	GF_IRect prev_clip;
	GF_Rect rc;
	SFVec2f prev_vp;

#ifndef GPAC_DISABLE_3D
	GF_Matrix mx3d, prev_layer_mx;
	GF_List *oldf, *oldn;
	GF_List *node_list_backup;
	GF_Rect prev_clipper;
	Bool had_clip;
#endif

	M_Layer2D *l = (M_Layer2D *)node;
	Layer2DStack *st = (Layer2DStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		BindableStackDelete(st->backs);
		BindableStackDelete(st->views);
		group_2d_destroy(node, (GroupingNode2D*)st);
		gf_free(st);
		return;
	}

	/*layers can only be used in a 2D context*/
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && tr_state->camera && tr_state->camera->is_3D) return;
#endif

	/*layer2D maintains its own stacks*/
	oldb = tr_state->backgrounds;
	oldv = tr_state->viewpoints;
	tr_state->backgrounds = st->backs;
	tr_state->viewpoints = st->views;
	prev_layer = tr_state->is_layer;
	tr_state->is_layer = 1;
#ifndef GPAC_DISABLE_3D
	oldf = tr_state->fogs;
	oldn = tr_state->navigations;
	tr_state->fogs = tr_state->navigations = NULL;
#endif

	l2d_CheckBindables(node, tr_state, st->first);

	back = (GF_Node*)gf_list_get(st->backs, 0);

	viewport = (GF_Node*)gf_list_get(st->views, 0);

	if ((tr_state->traversing_mode == TRAVERSE_SORT) || (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS)) {
		/*override group bounds*/
		visual_get_size_info(tr_state, &st->clip.width, &st->clip.height);
		/*setup bounds in local coord system*/
		if (l->size.x>=0) st->clip.width = l->size.x;
		if (l->size.y>=0) st->clip.height = l->size.y;
		st->clip = gf_rect_center(st->clip.width, st->clip.height);
		st->bounds = st->clip;
	}

	prev_vp = tr_state->vp_size;
	tr_state->vp_size.x = st->clip.width;
	tr_state->vp_size.y = st->clip.height;

	switch (tr_state->traversing_mode) {
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			gf_mx_copy(prev_layer_mx, tr_state->layer_matrix);
			tr_state->layer_clipper = compositor_2d_update_clipper(tr_state, st->clip, &had_clip, &prev_clipper, 1);

			gf_mx_copy(mx3d, tr_state->model_matrix);

			/*setup clipping*/
			if (had_clip) {
				visual_3d_reset_clipper_2d(tr_state->visual);
			}
			visual_3d_set_clipper_2d(tr_state->visual, tr_state->layer_clipper, &mx3d);

			/*apply background BEFORE viewport*/
			if (back) {
				tr_state->traversing_mode = TRAVERSE_BINDABLE;
				gf_bbox_from_rect(&tr_state->bbox, &st->clip);
				gf_node_traverse(back, tr_state);
			}

			/*apply viewport*/
			if (viewport) {
				tr_state->traversing_mode = TRAVERSE_BINDABLE;
				tr_state->bounds = st->clip;
				gf_node_traverse(viewport, tr_state);
			}


			node_list_backup = tr_state->visual->alpha_nodes_to_draw;
			tr_state->visual->alpha_nodes_to_draw = gf_list_new();
			tr_state->traversing_mode = TRAVERSE_SORT;
			/*reset cull flag*/
			tr_state->cull_flag = 0;
			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);

			visual_3d_flush_contexts(tr_state->visual, tr_state);
			tr_state->traversing_mode = TRAVERSE_SORT;

			assert(!gf_list_count(tr_state->visual->alpha_nodes_to_draw));
			gf_list_del(tr_state->visual->alpha_nodes_to_draw);
			tr_state->visual->alpha_nodes_to_draw = node_list_backup;

			gf_mx_copy(tr_state->model_matrix, mx3d);

			visual_3d_reset_clipper_2d(tr_state->visual);

			tr_state->has_layer_clip = had_clip;
			if (had_clip) {
				tr_state->layer_clipper = prev_clipper;
				gf_mx_copy(tr_state->layer_matrix, prev_layer_mx);
				visual_3d_set_clipper_2d(tr_state->visual, tr_state->layer_clipper, &prev_layer_mx);
			}
		} else
#endif
		{
			gf_mx2d_copy(backup, tr_state->transform);

			prev_clip = tr_state->visual->top_clipper;
			rc = st->clip;

			/*get clipper in world coordinate*/
			gf_mx2d_apply_rect(&tr_state->transform, &rc);

			if (viewport) {
				tr_state->traversing_mode = TRAVERSE_BINDABLE;
				tr_state->bounds = st->clip;
				gf_node_traverse(viewport, tr_state);
#if VIEWPORT_CLIPS
				/*move viewport box in world coordinate*/
				gf_mx2d_apply_rect(&backup, &tr_state->bounds);
				/*and intersect with layer clipper*/
				gf_rect_intersect(&rc, &tr_state->bounds);
#endif
			}

			rc.x -= FIX_ONE;
			rc.width += 2*FIX_ONE;
			rc.y += FIX_ONE;
			rc.height += 2*FIX_ONE;
			tr_state->visual->top_clipper = gf_rect_pixelize(&rc);
			gf_irect_intersect(&tr_state->visual->top_clipper, &prev_clip);
			tr_state->traversing_mode = TRAVERSE_SORT;

			if (tr_state->visual->top_clipper.width && tr_state->visual->top_clipper.height) {
				if (back && Bindable_GetIsBound(back) ) {
					DrawableContext *ctx;

					ctx = b2d_get_context((M_Background2D*) back, st->backs);
					gf_mx2d_init(ctx->transform);
					ctx->bi->clip = tr_state->visual->top_clipper;
					ctx->bi->unclip = rc;

					if (tr_state->immediate_draw) {
						tr_state->ctx = ctx;
						tr_state->traversing_mode = TRAVERSE_DRAW_2D;
						gf_node_traverse(back, tr_state);
						tr_state->traversing_mode = TRAVERSE_SORT;
						tr_state->ctx = NULL;
					} else {
						DrawableContext *back_ctx = visual_2d_get_drawable_context(tr_state->visual);

						gf_node_traverse(back, tr_state);

						back_ctx->flags = ctx->flags;
						back_ctx->flags &= ~CTX_IS_TRANSPARENT;
						back_ctx->flags |= CTX_IS_BACKGROUND;
						back_ctx->aspect = ctx->aspect;
						back_ctx->drawable = ctx->drawable;
						drawable_check_bounds(back_ctx, tr_state->visual);
						back_ctx->bi->clip = ctx->bi->clip;
						back_ctx->bi->unclip = ctx->bi->unclip;
					}
					/*keep track of node drawn*/
					if (!(ctx->drawable->flags & DRAWABLE_REGISTERED_WITH_VISUAL) ) {
						struct _drawable_store *it;
						GF_SAFEALLOC(it, struct _drawable_store);
						if (!it) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Layer2D] Failed to allocate drawable store\n"));
						} else {
							it->drawable = ctx->drawable;
							if (tr_state->visual->last_prev_entry) {
								tr_state->visual->last_prev_entry->next = it;
								tr_state->visual->last_prev_entry = it;
							} else {
								tr_state->visual->prev_nodes = tr_state->visual->last_prev_entry = it;
							}
							GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Layer2D] Registering new drawn node %s on visual\n", gf_node_get_class_name(it->drawable->node)));
							ctx->drawable->flags |= DRAWABLE_REGISTERED_WITH_VISUAL;
						}
					}
				}

				group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
			}
			tr_state->visual->top_clipper = prev_clip;
			gf_mx2d_copy(tr_state->transform, backup);
		}
		break;

	/*check picking - we must fall in our 2D clipper*/
	case TRAVERSE_PICK:
		if (gf_sc_pick_in_clipper(tr_state, &st->clip)) {

#ifndef GPAC_DISABLE_3D
			if (tr_state->visual->type_3d) {
				/*apply viewport*/
				if (viewport) {
					gf_mx_copy(mx3d, tr_state->model_matrix);
					tr_state->traversing_mode = TRAVERSE_BINDABLE;
					tr_state->bounds = st->clip;
					gf_node_traverse(viewport, tr_state);
					tr_state->traversing_mode = TRAVERSE_PICK;
					group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
					gf_mx_copy(tr_state->model_matrix, mx3d);
				} else {
					group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
				}
			} else
#endif
			{
				if (viewport) {
					gf_mx2d_copy(backup, tr_state->transform);
					tr_state->traversing_mode = TRAVERSE_BINDABLE;
					tr_state->bounds = st->clip;
					gf_node_traverse(viewport, tr_state);
					tr_state->traversing_mode = TRAVERSE_PICK;
					group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
					gf_mx2d_copy(tr_state->transform, backup);
				} else {
					group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
				}
			}
		}
		break;
	case TRAVERSE_GET_BOUNDS:
		if (tr_state->for_node) {
			group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
		} else {
			tr_state->bounds = st->clip;
#ifndef GPAC_DISABLE_3D
			gf_bbox_from_rect(&tr_state->bbox, &st->clip);
#endif
		}
		break;

	case TRAVERSE_DRAW_2D:
		group_2d_traverse(node, (GroupingNode2D *)st, tr_state);
		break;

#ifndef GPAC_DISABLE_3D
	/*drawing a layer means drawing all sub-elements as a whole (no depth sorting with parents)*/
	case TRAVERSE_DRAW_3D:
		assert(0);
		break;
#endif
	}

	/*restore traversing state*/
	tr_state->vp_size = prev_vp;
	tr_state->backgrounds = oldb;
	tr_state->viewpoints = oldv;
	tr_state->is_layer = prev_layer;
#ifndef GPAC_DISABLE_3D
	tr_state->fogs = oldf;
	tr_state->navigations = oldn;
#endif

	/*in case we missed bindables*/
	if (st->first) {
		st->first = 0;
		gf_sc_invalidate(tr_state->visual->compositor, NULL);
	}
}

void compositor_init_layer2d(GF_Compositor *compositor, GF_Node *node)
{
	Layer2DStack *stack;
	GF_SAFEALLOC(stack, Layer2DStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate layer2d stack\n"));
		return;
	}

	stack->backs = gf_list_new();
	stack->views = gf_list_new();
	stack->first = 1;

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseLayer2D);
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
