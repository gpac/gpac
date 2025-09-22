/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

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
	struct _drawable s_graph;
	Fixed ascent, descent;
	GF_List *spans;
	GF_Rect bounds;
	u32 texture_text_flag;
	Bool is_dirty;
	GF_Compositor *compositor;
} TextStack;

void text_clean_paths(GF_Compositor *compositor, TextStack *stack)
{
	/*delete all path objects*/
	while (gf_list_count(stack->spans)) {
		GF_TextSpan *span = (GF_TextSpan*) gf_list_get(stack->spans, 0);
		gf_list_rem(stack->spans, 0);
		gf_font_manager_delete_span(compositor->font_manager, span);
	}
	stack->bounds.width = stack->bounds.height = 0;
	drawable_reset_path(&stack->s_graph);
}


static void build_text_split(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	u32 i, j, k, len, styles, idx, first_char;
	Bool split_words = GF_FALSE;
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
		if (strstr(fs->style.buffer, "BOLD") || strstr(fs->style.buffer, "bold")) styles |= GF_FONT_WEIGHT_BOLD;
		if (strstr(fs->style.buffer, "ITALIC") || strstr(fs->style.buffer, "italic")) styles |= GF_FONT_ITALIC;
		if (strstr(fs->style.buffer, "UNDERLINED") || strstr(fs->style.buffer, "underlined")) styles |= GF_FONT_UNDERLINED;
		if (strstr(fs->style.buffer, "STRIKETHROUGH") || strstr(fs->style.buffer, "strikethrough")) styles |= GF_FONT_STRIKEOUT;
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
	split_words = (tr_state->text_split_mode==1) ? GF_TRUE : GF_FALSE;

	for (i=0; i < txt->string.count; i++) {

		char *str = txt->string.vals[i];
		if (!str || !strlen(str)) continue;

		tspan = gf_font_manager_create_span(ft_mgr, font, str, fontSize, GF_FALSE, GF_FALSE, GF_FALSE, NULL, GF_FALSE, styles, (GF_Node*)txt);
		if (!tspan) continue;

		len = tspan->nb_glyphs;
		tspan->flags |= GF_TEXT_SPAN_HORIZONTAL;

		first_char = 0;
		for (j=0; j<len; j++) {
			u32 is_space = 0;
			GF_TextSpan *span;

			if (!tspan->glyphs[j]) continue;

			/*we currently only split sentences at spaces*/
			if (tspan->glyphs[j]->utf_name == (unsigned short) ' ') is_space = 1;
			else if (tspan->glyphs[j]->utf_name == (unsigned short) '\n')
				is_space = 2;
			if (split_words && (j+1!=len) && !is_space)
				continue;

			span = (GF_TextSpan*) gf_malloc(sizeof(GF_TextSpan));
			memcpy(span, tspan, sizeof(GF_TextSpan));

			span->nb_glyphs = split_words ? (j - first_char) : 1;
			if (split_words && !is_space) span->nb_glyphs++;
			span->glyphs = (GF_Glyph**)gf_malloc(sizeof(void *)*span->nb_glyphs);

			span->bounds.height = st->ascent + st->descent;
			span->bounds.y = start_y;
			span->bounds.x = 0;
			span->bounds.width = 0;
			span->bounds.y += 2;
			span->bounds.height += 4;

			if (split_words) {
				for (k=0; k<span->nb_glyphs; k++) {
					span->glyphs[k] = tspan->glyphs[FSLTR ? (first_char+k) : (len - first_char - k - 1)];
					span->bounds.width += tspan->font_scale * (span->glyphs[k] ? span->glyphs[k]->horiz_advance : tspan->font->max_advance_h);
				}
			} else {
				//span->glyphs[0] = tspan->glyphs[FSLTR ? j : (len - j - 1) ];
				span->glyphs[0] = tspan->glyphs[j];
				span->bounds.width = tspan->font_scale * (span->glyphs[0] ? span->glyphs[0]->horiz_advance : tspan->font->max_advance_h);
			}

			gf_list_add(st->spans, span);

			/*request a context (first one is always valid when entering sort phase)*/
			if (idx) parent_node_start_group(tr_state->parent, NULL, GF_FALSE);

			idx++;
			parent_node_end_text_group(tr_state->parent, &span->bounds, st->ascent, st->descent, idx);

			if (is_space && split_words) {
				span = (GF_TextSpan*) gf_malloc(sizeof(GF_TextSpan));
				memcpy(span, tspan, sizeof(GF_TextSpan));
				span->nb_glyphs = 1;
				span->glyphs = (GF_Glyph**)gf_malloc(sizeof(void *));

				gf_list_add(st->spans, span);
				span->bounds.height = st->ascent + st->descent;
				span->bounds.y = start_y;
				span->bounds.x = 0;
				span->bounds.y += 2;
				span->bounds.height += 4;
				k = (j - first_char);
				span->glyphs[0] = tspan->glyphs[FSLTR ? (first_char+k) : (len - first_char - k - 1)];
				span->bounds.width = tspan->font_scale * (span->glyphs[0] ? span->glyphs[0]->horiz_advance : tspan->font->max_advance_h);
				parent_node_start_group(tr_state->parent, NULL, is_space);
				idx++;
				parent_node_end_text_group(tr_state->parent, &span->bounds, st->ascent, st->descent, idx);
			}
			first_char = j+1;
		}
		gf_font_manager_delete_span(ft_mgr, tspan);
	}
}


static void build_text(TextStack *st, M_Text *txt, GF_TraverseState *tr_state)
{
	u32 i, j, int_major, k, styles, count;
	Fixed fontSize, start_x, start_y, line_spacing, tot_width, tot_height, max_scale, maxExtent;
	u32 size, trim_size;
	GF_Font *font;
	char szBuf[100];
	Bool horizontal, use_pass = GF_FALSE;
	GF_TextSpan *trim_tspan = NULL;
	GF_FontManager *ft_mgr = tr_state->visual->compositor->font_manager;
	M_FontStyle *fs = (M_FontStyle *)txt->fontStyle;

	fontSize = FSSIZE;
	if (fontSize <= 0) {
		fontSize = INT2FIX(12);
		if (!tr_state->pixel_metrics) fontSize = gf_divfix(fontSize, tr_state->visual->compositor->output_width);
	}
	horizontal = FSHORIZ;
	start_x = start_y = 0;

	styles = 0;
	if (fs && fs->style.buffer) {
		if (strstr(fs->style.buffer, "BOLD") || strstr(fs->style.buffer, "bold")) styles |= GF_FONT_WEIGHT_BOLD;
		if (strstr(fs->style.buffer, "ITALIC") || strstr(fs->style.buffer, "italic")) styles |= GF_FONT_ITALIC;
		if (strstr(fs->style.buffer, "UNDERLINED") || strstr(fs->style.buffer, "underlined")) styles |= GF_FONT_UNDERLINED;
		if (strstr(fs->style.buffer, "STRIKETHROUGH") || strstr(fs->style.buffer, "strikethrough")) styles |= GF_FONT_STRIKEOUT;
		if (strstr(fs->style.buffer, "PASSWD") || strstr(fs->style.buffer, "passwd")) use_pass = GF_TRUE;
	}

	font = gf_font_manager_set_font(ft_mgr, fs ? fs->family.vals : NULL, fs ? fs->family.count : 0, styles);
	if (!font) return;

	/*NOTA: we could use integer maths here but we have a risk of overflow with large fonts, so use fixed maths*/
	st->ascent = gf_muldiv(fontSize, INT2FIX(font->ascent), INT2FIX(font->em_size));
	st->descent = -gf_muldiv(fontSize, INT2FIX(font->descent), INT2FIX(font->em_size));
	line_spacing = gf_mulfix(FSSPACE, fontSize);

	maxExtent = txt->maxExtent;
	trim_size = 0;

	if (maxExtent<0) {
		trim_tspan = gf_font_manager_create_span(ft_mgr, font, "...", fontSize, GF_FALSE, GF_FALSE, GF_FALSE, NULL, GF_FALSE, styles, (GF_Node*)txt);
		if (trim_tspan) {
			for (i=0; i<trim_tspan->nb_glyphs; i++) {
				if (horizontal) {
					trim_size += trim_tspan->glyphs[i] ? trim_tspan->glyphs[i]->horiz_advance : trim_tspan->font->max_advance_h;
				} else {
					trim_size += trim_tspan->glyphs[i] ? trim_tspan->glyphs[i]->vert_advance : trim_tspan->font->max_advance_v;
				}
			}
		}
	}

	tot_width = tot_height = 0;
	for (i=0; i < txt->string.count; i++) {
		GF_TextSpan *tspan;

		char *str = txt->string.vals[i];
		if (!str) continue;

		if (use_pass) {
			u32 j=0, len = (u32) strlen(str);
			if (len>=100) len=99;
			while (j<len) {
				if (str[j] == 0x1)
					szBuf[j] = str[j];
				else
					szBuf[j] = '*';
				j++;
			}
			szBuf[j] = 0;
			str = szBuf;
		}

		tspan = gf_font_manager_create_span(ft_mgr, font, str, fontSize, GF_FALSE, GF_FALSE, GF_FALSE, NULL, GF_FALSE, styles, (GF_Node*)txt);
		if (!tspan) continue;

		if (horizontal) tspan->flags |= GF_TEXT_SPAN_HORIZONTAL;

		size = 0;
		if (trim_size) {
			for (j=0; j<tspan->nb_glyphs; j++) {
				if (horizontal) {
					size += tspan->glyphs[j] ? tspan->glyphs[j]->horiz_advance : tspan->font->max_advance_h;
				} else {
					size += tspan->glyphs[j] ? tspan->glyphs[j]->vert_advance : tspan->font->max_advance_v;
				}
				/*word is bigger than allowed extent, rewrite 3 previous chars*/
				if ((s32)size*tspan->font_scale >= -maxExtent) {
					u32 nb_chars = (j<2) ? j : 3;

					for (k=0; k<nb_chars; k++) {
						u32 idx = nb_chars-k-1;
						if (horizontal) {
							size -= tspan->glyphs[j-k] ? tspan->glyphs[j-k]->horiz_advance : tspan->font->max_advance_h;
							size += trim_tspan->glyphs[idx] ? trim_tspan->glyphs[idx]->horiz_advance : tspan->font->max_advance_h;
						} else {
							size -= tspan->glyphs[j-k] ? tspan->glyphs[j-k]->vert_advance : tspan->font->max_advance_v;
							size += trim_tspan->glyphs[idx] ? trim_tspan->glyphs[idx]->vert_advance : tspan->font->max_advance_v;
						}
						tspan->glyphs[j-k] = trim_tspan->glyphs[idx];
					}
					tspan->nb_glyphs = j+1;
					break;
				}
			}
		}

		if ((horizontal && !FSLTR) || (!horizontal && !FSTTB)) {
			for (k=0; k<tspan->nb_glyphs/2; k++) {
				GF_Glyph *g = tspan->glyphs[k];
				tspan->glyphs[k] = tspan->glyphs[tspan->nb_glyphs-1-k];
				tspan->glyphs[tspan->nb_glyphs-k-1] = g;
			}
		}

		if (!size) {
			for (j=0; j<tspan->nb_glyphs; j++) {
				if (horizontal) {
					size += tspan->glyphs[j] ? tspan->glyphs[j]->horiz_advance : tspan->font->max_advance_h;
				} else {
					size += tspan->glyphs[j] ? tspan->glyphs[j]->vert_advance : tspan->font->max_advance_v;
				}
			}
		}
		gf_list_add(st->spans, tspan);

		if (horizontal) {
			tspan->bounds.width = tspan->font_scale * size;
			/*apply length*/
			if ((txt->length.count>i) && (txt->length.vals[i]>0)) {
				tspan->x_scale = gf_divfix(txt->length.vals[i], tspan->bounds.width);
				tspan->bounds.width = txt->length.vals[i];
			}
			if (tot_width < tspan->bounds.width ) tot_width = tspan->bounds.width;
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
	if (trim_tspan)	gf_font_manager_delete_span(ft_mgr, trim_tspan);


	max_scale = FIX_ONE;
	if (horizontal) {
		if ((maxExtent > 0) && (tot_width>maxExtent)) {
			max_scale = gf_divfix(maxExtent, tot_width);
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
		if ((maxExtent > 0) && (tot_height>maxExtent) ) {
			max_scale = gf_divfix(maxExtent, tot_height);
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

	count = gf_list_count(st->spans);
	for (i=0; i < count; i++) {
		GF_TextSpan *span = (GF_TextSpan*)gf_list_get(st->spans, i);
		switch (int_major) {
		/*major-justification MIDDLE*/
		case 0:
			if (horizontal) {
				start_x = -span->bounds.width/2;
			} else {
				//start_y = FSTTB ? span->bounds.height/2 : (-span->bounds.height/2 + space);
				start_y = span->bounds.height/2;
			}
			break;
		/*major-justification END*/
		case 1:
			if (horizontal) {
				start_x = (FSLTR) ? -span->bounds.width : 0;
			} else {
				//start_y = FSTTB ? span->bounds.height : (-span->bounds.height + space);
				start_y = FSTTB ? span->bounds.height : 0;
			}
			break;
		/*BEGIN, FIRST or default*/
		default:
			if (horizontal) {
				start_x = (FSLTR) ? 0 : -span->bounds.width;
			} else {
				//start_y = FSTTB ? 0 : space;
				start_y = FSTTB ? 0 : span->bounds.height;
			}
			break;
		}
		span->off_x = start_x;
		span->bounds.x = start_x;
		if (horizontal) {
			span->off_y = start_y - st->ascent;
			span->x_scale = gf_mulfix(span->x_scale, max_scale);
			span->bounds.y = start_y;
		} else {
			span->y_scale = gf_mulfix(span->y_scale, max_scale);
			span->off_y = start_y - gf_mulfix(st->ascent, span->y_scale);
			span->bounds.y = start_y;
		}
		span->off_x = gf_mulfix(span->off_x, max_scale);
		span->off_y = gf_mulfix(span->off_y, max_scale);

		if (horizontal) {
			start_y += FSTTB ? -line_spacing : line_spacing;
			span->bounds.height = st->descent + st->ascent;
		} else {
			start_x += FSLTR ? line_spacing : -line_spacing;
			span->bounds.width = line_spacing;
		}
		gf_rect_union(&st->bounds, &span->bounds);
	}
}

static void text_get_draw_opt(GF_Node *node, TextStack *st, Bool *force_texture, u32 *hl_color, DrawAspect2D *asp)
{
	const char *fs_style;
	char *hlight;
	M_FontStyle *fs = (M_FontStyle *) ((M_Text *) node)->fontStyle;

	*hl_color = 0;

	fs_style = FSSTYLE;
	hlight = strstr(fs_style, "HIGHLIGHT");
	if (hlight) hlight = strchr(hlight, '#');
	if (hlight) {
		hlight += 1;
		if (!strnicmp(hlight, "RV", 2)) *hl_color = 0x00FFFFFF;
		else {
			sscanf(hlight, "%x", hl_color);
			if (strlen(hlight)!=8) *hl_color |= 0xFF000000;
		}
	}
	*force_texture = st->texture_text_flag;
	if (strstr(fs_style, "TEXTURED")) *force_texture = GF_TRUE;
	if (strstr(fs_style, "OUTLINED")) {
		if (asp && !asp->pen_props.width) {
			asp->pen_props.width = FIX_ONE/2;
			asp->pen_props.align = GF_PATH_LINE_OUTSIDE;
			asp->line_scale=FIX_ONE;
			asp->line_color = 0xFF000000;
		}
	}
}


#ifndef GPAC_DISABLE_3D


static Bool text_is_3d_material(GF_TraverseState *tr_state)
{
	GF_Node *__mat;
	if (!tr_state->appear) return GF_FALSE;
	__mat = ((M_Appearance *)tr_state->appear)->material;
	if (!__mat) return GF_FALSE;
	if (gf_node_get_tag(__mat)==TAG_MPEG4_Material2D) return GF_FALSE;
	return GF_TRUE;
}

static void text_draw_3d(GF_TraverseState *tr_state, GF_Node *node, TextStack *st)
{
	DrawAspect2D the_asp, *asp;
	Bool is_3d, force_texture;
	u32 hl_color;

	is_3d = text_is_3d_material(tr_state);
	asp = NULL;
	if (!is_3d) {
		memset(&the_asp, 0, sizeof(DrawAspect2D));
		asp = &the_asp;
		drawable_get_aspect_2d_mpeg4(node, asp, tr_state);
	}
	text_get_draw_opt(node, st, &force_texture, &hl_color, asp);
	gf_font_spans_draw_3d(st->spans, tr_state, asp, hl_color, force_texture);
}

#endif




void text_draw_2d(GF_Node *node, GF_TraverseState *tr_state)
{
	Bool force_texture;
	u32 hl_color;
	TextStack *st = (TextStack *) gf_node_get_private((GF_Node *) node);

	if (!GF_COL_A(tr_state->ctx->aspect.fill_color) && !tr_state->ctx->aspect.pen_props.width) return;

	text_get_draw_opt(node, st, &force_texture, &hl_color, &tr_state->ctx->aspect);

	tr_state->text_parent = node;
	gf_font_spans_draw_2d(st->spans, tr_state, hl_color, force_texture, &st->bounds);
	tr_state->text_parent = NULL;
}



static void text_check_changes(GF_Node *node, TextStack *stack, GF_TraverseState *tr_state)
{
	if (gf_node_dirty_get(node) || tr_state->visual->compositor->reset_fonts) {
		text_clean_paths(tr_state->visual->compositor, stack);
		build_text(stack, (M_Text*)node, tr_state);
		gf_node_dirty_clear(node, 0);
		drawable_mark_modified(&stack->s_graph, tr_state);
	}

	if (tr_state->visual->compositor->edited_text && (tr_state->visual->compositor->focus_node==node)) {
		drawable_mark_modified(&stack->s_graph, tr_state);
		tr_state->visual->has_text_edit = GF_TRUE;
		if (!stack->bounds.width) stack->bounds.width = INT2FIX(1)/100;
		if (!stack->bounds.height) stack->bounds.height = INT2FIX(1)/100;
	}
}


static void Text_Traverse(GF_Node *n, void *rs, Bool is_destroy)
{
	DrawableContext *ctx;
	M_Text *txt = (M_Text *) n;
	TextStack *st = (TextStack *) gf_node_get_private(n);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	if (is_destroy) {
		GF_Compositor *compositor = gf_sc_get_compositor(n);
		text_clean_paths(compositor, st);
		drawable_del_ex(&st->s_graph, compositor, GF_TRUE);
		gf_list_del(st->spans);
		gf_free(st);
		return;
	}

	if (!txt->string.count) return;

	if (tr_state->text_split_mode) {
		st->is_dirty = gf_node_dirty_get(n) ? GF_TRUE : GF_FALSE;
		gf_node_dirty_clear(n, 0);
		text_clean_paths(tr_state->visual->compositor, st);
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
		tr_state->text_parent = n;
		gf_font_spans_pick(n, st->spans, tr_state, &st->bounds, GF_FALSE, NULL);
		tr_state->text_parent = NULL;
		return;
	case TRAVERSE_GET_BOUNDS:
		tr_state->bounds = st->bounds;
		return;
	case TRAVERSE_GET_TEXT:
		tr_state->text_parent = n;
		gf_font_spans_get_selection(n, st->spans, tr_state);
		tr_state->text_parent = NULL;
		return;
	case TRAVERSE_SORT:
		break;
	default:
		return;
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) return;
#endif

	ctx = drawable_init_context_mpeg4(&st->s_graph, tr_state);
	if (!ctx) return;
	ctx->sub_path_index = tr_state->text_split_idx;

	ctx->flags |= CTX_IS_TEXT;
	if (!GF_COL_A(ctx->aspect.fill_color)) {
		/*override line join*/
		ctx->aspect.pen_props.join = GF_LINE_JOIN_MITER;
		ctx->aspect.pen_props.cap = GF_LINE_CAP_FLAT;
	}

	/*if text selection mode, we must force redraw of the entire text span because we don't
	if glyphs have been (un)selected*/
	if (!tr_state->immediate_draw &&
	        /*text selection on*/
	        (tr_state->visual->compositor->text_selection
	         /*text sel release*/
	         || (tr_state->visual->compositor->store_text_state==GF_SC_TSEL_RELEASED))
	   ) {
		GF_TextSpan *span;
		u32 i = 0;
		Bool unselect = (tr_state->visual->compositor->store_text_state==GF_SC_TSEL_RELEASED) ? GF_TRUE : GF_FALSE;
		while ((span = (GF_TextSpan*)gf_list_enum(st->spans, &i))) {
			if (span->flags & GF_TEXT_SPAN_SELECTED) {
				if (unselect) span->flags &= ~GF_TEXT_SPAN_SELECTED;
				ctx->flags |= CTX_APP_DIRTY;
			}
		}
	} else if (st->is_dirty) {
		ctx->flags |= CTX_APP_DIRTY;
	}

	if (ctx->sub_path_index) {
		GF_TextSpan *span = (GF_TextSpan *)gf_list_get(st->spans, ctx->sub_path_index-1);
		if (span) drawable_finalize_sort(ctx, tr_state, &span->bounds);
	} else {
		drawable_finalize_sort(ctx, tr_state, &st->bounds);
	}
}


void compositor_init_text(GF_Compositor *compositor, GF_Node *node)
{
	TextStack *stack;
	GF_SAFEALLOC(stack, TextStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate text stack\n"));
		return;
	}
	drawable_init_ex(&stack->s_graph);
	stack->s_graph.node = node;
	stack->s_graph.flags = DRAWABLE_USE_TRAVERSE_DRAW;
	stack->ascent = stack->descent = 0;
	stack->spans = gf_list_new();
	stack->texture_text_flag = 0;

	stack->compositor = compositor;
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, Text_Traverse);
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
		drawable_reset_path(&st->s_graph);
		gf_node_dirty_clear(node, 0);
		build_text(st, (M_Text *)node, tr_state);
		tr_state->parent = parent;
	}

	min_cx = st->bounds.x;
	min_cy = st->bounds.y - st->bounds.height;
	width_cx = st->bounds.width;
	width_cy = st->bounds.height;

	mesh_reset(mesh);
	count = gf_list_count(st->spans);
	for (i=0; i<count; i++) {
		GF_TextSpan *span = (GF_TextSpan *)gf_list_get(st->spans, i);
		GF_Path *span_path = gf_font_span_create_path(span);
		mesh_extrude_path_ext(mesh, span_path, thespine, creaseAngle, min_cx, min_cy, width_cx, width_cy, begin_cap, end_cap, spine_ori, spine_scale, txAlongSpine);
		gf_path_del(span_path);
	}
	mesh_update_bounds(mesh);
	gf_mesh_build_aabbtree(mesh);
}

#endif /*GPAC_DISABLE_3D*/


#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

