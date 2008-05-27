/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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



#include "nodes_stacks.h"
#include "visual_manager.h"
#include "texturing.h"

typedef struct _bitmap_stack
{
	Drawable *graph;
	/*cached size for 3D mode*/
	SFVec2f size;
	u32 prev_tx_w, prev_tx_h;
	GF_Rect rc;
} BitmapStack;



static void Bitmap_BuildGraph(GF_Node *node, BitmapStack *st, GF_TraverseState *tr_state, GF_Rect *out_rc, Bool notify_changes)
{
	GF_TextureHandler *txh;
	Fixed sx, sy;
	SFVec2f size;

	M_Bitmap *bmp = (M_Bitmap *)node;

	if (!tr_state->appear) return;
	if (! ((M_Appearance *)tr_state->appear)->texture) return;
	txh = gf_sc_texture_get_handler( ((M_Appearance *)tr_state->appear)->texture );
	/*bitmap not ready*/
	if (!txh || !txh->tx_io || !txh->width || !txh->height) {
		if (notify_changes) gf_node_dirty_set(node, 0, 1);
		return;
	}
	/*no change in scale and same texture size*/
	if (!gf_node_dirty_get(node) && (st->prev_tx_w == txh->width) && (st->prev_tx_h == txh->height)) {
		*out_rc = st->rc;
		return;
	}
	st->prev_tx_w = txh->width;
	st->prev_tx_h = txh->height;

	sx = bmp->scale.x; if (sx<0) sx = FIX_ONE;
	sy = bmp->scale.y; if (sy<0) sy = FIX_ONE;

	compositor_adjust_scale(txh->owner, &sx, &sy);

	/*check size change*/
	size.x = txh->width*sx;
	size.y = txh->height*sy;
	/*if we have a PAR update it!!*/
	if (txh->pixel_ar) {
		u32 n = (txh->pixel_ar>>16) & 0xFFFF;
		u32 d = (txh->pixel_ar) & 0xFFFF;
		size.x = ( (txh->width * n) / d) * sx;
	}


	/*we're in meter metrics*/
	if (!tr_state->pixel_metrics) {
		size.x = gf_divfix(size.x, tr_state->min_hsize);
		size.y = gf_divfix(size.y, tr_state->min_hsize);
	}
	*out_rc = st->rc = gf_rect_center(size.x, size.y);

	gf_node_dirty_clear(node, 0);

	if ((st->size.x==size.x) && (st->size.y==size.y)) return; 
	st->size = size;

	/*change in size*/
	if (notify_changes) gf_node_dirty_set(node, 0, 1);

	/*get size with scale*/
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, 0, 0, st->size.x, st->size.y);
}

#ifndef GPAC_DISABLE_3D
static void draw_bitmap_3d(GF_Node *node, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;

	BitmapStack *st = (BitmapStack *)gf_node_get_private(node);
	M_Bitmap *bmp = (M_Bitmap *)node;

	/*no choice but to update the graph since a bitmap may be used with several textures ...*/
	Bitmap_BuildGraph(node, st, tr_state, &tr_state->bounds, 0);

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);

	compositor_3d_draw_bitmap(st->graph, &asp, tr_state, st->size.x, st->size.y, bmp->scale.x, bmp->scale.y);
}
#endif

static void draw_bitmap_2d(GF_Node *node, GF_TraverseState *tr_state)
{
	GF_ColorKey *key, keyColor;
	GF_Compositor *compositor;
	DrawableContext *ctx = tr_state->ctx;
	BitmapStack *st = (BitmapStack *) gf_node_get_private(node);


	compositor = tr_state->visual->compositor;
	/*bitmaps are NEVER rotated (forbidden in spec). In case a rotation was done we still display (reset the skew components)*/
	ctx->transform.m[1] = ctx->transform.m[3] = 0;

	/*check for material key materialKey*/
	key = NULL;
	if (ctx->appear) {
		M_Appearance *app = (M_Appearance *)ctx->appear;
		if ( app->material && (gf_node_get_tag((GF_Node *)app->material)==TAG_MPEG4_MaterialKey) ) {
			M_MaterialKey*mk = (M_MaterialKey*)app->material;
			if (mk->isKeyed) {
				keyColor.r = FIX2INT(mk->keyColor.red * 255);
				keyColor.g = FIX2INT(mk->keyColor.green * 255);
				keyColor.b = FIX2INT(mk->keyColor.blue * 255);
				keyColor.alpha = FIX2INT( (FIX_ONE - mk->transparency) * 255);
				keyColor.low = FIX2INT(mk->lowThreshold * 255);
				keyColor.high = FIX2INT(mk->highThreshold * 255);
				key = &keyColor;

			}
		}
	}

	/*no HW, fall back to the graphics driver*/
	if (!tr_state->visual->DrawBitmap(tr_state->visual, tr_state, ctx, key)) {
		GF_Matrix2D _mat;
		GF_Rect rc = gf_rect_center(ctx->bi->unclip.width, ctx->bi->unclip.height);
		gf_mx2d_copy(_mat, ctx->transform);
		gf_mx2d_inverse(&_mat);
		gf_mx2d_apply_rect(&_mat, &rc);

		drawable_reset_path(st->graph);
		gf_path_add_rect_center(st->graph->path, 0, 0, rc.width, rc.height);
		ctx->flags |= CTX_NO_ANTIALIAS;
		visual_2d_texture_path(tr_state->visual, st->graph->path, ctx, tr_state);
		return;
	}

}

static void TraverseBitmap(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Rect rc;
	DrawableContext *ctx;
	BitmapStack *st = (BitmapStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		drawable_del(st->graph);
		free(st);
		return;
	}

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		draw_bitmap_2d(node, tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		draw_bitmap_3d(node, tr_state);
		return;
#endif
	case TRAVERSE_PICK:
		drawable_pick(st->graph, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		Bitmap_BuildGraph(node, st, tr_state, &tr_state->bounds, 
#ifndef GPAC_DISABLE_3D
			tr_state->visual->type_3d ? 1 : 0
#else
		0
#endif
		);

		return;
	case TRAVERSE_SORT:
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) return;
#endif
		break;
	default:
		return;
	}

	memset(&rc, 0, sizeof(rc));
	Bitmap_BuildGraph(node, st, tr_state, &rc, 1);

	ctx = drawable_init_context_mpeg4(st->graph, tr_state);
	if (!ctx || !ctx->aspect.fill_texture ) {
		visual_2d_remove_last_context(tr_state->visual);
		return;
	}

	/*even if set this is not true*/
	ctx->aspect.pen_props.width = 0;
	ctx->flags |= CTX_NO_ANTIALIAS;

	ctx->flags &= ~CTX_IS_TRANSPARENT;
	/*if clipper then transparent*/

	if (ctx->aspect.fill_texture->transparent) {
		ctx->flags |= CTX_IS_TRANSPARENT;
	} else {
		M_Appearance *app = (M_Appearance *)ctx->appear;
		if ( app->material && (gf_node_get_tag((GF_Node *)app->material)==TAG_MPEG4_MaterialKey) ) {
			if (((M_MaterialKey*)app->material)->isKeyed) {
				if (((M_MaterialKey*)app->material)->transparency==FIX_ONE) {
					visual_2d_remove_last_context(tr_state->visual);
					return;
				}
				ctx->flags |= CTX_IS_TRANSPARENT;
			}
		} 
		else if (!tr_state->color_mat.identity) {
			ctx->flags |= CTX_IS_TRANSPARENT;
		} else {
			u8 alpha = GF_COL_A(ctx->aspect.fill_color);
			/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
			if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);
			if (alpha < 0xFF) ctx->flags |= CTX_IS_TRANSPARENT;
		}
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_sort(ctx, tr_state, &rc);
}


void compositor_init_bitmap(GF_Compositor  *compositor, GF_Node *node)
{
	BitmapStack *st;
	GF_SAFEALLOC(st, BitmapStack);
	st->graph = drawable_new();
	st->graph->node = node;
	st->graph->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, TraverseBitmap);
}

