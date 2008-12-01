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

	GF_List *loaded_fonts;

	/*default fonts*/
	char font_serif[1024];
	char font_sans[1024];
	char font_fixed[1024];
} FTBuilder;


static Bool ft_enum_fonts(void *cbck, char *file_name, char *file_path)
{
	char szFont[GF_MAX_PATH];
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

		/*only scan scalable fonts*/
		if (face->face_flags & FT_FACE_FLAG_SCALABLE) {
			Bool bold, italic, smallcaps;
			strcpy(szFont, face->family_name);

			/*remember first font found which looks like a alphabetical one*/
			if (!strlen(ftpriv->font_dir)) {
				u32 gidx;
				FT_Select_Charmap(face, FT_ENCODING_UNICODE);
				gidx = FT_Get_Char_Index(face, (u32) 'a');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) 'z');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) '1');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) '@');
				if (gidx) strcpy(ftpriv->font_dir, szFont);
			}

			bold = italic = smallcaps = 0;

			if (face->style_name) {
				char *name = strdup(face->style_name);
				strupr(name);
				if (strstr(name, "BOLD")) bold = 1;
				if (strstr(name, "ITALIC")) italic = 1;
				free(name);
			} else {
				if (face->style_flags & FT_STYLE_FLAG_BOLD) bold = 1;
				if (face->style_flags & FT_STYLE_FLAG_ITALIC) italic = 1;
			}
			
			if (bold) strcat(szFont, " Bold");
			if (italic) strcat(szFont, " Italic");
			if (smallcaps) strcat(szFont, " Smallcaps");
			gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", szFont, file_path);

			/*try to assign default fixed fonts*/
			if (!bold && !italic) {
				Bool store = 0;
				char szFont[1024];
				strcpy(szFont, face->family_name);
				strlwr(szFont);

				if (!strlen(ftpriv->font_fixed)) {
					if (face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) store = 1;
					else if (!strnicmp(face->family_name, "Courier", 15)) store = 1;
					else if (strstr(szFont, "sans") || strstr(szFont, "serif")) store = 0;
					else if (strstr(szFont, "monospace")) store = 1;

					if (store) strcpy(ftpriv->font_fixed, face->family_name);
				}
				if (!store && !strlen(ftpriv->font_sans)) {
					if (!strnicmp(face->family_name, "Arial", 5)) store = 1;
					else if (!strnicmp(face->family_name, "Verdana", 7)) store = 1;
					else if (strstr(szFont, "serif") || strstr(szFont, "fixed")) store = 0;
					else if (strstr(szFont, "sans")) store = 1;

					if (store) strcpy(ftpriv->font_sans, face->family_name);
				}
				if (!store && !strlen(ftpriv->font_serif)) {
					if (!strnicmp(face->family_name, "Times New Roman", 15)) store = 1;
					else if (strstr(szFont, "sans") || strstr(szFont, "fixed")) store = 0;
					else if (strstr(szFont, "serif")) store = 1;

					if (store) strcpy(ftpriv->font_serif, face->family_name);
				}
			}
		}

		FT_Done_Face(face);
		if (i+1==num_faces) return 0;

		/*load next font in collection*/
		if (FT_New_Face(ftpriv->library, file_path, i+1, & face )) return 0;
		if (!face) return 0;
	}
	return 0;
}

static Bool ft_enum_fonts_dir(void *cbck, char *file_name, char *file_path)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[FreeType] Scanning directory %s (%s)\n", file_name, file_path));
	gf_enum_directory(file_path, 0, ft_enum_fonts, cbck, "ttf;ttc");
	return gf_enum_directory(file_path, 1, ft_enum_fonts_dir, cbck, NULL);
}


static void ft_rescan_fonts(GF_FontReader *dr)
{
	char *font_dir;
	char font_def[1024];
	u32 i, count;
	GF_Config *cfg = gf_modules_get_config((GF_BaseInterface *)dr);
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[FreeType] Rescaning font directory %s\n", ftpriv->font_dir));

	count = gf_cfg_get_key_count(cfg, "FontEngine");
	for (i=0; i<count; i++) {
		const char *key = gf_cfg_get_key_name(cfg, "FontEngine", i);
		if (!strcmp(key, "FontReader")) continue;
		if (!strcmp(key, "FontDirectory")) continue;
		if (!strcmp(key, "RescanFonts")) continue;
		/*any other persistent options should go here*/

		gf_cfg_set_key(cfg, "FontEngine", key, NULL);
		count--;
		i--;
	}
	gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "RescanFonts", "no");

	strcpy(ftpriv->font_serif, "");
	strcpy(ftpriv->font_sans, "");
	strcpy(ftpriv->font_fixed, "");

	font_dir = ftpriv->font_dir;
	/*here we will store the first font found*/
	font_def[0] = 0;
	ftpriv->font_dir = font_def;
	
	gf_enum_directory(font_dir, 0, ft_enum_fonts, dr, "ttf;ttc");
	gf_enum_directory(font_dir, 1, ft_enum_fonts_dir, dr, NULL);
	ftpriv->font_dir = font_dir;

	if ( strlen(font_def) ) {
		if (!strlen(ftpriv->font_fixed)) strcpy(ftpriv->font_fixed, font_def);
		if (!strlen(ftpriv->font_serif)) strcpy(ftpriv->font_serif, font_def);
		if (!strlen(ftpriv->font_sans)) strcpy(ftpriv->font_sans, font_def);
	}
	gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed", ftpriv->font_fixed);
	gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif", ftpriv->font_serif);
	gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSans", ftpriv->font_sans);

	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[FreeType] Font directory scanned\n", ftpriv->font_dir));
}



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

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "RescanFonts");
	if (!sOpt || !strcmp(sOpt, "yes") ) 
		ft_rescan_fonts(dr);

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



static GF_Err ft_set_font(GF_FontReader *dr, const char *OrigFontName, u32 styles)
{
	char fname[1024];
	char *fontName;
	const char *opt;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	fontName = (char *) OrigFontName;
	ftpriv->active_face = NULL;

	if (!fontName || !strlen(fontName) || !stricmp(fontName, "SERIF")) {
		fontName = ftpriv->font_serif;
	}
	else if (!stricmp(fontName, "SANS") || !stricmp(fontName, "sans-serif")) {
		fontName = ftpriv->font_sans;
	}
	else if (!stricmp(fontName, "TYPEWRITER") || !stricmp(fontName, "monospace")) {
		fontName = ftpriv->font_fixed;
	}

	/*first look in loaded fonts*/
	ftpriv->active_face = ft_font_in_cache(ftpriv, fontName, styles);
	if (ftpriv->active_face) return GF_OK;

	/*check cfg file - freetype is slow at loading fonts so we keep the (font name + styles)=fontfile associations
	in the cfg file*/
	if (!fontName || !strlen(fontName)) return GF_NOT_SUPPORTED;
	strcpy(fname, fontName);
	if (styles & GF_FONT_WEIGHT_BOLD) strcat(fname, " Bold");
	if (styles & GF_FONT_ITALIC) strcat(fname, " Italic");

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", fname);
	if (opt) {
		FT_Face face;
		if (FT_New_Face(ftpriv->library, opt, 0, & face )) return GF_IO_ERR;
		if (!face) return GF_IO_ERR;
		gf_list_add(ftpriv->loaded_fonts, face);
		ftpriv->active_face = face;
		return GF_OK;
	}

	GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[FreeType] Font %s not found\n", fname));
	return GF_NOT_SUPPORTED;
}

static GF_Err ft_get_font_info(GF_FontReader *dr, char **font_name, s32 *em_size, s32 *ascent, s32 *descent, s32 *underline, s32 *line_spacing, s32 *max_advance_h, s32 *max_advance_v)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;
	if (!ftpriv->active_face) return GF_BAD_PARAM;

	*em_size = ftpriv->active_face->units_per_EM;
	*ascent = ftpriv->active_face->ascender;
	*descent = ftpriv->active_face->descender;
	*underline = ftpriv->active_face->underline_position;
	*line_spacing = ftpriv->active_face->height;
	*font_name = strdup(ftpriv->active_face->family_name);
	*max_advance_h = ftpriv->active_face->max_advance_width;
	*max_advance_v = ftpriv->active_face->max_advance_height;
	return GF_OK;
}


static GF_Err ft_get_glyphs(GF_FontReader *dr, const char *utf_string, u32 *glyph_buffer, u32 *io_glyph_buffer_size, const char *xml_lang, Bool *is_rtl)
{
	u32 len;
	u32 i;
	u16 *conv;
	char *utf8 = (char*) utf_string;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	if (!ftpriv->active_face) return GF_BAD_PARAM;

	/*TODO: glyph substitution / ligature */

	len = utf_string ? strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}
	if (*io_glyph_buffer_size < len+1) {
		*io_glyph_buffer_size = len+1;
		return GF_BUFFER_TOO_SMALL;
	}
	len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, (const char **) &utf8);
	if ((s32)len<0) return GF_IO_ERR;
	if (utf8) return GF_IO_ERR;

	/*perform bidi relayout*/
	conv = (u16*) glyph_buffer;
	*is_rtl = gf_utf8_reorder_bidi(conv, len);
	/*move 16bit buffer to 32bit*/
	for (i=len; i>0; i--) {
		glyph_buffer[i-1] = (u32) conv[i-1];
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


static int ft_move_to(const FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_move_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	ftol->last_x = to->x;
	ftol->last_y = to->y;
	return 0;
}
static int ft_line_to(const FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) {
		gf_path_close(ftol->path);
	} else {
		gf_path_add_line_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	}
	return 0;
}
static int ft_conic_to(const FT_Vector * control, const FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_quadratic_to(ftol->path, INT2FIX(control->x), INT2FIX(control->y), INT2FIX(to->x), INT2FIX(to->y) );
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) gf_path_close(ftol->path);
	return 0;
}
static int ft_cubic_to(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user)
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

