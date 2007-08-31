/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D rendering module
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

#ifndef GPAC_DISABLE_3D

GF_Err render_3d_set_aspect_ratio(Render *sr)
{
	GF_Event evt;
	Double ratio;
	Fixed scaleX, scaleY;

	if (!sr->compositor->height || !sr->compositor->width) return GF_OK;

	sr->visual->camera.flags |= CAM_IS_DIRTY;

	sr->cur_width = sr->vp_width = sr->compositor->width;
	sr->cur_height = sr->vp_height = sr->compositor->height;
	sr->vp_x = 0;
	sr->vp_y = 0;

	if (!sr->compositor->has_size_info) {
		render_set_aspect_ratio_scaling(sr, FIX_ONE, FIX_ONE);
		sr->visual->width = sr->vp_width;
		sr->visual->height = sr->vp_height;
	} else {

		switch (sr->compositor->aspect_ratio) {
		case GF_ASPECT_RATIO_FILL_SCREEN:
			break;
		case GF_ASPECT_RATIO_16_9:
			sr->vp_height = 9 * sr->vp_width  / 16;
			break;
		case GF_ASPECT_RATIO_4_3:
			sr->vp_height = 3 * sr->vp_width / 4;
			break;
		default:
			ratio = sr->compositor->scene_height;
			ratio /= sr->compositor->scene_width;
			if (sr->vp_width * ratio > sr->vp_height) {
				sr->vp_width = sr->vp_height * sr->compositor->scene_width;
				sr->vp_width /= sr->compositor->scene_height;
			}
			else {
				sr->vp_height = sr->vp_width * sr->compositor->scene_height;
				sr->vp_height /= sr->compositor->scene_width;
			}
			break;
		}
		sr->vp_x = (sr->compositor->width - sr->vp_width) / 2;
		sr->vp_y = (sr->compositor->height - sr->vp_height) / 2;
		/*update size info for main visual*/
		if (sr->visual) {
			sr->visual->width = sr->compositor->scene_width;
			sr->visual->height = sr->compositor->scene_height;
		}
		/*scaling is still needed for bitmap*/
		scaleX = gf_divfix(INT2FIX(sr->vp_width), INT2FIX(sr->compositor->scene_width));
		scaleY = gf_divfix(INT2FIX(sr->vp_height), INT2FIX(sr->compositor->scene_height));
		render_set_aspect_ratio_scaling(sr, scaleX, scaleY);
	}

	/*and resetup OpenGL*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = sr->compositor->width;
	evt.setup.height = sr->compositor->height;
	evt.setup.back_buffer = 1;
	evt.setup.opengl_mode = 1;
	sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
	sr->compositor->reset_graphics=0;
	return GF_OK;
}


GF_Camera *render_3d_get_camera(Render *sr) 
{
	if (sr->active_layer) {
		return render_layer3d_get_camera(sr->active_layer);
	} else {
		return &sr->visual->camera;
	}
}

void render_3d_reset_camera(Render *sr)
{
	GF_Camera *cam = render_3d_get_camera(sr);
	camera_reset_viewpoint(cam, 1);
	gf_sr_invalidate(sr->compositor, NULL);
}

void render_3d_draw_bitmap(Drawable *stack, DrawAspect2D *asp, GF_TraverseState *tr_state, Fixed width, Fixed height, Fixed bmp_scale_x, Fixed bmp_scale_y)
{
	u8 alpha;
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
	Fixed x, y;
	Fixed sx, sy;
	char *data;
	u32 format;
#endif
	GF_TextureHandler *txh;
	Render *sr = tr_state->visual->render;


	if (!asp->fill_texture) return;
	txh = asp->fill_texture;
	if (!txh || !txh->hwtx || !txh->width || !txh->height) return;

	alpha = GF_COL_A(asp->fill_color);
	/*texture is available in hw, use it - if blending, force using texture*/
	if (!tx_needs_reload(txh) || (alpha != 0xFF) || !sr->bitmap_use_pixels) {
		visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT, 0);
		visual_3d_enable_antialias(tr_state->visual, 0);
		if (alpha && (alpha != 0xFF)) {
			visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
			tx_set_blend_mode(txh, TX_MODULATE);
		} else if (render_texture_is_transparent(txh)) {
			tx_set_blend_mode(txh, TX_REPLACE);
		} else {
			visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);
		}
		/*ignore texture transform for bitmap*/
 		tr_state->mesh_has_texture = tx_enable(txh, NULL);
		if (tr_state->mesh_has_texture) {
			if (!stack->mesh) {
				SFVec2f size;
				size.x = width;
				size.y = height;

				stack->mesh = new_mesh();
				mesh_new_rectangle(stack->mesh, size);
			}

			visual_3d_mesh_paint(tr_state, stack->mesh);
 			tx_disable(txh);
			tr_state->mesh_has_texture = 0;
			return;
		}
	}

	/*otherwise use glDrawPixels*/
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
	data = tx_get_data(txh, &format);
	if (!data) return;

	x = INT2FIX(txh->width) / -2;
	y = INT2FIX(txh->height) / 2;

	{
	Fixed g[16];

	sx = bmp_scale_x; if (sx<0) sx = FIX_ONE;
	sy = bmp_scale_y; if (sy<0) sy = FIX_ONE;
	render_adjust_composite_scale(txh->owner, &sx, &sy);

	/*add top level scale if any*/
	sx = gf_mulfix(sx, sr->scale_x);
	sy = gf_mulfix(sy, sr->scale_y);

	/*get & apply current transform scale*/
	visual_3d_matrix_get(tr_state->visual, V3D_MATRIX_MODELVIEW, g);

	if (g[0]<0) g[0] *= -FIX_ONE;
	if (g[5]<0) g[5] *= -FIX_ONE;
	sx = gf_mulfix(sx, g[0]);
	sy = gf_mulfix(sy, g[5]);
	x = gf_mulfix(x, sx);
	y = gf_mulfix(y, sy);

	}
	visual_3d_draw_image(tr_state->visual, x, y, txh->width, txh->height, format, data, sx, sy);
#endif
}


#endif

