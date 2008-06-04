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

#ifdef GPAC_USE_TINYGL
#include "../../TinyGL/include/GL/oscontext.h"
//#include <GL/oscontext.h>
#endif

GF_Err compositor_3d_set_aspect_ratio(GF_Compositor *compositor)
{
	GF_Event evt;
	Double ratio;
	Fixed scaleX, scaleY;

	if (!compositor->display_height || !compositor->display_width) return GF_OK;

	compositor->visual->camera.flags |= CAM_IS_DIRTY;

	compositor->output_width = compositor->vp_width = compositor->display_width;
	compositor->output_height = compositor->vp_height = compositor->display_height;
	compositor->vp_x = 0;
	compositor->vp_y = 0;

	scaleX = scaleY = FIX_ONE;
	if (!compositor->has_size_info) {
		compositor->visual->width = compositor->vp_width;
		compositor->visual->height = compositor->vp_height;
	} else {

		switch (compositor->aspect_ratio) {
		case GF_ASPECT_RATIO_FILL_SCREEN:
			break;
		case GF_ASPECT_RATIO_16_9:
			compositor->vp_height = 9 * compositor->vp_width  / 16;
			break;
		case GF_ASPECT_RATIO_4_3:
			compositor->vp_height = 3 * compositor->vp_width / 4;
			break;
		default:
			ratio = compositor->scene_height;
			ratio /= compositor->scene_width;
			if (compositor->vp_width * ratio > compositor->vp_height) {
				compositor->vp_width = compositor->vp_height * compositor->scene_width;
				compositor->vp_width /= compositor->scene_height;
			}
			else {
				compositor->vp_height = compositor->vp_width * compositor->scene_height;
				compositor->vp_height /= compositor->scene_width;
			}
			break;
		}
		compositor->vp_x = (compositor->display_width - compositor->vp_width) / 2;
		compositor->vp_y = (compositor->display_height - compositor->vp_height) / 2;
		/*update size info for main visual*/
		if (compositor->visual) {
			compositor->visual->width = compositor->scene_width;
			compositor->visual->height = compositor->scene_height;
		}
		/*scaling is still needed for bitmap*/
		scaleX = gf_divfix(INT2FIX(compositor->vp_width), INT2FIX(compositor->scene_width));
		scaleY = gf_divfix(INT2FIX(compositor->vp_height), INT2FIX(compositor->scene_height));
	}

	if (compositor->has_size_info) {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->scene_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->scene_height);
	} else {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->output_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->output_height);
	}
	compositor_set_ar_scale(compositor, scaleX, scaleY);


	/*and resetup OpenGL*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = compositor->display_width;
	evt.setup.height = compositor->display_height;
	evt.setup.back_buffer = 1;
#ifdef GPAC_USE_TINYGL
	evt.setup.opengl_mode = 0;
#else
	evt.setup.opengl_mode = 1;
#endif
	compositor->video_out->ProcessEvent(compositor->video_out, &evt);

#ifdef GPAC_USE_TINYGL
	{
		u32 bpp;
		GF_VideoSurface bb;
		GF_Err e = compositor->video_out->LockBackBuffer(compositor->video_out, &bb, 1);
		if (e) return e;
		switch (bb.pixel_format) {
		case GF_PIXEL_RGB_32:
		case GF_PIXEL_ARGB:
			bpp = 32;
			break;
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_BGR_24:
			bpp = 24;
			break;
		case GF_PIXEL_RGB_565:
		case GF_PIXEL_RGB_555:
			bpp = 16;
			break;
		default:
			e = GF_NOT_SUPPORTED;
			bpp = 0;
			break;
		}
		compositor->tgl_ctx = ostgl_create_context(bb.width, bb.height, bpp, &bb.video_buffer, 1);
		if (compositor->tgl_ctx) ostgl_make_current(compositor->tgl_ctx, 0);
		compositor->video_out->LockBackBuffer(compositor->video_out, &bb, 0);
	}
#endif
	
	compositor->reset_graphics=0;
	return GF_OK;
}


GF_Camera *compositor_3d_get_camera(GF_Compositor *compositor) 
{
	if (compositor->active_layer) {
		return compositor_layer3d_get_camera(compositor->active_layer);
	} else {
		return &compositor->visual->camera;
	}
}

void compositor_3d_reset_camera(GF_Compositor *compositor)
{
	GF_Camera *cam = compositor_3d_get_camera(compositor);
	camera_reset_viewpoint(cam, 1);
	gf_sc_invalidate(compositor, NULL);
}

void compositor_3d_draw_bitmap(Drawable *stack, DrawAspect2D *asp, GF_TraverseState *tr_state, Fixed width, Fixed height, Fixed bmp_scale_x, Fixed bmp_scale_y)
{
	u8 alpha;
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
	Fixed x, y;
	Fixed sx, sy;
	char *data;
	u32 format;
#endif
	GF_TextureHandler *txh;
	GF_Compositor *compositor = tr_state->visual->compositor;


	if (!asp->fill_texture) return;
	txh = asp->fill_texture;
	if (!txh || !txh->tx_io || !txh->width || !txh->height) return;

	alpha = GF_COL_A(asp->fill_color);
	/*texture is available in hw, use it - if blending, force using texture*/
	if (!gf_sc_texture_needs_reload(txh) || (alpha != 0xFF) || !compositor->bitmap_use_pixels) {
		visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT, 0);
		visual_3d_enable_antialias(tr_state->visual, 0);
		if (alpha && (alpha != 0xFF)) {
			visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
			gf_sc_texture_set_blend_mode(txh, TX_MODULATE);
		} else if (gf_sc_texture_is_transparent(txh)) {
			gf_sc_texture_set_blend_mode(txh, TX_REPLACE);
		} else {
			visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);
		}
		/*ignore texture transform for bitmap*/
 		tr_state->mesh_num_textures = gf_sc_texture_enable(txh, NULL);
		if (tr_state->mesh_num_textures) {
			/*we must check the w & h passed are correct because of bitmap node initialization*/
			if (width && height) {
				if (!stack->mesh) {
					SFVec2f size;
					size.x = width;
					size.y = height;

					stack->mesh = new_mesh();
					mesh_new_rectangle(stack->mesh, size);
				}
			} 
			if (stack->mesh) {
				visual_3d_mesh_paint(tr_state, stack->mesh);
			}
 			gf_sc_texture_disable(txh);
			tr_state->mesh_num_textures = 0;
			return;
		}
	}

	/*otherwise use glDrawPixels*/
#if !defined(GPAC_USE_OGL_ES) && !defined(GPAC_USE_TINYGL)
	data = gf_sc_texture_get_data(txh, &format);
	if (!data) return;

	x = INT2FIX(txh->width) / -2;
	y = INT2FIX(txh->height) / 2;

	{
	Fixed g[16];

	sx = bmp_scale_x; if (sx<0) sx = FIX_ONE;
	sy = bmp_scale_y; if (sy<0) sy = FIX_ONE;
	compositor_adjust_scale(txh->owner, &sx, &sy);

	/*add top level scale if any*/
	sx = gf_mulfix(sx, compositor->scale_x);
	sy = gf_mulfix(sy, compositor->scale_y);

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

