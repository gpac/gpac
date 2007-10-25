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


GF_TextSpan *gf_font_manager_create_span(GF_FontManager *fm, GF_Font *font, char *span, Fixed font_size)
{
	GF_Err e;
	u32 len, i;
	GF_TextSpan *tspan;

	len = fm->conv_buffer_size;
	e = fm->reader->get_glyphs(fm->reader, span, fm->conv_buffer, &len);
	if (e==GF_BUFFER_TOO_SMALL) {
		fm->conv_buffer_size = len;
		fm->conv_buffer = realloc(fm->conv_buffer, sizeof(unsigned short) * len);
		if (!fm->conv_buffer) return NULL;
		e = fm->reader->get_glyphs(fm->reader, span, fm->conv_buffer, &len);
	}
	if (e) return NULL;

	GF_SAFEALLOC(tspan, GF_TextSpan);
	tspan->font = font;
	tspan->font_size = font_size;
	tspan->font_scale = gf_divfix(font_size, font->em_size);
	tspan->x_scale = tspan->y_scale = FIX_ONE;
	tspan->nb_glyphs = len;
	tspan->glyphs = malloc(sizeof(void *)*len);
	
	for (i=0; i<len; i++) {
		tspan->glyphs[i] = gf_font_get_glyph(fm, font, fm->conv_buffer[i]);
	}
	return tspan;
}


void gf_font_manager_delete_span(GF_FontManager *fm, GF_TextSpan *tspan)
{
	free(tspan->glyphs);
	free(tspan);
}

