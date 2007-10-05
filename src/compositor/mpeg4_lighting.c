/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#ifndef GPAC_DISABLE_3D

#include "visual_manager.h"


static void TraverseSpotLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_SpotLight *sl = (M_SpotLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy || !sl->on) return;

	/*store local bounds for culling*/
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*how crude - refine to only have the light cone :)*/
		SFVec3f size;
		size.x = size.y = size.z = sl->radius;
		gf_vec_add(tr_state->bbox.max_edge, sl->location, size);
		gf_vec_diff(tr_state->bbox.min_edge, sl->location, size);
		gf_bbox_refresh(&tr_state->bbox);
		return;
	}
	else if (tr_state->traversing_mode == TRAVERSE_LIGHTING) {
		visual_3d_matrix_push(tr_state->visual);
		visual_3d_matrix_add(tr_state->visual, tr_state->model_matrix.m);
		
		visual_3d_add_spot_light(tr_state->visual, sl->ambientIntensity, sl->attenuation, sl->beamWidth, 
					   sl->color, sl->cutOffAngle, sl->direction, sl->intensity, sl->location);
		
		visual_3d_matrix_pop(tr_state->visual);
	}
}

void compositor_init_spot_light(GF_Compositor *compositor, GF_Node *node)
{
	/*no need for a stck*/
	gf_node_set_callback_function(node, TraverseSpotLight);
}

static void TraversePointLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_PointLight *pl = (M_PointLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy || !pl->on) return;

	/*store local bounds for culling*/
	if (tr_state->traversing_mode==TRAVERSE_GET_BOUNDS) {
		SFVec3f size;
		size.x = size.y = size.z = pl->radius;
		gf_vec_add(tr_state->bbox.max_edge, pl->location, size);
		gf_vec_diff(tr_state->bbox.min_edge, pl->location, size);
		gf_bbox_refresh(&tr_state->bbox);
		return;
	}
	else if (tr_state->traversing_mode == TRAVERSE_LIGHTING) {
		visual_3d_matrix_push(tr_state->visual);
		visual_3d_matrix_add(tr_state->visual, tr_state->model_matrix.m);

		visual_3d_add_point_light(tr_state->visual, pl->ambientIntensity, pl->attenuation, pl->color, 
					pl->intensity, pl->location);

		visual_3d_matrix_pop(tr_state->visual);
	}
}

void compositor_init_point_light(GF_Compositor *compositor, GF_Node *node)
{
	/*no need for a stck*/
	gf_node_set_callback_function(node, TraversePointLight);
}


static void TraverseDirectionalLight(GF_Node *n, void *rs, Bool is_destroy)
{
	Bool *stack = (Bool*)gf_node_get_private(n);
	M_DirectionalLight *dl = (M_DirectionalLight *)n;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		free(stack);
		return;
	}
	if (tr_state->switched_off || !dl->on) return;

	/*1- DL only lights the parent group, no need for culling it*/
	/*DL is set dynamically while traversing, the only mode that interest us is draw*/
	if (tr_state->traversing_mode) return;

	if (tr_state->local_light_on) {
		*stack = visual_3d_add_directional_light(tr_state->visual, dl->ambientIntensity, dl->color, dl->intensity, dl->direction);
	} else {
		if (*stack) visual_3d_remove_last_light(tr_state->visual);
		*stack = 0;
	}
}

void compositor_init_directional_light(GF_Compositor *compositor, GF_Node *node)
{
	/*our stack is just a boolean used to store whether the light was turned on successfully*/
	Bool *stack = (Bool*)malloc(sizeof(Bool));
	*stack = 0;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, TraverseDirectionalLight);
}

#endif /*GPAC_DISABLE_3D*/
