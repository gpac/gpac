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

	/* Color Related Properties */
	GF_SAFEALLOC(svg_props->color, sizeof(SVG_Paint));
	svg_props->color->type = SVG_PAINT_COLOR;
	svg_props->color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */

	GF_SAFEALLOC(svg_props->fill, sizeof(SVG_Paint));
	svg_props->fill->type = SVG_PAINT_COLOR;
	svg_props->fill->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->fill->color.red, green, blue set to zero, so initial value for fill color is black */

	GF_SAFEALLOC(svg_props->stroke, sizeof(SVG_Paint));
	svg_props->stroke->type = SVG_PAINT_NONE;
	svg_props->stroke->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->stroke->color.red, green, blue set to zero, so initial value for stroke color is black */

	GF_SAFEALLOC(svg_props->solid_color, sizeof(SVG_Paint));
	svg_props->solid_color->type = SVG_PAINT_COLOR;
	svg_props->solid_color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->solid_color->color.red, green, blue set to zero, so initial value for solid_color is black */

	GF_SAFEALLOC(svg_props->stop_color, sizeof(SVG_Paint));
	svg_props->stop_color->type = SVG_PAINT_COLOR;
	svg_props->stop_color->color.type = SVG_COLOR_RGBCOLOR;
	/* svg_props->stop_color->color.red, green, blue set to zero, so initial value for stop_color is black */

	GF_SAFEALLOC(svg_props->viewport_fill, sizeof(SVG_Paint));
	svg_props->viewport_fill->type = SVG_PAINT_COLOR;
	svg_props->viewport_fill->color.type = SVG_COLOR_RGBCOLOR;
	svg_props->viewport_fill->color.red = 1;
	svg_props->viewport_fill->color.green = 1;
	svg_props->viewport_fill->color.blue = 1;


	/* Opacity Related Properties */
	GF_SAFEALLOC(svg_props->fill_opacity, sizeof(SVG_Opacity));
	svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->fill_opacity->value = FIX_ONE;
	
	GF_SAFEALLOC(svg_props->stroke_opacity, sizeof(SVG_Opacity));
	svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stroke_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->opacity, sizeof(SVG_Opacity));
	svg_props->opacity->type = SVG_NUMBER_VALUE;
	svg_props->opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->viewport_fill_opacity, sizeof(SVG_Opacity));
	svg_props->viewport_fill_opacity->type = SVG_NUMBER_VALUE;
	svg_props->viewport_fill_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->solid_opacity, sizeof(SVG_Opacity));
	svg_props->solid_opacity->type = SVG_NUMBER_VALUE;
	svg_props->solid_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->stop_opacity, sizeof(SVG_Opacity));
	svg_props->stop_opacity->type = SVG_NUMBER_VALUE;
	svg_props->stop_opacity->value = FIX_ONE;

	GF_SAFEALLOC(svg_props->audio_level, sizeof(SVG_AudioLevel));
	svg_props->audio_level->type = SVG_NUMBER_VALUE;
	svg_props->audio_level->value = FIX_ONE;

	/* Rendering Hints properties */
	GF_SAFEALLOC(svg_props->image_rendering, sizeof(SVG_RenderingHint));
	*svg_props->image_rendering = SVG_RENDERINGHINT_AUTO;
	GF_SAFEALLOC(svg_props->text_rendering, sizeof(SVG_RenderingHint));
	*svg_props->text_rendering = SVG_RENDERINGHINT_AUTO;
	GF_SAFEALLOC(svg_props->shape_rendering, sizeof(SVG_RenderingHint));
	*svg_props->shape_rendering = SVG_RENDERINGHINT_AUTO;
	GF_SAFEALLOC(svg_props->color_rendering, sizeof(SVG_RenderingHint));
	*svg_props->color_rendering = SVG_RENDERINGHINT_AUTO;

	/* Visibility properties */
	GF_SAFEALLOC(svg_props->visibility, sizeof(SVG_Visibility));
	*svg_props->visibility = SVG_VISIBILITY_VISIBLE;

	GF_SAFEALLOC(svg_props->display, sizeof(SVG_Display));
	*svg_props->display = SVG_DISPLAY_INLINE;

	GF_SAFEALLOC(svg_props->overflow, sizeof(SVG_Overflow));
	*svg_props->overflow = SVG_OVERFLOW_AUTO;

	/* Text related properties */
	GF_SAFEALLOC(svg_props->font_family, sizeof(SVG_FontFamily));
	svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
	svg_props->font_family->value = strdup("Arial");

	GF_SAFEALLOC(svg_props->font_size, sizeof(SVG_FontSize));
	svg_props->font_size->type = SVG_NUMBER_VALUE;
	svg_props->font_size->value = 12*FIX_ONE;

	GF_SAFEALLOC(svg_props->font_style, sizeof(SVG_FontStyle));
	*(svg_props->font_style) = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->font_variant, sizeof(SVG_FontVariant));
	*(svg_props->font_variant) = SVG_FONTVARIANT_NORMAL;

	GF_SAFEALLOC(svg_props->font_weight, sizeof(SVG_FontWeight));
	*svg_props->font_weight = SVG_FONTWEIGHT_NORMAL;

	GF_SAFEALLOC(svg_props->text_anchor, sizeof(SVG_TextAnchor));
	*svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->display_align, sizeof(SVG_DisplayAlign));
	*svg_props->display_align = SVG_DISPLAYALIGN_AUTO;

	GF_SAFEALLOC(svg_props->line_increment, sizeof(SVG_LineIncrement));
	svg_props->line_increment->type = SVG_NUMBER_AUTO;
	svg_props->line_increment->value = FIX_ONE;

	/* Interactivity related props */
	GF_SAFEALLOC(svg_props->pointer_events, sizeof(SVG_PointerEvents));
	*svg_props->pointer_events = SVG_POINTEREVENTS_VISIBLEPAINTED;

	/* Fill and Stroke related props */
	GF_SAFEALLOC(svg_props->fill_rule, sizeof(SVG_FillRule));
	*svg_props->fill_rule = SVG_FILLRULE_NONZERO;

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

	GF_SAFEALLOC(svg_props->vector_effect, sizeof(SVG_VectorEffect));
	*svg_props->vector_effect = SVG_VECTOREFFECT_NONE;
}

void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props)
{
	if (!svg_props) return;
	gf_svg_delete_paint(svg_props->color);
	gf_svg_delete_paint(svg_props->fill);
	gf_svg_delete_paint(svg_props->stroke);
	gf_svg_delete_paint(svg_props->solid_color);
	gf_svg_delete_paint(svg_props->stop_color);
	gf_svg_delete_paint(svg_props->viewport_fill);

	if(svg_props->fill_opacity) free(svg_props->fill_opacity);
	if(svg_props->solid_opacity) free(svg_props->solid_opacity);
	if(svg_props->stop_opacity) free(svg_props->stop_opacity);
	if(svg_props->stroke_opacity) free(svg_props->stroke_opacity);
	if(svg_props->opacity) free(svg_props->opacity);
	if(svg_props->viewport_fill_opacity) free(svg_props->viewport_fill_opacity);

	if(svg_props->audio_level) free(svg_props->audio_level);

	if(svg_props->image_rendering) free(svg_props->image_rendering);
	if(svg_props->color_rendering) free(svg_props->color_rendering);
	if(svg_props->text_rendering) free(svg_props->text_rendering);
	if(svg_props->shape_rendering) free(svg_props->shape_rendering);
	
	if(svg_props->display) free(svg_props->display);
	if(svg_props->visibility) free(svg_props->visibility);
	if(svg_props->overflow) free(svg_props->overflow);
	
	if(svg_props->font_family) {
		if (svg_props->font_family->value) free(svg_props->font_family->value);
		free(svg_props->font_family);
	}
	if(svg_props->font_size) free(svg_props->font_size);
	if(svg_props->font_style) free(svg_props->font_style);
	if(svg_props->font_variant) free(svg_props->font_variant);
	if(svg_props->font_weight) free(svg_props->font_weight);
	if(svg_props->text_anchor) free(svg_props->text_anchor);
	if(svg_props->display_align) free(svg_props->display_align);
	if(svg_props->line_increment) free(svg_props->line_increment);

	if(svg_props->pointer_events) free(svg_props->pointer_events);

	if(svg_props->fill_rule) free(svg_props->fill_rule);
	if(svg_props->stroke_width) free(svg_props->stroke_width);
	if(svg_props->stroke_linecap) free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_dashoffset) free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) free(svg_props->stroke_dasharray->array.vals);
		free(svg_props->stroke_dasharray);
	}
	if(svg_props->vector_effect) free(svg_props->vector_effect);

	memset(svg_props, 0, sizeof(SVGPropertiesPointers));
}

/* Updates the passed SVG Styling Properties Pointers with the properties of the given SVG element:
	1- applies inheritance whenever needed.
	2- applies any running animation on the element

	TODO: Check if all properties are implemented 
*/
void gf_svg_apply_inheritance_and_animation(GF_Node *node, SVGPropertiesPointers *render_svg_props)
{
	u32 count_all, i;
	SVGElement *elt = (SVGElement*)node;

	/*Step 1: perform inheritance*/
	if (render_svg_props && elt->properties) {
		if (elt->properties->color.color.type != SVG_COLOR_INHERIT) 
			render_svg_props->color = &elt->properties->color;
		if (elt->properties->fill.type != SVG_PAINT_INHERIT) 
			render_svg_props->fill = &(elt->properties->fill);
		if (elt->properties->stroke.type != SVG_PAINT_INHERIT)
			render_svg_props->stroke = &elt->properties->stroke;
		if (elt->properties->solid_color.type != SVG_PAINT_INHERIT)
			render_svg_props->solid_color = &(elt->properties->solid_color);		
		if (elt->properties->stop_color.type != SVG_PAINT_INHERIT)
			render_svg_props->stop_color = &(elt->properties->stop_color);
		if (elt->properties->viewport_fill.type != SVG_PAINT_INHERIT)
			render_svg_props->viewport_fill = &elt->properties->viewport_fill;		
		
		if (elt->properties->fill_opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->fill_opacity = &elt->properties->fill_opacity;
		if (elt->properties->stroke_opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->stroke_opacity = &elt->properties->stroke_opacity;
		if (elt->properties->solid_opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->solid_opacity = &elt->properties->solid_opacity;
		if (elt->properties->stop_opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->stop_opacity = &elt->properties->stop_opacity;
		if (elt->properties->viewport_fill_opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->viewport_fill_opacity = &elt->properties->viewport_fill_opacity;
		if (elt->properties->opacity.type != SVG_NUMBER_INHERIT)
			render_svg_props->opacity = &elt->properties->opacity;

		if (elt->properties->audio_level.type != SVG_NUMBER_INHERIT)
			render_svg_props->audio_level = &elt->properties->audio_level;		
		
		if (elt->properties->shape_rendering != SVG_RENDERINGHINT_INHERIT)
			render_svg_props->shape_rendering = &elt->properties->shape_rendering;
		if (elt->properties->image_rendering != SVG_RENDERINGHINT_INHERIT)
			render_svg_props->image_rendering = &elt->properties->image_rendering;
		if (elt->properties->color_rendering != SVG_RENDERINGHINT_INHERIT)
			render_svg_props->color_rendering = &elt->properties->color_rendering;
		if (elt->properties->text_rendering != SVG_RENDERINGHINT_INHERIT)
			render_svg_props->text_rendering = &elt->properties->text_rendering;

		if (elt->properties->display != SVG_DISPLAY_INHERIT)
			render_svg_props->display = &elt->properties->display;
		if (elt->properties->visibility != SVG_VISIBILITY_INHERIT)
			render_svg_props->visibility = &elt->properties->visibility;
		if (elt->properties->overflow != SVG_OVERFLOW_INHERIT)
			render_svg_props->overflow = &elt->properties->overflow;

		if (elt->properties->font_family.type != SVG_FONTFAMILY_INHERIT)
			render_svg_props->font_family = &elt->properties->font_family;
		if (elt->properties->font_size.type != SVG_NUMBER_INHERIT)
			render_svg_props->font_size = &elt->properties->font_size;
		if (elt->properties->font_style != SVG_FONTSTYLE_INHERIT)
			render_svg_props->font_style = &elt->properties->font_style;
		if (elt->properties->font_variant != SVG_FONTVARIANT_INHERIT)
			render_svg_props->font_variant = &elt->properties->font_variant;
		if (elt->properties->font_weight!= SVG_FONTWEIGHT_INHERIT)
			render_svg_props->font_weight = &elt->properties->font_weight;
		if (elt->properties->text_anchor != SVG_TEXTANCHOR_INHERIT)
			render_svg_props->text_anchor = &elt->properties->text_anchor;
		if (elt->properties->display_align != SVG_DISPLAYALIGN_INHERIT)
			render_svg_props->display_align = &elt->properties->display_align;
		if (elt->properties->line_increment.type != SVG_NUMBER_INHERIT)
			render_svg_props->line_increment = &elt->properties->line_increment;

		if (elt->properties->pointer_events != SVG_POINTEREVENTS_INHERIT)
			render_svg_props->pointer_events = &elt->properties->pointer_events;

		if (elt->properties->fill_rule != SVG_FILLRULE_INHERIT)
			render_svg_props->fill_rule = &elt->properties->fill_rule;
		if (elt->properties->stroke_width.type != SVG_NUMBER_INHERIT)
			render_svg_props->stroke_width = &elt->properties->stroke_width;
		if (elt->properties->stroke_miterlimit.type != SVG_NUMBER_INHERIT)
			render_svg_props->stroke_miterlimit = &elt->properties->stroke_miterlimit;
		if (elt->properties->stroke_linecap != SVG_STROKELINECAP_INHERIT)
			render_svg_props->stroke_linecap = &elt->properties->stroke_linecap;
		if (elt->properties->stroke_linejoin != SVG_STROKELINEJOIN_INHERIT)
			render_svg_props->stroke_linejoin = &elt->properties->stroke_linejoin;
		if (elt->properties->stroke_dashoffset.type != SVG_NUMBER_INHERIT)
			render_svg_props->stroke_dashoffset = &elt->properties->stroke_dashoffset;
		if (elt->properties->stroke_dasharray.type != SVG_STROKEDASHARRAY_INHERIT)
			render_svg_props->stroke_dasharray = &elt->properties->stroke_dasharray;
		if (elt->properties->vector_effect != SVG_VECTOREFFECT_INHERIT)
			render_svg_props->vector_effect = &elt->properties->vector_effect;
	}

	/*Step 2: handle all animations*/
	/*TODO FIXME - THIS IS WRONG, we're changing orders of animations which may corrupt the visual result*/
	count_all = gf_node_animation_count(node);
	/* Loop 1: For all animated attributes */
	for (i = 0; i < count_all; i++) {
		u32 j, count;
		
		SMIL_AttributeAnimations *aa = gf_node_animation_get(node, i);		
		count = gf_list_count(aa->anims);
		if (!count) continue;
	
		/* Resetting the presentation value issued at the previous rendering cycle to the computed value:
		   The result is either: 
			- the specified value (if no inheritance)
			- the inherited value from the current property context (if the specified value is 'inherit')
			- the value of the color property (if the specified value is 'currentColor') */
		gf_svg_attributes_copy_computed_value(&(aa->presentation_value), &(aa->saved_specified_value), 
											  (SVGElement*)node, aa->orig_dom_ptr, render_svg_props);

		/* we also need a special handling of the keyword 'currentColor' if used in animation values */
		aa->current_color_value.fieldType = SVG_Paint_datatype;
		aa->current_color_value.far_ptr = render_svg_props->color;

		/* Loop 2: For all animations targetting the current attribute */
		for (j = 0; j < count; j++) {
			SMIL_Anim_RTI *rai = gf_list_get(aa->anims, j);			
			SMIL_Timing_RTI *rti = rai->anim_elt->timing->runtime;
			Double scene_time = gf_node_get_scene_time(node);
			if (rti->evaluate) {
				Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
				rti->evaluate(rti, simple_time);
			}
		}
		/* end of Loop 2 */

		/*TODO FIXME, we need a finer granularity here 
		  and we must know if the animated attribute has changed or not (freeze)...*/
		gf_node_dirty_set(node, GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_SVG_APPEARANCE_DIRTY, 0);

	}
}

/* Equivalent to "get_property_pointer_by_name" but without string comparison
   Returns a pointer to the property in the given rendering property context based on the input attribute and the element property context.
	rendering_property_context: contains a list of pointers to the CSS properties used for rendering 
    elt_property_context:       contains the pointers to the CSS properties of a given SVG element,
	input_attribute:            is the attribute which has the same name as the property we want

  Principle:
	if the pointer to the attribute NNN is equal to the pointer elt_property_context->XXX, 
	then we return the properties called XXX from the rendering property context 
	(ie. rendering_property_context->XXX)
*/
void *gf_svg_get_property_pointer(SVGPropertiesPointers *rendering_property_context, 
								  SVGProperties *elt_property_context, 
								  void *input_attribute)
{
	if (!rendering_property_context || !elt_property_context) return NULL;
	else if (input_attribute == &elt_property_context->color) return rendering_property_context->color;
	else if (input_attribute == &elt_property_context->fill) return rendering_property_context->fill;
	else if (input_attribute == &elt_property_context->stroke) return rendering_property_context->stroke;
	else if (input_attribute == &elt_property_context->solid_color) return rendering_property_context->solid_color;
	else if (input_attribute == &elt_property_context->stop_color) return rendering_property_context->stop_color;
	else if (input_attribute == &elt_property_context->viewport_fill) return rendering_property_context->viewport_fill;
	else if (input_attribute == &elt_property_context->fill_opacity) return rendering_property_context->fill_opacity;
	else if (input_attribute == &elt_property_context->solid_opacity) return rendering_property_context->solid_opacity;
	else if (input_attribute == &elt_property_context->stop_opacity) return rendering_property_context->stop_opacity;
	else if (input_attribute == &elt_property_context->stroke_opacity) return rendering_property_context->stop_opacity;
	else if (input_attribute == &elt_property_context->viewport_fill_opacity) return rendering_property_context->viewport_fill_opacity;
	else if (input_attribute == &elt_property_context->audio_level) return rendering_property_context->audio_level;
	else if (input_attribute == &elt_property_context->color_rendering) return rendering_property_context->color_rendering;
	else if (input_attribute == &elt_property_context->image_rendering) return rendering_property_context->image_rendering;
	else if (input_attribute == &elt_property_context->shape_rendering) return rendering_property_context->shape_rendering;
	else if (input_attribute == &elt_property_context->text_rendering) return rendering_property_context->text_rendering;
	else if (input_attribute == &elt_property_context->display) return rendering_property_context->display;
	else if (input_attribute == &elt_property_context->display_align) return rendering_property_context->display_align;
	else if (input_attribute == &elt_property_context->fill_rule) return rendering_property_context->fill_rule;
	else if (input_attribute == &elt_property_context->font_family) return rendering_property_context->font_family;
	else if (input_attribute == &elt_property_context->font_size) return rendering_property_context->font_size;
	else if (input_attribute == &elt_property_context->font_style) return rendering_property_context->font_style;
	else if (input_attribute == &elt_property_context->font_weight) return rendering_property_context->font_weight;
	else if (input_attribute == &elt_property_context->line_increment) return rendering_property_context->line_increment;
	else if (input_attribute == &elt_property_context->pointer_events) return rendering_property_context->pointer_events;
	else if (input_attribute == &elt_property_context->stroke_dasharray) return rendering_property_context->stroke_dasharray;
	else if (input_attribute == &elt_property_context->stroke_dashoffset) return rendering_property_context->stroke_dashoffset;
	else if (input_attribute == &elt_property_context->stroke_linecap) return rendering_property_context->stroke_linecap;
	else if (input_attribute == &elt_property_context->stroke_linejoin) return rendering_property_context->stroke_linejoin;
	else if (input_attribute == &elt_property_context->stroke_miterlimit) return rendering_property_context->stroke_miterlimit;
	else if (input_attribute == &elt_property_context->stroke_width) return rendering_property_context->stroke_width;
	else if (input_attribute == &elt_property_context->text_anchor) return rendering_property_context->text_anchor;
	else if (input_attribute == &elt_property_context->vector_effect) return rendering_property_context->vector_effect;
	else if (input_attribute == &elt_property_context->visibility) return rendering_property_context->visibility;
	else if (input_attribute == &elt_property_context->opacity) return rendering_property_context->opacity;
	else if (input_attribute == &elt_property_context->overflow) return rendering_property_context->overflow;
	else return NULL;
}
