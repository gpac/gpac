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


void text2D_get_ascent_descent(DrawableContext *ctx, Fixed *a, Fixed *d)
{
	TextStack2D *stack = (TextStack2D *) gf_node_get_private(ctx->node->owner);
	*a = stack->ascent;
	*d = stack->descent;
}

typedef struct
{
	/*regular drawing*/
	GF_Path *path;
	GF_Rect bounds;

	/*texture drawing*/	
	GF_HWTEXTURE hwtx;
	Render2D *sr;
	GF_Path *tx_path;
	Bool tx_ready;
	/*take only into account the window-scaling zoom, not any navigation one (would be too big)*/
	SFVec2f last_scale;
	GF_Rect tx_bounds;
	Bool failed;
} TextLineEntry2D;


TextLineEntry2D *NewTextLine2D(Render2D *sr)
{
	TextLineEntry2D *tl;
	GF_SAFEALLOC(tl, sizeof(TextLineEntry2D));
	tl->path = gf_path_new();
	/*text texturing enabled*/
	tl->sr = sr;
	tl->last_scale.x = sr->scale_x;
	tl->last_scale.y = sr->scale_y;
	return tl;
}

void TextLine_StoreBounds(TextLineEntry2D *tl)
{
	gf_path_get_bounds(tl->path, &tl->bounds);
}

Bool TextLine2D_TextureIsReady(TextLineEntry2D *tl)
{
	GF_Matrix2D mx;
	GF_STENCIL stenc;
	GF_SURFACE surf;
	Fixed cx, cy;
	u32 width, height;
	Fixed scale, max;
	GF_Err e;
	GF_Raster2D *r2d = tl->sr->compositor->r2d;

	/*something failed*/
	if (tl->failed) return 0;
	if (!tl->hwtx) tl->hwtx = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);

	if (tl->tx_ready) {
		if ((tl->last_scale.x == tl->sr->scale_x)
			&& (tl->last_scale.y == tl->sr->scale_y)
			) return 1;

		if (tl->hwtx) r2d->stencil_delete(tl->hwtx);
		if (tl->tx_path) gf_path_del(tl->tx_path);
		tl->tx_path = NULL;
		tl->hwtx = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);

		tl->last_scale.x = tl->sr->scale_x;
		tl->last_scale.y = tl->sr->scale_y;
	}

	max = INT2FIX(512);
	scale = MAX(tl->last_scale.x, tl->last_scale.y);
	if ((gf_mulfix(scale, tl->bounds.width)>max)
	|| (gf_mulfix(scale, tl->bounds.height)>max)) {
		scale = MIN(gf_divfix(max, tl->bounds.width), gf_divfix(max, tl->bounds.height));
	}


	width = FIX2INT(gf_ceil(gf_mulfix(scale, tl->bounds.width)));
	height = FIX2INT(gf_ceil(gf_mulfix(scale, tl->bounds.height)));
	width += 1; 
	height += 1;

	surf = r2d->surface_new(r2d, 1);
	if (!surf) {
		r2d->stencil_delete(tl->hwtx);
		tl->hwtx = NULL;
		tl->failed = 1;
		return 0;
	}
	/*FIXME - make it work with alphagrey...*/
	e = r2d->stencil_create_texture(tl->hwtx, width, height, GF_PIXEL_ARGB);
	if (!e) e = r2d->surface_attach_to_texture(surf, tl->hwtx);
	r2d->surface_clear(surf, NULL, 0);
	stenc = r2d->stencil_new(r2d, GF_STENCIL_SOLID);
	r2d->stencil_set_brush_color(stenc, 0xFF000000);

//	cx = (tl->path->max_x + tl->path->min_x) / 2; 
//	cy = (tl->path->max_y + tl->path->min_y) / 2;
	cx = tl->bounds.x + tl->bounds.width/2; 
	cy = tl->bounds.y - tl->bounds.height/2;

	gf_mx2d_init(mx);
	gf_mx2d_add_translation(&mx, -cx, -cy);
	gf_mx2d_add_scale(&mx, scale, scale);
	gf_mx2d_add_translation(&mx, FIX_ONE/3, FIX_ONE/3);
	r2d->surface_set_matrix(surf, &mx);

	r2d->surface_set_raster_level(surf, GF_RASTER_HIGH_QUALITY);
	r2d->surface_set_path(surf, tl->path);
	r2d->surface_fill(surf, stenc);
	r2d->stencil_delete(stenc);
	r2d->surface_delete(surf);

	tl->tx_path = gf_path_new();
	gf_path_add_move_to(tl->tx_path, tl->bounds.x, tl->bounds.y-tl->bounds.height);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x+tl->bounds.width, tl->bounds.y-tl->bounds.height);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x+tl->bounds.width, tl->bounds.y);
	gf_path_add_line_to(tl->tx_path, tl->bounds.x, tl->bounds.y);
	gf_path_close(tl->tx_path);

	tl->tx_bounds.x = tl->tx_bounds.y = 0;
	tl->tx_bounds.width = INT2FIX(width);
	tl->tx_bounds.height = INT2FIX(height);

	if (e) {
		r2d->stencil_delete(tl->hwtx);
		tl->hwtx = NULL;
		tl->failed = 1;
		return 0;
	}
	tl->tx_ready = 1;
	return 1;
}



void TextStack2D_clean_paths(TextStack2D *stack)
{
	TextLineEntry2D *tl;
	/*delete all path objects*/
	while (gf_list_count(stack->text_lines)) {
		tl = gf_list_get(stack->text_lines, 0);
		gf_list_rem(stack->text_lines, 0);
		if (tl->path) gf_path_del(tl->path);
		if (tl->hwtx) tl->sr->compositor->r2d->stencil_delete(tl->hwtx);
		if (tl->tx_path) gf_path_del(tl->tx_path);
		free(tl);
	}
	gf_rect_reset(&stack->bounds);
	drawable_reset_path(stack->graph);
}

static void DestroyText(GF_Node *node)
{
	TextStack2D *stack = (TextStack2D *) gf_node_get_private(node);
	TextStack2D_clean_paths(stack);
	drawable_del(stack->graph);
	gf_list_del(stack->text_lines);
	free(stack);
}


static void split_text_letters(TextStack2D *st, M_Text *txt, RenderEffect2D *eff)
{
	unsigned short wcTemp[5000];
	unsigned short letter[2];
	DrawableContext *ctx;
	TextLineEntry2D *tl;
	u32 i, j, len;
	Fixed fontSize, start_y, font_height, line_spacing;
	GF_Rect rc;
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!R2D_IsPixelMetrics((GF_Node *)txt)) fontSize = gf_divfix(fontSize, eff->surface->render->cur_width);
    }
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, FSSTYLE) != GF_OK) {
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
	
	gf_rect_reset(&st->bounds);

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
			/*request a context (first one is always valid when entering render)*/
			if (j) group2d_start_child(eff->parent);
			ctx = drawable_init_context(st->graph, eff);
			if (!ctx) return;

			ctx->is_text = 1;

			tl = NewTextLine2D(eff->surface->render);

			gf_list_add(st->text_lines, tl);
			ctx->sub_path_index = gf_list_count(st->text_lines);

			ft_dr->add_text_to_path(ft_dr, tl->path, 1, letter, 0, start_y, FIX_ONE, FIX_ONE, st->ascent, &rc);
			ctx->original.width = rc.width;
			ctx->original.x = rc.x;
			ctx->original.height = MAX(st->ascent + st->descent, rc.height);
			ctx->original.y = start_y;

			TextLine_StoreBounds(tl);

			drawable_finalize_render(ctx, eff);
			group2d_end_child(eff->parent);
		}
	}
}

static void split_text_words(TextStack2D *st, M_Text *txt, RenderEffect2D *eff)
{
	unsigned short wcTemp[5000];
	unsigned short letter[5000];
	DrawableContext *ctx;
	TextLineEntry2D *tl;
	u32 i, j, len, k, first_char;
	Fixed fontSize, font_height, line_spacing;
	GF_Rect rc;
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!R2D_IsPixelMetrics((GF_Node *)txt)) fontSize = gf_divfix(fontSize, eff->surface->render->cur_width);
    }
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, FSSTYLE) != GF_OK) {
			return;
		}
	}
	ft_dr->set_font_size(ft_dr, fontSize);
	ft_dr->get_font_metrics(ft_dr, &st->ascent, &st->descent, &font_height);

	gf_rect_reset(&st->bounds);

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
			/*request a context (first one is always valid when entering render)*/
			if (first_char) group2d_start_child(eff->parent);
			
			ctx = drawable_init_context(st->graph, eff);
			if (!ctx) return;

			ctx->is_text = 1;
			tl = NewTextLine2D(eff->surface->render);

			gf_list_add(st->text_lines, tl);
			ctx->sub_path_index = gf_list_count(st->text_lines);

			/*word splitting only happen in layout, so we don't need top/left anchors*/
			ft_dr->add_text_to_path(ft_dr, tl->path, 1, letter, 0, 0, FIX_ONE, FIX_ONE, st->ascent, &rc);
			gf_path_get_bounds(tl->path, &ctx->original);
			ctx->original.width = rc.width;
			if (ctx->original.x != 0) ctx->original.width -= ctx->original.x;
			ctx->original.x = 0;
			ctx->original.height = MAX(st->ascent + st->descent, ctx->original.height);
			if (ctx->original.y != 0) ctx->original.height -= ctx->original.y;
			ctx->original.y = 0;

			TextLine_StoreBounds(tl);

			drawable_finalize_render(ctx, eff);
			group2d_end_child(eff->parent);

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

static void BuildVerticalTextGraph(TextStack2D *st, M_Text *txt, RenderEffect2D *eff)
{
	TextLine2D *lines;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, space, line_spacing, tot_width, tot_height, max_scale, tmp;
	GF_Rect rc, final;
	Fixed lw, lh, max_lw;
	unsigned short letter[2];
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!R2D_IsPixelMetrics((GF_Node *)txt)) fontSize = gf_divfix(fontSize, eff->surface->render->cur_width);
    }

	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, FSSTYLE) != GF_OK) {
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

		lines[i].wcText = malloc(sizeof(unsigned short) * len);
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

	gf_rect_reset(&final);
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
			TextLineEntry2D *tl = NewTextLine2D(eff->surface->render);
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

		if (! eff->parent) {
			rc = final;
			gf_mx2d_apply_rect(&eff->transform, &rc);
			if (FSLTR && (FIX2INT(rc.x) > eff->surface->top_clipper.x + eff->surface->top_clipper.width) ) {
				break;
			}
			else if (!FSLTR && (FIX2INT(rc.x + rc.width) < eff->surface->top_clipper.x) ) {
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


static void BuildTextGraph(TextStack2D *st, M_Text *txt, RenderEffect2D *eff)
{
	TextLine2D *lines;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, font_height, line_spacing, tot_width, tot_height, max_scale, tmp;
	GF_Rect rc, final;
	GF_FontRaster *ft_dr = eff->surface->render->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	if (!FSHORIZ) {
		BuildVerticalTextGraph(st, txt, eff);
		return;
	}

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!R2D_IsPixelMetrics((GF_Node *)txt)) fontSize = gf_divfix(fontSize, eff->surface->render->cur_width);
    }

	if (ft_dr->set_font(ft_dr, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_dr->set_font(ft_dr, NULL, FSSTYLE) != GF_OK) {
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
		lines[i].wcText = malloc(sizeof(unsigned short) * (len+1));
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
	gf_rect_reset(&final);


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
			TextLineEntry2D *tl = NewTextLine2D(eff->surface->render);

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

		if (! eff->parent) {
			rc = final;
			gf_mx2d_apply_rect(&eff->transform, &rc);
			if (FSTTB && (FIX2INT(rc.y) < eff->surface->top_clipper.y - eff->surface->top_clipper.height) ) {
				break;
			}
			else if (! FSTTB && (FIX2INT(rc.y - rc.height) > eff->surface->top_clipper.y) ) {
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


void Text2D_Draw(DrawableContext *ctx)
{
	u32 i;
	Bool can_texture_text;
	TextLineEntry2D *tl;
	const char *fs_style;
	char *hlight;
	u32 hl_color;
	TextStack2D *st = (TextStack2D *) gf_node_get_private((GF_Node *) ctx->node->owner);
	M_FontStyle *fs = (M_FontStyle *) ((M_Text *) ctx->node->owner)->fontStyle;

	if (!ctx->aspect.filled && !ctx->aspect.pen_props.width) return;

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
		tl = gf_list_get(st->text_lines, ctx->sub_path_index - 1);
		if (!tl || !tl->path) return;
		if (hl_color) VS2D_FillRect(ctx->surface, ctx, tl->bounds, hl_color);

		VS2D_TexturePath(ctx->surface, tl->path, ctx);
		VS2D_DrawPath(ctx->surface, tl->path, ctx, NULL, NULL);
		return;
	}

	can_texture_text = 0;
	if ((st->graph->compositor->texture_text_mode==GF_TEXTURE_TEXT_ALWAYS) || st->texture_text_flag) {
		can_texture_text = !ctx->h_texture && !ctx->aspect.pen_props.width;
	}

	for (i=0; i<gf_list_count(st->text_lines); i++) {
		tl = gf_list_get(st->text_lines, i);
		
		if (hl_color) VS2D_FillRect(ctx->surface, ctx, tl->bounds, hl_color);

		if (can_texture_text && TextLine2D_TextureIsReady(tl)) {
			VS2D_TexturePathText(ctx->surface, ctx, tl->tx_path, &tl->bounds, tl->hwtx, &tl->tx_bounds);
		} else {
			VS2D_TexturePath(ctx->surface, tl->path, ctx);
			VS2D_DrawPath(ctx->surface, tl->path, ctx, NULL, NULL);
		}
		/*reset fill/strike flags since we perform several draw per context*/
		ctx->path_filled = ctx->path_stroke = 0;
	}
}

Bool Text2D_PointOver(DrawableContext *ctx, Fixed x, Fixed y, Bool check_outline)
{
	GF_Matrix2D inv;
	u32 i;
	TextLineEntry2D *tl;
	TextStack2D *st;
	/*this is not documented anywhere but it speeds things up*/
	if (!check_outline) return 1;
	
	
	st = (TextStack2D *) gf_node_get_private((GF_Node *) ctx->node->owner);
	
	gf_mx2d_copy(inv, ctx->transform);
	gf_mx2d_inverse(&inv);
	gf_mx2d_apply_coords(&inv, &x, &y);

	/*otherwise get all paths*/
	if (ctx->sub_path_index > 0) {
		tl = gf_list_get(st->text_lines, ctx->sub_path_index - 1);
		if (!tl || !tl->path) return 0;
		return gf_path_point_over(tl->path, x, y);
	}

	for (i=0; i<gf_list_count(st->text_lines); i++) {
		tl = gf_list_get(st->text_lines, i);
		if (!tl->path) return 0;
		if (gf_path_point_over(tl->path, x, y)) return 1;
	}
	return 0;
}


static void Text_Render(GF_Node *n, void *rs)
{
	DrawableContext *ctx;
	M_Text *txt = (M_Text *) n;
	TextStack2D *st = (TextStack2D *) gf_node_get_private(n);
	RenderEffect2D *eff = (RenderEffect2D *)rs;

	if (!st->graph->compositor->font_engine) return;

	if (!txt->string.count) return;

	if (eff->text_split_mode == 2) {
		split_text_letters(st, txt, eff);
		return;
	}
	else if (eff->text_split_mode == 1) {
		split_text_words(st, txt, eff);
		return;
	}

	/*check for geometry change*/
	if (gf_node_dirty_get(n)) {
		TextStack2D_clean_paths(st);
		BuildTextGraph(st, txt, eff);
		gf_node_dirty_clear(n, 0);
		st->graph->node_changed = 1;
	}

	/*get the text bounds*/
	ctx = drawable_init_context(st->graph, eff);
	if (!ctx) return;

	/*store bounds*/
	ctx->original = st->bounds;

	ctx->is_text = 1;
	if (!ctx->aspect.filled) {
		/*override line join*/
		ctx->aspect.pen_props.join = GF_LINE_JOIN_MITER;
		ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	}
	drawable_finalize_render(ctx, eff);
}


void R2D_InitText(Render2D *sr, GF_Node *node)
{
	TextStack2D *stack = malloc(sizeof(TextStack2D));
	stack->graph = drawable_new();
	/*override all funct*/
	stack->graph->Draw = Text2D_Draw;
	stack->graph->IsPointOver = Text2D_PointOver;
	stack->ascent = stack->descent = 0;
	stack->text_lines = gf_list_new();
	stack->texture_text_flag = 0;
	
	gf_sr_traversable_setup(stack->graph, node, sr->compositor);
	gf_node_set_private(node, stack);
	gf_node_set_render_function(node, Text_Render);
	gf_node_set_predestroy_function(node, DestroyText);
}


static void RenderTextureText(GF_Node *node, void *rs)
{
	TextStack2D *stack;
	GF_Node *text;
	GF_FieldInfo field;
	if (gf_node_get_field(node, 0, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFNODE) return;
	text = *(GF_Node **)field.far_ptr;
	if (!text) return;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return;

	if (gf_node_get_tag(text) != TAG_MPEG4_Text) return;
	stack = (TextStack2D *) gf_node_get_private(text);
	stack->texture_text_flag = *(SFBool*)field.far_ptr ? 1 : 0;
}


void R2D_InitTextureText(Render2D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderTextureText);
}

