/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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


#include "render3d_nodes.h"
#include "grouping.h"

void group_start_child(GroupingNode *group, GF_Node *n)
{
	ChildGroup *cg;
	if (!n) {
		ChildGroup *cg_prev = (ChildGroup *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
		n = cg_prev ? cg_prev->child : NULL;
	}
	if (!n) return;
	GF_SAFEALLOC(cg, ChildGroup);
	cg->child = n;
	gf_list_add(group->groups, cg);
}

void group_end_child(GroupingNode *group, GF_BBox *bounds)
{
	ChildGroup *cg = (ChildGroup *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
	if (!cg) return;
	/*don't override splitted text info*/
	if (cg->is_text_group) return;
	gf_rect_from_bbox(&cg->original, bounds);
	cg->final = cg->original;
}

void group_end_text_child(GroupingNode *group, GF_Rect *bounds, Fixed ascent, Fixed descent, u32 split_text_idx)
{
	ChildGroup *cg = (ChildGroup *)gf_list_get(group->groups, gf_list_count(group->groups)-1);
	if (!cg) return;
	cg->split_text_idx = split_text_idx;
	cg->is_text_group = 1;
	cg->ascent = ascent;
	cg->descent = descent;
	cg->final = cg->original = *bounds;
}


void group_reset_children(GroupingNode *group)
{
	while (gf_list_count(group->groups)) {
		ChildGroup *cg = (ChildGroup *)gf_list_get(group->groups, 0);
		gf_list_rem(group->groups, 0);
		free(cg);
	}
}



/*This is the generic routine for child traversing*/
void grouping_traverse(GroupingNode *group, RenderEffect3D *eff, u32 *positions)
{
	u32 i, count, mode_back;
	Bool split_text_backup, is_parent, get_bounds, do_lights;
	DLightContext *dl;
	GF_List *sensor_backup;
	GF_Node *child;
	SensorHandler *hsens;
	GF_ChildNodeItem *l;

	is_parent = (eff->parent == group) ? 1 : 0;

	if (gf_node_dirty_get(group->owner) & GF_SG_CHILD_DIRTY) {
		/*need to recompute bounds*/
		if (eff->traversing_mode!=TRAVERSE_GET_BOUNDS) {
			/*traverse subtree to recompute bounds*/
			mode_back = eff->traversing_mode;
			eff->traversing_mode=TRAVERSE_GET_BOUNDS;
			grouping_traverse(group, eff, positions);
			eff->traversing_mode = mode_back;
		}
		/*we're recomputing bounds*/
		else {
			u32 ntag = gf_node_get_tag(group->owner);
			/*rebuild sensors/lights*/
			if (group->sensors) gf_list_del(group->sensors);
			group->sensors = gf_list_new();
			if (group->lights) gf_list_del(group->lights);
			group->lights = gf_list_new();

			/*special case for anchor which is a parent node acting as a sensor*/
			if ((ntag==TAG_MPEG4_Anchor) || (ntag==TAG_X3D_Anchor)) {
				SensorHandler *r3d_anchor_get_handler(GF_Node *n);
				hsens = r3d_anchor_get_handler(group->owner);
				if (hsens) gf_list_add(group->sensors, hsens);
			}

			l = *group->children;
			if (positions) {
				count = gf_node_list_get_count(l);
				for (i=0; i<count; i++) {
					child = gf_node_list_get_child(l, positions[i]);
					hsens = r3d_get_sensor_handler(child);
					if (hsens) gf_list_add(group->sensors, hsens);
					else if (r3d_is_light(child, 0)) gf_list_add(group->lights, child);
				}
			} else {
				while (l) {
					hsens = r3d_get_sensor_handler(l->node);
					if (hsens) gf_list_add(group->sensors, hsens);
					else if (r3d_is_light(l->node, 0)) gf_list_add(group->lights, l->node);
					l = l->next;
				}
			}	

			if (!gf_list_count(group->sensors)) {
				gf_list_del(group->sensors);
				group->sensors = NULL;
			}
			if (!gf_list_count(group->lights)) {
				gf_list_del(group->lights);
				group->lights = NULL;
			}

			/*now here is the trick: ExternProtos may not be loaded at this point, in which case we can't
			perform proper culling. Unloaded ExternProto signal themselves by invalidating their parent
			graph to get a new Render(). We must therefore reset the CHILD_DIRTY flag before computing 
			bounds otherwise we'll never re-invalidate the subgraph anymore*/
			gf_node_dirty_clear(group->owner, GF_SG_CHILD_DIRTY);
		}
	}
	/*not parent (eg form, layout...) sub-tree not dirty and getting bounds, direct copy */
	else if (!is_parent && (eff->traversing_mode==TRAVERSE_GET_BOUNDS) ) {
		eff->bbox = group->bbox;
		gf_node_dirty_clear(group->owner, 0);
		return;
	}

	gf_node_dirty_clear(group->owner, GF_SG_NODE_DIRTY);

	mode_back=eff->cull_flag;
	/*if culling not disabled*/
	if (!group->dont_cull &&
		/*for geometry AND lights*/
		(eff->traversing_mode==TRAVERSE_SORT) 
		/*do cull*/
		&& !node_cull(eff, &group->bbox, 0)) {

		eff->cull_flag = mode_back;
		return;
	}
	

	/*turn on global lights*/
	if (group->lights && (eff->traversing_mode==TRAVERSE_LIGHTING)) {
		u32 lcount = gf_list_count(group->lights);
		for (i=0; i<lcount; i++) {
			GF_Node *n = (GF_Node*)gf_list_get(group->lights, i);
			/*don't render local lights*/
			if (r3d_is_light(n, 1)) continue;
			gf_node_render(n, eff);
		}
	}

	/*picking*/
	sensor_backup = NULL;
	if (group->sensors && (eff->traversing_mode==TRAVERSE_PICK)) {
		u32 scount;
		/*reset sensor stack if any sensors at this level*/
		sensor_backup = eff->sensors;
		eff->sensors = gf_list_new();

		/*add sensor(s) to effects*/
		scount = gf_list_count(group->sensors);
		for (i=0; i <scount; i++) {
			SensorHandler *hsens = (SensorHandler *)gf_list_get(group->sensors, i);
			gf_list_add(eff->sensors, hsens);
		}
	}

	/*turn on local lights*/
	do_lights = 0;
	if (group->lights && (eff->traversing_mode==TRAVERSE_SORT)) {
		u32 lcount;
		do_lights = 1;
		eff->traversing_mode = TRAVERSE_RENDER;
		eff->local_light_on = 1;

		lcount = gf_list_count(group->lights);
		for (i=0; i<lcount; i++) {
			GF_Node *n = (GF_Node*)gf_list_get(group->lights, i);
			if (r3d_is_light(n, 1)) {
				/*store lights for alpha render*/
				dl = (DLightContext*)malloc(sizeof(DLightContext));
				dl->dlight = n;
				memcpy(&dl->light_matrix, &eff->model_matrix, sizeof(GF_Matrix));
				gf_list_add(eff->local_lights, dl);
				/*and turn them on for non-alpha render*/
				gf_node_render(dl->dlight, eff);
			}
		}
		eff->traversing_mode = TRAVERSE_SORT;
	}
	
	get_bounds = (!is_parent && (eff->traversing_mode==TRAVERSE_GET_BOUNDS)) ? 1 : 0;

	if (get_bounds || is_parent) {
		split_text_backup = eff->text_split_mode;
		if (!is_parent && (count>1)) eff->text_split_mode = 0;
		group->dont_cull = group->bbox.is_set = eff->bbox.is_set = 0;
		
		l = *group->children;
		if (positions) {
			count = gf_node_list_get_count(l);
			for (i=0; i<count; i++) {
				child = gf_node_list_get_child(l, positions[i]);
				if (is_parent) group_start_child(group, child);
				gf_node_render(child, eff);
				if (is_parent) group_end_child(group, &eff->bbox);
				else if (get_bounds) {
					if (eff->trav_flags & TF_DONT_CULL) {
						group->dont_cull = 1;
						eff->trav_flags &= ~TF_DONT_CULL;
					} 
					if (eff->bbox.is_set) {
						gf_bbox_union(&group->bbox, &eff->bbox);
					}
					eff->bbox.is_set = 0;
				}
			}
		} else {
			while (l) {
				if (is_parent) group_start_child(group, l->node);
				gf_node_render(l->node, eff);
				if (is_parent) group_end_child(group, &eff->bbox);
				else if (get_bounds) {
					if (eff->trav_flags & TF_DONT_CULL) {
						group->dont_cull = 1;
						eff->trav_flags &= ~TF_DONT_CULL;
					} 
					if (eff->bbox.is_set) {
						gf_bbox_union(&group->bbox, &eff->bbox);
					}
					eff->bbox.is_set = 0;
				}
				l = l->next;
			}
		}

		eff->bbox = group->bbox;
		if (group->dont_cull) eff->trav_flags |= TF_DONT_CULL;
		eff->text_split_mode = split_text_backup;
	} else {
		l = *group->children;
		if (positions) {
			count = gf_node_list_get_count(l);
			for (i=0; i<count; i++) {
				child = gf_node_list_get_child(l, positions[i]);
				gf_node_render(child, eff);
			} 
		} else {
			while (l) {
				gf_node_render(l->node, eff);
				l = l->next;
			} 
		}
	}
	eff->cull_flag = mode_back;

	if (sensor_backup) {
		/*destroy current effect list and restore previous*/
		gf_list_del(eff->sensors);
		eff->sensors = sensor_backup;
	}

	/*remove dlights*/
	if (do_lights) {
		u32 lcount;
		eff->traversing_mode = TRAVERSE_RENDER;
		eff->local_light_on = 0;
		while ( (lcount = gf_list_count(eff->local_lights)) ) {
			dl = (DLightContext*)gf_list_get(eff->local_lights, lcount-1);
			gf_list_rem(eff->local_lights, lcount-1);
			gf_node_render(dl->dlight, eff);
			free(dl);
		}
		/*and back to sort mode*/
		eff->traversing_mode = TRAVERSE_SORT;
	}
}

/*final drawing of each group*/
void child_render_done(ChildGroup *cg, RenderEffect3D *eff)
{
	GF_Matrix mx, gf_mx_bckup;
	gf_mx_init(mx);
	gf_mx_add_translation(&mx, cg->final.x - cg->original.x, cg->final.y - cg->original.y, 0);

	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_translation(&eff->model_matrix, cg->final.x - cg->original.x, cg->final.y - cg->original.y, 0);

	eff->split_text_idx = cg->split_text_idx;
	if (eff->traversing_mode==TRAVERSE_SORT) {
		VS3D_PushMatrix(eff->surface);
		VS3D_MultMatrix(eff->surface, mx.m);
	}
	gf_node_render(cg->child, eff);
	if (eff->traversing_mode==TRAVERSE_SORT) VS3D_PopMatrix(eff->surface);
	eff->split_text_idx = 0;
	gf_mx_copy(eff->model_matrix, gf_mx_bckup);
}

void child_render_done_complex(ChildGroup *cg, RenderEffect3D *eff, GF_Matrix2D *mat2D)
{
	GF_Matrix mx, gf_mx_bckup;
	if (!mat2D) return;
	gf_mx_from_mx2d(&mx, mat2D);
	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_matrix(&eff->model_matrix, &mx);
	eff->split_text_idx = cg->split_text_idx;
	if (eff->traversing_mode==TRAVERSE_SORT) {
		VS3D_PushMatrix(eff->surface);
		VS3D_MultMatrix(eff->surface, mx.m);
	}
	gf_node_render(cg->child, eff);
	if (eff->traversing_mode==TRAVERSE_SORT) VS3D_PopMatrix(eff->surface);
	eff->split_text_idx = 0;
	gf_mx_copy(eff->model_matrix, gf_mx_bckup);
}


void SetupGroupingNode(GroupingNode *group, GF_Renderer *sr, GF_Node *node, GF_ChildNodeItem **children)
{
	memset(group, 0, sizeof(GroupingNode));
	gf_sr_traversable_setup(group, node, sr);
	group->groups = gf_list_new();
	group->children = children;
}

void DeleteGroupingNode(GroupingNode *group)
{
	/*just in case*/
	group_reset_children(group);
	gf_list_del(group->groups);
	if (group->sensors) gf_list_del(group->sensors);
	group->sensors = NULL;
	if (group->lights) gf_list_del(group->lights);
	group->lights = NULL;
}

void DestroyBaseGrouping(GF_Node *node)
{
	GroupingNode *group = (GroupingNode *)gf_node_get_private(node);
	DeleteGroupingNode(group);
	free(group);
}

void NewGroupingNodeStack(GF_Renderer *sr, GF_Node *node, GF_ChildNodeItem **children)
{
	GroupingNode *st = (GroupingNode *)malloc(sizeof(GroupingNode));
	if (!st) return;
	SetupGroupingNode(st, sr, node, children);
	gf_node_set_private(node, st);
}

