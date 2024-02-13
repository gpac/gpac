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
#include "texturing.h"
#include "nodes_stacks.h"

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_COMPOSITOR)

/*generic drawable 3D constructor/destructor*/
Drawable3D *drawable_3d_new(GF_Node *node)
{
	Drawable3D *tmp;
	GF_SAFEALLOC(tmp, Drawable3D);
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate drawable 3D stack\n"));
		return NULL;
	}
	tmp->mesh = new_mesh();
	gf_node_set_private(node, tmp);
	return tmp;
}
void drawable_3d_del(GF_Node *n)
{
	Drawable3D *d = (Drawable3D *)gf_node_get_private(n);
	if (d) {
		if (d->mesh) mesh_free(d->mesh);
		gf_free(d);
	}
	gf_sc_check_focus_upon_destroy(n);
}


void drawable3d_check_focus_highlight(GF_Node *node, GF_TraverseState *tr_state, GF_BBox *orig_bounds)
{
	Drawable *hlight;
	GF_Node *prev_node;
	u32 prev_mode;
	GF_BBox *bounds;
	GF_Matrix cur;
	GF_Compositor *compositor = tr_state->visual->compositor;

	if (compositor->disable_focus_highlight) return;

	if (compositor->focus_node!=node) return;

	hlight = compositor->focus_highlight;
	if (!hlight) return;

	/*check if focus node has changed*/
	prev_node = gf_node_get_private(hlight->node);
	if (prev_node != node) {
		gf_node_set_private(hlight->node, node);

		drawable_reset_path(hlight);
		gf_path_reset(hlight->path);
	}
	/*this is a grouping node, get its bounds*/
	if (!orig_bounds) {
		gf_mx_copy(cur, tr_state->model_matrix);
		gf_mx_init(tr_state->model_matrix);
		prev_mode = tr_state->traversing_mode;
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		tr_state->bbox.is_set = 0;

//		gf_sc_get_nodes_bounds(node, ((GF_ParentNode *)node)->children, tr_state, 1);
		gf_node_traverse_children(node, tr_state);

		tr_state->traversing_mode = prev_mode;
		gf_mx_copy(tr_state->model_matrix, cur);
		bounds = &tr_state->bbox;
	} else {
		bounds = orig_bounds;
	}
	visual_3d_draw_bbox(tr_state, bounds, GF_FALSE);
}


static void visual_3d_setup_traversing_state(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	tr_state->visual = visual;
	tr_state->camera = &visual->camera;
#ifndef GPAC_DISABLE_VRML
	tr_state->backgrounds = visual->back_stack;
	tr_state->viewpoints = visual->view_stack;
	tr_state->fogs = visual->fog_stack;
	tr_state->navigations = visual->navigation_stack;
#endif
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
			if (visual->type_3d==0) {
				tr_state->camera->width = INT2FIX(tr_state->visual->width);
				tr_state->camera->height = INT2FIX(tr_state->visual->height);
			} else {
				tr_state->camera->width = INT2FIX(tr_state->visual->compositor->vp_width);
				tr_state->camera->height = INT2FIX(tr_state->visual->compositor->vp_height);
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
	/*if pixel metrics, the default znear may be way too far and lead to weird navigation*/
	else if (tr_state->camera->z_near>=FIX_ONE) tr_state->camera->z_near = FIX_ONE/2;
	tr_state->camera->z_near /= 2;
	tr_state->camera->z_far = tr_state->camera->visibility;

	/*z_far is selected so that an object the size of the viewport measures
	one pixel when located at the far plane. It can be found through the projection
	transformation (projection matrix) of x and y
		x transformation is: x'= (1/(ar*tg(fov/2)) )*x/z
		y transformation is: y'=(1/(tg(fov/2)))*x/z

	therefore when z=z_far and x=max(width/2, height/2), then
	x' = 1/max(vp_size.x, vp_size.y) (transformed OpenGL viewport measures one)

	this yields z_far = max(vp_size.x, vp_size.y) * max(width/2, height/2) * max(1/(ar*tg(fov/2)), 1/tg(fov/2))
		z_far = max(vp_size.x, vp_size.y) * max(width, height) / (2*min(1, ar)*tg(fov/2)) )

	to choose a z_far so that the size is more than one pixel, then z_far' = z_far/n_pixels*/
	if (tr_state->camera->z_far<=0) {
		Fixed ar = gf_divfix(tr_state->vp_size.x, tr_state->vp_size.y);
		if (ar>FIX_ONE) ar = FIX_ONE;
		tr_state->camera->z_far = gf_muldiv(
		                              MAX(tr_state->vp_size.x,tr_state->vp_size.y),
		                              MAX(tr_state->camera->width, tr_state->camera->height),
		                              gf_mulfix(ar*2, gf_tan(fieldOfView/2))
		                          );

		/*fixed-point overflow*/
		if (tr_state->camera->z_far <= tr_state->camera->z_near) {
			tr_state->camera->z_far = FIX_MAX/4;
		}
	}
	if (vp) {
#if 0
		/*now check if vp is in pixel metrics. If not then:
		- either it's in the main scene, there's nothing to do
		- or it's in an inline, and the inline has been scaled if main scene is in pm: nothing to do*/
		if ( gf_sg_use_pixel_metrics(gf_node_get_graph(vp))) {
			GF_Matrix mx;
			gf_mx_init(mx);
			gf_mx_add_scale(&mx, tr_state->min_hsize, tr_state->min_hsize, tr_state->min_hsize);
			gf_mx_apply_vec(&mx, &position);
			gf_mx_apply_vec(&mx, &local_center);
		}
#endif
	}
	/*default VP setup - this is undocumented in the spec. Default VP pos is (0, 0, 10) but not really nice
	in pixel metrics. We set z so that we see just the whole visual*/
	else if (tr_state->pixel_metrics) {
		if (tr_state->visual != tr_state->visual->compositor->visual) {
			position.z = gf_divfix(tr_state->camera->width, 2*gf_tan(fieldOfView/2) );
		} else {
			position.z = gf_mulfix(position.z, tr_state->min_hsize);
		}
	}
#ifdef GF_SR_USE_DEPTH
	/* 3D world calibration for stereoscopic screen */
	if (tr_state->visual->compositor->autocal && tr_state->visual->compositor->video_out->dispdist) {
		Fixed dispdist, disparity;

		/*get view distance in pixels*/
		dispdist = tr_state->visual->compositor->video_out->dispdist * tr_state->visual->compositor->video_out->dpi_x;
		dispdist = gf_divfix(dispdist , FLT2FIX(2.54f) );
		disparity = INT2FIX(tr_state->visual->compositor->video_out->disparity);

		if (tr_state->visual->depth_vp_range) {
			position.z = dispdist;
			tr_state->camera->z_near = dispdist - tr_state->visual->depth_vp_position + tr_state->visual->depth_vp_range/2;
			tr_state->camera->z_far = dispdist - tr_state->visual->depth_vp_position - tr_state->visual->depth_vp_range/2;
		}
		else if (disparity) {
			/*3,4 cm = 1,3386 inch -> pixels*/
			Fixed half_interocular_dist_pixel = FLT2FIX(1.3386) * tr_state->visual->compositor->video_out->dpi_x;

			//frustum placed to match user's real viewpoint
			position.z = dispdist;

			//near plane will match front side of the display's stereoscopic box
			//-> n=D- (dD)/(e+d)
			tr_state->camera->z_near = dispdist -
			                           gf_divfix( gf_mulfix(disparity,dispdist), (half_interocular_dist_pixel + disparity));
		}
		else if (tr_state->visual->compositor->dispdepth) {
			dist = INT2FIX(tr_state->visual->compositor->dispdepth);
			if (dist<0) dist = INT2FIX(tr_state->visual->height);

#if 1
			dispdist = gf_divfix(tr_state->visual->height, 2*gf_tan(fieldOfView/2) );
#else
			dispdist = gf_muldiv(dispdist, tr_state->visual->height, tr_state->visual->compositor->video_out->max_screen_height);
			fieldOfView = 2*gf_atan2(tr_state->visual->height/2, dispdist);
#endif

			//frustum placed to match user's real viewpoint
			position.z = dispdist;
			tr_state->camera->z_near = dispdist - 2*dist/3;
			tr_state->camera->z_far = dispdist + dist/2;
		}
	}
#endif

	gf_vec_diff(d, position, local_center);
	dist = gf_vec_len(d);

	if (!dist || (dist<tr_state->camera->z_near) || (dist > tr_state->camera->z_far)) {
		if (dist > tr_state->camera->z_far)
			tr_state->camera->z_far = 2*dist;

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

void visual_3d_setup_projection(GF_TraverseState *tr_state, Bool is_layer)
{
#ifndef GPAC_DISABLE_VRML
	GF_Node *bindable;
#endif
	u32 mode = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_BINDABLE;

	/*setup viewpoint (this directly modifies the frustum)*/
#ifndef GPAC_DISABLE_VRML
	bindable = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
		tr_state->camera->had_viewpoint = 1;
	} else
#endif
		if (tr_state->camera->had_viewpoint) {
			u32 had_vp = tr_state->camera->had_viewpoint;
			tr_state->camera->had_viewpoint = 0;
			if (tr_state->camera->is_3D) {
				SFVec3f pos, center;
				SFRotation r;
				Fixed fov = GF_PI/4;
#ifdef GF_SR_USE_DEPTH
				/* 3D world calibration for stereoscopic screen */
				if (tr_state->visual->compositor->autocal && tr_state->visual->compositor->video_out->dispdist) {
					/*get view distance in pixels*/
					Fixed dispdist = tr_state->visual->compositor->video_out->dispdist * tr_state->visual->compositor->video_out->dpi_x;
					dispdist = gf_divfix(dispdist , FLT2FIX(2.54f) );

					fov = 2*gf_atan2( INT2FIX(tr_state->visual->compositor->video_out->max_screen_width)/2, dispdist);
				}
#endif

				/*default viewpoint*/
				pos.x = pos.y = 0;
				pos.z = INT2FIX(10);
				center.x = center.y = center.z = 0;
				r.q = r.x = r.z = 0;
				r.y = FIX_ONE;
				/*this takes care of pixelMetrics*/
				visual_3d_viewpoint_change(tr_state, NULL, 0, fov, pos, r, center);
				/*initial vp compute, don't animate*/
				if (had_vp == 2) {
					camera_stop_anim(tr_state->camera);
					camera_reset_viewpoint(tr_state->camera, 0);
					/*scene not yet ready, force a recompute of world bounds at next frame*/
					if (!is_layer && gf_sc_fit_world_to_screen(tr_state->visual->compositor) == 0) {
						tr_state->camera->had_viewpoint = 2;
						gf_sc_invalidate(tr_state->visual->compositor, NULL);
					}
				}
			} else {
				tr_state->camera->flags &= ~CAM_HAS_VIEWPORT;
				tr_state->camera->flags |= CAM_IS_DIRTY;
			}
		}


	tr_state->camera_was_dirty = GF_FALSE;

	if (tr_state->visual->nb_views>1) {
		s32 view_idx;
		Fixed interocular_dist_pixel;
		Fixed delta = 0;

		interocular_dist_pixel = tr_state->visual->compositor->iod + tr_state->visual->compositor->interoccular_offset;

		view_idx = tr_state->visual->current_view;
		view_idx -= tr_state->visual->nb_views/2;
		delta = interocular_dist_pixel * view_idx;
		if (! (tr_state->visual->nb_views % 2)) {
			delta += interocular_dist_pixel/2;
		}
		if (tr_state->visual->compositor->rview) delta = - delta;

		tr_state->camera->flags |= CAM_IS_DIRTY;
		tr_state->camera_was_dirty = GF_TRUE;
		camera_update_stereo(tr_state->camera, &tr_state->transform, tr_state->visual->center_coords, delta, tr_state->visual->compositor->video_out->dispdist, tr_state->visual->compositor->focdist, tr_state->visual->camlay);
	} else {
		if (tr_state->camera->flags & CAM_IS_DIRTY) tr_state->camera_was_dirty = GF_TRUE;
		camera_update(tr_state->camera, &tr_state->transform, tr_state->visual->center_coords);
	}

	/*setup projection (we do this to avoidloading the projection matrix for each draw operation in non-GL3/GLES2 modes
		modelview will be loaded at each object
	*/
	visual_3d_projection_matrix_modified(tr_state->visual);
	gf_mx_init(tr_state->model_matrix);

	tr_state->traversing_mode = mode;
#ifdef GF_SR_USE_DEPTH
	tr_state->depth_offset = 0;
	tr_state->depth_gain = FIX_ONE;
#endif
}

static void visual_3d_draw_background(GF_TraverseState *tr_state, u32 layer_type)
{
	u32 mode;
#ifndef GPAC_DISABLE_VRML
	GF_Node *bindable;
#endif

	/*setup background*/
	mode = tr_state->traversing_mode;
	tr_state->traversing_mode = TRAVERSE_BINDABLE;

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

#ifndef GPAC_DISABLE_VRML
	bindable = (GF_Node*) gf_list_get(tr_state->backgrounds, 0);
	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
	}
	/*clear if not in layer*/
	else
#endif
		if (!layer_type) {
			SFColor col;
			Fixed alpha = 0;
			col.red = INT2FIX((tr_state->visual->compositor->back_color>>16)&0xFF) / 255;
			col.green = INT2FIX((tr_state->visual->compositor->back_color>>8)&0xFF) / 255;
			col.blue = INT2FIX((tr_state->visual->compositor->back_color)&0xFF) / 255;
			/*if composite visual, clear with alpha = 0*/
			if (tr_state->visual==tr_state->visual->compositor->visual) {
				alpha = FIX_ONE;
				if (tr_state->visual->compositor->init_flags & GF_VOUT_WINDOW_TRANSPARENT) {
					alpha = 0;
				} else if (tr_state->visual->compositor->forced_alpha) {
					alpha = 0;
				}
			}
			visual_3d_clear(tr_state->visual, col, alpha);
		}
	tr_state->traversing_mode = mode;

}

/*in off-axis, projection matrix is not aligned with view axis, however we need to draw the background
centered !!
In this case we draw the background before setting up the projection but after setting up scissor and Viewport*/
static void visual_3d_draw_background_on_axis(GF_TraverseState *tr_state, u32 layer_type)
{
	GF_Matrix proj, model;
	GF_Camera *cam = &tr_state->visual->camera;
	Fixed ar = gf_divfix(cam->width, cam->height);

	tr_state->visual->camlay = 0;
	gf_mx_copy(proj, cam->projection);
	gf_mx_copy(model, cam->modelview);

	gf_mx_perspective(&cam->projection, cam->fieldOfView, ar, cam->z_near, cam->z_far);
	visual_3d_projection_matrix_modified(tr_state->visual);

	/*setup modelview*/
	gf_mx_lookat(&cam->modelview, cam->position, cam->target, cam->up);

	visual_3d_draw_background(tr_state, layer_type);

	tr_state->visual->camlay = GF_3D_CAMERA_OFFAXIS;
	gf_mx_copy(cam->projection, proj);
	gf_mx_copy(cam->modelview, model);


}

void visual_3d_init_draw(GF_TraverseState *tr_state, u32 layer_type)
{
#ifndef GPAC_DISABLE_VRML
	GF_Node *bindable;
#endif
	Bool off_axis_background = (tr_state->camera->is_3D && (tr_state->visual->camlay==GF_3D_CAMERA_OFFAXIS)) ? 1 : 0;

	/*if not in layer, traverse navigation node
	FIXME: we should update the nav info according to the world transform at the current viewpoint (vrml)*/
	tr_state->traversing_mode = TRAVERSE_BINDABLE;
#ifndef GPAC_DISABLE_VRML
	bindable = tr_state->navigations ? (GF_Node*) gf_list_get(tr_state->navigations, 0) : NULL;
	if (Bindable_GetIsBound(bindable)) {
		gf_node_traverse(bindable, tr_state);
		tr_state->camera->had_nav_info = 1;
	} else
#endif
		if (tr_state->camera->had_nav_info) {
			/*if no navigation specified, use default VRML one*/
			tr_state->camera->avatar_size.x = FLT2FIX(0.25f);
			tr_state->camera->avatar_size.y = FLT2FIX(1.6f);
			tr_state->camera->avatar_size.z = FLT2FIX(0.75f);
			tr_state->camera->visibility = 0;
			tr_state->camera->speed = FIX_ONE;
			/*not specified in the spec, but by default we forbid navigation in layer*/
			if (layer_type) {
				tr_state->camera->navigation_flags = NAV_HEADLIGHT;
				tr_state->camera->navigate_mode = GF_NAVIGATE_NONE;
			} else {
				tr_state->camera->navigation_flags = NAV_ANY | NAV_HEADLIGHT;
				if (tr_state->camera->is_3D) {
					if (tr_state->visual->compositor->nav != GF_NAVIGATE_NONE) {
						tr_state->camera->navigate_mode = tr_state->visual->compositor->nav;
					} else {
						/*X3D is by default examine, VRML/MPEG4 is WALK*/
						tr_state->camera->navigate_mode = (tr_state->visual->type_3d==3) ? GF_NAVIGATE_EXAMINE : GF_NAVIGATE_WALK;
					}

#ifdef GF_SR_USE_DEPTH
					/*				if (tr_state->visual->compositor->dispdepth)
										tr_state->camera->navigate_mode = GF_NAVIGATE_NONE;
					*/
#endif
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
	if (camera_animate(tr_state->camera, tr_state->visual->compositor)) {
		if (tr_state->visual->compositor->active_layer) gf_node_dirty_set(tr_state->visual->compositor->active_layer, 0, 1);

		tr_state->visual->compositor->force_next_frame_redraw = GF_TRUE;
	}


	/*turn off depth buffer in 2D*/
	visual_3d_enable_depth_buffer(tr_state->visual, tr_state->camera->is_3D);

	tr_state->camera->proj_vp = tr_state->camera->vp;

	if ((tr_state->visual->autostereo_type==GF_3D_STEREO_SIDE) || (tr_state->visual->autostereo_type==GF_3D_STEREO_HEADSET)) {
		GF_Rect orig_vp = tr_state->camera->vp;
		Fixed vp_width = orig_vp.width;
//		Fixed vp_height = orig_vp.height;
		Fixed old_w = tr_state->camera->width;
		Fixed old_h = tr_state->camera->height;

		//fill up the entire screen matchin AR
		if (tr_state->visual->autostereo_type==GF_3D_STEREO_HEADSET) {
			Fixed max_width = INT2FIX(tr_state->visual->compositor->display_width) / tr_state->visual->nb_views;
			Fixed max_height = INT2FIX(tr_state->visual->compositor->display_height);

#if 0
				Fixed ratio = gf_divfix(vp_width, vp_height);

				if (max_width < gf_mulfix(ratio, max_height) ) {
					tr_state->camera->vp.width = max_width;
				} else {
					tr_state->camera->vp.width = gf_mulfix(ratio, max_height);
				}
				tr_state->camera->vp.height = gf_divfix(tr_state->camera->vp.width, ratio);
#else
				//fill max of screen
				tr_state->camera->vp.width = max_width;
				tr_state->camera->vp.height = max_height;
#endif
				tr_state->camera->width = max_width;
				tr_state->camera->height = max_height;

			tr_state->camera->vp.x = (INT2FIX(tr_state->visual->compositor->display_width) - tr_state->visual->nb_views*tr_state->camera->vp.width)/2 + tr_state->visual->current_view * tr_state->camera->vp.width;
			tr_state->camera->vp.y = (INT2FIX(tr_state->visual->compositor->display_height) - tr_state->camera->vp.height)/2;

		} else {
			tr_state->camera->vp.width = vp_width / tr_state->visual->nb_views;
			tr_state->camera->vp.x += tr_state->visual->current_view * tr_state->camera->vp.width;
		}
		//if first view clear up the original vp
		if (!tr_state->visual->current_view) {
			SFColor col;
			col.red = INT2FIX((tr_state->visual->compositor->back_color>>16)&0xFF) / 255;
			col.green = INT2FIX((tr_state->visual->compositor->back_color>>8)&0xFF) / 255;
			col.blue = INT2FIX((tr_state->visual->compositor->back_color)&0xFF) / 255;
			visual_3d_set_viewport(tr_state->visual, orig_vp);
			visual_3d_clear(tr_state->visual, col, 0);
		}

		visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);
		//we must set scissor in side-by-side to avoid drawing the background outside the viewport!
		visual_3d_set_scissor(tr_state->visual, &tr_state->camera->vp);

		if (off_axis_background) {
			visual_3d_draw_background_on_axis(tr_state, layer_type);
		}

		/*setup projection*/
		visual_3d_setup_projection(tr_state, layer_type);

		tr_state->camera->proj_vp = tr_state->camera->vp;
		tr_state->camera->vp = orig_vp;
		tr_state->camera->width = old_w;
		tr_state->camera->height = old_h;

	} else if (tr_state->visual->autostereo_type==GF_3D_STEREO_TOP) {
		GF_Rect orig_vp;
		orig_vp = tr_state->camera->vp;

		tr_state->camera->vp.height = tr_state->camera->vp.height / tr_state->visual->nb_views;

		if (tr_state->visual == tr_state->visual->compositor->visual) {
			Fixed remain = INT2FIX(tr_state->visual->compositor->output_height - orig_vp.height) / tr_state->visual->nb_views;

			tr_state->camera->vp.y = remain/2 + tr_state->visual->current_view*remain + tr_state->visual->current_view *tr_state->camera->vp.height;
		} else {
			tr_state->camera->vp.y = tr_state->visual->height - tr_state->camera->vp.y - tr_state->camera->vp.height;
		}
		visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);
		//we must set scissor in top-to-bottom to avoid drawing the background outside the viewport!
		visual_3d_set_scissor(tr_state->visual, &tr_state->camera->vp);

		if (off_axis_background) {
			visual_3d_draw_background_on_axis(tr_state, layer_type);
		}

		/*setup projection*/
		visual_3d_setup_projection(tr_state, layer_type);

		tr_state->camera->proj_vp = tr_state->camera->vp;
		tr_state->camera->vp = orig_vp;
	} else {
		visual_3d_set_viewport(tr_state->visual, tr_state->camera->vp);

		if (off_axis_background) {
			visual_3d_draw_background_on_axis(tr_state, layer_type);
		}

		/*setup projection*/
		visual_3d_setup_projection(tr_state, layer_type);

		if (tr_state->visual->autostereo_type) {
			visual_3d_clear_depth(tr_state->visual);
		}
	}

	/*regular background draw*/
	if (!tr_state->camera->is_3D || tr_state->visual->camlay != GF_3D_CAMERA_OFFAXIS) {
		visual_3d_draw_background(tr_state, layer_type);
	}

	/*set headlight if any*/
	if (tr_state->visual->type_3d>1) {
		visual_3d_enable_headlight(tr_state->visual, (tr_state->camera->navigation_flags & NAV_HEADLIGHT) ? 1 : 0, tr_state->camera);
	}

	//reset scissor
	visual_3d_set_scissor(tr_state->visual, NULL);
}


static GFINLINE Bool visual_3d_has_alpha(GF_TraverseState *tr_state, GF_Node *geom)
{
	Drawable3D *stack;

#ifndef GPAC_DISABLE_VRML
	Bool is_mat3D = GF_FALSE;
	if (tr_state->appear) {
		GF_Node *mat = ((M_Appearance *)tr_state->appear)->material;
		if (mat) {
			u32 tag = gf_node_get_tag(mat);
			switch (tag) {
			/*for M2D: if filled & transparent we're transparent - otherwise we must check texture*/
			case TAG_MPEG4_Material2D:
				if (((M_Material2D *)mat)->filled && ((M_Material2D *)mat)->transparency) return 1;
				break;
			case TAG_MPEG4_Material:
#ifndef GPAC_DISABLE_X3D
			case TAG_X3D_Material:
#endif
				is_mat3D = GF_TRUE;
				if ( ((M_Material *)mat)->transparency) return 1;
				break;
			case TAG_MPEG4_MaterialKey:
				return 1;
				break;
			}
		} else if (tr_state->camera->is_3D) {
			GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
			if (txh && txh->transparent) return 1;
		}
	}

	/*check alpha texture in3D or with bitmap*/
	if (tr_state->appear && ( is_mat3D || (gf_node_get_tag(geom)==TAG_MPEG4_Bitmap))) {
		GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
		if (txh && txh->transparent) return 1;
	}

#endif /*GPAC_DISABLE_VRML*/

	/*TODO - FIXME check alpha only...*/
	if (!tr_state->color_mat.identity) return 1;

	stack = (Drawable3D *)gf_node_get_private(geom);
	if (stack && stack->mesh && (stack->mesh->flags & MESH_HAS_ALPHA)) return 1;
	return 0;
}

void visual_3d_register_context(GF_TraverseState *tr_state, GF_Node *geometry)
{
	u32 i, count;
	DirectionalLightContext *ol;
	Drawable3DContext *ctx;
	Drawable3D *drawable;

	gf_assert(tr_state->traversing_mode == TRAVERSE_SORT);

	drawable = (Drawable3D*)gf_node_get_private(geometry);

	/*if 2D draw in declared order. Otherwise, if no alpha or node is a layer, draw directly
	if mesh is not setup yet, consider it as opaque*/
	if (!tr_state->camera->is_3D || !visual_3d_has_alpha(tr_state, geometry) || !drawable->mesh) {
		tr_state->traversing_mode = TRAVERSE_DRAW_3D;
		/*layout/form clipper, set it in world coords only*/
		if (tr_state->has_clip) {
			visual_3d_set_clipper_2d(tr_state->visual, tr_state->clipper, NULL);
		}

		gf_node_traverse(geometry, tr_state);

		/*clear appearance flag once drawn in 3D - not doing so would prevent
		dirty flag propagation in the tree, whcih would typically prevent redrawing
		layers/offscreen groups when texture changes*/
		if (tr_state->appear)
			gf_node_dirty_clear(tr_state->appear, 0);

		/*back to SORT*/
		tr_state->traversing_mode = TRAVERSE_SORT;

		if (tr_state->has_clip) visual_3d_reset_clipper_2d(tr_state->visual);
		return;
	}

	GF_SAFEALLOC(ctx, Drawable3DContext);
	if (!ctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate drawable 3D context\n"));
		return;
	}
	
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
		DirectionalLightContext *nl = (DirectionalLightContext*)gf_malloc(sizeof(DirectionalLightContext));
		memcpy(nl, ol, sizeof(DirectionalLightContext));
		gf_list_add(ctx->directional_lights, nl);
	}
	ctx->clipper = tr_state->clipper;
	ctx->has_clipper = tr_state->has_clip;
	ctx->cull_flag = tr_state->cull_flag;

	if ((ctx->num_clip_planes = tr_state->num_clip_planes))
		memcpy(ctx->clip_planes, tr_state->clip_planes, sizeof(GF_Plane)*MAX_USER_CLIP_PLANES);

	/*get bbox and and insert from further to closest*/
	tr_state->bbox = drawable->mesh->bounds;

	gf_mx_apply_bbox(&ctx->model_matrix, &tr_state->bbox);
	gf_mx_apply_bbox(&tr_state->camera->modelview, &tr_state->bbox);
	ctx->zmax = tr_state->bbox.max_edge.z;
#ifdef GF_SR_USE_DEPTH
	ctx->depth_offset=tr_state->depth_offset;
#endif

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

		/*apply directional lights*/
		tr_state->local_light_on = 1;
		i=0;
		while ((dl = (DirectionalLightContext*)gf_list_enum(ctx->directional_lights, &i))) {
			GF_Matrix mx;
			gf_mx_copy(mx, tr_state->model_matrix);

			gf_mx_add_matrix(&tr_state->model_matrix, &dl->light_matrix);
			gf_node_traverse(dl->dlight, tr_state);

			gf_mx_copy(tr_state->model_matrix, mx);
		}

		/*clipper, set it in world coords only*/
		if (ctx->has_clipper) {
			visual_3d_set_clipper_2d(visual, ctx->clipper, NULL);
		}

		/*clip planes, set it in world coords only*/
		for (i=0; i<ctx->num_clip_planes; i++)
			visual_3d_set_clip_plane(visual, ctx->clip_planes[i], NULL, 0);

		/*restore traversing state*/
		memcpy(&tr_state->model_matrix, &ctx->model_matrix, sizeof(GF_Matrix));
		tr_state->color_mat.identity = ctx->color_mat.identity;
		if (!tr_state->color_mat.identity) memcpy(&tr_state->color_mat, &ctx->color_mat, sizeof(GF_ColorMatrix));
		tr_state->text_split_idx = ctx->text_split_idx;
		tr_state->pixel_metrics = ctx->pixel_metrics;
		/*restore cull flag in case we're completely inside (avoids final frustum/AABB tree culling)*/
		tr_state->cull_flag = ctx->cull_flag;

		tr_state->appear = ctx->appearance;
#ifdef GF_SR_USE_DEPTH
		tr_state->depth_offset = ctx->depth_offset;
#endif
		gf_node_traverse(ctx->geometry, tr_state);

		/*clear appearance flag once drawn in 3D - not doing so would prevent
		dirty flag propagation in the tree, whcih would typically prevent redrawing
		layers/offscreen groups when texture changes*/
		if (tr_state->appear) {
			gf_node_dirty_clear(tr_state->appear, 0);
			tr_state->appear = NULL;
		}

		/*reset directional lights*/
		tr_state->local_light_on = 0;
		for (i=gf_list_count(ctx->directional_lights); i>0; i--) {
			dl = (DirectionalLightContext*)gf_list_get(ctx->directional_lights, i-1);
			gf_node_traverse(dl->dlight, tr_state);
			gf_free(dl);
		}

		if (ctx->has_clipper) visual_3d_reset_clipper_2d(visual);
		for (i=0; i<ctx->num_clip_planes; i++) visual_3d_reset_clip_plane(visual);

		/*and destroy*/
		gf_list_del(ctx->directional_lights);
		gf_free(ctx);
	}
	tr_state->pixel_metrics = pixel_metrics;
	gf_list_reset(tr_state->visual->alpha_nodes_to_draw);
}
static void visual_3d_draw_node(GF_TraverseState *tr_state, GF_Node *root_node)
{
#ifndef GPAC_DISABLE_VRML
	GF_Node *fog;
#endif
	if (!tr_state->camera || !tr_state->visual) return;

	visual_3d_init_draw(tr_state, 0);

	/*main visual, handle collisions*/
	if ((tr_state->visual==tr_state->visual->compositor->visual) && tr_state->camera->is_3D)
		visual_3d_check_collisions(tr_state, root_node, NULL);

#ifndef GPAC_DISABLE_VRML
	/*setup fog*/
	fog = (GF_Node*) gf_list_get(tr_state->visual->fog_stack, 0);
	tr_state->traversing_mode = TRAVERSE_BINDABLE;
	if (Bindable_GetIsBound(fog)) gf_node_traverse(fog, tr_state);
#endif

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

void visual_3d_setup_clipper(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	GF_Rect rc;
	/*setup top clipper*/
	if (visual->center_coords) {
		rc = gf_rect_center(INT2FIX(visual->width), INT2FIX(visual->height));
	} else {
		rc.width = INT2FIX(visual->width);
		rc.height = INT2FIX(visual->height);
		rc.x = 0;
		rc.y = rc.height;
		if (visual->compositor->visual==visual) {
			rc.x += INT2FIX(visual->compositor->vp_x);
			rc.y += INT2FIX(visual->compositor->vp_y);
		}
	}

	/*setup viewport*/
#ifndef GPAC_DISABLE_VRML
	if (gf_list_count(visual->view_stack)) {
		tr_state->traversing_mode = TRAVERSE_BINDABLE;
		tr_state->bounds = rc;
		gf_node_traverse((GF_Node *) gf_list_get(visual->view_stack, 0), tr_state);
	}
#endif

	visual->top_clipper = gf_rect_pixelize(&rc);
	tr_state->clipper = rc;
	gf_mx_init(tr_state->layer_matrix);
}

Bool visual_3d_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
#ifndef GPAC_DISABLE_LOG
	u32 time = gf_sys_clock();
#endif

	if (visual->compositor->fbo_id)
		compositor_3d_enable_fbo(visual->compositor, GF_TRUE);

	visual_3d_setup(visual);
	visual->active_glsl_flags = 0;

	/*setup our traversing state*/
	visual_3d_setup_traversing_state(visual, tr_state);

	if (is_root_visual) {
		Bool auto_stereo = 0;

		visual_3d_setup_clipper(visual, tr_state);

		if (tr_state->visual->autostereo_type > GF_3D_STEREO_LAST_SINGLE_BUFFER) {
			visual_3d_init_autostereo(visual);
			auto_stereo = 1;
		}

#ifndef GPAC_USE_GLES1X
		visual_3d_init_shaders(visual);
#endif

		for (visual->current_view=0; visual->current_view < visual->nb_views; visual->current_view++) {
			GF_SceneGraph *sg;
			u32 i;
			visual_3d_draw_node(tr_state, root);

			/*extra scene graphs*/
			i=0;
			while ((sg = (GF_SceneGraph*)gf_list_enum(visual->compositor->extra_scenes, &i))) {
				tr_state->traversing_mode = TRAVERSE_SORT;
				gf_sc_traverse_subscene(visual->compositor, root, sg, tr_state);
			}

			if (auto_stereo) {
				visual_3d_end_auto_stereo_pass(visual);
			}

			visual->compositor->reset_graphics = 0;
		}

	} else {
		visual_3d_draw_node(tr_state, root);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] Frame\t%d\t3D drawn in \t%d\tms\n", visual->compositor->frame_number, gf_sys_clock() - time));

	if (visual->compositor->fbo_id)
		compositor_3d_enable_fbo(visual->compositor, GF_FALSE);

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

void visual_3d_check_collisions(GF_TraverseState *tr_state, GF_Node *on_node, GF_ChildNodeItem *node_list)
{
	SFVec3f n, dir;
	Bool go;
	Fixed diff, pos_diff;

	if (!tr_state->visual || !tr_state->camera) return;
	/*don't collide on VP animations or when modes discard collision*/
	if ((tr_state->camera->anim_len && !tr_state->camera->jumping) || !tr_state->visual->compositor->collide_mode || (tr_state->camera->navigate_mode>=GF_NAVIGATE_EXAMINE)) {
		/*reset ground flag*/
		tr_state->camera->last_had_ground = 0;
		/*and avoid resetting move at next collision change*/
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
		if (on_node) {
			gf_node_traverse(on_node, tr_state);
		} else if (node_list) {
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Obstacle detected - too high (dist %g)\n", FIX2FLT(diff)));
			tr_state->camera->position = tr_state->camera->last_pos;
			tr_state->camera->flags |= CAM_IS_DIRTY;
		} else {
			if ((tr_state->camera->jumping && fabs(diff)>tr_state->camera->dheight)
			        || (!tr_state->camera->jumping && (ABS(diff)>FIX_ONE/1000) )) {
				tr_state->camera->last_had_ground = 1;
				n = gf_vec_scale(tr_state->camera->up, -diff);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Ground detected camera position: %g %g %g - offset: %g %g %g (dist %g)\n",
				                                      FIX2FLT(tr_state->camera->position.x), FIX2FLT(tr_state->camera->position.y), FIX2FLT(tr_state->camera->position.z),
				                                      FIX2FLT(n.x), FIX2FLT(n.y), FIX2FLT(n.z), FIX2FLT(diff)));

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
				if (tr_state->camera->collide_dist>=tr_state->camera->avatar_size.x) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Collision] Collision distance %g greater than avatar collide size %g\n", FIX2FLT(tr_state->camera->collide_dist), FIX2FLT(tr_state->camera->avatar_size.x)));

					/*safety check due to precision, always stay below collide dist*/
					tr_state->camera->collide_dist = tr_state->camera->avatar_size.x;
				}
				
				gf_vec_diff(n, tr_state->camera->position, tr_state->camera->collide_point);
				gf_vec_norm(&n);
				n = gf_vec_scale(n, tr_state->camera->avatar_size.x - tr_state->camera->collide_dist);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] offseting camera: position: %g %g %g - offset: %g %g %g\n",
				                                      FIX2FLT(tr_state->camera->position.x), FIX2FLT(tr_state->camera->position.y), FIX2FLT(tr_state->camera->position.z),
				                                      FIX2FLT(n.x), FIX2FLT(n.y), FIX2FLT(n.z)));

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

	if (tr_state->camera->flags & CAM_IS_DIRTY) visual_3d_setup_projection(tr_state, 0);
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
#ifdef DISABLE_VIEW_CULL
	tr_state->cull_flag = CULL_INSIDE;
	return 1;
#else
	GF_BBox b;
	Fixed irad, rad;
	GF_Camera *cam;
	Bool do_sphere;
	u32 i, p_idx;
	SFVec3f cdiff, vertices[8];

	if (!tr_state->camera || (tr_state->cull_flag == CULL_INSIDE)) return 1;
	gf_assert(tr_state->cull_flag != CULL_OUTSIDE);

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
	if (tr_state->camera->is_3D) {
		gf_vec_diff(cdiff, cam->center, b.center);
		rad = b.radius + cam->radius;
		if (gf_vec_len(cdiff) > rad) {
			tr_state->cull_flag = CULL_OUTSIDE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Culling] Node out (sphere-sphere test)\n"));
			return 0;
		}
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
#endif
}

Bool visual_3d_setup_ray(GF_VisualManager *visual, GF_TraverseState *tr_state, s32 ix, s32 iy)
{
	Fixed in_x, in_y, x, y;
	SFVec3f start, end;
	SFVec4f res;

	x = INT2FIX(ix);
	y = INT2FIX(iy);

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

#if 0
	start.z = visual->camera.z_near;
	end.z = visual->camera.z_far;
	if (!tr_state->camera->is_3D && !tr_state->pixel_metrics) {
		start.x = end.x = gf_divfix(x, tr_state->min_hsize);
		start.y = end.y = gf_divfix(y, tr_state->min_hsize);
	} else {
		start.x = end.x = x;
		start.y = end.y = y;
	}
#endif

	/*unproject to world coords*/
	in_x = 2*x/ (s32) visual->width;
	in_y = 2*y/ (s32) visual->height;

	res.x = in_x;
	res.y = in_y;
	res.z = -FIX_ONE/2;
	res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&visual->camera.unprojection, &res);
	if (!res.q) return GF_FALSE;
	start.x = gf_divfix(res.x, res.q);
	start.y = gf_divfix(res.y, res.q);
	start.z = gf_divfix(res.z, res.q);

	res.x = in_x;
	res.y = in_y;
	res.z = FIX_ONE/2;
	res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&visual->camera.unprojection, &res);
	if (!res.q) return GF_FALSE;
	end.x = gf_divfix(res.x, res.q);
	end.y = gf_divfix(res.y, res.q);
	end.z = gf_divfix(res.z, res.q);

	tr_state->ray = gf_ray(start, end);
	/*also update hit info world ray in case we have a grabbed sensor with mouse off*/
	visual->compositor->hit_world_ray = tr_state->ray;

	return GF_TRUE;
}

void visual_3d_pick_node(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{

	visual_3d_setup_traversing_state(visual, tr_state);
	visual_3d_setup_projection(tr_state, 0);


	if (!visual_3d_setup_ray(visual, tr_state, ev->mouse.x, ev->mouse.y))
		return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] cast ray Origin %.4f %.4f %.4f Direction %.4f %.4f %.4f\n",
	                                      FIX2FLT(tr_state->ray.orig.x), FIX2FLT(tr_state->ray.orig.y), FIX2FLT(tr_state->ray.orig.z),
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


#ifndef GPAC_DISABLE_VRML
void visual_3d_vrml_drawable_pick(GF_Node *n, GF_TraverseState *tr_state, GF_Mesh *mesh, Drawable *drawable)
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

	if (!mesh && !drawable) return;

	count = gf_list_count(tr_state->vrml_sensors);
	compositor = tr_state->visual->compositor;

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
			gf_bbox_from_rect(&box, &drawable->path->bbox);

		if (gf_bbox_plane_relation(&box, &p) == GF_BBOX_FRONT) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] bounding box of node %s (DEF %s) below current hit point - skipping\n", gf_node_get_class_name(n), gf_node_get_name(n)));
			return;
		}
	}
	if (drawable) {
		DrawAspect2D asp;
		node_is_over = 0;
		if (compositor_get_2d_plane_intersection(&r, &local_pt)) {
			if (gf_path_point_over(drawable->path, local_pt.x, local_pt.y)) {
				hit_normal.x = hit_normal.y = 0;
				hit_normal.z = FIX_ONE;
				text_coords.x = gf_divfix(local_pt.x, drawable->path->bbox.width) + FIX_ONE/2;
				text_coords.y = gf_divfix(local_pt.y, drawable->path->bbox.height) + FIX_ONE/2;
				node_is_over = 1;
			}
			memset(&asp, 0, sizeof(DrawAspect2D));
			drawable_get_aspect_2d_mpeg4(drawable->node, &asp, tr_state);
			if (asp.pen_props.width || asp.line_texture ) {
				StrikeInfo2D *si = drawable_get_strikeinfo(tr_state->visual->compositor, drawable, &asp, tr_state->appear, NULL, 0, NULL);
				if (si && si->outline && gf_path_point_over(si->outline, local_pt.x, local_pt.y)) {
					hit_normal.x = hit_normal.y = 0;
					hit_normal.z = FIX_ONE;
					text_coords.x = gf_divfix(local_pt.x, si->outline->bbox.width) + FIX_ONE/2;
					text_coords.y = gf_divfix(local_pt.y, si->outline->bbox.height) + FIX_ONE/2;
					node_is_over = 1;
				}
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
	compositor->hit_use_dom_events = 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Picking] node %s (def %s) is under mouse - hit %g %g %g\n", gf_node_get_class_name(n), gf_node_get_name(n),
	                                      FIX2FLT(world_pt.x), FIX2FLT(world_pt.y), FIX2FLT(world_pt.z)));
}


void visual_3d_vrml_drawable_collide(GF_Node *node, GF_TraverseState *tr_state)
{
	SFVec3f pos, v1, v2, collide_pt, last_pos;
	Fixed dist, m_dist;
	GF_Matrix mx;
	u32 cull_backup;
	Drawable3D *st = (Drawable3D *)gf_node_get_private(node);
	if (!st || !st->mesh) return;

	/*no collision with lines & points*/
	if (st->mesh->mesh_type != MESH_TRIANGLES) return;

#ifndef GPAC_DISABLE_VRML
	/*no collision with text (vrml)*/
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Text:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Text:
#endif
		return;
	}
#endif

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
			if (gf_log_tool_level_on(GF_LOG_COMPOSE, GF_LOG_DEBUG)) {
				gf_vec_diff(v1, pos, collide_pt);
				gf_vec_norm(&v1);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] found at %g %g %g (WC) - dist (%g) - local normal %g %g %g\n",
				                                      FIX2FLT(tr_state->camera->collide_point.x), FIX2FLT(tr_state->camera->collide_point.y), FIX2FLT(tr_state->camera->collide_point.z),
				                                      FIX2FLT(dist),
				                                      FIX2FLT(v1.x), FIX2FLT(v1.y), FIX2FLT(v1.z)));
			}
#endif
		}
		else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Existing collision (dist %g) closer than current collsion (dist %g)\n", FIX2FLT(tr_state->camera->collide_dist), FIX2FLT(dist) ));
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
				                                      FIX2FLT(tr_state->camera->ground_point.x), FIX2FLT(tr_state->camera->ground_point.y), FIX2FLT(tr_state->camera->ground_point.z),
				                                      FIX2FLT(dist),
				                                      FIX2FLT(v1.x), FIX2FLT(v1.y), FIX2FLT(v1.z)));
			}
			else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Collision] Existing ground (dist %g) closer than current (dist %g)\n", FIX2FLT(tr_state->camera->ground_dist), FIX2FLT(dist)));
			}
		}
	}
}

#endif /*GPAC_DISABLE_VRML*/


static GF_TextureHandler *visual_3d_setup_texture_2d(GF_TraverseState *tr_state, DrawAspect2D *asp, GF_Mesh *mesh)
{
	if (!asp->fill_texture) return NULL;

	if (asp->fill_color && (GF_COL_A(asp->fill_color) != 0xFF)) {
		visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
		gf_sc_texture_set_blend_mode(asp->fill_texture, TX_MODULATE);
	} else {
		visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);
		gf_sc_texture_set_blend_mode(asp->fill_texture, TX_REPLACE);
	}

	if (asp->fill_texture->flags & GF_SR_TEXTURE_SVG) {
		GF_Rect rc;
		gf_rect_from_bbox(&rc, &mesh->bounds);
		tr_state->mesh_num_textures = gf_sc_texture_enable_ex(asp->fill_texture, NULL, &rc);
	} else {
#ifndef GPAC_DISABLE_VRML
		tr_state->mesh_num_textures = gf_sc_texture_enable(asp->fill_texture, tr_state->appear ? ((M_Appearance *)tr_state->appear)->textureTransform : NULL);
#endif
	}
	if (tr_state->mesh_num_textures) return asp->fill_texture;
	return NULL;
}

void visual_3d_set_2d_strike(GF_TraverseState *tr_state, DrawAspect2D *asp)
{
	if (asp->line_texture) {
		GF_Node *txtrans = NULL;
#ifndef GPAC_DISABLE_VRML
		if (tr_state->appear
		        && (gf_node_get_tag( ((M_Appearance *)tr_state->appear)->material) == TAG_MPEG4_Material2D)
		        && (gf_node_get_tag(((M_Material2D *) ((M_Appearance *)tr_state->appear)->material)->lineProps) == TAG_MPEG4_XLineProperties)
		   ) {
			txtrans = ((M_XLineProperties *) ((M_Material2D *) ((M_Appearance *)tr_state->appear)->material)->lineProps)->textureTransform;
		}
#endif

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
		tr_state->mesh_num_textures = gf_sc_texture_enable(asp->line_texture, txtrans);
		if (tr_state->mesh_num_textures) return;
	}
	/*no texture or not ready, use color*/
	if (asp->line_color)
		visual_3d_set_material_2d_argb(tr_state->visual, asp->line_color);
}


void visual_3d_draw_2d_with_aspect(Drawable *st, GF_TraverseState *tr_state, DrawAspect2D *asp)
{
	StrikeInfo2D *si;
	GF_TextureHandler *fill_txh;

	fill_txh = visual_3d_setup_texture_2d(tr_state, asp, st->mesh);

	/*fill path*/
	if (fill_txh || (GF_COL_A(asp->fill_color)) ) {
		if (!st->mesh) return;

		if (asp->fill_color)
			visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
		else if (GF_COL_A(asp->line_color) && !(asp->line_color & 0x00FFFFFF)) {
			u32 col = asp->line_color | 0x00FFFFFF;
			visual_3d_set_material_2d_argb(tr_state->visual, col);
		}

		visual_3d_mesh_paint(tr_state, st->mesh);
		/*reset texturing in case of line texture*/
		if (tr_state->mesh_num_textures) {
			gf_sc_texture_disable(fill_txh);
			tr_state->mesh_num_textures = 0;
		}
	}

	if ((tr_state->visual->type_3d == 4) && !asp->line_texture) return;

	/*strike path*/
	if (!asp->pen_props.width || !GF_COL_A(asp->line_color)) return;

	si = drawable_get_strikeinfo(tr_state->visual->compositor, st, asp, tr_state->appear, NULL, 0, tr_state);
	if (!si) return;

	if (!si->mesh_outline) {
		si->is_vectorial = asp->line_texture ? GF_TRUE : !tr_state->visual->compositor->linegl;
		si->mesh_outline = new_mesh();
#ifdef GPAC_HAS_GLU
		if (si->is_vectorial) {
			gf_mesh_tesselate_path(si->mesh_outline, si->outline, asp->line_texture ? 2 : 1);
		} else
#endif
			mesh_get_outline(si->mesh_outline, st->path);
	}

	visual_3d_set_2d_strike(tr_state, asp);
	if (asp->line_texture) tr_state->mesh_num_textures = 1;

	if (!si->is_vectorial) {
		visual_3d_mesh_strike(tr_state, si->mesh_outline, asp->pen_props.width, asp->line_scale, asp->pen_props.dash);
	} else {
		visual_3d_mesh_paint(tr_state, si->mesh_outline);
	}
	if (asp->line_texture) {
		gf_sc_texture_disable(asp->line_texture);
		tr_state->mesh_num_textures = 0;
	}
}

#ifndef GPAC_DISABLE_VRML
void visual_3d_draw_2d(Drawable *st, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;
	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(st->node, &asp, tr_state);
	visual_3d_draw_2d_with_aspect(st, tr_state, &asp);
}
#endif


void visual_3d_draw_from_context(DrawableContext *ctx, GF_TraverseState *tr_state)
{
	GF_Rect rc;
	gf_path_get_bounds(ctx->drawable->path, &rc);
	visual_3d_draw_2d_with_aspect(ctx->drawable, tr_state, &ctx->aspect);

	drawable_check_focus_highlight(ctx->drawable->node, tr_state, &rc);
}


static GFINLINE Bool visual_3d_setup_material(GF_TraverseState *tr_state, u32 mesh_type, Fixed *diffuse_alpha)
{
#ifndef GPAC_DISABLE_VRML
	GF_Node *__mat;
#endif
	SFColor def;
	def.red = def.green = def.blue = FIX_ONE;
	/*store diffuse alpha*/
	if (diffuse_alpha) *diffuse_alpha = FIX_ONE;

	if (!tr_state->appear) {
		/*use material2D to disable lighting*/
		visual_3d_set_material_2d(tr_state->visual, def, FIX_ONE);
		return 1;
	}

#ifndef GPAC_DISABLE_VRML
#ifndef GPAC_DISABLE_X3D
	if (gf_node_get_tag(tr_state->appear)==TAG_X3D_Appearance) {
		X_FillProperties *fp = (X_FillProperties *) ((X_Appearance*)tr_state->appear)->fillProperties;
		if (fp && !fp->filled) return 0;
	}
#endif

	__mat = ((M_Appearance *)tr_state->appear)->material;
	if (!__mat) {
		/*use material2D to disable lighting (cf VRML specs)*/
		visual_3d_set_material_2d(tr_state->visual, def, FIX_ONE);
		return 1;
	}

	switch (gf_node_get_tag((GF_Node *)__mat)) {
	case TAG_MPEG4_Material:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Material:
#endif
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
		if (diffuse_alpha) *diffuse_alpha = diff_a;
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
			if (mat->transparency) {
				emi.red = emi.green = emi.blue = FIX_ONE;
			} else {
				GF_TextureHandler *txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
				if (txh) {
					gf_sc_texture_set_blend_mode(txh, TX_REPLACE);
					visual_3d_set_state(tr_state->visual, V3D_STATE_COLOR, 0);
					visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT, 1);
					return 1;
				}
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
#else
	return 0;
#endif	/*GPAC_DISABLE_VRML*/
}

Bool visual_3d_setup_texture(GF_TraverseState *tr_state, Fixed diffuse_alpha)
{
#ifndef GPAC_DISABLE_VRML
	GF_TextureHandler *txh;
	tr_state->mesh_num_textures = 0;
	if (!tr_state->appear) return GF_TRUE;

	gf_node_dirty_reset(tr_state->appear, 0);

	txh = gf_sc_texture_get_handler(((M_Appearance *)tr_state->appear)->texture);
	//no texture, return TRUE (eg draw)
	if (!txh)
		return GF_TRUE;

	gf_sc_texture_set_blend_mode(txh, gf_sc_texture_is_transparent(txh) ? TX_MODULATE : TX_REPLACE);
	tr_state->mesh_num_textures = gf_sc_texture_enable(txh, ((M_Appearance *)tr_state->appear)->textureTransform);
	if (tr_state->mesh_num_textures) {
		Fixed v[4];
		switch (txh->pixelformat) {
		/*override diffuse color with full intensity, but keep material alpha (cf VRML lighting)*/
		case GF_PIXEL_RGB:
			if (tr_state->visual->has_material_2d) {
				SFColor c;
				c.red = c.green = c.blue = FIX_ONE;
				visual_3d_set_material_2d(tr_state->visual, c, diffuse_alpha);
			} else {
				v[0] = v[1] = v[2] = FIX_ONE;
				v[3] = diffuse_alpha;
				visual_3d_set_material(tr_state->visual, V3D_MATERIAL_DIFFUSE, v);
			}
			break;
		/*override diffuse color AND material alpha (cf VRML lighting)*/
		case GF_PIXEL_RGBA:
			if (!tr_state->visual->has_material_2d) {
				v[0] = v[1] = v[2] = v[3] = FIX_ONE;
				visual_3d_set_material(tr_state->visual, V3D_MATERIAL_DIFFUSE, v);
			}
			tr_state->mesh_is_transparent = 1;
			break;
		}
	}
	return tr_state->mesh_num_textures ? GF_TRUE : GF_FALSE;
#else
	return GF_TRUE;
#endif /*GPAC_DISABLE_VRML*/
}

void visual_3d_disable_texture(GF_TraverseState *tr_state)
{
	if (tr_state->mesh_num_textures) {
#ifndef GPAC_DISABLE_VRML
		gf_sc_texture_disable(gf_sc_texture_get_handler( ((M_Appearance *)tr_state->appear)->texture) );
#endif
		tr_state->mesh_num_textures = 0;
	}
}

Bool visual_3d_setup_appearance(GF_TraverseState *tr_state)
{
	Fixed diff_a;
	/*setup material and check if 100% transparent - in which case don't draw*/
	if (!visual_3d_setup_material(tr_state, 0, &diff_a)) return 0;
	/*setup texture*/
	if (! visual_3d_setup_texture(tr_state, diff_a)) return 0;
	return 1;
}


void visual_3d_draw(GF_TraverseState *tr_state, GF_Mesh *mesh)
{
	if (mesh && tr_state->visual->compositor->clipframe) {
		void gf_mx_apply_bbox_4x4(GF_Matrix *mx, GF_BBox *box);
		GF_Matrix mx;
		GF_BBox bounds = mesh->bounds;
		gf_mx_copy(mx, tr_state->camera->modelview);
		gf_mx_add_matrix(&mx, &tr_state->model_matrix);

		gf_mx_apply_bbox(&mx, &bounds);
		//get persp-transformed bounds, all coords will be in NDC [-1,1]
		gf_mx_apply_bbox_4x4(&tr_state->camera->projection, &bounds);

		GF_Rect rc;
		rc.x = gf_mulfix(bounds.min_edge.x/2, tr_state->camera->vp.width);
		rc.y = gf_mulfix(bounds.max_edge.y/2, tr_state->camera->vp.height);
		rc.width = gf_mulfix(bounds.max_edge.x/2, tr_state->camera->vp.width) - rc.x;
		rc.height = rc.y - gf_mulfix(bounds.min_edge.y/2, tr_state->camera->vp.height);
		//no need to apply vp x/y offset, we are already centered
		GF_IRect irc = gf_rect_pixelize(&rc);
		if (!tr_state->visual->surf_rect.width) {
			rc = gf_rect_center(INT2FIX(tr_state->visual->compositor->display_width), INT2FIX((tr_state->visual->compositor->display_height)));
			tr_state->visual->surf_rect = gf_rect_pixelize(&rc);
		}
		gf_irect_intersect(&irc, &tr_state->visual->surf_rect);
		gf_irect_union(&tr_state->visual->frame_bounds, &irc);
	}
	if (mesh->mesh_type) {
		if (visual_3d_setup_material(tr_state, mesh->mesh_type, NULL)) {
			visual_3d_mesh_paint(tr_state, mesh);
		}
	} else if (visual_3d_setup_appearance(tr_state)) {
		visual_3d_mesh_paint(tr_state, mesh);
		visual_3d_disable_texture(tr_state);

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_DISABLE_X3D) && !defined(GPAC_USE_GLES2)
		if (tr_state->appear && gf_node_get_tag(tr_state->appear)==TAG_X3D_Appearance) {
			X_Appearance *ap = (X_Appearance *)tr_state->appear;
			X_FillProperties *fp = ap->fillProperties ? (X_FillProperties *) ap->fillProperties : NULL;
			if (fp && fp->hatched) visual_3d_mesh_hatch(tr_state, mesh, fp->hatchStyle, fp->hatchColor);
		}
#endif
	}
}

void visual_3d_projection_matrix_modified(GF_VisualManager *visual)
{
	visual->needs_projection_matrix_reload = 1;
}

void visual_3d_enable_headlight(GF_VisualManager *visual, Bool bOn, GF_Camera *cam)
{
	SFVec3f dir;
	SFColor col;

	if (!bOn) return;
	//if we have lights in the scene don't turn the headlight on
	if (visual->has_inactive_lights || visual->num_lights) return;

	col.blue = col.red = col.green = FIX_ONE;
	dir.x = dir.y = 0;
	dir.z = -FIX_ONE;
//	if (cam->is_3D) dir = camera_get_target_dir(cam);
//	visual_3d_add_directional_light(visual, 0, col, FIX_ONE, dir, &cam->modelview);

	visual_3d_add_directional_light(visual, 0, col, FIX_ONE, dir, NULL);
}

void visual_3d_set_material_2d(GF_VisualManager *visual, SFColor col, Fixed alpha)
{
	visual->has_material_2d = alpha ? GF_TRUE : GF_FALSE;
	visual->has_material = 0;
	if (visual->has_material_2d) {
		visual->mat_2d.red = col.red;
		visual->mat_2d.green = col.green;
		visual->mat_2d.blue = col.blue;
		visual->mat_2d.alpha = alpha;
	}

}

void visual_3d_set_material_2d_argb(GF_VisualManager *visual, u32 col)
{
	u32 a = GF_COL_A(col);
	visual->has_material_2d = a ? GF_TRUE : GF_FALSE;
	visual->has_material = 0;
	if (visual->has_material_2d) {
		visual->mat_2d.red = INT2FIX( GF_COL_R(col) ) / 255;
		visual->mat_2d.green = INT2FIX( GF_COL_G(col) ) / 255;
		visual->mat_2d.blue = INT2FIX( GF_COL_B(col) ) / 255;
		visual->mat_2d.alpha = INT2FIX( a ) / 255;
	}
}

void visual_3d_set_clipper_2d(GF_VisualManager *visual, GF_Rect clip, GF_Matrix *mx_at_clipper)
{
	if (mx_at_clipper)
		gf_mx_apply_rect(mx_at_clipper, &clip);
	visual->clipper_2d = gf_rect_pixelize(&clip);
	visual->has_clipper_2d = GF_TRUE;
}

void visual_3d_reset_clipper_2d(GF_VisualManager *visual)
{
	visual->has_clipper_2d = GF_FALSE;
}

void visual_3d_set_clip_plane(GF_VisualManager *visual, GF_Plane p, GF_Matrix *mx_at_clipper, Bool is_2d_clip)
{
	if (visual->num_clips==GF_MAX_GL_CLIPS) return;
	gf_vec_norm(&p.normal);
	visual->clippers[visual->num_clips].p = p;
	visual->clippers[visual->num_clips].is_2d_clip = is_2d_clip;
	visual->clippers[visual->num_clips].mx_clipper = mx_at_clipper;
	visual->num_clips++;
}

void visual_3d_reset_clip_plane(GF_VisualManager *visual)
{
	if (!visual->num_clips) return;
	visual->num_clips -= 1;
}

void visual_3d_set_material(GF_VisualManager *visual, u32 material_type, Fixed *rgba)
{
	visual->materials[material_type].red = rgba[0];
	visual->materials[material_type].green = rgba[1];
	visual->materials[material_type].blue = rgba[2];
	visual->materials[material_type].alpha = rgba[3];

	visual->has_material = 1;
	visual->has_material_2d=0;
}

void visual_3d_set_shininess(GF_VisualManager *visual, Fixed shininess)
{
	visual->shininess = shininess;
}

void visual_3d_set_state(GF_VisualManager *visual, u32 flag_mask, Bool setOn)
{
	if (flag_mask & V3D_STATE_LIGHT) visual->state_light_on = setOn;
	if (flag_mask & V3D_STATE_BLEND) visual->state_blend_on = setOn;
	if (flag_mask & V3D_STATE_COLOR) visual->state_color_on = setOn;
}


void visual_3d_set_texture_matrix(GF_VisualManager *visual, GF_Matrix *mx)
{
	visual->has_tx_matrix = mx ? 1 : 0;
	if (mx) gf_mx_copy(visual->tx_matrix, *mx);
}


void visual_3d_has_inactive_light(GF_VisualManager *visual)
{
	visual->has_inactive_lights = GF_TRUE;
}

Bool visual_3d_add_point_light(GF_VisualManager *visual, Fixed ambientIntensity, SFVec3f attenuation, SFColor color, Fixed intensity, SFVec3f location, GF_Matrix *light_mx)
{
	if (visual->num_lights==visual->max_lights) return 0;
	visual->lights[visual->num_lights].type = 2;
	visual->lights[visual->num_lights].ambientIntensity = ambientIntensity;
	visual->lights[visual->num_lights].attenuation = attenuation;
	visual->lights[visual->num_lights].color = color;
	visual->lights[visual->num_lights].intensity = intensity;
	visual->lights[visual->num_lights].position = location;
	memcpy(&visual->lights[visual->num_lights].light_mx, light_mx, sizeof(GF_Matrix) );
	visual->num_lights++;
	return 1;
}

Bool visual_3d_add_spot_light(GF_VisualManager *visual, Fixed ambientIntensity, SFVec3f attenuation, Fixed beamWidth,
                              SFColor color, Fixed cutOffAngle, SFVec3f direction, Fixed intensity, SFVec3f location, GF_Matrix *light_mx)
{
	if (visual->num_lights==visual->max_lights) return 0;
	visual->lights[visual->num_lights].type = 1;
	visual->lights[visual->num_lights].ambientIntensity = ambientIntensity;
	visual->lights[visual->num_lights].attenuation = attenuation;
	visual->lights[visual->num_lights].beamWidth = beamWidth;
	visual->lights[visual->num_lights].cutOffAngle = cutOffAngle;
	visual->lights[visual->num_lights].color = color;
	visual->lights[visual->num_lights].direction = direction;
	visual->lights[visual->num_lights].intensity = intensity;
	visual->lights[visual->num_lights].position = location;
	memcpy(&visual->lights[visual->num_lights].light_mx, light_mx, sizeof(GF_Matrix) );
	visual->num_lights++;
	return 1;
}

Bool visual_3d_add_directional_light(GF_VisualManager *visual, Fixed ambientIntensity, SFColor color, Fixed intensity, SFVec3f direction, GF_Matrix *light_mx)
{
	if (visual->num_lights==visual->max_lights) return 0;
	visual->lights[visual->num_lights].type = 0;
	visual->lights[visual->num_lights].ambientIntensity = ambientIntensity;
	visual->lights[visual->num_lights].color = color;
	visual->lights[visual->num_lights].intensity = intensity;
	visual->lights[visual->num_lights].direction = direction;
	if (light_mx) {
		memcpy(&visual->lights[visual->num_lights].light_mx, light_mx, sizeof(GF_Matrix) );
	} else {
		gf_mx_init(visual->lights[visual->num_lights].light_mx);
		visual->lights[visual->num_lights].type = 3;
		visual->lights[visual->num_lights].direction.x = 0;
		visual->lights[visual->num_lights].direction.y = 0;
		visual->lights[visual->num_lights].direction.z = -FIX_ONE;
	}

	visual->num_lights++;
	return 1;
}


void visual_3d_remove_last_light(GF_VisualManager *visual)
{
	if (visual->num_lights) {
		visual->num_lights--;
	}
}

void visual_3d_clear_all_lights(GF_VisualManager *visual)
{
	visual->num_lights = 0;
	visual->has_inactive_lights = GF_FALSE;
}

void visual_3d_set_fog(GF_VisualManager *visual, const char *type, SFColor color, Fixed density, Fixed visibility)
{
	visual->has_fog = GF_TRUE;
	if (!type || !stricmp(type, "LINEAR")) visual->fog_type = 0;
	else if (!stricmp(type, "EXPONENTIAL")) visual->fog_type = 1;
	else if (!stricmp(type, "EXPONENTIAL2")) visual->fog_type = 2;

	visual->fog_color = color;
	visual->fog_density = density;
	visual->fog_visibility = visibility;
}

#endif //!defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_COMPOSITOR)
