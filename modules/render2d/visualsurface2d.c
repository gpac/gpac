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

	tmp = malloc(sizeof(VisualSurface2D));
	memset(tmp, 0, sizeof(VisualSurface2D));

	tmp->center_coords = 1;
	ra_init(&tmp->to_redraw);
	tmp->back_stack = gf_list_new();
	tmp->view_stack = gf_list_new();
	tmp->sensors = gf_list_new();
	tmp->prev_nodes_drawn = gf_list_new();
	return tmp;
}

void VS2D_ResetSensors(VisualSurface2D *surf)
{
	while (gf_list_count(surf->sensors)) {
		SensorInfo *ptr = gf_list_get(surf->sensors, 0);
		gf_list_rem(surf->sensors, 0);
		gf_list_del(ptr->nodes_on_top);
		free(ptr);
	}
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
	VS2D_ResetSensors(surf);
	gf_list_del(surf->sensors);
	free(surf);
}


#ifdef _WIN32_WCE
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
	surf->contexts[i]->surface = surf;
	return surf->contexts[i];

}
void VS2D_RemoveLastContext(VisualSurface2D *surf)
{
	if (surf->num_contexts) surf->num_contexts--;
}


void VS2D_DrawableDeleted(struct _visual_surface_2D *surf, struct _drawable *node)
{
	u32 i, j;
	SensorInfo *si;
	gf_list_del_item(surf->prev_nodes_drawn, node);

	i=0;
	while ((si = gf_list_enum(surf->sensors, &i))) {
		if (si->ctx->node==node) {
			i--;
			gf_list_rem(surf->sensors, i);
			gf_list_del(si->nodes_on_top);
			free(si);
		} else {
			DrawableContext *ctx;
			j=0; 
			while ((ctx = gf_list_enum(si->nodes_on_top, &j))) {
				if (ctx->node==node) {
					j--;
					gf_list_rem(si->nodes_on_top, j);
				}
			}
		}
	}
}


void VS2D_InitDraw(VisualSurface2D *surf, RenderEffect2D *eff)
{
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
		rc = gf_rect_center(INT2FIX(surf->width), INT2FIX(surf->height));
	} else {
		rc.x = 0;
		rc.width = INT2FIX(surf->width);
		rc.y = rc.height = INT2FIX(surf->height);
	}
	/*set top-transform to pixelMetrics*/
	if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&eff->transform, eff->min_hsize, eff->min_hsize);

	surf->surf_rect = gf_rect_pixelize(&rc);

	/*setup surface, brush and pen */
	VS2D_InitSurface(surf);

	/*setup viewport*/
	if (gf_list_count(surf->view_stack)) {
		M_Viewport *vp = (M_Viewport *) gf_list_get(surf->view_stack, 0);
		vp_setup((GF_Node *) vp, eff, &rc);
	}

	surf->top_clipper = gf_rect_pixelize(&rc);
	/*if we're requested to invalidate everything, switch to direct render but still request bounds storage*/
	if (eff->invalidate_all) {
		eff->trav_flags |= TF_RENDER_DIRECT | TF_RENDER_STORE_BOUNDS;
		direct_render = 2;
	} else {
		direct_render = (eff->trav_flags & TF_RENDER_DIRECT) ? 1 : 0;
	}

	/*reset sensors*/
	VS2D_ResetSensors(surf);

	/*reset prev nodes if any (previous render was indirect)*/
	count = gf_list_count(surf->prev_nodes_drawn);
	for (i=0; i<count; i++) {
		Drawable *dr = gf_list_get(surf->prev_nodes_drawn, i);
		if (direct_render) {
			/*direct rendering mode, remove all bounds info and unreg node */
			drawable_reset_bounds(dr);
			gf_list_rem(surf->prev_nodes_drawn, i);
			i--;
			count--;
			drawable_unregister_from_surface(dr, surf);
		} else {
			drawable_flush_bounds(dr, surf->render->frame_num);
		}
	}
	if (!direct_render) return;

	/*direct mode, draw background*/
	bck = NULL;
	if (gf_list_count(surf->back_stack)) bck = gf_list_get(surf->back_stack, 0);
	if (bck && bck->isBound) {
		ctx = b2D_GetContext(bck, surf->back_stack);
		ctx->clip = surf->surf_rect;
		ctx->unclip = gf_rect_ft(&ctx->clip);
		eff->draw_background = 1;
		gf_node_render((GF_Node *) bck, eff);
		eff->draw_background = 0;
	} else {
		VS2D_Clear(surf, NULL, 0);
	}
}

void VS2D_RegisterSensor(VisualSurface2D *surf, DrawableContext *ctx)
{
	u32 i, len;
	SensorInfo *si;

	while ((si = gf_list_enum(surf->sensors, &i))) {
		if (gf_rect_overlaps(si->ctx->unclip, ctx->unclip)) {
			gf_list_add(si->nodes_on_top, ctx);
		}
	}
	
	len = gf_list_count(ctx->sensors);
	if (len) {
		/*if any of the attached sensor is active, register*/
		for (i=0; i<len; i++) {
			SensorContext *sc = gf_list_get(ctx->sensors, i);
			if (sc->h_node->IsEnabled(sc->h_node)) goto register_sensor;
		}
		/*disable all sensors*/
		drawctx_reset_sensors(ctx);
	}
	/*check for composite texture*/
	if (!ctx->h_texture || !(ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE) ) return;

	
register_sensor:
	si = malloc(sizeof(SensorInfo));
	si->ctx = ctx;
	si->nodes_on_top = gf_list_new();
	gf_list_add(surf->sensors, si);
}

#if 0
static void remove_hidden_sensors(VisualSurface2D *surf, u32 nodes_to_draw, DrawableContext *under_ctx)
{
	u32 i, k;

	for (i=0; i<nodes_to_draw - 1; i++) {
		if (! gf_irect_inside(under_ctx->clip, surf->contexts[surf->nodes_to_draw[i]]->clip) ) continue;
		if (! gf_list_count(surf->contexts[surf->nodes_to_draw[i]]->sensors) ) continue;

		for (k=0; k< gf_list_count(surf->sensors); k++) {
			SensorInfo *si= gf_list_get(surf->sensors, k);
			if (si->ctx == surf->contexts[surf->nodes_to_draw[i]] ) {
				gf_list_rem(surf->sensors, k);
				gf_list_del(si->nodes_on_top);
				free(si);
				break;
			}
		}
	}
}
#endif


#define CHECK_UNCHANGED		0

static void mark_opaque_areas(VisualSurface2D *surf)
{
	u32 i, k;
#if CHECK_UNCHANGED
	Bool remove;
#endif
	GF_RectArray *ra = &surf->to_redraw;
	if (!ra->count) return;
	ra->opaque_node_index = realloc(ra->opaque_node_index, sizeof(u32) * ra->count);

	for (k=0; k<ra->count; k++) {
#if CHECK_UNCHANGED
		remove = 1;
#endif
		ra->opaque_node_index[k] = 0;

		for (i=surf->num_contexts; i>0; i--) {
			DrawableContext *ctx = surf->contexts[i-1];
			if (ctx->invalid) continue;

			if (!gf_irect_inside(ctx->clip, ra->list[k]) ) {
#if CHECK_UNCHANGED
				if (remove && gf_rect_overlaps(ctx->clip, ra->list[k])) {
					if (ctx->need_redraw) remove = 0;
				}
#endif
				continue;
			}

			/*which opaquely covers the given area */
			if (!ctx->transparent) {
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

	bck = NULL;
	if (gf_list_count(surf->back_stack)) bck = gf_list_get(surf->back_stack, 0);
	if (bck) {
		if (!bck->isBound) redraw_all = 1;
		surf->last_had_back = 1;
		bck_ctx = b2D_GetContext(bck, surf->back_stack);
		if (bck_ctx->redraw_flags) redraw_all = 1;
	} else if (surf->last_had_back) {
		surf->last_had_back = 0;
		redraw_all = 1;
	}
	surf->last_had_back = 0;
	
	max_nodes_allowed = (u32) ((surf->top_clipper.width / MIN_BBOX_SIZE) * (surf->top_clipper.height / MIN_BBOX_SIZE));
	count = surf->num_contexts;
	for (i=0; i<count; i++) {
		ctx = surf->contexts[i];
		if (gf_rect_is_empty(ctx->clip)) {
			ctx->invalid = 1;
			continue;
		}

		drawctx_update_info(ctx);
		if (ctx->redraw_flags) num_changed ++;

		/*node has changed, add to redraw area*/
		if (!redraw_all && ctx->redraw_flags) {
			ra_union_rect(&surf->to_redraw, ctx->clip);
			CHECK_MAX_NODE
		}
		/*otherwise try to remove any sensor hidden below*/
		//if (!ctx->transparent) remove_hidden_sensors(surf, num_to_draw, ctx);
	}

	/*garbage collection*/

	/*clear all remaining bounds since last frames (the ones that moved or that are not drawn this frame)*/
	count = gf_list_count(surf->prev_nodes_drawn);
	for (j=0; j<count; j++) {
		Drawable *n = gf_list_get(surf->prev_nodes_drawn, j);
		while (drawable_get_previous_bound(n, &refreshRect, surf)) {
			if (!redraw_all) {
				ra_union_rect(&surf->to_redraw, refreshRect);
				CHECK_MAX_NODE
			}
		}
		/*if node is marked as undrawn, remove from surface*/
		if (!n->node_was_drawn) {
			drawable_flush_bounds(n, surf->render->frame_num);
			gf_list_rem(surf->prev_nodes_drawn, j);
			j--;
			count--;
			drawable_unregister_from_surface(n, surf);
		}
	}

	CHECK_MAX_NODE

	if (redraw_all) {
		ra_clear(&surf->to_redraw);
		ra_add(&surf->to_redraw, surf->surf_rect);
	} else {
		ra_refresh(&surf->to_redraw);
	}
	/*mark opaque areas to speed up*/
	mark_opaque_areas(surf);

	/*nothing to redraw*/
	if (ra_is_empty(&surf->to_redraw) ) goto exit;
	has_changed = 1;

	/*redraw everything*/
	if (redraw_all) {
		if (bck && bck->isBound) {
			bck_ctx->unclip = gf_rect_ft(&surf->surf_rect);
			bck_ctx->clip = surf->surf_rect;
			bck_ctx->node->Draw(bck_ctx);
		} else {
			VS2D_Clear(surf, &surf->surf_rect, 0);
		}
	} else {
		count = surf->to_redraw.count;
		if (bck_ctx) bck_ctx->unclip = gf_rect_ft(&surf->surf_rect);
		for (k=0; k<count; k++) {
			/*opaque area, skip*/
			if (surf->to_redraw.opaque_node_index[k] > 0) continue;
			if (bck_ctx) {
				bck_ctx->clip = surf->to_redraw.list[k];
				bck_ctx->node->Draw(bck_ctx);
			} else {
				GF_IRect rc = surf->to_redraw.list[k];
				gf_irect_intersect(&rc, &surf->top_clipper);
				VS2D_Clear(surf, &rc, 0);
			}
		}
	}


/*
	fprintf(stdout, "%d nodes to redraw (%d total) - %d dirty rects\n", num_changed, surf->num_contexts, surf->to_redraw.count);
	fprintf(stdout, "DR: X:%d Y:%d W:%d H:%d\n", surf->to_redraw.list[0].x, surf->to_redraw.list[0].y, surf->to_redraw.list[0].width, surf->to_redraw.list[0].height);
*/
	refreshRect = surf->to_redraw.list[0];
	for (i=0; i<surf->num_contexts; i++) {
		ctx = surf->contexts[i];
		if (ctx->invalid) continue;

		if ((surf->to_redraw.count>1) || gf_irect_overlaps(ctx->clip, refreshRect)) {
			surf->draw_node_index = i+1;
			ctx->node->Draw(ctx);
		}
		drawable_register_on_surface(ctx->node, surf);
	}

exit:
	/*clear dirty rects*/
	ra_clear(&surf->to_redraw);
	
	/*terminate surface*/
	VS2D_TerminateSurface(surf);
	return has_changed;
}

DrawableContext *VS2D_PickSensitiveNode(VisualSurface2D *surf, Fixed X, Fixed Y)
{
	u32 i, k, count;
	Fixed x, y;
	DrawableContext *ctx;

	count = gf_list_count(surf->sensors);
	if (!count) return NULL;

	if (!surf->center_coords) {
		x = X + INT2FIX(surf->width>>1);
		y = INT2FIX(surf->height>>1) - Y;
	} else {
		x = X;
		y = Y;
	}


	i = count;
restart:
	for (; i > 0; i--) {
		SensorInfo *si = gf_list_get(surf->sensors, i-1);

		/*check over bounds*/
		if (! gf_point_in_rect(si->ctx->clip, x, y)) continue;

		/*check over covering node for non-SVG*/
		if (surf->render->main_surface_setup==1) {
			for (k=gf_list_count(si->nodes_on_top); k>0; k--) {
				ctx = gf_list_get(si->nodes_on_top, k-1);
				if (! gf_point_in_rect(ctx->clip, x, y) ) continue;
				if (! ctx->node->IsPointOver(ctx, x, y, 0) ) continue;

				/*we're over another node*/
				if (!gf_list_count(ctx->sensors)) return NULL;
				if (i) i--;
				goto restart;
			}
		}
		/*SVG like, depends on sensitivity of node*/
		/* else { } */

		/*check over shape*/
		if (! si->ctx->node->IsPointOver(si->ctx, x, y, 0) ) continue;

		/*check if has sensors */
		if (gf_list_count(si->ctx->sensors)) return si->ctx;

		/*check for composite texture*/
		if (si->ctx->h_texture && (si->ctx->h_texture->flags & GF_SR_TEXTURE_COMPOSITE) ) {
			return CT2D_FindNode(si->ctx->h_texture, si->ctx, x, y);
		}
/*this is correct but VRML/MPEG-4 forbids picking on lines*/
#if 0
		else if (si->ctx->aspect.line_texture && (si->ctx->aspect.line_texture->flags & GF_SR_TEXTURE_COMPOSITE) ) {
			return CT2D_FindNode(si->ctx->aspect.line_texture, si->ctx, x, y);
		}
#endif
		return NULL;
    }
	return NULL;
}

DrawableContext *VS2D_PickContext(VisualSurface2D *surf, Fixed x, Fixed y)
{
	u32 i;
	i = surf->num_contexts;
	for (; i > 0; i--) {
		DrawableContext *ctx = surf->contexts[i-1];
		/*check over bounds*/
		if (!ctx->node || ! gf_point_in_rect(ctx->clip, x, y)) continue;
		/*check over shape*/
		if (!ctx->node->IsPointOver(ctx, x, y, 2) ) continue;

		return ctx;
    }
	return NULL;
}

/* this is to use carefully: picks a node based on the PREVIOUS frame state (no traversing)*/
GF_Node *VS2D_PickNode(VisualSurface2D *surf, Fixed x, Fixed y)
{
	u32 i;
	GF_Node *back;
	M_Background2D *bck;
	bck = NULL;
	back = NULL;
	if (gf_list_count(surf->back_stack)) bck = gf_list_get(surf->back_stack, 0);
	if (bck && bck->isBound) back = (GF_Node *) bck;

	i = surf->num_contexts;

	for (; i > 0; i--) {
		DrawableContext *ctx = surf->contexts[i-1];
		/*check over bounds*/
		if (!ctx->node || ! gf_point_in_rect(ctx->clip, x, y)) continue;
		/*check over shape*/
		if (!ctx->node->IsPointOver(ctx, x, y, 1) ) continue;

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


