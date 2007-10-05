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
	Bool texture_text_flag;
	GF_Compositor *compositor;
} TextStack;

void text2D_get_ascent_descent(DrawableContext *ctx, Fixed *a, Fixed *d)
{
	TextStack *stack = (TextStack *) gf_node_get_private(ctx->drawable->node);
	*a = stack->ascent;
	*d = stack->descent;
}

typedef struct
{
	/*regular drawing*/
	GF_Path *path;

#ifndef GPAC_DISABLE_3D
	GF_Mesh *mesh, *outline_mesh, *tx_mesh;
#endif
	GF_Rect bounds;

	/*texture drawing*/	
	GF_Path *tx_path;
	Bool tx_ready;
	/*take only into account the window-scaling zoom, not any navigation one (would be too big)*/
	SFVec2f last_scale;
	GF_Rect tx_bounds;
	Bool failed;

	GF_TextureHandler txh;
} TextLineEntry;


TextLineEntry *NewTextLine2D(GF_Compositor *compositor)
{
	TextLineEntry *tl;
	GF_SAFEALLOC(tl, TextLineEntry);
	tl->path = gf_path_new();
	/*text texturing enabled*/
	tl->last_scale.x = compositor->scale_x;
	tl->last_scale.y = compositor->scale_y;
	tl->txh.compositor = compositor;
	tl->txh.transparent = 1;
	return tl;
}

void TextLine_StoreBounds(TextLineEntry *tl)
{
	gf_path_get_bounds(tl->path, &tl->bounds);
}

#ifndef GPAC_DISABLE_3D
static void TextLine_BuildMesh(TextLineEntry *tl)
{
	tl->tx_mesh = new_mesh();
	mesh_set_vertex(tl->tx_mesh, tl->bounds.x, tl->bounds.y-tl->bounds.height, 0, 0, 0, FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(tl->tx_mesh, tl->bounds.x+tl->bounds.width, tl->bounds.y-tl->bounds.height, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(tl->tx_mesh, tl->bounds.x+tl->bounds.width, tl->bounds.y, 0, 0, 0, FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(tl->tx_mesh, tl->bounds.x, tl->bounds.y, 0, 0, 0, FIX_ONE, 0, 0);
	mesh_set_triangle(tl->tx_mesh, 0, 1, 2);
	mesh_set_triangle(tl->tx_mesh, 0, 2, 3);
	tl->tx_mesh->flags |= MESH_IS_2D;
	tl->tx_mesh->mesh_type = MESH_TRIANGLES;
	mesh_update_bounds(tl->tx_mesh);
}
#endif

/*don't build too large textures*/
#define MAX_TX_SIZE		512
/*and don't build too small ones otherwise result is as crap as non-textured*/
#define MIN_TX_SIZE		16

static Bool TextLine_TextureIsReady(GF_Compositor *compositor, TextLineEntry *tl, Bool for_3d)
{
	Fixed cx, cy, sx, sy, max, min;
	u32 tw, th;
	GF_Matrix2D mx;
	GF_STENCIL stencil, brush;
	GF_SURFACE surface;
	u32 width, height;
	Fixed scale;
	GF_Raster2D *raster = compositor->rasterizer;

	/*something failed*/
	if (tl->failed) return 0;

	if (tl->tx_ready) {
		if ((tl->last_scale.x == compositor->scale_x)
			&& (tl->last_scale.y == compositor->scale_y)
			) {

#ifndef GPAC_DISABLE_3D
			if (for_3d && !tl->tx_mesh) TextLine_BuildMesh(tl);
#endif
			return 1;
		}

		gf_sc_texture_release(&tl->txh);
		
		if (tl->tx_path) gf_path_del(tl->tx_path);
		tl->tx_path = NULL;
#ifndef GPAC_DISABLE_3D
		if (tl->tx_mesh) mesh_free(tl->tx_mesh);
		tl->tx_mesh = NULL;
#endif
		if (tl->txh.data) free(tl->txh.data);
		tl->txh.data = NULL;

		tl->last_scale.x = compositor->scale_x;
		tl->last_scale.y = compositor->scale_y;
	}

	if (!tl->txh.tx_io) gf_sc_texture_allocate(&tl->txh);
	stencil = gf_sc_texture_get_stencil(&tl->txh);
	if (!stencil) stencil = raster->stencil_new(raster, GF_STENCIL_TEXTURE);

	/*check not too big, but also not really small (meter metrics)*/
	max = INT2FIX(MAX_TX_SIZE);
	min = INT2FIX(MIN_TX_SIZE);
	scale = FIX_ONE;
	if ((gf_mulfix(scale, tl->bounds.width)>max) || (gf_mulfix(scale, tl->bounds.height)>max)) {
		scale = MIN(gf_divfix(max, tl->bounds.width), gf_divfix(max, tl->bounds.height));
	} 
	else if ((gf_mulfix(scale, tl->bounds.width)<min) || (gf_mulfix(scale, tl->bounds.height)<min)) {
		scale = MAX(gf_divfix(min, tl->bounds.width), gf_divfix(min, tl->bounds.height));
	}
	if (scale<FIX_ONE) scale = FIX_ONE;

	/*get closest pow2 sizes*/
	tw = FIX2INT( gf_ceil(gf_mulfix(scale,tl->bounds.width)) );
	width = MIN_TX_SIZE;
	while (width <tw) {
		if (width >=MAX_TX_SIZE) break;
		width *=2;
	}
	th = FIX2INT( gf_ceil(gf_mulfix(scale, tl->bounds.height)) );
	height = MIN_TX_SIZE;
	while (height<th) {
		height*=2;
		if (height>=MAX_TX_SIZE) break;
	}
	/*and get scaling*/
	sx = gf_divfix( INT2FIX(width), tl->bounds.width);
	sy = gf_divfix( INT2FIX(height), tl->bounds.height);

	tl->txh.width = width;
	tl->txh.height = height;
	tl->txh.stride = 4*width;
	tl->txh.pixelformat = GF_PIXEL_RGBA;

	surface = raster->surface_new(raster, 1);
	if (!surface) {
		gf_sc_texture_release(&tl->txh);
		tl->failed = 1;
		return 0;
	}
	tl->txh.data = (char *) malloc(sizeof(char)*tl->txh.stride*tl->txh.height);
	memset(tl->txh.data, 0, sizeof(char)*tl->txh.stride*tl->txh.height);

	/*FIXME - make it work with alphagrey...*/
	raster->stencil_set_texture(stencil, tl->txh.data, tl->txh.width, tl->txh.height, tl->txh.stride, tl->txh.pixelformat, tl->txh.pixelformat, 1);
	raster->surface_attach_to_texture(surface, stencil);

	brush = raster->stencil_new(raster, GF_STENCIL_SOLID);
	raster->stencil_set_brush_color(brush, 0xFF000000);

	cx = tl->bounds.x + tl->bounds.width/2; 
	cy = tl->bounds.y - tl->bounds.height/2;

	gf_mx2d_init(mx);
	gf_mx2d_add_translation(&mx, -cx, -cy);
//	gf_mx2d_add_scale(&mx, scale, scale);
	gf_mx2d_add_scale(&mx, sx, sy);
	gf_mx2d_add_scale(&mx, 98*FIX_ONE/100, 98*FIX_ONE/100);
	raster->surface_set_matrix(surface, &mx);

	raster->surface_set_raster_level(surface, GF_RASTER_HIGH_QUALITY);
	raster->surface_set_path(surface, tl->path);
	raster->surface_fill(surface, brush);
	raster->stencil_delete(brush);
	raster->surface_delete(surface);

	tl->tx_path = gf_path_new();
	gf_path_add_move_to(tl->tx_path, tl->bounds.x, tl->bounds.y-tl->bounds.height);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x+tl->bounds.width, tl->bounds.y-tl->bounds.height);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x+tl->bounds.width, tl->bounds.y);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x, tl->bounds.y);
	gf_path_close(tl->tx_path);

	tl->tx_bounds.x = tl->tx_bounds.y = 0;
	tl->tx_bounds.width = INT2FIX(width);
	tl->tx_bounds.height = INT2FIX(height);

	tl->tx_ready = 1;
	gf_sc_texture_set_stencil(&tl->txh, stencil);
	gf_sc_texture_set_data(&tl->txh);

#ifndef GPAC_DISABLE_3D
	gf_sc_texture_set_blend_mode(&tl->txh, TX_BLEND);
	if (for_3d) TextLine_BuildMesh(tl);
#endif
	return 1;
}



void TextStack2D_clean_paths(GF_Compositor *compositor, TextStack *stack)
{
	TextLineEntry *tl;
	/*delete all path objects*/
	while (gf_list_count(stack->text_lines)) {
		tl = (TextLineEntry *) gf_list_get(stack->text_lines, 0);
		gf_list_rem(stack->text_lines, 0);
		if (tl->path) gf_path_del(tl->path);
		if (tl->txh.data) free(tl->txh.data);
		tl->txh.data = NULL;
		gf_sc_texture_release(&tl->txh);
		if (tl->tx_path) gf_path_del(tl->tx_path);
#ifndef GPAC_DISABLE_3D
		if (tl->mesh) mesh_free(tl->mesh);
		if (tl->outline_mesh) mesh_free(tl->outline_mesh);
		if (tl->tx_mesh) mesh_free(tl->tx_mesh);
#endif
		
		free(tl);
	}
	stack->bounds.width = stack->bounds.height = 0;
	drawable_reset_path(stack->graph);
}


static void split_text_letters(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	unsigned short wcTemp[5000];
	unsigned short letter[2];
	TextLineEntry *tl;
	u32 i, j, len;
	Fixed fontSize, start_y, font_height, line_spacing;
	GF_Rect rc, bounds;
	GF_FontRaster *ft_dr = tr_state->visual->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_dr->set_font_size(ft_dr, fontSize);
	ft_dr->get_font_metrics(ft_dr, &st->ascent, &st->descent, &font_height);
			
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
	
	st->bounds.width = st->bounds.height = 0;

	for (i=0; i < txt->string.count; i++) {

		char *str = txt->string.vals[i];
		if (!str) continue;
		len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
		if (len == (size_t) (-1)) continue;

		letter[1] = (unsigned short) 0;
		for (j=0; j<len; j++) {
			if (FSLTR) {
				letter[0] = wcTemp[j];
			} else {
				letter[0] = wcTemp[len - j - 1];
			}
			/*request a context (first one is always valid when entering sort phase)*/
			if (j) parent_node_start_group(tr_state->parent, NULL);

			tl = NewTextLine2D(tr_state->visual->compositor);

			gf_list_add(st->text_lines, tl);

			ft_dr->add_text_to_path(ft_dr, tl->path, 1, letter, 0, start_y, FIX_ONE, FIX_ONE, st->ascent, &rc);

			bounds.width = rc.width;
			bounds.x = rc.x;
			bounds.height = MAX(st->ascent + st->descent, rc.height);
			bounds.y = start_y;

			TextLine_StoreBounds(tl);

			parent_node_end_text_group(tr_state->parent, &bounds, st->ascent, st->descent, gf_list_count(st->text_lines) );
		}
	}
}

static void split_text_words(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	unsigned short wcTemp[5000];
	unsigned short letter[5000];
	TextLineEntry *tl;
	u32 i, j, len, k, first_char;
	Fixed fontSize, font_height, line_spacing;
	GF_Rect rc, bounds;
	GF_FontRaster *ft_dr = tr_state->visual->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_dr->set_font_size(ft_dr, fontSize);
	ft_dr->get_font_metrics(ft_dr, &st->ascent, &st->descent, &font_height);

	st->bounds.width = st->bounds.height = 0;

	for (i=0; i < txt->string.count; i++) {
		char *str = txt->string.vals[i];
		if (!str) continue;
		len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
		if (len == (size_t) (-1)) continue;

		first_char = 0;
		for (j=0; j<len; j++) {
			/*we currently only split sentences at spaces*/
			if ((j+1!=len) && (wcTemp[j] != (unsigned short) ' ')) continue;

			if (FSLTR) {
				for (k=0; k<=j - first_char; k++) letter[k] = wcTemp[first_char+k];
			} else {
				for (k=0; k<=j - first_char; k++) letter[k] = wcTemp[len - first_char - k - 1];
			}
			letter[k] = (unsigned short) 0;
			/*request a context (first one is always valid when sort rendering)*/
			if (first_char) parent_node_start_group(tr_state->parent, NULL);
			
			tl = NewTextLine2D(tr_state->visual->compositor);

			gf_list_add(st->text_lines, tl);

			/*word splitting only happen in layout, so we don't need top/left anchors*/
			ft_dr->add_text_to_path(ft_dr, tl->path, 1, letter, 0, 0, FIX_ONE, FIX_ONE, st->ascent, &rc);

			bounds.width = rc.width;
			bounds.height = st->ascent + st->descent;
			bounds.x = bounds.y = 0;

			tl->bounds = bounds;

			parent_node_end_text_group(tr_state->parent, &bounds, st->ascent, st->descent, gf_list_count(st->text_lines) );

			first_char = j+1;
		}
	}
}

/*for vert and horiz text*/
typedef struct
{
	unsigned short *wcText;
	u32 length;
	Fixed width, height;
	Fixed x_scaling, y_scaling;
} TextLine2D;

static void BuildVerticalTextGraph(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	TextLine2D *lines;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, space, line_spacing, tot_width, tot_height, max_scale, tmp;
	GF_Rect rc, final;
	Fixed lw, lh, max_lw;
	unsigned short letter[2];
	GF_FontRaster *ft_dr = tr_state->visual->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }

	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_dr->set_font_size(ft_dr, fontSize);
	ft_dr->get_font_metrics(ft_dr, &st->ascent, &st->descent, &space);


	/*compute overall bounding box size*/
	tot_width = 0;
	tot_height = 0;
	lines = (TextLine2D *) malloc(sizeof(TextLine2D)*txt->string.count);
	memset(lines, 0, sizeof(TextLine2D)*txt->string.count);
		
	letter[1] = (unsigned short) '\0';

/*	space = st->ascent + st->descent;
	space = fontSize - st->ascent + st->descent;
*/

	for (i=0; i < txt->string.count; i++) {
		char *str = txt->string.vals[i];
		if (!str) continue;
		lines[i].length = 0;
		len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
		if (len == (size_t) (-1)) continue;

		lines[i].wcText = (u16*)malloc(sizeof(unsigned short) * len);
		memcpy(lines[i].wcText, wcTemp, sizeof(unsigned short) * len);
		lines[i].length = len;
		
		lines[i].y_scaling = lines[i].x_scaling = FIX_ONE;
		lines[i].height = len * space;
		if (!lines[i].height) continue;

		if ((txt->length.count>i) && (txt->length.vals[i]>0) ) 
			lines[i].y_scaling = gf_divfix(txt->length.vals[i], lines[i].height);
	
		tmp = gf_mulfix(lines[i].height, lines[i].y_scaling);
		if (tot_height < tmp) tot_height = tmp;
	}

	tot_width = txt->string.count * line_spacing;
	st->bounds.width = tot_width;

	max_scale = FIX_ONE;
	if ((txt->maxExtent>0) && (tot_height>txt->maxExtent)) {
		max_scale = gf_divfix(txt->maxExtent, tot_height);
		tot_height = txt->maxExtent;
	}

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

	if (!strcmp(FSMAJOR, "MIDDLE")) {
		int_major = 0;
	} else if (!strcmp(FSMAJOR, "END")) {
		int_major = 1;
	} else {
		int_major = 2;
	}

	final.width = final.height = 0;

	for (i=0; i < txt->string.count; i++) {
		switch (int_major) {
		case 0:
			if (FSTTB) 
				start_y = lines[i].height/2;
			else
				start_y = -lines[i].height/2 + space;
			break;
		case 1:
			if (FSTTB)
				start_y = lines[i].height;
			else
				start_y = -lines[i].height + space;
			break;
		default:
			if (FSTTB)
				start_y = 0;
			else
				start_y = space;
			break;
		}

		if (lines[i].length) {
			TextLineEntry *tl = NewTextLine2D(tr_state->visual->compositor);
			gf_list_add(st->text_lines, tl);

			/*adjust horizontal offset on first column*/
			if (!i) {
				max_lw = 0;
				for (k=0; k<lines[i].length; k++) {
					letter[0] = lines[i].wcText[k];
					/*get glyph width so that all letters are centered on the same vertical line*/
					ft_dr->get_text_size(ft_dr, letter, &lw, &lh);
					if (max_lw < lw) max_lw = lw;
				}
				st->bounds.width += max_lw/2;
				start_x += max_lw/2;
			}
			
			for (k=0; k<lines[i].length; k++) {
				letter[0] = lines[i].wcText[k];
				/*get glyph width so that all letters are centered on the same vertical line*/
				ft_dr->get_text_size(ft_dr, letter, &lw, &lh);
				ft_dr->add_text_to_path(ft_dr, tl->path,  1, letter, start_x - lw/2, start_y, lines[i].x_scaling, gf_mulfix(lines[i].y_scaling,max_scale), st->ascent, &rc);

				if (FSTTB)
					start_y -= space;
				else
					start_y += space;
			}
			gf_path_get_bounds(tl->path, &rc);
			gf_rect_union(&final, &rc);
			TextLine_StoreBounds(tl);
		}

		if (FSLTR) {
			start_x += line_spacing;
		} else {
			start_x -= line_spacing;
		}

		if (! tr_state->parent) {
			rc = final;
			gf_mx2d_apply_rect(&tr_state->transform, &rc);
			if (FSLTR && (FIX2INT(rc.x) > tr_state->visual->top_clipper.x + tr_state->visual->top_clipper.width) ) {
				break;
			}
			else if (!FSLTR && (FIX2INT(rc.x + rc.width) < tr_state->visual->top_clipper.x) ) {
				break;
			}
		}

		/*free unicode buffer*/
		free(lines[i].wcText);
	}

	/*free remaining unicode buffers*/
	for (; i < txt->string.count; i++) free(lines[i].wcText);

	free(lines);

	st->bounds.height = final.height;
	st->bounds.y = final.y;
}


static void BuildTextGraph(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	TextLine2D *lines;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, font_height, line_spacing, tot_width, tot_height, max_scale, tmp;
	GF_Rect rc, final;
	GF_FontRaster *ft_dr = tr_state->visual->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	if (!FSHORIZ) {
		BuildVerticalTextGraph(st, txt, tr_state);
		return;
	}

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
    }

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_dr->set_font_size(ft_dr, fontSize);
	ft_dr->get_font_metrics(ft_dr, &st->ascent, &st->descent, &font_height);

	/*spacing= FSSPACING * (font_height) and fontSize not adjusted */
	line_spacing = gf_mulfix(FSSPACE, fontSize);
	
	tot_width = 0;
	lines = (TextLine2D *) malloc(sizeof(TextLine2D)*txt->string.count);
	memset(lines, 0, sizeof(TextLine2D)*txt->string.count);
	
	for (i=0; i < txt->string.count; i++) {
		char *str = txt->string.vals[i];
		if (!str) continue;
		lines[i].length = 0;
		len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
		if (len == (size_t) (-1)) continue;

		lines[i].length = len;
		lines[i].wcText = (u16*)malloc(sizeof(unsigned short) * (len+1));
		if (!FSLTR) {
			for (k=0; k<len; k++) lines[i].wcText[k] = wcTemp[len-k-1];
		} else {
			memcpy(lines[i].wcText, wcTemp, sizeof(unsigned short) * len);
		}
		lines[i].wcText[len] = (unsigned short) '\0';

		lines[i].y_scaling = lines[i].x_scaling = FIX_ONE;
		ft_dr->get_text_size(ft_dr, lines[i].wcText, &lines[i].width, &lines[i].height);

		if (!lines[i].width) continue;
		if ((txt->length.count>i) && (txt->length.vals[i]>0)) {
			lines[i].x_scaling = gf_divfix(txt->length.vals[i], lines[i].width);
		}
		tmp = gf_mulfix(lines[i].width, lines[i].x_scaling);
		if (tot_width < tmp) tot_width = tmp;
	}
	
	max_scale = FIX_ONE;
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
	
	/*major-justification*/
	if (!strcmp(FSMAJOR, "MIDDLE") ) {
		int_major = 0;
	} else if (!strcmp(FSMAJOR, "END") ) {
		int_major = 1;
	} else {
		int_major = 2;
	}

	final.width = final.height = 0;

	for (i=0; i < txt->string.count; i++) {
		switch (int_major) {
		/*major-justification MIDDLE*/
		case 0:
			start_x = -lines[i].width/2;
			break;
		/*major-justification END*/
		case 1:
			start_x = (FSLTR) ? -lines[i].width : 0;
			break;
		/*BEGIN, FIRST or default*/
		default:
			start_x = (FSLTR) ? 0 : -lines[i].width;
			break;
		}

		if (lines[i].length) {
			TextLineEntry *tl = NewTextLine2D(tr_state->visual->compositor);

			/*if using the font engine the font is already configured*/
			ft_dr->add_text_to_path(ft_dr, tl->path, 1, lines[i].wcText, start_x, start_y, gf_mulfix(lines[i].x_scaling,max_scale), lines[i].y_scaling, st->ascent, &rc);

			gf_list_add(st->text_lines, tl);
			TextLine_StoreBounds(tl);
			gf_rect_union(&final, &rc);
		}
		if (FSTTB) {
			start_y -= line_spacing;
		} else {
			start_y += line_spacing;
		}

		if (! tr_state->parent) {
			rc = final;
			gf_mx2d_apply_rect(&tr_state->transform, &rc);
			if (FSTTB && (FIX2INT(rc.y) < tr_state->visual->top_clipper.y - tr_state->visual->top_clipper.height) ) {
				break;
			}
			else if (! FSTTB && (FIX2INT(rc.y - rc.height) > tr_state->visual->top_clipper.y) ) {
				break;
			}
		}

		/*free unicode buffer*/
		free(lines[i].wcText);
	}
	/*free remaining unicode buffers*/
	for (; i < txt->string.count; i++) free(lines[i].wcText);

	free(lines);

	st->bounds.width = final.width;
	st->bounds.x = final.x;
}

#ifndef GPAC_DISABLE_3D
static void Text_FillTextLine(GF_TraverseState *tr_state, TextLineEntry *tl)
{
	if (!tl->mesh) {
		tl->mesh = new_mesh();
		mesh_from_path(tl->mesh, tl->path);
	}
	visual_3d_mesh_paint(tr_state, tl->mesh);
}

static void Text_StrikeTextLine(GF_TraverseState *tr_state, TextLineEntry *tl, DrawAspect2D *asp, Bool vect_outline)
{
	if (!tl->outline_mesh) {
		tl->outline_mesh = new_mesh();
#ifndef GPAC_USE_OGL_ES
		if (vect_outline) {
			GF_Path *outline = gf_path_get_outline(tl->path, asp->pen_props);
			TesselatePath(tl->outline_mesh, outline, asp->line_texture ? 2 : 1);
			gf_path_del(outline);
		} else {
			mesh_get_outline(tl->outline_mesh, tl->path);
		}
#else
		/*VECTORIAL TEXT OUTLINE NOT SUPPORTED ON OGL-ES AT CURRENT TIME*/
		vect_outline = 0;
		mesh_get_outline(tl->outline_mesh, tl->path);
#endif

	}
	if (vect_outline) {
		visual_3d_mesh_paint(tr_state, tl->outline_mesh);
	} else {
		visual_3d_mesh_strike(tr_state, tl->outline_mesh, asp->pen_props.width, asp->line_scale, asp->pen_props.dash);
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
	TextLineEntry *tl;
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
			hl_color.alpha = INT2FIX(GF_COL_A(hlc)) / 255;
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
		if (compositor->texture_text_mode || st->texture_text_flag) {
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

		if (tr_state->text_split_idx) {
			tl = (TextLineEntry *)gf_list_get(st->text_lines, tr_state->text_split_idx-1);
			assert(tl);
			if (hlight) {
				visual_3d_fill_rect(tr_state->visual, tl->bounds, hl_color);
				if (fill_2d) visual_3d_set_material_2d_argb(tr_state->visual, asp.fill_color);
				else visual_3d_setup_appearance(tr_state);
			}

			if (can_texture_text && TextLine_TextureIsReady(tr_state->visual->compositor, tl, 1)) {
				tr_state->mesh_has_texture = 1;
				gf_sc_texture_enable(&tl->txh, NULL);
				visual_3d_mesh_paint(tr_state, tl->tx_mesh);
				gf_sc_texture_disable(&tl->txh);
				/*be nice to GL, we remove the text from HW at each frame since there may be a lot of text*/
				gf_sc_texture_reset(&tl->txh);
			} else {
				Text_FillTextLine(tr_state, tl);
			}
		} else {
			i=0; 
			while ((tl = (TextLineEntry *)gf_list_enum(st->text_lines, &i))) {

				if (hlight) {
					visual_3d_fill_rect(tr_state->visual, tl->bounds, hl_color);
					if (fill_2d) visual_3d_set_material_2d_argb(tr_state->visual, asp.fill_color);
					else visual_3d_setup_appearance(tr_state);
				}

				if (can_texture_text && TextLine_TextureIsReady(tr_state->visual->compositor, tl, 1)) {
					tr_state->mesh_has_texture = 1;
					gf_sc_texture_enable(&tl->txh, NULL);
					visual_3d_mesh_paint(tr_state, tl->tx_mesh);
					gf_sc_texture_disable(&tl->txh);
					/*be nice to GL, we remove the text from HW at each frame since there may be a lot of text*/
					gf_sc_texture_reset(&tl->txh);
				} else {
					Text_FillTextLine(tr_state, tl);
				}
			}
		}
		/*reset texturing in case of line texture*/
		visual_3d_disable_texture(tr_state);
	}

	visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);

	if (!draw_3d && asp.pen_props.width) {
		Fixed w = asp.pen_props.width;
		asp.pen_props.width = gf_divfix(asp.pen_props.width, asp.line_scale);
		visual_3d_set_2d_strike(tr_state, &asp);

		if (tr_state->text_split_idx) {
			tl = (TextLineEntry *)gf_list_get(st->text_lines, tr_state->text_split_idx-1);
			Text_StrikeTextLine(tr_state, tl, &asp, vect_outline);
		} else {
			i=0;
			while ((tl = (TextLineEntry *)gf_list_enum(st->text_lines, &i))) {
				Text_StrikeTextLine(tr_state, tl, &asp, vect_outline);
			}
		}
		asp.pen_props.width = w;
	}
}

#endif


void text_draw_2d(GF_Node *node, GF_TraverseState *tr_state)
{
	u32 i;
	Bool can_texture_text;
	TextLineEntry *tl;
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
		}
		if (GF_COL_A(hl_color) == 0) hl_color = 0;
	}
	if (strstr(fs_style, "TEXTURED")) st->texture_text_flag = 1;

	/*text has been splited*/
	if (ctx->sub_path_index > 0) {
		tl = (TextLineEntry*)gf_list_get(st->text_lines, ctx->sub_path_index - 1);
		if (!tl || !tl->path) return;
		if (hl_color) visual_2d_fill_rect(tr_state->visual, ctx, &tl->bounds, hl_color, 0);

		visual_2d_texture_path(tr_state->visual, tl->path, ctx);
		visual_2d_draw_path(tr_state->visual, tl->path, ctx, NULL, NULL);
		return;
	}

	can_texture_text = 0;
	if ((st->compositor->texture_text_mode==GF_TEXTURE_TEXT_ALWAYS) || st->texture_text_flag) {
		can_texture_text = !ctx->aspect.fill_texture && !ctx->aspect.pen_props.width;
	}

	i=0;
	while ((tl = (TextLineEntry*)gf_list_enum(st->text_lines, &i))) {
		if (hl_color) visual_2d_fill_rect(tr_state->visual, ctx, &tl->bounds, hl_color, 0);

		if (can_texture_text && TextLine_TextureIsReady(tr_state->visual->compositor, tl, 0)) {
			visual_2d_texture_path_text(tr_state->visual, ctx, tl->tx_path, &tl->bounds, &tl->txh, &tl->tx_bounds);
		} else {
			visual_2d_texture_path(tr_state->visual, tl->path, ctx);
			visual_2d_draw_path(tr_state->visual, tl->path, ctx, NULL, NULL);
		}
		/*reset fill/strike flags since we perform several draw per context*/
		ctx->flags &= ~CTX_PATH_FILLED;
		ctx->flags &= ~CTX_PATH_STROKE;
	}
}

static void text_pick(GF_Node *node, TextStack *st, GF_TraverseState *tr_state)
{
	u32 i, count;
	DrawAspect2D asp;
	GF_Matrix2D inv_2d;
	Fixed x, y;
	GF_Compositor *compositor = tr_state->visual->compositor;

	count = gf_list_count(st->text_lines);
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		for (i=0; i<count; i++) {
			TextLineEntry*tl = (TextLineEntry*)gf_list_get(st->text_lines, i);
			if (!tl->path) continue;

			if (tr_state->visual->type_3d) {
				visual_3d_drawable_pick(node, tr_state, NULL, tl->path);
				if (compositor->hit_node==node) return;
			}
		}
		return;
	}
#endif

	memset(&asp, 0, sizeof(DrawAspect2D));
	drawable_get_aspect_2d_mpeg4(node, &asp, tr_state);
	gf_mx2d_copy(inv_2d, tr_state->transform);
	gf_mx2d_inverse(&inv_2d);
	x = tr_state->ray.orig.x;
	y = tr_state->ray.orig.y;
	gf_mx2d_apply_coords(&inv_2d, &x, &y);

	for (i=0; i<count; i++) {
		TextLineEntry*tl = (TextLineEntry*)gf_list_get(st->text_lines, i);
		if (!tl->path) continue;

		if (gf_path_point_over(tl->path, x, y)) goto picked;
	}
	return;
picked:
	compositor->hit_local_point.x = x;
	compositor->hit_local_point.y = y;
	compositor->hit_local_point.z = 0;

	gf_mx_from_mx2d(&compositor->hit_world_to_local, &tr_state->transform);
	gf_mx_from_mx2d(&compositor->hit_local_to_world, &inv_2d);

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
		TextStack2D_clean_paths(tr_state->visual->compositor, stack);
		BuildTextGraph(stack, (M_Text*)node, tr_state);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(stack->graph, tr_state);
	}
}

static void TraverseText(GF_Node *n, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_Text *txt = (M_Text *) n;
	TextStack *st = (TextStack *) gf_node_get_private(n);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		TextStack2D_clean_paths(gf_sc_get_compositor(n), st);
		drawable_del(st->graph);
		gf_list_del(st->text_lines);
		free(st);
		return;
	}

	if (!st->compositor->font_engine) return;
	if (!txt->string.count) return;

	if (tr_state->text_split_mode == 2) {
		gf_node_dirty_clear(n, 0);
		split_text_letters(st, txt, tr_state);
		return;
	}
	else if (tr_state->text_split_mode == 1) {
		gf_node_dirty_clear(n, 0);
		split_text_words(st, txt, tr_state);
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
	if (tr_state->text_split_idx) {
		TextLineEntry *tl = (TextLineEntry*)gf_list_get(st->text_lines, ctx->sub_path_index - 1);
		drawable_finalize_sort(ctx, tr_state, &tl->bounds);
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
	gf_node_set_callback_function(node, TraverseText);
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
		TextStack2D_clean_paths(tr_state->visual->compositor, st);
		drawable_reset_path(st->graph);
		gf_node_dirty_clear(node, 0);
		BuildTextGraph(st, (M_Text *)node, tr_state);
		tr_state->parent = parent;
	}

	min_cx = st->bounds.x;
	min_cy = st->bounds.y - st->bounds.height;
	width_cx = st->bounds.width;
	width_cy = st->bounds.height;

	mesh_reset(mesh);
	count = gf_list_count(st->text_lines);
	for (i=0; i<count; i++) {
		TextLineEntry *tl = (TextLineEntry *)gf_list_get(st->text_lines, i);
		mesh_extrude_path_ext(mesh, tl->path, thespine, creaseAngle, min_cx, min_cy, width_cx, width_cy, begin_cap, end_cap, spine_ori, spine_scale, txAlongSpine);
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

#endif

