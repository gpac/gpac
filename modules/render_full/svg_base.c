/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D+3D Rendering sub-project
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
#include "node_stacks.h"

void svg_check_focus_upon_destroy(GF_Node *n)
{
	Render *raster;
	GF_Renderer *sr = gf_sr_get_renderer(n);
	if (sr) {
		raster = (Render *)sr->visual_renderer->user_priv; 
		if (raster->focus_node==n) raster->focus_node = NULL;
	}
}

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

static GF_Node *svg_set_focus_prev(Render *sr, GF_Node *elt, Bool current_focus)
{
	u32 i, count;
	GF_Node *n;
	SVGAllAttributes atts;

	if (gf_node_get_tag(elt) <= GF_NODE_FIRST_DOM_NODE_TAG) return NULL;

	gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

	if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) return NULL;

	if (!current_focus) {
		Bool is_auto = 1;
		if (atts.focusable) {
			if (*atts.focusable==SVG_FOCUSABLE_TRUE) return elt;
			if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = 0;
		}
		if (is_auto && svg_is_focus_target(elt)) return elt;
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
				atts.nav_prev->target.target = gf_sg_find_node_by_name(sr->compositor->scene, atts.nav_prev->target.string+1);
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
		n = svg_set_focus_prev(sr, n, current_focus);
		if (n) return n;
	}
	return NULL;
}

static GF_Node *svg_browse_parent_for_focus_prev(Render *sr, GF_Node *elt)
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
		n = svg_set_focus_prev(sr, n, 0);
		if (n) return n;
	}
	/*up one level*/
	return svg_browse_parent_for_focus_prev(sr, (GF_Node*)par);
}


static GF_Node *svg_set_focus_next(Render *sr, GF_Node *elt, Bool current_focus)
{
	GF_ChildNodeItem *child;
	GF_Node *n;
	SVGAllAttributes atts;

	if (gf_node_get_tag(elt) <= GF_NODE_FIRST_DOM_NODE_TAG) return NULL;

	gf_svg_flatten_attributes((SVG_Element *)elt, &atts);

	if (atts.display && (*atts.display==SVG_DISPLAY_NONE)) return NULL;

	if (!current_focus) {
		Bool is_auto = 1;
		if (atts.focusable) {
			if (*atts.focusable==SVG_FOCUSABLE_TRUE) return elt;
			if (*atts.focusable==SVG_FOCUSABLE_FALSE) is_auto = 0;
		}
		if (is_auto && svg_is_focus_target(elt)) return elt;
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
				atts.nav_next->target.target = gf_sg_find_node_by_name(sr->compositor->scene, atts.nav_next->target.string+1);
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
		n = svg_set_focus_next(sr, child->node, 0);
		if (n) return n;
		child = child->next;
	}
	return NULL;
}

static GF_Node *svg_browse_parent_for_focus_next(Render *sr, GF_Node *elt)
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
			n = svg_set_focus_next(sr, child->node, 0);
			if (n) return n;
		}
		idx--;
		child = child->next;
	}
	/*up one level*/
	return svg_browse_parent_for_focus_next(sr, (GF_Node*)par);
}

u32 render_svg_focus_switch_ring(Render *sr, Bool move_prev)
{
	GF_DOM_Event evt;
	Bool current_focus = 1;
	u32 ret = 0;
	GF_Node *n, *prev;

	prev = sr->focus_node;

	if (!sr->focus_node) {
		sr->focus_node = gf_sg_get_root_node(sr->compositor->scene);
		if (!sr->focus_node) return 0;
		current_focus = 0;
	}

	/*get focus in current doc order*/
	if (move_prev) {
		n = svg_set_focus_prev(sr, sr->focus_node, current_focus);
		if (!n) n = svg_browse_parent_for_focus_prev(sr, sr->focus_node);
	} else {
		n = svg_set_focus_next(sr, sr->focus_node, current_focus);
		if (!n) n = svg_browse_parent_for_focus_next(sr, sr->focus_node);
	}
	sr->focus_node = n;

	ret = 0;
	if (prev != sr->focus_node) {
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		if (prev) {
			evt.bubbles = 1;
			evt.target = prev;
			evt.type = GF_EVENT_FOCUSOUT;
			gf_dom_event_fire(prev, NULL, &evt);
		}
		if (sr->focus_node) {
			evt.bubbles = 1;
			evt.target = sr->focus_node;
			evt.type = GF_EVENT_FOCUSIN;
			gf_dom_event_fire(sr->focus_node, NULL, &evt);
		}
		//invalidate in case we draw focus rect
		gf_sr_invalidate(sr->compositor, NULL);
	}
	return ret;
}

u32 render_svg_focus_navigate(Render *sr, u32 key_code)
{
	SVGAllAttributes atts;
	GF_DOM_Event evt;
	u32 ret = 0;
	GF_Node *n;
	SVG_Focus *focus = NULL;

	if (!sr->focus_node) return 0;
	n=NULL;
	gf_svg_flatten_attributes((SVG_Element *)sr->focus_node, &atts);
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
		focus->target.target = gf_sg_find_node_by_name(sr->compositor->scene, focus->target.string+1);
	}
	n = focus->target.target;

	ret = 0;
	if (n != sr->focus_node) {
		/*the event is already handled, even though no listeners may be present*/
		ret = 1;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		if (sr->focus_node) {
			evt.target = sr->focus_node;
			evt.type = GF_EVENT_FOCUSOUT;
			gf_dom_event_fire(sr->focus_node, NULL, &evt);
		}
		if (n) {
			evt.relatedTarget = n;
			evt.type = GF_EVENT_FOCUSIN;
			gf_dom_event_fire(n, NULL, &evt);
		}
		sr->focus_node = n;
		//invalidate in case we draw focus rect
		gf_sr_invalidate(sr->compositor, NULL);
	}
	return ret;
}


/*
	This is the generic routine for child traversing
*/
void svg_render_node(GF_Node *node, GF_TraverseState *tr_state)
{
#if 0
	Bool has_listener = gf_dom_listener_count(node);
//	fprintf(stdout, "rendering %s of type %s\n", gf_node_get_name(node), gf_node_get_class_name(node));
	if (has_listener) {
		tr_state->nb_listeners++;
		gf_node_render(node, tr_state);
		tr_state->nb_listeners--;
	} else {
		gf_node_render(node, tr_state);
	}
#else
	gf_node_render(node, tr_state);
#endif
}
void render_svg_node_list(GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	while (children) {
		svg_render_node(children->node, tr_state);
		children = children->next;
	}
}

Bool render_svg_is_display_off(SVGPropertiesPointers *props)
{
	return (props->display && (*(props->display) == SVG_DISPLAY_NONE)) ? 1 : 0;
}

void render_svg_get_nodes_bounds(GF_Node *self, GF_ChildNodeItem *children, GF_TraverseState *tr_state)
{
	GF_Rect rc;
	GF_Matrix2D cur_mx;

	gf_mx2d_copy(cur_mx, tr_state->transform);
	rc = gf_rect_center(0,0);
	while (children) {
		gf_mx2d_init(tr_state->transform);
		tr_state->bounds = gf_rect_center(0,0);

		gf_node_render(children->node, tr_state);
		/*we hit the target node*/
		if (children->node == tr_state->for_node) {
			tr_state->for_node = NULL;
			return;
		}

		gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
		gf_rect_union(&rc, &tr_state->bounds);
		children = children->next;
	}
	gf_mx2d_copy(tr_state->transform, cur_mx);
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	tr_state->bounds = rc;
}

void render_svg_apply_local_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix tmp;
		Bool is_draw = (tr_state->traversing_mode==TRAVERSE_RENDER) ? 1 : 0;
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

void render_svg_restore_parent_transformation(GF_TraverseState *tr_state, GF_Matrix2D *backup_matrix_2d, GF_Matrix *backup_matrix)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_RENDER) 
			visual_3d_matrix_pop(tr_state->visual);
		gf_mx_copy(tr_state->model_matrix, *backup_matrix);  
		return;
	} 
#endif
	gf_mx2d_copy(tr_state->transform, *backup_matrix_2d);  
}


void render_svg_render_base(GF_Node *node, SVGAllAttributes *all_atts, GF_TraverseState *tr_state, 
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
#endif
	tr_state->svg_flags &= inherited_flags_mask;
	tr_state->svg_flags |= gf_node_dirty_get(node);
}

static Bool svg_set_viewport_transformation(GF_TraverseState *tr_state, SVGAllAttributes *atts, Bool is_root) 
{
	SVG_PreserveAspectRatio par;
	Fixed scale, vp_w, vp_h;
	Fixed parent_width, parent_height;
	GF_Matrix2D mat;

	gf_mx2d_init(mat);

	/*canvas size negociation has already been done when attaching the scene to the renderer*/
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

static void svg_render_svg(GF_Node *node, void *rs, Bool is_destroy)
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
	
	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	/*enable or disable navigation*/
	tr_state->visual->render->navigation_disabled = (all_atts.zoomAndPan && *all_atts.zoomAndPan == SVG_ZOOMANDPAN_DISABLE) ? 1 : 0;

	if (render_svg_is_display_off(tr_state->svg_props)) {
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
		if (tr_state->traversing_mode==TRAVERSE_RENDER) {
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
			} else if (tr_state->visual->render->compositor->back_color != viewport_color) {
				tr_state->visual->render->compositor->back_color = viewport_color;
				/*clear visual*/
				visual_2d_clear(tr_state->visual, NULL, 0);
				gf_sr_invalidate(tr_state->visual->render->compositor, NULL);
				goto exit;
			}
		}
	}
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		render_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		render_svg_node_list(((SVG_Element *)node)->children, tr_state);
	}

exit:
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		if (tr_state->traversing_mode==TRAVERSE_RENDER) visual_3d_matrix_pop(tr_state->visual);
		gf_mx_copy(tr_state->model_matrix, bck_mx);
	}
#endif
	gf_mx2d_copy(tr_state->transform, backup_matrix);  
	if (!is_root_svg) gf_mx2d_copy(tr_state->vb_transform, vb_bck);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
	tr_state->visual->top_clipper = top_clip;
}

void render_init_svg_svg(Render *sr, GF_Node *node)
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
	GF_Matrix mx_3d;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);

	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	SVGAllAttributes all_atts;

	if (is_destroy) {
		svg_check_focus_upon_destroy(node);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (render_svg_is_display_off(tr_state->svg_props)) {
		u32 prev_flags = tr_state->trav_flags;
		tr_state->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		render_svg_node_list(((SVG_Element *)node)->children, tr_state);
		tr_state->trav_flags = prev_flags;

		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}	
	
	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		render_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		render_svg_node_list(((SVG_Element *)node)->children, tr_state);

		drawable_check_focus_highlight(node, tr_state, NULL);
	}
	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}


void render_init_svg_g(Render *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_render_g);
}


static void svg_render_defs(GF_Node *node, void *rs, Bool is_destroy)
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

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	prev_flags = tr_state->trav_flags;
	tr_state->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
	render_svg_node_list(((SVG_Element *)node)->children, tr_state);
	tr_state->trav_flags = prev_flags;

	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}


void render_init_svg_defs(Render *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_render_defs);
}

static void svg_render_switch(GF_Node *node, void *rs, Bool is_destroy)
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

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (render_svg_is_display_off(tr_state->svg_props)) {
//		render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}
	
	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		render_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		GF_ChildNodeItem *l = ((SVG_Element *)node)->children;
		while (l) {
			if (1 /*eval_conditional(tr_state->visual->render->compositor, (SVG_Element*)l->node)*/) {
				svg_render_node((GF_Node*)l->node, tr_state);
				break;
			}
			l = l->next;
		}
	}


	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

void render_init_svg_switch(Render *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, svg_render_switch);
}

static Bool svg_drawable_is_over(Drawable *drawable, Fixed x, Fixed y, GF_Path *path, DrawAspect2D *asp, GF_TraverseState *tr_state)
{
	Bool check_fill, check_stroke, check_over, check_outline, check_vis;
	u8 ptr_evt;
	if (!path) path = drawable->path;

	ptr_evt = *tr_state->svg_props->pointer_events;

	if (ptr_evt==SVG_POINTEREVENTS_NONE) {
		return 0;
	}

	if (ptr_evt==SVG_POINTEREVENTS_BOUNDINGBOX) {
		if ( (x >= path->bbox.x) && (y <= path->bbox.y) 
		&& (x <= path->bbox.x + path->bbox.width) && (y >= path->bbox.y - path->bbox.height) )
			return 1;
		return 0;
	} 
	
	check_fill = check_stroke = check_over = check_outline = check_vis = 0;
	/*FIXME - todo*/
	switch (ptr_evt) {
	case SVG_POINTEREVENTS_VISIBLE:
		check_vis = 1;
		check_fill = 1;
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_VISIBLEFILL:
		check_vis = 1;
		check_fill = 2;
		break;
	case SVG_POINTEREVENTS_VISIBLESTROKE:
		check_vis = 1;
		check_stroke = 2;
		break;
	case SVG_POINTEREVENTS_VISIBLEPAINTED:
		check_vis = 1;
		check_fill = 2;
		check_stroke = 2;
		break;
	case SVG_POINTEREVENTS_FILL:
		check_fill = 1;
		break;
	case SVG_POINTEREVENTS_STROKE:
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_ALL:
		check_fill = 1;
		check_stroke = 1;
		break;
	case SVG_POINTEREVENTS_PAINTED:
		check_fill = 2;
		check_stroke = 2;
		break;
	default:
		return 0;
	}

	if (check_vis) {
		if (*tr_state->svg_props->visibility!=SVG_VISIBILITY_VISIBLE) return 0;
	}

	if (check_fill) {
		/*not painted*/
		if ((check_fill==2) && !asp->fill_texture && !GF_COL_A(asp->fill_color) ) return 0;
		/*point is over path*/
		if (gf_path_point_over(path, x, y)) return 1;
	}
	if (check_stroke) {
		StrikeInfo2D *si;
		/*not painted*/
		if ((check_stroke==2) && !asp->line_texture && !GF_COL_A(asp->line_color) ) return 0;

		si = drawable_get_strikeinfo(tr_state->visual->render, drawable, asp, tr_state->appear, NULL, 0, NULL);
		/*point is over outline*/
		if (si && si->outline && gf_path_point_over(si->outline, x, y)) 
			return 1;
	}
	return 0;
}

void svg_drawable_3d_pick(Drawable *drawable, GF_TraverseState *tr_state, DrawAspect2D *asp) 
{
	SFVec3f local_pt, world_pt, vdiff;
	SFVec3f hit_normal;
	SFVec2f text_coords;
	u32 i, count;
	Fixed sqdist;
	Bool node_is_over;
	Render *sr;
	GF_Matrix mx;
	GF_Ray r;
	u32 cull_bckup = tr_state->cull_flag;

	sr = tr_state->visual->render;

	node_is_over = 0;
	r = tr_state->ray;
	gf_mx_copy(mx, tr_state->model_matrix);
	gf_mx_inverse(&mx);
	gf_mx_apply_ray(&mx, &r);

	/*if we already have a hit point don't check anything below...*/
	if (sr->hit_info.picked_square_dist && !sr->grabbed_sensor && !tr_state->layer3d) {
		GF_Plane p;
		GF_BBox box;
		SFVec3f hit = sr->hit_info.world_point;
		gf_mx_apply_vec(&mx, &hit);
		p.normal = r.dir;
		p.d = -1 * gf_vec_dot(p.normal, hit);
		gf_bbox_from_rect(&box, &drawable->path->bbox);

		if (gf_bbox_plane_relation(&box, &p) == GF_BBOX_FRONT) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Picking: bounding box of node %s (DEF %s) below current hit point - skipping\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node)));
			return;
		}
	}
	node_is_over = 0;
	if (render_get_2d_plane_intersection(&r, &local_pt)) {
		node_is_over = svg_drawable_is_over(drawable, local_pt.x, local_pt.y, NULL, asp, tr_state);
	}

	if (!node_is_over) return;

	hit_normal.x = hit_normal.y = 0; hit_normal.z = FIX_ONE;
	text_coords.x = gf_divfix(local_pt.x, drawable->path->bbox.width) + FIX_ONE/2;
	text_coords.y = gf_divfix(local_pt.y, drawable->path->bbox.height) + FIX_ONE/2;

	/*check distance from user and keep the closest hitpoint*/
	world_pt = local_pt;
	gf_mx_apply_vec(&tr_state->model_matrix, &world_pt);

	for (i=0; i<tr_state->num_clip_planes; i++) {
		if (gf_plane_get_distance(&tr_state->clip_planes[i], &world_pt) < 0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Picking: node %s (def %s) is not in clipper half space\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node)));
			return;
		}
	}

	gf_vec_diff(vdiff, world_pt, tr_state->ray.orig);
	sqdist = gf_vec_lensq(vdiff);
	if (sr->hit_info.picked_square_dist && (sr->hit_info.picked_square_dist+FIX_EPSILON<sqdist)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Picking: node %s (def %s) is farther (%g) than current pick (%g)\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node), FIX2FLT(sqdist), FIX2FLT(sr->hit_info.picked_square_dist)));
		return;
	}

	sr->hit_info.picked_square_dist = sqdist;

	/*also stack any VRML sensors present at the current level. If the event is not catched
	by a listener in the SVG tree, the event will be forwarded to the VRML tree*/
	gf_list_reset(sr->sensors);
	count = gf_list_count(tr_state->vrml_sensors);
	for (i=0; i<count; i++) {
		gf_list_add(sr->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}

	gf_mx_copy(sr->hit_info.world_to_local, tr_state->model_matrix);
	gf_mx_copy(sr->hit_info.local_to_world, mx);
	sr->hit_info.local_point = local_pt;
	sr->hit_info.world_point = world_pt;
	sr->hit_info.world_ray = tr_state->ray;
	sr->hit_info.hit_normal = hit_normal;
	sr->hit_info.hit_texcoords = text_coords;

	sr->hit_info.appear = NULL;
	sr->hit_info.picked = drawable->node;
	sr->hit_info.use_dom_events = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Renderer] Picking: node %s (def %s) is under mouse - hit %g %g %g\n", gf_node_get_class_name(drawable->node), gf_node_get_name(drawable->node),
			FIX2FLT(world_pt.x), FIX2FLT(world_pt.y), FIX2FLT(world_pt.z)));
}

void svg_drawable_pick(GF_Node *node, Drawable *drawable, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;
	GF_Matrix2D inv_2d;
	Fixed x, y;
	Bool picked = 0;
	PickingInfo *hit = &tr_state->visual->render->hit_info;
	SVGPropertiesPointers backup_props;
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGAllAttributes all_atts;

	if (!drawable->path) return;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	memcpy(&backup_props, tr_state->svg_props, sizeof(SVGPropertiesPointers));
	gf_svg_apply_inheritance(&all_atts, tr_state->svg_props);

	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_svg(node, &asp, tr_state);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		svg_drawable_3d_pick(drawable, tr_state, &asp);
		render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
		return;
	} 
#endif
	gf_mx2d_copy(inv_2d, tr_state->transform);
	gf_mx2d_inverse(&inv_2d);
	x = tr_state->ray.orig.x;
	y = tr_state->ray.orig.y;
	gf_mx2d_apply_coords(&inv_2d, &x, &y);

	picked = svg_drawable_is_over(drawable, x, y, NULL, &asp, tr_state);

	if (picked) {
		u32 count, i;
		hit->local_point.x = x;
		hit->local_point.y = y;
		hit->local_point.z = 0;

		gf_mx_from_mx2d(&hit->world_to_local, &tr_state->transform);
		gf_mx_from_mx2d(&hit->local_to_world, &inv_2d);

		hit->picked = drawable->node;
		hit->use_dom_events = 1;
		hit->hit_normal.x = hit->hit_normal.y = 0; hit->hit_normal.z = FIX_ONE;
		hit->hit_texcoords.x = gf_divfix(x, drawable->path->bbox.width) + FIX_ONE/2;
		hit->hit_texcoords.y = gf_divfix(y, drawable->path->bbox.height) + FIX_ONE/2;

		if (render_has_composite_texture(tr_state->appear)) {
			hit->appear = tr_state->appear;
		} else {
			hit->appear = NULL;
		}

		/*also stack any VRML sensors present at the current level. If the event is not catched
		by a listener in the SVG tree, the event will be forwarded to the VRML tree*/
		gf_list_reset(tr_state->visual->render->sensors);
		count = gf_list_count(tr_state->vrml_sensors);
		for (i=0; i<count; i++) {
			gf_list_add(tr_state->visual->render->sensors, gf_list_get(tr_state->vrml_sensors, i));
		}
	}

	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
}

static void svg_drawable_render(GF_Node *node, void *rs, Bool is_destroy,
							void (*rebuild_path)(GF_Node *, Drawable *, SVGAllAttributes *),
							Bool is_svg_rect, Bool is_svg_path)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	DrawableContext *ctx;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	Drawable *stack = (Drawable *)gf_node_get_private(node);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;

	if (is_destroy) {
#if USE_GF_PATH
		/* The path is the same as the one in the SVG node, don't delete it here */
		if (is_svg_path) stack->path = NULL;
#endif
		drawable_node_del(node);
		return;
	}
	assert(tr_state->traversing_mode!=TRAVERSE_DRAW_2D);

	
	if (tr_state->traversing_mode==TRAVERSE_PICK) {
		svg_drawable_pick(node, stack, tr_state);
		return;
	}

	/*flatten attributes and apply animations + inheritance*/
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	render_svg_render_base(node, &all_atts, (GF_TraverseState *)rs, &backup_props, &backup_flags);
	
	/* Recreates the path (i.e the shape) only if the node is dirty 
	   (has changed compared to the previous rendering phase) */
	if (gf_node_dirty_get(node) & GF_SG_SVG_GEOMETRY_DIRTY) {
		/*the rebuild function is responsible for cleaning the path*/
		rebuild_path(node, stack, &all_atts);
		gf_node_dirty_clear(node, GF_SG_SVG_GEOMETRY_DIRTY);
		drawable_mark_modified(stack, tr_state);
	}
	if (all_atts.d) {
		if (*(tr_state->svg_props->fill_rule)==GF_PATH_FILL_ZERO_NONZERO) {
			if (!(stack->path->flags & GF_PATH_FILL_ZERO_NONZERO)) {
				stack->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
				drawable_mark_modified(stack, tr_state);
			}
		} else {
			if (stack->path->flags & GF_PATH_FILL_ZERO_NONZERO) {
				stack->path->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
				drawable_mark_modified(stack, tr_state);
			}
		}
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		if (! render_svg_is_display_off(tr_state->svg_props)) {
			gf_path_get_bounds(stack->path, &tr_state->bounds);
			render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
			gf_mx2d_apply_rect(&tr_state->transform, &tr_state->bounds);
			render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	} else if (tr_state->traversing_mode == TRAVERSE_RENDER) {

		if (!render_svg_is_display_off(tr_state->svg_props) &&
			( *(tr_state->svg_props->visibility) != SVG_VISIBILITY_HIDDEN) ) {

			render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

			ctx = drawable_init_context_svg(stack, tr_state);
			if (ctx) {
				if (is_svg_rect) {
					if (ctx->aspect.fill_texture && ctx->aspect.fill_texture->transparent) {}
					else if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {}
					else if (ctx->transform.m[1] || ctx->transform.m[3]) {}
					else {
						ctx->flags &= ~CTX_IS_TRANSPARENT;
					}
				}

				if (all_atts.pathLength && all_atts.pathLength->type==SVG_NUMBER_VALUE) 
					ctx->aspect.pen_props.path_length = all_atts.pathLength->value;

#ifndef GPAC_DISABLE_3D
				if (tr_state->visual->type_3d) {
					if (!stack->mesh) {
						stack->mesh = new_mesh();
						mesh_from_path(stack->mesh, stack->path);
					}
					visual_3d_draw_svg(ctx, tr_state);
					ctx->drawable = NULL;
				} else 
#endif
				{
					drawable_finalize_render(ctx, tr_state, NULL);
				}
			}
			render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		}
	}

	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

static void svg_rect_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	Fixed rx = (atts->rx ? atts->rx->value : 0);
	Fixed ry = (atts->ry ? atts->ry->value : 0);
	Fixed x = (atts->x ? atts->x->value : 0);
	Fixed y = (atts->y ? atts->y->value : 0);
	Fixed width = (atts->width ? atts->width->value : 0);
	Fixed height = (atts->height ? atts->height->value : 0);

	drawable_reset_path(stack);

	if (rx || ry) {
		if (rx >= width/2) rx = width/2;
		if (ry >= height/2) ry = height/2;
		if (rx == 0) rx = ry;
		if (ry == 0) ry = rx;
		gf_path_add_move_to(stack->path, x+rx, y);
		gf_path_add_line_to(stack->path, x+width-rx, y);
		gf_path_add_quadratic_to(stack->path, x+width, y, x+width, y+ry);
		gf_path_add_line_to(stack->path, x+width, y+height-ry);
		gf_path_add_quadratic_to(stack->path, x+width, y+height, x+width-rx, y+height);
		gf_path_add_line_to(stack->path, x+rx, y+height);
		gf_path_add_quadratic_to(stack->path, x, y+height, x, y+height-ry);
		gf_path_add_line_to(stack->path, x, y+ry);
		gf_path_add_quadratic_to(stack->path, x, y, x+rx, y);
		gf_path_close(stack->path);
	} else {
		gf_path_add_move_to(stack->path, x, y);
		gf_path_add_line_to(stack->path, x+width, y);		
		gf_path_add_line_to(stack->path, x+width, y+height);		
		gf_path_add_line_to(stack->path, x, y+height);		
		gf_path_close(stack->path);		
	}
}

static void svg_render_rect(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_rect_rebuild, 1, 0);
}

void render_init_svg_rect(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_rect);
}

static void svg_circle_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	Fixed r = 2*(atts->r ? atts->r->value : 0);
	drawable_reset_path(stack);
	gf_path_add_ellipse(stack->path, (atts->cx ? atts->cx->value : 0), (atts->cy ? atts->cy->value : 0), r, r);
}

static void svg_render_circle(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_circle_rebuild, 0, 0);
}

void render_init_svg_circle(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_circle);
}

static void svg_ellipse_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	drawable_reset_path(stack);
	gf_path_add_ellipse(stack->path, (atts->cx ? atts->cx->value : 0), 
								  (atts->cy ? atts->cy->value : 0), 
								  (atts->rx ? 2*atts->rx->value : 0), 
								  (atts->ry ? 2*atts->ry->value : 0));
}
static void svg_render_ellipse(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_ellipse_rebuild, 0, 0);
}

void render_init_svg_ellipse(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_ellipse);
}

static void svg_line_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	drawable_reset_path(stack);
	gf_path_add_move_to(stack->path, (atts->x1 ? atts->x1->value : 0), (atts->y1 ? atts->y1->value : 0));
	gf_path_add_line_to(stack->path, (atts->x2 ? atts->x2->value : 0), (atts->y2 ? atts->y2->value : 0));
}
static void svg_render_line(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_line_rebuild, 0, 0);
}

void render_init_svg_line(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_line);
}

static void svg_polyline_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	u32 i, nbPoints;
	drawable_reset_path(stack);
	if (atts->points) 
		nbPoints = gf_list_count(*atts->points);
	else 
		nbPoints = 0;

	if (nbPoints) {
		SVG_Point *p = (SVG_Point *)gf_list_get(*atts->points, 0);
		gf_path_add_move_to(stack->path, p->x, p->y);
		for (i = 1; i < nbPoints; i++) {
			p = (SVG_Point *)gf_list_get(*atts->points, i);
			gf_path_add_line_to(stack->path, p->x, p->y);
		}
	} else {
		gf_path_add_move_to(stack->path, 0, 0);
	}
}
static void svg_render_polyline(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_polyline_rebuild, 0, 0);
}

void render_init_svg_polyline(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_polyline);
}

static void svg_polygon_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
	u32 i, nbPoints;
	drawable_reset_path(stack);
	if (atts->points) 
		nbPoints = gf_list_count(*atts->points);
	else 
		nbPoints = 0;

	if (nbPoints) {
		SVG_Point *p = (SVG_Point *)gf_list_get(*atts->points, 0);
		gf_path_add_move_to(stack->path, p->x, p->y);
		for (i = 1; i < nbPoints; i++) {
			p = (SVG_Point *)gf_list_get(*atts->points, i);
			gf_path_add_line_to(stack->path, p->x, p->y);
		}
	} else {
		gf_path_add_move_to(stack->path, 0, 0);
	}
	/*according to the spec, the polygon path is closed*/
	gf_path_close(stack->path);
}
static void svg_render_polygon(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_polygon_rebuild, 0, 0);
}

void render_init_svg_polygon(Render *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, svg_render_polygon);
}


static void svg_path_rebuild(GF_Node *node, Drawable *stack, SVGAllAttributes *atts)
{
#if USE_GF_PATH
	drawable_reset_path_outline(stack);
	stack->path = atts->d;
#else
	drawable_reset_path(stack);
	gf_svg_path_build(stack->path, atts->d->commands, atts->d->points);
#endif
}

static void svg_render_path(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_drawable_render(node, rs, is_destroy, svg_path_rebuild, 0, 1);
}

void render_init_svg_path(Render *sr, GF_Node *node)
{
	Drawable *dr = drawable_stack_new(sr, node);
	gf_path_del(dr->path);
	dr->path = NULL;
	gf_node_set_callback_function(node, svg_render_path);
}

static void svg_render_a(GF_Node *node, void *rs, Bool is_destroy)
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

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	if (render_svg_is_display_off(tr_state->svg_props)) {
		u32 prev_flags = tr_state->trav_flags;
		tr_state->trav_flags |= GF_SR_TRAV_SWITCHED_OFF;
		render_svg_node_list(((SVG_Element *)node)->children, tr_state);
		tr_state->trav_flags = prev_flags;

		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}	
	
	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		render_svg_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state);
	} else {
		render_svg_node_list(((SVG_Element *)node)->children, tr_state);
	}
	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
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

void render_init_svg_a(Render *sr, GF_Node *node)
{
	SVG_handlerElement *handler;
	gf_node_set_callback_function(node, svg_render_a);

	/*listener for onClick event*/
	handler = gf_dom_listener_build(node, GF_EVENT_CLICK, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for activate event*/
	handler = gf_dom_listener_build(node, GF_EVENT_ACTIVATE, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

	/*listener for mousemove event*/
	handler = gf_dom_listener_build(node, GF_EVENT_MOUSEOVER, 0, NULL);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;
	gf_node_set_private((GF_Node *)handler, sr->compositor);

}

/* TODO: FIX ME we actually ignore the given sub_root since it is only valid 
	     when animations have been performed,
         animations evaluation (Render_base) should be part of the core renderer */
void render_svg_render_use(GF_Node *node, GF_Node *sub_root, void *rs)
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

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);

	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
		if (!render_svg_is_display_off(tr_state->svg_props)) {
			if (all_atts.xlink_href) gf_node_render(all_atts.xlink_href->target, tr_state);
			gf_mx2d_apply_rect(&translate, &tr_state->bounds);
		} 
		render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
		goto end;
	}

	if (render_svg_is_display_off(tr_state->svg_props) ||
		(*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN)) {
		goto end;
	}

	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx_add_matrix_2d(&tr_state->model_matrix, &translate);

		if (tr_state->traversing_mode==TRAVERSE_RENDER) {
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
		gf_node_render(sub_root, tr_state);
		tr_state->parent_use = prev_use;
	}
	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);  

end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}


void render_svg_render_animation(GF_Node *node, GF_Node *sub_root, void *rs)
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

	render_svg_render_base(node, &all_atts, tr_state, &backup_props, &backup_flags);


	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);
	
	if (render_svg_is_display_off(tr_state->svg_props) ||
		*(tr_state->svg_props->visibility) == SVG_VISIBILITY_HIDDEN) {
		goto end;
	}

	render_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &backup_matrix3d);


#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx_add_matrix_2d(&tr_state->model_matrix, &translate);

		if (tr_state->traversing_mode==TRAVERSE_RENDER) {
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
		//gf_sr_render_inline(st->is->root_od->term->renderer, root, rs);
		gf_node_render(root, rs);
		tr_state->svg_props = old_props;
		gf_svg_properties_reset_pointers(&new_props);
//	}
#endif

	old_props = tr_state->svg_props;
	tr_state->svg_props = NULL;
	
	prev_vp_atts = tr_state->parent_vp_atts;
	tr_state->parent_vp_atts = &all_atts;
	prev_vp = tr_state->vp_size;
	tr_state->vp_size.x = gf_sr_svg_convert_length_to_display(tr_state->visual->render->compositor, all_atts.width);
	tr_state->vp_size.y = gf_sr_svg_convert_length_to_display(tr_state->visual->render->compositor, all_atts.height);

	/*setup new clipper*/
	rc.width = tr_state->vp_size.x;
	rc.height = tr_state->vp_size.y;
	rc.x = 0;
	rc.y = tr_state->vp_size.y;
	gf_mx2d_apply_rect(&tr_state->transform, &rc);
	prev_clip = tr_state->visual->top_clipper;
	clip = gf_rect_pixelize(&rc);
//	gf_irect_intersect(&tr_state->visual->top_clipper, &clip);

	gf_node_render(sub_root, rs);
	tr_state->svg_props = old_props;
	tr_state->visual->top_clipper = prev_clip;

	tr_state->parent_vp_atts = prev_vp_atts;
	tr_state->vp_size = prev_vp;

	render_svg_restore_parent_transformation(tr_state, &backup_matrix, &backup_matrix3d);  
end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

#endif


