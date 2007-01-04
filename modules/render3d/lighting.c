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

Bool r3d_is_light(GF_Node *n, Bool local_only)
{
	switch (gf_node_get_tag(n)) {
	case TAG_MPEG4_DirectionalLight: 
	case TAG_X3D_DirectionalLight: 
		return 1;
	case TAG_MPEG4_PointLight:
	case TAG_MPEG4_SpotLight:
		return local_only ? 0 : 1;
	default: 
		return 0;
	}
}


static void RenderSpotLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_SpotLight *sl = (M_SpotLight *)n;
	RenderEffect3D *eff = (RenderEffect3D *) rs;

	if (is_destroy || !sl->on) return;

	/*store local bounds for culling*/
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		/*how crude - refine to only have the light cone :)*/
		SFVec3f size;
		size.x = size.y = size.z = sl->radius;
		gf_vec_add(eff->bbox.max_edge, sl->location, size);
		gf_vec_diff(eff->bbox.min_edge, sl->location, size);
		gf_bbox_refresh(&eff->bbox);
		return;
	}
	else if (eff->traversing_mode == TRAVERSE_LIGHTING) {
		VS3D_PushMatrix(eff->surface);
		VS3D_MultMatrix(eff->surface, eff->model_matrix.m);
		
		VS3D_AddSpotLight(eff->surface, sl->ambientIntensity, sl->attenuation, sl->beamWidth, 
					   sl->color, sl->cutOffAngle, sl->direction, sl->intensity, sl->location);
		
		VS3D_PopMatrix(eff->surface);
	}
}

void R3D_InitSpotLight(Render3D *sr, GF_Node *node)
{
	/*no need for a stck*/
	gf_node_set_callback_function(node, RenderSpotLight);
}

static void RenderPointLight(GF_Node *n, void *rs, Bool is_destroy)
{
	M_PointLight *pl = (M_PointLight *)n;
	RenderEffect3D *eff = (RenderEffect3D *) rs;

	if (is_destroy || !pl->on) return;

	/*store local bounds for culling*/
	if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		SFVec3f size;
		size.x = size.y = size.z = pl->radius;
		gf_vec_add(eff->bbox.max_edge, pl->location, size);
		gf_vec_diff(eff->bbox.min_edge, pl->location, size);
		gf_bbox_refresh(&eff->bbox);
		return;
	}
	else if (eff->traversing_mode == TRAVERSE_LIGHTING) {
		VS3D_PushMatrix(eff->surface);
		VS3D_MultMatrix(eff->surface, eff->model_matrix.m);

		VS3D_AddPointLight(eff->surface, pl->ambientIntensity, pl->attenuation, pl->color, 
					pl->intensity, pl->location);

		VS3D_PopMatrix(eff->surface);
	}
}

void R3D_InitPointLight(Render3D *sr, GF_Node *node)
{
	/*no need for a stck*/
	gf_node_set_callback_function(node, RenderPointLight);
}


static void RenderDirectionalLight(GF_Node *n, void *rs, Bool is_destroy)
{
	Bool *stack = (Bool*)gf_node_get_private(n);
	M_DirectionalLight *dl = (M_DirectionalLight *)n;
	RenderEffect3D *eff = (RenderEffect3D *) rs;

	if (is_destroy) {
		free(stack);
		return;
	}
	if ((eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) || !dl->on) return;

	/*1- DL only lights the parent group, no need for culling it*/
	/*DL is set dynamically while rendering, the only mode that interest us is draw*/
	if (eff->traversing_mode) return;

	if (eff->local_light_on) {
		*stack = VS3D_AddDirectionalLight(eff->surface, dl->ambientIntensity, dl->color, dl->intensity, dl->direction);
	} else {
		if (*stack) VS3D_RemoveLastLight(eff->surface);
		*stack = 0;
	}
}

void R3D_InitDirectionalLight(Render3D *sr, GF_Node *node)
{
	/*our stack is just a boolean used to store whether the light was turned on successfully*/
	Bool *stack = (Bool*)malloc(sizeof(Bool));
	*stack = 0;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, RenderDirectionalLight);
}

