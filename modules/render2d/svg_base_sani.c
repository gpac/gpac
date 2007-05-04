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


#ifdef GPAC_ENABLE_SVG_SANI

/*
	This is the generic routine for child traversing - note we are not duplicating the effect
*/
void gf_svg_sani_apply_local_transformation(RenderEffect2D *eff, GF_Node *node, GF_Matrix2D *backup_matrix)
{
	gf_mx2d_copy(*backup_matrix, eff->transform);

	if (((SVG_SANI_TransformableElement *)node)->transform.is_ref) 
		gf_mx2d_copy(eff->transform, eff->vb_transform);

	if (((SVG_SANI_TransformableElement *)node)->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, ((SVG_SANI_TransformableElement *)node)->motionTransform);

	gf_mx2d_pre_multiply(&eff->transform, &((SVG_SANI_TransformableElement *)node)->transform.mat);
}

void gf_svg_sani_restore_parent_transformation(RenderEffect2D *eff, GF_Matrix2D *backup_matrix)
{
	gf_mx2d_copy(eff->transform, *backup_matrix);  
}

/* Set the viewport of the renderer based on the element that contains a viewport 
   TODO: change the SVG_SANI_svgElement into an element that has a viewport (more generic)
*/
static void gf_svg_sani_set_viewport_transformation(RenderEffect2D *eff, SVG_SANI_svgElement *svg, Bool is_root) 
{
	GF_Matrix2D mat;
	Fixed real_width, real_height;

	gf_mx2d_init(mat);

	if (is_root) {
		u32 scene_width = eff->surface->render->compositor->scene_width;
		u32 scene_height = eff->surface->render->compositor->scene_height;
		u32 dpi = 90; /* Should retrieve the dpi from the system */

		switch (svg->width.type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_width = INT2FIX(scene_width);
			break;
		case SVG_NUMBER_PERCENTAGE:
			/*u32 * fixed / u32*/
			real_width = scene_width*svg->width.value/100;
			break;
		case SVG_NUMBER_IN:
			real_width = dpi * svg->width.value;
			break;
		case SVG_NUMBER_CM:
			real_width = gf_mulfix(dpi*FLT2FIX(0.39), svg->width.value);
			break;
		case SVG_NUMBER_MM:
			real_width = gf_mulfix(dpi*FLT2FIX(0.039), svg->width.value);
			break;
		case SVG_NUMBER_PT:
			real_width = dpi/12 * svg->width.value;
			break;
		case SVG_NUMBER_PC:
			real_width = dpi/6 * svg->width.value;
			break;
		default:
			real_width = INT2FIX(scene_width);
			break;
		}

		switch (svg->height.type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_height = INT2FIX(scene_height);
			break;
		case SVG_NUMBER_PERCENTAGE:
			real_height = scene_height*svg->height.value/100;
			break;
		case SVG_NUMBER_IN:
			real_height = dpi * svg->height.value;
			break;
		case SVG_NUMBER_CM:
			real_height = gf_mulfix(dpi*FLT2FIX(0.39), svg->height.value);
			break;
		case SVG_NUMBER_MM:
			real_height = gf_mulfix(dpi*FLT2FIX(0.039), svg->height.value);
			break;
		case SVG_NUMBER_PT:
			real_height = dpi/12 * svg->height.value;
			break;
		case SVG_NUMBER_PC:
			real_height = dpi/6 * svg->height.value;
			break;
		default:
			real_height = INT2FIX(scene_height);
			break;
		}
	} else {
		real_width = real_height = 0;
		switch (svg->width.type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_width = svg->width.value;
			break;
		default:
			break;
		}
		switch (svg->height.type) {
		case SVG_NUMBER_VALUE:
		case SVG_NUMBER_PX:
			real_height = svg->height.value;
			break;
		default:
			break;
		}
	}
	
	if (!real_width || !real_height) return;


	if (svg->viewBox.is_set && svg->viewBox.width != 0 && svg->viewBox.height != 0) {
		Fixed scale, vp_w, vp_h;
		if (svg->preserveAspectRatio.meetOrSlice==SVG_MEETORSLICE_MEET) {
			if (gf_divfix(real_width, svg->viewBox.width) > gf_divfix(real_height, svg->viewBox.height)) {
				scale = gf_divfix(real_height, svg->viewBox.height);
				vp_w = gf_mulfix(svg->viewBox.width, scale);
				vp_h = real_height;
			} else {
				scale = gf_divfix(real_width, svg->viewBox.width);
				vp_w = real_width;
				vp_h = gf_mulfix(svg->viewBox.height, scale);
			}
		} else {
			if (gf_divfix(real_width, svg->viewBox.width) < gf_divfix(real_height, svg->viewBox.height)) {
				scale = gf_divfix(real_height, svg->viewBox.height);
				vp_w = gf_mulfix(svg->viewBox.width, scale);
				vp_h = real_height;
			} else {
				scale = gf_divfix(real_width, svg->viewBox.width);
				vp_w = real_width;
				vp_h = gf_mulfix(svg->viewBox.height, scale);
			}
		}
		if (svg->preserveAspectRatio.align==SVG_PRESERVEASPECTRATIO_NONE) {
			mat.m[0] = gf_divfix(real_width, svg->viewBox.width);
			mat.m[4] = gf_divfix(real_height, svg->viewBox.height);
			mat.m[2] = - gf_muldiv(svg->viewBox.x, real_width, svg->viewBox.width); 
			mat.m[5] = - gf_muldiv(svg->viewBox.y, real_height, svg->viewBox.height); 
		} else {
			Fixed dx, dy;
			mat.m[0] = mat.m[4] = scale;
			mat.m[2] = - gf_mulfix(svg->viewBox.x, scale); 
			mat.m[5] = - gf_mulfix(svg->viewBox.y, scale); 

			dx = dy = 0;
			switch (svg->preserveAspectRatio.align) {
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
			if (svg->preserveAspectRatio.meetOrSlice==SVG_MEETORSLICE_SLICE) {
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

/* Node specific rendering functions
   All the nodes follow the same principles:
	* Check if the display property is not set to none, otherwise do not render
	* Back-up of the coordinate system & apply geometric transformation if any
	* Render the children if any or the shape if leaf node
	* Restore coordinate system
 */
static void svg_sani_render_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 viewport_color;
	GF_Matrix2D backup_matrix;
	GF_IRect top_clip;
	Bool is_root_svg = 1;
	SVG_SANI_svgElement *svg = (SVG_SANI_svgElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) return;

	/*enable or disable navigation*/
	eff->surface->render->navigation_disabled = (svg->zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	svg_sani_render_base(node, eff);

	if (svg->display == SVG_DISPLAY_NONE) return;

	top_clip = eff->surface->top_clipper;
	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_init(eff->vb_transform);
	gf_svg_sani_set_viewport_transformation(eff, svg, is_root_svg);
	gf_mx2d_pre_multiply(&eff->transform, &eff->vb_transform);

	if (!is_root_svg && (svg->x.value || svg->y.value)) 
		gf_mx2d_add_translation(&eff->transform, svg->x.value, svg->y.value);

	/* TODO: FIX ME: this only works for single SVG element in the doc*/
	if (is_root_svg && svg->viewport_fill.type != SVG_PAINT_NONE) {
		viewport_color = GF_COL_ARGB_FIXED(svg->viewport_fill_opacity.value, svg->viewport_fill.color.red, svg->viewport_fill.color.green, svg->viewport_fill.color.blue);
		if (eff->surface->render->compositor->back_color != viewport_color) {
			eff->invalidate_all = 1;
			eff->surface->render->compositor->back_color = viewport_color;
		}
	}
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, svg->children, eff);
	} else {
		svg_render_node_list(svg->children, eff);
	}

	gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
	eff->surface->top_clipper = top_clip;
}

void svg_sani_init_svg(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_sani_render_svg);
}

static void svg_sani_render_g(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVG_SANI_gElement *g = (SVG_SANI_gElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;
	if (is_destroy) return;

	svg_sani_render_base(node, eff);

	if (g->display == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(g->children, eff);
		eff->trav_flags = prev_flags;
		return;
	}	
	
	gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, g->children, eff);
	} else {
		svg_render_node_list(g->children, eff);
	}
	gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
}


void svg_sani_init_g(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_sani_render_g);
}

static Bool svg_sani_eval_conditional(GF_Renderer *sr, SVG_SANI_Element *elt)
{
	u32 i, count;
	Bool found;
	const char *lang_3cc, *lang_2cc;
	if (!elt->conditional) return 1;

	count = gf_list_count(elt->conditional->requiredFeatures);
	for (i=0;i<count;i++) {
		XMLRI *iri = (XMLRI *)gf_list_get(elt->conditional->requiredFeatures, i);
		if (!iri->string) continue;
		/*TODO FIXME: be a bit more precise :)*/
		if (!strnicmp(iri->string, "http://www.w3.org/", 18)) {
			char *feat = strrchr(iri->string, '#');
			if (!feat) continue;
			feat++;
			if (!strcmp(feat, "SVGDOM")) return 0;
			if (!strcmp(feat, "Font")) return 0;
			continue;
		}
		return 0;
	}
	count = gf_list_count(elt->conditional->requiredExtensions);
	if (count) return 0;

	lang_3cc = gf_cfg_get_key(sr->user->config, "Systems", "Language3CC");
	if (!lang_3cc) lang_3cc = "und";
	lang_2cc = gf_cfg_get_key(sr->user->config, "Systems", "Language2CC");
	if (!lang_2cc) lang_2cc = "un";

	count = gf_list_count(elt->conditional->systemLanguage);
	found = count ? 0 : 1;
	for (i=0;i<count;i++) {
		char *lang = (char*)gf_list_get(elt->conditional->systemLanguage, i);
		/*3 char-code*/
		if (strlen(lang)==3) {
			if (!stricmp(lang, lang_3cc)) { found = 1; break; }
		}
		/*2 char-code, only check first 2 chars - TODO FIXME*/
		else if (!strnicmp(lang, lang_2cc, 2)) { found = 1; break; }
	}
	if (!found) return 0;

	return 1;
}


static void svg_sani_render_switch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVG_SANI_switchElement *s = (SVG_SANI_switchElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) return;

	svg_sani_render_base(node, eff);

	if (s->display == SVG_DISPLAY_NONE) {
		gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
		return;
	}
	
	gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, s->children, eff);
	} else {
		GF_ChildNodeItem *l = s->children;
		while (l) {
			if (svg_sani_eval_conditional(eff->surface->render->compositor, (SVG_SANI_Element*)l->node)) {
				svg_render_node((GF_Node*)l->node, eff);
				break;
			}
			l = l->next;
		}
	}
	gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
}

void svg_sani_init_switch(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_sani_render_switch);
}


static void svg_sani_DrawablePostRender(Drawable *cs, SVG_SANI_TransformableElement *elt, RenderEffect2D *eff, 
									Bool rectangular, Fixed path_length)
{
	GF_FieldInfo info;
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_node_get_field_by_name(cs->node, "display", &info);
		if (*(SVG_Display *)info.far_ptr == SVG_DISPLAY_NONE) gf_path_get_bounds(cs->path, &eff->bounds);
		return;
	}

	gf_node_get_field_by_name(cs->node, "display", &info);
	if (*(SVG_Display *)info.far_ptr == SVG_DISPLAY_NONE) return;
		
	gf_node_get_field_by_name(cs->node, "visibility", &info);
	if (*(SVG_Visibility *)info.far_ptr == SVG_VISIBILITY_HIDDEN) return;

	gf_svg_sani_apply_local_transformation(eff, (GF_Node *)elt, &backup_matrix);

	gf_node_get_field_by_name(cs->node, "fill-rule", &info);
	if (*(SVG_FillRule *)info.far_ptr == SVG_FILLRULE_NONZERO)
		cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
	else 
		cs->path->flags &= ~GF_PATH_FILL_ZERO_NONZERO;

	ctx = svg_sani_drawable_init_context(cs, eff);
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

	gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
}

static void svg_sani_render_rect(GF_Node *node, void *rs, Bool is_destroy)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	SVG_SANI_rectElement *rect = (SVG_SANI_rectElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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

	svg_sani_render_base(node, (RenderEffect2D *)rs);
	
	/* 3) for a leaf node
	   Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed rx = rect->rx.value;
		Fixed ry = rect->ry.value;
		Fixed x = rect->x.value;
		Fixed y = rect->y.value;
		Fixed width = rect->width.value;
		Fixed height = rect->height.value;

		//fprintf(stdout, "Rebuilding rect %8x\n",rect);
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
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)rect, (RenderEffect2D *)rs, 1, 0);
}

void svg_sani_init_rect(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_rect);
}

static void svg_sani_render_circle(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_circleElement *circle = (SVG_SANI_circleElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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
	svg_sani_render_base(node, (RenderEffect2D *)rs);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed r = 2*circle->r.value;
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, circle->cx.value, circle->cy.value, r, r);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)circle, (RenderEffect2D *)rs, 0, 0);
}

void svg_sani_init_circle(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_circle);
}

static void svg_sani_render_ellipse(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_ellipseElement *ellipse = (SVG_SANI_ellipseElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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

	svg_sani_render_base(node, (RenderEffect2D *)rs);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, ellipse->cx.value, ellipse->cy.value, 2*ellipse->rx.value, 2*ellipse->ry.value);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)ellipse, (RenderEffect2D *)rs, 0, 0);
}

void svg_sani_init_ellipse(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_ellipse);
}

static void svg_sani_render_line(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_lineElement *line = (SVG_SANI_lineElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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

	svg_sani_render_base(node, (RenderEffect2D *)rs);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_move_to(cs->path, line->x1.value, line->y1.value);
		gf_path_add_line_to(cs->path, line->x2.value, line->y2.value);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)line, (RenderEffect2D *)rs, 0, 0);
}

void svg_sani_init_line(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_line);
}

static void svg_sani_render_polyline(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_polylineElement *polyline = (SVG_SANI_polylineElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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

	svg_sani_render_base(node, (RenderEffect2D *)rs);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i;
		u32 nbPoints = gf_list_count(polyline->points);
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = (SVG_Point *)gf_list_get(polyline->points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = (SVG_Point *)gf_list_get(polyline->points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)polyline, (RenderEffect2D *)rs, 0, 0);
}

void svg_sani_init_polyline(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_polyline);
}

static void svg_sani_render_polygon(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_polygonElement *polygon = (SVG_SANI_polygonElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

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

	svg_sani_render_base(node, (RenderEffect2D *)rs);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i;
		u32 nbPoints = gf_list_count(polygon->points);
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = (SVG_Point *)gf_list_get(polygon->points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = (SVG_Point *)gf_list_get(polygon->points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
			gf_path_close(cs->path);
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)polygon, (RenderEffect2D *)rs, 0, 0);
}

void svg_sani_init_polygon(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_polygon);
}

static void svg_sani_destroy_path(GF_Node *node)
{
	Drawable *dr = gf_node_get_private(node);
#if USE_GF_PATH
	/* The path is the same as the one in the SVG node, don't delete it here */
	dr->path = NULL;
#endif
	drawable_del(dr);
}

static void svg_sani_render_path(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_pathElement *path = (SVG_SANI_pathElement *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) {
		svg_sani_destroy_path(node);
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

	svg_sani_render_base(node, eff);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
#if USE_GF_PATH
		cs->path = &path->d;
#else
		drawable_reset_path(cs);
		gf_svg_path_build(cs->path, path->d.commands, path->d.points);
#endif
		if (path->fill_rule==GF_PATH_FILL_ZERO_NONZERO) cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;

		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	svg_sani_DrawablePostRender(cs, (SVG_SANI_TransformableElement *)path, eff, 0, (path->pathLength.type==SVG_NUMBER_VALUE) ? path->pathLength.value : 0);
}

void svg_sani_init_path(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_sani_render_path);
}

/* end of rendering of basic shapes */


static void svg_sani_render_a(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVG_SANI_aElement *a = (SVG_SANI_aElement *) node;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) return;

	svg_sani_render_base(node, eff);

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);
		if (a->display != SVG_DISPLAY_NONE) svg_get_nodes_bounds(node, a->children, eff);
		gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);  
		return;
	}

	if (a->display != SVG_DISPLAY_NONE && a->visibility != SVG_VISIBILITY_HIDDEN) {
		gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);
		svg_render_node_list(a->children, eff);
		gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);  
	}
}

static void svg_sani_a_HandleEvent(GF_Node *handler, GF_DOM_Event *event)
{
	GF_Renderer *compositor;
	GF_Event evt;
	SVG_SANI_aElement *a;

	assert(gf_node_get_tag(event->currentTarget)==TAG_SVG_SANI_a);

	a = (SVG_SANI_aElement *) event->currentTarget;
	compositor = (GF_Renderer *)gf_node_get_private(handler);

	if (!compositor->user->EventProc) return;

	evt.type = GF_EVENT_NAVIGATE;
	
	if (a->xlink->href.type == XMLRI_STRING) {
		evt.navigate.to_url = a->xlink->href.iri;
		if (evt.navigate.to_url) {
			evt.navigate.param_count = 1;
			evt.navigate.parameters = (const char **) &a->target;
			compositor->user->EventProc(compositor->user->opaque, &evt);
		}
	} else {
		u32 tag;
		if (!a->xlink->href.target) {
			/* TODO: check if href can be resolved */
			return;
		} 
		tag = gf_node_get_tag((GF_Node *)a->xlink->href.target);
		if (tag == TAG_SVG_SANI_set ||
			tag == TAG_SVG_SANI_animate ||
			tag == TAG_SVG_SANI_animateColor ||
			tag == TAG_SVG_SANI_animateTransform ||
			tag == TAG_SVG_SANI_animateMotion || 
			tag == TAG_SVG_SANI_discard) {
			u32 i, count, found;
			SVG_SANI_setElement *set = (SVG_SANI_setElement *)a->xlink->href.target;
			SMIL_Time *begin;
			GF_SAFEALLOC(begin, SMIL_Time);
			begin->type = GF_SMIL_TIME_EVENT_RESOLVED;
			begin->clock = gf_node_get_scene_time((GF_Node *)set);

			found = 0;
			count = gf_list_count(set->timing->begin);
			for (i=0; i<count; i++) {
				SMIL_Time *first = (SMIL_Time *)gf_list_get(set->timing->begin, i);
				/*remove past instanciations*/
				if ((first->type==GF_SMIL_TIME_EVENT_RESOLVED) && (first->clock < begin->clock)) {
					gf_list_rem(set->timing->begin, i);
					free(first);
					i--;
					count--;
					continue;
				}
				if ( (first->type == GF_SMIL_TIME_INDEFINITE) 
					|| ( (first->type == GF_SMIL_TIME_CLOCK) && (first->clock > begin->clock) ) 
				) {
					gf_list_insert(set->timing->begin, begin, i);
					found = 1;
					break;
				}
			}
			if (!found) gf_list_add(set->timing->begin, begin);
			gf_node_changed((GF_Node *)a->xlink->href.target, NULL);
		}
	}
	return;
}

void svg_sani_init_a(Render2D *sr, GF_Node *node)
{
	SVG_SANI_handlerElement *handler;
	gf_node_set_callback_function(node, svg_sani_render_a);

	/*listener for onClick event*/
	handler = gf_dom_listener_build(node, GF_EVENT_CLICK, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_sani_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for activate event*/
	handler = gf_dom_listener_build(node, GF_EVENT_ACTIVATE, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_sani_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for mouseover event*/
	handler = gf_dom_listener_build(node, GF_EVENT_MOUSEOVER, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_sani_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);
}
/* end of Interactive SVG elements */


/*SVG gradient common stuff*/
typedef struct
{
	GF_TextureHandler txh;
	u32 *cols;
	Fixed *keys;
	u32 nb_col;
} SVG_SANI_GradientStack;

static void svg_sani_DestroyGradient(GF_Node *node)
{
	SVG_SANI_GradientStack *st = (SVG_SANI_GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	if (st->cols) free(st->cols);
	if (st->keys) free(st->keys);
	free(st);
}

static void svg_sani_UpdateGradient(SVG_SANI_GradientStack *st, GF_ChildNodeItem *children)
{
	u32 count;
	Fixed alpha, max_offset;

	if (!gf_node_dirty_get(st->txh.owner)) return;
	gf_node_dirty_clear(st->txh.owner, 0);

	st->txh.needs_refresh = 1;
	st->txh.transparent = 0;
	count = gf_node_list_get_count(children);
	st->nb_col = 0;
	st->cols = (u32*)realloc(st->cols, sizeof(u32)*count);
	st->keys = (Fixed*)realloc(st->keys, sizeof(Fixed)*count);

	max_offset = 0;
	while (children) {
		Fixed key;
		SVG_SANI_stopElement *gstop = (SVG_SANI_stopElement *) children->node;
		children = children->next;
		if (gf_node_get_tag((GF_Node *)gstop) != TAG_SVG_SANI_stop) continue;

		if (gstop->stop_opacity.type==SVG_NUMBER_VALUE) alpha = gstop->stop_opacity.value;
		else alpha = FIX_ONE;
		st->cols[st->nb_col] = GF_COL_ARGB_FIXED(alpha, gstop->stop_color.color.red, gstop->stop_color.color.green, gstop->stop_color.color.blue);
		key = gstop->offset.value;
		if (gstop->offset.value>FIX_ONE) key/=100; 
		if (key>max_offset) max_offset=key;
		else key = max_offset;
		st->keys[st->nb_col] = key;

		st->nb_col++;
		if (alpha!=FIX_ONE) st->txh.transparent = 1;
	}
	st->txh.compositor->r2d->stencil_set_gradient_interpolation(st->txh.hwtx, st->keys, st->cols, st->nb_col);
	st->txh.compositor->r2d->stencil_set_gradient_mode(st->txh.hwtx, /*lg->spreadMethod*/ GF_GRADIENT_MODE_PAD);
}

static void svg_sani_render_PaintServer(GF_Node *node, void *rs, Bool is_destroy)
{
	SVG_SANI_Element *elt = (SVG_SANI_Element *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) {
		if (gf_node_get_private(node)) svg_sani_DestroyGradient(node);
		return;
	}

	svg_sani_render_base(node, eff);
	
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		return;
	} else {
		svg_render_node_list(elt->children, eff);
	}
}


/* linear gradient */
static void svg_sani_UpdateLinearGradient(GF_TextureHandler *txh)
{
	SVG_SANI_linearGradientElement *lg = (SVG_SANI_linearGradientElement *) txh->owner;
	SVG_SANI_GradientStack *st = (SVG_SANI_GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_LINEAR_GRADIENT);

	svg_sani_UpdateGradient(st, lg->children);
}

static void svg_sani_LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f start, end;
	SVG_SANI_linearGradientElement *lg = (SVG_SANI_linearGradientElement *) txh->owner;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	start.x = lg->x1.value;
	if (lg->x1.type==SVG_NUMBER_PERCENTAGE) start.x /= 100;
	start.y = lg->y1.value;
	if (lg->y1.type==SVG_NUMBER_PERCENTAGE) start.y /= 100;
	end.x = lg->x2.value;
	if (lg->x2.type==SVG_NUMBER_PERCENTAGE) end.x /= 100;
	end.y = lg->y2.value;
	if (lg->y2.type==SVG_NUMBER_PERCENTAGE) end.x /= 100;

	txh->compositor->r2d->stencil_set_gradient_mode(txh->hwtx, (GF_GradientMode) lg->spreadMethod);

	gf_mx2d_copy(*mat, lg->gradientTransform.mat);

	if (lg->gradientUnits==SVG_GRADIENTUNITS_OBJECT) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x - 1, bounds->y  - bounds->height - 1);
	}
	txh->compositor->r2d->stencil_set_linear_gradient(txh->hwtx, start.x, start.y, end.x, end.y);
}

void svg_sani_init_linearGradient(Render2D *sr, GF_Node *node)
{
	SVG_SANI_GradientStack *st;
	GF_SAFEALLOC(st, SVG_SANI_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = svg_sani_UpdateLinearGradient;

	st->txh.compute_gradient_matrix = svg_sani_LG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_sani_render_PaintServer);
}

/* radial gradient */

static void svg_sani_UpdateRadialGradient(GF_TextureHandler *txh)
{
	SVG_SANI_radialGradientElement *rg = (SVG_SANI_radialGradientElement *) txh->owner;
	SVG_SANI_GradientStack *st = (SVG_SANI_GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_RADIAL_GRADIENT);
	svg_sani_UpdateGradient(st, rg->children);
}

static void svg_sani_rG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f center, focal;
	Fixed radius;
	SVG_SANI_radialGradientElement *rg = (SVG_SANI_radialGradientElement *) txh->owner;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	gf_mx2d_copy(*mat, rg->gradientTransform.mat);

	radius = rg->r.value;
	if (rg->r.type==SVG_NUMBER_PERCENTAGE) radius /= 100;
	center.x = rg->cx.value;
	if (rg->cx.type==SVG_NUMBER_PERCENTAGE) center.x /= 100;
	center.y = rg->cy.value;
	if (rg->cy.type==SVG_NUMBER_PERCENTAGE) center.y /= 100;

	txh->compositor->r2d->stencil_set_gradient_mode(txh->hwtx, (GF_GradientMode) rg->spreadMethod);

	focal.x = rg->fx.value;
	if (rg->fx.type==SVG_NUMBER_PERCENTAGE) focal.x /= 100;
	focal.y = rg->fy.value;
	if (rg->fy.type==SVG_NUMBER_PERCENTAGE) focal.y /= 100;

	if (rg->gradientUnits==SVG_GRADIENTUNITS_OBJECT) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x, bounds->y  - bounds->height);
	} else if ((rg->fx.value==rg->fy.value) && (rg->fx.value==FIX_ONE/2)) {
		focal.x = center.x;
		focal.y = center.y;
	}
	txh->compositor->r2d->stencil_set_radial_gradient(txh->hwtx, center.x, center.y, focal.x, focal.y, radius, radius);
}

void svg_sani_init_radialGradient(Render2D *sr, GF_Node *node)
{
	SVG_SANI_GradientStack *st;
	GF_SAFEALLOC(st, SVG_SANI_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = svg_sani_UpdateRadialGradient;
	st->txh.compute_gradient_matrix = svg_sani_rG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_sani_render_PaintServer);
}

void svg_sani_init_solidColor(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_sani_render_PaintServer);
}

void svg_sani_init_stop(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_sani_render_PaintServer);
}

GF_TextureHandler *svg_sani_gradient_get_texture(GF_Node *node)
{
	SVG_SANI_Element *g = (SVG_SANI_Element *)node;
	SVG_SANI_GradientStack *st;
	if (g->xlink->href.target) g = g->xlink->href.target;
	st = (SVG_SANI_GradientStack*) gf_node_get_private((GF_Node *)g);
	return st->nb_col ? &st->txh : NULL;
}

void svg_sani_render_base(GF_Node *node, RenderEffect2D *eff)
{	
	gf_svg_sani_apply_animations(node);
	eff->svg_flags |= gf_node_dirty_get(node);
}

void r2d_render_svg_sani_use(GF_Node *node, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
  	GF_Matrix2D translate;
	GF_Node *prev_use;
	SVG_SANI_useElement *use = (SVG_SANI_useElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	svg_sani_render_base(node, eff);

	gf_mx2d_init(translate);
	translate.m[2] = use->x.value;
	translate.m[5] = use->y.value;

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);
		if (use->display != SVG_DISPLAY_NONE) {
			gf_node_render((GF_Node*)use->xlink->href.target, eff);
			gf_mx2d_apply_rect(&translate, &eff->bounds);
		} 
		gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);
		return;
	}

	if (use->display == SVG_DISPLAY_NONE ||
		use->visibility == SVG_VISIBILITY_HIDDEN) {
		return;
	}

	gf_svg_sani_apply_local_transformation(eff, node, &backup_matrix);

	gf_mx2d_pre_multiply(&eff->transform, &translate);
	prev_use = eff->parent_use;
	eff->parent_use = use->xlink->href.target;
	gf_node_render(use->xlink->href.target, eff);
	eff->parent_use = prev_use;
	gf_svg_sani_restore_parent_transformation(eff, &backup_matrix);  
}


#endif	/*GPAC_ENABLE_SVG_SANI*/

#endif //GPAC_DISABLE_SVG
