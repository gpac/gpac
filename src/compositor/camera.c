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

#include <gpac/internal/camera.h>
#ifndef GPAC_DISABLE_COMPOSITOR

#include "visual_manager.h"

#define FORCE_CAMERA_3D

void camera_invalidate(GF_Camera *cam)
{
	/*forces recompute of default viewpoint*/
	cam->had_viewpoint = 2;
	cam->had_nav_info = GF_TRUE;
	cam->flags = CAM_IS_DIRTY;
//	cam->navigate_mode = GF_NAVIGATE_NONE;
}

static void camera_frustum_from_matrix(GF_Camera *cam, GF_Matrix *mx)
{
	u32 i;

	cam->planes[FRUS_LEFT_PLANE].normal.x = mx->m[3] + mx->m[0];
	cam->planes[FRUS_LEFT_PLANE].normal.y = mx->m[7] + mx->m[4];
	cam->planes[FRUS_LEFT_PLANE].normal.z = mx->m[11] + mx->m[8];
	cam->planes[FRUS_LEFT_PLANE].d = mx->m[15] + mx->m[12];

	cam->planes[FRUS_RIGHT_PLANE].normal.x = mx->m[3] - mx->m[0];
	cam->planes[FRUS_RIGHT_PLANE].normal.y = mx->m[7] - mx->m[4];
	cam->planes[FRUS_RIGHT_PLANE].normal.z = mx->m[11] - mx->m[8];
	cam->planes[FRUS_RIGHT_PLANE].d = mx->m[15] - mx->m[12];

	cam->planes[FRUS_BOTTOM_PLANE].normal.x = mx->m[3] + mx->m[1];
	cam->planes[FRUS_BOTTOM_PLANE].normal.y = mx->m[7] + mx->m[5];
	cam->planes[FRUS_BOTTOM_PLANE].normal.z = mx->m[11] + mx->m[9];
	cam->planes[FRUS_BOTTOM_PLANE].d = mx->m[15] + mx->m[13];

	cam->planes[FRUS_TOP_PLANE].normal.x = mx->m[3] - mx->m[1];
	cam->planes[FRUS_TOP_PLANE].normal.y = mx->m[7] - mx->m[5];
	cam->planes[FRUS_TOP_PLANE].normal.z = mx->m[11] - mx->m[9];
	cam->planes[FRUS_TOP_PLANE].d = mx->m[15] - mx->m[13];

	cam->planes[FRUS_FAR_PLANE].normal.x = mx->m[3] - mx->m[2];
	cam->planes[FRUS_FAR_PLANE].normal.y = mx->m[7] - mx->m[6];
	cam->planes[FRUS_FAR_PLANE].normal.z = mx->m[11] - mx->m[10];
	cam->planes[FRUS_FAR_PLANE].d = mx->m[15] - mx->m[14];

	cam->planes[FRUS_NEAR_PLANE].normal.x = mx->m[3] + mx->m[2];
	cam->planes[FRUS_NEAR_PLANE].normal.y = mx->m[7] + mx->m[6];
	cam->planes[FRUS_NEAR_PLANE].normal.z = mx->m[11] + mx->m[10];
	cam->planes[FRUS_NEAR_PLANE].d = mx->m[15] + mx->m[14];

	for (i=0; i<6; ++i) {
#ifdef GPAC_FIXED_POINT
		/*after some testing, it's just safer to move back to float here, the smallest drift will
		result in completely wrong culling...*/
		Float vx, vy, vz, nor;
		vx = FIX2FLT(cam->planes[i].normal.x);
		vy = FIX2FLT(cam->planes[i].normal.y);
		vz = FIX2FLT(cam->planes[i].normal.z);
		nor = (Float) sqrt(vx*vx + vy*vy + vz*vz);
		vx /= nor;
		vy /= nor;
		vz /= nor;
		cam->planes[i].d = FLT2FIX (FIX2FLT(cam->planes[i].d) / nor);
		cam->planes[i].normal.x = FLT2FIX(vx);
		cam->planes[i].normal.y = FLT2FIX(vy);
		cam->planes[i].normal.z = FLT2FIX(vz);
#else
		Float len = (Float)(1.0f / gf_vec_len(cam->planes[i].normal));
		cam->planes[i].normal = gf_vec_scale(cam->planes[i].normal, len);
		cam->planes[i].d *= len;
#endif

		/*compute p-vertex idx*/
		cam->p_idx[i] = gf_plane_get_p_vertex_idx(&cam->planes[i]);
	}
}

/*based on  Copyright (C) 1995  Stephen Chenney (schenney@cs.berkeley.edu)*/
SFRotation camera_get_orientation(SFVec3f pos, SFVec3f target, SFVec3f up)
{
	SFVec3f dir, tmp, v, axis, new_y;
	SFVec4f norm, inv_norm, y_quat, ny_quat, rot_y, rot;

	gf_vec_diff(dir, target, pos);
	gf_vec_norm(&dir);
	tmp = gf_vec_scale(dir, gf_vec_dot(up, dir));
	gf_vec_diff(v, up, tmp);
	gf_vec_norm(&v);
	axis.x = dir.y;
	axis.y = -dir.x;
	axis.z = 0;

	if (gf_vec_dot(axis, axis) < FIX_EPSILON) {
		if (dir.z> 0) {
			norm.x = 0;
			norm.y = FIX_ONE;
			norm.z = 0;
			norm.q = 0;
		} else {
			norm.x = 0;
			norm.y = 0;
			norm.z = 0;
			norm.q = FIX_ONE;
		}
	} else {
		gf_vec_norm(&axis);
		norm = gf_quat_from_axis_cos(axis, -dir.z);
	}
	/* Find the inverse rotation. */
	inv_norm.x = -norm.x;
	inv_norm.y = -norm.y;
	inv_norm.z = -norm.z;
	inv_norm.q = norm.q;
	/* Rotate the y. */
	y_quat.x = y_quat.z = y_quat.q = 0;
	y_quat.y = FIX_ONE;
	ny_quat = gf_quat_multiply(&norm, &y_quat);
	ny_quat = gf_quat_multiply(&ny_quat, &inv_norm);

	new_y.x = ny_quat.x;
	new_y.y = ny_quat.y;
	new_y.z = ny_quat.z;

	tmp = gf_vec_cross(new_y, v);

	if (gf_vec_dot(tmp, tmp) < FIX_EPSILON) {
		/* The old and new may be pointing in the same or opposite. Need
		** to generate a vector perpendicular to the old or new y.
		*/
		tmp.x = 0;
		tmp.y = -v.z;
		tmp.z = v.y;
		if (gf_vec_dot(tmp, tmp) < FIX_EPSILON) {
			tmp.x = v.z;
			tmp.y = 0;
			tmp.z = -v.x;
		}
	}
	gf_vec_norm(&tmp);

	rot_y = gf_quat_from_axis_cos(tmp, gf_vec_dot(new_y, v));

	/* rot_y holds the rotation about the initial camera direction needed
	** to align the up vectors in the final position.
	*/

	/* Put the 2 rotations together. */
	rot = gf_quat_multiply(&rot_y, &norm);
	return gf_quat_to_rotation(&rot);
}

#define FAR_PLANE_2D	-10000
#define NEAR_PLANE_2D	1000

void camera_set_2d(GF_Camera *cam)
{
	cam->is_3D = GF_FALSE;
#ifdef FORCE_CAMERA_3D
	cam->position.x = cam->position.y = 0;
	cam->position.z = INT2FIX(NEAR_PLANE_2D);
	cam->up.x = cam->up.z = 0;
	cam->up.y = FIX_ONE;
	cam->target.x = cam->target.y = cam->target.z = 0;
	cam->vp_position = cam->position;
	cam->vp_orientation.x = cam->vp_orientation.y = 0;
	cam->vp_orientation.q = 0;
	cam->vp_orientation.y = FIX_ONE;
	cam->vp_fov = cam->fieldOfView = FLT2FIX(0.785398);
	cam->vp_dist = INT2FIX(NEAR_PLANE_2D);
	cam->end_zoom = FIX_ONE;
#endif
}

void camera_update_stereo(GF_Camera *cam, GF_Matrix2D *user_transform, Bool center_coords, Fixed horizontal_shift, Fixed nominal_view_distance, Fixed view_distance_offset, u32 camlay)
{
	Fixed vlen, h, w, ar;
	SFVec3f corner, center;
	GF_Matrix post_model_view;

	if (! (cam->flags & CAM_IS_DIRTY)) return;

	ar = gf_divfix(cam->width, cam->height);
	gf_mx_init(post_model_view);

	if (cam->is_3D) {
		if (!cam->z_far) {
			cam->z_near = FIX_ONE / 100;
			cam->z_far = FIX_ONE * 100;
		}
		/*setup perspective*/
		if (camlay==GF_3D_CAMERA_OFFAXIS) {
			Fixed left, right, top, bottom, shift, wd2, ndfl, viewing_distance;
			SFVec3f eye, pos, tar, disp;

			viewing_distance = nominal_view_distance;

			wd2 = gf_mulfix(cam->z_near, gf_tan(cam->fieldOfView/2));
			ndfl = gf_divfix(cam->z_near, viewing_distance);
			/*compute h displacement*/
			shift = gf_mulfix(horizontal_shift, ndfl);

			top = wd2;
			bottom = -top;
			left = -gf_mulfix(ar, wd2) - shift;
			right = gf_mulfix(ar, wd2) - shift;

			gf_mx_init(cam->projection);
			cam->projection.m[0] = gf_divfix(2*cam->z_near, (right-left));
			cam->projection.m[5] = gf_divfix(2*cam->z_near, (top-bottom));
			cam->projection.m[8] = gf_divfix(right+left, right-left);
			cam->projection.m[9] = gf_divfix(top+bottom, top-bottom);
			cam->projection.m[10] = gf_divfix(cam->z_far+cam->z_near, cam->z_near-cam->z_far);
			cam->projection.m[11] = -FIX_ONE;
			cam->projection.m[14] = 2*gf_muldiv(cam->z_near, cam->z_far, cam->z_near-cam->z_far);
			cam->projection.m[15] = 0;

			gf_vec_diff(eye, cam->target, cam->position);
			gf_vec_norm(&eye);
			disp = gf_vec_cross(eye, cam->up);
			gf_vec_norm(&disp);

			gf_vec_diff(center, cam->world_bbox.center, cam->position);
			vlen = gf_vec_len(center);
			shift = gf_mulfix(horizontal_shift, gf_divfix(vlen, viewing_distance));

			pos = gf_vec_scale(disp, shift);
			gf_vec_add(pos, pos, cam->position);
			gf_vec_add(tar, pos, eye);

			/*setup modelview*/
			gf_mx_lookat(&cam->modelview, pos, tar, cam->up);
		} else {
			gf_mx_perspective(&cam->projection, cam->fieldOfView, ar, cam->z_near, cam->z_far);

			/*setup modelview*/
			gf_mx_lookat(&cam->modelview, cam->position, cam->target, cam->up);
		}

		if (!center_coords) {
			gf_mx_add_scale(&post_model_view, FIX_ONE, -FIX_ONE, FIX_ONE);
			gf_mx_add_translation(&post_model_view, -cam->width / 2, -cam->height / 2, 0);
		}

		/*compute center and radius - CHECK ME!*/
		vlen = cam->z_far - cam->z_near;
		h = gf_mulfix(vlen , gf_tan(cam->fieldOfView / 2));
		w = gf_mulfix(h, ar);
		center.x = 0;
		center.y = 0;
		center.z = cam->z_near + vlen / 2;
		corner.x = w;
		corner.y = h;
		corner.z = vlen;
		gf_vec_diff(corner, corner, center);
		cam->radius = gf_vec_len(corner);
		gf_vec_diff(cam->center, cam->target, cam->position);
		gf_vec_norm(&cam->center);
		cam->center = gf_vec_scale(cam->center, cam->z_near + vlen/2);
		gf_vec_add(cam->center, cam->center, cam->position);
	} else {
		GF_BBox b;
		Fixed hw, hh;
		hw = cam->width / 2;
		hh = cam->height / 2;
		cam->z_near = INT2FIX(NEAR_PLANE_2D);
		cam->z_far = INT2FIX(FAR_PLANE_2D);

		/*setup ortho*/
		gf_mx_ortho(&cam->projection, -hw, hw, -hh, hh, cam->z_near, cam->z_far);

		/*setup modelview*/
		gf_mx_init(cam->modelview);
#ifdef FORCE_CAMERA_3D
		if (! (cam->flags & CAM_NO_LOOKAT))
			gf_mx_lookat(&cam->modelview, cam->position, cam->target, cam->up);
#endif

		if (!center_coords) {
			gf_mx_add_scale(&post_model_view, FIX_ONE, -FIX_ONE, FIX_ONE);
			gf_mx_add_translation(&post_model_view, -hw, -hh, 0);
		}
		if (user_transform) {
#ifdef FORCE_CAMERA_3D
			if (! (cam->flags & CAM_NO_LOOKAT)) {
				GF_Matrix mx;
				gf_mx_from_mx2d(&mx, user_transform);
				mx.m[10] = mx.m[0];
				gf_mx_add_matrix(&post_model_view, &mx);
			} else
#endif
				gf_mx_add_matrix_2d(&post_model_view, user_transform);
		}
		if (cam->end_zoom != FIX_ONE) gf_mx_add_scale(&post_model_view, cam->end_zoom, cam->end_zoom, cam->end_zoom);
		if (cam->flags & CAM_HAS_VIEWPORT) gf_mx_add_matrix(&post_model_view, &cam->viewport);

		/*compute center & radius*/
		b.max_edge.x = hw;
		b.max_edge.y = hh;
		b.min_edge.x = -hw;
		b.min_edge.y = -hh;
		b.min_edge.z = b.max_edge.z = (cam->z_near+cam->z_far) / 2;
		gf_bbox_refresh(&b);
		cam->center = b.center;
		cam->radius = b.radius;

		if (camlay==GF_3D_CAMERA_OFFAXIS)
			camlay=GF_3D_CAMERA_LINEAR;
	}

	if (camlay == GF_3D_CAMERA_CIRCULAR) {
		GF_Matrix mx;
		Fixed viewing_distance = nominal_view_distance;
		SFVec3f pos, target;
		Fixed angle;

		gf_vec_diff(center, cam->world_bbox.center, cam->position);
		vlen = gf_vec_len(center);
		vlen += gf_mulfix(view_distance_offset, gf_divfix(vlen, nominal_view_distance));

		gf_vec_diff(pos, cam->target, cam->position);
		gf_vec_norm(&pos);
		pos = gf_vec_scale(pos, vlen);
		gf_vec_add(target, pos, cam->position);

		gf_mx_init(mx);
		gf_mx_add_translation(&mx, target.x, target.y, target.z);
		angle = gf_atan2(horizontal_shift, viewing_distance);
		gf_mx_add_rotation(&mx, angle, cam->up.x, cam->up.y, cam->up.z);
		gf_mx_add_translation(&mx, -target.x, -target.y, -target.z);

		pos = cam->position;
		gf_mx_apply_vec(&mx, &pos);

		gf_mx_lookat(&cam->modelview, pos, target, cam->up);
	} else if (camlay == GF_3D_CAMERA_LINEAR) {
		Fixed viewing_distance = nominal_view_distance + view_distance_offset;
		GF_Vec eye, disp, pos, tar;

		gf_vec_diff(center, cam->world_bbox.center, cam->position);
		vlen = gf_vec_len(center);
		vlen += gf_mulfix(view_distance_offset, gf_divfix(vlen, nominal_view_distance));

		gf_vec_diff(eye, cam->target, cam->position);
		gf_vec_norm(&eye);
		tar = gf_vec_scale(eye, vlen);
		gf_vec_add(tar, tar, cam->position);

		disp = gf_vec_cross(eye, cam->up);
		gf_vec_norm(&disp);

		disp= gf_vec_scale(disp, gf_divfix(gf_mulfix(vlen, horizontal_shift), viewing_distance));
		gf_vec_add(pos, cam->position, disp);

		gf_mx_lookat(&cam->modelview, pos, tar, cam->up);
	}
	gf_mx_add_matrix(&cam->modelview, &post_model_view);

	/*compute frustum planes*/
	gf_mx_copy(cam->unprojection, cam->projection);
	gf_mx_add_matrix_4x4(&cam->unprojection, &cam->modelview);
	camera_frustum_from_matrix(cam, &cam->unprojection);
	/*also compute reverse PM for unprojections*/
	gf_mx_inverse_4x4(&cam->unprojection);
	cam->flags &= ~CAM_IS_DIRTY;
}

void camera_update(GF_Camera *cam, GF_Matrix2D *user_transform, Bool center_coords)
{
	camera_update_stereo(cam, user_transform, center_coords, 0, 0, 0, GF_3D_CAMERA_STRAIGHT);
}
void camera_set_vectors(GF_Camera *cam, SFVec3f pos, SFRotation ori, Fixed fov)
{
	Fixed sin_a, cos_a, icos_a, tmp;

	cam->fieldOfView = fov;
	cam->last_pos = cam->position;
	cam->position = pos;
	/*compute up & target vectors in local system*/
	sin_a = gf_sin(ori.q);
	cos_a = gf_cos(ori.q);
	icos_a = FIX_ONE - cos_a;
	tmp = gf_mulfix(icos_a, ori.z);
	cam->target.x = gf_mulfix(ori.x, tmp) + gf_mulfix(sin_a, ori.y);
	cam->target.y = gf_mulfix(ori.y, tmp) - gf_mulfix(sin_a, ori.x);
	cam->target.z = gf_mulfix(ori.z, tmp) + cos_a;
	gf_vec_norm(&cam->target);
	cam->target = gf_vec_scale(cam->target, -cam->vp_dist);
	gf_vec_add(cam->target, cam->target, pos);
	tmp = gf_mulfix(icos_a, ori.y);
	cam->up.x = gf_mulfix(ori.x, tmp) - gf_mulfix(sin_a, ori.z);
	cam->up.y = gf_mulfix(ori.y, tmp) + cos_a;
	cam->up.z = gf_mulfix(ori.z, tmp) + gf_mulfix(sin_a, ori.x);
	gf_vec_norm(&cam->up);
	cam->flags |= CAM_IS_DIRTY;
}

void camera_reset_viewpoint(GF_Camera *cam, Bool animate)
{
	if (!animate || (cam->had_viewpoint==2) ) {
		camera_set_vectors(cam, cam->vp_position, cam->vp_orientation, cam->vp_fov);
		cam->last_pos = cam->vp_position;
		cam->anim_len = 0;
		return;
	}
#ifndef FORCE_CAMERA_3D
	if (cam->is_3D)
#endif
	{
		cam->start_pos = cam->position;
		cam->start_ori = camera_get_orientation(cam->position, cam->target, cam->up);
		cam->start_fov = cam->fieldOfView;
		cam->end_pos = cam->vp_position;
		cam->end_ori = cam->vp_orientation;
		cam->end_fov = cam->vp_fov;

		cam->flags |= CAM_IS_DIRTY;
		cam->anim_start = 0;
		cam->anim_len = 1000;
	}
#ifndef FORCE_CAMERA_3D
	else {
		cam->start_zoom = FIX_ONE;
		cam->start_trans.x = cam->start_trans.y = 0;
		cam->start_rot.x = cam->start_rot.y = 0;
		cam->flags |= CAM_IS_DIRTY;
		/*no animation on 3D viewports*/
		cam->anim_start = cam->anim_len = 0;
	}
#endif
}

void camera_move_to(GF_Camera *cam, SFVec3f pos, SFVec3f target, SFVec3f up)
{
	if (!cam->anim_len) {
		cam->start_pos = cam->position;
		cam->start_ori = camera_get_orientation(cam->position, cam->target, cam->up);
		cam->start_fov = cam->fieldOfView;
	}
	cam->end_pos = pos;
	cam->end_ori = camera_get_orientation(pos, target, up);
	cam->end_fov = cam->fieldOfView;

	cam->flags |= CAM_IS_DIRTY;
	cam->anim_start = 0;
	cam->anim_len = 100;
}

void camera_stop_anim(GF_Camera *cam)
{
	cam->anim_len = 0;
}

void camera_jump(GF_Camera *cam)
{
	/*no "double-jump" :)*/
	if (cam->jumping) return;
	cam->anim_start = 0;
	cam->anim_len = 1000;
	cam->jumping = GF_TRUE;
	cam->flags |= CAM_IS_DIRTY;
}


Bool camera_animate(GF_Camera *cam, void *_compositor)
{
	u32 now;
	Fixed frac;
	GF_Compositor *compositor = (GF_Compositor *) _compositor;
	if (!cam->anim_len) return GF_FALSE;

	now = gf_sc_get_clock(compositor);

	if (cam->jumping) {
		if (!cam->anim_start) {
			cam->anim_start = now;
			cam->dheight = 0;
			return GF_TRUE;
		}
		cam->position.y -= cam->dheight;
		cam->target.y -= cam->dheight;

		now -= cam->anim_start;
		if (now > cam->anim_len) {
			cam->anim_len = 0;
			cam->jumping = GF_FALSE;
			cam->flags |= CAM_IS_DIRTY;
			return GF_TRUE;
		}
		frac = FLT2FIX ( ((Float) now) / cam->anim_len);
		if (frac>FIX_ONE / 2) frac = FIX_ONE - frac;
		cam->dheight = gf_mulfix(cam->avatar_size.y, frac);
		cam->position.y += cam->dheight;
		cam->target.y += cam->dheight;
		cam->flags |= CAM_IS_DIRTY;
		return GF_TRUE;
	}

	if (!cam->anim_start) {
		cam->anim_start = now;
		frac = 0;
	} else {
		now -= cam->anim_start;
		if (now > cam->anim_len) {
			cam->anim_len = 0;
#ifndef FORCE_CAMERA_3D
			if (cam->is_3D)
#endif
			{
				camera_set_vectors(cam, cam->end_pos, cam->end_ori, cam->end_fov);
				cam->end_zoom = FIX_ONE;
			}
#ifndef FORCE_CAMERA_3D
			else {
				cam->flags |= CAM_IS_DIRTY;
			}
#endif

			if (cam->flags & CF_STORE_VP) {
				cam->flags &= ~CF_STORE_VP;
				cam->vp_position = cam->position;
				cam->vp_fov = cam->fieldOfView;
				cam->vp_orientation = camera_get_orientation(cam->position, cam->target, cam->up);
			}
			return GF_TRUE;
		} else {
			frac = FLT2FIX( ((Float) now) / cam->anim_len);
		}
	}

#ifndef FORCE_CAMERA_3D
	if (cam->is_3D)
#endif
	{
		SFVec3f pos, dif;
		SFRotation rot;
		Fixed fov;
		rot = gf_sg_sfrotation_interpolate(cam->start_ori, cam->end_ori, frac);
		gf_vec_diff(dif, cam->end_pos, cam->start_pos);
		dif = gf_vec_scale(dif, frac);
		gf_vec_add(pos, cam->start_pos, dif);
		fov = gf_mulfix(cam->end_fov - cam->start_fov, frac) + cam->start_fov;
		cam->end_zoom = frac + gf_mulfix((FIX_ONE-frac), cam->start_zoom);
		camera_set_vectors(cam, pos, rot, fov);
	}
	return GF_TRUE;
}


SFVec3f camera_get_pos_dir(GF_Camera *cam)
{
	SFVec3f v;
	gf_vec_diff(v, cam->position, cam->target);
	gf_vec_norm(&v);
	return v;
}
SFVec3f camera_get_target_dir(GF_Camera *cam)
{
	SFVec3f v;
	gf_vec_diff(v, cam->target, cam->position);
	gf_vec_norm(&v);
	return v;
}
SFVec3f camera_get_right_dir(GF_Camera *cam)
{
	SFVec3f v, pos;
	pos = camera_get_pos_dir(cam);
	v = gf_vec_cross(cam->up, pos );
	gf_vec_norm(&v);
	return v;
}


#endif

