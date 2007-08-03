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
#include "node_stacks.h"


Bool gf_irect_overlaps(GF_IRect *rc1, GF_IRect *rc2)
{
	if (! rc2->height || !rc2->width || !rc1->height || !rc1->width) return 0;
	if (rc2->x+rc2->width<=rc1->x) return 0;
	if (rc2->x>=rc1->x+rc1->width) return 0;
	if (rc2->y-rc2->height>=rc1->y) return 0;
	if (rc2->y<=rc1->y-rc1->height) return 0;
	return 1;
}

/*intersects @rc1 with @rc2 - the new @rc1 is the intersection*/
void gf_irect_intersect(GF_IRect *rc1, GF_IRect *rc2)
{
	if (! gf_irect_overlaps(rc1, rc2)) {
		rc1->width = rc1->height = 0; 
		return;
	}
	if (rc2->x > rc1->x) {
		rc1->width -= rc2->x - rc1->x;
		rc1->x = rc2->x;
	} 
	if (rc2->x + rc2->width < rc1->x + rc1->width) {
		rc1->width = rc2->width + rc2->x - rc1->x;
	} 
	if (rc2->y < rc1->y) {
		rc1->height -= rc1->y - rc2->y; 
		rc1->y = rc2->y;
	} 
	if (rc2->y - rc2->height > rc1->y - rc1->height) {
		rc1->height = rc1->y - rc2->y + rc2->height;
	} 
}


GF_Rect gf_rect_ft(GF_IRect *rc)
{
	GF_Rect rcft;
	rcft.x = INT2FIX(rc->x); rcft.y = INT2FIX(rc->y); rcft.width = INT2FIX(rc->width); rcft.height = INT2FIX(rc->height); 
	return rcft;
}

DrawableContext *visual_2d_get_drawable_context(GF_VisualManager *vis)
{
	if (!vis->context) {
		vis->context = NewDrawableContext();
		vis->cur_context = vis->context;
		drawctx_reset(vis->context);
		return vis->context;
	}
	assert(vis->cur_context);
	/*current context is OK*/
	if (!vis->cur_context->drawable) {
		/*reset next context in display list for next call*/
		if (vis->cur_context->next) vis->cur_context->next->drawable = NULL;
		drawctx_reset(vis->cur_context);
		return vis->cur_context;
	}
	/*need a new context and next one is OK*/
	if (vis->cur_context->next) {
		vis->cur_context = vis->cur_context->next;
		assert(vis->cur_context->drawable == NULL);
		/*reset next context in display list for next call*/
		if (vis->cur_context->next) vis->cur_context->next->drawable = NULL;
		drawctx_reset(vis->cur_context);
		return vis->cur_context;
	}
	/*need to create a new context*/
	vis->cur_context->next = NewDrawableContext();
	vis->cur_context = vis->cur_context->next;
	drawctx_reset(vis->cur_context);
	return vis->cur_context;
}

void visual_2d_remove_last_context(GF_VisualManager *vis)
{
	assert(vis->cur_context);
	vis->cur_context->drawable = NULL;
}


void visual_2d_drawable_delete(GF_VisualManager *vis, struct _drawable *node)
{
	/*remove drawable from visual list*/
	struct _drawable_store *it = vis->prev_nodes;
	struct _drawable_store *prev = NULL;
	while (it) {
		if (it->drawable != node) {
			prev = it;
			it = prev->next;
			continue;
		}
		if (prev) prev->next = it->next;
		else vis->prev_nodes = it->next;
		if (!it->next) vis->last_prev_entry = prev;
		free(it);
		break;
	}

	/*check node isn't being tracked*/
	if (vis->render->grab_node==node->node) 
		vis->render->grab_node = NULL;

	if (vis->render->focus_node==node->node) 
		vis->render->focus_node = NULL;
}

Bool visual_2d_node_cull(GF_TraverseState *tr_state, GF_Rect *bounds)
{
	GF_Rect rc;
	GF_IRect i_rc;
	rc = *bounds;
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	i_rc = gf_rect_pixelize(&rc);
	if (gf_irect_overlaps(&tr_state->visual->top_clipper, &i_rc)) return 1;
	return 0;
}

void visual_2d_setup_projection(GF_VisualManager *vis, GF_TraverseState *tr_state)
{
	GF_Rect rc;

	tr_state->visual = vis;
	tr_state->backgrounds = vis->back_stack;
	tr_state->viewpoints = vis->view_stack;

	/*setup clipper*/
	if (vis->center_coords) {
		if (!vis->offscreen) {
			if (vis->render->scalable_zoom)
				rc = gf_rect_center(INT2FIX(vis->render->compositor->width), INT2FIX(vis->render->compositor->height));
			else
				rc = gf_rect_center(INT2FIX(vis->render->cur_width + 2*vis->render->vp_x), INT2FIX(vis->render->cur_height + 2*vis->render->vp_y));
		} else {
			rc = gf_rect_center(INT2FIX(vis->width), INT2FIX(vis->height));
		}
	} else {
		rc.x = 0;
		rc.width = INT2FIX(vis->width);
		rc.y = rc.height = INT2FIX(vis->height);
	}
	/*set top-transform to pixelMetrics*/
	if (!tr_state->is_pixel_metrics) gf_mx2d_add_scale(&tr_state->transform, tr_state->min_hsize, tr_state->min_hsize);

	vis->surf_rect = gf_rect_pixelize(&rc);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Visual rectangle setup - width %d height %d\n", vis->surf_rect.width, vis->surf_rect.height));

	/*setup top clipper*/
	if (vis->center_coords) {
		rc = gf_rect_center(INT2FIX(vis->width), INT2FIX(vis->height));
	} else {
		rc.width = INT2FIX(vis->width);
		rc.height = INT2FIX(vis->height);
		rc.x = 0;
		rc.y = rc.height;
		if (vis->render->visual==vis) {
			rc.x += INT2FIX(vis->render->vp_x);
			rc.y += INT2FIX(vis->render->vp_y);
		}
	}

	/*setup viewport*/
	if (gf_list_count(vis->view_stack)) {
		tr_state->traversing_mode = TRAVERSE_RENDER_BINDABLE;
		tr_state->bounds = rc;
		gf_node_render((GF_Node *) gf_list_get(vis->view_stack, 0), tr_state);
	}

	vis->top_clipper = gf_rect_pixelize(&rc);
	tr_state->clipper = rc;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Visual cliper setup - %d:%d@%dx%d\n", vis->top_clipper.x, vis->top_clipper.y, vis->top_clipper.width, vis->top_clipper.height));
}

GF_Err visual_2d_init_draw(GF_VisualManager *vis, GF_TraverseState *tr_state)
{
	GF_Err e;
	u32 rem, count;
	struct _drawable_store *it, *prev;
	DrawableContext *ctx;
	M_Background2D *bck;
	u32 render_mode;
	u32 time;
	
	/*reset display list*/
	vis->cur_context = vis->context;
	if (vis->context) vis->context->drawable = NULL;

	visual_2d_setup_projection(vis, tr_state);

	tr_state->traversing_mode = TRAVERSE_RENDER;

	/*setup raster surface, brush and pen */
	e = visual_2d_init_raster(vis);
	if (e) 
		return e;

	render_mode = 0;
	if (tr_state->trav_flags & TF_RENDER_DIRECT) render_mode = 1;
	/*if we're requested to invalidate everything, switch to direct render but don't reset bounds*/
	else if (tr_state->invalidate_all) {
		tr_state->trav_flags |= TF_RENDER_DIRECT;
		render_mode = 2;
	}

	time = gf_sys_clock();
	/*reset prev nodes if any (previous render was indirect)*/
	rem = count = 0;
	prev = NULL;
	it = vis->prev_nodes;
	while (it) {
		/*node was not drawn on this visual*/
		if (!drawable_flush_bounds(it->drawable, vis, render_mode)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Unregistering previously drawn node %s from visual\n", gf_node_get_class_name(it->drawable->node)));
			
			/*remove all bounds info related to this visual and unreg node */
			drawable_reset_bounds(it->drawable, vis);

			if (prev) prev->next = it->next;
			else vis->prev_nodes = it->next;
			if (!it->next) vis->last_prev_entry = prev;
			rem++;
			free(it);
			it = prev ? prev->next : vis->prev_nodes;
		} else {
			/*mark drawable as already registered with visual*/
			it->drawable->flags |= DRAWABLE_REGISTERED_WITH_VISUAL;
			prev = it;
			it = it->next;
			count++;
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Top visual initialized - %d nodes registered and %d removed - using %s rendering\n", count, rem, render_mode ? "direct" : "dirty-rect"));
	if (!render_mode) return GF_OK;

	/*direct mode, draw background*/
	bck = (M_Background2D*) gf_list_get(vis->back_stack, 0);
	if (bck && bck->isBound) {
		ctx = b2d_get_context(bck, vis->back_stack);
		if (ctx) {
			ctx->bi->clip = vis->surf_rect;
			ctx->bi->unclip = gf_rect_ft(&vis->surf_rect);
			tr_state->traversing_mode = TRAVERSE_RENDER_BINDABLE;
			gf_node_render((GF_Node *) bck, tr_state);
			tr_state->traversing_mode = TRAVERSE_RENDER;
		} else {
			visual_2d_clear(vis, NULL, 0);
		}
	} else {
		visual_2d_clear(vis, NULL, 0);
	}
	return GF_OK;
}



#ifdef TRACK_OPAQUE_REGIONS

/*@rc2 fully contained in @rc1*/
static GFINLINE Bool gf_irect_inside(GF_IRect *rc1, GF_IRect *rc2) 
{
	if (!rc1->width || !rc1->height) return 0;
	if ( (rc1->x <= rc2->x)  && (rc1->y >= rc2->y)  && (rc1->x + rc1->width >= rc2->x + rc2->width) && (rc1->y - rc1->height <= rc2->y - rc2->height) )
		return 1;
	return 0;
}

#define CHECK_UNCHANGED		0

static void mark_opaque_areas(GF_VisualManager *vis)
{
	u32 i, k;
#if CHECK_UNCHANGED
	Bool remove;
#endif
	GF_RectArray *ra = &vis->to_redraw;
	if (!ra->count) return;
	ra->opaque_node_index = (u32*)realloc(ra->opaque_node_index, sizeof(u32) * ra->count);

	for (k=0; k<ra->count; k++) {
#if CHECK_UNCHANGED
		remove = 1;
#endif
		ra->opaque_node_index[k] = 0;

		for (i=vis->num_contexts; i>0; i--) {
			DrawableContext *ctx = vis->contexts[i-1];

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

static void clean_contexts(GF_VisualManager *vis)
{
	u32 i, count;
	Bool is_root_visual = (vis->render->visual==vis) ? 1 : 0;
	DrawableContext *ctx = vis->context;
	while (ctx && ctx->drawable) {
		/*remove visual registration flag*/
		ctx->drawable->flags &= ~DRAWABLE_REGISTERED_WITH_VISUAL;
		if (is_root_visual && (ctx->flags & CTX_HAS_APPEARANCE)) 
			gf_node_dirty_reset(ctx->appear);
		ctx = ctx->next;
	}

	/*composite visual, cannot reset flags until root is done*/
	if (!is_root_visual) return;

	/*reset all flags of all appearance nodes registered on all visuals but main one (done above)
	this must be done once all visuals have been drawn, otherwise we won't detect the changes 
	for nodes drawn on several visuals*/
	count = gf_list_count(vis->render->visuals);
	for (i=1; i<count; i++) {
		GF_VisualManager *a_vis = gf_list_get(vis->render->visuals, i);
		ctx = a_vis->context;
		while (ctx && ctx->drawable) {
			if (ctx->flags & CTX_HAS_APPEARANCE) gf_node_dirty_reset(ctx->appear);
			ctx = ctx->next;
		}
	}
}


/*clears list*/
#define ra_clear(ra) { (ra)->count = 0; }

/*is list empty*/
#define ra_is_empty(ra) (!((ra)->count))

/*adds @rc2 to @rc1 - the new @rc1 contains the old @rc1 and @rc2*/
static GFINLINE void gf_irect_union(GF_IRect *rc1, GF_IRect *rc2) 
{
	if (!rc1->width || !rc1->height) {*rc1=*rc2; return;}

	if (rc2->x < rc1->x) {
		rc1->width += rc1->x - rc2->x;
		rc1->x = rc2->x;
	}
	if (rc2->x + rc2->width > rc1->x+rc1->width) rc1->width = rc2->x + rc2->width - rc1->x;
	if (rc2->y > rc1->y) {
		rc1->height += rc2->y - rc1->y;
		rc1->y = rc2->y;
	}
	if (rc2->y - rc2->height < rc1->y - rc1->height) rc1->height = rc1->y - rc2->y + rc2->height;
}

/*adds rectangle to the list performing union test*/
static GFINLINE void ra_union_rect(GF_RectArray *ra, GF_IRect *rc) 
{
	u32 i;

	for (i=0; i<ra->count; i++) { 
		if (gf_irect_overlaps(&ra->list[i], rc)) { 
			gf_irect_union(&ra->list[i], rc); 
			return; 
		} 
	}
	ra_add(ra, rc); 
}

/*refreshes the content of the array to have only non-overlapping rects*/
static void ra_refresh(GF_RectArray *ra)
{
	u32 i, j, k;
	for (i=0; i<ra->count; i++) {
		for (j=i+1; j<ra->count; j++) {
			if (gf_irect_overlaps(&ra->list[i], &ra->list[j])) {
				gf_irect_union(&ra->list[i], &ra->list[j]);

				/*remove rect*/
				k = ra->count - j - 1;
				if (k) memmove(&ra->list[j], & ra->list[j+1], sizeof(GF_IRect)*k);
				ra->count--; 

				ra_refresh(ra);
				return;
			}
		}
	}
}

/*this defines a partition of the rendering area in small squares of the given width. If the number of nodes
to redraw exceeds the possible number of squares on the visual, the entire area is redrawn - this avoids computing
dirty rects algo when a lot of small shapes are moving*/
#define MIN_BBOX_SIZE	16

#if 0
#define CHECK_MAX_NODE	if (vis->to_redraw.count > max_nodes_allowed) redraw_all = 1;
#else
#define CHECK_MAX_NODE
#endif


Bool visual_2d_terminate_draw(GF_VisualManager *vis, GF_TraverseState *tr_state)
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

	/*in direct mode the visual is always redrawn*/
	if (tr_state->trav_flags & TF_RENDER_DIRECT) {
		visual_2d_release_raster(vis);
		clean_contexts(vis);
		return 1;
	}

	num_changed = 0;
	
	/*if the aspect ratio has changed redraw everything*/
	redraw_all = tr_state->invalidate_all;

	/*check for background changes for transparent nodes*/
	bck = NULL;
	bck_ctx = NULL;

	bck = (M_Background2D*)gf_list_get(vis->back_stack, 0);
	if (bck) {
		if (!bck->isBound) {
			if (vis->last_had_back) redraw_all = 1;
			vis->last_had_back = 0;
		} else {
			if (!vis->last_had_back) redraw_all = 1;
			vis->last_had_back = 1;
			bck_ctx = b2d_get_context(bck, vis->back_stack);
			if (bck_ctx->flags & CTX_REDRAW_MASK) redraw_all = 1;
		}
	} else if (vis->last_had_back) {
		vis->last_had_back = 0;
		redraw_all = 1;
	}
	
	max_nodes_allowed = (u32) ((vis->top_clipper.width / MIN_BBOX_SIZE) * (vis->top_clipper.height / MIN_BBOX_SIZE));
	num_nodes = 0;
	ctx = vis->context;
	while (ctx && ctx->drawable) {
		num_nodes++;

		drawctx_update_info(ctx, vis);
		if (ctx->flags & CTX_REDRAW_MASK) {
			num_changed ++;

#ifndef TRACK_OPAQUE_REGIONS
			if (!first_opaque && !(ctx->flags & CTX_IS_TRANSPARENT) && (ctx->flags & CTX_NO_ANTIALIAS)) 
				first_opaque = ctx;
#endif
			/*node has changed, add to redraw area*/
			if (!redraw_all) {
				ra_union_rect(&vis->to_redraw, &ctx->bi->clip);
//				CHECK_MAX_NODE
			}
		}
		ctx = ctx->next;
	}

	/*garbage collection*/

	/*clear all remaining bounds since last frames (the ones that moved or that are not drawn this frame)*/
	prev = NULL;
	it = vis->prev_nodes;
	while (it) {
		while (drawable_get_previous_bound(it->drawable, &refreshRect, vis)) {
			if (!redraw_all) {
				ra_union_rect(&vis->to_redraw, &refreshRect);
//				CHECK_MAX_NODE
			}
		}
		/*if node is marked as undrawn, remove from visual*/
		if (!(it->drawable->flags & DRAWABLE_DRAWN_ON_VISUAL)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Node %s no longer on visual - unregistering it\n", gf_node_get_class_name(it->drawable->node)));

			/*remove all bounds info related to this visual and unreg node */
			drawable_reset_bounds(it->drawable, vis);

			it->drawable->flags &= ~DRAWABLE_REGISTERED_WITH_VISUAL;


			if (prev) prev->next = it->next;
			else vis->prev_nodes = it->next;
			if (!it->next) vis->last_prev_entry = prev;
			free(it);
			it = prev ? prev->next : vis->prev_nodes;
		} else {
			prev = it;
			it = it->next;
		}
	}
	/*no visual rendering*/
	//if (!vis->render->cur_width && !vis->render->cur_height) goto exit;

//	CHECK_MAX_NODE

	if (redraw_all) {
		ra_clear(&vis->to_redraw);
		ra_add(&vis->to_redraw, &vis->surf_rect);
	} else {
		ra_refresh(&vis->to_redraw);
	}
#ifdef TRACK_OPAQUE_REGIONS
	/*mark opaque areas to speed up*/
	mark_opaque_areas(vis);
#endif

	/*nothing to redraw*/
	if (ra_is_empty(&vis->to_redraw) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] No changes found since last frame - skipping redraw\n"));
		goto exit;
	}
	has_changed = 1;
	tr_state->traversing_mode = TRAVERSE_DRAW_2D;

#ifndef TRACK_OPAQUE_REGIONS
	if (first_opaque && (vis->to_redraw.count==1) && gf_rect_equal(first_opaque->bi->clip, vis->to_redraw.list[0]))
		goto skip_background;
#endif

	/*redraw everything*/
	if (bck_ctx) {
		drawable_check_bounds(bck_ctx, vis);
		tr_state->ctx = bck_ctx;
#ifdef TRACK_OPAQUE_REGIONS
		vis->draw_node_index = 0;
#endif
		bck_ctx->bi->unclip = gf_rect_ft(&vis->surf_rect);
		bck_ctx->bi->clip = vis->surf_rect;
		gf_node_render(bck_ctx->drawable->node, tr_state);
	} else {
		count = vis->to_redraw.count;
		if (bck_ctx) bck_ctx->bi->unclip = gf_rect_ft(&vis->surf_rect);
		for (k=0; k<count; k++) {
			GF_IRect rc;
			/*opaque area, skip*/
#ifdef TRACK_OPAQUE_REGIONS
			if (vis->to_redraw.opaque_node_index[k] > 0) continue;
#endif
			rc = vis->to_redraw.list[k];
			gf_irect_intersect(&rc, &vis->top_clipper);
			visual_2d_clear(vis, &rc, 0);
		}
	}

#ifndef TRACK_OPAQUE_REGIONS
skip_background:
#endif

#ifndef GPAC_DISABLE_LOG
	if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RENDER)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Redraw %d / %d nodes (all: %s)", num_changed, num_nodes, redraw_all ? "yes" : "no"));
		if (vis->to_redraw.count>1) GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\n"));

		for (i=0; i<vis->to_redraw.count; i++)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("\tDirtyRect #%d: %d:%d@%dx%d\n", i+1, vis->to_redraw.list[i].x, vis->to_redraw.list[i].y, vis->to_redraw.list[i].width, vis->to_redraw.list[i].height));
	}
#endif
	
#ifdef TRACK_OPAQUE_REGIONS
	i=0;
#endif
	check_rect = (vis->to_redraw.count>1) ? NULL : &vis->to_redraw.list[0];
	ctx = vis->context;
	while (ctx && ctx->drawable) {
		if (!check_rect || gf_irect_overlaps(&ctx->bi->clip, check_rect)) {
#ifdef TRACK_OPAQUE_REGIONS
			i++;
			vis->draw_node_index = i;
#endif
			tr_state->ctx = ctx;

			if (ctx->drawable->flags & DRAWABLE_USE_TRAVERSE_DRAW) {
				gf_node_render(ctx->drawable->node, tr_state);
			} else {
				drawable_draw(ctx->drawable, tr_state);
			}
		}
		ctx = ctx->next;
	}

exit:
	/*clear dirty rects*/
	ra_clear(&vis->to_redraw);
	visual_2d_release_raster(vis);
	clean_contexts(vis);
	return has_changed;
}

Bool visual_2d_render_frame(GF_VisualManager *vis, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
	GF_TraverseState static_state;

	GF_SceneGraph *sg;
	GF_Matrix2D backup;
	u32 i;
	GF_Err e;

	vis->bounds_tracker_modif_flag = DRAWABLE_HAS_CHANGED;

	memcpy(&static_state, tr_state, sizeof(GF_TraverseState));
	gf_mx2d_copy(backup, tr_state->transform);

	e = visual_2d_init_draw(vis, tr_state);
	if (e) {
		gf_mx2d_copy(tr_state->transform, backup);
		return 0;
	}

	if (is_root_visual) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Traversing scene tree (root node %s)\n", root ? gf_node_get_class_name(root) : "none"));
#ifndef GPAC_DISABLE_SVG
		i = gf_sys_clock();
#endif
		gf_node_render(root, tr_state);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Traversing scene done in %d ms\n", gf_sys_clock() - i));

		i=0;
		while ((sg = (GF_SceneGraph*)gf_list_enum(vis->render->compositor->extra_scenes, &i))) {
			GF_Node *n = gf_sg_get_root_node(sg);
			if (n) gf_node_render(n, tr_state);
		}
	} else {
		gf_node_render(root, tr_state);
	}
	gf_mx2d_copy(tr_state->transform, backup);
	e = visual_2d_terminate_draw(vis, tr_state);
	memcpy(tr_state, &static_state, sizeof(GF_TraverseState));
	return e;
}


void visual_2d_pick_node(GF_VisualManager *vis, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	GF_Matrix2D backup;
	vis->bounds_tracker_modif_flag = DRAWABLE_HAS_CHANGED_IN_LAST_TRAVERSE;

	gf_mx2d_copy(backup, tr_state->transform);

	visual_2d_setup_projection(vis, tr_state);

	vis->render->hit_info.picked = NULL;
	tr_state->ray.orig.x = INT2FIX(ev->mouse.x);
	tr_state->ray.orig.y = INT2FIX(ev->mouse.y);
	tr_state->ray.orig.z = 0;
	tr_state->ray.dir.x = 0;
	tr_state->ray.dir.y = 0;
	tr_state->ray.dir.z = -FIX_ONE;
	
	vis->render->hit_info.world_point = tr_state->ray.orig;
	vis->render->hit_info.world_ray = tr_state->ray;

	gf_list_reset(vis->render->sensors);
	tr_state->traversing_mode = TRAVERSE_PICK;

	/*not the root scene, use children list*/
	if (vis->render->visual != vis) {
		while (children) {
			gf_node_render(children->node, tr_state);
			children = children->next;
		}
	} else {
		gf_node_render(gf_sg_get_root_node(vis->render->compositor->scene), tr_state);
	}
	gf_mx2d_copy(tr_state->transform, backup);
}

