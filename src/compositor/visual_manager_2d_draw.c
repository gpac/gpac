/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

//#define SKIP_DRAW

GF_Err visual_2d_init_raster(GF_VisualManager *visual)
{
	if (!visual->raster_surface) {
		visual->raster_surface = gf_evg_surface_new(visual->center_coords);
		if (!visual->raster_surface) return GF_IO_ERR;
	}
	return visual->GetSurfaceAccess(visual);
}

void visual_2d_release_raster(GF_VisualManager *visual)
{
	if (visual->raster_surface) {
		visual->ReleaseSurfaceAccess(visual);
	}
}


void visual_2d_clear_surface(GF_VisualManager *visual, GF_IRect *rc, u32 BackColor, u32 is_offscreen)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (! visual->CheckAttached(visual) ) return;

	if (!BackColor && !visual->offscreen && !visual->compositor->forced_alpha) {
		if ( !(visual->compositor->init_flags & GF_VOUT_WINDOW_TRANSPARENT)) {
			BackColor = visual->compositor->back_color;
		}
	}

	gf_evg_surface_clear(visual->raster_surface, rc, BackColor);
}

static void draw_clipper(GF_VisualManager *visual, struct _drawable_context *ctx)
{
	GF_PenSettings clipset;
	GF_Path *clippath, *cliper;

	if (ctx->flags & CTX_IS_BACKGROUND) return;

	memset(&clipset, 0, sizeof(GF_PenSettings));
	clipset.width = 2*FIX_ONE;

	clippath = gf_path_new();
	gf_path_add_rect_center(clippath, ctx->bi->unclip.x + ctx->bi->unclip.width/2, ctx->bi->unclip.y - ctx->bi->unclip.height/2, ctx->bi->unclip.width, ctx->bi->unclip.height);
	cliper = gf_path_get_outline(clippath, clipset);
	gf_path_del(clippath);
	gf_evg_surface_set_matrix(visual->raster_surface, NULL);
	gf_evg_surface_set_clipper(visual->raster_surface, NULL);
	gf_evg_surface_set_path(visual->raster_surface, cliper);
	gf_evg_stencil_set_brush_color(visual->raster_brush, 0xFF000000);
	gf_evg_surface_fill(visual->raster_surface, visual->raster_brush);
	gf_path_del(cliper);
}

void visual_2d_fill_path(GF_VisualManager *visual, DrawableContext *ctx, GF_EVGStencil * stencil, GF_TraverseState *tr_state, Bool is_erase)
{
	Bool has_modif = GF_FALSE;
	GF_IRect clip;

	/*direct drawing : use ctx clip*/
	if (tr_state->immediate_draw) {
		if (ctx->bi->clip.width && ctx->bi->clip.height) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Redrawing node %s [%s] (direct draw)\n", gf_node_get_log_name(ctx->drawable->node), gf_node_get_class_name(ctx->drawable->node) ));

			if (stencil) {
				gf_evg_surface_set_clipper(visual->raster_surface, &ctx->bi->clip);
				gf_evg_surface_fill(visual->raster_surface, stencil);
			} else {
				gf_evg_surface_clear(visual->raster_surface, &ctx->bi->clip, 0);
			}

			has_modif = GF_TRUE;
		}
	}
	/*indirect drawing, draw path in all dirty areas*/
	else {
		u32 i;
		for (i=0; i<visual->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
#ifdef TRACK_OPAQUE_REGIONS
			if (!is_erase && (visual->draw_node_index<visual->to_redraw.list[i].opaque_node_index)) continue;
#endif
			clip = ctx->bi->clip;
			gf_irect_intersect(&clip, &visual->to_redraw.list[i].rect);
			if (clip.width && clip.height) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Visual2D] Redrawing node %s [%s] (indirect draw @ dirty rect idx %d)\n", gf_node_get_log_name(ctx->drawable->node), gf_node_get_class_name(ctx->drawable->node), i));
				if (stencil) {
					gf_evg_surface_set_clipper(visual->raster_surface, &clip);
					gf_evg_surface_fill(visual->raster_surface, stencil);
				} else {
					gf_evg_surface_clear(visual->raster_surface, &clip, 0);
				}
				has_modif = 1;
			}
		}
	}
#ifndef GPAC_DISABLE_3D
	if (!is_erase && (visual==visual->compositor->visual))
		visual->nb_objects_on_canvas_since_last_ogl_flush++;
#endif
	if (has_modif) {
		visual->has_modif = 1;
#ifndef GPAC_DISABLE_3D
		if (!visual->offscreen && visual->compositor->hybrid_opengl && !is_erase)
			ra_union_rect(&visual->hybgl_drawn, &ctx->bi->clip);
#endif
	}
}

static void visual_2d_set_options(GF_Compositor *compositor, GF_EVGSurface *rend, Bool forText, Bool no_antialias)
{
	if (no_antialias) {
		gf_evg_surface_set_raster_level(rend, GF_RASTER_HIGH_SPEED);
	} else {
		switch (compositor->aa) {
		case GF_ANTIALIAS_NONE:
			gf_evg_surface_set_raster_level(rend, GF_RASTER_HIGH_SPEED);
			break;
		case GF_ANTIALIAS_TEXT:
			if (forText) {
				gf_evg_surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			} else {
				gf_evg_surface_set_raster_level(rend, compositor->fast ? GF_RASTER_HIGH_QUALITY : GF_RASTER_MID);
			}
			break;
		case GF_ANTIALIAS_FULL:
		default:
			gf_evg_surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			break;
		}
	}
}


#ifndef GPAC_DISABLE_VRML
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
#endif /*GPAC_DISABLE_VRML*/

static void visual_2d_draw_gradient(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_TraverseState *tr_state, GF_Matrix2D *ext_mx, GF_Rect *orig_bounds)
{
	GF_Rect rc;
	GF_EVGStencil * stencil;
	GF_Matrix2D g_mat;

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

#ifndef GPAC_DISABLE_VRML
	if (ctx->flags & CTX_HAS_APPEARANCE) {
		GF_Matrix2D txt_mat;
		visual_2d_get_texture_transform(ctx->appear, txh, &txt_mat, (txh == ctx->aspect.fill_texture) ? 0 : 1, INT2FIX(txh->width), INT2FIX(txh->height));
		gf_mx2d_add_matrix(&g_mat, &txt_mat);
	}
#endif

	/*move to bottom-left corner of bounds */
	if (ext_mx) gf_mx2d_add_matrix(&g_mat, ext_mx);
	if (orig_bounds) gf_mx2d_add_translation(&g_mat, (orig_bounds->x), (orig_bounds->y - orig_bounds->height));
	gf_mx2d_add_matrix(&g_mat, &ctx->transform);

	gf_evg_stencil_set_matrix(stencil, &g_mat);
	gf_evg_stencil_set_auto_matrix(stencil, GF_FALSE);
	gf_evg_stencil_set_color_matrix(stencil, ctx->col_mat);

	/*MPEG-4/VRML context or no fill info*/
	if (ctx->flags & CTX_HAS_APPEARANCE || !ctx->aspect.fill_color)
		gf_evg_stencil_set_alpha(stencil, 0xFF);
	else
		gf_evg_stencil_set_alpha(stencil, GF_COL_A(ctx->aspect.fill_color) );

	gf_evg_surface_set_matrix(visual->raster_surface, &ctx->transform);
	txh->flags |= GF_SR_TEXTURE_USED;

	gf_evg_surface_set_path(visual->raster_surface, path);
	visual_2d_fill_path(visual, ctx, stencil, tr_state, 0);
	gf_evg_surface_set_path(visual->raster_surface, NULL);

	ctx->flags |= CTX_PATH_FILLED;
}



void visual_2d_texture_path_text(GF_VisualManager *visual, DrawableContext *txt_ctx, GF_Path *path, GF_Rect *object_bounds, GF_TextureHandler *txh, GF_TraverseState *tr_state)
{
	GF_EVGStencil * stencil;
	Fixed sS, sT;
	GF_Matrix2D gf_mx2d_txt;
	GF_Rect orig_rc;
	u8 alpha, r, g, b;
	GF_ColorMatrix cmat;

	if (! visual->CheckAttached(visual) ) return;


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
	gf_evg_stencil_set_matrix(stencil, &gf_mx2d_txt);
	gf_evg_stencil_set_auto_matrix(stencil, GF_FALSE);

	alpha = GF_COL_A(txt_ctx->aspect.fill_color);
	r = GF_COL_R(txt_ctx->aspect.fill_color);
	g = GF_COL_G(txt_ctx->aspect.fill_color);
	b = GF_COL_B(txt_ctx->aspect.fill_color);

	/*if col do a cxmatrix*/
	if (!r && !g && !b) {
		gf_evg_stencil_set_alpha(stencil, alpha);
	} else {
		gf_evg_stencil_set_alpha(stencil, 0xFF);
		memset(cmat.m, 0, sizeof(Fixed) * 20);
		cmat.m[4] = INT2FIX(r)/255;
		cmat.m[9] = INT2FIX(g)/255;
		cmat.m[14] = INT2FIX(b)/255;
		cmat.m[18] = INT2FIX(alpha)/255;
		cmat.identity = 0;
		gf_evg_stencil_set_color_matrix(stencil, &cmat);
	}

	gf_evg_surface_set_matrix(visual->raster_surface, &txt_ctx->transform);
	txh->flags |= GF_SR_TEXTURE_USED;

	/*push path*/
	gf_evg_surface_set_path(visual->raster_surface, path);

	visual_2d_fill_path(visual, txt_ctx, stencil, tr_state, 0);
	gf_evg_surface_set_path(visual->raster_surface, NULL);
	txt_ctx->flags |= CTX_PATH_FILLED;
}


#ifndef GPAC_DISABLE_3D

void visual_2d_flush_hybgl_canvas(GF_VisualManager *visual, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_TraverseState *tr_state)
{
	Bool line_texture = GF_FALSE;
	u32 i;
	u32 prev_color;
	Bool transparent, had_flush = 0;
	u32 nb_obj_left_on_canvas = visual->nb_objects_on_canvas_since_last_ogl_flush;
	u8 alpha;

	if (! visual->hybgl_drawn.count)
		return;

	//we have drawn things on the canvas before this object, flush canvas to GPU

	if (txh && (txh==ctx->aspect.line_texture)) {
		line_texture = GF_TRUE;
		alpha = GF_COL_A(ctx->aspect.line_color);
		prev_color = ctx->aspect.line_color;
		ctx->aspect.line_texture = NULL;
		ctx->aspect.line_color = 0;
	} else {
		alpha = GF_COL_A(ctx->aspect.fill_color);
		if (!alpha) alpha = GF_COL_A(ctx->aspect.line_color);
		prev_color = ctx->aspect.fill_color;
		ctx->aspect.fill_texture = NULL;
		ctx->aspect.fill_color = 0;

	}
	transparent = txh ? (txh->transparent || (alpha!=0xFF)) : GF_TRUE;
	//clear wherever we have overlap
	for (i=0; i<visual->hybgl_drawn.count; i++) {
		GF_IRect rc = ctx->bi->clip;
		gf_irect_intersect(&ctx->bi->clip, &visual->hybgl_drawn.list[i].rect);
		if (ctx->bi->clip.width && ctx->bi->clip.height) {
			//if something behind this, flush canvas to gpu
			if (transparent) {
				if (!had_flush) {
					//flush the complete area below this object, regardless of intersections
					compositor_2d_hybgl_flush_video(visual->compositor, tr_state->immediate_draw ? NULL : &rc);
					had_flush = 1;
				}
				//if object was not completely in the flush region we will need to flush the canvas
				if ( gf_irect_inside(&rc, &visual->hybgl_drawn.list[i].rect)) {
					//it may happen that we had no object on the canvas but syil have their bounds (we only drew textures)
					if (nb_obj_left_on_canvas)
						nb_obj_left_on_canvas--;
				}
			}
			//immediate mode flush, erase all canvas (we just completely flusged it)
			if (tr_state->immediate_draw && had_flush && !tr_state->immediate_for_defer) {
				gf_evg_surface_clear(visual->raster_surface, NULL, 0);
			}
			//defer mode, erase all part of the canvas below us
			else if (txh) {
				visual_2d_draw_path_extended(visual, ctx->drawable->path, ctx, NULL, NULL, tr_state, NULL, NULL, GF_TRUE);
			} else {
				gf_evg_surface_clear(visual->raster_surface, &ctx->bi->clip, 0);
			}
		}
		ctx->bi->clip = rc;
	}
	if (line_texture) {
		ctx->aspect.line_color = prev_color;
		ctx->aspect.line_texture = txh;
	} else {
		ctx->aspect.fill_color = prev_color;
		ctx->aspect.fill_texture = txh;
	}

	if (had_flush) {
		visual->nb_objects_on_canvas_since_last_ogl_flush = nb_obj_left_on_canvas;
	}
}

void visual_2d_texture_path_opengl_auto(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, GF_TraverseState *tr_state)
{
	GF_Rect clipper;
	GF_Matrix mx, bck_mx;
	u32 prev_mode = tr_state->traversing_mode;
	u32 prev_type_3d = tr_state->visual->type_3d;

	visual_2d_flush_hybgl_canvas(visual, txh, ctx, tr_state);

	tr_state->visual->type_3d = 4;
	tr_state->appear = ctx->appear;
	if (ctx->col_mat) gf_cmx_copy(&tr_state->color_mat, ctx->col_mat);//
	gf_mx_copy(bck_mx, tr_state->model_matrix);

	tr_state->traversing_mode=TRAVERSE_DRAW_3D;
	//in hybridGL the 2D camera is always setup as centered-coords, we have to insert flip+translation in case of top-left origin 
	if (tr_state->visual->center_coords) {
		gf_mx_from_mx2d(&tr_state->model_matrix, &ctx->transform);
	} else {
		gf_mx_init(tr_state->model_matrix);
		gf_mx_add_scale(&tr_state->model_matrix, FIX_ONE, -FIX_ONE, FIX_ONE);
		gf_mx_add_translation(&tr_state->model_matrix, -tr_state->camera->width/2, -tr_state->camera->height/2, 0);

		gf_mx_from_mx2d(&mx, &ctx->transform);
		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
	}

	clipper.x = INT2FIX(ctx->bi->clip.x);
	clipper.y = INT2FIX(ctx->bi->clip.y);
	clipper.width = INT2FIX(ctx->bi->clip.width);
	clipper.height = INT2FIX(ctx->bi->clip.height);
	visual_3d_set_clipper_2d(tr_state->visual, clipper, NULL);

	gf_node_allow_cyclic_traverse(ctx->drawable->node);
	gf_node_traverse(ctx->drawable->node, tr_state);

	tr_state->visual->type_3d=prev_type_3d;
	tr_state->traversing_mode=prev_mode;
	if (ctx->col_mat) gf_cmx_init(&tr_state->color_mat);

	ctx->flags |= CTX_PATH_FILLED;

	visual_3d_reset_clipper_2d(tr_state->visual);
	gf_mx_copy(tr_state->model_matrix, bck_mx);
}
#endif


void visual_2d_texture_path_extended(GF_VisualManager *visual, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, GF_TraverseState *tr_state)
{
	Fixed sS, sT;
	u32 tx_tile;
	GF_EVGStencil * tx_raster;
	GF_Matrix2D mx_texture;
	GF_Rect orig_rc;

	if (! visual->CheckAttached(visual) ) return;

	if (!txh) txh = ctx->aspect.fill_texture;
	if (!txh) return;
	if (!txh->tx_io && !txh->data) {
		gf_node_dirty_set(txh->owner, 0, 1);

		txh->needs_refresh=1;
		return;
	}


	/*this is gradient draw*/
	if (txh->compute_gradient_matrix) {
		visual_2d_draw_gradient(visual, path, txh, ctx, tr_state, ext_mx, orig_bounds);
		return;
	}


#ifndef GPAC_DISABLE_3D
	if (visual->compositor->hybrid_opengl) {
		visual_2d_texture_path_opengl_auto(visual, path, txh, ctx, orig_bounds, ext_mx, tr_state);
		return;
	}
#endif

	if (txh->flags & GF_SR_TEXTURE_PRIVATE_MEDIA) {
		GF_Window src, dst;
		visual_2d_fill_path(visual, ctx, NULL, tr_state, 0);

		/*if texture not ready, update the size before computing output rectangles */
		if (!txh->width || !txh->height) {
			gf_mo_get_visual_info(txh->stream, &txh->width, &txh->height, &txh->stride, &txh->pixel_ar, &txh->pixelformat, &txh->is_flipped);
			/*in case the node is an MPEG-4 bitmap, force stack rebuild at next frame */
			gf_node_dirty_set(ctx->drawable->node, GF_SG_NODE_DIRTY, 1);
		}

		compositor_texture_rectangles(visual, txh, &ctx->bi->clip, &ctx->bi->unclip, &src, &dst, NULL, NULL);
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

	/*get scaling ratio so that active texture view is stretched to original bounds (std 2D shape texture mapping in MPEG4)*/
	sS = orig_rc.width / txh->width;
	sT = orig_rc.height / txh->height;

	gf_mx2d_init(mx_texture);
	gf_mx2d_add_scale(&mx_texture, sS, sT);

#ifndef GPAC_DISABLE_VRML
	/*apply texture transform*/
	if (ctx->flags & CTX_HAS_APPEARANCE) {
		GF_Matrix2D tex_trans;
		visual_2d_get_texture_transform(ctx->appear, txh, &tex_trans, (txh == ctx->aspect.fill_texture) ? 0 : 1, txh->width * sS, txh->height * sT);
		gf_mx2d_add_matrix(&mx_texture, &tex_trans);
	}
#endif

	/*move to bottom-left corner of bounds */
	gf_mx2d_add_translation(&mx_texture, (orig_rc.x), (orig_rc.y - orig_rc.height));

	if (ext_mx) gf_mx2d_add_matrix(&mx_texture, ext_mx);

	/*move to final coordinate system (except background which is built directly in final coord system)*/
	if (!(ctx->flags & CTX_IS_BACKGROUND) ) gf_mx2d_add_matrix(&mx_texture, &ctx->transform);

	/*set path transform*/
	gf_evg_stencil_set_matrix(tx_raster, &mx_texture);
	gf_evg_stencil_set_auto_matrix(tx_raster, GF_FALSE);


	tx_tile = 0;
	if (txh->flags & GF_SR_TEXTURE_REPEAT_S) tx_tile |= GF_TEXTURE_REPEAT_S;
	if (txh->flags & GF_SR_TEXTURE_REPEAT_T) tx_tile |= GF_TEXTURE_REPEAT_T;
	if (ctx->flags & CTX_FLIPED_COORDS)
		tx_tile |= GF_TEXTURE_FLIP_Y;
	gf_evg_stencil_set_mapping(tx_raster, (GF_TextureMapFlags) tx_tile);

	if (!(ctx->flags & CTX_IS_BACKGROUND) ) {
		u8 a = GF_COL_A(ctx->aspect.fill_color);
		if (!a) a = GF_COL_A(ctx->aspect.line_color);
		/*texture alpha scale is the original material transparency, NOT the one after color transform*/
		gf_evg_stencil_set_alpha(tx_raster, a );
		gf_evg_stencil_set_color_matrix(tx_raster, ctx->col_mat);

		gf_evg_surface_set_matrix(visual->raster_surface, &ctx->transform);
	} else {
		gf_evg_surface_set_matrix(visual->raster_surface, NULL);
	}
	txh->flags |= GF_SR_TEXTURE_USED;

	/*push path & draw*/
	gf_evg_surface_set_path(visual->raster_surface, path);
	visual_2d_fill_path(visual, ctx, tx_raster, tr_state, 0);
	gf_evg_surface_set_path(visual->raster_surface, NULL);



	ctx->flags |= CTX_PATH_FILLED;
}

void visual_2d_texture_path(GF_VisualManager *visual, GF_Path *path, struct _drawable_context *ctx, GF_TraverseState *tr_state)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (! visual->CheckAttached(visual) ) return;

	if ((ctx->flags & CTX_PATH_FILLED) || !ctx->aspect.fill_texture || visual->compositor->is_hidden) return;

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


void visual_2d_draw_path_extended(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_EVGStencil * brush, GF_EVGStencil * pen, GF_TraverseState *tr_state, GF_Rect *orig_bounds, GF_Matrix2D *ext_mx, Bool is_erase)
{
	Bool dofill, dostrike;
#ifdef SKIP_DRAW
	return;
#endif
	if (! visual->CheckAttached(visual) ) return;

	if ((ctx->flags & CTX_PATH_FILLED) && (ctx->flags & CTX_PATH_STROKE) ) {
		if (visual->compositor->bvol) draw_clipper(visual, ctx);
		return;
	}

	if (! (ctx->flags & CTX_IS_BACKGROUND) )
		visual_2d_set_options(visual->compositor, visual->raster_surface, ctx->flags & CTX_IS_TEXT, ctx->flags & CTX_NO_ANTIALIAS);

	dofill = dostrike = 0;
	if (!(ctx->flags & CTX_PATH_FILLED) && (is_erase || GF_COL_A(ctx->aspect.fill_color)) ) {
		dofill = 1;
		if (!brush) {
			brush = visual->raster_brush;
			gf_evg_stencil_set_brush_color(brush, ctx->aspect.fill_color);
		}
	}


	/*compute width based on transform and top_level transform*/
	if (!(ctx->flags & CTX_PATH_STROKE) && ctx->aspect.pen_props.width) {
		dostrike = 1;
	} else if (!dofill) {
		return;
	}

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	gf_evg_surface_set_matrix(visual->raster_surface, (ctx->flags & CTX_IS_BACKGROUND) ? NULL : &ctx->transform);

	/*fill path*/
	if (dofill) {
#if ADAPTATION_SIZE
		if ((ctx->bi->clip.width<ADAPTATION_SIZE) && (ctx->bi->clip.height<ADAPTATION_SIZE)) {
			gf_evg_surface_clear(visual->raster_surface, &ctx->bi->clip, ctx->aspect.fill_color);
		} else
#endif
		{
			/*push path*/
			gf_evg_surface_set_path(visual->raster_surface, path);
			visual_2d_fill_path(visual, ctx, brush, tr_state, is_erase);
			gf_evg_surface_set_path(visual->raster_surface, NULL);
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
				gf_evg_stencil_set_brush_color(pen, ctx->aspect.line_color);
			}

			si = drawable_get_strikeinfo(visual->compositor, ctx->drawable, &ctx->aspect, ctx->appear, path, ctx->flags, NULL);
			if (si && si->outline) {
				if (ctx->aspect.line_texture) {
					visual_2d_texture_path_extended(visual, si->outline, ctx->aspect.line_texture, ctx, orig_bounds, ext_mx, tr_state);
				} else {
					gf_evg_surface_set_path(visual->raster_surface, si->outline);
					visual_2d_fill_path(visual, ctx, pen, tr_state, 0);
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

	if (visual->compositor->bvol) draw_clipper(visual, ctx);
}

void visual_2d_draw_path(GF_VisualManager *visual, GF_Path *path, DrawableContext *ctx, GF_EVGStencil * brush, GF_EVGStencil * pen, GF_TraverseState *tr_state)
{
	visual_2d_draw_path_extended(visual, path, ctx, brush, pen, tr_state, NULL, NULL, GF_FALSE);
}

void visual_2d_fill_rect(GF_VisualManager *visual, DrawableContext *ctx, GF_Rect *_rc, u32 color, u32 strike_color, GF_TraverseState *tr_state)
{
	GF_Path *path;
	GF_Rect *rc;
#ifdef SKIP_DRAW
	return;
#endif

	if (! visual->CheckAttached(visual) ) return;

	if (!color && !strike_color) return;

	if ((ctx->flags & CTX_PATH_FILLED) && (ctx->flags & CTX_PATH_STROKE) ) {
		if (visual->compositor->bvol) draw_clipper(visual, ctx);
		return;
	}

	/*no aa*/
	visual_2d_set_options(visual->compositor, visual->raster_surface, 0, 1);
	if (_rc) {
		rc = _rc;
		gf_evg_surface_set_matrix(visual->raster_surface, &ctx->transform);
	}
	else {
		rc = &ctx->bi->unclip;
		gf_evg_surface_set_matrix(visual->raster_surface, NULL);
	}

	path = gf_path_new();
	gf_path_add_move_to(path, rc->x, rc->y-rc->height);
	gf_path_add_line_to(path, rc->x+rc->width, rc->y-rc->height);
	gf_path_add_line_to(path, rc->x+rc->width, rc->y);
	gf_path_add_line_to(path, rc->x, rc->y);
	gf_path_close(path);


	if (color) {
		/*push path*/
		gf_evg_surface_set_path(visual->raster_surface, path);
		gf_evg_stencil_set_brush_color(visual->raster_brush, color);
		visual_2d_fill_path(visual, ctx, visual->raster_brush, tr_state, 0);
		gf_evg_surface_set_path(visual->raster_surface, NULL);
	}
	if (strike_color) {
		GF_Path *outline;
		GF_PenSettings pen;
		memset(&pen, 0, sizeof(GF_PenSettings));
		pen.width = 1;
		pen.join = GF_LINE_JOIN_BEVEL;
		pen.dash = GF_DASH_STYLE_DOT;
		gf_evg_stencil_set_brush_color(visual->raster_brush, strike_color);
		outline = gf_path_get_outline(path,  pen);
		outline->flags &= ~GF_PATH_FILL_ZERO_NONZERO;
		gf_evg_surface_set_path(visual->raster_surface, outline);
		visual_2d_fill_path(visual, ctx, visual->raster_brush, tr_state, 0);
		gf_evg_surface_set_path(visual->raster_surface, NULL);
		gf_path_del(outline);
	}

	gf_path_del(path);
}

#if 0 //unused
void visual_2d_fill_irect(GF_VisualManager *visual, GF_IRect *rc, u32 fill, u32 strike)
{
	GF_Path *path;
	GF_Path *outline;
	GF_PenSettings pen;
#ifdef SKIP_DRAW
	return;
#endif

	if (!rc) return;

	if (! visual->CheckAttached(visual) ) return;

	if (!fill && !strike ) return;

	/*no aa*/
	visual_2d_set_options(visual->compositor, visual->raster_surface, 0, 1);
	gf_evg_surface_set_matrix(visual->raster_surface, NULL);

	gf_evg_surface_set_raster_level(visual->raster_surface, GF_RASTER_HIGH_SPEED);
	gf_evg_surface_set_matrix(visual->raster_surface, NULL);

	path = gf_path_new();
	gf_path_add_move_to(path, INT2FIX(rc->x-1), INT2FIX(rc->y+2-rc->height));
	gf_path_add_line_to(path, INT2FIX(rc->x+rc->width-2), INT2FIX(rc->y+2-rc->height));
	gf_path_add_line_to(path, INT2FIX(rc->x+rc->width), INT2FIX(rc->y));
	gf_path_add_line_to(path, INT2FIX(rc->x), INT2FIX(rc->y));
	gf_path_close(path);

	if (fill) {
		gf_evg_surface_set_path(visual->raster_surface, path);
		gf_evg_stencil_set_brush_color(visual->raster_brush, fill);

		gf_evg_surface_set_clipper(visual->raster_surface, rc);
		gf_evg_surface_fill(visual->raster_surface, visual->raster_brush);

		gf_evg_surface_set_path(visual->raster_surface, NULL);
	}

	if (strike) {
		memset(&pen, 0, sizeof(GF_PenSettings));
		pen.width = 2;
		pen.align = GF_PATH_LINE_INSIDE;
		pen.join = GF_LINE_JOIN_BEVEL;
		outline = gf_path_get_outline(path, pen);
		outline->flags &= ~GF_PATH_FILL_ZERO_NONZERO;

		gf_evg_surface_set_path(visual->raster_surface, outline);
		gf_evg_stencil_set_brush_color(visual->raster_brush, strike);

		gf_evg_surface_set_clipper(visual->raster_surface, rc);
		gf_evg_surface_fill(visual->raster_surface, visual->raster_brush);

		gf_evg_surface_set_path(visual->raster_surface, NULL);
		gf_path_del(outline);
	}
	gf_path_del(path);
#ifndef GPAC_DISABLE_3D
	if (!visual->offscreen && visual->compositor->hybrid_opengl)
		ra_union_rect(&visual->hybgl_drawn, rc);
#endif
}
#endif

