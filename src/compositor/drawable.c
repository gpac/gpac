/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include "drawable.h"
#include "visual_manager.h"
#include "nodes_stacks.h"



/*default draw routine*/
void drawable_draw(Drawable *drawable, GF_TraverseState *tr_state)
{
	visual_2d_texture_path(tr_state->visual, tr_state->ctx->drawable->path, tr_state->ctx, tr_state);
	visual_2d_draw_path(tr_state->visual, tr_state->ctx->drawable->path, tr_state->ctx, NULL, NULL, tr_state);
}

void call_drawable_draw(DrawableContext *ctx, GF_TraverseState *tr_state, Bool set_cyclic)
{
	if (ctx->cliper) {
		gf_node_traverse(ctx->cliper, tr_state);
	}

	if (ctx->drawable->flags & DRAWABLE_USE_TRAVERSE_DRAW) {
		if (set_cyclic)
			gf_node_allow_cyclic_traverse(ctx->drawable->node);
		gf_node_traverse(ctx->drawable->node, tr_state);
	} else {
		drawable_draw(ctx->drawable, tr_state);
	}
	if (ctx->cliper)
		gf_evg_surface_set_mask_mode(tr_state->visual->raster_surface, GF_EVGMASK_NONE);
}

/*default point_over routine*/
#ifndef GPAC_DISABLE_VRML
void vrml_drawable_pick(Drawable *drawable, GF_TraverseState *tr_state)
{
	DrawAspect2D asp;
	GF_Matrix2D inv_2d;
	StrikeInfo2D *si;
	Fixed x, y;
	u32 i, count;
	GF_Node *appear = tr_state->override_appearance ? tr_state->override_appearance : tr_state->appear;
	GF_Compositor *compositor = tr_state->visual->compositor;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		visual_3d_vrml_drawable_pick(drawable->node, tr_state, NULL, drawable);
		return;
	}
#endif
	gf_mx2d_copy(inv_2d, tr_state->transform);
	gf_mx2d_inverse(&inv_2d);
	x = tr_state->ray.orig.x;
	y = tr_state->ray.orig.y;
	gf_mx2d_apply_coords(&inv_2d, &x, &y);

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(drawable->node, &asp, tr_state);

	/*MPEG-4 picking is always on regardless of color properties*/
	if (/*tr_state->ctx->aspect.fill_texture */
	    /* (tr_state->pick_type<PICK_FULL) */
	    /* GF_COL_A(asp.fill_color)*/
	    1) {
		if (gf_path_point_over(drawable->path, x, y)) {
			goto picked;
		}
	}

	if (asp.pen_props.width || asp.line_texture ) {
		si = drawable_get_strikeinfo(tr_state->visual->compositor, drawable, &asp, appear, NULL, 0, NULL);
		if (si && si->outline && gf_path_point_over(si->outline, x, y)) {
			goto picked;
		}
	}
	return;

picked:
	compositor->hit_local_point.x = x;
	compositor->hit_local_point.y = y;
	compositor->hit_local_point.z = 0;

	gf_mx_from_mx2d(&compositor->hit_world_to_local, &tr_state->transform);
	gf_mx_from_mx2d(&compositor->hit_local_to_world, &inv_2d);

	gf_list_reset(compositor->hit_use_stack);
	compositor->hit_node = drawable->node;
	compositor->hit_use_dom_events = 0;
	compositor->hit_normal.x = compositor->hit_normal.y = 0;
	compositor->hit_normal.z = FIX_ONE;
	compositor->hit_texcoords.x = gf_divfix(x - drawable->path->bbox.x, drawable->path->bbox.width);
	compositor->hit_texcoords.y = FIX_ONE - gf_divfix(drawable->path->bbox.y - y, drawable->path->bbox.height);

#ifndef GPAC_DISABLE_VRML
	if (compositor_is_composite_texture(appear)) {
		compositor->hit_appear = appear;
	} else
#endif
	{
		compositor->hit_appear = NULL;
	}
	compositor->hit_text = NULL;

	gf_list_reset(tr_state->visual->compositor->sensors);
	count = gf_list_count(tr_state->vrml_sensors);
	for (i=0; i<count; i++) {
		gf_list_add(tr_state->visual->compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}
}
#endif /*GPAC_DISABLE_VRML*/

void drawable_init_ex(Drawable *tmp)
{
	tmp->path = gf_path_new();
	GF_SAFEALLOC(tmp->dri, DRInfo);
	if (tmp->dri) {
		/*allocate a default bounds container*/
		GF_SAFEALLOC(tmp->dri->current_bounds, BoundInfo);
	}
}

Drawable *drawable_new()
{
	Drawable *tmp;
	GF_SAFEALLOC(tmp, Drawable)
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate drawable object\n"));
		return NULL;
	}
	tmp->path = gf_path_new();
	/*allocate a default visual container*/
	GF_SAFEALLOC(tmp->dri, DRInfo);
	if (tmp->dri) {
		/*allocate a default bounds container*/
		GF_SAFEALLOC(tmp->dri->current_bounds, BoundInfo);
	}
	
	if (!tmp->dri || !tmp->dri->current_bounds) {
		if (tmp->dri) gf_free(tmp->dri);
		gf_path_del(tmp->path);
		gf_free(tmp);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate drawable object bounds\n"));
		return NULL;
	}
	return tmp;
}

void drawable_reset_bounds(Drawable *dr, GF_VisualManager *visual)
{
	DRInfo *dri;
	BoundInfo *bi, *_cur;

	dri = dr->dri;
	while (dri) {
		if (dri->visual != visual) {
			dri = dri->next;
			continue;
		}
		/*destroy previous bounds only, since current ones are always used during traversing*/
		bi = dri->previous_bounds;
		while (bi) {
			_cur = bi;
			bi = bi->next;
			gf_free(_cur);
		}
		dri->previous_bounds = NULL;
		return;
	}
}

void drawable_del_ex(Drawable *dr, GF_Compositor *compositor, Bool no_free)
{
	StrikeInfo2D *si;
	DRInfo *dri;
	BoundInfo *bi, *_cur;


	/*remove node from all visuals it's on*/
	dri = dr->dri;
	while (dri) {
		DRInfo *cur;
		Bool is_reg = compositor ? gf_sc_visual_is_registered(compositor, dri->visual) : 0;

		bi = dri->current_bounds;
		while (bi) {
			_cur = bi;
			if (is_reg && bi->clip.width) {
				ra_add(&dri->visual->to_redraw, &bi->clip);
			}
			bi = bi->next;
			gf_free(_cur);
		}
		bi = dri->previous_bounds;
		while (bi) {
			_cur = bi;
			if (is_reg && bi->clip.width) {
				ra_add(&dri->visual->to_redraw, &bi->clip);
			}
			bi = bi->next;
			gf_free(_cur);
		}
		if (is_reg) visual_2d_drawable_delete(dri->visual, dr);
		cur = dri;
		dri = dri->next;
		gf_free(cur);
	}
	if (compositor) {
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);

		/*check node isn't being tracked*/
		if (compositor->grab_node==dr->node)
			compositor->grab_node = NULL;

		if (compositor->focus_node==dr->node) {
			compositor->focus_node = NULL;
			compositor->focus_text_type = 0;
		}
		if (compositor->hit_node==dr->node) compositor->hit_node = NULL;
		if (compositor->hit_text==dr->node) compositor->hit_text = NULL;
	}

	/*remove path object*/
	if (dr->path) gf_path_del(dr->path);

#ifndef GPAC_DISABLE_3D
	if (dr->mesh) mesh_free(dr->mesh);
#endif

	si = dr->outline;
	while (si) {
		StrikeInfo2D *next = si->next;
		/*remove from main strike list*/
		if (compositor) gf_list_del_item(compositor->strike_bank, si);
		delete_strikeinfo2d(si);
		si = next;
	}
	if (!no_free)
		gf_free(dr);
}

void drawable_del(Drawable *dr)
{
	GF_Compositor *compositor = gf_sc_get_compositor(dr->node);
	drawable_del_ex(dr, compositor, GF_FALSE);
}
void drawable_node_del(GF_Node *node)
{
	drawable_del( (Drawable *)gf_node_get_private(node) );
}

Drawable *drawable_stack_new(GF_Compositor *compositor, GF_Node *node)
{
	Drawable *stack = drawable_new();
	stack->node = node;
	gf_node_set_private(node, stack);
	return stack;
}

static BoundInfo *drawable_check_alloc_bounds(struct _drawable_context *ctx, GF_VisualManager *visual)
{
	DRInfo *dri, *prev;
	BoundInfo *bi, *_prev;

	/*get bounds info for this visual manager*/
	prev = NULL;
	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->visual == visual) break;
		if (!dri->visual) {
			dri->visual = visual;
			break;
		}
		prev = dri;
		dri = dri->next;
	}
	if (!dri) {
		GF_SAFEALLOC(dri, DRInfo);
		if (!dri) return NULL;
		dri->visual = visual;
		if (prev) prev->next = dri;
		else ctx->drawable->dri = dri;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Allocating new bound info storage on visual %08x for drawable %s\n", visual, gf_node_get_class_name(ctx->drawable->node)));
	}

	/*get available bound info slot*/
	_prev = NULL;
	bi = dri->current_bounds;
	while (bi) {
		if (!bi->clip.width) break;
		_prev = bi;
		bi = bi->next;
	}
	if (!bi) {
		GF_SAFEALLOC(bi, BoundInfo);
		if (!bi) return NULL;
		if (_prev) {
//			assert(!_prev->next);
			_prev->next = bi;
		}
		else {
//			assert(!dri->current_bounds);
			dri->current_bounds = bi;
		}
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Allocating new bound info for drawable %s\n", gf_node_get_class_name(ctx->drawable->node)));
	}
	/*reset next bound info*/
	if (bi->next) bi->next->clip.width = 0;
	return bi;
}

void drawable_reset_group_highlight(GF_TraverseState *tr_state, GF_Node *n)
{
	Drawable *hlight = tr_state->visual->compositor->focus_highlight;
	if (hlight && (n == gf_node_get_private(hlight->node))) gf_node_set_private(hlight->node, NULL);
}

void drawable_mark_modified(Drawable *drawable, GF_TraverseState *tr_state)
{
	/*mark drawable as modified*/
	drawable->flags |= tr_state->visual->bounds_tracker_modif_flag;
	/*and remove overlay flag*/
	drawable->flags &= ~DRAWABLE_IS_OVERLAY;

	drawable_reset_group_highlight(tr_state, drawable->node);
}

/*move current bounds to previous bounds*/
Bool drawable_flush_bounds(Drawable *drawable, GF_VisualManager *on_visual, u32 mode2d)
{
	Bool was_drawn;
	DRInfo *dri;
	BoundInfo *tmp;

	/*reset node modified flag*/
	drawable->flags &= ~DRAWABLE_HAS_CHANGED;
	if (drawable->flags & DRAWABLE_HAS_CHANGED_IN_LAST_TRAVERSE) {
		drawable->flags |= DRAWABLE_HAS_CHANGED;
		drawable->flags &= ~DRAWABLE_HAS_CHANGED_IN_LAST_TRAVERSE;
	}

	dri = drawable->dri;
	while (dri) {
		if (dri->visual == on_visual) break;
		dri = dri->next;
	}
	if (!dri) return 0;

	was_drawn = (dri->current_bounds && dri->current_bounds->clip.width) ? 1 : 0;

	if (mode2d) {
		/*permanent direct drawing mode, destroy previous bounds*/
		if (mode2d==1) {
			if (dri->previous_bounds) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Destroying previous bounds info for drawable %s\n", gf_node_get_class_name(drawable->node)));
				while (dri->previous_bounds) {
					BoundInfo *bi = dri->previous_bounds;
					dri->previous_bounds = bi->next;
					gf_free(bi);
				}
			}
		}
	}
	/*indirect drawing, flush bounds*/
	else {
		tmp = dri->previous_bounds;
		dri->previous_bounds = dri->current_bounds;
		dri->current_bounds = tmp;
	}
	/*reset first allocated bound*/
	if (dri->current_bounds) dri->current_bounds->clip.width = 0;

	drawable->flags &= ~DRAWABLE_DRAWN_ON_VISUAL;
	return was_drawn;
}

/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/

Bool drawable_has_same_bounds(struct _drawable_context *ctx, GF_VisualManager *visual)
{
	DRInfo *dri;
	BoundInfo *bi;

	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->visual == visual) break;
		dri = dri->next;
	}
	if (!dri) return 0;

	bi = dri->previous_bounds;
	while (bi) {
		if (
		    /*if 0, end of bounds used in the previous pass*/
		    bi->clip.width
		    /*we need the same Appearance || parent <use>*/
		    && (bi->extra_check == ctx->appear)
		    /*we need exact same cliper*/
		    && (bi->clip.x==ctx->bi->clip.x) && (bi->clip.y==ctx->bi->clip.y)
		    && (bi->clip.width==ctx->bi->clip.width) && (bi->clip.height==ctx->bi->clip.height)
		    /*only check x and y (if w or h have changed, object has changed -> bounds info has been reset*/
		    && (bi->unclip.x==ctx->bi->unclip.x) && (bi->unclip.y==ctx->bi->unclip.y)
		) {
			/*remove*/
			bi->clip.width = 0;
			return 1;
		}
		bi = bi->next;
	}
	return 0;
}

/*
	return any previous bounds related to the same visual in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *drawable, GF_IRect *rc, GF_VisualManager *visual)
{
	DRInfo *dri;
	BoundInfo *bi;

	dri = drawable->dri;
	while (dri) {
		if (dri->visual == visual) break;
		dri = dri->next;
	}
	if (!dri) return 0;

	bi = dri->previous_bounds;
	while (bi) {
		if (bi->clip.width) {
			*rc = bi->clip;
			bi->clip.width = 0;
			return 1;
		}
		bi = bi->next;
	}
	return 0;
}



DrawableContext *NewDrawableContext()
{
	DrawableContext *tmp;
	GF_SAFEALLOC(tmp, DrawableContext);
	return tmp;
}
void DeleteDrawableContext(DrawableContext *ctx)
{
	drawctx_reset(ctx);
	gf_free(ctx);
}
void drawctx_reset(DrawableContext *ctx)
{
	DrawableContext *next = ctx->next;
	if (ctx->col_mat) gf_free(ctx->col_mat);
	memset(ctx, 0, sizeof(DrawableContext));
	ctx->next = next;

	/*by default all nodes are transparent*/
	ctx->flags |= CTX_IS_TRANSPARENT;

	/*BIFS has default value for 2D appearance ...*/
	ctx->aspect.fill_color = 0xFFCCCCCC;
	ctx->aspect.line_color = 0xFFCCCCCC;
	ctx->aspect.pen_props.width = FIX_ONE;
	ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	ctx->aspect.pen_props.join = GF_LINE_JOIN_BEVEL;
	ctx->aspect.pen_props.miterLimit = 4*FIX_ONE;

}

void drawctx_update_info(DrawableContext *ctx, GF_VisualManager *visual)
{
	DRInfo *dri;
	Bool moved, need_redraw, drawn;
	need_redraw = (ctx->flags & CTX_REDRAW_MASK) ? 1 : 0;

	drawn = 0;
	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->visual==visual) {
			if (dri->current_bounds && dri->current_bounds->clip.width) drawn = 1;
			break;
		}
		dri = dri->next;
	}
	if (drawn) {
		ctx->drawable->flags |= DRAWABLE_DRAWN_ON_VISUAL;
		/*node has been modified, do not check bounds, just assumed it moved*/
		if (ctx->drawable->flags & DRAWABLE_HAS_CHANGED) {
			moved = 1;
		} else {
			/*check if same bounds are used*/
			moved = !drawable_has_same_bounds(ctx, visual);
		}

		if (need_redraw || moved)
			ctx->flags |= CTX_REDRAW_MASK;
	}

	/*in all cases reset dirty flag of appearance and its sub-nodes*/
	//if (ctx->flags & CTX_HAS_APPEARANCE) gf_node_dirty_reset(ctx->appear);
}

#ifndef GPAC_DISABLE_VRML
static Bool drawable_lineprops_dirty(GF_Node *node)
{
	LinePropStack *st = (LinePropStack *)gf_node_get_private(node);
	if (!st) return 0;

	if (st->compositor->current_frame == st->last_mod_time) return st->is_dirty;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		gf_node_dirty_clear(node, 0);
		st->is_dirty = 1;
	} else {
		st->is_dirty = 0;
	}
	st->last_mod_time = st->compositor->current_frame;
	return st->is_dirty;
}

u32 drawable_get_aspect_2d_mpeg4(GF_Node *node, DrawAspect2D *asp, GF_TraverseState *tr_state)
{
	M_Material2D *m = NULL;
	M_LineProperties *LP;
	M_XLineProperties *XLP;
	GF_Node *appear = tr_state->override_appearance ? tr_state->override_appearance : tr_state->appear;
	u32 ret = 0;

	asp->pen_props.cap = GF_LINE_CAP_FLAT;
	asp->pen_props.join = GF_LINE_JOIN_MITER;
	asp->pen_props.align = GF_PATH_LINE_CENTER;
	asp->pen_props.miterLimit = 4*FIX_ONE;
	asp->line_color = 0xFFCCCCCC;
	asp->pen_props.width = 0;

	if (appear == NULL) goto check_default;

	if ( ((M_Appearance *) appear)->texture ) {
		asp->fill_texture = gf_sc_texture_get_handler( ((M_Appearance *) appear)->texture );
	}


	m = (M_Material2D *) ((M_Appearance *)appear)->material;
	if ( m == NULL) {
		asp->fill_color &= 0x00FFFFFF;
		goto check_default;
	}
	switch (gf_node_get_tag((GF_Node *) m) ) {
	case TAG_MPEG4_Material2D:
		break;
	case TAG_MPEG4_Material:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Material:
#endif
	{
		M_Material *mat = (M_Material *)m;
		asp->pen_props.width = 0;
		asp->fill_color = GF_COL_ARGB_FIXED(FIX_ONE, mat->diffuseColor.red, mat->diffuseColor.green, mat->diffuseColor.blue);
		if (!tr_state->color_mat.identity)
			asp->fill_color = gf_cmx_apply(&tr_state->color_mat, asp->fill_color);
	}
	return 0;
	default:
		return 0;
	}

	asp->fill_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, m->emissiveColor.red, m->emissiveColor.green, m->emissiveColor.blue);
	if (!tr_state->color_mat.identity)
		asp->fill_color = gf_cmx_apply(&tr_state->color_mat, asp->fill_color);

	asp->line_color = asp->fill_color;
	if (!m->filled) asp->fill_color = 0;

	if (m->lineProps == NULL) {
check_default:
		/*this is a bug in the spec: by default line width is 1.0, but in meterMetrics this means half of the screen :)*/
		asp->pen_props.width = FIX_ONE;
		if (!tr_state->pixel_metrics) asp->pen_props.width = gf_divfix(asp->pen_props.width, tr_state->min_hsize);
		if (m && m->transparency==FIX_ONE) {
			asp->pen_props.width = 0;
		} else {
			switch (gf_node_get_tag(node)) {
			case TAG_MPEG4_IndexedLineSet2D:
				asp->fill_color &= 0x00FFFFFF;
				break;
			case TAG_MPEG4_PointSet2D:
				asp->fill_color |= ((u32 )(FIX2INT(255 * (m ? (FIX_ONE - m->transparency) : FIX_ONE))) ) << 24;
				asp->pen_props.width = 0;
				break;
			default:
				if (GF_COL_A(asp->fill_color)) asp->pen_props.width = 0;
				/*spec is unclear about that*/
				//else if (!m && ctx->aspect.fill_texture) ctx->aspect.pen_props.width = 0;
				break;
			}
		}
		return 0;
	}
	LP = NULL;
	XLP = NULL;
	switch (gf_node_get_tag((GF_Node *) m->lineProps) ) {
	case TAG_MPEG4_LineProperties:
		LP = (M_LineProperties *) m->lineProps;
		break;
	case TAG_MPEG4_XLineProperties:
		XLP = (M_XLineProperties *) m->lineProps;
		break;
	default:
		asp->pen_props.width = 0;
		return 0;
	}
	if (m->lineProps && drawable_lineprops_dirty(m->lineProps))
		ret = CTX_APP_DIRTY;

	if (LP) {
		asp->pen_props.dash = (u8) LP->lineStyle;
		asp->line_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, LP->lineColor.red, LP->lineColor.green, LP->lineColor.blue);
		asp->pen_props.width = LP->width;
		if (!tr_state->color_mat.identity) {
			asp->line_color = gf_cmx_apply(&tr_state->color_mat, asp->line_color);
		}
		return ret;
	}

	asp->pen_props.dash = (u8) XLP->lineStyle;
	asp->line_color = GF_COL_ARGB_FIXED(FIX_ONE-XLP->transparency, XLP->lineColor.red, XLP->lineColor.green, XLP->lineColor.blue);
	asp->pen_props.width = XLP->width;
	if (!tr_state->color_mat.identity) {
		asp->line_color = gf_cmx_apply(&tr_state->color_mat, asp->line_color);
	}

	asp->line_scale = XLP->isScalable ? FIX_ONE : 0;
	asp->pen_props.align = XLP->isCenterAligned ? GF_PATH_LINE_CENTER : GF_PATH_LINE_INSIDE;
	asp->pen_props.cap = (u8) XLP->lineCap;
	asp->pen_props.join = (u8) XLP->lineJoin;
	asp->pen_props.miterLimit = XLP->miterLimit;
	asp->pen_props.dash_offset = XLP->dashOffset;

	/*dash settings strutc is the same as MFFloat from XLP, typecast without storing*/
	if (XLP->dashes.count) {
		asp->pen_props.dash_set = (GF_DashSettings *) &XLP->dashes;
	} else {
		asp->pen_props.dash_set = NULL;
	}
	asp->line_texture = gf_sc_texture_get_handler(XLP->texture);
	return ret;
}
#endif /*GPAC_DISABLE_VRML*/


static Bool check_transparent_skip(DrawableContext *ctx, Bool skipFill)
{
	/*if texture, cannot skip*/
	if (ctx->aspect.fill_texture) return 0;
	if (! GF_COL_A(ctx->aspect.fill_color) && !GF_COL_A(ctx->aspect.line_color) ) return 1;
	if (ctx->aspect.pen_props.width == 0) {
		if (skipFill) return 1;
		if (!GF_COL_A(ctx->aspect.fill_color) ) return 1;
	}
	return 0;
}


void drawable_check_texture_dirty(DrawableContext *ctx, Drawable *drawable, GF_TraverseState *tr_state)
{
#ifndef GPAC_DISABLE_3D
	Bool texture_ready=0;
#endif

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->aspect.fill_texture) {
		if (ctx->aspect.fill_texture->needs_refresh) {
			ctx->flags |= CTX_TEXTURE_DIRTY;
		}
#ifndef GPAC_DISABLE_3D
		//in autoGL mode, a texture is dirty only if transparent. If not, the area covered over the video doesn't need to be repainted if unchanged
		//disable for color matrix as we don't yet handle them in GL
		if (tr_state->visual->compositor->hybrid_opengl && !tr_state->visual->offscreen) {
			u8 alpha = GF_COL_A(ctx->aspect.fill_color);
			if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);

			if (!ctx->aspect.fill_texture->transparent && (alpha==0xFF) && !ctx->aspect.fill_texture->compute_gradient_matrix && (drawable->flags & DRAWABLE_HYBGL_INIT)) {
				ctx->flags |= CTX_HYBOGL_NO_CLEAR;
			}
			//otherwise, we need to redraw all object below, whether they changed ot not, because we have erased this part of the canvas
			else {
				ctx->flags |= CTX_TEXTURE_DIRTY;
			}
			//wait until we have something to draw to decide that the texture is ready, otherwise we will not clear the canvas when texture is ready
			if (ctx->aspect.fill_texture->compute_gradient_matrix || ctx->aspect.fill_texture->data)
				texture_ready=1;
		}
#endif
	}
	//same as above
	if (ctx->aspect.line_texture) {
		if (ctx->aspect.line_texture->needs_refresh)
			ctx->flags |= CTX_TEXTURE_DIRTY;

#ifndef GPAC_DISABLE_3D
		//in autoGL mode, a texture is dirty only if transparent. If not, the area covered over the video doesn't need to be repainted if unchanged
		//disable for color matrix as we don't yet handle them in GL
		if (tr_state->visual->compositor->hybrid_opengl && !tr_state->visual->offscreen) {
			u8 alpha = GF_COL_A(ctx->aspect.line_color);
			if (!ctx->aspect.line_texture->transparent && (alpha==0xFF) && !ctx->aspect.line_texture->compute_gradient_matrix && (drawable->flags & DRAWABLE_HYBGL_INIT))
				ctx->flags |= CTX_HYBOGL_NO_CLEAR;
			//otherwise, we need to redraw all object below, whether they changed ot not, bacause we have erased this part of the canvas
			else
				ctx->flags |= CTX_TEXTURE_DIRTY;

			//wait until we have something to draw to decide that the texture is ready, otherwise we will not clear the canvas when texture is ready
			if (ctx->aspect.line_texture->compute_gradient_matrix || ctx->aspect.line_texture->data)
				texture_ready=1;
		}
#endif
	}

#ifndef GPAC_DISABLE_3D
	//from now on, we won't clear the canvas when updating this texture (unless transparent, cf above)
	if (texture_ready)
		drawable->flags |= DRAWABLE_HYBGL_INIT;
#endif
}

#ifndef GPAC_DISABLE_VRML

DrawableContext *drawable_init_context_mpeg4(Drawable *drawable, GF_TraverseState *tr_state)
{
	DrawableContext *ctx;
	Bool skipFill;
	GF_Node *appear;
	assert(tr_state->visual);

	/*switched-off geometry nodes are not drawn*/
	if (tr_state->switched_off) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Drawable is switched off - skipping\n"));
		return NULL;
	}

	//Get a empty context from the current visual
	ctx = visual_2d_get_drawable_context(tr_state->visual);
	if (!ctx) return NULL;

	ctx->drawable = drawable;

	appear = tr_state->override_appearance ? tr_state->override_appearance : tr_state->appear;

	/*usually set by colorTransform or changes in OrderedGroup*/
	if (tr_state->invalidate_all)
		ctx->flags |= CTX_APP_DIRTY;

	ctx->aspect.fill_texture = NULL;
	if (appear) {
		ctx->appear = appear;
		if (gf_node_dirty_get(appear))
			ctx->flags |= CTX_APP_DIRTY;
	}
	/*todo cliper*/

	/*FIXME - only needed for texture*/
	if (!tr_state->color_mat.identity) {
		GF_SAFEALLOC(ctx->col_mat, GF_ColorMatrix);
		if (ctx->col_mat)
			gf_cmx_copy(ctx->col_mat, &tr_state->color_mat);
	}

	/*IndexedLineSet2D and PointSet2D ignores fill flag and texturing*/
	skipFill = 0;
	ctx->aspect.fill_texture = NULL;
	switch (gf_node_get_tag(ctx->drawable->node) ) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_IndexedLineSet2D:
		skipFill = 1;
		break;
#endif
	default:
		break;
	}

	ctx->flags |= drawable_get_aspect_2d_mpeg4(drawable->node, &ctx->aspect, tr_state);

	drawable_check_texture_dirty(ctx, drawable, tr_state);

	/*not clear in the spec: what happens when a transparent node is in form/layout ?? this may
	completely break layout of children. We consider the node should be drawn*/
	if (!tr_state->parent && check_transparent_skip(ctx, skipFill)) {
		visual_2d_remove_last_context(tr_state->visual);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Drawable is fully transparent - skipping\n"));
		return NULL;
	}
	ctx->flags |= CTX_HAS_APPEARANCE;

	/*we are drawing on a fliped coord surface, remember to flip the texture*/
	if (tr_state->fliped_coords)
		ctx->flags |= CTX_FLIPED_COORDS;

#ifdef GF_SR_USE_DEPTH
	ctx->depth_gain=tr_state->depth_gain;
	ctx->depth_offset=tr_state->depth_offset;
#endif

	return ctx;
}
#endif

static Bool drawable_finalize_end(struct _drawable_context *ctx, GF_TraverseState *tr_state)
{
	/*if direct draw we can remove the context*/
	Bool res = tr_state->immediate_draw ? 1 : 0;
	/*copy final transform, once all parent layout is done*/
	gf_mx2d_copy(ctx->transform, tr_state->transform);

	/*setup clipper and register bounds & sensors*/
	gf_irect_intersect(&ctx->bi->clip, &tr_state->visual->top_clipper);
	if (!ctx->bi->clip.width || !ctx->bi->clip.height) {
		ctx->bi->clip.width = 0;
		/*remove if this is the last context*/
		if (tr_state->visual->cur_context == ctx) tr_state->visual->cur_context->drawable = NULL;

		return res;
	}

	/*keep track of node drawn, whether direct or indirect drawing*/
	if (!(ctx->drawable->flags & DRAWABLE_REGISTERED_WITH_VISUAL) ) {
		struct _drawable_store *it;
		GF_SAFEALLOC(it, struct _drawable_store);
		if (!it) {
			return 0;
		}
		it->drawable = ctx->drawable;
		if (tr_state->visual->last_prev_entry) {
			tr_state->visual->last_prev_entry->next = it;
			tr_state->visual->last_prev_entry = it;
		} else {
			tr_state->visual->prev_nodes = tr_state->visual->last_prev_entry = it;
		}
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor2D] Registering new drawn node %s on visual\n", gf_node_get_class_name(it->drawable->node)));
		ctx->drawable->flags |= DRAWABLE_REGISTERED_WITH_VISUAL;
	}

	/*we are in direct draw mode, draw ...*/
	if (res) {
		/*if over an overlay we cannot remove the context and cannot draw directly*/
		if (visual_2d_overlaps_overlay(tr_state->visual, ctx, tr_state))
			return 0;

		assert(!tr_state->traversing_mode);
		tr_state->traversing_mode = TRAVERSE_DRAW_2D;
		tr_state->ctx = ctx;

		call_drawable_draw(ctx, tr_state, GF_TRUE);

		tr_state->ctx = NULL;
		tr_state->traversing_mode = TRAVERSE_SORT;
	}
	/*if the drawable is an overlay, always mark it as dirty to avoid flickering*/
	else if (ctx->drawable->flags & DRAWABLE_IS_OVERLAY) {
		ctx->flags |= CTX_APP_DIRTY;
	}
	/*if direct draw we can remove the context*/
	return res;
}

void drawable_check_bounds(struct _drawable_context *ctx, GF_VisualManager *visual)
{
	if (!ctx->bi) {
		ctx->bi = drawable_check_alloc_bounds(ctx, visual);
		assert(ctx->bi);
		ctx->bi->extra_check = ctx->appear;
	}
}

void drawable_compute_line_scale(GF_TraverseState *tr_state, DrawAspect2D *asp)
{
	GF_Rect rc;
	rc.x = rc.y = 0;
	rc.width = rc.height = FIX_ONE;
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d)
		gf_mx_apply_rect(&tr_state->model_matrix, &rc);
	else
#endif
		gf_mx2d_apply_rect(&tr_state->transform, &rc);

	asp->line_scale = MAX(gf_divfix(tr_state->visual->compositor->scale_x, rc.width), gf_divfix(tr_state->visual->compositor->scale_y, rc.height));
}

//enabled for clipPath
#define REMOVE_UNUSED_CTX

void drawable_finalize_sort_ex(DrawableContext *ctx, GF_TraverseState *tr_state, GF_Rect *orig_bounds, Bool skip_focus)
{
#ifdef REMOVE_UNUSED_CTX
	Bool can_remove = 0;
#endif
	Fixed pw;
	GF_Rect unclip, store_orig_bounds;
	GF_Node *appear = tr_state->override_appearance ? tr_state->override_appearance : tr_state->appear;

	drawable_check_bounds(ctx, tr_state->visual);

	if (orig_bounds) {
		store_orig_bounds = *orig_bounds;
	} else {
		gf_path_get_bounds(ctx->drawable->path, &store_orig_bounds);
	}
	ctx->bi->unclip = store_orig_bounds;
	gf_mx2d_apply_rect(&tr_state->transform, &ctx->bi->unclip);

	/*apply pen width*/
	if (ctx->aspect.pen_props.width) {
		StrikeInfo2D *si = NULL;

		if (!ctx->aspect.line_scale)
			drawable_compute_line_scale(tr_state, &ctx->aspect);

#if 0
		/*if pen is not scalable, apply user/viewport transform so that original aspect is kept*/
		if (!ctx->aspect.line_scale) {
			GF_Point2D pt;
			pt.x = tr_state->transform.m[0] + tr_state->transform.m[1];
			pt.y = tr_state->transform.m[3] + tr_state->transform.m[4];
			ctx->aspect.line_scale = gf_divfix(FLT2FIX(1.41421356f) , gf_v2d_len(&pt));
		}
#endif

		/*get strike info & outline for exact bounds compute. If failure use default offset*/
		si = drawable_get_strikeinfo(tr_state->visual->compositor, ctx->drawable, &ctx->aspect, appear, ctx->drawable->path, ctx->flags, NULL);
		if (si && si->outline) {
			gf_path_get_bounds(si->outline, &ctx->bi->unclip);
			gf_mx2d_apply_rect(&tr_state->transform, &ctx->bi->unclip);
		} else {
			pw = gf_mulfix(ctx->aspect.pen_props.width, ctx->aspect.line_scale);
			ctx->bi->unclip.x -= pw/2;
			ctx->bi->unclip.y += pw/2;
			ctx->bi->unclip.width += pw;
			ctx->bi->unclip.height += pw;
		}
	}

	if (ctx->bi->unclip.width && ctx->bi->unclip.height) {
		unclip = ctx->bi->unclip;
		if (! (ctx->flags & CTX_NO_ANTIALIAS)) {
			/*grow of 2 pixels (-1 and +1) to handle AA, but ONLY on cliper otherwise we will modify layout/form */
			pw = (tr_state->pixel_metrics) ? FIX_ONE : 2*FIX_ONE/tr_state->visual->width;
			unclip.x -= pw;
			unclip.y += pw;
			unclip.width += 2*pw;
			unclip.height += 2*pw;
		}
		ctx->bi->clip = gf_rect_pixelize(&unclip);
	} else {
		ctx->bi->clip.width = 0;
	}

#ifdef REMOVE_UNUSED_CTX
	can_remove = drawable_finalize_end(ctx, tr_state);
#else
	drawable_finalize_end(ctx, tr_state);
#endif
	if (ctx->drawable && !skip_focus)
		drawable_check_focus_highlight(ctx->drawable->node, tr_state, &store_orig_bounds);

	/*remove if this is the last context*/
#ifdef REMOVE_UNUSED_CTX
	if (can_remove && (tr_state->visual->cur_context == ctx))
		tr_state->visual->cur_context->drawable = NULL;
#endif
}

void drawable_finalize_sort(struct _drawable_context *ctx, GF_TraverseState *tr_state, GF_Rect *orig_bounds)
{
	drawable_finalize_sort_ex(ctx, tr_state, orig_bounds, 0);
}


void drawable_check_focus_highlight(GF_Node *node, GF_TraverseState *tr_state, GF_Rect *orig_bounds)
{
	DrawableContext *hl_ctx;
	Drawable *hlight;
	GF_Node *prev_node;
	u32 prev_mode;
	GF_Matrix2D cur;
	GF_Compositor *compositor = tr_state->visual->compositor;

	if (compositor->disable_focus_highlight) return;

	if (compositor->focus_node!=node) return;
	/*if key navigator, don't draw a focus highlight*/
	if (compositor->keynav_node) return;

	if (compositor->focus_used) {
		u32 count = gf_list_count(tr_state->use_stack);
		if (!count || (gf_list_get(tr_state->use_stack, count-1)!=compositor->focus_used) )
			return;
	}

	hlight = compositor->focus_highlight;
	if (!hlight) return;

	/*check if focus node has changed*/
	prev_node = gf_node_get_private(hlight->node);
	if (prev_node != node) {
		GF_Rect *bounds;
		/*this is a grouping node, get its bounds*/
		if (!orig_bounds) {
			gf_mx2d_copy(cur, tr_state->transform);
			gf_mx2d_init(tr_state->transform);
			prev_mode = tr_state->traversing_mode;
			tr_state->traversing_mode = TRAVERSE_GET_BOUNDS;
			tr_state->bounds.width = tr_state->bounds.height = 0;
			tr_state->bounds.x = tr_state->bounds.y = 0;

			gf_sc_get_nodes_bounds(node, ((GF_ParentNode *)node)->children, tr_state, NULL);

			tr_state->traversing_mode = prev_mode;
			gf_mx2d_copy(tr_state->transform, cur);
			bounds = &tr_state->bounds;
		} else {
			bounds = orig_bounds;
		}
		gf_node_set_private(hlight->node, node);

		drawable_reset_path(hlight);
		gf_path_reset(hlight->path);
		gf_path_add_rect(hlight->path, bounds->x, bounds->y, bounds->width, bounds->height);
	}
	hl_ctx = visual_2d_get_drawable_context(tr_state->visual);
	hl_ctx->drawable = hlight;
	hl_ctx->aspect.fill_color = compositor->hlfill;
	hl_ctx->aspect.line_color = compositor->hlline;
	hl_ctx->aspect.line_scale = 0;
	hl_ctx->aspect.pen_props.width = compositor->hllinew;
	hl_ctx->aspect.pen_props.join = GF_LINE_JOIN_BEVEL;
	hl_ctx->aspect.pen_props.dash = GF_DASH_STYLE_DOT;

	/*editing this node - move to solid stroke*/
	if (compositor->edited_text) {
		hl_ctx->aspect.pen_props.width = 2*FIX_ONE;
		hl_ctx->aspect.pen_props.dash = 1;
		hl_ctx->aspect.line_color = compositor->hlline;
	}


#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx2d_copy(hl_ctx->transform, tr_state->transform);
		visual_3d_draw_2d_with_aspect(hl_ctx->drawable, tr_state, &hl_ctx->aspect);
		return;
	}
#endif
	gf_mx2d_copy(hl_ctx->transform, tr_state->transform);
	drawable_finalize_sort_ex(hl_ctx, tr_state, NULL, 1);
}


void drawable_traverse_focus(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	if (is_destroy) return;
	if (tr_state->traversing_mode == TRAVERSE_DRAW_2D)
		visual_2d_draw_path(tr_state->visual, tr_state->ctx->drawable->path, tr_state->ctx, NULL, NULL, tr_state);
}


void delete_strikeinfo2d(StrikeInfo2D *info)
{
	if (info->outline) gf_path_del(info->outline);
#ifndef GPAC_DISABLE_3D
	if (info->mesh_outline) mesh_free(info->mesh_outline);
#endif
	gf_free(info);
}


StrikeInfo2D *drawable_get_strikeinfo(GF_Compositor *compositor, Drawable *drawable, DrawAspect2D *asp, GF_Node *appear, GF_Path *path, u32 svg_flags, GF_TraverseState *tr_state)
{
	StrikeInfo2D *si, *prev;
	GF_Node *lp;
#ifndef GPAC_DISABLE_VRML
	Bool dirty;
#endif
	if (!asp->pen_props.width) return NULL;
	if (path && !path->n_points) return NULL;

	lp = NULL;
#ifndef GPAC_DISABLE_VRML
	if (appear && (gf_node_get_tag(appear) < GF_NODE_RANGE_LAST_X3D) ) {
		lp = ((M_Appearance *)appear)->material;
		if (lp) lp = ((M_Material2D *) lp)->lineProps;
	}
#endif

	prev = NULL;
	si = drawable->outline;
	while (si) {
		/*note this includes default LP (NULL)*/
		if ((si->lineProps == lp) && (!path || (path==si->original)) ) break;
		if (!si->lineProps) {
			gf_list_del_item(compositor->strike_bank, si);
			if (si->outline) gf_path_del(si->outline);
#ifndef GPAC_DISABLE_3D
			if (si->mesh_outline) mesh_free(si->mesh_outline);
#endif
			if (prev) prev->next = si->next;
			else drawable->outline = si->next;
			gf_free(si);
			si = prev ? prev->next : drawable->outline;
			continue;
		}
		prev = si;
		si = si->next;
	}
	/*not found, add*/
	if (!si) {
		GF_SAFEALLOC(si, StrikeInfo2D);
		if (!si) {
			return NULL;
		}
		si->lineProps = lp;
		si->drawable = drawable;

		if (drawable->outline) {
			prev = drawable->outline;
			while (prev->next) prev = prev->next;
			prev->next = si;
		} else {
			drawable->outline = si;
		}
		gf_list_add(compositor->strike_bank, si);
	}

	/*3D drawing of outlines*/
#ifndef GPAC_DISABLE_3D
	if (tr_state && !asp->line_scale) {
		drawable_compute_line_scale(tr_state, asp);
	}
#endif

	/*picking*/
	if (!asp->line_scale) return si;

	/*node changed or outline not build*/
#ifndef GPAC_DISABLE_VRML
	dirty = lp ? drawable_lineprops_dirty(lp) : 0;
#endif

	if (!si->outline
#ifndef GPAC_DISABLE_VRML
		|| dirty
#endif
		|| (si->line_scale != asp->line_scale) || (si->path_length != asp->pen_props.path_length) || (svg_flags & CTX_SVG_OUTLINE_GEOMETRY_DIRTY)) {
		u32 i;
		Fixed w = asp->pen_props.width;
		Fixed dash_o = asp->pen_props.dash_offset;
		si->line_scale = asp->line_scale;
		if (si->outline) gf_path_del(si->outline);
#ifndef GPAC_DISABLE_3D
		if (si->mesh_outline) {
			mesh_free(si->mesh_outline);
			si->mesh_outline = NULL;
		}
#endif
		/*apply scale whether scalable or not (if not scalable, scale is still needed for scalable zoom)*/
		asp->pen_props.width = gf_mulfix(asp->pen_props.width, asp->line_scale);
		if (asp->pen_props.dash != GF_DASH_STYLE_SVG)
			asp->pen_props.dash_offset = gf_mulfix(asp->pen_props.dash_offset, asp->pen_props.width);

		if (asp->pen_props.dash_set) {
			for(i=0; i<asp->pen_props.dash_set->num_dash; i++) {
				asp->pen_props.dash_set->dashes[i] = gf_mulfix(asp->pen_props.dash_set->dashes[i], asp->line_scale);
			}
		}

		if (path) {
			si->outline = gf_path_get_outline(path, asp->pen_props);
			si->original = path;
		} else {
			si->outline = gf_path_get_outline(drawable->path, asp->pen_props);
		}
		/*restore*/
		asp->pen_props.width = w;
		asp->pen_props.dash_offset = dash_o;
		if (asp->pen_props.dash_set) {
			for(i=0; i<asp->pen_props.dash_set->num_dash; i++) {
				asp->pen_props.dash_set->dashes[i] = gf_divfix(asp->pen_props.dash_set->dashes[i], asp->line_scale);
			}
		}
	}

	return si;
}

void drawable_reset_path_outline(Drawable *st)
{
	StrikeInfo2D *si = st->outline;
	while (si) {
		if (si->outline) gf_path_del(si->outline);
		si->outline = NULL;
#ifndef GPAC_DISABLE_3D
		if (si->mesh_outline) mesh_free(si->mesh_outline);
		si->mesh_outline = NULL;
#endif
		si->original = NULL;
		si = si->next;
	}
#ifndef GPAC_DISABLE_3D
	if (st->mesh) {
		mesh_free(st->mesh);
		st->mesh = NULL;
	}
#endif
}

void drawable_reset_path(Drawable *st)
{
	drawable_reset_path_outline(st);
	if (st->path) gf_path_reset(st->path);
#ifndef GPAC_DISABLE_3D
	if (st->mesh) {
		mesh_free(st->mesh);
		st->mesh = NULL;
	}
#endif
}

#ifndef GPAC_DISABLE_VRML

static void DestroyLineProps(GF_Node *n, void *rs, Bool is_destroy)
{
	StrikeInfo2D *si, *cur, *prev;
	u32 i;
	LinePropStack *st;
	if (!is_destroy) return;

	st = (LinePropStack *)gf_node_get_private(n);
	i = 0;

	while ((si = (StrikeInfo2D*)gf_list_enum(st->compositor->strike_bank, &i))) {
		if (si->lineProps == n) {
			/*remove from node*/
			if (si->drawable) {
				assert(si->drawable->outline);
				cur = si->drawable->outline;
				prev = NULL;
				while (cur) {
					if (cur!=si) {
						prev = cur;
						cur = cur->next;
						continue;
					}
					if (prev) prev->next = cur->next;
					else si->drawable->outline = cur->next;
					break;
				}
			}
			i--;
			gf_list_rem(st->compositor->strike_bank, i);
			delete_strikeinfo2d(si);
		}
	}

	gf_free(st);

}

void compositor_init_lineprops(GF_Compositor *compositor, GF_Node *node)
{
	LinePropStack *st;
	GF_SAFEALLOC(st, LinePropStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate line properties stack\n"));
		return;
	}
	st->compositor = compositor;
	st->last_mod_time = 0;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyLineProps);
}

#endif /*GPAC_DISABLE_VRML*/


#ifdef GPAC_DISABLE_SVG

Bool drawable_get_aspect_2d_svg(GF_Node *node, DrawAspect2D *asp, GF_TraverseState *tr_state)
{
	return 0;
}

#else

Bool drawable_get_aspect_2d_svg(GF_Node *node, DrawAspect2D *asp, GF_TraverseState *tr_state)
{
	Bool ret = 0;
	SVGPropertiesPointers *props = tr_state->svg_props;
	Fixed clamped_opacity = FIX_ONE;
	Fixed clamped_solid_opacity = FIX_ONE;
	Fixed clamped_fill_opacity = (props->fill_opacity->value < 0 ? 0 : (props->fill_opacity->value > FIX_ONE ? FIX_ONE : props->fill_opacity->value));
	Fixed clamped_stroke_opacity = (props->stroke_opacity->value < 0 ? 0 : (props->stroke_opacity->value > FIX_ONE ? FIX_ONE : props->stroke_opacity->value));

	if (props->opacity) {
		clamped_opacity = (props->opacity->value < 0 ? 0 : (props->opacity->value > FIX_ONE ? FIX_ONE : props->opacity->value));
		if (clamped_opacity!=FIX_ONE) {
			clamped_fill_opacity = gf_mulfix(clamped_fill_opacity, clamped_opacity);
			clamped_stroke_opacity = gf_mulfix(clamped_stroke_opacity, clamped_opacity);
		}
	}
	asp->fill_color = 0;

	if (props->fill->type==SVG_PAINT_URI) {
		if (props->fill->iri.type != XMLRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			XMLRI *iri = &props->fill->iri;
			GF_SceneGraph *sg = gf_node_get_graph(node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->string[1]));
			if (n) {
				iri->type = XMLRI_ELEMENTID;
				iri->target = n;
				gf_node_register_iri(sg, iri);
				gf_free(iri->string);
				iri->string = NULL;
			}
		}
		/* If paint server not found, paint is equivalent to none */
		if (props->fill->iri.type == XMLRI_ELEMENTID) {
			asp->fill_color = GF_COL_ARGB_FIXED(clamped_opacity, 0, 0, 0);
			switch (gf_node_get_tag((GF_Node *)props->fill->iri.target)) {
			case TAG_SVG_solidColor:
			{
				SVGAllAttributes all_atts;
				gf_svg_flatten_attributes((SVG_Element*)props->fill->iri.target, &all_atts);

				gf_node_traverse(props->fill->iri.target, tr_state);

				ret += compositor_svg_solid_color_dirty(tr_state->visual->compositor, props->fill->iri.target);

				if (all_atts.solid_color) {
					if (all_atts.solid_opacity) {
						Fixed val = all_atts.solid_opacity->value;
						clamped_solid_opacity = MIN(FIX_ONE, MAX(0, val) );
						clamped_solid_opacity = gf_mulfix(clamped_solid_opacity, clamped_opacity);
					}
					asp->fill_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, all_atts.solid_color->color.red, all_atts.solid_color->color.green, all_atts.solid_color->color.blue);
				}
			}
			break;
			case TAG_SVG_linearGradient:
			case TAG_SVG_radialGradient:
				asp->fill_texture = gf_sc_texture_get_handler((GF_Node *)props->fill->iri.target);
				break;
			/*FIXME*/
			default:
				break;
			}
		}
	} else if (props->fill->type == SVG_PAINT_COLOR) {
		if (props->fill->color.type == SVG_COLOR_CURRENTCOLOR) {
			asp->fill_color = GF_COL_ARGB_FIXED(clamped_fill_opacity, props->color->color.red, props->color->color.green, props->color->color.blue);
		} else if (props->fill->color.type == SVG_COLOR_RGBCOLOR) {
			asp->fill_color = GF_COL_ARGB_FIXED(clamped_fill_opacity, props->fill->color.red, props->fill->color.green, props->fill->color.blue);
		} else if (props->fill->color.type >= SVG_COLOR_ACTIVE_BORDER) {
			asp->fill_color = tr_state->visual->compositor->sys_colors[props->fill->color.type - 3];
			asp->fill_color |= ((u32) (clamped_fill_opacity*255) ) << 24;
		}
	}
	if (!tr_state->color_mat.identity)
		asp->fill_color = gf_cmx_apply(&tr_state->color_mat, asp->fill_color);

	asp->line_color = 0;
	asp->pen_props.width = (props->stroke->type != SVG_PAINT_NONE) ? props->stroke_width->value : 0;
	if (props->stroke->type==SVG_PAINT_URI) {
		if (props->stroke->iri.type != XMLRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			XMLRI *iri = &props->stroke->iri;
			GF_SceneGraph *sg = gf_node_get_graph(node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->string[1]));
			if (n) {
				iri->type = XMLRI_ELEMENTID;
				iri->target = n;
				gf_node_register_iri(sg, iri);
				gf_free(iri->string);
				iri->string = NULL;
			}
		}
		/* Paint server not found, stroke is equivalent to none */
		if ((props->stroke->iri.type == XMLRI_ELEMENTID) && props->stroke->iri.target) {
			switch (gf_node_get_tag((GF_Node *)props->stroke->iri.target)) {
			case TAG_SVG_solidColor:
			{
				SVGAllAttributes all_atts;
				gf_svg_flatten_attributes((SVG_Element*)props->stroke->iri.target, &all_atts);

				gf_node_traverse(props->stroke->iri.target, tr_state);

				ret += compositor_svg_solid_color_dirty(tr_state->visual->compositor, props->stroke->iri.target);

				if (all_atts.solid_color) {
					if (all_atts.solid_opacity) {
						Fixed val = all_atts.solid_opacity->value;
						clamped_solid_opacity = MIN(FIX_ONE, MAX(0, val) );
					}
					asp->line_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, all_atts.solid_color->color.red, all_atts.solid_color->color.green, all_atts.solid_color->color.blue);
				}
			}
			break;
			case TAG_SVG_linearGradient:
			case TAG_SVG_radialGradient:
				asp->line_texture = gf_sc_texture_get_handler((GF_Node *)props->stroke->iri.target);
				break;
			default:
				break;
			}
		}
	} else if (props->stroke->type == SVG_PAINT_COLOR) {
		if (props->stroke->color.type == SVG_COLOR_CURRENTCOLOR) {
			asp->line_color = GF_COL_ARGB_FIXED(clamped_stroke_opacity, props->color->color.red, props->color->color.green, props->color->color.blue);
		} else if (props->stroke->color.type == SVG_COLOR_RGBCOLOR) {
			asp->line_color = GF_COL_ARGB_FIXED(clamped_stroke_opacity, props->stroke->color.red, props->stroke->color.green, props->stroke->color.blue);
		} else if (props->stroke->color.type >= SVG_COLOR_ACTIVE_BORDER) {
			asp->line_color = tr_state->visual->compositor->sys_colors[SVG_COLOR_ACTIVE_BORDER - 3];
			asp->line_color |= ((u32) (clamped_stroke_opacity*255)) << 24;
		}
	}
	if (!tr_state->color_mat.identity)
		asp->line_color = gf_cmx_apply(&tr_state->color_mat, asp->line_color);

	if (props->stroke_dasharray->type != SVG_STROKEDASHARRAY_NONE) {
		asp->pen_props.dash = GF_DASH_STYLE_SVG;
		asp->pen_props.dash_offset = props->stroke_dashoffset->value;
		asp->pen_props.dash_set = (GF_DashSettings *) &(props->stroke_dasharray->array);
	}
	asp->line_scale = (props->vector_effect && (*props->vector_effect == SVG_VECTOREFFECT_NONSCALINGSTROKE)) ? 0 : FIX_ONE;

	asp->pen_props.cap = (u8) *props->stroke_linecap;
	asp->pen_props.join = (u8) *props->stroke_linejoin;
	asp->pen_props.miterLimit = props->stroke_miterlimit->value;

	if (!tr_state->color_mat.identity) {
		asp->fill_color = gf_cmx_apply(&tr_state->color_mat, asp->fill_color);
		asp->line_color = gf_cmx_apply(&tr_state->color_mat, asp->line_color);
	}
	return ret;
}

static Bool svg_appearance_flag_dirty(u32 flags)
{
#if 1
	/* fill-related */
	if (flags & GF_SG_SVG_FILL_DIRTY)				return 1;
	if (flags & GF_SG_SVG_FILLOPACITY_DIRTY)		return 1;
	if (flags & GF_SG_SVG_FILLRULE_DIRTY)			return 1;

	/* stroke-related */
	if (flags & GF_SG_SVG_STROKE_DIRTY)				return 1;
	if (flags & GF_SG_SVG_STROKEDASHARRAY_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKEDASHOFFSET_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKELINECAP_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKELINEJOIN_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKEMITERLIMIT_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKEOPACITY_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKEWIDTH_DIRTY)		return 1;
	if (flags & GF_SG_SVG_VECTOREFFECT_DIRTY)		return 1;

	/* gradients stops and solidcolor do not affect appearance directly */
	return 0;
#else
	if (flags &
	        (GF_SG_SVG_FILL_DIRTY | GF_SG_SVG_FILLOPACITY_DIRTY | GF_SG_SVG_FILLRULE_DIRTY
	         | GF_SG_SVG_STROKE_DIRTY | GF_SG_SVG_STROKEDASHARRAY_DIRTY
	         | GF_SG_SVG_STROKEDASHOFFSET_DIRTY | GF_SG_SVG_STROKELINECAP_DIRTY
	         | GF_SG_SVG_STROKELINEJOIN_DIRTY | GF_SG_SVG_STROKEMITERLIMIT_DIRTY
	         | GF_SG_SVG_STROKEOPACITY_DIRTY | GF_SG_SVG_STROKEWIDTH_DIRTY
	         | GF_SG_SVG_VECTOREFFECT_DIRTY) )
		return 1;
	return 0;
#endif
}

DrawableContext *drawable_init_context_svg(Drawable *drawable, GF_TraverseState *tr_state, SVG_ClipPath *clip_path)
{
	DrawableContext *ctx;
	assert(tr_state->visual);

	if (clip_path && !clip_path->target.target) {
		clip_path->target.target = clip_path->target.string ? gf_sg_find_node_by_name(tr_state->visual->compositor->scene, clip_path->target.string+1) : NULL;
		if (!clip_path->target.target) clip_path = NULL;
	}

#ifndef GPAC_DISABLE_VRML
	/*setup SVG based on override appearance node */
	if (tr_state->override_appearance) {
		ctx = drawable_init_context_mpeg4(drawable, tr_state);
		if (ctx && clip_path) ctx->cliper = (GF_Node*)clip_path->target.target;
		return ctx;
	}
#endif

	/*switched-off geometry nodes are not drawn*/
	if (tr_state->switched_off) return NULL;

	//Get a empty context from the current visual
	ctx = visual_2d_get_drawable_context(tr_state->visual);
	if (!ctx) return NULL;

	gf_mx2d_copy(ctx->transform, tr_state->transform);

	ctx->drawable = drawable;
	if (clip_path) ctx->cliper = (GF_Node*)clip_path->target.target;

	if (tr_state->invalidate_all || svg_appearance_flag_dirty(tr_state->svg_flags)) {
		ctx->flags |= CTX_APP_DIRTY;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("Node %s dirty - invalidating\n", gf_node_get_log_name(drawable->node) ));
	}
	if (tr_state->svg_flags & (GF_SG_SVG_STROKEDASHARRAY_DIRTY |
	                           GF_SG_SVG_STROKEDASHOFFSET_DIRTY |
	                           GF_SG_SVG_STROKELINECAP_DIRTY |
	                           GF_SG_SVG_STROKELINEJOIN_DIRTY |
	                           GF_SG_SVG_STROKEMITERLIMIT_DIRTY |
	                           GF_SG_SVG_STROKEWIDTH_DIRTY |
	                           GF_SG_SVG_VECTOREFFECT_DIRTY ))
		ctx->flags |= CTX_SVG_OUTLINE_GEOMETRY_DIRTY;

	ctx->aspect.fill_texture = NULL;

	/*FIXME - only needed for texture*/
	if (!tr_state->color_mat.identity) {
		GF_SAFEALLOC(ctx->col_mat, GF_ColorMatrix);
		if (ctx->col_mat)
			gf_cmx_copy(ctx->col_mat, &tr_state->color_mat);
	}

	switch (gf_node_get_tag(ctx->drawable->node) ) {
	case TAG_SVG_image:
	case TAG_SVG_video:
		ctx->aspect.fill_texture = gf_sc_texture_get_handler(ctx->drawable->node);
		break;
	case TAG_SVG_line:
	case TAG_SVG_polyline:
		break;
	default:
		break;
	}

	if (drawable_get_aspect_2d_svg(drawable->node, &ctx->aspect, tr_state))
		ctx->flags |= CTX_APP_DIRTY;

	if (ctx->drawable->path) {
		if (*tr_state->svg_props->fill_rule == SVG_FILLRULE_NONZERO) {
			ctx->drawable->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
		} else {
			ctx->drawable->path->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
		}
	}

	drawable_check_texture_dirty(ctx, drawable, tr_state);

	/*we are drawing on a centered coord surface, remember to flip the texture*/
	if (tr_state->fliped_coords)
		ctx->flags |= CTX_FLIPED_COORDS;


#ifdef GF_SR_USE_DEPTH
	ctx->depth_gain=tr_state->depth_gain;
	ctx->depth_offset=tr_state->depth_offset;
#endif

	return ctx;
}



#endif	//SVG
