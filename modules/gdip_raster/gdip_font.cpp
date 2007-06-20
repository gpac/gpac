/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


GF_Err gdip_init_font_engine(GF_FontRaster *dr)
{
	const char *sOpt;
	FontPriv *ctx = (FontPriv *)dr->priv;

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSerif");
	strcpy(ctx->font_serif, sOpt ? sOpt : "Times New Roman");
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontSans");
	strcpy(ctx->font_sans, sOpt ? sOpt : "Arial");
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "FontEngine", "FontFixed");
	strcpy(ctx->font_fixed, sOpt ? sOpt : "Courier New");

	return GF_OK;
}
GF_Err gdip_shutdown_font_engine(GF_FontRaster *dr)
{
	FontPriv *ctx = (FontPriv *)dr->priv;

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;
	
	/*nothing to do*/
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
	len = gf_utf8_wcslen(string);
	if (i != len) {
		i = len - 1;
		while (string[i] == (unsigned short) ' ') {
			*width += whiteSpaceWidth;
			i--;
		}
	}
}

static GF_Err gdip_get_font_metrics(GF_FontRaster *dr, Fixed *ascent, Fixed *descent, Fixed *lineSpacing)
{
	UINT16 val, em;
	FontPriv *ctx = (FontPriv *)dr->priv;

	*ascent = *descent = *lineSpacing = 0;
	if (!ctx->font) return GF_BAD_PARAM;


	GdipGetEmHeight(ctx->font, ctx->font_style, &em);
	GdipGetCellAscent(ctx->font, ctx->font_style, &val);
	*ascent = FLT2FIX( ctx->font_size * val / em );

	GdipGetCellDescent(ctx->font, ctx->font_style, &val);
	*descent = FLT2FIX( ctx->font_size * val / em);

	GdipGetLineSpacing(ctx->font, ctx->font_style, &val);
	*lineSpacing = FLT2FIX( ctx->font_size * val / em);
	return GF_OK;
}

static GF_Err gdip_get_text_size(GF_FontRaster *dr, const unsigned short *string, Fixed *width, Fixed *height)
{
	GpPath *path_tmp;
	GpStringFormat *fmt;
	FontPriv *ctx = (FontPriv *)dr->priv;
	*width = *height = 0;
	if (!ctx->font) return GF_BAD_PARAM;

	GdipCreateStringFormat(StringFormatFlagsNoWrap, LANG_NEUTRAL, &fmt);
	GdipCreatePath(FillModeAlternate, &path_tmp);
	RectF rc;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;
	GdipAddPathString(path_tmp, (const WCHAR *)string, -1, ctx->font, ctx->font_style, FIX2FLT(ctx->font_size), &rc, fmt);

	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);

	adjust_white_space(string, &rc.Width, ctx->whitespace_width);
	*width = FLT2FIX(rc.Width);
	*height = FLT2FIX(rc.Height);

	GdipDeleteStringFormat(fmt);
	GdipDeletePath(path_tmp);
	
	return GF_OK;
}


static GF_Err gdip_add_text_to_path(GF_FontRaster *dr, GF_Path *path, Bool flipText,
					const unsigned short* string, Fixed _left, Fixed _top, Fixed _x_scaling, Fixed _y_scaling, 
					Fixed _ascent, GF_Rect *bounds)
{
	GpPath *path_tmp;
	GpMatrix *mat;
	GpStringFormat *fmt;
	Float real_start;
	unsigned short str[4];
	Float left, top, x_scaling, y_scaling, ascent;
	FontPriv *ctx = (FontPriv *)dr->priv;

	if (!ctx->font) return GF_BAD_PARAM;

	left = FIX2FLT(_left);
	top = FIX2FLT(_top);
	x_scaling = FIX2FLT(_x_scaling);
	y_scaling = FIX2FLT(_y_scaling);
	ascent = FIX2FLT(_ascent);

	
	RectF rc;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;

	/*find first non-space char and estimate its glyph pos since GDIplus doesn't give this info*/
	s32 len = gf_utf8_wcslen(string);
	s32 i=0;
	for (; i<len; i++) {
		if (string[i] != (unsigned short) ' ') break;
	}
	if (i>=len) return GF_OK;

	GdipCreateStringFormat(StringFormatFlagsNoWrap | StringFormatFlagsNoFitBlackBox | StringFormatFlagsMeasureTrailingSpaces, LANG_NEUTRAL, &fmt);
	GdipSetStringFormatAlign(fmt, StringAlignmentNear);
	GdipCreatePath(FillModeAlternate, &path_tmp);

	/*to compute first glyph alignment (say 'x', we figure out its bounding full box by using the '_' char as wrapper (eg, "_x_")
	then the bounding box starting from xMin of the glyph ('x_'). The difference between both will give us a good approx 
	of the glyph alignment*/
	str[0] = (unsigned short) '_';
	str[1] = string[i];
	str[2] = (unsigned short) '_';
	str[3] = (unsigned short) 0;
	GdipAddPathString(path_tmp, (const WCHAR *)str, -1, ctx->font, ctx->font_style, FIX2FLT(ctx->font_size), &rc, fmt);
	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);
	Float w1 = rc.Width - 2 * ctx->underscore_width;
	
	GdipResetPath(path_tmp);

	str[0] = string[i];
	str[1] = (unsigned short) '_';
	str[2] = (unsigned short) 0;
	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;
	GdipAddPathString(path_tmp, (const WCHAR *)str, -1, ctx->font, ctx->font_style, FIX2FLT(ctx->font_size), &rc, fmt);
	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);
	real_start = w1 - (rc.Width - ctx->underscore_width);

	GdipResetPath(path_tmp);

	rc.X = rc.Y = 0;
	rc.Width = rc.Height = 0;
	GdipAddPathString(path_tmp, (const WCHAR *)string, -1, ctx->font, ctx->font_style, FIX2FLT(ctx->font_size), &rc, fmt);
	GdipGetPathWorldBounds(path_tmp, &rc, NULL, NULL);


	GdipCreateMatrix(&mat);
	Float off_left = rc.X;
	Float off_left_real = rc.X;

	/*adjust all white space at begin*/
	adjust_white_space(string, &off_left, -1*ctx->whitespace_width);

	if (flipText) {
		/*first translate in local system*/
		GdipTranslateMatrix(mat, left - off_left + real_start, -top, MatrixOrderAppend);
		/*then scale as specified*/
		GdipScaleMatrix(mat, x_scaling, -y_scaling, MatrixOrderAppend);
	} else {
		/*first translate in local system*/
		GdipTranslateMatrix(mat, left - off_left + real_start, top-ascent, MatrixOrderAppend);
		/*then scale as specified*/
		GdipScaleMatrix(mat, x_scaling, y_scaling, MatrixOrderAppend);
	}
	GdipTransformPath(path_tmp, mat);

	/*start enum*/
	s32 count;
	GdipGetPointCount(path_tmp, &count);
	GpPointF *pts = new GpPointF[count];
	BYTE *types = new BYTE[count];
	GdipGetPathTypes(path_tmp, types, count);
	GdipGetPathPoints(path_tmp, pts, count);

	for (i=0; i<count; ) {
		BOOL closed = 0;
		s32 sub_type;
		
		sub_type = types[i] & PathPointTypePathTypeMask;

		if (sub_type == PathPointTypeStart) {
			gf_path_add_move_to(path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y));
			i++;
		}
		else if (sub_type == PathPointTypeLine) {
			gf_path_add_line_to(path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y));
		
			if (types[i] & PathPointTypeCloseSubpath) gf_path_close(path);

			i++;
		}
		else if (sub_type == PathPointTypeBezier) {
			assert(i+2<=count);
			gf_path_add_cubic_to(path, FLT2FIX(pts[i].X), FLT2FIX(pts[i].Y), FLT2FIX(pts[i+1].X), FLT2FIX(pts[i+1].Y), FLT2FIX(pts[i+2].X), FLT2FIX(pts[i+2].Y));

			if (types[i+2] & PathPointTypeCloseSubpath) gf_path_close(path);

			i += 3;
		} else {
			assert(0);
			break;
		}
	}
	
	delete [] pts;
	delete [] types;
	
	GdipResetPath(path_tmp);
	adjust_white_space(string, &rc.Width, ctx->whitespace_width);
	rc.X = off_left_real;
	rc.X = off_left;
	/*special case where string is just space*/
	if (!rc.Height) rc.Height = 1;

	GdipAddPathRectangles(path_tmp, &rc, 1);
	GdipGetPathWorldBounds(path_tmp, &rc, mat, NULL);
	bounds->x = FLT2FIX(rc.X);
	bounds->y = FLT2FIX(rc.Y);
	bounds->width = FLT2FIX(rc.Width);
	bounds->height = FLT2FIX(rc.Height);

	GdipDeleteStringFormat(fmt);
	GdipDeletePath(path_tmp);
	GdipDeleteMatrix(mat);
	return GF_OK;
}

static GF_Err gdip_set_font(GF_FontRaster *dr, const char *fontName, const char *styles)
{
	WCHAR wcFontName[GDIP_MAX_STRING_SIZE];
	FontPriv *ctx = (FontPriv *)dr->priv;

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;

	if (fontName && strlen(fontName) >= GDIP_MAX_STRING_SIZE) fontName = NULL;

	if (!fontName || !strlen(fontName) ) fontName = ctx->font_serif;
	else if (!stricmp(fontName, "SANS") || !stricmp(fontName, "sans-serif")) fontName = ctx->font_sans;
	else if (!stricmp(fontName, "SERIF")) fontName = ctx->font_serif;
	else if (!stricmp(fontName, "TYPEWRITER") || !stricmp(fontName, "monospace")) fontName = ctx->font_fixed;

	MultiByteToWideChar(CP_ACP, 0, fontName, strlen(fontName)+1, 
						wcFontName, sizeof(wcFontName)/sizeof(wcFontName[0]) );


	GdipCreateFontFamilyFromName(wcFontName, NULL, &ctx->font);
	if (!ctx->font) return GF_NOT_SUPPORTED;

	//setup styles
	ctx->font_style = 0;
	if (styles) {
		char *upr_styles = strdup(styles);
		strupr(upr_styles);
		if (strstr(upr_styles, "BOLDITALIC")) ctx->font_style |= FontStyleBoldItalic;
		else if (strstr(upr_styles, "BOLD")) ctx->font_style |= FontStyleBold;
		else if (strstr(upr_styles, "ITALIC")) ctx->font_style |= FontStyleItalic;
		
		if (strstr(upr_styles, "UNDERLINED")) {
			ctx->font_style |= FontStyleUnderline;
		}
		if (strstr(upr_styles, "STRIKEOUT")) {
			ctx->font_style |= FontStyleStrikeout;
		}
		free(upr_styles);
	}	

	return GF_OK;
}


static GF_Err gdip_set_font_size(GF_FontRaster *dr, Fixed pixel_size)
{
	unsigned short test_str[4];
	FontPriv *ctx = (FontPriv *)dr->priv;
	if (!ctx->font) return GF_BAD_PARAM;

	ctx->font_size = FIX2FLT(pixel_size);

	/*GDI+ won't return begin/end whitespace info through GetBoundingRect, so compute a default value for space...*/
	ctx->whitespace_width = 0;
	Fixed w, h, w2;
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
	return GF_OK;
}


GF_FontRaster *gdip_new_font_driver()
{
	GdiplusStartupInput startupInput;
	GF_FontRaster *dr;
	FontPriv *ctx;

	SAFEALLOC(ctx, FontPriv);
	SAFEALLOC(dr, GF_FontRaster);
	GdiplusStartup(&ctx->gdiToken, &startupInput, NULL);

	GF_REGISTER_MODULE_INTERFACE(dr, GF_FONT_RASTER_INTERFACE, "GDIplus Font Engine", "gpac distribution")
	dr->add_text_to_path = gdip_add_text_to_path;
	dr->get_font_metrics = gdip_get_font_metrics;
	dr->get_text_size = gdip_get_text_size;
	dr->init_font_engine = gdip_init_font_engine;
	dr->set_font = gdip_set_font;
	dr->set_font_size = gdip_set_font_size;
	dr->shutdown_font_engine = gdip_shutdown_font_engine;
	dr->priv = ctx;
	return dr;
}

void gdip_delete_font_driver(GF_FontRaster *dr)
{
	FontPriv *ctx = (FontPriv *)dr->priv;
	GdiplusShutdown(ctx->gdiToken);

	if (ctx->font) GdipDeleteFontFamily(ctx->font);
	ctx->font = NULL;

	free(dr->priv);
	free(dr);
}



Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_FONT_RASTER_INTERFACE) return 1;
	if (InterfaceType == GF_RASTER_2D_INTERFACE) return 1;
	return 0;
}

GF_Raster2D *gdip_LoadRenderer();
void gdip_ShutdownRenderer(GF_Raster2D *driver);

GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType==GF_FONT_RASTER_INTERFACE) return (GF_BaseInterface *)gdip_new_font_driver();
	if (InterfaceType==GF_RASTER_2D_INTERFACE) return (GF_BaseInterface *)gdip_LoadRenderer();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_FONT_RASTER_INTERFACE:
		gdip_delete_font_driver((GF_FontRaster *)ifce);
		break;
	case GF_RASTER_2D_INTERFACE:
		gdip_ShutdownRenderer((GF_Raster2D *)ifce);
		break;
	}
}

