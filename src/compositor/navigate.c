/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include "visual_manager.h"


#ifndef GPAC_DISABLE_3D

static void camera_changed(GF_Compositor *compositor, GF_Camera *cam)
{
	cam->flags |= CAM_IS_DIRTY;
	gf_sc_invalidate(compositor, NULL);
	if (compositor->active_layer) gf_node_dirty_set(compositor->active_layer, 0, 1);
}

#endif

static void nav_set_zoom_trans_2d(GF_VisualManager *visual, Fixed zoom, Fixed dx, Fixed dy)
{
	compositor_2d_set_user_transform(visual->compositor, zoom, visual->compositor->trans_x + dx, visual->compositor->trans_y + dy, 0);
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d) camera_changed(visual->compositor, &visual->camera);
#endif
}


#ifndef GPAC_DISABLE_3D

/*shortcut*/
static void gf_mx_rotation_matrix(GF_Matrix *mx, SFVec3f axis_pt, SFVec3f axis, Fixed angle)
{
	gf_mx_init(*mx);
	gf_mx_add_translation(mx, axis_pt.x, axis_pt.y, axis_pt.z);
	gf_mx_add_rotation(mx, angle, axis.x, axis.y, axis.z);
	gf_mx_add_translation(mx, -axis_pt.x, -axis_pt.y, -axis_pt.z);
}

static void view_orbit_x(GF_Compositor *compositor, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->target, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	camera_changed(compositor, cam);
}
static void view_orbit_y(GF_Compositor *compositor, GF_Camera *cam, Fixed dy)
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
	camera_changed(compositor, cam);
}

static void view_exam_x(GF_Compositor *compositor, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->examine_center, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	gf_mx_apply_vec(&mx, &cam->target);
	camera_changed(compositor, cam);
}

static void view_exam_y(GF_Compositor *compositor, GF_Camera *cam, Fixed dy)
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
	camera_changed(compositor, cam);
}

static void view_roll(GF_Compositor *compositor, GF_Camera *cam, Fixed dd)
{
	GF_Matrix mx;
	SFVec3f delta;
	if (!dd) return;
	gf_vec_add(delta, cam->target, cam->up);
	gf_mx_rotation_matrix(&mx, cam->target, camera_get_pos_dir(cam), dd);
	gf_mx_apply_vec(&mx, &delta);
	gf_vec_diff(cam->up, delta, cam->target);
	gf_vec_norm(&cam->up);
	camera_changed(compositor, cam);
}

static void update_pan_up(GF_Compositor *compositor, GF_Camera *cam)
{
	SFVec3f axis, dir;
	/*update up vector so that right is always horizontal (no y component)*/
	dir = camera_get_pos_dir(cam);
	axis = camera_get_right_dir(cam);
	axis.y = 0;
	gf_vec_norm(&axis);
	cam->up = gf_vec_cross(dir, axis);
	gf_vec_norm(&cam->up);

	camera_changed(compositor, cam);
}

static void view_pan_x(GF_Compositor *compositor, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->position, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->target);

	update_pan_up(compositor, cam);
}
static void view_pan_y(GF_Compositor *compositor, GF_Camera *cam, Fixed dy)
{
	GF_Matrix mx;
	GF_Vec prev_target = cam->target;
	if (!dy) return;
	gf_mx_rotation_matrix(&mx, cam->position, camera_get_right_dir(cam), dy);
	gf_mx_apply_vec(&mx, &cam->target);
	switch (cam->navigate_mode) {
	case GF_NAVIGATE_WALK:
	case GF_NAVIGATE_VR:
	case GF_NAVIGATE_GAME:
		if (cam->target.z*prev_target.z<0) {
			cam->target = prev_target;
			return;
		}
	default:
		break;
	}

	update_pan_up(compositor, cam);
}

/*for translation moves when jumping*/
#define JUMP_SCALE_FACTOR	4

static void view_translate_x(GF_Compositor *compositor, GF_Camera *cam, Fixed dx)
{
	SFVec3f v;
	if (!dx) return;
	if (cam->jumping) dx *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(camera_get_right_dir(cam), dx);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(compositor, cam);
}
static void view_translate_y(GF_Compositor *compositor, GF_Camera *cam, Fixed dy)
{
	SFVec3f v;
	if (!dy) return;
	if (cam->jumping) dy *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(cam->up, dy);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(compositor, cam);
}

static void view_translate_z(GF_Compositor *compositor, GF_Camera *cam, Fixed dz)
{
	SFVec3f v;
	if (!dz) return;
	if (cam->jumping) dz *= JUMP_SCALE_FACTOR;
	dz = gf_mulfix(dz, cam->speed);
	v = gf_vec_scale(camera_get_target_dir(cam), dz);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(compositor, cam);
}

static void view_zoom(GF_Compositor *compositor, GF_Camera *cam, Fixed z)
{
	Fixed oz;
	if ((z>FIX_ONE) || (z<-FIX_ONE)) return;
	oz = gf_divfix(cam->vp_fov, cam->fieldOfView);
	if (oz<FIX_ONE) z/=4;
	oz += z;
	if (oz<=0) return;

	cam->fieldOfView = gf_divfix(cam->vp_fov, oz);
	if (cam->fieldOfView>GF_PI) cam->fieldOfView=GF_PI;
	camera_changed(compositor, cam);
}

Bool gf_sc_fit_world_to_screen(GF_Compositor *compositor)
{
	GF_TraverseState tr_state;
	SFVec3f pos, diff;
	Fixed dist, d;
	GF_Camera *cam;
	GF_Node *top;

#ifndef GPAC_DISABLE_VRML
//	if (gf_list_count(compositor->visual->back_stack)) return;
	if (gf_list_count(compositor->visual->view_stack)) return 0;
#endif

	gf_mx_p(compositor->mx);
	top = gf_sg_get_root_node(compositor->scene);
	if (!top) {
		gf_mx_v(compositor->mx);
		return 0;
	}
	memset(&tr_state, 0, sizeof(GF_TraverseState));
	gf_mx_init(tr_state.model_matrix);
	tr_state.traversing_mode = TRAVERSE_GET_BOUNDS;
	tr_state.visual = compositor->visual;
	gf_node_traverse(top, &tr_state);
	if (gf_node_dirty_get(top)) {
		tr_state.bbox.is_set = 0;
	}

	if (!tr_state.bbox.is_set) {
		gf_mx_v(compositor->mx);
		/*empty world ...*/
		if (tr_state.bbox.radius==-1) return 1;
		/*2D world with 3D camera forced*/
		if (tr_state.bounds.width&&tr_state.bounds.height) return 1;
		return 0;
	}

	cam = &compositor->visual->camera;

	cam->world_bbox = tr_state.bbox;
	/*fit is based on bounding sphere*/
	dist = gf_divfix(tr_state.bbox.radius, gf_sin(cam->fieldOfView/2) );
	gf_vec_diff(diff, cam->center, tr_state.bbox.center);
	/*do not update if camera is outside the scene bounding sphere and dist is too close*/
	if (gf_vec_len(diff) > tr_state.bbox.radius + cam->radius) {
		gf_vec_diff(diff, cam->vp_position, tr_state.bbox.center);
		d = gf_vec_len(diff);
		if (d<dist) {
			gf_mx_v(compositor->mx);
			return 1;
		}
	}

	diff = gf_vec_scale(camera_get_pos_dir(cam), dist);
	gf_vec_add(pos, tr_state.bbox.center, diff);
	diff = cam->position;
	camera_set_vectors(cam, pos, cam->vp_orientation, cam->fieldOfView);
	cam->position = diff;
	camera_move_to(cam, pos, cam->target, cam->up);
	if (!compositor->player) {
		camera_stop_anim(cam);
		camera_set_vectors(cam, cam->end_pos, cam->end_ori, cam->end_fov);
	}

	cam->examine_center = tr_state.bbox.center;
	cam->flags |= CF_STORE_VP;
	if (cam->z_far < dist) cam->z_far = 10*dist;
	camera_changed(compositor, cam);
	gf_mx_v(compositor->mx);
	return 1;
}

static void handle_mouse_move_3d(GF_Compositor *compositor, GF_Camera *cam, u32 keys, Fixed dx, Fixed dy)
{
	Fixed trans_scale = cam->width/20;
	//if default VP is quite far from center use larger dz/dy moves
	if (cam->vp_dist>100) trans_scale *= 10;

	switch (cam->navigate_mode) {
	/*FIXME- we'll likely need a "step" value for walk at some point*/
	case GF_NAVIGATE_WALK:
	case GF_NAVIGATE_FLY:
		view_pan_x(compositor, cam, -dx);
		if (keys & GF_KEY_MOD_CTRL) view_pan_y(compositor, cam, dy);
		else view_translate_z(compositor, cam, gf_mulfix(dy, trans_scale));
		break;
	case GF_NAVIGATE_VR:
		view_pan_x(compositor, cam, -dx);
		if (keys & GF_KEY_MOD_CTRL) view_zoom(compositor, cam, dy);
		else view_pan_y(compositor, cam, dy);
		break;
	case GF_NAVIGATE_PAN:
		view_pan_x(compositor, cam, -dx);
		if (keys & GF_KEY_MOD_CTRL) view_translate_z(compositor, cam, gf_mulfix(dy, trans_scale));
		else view_pan_y(compositor, cam, dy);
		break;
	case GF_NAVIGATE_SLIDE:
		view_translate_x(compositor, cam, gf_mulfix(dx, trans_scale));
		if (keys & GF_KEY_MOD_CTRL) view_translate_z(compositor, cam, gf_mulfix(dy, trans_scale));
		else view_translate_y(compositor, cam, gf_mulfix(dy, trans_scale));
		break;
	case GF_NAVIGATE_EXAMINE:
		if (keys & GF_KEY_MOD_CTRL) {
			view_translate_z(compositor, cam, gf_mulfix(dy, trans_scale));
			view_roll(compositor, cam, gf_mulfix(dx, trans_scale));
		} else {
			if (ABS(dx) > ABS(dy)) {
				view_exam_x(compositor, cam, -gf_mulfix(GF_PI, dx));
			} else {
				view_exam_y(compositor, cam, gf_mulfix(GF_PI, dy));
			}
		}
		break;
	case GF_NAVIGATE_ORBIT:
		if (keys & GF_KEY_MOD_CTRL) {
			view_translate_z(compositor, cam, gf_mulfix(dy, trans_scale));
		} else {
			view_orbit_x(compositor, cam, -gf_mulfix(GF_PI, dx));
			view_orbit_y(compositor, cam, gf_mulfix(GF_PI, dy));
		}
		break;
	case GF_NAVIGATE_GAME:
		view_pan_x(compositor, cam, -dx);
		view_pan_y(compositor, cam, dy);
		break;
	}
}

static Bool compositor_handle_navigation_3d(GF_Compositor *compositor, GF_Event *ev)
{
	Fixed x, y, trans_scale;
	Fixed dx, dy, key_trans, key_pan, key_exam;
	s32 key_inv;
	u32 keys;
	GF_Camera *cam;
	Fixed zoom = compositor->zoom;

	cam = NULL;
#ifndef GPAC_DISABLE_VRML
	if (compositor->active_layer) {
		cam = compositor_layer3d_get_camera(compositor->active_layer);
	}
#endif

	if (!cam) {
		cam = &compositor->visual->camera;
		assert(compositor);
		assert(compositor->scene);
	}
	if (!cam || (cam->navigate_mode==GF_NAVIGATE_NONE)) return 0;

	keys = compositor->key_states;
	if (!cam->navigate_mode && !(keys & GF_KEY_MOD_ALT) ) return 0;
	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->type<=GF_EVENT_MOUSEWHEEL) {
		x = gf_divfix( INT2FIX(ev->mouse.x - (s32) compositor->visual->width/2), INT2FIX(compositor->visual->width));
		y = gf_divfix( INT2FIX(ev->mouse.y - (s32) compositor->visual->height/2), INT2FIX(compositor->visual->height));
	}

	dx = (x - compositor->grab_x);
	dy = (compositor->grab_y - y);

	trans_scale = cam->width/20;
	key_trans = cam->avatar_size.x/2;
	//if default VP is quite far from center use larger dz/dy moves
	if (cam->vp_dist>100) trans_scale *= 10;

	if (cam->world_bbox.is_set && (key_trans*5 > cam->world_bbox.radius)) {
		key_trans = cam->world_bbox.radius / 100;
	}

	key_pan = FIX_ONE/20;
	key_exam = FIX_ONE/20;
	key_inv = 1;

	if (keys & GF_KEY_MOD_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_pan *= 4;
		key_exam *= 4;
		key_trans*=4;
	}

	if (! compositor->orientation_sensors_active) {
		Fixed yaw, pitch, roll;
		gf_mx_get_yaw_pitch_roll(&compositor->visual->camera.modelview, &yaw, &pitch, &roll);
		compositor->audio_renderer->yaw = yaw;
		compositor->audio_renderer->pitch = pitch;
		compositor->audio_renderer->roll = roll;
	}

	switch (ev->type) {
	case GF_EVENT_MOUSEDOWN:
		/*left*/
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			compositor->grab_x = x;
			compositor->grab_y = y;
			compositor->navigation_state = 1;

			/*change vp and examine center to current location*/
			if ((keys & GF_KEY_MOD_CTRL) && compositor->hit_square_dist) {
				cam->vp_position = cam->position;
				cam->vp_orientation = camera_get_orientation(cam->position, cam->target, cam->up);
				cam->vp_fov = cam->fieldOfView;
				cam->examine_center = compositor->hit_world_point;
				camera_changed(compositor, cam);
				return 1;
			}
		}
		/*right*/
		else if (ev->mouse.button==GF_MOUSE_RIGHT) {
			if (compositor->navigation_state && (cam->navigate_mode==GF_NAVIGATE_WALK)) {
				camera_jump(cam);
				gf_sc_invalidate(compositor, NULL);
				return 1;
			}
			else if (keys & GF_KEY_MOD_CTRL) gf_sc_fit_world_to_screen(compositor);
		}
		break;

	case GF_EVENT_MOUSEMOVE:
		if (compositor->orientation_sensors_active) return 0;

		if (!compositor->navigation_state) {
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				/*init mode*/
				compositor->grab_x = x;
				compositor->grab_y = y;
				compositor->navigation_state = 1;
			}
			compositor->auto_rotate=0;
			return 0;
		}
		compositor->navigation_state++;

		if (x <= -0.49) compositor->auto_rotate = 1;
		else if (x >= 0.49) compositor->auto_rotate = 2;
		else if (y <= -0.49) compositor->auto_rotate = 3;
		else if (y >= 0.49) compositor->auto_rotate = 4;
		else compositor->auto_rotate = 0;

		handle_mouse_move_3d(compositor, cam, keys, dx, dy);

		compositor->grab_x = x;
		compositor->grab_y = y;
		return 1;

	case GF_EVENT_MOUSEWHEEL:
		switch (cam->navigate_mode) {
		/*FIXME- we'll likely need a "step" value for walk at some point*/
		case GF_NAVIGATE_WALK:
		case GF_NAVIGATE_FLY:
			view_pan_y(compositor, cam, gf_mulfix(key_pan, ev->mouse.wheel_pos));
			break;
		case GF_NAVIGATE_VR:
			view_zoom(compositor, cam, gf_mulfix(key_pan, ev->mouse.wheel_pos));
			break;
		case GF_NAVIGATE_SLIDE:
		case GF_NAVIGATE_EXAMINE:
		case GF_NAVIGATE_ORBIT:
		case GF_NAVIGATE_PAN:
			if (cam->is_3D) {
				view_translate_z(compositor, cam, gf_mulfix(trans_scale, ev->mouse.wheel_pos) * ((keys & GF_KEY_MOD_SHIFT) ? 4 : 1));
			} else {
				nav_set_zoom_trans_2d(compositor->visual, zoom + INT2FIX(ev->mouse.wheel_pos)/10, 0, 0);
			}
		}
		return 1;

	case GF_EVENT_MULTITOUCH:
		compositor->auto_rotate=0;
		compositor->navigation_state = 0;
		if (ev->mtouch.num_fingers==2) {
			if( ABS(ev->mtouch.pinch) * 100 > 2 ) {
				if (cam->is_3D) {
					view_translate_z(compositor, cam, gf_mulfix(cam->width, compositor->visual->width* ev->mtouch.pinch));
				} else {
					nav_set_zoom_trans_2d(compositor->visual, zoom + gf_mulfix(trans_scale, ev->mtouch.pinch), 0, 0);
				}
				return 1;
			}
			if( ABS(ev->mtouch.rotation) > GF_PI/40 ) {
				view_roll(compositor, cam, gf_mulfix(ev->mtouch.rotation, trans_scale));
				return 1;
			}
		} else if (ev->mtouch.num_fingers==3) {
			compositor->visual->camera.start_zoom = compositor->zoom;
			compositor->zoom = FIX_ONE;
			compositor->interoccular_offset = 0;
			compositor->focdist = 0;
			compositor->interoccular_offset = 0;
			compositor->focdist = 0;
			compositor_3d_reset_camera(compositor);
			return 1;
		}
		return 0;

	case GF_EVENT_MOUSEUP:
		compositor->auto_rotate=0;
		if (ev->mouse.button==GF_MOUSE_LEFT) compositor->navigation_state = 0;
		break;

	case GF_EVENT_KEYDOWN:
		switch (ev->key.key_code) {
		case GF_KEY_BACKSPACE:
			gf_sc_reset_graphics(compositor);
			return 1;
		case GF_KEY_C:
			compositor->collide_mode = compositor->collide_mode  ? GF_COLLISION_NONE : GF_COLLISION_DISPLACEMENT;
			return 1;
		case GF_KEY_J:
			if (cam->navigate_mode==GF_NAVIGATE_WALK) {
				camera_jump(cam);
				gf_sc_invalidate(compositor, NULL);
				return 1;
			}
			break;
		case GF_KEY_HOME:
			if (!compositor->navigation_state) {
				compositor->visual->camera.start_zoom = compositor->zoom;
				compositor->zoom = FIX_ONE;
				compositor->interoccular_offset = 0;
				compositor->focdist = 0;
				compositor->interoccular_offset = 0;
				compositor->focdist = 0;
				compositor_3d_reset_camera(compositor);
			}
			break;
		case GF_KEY_END:
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				cam->navigate_mode = GF_NAVIGATE_WALK;
				compositor->navigation_state = 0;
				return 1;
			}
			break;
		case GF_KEY_LEFT:
			key_inv = -1;
		case GF_KEY_RIGHT:
			if (keys & GF_KEY_MOD_ALT) {
				if ( (keys & GF_KEY_MOD_SHIFT) && (compositor->visual->nb_views > 1) ) {
					/*+ or - 10 cm*/
					compositor->focdist += INT2FIX(key_inv);
					cam->flags |= CAM_IS_DIRTY;
					GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("AutoStereo view distance %f - focus %f\n", FIX2FLT(compositor->video_out->dispdist)/100, FIX2FLT(compositor->focdist)/100));
					gf_sc_invalidate(compositor, NULL);
					return 1;
				}
				return 0;
			}


			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KEY_MOD_CTRL) view_pan_x(compositor, cam, key_inv * key_pan);
				else view_translate_x(compositor, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KEY_MOD_CTRL) view_roll(compositor, cam, gf_mulfix(dx, trans_scale));
				else view_exam_x(compositor, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KEY_MOD_CTRL) view_translate_x(compositor, cam, key_inv * key_trans);
				else view_orbit_x(compositor, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_GAME:
				view_translate_x(compositor, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_VR:
				view_pan_x(compositor, cam, -key_inv * key_pan);
				break;
			/*walk/fly/pan*/
			default:
				if (keys & GF_KEY_MOD_CTRL) view_translate_x(compositor, cam, key_inv * key_trans);
				else view_pan_x(compositor, cam, -key_inv * key_pan);
				break;
			}
			return 1;
		case GF_KEY_DOWN:
			key_inv = -1;
		case GF_KEY_UP:
			if (keys & GF_KEY_MOD_ALT) {
				if ( (keys & GF_KEY_MOD_SHIFT) && (compositor->visual->nb_views > 1) ) {
					compositor->interoccular_offset += FLT2FIX(0.5) * key_inv;
					GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("AutoStereo interoccular distance %f\n", FIX2FLT(compositor->iod + compositor->interoccular_offset)));
					cam->flags |= CAM_IS_DIRTY;
					gf_sc_invalidate(compositor, NULL);
					return 1;
				}
				return 0;
			}
			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KEY_MOD_CTRL) view_translate_z(compositor, cam, key_inv * key_trans);
				else view_translate_y(compositor, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KEY_MOD_CTRL) view_translate_z(compositor, cam, key_inv * key_trans);
				else view_exam_y(compositor, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KEY_MOD_CTRL) view_translate_y(compositor, cam, key_inv * key_trans);
				else view_orbit_y(compositor, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_PAN:
				if (keys & GF_KEY_MOD_CTRL) view_translate_y(compositor, cam, key_inv * key_trans);
				else view_pan_y(compositor, cam, key_inv * key_pan);
				break;
			case GF_NAVIGATE_GAME:
				view_translate_z(compositor, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_VR:
				if (keys & GF_KEY_MOD_CTRL) view_zoom(compositor, cam, key_inv * key_pan);
				else view_pan_y(compositor, cam, key_inv * key_pan);
				break;
			/*walk/fly*/
			default:
				if (keys & GF_KEY_MOD_CTRL) view_pan_y(compositor, cam, key_inv * key_pan);
				else view_translate_z(compositor, cam, key_inv * key_trans);
				break;
			}
			return 1;

		case GF_KEY_PAGEDOWN:
			if (keys & GF_KEY_MOD_CTRL) {
				view_zoom(compositor, cam, FIX_ONE/10);
				return 1;
			}
			break;
		case GF_KEY_PAGEUP:
			if (keys & GF_KEY_MOD_CTRL) {
				view_zoom(compositor, cam, -FIX_ONE/10);
				return 1;
			}
			break;
		case GF_KEY_D:
			if (keys & GF_KEY_MOD_CTRL) {
				if (compositor->tvtd==TILE_DEBUG_FULL) compositor->tvtd = 0;
				else compositor->tvtd ++;
				gf_sc_invalidate(compositor, NULL);
				return 1;
			}
			break;
		case GF_KEY_G:
			if (keys & GF_KEY_MOD_CTRL) {
				compositor->vrhud_mode++;
				if (compositor->vrhud_mode==5) compositor->vrhud_mode=0;
				gf_sc_invalidate(compositor, NULL);
				return 1;
			}
			break;
		case GF_KEY_A:
			if (keys & GF_KEY_MOD_CTRL) {
				compositor->tvtf = !compositor->tvtf;
				gf_sc_invalidate(compositor, NULL);
				return 1;
			}
			break;
		}
		break;
	case GF_EVENT_SENSOR_ORIENTATION:
	{
		Fixed z, w, yaw, /*pitch, */roll;
		GF_Vec target;
		GF_Matrix mx;

		/* In iOS we get x, y, z in quaternions (first measurement is the frame of reference) */
		if (ev->sensor.w) {
			x = ev->sensor.x;
			y = ev->sensor.y;
			z = ev->sensor.z;
			w = ev->sensor.w;
		
			yaw = gf_atan2(2*gf_mulfix(z,w) - 2*gf_mulfix(y,x) , 1 - 2*gf_mulfix(z,z) - 2*gf_mulfix(x,x));
			//pitch = asin(2*y*z + 2*x*w);
			roll = gf_atan2(2*gf_mulfix(y,w) - 2*gf_mulfix(z,x) , 1 - 2*gf_mulfix(y,y) - 2*gf_mulfix(x,x));
		} else {
			/*
			 * In Android we get yaw, pitch, roll values (in rad)
			 * The frame of reference is absolute
			 */
			yaw = ev->sensor.x;
			//pitch = ev->sensor.y;
			roll = ev->sensor.z;
		}
		target.x = 0;
		target.y = -FIX_ONE;
		target.z = 0;
		gf_mx_init(mx);
		gf_mx_add_rotation(&mx, yaw, 0, FIX_ONE, 0);
		gf_mx_add_rotation(&mx, -roll, FIX_ONE, 0, 0);
		
		gf_mx_apply_vec(&mx, &target);

		cam->target = target;
		update_pan_up(compositor, cam);
	}
		return 1;
	}
	return 0;
}

#endif


static Bool compositor_handle_navigation_2d(GF_VisualManager *visual, GF_Event *ev)
{
	Fixed x, y, dx, dy, key_trans, key_rot, zoom, new_zoom;
	u32 navigation_mode;
	s32 key_inv;
	Bool is_pixel_metrics = visual->compositor->traverse_state->pixel_metrics;
	u32 keys = visual->compositor->key_states;

	zoom = visual->compositor->zoom;
	navigation_mode = visual->compositor->navigate_mode;
#ifndef GPAC_DISABLE_3D
	if (visual->type_3d) navigation_mode = visual->camera.navigate_mode;
#endif

	if (navigation_mode==GF_NAVIGATE_NONE) return 0;
	if (!navigation_mode && !(keys & GF_KEY_MOD_ALT) ) return 0;


	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->type<=GF_EVENT_MOUSEWHEEL) {
		x = INT2FIX(ev->mouse.x);
		y = INT2FIX(ev->mouse.y);
	}
	dx = x - visual->compositor->grab_x;
	if (visual->center_coords) {
		dy = visual->compositor->grab_y - y;
	} else {
		dy = y - visual->compositor->grab_y;
	}
	if (!is_pixel_metrics) {
		dx /= visual->width;
		dy /= visual->height;
	}

	key_inv = 1;
	key_trans = INT2FIX(2);
	key_rot = GF_PI/100;

	if (keys & GF_KEY_MOD_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_rot *= 4;
		key_trans*=4;
	}

	if (!is_pixel_metrics) {
		key_trans /= visual->width;
	}

	switch (ev->type) {
	case GF_EVENT_MOUSEDOWN:
		/*left*/
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			visual->compositor->grab_x = x;
			visual->compositor->grab_y = y;
			visual->compositor->navigation_state = 1;
			/*update zoom center*/
			if (keys & GF_KEY_MOD_CTRL) {
				visual->compositor->trans_x -= visual->compositor->grab_x - INT2FIX(visual->width)/2;
				visual->compositor->trans_y += INT2FIX(visual->height)/2 - visual->compositor->grab_y;
				nav_set_zoom_trans_2d(visual, visual->compositor->zoom, 0, 0);
			}
			return 0;
		}
		break;

	case GF_EVENT_MOUSEUP:
		if (ev->mouse.button==GF_MOUSE_LEFT) {
			visual->compositor->navigation_state = 0;
			return 0;
		}
		break;
	case GF_EVENT_MULTITOUCH:
		if (ev->mtouch.num_fingers==2) {
			if( ABS(ev->mtouch.pinch) * 100 > 2 ) {
				new_zoom = zoom + ev->mtouch.pinch * MIN(visual->width, visual->height)/100;
				nav_set_zoom_trans_2d(visual, new_zoom, 0, 0);
				return 1;
			}
			if( ABS(ev->mtouch.rotation) > GF_PI/40 ) {
				visual->compositor->rotation -= ev->mtouch.rotation;
				nav_set_zoom_trans_2d(visual, zoom, 0, 0);
				return 1;
			}
		} else if (ev->mtouch.num_fingers==3) {
			visual->compositor->trans_x = visual->compositor->trans_y = 0;
			visual->compositor->rotation = 0;
			visual->compositor->zoom = FIX_ONE;
			nav_set_zoom_trans_2d(visual, FIX_ONE, 0, 0);
			return 1;
		}
		return 0;

	case GF_EVENT_MOUSEWHEEL:
		switch (navigation_mode) {
		case GF_NAVIGATE_SLIDE:
			new_zoom = zoom + INT2FIX(ev->mouse.wheel_pos)/10;
			nav_set_zoom_trans_2d(visual, new_zoom, 0, 0);
			return 1;
		case GF_NAVIGATE_EXAMINE:
			if (ev->mouse.wheel_pos>0)
				visual->compositor->rotation += gf_asin( GF_PI / 10);
			else
				visual->compositor->rotation -= gf_asin( GF_PI / 10);
			nav_set_zoom_trans_2d(visual, zoom, 0, 0);
			return 1;
		}
		return 0;

	case GF_EVENT_MOUSEMOVE:
		if (!visual->compositor->navigation_state) return 0;
		visual->compositor->navigation_state++;
		switch (navigation_mode) {
		case GF_NAVIGATE_SLIDE:
			if (keys & GF_KEY_MOD_CTRL) {
				if (dy) {
					new_zoom = zoom;
					if (new_zoom > FIX_ONE) new_zoom += dy/20;
					else new_zoom += dy/80;
					nav_set_zoom_trans_2d(visual, new_zoom, 0, 0);
				}
			} else {
				nav_set_zoom_trans_2d(visual, zoom, dx, dy);
			}
			break;
		case GF_NAVIGATE_EXAMINE:
		{
			Fixed sin = gf_mulfix(GF_PI, dy) / visual->height;
			//truncate in [-1;1] for arcsin()
			if (sin < -FIX_ONE)
				sin = -FIX_ONE;
			if (sin >  FIX_ONE)
				sin =  FIX_ONE;
			visual->compositor->rotation += gf_asin(sin);
			nav_set_zoom_trans_2d(visual, zoom, 0, 0);
		}
		break;
		}
		visual->compositor->grab_x = x;
		visual->compositor->grab_y = y;
		return 1;
	case GF_EVENT_KEYDOWN:
		switch (ev->key.key_code) {
		case GF_KEY_BACKSPACE:
			gf_sc_reset_graphics(visual->compositor);
			return 1;
		case GF_KEY_HOME:
			if (!visual->compositor->navigation_state) {
				visual->compositor->trans_x = visual->compositor->trans_y = 0;
				visual->compositor->rotation = 0;
				visual->compositor->zoom = FIX_ONE;
				nav_set_zoom_trans_2d(visual, FIX_ONE, 0, 0);
			}
			return 1;
		case GF_KEY_LEFT:
			key_inv = -1;
		case GF_KEY_RIGHT:
			if (navigation_mode == GF_NAVIGATE_SLIDE) {
				nav_set_zoom_trans_2d(visual, zoom, key_inv*key_trans, 0);
			}
			else {
				visual->compositor->rotation -= key_inv * key_rot;
				nav_set_zoom_trans_2d(visual, zoom, 0, 0);
			}
			return 1;
		case GF_KEY_DOWN:
			key_inv = -1;
		case GF_KEY_UP:
			if (navigation_mode == GF_NAVIGATE_SLIDE) {
				if (keys & GF_KEY_MOD_CTRL) {
					new_zoom = zoom;
					if (new_zoom > FIX_ONE) new_zoom += key_inv*FIX_ONE/10;
					else new_zoom += key_inv*FIX_ONE/20;
					nav_set_zoom_trans_2d(visual, new_zoom, 0, 0);
				} else {
					nav_set_zoom_trans_2d(visual, zoom, 0, key_inv*key_trans);
				}
			}
			else {
				visual->compositor->rotation += key_inv*key_rot;
				nav_set_zoom_trans_2d(visual, zoom, 0, 0);
			}
			return 1;
		}
		break;
	}
	return 0;
}

void compositor_handle_auto_navigation(GF_Compositor *compositor)
{
#ifndef GPAC_DISABLE_3D
	GF_Camera *cam = NULL;
	Fixed dx, dy;
#ifndef GPAC_DISABLE_VRML
	if (compositor->active_layer) {
		cam = compositor_layer3d_get_camera(compositor->active_layer);
	}
	if (!cam)
		cam = &compositor->visual->camera;
#endif

	dx = dy = 0;
	switch (compositor->auto_rotate) {
	case 1:
		dx = -FIX_ONE/100;
		break;
	case 2:
		dx = FIX_ONE/100;
		break;
	case 3:
		dy = FIX_ONE/100;
		break;
	case 4:
		dy = -FIX_ONE/100;
		break;
	default:
		return;
	}
	handle_mouse_move_3d(compositor, cam, 0, dx, dy);
#endif
}

Bool compositor_handle_navigation(GF_Compositor *compositor, GF_Event *ev)
{
	if (!compositor->scene) return 0;
#ifndef GPAC_DISABLE_3D
	if ( (compositor->visual->type_3d>0) || compositor->active_layer)
		return compositor_handle_navigation_3d(compositor, ev);
#endif
	return compositor_handle_navigation_2d(compositor->visual, ev);
}
