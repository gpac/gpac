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



#include "stacks2d.h"
#include "visualsurface2d.h"


/*
		Shape 
*/
static void RenderShape(GF_Node *node, void *rs, Bool is_destroy)
{
	RenderEffect2D *eff;
	M_Shape *shape = (M_Shape *) node;
	if (is_destroy || !shape->geometry) return;
	eff = (RenderEffect2D *)rs;

	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return;
	eff->appear = (GF_Node *) shape->appearance;

	gf_node_render((GF_Node *) shape->geometry, eff);
	eff->appear = NULL;
}

void R2D_InitShape(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, RenderShape);
}


static void RenderCircle(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
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
	
	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, 0, 0, ((M_Circle *) node)->radius * 2, ((M_Circle *) node)->radius * 2);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	
	drawable_finalize_render(ctx, eff, NULL);
}

void R2D_InitCircle(Render2D *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, RenderCircle);
}

static void RenderEllipse(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
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

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, 0, 0, ((M_Ellipse *) node)->radius.x, ((M_Ellipse *) node)->radius.y);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}
	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	
	drawable_finalize_render(ctx, eff, NULL);
}

void R2D_InitEllipse(Render2D  *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, RenderEllipse);
}


static Bool txtrans_identity(GF_Node *appear)
{
	if (!appear || !((M_Appearance*)appear)->textureTransform) return 1;
	/*we could optimize, but let's assume that if a transform is used, it is not identity...*/
	return 0;
}

void R2D_DrawRectangle(RenderEffect2D *eff)
{
	DrawableContext *ctx = eff->ctx;
	if (!ctx->h_texture || !ctx->h_texture->stream || ctx->transform.m[1] || ctx->transform.m[3] || !txtrans_identity(ctx->appear) ) {
		VS2D_TexturePath(eff->surface, ctx->drawable->path, ctx);
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
	} else {
		GF_Rect unclip;
		GF_IRect clip, unclip_pix;
		u8 alpha = GF_COL_A(ctx->aspect.fill_color);
		/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
		if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);

		/*get image size WITHOUT line size*/
		gf_path_get_bounds(ctx->drawable->path, &unclip);
		gf_mx2d_apply_rect(&ctx->transform, &unclip);
		unclip_pix = clip = gf_rect_pixelize(&unclip);
		gf_irect_intersect(&clip, &ctx->bi->clip);

		/*direct rendering, render without clippers */
		if (eff->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
			eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &clip, &unclip, alpha, NULL, ctx->col_mat);
		}
		/*render bitmap for all dirty rects*/
		else {
			u32 i;
			GF_IRect a_clip;
			for (i=0; i<eff->surface->to_redraw.count; i++) {
				/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
				if (eff->surface->draw_node_index < eff->surface->to_redraw.opaque_node_index[i]) continue;
#endif
				a_clip = clip;
				gf_irect_intersect(&a_clip, &eff->surface->to_redraw.list[i]);
				if (a_clip.width && a_clip.height) {
					eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &a_clip, &unclip, alpha, NULL, ctx->col_mat);
				}
			}
		}
		ctx->flags |= CTX_PATH_FILLED;
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
	}
}

static void RenderRectangle(GF_Node *node, void *reff, Bool is_destroy)
{
	DrawableContext *ctx;
	Drawable *rs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)reff;

	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		R2D_DrawRectangle(eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		drawable_pick(eff);
		return;
	}

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(rs);
		gf_path_add_rect_center(rs->path, 0, 0, ((M_Rectangle *) node)->size.x, ((M_Rectangle *) node)->size.y);
		gf_node_dirty_clear(node, 0);
		rs->flags |= DRAWABLE_HAS_CHANGED;
	}
	ctx = drawable_init_context(rs, eff);
	if (!ctx) return;
	
	/*if alpha or not filled, transparent*/
	if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {
	} 
	/*if texture transparent, transparent*/
	else if (ctx->h_texture && ctx->h_texture->transparent) {
	} 
	/*if rotated, transparent (doesn't fill bounds)*/
	else if (ctx->transform.m[1] || ctx->transform.m[3]) {
	}
	/*TODO check matrix for alpha*/
	else if (!eff->color_mat.identity) {
	}
	/*otherwsie, not transparent*/
	else {
		ctx->flags &= ~CTX_IS_TRANSPARENT;	
	}

	drawable_finalize_render(ctx, eff, NULL);
}

void R2D_InitRectangle(Render2D  *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, RenderRectangle);
}


//#define CHECK_VALID_C2D(nbPts) if (cur_index+nbPts>=pt_count) { gd->path_reset(cs->path); return; }
#define CHECK_VALID_C2D(nbPts)

static void build_curve2D(Drawable *cs, M_Curve2D *c2D)
{
	SFVec2f orig, ct_orig, ct_end, end;
	u32 cur_index, i, remain, type_count, pt_count;
	SFVec2f *pts;
	M_Coordinate2D *coord = (M_Coordinate2D *)c2D->point;

	pts = coord->point.vals;
	if (!pts) 
		return;

	cur_index = c2D->type.count ? 1 : 0;
	/*if the first type is a moveTo skip initial moveTo*/
	i=0;
	i=0;
	if (cur_index) {
		while (c2D->type.vals[i]==0) i++;
	}
	ct_orig = orig = pts[i];

	gf_path_add_move_to(cs->path, orig.x, orig.y);

	pt_count = coord->point.count;
	type_count = c2D->type.count;
	for (; i<type_count; i++) {

		switch (c2D->type.vals[i]) {
		/*moveTo, 1 point*/
		case 0:
			CHECK_VALID_C2D(0);
			orig = pts[cur_index];
			if (i) gf_path_add_move_to(cs->path, orig.x, orig.y);
			cur_index += 1;
			break;
		/*lineTo, 1 point*/
		case 1:
			CHECK_VALID_C2D(0);
			end = pts[cur_index];
			gf_path_add_line_to(cs->path, end.x, end.y);
			orig = end;
			cur_index += 1;
			break;
		/*curveTo, 3 points*/
		case 2:
			CHECK_VALID_C2D(2);
			ct_orig = pts[cur_index];
			ct_end = pts[cur_index+1];
			end = pts[cur_index+2];
			gf_path_add_cubic_to(cs->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*nextCurveTo, 2 points (cf spec)*/
		case 3:
			CHECK_VALID_C2D(1);
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			ct_end = pts[cur_index];
			end = pts[cur_index+1];
			gf_path_add_cubic_to(cs->path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;
		
		/*all XCurve2D specific*/

		/*CW and CCW ArcTo*/
		case 4:
		case 5:
			CHECK_VALID_C2D(2);
			ct_orig = pts[cur_index];
			ct_end = pts[cur_index+1];
			end = pts[cur_index+2];
			gf_path_add_arc_to(cs->path, end.x, end.y, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, (c2D->type.vals[i]==5) ? 1 : 0);
			cur_index += 3;
			ct_orig = ct_end;
			orig = end;
			break;
		/*ClosePath*/
		case 6:
			gf_path_close(cs->path);
			break;
		/*quadratic CurveTo, 2 points*/
		case 7:
			CHECK_VALID_C2D(1);
			ct_end = pts[cur_index];
			end = pts[cur_index+1];
			gf_path_add_quadratic_to(cs->path, ct_end.x, ct_end.y, end.x, end.y);
			cur_index += 2;
			ct_orig = ct_end;
			orig = end;
			break;
		}
	}

	/*what's left is an N-bezier spline*/
	if (pt_count > cur_index) {
		/*first moveto*/
		if (!cur_index) cur_index++;

		remain = pt_count - cur_index;

		if (remain>1)
			gf_path_add_bezier(cs->path, &pts[cur_index], remain);
	}
}

static void RenderCurve2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_Curve2D *c2D = (M_Curve2D *)node;
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

	if (!c2D->point) return;


	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		cs->path->fineness = c2D->fineness;
		if (eff->surface->render->compositor->high_speed)  cs->path->fineness /= 2;
		build_curve2D(cs, c2D);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}

	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	drawable_finalize_render(ctx, eff, NULL);
}

void R2D_InitCurve2D(Render2D  *sr, GF_Node *node)
{
	drawable_stack_new(sr, node);
	gf_node_set_callback_function(node, RenderCurve2D);
}


typedef struct _bitmap_stack
{
	/*rendering node*/
	Drawable *graph;
} BitmapStack;


static void Bitmap_BuildGraph(BitmapStack *st, DrawableContext *ctx, RenderEffect2D *eff, GF_Rect *out_rc)
{
	Fixed w, h;
	M_Bitmap *bmp = (M_Bitmap *)ctx->drawable->node;

	w = INT2FIX(ctx->h_texture->width);
	/*if we have a PAR update it!!*/
	if (ctx->h_texture->pixel_ar) {
		u32 n = (ctx->h_texture->pixel_ar>>16) & 0xFFFF;
		u32 d = (ctx->h_texture->pixel_ar) & 0xFFFF;
		w = INT2FIX((ctx->h_texture->width * n) / d);
	}
	h = INT2FIX(ctx->h_texture->height);

	/*the spec says STRICTLY positive or -1, but some content use 0...*/
	w = gf_mulfix(w, ((bmp->scale.x>=0) ? bmp->scale.x : FIX_ONE) );
	h = gf_mulfix(h, ((bmp->scale.y>=0) ? bmp->scale.y : FIX_ONE) );
	/*reverse meterMetrics conversion*/
	if (!eff->is_pixel_metrics) {
		w = gf_divfix(w, eff->min_hsize);
		h = gf_divfix(h, eff->min_hsize);
	}

	/*get size with scale*/
	drawable_reset_path(st->graph);
	gf_path_add_rect_center(st->graph->path, 0, 0, w, h);

	*out_rc = gf_rect_center(w, h);
}

static void DrawBitmap(GF_Node *node, RenderEffect2D *eff)
{
	u8 alpha;
	u32 keyColor;
	Render2D *sr;
	Bool use_blit, has_key;
	DrawableContext *ctx = eff->ctx;
	BitmapStack *st = (BitmapStack *) gf_node_get_private(node);


	sr = eff->surface->render;
	/*bitmaps are NEVER rotated (forbidden in spec). In case a rotation was done we still display (reset the skew components)*/
	ctx->transform.m[1] = ctx->transform.m[3] = 0;

	use_blit = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);
	/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
	if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);

	/*materialKey*/
	has_key = 0;
	if (ctx->appear) {
		M_Appearance *app = (M_Appearance *)ctx->appear;
		if ( app->material && (gf_node_get_tag((GF_Node *)app->material)==TAG_MPEG4_MaterialKey) ) {
			if (((M_MaterialKey*)app->material)->isKeyed && ((M_MaterialKey*)app->material)->transparency) {
				SFColor col = ((M_MaterialKey*)app->material)->keyColor;
				has_key = 1;
				keyColor = GF_COL_ARGB_FIXED(
								FIX_ONE - ((M_MaterialKey*)app->material)->transparency,
								col.red, col.green, col.blue);
			}
		}
	}

	/*this is not a native texture, use graphics*/
	if (!ctx->h_texture->data) {
		use_blit = 0;
	} else {
		if (!eff->surface->SupportsFormat || !eff->surface->DrawBitmap ) use_blit = 0;
		/*format not supported directly, try with brush*/
		else if (!eff->surface->SupportsFormat(eff->surface, ctx->h_texture->pixelformat) ) use_blit = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_blit) {
		Fixed w, h;
		GF_Matrix2D _mat;
		w = ctx->bi->unclip.width;
		h = ctx->bi->unclip.height;
		gf_mx2d_copy(_mat, ctx->transform);
		gf_mx2d_inverse(&_mat);
		gf_mx2d_apply_coords(&_mat, &w, &h);

		drawable_reset_path(st->graph);
		gf_path_add_rect_center(st->graph->path, 0, 0, w, h);
		ctx->flags |= CTX_NO_ANTIALIAS;
		VS2D_TexturePath(eff->surface, st->graph->path, ctx);
		return;
	}

	/*direct rendering, render without clippers */
	if (eff->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &ctx->bi->clip, &ctx->bi->unclip, alpha, has_key ? &keyColor : NULL, ctx->col_mat);
	}
	/*render bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<eff->surface->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (eff->surface->draw_node_index < eff->surface->to_redraw.opaque_node_index[i]) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &eff->surface->to_redraw.list[i]);
			if (clip.width && clip.height) {
				eff->surface->DrawBitmap(eff->surface, ctx->h_texture, &clip, &ctx->bi->unclip, alpha, has_key ? &keyColor : NULL, ctx->col_mat);
			}
		}
	}
}

static void RenderBitmap(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Rect rc;
	DrawableContext *ctx;
	BitmapStack *st = (BitmapStack *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (is_destroy) {
		drawable_del(st->graph);
		free(st);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		DrawBitmap(node, eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		eff->is_over = 1;
		return;
	}

	/*we never cache anything with bitmap...*/
	gf_node_dirty_clear(node, 0);

	ctx = drawable_init_context(st->graph, eff);
	if (!ctx || !ctx->h_texture ) {
		VS2D_RemoveLastContext(eff->surface);
		return;
	}
	/*always build the path*/
	Bitmap_BuildGraph(st, ctx, eff, &rc);
	/*even if set this is not true*/
	ctx->aspect.pen_props.width = 0;
	ctx->flags |= CTX_NO_ANTIALIAS;

	ctx->flags &= ~CTX_IS_TRANSPARENT;
	/*if clipper then transparent*/

	if (ctx->h_texture->transparent) {
		ctx->flags |= CTX_IS_TRANSPARENT;
	} else {
		M_Appearance *app = (M_Appearance *)ctx->appear;
		if ( app->material && (gf_node_get_tag((GF_Node *)app->material)==TAG_MPEG4_MaterialKey) ) {
			if (((M_MaterialKey*)app->material)->isKeyed && ((M_MaterialKey*)app->material)->transparency)
				ctx->flags |= CTX_IS_TRANSPARENT;
		} 
		else if (!eff->color_mat.identity) {
			ctx->flags |= CTX_IS_TRANSPARENT;
		} else {
			u8 alpha = GF_COL_A(ctx->aspect.fill_color);
			/*THIS IS A HACK, will not work when setting filled=0, transparency and XLineProps*/
			if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);
			if (alpha < 0xFF) ctx->flags |= CTX_IS_TRANSPARENT;
		}
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff, &rc);
}


void R2D_InitBitmap(Render2D  *sr, GF_Node *node)
{
	BitmapStack *st = (BitmapStack *)malloc(sizeof(BitmapStack));
	st->graph = drawable_new();
	st->graph->node = node;
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, RenderBitmap);
}

/*
	Note on point set 2D: this is a very bad node and should be avoided in DEF/USE, since the size 
	of the rectangle representing the pixel shall always be 1 pixel w/h on the final surface, therefore
	the path object is likely not the same depending on transformation context...

*/

static void get_point_size(GF_Matrix2D *mat, Fixed *w, Fixed *h)
{
	GF_Point2D pt;
	pt.x = mat->m[0] + mat->m[1];
	pt.y = mat->m[3] + mat->m[4];
	*w = *h = gf_divfix(FLT2FIX(1.41421356f) , gf_v2d_len(&pt));
}

static void build_graph(Drawable *cs, GF_Matrix2D *mat, M_PointSet2D *ps2D)
{
	u32 i;
	Fixed w, h;
	M_Coordinate2D *coord = (M_Coordinate2D *)ps2D->coord;
	get_point_size(mat, &w, &h);
	/*for PS2D don't add to avoid too  much antialiasing, just try to fill the given pixel*/
	for (i=0; i < coord->point.count; i++) 
		gf_path_add_rect(cs->path, coord->point.vals[i].x, coord->point.vals[i].y, w, h);

	cs->path->flags |= GF_PATH_FILL_ZERO_NONZERO;
}

static void PointSet2D_Draw(GF_Node *node, RenderEffect2D *eff)
{
	GF_Path *path;
	Fixed alpha, w, h;
	u32 i;
	SFColor col;
	DrawableContext *ctx = eff->ctx;
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	M_Coordinate2D *coord = (M_Coordinate2D*) ps2D->coord;
	M_Color *color = (M_Color *) ps2D->color;

	/*never outline PS2D*/
	ctx->flags |= CTX_PATH_STROKE;
	if (!color || color->color.count<coord->point.count) {
		/*no texturing*/
		VS2D_DrawPath(eff->surface, ctx->drawable->path, ctx, NULL, NULL);
		return;
	}

	get_point_size(&ctx->transform, &w, &h);

	path = gf_path_new();
	alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
	for (i = 0; i < coord->point.count; i++) {
		col = color->color.vals[i];
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
		gf_path_add_rect_center(path, coord->point.vals[i].x, coord->point.vals[i].y, w, h);
		VS2D_DrawPath(eff->surface, path, ctx, NULL, NULL);
		gf_path_reset(path);
		ctx->flags &= ~CTX_PATH_FILLED;
	}
	gf_path_del(path);
}

static void RenderPointSet2D(GF_Node *node, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	Drawable *cs = (Drawable *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;
	
	if (is_destroy) {
		DestroyDrawableNode(node);
		return;
	}
	if (eff->traversing_mode==TRAVERSE_DRAW) {
		PointSet2D_Draw(node, eff);
		return;
	}
	else if (eff->traversing_mode==TRAVERSE_PICK) {
		return;
	}

	if (!ps2D->coord) return;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		build_graph(cs, &eff->transform, ps2D);
		gf_node_dirty_clear(node, 0);
		cs->flags |= DRAWABLE_HAS_CHANGED;
	}

	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	drawable_finalize_render(ctx, eff, NULL);
}


void R2D_InitPointSet2D(Render2D  *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, RenderPointSet2D);
}

static void RenderPathExtrusion(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_FieldInfo field;
	GF_Node *geom;
	if (is_destroy) return;
	
	if (gf_node_get_field(node, 0, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFNODE) return;
	geom = * (GF_Node **) field.far_ptr;
	if (geom) gf_node_render(geom, rs);
}

void R2D_InitPathExtrusion(Render2D *sr, GF_Node *node)
{
	gf_node_set_callback_function(node, RenderPathExtrusion);
}
