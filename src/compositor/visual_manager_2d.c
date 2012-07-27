/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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

//#define SKIP_CONTEXT

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

DrawableContext *visual_2d_get_drawable_context(GF_VisualManager *visual)
{
#ifdef SKIP_CONTEXT
	return NULL;
#endif

	if (!visual->context) {
		visual->context = NewDrawableContext();
		visual->cur_context = visual->context;
		drawctx_reset(visual->context);
		visual->num_nodes_current_frame ++;
		return visual->context;
	}
//	assert(visual->cur_context);
	/*current context is OK*/
	if (!visual->cur_context->drawable) {
		/*reset next context in display list for next call*/
		if (visual->cur_context->next) visual->cur_context->next->drawable = NULL;
		drawctx_reset(visual->cur_context);
		return visual->cur_context;
	}
	/*need a new context and next one is OK*/
	if (visual->cur_context->next) {
		visual->cur_context = visual->cur_context->next;
//		assert(visual->cur_context->drawable == NULL);
		/*reset next context in display list for next call*/
		if (visual->cur_context->next) visual->cur_context->next->drawable = NULL;
		drawctx_reset(visual->cur_context);
		visual->num_nodes_current_frame ++;
		return visual->cur_context;
	}
	/*need to create a new context*/
	visual->cur_context->next = NewDrawableContext();
	visual->cur_context = visual->cur_context->next;
	drawctx_reset(visual->cur_context);
	visual->num_nodes_current_frame ++;

	if (1) {
		u32 i;
		DrawableContext *last = visual->cur_context;
		for (i=0; i<50; i++) {
			last->next = gf_malloc(sizeof(DrawableContext));
			last = last->next;
			last->drawable = NULL;
			last->col_mat = NULL;
		}
		last->next = NULL;
	}
	return visual->cur_context;
}

void visual_2d_remove_last_context(GF_VisualManager *visual)
{
	assert(visual->cur_context);
	visual->cur_context->drawable = NULL;
}


void visual_2d_drawable_delete(GF_VisualManager *visual, struct _drawable *drawable)
{
	DrawableContext *ctx;
	/*remove drawable from visual list*/
	struct _drawable_store *it = visual->prev_nodes;
	struct _drawable_store *prev = NULL;
	while (it) {
		if (it->drawable != drawable) {
			prev = it;
			it = prev->next;
			continue;
		}
		if (prev) prev->next = it->next;
		else visual->prev_nodes = it->next;
		if (!it->next) visual->last_prev_entry = prev;
		gf_free(it);
		break;
	}
	
	ctx = visual->context;
	while (ctx && ctx->drawable) {
		/*remove visual registration flag*/
		if (ctx->drawable == drawable) {
			ctx->flags = 0;
			ctx->drawable = NULL;
		}
		ctx = ctx->next;
	}
	if (drawable->flags & DRAWABLE_IS_OVERLAY) {
		visual->compositor->video_out->Blit(visual->compositor->video_out, NULL, NULL, NULL, 1);
	}
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

void visual_2d_setup_projection(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	GF_Rect rc;

	tr_state->visual = visual;
#ifndef GPAC_DISABLE_VRML
	tr_state->backgrounds = visual->back_stack;
	tr_state->viewpoints = visual->view_stack;
#endif

	/*setup clipper*/
	if (visual->center_coords) {
		if (!visual->offscreen) {
			if (visual->compositor->scalable_zoom)
				rc = gf_rect_center(INT2FIX(visual->compositor->display_width), INT2FIX(visual->compositor->display_height));
			else
				rc = gf_rect_center(INT2FIX(visual->compositor->output_width + 2*visual->compositor->vp_x), INT2FIX(visual->compositor->output_height + 2*visual->compositor->vp_y));
		} else {
			rc = gf_rect_center(INT2FIX(visual->width), INT2FIX(visual->height));
		}
	} else {
		rc.x = 0;
		rc.width = INT2FIX(visual->width);
		rc.y = rc.height = INT2FIX(visual->height);
	}
	/*set top-transform to pixelMetrics*/
	if (!tr_state->pixel_metrics) gf_mx2d_add_scale(&tr_state->transform, tr_state->min_hsize, tr_state->min_hsize);

	visual->surf_rect = gf_rect_pixelize(&rc);

//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] output rectangle setup - width %d height %d\n", visual->surf_rect.width, visual->surf_rect.height));

	/*setup top clipper*/
	if (visual->center_coords) {
		rc = gf_rect_center(INT2FIX(visual->width), INT2FIX(visual->height));
	} else {
		rc.width = INT2FIX(visual->width);
		rc.height = INT2FIX(visual->height);
		rc.x = 0;
		rc.y = rc.height;
		if (visual->compositor->visual==visual) {
			rc.x += INT2FIX(visual->compositor->vp_x);
			rc.y += INT2FIX(visual->compositor->vp_y);
		}
	}

	/*setup viewport*/
#ifndef GPAC_DISABLE_VRML
	if (gf_list_count(visual->view_stack)) {
		tr_state->traversing_mode = TRAVERSE_BINDABLE;
		tr_state->bounds = rc;
		gf_node_traverse((GF_Node *) gf_list_get(visual->view_stack, 0), tr_state);
	}
#endif

	visual->top_clipper = gf_rect_pixelize(&rc);
	tr_state->clipper = rc;
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Cliper setup - %d:%d@%dx%d\n", visual->top_clipper.x, visual->top_clipper.y, visual->top_clipper.width, visual->top_clipper.height));
}

GF_Err visual_2d_init_draw(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	GF_Err e;
	u32 rem, count;
	struct _drawable_store *it, *prev;
#ifndef GPAC_DISABLE_VRML
	DrawableContext *ctx;
	M_Background2D *bck;
#endif
	u32 draw_mode;
	
	/*reset display list*/
	visual->cur_context = visual->context;
	if (visual->context) visual->context->drawable = NULL;
	visual->has_modif = 0;
	visual->has_overlays = 0;

	visual_2d_setup_projection(visual, tr_state);
	if (!visual->top_clipper.width || !visual->top_clipper.height) 
		return GF_OK;

	tr_state->traversing_mode = TRAVERSE_SORT;
	visual->num_nodes_current_frame = 0;

	/*setup raster surface, brush and pen */
	e = visual_2d_init_raster(visual);
	if (e) 
		return e;

	draw_mode = 0;
	if (tr_state->immediate_draw) draw_mode = 1;
	/*if we're requested to invalidate everything, switch to direct drawing but don't reset bounds*/
	else if (tr_state->invalidate_all) {
		tr_state->immediate_draw = 1;
		draw_mode = 2;
	}
	tr_state->invalidate_all = 0;

	/*reset prev nodes if any (previous traverse was indirect)*/
	rem = count = 0;
	prev = NULL;
	it = visual->prev_nodes;
	while (it) {
		/*node was not drawn on this visual*/
		if (!drawable_flush_bounds(it->drawable, visual, draw_mode)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Unregistering previously drawn node %s from visual\n", gf_node_get_class_name(it->drawable->node)));
			
			/*remove all bounds info related to this visual and unreg node */
			drawable_reset_bounds(it->drawable, visual);

			if (prev) prev->next = it->next;
			else visual->prev_nodes = it->next;
			if (!it->next) visual->last_prev_entry = prev;
			rem++;
			gf_free(it);
			it = prev ? prev->next : visual->prev_nodes;
		} else {
			/*mark drawable as already registered with visual*/
			it->drawable->flags |= DRAWABLE_REGISTERED_WITH_VISUAL;
			prev = it;
			it = it->next;
			count++;
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Top visual initialized - %d nodes registered and %d removed - using %s rendering\n", count, rem, draw_mode ? "direct" : "dirty-rect"));
	if (!draw_mode) return GF_OK;

#ifndef GPAC_DISABLE_VRML
	/*direct mode, draw background*/
	bck = (M_Background2D*) gf_list_get(visual->back_stack, 0);
	if (bck && bck->isBound) {
		ctx = b2d_get_context(bck, visual->back_stack);
		if (ctx) {
			/*force clearing entire zone, not just viewport, when using color. If texture, we MUST
			use the VP clipper in order to compute offsets when blitting bitmaps*/
			if (ctx->aspect.fill_texture &&ctx->aspect.fill_texture->stream) {
				ctx->bi->clip = visual->top_clipper;
			} else {
				ctx->bi->clip = visual->surf_rect;
			}
			ctx->bi->unclip = gf_rect_ft(&ctx->bi->clip);
			tr_state->traversing_mode = TRAVERSE_BINDABLE;
			gf_node_traverse((GF_Node *) bck, tr_state);
			tr_state->traversing_mode = TRAVERSE_SORT;
		} else {
			visual->ClearSurface(visual, NULL, 0);
		}
	} else
#endif
	{
		visual->ClearSurface(visual, NULL, 0);
	}
	return GF_OK;
}



/*@rc2 fully contained in @rc1*/
Bool gf_irect_inside(GF_IRect *rc1, GF_IRect *rc2) 
{
	if (!rc1->width || !rc1->height) return 0;
	if ( (rc1->x <= rc2->x)  && (rc1->y >= rc2->y)  && (rc1->x + rc1->width >= rc2->x + rc2->width) && (rc1->y - rc1->height <= rc2->y - rc2->height) )
		return 1;
	return 0;
}


/*clears list*/
#define ra_clear(ra) { (ra)->count = 0; }

/*is list empty*/
#define ra_is_empty(ra) (!((ra)->count))

/*adds @rc2 to @rc1 - the new @rc1 contains the old @rc1 and @rc2*/
void gf_irect_union(GF_IRect *rc1, GF_IRect *rc2) 
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
void ra_union_rect(GF_RectArray *ra, GF_IRect *rc) 
{
	u32 i;

	assert(rc->width && rc->height);

	for (i=0; i<ra->count; i++) { 
		if (gf_irect_overlaps(&ra->list[i], rc)) { 
			gf_irect_union(&ra->list[i], rc); 
			return; 
		} 
	}
	ra_add(ra, rc); 
}

/*returns relation between rc1 and rc2:
 0: rectangles are disjoint
 1: rectangles overlap
 2: rc2 completely covers rc1

*/
static u32 gf_irect_relation(GF_IRect *rc1, GF_IRect *rc2)
{
	if (! rc2->height || !rc2->width || !rc1->height || !rc1->width) return 0;
	if (rc2->x+rc2->width<=rc1->x) return 0;
	if (rc2->x>=rc1->x+rc1->width) return 0;
	if (rc2->y-rc2->height>=rc1->y) return 0;
	if (rc2->y<=rc1->y-rc1->height) return 0;

	if ( (rc2->x <= rc1->x)  && (rc2->y >= rc1->y)  && (rc2->x + rc2->width >= rc1->x + rc1->width) && (rc2->y - rc2->height <= rc1->y - rc1->height) )
		return 2;

	return 1;
}

/*refreshes the content of the array to have only non-overlapping rects*/
void ra_refresh(GF_RectArray *ra)
{
	u32 i, j, k;
	for (i=0; i<ra->count; i++) {
		for (j=i+1; j<ra->count; j++) {
			switch (gf_irect_relation(&ra->list[j], &ra->list[i])) {

			/*both rectangles overlap, merge them and remove opaque node info*/
			case 1:
				gf_irect_union(&ra->list[i], &ra->list[j]);
#ifdef TRACK_OPAQUE_REGIONS
				/*current dirty rect is no longer opaque*/
				ra->opaque_node_index[i] = 0;
#endif
				/*FALLTHROUGH*/
			/*first rectangle covers second, just remove*/
			case 2:
				/*remove rect*/
				k = ra->count - j - 1;
				if (k) {
					memmove(&ra->list[j], & ra->list[j+1], sizeof(GF_IRect)*k);
#ifdef TRACK_OPAQUE_REGIONS
					memmove(&ra->opaque_node_index[j], & ra->opaque_node_index[j+1], sizeof(u32)*k);
#endif
				}
				ra->count--; 
				if (ra->count>=2)
					ra_refresh(ra);
				return;
			default:
				break;
			}
		}
	}
}

static Bool register_context_rect(GF_RectArray *ra, DrawableContext *ctx, u32 ctx_idx, DrawableContext **first_opaque)
{
	u32 i;
	Bool needs_redraw;
#ifdef TRACK_OPAQUE_REGIONS
	Bool is_transparent = 1;
#endif
	GF_IRect *rc = &ctx->bi->clip;
	assert(rc->width && rc->height);

	/*node is modified*/
	needs_redraw = (ctx->flags & CTX_REDRAW_MASK) ? 1 : 0;

	/*node is transparent*/
	if ((ctx->flags & CTX_NO_ANTIALIAS) && !(ctx->flags & CTX_IS_TRANSPARENT) ) {
#ifdef TRACK_OPAQUE_REGIONS
		is_transparent = 0;
#endif
		if ((*first_opaque==NULL) && needs_redraw) *first_opaque = ctx;
	}

	for (i=0; i<ra->count; i++) { 
		if (needs_redraw) {
			switch (gf_irect_relation(&ra->list[i], rc)) { 
			/*context intersects an existing rectangle, merge them and remove opaque idx info*/
			case 1:
				gf_irect_union(&ra->list[i], rc); 
#ifdef TRACK_OPAQUE_REGIONS
				ra->opaque_node_index[i]= 0; 
#endif
				return needs_redraw; 
			/*context covers an existing rectangle, replace rect and add opaque idx info*/
			case 2:
				ra->list[i]= *rc; 
#ifdef TRACK_OPAQUE_REGIONS
				ra->opaque_node_index[i]= is_transparent ? 0 : ctx_idx; 
#endif
				return needs_redraw; 
			}
		}
#ifdef TRACK_OPAQUE_REGIONS
		/*context unchanged coverring an entire area*/
		else if (!is_transparent && gf_irect_inside(rc, &ra->list[i])) {
			/*remove rect*/
			u32 k = ra->count - i - 1;
			if (k) {
				memmove(&ra->list[i], & ra->list[i+1], sizeof(GF_IRect)*k);
				memmove(&ra->opaque_node_index[i], & ra->opaque_node_index[i+1], sizeof(u32)*k);
			}
			ra->count--; 
			i--;
		}
#endif
	}
	/*not found, add rect*/
	if (needs_redraw) {
		ra_add(ra, rc); 
#ifdef TRACK_OPAQUE_REGIONS
		if (!ra->opaque_node_index)
			ra->opaque_node_index = gf_malloc(sizeof(u32)*ra->alloc);

		ra->opaque_node_index[ra->count-1] = is_transparent ? 0 : ctx_idx; 
#endif
	}
	return needs_redraw;
}


static void register_dirty_rect(GF_RectArray *ra, GF_IRect *rc)
{
	/*technically this is correct however the gain is not that big*/
#if 0

#ifdef TRACK_OPAQUE_REGIONS
	u32 i;
	for (i=0; i<ra->count; i++) { 
		switch (gf_irect_relation(rc, &ra->list[i])) { 
		/*dirty area intersects this dirty rectangle, merge them and remove opaque idx info*/
		case 1:
			gf_irect_union(&ra->list[i], rc); 
			ra->opaque_node_index[i]= 0; 
			return; 
		/*dirty area is covered by this dirty rectangle, nothing to do*/
		case 2:
			return; 
		}
	}
#endif
	/*not found, add rect*/
	ra_add(ra, rc); 
#ifdef TRACK_OPAQUE_REGIONS
	if (!ra->opaque_node_index)
		ra->opaque_node_index = gf_malloc(sizeof(u32)*ra->alloc);

	ra->opaque_node_index[ra->count-1] = 0; 
#endif

#else

	ra_add(ra, rc); 

#ifdef TRACK_OPAQUE_REGIONS
	if (!ra->opaque_node_index)
		ra->opaque_node_index = gf_malloc(sizeof(u32)*ra->alloc);

	ra->opaque_node_index[ra->count-1] = 0; 
#endif

#endif
}


Bool visual_2d_terminate_draw(GF_VisualManager *visual, GF_TraverseState *tr_state)
{
	u32 k, i, count, num_nodes, num_changed;
	GF_IRect refreshRect;
	Bool redraw_all;
#ifndef GPAC_DISABLE_VRML
	M_Background2D *bck = NULL;
	DrawableContext *bck_ctx = NULL;
#endif
	DrawableContext *ctx;
	struct _drawable_store *it, *prev;
	DrawableContext *first_opaque = NULL;
	Bool has_clear = 0;
	Bool has_changed = 0;

	/*in direct mode the visual is always redrawn*/
	if (tr_state->immediate_draw) {
		/*flush pending contexts due to overlays*/
		visual_2d_flush_overlay_areas(visual, tr_state);

		visual_2d_release_raster(visual);
		visual_clean_contexts(visual);
		visual->num_nodes_prev_frame = visual->num_nodes_current_frame;
		return 1;
	}

	num_changed = 0;
	
	/*if the aspect ratio has changed redraw everything*/
	redraw_all = tr_state->invalidate_all;

	/*check for background changes for transparent nodes*/
#ifndef GPAC_DISABLE_VRML
	bck = (M_Background2D*)gf_list_get(visual->back_stack, 0);
	if (bck) {
		if (!bck->isBound) {
			if (visual->last_had_back) redraw_all = 1;
			visual->last_had_back = 0;
		} else {
			if (!visual->last_had_back) redraw_all = 1;
			bck_ctx = b2d_get_context(bck, visual->back_stack);
			if (bck_ctx->flags & CTX_REDRAW_MASK) redraw_all = 1;
			visual->last_had_back = (bck_ctx->aspect.fill_texture && !bck_ctx->aspect.fill_texture->transparent) ? 2 : 1;
		}
	} else 
#endif
	if (visual->last_had_back) {
		visual->last_had_back = 0;
		redraw_all = 1;
	}
	
	num_nodes = 0;
	ctx = visual->context;
	while (ctx && ctx->drawable) {
		num_nodes++;

		drawctx_update_info(ctx, visual);
		if (!redraw_all) {
			assert( gf_irect_inside(&visual->top_clipper, &ctx->bi->clip) );
			if (register_context_rect(&visual->to_redraw, ctx, num_nodes, &first_opaque)) {
				num_changed ++;
			}

		}
		ctx = ctx->next;
	}

	/*garbage collection*/

	/*clear all remaining bounds since last frames (the ones that moved or that are not drawn this frame)*/
	prev = NULL;
	it = visual->prev_nodes;
	while (it) {
		while (drawable_get_previous_bound(it->drawable, &refreshRect, visual)) {
			if (!redraw_all) {
				//assert( gf_irect_inside(&visual->top_clipper, &refreshRect) );
				gf_irect_intersect(&refreshRect, &visual->top_clipper);
				register_dirty_rect(&visual->to_redraw, &refreshRect);
				has_clear=1;
			}
		}
		/*if node is marked as undrawn, remove from visual*/
		if (!(it->drawable->flags & DRAWABLE_DRAWN_ON_VISUAL)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Node %s no longer on visual - unregistering it\n", gf_node_get_class_name(it->drawable->node)));

			/*remove all bounds info related to this visual and unreg node */
			drawable_reset_bounds(it->drawable, visual);

			it->drawable->flags &= ~DRAWABLE_REGISTERED_WITH_VISUAL;

			if (it->drawable->flags & DRAWABLE_IS_OVERLAY) {
				visual->compositor->video_out->Blit(visual->compositor->video_out, NULL, NULL, NULL, 1);
			}

			if (prev) prev->next = it->next;
			else visual->prev_nodes = it->next;
			if (!it->next) visual->last_prev_entry = prev;
			gf_free(it);
			it = prev ? prev->next : visual->prev_nodes;
		} else {
			prev = it;
			it = it->next;
		}
	}

	if (redraw_all) {
		ra_clear(&visual->to_redraw);
		ra_add(&visual->to_redraw, &visual->surf_rect);
#ifdef TRACK_OPAQUE_REGIONS
		if (!visual->to_redraw.opaque_node_index) visual->to_redraw.opaque_node_index = gf_malloc(sizeof(u32)*visual->to_redraw.alloc);
		visual->to_redraw.opaque_node_index[0]=0;
#endif
	} else {
		ra_refresh(&visual->to_redraw);

		if (visual->compositor->debug_defer) {
			visual->ClearSurface(visual, &visual->top_clipper, 0);
		}
	}
#ifdef TRACK_OPAQUE_REGIONS
	assert(!visual->to_redraw.count || (visual->to_redraw.opaque_node_index != NULL));
#endif
	
	/*nothing to redraw*/
	if (ra_is_empty(&visual->to_redraw) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] No changes found since last frame - skipping redraw\n"));
		goto exit;
	}
	has_changed = 1;
	tr_state->traversing_mode = TRAVERSE_DRAW_2D;

	if (first_opaque && (visual->to_redraw.count==1) && gf_rect_equal(first_opaque->bi->clip, visual->to_redraw.list[0])) {
		visual->has_modif=0;
		goto skip_background;
	}

	/*redraw everything*/
#ifndef GPAC_DISABLE_VRML
	if (bck_ctx) {
		drawable_check_bounds(bck_ctx, visual);
		tr_state->ctx = bck_ctx;
#ifdef TRACK_OPAQUE_REGIONS
		visual->draw_node_index = 0;
#endif
		/*force clearing entire zone, not just viewport, when using color. If texture, we MUST
		use the VP clipper in order to compute offsets when blitting bitmaps*/
		if (bck_ctx->aspect.fill_texture && bck_ctx->aspect.fill_texture->stream) {
			bck_ctx->bi->clip = visual->top_clipper;
		} else {
			bck_ctx->bi->clip = visual->surf_rect;
		}
		bck_ctx->bi->unclip = gf_rect_ft(&bck_ctx->bi->clip);
		bck_ctx->next = visual->context;
		gf_node_traverse(bck_ctx->drawable->node, tr_state);
	} else 
#endif /*GPAC_DISABLE_VRML*/
	{
		count = visual->to_redraw.count;
		for (k=0; k<count; k++) {
			GF_IRect rc;
			/*opaque area, skip*/
#ifdef TRACK_OPAQUE_REGIONS
			if (visual->to_redraw.opaque_node_index[k] > 0) continue;
#endif
			rc = visual->to_redraw.list[k];
			visual->ClearSurface(visual, &rc, 0);
		}
	}
	if (!redraw_all && !has_clear) visual->has_modif=0;

skip_background:

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_COMPOSE, GF_LOG_INFO)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Visual2D] Redraw %d / %d nodes (all: %s - %d dirty rects\n)", num_changed, num_nodes, redraw_all ? "yes" : "no", visual->to_redraw.count));
		if (visual->to_redraw.count>1) GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("\n"));

		for (i=0; i<visual->to_redraw.count; i++) {
			GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("\tDirtyRect #%d: %d:%d@%dx%d\n", i+1, visual->to_redraw.list[i].x, visual->to_redraw.list[i].y, visual->to_redraw.list[i].width, visual->to_redraw.list[i].height));
			assert(visual->to_redraw.list[i].width);
		}
	}
#endif
	
#ifdef TRACK_OPAQUE_REGIONS
	visual->draw_node_index = 0;
#endif

	ctx = visual->context;
	while (ctx && ctx->drawable) {

#ifdef TRACK_OPAQUE_REGIONS
		visual->draw_node_index ++;
#endif
		tr_state->ctx = ctx;

		/*if overlay we cannot remove the context and cannot draw directly*/
		if (! visual_2d_overlaps_overlay(tr_state->visual, ctx, tr_state)) {

			if (ctx->drawable->flags & DRAWABLE_USE_TRAVERSE_DRAW) {
				gf_node_traverse(ctx->drawable->node, tr_state);
			} else {
				drawable_draw(ctx->drawable, tr_state);
			}
		}
		ctx = ctx->next;
	}
	/*flush pending contexts due to overlays*/
	visual_2d_flush_overlay_areas(visual, tr_state);
#ifndef GPAC_DISABLE_VRML
	if (bck_ctx) bck_ctx->next = NULL;
#endif

	if (visual->direct_flush) {
		GF_DirtyRectangles dr;
		dr.count = visual->to_redraw.count;
		dr.list = visual->to_redraw.list;
		visual->compositor->video_out->FlushRectangles(visual->compositor->video_out, &dr);
		visual->compositor->skip_flush=1;
	}

exit:
	/*clear dirty rects*/
	ra_clear(&visual->to_redraw);
	visual_2d_release_raster(visual);
	visual_clean_contexts(visual);
	visual->num_nodes_prev_frame = visual->num_nodes_current_frame;
	return has_changed;
}

Bool visual_2d_draw_frame(GF_VisualManager *visual, GF_Node *root, GF_TraverseState *tr_state, Bool is_root_visual)
{
	GF_SceneGraph *sg;
	GF_Matrix2D backup;
	u32 i;
	GF_Err e;
#ifndef GPAC_DISABLE_LOG
	u32 itime, time = gf_sys_clock();
#endif

	gf_mx2d_copy(backup, tr_state->transform);
	visual->bounds_tracker_modif_flag = DRAWABLE_HAS_CHANGED;

	e = visual_2d_init_draw(visual, tr_state);
	if (e) {
		gf_mx2d_copy(tr_state->transform, backup);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Visual2D] Cannot init draw phase: %s\n", gf_error_to_string(e)));
		return 0;
	}

#ifndef GPAC_DISABLE_LOG
	itime = gf_sys_clock();
	visual->compositor->traverse_setup_time = itime - time;
	time = itime;
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Traversing scene subtree (root node %s)\n", root ? gf_node_get_class_name(root) : "none"));

	if (is_root_visual) {
		gf_node_traverse(root, tr_state);

		i=0;
		while ((sg = (GF_SceneGraph*)gf_list_enum(visual->compositor->extra_scenes, &i))) {
			gf_sc_traverse_subscene(visual->compositor, root, sg, tr_state);
		}
	} else {
		gf_node_traverse(root, tr_state);
	}

#ifndef GPAC_DISABLE_LOG
	itime = gf_sys_clock();
	visual->compositor->traverse_and_direct_draw_time = itime - time;
	time = itime;
#endif

	gf_mx2d_copy(tr_state->transform, backup);
	e = visual_2d_terminate_draw(visual, tr_state);
	
#ifndef GPAC_DISABLE_LOG
	if (!tr_state->immediate_draw) {
		visual->compositor->indirect_draw_time = gf_sys_clock() - time;
	}
#endif

	return e;
}


void visual_2d_pick_node(GF_VisualManager *visual, GF_TraverseState *tr_state, GF_Event *ev, GF_ChildNodeItem *children)
{
	GF_Matrix2D backup;
	visual->bounds_tracker_modif_flag = DRAWABLE_HAS_CHANGED_IN_LAST_TRAVERSE;

	gf_mx2d_copy(backup, tr_state->transform);

	visual_2d_setup_projection(visual, tr_state);

	visual->compositor->hit_node = NULL;
	tr_state->ray.orig.x = INT2FIX(ev->mouse.x);
	tr_state->ray.orig.y = INT2FIX(ev->mouse.y);
	tr_state->ray.orig.z = 0;
	tr_state->ray.dir.x = 0;
	tr_state->ray.dir.y = 0;
	tr_state->ray.dir.z = -FIX_ONE;
	
	visual->compositor->hit_world_point = tr_state->ray.orig;
	visual->compositor->hit_world_ray = tr_state->ray;
	visual->compositor->hit_square_dist = 0;

	gf_list_reset(visual->compositor->sensors);
	tr_state->traversing_mode = TRAVERSE_PICK;

	/*not the root scene, use children list*/
	if (visual->compositor->visual != visual) {
		while (children) {
			gf_node_traverse(children->node, tr_state);
			children = children->next;
		}
	} else {
		u32 i = 0;
		GF_SceneGraph *sg = visual->compositor->scene;
		GF_Node *root = gf_sg_get_root_node(sg);
		gf_node_traverse(root, tr_state);
		while ((sg = (GF_SceneGraph*)gf_list_enum(visual->compositor->extra_scenes, &i))) {
			gf_sc_traverse_subscene(visual->compositor, root, sg, tr_state);
		}
	}
	gf_mx2d_copy(tr_state->transform, backup);
}

