/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
#include "drawable.h"
#include "nodes_stacks.h"
#include "texturing.h"
#include <gpac/options.h>

//#define SKIP_DRAW

GF_Err visual_2d_init_raster(GF_VisualManager *visual)
{
	GF_Raster2D *raster = visual->compositor->rasterizer;
	if (!visual->raster_surface) {
		visual->raster_surface = raster->surface_new(raster, visual->center_coords);
		if (!visual->raster_surface) return GF_IO_ERR;
	}
	return visual->GetSurfaceAccess(visual);
}

void visual_2d_release_raster(GF_VisualManager *visual)
{
	if (visual->raster_surface) {
		if (visual->compositor->rasterizer->surface_flush) 
			visual->compositor->rasterizer->surface_flush(visual->raster_surface);

		visual->ReleaseSurfaceAccess(visual);
	}
}


void visual_2d_clear(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (!visual->raster_surface) return;
	
	if (!BackColor && !visual->offscreen) BackColor = visual->compositor->back_color;

	visual->compositor->rasterizer->surface_clear(visual->raster_surface, rc, BackColor);
}

static void draw_clipper(GF_VisualManager *visual, struct _drawable_context *ctx)
{
	GF_PenSettings clipset;
	GF_Path *clippath, *cliper;
	GF_Raster2D *raster = visual->compositor->rasterizer;

	if (ctx->flags & CTX_IS_BACKGROUND) return;

	memset(&clipset, 0, sizeof(GF_PenSettings));
	clipset.width = 2*FIX_ONE;

	clippath = gf_path_new();
	gf_path_add_rect_center(clippath, ctx->bi->unclip.x + ctx->bi->unclip.width/2, ctx->bi->unclip.y - ctx->bi->unclip.height/2, ctx->bi->unclip.width, ctx->bi->unclip.height);
	cliper = gf_path_get_outline(clippath, clipset);
	gf_path_del(clippath);
	raster->surface_set_matrix(visual->raster_surface, NULL);
	raster->surface_set_clipper(visual->raster_surface, NULL);
	raster->surface_set_path(visual->raster_surface, cliper);
	raster->stencil_set_brush_color(visual->raster_brush, 0xFF000000);
	raster->surface_fill(visual->raster_surface, visual->raster_brush);
	gf_path_del(cliper);
}

static void visual_2d_fill_path(GF_VisualManager *visual, DrawableContext *ctx, GF_STENCIL stencil, GF_TraverseState *tr_state)
{
	GF_IRect clip;
	GF_Raster2D *raster = visual->compositor->rasterizer;

	/*background & direct drawing : use ctx clip*/
	if ((ctx->flags & CTX_IS_BACKGROUND) || tr_state->direct_draw) {
		if (ctx->bi->clip.width && ctx->bi->clip.height) {
			//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Redrawing node %s (direct draw)\n", gf_node_get_log_name(ctx->drawable->node) ));
			raster->surface_set_clipper(visual->raster_surface, &ctx->bi->clip);
			raster->surface_fill(visual->raster_surface, stencil);

			visual->has_modif = 1;
		}
	} 
	/*indirect drawing, draw path in all dirty areas*/
	else {
		u32 i;
		for (i=0; i<visual->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (visual->draw_node_index<visual->to_redraw.opaque_node_index[i]) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &visual->to_redraw.list[i]);
			if (clip.width && clip.height) {
				//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Redrawing node %s (indirect draw)\n", gf_node_get_log_name(ctx->drawable->node) ));
				raster->surface_set_clipper(visual->raster_surface, &clip);
				raster->surface_fill(visual->raster_surface, stencil);

				visual->has_modif = 1;
			}
		}
	}
}

static void visual_2d_set_options(GF_Compositor *compositor, GF_SURFACE rend, Bool forText, Bool no_antialias)
{
	GF_Raster2D *raster = compositor->rasterizer;
	if (no_antialias) {
		raster->surface_set_raster_level(rend, compositor->high_speed ? GF_RASTER_HIGH_SPEED : GF_RASTER_MID);
	} else {
		switch (compositor->antiAlias) {
		case GF_ANTIALIAS_NONE:
			raster->surface_set_raster_level(rend, GF_RASTER_HIGH_SPEED);
			break;
		case GF_ANTIALIAS_TEXT:
			if (forText) {
				raster->surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			} else {
				raster->surface_set_raster_level(rend, compositor->high_speed ? GF_RASTER_HIGH_QUALITY : GF_RASTER_MID);
			}
			break;
		case GF_ANTIALIAS_FULL:
		default:
			raster->surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			break;
		}
	}
}


static void visual_2d_get_texture_transform(GF_Node *__appear, GF_TextureHandler *txh, GF_Matrix2D *mat, Bool line_texture, Fixed final_width, Fixed final_height)
{
	u32 node_tag;
	M_Appearance *appear;		
	GF_Node *txtrans = NULL;
	gf_mx2d_init(*mat);

	if (!__appear || !txh) return;
	appear = (M_Appearance *)__appear;

	if (!line_texture) {
		if (!appear->textureTransform) return;
		txtrans = appear->textureTransform;
	} else {
		if (gf_node_get_tag(appear->material) != TAG_MPEG4_Material2D) return;
		if (gf_node_get_tag(((M_Material2D *)appear->material)->lineProps) != TAG_MPEG4_XLineProperties) return;
		txtrans = ((M_XLineProperties *) ((M_Material2D *)appear->material)->lineProps)->textureTransform;
	}
	if (!txtrans) return;

	/*gradient doesn't need bounds info in texture transform*/
	if (txh->compute_gradient_matrix) {
		final_width = final_height = FIX_ONE;
	}
	node_tag = gf_node_get_tag(txtrans);
	if (node_tag==TAG_MPEG4_TextureTransform) {
		/*VRML: Tc' = -C � S � R � C � T � Tc*/
		M_TextureTransform *txt = (M_TextureTransform *) txtrans;
		SFVec2f scale = txt->scale;
		if (!scale.x) scale.x = FIX_ONE/100;
		if (!scale.y) scale.y = FIX_ONE/100;

		gf_mx2d_add_translation(mat, -gf_mulfix(txt->center.x, final_width), -gf_mulfix(txt->center.y, final_height) );
		gf_mx2d_add_scale(mat, scale.x, scale.y);
		gf_mx2d_add_rotation(mat, 0, 0, txt->rotation);
		gf_mx2d_add_translation(mat, gf_mulfix(txt->center.x, final_width), gf_mulfix(txt->center.y, final_height) );
		gf_mx2d_add_translation(mat, gf_mulfix(txt->translation.x, final_width), gf_mulfix(txt->translation.y, final_height) );
		/*and inverse the matrix (this is texture transform, cf VRML)*/
		gf_mx2d_inverse(mat);
		return;
	}
	if (node_tag==TAG_MPEG4_TransformMatrix2D) {
		tr_mx2d_get_matrix((GF_Node *) txtrans, mat);
		mat->m[2] = gf_mulfix(mat->m[2], final_width);
		mat->m[5] = gf_mulfix(mat->m[5], final_height);
		gf_mx2d_inverse(mat);
		return;
	}
}


static void visual_2d_draw_gradient(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_TraverseState *tr_state, GF_Matrix2D *ext_mx, GF_Rect *orig_bounds)
{
	GF_Rect rc;
	GF_STENCIL stencil;
	GF_Matrix2D g_mat, txt_mat;
	GF_Raster2D *raster = visual->compositor->rasterizer;

	if (!txh) txh = ctx->aspect.fill_texture;
	
	gf_path_get_bounds(path, &rc);
	if (!rc.width || !rc.height || !txh->tx_io) return;

	if (orig_bounds) {
		txh->compute_gradient_matrix(txh, orig_bounds, &g_mat, 0);	
	} else {
		txh->compute_gradient_matrix(txh, &rc, &g_mat, 0);	
	}
	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	if (ctx->flags & CTX_HAS_APPEARANCE) {
		visual_2d_get_texture_transform(ctx->appear, txh, &txt_mat, (txh == ctx->aspect.fill_texture) ? 0 : 1, INT2FIX(txh->width), INT2FIX(txh->height));
		gf_mx2d_add_matrix(&g_mat, &txt_mat);
	}
	/*move to bottom-left corner of bounds */
	if (ext_mx) gf_mx2d_add_matrix(&g_mat, ext_mx);
	if (orig_bounds) gf_mx2d_add_translation(&g_mat, (orig_bounds->x), (orig_bounds->y - orig_bounds->height));

	gf_mx2d_add_matrix(&g_mat, &ctx->transform);
	
	raster->stencil_set_matrix(stencil, &g_mat);
	raster->stencil_set_color_matrix(stencil, ctx->col_mat);

	/*MPEG-4/VRML context or no fill info*/	
	if (ctx->flags & CTX_HAS_APPEARANCE || !ctx->aspect.fill_color)
		raster->stencil_set_alpha(stencil, 0xFF);
	else
		raster->stencil_set_alpha(stencil, GF_COL_A(ctx->aspect.fill_color) );

	raster->surface_set_matrix(visual->raster_surface, &ctx->transform);
	txh->flags |= GF_SR_TEXTURE_USED;

	raster->surface_set_path(visual->raster_surface, path);
	visual_2d_fill_path(visual, ctx, stencil, tr_state);
	raster->surface_set_path(visual->raster_surface, NULL);

	ctx->flags |= CTX_PATH_FILLED;
}



void visual_2d_texture_path_text(GF_VisualManager *visual, DrawableContext *txt_ctx, GF_Path *path, GF_Rect *object_bounds, GF_TextureHandler *txh, GF_TraverseState *tr_state)
{
	GF_STENCIL stencil;
	Fixed sS, sT;
	GF_Matrix2D gf_mx2d_txt;
	GF_Rect orig_rc;
	u8 alpha, r, g, b;
	GF_ColorMatrix cmat;
	GF_Raster2D *raster = visual->compositor->rasterizer;

	stencil = gf_sc_texture_get_stencil(txh);
	if (!stencil) return;

	visual_2d_set_options(visual->compositor, visual->raster_surface, 0, 1);

	/*get original bounds*/
	orig_rc = *object_bounds;
	
	/*get scaling ratio so that active texture view is stretched to original bounds (std 2D shape texture mapping in MPEG4)*/
	sS = gf_divfix(orig_rc.width, INT2FIX(txh->width));
	sT = gf_divfix(orig_rc.height, INT2FIX(txh->height));
	
	gf_mx2d_init(gf_mx2d_txt);
	gf_mx2d_add_scale(&gf_mx2d_txt, sS, sT);

	/*move to bottom-left corner of bounds */
	gf_mx2d_add_translation(&gf_mx2d_txt, (orig_rc.x), (orig_rc.y - orig_rc.height));

	/*move to final coordinate system*/	
	gf_mx2d_add_matrix(&gf_mx2d_txt, &txt_ctx->transform);

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	raster->stencil_set_matrix(stencil, &gf_mx2d_txt);

	alpha = GF_COL_A(txt_ctx->aspect.fill_color);
	r = GF_COL_R(txt_ctx->aspect.fill_color);
	g = GF_COL_G(txt_ctx->aspect.fill_color);
	b = GF_COL_B(txt_ctx->aspect.fill_color);

	/*if col do a cxmatrix*/
	if (!r && !g && !b) {
		raster->stencil_set_alpha(stencil, alpha);
	} else {
		raster->stencil_set_alpha(stencil, 0xFF);
		memset(cmat.m, 0, sizeof(Fixed) * 20);
		cmat.m[4] = INT2FIX(r)/255;
		cmat.m[9] = INT2FIX(g)/255;
		cmat.m[14] = INT2FIX(b)/255;
		cmat.m[18] = INT2FIX(alpha)/255;
		cmat.identity = 0;
		raster->stencil_set_color_matrix(stencil, &cmat);
	}

	raster->surface_set_matrix(visual->raster_surface, &txt_ctx->transform);
	txh->flags |= GF_SR_TEXTURE_USED;

	/*push path*/
	raster->surface_set_path(visual->raster_surface, path);

	visual_2d_fill_path(visual, txt_ctx, stencil, tr_state);
	raster->surface_set_path(visual->raster_surface, NULL);
	txt_ctx->flags |= CTX_PATH_FILLED;
}

void visual_2d_texture_path_extended(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, GF_TraverseState *tr_state)
{
	Fixed sS, sT;
	u32 tx_tile;
	GF_STENCIL tx_raster;
	GF_Matrix2D mx_texture, tex_trans;
	GF_Rect rc, orig_rc;
	GF_Raster2D *raster = visual->compositor->rasterizer;

	if (!txh) txh = ctx->aspect.fill_texture;
	if (!txh || !txh->tx_io) return;


	/*this is gradient draw*/
	if (txh->compute_gradient_matrix) {
		visual_2d_draw_gradient(visual, path, txh, ctx, tr_state, ext_mx, orig_bounds);
		return;
	}

	if (!gf_sc_texture_push_image(txh, 0, 1)) return;
	tx_raster = gf_sc_texture_get_stencil(txh);

	/*setup quality even for background (since quality concerns images)*/
	visual_2d_set_options(visual->compositor, visual->raster_surface, ctx->flags & CTX_IS_TEXT, ctx->flags & CTX_NO_ANTIALIAS);

	/*get original bounds*/
	if (orig_bounds) {
		orig_rc = *orig_bounds;
	} else {
		gf_path_get_bounds(path, &orig_rc);
	}

	/*get active texture window in pixels*/
	rc.width = INT2FIX(txh->width);
	rc.height = INT2FIX(txh->height);

	/*get scaling ratio so that active texture view is stretched to original bounds (std 2D shape texture mapping in MPEG4)*/
	sS = orig_rc.width / txh->width;
	sT = orig_rc.height / txh->height;
	
	gf_mx2d_init(mx_texture);
	gf_mx2d_add_scale(&mx_texture, sS, sT);

	/*apply texture transform*/
	if (ctx->flags & CTX_HAS_APPEARANCE) {
		visual_2d_get_texture_transform(ctx->appear, txh, &tex_trans, (txh == ctx->aspect.fill_texture) ? 0 : 1, txh->width * sS, txh->height * sT);
		gf_mx2d_add_matrix(&mx_texture, &tex_trans);
	}

	/*move to bottom-left corner of bounds */
	gf_mx2d_add_translation(&mx_texture, (orig_rc.x), (orig_rc.y - orig_rc.height));

	if (ext_mx) gf_mx2d_add_matrix(&mx_texture, ext_mx);

	/*move to final coordinate system (except background which is built directly in final coord system)*/	
	if (!(ctx->flags & CTX_IS_BACKGROUND) ) gf_mx2d_add_matrix(&mx_texture, &ctx->transform);

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	raster->stencil_set_matrix(tx_raster, &mx_texture);


	tx_tile = 0;
	if (txh->flags & GF_SR_TEXTURE_REPEAT_S) tx_tile |= GF_TEXTURE_REPEAT_S;
	if (txh->flags & GF_SR_TEXTURE_REPEAT_T) tx_tile |= GF_TEXTURE_REPEAT_T;
	if (ctx->flags & CTX_FLIPED_COORDS)
		tx_tile |= GF_TEXTURE_FLIP;
	raster->stencil_set_tiling(tx_raster, (GF_TextureTiling) tx_tile);

	if (!(ctx->flags & CTX_IS_BACKGROUND) ) {
		u8 a = GF_COL_A(ctx->aspect.fill_color);
		if (!a) a = GF_COL_A(ctx->aspect.line_color);
		/*texture alpha scale is the original material transparency, NOT the one after color transform*/
		raster->stencil_set_alpha(tx_raster, a );
		raster->stencil_set_color_matrix(tx_raster, ctx->col_mat);

		raster->surface_set_matrix(visual->raster_surface, &ctx->transform);
	} else {
		raster->surface_set_matrix(visual->raster_surface, NULL);
	}
	txh->flags |= GF_SR_TEXTURE_USED;

	/*push path & draw*/
	raster->surface_set_path(visual->raster_surface, path);
	visual_2d_fill_path(visual, ctx, tx_raster, tr_state);
	raster->surface_set_path(visual->raster_surface, NULL);


	
	ctx->flags |= CTX_PATH_FILLED;
}

void visual_2d_texture_path(GF_VisualManager *visual, GF_Path *path, struct _drawable_context *ctx, GF_TraverseState *tr_state)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (!visual->raster_surface || (ctx->flags & CTX_PATH_FILLED) || !ctx->aspect.fill_texture || visual->compositor->is_hidden) return;

	/*this is ambiguous in the spec, what if the material is filled and the texture is transparent ?
	let's draw, it's nicer */
#if 0
	if (GF_COL_A(ctx->aspect.fill_color) && ctx->aspect.fill_texture->transparent) {
		visual_2d_draw_path(visual, path, ctx, NULL, NULL);
		ctx->flags &= ~CTX_PATH_FILLED;
	}
#endif

	visual_2d_texture_path_extended(visual, path, NULL, ctx, NULL, NULL, tr_state);
}

#define ADAPTATION_SIZE		0


void visual_2d_draw_path_extended(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_STENCIL brush, GF_STENCIL pen, GF_TraverseState *tr_state, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx)
{
	Bool dofill, dostrike;
	GF_Raster2D *raster = visual->compositor->rasterizer;
#ifdef SKIP_DRAW
	return;
#endif
	
	assert(visual->raster_surface);

	if ((ctx->flags & CTX_PATH_FILLED) && (ctx->flags & CTX_PATH_STROKE) ) {
		if (visual->compositor->draw_bvol) draw_clipper(visual, ctx);
		return;
	}

	if (! (ctx->flags & CTX_IS_BACKGROUND) ) visual_2d_set_options(visual->compositor, visual->raster_surface, ctx->flags & CTX_IS_TEXT, 0);

	dofill = dostrike = 0;
	if (!(ctx->flags & CTX_PATH_FILLED) && GF_COL_A(ctx->aspect.fill_color) ) {
		dofill = 1;
		if (!brush) {
			brush = visual->raster_brush;
			raster->stencil_set_brush_color(brush, ctx->aspect.fill_color);
		}
	}


	/*compute width based on transform and top_level transform*/
	if (!(ctx->flags & CTX_PATH_STROKE) && ctx->aspect.pen_props.width) {
		dostrike = 1;
	} else if (!dofill) {
		return;
	}

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	raster->surface_set_matrix(visual->raster_surface, (ctx->flags & CTX_IS_BACKGROUND) ? NULL : &ctx->transform);

	/*fill path*/
	if (dofill) {
#if ADAPTATION_SIZE
		if ((ctx->bi->clip.width<ADAPTATION_SIZE) && (ctx->bi->clip.height<ADAPTATION_SIZE)) {
			raster->surface_clear(visual->raster_surface, &ctx->bi->clip, ctx->aspect.fill_color);
		} else 
#endif
		{
			/*push path*/
			raster->surface_set_path(visual->raster_surface, path);
			visual_2d_fill_path(visual, ctx, brush, tr_state);
			raster->surface_set_path(visual->raster_surface, NULL);
		}
	}

	if (dostrike) {
#if ADAPTATION_SIZE
		if ((ctx->bi->clip.width<ADAPTATION_SIZE) && (ctx->bi->clip.height<ADAPTATION_SIZE)) {
		} else 
#endif
		{
			StrikeInfo2D *si;
			
			if (!pen) {
				pen = visual->raster_brush;
				raster->stencil_set_brush_color(pen, ctx->aspect.line_color);
			}

			si = drawable_get_strikeinfo(visual->compositor, ctx->drawable, &ctx->aspect, ctx->appear, path, ctx->flags, NULL);
			if (si && si->outline) {
				if (ctx->aspect.line_texture) {
					visual_2d_texture_path_extended(visual, si->outline, ctx->aspect.line_texture, ctx, orig_bounds, ext_mx, tr_state);
				} else {
					raster->surface_set_path(visual->raster_surface, si->outline);
					visual_2d_fill_path(visual, ctx, pen, tr_state);
				}
				/*that's ugly, but we cannot cache path outline for IFS2D/ILS2D*/
				if (path && !(ctx->flags & CTX_IS_TEXT) && (path!=ctx->drawable->path) ) {
					gf_path_del(si->outline);
					si->outline = NULL;
				}
			}
//			drawable_reset_path_outline(ctx->drawable);
		}
	}

	if (visual->compositor->draw_bvol) draw_clipper(visual, ctx);
}

void visual_2d_draw_path(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_STENCIL brush, GF_STENCIL pen, GF_TraverseState *tr_state)
{
	visual_2d_draw_path_extended(visual, path, ctx, brush, pen, tr_state, NULL, NULL);
}

void visual_2d_fill_rect(GF_VisualManager *visual, DrawableContext *ctx, GF_Rect *_rc, u32 color, u32 strike_color, GF_TraverseState *tr_state)
{
	GF_Path *path;
	GF_Rect *rc;
	GF_Raster2D *raster = visual->compositor->rasterizer;
#ifdef SKIP_DRAW
	return;
#endif

	if (!visual->raster_surface) return;
	if (!color && !strike_color) return;

	if ((ctx->flags & CTX_PATH_FILLED) && (ctx->flags & CTX_PATH_STROKE) ) {
		if (visual->compositor->draw_bvol) draw_clipper(visual, ctx);
		return;
	}

	/*no aa*/
	visual_2d_set_options(visual->compositor, visual->raster_surface, 0, 1);
	if (_rc) {
		rc = _rc;
		raster->surface_set_matrix(visual->raster_surface, &ctx->transform);
	}
	else {
		rc = &ctx->bi->unclip;
		raster->surface_set_matrix(visual->raster_surface, NULL);
	}

	path = gf_path_new();
	gf_path_add_move_to(path, rc->x, rc->y-rc->height);
	gf_path_add_line_to(path, rc->x+rc->width, rc->y-rc->height);
	gf_path_add_line_to(path, rc->x+rc->width, rc->y);
	gf_path_add_line_to(path, rc->x, rc->y);
	gf_path_close(path);
	

	if (color) {
		/*push path*/
		raster->surface_set_path(visual->raster_surface, path);
		raster->stencil_set_brush_color(visual->raster_brush, color);
		visual_2d_fill_path(visual, ctx, visual->raster_brush, tr_state);
		raster->surface_set_path(visual->raster_surface, NULL);
	}
	if (strike_color) {
		GF_Path *outline;
		GF_PenSettings pen;
		memset(&pen, 0, sizeof(GF_PenSettings));
		pen.width = 1;
		pen.join = GF_LINE_JOIN_BEVEL;
		pen.dash = GF_DASH_STYLE_DOT;
		raster->stencil_set_brush_color(visual->raster_brush, strike_color);
		outline = gf_path_get_outline(path,  pen);	
		outline->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
		raster->surface_set_path(visual->raster_surface, outline);
		visual_2d_fill_path(visual, ctx, visual->raster_brush, tr_state);
		raster->surface_set_path(visual->raster_surface, NULL);
		gf_path_del(outline);
	}

	gf_path_del(path);
}
