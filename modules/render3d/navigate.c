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
#include <gpac/options.h>

static void camera_changed(Render3D *sr, GF_Camera *cam)
{
	cam->flags |= CAM_IS_DIRTY;
	gf_sr_invalidate(sr->compositor, NULL);
}

/*shortcut*/
void gf_mx_rotation_matrix(GF_Matrix *mx, SFVec3f axis_pt, SFVec3f axis, Fixed angle)
{
	gf_mx_init(*mx);
	gf_mx_add_translation(mx, axis_pt.x, axis_pt.y, axis_pt.z);
	gf_mx_add_rotation(mx, angle, axis.x, axis.y, axis.z);
	gf_mx_add_translation(mx, -axis_pt.x, -axis_pt.y, -axis_pt.z);
}
	
void view_orbit_x(Render3D *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->target, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	camera_changed(sr, cam);
}
void view_orbit_y(Render3D *sr, GF_Camera *cam, Fixed dy)
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

void view_exam_x(Render3D *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->examine_center, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->position);
	gf_mx_apply_vec(&mx, &cam->target);
	camera_changed(sr, cam);
}

void view_exam_y(Render3D *sr, GF_Camera *cam, Fixed dy)
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

void view_roll(Render3D *sr, GF_Camera *cam, Fixed dd)
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

void view_pan_x(Render3D *sr, GF_Camera *cam, Fixed dx)
{
	GF_Matrix mx;
	if (!dx) return;
	gf_mx_rotation_matrix(&mx, cam->position, cam->up, dx);
	gf_mx_apply_vec(&mx, &cam->target);
	camera_changed(sr, cam);
}
void view_pan_y(Render3D *sr, GF_Camera *cam, Fixed dy)
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

void view_translate_x(Render3D *sr, GF_Camera *cam, Fixed dx)
{
	SFVec3f v;
	if (!dx) return;
	if (cam->jumping) dx *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(camera_get_right_dir(cam), dx);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(sr, cam);
}
void view_translate_y(Render3D *sr, GF_Camera *cam, Fixed dy)
{
	SFVec3f v;
	if (!dy) return;
	if (cam->jumping) dy *= JUMP_SCALE_FACTOR;
	v = gf_vec_scale(cam->up, dy);
	gf_vec_add(cam->target, cam->target, v);
	gf_vec_add(cam->position, cam->position, v);
	camera_changed(sr, cam);
}
void view_translate_z(Render3D *sr, GF_Camera *cam, Fixed dz)
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

void view_zoom(Render3D *sr, GF_Camera *cam, Fixed z)
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

void R3D_FitScene(Render3D *sr)
{
	RenderEffect3D eff;
	SFVec3f pos, diff;
	Fixed dist, d;
	GF_Camera *cam;
	GF_Node *top;

	if (gf_list_count(sr->surface->back_stack)) return;
	if (gf_list_count(sr->surface->view_stack)) return;

	gf_mx_p(sr->compositor->mx);
	top = gf_sg_get_root_node(sr->compositor->scene);
	if (!top) {
		gf_mx_v(sr->compositor->mx);
		return;
	}
	memset(&eff, 0, sizeof(RenderEffect3D));
	eff.traversing_mode = TRAVERSE_GET_BOUNDS;
	gf_node_render(top, &eff);
	if (!eff.bbox.is_set) {
		gf_mx_v(sr->compositor->mx);
		return;
	}

	cam = &sr->surface->camera;

	/*fit is based on bounding sphere*/
	dist = gf_divfix(eff.bbox.radius, gf_sin(cam->fieldOfView/2) );
	gf_vec_diff(diff, cam->center, eff.bbox.center);
	/*do not update if camera is outside the scene bounding sphere and dist is too close*/
	if (gf_vec_len(diff) > eff.bbox.radius + cam->radius) {
		gf_vec_diff(diff, cam->vp_position, eff.bbox.center);
		d = gf_vec_len(diff);
		if (d<dist) {
			gf_mx_v(sr->compositor->mx);
			return;
		}
	}
		
	diff = gf_vec_scale(camera_get_pos_dir(cam), dist);
	gf_vec_add(pos, eff.bbox.center, diff);
	diff = cam->position;
	camera_set_vectors(cam, pos, cam->vp_orientation, cam->fieldOfView);
	cam->position = diff;
	camera_move_to(cam, pos, cam->target, cam->up);
	cam->examine_center = eff.bbox.center;
	cam->flags |= CF_STORE_VP;
	camera_changed(sr, cam);
	gf_mx_v(sr->compositor->mx);
}

static Bool R3D_HandleEvents3D(Render3D *sr, GF_UserEvent *ev)
{
	Fixed x, y, trans_scale;
	Fixed dx, dy, key_trans, key_pan, key_exam;
	s32 key_inv;
	u32 keys;
	Bool is_pixel_metrics;
	GF_Camera *cam;

	if (sr->active_layer) {
		cam = l3d_get_camera(sr->active_layer);
		is_pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(sr->active_layer));
	} else {
		cam = &sr->surface->camera;
		assert(sr->compositor);
		assert(sr->compositor->scene);
		is_pixel_metrics = gf_sg_use_pixel_metrics(sr->compositor->scene);
	}

	if (!cam->navigate_mode) return 0;
	keys = sr->compositor->key_states;
	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->event_type<=GF_EVT_MOUSEWHEEL) {
		x = gf_divfix( INT2FIX(ev->mouse.x + (s32) sr->surface->width/2), INT2FIX(sr->surface->width));
		y = gf_divfix( INT2FIX(ev->mouse.y + (s32) sr->surface->height/2), INT2FIX(sr->surface->height));
	}

	dx = (x - sr->grab_x); 
	dy = (y - sr->grab_y);

	trans_scale = is_pixel_metrics ? cam->width/2 : INT2FIX(10);
	key_trans = is_pixel_metrics ? INT2FIX(10) : cam->avatar_size.x;

	key_pan = FIX_ONE/25;
	key_exam = FIX_ONE/10;
	key_inv = 1;

	if (keys & GF_KM_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_pan *= 4;
		key_exam *= 4;
		key_trans*=4;
	}

	switch (ev->event_type) {
	case GF_EVT_LEFTDOWN:
		sr->grab_x = x;
		sr->grab_y = y;
		sr->nav_is_grabbed = 1;

		/*change vp and examine center to current location*/
		if ((keys & GF_KM_CTRL) && sr->sq_dist) {
			cam->vp_position = cam->position;
			cam->vp_orientation = camera_get_orientation(cam->position, cam->target, cam->up);
			cam->vp_fov = cam->fieldOfView;
			cam->examine_center = sr->hit_info.world_point;
			camera_changed(sr, cam);
			return 1;
		}
		break;
	case GF_EVT_RIGHTDOWN:
		if (sr->nav_is_grabbed && (cam->navigate_mode==GF_NAVIGATE_WALK)) {
			camera_jump(cam);
			gf_sr_invalidate(sr->compositor, NULL);
			return 1;
		}
		else if (keys & GF_KM_CTRL) R3D_FitScene(sr);
		break;

	/* note: shortcuts are mostly the same as blaxxun contact, I don't feel like remembering 2 sets...*/
	case GF_EVT_MOUSEMOVE:
		if (!sr->nav_is_grabbed) {
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				/*init mode*/
				sr->grab_x = x;
				sr->grab_y = y;
				sr->nav_is_grabbed = 1;
			}
			return 0;
		}

		switch (cam->navigate_mode) {
		/*FIXME- we'll likely need a "step" value for walk at some point*/
		case GF_NAVIGATE_WALK:
		case GF_NAVIGATE_FLY:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KM_CTRL) view_pan_y(sr, cam, dy);
			else view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			break;
		case GF_NAVIGATE_VR:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KM_CTRL) view_zoom(sr, cam, dy);
			else view_pan_y(sr, cam, dy);
			break;
		case GF_NAVIGATE_PAN:
			view_pan_x(sr, cam, -dx);
			if (keys & GF_KM_CTRL) view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			else view_pan_y(sr, cam, dy);
			break;
		case GF_NAVIGATE_SLIDE:
			view_translate_x(sr, cam, gf_mulfix(dx, trans_scale));
			if (keys & GF_KM_CTRL) view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
			else view_translate_y(sr, cam, gf_mulfix(dy, trans_scale));
			break;
		case GF_NAVIGATE_EXAMINE:
			if (keys & GF_KM_CTRL) {
				view_translate_z(sr, cam, gf_mulfix(dy, trans_scale));
				view_roll(sr, cam, gf_mulfix(dx, trans_scale));
			} else {
				view_exam_x(sr, cam, -gf_mulfix(GF_PI, dx));
				view_exam_y(sr, cam, gf_mulfix(GF_PI, dy));
			}
			break;
		case GF_NAVIGATE_ORBIT:
			if (keys & GF_KM_CTRL) {
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

	case GF_EVT_MOUSEWHEEL:
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
				view_translate_z(sr, cam, gf_mulfix(trans_scale, ev->mouse.wheel_pos) * ((keys & GF_KM_SHIFT) ? 4 : 1));
			} else {
				view_translate_z(sr, cam, ev->mouse.wheel_pos * ((keys & GF_KM_SHIFT) ? 4 : 1));
			}
			break;
		}
		return 1;

	case GF_EVT_LEFTUP:
		sr->nav_is_grabbed = 0;
		break;

	case GF_EVT_KEYDOWN:
		if (ev->key.virtual_code=='\b') {
			gf_sr_reset_graphics(sr->compositor);
			return 1;
		}
		else if ((ev->key.virtual_code=='c') || (ev->key.virtual_code=='C')) {
			sr->collide_mode = sr->collide_mode  ? GF_COLLISION_NONE : GF_COLLISION_DISPLACEMENT;
			return 1;
		}
		else if ((ev->key.virtual_code=='j') || (ev->key.virtual_code=='J')) {
			if (cam->navigate_mode==GF_NAVIGATE_WALK) {
				camera_jump(cam);
				gf_sr_invalidate(sr->compositor, NULL);
				return 1;
			}
		}
		break;
	case GF_EVT_VKEYDOWN:
		switch (ev->key.vk_code) {
		case GF_VK_HOME:
			if (!sr->nav_is_grabbed) R3D_ResetCamera(sr);
			break;
		case GF_VK_END:
			if (cam->navigate_mode==GF_NAVIGATE_GAME) {
				cam->navigate_mode = GF_NAVIGATE_WALK;
				sr->nav_is_grabbed = 0;
				return 1;
			}
			break;
		case GF_VK_LEFT: key_inv = -1;
		case GF_VK_RIGHT:
			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KM_CTRL) view_pan_x(sr, cam, key_inv * key_pan);
				else view_translate_x(sr, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KM_CTRL) view_roll(sr, cam, gf_mulfix(dx, trans_scale));
				else view_exam_x(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KM_CTRL) view_translate_x(sr, cam, key_inv * key_trans);
				else view_orbit_x(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_GAME: view_translate_x(sr, cam, key_inv * key_trans); break;
			case GF_NAVIGATE_VR: view_pan_x(sr, cam, -key_inv * key_pan); break;
			/*walk/fly/pan*/
			default:
				if (keys & GF_KM_CTRL) view_translate_x(sr, cam, key_inv * key_trans);
				else view_pan_x(sr, cam, -key_inv * key_pan);
				break;
			}
			return 1;
		case GF_VK_DOWN: key_inv = -1;
		case GF_VK_UP:
			if (keys & GF_KM_ALT) return 0;
			switch (cam->navigate_mode) {
			case GF_NAVIGATE_SLIDE:
				if (keys & GF_KM_CTRL) view_translate_z(sr, cam, key_inv * key_trans);
				else view_translate_y(sr, cam, key_inv * key_trans);
				break;
			case GF_NAVIGATE_EXAMINE:
				if (keys & GF_KM_CTRL) view_translate_z(sr, cam, key_inv * key_trans);
				else view_exam_y(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_ORBIT:
				if (keys & GF_KM_CTRL) view_translate_y(sr, cam, key_inv * key_trans);
				else view_orbit_y(sr, cam, -key_inv * key_exam);
				break;
			case GF_NAVIGATE_PAN:
				if (keys & GF_KM_CTRL) view_translate_y(sr, cam, key_inv * key_trans);
				else view_pan_y(sr, cam, key_inv * key_pan);
				break;
			case GF_NAVIGATE_GAME: view_translate_z(sr, cam, key_inv * key_trans); break;
			case GF_NAVIGATE_VR:
				if (keys & GF_KM_CTRL) view_zoom(sr, cam, key_inv * key_pan);
				else view_pan_y(sr, cam, key_inv * key_pan);
				break;
			/*walk/fly*/
			default:
				if (keys & GF_KM_CTRL) view_pan_y(sr, cam, key_inv * key_pan);
				else view_translate_z(sr, cam, key_inv * key_trans);
				break;
			}
			return 1;

		case GF_VK_NEXT:
			if (keys & GF_KM_CTRL) { view_zoom(sr, cam, FIX_ONE/10); return 1; }
			break;
		case GF_VK_PRIOR:
			if (keys & GF_KM_CTRL) { view_zoom(sr, cam, -FIX_ONE/10); return 1; }
			break;
		}
		break;
	}
	return 0;
}

static void VS_SetZoom2D(VisualSurface *surf, Fixed zoom) 
{
	Fixed ratio;

	if (zoom <= 0) zoom = FIX_ONE/1000;
	if (zoom != surf->camera.zoom) {
		ratio = zoom/surf->camera.zoom;
		surf->camera.trans.x = gf_mulfix(surf->camera.trans.x, ratio);
		surf->camera.trans.y = gf_mulfix(surf->camera.trans.y, ratio);
		surf->camera.zoom = zoom;
	}
	camera_changed(surf->render, &surf->camera);
}

/*let's try to keep the same behaviour as the 2D renderer*/
static Bool VS_HandleEvents2D(VisualSurface *surf, GF_UserEvent *ev)
{
	Fixed x, y, dx, dy, key_trans, key_rot;
	s32 key_inv;
	Bool is_pixel_metrics = gf_sg_use_pixel_metrics(surf->render->compositor->scene);
	u32 keys = surf->render->compositor->key_states;
	if (!surf->camera.navigate_mode) return 0;

	x = y = 0;
	/*renorm between -1, 1*/
	if (ev->event_type<=GF_EVT_MOUSEWHEEL) {
		x = INT2FIX(ev->mouse.x);
		y = INT2FIX(ev->mouse.y);
	} else {
		if (!(keys & GF_KM_ALT) && (!surf->camera.navigate_mode || !(surf->camera.navigation_flags & NAV_ANY)) ) return 0;
	}
	dx = x - surf->render->grab_x;
	dy = y - surf->render->grab_y;
	if (!is_pixel_metrics) { dx /= surf->width; dy /= surf->height; }

	key_inv = 1;
	key_trans = INT2FIX(2);
	key_rot = FIX_ONE/10;

	if (keys & GF_KM_SHIFT) {
		dx *= 4;
		dy *= 4;
		key_rot *= 4;
		key_trans*=4;
	}

	if (!is_pixel_metrics) { key_trans /= surf->width;}

	switch (ev->event_type) {
	case GF_EVT_LEFTDOWN:
		surf->render->grab_x = x;
		surf->render->grab_y = y;
		surf->render->nav_is_grabbed = 1;
		return 0;
	case GF_EVT_LEFTUP:
		surf->render->nav_is_grabbed = 0;
		return 0;

	case GF_EVT_MOUSEMOVE:
		if (!surf->render->nav_is_grabbed) return 0;
		switch (surf->camera.navigate_mode) {
		case GF_NAVIGATE_SLIDE:
			if (keys & GF_KM_CTRL) {
				Fixed new_zoom = surf->camera.zoom;
				if (new_zoom > FIX_ONE) new_zoom += dy/20;
				else new_zoom += dy/80;
				VS_SetZoom2D(surf, new_zoom);
			} else {
				surf->camera.trans.x += dx;
				surf->camera.trans.y += dy;
				VS_SetZoom2D(surf, surf->camera.zoom);
			}
			break;
		case GF_NAVIGATE_EXAMINE:
			surf->camera.rot.y += gf_mulfix(GF_PI, dx) / surf->width;
			surf->camera.rot.x += gf_mulfix(GF_PI, dy) / surf->height;
			camera_changed(surf->render, &surf->camera);
			break;
		}
		surf->render->grab_x = x;
		surf->render->grab_y = y;
		return 1;
	case GF_EVT_VKEYDOWN:
		if (ev->key.virtual_code=='\b') gf_sr_reset_graphics(surf->render->compositor);
		switch (ev->key.vk_code) {
		case GF_VK_HOME:
			if (!surf->render->nav_is_grabbed) R3D_ResetCamera(surf->render);
			return 1;
		case GF_VK_LEFT: key_inv = -1;
		case GF_VK_RIGHT:
			if (surf->camera.navigate_mode == GF_NAVIGATE_SLIDE) surf->camera.trans.x += key_inv*key_trans;
			else surf->camera.rot.y -= key_inv * key_rot;
			camera_changed(surf->render, &surf->camera);
			return 1;
		case GF_VK_DOWN: key_inv = -1;
		case GF_VK_UP:
			if (surf->camera.navigate_mode == GF_NAVIGATE_SLIDE) {
				if (keys & GF_KM_CTRL) {
					Fixed new_zoom = surf->camera.zoom;
					if (new_zoom > FIX_ONE) new_zoom += key_inv*FIX_ONE/10;
					else new_zoom += key_inv*FIX_ONE/20;
					VS_SetZoom2D(surf, new_zoom);
				} else {
					surf->camera.trans.y += key_inv*key_trans;
					camera_changed(surf->render, &surf->camera);
				}
			}
			else {
				surf->camera.rot.x += key_rot;
				camera_changed(surf->render, &surf->camera);
			}
			return 1;
		}
		break;
	}
	return 0;
}


Bool R3D_HandleUserEvent(Render3D *sr, GF_UserEvent *ev)
{
	if (sr->root_is_3D || sr->active_layer) {
		return R3D_HandleEvents3D(sr, ev);
	} else {
		return VS_HandleEvents2D(sr->surface, ev);
	}
}

void VS_ViewpointChange(RenderEffect3D *eff, GF_Node *vp, Bool animate_change, Fixed fieldOfView, SFVec3f position, SFRotation orientation, SFVec3f local_center)
{
	Fixed dist;
	SFVec3f d;

	/*update znear&zfar*/
	eff->camera->z_near = eff->camera->avatar_size.x / 20; 
	if (eff->camera->z_near<=0) eff->camera->z_near = FIX_ONE/100;
	eff->camera->z_far = eff->camera->visibility; 
	if (eff->camera->z_far<=0) {
		eff->camera->z_far = INT2FIX(1000);
		/*use the current graph pm settings, NOT the viewpoint one*/
#ifdef GPAC_FIXED_POINT
		if (eff->is_pixel_metrics) eff->camera->z_far = FIX_MAX/40;
#else
		if (eff->is_pixel_metrics) eff->camera->z_far = gf_mulfix(eff->camera->z_far , eff->min_hsize);
#endif
	}

	if (vp) {
		/*now check if vp is in pixel metrics. If not then:
		- either it's in the main scene, there's nothing to do
		- or it's in an inline, and the inline has been scaled if main scene is in pm: nothing to do*/
		if (0 && gf_sg_use_pixel_metrics(gf_node_get_graph(vp))) {
			GF_Matrix mx;
			gf_mx_init(mx);
			gf_mx_add_scale(&mx, eff->min_hsize, eff->min_hsize, eff->min_hsize);
			gf_mx_apply_vec(&mx, &position);
			gf_mx_apply_vec(&mx, &local_center);
		}
	}
	/*default VP setup - this is undocumented in the spec. Default VP pos is (0, 0, 10) but not really nice 
	in pixel metrics. We set z so that we see just the whole surface*/
	else if (eff->is_pixel_metrics) {
		position.z = gf_divfix(eff->camera->width, 2*gf_tan(fieldOfView/2) );
	}

	gf_vec_diff(d, position, local_center);
	dist = gf_vec_len(d);
	if (!dist || (dist<eff->camera->z_near) || (dist > eff->camera->z_far)) {
		if (dist > eff->camera->z_far) eff->camera->z_far = 2*dist;

		dist = 10 * eff->camera->avatar_size.x;
		if ((dist<eff->camera->z_near) || (dist > eff->camera->z_far)) 
			dist = (eff->camera->avatar_size.x + eff->camera->z_far) / 5;
	}
	eff->camera->vp_dist = dist;
	eff->camera->vp_position = position;
	eff->camera->vp_orientation = orientation;
	eff->camera->vp_fov = fieldOfView;
	eff->camera->examine_center = local_center;

	camera_reset_viewpoint(eff->camera, animate_change);
	gf_sr_invalidate(eff->surface->render->compositor, NULL);
}

void R3D_ResetCamera(Render3D *sr)
{
	GF_Camera *cam;

	if (sr->active_layer) {
		cam = l3d_get_camera(sr->active_layer);
	} else {
		cam = &sr->surface->camera;
	}
	camera_reset_viewpoint(cam, 1);
	gf_sr_invalidate(sr->compositor, NULL);
}

