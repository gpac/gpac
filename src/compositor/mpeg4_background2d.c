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


#include "mpeg4_grouping.h"
#include "visual_manager.h"
#include "texturing.h"
#include "nodes_stacks.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)


#define B2D_PLANE_HSIZE		FLT2FIX(0.5025f)

typedef struct
{
	DrawableContext ctx;
	BoundInfo bi;
} BackgroundStatus;

static void DestroyBackground2D(GF_Node *node)
{
	Background2DStack *stack = (Background2DStack *) gf_node_get_private(node);

	PreDestroyBindable(node, stack->reg_stacks);
	gf_list_del(stack->reg_stacks);

	while (gf_list_count(stack->status_stack)) {
		BackgroundStatus *status = (BackgroundStatus *)gf_list_get(stack->status_stack, 0);
		gf_list_rem(stack->status_stack, 0);
		gf_free(status);
	}
	gf_list_del(stack->status_stack);

	drawable_del(stack->drawable);
	gf_sc_texture_destroy(&stack->txh);
#ifndef GPAC_DISABLE_3D
	if (stack->mesh) mesh_free(stack->mesh);
#endif
	gf_free(stack);
}

static void b2D_new_status(Background2DStack *bck, M_Background2D*back)
{
	BackgroundStatus *status;

	GF_SAFEALLOC(status, BackgroundStatus);
	if (!status) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate background2D status\n"));
		return;
	}
	gf_mx2d_init(status->ctx.transform);
	status->ctx.drawable = bck->drawable;
	status->ctx.flags = CTX_IS_BACKGROUND;
	status->ctx.bi = &status->bi;
	status->ctx.aspect.fill_color = GF_COL_ARGB_FIXED(FIX_ONE, back->backColor.red, back->backColor.green, back->backColor.blue);
	status->ctx.aspect.fill_texture = &bck->txh;
	gf_list_add(bck->status_stack, status);
}

static BackgroundStatus *b2d_get_status(Background2DStack *stack, GF_List *background_stack)
{
	u32 i, count;
	if (!background_stack) return NULL;

	count = gf_list_count(stack->reg_stacks);
	for (i=0; i<count; i++) {
		GF_List *bind_stack = (GF_List *)gf_list_get(stack->reg_stacks, i);
		if (bind_stack == background_stack) {
			return gf_list_get(stack->status_stack, i);
		}
	}
	return NULL;
}


static Bool back_use_texture(M_Background2D *bck)
{
	if (!bck->url.count) return 0;
	if (bck->url.vals[0].OD_ID > 0) return 1;
	if (bck->url.vals[0].url && strlen(bck->url.vals[0].url)) return 1;
	return 0;
}

static void DrawBackground2D_2D(DrawableContext *ctx, GF_TraverseState *tr_state)
{
	Bool clear_all = GF_TRUE;
	u32 color;
	Bool use_texture;
	Bool is_offscreen = GF_FALSE;
	Background2DStack *stack;
	if (!ctx || !ctx->drawable || !ctx->drawable->node) return;
	stack = (Background2DStack *) gf_node_get_private(ctx->drawable->node);

	if (!ctx->bi->clip.width || !ctx->bi->clip.height) return;

	stack->flags &= ~CTX_PATH_FILLED;
	color = ctx->aspect.fill_color;

	use_texture = back_use_texture((M_Background2D *)ctx->drawable->node);
	if (!use_texture && !tr_state->visual->is_attached) {
		use_texture = 1;
		stack->txh.data = stack->col_tx;
		stack->txh.width = 2;
		stack->txh.height = 2;
		stack->txh.stride = 6;
		stack->txh.pixelformat = GF_PIXEL_RGB;
	}

	if (use_texture) {

		if (!tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx)) {
			/*set target rect*/
			gf_path_reset(stack->drawable->path);
			gf_path_add_rect_center(stack->drawable->path,
			                        ctx->bi->unclip.x + ctx->bi->unclip.width/2,
			                        ctx->bi->unclip.y - ctx->bi->unclip.height/2,
			                        ctx->bi->unclip.width, ctx->bi->unclip.height);

			/*draw texture*/
			visual_2d_texture_path(tr_state->visual, stack->drawable->path, ctx, tr_state);
		}
		//a quick hack, if texture not ready return (we don't have direct notification of this through the above functions
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->compositor->hybrid_opengl && !stack->txh.tx_io)
			return;
#endif

		stack->flags &= ~(CTX_APP_DIRTY | CTX_TEXTURE_DIRTY);
		tr_state->visual->has_modif = 1;
#ifndef GPAC_DISABLE_3D
		//in opengl auto mode we still have to clear the canvas
		if (!tr_state->immediate_draw && !tr_state->visual->offscreen && tr_state->visual->compositor->hybrid_opengl) {
			clear_all = GF_FALSE;
			is_offscreen = GF_TRUE;
			color &= 0x00FFFFFF;
		} else
#endif
			return;
	}


#ifndef GPAC_DISABLE_3D
	if (ctx->flags & CTX_BACKROUND_NOT_LAYER) {
		if (clear_all && !tr_state->visual->offscreen && tr_state->visual->compositor->hybrid_opengl) {
			compositor_2d_hybgl_clear_surface(tr_state->visual, NULL, color, GF_FALSE);
			is_offscreen = GF_TRUE;
			color &= 0x00FFFFFF;
			//we may need to clear the canvas for immediate mode
		}
	} else {
		is_offscreen = GF_TRUE;
	}
	if (ctx->flags & CTX_BACKROUND_NO_CLEAR) {
		stack->flags &= ~(CTX_APP_DIRTY | CTX_TEXTURE_DIRTY);
		tr_state->visual->has_modif = 1;
		return;
	}
#endif

	/*direct drawing, draw without clippers */
	if (tr_state->immediate_draw
	   ) {
		/*directly clear with specified color*/
		if (clear_all)
			tr_state->visual->ClearSurface(tr_state->visual, &ctx->bi->clip, color, is_offscreen);
	} else {
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
				tr_state->visual->ClearSurface(tr_state->visual, &clip, color, is_offscreen ? 2 : 0);
			}
		}
	}
	stack->flags &= ~(CTX_APP_DIRTY | CTX_TEXTURE_DIRTY);
	tr_state->visual->has_modif = 1;
}

#ifndef GPAC_DISABLE_3D
static Bool back_texture_enabled(M_Background2D *bck, GF_TextureHandler *txh)
{
	Bool use_texture = back_use_texture(bck);
	if (use_texture) {
		/*texture not ready*/
		if (!txh->tx_io) {
			use_texture = 0;
			gf_sc_invalidate(txh->compositor, NULL);
		}
		gf_sc_texture_set_blend_mode(txh, gf_sc_texture_is_transparent(txh) ? TX_REPLACE : TX_DECAL);
	}
	return use_texture;
}

static void DrawBackground2D_3D(M_Background2D *bck, Background2DStack *st, GF_TraverseState *tr_state)
{
	GF_Matrix mx, bck_mx, bck_mx_cam;
	Bool use_texture;

	use_texture = back_texture_enabled(bck, &st->txh);

	visual_3d_set_background_state(tr_state->visual, 1);

	gf_mx_copy(bck_mx_cam, tr_state->camera->modelview);
	gf_mx_copy(bck_mx, tr_state->model_matrix);

	/*little opt: if we clear the main visual clear it entirely */
	if (! tr_state->is_layer) {
		visual_3d_clear(tr_state->visual, bck->backColor, FIX_ONE);
		if (!use_texture) {
			visual_3d_set_background_state(tr_state->visual, 0);
			return;
		}
		/*we need a hack here because main vp is always traversed before main background, and in the case of a
		2D viewport it modifies the modelview matrix, which we don't want ...*/
		gf_mx_init(tr_state->model_matrix);
		gf_mx_init(tr_state->camera->modelview);
	}
	if (!use_texture || (!tr_state->is_layer && st->txh.transparent) ) visual_3d_set_material_2d(tr_state->visual, bck->backColor, FIX_ONE);
	if (use_texture) {
		visual_3d_set_state(tr_state->visual, V3D_STATE_COLOR, ! tr_state->is_layer);
		tr_state->mesh_num_textures = gf_sc_texture_enable(&st->txh, NULL);
		if (!tr_state->mesh_num_textures) visual_3d_set_material_2d(tr_state->visual, bck->backColor, FIX_ONE);
	}

	/*create mesh object if needed*/
	if (!st->mesh) {
		st->mesh = new_mesh();
		mesh_set_vertex(st->mesh, -B2D_PLANE_HSIZE, -B2D_PLANE_HSIZE, 0,  0,  0,  FIX_ONE, 0, 0);
		mesh_set_vertex(st->mesh,  B2D_PLANE_HSIZE, -B2D_PLANE_HSIZE, 0,  0,  0,  FIX_ONE, FIX_ONE, 0);
		mesh_set_vertex(st->mesh,  B2D_PLANE_HSIZE,  B2D_PLANE_HSIZE, 0,  0,  0,  FIX_ONE, FIX_ONE, FIX_ONE);
		mesh_set_vertex(st->mesh, -B2D_PLANE_HSIZE,  B2D_PLANE_HSIZE, 0,  0,  0,  FIX_ONE, 0, FIX_ONE);
		mesh_set_triangle(st->mesh, 0, 1, 2);
		mesh_set_triangle(st->mesh, 0, 2, 3);
		st->mesh->flags |= MESH_IS_2D;
	}

	gf_mx_init(mx);
	if (tr_state->camera->is_3D) {
		Fixed sx, sy;
		/*reset matrix*/
		gf_mx_init(tr_state->model_matrix);
		sx = sy = 2 * gf_mulfix(gf_tan(tr_state->camera->fieldOfView/2), tr_state->camera->z_far);
		if (tr_state->camera->width > tr_state->camera->height) {
			sx = gf_muldiv(sx, tr_state->camera->width, tr_state->camera->height);
		} else {
			sy = gf_muldiv(sy, tr_state->camera->height, tr_state->camera->width);
		}
		gf_mx_add_scale(&mx, sx, sy, FIX_ONE);
#ifdef GPAC_FIXED_POINT
		gf_mx_add_translation(&mx, 0, 0, - (tr_state->camera->z_far/100)*99);
#else
		gf_mx_add_translation(&mx, 0, 0, -0.995f*tr_state->camera->z_far);
#endif
	} else {
		gf_mx_add_scale(&mx, tr_state->bbox.max_edge.x - tr_state->bbox.min_edge.x,
		                tr_state->bbox.max_edge.y - tr_state->bbox.min_edge.y,
		                FIX_ONE);
		/*when in layer2D, DON'T MOVE BACKGROUND TO ZFAR*/
		if (!tr_state->is_layer) {
			Fixed tr;
#ifdef GPAC_FIXED_POINT
			tr = -(tr_state->camera->z_far/100)*99;
#else
			tr = -0.999f*tr_state->camera->z_far;
#endif
			if (!tr_state->camera->is_3D) tr = -tr;
			gf_mx_add_translation(&mx, 0, 0, tr);
		}
	}
	gf_mx_add_matrix(&tr_state->model_matrix, &mx);
	visual_3d_mesh_paint(tr_state, st->mesh);
	if (tr_state->mesh_num_textures) {
		gf_sc_texture_disable(&st->txh);
		tr_state->mesh_num_textures = 0;
	}

	gf_mx_copy(tr_state->model_matrix, bck_mx);
	gf_mx_copy(tr_state->camera->modelview, bck_mx_cam);

	visual_3d_set_background_state(tr_state->visual, 0);
}
#endif

static void TraverseBackground2D(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 col;
	BackgroundStatus *status;
	M_Background2D *bck;
	Background2DStack *stack = (Background2DStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		DestroyBackground2D(node);
		return;
	}
	if (tr_state->visual->compositor->noback)
		return;

	bck = (M_Background2D *)node;

	/*special case for background in Layer2D: the background is seen as a regular drawable, so
	RENDER_BINDABLE is not used*/
	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		DrawBackground2D_2D(tr_state->ctx, tr_state);
		return;
	case  TRAVERSE_PICK:
	case TRAVERSE_GET_BOUNDS:
		return;
	}

	/*first traverse, bound if needed*/
	if (gf_list_find(tr_state->backgrounds, node) < 0) {
		M_Background2D *top_bck;

		gf_list_add(tr_state->backgrounds, node);
		assert(gf_list_find(stack->reg_stacks, tr_state->backgrounds)==-1);
		gf_list_add(stack->reg_stacks, tr_state->backgrounds);
		b2D_new_status(stack, bck);

		/*only bound if we're on top*/
		top_bck = gf_list_get(tr_state->backgrounds, 0);
		if (!bck->isBound) {
			if (top_bck== bck) {
				Bindable_SetIsBound(node, 1);
			} else if (!top_bck->isBound) {
				bck->set_bind = 1;
				bck->on_set_bind(node, NULL);
			}
		}
		/*open the stream if any*/
		if (back_use_texture(bck) && !stack->txh.is_open) gf_sc_texture_play(&stack->txh, &bck->url);
		/*in any case don't draw the first time (since the background could have been declared last)*/
		gf_sc_invalidate(stack->txh.compositor, NULL);
		return;
	}
	if (!bck->isBound) return;

	status = b2d_get_status(stack, tr_state->backgrounds);
	if (!status) return;

	if (gf_node_dirty_get(node)) {
		u32 i;

		stack->flags |= CTX_APP_DIRTY;
		gf_node_dirty_clear(node, 0);


		col = GF_COL_ARGB_FIXED(FIX_ONE, bck->backColor.red, bck->backColor.green, bck->backColor.blue);
		if (col != status->ctx.aspect.fill_color) {
			status->ctx.aspect.fill_color = col;
			stack->flags |= CTX_APP_DIRTY;
		}
		for (i=0; i<4; i++) {
			stack->col_tx[3*i] = FIX2INT(255 * bck->backColor.red);
			stack->col_tx[3*i+1] = FIX2INT(255 * bck->backColor.green);
			stack->col_tx[3*i+2] = FIX2INT(255 * bck->backColor.blue);
		}
	}

	if (back_use_texture(bck) ) {
#ifndef GPAC_DISABLE_3D
		if (stack->txh.compositor->hybrid_opengl && !tr_state->visual->offscreen && stack->hybgl_init) {
			stack->flags |= CTX_HYBOGL_NO_CLEAR;
		}
		stack->hybgl_init = 1;
#endif
		if (stack->txh.tx_io && !(status->ctx.flags & CTX_APP_DIRTY) && stack->txh.needs_refresh) {
			stack->flags |= CTX_TEXTURE_DIRTY;
		}
	}
	if (status->ctx.flags & CTX_BACKROUND_NOT_LAYER) {
		status->ctx.flags = stack->flags | CTX_BACKROUND_NOT_LAYER;
	} else {
		status->ctx.flags = stack->flags;
		if (tr_state->is_layer)
			status->ctx.flags &= ~CTX_BACKROUND_NOT_LAYER;
	}


	if (tr_state->traversing_mode != TRAVERSE_BINDABLE) return;

	/*3D mode*/
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		DrawBackground2D_3D(bck, stack, tr_state);
	} else
#endif
		DrawBackground2D_2D(&status->ctx, tr_state);
}


static void b2D_set_bind(GF_Node *node, GF_Route *route)
{
	Background2DStack *stack = (Background2DStack *)gf_node_get_private(node);
	Bindable_OnSetBind(node, stack->reg_stacks, NULL);

	if (stack->drawable->flags & DRAWABLE_IS_OVERLAY) {
		stack->txh.compositor->video_out->Blit(stack->txh.compositor->video_out, NULL, NULL, NULL, 1);
	}
	stack->flags |= CTX_APP_DIRTY;

}

DrawableContext *b2d_get_context(M_Background2D *node, GF_List *from_stack)
{
	Background2DStack *stack = (Background2DStack *)gf_node_get_private((GF_Node *)node);
	BackgroundStatus *status = b2d_get_status(stack, from_stack);
	if (status) {
		status->ctx.bi = &status->bi;
		return &status->ctx;
	}
	return NULL;
}



static void UpdateBackgroundTexture(GF_TextureHandler *txh)
{
	gf_sc_texture_update_frame(txh, 0);

	if (!txh->compositor->player && !txh->compositor->passthrough_txh && txh->stream && txh->stream->odm && (txh->stream->odm->flags & GF_ODM_PASSTHROUGH)) {
		if (!txh->width || ((txh->width==txh->compositor->display_width) && (txh->height==txh->compositor->display_height)))
			txh->compositor->passthrough_txh = txh;
		else
			txh->compositor->passthrough_txh = NULL;
	}

	/*restart texture if needed (movie background controled by MediaControl)*/
	if (txh->stream_finished && gf_mo_get_loop(txh->stream, 0))
		gf_sc_texture_restart(txh);
}

void compositor_init_background2d(GF_Compositor *compositor, GF_Node *node)
{
	Background2DStack *ptr;
	GF_SAFEALLOC(ptr, Background2DStack);
	if (!ptr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate background2D stack\n"));
		return;
	}

	ptr->status_stack = gf_list_new();
	ptr->reg_stacks = gf_list_new();
	/*setup drawable object for background*/
	ptr->drawable = drawable_stack_new(compositor, node);
	ptr->drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	((M_Background2D *)node)->on_set_bind = b2D_set_bind;


	gf_sc_texture_setup(&ptr->txh, compositor, node);
	ptr->txh.update_texture_fcnt = UpdateBackgroundTexture;
	ptr->txh.flags = GF_SR_TEXTURE_REPEAT_S | GF_SR_TEXTURE_REPEAT_T;
	ptr->flags = CTX_IS_BACKGROUND;

	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, TraverseBackground2D);
}

void compositor_background2d_modified(GF_Node *node)
{
	M_Background2D *bck = (M_Background2D *)node;
	Background2DStack *st = (Background2DStack *) gf_node_get_private(node);
	if (!st) return;

	/*dirty node and parents in order to trigger parent visual redraw*/
	gf_node_dirty_set(node, 0, 1);

	/*if open and changed, stop and play*/
	if (st->txh.is_open) {
		if (! gf_sc_texture_check_url_change(&st->txh, &bck->url)) return;
		gf_sc_texture_stop(&st->txh);
		gf_sc_texture_play(&st->txh, &bck->url);
		return;
	}
	/*if not open and changed play*/
	if (bck->url.count)
		gf_sc_texture_play(&st->txh, &bck->url);
	gf_sc_invalidate(st->txh.compositor, NULL);
}

#if 0 //unused
Bool compositor_background_transparent(GF_Node *node)
{
	if (node && (gf_node_get_tag(node) == TAG_MPEG4_Background2D)) {
		Background2DStack *st;
		if (!((M_Background2D *)node)->isBound) return 1;

		st = (Background2DStack *) gf_node_get_private(node);
		if (st->txh.transparent) return 1;
		return 0;
	}
	/*consider all other background nodes transparent*/
	return 1;
}
#endif


#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

