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



#include "visual_manager.h"
#include "texturing.h"
#include "nodes_stacks.h"
#include <gpac/options.h>

#ifndef GPAC_DISABLE_3D

#ifdef GPAC_USE_TINYGL
#include <GL/oscontext.h>
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
	if (!compositor->has_size_info || compositor->inherit_type_3d) {
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
	evt.setup.back_buffer = GF_TRUE;
	evt.setup.disable_vsync = compositor->bench_mode ? GF_TRUE : GF_FALSE;
#ifdef GPAC_USE_TINYGL
	evt.setup.use_opengl = GF_FALSE;
#else
	evt.setup.use_opengl = GF_TRUE;
	compositor->is_opengl = GF_TRUE;
#endif

	if (compositor->video_out->ProcessEvent(compositor->video_out, &evt)<0) {
		gf_sc_reset_graphics(compositor);
		return GF_OK;
	}
	if (evt.setup.use_opengl) {
		gf_opengl_init();
	}

#if defined(GPAC_USE_TINYGL)
	{
		u32 bpp;
		GF_VideoSurface bb;
		GF_Err e = compositor->video_out->LockBackBuffer(compositor->video_out, &bb, 1);
		if (e==GF_OK) {
			switch (bb.pixel_format) {
			case GF_PIXEL_RGBX:
			case GF_PIXEL_ARGB:
				bpp = 32;
				break;
			case GF_PIXEL_RGB:
			case GF_PIXEL_BGR:
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
			if (e==GF_OK) {
				compositor->tgl_ctx = ostgl_create_context(bb.width, bb.height, bpp, &bb.video_buffer, 1);
				if (compositor->tgl_ctx) ostgl_make_current(compositor->tgl_ctx, 0);
			}
			compositor->video_out->LockBackBuffer(compositor->video_out, &bb, 0);
		}
	}
#endif

	return GF_OK;
}


GF_Camera *compositor_3d_get_camera(GF_Compositor *compositor)
{
#ifndef GPAC_DISABLE_VRML
	if (compositor->active_layer) {
		return compositor_layer3d_get_camera(compositor->active_layer);
	}
#endif
	if (compositor->visual->type_3d)
		return &compositor->visual->camera;
	return NULL;
}

void compositor_3d_reset_camera(GF_Compositor *compositor)
{
	GF_Camera *cam = compositor_3d_get_camera(compositor);
	if (cam) {
		camera_reset_viewpoint(cam, GF_TRUE);
		gf_sc_invalidate(compositor, NULL);
	}
	if (compositor->active_layer) gf_node_dirty_set(compositor->active_layer, 0, GF_TRUE);
}

void compositor_3d_draw_bitmap(Drawable *stack, DrawAspect2D *asp, GF_TraverseState *tr_state, Fixed width, Fixed height, Fixed bmp_scale_x, Fixed bmp_scale_y)
{
	u8 alpha;
	GF_TextureHandler *txh;
	GF_Compositor *compositor = tr_state->visual->compositor;

	if (!asp->fill_texture)
		return;
	txh = asp->fill_texture;
	if (!txh || !txh->tx_io || !txh->width || !txh->height)
		return;

	if (((txh->pixelformat==GF_PIXEL_RGBD) || (txh->pixelformat==GF_PIXEL_YUVD))) {
		if (compositor->depth_gl_type) {
			if (txh->data && gf_sc_texture_convert(txh) )
				visual_3d_point_sprite(tr_state->visual, stack, txh, tr_state);
			return;
		}
	}

	alpha = GF_COL_A(asp->fill_color);
	/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
	if (!alpha) alpha = GF_COL_A(asp->line_color);

	visual_3d_set_state(tr_state->visual, V3D_STATE_LIGHT, GF_FALSE);
	visual_3d_enable_antialias(tr_state->visual, GF_FALSE);

	visual_3d_set_material_2d_argb(tr_state->visual, GF_COL_ARGB(alpha, 0xFF, 0xFF, 0xFF));
		
	if (alpha && (alpha != 0xFF)) {
		gf_sc_texture_set_blend_mode(txh, TX_MODULATE);
	} else if (gf_sc_texture_is_transparent(txh)) {
		gf_sc_texture_set_blend_mode(txh, TX_REPLACE);
	} else {
		visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, GF_FALSE);
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
				if (txh->stream) {
					mesh_new_rectangle_ex(stack->mesh, size, NULL, txh->stream->flip, txh->stream->rotate);
				} else {
					mesh_new_rectangle(stack->mesh, size, NULL, GF_FALSE);
				}
			}
		}
		if (stack->mesh) {
#ifdef GF_SR_USE_DEPTH
			if (tr_state->depth_offset) {
				GF_Matrix mx;
				Fixed offset;
				Fixed disp_depth = (compositor->dispdepth<0) ? INT2FIX(tr_state->visual->height) : INT2FIX(compositor->dispdepth);
				if (disp_depth) {
					GF_Matrix bck_mx;
					if (!tr_state->pixel_metrics) disp_depth = gf_divfix(disp_depth, tr_state->min_hsize);
					gf_mx_init(mx);
					/*add recalibration by the scene*/
					offset = tr_state->depth_offset;
					if (tr_state->visual->depth_vp_range) {
						offset = gf_divfix(offset, tr_state->visual->depth_vp_range/2);
					}
					gf_mx_add_translation(&mx, 0, 0, gf_mulfix(offset, disp_depth/2) );

					gf_mx_copy(bck_mx, tr_state->model_matrix);
					gf_mx_add_matrix(&tr_state->model_matrix, &mx);
					visual_3d_mesh_paint(tr_state, stack->mesh);
					gf_mx_copy(tr_state->model_matrix, bck_mx);
				} else {
					visual_3d_mesh_paint(tr_state, stack->mesh);
				}
			} else
#endif
				visual_3d_mesh_paint(tr_state, stack->mesh);
		}
		gf_sc_texture_disable(txh);
		tr_state->mesh_num_textures = 0;
	}
}

#endif

