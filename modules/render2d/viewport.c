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
/*for default scene view*/
#include <gpac/internal/terminal_dev.h>



GF_Err R2D_GetViewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	u32 count;
	GF_Node *n;
	Render2D *sr = (Render2D *) vr->user_priv;
	if (!sr->surface) return GF_BAD_PARAM;
	count = gf_list_count(sr->surface->view_stack);
	if (!viewpoint_idx) return GF_BAD_PARAM;
	if (viewpoint_idx>count) return GF_EOS;

	n = gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_Viewport: *outName = ((M_Viewport*)n)->description.buffer; *is_bound = ((M_Viewport*)n)->isBound; return GF_OK;
	default: *outName = NULL; return GF_OK;
	}
}

GF_Err R2D_SetViewport(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name)
{
	u32 count, i;
	M_Viewport *n;
	Render2D *sr = (Render2D *) vr->user_priv;
	if (!sr->surface) return GF_BAD_PARAM;
	count = gf_list_count(sr->surface->view_stack);
	if (viewpoint_idx>count) return GF_BAD_PARAM;
	if (!viewpoint_idx && !viewpoint_name) return GF_BAD_PARAM;

	/*note we're sure only viewport nodes are in the 2D view stack*/
	if (viewpoint_idx) {
		n = (M_Viewport *) gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
		n->set_bind = !n->set_bind;
		n->on_set_bind((GF_Node *) n);
		return GF_OK;
	}
	for (i=0; i<count;i++) {
		n = gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
		if (n->description.buffer && !stricmp(n->description.buffer, viewpoint_name)) {
			n->set_bind = !n->set_bind;
			n->on_set_bind((GF_Node *) n);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

#define VPCHANGED(__comp) { GF_Event evt; evt.type = GF_EVT_VIEWPOINTS; GF_USER_SENDEVENT(__comp->user, &evt); }

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_List *stack_list;
	u32 last_sim_time;
	Bool first_time;
} ViewportStack;


static void DestroyViewport(GF_Node *node)
{
	s32 i;
	GF_List *stack;
	ViewportStack *ptr = (ViewportStack *) gf_node_get_private(node);
	M_Viewport *n_vp;

	while (gf_list_count(ptr->stack_list)) {
		stack = gf_list_get(ptr->stack_list, 0);
		i = gf_list_del_item(stack, node);
		if (i==0) {
			n_vp = (M_Viewport *) gf_list_get(stack, 0);
			/*we were bound so bind new top*/
			if (n_vp) {
				n_vp->isBound = 1;
				n_vp->bindTime = gf_node_get_scene_time((GF_Node *)n_vp);
				gf_node_event_out_str((GF_Node *)n_vp, "isBound");
				gf_node_event_out_str((GF_Node *)n_vp, "bindTime");
			}
		}
		gf_list_rem(ptr->stack_list, 0);
	}
	gf_list_del(ptr->stack_list);
	/*notify change of vp stack*/
	VPCHANGED(ptr->compositor);
	free(ptr);
}

static void viewport_set_bind(GF_Node *node)
{
	Bool on_top;
	u32 i;
	GF_List *stack;
	GF_Node *tmp;
	ViewportStack *ptr = (ViewportStack *) gf_node_get_private(node);
	M_Viewport *vp = (M_Viewport *) ptr->owner;

	//notify all stacks using this node
	for (i=0; i<gf_list_count(ptr->stack_list);i++) {
		stack = gf_list_get(ptr->stack_list, i);
		on_top = (gf_list_get(stack, 0) == node) ? 1 : 0;
	
		if (!vp->set_bind) {
			if (vp->isBound) {
				vp->isBound = 0;
				gf_node_event_out_str(node, "isBound");
			}

			if (on_top) {
				tmp = gf_list_get(stack, 0);
				gf_list_rem(stack, 0);
				gf_list_add(stack, tmp);
				tmp = gf_list_get(stack, 0);
				if (tmp != node) {
					((M_Viewport *) tmp)->set_bind = 1;
					gf_node_event_out_str(tmp, "set_bind");
				}
			}
		} else {
			if (!vp->isBound) {
				vp->isBound = 1;
				vp->bindTime = gf_node_get_scene_time(node);
				gf_node_event_out_str(node, "isBound");
				gf_node_event_out_str(node, "bindTime");
			}

			if (!on_top) {
				tmp = gf_list_get(stack, 0);
				gf_list_del_item(stack, ptr);
				gf_list_insert(stack, ptr, 0);
				if (tmp != node) {
					((M_Viewport *) tmp)->set_bind = 0;
					gf_node_event_out_str(tmp, "isBound");
				}
			}
		}
	}
	gf_sr_invalidate(ptr->compositor, NULL);
	/*notify change of vp stack*/
	VPCHANGED(ptr->compositor);
}



static GF_List *vp_get_stack(ViewportStack *vp, RenderEffect2D *eff)
{
	u32 i;	
	if (!eff->view_stack) return NULL;

	for (i=0; i<gf_list_count(vp->stack_list); i++) {
		if (eff->view_stack == gf_list_get(vp->stack_list, i) ) return eff->view_stack;	
	}
	gf_list_add(vp->stack_list, eff->view_stack);
	gf_list_add(eff->view_stack, vp->owner);
	/*need a callback to user*/
	return eff->view_stack;
}

static void RenderViewport(GF_Node *node, void *rs)
{
	ViewportStack *st = (ViewportStack *) gf_node_get_private(node);
	M_Viewport *vp = (M_Viewport *) st->owner;

	if (st->first_time) {
		GF_List *stack = vp_get_stack(st, (RenderEffect2D *)rs);

		if (gf_list_get(stack, 0) == node) {
			if (! vp->isBound) {
				vp->isBound = 1;
				vp->bindTime = gf_node_get_scene_time(node);
				gf_node_event_out_str(node, "isBound");
				gf_node_event_out_str(node, "bindTime");
			}
		} else {
			if (gf_is_default_scene_viewpoint(node)) {
				vp->set_bind = 1;
				vp->on_set_bind(node);
			}
		}
		st->first_time = 0;
		VPCHANGED(st->compositor);
	}
}

void R2D_InitViewport(Render2D *sr, GF_Node *node)
{
	ViewportStack *ptr = malloc(sizeof(ViewportStack));
	memset(ptr, 0, sizeof(ViewportStack));
	ptr->first_time = 1;
	ptr->stack_list = gf_list_new();

	gf_sr_traversable_setup(ptr, node, sr->compositor);
	gf_node_set_private(node, ptr);
	gf_node_set_render_function(node, RenderViewport);
	gf_node_set_predestroy_function(node, DestroyViewport);
	((M_Viewport*)node)->on_set_bind = viewport_set_bind;
}



void vp_setup(GF_Node *n, RenderEffect2D *eff, GF_Rect *surf_clip)
{
	Fixed sx, sy, w, h, tx, ty;
	GF_Matrix2D mat;
	GF_Rect rc;
	M_Viewport *vp = (M_Viewport *) n;
	if (!vp->isBound || !surf_clip->width || !surf_clip->height) return;

	gf_mx2d_init(mat);
	gf_mx2d_add_translation(&mat, -1 * vp->position.x, -1 * vp->position.y);
	gf_mx2d_add_rotation(&mat, 0, 0, -1 * vp->orientation);
	
	gf_mx2d_add_matrix(&eff->transform, &mat);

	gf_mx2d_copy(mat, eff->transform);

	//compute scaling ratio
	rc = gf_rect_center(vp->size.x, vp->size.y);
	gf_mx2d_apply_rect(&mat, &rc);

	w = surf_clip->width;
	h = surf_clip->height;
	
	surf_clip->width = rc.width;
	surf_clip->height = rc.height;

	switch (vp->fit) {
	//covers all area and respect aspect ratio
	case 2:
		if (gf_divfix(rc.width, w) > gf_divfix(rc.height, h)) {
			rc.width = gf_muldiv(rc.width , h, rc.height);
			rc.height = h;
		} else {
			rc.height = gf_muldiv(rc.height , w, rc.width);
			rc.width = w;
		}
		break;
	//fits inside the area and respect AR
	case 1:
		if (gf_divfix(rc.width, w) > gf_divfix(rc.height, h)) {
			rc.height = gf_muldiv(rc.height, w, rc.width);
			rc.width = w;
		} else {
			rc.width = gf_muldiv(rc.width , h, rc.height);
			rc.height = h;
		}
		break;
	//fit entirely: nothing to change
	case 0:
		rc.width = w;
		rc.height = h;
		break;
	default:
		return;
	}
	sx = gf_divfix(rc.width, surf_clip->width);
	sy = gf_divfix(rc.height, surf_clip->height);

	surf_clip->width = rc.width;
	surf_clip->height = rc.height;
	surf_clip->x = - rc.width/2;
	surf_clip->y = rc.height/2;

	gf_mx2d_init(mat);
	if (!vp->fit) {
		gf_mx2d_add_scale(&mat, sx, sy);
		gf_mx2d_add_matrix(&eff->transform, &mat);
		return;
	}

	//setup x-alignment
	switch (vp->alignment.vals[0]) {
	//left align: 
	case -1:
		tx = rc.width/2 - w/2;
		break;
	//right align
	case 1:
		tx = w/2 - rc.width/2;
		break;
	//center
	case 0:
	default:
		tx = 0;
		break;
	}

	//setup y-alignment
	switch (vp->alignment.vals[1]) {
	//left align: 
	case -1:
		ty = rc.height/2 - h/2;
		break;
	//right align
	case 1:
		ty = h/2 - rc.height/2;
		break;
	//center
	case 0:
	default:
		ty = 0;
		break;
	}

	gf_mx2d_add_scale(&mat, sx, sy);
	gf_mx2d_add_translation(&mat, tx, ty);
	gf_mx2d_add_matrix(&eff->transform, &mat);
	surf_clip->x += tx;
	surf_clip->y += ty;
}

