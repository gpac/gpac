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

#include "visualsurface2d.h"
#include "drawable.h"
#include "stacks2d.h"
#include <gpac/options.h>

//#define SKIP_DRAW

GF_Err VS2D_InitSurface(VisualSurface2D *surf)
{
	GF_Raster2D *r2d = surf->render->compositor->r2d;
	if (!surf->the_surface) {
		surf->the_surface = r2d->surface_new(r2d, surf->center_coords);
		if (!surf->the_surface) return GF_IO_ERR;
	}
	if (!surf->the_brush) {
		surf->the_brush = r2d->stencil_new(r2d, GF_STENCIL_SOLID);
		if (!surf->the_brush) return GF_IO_ERR;
	}
	if (!surf->the_pen) {
		surf->the_pen = r2d->stencil_new(r2d, GF_STENCIL_SOLID);
		if (!surf->the_pen) return GF_IO_ERR;
	}
	return surf->GetSurfaceAccess(surf);
}

void VS2D_TerminateSurface(VisualSurface2D *surf)
{
	if (surf->the_surface) {
		if (surf->render->compositor->r2d->surface_flush) 
			surf->render->compositor->r2d->surface_flush(surf->the_surface);

		surf->ReleaseSurfaceAccess(surf);
	}
}

void VS2D_ResetGraphics(VisualSurface2D *surf)
{
	GF_Raster2D *r2d = surf->render->compositor->r2d;
	if (surf->the_surface) r2d->surface_delete(surf->the_surface);
	surf->the_surface = NULL;
	if (surf->the_brush) r2d->stencil_delete(surf->the_brush);
	surf->the_brush = NULL;
	if (surf->the_pen) r2d->stencil_delete(surf->the_pen);
	surf->the_pen = NULL;
}

void VS2D_Clear(VisualSurface2D *surf, GF_IRect *rc, u32 BackColor)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (!surf->the_surface) return;
	
	if (!BackColor) {
		if (!surf->composite) BackColor = surf->default_back_color;
	}
	surf->render->compositor->r2d->surface_clear(surf->the_surface, rc, BackColor);
}

static void draw_clipper(VisualSurface2D *surf, struct _drawable_context *ctx)
{
	GF_PenSettings clipset;
	GF_Path *clippath, *cliper;
	GF_Raster2D *r2d = surf->render->compositor->r2d;

	if (ctx->is_background) return;

	memset(&clipset, 0, sizeof(GF_PenSettings));
	clipset.width = 2*FIX_ONE;

	clippath = gf_path_new();
	gf_path_add_rect_center(clippath, ctx->original.x + ctx->original.width/2, ctx->original.y - ctx->original.height/2, ctx->original.width, ctx->original.height);
	cliper = gf_path_get_outline(clippath, clipset);
	gf_path_del(clippath);
	r2d->surface_set_matrix(surf->the_surface, &ctx->transform);
	r2d->surface_set_clipper(surf->the_surface, NULL);
	r2d->surface_set_path(surf->the_surface, cliper);
	r2d->stencil_set_brush_color(surf->the_pen, 0xFF000000);
	r2d->surface_fill(surf->the_surface, surf->the_pen);
	gf_path_del(cliper);
}

static void VS2D_DoFill(VisualSurface2D *surf, DrawableContext *ctx, GF_STENCIL stencil)
{
	GF_IRect clip;
	GF_Raster2D *r2d = surf->render->compositor->r2d;

	/*background rendering - direct rendering: use ctx clip*/
	if (ctx->is_background || (surf->render->top_effect->trav_flags & TF_RENDER_DIRECT)) {
		if (ctx->clip.width && ctx->clip.height) {
			r2d->surface_set_clipper(surf->the_surface, &ctx->clip);
			r2d->surface_fill(surf->the_surface, stencil);
		}
	} 
	/*indirect rendering, draw path in all dirty areas*/
	else {
		u32 i;
		for (i=0; i<surf->to_redraw.count; i++) {
			/*there's an opaque region above, don't draw*/
			if (surf->draw_node_index<surf->to_redraw.opaque_node_index[i]) continue;
			clip = ctx->clip;
			gf_irect_intersect(&clip, &surf->to_redraw.list[i]);
			if (clip.width && clip.height) {
				r2d->surface_set_clipper(surf->the_surface, &clip);
				r2d->surface_fill(surf->the_surface, stencil);
//			} else {
//				fprintf(stdout, "node outside clipper\n");
			}
		}
	}
}

void VS2D_SetOptions(Render2D *sr, GF_SURFACE rend, Bool forText, Bool no_antialias)
{
	GF_Raster2D *r2d = sr->compositor->r2d;
	if (no_antialias) {
		r2d->surface_set_raster_level(rend, sr->compositor->high_speed ? GF_RASTER_HIGH_SPEED : GF_RASTER_MID);
	} else {
		switch (sr->compositor->antiAlias) {
		case GF_ANTIALIAS_NONE:
			r2d->surface_set_raster_level(rend, GF_RASTER_HIGH_SPEED);
			break;
		case GF_ANTIALIAS_TEXT:
			if (forText) {
				r2d->surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			} else {
				r2d->surface_set_raster_level(rend, sr->compositor->high_speed ? GF_RASTER_HIGH_QUALITY : GF_RASTER_MID);
			}
			break;
		case GF_ANTIALIAS_FULL:
		default:
			r2d->surface_set_raster_level(rend, GF_RASTER_HIGH_QUALITY);
			break;
		}
	}
}


void get_gf_sr_texture_transform(GF_Node *__appear, GF_TextureHandler *txh, GF_Matrix2D *mat, Bool line_texture, Fixed final_width, Fixed final_height)
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
		/*VRML: Tc' = -C × S × R × C × T × Tc*/
		M_TextureTransform *txt = (M_TextureTransform *) txtrans;
		SFVec2f scale = txt->scale;
		if (!scale.x) scale.x = FIX_ONE/100;
		if (!scale.y) scale.y = FIX_ONE/100;

		gf_mx2d_add_translation(mat, -txt->center.x * final_width, -txt->center.y * final_height);
		gf_mx2d_add_scale(mat, scale.x, scale.y);
		gf_mx2d_add_rotation(mat, 0, 0, txt->rotation);
		gf_mx2d_add_translation(mat, txt->center.x * final_width, txt->center.y * final_height);
		gf_mx2d_add_translation(mat, txt->translation.x * final_width, txt->translation.y * final_height);
		/*and inverse the matrix (this is texture transform, cf VRML)*/
		gf_mx2d_inverse(mat);
		return;
	}
	if (node_tag==TAG_MPEG4_TransformMatrix2D) {
		TM2D_GetMatrix((GF_Node *) txtrans, mat);
		mat->m[2] *= final_width;
		mat->m[5] *= final_height;
		gf_mx2d_inverse(mat);
		return;
	}
}


static void VS2D_DrawGradient(VisualSurface2D *surf, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx)
{
	GF_Rect rc;
	Bool fill;
	GF_Matrix2D g_mat, txt_mat;
	GF_Raster2D *r2d = surf->render->compositor->r2d;

	if (!txh) txh = ctx->h_texture;
	gf_path_get_bounds(path, &rc);
	if (!rc.width || !rc.height || !txh->hwtx) return;
	txh->compute_gradient_matrix(txh, &rc, &g_mat);

	get_gf_sr_texture_transform(ctx->appear, txh, &txt_mat, (txh == ctx->h_texture) ? 0 : 1, INT2FIX(txh->width), INT2FIX(txh->height));
	gf_mx2d_add_matrix(&g_mat, &txt_mat);
	gf_mx2d_add_matrix(&g_mat, &ctx->transform);

	r2d->stencil_set_matrix(txh->hwtx, &g_mat);

	fill = ctx->aspect.filled;
	ctx->aspect.filled = 1;

	r2d->surface_set_matrix(surf->the_surface, &ctx->transform);

	r2d->surface_set_path(surf->the_surface, path);
	VS2D_DoFill(surf, ctx, txh->hwtx);
	r2d->surface_set_path(surf->the_surface, NULL);

	ctx->aspect.filled = fill;
	ctx->path_filled = 1;
}



void VS2D_TexturePathText(VisualSurface2D *surf, DrawableContext *txt_ctx, GF_Path *path, GF_Rect *object_bounds, GF_HWTEXTURE hwtx, GF_Rect *gf_sr_texture_bounds)
{
	Fixed sS, sT;
	GF_Matrix2D gf_mx2d_txt;
	GF_Rect rc, orig_rc;
	u8 alpha, r, g, b;
	GF_ColorMatrix cmat;
	GF_Raster2D *r2d = surf->render->compositor->r2d;

	VS2D_SetOptions(surf->render, surf->the_surface, 0, 1);

	/*get original bounds*/
	orig_rc = *object_bounds;
	rc = *gf_sr_texture_bounds;

	/*get scaling ratio so that active texture view is stretched to original bounds (std 2D shape texture mapping in MPEG4)*/
	sS = gf_divfix(orig_rc.width, rc.width);
	sT = gf_divfix(orig_rc.height, rc.height);
	
	gf_mx2d_init(gf_mx2d_txt);
	gf_mx2d_add_scale(&gf_mx2d_txt, sS, sT);

	/*move to bottom-left corner of bounds */
	gf_mx2d_add_translation(&gf_mx2d_txt, (orig_rc.x), (orig_rc.y - orig_rc.height));

	/*move to final coordinate system*/	
	gf_mx2d_add_matrix(&gf_mx2d_txt, &txt_ctx->transform);

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	r2d->stencil_set_matrix(hwtx, &gf_mx2d_txt);

	alpha = GF_COL_A(txt_ctx->aspect.fill_color);
	r = GF_COL_R(txt_ctx->aspect.fill_color);
	g = GF_COL_G(txt_ctx->aspect.fill_color);
	b = GF_COL_B(txt_ctx->aspect.fill_color);

	/*if col do a cxmatrix*/
	if (!r && !g && !b) {
		r2d->stencil_set_gf_sr_texture_alpha(hwtx, alpha);
	} else {
		r2d->stencil_set_gf_sr_texture_alpha(hwtx, 0xFF);
		memset(cmat.m, 0, sizeof(Fixed) * 20);
		cmat.m[4] = INT2FIX(r)/255;
		cmat.m[9] = INT2FIX(g)/255;
		cmat.m[14] = INT2FIX(b)/255;
		cmat.m[18] = INT2FIX(alpha)/255;
		cmat.identity = 0;
		r2d->stencil_set_color_matrix(hwtx, &cmat);
	}

	r2d->surface_set_matrix(surf->the_surface, &txt_ctx->transform);

	/*push path*/
	r2d->surface_set_path(surf->the_surface, path);

	VS2D_DoFill(surf, txt_ctx, hwtx);
	r2d->surface_set_path(surf->the_surface, NULL);
	txt_ctx->path_filled = 1;
}

void VS2D_TexturePathIntern(VisualSurface2D *surf, GF_Path *path, GF_TextureHandler *txh, struct _drawable_context *ctx)
{
	Fixed sS, sT;
	GF_Matrix2D gf_mx2d_txt, gf_sr_texture_transform;
	GF_Rect rc, orig_rc;
	GF_Raster2D *r2d = surf->render->compositor->r2d;

	if (!txh) txh = ctx->h_texture;
	if (!txh || !txh->hwtx) return;

	/*this is gradient draw*/
	if (txh->compute_gradient_matrix) {
		VS2D_DrawGradient(surf, path, txh, ctx);
		return;
	}

	/*setup quality even for background (since quality concerns images)*/
	VS2D_SetOptions(surf->render, surf->the_surface, ctx->is_text, ctx->no_antialias);

	/*get original bounds*/
//	orig_rc = ctx->original;
	gf_path_get_bounds(path, &orig_rc);

	/*get active texture window in pixels*/
	rc.width = INT2FIX(txh->width);
	rc.height = INT2FIX(txh->height);

	/*get scaling ratio so that active texture view is stretched to original bounds (std 2D shape texture mapping in MPEG4)*/
	sS = orig_rc.width / txh->width;
	sT = orig_rc.height / txh->height;
	
	gf_mx2d_init(gf_mx2d_txt);
	gf_mx2d_add_scale(&gf_mx2d_txt, sS, sT);
	/*apply texture transform*/
	get_gf_sr_texture_transform(ctx->appear, txh, &gf_sr_texture_transform, (txh == ctx->h_texture) ? 0 : 1, txh->width * sS, txh->height * sT);
	gf_mx2d_add_matrix(&gf_mx2d_txt, &gf_sr_texture_transform);

	/*move to bottom-left corner of bounds */
	gf_mx2d_add_translation(&gf_mx2d_txt, (orig_rc.x), (orig_rc.y - orig_rc.height));

	/*move to final coordinate system (except background which is built directly in final coord system)*/	
	if (!ctx->is_background) gf_mx2d_add_matrix(&gf_mx2d_txt, &ctx->transform);

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	r2d->stencil_set_matrix(txh->hwtx, &gf_mx2d_txt);


	if (!ctx->is_background) {
		/*texture alpha scale is the original material transparency, NOT the one after color transform*/
		r2d->stencil_set_gf_sr_texture_alpha(txh->hwtx, ctx->aspect.fill_alpha);
		if (!ctx->cmat.identity) r2d->stencil_set_color_matrix(txh->hwtx, &ctx->cmat);
		else r2d->stencil_reset_color_matrix(txh->hwtx);

		r2d->surface_set_matrix(surf->the_surface, &ctx->transform);
	} else {
		r2d->surface_set_matrix(surf->the_surface, NULL);
	}

	/*push path & draw*/
	r2d->surface_set_path(surf->the_surface, path);
	VS2D_DoFill(surf, ctx, txh->hwtx);
	r2d->surface_set_path(surf->the_surface, NULL);
	ctx->path_filled = 1;
}

void VS2D_TexturePath(VisualSurface2D *surf, GF_Path *path, struct _drawable_context *ctx)
{
#ifdef SKIP_DRAW
	return;
#endif
	if (!surf->the_surface || ctx->path_filled || !ctx->h_texture || surf->render->compositor->is_hidden) return;

	/*this is ambiguous in the spec, what if the material is filled and the texture is transparent ?
	let's draw, it's nicer */
	if (ctx->aspect.filled && ctx->h_texture->transparent) {
		VS2D_DrawPath(surf, path, ctx, NULL, NULL);
		ctx->path_filled = 0;
	}

	VS2D_TexturePathIntern(surf, path, NULL, ctx);
}

#define ADAPTATION_SIZE		0

void VS2D_DrawPath(VisualSurface2D *surf, GF_Path *path, DrawableContext *ctx, GF_STENCIL brush, GF_STENCIL pen)
{
	Bool dofill, dostrike;
	GF_Raster2D *r2d = surf->render->compositor->r2d;
#ifdef SKIP_DRAW
	return;
#endif
	
	assert(surf->the_surface);

	if (ctx->path_filled && ctx->path_stroke) {
		if (surf->render->compositor->draw_bvol) draw_clipper(surf, ctx);
		return;
	}

	if (!ctx->is_background) VS2D_SetOptions(surf->render, surf->the_surface, ctx->is_text, 0);

	dofill = dostrike = 0;
	if (!ctx->path_filled && ctx->aspect.filled) {
		dofill = 1;
		if (!brush) {
			brush = surf->the_brush;
			r2d->stencil_set_brush_color(brush, ctx->aspect.fill_color);
		}
	}


	/*compute width based on transform and top_level transform*/
	if (!ctx->path_stroke && ctx->aspect.pen_props.width) {
		dostrike = 1;
		if (!pen) {
			pen = surf->the_pen;
			r2d->stencil_set_brush_color(pen, ctx->aspect.line_color);
		}
	} else if (!dofill) {
		return;
	}

	/*set path transform, except for background2D node which is directly build in the final coord system*/
	r2d->surface_set_matrix(surf->the_surface, ctx->is_background ? NULL : &ctx->transform);

	/*fill path*/
	if (dofill) {
#if ADAPTATION_SIZE
		if ((ctx->clip.width<ADAPTATION_SIZE) && (ctx->clip.height<ADAPTATION_SIZE)) {
			r2d->surface_clear(surf->the_surface, &ctx->clip, ctx->aspect.fill_color);
		} else 
#endif
		{
			/*push path*/
			r2d->surface_set_path(surf->the_surface, path);
			VS2D_DoFill(surf, ctx, brush);
			r2d->surface_set_path(surf->the_surface, NULL);
		}
	}

	if (dostrike) {
#if ADAPTATION_SIZE
		if ((ctx->clip.width<ADAPTATION_SIZE) && (ctx->clip.height<ADAPTATION_SIZE)) {
		} else 
#endif
		{
			StrikeInfo2D *si = drawctx_get_strikeinfo(ctx, path);
			if (si && si->outline) {
				if (ctx->aspect.line_texture) {
					VS2D_TexturePathIntern(surf, si->outline, ctx->aspect.line_texture, ctx);
				} else {
					r2d->surface_set_path(surf->the_surface, si->outline);
					VS2D_DoFill(surf, ctx, pen);
				}
				/*that's ugly, but we cannot cache path outline for IFS2D/ILS2D*/
				if (path && !ctx->is_text && (path!=ctx->node->path) ) {
					gf_path_del(si->outline);
					si->outline = NULL;
				}
			}
		}
	}

	if (surf->render->compositor->draw_bvol) draw_clipper(surf, ctx);
}

void VS2D_FillRect(VisualSurface2D *surf, DrawableContext *ctx, GF_Rect rc, u32 color)
{
	GF_Path *path;
	GF_Raster2D *r2d = surf->render->compositor->r2d;
#ifdef SKIP_DRAW
	return;
#endif

	if (!surf->the_surface) return;
	if (ctx->path_filled && ctx->path_stroke) {
		if (surf->render->compositor->draw_bvol) draw_clipper(surf, ctx);
		return;
	}

	/*no aa*/
	VS2D_SetOptions(surf->render, surf->the_surface, 0, 1);

	r2d->stencil_set_brush_color(surf->the_brush, color);
	r2d->surface_set_matrix(surf->the_surface, &ctx->transform);
	path = gf_path_new();
	gf_path_add_move_to(path, rc.x, rc.y);
	gf_path_add_line_to(path, rc.x+rc.width, rc.y);
	gf_path_add_line_to(path, rc.x+rc.width, rc.y-rc.height);
	gf_path_add_line_to(path, rc.x, rc.y-rc.height);
	gf_path_close(path);

	/*push path*/
	r2d->surface_set_path(surf->the_surface, path);
	VS2D_DoFill(surf, ctx, surf->the_brush);
	r2d->surface_set_path(surf->the_surface, NULL);
	gf_path_del(path);
}
