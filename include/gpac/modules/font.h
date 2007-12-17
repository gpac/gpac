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


typedef struct _gf_glyph
{
	/*glyphs are stored as linked lists*/
	struct _gf_glyph *next;
	/*glyph ID as used in *_get_glyphs - this may not match the UTF name*/
	u32 ID;
	/*UTF-name of the glyph if any*/
	u32 utf_name;
	GF_Path *path;
	/*glyph horizontal advance in font EM size*/
	s32 horiz_advance;
	/*glyph vertical advance in font EM size*/
	s32 vert_advance;
} GF_Glyph;

enum
{
	GF_FONT_BOLD = 1,
	GF_FONT_ITALIC = 1<<1,
	GF_FONT_UNDERLINED = 1<<2,
	GF_FONT_STRIKEOUT = 1<<3,
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
	GF_Err (*get_glyphs)(struct _font_reader *dr, const char *utf_string, u32 *glyph_id_buffer, u32 *io_glyph_id_buffer_size, const char *xml_lang);

	/*loads glyph by name - returns NULL if glyph cannot be found*/
	GF_Glyph *(*load_glyph)(struct _font_reader *dr, u32 glyph_name);

/*module private*/
	void *udta;
} GF_FontReader;



#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_FONT_H_*/

