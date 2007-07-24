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


GF_Err render_2d_get_video_access(GF_VisualManager *surf)
{
	GF_Err e;
	Render *sr = surf->render;

	if (!surf->raster_surface) return GF_BAD_PARAM;
	sr->hw_locked = 0;
	e = GF_IO_ERR;
	
	/*try from device*/
	if (sr->compositor->r2d->surface_attach_to_device && sr->compositor->video_out->LockOSContext) {
		sr->hw_context = sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 1);
		if (sr->hw_context) {
			e = sr->compositor->r2d->surface_attach_to_device(surf->raster_surface, sr->hw_context, sr->cur_width, sr->cur_height);
			if (!e) {
				surf->is_attached = 1;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render] Video surface handle attached to raster\n"));
				return GF_OK;
			}
			sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 0);
			GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render] Cannot attach video surface handle to raster: %s\n", gf_error_to_string(e) ));
		}
	}
	
	/*TODO - collect hw accelerated blit routines if any*/

	if (sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 1)==GF_OK) {
		sr->hw_locked = 1;
		e = sr->compositor->r2d->surface_attach_to_buffer(surf->raster_surface, sr->hw_surface.video_buffer, 
							sr->hw_surface.width, 
							sr->hw_surface.height,
							sr->hw_surface.pitch,
							(GF_PixelFormat) sr->hw_surface.pixel_format);
		if (!e) {
			surf->is_attached = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render] Video surface memory attached to raster\n"));
			return GF_OK;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Render] Cannot attach video surface memory to raster: %s\n", gf_error_to_string(e) ));
		sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 0);
	}
	sr->hw_locked = 0;
	surf->is_attached = 0;
	return e;	
}

void render_2d_release_video_access(GF_VisualManager *surf)
{
	Render *sr = surf->render;
	if (surf->is_attached) {
		sr->compositor->r2d->surface_detach(surf->raster_surface);
		surf->is_attached = 0;
	}
	if (sr->hw_context) {
		sr->compositor->video_out->LockOSContext(sr->compositor->video_out, 0);
		sr->hw_context = NULL;
	} else if (sr->hw_locked) {
		sr->compositor->video_out->LockBackBuffer(sr->compositor->video_out, &sr->hw_surface, 0);
		sr->hw_locked = 0;
	}
}

Bool render_2d_pixel_format_supported(GF_VisualManager *surf, u32 pixel_format)
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

void render_2d_draw_bitmap(GF_VisualManager *surf, struct _gf_sr_texture_handler *txh, GF_IRect *clip, GF_Rect *unclip, u8 alpha, u32 *col_key, GF_ColorMatrix *cmat)
{
	GF_VideoSurface video_src;
	Fixed w_scale, h_scale, tmp;
	GF_Err e;
	Bool use_soft_stretch;
	GF_Window src_wnd, dst_wnd;
	u32 start_x, start_y, cur_width, cur_height;
	GF_IRect clipped_final = *clip;
	GF_Rect final = *unclip;

	if (!txh->data) return;

	if (!surf->render->compositor->has_size_info && !(surf->render->compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) 
		&& (surf->render->compositor->override_size_flags & 1) 
		&& !(surf->render->compositor->override_size_flags & 2) 
		) {
		if ( (surf->render->compositor->scene_width < txh->width) 
			|| (surf->render->compositor->scene_height < txh->height)) {
			surf->render->compositor->scene_width = txh->width;
			surf->render->compositor->scene_height = txh->height;
			surf->render->compositor->msg_type |= GF_SR_CFG_OVERRIDE_SIZE;
			return;
		}
	}
	
	/*this should never happen but we check for float rounding safety*/
	if (final.width<=0 || final.height <=0) return;

	w_scale = final.width / txh->width;
	h_scale = final.height / txh->height;

	/*take care of pixel rounding for odd width/height and make sure we strictly draw in the clipped bounds*/
	cur_width = surf->render->cur_width;
	cur_height = surf->render->cur_height;

	if (surf->center_coords) {
		if (cur_width % 2) {
			clipped_final.x += (cur_width-1) / 2;
			final.x += INT2FIX( (cur_width-1) / 2 );
		} else {
			clipped_final.x += cur_width / 2;
			final.x += INT2FIX( cur_width / 2 );
		}
		if (cur_height % 2) {
			clipped_final.y = (cur_height-1) / 2 - clipped_final.y;
			final.y = INT2FIX( (cur_height - 1) / 2) - final.y;
		} else {
			clipped_final.y = cur_height/ 2 - clipped_final.y;
			final.y = INT2FIX( cur_height / 2) - final.y;
		}
	} else {
		final.x -= surf->render->vp_x;
		clipped_final.x -= surf->render->vp_x;
		final.y -= surf->render->vp_y + final.height;
		clipped_final.y -= surf->render->vp_y + clipped_final.height;
	}

	/*make sure we lie in the final rect (this is needed for directRender mode)*/
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
	if (clipped_final.x + clipped_final.width > (s32) cur_width) {
		clipped_final.width = cur_width - clipped_final.x;
		clipped_final.x = cur_width - clipped_final.width;
	}
	if (clipped_final.y + clipped_final.height > (s32) cur_height) {
		clipped_final.height = cur_height - clipped_final.y;
		clipped_final.y = cur_height - clipped_final.height;
	}
	/*needed in direct rendering since clipping is not performed*/
	if (clipped_final.width<=0 || clipped_final.height <=0) 
		return;

	/*compute X offset in src bitmap*/
	start_x = 0;
	tmp = INT2FIX(clipped_final.x);
	if (tmp >= final.x)
		start_x = FIX2INT( gf_divfix(tmp - final.x, w_scale) );


	/*compute Y offset in src bitmap*/
	start_y = 0;
	tmp = INT2FIX(clipped_final.y);
	if (tmp >= final.y)
		start_y = FIX2INT( gf_divfix(tmp - final.y, h_scale) );
	
	/*add AR offset */
	clipped_final.x += surf->render->vp_x;
	clipped_final.y += surf->render->vp_y;

	dst_wnd.x = (u32) clipped_final.x;
	dst_wnd.y = (u32) clipped_final.y;
	dst_wnd.w = (u32) clipped_final.width;
	dst_wnd.h = (u32) clipped_final.height;

	src_wnd.w = FIX2INT( gf_divfix(INT2FIX(dst_wnd.w), w_scale) );
	src_wnd.h = FIX2INT( gf_divfix(INT2FIX(dst_wnd.h), h_scale) );
	if (src_wnd.w>txh->width) src_wnd.w=txh->width;
	if (src_wnd.h>txh->height) src_wnd.h=txh->height;
	
	src_wnd.x = start_x;
	src_wnd.y = start_y;


	if (!src_wnd.w || !src_wnd.h) return;
	/*make sure we lie in src bounds*/
	if (src_wnd.x + src_wnd.w>txh->width) src_wnd.w = txh->width - src_wnd.x;
	if (src_wnd.y + src_wnd.h>txh->height) src_wnd.h = txh->height - src_wnd.y;

	use_soft_stretch = 1;
	if (!cmat && (alpha==0xFF) && surf->render->compositor->video_out->Blit) {
		u32 hw_caps = surf->render->compositor->video_out->hw_caps;

		switch (txh->pixelformat) {
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_BGR_24:
			use_soft_stretch = 0;
			break;
		case GF_PIXEL_YV12:
		case GF_PIXEL_IYUV:
		case GF_PIXEL_I420:
			if (surf->render->enable_yuv_hw && (hw_caps & GF_VIDEO_HW_HAS_YUV))
				use_soft_stretch = 0;
			break;
		default:
			break;
		}
		if (col_key && (GF_COL_A(*col_key) || !(hw_caps & GF_VIDEO_HW_HAS_COLOR_KEY))) use_soft_stretch = 1;
	}

	/*most graphic cards can't perform bliting on locked surface - force unlock by releasing the hardware*/
	visual_2d_release_raster(surf);

	video_src.height = txh->height;
	video_src.width = txh->width;
	video_src.pitch = txh->stride;
	video_src.pixel_format = txh->pixelformat;
	video_src.video_buffer = txh->data;
	if (!use_soft_stretch) {
		e = surf->render->compositor->video_out->Blit(surf->render->compositor->video_out, &video_src, &src_wnd, &dst_wnd, col_key);
		/*HW pb, try soft*/
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RENDER, ("[Renderer] Error during hardware blit - trying with soft one\n"));
			use_soft_stretch = 1;
		}
	}
	if (use_soft_stretch) {
		GF_VideoSurface backbuffer;
		e = surf->render->compositor->video_out->LockBackBuffer(surf->render->compositor->video_out, &backbuffer, 1);
		gf_stretch_bits(&backbuffer, &video_src, &dst_wnd, &src_wnd, 0, alpha, 0, col_key, cmat);
		e = surf->render->compositor->video_out->LockBackBuffer(surf->render->compositor->video_out, &backbuffer, 0);
	}
	visual_2d_init_raster(surf);
}


GF_Err render_2d_set_aspect_ratio(Render *sr)
{
	Double ratio;
	GF_Event evt;
	Fixed scaleX, scaleY;

/*
	if (!sr->compositor->scene_height || !sr->compositor->scene_width) return GF_OK;
	if (!sr->compositor->height || !sr->compositor->width) return GF_OK;
*/

	sr->cur_width = sr->compositor->scene_width;
	sr->cur_height = sr->compositor->scene_height;
	sr->vp_x = sr->vp_y = 0;

	/*force complete clean*/
	sr->traverse_state->invalidate_all = 1;

	if (!sr->compositor->has_size_info && !(sr->compositor->override_size_flags & 2) ) {
		sr->cur_width = sr->compositor->width;
		sr->cur_height = sr->compositor->height;
		sr->visual->width = sr->cur_width;
		sr->visual->height = sr->cur_height;
		render_set_aspect_ratio_scaling(sr, FIX_ONE, FIX_ONE);
		/*and resize hardware surface*/
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.width = sr->cur_width;
		evt.setup.height = sr->cur_height;
		evt.setup.opengl_mode = 0;
		return sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
	}
	sr->vp_width = sr->compositor->width;
	sr->vp_height = sr->compositor->height;

	switch (sr->compositor->aspect_ratio) {
	case GF_ASPECT_RATIO_FILL_SCREEN:
		break;
	case GF_ASPECT_RATIO_16_9:
		sr->vp_width = sr->compositor->width;
		sr->vp_height = 9 * sr->compositor->width / 16;
		if (sr->vp_height>sr->compositor->height) {
			sr->vp_height = sr->compositor->height;
			sr->vp_width = 16 * sr->compositor->height / 9;
		}
		break;
	case GF_ASPECT_RATIO_4_3:
		sr->vp_width = sr->compositor->width;
		sr->vp_height = 3 * sr->compositor->width / 4;
		if (sr->vp_height>sr->compositor->height) {
			sr->vp_height = sr->compositor->height;
			sr->vp_width = 4 * sr->compositor->height / 3;
		}
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

	scaleX = gf_divfix(INT2FIX(sr->vp_width), INT2FIX(sr->compositor->scene_width));
	scaleY = gf_divfix(INT2FIX(sr->vp_height), INT2FIX(sr->compositor->scene_height));

	if (!sr->scalable_zoom) {
		sr->cur_width = sr->compositor->scene_width;
		sr->cur_height = sr->compositor->scene_height;
		sr->vp_width = FIX2INT(gf_divfix(INT2FIX(sr->compositor->width), scaleX));
		sr->vp_height = FIX2INT(gf_divfix(INT2FIX(sr->compositor->height), scaleY));

		sr->vp_x = (sr->vp_width - sr->cur_width) / 2;
		sr->vp_y = (sr->vp_height - sr->cur_height) / 2;

		scaleX = scaleY = FIX_ONE;
	} else {
		sr->cur_width = sr->vp_width;
		sr->cur_height = sr->vp_height;
		sr->vp_width = sr->compositor->width;
		sr->vp_height = sr->compositor->height;
	}
	sr->visual->width = sr->cur_width;
	sr->visual->height = sr->cur_height;
	/*set scale factor*/
	render_set_aspect_ratio_scaling(sr, scaleX, scaleY);
	gf_sr_invalidate(sr->compositor, NULL);
	/*and resize hardware surface*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = sr->vp_width;
	evt.setup.height = sr->vp_height;
	evt.setup.opengl_mode = 0;
	return sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
}

void render_2d_set_user_transform(Render *sr, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize) 
{
	Fixed ratio;
	Fixed old_tx, old_ty, old_z;
	
	gf_sr_lock(sr->compositor, 1);
	old_tx = tx;
	old_ty = ty;
	old_z = sr->zoom;

	if (zoom <= 0) zoom = FIX_ONE/1000;
	sr->trans_x = tx;
	sr->trans_y = ty;

	if (zoom != sr->zoom) {
		ratio = gf_divfix(zoom, sr->zoom);
		sr->trans_x = gf_mulfix(sr->trans_x, ratio);
		sr->trans_y = gf_mulfix(sr->trans_y, ratio);
		sr->zoom = zoom;

		/*recenter visual*/
		if (!sr->visual->center_coords) {
			Fixed c_x, c_y, nc_x, nc_y;
			c_x = INT2FIX(sr->compositor->width/2);
			nc_y = c_y = INT2FIX(sr->compositor->height/2);
			nc_x = gf_mulfix(c_x, ratio);
			nc_y = gf_mulfix(c_y, ratio);
			sr->trans_x -= (nc_x-c_x);
			sr->trans_y -= (nc_y-c_y);
		}
	}
	gf_mx2d_init(sr->traverse_state->transform);
	gf_mx2d_add_scale(&sr->traverse_state->transform, gf_mulfix(sr->zoom,sr->scale_x), gf_mulfix(sr->zoom,sr->scale_y));
	gf_mx2d_add_translation(&sr->traverse_state->transform, sr->trans_x, sr->trans_y);
	if (sr->rotation) gf_mx2d_add_rotation(&sr->traverse_state->transform, 0, 0, sr->rotation);

	if (!sr->visual->center_coords) {
		gf_mx2d_add_translation(&sr->traverse_state->transform, INT2FIX(sr->vp_x), INT2FIX(sr->vp_y));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Changing Zoom (%g) and Pan (%g %g)\n", FIX2FLT(sr->zoom), FIX2FLT(sr->trans_x) , FIX2FLT(sr->trans_y)));


	sr->compositor->draw_next_frame = 1;
	sr->traverse_state->invalidate_all = 1;

#ifndef GPAC_DISABLE_SVG
	if (sr->root_uses_dom_events) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.prev_scale = sr->scale_x*old_z;
		evt.new_scale = sr->scale_x*sr->zoom;

		if (is_resize) {
			evt.type = GF_EVENT_RESIZE;
		} else if (evt.prev_scale == evt.new_scale) {
			/*cannot get params for scroll events*/
			evt.type = GF_EVENT_SCROLL;
		} else {
			evt.screen_rect.x = INT2FIX(sr->vp_x);
			evt.screen_rect.y = INT2FIX(sr->vp_y);
			evt.screen_rect.width = INT2FIX(sr->cur_width);
			evt.screen_rect.height = INT2FIX(sr->cur_height);
			evt.prev_translate.x = old_tx;
			evt.prev_translate.y = old_ty;
			evt.new_translate.x = sr->trans_x;
			evt.new_translate.y = sr->trans_y;
			evt.type = GF_EVENT_ZOOM;
		}
		gf_dom_event_fire(gf_sg_get_root_node(sr->compositor->scene), NULL, &evt);
	}
#endif

	gf_sr_lock(sr->compositor, 0);
}

GF_Rect render_2d_update_clipper(GF_TraverseState *tr_state, GF_Rect this_clip, Bool *need_restore, GF_Rect *original, Bool for_layer)
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

