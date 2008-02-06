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
#include "texturing.h"
#include "nodes_stacks.h"
#include <gpac/options.h>

#ifndef GPAC_DISABLE_3D



/*generic drawable 3D constructor/destructor*/
Drawable3D *drawable_3d_new(GF_Node *node)
{
	Drawable3D *tmp;
	GF_SAFEALLOC(tmp, Drawable3D);
	tmp->mesh = new_mesh();
	gf_node_set_private(node, tmp);
	return tmp;
}
void drawable_3d_del(GF_Node *n)
{
	Drawable3D *d = (Drawable3D *)gf_node_get_private(n);
	if (d) {
		if (d->mesh) mesh_free(d->mesh);
		free(d);
	}
}

static void visual_3d_setup_traversing_state(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	tr_state->visual = visual;
	tr_state->camera = &visual->camera;
	tr_state->backgrounds = visual->back_stack;
	tr_state->viewpoints = visual->view_stack;
	tr_state->fogs = visual->fog_stack;
	tr_state->navigations = visual->navigation_stack;
	tr_state->color_mat.identity = 1;
	tr_state->camera->vp.x = tr_state->camera->vp.y = 0;
	tr_state->min_hsize = INT2FIX(MIN(visual->width, visual->height) / 2);
	if (!tr_state->min_hsize) tr_state->min_hsize = FIX_ONE;


	/*main visual, set AR*/
	if (visual->compositor->visual==visual) {
		if (tr_state->visual->compositor->has_size_info) {
			tr_state->camera->vp.x = INT2FIX(tr_state->visual->compositor->vp_x); 
			tr_state->camera->vp.y = INT2FIX(tr_state->visual->compositor->vp_y);
			tr_state->camera->vp.width = INT2FIX(tr_state->visual->compositor->vp_width);
			tr_state->camera->vp.height = INT2FIX(tr_state->visual->compositor->vp_height);

			/*2D ortho, scale is already present in the root user transform*/
			if (visual->type_3d==1) {
				tr_state->camera->width = INT2FIX(tr_state->visual->compositor->vp_width);
				tr_state->camera->height = INT2FIX(tr_state->visual->compositor->vp_height);
			} else {
				tr_state->camera->width = INT2FIX(tr_state->visual->width);
				tr_state->camera->height = INT2FIX(tr_state->visual->height);
			}
		} else {
			Fixed sw, sh;
			sw = INT2FIX(tr_state->visual->compositor->vp_width);
			sh = INT2FIX(tr_state->visual->compositor->vp_height);
			/*AR changed, rebuild camera*/

			if (tr_state->visual->compositor->recompute_ar 
				|| (sw!=tr_state->camera->vp.width) 
				|| (sh!=tr_state->camera->vp.height)) { 
				tr_state->camera->width = tr_state->camera->vp.width = INT2FIX(tr_state->visual->compositor->vp_width);
				tr_state->camera->height = tr_state->camera->vp.height = INT2FIX(tr_state->visual->compositor->vp_height);
				tr_state->camera->flags |= CAM_IS_DIRTY;
			}
		}
	}
	/*composite visual, no AR*/
	else {
		tr_state->camera->vp.width = tr_state->camera->width = INT2FIX(visual->width);
		tr_state->camera->vp.height = tr_state->camera->height = INT2FIX(visual->height);
	}

	if (!tr_state->pixel_metrics) {
		if (tr_state->camera->height > tr_state->camera->width) {
			tr_state->camera->height = 2*gf_divfix(tr_state->camera->height , tr_state->camera->width);
			tr_state->camera->width = 2*FIX_ONE;
		} else {
			tr_state->camera->width = 2 * gf_divfix(tr_state->camera->width, tr_state->camera->height);
			tr_state->camera->height = 2 * FIX_ONE;
		}
	}
	/*setup bounds*/
	tr_state->bbox.max_edge.x = tr_state->camera->width / 2;
	tr_state->bbox.min_edge.x = -tr_state->bbox.max_edge.x;
	tr_state->bbox.max_edge.y = tr_state->camera->height / 2;
	tr_state->bbox.min_edge.y = -tr_state->bbox.max_edge.y;
	tr_state->bbox.max_edge.z = tr_state->bbox.min_edge.z = 0;
	tr_state->bbox.is_set = 1;
}


void visual_3d_viewpoint_change(GF_TraverseState *tr_state, GF_Node *vp, Bool animate_change, Fixed fieldOfView, SFVec3f position, SFRotation orientation, SFVec3f local_center)
{
	Fixed dist;
	SFVec3f d;

	/*update znear&zfar*/
	tr_state->camera->z_near = tr_state->camera->avatar_size.x ; 
	if (tr_state->camera->z_near<=0) tr_state->camera->z_near = FIX_ONE/2;
	tr_state->camera->z_far = tr_state->camera->visibility; 
	if (tr_state->camera->z_far<=0) {
		tr_state->camera->z_far = INT2FIX(1000);
		/*use the current graph pm settings, NOT the viewpoint one*/
#ifdef GPAC_FIXED_POINT
//		if (tr_state->is_pixel_metrics) tr_state->camera->z_far = FIX_MAX/40;
#else
//		if (tr_state->is_pixel_metrics) tr_state->camera->z_far = gf_mulfix(tr_state->camera->z_far , tr_state->min_hsize);
#endif
	}

	if (vp) {
		/*now check if vp is in pixel metrics. If not then:
		- either it's in the main scene, there's nothing to do
		- or it's in an inline, and the inline has been scaled if main scene is in pm: nothing to do*/
		if (0 && gf_sg_use_pixel_metrics(gf_node_get_graph(vp))) {
			GF_Matrix mx;
			gf_mx_init(mx);
			gf_mx_add_scale(&mx, tr_state->min_hsize, tr_state->min_hsize, tr_state->min_hsize);
			gf_mx_apply_vec(&mx, &position);
			gf_mx_apply_vec(&mx, &local_center);
		}
	}
	/*default VP setup - this is undocumented in the spec. Default VP pos is (0, 0, 10) but not really nice 
	in pixel metrics. We set z so that we see just the whole visual*/
	else if (tr_state->pixel_metrics) {
		position.z = gf_divfix(tr_state->camera->width, 2*gf_tan(fieldOfView/2) );
	}

	gf_vec_diff(d, position, local_center);
	dist = gf_vec_len(d);

	if (!dist || (dist<tr_state->camera->z_near) || (dist > tr_state->camera->z_far)) {
		if (dist > tr_state->camera->z_far) tr_state->camera->z_far = 2*dist;

		dist = 10 * tr_state->camera->avatar_size.x;
		if ((dist<tr_state->camera->z_near) || (dist > tr_state->camera->z_far)) 
			dist = (tr_state->camera->avatar_size.x + tr_state->camera->z_far) / 5;
	}
	tr_state->camera->vp_dist = dist;
	tr_state->camera->vp_position = position;
	tr_state->camera->vp_orientation = orientation;
	tr_state->camera->vp_fov = fieldOfView;
	tr_state->camera->examine_center = local_center;

	camera_reset_viewpoint(tr_state->camera, animate_change);
	if (tr_state->layer3d) gf_node_dirty_set(tr_state->layer3d, GF_SG_VRML_BINDABLE_DIRTY, 0);
	gf_sc_invalidate(tr_state->visual->compositor, NULL);
}


void visual_3d_setup_projection(GF_TraverseState *tr_state)
{
	GF_Node *bindable;
	u32 mode = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_BINDABLE;

	/*setup viewpoint (this directly modifies the frustum)*/
	bindable = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
		tr_state->camera->had_viewpoint = 1;
	} else if (tr_state->camera->had_viewpoint) {
		if (tr_state->camera->is_3D) {
			SFVec3f pos, center;
			SFRotation r;
			Fixed fov = GF_PI/4;
			/*default viewpoint*/
			pos.x = pos.y = 0; pos.z = 10 * FIX_ONE;
			center.x = center.y = center.z = 0;
			r.q = r.x = r.z = 0; r.y = FIX_ONE;
			/*this takes care of pixelMetrics*/
			visual_3d_viewpoint_change(tr_state, NULL, 0, fov, pos, r, center);
			/*initial vp compute, don't animate*/
			if (tr_state->camera->had_viewpoint == 2) {
				camera_stop_anim(tr_state->camera);
				camera_reset_viewpoint(tr_state->camera, 0);
			}
		} else {
			tr_state->camera->flags &= ~CAM_HAS_VIEWPORT;
			tr_state->camera->flags |= CAM_IS_DIRTY;
		}
		tr_state->camera->had_viewpoint = 0;
	}

	camera_update(tr_state->camera, &tr_state->transform, tr_state->visual->center_coords);

	/*setup projection/modelview*/
	visual_3d_set_matrix_mode(tr_state->visual, V3D_MATRIX_PROJECTION);
	visual_3d_matrix_load(tr_state->visual, tr_state->camera->projection.m);
	visual_3d_set_matrix_mode(tr_state->visual, V3D_MATRIX_MODELVIEW);
	visual_3d_matrix_load(tr_state->visual, tr_state->camera->modelview.m);
	gf_mx_init(tr_state->model_matrix);

	tr_state->traversing_mode = mode;
}

void visual_3d_init_draw(GF_TraverseState *tr_state, u32 layer_type)
{
	u32 mode;
	GF_Node *bindable;

	/*if not in layer, traverse navigation node
	FIXME: we should update the nav info according to the world transform at the current viewpoint (vrml)*/
	tr_state->traversing_mode = TRAVERSE_BINDABLE;
	bindable = tr_state->navigations ? (GF_Node*) gf_list_get(tr_state->navigations, 0) : NULL;
	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
		tr_state->camera->had_nav_info = 1;
	} else if (tr_state->camera->had_nav_info) {
		/*if no navigation specified, use default VRML one*/
		tr_state->camera->avatar_size.x = FLT2FIX(0.25f); tr_state->camera->avatar_size.y = FLT2FIX(1.6f); tr_state->camera->avatar_size.z = FLT2FIX(0.75f);
		tr_state->camera->visibility = 0;
		tr_state->camera->speed = FIX_ONE;
		/*not specified in the spec, but by default we forbid navigation in layer*/
		if (layer_type) {
			tr_state->camera->navigation_flags = NAV_HEADLIGHT;
			tr_state->camera->navigate_mode = GF_NAVIGATE_NONE;
		} else {
			tr_state->camera->navigation_flags = NAV_ANY | NAV_HEADLIGHT;
			if (tr_state->camera->is_3D) {
				/*X3D is by default examine, VRML/MPEG4 is WALK*/
				tr_state->camera->navigate_mode = (tr_state->visual->type_3d==3) ? GF_NAVIGATE_EXAMINE : GF_NAVIGATE_WALK;
			} else {
				tr_state->camera->navigate_mode = GF_NAVIGATE_NONE;
			}
		}
		tr_state->camera->had_nav_info = 0;

		if (tr_state->pixel_metrics) {
			tr_state->camera->visibility = gf_mulfix(tr_state->camera->visibility, tr_state->min_hsize);
			tr_state->camera->avatar_size.x = gf_mulfix(tr_state->camera->avatar_size.x, tr_state->min_hsize);
			tr_state->camera->avatar_size.y = gf_mulfix(tr_state->camera->avatar_size.y, tr_state->min_hsize);
			tr_state->camera->avatar_size.z = gf_mulfix(tr_state->camera->avatar_size.z, tr_state->min_hsize);
		}
	}

	/*animate current camera - if returns TRUE draw next frame*/
	if (camera_animate(tr_state->camera)) 
		gf_sc_invalidate(tr_state->visual->compositor, NULL);

	visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);

	/*setup projection*/
	visual_3d_setup_projection(tr_state);

	/*turn off depth buffer in 2D*/
	visual_3d_enable_depth_buffer(tr_state->visual, tr_state->camera->is_3D);

	/*set headlight if any*/
	visual_3d_enable_headlight(tr_state->visual, (tr_state->camera->navigation_flags & NAV_HEADLIGHT) ? 1 : 0, tr_state->camera);

	/*setup background*/
	mode = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_BINDABLE;
	bindable = (GF_Node*) gf_list_get(tr_state->backgrounds, 0);

	/*if in layer clear z buffer (even if background)*/
	if (layer_type) visual_3d_clear_depth(tr_state->visual);

	/*clear requested - do it before background drawing for layer3D (transparent background)*/
	if (layer_type==2) {
		SFColor col;
		col.red = INT2FIX((tr_state->visual->compositor->back_color>>16)&0xFF) / 255;
		col.green = INT2FIX((tr_state->visual->compositor->back_color>>8)&0xFF) / 255;
		col.blue = INT2FIX((tr_state->visual->compositor->back_color)&0xFF) / 255;
		visual_3d_clear(tr_state->visual, col, 0);
	}

	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
	}
	/*clear if not in layer*/
	else if (!layer_type) {
		SFColor col;
		col.red = INT2FIX((tr_state->visual->compositor->back_color>>16)&0xFF) / 255;
		col.green = INT2FIX((tr_state->visual->compositor->back_color>>8)&0xFF) / 255;
		col.blue = INT2FIX((tr_state->visual->compositor->back_color)&0xFF) / 255;
		/*if composite visual, clear with alpha = 0*/
		visual_3d_clear(tr_state->visual, col, (tr_state->visual==tr_state->visual->compositor->visual) ? FIX_ONE : 0);
	}
	tr_state->traversing_mode = mode;
}


static GFINLINE Bool visual_3d_has_alpha(GF_TraverseState *tr_state, GF_Node *geom)
{
	u32 tag;
	Bool is_mat3D;
	Drawable3D *stack;
	GF_Node *mat = tr_state->appear ? ((M_Appearance *)tr_state->appear)->material : NULL;

	is_mat3D = 0;
	if (mat) {
		tag = gf_node_get_tag(mat);
		switch (tag) {
		/*for M2D: if filled & transparent we're transparent - otherwise we must check texture*/
		case TAG_MPEG4_Material2D:
			if (((M_Material2D *)mat)->filled && ((M_Material2D *)mat)->transparency) return 1;
			break;
		case TAG_MPEG4_Material:
		case TAG_X3D_Material:
			is_mat3D = 1;
			if ( ((M_Material *)mat)->transparency) return 1;
			break;
		case TAG_MPEG4_MaterialKey:
			return 1;
			break;
		}
	} else if (tr_state->camera->is_3D && tr_state->appear) {
		GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
		if (txh && txh->transparent) return 1;
	}

	/*check alpha texture in3D or with bitmap*/
	if (is_mat3D || (gf_node_get_tag(geom)==TAG_MPEG4_Bitmap)) {
		GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
		if (txh && txh->transparent) return 1;
	}
	/*TODO - FIXME check alpha only...*/
	if (!tr_state->color_mat.identity) return 1;

	stack = (Drawable3D *)gf_node_get_private(geom);
	if (stack && stack->mesh && (stack->mesh->flags & MESH_HAS_ALPHA)) return 1;
	return 0;
}

void visual_3d_register_context(GF_TraverseState *tr_state, GF_Node *geometry)
{
	u32 i, count;
	DirectionalLightContext *nl, *ol;
	Drawable3DContext *ctx;
	Drawable3D *drawable;

	assert(tr_state->traversing_mode == TRAVERSE_SORT);

	/*if 2D draw in declared order. Otherwise, if no alpha or node is a layer, draw directly*/
	if (!tr_state->camera->is_3D || !visual_3d_has_alpha(tr_state, geometry)) {
		tr_state->traversing_mode = TRAVERSE_DRAW_3D;
		/*layout/form clipper, set it in world coords only*/
		if (tr_state->has_clip) {
			visual_3d_matrix_push(tr_state->visual);
			visual_3d_matrix_reset(tr_state->visual);
			visual_3d_set_clipper_2d(tr_state->visual, tr_state->clipper);
			visual_3d_matrix_pop(tr_state->visual);
		}
		
		gf_node_traverse(geometry, tr_state);
	
		/*back to SORT*/
		tr_state->traversing_mode = TRAVERSE_SORT;

		if (tr_state->has_clip) visual_3d_reset_clipper_2d(tr_state->visual);
		return;
	}


	GF_SAFEALLOC(ctx, Drawable3DContext);
	ctx->directional_lights = gf_list_new();
	ctx->geometry = geometry;
	ctx->appearance = tr_state->appear;

	memcpy(&ctx->model_matrix, &tr_state->model_matrix, sizeof(GF_Matrix));
	ctx->color_mat.identity = tr_state->color_mat.identity;
	if (!tr_state->color_mat.identity) memcpy(&ctx->color_mat, &tr_state->color_mat, sizeof(GF_ColorMatrix));

	ctx->pixel_metrics = tr_state->pixel_metrics;
	ctx->text_split_idx = tr_state->text_split_idx;
	
	i=0;
	while ((ol = (DirectionalLightContext*)gf_list_enum(tr_state->local_lights, &i))) {
		nl = (DirectionalLightContext*)malloc(sizeof(DirectionalLightContext));
		memcpy(nl, ol, sizeof(DirectionalLightContext));
		gf_list_add(ctx->directional_lights, nl);
	}
	ctx->clipper = tr_state->clipper;
	ctx->has_clipper = tr_state->has_clip;
	ctx->cull_flag = tr_state->cull_flag;

	if ((ctx->num_clip_planes = tr_state->num_clip_planes))
		memcpy(ctx->clip_planes, tr_state->clip_planes, sizeof(GF_Plane)*MAX_USER_CLIP_PLANES);

	/*get bbox and and insert from further to closest*/
	drawable = (Drawable3D*)gf_node_get_private(geometry);
	tr_state->bbox = drawable->mesh->bounds;
		
	gf_mx_apply_bbox(&ctx->model_matrix, &tr_state->bbox);
	gf_mx_apply_bbox(&tr_state->camera->modelview, &tr_state->bbox);
	ctx->zmax = tr_state->bbox.max_edge.z;

	/*we don't need an exact sorting, as long as we keep transparent nodes above  -note that for
	speed purposes we store in reverse-z transparent nodes*/
	count = gf_list_count(tr_state->visual->alpha_nodes_to_draw);
	for (i=0; i<count; i++) {
		Drawable3DContext *next = (Drawable3DContext *)gf_list_get(tr_state->visual->alpha_nodes_to_draw, i);
		if (next->zmax>ctx->zmax) {
			gf_list_insert(tr_state->visual->alpha_nodes_to_draw, ctx, i);
			return;
		}
	}
	gf_list_add(tr_state->visual->alpha_nodes_to_draw, ctx);
}

void visual_3d_flush_contexts(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	u32 i, idx, count;
	Bool pixel_metrics = tr_state->pixel_metrics;

	tr_state->traversing_mode = TRAVERSE_DRAW_3D;

	count = gf_list_count(visual->alpha_nodes_to_draw);
	for (idx=0; idx<count; idx++) {
		DirectionalLightContext *dl;
		Drawable3DContext *ctx = (Drawable3DContext *)gf_list_get(visual->alpha_nodes_to_draw, idx);

		visual_3d_matrix_push(visual);

		/*apply directional lights*/
		tr_state->local_light_on = 1;
		i=0;
		while ((dl = (DirectionalLightContext*)gf_list_enum(ctx->directional_lights, &i))) {
			visual_3d_matrix_push(visual);
			visual_3d_matrix_add(visual, dl->light_matrix.m);
			gf_node_traverse(dl->dlight, tr_state);
			visual_3d_matrix_pop(visual);
		}
		
		/*clipper, set it in world coords only*/
		if (ctx->has_clipper) {
			visual_3d_matrix_push(visual);
			visual_3d_matrix_reset(visual);
			visual_3d_set_clipper_2d(visual, ctx->clipper);
			visual_3d_matrix_pop(visual);
		}

		/*clip planes, set it in world coords only*/
		for (i=0; i<ctx->num_clip_planes; i++)
			visual_3d_set_clip_plane(visual, ctx->clip_planes[i]);

		/*restore traversing state*/
		visual_3d_matrix_add(visual, ctx->model_matrix.m);
		memcpy(&tr_state->model_matrix, &ctx->model_matrix, sizeof(GF_Matrix));
		tr_state->color_mat.identity = ctx->color_mat.identity;
		if (!tr_state->color_mat.identity) memcpy(&tr_state->color_mat, &ctx->color_mat, sizeof(GF_ColorMatrix));
		tr_state->text_split_idx = ctx->text_split_idx;
		tr_state->pixel_metrics = ctx->pixel_metrics;
		/*restore cull flag in case we're completely inside (avoids final frustum/AABB tree culling)*/
		tr_state->cull_flag = ctx->cull_flag;

		tr_state->appear = ctx->appearance;
		gf_node_traverse(ctx->geometry, tr_state);
		tr_state->appear = NULL;
		
		/*reset directional lights*/
		tr_state->local_light_on = 0;
		for (i=gf_list_count(ctx->directional_lights); i>0; i--) {
			DirectionalLightContext *dl = (DirectionalLightContext*)gf_list_get(ctx->directional_lights, i-1);
			gf_node_traverse(dl->dlight, tr_state);
			free(dl);
		}

		if (ctx->has_clipper) visual_3d_reset_clipper_2d(visual);
		for (i=0; i<ctx->num_clip_planes; i++) visual_3d_reset_clip_plane(visual);

		visual_3d_matrix_pop(visual);

		/*and destroy*/
		gf_list_del(ctx->directional_lights);
		free(ctx);
	}
	tr_state->pixel_metrics = pixel_metrics;
	gf_list_reset(tr_state->visual->alpha_nodes_to_draw);
}
static void visual_3d_draw_node(GF_TraverseState *tr_state, GF_Node *root_node)
{
	GF_Node *fog;
	if (!tr_state->camera || !tr_state->visual) return;

	visual_3d_init_draw(tr_state, 0);

	/*main visual, handle collisions*/
	if ((tr_state->visual==tr_state->visual->compositor->visual) && tr_state->camera->is_3D) 
		visual_3d_check_collisions(tr_state, NULL);

	/*setup fog*/
	fog = (GF_Node*) gf_list_get(tr_state->visual->fog_stack, 0);
	tr_state->traversing_mode = TRAVERSE_BINDABLE;
	if (Bindable_GetIsBound(fog)) gf_node_traverse(fog, tr_state);

	/*turn global lights on*/
	if (tr_state->visual->type_3d>1) {
		tr_state->traversing_mode = TRAVERSE_LIGHTING;
		gf_node_traverse(root_node, tr_state);
	}
	
	/*sort graph*/
	tr_state->traversing_mode = TRAVERSE_SORT;
	gf_node_traverse(root_node, tr_state);

	/*and draw*/
	visual_3d_flush_contexts(tr_state->visual, tr_state);

	/*and turn off lights*/
	visual_3d_clear_all_lights(tr_state->visual);
}

Bool visual_3d_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
#ifndef GPAC_DISABLE_SVG
	u32 time = gf_sys_clock();
#endif
	visual_3d_setup(visual);

	/*setup our traversing state*/
	visual_3d_setup_traversing_state(visual, tr_state);

	if (is_root_visual) {
		GF_SceneGraph *sg;
		u32 i, tag;
		visual_3d_draw_node(tr_state, root);

		/*extra scene graphs*/
		i=0; 
		while ((sg = (GF_SceneGraph*)gf_list_enum(visual->compositor->extra_scenes, &i))) {
			GF_Node *n = gf_sg_get_root_node(sg);
			if (!n) continue;
			
			tag = gf_node_get_tag(n);

			visual_3d_setup_traversing_state(visual, tr_state);

			tr_state->traversing_mode = TRAVERSE_SORT;
			gf_node_traverse(n, tr_state);
		}
	} else {
		visual_3d_draw_node(tr_state, root);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] Frame\t%d\t3D drawn in \t%d\tms\n", visual->compositor->frame_number, gf_sys_clock() - time));
	
	return 1;
}

static void reset_collide_cursor(GF_Compositor *compositor)
{
	if (compositor->sensor_type == GF_CURSOR_COLLIDE) {
		GF_Event evt;
		compositor->sensor_type = evt.cursor.cursor_type = GF_CURSOR_NORMAL;
		evt.type = GF_EVENT_SET_CURSOR;
		compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	}
}

void visual_3d_check_collisions(GF_TraverseState *tr_state, GF_ChildNodeItem *node_list)
{
	SFVec3f n, pos, dir;
	Bool go;
	Fixed diff, pos_diff;

	assert(tr_state->visual && tr_state->camera);
	/*don't collide on VP animations or when modes discard collision*/
	if ((tr_state->camera->anim_len && !tr_state->camera->jumping) || !tr_state->visual->compositor->collide_mode || (tr_state->camera->navigate_mode>=GF_NAVIGATE_EXAMINE)) {
		/*reset ground flag*/
		tr_state->camera->last_had_ground = 0;
		/*and avoid reseting move at next collision change*/
		tr_state->camera->last_pos = tr_state->camera->position;
		return;
	}
	/*don't collide if not moved*/
	if (gf_vec_equal(tr_state->camera->position, tr_state->camera->last_pos)) {
		reset_collide_cursor(tr_state->visual->compositor);
		return;
	}
	tr_state->traversing_mode = TRAVERSE_COLLIDE;
	tr_state->camera->collide_flags = 0;
	tr_state->camera->collide_dist = FIX_MAX;
	tr_state->camera->ground_dist = FIX_MAX;
	if ((tr_state->camera->navigate_mode==GF_NAVIGATE_WALK) && tr_state->visual->compositor->gravity_on) tr_state->camera->collide_flags |= CF_DO_GRAVITY;

	gf_vec_diff(dir, tr_state->camera->position, tr_state->camera->last_pos);
	pos_diff = gf_vec_len(dir);
	gf_vec_norm(&dir);
	pos = tr_state->camera->position;
	
	diff = 0;
	go = 1;
	tr_state->camera->last_had_col = 0;
	/*some explanation: the current collision detection algo only checks closest distance
	to objects, but doesn't attempt to track object cross during a move. If we step more than
	the collision detection size, we may cross an object without detecting collision. we thus
	break the move in max collision size moves*/
	while (go) {
		if (pos_diff>tr_state->camera->avatar_size.x) {
			pos_diff-=tr_state->camera->avatar_size.x;
			diff += tr_state->camera->avatar_size.x;
		} else {
			diff += pos_diff;
			go = 0;
		}
		n = gf_vec_scale(dir, diff);	
		gf_vec_add(tr_state->camera->position, tr_state->camera->last_pos, n);	
		if (!node_list) {
			gf_node_traverse(gf_sg_get_root_node(tr_state->visual->compositor->scene), tr_state);
		} else {
			while (node_list) {
				gf_node_traverse(node_list->node, tr_state);
				node_list = node_list->next;
			}
		}
		if (tr_state->camera->collide_flags & CF_COLLISION) break;
		tr_state->camera->collide_flags &= ~CF_DO_GRAVITY;
	}

	/*gravity*/
	if (tr_state->camera->collide_flags & CF_GRAVITY) {
		diff = tr_state->camera->ground_dist - tr_state->camera->avatar_size.y;
		if (tr_state->camera->last_had_ground && (-diff>tr_state->camera->avatar_size.z)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Obstacle detected - too high (dist %g)\n", diff));
			tr_state->camera->position = tr_state->camera->last_pos;
			tr_state->camera->flags |= CAM_IS_DIRTY;
		} else {
			if ((tr_state->camera->jumping && fabs(diff)>tr_state->camera->dheight) 
				|| (!tr_state->camera->jumping && (ABS(diff)>FIX_ONE/1000) )) {
				tr_state->camera->last_had_ground = 1;
				n = gf_vec_scale(tr_state->camera->up, -diff);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Ground detected camera position: %g %g %g - offset: %g %g %g (dist %g)\n", tr_state->camera->position.x, tr_state->camera->position.y, tr_state->camera->position.z, n.x, n.y, n.z, diff));

				gf_vec_add(tr_state->camera->position, tr_state->camera->position, n);
				gf_vec_add(tr_state->camera->target, tr_state->camera->target, n);
				gf_vec_add(tr_state->camera->last_pos, tr_state->camera->position, n);
				tr_state->camera->flags |= CAM_IS_DIRTY;
			}
		}
	}
	/*collsion found*/
	if (tr_state->camera->collide_flags & CF_COLLISION) {
		if (tr_state->visual->compositor->sensor_type != GF_CURSOR_COLLIDE) {
			GF_Event evt;
			tr_state->camera->last_had_col = 1;
			evt.type = GF_EVENT_SET_CURSOR;
			tr_state->visual->compositor->sensor_type = evt.cursor.cursor_type = GF_CURSOR_COLLIDE;
			tr_state->visual->compositor->video_out->ProcessEvent(tr_state->visual->compositor->video_out, &evt);
		}

		/*regular collision*/
		if (tr_state->visual->compositor->collide_mode==GF_COLLISION_NORMAL) {
			tr_state->camera->position = tr_state->camera->last_pos;
			tr_state->camera->flags |= CAM_IS_DIRTY;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Collision detected - restoring previous avatar position\n"));
		} else {
			/*camera displacement collision*/
			if (tr_state->camera->collide_dist) {
				if (tr_state->camera->collide_dist>=tr_state->camera->avatar_size.x)
					GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Collision] Collision distance %g greater than avatar collide size %g\n", tr_state->camera->collide_dist, tr_state->camera->avatar_size.x));

				/*safety check due to precision, always stay below collide dist*/
				if (tr_state->camera->collide_dist>=tr_state->camera->avatar_size.x) tr_state->camera->collide_dist = tr_state->camera->avatar_size.x;

				gf_vec_diff(n, tr_state->camera->position, tr_state->camera->collide_point);
				gf_vec_norm(&n);
				n = gf_vec_scale(n, tr_state->camera->avatar_size.x - tr_state->camera->collide_dist);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] offseting camera: position: %g %g %g - offset: %g %g %g\n", tr_state->camera->position.x, tr_state->camera->position.y, tr_state->camera->position.z, n.x, n.y, n.z));
				gf_vec_add(tr_state->camera->position, tr_state->camera->position, n);
				gf_vec_add(tr_state->camera->target, tr_state->camera->target, n);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Collision detected and camera on hit point - restoring previous avatar position\n"));
				tr_state->camera->position = tr_state->camera->last_pos;
			}
			tr_state->camera->last_pos = tr_state->camera->position;
			tr_state->camera->flags |= CAM_IS_DIRTY;
		}
	} else {
		reset_collide_cursor(tr_state->visual->compositor);
		tr_state->camera->last_pos = tr_state->camera->position;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] no collision found\n"));
	}

	if (tr_state->camera->flags & CAM_IS_DIRTY) visual_3d_setup_projection(tr_state);
}

/*uncomment to disable frustum cull*/
//#define DISABLE_VIEW_CULL

#ifndef GPAC_DISABLE_LOG
static const char *szPlaneNames [] = 
{
	"Near", "Far", "Left", "Right", "Bottom", "Top"
};
#endif

Bool visual_3d_node_cull(GF_TraverseState *tr_state, GF_BBox *bbox, Bool skip_near) 
{
	GF_BBox b;
	Fixed irad, rad;
	GF_Camera *cam;
	Bool do_sphere;
	u32 i, p_idx;
	SFVec3f cdiff, vertices[8];

	if (!tr_state->camera || (tr_state->cull_flag == CULL_INSIDE)) return 1;
	assert(tr_state->cull_flag != CULL_OUTSIDE);

#ifdef DISABLE_VIEW_CULL
	tr_state->cull_flag = CULL_INSIDE;
	return 1;
#endif

	/*empty bounds*/
	if (!bbox->is_set) {
		tr_state->cull_flag = CULL_OUTSIDE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node out (bbox not set)\n"));
		return 0;
	}

	/*get bbox sphere in world space*/
	b = *bbox;
	gf_mx_apply_bbox_sphere(&tr_state->model_matrix, &b);	
	cam = tr_state->camera;

	/*if camera is inside bbox consider we intersect*/
	if (gf_bbox_point_inside(&b, &cam->position)) {
		tr_state->cull_flag = CULL_INTERSECTS;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node intersect (camera in box test)\n"));
		return 1;
	}
	/*first check: sphere vs frustum sphere intersection, this will discard far objects quite fast*/
	gf_vec_diff(cdiff, cam->center, b.center);
	rad = b.radius + cam->radius;
	if (gf_vec_len(cdiff) > rad) {
		tr_state->cull_flag = CULL_OUTSIDE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node out (sphere-sphere test)\n"));
		return 0;
	}

	/*second check: sphere vs frustum planes intersection, if any intersection is detected switch 
	to n/p vertex check.*/
	rad = b.radius;
	irad = -b.radius;
	do_sphere = 1;
	
	/*skip near/far tests in ortho mode, and near in 3D*/
	i = (tr_state->camera->is_3D) ? (skip_near ? 1 : 0) : 2;
	for (; i<6; i++) {
		Fixed d;
		if (do_sphere) {
			d = gf_plane_get_distance(&cam->planes[i], &b.center);
			if (d<irad) {
				tr_state->cull_flag = CULL_OUTSIDE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node out (sphere-planes test) plane %s\n", szPlaneNames[i]));
				return 0;
			}
			/*intersect, move to n-p vertex test*/
			if (d<rad) {
				/*get full bbox in world coords*/
				b = *bbox;
				gf_mx_apply_bbox(&tr_state->model_matrix, &b);
				/*get box vertices*/
				gf_bbox_get_vertices(b.min_edge, b.max_edge, vertices);
				do_sphere = 0;
			} else {
				continue;
			}
		}
		p_idx = cam->p_idx[i];
		/*check p-vertex: if not in plane, we're out (since p-vertex is the closest point to the plane)*/
		d = gf_plane_get_distance(&cam->planes[i], &vertices[p_idx]);
		if (d<0) {
			tr_state->cull_flag = CULL_OUTSIDE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node out (p-vertex test) plane %s - Distance %g\n", szPlaneNames[i], FIX2FLT(d) ));
			return 0;
		}
		
		/*check n-vertex: if not in plane, we're intersecting - don't check for near and far planes*/
		if (i>1) {
			d = gf_plane_get_distance(&cam->planes[i], &vertices[7-p_idx]);
			if (d<0) {
				tr_state->cull_flag = CULL_INTERSECTS;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node intersect (n-vertex test) plane %s - Distance %g\n", szPlaneNames[i], FIX2FLT(d) ));
				return 1;
			}
		}
	}

	tr_state->cull_flag = CULL_INSIDE;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node inside (%s test)\n", do_sphere ? "sphere-planes" : "n-p vertex"));
	return 1;
}

void visual_3d_pick_node(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	Fixed in_x, in_y, x, y;
	SFVec3f start, end;
	SFVec4f res;

	visual_3d_setup_traversing_state(visual, tr_state);
	visual_3d_setup_projection(tr_state);

	x = INT2FIX(ev->mouse.x); y = INT2FIX(ev->mouse.y);

	/*if coordinate system is not centered, move to centered coord before applying camera transform
	because the (un)projection matrices include this transform*/
	if (!visual->center_coords) {
		x = x - INT2FIX(tr_state->camera->width)/2;
		y = INT2FIX(tr_state->camera->height)/2 - y;
	}


	/*main visual with AR*/
	if ((visual->compositor->visual == visual) && visual->compositor->has_size_info) {
		Fixed scale = gf_divfix(INT2FIX(visual->width), INT2FIX(visual->compositor->vp_width));
		x = gf_mulfix(x, scale);
		scale = gf_divfix(INT2FIX(visual->height), INT2FIX(visual->compositor->vp_height));
		y = gf_mulfix(y, scale);
	}

	start.z = visual->camera.z_near;
	end.z = visual->camera.z_far;
	if (!tr_state->camera->is_3D && !tr_state->pixel_metrics) {
		start.x = end.x = gf_divfix(x, tr_state->min_hsize); 
		start.y = end.y = gf_divfix(y, tr_state->min_hsize);
	} else {
		start.x = end.x = x;
		start.y = end.y = y;
	}

	/*unproject to world coords*/
	in_x = 2*x/ (s32) visual->width;
	in_y = 2*y/ (s32) visual->height;
	
	res.x = in_x; res.y = in_y; res.z = -FIX_ONE; res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&visual->camera.unprojection, &res);
	if (!res.q) return;
	start.x = gf_divfix(res.x, res.q); start.y = gf_divfix(res.y, res.q); start.z = gf_divfix(res.z, res.q);

	res.x = in_x; res.y = in_y; res.z = FIX_ONE; res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&visual->camera.unprojection, &res);
	if (!res.q) return;
	end.x = gf_divfix(res.x, res.q); end.y = gf_divfix(res.y, res.q); end.z = gf_divfix(res.z, res.q);

	tr_state->ray = gf_ray(start, end);
	/*also update hit info world ray in case we have a grabbed sensor with mouse off*/
	visual->compositor->hit_world_ray = tr_state->ray;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] cast ray\n\tOrigin %.4f %.4f %.4f - End %.4f %.4f %.4f\n\tDir %.4f %.4f %.4f\n", 
		FIX2FLT(tr_state->ray.orig.x), FIX2FLT(tr_state->ray.orig.y), FIX2FLT(tr_state->ray.orig.z),
		FIX2FLT(end.x), FIX2FLT(end.y), FIX2FLT(end.z),
		FIX2FLT(tr_state->ray.dir.x), FIX2FLT(tr_state->ray.dir.y), FIX2FLT(tr_state->ray.dir.z)));

	 

	visual->compositor->hit_square_dist = 0;
	visual->compositor->hit_node = NULL;
	gf_list_reset(visual->compositor->sensors);

	/*not the root scene, use children list*/
	if (visual->compositor->visual != visual) {
		while (children) {
			gf_node_traverse(children->node, tr_state);
			children = children->next;
		}
	} else {
		gf_node_traverse(gf_sg_get_root_node(visual->compositor->scene), tr_state);
	}
}


void visual_3d_drawable_pick(GF_Node *n, GF_TraverseState *tr_state, GF_Mesh *mesh, GF_Path *path) 
{
	SFVec3f local_pt, world_pt, vdiff;
	SFVec3f hit_normal;
	SFVec2f text_coords;
	u32 i, count;
	Fixed sqdist;
	Bool node_is_over;
	GF_Compositor *compositor;
	GF_Matrix mx;
	GF_Ray r;
	u32 cull_bckup = tr_state->cull_flag;

	if (!mesh && !path) return;

	count = gf_list_count(tr_state->vrml_sensors);
	compositor = tr_state->visual->compositor;

	node_is_over = 0;
	if (mesh) {
		if (mesh->mesh_type!=MESH_TRIANGLES) 
			return;
		if (!visual_3d_node_cull(tr_state, &mesh->bounds, 0)) {
			tr_state->cull_flag = cull_bckup;
			return;
		}
	}
	tr_state->cull_flag = cull_bckup;
	r = tr_state->ray;
	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_inverse(&mx);
	gf_mx_apply_ray(&mx, &r);

	/*if we already have a hit point don't check anything below...*/
	if (compositor->hit_square_dist && !compositor->grabbed_sensor && !tr_state->layer3d) {
		GF_Plane p;
		GF_BBox box;
		SFVec3f hit = compositor->hit_world_point;
		gf_mx_apply_vec(&mx, &hit);
		p.normal = r.dir;
		p.d = -1 * gf_vec_dot(p.normal, hit);
		if (mesh) 
			box = mesh->bounds;
		else
			gf_bbox_from_rect(&box, &path->bbox);

		if (gf_bbox_plane_relation(&box, &p) == GF_BBOX_FRONT) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] bounding box of node %s (DEF %s) below current hit point - skipping\n", gf_node_get_class_name(n), gf_node_get_name(n)));
			return;
		}
	}
	if (path) {
		node_is_over = 0;
		if (compositor_get_2d_plane_intersection(&r, &local_pt)) {
			if (gf_path_point_over(path, local_pt.x, local_pt.y)) {
				hit_normal.x = hit_normal.y = 0; hit_normal.z = FIX_ONE;
				text_coords.x = gf_divfix(local_pt.x, path->bbox.width) + FIX_ONE/2;
				text_coords.y = gf_divfix(local_pt.y, path->bbox.height) + FIX_ONE/2;
				node_is_over = 1;
			}
		}
	} else {
		node_is_over = gf_mesh_intersect_ray(mesh, &r, &local_pt, &hit_normal, &text_coords);
	}

	if (!node_is_over) return;

	/*check distance from user and keep the closest hitpoint*/
	world_pt = local_pt;
	gf_mx_apply_vec(&tr_state->model_matrix, &world_pt);

	for (i=0; i<tr_state->num_clip_planes; i++) {
		if (gf_plane_get_distance(&tr_state->clip_planes[i], &world_pt) < 0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] node %s (def %s) is not in clipper half space\n", gf_node_get_class_name(n), gf_node_get_name(n)));
			return;
		}
	}

	gf_vec_diff(vdiff, world_pt, tr_state->ray.orig);
	sqdist = gf_vec_lensq(vdiff);
	if (compositor->hit_square_dist && (compositor->hit_square_dist+FIX_EPSILON<sqdist)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] node %s (def %s) is farther (%g) than current pick (%g)\n", gf_node_get_class_name(n), gf_node_get_name(n), FIX2FLT(sqdist), FIX2FLT(compositor->hit_square_dist)));
		return;
	}

	compositor->hit_square_dist = sqdist;
	gf_list_reset(compositor->sensors);
	for (i=0; i<count; i++) {
		gf_list_add(compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}

	gf_mx_copy(compositor->hit_world_to_local, tr_state->model_matrix);
	gf_mx_copy(compositor->hit_local_to_world, mx);
	compositor->hit_local_point = local_pt;
	compositor->hit_world_point = world_pt;
	compositor->hit_world_ray = tr_state->ray;
	compositor->hit_normal = hit_normal;
	compositor->hit_texcoords = text_coords;

	if (compositor_is_composite_texture(tr_state->appear)) {
		compositor->hit_appear = tr_state->appear;
	} else {
		compositor->hit_appear = NULL;
	}
	compositor->hit_node = n;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] node %s (def %s) is under mouse - hit %g %g %g\n", gf_node_get_class_name(n), gf_node_get_name(n),
			FIX2FLT(world_pt.x), FIX2FLT(world_pt.y), FIX2FLT(world_pt.z)));
}


void visual_3d_drawable_collide(GF_Node *node, GF_TraverseState *tr_state)
{
	SFVec3f pos, v1, v2, collide_pt, last_pos;
	Fixed dist, m_dist;
	GF_Matrix mx;
	u32 ntag, cull_backup;
	Drawable3D *st = (Drawable3D *)gf_node_get_private(node);
	if (!st || !st->mesh) return;

	/*no collision with lines & points*/
	if (st->mesh->mesh_type != MESH_TRIANGLES) return;
	/*no collision with text (vrml)*/
	ntag = gf_node_get_tag(node);
	if ((ntag==TAG_MPEG4_Text) || (ntag==TAG_X3D_Text)) return;


	/*cull but don't use near plane to detect objects behind us*/
	cull_backup = tr_state->cull_flag;
	if (!visual_3d_node_cull(tr_state, &st->mesh->bounds, 1)) {
		tr_state->cull_flag = cull_backup;
		return;
	}
	tr_state->cull_flag = cull_backup;

	/*use up & front to get an average size of the collision dist in this space*/
	pos = tr_state->camera->position;
	last_pos = tr_state->camera->last_pos;
	v1 = camera_get_target_dir(tr_state->camera);
	v1 = gf_vec_scale(v1, tr_state->camera->avatar_size.x);
	gf_vec_add(v1, v1, pos);
	v2 = camera_get_right_dir(tr_state->camera);
	v2 = gf_vec_scale(v2, tr_state->camera->avatar_size.x);
	gf_vec_add(v2, v2, pos);

	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_inverse(&mx);

	gf_mx_apply_vec(&mx, &pos);
	gf_mx_apply_vec(&mx, &last_pos);
	gf_mx_apply_vec(&mx, &v1);
	gf_mx_apply_vec(&mx, &v2);

	gf_vec_diff(v1, v1, pos);
	gf_vec_diff(v2, v2, pos);
	dist = gf_vec_len(v1);
	m_dist = gf_vec_len(v2);
	if (dist<m_dist) m_dist = dist;

	/*check for any collisions*/
	if (gf_mesh_closest_face(st->mesh, pos, m_dist, &collide_pt)) {
		/*get transformed hit*/
		gf_mx_apply_vec(&tr_state->model_matrix, &collide_pt);
		gf_vec_diff(v2, tr_state->camera->position, collide_pt);
		dist = gf_vec_len(v2);
		if (dist<tr_state->camera->collide_dist) {
			tr_state->camera->collide_dist = dist;
			tr_state->camera->collide_flags |= CF_COLLISION;
			tr_state->camera->collide_point = collide_pt;

#ifndef GPAC_DISABLE_LOG
			if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_COMPOSE)) { 				
				gf_vec_diff(v1, pos, collide_pt);
				gf_vec_norm(&v1);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] found at %g %g %g (WC) - dist (%g) - local normal %g %g %g\n", 
					tr_state->camera->collide_point.x, tr_state->camera->collide_point.y, tr_state->camera->collide_point.z, 
					dist, 
					v1.x, v1.y, v1.z));
			}
#endif
		} 
		else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Existing collision (dist %g) closer than current collsion (dist %g)\n", tr_state->camera->collide_dist, dist));
		}
	}

	if (tr_state->camera->collide_flags & CF_DO_GRAVITY) {
		GF_Ray r;
		Bool intersect;
		r.orig = tr_state->camera->position;
		r.dir = gf_vec_scale(tr_state->camera->up, -FIX_ONE);
		gf_mx_apply_ray(&mx, &r);

		intersect = gf_mesh_intersect_ray(st->mesh, &r, &collide_pt, &v1, NULL);

		if (intersect) {
			gf_mx_apply_vec(&tr_state->model_matrix, &collide_pt);
			gf_vec_diff(v2, tr_state->camera->position, collide_pt);
			dist = gf_vec_len(v2);
			if (dist<tr_state->camera->ground_dist) {
				tr_state->camera->ground_dist = dist;
				tr_state->camera->collide_flags |= CF_GRAVITY;
				tr_state->camera->ground_point = collide_pt;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Ground found at %g %g %g (WC) - dist %g - local normal %g %g %g\n", 
					tr_state->camera->ground_point.x, tr_state->camera->ground_point.y, tr_state->camera->ground_point.z, 
					dist, 
					v1.x, v1.y, v1.z));
			} 
			else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Existing ground (dist %g) closer than current (dist %g)\n", tr_state->camera->ground_dist, dist));
			}
		} 
	}
}

static GF_TextureHandler *visual_3d_setup_texture_2d(GF_TraverseState *tr_state, DrawAspect2D *asp, Bool is_svg, GF_Mesh *mesh)
{
	if (!asp->fill_texture) return NULL;

	if (asp->fill_color && (GF_COL_A(asp->fill_color) != 0xFF)) {
		visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
		gf_sc_texture_set_blend_mode(asp->fill_texture, TX_MODULATE);
	} else {
		visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);
		gf_sc_texture_set_blend_mode(asp->fill_texture, TX_REPLACE);
	}

	if (is_svg) {
		GF_Rect rc;
		gf_rect_from_bbox(&rc, &mesh->bounds);
		tr_state->mesh_has_texture = gf_sc_texture_enable_ex(asp->fill_texture, NULL, &rc);
	} else {
		tr_state->mesh_has_texture = gf_sc_texture_enable(asp->fill_texture, ((M_Appearance *)tr_state->appear)->textureTransform);
	}
	if (tr_state->mesh_has_texture) return asp->fill_texture;
	return NULL;
}

void visual_3d_set_2d_strike(GF_TraverseState *tr_state, DrawAspect2D *asp)
{
	if (asp->line_texture) {
		/*We forgot to specify this in the spec ...*/
		gf_sc_texture_set_blend_mode(asp->line_texture, TX_REPLACE);
#if 0
		if (asp->line_alpha != FIX_ONE) {
			visual_3d_set_material_2d(tr_state->visual, asp->line_color, asp->line_alpha);
			gf_sc_texture_set_blend_mode(asp->txh, TX_MODULATE);
		} else {
			visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);
			gf_sc_texture_set_blend_mode(asp->txh, TX_REPLACE);
		}
#endif
		tr_state->mesh_has_texture = gf_sc_texture_enable(asp->line_texture, NULL/*asp->tx_trans*/);
		if (tr_state->mesh_has_texture) return;
	}
	/*no texture or not ready, use color*/
	if (asp->line_color)
		visual_3d_set_material_2d_argb(tr_state->visual, asp->line_color);
}


void visual_3d_draw_2d_with_aspect(Drawable *st, GF_TraverseState *tr_state, DrawAspect2D *asp, Bool is_svg)
{
	StrikeInfo2D *si;
	GF_TextureHandler *fill_txh;

	fill_txh = visual_3d_setup_texture_2d(tr_state, asp, is_svg, st->mesh);

	/*fill path*/
	if (fill_txh || (GF_COL_A(asp->fill_color)) ) {
		if (!st->mesh) return;

		if (asp->fill_color) 
			visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
		
		visual_3d_mesh_paint(tr_state, st->mesh);
		/*reset texturing in case of line texture*/
		if (tr_state->mesh_has_texture) {
			gf_sc_texture_disable(fill_txh);
			tr_state->mesh_has_texture = 0;
		}
	}

	/*strike path*/
	if (!asp->pen_props.width) return;

	si = drawable_get_strikeinfo(tr_state->visual->compositor, st, asp, tr_state->appear, NULL, 0, tr_state);
	if (!si) return;

	if (!si->mesh_outline) {
		si->is_vectorial = !tr_state->visual->compositor->raster_outlines;
		si->mesh_outline = new_mesh();
#ifndef GPAC_USE_OGL_ES
		if (si->is_vectorial) {
			TesselatePath(si->mesh_outline, si->outline, asp->line_texture ? 2 : 1);
		} else 
#endif
			mesh_get_outline(si->mesh_outline, st->path);
	}

	visual_3d_set_2d_strike(tr_state, asp);
	if (asp->line_texture) tr_state->mesh_has_texture = 1;

	if (!si->is_vectorial) {
		visual_3d_mesh_strike(tr_state, si->mesh_outline, asp->pen_props.width, asp->line_scale, asp->pen_props.dash);
	} else {
		visual_3d_mesh_paint(tr_state, si->mesh_outline);
	}
	if (asp->line_texture) {
		gf_sc_texture_disable(asp->line_texture);
		tr_state->mesh_has_texture = 0;
	}		
}

void visual_3d_draw_2d(Drawable *st, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;
	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(st->node, &asp, tr_state);
	visual_3d_draw_2d_with_aspect(st, tr_state, &asp, 0);
}

void visual_3d_draw_from_context(DrawableContext *ctx, GF_TraverseState *tr_state)
{
	GF_Rect rc;
	gf_path_get_bounds(ctx->drawable->path, &rc);
	visual_3d_draw_2d_with_aspect(ctx->drawable, tr_state, &ctx->aspect, 1);

	drawable_check_focus_highlight(ctx->drawable->node, tr_state, &rc);
}


static GFINLINE Bool visual_3d_setup_material(GF_TraverseState *tr_state, u32 mesh_type)
{
	SFColor def;
	GF_Node *__mat;
	def.red = def.green = def.blue = FIX_ONE;
	/*temp storage of diffuse alpha*/
	tr_state->ray.orig.x = FIX_ONE;

	if (!tr_state->appear) {
		/*use material2D to disable lighting*/
		visual_3d_set_material_2d(tr_state->visual, def, FIX_ONE);
		return 1;
	}

	if (gf_node_get_tag(tr_state->appear)==TAG_X3D_Appearance) {
		X_FillProperties *fp = (X_FillProperties *) ((X_Appearance*)tr_state->appear)->fillProperties;
		if (fp && !fp->filled) return 0;
	}

	__mat = ((M_Appearance *)tr_state->appear)->material;
	if (!__mat) {
		/*use material2D to disable lighting (cf VRML specs)*/
		visual_3d_set_material_2d(tr_state->visual, def, FIX_ONE);
		return 1;
	} 
	
	switch (gf_node_get_tag((GF_Node *)__mat)) {
	case TAG_MPEG4_Material:
	case TAG_X3D_Material:
	{
		SFColor diff, spec, emi;
		Fixed diff_a, spec_a, emi_a;
		Fixed vec[4];
		Bool has_alpha;
		u32 flag = V3D_STATE_LIGHT /*| V3D_STATE_COLOR*/;
		M_Material *mat = (M_Material *)__mat;

		diff = mat->diffuseColor;
		diff_a = FIX_ONE - mat->transparency;

		/*if drawing in 2D context or special meshes (lines, points) disable lighting*/
		if (mesh_type || !tr_state->camera->is_3D) {
			if (tr_state->camera->is_3D) diff = mat->emissiveColor;
			if (!tr_state->color_mat.identity) gf_cmx_apply_fixed(&tr_state->color_mat, &diff_a, &diff.red, &diff.green, &diff.blue);
			visual_3d_set_material_2d(tr_state->visual, diff, diff_a);
			return 1;
		}

		spec = mat->specularColor;
		emi = mat->emissiveColor;
		spec_a = emi_a = FIX_ONE - mat->transparency;
		if (!tr_state->color_mat.identity) {
			gf_cmx_apply_fixed(&tr_state->color_mat, &diff_a, &diff.red, &diff.green, &diff.blue);
			gf_cmx_apply_fixed(&tr_state->color_mat, &spec_a, &spec.red, &spec.green, &spec.blue);
			gf_cmx_apply_fixed(&tr_state->color_mat, &emi_a, &emi.red, &emi.green, &emi.blue);

			if ((diff_a+FIX_EPSILON<FIX_ONE) || (spec_a+FIX_EPSILON<FIX_ONE) || (emi_a+FIX_EPSILON<FIX_ONE )) {
				has_alpha = 1;
			} else {
				has_alpha = 0;
			}
		} else {
			has_alpha = (mat->transparency>FIX_EPSILON) ? 1 : 0;
			/*100% transparent DON'T DRAW*/
			if (mat->transparency+FIX_EPSILON>=FIX_ONE) return 0;
		}

		/*using antialiasing with alpha usually gives bad results (non-edge face segments are visible)*/
		visual_3d_enable_antialias(tr_state->visual, !has_alpha);
		if (has_alpha) {
			flag |= V3D_STATE_BLEND;
			tr_state->mesh_is_transparent = 1;
		}
		visual_3d_set_state(tr_state->visual, flag, 1);

		vec[0] = gf_mulfix(diff.red, mat->ambientIntensity);
		vec[1] = gf_mulfix(diff.green, mat->ambientIntensity);
		vec[2] = gf_mulfix(diff.blue, mat->ambientIntensity);
		vec[3] = diff_a;
		visual_3d_set_material(tr_state->visual, V3D_MATERIAL_AMBIENT, vec);

		vec[0] = diff.red;
		vec[1] = diff.green;
		vec[2] = diff.blue;
		vec[3] = diff_a;
		visual_3d_set_material(tr_state->visual, V3D_MATERIAL_DIFFUSE, vec);

		vec[0] = spec.red;
		vec[1] = spec.green;
		vec[2] = spec.blue;
		vec[3] = spec_a;
		visual_3d_set_material(tr_state->visual, V3D_MATERIAL_SPECULAR, vec);

		
		vec[0] = emi.red;
		vec[1] = emi.green;
		vec[2] = emi.blue;
		vec[3] = emi_a;
		visual_3d_set_material(tr_state->visual, V3D_MATERIAL_EMISSIVE, vec);

		visual_3d_set_shininess(tr_state->visual, mat->shininess);
		tr_state->ray.orig.x = diff_a;
	}
		break;
	case TAG_MPEG4_Material2D:
	{
		SFColor emi;
		Fixed emi_a;
		M_Material2D *mat = (M_Material2D *)__mat;

		emi = mat->emissiveColor;
		emi_a = FIX_ONE - mat->transparency;
		if (!tr_state->color_mat.identity) gf_cmx_apply_fixed(&tr_state->color_mat, &emi_a, &emi.red, &emi.green, &emi.blue);
		/*100% transparent DON'T DRAW*/
		if (emi_a<FIX_EPSILON) return 0;
		else if (emi_a+FIX_EPSILON<FIX_ONE) visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 1);


		/*this is an extra feature: if material2D.filled is FALSE on 3D objects, switch to TX_REPLACE mode
		and enable lighting*/
		if (!mat->filled) {
			GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
			if (txh) {
				gf_sc_texture_set_blend_mode(txh, TX_REPLACE);
				visual_3d_set_state(tr_state->visual, V3D_STATE_COLOR, 0);
				visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT, 1);
				return 1;
			}
		}
		/*regular mat 2D*/
		visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT | V3D_STATE_COLOR, 0);
		visual_3d_set_material_2d(tr_state->visual, emi, emi_a);
	}	
		break;
	default:
		break;
	}
	return 1;
}

Bool visual_3d_setup_texture(GF_TraverseState *tr_state)
{
	GF_TextureHandler *txh;
	tr_state->mesh_has_texture = 0;
	if (!tr_state->appear) return 0;

	gf_node_dirty_reset(tr_state->appear);

	txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
	if (txh) {
		gf_sc_texture_set_blend_mode(txh, gf_sc_texture_is_transparent(txh) ? TX_MODULATE : TX_REPLACE);
		tr_state->mesh_has_texture = gf_sc_texture_enable(txh, ((M_Appearance *)tr_state->appear)->textureTransform);
		if (tr_state->mesh_has_texture) {
			Fixed v[4];
			switch (txh->pixelformat) {
			/*override diffuse color with full intensity, but keep material alpha (cf VRML lighting)*/
			case GF_PIXEL_RGB_24:
				v[0] = v[1] = v[2] = 1; v[3] = tr_state->ray.orig.x;
				visual_3d_set_material(tr_state->visual, V3D_MATERIAL_DIFFUSE, v);
				break;
			/*override diffuse color AND material alpha (cf VRML lighting)*/
			case GF_PIXEL_RGBA:
				v[0] = v[1] = v[2] = v[3] = 1;
				visual_3d_set_material(tr_state->visual, V3D_MATERIAL_DIFFUSE, v);
				tr_state->mesh_is_transparent = 1;
				break;
			case GF_PIXEL_GREYSCALE:
				tr_state->mesh_has_texture = 2;
				break;
			}
		}
		return tr_state->mesh_has_texture;
	}
	return 0;
}

void visual_3d_disable_texture(GF_TraverseState *tr_state)
{
	if (tr_state->mesh_has_texture) {
		gf_sc_texture_disable(gf_sc_texture_get_handler( ((M_Appearance *)tr_state->appear)->texture) );
		tr_state->mesh_has_texture = 0;
	}
}

Bool visual_3d_setup_appearance(GF_TraverseState *tr_state)
{
	/*setup material and check if 100% transparent - in which case don't draw*/
	if (!visual_3d_setup_material(tr_state, 0)) return 0;
	/*setup texture*/
	visual_3d_setup_texture(tr_state);
	return 1;
}


void visual_3d_draw(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	if (mesh->mesh_type) {
		if (visual_3d_setup_material(tr_state, mesh->mesh_type)) {
			visual_3d_mesh_paint(tr_state, mesh);
		}
	} else if (visual_3d_setup_appearance(tr_state)) {
		visual_3d_mesh_paint(tr_state, mesh);
		visual_3d_disable_texture(tr_state);
	
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
		if (tr_state->appear && gf_node_get_tag(tr_state->appear)==TAG_X3D_Appearance) {
			X_Appearance *ap = (X_Appearance *)tr_state->appear;
			X_FillProperties *fp = ap->fillProperties ? (X_FillProperties *) ap->fillProperties : NULL;
			if (fp && fp->hatched) visual_3d_mesh_hatch(tr_state, mesh, fp->hatchStyle, fp->hatchColor);
		}
#endif
	}
}



#endif
