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

#include <gpac/internal/camera.h>

#include <gpac/options.h>


GF_Camera *new_camera()
{
	GF_Camera *tmp;
	GF_SAFEALLOC(tmp, GF_Camera);
	tmp->speed = 1;

	return tmp;
}
void delete_camera(GF_Camera *cam)
{
	if (cam) free(cam);
}

void camera_invalidate(GF_Camera *cam)
{
	/*forces recompute of default viewpoint*/
	cam->had_viewpoint = 2;
	cam->had_nav_info = 1;
	cam->flags = CAM_IS_DIRTY;
	cam->navigate_mode = GF_NAVIGATE_NONE;
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
		vx /= nor; vy /= nor; vz /= nor;
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
	axis.x = dir.y; axis.y = -dir.x; axis.z = 0;

	if (gf_vec_dot(axis, axis) < FIX_EPSILON) {
		if (dir.z> 0) {
			norm.x = 0; norm.y = FIX_ONE; norm.z = 0; norm.q = 0;
		} else {
			norm.x = 0; norm.y = 0; norm.z = 0; norm.q = FIX_ONE;
		}
	} else {
		gf_vec_norm(&axis);
		norm = gf_quat_from_axis_cos(axis, -dir.z);
	}
	/* Find the inverse rotation. */
    inv_norm.x = -norm.x; inv_norm.y = -norm.y; inv_norm.z = -norm.z; inv_norm.q = norm.q;
	/* Rotate the y. */
	y_quat.x = y_quat.z = y_quat.q = 0; y_quat.y = FIX_ONE;
	ny_quat = gf_quat_multiply(&norm, &y_quat);
	ny_quat = gf_quat_multiply(&ny_quat, &inv_norm);

	new_y.x = ny_quat.x; new_y.y = ny_quat.y; new_y.z = ny_quat.z;

	tmp = gf_vec_cross(new_y, v);

	if (gf_vec_dot(tmp, tmp) < FIX_EPSILON) {
		/* The old and new may be pointing in the same or opposite. Need
		** to generate a vector perpendicular to the old or new y.
		*/
		tmp.x = 0; tmp.y = -v.z; tmp.z = v.y;
		if (gf_vec_dot(tmp, tmp) < FIX_EPSILON) {
			tmp.x = v.z; tmp.y = 0; tmp.z = -v.x;
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

void camera_update(GF_Camera *cam, GF_Matrix2D *user_transform, Bool center_coords)
{
	Fixed vlen, h, w, ar;
	SFVec3f corner, center;

	if (! (cam->flags & CAM_IS_DIRTY)) return;

	ar = gf_divfix(cam->width, cam->height);
	if (cam->is_3D) {
		/*setup perspective*/
		gf_mx_perspective(&cam->projection, cam->fieldOfView, ar, cam->z_near, cam->z_far);
		/*setup modelview*/
		gf_mx_lookat(&cam->modelview, cam->position, cam->target, cam->up);

		if (!center_coords) {
			gf_mx_add_scale(&cam->modelview, 1, -1, 1);
			gf_mx_add_translation(&cam->modelview, -cam->width / 2, -cam->height / 2, 0);
		}

		/*compute center and radius - CHECK ME!*/
		vlen = cam->z_far - cam->z_near;
		h = gf_mulfix(vlen , gf_tan(cam->fieldOfView / 2));
		w = gf_mulfix(h, ar);
		center.x = 0; center.y = 0; center.z = cam->z_near + vlen / 2;
		corner.x = w; corner.y = h; corner.z = vlen;
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
		cam->z_near = -INT2FIX(512);
		cam->z_far = INT2FIX(512);

		/*setup ortho*/
		gf_mx_ortho(&cam->projection, -hw, hw, -hh, hh, cam->z_near, cam->z_far);
		/*setup modelview*/
		gf_mx_init(cam->modelview);
		if (!center_coords) {
			gf_mx_add_scale(&cam->modelview, 1, -1, 1);
			gf_mx_add_translation(&cam->modelview, -hw, -hh, 0);
		}
		if (user_transform) gf_mx_add_matrix_2d(&cam->modelview, user_transform);
		if (cam->flags & CAM_HAS_VIEWPORT) gf_mx_add_matrix(&cam->modelview, &cam->viewport);

		/*compute center & radius*/
		b.max_edge.x = hw;
		b.max_edge.y = hh;
		b.min_edge.x = -hw;
		b.min_edge.y = -hh;
		b.min_edge.z = b.max_edge.z = (cam->z_near+cam->z_far) / 2;
		gf_bbox_refresh(&b);
		cam->center = b.center;
		cam->radius = b.radius;
	}
	/*compute frustum planes*/
	gf_mx_copy(cam->unprojection, cam->projection);
	gf_mx_add_matrix_4x4(&cam->unprojection, &cam->modelview);
	camera_frustum_from_matrix(cam, &cam->unprojection);
	/*also compute reverse PM for unprojections*/
	gf_mx_inverse_4x4(&cam->unprojection);
	cam->flags &= ~CAM_IS_DIRTY;
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
		return;
	}
	if (cam->is_3D) {
		cam->start_pos = cam->position;
		cam->start_ori = camera_get_orientation(cam->position, cam->target, cam->up);
		cam->start_fov = cam->fieldOfView;
		cam->end_pos = cam->vp_position;
		cam->end_ori = cam->vp_orientation;
		cam->end_fov = cam->vp_fov;

		cam->flags |= CAM_IS_DIRTY;
		cam->anim_start = 0;
		cam->anim_len = 1000;
	} else {
		cam->start_zoom = FIX_ONE;
		cam->start_trans.x = cam->start_trans.y = 0;
		cam->start_rot.x = cam->start_rot.y = 0;
		cam->flags |= CAM_IS_DIRTY;
		/*no animation on 3D viewports*/
		cam->anim_start = cam->anim_len = 0;
	}
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
	cam->jumping = 1;
	cam->flags |= CAM_IS_DIRTY;
}


Bool camera_animate(GF_Camera *cam)
{
	u32 now;
	Fixed frac;
	if (!cam->anim_len) return 0;

	if (cam->jumping) {
		if (!cam->anim_start) {
			cam->anim_start = gf_sys_clock();
			cam->dheight = 0;
			return 1;
		}
		cam->position.y -= cam->dheight;
		cam->target.y -= cam->dheight;

		now = gf_sys_clock() - cam->anim_start;
		if (now > cam->anim_len) {
			cam->anim_len = 0;
			cam->jumping = 0;
			cam->flags |= CAM_IS_DIRTY;
			return 1;
		}
		frac = FLT2FIX ( ((Float) now) / cam->anim_len);
		if (frac>FIX_ONE / 2) frac = FIX_ONE - frac;
		cam->dheight = gf_mulfix(cam->avatar_size.y, frac);
		cam->position.y += cam->dheight;
		cam->target.y += cam->dheight;
		cam->flags |= CAM_IS_DIRTY;
		return 1;
	}

	if (!cam->anim_start) {
		cam->anim_start = gf_sys_clock();
		now = 0;
		frac = 0;
	} else {
		now = gf_sys_clock() - cam->anim_start;
		if (now > cam->anim_len) {
			cam->anim_len = 0;
			if (cam->is_3D) {
				camera_set_vectors(cam, cam->end_pos, cam->end_ori, cam->end_fov);
			} else {
				cam->flags |= CAM_IS_DIRTY;
			}
			if (cam->flags & CF_STORE_VP) {
				cam->flags &= ~CF_STORE_VP;
				cam->vp_position = cam->position;
				cam->vp_fov = cam->fieldOfView;
				cam->vp_orientation = camera_get_orientation(cam->position, cam->target, cam->up);
			}
			return 1;
		} else {
			frac = FLT2FIX( ((Float) now) / cam->anim_len);
		}
	}

	if (cam->is_3D) {
		SFVec3f pos, dif;
		SFRotation rot;
		Fixed fov;
		rot = gf_sg_sfrotation_interpolate(cam->start_ori, cam->end_ori, frac);
		gf_vec_diff(dif, cam->end_pos, cam->start_pos);
		dif = gf_vec_scale(dif, frac);
		gf_vec_add(pos, cam->start_pos, dif);
		fov = gf_mulfix(cam->end_fov - cam->start_fov, frac) + cam->start_fov;
		camera_set_vectors(cam, pos, rot, fov);
	}
	return 1;
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


