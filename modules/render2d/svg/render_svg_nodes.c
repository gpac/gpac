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

#include "../visualsurface2d.h"

#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"


/*
	This is the generic routine for child traversing - note we are not duplicating the effect
*/
void svg_render_node(GF_Node *node, RenderEffect2D *eff)
{
	Bool has_listener = gf_node_listener_count(node);
	if (gf_node_listener_count(node)) {
		eff->nb_listeners++;
		gf_node_render(node, eff);
		eff->nb_listeners--;
	} else {
		gf_node_render(node, eff);
	}
}
void svg_render_node_list(GF_List *children, RenderEffect2D *eff)
{
	GF_Node *node;
	u32 i, count = gf_list_count(children);
	for (i=0; i<count; i++) {
		node = gf_list_get(children, i);
		svg_render_node(node, eff);
	}
}

void svg_get_nodes_bounds(GF_Node *self, GF_List *children, RenderEffect2D *eff)
{
	GF_Rect rc;
	GF_Matrix2D cur_mx;
	GF_Node *node;
	u32 i, count = gf_list_count(children);

	if (!eff->for_node) return;

	gf_mx2d_copy(cur_mx, eff->transform);
	rc = gf_rect_center(0,0);
	for (i=0; i<count; i++) {
		gf_mx2d_init(eff->transform);
		eff->bounds = gf_rect_center(0,0);
		node = gf_list_get(children, i);
		gf_node_render(node, eff);
		/*we hit the target node*/
		if (node == eff->for_node) eff->for_node = NULL;
		if (!eff->for_node) return;

		gf_mx2d_apply_rect(&eff->transform, &eff->bounds);
		gf_rect_union(&rc, &eff->bounds);
	}
	gf_mx2d_copy(eff->transform, cur_mx);
	eff->bounds = rc;
}

/* Sets all SVG Properties to their initial value 
   The properties are then updated when going down the tree
   TODO: Check that all properties are there */
static void SVGInitProperties(SVGStylingProperties *svg_props) 
{
	if (!svg_props) return;

	GF_SAFEALLOC(svg_props->fill, sizeof(SVG_Paint));
	svg_props->fill->type = SVG_PAINT_COLOR;
	GF_SAFEALLOC(svg_props->fill->color, sizeof(SVG_Color));
	svg_props->fill->color->type = SVG_COLOR_RGBCOLOR;
	svg_props->fill->color->red = 0;
	svg_props->fill->color->green = 0;
	svg_props->fill->color->blue = 0;

	GF_SAFEALLOC(svg_props->fill_rule, sizeof(SVG_FillRule));
	*svg_props->fill_rule = SVG_FILLRULE_NONZERO;

	GF_SAFEALLOC(svg_props->fill_opacity, sizeof(SVG_Opacity));
	svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->fill_opacity->value = FIX_ONE;
	
	GF_SAFEALLOC(svg_props->stroke, sizeof(SVG_Paint));
	svg_props->stroke->type = SVG_PAINT_NONE;
	GF_SAFEALLOC(svg_props->stroke->color, sizeof(SVG_Color));
	svg_props->stroke->color->type = SVG_COLOR_RGBCOLOR;

	GF_SAFEALLOC(svg_props->stroke_opacity, sizeof(SVG_Opacity));
	svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stroke_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_width, sizeof(SVG_StrokeWidth));
	svg_props->stroke_width->type = SVG_NUMBER_VALUE;
	svg_props->stroke_width->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_linecap, sizeof(SVG_StrokeLineCap));
	*(svg_props->stroke_linecap) = SVG_STROKELINECAP_BUTT;
	GF_SAFEALLOC(svg_props->stroke_linejoin, sizeof(SVG_StrokeLineJoin));
	*(svg_props->stroke_linejoin) = SVG_STROKELINEJOIN_MITER;

	GF_SAFEALLOC(svg_props->stroke_miterlimit, sizeof(SVG_StrokeMiterLimit));
	svg_props->stroke_miterlimit->type = SVG_NUMBER_VALUE;
	svg_props->stroke_miterlimit->value = 4*FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_dashoffset , sizeof(SVG_StrokeDashOffset));
	svg_props->stroke_dashoffset->type = SVG_NUMBER_VALUE;
	svg_props->stroke_dashoffset->value = 0;

	GF_SAFEALLOC(svg_props->stroke_dasharray, sizeof(SVG_StrokeDashArray));
	svg_props->stroke_dasharray->type = SVG_STROKEDASHARRAY_NONE;

	GF_SAFEALLOC(svg_props->font_family, sizeof(SVG_FontFamily));
	svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
	svg_props->font_family->value = strdup("Arial");

	GF_SAFEALLOC(svg_props->font_size, sizeof(SVG_FontSize));
	svg_props->font_size->type = SVG_NUMBER_VALUE;
	svg_props->font_size->value = 12*FIX_ONE;

	GF_SAFEALLOC(svg_props->font_style, sizeof(SVG_FontStyle));
	*(svg_props->font_style) = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->color, sizeof(SVG_Color));
	svg_props->color->type = SVG_COLOR_RGBCOLOR;
	/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */

	GF_SAFEALLOC(svg_props->text_anchor, sizeof(SVG_TextAnchor));
	*svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->visibility, sizeof(SVG_Visibility));
	*svg_props->visibility = SVG_VISIBILITY_VISIBLE;

	GF_SAFEALLOC(svg_props->display, sizeof(SVG_Display));
	*svg_props->display = SVG_DISPLAY_INLINE;

}

static void SVGResetProperties(SVGStylingProperties *svg_props)
{
	if (!svg_props) return;
	SVG_DeletePaint(svg_props->fill);
	if(svg_props->fill_rule) free(svg_props->fill_rule);
	if(svg_props->fill_opacity) free(svg_props->fill_opacity);
	SVG_DeletePaint(svg_props->stroke);
	if(svg_props->stroke_opacity) free(svg_props->stroke_opacity);
	if(svg_props->stroke_width) free(svg_props->stroke_width);
	if(svg_props->stroke_linecap) free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_dashoffset) free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) free(svg_props->stroke_dasharray->array.vals);
		free(svg_props->stroke_dasharray);
	}
	if(svg_props->font_family) {
		if (svg_props->font_family->value) free(svg_props->font_family->value);
		free(svg_props->font_family);
	}
	if(svg_props->font_size) free(svg_props->font_size);
	if(svg_props->font_style) free(svg_props->font_style);
	if(svg_props->color) free(svg_props->color);
	if(svg_props->text_anchor) free(svg_props->text_anchor);
	if(svg_props->visibility) free(svg_props->visibility);
	if(svg_props->display) free(svg_props->display);
	memset(svg_props, 0, sizeof(SVGStylingProperties));
}

/* Updates the SVG Styling Properties of the renderer (render_svg_props) with the properties
   of the current SVG element (current_svg_props). Only the properties in current_svg_props 
   with a value different than inherit are updated.
   This function implements inheritance. 
   TODO: Check if all properties are implemented */
void SVGApplyProperties(SVGStylingProperties *render_svg_props, SVGStylingProperties current_svg_props)
{
	if (!render_svg_props) return;

	if (current_svg_props.color && current_svg_props.color->type != SVG_COLOR_INHERIT) {
		render_svg_props->color = current_svg_props.color;
	}
	if (current_svg_props.fill && current_svg_props.fill->type != SVG_PAINT_INHERIT) {
		render_svg_props->fill = current_svg_props.fill;
	}
	if (current_svg_props.fill_rule && *current_svg_props.fill_rule != SVG_FILLRULE_INHERIT) {
		render_svg_props->fill_rule = current_svg_props.fill_rule;
	}
	if (current_svg_props.fill_opacity && current_svg_props.fill_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->fill_opacity = current_svg_props.fill_opacity;
	}
	if (current_svg_props.stroke && current_svg_props.stroke->type != SVG_PAINT_INHERIT) {
		render_svg_props->stroke = current_svg_props.stroke;
	}
	if (current_svg_props.stroke_opacity && current_svg_props.stroke_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_opacity = current_svg_props.stroke_opacity;
	}
	if (current_svg_props.stroke_width && current_svg_props.stroke_width->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_width = current_svg_props.stroke_width;
	}
	if (current_svg_props.stroke_miterlimit && current_svg_props.stroke_miterlimit->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_miterlimit = current_svg_props.stroke_miterlimit;
	}
	if (current_svg_props.stroke_linecap && *current_svg_props.stroke_linecap != SVG_STROKELINECAP_INHERIT) {
		render_svg_props->stroke_linecap = current_svg_props.stroke_linecap;
	}
	if (current_svg_props.stroke_linejoin && *current_svg_props.stroke_linejoin != SVG_STROKELINEJOIN_INHERIT) {
		render_svg_props->stroke_linejoin = current_svg_props.stroke_linejoin;
	}
	if (current_svg_props.stroke_dashoffset && current_svg_props.stroke_dashoffset->value != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_dashoffset = current_svg_props.stroke_dashoffset;
	}
	if (current_svg_props.stroke_dasharray && current_svg_props.stroke_dasharray->type != SVG_STROKEDASHARRAY_INHERIT) {
		render_svg_props->stroke_dasharray = current_svg_props.stroke_dasharray;
	}
	if (current_svg_props.font_family && current_svg_props.font_family->type != SVG_FONTFAMILY_INHERIT) {
		render_svg_props->font_family = current_svg_props.font_family;
	}
	if (current_svg_props.font_size && current_svg_props.font_size->type != SVG_NUMBER_INHERIT) {
		render_svg_props->font_size = current_svg_props.font_size;
	}
	if (current_svg_props.font_style && *current_svg_props.font_style != SVG_FONTSTYLE_INHERIT) {
		render_svg_props->font_style = current_svg_props.font_style;
	}
	if (current_svg_props.text_anchor && *current_svg_props.text_anchor != SVG_TEXTANCHOR_INHERIT) {
		render_svg_props->text_anchor = current_svg_props.text_anchor;
	}
	if (current_svg_props.visibility && *current_svg_props.visibility != SVG_VISIBILITY_INHERIT) {
		render_svg_props->visibility = current_svg_props.visibility;
	}
	if (current_svg_props.display && *current_svg_props.display != SVG_DISPLAY_INHERIT) {
		render_svg_props->display = current_svg_props.display;
	}
}

/* Set the viewport of the renderer based on the element that contains a viewport 
   TODO: change the SVGsvgElement into an element that has a viewport (more generic)
*/
static void SVGSetViewport(RenderEffect2D *eff, SVGsvgElement *svg, Bool is_root) 
{
	GF_Matrix2D mat;
	Fixed real_width, real_height;

	gf_mx2d_init(mat);

	if (is_root) {
		u32 scene_width = eff->surface->render->compositor->scene_width;
		u32 scene_height = eff->surface->render->compositor->scene_height;

		if (svg->width.type == SVG_NUMBER_VALUE) 
			real_width = INT2FIX(scene_width);
		else
			/*u32 * fixed / u32*/
			real_width = scene_width*svg->width.number/100;

		if (svg->height.type == SVG_NUMBER_VALUE)
			real_height = INT2FIX(scene_height);
		else 
			real_height = scene_height*svg->height.number/100;
	} else {
		real_width = real_height = 0;
		if (svg->width.type == SVG_NUMBER_VALUE) real_width = svg->width.number;
		if (svg->height.type == SVG_NUMBER_VALUE) real_height = svg->height.number;
	}
	
	if (!real_width || !real_height) return;


	if (svg->viewBox.width != 0 && svg->viewBox.height != 0) {
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
					gf_mx2d_apply_rect(&eff->transform, &rc);
				} else {
					rc.x = dx;
					rc.y = dy + real_height;
				}
				eff->surface->top_clipper = gf_rect_pixelize(&rc);
			}
		}
	}
	gf_mx2d_pre_multiply(&eff->transform, &mat);
}

/* Node specific rendering functions
   All the nodes follow the same principles:
	0) Check if the display property is not set to none, otherwise do not render
	1) Back-up of the renderer properties and apply the current ones
	2) Back-up of the coordinate system & apply geometric transformation if any
	3) Render the children if any or the shape if leaf node
	4) restore coordinate system
	5) restore styling properties
 */
static void SVG_Render_svg(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	GF_IRect top_clip;
	Bool is_root_svg = 0;
	SVGStylingProperties backup_props;
	u32 styling_size = sizeof(SVGStylingProperties);
	SVGsvgElement *svg = (SVGsvgElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	/* Exception for the SVG top node:
		before 1), initializes the styling properties and the geometric transformation */
	if (!eff->svg_props) {
		eff->svg_props = (SVGStylingProperties *) gf_node_get_private(node);
		is_root_svg = 1;
		/*To allow pan navigation*/
		//eff->transform.m[5] *= -1;
	}

	/* 1) */
	memcpy(&backup_props, eff->svg_props, styling_size);
	SVGApplyProperties(eff->svg_props, svg->properties);	

	/* 0) */
	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	/* 2) */
	top_clip = eff->surface->top_clipper;
	gf_mx2d_copy(backup_matrix, eff->transform);
	SVGSetViewport(eff, svg, is_root_svg);

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		svg_get_nodes_bounds(node, svg->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	/*enable or disable navigation*/
	eff->surface->render->navigation_disabled = (svg->zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	/* 3) */
	svg_render_node_list(svg->children, eff);

	/* 4) */
	gf_mx2d_copy(eff->transform, backup_matrix);  

	/* 5) */
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->surface->top_clipper = top_clip;
}

void SVG_Destroy_svg(GF_Node *node)
{
	SVGStylingProperties *svgp = (SVGStylingProperties *) gf_node_get_private(node);
	SVGResetProperties(svgp);
	free(svgp);
}

void SVG_Init_svg(Render2D *sr, GF_Node *node)
{
	SVGStylingProperties *svgp;

	GF_SAFEALLOC(svgp, sizeof(SVGStylingProperties));
	SVGInitProperties(svgp);
	gf_node_set_private(node, svgp);

	gf_node_set_render_function(node, SVG_Render_svg);
	gf_node_set_predestroy_function(node, SVG_Destroy_svg);
}

static void SVG_Render_g(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGStylingProperties backup_props;
	u32 styling_size = sizeof(SVGStylingProperties);

	SVGgElement *g = (SVGgElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	memcpy(&backup_props, eff->svg_props, styling_size);
	SVGApplyProperties(eff->svg_props, g->properties);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}	
	
	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, &g->transform);
		svg_get_nodes_bounds(node, g->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, &g->transform);
	svg_render_node_list(g->children, eff);

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}


void SVG_Init_g(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, SVG_Render_g);
}

static void SVG_Render_switch(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGStylingProperties backup_props;
	u32 styling_size = sizeof(SVGStylingProperties);

	SVGswitchElement *s = (SVGswitchElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	memcpy(&backup_props, eff->svg_props, styling_size);
	SVGApplyProperties(eff->svg_props, s->properties);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}
	
	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, &s->transform);
		svg_get_nodes_bounds(node, s->children, eff);
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, &s->transform);
	svg_render_node_list(s->children, eff);

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

void SVG_Init_switch(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, SVG_Render_switch);
}


static void SVG_DrawablePostRender(Drawable *cs, SVGStylingProperties *props, SVG_Matrix m, RenderEffect2D *eff)
{
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;
	SVGStylingProperties backup_props;

	memcpy(&backup_props, eff->svg_props, sizeof(SVGStylingProperties));
	SVGApplyProperties(eff->svg_props, *props);

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, &m);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) gf_path_get_bounds(cs->path, &eff->bounds);
		memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, &m);


	ctx = SVG_drawable_init_context(cs, eff);
	if (ctx) {
		drawctx_store_original_bounds(ctx);
		drawable_finalize_render(ctx, eff);
	}
	/* end of 3) */

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
}

static void SVG_Render_rect(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGrectElement *rect = (SVGrectElement *)node;
  
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
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &rect->properties, rect->transform, eff);
}

void SVG_Init_rect(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_rect);
}

static void SVG_Render_circle(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGcircleElement *circle = (SVGcircleElement *)node;

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed r = 2*circle->r.value;
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, circle->cx.value, circle->cy.value, r, r);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &circle->properties, circle->transform, eff);
}

void SVG_Init_circle(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_circle);
}

static void SVG_Render_ellipse(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGellipseElement *ellipse = (SVGellipseElement *)node;

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, ellipse->cx.value, ellipse->cy.value, 2*ellipse->rx.value, 2*ellipse->ry.value);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &ellipse->properties, ellipse->transform, eff);
}

void SVG_Init_ellipse(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_ellipse);
}

static void SVG_Render_line(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGlineElement *line = (SVGlineElement *)node;
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_move_to(cs->path, line->x1.value, line->y1.value);
		gf_path_add_line_to(cs->path, line->x2.value, line->y2.value);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &line->properties, line->transform, eff);
}

void SVG_Init_line(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_line);
}

static void SVG_Render_polyline(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGpolylineElement *polyline = (SVGpolylineElement *)node;

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i;
		u32 nbPoints = gf_list_count(polyline->points);
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = gf_list_get(polyline->points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = gf_list_get(polyline->points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
			cs->node_changed = 1;
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &polyline->properties, polyline->transform, eff);
}

void SVG_Init_polyline(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_polyline);
}

static void SVG_Render_polygon(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGpolygonElement *polygon = (SVGpolygonElement *)node;

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i;
		u32 nbPoints = gf_list_count(polygon->points);
		drawable_reset_path(cs);
		if (nbPoints) {
			SVG_Point *p = gf_list_get(polygon->points, 0);
			gf_path_add_move_to(cs->path, p->x, p->y);
			for (i = 1; i < nbPoints; i++) {
				p = gf_list_get(polygon->points, i);
				gf_path_add_line_to(cs->path, p->x, p->y);
			}
			gf_path_close(cs->path);
			cs->node_changed = 1;
		} else {
			gf_path_add_move_to(cs->path, 0, 0);
		}
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &polygon->properties, polygon->transform, eff);
}

void SVG_Init_polygon(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_polygon);
}

static void SVG_Render_path(GF_Node *node, void *rs)
{
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = rs;
	SVGpathElement *path = (SVGpathElement *)node;

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		u32 i, j;
		SVG_Point orig, ct_orig, ct_end, end, *tmp;
		
		//fprintf(stdout, "Rebuilding path %8x\n", path);	
		drawable_reset_path(cs);

		if (*(eff->svg_props->fill_rule)==GF_PATH_FILL_ZERO_NONZERO) cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;

		/* TODO: update for elliptical arcs */		
		for (i=0, j=0; i<gf_list_count(path->d.commands); i++) {
			u8 *command = gf_list_get(path->d.commands, i);
			switch (*command) {
			case 0: /* Move To */
				tmp = gf_list_get(path->d.points, j);
				orig = *tmp;
				gf_path_add_move_to(cs->path, orig.x, orig.y);
				j++;
				/*provision for nextCurveTo when no curve is specified:
					"If there is no previous command or if the previous command was not an C, c, S or s, 
					assume the first control point is coincident with the current point.
				*/
				ct_orig = orig;
				break;
			case 1: /* Line To */
				tmp = gf_list_get(path->d.points, j);
				end = *tmp;

				gf_path_add_line_to(cs->path, end.x, end.y);
				j++;
				orig = end;
				/*cf above*/
				ct_orig = orig;
				break;
			case 2: /* Curve To */
				tmp = gf_list_get(path->d.points, j);
				ct_orig = *tmp;
				tmp = gf_list_get(path->d.points, j+1);
				ct_end = *tmp;
				tmp = gf_list_get(path->d.points, j+2);
				end = *tmp;
				gf_path_add_cubic_to(cs->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
				ct_orig = ct_end;
				orig = end;
				j+=3;
				break;
			case 3: /* Next Curve To */
				ct_orig.x = 2*orig.x - ct_orig.x;
				ct_orig.y = 2*orig.y - ct_orig.y;
				tmp = gf_list_get(path->d.points, j);
				ct_end = *tmp;
				tmp = gf_list_get(path->d.points, j+1);
				end = *tmp;
				gf_path_add_cubic_to(cs->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
				ct_orig = ct_end;
				orig = end;
				j+=2;
				break;
			case 4: /* Quadratic Curve To */
				tmp = gf_list_get(path->d.points, j);
				ct_orig = *tmp;
				tmp = gf_list_get(path->d.points, j+1);
				end = *tmp;
				gf_path_add_quadratic_to(cs->path, ct_orig.x, ct_orig.y, end.x, end.y);			
				orig = end;
				j+=2;
				break;
			case 5: /* Next Quadratic Curve To */
				ct_orig.x = 2*orig.x - ct_orig.x;
				ct_orig.y = 2*orig.y - ct_orig.y;
				tmp = gf_list_get(path->d.points, j);
				end = *tmp;
				gf_path_add_quadratic_to(cs->path, ct_orig.x, ct_orig.y, end.x, end.y);
				orig = end;
				j++;
				break;
			case 6: /* Close */
				gf_path_close(cs->path);
				break;
			}
		}
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	SVG_DrawablePostRender(cs, &path->properties, path->transform, eff);
}

void SVG_Init_path(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_render_function(node, SVG_Render_path);
}

static void SVG_Render_use(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	RenderEffect2D *eff = rs;
	SVGuseElement *use = (SVGuseElement *)node;
  	GF_Matrix2D translate;
	SVGStylingProperties backup_props;

	if (!use->xlink_href.target) return;
	memcpy(&backup_props, eff->svg_props, sizeof(SVGStylingProperties));
	SVGApplyProperties(eff->svg_props, use->properties);


	gf_mx2d_init(translate);
	translate.m[2] = use->x.value;
	translate.m[5] = use->y.value;

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		gf_mx2d_pre_multiply(&eff->transform, &use->transform);

		if ( use->xlink_href.target && (*(eff->svg_props->display) != SVG_DISPLAY_NONE)) {
			gf_node_render((GF_Node *)use->xlink_href.target, eff);
			gf_mx2d_apply_rect(&translate, &eff->bounds);
		}
		memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
		return;
	}


	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);

	gf_mx2d_pre_multiply(&eff->transform, &use->transform);
	gf_mx2d_pre_multiply(&eff->transform, &translate);

	gf_node_render((GF_Node *)use->xlink_href.target, eff);

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, sizeof(SVGStylingProperties));
}

void SVG_Init_use(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, SVG_Render_use);
}

/* end of rendering of basic shapes */


static void SVG_Render_a(GF_Node *node, void *rs)
{
	GF_Matrix2D backup_matrix;
	SVGStylingProperties backup_props;
	u32 styling_size = sizeof(SVGStylingProperties);
	SVGaElement *a = (SVGaElement *) node;
	RenderEffect2D *eff = rs;

	memcpy(&backup_props, eff->svg_props, styling_size);
	SVGApplyProperties(eff->svg_props, a->properties);

	if (eff->trav_flags & TF_RENDER_GET_BOUNDS) {
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) {
			gf_mx2d_pre_multiply(&eff->transform, &a->transform);
			svg_get_nodes_bounds(node, a->children, eff);
		}
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		return;
	}

	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_pre_multiply(&eff->transform, &a->transform);

	svg_render_node_list(a->children, eff);

	gf_mx2d_copy(eff->transform, backup_matrix);  
	memcpy(eff->svg_props, &backup_props, styling_size);
}

static Bool SVG_a_HandleEvent(SVGhandlerElement *handler, GF_DOM_Event *event)
{
	GF_Renderer *compositor;
	GF_Event evt;
	SVGaElement *a;

	assert(gf_node_get_tag(event->currentTarget)==TAG_SVG_a);
	assert(event->type==SVG_DOM_EVT_CLICK);

	a = (SVGaElement *) event->currentTarget;
	compositor = gf_node_get_private((GF_Node *)handler);

#ifndef DANAE
	if (!compositor->user->EventProc) return 0;
#endif
	evt.type = GF_EVT_NAVIGATE;
	
	if (a->xlink_href.type == SVG_IRI_IRI) {
		evt.navigate.to_url = a->xlink_href.iri;
		if (evt.navigate.to_url) {
#ifdef DANAE
			loadDanaeUrl(compositor->danae_session, a->xlink_href.iri);
#else
			compositor->user->EventProc(compositor->user->opaque, &evt);
#endif
		}
	} else {
		u32 tag = gf_node_get_tag((GF_Node *)a->xlink_href.target);
		if (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard) {
			SVGsetElement *set = (SVGsetElement *)a->xlink_href.target;
			SMIL_Time *begin;
			GF_SAFEALLOC(begin, sizeof(SMIL_Time));
			begin->type = SMIL_TIME_CLOCK;
			begin->clock = gf_node_get_scene_time((GF_Node *)set);
			/* TODO insert the sorted value */
			gf_list_add(set->begin, begin);
			SMIL_Modified_Animation((GF_Node *)a->xlink_href.target);
		}
	}
	return 0;
}

void SVG_Init_a(Render2D *sr, GF_Node *node)
{
	SVGhandlerElement *handler;
	gf_node_set_render_function(node, SVG_Render_a);

	/*listener for onClick event*/
	handler = gf_sg_dom_create_listener(node, SVG_DOM_EVT_CLICK);
	/*and overwrite handler*/
	handler->handle_event = SVG_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for activate event*/
	handler = gf_sg_dom_create_listener(node, SVG_DOM_EVT_ACTIVATE);
	/*and overwrite handler*/
	handler->handle_event = SVG_a_HandleEvent;
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
} SVG_GradientStack;

static void SVG_DestroyGradient(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(node);
	gf_sr_texture_destroy(&st->txh);
	if (st->cols) free(st->cols);
	if (st->keys) free(st->keys);
	free(st);
}

static void SVG_UpdateGradient(SVG_GradientStack *st, GF_List *children)
{
	u32 i, count;
	Fixed alpha, max_offset;

	if (!gf_node_dirty_get(st->txh.owner)) return;
	gf_node_dirty_clear(st->txh.owner, 0);

	st->txh.needs_refresh = 1;
	st->txh.transparent = 0;
	count = gf_list_count(children);
	st->nb_col = 0;
	st->cols = realloc(st->cols, sizeof(u32)*count);
	st->keys = realloc(st->keys, sizeof(Fixed)*count);

	max_offset = 0;
	for (i=0; i<count; i++) {
		Fixed key;
		SVGstopElement *gstop = (SVGstopElement *)gf_list_get(children, i);
		if (gf_node_get_tag((GF_Node *)gstop) != TAG_SVG_stop) continue;

		if (gstop->stop_opacity.type==SVG_NUMBER_VALUE) alpha = gstop->stop_opacity.value;
		else alpha = FIX_ONE;
		st->cols[st->nb_col] = GF_COL_ARGB_FIXED(alpha, gstop->stop_color.color->red, gstop->stop_color.color->green, gstop->stop_color.color->blue);
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

/* linear gradient */

static void SVG_UpdateLinearGradient(GF_TextureHandler *txh)
{
	SVGlinearGradientElement *lg = (SVGlinearGradientElement *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_LINEAR_GRADIENT);

	SVG_UpdateGradient(st, lg->children);
}

static void SVG_LG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f start, end;
	SVGlinearGradientElement *lg = (SVGlinearGradientElement *) txh->owner;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	start.x = lg->x1.value;
	if (lg->x1.type==SVG_NUMBER_PERCENTAGE) start.x /= 100;
	start.y = lg->y1.value;
	if (lg->y1.type==SVG_NUMBER_PERCENTAGE) start.y /= 100;
	end.x = lg->x2.type ? lg->x2.value : FIX_ONE;
	if (lg->x2.type==SVG_NUMBER_PERCENTAGE) end.x /= 100;
	end.y = lg->y2.value;
	if (lg->y2.type==SVG_NUMBER_PERCENTAGE) end.x /= 100;

	/*gradientTransform???*/
	gf_mx2d_init(*mat);

	if (lg->gradientUnits==SVG_GRADIENTUNITS_OBJECT) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x - 1, bounds->y  - bounds->height - 1);
	}
	txh->compositor->r2d->stencil_set_linear_gradient(txh->hwtx, start.x, start.y, end.x, end.y);
}

void SVG_Init_linearGradient(Render2D *sr, GF_Node *node)
{
	SVG_GradientStack *st = malloc(sizeof(SVG_GradientStack));
	memset(st, 0, sizeof(SVG_GradientStack));

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateLinearGradient;

	st->txh.compute_gradient_matrix = SVG_LG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, SVG_DestroyGradient);
}

/* radial gradient */

static void SVG_UpdateRadialGradient(GF_TextureHandler *txh)
{
	SVGradialGradientElement *rg = (SVGradialGradientElement *) txh->owner;
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(txh->owner);
	if (!txh->hwtx) txh->hwtx = txh->compositor->r2d->stencil_new(txh->compositor->r2d, GF_STENCIL_RADIAL_GRADIENT);
	SVG_UpdateGradient(st, rg->children);
}

static void SVG_RG_ComputeMatrix(GF_TextureHandler *txh, GF_Rect *bounds, GF_Matrix2D *mat)
{
	SFVec2f center, focal;
	Fixed radius;
	SVGradialGradientElement *rg = (SVGradialGradientElement *) txh->owner;

	/*create gradient brush if needed*/
	if (!txh->hwtx) return;

	//GradientGetMatrix((GF_Node *) rg->transform, mat);
	gf_mx2d_init(*mat);

	radius = rg->r.type ? rg->r.value : FIX_ONE/2;
	if (rg->r.type==SVG_NUMBER_PERCENTAGE) radius /= 100;
	center.x = rg->cx.type ? rg->cx.value : FIX_ONE/2;
	if (rg->cx.type==SVG_NUMBER_PERCENTAGE) center.x /= 100;
	center.y = rg->cy.type ? rg->cy.value : FIX_ONE/2;
	if (rg->cy.type==SVG_NUMBER_PERCENTAGE) center.y /= 100;

	focal = center;

	if (rg->gradientUnits==SVG_GRADIENTUNITS_OBJECT) {
		/*move to local coord system - cf SVG spec*/
		gf_mx2d_add_scale(mat, bounds->width, bounds->height);
		gf_mx2d_add_translation(mat, bounds->x, bounds->y  - bounds->height);
	}
	txh->compositor->r2d->stencil_set_radial_gradient(txh->hwtx, center.x, center.y, focal.x, focal.y, radius, radius);
}

void SVG_Init_radialGradient(Render2D *sr, GF_Node *node)
{
	SVG_GradientStack *st = malloc(sizeof(SVG_GradientStack));
	memset(st, 0, sizeof(SVG_GradientStack));

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateRadialGradient;
	st->txh.compute_gradient_matrix = SVG_RG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, SVG_DestroyGradient);
}

GF_TextureHandler *svg_gradient_get_texture(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack*) gf_node_get_private(node);
	return st->nb_col ? &st->txh : NULL;
}

#endif //GPAC_DISABLE_SVG
