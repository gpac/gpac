/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Yi-Zhen Zhang, Jean Le Feuvre
 *
 *			Copyright (c) ENST 2005-200X
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

#include "offscreen_cache.h"

#include "visual_manager.h"
#include "mpeg4_grouping.h"
#include "texturing.h"

#define NUM_STATS_FRAMES	2

void group_cache_draw(GroupCache *cache, GF_TraverseState *tr_state) 
{
	GF_TextureHandler *old_txh = tr_state->ctx->aspect.fill_texture;
	/*switch the texture to our offscreen cache*/
	tr_state->ctx->aspect.fill_texture = &cache->txh;

	/*if skew/rotate, don't try the bitmap Blit (HW/SW)*/
	if (tr_state->ctx->transform.m[1] || tr_state->ctx->transform.m[3]) { 
		visual_2d_texture_path(tr_state->visual, cache->drawable->path, tr_state->ctx);
	} else {
		DrawableContext *ctx = tr_state->ctx;
		GF_Rect unclip;
		GF_IRect clip;
		u8 alpha = GF_COL_A(ctx->aspect.fill_color);
		/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
		if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);

		unclip = ctx->bi->unclip;
		clip = ctx->bi->clip;

		/*direct drawing, draw without clippers */
		if (tr_state->direct_draw) {
			tr_state->visual->DrawBitmap(tr_state->visual, ctx->aspect.fill_texture, ctx, &clip, &unclip, alpha, NULL);
		}
		/*draw bitmap for all dirty rects*/
		else {
			u32 i;
			GF_IRect a_clip;
			for (i=0; i<tr_state->visual->to_redraw.count; i++) {
				/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
				if (tr_state->visual->draw_node_index < tr_state->visual->to_redraw.opaque_node_index[i]) continue;
#endif
				a_clip = clip;
				gf_irect_intersect(&a_clip, &tr_state->visual->to_redraw.list[i]);
				if (a_clip.width && a_clip.height) {
					tr_state->visual->DrawBitmap(tr_state->visual, ctx->aspect.fill_texture, ctx, &a_clip, &unclip, alpha, NULL);
				}
			}
		}
	}

	tr_state->ctx->aspect.fill_texture = old_txh;
}

GroupCache *group_cache_new(GF_Compositor *compositor, GF_Node *node)
{
	GroupCache *cache;
	GF_SAFEALLOC(cache, GroupCache);
	gf_sc_texture_setup(&cache->txh, compositor, node);
	cache->drawable = drawable_new();
	/*draw the cache through traverse callback*/
	cache->drawable->flags |= DRAWABLE_USE_TRAVERSE_DRAW;
	cache->drawable->node = node;
	cache->opacity = FIX_ONE;
	gf_sc_texture_allocate(&cache->txh);
	return cache;
}

void group_cache_del(GroupCache *cache)
{
	drawable_del_ex(cache->drawable, cache->txh.compositor);
	if (cache->txh.data) free(cache->txh.data);
	gf_sc_texture_release(&cache->txh);
	gf_sc_texture_destroy(&cache->txh);
	free(cache);
}

void group_cache_setup(GroupCache *cache, GF_Rect *path_bounds, GF_IRect *pix_bounds, GF_Compositor *compositor, Bool for_gl)
{
	/*setup texture */
	cache->txh.compositor = compositor;
	cache->txh.height = pix_bounds->height;
	cache->txh.width = pix_bounds->width;

	cache->txh.stride = pix_bounds->width * 4;
	cache->txh.pixelformat = for_gl ? GF_PIXEL_RGBA : GF_PIXEL_ARGB;

	if (cache->txh.data) free(cache->txh.data);

	cache->txh.data = (u8 *) malloc (sizeof(char) * cache->txh.stride * cache->txh.height);
	memset(cache->txh.data, 0, sizeof(char) * cache->txh.stride * cache->txh.height);
	/*the path of drawable_cache is a rectangle one that is the the bound of the object*/
	gf_path_reset(cache->drawable->path);

	/*set a rectangle to the path
	  Attention, we want to center the cached bitmap at the center of the screen (main visual), so we use
	  the local coordinate to parameterize the path*/
	gf_path_add_rect_center(cache->drawable->path, 
		path_bounds->x + path_bounds->width/2,
		path_bounds->y - path_bounds->height/2,
		path_bounds->width, path_bounds->height);
}

Bool group_cache_traverse(GF_Node *node, GroupCache *cache, GF_TraverseState *tr_state, Bool force_recompute)
{
	DrawableContext *group_ctx = NULL;	
	GF_ChildNodeItem *l;

	if (!cache) return 0;

	/*do we need to recompute the cache*/
	if (cache->force_recompute) {
		force_recompute = 1;
		cache->force_recompute = 0;
	}
	else if (gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY) {
		force_recompute = 1;
	} 


	/*we need to redraw the group in an offscreen visual*/
	if (force_recompute) {
		GF_Matrix2D backup;
		GF_IRect rc1, rc2;
		u32 type_3d;
		u32 prev_flags;
		GF_Rect cache_bounds;
		GF_SURFACE offscreen_surface, old_surf;
		GF_Raster2D *r2d = tr_state->visual->compositor->rasterizer;
		DrawableContext *child_ctx;
		Fixed temp_x, temp_y, scale_x, scale_y;

		/*step 1 : store current state and indicate children should not be cached*/
		tr_state->in_group_cache = 1;
		prev_flags = tr_state->direct_draw;
		/*store the current transform matrix, create a new one for group_cache*/
		gf_mx2d_copy(backup, tr_state->transform);
		gf_mx2d_init(tr_state->transform);

		type_3d = 0;
#ifndef GPAC_DISABLE_3D
		/*force 2D rendering*/
		type_3d = tr_state->visual->type_3d;
		tr_state->visual->type_3d = 0;
#endif

		/*step 2: insert a DrawableContext for this group in the display list*/
		group_ctx = drawable_init_context_mpeg4(cache->drawable, tr_state);

		/*step 3: collect the bounds of all children*/		
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		cache_bounds.width = cache_bounds.height = 0;
		l = ((GF_ParentNode*)node)->children;
		while (l) {
			gf_node_traverse(l->node, tr_state);
			l = l->next;
			/*TODO for dynamic group caching: add some tests*/
			/*1. do we have a texture on one drawable - if so maybe no cache*/
			/*2- how many drawable*/
			/*3- what is the complexity of each drawable 
				a. nb points on path
				b. nb points in outlines
				c. alpha used
				...
			*/
			gf_rect_union(&cache_bounds, &tr_state->bounds);
		}
		tr_state->traversing_mode = TRAVERSE_SORT;


		/*step 4: now we have the bounds:
			allocate the offscreen memory
			create temp raster visual & attach to buffer
			override the tr_state->visual->the_surface with the temp raster
			!! SAME AS DRAWABLE CACHE: WATCHOUT FOR NON-CENTERED RECT (add translation to visual and 
			build proper path)
			setup top clipers
		*/
		old_surf = tr_state->visual->raster_surface;
		offscreen_surface = r2d->surface_new(r2d, tr_state->visual->center_coords);	/*a new temp raster visual*/
		tr_state->visual->raster_surface = offscreen_surface;

		/*don't waste memory: if scale down is setup at root level, apply it*/
		scale_x = MIN(tr_state->visual->compositor->scale_x, FIX_ONE);
		scale_y = MIN(tr_state->visual->compositor->scale_y, FIX_ONE);

		tr_state->bounds = cache_bounds;
		gf_mx2d_add_scale(&tr_state->transform, scale_x, scale_y);
		gf_mx2d_apply_rect(&tr_state->transform, &cache_bounds);
		rc1 = gf_rect_pixelize(&cache_bounds);
		if (rc1.width%2) rc1.width++;
		if (rc1.height%2) rc1.height++;

		/* Initialize the group cache with the scaled pixelized bounds for texture but the original bounds for path*/
		group_cache_setup(cache, &tr_state->bounds, &rc1, tr_state->visual->compositor, type_3d);
			
		/*attach the buffer to visual*/
		r2d->surface_attach_to_buffer(offscreen_surface, cache->txh.data,
										cache->txh.width, 
										cache->txh.height,
										cache->txh.stride, 
										cache->txh.pixelformat);
		
		/*centered the bitmap on the visual*/
		temp_x = -cache_bounds.x;
		temp_y = -cache_bounds.y;
		if (tr_state->visual->center_coords) {
			temp_x -= cache_bounds.width/2;
			temp_y += cache_bounds.height/2;
		} else {
			temp_y += cache_bounds.height;
		}
		gf_mx2d_add_translation(&tr_state->transform, temp_x, temp_y);		
		
		/*override top clippers*/
		rc1 = tr_state->visual->surf_rect;
		rc2 = tr_state->visual->top_clipper;
		tr_state->visual->surf_rect.width = cache->txh.width;
		tr_state->visual->surf_rect.height = cache->txh.height;
		if (tr_state->visual->center_coords) {
			tr_state->visual->surf_rect.y = cache->txh.height/2;
			tr_state->visual->surf_rect.x = -1 * (s32) cache->txh.width/2;
		} else {
			tr_state->visual->surf_rect.y = cache->txh.height;
			tr_state->visual->surf_rect.x = 0;
		}
		tr_state->visual->top_clipper = tr_state->visual->surf_rect;


		/*step 5: traverse subtree in direct draw mode*/
		tr_state->direct_draw = 1;
		group_ctx->flags &= ~CTX_NO_ANTIALIAS;

		l = ((GF_ParentNode*)node)->children;
		while (l) {
			gf_node_traverse(l->node, tr_state);
			l = l->next;
		}
		/*step 6: reset all contexts after the current group one*/
		child_ctx = group_ctx->next;
		while (child_ctx && child_ctx->drawable) {
			drawable_reset_bounds(child_ctx->drawable, tr_state->visual);
			child_ctx->drawable = NULL;	
			child_ctx = child_ctx->next;
		}	

		/*and set ourselves as the last context on the main visual*/
		tr_state->visual->cur_context = group_ctx;

		/*restore state and destroy whatever needs to be cleaned*/
		gf_mx2d_copy(tr_state->transform, backup);
		tr_state->in_group_cache = 0;
		tr_state->direct_draw = prev_flags;
		r2d->surface_delete(offscreen_surface);
		tr_state->visual->raster_surface = old_surf;
		tr_state->traversing_mode = TRAVERSE_SORT;

#ifndef GPAC_DISABLE_3D
		tr_state->visual->type_3d = type_3d;
#endif
		tr_state->visual->surf_rect = rc1;
		tr_state->visual->top_clipper = rc2;
		
		/*update texture*/
		cache->txh.transparent = 1;
		gf_sc_texture_set_data(&cache->txh);
		gf_sc_texture_push_image(&cache->txh, 0, type_3d ? 0 : 1);
	}
	/*just setup the context*/
	else {
		group_ctx = drawable_init_context_mpeg4(cache->drawable, tr_state);
	}
	assert(group_ctx);
	group_ctx->flags |= CTX_NO_ANTIALIAS;
	group_ctx->aspect.fill_color = GF_COL_ARGB_FIXED(cache->opacity, FIX_ONE, FIX_ONE, FIX_ONE);
	group_ctx->aspect.fill_texture = &cache->txh;

	if (!cache->opacity) {
		group_ctx->drawable = NULL;
		return 0;
	}

	if (gf_node_dirty_get(node)) group_ctx->flags |= CTX_TEXTURE_DIRTY;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (!cache->drawable->mesh) {
			cache->drawable->mesh = new_mesh();
			mesh_from_path(cache->drawable->mesh, cache->drawable->path);
		}
		visual_3d_draw_from_context(group_ctx, tr_state);
		group_ctx->drawable = NULL;
	} else 
#endif
		drawable_finalize_sort(group_ctx, tr_state, NULL);
	
	return (force_recompute==1);
}


#ifdef MPEG4_USE_GROUP_CACHE

/**/
Bool mpeg4_group2d_cache_traverse(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state)
{
	Bool is_dirty = gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY;
	Bool needs_recompute = 0;

	/*we are currently in a group cache, regular traversing*/
	if (tr_state->in_group_cache) return 0;

	/*this is not an offscreen group*/
	if (!(group->flags & GROUP_IS_CACHED) ) {
		Bool cache_on = 0;

		if (!is_dirty && (group->flags & GROUP_IS_CACHABLE)) {
			group->flags |= GROUP_IS_CACHED;
			group->flags &= ~GROUP_IS_CACHABLE;
			fprintf(stdout, "Turning group %s cache on\n", gf_node_get_log_name(node) );
			cache_on = 1;
		} 

		if (!cache_on) {
			if (is_dirty) {
				group->changed = 1;
			} 
			/*ask for stats again*/
			else if (group->changed) {
				group->changed = 0;
				group->nb_stats_frame = 0;
				group->traverse_time = 0;
			}
			if (is_dirty || (group->nb_stats_frame < NUM_STATS_FRAMES)) {
				/*force direct draw mode*/
				if (!is_dirty) tr_state->visual->compositor->traverse_state->invalidate_all = 1;
				/*force redraw*/
				tr_state->visual->compositor->draw_next_frame = 1;
			}
			return 0;
		}
	}
	/*dynamic cache which - figure out if we need to recompute it or not*/
	else if (is_dirty && !(group->flags & GROUP_PERMANENT_CACHE) ) {
		/*cache may not be deleted immediately*/
		if (group->cache) {
			if (group->changed) {
				group_cache_del(group->cache);
				group->cache = NULL;
				group->flags &= ~GROUP_IS_CACHED;
				group->changed = 0;
				fprintf(stdout, "Turning group %s cache off\n", gf_node_get_log_name(node) );
				return 0;
			} else {
				group->changed = 1;
				group->cache->force_recompute = 1;
			}
		}
	}

	/*draw mode*/
	if (tr_state->traversing_mode == TRAVERSE_DRAW_2D) {
		/*cache SHALL BE SETUP*/
		assert(group->cache);
		/*draw it*/
		group_cache_draw(group->cache, tr_state);
		return 1;
	}
	/*other modes than sorting, use regular traversing*/
	if (tr_state->traversing_mode != TRAVERSE_SORT) return 0;

	if (!group->cache) {
		/*ALLOCATE THE CACHE*/
		group->cache = group_cache_new(tr_state->visual->compositor, node);
		needs_recompute = 1;
	}

	/*cache has been modified due to node changes, reset stats*/
	if (group_cache_traverse(node, group->cache, tr_state, needs_recompute)) {
		group->changed = 0;
		group->nb_stats_frame = 0;
		group->traverse_time = 0;
	}
	return 1;
}


void group_cache_record_stats(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state, DrawableContext *first_child, Bool skip_first_child)
{
	GF_Rect rc, gr_bounds;
	GF_Matrix2D mx;
	DrawableContext *ctx;
	u32 nb_seg_opaque, nb_seg_alpha, nb_objects, avg_time, last_frame_time, nb_obj_prev;
	Fixed alpha_pixels, opaque_pixels, area_world, area_local, ratio;

	/*first frame is unusable for stats because lot of time is being spent building the path and allocating 
	the drawable contexts*/
	if (!tr_state->visual->compositor->frame_number || group->changed) {
		group->traverse_time = 0;
		return;
	}

	if (group->nb_stats_frame < NUM_STATS_FRAMES) {
		group->nb_stats_frame++;
		tr_state->visual->compositor->draw_next_frame = 1;
		return;
	}

	/*compute stats*/
	nb_objects = 0;
	nb_seg_opaque = nb_seg_alpha = 0;
	alpha_pixels = opaque_pixels = 0;
	/*reset bounds*/
	gr_bounds.width = gr_bounds.height = 0;

	ctx = first_child;
	if (!first_child) ctx = tr_state->visual->context;
	if (skip_first_child) ctx = ctx->next;
	/*compute properties for the sub display list*/
	while (ctx && ctx->drawable) {
		Fixed area;
		u32 alpha_comp;

		/*add to group area*/
		gf_rect_union(&gr_bounds, &ctx->bi->unclip);
		nb_objects++;
		
		/*get area and compute alpha/opaque coverage*/
		area = ctx->bi->unclip.width * ctx->bi->unclip.height;
		alpha_comp = GF_COL_A(ctx->aspect.fill_color);

		/*no alpha*/
		if ((alpha_comp==0xFF) 
			/*no transparent texture*/
			&& (!ctx->aspect.fill_texture || !ctx->aspect.fill_texture->transparent)
		) {
			Fixed coverage;
			/*transparent context: ellipse or any path - compute coverage*/
			if (ctx->flags & CTX_IS_TRANSPARENT) {
				/*FIXME: we need to get a better fast approximation of the covered area of the path
				for now, we only estimate to 75%*/
				coverage = INT2FIX(3) / 4;
			}
			/*non-transparent context: rectangle or bitmap*/
			else {
				coverage = FIX_ONE;
			}
			opaque_pixels += gf_mulfix(area, coverage);
			nb_seg_opaque += ctx->drawable->path->n_points;
		} else if (alpha_comp) {
			alpha_pixels += area;
			nb_seg_alpha += ctx->drawable->path->n_points;
		} else {
			/*don't count stroke areas, this will automatically lower the group coverage and we will
			not cache groups with mainly strokes*/
		}
		ctx = ctx->next;
	}
	/*visually empty group*/
	if (!gr_bounds.width || !gr_bounds.height) return;

	/*and recompute for local coordinate system*/
	gf_mx2d_copy(mx, tr_state->transform);
	gf_mx2d_inverse(&mx);
	rc = gr_bounds;
	gf_mx2d_apply_rect(&mx, &rc);
	area_world = gr_bounds.width * gr_bounds.height;
	area_local = rc.width * rc.height;

	ratio = gf_divfix(area_local, area_world);
	alpha_pixels = gf_mulfix(alpha_pixels, ratio);
	opaque_pixels = gf_mulfix(opaque_pixels, ratio);

#if 0
	fprintf(stdout, "Local stats for group %s at frame %d:\n"
					"\tNb of Modifications %d\n"
					"\tNb of Objects %d\n"
					"\tAverage Traverse Time %g\n"
					"\tPixel Area: Local %g (World %g)\n"
					"\tOpaque Pixels %d (%g %%) - %d segments\n"
					"\tAlpha Pixels %d (%g %%) - %d segments\n"
					"\tTotal Pixels %d (%g %%) - %d segments\n"
					"\n"
		
			, gf_node_get_log_name(node)
			, tr_state->visual->compositor->frame_number
			, group->nb_changes
			, nb_objects
			, INT2FIX(group->traverse_time) / tr_state->visual->compositor->frame_number
			, area_local, area_world
			, FIX2INT(opaque_pixels), gf_divfix(100*opaque_pixels, area_local), nb_seg_opaque
			, FIX2INT(alpha_pixels), gf_divfix(100*alpha_pixels, area_local), nb_seg_alpha
			, FIX2INT(opaque_pixels+alpha_pixels), gf_divfix(100*(opaque_pixels+alpha_pixels), area_local), nb_seg_alpha+nb_seg_opaque
	);
#endif

	last_frame_time = tr_state->visual->compositor->frame_time[tr_state->visual->compositor->current_frame];
	avg_time = group->traverse_time;
	avg_time /= group->nb_stats_frame;
	nb_obj_prev = tr_state->visual->num_nodes_prev_frame;


	if (/*small groups*/
		(nb_objects==1)
	/*low complexity group*/
	|| (nb_seg_alpha+nb_seg_opaque<10)
	/*low coverage groups (plenty of space wasted*/
	|| (opaque_pixels+alpha_pixels < 60*area_local/100)
	/*group doesn't take that long to render*/
//	|| (avg_time < last_frame_time/10)
	/*never cache root node!!*/
	|| (gf_node_get_parent(node, 0) == NULL)
	/*... whatever criteria desired ...*/
	) {
		group->flags &= ~GROUP_IS_CACHABLE;

		if (group->cache) {
			group_cache_del(group->cache);
			group->cache = NULL;
			group->flags &= ~GROUP_IS_CACHED;
		}
	} else {
		group->flags |= GROUP_IS_CACHABLE;
		tr_state->visual->compositor->draw_next_frame = 1;
	}
}


#endif /*MPEG4_USE_GROUP_CACHE*/
