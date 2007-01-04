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


typedef struct
{
	GF_Node *owner;
	GF_List *surfaces_links;
	Bool first_render;

	Drawable *node;

	/*for image background*/
	GF_TextureHandler txh;
} Background2DStack;

typedef struct
{
	DrawableContext ctx;
	GF_List *bind_stack;
} BackgroundStatus;

static void DestroyBackground2D(GF_Node *node)
{
	M_Background2D *top;
	Background2DStack *ptr;
	BackgroundStatus *status;
	
	ptr = (Background2DStack *) gf_node_get_private(node);

	drawable_del(ptr->node);


	while (gf_list_count(ptr->surfaces_links)) {
		status = (BackgroundStatus *)gf_list_get(ptr->surfaces_links, 0);
		gf_list_rem(ptr->surfaces_links, 0);
		gf_list_del_item(status->bind_stack, node);

		/*force bind - bindable nodes are the only cases where we generate eventIn in the scene graph*/
		if (gf_list_count(status->bind_stack)) {
			top = (M_Background2D*)gf_list_get(status->bind_stack, 0);
			if (!top->set_bind) {
				top->set_bind = 1;
				if (top->on_set_bind) top->on_set_bind((GF_Node *) top);
			}
		}
		free(status);
	}

	gf_sr_texture_destroy(&ptr->txh);
	gf_list_del(ptr->surfaces_links);
	free(ptr);
}


static BackgroundStatus *b2D_GetStatus(GF_Node *node, Background2DStack *bck, RenderEffect2D *eff)
{
	u32 i;
	BackgroundStatus *status;
	GF_List *stack;

	stack = eff->back_stack;
	if (!stack) return NULL;

	i=0;
	while ((status = (BackgroundStatus *)gf_list_enum(bck->surfaces_links, &i))) {
		if (status->bind_stack == stack) return status;
	}

	GF_SAFEALLOC(status, BackgroundStatus);
	gf_mx2d_init(status->ctx.transform);
	status->ctx.surface = eff->surface;
	status->ctx.aspect.filled = 1;
	status->ctx.node = bck->node;
	status->ctx.h_texture = &bck->txh;
	status->ctx.flags = CTX_IS_BACKGROUND;

	status->bind_stack = stack;
	status->ctx.aspect.fill_color = GF_COL_ARGB(0, 0, 0, 0);
	gf_list_add(bck->surfaces_links, status);
	gf_list_add(stack, node);
	return status;
}


static Bool back_use_texture(M_Background2D *bck)
{
	if (!bck->url.count) return 0;
	if (bck->url.vals[0].OD_ID > 0) return 1;
	if (bck->url.vals[0].url && strlen(bck->url.vals[0].url)) return 1;
	return 0;
}

static void DrawBackground(DrawableContext *ctx)
{
	Background2DStack *bcks = (Background2DStack *) gf_node_get_private(ctx->node->owner);

	if (gf_rect_is_empty(ctx->clip) ) return;

	ctx->flags &= ~CTX_PATH_FILLED;

	if ( back_use_texture((M_Background2D *)ctx->node->owner)) {

		if (!ctx->surface->DrawBitmap) {
			/*set target rect*/
			gf_path_reset(bcks->node->path);
			gf_path_add_rect_center(bcks->node->path, 
								ctx->unclip.x + ctx->unclip.width/2,
								ctx->unclip.y - ctx->unclip.height/2,
								ctx->unclip.width, ctx->unclip.height);

			/*draw texture*/
			VS2D_TexturePath(ctx->surface, bcks->node->path, ctx);

		} else {
			ctx->clip = gf_rect_pixelize(&ctx->unclip);

			/*direct rendering, render without clippers */
			if (ctx->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
				ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &ctx->clip, &ctx->unclip, 0xFF, NULL, NULL);
			}
			/*render bitmap for all dirty rects*/
			else {
				u32 i;
				GF_IRect clip;
				for (i=0; i<ctx->surface->to_redraw.count; i++) {
					/*there's an opaque region above, don't draw*/
					if (ctx->surface->draw_node_index<ctx->surface->to_redraw.opaque_node_index[i]) continue;
					clip = ctx->clip;
					gf_irect_intersect(&clip, &ctx->surface->to_redraw.list[i]);
					if (clip.width && clip.height) {
						ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &clip, &ctx->unclip, 0xFF, NULL, NULL);
					}
				}
			}
		}
		if (bcks->txh.hwtx) ctx->flags |= CTX_APP_DIRTY;
		else ctx->flags &= ~(CTX_APP_DIRTY | CTX_TEXTURE_DIRTY);
	} else {
		/*direct rendering, render without clippers */
		if (ctx->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
			/*directly clear with specified color*/
			VS2D_Clear(ctx->surface, &ctx->clip, ctx->aspect.fill_color);
		} else {
			u32 i;
			GF_IRect clip;
			for (i=0; i<ctx->surface->to_redraw.count; i++) {
				/*there's an opaque region above, don't draw*/
				if (ctx->surface->draw_node_index<ctx->surface->to_redraw.opaque_node_index[i]) continue;
				clip = ctx->clip;
				gf_irect_intersect(&clip, &ctx->surface->to_redraw.list[i]);
				if (clip.width && clip.height) {
					VS2D_Clear(ctx->surface, &clip, ctx->aspect.fill_color);
				}
			}
		}
		ctx->flags &= ~(CTX_APP_DIRTY | CTX_TEXTURE_DIRTY);
	}
}

static void RenderBackground2D(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 col;
	BackgroundStatus *status;
	M_Background2D *bck;
	Background2DStack *bcks = (Background2DStack *) gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) {
		DestroyBackground2D(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		DrawBackground(eff->ctx);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		return;
	}

	status = b2D_GetStatus(node, bcks, eff);
	if (!status) return;

	if (gf_node_dirty_get(node)) {
		status->ctx.flags |= CTX_APP_DIRTY;
		gf_node_dirty_clear(node, 0);
	}

	bck = (M_Background2D *)node;

	if (bcks->first_render) {
		bcks->first_render = 0;
		if (gf_list_get(status->bind_stack, 0) == node) {
			if (!bck->isBound) {
				bck->isBound = 1;
				gf_node_event_out_str((GF_Node *)bck, "isBound");
			}
		}
		/*open the stream if any*/
		if (back_use_texture(bck) && !bcks->txh.is_open) {
			gf_sr_texture_play(&bcks->txh, &bck->url);
		}

		/*we're in direct rendering and we missed background drawing - reset*/
		if (bck->isBound && (eff->trav_flags & TF_RENDER_DIRECT) && !eff->draw_background) {
			gf_sr_invalidate(eff->surface->render->compositor, NULL);
			return;
		}
	}

	if (!bck->isBound) return;
	if (!eff->draw_background && (eff->trav_flags & TF_RENDER_DIRECT)) return;

	/*background context bounds are always setup by parent group/surface*/
	if (back_use_texture(bck) ) {
		if (bcks->txh.hwtx && !(status->ctx.flags & CTX_APP_DIRTY) && bcks->txh.needs_refresh) 
			status->ctx.flags |= CTX_TEXTURE_DIRTY;
	} else {
		col = GF_COL_ARGB_FIXED(FIX_ONE, bck->backColor.red, bck->backColor.green, bck->backColor.blue);
		if (col != status->ctx.aspect.fill_color) {
			status->ctx.aspect.fill_color = col;
			status->ctx.flags |= CTX_APP_DIRTY;
		}
	}

	if (!eff->draw_background) return;

//	if (eff->back_stack == eff->surface->back_stack)
//		eff->surface->render->back_color = GF_COL_ARGB_FIXED(FIX_ONE, bck->backColor.red, bck->backColor.green, bck->backColor.blue);

	if (eff->parent) {
		group2d_add_to_context_list(eff->parent, &status->ctx);
	} else if (eff->trav_flags & TF_RENDER_DIRECT) {
		DrawBackground(&status->ctx);
	}
}


static void b2D_set_bind(GF_Node *node)
{
	u32 i;
	Bool isOnTop;
	BackgroundStatus *status;
	M_Background2D *newTop;
	M_Background2D *bck = (M_Background2D *) node;
	Background2DStack *bcks = (Background2DStack *)gf_node_get_private(node);

	i=0;
	while ((status = (BackgroundStatus *)gf_list_enum(bcks->surfaces_links, &i))) {
		isOnTop = (gf_list_get(status->bind_stack, 0)==node) ? 1 : 0;

		if (! bck->set_bind) {
			if (bck->isBound) {
				bck->isBound = 0;
				gf_node_event_out_str(node, "isBound");
			}
			if (isOnTop && (gf_list_count(status->bind_stack)>1)) {
				gf_list_rem(status->bind_stack, 0);
				gf_list_add(status->bind_stack, node);
				newTop = (M_Background2D*)gf_list_get(status->bind_stack, 0);
				newTop->set_bind = 1;
				newTop->on_set_bind((GF_Node *) newTop);
			}
		} else {
			if (! bck->isBound) {
				bck->isBound = 1;
				gf_node_event_out_str(node, "isBound");
				gf_node_dirty_set(node, 0, 0);
			}
			if (!isOnTop) {
				newTop = (M_Background2D*)gf_list_get(status->bind_stack, 0);
				gf_list_del_item(status->bind_stack, node);

				gf_list_insert(status->bind_stack, node, 0);
				newTop->set_bind = 0;
				newTop->on_set_bind((GF_Node *) newTop);
			}
		}
	}
	/*and redraw scene*/
	gf_sr_invalidate(gf_sr_get_renderer(node), NULL);
}

static Bool b2D_point_over(struct _drawable_context *ctx, Fixed x, Fixed y, u32 check_type) { return 0; }


DrawableContext *b2D_GetContext(M_Background2D *n, GF_List *from_stack)
{
	u32 i;
	BackgroundStatus *status;
	Background2DStack *ptr = (Background2DStack *)gf_node_get_private((GF_Node *)n);
	i=0;
	while ((status = (BackgroundStatus *)gf_list_enum(ptr->surfaces_links, &i))) {
		if (status->bind_stack == from_stack) return &status->ctx;
	}
	return NULL;
}



static void UpdateBackgroundTexture(GF_TextureHandler *txh)
{
	gf_sr_texture_update_frame(txh, 0);
	/*restart texture if needed (movie background controled by MediaControl)*/
	if (txh->stream_finished && gf_mo_get_loop(txh->stream, 0)) gf_sr_texture_restart(txh);
}

void R2D_InitBackground2D(Render2D *sr, GF_Node *node)
{
	Background2DStack *ptr;
	GF_SAFEALLOC(ptr, Background2DStack);

	ptr->owner = node;
	ptr->surfaces_links = gf_list_new();
	ptr->first_render = 1;
	/*setup rendering object for background*/
	ptr->node = drawable_stack_new(sr, node);
	((M_Background2D *)node)->on_set_bind = b2D_set_bind;


	gf_sr_texture_setup(&ptr->txh, sr->compositor, node);
	ptr->txh.update_texture_fcnt = UpdateBackgroundTexture;

	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, RenderBackground2D);
}

void R2D_Background2DModified(GF_Node *node)
{
	M_Background2D *bck = (M_Background2D *)node;
	Background2DStack *st = (Background2DStack *) gf_node_get_private(node);
	if (!st) return;

	/*if open and changed, stop and play*/
	if (st->txh.is_open) {
		if (! gf_sr_texture_check_url_change(&st->txh, &bck->url)) return;
		gf_sr_texture_stop(&st->txh);
		gf_sr_texture_play(&st->txh, &bck->url);
		return;
	}
	/*if not open and changed play*/
	if (bck->url.count) 
		gf_sr_texture_play(&st->txh, &bck->url);
	gf_sr_invalidate(st->txh.compositor, NULL);
}
