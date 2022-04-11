/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

#ifndef GPAC_DISABLE_VRML

typedef struct
{
	s32 last_switch;
} SwitchStack;

static void TraverseSwitch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_ChildNodeItem *l;
	u32 i;
	Bool prev_switch;
	GF_ChildNodeItem *children;
	s32 whichChoice;
	GF_Node *child;
	SwitchStack *st = (SwitchStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state;
	tr_state = (GF_TraverseState *)rs;
	children = NULL;

	whichChoice = -1;
	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		gf_free(st);
		return;
	}
	/*WARNING: X3D/MPEG4 NOT COMPATIBLE*/
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Switch:
		children = ((M_Switch *)node)->choice;
		whichChoice = ((M_Switch *)node)->whichChoice;
		break;
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Switch:
		children = ((X_Switch *)node)->children;
		whichChoice = ((X_Switch *)node)->whichChoice;
		break;
#endif
	}

	if (tr_state->traversing_mode!=TRAVERSE_GET_BOUNDS) {
		prev_switch = tr_state->switched_off;
		/*check changes in choice field*/
		if ((gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) || (st->last_switch != whichChoice) ) {
			tr_state->switched_off = 1;
			i=0;
			l = children;
			while (l) {
				//			if ((s32) i!=whichChoice) gf_node_traverse(l->node, tr_state);
				if ((s32) i == st->last_switch) gf_node_traverse(l->node, tr_state);
				l = l->next;
				i++;
			}
			tr_state->switched_off = 0;
			st->last_switch = whichChoice;
		}

		gf_node_dirty_clear(node, 0);

		/*no need to check for sensors since a sensor is active for the whole parent group, that is for switch itself
		CSQ: switch cannot be used to switch sensors, too bad...*/
		tr_state->switched_off = prev_switch;
	}

	if (!children) return;
	if (whichChoice==-2) {
#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->autostereo_type) {
			u32 idx;
			u32 count = gf_node_list_get_count(children);
			/*this should be a bit more subtle (reusing views if missing, ...)...*/
			idx = tr_state->visual->current_view % count;

			child = (GF_Node*)gf_node_list_get_child(children, idx);
			gf_node_traverse(child, tr_state);
			return;
		} else
#endif //GPAC_DISABLE_3D
		{
			/*fallback to first view*/
			whichChoice=0;
		}
	}
	if (whichChoice>=0) {
		child = (GF_Node*)gf_node_list_get_child(children, whichChoice);
		gf_node_traverse(child, tr_state);
	}
}

void compositor_init_switch(GF_Compositor *compositor, GF_Node *node)
{
	SwitchStack *st = (SwitchStack *)gf_malloc(sizeof(SwitchStack));
	st->last_switch = -1;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, TraverseSwitch);
}


/*transform2D*/
typedef struct
{
	GROUPING_MPEG4_STACK_2D
	GF_Matrix2D mat;
	u8 is_identity;
	u8 is_null;
	u8 is_untransform;
} Transform2DStack;

static void traverse_transform(GF_Node *node, Transform2DStack *stack, GF_TraverseState *tr_state)
{
	if (stack->is_null) return;

	/*note we don't clear dirty flag, this is done in traversing*/
	if (stack->is_identity) {
		group_2d_traverse(node, (GroupingNode2D *)stack, tr_state);
	}
#ifndef GPAC_DISABLE_3D
	else if (tr_state->visual->type_3d) {
		GF_Matrix mx_bckup;
		gf_mx_copy(mx_bckup, tr_state->model_matrix);

		gf_mx_add_matrix_2d(&tr_state->model_matrix, &stack->mat);
		group_2d_traverse(node, (GroupingNode2D *)stack, tr_state);
		gf_mx_copy(tr_state->model_matrix, mx_bckup);
	}
#endif
	else {
		GF_Matrix2D bckup;
		gf_mx2d_copy(bckup, tr_state->transform);
		gf_mx2d_pre_multiply(&tr_state->transform, &stack->mat);

		group_2d_traverse(node, (GroupingNode2D *)stack, tr_state);

		gf_mx2d_copy(tr_state->transform, bckup);
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_mx2d_apply_rect(&stack->mat, &tr_state->bounds);
	}
}

void TraverseUntransformEx(GF_Node *node, void *rs, GroupingNode2D *grp);

static void TraverseTransform2D(GF_Node *node, void *rs, Bool is_destroy)
{
	M_Transform2D *tr = (M_Transform2D *)node;
	Transform2DStack *ptr = (Transform2DStack *)gf_node_get_private(node);
	GF_TraverseState *tr_state;

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_2d_destroy(node, (GroupingNode2D*)ptr);
		gf_free(ptr);
		return;
	}

	tr_state = (GF_TraverseState *) rs;

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		gf_mx2d_init(ptr->mat);
		ptr->is_identity = 1;
		if ((tr->scale.x != FIX_ONE) || (tr->scale.y != FIX_ONE)) {
			gf_mx2d_add_scale_at(&ptr->mat, tr->scale.x, tr->scale.y, 0, 0, tr->scaleOrientation);
			ptr->is_identity = 0;
		}
		if (tr->rotationAngle) {
			gf_mx2d_add_rotation(&ptr->mat, tr->center.x, tr->center.y, tr->rotationAngle);
			ptr->is_identity = 0;
		}
		if (tr->translation.x || tr->translation.y) {
			ptr->is_identity = 0;
			gf_mx2d_add_translation(&ptr->mat, tr->translation.x, tr->translation.y);
		}
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
		ptr->is_null = (!tr->scale.x || !tr->scale.y) ? 1 : 0;
	}

	if (ptr->is_untransform) {
		TraverseUntransformEx(node, tr_state, (GroupingNode2D *)ptr);
	} else {
		traverse_transform(node, ptr, tr_state);
	}
}
void transform2d_set_untransform(GF_Node *node)
{
	Transform2DStack *stack;
	if (gf_node_get_tag(node)!=TAG_MPEG4_Transform2D) return;
	stack = (Transform2DStack *)gf_node_get_private(node);
	stack->is_untransform = 1;
}

void compositor_init_transform2d(GF_Compositor *compositor, GF_Node *node)
{
	Transform2DStack *stack;
	GF_SAFEALLOC(stack, Transform2DStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate transform2d stack\n"));
		return;
	}

	gf_mx2d_init(stack->mat);
	stack->is_identity = 1;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseTransform2D);
}

void tr_mx2d_get_matrix(GF_Node *n, GF_Matrix2D *mat)
{
	M_TransformMatrix2D *tr = (M_TransformMatrix2D*)n;
	gf_mx2d_init(*mat);
	mat->m[0] = tr->mxx;
	mat->m[1] = tr->mxy;
	mat->m[2] = tr->tx;
	mat->m[3] = tr->myx;
	mat->m[4] = tr->myy;
	mat->m[5] = tr->ty;
}


/*TransformMatrix2D*/
static void TraverseTransformMatrix2D(GF_Node *node, void *rs, Bool is_destroy)
{
	Transform2DStack *ptr = (Transform2DStack *) gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_2d_destroy(node, (GroupingNode2D*)ptr);
		gf_free(ptr);
		return;
	}

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		M_TransformMatrix2D *tr = (M_TransformMatrix2D*)node;
		tr_mx2d_get_matrix(node, &ptr->mat);
		if ((tr->mxx==FIX_ONE) && (tr->mxy==0) && (tr->tx==0)
		        && (tr->myx==0) && (tr->myy==FIX_ONE) && (tr->ty==0) )
			ptr->is_identity = 1;
		else
			ptr->is_identity = 0;

		ptr->is_null = ( (!ptr->mat.m[0] && !ptr->mat.m[1]) || (!ptr->mat.m[3] && !ptr->mat.m[4]) ) ? 1 : 0;
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	}
	traverse_transform(node, ptr, tr_state);
}


void compositor_init_transformmatrix2d(GF_Compositor *compositor, GF_Node *node)
{
	Transform2DStack *stack;
	GF_SAFEALLOC(stack, Transform2DStack);
	if (!stack) return;

	gf_mx2d_init(stack->mat);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseTransformMatrix2D);
}


typedef struct
{
	GROUPING_MPEG4_STACK_2D
	GF_ColorMatrix cmat;
} ColorTransformStack;

/*ColorTransform*/
static void TraverseColorTransform(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool c_changed;
	M_ColorTransform *tr = (M_ColorTransform *)node;
	ColorTransformStack *ptr = (ColorTransformStack  *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	Bool prev_inv;

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_2d_destroy(node, (GroupingNode2D*)ptr);
		gf_free(ptr);
		return;
	}
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		group_2d_traverse(node, (GroupingNode2D *) ptr, tr_state);
		return;
	}

	prev_inv = tr_state->invalidate_all;
	c_changed = 0;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		gf_cmx_set(&ptr->cmat,
		           tr->mrr , tr->mrg, tr->mrb, tr->mra, tr->tr,
		           tr->mgr , tr->mgg, tr->mgb, tr->mga, tr->tg,
		           tr->mbr, tr->mbg, tr->mbb, tr->mba, tr->tb,
		           tr->mar, tr->mag, tr->mab, tr->maa, tr->ta);
		c_changed = 1;
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	}

	if ((tr_state->traversing_mode==TRAVERSE_SORT)
	        && !tr->maa && !tr->mar && !tr->mag && !tr->mab && !tr->ta)
		return;

	/*if modified redraw all nodes*/
	if (c_changed) tr_state->invalidate_all = 1;

	/*note we don't clear dirty flag, this is done in traversing*/
	if (ptr->cmat.identity) {
		group_2d_traverse(node, (GroupingNode2D *) ptr, tr_state);
	} else {
		GF_ColorMatrix gf_cmx_bck;
		gf_cmx_copy(&gf_cmx_bck, &tr_state->color_mat);
		gf_cmx_multiply(&tr_state->color_mat, &ptr->cmat);


		group_2d_traverse(node, (GroupingNode2D *) ptr, tr_state);
		/*restore traversing state*/
		gf_cmx_copy(&tr_state->color_mat, &gf_cmx_bck);
	}
	tr_state->invalidate_all = prev_inv;
}

void compositor_init_colortransform(GF_Compositor *compositor, GF_Node *node)
{
	ColorTransformStack *stack;
	GF_SAFEALLOC(stack, ColorTransformStack);
	if (!stack) return;

	gf_cmx_init(&stack->cmat);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseColorTransform);
}


struct og_pos
{
	Fixed priority;
	u32 position;
};
typedef struct
{
	GROUPING_MPEG4_STACK_2D
	u32 *positions;
} OrderedGroupStack;


static s32 compare_priority(const void* elem1, const void* elem2)
{
	struct og_pos *p1, *p2;
	p1 = (struct og_pos *)elem1;
	p2 = (struct og_pos *)elem2;
	if (p1->priority < p2->priority) return -1;
	if (p1->priority > p2->priority) return 1;
	return 0;
}


static void TraverseOrderedGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 i, count;
	struct og_pos *priorities;
	Bool invalidate_backup;
	OrderedGroupStack *stack = (OrderedGroupStack *) gf_node_get_private(node);
	M_OrderedGroup *og = (M_OrderedGroup *) node;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		group_2d_destroy(node, (GroupingNode2D*)stack);
		if (stack->positions) gf_free(stack->positions);
		gf_free(stack);
		return;
	}

	if (!og->order.count || (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) ) {
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
		group_2d_traverse(node, (GroupingNode2D*)stack, tr_state);
		return;
	}

	invalidate_backup = tr_state->invalidate_all;
	/*check whether the OrderedGroup node has changed*/
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		if (stack->positions) gf_free(stack->positions);
		count = gf_node_list_get_count(og->children);
		priorities = (struct og_pos*)gf_malloc(sizeof(struct og_pos)*count);
		for (i=0; i<count; i++) {
			priorities[i].position = i;
			priorities[i].priority = (i<og->order.count) ? og->order.vals[i] : 0;
		}
		qsort(priorities, count, sizeof(struct og_pos), compare_priority);

		stack->positions = (u32*)gf_malloc(sizeof(u32) * count);
		for (i=0; i<count; i++) stack->positions[i] = priorities[i].position;
		gf_free(priorities);

		tr_state->invalidate_all = 1;
		gf_node_dirty_clear(node, GF_SG_NODE_DIRTY);
	}
	group_2d_traverse_with_order(node, (GroupingNode2D*)stack, tr_state, stack->positions);
	tr_state->invalidate_all = invalidate_backup;
}

void compositor_init_orderedgroup(GF_Compositor *compositor, GF_Node *node)
{
	OrderedGroupStack *ptr;
	GF_SAFEALLOC(ptr, OrderedGroupStack);
	if (!ptr) return;
	
	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, TraverseOrderedGroup);
}

#endif	/*GPAC_DISABLE_VRML*/

