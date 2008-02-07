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
#include "offscreen_cache.h"

#include <gpac/internal/terminal_dev.h>

#include <gpac/nodes_svg_da.h>
#include <gpac/compositor.h>


typedef struct 
{
	Bool root_svg;
	SVGPropertiesPointers *svg_props;
	GF_Matrix2D viewbox_mx;
	Drawable *vp_fill;
	u32 prev_color;
	Fixed prev_vp_w, prev_vp_h;
} SVGsvgStack;

static void svg_check_focus_upon_destroy(GF_Node *n)
{
	GF_Compositor *compositor = gf_sc_get_compositor(n);
	if (compositor && (compositor->focus_node==n)) {
		compositor->focus_node = NULL;
		compositor->focus_text_type = 0;
	}
}

static void svg_recompute_viewport_transformation(GF_Node *node, SVGsvgStack *stack, GF_TraverseState *tr_state, SVGAllAttributes *atts) 
{
	GF_Matrix2D mx;
	SVG_ViewBox ext_vb;
	SVG_ViewBox *vb;
	SVG_PreserveAspectRatio par;
	Fixed scale, vp_w, vp_h;
	Fixed parent_width, parent_height, doc_width, doc_height;

	/*canvas size negociation has already been done when attaching the scene to the compositor*/
	if (atts->width && (atts->width->type==SVG_NUMBER_PERCENTAGE) ) { 
		parent_width = gf_mulfix(tr_state->vp_size.x, atts->width->value/100);
		doc_width = 0;
	} else if (!stack->root_svg) {
		doc_width = parent_width = atts->width ? atts->width->value : 0;
	} else {
		parent_width = tr_state->vp_size.x;
		doc_width = atts->width ? atts->width->value : 0;
	}

	if (atts->height && (atts->height->type==SVG_NUMBER_PERCENTAGE) ) { 
		parent_height = gf_mulfix(tr_state->vp_size.y, atts->height->value/100);
		doc_height = 0;
	} else if (!stack->root_svg) {
		doc_height = parent_height = atts->height ? atts->height->value : 0;
	} else {
		parent_height = tr_state->vp_size.y;
		doc_height = atts->height ? atts->height->value : 0;
	}

	stack->prev_vp_w = tr_state->vp_size.x;
	stack->prev_vp_h = tr_state->vp_size.y;

	vb = atts->viewBox;

	gf_mx2d_init(mx);

	if (stack->root_svg) {
		const char *frag_uri = gf_inline_get_fragment_uri(node);
		if (frag_uri) {
			/*SVGView*/
			if (!strncmp(frag_uri, "svgView", 7)) {
				if (!strncmp(frag_uri, "svgView(viewBox(", 16)) {
					Float x, y, w, h;
					sscanf(frag_uri, "svgView(viewBox(%f,%f,%f,%f))", &x, &y, &w, &h);
					ext_vb.x = FLT2FIX(x);
					ext_vb.y = FLT2FIX(y);
					ext_vb.width = FLT2FIX(w);
					ext_vb.height = FLT2FIX(h);
					ext_vb.is_set = 1;
					vb = &ext_vb;
				}
				else if (!strncmp(frag_uri, "svgView(transform(", 18)) {
					gf_svg_parse_transformlist(&mx, (char *) frag_uri+18);
				}
			}
			/*fragID*/
			else {
				GF_Node *target = gf_sg_find_node_by_name(gf_node_get_graph(node), (char *) frag_uri);
				if (target) {
					GF_Matrix2D mx;
					GF_TraverseState bounds_state;
					memset(&bounds_state, 0, sizeof(bounds_state));
					bounds_state.traversing_mode = TRAVERSE_GET_BOUNDS;
					bounds_state.visual = tr_state->visual;
					bounds_state.for_node = target;
					bounds_state.svg_props = tr_state->svg_props;
					gf_mx2d_init(bounds_state.transform);
					gf_sc_svg_get_nodes_bounds(node, ((GF_ParentNode *)node)->children, &bounds_state);
					gf_mx2d_from_mx(&mx, &tr_state->visual->compositor->hit_world_to_local);
					gf_mx2d_apply_rect(&mx, &bounds_state.bounds);
					ext_vb.x = bounds_state.bounds.x;
					ext_vb.y = bounds_state.bounds.y-bounds_state.bounds.height;
					ext_vb.width = bounds_state.bounds.width;
					ext_vb.height = bounds_state.bounds.height;
					ext_vb.is_set = 1;
					vb = &ext_vb;
				}
			}
		}
	}
	gf_mx2d_init(stack->viewbox_mx);

	if (!vb) {
		if (!doc_width || !doc_height) {
			gf_mx2d_copy(stack->viewbox_mx, mx);
			return;
		}
		/*width/height were specified in the doc, use them to compute a dummy viewbox*/
		ext_vb.x = 0;
		ext_vb.y = 0;
		ext_vb.width = doc_width;
		ext_vb.height = doc_height;
		ext_vb.is_set = 1;
		vb = &ext_vb;
	}
	if ((vb->width<=0) || (vb->height<=0) ) {
		gf_mx2d_copy(stack->viewbox_mx, mx);
		return;
	}


	/*setup default*/
	par.defer = 0;
	par.meetOrSlice = SVG_MEETORSLICE_MEET;
	par.align = SVG_PRESERVEASPECTRATIO_XMIDYMID;

	/*use parent (animation, image) viewport settings*/
	if (tr_state->parent_anim_atts) {
		if (tr_state->parent_anim_atts->preserveAspectRatio) {
			if (tr_state->parent_anim_atts->preserveAspectRatio->defer) {
				if (atts->preserveAspectRatio) 
					par = *atts->preserveAspectRatio;
			} else {
				par = *tr_state->parent_anim_atts->preserveAspectRatio;
			}
		}
	}
	/*use current viewport settings*/
	else if (atts->preserveAspectRatio) {
		par = *atts->preserveAspectRatio;
	}

	if (par.meetOrSlice==SVG_MEETORSLICE_MEET) {
		if (gf_divfix(parent_width, vb->width) > gf_divfix(parent_height, vb->height)) {
			scale = gf_divfix(parent_height, vb->height);
			vp_w = gf_mulfix(vb->width, scale);
			vp_h = parent_height;
		} else {
			scale = gf_divfix(parent_width, vb->width);
			vp_w = parent_width;
			vp_h = gf_mulfix(vb->height, scale);
		}
	} else {
		if (gf_divfix(parent_width, vb->width) < gf_divfix(parent_height, vb->height)) {
			scale = gf_divfix(parent_height, vb->height);
			vp_w = gf_mulfix(vb->width, scale);
			vp_h = parent_height;
		} else {
			scale = gf_divfix(parent_width, vb->width);
			vp_w = parent_width;
			vp_h = gf_mulfix(vb->height, scale);
		}
	}

	if (par.align==SVG_PRESERVEASPECTRATIO_NONE) {
		stack->viewbox_mx.m[0] = gf_divfix(parent_width, vb->width);
		stack->viewbox_mx.m[4] = gf_divfix(parent_height, vb->height);
		stack->viewbox_mx.m[2] = - gf_muldiv(vb->x, parent_width, vb->width); 
		stack->viewbox_mx.m[5] = - gf_muldiv(vb->y, parent_height, vb->height); 
	} else {
		Fixed dx, dy;
		stack->viewbox_mx.m[0] = stack->viewbox_mx.m[4] = scale;
		stack->viewbox_mx.m[2] = - gf_mulfix(vb->x, scale); 
		stack->viewbox_mx.m[5] = - gf_mulfix(vb->y, scale); 

		dx = dy = 0;
		switch (par.align) {
		case SVG_PRESERVEASPECTRATIO_XMINYMIN:
			break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
			dx = ( parent_width - vp_w) / 2; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
			dx = parent_width - vp_w; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMINYMID:
			dy = ( parent_height - vp_h) / 2; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMID:
			dx = ( parent_width  - vp_w) / 2; 
			dy = ( parent_height - vp_h) / 2; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMAXYMID:
			dx = parent_width  - vp_w; 
			dy = ( parent_height - vp_h) / 2; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMINYMAX:
			dy = parent_height - vp_h; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
			dx = (parent_width - vp_w) / 2; 
			dy = parent_height - vp_h; 
			break;
		case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
			dx = parent_width  - vp_w; 
			dy = parent_height - vp_h; 
			break;
		}
		gf_mx2d_add_translation(&stack->viewbox_mx, dx, dy);

		/*we need a clipper*/
		if (stack->root_svg && !tr_state->parent_anim_atts && (par.meetOrSlice==SVG_MEETORSLICE_SLICE)) {
			GF_Rect rc;
			rc.width = parent_width;
			rc.height = parent_height;
			if (!stack->root_svg) {
				rc.x = 0;
				rc.y = parent_height;
				gf_mx2d_apply_rect(&stack->viewbox_mx, &rc);
			} else {
				rc.x = dx;
				rc.y = dy + parent_height;
			}
			tr_state->visual->top_clipper = gf_rect_pixelize(&rc);
		}
	}
	gf_mx2d_add_matrix(&stack->viewbox_mx, &mx);
}

static void svg_traverse_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool rootmost_svg;
	u32 viewport_color;
	SVGsvgStack *stack;
	GF_Matrix2D backup_matrix, vb_bck;
#ifndef GPAC_DISABLE_3D
	GF_Matrix bck_mx;
#endif
	Bool is_dirty;
	GF_IRect top_clip;
	SVGPropertiesPointers backup_props, *prev_props;
	u32 backup_flags;
	Bool invalidate_flag;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	SVGAllAttributes all_atts;
	stack = gf_node_get_private(node);

	if (is_destroy) {
		SVGPropertiesPointers *svgp = gf_node_get_private(node);
		if (stack->svg_props) {
			gf_svg_properties_reset_pointers(stack->svg_props);
			free(stack->svg_props);
		}
		svg_check_focus_upon_destroy(node);
		if (stack->vp_fill) drawable_del(stack->vp_fill);
		free(stack);
		return;
	}
	
	prev_props = tr_state->svg_props;
	/*SVG props not set: we are either the root-most <svg> of the compositor
	or an <svg> inside an <animation>*/
	if (!tr_state->svg_props) tr_state->svg_props = stack->svg_props;
	
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	/*enable or disable navigation*/
	tr_state->visual->compositor->navigation_disabled = (all_atts.zoomAndPan && *all_atts.zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	if (compositor_svg_is_display_off(tr_state->svg_props)) {
		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}

	top_clip = tr_state->visual->top_clipper;
	gf_mx2d_copy(backup_matrix, tr_state->transform);
	gf_mx2d_copy(vb_bck, tr_state->vb_transform);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) gf_mx_copy(bck_mx, tr_state->model_matrix);
#endif
	
	invalidate_flag = tr_state->invalidate_all;

	is_dirty = gf_node_dirty_get(node);
	gf_node_dirty_clear(node, 0);
	if ((stack->prev_vp_w != tr_state->vp_size.x) || (stack->prev_vp_h != tr_state->vp_size.y))
		is_dirty = 1;

	if (is_dirty || tr_state->visual->compositor->recompute_ar) 
		svg_recompute_viewport_transformation(node, stack, tr_state, &all_atts);

	gf_mx2d_copy(tr_state->vb_transform, stack->viewbox_mx);

	rootmost_svg = (stack->root_svg && !tr_state->parent_anim_atts) ? 1 : 0;	
	if (tr_state->traversing_mode == TRAVERSE_SORT) {
		SVG_Paint *vp_fill = NULL;
		Fixed vp_opacity;
		GF_IRect *clip = NULL;

		if (tr_state->parent_anim_atts) {
			vp_fill = tr_state->parent_anim_atts->viewport_fill;
			vp_opacity = tr_state->parent_anim_atts->viewport_fill_opacity ? tr_state->parent_anim_atts->viewport_fill_opacity->value : FIX_ONE;
		} else {
			vp_fill = tr_state->svg_props->viewport_fill;
			vp_opacity = tr_state->svg_props->viewport_fill_opacity ? tr_state->svg_props->viewport_fill_opacity->value : FIX_ONE;
		} 

		if (vp_fill && (vp_fill->type != SVG_PAINT_NONE) && vp_opacity) {
			Bool col_dirty = 0;
			viewport_color = GF_COL_ARGB_FIXED(vp_opacity, vp_fill->color.red, vp_fill->color.green, vp_fill->color.blue);

			if (stack->prev_color != viewport_color) {
				stack->prev_color = viewport_color;
				col_dirty = 1;
			}

			if (!rootmost_svg) {
				DrawableContext *ctx;
				Fixed width = tr_state->parent_anim_atts->width->value;
				Fixed height = tr_state->parent_anim_atts->height->value;

				if (!stack->vp_fill) {
					stack->vp_fill = drawable_new();
					stack->vp_fill->node = node;
				}
				if ((width != stack->vp_fill->path->bbox.width) || (height != stack->vp_fill->path->bbox.height)) {
					drawable_reset_path(stack->vp_fill);
					gf_path_add_rect(stack->vp_fill->path, 0, 0, width, -height);
				}

				ctx = drawable_init_context_svg(stack->vp_fill, tr_state);
				if (ctx) {
					ctx->flags &= ~CTX_IS_TRANSPARENT;
					ctx->aspect.pen_props.width = 0;
					ctx->aspect.fill_color = viewport_color;
					ctx->aspect.fill_texture = NULL;
					if (col_dirty) ctx->flags |= CTX_APP_DIRTY;
					drawable_finalize_sort(ctx, tr_state, NULL);
				}

			} else if (col_dirty) {
				tr_state->visual->compositor->back_color = viewport_color;
				/*invalidate the entire visual*/
				tr_state->invalidate_all = 1;
			}
		}
	}


	if (!stack->root_svg && (all_atts.x || all_atts.y)) 
		gf_mx2d_add_translation(&tr_state->vb_transform, all_atts.x->value, all_atts.y->value);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_SORT) {
			GF_Matrix tmp;
			visual_3d_matrix_push(tr_state->visual);

			gf_mx_from_mx2d(&tmp, &tr_state->vb_transform);
			visual_3d_matrix_add(tr_state->visual, tmp.m);
		} else {
			gf_mx_add_matrix_2d(&tr_state->model_matrix, &tr_state->vb_transform);
		}
	} else 
#endif
	{
		gf_mx2d_pre_multiply(&tr_state->transform, &tr_state->vb_transform);
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_SORT) visual_3d_matrix_pop(tr_state->visual);
		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}
#endif
	gf_mx2d_copy(tr_state->transform, backup_matrix);  
	gf_mx2d_copy(tr_state->vb_transform, vb_bck);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
	tr_state->visual->top_clipper = top_clip;
	if (!stack->root_svg) {
		tr_state->invalidate_all = invalidate_flag;
	}
	tr_state->svg_props = prev_props;
}

void compositor_init_svg_svg(GF_Compositor *compositor, GF_Node *node)
{
	GF_Node *root;
	SVGsvgStack *stack;

	GF_SAFEALLOC(stack, SVGsvgStack);

	root = gf_sg_get_root_node(gf_node_get_graph(node));
	stack->root_svg = (root==node) ? 1 : 0;
	if (stack->root_svg) {
		GF_SAFEALLOC(stack->svg_props, SVGPropertiesPointers);
		gf_svg_properties_init_pointers(stack->svg_props);
	}
	gf_mx2d_init(stack->viewbox_mx);

	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_svg);
}


static void svg_traverse_g(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);

	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	SVGAllAttributes all_atts;

	if (is_destroy) {
		GroupCache *gc = gf_node_get_private(node);
		if (gc) group_cache_del(gc);

		svg_check_focus_upon_destroy(node);
		return;
	}
	/*group cache traverse routine*/
	else if (tr_state->traversing_mode == TRAVERSE_DRAW_2D) {
		group_cache_draw((GroupCache *) gf_node_get_private(node), tr_state);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props)) {
/*		u32 prev_flags = tr_state->switched_off;
		tr_state->switched_off = 1;
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		tr_state->switched_off = prev_flags;*/

		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}	
	
	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else if (tr_state->traversing_mode == TRAVERSE_SORT) {
		if (all_atts.opacity && (all_atts.opacity->value < FIX_ONE) ) {
			Bool force_recompute = 0;
			GroupCache *gc = gf_node_get_private(node);
			if (!gc) {
				force_recompute = 1;
				gc = group_cache_new(tr_state->visual->compositor, node);
				gf_node_set_private(node, gc);
			}
			gc->opacity = all_atts.opacity->value;
			group_cache_traverse(node, gc, tr_state, force_recompute);
			gf_node_dirty_clear(node, 0);
		} else {
			compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		}
		drawable_check_focus_highlight(node, tr_state, NULL);
	} else {
			compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}


void compositor_init_svg_g(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_traverse_g);
}


static void svg_traverse_defs(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 prev_flags, backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);

	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	SVGAllAttributes all_atts;

	if (is_destroy) {
		svg_check_focus_upon_destroy(node);
		return;
	}
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	prev_flags = tr_state->switched_off;
	tr_state->switched_off = 1;
	compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	tr_state->switched_off = prev_flags;

	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}


void compositor_init_svg_defs(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_traverse_defs);
}

static void svg_traverse_switch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGAllAttributes all_atts;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		svg_check_focus_upon_destroy(node);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props)) {
//		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		GF_ChildNodeItem *l = ((SVG_Element *)node)->children;
		while (l) {
			if (1 /*eval_conditional(tr_state->visual->compositor, (SVG_Element*)l->node)*/) {
				gf_node_traverse((GF_Node*)l->node, tr_state);
				break;
			}
			l = l->next;
		}
	}


	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_switch(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_traverse_switch);
}

static void svg_traverse_a(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGPropertiesPointers backup_props;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	u32 backup_flags;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		svg_check_focus_upon_destroy(node);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (compositor_svg_is_display_off(tr_state->svg_props)) {
		/*u32 prev_flags = tr_state->switched_off;
		tr_state->switched_off = 1;
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		tr_state->switched_off = prev_flags;*/

		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}	
	
	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

static Bool is_timing_target(GF_Node *n)
{
	if (!n) return 0;
	switch (gf_node_get_tag(n)) {
	case TAG_SVG_set:
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateTransform:
	case TAG_SVG_animateMotion:
	case TAG_SVG_discard:
	case TAG_SVG_animation:
	case TAG_SVG_video:
	case TAG_SVG_audio:
		return 1;
	}
	return 0;
}

static void svg_a_handle_event(GF_Node *handler, GF_DOM_Event *event)
{
	GF_Compositor *compositor;
	GF_Event evt;
	SVG_Element *a;
	SVGAllAttributes all_atts;

	assert(gf_node_get_tag(event->currentTarget)==TAG_SVG_a);

	a = (SVG_Element *) event->currentTarget;
	gf_svg_flatten_attributes(a, &all_atts);

	compositor = (GF_Compositor *)gf_node_get_private((GF_Node *)handler);

	if (!compositor->user->EventProc) return;
	if (!all_atts.xlink_href) return;

	if (event->type==GF_EVENT_MOUSEOVER) {
		evt.type = GF_EVENT_NAVIGATE_INFO;
		
		if (all_atts.xlink_title) evt.navigate.to_url = *all_atts.xlink_title;
		else if (all_atts.xlink_href->string) evt.navigate.to_url = all_atts.xlink_href->string;
		else {
			evt.navigate.to_url = gf_node_get_name(all_atts.xlink_href->target);
			if (!evt.navigate.to_url) evt.navigate.to_url = "document internal link";
		}

		compositor->user->EventProc(compositor->user->opaque, &evt);
		return;
	}

	evt.type = GF_EVENT_NAVIGATE;
	
	if (all_atts.xlink_href->type == XMLRI_STRING) {
		evt.navigate.to_url = gf_term_resolve_xlink(handler, all_atts.xlink_href->string);
		if (evt.navigate.to_url) {
			if (all_atts.target) {
				evt.navigate.parameters = (const char **) &all_atts.target;
				evt.navigate.param_count = 1;
			} else {
				evt.navigate.parameters = NULL;
				evt.navigate.param_count = 0;
			}

			if (evt.navigate.to_url[0] == '#') {
				all_atts.xlink_href->target = gf_sg_find_node_by_name(gf_node_get_graph(handler), (char *) evt.navigate.to_url+1);
				if (is_timing_target(all_atts.xlink_href->target)) {
					all_atts.xlink_href->type = XMLRI_ELEMENTID;
					free((char *)evt.navigate.to_url);
					goto process_a_timing;
				}

				gf_inline_set_fragment_uri(handler, evt.navigate.to_url+1);
				/*force recompute viewbox of root SVG - FIXME in full this should be the parent svg*/
				gf_node_dirty_set(gf_sg_get_root_node(gf_node_get_graph(handler)), 0, 0);

				compositor->trans_x = compositor->trans_y = 0;
				compositor->rotation = 0;
				compositor->zoom = FIX_ONE;
				compositor_2d_set_user_transform(compositor, FIX_ONE, 0, 0, 0); 				
				gf_sc_invalidate(compositor, NULL);
			} else if (compositor->term) {
				gf_inline_process_anchor(handler, &evt);
			} else {
				compositor->user->EventProc(compositor->user->opaque, &evt);
			}
			free((char *)evt.navigate.to_url);
		}
		return;
	} 
	
	if (!all_atts.xlink_href->target) {
		/* TODO: check if href can be resolved */
		return;
	} 
	if (!is_timing_target(all_atts.xlink_href->target)) return;

process_a_timing:
	gf_smil_timing_insert_clock(all_atts.xlink_href->target, 0, gf_node_get_scene_time((GF_Node *)handler) );
	return;
}

void compositor_init_svg_a(GF_Compositor *compositor, GF_Node *node)
{
	SVG_handlerElement *handler;
	gf_node_set_callback_function(node, svg_traverse_a);

	/*listener for onClick event*/
	handler = gf_dom_listener_build(node, GF_EVENT_CLICK, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, compositor);

	/*listener for activate event*/
	handler = gf_dom_listener_build(node, GF_EVENT_ACTIVATE, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, compositor);

	/*listener for mousemove event*/
	handler = gf_dom_listener_build(node, GF_EVENT_MOUSEOVER, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, compositor);

}

typedef struct
{
	GF_MediaObject *resource;
	GF_Node *used_node;
} SVGlinkStack;


static void svg_traverse_use(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
  	GF_Matrix2D translate;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;
	SVGlinkStack *stack = gf_node_get_private(node);

	if (is_destroy) {
		if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
		free(stack);
		return;
	}


	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY) {
		GF_MediaObject *new_res = gf_mo_load_xlink_resource(node, 0, 0, -1);
		if (new_res != stack->resource) {
			if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
			stack->resource = new_res;
		}
		stack->used_node = NULL;
		gf_node_dirty_clear(node, GF_SG_SVG_XLINK_HREF_DIRTY);
	}

	/*locate the used node - this is done at each step to handle progressive loading*/
	if (!stack->used_node && all_atts.xlink_href) {
		if (all_atts.xlink_href->type == XMLRI_ELEMENTID) {
			stack->used_node = all_atts.xlink_href->target;
		} else if (stack->resource ) {
			GF_SceneGraph *sg = gf_mo_get_scenegraph(stack->resource);
			char *fragment = strchr(all_atts.xlink_href->string, '#');
			if (fragment) {
				stack->used_node = gf_sg_find_node_by_name(sg, fragment+1);
			} else {
				stack->used_node = gf_sg_get_root_node(sg);
			}
		}
		if (!stack->used_node) goto end;
	}

	gf_list_add(tr_state->use_stack, stack->used_node);
	gf_list_add(tr_state->use_stack, node);

	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
		if (!compositor_svg_is_display_off(tr_state->svg_props)) {
			if (all_atts.xlink_href) gf_node_traverse(all_atts.xlink_href->target, tr_state);
			gf_mx2d_apply_rect(&translate, &tr_state->bounds);
		} 
		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	}
	/*SORT mode and visible, traverse*/
	else if (!compositor_svg_is_display_off(tr_state->svg_props) 
		&& (*(tr_state->svg_props->visibility) != SVG_VISIBILITY_HIDDEN)) {

		compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

#ifndef GPAC_DISABLE_3D
		if (tr_state->visual->type_3d) {
			gf_mx_add_matrix_2d(&tr_state->model_matrix, &translate);

			if (tr_state->traversing_mode==TRAVERSE_SORT) {
				GF_Matrix tmp;
				gf_mx_from_mx2d(&tmp, &translate);
				visual_3d_matrix_add(tr_state->visual, tmp.m);
			}
		} else 
#endif
			gf_mx2d_pre_multiply(&tr_state->transform, &translate);


		gf_node_traverse(stack->used_node, tr_state);
	
		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);  

	}
	gf_list_rem_last(tr_state->use_stack);
	gf_list_rem_last(tr_state->use_stack);


end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_use(GF_Compositor *compositor, GF_Node *node)
{
	SVGlinkStack *stack;
	GF_SAFEALLOC(stack, SVGlinkStack);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_use);
	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
}



/***********************************
 *  'animation' specific functions *
 ***********************************/

static void svg_animation_smil_update(GF_Node *node, SVGlinkStack *stack, Fixed normalized_scene_time)
{
	if (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY ) {
		SVGAllAttributes all_atts;
		Double clipBegin, clipEnd;
		GF_MediaObject *new_res;
		gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
		clipBegin = all_atts.clipBegin ? *all_atts.clipBegin : 0;
		clipEnd = all_atts.clipEnd ? *all_atts.clipEnd : -1;

		new_res = gf_mo_load_xlink_resource(node, 1, clipBegin, clipEnd);
		if (new_res != stack->resource) {
			if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
			stack->resource = new_res;
			stack->used_node = NULL;
		}
		gf_node_dirty_clear(node, 0);
	}
}

static void svg_animation_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	GF_Node *node = gf_smil_get_element(rti);
	SVGlinkStack *stack = gf_node_get_private(node);
	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		svg_animation_smil_update(node, stack, normalized_scene_time);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		if (stack->resource) gf_mo_stop(stack->resource);
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		if (stack->resource) gf_mo_stop(stack->resource);
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		if (stack->resource) gf_mo_restart(stack->resource);
		break;
	}
}


static void svg_traverse_animation(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGAllAttributes all_atts;
	GF_Matrix2D backup_matrix;
	GF_Matrix backup_matrix3d;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	SFVec2f prev_vp;
	GF_Rect rc;
	GF_IRect clip, prev_clip;
	SVGAllAttributes *prev_vp_atts;
	GF_TraverseState *tr_state = (GF_TraverseState*)rs;
  	GF_Matrix2D translate;
	SVGPropertiesPointers *old_props;
	SVGlinkStack *stack = gf_node_get_private(node);

	if (is_destroy) {
		if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
		free(stack);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!all_atts.width || !all_atts.height) return;
	if (!all_atts.width->value || !all_atts.height->value) return;

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);
	
	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &backup_matrix3d);

	/*add x/y translation*/
	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx_add_matrix_2d(&tr_state->model_matrix, &translate);

		if (tr_state->traversing_mode==TRAVERSE_SORT) {
			GF_Matrix tmp;
			gf_mx_from_mx2d(&tmp, &translate);
			visual_3d_matrix_add(tr_state->visual, tmp.m);
		}
	} else 
#endif
		gf_mx2d_pre_multiply(&tr_state->transform, &translate);

	/*reset SVG props to reload a new inheritance context*/
	old_props = tr_state->svg_props;
	tr_state->svg_props = NULL;
	
	/*store this node's attribute to compute PAR/ViewBox of the child <svg>*/
	prev_vp_atts = tr_state->parent_anim_atts;
	tr_state->parent_anim_atts = &all_atts;

	/*update VP size*/
	prev_vp = tr_state->vp_size;
	tr_state->vp_size.x = gf_sc_svg_convert_length_to_display(tr_state->visual->compositor, all_atts.width);
	tr_state->vp_size.y = gf_sc_svg_convert_length_to_display(tr_state->visual->compositor, all_atts.height);

	/*setup new clipper*/
	rc.width = tr_state->vp_size.x;
	rc.height = tr_state->vp_size.y;
	rc.x = 0;
	rc.y = tr_state->vp_size.y;
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	prev_clip = tr_state->visual->top_clipper;
	clip = gf_rect_pixelize(&rc);
//	gf_irect_intersect(&tr_state->visual->top_clipper, &clip);

	if (!stack->used_node && stack->resource) 
		stack->used_node = gf_sg_get_root_node(gf_mo_get_scenegraph(stack->resource));

	if (stack->used_node) gf_node_traverse(stack->used_node, rs);

	tr_state->svg_props = old_props;
	tr_state->visual->top_clipper = prev_clip;

	tr_state->parent_anim_atts = prev_vp_atts;
	tr_state->vp_size = prev_vp;

	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &backup_matrix3d);  

end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_animation(GF_Compositor *compositor, GF_Node *node)
{
	SVGlinkStack *stack;

	GF_SAFEALLOC(stack, SVGlinkStack);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_animation);

	gf_smil_set_evaluation_callback(node, svg_animation_smil_evaluate);

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
}

#endif


