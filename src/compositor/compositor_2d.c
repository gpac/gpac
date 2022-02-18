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
#include "nodes_stacks.h"
#include <gpac/options.h>
#include "texturing.h"

#include "gl_inc.h"


#ifndef GPAC_DISABLE_3D
void compositor_2d_hybgl_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 is_offscreen_clear)
{
	SFColor rgb;
	Fixed alpha = INT2FIX( GF_COL_A(BackColor) )/255;
	if (!visual->is_attached) return;

	if (!BackColor && !visual->offscreen && !is_offscreen_clear) {
		if ( !(visual->compositor->init_flags & GF_TERM_WINDOW_TRANSPARENT)) {
			BackColor = visual->compositor->back_color & 0x00FFFFFF;
		}
	}
	if (is_offscreen_clear) {
		gf_evg_surface_clear(visual->raster_surface, rc, BackColor);
		//if we clear the canvas with non-0 alpha, remember the area cleared in case we have to erase it later (overlapping bitmap)
		//if we clear dirty area of the canvas, remember the area to force gl flush 
		if (GF_COL_A(BackColor) || (is_offscreen_clear==2))
		{
			ra_union_rect(&visual->hybgl_drawn, rc);
		}
	} else {
		rgb.red = INT2FIX( GF_COL_R(BackColor) ) / 255;
		rgb.green = INT2FIX( GF_COL_G(BackColor) )/255;
		rgb.blue = INT2FIX( GF_COL_B(BackColor) )/255;
		visual_3d_clear(visual, rgb , alpha);
	}
}


void compositor_2d_hybgl_flush_video(GF_Compositor *compositor, GF_IRect *area)
{
	GF_TraverseState a_tr_state;

	//check if texture data has changed - if so, mark texture to be updated
	if (compositor->traverse_state->immediate_draw) {
		//nothing drawn, nothing to do
		if (!compositor->visual->hybgl_drawn.count) {
			return;
		}
		//nodes where drawn on canvas, push texture
		gf_sc_texture_set_data(compositor->hybgl_txh);
	} else {
		//nodes where drawn on canvas, push texture
		if (compositor->visual->hybgl_drawn.count) {
			//we could actually use texSubImage here, but we would need either to copy the data before pushing it or use current stride which will result in pushing too much data ...
			gf_sc_texture_set_data(compositor->hybgl_txh);
		}
	}
	//if no object drawn since the last flush, no need to draw the texture
	if (!compositor->visual->nb_objects_on_canvas_since_last_ogl_flush)
		goto exit;

	memset(&a_tr_state, 0, sizeof(GF_TraverseState));
	a_tr_state.color_mat.identity = 1;
	a_tr_state.visual = compositor->visual;
	a_tr_state.camera = &compositor->visual->camera;
	gf_mx_init(a_tr_state.model_matrix);

	visual_3d_set_state(compositor->visual, V3D_STATE_LIGHT, GF_FALSE);
	visual_3d_enable_antialias(compositor->visual, GF_FALSE);
	gf_sc_texture_set_blend_mode(compositor->hybgl_txh, TX_MODULATE);
	//visual_3d_set_material_2d_argb(compositor->visual, 0xFFFFFFFF);
	compositor->visual->has_material_2d = GF_FALSE;
	a_tr_state.mesh_num_textures = gf_sc_texture_enable(compositor->hybgl_txh, NULL);
	if (a_tr_state.mesh_num_textures ) {
		if (area) {
			u32 i;
			Fixed umin, umax, vmin, vmax;
			SFVec2f size, orig;
			size.x = INT2FIX(area->width);
			size.y = INT2FIX(area->height);

			orig.x = INT2FIX(area->x);
			orig.y = INT2FIX(area->y);
			mesh_new_rectangle(compositor->hybgl_mesh, size, &orig, GF_TRUE);

			orig.x = INT2FIX(area->x) + INT2FIX(compositor->vp_width)/2;
			orig.y = INT2FIX(compositor->vp_height)/2 - INT2FIX(area->y) + INT2FIX(area->height);
			//set our coor texture so that we update only the right part of the canvas
			umin = gf_divfix(orig.x, INT2FIX(compositor->vp_width));
			umax = gf_divfix(orig.x+size.x, INT2FIX(compositor->vp_width));
			vmin = gf_divfix(orig.y-size.y, INT2FIX(compositor->vp_height));
			vmax = gf_divfix(orig.y, INT2FIX(compositor->vp_height));

			for (i=0; i<compositor->hybgl_mesh->v_count; i++) {
				if (compositor->hybgl_mesh->vertices[i].texcoords.x == FIX_ONE) {
					compositor->hybgl_mesh->vertices[i].texcoords.x = umax;
				} else {
					compositor->hybgl_mesh->vertices[i].texcoords.x = umin;
				}
				if (compositor->hybgl_mesh->vertices[i].texcoords.y == FIX_ONE) {
					compositor->hybgl_mesh->vertices[i].texcoords.y = vmax;
				} else {
					compositor->hybgl_mesh->vertices[i].texcoords.y = vmin;
				}
			}
		}

		visual_3d_mesh_paint(&a_tr_state, compositor->hybgl_mesh);
		gf_sc_texture_disable(compositor->hybgl_txh);

		if (area) {
			SFVec2f size;
			size.x = INT2FIX(compositor->vp_width);
			size.y = INT2FIX(compositor->vp_height);
			mesh_new_rectangle(compositor->hybgl_mesh, size, NULL, GF_TRUE);
		}
	}

exit:
	//in immediate mode always clear the canvas
	if (compositor->traverse_state->immediate_draw) {
		compositor->visual->hybgl_drawn.count = 0;
		//partial flush, reset
		if (area)
			memset(compositor->hybgl_txh->data, 0, compositor->hybgl_txh->stride*compositor->hybgl_txh->height);
	}
	//complete flush (end of frame)
	else if (!area) {
		compositor->visual->hybgl_drawn.count = 0;
	}
	compositor->visual->nb_objects_on_canvas_since_last_ogl_flush = 0;
}

Bool compositor_2d_hybgl_draw_bitmap(GF_VisualManager *visual, GF_TraverseState *tr_state, DrawableContext *ctx)
{
	GF_Node *txtrans = NULL;
	//for anything but background use regular routines
	if (!(ctx->flags & CTX_IS_BACKGROUND)) return GF_FALSE;

#ifndef GPAC_DISABLE_VRML
	if (tr_state->appear ) txtrans = ((M_Appearance *)tr_state->appear)->textureTransform;
#endif

	/*ignore texture transform for bitmap*/
	tr_state->mesh_num_textures = gf_sc_texture_enable(ctx->aspect.fill_texture, txtrans);

	if (tr_state->mesh_num_textures) {
		SFVec2f size, orig;
		size.x = ctx->bi->unclip.width;
		size.y = ctx->bi->unclip.height;
		orig.x = ctx->bi->unclip.x ;
		orig.y = ctx->bi->unclip.y;
		mesh_new_rectangle(visual->compositor->hybgl_mesh_background, size, &orig, GF_FALSE);

		visual_3d_mesh_paint(tr_state, visual->compositor->hybgl_mesh_background);
		gf_sc_texture_disable(ctx->aspect.fill_texture);
		tr_state->mesh_num_textures = 0;
	}
	return GF_TRUE;
}
#endif

#ifndef GPAC_DISABLE_3D
void compositor_2d_reset_gl_auto(GF_Compositor *compositor)
{
	if (compositor->hybgl_txh) {
		if (compositor->hybgl_txh->data) {
			gf_free(compositor->hybgl_txh->data);
			compositor->hybgl_txh->data = NULL;
		}
		if (compositor->hybgl_txh->tx_io)
			gf_sc_texture_release(compositor->hybgl_txh);

		gf_free(compositor->hybgl_txh);
		compositor->hybgl_txh = NULL;
	}
	if (compositor->hybgl_mesh) {
		mesh_free(compositor->hybgl_mesh);
		compositor->hybgl_mesh = NULL;
	}
	if (compositor->hybgl_mesh_background) {
		mesh_free(compositor->hybgl_mesh_background);
		compositor->hybgl_mesh_background = NULL;
	}
}

static GF_Err compositor_2d_setup_opengl(GF_VisualManager *visual)
{
	GF_Compositor *compositor = visual->compositor;
	visual->is_attached = GF_TRUE;

	visual_3d_setup(visual);
	visual->compositor->traverse_state->camera = &visual->camera;


	glViewport(0, 0, compositor->vp_width, compositor->vp_height);

	visual->camera.vp.x = visual->camera.vp.y = 0;
	visual->camera.vp.width = visual->camera.width = INT2FIX(compositor->vp_width);
	visual->camera.vp.height = visual->camera.height = INT2FIX(compositor->vp_height);
	visual->camera.up.y = FIX_ONE;
	visual->camera.end_zoom = FIX_ONE;
	visual->camera.position.z = INT2FIX(1000);
	visual->camera.flags = CAM_IS_DIRTY;

	camera_update(&visual->camera, NULL, visual->compositor->hybrid_opengl ? GF_TRUE : visual->center_coords);

	visual_3d_projection_matrix_modified(visual);
	return GF_OK;
}
#endif

#ifndef GPAC_DISABLE_3D

static GF_Err c2d_video_access_hybrid_opengl(GF_VisualManager *visual)
{
	GF_Err e;
	GF_Compositor *compositor = visual->compositor;

	if (!compositor->hybgl_txh) {
		GF_SAFEALLOC(compositor->hybgl_txh, GF_TextureHandler);
		if (!compositor->hybgl_txh) return GF_IO_ERR;
		compositor->hybgl_txh->compositor = compositor;
	}

	if ((compositor->hybgl_txh->width != compositor->vp_width) || (compositor->hybgl_txh->height != compositor->vp_height)) {
		SFVec2f size;
		compositor->hybgl_txh->data = (char*)gf_realloc(compositor->hybgl_txh->data, 4*compositor->vp_width*compositor->vp_height);

		if (compositor->hybgl_txh->tx_io)
			gf_sc_texture_release(compositor->hybgl_txh);

		compositor->hybgl_txh->width = compositor->vp_width;
		compositor->hybgl_txh->height = compositor->vp_height;
		compositor->hybgl_txh->stride = 4*compositor->vp_width;
		compositor->hybgl_txh->pixelformat = GF_PIXEL_RGBA;
		compositor->hybgl_txh->transparent = GF_TRUE;
		compositor->hybgl_txh->flags = GF_SR_TEXTURE_PRIVATE_MEDIA | GF_SR_TEXTURE_NO_GL_FLIP;

		memset(compositor->hybgl_txh->data, 0, 4*compositor->hybgl_txh->width*compositor->hybgl_txh->height);
		gf_sc_texture_allocate(compositor->hybgl_txh);
		gf_sc_texture_set_data(compositor->hybgl_txh);

		if (!compositor->hybgl_mesh)
			compositor->hybgl_mesh = new_mesh();

		if (!compositor->hybgl_mesh_background)
			compositor->hybgl_mesh_background = new_mesh();

		size.x = INT2FIX(compositor->vp_width);
		size.y = INT2FIX(compositor->vp_height);
		mesh_new_rectangle(compositor->hybgl_mesh, size, NULL, GF_TRUE);
		mesh_new_rectangle(compositor->hybgl_mesh_background, size, NULL, GF_FALSE);
	}
	if (!compositor->hybgl_txh->data) return GF_IO_ERR;

	if (visual->compositor->traverse_state->immediate_draw)
		memset(compositor->hybgl_txh->data, 0, 4*compositor->hybgl_txh->width*compositor->hybgl_txh->height);

	e = gf_evg_surface_attach_to_buffer(visual->raster_surface, compositor->hybgl_txh->data,
	        compositor->hybgl_txh->width,
	        compositor->hybgl_txh->height,
	        0,
	        compositor->hybgl_txh->width * 4,
	        (GF_PixelFormat) GF_PIXEL_RGBA);
	if (e) return e;
	e = compositor_2d_setup_opengl(visual);
	if (e) return e;
	visual->ClearSurface = compositor_2d_hybgl_clear_surface;
	visual->DrawBitmap = compositor_2d_hybgl_draw_bitmap;
	return GF_OK;
}
#endif

static GF_Err c2d_get_video_access_normal(GF_VisualManager *visual)
{
	GF_Err e;
	GF_Compositor *compositor = visual->compositor;

	compositor->hw_locked = GF_FALSE;

	e = compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, GF_TRUE);
	if (e==GF_OK) {
		compositor->hw_locked = GF_TRUE;

		e = gf_evg_surface_attach_to_buffer(visual->raster_surface, compositor->hw_surface.video_buffer,
		        compositor->hw_surface.width,
		        compositor->hw_surface.height,
		        compositor->hw_surface.pitch_x,
		        compositor->hw_surface.pitch_y,
		        (GF_PixelFormat) compositor->hw_surface.pixel_format);
		if (!e) {
			visual->is_attached = GF_TRUE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Video surface memory attached to raster - w=%d h=%d pitch_x=%d pitch_y=%d\n", compositor->hw_surface.width, compositor->hw_surface.height, compositor->hw_surface.pitch_x, compositor->hw_surface.pitch_y));
			return GF_OK;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Cannot attach video surface memory to raster for pixel format %s: %s\n", gf_pixel_fmt_name(compositor->hw_surface.pixel_format), gf_error_to_string(e) ));
		compositor->last_error = e;
		compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, GF_FALSE);
	}
	compositor->hw_locked = GF_FALSE;
	visual->is_attached = GF_FALSE;
	/*if using BlitTexture, return OK to still be able to blit images*/
	if (compositor->video_out->BlitTexture) e = GF_OK;
	return e;
}

GF_Err compositor_2d_get_video_access(GF_VisualManager *visual)
{
	if (!visual->raster_surface) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_3D
	if (visual->compositor->hybrid_opengl) {
		return c2d_video_access_hybrid_opengl(visual);
	}
#endif
	//do nothing until asked to really attach
	return GF_OK;
}

Bool compositor_2d_check_attached(GF_VisualManager *visual)
{
	if (!visual->is_attached) {
		c2d_get_video_access_normal(visual);
	}

	return visual->is_attached;
}

void compositor_2d_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 offscreen_clear)
{
	//visual not attached on main (direct video) visual, use texture bliting
	if (!visual->is_attached && visual->compositor->video_out->Blit && (visual->compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_RGB)) {
		char data[12];
		GF_Err e;
		GF_VideoSurface video_src;
		GF_Window src_wnd, dst_wnd;

		if (!BackColor && !visual->offscreen) {
			if ( !(visual->compositor->init_flags & GF_TERM_WINDOW_TRANSPARENT)) {
				BackColor = visual->compositor->back_color;
			}
		}

		data[0] = data[3] = data[6] = data[9] = GF_COL_R(BackColor);
		data[1] = data[4] = data[7] = data[10] = GF_COL_G(BackColor);
		data[2] = data[5] = data[8] = data[11] = GF_COL_B(BackColor);

		memset(&video_src, 0, sizeof(GF_VideoSurface));
		video_src.height = 2;
		video_src.width = 2;
		video_src.pitch_x = 0;
		video_src.pitch_y = 6;
		video_src.pixel_format = GF_PIXEL_RGB;
		video_src.video_buffer = data;

		src_wnd.x = src_wnd.y = 0;
		src_wnd.w = src_wnd.h = 1;
		if (rc) {
			if (visual->center_coords) {
				dst_wnd.x = rc->x + visual->width/2;
				dst_wnd.y = visual->height/2 - rc->y;
			} else {
				dst_wnd.x = rc->x;
				dst_wnd.y = rc->y - visual->height/2;
			}
			dst_wnd.w = rc->width;
			dst_wnd.h = rc->height;
		} else {
			dst_wnd.x = dst_wnd.y = 0;
			dst_wnd.w = visual->width;
			dst_wnd.h = visual->height;
		}

		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, 0);
		if (e==GF_OK) return;
	}

	visual_2d_clear_surface(visual, rc, BackColor, offscreen_clear);
}



void compositor_2d_release_video_access(GF_VisualManager *visual)
{
	GF_Compositor *compositor = visual->compositor;
	if (visual->is_attached) {
		visual->is_attached = GF_FALSE;
	}

#ifndef GPAC_DISABLE_3D
	if (compositor->hybrid_opengl) {
		compositor_2d_hybgl_flush_video(compositor, NULL);
		return;
	}
#endif //GPAC_DISABLE_3D

	if (compositor->hw_context) {
		compositor->video_out->LockOSContext(compositor->video_out, GF_FALSE);
		compositor->hw_context = NULL;
	} else if (compositor->hw_locked) {
		compositor->video_out->LockBackBuffer(compositor->video_out, &compositor->hw_surface, GF_FALSE);
		compositor->hw_locked = GF_FALSE;
	}
}

static void store_blit_times(GF_TextureHandler *txh, u32 push_time)
{
#ifndef GPAC_DISABLE_LOG
	u32 ck;
#endif

	push_time = gf_sys_clock() - push_time;
	txh->nb_frames ++;
	txh->upload_time += push_time;

#ifndef GPAC_DISABLE_LOG
	gf_mo_get_object_time(txh->stream, &ck);
	if (ck>txh->last_frame_time) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Bliting frame (CTS %d) %d ms too late\n", txh->last_frame_time, ck - txh->last_frame_time ));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[2D Blitter] At %u Blit texture (CTS %u) %d ms after due date - blit in %d ms - average push time %d ms\n", ck, txh->last_frame_time, ck - txh->last_frame_time, push_time, txh->upload_time / txh->nb_frames));
#endif
}

Bool compositor_texture_rectangles(GF_VisualManager *visual, GF_TextureHandler *txh, GF_IRect *clip, GF_Rect *unclip, GF_Window *src, GF_Window *dst, Bool *disable_blit, Bool *has_scale)
{
	Fixed w_scale, h_scale, tmp;
	u32 output_width, output_height;
	GF_IRect clipped_final = *clip;
	GF_Rect final = *unclip;
	Bool use_blit;

	src->w = src->h = 0;
	dst->w = dst->h = 0;
	if (disable_blit) *disable_blit = GF_FALSE;
	if (has_scale) *has_scale = GF_FALSE;

	if (final.width<=0 || final.height <=0) return GF_FALSE;
	if (txh->width==0 || txh->height==0) return GF_FALSE;

	w_scale = final.width / txh->width;
	h_scale = final.height / txh->height;

	if ((w_scale != FIX_ONE) || (h_scale!=FIX_ONE)) {
		if (has_scale) *has_scale = GF_TRUE;
	}

	if (visual->offscreen) {
		output_width = visual->width;
		output_height = visual->height;
	} else {
		/*use entire video surface for un-centering coord system*/
		output_width = visual->compositor->vp_width;
		output_height = visual->compositor->vp_height;
	}
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
		if (clipped_final.width <= 0) return GF_FALSE;
	}
	if (clipped_final.y<0) {
		clipped_final.height += clipped_final.y;
		clipped_final.y = 0;
		if (clipped_final.height <= 0) return GF_FALSE;
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
		return GF_FALSE;

	if (clipped_final.width-1>= FIX2INT(final.width) ) clipped_final.width = FIX2INT(final.width);
	if (clipped_final.height-1>= FIX2INT(final.height) ) clipped_final.height = FIX2INT(final.height);

	/*set dest window*/
	dst->x = (u32) clipped_final.x;
	dst->y = (u32) clipped_final.y;
	dst->w = (u32) clipped_final.width;
	dst->h = (u32) clipped_final.height;

	if (!dst->w || !dst->h) return GF_FALSE;

#ifdef GPAC_FIXED_POINT
#define ROUND_FIX(_v)	\
	_v = FIX2INT(tmp);
#define CEILING(_v)	\
	_v = FIX2INT(tmp);	\
	if (INT2FIX(_v)!=tmp) _v++;
#else
#define ROUND_FIX(_v)	\
	_v = FIX2INT(tmp);	\
	tmp -= INT2FIX(_v);	\
	if (tmp>99*FIX_ONE/100) { _v++; tmp = 0; }	\
	if (ABS(tmp) > FIX_EPSILON) use_blit = 0;
#define CEILING(_v)	\
	_v = FIX2INT(tmp);	\
	tmp -= INT2FIX(_v);	\
	if (tmp>0) { _v++; tmp = 0; }	\
	if (ABS(tmp) > FIX_EPSILON) use_blit = 0;
#endif

	use_blit = GF_TRUE;

	if (txh->data && !txh->size && (txh->width==2) && (txh->height==2) ) {
		src->x = src->y = 0;
		src->w = 1;
		src->h = 1;
	} else {
		/*compute SRC window*/
		tmp = gf_divfix(INT2FIX(clipped_final.x) - final.x, w_scale);
		if (tmp<0) tmp=0;
		CEILING(src->x);

		tmp = gf_divfix(INT2FIX(clipped_final.y) - final.y, h_scale);
		if (tmp<0) tmp=0;
		CEILING(src->y);

		tmp = gf_divfix(INT2FIX(clip->width), w_scale);
		ROUND_FIX(src->w);

		tmp = gf_divfix(INT2FIX(clip->height), h_scale);
		ROUND_FIX(src->h);


		if (src->w>txh->width) src->w=txh->width;
		if (src->h>txh->height) src->h=txh->height;

		if (!src->w || !src->h) return GF_FALSE;

		/*make sure we lie in src bounds*/
		if (src->x + src->w>txh->width) src->w = txh->width - src->x;
		if (src->y + src->h>txh->height) src->h = txh->height - src->y;
	}
#undef ROUND_FIX

	if (disable_blit) *disable_blit = use_blit ? GF_FALSE : GF_TRUE;
	return GF_TRUE;
}

static Bool compositor_2d_draw_bitmap_ex(GF_VisualManager *visual, GF_TextureHandler *txh, DrawableContext *ctx, GF_IRect *clip, GF_Rect *unclip, u8 alpha, GF_TraverseState *tr_state, Bool force_soft_blt)
{
	GF_VideoSurface video_src;
	GF_Err e;
	Bool use_soft_stretch, use_blit, flush_video, is_attached, has_scale;
	u32 overlay_type;
	GF_Window src_wnd, dst_wnd;
	u32 output_width, output_height, hw_caps;


	if (!txh->data) return GF_TRUE;

	if (!visual->compositor->has_size_info && !(visual->compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE)
	        && (visual->compositor->override_size_flags & 1)
	        && !(visual->compositor->override_size_flags & 2)
	   ) {
		if ( (visual->compositor->scene_width < txh->width)
		        || (visual->compositor->scene_height < txh->height)) {
			visual->compositor->scene_width = txh->width;
			visual->compositor->scene_height = txh->height;
			visual->compositor->msg_type |= GF_SR_CFG_OVERRIDE_SIZE;
			return GF_TRUE;
		}
	}

	if (!compositor_texture_rectangles(visual, txh, clip, unclip, &src_wnd, &dst_wnd, &use_blit, &has_scale)) return GF_TRUE;

	//blitter is disabled
	if (txh->flags & GF_SR_TEXTURE_DISABLE_BLIT) return GF_FALSE;

	/*can we use hardware blitter ?*/
	hw_caps = visual->compositor->video_out->hw_caps;
	overlay_type = 0;
	flush_video = GF_FALSE;
	use_soft_stretch = GF_TRUE;

	output_width = visual->compositor->vp_width;
	output_height = visual->compositor->vp_height;

	if (visual->compositor->disable_hardware_blit) force_soft_blt = GF_TRUE;

	if (!force_soft_blt) {

		/*avoid partial redraw that don't come close to src pixels with the bliter, this leads to ugly artefacts -
		fall back to rasterizer*/
//		if (!(ctx->flags & CTX_TEXTURE_DIRTY) && !use_blit && (src_wnd.x || src_wnd.y) )
//			return 0;

		switch (txh->pixelformat) {
		case GF_PIXEL_RGB:
		case GF_PIXEL_BGR:
		case GF_PIXEL_RGBS:
		case GF_PIXEL_RGBD:
//		case GF_PIXEL_RGB_555:
//		case GF_PIXEL_RGB_565:
			if ((alpha==0xFF) && (hw_caps & GF_VIDEO_HW_HAS_RGB)) {
				use_soft_stretch = GF_FALSE;
			}
			else if ((alpha!=0xFF) && (hw_caps & GF_VIDEO_HW_HAS_RGBA)) {
				use_soft_stretch = GF_FALSE;
			}
			break;
		case GF_PIXEL_ARGB:
		case GF_PIXEL_RGBA:
		case GF_PIXEL_RGBAS:
		case GF_PIXEL_RGBDS:
			if (hw_caps & GF_VIDEO_HW_HAS_RGBA)
				use_soft_stretch = GF_FALSE;
			break;
		default:
			if (gf_pixel_fmt_is_yuv(txh->pixelformat)) {
				if (hw_caps & GF_VIDEO_HW_HAS_YUV) use_soft_stretch = GF_FALSE;
				else if (hw_caps & GF_VIDEO_HW_HAS_YUV_OVERLAY) overlay_type = 1;
			}
			break;
		}
		/*disable based on settings*/
		if (!visual->compositor->yuvhw
		        || (ctx->col_mat || !visual->compositor->video_out->Blit)
		   ) {
			use_soft_stretch = GF_TRUE;
			overlay_type = 0;
		}
		if (!visual->compositor->blitp && ((src_wnd.w!=txh->width) || (src_wnd.h!=txh->height) )) {
			use_soft_stretch = GF_TRUE;
		}

		/*disable HW color keying - not compatible with MPEG-4 MaterialKey*/
		if (tr_state->col_key) {
			use_soft_stretch = GF_TRUE;
			overlay_type = 0;
		}

		if (overlay_type) {
			/*no more than one overlay is supported at the current time*/
			if (visual->overlays) {
				ctx->drawable->flags &= ~DRAWABLE_IS_OVERLAY;
				overlay_type = 0;
			}
			/*direct draw or not last context: we must queue the overlay*/
			else if (tr_state->immediate_draw || (ctx->next && ctx->next->drawable)) {
				overlay_type = 2;
			}
			/*OK we can overlay this video - if full display, don't flush*/
			if (overlay_type==1) {
				if (dst_wnd.w==visual->compositor->display_width) flush_video = GF_FALSE;
				else if (dst_wnd.h==visual->compositor->display_height) flush_video = GF_FALSE;
				else flush_video = visual->has_modif;
			}
			/*if no color keying, we cannot queue the overlay*/
			else if (!visual->compositor->video_out->overlay_color_key) {
				overlay_type = 0;
			}
		}
	}

	if (has_scale && !(hw_caps & GF_VIDEO_HW_HAS_STRETCH) && !overlay_type) {
		use_soft_stretch = GF_TRUE;
	}

	memset(&video_src, 0, sizeof(GF_VideoSurface));
	video_src.height = txh->height;
	video_src.width = txh->width;
	video_src.pitch_x = 0;
	if (!txh->stride)
		gf_pixel_get_size_info(txh->pixelformat, txh->width, txh->height, NULL, &txh->stride, NULL, NULL, NULL);
	video_src.pitch_y = txh->stride;
	video_src.pixel_format = txh->pixelformat;
#ifdef GF_SR_USE_DEPTH
	if (txh->pixelformat==GF_PIXEL_YUVD) video_src.pixel_format = GF_PIXEL_YUV;
#endif
	video_src.video_buffer = txh->data;
	if (txh->frame_ifce) {
		u32 stride;
		if (!txh->frame_ifce->get_plane) return GF_FALSE;

		e = txh->frame_ifce->get_plane(txh->frame_ifce, 0, (const u8 **) &video_src.video_buffer, &video_src.pitch_y);
		if (e) return GF_FALSE;
		txh->frame_ifce->get_plane(txh->frame_ifce, 1, (const u8 **) video_src.u_ptr, &stride);
		txh->frame_ifce->get_plane(txh->frame_ifce, 2, (const u8 **) video_src.v_ptr, &stride);
	}
	video_src.global_alpha = alpha;

	//overlay queing
	if (overlay_type==2) {
		GF_IRect o_rc;
		GF_OverlayStack *ol, *first;

		/*queue overlay in order*/
		GF_SAFEALLOC(ol, GF_OverlayStack);
		if (!ol) {
			return GF_FALSE;
		}
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
			o_rc.y = dst_wnd.y + dst_wnd.h;
		}

		o_rc.width = dst_wnd.w;
		o_rc.height = dst_wnd.h;
		visual->ClearSurface(visual, &o_rc, visual->compositor->video_out->overlay_color_key, GF_FALSE);
		visual->has_overlays = GF_TRUE;
		/*mark drawable as overlay*/
		ctx->drawable->flags |= DRAWABLE_IS_OVERLAY;

		/*prevents this context from being removed in direct draw mode by requesting a new one
		but not allocating it*/
		if (tr_state->immediate_draw)
			visual_2d_get_drawable_context(visual);

		return GF_TRUE;
	}

	//will pause clock if first HW load
	gf_sc_texture_check_pause_on_first_load(txh, GF_TRUE);

	if (overlay_type) {
		u32 push_time;

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

		push_time = gf_sys_clock();
		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, 1);

		if (!e) {
			store_blit_times(txh, push_time);
			/*mark drawable as overlay*/
			ctx->drawable->flags |= DRAWABLE_IS_OVERLAY;
			visual->has_overlays = GF_TRUE;

			//will resume clock if first HW load
			gf_sc_texture_check_pause_on_first_load(txh, GF_FALSE);
			return GF_TRUE;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Error during overlay blit - trying with soft one\n"));
		visual->compositor->skip_flush = GF_FALSE;
	}

	/*most graphic cards can't perform bliting on locked surface - force unlock by releasing the hardware*/
	is_attached = visual->is_attached;
	if (is_attached) visual_2d_release_raster(visual);

	if (!use_soft_stretch) {
		u32 push_time = gf_sys_clock();
		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &src_wnd, &dst_wnd, 0);

		/*HW pb, try soft*/
		if (e) {
			use_soft_stretch = GF_TRUE;
			if (visual->compositor->video_memory==1) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor2D] Error during hardware blit - will use soft one\n"));
				visual->compositor->video_memory = 2;
			}
			/*force a reconfigure of video output*/
			else if (visual->compositor->video_memory!=2) {
				GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor2D] Reconfiguring video output to use video memory\n"));
				visual->compositor->request_video_memory = GF_TRUE;
				visual->compositor->root_visual_setup = GF_FALSE;
				gf_sc_next_frame_state(visual->compositor, GF_SC_DRAW_FRAME);
			}
		} else {
			store_blit_times(txh, push_time);
		}
	}

	//will resume clock if first HW load
	gf_sc_texture_check_pause_on_first_load(txh, GF_FALSE);

	if (use_soft_stretch) {
		GF_VideoSurface backbuffer;
		if (!visual->compositor->softblt) {
			if (is_attached) visual_2d_init_raster(visual);
			txh->flags |= GF_SR_TEXTURE_DISABLE_BLIT;
			return GF_FALSE;
		}

		e = visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, GF_TRUE);
		if (!e) {
			u32 push_time = gf_sys_clock();
			e = gf_stretch_bits(&backbuffer, &video_src, &dst_wnd, &src_wnd, alpha, GF_FALSE, tr_state->col_key, ctx->col_mat);
			store_blit_times(txh, push_time);
			visual->compositor->video_out->LockBackBuffer(visual->compositor->video_out, &backbuffer, GF_FALSE);
			if (e) {
				if (e != GF_NOT_SUPPORTED) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor2D] Cannot soft blit surface (error %s) - will try using software rasterizer\n", gf_error_to_string(e) ));
					visual->compositor->last_error = e;
				}
				if (is_attached) visual_2d_init_raster(visual);
				txh->flags |= GF_SR_TEXTURE_DISABLE_BLIT;
				return GF_FALSE;
			}
		} else {
			if (e != GF_NOT_SUPPORTED) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Cannot lock back buffer - Error %s\n", gf_error_to_string(e) ));
				visual->compositor->last_error = e;
			}
			if (is_attached) visual_2d_init_raster(visual);
			return GF_FALSE;
		}
		if (!visual->compositor->video_memory) {
			GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor2D] Reconfiguring video output to use video memory\n"));
			visual->compositor->video_memory = 1;
			visual->compositor->root_visual_setup = GF_FALSE;
			gf_sc_next_frame_state(visual->compositor, GF_SC_DRAW_FRAME);
		}
	}
	visual->has_modif = GF_TRUE;
	if (is_attached) visual_2d_init_raster(visual);
	return GF_TRUE;
}



Bool compositor_2d_draw_bitmap(GF_VisualManager *visual, GF_TraverseState *tr_state, DrawableContext *ctx)
{
	u8 alpha = 0xFF;

	if (!ctx->aspect.fill_texture) return GF_TRUE;
	if ((ctx->aspect.fill_texture == visual->compositor->passthrough_txh) && visual->compositor->passthrough_inplace)
		return GF_TRUE;
	/*check if texture is ready - if not pretend we drew it*/
	if (!ctx->aspect.fill_texture->data) return GF_TRUE;
	if (ctx->transform.m[0]<0) return GF_FALSE;
	/*check if the <0 value is due to a flip in he scene description or
	due to bifs<->svg... context switching*/
	if (ctx->transform.m[4]<0) {
		if (!(ctx->flags & CTX_FLIPED_COORDS)) return GF_FALSE;
	} else {
		if (ctx->flags & CTX_FLIPED_COORDS) return GF_FALSE;
	}
	if (ctx->transform.m[1] || ctx->transform.m[3]) return GF_FALSE;
#ifndef GPAC_DISABLE_VRML
	if ((ctx->flags & CTX_HAS_APPEARANCE) && ctx->appear && ((M_Appearance*)ctx->appear)->textureTransform)
		return GF_FALSE;
#endif

	alpha = GF_COL_A(ctx->aspect.fill_color);
	/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
	if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);

	if (!alpha) return GF_TRUE;

	switch (ctx->aspect.fill_texture->pixelformat) {
	case GF_PIXEL_YUVD:
	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGBDS:
#ifndef GPAC_DISABLE_3D
		/*using OpenGL to render depth images*/
		if (visual->compositor->depth_gl_type) {
			gf_sc_set_option(visual->compositor, GF_OPT_USE_OPENGL, 2);
			return GF_TRUE;
		}
#endif
		//fallthrought
	default:
		break;
	}

	/*direct drawing, no clippers */
	if (tr_state->immediate_draw) {
		if (visual->compositor->video_out->BlitTexture) {
			if (! visual->compositor->video_out->BlitTexture(visual->compositor->video_out, ctx->aspect.fill_texture, &ctx->transform, &ctx->bi->clip, alpha, tr_state->col_key
#ifdef GF_SR_USE_DEPTH
			        , ctx->depth_offset, ctx->depth_gain
#else
			        , 0, 0
#endif
			                                                ))
				return GF_FALSE;
		} else {
			if (!compositor_2d_draw_bitmap_ex(visual, ctx->aspect.fill_texture, ctx, &ctx->bi->clip, &ctx->bi->unclip, alpha, tr_state, GF_FALSE))
				return GF_FALSE;
		}
	}
	/*draw bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<tr_state->visual->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (tr_state->visual->draw_node_index < tr_state->visual->to_redraw.list[i].opaque_node_index) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &tr_state->visual->to_redraw.list[i].rect);
			if (clip.width && clip.height) {
				if (visual->compositor->video_out->BlitTexture) {
					if (!visual->compositor->video_out->BlitTexture(visual->compositor->video_out, ctx->aspect.fill_texture, &ctx->transform, &ctx->bi->clip, alpha, tr_state->col_key
#ifdef GF_SR_USE_DEPTH
					        , ctx->depth_offset, ctx->depth_gain
#else
					        , 0, 0
#endif
					                                               ))
						return GF_FALSE;
				} else if (!compositor_2d_draw_bitmap_ex(visual, ctx->aspect.fill_texture, ctx, &clip, &ctx->bi->unclip, alpha, tr_state, GF_FALSE)) {
					return GF_FALSE;
				}
			}
		}
	}
	ctx->aspect.fill_texture->flags |= GF_SR_TEXTURE_USED;
	return GF_TRUE;
}


GF_Err compositor_2d_set_aspect_ratio(GF_Compositor *compositor)
{
	u32 old_vp_width, old_vp_height;
	Bool changed = GF_FALSE;
	Double ratio;
	GF_Event evt;
	GF_Err e;
	Fixed scaleX, scaleY;

	compositor->output_width = compositor->scene_width;
	compositor->output_height = compositor->scene_height;
	compositor->vp_x = compositor->vp_y = 0;
	scaleX = scaleY = FIX_ONE;

	old_vp_width = compositor->vp_width;
	old_vp_height = compositor->vp_height;

	/*force complete clean*/
	compositor->traverse_state->invalidate_all = GF_TRUE;

	if (!compositor->has_size_info && !(compositor->override_size_flags & 2) ) {
		compositor->output_width = compositor->display_width;
		compositor->output_height = compositor->display_height;
		compositor->vp_width = compositor->visual->width = compositor->output_width;
		compositor->vp_height = compositor->visual->height = compositor->output_height;
	} else {
		if (compositor->rotate_mode % 2) {
			compositor->vp_height = compositor->display_width;
			compositor->vp_width = compositor->display_height;
		} else {
			compositor->vp_width = compositor->display_width;
			compositor->vp_height = compositor->display_height;
		}

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

		if (!compositor->sz) {
			compositor->output_width = compositor->scene_width;
			compositor->output_height = compositor->scene_height;
			compositor->vp_width = FIX2INT(gf_divfix(INT2FIX(compositor->display_width), scaleX));
			compositor->vp_height = FIX2INT(gf_divfix(INT2FIX(compositor->display_height), scaleY));

			compositor->vp_x = (compositor->vp_width - compositor->output_width) / 2;
			compositor->vp_y = (compositor->vp_height - compositor->output_height) / 2;

			scaleX = scaleY = FIX_ONE;
		} else {
			compositor->output_width = compositor->display_width;
			compositor->output_height = compositor->display_height;
			compositor->vp_width = compositor->display_width;
			compositor->vp_height = compositor->display_height;
		}
		compositor->visual->width = compositor->output_width;
		compositor->visual->height = compositor->output_height;
	}

	/*resize hardware surface*/
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.setup.width = compositor->vp_width;
	evt.setup.height = compositor->vp_height;
	evt.setup.use_opengl = GF_FALSE;
	evt.setup.disable_vsync = compositor->bench_mode ? GF_TRUE : GF_FALSE;
	/*copy over settings*/
	evt.setup.system_memory = compositor->video_memory ? GF_FALSE : GF_TRUE;
	if (compositor->request_video_memory) evt.setup.system_memory = GF_FALSE;
	compositor->request_video_memory = GF_FALSE;

#ifndef GPAC_DISABLE_3D
	if (compositor->hybrid_opengl) {
		evt.setup.use_opengl = GF_TRUE;
		evt.setup.system_memory = GF_FALSE;
		evt.setup.back_buffer = GF_TRUE;
	}
#endif

	if (compositor->was_system_memory != evt.setup.system_memory) changed = GF_TRUE;
	else if (old_vp_width != compositor->vp_width) changed = GF_TRUE;
	else if (old_vp_height != compositor->vp_height) changed = GF_TRUE;
	else if (compositor->is_opengl != evt.setup.use_opengl) changed = GF_TRUE;


	if (changed) {
		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor2D] Reconfiguring display size %d x %d - opengl %s - use %s memory\n", evt.setup.width, evt.setup.height,
		                                     evt.setup.use_opengl ? "yes" : "no", evt.setup.system_memory ? "systems" : "video"
		                                    ));

		e = compositor->video_out->ProcessEvent(compositor->video_out, &evt);
		if (e) {
#ifndef GPAC_DISABLE_3D
			if (!compositor->hybrid_opengl) {
				compositor->hybrid_opengl = GF_TRUE;
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor2D] Failed to configure 2D output (%s) - retrying in OpenGL mode\n", gf_error_to_string(e) ));
				return compositor_2d_set_aspect_ratio(compositor);
			}
#endif
			compositor->video_setup_failed = GF_TRUE;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			evt.message.error = e;
			evt.message.message = "Cannot setup video output";
			gf_sc_send_event(compositor, &evt);
			return e;
		}

		compositor->is_opengl = evt.setup.use_opengl;
		compositor->was_system_memory = evt.setup.system_memory;

		if (evt.setup.use_opengl) {
			gf_opengl_init();
		}
	}
	if (compositor->has_size_info) {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->scene_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->scene_height);
	} else {
		compositor->traverse_state->vp_size.x = INT2FIX(compositor->output_width);
		compositor->traverse_state->vp_size.y = INT2FIX(compositor->output_height);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Reconfigured display size %d x %d done\n", evt.setup.width, evt.setup.height));

	/*set scale factor*/
	compositor_set_ar_scale(compositor, scaleX, scaleY);
	return GF_OK;
}

void compositor_send_resize_event(GF_Compositor *compositor, GF_SceneGraph *subscene, Fixed old_z, Fixed old_tx, Fixed old_ty, Bool is_resize)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOM_Event evt;
	u32 i;
	GF_SceneGraph *scene = (subscene ? subscene : compositor->scene);
	GF_Node *root = gf_sg_get_root_node(scene);
	/*if root node is not DOM, sent a resize event (for VRML/BIFS). Otherwise this must be handled
	by the composition code of the node*/
	if (!root || (gf_node_get_tag(root) > GF_NODE_RANGE_LAST_VRML) )
		return;

	memset(&evt, 0, sizeof(GF_DOM_Event));
	evt.prev_scale = compositor->scale_x*old_z;
	evt.new_scale = compositor->scale_x*compositor->zoom;
	evt.bubbles = 1;

	if (is_resize) {
		evt.type = GF_EVENT_RESIZE;
		if (subscene == NULL) {
			evt.screen_rect.width = INT2FIX(compositor->display_width);
			evt.screen_rect.height = INT2FIX(compositor->display_height);
		} else {
			u32 w, h;
			gf_sg_get_scene_size_info(scene, &w, &h);
			evt.screen_rect.width = INT2FIX(w);
			evt.screen_rect.height = INT2FIX(h);
		}
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
	gf_dom_event_fire(gf_sg_get_root_node(scene), &evt);

	i=0;
	while ((scene = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
		gf_dom_event_fire(gf_sg_get_root_node(scene), &evt);
	}

#endif
}
void compositor_2d_set_user_transform(GF_Compositor *compositor, Fixed zoom, Fixed tx, Fixed ty, Bool is_resize)
{
	Fixed ratio;
	Fixed old_tx, old_ty, old_z;

	gf_sc_lock(compositor, GF_TRUE);
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
		compositor->zoom_changed = GF_TRUE;

		/*recenter visual*/
		if (!compositor->visual->center_coords) {
			Fixed c_x, c_y, nc_x, nc_y;
			c_x = INT2FIX(compositor->display_width/2);
			c_y = INT2FIX(compositor->display_height/2);
			nc_x = gf_mulfix(c_x, ratio);
			nc_y = gf_mulfix(c_y, ratio);
			compositor->trans_x -= (nc_x-c_x);
			compositor->trans_y -= (nc_y-c_y);
		}
	}
	gf_mx2d_init(compositor->traverse_state->transform);

	switch (compositor->rotate_mode) {
	case 1:
		gf_mx2d_add_rotation(&compositor->traverse_state->transform, 0, 0, -GF_PI/2);
		break;
	case 2:
		gf_mx2d_add_scale(&compositor->traverse_state->transform, -1, -1);
		break;
	case 3:
		gf_mx2d_add_rotation(&compositor->traverse_state->transform, 0, 0, GF_PI/2);
		break;
	}

	gf_mx2d_add_scale(&compositor->traverse_state->transform, gf_mulfix(compositor->zoom,compositor->scale_x), gf_mulfix(compositor->zoom,compositor->scale_y));

	gf_mx2d_add_translation(&compositor->traverse_state->transform, compositor->trans_x, compositor->trans_y);
	if (compositor->rotation) gf_mx2d_add_rotation(&compositor->traverse_state->transform, 0, 0, compositor->rotation);

	if (!compositor->visual->center_coords) {
		gf_mx2d_add_translation(&compositor->traverse_state->transform, INT2FIX(compositor->vp_x), INT2FIX(compositor->vp_y));
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Changing Zoom (%g) and Pan (%g %g)\n", FIX2FLT(compositor->zoom), FIX2FLT(compositor->trans_x) , FIX2FLT(compositor->trans_y)));


	gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
	compositor->traverse_state->invalidate_all = GF_TRUE;

	/*for zoom&pan, send the event right away. For resize/scroll, wait for the frame to be drawn before sending it
	otherwise viewport info of SVG nodes won't be correct*/
	if (!is_resize) compositor_send_resize_event(compositor, NULL, old_z, old_tx, old_ty, GF_FALSE);
	gf_sc_lock(compositor, GF_FALSE);
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
			gf_mx_apply_rect(&tr_state->layer_matrix, &orig);
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
		tr_state->has_layer_clip = GF_TRUE;
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			gf_mx_copy(tr_state->layer_matrix, tr_state->model_matrix);
		}
#endif
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

		tr_state->has_clip = GF_TRUE;
	}
	return clip;
}


/*overlay management*/
Bool visual_2d_overlaps_overlay(GF_VisualManager *visual, DrawableContext *ctx, GF_TraverseState *tr_state)
{
	u32 res = 0;
	GF_OverlayStack *ol;
	GF_Compositor *compositor = visual->compositor;
	if (compositor->visual != visual) return GF_FALSE;

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
			if (gf_irect_inside(&ol->ra.list[i].rect, &clip))
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
	return res ? GF_TRUE : GF_FALSE;
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
		Bool needs_draw = GF_TRUE;
		GF_IRect the_clip, vid_clip;

		ra_refresh(&ol->ra);

		for (i=0; i<ol->ra.count; i++) {
			the_clip = ol->ra.list[i].rect;

			/*draw all objects above this overlay*/
			ctx = ol->ctx->next;
			while (ctx && ctx->drawable) {
				if (gf_irect_overlaps(&ctx->bi->clip, &the_clip)) {
					GF_IRect prev_clip = ctx->bi->clip;

					if (needs_draw) {
						/*if first object above is not transparent and completely covers the overlay skip video redraw*/
						if ((ctx->flags & CTX_IS_TRANSPARENT) || !gf_irect_inside(&prev_clip, &the_clip)) {
							vid_clip = ol->ra.list[i].rect;
							gf_irect_intersect(&vid_clip, &ol->ctx->bi->clip);
							compositor_2d_draw_bitmap_ex(visual, ol->ctx->aspect.fill_texture, ol->ctx, &vid_clip, &ol->ctx->bi->unclip, 0xFF, tr_state, GF_TRUE);
						}
						needs_draw = GF_FALSE;
					}
					gf_irect_intersect(&ctx->bi->clip, &the_clip);
					tr_state->ctx = ctx;

					call_drawable_draw(ctx, tr_state, GF_FALSE);

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
		memset(&video_src, 0, sizeof(GF_VideoSurface));
		video_src.height = txh->height;
		video_src.width = txh->width;
		video_src.pitch_x = 0;
		video_src.pitch_y = txh->stride;
		video_src.pixel_format = txh->pixelformat;
		video_src.video_buffer = txh->data;

		e = visual->compositor->video_out->Blit(visual->compositor->video_out, &video_src, &ol->src, &ol->dst, 2);
		if (e) GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Visual2D] Error %s during overlay update\n", gf_error_to_string(e) ));

		ra_del(&ol->ra);
		gf_free(ol);
	}
}

void compositor_2d_init_callbacks(GF_Compositor *compositor)
{
	compositor->visual->DrawBitmap = compositor_2d_draw_bitmap;
}
