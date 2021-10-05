/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2004-2012
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

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
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
	if (svg_props->audio_level) {
		svg_props->audio_level->type = SVG_NUMBER_VALUE;
		svg_props->audio_level->value = FIX_ONE;
		svg_props->computed_audio_level = FIX_ONE;
	}
	
	GF_SAFEALLOC(svg_props->color, SVG_Paint);
	if (svg_props->color) {
		svg_props->color->type = SVG_PAINT_COLOR;
		svg_props->color->color.type = SVG_COLOR_RGBCOLOR;
		/* svg_props->color->red, green, blue set to zero, so initial value for color property is black */
	}
	
	GF_SAFEALLOC(svg_props->color_rendering, SVG_RenderingHint);
	if (svg_props->color_rendering) *svg_props->color_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->display, SVG_Display);
	if (svg_props->display) *svg_props->display = SVG_DISPLAY_INLINE;

	GF_SAFEALLOC(svg_props->display_align, SVG_DisplayAlign);
	if (svg_props->display_align) *svg_props->display_align = SVG_DISPLAYALIGN_AUTO;

	GF_SAFEALLOC(svg_props->fill, SVG_Paint);
	if (svg_props->fill) {
		svg_props->fill->type = SVG_PAINT_COLOR;
		svg_props->fill->color.type = SVG_COLOR_RGBCOLOR;
		/* svg_props->fill->color.red, green, blue set to zero, so initial value for fill color is black */
	}
	GF_SAFEALLOC(svg_props->fill_opacity, SVG_Number);
	if (svg_props->fill_opacity) {
		svg_props->fill_opacity->type = SVG_NUMBER_VALUE;
		svg_props->fill_opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->fill_rule, SVG_FillRule);
	if (svg_props->fill_rule) *svg_props->fill_rule = SVG_FILLRULE_NONZERO;

	GF_SAFEALLOC(svg_props->font_family, SVG_FontFamily);
	if (svg_props->font_family) {
		svg_props->font_family->type = SVG_FONTFAMILY_VALUE;
		svg_props->font_family->value = gf_strdup("Arial");
	}
	GF_SAFEALLOC(svg_props->font_size, SVG_FontSize);
	if (svg_props->font_size) {
		svg_props->font_size->type = SVG_NUMBER_VALUE;
		svg_props->font_size->value = 12*FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->font_style, SVG_FontStyle);
	if (svg_props->font_style) *svg_props->font_style = SVG_FONTSTYLE_NORMAL;

	GF_SAFEALLOC(svg_props->font_variant, SVG_FontVariant);
	if (svg_props->font_variant) *svg_props->font_variant = SVG_FONTVARIANT_NORMAL;

	GF_SAFEALLOC(svg_props->font_weight, SVG_FontWeight);
	if (svg_props->font_weight) *svg_props->font_weight = SVG_FONTWEIGHT_NORMAL;

	GF_SAFEALLOC(svg_props->image_rendering, SVG_RenderingHint);
	if (svg_props->image_rendering) *svg_props->image_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->line_increment, SVG_Number);
	if (svg_props->line_increment) svg_props->line_increment->type = SVG_NUMBER_AUTO;

	GF_SAFEALLOC(svg_props->opacity, SVG_Number);
	if (svg_props->opacity) {
		svg_props->opacity->type = SVG_NUMBER_VALUE;
		svg_props->opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->pointer_events, SVG_PointerEvents);
	if (svg_props->pointer_events) *svg_props->pointer_events = SVG_POINTEREVENTS_VISIBLEPAINTED;

	GF_SAFEALLOC(svg_props->shape_rendering, SVG_RenderingHint);
	if (svg_props->shape_rendering) *svg_props->shape_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->solid_color, SVG_Paint);
	if (svg_props->solid_color) {
		svg_props->solid_color->type = SVG_PAINT_COLOR;
		svg_props->solid_color->color.type = SVG_COLOR_RGBCOLOR;
		/* svg_props->solid_color->color.red, green, blue set to zero, so initial value for solid_color is black */
	}
	GF_SAFEALLOC(svg_props->solid_opacity, SVG_Number);
	if (svg_props->solid_opacity) {
		svg_props->solid_opacity->type = SVG_NUMBER_VALUE;
		svg_props->solid_opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->stop_color, SVG_Paint);
	if (svg_props->stop_color) {
		svg_props->stop_color->type = SVG_PAINT_COLOR;
		svg_props->stop_color->color.type = SVG_COLOR_RGBCOLOR;
		/* svg_props->stop_color->color.red, green, blue set to zero, so initial value for stop_color is black */
	}
	GF_SAFEALLOC(svg_props->stop_opacity, SVG_Number);
	if (svg_props->stop_opacity) {
		svg_props->stop_opacity->type = SVG_NUMBER_VALUE;
		svg_props->stop_opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->stroke, SVG_Paint);
	if (svg_props->stroke) {
		svg_props->stroke->type = SVG_PAINT_NONE;
		svg_props->stroke->color.type = SVG_COLOR_RGBCOLOR;
		/* svg_props->stroke->color.red, green, blue set to zero, so initial value for stroke color is black */
	}
	
	GF_SAFEALLOC(svg_props->stroke_dasharray, SVG_StrokeDashArray);
	if (svg_props->stroke_dasharray) svg_props->stroke_dasharray->type = SVG_STROKEDASHARRAY_NONE;

	GF_SAFEALLOC(svg_props->stroke_dashoffset , SVG_Length);
	if (svg_props->stroke_dashoffset) {
		svg_props->stroke_dashoffset->type = SVG_NUMBER_VALUE;
		svg_props->stroke_dashoffset->value = 0;
	}
	
	GF_SAFEALLOC(svg_props->stroke_linecap, SVG_StrokeLineCap);
	if (svg_props->stroke_linecap) *svg_props->stroke_linecap = SVG_STROKELINECAP_BUTT;

	GF_SAFEALLOC(svg_props->stroke_linejoin, SVG_StrokeLineJoin);
	if (svg_props->stroke_linejoin) *svg_props->stroke_linejoin = SVG_STROKELINEJOIN_MITER;

	GF_SAFEALLOC(svg_props->stroke_miterlimit, SVG_Number);
	if (svg_props->stroke_miterlimit) {
		svg_props->stroke_miterlimit->type = SVG_NUMBER_VALUE;
		svg_props->stroke_miterlimit->value = 4*FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->stroke_opacity, SVG_Number);
	if (svg_props->stroke_opacity) {
		svg_props->stroke_opacity->type = SVG_NUMBER_VALUE;
		svg_props->stroke_opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->stroke_width, SVG_Length);
	if (svg_props->stroke_width) {
		svg_props->stroke_width->type = SVG_NUMBER_VALUE;
		svg_props->stroke_width->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->text_align, SVG_TextAlign);
	if (svg_props->text_align) *svg_props->text_align = SVG_TEXTALIGN_START;

	GF_SAFEALLOC(svg_props->text_anchor, SVG_TextAnchor);
	if (svg_props->text_anchor) *svg_props->text_anchor = SVG_TEXTANCHOR_START;

	GF_SAFEALLOC(svg_props->text_rendering, SVG_RenderingHint);
	if (svg_props->text_rendering) *svg_props->text_rendering = SVG_RENDERINGHINT_AUTO;

	GF_SAFEALLOC(svg_props->vector_effect, SVG_VectorEffect);
	if (svg_props->vector_effect) *svg_props->vector_effect = SVG_VECTOREFFECT_NONE;

	GF_SAFEALLOC(svg_props->viewport_fill, SVG_Paint);
	if (svg_props->viewport_fill) svg_props->viewport_fill->type = SVG_PAINT_NONE;

	GF_SAFEALLOC(svg_props->viewport_fill_opacity, SVG_Number);
	if (svg_props->viewport_fill_opacity) {
		svg_props->viewport_fill_opacity->type = SVG_NUMBER_VALUE;
		svg_props->viewport_fill_opacity->value = FIX_ONE;
	}
	GF_SAFEALLOC(svg_props->visibility, SVG_Visibility);
	if (svg_props->visibility) *svg_props->visibility = SVG_VISIBILITY_VISIBLE;

}

GF_EXPORT
void gf_svg_properties_reset_pointers(SVGPropertiesPointers *svg_props)
{
	if (!svg_props) return;
	if(svg_props->audio_level) gf_free(svg_props->audio_level);
	gf_svg_delete_paint(NULL, svg_props->color);
	if(svg_props->color_rendering) gf_free(svg_props->color_rendering);
	if(svg_props->display) gf_free(svg_props->display);
	if(svg_props->display_align) gf_free(svg_props->display_align);
	gf_svg_delete_paint(NULL, svg_props->fill);
	if(svg_props->fill_opacity) gf_free(svg_props->fill_opacity);
	if(svg_props->fill_rule) gf_free(svg_props->fill_rule);
	if(svg_props->font_family) {
		if (svg_props->font_family->value) gf_free(svg_props->font_family->value);
		gf_free(svg_props->font_family);
	}
	if(svg_props->font_size) gf_free(svg_props->font_size);
	if(svg_props->font_style) gf_free(svg_props->font_style);
	if(svg_props->font_variant) gf_free(svg_props->font_variant);
	if(svg_props->font_weight) gf_free(svg_props->font_weight);
	if(svg_props->image_rendering) gf_free(svg_props->image_rendering);
	if(svg_props->line_increment) gf_free(svg_props->line_increment);
	if(svg_props->opacity) gf_free(svg_props->opacity);
	if(svg_props->pointer_events) gf_free(svg_props->pointer_events);
	if(svg_props->shape_rendering) gf_free(svg_props->shape_rendering);
	gf_svg_delete_paint(NULL, svg_props->solid_color);
	if(svg_props->solid_opacity) gf_free(svg_props->solid_opacity);
	gf_svg_delete_paint(NULL, svg_props->stop_color);
	if(svg_props->stop_opacity) gf_free(svg_props->stop_opacity);
	gf_svg_delete_paint(NULL, svg_props->stroke);
	if(svg_props->stroke_dasharray) {
		if (svg_props->stroke_dasharray->array.count) gf_free(svg_props->stroke_dasharray->array.vals);
		gf_free(svg_props->stroke_dasharray);
	}
	if(svg_props->stroke_dashoffset) gf_free(svg_props->stroke_dashoffset);
	if(svg_props->stroke_linecap) gf_free(svg_props->stroke_linecap);
	if(svg_props->stroke_linejoin) gf_free(svg_props->stroke_linejoin);
	if(svg_props->stroke_miterlimit) gf_free(svg_props->stroke_miterlimit);
	if(svg_props->stroke_opacity) gf_free(svg_props->stroke_opacity);
	if(svg_props->stroke_width) gf_free(svg_props->stroke_width);
	if(svg_props->text_align) gf_free(svg_props->text_align);
	if(svg_props->text_anchor) gf_free(svg_props->text_anchor);
	if(svg_props->text_rendering) gf_free(svg_props->text_rendering);
	if(svg_props->vector_effect) gf_free(svg_props->vector_effect);
	gf_svg_delete_paint(NULL, svg_props->viewport_fill);
	if(svg_props->viewport_fill_opacity) gf_free(svg_props->viewport_fill_opacity);
	if(svg_props->visibility) gf_free(svg_props->visibility);
	memset(svg_props, 0, sizeof(SVGPropertiesPointers));
}


void *gf_svg_get_property_pointer_from_tag(SVGPropertiesPointers *output_property_context, u32 prop_tag)
{
	switch (prop_tag) {
	case TAG_SVG_ATT_audio_level:
		return output_property_context->audio_level;
	case TAG_SVG_ATT_color:
		return output_property_context->color;
	case TAG_SVG_ATT_color_rendering:
		return output_property_context->color_rendering;
	case TAG_SVG_ATT_display:
		return output_property_context->display;
	case TAG_SVG_ATT_display_align:
		return output_property_context->display_align;
	case TAG_SVG_ATT_fill:
		return output_property_context->fill;
	case TAG_SVG_ATT_fill_opacity:
		return output_property_context->fill_opacity;
	case TAG_SVG_ATT_fill_rule:
		return output_property_context->fill_rule;
	case TAG_SVG_ATT_font_family:
		return output_property_context->font_family;
	case TAG_SVG_ATT_font_size:
		return output_property_context->font_size;
	case TAG_SVG_ATT_font_style:
		return output_property_context->font_style;
	case TAG_SVG_ATT_font_variant:
		return output_property_context->font_variant;
	case TAG_SVG_ATT_font_weight:
		return output_property_context->font_weight;
	case TAG_SVG_ATT_image_rendering:
		return output_property_context->image_rendering;
	case TAG_SVG_ATT_line_increment:
		return output_property_context->line_increment;
	case TAG_SVG_ATT_opacity:
		return output_property_context->opacity;
	case TAG_SVG_ATT_pointer_events:
		return output_property_context->pointer_events;
	case TAG_SVG_ATT_shape_rendering:
		return output_property_context->shape_rendering;
	case TAG_SVG_ATT_solid_color:
		return output_property_context->solid_color;
	case TAG_SVG_ATT_solid_opacity:
		return output_property_context->solid_opacity;
	case TAG_SVG_ATT_stop_color:
		return output_property_context->stop_color;
	case TAG_SVG_ATT_stop_opacity:
		return output_property_context->stop_opacity;
	case TAG_SVG_ATT_stroke:
		return output_property_context->stroke;
	case TAG_SVG_ATT_stroke_dasharray:
		return output_property_context->stroke_dasharray;
	case TAG_SVG_ATT_stroke_dashoffset:
		return output_property_context->stroke_dashoffset;
	case TAG_SVG_ATT_stroke_linecap:
		return output_property_context->stroke_linecap;
	case TAG_SVG_ATT_stroke_linejoin:
		return output_property_context->stroke_linejoin;
	case TAG_SVG_ATT_stroke_miterlimit:
		return output_property_context->stroke_miterlimit;
	case TAG_SVG_ATT_stroke_opacity:
		return output_property_context->stroke_opacity;
	case TAG_SVG_ATT_stroke_width:
		return output_property_context->stroke_width;
	case TAG_SVG_ATT_text_align:
		return output_property_context->text_align;
	case TAG_SVG_ATT_text_anchor:
		return output_property_context->text_anchor;
	case TAG_SVG_ATT_text_rendering:
		return output_property_context->text_rendering;
	case TAG_SVG_ATT_vector_effect:
		return output_property_context->vector_effect;
	case TAG_SVG_ATT_viewport_fill:
		return output_property_context->viewport_fill;
	case TAG_SVG_ATT_viewport_fill_opacity:
		return output_property_context->viewport_fill_opacity;
	case TAG_SVG_ATT_visibility:
		return output_property_context->visibility;
	default:
		return NULL;
	}
}

void *gf_svg_get_property_pointer(SVG_Element *elt, void *input_attribute,
                                  SVGPropertiesPointers *output_property_context)
{
	SVGAttribute *att = elt->attributes;
	while (att) {
		if (att->data == input_attribute) break;
		att = att->next;
	}
	if (!att) return NULL;
	return gf_svg_get_property_pointer_from_tag(output_property_context, att->tag);
}

Bool gf_svg_is_property(GF_Node *node, GF_FieldInfo *target_attribute)
{
	u32 tag = gf_node_get_tag(node);

	if (tag > GF_NODE_RANGE_LAST_VRML) {
		SVG_Element *e = (SVG_Element *)node;
		SVGAttribute *att = e->attributes;
		while (att) {
			if (att->data == target_attribute->far_ptr) break;
			att = att->next;
		}
		if (!att) return 0;
		switch (att->tag) {
		case TAG_SVG_ATT_audio_level:
		case TAG_SVG_ATT_color:
		case TAG_SVG_ATT_color_rendering:
		case TAG_SVG_ATT_display:
		case TAG_SVG_ATT_display_align:
		case TAG_SVG_ATT_fill:
		case TAG_SVG_ATT_fill_opacity:
		case TAG_SVG_ATT_fill_rule:
		case TAG_SVG_ATT_font_family:
		case TAG_SVG_ATT_font_size:
		case TAG_SVG_ATT_font_style:
		case TAG_SVG_ATT_font_variant:
		case TAG_SVG_ATT_font_weight:
		case TAG_SVG_ATT_image_rendering:
		case TAG_SVG_ATT_line_increment:
		case TAG_SVG_ATT_opacity:
		case TAG_SVG_ATT_pointer_events:
		case TAG_SVG_ATT_shape_rendering:
		case TAG_SVG_ATT_solid_color:
		case TAG_SVG_ATT_solid_opacity:
		case TAG_SVG_ATT_stop_color:
		case TAG_SVG_ATT_stop_opacity:
		case TAG_SVG_ATT_stroke:
		case TAG_SVG_ATT_stroke_dasharray:
		case TAG_SVG_ATT_stroke_dashoffset:
		case TAG_SVG_ATT_stroke_linecap:
		case TAG_SVG_ATT_stroke_linejoin:
		case TAG_SVG_ATT_stroke_miterlimit:
		case TAG_SVG_ATT_stroke_opacity:
		case TAG_SVG_ATT_stroke_width:
		case TAG_SVG_ATT_text_align:
		case TAG_SVG_ATT_text_anchor:
		case TAG_SVG_ATT_text_rendering:
		case TAG_SVG_ATT_vector_effect:
		case TAG_SVG_ATT_viewport_fill:
		case TAG_SVG_ATT_viewport_fill_opacity:
		case TAG_SVG_ATT_visibility:
			return 1;
		default:
			return 0;
		}
	}
	else {
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


u32 gf_svg_get_modification_flags(SVG_Element *n, GF_FieldInfo *info)
{
//	return 0xFFFFFFFF;
	switch (info->fieldType) {
	case SVG_Paint_datatype:
		if (info->fieldIndex == TAG_SVG_ATT_fill)	return GF_SG_SVG_FILL_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke) return GF_SG_SVG_STROKE_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_solid_color) return GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stop_color) return GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_color) return GF_SG_SVG_COLOR_DIRTY;
		break;
	case SVG_Number_datatype:
		if (info->fieldIndex == TAG_SVG_ATT_opacity) return GF_SG_SVG_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_fill_opacity) return GF_SG_SVG_FILLOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_opacity) return GF_SG_SVG_STROKEOPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_solid_opacity) return GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stop_opacity) return GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_line_increment) return GF_SG_SVG_LINEINCREMENT_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_miterlimit) return GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
		break;
	case SVG_Length_datatype:
		if (info->fieldIndex == TAG_SVG_ATT_stroke_dashoffset) return GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
		if (info->fieldIndex == TAG_SVG_ATT_stroke_width) return GF_SG_SVG_STROKEWIDTH_DIRTY;
		break;
	case SVG_DisplayAlign_datatype:
		return GF_SG_SVG_DISPLAYALIGN_DIRTY;
	case SVG_FillRule_datatype:
		return GF_SG_SVG_FILLRULE_DIRTY;
	case SVG_FontFamily_datatype:
		return GF_SG_SVG_FONTFAMILY_DIRTY;
	case SVG_FontSize_datatype:
		return GF_SG_SVG_FONTSIZE_DIRTY;
	case SVG_FontStyle_datatype:
		return GF_SG_SVG_FONTSTYLE_DIRTY;
	case SVG_FontVariant_datatype:
		return GF_SG_SVG_FONTVARIANT_DIRTY;
	case SVG_FontWeight_datatype:
		return GF_SG_SVG_FONTWEIGHT_DIRTY;
	case SVG_StrokeDashArray_datatype:
		return GF_SG_SVG_STROKEDASHARRAY_DIRTY;
	case SVG_StrokeLineCap_datatype:
		return GF_SG_SVG_STROKELINECAP_DIRTY;
	case SVG_StrokeLineJoin_datatype:
		return GF_SG_SVG_STROKELINEJOIN_DIRTY;
	case SVG_TextAlign_datatype:
		return GF_SG_SVG_TEXTPOSITION_DIRTY;
	case SVG_TextAnchor_datatype:
		return GF_SG_SVG_TEXTPOSITION_DIRTY;
	case SVG_Display_datatype:
		return GF_SG_SVG_DISPLAY_DIRTY;
	case SVG_VectorEffect_datatype:
		return GF_SG_SVG_VECTOREFFECT_DIRTY;
	}

	/* this is not a property but a regular attribute, the animatable attributes are at the moment:
		focusable, focusHighlight, gradientUnits, nav-*, target, xlink:href, xlink:type,


		the following affect the geometry of the element (some affect the positioning):
		cx, cy, d, height, offset, pathLength, points, r, rx, ry, width, x, x1, x2, y, y1, y2, rotate

		the following affect the positioning and are computed at each frame:
		transform, motion
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

	case XMLRI_datatype:
		return GF_SG_SVG_XLINK_HREF_DIRTY;
	/*for viewbox & PAR, use node dirty to force recomputing of the viewbox*/
	case SVG_PreserveAspectRatio_datatype:
	case SVG_ViewBox_datatype:
		return GF_SG_NODE_DIRTY;

	//case SVG_Matrix_datatype:
	//case SVG_Motion_datatype:

	default:
		return 0;
	}
}

/* NOTE: Some properties (audio-level, display, opacity, solid*, stop*, vector-effect, viewport*)
         are inherited only when they are  specified with the value 'inherit'
         otherwise they default to their initial value
		 which for the function below means NULL, the compositor will take care of the rest
 */
GF_EXPORT
u32 gf_svg_apply_inheritance(SVGAllAttributes *all_atts, SVGPropertiesPointers *render_svg_props)
{
	u32 inherited_flags_mask = GF_SG_NODE_DIRTY | GF_SG_CHILD_DIRTY;
	if(!all_atts || !render_svg_props) return ~inherited_flags_mask;

	if (!all_atts->audio_level) {
		render_svg_props->audio_level = NULL;
	} else {
		Fixed par_val = render_svg_props->computed_audio_level;
		Fixed val;
		if (all_atts->audio_level->type != SVG_NUMBER_INHERIT) {
			render_svg_props->audio_level = all_atts->audio_level;
			val = all_atts->audio_level->value;
		} else if (render_svg_props->audio_level) {
			val = render_svg_props->audio_level->value;
		} else {
			val = FIX_ONE;
		}
		par_val = MIN(FIX_ONE, MAX( par_val, 0));
		val = MIN(FIX_ONE, MAX( val, 0));
		render_svg_props->computed_audio_level = gf_mulfix(val, par_val);
	}

	if (all_atts->color && all_atts->color->type == SVG_PAINT_COLOR
	        && all_atts->color->color.type != SVG_COLOR_INHERIT) {
		render_svg_props->color = all_atts->color;
	} else {
		inherited_flags_mask |= GF_SG_SVG_COLOR_DIRTY;
	}
	if (all_atts->color_rendering && *(all_atts->color_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->color_rendering = all_atts->color_rendering;
	}
	if (all_atts->display && *(all_atts->display) != SVG_DISPLAY_INHERIT) {
		render_svg_props->display = all_atts->display;
	} else if (!all_atts->display) {
		render_svg_props->display = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_DISPLAY_DIRTY;
	}

	if (all_atts->display_align && *(all_atts->display_align) != SVG_DISPLAYALIGN_INHERIT) {
		render_svg_props->display_align = all_atts->display_align;
	} else {
		inherited_flags_mask |= GF_SG_SVG_DISPLAYALIGN_DIRTY;
	}
	if (all_atts->fill && all_atts->fill->type != SVG_PAINT_INHERIT) {
		render_svg_props->fill = all_atts->fill;
		if (all_atts->fill->type == SVG_PAINT_COLOR &&
		        all_atts->fill->color.type == SVG_COLOR_CURRENTCOLOR) {
			render_svg_props->fill = render_svg_props->color;
			if (inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY) {
				inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
			}
		}
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILL_DIRTY;
	}
	if (all_atts->fill_opacity && all_atts->fill_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->fill_opacity = all_atts->fill_opacity;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILLOPACITY_DIRTY;
	}
	if (all_atts->fill_rule && *(all_atts->fill_rule) != SVG_FILLRULE_INHERIT) {
		render_svg_props->fill_rule = all_atts->fill_rule;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FILLRULE_DIRTY;
	}
	if (all_atts->font_family && all_atts->font_family->type != SVG_FONTFAMILY_INHERIT) {
		render_svg_props->font_family = all_atts->font_family;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTFAMILY_DIRTY;
	}
	if (all_atts->font_size && all_atts->font_size->type != SVG_NUMBER_INHERIT) {
		render_svg_props->font_size = all_atts->font_size;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTSIZE_DIRTY;
	}
	if (all_atts->font_style && *(all_atts->font_style) != SVG_FONTSTYLE_INHERIT) {
		render_svg_props->font_style = all_atts->font_style;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTSTYLE_DIRTY;
	}
	if (all_atts->font_variant && *(all_atts->font_variant) != SVG_FONTVARIANT_INHERIT) {
		render_svg_props->font_variant = all_atts->font_variant;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTVARIANT_DIRTY;
	}
	if (all_atts->font_weight && *(all_atts->font_weight) != SVG_FONTWEIGHT_INHERIT) {
		render_svg_props->font_weight = all_atts->font_weight;
	} else {
		inherited_flags_mask |= GF_SG_SVG_FONTWEIGHT_DIRTY;
	}
	if (all_atts->image_rendering && *(all_atts->image_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->image_rendering = all_atts->image_rendering;
	}
	if (all_atts->line_increment && all_atts->line_increment->type != SVG_NUMBER_INHERIT) {
		render_svg_props->line_increment = all_atts->line_increment;
	} else {
		inherited_flags_mask |= GF_SG_SVG_LINEINCREMENT_DIRTY;
	}
	if (all_atts->opacity && all_atts->opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->opacity = all_atts->opacity;
	} else if (!all_atts->opacity) {
		render_svg_props->opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_OPACITY_DIRTY;
	}

	if (all_atts->pointer_events && *(all_atts->pointer_events) != SVG_POINTEREVENTS_INHERIT) {
		render_svg_props->pointer_events = all_atts->pointer_events;
	}
	if (all_atts->shape_rendering && *(all_atts->shape_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->shape_rendering = all_atts->shape_rendering;
	}
	if (all_atts->solid_color && all_atts->solid_color->type != SVG_PAINT_INHERIT) {
		render_svg_props->solid_color = all_atts->solid_color;
		if (all_atts->solid_color->type == SVG_PAINT_COLOR &&
		        all_atts->solid_color->color.type == SVG_COLOR_CURRENTCOLOR) {
			render_svg_props->solid_color = render_svg_props->color;
			if (inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY) {
				inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY;
			}
		}
	} else if (!all_atts->solid_color) {
		render_svg_props->solid_color = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY;
	}
	if (all_atts->solid_opacity && all_atts->solid_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->solid_opacity = all_atts->solid_opacity;
	} else if (!all_atts->solid_opacity) {
		render_svg_props->solid_opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_SOLIDCOLOR_OR_OPACITY_DIRTY;
	}
	if (all_atts->stop_color && all_atts->stop_color->type != SVG_PAINT_INHERIT) {
		render_svg_props->stop_color = all_atts->stop_color;
		if (all_atts->stop_color->type == SVG_PAINT_COLOR &&
		        all_atts->stop_color->color.type == SVG_COLOR_CURRENTCOLOR) {
			render_svg_props->stop_color = render_svg_props->color;
			if (inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY) {
				inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY;
			}
		}
	} else if (!all_atts->stop_color) {
		render_svg_props->stop_color = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY;
	}
	if (all_atts->stop_opacity && all_atts->stop_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stop_opacity = all_atts->stop_opacity;
	} else if (!all_atts->stop_opacity) {
		render_svg_props->stop_opacity = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STOPCOLOR_OR_OPACITY_DIRTY;
	}
	if (all_atts->stroke && all_atts->stroke->type != SVG_PAINT_INHERIT) {
		render_svg_props->stroke = all_atts->stroke;
		if (all_atts->stroke->type == SVG_PAINT_COLOR &&
		        all_atts->stroke->color.type == SVG_COLOR_CURRENTCOLOR) {
			render_svg_props->stroke = render_svg_props->color;
			if (inherited_flags_mask & GF_SG_SVG_COLOR_DIRTY) {
				inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
			}
		}
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKE_DIRTY;
	}
	if (all_atts->stroke_dasharray && all_atts->stroke_dasharray->type != SVG_STROKEDASHARRAY_INHERIT) {
		render_svg_props->stroke_dasharray = all_atts->stroke_dasharray;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEDASHARRAY_DIRTY;
	}
	if (all_atts->stroke_dashoffset && all_atts->stroke_dashoffset->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_dashoffset = all_atts->stroke_dashoffset;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEDASHOFFSET_DIRTY;
	}
	if (all_atts->stroke_linecap && *(all_atts->stroke_linecap) != SVG_STROKELINECAP_INHERIT) {
		render_svg_props->stroke_linecap = all_atts->stroke_linecap;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKELINECAP_DIRTY;
	}
	if (all_atts->stroke_linejoin && *(all_atts->stroke_linejoin) != SVG_STROKELINEJOIN_INHERIT) {
		render_svg_props->stroke_linejoin = all_atts->stroke_linejoin;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKELINEJOIN_DIRTY;
	}
	if (all_atts->stroke_miterlimit && all_atts->stroke_miterlimit->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_miterlimit = all_atts->stroke_miterlimit;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEMITERLIMIT_DIRTY;
	}
	if (all_atts->stroke_opacity && all_atts->stroke_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_opacity = all_atts->stroke_opacity;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEOPACITY_DIRTY;
	}
	if (all_atts->stroke_width && all_atts->stroke_width->type != SVG_NUMBER_INHERIT) {
		render_svg_props->stroke_width = all_atts->stroke_width;
	} else {
		inherited_flags_mask |= GF_SG_SVG_STROKEWIDTH_DIRTY;
	}
	if (all_atts->text_align && *(all_atts->text_align) != SVG_TEXTALIGN_INHERIT) {
		render_svg_props->text_align = all_atts->text_align;
	} else {
		inherited_flags_mask |= GF_SG_SVG_TEXTPOSITION_DIRTY;
	}
	if (all_atts->text_anchor && *(all_atts->text_anchor) != SVG_TEXTANCHOR_INHERIT) {
		render_svg_props->text_anchor = all_atts->text_anchor;
	} else {
		inherited_flags_mask |= GF_SG_SVG_TEXTPOSITION_DIRTY;
	}
	if (all_atts->text_rendering && *(all_atts->text_rendering) != SVG_RENDERINGHINT_INHERIT) {
		render_svg_props->text_rendering = all_atts->text_rendering;
	}
	if (all_atts->vector_effect && *(all_atts->vector_effect) != SVG_VECTOREFFECT_INHERIT) {
		render_svg_props->vector_effect = all_atts->vector_effect;
	} else if (!all_atts->vector_effect) {
		render_svg_props->vector_effect = NULL;
	} else {
		inherited_flags_mask |= GF_SG_SVG_VECTOREFFECT_DIRTY;
	}
	if (all_atts->viewport_fill && all_atts->viewport_fill->type != SVG_PAINT_INHERIT) {
		render_svg_props->viewport_fill = all_atts->viewport_fill;
	} else if (!all_atts->viewport_fill) {
		render_svg_props->viewport_fill = NULL;
	}
	if (all_atts->viewport_fill_opacity && all_atts->viewport_fill_opacity->type != SVG_NUMBER_INHERIT) {
		render_svg_props->viewport_fill_opacity = all_atts->viewport_fill_opacity;
	} else if (!all_atts->viewport_fill_opacity) {
		render_svg_props->viewport_fill_opacity = NULL;
	}
	if (all_atts->visibility && *(all_atts->visibility) != SVG_VISIBILITY_INHERIT) {
		render_svg_props->visibility = all_atts->visibility;
	}
	return inherited_flags_mask;
}



GF_EXPORT
void gf_svg_flatten_attributes(SVG_Element *e, SVGAllAttributes *all_atts)
{
	SVGAttribute *att;
	memset(all_atts, 0, sizeof(SVGAllAttributes));
	if (e->sgprivate->tag <= GF_NODE_FIRST_DOM_NODE_TAG) return;
	att = e->attributes;
	while (att) {
		switch(att->tag) {
		case TAG_XML_ATT_id:
			all_atts->xml_id = (SVG_ID *)att->data;
			break;
		case TAG_XML_ATT_base:
			all_atts->xml_base = (XMLRI *)att->data;
			break;
		case TAG_XML_ATT_lang:
			all_atts->xml_lang = (SVG_LanguageID *)att->data;
			break;
		case TAG_XML_ATT_space:
			all_atts->xml_space = (XML_Space *)att->data;
			break;

		case TAG_XLINK_ATT_type:
			all_atts->xlink_type = (SVG_String *)att->data;
			break;
		case TAG_XLINK_ATT_role:
			all_atts->xlink_role = (XMLRI *)att->data;
			break;
		case TAG_XLINK_ATT_arcrole:
			all_atts->xlink_arcrole = (XMLRI *)att->data;
			break;
		case TAG_XLINK_ATT_title:
			all_atts->xlink_title = (SVG_String *)att->data;
			break;
		case TAG_XLINK_ATT_href:
			all_atts->xlink_href = (XMLRI *)att->data;
			break;
		case TAG_XLINK_ATT_show:
			all_atts->xlink_show = (SVG_String *)att->data;
			break;
		case TAG_XLINK_ATT_actuate:
			all_atts->xlink_actuate = (SVG_String *)att->data;
			break;

		case TAG_XMLEV_ATT_event:
			all_atts->event = (XMLEV_Event *)att->data;
			break;
		case TAG_XMLEV_ATT_phase:
			all_atts->phase = (XMLEV_Phase *)att->data;
			break;
		case TAG_XMLEV_ATT_propagate:
			all_atts->propagate = (XMLEV_Propagate *)att->data;
			break;
		case TAG_XMLEV_ATT_defaultAction:
			all_atts->defaultAction = (XMLEV_DefaultAction *)att->data;
			break;
		case TAG_XMLEV_ATT_observer:
			all_atts->observer = (XML_IDREF *)att->data;
			break;
		case TAG_XMLEV_ATT_target:
			all_atts->listener_target = (XML_IDREF *)att->data;
			break;
		case TAG_XMLEV_ATT_handler:
			all_atts->handler = (XMLRI *)att->data;
			break;

		case TAG_LSR_ATT_enabled:
			all_atts->lsr_enabled = (SVG_Boolean *)att->data;
			break;

		case TAG_SVG_ATT_id:
			all_atts->id = (SVG_ID *)att->data;
			break;
		case TAG_SVG_ATT__class:
			all_atts->_class = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_requiredFeatures:
			all_atts->requiredFeatures = (SVG_ListOfIRI *)att->data;
			break;
		case TAG_SVG_ATT_requiredExtensions:
			all_atts->requiredExtensions = (SVG_ListOfIRI *)att->data;
			break;
		case TAG_SVG_ATT_requiredFormats:
			all_atts->requiredFormats = (SVG_FormatList *)att->data;
			break;
		case TAG_SVG_ATT_requiredFonts:
			all_atts->requiredFonts = (SVG_FontList *)att->data;
			break;
		case TAG_SVG_ATT_systemLanguage:
			all_atts->systemLanguage = (SVG_LanguageIDs *)att->data;
			break;
		case TAG_SVG_ATT_display:
			all_atts->display = (SVG_Display *)att->data;
			break;
		case TAG_SVG_ATT_visibility:
			all_atts->visibility = (SVG_Visibility *)att->data;
			break;
		case TAG_SVG_ATT_image_rendering:
			all_atts->image_rendering = (SVG_RenderingHint *)att->data;
			break;
		case TAG_SVG_ATT_pointer_events:
			all_atts->pointer_events = (SVG_PointerEvents *)att->data;
			break;
		case TAG_SVG_ATT_shape_rendering:
			all_atts->shape_rendering = (SVG_RenderingHint *)att->data;
			break;
		case TAG_SVG_ATT_text_rendering:
			all_atts->text_rendering = (SVG_RenderingHint *)att->data;
			break;
		case TAG_SVG_ATT_audio_level:
			all_atts->audio_level = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_viewport_fill:
			all_atts->viewport_fill = (SVG_Paint *)att->data;
			break;
		case TAG_SVG_ATT_viewport_fill_opacity:
			all_atts->viewport_fill_opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_overflow:
			all_atts->overflow = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_fill_opacity:
			all_atts->fill_opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_stroke_opacity:
			all_atts->stroke_opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_fill:
			all_atts->fill = (SVG_Paint *)att->data;
			break;
		case TAG_SVG_ATT_filter:
			all_atts->filter = (SVG_Paint *)att->data;
			break;
		case TAG_SVG_ATT_fill_rule:
			all_atts->fill_rule = (SVG_FillRule *)att->data;
			break;
		case TAG_SVG_ATT_stroke:
			all_atts->stroke = (SVG_Paint *)att->data;
			break;
		case TAG_SVG_ATT_stroke_dasharray:
			all_atts->stroke_dasharray = (SVG_StrokeDashArray *)att->data;
			break;
		case TAG_SVG_ATT_stroke_dashoffset:
			all_atts->stroke_dashoffset = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_stroke_linecap:
			all_atts->stroke_linecap = (SVG_StrokeLineCap *)att->data;
			break;
		case TAG_SVG_ATT_stroke_linejoin:
			all_atts->stroke_linejoin = (SVG_StrokeLineJoin *)att->data;
			break;
		case TAG_SVG_ATT_stroke_miterlimit:
			all_atts->stroke_miterlimit = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_stroke_width:
			all_atts->stroke_width = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_color:
			all_atts->color = (SVG_Paint *)att->data;
			break;
		case TAG_SVG_ATT_color_rendering:
			all_atts->color_rendering = (SVG_RenderingHint *)att->data;
			break;
		case TAG_SVG_ATT_vector_effect:
			all_atts->vector_effect = (SVG_VectorEffect *)att->data;
			break;
		case TAG_SVG_ATT_solid_color:
			all_atts->solid_color = (SVG_SVGColor *)att->data;
			break;
		case TAG_SVG_ATT_solid_opacity:
			all_atts->solid_opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_display_align:
			all_atts->display_align = (SVG_DisplayAlign *)att->data;
			break;
		case TAG_SVG_ATT_line_increment:
			all_atts->line_increment = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_stop_color:
			all_atts->stop_color = (SVG_SVGColor *)att->data;
			break;
		case TAG_SVG_ATT_stop_opacity:
			all_atts->stop_opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_font_family:
			all_atts->font_family = (SVG_FontFamily *)att->data;
			break;
		case TAG_SVG_ATT_font_size:
			all_atts->font_size = (SVG_FontSize *)att->data;
			break;
		case TAG_SVG_ATT_font_style:
			all_atts->font_style = (SVG_FontStyle *)att->data;
			break;
		case TAG_SVG_ATT_font_variant:
			all_atts->font_variant = (SVG_FontVariant *)att->data;
			break;
		case TAG_SVG_ATT_font_weight:
			all_atts->font_weight = (SVG_FontWeight *)att->data;
			break;
		case TAG_SVG_ATT_text_anchor:
			all_atts->text_anchor = (SVG_TextAnchor *)att->data;
			break;
		case TAG_SVG_ATT_text_align:
			all_atts->text_align = (SVG_TextAlign *)att->data;
			break;
		case TAG_SVG_ATT_text_decoration:
			all_atts->text_decoration = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_focusHighlight:
			all_atts->focusHighlight = (SVG_FocusHighlight *)att->data;
			break;
		case TAG_SVG_ATT_externalResourcesRequired:
			all_atts->externalResourcesRequired = (SVG_Boolean *)att->data;
			break;
		case TAG_SVG_ATT_focusable:
			all_atts->focusable = (SVG_Focusable *)att->data;
			break;
		case TAG_SVG_ATT_nav_next:
			all_atts->nav_next = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_prev:
			all_atts->nav_prev = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_up:
			all_atts->nav_up = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_up_right:
			all_atts->nav_up_right = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_right:
			all_atts->nav_right = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_down_right:
			all_atts->nav_down_right = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_down:
			all_atts->nav_down = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_down_left:
			all_atts->nav_down_left = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_left:
			all_atts->nav_left = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_nav_up_left:
			all_atts->nav_up_left = (SVG_Focus *)att->data;
			break;
		case TAG_SVG_ATT_transform:
			all_atts->transform = (SVG_Transform *)att->data;
			break;
		case TAG_SVG_ATT_target:
			all_atts->target = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_attributeName:
			all_atts->attributeName = (SMIL_AttributeName *)att->data;
			break;
		case TAG_SVG_ATT_attributeType:
			all_atts->attributeType = (SMIL_AttributeType *)att->data;
			break;
		case TAG_SVG_ATT_begin:
			all_atts->begin = (SMIL_Times *)att->data;
			break;
		case TAG_SVG_ATT_dur:
			all_atts->dur = (SMIL_Duration *)att->data;
			break;
		case TAG_SVG_ATT_end:
			all_atts->end = (SMIL_Times *)att->data;
			break;
		case TAG_SVG_ATT_repeatCount:
			all_atts->repeatCount = (SMIL_RepeatCount *)att->data;
			break;
		case TAG_SVG_ATT_repeatDur:
			all_atts->repeatDur = (SMIL_Duration *)att->data;
			break;
		case TAG_SVG_ATT_restart:
			all_atts->restart = (SMIL_Restart *)att->data;
			break;
		case TAG_SVG_ATT_smil_fill:
			all_atts->smil_fill = (SMIL_Fill *)att->data;
			break;
		case TAG_SVG_ATT_min:
			all_atts->min = (SMIL_Duration *)att->data;
			break;
		case TAG_SVG_ATT_max:
			all_atts->max = (SMIL_Duration *)att->data;
			break;
		case TAG_SVG_ATT_to:
			all_atts->to = (SMIL_AnimateValue *)att->data;
			break;
		case TAG_SVG_ATT_calcMode:
			all_atts->calcMode = (SMIL_CalcMode *)att->data;
			break;
		case TAG_SVG_ATT_values:
			all_atts->values = (SMIL_AnimateValues *)att->data;
			break;
		case TAG_SVG_ATT_keyTimes:
			all_atts->keyTimes = (SMIL_KeyTimes *)att->data;
			break;
		case TAG_SVG_ATT_keySplines:
			all_atts->keySplines = (SMIL_KeySplines *)att->data;
			break;
		case TAG_SVG_ATT_from:
			all_atts->from = (SMIL_AnimateValue *)att->data;
			break;
		case TAG_SVG_ATT_by:
			all_atts->by = (SMIL_AnimateValue *)att->data;
			break;
		case TAG_SVG_ATT_additive:
			all_atts->additive = (SMIL_Additive *)att->data;
			break;
		case TAG_SVG_ATT_accumulate:
			all_atts->accumulate = (SMIL_Accumulate *)att->data;
			break;
		case TAG_SVG_ATT_path:
			all_atts->path = (SVG_PathData *)att->data;
			break;
		case TAG_SVG_ATT_keyPoints:
			all_atts->keyPoints = (SMIL_KeyPoints *)att->data;
			break;
		case TAG_SVG_ATT_rotate:
			all_atts->rotate = (SVG_Rotate *)att->data;
			break;
		case TAG_SVG_ATT_origin:
			all_atts->origin = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_transform_type:
			all_atts->transform_type = (SVG_TransformType *)att->data;
			break;
		case TAG_SVG_ATT_clipBegin:
			all_atts->clipBegin = (SVG_Clock *)att->data;
			break;
		case TAG_SVG_ATT_clipEnd:
			all_atts->clipEnd = (SVG_Clock *)att->data;
			break;
		case TAG_SVG_ATT_syncBehavior:
			all_atts->syncBehavior = (SMIL_SyncBehavior *)att->data;
			break;
		case TAG_SVG_ATT_syncTolerance:
			all_atts->syncTolerance = (SMIL_SyncTolerance *)att->data;
			break;
		case TAG_SVG_ATT_syncMaster:
			all_atts->syncMaster = (SVG_Boolean *)att->data;
			break;
		case TAG_SVG_ATT_syncReference:
			all_atts->syncReference = (XMLRI *)att->data;
			break;
		case TAG_SVG_ATT_x:
			all_atts->x = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_y:
			all_atts->y = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_width:
			all_atts->width = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_height:
			all_atts->height = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_preserveAspectRatio:
			all_atts->preserveAspectRatio = (SVG_PreserveAspectRatio *)att->data;
			break;
		case TAG_SVG_ATT_initialVisibility:
			all_atts->initialVisibility = (SVG_InitialVisibility *)att->data;
			break;
		case TAG_SVG_ATT_type:
			all_atts->type = (SVG_ContentType *)att->data;
			break;
		case TAG_SVG_ATT_cx:
			all_atts->cx = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_cy:
			all_atts->cy = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_r:
			all_atts->r = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_cursorManager_x:
			all_atts->cursorManager_x = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_cursorManager_y:
			all_atts->cursorManager_y = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_rx:
			all_atts->rx = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_ry:
			all_atts->ry = (SVG_Length *)att->data;
			break;
		case TAG_SVG_ATT_horiz_adv_x:
			all_atts->horiz_adv_x = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_horiz_origin_x:
			all_atts->horiz_origin_x = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_font_stretch:
			all_atts->font_stretch = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_unicode_range:
			all_atts->unicode_range = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_panose_1:
			all_atts->panose_1 = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_widths:
			all_atts->widths = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_bbox:
			all_atts->bbox = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_units_per_em:
			all_atts->units_per_em = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_stemv:
			all_atts->stemv = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_stemh:
			all_atts->stemh = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_slope:
			all_atts->slope = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_cap_height:
			all_atts->cap_height = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_x_height:
			all_atts->x_height = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_accent_height:
			all_atts->accent_height = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_ascent:
			all_atts->ascent = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_descent:
			all_atts->descent = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_ideographic:
			all_atts->ideographic = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_alphabetic:
			all_atts->alphabetic = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_mathematical:
			all_atts->mathematical = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_hanging:
			all_atts->hanging = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_underline_position:
			all_atts->underline_position = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_underline_thickness:
			all_atts->underline_thickness = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_strikethrough_position:
			all_atts->strikethrough_position = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_strikethrough_thickness:
			all_atts->strikethrough_thickness = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_overline_position:
			all_atts->overline_position = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_overline_thickness:
			all_atts->overline_thickness = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_d:
			all_atts->d = (SVG_PathData *)att->data;
			break;
		case TAG_SVG_ATT_unicode:
			all_atts->unicode = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_glyph_name:
			all_atts->glyph_name = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_arabic_form:
			all_atts->arabic_form = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_lang:
			all_atts->lang = (SVG_LanguageIDs *)att->data;
			break;
		case TAG_SVG_ATT_u1:
			all_atts->u1 = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_g1:
			all_atts->g1 = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_u2:
			all_atts->u2 = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_g2:
			all_atts->g2 = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_k:
			all_atts->k = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_opacity:
			all_atts->opacity = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_x1:
			all_atts->x1 = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_y1:
			all_atts->y1 = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_x2:
			all_atts->x2 = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_y2:
			all_atts->y2 = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_gradientUnits:
			all_atts->gradientUnits = (SVG_GradientUnit *)att->data;
			break;
		case TAG_SVG_ATT_filterUnits:
			all_atts->focusable = (SVG_GradientUnit *)att->data;
			break;
		case TAG_SVG_ATT_spreadMethod:
			all_atts->spreadMethod = (SVG_SpreadMethod *)att->data;
			break;
		case TAG_SVG_ATT_gradientTransform:
			all_atts->gradientTransform = (SVG_Transform *)att->data;
			break;
		case TAG_SVG_ATT_pathLength:
			all_atts->pathLength = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_points:
			all_atts->points = (SVG_Points *)att->data;
			break;
		case TAG_SVG_ATT_mediaSize:
			all_atts->mediaSize = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_mediaTime:
			all_atts->mediaTime = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_mediaCharacterEncoding:
			all_atts->mediaCharacterEncoding = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_mediaContentEncodings:
			all_atts->mediaContentEncodings = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_bandwidth:
			all_atts->bandwidth = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_fx:
			all_atts->fx = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_fy:
			all_atts->fy = (SVG_Coordinate *)att->data;
			break;
		case TAG_SVG_ATT_size:
			all_atts->size = (LASeR_Size *)att->data;
			break;
		case TAG_SVG_ATT_choice:
			all_atts->choice = (LASeR_Choice *)att->data;
			break;
		case TAG_SVG_ATT_delta:
			all_atts->delta = (LASeR_Size *)att->data;
			break;
		case TAG_SVG_ATT_offset:
			all_atts->offset = (SVG_Number *)att->data;
			break;
		case TAG_SVG_ATT_syncBehaviorDefault:
			all_atts->syncBehaviorDefault = (SMIL_SyncBehavior *)att->data;
			break;
		case TAG_SVG_ATT_syncToleranceDefault:
			all_atts->syncToleranceDefault = (SMIL_SyncTolerance *)att->data;
			break;
		case TAG_SVG_ATT_viewBox:
			all_atts->viewBox = (SVG_ViewBox *)att->data;
			break;
		case TAG_SVG_ATT_zoomAndPan:
			all_atts->zoomAndPan = (SVG_ZoomAndPan *)att->data;
			break;
		case TAG_SVG_ATT_version:
			all_atts->version = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_baseProfile:
			all_atts->baseProfile = (SVG_String *)att->data;
			break;
		case TAG_SVG_ATT_contentScriptType:
			all_atts->contentScriptType = (SVG_ContentType *)att->data;
			break;
		case TAG_SVG_ATT_snapshotTime:
			all_atts->snapshotTime = (SVG_Clock *)att->data;
			break;
		case TAG_SVG_ATT_timelineBegin:
			all_atts->timelineBegin = (SVG_TimelineBegin *)att->data;
			break;
		case TAG_SVG_ATT_playbackOrder:
			all_atts->playbackOrder = (SVG_PlaybackOrder *)att->data;
			break;
		case TAG_SVG_ATT_editable:
			all_atts->editable = (SVG_Boolean *)att->data;
			break;
		case TAG_SVG_ATT_text_x:
			all_atts->text_x = (SVG_Coordinates *)att->data;
			break;
		case TAG_SVG_ATT_text_y:
			all_atts->text_y = (SVG_Coordinates *)att->data;
			break;
		case TAG_SVG_ATT_text_rotate:
			all_atts->text_rotate = (SVG_Numbers *)att->data;
			break;
		case TAG_SVG_ATT_transformBehavior:
			all_atts->transformBehavior = (SVG_TransformBehavior *)att->data;
			break;
		case TAG_SVG_ATT_overlay:
			all_atts->overlay = (SVG_Overlay *)att->data;
			break;
		case TAG_SVG_ATT_fullscreen:
			all_atts->fullscreen = (SVG_Boolean *)att->data;
			break;
		case TAG_SVG_ATT_motionTransform:
			all_atts->motionTransform = (SVG_Motion *)att->data;
			break;
		case TAG_SVG_ATT_clip_path:
			all_atts->clip_path = (SVG_ClipPath *)att->data;
			break;

		case TAG_GSVG_ATT_useAsPrimary:
			all_atts->gpac_useAsPrimary = (SVG_Boolean *)att->data;
			break;
		case TAG_GSVG_ATT_depthOffset:
			all_atts->gpac_depthOffset = (SVG_Number *)att->data;
			break;
		case TAG_GSVG_ATT_depthGain:
			all_atts->gpac_depthGain = (SVG_Number *)att->data;
			break;
		}

		att = att->next;
	}
}

#endif

