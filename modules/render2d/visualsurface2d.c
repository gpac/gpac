/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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

#include "stacks2d.h"
#include "visualsurface2d.h"

VisualSurface2D *NewVisualSurface2D()
{
	VisualSurface2D *tmp;
	GF_SAFEALLOC(tmp, VisualSurface2D);

	tmp->center_coords = 1;
	ra_init(&tmp->to_redraw);
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();
	tmp->prev_nodes_drawn = gf_list_new();
	return tmp;
}

void DeleteVisualSurface2D(VisualSurface2D *surf)
{
	u32 i;
	ra_del(&surf->to_redraw);
	VS2D_ResetGraphics(surf);

	for (i=0; i<surf->alloc_contexts; i++)
		DeleteDrawableContext(surf->contexts[i]);
	
	free(surf->contexts);
	gf_list_del(surf->back_stack);
	gf_list_del(surf->view_stack);
	gf_list_del(surf->prev_nodes_drawn);
	free(surf);
}


#if defined(_WIN32_WCE) || defined(__SYMBIAN32__)
#define CONTEXT_ALLOC_STEP	1
#else
#define CONTEXT_ALLOC_STEP	20
#endif

DrawableContext *VS2D_GetDrawableContext(VisualSurface2D *surf)
{
	u32 i;
	if (surf->alloc_contexts==surf->num_contexts) {
		DrawableContext **newctx;
		surf->alloc_contexts += CONTEXT_ALLOC_STEP;
		newctx = (DrawableContext **) malloc(surf->alloc_contexts * sizeof(DrawableContext *) );
		for (i=0; i<surf->num_contexts; i++) newctx[i] = surf->contexts[i];
		for (i=surf->num_contexts; i<surf->alloc_contexts; i++) newctx[i] = NewDrawableContext();
		free(surf->contexts);
		surf->contexts = newctx;
	}
	i = surf->num_contexts;
	surf->num_contexts++;
	drawctx_reset(surf->contexts[i]);
	return surf->contexts[i];

}
void VS2D_RemoveLastContext(VisualSurface2D *surf)
{
	if (surf->num_contexts) surf->num_contexts--;
}


void VS2D_DrawableDeleted(struct _visual_surface_2D *surf, struct _drawable *node)
{
	gf_list_del_item(surf->prev_nodes_drawn, node);

	/*check node isn't being tracked*/
	if (surf->render->grab_node==node) {
		surf->render->grab_ctx = NULL;
		surf->render->grab_node = NULL;
		surf->render->is_tracking = 0;
	}
}


GF_Err VS2D_InitDraw(VisualSurface2D *surf, RenderEffect2D *eff)
{
	GF_Err e;
	u32 i, count;
	GF_Rect rc;
	DrawableContext *ctx;
	M_Background2D *bck;
	Bool direct_render;
	surf->num_contexts = 0;
	eff->surface = surf;
	eff->draw_background = 0;
	gf_mx2d_copy(surf->top_transform, eff->transform);
	eff->back_stack = surf->back_stack;
	eff->view_stack = surf->view_stack;

	/*setup clipper*/
	if (surf->center_coords) {
		if (!surf->composite) {
			if (surf->render->scalable_zoom)
				rc = gf_rect_center(INT2FIX(surf->render->compositor->width), INT2FIX(surf->render->compositor->height));
			else
				rc = gf_rect_center(INT2FIX(surf->render->cur_width + 2*surf->render->offset_x), INT2FIX(surf->render->cur_height + 2*surf->render->offset_y));
		} else {
			rc = gf_rect_center(INT2FIX(surf->width), INT2FIX(surf->height));
		}
	} else {
		rc.x = 0;
		rc.width = INT2FIX(surf->width);
		rc.y = rc.height = INT2FIX(surf->height);
	}
	/*set top-transform to pixelMetrics*/
	if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&eff->transform, eff->min_hsize, eff->min_hsize);

	surf->surf_rect = gf_rect_pixelize(&rc);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Top surface rectangle setup - width %d height %d\n", surf->surf_rect.width, surf->surf_rect.height));

	/*setup surface, brush and pen */
	e = VS2D_InitSurface(surf);
	if (e) return e;

	/*setup top clipper*/
	if (surf->center_coords) {
		rc = gf_rect_center(INT2FIX(surf->width), INT2FIX(surf->height));
	} else {
		rc.width = INT2FIX(surf->width);
		rc.height = INT2FIX(surf->height);
		rc.x = 0;
		rc.y = rc.height;
		if (surf->render->surface==surf) {
			rc.x += INT2FIX(surf->render->offset_x);
			rc.y += INT2FIX(surf->render->offset_y);
		}
	}
	/*setup viewport*/
	if (gf_list_count(surf->view_stack)) {
		M_Viewport *vp = (M_Viewport *) gf_list_get(surf->view_stack, 0);
		vp_setup((GF_Node *) vp, eff, &rc);
	}

	surf->top_clipper = gf_rect_pixelize(&rc);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Top surface cliper setup - %d:%d@%dx%d\n", surf->top_clipper.x, surf->top_clipper.y, surf->top_clipper.width, surf->top_clipper.height));

	/*if we're requested to invalidate everything, switch to direct render but still request bounds storage*/
	if (eff->invalidate_all) {
		eff->trav_flags |= TF_RENDER_DIRECT | TF_RENDER_STORE_BOUNDS;
		direct_render = 2;
	} else {
		direct_render = (eff->trav_flags & TF_RENDER_DIRECT) ? 1 : 0;
	}

	/*reset sensors*/
	surf->has_sensors = 0;

	/*reset prev nodes if any (previous render was indirect)*/
	count = gf_list_count(surf->prev_nodes_drawn);
	for (i=0; i<count; i++) {
		Drawable *dr = (Drawable *)gf_list_get(surf->prev_nodes_drawn, i);
		if (direct_render) {
			/*direct rendering mode, remove all bounds info and unreg node */
			drawable_reset_bounds(dr);
			gf_list_rem(surf->prev_nodes_drawn, i);
			i--;
			count--;
		} else {
			drawable_flush_bounds(dr, surf);
		}
	}
	if (!direct_render) return GF_OK;

	/*direct mode, draw background*/
	bck = (M_Background2D*) gf_list_get(surf->back_stack, 0);
	if (bck && bck->isBound) {
		ctx = b2D_GetContext(bck, surf->back_stack);
		if (ctx) {
			ctx->bi->clip = surf->surf_rect;
			ctx->bi->unclip = gf_rect_ft(&surf->surf_rect);
			eff->draw_background = 1;
			gf_node_render((GF_Node *) bck, eff);
			eff->draw_background = 0;
		} else {
			VS2D_Clear(surf, NULL, 0);
		}
	} else {
		VS2D_Clear(surf, NULL, 0);
	}
	return GF_OK;
}

void VS2D_RegisterSensor(VisualSurface2D *surf, DrawableContext *ctx)
{
	SensorContext *sc;
	
	sc = ctx->sensor;
	while (sc) {
		/*if any of the attached sensor is active, register*/
		if (sc->h_node->IsEnabled(sc->h_node)) {
			surf->has_sensors = 1;
			return;
		}
	}
	/*disable all sensors*/
	drawctx_reset_sensors(ctx);
	
	/*check for composite texture*/
	if (!ctx->h_texture || !(ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE) ) return;

	surf->has_sensors = 1;
}

#ifdef TRACK_OPAQUE_REGIONS

#define CHECK_UNCHANGED		0

static void mark_opaque_areas(VisualSurface2D *surf)
{
	u32 i, k;
#if CHECK_UNCHANGED
	Bool remove;
#endif
	GF_RectArray *ra = &surf->to_redraw;
	if (!ra->count) return;
	ra->opaque_node_index = (u32*)realloc(ra->opaque_node_index, sizeof(u32) * ra->count);

	for (k=0; k<ra->count; k++) {
#if CHECK_UNCHANGED
		remove = 1;
#endif
		ra->opaque_node_index[k] = 0;

		for (i=surf->num_contexts; i>0; i--) {
			DrawableContext *ctx = surf->contexts[i-1];

			if (!gf_irect_inside(&ctx->bi->clip, &ra->list[k]) ) {
#if CHECK_UNCHANGED
				if (remove && gf_rect_overlaps(&ctx->bi->clip, ra->list[k])) {
					if (ctx->need_redraw) remove = 0;
				}
#endif
				continue;
			}

			/*which opaquely covers the given area */
			if (! (ctx->flags & CTX_IS_TRANSPARENT) ) {
#if CHECK_UNCHANGED
				/*the opaque area has nothing changed above, remove the dirty rect*/
				if (remove && !ctx->need_redraw) {
					u32 j = ra->count - k - 1;
					if (j) {
						memmove(&ra->list[j], &ra->list[j+1], sizeof(GF_IRect)*j);
						memmove(&ra->opaque_node_index[j], &ra->opaque_node_index[j+1], sizeof(u32)*j);
					}
					ra->count--;
					k--;
				} 
				/*opaque area has something changed above, mark index */
				else 
#endif
				{
					ra->opaque_node_index[k] = i;
				}
				break;
#if CHECK_UNCHANGED
			} else {
				if (ctx->need_redraw) remove = 0;
#endif
			}
		}
	}
}
#endif


/*this defines a partition of the rendering area in small squares of the given width. If the number of nodes
to redraw exceeds the possible number of squares on the surface, the entire area is redrawn - this avoids computing
dirty rects algo when a lot of small shapes are moving*/
#define MIN_BBOX_SIZE	16

#if 0
#define CHECK_MAX_NODE	if (surf->to_redraw.count > max_nodes_allowed) redraw_all = 1;
#else
#define CHECK_MAX_NODE
#endif


Bool VS2D_TerminateDraw(VisualSurface2D *surf, RenderEffect2D *eff)
{
	u32 j, k, i, count, max_nodes_allowed, num_changed;
	GF_IRect refreshRect;
	Bool redraw_all;
	M_Background2D *bck;
	DrawableContext *bck_ctx;
	DrawableContext *ctx;
	Bool has_changed = 0;

	/*in direct mode the surface is always redrawn*/
	if (eff->trav_flags & TF_RENDER_DIRECT) {
		VS2D_TerminateSurface(surf);
		return 1;
	}

	num_changed = 0;
	
	/*if the aspect ratio has changed redraw everything*/
	redraw_all = eff->invalidate_all;

	/*check for background changes for transparent nodes*/
	bck = NULL;
	bck_ctx = NULL;

	bck = (M_Background2D*)gf_list_get(surf->back_stack, 0);
	if (bck) {
		if (!bck->isBound) {
			if (surf->last_had_back) redraw_all = 1;
			surf->last_had_back = 0;
		} else {
			if (!surf->last_had_back) redraw_all = 1;
			surf->last_had_back = 1;
			bck_ctx = b2D_GetContext(bck, surf->back_stack);
			if (bck_ctx->flags & CTX_REDRAW_MASK) redraw_all = 1;
		}
	} else if (surf->last_had_back) {
		surf->last_had_back = 0;
		redraw_all = 1;
	}
	
	max_nodes_allowed = (u32) ((surf->top_clipper.width / MIN_BBOX_SIZE) * (surf->top_clipper.height / MIN_BBOX_SIZE));
	count = surf->num_contexts;
	for (i=0; i<count; i++) {
		ctx = surf->contexts[i];

		drawctx_update_info(ctx, surf);
		if (ctx->flags & CTX_REDRAW_MASK) {
			num_changed ++;

			/*node has changed, add to redraw area*/
			if (!redraw_all) {
				ra_union_rect(&surf->to_redraw, &ctx->bi->clip);
//				CHECK_MAX_NODE
			}
		}
		/*otherwise try to remove any sensor hidden below*/
		//if (!ctx->transparent) remove_hidden_sensors(surf, num_to_draw, ctx);
	}

	/*garbage collection*/

	/*clear all remaining bounds since last frames (the ones that moved or that are not drawn this frame)*/
	count = gf_list_count(surf->prev_nodes_drawn);
	for (j=0; j<count; j++) {
		Drawable *n = (Drawable *)gf_list_get(surf->prev_nodes_drawn, j);
		while (drawable_get_previous_bound(n, &refreshRect, surf)) {
			if (!redraw_all) {
				ra_union_rect(&surf->to_redraw, &refreshRect);
//				CHECK_MAX_NODE
			}
		}
		/*if node is marked as undrawn, remove from surface*/
		if (!(n->flags & DRAWABLE_DRAWN_ON_SURFACE)) {
			drawable_flush_bounds(n, surf);
			gf_list_rem(surf->prev_nodes_drawn, j);
			j--;
			count--;
		}
	}
	/*no visual rendering*/
	//if (!surf->render->cur_width && !surf->render->cur_height) goto exit;

//	CHECK_MAX_NODE

	if (redraw_all) {
		ra_clear(&surf->to_redraw);
		ra_add(&surf->to_redraw, &surf->surf_rect);
	} else {
		ra_refresh(&surf->to_redraw);
	}
#ifdef TRACK_OPAQUE_REGIONS
	/*mark opaque areas to speed up*/
	mark_opaque_areas(surf);
#endif

	/*nothing to redraw*/
	if (ra_is_empty(&surf->to_redraw) ) goto exit;
	has_changed = 1;
	eff->traversing_mode = TRAVERSE_DRAW;

	/*redraw everything*/
	if (bck_ctx) {
		drawable_check_bounds(bck_ctx, surf);
		eff->ctx = bck_ctx;
#ifdef TRACK_OPAQUE_REGIONS
		surf->draw_node_index = 0;
#endif
		bck_ctx->bi->unclip = gf_rect_ft(&surf->surf_rect);
		bck_ctx->bi->clip = surf->surf_rect;
		gf_node_render(bck_ctx->node->owner, eff);
	} else {
		count = surf->to_redraw.count;
		if (bck_ctx) bck_ctx->bi->unclip = gf_rect_ft(&surf->surf_rect);
		for (k=0; k<count; k++) {
			GF_IRect rc;
			/*opaque area, skip*/
#ifdef TRACK_OPAQUE_REGIONS
			if (surf->to_redraw.opaque_node_index[k] > 0) continue;
#endif
			rc = surf->to_redraw.list[k];
			gf_irect_intersect(&rc, &surf->top_clipper);
			VS2D_Clear(surf, &rc, 0);
		}
	}

#ifndef GPAC_DISABLE_LOG
	if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RENDER)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Redraw %d / %d nodes (all: %s)", num_changed, surf->num_contexts, redraw_all ? "yes" : "no"));
		if (surf->to_redraw.count>1) GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\n"));

		for (i=0; i<surf->to_redraw.count; i++)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\tDirtyRect #%d: %d:%d@%dx%d\n", i+1, surf->to_redraw.list[i].x, surf->to_redraw.list[i].y, surf->to_redraw.list[i].width, surf->to_redraw.list[i].height));
	}
#endif
	
	refreshRect = surf->to_redraw.list[0];
	for (i=0; i<surf->num_contexts; i++) {
		ctx = surf->contexts[i];
		if ((surf->to_redraw.count>1) || gf_irect_overlaps(&ctx->bi->clip, &refreshRect)) {
#ifdef TRACK_OPAQUE_REGIONS
			surf->draw_node_index = i+1;
#endif
			eff->ctx = ctx;
			gf_node_render(ctx->node->owner, eff);
		}
		/*keep track of node drawn*/
		if (gf_list_find(surf->prev_nodes_drawn, ctx->node)<0) 
			gf_list_add(surf->prev_nodes_drawn, ctx->node);
	}

exit:
	/*clear dirty rects*/
	ra_clear(&surf->to_redraw);
	
	/*terminate surface*/
	VS2D_TerminateSurface(surf);
	return has_changed;
}

DrawableContext *VS2D_PickSensitiveNode(VisualSurface2D *surf, Fixed x, Fixed y)
{
	RenderEffect2D eff;
	u32 i;
	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_PATH;
	eff.x = x;
	eff.y = y;
	i = surf->num_contexts;
	for (; i > 0; i--) {
		DrawableContext *ctx = surf->contexts[i-1];
		/*check over bounds*/
		if (!ctx->node || ! gf_point_in_rect(&ctx->bi->clip, x, y)) continue;
		eff.ctx = ctx;
		eff.is_over = 0;
		gf_node_render(ctx->node->owner, &eff);
		if (!eff.is_over) continue;
		/*check if has sensors */
		if (ctx->sensor) return ctx;

		/*check for composite texture*/
		if (ctx->h_texture && (ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE) ) {
			return CT2D_FindNode(ctx->h_texture, ctx, x, y);
		}
		return NULL;
    }
	return NULL;
}

DrawableContext *VS2D_PickContext(VisualSurface2D *surf, Fixed x, Fixed y)
{
	RenderEffect2D eff;
	u32 i;
	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_FULL;
	eff.x = x;
	eff.y = y;
	i = surf->num_contexts;
	for (; i > 0; i--) {
		DrawableContext *ctx = surf->contexts[i-1];
		/*check over bounds*/
		if (!ctx->node || ! gf_point_in_rect(&ctx->bi->clip, x, y)) continue;
		eff.ctx = ctx;
		eff.is_over = 0;
		gf_node_render(ctx->node->owner, &eff);
		if (eff.is_over) return ctx;
    }
	return NULL;
}

/* this is to use carefully: picks a node based on the PREVIOUS frame state (no traversing)*/
GF_Node *VS2D_PickNode(VisualSurface2D *surf, Fixed x, Fixed y)
{
	u32 i;
	RenderEffect2D eff;
	GF_Node *back;
	M_Background2D *bck;
	bck = NULL;
	back = NULL;
	bck = (M_Background2D *) gf_list_get(surf->back_stack, 0);
	if (bck && bck->isBound) back = (GF_Node *) bck;

	i = surf->num_contexts;
	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_PATH_AND_OUTLINE;
	eff.x = x;
	eff.y = y;
	for (; i > 0; i--) {
		DrawableContext *ctx = surf->contexts[i-1];
		/*check over bounds*/
		if (!ctx->node || ! gf_point_in_rect(&ctx->bi->clip, x, y)) continue;
		/*check over shape*/
		eff.is_over = 0;
		gf_node_render(ctx->node->owner, &eff);
		if (!eff.is_over) continue;

		/*check for composite texture*/
		if (!ctx->h_texture && !ctx->aspect.line_texture) return ctx->node->owner;

		if (ctx->h_texture && (ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE)) {
			return CT2D_PickNode(ctx->h_texture, ctx, x, y);
		}
		else if (ctx->aspect.line_texture && (gf_node_get_tag(ctx->aspect.line_texture->owner)==TAG_MPEG4_CompositeTexture2D)) {
			return CT2D_PickNode(ctx->aspect.line_texture, ctx, x, y);
		}
		return ctx->node->owner;
    }
	return back;
}


