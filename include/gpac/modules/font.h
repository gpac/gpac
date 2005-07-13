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


#define GF_FONT_RASTER_INTERFACE		FOUR_CHAR_INT('G','F','N','T')

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
		x and y scaling: string display length control by the renderer, to apply to the text string
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


#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_FONT_H_*/

