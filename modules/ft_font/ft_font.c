/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifndef GPAC_DISABLE_EVG


#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "freetype")
# elif defined (WIN32)
#  pragma comment(lib, "freetype")
# endif
#endif


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

	GF_List *font_dirs;

	GF_List *loaded_fonts;

	/*default fonts*/
	char *font_serif, *font_sans, *font_fixed, *font_default;
	Bool cache_checked;
} FTBuilder;

static const char * BEST_FIXED_FONTS[] = {
	"Courier New",
	"Courier",
	"Monaco",
	"Bitstream Vera Monospace",
	"Droid Sans Mono",
	NULL
};

static const char * BEST_SERIF_FONTS[] = {
	"Times New Roman",
	"Bitstream Vera",
	"Times",
	"Droid Serif",
	NULL
};

static const char * BEST_SANS_FONTS[] = {
	"Arial",
	"Tahoma",
	"Verdana",
	"Helvetica",
	"Bitstream Vera Sans",
	"Frutiger",
	"Droid Sans",
	NULL
};

/**
 * Choose the best font in the list of fonts
 */
static Bool isBestFontFor(const char * listOfFonts[], const char * currentBestFont, const char * fontName) {
	u32 i;
	assert( fontName );
	assert( listOfFonts );
	for (i = 0 ; listOfFonts[i]; i++) {
		const char * best = listOfFonts[i];
		if (!stricmp(best, fontName))
			return GF_TRUE;
		if (currentBestFont && !stricmp(best, currentBestFont))
			return GF_FALSE;
	}
	/* Nothing has been found, the font is the best if none has been choosen before */
	return currentBestFont == NULL;
}

void setBestFont(const char * listOfFonts[], char ** currentBestFont, const char * fontName) {
	if (isBestFontFor(listOfFonts, *currentBestFont, fontName)) {
		if (*currentBestFont)
			gf_free(*currentBestFont);
		*currentBestFont = NULL;
	}
	if (! (*currentBestFont)) {
		*currentBestFont = gf_strdup(fontName);
	}
}

static Bool ft_enum_fonts(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	char *szfont;
	FT_Face face;
	u32 num_faces, i;
	GF_FontReader *dr = cbck;
	FTBuilder *ftpriv = dr->udta;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[FreeType] Enumerating font %s (%s)\n", file_name, file_path));

	if (FT_New_Face(ftpriv->library, file_path, 0, & face )) return 0;
	if (!face || !face->family_name) return 0;

	num_faces = (u32) face->num_faces;
	/*locate right font in collection if several*/
	for (i=0; i<num_faces; i++) {

		/*only scan scalable fonts*/
		if (face->face_flags & FT_FACE_FLAG_SCALABLE) {
			Bool bold, italic;
			szfont = gf_malloc(sizeof(char)* (strlen(face->family_name)+100));
			if (!szfont) continue;
			strcpy(szfont, face->family_name);

			bold = italic = 0;

			if (face->style_name) {
				char *name = gf_strdup(face->style_name);
				strupr(name);
				if (strstr(name, "BOLD")) bold = 1;
				if (strstr(name, "ITALIC")) italic = 1;
				/*if font is not regular style, append all styles blindly*/
				if (!strstr(name, "REGULAR")) {
					strcat(szfont, " ");
					strcat(szfont, face->style_name);
				}
				gf_free(name);
			} else {
				if (face->style_flags & FT_STYLE_FLAG_BOLD) bold = 1;
				if (face->style_flags & FT_STYLE_FLAG_ITALIC) italic = 1;

				if (bold) strcat(szfont, " Bold");
				if (italic) strcat(szfont, " Italic");
			}
			gf_opts_set_key("FontCache", szfont, file_path);

			/*remember first font found which looks like a alphabetical one*/
			if (!ftpriv->font_default) {
				u32 gidx;
				FT_Select_Charmap(face, FT_ENCODING_UNICODE);
				gidx = FT_Get_Char_Index(face, (u32) 'a');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) 'z');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) '1');
				if (gidx) gidx = FT_Get_Char_Index(face, (u32) '@');
				if (gidx) ftpriv->font_default = gf_strdup(szfont);
			}

			/*try to assign default fixed fonts*/
			if (!bold && !italic) {
				strcpy(szfont, face->family_name);
				strlwr(szfont);

				if (face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) {
					setBestFont(BEST_FIXED_FONTS, &(ftpriv->font_fixed), face->family_name);
				}
				setBestFont(BEST_SERIF_FONTS, &(ftpriv->font_serif), face->family_name);
				setBestFont(BEST_SANS_FONTS, &(ftpriv->font_sans), face->family_name);
			}
			gf_free(szfont);
		}

		FT_Done_Face(face);
		if (i+1==num_faces) return 0;

		/*load next font in collection*/
		if (FT_New_Face(ftpriv->library, file_path, i+1, & face )) return 0;
		if (!face) return 0;
	}
	return 0;
}

static Bool ft_enum_fonts_dir(void *cbck, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[FreeType] Scanning directory %s (%s)\n", file_name, file_path));
	gf_enum_directory(file_path, 0, ft_enum_fonts, cbck, "ttf;ttc");
	return (gf_enum_directory(file_path, 1, ft_enum_fonts_dir, cbck, NULL)==GF_OK) ? GF_FALSE : GF_TRUE;
}


static void ft_rescan_fonts(GF_FontReader *dr)
{
	u32 i, count;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	ftpriv->cache_checked = GF_TRUE;

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[FreeType] Rescaning %d font directories\n", gf_list_count(ftpriv->font_dirs) ));

	gf_opts_del_section("FontCache");
	gf_opts_del_section("temp_freetype");
	gf_opts_set_key("core", "rescan-fonts", "no");

	if (ftpriv->font_fixed) gf_free(ftpriv->font_fixed);
	ftpriv->font_fixed = NULL;
	if (ftpriv->font_sans) gf_free(ftpriv->font_sans);
	ftpriv->font_sans = NULL;
	if (ftpriv->font_serif) gf_free(ftpriv->font_serif);
	ftpriv->font_serif = NULL;

	if (ftpriv->font_default) gf_free(ftpriv->font_default);
	ftpriv->font_default = NULL;


	count = gf_list_count(ftpriv->font_dirs);
	for (i=0; i<count; i++) {
		char *font_dir = gf_list_get(ftpriv->font_dirs, i);

		if (gf_dir_exists(font_dir)) {
			gf_enum_directory(font_dir, 0, ft_enum_fonts, dr, "ttf;ttc");
			gf_enum_directory(font_dir, 1, ft_enum_fonts_dir, dr, NULL);
		}
	}

	if (ftpriv->font_fixed) gf_free(ftpriv->font_fixed);
	ftpriv->font_fixed = NULL;
	if (ftpriv->font_sans) gf_free(ftpriv->font_sans);
	ftpriv->font_sans = NULL;
	if (ftpriv->font_serif) gf_free(ftpriv->font_serif);
	ftpriv->font_serif = NULL;

	/* let's check we have fonts that match our default Bold/Italic/BoldItalic conventions*/
	count = gf_opts_get_key_count("FontCache");
	for (i=0; i<count; i++) {
		const char *opt;
		char fkey[GF_MAX_PATH];
		const char *key = gf_opts_get_key_name("FontCache", i);
		opt = gf_opts_get_key("FontCache", key);
		if (!strchr(opt, '/') && !strchr(opt, '\\')) continue;

		if (strstr(key, "Bold")) continue;
		if (strstr(key, "Italic")) continue;

		strcpy(fkey, key);
		strcat(fkey, " Italic");
		opt = gf_opts_get_key("FontCache", fkey);
		if (!opt) continue;

		strcpy(fkey, key);
		strcat(fkey, " Bold");
		opt = gf_opts_get_key("FontCache", fkey);
		if (!opt) continue;

		strcpy(fkey, key);
		strcat(fkey, " Bold Italic");
		opt = gf_opts_get_key("FontCache", fkey);
		if (!opt) continue;

		strcpy(fkey, key);
		strlwr(fkey);

		/*this font is suited for our case*/
		if (isBestFontFor(BEST_FIXED_FONTS, ftpriv->font_fixed, key) || (!ftpriv->font_fixed && (strstr(fkey, "fixed") || strstr(fkey, "mono")) ) ) {
			if (ftpriv->font_fixed) gf_free(ftpriv->font_fixed);
			ftpriv->font_fixed = gf_strdup(key);
		}

		if (isBestFontFor(BEST_SANS_FONTS, ftpriv->font_sans, key) || (!ftpriv->font_sans && strstr(fkey, "sans")) ) {
			if (ftpriv->font_sans) gf_free(ftpriv->font_sans);
			ftpriv->font_sans = gf_strdup(key);
		}

		if (isBestFontFor(BEST_SERIF_FONTS, ftpriv->font_serif, key) || (!ftpriv->font_serif && strstr(fkey, "serif")) ) {
			if (ftpriv->font_serif) gf_free(ftpriv->font_serif);
			ftpriv->font_serif = gf_strdup(key);
		}
	}

	if (!ftpriv->font_serif) ftpriv->font_serif = gf_strdup(ftpriv->font_default ? ftpriv->font_default : "");
	if (!ftpriv->font_sans) ftpriv->font_sans = gf_strdup(ftpriv->font_default ? ftpriv->font_default : "");
	if (!ftpriv->font_fixed) ftpriv->font_fixed = gf_strdup(ftpriv->font_default ? ftpriv->font_default : "");

	gf_opts_set_key("FontCache", "FontFixed", ftpriv->font_fixed);
	gf_opts_set_key("FontCache", "FontSerif", ftpriv->font_serif);
	gf_opts_set_key("FontCache", "FontSans", ftpriv->font_sans);
	gf_opts_save();

	GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[FreeType] Font directories scanned - defaults: SANS %s FIXED %s SERIF %s\n", ftpriv->font_sans, ftpriv->font_fixed, ftpriv->font_serif));
}



static GF_Err ft_init_font_engine(GF_FontReader *dr)
{
	const char *sOpt;
	u32 rescan = 0;
	GF_Err e;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	/*inits freetype*/
	if (FT_Init_FreeType(&ftpriv->library) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[FreeType] Cannot initialize FreeType\n"));
		return GF_IO_ERR;
	}

	if (!gf_opts_get_key_count("FontCache"))
		rescan = 1;
	else {
		sOpt = gf_opts_get_key("core", "rescan-fonts");
		if (!sOpt || !strcmp(sOpt, "yes") )
			rescan = 1;
	}

rescan_font_dirs:
	sOpt = gf_opts_get_key("core", "font-dirs");
	if (!sOpt) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[FreeType] No fonts directory indicated!"));
		return GF_BAD_PARAM;
	}

	while (sOpt) {
		char dir[GF_MAX_PATH];
		char *sep = (char *) strchr(sOpt, ',');
		if (sep) sep[0] = 0;

		strcpy(dir, sOpt);
		while ( (dir[strlen(dir)-1] == '\n') || (dir[strlen(dir)-1] == '\r') )
			dir[strlen(dir)-1] = 0;

		if (dir[strlen(dir)-1] != GF_PATH_SEPARATOR) {
			char ext[2];
			ext[0] = GF_PATH_SEPARATOR;
			ext[1] = 0;
			strcat(dir, ext);
		}

		gf_list_add(ftpriv->font_dirs, gf_strdup(dir) );

		if (!sep) break;
		sep[0] = ',';
		sOpt = sep+1;
	}

rescan_fonts:
	if (rescan)
		ft_rescan_fonts(dr);

	if (!ftpriv->font_serif) {
		sOpt = gf_opts_get_key("FontCache", "FontSerif");
		ftpriv->font_serif = gf_strdup(sOpt ? sOpt : "");
	}

	if (!ftpriv->font_sans) {
		sOpt = gf_opts_get_key("FontCache", "FontSans");
		ftpriv->font_sans = gf_strdup(sOpt ? sOpt : "");
	}

	if (!ftpriv->font_fixed) {
		sOpt = gf_opts_get_key("FontCache", "FontFixed");
		ftpriv->font_fixed = gf_strdup(sOpt ? sOpt : "");
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[FreeType] Init OK - %d font directory (first %s)\n", gf_list_count(ftpriv->font_dirs), gf_list_get(ftpriv->font_dirs, 0) ));

	e = dr->set_font(dr, ftpriv->font_serif, 0);
	if (!e) e = dr->set_font(dr, ftpriv->font_sans, 0);
	if (!e) e = dr->set_font(dr, ftpriv->font_fixed, 0);
	if (!e) return GF_OK;

	if (rescan==3)
		return e;

	if (ftpriv->font_serif) gf_free(ftpriv->font_serif);
	ftpriv->font_serif = NULL;
	if (ftpriv->font_sans) gf_free(ftpriv->font_sans);
	ftpriv->font_sans = NULL;
	if (ftpriv->font_fixed) gf_free(ftpriv->font_fixed);
	ftpriv->font_fixed = NULL;

	//error and we rescanned font dirs, restore default fonts
	if (e && ftpriv->cache_checked)
		rescan = 2;

	if (!rescan) {
		sOpt = gf_opts_get_key("core", "font-dirs");
		rescan = 2;
		if (sOpt) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[FreeType] Default fonts not valid, rescanning font directories %s\n", sOpt));
			goto rescan_fonts;
		}
	}
	if (rescan==2) {
		void gf_get_default_font_dir(char szPath[GF_MAX_PATH]);

		char szPath[GF_MAX_PATH];
		//check if font directory is default one, if not reset and rescan
		gf_get_default_font_dir(szPath);
		sOpt = gf_opts_get_key("core", "font-dirs");
		if (sOpt && !strcmp(sOpt, szPath))
			return e;

		GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[FreeType] No fonts found in %s, restoring default directories %s and rescanning\n", sOpt, szPath));
		gf_opts_set_key("core", "font-dirs", szPath);
		rescan = 3;
		while (gf_list_count(ftpriv->font_dirs)) {
			gf_free(gf_list_pop_back(ftpriv->font_dirs));
		}
		goto rescan_font_dirs;
	}

	return GF_OK;
}

static GF_Err ft_shutdown_font_engine(GF_FontReader *dr)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	ftpriv->active_face = NULL;
	/*reset loaded fonts*/
	while (gf_list_count(ftpriv->loaded_fonts)) {
		FT_Face face = gf_list_pop_front(ftpriv->loaded_fonts);
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
		name = gf_strdup(font->style_name);
		strupr(name);
		if (strstr(name, "BOLD")) ft_style |= GF_FONT_WEIGHT_BOLD;
		if (strstr(name, "ITALIC")) ft_style |= GF_FONT_ITALIC;
		gf_free(name);
	} else {
		if (font->style_flags & FT_STYLE_FLAG_BOLD) ft_style |= GF_FONT_WEIGHT_BOLD;
		if (font->style_flags & FT_STYLE_FLAG_ITALIC) ft_style |= GF_FONT_ITALIC;
	}
	name = gf_strdup(font->family_name);
	strupr(name);
	if (strstr(name, "BOLD")) ft_style |= GF_FONT_WEIGHT_BOLD;
	if (strstr(name, "ITALIC")) ft_style |= GF_FONT_ITALIC;
	gf_free(name);

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
	GF_Err e = GF_OK;
	char *fname;
	char *fontName;
	const char *opt;
	Bool is_def_font = GF_FALSE;
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;

	fontName = (char *) OrigFontName;
	ftpriv->active_face = NULL;

	opt = gf_opts_get_key("temp_freetype", OrigFontName);
	if (opt) return GF_NOT_SUPPORTED;

	if (!fontName || !strlen(fontName) || !stricmp(fontName, "SERIF")) {
		fontName = ftpriv->font_serif;
		is_def_font = GF_TRUE;
		OrigFontName = "";
	}
	else if (!stricmp(fontName, "SANS") || !stricmp(fontName, "sans-serif")) {
		fontName = ftpriv->font_sans;
		is_def_font = GF_TRUE;
		OrigFontName = "SANS";
	}
	else if (!stricmp(fontName, "TYPEWRITER") || !stricmp(fontName, "monospace")) {
		fontName = ftpriv->font_fixed;
		is_def_font = GF_TRUE;
		OrigFontName = "TYPEWRITER";
	}

	/*first look in loaded fonts*/
	ftpriv->active_face = ft_font_in_cache(ftpriv, fontName, styles);
	if (ftpriv->active_face) return GF_OK;

	//we likely have a problem with the font cache, rebuild if
	if (!fontName || !strlen(fontName)) {
		if (is_def_font && !ftpriv->cache_checked) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[FreeType] No default font set, rescanning fonts\n"));
			ft_rescan_fonts(dr);
			return ft_set_font(dr, OrigFontName, styles);
		}
		return GF_NOT_SUPPORTED;
	}
	fname = gf_malloc(sizeof(char) * (strlen(fontName) + 50));

	int checkStyles = (styles & GF_FONT_WEIGHT_BOLD) | (styles & GF_FONT_ITALIC);

checkFont:
	strcpy(fname, fontName);
	if (checkStyles & GF_FONT_WEIGHT_BOLD) strcat(fname, " Bold");
	if (checkStyles & GF_FONT_ITALIC) strcat(fname, " Italic");

	opt = gf_opts_get_key("FontCache", fname);

	if (opt) {
		u32 i, num_faces;
		FT_Face face;

		//check font is in cache
		ftpriv->active_face = ft_font_in_cache(ftpriv, fname, checkStyles);
		if (ftpriv->active_face) {
			gf_free(fname);
			return GF_OK;
		}

		//load face
		if (FT_New_Face(ftpriv->library, opt, 0, & face) || !face) {
			e = GF_IO_ERR;
			goto exit;
		}

		//handle collections, figure out which face matches our styles
		num_faces = face->num_faces;
		for (i=0; i<num_faces; i++) {
			if ( ft_check_face(face, NULL, checkStyles)) {
				gf_free(fname);
				gf_list_add(ftpriv->loaded_fonts, face);
				ftpriv->active_face = face;
				return GF_OK;
			}
			FT_Done_Face(face);
			if (i+1==num_faces) break;

			/*load next font in collection*/
			if (FT_New_Face(ftpriv->library, opt, i+1, & face ))
				break;
			if (!face)
				break;
		}
	}
	if (checkStyles) {
		//we tried bold italic, try bold
		if (checkStyles == (GF_FONT_WEIGHT_BOLD | GF_FONT_ITALIC))
			checkStyles = GF_FONT_WEIGHT_BOLD;
		//we tried bold at previous pass, and input styles had italic, try italic
		else if ((checkStyles == GF_FONT_WEIGHT_BOLD) && (styles & GF_FONT_ITALIC))
			checkStyles = GF_FONT_ITALIC;
		//otherwise try with no style
		else if ((checkStyles == GF_FONT_WEIGHT_BOLD) || (checkStyles == GF_FONT_ITALIC))
			checkStyles = 0;
		goto checkFont;
	}

exit:
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[FreeType] Error loading font %s (%s): %s\n", fontName, fname, gf_error_to_string(e) ));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[FreeType] Font %s (%s) not found\n", fontName, fname));
		e = GF_NOT_SUPPORTED;
	}
	gf_free(fname);
	gf_opts_set_key("temp_freetype", OrigFontName, "not found");
	return e;
}

static GF_Err ft_get_font_info(GF_FontReader *dr, char **font_name, u32 *em_size, s32 *ascent, s32 *descent, s32 *underline, s32 *line_spacing, s32 *max_advance_h, s32 *max_advance_v)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->udta;
	if (!ftpriv->active_face) return GF_BAD_PARAM;

	*em_size = ftpriv->active_face->units_per_EM;
	*ascent = ftpriv->active_face->ascender;
	*descent = ftpriv->active_face->descender;
	*underline = ftpriv->active_face->underline_position;
	*line_spacing = ftpriv->active_face->height;
	*font_name = gf_strdup(ftpriv->active_face->family_name);
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

	len = utf_string ? (u32) strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}
	if (*io_glyph_buffer_size < len+1) {
		*io_glyph_buffer_size = len+1;
		return GF_BUFFER_TOO_SMALL;
	}
	len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, (const char **) &utf8);
	if (len == GF_UTF8_FAIL) return GF_IO_ERR;
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

#if defined(GPAC_CONFIG_ANDROID)
#define FTCST
#else
#define FTCST const
#endif



static int ft_move_to(FTCST FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_move_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	ftol->last_x = (s32) to->x;
	ftol->last_y = (s32) to->y;
	return 0;
}

static int ft_line_to(FTCST FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) {
		gf_path_close(ftol->path);
	} else {
		gf_path_add_line_to(ftol->path, INT2FIX(to->x), INT2FIX(to->y) );
	}
	return 0;
}

static int ft_conic_to(FTCST FT_Vector * control, FTCST FT_Vector *to, void *user)
{
	ft_outliner *ftol = (ft_outliner *)user;
	gf_path_add_quadratic_to(ftol->path, INT2FIX(control->x), INT2FIX(control->y), INT2FIX(to->x), INT2FIX(to->y) );
	if ( (ftol->last_x == to->x) && (ftol->last_y == to->y)) gf_path_close(ftol->path);
	return 0;
}

static int ft_cubic_to(FTCST FT_Vector *c1, FTCST FT_Vector *c2, FTCST FT_Vector *to, void *user)
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
	if (!glyph_idx) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[FreeType] Glyph not found for char %d in font %s (style %s)\n", glyph_name, ftpriv->active_face->family_name, ftpriv->active_face->style_name));
		return NULL;
	}

	/*work in design units*/
	FT_Load_Glyph(ftpriv->active_face, glyph_idx, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);

	FT_Get_Glyph(ftpriv->active_face->glyph, (FT_Glyph*)&outline);

	/*oops not vectorial...*/
	if (outline->root.format==FT_GLYPH_FORMAT_BITMAP) return NULL;


	GF_SAFEALLOC(glyph, GF_Glyph);
	if (!glyph) return NULL;
	GF_SAFEALLOC(glyph->path, GF_Path);
	if (!glyph->path) {
		gf_free(glyph);
		return NULL;
	}
	/*setup outliner*/
	ft_outl_funcs.shift = 0;
	ft_outl_funcs.delta = 0;
	ft_outl_funcs.move_to = ft_move_to;
	ft_outl_funcs.line_to = ft_line_to;
	ft_outl_funcs.conic_to = ft_conic_to;
	ft_outl_funcs.cubic_to = ft_cubic_to;
	outl.path = glyph->path;
	outl.ftpriv = ftpriv;

	/*FreeType is marvelous and gives back the right advance on space char !!!*/
	FT_Outline_Decompose(&outline->outline, &ft_outl_funcs, &outl);

	FT_Glyph_Get_CBox((FT_Glyph) outline, ft_glyph_bbox_unscaled, &bbox);

	glyph->ID = glyph_name;
	glyph->utf_name = glyph_name;
	glyph->horiz_advance = (s32) ftpriv->active_face->glyph->metrics.horiAdvance;
	glyph->vert_advance = (s32) ftpriv->active_face->glyph->metrics.vertAdvance;
	/*
		glyph->x = bbox.xMin;
		glyph->y = bbox.yMax;
	*/
	glyph->width = (u32) ftpriv->active_face->glyph->metrics.width;
	glyph->height = (u32) ftpriv->active_face->glyph->metrics.height;
	FT_Done_Glyph((FT_Glyph) outline);
	return glyph;
}


static GF_FontReader *ft_load()
{
	GF_FontReader *dr;
	FTBuilder *ftpriv;
	dr = gf_malloc(sizeof(GF_FontReader));
	memset(dr, 0, sizeof(GF_FontReader));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_FONT_READER_INTERFACE, "freetype", "gpac distribution");

	ftpriv = gf_malloc(sizeof(FTBuilder));
	memset(ftpriv, 0, sizeof(FTBuilder));

	ftpriv->font_dirs = gf_list_new();

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


static void ft_delete(GF_BaseInterface *ifce)
{
	GF_FontReader *dr = (GF_FontReader *) ifce;
	FTBuilder *ftpriv = dr->udta;

	while (gf_list_count(ftpriv->font_dirs)) {
		char *font = gf_list_pop_back(ftpriv->font_dirs);
		if (font)
			gf_free(font);
	}

	gf_list_del(ftpriv->font_dirs);

	if (ftpriv->font_serif) gf_free(ftpriv->font_serif);
	if (ftpriv->font_sans) gf_free(ftpriv->font_sans);
	if (ftpriv->font_fixed) gf_free(ftpriv->font_fixed);
	if (ftpriv->font_default) gf_free(ftpriv->font_default);
	assert(!gf_list_count(ftpriv->loaded_fonts) );

	gf_list_del(ftpriv->loaded_fonts);

	gf_free(dr->udta);
	gf_free(dr);
}

#ifndef GPAC_STANDALONE_RENDER_2D

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_FONT_READER_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_FONT_READER_INTERFACE) return (GF_BaseInterface *)ft_load();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_FONT_READER_INTERFACE:
		ft_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( ftfont )

#endif

#endif //GPAC_DISABLE_EVG
