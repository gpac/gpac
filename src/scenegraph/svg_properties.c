/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre - Jean-Claude Moissinac
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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

#include <gpac/scenegraph_svg.h>
#include <gpac/internal/scenegraph_dev.h>
 
 /* Sets all SVG Properties to their initial value 
   The properties are then updated when going down the tree using svg_properties_apply
   TODO: Check that all properties are there */
void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props) 
{
	if (!svg_props) return;

	GF_SAFEALLOC(svg_props->fill, sizeof(SVG_Paint));
	svg_props->fill->type = SVG_PAINT_COLOR;
	svg_props->fill->color.type = SVG_COLOR_RGBCOLOR;
	svg_props->fill->color.red = 0;
	svg_props->fill->color.green = 0;
	svg_props->fill->color.blue = 0;

	GF_SAFEALLOC(svg_props->fill_rule, sizeof(SVG_FillRule));
	*svg_props->fill_rule = SVG_FILLRULE_NONZERO;

	GF_SAFEALLOC(svg_props->fill_opacity, sizeof(SVG_Opacity));
	svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->fill_opacity->value = FIX_ONE;
	
	GF_SAFEALLOC(svg_props->stroke, sizeof(SVG_Paint));
	svg_props->stroke->type = SVG_PAINT_NONE;
	svg_props->stroke->color.type = SVG_COLOR_RGBCOLOR;

	GF_SAFEALLOC(svg_props->stroke_opacity, sizeof(SVG_Opacity));
	svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stroke_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_width, sizeof(SVG_StrokeWidth));
	svg_props->stroke_width->type = SVG_NUMBER_VALUE;
	svg_props->stroke_width->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_linecap, sizeof(SVG_StrokeLineCap));
	*(svg_props->stroke_linecap) = SVG_STROKELINECAP_BUTT;
	GF_SAFEALLOC(svg_props->stroke_linejoin, sizeof(SVG_StrokeLineJoin));
	*(svg_props->stroke_linejoin) = SVG_STROKELINEJOIN_MITER;

	GF_SAFEALLOC(svg_props->stroke_miterlimit, sizeof(SVG_StrokeMiterLimit));
	svg_props->stroke_miterlimit->type = SVG_NUMBER_VALUE;
	svg_props->stroke_miterlimit->value = 4*FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_dashoffset , sizeof(SVG_StrokeDashOffset));
	svg_props->stroke_dashoffset->type = SVG_NUMBER_VALUE;
	svg_props->stroke_dashoffset->value = 0;

	GF_SAFEALLOC(svg_props->stroke_dasharray, sizeof(SVG_StrokeDashArray));
	svg_props->stroke_dasharray->type = SVG_STROKEDASHARRAY_NONE;

	GF_SAFEALLOC(svg_props->font_family, sizeof(SVG_FontFamily));
	svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
	svg_props->font_family->value = strdup("Arial");

	GF_SAFEALLOC(svg_props->font_size, sizeof(SVG_FontSize));
	svg_props->font_size->type = SVG_NUMBER_VALUE;
	svg_props->font_size->value = 12*FIX_ONE;

	GF_SAFEALLOC(svg_props->font_style, sizeof(SVG_FontStyle));
	*(svg_props->font_style) = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->color, sizeof(SVG_Paint));
	svg_props->color->type = SVG_PAINT_COLOR;
	svg_props->color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */

	GF_SAFEALLOC(svg_props->text_anchor, sizeof(SVG_TextAnchor));
	*svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->visibility, sizeof(SVG_Visibility));
	*svg_props->visibility = SVG_VISIBILITY_VISIBLE;

	GF_SAFEALLOC(svg_props->display, sizeof(SVG_Display));
	*svg_props->display = SVG_DISPLAY_INLINE;

}

void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props)
{
	if (!svg_props) return;
	gf_svg_delete_paint(svg_props->fill);
	if(svg_props->fill_rule) free(svg_props->fill_rule);
	if(svg_props->fill_opacity) free(svg_props->fill_opacity);
	gf_svg_delete_paint(svg_props->stroke);
	if(svg_props->stroke_opacity) free(svg_props->stroke_opacity);
	if(svg_props->stroke_width) free(svg_props->stroke_width);
	if(svg_props->stroke_linecap) free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_dashoffset) free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) free(svg_props->stroke_dasharray->array.vals);
		free(svg_props->stroke_dasharray);
	}
	if(svg_props->font_family) {
		if (svg_props->font_family->value) free(svg_props->font_family->value);
		free(svg_props->font_family);
	}
	if(svg_props->font_size) free(svg_props->font_size);
	if(svg_props->font_style) free(svg_props->font_style);
	gf_svg_delete_paint(svg_props->color);
	if(svg_props->text_anchor) free(svg_props->text_anchor);
	if(svg_props->visibility) free(svg_props->visibility);
	if(svg_props->display) free(svg_props->display);
	memset(svg_props, 0, sizeof(SVGPropertiesPointers));
}

/* Updates the SVG Styling Properties Pointers of the renderer (render_svg_props) with the properties
   of the current SVG element (current_svg_props). Only the properties in current_svg_props 
   with a value different than inherit are updated.
   This function implements inheritance. 
   TODO: Check if all properties are implemented */
void gf_svg_properties_apply(SVGPropertiesPointers *render_svg_props, SVGProperties *current_svg_props)
{
	if (!render_svg_props) return;

	if (current_svg_props->color.color.type != SVG_COLOR_INHERIT) {
		render_svg_props->color = &current_svg_props->color;
	}
	if (current_svg_props->fill.type != SVG_PAINT_INHERIT) {
		render_svg_props->fill = &(current_svg_props->fill);
	}
	if (current_svg_props->fill_rule != SVG_FILLRULE_INHERIT) {
		render_svg_props->fill_rule = &current_svg_props->fill_rule;
	}
	if (current_svg_props->fill_opacity.type != SVG_NUMBER_INHERIT) {
		render_svg_props->fill_opacity = &current_svg_props->fill_opacity;
	}
	if (current_svg_props->stroke.type != SVG_PAINT_INHERIT) {
		render_svg_props->stroke = &current_svg_props->stroke;
	}
	if (current_svg_props->stroke_opacity.type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_opacity = &current_svg_props->stroke_opacity;
	}
	if (current_svg_props->stroke_width.type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_width = &current_svg_props->stroke_width;
	}
	if (current_svg_props->stroke_miterlimit.type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_miterlimit = &current_svg_props->stroke_miterlimit;
	}
	if (current_svg_props->stroke_linecap != SVG_STROKELINECAP_INHERIT) {
		render_svg_props->stroke_linecap = &current_svg_props->stroke_linecap;
	}
	if (current_svg_props->stroke_linejoin != SVG_STROKELINEJOIN_INHERIT) {
		render_svg_props->stroke_linejoin = &current_svg_props->stroke_linejoin;
	}
	if (current_svg_props->stroke_dashoffset.type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_dashoffset = &current_svg_props->stroke_dashoffset;
	}
	if (current_svg_props->stroke_dasharray.type != SVG_STROKEDASHARRAY_INHERIT) {
		render_svg_props->stroke_dasharray = &current_svg_props->stroke_dasharray;
	}
	if (current_svg_props->font_family.type != SVG_FONTFAMILY_INHERIT) {
		render_svg_props->font_family = &current_svg_props->font_family;
	}
	if (current_svg_props->font_size.type != SVG_NUMBER_INHERIT) {
		render_svg_props->font_size = &current_svg_props->font_size;
	}
	if (current_svg_props->font_style != SVG_FONTSTYLE_INHERIT) {
		render_svg_props->font_style = &current_svg_props->font_style;
	}
	if (current_svg_props->text_anchor != SVG_TEXTANCHOR_INHERIT) {
		render_svg_props->text_anchor = &current_svg_props->text_anchor;
	}
	if (current_svg_props->visibility != SVG_VISIBILITY_INHERIT) {
		render_svg_props->visibility = &current_svg_props->visibility;
	}
	if (current_svg_props->display != SVG_DISPLAY_INHERIT) {
		render_svg_props->display = &current_svg_props->display;
	}
}