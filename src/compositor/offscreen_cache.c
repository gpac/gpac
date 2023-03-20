/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2023
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

#if !defined(GPAC_DISABLE_COMPOSITOR)

#define NUM_STATS_FRAMES		2
#define MIN_OBJECTS_IN_CACHE	2


//#define CACHE_DEBUG_ALPHA
//#define CACHE_DEBUG_CENTER

void group_cache_draw(GroupCache *cache, GF_TraverseState *tr_state)
{
	GF_TextureHandler *old_txh = tr_state->ctx->aspect.fill_texture;
	/*switch the texture to our offscreen cache*/
	tr_state->ctx->aspect.fill_texture = &cache->txh;


#if !defined( GPAC_DISABLE_3D) && !defined(GPAC_DISABLE_VRML)
	if (tr_state->traversing_mode == TRAVERSE_DRAW_3D) {
		if (!cache->drawable->mesh) {
			cache->drawable->mesh = new_mesh();
		}
		mesh_from_path(cache->drawable->mesh, cache->drawable->path);
		visual_3d_draw_2d_with_aspect(cache->drawable, tr_state, &tr_state->ctx->aspect);
		return;
	}
#endif

	if (! tr_state->visual->DrawBitmap(tr_state->visual, tr_state, tr_state->ctx)) {
		visual_2d_texture_path(tr_state->visual, cache->drawable->path, tr_state->ctx, tr_state);
	}
	tr_state->ctx->aspect.fill_texture = old_txh;
}

GroupCache *group_cache_new(GF_Compositor *compositor, GF_Node *node)
{
	GroupCache *cache;
	GF_SAFEALLOC(cache, GroupCache);
	if (!cache) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate group cache\n"));
		return NULL;
	}
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
	drawable_del(cache->drawable);
	if (cache->txh.data) gf_free(cache->txh.data);
	gf_sc_texture_release(&cache->txh);
	gf_sc_texture_destroy(&cache->txh);
	gf_free(cache);
}

void group_cache_setup(GroupCache *cache, GF_Rect *path_bounds, GF_IRect *pix_bounds, GF_Compositor *compositor, Bool for_gl)
{
	/*setup texture */
	cache->txh.compositor = compositor;
	cache->txh.height = pix_bounds->height;
	cache->txh.width = pix_bounds->width;

	cache->txh.stride = pix_bounds->width * 4;
	cache->txh.pixelformat = for_gl ? GF_PIXEL_RGBA : GF_PIXEL_ARGB;
	cache->txh.transparent = 1;

	if (cache->txh.data)
		gf_free(cache->txh.data);
#ifdef CACHE_DEBUG_ALPHA
	cache->txh.stride = pix_bounds->width * 3;
	cache->txh.pixelformat = GF_PIXEL_RGB;
	cache->txh.transparent = 0;
#endif

	cache->txh.data = (char *) gf_malloc (sizeof(char) * cache->txh.stride * cache->txh.height);
	memset(cache->txh.data, 0x0, sizeof(char) * cache->txh.stride * cache->txh.height);
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

Bool group_cache_traverse(GF_Node *node, GroupCache *cache, GF_TraverseState *tr_state, Bool force_recompute, Bool is_mpeg4, Bool auto_fit_vp)
{
	GF_Matrix2D backup;
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
		GF_IRect rc1, rc2;
		u32 prev_flags;
		Bool prev_hybgl, visual_attached, for_3d=GF_FALSE;
		GF_Rect cache_bounds;
		GF_EVGSurface *offscreen_surface, *old_surf;
		DrawableContext *child_ctx;
		Fixed temp_x, temp_y, scale_x, scale_y;
#ifndef GPAC_DISABLE_3D
		u32 type_3d;
		GF_Matrix2D transf;
#endif

		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Recomputing cache for subtree %s\n", gf_node_get_log_name(node)));
		/*step 1 : store current state and indicate children should not be cached*/
		tr_state->in_group_cache = 1;
		prev_flags = tr_state->immediate_draw;
		/*store the current transform matrix, create a new one for group_cache*/
		gf_mx2d_copy(backup, tr_state->transform);
		gf_mx2d_init(tr_state->transform);

#ifndef GPAC_DISABLE_3D
		/*force 2D rendering*/
		type_3d = tr_state->visual->type_3d;
		tr_state->visual->type_3d = 0;
		if (type_3d || tr_state->visual->compositor->hybrid_opengl)
			for_3d = GF_TRUE;
#endif
		prev_hybgl = tr_state->visual->compositor->hybrid_opengl;
		tr_state->visual->compositor->hybrid_opengl = GF_FALSE;

		/*step 2: collect the bounds of all children*/
		tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
		cache_bounds.width = cache_bounds.height = 0;
		l = ((GF_ParentNode*)node)->children;
		while (l) {
			tr_state->bounds.width = tr_state->bounds.height = 0;
			gf_node_traverse(l->node, tr_state);
			l = l->next;
			gf_rect_union(&cache_bounds, &tr_state->bounds);
		}
		tr_state->traversing_mode = TRAVERSE_SORT;

		if (!cache_bounds.width || !cache_bounds.height) {
			tr_state->in_group_cache = 0;
			tr_state->immediate_draw = prev_flags;
			gf_mx2d_copy(tr_state->transform, backup);
#ifndef GPAC_DISABLE_3D
			tr_state->visual->type_3d = type_3d;
#endif
			tr_state->visual->compositor->hybrid_opengl = prev_hybgl;
			return 0;
		}

		/*step 3: insert a DrawableContext for this group in the display list*/
		if (is_mpeg4) {
#ifndef GPAC_DISABLE_VRML
			group_ctx = drawable_init_context_mpeg4(cache->drawable, tr_state);
#endif
		} else {
#ifndef GPAC_DISABLE_SVG
			group_ctx = drawable_init_context_svg(cache->drawable, tr_state, NULL);
#endif
		}
		if (!group_ctx) return 0;
		
		/*step 4: now we have the bounds:
			allocate the offscreen memory
			create temp raster visual & attach to buffer
			override the tr_state->visual->the_surface with the temp raster
			add translation (shape is not always centered)
			setup top clipers
		*/
		old_surf = tr_state->visual->raster_surface;
		offscreen_surface = gf_evg_surface_new(tr_state->visual->center_coords);	/*a new temp raster visual*/
		tr_state->visual->raster_surface = offscreen_surface;
#ifndef GPAC_DISABLE_3D
		if (type_3d) {
			gf_mx2d_from_mx(&transf, &tr_state->model_matrix);
			scale_x = transf.m[0];
			scale_y = transf.m[4];
		} else
#endif
		{
			scale_x = backup.m[0];
			scale_y = backup.m[4];
		}

		/*use current surface coordinate scaling to compute the cache*/
#ifdef GF_SR_USE_VIDEO_CACHE
		scale_x = tr_state->visual->compositor->vcscale * scale_x / 100;
		scale_y = tr_state->visual->compositor->vcscale * scale_y / 100;
#endif

		if (scale_x<0) scale_x = -scale_x;
		if (scale_y<0) scale_y = -scale_y;

		cache->scale = MAX(scale_x, scale_y);
		tr_state->bounds = cache_bounds;

		gf_mx2d_add_scale(&tr_state->transform, scale_x, scale_y);
		gf_mx2d_apply_rect(&tr_state->transform, &cache_bounds);

		rc1 = gf_rect_pixelize(&cache_bounds);
		if (rc1.width % 2) rc1.width++;
		if (rc1.height%2) rc1.height++;
		
		//TODO - set min offscreen size in cfg file
		while (rc1.width && rc1.width<128) rc1.width *= 2;
		while (rc1.height && rc1.height<128) rc1.height *= 2;

		/* Initialize the group cache with the scaled pixelized bounds for texture but the original bounds for path*/
		group_cache_setup(cache, &tr_state->bounds, &rc1, tr_state->visual->compositor, for_3d);


		/*attach the buffer to visual*/
		gf_evg_surface_attach_to_buffer(offscreen_surface, cache->txh.data,
		                              cache->txh.width,
		                              cache->txh.height,
		                              0,
		                              cache->txh.stride,
		                              cache->txh.pixelformat);

		visual_attached = tr_state->visual->is_attached;
		tr_state->visual->is_attached = 1;

		/*recompute the bounds with the final scaling used*/
		scale_x = gf_divfix(INT2FIX(rc1.width), tr_state->bounds.width);
		scale_y = gf_divfix(INT2FIX(rc1.height), tr_state->bounds.height);
		gf_mx2d_init(tr_state->transform);
		gf_mx2d_add_scale(&tr_state->transform, scale_x, scale_y);
		cache_bounds = tr_state->bounds;
		gf_mx2d_apply_rect(&tr_state->transform, &cache_bounds);

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
		tr_state->immediate_draw = 1;
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
		tr_state->immediate_draw = prev_flags;
		tr_state->visual->compositor->hybrid_opengl = prev_hybgl;
		tr_state->visual->is_attached = visual_attached;

		gf_evg_surface_delete(offscreen_surface);
		tr_state->visual->raster_surface = old_surf;
		tr_state->traversing_mode = TRAVERSE_SORT;

#ifndef GPAC_DISABLE_3D
		tr_state->visual->type_3d = type_3d;
#endif
		tr_state->visual->surf_rect = rc1;
		tr_state->visual->top_clipper = rc2;

		/*update texture*/
		cache->txh.transparent = 1;
		if (tr_state->visual->center_coords)
			cache->txh.flags |= GF_SR_TEXTURE_NO_GL_FLIP;
		
		gf_sc_texture_set_data(&cache->txh);
		gf_sc_texture_push_image(&cache->txh, 0, for_3d ? 0 : 1);

		cache->orig_vp = tr_state->vp_size;
	}
	/*just setup the context*/
	else {
		if (is_mpeg4) {
#ifndef GPAC_DISABLE_VRML
			group_ctx = drawable_init_context_mpeg4(cache->drawable, tr_state);
#endif
		} else {
#ifndef GPAC_DISABLE_SVG
			group_ctx = drawable_init_context_svg(cache->drawable, tr_state, NULL);
#endif
		}
	}
	if (!group_ctx) return 0;
	group_ctx->flags |= CTX_NO_ANTIALIAS;
	if (cache->opacity != FIX_ONE)
		group_ctx->aspect.fill_color = GF_COL_ARGB_FIXED(cache->opacity, FIX_ONE, FIX_ONE, FIX_ONE);
	else
		group_ctx->aspect.fill_color = 0;
	group_ctx->aspect.fill_texture = &cache->txh;

	if (!cache->opacity) {
		group_ctx->drawable = NULL;
		return 0;
	}

	drawable_check_texture_dirty(group_ctx, group_ctx->drawable, tr_state);

	if (gf_node_dirty_get(node)) group_ctx->flags |= CTX_TEXTURE_DIRTY;

#ifdef CACHE_DEBUG_CENTER
	gf_mx2d_copy(backup, tr_state->transform);
	gf_mx2d_init(tr_state->transform);
#else
	gf_mx2d_copy(backup, tr_state->transform);
	if (auto_fit_vp) {
		if ((tr_state->vp_size.x != cache->orig_vp.x) || (tr_state->vp_size.y != cache->orig_vp.y)) {
			GF_Matrix2D m;
			gf_mx2d_init(m);
			gf_mx2d_copy(backup, tr_state->transform);
			gf_mx2d_add_scale(&m, gf_divfix(tr_state->vp_size.x, cache->orig_vp.x), gf_divfix(tr_state->vp_size.y, cache->orig_vp.y) );
			gf_mx2d_pre_multiply(&tr_state->transform, &m);
		} else {
			auto_fit_vp = 0;
		}
	}
#endif

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

#ifndef CACHE_DEBUG_CENTER
	if (auto_fit_vp)
#endif
	{
		gf_mx2d_copy(tr_state->transform, backup);
	}
	return (force_recompute==1);
}


#ifdef GF_SR_USE_VIDEO_CACHE

/*guarentee the tr_state->candidate has the lowest delta value*/
static void group_cache_insert_entry(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state)
{
	u32 i, count;
	GF_List *cache_candidates = tr_state->visual->compositor->cached_groups;
	GroupingNode2D *current;

	current = NULL;
	count = gf_list_count(cache_candidates);
	for (i=0; i<count; i++) {
		current = gf_list_get(cache_candidates, i);
		/*if entry's priority is higher than our group, insert our group here*/
		if (current->priority >= group->priority) {
			gf_list_insert(cache_candidates, group, i);
			break;
		}
	}
	if (i==count)
		gf_list_add(cache_candidates, group);

	tr_state->visual->compositor->video_cache_current_size += group->cached_size;
	/*log the information*/
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE]\tAdding object %s\tObjects: %d\tSlope: %g\tSize: %d\tTime: %d\n",
	                                    gf_node_get_log_name(node),
	                                    group->nb_objects,
	                                    FIX2FLT(group->priority),
	                                    group->cached_size,
	                                    group->traverse_time));

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Status (KB): Max: %d\tUsed: %d\tNb Groups: %d\n",
	                                    tr_state->visual->compositor->vcsize,
	                                    tr_state->visual->compositor->video_cache_current_size,
	                                    gf_list_count(tr_state->visual->compositor->cached_groups)
	                                   ));
}


static Bool gf_cache_remove_entry(GF_Compositor *compositor, GF_Node *node, GroupingNode2D *group)
{
	u32 bytes_remove = 0;
	GF_List *cache_candidates = compositor->cached_groups;

	/*auto mode*/
	if (!group) {
		group = gf_list_get(cache_candidates, 0);
		if (!group) return 0;
		/*remove entry*/
		gf_list_rem(cache_candidates, 0);
		node = NULL;
	} else {
		/*remove entry if present*/
		if (gf_list_del_item(cache_candidates, group)<0)
			return 0;
	}

	/*disable the caching flag of the group if it was marked as such*/
	if(group->flags & GROUP_IS_CACHABLE) {
		group->flags &= ~GROUP_IS_CACHABLE;
		/*the discarded bytes*/
		bytes_remove = group->cached_size;
	}

	/*indicates cache destruction for next frame*/
	if (group->cache && (group->flags & GROUP_IS_CACHED)) {
		group->flags &= ~GROUP_IS_CACHED;
		/*the discarded bytes*/
		bytes_remove = group->cached_size;
	}

	if (bytes_remove == 0) return 0;

	assert(compositor->video_cache_current_size >= bytes_remove);
	compositor->video_cache_current_size -= bytes_remove;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Removing cache %s:\t Objects: %d\tSlope: %g\tBytes: %d\tTime: %d\n",
	                                    gf_node_get_log_name(node),
	                                    group->nb_objects,
	                                    FIX2FLT(group->priority),
	                                    group->cached_size,
	                                    FIX2FLT(group->traverse_time)));

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Status (B): Max: %d\tUsed: %d\tNb Groups: %d\n",
	                                    compositor->vcsize,
	                                    compositor->video_cache_current_size,
	                                    gf_list_count(compositor->cached_groups)
	                                   ));
	return 1;
}


/**/
Bool group_2d_cache_traverse(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state)
{
	Bool is_dirty = gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY;
	Bool zoom_changed = tr_state->visual->compositor->zoom_changed;
	Bool needs_recompute = 0;

	/*we are currently in a group cache, regular traversing*/
	if (tr_state->in_group_cache) return 0;

	/*draw mode*/
	if (tr_state->traversing_mode == TRAVERSE_DRAW_2D) {
		/*shall never happen*/
		assert(group->cache);
		/*draw it*/
		group_cache_draw(group->cache, tr_state);
		return 1;
	}
	/*other modes than sorting, use regular traversing*/
	if (tr_state->traversing_mode != TRAVERSE_SORT) return 0;

	/*this is not an offscreen group*/
	if (!(group->flags & GROUP_IS_CACHED) ) {
		Bool cache_on = 0;

		/*group cache has been turned on in the previous frame*/
		if (!is_dirty && (group->flags & GROUP_IS_CACHABLE)) {
			group->flags |= GROUP_IS_CACHED;
			group->flags &= ~GROUP_IS_CACHABLE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Turning group %s cache on - size %d\n", gf_node_get_log_name(node), group->cached_size ));
			cache_on = 1;
		}
		/*group cache has been turned off in the previous frame*/
		else if (group->cache) {
			group_cache_del(group->cache);
			group->cache = NULL;
			group->changed = is_dirty;
			group->nb_stats_frame = 0;
			group->traverse_time = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Turning group %s cache off\n", gf_node_get_log_name(node) ));
			return 0;
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
			} else if (zoom_changed) {
				group->nb_stats_frame = 0;
				group->traverse_time = 0;
			}
			if (is_dirty || (group->nb_stats_frame < NUM_STATS_FRAMES)) {
				/*force direct draw mode*/
				if (!is_dirty)
					tr_state->visual->compositor->traverse_state->invalidate_all = 1;
				/*force redraw*/
				tr_state->visual->compositor->draw_next_frame = 1;
			}
			return 0;
		}
	}
	/*cache is dirty*/
	else if (is_dirty) {
		/*permanent cache, just recompute*/
		if (group->flags & GROUP_PERMANENT_CACHE) {
			group->changed = 1;
			group->cache->force_recompute = 1;
		}
		/*otherwise destroy the cache*/
		else if (group->cache) {
			gf_cache_remove_entry(tr_state->visual->compositor, node, group);
			group_cache_del(group->cache);
			group->cache = NULL;
			group->flags &= ~GROUP_IS_CACHED;
			group->changed = 0;
			group->nb_stats_frame = 0;
			group->traverse_time = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Turning group %s cache off due to sub-tree modifications\n", gf_node_get_log_name(node) ));
			return 0;
		}
	}
	/*zoom has changed*/
	else if (zoom_changed) {
		/*permanent cache, just recompute*/
		if (group->flags & GROUP_PERMANENT_CACHE) {
			group->changed = 1;
			group->cache->force_recompute = 1;
		}
		/*otherwise check if we accept this scale ratio or if we must recompute*/
		else if (group->cache) {
			Fixed scale = MAX(tr_state->transform.m[0], tr_state->transform.m[4]);

			if (100*scale >= group->cache->scale*(100 + tr_state->visual->compositor->vctol))
				zoom_changed = 1;
			else if ((100+tr_state->visual->compositor->vctol)*scale <= 100*group->cache->scale)
				zoom_changed = 1;
			else
				zoom_changed = 0;

			if (zoom_changed) {
				gf_cache_remove_entry(tr_state->visual->compositor, node, group);
				group_cache_del(group->cache);
				group->cache = NULL;
				group->flags &= ~GROUP_IS_CACHED;
				group->changed = 0;
				group->nb_stats_frame = 0;
				group->traverse_time = 0;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Turning group %s cache off due to zoom changes\n", gf_node_get_log_name(node) ));
				return 0;
			}
		}
	}

	/*keep track of this cache object for later removal*/
	if (!(group->flags & GROUP_PERMANENT_CACHE))
		gf_list_add(tr_state->visual->compositor->cached_groups_queue, group);

	if (!group->cache) {
		/*ALLOCATE THE CACHE*/
		group->cache = group_cache_new(tr_state->visual->compositor, node);
		needs_recompute = 1;
	}

	/*cache has been modified due to node changes, reset stats*/
	group_cache_traverse(node, group->cache, tr_state, needs_recompute, 1, 0);
	return 1;
}


Bool group_cache_compute_stats(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state, DrawableContext *first_child, Bool skip_first_child)
{
	GF_Rect group_bounds;
	DrawableContext *ctx;
	u32 nb_segments, nb_objects;
	u32 alpha_pixels, opaque_pixels, area_world;
	u32 vcsize, cache_size, prev_cache_size;
	u32 i;
	GF_RectArray ra;

	/*compute stats*/
	nb_objects = 0;
	nb_segments = 0;
	alpha_pixels = opaque_pixels = 0;
	prev_cache_size = group->cached_size;
	/*reset bounds*/
	group_bounds.width = group_bounds.height = 0;
	vcsize = tr_state->visual->compositor->vcsize;

	/*never cache root node - this should be refined*/
	if (gf_node_get_parent(node, 0) == NULL) goto group_reject;
	if (!group->traverse_time) goto group_reject;

	ra_init(&ra);

	ctx = first_child;
	if (!first_child) ctx = tr_state->visual->context;
	if (skip_first_child) ctx = ctx->next;
	/*compute properties for the sub display list*/
	while (ctx && ctx->drawable) {
		//Fixed area;
		u32 alpha_comp;

		/*get area and compute alpha/opaque coverage*/
		alpha_comp = GF_COL_A(ctx->aspect.fill_color);

		/*add to group area*/
		gf_rect_union(&group_bounds, &ctx->bi->unclip);
		nb_objects++;

		/*no alpha*/
		if ((alpha_comp==0xFF)
		        /*no transparent texture*/
		        && (!ctx->aspect.fill_texture || !ctx->aspect.fill_texture->transparent)
		   ) {

			ra_union_rect(&ra, &ctx->bi->clip);
		}
		nb_segments += ctx->drawable->path->n_points;

		ctx = ctx->next;
	}

	if (
	    /*TEST 1: discard visually empty groups*/
	    (!group_bounds.width || !group_bounds.height)
	    ||
	    /*TEST 2: discard small groups*/
	    (nb_objects<MIN_OBJECTS_IN_CACHE)
	    ||
	    /*TEST 3: low complexity group*/
	    (nb_segments && (nb_segments<10))
	) {
		ra_del(&ra);
		goto group_reject;
	}

	ra_refresh(&ra);
	opaque_pixels = 0;
	for (i=0; i<ra.count; i++) {
		opaque_pixels += ra.list[i].width * ra.list[i].height;
	}
	ra_del(&ra);

	/*get coverage in world coords*/
	area_world = FIX2INT(group_bounds.width) * FIX2INT(group_bounds.height);

	/*TEST 4: discard low coverage groups in world coords (plenty of space wasted)
		we consider that this % of the area is actually drawn - this is of course wrong,
		we would need to compute each path coverage in local coords then get the ratio
	*/
	if (10*opaque_pixels < 7*area_world) goto group_reject;

	/*the memory size allocated for the cache - cache is drawn in final coordinate system !!*/
	group_bounds.width = tr_state->visual->compositor->vcscale * group_bounds.width / 100;
	group_bounds.height = tr_state->visual->compositor->vcscale * group_bounds.height / 100;
	cache_size = FIX2INT(group_bounds.width) * FIX2INT(group_bounds.height) * 4 /* pixelFormat is ARGB*/;

	/*TEST 5: cache is less than 10x10 pixels: discard*/
	if (cache_size < 400) goto group_reject;
	/*TEST 6: cache is larger than our allowed memory: discard*/
	if (cache_size>=vcsize) {
		tr_state->cache_too_small = 1;
		goto group_reject;
	}

	/*compute the delta value for measuring the group importance for later discard
		(avg_time - Tcache) / (size_cache - drawable_gain)
	*/
	group->priority = INT2FIX(nb_objects*1024*group->traverse_time) / cache_size / group->nb_stats_frame;
	/*OK, group is a good candidate for caching*/
	group->nb_objects = nb_objects;
	group->cached_size = cache_size;


	/*we're moving from non-cached to cached*/
	if (!(group->flags & GROUP_IS_CACHABLE)) {
		group->flags |= GROUP_IS_CACHABLE;
		tr_state->visual->compositor->draw_next_frame = 1;

		/*insert the candidate and then update the list in order*/
		group_cache_insert_entry(node, group, tr_state);
		/*keep track of this cache object for later removal*/
		gf_list_add(tr_state->visual->compositor->cached_groups_queue, group);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Turning cache on during stat pass for node %s - %d kb used in all caches\n", gf_node_get_log_name(node), tr_state->visual->compositor->video_cache_current_size ));
	}
	/*update memory occupation*/
	else {
		tr_state->visual->compositor->video_cache_current_size -= prev_cache_size;
		tr_state->visual->compositor->video_cache_current_size += group->cached_size;

		if (group->cache)
			group->cache->force_recompute = 1;
	}
	return 1;


group_reject:
	group->nb_objects = nb_objects;

	if ((group->flags & GROUP_IS_CACHABLE) || group->cache) {
		group->flags &= ~GROUP_IS_CACHABLE;

		if (group->cache) {
			group_cache_del(group->cache);
			group->cache = NULL;
			group->flags &= ~GROUP_IS_CACHED;
		}
		gf_list_del_item(tr_state->visual->compositor->cached_groups, group);
		tr_state->visual->compositor->video_cache_current_size -= cache_size;
	}

#if 0
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] REJECT %s\tObjects: %d\tSlope: %g\tBytes: %d\tTime: %d\n",
	                                    gf_node_get_log_name(node),
	                                    group->nb_objects,
	                                    FIX2FLT(group->priority),
	                                    group->cached_size,
	                                    group->traverse_time
	                                   ));

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Status (B): Max: %d\tUsed: %d\tNb Groups: %d\n",
	                                    tr_state->visual->compositor->vcsize,
	                                    tr_state->visual->compositor->video_cache_current_size,
	                                    gf_list_count(tr_state->visual->compositor->cached_groups)
	                                   ));
#endif
	return 0;
}


void group_2d_cache_evaluate(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state, DrawableContext *first_child, Bool skip_first_child, u32 last_cache_idx)
{
	u32 nb_cache_added, i;
	GF_Compositor *compositor = tr_state->visual->compositor;

	/*first frame is unusable for stats because lot of time is being spent building the path and allocating
	the drawable contexts*/
	if (!compositor->vcsize || !compositor->frame_number || group->changed || tr_state->in_group_cache) {
		group->traverse_time = 0;
		return;
	}

	if (group->nb_stats_frame < NUM_STATS_FRAMES) {
		group->nb_stats_frame++;
		tr_state->visual->compositor->draw_next_frame = 1;
		return;
	}
	if (group->nb_stats_frame > NUM_STATS_FRAMES) return;
	group->nb_stats_frame++;

	/*the way to produce the result of memory-computation optimization*/
	if (group_cache_compute_stats(node, group, tr_state, first_child, skip_first_child)) {
		Fixed avg_time;
		nb_cache_added = gf_list_count(compositor->cached_groups_queue) - last_cache_idx - 1;

		/*force redraw*/
		tr_state->visual->compositor->draw_next_frame = 1;

		/*update priority by adding cache priorities */
		avg_time = group->priority * group->cached_size / (1024*group->nb_objects);

		/*remove all queued cached groups of this node's children*/
		for (i=0; i<nb_cache_added; i++) {
			Fixed cache_time;
			GroupingNode2D *cache = gf_list_get(compositor->cached_groups_queue, last_cache_idx);
			/*we have been computed the prioirity of the group using a cached subtree, update
			the priority to reflect that the new cache won't use a cached subtree*/
			if (cache->cache) {
				/*fixme - this assumes cache draw time is 0*/
				cache_time = cache->priority * cache->cached_size / (1024*group->nb_objects);
				avg_time += cache_time;
			}
			gf_cache_remove_entry(compositor, NULL, cache);
			cache->nb_stats_frame = 0;
			cache->traverse_time = 0;
			gf_list_rem(compositor->cached_groups_queue, last_cache_idx);
		}
		group->priority = INT2FIX (group->nb_objects*1024*1024*avg_time) / group->cached_size;

		/*when the memory exceeds the constraint, remove the subgroups that have the lowest deltas*/
		while (compositor->video_cache_current_size > compositor->vcsize)	{
			gf_cache_remove_entry(compositor, node, NULL);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CACHE, ("[CACHE] Removing low priority cache - current total size %d\n", compositor->video_cache_current_size));
		}
	}
}

void compositor_set_cache_memory(GF_Compositor *compositor, u32 memory)
{
	/*when the memory exceeds the constraint, remove the subgroups that have the lowest deltas*/
	while (compositor->video_cache_current_size) {
		gf_cache_remove_entry(compositor, NULL, NULL);
	}
	compositor->vcsize = memory;
	/*and force recompute*/
	compositor->zoom_changed = 1;
}

#endif /*GF_SR_USE_VIDEO_CACHE*/

void group_2d_destroy_svg(GF_Node *node, GroupingNode2D *group)
{
#ifdef GF_SR_USE_VIDEO_CACHE
	GF_Compositor *compositor = gf_sc_get_compositor(node);
	if (gf_cache_remove_entry(compositor, node, group)) {
		/*simulate a zoom changed for cache recompute*/
		compositor->zoom_changed = 1;
		compositor->draw_next_frame = 1;
	}
	if (group->cache) group_cache_del(group->cache);
#endif
}

void group_2d_destroy(GF_Node *node, GroupingNode2D *group)
{
	group_2d_destroy_svg(node, group);
	if (group->sensors) gf_list_del(group->sensors);
}

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
