/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / GDIplus rasterizer module
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

#include "gdip_priv.h"
#include <gpac/utf.h>



#ifndef GDIP_MAX_STRING_SIZE
#define GDIP_MAX_STRING_SIZE	5000
#endif


GF_Err gdip_init_font_engine(GF_FontReader *dr)
{
	const char *sOpt;
	FontPriv *ctx = (FontPriv *)dr->udta;

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif");
	strcpy(ctx->font_serif, sOpt ? sOpt : "Times New Roman");
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSans");
	strcpy(ctx->font_sans, sOpt ? sOpt : "Arial");
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed");
	strcpy(ctx->font_fixed, sOpt ? sOpt : "Courier New");

	return GF_OK;
}
GF_Err gdip_shutdown_font_engine(GF_FontReader *dr)
{
	FontPriv *ctx = (FontPriv *)dr->udta;

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;

	/*nothing to do*/
	return GF_OK;
}



static GF_Err gdip_get_glyphs(GF_FontReader *dr, const char *utf_string, u32 *glyph_buffer, u32 *io_glyph_buffer_size, const char *xml_lang, Bool *is_rtl)
{
	size_t _len;
	u32 len;
	u32 i;
	u16 *conv;
	char *utf8 = (char*) utf_string;
	FontPriv *priv = (FontPriv*)dr->udta;

	len = utf_string ? (u32) strlen(utf_string) : 0;
	if (!len) {
		*io_glyph_buffer_size = 0;
		return GF_OK;
	}
	if (*io_glyph_buffer_size < len+1) {
		*io_glyph_buffer_size = len+1;
		return GF_BUFFER_TOO_SMALL;
	}
	_len = gf_utf8_mbstowcs((u16*) glyph_buffer, *io_glyph_buffer_size, (const char **) &utf8);
	if (_len==(size_t)-1) return GF_IO_ERR;
	len = (u32) _len;
	if (utf8) return GF_IO_ERR;

	/*perform bidi relayout*/
	conv = (u16*) glyph_buffer;
	*is_rtl = gf_utf8_reorder_bidi(conv, len);
	/*move 16bit buffer to 32bit*/
	for (i=len; i>0; i--) {
		glyph_buffer[i-1] = (u32) conv[i-1];
	}
	*io_glyph_buffer_size = (u32) len;
	return GF_OK;
}


static void adjust_white_space(const unsigned short *string, Float *width, Float whiteSpaceWidth)
{
	u32 len , i=0;
	while (string[i] == (unsigned short) ' ') {
		*width += whiteSpaceWidth;
		i++;
	}
	if (whiteSpaceWidth<0) return;
	len = (u32) gf_utf8_wcslen(string);
	if (i != len) {
		i = len - 1;
		while (string[i] == (unsigned short) ' ') {
			*width += whiteSpaceWidth;
			i--;
		}
	}
}

static GF_Err gdip_get_text_size(GF_FontReader *dr, const unsigned short *string, Fixed *width, Fixed *height)
{
	GpPath *path_tmp;
	GpStringFormat *fmt;
	FontPriv *ctx = (FontPriv *)dr->udta;
	*width = *height = 0;
	if (!ctx->font) return GF_BAD_PARAM;

	GdipCreateStringFormat(StringFormatFlagsNoWrap, LANG_NEUTRAL, &fmt);
	GdipCreatePath(FillModeAlternate, &path_tmp);
	RectF rc;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;
	GdipAddPathString(path_tmp, (const WCHAR *)string, -1, ctx->font, ctx->font_style, ctx->em_size, &rc, fmt);

	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);

	adjust_white_space(string, &rc.Width, ctx->whitespace_width);
	*width = FLT2FIX(rc.Width);
	*height = FLT2FIX(rc.Height);

	GdipDeleteStringFormat(fmt);
	GdipDeletePath(path_tmp);

	return GF_OK;
}

static GF_Err gdip_set_font(GF_FontReader *dr, const char *fontName, u32 styles)
{
	WCHAR wcFontName[GDIP_MAX_STRING_SIZE];
	FontPriv *ctx = (FontPriv *)dr->udta;

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;

	if (fontName && strlen(fontName) >= GDIP_MAX_STRING_SIZE) fontName = NULL;

	if (!fontName || !strlen(fontName) ) fontName = ctx->font_serif;
	else if (!stricmp(fontName, "SANS") || !stricmp(fontName, "sans-serif")) fontName = ctx->font_sans;
	else if (!stricmp(fontName, "SERIF")) fontName = ctx->font_serif;
	else if (!stricmp(fontName, "TYPEWRITER") || !stricmp(fontName, "monospace")) fontName = ctx->font_fixed;

	MultiByteToWideChar(CP_ACP, 0, fontName, (u32)strlen(fontName)+1,
	                    wcFontName, sizeof(wcFontName)/sizeof(wcFontName[0]) );


	GdipCreateFontFamilyFromName(wcFontName, NULL, &ctx->font);
	if (!ctx->font) return GF_NOT_SUPPORTED;

	//setup styles
	ctx->font_style = 0;
	if (styles & GF_FONT_WEIGHT_BOLD ) ctx->font_style |= FontStyleBold;
	if (styles & GF_FONT_ITALIC) ctx->font_style |= FontStyleItalic;

	if (styles & GF_FONT_UNDERLINED) ctx->font_style |= FontStyleUnderline;
	if (styles & GF_FONT_STRIKEOUT) ctx->font_style |= FontStyleStrikeout;
	return GF_OK;
}

static GF_Err gdip_get_font_info(GF_FontReader *dr, char **font_name, u32 *em_size, s32 *ascent, s32 *descent, s32 *underline, s32 *line_spacing, s32 *max_advance_h, s32 *max_advance_v)
{
	UINT16 val, em;
	FontPriv *ctx = (FontPriv *)dr->udta;

	*font_name = NULL;
	*em_size = *ascent = *descent = *line_spacing = *max_advance_h = *max_advance_v = 0;
	if (!ctx->font) return GF_BAD_PARAM;

	GdipGetEmHeight(ctx->font, ctx->font_style, &em);
	*em_size = (s32) em;
	GdipGetCellAscent(ctx->font, ctx->font_style, &val);
	ctx->ascent = (Float) val;
	*ascent = (s32) val;
	GdipGetCellDescent(ctx->font, ctx->font_style, &val);
	*descent = (s32) val;
	*descent *= -1;
	ctx->descent = -1 * (Float) val;
	*underline = *descent / 2;
	GdipGetLineSpacing(ctx->font, ctx->font_style, &val);
	*line_spacing = (s32) val;
	*max_advance_v = *ascent - *descent;


	unsigned short test_str[4];
	Fixed w, h, w2;
	ctx->em_size = (Float) *em_size;
	test_str[0] = (unsigned short) '_';
	test_str[1] = (unsigned short) '\0';
	gdip_get_text_size(dr, test_str, &w, &h);
	ctx->underscore_width = FIX2FLT(w);

	test_str[0] = (unsigned short) '_';
	test_str[1] = (unsigned short) ' ';
	test_str[2] = (unsigned short) '_';
	test_str[3] = (unsigned short) '\0';
	gdip_get_text_size(dr, test_str, &w2, &h);
	ctx->whitespace_width = FIX2FLT(w2 - 2*w);

	*max_advance_h = (s32) MAX(ctx->underscore_width, ctx->whitespace_width);
	return GF_OK;
}



static GF_Glyph *gdip_load_glyph(GF_FontReader *dr, u32 glyph_name)
{
	GF_Rect bounds;
	GF_Glyph *glyph;
	GpPath *path_tmp;
	GpStringFormat *fmt;
	GpMatrix *mat;
	Float est_advance_h;
	unsigned short str[4];
	int i;
	FontPriv *ctx = (FontPriv *)dr->udta;

	if (!ctx->font) return NULL;

	RectF rc;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;

	GdipCreateStringFormat(StringFormatFlagsNoWrap | StringFormatFlagsNoFitBlackBox | StringFormatFlagsMeasureTrailingSpaces, LANG_NEUTRAL, &fmt);
	GdipSetStringFormatAlign(fmt, StringAlignmentNear);
	GdipCreatePath(FillModeAlternate, &path_tmp);

	if (glyph_name==0x20) {
		est_advance_h = ctx->whitespace_width;
	} else {
		/*to compute first glyph alignment (say 'x', we figure out its bounding full box by using the '_' char as wrapper (eg, "_x_")
		then the bounding box starting from xMin of the glyph ('x_'). The difference between both will give us a good approx
		of the glyph alignment*/
		str[0] = glyph_name;
		str[1] = (unsigned short) '_';
		str[2] = (unsigned short) 0;
		GdipAddPathString(path_tmp, (const WCHAR *)str, -1, ctx->font, ctx->font_style, ctx->em_size, &rc, fmt);
		GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);
		est_advance_h = rc.Width - ctx->underscore_width;
	}

	GdipResetPath(path_tmp);

	str[0] = glyph_name;
	str[1] = (unsigned short) 0;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;
	GdipAddPathString(path_tmp, (const WCHAR *)str, -1, ctx->font, ctx->font_style, ctx->em_size, &rc, fmt);

	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);

	/*flip so that we are in a font coordinate system - also move back the glyph to x=0 and y=baseline, GdiPlus doesn't do so*/
	GdipCreateMatrix(&mat);
	GdipTranslateMatrix(mat, - rc.X, -ctx->ascent, MatrixOrderAppend);
	GdipScaleMatrix(mat, 1, -1, MatrixOrderAppend);
	GdipTransformPath(path_tmp, mat);
	GdipDeleteMatrix(mat);


	/*start enum*/
	s32 count;
	GdipGetPointCount(path_tmp, &count);
	GpPointF *pts = new GpPointF[count];
	BYTE *types = new BYTE[count];
	GdipGetPathTypes(path_tmp, types, count);
	GdipGetPathPoints(path_tmp, pts, count);

	GF_SAFEALLOC(glyph, GF_Glyph);
	GF_SAFEALLOC(glyph->path, GF_Path);

	for (i=0; i<count; ) {
		BOOL closed = 0;
		s32 sub_type;

		sub_type = types[i] & PathPointTypePathTypeMask;

		if (sub_type == PathPointTypeStart) {
			gf_path_add_move_to(glyph->path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y));
			i++;
		}
		else if (sub_type == PathPointTypeLine) {
			gf_path_add_line_to(glyph->path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y));

			if (types[i] & PathPointTypeCloseSubpath) gf_path_close(glyph->path);

			i++;
		}
		else if (sub_type == PathPointTypeBezier) {
			assert(i+2<=count);
			gf_path_add_cubic_to(glyph->path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y), FLT2FIX(pts[i+1].X), FLT2FIX(pts[i+1].Y), FLT2FIX(pts[i+2].X), FLT2FIX(pts[i+2].Y));

			if (types[i+2] & PathPointTypeCloseSubpath) gf_path_close(glyph->path);

			i += 3;
		} else {
			assert(0);
			break;
		}
	}

	delete [] pts;
	delete [] types;
	GdipDeleteStringFormat(fmt);
	GdipDeletePath(path_tmp);

	glyph->ID = glyph_name;
	glyph->utf_name = glyph_name;
	glyph->vert_advance = (s32) (ctx->ascent-ctx->descent);
	glyph->horiz_advance = (s32) est_advance_h;
	gf_path_get_bounds(glyph->path, &bounds);
	glyph->width = FIX2INT(bounds.width);
	glyph->height = FIX2INT(bounds.height);
	return glyph;
}




GF_FontReader *gdip_new_font_driver()
{
	GdiplusStartupInput startupInput;
	GF_FontReader *dr;
	FontPriv *ctx;

	SAFEALLOC(ctx, FontPriv);
	SAFEALLOC(dr, GF_FontReader);
	GdiplusStartup(&ctx->gdiToken, &startupInput, NULL);

	GF_REGISTER_MODULE_INTERFACE(dr, GF_FONT_READER_INTERFACE, "GDIplus Font Reader", "gpac distribution")
	dr->init_font_engine = gdip_init_font_engine;
	dr->shutdown_font_engine = gdip_shutdown_font_engine;
	dr->set_font = gdip_set_font;
	dr->get_font_info = gdip_get_font_info;
	dr->get_glyphs = gdip_get_glyphs;
	dr->load_glyph = gdip_load_glyph;

	dr->udta = ctx;
	return dr;
}

void gdip_delete_font_driver(GF_FontReader *dr)
{
	FontPriv *ctx = (FontPriv *)dr->udta;
	GdiplusShutdown(ctx->gdiToken);

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;

	gf_free(dr->udta);
	gf_free(dr);
}

#ifdef __cplusplus
extern "C" {
#endif

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_FONT_READER_INTERFACE,
		GF_RASTER_2D_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType==GF_FONT_READER_INTERFACE) return (GF_BaseInterface *)gdip_new_font_driver();
	if (InterfaceType==GF_RASTER_2D_INTERFACE) return (GF_BaseInterface *)gdip_LoadRenderer();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_FONT_READER_INTERFACE:
		gdip_delete_font_driver((GF_FontReader *)ifce);
		break;
	case GF_RASTER_2D_INTERFACE:
		gdip_ShutdownRenderer((GF_Raster2D *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( gdiplus )

#ifdef __cplusplus
}
#endif
