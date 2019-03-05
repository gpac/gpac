/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include "nodes_stacks.h"
#include "texturing.h"

struct _gf_ft_mgr
{
	GF_FontReader *reader;

	GF_Font *font, *default_font;
	GF_Path *line_path;

	u32 *id_buffer;
	u32 id_buffer_size;

	Bool wait_font_load;
};

GF_EXPORT
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
	if (!font_mgr) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate font manager\n"));
		return NULL;
	}
	font_mgr->reader = ifce;
	font_mgr->id_buffer_size = 20;
	font_mgr->id_buffer = gf_malloc(sizeof(u32)*font_mgr->id_buffer_size);
	gf_font_manager_set_font(font_mgr, &def_font, 1, 0);
	font_mgr->default_font = font_mgr->font;

	font_mgr->line_path= gf_path_new();
	gf_path_add_move_to(font_mgr->line_path, -FIX_ONE/2, FIX_ONE/2);
	gf_path_add_line_to(font_mgr->line_path, FIX_ONE/2, FIX_ONE/2);
	gf_path_add_line_to(font_mgr->line_path, FIX_ONE/2, -FIX_ONE/2);
	gf_path_add_line_to(font_mgr->line_path, -FIX_ONE/2, -FIX_ONE/2);
	gf_path_close(font_mgr->line_path);

	opt = gf_cfg_get_key(user->config, "FontEngine", "WaitForFontLoad");
	if (!opt) gf_cfg_set_key(user->config, "FontEngine", "WaitForFontLoad", "no");
	if (opt && !strcmp(opt, "yes")) font_mgr->wait_font_load = 1;

	return font_mgr;
}

void gf_font_predestroy(GF_Font *font)
{
	if (font->spans) {
		while (gf_list_count(font->spans)) {
			GF_TextSpan *ts = gf_list_get(font->spans, 0);
			gf_list_rem(font->spans, 0);
			gf_node_dirty_set(ts->user, 0, 0);
			ts->user=NULL;
		}
		gf_list_del(font->spans);
		font->spans = NULL;
	}
}

void gf_font_del(GF_Font *font)
{
	gf_font_predestroy(font);
	if (!font->get_glyphs) {
		GF_Glyph *glyph;
		glyph = font->glyph;
		while (glyph) {
			GF_Glyph *next = glyph->next;
			gf_path_del(glyph->path);
			gf_free(glyph);
			glyph = next;
		}
	}
	gf_free(font->name);
	gf_free(font);
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
	gf_free(fm->id_buffer);
	gf_path_del(fm->line_path);
	gf_free(fm);
}

GF_Err gf_font_manager_register_font(GF_FontManager *fm, GF_Font *font)
{
	if (fm->font) {
		GF_Font *a_font = fm->font;
		while (a_font->next) a_font = a_font->next;
		a_font->next = font;
	} else {
		fm->font = font;
	}
	font->ft_mgr = fm;
	if (!font->spans) font->spans = gf_list_new();
	return GF_OK;
}

GF_Err gf_font_manager_unregister_font(GF_FontManager *fm, GF_Font *font)
{
	GF_Font *prev_font, *a_font;

	prev_font = NULL;
	a_font = fm->font;
	while (a_font) {
		if (a_font==font) break;
		prev_font = a_font;
		a_font = a_font->next;
	}
	if (prev_font) {
		prev_font->next = font->next;
	} else {
		fm->font = font->next;
	}
	gf_font_predestroy(font);
	return GF_OK;
}


GF_EXPORT
GF_Font *gf_font_manager_set_font_ex(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles, Bool check_only)
{
	u32 i;
	GF_Err e;
	Bool has_italic = (styles & GF_FONT_ITALIC) ? 1 : 0;
	Bool has_smallcaps = (styles & GF_FONT_SMALLCAPS) ? 1 : 0;
	GF_Font *the_font = NULL;

	for (i=0; i<nb_fonts; i++) {
		char *font_name;
		const char *opt;
		u32 weight_diff = 0xFFFFFFFF;
		GF_Font *best_font = NULL;
		GF_Font *font = fm->font;
		font_name = alt_fonts[i];

		if (!stricmp(font_name, "SERIF")) {
			opt = gf_modules_get_option((GF_BaseInterface *)fm->reader, "FontEngine", "FontSerif");
			if (opt) font_name = (char*)opt;
		}
		else if (!stricmp(font_name, "SANS") || !stricmp(font_name, "sans-serif")) {
			opt = gf_modules_get_option((GF_BaseInterface *)fm->reader, "FontEngine", "FontSans");
			if (opt) font_name = (char*)opt;
		}
		else if (!stricmp(font_name, "TYPEWRITER") || !stricmp(font_name, "monospace")) {
			opt = gf_modules_get_option((GF_BaseInterface *)fm->reader, "FontEngine", "FontFixed");
			if (opt) font_name = (char*)opt;
		}

		while (font) {
			if (fm->wait_font_load && font->not_loaded && !check_only && !stricmp(font->name, font_name)) {
				GF_Font *a_font = NULL;
				if (font->get_alias) a_font = font->get_alias(font->udta);
				if (!a_font || a_font->not_loaded)
					return font;
			}
			if ((check_only || !font->not_loaded) && font->name && !stricmp(font->name, font_name)) {
				s32 fw;
				s32 w;
				u32 diff;
				Bool ft_has_smallcaps;
				Bool ft_has_weight;

				if (check_only) return font;

				ft_has_weight = (font->styles & GF_FONT_WEIGHT_MASK) ? 1 : 0;
				if (font->styles == styles) {
					the_font = font;
					break;
				}
				/*check we have the same font variant*/
				ft_has_smallcaps = (font->styles & GF_FONT_SMALLCAPS) ? 1 : 0;
				if (ft_has_smallcaps != has_smallcaps) {
					font = font->next;
					continue;
				}
				/*check if we have an oblique/italic match*/
				if (has_italic) {
					if (! (font->styles & (GF_FONT_OBLIQUE|GF_FONT_ITALIC))) {
						font = font->next;
						continue;
					}
					/*if italic force it*/
					if (font->styles & GF_FONT_ITALIC) best_font = font;
					/*if oblic use it if no italic found*/
					else if (!best_font) best_font  = font;
					else {
						font = font->next;
						continue;
					}
				}

				/*compute min weight diff*/
				fw = font->styles>>10;
				w = styles>>10;
				diff = ABS(fw - w);
				if (ft_has_weight) {
					if (diff<weight_diff) {
						weight_diff = diff;
						best_font = font;
					}
				}
				/*no weight means "all"*/
				else {
					if ((font->styles & GF_FONT_STYLE_MASK) == (styles & GF_FONT_STYLE_MASK) ) {
						weight_diff = diff;
						best_font = font;
						font = font->next;
						continue;
					}
				}
			}
			font = font->next;
		}
		if (the_font) break;
		if (fm->reader) {
			e = fm->reader->set_font(fm->reader, font_name, styles);
			if (!e) {
				GF_SAFEALLOC(the_font, GF_Font);
				if (!the_font) {
					return NULL;
				}
				fm->reader->get_font_info(fm->reader, &the_font->name, &the_font->em_size, &the_font->ascent, &the_font->descent, &the_font->underline, &the_font->line_spacing, &the_font->max_advance_h, &the_font->max_advance_v);
				the_font->styles = styles;
				if (!the_font->name) the_font->name = gf_strdup(font_name);

				if (fm->font) {
					font = fm->font;
					while (font->next) font = font->next;
					font->next = the_font;
				} else {
					fm->font = the_font;
				}
				the_font->ft_mgr = fm;
				return the_font;
			}
		}
		if (best_font) {
			the_font = best_font;
			break;
		}
	}

	/*check for font alias*/
	if (the_font && the_font->get_alias) {
		return the_font->get_alias(the_font->udta);

	}

	if (!the_font) {
		if (check_only) return NULL;
		the_font = fm->default_font;
	}
	/*embeded font*/
	if (fm->reader && the_font && !the_font->get_glyphs)
		fm->reader->set_font(fm->reader, the_font->name, the_font->styles);

	return the_font;
}
GF_Font *gf_font_manager_set_font(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles)
{
	return gf_font_manager_set_font_ex(fm, alt_fonts, nb_fonts, styles, 0);
}

static GF_Glyph *gf_font_get_glyph(GF_FontManager *fm, GF_Font *font, u32 name)
{
	GF_Glyph *glyph = font->glyph;
	while (glyph) {
		if (glyph->ID==name) return glyph;
		glyph = glyph->next;
	}

	if (name==GF_CARET_CHAR) {
		GF_SAFEALLOC(glyph, GF_Glyph);
		if (!glyph) return NULL;
		glyph->height = font->ascent;
		glyph->horiz_advance = 0;
		glyph->width = 0;
		glyph->ID = GF_CARET_CHAR;
		glyph->path = gf_path_new();
		gf_path_add_move_to(glyph->path, 0, INT2FIX(font->descent));
		gf_path_add_line_to(glyph->path, 0, INT2FIX(font->ascent));
		gf_path_add_line_to(glyph->path, 1, INT2FIX(font->ascent));
		gf_path_add_line_to(glyph->path, 1, INT2FIX(font->descent));
		gf_path_close(glyph->path);
		glyph->utf_name=0;
	} else if (name==(u32) '\n') {
		GF_SAFEALLOC(glyph, GF_Glyph);
		if (!glyph) return NULL;
		glyph->height = font->ascent;
		glyph->horiz_advance = 0;
		glyph->width = 0;
		glyph->ID = name;
		glyph->utf_name=name;
	} else if (name==(u32) '\t') {
		return NULL;
	} else {
		/*load glyph*/
		if (font->load_glyph) {
			glyph = font->load_glyph(font->udta, name);
		} else {
			if (!fm->reader) return NULL;
			//		fm->reader->set_font(fm->reader, font->name, font->styles);
			glyph = fm->reader->load_glyph(fm->reader, name);
		}
	}
	if (!glyph) return NULL;

	if (!font->glyph) font->glyph = glyph;
	else {
		GF_Glyph *a_glyph = font->glyph;
		while (a_glyph->next) a_glyph = a_glyph->next;
		a_glyph->next = glyph;
	}
	/*space character - this may need adjustment for other empty glyphs*/
	if (glyph->path && !glyph->path->n_points) {
		glyph->path->bbox.x = 0;
		glyph->path->bbox.width = INT2FIX(font->max_advance_h);
		glyph->path->bbox.y = INT2FIX(font->ascent);
		glyph->path->bbox.height = INT2FIX(font->ascent - font->descent);
	}
	return glyph;
}

GF_EXPORT
GF_TextSpan *gf_font_manager_create_span(GF_FontManager *fm, GF_Font *font, char *text, Fixed font_size, Bool needs_x_offset, Bool needs_y_offset, Bool needs_rotate, const char *xml_lang, Bool fliped_text, u32 styles, GF_Node *user)
{
	GF_Err e;
	Bool is_rtl;
	u32 len, i;
	GF_TextSpan *span;

	if (!strlen(text)) return NULL;

	len = fm->id_buffer_size;
	if (font->get_glyphs)
		e = font->get_glyphs(font->udta, text, fm->id_buffer, &len, xml_lang, &is_rtl);
	else
		e = fm->reader->get_glyphs(fm->reader, text, fm->id_buffer, &len, xml_lang, &is_rtl);

	if (e==GF_BUFFER_TOO_SMALL) {
		fm->id_buffer_size = len;
		fm->id_buffer = gf_realloc(fm->id_buffer, sizeof(u32) * len);
		if (!fm->id_buffer) return NULL;

		if (font->get_glyphs)
			e = font->get_glyphs(font->udta, text, fm->id_buffer, &len, xml_lang, &is_rtl);
		else
			e = fm->reader->get_glyphs(fm->reader, text, fm->id_buffer, &len, xml_lang, &is_rtl);
	}
	if (e) return NULL;

	GF_SAFEALLOC(span, GF_TextSpan);
	if (!span) return NULL;
	span->font = font;
	span->font_size = font_size;
	if (font->em_size)
		span->font_scale = font_size / font->em_size;
	span->x_scale = span->y_scale = FIX_ONE;
//	span->lang = xml_lang;
	if (fliped_text) span->flags |= GF_TEXT_SPAN_FLIP;
	if (styles & GF_FONT_UNDERLINED) span->flags |= GF_TEXT_SPAN_UNDERLINE;
	span->nb_glyphs = len;
	span->glyphs = gf_malloc(sizeof(void *)*len);
	if (needs_x_offset) {
		span->dx = gf_malloc(sizeof(Fixed)*len);
		memset(span->dx, 0, sizeof(Fixed)*len);
	}
	if (needs_y_offset) {
		span->dy = gf_malloc(sizeof(Fixed)*len);
		memset(span->dy, 0, sizeof(Fixed)*len);
	}
	if (needs_rotate) {
		span->rot = gf_malloc(sizeof(Fixed)*len);
		memset(span->rot, 0, sizeof(Fixed)*len);
	}

	for (i=0; i<len; i++) {
		span->glyphs[i] = gf_font_get_glyph(fm, font, fm->id_buffer[i]);
	}
	span->user = user;
	if (span->font->spans)
		gf_list_add(font->spans, span);
	if (is_rtl) span->flags |= GF_TEXT_SPAN_RIGHT_TO_LEFT;
	return span;
}



typedef struct _span_internal
{
	/*zoom when texture was computed*/
	Fixed last_zoom;
	/*texture handler for bitmap text*/
	GF_TextureHandler *txh;
	/*texture path (rectangle)*/
	GF_Path *path;

#ifndef GPAC_DISABLE_3D
	/*span mesh (built out of the # glyphs)*/
	GF_Mesh *mesh;
	/*span texture mesh (rectangle)*/
	GF_Mesh *tx_mesh;
	/*span outline*/
	GF_Mesh *outline;
#endif
} GF_SpanExtensions;


void gf_font_manager_delete_span(GF_FontManager *fm, GF_TextSpan *span)
{
	if (span->user && span->font->spans) gf_list_del_item(span->font->spans, span);

	gf_free(span->glyphs);
	if (span->dx) gf_free(span->dx);
	if (span->dy) gf_free(span->dy);
	if (span->rot) gf_free(span->rot);

	if (span->ext) {
		if (span->ext->path) gf_path_del(span->ext->path);
#ifndef GPAC_DISABLE_3D
		if (span->ext->mesh) mesh_free(span->ext->mesh);
		if (span->ext->tx_mesh) mesh_free(span->ext->tx_mesh);
		if (span->ext->outline) mesh_free(span->ext->outline);
#endif
		if (span->ext->txh) {
			gf_sc_texture_destroy(span->ext->txh);
			if (span->ext->txh->data) gf_free(span->ext->txh->data);
			gf_free(span->ext->txh);
		}
		gf_free(span->ext);
	}
	gf_free(span);
}

GF_EXPORT
void gf_font_manager_refresh_span_bounds(GF_TextSpan *span)
{
	u32 i;
	Fixed descent, ascent, bline;
	Fixed min_x, min_y, max_y;

	if (!span->nb_glyphs) {
		span->bounds.width = span->bounds.height = 0;
		return;
	}
	descent = 0;
	if (span->font->descent<0) descent = -span->font_scale * span->font->descent;
	ascent = span->font->ascent * span->font_scale;

	/*if fliped text (SVG), the min_y is at the ascent side*/
	if (span->flags & GF_TEXT_SPAN_FLIP) {
		Fixed tmp = ascent;
		ascent = descent;
		descent = tmp;
	}

	bline = span->font->baseline * span->font_scale;

	min_x = span->dx ? FIX_MAX : span->off_x;
	min_y = span->dy ? FIX_MAX : span->off_y - descent;
	max_y = span->dy ? -FIX_MAX : span->off_y + ascent;

	/*adjust start_x for first glyph*/
	if (span->glyphs[0] && span->glyphs[0]->path) {
		min_x += gf_mulfix(span->glyphs[0]->path->bbox.x, span->font_scale);
	}
	span->bounds = gf_rect_center(0, 0);

	for (i=0; i<span->nb_glyphs; i++) {
		Fixed g_width;
		GF_Rect rc;

		/*compute glyph size*/
		if (!span->glyphs[i]) g_width = span->font->max_advance_h * span->font_scale;
		/*if last glyph of the span, increase by width only*/
//		else if (i+1==span->nb_glyphs) g_width = span->glyphs[i]->width * span->font_scale;
		/*otherwise increase by the horizontal advance*/
		else g_width = span->glyphs[i]->horiz_advance * span->font_scale;

		if (span->dy) {
			if (span->dy[i] - descent < min_y) min_y = span->dy[i] - descent;
			if (span->dy[i] + ascent > max_y) max_y = span->dy[i] + ascent;
		}
		else if (span->glyphs[i]) {
			Fixed size = span->glyphs[i]->height * span->font_scale;
			if (size > max_y-min_y) max_y = size + min_y;
		}

		if (span->dx) {
			rc.x = span->dx[i];
		} else {
			rc.x = min_x;
			if (span->flags & GF_TEXT_SPAN_HORIZONTAL)
				min_x += g_width;
		}
		rc.y = span->dy ? span->dy[i] + ascent : max_y;
		rc.width = g_width;
		rc.height = max_y - min_y;

		if (span->rot) {
			GF_Matrix2D tr;
			gf_mx2d_init(tr);
			gf_mx2d_add_rotation(&tr, rc.x, rc.y - ascent - bline, span->rot[i]);
			gf_mx2d_apply_rect(&tr, &rc);
		}
		gf_rect_union(&span->bounds, &rc);
	}
}




GF_Path *gf_font_span_create_path(GF_TextSpan *span)
{
	u32 i;
	GF_Matrix2D mat;
	Fixed dx, dy;
	GF_Path *path = gf_path_new();

	gf_mx2d_init(mat);
	mat.m[0] = gf_mulfix(span->font_scale, span->x_scale);
	mat.m[4] = gf_mulfix(span->font_scale, span->y_scale);
	if (span->flags & GF_TEXT_SPAN_FLIP) gf_mx2d_add_scale(&mat, FIX_ONE, -FIX_ONE);

	dx = gf_divfix(span->off_x, mat.m[0]);
	dy = gf_divfix(span->off_y, mat.m[4]);

	for (i=0; i<span->nb_glyphs; i++) {
		if (!span->glyphs[i]) {
			if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
				dx += INT2FIX(span->font->max_advance_h);
			} else {
				dy -= INT2FIX(span->font->max_advance_v);
			}
		} else {
			if (span->dx) dx = gf_divfix(span->dx[i], mat.m[0]);
			if (span->dy) dy = gf_divfix(span->dy[i], mat.m[4]);


			if (span->glyphs[i]->utf_name != ' ') {
				GF_Matrix2D mx;
				gf_mx2d_init(mx);
				if (span->rot) {
					gf_mx2d_add_rotation(&mx, 0, 0, -span->rot[i]);
				}
				if (span->glyphs[i]->ID==GF_CARET_CHAR) {
					gf_mx2d_add_scale(&mx, mat.m[0], FIX_ONE);
				}
				gf_mx2d_add_translation(&mx, dx, dy);
				gf_path_add_subpath(path, span->glyphs[i]->path, &mx);
			}

			if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
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
	span->ext->tx_mesh = new_mesh();
	mesh_set_vertex(span->ext->tx_mesh, span->bounds.x, span->bounds.y-span->bounds.height, 0, 0, 0, FIX_ONE, 0, FIX_ONE);
	mesh_set_vertex(span->ext->tx_mesh, span->bounds.x+span->bounds.width, span->bounds.y-span->bounds.height, 0, 0, 0, FIX_ONE, FIX_ONE, FIX_ONE);
	mesh_set_vertex(span->ext->tx_mesh, span->bounds.x+span->bounds.width, span->bounds.y, 0, 0, 0, FIX_ONE, FIX_ONE, 0);
	mesh_set_vertex(span->ext->tx_mesh, span->bounds.x, span->bounds.y, 0, 0, 0, FIX_ONE, 0, 0);
	mesh_set_triangle(span->ext->tx_mesh, 0, 1, 2);
	mesh_set_triangle(span->ext->tx_mesh, 0, 2, 3);
	span->ext->tx_mesh->flags |= MESH_IS_2D;
	span->ext->tx_mesh->mesh_type = MESH_TRIANGLES;
	mesh_update_bounds(span->ext->tx_mesh);
}
#endif

/*don't build too large textures*/
#define MAX_TX_SIZE		512
/*and don't build too small ones otherwise result is as crap as non-textured*/
#define MIN_TX_SIZE		32

static Bool span_setup_texture(GF_Compositor *compositor, GF_TextSpan *span, Bool for_3d, GF_TraverseState *tr_state)
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
			if (for_3d && !span->ext->tx_mesh) span_build_mesh(span);
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
	if (!tr_state->pixel_metrics) {
		scale = gf_mulfix(scale, tr_state->min_hsize);
	}
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
	if (span->ext->tx_mesh) mesh_free(span->ext->tx_mesh);
	span->ext->tx_mesh = NULL;
#endif

	if (span->ext->txh) {
		gf_sc_texture_destroy(span->ext->txh);
		if (span->ext->txh->data) gf_free(span->ext->txh->data);
		gf_free(span->ext->txh);
	}
	GF_SAFEALLOC(span->ext->txh, GF_TextureHandler);
	if (!span->ext->txh) return 0;
	gf_sc_texture_setup(span->ext->txh, compositor, NULL);
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
	span->ext->txh->data = (char *) gf_malloc(sizeof(char)*span->ext->txh->stride*span->ext->txh->height);
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
	span_path = gf_font_span_create_path(span);
	raster->surface_set_path(surface, span_path);

	raster->surface_fill(surface, brush);
	raster->stencil_delete(brush);
	raster->surface_delete(surface);
	gf_path_del(span_path);

	if (span->font->baseline) {
		Fixed dy = gf_mulfix(span->font->baseline, span->font_scale);
		bounds.y += dy;
		span->bounds.y += dy;
	}
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

static void span_fill_3d(GF_TextSpan *span, GF_TraverseState *tr_state)
{
	span_alloc_extensions(span);
	if (!span->ext->mesh) {
		GF_Path *path = gf_font_span_create_path(span);
		span->ext->mesh = new_mesh();
		mesh_from_path(span->ext->mesh, path);
		gf_path_del(path);
	}

	visual_3d_mesh_paint(tr_state, span->ext->mesh);
}

static void span_strike_3d(GF_TextSpan *span, GF_TraverseState *tr_state, DrawAspect2D *asp, Bool vect_outline)
{
	span_alloc_extensions(span);
	if (!span->ext->outline) {
		GF_Path *path = gf_font_span_create_path(span);
		span->ext->outline = new_mesh();
#ifdef GPAC_HAS_GLU
		if (vect_outline) {
			GF_Path *outline = gf_path_get_outline(path, asp->pen_props);
			gf_mesh_tesselate_path(span->ext->outline, outline, asp->line_texture ? 2 : 1);
			gf_path_del(outline);
		} else {
			mesh_get_outline(span->ext->outline, path);
		}
#else
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


void gf_font_spans_draw_3d(GF_List *spans, GF_TraverseState *tr_state, DrawAspect2D *asp, u32 text_hl, Bool force_texturing)
{
	u32 i;
	SFColorRGBA hl_color;
	GF_TextSpan *span;
	Bool fill_2d, vect_outline, can_texture_text;
	GF_Compositor *compositor = (GF_Compositor*)tr_state->visual->compositor;

	vect_outline = !compositor->raster_outlines;

	visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);

	fill_2d = 0;
	if (!asp) {
		if (!visual_3d_setup_appearance(tr_state)) return;
	} else {
		fill_2d = (asp->fill_color) ? 1 : 0;
	}
	memset(&hl_color, 0, sizeof(SFColorRGBA));

	if (text_hl && (fill_2d || !asp) ) {
		/*reverse video: highlighting uses the text color, and text color is inverted (except alpha channel)
		the ideal impl would be to use the background color for the text, but since the text may be
		displayed over anything non uniform this would require clipping the highlight rect with the text
		which is too onerous (and not supported anyway) */
		if (text_hl == 0x00FFFFFF) {
			if (!asp) {
#ifndef GPAC_DISABLE_VRML
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
				} else
#endif /*GPAC_DISABLE_VRML*/
				{
					hl_color.red = hl_color.green = hl_color.blue = 0;
					hl_color.alpha = FIX_ONE;
				}
			} else {
				hl_color.alpha = FIX_ONE;
				hl_color.red = INT2FIX( GF_COL_R(asp->fill_color) ) / 255;
				hl_color.green = INT2FIX( GF_COL_G(asp->fill_color) ) / 255;
				hl_color.blue = INT2FIX( GF_COL_B(asp->fill_color) ) / 255;
				if (GF_COL_A(asp->fill_color) ) {
					u8 r = GF_COL_R(asp->fill_color);
					u8 g = GF_COL_G(asp->fill_color);
					u8 b = GF_COL_B(asp->fill_color);
					asp->fill_color = GF_COL_ARGB(GF_COL_A(asp->fill_color), 255-r, 255-g, 255-b);
				}
			}
		} else {
			hl_color.red = INT2FIX(GF_COL_R(text_hl)) / 255;
			hl_color.green = INT2FIX(GF_COL_G(text_hl)) / 255;
			hl_color.blue = INT2FIX(GF_COL_B(text_hl)) / 255;
			hl_color.alpha = INT2FIX(GF_COL_A(text_hl)) / 255;
		}
		if (asp && !asp->fill_color) text_hl = 0;
	}

	/*setup texture*/
	visual_3d_setup_texture(tr_state, FIX_ONE);
	can_texture_text = 0;
	if (fill_2d || !asp) {
		/*check if we can use text texturing*/
		if (force_texturing || (compositor->texture_text_mode != GF_TEXTURE_TEXT_NEVER) ) {
			if (fill_2d && asp->pen_props.width) {
				can_texture_text = 0;
			} else {
				can_texture_text = tr_state->mesh_num_textures ? 0 : 1;
			}
		}
	}

	visual_3d_enable_antialias(tr_state->visual, compositor->antiAlias);
	if (fill_2d || !asp || tr_state->mesh_num_textures) {
		if (fill_2d) visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);

		i = tr_state->text_split_idx ? tr_state->text_split_idx-1 : 0;
		while ((span = (GF_TextSpan *)gf_list_enum(spans, &i))) {
			if (text_hl) {
				visual_3d_fill_rect(tr_state->visual, span->bounds, hl_color);

				if (fill_2d)
					visual_3d_set_material_2d_argb(tr_state->visual, asp->fill_color);
				else
					visual_3d_setup_appearance(tr_state);
			}

			if (can_texture_text && span_setup_texture(tr_state->visual->compositor, span, 1, tr_state)) {
				tr_state->mesh_num_textures = gf_sc_texture_enable(span->ext->txh, NULL);
				if (tr_state->mesh_num_textures) {
					Bool has_mat_2d = tr_state->visual->compositor->visual->has_material_2d;
					visual_3d_mesh_paint(tr_state, span->ext->tx_mesh);
					gf_sc_texture_disable(span->ext->txh);
					tr_state->mesh_num_textures = 0;
					tr_state->visual->has_material_2d = has_mat_2d;
				}
			} else {
				span_fill_3d(span, tr_state);
			}

			if (tr_state->text_split_idx) break;
		}
		tr_state->visual->has_material_2d = 0;

		/*reset texturing in case of line texture*/
		if (!asp) visual_3d_disable_texture(tr_state);
	}
	visual_3d_set_state(tr_state->visual, V3D_STATE_BLEND, 0);

	if (asp && asp->pen_props.width) {
		if (!asp->line_scale) {
			drawable_compute_line_scale(tr_state, asp);
		}
		asp->pen_props.width = gf_divfix(asp->pen_props.width, asp->line_scale);
		visual_3d_set_2d_strike(tr_state, asp);

		if (tr_state->text_split_idx) {
			span = (GF_TextSpan *)gf_list_get(spans, tr_state->text_split_idx-1);
			span_strike_3d(span, tr_state, asp, vect_outline);
		} else {
			i=0;
			while ((span = (GF_TextSpan *)gf_list_enum(spans, &i))) {
				span_strike_3d(span, tr_state, asp, vect_outline);
			}
		}
	}
}


#endif

static void gf_font_span_draw_2d(GF_TraverseState *tr_state, GF_TextSpan *span, DrawableContext *ctx, GF_Rect *bounds)
{
	u32 flags, i;
	Bool flip_text;
	Fixed dx, dy, sx, sy, lscale, bline;
	Bool needs_texture = (ctx->aspect.fill_texture || ctx->aspect.line_texture) ? 1 : 0;
	GF_Matrix2D mx, tx;

	gf_mx2d_copy(mx, ctx->transform);

	flags = ctx->flags;
	dx = span->off_x;
	dy = span->off_y;
	sx = gf_mulfix(span->font_scale, span->x_scale);
	sy = gf_mulfix(span->font_scale, span->y_scale);

	flip_text = (ctx->flags & CTX_FLIPED_COORDS) ? tr_state->visual->center_coords : !tr_state->visual->center_coords;

	bline = span->font->baseline*span->font_scale;
	lscale = ctx->aspect.line_scale;
	ctx->aspect.line_scale = gf_divfix(ctx->aspect.line_scale, span->font_scale);

	for (i=0; i<span->nb_glyphs; i++) {
		if (!span->glyphs[i]) {
			if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
				dx += sx * span->font->max_advance_h;
			} else {
				dy -= sy * span->font->max_advance_v;
			}
			continue;
		}
		if (span->glyphs[i]->ID==GF_CARET_CHAR) {
			if (tr_state->visual->compositor->show_caret) {
				ctx->transform.m[0] = FIX_ONE;
			} else {
				continue;
			}
		} else {
			ctx->transform.m[0] = sx;
		}
		ctx->transform.m[4] = flip_text  ? -sy : sy;
		ctx->transform.m[1] = ctx->transform.m[3] = 0;
		ctx->transform.m[2] = span->dx ? span->dx[i] : dx;
		ctx->transform.m[5] = span->dy ? span->dy[i] : dy;
		ctx->transform.m[5] += bline;
		if (span->rot) {
			gf_mx2d_add_rotation(&ctx->transform, ctx->transform.m[2], ctx->transform.m[5], span->rot[i]);
		}

		gf_mx2d_add_matrix(&ctx->transform, &mx);

		if (needs_texture) {
			tx.m[0] = sx;
			tx.m[4] = sy;
			tx.m[1] = tx.m[3] = 0;
			tx.m[2] = span->dx ? span->dx[i] : dx;
			tx.m[5] = span->dy ? span->dy[i] : dy;
			tx.m[5] += bline;
			if (span->rot) {
				gf_mx2d_add_rotation(&tx, tx.m[2], tx.m[5], span->rot[i]);
			}
			gf_mx2d_inverse(&tx);

			visual_2d_texture_path_extended(tr_state->visual, span->glyphs[i]->path, ctx->aspect.fill_texture, ctx, bounds ? bounds : &span->bounds, &tx, tr_state);
			visual_2d_draw_path_extended(tr_state->visual, span->glyphs[i]->path, ctx, NULL, NULL, tr_state, bounds ? bounds : &span->bounds, &tx, GF_FALSE);
		} else {
			visual_2d_draw_path(tr_state->visual, span->glyphs[i]->path, ctx, NULL, NULL, tr_state);
		}
		ctx->flags = flags;

		if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
			dx += sx * span->glyphs[i]->horiz_advance;
		} else {
			dy -= sy * span->glyphs[i]->vert_advance;
		}
	}
	gf_mx2d_copy(ctx->transform, mx);
	ctx->aspect.line_scale = lscale;
}

void gf_font_underline_span(GF_TraverseState *tr_state, GF_TextSpan *span, DrawableContext *ctx)
{
	GF_Matrix2D mx, m;
	u32 col;
	Fixed sx, width, diff;
	if (span->dx || span->dy) return;

	gf_mx2d_copy(mx, ctx->transform);
	sx = gf_mulfix(span->font_scale, span->x_scale);

	if (span->flags & GF_TEXT_SPAN_FLIP)
		diff = sx * (span->font->descent - span->font->underline);
	else
		diff = sx * (- span->font->ascent + span->font->underline);

	gf_mx2d_init(m);
	gf_mx2d_add_scale(&m, span->bounds.width, FIX_ONE);
	gf_mx2d_add_translation(&m, span->bounds.x + span->bounds.width / 2, span->bounds.y+diff);
	gf_mx2d_pre_multiply(&ctx->transform, &m);

	col = ctx->aspect.fill_color;
	width = ctx->aspect.pen_props.width;
	ctx->aspect.pen_props.width = 0;
	ctx->flags &= ~CTX_PATH_FILLED;
	if (span->anchor) ctx->aspect.fill_color = 0xFF0000FF;

	visual_2d_draw_path(tr_state->visual, span->font->ft_mgr->line_path, ctx, NULL, NULL, tr_state);
	ctx->aspect.fill_color = col;

	gf_mx2d_copy(ctx->transform, mx);
	ctx->aspect.pen_props.width = width;
}

#if 0
static u32 col_reverse_video(u32 col)
{
	u32 a, r, g, b;
	a = GF_COL_A(col);
	r = GF_COL_R(col);
	g = GF_COL_G(col);
	b = GF_COL_B(col);
	return GF_COL_ARGB(a, 255-r, 255-g, 255-b);
}
#endif

static GF_Rect font_get_sel_rect(GF_TraverseState *tr_state)
{
	GF_Vec s, e;
	GF_Rect rc;
	GF_Compositor *compositor = tr_state->visual->compositor;

	e.x = compositor->end_sel.x;
	e.y = compositor->end_sel.y;
	e.z = 0;
	gf_mx_apply_vec(&compositor->hit_local_to_world, &e);

	s.x = compositor->start_sel.x;
	s.y = compositor->start_sel.y;
	s.z = 0;
	gf_mx_apply_vec(&compositor->hit_local_to_world, &s);

	if (s.x > e.x) {
		rc.x = e.x;
		rc.width = s.x - e.x;
	} else {
		rc.x = s.x;
		rc.width = e.x - s.x;
	}
	if (s.y > e.y) {
		rc.y = s.y;
		rc.height = s.y - e.y;
	} else {
		rc.y = e.y;
		rc.height = e.y - s.y;
	}
	if (!rc.height) rc.height = FIX_ONE;
	return rc;
}

static void gf_font_spans_select(GF_TextSpan *span, GF_TraverseState *tr_state, DrawableContext *ctx, Bool has_more_spans, Bool first_span, GF_Rect *rc)
{
	GF_Matrix2D mx;
	u32 flags, i, color;
	Bool has_selection = 0;
	Fixed dx, dy, sx, sy, width, ascent, descent;
	GF_Compositor *compositor = tr_state->visual->compositor;

	if (first_span) rc->width = 0;

	if (!(span->flags & GF_TEXT_SPAN_SELECTED) ) return;


	dx = gf_mulfix(span->off_x, span->x_scale);
	dy = gf_mulfix(span->off_y, span->y_scale);
	sx = gf_mulfix(span->font_scale, span->x_scale);
	sy = gf_mulfix(span->font_scale, span->y_scale);
	ascent = span->font->ascent*sy;
	descent = span->font->descent*sy;

	width = 0;
	flags = 0;
	if (ctx) {
		width = ctx->aspect.pen_props.width;
		ctx->aspect.pen_props.width = 0;
		flags = ctx->flags;
		gf_mx2d_copy(mx, ctx->transform);
	}

	/*compute sel rectangle*/
	if (!rc->width) {
		*rc = font_get_sel_rect(tr_state);
	}

	color = compositor->text_sel_color;

	for (i=0; i<span->nb_glyphs; i++) {
		GF_Rect g_rc;
		Bool end_of_line = 0;
		Fixed advance;
		if (!span->glyphs[i]) continue;
		advance = sx * span->glyphs[i]->horiz_advance;
		if (span->dx) dx = span->dx[i];
		if (span->dy) dy = span->dy[i];
		if (dx + advance/2 < rc->x) {
			dx += advance;
			continue;
		}

		g_rc.height = ascent-descent;

		if (dx + advance/2 > rc->x + rc->width) {
			u32 j;
			Bool has_several_lines = 0;
			if (!span->dy) break;
			if (!has_selection) continue;

			for (j=i+1; j<span->nb_glyphs; j++) {
				if (span->dy[j] > span->dy[i]) {
					if (span->flags & GF_TEXT_SPAN_FLIP)
						g_rc.y = span->dy[j]-descent;
					else
						g_rc.y = span->dy[j]+ascent;
					g_rc.x = span->bounds.x;
					g_rc.width = span->bounds.width;
					if (gf_rect_overlaps(g_rc, *rc)) {
						has_several_lines =1;
						break;
					}
				}
			}
			if (has_more_spans && dy<rc->y) has_several_lines =1;

			if (!has_several_lines) break;
			end_of_line = 1;
			/*move selection rect to include start of line - FIXME this depends on ltr/rtl*/
			rc->width = rc->width+rc->x - span->bounds.x;
			rc->x = span->bounds.x;
		}

		if (span->flags & GF_TEXT_SPAN_FLIP)
			g_rc.y = dy-descent;
		else
			g_rc.y = dy+ascent;
		g_rc.x = dx;
		g_rc.width = advance;


		if (!end_of_line && !gf_rect_overlaps(g_rc, *rc))
			continue;
		/*
				if (dy < rc.y-rc.height) {
					if (!span->dx) break;
					continue;
				}
		*/
		has_selection = 1;
		if (ctx) {
			g_rc.width+=2;
			if (span->rot) {
				gf_mx2d_init(ctx->transform);
				gf_mx2d_add_rotation(&ctx->transform, g_rc.x , g_rc.y, span->rot[i]);
				gf_mx2d_add_matrix(&ctx->transform, &mx);
			}
			visual_2d_fill_rect(tr_state->visual, ctx, &g_rc, color, 0, tr_state);
			ctx->flags = flags;
			compositor->store_text_state = GF_SC_TSEL_ACTIVE;
		} else {
			if (!compositor->sel_buffer_alloc || compositor->sel_buffer_len == compositor->sel_buffer_alloc) {
				if (!compositor->sel_buffer_alloc) compositor->sel_buffer_alloc ++;
				compositor->sel_buffer_alloc = 2*compositor->sel_buffer_alloc;
				compositor->sel_buffer = gf_realloc(compositor->sel_buffer, sizeof(u16)*compositor->sel_buffer_alloc);
			}
			compositor->sel_buffer[compositor->sel_buffer_len] = (u16) span->glyphs[i]->utf_name;
			compositor->sel_buffer_len++;
		}

		dx += advance;
	}
	if (ctx) {
		gf_mx2d_copy(ctx->transform, mx);
		ctx->aspect.pen_props.width = width;
	}
	if (has_selection && has_more_spans && dy<rc->y) {
		rc->width = rc->width+rc->x - span->bounds.x;
		rc->x = span->bounds.x;
	}
}

void gf_font_spans_get_selection(GF_Node *node, GF_List *spans, GF_TraverseState *tr_state)
{
	GF_TextSpan *span;
	u32 i, count;
	GF_Rect rc;
	count = gf_list_count(spans);
	for (i=0; i<count; i++) {
		span = (GF_TextSpan *)gf_list_get(spans, i);
		gf_font_spans_select(span, tr_state, NULL, (i+1<count) ? 1 : 0, (i==0) ? 1 : 0, &rc);
	}
}


void gf_font_spans_draw_2d(GF_List *spans, GF_TraverseState *tr_state, u32 hl_color, Bool force_texture_text, GF_Rect *bounds)
{
	Bool use_texture_text, is_rv;
	GF_Compositor *compositor = tr_state->visual->compositor;
	u32 i, count;
	GF_TextSpan *span;
	DrawableContext *ctx = tr_state->ctx;
	GF_Rect rc;

	use_texture_text = 0;
	if (force_texture_text || (compositor->texture_text_mode==GF_TEXTURE_TEXT_ALWAYS) ) {
		use_texture_text = !ctx->aspect.fill_texture && !ctx->aspect.pen_props.width;
	}

	is_rv = 0;
	if (hl_color) {
		/*reverse video: highlighting uses the text color, and text color is inverted (except alpha channel)
		the ideal impl would be to use the background color for the text, but since the text may be
		displayed over anything non uniform this would require clipping the highlight rect with the text
		which is too onerous (and not supported anyway) */
		if (hl_color==0x00FFFFFF) {
			u32 a, r, g, b;
			hl_color = tr_state->ctx->aspect.fill_color;

			a = GF_COL_A(tr_state->ctx->aspect.fill_color);
			if (a) {
				r = GF_COL_R(tr_state->ctx->aspect.fill_color);
				g = GF_COL_G(tr_state->ctx->aspect.fill_color);
				b = GF_COL_B(tr_state->ctx->aspect.fill_color);
				tr_state->ctx->aspect.fill_color = GF_COL_ARGB(a, 255-r, 255-g, 255-b);
			}
			is_rv = 1;
		}
		if (GF_COL_A(hl_color) == 0) hl_color = 0;
	}

	count = gf_list_count(spans);
	i=ctx->sub_path_index ? ctx->sub_path_index-1 : 0;
	for(; i<count; i++) {
		span = (GF_TextSpan *)gf_list_get(spans, i);
		if (compositor->text_selection)
			gf_font_spans_select(span, tr_state, ctx, (i+1<count) ? 1 : 0, (i==0)? GF_TRUE : GF_FALSE, &rc);
		else if (hl_color)
			visual_2d_fill_rect(tr_state->visual, ctx, &span->bounds, hl_color, 0, tr_state);

		if (use_texture_text && span_setup_texture(compositor, span, 0, tr_state)) {
			visual_2d_texture_path_text(tr_state->visual, ctx, span->ext->path, &span->bounds, span->ext->txh, tr_state);
		} else {
			gf_font_span_draw_2d(tr_state, span, ctx, bounds);
		}
		if (span->anchor || (span->flags & GF_TEXT_SPAN_UNDERLINE) ) gf_font_underline_span(tr_state, span, ctx);
		if (ctx->sub_path_index) break;
	}
	if (is_rv) tr_state->ctx->aspect.fill_color = hl_color;
}

void gf_font_spans_pick(GF_Node *node, GF_List *spans, GF_TraverseState *tr_state, GF_Rect *node_bounds, Bool use_dom_events, Drawable *drawable)
{
	u32 i, count, j, glyph_idx;
	Fixed dx, dy;
#ifndef GPAC_DISABLE_3D
	GF_Matrix inv_mx;
#endif
	GF_TextSpan *span;
	DrawAspect2D asp;
	GF_Matrix2D inv_2d;
	Fixed x, y;
	GF_Compositor *compositor = tr_state->visual->compositor;

	if (compositor->text_selection) {
		if (compositor->text_selection != tr_state->text_parent) return;
		if (compositor->store_text_state==GF_SC_TSEL_FROZEN) return;
	}

	/*TODO: pick the real glyph and not just the bounds of the text span*/
	count = gf_list_count(spans);

#ifndef GPAC_DISABLE_3D
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
	} else
#endif
	{
		gf_mx2d_copy(inv_2d, tr_state->transform);
		gf_mx2d_inverse(&inv_2d);
		x = tr_state->ray.orig.x;
		y = tr_state->ray.orig.y;
		gf_mx2d_apply_coords(&inv_2d, &x, &y);
	}

	if (use_dom_events) {
		memset(&asp, 0, sizeof(DrawAspect2D));
		drawable_get_aspect_2d_svg(node, &asp, tr_state);
	}

	span = NULL;
	for (i=0; i<count; i++) {
		Fixed loc_x, loc_y;
		span = (GF_TextSpan*)gf_list_get(spans, i);

		if ((x>=span->bounds.x)
		        && (y<=span->bounds.y)
		        && (x<=span->bounds.x+span->bounds.width)
		        && (y>=span->bounds.y-span->bounds.height)) {
		} else {
			continue;
		}

		glyph_idx = 0;
		dx = span->off_x;
		dy = span->off_y;
		for (j=0; j<span->nb_glyphs; j++) {
			GF_Rect rc;
			if (!span->glyphs[j]) {
				if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
					dx += span->font_scale * span->font->max_advance_h;
				} else {
					dy -= span->font_scale * span->font->max_advance_v;
				}
				continue;
			}
			else if (span->glyphs[j]->ID==GF_CARET_CHAR) {
				continue;
			}
			glyph_idx++;

			loc_x = x - (span->dx ? span->dx[j] : dx);
			loc_y = y - (span->dy ? span->dy[j] : dy);
			loc_x = gf_divfix(loc_x, span->font_scale);
			loc_y = gf_divfix(loc_y, span->font_scale) + span->font->baseline;
			if (span->flags & GF_TEXT_SPAN_FLIP) loc_y = - loc_y;

			gf_path_get_bounds(span->glyphs[j]->path, &rc);
			rc.height = INT2FIX(span->font->ascent + span->font->descent);

			if (0&&span->rot) {
				GF_Matrix2D r;
				gf_mx2d_init(r);
				gf_mx2d_add_rotation(&r, 0, 0, span->rot[i]);
				gf_mx2d_apply_coords(&r, &loc_x, &loc_y);
			}

			if (use_dom_events && !compositor->sel_buffer) {
				if (svg_drawable_is_over(drawable, loc_x, loc_y, &asp, tr_state, &rc))
					goto picked;
			} else {
				if ( (loc_x >= rc.x) && (loc_y <= rc.y) && (loc_x <= rc.x + rc.width) && (loc_y >= rc.y - rc.height) ) {
					goto picked;
				}
//				if (gf_path_point_over(span->glyphs[j]->path, loc_x, loc_y)) goto picked;
			}

			if (span->flags & GF_TEXT_SPAN_HORIZONTAL) {
				dx += span->font_scale * span->glyphs[j]->horiz_advance;
			} else {
				dy -= span->font_scale * span->glyphs[j]->vert_advance;
			}
		}
	}
	return;

picked:
	compositor->hit_local_point.x = x;
	compositor->hit_local_point.y = y;
	compositor->hit_local_point.z = 0;

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		gf_mx_copy(compositor->hit_world_to_local, tr_state->model_matrix);
		gf_mx_copy(compositor->hit_local_to_world, inv_mx);
	} else
#endif
	{
		gf_mx_from_mx2d(&compositor->hit_world_to_local, &tr_state->transform);
		gf_mx_from_mx2d(&compositor->hit_local_to_world, &inv_2d);
	}

	compositor->hit_node = node;
	if (span->anchor) compositor->hit_node = span->anchor;

	compositor->end_sel.x = compositor->hit_world_point.x;
	compositor->end_sel.y = compositor->hit_world_point.y;
	if (compositor->text_selection) {
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		if (tr_state->visual->offscreen) gf_node_dirty_set(tr_state->visual->offscreen, GF_SG_CHILD_DIRTY, 0);
		span->flags |= GF_TEXT_SPAN_SELECTED;
	} else {
		compositor->start_sel = compositor->end_sel;
	}
	compositor->picked_span_idx = i;
	compositor->picked_glyph_idx = glyph_idx;

	compositor->hit_text = tr_state->text_parent;
	compositor->hit_use_dom_events = use_dom_events;
	compositor->hit_normal.x = compositor->hit_normal.y = 0;
	compositor->hit_normal.z = FIX_ONE;
	compositor->hit_texcoords.x = gf_divfix(x, node_bounds->width) + FIX_ONE/2;
	compositor->hit_texcoords.y = gf_divfix(y, node_bounds->height) + FIX_ONE/2;

#ifndef GPAC_DISABLE_VRML
	if (compositor_is_composite_texture(tr_state->appear)) {
		compositor->hit_appear = tr_state->appear;
	} else
#endif /*GPAC_DISABLE_VRML*/
	{
		compositor->hit_appear = NULL;
	}

	gf_list_reset(tr_state->visual->compositor->sensors);
	count = gf_list_count(tr_state->vrml_sensors);
	for (i=0; i<count; i++) {
		gf_list_add(tr_state->visual->compositor->sensors, gf_list_get(tr_state->vrml_sensors, i));
	}
}
