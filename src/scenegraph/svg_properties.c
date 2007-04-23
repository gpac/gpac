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
#include <gpac/nodes_svg_da.h>

/* 
	Initialization of properties at the element level 
	  -	If a property is inheritable by default ('inherit: yes' in the property definition), 
		the default value should be inherit.
	  -	If a property is not inheritable by default ('inherit: no' in the property definition), 
		it should have the initial value.
*/
#if 1
void gf_svg_init_properties(SVGElement *p) 
{
	GF_SAFEALLOC(p->properties, SVGProperties)

	p->properties->audio_level.type = SVG_NUMBER_VALUE;
	p->properties->audio_level.value = FIX_ONE;
	
	p->properties->color.type = SVG_PAINT_COLOR;
	p->properties->color.color.type = SVG_COLOR_INHERIT;

	p->properties->color_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->display = SVG_DISPLAY_INLINE;

	p->properties->display_align = SVG_DISPLAYALIGN_INHERIT;

	p->properties->fill.type = SVG_PAINT_INHERIT;

	p->properties->fill_opacity.type = SVG_NUMBER_INHERIT;
	
	p->properties->fill_rule = SVG_FILLRULE_INHERIT;

	p->properties->font_family.type = SVG_FONTFAMILY_INHERIT;

	p->properties->font_size.type = SVG_NUMBER_INHERIT;

	p->properties->font_style = SVG_FONTSTYLE_INHERIT;

	p->properties->font_variant = SVG_FONTVARIANT_INHERIT;

	p->properties->font_weight = SVG_FONTWEIGHT_INHERIT;

	p->properties->image_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->line_increment.type = SVG_NUMBER_INHERIT;

	p->properties->opacity.type = SVG_NUMBER_VALUE;
	p->properties->opacity.value = FIX_ONE;

	p->properties->pointer_events = SVG_POINTEREVENTS_INHERIT;

	p->properties->shape_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->solid_color.type = SVG_PAINT_COLOR;
	p->properties->solid_color.color.type = SVG_COLOR_RGBCOLOR;
	/*
	p->properties->solid_color.color.red = 0;
	p->properties->solid_color.color.green = 0;
	p->properties->solid_color.color.blue = 0;
	*/

	p->properties->solid_opacity.type = SVG_NUMBER_VALUE;
	p->properties->solid_opacity.value = FIX_ONE;

	p->properties->stop_color.type = SVG_PAINT_COLOR;
	p->properties->stop_color.color.type = SVG_COLOR_RGBCOLOR;
	/*
	p->properties->stop_color.color.red = 0;
	p->properties->stop_color.color.green = 0;
	p->properties->stop_color.color.blue = 0;
	*/

	p->properties->stop_opacity.type = SVG_NUMBER_VALUE;
	p->properties->stop_opacity.value = FIX_ONE;

	p->properties->stroke.type = SVG_PAINT_INHERIT;
	
	p->properties->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	
	p->properties->stroke_dashoffset.type = SVG_NUMBER_INHERIT;

	p->properties->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	
	p->properties->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	
	p->properties->stroke_miterlimit.type = SVG_NUMBER_INHERIT;
	
	p->properties->stroke_opacity.type = SVG_NUMBER_INHERIT;
	
	p->properties->stroke_width.type = SVG_NUMBER_INHERIT;
	
	p->properties->text_align = SVG_TEXTALIGN_INHERIT;
	
	p->properties->text_anchor = SVG_TEXTANCHOR_INHERIT;
	
	p->properties->text_rendering = SVG_RENDERINGHINT_INHERIT;

	p->properties->vector_effect = SVG_VECTOREFFECT_NONE;

	p->properties->viewport_fill.type = SVG_PAINT_NONE;
	
	p->properties->viewport_fill_opacity.type = SVG_NUMBER_VALUE;
	p->properties->viewport_fill_opacity.value = FIX_ONE;
	
	p->properties->visibility = SVG_VISIBILITY_INHERIT;

/*
	This is the list of attributes which either inherit a value from the parent when not specified, 
	or explicitely use an inherit value, even though they are not CSS properties:
	contentScriptType
	externalResourcesRequired
	syncBehaviorDefault
	syncToleranceDefault
	xml:base
	xml:lang
	xml:space
  
*/
}

#else
void gf_svg_init_properties(SVGElement *p) { }
#endif
/* 
	Initialization of properties at the top level before any rendering 
	The value shall not use the 'inherit' value, it uses the initial value.
    The property values are then updated when going down the tree using svg_properties_apply 
*/
GF_EXPORT
void gf_svg_properties_init_pointers(SVGPropertiesPointers *svg_props) 
{
	if (!svg_props) return;

	GF_SAFEALLOC(svg_props->audio_level, SVG_Number);
	svg_props->audio_level->type = SVG_NUMBER_VALUE;
	svg_props->audio_level->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->color, SVG_Paint);
	svg_props->color->type = SVG_PAINT_COLOR;
	svg_props->color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */

	GF_SAFEALLOC(svg_props->color_rendering, SVG_RenderingHint);
	*svg_props->color_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->display, SVG_Display);
	*svg_props->display = SVG_DISPLAY_INLINE;

	GF_SAFEALLOC(svg_props->display_align, SVG_DisplayAlign);
	*svg_props->display_align = SVG_DISPLAYALIGN_AUTO;

	GF_SAFEALLOC(svg_props->fill, SVG_Paint);
	svg_props->fill->type = SVG_PAINT_COLOR;
	svg_props->fill->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->fill->color.red, green, blue set to zero, so initial value for fill color is black */

	GF_SAFEALLOC(svg_props->fill_opacity, SVG_Number);
	svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->fill_opacity->value = FIX_ONE;
	
	GF_SAFEALLOC(svg_props->fill_rule, SVG_FillRule);
	*svg_props->fill_rule = SVG_FILLRULE_NONZERO;

	GF_SAFEALLOC(svg_props->font_family, SVG_FontFamily);
	svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
	svg_props->font_family->value = strdup("Arial");

	GF_SAFEALLOC(svg_props->font_size, SVG_FontSize);
	svg_props->font_size->type = SVG_NUMBER_VALUE;
	svg_props->font_size->value = 12*FIX_ONE;

	GF_SAFEALLOC(svg_props->font_style, SVG_FontStyle);
	*svg_props->font_style = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->font_variant, SVG_FontVariant);
	*svg_props->font_variant = SVG_FONTVARIANT_NORMAL;

	GF_SAFEALLOC(svg_props->font_weight, SVG_FontWeight);
	*svg_props->font_weight = SVG_FONTWEIGHT_NORMAL;

	GF_SAFEALLOC(svg_props->image_rendering, SVG_RenderingHint);
	*svg_props->image_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->line_increment, SVG_Number);
	svg_props->line_increment->type = SVG_NUMBER_AUTO;
	svg_props->line_increment->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->opacity, SVG_Number);
	svg_props->opacity->type = SVG_NUMBER_VALUE;
	svg_props->opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->pointer_events, SVG_PointerEvents);
	*svg_props->pointer_events = SVG_POINTEREVENTS_VISIBLEPAINTED;

	GF_SAFEALLOC(svg_props->shape_rendering, SVG_RenderingHint);
	*svg_props->shape_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->solid_color, SVG_Paint);
	svg_props->solid_color->type = SVG_PAINT_COLOR;
	svg_props->solid_color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->solid_color->color.red, green, blue set to zero, so initial value for solid_color is black */

	GF_SAFEALLOC(svg_props->solid_opacity, SVG_Number);
	svg_props->solid_opacity->type = SVG_NUMBER_VALUE;
	svg_props->solid_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stop_color, SVG_Paint);
	svg_props->stop_color->type = SVG_PAINT_COLOR;
	svg_props->stop_color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->stop_color->color.red, green, blue set to zero, so initial value for stop_color is black */

	GF_SAFEALLOC(svg_props->stop_opacity, SVG_Number);
	svg_props->stop_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stop_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke, SVG_Paint);
	svg_props->stroke->type = SVG_PAINT_NONE;
	svg_props->stroke->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->stroke->color.red, green, blue set to zero, so initial value for stroke color is black */

	GF_SAFEALLOC(svg_props->stroke_dasharray, SVG_StrokeDashArray);
	svg_props->stroke_dasharray->type = SVG_STROKEDASHARRAY_NONE;

	GF_SAFEALLOC(svg_props->stroke_dashoffset , SVG_Length);
	svg_props->stroke_dashoffset->type = SVG_NUMBER_VALUE;
	svg_props->stroke_dashoffset->value = 0;

	GF_SAFEALLOC(svg_props->stroke_linecap, SVG_StrokeLineCap);
	*svg_props->stroke_linecap = SVG_STROKELINECAP_BUTT;

	GF_SAFEALLOC(svg_props->stroke_linejoin, SVG_StrokeLineJoin);
	*svg_props->stroke_linejoin = SVG_STROKELINEJOIN_MITER;

	GF_SAFEALLOC(svg_props->stroke_miterlimit, SVG_Number);
	svg_props->stroke_miterlimit->type = SVG_NUMBER_VALUE;
	svg_props->stroke_miterlimit->value = 4*FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_opacity, SVG_Number);
	svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stroke_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stroke_width, SVG_Length);
	svg_props->stroke_width->type = SVG_NUMBER_VALUE;
	svg_props->stroke_width->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->text_align, SVG_TextAlign);
	*svg_props->text_align = SVG_TEXTALIGN_START;

	GF_SAFEALLOC(svg_props->text_anchor, SVG_TextAnchor);
	*svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->text_rendering, SVG_RenderingHint);
	*svg_props->text_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->vector_effect, SVG_VectorEffect);
	*svg_props->vector_effect = SVG_VECTOREFFECT_NONE;

	GF_SAFEALLOC(svg_props->viewport_fill, SVG_Paint);
	svg_props->viewport_fill->type = SVG_PAINT_NONE;

	GF_SAFEALLOC(svg_props->viewport_fill_opacity, SVG_Number);
	svg_props->viewport_fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->viewport_fill_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->visibility, SVG_Visibility);
	*svg_props->visibility = SVG_VISIBILITY_VISIBLE;

}

GF_EXPORT
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props)
{
	if (!svg_props) return;
	if(svg_props->audio_level) free(svg_props->audio_level);
	gf_svg_delete_paint(NULL, svg_props->color);
	if(svg_props->color_rendering) free(svg_props->color_rendering);
	if(svg_props->display) free(svg_props->display);
	if(svg_props->display_align) free(svg_props->display_align);
	gf_svg_delete_paint(NULL, svg_props->fill);
	if(svg_props->fill_opacity) free(svg_props->fill_opacity);
	if(svg_props->fill_rule) free(svg_props->fill_rule);
	if(svg_props->font_family) {
		if (svg_props->font_family->value) free(svg_props->font_family->value);
		free(svg_props->font_family);
	}
	if(svg_props->font_size) free(svg_props->font_size);
	if(svg_props->font_style) free(svg_props->font_style);
	if(svg_props->font_variant) free(svg_props->font_variant);
	if(svg_props->font_weight) free(svg_props->font_weight);
	if(svg_props->image_rendering) free(svg_props->image_rendering);
	if(svg_props->line_increment) free(svg_props->line_increment);
	if(svg_props->opacity) free(svg_props->opacity);
	if(svg_props->pointer_events) free(svg_props->pointer_events);
	if(svg_props->shape_rendering) free(svg_props->shape_rendering);
	gf_svg_delete_paint(NULL, svg_props->solid_color);
	if(svg_props->solid_opacity) free(svg_props->solid_opacity);
	gf_svg_delete_paint(NULL, svg_props->stop_color);
	if(svg_props->stop_opacity) free(svg_props->stop_opacity);
	gf_svg_delete_paint(NULL, svg_props->stroke);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) free(svg_props->stroke_dasharray->array.vals);
		free(svg_props->stroke_dasharray);
	}
	if(svg_props->stroke_dashoffset) free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_linecap) free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_opacity) free(svg_props->stroke_opacity);
	if(svg_props->stroke_width) free(svg_props->stroke_width);
	if(svg_props->text_align) free(svg_props->text_align);
	if(svg_props->text_anchor) free(svg_props->text_anchor);
	if(svg_props->text_rendering) free(svg_props->text_rendering);
	if(svg_props->vector_effect) free(svg_props->vector_effect);
	gf_svg_delete_paint(NULL, svg_props->viewport_fill);
	if(svg_props->viewport_fill_opacity) free(svg_props->viewport_fill_opacity);
	if(svg_props->visibility) free(svg_props->visibility);
	memset(svg_props, 0, sizeof(SVGPropertiesPointers));
}

/* 
	TODO: Check if all properties are implemented 
*/
GF_EXPORT
u32 gf_svg_apply_inheritance(SVGElement *elt, SVGPropertiesPointers *render_svg_props) 
{
	u32 inherited_flags_mask = 0;
	if(!elt || !render_svg_props) return 0;

	/* Perform inheritance of all the properties */
	if (render_svg_props && elt->properties) {
		if (elt->properties->audio_level.type != SVG_NUMBER_INHERIT)
			render_svg_props->audio_level = &elt->properties->audio_level;	
		
		if (elt->properties->color.color.type != SVG_COLOR_INHERIT) {
			render_svg_props->color = &elt->properties->color;
		} else {
			inherited_flags_mask |= GF_SG_SVG_COLOR_DIRTY;
		}
		if (elt->properties->color_rendering != SVG_RENDERINGHINT_INHERIT) {
			render_svg_props->color_rendering = &elt->properties->color_rendering;
		}
		if (elt->properties->display != SVG_DISPLAY_INHERIT) {
			render_svg_props->display = &elt->properties->display;
		}
		if (elt->properties->display_align != SVG_DISPLAYALIGN_INHERIT) {
			render_svg_props->display_align = &elt->properties->display_align;
		} else {
			inherited_flags_mask |= GF_SG_SVG_DISPLAYALIGN_DIRTY;
		}
		if (elt->properties->fill.type != SVG_PAINT_INHERIT) {
			render_svg_props->fill = &(elt->properties->fill);
			if (elt->properties->fill.type == SVG_PAINT_COLOR && 
				elt->properties->fill.color.type == SVG_COLOR_CURRENTCOLOR &&
				(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
				inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
			}
		} else {
			inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
		}
		if (elt->properties->fill_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->fill_opacity = &elt->properties->fill_opacity;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FILLOPACITY_DIRTY;
		}
		if (elt->properties->fill_rule != SVG_FILLRULE_INHERIT) {
			render_svg_props->fill_rule = &elt->properties->fill_rule;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FILLRULE_DIRTY;
		}
		if (elt->properties->font_family.type != SVG_FONTFAMILY_INHERIT) {
			render_svg_props->font_family = &elt->properties->font_family;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FONTFAMILY_DIRTY;
		}
		if (elt->properties->font_size.type != SVG_NUMBER_INHERIT) {
			render_svg_props->font_size = &elt->properties->font_size;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FONTSIZE_DIRTY;
		}
		if (elt->properties->font_style != SVG_FONTSTYLE_INHERIT) {
			render_svg_props->font_style = &elt->properties->font_style;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FONTSTYLE_DIRTY;
		}
		if (elt->properties->font_variant != SVG_FONTVARIANT_INHERIT) {
			render_svg_props->font_variant = &elt->properties->font_variant;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FONTVARIANT_DIRTY;
		}
		if (elt->properties->font_weight!= SVG_FONTWEIGHT_INHERIT) {
			render_svg_props->font_weight = &elt->properties->font_weight;
		} else {
			inherited_flags_mask |= GF_SG_SVG_FONTWEIGHT_DIRTY;
		}
		if (elt->properties->image_rendering != SVG_RENDERINGHINT_INHERIT) {
			render_svg_props->image_rendering = &elt->properties->image_rendering;
		}
		if (elt->properties->line_increment.type != SVG_NUMBER_INHERIT) {
			render_svg_props->line_increment = &elt->properties->line_increment;
		} else {
			inherited_flags_mask |= GF_SG_SVG_LINEINCREMENT_DIRTY;
		}
		if (elt->properties->opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->opacity = &elt->properties->opacity;
		} else {
			inherited_flags_mask |= GF_SG_SVG_OPACITY_DIRTY;
		}
		if (elt->properties->pointer_events != SVG_POINTEREVENTS_INHERIT) {
			render_svg_props->pointer_events = &elt->properties->pointer_events;
		}
		if (elt->properties->shape_rendering != SVG_RENDERINGHINT_INHERIT) {
			render_svg_props->shape_rendering = &elt->properties->shape_rendering;
		}
		if (elt->properties->solid_color.type != SVG_PAINT_INHERIT) {
			render_svg_props->solid_color = &(elt->properties->solid_color);		
			if (elt->properties->solid_color.type == SVG_PAINT_COLOR && 
				elt->properties->solid_color.color.type == SVG_COLOR_CURRENTCOLOR &&
				(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
				inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_DIRTY;
			}
		} else {
			inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_DIRTY;
		}
		if (elt->properties->solid_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->solid_opacity = &elt->properties->solid_opacity;
		} else {
			inherited_flags_mask |= GF_SG_SVG_SOLIDOPACITY_DIRTY;
		}
		if (elt->properties->stop_color.type != SVG_PAINT_INHERIT) {
			render_svg_props->stop_color = &(elt->properties->stop_color);
			if (elt->properties->stop_color.type == SVG_PAINT_COLOR && 
				elt->properties->stop_color.color.type == SVG_COLOR_CURRENTCOLOR &&
				(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
				inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_DIRTY;
			}
		} else {
			inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_DIRTY;
		}
		if (elt->properties->stop_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stop_opacity = &elt->properties->stop_opacity;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STOPOPACITY_DIRTY;
		}
		if (elt->properties->stroke.type != SVG_PAINT_INHERIT) {
			render_svg_props->stroke = &elt->properties->stroke;
			if (elt->properties->stroke.type == SVG_PAINT_COLOR && 
				elt->properties->stroke.color.type == SVG_COLOR_CURRENTCOLOR &&
				(inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY)) {
				inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
			}
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
		}
		if (elt->properties->stroke_dasharray.type != SVG_STROKEDASHARRAY_INHERIT) {
			render_svg_props->stroke_dasharray = &elt->properties->stroke_dasharray;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKEDASHARRAY_DIRTY;
		}
		if (elt->properties->stroke_dashoffset.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_dashoffset = &elt->properties->stroke_dashoffset;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		}
		if (elt->properties->stroke_linecap != SVG_STROKELINECAP_INHERIT) {
			render_svg_props->stroke_linecap = &elt->properties->stroke_linecap;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKELINECAP_DIRTY;
		}
		if (elt->properties->stroke_linejoin != SVG_STROKELINEJOIN_INHERIT) {
			render_svg_props->stroke_linejoin = &elt->properties->stroke_linejoin;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKELINEJOIN_DIRTY;
		}
		if (elt->properties->stroke_miterlimit.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_miterlimit = &elt->properties->stroke_miterlimit;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		}
		if (elt->properties->stroke_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_opacity = &elt->properties->stroke_opacity;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKEOPACITY_DIRTY;
		}
		if (elt->properties->stroke_width.type != SVG_NUMBER_INHERIT) {
			render_svg_props->stroke_width = &elt->properties->stroke_width;
		} else {
			inherited_flags_mask |= GF_SG_SVG_STROKEWIDTH_DIRTY;
		}
		if (elt->properties->text_align != SVG_TEXTALIGN_INHERIT) {
			render_svg_props->text_align = &elt->properties->text_align;
		} else {
			inherited_flags_mask |= GF_SG_SVG_TEXTALIGN_DIRTY;
		}
		if (elt->properties->text_anchor != SVG_TEXTANCHOR_INHERIT) {
			render_svg_props->text_anchor = &elt->properties->text_anchor;
		} else {
			inherited_flags_mask |= GF_SG_SVG_TEXTANCHOR_DIRTY;
		}
		if (elt->properties->text_rendering != SVG_RENDERINGHINT_INHERIT) {
			render_svg_props->text_rendering = &elt->properties->text_rendering;
		}
		if (elt->properties->vector_effect != SVG_VECTOREFFECT_INHERIT) {
			render_svg_props->vector_effect = &elt->properties->vector_effect;
		} else {
			inherited_flags_mask |= GF_SG_SVG_VECTOREFFECT_DIRTY;
		}
		if (elt->properties->viewport_fill.type != SVG_PAINT_INHERIT) {
			render_svg_props->viewport_fill = &elt->properties->viewport_fill;		
		}
		if (elt->properties->viewport_fill_opacity.type != SVG_NUMBER_INHERIT) {
			render_svg_props->viewport_fill_opacity = &elt->properties->viewport_fill_opacity;
		}
		if (elt->properties->visibility != SVG_VISIBILITY_INHERIT) {
			render_svg_props->visibility = &elt->properties->visibility;
		}
	}
	return inherited_flags_mask;
}

/* Equivalent to "get_property_pointer_by_name" but without string comparison
   Returns a pointer to the property in the given output property context based 
   on the input attribute and the input property context.
	output_property_context: contains a list of pointers to the CSS properties 
    input_property_context:  contains a list of the CSS properties of a given SVG element,
	input_attribute:         is the pointer to attribute of the same SVG element
	                         and which has the same name as the property we want

  Principle:
	if the address of the attribute NNN is equal to the pointer input_property_context->XXX, 
	then we return the property output_property_context->XXX 
	(ie. output_property_context->XXX)
*/
void *gf_svg_get_property_pointer(SVGPropertiesPointers *output_property_context, 
								  SVGProperties *input_property_context, 
								  void *input_attribute)
{
	if (!output_property_context || !input_property_context) return NULL;
	else if (input_attribute == &input_property_context->audio_level) return output_property_context->audio_level;
	else if (input_attribute == &input_property_context->color) return output_property_context->color;
	else if (input_attribute == &input_property_context->color_rendering) return output_property_context->color_rendering;
	else if (input_attribute == &input_property_context->display) return output_property_context->display;
	else if (input_attribute == &input_property_context->display_align) return output_property_context->display_align;
	else if (input_attribute == &input_property_context->fill) return output_property_context->fill;
	else if (input_attribute == &input_property_context->fill_opacity) return output_property_context->fill_opacity;
	else if (input_attribute == &input_property_context->fill_rule) return output_property_context->fill_rule;
	else if (input_attribute == &input_property_context->font_family) return output_property_context->font_family;
	else if (input_attribute == &input_property_context->font_size) return output_property_context->font_size;
	else if (input_attribute == &input_property_context->font_style) return output_property_context->font_style;
	else if (input_attribute == &input_property_context->font_variant) return output_property_context->font_variant;
	else if (input_attribute == &input_property_context->font_weight) return output_property_context->font_weight;
	else if (input_attribute == &input_property_context->image_rendering) return output_property_context->image_rendering;
	else if (input_attribute == &input_property_context->line_increment) return output_property_context->line_increment;
	else if (input_attribute == &input_property_context->opacity) return output_property_context->opacity;
	else if (input_attribute == &input_property_context->pointer_events) return output_property_context->pointer_events;
	else if (input_attribute == &input_property_context->shape_rendering) return output_property_context->shape_rendering;
	else if (input_attribute == &input_property_context->solid_color) return output_property_context->solid_color;
	else if (input_attribute == &input_property_context->solid_opacity) return output_property_context->solid_opacity;
	else if (input_attribute == &input_property_context->stop_color) return output_property_context->stop_color;
	else if (input_attribute == &input_property_context->stop_opacity) return output_property_context->stop_opacity;
	else if (input_attribute == &input_property_context->stroke) return output_property_context->stroke;
	else if (input_attribute == &input_property_context->stroke_dasharray) return output_property_context->stroke_dasharray;
	else if (input_attribute == &input_property_context->stroke_dashoffset) return output_property_context->stroke_dashoffset;
	else if (input_attribute == &input_property_context->stroke_linecap) return output_property_context->stroke_linecap;
	else if (input_attribute == &input_property_context->stroke_linejoin) return output_property_context->stroke_linejoin;
	else if (input_attribute == &input_property_context->stroke_miterlimit) return output_property_context->stroke_miterlimit;
	else if (input_attribute == &input_property_context->stroke_opacity) return output_property_context->stop_opacity;
	else if (input_attribute == &input_property_context->stroke_width) return output_property_context->stroke_width;
	else if (input_attribute == &input_property_context->text_align) return output_property_context->text_align;
	else if (input_attribute == &input_property_context->text_anchor) return output_property_context->text_anchor;
	else if (input_attribute == &input_property_context->text_rendering) return output_property_context->text_rendering;
	else if (input_attribute == &input_property_context->vector_effect) return output_property_context->vector_effect;
	else if (input_attribute == &input_property_context->viewport_fill) return output_property_context->viewport_fill;
	else if (input_attribute == &input_property_context->viewport_fill_opacity) return output_property_context->viewport_fill_opacity;
	else if (input_attribute == &input_property_context->visibility) return output_property_context->visibility;
	else return NULL;
}

void *gf_svg3_get_property_pointer(SVG3Element *elt, void *input_attribute,
								   SVGPropertiesPointers *output_property_context)
{
	SVG3Attribute *att = elt->attributes;
	while (att) {
		if (att->data == input_attribute) break;
		att = att->next;
	}
	if (!att) return NULL;
	switch (att->tag) {
		case TAG_SVG3_ATT_audio_level: return output_property_context->audio_level;
		case TAG_SVG3_ATT_color: return output_property_context->color;
		case TAG_SVG3_ATT_color_rendering: return output_property_context->color_rendering;
		case TAG_SVG3_ATT_display: return output_property_context->display;
		case TAG_SVG3_ATT_display_align: return output_property_context->display_align;
		case TAG_SVG3_ATT_fill: return output_property_context->fill;
		case TAG_SVG3_ATT_fill_opacity: return output_property_context->fill_opacity;
		case TAG_SVG3_ATT_fill_rule: return output_property_context->fill_rule;
		case TAG_SVG3_ATT_font_family: return output_property_context->font_family;
		case TAG_SVG3_ATT_font_size: return output_property_context->font_size;
		case TAG_SVG3_ATT_font_style: return output_property_context->font_style;
		case TAG_SVG3_ATT_font_variant: return output_property_context->font_variant;
		case TAG_SVG3_ATT_font_weight: return output_property_context->font_weight;
		case TAG_SVG3_ATT_image_rendering: return output_property_context->image_rendering;
		case TAG_SVG3_ATT_line_increment: return output_property_context->line_increment;
		case TAG_SVG3_ATT_opacity: return output_property_context->opacity;
		case TAG_SVG3_ATT_pointer_events: return output_property_context->pointer_events;
		case TAG_SVG3_ATT_shape_rendering: return output_property_context->shape_rendering;
		case TAG_SVG3_ATT_solid_color: return output_property_context->solid_color;
		case TAG_SVG3_ATT_solid_opacity: return output_property_context->solid_opacity;
		case TAG_SVG3_ATT_stop_color: return output_property_context->stop_color;
		case TAG_SVG3_ATT_stop_opacity: return output_property_context->stop_opacity;
		case TAG_SVG3_ATT_stroke: return output_property_context->stroke;
		case TAG_SVG3_ATT_stroke_dasharray: return output_property_context->stroke_dasharray;
		case TAG_SVG3_ATT_stroke_dashoffset: return output_property_context->stroke_dashoffset;
		case TAG_SVG3_ATT_stroke_linecap: return output_property_context->stroke_linecap;
		case TAG_SVG3_ATT_stroke_linejoin: return output_property_context->stroke_linejoin;
		case TAG_SVG3_ATT_stroke_miterlimit: return output_property_context->stroke_miterlimit;
		case TAG_SVG3_ATT_stroke_opacity: return output_property_context->stroke_opacity;
		case TAG_SVG3_ATT_stroke_width: return output_property_context->stroke_width;
		case TAG_SVG3_ATT_text_align: return output_property_context->text_align;
		case TAG_SVG3_ATT_text_anchor: return output_property_context->text_anchor;
		case TAG_SVG3_ATT_text_rendering: return output_property_context->text_rendering;
		case TAG_SVG3_ATT_vector_effect: return output_property_context->vector_effect;
		case TAG_SVG3_ATT_viewport_fill: return output_property_context->viewport_fill;
		case TAG_SVG3_ATT_viewport_fill_opacity: return output_property_context->viewport_fill_opacity;
		case TAG_SVG3_ATT_visibility: return output_property_context->visibility;
		default:return NULL;
	}
}

Bool gf_svg_is_property(GF_Node *node, GF_FieldInfo *target_attribute)
{
	u32 tag = gf_node_get_tag(node);
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		SVGElement *e = (SVGElement *)node;
		if (target_attribute->far_ptr == &e->properties->audio_level) return 1;
		else if (target_attribute->far_ptr == &e->properties->color) return 1;
		else if (target_attribute->far_ptr == &e->properties->color_rendering) return 1;
		else if (target_attribute->far_ptr == &e->properties->display) return 1;
		else if (target_attribute->far_ptr == &e->properties->display_align) return 1;
		else if (target_attribute->far_ptr == &e->properties->fill) return 1;
		else if (target_attribute->far_ptr == &e->properties->fill_opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->fill_rule) return 1;
		else if (target_attribute->far_ptr == &e->properties->font_family) return 1;
		else if (target_attribute->far_ptr == &e->properties->font_size) return 1;
		else if (target_attribute->far_ptr == &e->properties->font_style) return 1;
		else if (target_attribute->far_ptr == &e->properties->font_variant) return 1;
		else if (target_attribute->far_ptr == &e->properties->font_weight) return 1;
		else if (target_attribute->far_ptr == &e->properties->image_rendering) return 1;
		else if (target_attribute->far_ptr == &e->properties->line_increment) return 1;
		else if (target_attribute->far_ptr == &e->properties->opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->pointer_events) return 1;
		else if (target_attribute->far_ptr == &e->properties->shape_rendering) return 1;
		else if (target_attribute->far_ptr == &e->properties->solid_color) return 1;
		else if (target_attribute->far_ptr == &e->properties->solid_opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->stop_color) return 1;
		else if (target_attribute->far_ptr == &e->properties->stop_opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_dasharray) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_dashoffset) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_linecap) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_linejoin) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_miterlimit) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->stroke_width) return 1;
		else if (target_attribute->far_ptr == &e->properties->text_align) return 1;
		else if (target_attribute->far_ptr == &e->properties->text_anchor) return 1;
		else if (target_attribute->far_ptr == &e->properties->text_rendering) return 1;
		else if (target_attribute->far_ptr == &e->properties->vector_effect) return 1;
		else if (target_attribute->far_ptr == &e->properties->viewport_fill) return 1;
		else if (target_attribute->far_ptr == &e->properties->viewport_fill_opacity) return 1;
		else if (target_attribute->far_ptr == &e->properties->visibility) return 1;
		else return 0;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG2) && (tag<=GF_NODE_RANGE_LAST_SVG2)) {
		return 0;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG3) && (tag<=GF_NODE_RANGE_LAST_SVG3)) {
		SVG3Element *e = (SVG3Element *)node;
		SVG3Attribute *att = e->attributes;
		while (att) {
			if (att->data == target_attribute->far_ptr) break;
			att = att->next;
		}
		if (!att) return 0;
		switch (att->tag) {
			case TAG_SVG3_ATT_audio_level: 
			case TAG_SVG3_ATT_color: 
			case TAG_SVG3_ATT_color_rendering: 
			case TAG_SVG3_ATT_display: 
			case TAG_SVG3_ATT_display_align: 
			case TAG_SVG3_ATT_fill: 
			case TAG_SVG3_ATT_fill_opacity: 
			case TAG_SVG3_ATT_fill_rule: 
			case TAG_SVG3_ATT_font_family: 
			case TAG_SVG3_ATT_font_size: 
			case TAG_SVG3_ATT_font_style: 
			case TAG_SVG3_ATT_font_variant: 
			case TAG_SVG3_ATT_font_weight: 
			case TAG_SVG3_ATT_image_rendering: 
			case TAG_SVG3_ATT_line_increment: 
			case TAG_SVG3_ATT_opacity: 
			case TAG_SVG3_ATT_pointer_events: 
			case TAG_SVG3_ATT_shape_rendering: 
			case TAG_SVG3_ATT_solid_color: 
			case TAG_SVG3_ATT_solid_opacity: 
			case TAG_SVG3_ATT_stop_color: 
			case TAG_SVG3_ATT_stop_opacity: 
			case TAG_SVG3_ATT_stroke: 
			case TAG_SVG3_ATT_stroke_dasharray: 
			case TAG_SVG3_ATT_stroke_dashoffset: 
			case TAG_SVG3_ATT_stroke_linecap: 
			case TAG_SVG3_ATT_stroke_linejoin: 
			case TAG_SVG3_ATT_stroke_miterlimit: 
			case TAG_SVG3_ATT_stroke_opacity: 
			case TAG_SVG3_ATT_stroke_width: 
			case TAG_SVG3_ATT_text_align: 
			case TAG_SVG3_ATT_text_anchor: 
			case TAG_SVG3_ATT_text_rendering: 
			case TAG_SVG3_ATT_vector_effect: 
			case TAG_SVG3_ATT_viewport_fill: 
			case TAG_SVG3_ATT_viewport_fill_opacity: 
			case TAG_SVG3_ATT_visibility: 
				return 1;
			default:
				return 0;
		}
	} else {
		return 0;
	}
}

/* TODO: Check that all possibly inherited types are treated */
Bool gf_svg_is_inherit(GF_FieldInfo *a)
{
	if (!a->far_ptr) return 1;

	switch (a->fieldType) {
	case SVG_Color_datatype:
		return (((SVG_Color *)a->far_ptr)->type == SVG_COLOR_INHERIT);
		break;
	case SVG_Paint_datatype:
		return (((SVG_Paint *)a->far_ptr)->type == SVG_PAINT_INHERIT);
		break;
	case SVG_FontSize_datatype:
	case SVG_Number_datatype:
		return (((SVG_Number *)a->far_ptr)->type == SVG_NUMBER_INHERIT);
		break;
	case SVG_RenderingHint_datatype:
		return (*((SVG_RenderingHint *)a->far_ptr) == SVG_RENDERINGHINT_INHERIT);
		break;
	case SVG_Display_datatype:
		return (*((SVG_Display *)a->far_ptr) == SVG_DISPLAY_INHERIT);
		break;
	case SVG_DisplayAlign_datatype:
		return (*((SVG_DisplayAlign *)a->far_ptr) == SVG_DISPLAYALIGN_INHERIT);
		break;
	case SVG_TextAlign_datatype:
		return (*((SVG_TextAlign *)a->far_ptr) == SVG_TEXTALIGN_INHERIT);
		break;
	case SVG_FillRule_datatype:
		return (*((SVG_FillRule *)a->far_ptr) == SVG_FILLRULE_INHERIT);
		break;
	case SVG_FontFamily_datatype:
		return (((SVG_FontFamily *)a->far_ptr)->type == SVG_FONTFAMILY_INHERIT);
		break;
	case SVG_FontStyle_datatype:
		return (*((SVG_FontStyle *)a->far_ptr) == SVG_FONTSTYLE_INHERIT);
		break;
	case SVG_FontWeight_datatype:
		return (*((SVG_FontWeight *)a->far_ptr) == SVG_FONTWEIGHT_INHERIT);
		break;
	case SVG_PointerEvents_datatype:
		return (*((SVG_PointerEvents *)a->far_ptr) == SVG_POINTEREVENTS_INHERIT);
		break;
	case SVG_StrokeDashArray_datatype:
		return (((SVG_StrokeDashArray *)a->far_ptr)->type == SVG_STROKEDASHARRAY_INHERIT);
		break;
	case SVG_StrokeLineCap_datatype:
		return (*((SVG_StrokeLineCap *)a->far_ptr) == SVG_STROKELINECAP_INHERIT);
		break;
	case SVG_StrokeLineJoin_datatype:
		return (*((SVG_StrokeLineJoin *)a->far_ptr) == SVG_STROKELINEJOIN_INHERIT);
		break;
	case SVG_TextAnchor_datatype:
		return (*((SVG_TextAnchor *)a->far_ptr) == SVG_TEXTANCHOR_INHERIT);
		break;
	case SVG_VectorEffect_datatype:
		return (*((SVG_VectorEffect *)a->far_ptr) == SVG_VECTOREFFECT_INHERIT);
		break;
	case SVG_Visibility_datatype:
		return (*((SVG_Visibility *)a->far_ptr) == SVG_VISIBILITY_INHERIT);
		break;
	case SVG_Overflow_datatype:
		return (*((SVG_Overflow *)a->far_ptr) == SVG_OVERFLOW_INHERIT);
		break;
	default:
		return 0;
	}
}

u32 gf_svg_get_rendering_flag_if_modified(SVGElement *n, GF_FieldInfo *info)
{
	if (n->properties) {		
		if      (&n->properties->audio_level == info->far_ptr)			return 0;
		else if (&n->properties->color == info->far_ptr)				return GF_SG_SVG_COLOR_DIRTY;
		else if (&n->properties->color_rendering == info->far_ptr)		return 0;
		else if (&n->properties->display == info->far_ptr)				return 0;
		else if (&n->properties->display_align == info->far_ptr)		return GF_SG_SVG_DISPLAYALIGN_DIRTY;
		else if (&n->properties->fill == info->far_ptr)					return GF_SG_SVG_FILL_DIRTY;
		else if (&n->properties->fill_opacity == info->far_ptr)			return GF_SG_SVG_FILLOPACITY_DIRTY;
		else if (&n->properties->fill_rule == info->far_ptr)			return GF_SG_SVG_FILLRULE_DIRTY;
		else if (&n->properties->font_family == info->far_ptr)			return GF_SG_SVG_FONTFAMILY_DIRTY;
		else if (&n->properties->font_size == info->far_ptr)			return GF_SG_SVG_FONTSIZE_DIRTY;
		else if (&n->properties->font_style == info->far_ptr)			return GF_SG_SVG_FONTSTYLE_DIRTY;
		else if (&n->properties->font_variant == info->far_ptr)			return GF_SG_SVG_FONTVARIANT_DIRTY;
		else if (&n->properties->font_weight == info->far_ptr)			return GF_SG_SVG_FONTWEIGHT_DIRTY;		
		else if (&n->properties->image_rendering == info->far_ptr)		return 0;
		else if (&n->properties->line_increment == info->far_ptr)		return GF_SG_SVG_LINEINCREMENT_DIRTY;
		else if (&n->properties->opacity == info->far_ptr)				return GF_SG_SVG_OPACITY_DIRTY;
		else if (&n->properties->pointer_events == info->far_ptr)		return 0;
		else if (&n->properties->shape_rendering == info->far_ptr)		return 0;
		else if (&n->properties->solid_color == info->far_ptr)			return GF_SG_SVG_SOLIDCOLOR_DIRTY;
		else if (&n->properties->solid_opacity == info->far_ptr)		return GF_SG_SVG_SOLIDOPACITY_DIRTY;
		else if (&n->properties->stop_color == info->far_ptr)			return GF_SG_SVG_STOPCOLOR_DIRTY;
		else if (&n->properties->stop_opacity == info->far_ptr)			return GF_SG_SVG_STOPOPACITY_DIRTY;
		else if (&n->properties->stroke == info->far_ptr)				return GF_SG_SVG_STROKE_DIRTY;
		else if (&n->properties->stroke_dasharray == info->far_ptr)		return GF_SG_SVG_STROKEDASHARRAY_DIRTY;
		else if (&n->properties->stroke_dashoffset == info->far_ptr)	return GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		else if (&n->properties->stroke_linecap == info->far_ptr)		return GF_SG_SVG_STROKELINECAP_DIRTY;
		else if (&n->properties->stroke_linejoin == info->far_ptr)		return GF_SG_SVG_STROKELINEJOIN_DIRTY;
		else if (&n->properties->stroke_miterlimit == info->far_ptr)	return GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		else if (&n->properties->stroke_opacity == info->far_ptr)		return GF_SG_SVG_STROKEOPACITY_DIRTY;
		else if (&n->properties->stroke_width == info->far_ptr)			return GF_SG_SVG_STROKEWIDTH_DIRTY;
		else if (&n->properties->text_align == info->far_ptr)			return GF_SG_SVG_TEXTALIGN_DIRTY;
		else if (&n->properties->text_anchor == info->far_ptr)			return GF_SG_SVG_TEXTANCHOR_DIRTY;
		else if (&n->properties->text_rendering == info->far_ptr)		return 0;
		else if (&n->properties->vector_effect == info->far_ptr)		return GF_SG_SVG_VECTOREFFECT_DIRTY;
		else if (&n->properties->viewport_fill == info->far_ptr)		return 0;
		else if (&n->properties->viewport_fill_opacity == info->far_ptr)return 0;
		else if (&n->properties->visibility == info->far_ptr)			return 0;
	}
	
	/* this is not a property but a regular attribute, the animatable attributes are at the moment:
		focusable, focusHighlight, gradientUnits, nav-*, target, xlink:href, xlink:type, 


		the following affect the geometry of the element (some affect the positioning):
		cx, cy, d, height, offset, pathLength, points, r, rx, ry, width, x, x1, x2, y, y1, y2, rotate

		the following affect the positioning and are computed at each frame:
		transform, viewBox, preserveAspectRatio
	*/
	switch (info->fieldType) {
		case SVG_Number_datatype:
		case SVG_Length_datatype:
		case SVG_Coordinate_datatype:
		case SVG_Numbers_datatype:
		case SVG_Points_datatype:
		case SVG_Coordinates_datatype:
		case SVG_PathData_datatype:
		case SVG_Rotate_datatype:
			return GF_SG_SVG_GEOMETRY_DIRTY;

		case SVG_IRI_datatype:
			return GF_SG_NODE_DIRTY;

		//case SVG_Matrix_datatype:
		//case SVG_Motion_datatype:
		//case SVG_ViewBox_datatype:

		default:
			return 0;
	}
}

GF_EXPORT
Bool gf_svg_has_appearance_flag_dirty(u32 flags)
{
#if 0
	/* fill-related */
	if (flags & GF_SG_SVG_FILL_DIRTY)				return 1;
	if (flags & GF_SG_SVG_FILLOPACITY_DIRTY)		return 1;
	if (flags & GF_SG_SVG_FILLRULE_DIRTY)			return 1;

	/* stroke-related */
	if (flags & GF_SG_SVG_STROKE_DIRTY)				return 1;
	if (flags & GF_SG_SVG_STROKEDASHARRAY_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKEDASHOFFSET_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKELINECAP_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKELINEJOIN_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKEMITERLIMIT_DIRTY)	return 1;
	if (flags & GF_SG_SVG_STROKEOPACITY_DIRTY)		return 1;
	if (flags & GF_SG_SVG_STROKEWIDTH_DIRTY)		return 1;
	if (flags & GF_SG_SVG_VECTOREFFECT_DIRTY)		return 1;

	/* gradients stops and solidcolor do not affect appearance directly */
	return 0;
#else
	if (flags & 
		(GF_SG_SVG_FILL_DIRTY | GF_SG_SVG_FILLOPACITY_DIRTY | GF_SG_SVG_FILLRULE_DIRTY
		| GF_SG_SVG_STROKE_DIRTY | GF_SG_SVG_STROKEDASHARRAY_DIRTY
		| GF_SG_SVG_STROKEDASHOFFSET_DIRTY | GF_SG_SVG_STROKELINECAP_DIRTY
		| GF_SG_SVG_STROKELINEJOIN_DIRTY | GF_SG_SVG_STROKEMITERLIMIT_DIRTY
		| GF_SG_SVG_STROKEOPACITY_DIRTY | GF_SG_SVG_STROKEWIDTH_DIRTY
		| GF_SG_SVG_VECTOREFFECT_DIRTY) )
			return 1;
	return 0;
#endif
}
