/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / FreeType font engine module
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


#include <gpac/modules/font.h>
#include <gpac/list.h>
#include <gpac/utf.h>
#include <gpac/tools.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
/*TrueType tables*/
#include FT_TRUETYPE_TABLES_H 

typedef struct
{
	FT_Library library;
	FT_Face active_face;

	char *font_dir;

	Fixed pixel_size;

	GF_List *loaded_fonts;

	/*0: no line, 1: underlined, 2: strikeout*/
	u32 strike_style;

	Bool register_font;

	/*temp storage for enum - may be NULL*/
	const char *tmp_font_name;
	u32 tmp_font_style;
	/*default fonts*/
	char font_serif[1024];
	char font_sans[1024];
	char font_fixed[1024];
} FTBuilder;



static GF_Err ft_init_font_engine(GF_FontReader *dr)
{
	const char *sOpt;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontDirectory");
	if (!sOpt) return GF_BAD_PARAM;

	/*inits freetype*/
	if (FT_Init_FreeType(&ftpriv->library) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[FreeType] Cannot initialize FreeType\n"));
		return GF_IO_ERR;
	}

	/*remove the final delimiter*/
    ftpriv->font_dir = strdup(sOpt);
	while ( (ftpriv->font_dir[strlen(ftpriv->font_dir)-1] == '\n') || (ftpriv->font_dir[strlen(ftpriv->font_dir)-1] == '\r') )
		ftpriv->font_dir[strlen(ftpriv->font_dir)-1] = 0;

	/*store font path*/
	if (ftpriv->font_dir[strlen(ftpriv->font_dir)-1] != GF_PATH_SEPARATOR) {
		char ext[2], *temp;
		ext[0] = GF_PATH_SEPARATOR;
		ext[1] = 0;
		temp = malloc(sizeof(char) * (strlen(ftpriv->font_dir) + 2));
		strcpy(temp, ftpriv->font_dir);
		strcat(temp, ext);
		free(ftpriv->font_dir);
		ftpriv->font_dir = temp;
	}
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif");
	if (sOpt) strcpy(ftpriv->font_serif, sOpt);

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSans");
	if (sOpt) strcpy(ftpriv->font_sans, sOpt);
	
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed");
	if (sOpt) strcpy(ftpriv->font_fixed, sOpt);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[FreeType] Init OK - font directory %s\n", ftpriv->font_dir));
	
	return GF_OK;
}

static GF_Err ft_shutdown_font_engine(GF_FontReader *dr)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	ftpriv->active_face = NULL;
	/*reset loaded fonts*/
	while (gf_list_count(ftpriv->loaded_fonts)) {
		FT_Face face = gf_list_get(ftpriv->loaded_fonts, 0);
		gf_list_rem(ftpriv->loaded_fonts, 0);
		FT_Done_Face(face);
	}

	/*exit FT*/
	if (ftpriv->library) FT_Done_FreeType(ftpriv->library);
	ftpriv->library = NULL;
	return GF_OK;
}


static Bool ft_check_face(FT_Face font, const char *fontName, u32 styles)
{
	u32 ft_style, loc_styles;
	char *name;

	if (fontName && stricmp(font->family_name, fontName)) return 0;
	ft_style = 0;
	if (font->style_name) {
		name = strdup(font->style_name);
		strupr(name);
		if (strstr(name, "BOLD")) ft_style |= GF_FONT_WEIGHT_BOLD;
		if (strstr(name, "ITALIC")) ft_style |= GF_FONT_ITALIC;
		free(name);
	} else {
		if (font->style_flags & FT_STYLE_FLAG_BOLD) ft_style |= GF_FONT_WEIGHT_BOLD;
		if (font->style_flags & FT_STYLE_FLAG_ITALIC) ft_style |= GF_FONT_ITALIC;
	}
	name = strdup(font->family_name);
	strupr(name);
	if (strstr(name, "BOLD")) ft_style |= GF_FONT_WEIGHT_BOLD;
	if (strstr(name, "ITALIC")) ft_style |= GF_FONT_ITALIC;
	free(name);

	loc_styles = styles & GF_FONT_WEIGHT_MASK;
	if (loc_styles>=GF_FONT_WEIGHT_BOLD)
		styles = (styles & 0x00000007) | GF_FONT_WEIGHT_BOLD;
	else 
		styles = (styles & 0x00000007);

	if (ft_style==styles) 
		return 1;
	return 0;
}

static FT_Face ft_font_in_cache(FTBuilder *ft, const char *fontName, u32 styles)
{
	u32 i=0;
	FT_Face font;

	while ((font = gf_list_enum(ft->loaded_fonts, &i))) {
		if (ft_check_face(font, fontName, styles)) return font;
	}
	return NULL;
}


static Bool ft_enum_fonts(void *cbck, char *file_name, char *file_path)
{
	FT_Face face;
	u32 num_faces, i;
	GF_FontReader *dr = cbck;
	FTBuilder *ftpriv = dr->udta;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[FreeType] Enumerating font %s (%s)\n", file_name, file_path));

	if (FT_New_Face(ftpriv->library, file_path, 0, & face )) return 0;
	if (!face) return 0;

	num_faces = face->num_faces;
	/*locate right font in collection if several*/
	for (i=0; i<num_faces; i++) {
		if (ft_check_face(face, ftpriv->tmp_font_name ? ftpriv->tmp_font_name : face->family_name, ftpriv->tmp_font_style)) 
			break;
		
		FT_Done_Face(face);
		if (i+1==num_faces) return 0;

		/*load next font in collection*/
		if (FT_New_Face(ftpriv->library, file_path, i+1, & face )) return 0;
		if (!face) return 0;
	}

	/*reject font if not scalablebitmap glyphs*/
	if (! (face->face_flags & FT_FACE_FLAG_SCALABLE)) {
		FT_Done_Face(face);
		return 0;
	}
	/*OK store in cache*/
	gf_list_add(ftpriv->loaded_fonts, face);
	ftpriv->active_face = face;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[FreeType] Found font %s in directory %s\n", file_name, file_path));
	/*and store entry in cfg file*/
	if (ftpriv->register_font) {
		char szFont[GF_MAX_PATH];
		strcpy(szFont, face->family_name);
		if (ftpriv->tmp_font_style & GF_FONT_WEIGHT_BOLD) strcat(szFont, " Bold");
		if (ftpriv->tmp_font_style & GF_FONT_ITALIC) strcat(szFont, " Italic");
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", szFont, file_path);
	}
	return 1;
}

static Bool ft_enum_fonts_dir(void *cbck, char *file_name, char *file_path)
{
	Bool ret = gf_enum_directory(file_path, 0, ft_enum_fonts, cbck, "ttf;ttc");
	if (ret) return 1;
	return gf_enum_directory(file_path, 1, ft_enum_fonts_dir, cbck, NULL);
}

static GF_Err ft_set_font(GF_FontReader *dr, const char *OrigFontName, u32 styles)
{
	char fname[1024];
	char *fontName;
	Bool check_def_fonts = 0;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	fontName = (char *) OrigFontName;
	ftpriv->active_face = NULL;
	ftpriv->strike_style = 0;
	if (!fontName || !strlen(fontName) || !stricmp(fontName, "SERIF")) {
		fontName = ftpriv->font_serif;
		check_def_fonts = 1;
	}
	else if (!stricmp(fontName, "SANS") || !stricmp(fontName, "sans-serif")) {
		fontName = ftpriv->font_sans;
		check_def_fonts = 1;
	}
	else if (!stricmp(fontName, "TYPEWRITER") || !stricmp(fontName, "monospace")) {
		fontName = ftpriv->font_fixed;
		check_def_fonts = 1;
	}

	/*first look in loaded fonts*/
	ftpriv->active_face = ft_font_in_cache(ftpriv, fontName, styles);
	if (ftpriv->active_face) return GF_OK;

	ftpriv->tmp_font_name = fontName;
	ftpriv->tmp_font_style = styles;
	ftpriv->register_font = 0;

	/*check cfg file - freetype is slow at loading fonts so we keep the (font name + styles)=fontfile associations
	in the cfg file*/
	if (fontName && strlen(fontName)) {
		const char *opt;
		strcpy(fname, fontName);
		if (styles & GF_FONT_WEIGHT_BOLD) strcat(fname, " Bold");
		if (styles & GF_FONT_ITALIC) strcat(fname, " Italic");

		opt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", fname);
		if (opt) {
			char *font_name;
			if (!stricmp(opt, "UNKNOWN")) return GF_NOT_SUPPORTED;
			font_name = strrchr(opt, '/');
			if (!font_name) font_name = strrchr(opt, '\\');
			if (font_name && ft_enum_fonts(dr, font_name+1, (char *)opt)) return GF_OK;
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[FreeType] Looking for fonts in %s\n", ftpriv->font_dir));
	/*not found, browse all fonts*/
	ftpriv->register_font = 1;
	if (!strlen(ftpriv->tmp_font_name)) ftpriv->tmp_font_name = NULL;
	gf_enum_directory(ftpriv->font_dir, 0, ft_enum_fonts, dr, "ttf;ttc");
	if (!ftpriv->active_face) 
		gf_enum_directory(ftpriv->font_dir, 1, ft_enum_fonts_dir, dr, NULL);
	ftpriv->register_font = 0;

	if (ftpriv->active_face) {
		if (check_def_fonts) {
			/*reassign default - they may be wrong, but this will avoid future browsing*/
			if (!ftpriv->font_serif[0] && (!OrigFontName || !stricmp(OrigFontName, "SERIF")) ) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif", ftpriv->active_face->family_name);
				strcpy(ftpriv->font_serif, ftpriv->active_face->family_name);
			}
			else if (!ftpriv->font_sans[0] && OrigFontName && !stricmp(OrigFontName, "SANS")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSans", ftpriv->active_face->family_name);
				strcpy(ftpriv->font_sans, ftpriv->active_face->family_name);
			}
			else if (!ftpriv->font_fixed[0] && OrigFontName && !stricmp(OrigFontName, "TYPEWRITTER")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed", ftpriv->active_face->family_name);
				strcpy(ftpriv->font_fixed, ftpriv->active_face->family_name);
			}
		}
		return GF_OK;
	}

	if (fontName) {
		/*font not on system...*/
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", fname, "UNKNOWN");

		if (!styles) {
			/*remove cache if default fonts not found*/
			if (!OrigFontName || !stricmp(OrigFontName, "SERIF")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif", NULL);
				strcpy(ftpriv->font_serif, "");
			}
			else if (!stricmp(OrigFontName, "SANS")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSans", NULL);
				strcpy(ftpriv->font_sans, "");
			}
			else if (!stricmp(OrigFontName, "TYPEWRITTER")) {
				gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed", NULL);
				strcpy(ftpriv->font_fixed, "");
			}
		}
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[FreeType] Font not found\n"));
	return GF_NOT_SUPPORTED;
}

static GF_Err ft_get_font_info(GF_FontReader *dr, char **font_name, s32 *em_size, s32 *ascent, s32 *descent, s32 *line_spacing, s32 *max_advance_h, s32 *max_advance_v)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;
	if (!ftpriv->active_face) return GF_BAD_PARAM;

	*em_size = ftpriv->active_face->units_per_EM;
	*ascent = ftpriv->active_face->ascender;
	*descent = ftpriv->active_face->descender;
	*line_spacing = ftpriv->active_face->height;
	*font_name = strdup(ftpriv->active_face->family_name);
	*max_advance_h = ftpriv->active_face->max_advance_width;
	*max_advance_v = ftpriv->active_face->max_advance_height;
	return GF_OK;
}


static GF_Err ft_get_glyphs(GF_FontReader *dr, const char *utf_string, u32 *glyph_buffer, u32 *io_glyph_buffer_size, const char *xml_lang)
{
	u32 len;
	u32 i;
	Bool rev;
	u16 *conv;
	char *utf8 = (char*) utf_string;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	if (!ftpriv->active_face) return GF_BAD_PARAM;

	/*TODO: reverse layout (for arabic fonts) and glyph substitution / ligature */

	len = utf_string ? strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}
	if (*io_glyph_buffer_size < len+1) {
		*io_glyph_buffer_size = len+1;
		return GF_BUFFER_TOO_SMALL;
	}
	len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, &utf8);
	if ((s32)len<0) return GF_IO_ERR;
	if (utf8) return GF_IO_ERR;

	conv = (u16*) glyph_buffer;
	rev = gf_utf8_is_right_to_left(conv);
	for (i=len; i>0; i--) {
		glyph_buffer[i-1] = (u32) conv[i-1];
	}
	if (rev) { 
		for (i=0; i<len/2; i++) {
			u32 v = glyph_buffer[i];
			glyph_buffer[i] = glyph_buffer[len-1-i];
			glyph_buffer[len-1-i] = v;
		}
	}
	*io_glyph_buffer_size = len;
	return GF_OK;
}





typedef struct
{
	FTBuilder *ftpriv;
	GF_Path *path;
	s32 last_x, last_y;
} ft_outliner;


static int ft_move_to(FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_move_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	ftol->last_x = to->x;
	ftol->last_y = to->y;
	return 0;
}
static int ft_line_to(FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) {
		gf_path_close(ftol->path);
	} else {
		gf_path_add_line_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	}
	return 0;
}
static int ft_conic_to(FT_Vector * control, FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_quadratic_to(ftol->path, INT2FIX(control->x), INT2FIX(control->y), INT2FIX(to->x), INT2FIX(to->y) );
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) gf_path_close(ftol->path);
	return 0;
}
static int ft_cubic_to(FT_Vector *c1, FT_Vector *c2, FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_cubic_to(ftol->path, INT2FIX(c1->x), INT2FIX(c1->y), INT2FIX(c2->x), INT2FIX(c2->y), INT2FIX(to->x), INT2FIX(to->y) );
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) gf_path_close(ftol->path);
	return 0;
}


static GF_Glyph *ft_load_glyph(GF_FontReader *dr, u32 glyph_name)
{
	GF_Glyph *glyph;
	u32 glyph_idx;
	FT_BBox bbox;
	FT_OutlineGlyph outline;
	ft_outliner outl;
	FT_Outline_Funcs ft_outl_funcs;

	FTBuilder *ftpriv = (FTBuilder *)dr->udta;
	if (!ftpriv->active_face || !glyph_name) return NULL;

	FT_Select_Charmap(ftpriv->active_face, FT_ENCODING_UNICODE);

	glyph_idx = FT_Get_Char_Index(ftpriv->active_face, glyph_name);
	/*missing glyph*/
	if (!glyph_idx) return NULL;

	/*work in design units*/
	FT_Load_Glyph(ftpriv->active_face, glyph_idx, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);

	FT_Get_Glyph(ftpriv->active_face->glyph, (FT_Glyph*)&outline);

#ifdef FT_GLYPH_FORMAT_OUTLINE
	/*oops not vectorial...*/
	if (outline->root.format != FT_GLYPH_FORMAT_OUTLINE) return NULL;
#endif


	GF_SAFEALLOC(glyph, GF_Glyph);
	GF_SAFEALLOC(glyph->path, GF_Path);

	/*setup outliner*/
	ft_outl_funcs.shift = 0;
	ft_outl_funcs.delta = 0;
	ft_outl_funcs.move_to = ft_move_to;
	ft_outl_funcs.line_to = ft_line_to;
	ft_outl_funcs.conic_to = ft_conic_to;
	ft_outl_funcs.cubic_to = ft_cubic_to;
	outl.path = glyph->path;
	outl.ftpriv = ftpriv;
	
	/*freeType is marvelous and gives back the right advance on space char !!!*/
	FT_Outline_Decompose(&outline->outline, &ft_outl_funcs, &outl);

	FT_Glyph_Get_CBox((FT_Glyph) outline, ft_glyph_bbox_unscaled, &bbox);

	glyph->ID = glyph_name;
	glyph->utf_name = glyph_name;
	glyph->horiz_advance = ftpriv->active_face->glyph->metrics.horiAdvance;
	glyph->vert_advance = ftpriv->active_face->glyph->metrics.vertAdvance;
/*
	glyph->x = bbox.xMin;
	glyph->y = bbox.yMax;
*/
	glyph->width = ftpriv->active_face->glyph->metrics.width;
	glyph->height = ftpriv->active_face->glyph->metrics.height;
	FT_Done_Glyph((FT_Glyph) outline);
	return glyph;
}


GF_FontReader *ft_load()
{
	GF_FontReader *dr;
	FTBuilder *ftpriv;
	dr = malloc(sizeof(GF_FontReader));
	memset(dr, 0, sizeof(GF_FontReader));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_FONT_READER_INTERFACE, "FreeType Font Reader", "gpac distribution");

	ftpriv = malloc(sizeof(FTBuilder));
	memset(ftpriv, 0, sizeof(FTBuilder));

	ftpriv->loaded_fonts = gf_list_new();

	dr->udta = ftpriv;

	
	dr->init_font_engine = ft_init_font_engine;
	dr->shutdown_font_engine = ft_shutdown_font_engine;
	dr->set_font = ft_set_font;
	dr->get_font_info = ft_get_font_info;
	dr->get_glyphs = ft_get_glyphs;
	dr->load_glyph = ft_load_glyph;
	return dr;
}


void ft_delete(GF_BaseInterface *ifce)
{
	GF_FontReader *dr = (GF_FontReader *) ifce;
	FTBuilder *ftpriv = dr->udta;


	if (ftpriv->font_dir) free(ftpriv->font_dir);
	assert(!gf_list_count(ftpriv->loaded_fonts) );

	gf_list_del(ftpriv->loaded_fonts);

	free(dr->udta);
	free(dr);
}

#ifndef GPAC_STANDALONE_RENDER_2D

GF_EXPORT
Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_FONT_READER_INTERFACE) return 1;
	return 0;
}

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_FONT_READER_INTERFACE) return (GF_BaseInterface *)ft_load();
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_FONT_READER_INTERFACE:
		ft_delete(ifce);
		break;
	}
}

#endif

