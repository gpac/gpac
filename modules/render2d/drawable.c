/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 2D rendering module
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
#include "visualsurface2d.h"
#include "stacks2d.h"
#ifndef GPAC_DISABLE_SVG
#include "svg_stacks.h"
#endif


/*default rendering routine*/
void drawable_draw(RenderEffect2D *eff) 
{
	VS2D_TexturePath(eff->surface, eff->ctx->drawable->path, eff->ctx);
	VS2D_DrawPath(eff->surface, eff->ctx->drawable->path, eff->ctx, NULL, NULL);
}

/*default point_over routine*/
void drawable_pick(RenderEffect2D *eff)
{
	GF_Matrix2D inv;
	StrikeInfo2D *si;
	Fixed x, y;
	if (!eff->ctx || !eff->ctx->drawable->path) return;

	assert(eff->surface);
	gf_mx2d_copy(inv, eff->ctx->transform);
	gf_mx2d_inverse(&inv);
	x = eff->x;
	y = eff->y;

	gf_mx2d_apply_coords(&inv, &x, &y);
	if (eff->ctx->h_texture || (eff->pick_type<PICK_FULL) || GF_COL_A(eff->ctx->aspect.fill_color) ) {
		if (gf_path_point_over(eff->ctx->drawable->path, x, y)) {
			eff->is_over = 1;
			return;
		}
	}

	if (eff->ctx->aspect.pen_props.width || eff->ctx->aspect.line_texture || (eff->pick_type!=PICK_PATH)) {
		si = drawctx_get_strikeinfo(eff->ctx, NULL);
		if (si && si->outline && gf_path_point_over(si->outline, x, y)) {
			eff->is_over = 1;
		}
	}
}

Drawable *drawable_new()
{
	Drawable *tmp;
	GF_SAFEALLOC(tmp, Drawable)
	tmp->path = gf_path_new();
	/*allocate a default surface container*/
	GF_SAFEALLOC(tmp->dri, DRInfo);
	/*allocate a default bounds container*/
	GF_SAFEALLOC(tmp->dri->current_bounds, BoundInfo);
	return tmp;
}

void drawable_reset_bounds(Drawable *dr, VisualSurface2D *surf)
{
	DRInfo *dri;
	BoundInfo *bi, *_cur;

	dri = dr->dri;
	while (dri) {
		if (dri->surface != surf) {
			dri = dri->next;
			continue;
		}
		/*destroy previous bounds only, since current ones are always used during traversing*/
		bi = dri->previous_bounds;
		while (bi) {
			_cur = bi;
			bi = bi->next;
			free(_cur);
		}
		dri->previous_bounds = NULL;
		return;
	}
}

void drawable_del(Drawable *dr)
{
	StrikeInfo2D *si;
	Bool is_reg = 0;
	DRInfo *dri, *cur;
	BoundInfo *bi, *_cur;
	Render2D *r2d = gf_sr_get_renderer(dr->node) ? gf_sr_get_renderer(dr->node)->visual_renderer->user_priv : NULL; 

	/*remove node from all surfaces it's on*/
	dri = dr->dri;
	while (dri) {
		is_reg = R2D_IsSurfaceRegistered(r2d, dri->surface);
	
		bi = dri->current_bounds;
		while (bi) {
			_cur = bi;
			if (is_reg) ra_add(&dri->surface->to_redraw, &bi->clip);
			bi = bi->next;
			free(_cur);
		}
		bi = dri->previous_bounds;
		while (bi) {
			_cur = bi;
			if (is_reg) ra_add(&dri->surface->to_redraw, &bi->clip);
			bi = bi->next;
			free(_cur);
		}
		if (is_reg) VS2D_DrawableDeleted(dri->surface, dr);
		cur = dri;
		dri = dri->next;
		free(cur);
	}
	r2d->compositor->draw_next_frame = 1;

	/*remove path object*/
	if (dr->path) gf_path_del(dr->path);

	si = dr->outline;
	while (si) {
		StrikeInfo2D *next = si->next;
		/*remove from main strike list*/
		if (r2d) gf_list_del_item(r2d->strike_bank, si);
		delete_strikeinfo2d(si);
		si = next;
	}
	free(dr);
}

void DestroyDrawableNode(GF_Node *node)
{
	drawable_del( (Drawable *)gf_node_get_private(node) );
}

Drawable *drawable_stack_new(Render2D *sr, GF_Node *node)
{
	Drawable *stack = drawable_new();
	stack->node = node;
	gf_node_set_private(node, stack);
	return stack;
}

static BoundInfo *drawable_check_alloc_bounds(struct _drawable_context *ctx, VisualSurface2D *surf)
{
	DRInfo *dri, *prev;
	BoundInfo *bi, *_prev;

	/*get bounds info for this surface*/
	prev = NULL;
	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->surface == surf) break;
		if (!dri->surface) {
			dri->surface = surf;
			break;
		}
		prev = dri;
		dri = dri->next;
	}
	if (!dri) {
		GF_SAFEALLOC(dri, DRInfo);
		dri->surface = surf;
		if (prev) prev->next = dri;
		else ctx->drawable->dri = dri;
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
		if (_prev) {
			assert(!_prev->next);
			_prev->next = bi;
		}
		else {
			assert(!dri->current_bounds);
			dri->current_bounds = bi;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Allocating new bound info for drawable %s\n", gf_node_get_class_name(ctx->drawable->node)));
	}
	/*reset next bound info*/
	if (bi->next) bi->next->clip.width = 0;
	return bi;
}

/*move current bounds to previous bounds*/
Bool drawable_flush_bounds(Drawable *drawable, struct _visual_surface_2D *on_surface, u32 render_mode)
{
	Bool was_drawn;
	DRInfo *dri;
	BoundInfo *tmp;

	/*reset node modified flag*/
	drawable->flags &= ~DRAWABLE_HAS_CHANGED;

	dri = drawable->dri;
	while (dri) {
		if (dri->surface == on_surface) break;
		dri = dri->next;
	}
	if (!dri) return 0;

	was_drawn = (dri->current_bounds && dri->current_bounds->clip.width) ? 1 : 0;

	if (render_mode) {
		/*permanent direct rendering, destroy previous bounds*/
		if (render_mode==1) {
			if (dri->previous_bounds) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render 2D] Destroying previous bounds info for drawable %s\n", gf_node_get_class_name(drawable->node)));
				while (dri->previous_bounds) {
					BoundInfo *bi = dri->previous_bounds;
					dri->previous_bounds = bi->next;
					free(bi);
				}
			}
		}
	}
	/*indirect rendering, flush bounds*/
	else {
		tmp = dri->previous_bounds;
		dri->previous_bounds = dri->current_bounds;
		dri->current_bounds = tmp;
	}
	/*reset first allocated bound*/
	if (dri->current_bounds) dri->current_bounds->clip.width = 0;

	drawable->flags &= ~DRAWABLE_DRAWN_ON_SURFACE;
	return was_drawn;
}	

/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/

Bool drawable_has_same_bounds(struct _drawable_context *ctx, struct _visual_surface_2D *surf)
{
	DRInfo *dri;
	BoundInfo *bi;
	
	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->surface == surf) break;
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
	return any previous bounds related to the same surface in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *drawable, GF_IRect *rc, struct _visual_surface_2D *surf)
{
	DRInfo *dri;
	BoundInfo *bi;

	dri = drawable->dri;
	while (dri) {
		if (dri->surface == surf) break;
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
	free(ctx);
}
void drawctx_reset(DrawableContext *ctx)
{
	DrawableContext *next = ctx->next;
	drawctx_reset_sensors(ctx);
	if (ctx->col_mat) free(ctx->col_mat);
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

void drawctx_reset_sensors(DrawableContext *ctx)
{
	SensorContext *sc = ctx->sensor;
	while (sc) {
		SensorContext *cur = sc;
		sc = sc->next;
		free(cur);
	}
	ctx->sensor = NULL;
}
static void drawctx_add_sensor(DrawableContext *ctx, SensorContext *handler)
{
	SensorContext *sc = (SensorContext *)malloc(sizeof(SensorContext));
	sc->h_node = handler->h_node;
	sc->next = NULL;
	gf_mx2d_copy(sc->matrix, handler->matrix);
	if (!ctx->sensor) {
		ctx->sensor = sc;
	} else {
		SensorContext *cur = ctx->sensor;
		while (cur->next) cur = cur->next;
		cur->next = sc;
	}
}

void drawctx_update_info(DrawableContext *ctx, struct _visual_surface_2D *surf)
{
	DRInfo *dri;
	Bool moved, need_redraw, drawn;
	need_redraw = (ctx->flags & CTX_REDRAW_MASK) ? 1 : 0;

	drawn = 0;
	dri = ctx->drawable->dri;
	while (dri) {
		if (dri->surface==surf) {
			if (dri->current_bounds && dri->current_bounds->clip.width) drawn = 1;
			break;
		}
		dri = dri->next;
	}
	if (drawn) {
		ctx->drawable->flags |= DRAWABLE_DRAWN_ON_SURFACE; 
		/*node has been modified, do not check bounds, just assumed it moved*/
		if (ctx->drawable->flags & DRAWABLE_HAS_CHANGED) {
			moved = 1;
		} else {
			/*check if same bounds are used*/
			moved = !drawable_has_same_bounds(ctx, surf);
		}
		if (need_redraw || moved) ctx->flags |= CTX_REDRAW_MASK;
	}

	/*in all cases reset dirty flag of appearance and its sub-nodes*/
	if (ctx->flags & CTX_HAS_APPEARANCE)
		gf_node_dirty_reset(ctx->appear);
}


static void setup_drawable_context(DrawableContext *ctx, RenderEffect2D *eff)
{
	M_Material2D *m = NULL;
	M_LineProperties *LP;
	M_XLineProperties *XLP;

	if (ctx->appear == NULL) goto check_default;
	m = (M_Material2D *) ((M_Appearance *)ctx->appear)->material;
	if ( m == NULL) {
		ctx->aspect.fill_color &= 0x00FFFFFF;
		goto check_default;
	}
	if (gf_node_get_tag((GF_Node *) m) != TAG_MPEG4_Material2D) return;

	ctx->aspect.fill_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, m->emissiveColor.red, m->emissiveColor.green, m->emissiveColor.blue);
	if (ctx->col_mat) 
		ctx->aspect.fill_color = gf_cmx_apply(ctx->col_mat, ctx->aspect.fill_color);

	ctx->aspect.line_color = ctx->aspect.fill_color;
	if (!m->filled) ctx->aspect.fill_color &= 0x00FFFFFF;

	ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	ctx->aspect.pen_props.join = GF_LINE_JOIN_MITER;

	if (m->lineProps == NULL) {
check_default:
		/*this is a bug in the spec: by default line width is 1.0, but in meterMetrics this means half of the screen :)*/
		ctx->aspect.pen_props.width = FIX_ONE;
		if (!eff->is_pixel_metrics) ctx->aspect.pen_props.width = gf_divfix(ctx->aspect.pen_props.width, eff->min_hsize);
		if (m && m->transparency==FIX_ONE) {
			ctx->aspect.pen_props.width = 0;
		} else {
			switch (gf_node_get_tag(ctx->drawable->node)) {
			case TAG_MPEG4_IndexedLineSet2D:
				ctx->aspect.fill_color &= 0x00FFFFFF;
				break;
			case TAG_MPEG4_PointSet2D:
				ctx->aspect.fill_color |= FIX2INT(255 * (m ? (FIX_ONE - m->transparency) : FIX_ONE)) << 24;
				ctx->aspect.pen_props.width = 0;
				break;
			default:
				if (GF_COL_A(ctx->aspect.fill_color)) ctx->aspect.pen_props.width = 0;
				/*spec is unclear about that*/
				else if (!m && ctx->h_texture) ctx->aspect.pen_props.width = 0;
				break;
			}
		}
		return;
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
		ctx->aspect.pen_props.width = 0;
		return;
	}
	if (m->lineProps && gf_node_dirty_get(m->lineProps)) ctx->flags |= CTX_APP_DIRTY;

	if (LP) {
		ctx->aspect.pen_props.dash = (u8) LP->lineStyle;
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, LP->lineColor.red, LP->lineColor.green, LP->lineColor.blue);
		ctx->aspect.pen_props.width = LP->width;
		if (ctx->col_mat) {
			ctx->aspect.line_color = gf_cmx_apply(ctx->col_mat, ctx->aspect.line_color);
		}
		return;
	} 

	ctx->aspect.pen_props.dash = (u8) XLP->lineStyle;
	ctx->aspect.line_color = GF_COL_ARGB_FIXED(FIX_ONE-XLP->transparency, XLP->lineColor.red, XLP->lineColor.green, XLP->lineColor.blue);
	ctx->aspect.pen_props.width = XLP->width;
	if (ctx->col_mat) {
		ctx->aspect.line_color = gf_cmx_apply(ctx->col_mat, ctx->aspect.line_color);
	}
	
	ctx->aspect.line_scale = XLP->isScalable ? FIX_ONE : 0;
	ctx->aspect.pen_props.align = XLP->isCenterAligned ? GF_PATH_LINE_CENTER : GF_PATH_LINE_INSIDE;
	ctx->aspect.pen_props.cap = (u8) XLP->lineCap;
	ctx->aspect.pen_props.join = (u8) XLP->lineJoin;
	ctx->aspect.pen_props.miterLimit = XLP->miterLimit;
	ctx->aspect.pen_props.dash_offset = XLP->dashOffset;

	/*dash settings strutc is the same as MFFloat from XLP, typecast without storing*/
	if (XLP->dashes.count) {
		ctx->aspect.pen_props.dash_set = (GF_DashSettings *) &XLP->dashes;
	} else {
		ctx->aspect.pen_props.dash_set = NULL;
	}
	ctx->aspect.line_texture = R2D_GetTextureHandler(XLP->texture);
}

static Bool check_transparent_skip(DrawableContext *ctx, Bool skipFill)
{
	/*if sensor cannot skip*/
	if (ctx->sensor) return 0;
	/*if texture, cannot skip*/
	if (ctx->h_texture) return 0;
	if (! GF_COL_A(ctx->aspect.fill_color) && !GF_COL_A(ctx->aspect.line_color) ) return 1;
	if (ctx->aspect.pen_props.width == 0) {
		if (skipFill) return 1;
		if (!GF_COL_A(ctx->aspect.fill_color) ) return 1;
	}
	return 0;
}


GF_TextureHandler *drawable_get_texture(RenderEffect2D *eff)
{
	M_Appearance *appear = (M_Appearance *) eff->appear;
	if (!appear || !appear->texture) return NULL;
	return R2D_GetTextureHandler(appear->texture);
}

DrawableContext *drawable_init_context(Drawable *drawable, RenderEffect2D *eff)
{
	DrawableContext *ctx;
	u32 i, count;
	Bool skipFill;
	assert(eff->surface);

	/*switched-off geometry nodes are not rendered*/
	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] Drawable is switched off - skipping\n"));
		return NULL;
	}

	//Get a empty context from the current surface
	ctx = VS2D_GetDrawableContext(eff->surface);

	gf_mx2d_copy(ctx->transform, eff->transform);
	ctx->drawable = drawable;

	/*usually set by colorTransform or changes in OrderedGroup*/
	if (eff->invalidate_all) ctx->flags |= CTX_APP_DIRTY;

	ctx->h_texture = NULL;
	if (eff->appear) {
		ctx->appear = eff->appear;
		if (gf_node_dirty_get(eff->appear)) ctx->flags |= CTX_APP_DIRTY;
	}
#ifndef FIXME
	/*todo cliper*/
#else
	else {
		VS2D_RemoveLastContext(eff->surface);
		return NULL;
	}
#endif

	/*FIXME - only needed for texture*/
	if (!eff->color_mat.identity) {
		GF_SAFEALLOC(ctx->col_mat, GF_ColorMatrix);
		gf_cmx_copy(ctx->col_mat, &eff->color_mat);
	}

	/*IndexedLineSet2D and PointSet2D ignores fill flag and texturing*/
	skipFill = 0;
	ctx->h_texture = NULL;
	switch (gf_node_get_tag(ctx->drawable->node) ) {
	case TAG_MPEG4_IndexedLineSet2D: 
		skipFill = 1;
		break;
	default:
		ctx->h_texture = drawable_get_texture(eff);
		break;
	}
	

	/*setup sensors*/
	count = gf_list_count(eff->sensors);
	for (i=0; i<count; i++) 
		drawctx_add_sensor(ctx, (SensorContext*)gf_list_get(eff->sensors, i));

	setup_drawable_context(ctx, eff);

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->h_texture && ctx->h_texture->needs_refresh) ctx->flags |= CTX_TEXTURE_DIRTY;

	/*not clear in the spec: what happens when a transparent node is in form/layout ?? this may 
	completely break layout of children. We consider the node should be drawn*/
	if (!eff->parent && check_transparent_skip(ctx, skipFill)) {
		VS2D_RemoveLastContext(eff->surface);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Render2D] Drawable is fully transparent - skipping\n"));
		return NULL;
	}
	ctx->flags |= CTX_HAS_APPEARANCE;
	//setup clipper if needed
	return ctx;
}

void drawable_finalize_end(struct _drawable_context *ctx, RenderEffect2D *eff)
{
	if (eff->parent) {
		group2d_add_to_context_list(eff->parent, ctx);
	} else {
		/*setup clipper and register bounds & sensors*/
		gf_irect_intersect(&ctx->bi->clip, &eff->surface->top_clipper);
		if (!ctx->bi->clip.width || !ctx->bi->clip.height) {
			ctx->bi->clip.width = 0;
			/*remove if this is the last context*/
			if (eff->surface->cur_context == ctx) eff->surface->cur_context->drawable = NULL;
			return;
		}
		VS2D_RegisterSensor(eff->surface, ctx);

		/*keep track of node drawn, whether direct or indirect rendering*/
		if (!(ctx->drawable->flags & DRAWABLE_REG_WITH_SURFACE)) {
			struct _drawable_store *it;
			GF_SAFEALLOC(it, struct _drawable_store);
			it->drawable = ctx->drawable;
			if (eff->surface->last_prev_entry) {
				eff->surface->last_prev_entry->next = it;
				eff->surface->last_prev_entry = it;
			} else {
				eff->surface->prev_nodes = eff->surface->last_prev_entry = it;
			}
			ctx->drawable->flags |= DRAWABLE_REG_WITH_SURFACE;
		}

		if (eff->trav_flags & TF_RENDER_DIRECT) {
			assert(!eff->traversing_mode);
			eff->traversing_mode = TRAVERSE_DRAW;
			eff->ctx = ctx;
			gf_node_allow_cyclic_render(ctx->drawable->node);
			gf_node_render(ctx->drawable->node, eff);
			eff->ctx = NULL;
			eff->traversing_mode = 0;
		}
	}
}

void drawable_check_bounds(struct _drawable_context *ctx, VisualSurface2D *surf)
{
	if (!ctx->bi) {
		ctx->bi = drawable_check_alloc_bounds(ctx, surf);
		assert(ctx->bi);
		ctx->bi->extra_check = ctx->appear;
	}
}

void drawable_finalize_render(struct _drawable_context *ctx, RenderEffect2D *eff, GF_Rect *orig_bounds)
{
	Fixed pw;
	GF_Rect unclip;

	drawable_check_bounds(ctx, eff->surface);

	if (orig_bounds) {
		ctx->bi->unclip = *orig_bounds;
	} else {
		gf_path_get_bounds(ctx->drawable->path, &ctx->bi->unclip);
	}
	gf_mx2d_apply_rect(&eff->transform, &ctx->bi->unclip);

	/*apply pen width*/
	if (ctx->aspect.pen_props.width) {
		StrikeInfo2D *si = NULL;

		/*if pen is not scalable, apply user/viewport transform so that original aspect is kept*/
		if (!ctx->aspect.line_scale) {
			GF_Point2D pt;
			pt.x = ctx->transform.m[0] + ctx->transform.m[1];
			pt.y = ctx->transform.m[3] + ctx->transform.m[4];
			ctx->aspect.line_scale = gf_divfix(FLT2FIX(1.41421356f) , gf_v2d_len(&pt));
		}
		
		/*get strike info & outline for exact bounds compute. If failure use default offset*/
		si = drawctx_get_strikeinfo(ctx, ctx->drawable->path);
		if (si && si->outline) {
			gf_path_get_bounds(si->outline, &ctx->bi->unclip);
			gf_mx2d_apply_rect(&eff->transform, &ctx->bi->unclip);
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
			pw = (eff->is_pixel_metrics) ? FIX_ONE : 2*FIX_ONE/eff->surface->width;
			unclip.x -= pw;
			unclip.y += pw;
			unclip.width += 2*pw;
			unclip.height += 2*pw;
		}
		ctx->bi->clip = gf_rect_pixelize(&unclip);
	} else {
		ctx->bi->clip.width = 0;
	}
	drawable_finalize_end(ctx, eff);
}


void delete_strikeinfo2d(StrikeInfo2D *info)
{
	if (info->outline) gf_path_del(info->outline);
	free(info);
}

StrikeInfo2D *drawctx_get_strikeinfo(DrawableContext *ctx, GF_Path *path)
{
	StrikeInfo2D *si, *prev;
	GF_Node *lp;
	u32 now;
	if (ctx->appear && !ctx->aspect.pen_props.width) return NULL;
	if (path && !path->n_points) return NULL;

	lp = NULL;
	if (ctx->appear && (gf_node_get_tag(ctx->appear) <GF_NODE_RANGE_FIRST_SVG) ) {
		lp = ((M_Appearance *)ctx->appear)->material;
		if (lp) lp = ((M_Material2D *) lp)->lineProps;
	}

	prev = NULL;
	si = ctx->drawable->outline;
	while (si) {
		/*note this includes default LP (NULL)*/
		if ((si->lineProps == lp) && (!path || (path==si->original)) ) break;
		if (!si->lineProps) {
			Render2D *r2d = gf_sr_get_renderer(ctx->drawable->node) ? gf_sr_get_renderer(ctx->drawable->node)->visual_renderer->user_priv : NULL; 
			gf_list_del_item(r2d->strike_bank, si);
			if (si->outline) gf_path_del(si->outline);
			if (prev) prev->next = si->next;
			else ctx->drawable->outline = si->next;
			free(si);
			si = prev ? prev->next : ctx->drawable->outline;
			continue;
		}
		prev = si;
		si = si->next;
	}
	/*not found, add*/
	if (!si) {
		Render2D *r2d = gf_sr_get_renderer(ctx->drawable->node) ? gf_sr_get_renderer(ctx->drawable->node)->visual_renderer->user_priv : NULL; 
		GF_SAFEALLOC(si, StrikeInfo2D);
		si->lineProps = lp;
		si->node = ctx->drawable->node;
		if (ctx->drawable->outline) {
			prev = ctx->drawable->outline;
			while (prev->next) prev = prev->next;
			prev->next = si;
		} else {
			ctx->drawable->outline = si;
		}
		gf_list_add(r2d->strike_bank, si);
	}

	/*node changed or outline not build*/
	now = lp ? R2D_LP_GetLastUpdateTime(lp) : si->last_update_time;
	if (!si->outline || (now!=si->last_update_time) || (si->line_scale != ctx->aspect.line_scale) || (si->path_length != ctx->aspect.pen_props.path_length) || (ctx->flags & CTX_SVG_OUTLINE_GEOMETRY_DIRTY)) {
		u32 i;
		Fixed w = ctx->aspect.pen_props.width;
		Fixed dash_o = ctx->aspect.pen_props.dash_offset;
		si->last_update_time = now;
		si->line_scale = ctx->aspect.line_scale;
		if (si->outline) gf_path_del(si->outline);
		/*apply scale whether scalable or not (if not scalable, scale is still needed for scalable zoom)*/
		ctx->aspect.pen_props.width = gf_mulfix(ctx->aspect.pen_props.width, ctx->aspect.line_scale);
		if (ctx->aspect.pen_props.dash != GF_DASH_STYLE_CUSTOM_ABS) 
			ctx->aspect.pen_props.dash_offset = gf_mulfix(ctx->aspect.pen_props.dash_offset, ctx->aspect.pen_props.width);
		
		if (ctx->aspect.pen_props.dash_set) {
			for(i=0; i<ctx->aspect.pen_props.dash_set->num_dash; i++) {
				ctx->aspect.pen_props.dash_set->dashes[i] = gf_mulfix(ctx->aspect.pen_props.dash_set->dashes[i], ctx->aspect.line_scale);
			}
		}

		if (path) {
			si->outline = gf_path_get_outline(path, ctx->aspect.pen_props);
			si->original = path;
		} else {
			si->outline = gf_path_get_outline(ctx->drawable->path, ctx->aspect.pen_props);
		}
		/*restore*/
		ctx->aspect.pen_props.width = w;
		ctx->aspect.pen_props.dash_offset = dash_o;
		if (ctx->aspect.pen_props.dash_set) {
			for(i=0; i<ctx->aspect.pen_props.dash_set->num_dash; i++) {
				ctx->aspect.pen_props.dash_set->dashes[i] = gf_divfix(ctx->aspect.pen_props.dash_set->dashes[i], ctx->aspect.line_scale);
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
		si->original = NULL;
		si = si->next;
	}
}

void drawable_reset_path(Drawable *st)
{
	drawable_reset_path_outline(st);
	if (st->path) gf_path_reset(st->path);
}


void R2D_LinePropsRemoved(Render2D *sr, GF_Node *n)
{
	StrikeInfo2D *si, *cur, *prev;
	u32 i = 0;
	while ((si = (StrikeInfo2D*)gf_list_enum(sr->strike_bank, &i))) {
		if (si->lineProps == n) {
			/*remove from node*/
			if (si->node) {
				Drawable *st = (Drawable *) gf_node_get_private(si->node);
				/*yerk this is ugly, but text is not a regular drawable and adding a fct just to get
				the drawable would be just pain*/
				if (gf_node_get_tag(si->node)==TAG_MPEG4_Text) st = ((TextStack2D*)st)->graph;
				assert(st && st->outline);
				cur = st->outline;
				prev = NULL;
				while (cur) {
					if (cur!=si) {
						prev = cur;
						cur = cur->next;
						continue;
					}
					if (prev) prev->next = cur->next;
					else st->outline = cur->next;
					break;
				}
			}
			i--;
			gf_list_rem(sr->strike_bank, i);
			delete_strikeinfo2d(si);
		}
	}
}

static void DestroyLineProps(GF_Node *n, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		LinePropStack *st = (LinePropStack *)gf_node_get_private(n);
		R2D_LinePropsRemoved(st->sr, n);
		free(st);
	}
}
void R2D_InitLineProps(Render2D *sr, GF_Node *node)
{
	LinePropStack *st = (LinePropStack *)malloc(sizeof(LinePropStack));
	st->sr = sr;
	st->last_mod_time = 1;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, DestroyLineProps);
}

u32 R2D_LP_GetLastUpdateTime(GF_Node *node)
{
	LinePropStack *st = (LinePropStack *)gf_node_get_private(node);
	if (!st) return 0;
	if (gf_node_dirty_get(node) & GF_SG_NODE_DIRTY) {
		st->last_mod_time ++;
		gf_node_dirty_clear(node, 0);
	}
	return st->last_mod_time;
}

#ifndef GPAC_DISABLE_SVG

static GF_Node *svg_get_texture_target(GF_Node *node, DOM_String uri)
{
	GF_Node *target = NULL;
	if (uri[0]=='#') target = gf_sg_find_node_by_name(gf_node_get_graph(node), uri+1);
	return target;
}

static void setup_SVG_drawable_context(DrawableContext *ctx, struct _visual_surface_2D *surf, SVGPropertiesPointers *props)
{
	Fixed clamped_solid_opacity = FIX_ONE;
	Fixed clamped_fill_opacity = (props->fill_opacity->value < 0 ? 0 : (props->fill_opacity->value > FIX_ONE ? FIX_ONE : props->fill_opacity->value));
	Fixed clamped_stroke_opacity = (props->stroke_opacity->value < 0 ? 0 : (props->stroke_opacity->value > FIX_ONE ? FIX_ONE : props->stroke_opacity->value));	

	ctx->aspect.fill_color = 0;

	if (props->fill->type==SVG_PAINT_URI) {
		if (props->fill->iri.type != SVG_IRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			SVG_IRI *iri = &props->fill->iri;
			GF_SceneGraph *sg = gf_node_get_graph(ctx->drawable->node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->iri[1]));
			if (n) {
				iri->type = SVG_IRI_ELEMENTID;
				iri->target = (SVGElement *) n;
				gf_svg_register_iri(sg, iri);
				free(iri->iri);
				iri->iri = NULL;
			}
		}		
		/* If paint server not found, paint is equivalent to none */
		if (props->fill->iri.type == SVG_IRI_ELEMENTID) {
			switch (gf_node_get_tag((GF_Node *)props->fill->iri.target)) {
			case TAG_SVG_solidColor:
				{
					SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)props->fill->iri.target;
					clamped_solid_opacity = (solidColorElt->properties->solid_opacity.value < 0 ? 0 : (solidColorElt->properties->solid_opacity.value > FIX_ONE ? FIX_ONE : solidColorElt->properties->solid_opacity.value));
					ctx->aspect.fill_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);			
				}
				break;
			case TAG_SVG_linearGradient: 
			case TAG_SVG_radialGradient: 
				ctx->h_texture = svg_gradient_get_texture((GF_Node *)props->fill->iri.target);
				break;
			default: 
				break;
			}
		}
	} else if (props->fill->type == SVG_PAINT_COLOR) {
		if (props->fill->color.type == SVG_COLOR_CURRENTCOLOR) {
			ctx->aspect.fill_color = GF_COL_ARGB_FIXED(clamped_fill_opacity, props->color->color.red, props->color->color.green, props->color->color.blue);
		} else if (props->fill->color.type == SVG_COLOR_RGBCOLOR) {
			ctx->aspect.fill_color = GF_COL_ARGB_FIXED(clamped_fill_opacity, props->fill->color.red, props->fill->color.green, props->fill->color.blue);
		} else if (props->fill->color.type >= SVG_COLOR_ACTIVE_BORDER) {
			ctx->aspect.fill_color = surf->render->compositor->sys_colors[props->fill->color.type - 3];
			ctx->aspect.fill_color |= ((u32) (clamped_fill_opacity*255) ) << 24;
		}
	}

	if (*props->fill_rule == SVG_FILLRULE_NONZERO) {
		ctx->drawable->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
	} else {
		ctx->drawable->path->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
	}

	ctx->aspect.line_color = 0;
	ctx->aspect.pen_props.width = (props->stroke->type != SVG_PAINT_NONE) ? props->stroke_width->value : 0;
	if (props->stroke->type==SVG_PAINT_URI) {
		if (props->stroke->iri.type != SVG_IRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			SVG_IRI *iri = &props->stroke->iri;
			GF_SceneGraph *sg = gf_node_get_graph(ctx->drawable->node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->iri[1]));
			if (n) {
				iri->type = SVG_IRI_ELEMENTID;
				iri->target = (SVGElement *) n;
				gf_svg_register_iri(sg, iri);
				free(iri->iri);
				iri->iri = NULL;
			}
		}		
		/* Paint server not found, stroke is equivalent to none */
		if (props->stroke->iri.type == SVG_IRI_ELEMENTID) {
			switch (gf_node_get_tag((GF_Node *)props->stroke->iri.target)) {
			case TAG_SVG_solidColor:
				{
					SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)props->stroke->iri.target;
					clamped_solid_opacity = (solidColorElt->properties->solid_opacity.value < 0 ? 0 : (solidColorElt->properties->solid_opacity.value > FIX_ONE ? FIX_ONE : solidColorElt->properties->solid_opacity.value));
					ctx->aspect.line_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);			
				}
				break;
			case TAG_SVG_linearGradient: 
			case TAG_SVG_radialGradient: 
				ctx->aspect.line_texture = svg_gradient_get_texture((GF_Node*)props->stroke->iri.target);
				break;
			default: 
				break;
			}
		}
	} else if (props->stroke->type == SVG_PAINT_COLOR) {
		if (props->stroke->color.type == SVG_COLOR_CURRENTCOLOR) {
			ctx->aspect.line_color = GF_COL_ARGB_FIXED(clamped_stroke_opacity, props->color->color.red, props->color->color.green, props->color->color.blue);
		} else if (props->stroke->color.type == SVG_COLOR_RGBCOLOR) {
			ctx->aspect.line_color = GF_COL_ARGB_FIXED(clamped_stroke_opacity, props->stroke->color.red, props->stroke->color.green, props->stroke->color.blue);
		} else if (props->stroke->color.type >= SVG_COLOR_ACTIVE_BORDER) {
			ctx->aspect.line_color = surf->render->compositor->sys_colors[SVG_COLOR_ACTIVE_BORDER - 3];
			ctx->aspect.line_color |= ((u32) (clamped_stroke_opacity*255)) << 24;
		}
	}
	if (props->stroke_dasharray->type != SVG_STROKEDASHARRAY_NONE) {
		ctx->aspect.pen_props.dash = GF_DASH_STYLE_CUSTOM_ABS;
		ctx->aspect.pen_props.dash_offset = props->stroke_dashoffset->value;
		ctx->aspect.pen_props.dash_set = (GF_DashSettings *) &(props->stroke_dasharray->array);
	}
	ctx->aspect.line_scale = (*props->vector_effect == SVG_VECTOREFFECT_NONSCALINGSTROKE) ? 0 : FIX_ONE;
	
	ctx->aspect.pen_props.cap = (u8) *props->stroke_linecap;
	ctx->aspect.pen_props.join = (u8) *props->stroke_linejoin;
	ctx->aspect.pen_props.miterLimit = props->stroke_miterlimit->value;
}

DrawableContext *SVG_drawable_init_context(Drawable *drawable, RenderEffect2D *eff)
{
	DrawableContext *ctx;
	Bool skipFill = 0;
	assert(eff->surface);

	/*switched-off geometry nodes are not rendered*/
	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return NULL;

	//Get a empty context from the current surface
	ctx = VS2D_GetDrawableContext(eff->surface);

	gf_mx2d_copy(ctx->transform, eff->transform);

	ctx->drawable = drawable;
	ctx->appear = eff->parent_use;

	if (eff->invalidate_all || gf_svg_has_appearance_flag_dirty(eff->svg_flags)) ctx->flags |= CTX_APP_DIRTY;
	if (eff->svg_flags & (GF_SG_SVG_STROKEDASHARRAY_DIRTY | 
						  GF_SG_SVG_STROKEDASHOFFSET_DIRTY |
						  GF_SG_SVG_STROKELINECAP_DIRTY | 
						  GF_SG_SVG_STROKELINEJOIN_DIRTY |
						  GF_SG_SVG_STROKEMITERLIMIT_DIRTY | 
						  GF_SG_SVG_STROKEWIDTH_DIRTY |
						  GF_SG_SVG_VECTOREFFECT_DIRTY )) ctx->flags |= CTX_SVG_OUTLINE_GEOMETRY_DIRTY;

	ctx->h_texture = NULL;

	/*FIXME - only needed for texture*/
	if (!eff->color_mat.identity) {
		GF_SAFEALLOC(ctx->col_mat, GF_ColorMatrix);
		gf_cmx_copy(ctx->col_mat, &eff->color_mat);
	}
	
	switch (gf_node_get_tag(ctx->drawable->node) ) {
	case TAG_SVG_image:
	case TAG_SVG_video:
	case TAG_SVG2_image:
	case TAG_SVG2_video:
	case TAG_SVG3_image:
	case TAG_SVG3_video:
		{
			SVG_image_stack *st = (SVG_image_stack*) gf_node_get_private(ctx->drawable->node);
			ctx->h_texture = &(st->txh);
		}
		break;
	default:
		break;
	}

	setup_SVG_drawable_context(ctx, eff->surface, eff->svg_props);

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->h_texture && ctx->h_texture->needs_refresh) ctx->flags |= CTX_TEXTURE_DIRTY;

	if (check_transparent_skip(ctx, skipFill)) {
		VS2D_RemoveLastContext(eff->surface);
		return NULL;
	}
	//ctx->flags |= CTX_HAS_LISTENERS;
	return ctx;
}

static void setup_svg2_drawable_context(DrawableContext *ctx, struct _visual_surface_2D *surf)
{
	SVG2pathElement *path = (SVG2pathElement *)ctx->drawable->node;
	Fixed clamped_solid_opacity = FIX_ONE;
	Fixed clamped_fill_opacity = (path->fill_opacity.value < 0 ? 0 : (path->fill_opacity.value > FIX_ONE ? FIX_ONE : path->fill_opacity.value));
	Fixed clamped_stroke_opacity = (path->stroke_opacity.value < 0 ? 0 : (path->stroke_opacity.value > FIX_ONE ? FIX_ONE : path->stroke_opacity.value));	

	ctx->aspect.fill_color = 0;

	if (path->fill.type==SVG_PAINT_URI) {
		if (path->fill.iri.type != SVG_IRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			SVG_IRI *iri = &path->fill.iri;
			GF_SceneGraph *sg = gf_node_get_graph(ctx->drawable->node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->iri[1]));
			if (n) {
				iri->type = SVG_IRI_ELEMENTID;
				iri->target = (SVGElement *) n;
				gf_svg_register_iri(sg, iri);
				free(iri->iri);
				iri->iri = NULL;
			}
		}		
		/* Paint server not found, paint is equivalent to none */
		if (path->fill.iri.type == SVG_IRI_ELEMENTID) {
			switch (gf_node_get_tag((GF_Node *)path->fill.iri.target)) {
			case TAG_SVG_solidColor:
				{
					SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)path->fill.iri.target;
					clamped_solid_opacity = (solidColorElt->properties->solid_opacity.value < 0 ? 0 : (solidColorElt->properties->solid_opacity.value > FIX_ONE ? FIX_ONE : solidColorElt->properties->solid_opacity.value));
					ctx->aspect.fill_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);			
				}
				break;
			case TAG_SVG_linearGradient: 
			case TAG_SVG_radialGradient: 
				ctx->h_texture = svg_gradient_get_texture((GF_Node *)path->fill.iri.target);
				break;
			default: 
				break;
			}
		}
	}
	else if (path->fill.color.type == SVG_COLOR_RGBCOLOR) {
		ctx->aspect.fill_color = GF_COL_ARGB_FIXED(clamped_fill_opacity, path->fill.color.red, path->fill.color.green, path->fill.color.blue);
	} else if (path->fill.color.type >= SVG_COLOR_ACTIVE_BORDER) {
		ctx->aspect.fill_color = surf->render->compositor->sys_colors[path->fill.color.type - 3];
		ctx->aspect.fill_color |= ((u32) (clamped_fill_opacity*255) ) << 24;
	}

	ctx->aspect.pen_props.width = (path->stroke.type != SVG_PAINT_NONE) ? path->stroke_width.value : 0;
	if (path->stroke.type==SVG_PAINT_URI) {
		if (path->stroke.iri.type != SVG_IRI_ELEMENTID) {
			/* trying to resolve the IRI to the Paint Server */
			SVG_IRI *iri = &path->stroke.iri;
			GF_SceneGraph *sg = gf_node_get_graph(ctx->drawable->node);
			GF_Node *n = gf_sg_find_node_by_name(sg, &(iri->iri[1]));
			if (n) {
				iri->type = SVG_IRI_ELEMENTID;
				iri->target = (SVGElement *) n;
				gf_svg_register_iri(sg, iri);
				free(iri->iri);
				iri->iri = NULL;
			}
		}		
		if (path->stroke.iri.type != SVG_IRI_ELEMENTID) {
			/* Paint server not found, stroke is equivalent to none */
			ctx->aspect.pen_props.width = 0;
		} else {
			switch (gf_node_get_tag((GF_Node *)path->stroke.iri.target)) {
			case TAG_SVG_solidColor:
				{
					SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)path->stroke.iri.target;
					clamped_solid_opacity = (solidColorElt->properties->solid_opacity.value < 0 ? 0 : (solidColorElt->properties->solid_opacity.value > FIX_ONE ? FIX_ONE : solidColorElt->properties->solid_opacity.value));
					ctx->aspect.line_color = GF_COL_ARGB_FIXED(clamped_solid_opacity, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);			
				}
				break;
			case TAG_SVG_linearGradient: 
			case TAG_SVG_radialGradient: 
				ctx->aspect.line_texture = svg_gradient_get_texture((GF_Node*)path->stroke.iri.target);
				break;
			default: 
				ctx->aspect.pen_props.width = 0;
			}
		}
	}
	else if (path->stroke.color.type == SVG_COLOR_RGBCOLOR) {
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(clamped_stroke_opacity, path->stroke.color.red, path->stroke.color.green, path->stroke.color.blue);
	} else if (path->stroke.color.type >= SVG_COLOR_ACTIVE_BORDER) {
		ctx->aspect.line_color = surf->render->compositor->sys_colors[SVG_COLOR_ACTIVE_BORDER - 3];
		ctx->aspect.line_color |= ((u32) (clamped_stroke_opacity*255)) << 24;
	}
	if (path->stroke_dasharray.type != SVG_STROKEDASHARRAY_NONE) {
		ctx->aspect.pen_props.dash = GF_DASH_STYLE_CUSTOM_ABS;
		ctx->aspect.pen_props.dash_offset = path->stroke_dashoffset.value;
		ctx->aspect.pen_props.dash_set = (GF_DashSettings *) &(path->stroke_dasharray.array);
	}
	ctx->aspect.line_scale = (path->vector_effect == SVG_VECTOREFFECT_NONSCALINGSTROKE) ? 0 : FIX_ONE;
	
	ctx->aspect.pen_props.cap = path->stroke_linecap;
	ctx->aspect.pen_props.join = path->stroke_linejoin;
	ctx->aspect.pen_props.miterLimit = path->stroke_miterlimit.value;
}

DrawableContext *svg2_drawable_init_context(Drawable *drawable, RenderEffect2D *eff)
{
	DrawableContext *ctx;
	Bool skipFill = 0;
	assert(eff->surface);

	/*switched-off geometry nodes are not rendered*/
	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return NULL;
	
	//Get a empty context from the current surface
	ctx = VS2D_GetDrawableContext(eff->surface);

	gf_mx2d_copy(ctx->transform, eff->transform);

	ctx->drawable = drawable;
	ctx->appear = eff->parent_use;
	if (eff->invalidate_all || gf_svg_has_appearance_flag_dirty(eff->svg_flags)) ctx->flags |= CTX_APP_DIRTY;

	ctx->h_texture = NULL;

	setup_svg2_drawable_context(ctx, eff->surface);

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->h_texture && ctx->h_texture->needs_refresh) ctx->flags |= CTX_TEXTURE_DIRTY;

	if (check_transparent_skip(ctx, skipFill)) {
		VS2D_RemoveLastContext(eff->surface);
		return NULL;
	}
	//ctx->flags |= CTX_HAS_LISTENERS;
	return ctx;
}

#endif	//SVG
