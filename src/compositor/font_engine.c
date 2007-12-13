/*
 *			GPAC - Multimedia Framework C SDK
 *			
 *			Authors: Jean Le Feuvre 
 *
 *			Copyright (c) ENST 2007-200X
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#include <gpac/internal/compositor_dev.h>
#include <gpac/modules/font.h>
#include <gpac/options.h>
#include "visual_manager.h"

#include "texturing.h"

struct _gf_ft_mgr
{
	GF_FontReader *reader;

	GF_Font *font, *default_font;

	unsigned short *conv_buffer;
	u32 conv_buffer_size;
};


GF_FontManager *gf_font_manager_new(GF_User *user)
{
	char *def_font = "SERIF";
	u32 i, count;
	GF_FontManager *font_mgr;
	GF_FontReader *ifce;
	const char *opt;

	ifce = NULL;
	opt = gf_cfg_get_key(user->config, "FontEngine", "FontReader");
	if (opt) {
		ifce = (GF_FontReader *) gf_modules_load_interface_by_name(user->modules, opt, GF_FONT_READER_INTERFACE);
		if (ifce && ifce->init_font_engine(ifce) != GF_OK) {
			gf_modules_close_interface((GF_BaseInterface *)ifce);
			ifce = NULL;
		}
	}

	if (!ifce) {
		count = gf_modules_get_count(user->modules);
		for (i=0; i<count; i++) {
			ifce = (GF_FontReader *) gf_modules_load_interface(user->modules, i, GF_FONT_READER_INTERFACE);
			if (!ifce) continue;

			if (ifce->init_font_engine(ifce) != GF_OK) {
				gf_modules_close_interface((GF_BaseInterface *)ifce);
				ifce = NULL;
				continue;
			}

			gf_cfg_set_key(user->config, "FontEngine", "FontReader", ifce->module_name);
			break;
		}
	}
	GF_SAFEALLOC(font_mgr, GF_FontManager);
	font_mgr->reader = ifce;
	font_mgr->conv_buffer_size = 20;
	font_mgr->conv_buffer = malloc(sizeof(unsigned short)*font_mgr->conv_buffer_size);
	gf_font_manager_set_font(font_mgr, &def_font, 1, 0);
	font_mgr->default_font = font_mgr->font;
	return font_mgr;
}


void gf_font_del(GF_Font *font)
{
	GF_Glyph *glyph = font->glyph;

	while (glyph) {
		GF_Glyph *next = glyph->next;
		gf_path_del(glyph->path);
		free(glyph);
		glyph = next;
	}
	free(font->name);
	free(font);
}


void gf_font_manager_del(GF_FontManager *fm)
{
	GF_Font *font;
	if (fm->reader) {
		fm->reader->shutdown_font_engine(fm->reader);
		gf_modules_close_interface((GF_BaseInterface *)fm->reader);
	}

	font = fm->font;
	while (font) {
		GF_Font *next = font->next;
		gf_font_del(font);
		font = next;
	}
	free(fm->conv_buffer);
	free(fm);
}


GF_Font *gf_font_manager_set_font(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles)
{
	u32 i;
	GF_Err e;
	GF_Font *the_font = NULL;

	for (i=0; i<nb_fonts; i++) {
		GF_Font *font = fm->font;
		while (font) {
			if (!stricmp(font->name, alt_fonts[i])) {
				the_font = font;
				break;
			}
			font = font->next;
		}
		if (the_font) break;
		if (!fm->reader) continue;

		e = fm->reader->set_font(fm->reader, alt_fonts[i], styles);
		if (!e) {
			GF_SAFEALLOC(the_font, GF_Font);
			fm->reader->get_font_info(fm->reader, &the_font->name, &the_font->em_size, &the_font->ascent, &the_font->descent, &the_font->line_spacing, &the_font->max_advance_h, &the_font->max_advance_v);
			the_font->styles = styles;
		
			if (fm->font) {
				font = fm->font;
				while (font->next) font = font->next;
				font->next = the_font;
			} else {
				fm->font = the_font;
			}
			return the_font;
		}
	}
	if (!the_font) the_font = fm->default_font;
	fm->reader->set_font(fm->reader, the_font->name, the_font->styles);

	return the_font;
}

GF_Glyph *gf_font_get_glyph(GF_FontManager *fm, GF_Font *font, u32 name)
{
	GF_Glyph *glyph = font->glyph;
	while (glyph) {
		if (glyph->utf_name==name) return glyph;
		glyph = glyph->next;
	}
	/*load it*/
	if (!fm->reader) return NULL;
	fm->reader->set_font(fm->reader, font->name, font->styles);
	glyph = fm->reader->load_glyph(fm->reader, name);
	if (!glyph) return NULL;

	if (!font->glyph) font->glyph = glyph;
	else {
		GF_Glyph *a_glyph = font->glyph;
		while (a_glyph->next) a_glyph = a_glyph->next;
		a_glyph->next = glyph;
	}
	return glyph;
}


GF_TextSpan *gf_font_manager_create_span(GF_FontManager *fm, GF_Font *font, char *text, Fixed font_size, Bool needs_x_offset, Bool needs_y_offset)
{
	GF_Err e;
	u32 len, i;
	GF_TextSpan *span;

	len = fm->conv_buffer_size;
	e = fm->reader->get_glyphs(fm->reader, text, fm->conv_buffer, &len);
	if (e==GF_BUFFER_TOO_SMALL) {
		fm->conv_buffer_size = len;
		fm->conv_buffer = realloc(fm->conv_buffer, sizeof(unsigned short) * len);
		if (!fm->conv_buffer) return NULL;
		e = fm->reader->get_glyphs(fm->reader, text, fm->conv_buffer, &len);
	}
	if (e) return NULL;

	GF_SAFEALLOC(span, GF_TextSpan);
	span->font = font;
	span->font_size = font_size;
	span->font_scale = font_size / font->em_size;
	span->x_scale = span->y_scale = FIX_ONE;
	span->nb_glyphs = len;
	span->glyphs = malloc(sizeof(void *)*len);
	if (needs_x_offset) {
		span->dx = malloc(sizeof(Fixed)*len);
		memset(span->dx, 0, sizeof(Fixed)*len);
	}
	if (needs_y_offset) {
		span->dy = malloc(sizeof(Fixed)*len);
		memset(span->dy, 0, sizeof(Fixed)*len);
	}
	
	for (i=0; i<len; i++) {
		span->glyphs[i] = gf_font_get_glyph(fm, font, fm->conv_buffer[i]);
	}
	return span;
}



typedef struct _span_internal 
{
	/*zoom when texture was computed*/
	Fixed last_zoom;
	/*texture handler*/
	GF_TextureHandler *txh;
	/*texture path (rectangle)*/
	GF_Path *path;

#ifndef GPAC_DISABLE_3D
	/*span mesh (built out of the # glyphs*/
	GF_Mesh *mesh;
	/*span outline*/
	GF_Mesh *outline;
#endif
} GF_SpanExtensions;


void gf_font_manager_delete_span(GF_FontManager *fm, GF_TextSpan *span)
{
	free(span->glyphs);
	if (span->dx) free(span->dx);
	if (span->dy) free(span->dy);

	if (span->ext) {
		if (span->ext->mesh) mesh_free(span->ext->mesh);
		if (span->ext->outline) mesh_free(span->ext->outline);
		if (span->ext->txh) {
			gf_sc_texture_destroy(span->ext->txh);
			if (span->ext->txh->data) free(span->ext->txh->data);
			free(span->ext->txh);
		}
		free(span->ext);
	}
	free(span);
}

void gf_font_manager_refresh_span_bounds(GF_TextSpan *span)
{
	u32 i;
	Fixed size;
	Fixed min_x, min_y, width, height;

	width = span->horizontal ? 0 : gf_mulfix(span->font->max_advance_h, span->font_scale);
	height = !span->horizontal ? 0 : gf_mulfix(span->font->ascent-span->font->descent, span->font_scale);
	min_x = span->dx ? FIX_MAX : span->off_x;
	min_y = span->dy ? FIX_MAX : span->off_y;
	for (i=0;i<span->nb_glyphs; i++) {
		size = gf_mulfix(span->glyphs[i] ? span->glyphs[i]->horiz_advance : span->font->max_advance_h, span->font_scale);
		if (span->dx) {
			if (span->dx[i]<min_x) {
				if (i) width += min_x-span->dx[i];
				min_x = span->dx[i];
			}
			if (span->dx[i] + size > width + min_x) width = span->dx[i] + size - min_x;
		} else if (span->horizontal) {
			width += size;
		}
		if (span->dy) {
			if (span->dy[i]<min_y) {
				if (i) height += min_y-span->dy[i];
				min_y = span->dy[i];
			}
			size = gf_mulfix(span->glyphs[i] ? span->glyphs[i]->vert_advance : span->font->max_advance_v, span->font_scale);
			if (span->dy[i] + size > height + min_y) height = span->dy[i] + size - min_y;
		} else if (!span->horizontal) {
			height += gf_mulfix(span->font->ascent-span->font->descent, span->font_scale);
		}
	}
	span->bounds.x = min_x;
	span->bounds.width = width;
	span->bounds.y = min_y - gf_mulfix(span->font->ascent, span->font_scale) + height;
	span->bounds.height = height;
}



static GF_Path *span_create_path(GF_TextSpan *span)
{
	u32 i;
	GF_Matrix2D mat;
	Fixed dx, dy;
	GF_Path *path = gf_path_new();

	gf_mx2d_init(mat);
	mat.m[0] = gf_mulfix(span->font_scale, span->x_scale);
	mat.m[4] = gf_mulfix(span->font_scale, span->y_scale);

	dx = gf_divfix(span->off_x, mat.m[0]);
	dy = gf_divfix(span->off_y, mat.m[4]);

	for (i=0; i<span->nb_glyphs; i++) {
		if (!span->glyphs[i]) {
			if (span->horizontal) {
				dx += INT2FIX(span->font->max_advance_h);
			} else {
				dy -= INT2FIX(span->font->max_advance_v);
			}
		} else {
			gf_path_add_subpath(path, span->glyphs[i]->path, dx, dy);

			if (span->horizontal) {
				dx += INT2FIX(span->glyphs[i]->horiz_advance);
			} else {
				dy -= INT2FIX(span->glyphs[i]->vert_advance);
			}
		}
	}

	for (i=0; i<path->n_points; i++) {
		gf_mx2d_apply_point(&mat, &path->points[i]);
	}
	return path;
}

static void span_alloc_extensions(GF_TextSpan *span)
{
	if (span->ext) return;
	GF_SAFEALLOC(span->ext, GF_SpanExtensions);
}

#ifndef GPAC_DISABLE_3D
static void span_build_mesh(GF_TextSpan *span)
{
	span_alloc_extensions(span);
	span->ext->mesh = new_mesh();
	mesh_set_vertex(span->ext->mesh, span->bounds.x, span->bounds.y-span->bounds.height, 0, 0, 0, FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(span->ext->mesh, span->bounds.x+span->bounds.width, span->bounds.y-span->bounds.height, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(span->ext->mesh, span->bounds.x+span->bounds.width, span->bounds.y, 0, 0, 0, FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(span->ext->mesh, span->bounds.x, span->bounds.y, 0, 0, 0, FIX_ONE, 0, 0);
	mesh_set_triangle(span->ext->mesh, 0, 1, 2);
	mesh_set_triangle(span->ext->mesh, 0, 2, 3);
	span->ext->mesh->flags |= MESH_IS_2D;
	span->ext->mesh->mesh_type = MESH_TRIANGLES;
	mesh_update_bounds(span->ext->mesh);
}
#endif

/*don't build too large textures*/
#define MAX_TX_SIZE		512
/*and don't build too small ones otherwise result is as crap as non-textured*/
#define MIN_TX_SIZE		16

static Bool span_setup_texture(GF_Compositor *compositor, GF_TextSpan *span, Bool for_3d)
{
	GF_Path *span_path;
	GF_Rect bounds;
	Fixed cx, cy, sx, sy, max, min;
	u32 tw, th;
	GF_Matrix2D mx;
	GF_STENCIL stencil, brush;
	GF_SURFACE surface;
	u32 width, height;
	Fixed scale;
	GF_Raster2D *raster = compositor->rasterizer;

	span_alloc_extensions(span);

	/*something failed*/
	if (span->ext->txh && !span->ext->txh->data) return 0;

	if (span->ext->txh && span->ext->txh->data) {
		if (span->ext->last_zoom == compositor->zoom) {

#ifndef GPAC_DISABLE_3D
			if (for_3d && !span->ext->mesh) span_build_mesh(span);
#endif
			return 1;
		}
	}
	span->ext->last_zoom = compositor->zoom;

	bounds = span->bounds;

	/*check not too big, but also not really small (meter metrics)*/
	max = INT2FIX(MAX_TX_SIZE);
	min = INT2FIX(MIN_TX_SIZE);
	scale = compositor->zoom;
	if ((gf_mulfix(scale, bounds.width)>max) || (gf_mulfix(scale, bounds.height)>max)) {
		scale = MIN(gf_divfix(max, bounds.width), gf_divfix(max, bounds.height));
	} 
	else if ((gf_mulfix(scale, bounds.width)<min) || (gf_mulfix(scale, bounds.height)<min)) {
		scale = MAX(gf_divfix(min, bounds.width), gf_divfix(min, bounds.height));
	}
	if (scale<FIX_ONE) scale = FIX_ONE;

	/*get closest pow2 sizes*/
	tw = FIX2INT( gf_ceil(gf_mulfix(scale, bounds.width)) );
	width = MIN_TX_SIZE;
	while (width <tw) {
		if (width >=MAX_TX_SIZE) break;
		width *=2;
	}
	th = FIX2INT( gf_ceil(gf_mulfix(scale, bounds.height)) );
	height = MIN_TX_SIZE;
	while (height<th) {
		height*=2;
		if (height>=MAX_TX_SIZE) break;
	}
	/*and get scaling*/
	sx = gf_divfix( INT2FIX(width), bounds.width);
	sy = gf_divfix( INT2FIX(height), bounds.height);

	if (span->ext->txh && (width == span->ext->txh->width) && (height==span->ext->txh->height)) return 1;

	if (span->ext->path) gf_path_del(span->ext->path);
	span->ext->path = NULL;
#ifndef GPAC_DISABLE_3D
	if (span->ext->mesh) mesh_free(span->ext->mesh);
	span->ext->mesh = NULL;
#endif

	if (span->ext->txh) {
		gf_sc_texture_release(span->ext->txh);
		if (span->ext->txh->data) free(span->ext->txh->data);
		free(span->ext->txh);
	}
	GF_SAFEALLOC(span->ext->txh, GF_TextureHandler);
	span->ext->txh->compositor = compositor;
	gf_sc_texture_allocate(span->ext->txh);
	stencil = gf_sc_texture_get_stencil(span->ext->txh);
	if (!stencil) stencil = raster->stencil_new(raster, GF_STENCIL_TEXTURE);

	/*FIXME - make it work with alphagrey...*/
	span->ext->txh->width = width;
	span->ext->txh->height = height;
	span->ext->txh->stride = 4*width;
	span->ext->txh->pixelformat = GF_PIXEL_RGBA;
	span->ext->txh->transparent = 1;
	span->ext->txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;

	surface = raster->surface_new(raster, 1);
	if (!surface) {
		gf_sc_texture_release(span->ext->txh);
		return 0;
	}
	span->ext->txh->data = (char *) malloc(sizeof(char)*span->ext->txh->stride*span->ext->txh->height);
	memset(span->ext->txh->data, 0, sizeof(char)*span->ext->txh->stride*span->ext->txh->height);

	raster->stencil_set_texture(stencil, span->ext->txh->data, span->ext->txh->width, span->ext->txh->height, span->ext->txh->stride, span->ext->txh->pixelformat, span->ext->txh->pixelformat, 1);
	raster->surface_attach_to_texture(surface, stencil);

	brush = raster->stencil_new(raster, GF_STENCIL_SOLID);
	raster->stencil_set_brush_color(brush, 0xFF000000);

	cx = bounds.x + bounds.width/2; 
	cy = bounds.y - bounds.height/2;

	gf_mx2d_init(mx);
	gf_mx2d_add_translation(&mx, -cx, -cy);
	gf_mx2d_add_scale(&mx, sx, sy);
//	gf_mx2d_add_scale(&mx, 99*FIX_ONE/100, 99*FIX_ONE/100);

	raster->surface_set_matrix(surface, &mx);
	raster->surface_set_raster_level(surface, GF_RASTER_HIGH_QUALITY);
	span_path = span_create_path(span);
	raster->surface_set_path(surface, span_path);

	raster->surface_fill(surface, brush);
	raster->stencil_delete(brush);
	raster->surface_delete(surface);
	gf_path_del(span_path);

	span->ext->path = gf_path_new();
	gf_path_add_move_to(span->ext->path, bounds.x, bounds.y-bounds.height);
	gf_path_add_line_to(span->ext->path, bounds.x+bounds.width, bounds.y-bounds.height);
	gf_path_add_line_to(span->ext->path, bounds.x+bounds.width, bounds.y);
	gf_path_add_line_to(span->ext->path, bounds.x, bounds.y);
	gf_path_close(span->ext->path);


	gf_sc_texture_set_stencil(span->ext->txh, stencil);
	gf_sc_texture_set_data(span->ext->txh);

#ifndef GPAC_DISABLE_3D
	gf_sc_texture_set_blend_mode(span->ext->txh, TX_BLEND);
	if (for_3d) span_build_mesh(span);
#endif
	return 1;
}


#ifndef GPAC_DISABLE_3D

void gf_font_span_fill_3d(GF_TextSpan *span, GF_TraverseState *tr_state)
{
	span_alloc_extensions(span);
	if (!span->ext->mesh) {
		GF_Path *path = span_create_path(span);
		span->ext->mesh = new_mesh();
		mesh_from_path(span->ext->mesh, path);
		gf_path_del(path);
	}

	visual_3d_mesh_paint(tr_state, span->ext->mesh);
}

void gf_font_span_strike_3d(GF_TextSpan *span, GF_TraverseState *tr_state, DrawAspect2D *asp, Bool vect_outline)
{
	span_alloc_extensions(span);
	if (!span->ext->outline) {
		GF_Path *path = span_create_path(span);
		span->ext->outline = new_mesh();
#ifndef GPAC_USE_OGL_ES
		if (vect_outline) {
			GF_Path *outline = gf_path_get_outline(path, asp->pen_props);
			TesselatePath(span->ext->outline, outline, asp->line_texture ? 2 : 1);
			gf_path_del(outline);
		} else {
			mesh_get_outline(span->ext->outline, path);
		}
#else
		/*VECTORIAL TEXT OUTLINE NOT SUPPORTED ON OGL-ES AT CURRENT TIME*/
		vect_outline = 0;
		mesh_get_outline(span->ext->outline, path);
#endif
		gf_path_del(path);
	}
	if (vect_outline) {
		visual_3d_mesh_paint(tr_state, span->ext->outline);
	} else {
		visual_3d_mesh_strike(tr_state, span->ext->outline, asp->pen_props.width, asp->line_scale, asp->pen_props.dash);
	}
}




#endif



void gf_font_span_draw_2d(GF_TraverseState *tr_state, GF_TextSpan *span, DrawableContext *ctx)
{
	u32 flags, i;
	Bool flip_text;
	Fixed dx, dy, sx, sy, lscale;
	Bool has_texture = ctx->aspect.fill_texture ? 1 : 0;
	GF_Matrix2D mx, tx;

	gf_mx2d_copy(mx, ctx->transform);

	flags = ctx->flags;
	dx = span->off_x;
	dy = span->off_y;
	sx = gf_mulfix(span->font_scale, span->x_scale);
	sy = gf_mulfix(span->font_scale, span->y_scale);
	flip_text = (! tr_state->visual->center_coords) ? 1 : 0;

	lscale = ctx->aspect.line_scale;
	ctx->aspect.line_scale = gf_divfix(ctx->aspect.line_scale, span->font_scale);

	for (i=0; i<span->nb_glyphs; i++) {
		if (!span->glyphs[i]) {
			if (span->horizontal) {
				dx += sx * span->font->max_advance_h;
			} else {
				dy -= sy * span->font->max_advance_v;
			}
			continue;
		}
		ctx->transform.m[0] = sx;
		ctx->transform.m[4] = flip_text  ? -sy : sy;
		ctx->transform.m[1] = ctx->transform.m[3] = 0;
		ctx->transform.m[2] = span->dx ? span->dx[i] : dx;
		ctx->transform.m[5] = span->dy ? span->dy[i] : dy;
		gf_mx2d_add_matrix(&ctx->transform, &mx);

		if (has_texture) {
			tx.m[0] = sx;
			tx.m[4] = sy;
			tx.m[1] = tx.m[3] = 0;
			tx.m[2] = span->dx ? span->dx[i] : dx;
			tx.m[5] = span->dy ? span->dy[i] : dy;
			gf_mx2d_inverse(&tx);

			visual_2d_texture_path_extended(tr_state->visual, span->glyphs[i]->path, ctx->aspect.fill_texture, ctx, &span->bounds, &tx);
		}

		visual_2d_draw_path(tr_state->visual, span->glyphs[i]->path, ctx, NULL, NULL);
		ctx->flags = flags;

		if (span->horizontal) {
			dx += sx * span->glyphs[i]->horiz_advance;
		} else {
			dy -= sy * span->glyphs[i]->vert_advance;
		}
	}
	gf_mx2d_copy(ctx->transform, mx);
	ctx->aspect.line_scale = lscale;
}

void gf_font_spans_draw_2d(GF_List *spans, GF_TraverseState *tr_state, DrawableContext *ctx, u32 hl_color, Bool force_texture_text)
{
	Bool use_texture_text;
	u32 i = 0;
	GF_TextSpan *span;

	use_texture_text = 0;
	if (force_texture_text || (tr_state->visual->compositor->texture_text_mode==GF_TEXTURE_TEXT_ALWAYS) ) {
		use_texture_text = !ctx->aspect.fill_texture && !ctx->aspect.pen_props.width;
	}

	i=ctx->sub_path_index ? ctx->sub_path_index-1 : 0;
	while ((span = (GF_TextSpan *)gf_list_enum(spans, &i))) {
		
		if (hl_color) visual_2d_fill_rect(tr_state->visual, ctx, &span->bounds, hl_color, 0);

		if (use_texture_text && span_setup_texture(tr_state->visual->compositor, span, 0)) {
			visual_2d_texture_path_text(tr_state->visual, ctx, span->ext->path, &span->bounds, span->ext->txh);
		} else {
			gf_font_span_draw_2d(tr_state, span, ctx);
		}
		if (ctx->sub_path_index) break;
	}
}
