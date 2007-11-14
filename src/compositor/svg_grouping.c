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

static void svg_check_focus_upon_destroy(GF_Node *n)
{
	GF_Compositor *compositor = gf_sc_get_compositor(n);
	if (compositor && (compositor->focus_node==n)) compositor->focus_node = NULL;
}

static Bool svg_set_viewport_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, Bool is_root) 
{
	SVG_PreserveAspectRatio par;
	Fixed scale, vp_w, vp_h;
	Fixed parent_width, parent_height;
	GF_Matrix2D mat;

	gf_mx2d_init(mat);

	/*canvas size negociation has already been done when attaching the scene to the compositor*/
	if (atts->width && (atts->width->type==SVG_NUMBER_PERCENTAGE) ) { 
		parent_width = gf_mulfix(tr_state->vp_size.x, atts->width->value/100);
	} else {
		parent_width = tr_state->vp_size.x;
	}

	if (atts->height && (atts->height->type==SVG_NUMBER_PERCENTAGE) ) { 
		parent_height = gf_mulfix(tr_state->vp_size.y, atts->height->value/100);
	} else {
		parent_height = tr_state->vp_size.y;
	}

	if (!atts->viewBox) return 1;
	if ((atts->viewBox->width<=0) || (atts->viewBox->height<=0) ) return 0;

	/*setup default*/
	par.defer = 0;
	par.meetOrSlice = SVG_MEETORSLICE_MEET;
	par.align = SVG_PRESERVEASPECTRATIO_XMIDYMID;

	/*use parent (animation, image) viewport settings*/
	if (tr_state->parent_vp_atts) {
		if (tr_state->parent_vp_atts->preserveAspectRatio) {
			if (tr_state->parent_vp_atts->preserveAspectRatio->defer) {
				if (atts->preserveAspectRatio) 
					par = *atts->preserveAspectRatio;
			} else {
				par = *tr_state->parent_vp_atts->preserveAspectRatio;
			}
		}
	}
	/*use current viewport settings*/
	else if (atts->preserveAspectRatio) {
		par = *atts->preserveAspectRatio;
	}

	if (par.meetOrSlice==SVG_MEETORSLICE_MEET) {
		if (gf_divfix(parent_width, atts->viewBox->width) > gf_divfix(parent_height, atts->viewBox->height)) {
			scale = gf_divfix(parent_height, atts->viewBox->height);
			vp_w = gf_mulfix(atts->viewBox->width, scale);
			vp_h = parent_height;
		} else {
			scale = gf_divfix(parent_width, atts->viewBox->width);
			vp_w = parent_width;
			vp_h = gf_mulfix(atts->viewBox->height, scale);
		}
	} else {
		if (gf_divfix(parent_width, atts->viewBox->width) < gf_divfix(parent_height, atts->viewBox->height)) {
			scale = gf_divfix(parent_height, atts->viewBox->height);
			vp_w = gf_mulfix(atts->viewBox->width, scale);
			vp_h = parent_height;
		} else {
			scale = gf_divfix(parent_width, atts->viewBox->width);
			vp_w = parent_width;
			vp_h = gf_mulfix(atts->viewBox->height, scale);
		}
	}

	if (par.align==SVG_PRESERVEASPECTRATIO_NONE) {
		mat.m[0] = gf_divfix(parent_width, atts->viewBox->width);
		mat.m[4] = gf_divfix(parent_height, atts->viewBox->height);
		mat.m[2] = - gf_muldiv(atts->viewBox->x, parent_width, atts->viewBox->width); 
		mat.m[5] = - gf_muldiv(atts->viewBox->y, parent_height, atts->viewBox->height); 
	} else {
		Fixed dx, dy;
		mat.m[0] = mat.m[4] = scale;
		mat.m[2] = - gf_mulfix(atts->viewBox->x, scale); 
		mat.m[5] = - gf_mulfix(atts->viewBox->y, scale); 

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
		mat.m[2] += dx;
		mat.m[5] += dy;
		/*we need a clipper*/
		if (par.meetOrSlice==SVG_MEETORSLICE_SLICE) {
			GF_Rect rc;
			rc.width = parent_width;
			rc.height = parent_height;
			if (!is_root) {
				rc.x = 0;
				rc.y = parent_height;
				gf_mx2d_apply_rect(&tr_state->vb_transform, &rc);
			} else {
				rc.x = dx;
				rc.y = dy + parent_height;
			}
			tr_state->visual->top_clipper = gf_rect_pixelize(&rc);
		}
	}
	gf_mx2d_pre_multiply(&tr_state->vb_transform, &mat);
	return 1;
}

static void svg_traverse_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 viewport_color;
	GF_Matrix2D backup_matrix, vb_bck;
	GF_Matrix bck_mx;
	GF_IRect top_clip;
	Bool is_root_svg = 0;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
		SVGPropertiesPointers *svgp = gf_node_get_private(node);
		gf_svg_properties_reset_pointers(svgp);
		free(svgp);
		svg_check_focus_upon_destroy(node);
		return;
	}
	
	if (!tr_state->svg_props) {
		tr_state->svg_props = (SVGPropertiesPointers *) gf_node_get_private(node);
		is_root_svg = 1;
	}
	
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
	if (!is_root_svg) gf_mx2d_copy(vb_bck, tr_state->vb_transform);
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) gf_mx_copy(bck_mx, tr_state->model_matrix);
#endif
	

	gf_mx2d_init(tr_state->vb_transform);
	if (!svg_set_viewport_transformation(tr_state, &all_atts, is_root_svg))
		goto exit;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_SORT) {
			GF_Matrix tmp;
			visual_3d_matrix_push(tr_state->visual);

			gf_mx_from_mx2d(&tmp, &tr_state->vb_transform);

			if (!is_root_svg && (all_atts.x || all_atts.y)) {
				tmp.m[12] += all_atts.x->value;
				tmp.m[13] += all_atts.y->value;
			}
			visual_3d_matrix_add(tr_state->visual, tmp.m);
		} else {
			gf_mx_add_matrix_2d(&tr_state->model_matrix, &tr_state->vb_transform);

			if (!is_root_svg && (all_atts.x || all_atts.y)) 
				gf_mx_add_translation(&tr_state->model_matrix, all_atts.x->value, all_atts.y->value, 0);
		}
	} else 
#endif
	{
		gf_mx2d_pre_multiply(&tr_state->transform, &tr_state->vb_transform);

		if (!is_root_svg && (all_atts.x || all_atts.y)) 
			gf_mx2d_add_translation(&tr_state->transform, all_atts.x->value, all_atts.y->value);
	}

	/* TODO: FIX ME: this only works for single SVG element in the doc*/
	if (is_root_svg) {
		SVG_Paint *vp_fill = NULL;
		Fixed vp_opacity;
		GF_IRect *clip = NULL;
		
		if (tr_state->parent_vp_atts) {
			vp_fill = tr_state->parent_vp_atts->viewport_fill;
			vp_opacity = tr_state->parent_vp_atts->viewport_fill_opacity ? tr_state->parent_vp_atts->viewport_fill_opacity->value : FIX_ONE;
			clip = &tr_state->visual->top_clipper;
		} else {
			vp_fill = tr_state->svg_props->viewport_fill;
			vp_opacity = tr_state->svg_props->viewport_fill_opacity ? tr_state->svg_props->viewport_fill_opacity->value : FIX_ONE;
		}
		
		if (vp_fill && (vp_fill->type != SVG_PAINT_NONE)) {
			viewport_color = GF_COL_ARGB_FIXED(vp_opacity, vp_fill->color.red, vp_fill->color.green, vp_fill->color.blue);

			if (clip) {
				/*clear visual*/
				//visual_2d_clear(tr_state->visual, clip, viewport_color);
			} else if (tr_state->visual->compositor->back_color != viewport_color) {
				tr_state->visual->compositor->back_color = viewport_color;
				/*clear visual*/
				visual_2d_clear(tr_state->visual, NULL, 0);
				gf_sc_invalidate(tr_state->visual->compositor, NULL);
				goto exit;
			}
		}
	}
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}

exit:
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_SORT) visual_3d_matrix_pop(tr_state->visual);
		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}
#endif
	gf_mx2d_copy(tr_state->transform, backup_matrix);  
	if (!is_root_svg) gf_mx2d_copy(tr_state->vb_transform, vb_bck);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
	tr_state->visual->top_clipper = top_clip;
}

void compositor_init_svg_svg(GF_Compositor *compositor, GF_Node *node)
{
	SVGPropertiesPointers *svgp;
	GF_SAFEALLOC(svgp, SVGPropertiesPointers);
	gf_svg_properties_init_pointers(svgp);
	gf_node_set_private(node, svgp);

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
		u32 prev_flags = tr_state->switched_off;
		tr_state->switched_off = 1;
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		tr_state->switched_off = prev_flags;

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
		u32 prev_flags = tr_state->switched_off;
		tr_state->switched_off = 1;
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		tr_state->switched_off = prev_flags;

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
		evt.navigate.to_url = all_atts.xlink_href->string;
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
#if 0
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
#endif
			gf_smil_timing_insert_clock(all_atts.xlink_href->target, 0, gf_node_get_scene_time((GF_Node *)handler) );

		}
	}
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

/* TODO: FIX ME we actually ignore the given sub_root since it is only valid 
	     when animations have been performed,
         animations evaluation should be part of the core compositor */
void compositor_svg_traverse_use(GF_Node *node, GF_Node *sub_root, void *rs)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
  	GF_Matrix2D translate;
	GF_Node *prev_use;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

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
		goto end;
	}

	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		(*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN)) {
		goto end;
	}

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

	if (sub_root) {
		prev_use = tr_state->parent_use;
		tr_state->parent_use = node;
		gf_node_traverse(sub_root, tr_state);
		tr_state->parent_use = prev_use;
	}
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);  

end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}


void compositor_svg_traverse_animation(GF_Node *node, GF_Node *sub_root, void *rs)
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

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!all_atts.width || !all_atts.height) return;
	if (!all_atts.width->value || !all_atts.height->value) return;

	compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags);


	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);
	
	if (compositor_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &backup_matrix3d);


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

#if 0
	st = gf_node_get_private(n);
	if (!st->is) return;
	root = gf_sg_get_root_node(st->is->graph);
	if (root) {
		old_props = tr_state->svg_props;
		tr_state->svg_props = &new_props;
		gf_svg_properties_init_pointers(tr_state->svg_props);
		//gf_sc_traverse_subscene(st->is->root_od->term->compositor, root, rs);
		gf_node_traverse(root, rs);
		tr_state->svg_props = old_props;
		gf_svg_properties_reset_pointers(&new_props);
//	}
#endif

	old_props = tr_state->svg_props;
	tr_state->svg_props = NULL;
	
	prev_vp_atts = tr_state->parent_vp_atts;
	tr_state->parent_vp_atts = &all_atts;
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

	gf_node_traverse(sub_root, rs);
	tr_state->svg_props = old_props;
	tr_state->visual->top_clipper = prev_clip;

	tr_state->parent_vp_atts = prev_vp_atts;
	tr_state->vp_size = prev_vp;

	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &backup_matrix3d);  
end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

#endif


