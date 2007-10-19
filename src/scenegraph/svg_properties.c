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


void *gf_svg_get_property_pointer_from_tag(SVGPropertiesPointers *output_property_context, u32 prop_tag)
{
	switch (prop_tag) {
		case TAG_SVG_ATT_audio_level: return output_property_context->audio_level;
		case TAG_SVG_ATT_color: return output_property_context->color;
		case TAG_SVG_ATT_color_rendering: return output_property_context->color_rendering;
		case TAG_SVG_ATT_display: return output_property_context->display;
		case TAG_SVG_ATT_display_align: return output_property_context->display_align;
		case TAG_SVG_ATT_fill: return output_property_context->fill;
		case TAG_SVG_ATT_fill_opacity: return output_property_context->fill_opacity;
		case TAG_SVG_ATT_fill_rule: return output_property_context->fill_rule;
		case TAG_SVG_ATT_font_family: return output_property_context->font_family;
		case TAG_SVG_ATT_font_size: return output_property_context->font_size;
		case TAG_SVG_ATT_font_style: return output_property_context->font_style;
		case TAG_SVG_ATT_font_variant: return output_property_context->font_variant;
		case TAG_SVG_ATT_font_weight: return output_property_context->font_weight;
		case TAG_SVG_ATT_image_rendering: return output_property_context->image_rendering;
		case TAG_SVG_ATT_line_increment: return output_property_context->line_increment;
		case TAG_SVG_ATT_opacity: return output_property_context->opacity;
		case TAG_SVG_ATT_pointer_events: return output_property_context->pointer_events;
		case TAG_SVG_ATT_shape_rendering: return output_property_context->shape_rendering;
		case TAG_SVG_ATT_solid_color: return output_property_context->solid_color;
		case TAG_SVG_ATT_solid_opacity: return output_property_context->solid_opacity;
		case TAG_SVG_ATT_stop_color: return output_property_context->stop_color;
		case TAG_SVG_ATT_stop_opacity: return output_property_context->stop_opacity;
		case TAG_SVG_ATT_stroke: return output_property_context->stroke;
		case TAG_SVG_ATT_stroke_dasharray: return output_property_context->stroke_dasharray;
		case TAG_SVG_ATT_stroke_dashoffset: return output_property_context->stroke_dashoffset;
		case TAG_SVG_ATT_stroke_linecap: return output_property_context->stroke_linecap;
		case TAG_SVG_ATT_stroke_linejoin: return output_property_context->stroke_linejoin;
		case TAG_SVG_ATT_stroke_miterlimit: return output_property_context->stroke_miterlimit;
		case TAG_SVG_ATT_stroke_opacity: return output_property_context->stroke_opacity;
		case TAG_SVG_ATT_stroke_width: return output_property_context->stroke_width;
		case TAG_SVG_ATT_text_align: return output_property_context->text_align;
		case TAG_SVG_ATT_text_anchor: return output_property_context->text_anchor;
		case TAG_SVG_ATT_text_rendering: return output_property_context->text_rendering;
		case TAG_SVG_ATT_vector_effect: return output_property_context->vector_effect;
		case TAG_SVG_ATT_viewport_fill: return output_property_context->viewport_fill;
		case TAG_SVG_ATT_viewport_fill_opacity: return output_property_context->viewport_fill_opacity;
		case TAG_SVG_ATT_visibility: return output_property_context->visibility;
		default:return NULL;
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
