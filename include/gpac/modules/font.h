/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / modules interfaces
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


#ifndef _GF_MODULE_FONT_H_
#define _GF_MODULE_FONT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/path2d.h>
#include <gpac/module.h>
#include <gpac/user.h>


/*interface name and version for font raster*/
#define GF_FONT_RASTER_INTERFACE		GF_4CC('G','F','R', 0x01)

typedef struct _font_raster
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*inits font engine.*/
	GF_Err (*init_font_engine)(struct _font_raster*dr);
	/*shutdown font engine*/
	GF_Err (*shutdown_font_engine)(struct _font_raster*dr);

	/*set active font . @styles indicates font styles (PLAIN, BOLD, ITALIC, 
	BOLDITALIC and UNDERLINED, STRIKEOUT)*/
	GF_Err (*set_font)(struct _font_raster*dr, const char *fontName, const char *styles);
	/*set active font pixel size*/
	GF_Err (*set_font_size)(struct _font_raster*dr, Fixed pixel_size);
	/*gets font metrics*/
	GF_Err (*get_font_metrics)(struct _font_raster*dr, Fixed *ascent, Fixed *descent, Fixed *lineSpacing);
	/*gets size of the given string (wide char)*/
	GF_Err (*get_text_size)(struct _font_raster*dr, const unsigned short *string, Fixed *width, Fixed *height);

	/*add text to path - graphics driver may be changed at any time, therefore the font engine shall not 
	cache graphics data
		@path: target path
		x and y scaling: string display length control by the compositor, to apply to the text string
		left and top: top-left corner where to place the string (top alignment)
		ascent: offset between @top and baseline - may be needed by some fonts engine
		bounds: output bounds of the added text including white space
		flipText: true = BIFS, false = SVG
	*/
	GF_Err (*add_text_to_path)(struct _font_raster*dr, GF_Path *path, Bool flipText,
					const unsigned short* string, Fixed left, Fixed top, Fixed x_scaling, Fixed y_scaling, 
					Fixed ascent, GF_Rect *bounds);
/*private*/
	void *priv;
} GF_FontRaster;




typedef struct gf_ft_glyph
{
	struct gf_ft_glyph *next;
	u32 utf_name;
	GF_Path *path;
	/*glyph bbox in font EM size*/
	s32 x,y,width,height;
	/*glyph horizontal advance in font EM size*/
	s32 horiz_advance;
	/*glyph vertical advance in font EM size*/
	s32 vert_advance;
} GF_Glyph;

typedef struct gf_ft_font
{
	struct gf_ft_font *next;
	struct gf_ft_glyph *glyph;

	char *name;
	u32 em_size;
	u32 styles;
	/*font uits in em size*/
	s32 ascent, descent, line_spacing, max_advance_h, max_advance_v;
} GF_Font;

enum
{
	GF_FONT_BOLD = 1,
	GF_FONT_ITALIC = 1<<1,
	GF_FONT_UNDERLINED = 1<<2,
};

/*interface name and version for font engine*/
#define GF_FONT_READER_INTERFACE		GF_4CC('G','F','T', 0x01)


typedef struct _font_reader
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*inits font engine.*/
	GF_Err (*init_font_engine)(struct _font_reader *dr);
	/*shutdown font engine*/
	GF_Err (*shutdown_font_engine)(struct _font_reader *dr);

	/*set active font . @styles indicates font styles (PLAIN, BOLD, ITALIC, 
	BOLDITALIC and UNDERLINED, STRIKEOUT)*/
	GF_Err (*set_font)(struct _font_reader *dr, const char *fontName, u32 styles);
	/*gets font info*/
	GF_Err (*get_font_info)(struct _font_reader *dr, char **font_name, s32 *em_size, s32 *ascent, s32 *descent, s32 *line_spacing, s32 *max_advance_h, s32 *max_advance_v);

	/*translate string to glyph sequence*/
	GF_Err (*get_glyphs)(struct _font_reader *dr, const char *utf_string, unsigned short *glyph_buffer, u32 *io_glyph_buffer_size);

	/*loads glyph by name - returns NULL if glyph cannot be found*/
	GF_Glyph *(*load_glyph)(struct _font_reader *dr, u32 glyph_name);


/*module private*/
	void *udta;
} GF_FontReader;



typedef struct
{
	GF_Font *font;
	
	GF_Glyph **glyphs;
	u32 nb_glyphs;

	Fixed font_size;
	Bool horizontal;
	/*scale to apply to get to requested font size*/
	Fixed font_scale;
	GF_Rect bounds;

	/*x and y offset in local coord system*/
	Fixed off_x, off_y;
	Fixed x_scale, y_scale;

	/*per-glyph positioning if any - shall be the same number as the glyphs*/
	Fixed *dx, *dy;
} GF_TextSpan;
typedef struct _gf_ft_mgr GF_FontManager;

GF_FontManager *gf_font_manager_new(GF_User *user);
void gf_font_manager_del(GF_FontManager *fm);
GF_TextSpan *gf_font_manager_create_span(GF_FontManager *fm, GF_Font *font, char *span, Fixed font_size);
void gf_font_manager_delete_span(GF_FontManager *fm, GF_TextSpan *tspan);
GF_Glyph *gf_font_get_glyph(GF_FontManager *fm, GF_Font *font, u32 name);
GF_Font *gf_font_manager_set_font(GF_FontManager *fm, char **alt_fonts, u32 nb_fonts, u32 styles);

#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_FONT_H_*/

