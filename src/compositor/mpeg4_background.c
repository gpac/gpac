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

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_COMPOSITOR)

#include "texturing.h"
#include "visual_manager.h"

/*for background textures - the offset is to prevent lines on cube map edges*/
#define PLANE_HSIZE		FLT2FIX(0.5025f)
#define PLANE_HSIZE_LOW		FLT2FIX(0.5f)


static Bool back_use_texture(MFURL *url)
{
	if (!url->count) return 0;
	if (url->vals[0].OD_ID > 0) return 1;
	if (url->vals[0].url && strlen(url->vals[0].url)) return 1;
	return 0;
}

static void UpdateBackgroundTexture(GF_TextureHandler *txh)
{
	gf_sc_texture_update_frame(txh, 0);
	/*restart texture if needed (movie background controled by MediaControl)*/
	if (txh->stream_finished && gf_mo_get_loop(txh->stream, 0)) gf_sc_texture_restart(txh);
}


static Bool back_gf_sc_texture_enabled(MFURL *url, GF_TextureHandler *txh)
{
	Bool use_texture = back_use_texture(url);
	if (use_texture) {
		/*texture not ready*/
		if (!txh->tx_io) {
			use_texture = 0;
			gf_sc_invalidate(txh->compositor, NULL);
		}
		gf_sc_texture_set_blend_mode(txh, gf_sc_texture_is_transparent(txh) ? TX_REPLACE : TX_DECAL);
	}
	return use_texture;
}

static void back_check_gf_sc_texture_change(GF_TextureHandler *txh, MFURL *url)
{
	/*if open and changed, stop and play*/
	if (txh->is_open) {
		if (! gf_sc_texture_check_url_change(txh, url)) return;
		gf_sc_texture_stop(txh);
		gf_sc_texture_play(txh, url);
		return;
	}
	/*if not open and changed play*/
	if (url->count) gf_sc_texture_play(txh, url);
}

static void DestroyBackground(GF_Node *node)
{
	BackgroundStack *ptr = (BackgroundStack *) gf_node_get_private(node);
	PreDestroyBindable(node, ptr->reg_stacks);
	gf_list_del(ptr->reg_stacks);

	if (ptr->sky_mesh) mesh_free(ptr->sky_mesh);
	if (ptr->ground_mesh) mesh_free(ptr->ground_mesh);

	gf_sg_vrml_mf_reset(&ptr->ground_ang, GF_SG_VRML_MFFLOAT);
	gf_sg_vrml_mf_reset(&ptr->sky_ang, GF_SG_VRML_MFFLOAT);
	gf_sg_vrml_mf_reset(&ptr->ground_col, GF_SG_VRML_MFCOLOR);
	gf_sg_vrml_mf_reset(&ptr->sky_col, GF_SG_VRML_MFCOLOR);

	mesh_free(ptr->front_mesh);
	mesh_free(ptr->back_mesh);
	mesh_free(ptr->top_mesh);
	mesh_free(ptr->bottom_mesh);
	mesh_free(ptr->left_mesh);
	mesh_free(ptr->right_mesh);


	gf_sc_texture_destroy(&ptr->txh_front);
	gf_sc_texture_destroy(&ptr->txh_back);
	gf_sc_texture_destroy(&ptr->txh_top);
	gf_sc_texture_destroy(&ptr->txh_bottom);
	gf_sc_texture_destroy(&ptr->txh_left);
	gf_sc_texture_destroy(&ptr->txh_right);

	gf_free(ptr);
}

#define COL_TO_RGBA(res, col) { res.red = col.red; res.green = col.green; res.blue = col.blue; res.alpha = FIX_ONE; }

#define DOME_STEP_V	32
#define DOME_STEP_H	16

static void back_build_dome(GF_Mesh *mesh, MFFloat *angles, MFColor *color, Bool ground_dome)
{
	u32 i, j, last_idx, ang_idx, new_idx;
	Bool pad;
	u32 step_div_h;
	GF_Vertex vx;
	SFColorRGBA start_col, end_col, fcol;
	Fixed start_angle, next_angle, angle, r, frac, first_angle;

	start_angle = 0;
	mesh_reset(mesh);

	start_col.red = start_col.green = start_col.blue = 0;
	start_col.alpha = FIX_ONE;
	end_col = start_col;

	if (color->count) {
		COL_TO_RGBA(start_col, color->vals[0]);
		end_col = start_col;
		if (color->count>1) COL_TO_RGBA(end_col, color->vals[1]);
	}

	vx.texcoords.x = vx.texcoords.y = 0;
	vx.color = MESH_MAKE_COL(start_col);
	vx.pos.x = vx.pos.z = 0;
	vx.pos.y = FIX_ONE;
	vx.normal.x = vx.normal.z = 0;
	vx.normal.y = -MESH_NORMAL_UNIT;

	mesh_set_vertex_vx(mesh, &vx);
	last_idx = 0;
	ang_idx = 0;

	pad = 1;
	next_angle = first_angle = 0;
	if (angles->count) {
		next_angle = angles->vals[0];
		first_angle = 7*next_angle/8;
		pad = 0;
	}

	step_div_h = DOME_STEP_H;
	i=0;
	if (ground_dome) {
		step_div_h *= 2;
		i=1;
	}

	for (; i<DOME_STEP_V; i++) {
		if (ground_dome) {
			angle = first_angle + (i * (GF_PI2-first_angle) / DOME_STEP_V);
		} else {
			angle = (i * GF_PI / DOME_STEP_V);
		}

		/*switch cols*/
		if (angle >= next_angle) {
			if (ang_idx+1 < angles->count) {
				start_angle = next_angle;
				next_angle = angles->vals[ang_idx+1];
				if (next_angle>GF_PI) next_angle=GF_PI;
				start_col = end_col;
				ang_idx++;
				if (ang_idx+1<color->count) {
					COL_TO_RGBA(end_col, color->vals[ang_idx+1]);
				} else {
					pad = 1;
				}
			} else {
				if (ground_dome) break;
				pad = 1;
			}
		}

		if (pad) {
			fcol = end_col;
		} else {
			frac = gf_divfix(angle - start_angle, next_angle - start_angle) ;
			fcol.red = gf_mulfix(end_col.red - start_col.red, frac) + start_col.red;
			fcol.green = gf_mulfix(end_col.green - start_col.green, frac) + start_col.green;
			fcol.blue = gf_mulfix(end_col.blue - start_col.blue, frac) + start_col.blue;
			fcol.alpha = FIX_ONE;
		}
		vx.color = MESH_MAKE_COL(fcol);

		vx.pos.y = gf_sin(GF_PI2 - angle);
		r = gf_sqrt(FIX_ONE - gf_mulfix(vx.pos.y, vx.pos.y));

		new_idx = mesh->v_count;
		for (j = 0; j < step_div_h; j++) {
			SFVec3f n;
			Fixed lon = 2 * GF_PI * j / step_div_h;
			vx.pos.x = gf_mulfix(gf_sin(lon), r);
			vx.pos.z = gf_mulfix(gf_cos(lon), r);
			n = gf_vec_scale(vx.pos, FIX_ONE /*-FIX_ONE*/);
			gf_vec_norm(&n);
			MESH_SET_NORMAL(vx, n);
			mesh_set_vertex_vx(mesh, &vx);

			if (j) {
				if (i>1) {
					mesh_set_triangle(mesh, last_idx+j, new_idx+j, new_idx+j-1);
					mesh_set_triangle(mesh, last_idx+j, new_idx+j-1, last_idx+j-1);
				} else {
					mesh_set_triangle(mesh, 0, new_idx+j, new_idx+j-1);
				}
			}
		}
		if (i>1) {
			mesh_set_triangle(mesh, last_idx, new_idx, new_idx+step_div_h-1);
			mesh_set_triangle(mesh, last_idx, new_idx+step_div_h-1, last_idx+step_div_h-1);
		} else {
			mesh_set_triangle(mesh, 0, new_idx, new_idx+step_div_h-1);
		}
		last_idx = new_idx;
	}

	if (!ground_dome) {
		new_idx = mesh->v_count;
		vx.pos.x = vx.pos.z = 0;
		vx.pos.y = -FIX_ONE;
		vx.normal.x = vx.normal.z = 0;
		vx.normal.y = MESH_NORMAL_UNIT;
		mesh_set_vertex_vx(mesh, &vx);

		for (j=1; j < step_div_h; j++) {
			mesh_set_triangle(mesh, last_idx+j-1, last_idx+j, new_idx);
		}
		mesh_set_triangle(mesh, last_idx+step_div_h-1, last_idx, new_idx);
	}

	mesh->flags |= MESH_HAS_COLOR | MESH_NO_TEXTURE;
	mesh_update_bounds(mesh);
}

static void back_draw_texture(GF_TraverseState *tr_state, GF_TextureHandler *txh, GF_Mesh *mesh)
{
	if (gf_sc_texture_enable(txh, NULL)) {
		tr_state->mesh_num_textures = 1;
		visual_3d_mesh_paint(tr_state, mesh);
		gf_sc_texture_disable(txh);
		tr_state->mesh_num_textures = 0;
	}
}

static void TraverseBackground(GF_Node *node, void *rs, Bool is_destroy)
{
	M_Background *bck;
	BackgroundStack *st;
	SFVec4f res;
	Fixed scale;
	Bool has_sky, has_ground, front_tx, back_tx, top_tx, bottom_tx, right_tx, left_tx;
	GF_Matrix mx;
	GF_Compositor *compositor;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		DestroyBackground(node);
		return;
	}
	if (tr_state->visual->compositor->noback)
		return;

	gf_node_dirty_clear(node, 0);
	bck = (M_Background *)node;
	st = (BackgroundStack *) gf_node_get_private(node);
	compositor = (GF_Compositor*)st->compositor;


	/*may happen in get_bounds*/
	if (!tr_state->backgrounds) return;

	/*first traverse, bound if needed*/
	if (gf_list_find(tr_state->backgrounds, node) < 0) {
		gf_list_add(tr_state->backgrounds, node);
		assert(gf_list_find(st->reg_stacks, tr_state->backgrounds)==-1);
		gf_list_add(st->reg_stacks, tr_state->backgrounds);
		/*only bound if we're on top*/
		if (gf_list_get(tr_state->backgrounds, 0) == bck) {
			if (!bck->isBound) Bindable_SetIsBound(node, 1);
		}

		/*check streams*/
		if (back_use_texture(&bck->frontUrl) && !st->txh_front.is_open) gf_sc_texture_play(&st->txh_front, &bck->frontUrl);
		if (back_use_texture(&bck->bottomUrl) && !st->txh_bottom.is_open) gf_sc_texture_play(&st->txh_bottom, &bck->bottomUrl);
		if (back_use_texture(&bck->backUrl) && !st->txh_back.is_open) gf_sc_texture_play(&st->txh_back, &bck->backUrl);
		if (back_use_texture(&bck->topUrl) && !st->txh_top.is_open) gf_sc_texture_play(&st->txh_top, &bck->topUrl);
		if (back_use_texture(&bck->rightUrl) && !st->txh_right.is_open) gf_sc_texture_play(&st->txh_right, &bck->rightUrl);
		if (back_use_texture(&bck->leftUrl) && !st->txh_left.is_open) gf_sc_texture_play(&st->txh_left, &bck->leftUrl);

		/*in any case don't draw the first time (since the background could have been declared last)*/
		gf_sc_invalidate(st->compositor, NULL);
		return;
	}
	if (!bck->isBound) return;

	if (tr_state->traversing_mode != TRAVERSE_BINDABLE) {
		if (tr_state->traversing_mode == TRAVERSE_SORT) {
			gf_mx_copy(st->current_mx, tr_state->model_matrix);
			if (!tr_state->pixel_metrics && tr_state->visual->compositor->inherit_type_3d) {
				Fixed pix_scale = gf_divfix(FIX_ONE, tr_state->min_hsize);
				gf_mx_add_scale(&st->current_mx, pix_scale, pix_scale, pix_scale);

			}
		}
		return;
	}

	front_tx = back_gf_sc_texture_enabled(&bck->frontUrl, &st->txh_front);
	back_tx = back_gf_sc_texture_enabled(&bck->backUrl, &st->txh_back);
	top_tx = back_gf_sc_texture_enabled(&bck->topUrl, &st->txh_top);
	bottom_tx = back_gf_sc_texture_enabled(&bck->bottomUrl, &st->txh_bottom);
	right_tx = back_gf_sc_texture_enabled(&bck->rightUrl, &st->txh_right);
	left_tx = back_gf_sc_texture_enabled(&bck->leftUrl, &st->txh_left);

	has_sky = ((bck->skyColor.count>1) && bck->skyAngle.count) ? 1 : 0;
	has_ground = ((bck->groundColor.count>1) && bck->groundAngle.count) ? 1 : 0;

	/*if we clear the main visual clear it entirely - ONLY IF NOT IN LAYER*/
	if ((tr_state->visual == compositor->visual) && (tr_state->visual->back_stack == tr_state->backgrounds)) {
		SFColor bcol;
		bcol.red = INT2FIX( GF_COL_R(tr_state->visual->compositor->back_color)) / 255;
		bcol.green = INT2FIX( GF_COL_G(tr_state->visual->compositor->back_color)) / 255;
		bcol.blue = INT2FIX( GF_COL_B(tr_state->visual->compositor->back_color)) / 255;

		visual_3d_clear(tr_state->visual, bcol, FIX_ONE);
		if (!has_sky && !has_ground && !front_tx && !back_tx && !top_tx && !bottom_tx && !left_tx && !right_tx) {
			return;
		}
	}

	/*undo translation*/
	res.x = res.y = res.z = 0;
	res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&tr_state->camera->unprojection, &res);
	assert(res.q);
	res.x = gf_divfix(res.x, res.q);
	res.y = gf_divfix(res.y, res.q);
	res.z = gf_divfix(res.z, res.q);
	/*NB: we don't support local rotation of the background ...*/

	/*enable background state (turn off all quality options)*/
	visual_3d_set_background_state(tr_state->visual, 1);

	if (has_sky) {
		GF_Matrix bck_mx;
		gf_mx_copy(bck_mx, tr_state->model_matrix);
		gf_mx_copy(tr_state->model_matrix, st->current_mx);

		if (!st->sky_mesh) {
			st->sky_mesh = new_mesh();
			back_build_dome(st->sky_mesh, &bck->skyAngle, &bck->skyColor, 0);
		}

		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);

		/*CHECKME - not sure why, we need to scale less in fixed point otherwise z-far clipping occur - probably some
		rounding issues...*/
#ifdef GPAC_FIXED_POINT
		scale = (tr_state->camera->z_far/10)*8;
#else
		scale = 8*tr_state->camera->z_far/10;
#endif
		gf_mx_add_scale(&mx, scale, scale, scale);

		gf_mx_add_matrix(&tr_state->model_matrix, &mx);

		visual_3d_mesh_paint(tr_state, st->sky_mesh);

		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}

	if (has_ground) {
		GF_Matrix bck_mx;
		gf_mx_copy(bck_mx, tr_state->model_matrix);
		gf_mx_copy(tr_state->model_matrix, st->current_mx);

		if (!st->ground_mesh) {
			st->ground_mesh = new_mesh();
			back_build_dome(st->ground_mesh, &bck->groundAngle, &bck->groundColor, 1);
		}

		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);
		/*cf above*/
#ifdef GPAC_FIXED_POINT
		scale = (tr_state->camera->z_far/100)*70;
#else
		scale = 70*tr_state->camera->z_far/100;
#endif
		gf_mx_add_scale(&mx, scale, -scale, scale);

		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
		visual_3d_mesh_paint(tr_state, st->ground_mesh);
		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}

	if (front_tx || back_tx || left_tx || right_tx || top_tx || bottom_tx) {
		GF_Matrix bck_mx;
		gf_mx_copy(bck_mx, tr_state->model_matrix);
		gf_mx_copy(tr_state->model_matrix, st->current_mx);

		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);
#ifdef GPAC_FIXED_POINT
		scale = (tr_state->camera->z_far/100)*90;
#else
		scale = (tr_state->camera->z_far/100)*90;
#endif
		gf_mx_add_scale(&mx, scale, scale, scale);

		visual_3d_enable_antialias(tr_state->visual, 1);

		gf_mx_add_matrix(&tr_state->model_matrix, &mx);

		if (front_tx) back_draw_texture(tr_state, &st->txh_front, st->front_mesh);
		if (back_tx) back_draw_texture(tr_state, &st->txh_back, st->back_mesh);
		if (top_tx) back_draw_texture(tr_state, &st->txh_top, st->top_mesh);
		if (bottom_tx) back_draw_texture(tr_state, &st->txh_bottom, st->bottom_mesh);
		if (left_tx) back_draw_texture(tr_state, &st->txh_left, st->left_mesh);
		if (right_tx) back_draw_texture(tr_state, &st->txh_right, st->right_mesh);

		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}

	/*enable background state (turn off all quality options)*/
	visual_3d_set_background_state(tr_state->visual, 0);
}


static void back_set_bind(GF_Node *node, GF_Route *route)
{
	BackgroundStack *st = (BackgroundStack *)gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks, NULL);
	/*and redraw scene*/
	gf_sc_invalidate(st->compositor, NULL);
}

void compositor_init_background(GF_Compositor *compositor, GF_Node *node)
{
	BackgroundStack *ptr;
	GF_SAFEALLOC(ptr, BackgroundStack);
	if (!ptr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate background stack\n"));
		return;
	}

	ptr->compositor = compositor;
	ptr->reg_stacks = gf_list_new();
	((M_Background *)node)->on_set_bind = back_set_bind;

	gf_mx_init(ptr->current_mx);

	/*build texture cube*/
	ptr->front_mesh = new_mesh();
	mesh_set_vertex(ptr->front_mesh, -PLANE_HSIZE, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, 0, 0);
	mesh_set_vertex(ptr->front_mesh,  PLANE_HSIZE, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(ptr->front_mesh,  PLANE_HSIZE,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->front_mesh, -PLANE_HSIZE,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(ptr->front_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->front_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->front_mesh);

	ptr->back_mesh = new_mesh();
	mesh_set_vertex(ptr->back_mesh, -PLANE_HSIZE, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(ptr->back_mesh,  PLANE_HSIZE, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, 0, 0);
	mesh_set_vertex(ptr->back_mesh,  PLANE_HSIZE,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(ptr->back_mesh, -PLANE_HSIZE,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->back_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->back_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->back_mesh);

	ptr->top_mesh = new_mesh();
	mesh_set_vertex(ptr->top_mesh, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  0,  -FIX_ONE,  0, 0, 0);
	mesh_set_vertex(ptr->top_mesh,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  0,  -FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->top_mesh,  PLANE_HSIZE,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  0,  -FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->top_mesh, -PLANE_HSIZE,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  0,  -FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_triangle(ptr->top_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->top_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->top_mesh);

	ptr->bottom_mesh = new_mesh();
	mesh_set_vertex(ptr->bottom_mesh, -PLANE_HSIZE, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  0, FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->bottom_mesh,  PLANE_HSIZE, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  0, FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->bottom_mesh,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  0, FIX_ONE,  0, 0, 0);
	mesh_set_vertex(ptr->bottom_mesh, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  0, FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_triangle(ptr->bottom_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->bottom_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->bottom_mesh);

	ptr->left_mesh = new_mesh();
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW, -PLANE_HSIZE, -PLANE_HSIZE, FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  PLANE_HSIZE, FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  PLANE_HSIZE, FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW,  PLANE_HSIZE, -PLANE_HSIZE, FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->left_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->left_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->left_mesh);

	ptr->right_mesh = new_mesh();
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  PLANE_HSIZE, -FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW, -PLANE_HSIZE, -PLANE_HSIZE, -FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW,  PLANE_HSIZE, -PLANE_HSIZE, -FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  PLANE_HSIZE, -FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->right_mesh, 0, 1, 2);
	mesh_set_triangle(ptr->right_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->right_mesh);


	gf_sc_texture_setup(&ptr->txh_back, compositor, node);
	ptr->txh_back.update_texture_fcnt = UpdateBackgroundTexture;
	gf_sc_texture_setup(&ptr->txh_front, compositor, node);
	ptr->txh_front.update_texture_fcnt = UpdateBackgroundTexture;
	gf_sc_texture_setup(&ptr->txh_top, compositor, node);
	ptr->txh_top.update_texture_fcnt = UpdateBackgroundTexture;
	gf_sc_texture_setup(&ptr->txh_bottom, compositor, node);
	ptr->txh_bottom.update_texture_fcnt = UpdateBackgroundTexture;
	gf_sc_texture_setup(&ptr->txh_left, compositor, node);
	ptr->txh_left.update_texture_fcnt = UpdateBackgroundTexture;
	gf_sc_texture_setup(&ptr->txh_right, compositor, node);
	ptr->txh_right.update_texture_fcnt = UpdateBackgroundTexture;

	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, TraverseBackground);

}

void compositor_background_modified(GF_Node *node)
{
	M_Background *bck = (M_Background *)node;
	BackgroundStack *st = (BackgroundStack *) gf_node_get_private(node);
	if (!st) return;

	if (!gf_sg_vrml_field_equal(&bck->skyColor, &st->sky_col, GF_SG_VRML_MFCOLOR)
	        || !gf_sg_vrml_field_equal(&bck->skyAngle, &st->sky_ang, GF_SG_VRML_MFFLOAT)
	   ) {

		if (st->sky_mesh) mesh_free(st->sky_mesh);
		st->sky_mesh = NULL;
		gf_sg_vrml_field_copy(&st->sky_col, &bck->skyColor, GF_SG_VRML_MFCOLOR);
		gf_sg_vrml_field_copy(&st->sky_ang, &bck->skyAngle, GF_SG_VRML_MFFLOAT);
	}
	if (!gf_sg_vrml_field_equal(&bck->groundColor, &st->ground_col, GF_SG_VRML_MFCOLOR)
	        || !gf_sg_vrml_field_equal(&bck->groundAngle, &st->ground_ang, GF_SG_VRML_MFFLOAT)
	   ) {

		if (st->ground_mesh) mesh_free(st->ground_mesh);
		st->ground_mesh = NULL;
		gf_sg_vrml_field_copy(&st->ground_col, &bck->groundColor, GF_SG_VRML_MFCOLOR);
		gf_sg_vrml_field_copy(&st->ground_ang, &bck->groundAngle, GF_SG_VRML_MFFLOAT);
	}

	back_check_gf_sc_texture_change(&st->txh_front, &bck->frontUrl);
	back_check_gf_sc_texture_change(&st->txh_back, &bck->backUrl);
	back_check_gf_sc_texture_change(&st->txh_top, &bck->topUrl);
	back_check_gf_sc_texture_change(&st->txh_bottom, &bck->bottomUrl);
	back_check_gf_sc_texture_change(&st->txh_left, &bck->leftUrl);
	back_check_gf_sc_texture_change(&st->txh_right, &bck->rightUrl);


	gf_sc_invalidate(st->compositor, NULL);
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_COMPOSITOR)
