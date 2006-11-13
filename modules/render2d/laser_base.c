/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR Rendering sub-project
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

#include "visualsurface2d.h"

#ifndef GPAC_DISABLE_SVG
#include "laser_stacks.h"

static void LASeR_Render_selector(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGselectorElement *sel = (SVGselectorElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	SVG_Render_base(node, eff, &backup_props);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(sel->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->trav_flags = prev_flags;
		return;
	}	
	
	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		if (((SVGTransformableElement *)node)->motionTransform) 
			gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
		gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
		svg_get_nodes_bounds(node, sel->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	if (((SVGTransformableElement *)node)->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
	gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
	switch (sel->choice.type) {
	case LASeR_CHOICE_NONE:
		break;
	case LASeR_CHOICE_ALL:
		svg_render_node_list(sel->children, eff);
		break;
	case LASeR_CHOICE_N:
		svg_render_node((GF_Node*)gf_list_get(sel->children, sel->choice.choice_index), eff);
		break;
	}

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

void LASeR_Init_selector(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, LASeR_Render_selector);
}


static void LASeR_Render_simpleLayout(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGsimpleLayoutElement *sl = (SVGsimpleLayoutElement*)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	SVG_Render_base(node, eff, &backup_props);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(sl->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->trav_flags = prev_flags;
		return;
	}	
	
	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		if (((SVGTransformableElement *)node)->motionTransform) 
			gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
		gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
		if (sl->delta.enabled) {
			/*TODO*/
		} else {
			svg_get_nodes_bounds(node, sl->children, eff);
		}
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	if (((SVGTransformableElement *)node)->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
	gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
	if (sl->delta.enabled) {
		/*TODO*/
	} else {
		svg_render_node_list(sl->children, eff);
	}
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

void LASeR_Init_simpleLayout(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, LASeR_Render_simpleLayout);
}

static void LASeR_Render_rectClip(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGrectClipElement *rc = (SVGrectClipElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	SVG_Render_base(node, eff, &backup_props);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(rc->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->trav_flags = prev_flags;
		return;
	}	
	
	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		if (((SVGTransformableElement *)node)->motionTransform) 
			gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
		gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
		if (rc->size.enabled) {
			eff->bounds.width = rc->size.width;
			eff->bounds.x = - rc->size.width / 2;
			eff->bounds.height = rc->size.height;
			eff->bounds.y = rc->size.height / 2;
			gf_mx2d_apply_rect(&eff->transform, &eff->bounds);
		} else {
			svg_get_nodes_bounds(node, rc->children, eff);
		}
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	if (((SVGTransformableElement *)node)->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, ((SVGTransformableElement *)node)->motionTransform);
	gf_mx2d_pre_multiply(&eff->transform, &((SVGTransformableElement *)node)->transform);
	/*setup cliper*/
	if (rc->size.enabled) {
		/*TODO*/
	}
	svg_render_node_list(rc->children, eff);
	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

void LASeR_Init_rectClip(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, LASeR_Render_rectClip);
}

#endif //GPAC_DISABLE_SVG
