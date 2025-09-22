/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
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

#if !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)

#include "nodes_stacks.h"
#include "offscreen_cache.h"

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/mediaobject.h>

#include <gpac/nodes_svg.h>
#include <gpac/compositor.h>

/*for svg <g> caching*/
#include "mpeg4_grouping.h"


typedef struct
{
	Bool root_svg;
	SVGPropertiesPointers *svg_props;
	GF_Matrix2D viewbox_mx;
	Drawable *vp_fill;
	u32 prev_color;
	/*parent VP size used to compute the vp->ViewBox matrix*/
	SFVec2f parent_vp;
	/*current VP size used by all children*/
	SFVec2f vp;
	Fixed dx, dy, vpw, vph;
} SVGsvgStack;


static void svg_recompute_viewport_transformation(GF_Node *node, SVGsvgStack *stack, GF_TraverseState *tr_state, SVGAllAttributes *atts)
{
	GF_Matrix2D mx;
	SVG_ViewBox ext_vb, *vb;
	SVG_PreserveAspectRatio par;
	Fixed scale, vp_w, vp_h;
	Fixed parent_width, parent_height, doc_width, doc_height;

	/*canvas size negotiation has already been done when attaching the scene to the compositor*/
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

	stack->vp = stack->parent_vp = tr_state->vp_size;

	vb = atts->viewBox;

	gf_mx2d_init(mx);

	if (stack->root_svg && !tr_state->parent_is_use) {
		const char *frag_uri = gf_scene_get_fragment_uri(node);
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
					Bool ret = gf_svg_parse_transformlist(&mx, (char *) frag_uri+18);
					if (!ret) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] Error parsing SVG View transform component: %s\n", frag_uri+18));
					}
				}
			}
			/*fragID*/
			else {
				GF_Node *target = gf_sg_find_node_by_name(gf_node_get_graph(node), (char *) frag_uri);
				if (target) {
					GF_Matrix2D vp_mx;
					GF_TraverseState bounds_state;
					memset(&bounds_state, 0, sizeof(bounds_state));
					bounds_state.traversing_mode = TRAVERSE_GET_BOUNDS;
					bounds_state.visual = tr_state->visual;
					bounds_state.for_node = target;
					bounds_state.svg_props = tr_state->svg_props;
					gf_mx2d_init(bounds_state.transform);
					gf_mx2d_init(bounds_state.mx_at_node);
					gf_mx_init(tr_state->visual->compositor->hit_world_to_local);
					gf_sc_get_nodes_bounds(node, ((GF_ParentNode *)node)->children, &bounds_state, NULL);
					gf_mx2d_from_mx(&vp_mx, &tr_state->visual->compositor->hit_world_to_local);
					gf_mx2d_apply_rect(&vp_mx, &bounds_state.bounds);
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
	stack->vp.x = vb->width;
	stack->vp.y = vb->height;

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
		stack->dx = dx;
		stack->dy = dy;
		stack->vpw = vp_w;
		stack->vph = vp_h;

#if 0
		/*we need a clipper*/
		if (stack->root_svg && !tr_state->parent_anim_atts && (par.meetOrSlice==SVG_MEETORSLICE_SLICE)) {
			GF_Rect rc;
			rc.width = parent_width;
			rc.height = parent_height;
			rc.x = dx;
			rc.y = dy + parent_height;
			tr_state->visual->top_clipper = gf_rect_pixelize(&rc);
		}
#endif

	}
	gf_mx2d_add_matrix(&stack->viewbox_mx, &mx);
}

static void svg_traverse_svg(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool rootmost_svg, send_resize;
	u32 viewport_color;
	SVGsvgStack *stack;
	GF_Matrix2D backup_matrix, vb_bck;
#ifndef GPAC_DISABLE_3D
	GF_Matrix bck_mx;
#endif
	Bool is_dirty;
	GF_IRect top_clip;
	SFVec2f prev_vp;
	SVGPropertiesPointers backup_props, *prev_props;
	u32 backup_flags;
	Bool invalidate_flag;
	u32 styling_size = sizeof(SVGPropertiesPointers);
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;
	SVGAllAttributes all_atts;
	stack = gf_node_get_private(node);

	if (is_destroy) {
		if (stack->svg_props) {
			gf_svg_properties_reset_pointers(stack->svg_props);
			gf_free(stack->svg_props);
		}
		gf_sc_check_focus_upon_destroy(node);
		if (stack->vp_fill) drawable_del(stack->vp_fill);
		gf_free(stack);
		return;
	}

	prev_props = tr_state->svg_props;
	/*SVG props not set: we are either the root-most <svg> of the compositor
	or an <svg> inside an <animation>*/
	if (!tr_state->svg_props) {
		tr_state->svg_props = stack->svg_props;
		if (!tr_state->svg_props) return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags)) {
		tr_state->svg_props = prev_props;
		return;
	}

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
	//commented to get rid of GCC warning
	//if (tr_state->visual->type_3d)
	gf_mx_copy(bck_mx, tr_state->model_matrix);

#endif

	invalidate_flag = tr_state->invalidate_all;

	is_dirty = gf_node_dirty_get(node);
	if (is_dirty  & GF_SG_CHILD_DIRTY) drawable_reset_group_highlight(tr_state, node);
	gf_node_dirty_clear(node, 0);

	send_resize = 0;
	if ((stack->parent_vp.x != tr_state->vp_size.x) || (stack->parent_vp.y != tr_state->vp_size.y)) {
		is_dirty = 1;
		send_resize = 1;
	}

	if (is_dirty || tr_state->visual->compositor->recompute_ar) {
		svg_recompute_viewport_transformation(node, stack, tr_state, &all_atts);
	}

	gf_mx2d_copy(tr_state->vb_transform, stack->viewbox_mx);

	rootmost_svg = (stack->root_svg && !tr_state->parent_anim_atts) ? 1 : 0;
	if (tr_state->traversing_mode == TRAVERSE_SORT) {
		SVG_Paint *vp_fill = NULL;
		Fixed vp_opacity;

		if (tr_state->parent_anim_atts) {
			vp_fill = tr_state->parent_anim_atts->viewport_fill;
			vp_opacity = tr_state->parent_anim_atts->viewport_fill_opacity ? tr_state->parent_anim_atts->viewport_fill_opacity->value : FIX_ONE;
		} else {
			vp_fill = tr_state->svg_props->viewport_fill;
			vp_opacity = tr_state->svg_props->viewport_fill_opacity ? tr_state->svg_props->viewport_fill_opacity->value : FIX_ONE;
		}
		if (tr_state->visual->compositor->noback) {
			vp_fill = NULL;
			vp_opacity = 0;
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
				Fixed width = tr_state->parent_anim_atts ? tr_state->parent_anim_atts->width->value : 0;
				Fixed height = tr_state->parent_anim_atts ? tr_state->parent_anim_atts->height->value : 0;

				if (!stack->vp_fill) {
					stack->vp_fill = drawable_new();
					stack->vp_fill->node = node;
				}
				if ((width != stack->vp_fill->path->bbox.width) || (height != stack->vp_fill->path->bbox.height)) {
					drawable_reset_path(stack->vp_fill);
					gf_path_add_rect(stack->vp_fill->path, 0, 0, width, -height);
				}

				ctx = drawable_init_context_svg(stack->vp_fill, tr_state, NULL);
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


	if (!stack->root_svg)
		gf_mx2d_add_translation(&tr_state->vb_transform, all_atts.x ? all_atts.x->value : 0, all_atts.y ? all_atts.y->value : 0);

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx_add_matrix_2d(&tr_state->model_matrix, &tr_state->vb_transform);
	} else
#endif
	{
		gf_mx2d_pre_multiply(&tr_state->transform, &tr_state->vb_transform);
	}

	/*store VP and move it to current VP (eg, the one used to compute the vb_transform)*/
	prev_vp = tr_state->vp_size;
	tr_state->vp_size = stack->vp;

	/*the event may trigger scripts which may delete nodes / modify the scene. We therefore send the resize event
	before traversing the scene*/
	if (send_resize) {
		GF_DOM_Event evt;
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.bubbles = 1;
		evt.type = GF_EVENT_RESIZE;
		gf_dom_event_fire(node, &evt);
	}
	if ((stack->vp.x != prev_vp.x) || (stack->vp.y != prev_vp.y)) {
		GF_Scene *scene = node->sgprivate->scenegraph->userpriv;

		if (scene) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.bubbles = 0;
			evt.screen_rect.width = stack->vpw;
			evt.screen_rect.height = stack->vph;
			evt.screen_rect.x = stack->dx;
			evt.screen_rect.y = stack->dy;
			evt.prev_translate.x = stack->vp.x;
			evt.prev_translate.y = stack->vp.y;
			evt.type = GF_EVENT_VP_RESIZE;
			gf_scene_notify_event(scene, 0, NULL, &evt, GF_OK, GF_TRUE);
		}
	}

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		gf_sc_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state, NULL);
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}
	tr_state->vp_size = prev_vp;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
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
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg stack\n"));
		return;
	}

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

Bool compositor_svg_get_viewport(GF_Node *n, GF_Rect *rc)
{
	SVGsvgStack *stack;
	if (!n || (gf_node_get_tag(n) != TAG_SVG_svg)) return 0;
	stack = gf_node_get_private(n);
	rc->width = stack->parent_vp.x;
	rc->height = stack->parent_vp.y;
	/*not supported yet*/
	rc->x = rc->y = 0;
	return 1;
}

typedef struct
{
	GROUPING_NODE_STACK_2D
#ifndef GF_SR_USE_VIDEO_CACHE
	struct _group_cache *cache;
#endif

	Drawable *clip_drawable;
} SVGgStack;

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
		SVGgStack *group = gf_node_get_private(node);
#ifdef GF_SR_USE_VIDEO_CACHE
		group_2d_destroy_svg(node, group);
#else
		if (group->cache) group_cache_del(group->cache);
#endif
		drawable_del(group->clip_drawable);
		gf_free(group);
		gf_sc_check_focus_upon_destroy(node);
		return;
	}
	/*group cache traverse routine*/
	else if (tr_state->traversing_mode == TRAVERSE_DRAW_2D
#ifndef GPAC_DISABLE_3D
	         || tr_state->traversing_mode == TRAVERSE_DRAW_3D
#endif
	) {
		SVGgStack *group = gf_node_get_private(node);

		//clip path on group
		if (tr_state->ctx->appear) {
			//draw clip
			if (tr_state->ctx->aspect.fill_color) {
				gf_node_traverse(tr_state->ctx->appear, tr_state);
			}
			//reset clip
			else {
				gf_evg_surface_set_mask_mode(tr_state->visual->raster_surface, GF_EVGMASK_NONE);
			}
			return;
		}
		group_cache_draw(group->cache, tr_state);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

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
		gf_sc_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state, NULL);
	} else if (tr_state->traversing_mode == TRAVERSE_SORT) {
#ifdef GF_SR_USE_DEPTH
		Fixed scale, offset, dscale, doffset;
#endif
		Fixed opacity = FIX_ONE;
		Bool clear = 0;
		SVGgStack *group;

		if (!tr_state->in_svg_filter && all_atts.filter && all_atts.filter->iri.target) {
#ifdef GPAC_ENABLE_SVG_FILTERS
			svg_draw_filter(all_atts.filter->iri.target, node, tr_state);
#endif
			return;
		}
		group = gf_node_get_private(node);

		if (tr_state->parent_use_opacity) {
			opacity = tr_state->parent_use_opacity->value;
			tr_state->parent_use_opacity = NULL;
		}
		if (all_atts.opacity) {
			opacity = gf_mulfix(opacity, all_atts.opacity->value);
		}
		if (gf_node_dirty_get(node)&GF_SG_CHILD_DIRTY) {
			drawable_reset_group_highlight(tr_state, node);
			clear=1;
		}

#ifdef GF_SR_USE_DEPTH
		dscale = FIX_ONE;
		doffset=0;
		if (all_atts.gpac_depthGain && all_atts.gpac_depthGain->type==SVG_NUMBER_VALUE) dscale = all_atts.gpac_depthGain->value;
		if (all_atts.gpac_depthOffset && all_atts.gpac_depthOffset->type==SVG_NUMBER_VALUE) doffset = all_atts.gpac_depthOffset->value;
		scale = tr_state->depth_gain;
		offset = tr_state->depth_offset;
		// new offset is multiplied by parent gain and added to parent offset
		tr_state->depth_offset = gf_mulfix(doffset, scale) + offset;
		// gain is multiplied by parent gain
		tr_state->depth_gain = gf_mulfix(scale, dscale);
#endif

		if (!tr_state->override_appearance && (opacity < FIX_ONE)) {
			if (!group->cache) {
				group->cache = group_cache_new(tr_state->visual->compositor, node);
				group->cache->force_recompute = 1;
			}
			group->cache->opacity = opacity;
			if (tr_state->visual->compositor->zoom_changed)
				group->cache->force_recompute = 1;
			group->flags |= GROUP_IS_CACHED | GROUP_PERMANENT_CACHE;
#ifdef GF_SR_USE_VIDEO_CACHE
			group_2d_cache_traverse(node, group, tr_state);
#else
			group_cache_traverse(node, group->cache, tr_state, group->cache->force_recompute, 0, 0);
#endif
		} else {
#ifdef GF_SR_USE_VIDEO_CACHE
			Bool group_cached;

			group_cached = group_2d_cache_traverse(node, group, tr_state);
			gf_node_dirty_clear(node, GF_SG_CHILD_DIRTY);
			/*group is not cached, traverse the children*/
			if (!group_cached) {
				GF_ChildNodeItem *child;
				DrawableContext *first_ctx = tr_state->visual->cur_context;
				u32 cache_too_small = 0;
				Bool skip_first_ctx = (first_ctx && first_ctx->drawable) ? 1 : 0;
				u32 traverse_time = gf_sys_clock();
				u32 last_cache_idx = gf_list_count(tr_state->visual->compositor->cached_groups_queue);
				tr_state->cache_too_small = 0;

				child = ((GF_ParentNode *)node)->children;
				while (child) {
					gf_node_traverse(child->node, tr_state);
					child = child->next;
					if (tr_state->cache_too_small)
						cache_too_small++;
				}

				if (cache_too_small) {
					tr_state->cache_too_small = 1;
				} else {
					/*get the traversal time for each group*/
					traverse_time = gf_sys_clock() - traverse_time;
					group->traverse_time += traverse_time;
					/*record the traversal information and turn cache on if possible*/
					group_2d_cache_evaluate(node, group, tr_state, first_ctx, skip_first_ctx, last_cache_idx);
				}
			}
#else

			SVG_ClipPath *cp = all_atts.clip_path;
			if (cp && !cp->target.target) {
				cp->target.target = cp->target.string ? gf_sg_find_node_by_name(tr_state->visual->compositor->scene, cp->target.string+1) : NULL;
				if (!cp->target.target) cp = NULL;
			}

			//we handle group clip by faking a drawable the size of the viewport
			//we insert one drawable doing the clipping, and one disabling it
			if (cp && (tr_state->traversing_mode==TRAVERSE_SORT)) {
				DrawableContext *ctx;
				gf_path_reset(group->clip_drawable->path);
				gf_path_add_rect(group->clip_drawable->path,
					INT2FIX(tr_state->visual->top_clipper.x), INT2FIX(tr_state->visual->top_clipper.y),
					INT2FIX(tr_state->visual->top_clipper.width), INT2FIX(tr_state->visual->top_clipper.height)
				);
				ctx = drawable_init_context_svg(group->clip_drawable, tr_state, NULL);
				ctx->appear = (GF_Node *) cp->target.target;
				ctx->aspect.fill_color = 0xFFFFFFFF;
				drawable_finalize_sort(ctx, tr_state, NULL);

				compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);

				ctx = drawable_init_context_svg(group->clip_drawable, tr_state, NULL);
				ctx->appear = (GF_Node *) cp->target.target;
				ctx->aspect.fill_color = 0;
				drawable_finalize_sort(ctx, tr_state, NULL);
			} else {
				compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
			}
#endif
		}
		if (clear) gf_node_dirty_clear(node, 0);

		drawable_check_focus_highlight(node, tr_state, NULL);

#ifdef GF_SR_USE_DEPTH
		tr_state->depth_gain = scale;
		tr_state->depth_offset = offset;
#endif
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
	}
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}


void compositor_init_svg_g(GF_Compositor *compositor, GF_Node *node)
{
	SVGgStack *stack;
	GF_SAFEALLOC(stack, SVGgStack);
	if (!stack) return;
	gf_node_set_private(node, stack);
	stack->clip_drawable = drawable_new();
	stack->clip_drawable->node = node;
	stack->clip_drawable->flags = DRAWABLE_USE_TRAVERSE_DRAW;

	gf_node_set_callback_function(node, svg_traverse_g);
}


#if 0 //unused
static void svg_traverse_defs(GF_Node *node, void *rs, Bool is_destroy)
{
	SVGPropertiesPointers backup_props;
	u32 prev_flags, backup_flags;
	u32 styling_size = sizeof(SVGPropertiesPointers);

	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	SVGAllAttributes all_atts;

	if (is_destroy) {
		gf_sc_check_focus_upon_destroy(node);
		return;
	}
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

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
#endif



static void svg_traverse_switch(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	SVGPropertiesPointers backup_props;
	u32 backup_flags;
	s32 *selected_idx = gf_node_get_private(node);
	u32 styling_size = sizeof(SVGPropertiesPointers);
	SVGAllAttributes all_atts;
	GF_TraverseState *tr_state = (GF_TraverseState *) rs;

	if (is_destroy) {
		gf_free(selected_idx);
		gf_sc_check_focus_upon_destroy(node);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (gf_node_dirty_get(node)) {
		u32 pos = 0;
		GF_ChildNodeItem *child = ((SVG_Element*)node)->children;
		*selected_idx = -1;
		while (child) {
			SVGAllAttributes atts;
			gf_svg_flatten_attributes((SVG_Element *)child->node, &atts);
			if (compositor_svg_evaluate_conditional(tr_state->visual->compositor, &atts)) {
				*selected_idx = pos;
				break;
			}
			pos++;
			child = child->next;
		}
		drawable_reset_group_highlight(tr_state, node);
		gf_node_dirty_clear(node, 0);
	}

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

	if (compositor_svg_is_display_off(tr_state->svg_props)) {
		memcpy(tr_state->svg_props, &backup_props, styling_size);
		tr_state->svg_flags = backup_flags;
		return;
	}

	if (*selected_idx >= 0) {
		compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
		if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
			gf_sc_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state, selected_idx);
		} else {
			GF_Node *child = gf_node_list_get_child(((SVG_Element *)node)->children, *selected_idx);
			gf_node_traverse(child, tr_state);

			drawable_check_focus_highlight(node, tr_state, NULL);
		}
		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	}

	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

void compositor_init_svg_switch(GF_Compositor *compositor, GF_Node *node)
{
	s32 *selected_idx;
	GF_SAFEALLOC(selected_idx, s32);
	if (!selected_idx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate font for svg switch stack\n"));
		return;
	}
	*selected_idx = -1;
	gf_node_set_private(node, selected_idx);
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
		gf_sc_check_focus_upon_destroy(node);
		return;
	}

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

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
		gf_sc_get_nodes_bounds(node, ((SVG_Element *)node)->children, tr_state, NULL);
	} else {
		compositor_svg_traverse_children(((SVG_Element *)node)->children, tr_state);
		if (tr_state->traversing_mode==TRAVERSE_SORT)
			drawable_check_focus_highlight(node, tr_state, NULL);
	}
	compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);
	memcpy(tr_state->svg_props, &backup_props, styling_size);
	tr_state->svg_flags = backup_flags;
}

static void svg_a_set_view(GF_Node *handler, GF_Compositor *compositor, const char *url)
{
	gf_scene_set_fragment_uri(handler, url);
	/*force recompute viewbox of root SVG - FIXME in full this should be the parent svg*/
	gf_node_dirty_set(gf_sg_get_root_node(gf_node_get_graph(handler)), 0, 0);

	compositor->trans_x = compositor->trans_y = 0;
	compositor->rotation = 0;
	compositor->zoom = FIX_ONE;
	compositor_2d_set_user_transform(compositor, FIX_ONE, 0, 0, 0);
	gf_sc_invalidate(compositor, NULL);
}

static void svg_a_handle_event(GF_Node *handler, GF_DOM_Event *event, GF_Node *observer)
{
	GF_Compositor *compositor;
	GF_Event evt;
	SVG_Element *a;
	SVGAllAttributes all_atts;

	if (event->event_phase & GF_DOM_EVENT_PHASE_PREVENT) return;

	gf_assert(gf_node_get_tag((GF_Node*)event->currentTarget->ptr)==TAG_SVG_a);
	a = (SVG_Element *) event->currentTarget->ptr;
	gf_svg_flatten_attributes(a, &all_atts);

	compositor = (GF_Compositor *)gf_node_get_private((GF_Node *)a);

	if (!all_atts.xlink_href) return;

	if (event->type==GF_EVENT_MOUSEOVER) {
		evt.type = GF_EVENT_NAVIGATE_INFO;

		if (all_atts.xlink_title) evt.navigate.to_url = *all_atts.xlink_title;
		else if (all_atts.xlink_href->string) evt.navigate.to_url = all_atts.xlink_href->string;
		else {
			evt.navigate.to_url = gf_node_get_name(all_atts.xlink_href->target);
			if (!evt.navigate.to_url) evt.navigate.to_url = "document internal link";
		}

		gf_sc_send_event(compositor, &evt);
		return;
	}

	evt.type = GF_EVENT_NAVIGATE;

	if (all_atts.xlink_href->type == XMLRI_STRING) {
		evt.navigate.to_url = gf_scene_resolve_xlink(handler, all_atts.xlink_href->string);
		if (evt.navigate.to_url) {
			if (all_atts.target) {
				evt.navigate.parameters = (const char **) &all_atts.target;
				evt.navigate.param_count = 1;
			} else {
				evt.navigate.parameters = NULL;
				evt.navigate.param_count = 0;
			}

			if (evt.navigate.to_url[0] != '#') {
				gf_scene_process_anchor(handler, &evt);
				gf_free((char *)evt.navigate.to_url);
				return;
			}
			all_atts.xlink_href->target = gf_sg_find_node_by_name(gf_node_get_graph(handler), (char *) evt.navigate.to_url+1);
			if (all_atts.xlink_href->target) {
				all_atts.xlink_href->type = XMLRI_ELEMENTID;
				gf_free((char *)evt.navigate.to_url);
			} else {
				svg_a_set_view(handler, compositor, evt.navigate.to_url + 1);
				gf_free((char *)evt.navigate.to_url);
				return;
			}
		}
	}
	if (!all_atts.xlink_href->target) {
		return;
	}
	/*this is a time event*/
	switch (gf_node_get_tag(all_atts.xlink_href->target)) {
	case TAG_SVG_set:
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateTransform:
	case TAG_SVG_animateMotion:
	case TAG_SVG_discard:
	case TAG_SVG_animation:
	case TAG_SVG_video:
	case TAG_SVG_audio:
		gf_smil_timing_insert_clock(all_atts.xlink_href->target, 0, gf_node_get_scene_time((GF_Node *)handler) );
		break;
	default:
		/*this is an implicit SVGView event*/
		svg_a_set_view(handler, compositor, gf_node_get_name(all_atts.xlink_href->target));
		break;
	}
}

void compositor_init_svg_a(GF_Compositor *compositor, GF_Node *node)
{
	SVG_handlerElement *handler;
	gf_node_set_callback_function(node, svg_traverse_a);
	gf_node_set_private((GF_Node *)node, compositor);

	/*listener for onClick event*/
	handler = gf_dom_listener_build(node, GF_EVENT_CLICK, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;

	/*listener for activate event*/
	handler = gf_dom_listener_build(node, GF_EVENT_ACTIVATE, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;

	/*listener for mousemove event*/
	handler = gf_dom_listener_build(node, GF_EVENT_MOUSEOVER, 0);
	/*and overwrite handler*/
	handler->handle_event = svg_a_handle_event;

}

typedef struct
{
	GF_MediaObject *resource;
//	GF_Node *used_node;
	GF_SceneGraph *inline_sg;
	const char *fragment_id;
	Bool needs_play;
	u32 init_vis_state;
} SVGlinkStack;


static void svg_traverse_resource(GF_Node *node, void *rs, Bool is_destroy, Bool is_foreign_object)
{
	GF_Matrix2D backup_matrix;
	GF_Matrix mx_3d;
	GF_Matrix2D translate;
	SVGPropertiesPointers backup_props;
	u32 backup_flags, dirty;
	Bool is_fragment, parent_is_use;
	GF_Node *used_node;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	SVGAllAttributes all_atts;
	SVGlinkStack *stack = gf_node_get_private(node);
	SFVec2f prev_vp;
	SVG_Number *prev_opacity;

	if (is_destroy) {
		if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
		gf_free(stack);
		return;
	}


	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	if (!all_atts.xlink_href) return;

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

	dirty = gf_node_dirty_get(node);
	if (dirty & GF_SG_CHILD_DIRTY) drawable_reset_group_highlight(tr_state, node);

	if (dirty & GF_SG_SVG_XLINK_HREF_DIRTY) {
		stack->fragment_id = NULL;
		stack->inline_sg = NULL;
		if (all_atts.xlink_href->string && (all_atts.xlink_href->string[0]=='#')) {
			stack->fragment_id = all_atts.xlink_href->string;
			stack->inline_sg = gf_node_get_graph(node);
		} else {
			GF_MediaObject *new_res = gf_mo_load_xlink_resource(node, is_foreign_object, 0, -1);
			if (new_res != stack->resource) {
				if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
				stack->resource = new_res;
			}
		}
	}
	gf_node_dirty_clear(node, 0);

	/*locate the used node - this is done at each step to handle progressive loading*/
	is_fragment = 0;
	used_node = NULL;
	if (!stack->inline_sg && !stack->fragment_id && all_atts.xlink_href) {
		if (all_atts.xlink_href->type == XMLRI_ELEMENTID) {
			used_node = all_atts.xlink_href->target;
			is_fragment = 1;
		} else if (stack->resource) {
			stack->inline_sg = gf_mo_get_scenegraph(stack->resource);
			if (!is_foreign_object && all_atts.xlink_href->string) {
				stack->fragment_id = strchr(all_atts.xlink_href->string, '#');
			}
		}
	}
	if (!used_node && stack->inline_sg) {
		if (stack->fragment_id) {
			used_node = gf_sg_find_node_by_name(stack->inline_sg, (char *) stack->fragment_id+1);
			is_fragment = 1;
		} else if (is_foreign_object) {
			used_node = gf_sg_get_root_node(stack->inline_sg);
		}
	}
	if (!used_node) goto end;

	/*stack use nodes for picking*/
	gf_list_add(tr_state->use_stack, used_node);
	gf_list_add(tr_state->use_stack, node);

	gf_mx2d_init(translate);
	translate.m[2] = (all_atts.x ? all_atts.x->value : 0);
	translate.m[5] = (all_atts.y ? all_atts.y->value : 0);

	/*update VP size (SVG 1.1)*/
	prev_vp = tr_state->vp_size;
	if (all_atts.width && all_atts.height) {
		tr_state->vp_size.x = gf_sc_svg_convert_length_to_display(tr_state->visual->compositor, all_atts.width);
		tr_state->vp_size.y = gf_sc_svg_convert_length_to_display(tr_state->visual->compositor, all_atts.height);
	}

	prev_opacity = tr_state->parent_use_opacity;
	tr_state->parent_use_opacity = all_atts.opacity;
	parent_is_use = tr_state->parent_is_use;
	tr_state->parent_is_use = is_foreign_object ? 0 : 1;

	if (tr_state->traversing_mode == TRAVERSE_GET_BOUNDS) {
		compositor_svg_apply_local_transformation(tr_state, &all_atts, &backup_matrix, &mx_3d);
		if (!compositor_svg_is_display_off(tr_state->svg_props)) {
			gf_node_traverse(used_node, tr_state);
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
		} else
#endif
			gf_mx2d_pre_multiply(&tr_state->transform, &translate);


		drawable_check_focus_highlight(node, tr_state, NULL);
		if (is_fragment) {
			gf_node_traverse(used_node, tr_state);
		} else {
			gf_sc_traverse_subscene(tr_state->visual->compositor, node, stack->inline_sg, tr_state);
		}
		compositor_svg_restore_parent_transformation(tr_state, &backup_matrix, &mx_3d);

	}
	gf_list_rem_last(tr_state->use_stack);
	gf_list_rem_last(tr_state->use_stack);
	tr_state->vp_size = prev_vp;

	tr_state->parent_is_use = parent_is_use;
	tr_state->parent_use_opacity = prev_opacity;

end:
	memcpy(tr_state->svg_props, &backup_props, sizeof(SVGPropertiesPointers));
	tr_state->svg_flags = backup_flags;
}

static void svg_traverse_use(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_resource(node, rs, is_destroy, 0);
}

void compositor_init_svg_use(GF_Compositor *compositor, GF_Node *node)
{
	SVGlinkStack *stack;
	GF_SAFEALLOC(stack, SVGlinkStack);
	if (!stack) return;
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
	if (stack->init_vis_state == 3) {
		stack->init_vis_state = 4;
		gf_mo_resume(stack->resource);
	} else if (stack->needs_play || (gf_node_dirty_get(node) & GF_SG_SVG_XLINK_HREF_DIRTY )) {
		SVGAllAttributes all_atts;
		Double clipBegin, clipEnd;
		GF_MediaObject *new_res;
		gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
		clipBegin = all_atts.clipBegin ? *all_atts.clipBegin : 0;
		clipEnd = all_atts.clipEnd ? *all_atts.clipEnd : -1;

		if (stack->needs_play) {
			gf_mo_play(stack->resource, clipBegin, clipEnd, 0);
			stack->needs_play = 0;
		} else {
			Bool primary = all_atts.gpac_useAsPrimary ? *all_atts.gpac_useAsPrimary : 1;
			new_res = gf_mo_load_xlink_resource(node, primary, clipBegin, clipEnd);
			if (new_res != stack->resource) {
				if (stack->resource) gf_mo_unload_xlink_resource(node, stack->resource);
				if (all_atts.xlink_href) all_atts.xlink_href->target = NULL;
				stack->resource = new_res;
				stack->fragment_id = NULL;
				stack->inline_sg = NULL;
			}
			gf_node_dirty_clear(node, 0);
		}
	}
}

static void svg_animation_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, GF_SGSMILTimingEvalState status)
{
	Bool reset_target = GF_FALSE;
	GF_Node *node = gf_smil_get_element(rti);
	SVGlinkStack *stack = gf_node_get_private(node);
	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		svg_animation_smil_update(node, stack, normalized_scene_time);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		if (stack->resource) {
			gf_mo_stop(&stack->resource);
			stack->needs_play = 1;
		}
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		if (stack->resource) {
			reset_target = GF_TRUE;
			gf_mo_unload_xlink_resource(node, stack->resource);
			stack->resource = NULL;
			stack->fragment_id = NULL;
			stack->inline_sg = NULL;
			gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
		}
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		if (stack->resource) {
			reset_target = GF_TRUE;
			stack->fragment_id = NULL;
			stack->inline_sg = NULL;
			gf_mo_restart(stack->resource);
		}
		break;
	default:
		break;
	}
	if (reset_target) {
		SVGAllAttributes all_atts;
		gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
		if (all_atts.xlink_href) all_atts.xlink_href->target=NULL;
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
		gf_free(stack);
		return;
	}
	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);


	if (!stack->inline_sg && !stack->resource) {
		if (!stack->init_vis_state) {
			if (all_atts.initialVisibility && (*all_atts.initialVisibility==SVG_INITIALVISIBILTY_ALWAYS)) {
				stack->init_vis_state = 2;
				svg_animation_smil_update(node, stack, 0);
			} else {
				stack->init_vis_state = 1;
			}
		}
		if (!stack->inline_sg && !stack->resource)
			return;
	}

	if (!all_atts.width || !all_atts.height) return;
	if (!all_atts.width->value || !all_atts.height->value) return;

	if (!compositor_svg_traverse_base(node, &all_atts, tr_state, &backup_props, &backup_flags))
		return;

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
	gf_irect_intersect(&tr_state->visual->top_clipper, &clip);

	if (!stack->inline_sg && stack->resource) {
		stack->inline_sg = gf_mo_get_scenegraph(stack->resource);
	}
	/*if we have the focus, move it to the subtree*/
	if (tr_state->visual->compositor->focus_node==node) {
		GF_Node *subroot = gf_sg_get_root_node(stack->inline_sg);
		if (subroot) tr_state->visual->compositor->focus_node = subroot;
	}

	if (stack->inline_sg && stack->resource->odm) {
		gf_sc_traverse_subscene(tr_state->visual->compositor, node, stack->inline_sg, tr_state);
	}

	if (stack->init_vis_state == 2) {
		stack->init_vis_state = 3;
		gf_mo_pause(stack->resource);
	}

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
	if (!stack) return;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_animation);

	gf_smil_set_evaluation_callback(node, svg_animation_smil_evaluate);

	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
}

void svg_pause_animation(GF_Node *n, Bool pause)
{
	SVGlinkStack *st =  gf_node_get_private(n);
	if (!st) return;
	if (pause) gf_mo_pause(st->resource);
	else gf_mo_resume(st->resource);
}

static void svg_traverse_foreign_object(GF_Node *node, void *rs, Bool is_destroy)
{
	svg_traverse_resource(node, rs, is_destroy, 1);
}

void compositor_init_svg_foreign_object(GF_Compositor *compositor, GF_Node *node)
{
	SVGlinkStack *stack;
	GF_SAFEALLOC(stack, SVGlinkStack);
	if (!stack) return;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_foreign_object);
	/*force first processing of xlink-href*/
	gf_node_dirty_set(node, GF_SG_SVG_XLINK_HREF_DIRTY, 0);
}

GF_Node *compositor_svg_get_xlink_resource_node(GF_Node *node, XMLRI *xlink)
{
	SVGlinkStack *stack;
	switch (gf_node_get_tag(node)) {
	case TAG_SVG_animation:
		stack = gf_node_get_private(node);
		return gf_sg_get_root_node(stack->inline_sg);
	case TAG_SVG_use:
		stack = gf_node_get_private(node);
		if (stack && stack->fragment_id)
			return gf_sg_find_node_by_name(stack->inline_sg, (char *) stack->fragment_id+1);
		return xlink ? xlink->target : NULL;
	}
	return NULL;
}

#if 0 //unused
GF_SceneGraph *gf_sc_animation_get_scenegraph(GF_Node *node)
{
	SVGlinkStack *stack;
	if (node->sgprivate->tag!=TAG_SVG_animation) return NULL;
	stack = gf_node_get_private(node);
	return stack->inline_sg;
}
#endif


#endif //!defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)
