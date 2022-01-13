/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2012
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

#include <gpac/utf.h>

#ifndef GPAC_DISABLE_SVG

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
static GF_Err svg_font_get_glyphs(void *udta, const char *utf_string, u32 *glyph_buffer, u32 *io_glyph_buffer_size, const char *lang, Bool *is_rtl)
{
	u32 prev_c;
	u32 len;
	u32 i, gl_idx;
	u16 *utf_res;
	GF_Node *node = (GF_Node *)udta;
	GF_ChildNodeItem *child;
	char *utf8 = (char*) utf_string;

	/*FIXME - use glyphs unicode attributes for glyph substitution*/
	len = utf_string ? (u32) strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}

	if (*io_glyph_buffer_size < len+1) {
		*io_glyph_buffer_size = (u32) len+1;
		return GF_BUFFER_TOO_SMALL;
	}

	len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, (const char**)&utf8);
	if (len == GF_UTF8_FAIL) return GF_IO_ERR;
	/*should not happen*/
	if (utf8) return GF_IO_ERR;

	/*perform bidi relayout*/
	utf_res = (u16 *) glyph_buffer;
	*is_rtl = gf_utf8_reorder_bidi(utf_res, len);

	/*move 16bit buffer to 32bit*/
	for (i=len; i>0; i--) {
		glyph_buffer[i-1] = utf_res[i-1];
	}

	gl_idx = 0;
	prev_c = 0;
	for (i=0; i<len; i++) {
		SVG_GlyphStack *missing_glyph = NULL;
		SVG_GlyphStack *st = NULL;
		child = ((GF_ParentNode *) node)->children;
		while (child) {
			u32 tag = gf_node_get_tag(child->node);
			if (tag==TAG_SVG_missing_glyph) {
				missing_glyph = gf_node_get_private(child->node);
			} else if (tag ==TAG_SVG_glyph) {
				Bool glyph_ok = 0;
				SVGAllAttributes atts;

				st = gf_node_get_private(child->node);
				if (!st) {
					child = child->next;
					continue;
				}

				if (st->glyph.utf_name==glyph_buffer[i]) {
					u32 j, count;
					gf_svg_flatten_attributes((SVG_Element*)child->node, &atts);
					if (!lang) {
						glyph_ok = 1;
					} else {
						if (!atts.lang) {
							glyph_ok = 1;
						} else {
							count = gf_list_count(*atts.lang);
							for (j=0; j<count; j++) {
								char *name = gf_list_get(*atts.lang, j);
								if (!stricmp(name, lang) || strstr(lang, name)) {
									glyph_ok = 1;
									break;
								}
							}
						}
					}
					if (atts.arabic_form) {
						Bool first = (!prev_c || (prev_c==' ')) ? 1 : 0;
						Bool last = ((i+1==len) || (glyph_buffer[i+1]==' ') ) ? 1 : 0;
						if (!strcmp(*atts.arabic_form, "isolated")) {
							if (!first || !last) glyph_ok = 0;
						}
						if (!strcmp(*atts.arabic_form, "initial")) {
							if (!first) glyph_ok = 0;
						}
						if (!strcmp(*atts.arabic_form, "medial")) {
							if (first || last) glyph_ok = 0;
						}
						if (!strcmp(*atts.arabic_form, "terminal")) {
							if (!last) glyph_ok = 0;
						}
					}
					if (glyph_ok) break;
				}
				/*perform glyph substitution*/
				else if (st->uni_len>1) {
					u32 j;
					for (j=0; j<st->uni_len; j++) {
						if (i+j>=len) break;
						if (glyph_buffer[i+j] != st->unicode[j]) break;
					}
					if (j==st->uni_len)
						break;
				}
				st = NULL;
			}
			child = child->next;
		}
		prev_c = glyph_buffer[i];

		if (!st)
			st = missing_glyph;
		glyph_buffer[gl_idx] = st ? st->glyph.ID : 0;
		if (st && st->uni_len>1) i++;

		gl_idx++;
	}
	*io_glyph_buffer_size = /* len = */ gl_idx;

	return GF_OK;
}

/*loads glyph by name - returns NULL if glyph cannot be found*/
static GF_Glyph *svg_font_load_glyph(void *udta, u32 glyph_name)
{
	GF_ChildNodeItem *child = ((GF_ParentNode *) udta)->children;

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
			if (font->name) gf_free(font->name);
			gf_free(font);
		}
	}
}

static void svg_font_on_load(GF_Node *handler, GF_DOM_Event *event, GF_Node *observer)
{
	GF_Font *font;
	assert(event->currentTarget->ptr_type==GF_DOM_EVENT_TARGET_NODE);
	assert(gf_node_get_tag((GF_Node*)event->currentTarget->ptr)==TAG_SVG_font);
	font = gf_node_get_private((GF_Node*)event->currentTarget->ptr);
	font->not_loaded = 0;

	/*brute-force signaling that all fonts have changed and texts must be recomputed*/
	font->compositor->reset_fonts = 1;
	gf_sc_next_frame_state(font->compositor, GF_SC_DRAW_FRAME);
	font->compositor->fonts_pending--;
}

void compositor_init_svg_font(GF_Compositor *compositor, GF_Node *node)
{
	SVG_handlerElement *handler;
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
	if (!font) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg font\n"));
		return;
	}
	e = gf_font_manager_register_font(compositor->font_manager, font);
	if (e) {
		gf_free(font);
		return;
	}
	font->ft_mgr = compositor->font_manager;
	font->compositor = compositor;

	font->get_glyphs = svg_font_get_glyphs;
	font->load_glyph = svg_font_load_glyph;
	font->udta = node_font;
	gf_node_set_private(node_font, font);
	gf_node_set_callback_function(node_font, svg_traverse_font);
	font->name = gf_strdup(atts.font_family->value);

	font->em_size = atts.units_per_em ? FIX2INT( gf_ceil(atts.units_per_em->value) ) : 1000;
	/*Inconsistency between SVG 1.2 and 1.1
		when not specify, ascent and descent are computed based on font.vert-origin-y, WHICH DOES NOT EXIST
	IN Tiny 1.2 !!! We assume it to be 0.
	*/
	font->ascent = atts.ascent ? FIX2INT( gf_ceil(atts.ascent->value) ) : 0;
	if (!font->ascent) font->ascent = font->em_size;
	font->descent = atts.descent ? FIX2INT( gf_ceil(atts.descent->value) ) : 0;
	font->baseline = atts.alphabetic ? FIX2INT( gf_ceil(atts.alphabetic->value) ) : 0;
	font->line_spacing = font->em_size;
	font->styles = 0;
	if (atts.font_style) {
		switch (*atts.font_style) {
		case SVG_FONTSTYLE_ITALIC:
			font->styles |= GF_FONT_ITALIC;
			break;
		case SVG_FONTSTYLE_OBLIQUE:
			font->styles |= GF_FONT_OBLIQUE;
			break;
		}
	}
	if (atts.font_variant && (*atts.font_variant ==SVG_FONTVARIANT_SMALLCAPS))
		font->styles |= GF_FONT_SMALLCAPS;

	if (atts.font_weight) {
		switch(*atts.font_weight) {
		case SVG_FONTWEIGHT_100:
			font->styles |= GF_FONT_WEIGHT_100;
			break;
		case SVG_FONTWEIGHT_LIGHTER:
			font->styles |= GF_FONT_WEIGHT_LIGHTER;
			break;
		case SVG_FONTWEIGHT_200:
			font->styles |= GF_FONT_WEIGHT_200;
			break;
		case SVG_FONTWEIGHT_300:
			font->styles |= GF_FONT_WEIGHT_300;
			break;
		case SVG_FONTWEIGHT_400:
			font->styles |= GF_FONT_WEIGHT_400;
			break;
		case SVG_FONTWEIGHT_NORMAL:
			font->styles |= GF_FONT_WEIGHT_NORMAL;
			break;
		case SVG_FONTWEIGHT_500:
			font->styles |= GF_FONT_WEIGHT_500;
			break;
		case SVG_FONTWEIGHT_600:
			font->styles |= GF_FONT_WEIGHT_600;
			break;
		case SVG_FONTWEIGHT_700:
			font->styles |= GF_FONT_WEIGHT_700;
			break;
		case SVG_FONTWEIGHT_BOLD:
			font->styles |= GF_FONT_WEIGHT_BOLD;
			break;
		case SVG_FONTWEIGHT_800:
			font->styles |= GF_FONT_WEIGHT_800;
			break;
		case SVG_FONTWEIGHT_900:
			font->styles |= GF_FONT_WEIGHT_900;
			break;
		case SVG_FONTWEIGHT_BOLDER:
			font->styles |= GF_FONT_WEIGHT_BOLDER;
			break;
		}
	}

	gf_svg_flatten_attributes((SVG_Element*)node_font, &atts);
	font->max_advance_h = atts.horiz_adv_x ? FIX2INT( gf_ceil(atts.horiz_adv_x->value) ) : 0;

	font->not_loaded = 1;

	/*wait for onLoad event before activating the font, otherwise we may not have all the glyphs*/
	handler = gf_dom_listener_build(node_font, GF_EVENT_LOAD, 0);
	handler->handle_event = svg_font_on_load;
}


static void svg_traverse_glyph(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Font *font;
		GF_Glyph *prev_glyph, *a_glyph;
		SVG_GlyphStack *st = gf_node_get_private(node);
		if (st->unicode) gf_free(st->unicode);

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
		gf_free(st);
	}
}

void compositor_init_svg_glyph(GF_Compositor *compositor, GF_Node *node)
{
	u16 utf_name[20];
	u8 *utf8;
	u32 len;
	GF_Rect rc;
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
		if (!st) return;
		goto reg_common;
	}
	/*we must have unicode specified*/
	if (!atts.unicode) return;

	GF_SAFEALLOC(st, SVG_GlyphStack);
	if (!st) return;
	utf8 = (u8 *) *atts.unicode;
	len = gf_utf8_mbstowcs(utf_name, 200, (const char **) &utf8);
	if (len == GF_UTF8_FAIL) return;
	/*this is a single glyph*/
	if (len==1) {
		st->glyph.utf_name = utf_name[0];
		st->uni_len = 1;
	} else {
		st->glyph.utf_name = (u32) (PTR_TO_U_CAST st);
		st->unicode = gf_malloc(sizeof(u16)*len);
		st->uni_len = (u16) len;
		memcpy(st->unicode, utf_name, sizeof(u16)*len);
	}

reg_common:
	st->glyph.ID = (u32)(PTR_TO_U_CAST st);
	st->font = font;
	st->glyph.horiz_advance = font->max_advance_h;
	if (atts.horiz_adv_x) st->glyph.horiz_advance = FIX2INT( gf_ceil(atts.horiz_adv_x->value) );
	if (atts.d) {
		st->glyph.path = atts.d;
		gf_path_get_bounds(atts.d, &rc);
		st->glyph.width = FIX2INT( gf_ceil(rc.width) );
		st->glyph.height = FIX2INT( gf_ceil(rc.height) );
	}
	st->glyph.vert_advance = st->glyph.height;
	if (!st->glyph.vert_advance)
		st->glyph.vert_advance = font->max_advance_v;

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


typedef struct
{
	GF_Font *font;
	GF_Font *alias;
	GF_Compositor *compositor;
	GF_MediaObject *mo;
} FontURIStack;

static Bool svg_font_uri_check(GF_Node *node, FontURIStack *st)
{
	GF_Font *font;
	GF_Node *font_elt;
	SVGAllAttributes atts;
	gf_svg_flatten_attributes((SVG_Element*)node, &atts);
	if (!atts.xlink_href) return 0;

	if (atts.xlink_href->type == XMLRI_ELEMENTID) {
		if (!atts.xlink_href->target) atts.xlink_href->target = gf_sg_find_node_by_name(gf_node_get_graph(node), atts.xlink_href->string+1);
	} else {
		GF_SceneGraph *ext_sg;
		char *font_name = strchr(atts.xlink_href->string, '#');
		if (!font_name) return 0;
		if (!st->mo) {
			st->mo = gf_mo_load_xlink_resource(node, 0, 0, -1);
			if (!st->mo) {
				st->compositor->fonts_pending--;
				return 0;
			}
		}
		ext_sg = gf_mo_get_scenegraph(st->mo);
		if (!ext_sg) {
			st->compositor->fonts_pending--;
			return 0;
		}
		atts.xlink_href->target = gf_sg_find_node_by_name(ext_sg, font_name+1);
		if (!atts.xlink_href->target) {
			st->compositor->fonts_pending--;
			return 0;
		}
	}
	font_elt = atts.xlink_href->target;
	if (gf_node_get_tag(font_elt) != TAG_SVG_font) {
		st->compositor->fonts_pending--;
		return 0;
	}
	font = gf_node_get_private(font_elt);
	if (!font) {
		st->compositor->fonts_pending--;
		return 0;
	}

	st->alias = font;

	gf_mo_is_done(st->mo);
	font->not_loaded = 0;
	return 1;
}

GF_Font *svg_font_uri_get_alias(void *udta)
{
	GF_Node *node = (GF_Node *)udta;
	FontURIStack *st = gf_node_get_private(node);
	if (!st->alias && !svg_font_uri_check(node, st)) {
		return NULL;
	}
	return st->alias;
}

static void svg_traverse_font_face_uri(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		FontURIStack *st = gf_node_get_private(node);
		if (st) {
			gf_font_manager_unregister_font(st->font->ft_mgr, st->font);
			if (st->font->name) gf_free(st->font->name);
			gf_free(st->font);
			if (st->mo) gf_mo_unload_xlink_resource(node, st->mo);
			gf_free(st);
		}
	}
}

void compositor_init_svg_font_face_uri(GF_Compositor *compositor, GF_Node *node)
{
	GF_Node *par;
	GF_Font *font;
	FontURIStack *stack;
	GF_Err e;
	SVGAllAttributes atts;

	/*check parent is a font-face-src*/
	par = gf_node_get_parent(node, 0);
	if (!par || (gf_node_get_tag(par)!=TAG_SVG_font_face_src)) return;
	/*check parent's parent is a font-face*/
	par = gf_node_get_parent(par, 0);
	if (!par || (gf_node_get_tag(par)!=TAG_SVG_font_face)) return;


	gf_svg_flatten_attributes((SVG_Element*)node, &atts);
	if (!atts.xlink_href) return;

	/*get font familly*/
	gf_svg_flatten_attributes((SVG_Element*)par, &atts);
	if (!atts.font_family) return;

	/*if font with the same name exists, don't load*/
	if (gf_compositor_svg_set_font(compositor->font_manager, atts.font_family->value, 0, 1) != NULL)
		return;

	/*register font to font manager*/
	GF_SAFEALLOC(font, GF_Font);
	if (!font) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate font for svg font face URI\n"));
		return;
	}
	
	e = gf_font_manager_register_font(compositor->font_manager, font);
	if (e) {
		gf_free(font);
		return;
	}
	GF_SAFEALLOC(stack, FontURIStack);
	if (!stack) {
		gf_free(font);
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate svg font face URI stack\n"));
		return;
	}
	stack->font = font;
	stack->compositor = compositor;

	font->ft_mgr = compositor->font_manager;

	font->get_alias = svg_font_uri_get_alias;
	font->udta = node;
	font->name = gf_strdup(atts.font_family->value);
	gf_node_set_private(node, stack);
	gf_node_set_callback_function(node, svg_traverse_font_face_uri);

	font->not_loaded = 1;
	compositor->fonts_pending++;
	svg_font_uri_check(node, stack);
}


#endif /*GPAC_DISABLE_SVG*/

