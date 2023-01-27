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

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR) && !defined(GPAC_DISABLE_3D)

#include "visual_manager.h"


static void TraverseSpotLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_SpotLight *sl = (M_SpotLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		Bool *vis = (Bool*)gf_node_get_private(n);
		gf_free(vis);
		return;
	}
	if (!sl->on) {
		visual_3d_has_inactive_light(tr_state->visual);
		return;
	}

	/*store local bounds for culling*/
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		GF_BBox b;
		SFVec3f size;
		Bool *visible = (Bool*)gf_node_get_private(n);
		size.x = size.y = size.z = sl->radius;
		gf_vec_add(b.max_edge, sl->location, size);
		gf_vec_diff(b.min_edge, sl->location, size);
		gf_bbox_refresh(&b);
		*visible = visual_3d_node_cull(tr_state, &b, GF_FALSE);
		/*if visible, disable culling on our parent branch - this is not very efficient but
		we only store one bound per grouping node, and we don't want the lights to interfere with it*/
		if (*visible) tr_state->disable_cull = GF_TRUE;
		return;
	}
	else if (tr_state->traversing_mode == TRAVERSE_LIGHTING) {
		Bool *visible = (Bool*)gf_node_get_private(n);
		if (*visible) {
			visual_3d_add_spot_light(tr_state->visual, sl->ambientIntensity, sl->attenuation, sl->beamWidth,
			                         sl->color, sl->cutOffAngle, sl->direction, sl->intensity, sl->location, &tr_state->model_matrix);
		} else {
			visual_3d_has_inactive_light(tr_state->visual);
		}
	}
}

void compositor_init_spot_light(GF_Compositor *compositor, GF_Node *node)
{
	Bool *vis = (Bool*)gf_malloc(sizeof(Bool));
	*vis = GF_FALSE;
	gf_node_set_private(node, vis);
	/*no need for a stck*/
	gf_node_set_callback_function(node, TraverseSpotLight);
}

static void TraversePointLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_PointLight *pl = (M_PointLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		Bool *vis = (Bool*)gf_node_get_private(n);
		gf_free(vis);
		return;
	}
	if (!pl->on) {
		visual_3d_has_inactive_light(tr_state->visual);
		return;
	}

	/*store local bounds for culling*/
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		SFVec3f size;
		GF_BBox b;
		Bool *visible = (Bool*)gf_node_get_private(n);
		size.x = size.y = size.z = pl->radius;
		gf_vec_add(b.max_edge, pl->location, size);
		gf_vec_diff(b.min_edge, pl->location, size);
		gf_bbox_refresh(&b);
		*visible = visual_3d_node_cull(tr_state, &b, GF_FALSE);
		/*if visible, disable culling on our parent branch*/
		if (*visible) tr_state->disable_cull = GF_TRUE;
		return;
	}
	else if (tr_state->traversing_mode == TRAVERSE_LIGHTING) {
		Bool *visible = (Bool*)gf_node_get_private(n);
		if (*visible) {
			visual_3d_add_point_light(tr_state->visual, pl->ambientIntensity, pl->attenuation, pl->color,
			                          pl->intensity, pl->location, &tr_state->model_matrix);
		} else {
			visual_3d_has_inactive_light(tr_state->visual);
		}
	}
}

void compositor_init_point_light(GF_Compositor *compositor, GF_Node *node)
{
	Bool *vis = (Bool*)gf_malloc(sizeof(Bool));
	*vis = GF_FALSE;
	gf_node_set_private(node, vis);
	/*no need for a stck*/
	gf_node_set_callback_function(node, TraversePointLight);
}


static void TraverseDirectionalLight(GF_Node *n, void *rs, Bool is_destroy)
{
	Bool *stack = (Bool*)gf_node_get_private(n);
	M_DirectionalLight *dl = (M_DirectionalLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		gf_free(stack);
		return;
	}
	if (tr_state->switched_off || !dl->on) {
		visual_3d_has_inactive_light(tr_state->visual);
		return;
	}

	/*1- DL only lights the parent group, no need for culling it*/
	/*DL is set dynamically while traversing, the only mode that interest us is draw*/
	if (tr_state->traversing_mode) return;

	if (tr_state->local_light_on) {
		*stack = visual_3d_add_directional_light(tr_state->visual, dl->ambientIntensity, dl->color, dl->intensity, dl->direction, &tr_state->model_matrix);
	} else {
		if (*stack) visual_3d_remove_last_light(tr_state->visual);
		*stack = GF_FALSE;
		visual_3d_has_inactive_light(tr_state->visual);
	}
}

void compositor_init_directional_light(GF_Compositor *compositor, GF_Node *node)
{
	Bool *stack = (Bool*)gf_malloc(sizeof(Bool));
	*stack = GF_FALSE;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseDirectionalLight);
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR) && !defined(GPAC_DISABLE_3D)
