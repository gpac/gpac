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



#include "node_stacks.h"
#include "visual_manager.h"
#include "texturing.h"

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
	
#ifdef GPAC_USE_TINYGL
	ostgl_context *tgl_ctx;
#endif
} CompositeTextureStack;

static void composite_render(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(node);
		/*unregister visual*/
		render_visual_unregister(st->visual->render, st->visual);
		visual_del(st->visual);
		if (st->txh.data) free(st->txh.data);
		/*destroy texture*/
		gf_sr_texture_destroy(&st->txh);
#ifdef GPAC_USE_TINYGL
		if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif
		free(st);
	} else {
		/*render*/
		gf_node_render_children(node, rs);
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
		if (force_check || gf_node_dirty_get(c3d->background)) { gf_node_render(c3d->background, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
		if (btop != c3d->background) { 
			gf_node_unregister(c3d->background, n);
			gf_node_register(btop, n); 
			c3d->background = btop;
			gf_node_event_out_str(n, "background");
			ret = 1; 
		}
		if (force_check || gf_node_dirty_get(c3d->viewpoint)) { gf_node_render(c3d->viewpoint, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
		if (btop != c3d->viewpoint) { 
			gf_node_unregister(c3d->viewpoint, n);
			gf_node_register(btop, n); 
			c3d->viewpoint = btop; 
			gf_node_event_out_str(n, "viewpoint");
			ret = 1; 
		}
		
		if (force_check || gf_node_dirty_get(c3d->fog)) { gf_node_render(c3d->fog, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->fogs, 0);
		if (btop != c3d->fog) {
			gf_node_unregister(c3d->fog, n);
			gf_node_register(btop, n); 
			c3d->fog = btop;
			gf_node_event_out_str(n, "fog");
			ret = 1;
		}
		
		if (force_check || gf_node_dirty_get(c3d->navigationInfo)) { gf_node_render(c3d->navigationInfo, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->navigations, 0);
		if (btop != c3d->navigationInfo) {
			gf_node_unregister(c3d->navigationInfo, n);
			gf_node_register(btop, n); 
			c3d->navigationInfo = btop;
			gf_node_event_out_str(n, "navigationInfo");
			ret = 1;
		}
		return ret;
	}
#endif
	case TAG_MPEG4_CompositeTexture2D: 
	{
		M_CompositeTexture2D *c2d = (M_CompositeTexture2D*)n;
		if (force_check || gf_node_dirty_get(c2d->background)) { gf_node_render(c2d->background, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->backgrounds, 0);
		if (btop != c2d->background) {
			gf_node_unregister(c2d->background, n);
			gf_node_register(btop, n); 
			c2d->background = btop;
			gf_node_event_out_str(n, "background");
			ret = 1;
		}

		if (force_check || gf_node_dirty_get(c2d->viewport)) { gf_node_render(c2d->viewport, tr_state); ret = 1; }
		btop = (GF_Node*)gf_list_get(tr_state->viewpoints, 0);
		if (btop != c2d->viewport) { 
			gf_node_unregister(c2d->viewport, n);
			gf_node_register(btop, n); 
			c2d->viewport = btop;
			gf_node_event_out_str(n, "viewport");
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
	GF_STENCIL stencil;
	GF_Err e;
	M_Background2D *back;
	GF_TraverseState *tr_state;
	Bool invalidate_all;
	u32 new_pixel_format;
	Render *sr = (Render *)txh->compositor->visual_renderer->user_priv;
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(txh->owner);
	GF_Raster2D *raster = st->visual->render->compositor->r2d;

	if (st->unsupported) return;


	if (sr->recompute_ar) {
		gf_node_dirty_set(txh->owner, 0, 0);
		return;
	}

	if (!gf_node_dirty_get(txh->owner)) {
		txh->needs_refresh = 0;
		return;
	}
	gf_node_dirty_clear(st->txh.owner, 0);

	new_pixel_format = 0;
	back = gf_list_get(st->visual->back_stack, 0);
	if (back && back->isBound) new_pixel_format = GF_PIXEL_RGB_24;
	else new_pixel_format = GF_PIXEL_RGBA;

#ifdef GPAC_USE_TINYGL
	/*TinyGL pixel format is fixed at compile time, we cannot override it !*/
	new_pixel_format = GF_PIXEL_RGBA;
#else

	/*in OpenGL_ES, only RGBA can be safelly used with glReadPixels*/
#ifdef GPAC_USE_OGL_ES
	new_pixel_format = GF_PIXEL_RGBA;
#else
	/*no alpha support in offscreen rendering*/
	if (!(sr->compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA)) 
		new_pixel_format = GF_PIXEL_RGB_24;
#endif

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
	if (w<0) w = 0;
	if (h<0) h = 0;

	if (!w || !h) return;
	invalidate_all = 0;

	/*rebuild stencil*/
	if (!txh->hwtx 
		|| (w != (s32) txh->width) || ( h != (s32) txh->height) 
		|| (new_pixel_format != txh->pixelformat)
		) {

		Bool needs_stencil = 1;
		if (txh->hwtx) {
#ifdef GPAC_USE_TINYGL
			if (st->tgl_ctx) ostgl_delete_context(st->tgl_ctx);
#endif
			render_texture_release(txh);
			if (txh->data) free(txh->data);
			txh->data = NULL;
		}

		/*we don't use rect ext because of no support for texture transforms*/
		if (1 
#ifndef GPAC_DISABLE_3D
			|| sr->gl_caps.npot_texture 
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

		render_texture_allocate(txh);
		txh->pixelformat = new_pixel_format;
		if (new_pixel_format==GF_PIXEL_RGBA) {
			txh->stride = txh->width * 4;
			txh->transparent = 1;
		} else if (new_pixel_format==GF_PIXEL_RGB_565) {
			txh->stride = txh->width * 2;
			txh->transparent = 0;
		} else {
			txh->stride = txh->width * 3;
			txh->transparent = 0;
		}

		st->visual->width = txh->width;
		st->visual->height = txh->height;

		stencil = raster->stencil_new(raster, GF_STENCIL_TEXTURE);
		/*TODO - add support for compositeTexture3D when root is 2D visual*/
#ifndef GPAC_DISABLE_3D
		if (st->visual->type_3d) {
			Render *sr = st->visual->render;
			/*figure out what to do if main visual (eg video out) is not in OpenGL ...*/
			if (!sr->visual->type_3d) {
				/*create an offscreen window for OpenGL rendering*/
				if ((sr->offscreen_width < st->txh.width) || (sr->offscreen_height < st->txh.height)) {
#ifndef GPAC_USE_TINYGL
					GF_Event evt;
					sr->offscreen_width = MAX(sr->offscreen_width, st->txh.width);
					sr->offscreen_height = MAX(sr->offscreen_height, st->txh.height);

					evt.type = GF_EVENT_VIDEO_SETUP;
					evt.setup.width = sr->offscreen_width;
					evt.setup.height = sr->offscreen_height;
					evt.setup.back_buffer = 0;
					evt.setup.opengl_mode = 2;
					e = sr->compositor->video_out->ProcessEvent(sr->compositor->video_out, &evt);
					if (e) {
						render_texture_release(txh);
						st->unsupported = 1;
						return;
					}
#endif
				}
			} else {
				needs_stencil = 0;
			}
		}
#endif
	
		if (needs_stencil) {
			txh->data = (char*)malloc(sizeof(unsigned char) * txh->stride * txh->height);
			memset(txh->data, 0, sizeof(unsigned char) * txh->stride * txh->height);
			e = raster->stencil_set_texture(stencil, txh->data, txh->width, txh->height, txh->stride, txh->pixelformat, txh->pixelformat, 0);
			if (e) {
				raster->stencil_delete(stencil);
				render_texture_release(txh);
				free(txh->data);
				txh->data = NULL;
			}
#ifdef GPAC_USE_TINYGL
			if (st->visual->type_3d && !sr->visual->type_3d) {
				st->tgl_ctx = ostgl_create_context(txh->width, txh->height, txh->transparent ? 32 : 16, &txh->data, 1);
			}
#endif

		}
		invalidate_all = 1;
		render_texture_set_stencil(txh, stencil);
	}
	if (!txh->hwtx) return;
		
	stencil = render_texture_get_stencil(txh);
	if (!stencil) return;

#ifdef GPAC_USE_TINYGL
	if (st->tgl_ctx) ostgl_make_current(st->tgl_ctx, 0);
#endif

	GF_SAFEALLOC(tr_state, GF_TraverseState);
	tr_state->vrml_sensors = gf_list_new();
	tr_state->visual = st->visual;
	tr_state->invalidate_all = invalidate_all;

	if (st->visual->render->traverse_state->trav_flags & TF_RENDER_DIRECT) {
		tr_state->trav_flags = TF_RENDER_DIRECT;
	}

	gf_mx2d_init(tr_state->transform);
	gf_cmx_init(&tr_state->color_mat);

	tr_state->backgrounds = st->visual->back_stack;
	tr_state->viewpoints = st->visual->view_stack;
	tr_state->is_pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(st->txh.owner));
	tr_state->min_hsize = INT2FIX( MIN(txh->width, txh->height) ) / 2;

	composite_do_bindable(st->txh.owner, tr_state, st->first);
	st->first = 0;
		
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Entering CompositeTexture2D Render Cycle\n"));

	txh->needs_refresh = visual_render_frame(st->visual, st->txh.owner, tr_state, 0);

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

	if (txh->needs_refresh) {
#ifndef GPAC_DISABLE_3D
		if (st->visual->camera.is_3D) {
			if (st->visual->render->visual->type_3d) {
#ifndef GPAC_USE_TINYGL
				tx_copy_to_texture(&st->txh);
#else
				/*in TinyGL we only need to push associated bitmap to the texture*/
				render_texture_push_image(&st->txh, 0, 0);
#endif
			} else {
#ifndef GPAC_USE_TINYGL
				tx_copy_to_stencil(&st->txh);
#endif
			}
		} else 
#endif
		{
			if (raster->stencil_texture_modified) raster->stencil_texture_modified(stencil); 
			render_texture_set_stencil(txh, stencil);
		}
		gf_sr_invalidate(st->txh.compositor, NULL);
	}
	gf_list_del(tr_state->vrml_sensors);
	free(tr_state);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Leaving CompositeTexture2D Render Cycle\n"));
}


static GF_Err composite_get_video_access(GF_VisualManager *visual)
{
	GF_STENCIL stencil;
	GF_Err e;
	CompositeTextureStack *st = (CompositeTextureStack *) gf_node_get_private(visual->offscreen);

	if (!st->txh.hwtx || !visual->raster_surface) return GF_BAD_PARAM;
	stencil = render_texture_get_stencil(&st->txh);
	if (!stencil) return GF_BAD_PARAM;
	e = visual->render->compositor->r2d->surface_attach_to_texture(visual->raster_surface, stencil);
	if (!e) visual->is_attached = 1;
	return e;
}

static void composite_release_video_access(GF_VisualManager *visual)
{
	visual->render->compositor->r2d->surface_detach(visual->raster_surface);
}

void render_init_compositetexture2d(Render *sr, GF_Node *node)
{
	M_CompositeTexture2D *c2d = (M_CompositeTexture2D *)node;
	CompositeTextureStack *st;
	GF_SAFEALLOC(st, CompositeTextureStack);
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = composite_update;

	if ((c2d->repeatSandT==1) || (c2d->repeatSandT==3) ) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (c2d->repeatSandT>1) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;

	/*create composite visual*/
	st->visual = visual_new(sr);
	st->visual->offscreen = node;
	st->visual->GetSurfaceAccess = composite_get_video_access;
	st->visual->ReleaseSurfaceAccess = composite_release_video_access;
	st->visual->raster_surface = sr->compositor->r2d->surface_new(sr->compositor->r2d, 1);

	/*Bitmap drawn with brush, not hardware since we don't know how the graphics driver handles the texture bytes*/
	st->visual->DrawBitmap = NULL;
	st->visual->SupportsFormat = NULL;
	st->first = 1;
	st->visual->render = sr;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, composite_render);
	render_visual_register(sr, st->visual);
}


#ifndef GPAC_DISABLE_3D
void render_init_compositetexture3d(Render *sr, GF_Node *node)
{
	M_CompositeTexture3D *c3d = (M_CompositeTexture3D *)node;
	CompositeTextureStack *st;
	GF_SAFEALLOC(st, CompositeTextureStack);
	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = composite_update;

	if (c3d->repeatS) st->txh.flags |= GF_SR_TEXTURE_REPEAT_S;
	if (c3d->repeatT) st->txh.flags |= GF_SR_TEXTURE_REPEAT_T;

	/*create composite visual*/
	st->visual = visual_new(sr);
	st->visual->offscreen = node;
	st->visual->GetSurfaceAccess = composite_get_video_access;
	st->visual->ReleaseSurfaceAccess = composite_release_video_access;

	/*Bitmap drawn with brush, not hardware since we don't know how the graphics driver handles the texture bytes*/
	st->visual->DrawBitmap = NULL;
	st->visual->SupportsFormat = NULL;
	st->first = 1;
	st->visual->render = sr;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, composite_render);
	render_visual_register(sr, st->visual);

	if (! (sr->compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN)) {
		st->visual->type_3d = 0;
		st->visual->camera.is_3D = 0;
	} else {
		st->visual->type_3d = 2;
		st->visual->camera.is_3D = 1;
	}
	camera_invalidate(&st->visual->camera);
}
#endif

GF_TextureHandler *render_get_composite_texture(GF_Node *node)
{
	CompositeTextureStack *st = (CompositeTextureStack*) gf_node_get_private(node);
	return &st->txh;
}

Bool render_composite_texture_handle_event(Render *sr, GF_Event *ev)
{
	GF_Matrix mx;
	GF_TraverseState *tr_state;
	GF_ChildNodeItem *children, *l;
	Bool res;
	SFVec3f txcoord;
	CompositeTextureStack *stack;
	M_Appearance *ap = (M_Appearance *)sr->hit_info.appear;
	assert(ap && ap->texture);

	if (ev->type > GF_EVENT_MOUSEMOVE) return 0;

	stack = gf_node_get_private(ap->texture);

	txcoord.x = sr->hit_info.hit_texcoords.x;
	txcoord.y = sr->hit_info.hit_texcoords.y;
	txcoord.z = 0;
	if (render_texture_get_transform(&stack->txh, ap->textureTransform, &mx)) {
		/*tx coords are inverted when mapping, thus applying directly the matrix will give us the 
		untransformed coords*/
		gf_mx_apply_vec(&mx, &txcoord);
		while (txcoord.x<0) txcoord.x += FIX_ONE; while (txcoord.x>FIX_ONE) txcoord.x -= FIX_ONE;
		while (txcoord.y<0) txcoord.y += FIX_ONE; while (txcoord.y>FIX_ONE) txcoord.y -= FIX_ONE;
	}
	/*convert to tx space*/
	ev->mouse.x = FIX2INT( (txcoord.x - FIX_ONE/2) * stack->visual->width);
	ev->mouse.y = FIX2INT( (txcoord.y - FIX_ONE/2) * stack->visual->height);


	GF_SAFEALLOC(tr_state, GF_TraverseState);
	tr_state->vrml_sensors = gf_list_new();
	tr_state->visual = stack->visual;
	tr_state->traversing_mode = TRAVERSE_PICK;
	tr_state->is_pixel_metrics = gf_sg_use_pixel_metrics(gf_node_get_graph(ap->texture));

	/*collect sensors*/
	l = children = ((M_CompositeTexture2D*)ap->texture)->children;
	while (l) {
		SensorHandler *hsens = render_get_sensor_handler(l->node);
		if (hsens) gf_list_add(tr_state->vrml_sensors, hsens);
		l = l->next;
	}

	res = visual_execute_event(stack->visual, tr_state, ev, children);
	gf_list_del(tr_state->vrml_sensors);
	free(tr_state);
	return res;
}

void render_adjust_composite_scale(GF_Node *node, Fixed *sx, Fixed *sy)
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
