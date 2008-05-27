/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
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

#include "visual_manager.h"

#ifndef GPAC_DISABLE_SVG
#include "nodes_stacks.h"


/*
	This is the generic routine for children traversing
*/
void compositor_svg_traverse_children(GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	while (children) {
		gf_node_traverse(children->node, tr_state);
		children = children->next;
	}
}

Bool compositor_svg_is_display_off(SVGPropertiesPointers *props)
{
	return (props->display && (*(props->display) == SVG_DISPLAY_NONE)) ? 1 : 0;
}


void compositor_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && backup_matrix) {
		GF_Matrix tmp;
		Bool is_draw = (tr_state->traversing_mode==TRAVERSE_SORT) ? 1 : 0;
		gf_mx_copy(*backup_matrix, tr_state->model_matrix);

		if (is_draw) visual_3d_matrix_push(tr_state->visual);

		if (atts->transform && atts->transform->is_ref) {
			gf_mx_from_mx2d(&tr_state->model_matrix, &tr_state->vb_transform);
			if (is_draw) {
				GF_Matrix tmp;
				gf_mx_init(tmp);
				gf_mx_add_translation(&tmp, -tr_state->camera->width/2, tr_state->camera->height/2, 0);
				gf_mx_add_scale(&tmp, FIX_ONE, -FIX_ONE, FIX_ONE);
				gf_mx_add_matrix(&tmp, &tr_state->model_matrix);
				visual_3d_matrix_load(tr_state->visual, tmp.m);
			}
		}

		if (atts->motionTransform) {
			if (is_draw) {
				gf_mx_from_mx2d(&tmp, atts->motionTransform);
				visual_3d_matrix_add(tr_state->visual, tmp.m);
			} else {
				gf_mx_add_matrix_2d(&tr_state->model_matrix, atts->motionTransform);
			}
		}

		if (atts->transform) {
			if (is_draw) {
				gf_mx_from_mx2d(&tmp, &atts->transform->mat);
				visual_3d_matrix_add(tr_state->visual, tmp.m);
			} else {
				gf_mx_add_matrix_2d(&tr_state->model_matrix, &atts->transform->mat);
			}
		}
		return;
	} 
#endif
	gf_mx2d_copy(*backup_matrix_2d, tr_state->transform);

	if (atts->transform && atts->transform->is_ref) 
		gf_mx2d_copy(tr_state->transform, tr_state->vb_transform);

	if (atts->motionTransform) 
		gf_mx2d_pre_multiply(&tr_state->transform, atts->motionTransform);

	if (atts->transform) 
		gf_mx2d_pre_multiply(&tr_state->transform, &atts->transform->mat);

}

void compositor_svg_restore_parent_transformation(GF_TraverseState *tr_state, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d && backup_matrix) {
		if (tr_state->traversing_mode==TRAVERSE_SORT) 
			visual_3d_matrix_pop(tr_state->visual);
		gf_mx_copy(tr_state->model_matrix, *backup_matrix);  
		return;
	} 
#endif
	gf_mx2d_copy(tr_state->transform, *backup_matrix_2d);  
}


static void gf_svg_apply_inheritance_no_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props) 
{
#define CHECK_PROP(a, b) if (b) a = b;

	render_svg_props->audio_level = NULL;
	CHECK_PROP(render_svg_props->display, all_atts->display);
	CHECK_PROP(render_svg_props->fill, all_atts->fill);
	CHECK_PROP(render_svg_props->fill_opacity, all_atts->fill_opacity);
	CHECK_PROP(render_svg_props->fill_rule, all_atts->fill_rule);
	CHECK_PROP(render_svg_props->solid_color, all_atts->solid_color);		
	CHECK_PROP(render_svg_props->solid_opacity, all_atts->solid_opacity);
	CHECK_PROP(render_svg_props->stop_color, all_atts->stop_color);
	CHECK_PROP(render_svg_props->stop_opacity, all_atts->stop_opacity);
	CHECK_PROP(render_svg_props->stroke, all_atts->stroke);
	CHECK_PROP(render_svg_props->stroke_dasharray, all_atts->stroke_dasharray);
	CHECK_PROP(render_svg_props->stroke_dashoffset, all_atts->stroke_dashoffset);
	CHECK_PROP(render_svg_props->stroke_linecap, all_atts->stroke_linecap);
	CHECK_PROP(render_svg_props->stroke_linejoin, all_atts->stroke_linejoin);
	CHECK_PROP(render_svg_props->stroke_miterlimit, all_atts->stroke_miterlimit);
	CHECK_PROP(render_svg_props->stroke_opacity, all_atts->stroke_opacity);
	CHECK_PROP(render_svg_props->stroke_width, all_atts->stroke_width);
	CHECK_PROP(render_svg_props->visibility, all_atts->visibility);
}

void compositor_svg_traverse_base(GF_Node *node, SVGAllAttributes *all_atts, GF_TraverseState *tr_state, 
					 SVGPropertiesPointers *backup_props, u32 *backup_flags)
{
	u32 inherited_flags_mask;

	memcpy(backup_props, tr_state->svg_props, sizeof(SVGPropertiesPointers));
	*backup_flags = tr_state->svg_flags;

#if 0
	// applying inheritance and determining which group of properties are being inherited
	inherited_flags_mask = gf_svg_apply_inheritance(all_atts, tr_state->svg_props);	
	gf_svg_apply_animations(node, tr_state->svg_props); // including again inheritance if values are 'inherit'
#else
	/* animation (including possibly inheritance) then full inheritance */
	gf_svg_apply_animations(node, tr_state->svg_props); 
	inherited_flags_mask = gf_svg_apply_inheritance(all_atts, tr_state->svg_props);
//	gf_svg_apply_inheritance_no_inheritance(all_atts, tr_state->svg_props);
//	inherited_flags_mask = 0xFFFFFFFF;
#endif
	tr_state->svg_flags &= inherited_flags_mask;
	tr_state->svg_flags |= gf_node_dirty_get(node);
}
#endif


