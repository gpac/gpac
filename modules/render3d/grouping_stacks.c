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

struct og_pos
{
	Fixed priority;
	u32 position;
};
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
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
static void RenderOrderedGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 i, count;
	struct og_pos *priorities;
	OrderedGroupStack *ogs = (OrderedGroupStack *) gf_node_get_private(node);
	M_OrderedGroup *og = (M_OrderedGroup *)node;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (is_destroy) {
		DeleteGroupingNode((GroupingNode *)ogs);
		if (ogs->positions) free(ogs->positions);
		free(ogs);
		return;
	}
	if (!og->order.count) {
		grouping_traverse((GroupingNode*)ogs, eff, NULL);
		return;
	}

	/*check whether the OrderedGroup node has changed*/
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		if (ogs->positions) free(ogs->positions);
		count = gf_node_list_get_count(og->children);
		priorities = (struct og_pos*)malloc(sizeof(struct og_pos)*count);
		for (i=0; i<count; i++) {
			priorities[i].position = i;
			priorities[i].priority = (i<og->order.count) ? og->order.vals[i] : 0;
		}
		qsort(priorities, count, sizeof(struct og_pos), compare_priority);

		ogs->positions = (u32*)malloc(sizeof(u32) * count);
		for (i=0; i<count; i++) ogs->positions[i] = priorities[i].position;
		free(priorities);
	}
	grouping_traverse((GroupingNode*)ogs, eff, ogs->positions);
}

void R3D_InitOrderedGroup(Render3D *sr, GF_Node *node)
{
	OrderedGroupStack *ptr;
	GF_SAFEALLOC(ptr, OrderedGroupStack);

	SetupGroupingNode((GroupingNode*)ptr, sr->compositor, node, & ((M_OrderedGroup *)node)->children);
	gf_node_set_private(node, ptr);
	gf_node_set_callback_function(node, RenderOrderedGroup);
}

static void RenderSwitch(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 i;
	GF_ChildNodeItem *children;
	s32 whichChoice;
	Bool prev_switch;
	s32 *last_switch = (s32 *)gf_node_get_private(node);
	RenderEffect3D *eff; 
	
	if (is_destroy) {
		free(last_switch);
		return;
	}

	eff = (RenderEffect3D *)rs;
	gf_node_dirty_clear(node, 0);
	prev_switch = eff->trav_flags;

	/*WARNING: X3D/MPEG4 NOT COMPATIBLE*/
	if (gf_node_get_tag(node)==TAG_MPEG4_Switch) {
		children = ((M_Switch *)node)->choice;
		whichChoice = ((M_Switch *)node)->whichChoice;
	} else {
		children = ((X_Switch *)node)->children;
		whichChoice = ((X_Switch *)node)->whichChoice;
	}

	/*check changes in choice field*/
	if (*last_switch != whichChoice) {
		GF_ChildNodeItem *l = children;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		/*deactivation must be signaled because switch may contain audio nodes (I hate this spec!!!)*/
		i = 0;
		while (l) {
			if ((s32) i!=whichChoice) gf_node_render(l->node, eff);
			i++;
			l = l->next;
		}
		eff->trav_flags &= ~GF_SR_TRAV_SWITCHED_OFF;
		*last_switch = whichChoice;
	}

	eff->trav_flags = prev_switch;
	if (whichChoice>=0) {
		gf_node_render( gf_node_list_get_child(children, whichChoice), eff);
	}
}

void R3D_InitSwitch(Render3D *sr, GF_Node *node)
{
	s32 *last_switch = (s32*)malloc(sizeof(s32));
	*last_switch = -1;
	gf_node_set_private(node, last_switch);
	gf_node_set_callback_function(node, RenderSwitch);
}

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
	GF_ColorMatrix cmat;
} ColorTransformStack;

/*ColorTransform*/
static void RenderColorTransform(GF_Node *node, void *rs, Bool is_destroy)
{
	M_ColorTransform *tr = (M_ColorTransform *)node;
	ColorTransformStack *ptr = (ColorTransformStack  *)gf_node_get_private(node);
	RenderEffect3D *eff;

	if (is_destroy) {
		DeleteGroupingNode((GroupingNode *)ptr);
		free(ptr);
		return;
	}
	eff = (RenderEffect3D *) rs;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		gf_cmx_set(&ptr->cmat, 
			tr->mrr , tr->mrg, tr->mrb, tr->mra, tr->tr, 
			tr->mgr , tr->mgg, tr->mgb, tr->mga, tr->tg, 
			tr->mbr, tr->mbg, tr->mbb, tr->mba, tr->tb, 
			tr->mar, tr->mag, tr->mab, tr->maa, tr->ta); 
	}

	/*note we don't clear dirty flag, this is done in traversing*/
	if (ptr->cmat.identity) {
		grouping_traverse((GroupingNode *) ptr, eff, NULL);
	} else {
		GF_ColorMatrix gf_cmx_bck;
		Bool prev_cmat = !eff->color_mat.identity;
		if (prev_cmat) {
			gf_cmx_copy(&gf_cmx_bck, &eff->color_mat);
			gf_cmx_multiply(&eff->color_mat, &ptr->cmat);
		} else {
			gf_cmx_copy(&eff->color_mat, &ptr->cmat);
		}
		grouping_traverse((GroupingNode *) ptr, eff, NULL);
		/*restore effects*/
		if (prev_cmat) gf_cmx_copy(&eff->color_mat, &gf_cmx_bck);
		else eff->color_mat.identity = 1;
	}
}

void R3D_InitColorTransform(Render3D *sr, GF_Node *node)
{
	ColorTransformStack *stack = (ColorTransformStack *)malloc(sizeof(ColorTransformStack));
	SetupGroupingNode((GroupingNode *)stack, sr->compositor, node, & ((M_ColorTransform *)node)->children);
	gf_cmx_init(&stack->cmat);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderColorTransform);
}

static void RenderGroup(GF_Node *node, void *rs, Bool is_destroy)
{
	GroupingNode *group = (GroupingNode *) gf_node_get_private(node);
	if (is_destroy) {
		DestroyBaseGrouping(node);
	} else {
		grouping_traverse(group, (RenderEffect3D*)rs, NULL);
	}
}
void R3D_InitGroup(Render3D *sr, GF_Node *node)
{
	GroupingNode *stack = (GroupingNode *)malloc(sizeof(GroupingNode));
	SetupGroupingNode(stack, sr->compositor, node, & ((M_Group *)node)->children);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderGroup);
}

void RenderCollision(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 collide_flags;
	SFVec3f last_point;
	Fixed last_dist;
	M_Collision *col = (M_Collision *)node;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	GroupingNode *group = (GroupingNode *) gf_node_get_private(node);

	if (is_destroy) {
		DestroyBaseGrouping(node);
		return;
	}

	if (eff->traversing_mode != TRAVERSE_COLLIDE) {
		grouping_traverse(group, eff, NULL);
	} else if (col->collide) {

		collide_flags = eff->camera->collide_flags;
		last_dist = eff->camera->collide_dist;
		eff->camera->collide_flags &= 0;
		eff->camera->collide_dist = FIX_MAX;
		last_point = eff->camera->collide_point;

		if (col->proxy) {
			/*always check bounds to update any dirty node*/
			eff->traversing_mode = TRAVERSE_GET_BOUNDS;
			gf_node_render(col->proxy, rs);

			eff->traversing_mode = TRAVERSE_COLLIDE;
			gf_node_render(col->proxy, rs);
		} else {
			grouping_traverse(group, (RenderEffect3D*)rs, NULL);
		}
		if (eff->camera->collide_flags & CF_COLLISION) {
			col->collideTime = gf_node_get_scene_time(node);
			gf_node_event_out_str(node, "collideTime");
			/*if not closer restore*/
			if (collide_flags && (last_dist<eff->camera->collide_dist)) {
				eff->camera->collide_flags = collide_flags;
				eff->camera->collide_dist = last_dist;
				eff->camera->collide_point = last_point;
			}
		} else {
			eff->camera->collide_flags = collide_flags;
			eff->camera->collide_dist = last_dist;
		}
	}
}

void R3D_InitCollision(Render3D *sr, GF_Node *node)
{
	GroupingNode *stack = (GroupingNode *)malloc(sizeof(GroupingNode));
	SetupGroupingNode(stack, sr->compositor, node, & ((M_Group *)node)->children);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderCollision);
}

/*for transform, transform2D & transformMatrix2D*/
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK
	GF_Matrix mx;
	Bool has_scale;
} TransformStack;

static void DestroyTransform(GF_Node *n)
{
	TransformStack *ptr = (TransformStack *)gf_node_get_private(n);
	DeleteGroupingNode((GroupingNode *)ptr);
	free(ptr);
}

static void NewTransformStack(Render3D *sr, GF_Node *node, GF_ChildNodeItem **children)
{
	TransformStack *st;
	GF_SAFEALLOC(st, TransformStack);

	gf_mx_init(st->mx);
	SetupGroupingNode((GroupingNode *)st, sr->compositor, node, children);
	gf_node_set_private(node, st);
}

#define TRANS_PUSH_MX	\
		if (eff->traversing_mode == TRAVERSE_SORT) {	\
			VS3D_PushMatrix(eff->surface); \
			VS3D_MultMatrix(eff->surface, st->mx.m);	\
		}	\

#define TRANS_POP_MX	if (eff->traversing_mode==TRAVERSE_SORT) VS3D_PopMatrix(eff->surface);

static void RenderTransform(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	TransformStack *st = (TransformStack *)gf_node_get_private(n);
	M_Transform *tr = (M_Transform *)n;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (is_destroy) {
		DestroyTransform(n);
		return;
	}

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

	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_matrix(&eff->model_matrix, &st->mx);

	TRANS_PUSH_MX
	
	/*note we don't clear dirty flag, this is done in traversing*/
	grouping_traverse((GroupingNode *) st, eff, NULL);

	TRANS_POP_MX	

	gf_mx_copy(eff->model_matrix, gf_mx_bckup);
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) gf_mx_apply_bbox(&st->mx, &eff->bbox);
}

void R3D_InitTransform(Render3D *sr, GF_Node *node)
{
	NewTransformStack(sr, node, & ((M_Transform *)node)->children);
	gf_node_set_callback_function(node, RenderTransform);

}

static void RenderTransform2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	M_Transform2D *tr = (M_Transform2D *)node;
	TransformStack *st = (TransformStack*)gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *) rs;

	if (is_destroy) {
		DestroyTransform(node);
		return;
	}

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		GF_Matrix2D mx;
		gf_mx2d_init(mx);
		if ((tr->scale.x != FIX_ONE) || (tr->scale.y != FIX_ONE)) 
			gf_mx2d_add_scale_at(&mx, tr->scale.x, tr->scale.y, 0, 0, tr->scaleOrientation);
		if (tr->rotationAngle) 
			gf_mx2d_add_rotation(&mx, tr->center.x, tr->center.y, tr->rotationAngle);
		if (tr->translation.x || tr->translation.y) 
			gf_mx2d_add_translation(&mx, tr->translation.x, tr->translation.y);

		st->has_scale = ((tr->scale.x != FIX_ONE) || (tr->scale.y != FIX_ONE)) ? 1 : 0;
		gf_mx_from_mx2d(&st->mx, &mx);
	}

	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_matrix(&eff->model_matrix, &st->mx);

	TRANS_PUSH_MX

	/*note we don't clear dirty flag, this is done in traversing*/
	grouping_traverse((GroupingNode *) st, eff, NULL);

	TRANS_POP_MX
	
	gf_mx_copy(eff->model_matrix, gf_mx_bckup);

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) 
		gf_mx_apply_bbox(&st->mx, &eff->bbox);
}
void R3D_InitTransform2D(Render3D *sr, GF_Node *node)
{
	NewTransformStack(sr, node, & ((M_Transform2D *)node)->children);
	gf_node_set_callback_function(node, RenderTransform2D);
}

void TM2D_GetMatrix(GF_Node *n, GF_Matrix *mx)
{
	GF_Matrix2D mat;
	M_TransformMatrix2D *tm = (M_TransformMatrix2D*)n;
	gf_mx2d_init(mat);
	mat.m[0] = tm->mxx; mat.m[1] = tm->mxy; mat.m[2] = tm->tx;
	mat.m[3] = tm->myx; mat.m[4] = tm->myy; mat.m[5] = tm->ty;
	gf_mx_from_mx2d(mx, &mat);
}


/*TransformMatrix2D*/
static void RenderTransformMatrix2D(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	M_TransformMatrix2D *tm = (M_TransformMatrix2D*)node;
	TransformStack *st = (TransformStack *) gf_node_get_private(node);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (is_destroy) {
		DestroyTransform(node);
		return;
	}

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		TM2D_GetMatrix(node, &st->mx);
		st->has_scale = ((tm->mxx != FIX_ONE) || (tm->myy != FIX_ONE) || tm->mxy || tm->myx) ? 1 : 0;
	}

	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_matrix(&eff->model_matrix, &st->mx);

	TRANS_PUSH_MX

	/*note we don't clear dirty flag, this is done in traversing*/
	grouping_traverse((GroupingNode *)st, eff, NULL);

	TRANS_POP_MX
	
	gf_mx_copy(eff->model_matrix, gf_mx_bckup);

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) gf_mx_apply_bbox(&st->mx, &eff->bbox);
}


void R3D_InitTransformMatrix2D(Render3D *sr, GF_Node *node)
{
	NewTransformStack(sr, node, & ((M_TransformMatrix2D *)node)->children);
	gf_node_set_callback_function(node, RenderTransformMatrix2D);
}

static void RenderBillboard(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_Matrix gf_mx_bckup;
	TransformStack *st = (TransformStack *)gf_node_get_private(n);
	M_Billboard *bb = (M_Billboard *)n;
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (is_destroy) {
		DestroyTransform(n);
		return;
	}

	/*can't cache the matrix here*/
	gf_mx_init(st->mx);
	if (eff->camera->is_3D) {
		SFVec3f z, axis;
		Fixed axis_len;
		SFVec3f user_pos = eff->camera->position;

		gf_mx_apply_vec(&eff->model_matrix, &user_pos);
		gf_vec_norm(&user_pos);
		axis = bb->axisOfRotation;
		axis_len = gf_vec_len(axis);
		if (axis_len<FIX_EPSILON) {
			SFVec3f x, y, t;
			/*get user's right in local coord*/
			gf_vec_diff(t, eff->camera->position, eff->camera->target);
			gf_vec_norm(&t);
			x = gf_vec_cross(eff->camera->up, t);
			gf_vec_norm(&x);
			gf_mx_rotate_vector(&eff->model_matrix, &x);
			gf_vec_norm(&x);
			/*get user's up in local coord*/
			y = eff->camera->up;
			gf_mx_rotate_vector(&eff->model_matrix, &y);
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

			z.x = z.y = 0; z.z = FIX_ONE;
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

	gf_mx_copy(gf_mx_bckup, eff->model_matrix);
	gf_mx_add_matrix(&eff->model_matrix, &st->mx);
	
	TRANS_PUSH_MX

	/*note we don't clear dirty flag, this is done in traversing*/
	grouping_traverse((GroupingNode *) st, eff, NULL);

	TRANS_POP_MX

	gf_mx_copy(eff->model_matrix, gf_mx_bckup);

	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) gf_mx_apply_bbox(&st->mx, &eff->bbox);
}

void R3D_InitBillboard(Render3D *sr, GF_Node *node)
{
	NewTransformStack(sr, node, & ((M_Billboard *)node)->children);
	gf_node_set_callback_function(node, RenderBillboard);

}

static void RenderLOD(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_ChildNodeItem *children;
	MFFloat *ranges;
	SFVec3f pos, usr;
	u32 which_child, nb_children;
	Fixed dist;
	Bool do_all;
	GF_Matrix mx;
	SFVec3f center;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	s32 *prev_child = (s32 *)gf_node_get_private(node);

	if (is_destroy) {
		free(prev_child);
		return;
	}



	/*WARNING: X3D/MPEG4 NOT COMPATIBLE*/
	if (gf_node_get_tag(node) == TAG_MPEG4_LOD) {
		children = ((M_LOD *) node)->level;
		ranges = &((M_LOD *) node)->range;
		center = ((M_LOD *) node)->center;
	} else {
		children = ((X_LOD *) node)->children;
		ranges = &((X_LOD *) node)->range;
		center = ((X_LOD *) node)->center;
	}

	if (!children) return;
	nb_children = gf_node_list_get_count(children);
	
	/*can't cache the matric here*/
	usr = eff->camera->position;
	pos = center;
	gf_mx_copy(mx, eff->model_matrix);
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
	
	if (do_all) {
		u32 i;
		Bool prev_flags = eff->trav_flags;
		GF_ChildNodeItem *l = children;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		i=0;
		while (l) {
			if (i!=which_child) gf_node_render(l->node, rs);
			l = l->next;
		}
		eff->trav_flags = prev_flags;
	}
	gf_node_render(gf_node_list_get_child(children, which_child), rs);
}

void R3D_InitLOD(Render3D *sr, GF_Node *node)
{
	s32 *stack = (s32*)malloc(sizeof(s32));
	*stack = -1;
	gf_node_set_callback_function(node, RenderLOD);
	gf_node_set_private(node, stack);
}

/*currently static group use the same render as all grouping nodes, without any opts.
since we already do a lot of caching (lights, sensors) at the grouping node level, I'm not sure
we could get anything better trying to optimize the rest, since what is not cached in the base grouping
node always involves frustum culling, and caching that would require caching partial model matrix (from
the static group to each leaf)*/
#if 0
static void RenderStaticGroup(GF_Node *node, void *rs)
{
	GroupingNode *group = (GroupingNode *) gf_node_get_private(node);
	grouping_traverse(group, (RenderEffect3D*)rs, NULL);
}
#endif

void R3D_InitStaticGroup(Render3D *sr, GF_Node *node)
{
	GroupingNode *stack = (GroupingNode *)malloc(sizeof(GroupingNode));
	SetupGroupingNode(stack, sr->compositor, node, & ((X_StaticGroup *)node)->children);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderGroup);
}
