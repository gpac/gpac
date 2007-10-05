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
#include <gpac/options.h>


GF_Err compositor_2d_get_video_access(GF_VisualManager *visual)
{
	GF_Err e;
	GF_Compositor *compositor = visual->compositor;

	if (!visual->raster_surface) return GF_BAD_PARAM;
	compositor->hw_locked = 0;
	e = GF_IO_ERR;
	
	/*try from device*/
	if (compositor->rasterizer->surface_attach_to_device && compositor->video_out->LockOSContext) {
		compositor->hw_context = compositor->video_out->LockOSContext(compositor->video_out, 1);
		if (compositor->hw_context) {
			e = compositor->rasterizer->surface_attach_to_device(visual->raster_surface, compositor->hw_context, compositor->vp_width, compositor->vp_height);
			if (!e) {
				visual->is_attached = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Video surface handle attached to raster\n"));
				return GF_OK;
			}
			compositor->video_out->LockOSContext(compositor->video_out, 0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Cannot attach video surface handle to raster: %s\n", gf_error_to_string(e) ));
		}
	}
	
	/*TODO - collect hw accelerated blit routines if any*/

	if (compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, 1)==GF_OK) {
		compositor->hw_locked = 1;
		e = compositor->rasterizer->surface_attach_to_buffer(visual->raster_surface, compositor->hw_surface.video_buffer, 
							compositor->hw_surface.width, 
							compositor->hw_surface.height,
							compositor->hw_surface.pitch,
							(GF_PixelFormat) compositor->hw_surface.pixel_format);
		if (!e) {
			visual->is_attached = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Video surface memory attached to raster\n"));
			return GF_OK;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Cannot attach video surface memory to raster: %s\n", gf_error_to_string(e) ));
		compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, 0);
	}
	compositor->hw_locked = 0;
	visual->is_attached = 0;
	return e;	
}

void compositor_2d_release_video_access(GF_VisualManager *visual)
{
	GF_Compositor *compositor = visual->compositor;
	if (visual->is_attached) {
		compositor->rasterizer->surface_detach(visual->raster_surface);
		visual->is_attached = 0;
	}
	if (compositor->hw_context) {
		compositor->video_out->LockOSContext(compositor->video_out, 0);
		compositor->hw_context = NULL;
	} else if (compositor->hw_locked) {
		compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, 0);
		compositor->hw_locked = 0;
	}
}

Bool compositor_2d_pixel_format_supported(GF_VisualManager *visual, u32 pixel_format)
{
	switch (pixel_format) {
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
		return 1;
	/*the rest has to be displayed through brush for now, we only use YUV and RGB pool*/
	default:
		return 0;
	}
}

void compositor_2d_draw_bitmap(GF_VisualManager *visual, struct _gf_sc_texture_handler *txh, GF_IRect *clip, GF_Rect *unclip, u8 alpha, u32 *col_key, GF_ColorMatrix *cmat)
{
	GF_VideoSurface video_src;
	Fixed w_scale, h_scale, tmp;
	GF_Err e;
	Bool use_soft_stretch;
	GF_Window src_wnd, dst_wnd;
	u32 output_width, output_height;
	GF_IRect clipped_final = *clip;
	GF_Rect final = *unclip;

	if (!txh->data) return;

	if (!visual->compositor->has_size_info && !(visual->compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) 
		&& (visual->compositor->override_size_flags & 1) 
		&& !(visual->compositor->override_size_flags & 2) 
		) {
		if ( (visual->compositor->scene_width < txh->width) 
			|| (visual->compositor->scene_height < txh->height)) {
			visual->compositor->scene_width = txh->width;
			visual->compositor->scene_height = txh->height;
			visual->compositor->msg_type |= GF_SR_CFG_OVERRIDE_SIZE;
			return;
		}
	}
	
	/*this should never happen but we check for float rounding safety*/
	if (final.width<=0 || final.height <=0) return;

	w_scale = final.width / txh->width;
	h_scale = final.height / txh->height;

	/*use entire video surface for un-centering coord system*/
	output_width = visual->compositor->vp_width;
	output_height = visual->compositor->vp_height;

	/*take care of pixel rounding for odd width/height and make sure we strictly draw in the clipped bounds*/
	if (visual->center_coords) {
		if (0 && output_width % 2) {
			clipped_final.x += (output_width-1) / 2;
			final.x += INT2FIX( (output_width-1) / 2 );
		} else {
			clipped_final.x += output_width / 2;
			final.x += INT2FIX( output_width / 2 );
		}
		if (0 && output_height % 2) {
			clipped_final.y = (output_height-1) / 2 - clipped_final.y;
			final.y = INT2FIX( (output_height - 1) / 2) - final.y;
		} else {
			clipped_final.y = output_height/ 2 - clipped_final.y;
			final.y = INT2FIX( output_height / 2) - final.y;
		}
	} else {
		final.x -= visual->compositor->vp_x;
		clipped_final.x -= visual->compositor->vp_x;
		final.y -= visual->compositor->vp_y + final.height;
		clipped_final.y -= visual->compositor->vp_y + clipped_final.height;
	}

	/*make sure we lie in the final rect (this is needed for directdraw mode)*/
	if (clipped_final.x<0) {
		clipped_final.width += clipped_final.x;
		clipped_final.x = 0;
		if (clipped_final.width <= 0) return;
	}
	if (clipped_final.y<0) {
		clipped_final.height += clipped_final.y;
		clipped_final.y = 0;
		if (clipped_final.height <= 0) return;
	}
	if (clipped_final.x + clipped_final.width > (s32) output_width) {
		clipped_final.width = output_width - clipped_final.x;
		clipped_final.x = output_width - clipped_final.width;
	}
	if (clipped_final.y + clipped_final.height > (s32) output_height) {
		clipped_final.height = output_height - clipped_final.y;
		clipped_final.y = output_height - clipped_final.height;
	}
	/*needed in direct drawing since clipping is not performed*/
	if (clipped_final.width<=0 || clipped_final.height <=0) 
		return;

	/*set dest window*/
	dst_wnd.x = (u32) clipped_final.x;
	dst_wnd.y = (u32) clipped_final.y;
	dst_wnd.w = (u32) clipped_final.width;
	dst_wnd.h = (u32) clipped_final.height;

	/*compute SRC window*/
	src_wnd.x = src_wnd.y = 0;
	src_wnd.w = FIX2INT( gf_divfix(INT2FIX(clip->width), w_scale)  + FIX_ONE/2 );
	src_wnd.h = FIX2INT( gf_divfix(INT2FIX(clip->height), h_scale)  + FIX_ONE/2 );

	if (src_wnd.w>txh->width) src_wnd.w=txh->width;
	if (src_wnd.h>txh->height) src_wnd.h=txh->height;

	tmp = INT2FIX(clipped_final.x) - final.x /*- INT2FIX(visual->compositor->vp_x)*/;
	if (tmp>=0) src_wnd.x = FIX2INT( gf_divfix( tmp, w_scale) + FIX_ONE/2 );

	tmp = INT2FIX(clipped_final.y) - final.y /*+ INT2FIX(visual->compositor->vp_y)*/;
	if (tmp>=0) src_wnd.y = FIX2INT( gf_divfix(tmp, h_scale) + FIX_ONE/2 );

	if (!src_wnd.w || !src_wnd.h) return;
	/*make sure we lie in src bounds*/
	if (src_wnd.x + src_wnd.w>txh->width) src_wnd.w = txh->width - src_wnd.x;
	if (src_wnd.y + src_wnd.h>txh->height) src_wnd.h = txh->height - src_wnd.y;

	/*can we use hardware blitter ?*/
	use_soft_stretch = 1;
	if (!visual->compositor->disable_partial_hw_blit 
		|| ((src_wnd.w==txh->width) && (src_wnd.h==txh->height) )) {
		if (!cmat && (alpha==0xFF) && visual->compositor->video_out->Blit) {
			u32 hw_caps = visual->compositor->video_out->hw_caps;

			switch (txh->pixelformat) {
			case GF_PIXEL_RGB_24:
			case GF_PIXEL_BGR_24:
				use_soft_stretch = 0;
				break;
			case GF_PIXEL_YV12:
			case GF_PIXEL_IYUV:
			case GF_PIXEL_I420:
				if (visual->compositor->enable_yuv_hw && (hw_caps & GF_VIDEO_HW_HAS_YUV))
					use_soft_stretch = 0;
				break;
			default:
				break;
			}
			if (col_key && (GF_COL_A(*col_key) || !(hw_caps & GF_VIDEO_HW_HAS_COLOR_KEY))) use_soft_stretch = 1;
		}
	}

	/*most graphic cards can't perform bliting on locked surface - force unlock by releasing the hardware*/
	visual_2d_release_raster(visual);

	video_src.height = txh->height;
	video_src.width = txh->width;
	video_src.pitch = txh->stride;
	video_src.pixel_format = txh->pixelformat;
	video_src.video_buffer = txh->data;
	if (!use_soft_stretch) {
		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, col_key);
		/*HW pb, try soft*/
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Error during hardware blit - trying with soft one\n"));
			use_soft_stretch = 1;
		}
	}
	if (use_soft_stretch) {
		GF_VideoSurface backbuffer;
		e = visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, 1);
		gf_stretch_bits(&backbuffer, &video_src, &dst_wnd, &src_wnd, 0, alpha, 0, col_key, cmat);
		e = visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, 0);
	}
	visual_2d_init_raster(visual);
}


GF_Err compositor_2d_set_aspect_ratio(GF_Compositor *compositor)
{
	Double ratio;
	GF_Event evt;
	Fixed scaleX, scaleY;

/*
	if (!compositor->scene_height || !compositor->scene_width) return GF_OK;
	if (!compositor->display_height || !compositor->display_width) return GF_OK;
*/

	compositor->output_width = compositor->scene_width;
	compositor->output_height = compositor->scene_height;
	compositor->vp_x = compositor->vp_y = 0;

	/*force complete clean*/
	compositor->traverse_state->invalidate_all = 1;

	if (!compositor->has_size_info && !(compositor->override_size_flags & 2) ) {
		compositor->output_width = compositor->display_width;
		compositor->output_height = compositor->display_height;
		compositor->visual->width = compositor->output_width;
		compositor->visual->height = compositor->output_height;
		compositor_set_ar_scale(compositor, FIX_ONE, FIX_ONE);
		/*and resize hardware surface*/
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.width = compositor->output_width;
		evt.setup.height = compositor->output_height;
		evt.setup.opengl_mode = 0;
		return compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	}
	compositor->vp_width = compositor->display_width;
	compositor->vp_height = compositor->display_height;

	switch (compositor->aspect_ratio) {
	case GF_ASPECT_RATIO_FILL_SCREEN:
		break;
	case GF_ASPECT_RATIO_16_9:
		compositor->vp_width = compositor->display_width;
		compositor->vp_height = 9 * compositor->display_width / 16;
		if (compositor->vp_height>compositor->display_height) {
			compositor->vp_height = compositor->display_height;
			compositor->vp_width = 16 * compositor->display_height / 9;
		}
		break;
	case GF_ASPECT_RATIO_4_3:
		compositor->vp_width = compositor->display_width;
		compositor->vp_height = 3 * compositor->display_width / 4;
		if (compositor->vp_height>compositor->display_height) {
			compositor->vp_height = compositor->display_height;
			compositor->vp_width = 4 * compositor->display_height / 3;
		}
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

	scaleX = gf_divfix(INT2FIX(compositor->vp_width), INT2FIX(compositor->scene_width));
	scaleY = gf_divfix(INT2FIX(compositor->vp_height), INT2FIX(compositor->scene_height));

	if (!compositor->scalable_zoom) {
		compositor->output_width = compositor->scene_width;
		compositor->output_height = compositor->scene_height;
		compositor->vp_width = FIX2INT(gf_divfix(INT2FIX(compositor->display_width), scaleX));
		compositor->vp_height = FIX2INT(gf_divfix(INT2FIX(compositor->display_height), scaleY));

		compositor->vp_x = (compositor->vp_width - compositor->output_width) / 2;
		compositor->vp_y = (compositor->vp_height - compositor->output_height) / 2;

		scaleX = scaleY = FIX_ONE;
	} else {
		compositor->output_width = compositor->vp_width;
		compositor->output_height = compositor->vp_height;
		compositor->vp_width = compositor->display_width;
		compositor->vp_height = compositor->display_height;
	}
	compositor->visual->width = compositor->output_width;
	compositor->visual->height = compositor->output_height;
	/*set scale factor*/
	compositor_set_ar_scale(compositor, scaleX, scaleY);
	gf_sc_invalidate(compositor, NULL);
	/*and resize hardware surface*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = compositor->vp_width;
	evt.setup.height = compositor->vp_height;
	evt.setup.opengl_mode = 0;
	return compositor->video_out->ProcessEvent(compositor->video_out, &evt);
}

void compositor_2d_set_user_transform(GF_Compositor *compositor, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize) 
{
	Fixed ratio;
	Fixed old_tx, old_ty, old_z;
	
	gf_sc_lock(compositor, 1);
	old_tx = tx;
	old_ty = ty;
	old_z = compositor->zoom;

	if (zoom <= 0) zoom = FIX_ONE/1000;
	compositor->trans_x = tx;
	compositor->trans_y = ty;

	if (zoom != compositor->zoom) {
		ratio = gf_divfix(zoom, compositor->zoom);
		compositor->trans_x = gf_mulfix(compositor->trans_x, ratio);
		compositor->trans_y = gf_mulfix(compositor->trans_y, ratio);
		compositor->zoom = zoom;

		/*recenter visual*/
		if (!compositor->visual->center_coords) {
			Fixed c_x, c_y, nc_x, nc_y;
			c_x = INT2FIX(compositor->display_width/2);
			nc_y = c_y = INT2FIX(compositor->display_height/2);
			nc_x = gf_mulfix(c_x, ratio);
			nc_y = gf_mulfix(c_y, ratio);
			compositor->trans_x -= (nc_x-c_x);
			compositor->trans_y -= (nc_y-c_y);
		}
	}
	gf_mx2d_init(compositor->traverse_state->transform);
	gf_mx2d_add_scale(&compositor->traverse_state->transform, gf_mulfix(compositor->zoom,compositor->scale_x), gf_mulfix(compositor->zoom,compositor->scale_y));

	gf_mx2d_add_translation(&compositor->traverse_state->transform, compositor->trans_x, compositor->trans_y);
	if (compositor->rotation) gf_mx2d_add_rotation(&compositor->traverse_state->transform, 0, 0, compositor->rotation);

	if (!compositor->visual->center_coords) {
		gf_mx2d_add_translation(&compositor->traverse_state->transform, INT2FIX(compositor->vp_x), INT2FIX(compositor->vp_y));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Changing Zoom (%g) and Pan (%g %g)\n", FIX2FLT(compositor->zoom), FIX2FLT(compositor->trans_x) , FIX2FLT(compositor->trans_y)));


	compositor->draw_next_frame = 1;
	compositor->traverse_state->invalidate_all = 1;

#ifndef GPAC_DISABLE_SVG
	if (compositor->root_uses_dom_events) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.prev_scale = compositor->scale_x*old_z;
		evt.new_scale = compositor->scale_x*compositor->zoom;

		if (is_resize) {
			evt.type = GF_EVENT_RESIZE;
		} else if (evt.prev_scale == evt.new_scale) {
			/*cannot get params for scroll events*/
			evt.type = GF_EVENT_SCROLL;
		} else {
			evt.screen_rect.x = INT2FIX(compositor->vp_x);
			evt.screen_rect.y = INT2FIX(compositor->vp_y);
			evt.screen_rect.width = INT2FIX(compositor->output_width);
			evt.screen_rect.height = INT2FIX(compositor->output_height);
			evt.prev_translate.x = old_tx;
			evt.prev_translate.y = old_ty;
			evt.new_translate.x = compositor->trans_x;
			evt.new_translate.y = compositor->trans_y;
			evt.type = GF_EVENT_ZOOM;
		}
		gf_dom_event_fire(gf_sg_get_root_node(compositor->scene), NULL, &evt);
	}
#endif

	gf_sc_lock(compositor, 0);
}

GF_Rect compositor_2d_update_clipper(GF_TraverseState *tr_state, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer)
{
	GF_Rect clip, orig;
	if (for_layer) {
		orig = tr_state->layer_clipper;
		*need_restore = tr_state->has_layer_clip;
	} else {
		orig = tr_state->clipper;
		*need_restore = tr_state->has_clip;
	}
	*original = orig;

	clip = this_clip;
	if (*need_restore) {
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			GF_Matrix mx;
			gf_mx_copy(mx, tr_state->model_matrix);
			gf_mx_inverse(&mx);
			gf_mx_apply_rect(&mx, &orig);
		} else 
#endif
		{
			GF_Matrix2D mx2d;
			gf_mx2d_copy(mx2d, tr_state->transform);
			gf_mx2d_inverse(&mx2d);
			gf_mx2d_apply_rect(&mx2d, &orig);
		}

		if (clip.x < orig.x) {
			clip.width -= (orig.x - clip.x);
			clip.x = orig.x;
		}
		if (clip.x + clip.width > orig.x + orig.width) {
			clip.width = orig.x + orig.width - clip.x;
		}
		if (clip.y > orig.y) {
			clip.height -= (clip.y - orig.y);
			clip.y = orig.y;
		}
		if (clip.y - clip.height < orig.y - orig.height) {
			clip.height = clip.y - orig.y + orig.height;
		}
	}
	if (for_layer) {
		tr_state->layer_clipper = clip;
		tr_state->has_layer_clip = 1;
	} else {
		tr_state->clipper = clip;
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			/*retranslate to world coords*/
			gf_mx_apply_rect(&tr_state->model_matrix, &tr_state->clipper);
			/*if 2D, also update with user zoom and translation*/
			if (!tr_state->camera->is_3D)
				gf_mx_apply_rect(&tr_state->camera->modelview, &tr_state->clipper);
		} else 
#endif
		
			gf_mx2d_apply_rect(&tr_state->transform, &tr_state->clipper);
		
		tr_state->has_clip = 1;
	}
	return clip;
}

