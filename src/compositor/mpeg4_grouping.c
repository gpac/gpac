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


#include "nodes_stacks.h"
#include "mpeg4_grouping.h"
#include "visual_manager.h"

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

/*This is the generic routine for child traversing*/
void group_2d_traverse(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state)
{
	u32 backup;
#ifdef GF_SR_USE_VIDEO_CACHE
	Bool group_cached;
#endif
	GF_List *sensor_backup;
	GF_ChildNodeItem *child;

	backup = gf_node_dirty_get(node);
	if (backup & GF_SG_CHILD_DIRTY) {
		GF_SensorHandler *hsens;
		Bool check_anchor=0;
		u32 ntag = gf_node_get_tag(node);
		group->flags &= ~GROUP_HAS_SENSORS;
		if (group->sensors) gf_list_reset(group->sensors);

		drawable_reset_group_highlight(tr_state, node);

		/*never performs bounds recompute on the fly in 2D since we don't cull 2D groups
		but still mark the group as empty*/
		group->bounds.width = 0;
		/*special case for anchor which is a parent node acting as a sensor*/
		if (ntag==TAG_MPEG4_Anchor) check_anchor=1;
#ifndef GPAC_DISABLE_X3D
		else if (ntag==TAG_X3D_Anchor) check_anchor=1;
#endif
		if (check_anchor) {
			GF_SensorHandler *gf_sc_anchor_get_handler(GF_Node *n);

			hsens = gf_sc_anchor_get_handler(node);
			if (hsens) {
				if (!group->sensors) group->sensors = gf_list_new();
				gf_list_add(group->sensors, hsens);
				group->flags |= GROUP_HAS_SENSORS | GROUP_IS_ANCHOR;
			}
		} else {
			child = ((GF_ParentNode *)node)->children;
			while (child) {
				hsens = compositor_mpeg4_get_sensor_handler_ex(child->node, GF_TRUE);
				if (hsens) {
					if (!group->sensors) group->sensors = gf_list_new();
					gf_list_add(group->sensors, hsens);
					group->flags |= GROUP_HAS_SENSORS;
				}
				child = child->next;
			}
		}
	}
	/*sub-tree not dirty and getting bounds, direct copy */
	else if ((tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) && !tr_state->for_node && group->bounds.width) {
		tr_state->bounds = group->bounds;
		return;
	}


#ifdef GF_SR_USE_VIDEO_CACHE
	group_cached = group_2d_cache_traverse(node, group, tr_state);
#endif

	/*now here is the trick: ExternProtos may not be loaded at this point, in which case we can't
	perform proper culling. Unloaded ExternProto signal themselves by invalidating their parent
	graph to get a new traversal. We must therefore reset the CHILD_DIRTY flag before computing
	bounds otherwise we'll never re-invalidate the subgraph anymore*/
	gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);


#ifdef GF_SR_USE_VIDEO_CACHE
	if (group_cached) return;
#endif

	/*no culling in 2d*/

	/*picking: collect sensors*/
	sensor_backup = NULL;
	if ((tr_state->traversing_mode==TRAVERSE_PICK) && (group->flags & GROUP_HAS_SENSORS) ) {
		/*reset sensor stack if any sensors at this level*/
		sensor_backup = tr_state->vrml_sensors;
		assert(group->sensors);
		tr_state->vrml_sensors = group->sensors;
	}


	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		child = ((GF_ParentNode *)node)->children;
		backup = tr_state->text_split_mode;
		if (tr_state->text_split_mode && (gf_node_list_get_count(child)>1) ) tr_state->text_split_mode = 0;
		group->flags &= ~GROUP_SKIP_CULLING;
		group->bounds.width = group->bounds.height = 0;
		tr_state->bounds.width = tr_state->bounds.height = 0;
#ifndef GPAC_DISABLE_3D
		tr_state->bbox.is_set = 0;
#endif
		while (child) {
			gf_node_traverse(child->node, tr_state);
			if (tr_state->disable_cull) {
				group->flags |= GROUP_SKIP_CULLING;
				tr_state->disable_cull = 0;
			}
			/*handle 3D nodes in 2D groups*/
#ifndef GPAC_DISABLE_3D
			if (tr_state->bbox.is_set) {
				gf_rect_from_bbox(&tr_state->bounds, &tr_state->bbox);
				tr_state->bbox.is_set = 0;
			}
#endif
			gf_rect_union(&group->bounds, &tr_state->bounds);
			tr_state->bounds.width = tr_state->bounds.height = 0;
			child = child->next;
		}

		tr_state->bounds = group->bounds;

		if (group->flags & GROUP_SKIP_CULLING)
			tr_state->disable_cull = 1;
		tr_state->text_split_mode = backup;
	}
	/*TRAVERSE_SORT */
	else if (tr_state->traversing_mode==TRAVERSE_SORT) {
		Bool prev_inv = tr_state->invalidate_all;
#ifdef GF_SR_USE_VIDEO_CACHE
		DrawableContext *first_ctx = tr_state->visual->cur_context;
		Bool skip_first_ctx = (first_ctx && first_ctx->drawable) ? 1 : 0;
		u32 cache_too_small = 0;
		u32 traverse_time = gf_sys_clock();
		u32 last_cache_idx = gf_list_count(tr_state->visual->compositor->cached_groups_queue);
		tr_state->cache_too_small = 0;
#endif

		if (backup & GF_SG_VRML_COLOR_DIRTY) {
			tr_state->invalidate_all = 1;
			gf_node_dirty_clear(node, GF_SG_VRML_COLOR_DIRTY);
		}

		child = ((GF_ParentNode *)node)->children;
		while (child) {
			gf_node_traverse(child->node, tr_state);
			child = child->next;
#ifdef GF_SR_USE_VIDEO_CACHE
			if (tr_state->cache_too_small)
				cache_too_small++;
#endif
		}

		tr_state->invalidate_all = prev_inv;

#ifdef GF_SR_USE_VIDEO_CACHE
		if (cache_too_small) {
			tr_state->cache_too_small = 1;
		} else {
			/*get the traversal time for each group*/
			traverse_time = gf_sys_clock() - traverse_time;
			group->traverse_time += traverse_time;
			/*record the traversal information and turn cache on if possible*/
			group_2d_cache_evaluate(node, group, tr_state, first_ctx, skip_first_ctx, last_cache_idx);
		}
#endif

		drawable_check_focus_highlight(node, tr_state, NULL);
	}
	else {
		child = ((GF_ParentNode *)node)->children;
		while (child) {
			gf_node_traverse(child->node, tr_state);
			child = child->next;
		}

	}

	if (sensor_backup) {
		/*restore previous traversing state sensors */
		tr_state->vrml_sensors = sensor_backup;
	}
}

/*This is the routine for OrderedGroup child traversing*/
void group_2d_traverse_with_order(GF_Node *node, GroupingNode2D *group, GF_TraverseState *tr_state, u32 *positions)
{
	u32 i, count;
	Bool backup;
	GF_List *sensor_backup;
	GF_Node *child;
	GF_ChildNodeItem *list;
#ifdef GF_SR_USE_VIDEO_CACHE
	Bool group_cached;
#endif

	backup = gf_node_dirty_get(node);
	if (backup & GF_SG_CHILD_DIRTY) {
		GF_SensorHandler *hsens;
		Bool check_anchor=0;
		/*never trigger bounds recompute in 2D since we don't cull 2D groups*/
		u32 ntag = gf_node_get_tag(node);
		group->flags &= ~GROUP_HAS_SENSORS;
		drawable_reset_group_highlight(tr_state, node);
		/*special case for anchor which is a parent node acting as a sensor*/
		if (ntag==TAG_MPEG4_Anchor) check_anchor=1;
#ifndef GPAC_DISABLE_X3D
		else if (ntag==TAG_X3D_Anchor) check_anchor=1;
#endif
		if (check_anchor) {
			GF_SensorHandler *gf_sc_anchor_get_handler(GF_Node *n);

			hsens = gf_sc_anchor_get_handler(node);
			if (hsens) {
				if (!group->sensors) group->sensors = gf_list_new();
				gf_list_add(group->sensors, hsens);
				group->flags |= GROUP_HAS_SENSORS | GROUP_IS_ANCHOR;
			}
		} else {
			list = ((GF_ParentNode *)node)->children;
			while (list) {
				hsens = compositor_mpeg4_get_sensor_handler_ex(list->node, GF_TRUE);
				if (hsens) {
					if (!group->sensors) group->sensors = gf_list_new();
					gf_list_add(group->sensors, hsens);
					group->flags |= GROUP_HAS_SENSORS;
				}
				list = list->next;
			}
		}
	}
	/*not parent (eg form, layout...) sub-tree not dirty and getting bounds, direct copy */
	else if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		tr_state->bounds = group->bounds;
		return;
	}

#ifdef GF_SR_USE_VIDEO_CACHE
	group_cached = group_2d_cache_traverse(node, group, tr_state);
#endif

	/*now here is the trick: ExternProtos may not be loaded at this point, in which case we can't
	perform proper culling. Unloaded ExternProto signal themselves by invalidating their parent
	graph to get a new traversal. We must therefore reset the CHILD_DIRTY flag before computing
	bounds otherwise we'll never re-invalidate the subgraph anymore*/
	gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);


#ifdef GF_SR_USE_VIDEO_CACHE
	if (group_cached) return;
#endif

	/*picking: collect sensors*/
	sensor_backup = NULL;
	if ((tr_state->traversing_mode==TRAVERSE_PICK) && (group->flags & GROUP_HAS_SENSORS) ) {
		/*reset sensor stack if any sensors at this level*/
		sensor_backup = tr_state->vrml_sensors;
		tr_state->vrml_sensors = group->sensors;
	}

	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		list = ((GF_ParentNode *)node)->children;
		backup = tr_state->text_split_mode;
		if (tr_state->text_split_mode && (gf_node_list_get_count(list)>1) ) tr_state->text_split_mode = 0;
		group->flags &= ~GROUP_SKIP_CULLING;
		group->bounds.width = group->bounds.height = 0;
		tr_state->bounds.width = tr_state->bounds.height = 0;
#ifndef GPAC_DISABLE_3D
		tr_state->bbox.is_set = 0;
#endif
		count = gf_node_list_get_count(list);
		for (i=0; i<count; i++) {
			child = gf_node_list_get_child(list, positions[i]);
			gf_node_traverse(child, tr_state);
			if (tr_state->disable_cull) {
				group->flags |= GROUP_SKIP_CULLING;
				tr_state->disable_cull = 0;
			}
			/*handle 3D nodes in 2D groups*/
#ifndef GPAC_DISABLE_3D
			if (tr_state->bbox.is_set) {
				gf_rect_from_bbox(&tr_state->bounds, &tr_state->bbox);
				tr_state->bbox.is_set = 0;
			}
#endif
			gf_rect_union(&group->bounds, &tr_state->bounds);
			tr_state->bounds.width = tr_state->bounds.height = 0;
		}
		tr_state->bounds = group->bounds;

		if (group->flags & GROUP_SKIP_CULLING)
			tr_state->disable_cull = 1;
		tr_state->text_split_mode = backup;

		/*TRAVERSE_SORT */
	} else if (tr_state->traversing_mode==TRAVERSE_SORT) {
		Bool prev_inv = tr_state->invalidate_all;
#ifdef GF_SR_USE_VIDEO_CACHE
		DrawableContext *first_ctx = tr_state->visual->cur_context;
		u32 cache_too_small = 0;
		Bool skip_first_ctx = (first_ctx && first_ctx->drawable) ? 1 : 0;
		u32 traverse_time = gf_sys_clock();
		u32 last_cache_idx = gf_list_count(tr_state->visual->compositor->cached_groups_queue);
		tr_state->cache_too_small = 0;
#endif

		if (backup & GF_SG_VRML_COLOR_DIRTY) {
			tr_state->invalidate_all = 1;
			gf_node_dirty_clear(node, GF_SG_VRML_COLOR_DIRTY);
		}

		list = ((GF_ParentNode *)node)->children;
		count = gf_node_list_get_count(list);
		for (i=0; i<count; i++) {
			child = gf_node_list_get_child(list, positions[i]);
			gf_node_traverse(child, tr_state);
#ifdef GF_SR_USE_VIDEO_CACHE
			if (tr_state->cache_too_small)
				cache_too_small++;
#endif
		}
		tr_state->invalidate_all = prev_inv;

#ifdef GF_SR_USE_VIDEO_CACHE
		if (cache_too_small) {
			tr_state->cache_too_small = 1;
		} else {
			/*get the traversal time for each group*/
			traverse_time = gf_sys_clock() - traverse_time;
			group->traverse_time += traverse_time;
			/*record the traversal information and turn cache on if possible*/
			group_2d_cache_evaluate(node, group, tr_state, first_ctx, skip_first_ctx, last_cache_idx);
		}
#endif

		drawable_check_focus_highlight(node, tr_state, NULL);
	} else {
		list = ((GF_ParentNode *)node)->children;
		count = gf_node_list_get_count(list);
		for (i=0; i<count; i++) {
			child = gf_node_list_get_child(list, positions[i]);
			gf_node_traverse(child, tr_state);
		}
	}

	if (sensor_backup) {
		/*restore previous traversing state sensors*/
		tr_state->vrml_sensors = sensor_backup;
	}
}

/*
 *	3D Grouping tools
 */

#ifndef GPAC_DISABLE_3D

void group_3d_delete(GF_Node *node)
{
	GroupingNode *group = (GroupingNode *)gf_node_get_private(node);

	gf_free(group);
}

GroupingNode *group_3d_new(GF_Node *node)
{
	GroupingNode *st;
	GF_SAFEALLOC(st, GroupingNode);
	gf_node_set_private(node, st);
	return st;
}

/*returns 2 if local light, 1 if global light and 0 if not a light*/
static u32 get_light_type(GF_Node *n)
{
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_DirectionalLight:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_DirectionalLight:
#endif
		return 2;
	case TAG_MPEG4_PointLight:
	case TAG_MPEG4_SpotLight:
		return 1;
	default:
		return 0;
	}
}
/*This is the generic routine for child traversing*/
void group_3d_traverse(GF_Node *node, GroupingNode *group, GF_TraverseState *tr_state)
{
	u32 mode_back;
	Bool split_text_backup, do_lights;
	DirectionalLightContext *dl;
	GF_List *sensor_backup;
	GF_SensorHandler *hsens;
	GF_ChildNodeItem *l;

	if (gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY) {
		//we are drawing 3D object but configured for 2D, force 3D
		if (!tr_state->visual->type_3d && tr_state->visual->compositor->hybrid_opengl) {
			tr_state->visual->compositor->root_visual_setup=0;
			tr_state->visual->compositor->force_type_3d=1;
		}
		
		/*need to recompute bounds*/
		if (tr_state->traversing_mode!=TRAVERSE_GET_BOUNDS) {
			/*traverse subtree to recompute bounds*/
			mode_back = tr_state->traversing_mode;
			tr_state->traversing_mode=TRAVERSE_GET_BOUNDS;
			group_3d_traverse(node, group, tr_state);
			tr_state->traversing_mode = mode_back;
		}
		/*we're recomputing bounds*/
		else {
			u32 ntag = gf_node_get_tag(node);
			group->flags &= ~(GROUP_HAS_SENSORS | GROUP_HAS_LIGHTS);

			/*special case for anchor which is a parent node acting as a sensor*/
			if ((ntag==TAG_MPEG4_Anchor)
#ifndef GPAC_DISABLE_X3D
			        || (ntag==TAG_X3D_Anchor)
#endif
			   ) group->flags |= GROUP_HAS_SENSORS;

			l = ((GF_ParentNode*)node)->children;
			while (l) {
				hsens = compositor_mpeg4_get_sensor_handler_ex(l->node, GF_TRUE);
				if (hsens) {
					group->flags |= GROUP_HAS_SENSORS;
				}
				else if (get_light_type(l->node)) {
					group->flags |= GROUP_HAS_LIGHTS;
				}
				l = l->next;
			}

			/*now here is the trick: ExternProtos may not be loaded at this point, in which case we can't
			perform proper culling. Unloaded ExternProto signal themselves by invalidating their parent
			graph to get a new traversal. We must therefore reset the CHILD_DIRTY flag before computing
			bounds otherwise we'll never re-invalidate the subgraph anymore*/
			gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
		}
	}
	/*not parent (eg form, layout...) sub-tree not dirty and getting bounds, direct copy */
	else if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		tr_state->bbox = group->bbox;
		if (!tr_state->bbox.is_set) tr_state->bbox.radius=-FIX_ONE;
		gf_node_dirty_clear(node, 0);
		return;
	}

	gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);

	mode_back=tr_state->cull_flag;
	/*if culling not disabled*/
	if (!(group->flags & GROUP_SKIP_CULLING)
	        /*for geometry AND lights*/
	        && (tr_state->traversing_mode==TRAVERSE_SORT)
	        /*do cull*/
	        && !visual_3d_node_cull(tr_state, &group->bbox, 0)) {

		tr_state->cull_flag = mode_back;
		return;
	}


	/*picking: collect sensors*/
	sensor_backup = NULL;
	if ((tr_state->traversing_mode==TRAVERSE_PICK) && (group->flags & GROUP_HAS_SENSORS) ) {
		/*reset sensor stack if any sensors at this level*/
		sensor_backup = tr_state->vrml_sensors;
		tr_state->vrml_sensors = gf_list_new();

		l = ((GF_ParentNode*)node)->children;
		while (l) {
			hsens = compositor_mpeg4_get_sensor_handler_ex(l->node, GF_TRUE);
			if (hsens && hsens->IsEnabled(l->node))
				gf_list_add(tr_state->vrml_sensors, hsens);

			l = l->next;
		}
	}

	/*turn on local lights and global ones*/
	do_lights = 0;
	if (group->flags & GROUP_HAS_LIGHTS) {
		/*turn on global lights*/
		if (tr_state->traversing_mode==TRAVERSE_LIGHTING) {
			l = ((GF_ParentNode*)node)->children;
			while (l) {
				if (get_light_type(l->node)==2) gf_node_traverse(l->node, tr_state);
				l = l->next;
			}
		}
		/*turn on local lights*/
		else if (tr_state->traversing_mode==TRAVERSE_SORT) {
			do_lights = 1;
			tr_state->traversing_mode = TRAVERSE_DRAW_3D;
			tr_state->local_light_on = 1;

			l = ((GF_ParentNode*)node)->children;
			while (l) {
				if (get_light_type(l->node)==1) {
					/*store lights for alpha draw*/
					dl = (DirectionalLightContext*)gf_malloc(sizeof(DirectionalLightContext));
					dl->dlight = l->node;
					memcpy(&dl->light_matrix, &tr_state->model_matrix, sizeof(GF_Matrix));
					gf_list_add(tr_state->local_lights, dl);
					/*and turn them on for non-alpha draw*/
					gf_node_traverse(dl->dlight, tr_state);
				}
				l = l->next;
			}
			tr_state->traversing_mode = TRAVERSE_SORT;
		}
	}


	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		l = ((GF_ParentNode*)node)->children;
		split_text_backup = tr_state->text_split_mode;
		if (tr_state->text_split_mode && (gf_node_list_get_count(l)>1) ) tr_state->text_split_mode = 0;
		group->bbox.is_set = tr_state->bbox.is_set = 0;
		tr_state->bounds.width = 0;
		group->flags &= ~GROUP_SKIP_CULLING;

		while (l) {
			gf_node_traverse(l->node, tr_state);
			if (tr_state->disable_cull) {
				group->flags |= GROUP_SKIP_CULLING;
				tr_state->disable_cull = 0;
			}
			/*handle 2D nodes in 3D groups*/
			if (tr_state->bounds.width) {
				gf_bbox_from_rect(&tr_state->bbox, &tr_state->bounds);
				tr_state->bounds.width = 0;
			}

			if (tr_state->bbox.is_set) {
				gf_bbox_union(&group->bbox, &tr_state->bbox);
			}
			tr_state->bbox.is_set = 0;
			l = l->next;
		}
		tr_state->bbox = group->bbox;
		if (group->flags & GROUP_SKIP_CULLING)
			tr_state->disable_cull = 1;
		tr_state->text_split_mode = split_text_backup;
	} else {
		l = ((GF_ParentNode*)node)->children;
		while (l) {
			gf_node_traverse(l->node, tr_state);
			l = l->next;
		}

		if (tr_state->traversing_mode==TRAVERSE_SORT)
			drawable3d_check_focus_highlight(node, tr_state, NULL);
	}
	tr_state->cull_flag = mode_back;

	if (sensor_backup) {
		/*destroy current traversing state sensors and restore previous*/
		gf_list_del(tr_state->vrml_sensors);
		tr_state->vrml_sensors = sensor_backup;
	}

	/*remove dlights*/
	if (do_lights) {
		u32 lcount;
		tr_state->traversing_mode = TRAVERSE_DRAW_3D;
		tr_state->local_light_on = 0;
		while ( (lcount = gf_list_count(tr_state->local_lights)) ) {
			dl = (DirectionalLightContext*)gf_list_get(tr_state->local_lights, lcount-1);
			gf_list_rem(tr_state->local_lights, lcount-1);
			gf_node_traverse(dl->dlight, tr_state);
			gf_free(dl);
		}
		/*and back to sort mode*/
		tr_state->traversing_mode = TRAVERSE_SORT;
	}
}


#endif


/*
 *	2D ParentNode tools - used by all nodes performing children layout
 */


void parent_node_setup(ParentNode2D *group)
{
	group->groups = gf_list_new();
}

void parent_node_predestroy(ParentNode2D *group)
{
	/*just in case*/
	parent_node_reset(group);
	gf_list_del(group->groups);
}

void parent_node_reset(ParentNode2D *group)
{
	while (gf_list_count(group->groups)) {
		ChildGroup *cg = (ChildGroup *)gf_list_get(group->groups, 0);
		gf_list_rem(group->groups, 0);
		gf_free(cg);
	}
}

void parent_node_start_group(ParentNode2D *group, GF_Node *n, Bool discardable)
{
	ChildGroup *cg;
	if (!n) {
		cg = gf_list_last(group->groups);
		if (!cg) return;
		n = cg->child;
	}
	GF_SAFEALLOC(cg, ChildGroup);
	if (!cg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate child group\n"));
		return;
	}
	cg->child = n;
	cg->text_type = discardable;
	gf_list_add(group->groups, cg);
}

void parent_node_end_group(ParentNode2D *group, GF_Rect *bounds)
{
	ChildGroup *cg = (ChildGroup *)gf_list_last(group->groups);
	if (!cg) return;
	/*don't override splitted text info*/
	if (cg->ascent || cg->descent) return;
	cg->original = *bounds;
	cg->final = cg->original;
}

void parent_node_end_text_group(ParentNode2D *group, GF_Rect *bounds, Fixed ascent, Fixed descent, u32 text_split_idx)
{
	ChildGroup *cg = (ChildGroup *)gf_list_last(group->groups);
	if (!cg) return;
	cg->text_split_idx = text_split_idx;
	cg->ascent = ascent;
	cg->descent = descent;
	cg->final = cg->original = *bounds;
}



void parent_node_traverse(GF_Node *node, ParentNode2D *group, GF_TraverseState *tr_state)
{
	Bool split_text_backup;
	GF_List *sensor_backup;
	GF_ChildNodeItem *l;

	if (gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY) {
		Bool check_anchor=0;
		/*parent groups must recompute their bounds themselves since they modify children layout*/
		u32 ntag = gf_node_get_tag(node);
		group->flags &= ~GROUP_HAS_SENSORS;
		/*special case for anchor which is a parent node acting as a sensor*/
		if (ntag==TAG_MPEG4_Anchor) check_anchor=1;
#ifndef GPAC_DISABLE_X3D
		else if (ntag==TAG_X3D_Anchor) check_anchor=1;
#endif
		if (check_anchor) {
			group->flags |= GROUP_HAS_SENSORS | GROUP_IS_ANCHOR;
		} else {
			l = ((GF_ParentNode *)node)->children;
			while (l) {
				GF_SensorHandler *hsens = compositor_mpeg4_get_sensor_handler_ex(l->node, GF_TRUE);
				if (hsens) {
					group->flags |= GROUP_HAS_SENSORS;
					break;
				}
				l = l->next;
			}
		}

		/*now here is the trick: ExternProtos may not be loaded at this point, in which case we can't
		perform proper culling. Unloaded ExternProto signal themselves by invalidating their parent
		graph to get a new traversal. We must therefore reset the CHILD_DIRTY flag before computing
		bounds otherwise we'll never re-invalidate the subgraph anymore*/
		gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
	}
	gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);

	/*no culling in 2D*/

	/*picking: collect sensors*/
	sensor_backup = NULL;
	if ((tr_state->traversing_mode==TRAVERSE_PICK) && (group->flags & GROUP_HAS_SENSORS) ) {
		/*reset sensor stack if any sensors at this level*/
		sensor_backup = tr_state->vrml_sensors;
		tr_state->vrml_sensors = gf_list_new();


		/*add sensor(s) to traversing state*/
		l = ((GF_ParentNode *)node)->children;
		while (l) {
			GF_SensorHandler *hsens = compositor_mpeg4_get_sensor_handler_ex(l->node, GF_TRUE);
			if (hsens) gf_list_add(tr_state->vrml_sensors, hsens);
			l = l->next;
		}
	}


	split_text_backup = tr_state->text_split_mode;

	group->flags &= ~GROUP_SKIP_CULLING;
	tr_state->bounds.width = tr_state->bounds.height = 0;
#ifndef GPAC_DISABLE_3D
	tr_state->bbox.is_set = 0;
#endif

	l = ((GF_ParentNode *)node)->children;
	while (l) {
		parent_node_start_group(group, l->node, 0);

		tr_state->bounds.width = tr_state->bounds.height = 0;

		gf_node_traverse(l->node, tr_state);

		/*handle 3D nodes in 2D groups*/
#ifndef GPAC_DISABLE_3D
		if (tr_state->bbox.is_set) {
			gf_rect_from_bbox(&tr_state->bounds, &tr_state->bbox);
			tr_state->bbox.is_set = 0;
		}
#endif
		parent_node_end_group(group, &tr_state->bounds);
		l = l->next;
	}
	tr_state->text_split_mode = split_text_backup;

	if (sensor_backup) {
		/*destroy current traversing state sensors and restore previous*/
		gf_list_del(tr_state->vrml_sensors);
		tr_state->vrml_sensors = sensor_backup;
	}
}

/*final drawing of each group*/
void parent_node_child_traverse(ChildGroup *cg, GF_TraverseState *tr_state)
{
	Fixed dx, dy;

	dx = cg->final.x - cg->original.x + cg->scroll_x;
	dy = cg->final.y - cg->original.y + cg->scroll_y;
	tr_state->text_split_idx = cg->text_split_idx;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx_bckup;

		gf_mx_copy(mx_bckup, tr_state->model_matrix);
		gf_mx_add_translation(&tr_state->model_matrix, dx, dy, 0);
		gf_node_traverse(cg->child, tr_state);
		gf_mx_copy(tr_state->model_matrix, mx_bckup);
	} else
#endif
	{
		GF_Matrix2D mx2d;
		gf_mx2d_copy(mx2d, tr_state->transform);
		gf_mx2d_init(tr_state->transform);
		gf_mx2d_add_translation(&tr_state->transform, dx, dy);
		gf_mx2d_add_matrix(&tr_state->transform, &mx2d);
		gf_node_traverse(cg->child, tr_state);
		gf_mx2d_copy(tr_state->transform, mx2d);
	}
	tr_state->text_split_idx = 0;
}

void parent_node_child_traverse_matrix(ChildGroup *cg, GF_TraverseState *tr_state, GF_Matrix2D *mat2D)
{
	if (!mat2D) return;

	tr_state->text_split_idx = cg->text_split_idx;
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx, mx_bckup;
		gf_mx_from_mx2d(&mx, mat2D);
		gf_mx_copy(mx_bckup, tr_state->model_matrix);
		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
		gf_node_traverse(cg->child, tr_state);
		gf_mx_copy(tr_state->model_matrix, mx_bckup);
	} else
#endif
	{
		GF_Matrix2D mx2d;

		gf_mx2d_copy(mx2d, tr_state->transform);
		gf_mx2d_pre_multiply(&tr_state->transform, mat2D);
		gf_node_traverse(cg->child, tr_state);
		gf_mx2d_copy(tr_state->transform, mx2d);
	}
	tr_state->text_split_idx = 0;
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
