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



#include "nodes_stacks.h"
#include "visual_manager.h"
#include "mpeg4_grouping.h"
#include "texturing.h"
#include <gpac/utf.h>
#include <gpac/options.h>

/*default value when no fontStyle*/
#define FSFAMILY	(fs && fs->family.count) ? (const char *)fs->family.vals[0]	: ""

/*here it's tricky since it depends on our metric system...*/
#define FSSIZE		(fs ? fs->size : -1)
#define FSSTYLE		(fs && fs->style.buffer) ? (const char *)fs->style.buffer : ""
#define FSMAJOR		( (fs && fs->justify.count && fs->justify.vals[0]) ? (const char *)fs->justify.vals[0] : "FIRST")
#define FSMINOR		( (fs && (fs->justify.count>1) && fs->justify.vals[1]) ? (const char *)fs->justify.vals[1] : "FIRST")
#define FSHORIZ		(fs ? fs->horizontal : 1)
#define FSLTR		(fs ? fs->leftToRight : 1)
#define FSTTB		(fs ? fs->topToBottom : 1)
#define FSLANG		(fs ? fs->language : "")
#define FSSPACE		(fs ? fs->spacing : 1)


/*exported to access the strike list ...*/
typedef struct
{
	Drawable *graph;
	Fixed ascent, descent;
	GF_List *text_lines;
	GF_Rect bounds;
	u32 texture_text_flag;
	GF_Compositor *compositor;
} TextStack;

typedef struct
{
	Fixed last_zoom;
	GF_TextSpan *span;
	GF_TextureHandler *txh;
	/*texture path (rectangle)*/
	GF_Path *path;
#ifndef GPAC_DISABLE_3D
	GF_Mesh *mesh;
	GF_Mesh *outline;
#endif
} GF_TextLine;


void text_clean_paths(GF_Compositor *compositor, TextStack *stack)
{
	GF_TextLine *line;
	/*delete all path objects*/
	while (gf_list_count(stack->text_lines)) {
		line = (GF_TextLine *) gf_list_get(stack->text_lines, 0);
		gf_list_rem(stack->text_lines, 0);
		gf_font_manager_delete_span(compositor->font_manager, line->span);
		if (line->mesh) mesh_free(line->mesh);
		if (line->outline) mesh_free(line->outline);
		if (line->txh) {
			gf_sc_texture_destroy(line->txh);
			if (line->txh->data) free(line->txh->data);
			free(line->txh);
		}
		free(line);
	}
	stack->bounds.width = stack->bounds.height = 0;
	drawable_reset_path(stack->graph);
}


static void build_text_split(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	u32 i, j, k, len, styles, idx, first_char;
	Bool split_words = 0;
	GF_Font *font;
	GF_TextSpan *tspan;
	GF_FontManager *ft_mgr = tr_state->visual->compositor->font_manager;
	Fixed fontSize, start_y;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }

	styles = 0;
	if (fs && fs->style.buffer) {
		if (strstr(fs->style.buffer, "BOLD") || strstr(fs->style.buffer, "bold")) styles |= GF_FONT_BOLD;
		if (strstr(fs->style.buffer, "ITALIC") || strstr(fs->style.buffer, "italic")) styles |= GF_FONT_ITALIC;
		if (strstr(fs->style.buffer, "UNDERLINED") || strstr(fs->style.buffer, "underlined")) styles |= GF_FONT_UNDERLINED;
	}

	font = gf_font_manager_set_font(ft_mgr, fs ? fs->family.vals : NULL, fs ? fs->family.count : 0, styles);
	if (!font) return;

	st->ascent = (fontSize*font->ascent) / font->em_size;
	st->descent = -(fontSize*font->descent) / font->em_size;

	if (!strcmp(FSMINOR, "MIDDLE")) {
		start_y = (st->descent + st->ascent)/2;
	}
	else if (!strcmp(FSMINOR, "BEGIN")) {
		start_y = st->descent;
	}
	else if (!strcmp(FSMINOR, "END")) {
		start_y = st->descent + st->ascent;
	}
	else {
		start_y = st->ascent;
	}
	
	st->bounds.width = st->bounds.x = st->bounds.height = 0;
	idx = 0;
	split_words = (tr_state->text_split_mode==1) ? 1 : 0;

	for (i=0; i < txt->string.count; i++) {

		char *str = txt->string.vals[i];
		if (!str) continue;

		tspan = gf_font_manager_create_span(ft_mgr, font, str, fontSize);
		len = tspan->nb_glyphs;
		tspan->horizontal = 1;

		first_char = 0;
		for (j=0; j<len; j++) {
			u32 is_space = 0;
			GF_TextLine *txt_line;

			/*we currently only split sentences at spaces*/
			if (tspan->glyphs[j]->utf_name == (unsigned short) ' ') is_space = 1;
			if (split_words && (j+1!=len) && !is_space) 
				continue;

			GF_SAFEALLOC(txt_line, GF_TextLine);

			txt_line->span = (GF_TextSpan*) malloc(sizeof(GF_TextSpan));
			memcpy(txt_line->span, tspan, sizeof(GF_TextSpan));

			txt_line->span->nb_glyphs = split_words ? (j - first_char) : 1;
			if (split_words && !is_space) txt_line->span->nb_glyphs++;
			txt_line->span->glyphs = malloc(sizeof(void *)*txt_line->span->nb_glyphs);

			txt_line->span->bounds.height = st->ascent + st->descent;
			txt_line->span->bounds.y = start_y;
			txt_line->span->bounds.x = 0;
			txt_line->span->bounds.width = 0;

			if (split_words) {
				for (k=0; k<txt_line->span->nb_glyphs; k++) {
					txt_line->span->glyphs[k] = tspan->glyphs[FSLTR ? (first_char+k) : (len - first_char - k - 1)];
					txt_line->span->bounds.width += tspan->font_scale * (txt_line->span->glyphs[k] ? txt_line->span->glyphs[k]->horiz_advance : tspan->font->max_advance_h);
				}
			} else {
				txt_line->span->glyphs[0] = tspan->glyphs[FSLTR ? j : (len - j - 1) ];
				txt_line->span->glyphs[0] = tspan->glyphs[j];
				txt_line->span->bounds.width = tspan->font_scale * (txt_line->span->glyphs[0] ? txt_line->span->glyphs[0]->horiz_advance : tspan->font->max_advance_h);
			}

			gf_list_add(st->text_lines, txt_line);

			/*request a context (first one is always valid when entering sort phase)*/
			if (idx) parent_node_start_group(tr_state->parent, NULL, 0);

			idx++;
			parent_node_end_text_group(tr_state->parent, &txt_line->span->bounds, st->ascent, st->descent, idx);

			if (is_space && split_words) {
				GF_SAFEALLOC(txt_line, GF_TextLine);
				txt_line->span = (GF_TextSpan*) malloc(sizeof(GF_TextSpan));
				memcpy(txt_line->span, tspan, sizeof(GF_TextSpan));
				txt_line->span->nb_glyphs = 1;
				txt_line->span->glyphs = malloc(sizeof(void *));

				gf_list_add(st->text_lines, txt_line);
				txt_line->span->bounds.height = st->ascent + st->descent;
				txt_line->span->bounds.y = start_y;
				txt_line->span->bounds.x = 0;
				k = (j - first_char);
				txt_line->span->glyphs[0] = tspan->glyphs[FSLTR ? (first_char+k) : (len - first_char - k - 1)];
				txt_line->span->bounds.width = tspan->font_scale * (txt_line->span->glyphs[0] ? txt_line->span->glyphs[0]->horiz_advance : tspan->font->max_advance_h);
				parent_node_start_group(tr_state->parent, NULL, 1);
				idx++;
				parent_node_end_text_group(tr_state->parent, &txt_line->span->bounds, st->ascent, st->descent, idx);
			}
			first_char = j+1;
		}
		gf_font_manager_delete_span(ft_mgr, tspan);
	}
}


static void build_text(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	u32 i, j, int_major, k, styles, count;
	Fixed fontSize, start_x, start_y, line_spacing, tot_width, tot_height, max_scale, space;
	GF_Font *font;
	Bool horizontal;
	GF_FontManager *ft_mgr = tr_state->visual->compositor->font_manager;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }
	horizontal = FSHORIZ;

	styles = 0;
	if (fs && fs->style.buffer) {
		if (strstr(fs->style.buffer, "BOLD") || strstr(fs->style.buffer, "bold")) styles |= GF_FONT_BOLD;
		if (strstr(fs->style.buffer, "ITALIC") || strstr(fs->style.buffer, "italic")) styles |= GF_FONT_ITALIC;
		if (strstr(fs->style.buffer, "UNDERLINED") || strstr(fs->style.buffer, "underlined")) styles |= GF_FONT_UNDERLINED;
	}

	font = gf_font_manager_set_font(ft_mgr, fs ? fs->family.vals : NULL, fs ? fs->family.count : 0, styles);
	if (!font) return;

	/*NOTA: we could use integer maths here but we have a risk of overflow with large fonts, so use fixed maths*/
	st->ascent = gf_muldiv(fontSize, INT2FIX(font->ascent), INT2FIX(font->em_size));
	st->descent = -gf_muldiv(fontSize, INT2FIX(font->descent), INT2FIX(font->em_size));
	space = gf_muldiv(fontSize, INT2FIX(font->line_spacing), INT2FIX(font->em_size)) ;
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	tot_width = tot_height = 0;
	for (i=0; i < txt->string.count; i++) {
		GF_TextSpan *tspan;
		GF_TextLine *line;
		u32 size;
		char *str = txt->string.vals[i];
		if (!str) continue;

		tspan = gf_font_manager_create_span(ft_mgr, font, txt->string.vals[i], fontSize);
		if (!tspan) continue;
		
		GF_SAFEALLOC(line, GF_TextLine);
		line->span = tspan;

		tspan->horizontal = horizontal;

		if ((horizontal && !FSLTR) || (!horizontal && !FSTTB)) {
			for (k=0; k<tspan->nb_glyphs/2; k++) {
				GF_Glyph *g = tspan->glyphs[k];
				tspan->glyphs[k] = tspan->glyphs[tspan->nb_glyphs-1-k];
				tspan->glyphs[tspan->nb_glyphs-k-1] = g;
			}
		}

		size = 0;
		for (j=0; j<tspan->nb_glyphs; j++) {
			if (horizontal) {
				size += tspan->glyphs[j] ? tspan->glyphs[j]->horiz_advance : tspan->font->max_advance_h;
			} else {
				size += tspan->glyphs[j] ? tspan->glyphs[j]->vert_advance : tspan->font->max_advance_v;
			}
		}
		gf_list_add(st->text_lines, line);

		if (horizontal) {
			tspan->bounds.width = tspan->font_scale * size;
			/*apply length*/
			if ((txt->length.count>i) && (txt->length.vals[i]>0)) {
				tspan->x_scale = gf_divfix(txt->length.vals[i], tspan->bounds.width);
				tspan->bounds.width = txt->length.vals[i];
			}
			if (tot_width < tspan->bounds.width ) tot_width = tspan->bounds.width ;
		} else {
			tspan->bounds.height = tspan->font_scale * size;

			/*apply length*/
			if ((txt->length.count>i) && (txt->length.vals[i]>0)) {
				tspan->y_scale = gf_divfix(txt->length.vals[i], tspan->bounds.height);
				tspan->bounds.height = txt->length.vals[i];
			}
			if (tot_height < tspan->bounds.height) tot_height = tspan->bounds.height;
		}
	}
	
	max_scale = FIX_ONE;
	if (horizontal) {
		if ((txt->maxExtent > 0) && (tot_width>txt->maxExtent)) {
			max_scale = gf_divfix(txt->maxExtent, tot_width);
			tot_width = txt->maxExtent;
		}
		tot_height = (txt->string.count-1) * line_spacing + (st->ascent + st->descent);
		st->bounds.height = tot_height;

		if (!strcmp(FSMINOR, "MIDDLE")) {
			if (FSTTB) {
				start_y = tot_height/2;
				st->bounds.y = start_y;
			} else {
				start_y = st->descent + st->ascent - tot_height/2;
				st->bounds.y = tot_height/2;
			}
		}
		else if (!strcmp(FSMINOR, "BEGIN")) {
			if (FSTTB) {
				start_y = st->descent;
				start_y = 0;
				st->bounds.y = start_y;
			} else {
				st->bounds.y = st->bounds.height;
				start_y = st->descent + st->ascent;
			}
		}
		else if (!strcmp(FSMINOR, "END")) {
			if (FSTTB) {
				start_y = tot_height;
				st->bounds.y = start_y;
			} else {
				start_y = -tot_height + 2*st->descent + st->ascent;
				st->bounds.y = start_y - (st->descent + st->ascent) + tot_height;
			}
		}
		else {
			start_y = st->ascent;
			st->bounds.y = FSTTB ? start_y : (tot_height - st->descent);
		}
	} else {
		if ((txt->maxExtent > 0) && (tot_height>txt->maxExtent) ) {
			max_scale = gf_divfix(txt->maxExtent, tot_height);
			tot_height = txt->maxExtent;
		}
		tot_width = txt->string.count * line_spacing;
		st->bounds.width = tot_width;

		if (!strcmp(FSMINOR, "MIDDLE")) {
			if (FSLTR) {
				start_x = -tot_width/2;
				st->bounds.x = start_x;
			} else {
				start_x = tot_width/2 - line_spacing;
				st->bounds.x = - tot_width + line_spacing;
			}
		}
		else if (!strcmp(FSMINOR, "END")) {
			if (FSLTR) {
				start_x = -tot_width;
				st->bounds.x = start_x;
			} else {
				start_x = tot_width-line_spacing;
				st->bounds.x = 0;
			}
		}
		else {
			if (FSLTR) {
				start_x = 0;
				st->bounds.x = start_x;
			} else {
				start_x = -line_spacing;
				st->bounds.x = -tot_width;
			}
		}
	}
			

	/*major-justification*/
	if (!strcmp(FSMAJOR, "MIDDLE") ) {
		int_major = 0;
	} else if (!strcmp(FSMAJOR, "END") ) {
		int_major = 1;
	} else {
		int_major = 2;
	}

	st->bounds.width = st->bounds.height = 0;

	count = gf_list_count(st->text_lines);
	for (i=0; i < count; i++) {
		GF_TextLine *line = gf_list_get(st->text_lines, i);
		switch (int_major) {
		/*major-justification MIDDLE*/
		case 0:
			if (horizontal) {
				start_x = -line->span->bounds.width/2;
			} else {
				start_y = FSTTB ? line->span->bounds.height/2 : (-line->span->bounds.height/2 + space);
			}
			break;
		/*major-justification END*/
		case 1:
			if (horizontal) {
				start_x = (FSLTR) ? -line->span->bounds.width : 0;
			} else {
				start_y = FSTTB ? line->span->bounds.height : (-line->span->bounds.height + space);
			}
			break;
		/*BEGIN, FIRST or default*/
		default:
			if (horizontal) {
				start_x = (FSLTR) ? 0 : -line->span->bounds.width;
			} else {
				start_y = FSTTB ? 0 : space;
			}
			break;
		}
		line->span->off_x = start_x;
		line->span->bounds.x = start_x;
		if (horizontal) {
			line->span->off_y = start_y - st->ascent;
			line->span->x_scale = gf_mulfix(line->span->x_scale, max_scale);
			line->span->bounds.y = start_y;
		} else {
			line->span->y_scale = gf_mulfix(line->span->y_scale, max_scale);
			line->span->off_y = start_y - gf_mulfix(st->ascent, line->span->y_scale);
			line->span->bounds.y = start_y;
		}

		if (horizontal) {
			start_y += FSTTB ? -line_spacing : line_spacing;
			line->span->bounds.height = st->descent + st->ascent;
		} else {
			start_x += FSLTR ? line_spacing : -line_spacing;
			line->span->bounds.width = line_spacing;
		}
		gf_rect_union(&st->bounds, &line->span->bounds);
	}
}

static GF_Path *text_create_span_path(GF_TextLine *tl)
{
	u32 i;
	GF_Matrix2D mat;
	Fixed dx, dy;
	GF_Path *path = gf_path_new();

	gf_mx2d_init(mat);
	mat.m[0] = gf_mulfix(tl->span->font_scale, tl->span->x_scale);
	mat.m[4] = gf_mulfix(tl->span->font_scale, tl->span->y_scale);

	dx = gf_divfix(tl->span->off_x, mat.m[0]);
	dy = gf_divfix(tl->span->off_y, mat.m[4]);

	for (i=0; i<tl->span->nb_glyphs; i++) {

		if (!tl->span->glyphs[i]) {
			if (tl->span->horizontal) {
				dx += INT2FIX(tl->span->font->max_advance_h);
			} else {
				dy -= INT2FIX(tl->span->font->max_advance_v);
			}
		} else {
			gf_path_add_subpath(path, tl->span->glyphs[i]->path, dx, dy);

			if (tl->span->horizontal) {
				dx += INT2FIX(tl->span->glyphs[i]->horiz_advance);
			} else {
				dy -= INT2FIX(tl->span->glyphs[i]->vert_advance);
			}
		}
	}

	for (i=0; i<path->n_points; i++) {
		gf_mx2d_apply_point(&mat, &path->points[i]);
	}

	return path;
}


#ifndef GPAC_DISABLE_3D
static void text_span_build_mesh(GF_TextLine *tl)
{
	tl->mesh = new_mesh();
	mesh_set_vertex(tl->mesh, tl->span->bounds.x, tl->span->bounds.y-tl->span->bounds.height, 0, 0, 0, FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(tl->mesh, tl->span->bounds.x+tl->span->bounds.width, tl->span->bounds.y-tl->span->bounds.height, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(tl->mesh, tl->span->bounds.x+tl->span->bounds.width, tl->span->bounds.y, 0, 0, 0, FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(tl->mesh, tl->span->bounds.x, tl->span->bounds.y, 0, 0, 0, FIX_ONE, 0, 0);
	mesh_set_triangle(tl->mesh, 0, 1, 2);
	mesh_set_triangle(tl->mesh, 0, 2, 3);
	tl->mesh->flags |= MESH_IS_2D;
	tl->mesh->mesh_type = MESH_TRIANGLES;
	mesh_update_bounds(tl->mesh);
}
#endif

/*don't build too large textures*/
#define MAX_TX_SIZE		512
/*and don't build too small ones otherwise result is as crap as non-textured*/
#define MIN_TX_SIZE		16

static Bool text_setup_span_texture(GF_Compositor *compositor, GF_TextLine *tl, Bool for_3d)
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

	/*something failed*/
	if (tl->txh && !tl->txh->data) return 0;

	if (tl->txh && tl->txh->data) {
		if (tl->last_zoom == compositor->zoom) {

#ifndef GPAC_DISABLE_3D
			if (for_3d && !tl->mesh) text_span_build_mesh(tl);
#endif
			return 1;
		}
	}
	tl->last_zoom = compositor->zoom;

	bounds = tl->span->bounds;

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

	if (tl->txh && (width == tl->txh->width) && (height==tl->txh->height)) return 1;

	if (tl->path) gf_path_del(tl->path);
	tl->path = NULL;
#ifndef GPAC_DISABLE_3D
	if (tl->mesh) mesh_free(tl->mesh);
	tl->mesh = NULL;
#endif

	if (tl->txh) {
		gf_sc_texture_release(tl->txh);
		if (tl->txh->data) free(tl->txh->data);
		free(tl->txh);
	}
	GF_SAFEALLOC(tl->txh, GF_TextureHandler);
	tl->txh->compositor = compositor;
	gf_sc_texture_allocate(tl->txh);
	stencil = gf_sc_texture_get_stencil(tl->txh);
	if (!stencil) stencil = raster->stencil_new(raster, GF_STENCIL_TEXTURE);

	/*FIXME - make it work with alphagrey...*/
	tl->txh->width = width;
	tl->txh->height = height;
	tl->txh->stride = 4*width;
	tl->txh->pixelformat = GF_PIXEL_RGBA;
	tl->txh->transparent = 1;
	tl->txh->flags |= GF_SR_TEXTURE_NO_GL_FLIP;

	surface = raster->surface_new(raster, 1);
	if (!surface) {
		gf_sc_texture_release(tl->txh);
		return 0;
	}
	tl->txh->data = (char *) malloc(sizeof(char)*tl->txh->stride*tl->txh->height);
	memset(tl->txh->data, 0, sizeof(char)*tl->txh->stride*tl->txh->height);

	raster->stencil_set_texture(stencil, tl->txh->data, tl->txh->width, tl->txh->height, tl->txh->stride, tl->txh->pixelformat, tl->txh->pixelformat, 1);
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
	span_path = text_create_span_path(tl);
	raster->surface_set_path(surface, span_path);

	raster->surface_fill(surface, brush);
	raster->stencil_delete(brush);
	raster->surface_delete(surface);
	gf_path_del(span_path);

	tl->path = gf_path_new();
	gf_path_add_move_to(tl->path, bounds.x, bounds.y-bounds.height);
	gf_path_add_line_to(tl->path, bounds.x+bounds.width, bounds.y-bounds.height);
	gf_path_add_line_to(tl->path, bounds.x+bounds.width, bounds.y);
	gf_path_add_line_to(tl->path, bounds.x, bounds.y);
	gf_path_close(tl->path);


	gf_sc_texture_set_stencil(tl->txh, stencil);
	gf_sc_texture_set_data(tl->txh);

#ifndef GPAC_DISABLE_3D
	gf_sc_texture_set_blend_mode(tl->txh, TX_BLEND);
	if (for_3d) text_span_build_mesh(tl);
#endif
	return 1;
}


#ifndef GPAC_DISABLE_3D

static void text_3d_fill_span(GF_TraverseState *tr_state, GF_TextLine *tl)
{
	if (!tl->mesh) {
		GF_Path *path = text_create_span_path(tl);
		tl->mesh = new_mesh();
		mesh_from_path(tl->mesh, path);
		gf_path_del(path);
	}

	visual_3d_mesh_paint(tr_state, tl->mesh);
}

static void text_3d_strike_span(GF_TraverseState *tr_state, GF_TextLine *tl, DrawAspect2D *asp, Bool vect_outline)
{
	if (!tl->outline) {
		GF_Path *path = text_create_span_path(tl);
		tl->outline = new_mesh();
#ifndef GPAC_USE_OGL_ES
		if (vect_outline) {
			GF_Path *outline = gf_path_get_outline(path, asp->pen_props);
			TesselatePath(tl->outline, outline, asp->line_texture ? 2 : 1);
			gf_path_del(outline);
		} else {
			mesh_get_outline(tl->outline, path);
		}
#else
		/*VECTORIAL TEXT OUTLINE NOT SUPPORTED ON OGL-ES AT CURRENT TIME*/
		vect_outline = 0;
		mesh_get_outline(tl->outline, path);
#endif
		gf_path_del(path);
	}
	if (vect_outline) {
		visual_3d_mesh_paint(tr_state, tl->outline);
	} else {
		visual_3d_mesh_strike(tr_state, tl->outline, asp->pen_props.width, asp->line_scale, asp->pen_props.dash);
	}
}

static Bool text_is_3d_material(GF_TraverseState *tr_state)
{
	GF_Node *__mat;
	if (!tr_state->appear) return 0;
	__mat = ((M_Appearance *)tr_state->appear)->material;
	if (!__mat) return 0;
	if (gf_node_get_tag(__mat)==TAG_MPEG4_Material2D) return 0;
	return 1;
}

static void text_draw_3d(GF_TraverseState *tr_state, GF_Node *node, TextStack *st)
{
	u32 i;
	DrawAspect2D asp;
	const char *fs_style;
	char *hlight;
	SFColorRGBA hl_color;
	GF_TextLine *tl;
	Bool draw_3d, fill_2d, vect_outline, can_texture_text;
	GF_Compositor *compositor = (GF_Compositor*)st->compositor;
	M_FontStyle *fs = (M_FontStyle *) ((M_Text *) node)->fontStyle;

	vect_outline = !compositor->raster_outlines;

	visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);

	fill_2d = 0;
	draw_3d = text_is_3d_material(tr_state);
	if (draw_3d) {
		if (!visual_3d_setup_appearance(tr_state)) return;
	} else {
		fill_2d = (asp.fill_color) ? 1 : 0;
	}


	fs_style = FSSTYLE;
	hlight = NULL;
	hlight = strstr(fs_style, "HIGHLIGHT");
	if (hlight) hlight = strchr(hlight, '#');
	if (hlight && (fill_2d || draw_3d) ) {
		hlight += 1;
		/*reverse video: highlighting uses the text color, and text color is inverted (except alpha channel)
		the ideal impl would be to use the background color for the text, but since the text may be 
		displayed over anything non uniform this would require clipping the highlight rect with the text
		which is too onerous (and not supported anyway) */
		if (!strnicmp(hlight, "RV", 2)) {
			if (draw_3d) {
				if (tr_state->appear) {
					SFColor c, rc;
					c = ((M_Material *) ((M_Appearance *)  tr_state->appear)->material)->diffuseColor;
					hl_color = gf_sg_sfcolor_to_rgba(c);
					hl_color.alpha = ((M_Material *) ((M_Appearance *)  tr_state->appear)->material)->transparency;
					/*invert diffuse color and resetup*/
					rc.red = FIX_ONE - c.red;
					rc.green = FIX_ONE - c.green;
					rc.blue = FIX_ONE - c.blue;
					((M_Material *) ((M_Appearance *)  tr_state->appear)->material)->diffuseColor = rc;
					visual_3d_setup_appearance(tr_state);
					((M_Material *) ((M_Appearance *)  tr_state->appear)->material)->diffuseColor = c;
				} else {
					hl_color.red = hl_color.green = hl_color.blue = 0;
					hl_color.alpha = FIX_ONE;
				}
			} else {
				hl_color.alpha = FIX_ONE;
				hl_color.red = INT2FIX( GF_COL_R(asp.fill_color) ) / 255;
				hl_color.green = INT2FIX( GF_COL_G(asp.fill_color) ) / 255;
				hl_color.blue = INT2FIX( GF_COL_B(asp.fill_color) ) / 255;
				if (GF_COL_A(asp.fill_color) ) {
					u8 r = GF_COL_R(asp.fill_color);
					u8 g = GF_COL_G(asp.fill_color);
					u8 b = GF_COL_B(asp.fill_color);
					asp.fill_color = GF_COL_ARGB(GF_COL_A(asp.fill_color), 255-r, 255-g, 255-b);
				}
			}
		} else {
			u32 hlc;
			sscanf(hlight, "%x", &hlc);
			if (strlen(hlight)==8)
				hl_color.alpha = INT2FIX(GF_COL_A(hlc)) / 255;
			else
				hl_color.alpha = FIX_ONE;
			hl_color.red = INT2FIX(GF_COL_R(hlc)) / 255;
			hl_color.green = INT2FIX(GF_COL_G(hlc)) / 255;
			hl_color.blue = INT2FIX(GF_COL_B(hlc)) / 255;
		}
		if (!asp.fill_color) hlight = NULL;
	}
	if (strstr(fs_style, "TEXTURED")) st->texture_text_flag = 1;

	/*setup texture*/
	visual_3d_setup_texture(tr_state);
	can_texture_text = 0;
	if (fill_2d || draw_3d) {
		/*check if we can use text texturing*/
		if ((compositor->texture_text_mode != GF_TEXTURE_TEXT_NEVER) || st->texture_text_flag) {
			if (fill_2d && asp.pen_props.width) {
				can_texture_text = 0;
			} else {
				can_texture_text = tr_state->mesh_has_texture ? 0 : 1;
			}
		}
	}

	visual_3d_enable_antialias(tr_state->visual, st->compositor->antiAlias);
	if (fill_2d || draw_3d || tr_state->mesh_has_texture) {

		if (fill_2d) visual_3d_set_material_2d_argb(tr_state->visual, asp.fill_color);

		i=tr_state->text_split_idx ? tr_state->text_split_idx-1 : 0;
		while ((tl = (GF_TextLine *)gf_list_enum(st->text_lines, &i))) {

			if (hlight) {
				visual_3d_fill_rect(tr_state->visual, tl->span->bounds, hl_color);
				if (fill_2d) visual_3d_set_material_2d_argb(tr_state->visual, asp.fill_color);
				else visual_3d_setup_appearance(tr_state);
			}

			if (can_texture_text && text_setup_span_texture(tr_state->visual->compositor, tl, 1)) {
				tr_state->mesh_has_texture = 1;
				gf_sc_texture_enable(tl->txh, NULL);
				visual_3d_mesh_paint(tr_state, tl->mesh);
				gf_sc_texture_disable(tl->txh);
				/*be nice to GL, we remove the text from HW at each frame since there may be a lot of text*/
				gf_sc_texture_reset(tl->txh);
			} else {
				text_3d_fill_span(tr_state, tl);
			}

			if (tr_state->text_split_idx) break;
		}

		/*reset texturing in case of line texture*/
		visual_3d_disable_texture(tr_state);
	}

	visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);

	if (!draw_3d && asp.pen_props.width) {
		if (!asp.line_scale) {
			drawable_compute_line_scale(tr_state, &asp);
		}
		asp.pen_props.width = gf_divfix(asp.pen_props.width, asp.line_scale);
		visual_3d_set_2d_strike(tr_state, &asp);

		if (tr_state->text_split_idx) {
			tl = (GF_TextLine *)gf_list_get(st->text_lines, tr_state->text_split_idx-1);
			text_3d_strike_span(tr_state, tl, &asp, vect_outline);
		} else {
			i=0;
			while ((tl = (GF_TextLine *)gf_list_enum(st->text_lines, &i))) {
				text_3d_strike_span(tr_state, tl, &asp, vect_outline);
			}
		}
	}
}

#endif


void text_draw_tspan_2d(GF_TraverseState *tr_state, GF_TextSpan *span, DrawableContext *ctx)
{
	u32 flags, i;
	Fixed dx, dy, sx, sy, lscale;
	Bool has_texture = ctx->aspect.fill_texture ? 1 : 0;
	GF_Matrix2D mx, tx;

	gf_mx2d_copy(mx, ctx->transform);

	flags = ctx->flags;
	dx = span->off_x;
	dy = span->off_y;
	sx = gf_mulfix(span->font_scale, span->x_scale);
	sy = gf_mulfix(span->font_scale, span->y_scale);

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
		ctx->transform.m[4] = sy;
		ctx->transform.m[1] = ctx->transform.m[3] = 0;
		ctx->transform.m[2] = dx;
		ctx->transform.m[5] = dy;
		gf_mx2d_add_matrix(&ctx->transform, &mx);

		if (has_texture) {
			tx.m[0] = sx;
			tx.m[4] = sy;
			tx.m[1] = tx.m[3] = 0;
			tx.m[2] = dx;
			tx.m[5] = dy;
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

void text_draw_2d(GF_Node *node, GF_TraverseState *tr_state)
{
	u32 i;
	Bool use_texture_text;
	GF_TextLine *line;
	const char *fs_style;
	char *hlight;
	u32 hl_color;
	DrawableContext *ctx = tr_state->ctx;
	TextStack *st = (TextStack *) gf_node_get_private((GF_Node *) ctx->drawable->node);
	M_FontStyle *fs = (M_FontStyle *) ((M_Text *) node)->fontStyle;

	if (!GF_COL_A(ctx->aspect.fill_color) && !ctx->aspect.pen_props.width) return;

	hl_color = 0;
	fs_style = FSSTYLE;
	hlight = strstr(fs_style, "HIGHLIGHT");
	if (hlight) hlight = strchr(hlight, '#');
	if (hlight) {
		hlight += 1;
		/*reverse video: highlighting uses the text color, and text color is inverted (except alpha channel)
		the ideal impl would be to use the background color for the text, but since the text may be 
		displayed over anything non uniform this would require clipping the highlight rect with the text
		which is too onerous (and not supported anyway) */
		if (!strnicmp(hlight, "RV", 2)) {
			u32 a, r, g, b;
			hl_color = ctx->aspect.fill_color;
			
			a = GF_COL_A(ctx->aspect.fill_color);
			if (a) {
				r = GF_COL_R(ctx->aspect.fill_color);
				g = GF_COL_G(ctx->aspect.fill_color);
				b = GF_COL_B(ctx->aspect.fill_color);
				ctx->aspect.fill_color = GF_COL_ARGB(a, 255-r, 255-g, 255-b);
			}
		} else {
			sscanf(hlight, "%x", &hl_color);
			if (strlen(hlight)<8) hl_color |= 0xFF000000;
		}
		if (GF_COL_A(hl_color) == 0) hl_color = 0;
	}
	if (strstr(fs_style, "TEXTURED")) st->texture_text_flag = 1;

	use_texture_text = 0;
	if ((st->compositor->texture_text_mode==GF_TEXTURE_TEXT_ALWAYS) || st->texture_text_flag) {
		use_texture_text = !ctx->aspect.fill_texture && !ctx->aspect.pen_props.width;
	}

	i=ctx->sub_path_index ? ctx->sub_path_index-1 : 0;
	while ((line = (GF_TextLine *)gf_list_enum(st->text_lines, &i))) {
		if (hl_color) visual_2d_fill_rect(tr_state->visual, ctx, &line->span->bounds, hl_color, 0);
		if (use_texture_text && text_setup_span_texture(tr_state->visual->compositor, line, 0)) {
			visual_2d_texture_path_text(tr_state->visual, ctx, line->path, &line->span->bounds, line->txh);
		} else {
			text_draw_tspan_2d(tr_state, line->span, ctx);
		}
		if (ctx->sub_path_index) break;
	}
}



static void text_pick(GF_Node *node, TextStack *st, GF_TraverseState *tr_state)
{
	u32 i, count;
	GF_Matrix inv_mx;
	GF_Matrix2D inv_2d;
	Fixed x, y;
	GF_Compositor *compositor = tr_state->visual->compositor;

	/*TODO: pick the real glyph and not just the bounds of the text span*/
	count = gf_list_count(st->text_lines);

	if (tr_state->visual->type_3d) {
		GF_Ray r;
		SFVec3f local_pt;

		r = tr_state->ray;
		gf_mx_copy(inv_mx, tr_state->model_matrix);
		gf_mx_inverse(&inv_mx);
		gf_mx_apply_ray(&inv_mx, &r);

		if (!compositor_get_2d_plane_intersection(&r, &local_pt)) return;

		x = local_pt.x;
		y = local_pt.y;
	} else {
		gf_mx2d_copy(inv_2d, tr_state->transform);
		gf_mx2d_inverse(&inv_2d);
		x = tr_state->ray.orig.x;
		y = tr_state->ray.orig.y;
		gf_mx2d_apply_coords(&inv_2d, &x, &y);
	}

	for (i=0; i<count; i++) {
		GF_TextLine *tl = (GF_TextLine *)gf_list_get(st->text_lines, i);

		if ((x>=tl->span->bounds.x)
			&& (y<=tl->span->bounds.y) 
			&& (x<=tl->span->bounds.x+tl->span->bounds.width) 
			&& (y>=tl->span->bounds.y-tl->span->bounds.height)) goto picked;

	}
	return;

picked:
	compositor->hit_local_point.x = x;
	compositor->hit_local_point.y = y;
	compositor->hit_local_point.z = 0;

	if (tr_state->visual->type_3d) {
		gf_mx_copy(compositor->hit_world_to_local, tr_state->model_matrix);
		gf_mx_copy(compositor->hit_local_to_world, inv_mx);
	} else {
		gf_mx_from_mx2d(&compositor->hit_world_to_local, &tr_state->transform);
		gf_mx_from_mx2d(&compositor->hit_local_to_world, &inv_2d);
	}

	compositor->hit_node = node;
	compositor->hit_use_dom_events = 0;
	compositor->hit_normal.x = compositor->hit_normal.y = 0; compositor->hit_normal.z = FIX_ONE;
	compositor->hit_texcoords.x = gf_divfix(x, st->bounds.width) + FIX_ONE/2;
	compositor->hit_texcoords.y = gf_divfix(y, st->bounds.height) + FIX_ONE/2;

	if (compositor_is_composite_texture(tr_state->appear)) {
		compositor->hit_appear = tr_state->appear;
	} else {
		compositor->hit_appear = NULL;
	}

	gf_list_reset(tr_state->visual->compositor->sensors);
	count = gf_list_count(tr_state->vrml_sensors);
	for (i=0; i<count; i++) {
		gf_list_add(tr_state->visual->compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}
}


static void text_check_changes(GF_Node *node, TextStack *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node)) {
		text_clean_paths(tr_state->visual->compositor, stack);
		build_text(stack, (M_Text*)node, tr_state);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack->graph, tr_state);
	}
}


static void Text_Traverse(GF_Node *n, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_Text *txt = (M_Text *) n;
	TextStack *st = (TextStack *) gf_node_get_private(n);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		text_clean_paths(gf_sc_get_compositor(n), st);
		drawable_del(st->graph);
		gf_list_del(st->text_lines);
		free(st);
		return;
	}

	if (!st->compositor->font_engine) return;
	if (!txt->string.count) return;

	if (tr_state->text_split_mode) {
		gf_node_dirty_clear(n, 0);
		build_text_split(st, txt, tr_state);
		return;
	}

	text_check_changes(n, st, tr_state);

	switch (tr_state->traversing_mode) {
	case TRAVERSE_DRAW_2D:
		text_draw_2d(n, tr_state);
		return;
#ifndef GPAC_DISABLE_3D
	case TRAVERSE_DRAW_3D:
		text_draw_3d(tr_state, n, st);
		return;
#endif
	case TRAVERSE_PICK:
		text_pick(n, st, tr_state);
		return;
	case TRAVERSE_GET_BOUNDS:
		tr_state->bounds = st->bounds;
		return;
	case TRAVERSE_SORT:
		break;
	default:
		return;
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) return;
#endif

	ctx = drawable_init_context_mpeg4(st->graph, tr_state);
	if (!ctx) return;
	ctx->sub_path_index = tr_state->text_split_idx;

	ctx->flags |= CTX_IS_TEXT;
	if (!GF_COL_A(ctx->aspect.fill_color)) {
		/*override line join*/
		ctx->aspect.pen_props.join = GF_LINE_JOIN_MITER;
		ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	}
	if (ctx->sub_path_index) {
		GF_TextLine *tl = (GF_TextLine *)gf_list_get(st->text_lines, ctx->sub_path_index-1);
		if (tl) drawable_finalize_sort(ctx, tr_state, &tl->span->bounds);
	} else {
		drawable_finalize_sort(ctx, tr_state, &st->bounds);
	}
}


void compositor_init_text(GF_Compositor *compositor, GF_Node *node)
{
	TextStack *stack = (TextStack *)malloc(sizeof(TextStack));
	stack->graph = drawable_new();
	stack->graph->node = node;
	stack->graph->flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->ascent = stack->descent = 0;
	stack->text_lines = gf_list_new();
	stack->texture_text_flag = 0;

	stack->compositor = compositor;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, Text_Traverse);
}


static void TraverseTextureText(GF_Node *node, void *rs, Bool is_destroy)
{
	TextStack *stack;
	GF_Node *text;
	GF_FieldInfo field;
	if (is_destroy) return;
	if (gf_node_get_field(node, 0, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFNODE) return;
	text = *(GF_Node **)field.far_ptr;
	if (!text) return;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return;

	if (gf_node_get_tag(text) != TAG_MPEG4_Text) return;
	stack = (TextStack *) gf_node_get_private(text);
	stack->texture_text_flag = *(SFBool*)field.far_ptr ? 1 : 0;
}


void compositor_init_texture_text(GF_Compositor *compositor, GF_Node *node)
{
	gf_node_set_callback_function(node, TraverseTextureText);
}

#ifndef GPAC_DISABLE_3D
void compositor_extrude_text(GF_Node *node, GF_TraverseState *tr_state, GF_Mesh *mesh, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool txAlongSpine)
{
	u32 i, count;
	Fixed min_cx, min_cy, width_cx, width_cy;
	TextStack *st = (TextStack *) gf_node_get_private(node);

	/*rebuild text node*/
	if (gf_node_dirty_get(node)) {
		ParentNode2D *parent = tr_state->parent;
		tr_state->parent = NULL;
		text_clean_paths(tr_state->visual->compositor, st);
		drawable_reset_path(st->graph);
		gf_node_dirty_clear(node, 0);
		build_text(st, (M_Text *)node, tr_state);
		tr_state->parent = parent;
	}

	min_cx = st->bounds.x;
	min_cy = st->bounds.y - st->bounds.height;
	width_cx = st->bounds.width;
	width_cy = st->bounds.height;

	mesh_reset(mesh);
	count = gf_list_count(st->text_lines);
	for (i=0; i<count; i++) {
		GF_TextLine *tl = (GF_TextLine *)gf_list_get(st->text_lines, i);
		GF_Path *span_path = text_create_span_path(tl);
		mesh_extrude_path_ext(mesh, span_path, thespine, creaseAngle, min_cx, min_cy, width_cx, width_cy, begin_cap, end_cap, spine_ori, spine_scale, txAlongSpine);
		gf_path_del(span_path);
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

#endif



