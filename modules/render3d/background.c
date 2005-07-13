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
	gf_sr_texture_update_frame(txh, 0);
	/*restart texture if needed (movie background controled by MediaControl)*/
	if (txh->stream_finished && gf_mo_get_loop(txh->stream, 0)) gf_sr_texture_restart(txh);
}


static Bool back_gf_sr_texture_enabled(MFURL *url, GF_TextureHandler *txh)
{
	Bool use_texture = back_use_texture(url);
	if (use_texture) {
		/*texture not ready*/
		if (!txh->hwtx) {
			use_texture = 0;
			gf_sr_invalidate(txh->compositor, NULL);
		}
		tx_set_blend_mode(txh, txh->transparent ? TX_REPLACE : TX_DECAL);
	}
	return use_texture;
}

static void back_check_gf_sr_texture_change(GF_TextureHandler *txh, MFURL *url)
{
	/*if open and changed, stop and play*/
	if (txh->is_open) {
		if (! gf_sr_texture_check_url_change(txh, url)) return;
		gf_sr_texture_stop(txh);
		gf_sr_texture_play(txh, url);
		return;
	}
	/*if not open and changed play*/
	if (url->count) gf_sr_texture_play(txh, url);
}

static void DestroyBackground2D(GF_Node *node)
{
	Background2DStack *ptr;
	ptr = (Background2DStack *) gf_node_get_private(node);
	PreDestroyBindable(node, ptr->reg_stacks);
	gf_list_del(ptr->reg_stacks);
	gf_sr_texture_destroy(&ptr->txh);
	mesh_free(ptr->mesh);
	free(ptr);
}

static void RenderBackground2D(GF_Node *node, void *rs)
{
	M_Background2D *bck;
	Bool use_texture;
	Background2DStack *st;
	GF_Matrix mx;
	Bool is_layer;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	Render3D *sr;

	gf_node_dirty_clear(node, 0);
	bck = (M_Background2D *)node;
	st = (Background2DStack *) gf_node_get_private(node);
	sr = (Render3D*)st->compositor->visual_renderer->user_priv;

	assert(eff->backgrounds);

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->backgrounds, node) < 0) {
		gf_list_add(eff->backgrounds, node);
		assert(gf_list_find(st->reg_stacks, eff->backgrounds)==-1);
		gf_list_add(st->reg_stacks, eff->backgrounds);
		/*only bound if we're on top*/
		if (gf_list_get(eff->backgrounds, 0) == bck) {
			if (!bck->isBound) Bindable_SetIsBound(node, 1);
		}
		/*open the stream if any*/
		if (back_use_texture(&bck->url) && !st->txh.is_open) gf_sr_texture_play(&st->txh, &bck->url);
		/*in any case don't draw the first time (since the background could have been declared last)*/
		gf_sr_invalidate(st->compositor, NULL);
		return;
	}
	if (!bck->isBound) return;
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) return;

	use_texture = back_gf_sr_texture_enabled(&bck->url, &st->txh);

	/*no lights on background*/
	VS3D_SetState(eff->surface, F3D_LIGHT | F3D_BLEND, 0);

	VS3D_PushMatrix(eff->surface);

	is_layer = (eff->surface->back_stack == eff->backgrounds) ? 0 : 1;
	/*little opt: if we clear the main surface clear it entirely */
	if (!is_layer) {
		VS3D_ClearSurface(eff->surface, bck->backColor, FIX_ONE);
		if (!use_texture) {
			VS3D_PopMatrix(eff->surface);
			return;
		}
		/*we need a hack here because main vp is always rendered before main background, and in the case of a
		2D viewport it modifies the modelview matrix, which we don't want ...*/
		VS3D_ResetMatrix(eff->surface);
	}
	if (!use_texture || (!is_layer && st->txh.transparent) ) VS3D_SetMaterial2D(eff->surface, bck->backColor, FIX_ONE);
	if (use_texture) {
		VS3D_SetState(eff->surface, F3D_COLOR, !is_layer);
		use_texture = tx_enable(&st->txh, NULL);
	}

	gf_mx_init(mx);
	if (eff->camera->is_3D) {
		Fixed sx, sy;
		/*reset matrix*/
		VS3D_ResetMatrix(eff->surface);
		sx = sy = 2 * gf_mulfix(gf_tan(eff->camera->fieldOfView/2), eff->camera->z_far);
		if (eff->camera->width > eff->camera->height) {
			sx = gf_muldiv(sx, eff->camera->width, eff->camera->height);
		} else {
			sy = gf_muldiv(sy, eff->camera->height, eff->camera->width);
		}
		gf_mx_add_scale(&mx, sx, sy, FIX_ONE);
#ifdef GPAC_FIXED_POINT
		gf_mx_add_translation(&mx, 0, 0, -85*eff->camera->z_far/100);
#else
		gf_mx_add_translation(&mx, 0, 0, -0.999f*eff->camera->z_far);
#endif
	} else {
		gf_mx_add_scale(&mx, eff->bbox.max_edge.x - eff->bbox.min_edge.x, 
							eff->bbox.max_edge.y - eff->bbox.min_edge.y,
							FIX_ONE);
		/*when in layer2D, DON'T MOVE BACKGROUND TO ZFAR*/
#ifdef GPAC_FIXED_POINT
		if (!is_layer) gf_mx_add_translation(&mx, 0, 0, -85*eff->camera->z_far/100);
#else
		if (!is_layer) gf_mx_add_translation(&mx, 0, 0, -0.999f*eff->camera->z_far);
#endif
	}
	VS3D_MultMatrix(eff->surface, mx.m);
	VS3D_DrawMesh(eff, st->mesh, 0);
	if (use_texture) tx_disable(&st->txh);

	VS3D_PopMatrix(eff->surface);
}


static void b2D_set_bind(GF_Node *node)
{
	Background2DStack *st = (Background2DStack *)gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	/*and redraw scene*/
	gf_sr_invalidate(st->compositor, NULL);
}

void R3D_InitBackground2D(Render3D *sr, GF_Node *node)
{
	Background2DStack *ptr = malloc(sizeof(Background2DStack));
	memset(ptr, 0, sizeof(Background2DStack));

	gf_sr_traversable_setup(ptr, node, sr->compositor);
	ptr->reg_stacks = gf_list_new();
	((M_Background2D *)node)->on_set_bind = b2D_set_bind;
	gf_sr_texture_setup(&ptr->txh, sr->compositor, node);
	ptr->txh.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_node_set_private(node, ptr);
	gf_node_set_predestroy_function(node, DestroyBackground2D);
	gf_node_set_render_function(node, RenderBackground2D);

	ptr->mesh = new_mesh();
	mesh_set_vertex(ptr->mesh, -PLANE_HSIZE, -PLANE_HSIZE, 0,  0,  0,  FIX_ONE, 0, 0);
	mesh_set_vertex(ptr->mesh,  PLANE_HSIZE, -PLANE_HSIZE, 0,  0,  0,  FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(ptr->mesh,  PLANE_HSIZE,  PLANE_HSIZE, 0,  0,  0,  FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->mesh, -PLANE_HSIZE,  PLANE_HSIZE, 0,  0,  0,  FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(ptr->mesh, 0, 1, 2); mesh_set_triangle(ptr->mesh, 0, 2, 3);
	ptr->mesh->flags |= MESH_IS_2D;
}

void R3D_Background2DModified(GF_Node *node)
{
	M_Background2D *bck = (M_Background2D *)node;
	Background2DStack *st = (Background2DStack *) gf_node_get_private(node);
	if (!st) return;

	back_check_gf_sr_texture_change(&st->txh, &bck->url);
	
	gf_sr_invalidate(st->txh.compositor, NULL);
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

	
	gf_sr_texture_destroy(&ptr->txh_front);
	gf_sr_texture_destroy(&ptr->txh_back);
	gf_sr_texture_destroy(&ptr->txh_top);
	gf_sr_texture_destroy(&ptr->txh_bottom);
	gf_sr_texture_destroy(&ptr->txh_left);
	gf_sr_texture_destroy(&ptr->txh_right);
	
	free(ptr);
}

#define COL_TO_RGBA(res, col) { res.red = col.red; res.green = col.green; res.blue = col.blue; res.alpha = FIX_ONE; }

#define DOME_STEP_V	12
#define DOME_STEP_H	16

static void back_build_dome(GF_Mesh *mesh, MFFloat *angles, MFColor *color, Bool ground_dome)
{
	u32 i, j, last_idx, ang_idx, new_idx;
	Bool pad;
	u32 step_div_h;
	GF_Vertex vx;
	SFColorRGBA start_col, end_col;
	Fixed start_angle, next_angle, angle, r, frac;

	start_angle = 0;
	mesh_reset(mesh);

	start_col.red = start_col.green = start_col.blue = 0;
	end_col = start_col;
	if (color->count) {
		COL_TO_RGBA(start_col, color->vals[0]);
		end_col = start_col;
		if (color->count>1) COL_TO_RGBA(end_col, color->vals[1]);
	}

	start_col.alpha = end_col.alpha = FIX_ONE;
	vx.texcoords.x = vx.texcoords.y = 0;
	vx.color = start_col;
	vx.pos.x = vx.pos.z = 0; vx.pos.y = FIX_ONE;
	vx.normal.x = vx.normal.z = 0; vx.normal.y = -FIX_ONE;
	mesh_set_vertex_vx(mesh, &vx);
	last_idx = 0;
	ang_idx = 0;
	
	pad = 1;
	next_angle = 0;
	if (angles->count) {
		next_angle = angles->vals[0];
		pad = 0;
	}

	step_div_h = DOME_STEP_H;
	if (ground_dome) step_div_h *= 2;


	for (i=1; i<DOME_STEP_V; i++) {
        angle = (i * GF_PI / DOME_STEP_V);

		/*switch cols*/
		if (angle >= next_angle) {
			if (ang_idx+1<=angles->count) {
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
			vx.color = end_col;
		} else {
			frac = gf_divfix(angle - start_angle, next_angle - start_angle) ;
			vx.color.red = gf_mulfix(end_col.red - start_col.red, frac) + start_col.red;
			vx.color.green = gf_mulfix(end_col.green - start_col.green, frac) + start_col.green;
			vx.color.blue = gf_mulfix(end_col.blue - start_col.blue, frac) + start_col.blue;
		}

        vx.pos.y = gf_sin(GF_PI2 - angle);
        r = gf_sqrt(FIX_ONE - gf_mulfix(vx.pos.y, vx.pos.y));
	
		new_idx = mesh->v_count;
        for (j = 0; j < step_div_h; j++) {
            Fixed lon = 2 * GF_PI * j / step_div_h;
            vx.pos.x = gf_mulfix(gf_sin(lon), r);
            vx.pos.z = gf_mulfix(gf_cos(lon), r);
			vx.normal = gf_vec_scale(vx.pos, FIX_ONE /*-FIX_ONE*/);
			gf_vec_norm(&vx.normal);
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
		vx.pos.x = vx.pos.z = 0; vx.pos.y = -FIX_ONE;
		vx.normal.x = vx.normal.z = 0; vx.normal.y = FIX_ONE;
		mesh_set_vertex_vx(mesh, &vx);

		for (j=1; j < step_div_h; j++) {
			mesh_set_triangle(mesh, last_idx+j-1, last_idx+j, new_idx);
		}
		mesh_set_triangle(mesh, last_idx+step_div_h-1, last_idx, new_idx);
	}

	mesh->flags |= MESH_HAS_COLOR | MESH_NO_TEXTURE;
	mesh_update_bounds(mesh);
}

static void back_draw_texture(RenderEffect3D *eff, GF_TextureHandler *txh, GF_Mesh *mesh)
{
	if (tx_enable(txh, NULL)) {
		VS3D_DrawMesh(eff, mesh, 0);
		tx_disable(txh);
	}
}

static void RenderBackground(GF_Node *node, void *rs)
{
	M_Background *bck;
	BackgroundStack *st;
	SFColor bcol;
	SFVec4f res;
	Fixed scale;
	Bool has_sky, has_ground, front_tx, back_tx, top_tx, bottom_tx, right_tx, left_tx;
	GF_Matrix mx;
	Render3D *sr;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	gf_node_dirty_clear(node, 0);
	bck = (M_Background *)node;
	st = (BackgroundStack *) gf_node_get_private(node);
	sr = (Render3D*)st->compositor->visual_renderer->user_priv;


	assert(eff->backgrounds);

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->backgrounds, node) < 0) {
		gf_list_add(eff->backgrounds, node);
		assert(gf_list_find(st->reg_stacks, eff->backgrounds)==-1);
		gf_list_add(st->reg_stacks, eff->backgrounds);
		/*only bound if we're on top*/
		if (gf_list_get(eff->backgrounds, 0) == bck) {
			if (!bck->isBound) Bindable_SetIsBound(node, 1);
		}

		/*check streams*/
		if (back_use_texture(&bck->frontUrl) && !st->txh_front.is_open) gf_sr_texture_play(&st->txh_front, &bck->frontUrl);
		if (back_use_texture(&bck->bottomUrl) && !st->txh_bottom.is_open) gf_sr_texture_play(&st->txh_bottom, &bck->bottomUrl);
		if (back_use_texture(&bck->backUrl) && !st->txh_back.is_open) gf_sr_texture_play(&st->txh_back, &bck->backUrl);
		if (back_use_texture(&bck->topUrl) && !st->txh_top.is_open) gf_sr_texture_play(&st->txh_top, &bck->topUrl);
		if (back_use_texture(&bck->rightUrl) && !st->txh_right.is_open) gf_sr_texture_play(&st->txh_right, &bck->rightUrl);
		if (back_use_texture(&bck->leftUrl) && !st->txh_left.is_open) gf_sr_texture_play(&st->txh_left, &bck->leftUrl);

		/*in any case don't draw the first time (since the background could have been declared last)*/
		gf_sr_invalidate(st->compositor, NULL);
		return;
	}
	if (!bck->isBound) return;
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) return;

	front_tx = back_gf_sr_texture_enabled(&bck->frontUrl, &st->txh_front);
	back_tx = back_gf_sr_texture_enabled(&bck->backUrl, &st->txh_back);
	top_tx = back_gf_sr_texture_enabled(&bck->topUrl, &st->txh_top);
	bottom_tx = back_gf_sr_texture_enabled(&bck->bottomUrl, &st->txh_bottom);
	right_tx = back_gf_sr_texture_enabled(&bck->rightUrl, &st->txh_right);
	left_tx = back_gf_sr_texture_enabled(&bck->leftUrl, &st->txh_left);

	has_sky = ((bck->skyColor.count>1) && bck->skyAngle.count) ? 1 : 0;
	has_ground = ((bck->groundColor.count>1) && bck->groundAngle.count) ? 1 : 0;
	bcol.red = bcol.green = bcol.blue = 0;
	if (bck->skyColor.count) bcol = bck->skyColor.vals[0];

	/*if we clear the main surface clear it entirely - ONLY IF NOT IN LAYER*/
	if ((eff->surface == sr->surface) && (eff->surface->back_stack == eff->backgrounds)) {
		VS3D_ClearSurface(eff->surface, bcol, FIX_ONE);
		if (!has_sky && !has_ground && !front_tx && !back_tx && !top_tx && !bottom_tx && !left_tx && !right_tx) {
			return;
		}
	}

	VS3D_SetState(eff->surface, F3D_LIGHT | F3D_BLEND, 0);

	/*undo translation*/
	res.x = res.y = res.z = 0; res.q = FIX_ONE;
	gf_mx_apply_vec_4x4(&eff->camera->unprojection, &res);
	assert(res.q);
	res.x = gf_divfix(res.x, res.q);
	res.y = gf_divfix(res.y, res.q);
	res.z = gf_divfix(res.z, res.q);
	/*NB: we don't support local rotation of the background ...*/

	if (has_sky) {
		if (!st->sky_mesh) {
			st->sky_mesh = new_mesh();
			back_build_dome(st->sky_mesh, &bck->skyAngle, &bck->skyColor, 0);
		}
		VS3D_PushMatrix(eff->surface);
		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);

		/*CHECKME - not sure why, we need to scale less in fixed point otherwise z-far clipping occur - probably some
		rounding issues...*/
#ifdef GPAC_FIXED_POINT
		scale = 4*eff->camera->z_far/5;
#else
		scale = 9*eff->camera->z_far/10;
#endif
		gf_mx_add_scale(&mx, scale, scale, scale);

		VS3D_MultMatrix(eff->surface, mx.m);
		VS3D_DrawMesh(eff, st->sky_mesh, 0);
		VS3D_PopMatrix(eff->surface);
	}

	if (has_ground) {
		if (!st->ground_mesh) {
			st->ground_mesh = new_mesh();
			back_build_dome(st->ground_mesh, &bck->groundAngle, &bck->groundColor, 1);
		}
		VS3D_PushMatrix(eff->surface);
		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);
		/*cf above*/
#ifdef GPAC_FIXED_POINT
		scale = 75*eff->camera->z_far/100;
#else
		scale = 85*eff->camera->z_far/100;
#endif
		gf_mx_add_scale(&mx, scale, -scale, scale);
		VS3D_MultMatrix(eff->surface, mx.m);
		VS3D_DrawMesh(eff, st->ground_mesh, 0);
		VS3D_PopMatrix(eff->surface);
	}

	if (front_tx || back_tx || left_tx || right_tx || top_tx || bottom_tx) {
		VS3D_PushMatrix(eff->surface);
		gf_mx_init(mx);
		gf_mx_add_translation(&mx, res.x, res.y, res.z);
#ifdef GPAC_FIXED_POINT
		scale = 70*eff->camera->z_far/100;
		gf_mx_add_scale(&mx, scale, scale, scale);
#else
		gf_mx_add_scale(&mx, eff->camera->z_far, eff->camera->z_far, eff->camera->z_far);
#endif
		VS3D_MultMatrix(eff->surface, mx.m);

		VS3D_SetAntiAlias(eff->surface, 1);

		if (front_tx) back_draw_texture(eff, &st->txh_front, st->front_mesh);
		if (back_tx) back_draw_texture(eff, &st->txh_back, st->back_mesh);
		if (top_tx) back_draw_texture(eff, &st->txh_top, st->top_mesh);
		if (bottom_tx) back_draw_texture(eff, &st->txh_bottom, st->bottom_mesh);
		if (left_tx) back_draw_texture(eff, &st->txh_left, st->left_mesh);
		if (right_tx) back_draw_texture(eff, &st->txh_right, st->right_mesh);

		VS3D_PopMatrix(eff->surface);
	}
}


static void back_set_bind(GF_Node *node)
{
	BackgroundStack *st = (BackgroundStack *)gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	/*and redraw scene*/
	gf_sr_invalidate(st->compositor, NULL);
}

void R3D_InitBackground(Render3D *sr, GF_Node *node)
{
	BackgroundStack *ptr = malloc(sizeof(BackgroundStack));
	memset(ptr, 0, sizeof(BackgroundStack));

	gf_sr_traversable_setup(ptr, node, sr->compositor);
	ptr->reg_stacks = gf_list_new();
	((M_Background *)node)->on_set_bind = back_set_bind;


	/*build texture cube*/
	ptr->front_mesh = new_mesh();
	mesh_set_vertex(ptr->front_mesh, -PLANE_HSIZE, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, 0, 0);
	mesh_set_vertex(ptr->front_mesh,  PLANE_HSIZE, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(ptr->front_mesh,  PLANE_HSIZE,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->front_mesh, -PLANE_HSIZE,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  0,  0,  FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(ptr->front_mesh, 0, 1, 2); mesh_set_triangle(ptr->front_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->front_mesh);

	ptr->back_mesh = new_mesh();
	mesh_set_vertex(ptr->back_mesh, -PLANE_HSIZE, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(ptr->back_mesh,  PLANE_HSIZE, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, 0, 0);
	mesh_set_vertex(ptr->back_mesh,  PLANE_HSIZE,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(ptr->back_mesh, -PLANE_HSIZE,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  0,  0,  -FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->back_mesh, 0, 1, 2); mesh_set_triangle(ptr->back_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->back_mesh);

	ptr->top_mesh = new_mesh();
	mesh_set_vertex(ptr->top_mesh, -PLANE_HSIZE,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  0,  -FIX_ONE,  0, 0, 0);
	mesh_set_vertex(ptr->top_mesh,  PLANE_HSIZE,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  0,  -FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->top_mesh,  PLANE_HSIZE,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  0,  -FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->top_mesh, -PLANE_HSIZE,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  0,  -FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_triangle(ptr->top_mesh, 0, 1, 2); mesh_set_triangle(ptr->top_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->top_mesh);

	ptr->bottom_mesh = new_mesh();
	mesh_set_vertex(ptr->bottom_mesh, -PLANE_HSIZE, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  0, FIX_ONE,  0, FIX_ONE, FIX_ONE);
	mesh_set_vertex(ptr->bottom_mesh,  PLANE_HSIZE, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  0, FIX_ONE,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->bottom_mesh,  PLANE_HSIZE, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  0, FIX_ONE,  0, 0, 0);
	mesh_set_vertex(ptr->bottom_mesh, -PLANE_HSIZE, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  0, FIX_ONE,  0, 0, FIX_ONE);
	mesh_set_triangle(ptr->bottom_mesh, 0, 1, 2); mesh_set_triangle(ptr->bottom_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->bottom_mesh);

	ptr->left_mesh = new_mesh();
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW, -PLANE_HSIZE, -PLANE_HSIZE, FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW, -PLANE_HSIZE,  PLANE_HSIZE, FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW,  PLANE_HSIZE,  PLANE_HSIZE, FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->left_mesh, -PLANE_HSIZE_LOW,  PLANE_HSIZE, -PLANE_HSIZE, FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->left_mesh, 0, 1, 2); mesh_set_triangle(ptr->left_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->left_mesh);

	ptr->right_mesh = new_mesh();
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW, -PLANE_HSIZE,  PLANE_HSIZE, -FIX_ONE,  0,  0, FIX_ONE, 0);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW, -PLANE_HSIZE, -PLANE_HSIZE, -FIX_ONE,  0,  0, 0, 0);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW,  PLANE_HSIZE, -PLANE_HSIZE, -FIX_ONE,  0,  0, 0, FIX_ONE);
	mesh_set_vertex(ptr->right_mesh,  PLANE_HSIZE_LOW,  PLANE_HSIZE,  PLANE_HSIZE, -FIX_ONE,  0,  0, FIX_ONE, FIX_ONE);
	mesh_set_triangle(ptr->right_mesh, 0, 1, 2); mesh_set_triangle(ptr->right_mesh, 0, 2, 3);
	mesh_update_bounds(ptr->right_mesh);


	gf_sr_texture_setup(&ptr->txh_back, sr->compositor, node);
	ptr->txh_back.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_sr_texture_setup(&ptr->txh_front, sr->compositor, node);
	ptr->txh_front.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_sr_texture_setup(&ptr->txh_top, sr->compositor, node);
	ptr->txh_top.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_sr_texture_setup(&ptr->txh_bottom, sr->compositor, node);
	ptr->txh_bottom.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_sr_texture_setup(&ptr->txh_left, sr->compositor, node);
	ptr->txh_left.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;
	gf_sr_texture_setup(&ptr->txh_right, sr->compositor, node);
	ptr->txh_right.update_gf_sr_texture_fcnt = UpdateBackgroundTexture;

	gf_node_set_private(node, ptr);
	gf_node_set_predestroy_function(node, DestroyBackground);
	gf_node_set_render_function(node, RenderBackground);

}

void R3D_BackgroundModified(GF_Node *node)
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

	back_check_gf_sr_texture_change(&st->txh_front, &bck->frontUrl);
	back_check_gf_sr_texture_change(&st->txh_back, &bck->backUrl);
	back_check_gf_sr_texture_change(&st->txh_top, &bck->topUrl);
	back_check_gf_sr_texture_change(&st->txh_bottom, &bck->bottomUrl);
	back_check_gf_sr_texture_change(&st->txh_left, &bck->leftUrl);
	back_check_gf_sr_texture_change(&st->txh_right, &bck->rightUrl);

	
	gf_sr_invalidate(st->compositor, NULL);
}

