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



static Bool svg_is_focus_target(GF_Node *elt)
{
	u32 i, count;
	if (gf_node_get_tag(elt)==TAG_SVG_a) 
		return 1;

	count = gf_dom_listener_count(elt);	
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_Node *l = gf_dom_listener_get(elt, i);
		if (gf_svg_get_attribute_by_tag(l, TAG_SVG_ATT_event, 0, 0, &info)==GF_OK) {
			switch ( ((XMLEV_Event*)info.far_ptr)->type) {
			case GF_EVENT_FOCUSIN:
			case GF_EVENT_FOCUSOUT:
			case GF_EVENT_ACTIVATE:
				return 1;
			}
		}
	}
	return 0;
}

static GF_Node *svg_set_focus_prev(GF_Compositor *compositor, GF_Node *elt, Bool current_focus)
{
	u32 i, count, tag;
	GF_Node *n;
	SVGAllAttributes atts;

	tag = gf_node_get_tag(elt);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) return NULL;

	gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

	if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) return NULL;

	if (!current_focus) {
		Bool is_auto = 1;
		if (atts.focusable) {
			if (*atts.focusable==SVG_FOCUSABLE_TRUE) return elt;
			if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = 0;
		}
		if (is_auto && svg_is_focus_target(elt)) return elt;

		if (atts.editable && *atts.editable) {
			switch (tag) {
			case TAG_SVG_text:
			case TAG_SVG_textArea:
				compositor->focus_text_type = 1;
				return elt;
			case TAG_SVG_tspan:
				compositor->focus_text_type = 2;
				return elt;
			default:
				break;
			}
		}
	}

	/**/
	if (atts.nav_prev) {
		switch (atts.nav_prev->type) {
		case SVG_FOCUS_SELF:
			/*focus locked on element*/
			return elt;
		case SVG_FOCUS_IRI:
			if (!atts.nav_prev->target.target) {
				if (!atts.nav_prev->target.string) return NULL;
				atts.nav_prev->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_prev->target.string+1);
			}
			return atts.nav_prev->target.target;
		default:
			break;
		}
	}

	/*check all children except if current focus*/
	if (current_focus) return NULL;
	count = gf_node_list_get_count( ((SVG_Element*)elt)->children );
	for (i=count; i>0; i--) {
		/*get in the subtree*/
		n = gf_node_list_get_child( ((SVG_Element*)elt)->children, i-1);
		n = svg_set_focus_prev(compositor, n, current_focus);
		if (n) return n;
	}
	return NULL;
}

static GF_Node *svg_browse_parent_for_focus_prev(GF_Compositor *compositor, GF_Node *elt)
{
	s32 idx = 0;
	u32 i, count;
	GF_Node *n;
	SVG_Element *par;
	/*not found, browse parent list*/
	par = (SVG_Element *)gf_node_get_parent((GF_Node*)elt, 0);
	/*root, return NULL if next, current otherwise*/
	if (!par) return NULL;

	/*locate element*/
	count = gf_node_list_get_count(par->children);
	idx = gf_node_list_find_child(par->children, elt);
	if (idx<0) idx=count;
	for (i=idx; i>0; i--) {
		n = gf_node_list_get_child(par->children, i-1);
		/*get in the subtree*/
		n = svg_set_focus_prev(compositor, n, 0);
		if (n) return n;
	}
	/*up one level*/
	return svg_browse_parent_for_focus_prev(compositor, (GF_Node*)par);
}


static GF_Node *svg_set_focus_next(GF_Compositor *compositor, GF_Node *elt, Bool current_focus)
{
	u32 tag;
	GF_ChildNodeItem *child;
	GF_Node *n;
	SVGAllAttributes atts;

	tag = gf_node_get_tag(elt);
	if (tag <= GF_NODE_FIRST_DOM_NODE_TAG) return NULL;

	gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

	if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) return NULL;

	if (!current_focus) {
		Bool is_auto = 1;
		if (atts.focusable) {
			if (*atts.focusable==SVG_FOCUSABLE_TRUE) return elt;
			if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = 0;
		}
		if (is_auto && svg_is_focus_target(elt)) return elt;

		if (atts.editable && *atts.editable) {
			switch (tag) {
			case TAG_SVG_text:
			case TAG_SVG_textArea:
				compositor->focus_text_type = 1;
				return elt;
			case TAG_SVG_tspan:
				compositor->focus_text_type = 2;
				return elt;
			default:
				break;
			}
		}
	}

	/*check next*/
	if (atts.nav_next) {
		switch (atts.nav_next->type) {
		case SVG_FOCUS_SELF:
			/*focus locked on element*/
			return elt;
		case SVG_FOCUS_IRI:
			if (!atts.nav_next->target.target) {
				if (!atts.nav_next->target.string) return NULL;
				atts.nav_next->target.target = gf_sg_find_node_by_name(compositor->scene, atts.nav_next->target.string+1);
			}
			return atts.nav_next->target.target;
		default:
			break;
		}
	}

	/*check all children */
	child = ((SVG_Element*)elt)->children;
	while (child) {
		/*get in the subtree*/
		n = svg_set_focus_next(compositor, child->node, 0);
		if (n) return n;
		child = child->next;
	}
	return NULL;
}

static GF_Node *svg_browse_parent_for_focus_next(GF_Compositor *compositor, GF_Node *elt)
{
	s32 idx = 0;
	GF_ChildNodeItem *child;
	GF_Node *n;
	SVG_Element *par;
	/*not found, browse parent list*/
	par = (SVG_Element *)gf_node_get_parent((GF_Node*)elt, 0);
	/*root, return NULL if next, current otherwise*/
	if (!par) return NULL;

	/*locate element*/
	child = par->children;
	idx = gf_node_list_find_child(child, elt);
	while (child) {
		if (idx<0) {
			/*get in the subtree*/
			n = svg_set_focus_next(compositor, child->node, 0);
			if (n) return n;
		}
		idx--;
		child = child->next;
	}
	/*up one level*/
	return svg_browse_parent_for_focus_next(compositor, (GF_Node*)par);
}

u32 gf_sc_svg_focus_switch_ring(GF_Compositor *compositor, Bool move_prev)
{
	GF_DOM_Event evt;
	Bool current_focus = 1;
	u32 ret = 0;
	GF_Node *n, *prev;

	prev = compositor->focus_node;
	compositor->focus_text_type = 0;

	if (!compositor->focus_node) {
		compositor->focus_node = gf_sg_get_root_node(compositor->scene);
		if (!compositor->focus_node) return 0;
		current_focus = 0;
	}

	/*get focus in current doc order*/
	if (move_prev) {
		n = svg_set_focus_prev(compositor, compositor->focus_node, current_focus);
		if (!n) n = svg_browse_parent_for_focus_prev(compositor, compositor->focus_node);
	} else {
		n = svg_set_focus_next(compositor, compositor->focus_node, current_focus);
		if (!n) n = svg_browse_parent_for_focus_next(compositor, compositor->focus_node);
	}
	compositor->focus_node = n;

	ret = 0;
	if (prev != compositor->focus_node) {
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		if (prev) {
			evt.bubbles = 1;
			evt.target = prev;
			evt.type = GF_EVENT_FOCUSOUT;
			gf_dom_event_fire(prev, NULL, &evt);
		}
		if (compositor->focus_node) {
			evt.bubbles = 1;
			evt.target = compositor->focus_node;
			evt.type = GF_EVENT_FOCUSIN;
			gf_dom_event_fire(compositor->focus_node, NULL, &evt);
		}
		//invalidate in case we draw focus rect
		gf_sc_invalidate(compositor, NULL);
	}
	return ret;
}

u32 gf_sc_svg_focus_navigate(GF_Compositor *compositor, u32 key_code)
{
	SVGAllAttributes atts;
	GF_DOM_Event evt;
	u32 ret = 0;
	GF_Node *n;
	SVG_Focus *focus = NULL;

	if (!compositor->focus_node) return 0;
	n=NULL;
	gf_svg_flatten_attributes((SVG_Element *)compositor->focus_node, &atts);
	switch (key_code) {
	case GF_KEY_LEFT: focus = atts.nav_left; break;
	case GF_KEY_RIGHT: focus = atts.nav_right; break;
	case GF_KEY_UP: focus = atts.nav_up; break;
	case GF_KEY_DOWN: focus = atts.nav_down; break;
	default: return 0;
	}
	if (!focus) return 0;

	if (focus->type==SVG_FOCUS_SELF) return 0;
	if (focus->type==SVG_FOCUS_AUTO) return 0;
	if (!focus->target.target) {
		if (!focus->target.string) return 0;
		focus->target.target = gf_sg_find_node_by_name(compositor->scene, focus->target.string+1);
	}
	n = focus->target.target;

	ret = 0;
	if (n != compositor->focus_node) {
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		if (compositor->focus_node) {
			evt.target = compositor->focus_node;
			evt.type = GF_EVENT_FOCUSOUT;
			gf_dom_event_fire(compositor->focus_node, NULL, &evt);
		}
		if (n) {
			evt.relatedTarget = n;
			evt.type = GF_EVENT_FOCUSIN;
			gf_dom_event_fire(n, NULL, &evt);
		}
		compositor->focus_node = n;
		//invalidate in case we draw focus rect
		gf_sc_invalidate(compositor, NULL);
	}
	return ret;
}


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

void gf_sc_svg_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	GF_Rect rc;
	GF_Matrix2D cur_mx;

	if (tr_state->abort_bounds_traverse) return;

	gf_mx2d_copy(cur_mx, tr_state->transform);
	rc = gf_rect_center(0,0);
	while (children) {
		gf_mx2d_init(tr_state->transform);
		tr_state->bounds = gf_rect_center(0,0);

		gf_node_traverse(children->node, tr_state);
		/*we hit the target node*/
		if (children->node == tr_state->for_node) {
			tr_state->abort_bounds_traverse = 1;
			gf_mx_from_mx2d(&tr_state->visual->compositor->hit_world_to_local, &tr_state->transform);
			return;
		}
		if (tr_state->abort_bounds_traverse) return;

		gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
		gf_rect_union(&rc, &tr_state->bounds);
		children = children->next;
	}
	gf_mx2d_copy(tr_state->transform, cur_mx);
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	tr_state->bounds = rc;
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
				gf_mx_add_scale(&tmp, 1, -1, 1);
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


