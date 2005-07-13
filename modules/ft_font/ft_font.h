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

#ifndef _FT_FONT_H_
#define _FT_FONT_H_

#include <gpac/modules/font.h>
#include <gpac/list.h>
#include <gpac/utf.h>

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
	const char *tmp_font_style;
	/*default fonts*/
	char font_serif[1024];
	char font_sans[1024];
	char font_fixed[1024];
} FTBuilder;


#endif /*_FT_FONT_H_*/
