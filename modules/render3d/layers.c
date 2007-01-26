/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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



#include "render3d_nodes.h"
#include "grouping.h"


typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
	GF_List *backs;
	GF_List *views;
	Bool first;
	GF_Rect clip;
} Layer2DStack;



static void l2d_CheckBindables(GF_Node *n, RenderEffect3D *eff, Bool force_render)
{
	GF_Node *btop;
	M_Layer2D *l2d;
	l2d = (M_Layer2D *)n;
	if (force_render) gf_node_render(l2d->background, eff);
	btop = (GF_Node*)gf_list_get(eff->backgrounds, 0);
	if (btop != l2d->background) { 
		gf_node_unregister(l2d->background, n);
		gf_node_register(btop, n); 
		l2d->background = btop;
		gf_node_event_out_str(n, "background");
	}
	if (force_render) gf_node_render(l2d->viewport, eff);
	btop = (GF_Node*)gf_list_get(eff->viewpoints, 0);
	if (btop != l2d->viewport) { 
		gf_node_unregister(l2d->viewport, n);
		gf_node_register(btop, n); 
		l2d->viewport = btop;
		gf_node_event_out_str(n, "viewport");
	}
}

static void RenderLayer2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_List *oldb, *oldv, *oldf, *oldn;
	GF_Node *viewport;
	GF_List *node_list_backup;
	GF_Node *back;
	M_Layer2D *l = (M_Layer2D *)node;
	Layer2DStack *st = (Layer2DStack *) gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *) rs;
	
	if (is_destroy) {
		DeleteGroupingNode((GroupingNode *)st);
		gf_list_del(st->backs);
		gf_list_del(st->views);
		free(st);
		return;
	}

	/*layers can only be rendered in a 2D context*/
	if (eff->camera->is_3D) return;

	/*layer2D maintains its own stacks*/
	oldb = eff->backgrounds;
	oldv = eff->viewpoints;
	oldf = eff->fogs;
	oldn = eff->navigations;
	eff->backgrounds = st->backs;
	eff->viewpoints = st->views;
	eff->fogs = eff->navigations = NULL;

	l2d_CheckBindables(node, eff, st->first);

	back = (GF_Node*)gf_list_get(st->backs, 0);

	viewport = (GF_Node*)gf_list_get(st->views, 0);

	if (gf_node_dirty_get(node)) {
		/*recompute children bounds (otherwise culling will fail)*/
		if (gf_node_dirty_get(node) && GF_SG_CHILD_DIRTY) 
			grouping_traverse((GroupingNode *)st, eff, NULL);
		
		/*and override group bounds*/
		R3D_GetSurfaceSizeInfo(eff, &st->clip.width, &st->clip.height);
		/*setup bounds in local coord system*/
		if (l->size.x>=0) st->clip.width = l->size.x;
		if (l->size.y>=0) st->clip.height = l->size.y;
		st->clip = gf_rect_center(st->clip.width, st->clip.height);
		
		gf_bbox_from_rect(&st->bbox, &st->clip);
	}
	
	/*drawing a layer means drawing all subelements as a whole (no depth sorting with parents)*/
	if (eff->traversing_mode==TRAVERSE_RENDER) {
		GF_Rect prev_clipper;
		Bool had_clip;

		eff->layer_clipper = R3D_UpdateClipper(eff, st->clip, &had_clip, &prev_clipper, 1);

		VS3D_PushMatrix(eff->surface);
		/*setup clipping*/
		VS3D_SetClipper2D(eff->surface, eff->layer_clipper);
		
		/*apply background BEFORE viewport*/
		if (back) {
			eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
			eff->bbox = st->bbox;
			gf_node_render(back, eff);
		}

		/*sort all children without transform, and use current transform when flushing contexts*/
		gf_mx_init(eff->model_matrix);

		/*apply viewport*/
		if (viewport) {
			eff->traversing_mode = TRAVERSE_RENDER_BINDABLE;
			gf_bbox_from_rect(&eff->bbox, &st->clip);
			gf_node_render(viewport, eff);
			VS3D_MultMatrix(eff->surface, eff->model_matrix.m);
		}

		node_list_backup = eff->surface->alpha_nodes_to_draw;
		eff->surface->alpha_nodes_to_draw = gf_list_new();
		eff->traversing_mode = TRAVERSE_SORT;
		/*reset cull flag*/
		eff->cull_flag = 0;
		grouping_traverse((GroupingNode *)st, eff, NULL);

		VS_FlushContexts(eff->surface, eff);

		assert(!gf_list_count(eff->surface->alpha_nodes_to_draw));
		gf_list_del(eff->surface->alpha_nodes_to_draw);
		eff->surface->alpha_nodes_to_draw = node_list_backup;

		
		VS3D_PopMatrix(eff->surface);

		VS3D_ResetClipper2D(eff->surface);

		eff->has_layer_clip = had_clip;
		if (had_clip) {
			eff->layer_clipper = prev_clipper;
			VS3D_SetClipper2D(eff->surface, eff->layer_clipper);
		}

	} else if (eff->traversing_mode==TRAVERSE_SORT) {
		gf_node_allow_cyclic_render(node);
		VS_RegisterContext(eff, node, &st->bbox, 0);
	}
	/*check picking - we must fall in our 2D clipper*/
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		if (R3D_PickInClipper(eff, &st->clip)) 
			grouping_traverse((GroupingNode *)st, eff, NULL);
	} else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		eff->bbox = st->bbox;
	}
	
	group_reset_children((GroupingNode*)st);

	/*restore effect*/
	eff->backgrounds = oldb;
	eff->viewpoints = oldv;
	eff->fogs = oldf;
	eff->navigations = oldn;

	/*in case we missed bindables*/
	if (st->first) {
		st->first = 0;
		gf_sr_invalidate(st->compositor, NULL);
	}
}

void R3D_InitLayer2D(Render3D *sr, GF_Node *node)
{
	Layer2DStack *stack = (Layer2DStack *)malloc(sizeof(Layer2DStack));
	SetupGroupingNode((GroupingNode*)stack, sr->compositor, node, & ((M_Layer2D *)node)->children);
	stack->backs = gf_list_new();
	stack->views = gf_list_new();
	stack->first = 1;

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderLayer2D);
}


typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
	GF_List *backs;
	GF_List *views;
	GF_List *navinfos;
	GF_List *fogs;
	GF_Camera cam;
	Bool first;
	GF_Rect clip;
} Layer3DStack;


static void DestroyLayer3D(GF_Node *node)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	Render3D *sr = (Render3D *)st->compositor->visual_renderer->user_priv;
	DeleteGroupingNode((GroupingNode *)st);
	
	BindableStackDelete(st->backs);
	BindableStackDelete(st->views);
	BindableStackDelete(st->fogs);
	BindableStackDelete(st->navinfos);
	if (sr->active_layer == node) sr->active_layer = NULL;
	free(st);
}

static void l3d_CheckBindables(GF_Node *n, RenderEffect3D *eff, Bool force_render)
{
	GF_Node *btop;
	u32 mode;
	M_Layer3D *l3d;
	l3d = (M_Layer3D *)n;

	mode = eff->traversing_mode;
	eff->traversing_mode = TRAVERSE_GET_BOUNDS;

	if (force_render) gf_node_render(l3d->background, eff);
	btop = (GF_Node*)gf_list_get(eff->backgrounds, 0);
	if (btop != l3d->background) { 
		gf_node_unregister(l3d->background, n);
		gf_node_register(btop, n); 
		l3d->background = btop;
		gf_node_event_out_str(n, "background");
	}
	if (force_render) gf_node_render(l3d->viewpoint, eff);
	btop = (GF_Node*)gf_list_get(eff->viewpoints, 0);
	if (btop != l3d->viewpoint) { 
		gf_node_unregister(l3d->viewpoint, n);
		gf_node_register(btop, n); 
		l3d->viewpoint = btop;
		gf_node_event_out_str(n, "viewpoint");
	}
	if (force_render) gf_node_render(l3d->navigationInfo, eff);
	btop = (GF_Node*)gf_list_get(eff->navigations, 0);
	if (btop != l3d->navigationInfo) { 
		gf_node_unregister(l3d->navigationInfo, n);
		gf_node_register(btop, n); 
		l3d->navigationInfo = btop;
		gf_node_event_out_str(n, "navigationInfo");
	}
	if (force_render) gf_node_render(l3d->fog, eff);
	btop = (GF_Node*)gf_list_get(eff->fogs, 0);
	if (btop != l3d->fog) { 
		gf_node_unregister(l3d->fog, n);
		gf_node_register(btop, n); 
		l3d->fog = btop;
		gf_node_event_out_str(n, "fog");
	}
	eff->traversing_mode = mode;
}


static void RenderLayer3D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_List *oldb, *oldv, *oldf, *oldn;
	GF_Rect rc;
	u32 cur_lights;
	Fixed sw, sh;
	GF_List *node_list_backup;
	GF_BBox bbox_backup;
	GF_Matrix model_backup;
	GF_Camera *prev_cam;
	M_Layer3D *l = (M_Layer3D *)node;
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *) rs;
	
	if (is_destroy) {
		DestroyLayer3D(node);
		return;
	}
	/*layers can only be rendered in a 2D context*/
	if (eff->camera->is_3D) return;

	if (gf_node_dirty_get(node)) {

		/*main surface in pixel metrics, use output width*/
		if (eff->is_pixel_metrics && (eff->surface->render->surface==eff->surface)) {
			st->clip.width = INT2FIX(eff->surface->render->out_width);
			st->clip.height = INT2FIX(eff->surface->render->out_height);
		} else {
			R3D_GetSurfaceSizeInfo(eff, &st->clip.width, &st->clip.height);
		}
		/*setup bounds in local coord system*/
		if (l->size.x>=0) st->clip.width = l->size.x;
		if (l->size.y>=0) st->clip.height = l->size.y;
		st->clip = gf_rect_center(st->clip.width, st->clip.height);
		gf_bbox_from_rect(&st->bbox, &st->clip);
	}

	switch (eff->traversing_mode) {
	case TRAVERSE_SORT:
		gf_node_allow_cyclic_render(node);
		VS_RegisterContext(eff, node, &st->bbox, 0);
		return;
	case TRAVERSE_GET_BOUNDS:
		eff->bbox = st->bbox;
		return;
	case TRAVERSE_RENDER:
	case TRAVERSE_PICK:
		break;
	default:
		return;
	}

	/*layer3D maintains its own stacks*/
	oldb = eff->backgrounds;
	oldv = eff->viewpoints;
	oldf = eff->fogs;
	oldn = eff->navigations;
	eff->backgrounds = st->backs;
	eff->viewpoints = st->views;
	eff->fogs = st->fogs;
	eff->navigations = st->navinfos;

	prev_cam = eff->camera;
	eff->camera = &st->cam;

	/*check bindables*/
	l3d_CheckBindables(node, eff, st->first);

	/*compute viewport in surface coordinate*/
	rc = st->clip;
	gf_mx_apply_rect(&prev_cam->modelview, &rc);
	if (eff->camera->flags & CAM_HAS_VIEWPORT)
		gf_mx_apply_rect(&prev_cam->viewport, &rc);
	gf_mx_apply_rect(&eff->model_matrix, &rc);
	if (eff->surface->render->surface==eff->surface) {
		gf_mx_init(model_backup);
		gf_mx_add_scale(&model_backup, eff->surface->render->scale_x, eff->surface->render->scale_y, FIX_ONE);
		gf_mx_apply_rect(&model_backup, &rc);
		sw = INT2FIX(eff->surface->render->compositor->width);
		sh = INT2FIX(eff->surface->render->compositor->height);
	} else {
		sw = INT2FIX(eff->surface->width);
		sh = INT2FIX(eff->surface->height);
	}

	st->cam.vp = rc;
	st->cam.vp.x += sw/2;
	st->cam.vp.y -= st->cam.vp.height;
	st->cam.vp.y += sh/2;
//	if (st->cam.vp.x<0) { st->cam.vp.width += st->cam.vp.x; st->cam.vp.x = 0; }
//	if (st->cam.vp.y<0) { st->cam.vp.height += st->cam.vp.y; st->cam.vp.y = 0; }
//	if (st->cam.vp.width>sw) st->cam.vp.width = sw;
//	if (st->cam.vp.height>sh) st->cam.vp.height = sh;

	eff->camera->width = eff->camera->vp.width;
	eff->camera->height = eff->camera->vp.height;

	if (!eff->is_pixel_metrics) {
		if (eff->camera->height > eff->camera->width) {
			eff->camera->height = 2 * gf_divfix(eff->camera->height, eff->camera->width);
			eff->camera->width = 2*FIX_ONE;
		} else {
			eff->camera->width = 2 * gf_divfix(eff->camera->width, eff->camera->height);
			eff->camera->height = 2*FIX_ONE;
		}
	}
	bbox_backup = eff->bbox;
	gf_mx_copy(model_backup, eff->model_matrix);

	/*setup surface bounds*/
	eff->bbox.max_edge.x = eff->camera->width/2;
	eff->bbox.min_edge.x = -eff->bbox.max_edge.x;
	eff->bbox.max_edge.y = eff->camera->height/2;
	eff->bbox.min_edge.y = -eff->bbox.max_edge.y;
	eff->bbox.max_edge.z = eff->bbox.min_edge.z = 0;
	eff->bbox.is_set = 1;


	/*recompute children bounds (otherwise culling will fail)*/
	if (gf_node_dirty_get(node) && GF_SG_CHILD_DIRTY) {
		u32 mode_back = eff->traversing_mode;
		eff->traversing_mode = TRAVERSE_GET_BOUNDS;
		grouping_traverse((GroupingNode *)st, eff, NULL);
		eff->traversing_mode = mode_back;
	}
	gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);


	VS3D_SetMatrixMode(eff->surface, MAT_PROJECTION);
	VS3D_PushMatrix(eff->surface);
	VS3D_SetMatrixMode(eff->surface, MAT_MODELVIEW);
	VS3D_PushMatrix(eff->surface);

	/*drawing a layer means drawing all subelements as a whole (no depth sorting with parents)*/
	if (eff->traversing_mode==TRAVERSE_RENDER) {

		cur_lights = eff->surface->num_lights;
		/*this will init projection. Note that we're binding the viewpoint in the current pixelMetrics context
		even if the viewpoint was declared in an inline below*/
		VS_InitRender(eff);

		VS_DoCollisions(eff, l->children);
		eff->traversing_mode = TRAVERSE_SORT;

		/*shortcut node list*/
		node_list_backup = eff->surface->alpha_nodes_to_draw;
		eff->surface->alpha_nodes_to_draw = gf_list_new();

		/*reset cull flag*/
		eff->cull_flag = 0;
		grouping_traverse((GroupingNode *)st, eff, NULL);

		VS_FlushContexts(eff->surface, eff);

		gf_list_del(eff->surface->alpha_nodes_to_draw);
		eff->surface->alpha_nodes_to_draw = node_list_backup;

		while (cur_lights < eff->surface->num_lights) {
			VS3D_RemoveLastLight(eff->surface);
		}

	}
	/*check picking - we must fall in our 2D clipper except when mvt is grabbed on layer*/
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		GF_Ray prev_r;
		SFVec3f start, end;
		SFVec4f res;
		Fixed in_x, in_y;
		Bool do_pick = 0;
		if (eff->surface->render->active_layer==node) {
			do_pick = (eff->surface->render->is_grabbed || eff->surface->render->nav_is_grabbed) ? 1 : 0;
		}
		if (!do_pick && !gf_list_count(eff->surface->render->sensors)) do_pick = R3D_PickInClipper(eff, &st->clip);
		if (!do_pick) goto l3d_exit;

		prev_r = eff->ray;

		R3D_Get2DPlaneIntersection(&eff->ray, &start);

		gf_mx_inverse(&eff->model_matrix);
		gf_mx_apply_vec(&eff->model_matrix, &start);


		if (eff->surface->render->surface==eff->surface) {
			start.x = gf_mulfix(start.x, eff->surface->render->scale_x);
			start.y = gf_mulfix(start.y, eff->surface->render->scale_y);
		}

		VS_SetupProjection(eff);
		in_x = 2 * gf_divfix(start.x, st->cam.width);
		in_y = 2 * gf_divfix(start.y, st->cam.height);
			
		res.x = in_x; res.y = in_y; res.z = -FIX_ONE; res.q = FIX_ONE;
		gf_mx_apply_vec_4x4(&st->cam.unprojection, &res);
		if (!res.q) goto l3d_exit;
		start.x = gf_divfix(res.x, res.q);
		start.y = gf_divfix(res.y, res.q);
		start.z = gf_divfix(res.z, res.q);

		res.x = in_x; res.y = in_y; res.z = FIX_ONE; res.q = FIX_ONE;
		gf_mx_apply_vec_4x4(&st->cam.unprojection, &res);
		if (!res.q) goto l3d_exit;
		end.x = gf_divfix(res.x, res.q);
		end.y = gf_divfix(res.y, res.q);
		end.z = gf_divfix(res.z, res.q);
		eff->ray = gf_ray(start, end);
		grouping_traverse((GroupingNode *)st, eff, NULL);
		eff->ray = prev_r;

		/*we're the first layer being traversed, store info if navigation allowed*/
		if (!eff->collect_layer) {
			if (eff->camera->navigate_mode || (eff->camera->navigation_flags & NAV_ANY))
				eff->collect_layer = node;
		}
	}

l3d_exit:
	VS3D_SetMatrixMode(eff->surface, MAT_PROJECTION);
	VS3D_PopMatrix(eff->surface);
	VS3D_SetMatrixMode(eff->surface, MAT_MODELVIEW);
	VS3D_PopMatrix(eff->surface);
	/*restore camera*/
	eff->camera = prev_cam;
	VS3D_SetViewport(eff->surface, eff->camera->vp);

	group_reset_children((GroupingNode*)st);

	/*restore effect*/
	eff->backgrounds = oldb;
	eff->viewpoints = oldv;
	eff->fogs = oldf;
	eff->navigations = oldn;
	eff->bbox = bbox_backup;
	gf_mx_copy(eff->model_matrix, model_backup);

	/*in case we missed bindables*/
	if (st->first) {
		st->first = 0;
		gf_sr_invalidate(st->compositor, NULL);
	}
}

void R3D_InitLayer3D(Render3D *sr, GF_Node *node)
{
	Layer3DStack *stack;
	GF_SAFEALLOC(stack, Layer3DStack);
	SetupGroupingNode((GroupingNode*)stack, sr->compositor, node, & ((M_Layer3D *)node)->children);
	stack->backs = gf_list_new();
	stack->views = gf_list_new();
	stack->fogs = gf_list_new();
	stack->navinfos = gf_list_new();
	stack->first = 1;
	stack->cam.is_3D = 1;
	camera_invalidate(&stack->cam);


	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderLayer3D);
}

GF_Camera *l3d_get_camera(GF_Node *node)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	return &st->cam;
}

void l3d_bind_camera(GF_Node *node, Bool do_bind, u32 nav_value)
{
	Layer3DStack *st = (Layer3DStack *) gf_node_get_private(node);
	GF_Node *n = (GF_Node*)gf_list_get(st->navinfos, 0);
	if (n) Bindable_SetSetBind(n, do_bind);
	else st->cam.navigate_mode = nav_value;
}
