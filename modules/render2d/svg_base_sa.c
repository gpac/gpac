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

void gf_svg_apply_local_transformation(RenderEffect2D *eff, GF_Node *node, GF_Matrix2D *backup_matrix)
{
	SVGTransformableElement *tr = (SVGTransformableElement *)node;
	gf_mx2d_copy(*backup_matrix, eff->transform);

	if (tr->transform.is_ref) 
		gf_mx2d_copy(eff->transform, eff->vb_transform);

	if (tr->motionTransform) 
		gf_mx2d_pre_multiply(&eff->transform, tr->motionTransform);

	gf_mx2d_pre_multiply(&eff->transform, &tr->transform.mat);
}

void gf_svg_restore_parent_transformation(RenderEffect2D *eff, GF_Matrix2D *backup_matrix)
{
	gf_mx2d_copy(eff->transform, *backup_matrix);  
}

/* Set the viewport of the renderer based on the element that contains a viewport 
   TODO: change the SVGsvgElement into an element that has a viewport (more generic)
*/
static void gf_svg_set_viewport_transformation(RenderEffect2D *eff, SVGsvgElement *svg, Bool is_root) 
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
	* Back-up of the renderer properties and apply the current ones
	* Back-up of the coordinate system & apply geometric transformation if any
	* Render the children if any or the shape if leaf node
	* Restore coordinate system
	* Restore styling properties
 */
static void SVG_Render_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 viewport_color;
	GF_Matrix2D backup_matrix;
	GF_IRect top_clip;
	Bool is_root_svg = 0;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGsvgElement *svg = (SVGsvgElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) {
		SVGPropertiesPointers *svgp = (SVGPropertiesPointers *) gf_node_get_private(node);
		if (svgp) {
			gf_svg_properties_reset_pointers(svgp);
			free(svgp);
		}
		return;
	}

	/*enable or disable navigation*/
	eff->surface->render->navigation_disabled = (svg->zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	if (!eff->svg_props) {
		eff->svg_props = (SVGPropertiesPointers *) gf_node_get_private(node);
		is_root_svg = 1;
	}

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}

	top_clip = eff->surface->top_clipper;
	gf_mx2d_copy(backup_matrix, eff->transform);
	gf_mx2d_init(eff->vb_transform);
	gf_svg_set_viewport_transformation(eff, svg, is_root_svg);
	gf_mx2d_pre_multiply(&eff->transform, &eff->vb_transform);

	if (!is_root_svg && (svg->x.value || svg->y.value)) 
		gf_mx2d_add_translation(&eff->transform, svg->x.value, svg->y.value);

	/* TODO: FIX ME: this only works for single SVG element in the doc*/
	if (is_root_svg && eff->svg_props->viewport_fill->type != SVG_PAINT_NONE) {
		viewport_color = GF_COL_ARGB_FIXED(eff->svg_props->viewport_fill_opacity->value, eff->svg_props->viewport_fill->color.red, eff->svg_props->viewport_fill->color.green, eff->svg_props->viewport_fill->color.blue);
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

	gf_svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
	eff->surface->top_clipper = top_clip;
}

void SVG_Init_svg(Render2D *sr, GF_Node *node)
{
	SVGPropertiesPointers *svgp;

	GF_SAFEALLOC(svgp, SVGPropertiesPointers);
	gf_svg_properties_init_pointers(svgp);
	gf_node_set_private(node, svgp);

	gf_node_set_callback_function(node, SVG_Render_svg);
}

#undef ENABLE_CULLING

#ifdef ENABLE_CULLING
static GFINLINE Bool rect_over_cliper(GF_Rect *r, GF_IRect *cliper)
{
	if (cliper->x+cliper->width<=r->x) return 0;
	if (cliper->x>=r->x+r->width) return 0;
	if (cliper->y-cliper->height>=r->y) return 0;
	if (cliper->y<=r->y-r->height) return 0;
	return 1;
}

/*@rc2 fully contained in @rc1*/
static GFINLINE Bool rect_inside_cliper(GF_IRect *cliper, GF_Rect *r) 
{
	if ( (cliper->x <= r->x) 
		&& (cliper->y >= r->y) 
		&& (cliper->x + cliper->width >= r->x + r->width) 
		&& (cliper->y - cliper->height <= r->y - r->height) )
		return 1;
	return 0;
}
#endif

static void SVG_Render_g(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGgElement *g = (SVGgElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;
#ifdef ENABLE_CULLING	
	GF_Rect *bounds	= gf_node_get_private(node);
#endif
	if (is_destroy) {
#ifdef ENABLE_CULLING
		free(bounds);
#endif
		return;
	}

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		u32 prev_flags = eff->trav_flags;
		eff->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		svg_render_node_list(g->children, eff);
		eff->trav_flags = prev_flags;

		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}	
#ifdef ENABLE_CULLING
	if (gf_node_dirty_get(node) & GF_SG_CHILD_DIRTY) {
		u32 prev_mode = eff->traversing_mode;
		eff->traversing_mode = TRAVERSE_GET_BOUNDS;

		svg_get_nodes_bounds(node, g->children, eff);
		*bounds = eff->bounds;

		gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
		eff->traversing_mode = prev_mode;
	}
#endif
	gf_svg_apply_local_transformation(eff, node, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, g->children, eff);
	} else {
#ifdef ENABLE_CULLING
		if (bounds->width && bounds->height) {
			if (eff->inside_cliper) {
				svg_render_node_list(g->children, eff);
			} else {
				GF_Rect rc = *bounds;
				gf_mx2d_apply_rect(&eff->transform, &rc);
				if (rect_inside_cliper(&eff->surface->top_clipper, &rc)) {
					eff->inside_cliper = 1;
					svg_render_node_list(g->children, eff);
				} else if (rect_over_cliper(&rc, &eff->surface->top_clipper)) {
					svg_render_node_list(g->children, eff);
				}
				eff->inside_cliper = 0;
			}
		}
#else
		svg_render_node_list(g->children, eff);
#endif
	}
	gf_svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}


void SVG_Init_g(Render2D *sr, GF_Node *node)
{
#ifdef ENABLE_CULLING
	GF_Rect *bounds;
	GF_SAFEALLOC(bounds, GF_Rect);
	gf_node_set_private(node, bounds);
#endif
	gf_node_set_callback_function(node, SVG_Render_g);
}

static Bool eval_conditional(GF_Renderer *sr, SVGElement *elt)
{
	u32 i, count;
	Bool found;
	const char *lang_3cc, *lang_2cc;
	if (!elt->conditional) return 1;

	count = gf_list_count(elt->conditional->requiredFeatures);
	for (i=0;i<count;i++) {
		SVG_IRI *iri = (SVG_IRI *)gf_list_get(elt->conditional->requiredFeatures, i);
		if (!iri->iri) continue;
		/*TODO FIXME: be a bit more precise :)*/
		if (!strnicmp(iri->iri, "http://www.w3.org/", 18)) {
			char *feat = strrchr(iri->iri, '#');
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


static void SVG_Render_switch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGswitchElement *s = (SVGswitchElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;
	if (is_destroy) return;

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE) {
		gf_svg_restore_parent_transformation(eff, &backup_matrix);
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}
	
	gf_svg_apply_local_transformation(eff, node, &backup_matrix);
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		svg_get_nodes_bounds(node, s->children, eff);
	} else {
		GF_ChildNodeItem *l = s->children;
		while (l) {
			if (eval_conditional(eff->surface->render->compositor, (SVGElement*)l->node)) {
				svg_render_node((GF_Node*)l->node, eff);
				break;
			}
			l = l->next;
		}
	}
	gf_svg_restore_parent_transformation(eff, &backup_matrix);
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}

void SVG_Init_switch(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_switch);
}


static void SVG_DrawablePostRender(Drawable *cs, SVGPropertiesPointers *backup_props, u32 *backup_flags,
								   SVGTransformableElement *elt, RenderEffect2D *eff, Bool rectangular, Fixed path_length)
{
	GF_Matrix2D backup_matrix;
	DrawableContext *ctx;

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) 
			gf_path_get_bounds(cs->path, &eff->bounds);
		goto end;
	}

	if ((*(eff->svg_props->display) == SVG_DISPLAY_NONE) ||
		(*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) ) {
		goto end;
	}

	gf_svg_apply_local_transformation(eff, (GF_Node *)elt, &backup_matrix);

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

	gf_svg_restore_parent_transformation(eff, &backup_matrix);
end:
	memcpy(eff->svg_props, backup_props, sizeof(SVGPropertiesPointers));
	eff->svg_flags = *backup_flags;
}

static void SVG_Render_rect(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	SVGrectElement *rect = (SVGrectElement *)node;

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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);
	
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
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)rect, eff, 1, 0);
}

void SVG_Init_rect(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_rect);
}

static void SVG_Render_circle(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGcircleElement *circle = (SVGcircleElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		Fixed r = 2*circle->r.value;
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, circle->cx.value, circle->cy.value, r, r);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)circle, eff, 0, 0);
}

void SVG_Init_circle(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_circle);
}

static void SVG_Render_ellipse(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGellipseElement *ellipse = (SVGellipseElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, ellipse->cx.value, ellipse->cy.value, 2*ellipse->rx.value, 2*ellipse->ry.value);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)ellipse, eff, 0, 0);
}

void SVG_Init_ellipse(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_ellipse);
}

static void SVG_Render_line(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGlineElement *line = (SVGlineElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		drawable_reset_path(cs);
		gf_path_add_move_to(cs->path, line->x1.value, line->y1.value);
		gf_path_add_line_to(cs->path, line->x2.value, line->y2.value);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)line, eff, 0, 0);
}

void SVG_Init_line(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_line);
}

static void SVG_Render_polyline(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGpolylineElement *polyline = (SVGpolylineElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

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
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)polyline, eff, 0, 0);
}

void SVG_Init_polyline(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_polyline);
}

static void SVG_Render_polygon(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGpolygonElement *polygon = (SVGpolygonElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
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

	SVG_Render_base(node, (RenderEffect2D *)rs, &backup_props, &backup_flags);

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
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)polygon, (RenderEffect2D *)rs, 0, 0);
}

void SVG_Init_polygon(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, SVG_Render_polygon);
}

static void SVG_Destroy_path(GF_Node *node)
{
	Drawable *dr = gf_node_get_private(node);
#if USE_GF_PATH
	/* The path is the same as the one in the SVG node, don't delete it here */
	dr->path = NULL;
#endif
	drawable_del(dr);
}

static void SVG_Render_path(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGpathElement *path = (SVGpathElement *)node;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	if (is_destroy) {
		SVG_Destroy_path(node);
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

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		
#if USE_GF_PATH
		cs->path = &path->d;
#else
		drawable_reset_path(cs);
		gf_svg_path_build(cs->path, path->d.commands, path->d.points);
#endif
		if (*(eff->svg_props->fill_rule)==GF_PATH_FILL_ZERO_NONZERO) cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	SVG_DrawablePostRender(cs, &backup_props, &backup_flags, (SVGTransformableElement *)path, eff, 0, (path->pathLength.type==SVG_NUMBER_VALUE) ? path->pathLength.value : 0);
}


void SVG_Init_path(Render2D *sr, GF_Node *node)
{
	Drawable *st = drawable_stack_new(sr, node);
#if USE_GF_PATH
	gf_path_del(st->path);
	st->path = NULL;
#endif
	gf_node_set_callback_function(node, SVG_Render_path);
}

/* end of rendering of basic shapes */


static void SVG_Render_a(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGaElement *a = (SVGaElement *) node;
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	if (is_destroy) return;

	SVG_Render_base(node, eff, &backup_props, &backup_flags);

	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_svg_apply_local_transformation(eff, node, &backup_matrix);
		if (*(eff->svg_props->display) != SVG_DISPLAY_NONE) 
			svg_get_nodes_bounds(node, a->children, eff);
		gf_svg_restore_parent_transformation(eff, &backup_matrix);  
		memcpy(eff->svg_props, &backup_props, styling_size);
		eff->svg_flags = backup_flags;
		return;
	}

	if (*(eff->svg_props->display) == SVG_DISPLAY_NONE ||
		*(eff->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
	} else {
		gf_svg_apply_local_transformation(eff, node, &backup_matrix);
		svg_render_node_list(a->children, eff);
		gf_svg_restore_parent_transformation(eff, &backup_matrix);  
	}
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
}

static void SVG_a_HandleEvent(GF_Node *handler, GF_DOM_Event *event)
{
	GF_Renderer *compositor;
	GF_Event evt;
	SVGaElement *a;

	assert(gf_node_get_tag(event->currentTarget)==TAG_SVG_a);

	a = (SVGaElement *) event->currentTarget;
	compositor = (GF_Renderer *)gf_node_get_private((GF_Node *)handler);

	if (!compositor->user->EventProc) return;

	if (event->type==GF_EVENT_MOUSEOVER) {
		evt.type = GF_EVENT_NAVIGATE_INFO;
		evt.navigate.to_url = a->xlink->href.iri;
		if (a->xlink->title) evt.navigate.to_url = a->xlink->title;
		compositor->user->EventProc(compositor->user->opaque, &evt);
		return;
	}

	evt.type = GF_EVENT_NAVIGATE;
	
	if (a->xlink->href.type == SVG_IRI_IRI) {
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
		if (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard) {
			u32 i, count, found;
			SVGsetElement *set = (SVGsetElement *)a->xlink->href.target;
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

void SVG_Init_a(Render2D *sr, GF_Node *node)
{
	XMLEV_Event evt;
	SVGhandlerElement *handler;
	gf_node_set_callback_function(node, SVG_Render_a);

	/*listener for onClick event*/
	evt.parameter = 0;
	evt.type = GF_EVENT_CLICK;
	handler = gf_dom_listener_build(node, evt);
	/*and overwrite handler*/
	handler->handle_event = SVG_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for activate event*/
	evt.type = GF_EVENT_ACTIVATE;
	handler = gf_dom_listener_build(node, evt);
	/*and overwrite handler*/
	handler->handle_event = SVG_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

#ifndef DANAE
	/*listener for mouseover event*/
	evt.type = GF_EVENT_MOUSEOVER;
	handler = gf_dom_listener_build(node, evt);
	/*and overwrite handler*/
	handler->handle_event = SVG_a_HandleEvent;
	gf_node_set_private((GF_Node *)handler, sr->compositor);
#endif
}

/* end of Interactive SVG elements */

void SVG_DestroyPaintServer(GF_Node *node)
{
	SVG_GradientStack *st = (SVG_GradientStack *) gf_node_get_private(node);
	if (st) {
		gf_sr_texture_destroy(&st->txh);
		if (st->cols) free(st->cols);
		if (st->keys) free(st->keys);
		free(st);
	}
}

static void SVG_UpdateGradient(SVG_GradientStack *st, GF_ChildNodeItem *children)
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
		SVGstopElement *gstop = (SVGstopElement *) children->node;
		children = children->next;
		if (gf_node_get_tag((GF_Node *)gstop) != TAG_SVG_stop) continue;

		if (gstop->properties->stop_opacity.type==SVG_NUMBER_VALUE) alpha = gstop->properties->stop_opacity.value;
		else alpha = FIX_ONE;
		st->cols[st->nb_col] = GF_COL_ARGB_FIXED(alpha, gstop->properties->stop_color.color.red, gstop->properties->stop_color.color.green, gstop->properties->stop_color.color.blue);
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

static void SVG_Render_PaintServer(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGElement *elt = (SVGElement *)node;
	RenderEffect2D *eff = (RenderEffect2D *) rs;

	if (is_destroy) {
		SVG_DestroyPaintServer(node);
		return;
	}
	SVG_Render_base(node, eff, &backup_props, &backup_flags);
	
	if (eff->traversing_mode == TRAVERSE_GET_BOUNDS) {
		return;
	} else {
		svg_render_node_list(elt->children, eff);
	}
	memcpy(eff->svg_props, &backup_props, styling_size);
	eff->svg_flags = backup_flags;
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

void SVG_Init_linearGradient(Render2D *sr, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateLinearGradient;

	st->txh.compute_gradient_matrix = SVG_LG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
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

void SVG_Init_radialGradient(Render2D *sr, GF_Node *node)
{
	SVG_GradientStack *st;
	GF_SAFEALLOC(st, SVG_GradientStack);

	gf_sr_texture_setup(&st->txh, sr->compositor, node);
	st->txh.update_texture_fcnt = SVG_UpdateRadialGradient;
	st->txh.compute_gradient_matrix = SVG_RG_ComputeMatrix;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

void SVG_Init_solidColor(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

void SVG_Init_stop(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_PaintServer);
}

GF_TextureHandler *svg_gradient_get_texture(GF_Node *node)
{
	SVGElement *g = (SVGElement *)node;
	SVG_GradientStack *st;
	if (g->xlink->href.target) g = g->xlink->href.target;
	st = (SVG_GradientStack*) gf_node_get_private((GF_Node *)g);
	return st->nb_col ? &st->txh : NULL;
}

void SVG_Render_base(GF_Node *node, RenderEffect2D *eff, 
					 SVGPropertiesPointers *backup_props, u32 *backup_flags)
{
	u32 inherited_flags_mask;

	memcpy(backup_props, eff->svg_props, sizeof(SVGPropertiesPointers));
	*backup_flags = eff->svg_flags;

	/*following has been checked against animate-elem-84-t.svg and animate-elem-78-t.svg (and others)*/
	
	/*first apply animations - all inherit values used during interpolation are resolved against the current 
	property context (ie parent one) during this step*/
	gf_svg_apply_animations(node, eff->svg_props);
	
	/*then apply inheritance now that all animated attributes are set (potentially to 'inherit')
	and store groups of inherited properties for dirty rect algo*/
	inherited_flags_mask = gf_svg_apply_inheritance((SVGElement *)node, eff->svg_props);	
	eff->svg_flags &= inherited_flags_mask;

	eff->svg_flags |= gf_node_dirty_get(node);
}

#endif //GPAC_DISABLE_SVG
