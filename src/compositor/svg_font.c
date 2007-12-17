/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *
 *			Copyright (c) ENST 2007-200X
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

#ifndef GPAC_DISABLE_SVG

#include <gpac/utf.h>
#include "visual_manager.h"
#include "nodes_stacks.h"

typedef struct
{
	u16 *unicode;
	u16 uni_len;

	GF_Glyph glyph;
	GF_Font *font;
} SVG_GlyphStack;

/*translate string to glyph sequence*/
static GF_Err svg_font_get_glyphs(GF_Node *node, const char *utf_string, u32 *glyph_buffer, u32 *io_glyph_buffer_size, const char *lang)
{
	s32 len;
	u32 i;
	u16 *utf_res;
	GF_ChildNodeItem *child;
	GF_Font *font = gf_node_get_private(node);
	char *utf8 = (char*) utf_string;

	/*FIXME - use glyphs unicode attributes for glyph substitution*/
	len = utf_string ? strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}

	if (*io_glyph_buffer_size < len) {
		*io_glyph_buffer_size = len;
		return GF_BUFFER_TOO_SMALL;
	}

	len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, &utf8);
	if (len<0) return GF_IO_ERR;
	/*should not happen*/
	if (utf8) return GF_IO_ERR;

	/*unpack to 32bits*/
	utf_res = (u16 *) glyph_buffer;
	for (i=len; i>0; i--) {
		glyph_buffer[i-1] = utf_res[i-1];
	}

	for (i=0;i<len; i++) {
		SVG_GlyphStack *missing_glyph = NULL;
		SVG_GlyphStack *st = NULL;
		child = ((GF_ParentNode *) node)->children;
		while (child) {
			u32 tag = gf_node_get_tag(child->node);
			if (tag==TAG_SVG_missing_glyph) {
				missing_glyph = gf_node_get_private(child->node);
			} else if (tag ==TAG_SVG_glyph) {
				SVGAllAttributes atts;
				
				st = gf_node_get_private(child->node);
				if (!st) {
					child = child->next;
					continue;
				}
				
				if (st->glyph.utf_name==glyph_buffer[i]) {
					u32 j, count;
					if (!lang) break;
					gf_svg_flatten_attributes((SVG_Element*)child->node, &atts);
					if (!atts.lang) break;

					count = gf_list_count(*atts.lang);
					for (j=0; j<count; j++) {
						char *name = gf_list_get(*atts.lang, j);
						if (!stricmp(name, lang) || strstr(lang, name)) goto glyph_found;
					}
				}
				/*perform glyph substitution*/
				else if (st->uni_len>1) {
					u32 j;
					for (j=0; j<st->uni_len; j++) {
						if (i+j>=len) break;
						if (glyph_buffer[i+j] != st->unicode[j]) break;
					}
					if (j==st->uni_len) break;
				}
				st = NULL;
			}
			child = child->next;
		}
glyph_found:
		if (!st) st = missing_glyph;
		glyph_buffer[i] = st ? st->glyph.ID : 0;
		if (st && st->uni_len>1) len -= st->uni_len-1;
	}
	*io_glyph_buffer_size = len;
	return GF_OK;
}

/*loads glyph by name - returns NULL if glyph cannot be found*/
static GF_Glyph *svg_font_load_glyph(GF_Node *node, u32 glyph_name)
{
	GF_Font *font = gf_node_get_private(node);
	GF_ChildNodeItem *child = ((GF_ParentNode *) node)->children;

	while (child) {
		if (gf_node_get_tag(child->node)==TAG_SVG_glyph) {
			SVG_GlyphStack *st = gf_node_get_private(child->node);
			if (st->glyph.ID==glyph_name) {
				return &st->glyph;
			}
		}
		child = child->next;
	}
	
	return NULL;
}

static void svg_traverse_font(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Font *font = gf_node_get_private(node);
		if (font) {
			gf_font_manager_unregister_font(font->ft_mgr, font);
			if (font->name) free(font->name);
			free(font);
		}
	}
}

void compositor_init_svg_font(GF_Compositor *compositor, GF_Node *node)
{
	GF_Err e;
	SVGAllAttributes atts;
	GF_Font *font;
	GF_Node *node_font = gf_node_get_parent(node, 0);
	if (!node_font) return;

	if (gf_node_get_tag(node_font)!=TAG_SVG_font) return;

	gf_svg_flatten_attributes((SVG_Element*)node, &atts);
	if (!atts.font_family) return;

	/*register font to font manager*/
	GF_SAFEALLOC(font, GF_Font);
	e = gf_font_manager_register_font(compositor->font_manager, font);
	if (e) {
		free(font);
		return;
	}
	font->ft_mgr = compositor->font_manager;

	font->get_glyphs = svg_font_get_glyphs;
	font->load_glyph = svg_font_load_glyph;
	font->udta = node_font;
	gf_node_set_private(node_font, font);
	font->name = strdup(atts.font_family->value);

	font->em_size = atts.units_per_em ? FIX2INT(atts.units_per_em->value) : 0;
	font->ascent = atts.ascent ? FIX2INT(atts.ascent->value) : 0;
	font->descent = atts.descent ? FIX2INT(atts.descent->value) : 0;
	font->line_spacing = font->em_size;
	font->styles = 0;
	if (atts.font_style) {
		switch (*atts.font_style) {
		case SVG_FONTSTYLE_ITALIC:
		case SVG_FONTSTYLE_OBLIQUE:
			font->styles |= GF_FONT_ITALIC;
			break;
		}
	}
	if (atts.font_weight) {
		switch(*atts.font_weight) {
		case SVG_FONTWEIGHT_100:
		case SVG_FONTWEIGHT_LIGHTER:
		case SVG_FONTWEIGHT_200:
		case SVG_FONTWEIGHT_300:
		case SVG_FONTWEIGHT_400:
			break;
		case SVG_FONTWEIGHT_NORMAL:
		case SVG_FONTWEIGHT_500:
			break;	
		case SVG_FONTWEIGHT_600:
		case SVG_FONTWEIGHT_700:
		case SVG_FONTWEIGHT_BOLD:
		case SVG_FONTWEIGHT_800:
		case SVG_FONTWEIGHT_900:
		case SVG_FONTWEIGHT_BOLDER:
			font->styles |= GF_FONT_BOLD;
			break;
		}
	}

	gf_svg_flatten_attributes((SVG_Element*)node_font, &atts);
	font->max_advance_h = atts.horiz_adv_x ? FIX2INT(atts.horiz_adv_x->value) : 0;
	font->max_advance_v = font->max_advance_h;
}


static void svg_traverse_glyph(GF_Node *node, void *rs, Bool is_destroy)
{	
	if (is_destroy) {
		GF_Font *font;
		GF_Glyph *prev_glyph, *a_glyph;
		SVG_GlyphStack *st = gf_node_get_private(node);
		if (st->unicode) free(st->unicode);

		font = st->font;
		prev_glyph = NULL;
		a_glyph = font->glyph;
		while (a_glyph) {
			if (a_glyph == &st->glyph) break;
			prev_glyph = a_glyph;
			a_glyph = a_glyph->next;
		}
		if (prev_glyph) {
			prev_glyph->next = st->glyph.next;
		} else {
			font->glyph = st->glyph.next;
		}
		free(st);
	}
}

void compositor_init_svg_glyph(GF_Compositor *compositor, GF_Node *node)
{
	u16 utf_name[20];
	u8 *utf8;
	u32 len;
	GF_Glyph *glyph;
	GF_Font *font;
	SVG_GlyphStack *st;
	SVGAllAttributes atts;
	GF_Node *node_font = gf_node_get_parent(node, 0);

	/*locate the font node*/
	if (node_font) node_font = gf_node_get_parent(node, 0);
	if (!node_font || (gf_node_get_tag(node_font)!=TAG_SVG_font) ) return;
	font = gf_node_get_private(node_font);
	if (!font) return;

	gf_svg_flatten_attributes((SVG_Element*)node, &atts);

	if (gf_node_get_tag(node)==TAG_SVG_missing_glyph) {
		GF_SAFEALLOC(st, SVG_GlyphStack);
		goto reg_common;
	}
	/*we must have unicode specified*/
	if (!atts.unicode) return;

	GF_SAFEALLOC(st, SVG_GlyphStack);
	utf8 = *atts.unicode;
	len = gf_utf8_mbstowcs(utf_name, 200, &utf8);
	/*this is a single glyph*/
	if (len==1) {
		st->glyph.utf_name = utf_name[0];
		st->uni_len = 1;
	} else {
		st->glyph.utf_name = (u32) st;
		st->unicode = malloc(sizeof(u16)*len);
		st->uni_len = len;
		memcpy(st->unicode, utf_name, sizeof(u16)*len);
	}

reg_common:
	st->glyph.ID = (u32) st;
	st->font = font;
	st->glyph.horiz_advance = font->max_advance_h;
	if (atts.horiz_adv_x) st->glyph.horiz_advance = FIX2INT(atts.horiz_adv_x->value);
	if (atts.d) st->glyph.path = atts.d;

	/*register glyph*/
	if (!font->glyph) {
		font->glyph = &st->glyph;
	} else {
		glyph = font->glyph;
		while (glyph->next) glyph = glyph->next;
		glyph->next = &st->glyph;
	}

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, svg_traverse_glyph);
}


void compositor_init_svg_font_face_uri(GF_Compositor *compositor, GF_Node *node)
{

}
#endif /*GPAC_DISABLE_SVG*/

