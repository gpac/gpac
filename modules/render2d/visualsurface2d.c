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
	return tmp;
}

void DeleteVisualSurface2D(VisualSurface2D *surf)
{
	ra_del(&surf->to_redraw);
	VS2D_ResetGraphics(surf);

	while (surf->context) {
		DrawableContext *ctx = surf->context;
		surf->context = ctx->next;
		DeleteDrawableContext(ctx);
	}
	while (surf->prev_nodes) {
		struct _drawable_store *cur = surf->prev_nodes;
		surf->prev_nodes = cur->next;
		free(cur);
	}

	gf_list_del(surf->back_stack);
	gf_list_del(surf->view_stack);
	free(surf);
}

DrawableContext *VS2D_GetDrawableContext(VisualSurface2D *surf)
{
	if (!surf->context) {
		surf->context = NewDrawableContext();
		surf->cur_context = surf->context;
		drawctx_reset(surf->context);
		return surf->context;
	}
	assert(surf->cur_context);
	/*current context is OK*/
	if (!surf->cur_context->drawable) {
		/*reset next context in display list for next call*/
		if (surf->cur_context->next) surf->cur_context->next->drawable = NULL;
		drawctx_reset(surf->cur_context);
		return surf->cur_context;
	}
	/*need a new context and next one is OK*/
	if (surf->cur_context->next) {
		surf->cur_context = surf->cur_context->next;
		assert(surf->cur_context->drawable == NULL);
		/*reset next context in display list for next call*/
		if (surf->cur_context->next) surf->cur_context->next->drawable = NULL;
		drawctx_reset(surf->cur_context);
		return surf->cur_context;
	}
	/*need to create a new context*/
	surf->cur_context->next = NewDrawableContext();
	surf->cur_context = surf->cur_context->next;
	drawctx_reset(surf->cur_context);
	return surf->cur_context;
}

void VS2D_RemoveLastContext(VisualSurface2D *surf)
{
	assert(surf->cur_context);
	surf->cur_context->drawable = NULL;
}


void VS2D_DrawableDeleted(struct _visual_surface_2D *surf, struct _drawable *node)
{
	if (node->flags & DRAWABLE_REG_WITH_SURFACE) {
		struct _drawable_store *it = surf->prev_nodes;
		struct _drawable_store *prev = NULL;
		while (it) {
			if (it->drawable != node) {
				prev = it;
				it = prev->next;
				continue;
			}
			if (prev) prev->next = it->next;
			else surf->prev_nodes = it->next;
			if (!it->next) surf->last_prev_entry = prev;
			free(it);
			break;
		}
	}
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
	u32 rem, count;
	GF_Rect rc;
	struct _drawable_store *it, *prev;
	DrawableContext *ctx;
	M_Background2D *bck;
	u32 render_mode;
	u32 time;
	
	/*reset display list*/
	surf->cur_context = surf->context;
	if (surf->context) surf->context->drawable = NULL;

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
	if (e) 
		return e;

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

	/*reset sensors*/
	surf->has_sensors = 0;

	render_mode = 0;
	if (eff->trav_flags & TF_RENDER_DIRECT) render_mode = 1;
	/*if we're requested to invalidate everything, switch to direct render but don't reset bounds*/
	else if (eff->invalidate_all) {
		eff->trav_flags |= TF_RENDER_DIRECT;
		render_mode = 2;
	}

	time = gf_sys_clock();
	/*reset prev nodes if any (previous render was indirect)*/
	rem = count = 0;
	prev = NULL;
	it = surf->prev_nodes;
	while (it) {
		assert(it->drawable->flags & DRAWABLE_REG_WITH_SURFACE);

		/*node was not drawn on this surface*/
		if (!drawable_flush_bounds(it->drawable, surf, render_mode)) {
			//GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Unregistering previously drawn node %s from surface\n", gf_node_get_class_name(it->drawable->node)));
			/*direct rendering mode, remove all bounds info and unreg node */
			drawable_reset_bounds(it->drawable, surf);
			it->drawable->flags &= ~DRAWABLE_REG_WITH_SURFACE;
			if (prev) prev->next = it->next;
			else surf->prev_nodes = it->next;
			if (!it->next) surf->last_prev_entry = prev;
			rem++;
			free(it);
			it = prev ? prev->next : surf->prev_nodes;
		} else {
			prev = it;
			it = it->next;
			count++;
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Top surface initialized - %d nodes registered and %d removed - using %s rendering\n", count, rem, render_mode ? "direct" : "dirty-rect"));
	if (!render_mode) return GF_OK;

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

static void VS2D_ReverseContexts(VisualSurface2D *surf)
{
	u32 i, depth;
	DrawableContext *ctx, *prev, *tmp;

	i = depth = 0;
	/*reverse all contexts used to front->back order for picking*/
	prev = NULL;
	ctx = surf->context;

	if (surf->render->grab_ctx) {
		while (ctx && ctx->drawable) {
			tmp = ctx->next;
			ctx->next = prev;
			prev = ctx;
			i++;
			if (surf->render->grab_ctx == ctx) depth = i;
			ctx = tmp;
		}
	} else {
		while (ctx && ctx->drawable) {
			tmp = ctx->next;
			ctx->next = prev;
			prev = ctx;
			ctx = tmp;
		}
	} 
	if (prev) {
		surf->context->next = ctx;
		surf->context = surf->cur_context = prev;

		if (surf->render->grab_ctx) {
			surf->render->grab_ctx = surf->context;
			while (depth>1) {
				surf->render->grab_ctx = surf->render->grab_ctx->next;
				depth--;
			}
		}
	}
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
	u32 k, i, count, max_nodes_allowed, num_nodes, num_changed;
	GF_IRect refreshRect, *check_rect;
	Bool redraw_all;
	M_Background2D *bck;
	DrawableContext *bck_ctx, *ctx;
	struct _drawable_store *it, *prev;
#ifndef TRACK_OPAQUE_REGIONS
	DrawableContext *first_opaque = NULL;
#endif
	Bool has_changed = 0;

	/*in direct mode the surface is always redrawn*/
	if (eff->trav_flags & TF_RENDER_DIRECT) {
		VS2D_TerminateSurface(surf);
		VS2D_ReverseContexts(surf);
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
	num_nodes = 0;
	ctx = surf->context;
	while (ctx && ctx->drawable) {
		num_nodes++;

		drawctx_update_info(ctx, surf);
		if (ctx->flags & CTX_REDRAW_MASK) {
			num_changed ++;

#ifndef TRACK_OPAQUE_REGIONS
			if (!first_opaque && !(ctx->flags & CTX_IS_TRANSPARENT) && (ctx->flags & CTX_NO_ANTIALIAS)) 
				first_opaque = ctx;
#endif
			/*node has changed, add to redraw area*/
			if (!redraw_all) {
				ra_union_rect(&surf->to_redraw, &ctx->bi->clip);
//				CHECK_MAX_NODE
			}
		}
		ctx = ctx->next;
	}

	/*garbage collection*/

	/*clear all remaining bounds since last frames (the ones that moved or that are not drawn this frame)*/
	prev = NULL;
	it = surf->prev_nodes;
	while (it) {
		while (drawable_get_previous_bound(it->drawable, &refreshRect, surf)) {
			if (!redraw_all) {
				ra_union_rect(&surf->to_redraw, &refreshRect);
//				CHECK_MAX_NODE
			}
		}
		/*if node is marked as undrawn, remove from surface*/
		if (!(it->drawable->flags & DRAWABLE_DRAWN_ON_SURFACE)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Node %s no longer on surface - unregistering it\n", gf_node_get_class_name(it->drawable->node)));
			it->drawable->flags &= ~DRAWABLE_REG_WITH_SURFACE;

			if (prev) prev->next = it->next;
			else surf->prev_nodes = it->next;
			if (!it->next) surf->last_prev_entry = prev;
			free(it);
			it = prev ? prev->next : surf->prev_nodes;
		} else {
			prev = it;
			it = it->next;
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
	if (ra_is_empty(&surf->to_redraw) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] No changes found since last frame - skipping redraw\n"));
		goto exit;
	}
	has_changed = 1;
	eff->traversing_mode = TRAVERSE_DRAW;

#ifndef TRACK_OPAQUE_REGIONS
	if (first_opaque && (surf->to_redraw.count==1) && gf_rect_equal(first_opaque->bi->clip, surf->to_redraw.list[0]))
		goto skip_background;
#endif

	/*redraw everything*/
	if (bck_ctx) {
		drawable_check_bounds(bck_ctx, surf);
		eff->ctx = bck_ctx;
#ifdef TRACK_OPAQUE_REGIONS
		surf->draw_node_index = 0;
#endif
		bck_ctx->bi->unclip = gf_rect_ft(&surf->surf_rect);
		bck_ctx->bi->clip = surf->surf_rect;
		gf_node_render(bck_ctx->drawable->node, eff);
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

#ifndef TRACK_OPAQUE_REGIONS
skip_background:
#endif

#ifndef GPAC_DISABLE_LOG
	if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RENDER)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Redraw %d / %d nodes (all: %s)", num_changed, num_nodes, redraw_all ? "yes" : "no"));
		if (surf->to_redraw.count>1) GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\n"));

		for (i=0; i<surf->to_redraw.count; i++)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\tDirtyRect #%d: %d:%d@%dx%d\n", i+1, surf->to_redraw.list[i].x, surf->to_redraw.list[i].y, surf->to_redraw.list[i].width, surf->to_redraw.list[i].height));
	}
#endif
	
#ifdef TRACK_OPAQUE_REGIONS
	i=0;
#endif
	check_rect = (surf->to_redraw.count>1) ? NULL : &surf->to_redraw.list[0];
	ctx = surf->context;
	while (ctx && ctx->drawable) {
		if (!check_rect || gf_irect_overlaps(&ctx->bi->clip, check_rect)) {
#ifdef TRACK_OPAQUE_REGIONS
			i++;
			surf->draw_node_index = i;
#endif
			eff->ctx = ctx;
			gf_node_render(ctx->drawable->node, eff);
		}
		/*node no longer register with this surface, destroy its bounds*/
		if (!(ctx->drawable->flags & DRAWABLE_REG_WITH_SURFACE)) {
			drawable_reset_bounds(ctx->drawable, surf);
		}

		ctx = ctx->next;
	}

exit:
	/*clear dirty rects*/
	ra_clear(&surf->to_redraw);
	
	/*terminate surface*/
	VS2D_TerminateSurface(surf);
	VS2D_ReverseContexts(surf);
	return has_changed;
}

DrawableContext *VS2D_PickSensitiveNode(VisualSurface2D *surf, Fixed x, Fixed y)
{
	RenderEffect2D eff;
	DrawableContext *ctx;

	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_PATH;
	eff.x = x;
	eff.y = y;
	
	ctx = surf->context;
	while (ctx && ctx->drawable) {
		/*check over bounds*/
		if (!ctx->drawable || ! gf_point_in_rect(&ctx->bi->clip, x, y)) {
			ctx = ctx->next;
			continue;
		}
		eff.ctx = ctx;
		eff.is_over = 0;
		gf_node_render(ctx->drawable->node, &eff);
		if (!eff.is_over) {
			ctx = ctx->next;
			continue;
		}
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
	DrawableContext *ctx;

	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_FULL;
	eff.x = x;
	eff.y = y;

	ctx = surf->context;
	while (ctx && ctx->drawable) {
		/*check over bounds*/
		if (ctx->drawable && gf_point_in_rect(&ctx->bi->clip, x, y)) {
			eff.ctx = ctx;
			eff.is_over = 0;
			gf_node_render(ctx->drawable->node, &eff);
			if (eff.is_over) return ctx;
		}
		ctx = ctx->next;
    }
	return NULL;
}

/* this is to use carefully: picks a node based on the PREVIOUS frame state (no traversing)*/
GF_Node *VS2D_PickNode(VisualSurface2D *surf, Fixed x, Fixed y)
{
	RenderEffect2D eff;
	GF_Node *back;
	M_Background2D *bck;
	DrawableContext *ctx;

	bck = NULL;
	back = NULL;
	bck = (M_Background2D *) gf_list_get(surf->back_stack, 0);
	if (bck && bck->isBound) back = (GF_Node *) bck;

	eff.surface = surf;
	eff.traversing_mode = TRAVERSE_PICK;
	eff.pick_type = PICK_PATH_AND_OUTLINE;
	eff.x = x;
	eff.y = y;
	ctx = surf->context;
	while (ctx && ctx->drawable) {
		/*check over bounds*/
		if (!ctx->drawable || ! gf_point_in_rect(&ctx->bi->clip, x, y)) {
			ctx = ctx->next;
			continue;
		}
		/*check over shape*/
		eff.is_over = 0;
		gf_node_render(ctx->drawable->node, &eff);
		if (!eff.is_over) {
			ctx = ctx->next;
			continue;
		}

		/*check for composite texture*/
		if (!ctx->h_texture && !ctx->aspect.line_texture) return ctx->drawable->node;

		if (ctx->h_texture && (ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE)) {
			return CT2D_PickNode(ctx->h_texture, ctx, x, y);
		}
		else if (ctx->aspect.line_texture && (gf_node_get_tag(ctx->aspect.line_texture->owner)==TAG_MPEG4_CompositeTexture2D)) {
			return CT2D_PickNode(ctx->aspect.line_texture, ctx, x, y);
		}
		return ctx->drawable->node;
    }
	return back;
}


