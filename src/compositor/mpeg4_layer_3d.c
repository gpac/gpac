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
#include "texturing.h"
#include "visual_manager.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR) && !defined(GPAC_DISABLE_3D)

#ifdef GPAC_USE_TINYGL
#include <GL/oscontext.h>
#endif

typedef struct
{
	GROUPING_NODE_STACK_3D
	Bool first;
	GF_Rect clip, vp;
	GF_VisualManager *visual;

	Bool unsupported;
	GF_TextureHandler txh;
	Drawable *drawable;
#ifdef GPAC_USE_TINYGL
	ostgl_context *tgl_ctx;
#endif
	Fixed scale_x, scale_y;
} Layer3DStack;


static void DestroyLayer3D(GF_Node *node)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	GF_Compositor *compositor = st->visual->compositor;

	drawable_del(st->drawable);

#ifdef GPAC_USE_TINYGL
	if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif

	if (st->txh.data) gf_free(st->txh.data);
	/*destroy texture*/
	gf_sc_texture_destroy(&st->txh);

	BindableStackDelete(st->visual->back_stack);
	st->visual->back_stack = NULL;
	BindableStackDelete(st->visual->view_stack);
	st->visual->view_stack = NULL;
	BindableStackDelete(st->visual->navigation_stack);
	st->visual->navigation_stack = NULL;
	BindableStackDelete(st->visual->fog_stack);
	st->visual->fog_stack = NULL;
	visual_del(st->visual);


	if (compositor && compositor->active_layer == node) compositor->active_layer = NULL;
	gf_free(st);
}

static void l3d_CheckBindables(GF_Node *n, GF_TraverseState *tr_state, Bool force_traverse)
{
	GF_Node *btop;
	u32 mode;
	M_Layer3D *l3d;
	l3d = (M_Layer3D *)n;

	mode = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;

	if (force_traverse) gf_node_traverse(l3d->background, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
	if (btop != l3d->background) {
		gf_node_unregister(l3d->background, n);
		gf_node_register(btop, n);
		l3d->background = btop;
		gf_node_event_out(n, 4/*"background"*/);
	}
	if (force_traverse) gf_node_traverse(l3d->viewpoint, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
	if (btop != l3d->viewpoint) {
		gf_node_unregister(l3d->viewpoint, n);
		gf_node_register(btop, n);
		l3d->viewpoint = btop;
		gf_node_event_out(n, 7/*"viewpoint"*/);
	}
	if (force_traverse) gf_node_traverse(l3d->navigationInfo, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->navigations, 0);
	if (btop != l3d->navigationInfo) {
		gf_node_unregister(l3d->navigationInfo, n);
		gf_node_register(btop, n);
		l3d->navigationInfo = btop;
		gf_node_event_out(n, 6/*"navigationInfo"*/);
	}
	if (force_traverse) gf_node_traverse(l3d->fog, tr_state);
	btop = (GF_Node*)gf_list_get(tr_state->fogs, 0);
	if (btop != l3d->fog) {
		gf_node_unregister(l3d->fog, n);
		gf_node_register(btop, n);
		l3d->fog = btop;
		gf_node_event_out(n, 5/*"fog"*/);
	}
	tr_state->traversing_mode = mode;
}


static void layer3d_draw_2d(GF_Node *node, GF_TraverseState *tr_state)
{
	DrawableContext *ctx = tr_state->ctx;
	if (tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx))
		return;

	visual_2d_texture_path(tr_state->visual, ctx->drawable->path, ctx, tr_state);
}

static void layer3d_setup_clip(Layer3DStack *st, GF_TraverseState *tr_state, Bool prev_cam, GF_Rect rc)
{
	Fixed sw, sh;

	if (tr_state->visual->compositor->visual==tr_state->visual) {
		sw = INT2FIX(tr_state->visual->compositor->display_width);
		sh = INT2FIX(tr_state->visual->compositor->display_height);
	} else {
		sw = INT2FIX(tr_state->visual->width);
		sh = INT2FIX(tr_state->visual->height);
	}

	if ((tr_state->traversing_mode==TRAVERSE_SORT) && !prev_cam) {
		rc.x = rc.y = 0;
		st->visual->camera.vp = rc;
	} else {
		st->visual->camera.vp = rc;
		st->visual->camera.vp.x += sw/2;
		st->visual->camera.vp.y -= st->visual->camera.vp.height;
		st->visual->camera.vp.y += sh/2;
	}

	tr_state->camera->width = tr_state->camera->vp.width;
	tr_state->camera->height = tr_state->camera->vp.height;

	if (!tr_state->pixel_metrics) {
		if (tr_state->camera->height > tr_state->camera->width) {
			tr_state->camera->height = 2 * gf_divfix(tr_state->camera->height, tr_state->camera->width);
			tr_state->camera->width = 2*FIX_ONE;
		} else {
			tr_state->camera->width = 2 * gf_divfix(tr_state->camera->width, tr_state->camera->height);
			tr_state->camera->height = 2*FIX_ONE;
		}
	}

	/*setup bounds*/
	tr_state->bbox.max_edge.x = tr_state->camera->width/2;
	tr_state->bbox.min_edge.x = -tr_state->bbox.max_edge.x;
	tr_state->bbox.max_edge.y = tr_state->camera->height/2;
	tr_state->bbox.min_edge.y = -tr_state->bbox.max_edge.y;
	tr_state->bbox.max_edge.z = tr_state->bbox.min_edge.z = 0;
	tr_state->bbox.is_set = 1;

}

static void TraverseLayer3D(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool prev_layer;//, changed;
	GF_List *oldb, *oldv, *oldf, *oldn;
	GF_Rect rc;
	u32 cur_lights;
	GF_List *node_list_backup;
	GF_BBox bbox_backup;
	GF_Matrix model_backup;
	GF_Matrix2D mx2d_backup;
	GF_Camera *prev_cam;
	GF_VisualManager *old_visual;
	GF_Matrix2D *transform;
	M_Layer3D *l = (M_Layer3D *)node;
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	GF_TraverseState a_tr;
	u32 old_type_3d;

	if (is_destroy) {
		DestroyLayer3D(node);
		return;
	}
	if (st->unsupported) return;

	memcpy(&a_tr, tr_state, sizeof(GF_TraverseState));

	if (gf_node_dirty_get(node)) {

		/*main visual in pixel metrics, use output width*/
		if (tr_state->pixel_metrics && (tr_state->visual->compositor->visual==tr_state->visual)) {
			st->clip.width = INT2FIX(tr_state->visual->compositor->vp_width);
			st->clip.height = INT2FIX(tr_state->visual->compositor->vp_height);
		} else {
			visual_get_size_info(tr_state, &st->clip.width, &st->clip.height);
		}
		/*setup bounds in local coord system*/
		if (l->size.x>=0) st->clip.width = l->size.x;
		if (l->size.y>=0) st->clip.height = l->size.y;
		st->clip = gf_rect_center(st->clip.width, st->clip.height);

//		changed = 1;
	}

	transform = &tr_state->transform;

	/*layer3D maintains its own stacks*/
	oldb = tr_state->backgrounds;
	oldv = tr_state->viewpoints;
	oldf = tr_state->fogs;
	oldn = tr_state->navigations;
	old_visual = tr_state->visual;
	prev_layer = tr_state->is_layer;
	prev_cam = tr_state->camera;
	bbox_backup = tr_state->bbox;


	switch (tr_state->traversing_mode) {
	case TRAVERSE_GET_BOUNDS:
		if (!tr_state->for_node) {
			tr_state->bounds = st->clip;
			gf_bbox_from_rect(&tr_state->bbox, &st->clip);
			return;
		}
	case TRAVERSE_PICK:
		/*layers can only be used in a 2D context*/
		if (tr_state->camera && tr_state->camera->is_3D)
			return;
		break;

	case TRAVERSE_SORT:
		/*layers can only be used in a 2D context*/
		if (tr_state->camera && tr_state->camera->is_3D)
			return;

		if (tr_state->visual->compositor->hybrid_opengl) {
			DrawableContext *ctx = drawable_init_context_mpeg4(st->drawable, tr_state);
			if (!ctx) goto l3d_exit;
			ctx->aspect.fill_texture = &st->txh;
			ctx->flags |= CTX_APP_DIRTY | CTX_IS_TRANSPARENT;
			drawable_finalize_sort(ctx, tr_state, &st->clip);
			return;
		}
		break;
	case TRAVERSE_DRAW_2D:
		if (tr_state->visual->compositor->hybrid_opengl) {
			transform = &tr_state->ctx->transform;
			break;
		}
		layer3d_draw_2d(node, tr_state);
		return;
	case TRAVERSE_DRAW_3D:
	default:
		return;
	}

	/*layer3D maintains its own stacks*/
	tr_state->backgrounds = st->visual->back_stack;
	tr_state->viewpoints = st->visual->view_stack;
	tr_state->navigations = st->visual->navigation_stack;
	tr_state->fogs = st->visual->fog_stack;
	tr_state->is_layer = 1;
	tr_state->camera = &st->visual->camera;

	gf_mx_copy(model_backup, tr_state->model_matrix);
	gf_mx2d_copy(mx2d_backup, tr_state->transform);

	/*compute viewport in visual coordinate*/
	rc = st->clip;
	if (prev_cam) {

		gf_mx_apply_rect(&tr_state->model_matrix, &rc);

		if (tr_state->visual->compositor->hybrid_opengl) {
			gf_mx2d_apply_rect(transform, &rc);
		} else {
			gf_mx_apply_rect(&prev_cam->modelview, &rc);
		}

		if (tr_state->camera->flags & CAM_HAS_VIEWPORT)
			gf_mx_apply_rect(&prev_cam->viewport, &rc);

	} else {
		gf_mx2d_apply_rect(&tr_state->transform, &rc);

		/*switch visual*/
		tr_state->visual = st->visual;
	}


	/*check bindables*/
	gf_mx_init(tr_state->model_matrix);
	l3d_CheckBindables(node, tr_state, st->first);

	if (prev_cam) gf_mx_copy(tr_state->model_matrix, model_backup);

	/*drawing a layer means drawing all subelements as a whole (no depth sorting with parents)*/
	if ((tr_state->traversing_mode==TRAVERSE_SORT) || (tr_state->traversing_mode==TRAVERSE_DRAW_2D)) {
		u32 trav_mode = tr_state->traversing_mode;

//		if (gf_node_dirty_get(node)) changed = 1;
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY|GF_SG_VRML_BINDABLE_DIRTY);

		/*!! we were in a 2D mode without hybridGL, not supported !*/
		if (!prev_cam) {
			st->unsupported = GF_TRUE;
			goto l3d_exit;
		}
		/*note that we don't backup the state as a layer3D cannot be declared in a layer3D*/
		tr_state->layer3d = node;


		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Layer3D] Redrawing\n"));

		layer3d_setup_clip(st, tr_state, 1, rc);

		//this only happens in hybridGL mode
		if (trav_mode==TRAVERSE_DRAW_2D) {
			visual_2d_flush_hybgl_canvas(tr_state->visual, NULL, tr_state->ctx, tr_state);
		}

		old_type_3d = tr_state->visual->type_3d;
		tr_state->visual->type_3d = 2;

		visual_3d_clear_all_lights(tr_state->visual);

		cur_lights = tr_state->visual->num_lights;
		/*this will init projection. Note that we're binding the viewpoint in the current pixelMetrics context
		even if the viewpoint was declared in an inline below
		if no previous camera, we're using offscreen rendering, force clear */
		visual_3d_init_draw(tr_state, 1);

		visual_3d_check_collisions(tr_state, NULL, l->children);
		tr_state->traversing_mode = TRAVERSE_SORT;

		/*shortcut node list*/
		node_list_backup = tr_state->visual->alpha_nodes_to_draw;
		tr_state->visual->alpha_nodes_to_draw = gf_list_new();

		/*reset cull flag*/
		tr_state->cull_flag = 0;
		group_3d_traverse(node, (GroupingNode *)st, tr_state);

		visual_3d_flush_contexts(tr_state->visual, tr_state);

		gf_list_del(tr_state->visual->alpha_nodes_to_draw);
		tr_state->visual->alpha_nodes_to_draw = node_list_backup;

		while (cur_lights < tr_state->visual->num_lights) {
			visual_3d_remove_last_light(tr_state->visual);
		}

		tr_state->traversing_mode = trav_mode ;
		tr_state->visual->type_3d = old_type_3d;
		tr_state->layer3d = NULL;

#if 0
		/*!! we were in a 2D mode, create drawable context!!*/
		if (!prev_cam ) {
			DrawableContext *ctx;
			/*with TinyGL we draw directly to the offscreen buffer*/
#ifndef GPAC_USE_TINYGL
			gf_sc_copy_to_stencil(&st->txh);
#else
			if (st->txh.pixelformat==GF_PIXEL_RGBDS)
				gf_get_tinygl_depth(&st->txh);
#endif

			gf_sc_texture_set_stencil(&st->txh, gf_sc_texture_get_stencil(&st->txh) );
			changed = 1;

			/*restore visual*/
			tr_state->visual = old_visual;
			tr_state->appear = NULL;
			//	tr_state->camera = prev_cam;

			ctx = drawable_init_context_mpeg4(st->drawable, tr_state);
			if (!ctx) goto l3d_exit;
			ctx->aspect.fill_texture = &st->txh;
			ctx->flags |= CTX_NO_ANTIALIAS;
			if (changed) ctx->flags |= CTX_APP_DIRTY;
			if (st->txh.transparent) ctx->flags |= CTX_IS_TRANSPARENT;
			drawable_finalize_sort(ctx, tr_state, NULL);
		}
#endif

	}
	/*check picking - we must fall in our 2D clipper except when mvt is grabbed on layer*/
	else if (!gf_node_dirty_get(node)  && (tr_state->traversing_mode==TRAVERSE_PICK)) {
		GF_Ray prev_r;
		SFVec3f start, end;
		SFVec4f res;
		Fixed in_x, in_y;
		Bool do_pick = 0;

		if (!prev_cam) rc = st->vp;

		layer3d_setup_clip(st, tr_state, prev_cam ? 1 : 0, rc);

		old_type_3d = tr_state->visual->type_3d;
		tr_state->visual->type_3d = 2;

		if (tr_state->visual->compositor->active_layer==node) {
			do_pick = (tr_state->visual->compositor->grabbed_sensor || tr_state->visual->compositor->navigation_state) ? 1 : 0;
		}

		if (!prev_cam || tr_state->visual->compositor->hybrid_opengl) gf_mx_from_mx2d(&tr_state->model_matrix, &tr_state->transform);

		if (!do_pick && !gf_list_count(tr_state->visual->compositor->sensors))
			do_pick = gf_sc_pick_in_clipper(tr_state, &st->clip);

		if (!do_pick) {
			tr_state->visual->type_3d = old_type_3d;
			goto l3d_exit;
		}

		prev_r = tr_state->ray;

		compositor_get_2d_plane_intersection(&tr_state->ray, &start);

		gf_mx_inverse(&tr_state->model_matrix);
		gf_mx_apply_vec(&tr_state->model_matrix, &start);


		if (tr_state->visual->compositor->visual==tr_state->visual) {
			start.x = gf_mulfix(start.x, tr_state->visual->compositor->scale_x);
			start.y = gf_mulfix(start.y, tr_state->visual->compositor->scale_y);
		} else if (!prev_cam) {
			start.x = gf_muldiv(start.x, st->visual->camera.width, st->clip.width);
			start.y = gf_muldiv(start.y, st->visual->camera.height, st->clip.height);
		}

		visual_3d_setup_projection(tr_state, 1);

		in_x = 2 * gf_divfix(start.x, st->visual->camera.width);
		in_y = 2 * gf_divfix(start.y, st->visual->camera.height);

		res.x = in_x;
		res.y = in_y;
		res.z = -FIX_ONE;
		res.q = FIX_ONE;
		gf_mx_apply_vec_4x4(&st->visual->camera.unprojection, &res);
		if (!res.q) {
			tr_state->visual->type_3d = old_type_3d;
			goto l3d_exit;
		}
		start.x = gf_divfix(res.x, res.q);
		start.y = gf_divfix(res.y, res.q);
		start.z = gf_divfix(res.z, res.q);

		res.x = in_x;
		res.y = in_y;
		res.z = FIX_ONE;
		res.q = FIX_ONE;
		gf_mx_apply_vec_4x4(&st->visual->camera.unprojection, &res);
		if (!res.q) {
			tr_state->visual->type_3d = old_type_3d;
			goto l3d_exit;
		}
		end.x = gf_divfix(res.x, res.q);
		end.y = gf_divfix(res.y, res.q);
		end.z = gf_divfix(res.z, res.q);
		tr_state->ray = gf_ray(start, end);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Layer3D] Picking: cast ray\n\tOrigin %.4f %.4f %.4f - End %.4f %.4f %.4f\n\tDir %.4f %.4f %.4f\n",
		                                      FIX2FLT(tr_state->ray.orig.x), FIX2FLT(tr_state->ray.orig.y), FIX2FLT(tr_state->ray.orig.z),
		                                      FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z),
		                                      FIX2FLT(tr_state->ray.dir.x), FIX2FLT(tr_state->ray.dir.y), FIX2FLT(tr_state->ray.dir.z)));

		group_3d_traverse(node, (GroupingNode *)st, tr_state);
		tr_state->ray = prev_r;

		tr_state->visual->type_3d = old_type_3d;

		/*store info if navigation allowed - we just override any layer3D picked first since we are picking 2D
		objects*/
		if (tr_state->camera->navigate_mode || (tr_state->camera->navigation_flags & NAV_ANY))
			tr_state->layer3d = node;

		tr_state->traversing_mode = TRAVERSE_PICK;
	}

l3d_exit:

	/*restore camera*/
	tr_state->camera = prev_cam;
	if (prev_cam) {
		//remember to reload previous projection matrix
		visual_3d_projection_matrix_modified(tr_state->visual);
		if (tr_state->visual == old_visual) {
			visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);
		}
	}
	tr_state->visual = old_visual;

	/*restore traversing state*/
	tr_state->backgrounds = oldb;
	tr_state->viewpoints = oldv;
	tr_state->fogs = oldf;
	tr_state->navigations = oldn;
	tr_state->bbox = bbox_backup;
	tr_state->is_layer = prev_layer;
	gf_mx_copy(tr_state->model_matrix, model_backup);
	gf_mx2d_copy(tr_state->transform, mx2d_backup);

	/*in case we missed bindables*/
	if (st->first) {
		st->first = 0;
		gf_node_dirty_set(node, 0, 0);
		gf_sc_invalidate(tr_state->visual->compositor, NULL);
	}
}

void compositor_init_layer3d(GF_Compositor *compositor, GF_Node *node)
{
	Layer3DStack *stack;

	if (!gf_sc_check_gl_support(compositor)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Driver disabled, cannot render Layer 3D\n"));
		return;
	}

	GF_SAFEALLOC(stack, Layer3DStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate layer 3d stack\n"));
		return;
	}
	stack->visual = visual_new(compositor);
	stack->visual->type_3d = 2;
	stack->visual->camera.is_3D = 1;
	stack->visual->camera.visibility = 0;
	stack->visual->camera.speed = FIX_ONE;
	camera_invalidate(&stack->visual->camera);
	stack->first = 1;

	stack->txh.compositor = compositor;
	stack->drawable = drawable_new();
	stack->drawable->node = node;
	stack->drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseLayer3D);
}

GF_Camera *compositor_layer3d_get_camera(GF_Node *node)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	return &st->visual->camera;
}

void compositor_layer3d_bind_camera(GF_Node *node, Bool do_bind, u32 nav_value)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	GF_Node *n = (GF_Node*)gf_list_get(st->visual->navigation_stack, 0);
	if (n) Bindable_SetSetBind(n, do_bind);
	else st->visual->camera.navigate_mode = nav_value;
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR) && !defined(GPAC_DISABLE_3D)
