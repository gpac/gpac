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


#include "ft_font.h"


static GF_Err ft_init_font_engine(GF_FontRaster *dr)
{
	const char *sOpt;
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontDirectory");
	if (!sOpt) return GF_BAD_PARAM;

	/*inits freetype*/
	if (FT_Init_FreeType(&ftpriv->library) ) return GF_IO_ERR;

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
	if (!sOpt) {
		sOpt = "Times New Roman";
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif", "Times New Roman");
	}
	strcpy(ftpriv->font_serif, sOpt);

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSans");
	if (!sOpt) {
		sOpt = "Arial";
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSans", "Arial");
	}
	strcpy(ftpriv->font_sans, sOpt);
	
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed");
	if (!sOpt) {
		sOpt = "Courier New";
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed", "Courier New");
	}
	strcpy(ftpriv->font_fixed, sOpt);
	return GF_OK;
}

static GF_Err ft_shutdown_font_engine(GF_FontRaster *dr)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;

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


static Bool ft_check_face(FT_Face font, const char *fontName, const char *styles)
{
	Bool ret;
	char *ft_name;
	char *ft_style;

	if (fontName && stricmp(font->family_name, fontName)) return 0;
	ft_style = strdup(font->style_name);
	strupr(ft_style);
	if (!styles) {
		ret = 1;
		if (strstr(ft_style, "BOLD") || strstr(ft_style, "ITALIC") ) ret = 0;
		free(ft_style);
		return ret;
	}
	ft_name = strdup(font->family_name);
	strupr(ft_name);
	if (strstr(styles, "BOLDITALIC") ) {
		if (!strstr(ft_name, "BOLD") && !strstr(ft_style, "BOLD") ) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
		if (!strstr(ft_name, "ITALIC") && !strstr(ft_style, "ITALIC") ) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
	}
	else if (strstr(styles, "BOLD")) {
		if (!strstr(ft_name, "BOLD") && !strstr(ft_style, "BOLD") ) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
		if (strstr(ft_style, "ITALIC")) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
	}
	else if (strstr(styles, "ITALIC")) {
		if (!strstr(ft_name, "ITALIC") && !strstr(ft_style, "ITALIC")) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
		if (strstr(ft_style, "BOLD")) {
			free(ft_name);
			free(ft_style);
			return 0;
		}
	}
	else if (strstr(ft_name, "ITALIC") || strstr(ft_style, "ITALIC") || strstr(ft_name, "BOLD") || strstr(ft_style, "BOLD") ) {
		free(ft_name);
		free(ft_style);
		return 0;
	}
	/*looks good, let's use this one*/
	free(ft_name);
	free(ft_style);
	return 1;
}

static FT_Face ft_font_in_cache(FTBuilder *ft, const char *fontName, const char *styles)
{
	u32 i;
	FT_Face font;

	for (i=0; i<gf_list_count(ft->loaded_fonts); i++) {
		font = gf_list_get(ft->loaded_fonts, i);
		if (ft_check_face(font, fontName, styles)) return font;
	}
	return NULL;
}

static Bool ft_enum_fonts(void *cbck, char *file_name, char *file_path)
{
	FT_Face face;
	u32 num_faces, i;
	GF_FontRaster *dr = cbck;
	FTBuilder *ftpriv = dr->priv;

	
	/*only trueType fonts/collections are handled*/
	if (!strstr(file_name, ".ttf") 
		&& !strstr(file_name, ".TTF")
		&& !strstr(file_name, ".ttc")
		&& !strstr(file_name, ".TTC") )
	
		return 0;
	
	if (FT_New_Face(ftpriv->library, file_path, 0, & face )) return 0;
	if (!face) return 0;

	num_faces = face->num_faces;
	/*locate right font in collection if several*/
	for (i=0; i<num_faces; i++) {
		if (ft_check_face(face, ftpriv->tmp_font_name, ftpriv->tmp_font_style)) 
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

	/*and store entry in cfg file*/
	if (ftpriv->register_font) {
		char szFont[GF_MAX_PATH];
		strcpy(szFont, face->family_name);
		if (ftpriv->tmp_font_style && strstr(ftpriv->tmp_font_style, "BOLD") && strstr(ftpriv->tmp_font_style, "ITALIC")) {
			strcat(szFont, " Bold Italic");
		} else if (ftpriv->tmp_font_style && strstr(ftpriv->tmp_font_style, "BOLD") ) {
			strcat(szFont, " Bold");
		} else if (ftpriv->tmp_font_style && strstr(ftpriv->tmp_font_style, "ITALIC") ) {
			strcat(szFont, " Italic");
		}
		gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", szFont, file_name);
	}
	return 1;
}

static GF_Err ft_set_font(GF_FontRaster *dr, const char *OrigFontName, const char *styles)
{
	char fname[1024];
	char *fontName;
	GF_Err e;
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;

	fontName = (char *) OrigFontName;
	ftpriv->active_face = NULL;
	ftpriv->strike_style = 0;
	if (styles && strstr(styles, "UNDERLINE")) ftpriv->strike_style = 1;
	else if (styles && strstr(styles, "STRIKE")) ftpriv->strike_style = 2;

	if (fontName) {
		if (!strlen(fontName) || !stricmp(fontName, "SERIF")) fontName = ftpriv->font_serif;
		else if (!stricmp(fontName, "SANS")) fontName = ftpriv->font_sans;
		else if (!stricmp(fontName, "TYPEWRITER")) fontName = ftpriv->font_fixed;
	}

	if (styles && (!stricmp(styles, "PLAIN") || !stricmp(styles, "REGULAR"))) styles = NULL;

	/*first look in loaded fonts*/
	ftpriv->active_face = ft_font_in_cache(ftpriv, fontName, styles);
	if (ftpriv->active_face) {
		return GF_OK;
	}

	ftpriv->tmp_font_name = fontName;
	ftpriv->tmp_font_style = styles;
	ftpriv->register_font = 0;

	/*check cfg file - freetype is slow at loading fonts so we keep the (font name + styles)=fontfile associations
	in the cfg file*/
	if (fontName) {
		const char *opt;
		char file_path[GF_MAX_PATH];
		strcpy(fname, fontName);
		if (styles && strstr(styles, "BOLD") && strstr(styles, "ITALIC")) {
			strcat(fname, " Bold Italic");
		}
		else if (styles && strstr(styles, "BOLD")) {
			strcat(fname, " Bold");
		}
		else if (styles && strstr(styles, "ITALIC")) {
			strcat(fname, " Italic");
		}
		opt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", fname);
		if (opt) {
			strcpy(file_path, ftpriv->font_dir);
			strcat(file_path, opt);
			if (ft_enum_fonts(dr, (char *)opt, file_path)) return GF_OK;
		}
	}

	/*not found, browse all fonts*/
	ftpriv->register_font = 1;
	gf_enum_directory(ftpriv->font_dir, 0, ft_enum_fonts, dr);
	ftpriv->register_font = 0;

	if (ftpriv->active_face) return GF_OK;

	/*try load the first font that has the desired styles*/
	if (fontName) {
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
		e = ft_set_font(dr, NULL, styles);
		if ((e!=GF_OK) && styles) e = ft_set_font(dr, fontName, NULL);
		if (e!=GF_OK) return e;

		/*reassign default - they may be wrong, but this will avoid future browsing*/
		if (!OrigFontName || !stricmp(OrigFontName, "SERIF")) {
			gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif", ftpriv->active_face->family_name);
			strcpy(ftpriv->font_serif, ftpriv->active_face->family_name);
		}
		else if (!stricmp(OrigFontName, "SANS")) {
			gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontSans", ftpriv->active_face->family_name);
			strcpy(ftpriv->font_sans, ftpriv->active_face->family_name);
		}
		else if (!stricmp(OrigFontName, "TYPEWRITTER")) {
			gf_modules_set_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed", ftpriv->active_face->family_name);
			strcpy(ftpriv->font_fixed, ftpriv->active_face->family_name);
		}

		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static GF_Err ft_set_font_size(GF_FontRaster *dr, Fixed pixel_size)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;
	if (!ftpriv->active_face || !pixel_size) return GF_BAD_PARAM;

	ftpriv->pixel_size = pixel_size;

	return GF_OK;
}

static GF_Err ft_get_font_metrics(GF_FontRaster *dr, Fixed *ascent, Fixed *descent, Fixed *lineSpacing)
{
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;
	if (!ftpriv->active_face || !ftpriv->pixel_size) return GF_BAD_PARAM;

	*ascent = (ftpriv->pixel_size / ftpriv->active_face->units_per_EM) * ftpriv->active_face->ascender;
	*descent = - (ftpriv->pixel_size / ftpriv->active_face->units_per_EM) * ftpriv->active_face->descender;
	*lineSpacing = (ftpriv->pixel_size / ftpriv->active_face->units_per_EM) * ftpriv->active_face->height;
	return GF_OK;
}


/*small hack on charmap selection: by default use UNICODE, otherwise use first available charmap*/
static void ft_set_charmap(FT_Face face)
{
	if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
		FT_CharMap *cur = face->charmaps;
		assert(cur);
		face->charmap = cur[0];
	}
}

static GF_Err ft_get_text_size(GF_FontRaster *dr, const unsigned short *string, Fixed *width, Fixed *height)
{
	u32 i, count, glyph_idx, w, h;
	FT_Glyph glyph;
	FT_BBox bbox;
	FTBuilder *ftpriv = (FTBuilder *)dr->priv;
	if (!ftpriv->active_face || !ftpriv->pixel_size) return GF_BAD_PARAM;

	ft_set_charmap(ftpriv->active_face);

	count = gf_utf8_wcslen(string);
	if (count == (size_t) (-1)) return GF_BAD_PARAM;

	w = h = 0;
	for (i=0; i<count; i++) {
		glyph_idx = FT_Get_Char_Index(ftpriv->active_face, string[i]);
		/*missing glyph*/
		if (!glyph_idx) continue;

		/*work in design units*/
		FT_Load_Glyph(ftpriv->active_face, glyph_idx, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);
		FT_Get_Glyph(ftpriv->active_face->glyph, &glyph);

		FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_unscaled, &bbox);		
		if (h < (u32) (bbox.yMax - bbox.yMin)) h = (bbox.yMax - bbox.yMin);

		if (!i) w = -1 * bbox.xMin;
		if (i+1<count) {
			w += ftpriv->active_face->glyph->metrics.horiAdvance;
		} else {
			w += bbox.xMax;
		}
		FT_Done_Glyph(glyph);
	}

	/*convert to font size*/
	*width = (ftpriv->pixel_size / ftpriv->active_face->units_per_EM) * w;
	*height = (ftpriv->pixel_size / ftpriv->active_face->units_per_EM) * h;
	return GF_OK;
}




typedef struct
{
//	Fixed font_scale;
	Fixed top;
	Fixed pos_x;
	FTBuilder *ftpriv;
	GF_Path *path;

	Fixed x_scale, y_scale;
	Fixed last_x, last_y;
} ft_outliner;


#define GETX(_x) (ftol->pos_x + gf_mulfix(ftol->x_scale, INT2FIX(_x)))
#define GETY(_y) (ftol->top + gf_mulfix(ftol->y_scale, INT2FIX(_y)))


/*
	this is not explicit in FreeType doc, i assume each line/curve ending at last moveTo
	shall be closed
*/

int ft_move_to(FT_Vector *to, void *user)
{
	Fixed x, y;
	ft_outliner *ftol = (ft_outliner *)user;
	x = GETX(to->x);
	y = GETY(to->y);
	gf_path_add_move_to(ftol->path, x, y);
	ftol->last_x = x;
	ftol->last_y = y;
	return 0;
}
int ft_line_to(FT_Vector *to, void *user)
{
	Fixed x, y;
	ft_outliner *ftol = (ft_outliner *)user;
	x = GETX(to->x);
	y = GETY(to->y);

	if ( (ftol->last_x == x) && (ftol->last_y == y)) {
		gf_path_close(ftol->path);
	} else {
		gf_path_add_line_to(ftol->path, x, y);
	}
	return 0;
}
int ft_conic_to(FT_Vector * control, FT_Vector *to, void *user)
{
	Fixed x, y, cx, cy;
	ft_outliner *ftol = (ft_outliner *)user;
	cx = GETX(control->x);
	cy = GETY(control->y);
	x = GETX(to->x);
	y = GETY(to->y);
	gf_path_add_quadratic_to(ftol->path, cx, cy, x, y);

	if ( (ftol->last_x == x) && (ftol->last_y == y)) gf_path_close(ftol->path);
	return 0;
}
int ft_cubic_to(FT_Vector *control1, FT_Vector *control2, FT_Vector *to, void *user)
{
	Fixed x, y, c1x, c1y, c2x, c2y;
	ft_outliner *ftol = (ft_outliner *)user;
	c1x = GETX(control1->x);
	c1y = GETY(control1->y);
	c2x = GETX(control2->x);
	c2y = GETY(control2->y);
	x = GETX(to->x);
	y = GETY(to->y);
	gf_path_add_cubic_to(ftol->path, c1x, c1y, c1x, c1y, x, y);
	if ( (ftol->last_x == x) && (ftol->last_y == y)) gf_path_close(ftol->path);
	return 0;
}


static GF_Err ft_add_text_to_path(GF_FontRaster *dr, GF_Path *path, Bool flipText,
					const unsigned short *string, Fixed left, Fixed top, Fixed x_scaling, Fixed y_scaling, 
					Fixed ascent, GF_Rect *bounds)
{

	u32 i, count, glyph_idx;
	Fixed def_inc_x, font_scale;
	s32 ymin, ymax;
	FT_BBox bbox;
	FT_OutlineGlyph outline;
	ft_outliner outl;
	FT_Outline_Funcs ft_outl_funcs;

	FTBuilder *ftpriv = (FTBuilder *)dr->priv;
	if (!ftpriv->active_face || !ftpriv->pixel_size) return GF_BAD_PARAM;

	ft_set_charmap(ftpriv->active_face);

	/*setup outliner*/
	ft_outl_funcs.shift = 0;
	ft_outl_funcs.delta = 0;
	ft_outl_funcs.move_to = ft_move_to;
	ft_outl_funcs.line_to = ft_line_to;
	ft_outl_funcs.conic_to = ft_conic_to;
	ft_outl_funcs.cubic_to = ft_cubic_to;

	/*units per EM are 24.6 fixed point in freetype, consider them as integers (no risk of overflow)*/
	font_scale = ftpriv->pixel_size / ftpriv->active_face->units_per_EM;
	outl.path = path;
	outl.ftpriv = ftpriv;
	
	outl.x_scale = gf_mulfix(x_scaling, font_scale);
	outl.y_scale = gf_mulfix(y_scaling, font_scale);
	if (!flipText) outl.y_scale *= -1;
	
	bounds->x = outl.pos_x = gf_mulfix(left, x_scaling);
	if (flipText) 
		bounds->y = outl.top = gf_mulfix(top - ascent, y_scaling);
	else 
		bounds->y = outl.top = gf_mulfix(top, y_scaling);

	/*same remark as above*/
	def_inc_x = ftpriv->active_face->max_advance_width * outl.x_scale;

	/*TODO: reverse layout (for arabic fonts) and glyph substitution / ligature once openType is in place in freetype*/

	bounds->height = 0;

	count = gf_utf8_wcslen(string);
	if (count == (size_t) (-1)) return GF_BAD_PARAM;
	ymin = ymax	= 0;

	for (i=0; i<count; i++) {
		glyph_idx = FT_Get_Char_Index(ftpriv->active_face, string[i]);
		/*missing glyph*/
		if (!glyph_idx) continue;

		/*work in design units*/
		FT_Load_Glyph(ftpriv->active_face, glyph_idx, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP);

		FT_Get_Glyph(ftpriv->active_face->glyph, (FT_Glyph*)&outline);
		/*oops not vectorial...*/
		if (outline->root.format != FT_GLYPH_FORMAT_OUTLINE) {
			outl.pos_x += def_inc_x;
			FT_Done_Glyph((FT_Glyph) outline);
			continue;
		}

		/*freeType is marvelous and gives back the right advance on space char !!!*/
	    FT_Outline_Decompose(&outline->outline, &ft_outl_funcs, &outl);

		FT_Glyph_Get_CBox((FT_Glyph) outline, ft_glyph_bbox_unscaled, &bbox);		
		if (ymax < bbox.yMax ) ymax = bbox.yMax ;
		if (ymin > bbox.yMin) ymin = bbox.yMin;

		/*update start of bounding box on first char*/
		if (!i) bounds->x += bbox.xMin * outl.x_scale;

		/*take care of last char (may be AFTER last horiz_advanced with certain glyphs)*/
		if ((i+1==count) && (bbox.xMax)) {
			outl.pos_x += bbox.xMax * outl.x_scale;
		} else {
			outl.pos_x += ftpriv->active_face->glyph->metrics.horiAdvance * outl.x_scale;
		}
		FT_Done_Glyph((FT_Glyph) outline);
	}


	bounds->width = outl.pos_x - bounds->x;
	bounds->height = (ymax - ymin) * outl.y_scale;
	bounds->y = ymax * outl.y_scale;
	if (!bounds->height && count) bounds->height = FIX_ONE/1000;

	/*add underline/strikeout*/
	if (ftpriv->strike_style) {
		Fixed sy;
		if (ftpriv->strike_style==1) {
			sy = top - ascent + ftpriv->active_face->underline_position * font_scale;
		} else {
			sy = top - 3 * ascent / 4;
		}
		gf_path_add_rect_center(path, bounds->x + bounds->width / 2, sy, bounds->width, ftpriv->active_face->underline_thickness * font_scale);
	}
	return GF_OK;
}


GF_FontRaster *FT_Load()
{
	GF_FontRaster *dr;
	FTBuilder *ftpriv;
	dr = malloc(sizeof(GF_FontRaster));
	memset(dr, 0, sizeof(GF_FontRaster));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_FONT_RASTER_INTERFACE, "FreeType Font Engine", "gpac distribution");

	ftpriv = malloc(sizeof(FTBuilder));
	memset(ftpriv, 0, sizeof(FTBuilder));

	ftpriv->loaded_fonts = gf_list_new();

	dr->priv = ftpriv;

	
	dr->init_font_engine = ft_init_font_engine;
	dr->shutdown_font_engine = ft_shutdown_font_engine;
	dr->set_font = ft_set_font;
	dr->set_font_size = ft_set_font_size;
	dr->get_font_metrics = ft_get_font_metrics;
	dr->get_text_size = ft_get_text_size;
	dr->add_text_to_path = ft_add_text_to_path;
	
	return dr;
}


void FT_Delete(GF_BaseInterface *ifce)
{
	GF_FontRaster *dr = (GF_FontRaster *) ifce;
	FTBuilder *ftpriv = dr->priv;


	if (ftpriv->font_dir) free(ftpriv->font_dir);
	assert(!gf_list_count(ftpriv->loaded_fonts) );

	gf_list_del(ftpriv->loaded_fonts);

	free(dr->priv);
	free(dr);
}

#ifndef GPAC_STANDALONE_RENDER_2D

Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_FONT_RASTER_INTERFACE) return 1;
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_FONT_RASTER_INTERFACE) return (GF_BaseInterface *)FT_Load();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_FONT_RASTER_INTERFACE:
		FT_Delete(ifce);
		break;
	}
}

#endif

