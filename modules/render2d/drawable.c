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

#define BOUNDSINFO_STEPALLOC		1

static Bool check_bounds_size(Drawable *node)
{
	u32 i;
	BoundsInfo **new_bounds;
	if (node->current_count < node->bounds_size) return 1;
	new_bounds = realloc(node->previous_bounds, sizeof(BoundsInfo *) * (node->bounds_size + BOUNDSINFO_STEPALLOC));
	if (!new_bounds) return 0;
	node->previous_bounds = new_bounds;
	new_bounds = realloc(node->current_bounds, sizeof(BoundsInfo *) * (node->bounds_size + BOUNDSINFO_STEPALLOC));
	if (!new_bounds) return 0;
	node->current_bounds = new_bounds;
	
	for (i=node->bounds_size; i<node->bounds_size + BOUNDSINFO_STEPALLOC; i++) {
		node->current_bounds[i] = malloc(sizeof(BoundsInfo));
		node->previous_bounds[i] = malloc(sizeof(BoundsInfo));
	}
	node->bounds_size += BOUNDSINFO_STEPALLOC;
	return 1;
}

static void bounds_remove_prev_item(Drawable *node, u32 pos)
{
	BoundsInfo *bi = node->previous_bounds[pos];
	u32 count = node->previous_count - pos - 1;
	if (count) {
		memmove(&node->previous_bounds[pos], &node->previous_bounds[pos+1], sizeof(BoundsInfo *)*count);
		node->previous_bounds[node->previous_count-1] = bi;
	}
	node->previous_count--; 
}


/*default rendering routine*/
static void drawable_draw(DrawableContext *ctx) 
{
	VS2D_TexturePath(ctx->surface, ctx->node->path, ctx);
	VS2D_DrawPath(ctx->surface, ctx->node->path, ctx, NULL, NULL);
}

/*default point_over routine*/
static Bool drawable_point_over(DrawableContext *ctx, Fixed x, Fixed y, u32 check_type)
{
	GF_Matrix2D inv;
	StrikeInfo2D *si;
	if (!ctx || !ctx->node->path) return 0;

	assert(ctx->surface);
	gf_mx2d_copy(inv, ctx->transform);
	gf_mx2d_inverse(&inv);
	gf_mx2d_apply_coords(&inv, &x, &y);
	if (ctx->aspect.filled || ctx->h_texture || (check_type<2) )
		if (gf_path_point_over(ctx->node->path, x, y)) return 1;

	if (ctx->aspect.pen_props.width || ctx->aspect.line_texture || check_type) {
		si = drawctx_get_strikeinfo(ctx, NULL);
		if (si && si->outline && gf_path_point_over(si->outline, x, y)) return 1;
	}
	return 0;
}

/*get the orignal path without transform*/
void drawctx_store_original_bounds(struct _drawable_context *ctx)
{
	gf_path_get_bounds(ctx->node->path, &ctx->original);
}

Drawable *drawable_new()
{
	Drawable *tmp;
	GF_SAFEALLOC(tmp, sizeof(Drawable))
	tmp->on_surfaces = gf_list_new();
	tmp->path = gf_path_new();
	/*init with default*/
	tmp->Draw = drawable_draw;
	tmp->IsPointOver = drawable_point_over;
	tmp->strike_list = gf_list_new();

	/*alloc bounds storage*/
	check_bounds_size(tmp);	
	return tmp;
}

void drawable_reset_bounds(Drawable *dr)
{
	u32 i;
	/*destroy bounds storage*/
	for (i=0; i<dr->bounds_size; i++) {
		free(dr->current_bounds[i]);
		free(dr->previous_bounds[i]);
	}
	free(dr->current_bounds);
	free(dr->previous_bounds);
	dr->previous_count = dr->current_count = 0;
	dr->previous_bounds = dr->current_bounds = NULL;
	dr->bounds_size = 0;
}

void drawable_del(Drawable *dr)
{
	u32 i;

	/*garbage collection*/
	for (i=0; i<dr->current_count; i++) {
		if (R2D_IsSurfaceRegistered((Render2D*)dr->compositor->visual_renderer->user_priv, dr->current_bounds[i]->surface)) 
			ra_add(&dr->current_bounds[i]->surface->to_redraw, dr->current_bounds[i]->clip);
	}
	for (i=0; i<dr->previous_count; i++) {
		if (R2D_IsSurfaceRegistered((Render2D*)dr->compositor->visual_renderer->user_priv, dr->previous_bounds[i]->surface)) 
			ra_add(&dr->previous_bounds[i]->surface->to_redraw, dr->previous_bounds[i]->clip);
	}


	drawable_reset_previous_bounds(dr);
	
	dr->compositor->draw_next_frame = 1;
	/*remove node from all surfaces it's on*/
	while (gf_list_count(dr->on_surfaces)) {
		VisualSurface2D *surf = gf_list_get(dr->on_surfaces, 0);
		gf_list_rem(dr->on_surfaces, 0);
		if (R2D_IsSurfaceRegistered((Render2D *)dr->compositor->visual_renderer->user_priv, surf)) 
			VS2D_DrawableDeleted(surf, dr);
	}
	gf_list_del(dr->on_surfaces);

	/*remove path object*/
	if (dr->path) gf_path_del(dr->path);

	while (gf_list_count(dr->strike_list)) {
		StrikeInfo2D *si = gf_list_get(dr->strike_list, 0);
		gf_list_rem(dr->strike_list, 0);
		/*remove from main strike list*/
		gf_list_del_item(((Render2D *)dr->compositor->visual_renderer->user_priv)->strike_bank, si);
		delete_strikeinfo2d(si);
	}
	gf_list_del(dr->strike_list);
	
	drawable_reset_bounds(dr);
	free(dr);
}

static void DestroyDrawableNode(GF_Node *node)
{
	Drawable *ptr = gf_node_get_private(node);
	drawable_del(ptr);
}

Drawable *drawable_stack_new(Render2D *sr, GF_Node *node)
{
	Drawable *stack = drawable_new();
	gf_sr_traversable_setup(stack, node, sr->compositor);
	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, DestroyDrawableNode);
	return stack;
}

/*store ctx bounds in current bounds*/
void drawable_store_bounds(struct _drawable_context *ctx)
{
	BoundsInfo *bi;
	Drawable *node = ctx->node;
	if (!check_bounds_size(ctx->node)) return;
	bi = node->current_bounds[node->current_count];
	node->current_count++;
	bi->appear = (M_Appearance *) ctx->appear;
	bi->clip = ctx->clip;
	bi->unclip = ctx->unclip_pix;
	bi->surface = ctx->surface;
}

/*move current bounds to previous bounds*/
void drawable_flush_bounds(Drawable *node, u32 frame_num)
{
	BoundsInfo **tmp;
	if (node->first_ctx_update) return;
	if (node->last_bound_flush_frame==frame_num) 
		return;
	/*reset previous bounds on this surface*/
	tmp = node->previous_bounds;
	node->previous_bounds = node->current_bounds;
	node->previous_count = node->current_count;
	node->current_bounds = tmp;
	node->current_count = 0;
	node->first_ctx_update = 1;
	node->node_was_drawn = 0;
	node->last_bound_flush_frame = frame_num;
}	

/*
	return 1 if same bound is found in previous list (and remove it from the list)
	return 0 otherwise
*/
Bool drawable_has_same_bounds(struct _drawable_context *ctx)
{
	u32 i;
	Drawable *node = ctx->node;
	
	for(i=0; i<node->previous_count; i++) {
		BoundsInfo *bi = node->previous_bounds[i];
		if (bi->surface != ctx->surface) continue;
		if (bi->appear != (M_Appearance *) ctx->appear) continue;
		/*bounds shall match for unclip (large object pan) and clip */
		if (gf_irect_equal(bi->unclip, ctx->unclip_pix) && gf_irect_equal(bi->clip, ctx->clip)) {
			bounds_remove_prev_item(node, i);
			return 1;
		}
	}
	return 0;
}

/*
	return any previous bounds related to the same surface in @rc if any
	if nothing found return 0
*/
Bool drawable_get_previous_bound(Drawable *node, GF_IRect *rc, struct _visual_surface_2D *surf)
{
	u32 i;
	for (i=0; i<node->previous_count; i++) {
		BoundsInfo *bi = node->previous_bounds[i];
		if (bi->surface != surf) continue;
		*rc = bi->clip;
		bounds_remove_prev_item(node, i);
		return 1;
	}
	return 0;
}

/*reset content of previous bounds list*/
void drawable_reset_previous_bounds(Drawable *node)
{
	node->previous_count = 0;
}


void drawable_register_on_surface(Drawable *node, struct _visual_surface_2D *surf)
{
	/*reset the draw state check in case the node is being rendered on several surfaces (in which case it
	may not be drawn on one but drawn on the other)*/
	node->first_ctx_update = 0;

	if (gf_list_find(node->on_surfaces, surf)>=0) return;
	gf_list_add(node->on_surfaces, surf);
	gf_list_add(surf->prev_nodes_drawn, node);
}
void drawable_unregister_from_surface(Drawable *node, struct _visual_surface_2D *surf)
{
	gf_list_del_item(node->on_surfaces, surf);
	/*no longer registered, remove bounds*/
	if (!gf_list_count(node->on_surfaces)) drawable_reset_bounds(node);
}



DrawableContext *NewDrawableContext()
{
	DrawableContext *tmp = malloc(sizeof(DrawableContext));
	memset(tmp, 0, sizeof(DrawableContext));
	tmp->sensors = gf_list_new();
	return tmp;
}
void DeleteDrawableContext(DrawableContext *ctx)
{
	drawctx_reset(ctx);
	if (ctx->sensors) gf_list_del(ctx->sensors);
	free(ctx);
}
void drawctx_reset(DrawableContext *ctx)
{
	GF_List *bckup;
	drawctx_reset_sensors(ctx);
	bckup = ctx->sensors;
	memset(ctx, 0, sizeof(DrawableContext));
	ctx->sensors = bckup;
	gf_cmx_init(&ctx->cmat);

	/*by default all nodes are transparent*/
	ctx->transparent = 1;

	/*BIFS has default value for 2D appearance ...*/
	ctx->aspect.fill_alpha  = 0xFF;
	ctx->aspect.fill_color = 0xFFCCCCCC;
	ctx->aspect.line_color = 0xFFCCCCCC;
	ctx->aspect.pen_props.width = FIX_ONE;
	ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	ctx->aspect.pen_props.join = GF_LINE_JOIN_BEVEL;
	ctx->aspect.pen_props.miterLimit = 4*FIX_ONE;

}

void drawctx_reset_sensors(DrawableContext *ctx)
{
	while (gf_list_count(ctx->sensors)) {
		SensorContext *ptr = gf_list_get(ctx->sensors, 0);
		gf_list_rem(ctx->sensors, 0);
		free(ptr);
	}
}
static void drawctx_add_sensor(DrawableContext *ctx, SensorContext *handler)
{
	SensorContext *pNew = malloc(sizeof(SensorContext));
	pNew->h_node = handler->h_node;
	gf_mx2d_copy(pNew->matrix, handler->matrix);
	gf_list_add(ctx->sensors, pNew);
}

void drawctx_update_info(DrawableContext *ctx)
{
	Bool moved, need_redraw;
	Drawable *node = ctx->node;
	need_redraw = ctx->redraw_flags ? 1 : 0;

	node->node_changed = 0;
	/*first update, remove all bounds*/
	if (node->first_ctx_update) {
		node->node_was_drawn = node->current_count; 
		node->first_ctx_update = 0;
	}
	/*only checked if node is not marked as dirty (otherwise we assume bounds have changed)*/
	if (! (ctx->redraw_flags & CTX_NODE_DIRTY) ) {
		moved = !drawable_has_same_bounds(ctx);
		if (!need_redraw) need_redraw = moved;
	}
	ctx->redraw_flags = need_redraw;

	/*in all cases reset dirty flag of appearance and its sub-nodes*/
	gf_node_dirty_reset(ctx->appear);
}


static void setup_drawable_context(DrawableContext *ctx, RenderEffect2D *eff)
{
	M_Material2D *m = NULL;
	M_LineProperties *LP;
	M_XLineProperties *XLP;

	if (ctx->appear == NULL) goto check_default;
	m = (M_Material2D *) ((M_Appearance *)ctx->appear)->material;
	if ( m == NULL) goto check_default;
	if (gf_node_get_tag((GF_Node *) m) != TAG_MPEG4_Material2D) return;

	/*store alpha*/
	ctx->aspect.fill_alpha = (u8) FIX2INT(255*(FIX_ONE - m->transparency));
	ctx->aspect.fill_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, m->emissiveColor.red, m->emissiveColor.green, m->emissiveColor.blue);
	ctx->aspect.fill_color = gf_cmx_apply(&ctx->cmat, ctx->aspect.fill_color);

	ctx->aspect.line_color = ctx->aspect.fill_color;
	ctx->aspect.filled = m->filled;

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
			switch (gf_node_get_tag(ctx->node->owner)) {
			case TAG_MPEG4_IndexedLineSet2D:
				ctx->aspect.filled = 0;
				ctx->aspect.has_line = 1;
				break;
			case TAG_MPEG4_PointSet2D:
				break;
			default:
				if (ctx->aspect.filled) ctx->aspect.pen_props.width = 0;
				else ctx->aspect.has_line = 1;
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
		return;
	}
	ctx->aspect.has_line = 1;
	if (m->lineProps && gf_node_dirty_get(m->lineProps)) ctx->redraw_flags |= CTX_APP_DIRTY;

	if (LP) {
		ctx->aspect.pen_props.dash = LP->lineStyle;
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(FIX_ONE-m->transparency, LP->lineColor.red, LP->lineColor.green, LP->lineColor.blue);
		ctx->aspect.pen_props.width = LP->width;
		ctx->aspect.line_color = gf_cmx_apply(&ctx->cmat, ctx->aspect.line_color);
		return;
	} 

	ctx->aspect.pen_props.dash = XLP->lineStyle;
	ctx->aspect.line_color = GF_COL_ARGB_FIXED(FIX_ONE-XLP->transparency, XLP->lineColor.red, XLP->lineColor.green, XLP->lineColor.blue);
	ctx->aspect.pen_props.width = XLP->width;
	ctx->aspect.line_color = gf_cmx_apply(&ctx->cmat, ctx->aspect.line_color);
	
	ctx->aspect.is_scalable = XLP->isScalable;
	ctx->aspect.pen_props.align = XLP->isCenterAligned ? GF_PATH_LINE_CENTER : GF_PATH_LINE_INSIDE;
	ctx->aspect.pen_props.cap = XLP->lineCap;
	ctx->aspect.pen_props.join = XLP->lineJoin;
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
	if (gf_list_count(ctx->sensors)) return 0;
	/*if texture, cannot skip*/
	if (ctx->h_texture) return 0;
	if (! GF_COL_A(ctx->aspect.fill_color) && !GF_COL_A(ctx->aspect.line_color) ) return 1;
	if (ctx->aspect.pen_props.width == 0) {
		if (skipFill) return 1;
		if (!ctx->aspect.filled) return 1;
	}
	return 0;
}


GF_TextureHandler *drawable_get_texture(RenderEffect2D *eff)
{
	M_Appearance *appear = (M_Appearance *) eff->appear;
	if (!appear || !appear->texture) return NULL;
	return R2D_GetTextureHandler(appear->texture);
}

DrawableContext *drawable_init_context(Drawable *node, RenderEffect2D *eff)
{
	DrawableContext *ctx;
	u32 i, count;
	Bool skipFill;
	assert(eff->surface);

	/*switched-off geometry nodes are not rendered*/
	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return NULL;

	//Get a empty context from the current surface
	ctx = VS2D_GetDrawableContext(eff->surface);

	gf_mx2d_copy(ctx->transform, eff->transform);
	ctx->node = node;
	if (eff->invalidate_all || node->node_changed) 
		ctx->redraw_flags |= CTX_NODE_DIRTY;

	ctx->h_texture = NULL;
	if (eff->appear) {
		ctx->appear = eff->appear;
		if (gf_node_dirty_get(eff->appear)) ctx->redraw_flags |= CTX_APP_DIRTY;
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
	gf_cmx_copy(&ctx->cmat, &eff->color_mat);

	/*IndexedLineSet2D and PointSet2D ignores fill flag and texturing*/
	skipFill = 0;
	ctx->h_texture = NULL;
	switch (gf_node_get_tag(ctx->node->owner) ) {
	case TAG_MPEG4_IndexedLineSet2D: 
	case TAG_MPEG4_PointSet2D: 
		skipFill = 1;
		break;
	default:
		ctx->h_texture = drawable_get_texture(eff);
		break;
	}
	

	/*setup sensors*/
	count = gf_list_count(eff->sensors);
	for (i=0; i<count; i++) 
		drawctx_add_sensor(ctx, gf_list_get(eff->sensors, i));

	setup_drawable_context(ctx, eff);

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->h_texture && ctx->h_texture->needs_refresh) ctx->redraw_flags |= CTX_TEXTURE_DIRTY;

	/*not clear in the spec: what happens when a transparent node is in form/layout ?? this may 
	completely break layout of children. We consider the node should be drawn*/
	if (!eff->parent && check_transparent_skip(ctx, skipFill)) {
		VS2D_RemoveLastContext(eff->surface);
		return NULL;
	}

	//setup clipper if needed

	return ctx;
}

void drawable_finalize_end(struct _drawable_context *ctx, RenderEffect2D *eff)
{
	if (eff->parent) {
		group2d_add_to_context_list(eff->parent, ctx);
	} else {
		/*setup clipper and register bounds & sensors*/
		gf_irect_intersect(&ctx->clip, &eff->surface->top_clipper);
		if (gf_rect_is_empty(ctx->clip) ) return;
		VS2D_RegisterSensor(eff->surface, ctx);
	
		if (eff->trav_flags & TF_RENDER_DIRECT) {
			/*full frame redraw in indirect mode: store bounds for next pass*/
			if (eff->trav_flags & TF_RENDER_STORE_BOUNDS) {
				drawable_store_bounds(ctx);
				drawable_register_on_surface(ctx->node, eff->surface);
			}
			ctx->node->Draw(ctx);
		}
		else drawable_store_bounds(ctx);
	}
}

void drawable_finalize_render(struct _drawable_context *ctx, RenderEffect2D *eff)
{
	Fixed pw;
	GF_Rect unclip;

	ctx->unclip = ctx->original;
	gf_mx2d_apply_rect(&eff->transform, &ctx->unclip);

	/*apply pen width*/
	if (ctx->aspect.has_line && ctx->aspect.pen_props.width ) {
		StrikeInfo2D *si = NULL;

		/*if pen is not scalable, apply user/viewport transform so that original aspect is kept*/
		if (!ctx->aspect.is_scalable) {
			GF_Point2D pt;
			pt.x = ctx->transform.m[0] + ctx->transform.m[1];
			pt.y = ctx->transform.m[3] + ctx->transform.m[4];
			ctx->aspect.line_scale = gf_divfix(FLT2FIX(1.41421356f) , gf_v2d_len(&pt));
		} else {
			ctx->aspect.line_scale = FIX_ONE;
		}
		
		/*get strike info & outline for exact bounds compute. If failure use default offset*/
		si = drawctx_get_strikeinfo(ctx, ctx->node->path);
		if (si && si->outline) {
			gf_path_get_bounds(si->outline, &ctx->unclip);
			gf_mx2d_apply_rect(&eff->transform, &ctx->unclip);
		} else {
			pw = gf_mulfix(ctx->aspect.pen_props.width, ctx->aspect.line_scale);
			ctx->unclip.x -= pw/2;
			ctx->unclip.y += pw/2;
			ctx->unclip.width += pw;
			ctx->unclip.height += pw;
		}
	}
	unclip = ctx->unclip;

	if (!ctx->no_antialias) {
		/*grow of 2 pixels (-1 and +1) to handle AA, but ONLY on cliper otherwise we will modify layout/form */
		pw = (eff->is_pixel_metrics) ? FIX_ONE : 2*FIX_ONE/eff->surface->width;
		unclip.x -= pw;
		unclip.y += pw;
		unclip.width += 2*pw;
		unclip.height += 2*pw;
	}

	ctx->clip = gf_rect_pixelize(&unclip);
	ctx->unclip_pix = ctx->clip;

	drawable_finalize_end(ctx, eff);
}


void delete_strikeinfo2d(StrikeInfo2D *info)
{
	if (info->outline) gf_path_del(info->outline);
	free(info);
}

StrikeInfo2D *drawctx_get_strikeinfo(DrawableContext *ctx, GF_Path *path)
{
	StrikeInfo2D *si;
	GF_Node *lp;
	u32 now, i;
	if (ctx->appear && !ctx->aspect.pen_props.width) return NULL;
	if (path && !path->n_points) return NULL;

	lp = NULL;
	if (ctx->appear) {
		lp = ((M_Appearance *)ctx->appear)->material;
		if (lp) lp = ((M_Material2D *) lp)->lineProps;
	}

	si = NULL;
	i=0;
	while ((si = gf_list_enum(ctx->node->strike_list, &i))) {
		/*note this includes default LP (NULL)*/
		if ((si->lineProps == lp) && (!path || (path==si->original)) ) break;
		if (!si->lineProps) {
			i--;
			gf_list_rem(ctx->node->strike_list, i);

			gf_list_del_item(((Render2D *)ctx->node->compositor->visual_renderer->user_priv)->strike_bank, si);
			if (si->outline) gf_path_del(si->outline);
			free(si);
		}
		si = NULL;
	}
	/*not found, add*/
	if (!si) {
		si = malloc(sizeof(StrikeInfo2D));
		memset(si, 0, sizeof(StrikeInfo2D));
		si->lineProps = lp;
		si->node = ctx->node->owner;
		gf_list_add(ctx->node->strike_list, si);
		gf_list_add(((Render2D *)ctx->node->compositor->visual_renderer->user_priv)->strike_bank, si);
	}

	/*node changed or outline not build*/
	now = lp ? R2D_LP_GetLastUpdateTime(lp) : si->last_update_time;
	if (!si->outline || (now!=si->last_update_time) || (si->line_scale != ctx->aspect.line_scale) || (si->path_length != ctx->aspect.pen_props.path_length)) {
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
			si->outline = gf_path_get_outline(ctx->node->path, ctx->aspect.pen_props);
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

void drawable_reset_path(Drawable *st)
{
	StrikeInfo2D *si;
	u32 i=0;
	while ((si = gf_list_enum(st->strike_list, &i))) {
		if (si->outline) gf_path_del(si->outline);
		si->outline = NULL;
		si->original = NULL;
	}
	if (st->path) gf_path_reset(st->path);
}


void R2D_LinePropsRemoved(Render2D *sr, GF_Node *n)
{
	StrikeInfo2D *si;
	u32 i = 0;
	while ((si = gf_list_enum(sr->strike_bank, &i))) {
		if (si->lineProps == n) {
			/*remove from node*/
			if (si->node) {
				s32 res;
				Drawable *st = (Drawable *) gf_node_get_private(si->node);
				/*yerk this is ugly, but text is not a regular drawable and adding a fct just to get
				the drawable would be just pain*/
				if (gf_node_get_tag(si->node)==TAG_MPEG4_Text) st = ((TextStack2D*)st)->graph;
				assert(st && st->strike_list);
				res = gf_list_del_item(st->strike_list, si);
				assert(res>=0);
			}
			i--;
			gf_list_rem(sr->strike_bank, i);
			delete_strikeinfo2d(si);
		}
	}
}

static void DestroyLineProps(GF_Node *n)
{
	LinePropStack *st = gf_node_get_private(n);
	R2D_LinePropsRemoved(st->sr, n);
	free(st);
}
void R2D_InitLineProps(Render2D *sr, GF_Node *node)
{
	LinePropStack *st = malloc(sizeof(LinePropStack));
	st->sr = sr;
	st->last_mod_time = 1;
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyLineProps);
}

u32 R2D_LP_GetLastUpdateTime(GF_Node *node)
{
	LinePropStack *st = gf_node_get_private(node);
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

static u32 svg_get_texture_type(GF_Node *node, DOM_String uri)
{
	GF_Node *target = NULL;
	if (uri[0]=='#') target = gf_sg_find_node_by_name(gf_node_get_graph(node), uri+1);

	if (!target) return TAG_SVG_UndefinedElement;
	return gf_node_get_tag(target);
}

static GF_TextureHandler *svg_get_texture_handle(GF_Node *node, DOM_String uri)
{
	GF_Node *target = NULL;
	if (uri[0]=='#') {
		target = gf_sg_find_node_by_name(gf_node_get_graph(node), uri+1);
		if (!target && (uri[1]=='N')) target = gf_sg_find_node(gf_node_get_graph(node), atoi(uri+2)+1);
	}

	if (!target) return NULL;
	switch (gf_node_get_tag(target)) {
	case TAG_SVG_linearGradient: 
	case TAG_SVG_radialGradient: 
		return svg_gradient_get_texture(target);
	default: 
		return NULL;
	}
}

static void setup_SVG_drawable_context(DrawableContext *ctx, SVGPropertiesPointers props)
{
	ctx->aspect.fill_alpha = 255;
	ctx->aspect.filled = (props.fill->type != SVG_PAINT_NONE);
	if (props.fill->type==SVG_PAINT_URI) {
		if (svg_get_texture_type(ctx->node->owner, props.fill->uri) == TAG_SVG_solidColor) {
			SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)svg_get_texture_target(ctx->node->owner, props.fill->uri);
			ctx->aspect.fill_color = GF_COL_ARGB_FIXED(solidColorElt->properties->solid_opacity.value, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);			
		} else {
			ctx->h_texture = svg_get_texture_handle(ctx->node->owner, props.fill->uri);
			ctx->aspect.filled = 0;
		}
	}
	else if (props.fill->color.type == SVG_COLOR_CURRENTCOLOR) {
		ctx->aspect.fill_color = GF_COL_ARGB_FIXED(props.fill_opacity->value, props.color->color.red, props.color->color.green, props.color->color.blue);
	} else if (props.fill->color.type == SVG_COLOR_RGBCOLOR) {
		ctx->aspect.fill_color = GF_COL_ARGB_FIXED(props.fill_opacity->value, props.fill->color.red, props.fill->color.green, props.fill->color.blue);
	} else if (props.fill->color.type >= SVG_COLOR_ACTIVE_BORDER) {
		ctx->aspect.fill_color = ctx->surface->render->compositor->sys_colors[props.fill->color.type - 3];
		ctx->aspect.fill_color |= ((u32) (props.fill_opacity->value*255) ) << 24;
	}

	ctx->aspect.has_line = (props.stroke->type != SVG_PAINT_NONE);
	if (props.stroke->type==SVG_PAINT_URI) {
		if (svg_get_texture_type(ctx->node->owner, props.stroke->uri) == TAG_SVG_solidColor) {
			SVGsolidColorElement *solidColorElt = (SVGsolidColorElement *)svg_get_texture_target(ctx->node->owner, props.stroke->uri);
			ctx->aspect.line_color = GF_COL_ARGB_FIXED(solidColorElt->properties->solid_opacity.value, solidColorElt->properties->solid_color.color.red, solidColorElt->properties->solid_color.color.green, solidColorElt->properties->solid_color.color.blue);
		} else {
			ctx->aspect.line_texture = svg_get_texture_handle(ctx->node->owner, props.stroke->uri);
		}
	}
	else if (props.stroke->color.type == SVG_COLOR_CURRENTCOLOR) {
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(props.stroke_opacity->value, props.color->color.red, props.color->color.green, props.color->color.blue);
	} else if (props.stroke->color.type == SVG_COLOR_RGBCOLOR) {
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(props.stroke_opacity->value, props.stroke->color.red, props.stroke->color.green, props.stroke->color.blue);
	} else if (props.stroke->color.type >= SVG_COLOR_ACTIVE_BORDER) {
		ctx->aspect.line_color = ctx->surface->render->compositor->sys_colors[SVG_COLOR_ACTIVE_BORDER - 3];
		ctx->aspect.line_color |= ((u32) (props.stroke_opacity->value*255)) << 24;
	}
	if (props.stroke_dasharray->type != SVG_STROKEDASHARRAY_NONE) {
		ctx->aspect.pen_props.dash = GF_DASH_STYLE_CUSTOM_ABS;
		ctx->aspect.pen_props.dash_offset = props.stroke_dashoffset->value;
		ctx->aspect.pen_props.dash_set = (GF_DashSettings *) &(props.stroke_dasharray->array);
	}
	ctx->aspect.is_scalable = (*props.vector_effect == SVG_VECTOREFFECT_NONSCALINGSTROKE)?0:1;
	
	ctx->aspect.pen_props.cap = *props.stroke_linecap;
	ctx->aspect.pen_props.join = *props.stroke_linejoin;
	ctx->aspect.pen_props.width = (ctx->aspect.has_line?props.stroke_width->value:0);
	ctx->aspect.pen_props.miterLimit = props.stroke_miterlimit->value;
}

DrawableContext *SVG_drawable_init_context(Drawable *node, RenderEffect2D *eff)
{
	DrawableContext *ctx;
	Bool skipFill = 0;
	assert(eff->surface);

	/*switched-off geometry nodes are not rendered*/
	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return NULL;
	
	//Get a empty context from the current surface
	ctx = VS2D_GetDrawableContext(eff->surface);

	gf_mx2d_copy(ctx->transform, eff->transform);

	ctx->node = node;
	ctx->parent_use = eff->parent_use;
	if (eff->invalidate_all || node->node_changed) ctx->redraw_flags |= CTX_NODE_DIRTY;
	else if (gf_node_dirty_get(node->owner) & GF_SG_SVG_APPEARANCE_DIRTY) ctx->redraw_flags |= CTX_NODE_DIRTY;
	else if (ctx->parent_use && (gf_node_dirty_get(ctx->parent_use) & GF_SG_SVG_APPEARANCE_DIRTY)) {
		ctx->redraw_flags |= CTX_NODE_DIRTY;
		gf_node_dirty_clear(ctx->parent_use, GF_SG_SVG_APPEARANCE_DIRTY);
	}

	ctx->h_texture = NULL;

	/*FIXME - only needed for texture*/
	gf_cmx_copy(&ctx->cmat, &eff->color_mat);
	
	switch (gf_node_get_tag(ctx->node->owner) ) {
	case TAG_SVG_image:
		{
			SVG_image_stack *st = (SVG_image_stack*) gf_node_get_private(ctx->node->owner);
			ctx->h_texture = &(st->txh);
		}
		break;
	case TAG_SVG_video:
		{
			SVG_video_stack *st = (SVG_video_stack*) gf_node_get_private(ctx->node->owner);
			ctx->h_texture = &(st->txh);
		}
		break;
	default:
		break;
	}

	setup_SVG_drawable_context(ctx, *(eff->svg_props));

	/*Update texture info - draw even if texture not created (this may happen if the media is removed)*/
	if (ctx->h_texture && ctx->h_texture->needs_refresh) ctx->redraw_flags |= CTX_TEXTURE_DIRTY;

	if (check_transparent_skip(ctx, skipFill)) {
		VS2D_RemoveLastContext(eff->surface);
		return NULL;
	}
	ctx->num_listeners = eff->nb_listeners;
	return ctx;
}

#endif	//SVG
