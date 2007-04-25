/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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
#include "svg_stacks.h"

/*
	This is the generic routine for child traversing - note we are not duplicating the effect
*/
void svg_render_node(GF_Node *node, RenderEffect2D *eff)
{
#if 0
	Bool has_listener = gf_dom_listener_count(node);
//	fprintf(stdout, "rendering %s of type %s\n", gf_node_get_name(node), gf_node_get_class_name(node));
	if (has_listener) {
		eff->nb_listeners++;
		gf_node_render(node, eff);
		eff->nb_listeners--;
	} else {
		gf_node_render(node, eff);
	}
#else
	gf_node_render(node, eff);
#endif
}
void svg_render_node_list(GF_ChildNodeItem *children, RenderEffect2D *eff)
{
	while (children) {
		svg_render_node(children->node, eff);
		children = children->next;
	}
}

void svg_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, RenderEffect2D *eff)
{
	GF_Rect rc;
	GF_Matrix2D cur_mx;

	gf_mx2d_copy(cur_mx, eff->transform);
	rc = gf_rect_center(0,0);
	while (children) {
		gf_mx2d_init(eff->transform);
		eff->bounds = gf_rect_center(0,0);

		gf_node_render(children->node, eff);
		/*we hit the target node*/
		if (children->node == eff->for_node) {
			eff->for_node = NULL;
			return;
		}

		gf_mx2d_apply_rect(&eff->transform, &eff->bounds);
		gf_rect_union(&rc, &eff->bounds);
		children = children->next;
	}
	gf_mx2d_copy(eff->transform, cur_mx);
	eff->bounds = rc;
}

void svg_apply_local_transformation(RenderEffect2D *eff, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix)
{
	gf_mx2d_copy(*backup_matrix, eff->transform);

	if (atts->transform && atts->transform->is_ref) 
		gf_mx2d_copy(eff->transform, eff->vb_transform);

	if (atts->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, atts->motionTransform);

	if (atts->transform) 
		gf_mx2d_pre_multiply(&eff->transform, &atts->transform->mat);
}

void svg_restore_parent_transformation(RenderEffect2D *eff, GF_Matrix2D *backup_matrix)
{
	gf_mx2d_copy(eff->transform, *backup_matrix);  
}


void svg_render_base(GF_Node *node, SVGAllAttributes *all_atts, RenderEffect2D *eff, 
					 SVGPropertiesPointers *backup_props, u32 *backup_flags)
{
	u32 inherited_flags_mask;

	memcpy(backup_props, eff->svg_props, sizeof(SVGPropertiesPointers));
	*backup_flags = eff->svg_flags;

#if 0
	// applying inheritance and determining which group of properties are being inherited
	inherited_flags_mask = gf_svg_apply_inheritance(all_atts, eff->svg_props);	
	gf_svg_apply_animations(node, eff->svg_props); // including again inheritance if values are 'inherit'
#else
	/* animation (including possibly inheritance) then full inheritance */
	gf_svg_apply_animations(node, eff->svg_props); 
	inherited_flags_mask = gf_svg_apply_inheritance(all_atts, eff->svg_props);
#endif
	eff->svg_flags &= inherited_flags_mask;
	eff->svg_flags |= gf_node_dirty_get(node);
}

static void svg_set_viewport_transformation(RenderEffect2D *eff, SVGAllAttributes *atts, Bool is_root) 
{
	u32 dpi = 90; /* Should retrieve the dpi from the system */
	Fixed real_width, real_height;
	u32 scene_width, scene_height;
	GF_Matrix2D mat;

	gf_mx2d_init(mat);

	scene_width = eff->surface->render->compositor->scene_width;
	scene_height = eff->surface->render->compositor->scene_height;


	if (!atts->width) { 
		real_width = INT2FIX(scene_width);
	} else {
		switch (atts->width->type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_width = atts->width->value;
			break;
		case SVG_NUMBER_PERCENTAGE:
			real_width = scene_width*atts->width->value/100;
			break;
		case SVG_NUMBER_IN:
			real_width = dpi * atts->width->value;
			break;
		case SVG_NUMBER_CM:
			real_width = gf_mulfix(dpi*FLT2FIX(0.39), atts->width->value);
			break;
		case SVG_NUMBER_MM:
			real_width = gf_mulfix(dpi*FLT2FIX(0.039), atts->width->value);
			break;
		case SVG_NUMBER_PT:
			real_width = dpi/12 * atts->width->value;
			break;
		case SVG_NUMBER_PC:
			real_width = dpi/6 * atts->width->value;
			break;
		default:
			real_width = INT2FIX(scene_width);
			break;
		}
	}

	if (!atts->height) {
		real_height = INT2FIX(scene_height);
	} else {
		switch (atts->height->type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_height = atts->height->value;
			break;
		case SVG_NUMBER_PERCENTAGE:
			real_height = scene_height*atts->height->value/100;
			break;
		case SVG_NUMBER_IN:
			real_height = dpi * atts->height->value;
			break;
		case SVG_NUMBER_CM:
			real_height = gf_mulfix(dpi*FLT2FIX(0.39), atts->height->value);
			break;
		case SVG_NUMBER_MM:
			real_height = gf_mulfix(dpi*FLT2FIX(0.039), atts->height->value);
			break;
		case SVG_NUMBER_PT:
			real_height = dpi/12 * atts->height->value;
			break;
		case SVG_NUMBER_PC:
			real_height = dpi/6 * atts->height->value;
			break;
		default:
			real_height = INT2FIX(scene_height);
			break;
		}
	}

	if (atts->viewBox && atts->viewBox->width != 0 && atts->viewBox->height != 0) {
		Fixed scale, vp_w, vp_h;
		
		if (atts->preserveAspectRatio && atts->preserveAspectRatio->meetOrSlice==SVG_MEETORSLICE_MEET) {
			if (gf_divfix(real_width, atts->viewBox->width) > gf_divfix(real_height, atts->viewBox->height)) {
				scale = gf_divfix(real_height, atts->viewBox->height);
				vp_w = gf_mulfix(atts->viewBox->width, scale);
				vp_h = real_height;
			} else {
				scale = gf_divfix(real_width, atts->viewBox->width);
				vp_w = real_width;
				vp_h = gf_mulfix(atts->viewBox->height, scale);
			}
		} else {
			if (gf_divfix(real_width, atts->viewBox->width) < gf_divfix(real_height, atts->viewBox->height)) {
				scale = gf_divfix(real_height, atts->viewBox->height);
				vp_w = gf_mulfix(atts->viewBox->width, scale);
				vp_h = real_height;
			} else {
				scale = gf_divfix(real_width, atts->viewBox->width);
				vp_w = real_width;
				vp_h = gf_mulfix(atts->viewBox->height, scale);
			}
		}

		if (!atts->preserveAspectRatio || atts->preserveAspectRatio->align==SVG_PRESERVEASPECTRATIO_NONE) {
			mat.m[0] = gf_divfix(real_width, atts->viewBox->width);
			mat.m[4] = gf_divfix(real_height, atts->viewBox->height);
			mat.m[2] = - gf_muldiv(atts->viewBox->x, real_width, atts->viewBox->width); 
			mat.m[5] = - gf_muldiv(atts->viewBox->y, real_height, atts->viewBox->height); 
		} else {
			Fixed dx, dy;
			mat.m[0] = mat.m[4] = scale;
			mat.m[2] = - gf_mulfix(atts->viewBox->x, scale); 
			mat.m[5] = - gf_mulfix(atts->viewBox->y, scale); 

			dx = dy = 0;
			switch (atts->preserveAspectRatio->align) {
			case SVG_PRESERVEASPECTRATIO_XMINYMIN:
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
				dx = ( real_width - vp_w) / 2; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
				dx = real_width - vp_w; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMID:
				dy = ( real_height - vp_h) / 2; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMID:
				dx = ( real_width  - vp_w) / 2; 
				dy = ( real_height - vp_h) / 2; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMID:
				dx = real_width  - vp_w; 
				dy = ( real_height - vp_h) / 2; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMINYMAX:
				dy = real_height - vp_h; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
				dx = (real_width - vp_w) / 2; 
				dy = real_height - vp_h; 
				break;
			case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
				dx = real_width  - vp_w; 
				dy = real_height - vp_h; 
				break;
			}
			mat.m[2] += dx;
			mat.m[5] += dy;
			/*we need a clipper*/
			if (atts->preserveAspectRatio->meetOrSlice==SVG_MEETORSLICE_SLICE) {
				GF_Rect rc;
				rc.width = real_width;
				rc.height = real_height;
				if (!is_root) {
					rc.x = 0;
					rc.y = real_height;
					gf_mx2d_apply_rect(&eff->vb_transform, &rc);
				} else {
					rc.x = dx;
					rc.y = dy + real_height;
				}
				eff->surface->top_clipper = gf_rect_pixelize(&rc);
			}
		}
	}
	gf_mx2d_pre_multiply(&eff->vb_transform, &mat);
}

static void svg_render_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 viewport_color;
	GF_Matrix2D backup_matrix;
	GF_IRect top_clip;
	Bool is_root_svg = 0;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	RenderEffect2D *eff = (RenderEffect2D *) rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		SVGPropertiesPointers *svgp = (SVGPropertiesPointers *) gf_node_get_private(node);
		if (svgp) {
			gf_svg_properties_reset_pointers(svgp);
			free(svgp);
		}
		return;
	}
	
	
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	
	if (!eff->svg_props) {
		eff->svg_props = (SVGPropertiesPointers *) gf_node_get_private(node);
		is_root_svg = 1;
	}

	svg_render_base(node, &all_atts, eff, &backup_props, &backup_flags);

	/*enable or disable navigation*/
	eff->surface->render->navigation_disabled = (all_atts.zoomAndPan && *all_atts.zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}

	top_clip = eff->surface->top_clipper;
	gf_mx2d_copy(backup_matrix, eff->transform);
	
	gf_mx2d_init(eff->vb_transform);
	svg_set_viewport_transformation(eff, &all_atts, is_root_svg);
	gf_mx2d_pre_multiply(&eff->transform, &eff->vb_transform);

	if (!is_root_svg && (all_atts.x || all_atts.y)) 
		gf_mx2d_add_translation(&eff->transform, all_atts.x->value, all_atts.y->value);

	/* TODO: FIX ME: this only works for single SVG element in the doc*/
	if (is_root_svg && eff->svg_props->viewport_fill->type != SVG_PAINT_NONE) {
		viewport_color = GF_COL_ARGB_FIXED(eff->svg_props->viewport_fill_opacity->value, eff->svg_props->viewport_fill->color.red, eff->svg_props->viewport_fill->color.green, eff->svg_props->viewport_fill->color.blue);
		if (eff->surface->render->compositor->back_color != viewport_color) {
			eff->invalidate_all = 1;
			eff->surface->render->compositor->back_color = viewport_color;
		}
	}
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, eff);
	} else {
		svg_render_node_list(((SVG_Element *)node)->children, eff);
	}

	svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
	eff->surface->top_clipper = top_clip;
}

void svg_init_svg(Render2D *sr, GF_Node *node)
{
	SVGPropertiesPointers *svgp;

	GF_SAFEALLOC(svgp, SVGPropertiesPointers);
	gf_svg_properties_init_pointers(svgp);
	gf_node_set_private(node, svgp);

	gf_node_set_callback_function(node, svg_render_svg);
}

static void svg_render_g(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);

	RenderEffect2D *eff = (RenderEffect2D *) rs;

	SVGAllAttributes all_atts;

	if (is_destroy) return;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(((SVG_Element *)node)->children, eff);
		eff->trav_flags = prev_flags;

		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}	
	
	svg_apply_local_transformation(eff, &all_atts, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, eff);
	} else {
		svg_render_node_list(((SVG_Element *)node)->children, eff);
	}
	svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}


void svg_init_g(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_render_g);
}

static void svg_render_switch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGAllAttributes all_atts;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) return;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		svg_restore_parent_transformation(eff, &backup_matrix);
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}
	
	svg_apply_local_transformation(eff, &all_atts, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, eff);
	} else {
		GF_ChildNodeItem *l = ((SVG_Element *)node)->children;
		while (l) {
			if (1 /*eval_conditional(eff->surface->render->compositor, (SVG_Element*)l->node)*/) {
				svg_render_node((GF_Node*)l->node, eff);
				break;
			}
			l = l->next;
		}
	}
	svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}

void svg_init_switch(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_render_switch);
}

static void svg_drawable_post_render(Drawable *cs, SVGPropertiesPointers *backup_props, u32 *backup_flags,
								   RenderEffect2D *eff, Bool rectangular, Fixed path_length, SVGAllAttributes *atts)
{
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) 
			gf_path_get_bounds(cs->path, &eff->bounds);
		goto end;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	svg_apply_local_transformation(eff, atts, &backup_matrix);

	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) {
		if (rectangular) {
			if (ctx->h_texture && ctx->h_texture->transparent) {}
			else if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {}
			else if (ctx->transform.m[1] || ctx->transform.m[3]) {}
			else {
				ctx->flags &= ~CTX_IS_TRANSPARENT;
			}
		}
		if (path_length) ctx->aspect.pen_props.path_length = path_length;
		drawable_finalize_render(ctx, eff, NULL);
	}

	svg_restore_parent_transformation(eff, &backup_matrix);
end:
	memcpy(eff->svg_props, backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = *backup_flags;
}

static void svg_render_rect(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed rx = (all_atts.rx ? all_atts.rx->value : 0);
		Fixed ry = (all_atts.ry ? all_atts.ry->value : 0);
		Fixed x = (all_atts.x ? all_atts.x->value : 0);
		Fixed y = (all_atts.y ? all_atts.y->value : 0);
		Fixed width = (all_atts.width ? all_atts.width->value : 0);
		Fixed height = (all_atts.height ? all_atts.height->value : 0);

		drawable_reset_path(cs);
		if (rx || ry) {
			if (rx >= width/2) rx = width/2;
			if (ry >= height/2) ry = height/2;
			if (rx == 0) rx = ry;
			if (ry == 0) ry = rx;
			gf_path_add_move_to(cs->path, x+rx, y);
			gf_path_add_line_to(cs->path, x+width-rx, y);
			gf_path_add_quadratic_to(cs->path, x+width, y, x+width, y+ry);
			gf_path_add_line_to(cs->path, x+width, y+height-ry);
			gf_path_add_quadratic_to(cs->path, x+width, y+height, x+width-rx, y+height);
			gf_path_add_line_to(cs->path, x+rx, y+height);
			gf_path_add_quadratic_to(cs->path, x, y+height, x, y+height-ry);
			gf_path_add_line_to(cs->path, x, y+ry);
			gf_path_add_quadratic_to(cs->path, x, y, x+rx, y);
			gf_path_close(cs->path);
		} else {
			gf_path_add_move_to(cs->path, x, y);
			gf_path_add_line_to(cs->path, x+width, y);		
			gf_path_add_line_to(cs->path, x+width, y+height);		
			gf_path_add_line_to(cs->path, x, y+height);		
			gf_path_close(cs->path);		
		}
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_rect(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_rect);
}

static void svg_render_circle(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed r = 2*(all_atts.r ? all_atts.r->value : 0);
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, (all_atts.cx ? all_atts.cx->value : 0), (all_atts.cy ? all_atts.cy->value : 0), r, r);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;

	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_circle(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_circle);
}

static void svg_render_ellipse(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, (all_atts.cx ? all_atts.cx->value : 0), 
									  (all_atts.cy ? all_atts.cy->value : 0), 
									  (all_atts.rx ? 2*all_atts.rx->value : 0), 
									  (all_atts.ry ? 2*all_atts.ry->value : 0));
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_ellipse(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_ellipse);
}

static void svg_render_line(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_move_to(cs->path, (all_atts.x1 ? all_atts.x1->value : 0), (all_atts.y1 ? all_atts.y1->value : 0));
		gf_path_add_line_to(cs->path, (all_atts.x2 ? all_atts.x2->value : 0), (all_atts.y2 ? all_atts.y2->value : 0));
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_line(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_line);
}

static void svg_render_polyline(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i, nbPoints;
		if (all_atts.points) 
			nbPoints = gf_list_count(*all_atts.points);
		else 
			nbPoints = 0;
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = (SVG_Point *)gf_list_get(*all_atts.points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = (SVG_Point *)gf_list_get(*all_atts.points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_polyline(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_polyline);
}

static void svg_render_polygon(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i, nbPoints;
		if (all_atts.points) 
			nbPoints = gf_list_count(*all_atts.points);
		else 
			nbPoints = 0;
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = (SVG_Point *)gf_list_get(*all_atts.points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = (SVG_Point *)gf_list_get(*all_atts.points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, (RenderEffect2D *)rs, 1, 0, &all_atts);
}

void svg_init_polygon(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_polygon);
}

static void svg_destroy_path(GF_Node *node)
{
	Drawable *dr = gf_node_get_private(node);
#if USE_GF_PATH
	/* The path is the same as the one in the SVG node, don't delete it here */
	dr->path = NULL;
#endif
	drawable_del(dr);
}


static void svg_render_path(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		svg_destroy_path(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		drawable_draw(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, (RenderEffect2D *)rs, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		
#if USE_GF_PATH
		drawable_reset_path_outline(cs);
		cs->path = all_atts.d;
#else
		drawable_reset_path(cs);
		gf_svg_path_build(cs->path, all_atts.d->commands, all_atts.d->points);
#endif
		if (*(eff->svg_props->fill_rule)==GF_PATH_FILL_ZERO_NONZERO) cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_drawable_post_render(cs, &backup_props, &backup_flags, eff, 0, 
		(all_atts.pathLength && all_atts.pathLength->type==SVG_NUMBER_VALUE) ? all_atts.pathLength->value : 0,
		&all_atts);
}

void svg_init_path(Render2D *sr, GF_Node *node)
{
	Drawable *dr = drawable_stack_new(sr, node);
	gf_path_del(dr->path);
	dr->path = NULL;
	gf_node_set_callback_function(node, svg_render_path);
}

static void svg_render_a(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) return;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(((SVG_Element *)node)->children, eff);
		eff->trav_flags = prev_flags;

		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}	
	
	svg_apply_local_transformation(eff, &all_atts, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, eff);
	} else {
		svg_render_node_list(((SVG_Element *)node)->children, eff);
	}
	svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}

static void svg_a_handle_event(GF_Node *handler, GF_DOM_Event *event)
{
	GF_Renderer *compositor;
	GF_Event evt;
	SVG_Element *a;
	SVGAllAttributes all_atts;

	assert(gf_node_get_tag(event->currentTarget)==TAG_SVG_a);

	a = (SVG_Element *) event->currentTarget;
	gf_svg_flatten_attributes(a, &all_atts);

	compositor = (GF_Renderer *)gf_node_get_private((GF_Node *)handler);

	if (!compositor->user->EventProc) return;
	if (!all_atts.xlink_href) return;

	if (event->type==GF_EVENT_MOUSEOVER) {
		evt.type = GF_EVENT_NAVIGATE_INFO;
		evt.navigate.to_url = all_atts.xlink_href->iri;
		if (all_atts.xlink_title) evt.navigate.to_url = *all_atts.xlink_title;
		compositor->user->EventProc(compositor->user->opaque, &evt);
		return;
	}

	evt.type = GF_EVENT_NAVIGATE;
	
	if (all_atts.xlink_href->type == SVG_IRI_IRI) {
		evt.navigate.to_url = all_atts.xlink_href->iri;
		if (evt.navigate.to_url) {
			evt.navigate.param_count = 1;
			evt.navigate.parameters = (const char **) &all_atts.target;
			compositor->user->EventProc(compositor->user->opaque, &evt);
		}
	} else {
		u32 tag;
		if (!all_atts.xlink_href->target) {
			/* TODO: check if href can be resolved */
			return;
		} 
		tag = gf_node_get_tag((GF_Node *)all_atts.xlink_href->target);
		if (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard) {
			u32 i, count, found;
			SVGTimedAnimBaseElement *set = (SVGTimedAnimBaseElement*)all_atts.xlink_href->target;
			SMIL_Time *begin;
			GF_SAFEALLOC(begin, SMIL_Time);
			begin->type = GF_SMIL_TIME_EVENT_RESOLVED;
			begin->clock = gf_node_get_scene_time((GF_Node *)set);

			found = 0;
			count = gf_list_count(*set->timingp->begin);
			for (i=0; i<count; i++) {
				SMIL_Time *first = (SMIL_Time *)gf_list_get(*set->timingp->begin, i);
				/*remove past instanciations*/
				if ((first->type==GF_SMIL_TIME_EVENT_RESOLVED) && (first->clock < begin->clock)) {
					gf_list_rem(*set->timingp->begin, i);
					free(first);
					i--;
					count--;
					continue;
				}
				if ( (first->type == GF_SMIL_TIME_INDEFINITE) 
					|| ( (first->type == GF_SMIL_TIME_CLOCK) && (first->clock > begin->clock) ) 
				) {
					gf_list_insert(*set->timingp->begin, begin, i);
					found = 1;
					break;
				}
			}
			if (!found) gf_list_add(*set->timingp->begin, begin);
			gf_node_changed((GF_Node *)all_atts.xlink_href->target, NULL);
		}
	}
	return;
}

void svg_init_a(Render2D *sr, GF_Node *node)
{
	XMLEV_Event evt;
	SVG_handlerElement *handler;
	gf_node_set_callback_function(node, svg_render_a);

	/*listener for onClick event*/
	evt.parameter = 0;
	evt.type = GF_EVENT_CLICK;
	handler = gf_dom_listener_build(node, evt);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for activate event*/
	evt.type = GF_EVENT_ACTIVATE;
	handler = gf_dom_listener_build(node, evt);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

}

/* TODO: FIX ME we actually ignore the given sub_root since it is only valid 
	     when animations have been performed,
         animations evaluation (Render_base) should be part of the core renderer */
void R2D_RenderUse3(GF_Node *node, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
  	GF_Matrix2D translate;
	GF_Node *prev_use;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	SVGAllAttributes all_atts;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	svg_render_base(node, &all_atts, eff, &backup_props, &backup_flags);

	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_apply_local_transformation(eff, &all_atts, &backup_matrix);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			if (all_atts.xlink_href) gf_node_render(all_atts.xlink_href->target, eff);
			gf_mx2d_apply_rect(&translate, &eff->bounds);
		} 
		svg_restore_parent_transformation(eff, &backup_matrix);
		goto end;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	svg_apply_local_transformation(eff, &all_atts, &backup_matrix);

	gf_mx2d_pre_multiply(&eff->transform, &translate);
	prev_use = eff->parent_use;
	if (all_atts.xlink_href) {
		eff->parent_use = all_atts.xlink_href->target;
		gf_node_render(all_atts.xlink_href->target, eff);
		eff->parent_use = prev_use;
	}
	svg_restore_parent_transformation(eff, &backup_matrix);  

end:
	memcpy(eff->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = backup_flags;
}

#endif


