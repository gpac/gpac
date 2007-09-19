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

#include "node_stacks.h"
#include "visual_manager.h"
#include <gpac/options.h>


#ifndef GPAC_DISABLE_3D

static void camera_changed(Render *sr, GF_Camera *cam)
{
	cam->flags |= CAM_IS_DIRTY;
	gf_sr_invalidate(sr->compositor, NULL);
}

/*shortcut*/
static void gf_mx_rotation_matrix(GF_Matrix *mx, SFVec3f axis_pt, SFVec3f axis, Fixed angle)
{
	gf_mx_init(*mx);
	gf_mx_add_translation(mx, axis_pt.x, axis_pt.y, axis_pt.z);
	gf_mx_add_rotation(mx, angle, axis.x, axis.y, axis.z);
	gf_mx_add_translation(mx, -axis_pt.x, -axis_pt.y, -axis_pt.z);
}
	
static void view_orbit_x(Render *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->target, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	camera_changed(sr, cam);
}
static void view_orbit_y(Render *sr, GF_Camera *cam, Fixed dy)
{
	GF_Matrix mx;
	SFVec3f axis;
	if (!dy) return;
	axis = camera_get_right_dir(cam);
	gf_mx_rotation_matrix(&mx, cam->target, axis, dy);
	gf_mx_apply_vec(&mx, &cam->position);
	/*update up vector*/
	cam->up = gf_vec_cross(camera_get_pos_dir(cam), axis);
	gf_vec_norm(&cam->up);
	camera_changed(sr, cam);
}

static void view_exam_x(Render *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->examine_center, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	gf_mx_apply_vec(&mx, &cam->target);
	camera_changed(sr, cam);
}

static void view_exam_y(Render *sr, GF_Camera *cam, Fixed dy)
{
	GF_Matrix mx;
	SFVec3f axis;
	if (!dy) return;
	axis = camera_get_right_dir(cam);
	gf_mx_rotation_matrix(&mx, cam->examine_center, axis, dy);
	gf_mx_apply_vec(&mx, &cam->position);
	gf_mx_apply_vec(&mx, &cam->target);
	/*update up vector*/
	cam->up = gf_vec_cross(camera_get_pos_dir(cam), axis);
	gf_vec_norm(&cam->up);
	camera_changed(sr, cam);
}

static void view_roll(Render *sr, GF_Camera *cam, Fixed dd)
{
	GF_Matrix mx;
	SFVec3f delta;
	if (!dd) return;
	gf_vec_add(delta, cam->target, cam->up);
	gf_mx_rotation_matrix(&mx, cam->target, camera_get_pos_dir(cam), dd);
	gf_mx_apply_vec(&mx, &delta);
	gf_vec_diff(cam->up, delta, cam->target);
	gf_vec_norm(&cam->up);
	camera_changed(sr, cam);
}

static void view_pan_x(Render *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->position, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->target);
	camera_changed(sr, cam);
}
static void view_pan_y(Render *sr, GF_Camera *cam, Fixed dy)
{
	GF_Matrix mx;
	SFVec3f axis;
	if (!dy) return;
	axis = camera_get_right_dir(cam);
	gf_mx_rotation_matrix(&mx, cam->position, axis, dy);
	gf_mx_apply_vec(&mx, &cam->target);
	/*update up vector*/
	cam->up = gf_vec_cross(camera_get_pos_dir(cam), axis);
	gf_vec_norm(&cam->up);
	camera_changed(sr, cam);
}

/*for translation moves when jumping*/
#define JUMP_SCALE_FACTOR	4

static void view_translate_x(Render *sr, GF_Camera *cam, Fixed dx)
{
	SFVec3f v;
	if (!dx) return;
	if (cam->jumping) dx *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(camera_get_right_dir(cam), dx);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(sr, cam);
}
static void view_translate_y(Render *sr, GF_Camera *cam, Fixed dy)
{
	SFVec3f v;
	if (!dy) return;
	if (cam->jumping) dy *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(cam->up, dy);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(sr, cam);
}

static void view_translate_z(Render *sr, GF_Camera *cam, Fixed dz)
{
	SFVec3f v;
	if (!dz) return;
	if (cam->jumping) dz *= JUMP_SCALE_FACTOR;
	dz = gf_mulfix(dz, cam->speed);
	v = gf_vec_scale(camera_get_target_dir(cam), dz);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(sr, cam);
}

static void view_zoom(Render *sr, GF_Camera *cam, Fixed z)
{
	Fixed oz;
	if ((z>FIX_ONE) || (z<-FIX_ONE)) return;
	oz = gf_divfix(cam->vp_fov, cam->fieldOfView);
	if (oz<FIX_ONE) z/=4; oz += z;
	if (oz<=0) return;

	cam->fieldOfView = gf_divfix(cam->vp_fov, oz);
	if (cam->fieldOfView>GF_PI) cam->fieldOfView=GF_PI;
	camera_changed(sr, cam);
}

static void render_fit_screen(Render *sr)
{
	GF_TraverseState tr_state;
	SFVec3f pos, diff;
	Fixed dist, d;
	GF_Camera *cam;
	GF_Node *top;

	if (gf_list_count(sr->visual->back_stack)) return;
	if (gf_list_count(sr->visual->view_stack)) return;

	gf_mx_p(sr->compositor->mx);
	top = gf_sg_get_root_node(sr->compositor->scene);
	if (!top) {
		gf_mx_v(sr->compositor->mx);
		return;
	}
	memset(&tr_state, 0, sizeof(GF_TraverseState));
	tr_state.traversing_mode = TRAVERSE_GET_BOUNDS;
	gf_node_render(top, &tr_state);
	if (!tr_state.bbox.is_set) {
		gf_mx_v(sr->compositor->mx);
		return;
	}

	cam = &sr->visual->camera;

	/*fit is based on bounding sphere*/
	dist = gf_divfix(tr_state.bbox.radius, gf_sin(cam->fieldOfView/2) );
	gf_vec_diff(diff, cam->center, tr_state.bbox.center);
	/*do not update if camera is outside the scene bounding sphere and dist is too close*/
	if (gf_vec_len(diff) > tr_state.bbox.radius + cam->radius) {
		gf_vec_diff(diff, cam->vp_position, tr_state.bbox.center);
		d = gf_vec_len(diff);
		if (d<dist) {
			gf_mx_v(sr->compositor->mx);
			return;
		}
	}
		
	diff = gf_vec_scale(camera_get_pos_dir(cam), dist);
	gf_vec_add(pos, tr_state.bbox.center, diff);
	diff = cam->position;
	camera_set_vectors(cam, pos, cam->vp_orientation, cam->fieldOfView);
	cam->position = diff;
	camera_move_to(cam, pos, cam->target, cam->up);
	cam->examine_center = tr_state.bbox.center;
	cam->flags |= CF_STORE_VP;
	camera_changed(sr, cam);
	gf_mx_v(sr->compositor->mx);
}

static Bool render_handle_navigation_3d(Render *sr, GF_Event *ev)
{
	Fixed x, y, trans_scale;
	Fixed dx, dy, key_trans, key_pan, key_exam;
	s32 key_inv;
	u32 keys;
	Bool is_pixel_metrics;
	GF_Camera *cam;

	cam = NULL;
	if (sr->active_layer) {
		cam = render_layer3d_get_camera(sr->active_layer);
		is_pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(sr->active_layer));
	} 
	if (!cam) {
		cam = &sr->visual->camera;
		assert(sr->compositor);
		assert(sr->compositor->scene);
		is_pixel_metrics = sr->traverse_state->pixel_metrics;
	}

	if (!cam->navigate_mode) return 0;
	keys = sr->compositor->key_states;
	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->type<=GF_EVENT_MOUSEWHEEL) {
		x = gf_divfix( INT2FIX(ev->mouse.x + (s32) sr->visual->width/2), INT2FIX(sr->visual->width));
		y = gf_divfix( INT2FIX(ev->mouse.y + (s32) sr->visual->height/2), INT2FIX(sr->visual->height));
	}

	dx = (x - sr->grab_x); 
	dy = (y - sr->grab_y);

/*	trans_scale = is_pixel_metrics ? cam->width/2 : INT2FIX(10);
	key_trans = is_pixel_metrics ? INT2FIX(10) : cam->avatar_size.x;
*/
	trans_scale = cam->width/2;
	key_trans = cam->avatar_size.x;

	key_pan = FIX_ONE/25;
	key_exam = FIX_ONE/10;
	key_inv = 1;

	if (keys & GF_KEY_MOD_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_pan *= 4;
		key_exam *= 4;
		key_trans*=4;
	}

	switch (ev->type) {
	case GF_EVENT_MOUSEDOWN:
		/*left*/
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			sr->grab_x = x;
			sr->grab_y = y;
			sr->navigation_grabbed = 1;

			/*change vp and examine center to current location*/
			if ((keys & GF_KEY_MOD_CTRL) && sr->hit_info.picked_square_dist) {
				cam->vp_position = cam->position;
				cam->vp_orientation = camera_get_orientation(cam->position, cam->target, cam->up);
				cam->vp_fov = cam->fieldOfView;
				cam->examine_center = sr->hit_info.world_point;
				camera_changed(sr, cam);
				return 1;
			}
		} 
		/*right*/
		else if (ev->mouse.button==GF_MOUSE_RIGHT) {
			if (sr->navigation_grabbed && (cam->navigate_mode==GF_NAVIGATE_WALK)) {
				camera_jump(cam);
				gf_sr_invalidate(sr->compositor, NULL);
				return 1;
			}
			else if (keys & GF_KEY_MOD_CTRL) render_fit_screen(sr);
		}
		break;

	/* note: shortcuts are mostly the same as blaxxun contact, I don't feel like remembering 2 sets...*/
	case GF_EVENT_MOUSEMOVE:
		if (!sr->navigation_grabbed) {
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				/*init mode*/
				sr->grab_x = x;
				sr->grab_y = y;
				sr->navigation_grabbed = 1;
			}
			return 0;
		}

		switch (cam->navigate_mode) {
		/*FIXME- we'll likely need a "step" value for walk at some point*/
		case GF_NAVIGATE_WALK:
		case GF_NAVIGATE_FLY:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KEY_MOD_CTRL) view_pan_y(sr, cam, dy);
			else view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			break;
		case GF_NAVIGATE_VR:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KEY_MOD_CTRL) view_zoom(sr, cam, dy);
			else view_pan_y(sr, cam, dy);
			break;
		case GF_NAVIGATE_PAN:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KEY_MOD_CTRL) view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			else view_pan_y(sr, cam, dy);
			break;
		case GF_NAVIGATE_SLIDE:
			view_translate_x(sr, cam, gf_mulfix(dx, trans_scale));
			if (keys & GF_KEY_MOD_CTRL) view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			else view_translate_y(sr, cam, gf_mulfix(dy, trans_scale));
			break;
		case GF_NAVIGATE_EXAMINE:
			if (keys & GF_KEY_MOD_CTRL) {
				view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
				view_roll(sr, cam, gf_mulfix(dx, trans_scale));
			} else {
				view_exam_x(sr, cam, -gf_mulfix(GF_PI, dx));
				view_exam_y(sr, cam, gf_mulfix(GF_PI, dy));
			}
			break;
		case GF_NAVIGATE_ORBIT:
			if (keys & GF_KEY_MOD_CTRL) {
				view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			} else {
				view_orbit_x(sr, cam, -gf_mulfix(GF_PI, dx));
				view_orbit_y(sr, cam, gf_mulfix(GF_PI, dy));
			}
			break;
		case GF_NAVIGATE_GAME:
			view_pan_x(sr, cam, -dx);
			view_pan_y(sr, cam, dy);
			break;
		}
		sr->grab_x = x;
		sr->grab_y = y;
		return 1;

	case GF_EVENT_MOUSEWHEEL:
		switch (cam->navigate_mode) {
		/*FIXME- we'll likely need a "step" value for walk at some point*/
		case GF_NAVIGATE_WALK:
		case GF_NAVIGATE_FLY:
			view_pan_y(sr, cam, gf_mulfix(key_pan, ev->mouse.wheel_pos));
			break;
		case GF_NAVIGATE_VR:
			view_zoom(sr, cam, gf_mulfix(key_pan, ev->mouse.wheel_pos));
			break;
		case GF_NAVIGATE_SLIDE:
		case GF_NAVIGATE_EXAMINE:
		case GF_NAVIGATE_ORBIT:
		case GF_NAVIGATE_PAN:
			if (is_pixel_metrics) {
				view_translate_z(sr, cam, gf_mulfix(trans_scale, ev->mouse.wheel_pos) * ((keys & GF_KEY_MOD_SHIFT) ? 4 : 1));
			} else {
				view_translate_z(sr, cam, ev->mouse.wheel_pos * ((keys & GF_KEY_MOD_SHIFT) ? 4 : 1));
			}
			break;
		}
		return 1;

	case GF_EVENT_MOUSEUP:
		if (ev->mouse.button==GF_MOUSE_LEFT) sr->navigation_grabbed = 0;
		break;

	case GF_EVENT_KEYDOWN:
		switch (ev->key.key_code) {
		case GF_KEY_BACKSPACE:
			gf_sr_reset_graphics(sr->compositor);
			return 1;
		case GF_KEY_C:
			sr->collide_mode = sr->collide_mode  ? GF_COLLISION_NONE : GF_COLLISION_DISPLACEMENT;
			return 1;
		case GF_KEY_J:
			if (cam->navigate_mode==GF_NAVIGATE_WALK) {
				camera_jump(cam);
				gf_sr_invalidate(sr->compositor, NULL);
				return 1;
			}
			break;
		case GF_KEY_HOME:
			if (!sr->navigation_grabbed) render_3d_reset_camera(sr);
			break;
		case GF_KEY_END:
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				cam->navigate_mode = GF_NAVIGATE_WALK;
				sr->navigation_grabbed = 0;
				return 1;
			}
			break;
		case GF_KEY_LEFT: key_inv = -1;
		case GF_KEY_RIGHT:
			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KEY_MOD_CTRL) view_pan_x(sr, cam, key_inv * key_pan);
				else view_translate_x(sr, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KEY_MOD_CTRL) view_roll(sr, cam, gf_mulfix(dx, trans_scale));
				else view_exam_x(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KEY_MOD_CTRL) view_translate_x(sr, cam, key_inv * key_trans);
				else view_orbit_x(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_GAME: view_translate_x(sr, cam, key_inv * key_trans); break;
			case GF_NAVIGATE_VR: view_pan_x(sr, cam, -key_inv * key_pan); break;
			/*walk/fly/pan*/
			default:
				if (keys & GF_KEY_MOD_CTRL) view_translate_x(sr, cam, key_inv * key_trans);
				else view_pan_x(sr, cam, -key_inv * key_pan);
				break;
			}
			return 1;
		case GF_KEY_DOWN: key_inv = -1;
		case GF_KEY_UP:
			if (keys & GF_KEY_MOD_ALT) return 0;
			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KEY_MOD_CTRL) view_translate_z(sr, cam, key_inv * key_trans);
				else view_translate_y(sr, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KEY_MOD_CTRL) view_translate_z(sr, cam, key_inv * key_trans);
				else view_exam_y(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KEY_MOD_CTRL) view_translate_y(sr, cam, key_inv * key_trans);
				else view_orbit_y(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_PAN:
				if (keys & GF_KEY_MOD_CTRL) view_translate_y(sr, cam, key_inv * key_trans);
				else view_pan_y(sr, cam, key_inv * key_pan);
				break;
			case GF_NAVIGATE_GAME: view_translate_z(sr, cam, key_inv * key_trans); break;
			case GF_NAVIGATE_VR:
				if (keys & GF_KEY_MOD_CTRL) view_zoom(sr, cam, key_inv * key_pan);
				else view_pan_y(sr, cam, key_inv * key_pan);
				break;
			/*walk/fly*/
			default:
				if (keys & GF_KEY_MOD_CTRL) view_pan_y(sr, cam, key_inv * key_pan);
				else view_translate_z(sr, cam, key_inv * key_trans);
				break;
			}
			return 1;

		case GF_KEY_PAGEDOWN:
			if (keys & GF_KEY_MOD_CTRL) { view_zoom(sr, cam, FIX_ONE/10); return 1; }
			break;
		case GF_KEY_PAGEUP:
			if (keys & GF_KEY_MOD_CTRL) { view_zoom(sr, cam, -FIX_ONE/10); return 1; }
			break;
		}
		break;
	}
	return 0;
}

#endif

static void render_set_zoom_trans_2d(GF_VisualManager *surf, Fixed zoom, Fixed dx, Fixed dy) 
{
	render_2d_set_user_transform(surf->render, zoom, surf->render->trans_x + dx, surf->render->trans_y + dy, 0); 
#ifndef GPAC_DISABLE_3D
	if (surf->type_3d) camera_changed(surf->render, &surf->camera);
#endif
}


static Bool render_handle_navigation_2d(GF_VisualManager *surf, GF_Event *ev)
{
	Fixed x, y, dx, dy, key_trans, key_rot, zoom;
	u32 navigation_mode;
	s32 key_inv;
	Bool is_pixel_metrics = surf->render->traverse_state->pixel_metrics;
	u32 keys = surf->render->compositor->key_states;

	zoom = surf->render->zoom;
	navigation_mode = surf->render->navigate_mode;
#ifndef GPAC_DISABLE_3D
	if (surf->type_3d) navigation_mode = surf->camera.navigate_mode;
#endif

	if (!navigation_mode /*&& !(keys & GF_KEY_MOD_ALT) */) return 0;


	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->type<=GF_EVENT_MOUSEWHEEL) {
		x = INT2FIX(ev->mouse.x);
		y = INT2FIX(ev->mouse.y);
	}
	dx = x - surf->render->grab_x;
	dy = y - surf->render->grab_y;
	if (!is_pixel_metrics) { dx /= surf->width; dy /= surf->height; }

	key_inv = 1;
	key_trans = INT2FIX(2);
	key_rot = GF_PI/100;

	if (keys & GF_KEY_MOD_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_rot *= 4;
		key_trans*=4;
	}

	if (!is_pixel_metrics) { key_trans /= surf->width;}

	switch (ev->type) {
	case GF_EVENT_MOUSEDOWN:
		/*left*/
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			surf->render->grab_x = x;
			surf->render->grab_y = y;
			surf->render->navigation_grabbed = 1;
			return 0;
		}
		break;

	case GF_EVENT_MOUSEUP:
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			surf->render->navigation_grabbed = 0;
			return 0;
		}
		break;

	case GF_EVENT_MOUSEMOVE:
		if (!surf->render->navigation_grabbed) return 0;
		switch (navigation_mode) {
		case GF_NAVIGATE_SLIDE:
			if (keys & GF_KEY_MOD_CTRL) {
				Fixed new_zoom = zoom;
				if (new_zoom > FIX_ONE) new_zoom += dy/20;
				else new_zoom += dy/80;
				render_set_zoom_trans_2d(surf, new_zoom, 0, 0);
			} else {
				render_set_zoom_trans_2d(surf, zoom, dx, dy);
			}
			break;
		case GF_NAVIGATE_EXAMINE:
		{
			Fixed sin = gf_mulfix(GF_PI, dy) / surf->height;
			surf->render->rotation += gf_asin(sin);
			render_set_zoom_trans_2d(surf, zoom, 0, 0);
		}
			break;
		}
		surf->render->grab_x = x;
		surf->render->grab_y = y;
		return 1;
	case GF_EVENT_KEYDOWN:
		switch (ev->key.key_code) {
		case GF_KEY_BACKSPACE:
			gf_sr_reset_graphics(surf->render->compositor);
			return 1;
		case GF_KEY_HOME:
			if (!surf->render->navigation_grabbed) {
				surf->render->trans_x = surf->render->trans_y = 0;
				surf->render->rotation = 0;
				surf->render->zoom = FIX_ONE;
				render_set_zoom_trans_2d(surf, FIX_ONE, 0, 0);
			}
			return 1;
		case GF_KEY_LEFT: key_inv = -1;
		case GF_KEY_RIGHT:
			if (navigation_mode == GF_NAVIGATE_SLIDE) {
				render_set_zoom_trans_2d(surf, zoom, key_inv*key_trans, 0);
			}
			else {
				surf->render->rotation -= key_inv * key_rot;
				render_set_zoom_trans_2d(surf, zoom, 0, 0);
			}
			return 1;
		case GF_KEY_DOWN: key_inv = -1;
		case GF_KEY_UP:
			if (navigation_mode == GF_NAVIGATE_SLIDE) {
				if (keys & GF_KEY_MOD_CTRL) {
					Fixed new_zoom = zoom;
					if (new_zoom > FIX_ONE) new_zoom += key_inv*FIX_ONE/10;
					else new_zoom += key_inv*FIX_ONE/20;
					render_set_zoom_trans_2d(surf, new_zoom, 0, 0);
				} else {
					render_set_zoom_trans_2d(surf, zoom, 0, key_inv*key_trans);
				}
			}
			else {
				surf->render->rotation += key_inv*key_rot;
				render_set_zoom_trans_2d(surf, zoom, 0, 0);
			}
			return 1;
		}
		break;
	}
	return 0;
}


Bool render_handle_navigation(Render *sr, GF_Event *ev)
{
#ifndef GPAC_DISABLE_3D
	if ( (sr->visual->type_3d>1) || sr->active_layer) {
		return render_handle_navigation_3d(sr, ev);
	}
#endif
	return render_handle_navigation_2d(sr->visual, ev);
}
