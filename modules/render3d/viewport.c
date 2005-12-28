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
#include <gpac/options.h>
/*for default viewport*/
#include <gpac/internal/terminal_dev.h>



GF_Err R3D_GetViewpoint(GF_VisualRenderer *vr, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	u32 count;
	GF_Node *n;
	Render3D *sr = (Render3D *) vr->user_priv;
	if (!sr->surface) return GF_BAD_PARAM;
	count = gf_list_count(sr->surface->view_stack);
	if (!viewpoint_idx) return GF_BAD_PARAM;
	if (viewpoint_idx>count) return GF_EOS;

	n = gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_Viewport: *outName = ((M_Viewport*)n)->description.buffer; *is_bound = ((M_Viewport*)n)->isBound;return GF_OK;
	case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: *outName = ((M_Viewpoint*)n)->description.buffer; *is_bound = ((M_Viewpoint*)n)->isBound; return GF_OK;
	default: *outName = NULL; return GF_OK;
	}
}

GF_Err R3D_SetViewpoint(GF_VisualRenderer *vr, u32 viewpoint_idx, const char *viewpoint_name)
{
	u32 count, i;
	GF_Node *n;
	Render3D *sr = (Render3D *) vr->user_priv;
	if (!sr->surface) return GF_BAD_PARAM;
	count = gf_list_count(sr->surface->view_stack);
	if (viewpoint_idx>count) return GF_BAD_PARAM;
	if (!viewpoint_idx && !viewpoint_name) return GF_BAD_PARAM;

	if (viewpoint_idx) {
		Bool bind;
		n = gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
		bind = Bindable_GetIsBound(n);
		Bindable_SetSetBind(n, !bind);
		return GF_OK;
	}
	for (i=0; i<count;i++) {
		char *name = NULL;
		n = gf_list_get(sr->surface->view_stack, viewpoint_idx-1);
		switch (gf_node_get_tag(n)) {
		case TAG_MPEG4_Viewport: name = ((M_Viewport*)n)->description.buffer; break;
		case TAG_MPEG4_Viewpoint: case TAG_X3D_Viewpoint: name = ((M_Viewpoint*)n)->description.buffer; break;
		default: break;
		}
		if (name && !stricmp(name, viewpoint_name)) {
			Bool bind = Bindable_GetIsBound(n);
			Bindable_SetSetBind(n, !bind);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}


#define VPCHANGED(__comp) { GF_Event evt; evt.type = GF_EVT_VIEWPOINTS; GF_USER_SENDEVENT(__comp->user, &evt); }



static void DestroyViewStack(GF_Node *node)
{
	ViewStack *st = (ViewStack *) gf_node_get_private(node);
	PreDestroyBindable(node, st->reg_stacks);
	gf_list_del(st->reg_stacks);
	VPCHANGED(st->compositor);
	free(st);
}

static void viewport_set_bind(GF_Node *node)
{
	ViewStack *st = (ViewStack *) gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	gf_sr_invalidate(st->compositor, NULL);
	/*notify change of vp stack*/
	VPCHANGED(st->compositor);
	/*and dirty ourselves to force frustrum update*/
	gf_node_dirty_set(node, 0, 0);
}

static void RenderViewport(GF_Node *node, void *rs)
{
	Fixed ar, sx, sy, w, h, tx, ty;
	GF_Matrix2D mat;
	GF_Matrix mx;
	GF_Rect rc, rc_bckup;
	Bool is_layer;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_Viewport *vp = (M_Viewport*) node;
	ViewStack *st = (ViewStack *) gf_node_get_private(node);

	assert(eff->viewpoints);
	if (eff->camera->is_3D) return;

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->viewpoints, node) < 0) {
		gf_list_add(eff->viewpoints, node);
		assert(gf_list_find(st->reg_stacks, eff->viewpoints)==-1);
		gf_list_add(st->reg_stacks, eff->viewpoints);

		if (gf_list_get(eff->viewpoints, 0) == vp) {
			if (!vp->isBound) Bindable_SetIsBound(node, 1);
		} else {
			if (gf_is_default_scene_viewpoint(node)) Bindable_SetSetBind(node, 1);
		}
		VPCHANGED(st->compositor);
		/*in any case don't draw the first time (since the viewport could have been declared last)*/
		gf_sr_invalidate(st->compositor, NULL);
		return;
	}
	/*not bound ignore*/
	if (!vp->isBound) return;
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) return;
	
	is_layer = (eff->viewpoints == eff->surface->view_stack) ? 0 : 1;
	/*if no parent this is the main viewport, don't update if not changed*/
	if (!is_layer && !gf_node_dirty_get(node)) return;

	gf_node_dirty_clear(node, 0);

	gf_mx2d_init(mat);
	gf_mx2d_add_translation(&mat, vp->position.x, vp->position.y);
	gf_mx2d_add_rotation(&mat, 0, 0, vp->orientation);

	//compute scaling ratio
	rc = gf_rect_center(vp->size.x, vp->size.y);
	gf_mx2d_apply_rect(&mat, &rc);

	w = eff->bbox.max_edge.x - eff->bbox.min_edge.x;
	h = eff->bbox.max_edge.y - eff->bbox.min_edge.y;
	ar = gf_divfix(h, w);

	rc_bckup = rc;

	switch (vp->fit) {
	/*covers all area and respect aspect ratio*/
	case 2:
		if (gf_divfix(rc.width, w) > gf_divfix(rc.height, h)) {
			rc.width = gf_muldiv(rc.width, h, rc.height);
			rc.height = h;
		} else {
			rc.height = gf_muldiv(rc.height , w, rc.width);
			rc.width = w;
		}
		break;
	/*fits inside the area and respect AR*/
	case 1:
		if (gf_divfix(rc.width, w)> gf_divfix(rc.height, h)) {
			rc.height = gf_muldiv(rc.height, w, rc.width);
			rc.width = w;
		} else {
			rc.width = gf_muldiv(rc.width, h, rc.height);
			rc.height = h;
		}
		break;
	/*fit entirely: nothing to change*/
	case 0:
		rc.width = w;
		rc.height = h;
		break;
	default:
		return;
	}
	sx = gf_divfix(rc_bckup.width , rc.width);
	sy = gf_divfix(rc_bckup.height , rc.height);

	rc.x = - rc.width/2;
	rc.y = rc.height/2;

	tx = ty = 0;
	if (vp->fit) {
		/*left alignment*/
		if (vp->alignment.vals[0] == -1) tx = rc.width/2 - w/2;
		else if (vp->alignment.vals[0] == 1) tx = w/2 - rc.width/2;

		/*top-alignment*/
		if (vp->alignment.vals[1]==-1) ty = rc.height/2 - h/2;
		else if (vp->alignment.vals[1]==1) ty = h/2 - rc.height/2;
	}

	
	gf_mx_from_mx2d(&mx, &mat);
	gf_mx_add_scale(&mx, sx, sy, FIX_ONE);
	gf_mx_add_translation(&mx, -tx, -ty, 0);
	gf_mx_inverse(&mx);

	/*in layers directly modify the model matrix*/
	if (eff->viewpoints != eff->surface->view_stack) {
		gf_mx_add_matrix(&eff->model_matrix, &mx);
	} else {
		gf_mx_copy(eff->camera->viewport, mx);
		eff->camera->flags = (CAM_HAS_VIEWPORT | CAM_IS_DIRTY);
	}
}

void R3D_InitViewport(Render3D *sr, GF_Node *node)
{
	ViewStack *st = malloc(sizeof(ViewStack));
	memset(st, 0, sizeof(ViewStack));
	st->reg_stacks = gf_list_new();
	gf_sr_traversable_setup(st, node, sr->compositor);
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderViewport);
	gf_node_set_predestroy_function(node, DestroyViewStack);
	((M_Viewport*)node)->on_set_bind = viewport_set_bind;
}

static void viewpoint_set_bind(GF_Node *node)
{
	ViewStack *st = (ViewStack *) gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	gf_sr_invalidate(st->compositor, NULL);
	/*notify change of vp stack*/
	VPCHANGED(st->compositor);
	/*and dirty ourselves to force frustrum update*/
	gf_node_dirty_set(node, 0, 0);

	if (!((M_Viewpoint*)node)->isBound) 
		st->prev_was_bound = 0;
}

static void RenderViewpoint(GF_Node *node, void *rs)
{
	SFVec3f pos, v1, v2;
	SFRotation ori;
	GF_Matrix mx;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_Viewpoint *vp = (M_Viewpoint*) node;
	ViewStack *st = (ViewStack *) gf_node_get_private(node);

	assert(eff->viewpoints);
//	if (!eff->camera->is_3D) return;

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->viewpoints, node) < 0) {
		gf_list_add(eff->viewpoints, node);
		assert(gf_list_find(st->reg_stacks, eff->viewpoints)==-1);
		gf_list_add(st->reg_stacks, eff->viewpoints);

		if (gf_list_get(eff->viewpoints, 0) == vp) {
			if (!vp->isBound) Bindable_SetIsBound(node, 1);
		} else {
			if (gf_is_default_scene_viewpoint(node)) Bindable_SetSetBind(node, 1);
		}
		VPCHANGED(st->compositor);
		/*in any case don't draw the first time (since the viewport could have been declared last)*/
		gf_sr_invalidate(st->compositor, NULL);
	}
	/*not rendering vp, return*/
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) {
		/*store model matrix if changed - NOTE: we always have a 1-frame delay between VP used and real world...
		we could remove this by pre-rendering the scene before applying vp, but that would mean 2 scene 
		traversals*/
		if ((eff->traversing_mode==TRAVERSE_SORT) || (eff->traversing_mode==TRAVERSE_GET_BOUNDS) ) {
			if (!gf_mx_equal(&st->world_view_mx, &eff->model_matrix)) {
				gf_mx_copy(st->world_view_mx, eff->model_matrix);
				gf_node_dirty_set(node, 0, 0);
			}
		}
		eff->trav_flags |= TF_DONT_CULL;
		return;
	}

	/*not bound or in 2D surface*/
	if (!vp->isBound || !eff->navigations) return;

	if (!gf_node_dirty_get(node)) return;
	gf_node_dirty_clear(node, 0);

	/*move to local system*/
	gf_mx_copy(mx, st->world_view_mx);
	gf_mx_add_translation(&mx, vp->position.x, vp->position.y, vp->position.z);
	gf_mx_add_rotation(&mx, vp->orientation.q, vp->orientation.x, vp->orientation.y, vp->orientation.z);
	gf_mx_decompose(&mx, &pos, &v1, &ori, &v2);
	/*get center*/
	v1.x = v1.y = v1.z = 0;
	/*X3D specifies examine center*/
	if (gf_node_get_tag(node)==TAG_X3D_Viewpoint) v1 = ((X_Viewpoint *)node)->centerOfRotation;
	gf_mx_apply_vec(&st->world_view_mx, &v1);
	/*set frustrum param - animate only if not bound last frame and jump false*/
	VS_ViewpointChange(eff, node, (!st->prev_was_bound && !vp->jump) ? 1 : 0, vp->fieldOfView, pos, ori, v1);
	st->prev_was_bound = 1;
}

void R3D_InitViewpoint(Render3D *sr, GF_Node *node)
{
	ViewStack *st = malloc(sizeof(ViewStack));
	memset(st, 0, sizeof(ViewStack));
	st->reg_stacks = gf_list_new();
	gf_mx_init(st->world_view_mx);
	gf_sr_traversable_setup(st, node, sr->compositor);
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderViewpoint);
	gf_node_set_predestroy_function(node, DestroyViewStack);
	((M_Viewpoint*)node)->on_set_bind = viewpoint_set_bind;
}


static void navinfo_set_bind(GF_Node *node)
{
	ViewStack *st = (ViewStack *) gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	gf_sr_invalidate(st->compositor, NULL);
}

static void RenderNavigationInfo(GF_Node *node, void *rs)
{
	u32 i;
	SFVec3f start, end;
	Fixed scale;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_NavigationInfo *ni = (M_NavigationInfo *) node;
	ViewStack *st = (ViewStack *) gf_node_get_private(node);

	if (!eff->navigations) return;

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->navigations, node) < 0) {
		gf_list_add(eff->navigations, node);
		if (gf_list_get(eff->navigations, 0) == ni) {
			if (!ni->isBound) Bindable_SetIsBound(node, 1);
		}
		assert(gf_list_find(st->reg_stacks, eff->navigations)==-1);
		gf_list_add(st->reg_stacks, eff->navigations);
		/*in any case don't draw the first time*/
		gf_sr_invalidate(st->compositor, NULL);
		return;
	}
	/*not bound*/
	if (!ni->isBound) return;
	/*not rendering, return*/
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) {
		if ((eff->traversing_mode==TRAVERSE_SORT) || (eff->traversing_mode==TRAVERSE_GET_BOUNDS) ) {
			if (!gf_mx_equal(&st->world_view_mx, &eff->model_matrix)) {
				gf_mx_copy(st->world_view_mx, eff->model_matrix);
				gf_node_dirty_set(node, 0, 0);
			}
		}
		return;
	}

	if (!gf_node_dirty_get(node)) return;
	gf_node_dirty_clear(node, 0);

	eff->camera->navigation_flags = 0;
	eff->camera->navigate_mode = 0;
	for (i=0; i<ni->type.count; i++) {
		if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "ANY")) eff->camera->navigation_flags |= NAV_ANY;
		if (!eff->camera->navigate_mode) {
			if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "NONE")) eff->camera->navigate_mode = GF_NAVIGATE_NONE;
			else if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "WALK")) eff->camera->navigate_mode = GF_NAVIGATE_WALK;
			else if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "EXAMINE")) eff->camera->navigate_mode = GF_NAVIGATE_EXAMINE;
			else if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "FLY")) eff->camera->navigate_mode = GF_NAVIGATE_FLY;
			else if (ni->type.vals[i] && !stricmp(ni->type.vals[i], "QTVR")) eff->camera->navigate_mode = GF_NAVIGATE_VR;
		}
	}
	if (ni->headlight) eff->camera->navigation_flags |= NAV_HEADLIGHT;

	start.x = start.y = start.z = 0;
	end.x = end.y = 0;
	end.z = FIX_ONE;
	gf_mx_apply_vec(&st->world_view_mx, &start);
	gf_mx_apply_vec(&st->world_view_mx, &end);
	gf_vec_diff(end, end, start);
	scale = gf_vec_len(end);

	eff->camera->speed = gf_mulfix(scale, ni->speed);
    eff->camera->visibility = gf_mulfix(scale, ni->visibilityLimit);
	if (ni->avatarSize.count) eff->camera->avatar_size.x = gf_mulfix(scale, ni->avatarSize.vals[0]);
	if (ni->avatarSize.count>1) eff->camera->avatar_size.y = gf_mulfix(scale, ni->avatarSize.vals[1]);
	if (ni->avatarSize.count>2) eff->camera->avatar_size.z = gf_mulfix(scale, ni->avatarSize.vals[2]);

	if (eff->is_pixel_metrics) {
		u32 s = MAX(eff->surface->width, eff->surface->height);
		s /= 2;
//		eff->camera->speed = ni->speed;
	    eff->camera->visibility *= s;
		eff->camera->avatar_size.x *= s;
		eff->camera->avatar_size.y *= s;
		eff->camera->avatar_size.z *= s;
	}
}

void R3D_InitNavigationInfo(Render3D *sr, GF_Node *node)
{
	ViewStack *st = malloc(sizeof(ViewStack));
	memset(st, 0, sizeof(ViewStack));
	st->reg_stacks = gf_list_new();
	gf_sr_traversable_setup(st, node, sr->compositor);
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderNavigationInfo);
	gf_node_set_predestroy_function(node, DestroyViewStack);
	((M_NavigationInfo*)node)->on_set_bind = navinfo_set_bind;
}


static void fog_set_bind(GF_Node *node)
{
	ViewStack *st = (ViewStack *) gf_node_get_private(node);
	Bindable_OnSetBind(node, st->reg_stacks);
	gf_sr_invalidate(st->compositor, NULL);
}

static void RenderFog(GF_Node *node, void *rs)
{
	Fixed density, vrange;
	SFVec3f start, end;
	ViewStack *vp_st;
	M_Viewpoint *vp;
	RenderEffect3D *eff = (RenderEffect3D *)rs;
	M_Fog *fog = (M_Fog *) node;
	ViewStack *st = (ViewStack *) gf_node_get_private(node);

	if (!eff->fogs) return;

	/*first traverse, bound if needed*/
	if (gf_list_find(eff->fogs, node) < 0) {
		gf_list_add(eff->fogs, node);
		if (gf_list_get(eff->fogs, 0) == fog) {
			if (!fog->isBound) Bindable_SetIsBound(node, 1);
		}
		assert(gf_list_find(st->reg_stacks, eff->fogs)==-1);
		gf_list_add(st->reg_stacks, eff->fogs);

		gf_mx_copy(st->world_view_mx, eff->model_matrix);
		/*in any case don't draw the first time*/
		gf_sr_invalidate(st->compositor, NULL);
		return;
	}
	/*not rendering, return*/
	if (eff->traversing_mode != TRAVERSE_RENDER_BINDABLE) {
		if ((eff->traversing_mode==TRAVERSE_SORT) || (eff->traversing_mode==TRAVERSE_GET_BOUNDS) ) 
			gf_mx_copy(st->world_view_mx, eff->model_matrix);
		return;
	}
	/*not bound*/
	if (!fog->isBound || !fog->visibilityRange) return;

	/*fog visibility is expressed in current bound VP so get its matrix*/
	vp = gf_list_get(eff->viewpoints, 0);
	vp_st = NULL;
	if (vp->isBound) vp_st = (ViewStack *) gf_node_get_private((GF_Node *)vp);

	start.x = start.y = start.z = 0;
	end.x = end.y = 0; end.z = fog->visibilityRange;
	if (vp_st) {
		gf_mx_apply_vec(&vp_st->world_view_mx, &start);
		gf_mx_apply_vec(&vp_st->world_view_mx, &end);
	}
	gf_mx_apply_vec(&st->world_view_mx, &start);
	gf_mx_apply_vec(&st->world_view_mx, &end);
	gf_vec_diff(end, end, start);
	vrange = gf_vec_len(end);

	density = gf_invfix(vrange);
	VS3D_SetFog(eff->surface, fog->fogType.buffer, fog->color, density, vrange);
}

void R3D_InitFog(Render3D *sr, GF_Node *node)
{
	ViewStack *st = malloc(sizeof(ViewStack));
	memset(st, 0, sizeof(ViewStack));
	st->reg_stacks = gf_list_new();
	gf_sr_traversable_setup(st, node, sr->compositor);
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyViewStack);
	gf_node_set_render_function(node, RenderFog);
	((M_Fog*)node)->on_set_bind = fog_set_bind;
}

