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
static void RenderShape(GF_Node *node, void *rs)
{
	RenderEffect2D *eff;
	M_Shape *shape = (M_Shape *) node;
	if (!shape->geometry) return;
	eff = rs;

	if (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF) return;
	eff->appear = (GF_Node *) shape->appearance;

	gf_node_render((GF_Node *) shape->geometry, eff);
	eff->appear = NULL;
}

void R2D_InitShape(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderShape);
}


static void RenderCircle(GF_Node *node, void *rs)
{
	DrawableContext *ctx;
	Drawable *cs = gf_node_get_private(node);
	RenderEffect2D *eff = rs;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, 0, 0, ((M_Circle *) node)->radius * 2, ((M_Circle *) node)->radius * 2);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	
	drawctx_store_original_bounds(ctx);
	drawable_finalize_render(ctx, eff);
}

void R2D_InitCircle(Render2D *sr, GF_Node *node)
{
	BaseDrawStack2D(sr, node);
	gf_node_set_render_function(node, RenderCircle);
}

static void RenderEllipse(GF_Node *node, void *rs)
{
	DrawableContext *ctx;
	Drawable *cs = gf_node_get_private(node);
	RenderEffect2D *eff = rs;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		gf_path_add_ellipse(cs->path, 0, 0, ((M_Ellipse *) node)->radius.x, ((M_Ellipse *) node)->radius.y);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}
	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	
	drawctx_store_original_bounds(ctx);
	drawable_finalize_render(ctx, eff);
}

void R2D_InitEllipse(Render2D  *sr, GF_Node *node)
{
	BaseDrawStack2D(sr, node);
	gf_node_set_render_function(node, RenderEllipse);
}

static void RenderRectangle(GF_Node *node, void *reff)
{
	DrawableContext *ctx;
	Drawable *rs = gf_node_get_private(node);
	RenderEffect2D *eff = reff;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(rs);
		gf_path_add_rect_center(rs->path, 0, 0, ((M_Rectangle *) node)->size.x, ((M_Rectangle *) node)->size.y);
		gf_node_dirty_clear(node, 0);
		rs->node_changed = 1;
	}
	ctx = drawable_init_context(rs, eff);
	if (!ctx) return;
	
	ctx->transparent = 0;
	/*if not filled, transparent*/
	if (!ctx->aspect.filled) {
		ctx->transparent = 1;
	} 
	/*if alpha, transparent*/
	else if (GF_COL_A(ctx->aspect.fill_color) != 0xFF) {
		ctx->transparent = 1;
	} 
	/*if rotated, transparent (doesn't fill bounds)*/
	else if (ctx->transform.m[1] || ctx->transform.m[3]) {
		ctx->transparent = 1;
	}
	else if (!eff->color_mat.identity) 
		ctx->transparent = 1;	

	drawctx_store_original_bounds(ctx);
	drawable_finalize_render(ctx, eff);
}

void R2D_InitRectangle(Render2D  *sr, GF_Node *node)
{
	BaseDrawStack2D(sr, node);
	gf_node_set_render_function(node, RenderRectangle);
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

static void RenderCurve2D(GF_Node *node, void *rs)
{
	DrawableContext *ctx;
	M_Curve2D *c2D = (M_Curve2D *)node;
	Drawable *cs = gf_node_get_private(node);
	RenderEffect2D *eff = rs;

	if (!c2D->point) return;


	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		cs->path->fineness = c2D->fineness;
		if (eff->surface->render->compositor->high_speed)  cs->path->fineness /= 2;
		build_curve2D(cs, c2D);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}

	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	drawctx_store_original_bounds(ctx);
	drawable_finalize_render(ctx, eff);
}

void R2D_InitCurve2D(Render2D  *sr, GF_Node *node)
{
	BaseDrawStack2D(sr, node);
	gf_node_set_render_function(node, RenderCurve2D);
}


typedef struct _bitmap_stack
{
	/*rendering node*/
	Drawable *graph;
} BitmapStack;

static void DestroyBitmap(GF_Node *n)
{
	BitmapStack *st = (BitmapStack *)gf_node_get_private(n);

	DeleteDrawableNode(st->graph);

	/*destroy hw surface if any*/

	free(st);
}

static void Bitmap_BuildGraph(BitmapStack *st, DrawableContext *ctx, RenderEffect2D *eff)
{
	M_Bitmap *bmp = (M_Bitmap *)st->graph->owner;
	GF_Matrix2D mat;
	Fixed w, h;

	w = INT2FIX(ctx->h_texture->width);
	/*if we have a PAR update it!!*/
	if (ctx->h_texture->pixel_ar) {
		u32 n = (ctx->h_texture->pixel_ar>>16) & 0xFF;
		u32 d = (ctx->h_texture->pixel_ar) & 0xFF;
		w = INT2FIX((ctx->h_texture->width * n) / d);
	}
	h = INT2FIX(ctx->h_texture->height);

	/*get size with scale*/
	drawable_reset_path(st->graph);
	/*reverse meterMetrics conversion*/
	gf_mx2d_init(mat);
	if (!eff->is_pixel_metrics) gf_mx2d_add_scale(&mat, eff->min_hsize, eff->min_hsize);
	gf_mx2d_inverse(&mat);
	/*the spec says STRICTLY positive or -1, but some content use 0...*/
	gf_mx2d_add_scale(&mat, (bmp->scale.x>=0) ? bmp->scale.x : FIX_ONE, (bmp->scale.y>=0) ? bmp->scale.y : FIX_ONE);
	gf_mx2d_apply_coords(&mat, &w, &h);
	gf_path_add_rect_center(st->graph->path, 0, 0, w, h);

	ctx->original = gf_rect_center(w, h);
}

static void RenderBitmap(GF_Node *node, void *rs)
{
	DrawableContext *ctx;
	BitmapStack *st = (BitmapStack *)gf_node_get_private(node);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	/*we never cache anything with bitmap...*/
	gf_node_dirty_clear(node, 0);

	ctx = drawable_init_context(st->graph, eff);
	if (!ctx || !ctx->h_texture ) return;
	/*always build the path*/
	Bitmap_BuildGraph(st, ctx, eff);
	/*even if set this is not true*/
	ctx->aspect.has_line = 0;
	/*this is to make sure we don't fill the path if the texture is transparent*/
	ctx->aspect.filled = 0;
	ctx->aspect.pen_props.width = 0;
	ctx->no_antialias = 1;

	ctx->transparent = 0;
	/*if clipper then transparent*/

	if (ctx->h_texture->transparent) {
		ctx->transparent = 1;
	} else {
		M_Appearance *app = (M_Appearance *)ctx->appear;
		if ( app->material && (gf_node_get_tag((GF_Node *)app->material)==TAG_MPEG4_MaterialKey) ) {
			if (((M_MaterialKey*)app->material)->isKeyed) ctx->transparent = 1;
		} else if (!eff->color_mat.identity) ctx->transparent = 1;
	}

	/*bounds are stored when building graph*/	
	drawable_finalize_render(ctx, eff);
}

static void DrawBitmap(DrawableContext *ctx)
{
	u8 alpha;
	Render2D *sr;
	Bool use_hw, has_scale;
	M_Bitmap *bmp = (M_Bitmap *) ctx->node->owner;
	BitmapStack *st = (BitmapStack *) gf_node_get_private(ctx->node->owner);


	sr = ctx->surface->render;
	/*bitmaps are NEVER rotated (forbidden in spec). In case a rotation was done we still display (reset the skew components)*/
	ctx->transform.m[1] = ctx->transform.m[3] = 0;

	has_scale = 0;
	if (bmp->scale.x>0 && (bmp->scale.x!=FIX_ONE) ) has_scale = 1;
	if (bmp->scale.y>0 && (bmp->scale.y!=FIX_ONE) ) has_scale = 1;

	use_hw = 1;
	alpha = GF_COL_A(ctx->aspect.fill_color);

	/*check if driver can handle alpha blit*/
	if (alpha!=255) {
		use_hw = sr->compositor->video_out->bHasAlpha;
		if (has_scale) use_hw = sr->compositor->video_out->bHasAlphaStretch;
	}
	/*for MatteTexture*/
	if (!ctx->cmat.identity || ctx->h_texture->has_cmat) use_hw = 0;

	/*to do - materialKey*/

	/*this is not a native texture, use graphics*/
	if (!ctx->h_texture->data) {
		use_hw = 0;
	} else {
		if (!ctx->surface->SupportsFormat || !ctx->surface->DrawBitmap ) use_hw = 0;
		/*format not supported directly, try with brush*/
		else if (!ctx->surface->SupportsFormat(ctx->surface, ctx->h_texture->pixelformat) ) use_hw = 0;
	}

	/*no HW, fall back to the graphics driver*/
	if (!use_hw) {
		drawable_reset_path(st->graph);
		gf_path_add_rect_center(st->graph->path, 0, 0, ctx->original.width, ctx->original.height);
		ctx->no_antialias = 1;
		VS2D_TexturePath(ctx->surface, st->graph->path, ctx);
		return;
	}

	/*direct rendering, render without clippers */
	if (ctx->surface->render->top_effect->trav_flags & TF_RENDER_DIRECT) {
		ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &ctx->clip, &ctx->unclip);
	}
	/*render bitmap for all dirty rects*/
	else {
		u32 i;
		GF_IRect clip;
		for (i=0; i<ctx->surface->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
			if (ctx->surface->draw_node_index<ctx->surface->to_redraw.opaque_node_index[i]) continue;
			clip = ctx->clip;
			gf_irect_intersect(&clip, &ctx->surface->to_redraw.list[i]);
			if (clip.width && clip.height) {
				ctx->surface->DrawBitmap(ctx->surface, ctx->h_texture, &clip, &ctx->unclip);
			}
		}
	}

}

static Bool Bitmap_PointOver(DrawableContext *ctx, Fixed x, Fixed y, Bool check_outline)
{
	return 1;
}

void R2D_InitBitmap(Render2D  *sr, GF_Node *node)
{
	BitmapStack *st = malloc(sizeof(BitmapStack));
	st->graph = NewDrawableNode();

	gf_sr_traversable_setup(st->graph, node, sr->compositor);
	st->graph->Draw = DrawBitmap;
	st->graph->IsPointOver = Bitmap_PointOver;
	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderBitmap);
	gf_node_set_predestroy_function(node, DestroyBitmap);
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
}

static void RenderPointSet2D(GF_Node *node, void *rs)
{
	DrawableContext *ctx;
	M_PointSet2D *ps2D = (M_PointSet2D *)node;
	Drawable *cs = gf_node_get_private(node);
	RenderEffect2D *eff = rs;

	if (!ps2D->coord) return;

	if (gf_node_dirty_get(node)) {
		drawable_reset_path(cs);
		build_graph(cs, &eff->transform, ps2D);
		gf_node_dirty_clear(node, 0);
		cs->node_changed = 1;
	}

	ctx = drawable_init_context(cs, eff);
	if (!ctx) return;
	ctx->aspect.filled = 1;
	ctx->aspect.has_line = 0;
	drawctx_store_original_bounds(ctx);
	drawable_finalize_render(ctx, eff);
}

static void PointSet2D_Draw(DrawableContext *ctx)
{
	GF_Path *path;
	Fixed alpha, w, h;
	u32 i;
	SFColor col;
	M_PointSet2D *ps2D = (M_PointSet2D *)ctx->node->owner;
	M_Coordinate2D *coord = (M_Coordinate2D*) ps2D->coord;
	M_Color *color = (M_Color *) ps2D->color;

	/*never outline PS2D*/
	ctx->path_stroke = 1;
	if (!color || color->color.count<coord->point.count) {
		/*no texturing*/
		VS2D_DrawPath(ctx->surface, ctx->node->path, ctx, NULL, NULL);
		return;
	}

	get_point_size(&ctx->transform, &w, &h);

	path = gf_path_new();
	alpha = INT2FIX(GF_COL_A(ctx->aspect.line_color)) / 255;
	for (i = 0; i < coord->point.count; i++) {
		col = color->color.vals[i];
		ctx->aspect.line_color = GF_COL_ARGB_FIXED(alpha, col.red, col.green, col.blue);
		gf_path_add_rect_center(path, coord->point.vals[i].x, coord->point.vals[i].y, w, h);
		VS2D_DrawPath(ctx->surface, path, ctx, NULL, NULL);
		gf_path_reset(path);
		ctx->path_filled = 0;
	}
	gf_path_del(path);
}

void R2D_InitPointSet2D(Render2D  *sr, GF_Node *node)
{
	Drawable *stack = BaseDrawStack2D(sr, node);
	/*override draw*/
	stack->Draw = PointSet2D_Draw;
	gf_node_set_render_function(node, RenderPointSet2D);
}

static void RenderPathExtrusion(GF_Node *node, void *rs)
{
	GF_FieldInfo field;
	GF_Node *geom;
	if (gf_node_get_field(node, 0, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFNODE) return;
	geom = * (GF_Node **) field.far_ptr;
	if (geom) gf_node_render(geom, rs);
}

void R2D_InitPathExtrusion(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderPathExtrusion);
}
