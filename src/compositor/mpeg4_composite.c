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



#include <gpac/internal/scenegraph_dev.h>

#include "nodes_stacks.h"
#include "visual_manager.h"
#include "texturing.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

#ifdef GPAC_USE_TINYGL
#include <GL/oscontext.h>
#endif

typedef struct
{
	GF_TextureHandler txh;
	Fixed sx, sy;
	/*the visual object handling the texture*/
	GF_VisualManager *visual;
	Bool first, unsupported;
	GF_List *sensors, *previous_sensors, *temp_sensors, *temp_previous_sensors;
	GF_Node *prev_hit_appear;
	//revent recursions
	Bool in_handle_event;
	GF_TraverseState *tr_state;

#ifdef GPAC_USE_TINYGL
	ostgl_context *tgl_ctx;
#else
	Bool use_fbo;
#endif
} CompositeTextureStack;


static Bool composite2d_draw_bitmap(GF_VisualManager *visual, GF_TraverseState *tr_state, struct _drawable_context *ctx)
{
	u8 alpha = 0xFF;
	GF_VideoSurface offscreen_dst, video_src;
	GF_Window src_wnd, dst_wnd;
	Bool use_blit, has_scale;
	CompositeTextureStack *st;

	if (visual->compositor->disable_composite_blit) return 0;

	if (!ctx->aspect.fill_texture) return 1;
	if (ctx->transform.m[0]<0) return 0;
	if (ctx->transform.m[4]<0) {
		if (!(ctx->flags & CTX_FLIPED_COORDS)) return 0;
	} else {
		if (ctx->flags & CTX_FLIPED_COORDS) return 0;
	}
	if (ctx->transform.m[1] || ctx->transform.m[3]) return 0;
#ifndef GPAC_DISABLE_VRML
	if ((ctx->flags & CTX_HAS_APPEARANCE) && ctx->appear && ((M_Appearance*)ctx->appear)->textureTransform)
		return 0;
#endif

	alpha = GF_COL_A(ctx->aspect.fill_color);
	/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
	if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);
	if (!alpha) return 1;

	st = (CompositeTextureStack *) gf_node_get_private(visual->offscreen);

	if (!compositor_texture_rectangles(visual, ctx->aspect.fill_texture, &ctx->bi->clip, &ctx->bi->unclip, &src_wnd, &dst_wnd, &use_blit, &has_scale)) return 1;
	if (! ctx->aspect.fill_texture->data) return 0;

	memset(&video_src, 0, sizeof(GF_VideoSurface));
	video_src.height = ctx->aspect.fill_texture->height;
	video_src.width = ctx->aspect.fill_texture->width;
	video_src.pitch_x = 0;
	video_src.pitch_y = ctx->aspect.fill_texture->stride;
	video_src.pixel_format = ctx->aspect.fill_texture->pixelformat;
#ifdef GF_SR_USE_DEPTH
	if (ctx->aspect.fill_texture->pixelformat==GF_PIXEL_YUVD) video_src.pixel_format = GF_PIXEL_YUV;
#endif
	video_src.video_buffer = ctx->aspect.fill_texture->data;

	memset(&offscreen_dst, 0, sizeof(GF_VideoSurface));
	offscreen_dst.width = st->txh.width;
	offscreen_dst.height = st->txh.height;
	offscreen_dst.pitch_y = st->txh.stride;
	offscreen_dst.pixel_format = st->txh.pixelformat;
	offscreen_dst.video_buffer = st->txh.data;

	gf_stretch_bits(&offscreen_dst, &video_src, &dst_wnd, &src_wnd, alpha, 0, tr_state->col_key, ctx->col_mat);
	return 1;
}

static void composite_traverse(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		u32 i=0;
		GF_VisualManager *a_visual;
		CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
		/*unregister visual*/
		gf_sc_visual_unregister(st->visual->compositor, st->visual);

		/*We must make sure we don't keep pointers to this composite in the different visuals.
		  - we must track Appearance nodes at the compositor level to undo the textureTransform while picking
		  - but we clearly don't want to track destruction of all appearance nodes just to solve this texture delete
				=> remove the entire compositeTexture appearance state - this may lead to small bugs in interaction logics, however they should
				not be too damageable
		*/
		st->visual->compositor->hit_appear = NULL;
		st->visual->compositor->prev_hit_appear = NULL;

		while ( (a_visual = gf_list_enum(st->visual->compositor->visuals, &i))) {
			if (a_visual->offscreen) {
				CompositeTextureStack *a_st = (CompositeTextureStack *) gf_node_get_private(a_visual->offscreen);
				a_st->prev_hit_appear = NULL;
			}
		}

		visual_del(st->visual);
		if (st->txh.data) gf_free(st->txh.data);
		/*destroy texture*/
		gf_sc_texture_destroy(&st->txh);
#ifdef GPAC_USE_TINYGL
		if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif

		gf_list_del(st->sensors);
		gf_list_del(st->previous_sensors);

		gf_list_del(st->tr_state->vrml_sensors);
		gf_free(st->tr_state);

		gf_free(st);
	} else {
		gf_node_traverse_children(node, rs);
	}
}

static Bool composite_do_bindable(GF_Node *n, GF_TraverseState *tr_state, Bool force_check)
{
	GF_Node *btop;
	Bool ret = 0;
	switch (gf_node_get_tag(n)) {
#ifndef GPAC_DISABLE_3D
	case TAG_MPEG4_CompositeTexture3D:
	{
		M_CompositeTexture3D*c3d = (M_CompositeTexture3D*)n;
		if (force_check || gf_node_dirty_get(c3d->background)) {
			gf_node_traverse(c3d->background, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
		if (btop != c3d->background) {
			gf_node_unregister(c3d->background, n);
			gf_node_register(btop, n);
			c3d->background = btop;
			gf_node_event_out(n, 5/*"background"*/);
			ret = 1;
		}
		if (force_check || gf_node_dirty_get(c3d->viewpoint)) {
			gf_node_traverse(c3d->viewpoint, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
		if (btop != c3d->viewpoint) {
			gf_node_unregister(c3d->viewpoint, n);
			gf_node_register(btop, n);
			c3d->viewpoint = btop;
			gf_node_event_out(n, 8/*"viewpoint"*/);
			ret = 1;
		}

		if (force_check || gf_node_dirty_get(c3d->fog)) {
			gf_node_traverse(c3d->fog, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->fogs, 0);
		if (btop != c3d->fog) {
			gf_node_unregister(c3d->fog, n);
			gf_node_register(btop, n);
			c3d->fog = btop;
			gf_node_event_out(n, 6/*"fog"*/);
			ret = 1;
		}

		if (force_check || gf_node_dirty_get(c3d->navigationInfo)) {
			gf_node_traverse(c3d->navigationInfo, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->navigations, 0);
		if (btop != c3d->navigationInfo) {
			gf_node_unregister(c3d->navigationInfo, n);
			gf_node_register(btop, n);
			c3d->navigationInfo = btop;
			gf_node_event_out(n, 7/*"navigationInfo"*/);
			ret = 1;
		}
		return ret;
	}
#endif
	case TAG_MPEG4_CompositeTexture2D:
	{
		M_CompositeTexture2D *c2d = (M_CompositeTexture2D*)n;
		if (force_check || gf_node_dirty_get(c2d->background)) {
			gf_node_traverse(c2d->background, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
		if (btop != c2d->background) {
			gf_node_unregister(c2d->background, n);
			gf_node_register(btop, n);
			c2d->background = btop;
			gf_node_event_out(n, 5/*"background"*/);
			ret = 1;
		}

		if (force_check || gf_node_dirty_get(c2d->viewport)) {
			gf_node_traverse(c2d->viewport, tr_state);
			ret = 1;
		}
		btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
		if (btop != c2d->viewport) {
			gf_node_unregister(c2d->viewport, n);
			gf_node_register(btop, n);
			c2d->viewport = btop;
			gf_node_event_out(n, 6/*"viewport"*/);
			ret = 1;
		}

		return ret;
	}
	}
	return 0;
}

static void composite_update(GF_TextureHandler *txh)
{
	s32 w, h;
	GF_EVGStencil *stencil;
#ifndef GPAC_USE_GLES1X
	M_Background2D *back;
#endif
	GF_List *sensor_bck;
	Bool invalidate_all;
	u32 new_pixel_format;
	GF_Compositor *compositor = (GF_Compositor *)txh->compositor;
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(txh->owner);

	if (st->unsupported) return;

	/*
		if (compositor->recompute_ar) {
			gf_node_dirty_set(txh->owner, 0, 0);
			return;
		}
	*/
	if (!compositor->rebuild_offscreen_textures && (!compositor->text_edit_changed || !st->visual->has_text_edit ) && !gf_node_dirty_get(txh->owner)) {
		txh->needs_refresh = 0;
		return;
	}
	gf_node_dirty_clear(st->txh.owner, 0);

	/*in OpenGL_ES, only RGBA can be safelly used with glReadPixels*/
#if defined(GPAC_USE_GLES1X)
	new_pixel_format = GF_PIXEL_RGBA;

#else

	back = gf_list_get(st->visual->back_stack, 0);
	if (back && back->isBound) new_pixel_format = GF_PIXEL_RGB;
	else new_pixel_format = GF_PIXEL_RGBA;

#ifdef GPAC_USE_TINYGL
	/*TinyGL pixel format is fixed at compile time, we cannot override it !*/
	if (st->visual->type_3d) new_pixel_format = GF_PIXEL_RGBA;

#elif !defined(GPAC_DISABLE_3D)
	/*no alpha support in offscreen rendering*/
	if (!compositor->visual->type_3d && !compositor->hybrid_opengl && !compositor->fbo_id && (st->visual->type_3d) && !(compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA))
		new_pixel_format = GF_PIXEL_RGB;
#endif

#endif //GPAC_USE_GLES1X

	/*FIXME - we assume RGB+Depth+bitshape, we should check with the video out module*/
#if defined(GF_SR_USE_DEPTH) && !defined(GPAC_DISABLE_3D)
	if (st->visual->type_3d && (compositor->video_out->hw_caps & GF_VIDEO_HW_HAS_DEPTH) ) new_pixel_format = GF_PIXEL_RGBDS;
#endif


#ifndef GPAC_DISABLE_3D
	if (st->visual->type_3d>1) {
		w = ((M_CompositeTexture3D*)txh->owner)->pixelWidth;
		h = ((M_CompositeTexture3D*)txh->owner)->pixelHeight;
	} else
#endif
	{
		w = ((M_CompositeTexture2D*)txh->owner)->pixelWidth;
		h = ((M_CompositeTexture2D*)txh->owner)->pixelHeight;
	}

	/*internal GPAC hacks for testing color spaces*/
	if (w<-1) {
		w = -w;
		if (h<0) {
			h = -h;
			if (new_pixel_format==GF_PIXEL_RGBA) {
				new_pixel_format=GF_PIXEL_ARGB;
			} else {
				new_pixel_format=GF_PIXEL_BGR;
			}
		} else {
			if (new_pixel_format==GF_PIXEL_RGB) {
				new_pixel_format=GF_PIXEL_RGBX;
			}
		}
	}
	else if (h<-1) {
		h = -h;
		if (new_pixel_format==GF_PIXEL_RGB) {
			new_pixel_format=GF_PIXEL_RGBX;
		}
	}

	if (w<0) w = 0;
	if (h<0) h = 0;


	if (!w || !h) {
		if (txh->tx_io) {
#ifdef GPAC_USE_TINYGL
			if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif
			gf_sc_texture_release(txh);
			if (txh->data) gf_free(txh->data);
			txh->data = NULL;
			txh->width = txh->height = txh->stride = 0;
		}
		return;
	}
	invalidate_all = compositor->rebuild_offscreen_textures;

	/*rebuild stencil*/
	if (!txh->tx_io
	        || (w != (s32) txh->width) || ( h != (s32) txh->height)
	        || (new_pixel_format != txh->pixelformat)
	   ) {

		Bool needs_stencil = 1;
		if (txh->tx_io) {
#ifdef GPAC_USE_TINYGL
			if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif
			gf_sc_texture_release(txh);
			if (txh->data)
				gf_free(txh->data);
			txh->data = NULL;
		}

		/*we don't use rect ext because of no support for texture transforms*/
		if ((1)
#ifndef GPAC_DISABLE_3D
		        || compositor->gl_caps.npot_texture
#endif
		   ) {
			st->txh.width = w;
			st->txh.height = h;
			st->sx = st->sy = FIX_ONE;
		} else {
			st->txh.width = 2;
			while (st->txh.width<(u32)w) st->txh.width*=2;
			st->txh.height = 2;
			while (st->txh.height<(u32)h) st->txh.height*=2;

			st->sx = INT2FIX(st->txh.width) / w;
			st->sy = INT2FIX(st->txh.height) / h;
		}

		gf_sc_texture_allocate(txh);
		txh->pixelformat = new_pixel_format;
		switch (new_pixel_format) {
		case GF_PIXEL_RGBA:
		case GF_PIXEL_ARGB:
			txh->stride = txh->width * 4;
			txh->transparent = 1;
			break;
		case GF_PIXEL_RGB_565:
			txh->stride = txh->width * 2;
			txh->transparent = 0;
			break;
		case GF_PIXEL_RGBDS:
			txh->stride = txh->width * 4;
			txh->transparent = 1;
			break;
		case GF_PIXEL_RGB:
			txh->stride = txh->width * 3;
			txh->transparent = 0;
			break;
		}

		st->visual->width = txh->width;
		st->visual->height = txh->height;
		st->use_fbo = GF_FALSE;

		stencil = gf_evg_stencil_new(GF_STENCIL_TEXTURE);

#ifndef GPAC_DISABLE_3D
		if (st->visual->type_3d) {
			/*figure out what to do if main visual (eg video out) is not in OpenGL ...*/
			if (!compositor->visual->type_3d && !compositor->hybrid_opengl) {
				/*create an offscreen window for OpenGL rendering*/
				if (!compositor->fbo_id
					&& ((compositor->offscreen_width < st->txh.width) || (compositor->offscreen_height < st->txh.height))
				) {

					GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[CompositeTexture] Offscreen OpenGL is not possible if no OpenGL context is created - use hybridGL mode for compositor\n"));
					st->unsupported = GF_TRUE;
				}
			} else {
				needs_stencil = 0;
			}
		}
#endif

		if (needs_stencil) {
			txh->data = (char*)gf_malloc(sizeof(unsigned char) * txh->stride * txh->height);
			memset(txh->data, 0, sizeof(unsigned char) * txh->stride * txh->height);

			/*set stencil texture - we don't check error as an image could not be supported by the rasterizer
			but still supported by the blitter (case of RGBD/RGBDS)*/
			gf_evg_stencil_set_texture(stencil, txh->data, txh->width, txh->height, txh->stride, txh->pixelformat);

#ifdef GPAC_USE_TINYGL
			if (st->visual->type_3d && !compositor->visual->type_3d) {
				st->tgl_ctx = ostgl_create_context(txh->width, txh->height, txh->transparent ? 32 : 24, &txh->data, 1);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[CompositeTexture] Creating TinyGL Offscreen context %p (%d %d - pf %s)\n", st->tgl_ctx, txh->width, txh->width, gf_4cc_to_str(txh->pixelformat)));
			}
#endif
		}
#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_DISABLE_3D)
		else if (compositor->gl_caps.fbo) {
			if (gf_sc_texture_setup_fbo(&st->txh)==GF_OK)
				st->use_fbo = GF_TRUE;
		}
#endif

		invalidate_all = 1;
		gf_sc_texture_set_stencil(txh, stencil);
	}
	if (!txh->tx_io) return;

#ifndef GPAC_DISABLE_3D
	if (st->visual->camera.is_3D) {
#ifdef GPAC_USE_TINYGL
		st->visual->type_3d = 2;
#else
		if (compositor->visual->type_3d) {
			st->visual->type_3d = 2;
		} else if (compositor->hybrid_opengl || compositor->fbo_id) {
			st->visual->type_3d = 2;
		} else if (! (compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN)) {
			st->visual->type_3d = 0;
		} else {
			st->visual->type_3d = 2;
		}
#endif
	}
#endif

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

#ifdef GPAC_USE_TINYGL
	if (st->tgl_ctx)
		ostgl_make_current(st->tgl_ctx, 0);
#elif !defined(GPAC_DISABLE_3D)
	if (st->use_fbo) {
		gf_sc_texture_enable_fbo(&st->txh, GF_TRUE);
	}
#endif

	sensor_bck = st->tr_state->vrml_sensors;
	memset(st->tr_state, 0, sizeof(GF_TraverseState));
	st->tr_state->vrml_sensors = sensor_bck;
	st->tr_state->visual = st->visual;
#ifndef GPAC_DISABLE_3D
	st->tr_state->camera = &st->visual->camera;
#endif
	st->tr_state->invalidate_all = invalidate_all;

	st->tr_state->immediate_draw = st->visual->compositor->traverse_state->immediate_draw;

	gf_mx2d_init(st->tr_state->transform);
	gf_cmx_init(&st->tr_state->color_mat);

	st->tr_state->backgrounds = st->visual->back_stack;
	st->tr_state->viewpoints = st->visual->view_stack;
	st->tr_state->pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(st->txh.owner));
	st->tr_state->min_hsize = INT2FIX( MIN(txh->width, txh->height) ) / 2;
	st->tr_state->vp_size.x = INT2FIX(txh->width);
	st->tr_state->vp_size.y = INT2FIX(txh->height);

	composite_do_bindable(st->txh.owner, st->tr_state, st->first);
	st->first = 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[CompositeTexture] Entering draw cycle\n"));

	txh->needs_refresh = visual_draw_frame(st->visual, st->txh.owner, st->tr_state, 0);
	txh->transparent = (st->visual->last_had_back==2) ? 0 : 1;

	if (!compositor->edited_text && st->visual->has_text_edit)
		st->visual->has_text_edit = 0;


#if 0
	/*set active viewport in image coordinates top-left=(0, 0), not in BIFS*/
	if (gf_list_count(st->visual->view_stack)) {
		M_Viewport *vp = (M_Viewport *)gf_list_get(st->visual->view_stack, 0);

		if (vp->isBound) {
			SFVec2f size = vp->size;
			if (size.x >=0 && size.y>=0) {
				/*FIXME - we need tracking of VP changes*/
				txh->needs_refresh = 1;
			}
		}
	}
#endif

	if (txh->needs_refresh) {
#ifndef GPAC_DISABLE_3D

#ifndef GPAC_USE_TINYGL
		if (st->use_fbo) {
			gf_sc_texture_enable_fbo(&st->txh, GF_FALSE);
		} else
#endif
		//no FBO, for composite 3D, store current buffer to texture
		if (st->visual->camera.is_3D && (st->visual->compositor->visual->type_3d || st->visual->compositor->hybrid_opengl)) {
#ifndef GPAC_USE_TINYGL
			gf_sc_copy_to_texture(&st->txh);
#endif
		}
		else if (st->visual->camera.is_3D) {
			if (st->visual->compositor->visual->type_3d) {
#ifndef GPAC_USE_TINYGL
				gf_sc_copy_to_texture(&st->txh);
#else
				/*in TinyGL we only need to push associated bitmap to the texture*/
				gf_sc_texture_push_image(&st->txh, 0, 0);
#endif
			} else {
#ifndef GPAC_USE_TINYGL
				gf_sc_copy_to_stencil(&st->txh);
#else
				if (txh->pixelformat==GF_PIXEL_RGBDS)
					gf_get_tinygl_depth(&st->txh);
#endif
			}
		} else
#endif
		{
			gf_sc_texture_set_stencil(txh, stencil);
		}
		gf_sc_invalidate(st->txh.compositor, NULL);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[CompositeTexture] Leaving draw cycle\n"));
}


GF_Err composite_get_video_access(GF_VisualManager *visual)
{
	GF_EVGStencil *stencil;
	GF_Err e;
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(visual->offscreen);

	if (!st->txh.tx_io || !visual->raster_surface) return GF_BAD_PARAM;
	stencil = gf_sc_texture_get_stencil(&st->txh);
	if (!stencil) return GF_BAD_PARAM;
	e = gf_evg_surface_attach_to_texture(visual->raster_surface, stencil);
	if (!e) visual->is_attached = 1;
	return e;
}

void composite_release_video_access(GF_VisualManager *visual)
{
}

Bool composite_check_visual_attached(GF_VisualManager *visual)
{
	return visual->is_attached;
}

void compositor_init_compositetexture2d(GF_Compositor *compositor, GF_Node *node)
{
	M_CompositeTexture2D *c2d = (M_CompositeTexture2D *)node;
	CompositeTextureStack *st;
	GF_SAFEALLOC(st, CompositeTextureStack);

	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate composite texture stack\n"));
		return;
	}

	GF_SAFEALLOC(st->tr_state, GF_TraverseState);
	if (!st->tr_state) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate composite texture state\n"));
		return;
	}
	st->tr_state->vrml_sensors = gf_list_new();

	st->sensors = gf_list_new();
	st->previous_sensors = gf_list_new();
	gf_sc_texture_setup(&st->txh, compositor, node);
	/*remove texture from compositor and add it at the end, so that any sub-textures are handled before*/
	gf_list_del_item(compositor->textures, &st->txh);
	gf_list_add(compositor->textures, &st->txh);

	st->txh.update_texture_fcnt = composite_update;

	if ((c2d->repeatSandT==1) || (c2d->repeatSandT==3) ) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (c2d->repeatSandT>1) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;

	/*create composite visual*/
	st->visual = visual_new(compositor);
	st->visual->offscreen = node;
	st->visual->GetSurfaceAccess = composite_get_video_access;
	st->visual->ReleaseSurfaceAccess = composite_release_video_access;
	st->visual->DrawBitmap = composite2d_draw_bitmap;
	st->visual->CheckAttached = composite_check_visual_attached;

	st->visual->raster_surface = gf_evg_surface_new(1);


	st->first = 1;
	st->visual->compositor = compositor;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, composite_traverse);
	gf_sc_visual_register(compositor, st->visual);
}


#ifndef GPAC_DISABLE_3D
void compositor_init_compositetexture3d(GF_Compositor *compositor, GF_Node *node)
{
	M_CompositeTexture3D *c3d = (M_CompositeTexture3D *)node;
	CompositeTextureStack *st;

	if (!gf_sc_check_gl_support(compositor)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Driver disabled, cannot render 3D composite textures\n"));
		return;
	}

	GF_SAFEALLOC(st, CompositeTextureStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate composite texture stack\n"));
		return;
	}
	GF_SAFEALLOC(st->tr_state, GF_TraverseState);
	if (!st->tr_state) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate composite texture state\n"));
		return;
	}

	st->tr_state->vrml_sensors = gf_list_new();

	st->sensors = gf_list_new();
	st->previous_sensors = gf_list_new();
	gf_sc_texture_setup(&st->txh, compositor, node);
	/*remove texture from compositor and add it at the end, so that any sub-textures are handled before*/
	gf_list_del_item(compositor->textures, &st->txh);
	gf_list_add(compositor->textures, &st->txh);

	st->txh.update_texture_fcnt = composite_update;

	if (c3d->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (c3d->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;

	/*create composite visual*/
	st->visual = visual_new(compositor);
	st->visual->offscreen = node;
	st->visual->GetSurfaceAccess = composite_get_video_access;
	st->visual->ReleaseSurfaceAccess = composite_release_video_access;
	st->visual->CheckAttached = composite_check_visual_attached;

	st->visual->camera.is_3D = 1;
	st->first = 1;
	st->visual->compositor = compositor;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, composite_traverse);
	gf_sc_visual_register(compositor, st->visual);

	camera_invalidate(&st->visual->camera);
}
#endif

GF_TextureHandler *compositor_get_composite_texture(GF_Node *node)
{
	CompositeTextureStack *st = (CompositeTextureStack*) gf_node_get_private(node);
	return &st->txh;
}

Bool compositor_compositetexture_handle_event(GF_Compositor *compositor, GF_Node *composite_appear, GF_Event *ev, Bool is_flush)
{
	GF_Ray ray;
	Fixed dist;
	Bool had_text_sel=0;
	GF_Matrix mx;
	GF_ChildNodeItem *children, *l;
	Bool res;
	SFVec3f txcoord, loc_pt, world_pt;
	GF_Matrix l2w_mx, w2l_mx;
	CompositeTextureStack *stack;
	GF_Node *appear, *prev_appear;
	GF_List *sensor_bck;
	M_Appearance *ap = (M_Appearance *)composite_appear;
	assert(ap && ap->texture);

	if (ev->type > GF_EVENT_MOUSEMOVE) return 0;
	stack = gf_node_get_private(ap->texture);
	if (!stack->txh.tx_io) return 0;

	if (stack->in_handle_event)
		return 0;

	children = NULL;
	stack->in_handle_event = GF_TRUE;

	if (!is_flush) {
		txcoord.x = compositor->hit_texcoords.x;
		txcoord.y = compositor->hit_texcoords.y;
		txcoord.z = 0;
		if (gf_sc_texture_get_transform(&stack->txh, ap->textureTransform, &mx, 1)) {
			/*tx coords are inverted when mapping, thus applying directly the matrix will give us the
			untransformed coords*/
			gf_mx_apply_vec(&mx, &txcoord);
			while (txcoord.x<0) txcoord.x += FIX_ONE;
			while (txcoord.x>FIX_ONE) txcoord.x -= FIX_ONE;
			while (txcoord.y<0) txcoord.y += FIX_ONE;
			while (txcoord.y>FIX_ONE) txcoord.y -= FIX_ONE;
		}

		/*convert to tx space*/
		ev->mouse.x = FIX2INT( (txcoord.x - FIX_ONE/2) * stack->visual->width + FIX_ONE/2);
		ev->mouse.y = FIX2INT( (txcoord.y - FIX_ONE/2) * stack->visual->height + FIX_ONE/2);

		sensor_bck = stack->tr_state->vrml_sensors;
		memset(stack->tr_state, 0, sizeof(GF_TraverseState));
		stack->tr_state->vrml_sensors = sensor_bck;

		stack->tr_state->visual = stack->visual;
		stack->tr_state->traversing_mode = TRAVERSE_PICK;
		stack->tr_state->pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(ap->texture));
		stack->tr_state->vp_size.x = INT2FIX(stack->txh.width);
		stack->tr_state->vp_size.y = INT2FIX(stack->txh.height);
		stack->tr_state->color_mat.identity = 1;

		gf_mx2d_init(stack->tr_state->transform);
#ifndef GPAC_DISABLE_3D
		gf_mx_init(stack->tr_state->model_matrix);
#endif
		/*collect sensors but not anchors*/
		l = children = ((M_CompositeTexture2D*)ap->texture)->children;
		while (l) {
			GF_SensorHandler *hsens = compositor_mpeg4_get_sensor_handler_ex(l->node, GF_TRUE);
			if (hsens) gf_list_add(stack->tr_state->vrml_sensors, hsens);
			l = l->next;
		}
	}

	stack->temp_sensors = compositor->sensors;
	stack->temp_previous_sensors = compositor->previous_sensors;
	compositor->sensors = stack->sensors;
	compositor->previous_sensors = stack->previous_sensors;

	ray = compositor->hit_world_ray;
	dist = compositor->hit_square_dist;
	prev_appear = compositor->prev_hit_appear;

	/*protect against destrucion in case of self-destroy*/
	if (prev_appear) {
		gf_node_register(prev_appear, NULL);
	}
	compositor->prev_hit_appear = stack->prev_hit_appear;
	appear = compositor->hit_appear;
	compositor->hit_appear = NULL;

	/*also backup current hit state in case we hit a node in the texture but don't consume the event*/
	loc_pt = compositor->hit_local_point;
	world_pt = compositor->hit_world_point;
	gf_mx_copy(l2w_mx, compositor->hit_local_to_world);
	gf_mx_copy(w2l_mx, compositor->hit_world_to_local);

	if (compositor->text_selection) had_text_sel=1;

	if (is_flush) {
		res = 0;
		gf_list_reset(stack->sensors);
		gf_sc_exec_event_vrml(compositor, ev);
	} else {
		res = visual_execute_event(stack->visual, stack->tr_state, ev, children);
	}

	if (!had_text_sel && compositor->edited_text) {
		stack->visual->has_text_edit = 1;
	} else if (!compositor->text_selection) {
		stack->visual->has_text_edit = 0;
	}

	if (!res) {
		compositor->hit_local_point = loc_pt;
		gf_mx_copy(compositor->hit_local_to_world, l2w_mx);
		gf_mx_copy(compositor->hit_world_to_local, w2l_mx);
	}

	compositor->hit_world_point = world_pt;
	compositor->hit_world_ray = ray;
	compositor->hit_square_dist = dist;

	stack->sensors = compositor->sensors;
	stack->previous_sensors = compositor->previous_sensors;
	compositor->sensors = stack->temp_sensors;
	stack->temp_sensors = NULL;
	compositor->previous_sensors = stack->temp_previous_sensors;
	stack->temp_previous_sensors = NULL;


	if (!is_flush) {
#ifndef GPAC_DISABLE_3D
		if (stack->tr_state->layer3d) compositor->traverse_state->layer3d = stack->tr_state->layer3d;
#endif
	}

	stack->prev_hit_appear = compositor->prev_hit_appear;

	//finally unregister the node, this may destroy the stack !
	if (prev_appear) {
		if (prev_appear->sgprivate->num_instances>1) {
			compositor->prev_hit_appear = prev_appear;
			compositor->hit_appear = appear;
		} else {
			compositor->prev_hit_appear = NULL;
			compositor->hit_appear = NULL;
		}
		gf_node_unregister(prev_appear, NULL);
	} else {
		compositor->prev_hit_appear = prev_appear;
		compositor->hit_appear = appear;
	}
	stack->in_handle_event = GF_FALSE;

	return res;
}

void compositor_compositetexture_sensor_delete(GF_Node *composite_appear, GF_SensorHandler *hdl)
{
	CompositeTextureStack *stack = gf_node_get_private(composite_appear);
	gf_list_del_item(stack->previous_sensors, hdl);
	gf_list_del_item(stack->sensors, hdl);
	if (stack->temp_sensors)
		gf_list_del_item(stack->temp_sensors, hdl);
	if (stack->temp_previous_sensors)
		gf_list_del_item(stack->temp_previous_sensors, hdl);
}


void compositor_adjust_scale(GF_Node *node, Fixed *sx, Fixed *sy)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_CompositeTexture2D:
	case TAG_MPEG4_CompositeTexture3D:
	{
		CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
		(*sx) = gf_divfix(*sx, st->sx);
		(*sy) = gf_divfix(*sy, st->sy);
		break;
	}
	default:
		return;
	}
}

Bool compositor_is_composite_texture(GF_Node *appear)
{
	M_Appearance *ap = NULL;
	u32 tag;
	if (!appear) return 0;

	tag = gf_node_get_tag(appear);
	if (tag==TAG_MPEG4_Appearance) ap = (M_Appearance *)appear;
#ifndef GPAC_DISABLE_X3D
	else if (tag==TAG_X3D_Appearance) ap = (M_Appearance *)appear;
#endif
	if (!ap) return 0;
	if (!ap->texture) return 0;
	switch (gf_node_get_tag(((M_Appearance *)appear)->texture)) {
	case TAG_MPEG4_CompositeTexture2D:
	case TAG_MPEG4_CompositeTexture3D:
		return 1;
	}
	return 0;
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
