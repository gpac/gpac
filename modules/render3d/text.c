/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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



#include "render3d_nodes.h"
#include "grouping.h"
#include <gpac/utf.h>


typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_Mesh *mesh;
	Bool (*IntersectWithRay)(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint);
	Bool (*ClosestFace)(GF_Node *owner, SFVec3f user_pos, Fixed min_dist, SFVec3f *outPoint);
	GF_Path *path;
	GF_List *strike_list;
	Fixed ascent, descent;
	GF_List *text_lines;
	GF_Rect bounds;
	Bool texture_text_flag;
} TextStack;

/*default value when no fontStyle*/
#define FSFAMILY	(fs && fs->family.count) ? (const char *)fs->family.vals[0]	: ""

/*here it's tricky since it depends on our metric system...*/
#define FSSIZE		(fs ? fs->size : FIX_ONE)
#define FSSTYLE		(fs && fs->style.buffer) ? (const char *)fs->style.buffer : ""
#define FSMAJOR		( (fs && fs->justify.count && fs->justify.vals[0]) ? (const char *)fs->justify.vals[0] : "FIRST")
#define FSMINOR		( (fs && (fs->justify.count>1) && fs->justify.vals[1]) ? (const char *)fs->justify.vals[1] : "FIRST")
#define FSHORIZ		(fs ? fs->horizontal : 1)
#define FSLTR		(fs ? fs->leftToRight : 1)
#define FSTTB		(fs ? fs->topToBottom : 1)
#define FSLANG		(fs ? fs->language : "")
#define FSSPACE		(fs ? fs->spacing : FIX_ONE)

typedef struct
{
	GF_Path *path;
	GF_Path *outline;
	GF_Mesh *mesh;
	GF_Mesh *outline_mesh;
	GF_Rect bounds;

	/*for text texturing*/
	GF_TextureHandler txh;
	Render3D *sr;
	/*RGBA data*/
	char *tx_data;
	/*a simple rectangle*/
	GF_Mesh *tx_mesh;
	Bool tx_ready;
	u32 tx_width, tx_height;
	Bool failed;
	GF_Rect tx_bounds;
} CachedTextLine;

static CachedTextLine *new_text_line(Render3D *sr, GF_List *lines_register)
{
	CachedTextLine *tl = malloc(sizeof(CachedTextLine));
	memset(tl, 0, sizeof(CachedTextLine));
	tl->path = gf_path_new();

	/*text texturing enabled*/
	tl->sr = sr;
	memset(&tl->txh, 0, sizeof(GF_TextureHandler));
	tl->txh.compositor = sr->compositor;
	tl->txh.transparent = 1;

	gf_list_add(lines_register, tl);
	return tl;
}

static void delete_text_line(CachedTextLine *tl)
{
	gf_path_del(tl->path);
	if (tl->outline) gf_path_del(tl->outline);
	if (tl->mesh) { mesh_free(tl->mesh); tl->mesh = NULL; }
	if (tl->outline_mesh) { mesh_free(tl->outline_mesh); tl->outline_mesh = NULL; }

	tx_delete(&tl->txh);
	if (tl->tx_data) free(tl->tx_data);
	if (tl->tx_mesh) mesh_free(tl->tx_mesh);
	free(tl);
}

/*don't build too large textures*/
#define MAX_TX_SIZE		512
/*and don't build too small ones otherwise result is as crap as non-textured*/
#define MIN_TX_SIZE		16

Bool TextLine_TextureIsReady(CachedTextLine *tl)
{
	GF_Matrix2D mx;
	GF_STENCIL stenc;
	GF_SURFACE surf;
	Fixed cx, cy, sx, sy, max, min;
	u32 tw, th;
	GF_Err e;
	Fixed scale;
	GF_STENCIL texture2D;
	GF_Raster2D *r2d = tl->sr->compositor->r2d;

	if (tl->failed) return 0;

	if (tl->tx_ready) {
		if (!tl->sr->compositor->reset_graphics) goto exit;
		tx_delete(&tl->txh);
		if (tl->tx_mesh) mesh_free(tl->tx_mesh);
		tl->tx_mesh = NULL;
		if (tl->tx_data) free(tl->tx_data);
		tl->tx_data = NULL;
		tl->failed = 0;
		tl->tx_ready = 0;
	}

	gf_path_get_bounds(tl->path, &tl->tx_bounds);


	/*check not too big, but also not really small (meter metrics)*/
	max = INT2FIX(MAX_TX_SIZE);
	min = INT2FIX(MIN_TX_SIZE);
	scale = FIX_ONE;
	if ((gf_mulfix(scale, tl->tx_bounds.width)>max) || (gf_mulfix(scale, tl->tx_bounds.height)>max)) {
		scale = MIN(gf_divfix(max, tl->tx_bounds.width), gf_divfix(max, tl->tx_bounds.height));
	} 
	else if ((gf_mulfix(scale, tl->tx_bounds.width)<min) || (gf_mulfix(scale, tl->tx_bounds.height)<min)) {
		scale = MAX(gf_divfix(min, tl->tx_bounds.width), gf_divfix(min, tl->tx_bounds.height));
	}

	/*get closest pow2 sizes*/
	tw = FIX2INT( gf_ceil(gf_mulfix(scale,tl->tx_bounds.width)) );
	tl->tx_width = MIN_TX_SIZE;
	while (tl->tx_width<tw) {
		if (tl->tx_width>=MAX_TX_SIZE) break;
		tl->tx_width*=2;
	}
	th = FIX2INT( gf_ceil(gf_mulfix(scale, tl->tx_bounds.height)) );
	tl->tx_height = MIN_TX_SIZE;
	while (tl->tx_height<th) {
		tl->tx_height*=2;
		if (tl->tx_height>=MAX_TX_SIZE) break;
	}
	/*and get scaling*/
	sx = gf_divfix( INT2FIX(tl->tx_width), tl->tx_bounds.width);
	sy = gf_divfix( INT2FIX(tl->tx_height), tl->tx_bounds.height);

	texture2D = r2d->stencil_new(r2d, GF_STENCIL_TEXTURE);
	if (!texture2D) {
		tl->failed = 1;
		return 0;
	}

	surf = r2d->surface_new(r2d, 1);
	if (!surf) {
		r2d->stencil_delete(texture2D);
		return 0;
	}

	tl->tx_data = (char *) malloc(sizeof(char)*tl->tx_width*tl->tx_height*4);
	/*FIXME - make it work with alphagrey...*/
	e = r2d->stencil_set_texture(texture2D, tl->tx_data, tl->tx_width, tl->tx_height, 4*tl->tx_width, GF_PIXEL_ARGB, GF_PIXEL_ARGB, 1);
	if (!e) e = r2d->surface_attach_to_texture(surf, texture2D);
	stenc = r2d->stencil_new(r2d, GF_STENCIL_SOLID);
	r2d->stencil_set_brush_color(stenc, 0xFF000000);
	cx = tl->tx_bounds.x + tl->tx_bounds.width/2;
	cy = tl->tx_bounds.y - tl->tx_bounds.height/2;

	gf_mx2d_init(mx);
	gf_mx2d_add_translation(&mx, -cx, -cy);
//	gf_mx2d_add_scale(&mx, scale, -scale);
	gf_mx2d_add_scale(&mx, sx, -sy);
	r2d->surface_set_matrix(surf, &mx);

	r2d->surface_set_raster_level(surf, GF_RASTER_HIGH_QUALITY);
	r2d->surface_set_path(surf, tl->path);
	r2d->surface_fill(surf, stenc);
	r2d->stencil_delete(stenc);
	r2d->surface_delete(surf);
	r2d->stencil_delete(texture2D);

	tl->tx_mesh = new_mesh();
	mesh_set_vertex(tl->tx_mesh, tl->tx_bounds.x, tl->tx_bounds.y-tl->tx_bounds.height, 0, 0, 0, FIX_ONE, 0, 0);
	mesh_set_vertex(tl->tx_mesh, tl->tx_bounds.x+tl->tx_bounds.width, tl->tx_bounds.y-tl->tx_bounds.height, 0, 0, 0, FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(tl->tx_mesh, tl->tx_bounds.x+tl->tx_bounds.width, tl->tx_bounds.y, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(tl->tx_mesh, tl->tx_bounds.x, tl->tx_bounds.y, 0, 0, 0, FIX_ONE, 0, FIX_ONE);
	mesh_set_triangle(tl->tx_mesh, 0, 1, 2);
	mesh_set_triangle(tl->tx_mesh, 0, 2, 3);
	tl->tx_mesh->flags |= MESH_IS_2D;
	tl->tx_mesh->mesh_type = MESH_TRIANGLES;
	mesh_update_bounds(tl->tx_mesh);

	/*and setup the texture*/
	tl->txh.width = tl->tx_width;
	tl->txh.height = tl->tx_height;
	tl->txh.stride = 4*tl->tx_width;
	/*back to RGBA texturing*/
	{
		u32 i, j;
		for (i=0; i<tl->tx_height; i++) {
			char *data = tl->tx_data + i*tl->txh.stride;
			for (j=0; j<tl->txh.width; j++) {
				u32 val = *(u32 *) &data[4*j];
				data[4*j] = (val>>16) & 0xFF;
				data[4*j+1] = (val>>8) & 0xFF;
				data[4*j+2] = (val) & 0xFF;
				data[4*j+3] = (val>>24) & 0xFF;
			}
		}
	}
	
	tl->txh.data = tl->tx_data;
	tl->txh.pixelformat = GF_PIXEL_RGBA;
	if (!e) e = tx_allocate(&tl->txh);

	if (e) {
		tl->failed = 1;
		mesh_free(tl->tx_mesh);
		tl->tx_mesh = NULL;
		free(tl->tx_data);
		tl->tx_data = NULL;
		return 0;
	}
	tx_set_blend_mode(&tl->txh, TX_BLEND);
	tl->tx_ready = 1;


exit:
	R3D_SetTextureData(&tl->txh);
	return 1;
}

static void clean_paths(TextStack *stack)
{
	CachedTextLine *tl;
	/*delete all path objects*/
	while (gf_list_count(stack->text_lines)) {
		tl = gf_list_get(stack->text_lines, 0);
		gf_list_rem(stack->text_lines, 0);
		delete_text_line(tl);
	}
	stack->bounds.width = stack->bounds.height = 0;
}

static void DestroyText(GF_Node *node)
{
	TextStack *stack = (TextStack *) gf_node_get_private(node);
	clean_paths(stack);
	stack2D_predestroy((stack2D *)stack);
	gf_list_del(stack->text_lines);
	free(stack);
}

static Fixed get_font_size(M_FontStyle *fs, RenderEffect3D *eff)
{
	Fixed fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!eff->is_pixel_metrics) {
			Fixed w, h;
			R3D_GetSurfaceSizeInfo(eff, &w, &h);
			fontSize = gf_divfix(fontSize, w);
		}
    }
	return fontSize;
}

static void split_text_letters(TextStack *st, M_Text *txt, RenderEffect3D *eff)
{
	CachedTextLine *tl;
	unsigned short wcTemp[5000];
	unsigned short letter[2];
	u32 i, j, len;
	Fixed fontSize, start_y, font_height, line_spacing;
	GF_FontRaster *ft_r = st->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = get_font_size(fs, eff);
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_r->set_font(ft_r, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_r->set_font(ft_r, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_r->set_font_size(ft_r, fontSize);
	ft_r->get_font_metrics(ft_r, &st->ascent, &st->descent, &font_height);

			
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
			tl = new_text_line(eff->surface->render, st->text_lines);
			ft_r->add_text_to_path(ft_r, tl->path, 1, letter, 0, start_y, FIX_ONE, FIX_ONE, st->ascent, &tl->bounds);
			tl->bounds.height = MAX(st->ascent + st->descent, tl->bounds.height);
		}
	}
}

static void split_text_words(TextStack *st, M_Text *txt, RenderEffect3D *eff)
{
	CachedTextLine *tl;
	unsigned short wcTemp[5000];
	unsigned short letter[5000];
	u32 i, j, len, k, first_char;
	Fixed fontSize, font_height, line_spacing;
	GF_Rect rc;
	GF_FontRaster *ft_r = st->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = get_font_size(fs, eff);
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	if (ft_r->set_font(ft_r, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_r->set_font(ft_r, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_r->set_font_size(ft_r, fontSize);
	ft_r->get_font_metrics(ft_r, &st->ascent, &st->descent, &font_height);

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
			tl = new_text_line(eff->surface->render, st->text_lines);

			/*word splitting only happen in layout, so we don't need top/left anchors*/
			ft_r->add_text_to_path(ft_r, tl->path, 1, letter, 0, 0, FIX_ONE, FIX_ONE, st->ascent, &rc);
			gf_path_get_bounds(tl->path, &tl->bounds);
			tl->bounds.width = rc.width;
			if (tl->bounds.x != 0) tl->bounds.width -= tl->bounds.x;
			tl->bounds.x = 0;
			tl->bounds.height = MAX(st->ascent + st->descent, tl->bounds.height);
			if (tl->bounds.y != 0) tl->bounds.height -= tl->bounds.y;
			tl->bounds.y = 0;
			first_char = j+1;
		}
	}
}

typedef struct
{
	unsigned short *wcText;
	u32 length;
	Fixed width, height;
	Fixed x_scaling, y_scaling;
} TextLine;

static void BuildVerticalTextGraph(TextStack *st, M_Text *txt, RenderEffect3D *eff)
{
	TextLine *lines;
	CachedTextLine *tl;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, space, line_spacing, tot_width, tot_height, max_scale;
	GF_Rect rc, final;
	Fixed lw, lh, max_lw;
	unsigned short letter[2];
	GF_FontRaster *ft_r = st->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = get_font_size(fs, eff);
	line_spacing = gf_mulfix(FSSPACE , fontSize);

	if (ft_r->set_font(ft_r, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_r->set_font(ft_r, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_r->set_font_size(ft_r, fontSize);
	ft_r->get_font_metrics(ft_r, &st->ascent, &st->descent, &space);


	/*compute overall bounding box size*/
	tot_width = 0;
	tot_height = 0;
	lines = (TextLine *) malloc(sizeof(TextLine)*txt->string.count);
	memset(lines, 0, sizeof(TextLine)*txt->string.count);
	
	letter[1] = (unsigned short) '\0';

	for (i=0; i < txt->string.count; i++) {
		Fixed tmp;
		char *str = txt->string.vals[i];
		if (!str) continue;
		lines[i].length = 0;
		len = gf_utf8_mbstowcs(wcTemp, 5000, (const char **) &str);
		if (len == (size_t) (-1)) continue;

		lines[i].wcText = malloc(sizeof(unsigned short) * len);
		memcpy(lines[i].wcText, wcTemp, sizeof(unsigned short) * len);
		lines[i].length = len;
		
		lines[i].y_scaling = lines[i].x_scaling = FIX_ONE;
		lines[i].height = gf_mulfix(len , space);
		if (!lines[i].height) continue;

		if ((txt->length.count>i) && (txt->length.vals[i]>0) ) 
			lines[i].y_scaling = gf_divfix(txt->length.vals[i], lines[i].height);
	
		tmp = gf_mulfix(lines[i].height , lines[i].y_scaling);
		if (tot_height < tmp) tot_height = tmp;
	}

	tot_width = gf_mulfix(txt->string.count , line_spacing);
	st->bounds.width = tot_width;

	max_scale = FIX_ONE;
	if ((txt->maxExtent>0) && (tot_height>txt->maxExtent)) {
		max_scale = gf_divfix(txt->maxExtent , tot_height);
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
			tl = new_text_line(eff->surface->render, st->text_lines);

			/*adjust horizontal offset on first column*/
			if (!i) {
				max_lw = 0;
				for (k=0; k<lines[i].length; k++) {
					letter[0] = lines[i].wcText[k];
					/*get glyph width so that all letters are centered on the same vertical line*/
					ft_r->get_text_size(ft_r, letter, &lw, &lh);
					if (max_lw < lw) max_lw = lw;
				}
				st->bounds.width += max_lw/2;
				start_x += max_lw/2;
			}
			
			for (k=0; k<lines[i].length; k++) {
				letter[0] = lines[i].wcText[k];
				/*get glyph width so that all letters are centered on the same vertical line*/
				ft_r->get_text_size(ft_r, letter, &lw, &lh);
				ft_r->add_text_to_path(ft_r, tl->path, 1, letter, start_x - lw/2, start_y, lines[i].x_scaling, gf_mulfix(lines[i].y_scaling, max_scale), st->ascent, &rc);

				if (FSTTB)
					start_y -= space;
				else
					start_y += space;
			}
			gf_path_get_bounds(tl->path, &rc);
			gf_rect_union(&final, &rc);
		}

		if (FSLTR) {
			start_x += line_spacing;
		} else {
			start_x -= line_spacing;
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


static void BuildTextGraph(TextStack *st, M_Text *txt, RenderEffect3D *eff)
{
	TextLine *lines;
	CachedTextLine *tl;
	unsigned short wcTemp[5000];
	u32 i, int_major, len, k;
	Fixed fontSize, start_x, start_y, font_height, line_spacing, tot_width, tot_height, max_scale;
	GF_Rect final;
	GF_FontRaster *ft_r = st->compositor->font_engine;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	if (!FSHORIZ) {
		BuildVerticalTextGraph(st, txt, eff);
		return;
	}

	fontSize = get_font_size(fs, eff);
	if (ft_r->set_font(ft_r, FSFAMILY, FSSTYLE) != GF_OK) {
		if (ft_r->set_font(ft_r, NULL, NULL) != GF_OK) {
			return;
		}
	}
	ft_r->set_font_size(ft_r, fontSize);
	ft_r->get_font_metrics(ft_r, &st->ascent, &st->descent, &font_height);

	/*spacing= FSSPACING * (font_height) and fontSize not adjusted */
	line_spacing = gf_mulfix(FSSPACE , fontSize);
	
	tot_width = 0;
	lines = (TextLine *) malloc(sizeof(TextLine)*txt->string.count);
	memset(lines, 0, sizeof(TextLine)*txt->string.count);
	
	for (i=0; i < txt->string.count; i++) {
		Fixed tmp;
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
		ft_r->get_text_size(ft_r, lines[i].wcText, &lines[i].width, &lines[i].height);

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
			tl = new_text_line(eff->surface->render, st->text_lines);
			/*if using the font engine the font is already configured*/
			ft_r->add_text_to_path(ft_r, tl->path, 1, lines[i].wcText, start_x, start_y, gf_mulfix(lines[i].x_scaling, max_scale), lines[i].y_scaling, st->ascent, &tl->bounds);
			gf_rect_union(&final, &tl->bounds);
			gf_path_get_bounds(tl->path, &tl->bounds);
		}
		if (FSTTB) {
			start_y -= line_spacing;
		} else {
			start_y += line_spacing;
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

static void Text_SetupBounds(RenderEffect3D *eff, TextStack *st)
{
	CachedTextLine *tl;
	u32 i;
	/*text is not splitted, handle as a whole node*/
	if (!eff->text_split_mode) {
		if (eff->parent) group_end_text_child(eff->parent, &st->bounds, st->ascent, st->descent, 0);
		gf_bbox_from_rect(&eff->bbox, &st->bounds);
		return;
	}
	assert(eff->parent);

	/*otherwise notify each node as a different group*/
	i=0;
	while ((tl = gf_list_enum(st->text_lines, &i))) {
		/*the first one is always started by parent group*/
		if (i>1) group_start_child(eff->parent, NULL);
		group_end_text_child(eff->parent, &tl->bounds, st->ascent, st->descent, i);
	}
}

static void Text_FillTextLine(RenderEffect3D *eff, CachedTextLine *tl)
{
	if (!tl->mesh) {
		tl->mesh = new_mesh();
		mesh_from_path(tl->mesh, tl->path);
	}
	VS3D_DrawMesh(eff, tl->mesh);
}

static void Text_StrikeTextLine(RenderEffect3D *eff, CachedTextLine *tl, Aspect2D *asp, Bool vect_outline)
{
	if (!tl->outline_mesh) {
		tl->outline_mesh = new_mesh();
#ifndef GPAC_USE_OGL_ES
		if (vect_outline) {
			if (!tl->outline) tl->outline = gf_path_get_outline(tl->path, asp->pen_props);
			TesselatePath(tl->outline_mesh, tl->outline, asp->txh ? 2 : 1);
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
		VS3D_DrawMesh(eff, tl->outline_mesh);
	} else {
		VS3D_StrikeMesh(eff, tl->outline_mesh, Aspect_GetLineWidth(asp), asp->pen_props.dash);
	}
}

static void Text_Draw(RenderEffect3D *eff, TextStack *st)
{
	u32 i;
	Aspect2D asp;
	const char *fs_style;
	char *hlight;
	SFColorRGBA hl_color;
	CachedTextLine *tl;
	Bool draw2D, draw3D, vect_outline, can_texture_text;
	Render3D *sr = (Render3D*)st->compositor->visual_renderer->user_priv;
	M_FontStyle *fs = (M_FontStyle *) ((M_Text *) st->owner)->fontStyle;

	vect_outline = !sr->raster_outlines;

	VS3D_SetState(eff->surface, F3D_BLEND, 0);

	/*get material, check if 2D or 3D drawing shall be used*/
	if (!VS_GetAspect2D(eff, &asp)) {
		draw2D = 0;
		draw3D = VS_SetupAppearance(eff);
	} else {
		draw3D = 0;
		draw2D = (asp.filled && asp.alpha) ? 1 : 0;
		hl_color = gf_sg_sfcolor_to_rgba(asp.fill_color);
	}


	fs_style = FSSTYLE;
	hlight = NULL;
	hlight = strstr(fs_style, "HIGHLIGHT");
	if (hlight) hlight = strchr(hlight, '#');
	if (hlight && (draw2D || draw3D) ) {
		hlight += 1;
		/*reverse video: highlighting uses the text color, and text color is inverted (except alpha channel)
		the ideal impl would be to use the background color for the text, but since the text may be 
		displayed over anything non uniform this would require clipping the highlight rect with the text
		which is too onerous (and not supported anyway) */
		if (!strnicmp(hlight, "RV", 2)) {
			if (draw3D) {
				if (eff->appear) {
					SFColor c, rc;
					c = ((M_Material *) ((M_Appearance *)  eff->appear)->material)->diffuseColor;
					hl_color = gf_sg_sfcolor_to_rgba(c);
					hl_color.alpha = ((M_Material *) ((M_Appearance *)  eff->appear)->material)->transparency;
					/*invert diffuse color and resetup*/
					rc.red = FIX_ONE - c.red;
					rc.green = FIX_ONE - c.green;
					rc.blue = FIX_ONE - c.blue;
					((M_Material *) ((M_Appearance *)  eff->appear)->material)->diffuseColor = rc;
					VS_SetupAppearance(eff);
					((M_Material *) ((M_Appearance *)  eff->appear)->material)->diffuseColor = c;
				} else {
					hl_color.red = hl_color.green = hl_color.blue = 0;
					hl_color.alpha = FIX_ONE;
				}
			} else {
				hl_color = gf_sg_sfcolor_to_rgba(asp.fill_color);
				if (asp.alpha) {
					asp.fill_color.red = FIX_ONE - asp.fill_color.red;
					asp.fill_color.green = FIX_ONE - asp.fill_color.green;
					asp.fill_color.blue = FIX_ONE - asp.fill_color.blue;
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
		if (asp.alpha == 0) hlight = NULL;
	}
	if (strstr(fs_style, "TEXTURED")) st->texture_text_flag = 1;

	/*setup texture*/
	VS_setup_texture(eff);
	can_texture_text = 0;
	if (draw2D || draw3D) {
		/*check if we can use text texturing*/
		if (sr->compositor->texture_text_mode || st->texture_text_flag) {
			if (draw2D && asp.pen_props.width) {
				can_texture_text = 0;
			} else {
				can_texture_text = eff->mesh_has_texture ? 0 : 1;
			}
		}
	}

	VS3D_SetAntiAlias(eff->surface, st->compositor->antiAlias);
	if (draw2D || draw3D || eff->mesh_has_texture) {

		if (draw2D) VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);

		if (eff->split_text_idx) {
			tl = gf_list_get(st->text_lines, eff->split_text_idx-1);
			assert(tl);
			if (hlight) {
				VS3D_FillRect(eff->surface, tl->bounds, hl_color);
				if (draw2D) VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);
				else VS_SetupAppearance(eff);
			}

			if (can_texture_text && TextLine_TextureIsReady(tl)) {
				eff->mesh_has_texture = 1;
				tx_enable(&tl->txh, NULL);
				VS3D_DrawMesh(eff, tl->tx_mesh);
				tx_disable(&tl->txh);
				/*be nice to GL, we remove the text from HW at each frame since there may be a lot of text*/
				R3D_TextureHWReset(&tl->txh);
			} else {
				Text_FillTextLine(eff, tl);
			}
		} else {
			i=0; 
			while ((tl = gf_list_enum(st->text_lines, &i))) {

				if (hlight) {
					VS3D_FillRect(eff->surface, tl->bounds, hl_color);
					if (draw2D) VS3D_SetMaterial2D(eff->surface, asp.fill_color, asp.alpha);
					else VS_SetupAppearance(eff);
				}

				if (can_texture_text && TextLine_TextureIsReady(tl)) {
					eff->mesh_has_texture = 1;
					tx_enable(&tl->txh, NULL);
					VS3D_DrawMesh(eff, tl->tx_mesh);
					tx_disable(&tl->txh);
					/*be nice to GL, we remove the text from HW at each frame since there may be a lot of text*/
					R3D_TextureHWReset(&tl->txh);
				} else {
					Text_FillTextLine(eff, tl);
				}
			}
		}
		/*reset texturing in case of line texture*/
		VS_disable_texture(eff);
	}

	VS3D_SetState(eff->surface, F3D_BLEND, 0);

	if (!draw3D && asp.pen_props.width) {
		Fixed w = asp.pen_props.width;
		asp.pen_props.width = gf_divfix(asp.pen_props.width, asp.line_scale);
		VS_Set2DStrikeAspect(eff->surface, &asp);

		if (eff->split_text_idx) {
			tl = gf_list_get(st->text_lines, eff->split_text_idx-1);
			Text_StrikeTextLine(eff, tl, &asp, vect_outline);
		} else {
			i=0;
			while ((tl = gf_list_enum(st->text_lines, &i))) {
				Text_StrikeTextLine(eff, tl, &asp, vect_outline);
			}
		}
		asp.pen_props.width = w;
	}
}

static void Text_Render(GF_Node *n, void *rs)
{
	M_Text *txt = (M_Text *) n;
	TextStack *st = (TextStack *) gf_node_get_private(n);
	RenderEffect3D *eff = (RenderEffect3D *)rs;

	if (!st->compositor->font_engine) return;
	if (!txt->string.count) return;

	/*check for geometry change*/
	if (gf_node_dirty_get(n)) {
		clean_paths(st);
		stack2D_reset((stack2D *) st);
		gf_node_dirty_clear(n, 0);

		if (eff->text_split_mode == 2) {
			split_text_letters(st, txt, eff);
		} else if (eff->text_split_mode == 1) {
			split_text_words(st, txt, eff);
		} else {
			BuildTextGraph(st, txt, eff);
		}
		/*that's the largest bounds of the text in normal display - store it for culling*/
		gf_bbox_from_rect(&st->mesh->bounds, &st->bounds);
	}

	/*drawing*/
	if (!eff->traversing_mode) {
		Text_Draw(eff, st);
	}
	/*getting bounds info*/
	else if (eff->traversing_mode==TRAVERSE_GET_BOUNDS) {
		Text_SetupBounds(eff, st);
	}
}

static Bool TextIntersectWithRay(GF_Node *owner, GF_Ray *ray, SFVec3f *outPoint)
{
	TextStack *st = gf_node_get_private(owner);
	u32 i;
	CachedTextLine *tl;

	if (!R3D_Get2DPlaneIntersection(ray, outPoint)) return 0;

	i=0;
	while ((tl = gf_list_enum(st->text_lines, &i))) {	
		if ( (outPoint->x >= tl->bounds.x) 
		&& (outPoint->y <= tl->bounds.y) 
		&& (outPoint->x <= tl->bounds.x + tl->bounds.width)
		&& (outPoint->y >= tl->bounds.y - tl->bounds.height) 
		) return 1;
	}
	return 0;
}

void R3D_InitText(Render3D *sr, GF_Node *node)
{
	TextStack *stack;
	GF_SAFEALLOC(stack, TextStack);
	stack2D_setup((stack2D *)stack, sr->compositor, node);
	/*override all funct*/
	stack->ascent = stack->descent = 0;
	stack->text_lines = gf_list_new();
	gf_node_set_private(node, stack);
	gf_node_set_render_function(node, Text_Render);
	gf_node_set_predestroy_function(node, DestroyText);
	stack->IntersectWithRay = TextIntersectWithRay;
}

#include "mesh.h"
void Text_Extrude(GF_Node *node, RenderEffect3D *eff, GF_Mesh *mesh, MFVec3f *thespine, Fixed creaseAngle, Bool begin_cap, Bool end_cap, MFRotation *spine_ori, MFVec2f *spine_scale, Bool txAlongSpine)
{
	u32 i, count;
	Fixed min_cx, min_cy, width_cx, width_cy;
	CachedTextLine *tl;
	TextStack *st = (TextStack *) gf_node_get_private(node);

	/*rebuild text node*/
	if (gf_node_dirty_get(node)) {
		struct _parent_group *parent = eff->parent;
		eff->parent = NULL;
		clean_paths(st);
		stack2D_reset((stack2D *) st);
		gf_node_dirty_clear(node, 0);
		BuildTextGraph(st, (M_Text *)node, eff);
		eff->parent = parent;
	}

	min_cx = st->bounds.x;
	min_cy = st->bounds.y - st->bounds.height;
	width_cx = st->bounds.width;
	width_cy = st->bounds.height;

	mesh_reset(mesh);
	count = gf_list_count(st->text_lines);
	for (i=0; i<count; i++) {
		tl = gf_list_get(st->text_lines, i);
		mesh_extrude_path_ext(mesh, tl->path, thespine, creaseAngle, min_cx, min_cy, width_cx, width_cy, begin_cap, end_cap, spine_ori, spine_scale, txAlongSpine);
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

static void RenderTextureText(GF_Node *node, void *rs)
{
	u32 ntag;
	TextStack *stack;
	GF_Node *text;
	GF_FieldInfo field;
	if (gf_node_get_field(node, 0, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFNODE) return;
	text = *(GF_Node **)field.far_ptr;
	if (!text) return;

	if (gf_node_get_field(node, 1, &field) != GF_OK) return;
	if (field.fieldType != GF_SG_VRML_SFBOOL) return;

	ntag = gf_node_get_tag(text);
	if ((ntag != TAG_MPEG4_Text) && (ntag != TAG_X3D_Text)) return;
	stack = (TextStack *) gf_node_get_private(text);
	stack->texture_text_flag = *(SFBool*)field.far_ptr ? 1 : 0;
}

void R3D_InitTextureText(Render3D *sr, GF_Node *node)
{
	gf_node_set_render_function(node, RenderTextureText);
}
