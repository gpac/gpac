/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
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


#if !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)

#include "nodes_stacks.h"
#include "texturing.h"

#ifdef GPAC_ENABLE_SVG_FILTERS

typedef struct
{
	GF_TextureHandler txh;
	Drawable *drawable;
	u8 *data;
	u32 alloc_size;
} GF_FilterStack;



void apply_feComponentTransfer(GF_Node *node, GF_TextureHandler *source, GF_Rect *region)
{
	GF_ChildNodeItem *l = ((GF_ParentNode*)node)->children;
	while (l) {
		SVG_Filter_TransferType type = SVG_FILTER_TRANSFER_IDENTITY;
		u32 i, j;
		GF_List *table = NULL;
		Fixed slope = FIX_ONE;
		Fixed intercept = 0;
		Fixed amplitude = 1;
		Fixed exponent = 1;
		Fixed offset = 0;
		char *ptr = NULL;
		/*FIXME: unused u32 tag = gf_node_get_tag(l->node);*/
		GF_DOMAttribute *att = ((SVG_Element *)l->node)->attributes;

		while (att) {
			switch (att->tag) {
			case TAG_SVG_ATT_filter_transfer_type:
				type = *(u8*)att->data;
				break;
			case TAG_SVG_ATT_filter_table_values:
				table = *(GF_List **)att->data;
				break;
			case TAG_SVG_ATT_slope:
				slope = ((SVG_Number *)att->data)->value;
				break;
			case TAG_SVG_ATT_filter_intercept:
				intercept = ((SVG_Number *)att->data)->value;
				break;
			case TAG_SVG_ATT_filter_amplitude:
				amplitude = ((SVG_Number *)att->data)->value;
				break;
			case TAG_SVG_ATT_filter_exponent:
				exponent = ((SVG_Number *)att->data)->value;
				break;
			case TAG_SVG_ATT_offset:
				offset = ((SVG_Number *)att->data)->value;
				break;
			}
			att = att->next;
		}
		if (type==SVG_FILTER_TRANSFER_IDENTITY) {
			l = l->next;
			continue;
		}
		switch (gf_node_get_tag(l->node)) {
		case TAG_SVG_feFuncR:
			if (source->pixelformat==GF_PIXEL_RGBA) ptr = source->data;
			else ptr = source->data+2;
			break;
		case TAG_SVG_feFuncG:
			if (source->pixelformat==GF_PIXEL_RGBA) ptr = source->data+1;
			else ptr = source->data+1;
			break;
		case TAG_SVG_feFuncB:
			if (source->pixelformat==GF_PIXEL_RGBA) ptr = source->data+2;
			else ptr = source->data;
			break;
		case TAG_SVG_feFuncA:
			if (source->pixelformat==GF_PIXEL_RGBA) ptr = source->data+3;
			else ptr = source->data+3;
			break;
		}

		if ((type==SVG_FILTER_TRANSFER_LINEAR) && (intercept || (slope!=FIX_ONE)) ) {
			intercept *= 255;
			assert( ptr );
			for (i=0; i<source->height; i++) {
				for (j=0; j<source->width; j++) {
					Fixed p = (*ptr) * slope + intercept;
					ptr[0] = (u8) MIN(MAX(0, p), 255);
					ptr += 4;
				}
			}
		} else if (type==SVG_FILTER_TRANSFER_GAMMA) {
			assert( ptr );
			for (i=0; i<source->height; i++) {
				for (j=0; j<source->width; j++) {
					Fixed p = 255 * gf_mulfix(amplitude,  FLT2FIX( pow( INT2FIX(*ptr)/255, FIX2FLT(exponent) ) ) ) + offset;
					ptr[0] = (u8) MIN(MAX(0, p), 255);
					ptr += 4;
				}
			}
		} else if ((type==SVG_FILTER_TRANSFER_TABLE) && table && (gf_list_count(table)>=2) ) {
			u32 count = gf_list_count(table);
			u32 N = count-1;
			assert( ptr );
			for (i=0; i<source->height; i++) {
				for (j=0; j<source->width; j++) {
					SVG_Number *vk, *vk1;
					Fixed p = INT2FIX(ptr[0]) / 255;
					Fixed pN = p*N;
					u32 k = FIX2INT(p*N);
					if (k==N) k--;
					vk = gf_list_get(table, k);
					vk1 = gf_list_get(table, k+1);
					p = 255 * ( vk->value + gf_mulfix( pN - INT2FIX(k), (vk1->value - vk->value)) );
					ptr[0] = (u8) MIN(MAX(0, p), 255);
					ptr += 4;
				}
			}
		} else if ((type==SVG_FILTER_TRANSFER_DISCRETE) && table && gf_list_count(table) ) {
			u32 count = gf_list_count(table);
			assert( ptr );
			for (i=0; i<source->height; i++) {
				for (j=0; j<source->width; j++) {
					SVG_Number *vk;
					Fixed p = INT2FIX(ptr[0]) / 255;
					Fixed pN = p*count;
					u32 k = 0;
					while (k<count) {
						if ((s32)k+1>pN) break;
						k++;
					}
					if (k) k--;
					vk = gf_list_get(table, k);
					p = 255 * vk->value;
					ptr[0] = (u8) MIN(MAX(0, p), 255);
					ptr += 4;
				}
			}
		}
		l = l->next;
	}
}

void svg_filter_apply(GF_Node *node, GF_TextureHandler *source, GF_Rect *region)
{
	GF_ChildNodeItem *l = ((GF_ParentNode*)node)->children;

	while (l) {
		switch (gf_node_get_tag(l->node)) {
		case TAG_SVG_feComponentTransfer:
			apply_feComponentTransfer(l->node, source, region);
			break;
		}
		l = l->next;
	}
}

/*
	This is a crude draft implementation of filter. The main drawback is that we don't cache any data.
	We should be able to check for changes in the sub-group or in the filter
*/
void svg_draw_filter(GF_Node *filter, GF_Node *node, GF_TraverseState *tr_state)
{
	GF_IRect rc1, rc2;

#ifndef GPAC_DISABLE_3D
	u32 type_3d;
#endif
	u32 prev_flags;
	GF_IRect txrc;
	Fixed scale_x, scale_y, temp_x, temp_y;
	DrawableContext *ctx, *child_ctx;
	GF_EVGSurface *offscreen_surface, *old_surf;
	GF_Rect bounds, local_bounds, rc;
	GF_Matrix2D backup;
	SVGAllAttributes all_atts;
	GF_FilterStack *st = gf_node_get_private(filter);
	assert(tr_state->traversing_mode==TRAVERSE_SORT);

	/*store the current transform matrix, create a new one for group_cache*/
	gf_mx2d_copy(backup, tr_state->transform);
	gf_mx2d_init(tr_state->transform);

	gf_node_allow_cyclic_traverse(node);
	tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
	tr_state->bounds.width = tr_state->bounds.height = 0;
	gf_node_traverse(node, tr_state);

	local_bounds = bounds = tr_state->bounds;
	/*compute bounds in final coordinate system - this ensures that the cache has the correct anti aliasing*/
	gf_mx2d_apply_rect(&backup, &bounds);
	txrc = gf_rect_pixelize(&bounds);
	if (txrc.width%2) txrc.width++;
	if (txrc.height%2) txrc.height++;
	bounds = gf_rect_ft(&txrc);

	tr_state->traversing_mode = TRAVERSE_SORT;

	gf_mx2d_copy(tr_state->transform, backup);

	if (!bounds.width || !bounds.height) {
		return;
	}

	/*create a context */
	ctx = drawable_init_context_svg(st->drawable, tr_state, NULL);
	if (!ctx) return;

	/*setup texture */
	st->txh.height = txrc.height;
	st->txh.width = txrc.width;

	st->txh.stride = txrc.width * 4;
	st->txh.pixelformat = GF_PIXEL_ARGB;
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) st->txh.pixelformat = GF_PIXEL_RGBA;
#endif
	st->txh.transparent = 1;

	if (st->txh.stride * st->txh.height > st->alloc_size) {
		st->alloc_size = st->txh.stride * st->txh.height;
		st->data = (u8*)gf_realloc(st->data, sizeof(u8) * st->alloc_size);
	}
	memset(st->data, 0x0, sizeof(char) * st->txh.stride * st->txh.height);
	st->txh.data = (char *) st->data;
	/*setup geometry (rectangle matching the bounds of the object)
	Warning, we want to center the cached bitmap at the center of the screen (main visual)*/
	gf_path_reset(st->drawable->path);

	gf_path_add_rect_center(st->drawable->path,
	                        bounds.x + bounds.width/2,
	                        bounds.y - bounds.height/2,
	                        bounds.width,
	                        bounds.height);


	old_surf = tr_state->visual->raster_surface;
	offscreen_surface = gf_evg_surface_new(tr_state->visual->center_coords);
	tr_state->visual->raster_surface = offscreen_surface;

	gf_mx2d_copy(backup, tr_state->transform);
	gf_mx2d_init(tr_state->transform);

	/*attach the buffer to visual*/
	gf_evg_surface_attach_to_buffer(offscreen_surface, st->txh.data,
	        st->txh.width,
	        st->txh.height,
	        0,
	        st->txh.stride,
	        st->txh.pixelformat);

	prev_flags = tr_state->immediate_draw;
	tr_state->immediate_draw = 1;
	tr_state->traversing_mode = TRAVERSE_SORT;
	tr_state->in_svg_filter = 1;

	/*recompute the bounds with the final scaling used*/
	scale_x = gf_divfix(bounds.width, local_bounds.width);
	scale_y = gf_divfix(bounds.height, local_bounds.height);
	gf_mx2d_init(tr_state->transform);
	gf_mx2d_add_scale(&tr_state->transform, scale_x, scale_y);

	rc = local_bounds;
	gf_mx2d_apply_rect(&tr_state->transform, &rc);

	/*centered the bitmap on the visual*/
	if (tr_state->visual->center_coords) {
		temp_x = -rc.x - rc.width/2;
		temp_y = rc.height/2 - rc.y;
	} else {
		temp_x = -rc.x;
		temp_y = rc.height - rc.y;
	}
	gf_mx2d_add_translation(&tr_state->transform, temp_x, temp_y);


	rc1 = tr_state->visual->surf_rect;
	rc2 = tr_state->visual->top_clipper;
	tr_state->visual->surf_rect.width = st->txh.width;
	tr_state->visual->surf_rect.height = st->txh.height;
	if (tr_state->visual->center_coords) {
		tr_state->visual->surf_rect.y = st->txh.height/2;
		tr_state->visual->surf_rect.x = -1 * (s32) st->txh.width/2;
	} else {
		tr_state->visual->surf_rect.y = st->txh.height;
		tr_state->visual->surf_rect.x = 0;
	}
	tr_state->visual->top_clipper = tr_state->visual->surf_rect;


#ifndef GPAC_DISABLE_3D
	type_3d = tr_state->visual->type_3d;
	tr_state->visual->type_3d=0;
#endif

	if (prev_flags) gf_node_allow_cyclic_traverse(node);
	gf_node_traverse(node, tr_state);

	child_ctx = ctx->next;
	while (child_ctx && child_ctx->drawable) {
		drawable_reset_bounds(child_ctx->drawable, tr_state->visual);
		child_ctx->drawable = NULL;
		child_ctx = child_ctx->next;
	}
	tr_state->visual->cur_context = ctx;


	/*restore state and destroy whatever needs to be cleaned*/
	tr_state->in_svg_filter = 0;
	tr_state->immediate_draw = prev_flags;
	gf_evg_surface_delete(offscreen_surface);
	tr_state->visual->raster_surface = old_surf;
	tr_state->traversing_mode = TRAVERSE_SORT;
	tr_state->visual->surf_rect = rc1;
	tr_state->visual->top_clipper = rc2;
#ifndef GPAC_DISABLE_3D
	tr_state->visual->type_3d = type_3d ;
#endif

	/*update texture*/
	st->txh.transparent = 1;
	st->txh.flags |= GF_SR_TEXTURE_NO_GL_FLIP;
	gf_sc_texture_set_data(&st->txh);
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d)
		gf_sc_texture_push_image(&st->txh, 0, 0);
	else
#endif
		gf_sc_texture_push_image(&st->txh, 0, 1);

	ctx->flags |= CTX_NO_ANTIALIAS;
	ctx->aspect.fill_color = 0;
	ctx->aspect.line_color = 0xFF000000;
	ctx->aspect.fill_texture = &st->txh;
	ctx->flags |= CTX_TEXTURE_DIRTY;

	/*get the filter region*/
	bounds = local_bounds;
	gf_svg_flatten_attributes((SVG_Element *)filter, &all_atts);
	if (!all_atts.filterUnits || (*all_atts.filterUnits==SVG_GRADIENTUNITS_OBJECT)) {
		Fixed v;
		v = all_atts.x ? all_atts.x->value : INT2FIX(-10);
		bounds.x += gf_mulfix(v, bounds.width);
		v = all_atts.y ? all_atts.y->value : INT2FIX(-10);
		bounds.y += gf_mulfix(v, bounds.height);
		v = all_atts.width ? all_atts.width->value : INT2FIX(120);
		bounds.width = gf_mulfix(v, bounds.width);
		v = all_atts.height ? all_atts.height->value : INT2FIX(120);
		bounds.height = gf_mulfix(v, bounds.height);
	} else {
		bounds.x = all_atts.x ? all_atts.x->value : 0;
		bounds.y = all_atts.y ? all_atts.y->value : 0;
		bounds.width = all_atts.width ? all_atts.width->value : bounds.width;
		bounds.height = all_atts.height ? all_atts.height->value : 120;
	}
	gf_mx2d_apply_rect(&backup, &bounds);

	svg_filter_apply(filter, &st->txh, &bounds);


#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (!st->drawable->mesh) {
			st->drawable->mesh = new_mesh();
			mesh_from_path(st->drawable->mesh, st->drawable->path);
		}
		visual_3d_draw_from_context(tr_state->ctx, tr_state);
		ctx->drawable = NULL;
		return;
	}
#endif

	/*we computed the texture in final screen coordinate, so use the identity matrix for the context*/
	gf_mx2d_init(tr_state->transform);
	drawable_finalize_sort(ctx, tr_state, NULL);
	gf_mx2d_copy(tr_state->transform, backup);
}

static void svg_traverse_filter(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	GF_FilterStack *st = gf_node_get_private(node);
	if (is_destroy) {
		drawable_del(st->drawable);
		if (st->data) gf_free(st->data);
		st->txh.data = NULL;
		gf_sc_texture_release(&st->txh);
		gf_sc_texture_destroy(&st->txh);
		gf_free(st);
		return;
	}

	if (tr_state->traversing_mode==TRAVERSE_DRAW_2D) {
		if (! tr_state->visual->DrawBitmap(tr_state->visual, tr_state, tr_state->ctx)) {
			visual_2d_texture_path(tr_state->visual, st->drawable->path, tr_state->ctx, tr_state);
		}
	}
}

void compositor_init_svg_filter(GF_Compositor *compositor, GF_Node *node)
{
	GF_FilterStack *stack;
	GF_SAFEALLOC(stack, GF_FilterStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg filter stack\n"));
		return;
	}
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_filter);


	gf_sc_texture_setup(&stack->txh, compositor, node);
	stack->drawable = drawable_new();
	/*draw the cache through traverse callback*/
	stack->drawable->flags |= DRAWABLE_USE_TRAVERSE_DRAW;
	stack->drawable->node = node;
	gf_sc_texture_allocate(&stack->txh);
}

#endif //GPAC_ENABLE_SVG_FILTERS

#endif // !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)
