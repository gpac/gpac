/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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

#include "stacks2d.h"
#include "visualsurface2d.h"
/*for anchor processing, which needs to be filtered at the inline scene level*/
#include <gpac/internal/terminal_dev.h>

typedef struct
{
	s32 last_switch;
} SwitchStack;

static void DestroySwitch(GF_Node *node)
{
	SwitchStack *st = (SwitchStack *)gf_node_get_private(node);
	free(st);
}
static void RenderSwitch(GF_Node *node, void *rs)
{
	u32 i, count;
	Bool prev_switch;
	GF_List *children;
	s32 whichChoice;
	GF_Node *child;
	SwitchStack *st = (SwitchStack *)gf_node_get_private(node);
	RenderEffect2D *eff; 
	eff = (RenderEffect2D *)rs;


	if (gf_node_get_name(node)) {
		node = node;
	}
	/*WARNING: X3D/MPEG4 NOT COMPATIBLE*/
	if (gf_node_get_tag(node)==TAG_MPEG4_Switch) {
		children = ((M_Switch *)node)->choice;
		whichChoice = ((M_Switch *)node)->whichChoice;
	} else {
		children = ((X_Switch *)node)->children;
		whichChoice = ((X_Switch *)node)->whichChoice;
	}
	count = gf_list_count(children);

	prev_switch = eff->trav_flags;
	/*check changes in choice field*/
	if ((gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) || (st->last_switch != whichChoice) ) {
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		/*deactivation must be signaled because switch may contain audio nodes...*/
		for (i=0; i<count; i++) {
			if ((s32) i==whichChoice) continue;
			child = gf_list_get(children, i);
			gf_node_render(child, eff);
		}
		eff->trav_flags &= ~GF_SR_TRAV_SWITCHED_OFF;
		st->last_switch = whichChoice;
	}

	gf_node_dirty_clear(node, 0);

	/*no need to check for sensors since a sensor is active for the whole parent group, that is for switch itself
	CSQ: switch cannot be used to switch sensors, too bad...*/
	eff->trav_flags = prev_switch;

	if (whichChoice>=0) {
		child = gf_list_get(children, whichChoice);
		gf_node_render(child, eff);
	}
}

void R2D_InitSwitch(Render2D *sr, GF_Node *node)
{
	SwitchStack *st = malloc(sizeof(SwitchStack));
	st->last_switch = -1;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroySwitch);
	gf_node_set_render_function(node, RenderSwitch);
}


/*transform2D*/
static void DestroyTransform2D(GF_Node *n)
{
	Transform2DStack *ptr = (Transform2DStack *)gf_node_get_private(n);
	DeleteGroupingNode2D((GroupingNode2D *)ptr);
	free(ptr);
}

static void RenderTransform2D(GF_Node *node, void *rs)
{
	GF_Matrix2D bckup;
	M_Transform2D *tr = (M_Transform2D *)node;
	Transform2DStack *ptr = (Transform2DStack *)gf_node_get_private(node);
	RenderEffect2D *eff;
	
	eff = (RenderEffect2D *) rs;

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
	}

	/*note we don't clear dirty flag, this is done in traversing*/
	if (ptr->is_identity) {
		group2d_traverse((GroupingNode2D *)ptr, tr->children, eff);
	} else {
		gf_mx2d_copy(bckup, eff->transform);
		gf_mx2d_copy(eff->transform, ptr->mat);
		gf_mx2d_add_matrix(&eff->transform, &bckup);
		group2d_traverse((GroupingNode2D *)ptr, tr->children, eff);
		gf_mx2d_copy(eff->transform, bckup);
	}
}

void R2D_InitTransform2D(Render2D *sr, GF_Node *node)
{
	Transform2DStack *stack = malloc(sizeof(Transform2DStack));
	SetupGroupingNode2D((GroupingNode2D *)stack, sr, node);
	gf_mx2d_init(stack->mat);
	stack->is_identity = 1;
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyTransform2D);
	gf_node_set_render_function(node, RenderTransform2D);
}

void TM2D_GetMatrix(GF_Node *n, GF_Matrix2D *mat)
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
static void RenderTransformMatrix2D(GF_Node *node, void *rs)
{
	GF_Matrix2D bckup;
	M_TransformMatrix2D *tr = (M_TransformMatrix2D*)node;
	Transform2DStack *ptr = (Transform2DStack *) gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		TM2D_GetMatrix(node, &ptr->mat);
		if ((tr->mxx==FIX_ONE) && (tr->mxy==0) && (tr->tx==0)
			&& (tr->myx==0) && (tr->myy==FIX_ONE) && (tr->ty==0) )
			ptr->is_identity = 1;
		else
			ptr->is_identity = 0;
	}

	/*note we don't clear dirty flag, this is done in traversing*/
	if (ptr->is_identity) {
		group2d_traverse((GroupingNode2D *)ptr, tr->children, eff);
	} else {
		gf_mx2d_copy(bckup, eff->transform);
		gf_mx2d_copy(eff->transform, ptr->mat);
		gf_mx2d_add_matrix(&eff->transform, &bckup);
		group2d_traverse((GroupingNode2D *)ptr, tr->children, eff);
		gf_mx2d_copy(eff->transform, bckup);
	}
}


void R2D_InitTransformMatrix2D(Render2D *sr, GF_Node *node)
{
	Transform2DStack *stack = malloc(sizeof(Transform2DStack));
	SetupGroupingNode2D((GroupingNode2D *)stack, sr, node);
	gf_mx2d_init(stack->mat);
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyTransform2D);
	gf_node_set_render_function(node, RenderTransformMatrix2D);
}


typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D
	GF_ColorMatrix cmat;
} ColorTransformStack;

static void DestroyColorTransform(GF_Node *n)
{
	ColorTransformStack *ptr = (ColorTransformStack *)gf_node_get_private(n);
	DeleteGroupingNode2D((GroupingNode2D *)ptr);
	free(ptr);
}

/*ColorTransform*/
static void RenderColorTransform(GF_Node *node, void *rs)
{
	Bool c_changed;
	M_ColorTransform *tr = (M_ColorTransform *)node;
	ColorTransformStack *ptr = (ColorTransformStack  *)gf_node_get_private(node);
	RenderEffect2D *eff;
	eff = (RenderEffect2D *) rs;

	c_changed = 0;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		gf_cmx_set(&ptr->cmat, 
			tr->mrr , tr->mrg, tr->mrb, tr->mra, tr->tr, 
			tr->mgr , tr->mgg, tr->mgb, tr->mga, tr->tg, 
			tr->mbr, tr->mbg, tr->mbb, tr->mba, tr->tb, 
			tr->mar, tr->mag, tr->mab, tr->maa, tr->ta); 
		c_changed = 1;
	}
	/*note we don't clear dirty flag, this is done in traversing*/
	if (ptr->cmat.identity) {
		group2d_traverse((GroupingNode2D *) ptr, tr->children, eff);
	} else {
		GF_ColorMatrix gf_cmx_bck;
		Bool prev_inv = eff->invalidate_all;
		/*if modified redraw all nodes*/
		if (c_changed) eff->invalidate_all = 1;
		gf_cmx_copy(&gf_cmx_bck, &eff->color_mat);
		gf_cmx_multiply(&eff->color_mat, &ptr->cmat);
		group2d_traverse((GroupingNode2D *) ptr, tr->children, eff);
		/*restore effects*/
		gf_cmx_copy(&eff->color_mat, &gf_cmx_bck);
		eff->invalidate_all = prev_inv;
	}
}

void R2D_InitColorTransform(Render2D *sr, GF_Node *node)
{
	ColorTransformStack *stack = malloc(sizeof(ColorTransformStack));
	SetupGroupingNode2D((GroupingNode2D *)stack, sr, node);
	gf_cmx_init(&stack->cmat);
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyColorTransform);
	gf_node_set_render_function(node, RenderColorTransform);
}

static void RenderGroup(GF_Node *node, void *rs)
{
	GroupingNode2D *group = (GroupingNode2D *) gf_node_get_private(node);
	group2d_traverse(group, ((M_Group *)node)->children, (RenderEffect2D*)rs);
}

void R2D_InitGroup(Render2D *sr, GF_Node *node)
{
	GroupingNode2D *stack = malloc(sizeof(GroupingNode2D));
	SetupGroupingNode2D(stack, sr, node);
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyBaseGrouping2D);
	gf_node_set_render_function(node, RenderGroup);
}


typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D
	Bool enabled;
	SensorHandler hdl;
} AnchorStack;

static void DestroyAnchor(GF_Node *n)
{
	AnchorStack *st = (AnchorStack*)gf_node_get_private(n);
	R2D_UnregisterSensor(st->compositor, &st->hdl);
	if (st->compositor->interaction_sensors) st->compositor->interaction_sensors--;
	DeleteGroupingNode2D((GroupingNode2D *)st);
	free(st);
}

static void RenderAnchor(GF_Node *node, void *rs)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	M_Anchor *an = (M_Anchor *) node;
	RenderEffect2D *eff = rs;

	/*update enabled state*/
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		st->enabled = 0;
		if (an->url.count && an->url.vals[0].url && strlen(an->url.vals[0].url) )
			st->enabled = 1;
	}
	/*note we don't clear dirty flag, this is done in traversing*/
	group2d_traverse((GroupingNode2D*)st, an->children, eff);
}

static Bool anchor_is_enabled(SensorHandler *sh)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(sh->owner);
	return st->enabled;
}

static Bool OnAnchor(SensorHandler *sh, UserEvent2D *ev, GF_Matrix2D *sensor_matrix)
{
	u32 i;
	GF_Event evt;
	AnchorStack *st = (AnchorStack *) gf_node_get_private(sh->owner);
	M_Anchor *an = (M_Anchor *) sh->owner;

	if (ev->event_type == GF_EVT_MOUSEMOVE) {
		if (st->compositor->user->EventProc) {
			evt.type = GF_EVT_NAVIGATE_INFO;
			evt.navigate.to_url = an->description.buffer;
			if (!evt.navigate.to_url || !strlen(evt.navigate.to_url)) evt.navigate.to_url = an->url.vals[0].url;
			st->compositor->user->EventProc(st->compositor->user->opaque, &evt);
		}
		return 0;
	}

	if (ev->event_type != GF_EVT_LEFTUP) return 0;

	evt.type = GF_EVT_NAVIGATE;
	evt.navigate.param_count = an->parameter.count;
	evt.navigate.parameters = (const char **) an->parameter.vals;
	i=0;
	while (i<an->url.count) {
		evt.navigate.to_url = an->url.vals[i].url;
		if (!evt.navigate.to_url) break;
		/*current scene navigation*/
		if (evt.navigate.to_url[0] == '#') {
			GF_Node *n;
			evt.navigate.to_url++;
			n = gf_sg_find_node_by_name(gf_node_get_graph(sh->owner), (char *) evt.navigate.to_url);
			if (n) {
				switch (gf_node_get_tag(n)) {
				case TAG_MPEG4_Viewport:
					((M_Viewport *)n)->set_bind = 1;
					((M_Viewport *)n)->on_set_bind(n);
					break;
				}
				break;
			}
		} else if (st->compositor->term) {
			if (gf_is_process_anchor(sh->owner, &evt))
				break;
		} else if (st->compositor->user->EventProc) {
			if (st->compositor->user->EventProc(st->compositor->user->opaque, &evt))
				break;
		}

		i++;
	}
	return 0;
}

static void on_activate_anchor(GF_Node *node)
{
	UserEvent2D ev;
	AnchorStack *st = (AnchorStack *) gf_node_get_private(node);
	if (!((M_Anchor *)node)->activate) return;

	ev.event_type = GF_EVT_LEFTUP;
	OnAnchor(&st->hdl, &ev, NULL);
	return;
}

SensorHandler *r2d_anchor_get_handler(GF_Node *n)
{
	AnchorStack *st = (AnchorStack *) gf_node_get_private(n);
	return &st->hdl;
}

void R2D_InitAnchor(Render2D *sr, GF_Node *node)
{
	M_Anchor *an = (M_Anchor *)node;
	AnchorStack *stack = malloc(sizeof(AnchorStack));
	memset(stack, 0, sizeof(AnchorStack));

	SetupGroupingNode2D((GroupingNode2D*)stack, sr, node);

	sr->compositor->interaction_sensors++;

	an->on_activate = on_activate_anchor;
	stack->hdl.IsEnabled = anchor_is_enabled;
	stack->hdl.OnUserEvent = OnAnchor;
	stack->hdl.owner = node;
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyAnchor);
	gf_node_set_render_function(node, RenderAnchor);
}

struct og_pos
{
	Fixed priority;
	u32 position;
};
typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D

	struct og_pos *priorities;
	u32 count;
} OrderedGroupStack;

static void DestroyOrderedGroup(GF_Node *node)
{
	OrderedGroupStack *ptr = (OrderedGroupStack *) gf_node_get_private(node);
	DeleteGroupingNode2D((GroupingNode2D *)ptr);
	if (ptr->priorities) free(ptr->priorities);
	free(ptr);
}

static s32 compare_priority(const void* elem1, const void* elem2)
{
	struct og_pos *p1, *p2;
	p1 = (struct og_pos *)elem1;
	p2 = (struct og_pos *)elem2;
	if (p1->priority < p2->priority) return -1;
	if (p1->priority > p2->priority) return 1;
	return 0;
}


static void RenderOrderedGroup(GF_Node *node, void *rs)
{
	u32 i, count;
	GF_Node *child;
	Bool split_text_backup, invalidate_backup;
	M_OrderedGroup *og;
	u32 count2;
	GF_List *sensor_backup;
	SensorHandler *hsens;
	
	OrderedGroupStack *ogs = (OrderedGroupStack *) gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	og = (M_OrderedGroup *) ogs->owner;

	if (!og->order.count) {
		group2d_traverse((GroupingNode2D*)ogs, og->children, eff);
		return;
	}
	count = gf_list_count(og->children);
	invalidate_backup = eff->invalidate_all;

	/*check whether the OrderedGroup node has changed*/
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {

		if (ogs->priorities) free(ogs->priorities);
		ogs->priorities = malloc(sizeof(struct og_pos)*count);
		for (i=0; i<count; i++) {
			ogs->priorities[i].position = i;
			ogs->priorities[i].priority = (i<og->order.count) ? og->order.vals[i] : 0;
		}
		qsort(ogs->priorities, count, sizeof(struct og_pos), compare_priority);
		eff->invalidate_all = 1;
	}

	sensor_backup = NULL;
	if (gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY) {
		/*rebuild sensor list*/
		if (gf_list_count(ogs->sensors)) {
			gf_list_del(ogs->sensors);
			ogs->sensors = gf_list_new();
		}

		for (i=0; i<count; i++) {
			child = gf_list_get(og->children, ogs->priorities[i].position);
			if (!child || !is_sensor_node(child) ) continue;
			hsens = get_sensor_handler(child);
			if (hsens) gf_list_add(ogs->sensors, hsens);
		}
	}

	/*if we have an active sensor at this level discard all sensors in current render context (cf VRML)*/
	count2 = gf_list_count(ogs->sensors);
	if (count2) {
		sensor_backup = eff->sensors;
		eff->sensors = gf_list_new();
		/*add sensor to effects*/	
		for (i=0; i <count2; i++) {
			SensorHandler *hsens = gf_list_get(ogs->sensors, i);
			effect_add_sensor(eff, hsens, &eff->transform);
		}
	}
	gf_node_dirty_clear(node, 0);

	if (eff->parent == (GroupingNode2D *) ogs) {
		for (i=0; i<count; i++) {
			group2d_start_child((GroupingNode2D *) ogs);
			child = gf_list_get(og->children, ogs->priorities[i].position);
			gf_node_render(child, eff);
			group2d_end_child((GroupingNode2D *) ogs);
		}
	} else {
		split_text_backup = eff->text_split_mode;
		if (count>1) eff->text_split_mode = 0;
		for (i=0; i<count; i++) {
			child = gf_list_get(og->children, ogs->priorities[i].position);
			gf_node_render(child, eff);
		}
		eff->text_split_mode = split_text_backup;
	}

	/*restore effect*/
	invalidate_backup = eff->invalidate_all;
	if (count2) {
		/*destroy current effect list and restore previous*/
		effect_reset_sensors(eff);
		gf_list_del(eff->sensors);
		eff->sensors = sensor_backup;
	}
}

void R2D_InitOrderedGroup(Render2D *sr, GF_Node *node)
{
	OrderedGroupStack *ptr = malloc(sizeof(OrderedGroupStack));
	memset(ptr, 0, sizeof(OrderedGroupStack));

	SetupGroupingNode2D((GroupingNode2D*)ptr, sr, node);
	
	gf_node_set_private(node, ptr);
	gf_node_set_predestroy_function(node, DestroyOrderedGroup);
	gf_node_set_render_function(node, RenderOrderedGroup);
}

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GROUPINGNODESTACK2D

	GF_List *backs;
	GF_List *views;
	Bool first;
	GF_Rect clip;
} Layer2DStack;

static void DestroyLayer2D(GF_Node *node)
{
	Layer2DStack *l2D = (Layer2DStack *) gf_node_get_private(node);
	DeleteGroupingNode2D((GroupingNode2D *)l2D);
	gf_list_del(l2D->backs);
	gf_list_del(l2D->views);
	free(l2D);
}

static void RenderLayer2D(GF_Node *node, void *rs)
{
	u32 i;
	GF_List *prevback, *prevviews;
	GF_Rect clip;
	M_Viewport *vp;
	ChildGroup2D *cg;
	GF_Matrix2D gf_mx2d_bck;
	GroupingNode2D *parent_bck;
	DrawableContext *back_ctx;
	Bool bool_bck;
	DrawableContext *ctx;
	M_Background2D *back;
	M_Layer2D *l = (M_Layer2D *)node;
	Layer2DStack *l2D = (Layer2DStack *) gf_node_get_private(node);
	RenderEffect2D *eff;


	eff = (RenderEffect2D *) rs;
	gf_mx2d_copy(gf_mx2d_bck, eff->transform);
	parent_bck = eff->parent;
	eff->parent = (GroupingNode2D *) l2D;
	gf_mx2d_init(eff->transform);
	bool_bck = eff->draw_background;
	prevback = eff->back_stack;
	prevviews = eff->view_stack;
	eff->back_stack = l2D->backs;
	eff->view_stack = l2D->views;

	if (l2D->first) {
		/*render on back first to register with stack*/
		if (l->background) {
			eff->draw_background = 0;
			gf_node_render((GF_Node*) l->background, eff);
			group2d_reset_children((GroupingNode2D*) l2D);
			eff->draw_background = 1;
		}
		vp = (M_Viewport*)l->viewport;
		if (vp) {
			gf_list_add(l2D->views, vp);
			if (!vp->isBound) {
				vp->isBound = 1;
				gf_node_event_out_str((GF_Node*)vp, "isBound");
			}
		}
	}

	back = NULL;
	if (gf_list_count(l2D->backs) ) back = gf_list_get(l2D->backs, 0);
	vp = NULL;
	if (gf_list_count(l2D->views)) vp = gf_list_get(l2D->views, 0);

	if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&eff->transform, eff->min_hsize, eff->min_hsize);
	l2D->clip = R2D_ClipperToPixelMetrics(eff, l->size);

	/*apply viewport*/
	if (vp) {
		clip = l2D->clip;
		vp_setup((GF_Node *) vp, eff, &clip);
	}


	back_ctx = NULL;
	if (back) {
		/*setup back size and render*/
		group2d_start_child((GroupingNode2D *)l2D);
		
		eff->draw_background = 1;
		ctx = b2D_GetContext(back, l2D->backs);
		ctx->unclip = l2D->clip;
		ctx->clip = gf_rect_pixelize(&ctx->unclip);
		gf_mx2d_init(ctx->transform);
		gf_node_render((GF_Node *) back, eff);
		eff->draw_background = 0;
	
		/*we need a trick since we're not using a dedicated surface for layer rendering, 
		we emulate the back context: remove previous context and insert fake one*/
		if (!(eff->trav_flags & TF_RENDER_DIRECT) && (gf_list_count(l2D->groups)==1)) {
			ChildGroup2D *cg = gf_list_get(l2D->groups, 0);
			back_ctx = VS2D_GetDrawableContext(eff->surface);
			gf_list_rem(cg->contexts, 0);
			gf_list_add(cg->contexts, back_ctx);
			back_ctx->unclip = ctx->unclip;
			back_ctx->clip = ctx->clip;
			back_ctx->h_texture = ctx->h_texture;
			back_ctx->transparent = 0;
			back_ctx->redraw_flags = ctx->redraw_flags;
			back_ctx->is_background = 1;
			back_ctx->aspect = ctx->aspect;
			back_ctx->node = ctx->node;
		}
		group2d_end_child((GroupingNode2D *)l2D);
	}

	group2d_traverse((GroupingNode2D *)l2D, l->children, eff);
	/*restore effect*/
	eff->draw_background = bool_bck;
	gf_mx2d_copy(eff->transform, gf_mx2d_bck);
	eff->parent = parent_bck;
	eff->back_stack = prevback;
	eff->view_stack = prevviews;

	/*check bindables*/
	if (l2D->first) {
		Bool redraw = 0;
		l2D->first = 0;
		if (!back && gf_list_count(l2D->backs)) redraw = 1;
		if (!vp && gf_list_count(l2D->views) ) redraw = 1;

		/*we missed background or viewport (was not declared as bound during traversal, and is bound now)*/
		if (redraw) {
			group2d_reset_children((GroupingNode2D*)l2D);
			gf_sr_invalidate(l2D->compositor, NULL);
			return;
		}
	}

	i=0;
	while ((cg = gf_list_enum(l2D->groups, &i))) {
		child2d_render_done(cg, eff, &l2D->clip);
	}
	group2d_reset_children((GroupingNode2D*)l2D);

	group2d_force_bounds(eff->parent, &l2D->clip);
}

void R2D_InitLayer2D(Render2D *sr, GF_Node *node)
{
	Layer2DStack *stack = malloc(sizeof(Layer2DStack));
	SetupGroupingNode2D((GroupingNode2D*)stack, sr, node);
	stack->backs = gf_list_new();
	stack->views = gf_list_new();
	stack->first = 1;

	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyLayer2D);
	gf_node_set_render_function(node, RenderLayer2D);
}

