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

#ifdef GPAC_DISABLE_3D

static void TraverseGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	GroupingNode2D *group = (GroupingNode2D *) gf_node_get_private(node);
	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		gf_free(group);
	} else {
		group_2d_traverse(node, group, (GF_TraverseState*)rs);
	}
}

void compositor_init_group(GF_Compositor *compositor, GF_Node *node)
{
	GroupingNode2D *ptr;
	GF_SAFEALLOC(ptr, GroupingNode2D);
	if (!ptr) return;
	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, TraverseGroup);
}

void compositor_init_static_group(GF_Compositor *compositor, GF_Node *node)
{
	compositor_init_group(compositor, node);
}

#else

static void TraverseGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	GroupingNode *group = (GroupingNode *) gf_node_get_private(node);
	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_3d_delete(node);
	} else {
		group_3d_traverse(node, group, (GF_TraverseState*)rs);
	}
}

void compositor_init_group(GF_Compositor *compositor, GF_Node *node)
{
	group_3d_new(node);
	gf_node_set_callback_function(node, TraverseGroup);
}


/*currently static group use the same drawing as all grouping nodes, without any opts.
since we already do a lot of caching (lights, sensors) at the grouping node level, I'm not sure
we could get anything better trying to optimize the rest, since what is not cached in the base grouping
node always involves frustum culling, and caching that would require caching partial model matrix (from
the static group to each leaf)*/
void compositor_init_static_group(GF_Compositor *compositor, GF_Node *node)
{
	group_3d_new(node);
	gf_node_set_callback_function(node, TraverseGroup);
}


void TraverseCollision(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 collide_flags;
	SFVec3f last_point;
	Fixed last_dist;
	M_Collision *col = (M_Collision *)node;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	GroupingNode *group = (GroupingNode *) gf_node_get_private(node);

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_3d_delete(node);
		return;
	}

	if (tr_state->traversing_mode != TRAVERSE_COLLIDE) {
		group_3d_traverse(node, group, tr_state);
	} else if (col->collide) {

		collide_flags = tr_state->camera->collide_flags;
		last_dist = tr_state->camera->collide_dist;
		tr_state->camera->collide_flags &= 0;
		tr_state->camera->collide_dist = FIX_MAX;
		last_point = tr_state->camera->collide_point;

		if (col->proxy) {
			/*always check bounds to update any dirty node*/
			tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
			gf_node_traverse(col->proxy, rs);

			tr_state->traversing_mode = TRAVERSE_COLLIDE;
			gf_node_traverse(col->proxy, rs);
		} else {
			group_3d_traverse(node, group, (GF_TraverseState*)rs);
		}
		if (tr_state->camera->collide_flags & CF_COLLISION) {
			col->collideTime = gf_node_get_scene_time(node);
			gf_node_event_out(node, 5/*"collideTime"*/);
			/*if not closer restore*/
			if (collide_flags && (last_dist<tr_state->camera->collide_dist)) {
				tr_state->camera->collide_flags = collide_flags;
				tr_state->camera->collide_dist = last_dist;
				tr_state->camera->collide_point = last_point;
			}
		} else {
			tr_state->camera->collide_flags = collide_flags;
			tr_state->camera->collide_dist = last_dist;
		}
	}
}

void compositor_init_collision(GF_Compositor *compositor, GF_Node *node)
{
	group_3d_new(node);
	gf_node_set_callback_function(node, TraverseCollision);
}

/*for transform, transform2D & transformMatrix2D*/
typedef struct
{
	GROUPING_NODE_STACK_3D

	GF_Matrix mx;
	Bool has_scale;
} TransformStack;

static void DestroyTransform(GF_Node *n)
{
	TransformStack *ptr = (TransformStack *)gf_node_get_private(n);
	gf_sc_check_focus_upon_destroy(n);
	gf_free(ptr);
}

static void NewTransformStack(GF_Compositor *compositor, GF_Node *node, GF_ChildNodeItem **children)
{
	TransformStack *st;
	GF_SAFEALLOC(st, TransformStack);
	if (!st) return;

	gf_mx_init(st->mx);
	gf_node_set_private(node, st);
}

static void TraverseTransform(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	TransformStack *st = (TransformStack *)gf_node_get_private(n);
	M_Transform *tr = (M_Transform *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		DestroyTransform(n);
		return;
	}

	/*note we don't clear dirty flag, this is done in traversing*/
	if (gf_node_dirty_get(n) & GF_SG_NODE_DIRTY) {
		Bool scale_rot, recenter;
		gf_mx_init(st->mx);
		if (tr->translation.x || tr->translation.y || tr->translation.z)
			gf_mx_add_translation(&st->mx, tr->translation.x, tr->translation.y, tr->translation.z);
		recenter = (tr->center.x || tr->center.y || tr->center.z) ? 1 : 0;
		if (recenter)
			gf_mx_add_translation(&st->mx, tr->center.x, tr->center.y, tr->center.z);

		if (tr->rotation.q) gf_mx_add_rotation(&st->mx, tr->rotation.q, tr->rotation.x, tr->rotation.y, tr->rotation.z);
		scale_rot = (tr->scaleOrientation.q) ? 1 : 0;
		if (scale_rot)
			gf_mx_add_rotation(&st->mx, tr->scaleOrientation.q, tr->scaleOrientation.x, tr->scaleOrientation.y, tr->scaleOrientation.z);
		if ((tr->scale.x != FIX_ONE) || (tr->scale.y != FIX_ONE) || (tr->scale.z != FIX_ONE))
			gf_mx_add_scale(&st->mx, tr->scale.x, tr->scale.y, tr->scale.z);
		if (scale_rot)
			gf_mx_add_rotation(&st->mx, -tr->scaleOrientation.q, tr->scaleOrientation.x, tr->scaleOrientation.y, tr->scaleOrientation.z);
		if (recenter)
			gf_mx_add_translation(&st->mx, -tr->center.x, -tr->center.y, -tr->center.z);

		st->has_scale = ((tr->scale.x != FIX_ONE) || (tr->scale.y != FIX_ONE) || (tr->scale.z != FIX_ONE)) ? 1 : 0;
	}

	gf_mx_copy(gf_mx_bckup, tr_state->model_matrix);
	gf_mx_add_matrix(&tr_state->model_matrix, &st->mx);

	/*note we don't clear dirty flag, this is done in traversing*/
	group_3d_traverse(n, (GroupingNode *) st, tr_state);

	gf_mx_copy(tr_state->model_matrix, gf_mx_bckup);
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS)
		gf_mx_apply_bbox(&st->mx, &tr_state->bbox);
}

void compositor_init_transform(GF_Compositor *compositor, GF_Node *node)
{
	NewTransformStack(compositor, node, & ((M_Transform *)node)->children);
	gf_node_set_callback_function(node, TraverseTransform);

}


static void TraverseBillboard(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	TransformStack *st = (TransformStack *)gf_node_get_private(n);
	M_Billboard *bb = (M_Billboard *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		DestroyTransform(n);
		return;
	}
	if (! tr_state->camera) return;

	/*can't cache the matrix here*/
	gf_mx_init(st->mx);
	if (tr_state->camera->is_3D) {
		SFVec3f z, axis;
		Fixed axis_len;
		SFVec3f user_pos = tr_state->camera->position;

		gf_mx_apply_vec(&tr_state->model_matrix, &user_pos);
		gf_vec_norm(&user_pos);
		axis = bb->axisOfRotation;
		axis_len = gf_vec_len(axis);
		if (axis_len<FIX_EPSILON) {
			SFVec3f x, y, t;
			/*get user's right in local coord*/
			gf_vec_diff(t, tr_state->camera->position, tr_state->camera->target);
			gf_vec_norm(&t);
			x = gf_vec_cross(tr_state->camera->up, t);
			gf_vec_norm(&x);
			gf_mx_rotate_vector(&tr_state->model_matrix, &x);
			gf_vec_norm(&x);
			/*get user's up in local coord*/
			y = tr_state->camera->up;
			gf_mx_rotate_vector(&tr_state->model_matrix, &y);
			gf_vec_norm(&y);
			z = gf_vec_cross(x, y);
			gf_vec_norm(&z);

			gf_mx_rotation_matrix_from_vectors(&st->mx, x, y, z);
			gf_mx_inverse(&st->mx);
		} else {
			SFVec3f tmp;
			Fixed d, cosw, sinw, angle;
			gf_vec_norm(&axis);
			/*map eye & z into plane with normal axis through 0.0*/
			d = -gf_vec_dot(axis, user_pos);
			tmp = gf_vec_scale(axis, d);
			gf_vec_add(user_pos, user_pos, tmp);
			gf_vec_norm(&user_pos);

			z.x = z.y = 0;
			z.z = FIX_ONE;
			d = -gf_vec_dot(axis, z);
			tmp = gf_vec_scale(axis, d);
			gf_vec_add(z, z, tmp);
			gf_vec_norm(&z);

			cosw = gf_vec_dot(user_pos, z);
			tmp = gf_vec_cross(user_pos, z);
			sinw = gf_vec_len(tmp);
			angle = gf_acos(cosw);
			gf_vec_norm(&tmp);
			if ((sinw>0) && (gf_vec_dot(axis, tmp) > 0)) gf_vec_rev(axis);
			gf_mx_add_rotation(&st->mx, angle, axis.x, axis.y, axis.z);
		}
	}

	gf_mx_copy(gf_mx_bckup, tr_state->model_matrix);
	gf_mx_add_matrix(&tr_state->model_matrix, &st->mx);

	/*note we don't clear dirty flag, this is done in traversing*/
	group_3d_traverse(n, (GroupingNode *) st, tr_state);

	gf_mx_copy(tr_state->model_matrix, gf_mx_bckup);

	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) gf_mx_apply_bbox(&st->mx, &tr_state->bbox);
}

void compositor_init_billboard(GF_Compositor *compositor, GF_Node *node)
{
	NewTransformStack(compositor, node, & ((M_Billboard *)node)->children);
	gf_node_set_callback_function(node, TraverseBillboard);

}

static void TraverseLOD(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_ChildNodeItem *children;
	MFFloat *ranges;
	SFVec3f pos, usr;
	u32 which_child, nb_children;
	Fixed dist;
	Bool do_all;
	GF_Matrix mx;
	SFVec3f center;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	s32 *prev_child = (s32 *)gf_node_get_private(node);

	if (is_destroy) {
		gf_free(prev_child);
		gf_sc_check_focus_upon_destroy(node);
		return;
	}

	/*WARNING: X3D/MPEG4 NOT COMPATIBLE*/
	if (gf_node_get_tag(node) == TAG_MPEG4_LOD) {
		children = ((M_LOD *) node)->level;
		ranges = &((M_LOD *) node)->range;
		center = ((M_LOD *) node)->center;
#ifndef GPAC_DISABLE_X3D
	} else {
		children = ((X_LOD *) node)->children;
		ranges = &((X_LOD *) node)->range;
		center = ((X_LOD *) node)->center;
#endif
	}

	if (!children) return;
	nb_children = gf_node_list_get_count(children);

	if (!tr_state->camera) {
		do_all = 1;
		which_child = 0;
	} else {
		/*can't cache the matrix here*/
		usr = tr_state->camera->position;
		pos = center;
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);
		gf_mx_apply_vec(&mx, &usr);
		gf_vec_diff(pos, pos, usr);
		dist = gf_vec_len(pos);
		for (which_child=0; which_child<ranges->count; which_child++) {
			if (dist<ranges->vals[which_child]) break;
		}
		if (which_child>=nb_children) which_child = nb_children-1;

		/*check if we're traversing the same child or not for audio rendering*/
		do_all = 0;
		if (gf_node_dirty_get(node)) {
			gf_node_dirty_clear(node, 0);
			do_all = 1;
		} else if ((s32) which_child != *prev_child) {
			*prev_child = which_child;
			do_all = 1;
		}
	}

	if (do_all) {
		u32 i;
		Bool prev_switch = tr_state->switched_off;
		GF_ChildNodeItem *l = children;
		tr_state->switched_off = 1;
		i=0;
		while (l) {
			if (i!=which_child) gf_node_traverse(l->node, rs);
			l = l->next;
		}
		tr_state->switched_off = prev_switch;
	}
	gf_node_traverse(gf_node_list_get_child(children, which_child), rs);
}

void compositor_init_lod(GF_Compositor *compositor, GF_Node *node)
{
	s32 *stack = (s32*)gf_malloc(sizeof(s32));
	*stack = -1;
	gf_node_set_callback_function(node, TraverseLOD);
	gf_node_set_private(node, stack);
}

#endif //(GPAC_DISABLE_3D)


#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
