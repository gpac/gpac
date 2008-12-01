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
#include "nodes_stacks.h"
#include <gpac/options.h>

#ifdef GPAC_TRISCOPE_MODE
#include "../src/compositor/triscope_renoir/triscope_renoir.h"
#endif

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
#ifdef GPAC_TRISCOPE_MODE 
		/*CALL SNOUTPUT FOR RENOIR*/
		SetRenoirOutput(compositor);
#endif
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


static Bool compositor_2d_draw_bitmap_ex(GF_VisualManager *visual, GF_TextureHandler *txh, DrawableContext *ctx, GF_IRect *clip, GF_Rect *unclip, u8 alpha, GF_ColorKey *col_key, GF_TraverseState *tr_state, Bool force_soft_blt)
{
	GF_VideoSurface video_src;
	Fixed w_scale, h_scale, tmp;
	GF_Err e;
	Bool use_soft_stretch, use_blit, flush_video;
	u32 overlay_type;
	GF_Window src_wnd, dst_wnd;
	u32 output_width, output_height, hw_caps;
	GF_IRect clipped_final = *clip;
	GF_Rect final = *unclip;

	if (!txh->data) return 1;

	if (!visual->compositor->has_size_info && !(visual->compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) 
		&& (visual->compositor->override_size_flags & 1) 
		&& !(visual->compositor->override_size_flags & 2) 
		) {
		if ( (visual->compositor->scene_width < txh->width) 
			|| (visual->compositor->scene_height < txh->height)) {
			visual->compositor->scene_width = txh->width;
			visual->compositor->scene_height = txh->height;
			visual->compositor->msg_type |= GF_SR_CFG_OVERRIDE_SIZE;
			return 1;
		}
	}
	
	/*this should never happen but we check for float rounding safety*/
	if (final.width<=0 || final.height <=0) return 1;

	w_scale = final.width / txh->width;
	h_scale = final.height / txh->height;

	/*use entire video surface for un-centering coord system*/
	output_width = visual->compositor->vp_width;
	output_height = visual->compositor->vp_height;

	/*take care of pixel rounding for odd width/height and make sure we strictly draw in the clipped bounds*/
	if (visual->center_coords) {
		clipped_final.x += output_width / 2;
		final.x += INT2FIX( output_width / 2 );

		clipped_final.y = output_height/ 2 - clipped_final.y;
		final.y = INT2FIX( output_height / 2) - final.y;

	} else {
		final.y -= final.height;
		clipped_final.y -= clipped_final.height;
	}

	/*make sure we lie in the final rect (this is needed for directdraw mode)*/
	if (clipped_final.x<0) {
		clipped_final.width += clipped_final.x;
		clipped_final.x = 0;
		if (clipped_final.width <= 0) return 0;
	}
	if (clipped_final.y<0) {
		clipped_final.height += clipped_final.y;
		clipped_final.y = 0;
		if (clipped_final.height <= 0) return 0;
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
		return 0;

	/*set dest window*/
	dst_wnd.x = (u32) clipped_final.x;
	dst_wnd.y = (u32) clipped_final.y;
	dst_wnd.w = (u32) clipped_final.width;
	dst_wnd.h = (u32) clipped_final.height;


#ifdef GPAC_FIXED_POINT
#define ROUND_FIX(_v)	\
	_v = FIX2INT(tmp);
#else
#define ROUND_FIX(_v)	\
	_v = FIX2INT(tmp);	\
	tmp -= INT2FIX(_v);	\
	if (tmp>99*FIX_ONE/100) { _v++; tmp = 0; }	\
	if (ABS(tmp) > FIX_EPSILON) use_blit = 0;
#endif

	use_blit = 1;
	/*compute SRC window*/
	src_wnd.x = src_wnd.y = 0;
	tmp = gf_divfix(INT2FIX(clipped_final.x) - final.x, w_scale);
	if (tmp<0) tmp=0;
	ROUND_FIX(src_wnd.x);

	tmp = gf_divfix(INT2FIX(clipped_final.y) - final.y, h_scale);
	if (tmp<0) tmp=0;
	ROUND_FIX(src_wnd.y);

	tmp = gf_divfix(INT2FIX(clip->width), w_scale);
	ROUND_FIX(src_wnd.w);

	tmp = gf_divfix(INT2FIX(clip->height), h_scale);
	ROUND_FIX(src_wnd.h);
	
#undef ROUND_FIX

	if (src_wnd.w>txh->width) src_wnd.w=txh->width;
	if (src_wnd.h>txh->height) src_wnd.h=txh->height;

	if (!src_wnd.w || !src_wnd.h) return 1;
	/*make sure we lie in src bounds*/
	if (src_wnd.x + src_wnd.w>txh->width) src_wnd.w = txh->width - src_wnd.x;
	if (src_wnd.y + src_wnd.h>txh->height) src_wnd.h = txh->height - src_wnd.y;

	/*can we use hardware blitter ?*/
	hw_caps = visual->compositor->video_out->hw_caps;
	overlay_type = 0;
	flush_video = 0;
	use_soft_stretch = 1;
	if (!force_soft_blt) {

		/*avoid partial redraw that don't come close to src pixels with the bliter, this leads to ugly artefacts - 
		fall back to rasterizer*/
//		if (!(ctx->flags & CTX_TEXTURE_DIRTY) && !use_blit && (src_wnd.x || src_wnd.y) )
//			return 0;

		switch (txh->pixelformat) {
		case GF_PIXEL_RGB_24:
		case GF_PIXEL_BGR_24:
			use_soft_stretch = 0;
			break;
		case GF_PIXEL_YV12:
		case GF_PIXEL_IYUV:
		case GF_PIXEL_I420:
			if (hw_caps & GF_VIDEO_HW_HAS_YUV) use_soft_stretch = 0;
			else if (hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY) overlay_type = 1;
			break;
		default:
			break;
		}
		/*disable based on settings*/
		if (!visual->compositor->enable_yuv_hw
			|| (ctx->col_mat || (alpha!=0xFF) || !visual->compositor->video_out->Blit) 
			) {
			use_soft_stretch = 1;
			overlay_type = 0;
		}
		if (visual->compositor->disable_partial_hw_blit && ((src_wnd.w!=txh->width) || (src_wnd.h!=txh->height) )) {
			use_soft_stretch = 1;
		}

		/*disable HW color keying - not compatible with MPEG-4 MaterialKey*/
		if (col_key) {
			use_soft_stretch = 1;
			overlay_type = 0;
		}

		if (overlay_type) {
			/*no more than one overlay is supported at the current time*/
			if (visual->overlays) {
				ctx->drawable->flags &= ~DRAWABLE_IS_OVERLAY;
				overlay_type = 0;
			}
			/*direct draw or not last context: we must queue the overlay*/
			else if (tr_state->direct_draw || (ctx->next && ctx->next->drawable)) {
				overlay_type = 2;
			}
			/*OK we can overlay this video - if full display, don't flush*/
			if (overlay_type==1) {
				if (dst_wnd.w==visual->compositor->display_width) flush_video = 0;
				else if (dst_wnd.h==visual->compositor->display_height) flush_video = 0;
				else flush_video = visual->has_modif;
			}
			/*if no color keying, we cannot queue the overlay*/
			else if (!visual->compositor->video_out->overlay_color_key) {
				overlay_type = 0;
			}
		}

	}

	video_src.height = txh->height;
	video_src.width = txh->width;
	video_src.pitch = txh->stride;
	video_src.pixel_format = txh->pixelformat;
	video_src.video_buffer = txh->data;
	if (overlay_type) {
		if (overlay_type==2) {
			GF_IRect o_rc;
			GF_OverlayStack *ol, *first;

			/*queue overlay in order*/
			GF_SAFEALLOC(ol, GF_OverlayStack);
			ol->ctx = ctx;
			ol->dst = dst_wnd;
			ol->src = src_wnd;
			first = visual->overlays;
			if (first) {
				while (first->next) first = first->next;
				first->next = ol;
			} else {
				visual->overlays = ol;
			}

			if (visual->center_coords) {
				o_rc.x = dst_wnd.x - output_width/2;
				o_rc.y = output_height/2- dst_wnd.y;
			} else {
				o_rc.x = dst_wnd.x;
				o_rc.y = dst_wnd.y;
			}
			o_rc.width = dst_wnd.w;
			o_rc.height = dst_wnd.h;
			visual_2d_clear(visual, &o_rc, visual->compositor->video_out->overlay_color_key);
			visual->has_overlays = 1;
			/*mark drawable as overlay*/
			ctx->drawable->flags |= DRAWABLE_IS_OVERLAY;

			/*prevents this context from being removed in direct draw mode by requesting a new one
			but not allocating it*/
			if (tr_state->direct_draw) 
				visual_2d_get_drawable_context(visual);
			return 1;
		}
		/*top level overlay*/
		if (flush_video) {
			GF_Window rc;
			rc.x = rc.y = 0; 
			rc.w = visual->compositor->display_width;	
			rc.h = visual->compositor->display_height;

			visual_2d_release_raster(visual);
			visual->compositor->video_out->Flush(visual->compositor->video_out, &rc);
			visual_2d_init_raster(visual);
		}
		visual->compositor->skip_flush = 1;

		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, 1);
		if (!e) {
			/*mark drawable as overlay*/
			ctx->drawable->flags |= DRAWABLE_IS_OVERLAY;
			visual->has_overlays = 1;
			return 1;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Error during overlay blit - trying with soft one\n"));
		visual->compositor->skip_flush = 0;
	}
	
	/*most graphic cards can't perform bliting on locked surface - force unlock by releasing the hardware*/
	visual_2d_release_raster(visual);

	if (!use_soft_stretch) {
		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, 0);
		/*HW pb, try soft*/
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Error during hardware blit - trying with soft one\n"));
			use_soft_stretch = 1;
			if (!visual->compositor->video_memory) {
				GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor2D] Reconfiguring video output to use video memory\n"));
				visual->compositor->video_memory = 1;
				visual->compositor->root_visual_setup = 0;
			}
		}
	}
	if (use_soft_stretch) {
		GF_VideoSurface backbuffer;
		e = visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, 1);
		if (!e) {
			gf_stretch_bits(&backbuffer, &video_src, &dst_wnd, &src_wnd, 0, alpha, 0, col_key, ctx->col_mat);
			e = visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, 0);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Cannot lock back buffer - Error %s\n", gf_error_to_string(e) ));
		}
	}
	visual->has_modif = 1;
	visual_2d_init_raster(visual);
	return 1;
}



Bool compositor_2d_draw_bitmap(GF_VisualManager *visual, GF_TraverseState *tr_state, DrawableContext *ctx, GF_ColorKey *col_key)
{
	u8 alpha = 0xFF;
	

	if (!ctx->aspect.fill_texture) return 1;
	/*check if texture is ready*/
	if (!ctx->aspect.fill_texture->data) return 0;
	if (ctx->transform.m[0]<0) return 0;
	/*check if the <0 value is due to a flip in he scene description or 
	due to bifs<->svg... context switching*/
	if (ctx->transform.m[4]<0) {
		if (!(ctx->flags & CTX_FLIPED_COORDS)) return 0;
	} else {
		if (ctx->flags & CTX_FLIPED_COORDS) return 0;
	}
	if (ctx->transform.m[1] || ctx->transform.m[3]) return 0;
	if ((ctx->flags & CTX_HAS_APPEARANCE) && ctx->appear && ((M_Appearance*)ctx->appear)->textureTransform)
		return 0;

	alpha = GF_COL_A(ctx->aspect.fill_color);
	/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
	if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);
	
	ctx->aspect.fill_texture->flags |= GF_SR_TEXTURE_USED;
	if (!alpha) return 1;

	switch (ctx->aspect.fill_texture->pixelformat) {
	case GF_PIXEL_RGB_24:
	case GF_PIXEL_BGR_24:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_YV12:
	case GF_PIXEL_IYUV:
	case GF_PIXEL_I420:
	case GF_PIXEL_YUVA:
#ifdef GPAC_TRISCOPE_MODE
	case GF_PIXEL_RGBDS:	
#endif
		break;
	/*the rest has to be displayed through brush for now, we only use YUV and RGB pool*/
	default:
		return 0;
	}

	/*direct drawing, no clippers */
	if (tr_state->direct_draw) {
		
#ifdef GPAC_TRISCOPE_MODE
#ifdef TRISCOPE_DEBUG
   printf("\nEntering SetRenoirTexture...\n");
#endif
		return SetRenoirTexture(visual, ctx->aspect.fill_texture, ctx, &ctx->bi->clip, &ctx->bi->unclip, alpha, col_key, tr_state, 0);
		
#else 		
		return compositor_2d_draw_bitmap_ex(visual, ctx->aspect.fill_texture, ctx, &ctx->bi->clip, &ctx->bi->unclip, alpha, col_key, tr_state, 0);

#endif

	}
	/*draw bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<tr_state->visual->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (tr_state->visual->draw_node_index < tr_state->visual->to_redraw.opaque_node_index[i]) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &tr_state->visual->to_redraw.list[i]);
			if (clip.width && clip.height) {
				if (!compositor_2d_draw_bitmap_ex(visual, ctx->aspect.fill_texture, ctx, &clip, &ctx->bi->unclip, alpha, col_key, tr_state, 0))
					return 0;
			}
		}
	}
	return 1;
}


GF_Err compositor_2d_set_aspect_ratio(GF_Compositor *compositor)
{
	Double ratio;
	GF_Event evt;
	GF_Err e;
	Fixed scaleX, scaleY;

	compositor->output_width = compositor->scene_width;
	compositor->output_height = compositor->scene_height;
	compositor->vp_x = compositor->vp_y = 0;
	scaleX = scaleY = FIX_ONE;

	/*force complete clean*/
	compositor->traverse_state->invalidate_all = 1;

	if (!compositor->has_size_info && !(compositor->override_size_flags & 2) ) {
		compositor->output_width = compositor->display_width;
		compositor->output_height = compositor->display_height;
		compositor->vp_width = compositor->visual->width = compositor->output_width;
		compositor->vp_height = compositor->visual->height = compositor->output_height;
	} else {
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
		if (!scaleX) scaleX = FIX_ONE;
		scaleY = gf_divfix(INT2FIX(compositor->vp_height), INT2FIX(compositor->scene_height));
		if (!scaleY) scaleY = FIX_ONE;

		if (!compositor->scalable_zoom) {
			compositor->output_width = compositor->scene_width;
			compositor->output_height = compositor->scene_height;
			compositor->vp_width = FIX2INT(gf_divfix(INT2FIX(compositor->display_width), scaleX));
			compositor->vp_height = FIX2INT(gf_divfix(INT2FIX(compositor->display_height), scaleY));

			compositor->vp_x = (compositor->vp_width - compositor->output_width) / 2;
			compositor->vp_y = (compositor->vp_height - compositor->output_height) / 2;

			scaleX = scaleY = FIX_ONE;
		} else {
/*
			compositor->output_width = compositor->vp_width;
			compositor->output_height = compositor->vp_height;
*/
			compositor->output_width = compositor->display_width;
			compositor->output_height = compositor->display_height;
			compositor->vp_width = compositor->display_width;
			compositor->vp_height = compositor->display_height;
		}
		compositor->visual->width = compositor->output_width;
		compositor->visual->height = compositor->output_height;
	}

	/*resize hardware surface*/
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = compositor->vp_width;
	evt.setup.height = compositor->vp_height;
	evt.setup.opengl_mode = 0;
	evt.setup.system_memory = compositor->video_memory ? 0 : 1;

	e = compositor->video_out->ProcessEvent(compositor->video_out, &evt);
	if (e) return e;

	if (compositor->has_size_info) {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->scene_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->scene_height);
	} else {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->output_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->output_height);
	}
	/*set scale factor*/
	compositor_set_ar_scale(compositor, scaleX, scaleY);
	return GF_OK;
}

void compositor_send_resize_event(GF_Compositor *compositor, Fixed old_z, Fixed old_tx, Fixed old_ty, Bool is_resize)
{
#ifndef GPAC_DISABLE_SVG
	GF_Node *root;
	root = gf_sg_get_root_node(compositor->scene);
	/*if root node is DOM, sent a resize event*/
	if (root /* && (gf_node_get_tag(root) >= GF_NODE_FIRST_DOM_NODE_TAG) */) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.prev_scale = compositor->scale_x*old_z;
		evt.new_scale = compositor->scale_x*compositor->zoom;
		evt.bubbles = 1;

		if (is_resize) {
			evt.type = GF_EVENT_RESIZE;
			evt.screen_rect.width = INT2FIX(compositor->display_width);
			evt.screen_rect.height = INT2FIX(compositor->display_height);
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
			evt.bubbles = 0;
		}
		gf_dom_event_fire(gf_sg_get_root_node(compositor->scene), &evt);
	}
#endif
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
		compositor->zoom_changed = 1;

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

	/*for zoom&pan, send the event right away. For resize/scroll, wait for the frame to be drawn before sending it
	otherwise viewport info of SVG nodes won't be correct*/
	if (!is_resize) compositor_send_resize_event(compositor, old_z, old_tx, old_ty, 0);
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


/*overlay management*/
Bool visual_2d_overlaps_overlay(GF_VisualManager *visual, DrawableContext *ctx, GF_TraverseState *tr_state)
{
	u32 res = 0;
	GF_OverlayStack *ol;
	GF_Compositor *compositor = visual->compositor;
	if (compositor->visual != visual) return 0;

	ol = visual->overlays;
	while (ol) {
		u32 i;
		GF_IRect clip;
		if (ctx == ol->ctx) {
			ol = ol->next;
			continue;
		}
		clip = ctx->bi->clip;
		if (!ol->ra.count && !gf_irect_overlaps(&ol->ctx->bi->clip, &clip)) {
			ol = ol->next;
			continue;
		}
		/*check previsously drawn areas*/
		for (i=0; i<ol->ra.count; i++) {
			/*we have drawn something here, don't draw*/
			if (gf_irect_inside(&ol->ra.list[i], &clip))
				break;
		}
		res++;
		if (i<ol->ra.count) {
			ol = ol->next;
			continue;
		}

		/*note that we add the entire cliper, not the intersection with the overlay one. This is a simple way
		to handle the case where Drawble2 overlaps Drawable1 overlaps Overlay but Drawable2 doesn't overlaps Overlay
		by adding the entire drawable cliper, we will postpone drawing of all interconnected regions touching the overlay*/
		ra_union_rect(&ol->ra, &clip);
		ol = ol->next;
	}
	return res ? 1 : 0;
}

void visual_2d_flush_overlay_areas(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	DrawableContext *ctx;
	GF_OverlayStack *ol;
	GF_Compositor *compositor = visual->compositor;
	if (compositor->visual != visual) return;

	/*draw all overlays*/
	tr_state->traversing_mode = TRAVERSE_DRAW_2D;
	ol = visual->overlays;
	while (ol) {
		u32 i;
		Bool needs_draw = 1;
		GF_IRect the_clip, vid_clip;

		ra_refresh(&ol->ra);

		for (i=0; i<ol->ra.count; i++) {
			the_clip = ol->ra.list[i];

			/*draw all objects above this overlay*/
			ctx = ol->ctx->next;			
			while (ctx && ctx->drawable) {
				if (gf_irect_overlaps(&ctx->bi->clip, &the_clip)) {
					GF_IRect prev_clip = ctx->bi->clip;

					if (needs_draw) {
						/*if first object above is not transparent and completely covers the overlay skip video redraw*/
						if ((ctx->flags & CTX_IS_TRANSPARENT) || !gf_irect_inside(&prev_clip, &the_clip)) {
							vid_clip = ol->ra.list[i];
							gf_irect_intersect(&vid_clip, &ol->ctx->bi->clip);
							compositor_2d_draw_bitmap_ex(visual, ol->ctx->aspect.fill_texture, ol->ctx, &vid_clip, &ol->ctx->bi->unclip, 0xFF, NULL, tr_state, 1);
						}
						needs_draw = 0;
					}
					gf_irect_intersect(&ctx->bi->clip, &the_clip);
					tr_state->ctx = ctx;

					if (ctx->drawable->flags & DRAWABLE_USE_TRAVERSE_DRAW) {
						gf_node_traverse(ctx->drawable->node, tr_state);
					} else {
						drawable_draw(ctx->drawable, tr_state);
					}
					ctx->bi->clip = prev_clip;
				}
				ctx = ctx->next;
			}
		}
		ol = ol->next;
	}
}

void visual_2d_draw_overlays(GF_VisualManager *visual)
{
	GF_Err e;
	GF_TextureHandler *txh;
	GF_VideoSurface video_src;

	while (1) {
		GF_OverlayStack *ol = visual->overlays;
		if (!ol) return;
		visual->overlays = ol->next;
		
		txh = ol->ctx->aspect.fill_texture;
		video_src.height = txh->height;
		video_src.width = txh->width;
		video_src.pitch = txh->stride;
		video_src.pixel_format = txh->pixelformat;
		video_src.video_buffer = txh->data;

		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &ol->src, &ol->dst, 2);
		if (e) GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Visual2D] Error %s during overlay update\n", gf_error_to_string(e) ));

		ra_del(&ol->ra);
		free(ol);
	}
}

