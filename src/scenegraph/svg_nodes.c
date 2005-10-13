/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
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


/*
	DO NOT MOFIFY - File generated on GMT Fri Oct 07 08:46:55 2005

	BY SVGGen for GPAC Version 0.4.1-DEV
*/

#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

static void SVG_a_Del(GF_Node *node)
{
	SVGaElement *p = (SVGaElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_ResetIRI(&p->xlink_href);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_a_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGaElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGaElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGaElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGaElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGaElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGaElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGaElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_type;
			return GF_OK;
		case 26:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_role;
			return GF_OK;
		case 27:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_arcrole;
			return GF_OK;
		case 28:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_title;
			return GF_OK;
		case 29:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_href;
			return GF_OK;
		case 30:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_show;
			return GF_OK;
		case 31:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->xlink_actuate;
			return GF_OK;
		case 32:
			info->name = "target";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaElement *)node)->target;
			return GF_OK;
		case 33:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGaElement *)node)->display;
			return GF_OK;
		case 34:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGaElement *)node)->visibility;
			return GF_OK;
		case 35:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->image_rendering;
			return GF_OK;
		case 36:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->pointer_events;
			return GF_OK;
		case 37:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->shape_rendering;
			return GF_OK;
		case 38:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->text_rendering;
			return GF_OK;
		case 39:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGaElement *)node)->audio_level;
			return GF_OK;
		case 40:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGaElement *)node)->fill_opacity;
			return GF_OK;
		case 41:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_opacity;
			return GF_OK;
		case 42:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGaElement *)node)->fill;
			return GF_OK;
		case 43:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGaElement *)node)->fill_rule;
			return GF_OK;
		case 44:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke;
			return GF_OK;
		case 45:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_dasharray;
			return GF_OK;
		case 46:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 47:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_linecap;
			return GF_OK;
		case 48:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_linejoin;
			return GF_OK;
		case 49:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 50:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stroke_width;
			return GF_OK;
		case 51:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGaElement *)node)->color;
			return GF_OK;
		case 52:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->color_rendering;
			return GF_OK;
		case 53:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->vector_effect;
			return GF_OK;
		case 54:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGaElement *)node)->viewport_fill;
			return GF_OK;
		case 55:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGaElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 56:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGaElement *)node)->solid_color;
			return GF_OK;
		case 57:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGaElement *)node)->solid_opacity;
			return GF_OK;
		case 58:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaElement *)node)->display_align;
			return GF_OK;
		case 59:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGaElement *)node)->line_increment;
			return GF_OK;
		case 60:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stop_color;
			return GF_OK;
		case 61:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGaElement *)node)->stop_opacity;
			return GF_OK;
		case 62:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGaElement *)node)->font_family;
			return GF_OK;
		case 63:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGaElement *)node)->font_size;
			return GF_OK;
		case 64:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGaElement *)node)->font_style;
			return GF_OK;
		case 65:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGaElement *)node)->font_weight;
			return GF_OK;
		case 66:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGaElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_a()
{
	SVGaElement *p;
	GF_SAFEALLOC(p, sizeof(SVGaElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_a);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "a";
	((GF_Node *p)->sgprivate->node_del = SVG_a_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_a_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_animate_Del(GF_Node *node)
{
	SVGanimateElement *p = (SVGanimateElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SMIL_DeleteAnimateValue(&(p->to));
	SMIL_DeleteAnimateValues(&(p->values));
	SMIL_DeleteAnimateValue(&(p->from));
	SMIL_DeleteAnimateValue(&(p->by));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_animate_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_show;
			return GF_OK;
		case 9:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_actuate;
			return GF_OK;
		case 10:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_type;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->xlink_title;
			return GF_OK;
		case 14:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->attributeName;
			return GF_OK;
		case 15:
			info->name = "attributeType";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->attributeType;
			return GF_OK;
		case 16:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->begin;
			return GF_OK;
		case 17:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->dur;
			return GF_OK;
		case 18:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->end;
			return GF_OK;
		case 19:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->repeatCount;
			return GF_OK;
		case 20:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->repeatDur;
			return GF_OK;
		case 21:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->restart;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->fill;
			return GF_OK;
		case 23:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->min;
			return GF_OK;
		case 24:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->max;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->to;
			return GF_OK;
		case 26:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->calcMode;
			return GF_OK;
		case 27:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->values;
			return GF_OK;
		case 28:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->keyTimes;
			return GF_OK;
		case 29:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->keySplines;
			return GF_OK;
		case 30:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->from;
			return GF_OK;
		case 31:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->by;
			return GF_OK;
		case 32:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->additive;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = & ((SVGanimateElement *)node)->accumulate;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_animate()
{
	SVGanimateElement *p;
	GF_SAFEALLOC(p, sizeof(SVGanimateElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_animate);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animate";
	((GF_Node *p)->sgprivate->node_del = SVG_animate_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_animate_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->min.type = SMIL_DURATION_DEFINED;
	p->values.values = gf_list_new();
	p->keyTimes = gf_list_new();
	p->keySplines = gf_list_new();
	return p;
}

static void SVG_animateColor_Del(GF_Node *node)
{
	SVGanimateColorElement *p = (SVGanimateColorElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SMIL_DeleteAnimateValue(&(p->to));
	SMIL_DeleteAnimateValues(&(p->values));
	SMIL_DeleteAnimateValue(&(p->from));
	SMIL_DeleteAnimateValue(&(p->by));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_animateColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_show;
			return GF_OK;
		case 9:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_actuate;
			return GF_OK;
		case 10:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_type;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->xlink_title;
			return GF_OK;
		case 14:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->attributeName;
			return GF_OK;
		case 15:
			info->name = "attributeType";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->attributeType;
			return GF_OK;
		case 16:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->begin;
			return GF_OK;
		case 17:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->dur;
			return GF_OK;
		case 18:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->end;
			return GF_OK;
		case 19:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->repeatCount;
			return GF_OK;
		case 20:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->repeatDur;
			return GF_OK;
		case 21:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->restart;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->fill;
			return GF_OK;
		case 23:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->min;
			return GF_OK;
		case 24:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->max;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->to;
			return GF_OK;
		case 26:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->calcMode;
			return GF_OK;
		case 27:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->values;
			return GF_OK;
		case 28:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->keyTimes;
			return GF_OK;
		case 29:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->keySplines;
			return GF_OK;
		case 30:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->from;
			return GF_OK;
		case 31:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->by;
			return GF_OK;
		case 32:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->additive;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = & ((SVGanimateColorElement *)node)->accumulate;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_animateColor()
{
	SVGanimateColorElement *p;
	GF_SAFEALLOC(p, sizeof(SVGanimateColorElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_animateColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateColor";
	((GF_Node *p)->sgprivate->node_del = SVG_animateColor_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_animateColor_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->min.type = SMIL_DURATION_DEFINED;
	p->values.values = gf_list_new();
	p->keyTimes = gf_list_new();
	p->keySplines = gf_list_new();
	return p;
}

static void SVG_animateMotion_Del(GF_Node *node)
{
	SVGanimateMotionElement *p = (SVGanimateMotionElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SMIL_DeleteAnimateValue(&(p->to));
	SMIL_DeleteAnimateValues(&(p->values));
	SMIL_DeleteAnimateValue(&(p->from));
	SMIL_DeleteAnimateValue(&(p->by));
	SVG_DeletePath(&(p->path));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_animateMotion_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_show;
			return GF_OK;
		case 9:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_actuate;
			return GF_OK;
		case 10:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_type;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->xlink_title;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->begin;
			return GF_OK;
		case 15:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->dur;
			return GF_OK;
		case 16:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->end;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->restart;
			return GF_OK;
		case 20:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->fill;
			return GF_OK;
		case 21:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->min;
			return GF_OK;
		case 22:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->max;
			return GF_OK;
		case 23:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->additive;
			return GF_OK;
		case 24:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->accumulate;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->to;
			return GF_OK;
		case 26:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->calcMode;
			return GF_OK;
		case 27:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->values;
			return GF_OK;
		case 28:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->keyTimes;
			return GF_OK;
		case 29:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->keySplines;
			return GF_OK;
		case 30:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->from;
			return GF_OK;
		case 31:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->by;
			return GF_OK;
		case 32:
			info->name = "type";
			info->fieldType = SVG_TransformType_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->type;
			return GF_OK;
		case 33:
			info->name = "path";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->path;
			return GF_OK;
		case 34:
			info->name = "keyPoints";
			info->fieldType = SMIL_KeyPoints_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->keyPoints;
			return GF_OK;
		case 35:
			info->name = "rotate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->rotate;
			return GF_OK;
		case 36:
			info->name = "origin";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateMotionElement *)node)->origin;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_animateMotion()
{
	SVGanimateMotionElement *p;
	GF_SAFEALLOC(p, sizeof(SVGanimateMotionElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_animateMotion);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateMotion";
	((GF_Node *p)->sgprivate->node_del = SVG_animateMotion_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_animateMotion_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->min.type = SMIL_DURATION_DEFINED;
	p->calcMode = SMIL_CALCMODE_PACED;
	p->values.values = gf_list_new();
	p->keyTimes = gf_list_new();
	p->keySplines = gf_list_new();
	p->path.commands = gf_list_new();
	p->path.points = gf_list_new();
	p->keyPoints = gf_list_new();
	return p;
}

static void SVG_animateTransform_Del(GF_Node *node)
{
	SVGanimateTransformElement *p = (SVGanimateTransformElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SMIL_DeleteAnimateValue(&(p->to));
	SMIL_DeleteAnimateValues(&(p->values));
	SMIL_DeleteAnimateValue(&(p->from));
	SMIL_DeleteAnimateValue(&(p->by));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_animateTransform_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_show;
			return GF_OK;
		case 9:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_actuate;
			return GF_OK;
		case 10:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_type;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->xlink_title;
			return GF_OK;
		case 14:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->attributeName;
			return GF_OK;
		case 15:
			info->name = "attributeType";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->attributeType;
			return GF_OK;
		case 16:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->begin;
			return GF_OK;
		case 17:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->dur;
			return GF_OK;
		case 18:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->end;
			return GF_OK;
		case 19:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->repeatCount;
			return GF_OK;
		case 20:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->repeatDur;
			return GF_OK;
		case 21:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->restart;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->fill;
			return GF_OK;
		case 23:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->min;
			return GF_OK;
		case 24:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->max;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->to;
			return GF_OK;
		case 26:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->calcMode;
			return GF_OK;
		case 27:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->values;
			return GF_OK;
		case 28:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->keyTimes;
			return GF_OK;
		case 29:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->keySplines;
			return GF_OK;
		case 30:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->from;
			return GF_OK;
		case 31:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->by;
			return GF_OK;
		case 32:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->additive;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->accumulate;
			return GF_OK;
		case 34:
			info->name = "type";
			info->fieldType = SVG_TransformType_datatype;
			info->far_ptr = & ((SVGanimateTransformElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_animateTransform()
{
	SVGanimateTransformElement *p;
	GF_SAFEALLOC(p, sizeof(SVGanimateTransformElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_animateTransform);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animateTransform";
	((GF_Node *p)->sgprivate->node_del = SVG_animateTransform_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_animateTransform_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->min.type = SMIL_DURATION_DEFINED;
	p->values.values = gf_list_new();
	p->keyTimes = gf_list_new();
	p->keySplines = gf_list_new();
	return p;
}

static void SVG_animation_Del(GF_Node *node)
{
	SVGanimationElement *p = (SVGanimationElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SVG_DeleteTransformList(p->transform);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_animation_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 13:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_actuate;
			return GF_OK;
		case 14:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_type;
			return GF_OK;
		case 15:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_role;
			return GF_OK;
		case 16:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_arcrole;
			return GF_OK;
		case 17:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_title;
			return GF_OK;
		case 18:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_href;
			return GF_OK;
		case 19:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->xlink_show;
			return GF_OK;
		case 20:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusable;
			return GF_OK;
		case 21:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusNext;
			return GF_OK;
		case 22:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusPrev;
			return GF_OK;
		case 23:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusNorth;
			return GF_OK;
		case 24:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusNorthEast;
			return GF_OK;
		case 25:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusEast;
			return GF_OK;
		case 26:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusSouthEast;
			return GF_OK;
		case 27:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusSouth;
			return GF_OK;
		case 28:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusSouthWest;
			return GF_OK;
		case 29:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusWest;
			return GF_OK;
		case 30:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->focusNorthWest;
			return GF_OK;
		case 31:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->begin;
			return GF_OK;
		case 32:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->dur;
			return GF_OK;
		case 33:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->end;
			return GF_OK;
		case 34:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->repeatCount;
			return GF_OK;
		case 35:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->repeatDur;
			return GF_OK;
		case 36:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->restart;
			return GF_OK;
		case 37:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->fill;
			return GF_OK;
		case 38:
			info->name = "syncBehavior";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->syncBehavior;
			return GF_OK;
		case 39:
			info->name = "syncBehaviorDefault";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->syncBehaviorDefault;
			return GF_OK;
		case 40:
			info->name = "syncTolerance";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->syncTolerance;
			return GF_OK;
		case 41:
			info->name = "syncToleranceDefault";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->syncToleranceDefault;
			return GF_OK;
		case 42:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->x;
			return GF_OK;
		case 43:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->y;
			return GF_OK;
		case 44:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->width;
			return GF_OK;
		case 45:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->height;
			return GF_OK;
		case 46:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 47:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->transform;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_animation()
{
	SVGanimationElement *p;
	GF_SAFEALLOC(p, sizeof(SVGanimationElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_animation);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "animation";
	((GF_Node *p)->sgprivate->node_del = SVG_animation_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_animation_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->transform = gf_list_new();
	return p;
}

static void SVG_audio_Del(GF_Node *node)
{
	SVGaudioElement *p = (SVGaudioElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_audio_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_actuate;
			return GF_OK;
		case 8:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_type;
			return GF_OK;
		case 9:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_role;
			return GF_OK;
		case 10:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_arcrole;
			return GF_OK;
		case 11:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_title;
			return GF_OK;
		case 12:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_href;
			return GF_OK;
		case 13:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->xlink_show;
			return GF_OK;
		case 14:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->requiredFeatures;
			return GF_OK;
		case 15:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->requiredExtensions;
			return GF_OK;
		case 16:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->requiredFormats;
			return GF_OK;
		case 17:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->requiredFonts;
			return GF_OK;
		case 18:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->systemLanguage;
			return GF_OK;
		case 19:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 20:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->begin;
			return GF_OK;
		case 21:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->dur;
			return GF_OK;
		case 22:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->end;
			return GF_OK;
		case 23:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->repeatCount;
			return GF_OK;
		case 24:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->repeatDur;
			return GF_OK;
		case 25:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->restart;
			return GF_OK;
		case 26:
			info->name = "syncBehavior";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->syncBehavior;
			return GF_OK;
		case 27:
			info->name = "syncBehaviorDefault";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->syncBehaviorDefault;
			return GF_OK;
		case 28:
			info->name = "syncTolerance";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->syncTolerance;
			return GF_OK;
		case 29:
			info->name = "syncToleranceDefault";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->syncToleranceDefault;
			return GF_OK;
		case 30:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGaudioElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_audio()
{
	SVGaudioElement *p;
	GF_SAFEALLOC(p, sizeof(SVGaudioElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_audio);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "audio";
	((GF_Node *p)->sgprivate->node_del = SVG_audio_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_audio_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	return p;
}

static void SVG_circle_Del(GF_Node *node)
{
	SVGcircleElement *p = (SVGcircleElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_circle_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->cx;
			return GF_OK;
		case 25:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->cy;
			return GF_OK;
		case 26:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->r;
			return GF_OK;
		case 27:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->display;
			return GF_OK;
		case 28:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->visibility;
			return GF_OK;
		case 29:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->image_rendering;
			return GF_OK;
		case 30:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->pointer_events;
			return GF_OK;
		case 31:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->shape_rendering;
			return GF_OK;
		case 32:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->text_rendering;
			return GF_OK;
		case 33:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->audio_level;
			return GF_OK;
		case 34:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->fill_opacity;
			return GF_OK;
		case 35:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_opacity;
			return GF_OK;
		case 36:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->fill;
			return GF_OK;
		case 37:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->fill_rule;
			return GF_OK;
		case 38:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke;
			return GF_OK;
		case 39:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_dasharray;
			return GF_OK;
		case 40:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 41:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_linecap;
			return GF_OK;
		case 42:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_linejoin;
			return GF_OK;
		case 43:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 44:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stroke_width;
			return GF_OK;
		case 45:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->color;
			return GF_OK;
		case 46:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->color_rendering;
			return GF_OK;
		case 47:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->vector_effect;
			return GF_OK;
		case 48:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->viewport_fill;
			return GF_OK;
		case 49:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 50:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->solid_color;
			return GF_OK;
		case 51:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->solid_opacity;
			return GF_OK;
		case 52:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->display_align;
			return GF_OK;
		case 53:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->line_increment;
			return GF_OK;
		case 54:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stop_color;
			return GF_OK;
		case 55:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->stop_opacity;
			return GF_OK;
		case 56:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->font_family;
			return GF_OK;
		case 57:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->font_size;
			return GF_OK;
		case 58:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->font_style;
			return GF_OK;
		case 59:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->font_weight;
			return GF_OK;
		case 60:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_circle()
{
	SVGcircleElement *p;
	GF_SAFEALLOC(p, sizeof(SVGcircleElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_circle);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "circle";
	((GF_Node *p)->sgprivate->node_del = SVG_circle_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_circle_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_defs_Del(GF_Node *node)
{
	SVGdefsElement *p = (SVGdefsElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_defs_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->display;
			return GF_OK;
		case 8:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->visibility;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->text_rendering;
			return GF_OK;
		case 13:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->audio_level;
			return GF_OK;
		case 14:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->fill_opacity;
			return GF_OK;
		case 15:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_opacity;
			return GF_OK;
		case 16:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->fill;
			return GF_OK;
		case 17:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->fill_rule;
			return GF_OK;
		case 18:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke;
			return GF_OK;
		case 19:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_dasharray;
			return GF_OK;
		case 20:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 21:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_linecap;
			return GF_OK;
		case 22:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_linejoin;
			return GF_OK;
		case 23:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 24:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stroke_width;
			return GF_OK;
		case 25:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->color;
			return GF_OK;
		case 26:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->color_rendering;
			return GF_OK;
		case 27:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->vector_effect;
			return GF_OK;
		case 28:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->viewport_fill;
			return GF_OK;
		case 29:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 30:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->solid_color;
			return GF_OK;
		case 31:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->solid_opacity;
			return GF_OK;
		case 32:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->display_align;
			return GF_OK;
		case 33:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->line_increment;
			return GF_OK;
		case 34:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stop_color;
			return GF_OK;
		case 35:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->stop_opacity;
			return GF_OK;
		case 36:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->font_family;
			return GF_OK;
		case 37:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->font_size;
			return GF_OK;
		case 38:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->font_style;
			return GF_OK;
		case 39:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->font_weight;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGdefsElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_defs()
{
	SVGdefsElement *p;
	GF_SAFEALLOC(p, sizeof(SVGdefsElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_defs);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "defs";
	((GF_Node *p)->sgprivate->node_del = SVG_defs_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_defs_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_desc_Del(GF_Node *node)
{
	SVGdescElement *p = (SVGdescElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_desc_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdescElement *)node)->xml_space;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_desc()
{
	SVGdescElement *p;
	GF_SAFEALLOC(p, sizeof(SVGdescElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_desc);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "desc";
	((GF_Node *p)->sgprivate->node_del = SVG_desc_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_desc_get_attribute;
#endif
	return p;
}

static void SVG_discard_Del(GF_Node *node)
{
	SVGdiscardElement *p = (SVGdiscardElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_discard_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_show;
			return GF_OK;
		case 8:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_actuate;
			return GF_OK;
		case 9:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_type;
			return GF_OK;
		case 10:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_role;
			return GF_OK;
		case 11:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_arcrole;
			return GF_OK;
		case 12:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_title;
			return GF_OK;
		case 13:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->xlink_href;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGdiscardElement *)node)->begin;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_discard()
{
	SVGdiscardElement *p;
	GF_SAFEALLOC(p, sizeof(SVGdiscardElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_discard);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "discard";
	((GF_Node *p)->sgprivate->node_del = SVG_discard_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_discard_get_attribute;
#endif
	p->begin = gf_list_new();
	return p;
}

static void SVG_ellipse_Del(GF_Node *node)
{
	SVGellipseElement *p = (SVGellipseElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_ellipse_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->rx;
			return GF_OK;
		case 25:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->ry;
			return GF_OK;
		case 26:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->cx;
			return GF_OK;
		case 27:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->cy;
			return GF_OK;
		case 28:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->display;
			return GF_OK;
		case 29:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->visibility;
			return GF_OK;
		case 30:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->image_rendering;
			return GF_OK;
		case 31:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->pointer_events;
			return GF_OK;
		case 32:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->shape_rendering;
			return GF_OK;
		case 33:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->text_rendering;
			return GF_OK;
		case 34:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->audio_level;
			return GF_OK;
		case 35:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->fill_opacity;
			return GF_OK;
		case 36:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_opacity;
			return GF_OK;
		case 37:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->fill;
			return GF_OK;
		case 38:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->fill_rule;
			return GF_OK;
		case 39:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke;
			return GF_OK;
		case 40:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_dasharray;
			return GF_OK;
		case 41:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 42:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_linecap;
			return GF_OK;
		case 43:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_linejoin;
			return GF_OK;
		case 44:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 45:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stroke_width;
			return GF_OK;
		case 46:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->color;
			return GF_OK;
		case 47:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->color_rendering;
			return GF_OK;
		case 48:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->vector_effect;
			return GF_OK;
		case 49:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->viewport_fill;
			return GF_OK;
		case 50:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 51:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->solid_color;
			return GF_OK;
		case 52:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->solid_opacity;
			return GF_OK;
		case 53:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->display_align;
			return GF_OK;
		case 54:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->line_increment;
			return GF_OK;
		case 55:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stop_color;
			return GF_OK;
		case 56:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->stop_opacity;
			return GF_OK;
		case 57:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->font_family;
			return GF_OK;
		case 58:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->font_size;
			return GF_OK;
		case 59:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->font_style;
			return GF_OK;
		case 60:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->font_weight;
			return GF_OK;
		case 61:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_ellipse()
{
	SVGellipseElement *p;
	GF_SAFEALLOC(p, sizeof(SVGellipseElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_ellipse);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "ellipse";
	((GF_Node *p)->sgprivate->node_del = SVG_ellipse_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_ellipse_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_font_Del(GF_Node *node)
{
	SVGfontElement *p = (SVGfontElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_font_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 8:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->horiz_adv_x;
			return GF_OK;
		case 9:
			info->name = "horiz-origin-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->horiz_origin_x;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_font()
{
	SVGfontElement *p;
	GF_SAFEALLOC(p, sizeof(SVGfontElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_font);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font";
	((GF_Node *p)->sgprivate->node_del = SVG_font_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_font_get_attribute;
#endif
	return p;
}

static void SVG_font_face_Del(GF_Node *node)
{
	SVGfont_faceElement *p = (SVGfont_faceElement *)node;
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_font_face_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 8:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_family;
			return GF_OK;
		case 9:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_style;
			return GF_OK;
		case 10:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_weight;
			return GF_OK;
		case 11:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_size;
			return GF_OK;
		case 12:
			info->name = "font-variant";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_variant;
			return GF_OK;
		case 13:
			info->name = "font-stretch";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_stretch;
			return GF_OK;
		case 14:
			info->name = "unicode-range";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->unicode_range;
			return GF_OK;
		case 15:
			info->name = "panose-1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->panose_1;
			return GF_OK;
		case 16:
			info->name = "widths";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->widths;
			return GF_OK;
		case 17:
			info->name = "bbox";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->bbox;
			return GF_OK;
		case 18:
			info->name = "units-per-em";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->units_per_em;
			return GF_OK;
		case 19:
			info->name = "stemv";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->stemv;
			return GF_OK;
		case 20:
			info->name = "stemh";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->stemh;
			return GF_OK;
		case 21:
			info->name = "slope";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->slope;
			return GF_OK;
		case 22:
			info->name = "cap-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->cap_height;
			return GF_OK;
		case 23:
			info->name = "x-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->x_height;
			return GF_OK;
		case 24:
			info->name = "accent-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->accent_height;
			return GF_OK;
		case 25:
			info->name = "ascent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->ascent;
			return GF_OK;
		case 26:
			info->name = "descent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->descent;
			return GF_OK;
		case 27:
			info->name = "ideographic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->ideographic;
			return GF_OK;
		case 28:
			info->name = "alphabetic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->alphabetic;
			return GF_OK;
		case 29:
			info->name = "mathematical";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->mathematical;
			return GF_OK;
		case 30:
			info->name = "hanging";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->hanging;
			return GF_OK;
		case 31:
			info->name = "underline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->underline_position;
			return GF_OK;
		case 32:
			info->name = "underline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->underline_thickness;
			return GF_OK;
		case 33:
			info->name = "strikethrough-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->strikethrough_position;
			return GF_OK;
		case 34:
			info->name = "strikethrough-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->strikethrough_thickness;
			return GF_OK;
		case 35:
			info->name = "overline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->overline_position;
			return GF_OK;
		case 36:
			info->name = "overline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->overline_thickness;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_font_face()
{
	SVGfont_faceElement *p;
	GF_SAFEALLOC(p, sizeof(SVGfont_faceElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_font_face);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face";
	((GF_Node *p)->sgprivate->node_del = SVG_font_face_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_font_face_get_attribute;
#endif
	return p;
}

static void SVG_font_face_name_Del(GF_Node *node)
{
	SVGfont_face_nameElement *p = (SVGfont_face_nameElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_font_face_name_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->name;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_font_face_name()
{
	SVGfont_face_nameElement *p;
	GF_SAFEALLOC(p, sizeof(SVGfont_face_nameElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_font_face_name);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_name";
	((GF_Node *p)->sgprivate->node_del = SVG_font_face_name_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_font_face_name_get_attribute;
#endif
	return p;
}

static void SVG_font_face_src_Del(GF_Node *node)
{
	SVGfont_face_srcElement *p = (SVGfont_face_srcElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_font_face_src_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_srcElement *)node)->xml_space;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_font_face_src()
{
	SVGfont_face_srcElement *p;
	GF_SAFEALLOC(p, sizeof(SVGfont_face_srcElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_font_face_src);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_src";
	((GF_Node *p)->sgprivate->node_del = SVG_font_face_src_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_font_face_src_get_attribute;
#endif
	return p;
}

static void SVG_font_face_uri_Del(GF_Node *node)
{
	SVGfont_face_uriElement *p = (SVGfont_face_uriElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_font_face_uri_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_show;
			return GF_OK;
		case 8:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_actuate;
			return GF_OK;
		case 9:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_type;
			return GF_OK;
		case 10:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_role;
			return GF_OK;
		case 11:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_arcrole;
			return GF_OK;
		case 12:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_title;
			return GF_OK;
		case 13:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGfont_face_uriElement *)node)->xlink_href;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_font_face_uri()
{
	SVGfont_face_uriElement *p;
	GF_SAFEALLOC(p, sizeof(SVGfont_face_uriElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_font_face_uri);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "font_face_uri";
	((GF_Node *p)->sgprivate->node_del = SVG_font_face_uri_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_font_face_uri_get_attribute;
#endif
	return p;
}

static void SVG_foreignObject_Del(GF_Node *node)
{
	SVGforeignObjectElement *p = (SVGforeignObjectElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_foreignObject_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_actuate;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_type;
			return GF_OK;
		case 14:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_role;
			return GF_OK;
		case 15:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_arcrole;
			return GF_OK;
		case 16:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_title;
			return GF_OK;
		case 17:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_href;
			return GF_OK;
		case 18:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->xlink_show;
			return GF_OK;
		case 19:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusable;
			return GF_OK;
		case 20:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusNext;
			return GF_OK;
		case 21:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusPrev;
			return GF_OK;
		case 22:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusNorth;
			return GF_OK;
		case 23:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusNorthEast;
			return GF_OK;
		case 24:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusEast;
			return GF_OK;
		case 25:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusSouthEast;
			return GF_OK;
		case 26:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusSouth;
			return GF_OK;
		case 27:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusSouthWest;
			return GF_OK;
		case 28:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusWest;
			return GF_OK;
		case 29:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->focusNorthWest;
			return GF_OK;
		case 30:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->x;
			return GF_OK;
		case 33:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->y;
			return GF_OK;
		case 34:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->width;
			return GF_OK;
		case 35:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->height;
			return GF_OK;
		case 36:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->display;
			return GF_OK;
		case 37:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->visibility;
			return GF_OK;
		case 38:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->image_rendering;
			return GF_OK;
		case 39:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->pointer_events;
			return GF_OK;
		case 40:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->shape_rendering;
			return GF_OK;
		case 41:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->text_rendering;
			return GF_OK;
		case 42:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->audio_level;
			return GF_OK;
		case 43:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->fill_opacity;
			return GF_OK;
		case 44:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_opacity;
			return GF_OK;
		case 45:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->fill;
			return GF_OK;
		case 46:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->fill_rule;
			return GF_OK;
		case 47:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke;
			return GF_OK;
		case 48:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_dasharray;
			return GF_OK;
		case 49:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 50:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_linecap;
			return GF_OK;
		case 51:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_linejoin;
			return GF_OK;
		case 52:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 53:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stroke_width;
			return GF_OK;
		case 54:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->color;
			return GF_OK;
		case 55:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->color_rendering;
			return GF_OK;
		case 56:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->vector_effect;
			return GF_OK;
		case 57:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->viewport_fill;
			return GF_OK;
		case 58:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 59:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->solid_color;
			return GF_OK;
		case 60:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->solid_opacity;
			return GF_OK;
		case 61:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->display_align;
			return GF_OK;
		case 62:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->line_increment;
			return GF_OK;
		case 63:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stop_color;
			return GF_OK;
		case 64:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->stop_opacity;
			return GF_OK;
		case 65:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->font_family;
			return GF_OK;
		case 66:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->font_size;
			return GF_OK;
		case 67:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->font_style;
			return GF_OK;
		case 68:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->font_weight;
			return GF_OK;
		case 69:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_foreignObject()
{
	SVGforeignObjectElement *p;
	GF_SAFEALLOC(p, sizeof(SVGforeignObjectElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_foreignObject);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "foreignObject";
	((GF_Node *p)->sgprivate->node_del = SVG_foreignObject_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_foreignObject_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_g_Del(GF_Node *node)
{
	SVGgElement *p = (SVGgElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_g_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGgElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGgElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGgElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGgElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGgElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGgElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGgElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGgElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGgElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGgElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGgElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGgElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGgElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGgElement *)node)->display;
			return GF_OK;
		case 26:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGgElement *)node)->visibility;
			return GF_OK;
		case 27:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->image_rendering;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->shape_rendering;
			return GF_OK;
		case 30:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->text_rendering;
			return GF_OK;
		case 31:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGgElement *)node)->audio_level;
			return GF_OK;
		case 32:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGgElement *)node)->fill_opacity;
			return GF_OK;
		case 33:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_opacity;
			return GF_OK;
		case 34:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGgElement *)node)->fill;
			return GF_OK;
		case 35:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGgElement *)node)->fill_rule;
			return GF_OK;
		case 36:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke;
			return GF_OK;
		case 37:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_dasharray;
			return GF_OK;
		case 38:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 39:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_linecap;
			return GF_OK;
		case 40:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_linejoin;
			return GF_OK;
		case 41:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 42:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stroke_width;
			return GF_OK;
		case 43:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGgElement *)node)->color;
			return GF_OK;
		case 44:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->color_rendering;
			return GF_OK;
		case 45:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->vector_effect;
			return GF_OK;
		case 46:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGgElement *)node)->viewport_fill;
			return GF_OK;
		case 47:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGgElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 48:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGgElement *)node)->solid_color;
			return GF_OK;
		case 49:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGgElement *)node)->solid_opacity;
			return GF_OK;
		case 50:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGgElement *)node)->display_align;
			return GF_OK;
		case 51:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGgElement *)node)->line_increment;
			return GF_OK;
		case 52:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stop_color;
			return GF_OK;
		case 53:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGgElement *)node)->stop_opacity;
			return GF_OK;
		case 54:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGgElement *)node)->font_family;
			return GF_OK;
		case 55:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGgElement *)node)->font_size;
			return GF_OK;
		case 56:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGgElement *)node)->font_style;
			return GF_OK;
		case 57:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGgElement *)node)->font_weight;
			return GF_OK;
		case 58:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGgElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_g()
{
	SVGgElement *p;
	GF_SAFEALLOC(p, sizeof(SVGgElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_g);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "g";
	((GF_Node *p)->sgprivate->node_del = SVG_g_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_g_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_glyph_Del(GF_Node *node)
{
	SVGglyphElement *p = (SVGglyphElement *)node;
	free(p->textContent);
	SVG_DeletePath(&(p->d));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->d;
			return GF_OK;
		case 9:
			info->name = "unicode";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->unicode;
			return GF_OK;
		case 10:
			info->name = "glyph-name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->glyph_name;
			return GF_OK;
		case 11:
			info->name = "arabic-form";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->arabic_form;
			return GF_OK;
		case 12:
			info->name = "lang";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->lang;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_glyph()
{
	SVGglyphElement *p;
	GF_SAFEALLOC(p, sizeof(SVGglyphElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "glyph";
	((GF_Node *p)->sgprivate->node_del = SVG_glyph_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_glyph_get_attribute;
#endif
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	return p;
}

static void SVG_handler_Del(GF_Node *node)
{
	SVGhandlerElement *p = (SVGhandlerElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_handler_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 8:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->type;
			return GF_OK;
		case 9:
			info->name = "ev:event";
			info->fieldType = SVG_XSLT_QName_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->ev_event;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_handler()
{
	SVGhandlerElement *p;
	GF_SAFEALLOC(p, sizeof(SVGhandlerElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_handler);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "handler";
	((GF_Node *p)->sgprivate->node_del = SVG_handler_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_handler_get_attribute;
#endif
	return p;
}

static void SVG_hkern_Del(GF_Node *node)
{
	SVGhkernElement *p = (SVGhkernElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_hkern_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "u1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->u1;
			return GF_OK;
		case 8:
			info->name = "g1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->g1;
			return GF_OK;
		case 9:
			info->name = "u2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->u2;
			return GF_OK;
		case 10:
			info->name = "g2";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->g2;
			return GF_OK;
		case 11:
			info->name = "k";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGhkernElement *)node)->k;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_hkern()
{
	SVGhkernElement *p;
	GF_SAFEALLOC(p, sizeof(SVGhkernElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_hkern);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "hkern";
	((GF_Node *p)->sgprivate->node_del = SVG_hkern_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_hkern_get_attribute;
#endif
	return p;
}

static void SVG_image_Del(GF_Node *node)
{
	SVGimageElement *p = (SVGimageElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_image_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_actuate;
			return GF_OK;
		case 8:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_type;
			return GF_OK;
		case 9:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_role;
			return GF_OK;
		case 10:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_arcrole;
			return GF_OK;
		case 11:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_title;
			return GF_OK;
		case 12:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_href;
			return GF_OK;
		case 13:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->xlink_show;
			return GF_OK;
		case 14:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->requiredFeatures;
			return GF_OK;
		case 15:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->requiredExtensions;
			return GF_OK;
		case 16:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->requiredFormats;
			return GF_OK;
		case 17:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->requiredFonts;
			return GF_OK;
		case 18:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->systemLanguage;
			return GF_OK;
		case 19:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 20:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusable;
			return GF_OK;
		case 21:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusNext;
			return GF_OK;
		case 22:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusPrev;
			return GF_OK;
		case 23:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusNorth;
			return GF_OK;
		case 24:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusNorthEast;
			return GF_OK;
		case 25:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusEast;
			return GF_OK;
		case 26:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusSouthEast;
			return GF_OK;
		case 27:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusSouth;
			return GF_OK;
		case 28:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusSouthWest;
			return GF_OK;
		case 29:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusWest;
			return GF_OK;
		case 30:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->focusNorthWest;
			return GF_OK;
		case 31:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->transform;
			return GF_OK;
		case 32:
			info->name = "opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->opacity;
			return GF_OK;
		case 33:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->x;
			return GF_OK;
		case 34:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->y;
			return GF_OK;
		case 35:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->width;
			return GF_OK;
		case 36:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->height;
			return GF_OK;
		case 37:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 38:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->type;
			return GF_OK;
		case 39:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->display;
			return GF_OK;
		case 40:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->visibility;
			return GF_OK;
		case 41:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->image_rendering;
			return GF_OK;
		case 42:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->pointer_events;
			return GF_OK;
		case 43:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->shape_rendering;
			return GF_OK;
		case 44:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->text_rendering;
			return GF_OK;
		case 45:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->audio_level;
			return GF_OK;
		case 46:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->fill_opacity;
			return GF_OK;
		case 47:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_opacity;
			return GF_OK;
		case 48:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->fill;
			return GF_OK;
		case 49:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->fill_rule;
			return GF_OK;
		case 50:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke;
			return GF_OK;
		case 51:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_dasharray;
			return GF_OK;
		case 52:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 53:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_linecap;
			return GF_OK;
		case 54:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_linejoin;
			return GF_OK;
		case 55:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 56:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stroke_width;
			return GF_OK;
		case 57:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->color;
			return GF_OK;
		case 58:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->color_rendering;
			return GF_OK;
		case 59:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->vector_effect;
			return GF_OK;
		case 60:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->viewport_fill;
			return GF_OK;
		case 61:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 62:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->solid_color;
			return GF_OK;
		case 63:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->solid_opacity;
			return GF_OK;
		case 64:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->display_align;
			return GF_OK;
		case 65:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->line_increment;
			return GF_OK;
		case 66:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stop_color;
			return GF_OK;
		case 67:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->stop_opacity;
			return GF_OK;
		case 68:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->font_family;
			return GF_OK;
		case 69:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->font_size;
			return GF_OK;
		case 70:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->font_style;
			return GF_OK;
		case 71:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->font_weight;
			return GF_OK;
		case 72:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_image()
{
	SVGimageElement *p;
	GF_SAFEALLOC(p, sizeof(SVGimageElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_image);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "image";
	((GF_Node *p)->sgprivate->node_del = SVG_image_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_image_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_line_Del(GF_Node *node)
{
	SVGlineElement *p = (SVGlineElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_line_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->x1;
			return GF_OK;
		case 25:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->y1;
			return GF_OK;
		case 26:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->x2;
			return GF_OK;
		case 27:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->y2;
			return GF_OK;
		case 28:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->display;
			return GF_OK;
		case 29:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->visibility;
			return GF_OK;
		case 30:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->image_rendering;
			return GF_OK;
		case 31:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->pointer_events;
			return GF_OK;
		case 32:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->shape_rendering;
			return GF_OK;
		case 33:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->text_rendering;
			return GF_OK;
		case 34:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->audio_level;
			return GF_OK;
		case 35:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->fill_opacity;
			return GF_OK;
		case 36:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_opacity;
			return GF_OK;
		case 37:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->fill;
			return GF_OK;
		case 38:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->fill_rule;
			return GF_OK;
		case 39:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke;
			return GF_OK;
		case 40:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_dasharray;
			return GF_OK;
		case 41:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 42:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_linecap;
			return GF_OK;
		case 43:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_linejoin;
			return GF_OK;
		case 44:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 45:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stroke_width;
			return GF_OK;
		case 46:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->color;
			return GF_OK;
		case 47:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->color_rendering;
			return GF_OK;
		case 48:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->vector_effect;
			return GF_OK;
		case 49:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->viewport_fill;
			return GF_OK;
		case 50:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 51:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->solid_color;
			return GF_OK;
		case 52:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->solid_opacity;
			return GF_OK;
		case 53:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->display_align;
			return GF_OK;
		case 54:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->line_increment;
			return GF_OK;
		case 55:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stop_color;
			return GF_OK;
		case 56:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->stop_opacity;
			return GF_OK;
		case 57:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->font_family;
			return GF_OK;
		case 58:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->font_size;
			return GF_OK;
		case 59:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->font_style;
			return GF_OK;
		case 60:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->font_weight;
			return GF_OK;
		case 61:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_line()
{
	SVGlineElement *p;
	GF_SAFEALLOC(p, sizeof(SVGlineElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_line);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "line";
	((GF_Node *p)->sgprivate->node_del = SVG_line_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_line_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_linearGradient_Del(GF_Node *node)
{
	SVGlinearGradientElement *p = (SVGlinearGradientElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_linearGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->x1;
			return GF_OK;
		case 8:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->y1;
			return GF_OK;
		case 9:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->x2;
			return GF_OK;
		case 10:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->y2;
			return GF_OK;
		case 11:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->display;
			return GF_OK;
		case 12:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->visibility;
			return GF_OK;
		case 13:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->image_rendering;
			return GF_OK;
		case 14:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->pointer_events;
			return GF_OK;
		case 15:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->shape_rendering;
			return GF_OK;
		case 16:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->text_rendering;
			return GF_OK;
		case 17:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->audio_level;
			return GF_OK;
		case 18:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->fill_opacity;
			return GF_OK;
		case 19:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_opacity;
			return GF_OK;
		case 20:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->fill;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->fill_rule;
			return GF_OK;
		case 22:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke;
			return GF_OK;
		case 23:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_dasharray;
			return GF_OK;
		case 24:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 25:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_linecap;
			return GF_OK;
		case 26:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_linejoin;
			return GF_OK;
		case 27:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 28:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stroke_width;
			return GF_OK;
		case 29:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->color;
			return GF_OK;
		case 30:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->color_rendering;
			return GF_OK;
		case 31:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->vector_effect;
			return GF_OK;
		case 32:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->viewport_fill;
			return GF_OK;
		case 33:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 34:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->solid_color;
			return GF_OK;
		case 35:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->solid_opacity;
			return GF_OK;
		case 36:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->display_align;
			return GF_OK;
		case 37:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->line_increment;
			return GF_OK;
		case 38:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stop_color;
			return GF_OK;
		case 39:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->stop_opacity;
			return GF_OK;
		case 40:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->font_family;
			return GF_OK;
		case 41:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->font_size;
			return GF_OK;
		case 42:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->font_style;
			return GF_OK;
		case 43:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->font_weight;
			return GF_OK;
		case 44:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_linearGradient()
{
	SVGlinearGradientElement *p;
	GF_SAFEALLOC(p, sizeof(SVGlinearGradientElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_linearGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "linearGradient";
	((GF_Node *p)->sgprivate->node_del = SVG_linearGradient_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_linearGradient_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_listener_Del(GF_Node *node)
{
	SVGlistenerElement *p = (SVGlistenerElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_listener_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "event";
			info->fieldType = SVG_XSLT_QName_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->event;
			return GF_OK;
		case 8:
			info->name = "phase";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->phase;
			return GF_OK;
		case 9:
			info->name = "propagate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->propagate;
			return GF_OK;
		case 10:
			info->name = "defaultAction";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->defaultAction;
			return GF_OK;
		case 11:
			info->name = "observer";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->observer;
			return GF_OK;
		case 12:
			info->name = "target";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->target;
			return GF_OK;
		case 13:
			info->name = "handler";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->handler;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_listener()
{
	SVGlistenerElement *p;
	GF_SAFEALLOC(p, sizeof(SVGlistenerElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_listener);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "listener";
	((GF_Node *p)->sgprivate->node_del = SVG_listener_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_listener_get_attribute;
#endif
	return p;
}

static void SVG_metadata_Del(GF_Node *node)
{
	SVGmetadataElement *p = (SVGmetadataElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_metadata_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmetadataElement *)node)->xml_space;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_metadata()
{
	SVGmetadataElement *p;
	GF_SAFEALLOC(p, sizeof(SVGmetadataElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_metadata);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "metadata";
	((GF_Node *p)->sgprivate->node_del = SVG_metadata_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_metadata_get_attribute;
#endif
	return p;
}

static void SVG_missing_glyph_Del(GF_Node *node)
{
	SVGmissing_glyphElement *p = (SVGmissing_glyphElement *)node;
	free(p->textContent);
	SVG_DeletePath(&(p->d));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_missing_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 8:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGmissing_glyphElement *)node)->d;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_missing_glyph()
{
	SVGmissing_glyphElement *p;
	GF_SAFEALLOC(p, sizeof(SVGmissing_glyphElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_missing_glyph);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "missing_glyph";
	((GF_Node *p)->sgprivate->node_del = SVG_missing_glyph_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_missing_glyph_get_attribute;
#endif
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	return p;
}

static void SVG_mpath_Del(GF_Node *node)
{
	SVGmpathElement *p = (SVGmpathElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_mpath_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_show;
			return GF_OK;
		case 8:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_actuate;
			return GF_OK;
		case 9:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_type;
			return GF_OK;
		case 10:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_role;
			return GF_OK;
		case 11:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_arcrole;
			return GF_OK;
		case 12:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_title;
			return GF_OK;
		case 13:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGmpathElement *)node)->xlink_href;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_mpath()
{
	SVGmpathElement *p;
	GF_SAFEALLOC(p, sizeof(SVGmpathElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_mpath);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "mpath";
	((GF_Node *p)->sgprivate->node_del = SVG_mpath_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_mpath_get_attribute;
#endif
	return p;
}

static void SVG_path_Del(GF_Node *node)
{
	SVGpathElement *p = (SVGpathElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_DeletePath(&(p->d));
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_path_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->d;
			return GF_OK;
		case 25:
			info->name = "pathLength";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->pathLength;
			return GF_OK;
		case 26:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->display;
			return GF_OK;
		case 27:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->visibility;
			return GF_OK;
		case 28:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->image_rendering;
			return GF_OK;
		case 29:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->pointer_events;
			return GF_OK;
		case 30:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->shape_rendering;
			return GF_OK;
		case 31:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->text_rendering;
			return GF_OK;
		case 32:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->audio_level;
			return GF_OK;
		case 33:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->fill_opacity;
			return GF_OK;
		case 34:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_opacity;
			return GF_OK;
		case 35:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->fill;
			return GF_OK;
		case 36:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->fill_rule;
			return GF_OK;
		case 37:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke;
			return GF_OK;
		case 38:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_dasharray;
			return GF_OK;
		case 39:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 40:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_linecap;
			return GF_OK;
		case 41:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_linejoin;
			return GF_OK;
		case 42:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 43:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stroke_width;
			return GF_OK;
		case 44:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->color;
			return GF_OK;
		case 45:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->color_rendering;
			return GF_OK;
		case 46:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->vector_effect;
			return GF_OK;
		case 47:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->viewport_fill;
			return GF_OK;
		case 48:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 49:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->solid_color;
			return GF_OK;
		case 50:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->solid_opacity;
			return GF_OK;
		case 51:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->display_align;
			return GF_OK;
		case 52:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->line_increment;
			return GF_OK;
		case 53:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stop_color;
			return GF_OK;
		case 54:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->stop_opacity;
			return GF_OK;
		case 55:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->font_family;
			return GF_OK;
		case 56:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->font_size;
			return GF_OK;
		case 57:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->font_style;
			return GF_OK;
		case 58:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->font_weight;
			return GF_OK;
		case 59:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_path()
{
	SVGpathElement *p;
	GF_SAFEALLOC(p, sizeof(SVGpathElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_path);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "path";
	((GF_Node *p)->sgprivate->node_del = SVG_path_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_path_get_attribute;
#endif
	p->transform = gf_list_new();
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_polygon_Del(GF_Node *node)
{
	SVGpolygonElement *p = (SVGpolygonElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_DeletePoints(p->points);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_polygon_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->points;
			return GF_OK;
		case 25:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->display;
			return GF_OK;
		case 26:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->visibility;
			return GF_OK;
		case 27:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->image_rendering;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->shape_rendering;
			return GF_OK;
		case 30:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->text_rendering;
			return GF_OK;
		case 31:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->audio_level;
			return GF_OK;
		case 32:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->fill_opacity;
			return GF_OK;
		case 33:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_opacity;
			return GF_OK;
		case 34:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->fill;
			return GF_OK;
		case 35:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->fill_rule;
			return GF_OK;
		case 36:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke;
			return GF_OK;
		case 37:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_dasharray;
			return GF_OK;
		case 38:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 39:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_linecap;
			return GF_OK;
		case 40:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_linejoin;
			return GF_OK;
		case 41:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 42:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stroke_width;
			return GF_OK;
		case 43:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->color;
			return GF_OK;
		case 44:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->color_rendering;
			return GF_OK;
		case 45:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->vector_effect;
			return GF_OK;
		case 46:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->viewport_fill;
			return GF_OK;
		case 47:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 48:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->solid_color;
			return GF_OK;
		case 49:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->solid_opacity;
			return GF_OK;
		case 50:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->display_align;
			return GF_OK;
		case 51:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->line_increment;
			return GF_OK;
		case 52:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stop_color;
			return GF_OK;
		case 53:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->stop_opacity;
			return GF_OK;
		case 54:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->font_family;
			return GF_OK;
		case 55:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->font_size;
			return GF_OK;
		case 56:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->font_style;
			return GF_OK;
		case 57:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->font_weight;
			return GF_OK;
		case 58:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_polygon()
{
	SVGpolygonElement *p;
	GF_SAFEALLOC(p, sizeof(SVGpolygonElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_polygon);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polygon";
	((GF_Node *p)->sgprivate->node_del = SVG_polygon_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_polygon_get_attribute;
#endif
	p->transform = gf_list_new();
	p->points = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_polyline_Del(GF_Node *node)
{
	SVGpolylineElement *p = (SVGpolylineElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_DeletePoints(p->points);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_polyline_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->points;
			return GF_OK;
		case 25:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->display;
			return GF_OK;
		case 26:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->visibility;
			return GF_OK;
		case 27:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->image_rendering;
			return GF_OK;
		case 28:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->pointer_events;
			return GF_OK;
		case 29:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->shape_rendering;
			return GF_OK;
		case 30:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->text_rendering;
			return GF_OK;
		case 31:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->audio_level;
			return GF_OK;
		case 32:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->fill_opacity;
			return GF_OK;
		case 33:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_opacity;
			return GF_OK;
		case 34:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->fill;
			return GF_OK;
		case 35:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->fill_rule;
			return GF_OK;
		case 36:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke;
			return GF_OK;
		case 37:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_dasharray;
			return GF_OK;
		case 38:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 39:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_linecap;
			return GF_OK;
		case 40:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_linejoin;
			return GF_OK;
		case 41:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 42:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stroke_width;
			return GF_OK;
		case 43:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->color;
			return GF_OK;
		case 44:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->color_rendering;
			return GF_OK;
		case 45:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->vector_effect;
			return GF_OK;
		case 46:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->viewport_fill;
			return GF_OK;
		case 47:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 48:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->solid_color;
			return GF_OK;
		case 49:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->solid_opacity;
			return GF_OK;
		case 50:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->display_align;
			return GF_OK;
		case 51:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->line_increment;
			return GF_OK;
		case 52:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stop_color;
			return GF_OK;
		case 53:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->stop_opacity;
			return GF_OK;
		case 54:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->font_family;
			return GF_OK;
		case 55:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->font_size;
			return GF_OK;
		case 56:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->font_style;
			return GF_OK;
		case 57:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->font_weight;
			return GF_OK;
		case 58:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_polyline()
{
	SVGpolylineElement *p;
	GF_SAFEALLOC(p, sizeof(SVGpolylineElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_polyline);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "polyline";
	((GF_Node *p)->sgprivate->node_del = SVG_polyline_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_polyline_get_attribute;
#endif
	p->transform = gf_list_new();
	p->points = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_prefetch_Del(GF_Node *node)
{
	SVGprefetchElement *p = (SVGprefetchElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_prefetch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_show;
			return GF_OK;
		case 8:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_actuate;
			return GF_OK;
		case 9:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_type;
			return GF_OK;
		case 10:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_role;
			return GF_OK;
		case 11:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_arcrole;
			return GF_OK;
		case 12:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_title;
			return GF_OK;
		case 13:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->xlink_href;
			return GF_OK;
		case 14:
			info->name = "mediaSize";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->mediaSize;
			return GF_OK;
		case 15:
			info->name = "mediaTime";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->mediaTime;
			return GF_OK;
		case 16:
			info->name = "mediaCharacterEncoding";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->mediaCharacterEncoding;
			return GF_OK;
		case 17:
			info->name = "mediaContentEncodings";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->mediaContentEncodings;
			return GF_OK;
		case 18:
			info->name = "bandwidth";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGprefetchElement *)node)->bandwidth;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_prefetch()
{
	SVGprefetchElement *p;
	GF_SAFEALLOC(p, sizeof(SVGprefetchElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_prefetch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "prefetch";
	((GF_Node *p)->sgprivate->node_del = SVG_prefetch_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_prefetch_get_attribute;
#endif
	return p;
}

static void SVG_radialGradient_Del(GF_Node *node)
{
	SVGradialGradientElement *p = (SVGradialGradientElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_radialGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->cx;
			return GF_OK;
		case 8:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->cy;
			return GF_OK;
		case 9:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->r;
			return GF_OK;
		case 10:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->display;
			return GF_OK;
		case 11:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->visibility;
			return GF_OK;
		case 12:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->image_rendering;
			return GF_OK;
		case 13:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->pointer_events;
			return GF_OK;
		case 14:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->shape_rendering;
			return GF_OK;
		case 15:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->text_rendering;
			return GF_OK;
		case 16:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->audio_level;
			return GF_OK;
		case 17:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->fill_opacity;
			return GF_OK;
		case 18:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_opacity;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->fill;
			return GF_OK;
		case 20:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->fill_rule;
			return GF_OK;
		case 21:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke;
			return GF_OK;
		case 22:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_dasharray;
			return GF_OK;
		case 23:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 24:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_linecap;
			return GF_OK;
		case 25:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_linejoin;
			return GF_OK;
		case 26:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 27:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stroke_width;
			return GF_OK;
		case 28:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->color;
			return GF_OK;
		case 29:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->color_rendering;
			return GF_OK;
		case 30:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->vector_effect;
			return GF_OK;
		case 31:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->viewport_fill;
			return GF_OK;
		case 32:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 33:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->solid_color;
			return GF_OK;
		case 34:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->solid_opacity;
			return GF_OK;
		case 35:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->display_align;
			return GF_OK;
		case 36:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->line_increment;
			return GF_OK;
		case 37:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stop_color;
			return GF_OK;
		case 38:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->stop_opacity;
			return GF_OK;
		case 39:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->font_family;
			return GF_OK;
		case 40:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->font_size;
			return GF_OK;
		case 41:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->font_style;
			return GF_OK;
		case 42:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->font_weight;
			return GF_OK;
		case 43:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_radialGradient()
{
	SVGradialGradientElement *p;
	GF_SAFEALLOC(p, sizeof(SVGradialGradientElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_radialGradient);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "radialGradient";
	((GF_Node *p)->sgprivate->node_del = SVG_radialGradient_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_radialGradient_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_rect_Del(GF_Node *node)
{
	SVGrectElement *p = (SVGrectElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_rect_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->transform;
			return GF_OK;
		case 8:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->requiredFeatures;
			return GF_OK;
		case 9:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->requiredExtensions;
			return GF_OK;
		case 10:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->requiredFormats;
			return GF_OK;
		case 11:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->requiredFonts;
			return GF_OK;
		case 12:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->systemLanguage;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->x;
			return GF_OK;
		case 25:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->y;
			return GF_OK;
		case 26:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->width;
			return GF_OK;
		case 27:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->height;
			return GF_OK;
		case 28:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->rx;
			return GF_OK;
		case 29:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->ry;
			return GF_OK;
		case 30:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->display;
			return GF_OK;
		case 31:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->visibility;
			return GF_OK;
		case 32:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->image_rendering;
			return GF_OK;
		case 33:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->pointer_events;
			return GF_OK;
		case 34:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->shape_rendering;
			return GF_OK;
		case 35:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->text_rendering;
			return GF_OK;
		case 36:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->audio_level;
			return GF_OK;
		case 37:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->fill_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_opacity;
			return GF_OK;
		case 39:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->fill;
			return GF_OK;
		case 40:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->fill_rule;
			return GF_OK;
		case 41:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke;
			return GF_OK;
		case 42:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_dasharray;
			return GF_OK;
		case 43:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 44:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_linecap;
			return GF_OK;
		case 45:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_linejoin;
			return GF_OK;
		case 46:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 47:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stroke_width;
			return GF_OK;
		case 48:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->color;
			return GF_OK;
		case 49:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->color_rendering;
			return GF_OK;
		case 50:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->vector_effect;
			return GF_OK;
		case 51:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->viewport_fill;
			return GF_OK;
		case 52:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 53:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->solid_color;
			return GF_OK;
		case 54:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->solid_opacity;
			return GF_OK;
		case 55:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->display_align;
			return GF_OK;
		case 56:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->line_increment;
			return GF_OK;
		case 57:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stop_color;
			return GF_OK;
		case 58:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->stop_opacity;
			return GF_OK;
		case 59:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->font_family;
			return GF_OK;
		case 60:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->font_size;
			return GF_OK;
		case 61:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->font_style;
			return GF_OK;
		case 62:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->font_weight;
			return GF_OK;
		case 63:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_rect()
{
	SVGrectElement *p;
	GF_SAFEALLOC(p, sizeof(SVGrectElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_rect);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "rect";
	((GF_Node *p)->sgprivate->node_del = SVG_rect_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_rect_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_script_Del(GF_Node *node)
{
	SVGscriptElement *p = (SVGscriptElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_script_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 8:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_script()
{
	SVGscriptElement *p;
	GF_SAFEALLOC(p, sizeof(SVGscriptElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_script);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "script";
	((GF_Node *p)->sgprivate->node_del = SVG_script_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_script_get_attribute;
#endif
	return p;
}

static void SVG_set_Del(GF_Node *node)
{
	SVGsetElement *p = (SVGsetElement *)node;
	free(p->textContent);
	SVG_ResetIRI(&p->xlink_href);
	SMIL_DeleteTimes(p->begin);
	SMIL_DeleteTimes(p->end);
	SMIL_DeleteAnimateValue(&(p->to));
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_set_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_show;
			return GF_OK;
		case 9:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_actuate;
			return GF_OK;
		case 10:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_type;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->xlink_title;
			return GF_OK;
		case 14:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->attributeName;
			return GF_OK;
		case 15:
			info->name = "attributeType";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->attributeType;
			return GF_OK;
		case 16:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->begin;
			return GF_OK;
		case 17:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->dur;
			return GF_OK;
		case 18:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->end;
			return GF_OK;
		case 19:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->repeatCount;
			return GF_OK;
		case 20:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->repeatDur;
			return GF_OK;
		case 21:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->restart;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->fill;
			return GF_OK;
		case 23:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->min;
			return GF_OK;
		case 24:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->max;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = & ((SVGsetElement *)node)->to;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_set()
{
	SVGsetElement *p;
	GF_SAFEALLOC(p, sizeof(SVGsetElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_set);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "set";
	((GF_Node *p)->sgprivate->node_del = SVG_set_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_set_get_attribute;
#endif
	p->begin = gf_list_new();
	p->end = gf_list_new();
	p->repeatDur.type = SMIL_DURATION_UNSPECIFIED;
	p->min.type = SMIL_DURATION_DEFINED;
	return p;
}

static void SVG_solidColor_Del(GF_Node *node)
{
	SVGsolidColorElement *p = (SVGsolidColorElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_solidColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->display;
			return GF_OK;
		case 8:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->visibility;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->text_rendering;
			return GF_OK;
		case 13:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->audio_level;
			return GF_OK;
		case 14:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->fill_opacity;
			return GF_OK;
		case 15:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_opacity;
			return GF_OK;
		case 16:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->fill;
			return GF_OK;
		case 17:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->fill_rule;
			return GF_OK;
		case 18:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke;
			return GF_OK;
		case 19:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_dasharray;
			return GF_OK;
		case 20:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 21:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_linecap;
			return GF_OK;
		case 22:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_linejoin;
			return GF_OK;
		case 23:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 24:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stroke_width;
			return GF_OK;
		case 25:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->color;
			return GF_OK;
		case 26:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->color_rendering;
			return GF_OK;
		case 27:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->vector_effect;
			return GF_OK;
		case 28:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->viewport_fill;
			return GF_OK;
		case 29:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 30:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->solid_color;
			return GF_OK;
		case 31:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->solid_opacity;
			return GF_OK;
		case 32:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->display_align;
			return GF_OK;
		case 33:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->line_increment;
			return GF_OK;
		case 34:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stop_color;
			return GF_OK;
		case 35:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->stop_opacity;
			return GF_OK;
		case 36:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->font_family;
			return GF_OK;
		case 37:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->font_size;
			return GF_OK;
		case 38:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->font_style;
			return GF_OK;
		case 39:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->font_weight;
			return GF_OK;
		case 40:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGsolidColorElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_solidColor()
{
	SVGsolidColorElement *p;
	GF_SAFEALLOC(p, sizeof(SVGsolidColorElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_solidColor);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "solidColor";
	((GF_Node *p)->sgprivate->node_del = SVG_solidColor_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_solidColor_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_stop_Del(GF_Node *node)
{
	SVGstopElement *p = (SVGstopElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_stop_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "offset";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->offset;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->display;
			return GF_OK;
		case 9:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->visibility;
			return GF_OK;
		case 10:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->image_rendering;
			return GF_OK;
		case 11:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->pointer_events;
			return GF_OK;
		case 12:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->shape_rendering;
			return GF_OK;
		case 13:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->text_rendering;
			return GF_OK;
		case 14:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->audio_level;
			return GF_OK;
		case 15:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->fill_opacity;
			return GF_OK;
		case 16:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_opacity;
			return GF_OK;
		case 17:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->fill;
			return GF_OK;
		case 18:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->fill_rule;
			return GF_OK;
		case 19:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke;
			return GF_OK;
		case 20:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_dasharray;
			return GF_OK;
		case 21:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 22:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_linecap;
			return GF_OK;
		case 23:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_linejoin;
			return GF_OK;
		case 24:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 25:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stroke_width;
			return GF_OK;
		case 26:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->color;
			return GF_OK;
		case 27:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->color_rendering;
			return GF_OK;
		case 28:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->vector_effect;
			return GF_OK;
		case 29:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->viewport_fill;
			return GF_OK;
		case 30:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 31:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->solid_color;
			return GF_OK;
		case 32:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->solid_opacity;
			return GF_OK;
		case 33:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->display_align;
			return GF_OK;
		case 34:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->line_increment;
			return GF_OK;
		case 35:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stop_color;
			return GF_OK;
		case 36:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->stop_opacity;
			return GF_OK;
		case 37:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->font_family;
			return GF_OK;
		case 38:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->font_size;
			return GF_OK;
		case 39:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->font_style;
			return GF_OK;
		case 40:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->font_weight;
			return GF_OK;
		case 41:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_stop()
{
	SVGstopElement *p;
	GF_SAFEALLOC(p, sizeof(SVGstopElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_stop);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "stop";
	((GF_Node *p)->sgprivate->node_del = SVG_stop_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_stop_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_svg_Del(GF_Node *node)
{
	SVGsvgElement *p = (SVGsvgElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_svg_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 1:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->textContent;
			return GF_OK;
		case 2:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusable;
			return GF_OK;
		case 3:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusNext;
			return GF_OK;
		case 4:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusPrev;
			return GF_OK;
		case 5:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusNorth;
			return GF_OK;
		case 6:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusNorthEast;
			return GF_OK;
		case 7:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusEast;
			return GF_OK;
		case 8:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusSouthEast;
			return GF_OK;
		case 9:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusSouth;
			return GF_OK;
		case 10:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusSouthWest;
			return GF_OK;
		case 11:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusWest;
			return GF_OK;
		case 12:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->focusNorthWest;
			return GF_OK;
		case 13:
			info->name = "syncBehavior";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->syncBehavior;
			return GF_OK;
		case 14:
			info->name = "syncBehaviorDefault";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->syncBehaviorDefault;
			return GF_OK;
		case 15:
			info->name = "syncTolerance";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->syncTolerance;
			return GF_OK;
		case 16:
			info->name = "syncToleranceDefault";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->syncToleranceDefault;
			return GF_OK;
		case 17:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->id;
			return GF_OK;
		case 18:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->class_attribute;
			return GF_OK;
		case 19:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->xml_id;
			return GF_OK;
		case 20:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->xml_base;
			return GF_OK;
		case 21:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->xml_lang;
			return GF_OK;
		case 22:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->xml_space;
			return GF_OK;
		case 23:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->width;
			return GF_OK;
		case 24:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->height;
			return GF_OK;
		case 25:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 26:
			info->name = "viewBox";
			info->fieldType = SVG_ViewBox_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->viewBox;
			return GF_OK;
		case 27:
			info->name = "zoomAndPan";
			info->fieldType = SVG_ZoomAndPan_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->zoomAndPan;
			return GF_OK;
		case 28:
			info->name = "version";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->version;
			return GF_OK;
		case 29:
			info->name = "baseProfile";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->baseProfile;
			return GF_OK;
		case 30:
			info->name = "contentScriptType";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->contentScriptType;
			return GF_OK;
		case 31:
			info->name = "snapshotTime";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->snapshotTime;
			return GF_OK;
		case 32:
			info->name = "timelineBegin";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->timelineBegin;
			return GF_OK;
		case 33:
			info->name = "playbackOrder";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->playbackOrder;
			return GF_OK;
		case 34:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->display;
			return GF_OK;
		case 35:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->visibility;
			return GF_OK;
		case 36:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->image_rendering;
			return GF_OK;
		case 37:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->pointer_events;
			return GF_OK;
		case 38:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->shape_rendering;
			return GF_OK;
		case 39:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->text_rendering;
			return GF_OK;
		case 40:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->audio_level;
			return GF_OK;
		case 41:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->fill_opacity;
			return GF_OK;
		case 42:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_opacity;
			return GF_OK;
		case 43:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->fill;
			return GF_OK;
		case 44:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->fill_rule;
			return GF_OK;
		case 45:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke;
			return GF_OK;
		case 46:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_dasharray;
			return GF_OK;
		case 47:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 48:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_linecap;
			return GF_OK;
		case 49:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_linejoin;
			return GF_OK;
		case 50:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 51:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stroke_width;
			return GF_OK;
		case 52:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->color;
			return GF_OK;
		case 53:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->color_rendering;
			return GF_OK;
		case 54:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->vector_effect;
			return GF_OK;
		case 55:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->viewport_fill;
			return GF_OK;
		case 56:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 57:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->solid_color;
			return GF_OK;
		case 58:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->solid_opacity;
			return GF_OK;
		case 59:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->display_align;
			return GF_OK;
		case 60:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->line_increment;
			return GF_OK;
		case 61:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stop_color;
			return GF_OK;
		case 62:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->stop_opacity;
			return GF_OK;
		case 63:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->font_family;
			return GF_OK;
		case 64:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->font_size;
			return GF_OK;
		case 65:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->font_style;
			return GF_OK;
		case 66:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->font_weight;
			return GF_OK;
		case 67:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_svg()
{
	SVGsvgElement *p;
	GF_SAFEALLOC(p, sizeof(SVGsvgElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_svg);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "svg";
	((GF_Node *p)->sgprivate->node_del = SVG_svg_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_svg_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_switch_Del(GF_Node *node)
{
	SVGswitchElement *p = (SVGswitchElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_switch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 13:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->transform;
			return GF_OK;
		case 14:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->display;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->visibility;
			return GF_OK;
		case 16:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->image_rendering;
			return GF_OK;
		case 17:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->pointer_events;
			return GF_OK;
		case 18:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->shape_rendering;
			return GF_OK;
		case 19:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->text_rendering;
			return GF_OK;
		case 20:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->audio_level;
			return GF_OK;
		case 21:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->fill_opacity;
			return GF_OK;
		case 22:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_opacity;
			return GF_OK;
		case 23:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->fill;
			return GF_OK;
		case 24:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->fill_rule;
			return GF_OK;
		case 25:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke;
			return GF_OK;
		case 26:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_dasharray;
			return GF_OK;
		case 27:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 28:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_linecap;
			return GF_OK;
		case 29:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_linejoin;
			return GF_OK;
		case 30:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 31:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stroke_width;
			return GF_OK;
		case 32:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->color;
			return GF_OK;
		case 33:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->color_rendering;
			return GF_OK;
		case 34:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->vector_effect;
			return GF_OK;
		case 35:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->viewport_fill;
			return GF_OK;
		case 36:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 37:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->solid_color;
			return GF_OK;
		case 38:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->solid_opacity;
			return GF_OK;
		case 39:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->display_align;
			return GF_OK;
		case 40:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->line_increment;
			return GF_OK;
		case 41:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stop_color;
			return GF_OK;
		case 42:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->stop_opacity;
			return GF_OK;
		case 43:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->font_family;
			return GF_OK;
		case 44:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->font_size;
			return GF_OK;
		case 45:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->font_style;
			return GF_OK;
		case 46:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->font_weight;
			return GF_OK;
		case 47:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGswitchElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_switch()
{
	SVGswitchElement *p;
	GF_SAFEALLOC(p, sizeof(SVGswitchElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_switch);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "switch";
	((GF_Node *p)->sgprivate->node_del = SVG_switch_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_switch_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_tbreak_Del(GF_Node *node)
{
	SVGtbreakElement *p = (SVGtbreakElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_tbreak_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtbreakElement *)node)->xml_space;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_tbreak()
{
	SVGtbreakElement *p;
	GF_SAFEALLOC(p, sizeof(SVGtbreakElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_tbreak);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tbreak";
	((GF_Node *p)->sgprivate->node_del = SVG_tbreak_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_tbreak_get_attribute;
#endif
	return p;
}

static void SVG_text_Del(GF_Node *node)
{
	SVGtextElement *p = (SVGtextElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_DeleteCoordinates(p->x);
	SVG_DeleteCoordinates(p->y);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_text_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->editable;
			return GF_OK;
		case 13:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusable;
			return GF_OK;
		case 14:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusNext;
			return GF_OK;
		case 15:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusPrev;
			return GF_OK;
		case 16:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusNorth;
			return GF_OK;
		case 17:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusNorthEast;
			return GF_OK;
		case 18:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusEast;
			return GF_OK;
		case 19:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusSouthEast;
			return GF_OK;
		case 20:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusSouth;
			return GF_OK;
		case 21:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusSouthWest;
			return GF_OK;
		case 22:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusWest;
			return GF_OK;
		case 23:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->focusNorthWest;
			return GF_OK;
		case 24:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->transform;
			return GF_OK;
		case 25:
			info->name = "x";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->x;
			return GF_OK;
		case 26:
			info->name = "y";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->y;
			return GF_OK;
		case 27:
			info->name = "rotate";
			info->fieldType = SVG_Numbers_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->rotate;
			return GF_OK;
		case 28:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->display;
			return GF_OK;
		case 29:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->visibility;
			return GF_OK;
		case 30:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->image_rendering;
			return GF_OK;
		case 31:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->pointer_events;
			return GF_OK;
		case 32:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->shape_rendering;
			return GF_OK;
		case 33:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->text_rendering;
			return GF_OK;
		case 34:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->audio_level;
			return GF_OK;
		case 35:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->fill_opacity;
			return GF_OK;
		case 36:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_opacity;
			return GF_OK;
		case 37:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->fill;
			return GF_OK;
		case 38:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->fill_rule;
			return GF_OK;
		case 39:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke;
			return GF_OK;
		case 40:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_dasharray;
			return GF_OK;
		case 41:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 42:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_linecap;
			return GF_OK;
		case 43:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_linejoin;
			return GF_OK;
		case 44:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 45:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stroke_width;
			return GF_OK;
		case 46:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->color;
			return GF_OK;
		case 47:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->color_rendering;
			return GF_OK;
		case 48:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->vector_effect;
			return GF_OK;
		case 49:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->viewport_fill;
			return GF_OK;
		case 50:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 51:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->solid_color;
			return GF_OK;
		case 52:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->solid_opacity;
			return GF_OK;
		case 53:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->display_align;
			return GF_OK;
		case 54:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->line_increment;
			return GF_OK;
		case 55:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stop_color;
			return GF_OK;
		case 56:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->stop_opacity;
			return GF_OK;
		case 57:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->font_family;
			return GF_OK;
		case 58:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->font_size;
			return GF_OK;
		case 59:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->font_style;
			return GF_OK;
		case 60:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->font_weight;
			return GF_OK;
		case 61:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_text()
{
	SVGtextElement *p;
	GF_SAFEALLOC(p, sizeof(SVGtextElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_text);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "text";
	((GF_Node *p)->sgprivate->node_del = SVG_text_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_text_get_attribute;
#endif
	p->transform = gf_list_new();
	p->x = gf_list_new();
	p->y = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_textArea_Del(GF_Node *node)
{
	SVGtextAreaElement *p = (SVGtextAreaElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_textArea_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusable;
			return GF_OK;
		case 13:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusNext;
			return GF_OK;
		case 14:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusPrev;
			return GF_OK;
		case 15:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusNorth;
			return GF_OK;
		case 16:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusNorthEast;
			return GF_OK;
		case 17:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusEast;
			return GF_OK;
		case 18:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusSouthEast;
			return GF_OK;
		case 19:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusSouth;
			return GF_OK;
		case 20:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusSouthWest;
			return GF_OK;
		case 21:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusWest;
			return GF_OK;
		case 22:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->focusNorthWest;
			return GF_OK;
		case 23:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->transform;
			return GF_OK;
		case 24:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->x;
			return GF_OK;
		case 25:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->y;
			return GF_OK;
		case 26:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->editable;
			return GF_OK;
		case 27:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->width;
			return GF_OK;
		case 28:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->height;
			return GF_OK;
		case 29:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->display;
			return GF_OK;
		case 30:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->visibility;
			return GF_OK;
		case 31:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->image_rendering;
			return GF_OK;
		case 32:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->pointer_events;
			return GF_OK;
		case 33:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->shape_rendering;
			return GF_OK;
		case 34:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->text_rendering;
			return GF_OK;
		case 35:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->audio_level;
			return GF_OK;
		case 36:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->fill_opacity;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->fill;
			return GF_OK;
		case 39:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->fill_rule;
			return GF_OK;
		case 40:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke;
			return GF_OK;
		case 41:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_dasharray;
			return GF_OK;
		case 42:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 43:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_linecap;
			return GF_OK;
		case 44:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_linejoin;
			return GF_OK;
		case 45:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 46:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stroke_width;
			return GF_OK;
		case 47:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->color;
			return GF_OK;
		case 48:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->color_rendering;
			return GF_OK;
		case 49:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->vector_effect;
			return GF_OK;
		case 50:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->viewport_fill;
			return GF_OK;
		case 51:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 52:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->solid_color;
			return GF_OK;
		case 53:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->solid_opacity;
			return GF_OK;
		case 54:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->display_align;
			return GF_OK;
		case 55:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->line_increment;
			return GF_OK;
		case 56:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stop_color;
			return GF_OK;
		case 57:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->stop_opacity;
			return GF_OK;
		case 58:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->font_family;
			return GF_OK;
		case 59:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->font_size;
			return GF_OK;
		case 60:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->font_style;
			return GF_OK;
		case 61:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->font_weight;
			return GF_OK;
		case 62:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_textArea()
{
	SVGtextAreaElement *p;
	GF_SAFEALLOC(p, sizeof(SVGtextAreaElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_textArea);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "textArea";
	((GF_Node *p)->sgprivate->node_del = SVG_textArea_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_textArea_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_title_Del(GF_Node *node)
{
	SVGtitleElement *p = (SVGtitleElement *)node;
	free(p->textContent);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_title_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtitleElement *)node)->xml_space;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_title()
{
	SVGtitleElement *p;
	GF_SAFEALLOC(p, sizeof(SVGtitleElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_title);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "title";
	((GF_Node *p)->sgprivate->node_del = SVG_title_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_title_get_attribute;
#endif
	return p;
}

static void SVG_tspan_Del(GF_Node *node)
{
	SVGtspanElement *p = (SVGtspanElement *)node;
	free(p->textContent);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_tspan_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusable;
			return GF_OK;
		case 13:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusNext;
			return GF_OK;
		case 14:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusPrev;
			return GF_OK;
		case 15:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusNorth;
			return GF_OK;
		case 16:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusNorthEast;
			return GF_OK;
		case 17:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusEast;
			return GF_OK;
		case 18:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusSouthEast;
			return GF_OK;
		case 19:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusSouth;
			return GF_OK;
		case 20:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusSouthWest;
			return GF_OK;
		case 21:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusWest;
			return GF_OK;
		case 22:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->focusNorthWest;
			return GF_OK;
		case 23:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->display;
			return GF_OK;
		case 24:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->visibility;
			return GF_OK;
		case 25:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->image_rendering;
			return GF_OK;
		case 26:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->pointer_events;
			return GF_OK;
		case 27:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->shape_rendering;
			return GF_OK;
		case 28:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->text_rendering;
			return GF_OK;
		case 29:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->audio_level;
			return GF_OK;
		case 30:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->fill_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_opacity;
			return GF_OK;
		case 32:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->fill;
			return GF_OK;
		case 33:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->fill_rule;
			return GF_OK;
		case 34:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke;
			return GF_OK;
		case 35:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_dasharray;
			return GF_OK;
		case 36:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 37:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_linecap;
			return GF_OK;
		case 38:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_linejoin;
			return GF_OK;
		case 39:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 40:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stroke_width;
			return GF_OK;
		case 41:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->color;
			return GF_OK;
		case 42:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->color_rendering;
			return GF_OK;
		case 43:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->vector_effect;
			return GF_OK;
		case 44:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->viewport_fill;
			return GF_OK;
		case 45:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 46:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->solid_color;
			return GF_OK;
		case 47:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->solid_opacity;
			return GF_OK;
		case 48:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->display_align;
			return GF_OK;
		case 49:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->line_increment;
			return GF_OK;
		case 50:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stop_color;
			return GF_OK;
		case 51:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->stop_opacity;
			return GF_OK;
		case 52:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->font_family;
			return GF_OK;
		case 53:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->font_size;
			return GF_OK;
		case 54:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->font_style;
			return GF_OK;
		case 55:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->font_weight;
			return GF_OK;
		case 56:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGtspanElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_tspan()
{
	SVGtspanElement *p;
	GF_SAFEALLOC(p, sizeof(SVGtspanElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_tspan);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "tspan";
	((GF_Node *p)->sgprivate->node_del = SVG_tspan_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_tspan_get_attribute;
#endif
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_use_Del(GF_Node *node)
{
	SVGuseElement *p = (SVGuseElement *)node;
	free(p->textContent);
	SVG_DeleteTransformList(p->transform);
	SVG_ResetIRI(&p->xlink_href);
	free(p->fill.color);
	free(p->stroke.color);
	free(p->stroke_dasharray.array.vals);
	free(p->viewport_fill.color);
	free(p->stop_color.color);
	free(p->font_family.value);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_use_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->requiredFeatures;
			return GF_OK;
		case 8:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->requiredExtensions;
			return GF_OK;
		case 9:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->requiredFormats;
			return GF_OK;
		case 10:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->requiredFonts;
			return GF_OK;
		case 11:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->systemLanguage;
			return GF_OK;
		case 12:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->transform;
			return GF_OK;
		case 13:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_actuate;
			return GF_OK;
		case 14:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_type;
			return GF_OK;
		case 15:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_role;
			return GF_OK;
		case 16:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_arcrole;
			return GF_OK;
		case 17:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_title;
			return GF_OK;
		case 18:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_href;
			return GF_OK;
		case 19:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->xlink_show;
			return GF_OK;
		case 20:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusable;
			return GF_OK;
		case 21:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusNext;
			return GF_OK;
		case 22:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusPrev;
			return GF_OK;
		case 23:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusNorth;
			return GF_OK;
		case 24:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusNorthEast;
			return GF_OK;
		case 25:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusEast;
			return GF_OK;
		case 26:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusSouthEast;
			return GF_OK;
		case 27:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusSouth;
			return GF_OK;
		case 28:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusSouthWest;
			return GF_OK;
		case 29:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusWest;
			return GF_OK;
		case 30:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->focusNorthWest;
			return GF_OK;
		case 31:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 32:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->x;
			return GF_OK;
		case 33:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->y;
			return GF_OK;
		case 34:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->display;
			return GF_OK;
		case 35:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->visibility;
			return GF_OK;
		case 36:
			info->name = "image-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->image_rendering;
			return GF_OK;
		case 37:
			info->name = "pointer-events";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->pointer_events;
			return GF_OK;
		case 38:
			info->name = "shape-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->shape_rendering;
			return GF_OK;
		case 39:
			info->name = "text-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->text_rendering;
			return GF_OK;
		case 40:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->audio_level;
			return GF_OK;
		case 41:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->fill_opacity;
			return GF_OK;
		case 42:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_opacity;
			return GF_OK;
		case 43:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->fill;
			return GF_OK;
		case 44:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->fill_rule;
			return GF_OK;
		case 45:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke;
			return GF_OK;
		case 46:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_dasharray;
			return GF_OK;
		case 47:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_dashoffset;
			return GF_OK;
		case 48:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_linecap;
			return GF_OK;
		case 49:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_linejoin;
			return GF_OK;
		case 50:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_miterlimit;
			return GF_OK;
		case 51:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stroke_width;
			return GF_OK;
		case 52:
			info->name = "color";
			info->fieldType = SVG_Color_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->color;
			return GF_OK;
		case 53:
			info->name = "color-rendering";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->color_rendering;
			return GF_OK;
		case 54:
			info->name = "vector-effect";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->vector_effect;
			return GF_OK;
		case 55:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->viewport_fill;
			return GF_OK;
		case 56:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->viewport_fill_opacity;
			return GF_OK;
		case 57:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->solid_color;
			return GF_OK;
		case 58:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->solid_opacity;
			return GF_OK;
		case 59:
			info->name = "display-align";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->display_align;
			return GF_OK;
		case 60:
			info->name = "line-increment";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->line_increment;
			return GF_OK;
		case 61:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stop_color;
			return GF_OK;
		case 62:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->stop_opacity;
			return GF_OK;
		case 63:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->font_family;
			return GF_OK;
		case 64:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->font_size;
			return GF_OK;
		case 65:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->font_style;
			return GF_OK;
		case 66:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->font_weight;
			return GF_OK;
		case 67:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->text_anchor;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_use()
{
	SVGuseElement *p;
	GF_SAFEALLOC(p, sizeof(SVGuseElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_use);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "use";
	((GF_Node *p)->sgprivate->node_del = SVG_use_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_use_get_attribute;
#endif
	p->transform = gf_list_new();
	p->properties.display = &(p->display);
	p->properties.visibility = &(p->visibility);
	p->properties.image_rendering = &(p->image_rendering);
	p->properties.pointer_events = &(p->pointer_events);
	p->properties.shape_rendering = &(p->shape_rendering);
	p->properties.text_rendering = &(p->text_rendering);
	p->properties.audio_level = &(p->audio_level);
	p->fill_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.fill_opacity = &(p->fill_opacity);
	p->stroke_opacity.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_opacity = &(p->stroke_opacity);
	p->fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->fill.color, sizeof(SVG_Color));
	p->properties.fill = &(p->fill);
	p->fill_rule = SVG_FILLRULE_INHERIT;
	p->properties.fill_rule = &(p->fill_rule);
	p->stroke.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stroke.color, sizeof(SVG_Color));
	p->properties.stroke = &(p->stroke);
	p->stroke_dasharray.type = SVG_STROKEDASHARRAY_INHERIT;
	p->properties.stroke_dasharray = &(p->stroke_dasharray);
	p->stroke_dashoffset.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_dashoffset = &(p->stroke_dashoffset);
	p->stroke_linecap = SVG_STROKELINECAP_INHERIT;
	p->properties.stroke_linecap = &(p->stroke_linecap);
	p->stroke_linejoin = SVG_STROKELINEJOIN_INHERIT;
	p->properties.stroke_linejoin = &(p->stroke_linejoin);
	p->stroke_miterlimit.type = SVG_FLOAT_INHERIT;
	p->properties.stroke_miterlimit = &(p->stroke_miterlimit);
	p->stroke_width.type = SVG_LENGTH_INHERIT;
	p->properties.stroke_width = &(p->stroke_width);
	p->color.type = SVG_COLOR_INHERIT;
	p->properties.color = &(p->color);
	p->properties.color_rendering = &(p->color_rendering);
	p->properties.vector_effect = &(p->vector_effect);
	p->viewport_fill.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->viewport_fill.color, sizeof(SVG_Color));
	p->properties.viewport_fill = &(p->viewport_fill);
	p->properties.viewport_fill_opacity = &(p->viewport_fill_opacity);
	p->properties.solid_color = &(p->solid_color);
	p->properties.solid_opacity = &(p->solid_opacity);
	p->properties.display_align = &(p->display_align);
	p->properties.line_increment = &(p->line_increment);
	p->stop_color.type = SVG_PAINT_INHERIT;
	GF_SAFEALLOC(p->stop_color.color, sizeof(SVG_Color));
	p->properties.stop_color = &(p->stop_color);
	p->properties.stop_opacity = &(p->stop_opacity);
	p->properties.font_family = &(p->font_family);
	p->font_size.type = SVG_FLOAT_INHERIT;
	p->properties.font_size = &(p->font_size);
	p->properties.font_style = &(p->font_style);
	p->properties.font_weight = &(p->font_weight);
	p->text_anchor = SVG_TEXTANCHOR_INHERIT;
	p->properties.text_anchor = &(p->text_anchor);
	return p;
}

static void SVG_video_Del(GF_Node *node)
{
	SVGvideoElement *p = (SVGvideoElement *)node;
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err SVG_video_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->id;
			return GF_OK;
		case 1:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->class_attribute;
			return GF_OK;
		case 2:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xml_id;
			return GF_OK;
		case 3:
			info->name = "xml:base";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xml_base;
			return GF_OK;
		case 4:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xml_lang;
			return GF_OK;
		case 5:
			info->name = "textContent";
			info->fieldType = SVG_TextContent_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->textContent;
			return GF_OK;
		case 6:
			info->name = "xml:space";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xml_space;
			return GF_OK;
		case 7:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_actuate;
			return GF_OK;
		case 8:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_type;
			return GF_OK;
		case 9:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_role;
			return GF_OK;
		case 10:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_arcrole;
			return GF_OK;
		case 11:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_title;
			return GF_OK;
		case 12:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_href;
			return GF_OK;
		case 13:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->xlink_show;
			return GF_OK;
		case 14:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->requiredFeatures;
			return GF_OK;
		case 15:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->requiredExtensions;
			return GF_OK;
		case 16:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->requiredFormats;
			return GF_OK;
		case 17:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->requiredFonts;
			return GF_OK;
		case 18:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->systemLanguage;
			return GF_OK;
		case 19:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->externalResourcesRequired;
			return GF_OK;
		case 20:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->begin;
			return GF_OK;
		case 21:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->dur;
			return GF_OK;
		case 22:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->end;
			return GF_OK;
		case 23:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->repeatCount;
			return GF_OK;
		case 24:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->repeatDur;
			return GF_OK;
		case 25:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->restart;
			return GF_OK;
		case 26:
			info->name = "syncBehavior";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->syncBehavior;
			return GF_OK;
		case 27:
			info->name = "syncBehaviorDefault";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->syncBehaviorDefault;
			return GF_OK;
		case 28:
			info->name = "syncTolerance";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->syncTolerance;
			return GF_OK;
		case 29:
			info->name = "syncToleranceDefault";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->syncToleranceDefault;
			return GF_OK;
		case 30:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusable;
			return GF_OK;
		case 31:
			info->name = "focusNext";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusNext;
			return GF_OK;
		case 32:
			info->name = "focusPrev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusPrev;
			return GF_OK;
		case 33:
			info->name = "focusNorth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusNorth;
			return GF_OK;
		case 34:
			info->name = "focusNorthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusNorthEast;
			return GF_OK;
		case 35:
			info->name = "focusEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusEast;
			return GF_OK;
		case 36:
			info->name = "focusSouthEast";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusSouthEast;
			return GF_OK;
		case 37:
			info->name = "focusSouth";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusSouth;
			return GF_OK;
		case 38:
			info->name = "focusSouthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusSouthWest;
			return GF_OK;
		case 39:
			info->name = "focusWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusWest;
			return GF_OK;
		case 40:
			info->name = "focusNorthWest";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->focusNorthWest;
			return GF_OK;
		case 41:
			info->name = "transform";
			info->fieldType = SVG_TransformList_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->transform;
			return GF_OK;
		case 42:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->x;
			return GF_OK;
		case 43:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->y;
			return GF_OK;
		case 44:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->width;
			return GF_OK;
		case 45:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->height;
			return GF_OK;
		case 46:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 47:
			info->name = "type";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->type;
			return GF_OK;
		case 48:
			info->name = "transformBehavior";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->transformBehavior;
			return GF_OK;
		case 49:
			info->name = "overlay";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->overlay;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *SVG_New_video()
{
	SVGvideoElement *p;
	GF_SAFEALLOC(p, sizeof(SVGvideoElement));
	if (!p) return NULL;
	gf_node_setup((GF_Node *)p, TAG_SVG_video);
	gf_sg_parent_setup((GF_Node *) p);
#ifdef GF_NODE_USE_POINTERS
	((GF_Node *p)->sgprivate->name = "video";
	((GF_Node *p)->sgprivate->node_del = SVG_video_Del;
	((GF_Node *p)->sgprivate->get_field = SVG_video_get_attribute;
#endif
	return p;
}

SVGElement *SVG_CreateNode(u32 ElementTag)
{
	switch (ElementTag) {
		case TAG_SVG_a: return SVG_New_a();
		case TAG_SVG_animate: return SVG_New_animate();
		case TAG_SVG_animateColor: return SVG_New_animateColor();
		case TAG_SVG_animateMotion: return SVG_New_animateMotion();
		case TAG_SVG_animateTransform: return SVG_New_animateTransform();
		case TAG_SVG_animation: return SVG_New_animation();
		case TAG_SVG_audio: return SVG_New_audio();
		case TAG_SVG_circle: return SVG_New_circle();
		case TAG_SVG_defs: return SVG_New_defs();
		case TAG_SVG_desc: return SVG_New_desc();
		case TAG_SVG_discard: return SVG_New_discard();
		case TAG_SVG_ellipse: return SVG_New_ellipse();
		case TAG_SVG_font: return SVG_New_font();
		case TAG_SVG_font_face: return SVG_New_font_face();
		case TAG_SVG_font_face_name: return SVG_New_font_face_name();
		case TAG_SVG_font_face_src: return SVG_New_font_face_src();
		case TAG_SVG_font_face_uri: return SVG_New_font_face_uri();
		case TAG_SVG_foreignObject: return SVG_New_foreignObject();
		case TAG_SVG_g: return SVG_New_g();
		case TAG_SVG_glyph: return SVG_New_glyph();
		case TAG_SVG_handler: return SVG_New_handler();
		case TAG_SVG_hkern: return SVG_New_hkern();
		case TAG_SVG_image: return SVG_New_image();
		case TAG_SVG_line: return SVG_New_line();
		case TAG_SVG_linearGradient: return SVG_New_linearGradient();
		case TAG_SVG_listener: return SVG_New_listener();
		case TAG_SVG_metadata: return SVG_New_metadata();
		case TAG_SVG_missing_glyph: return SVG_New_missing_glyph();
		case TAG_SVG_mpath: return SVG_New_mpath();
		case TAG_SVG_path: return SVG_New_path();
		case TAG_SVG_polygon: return SVG_New_polygon();
		case TAG_SVG_polyline: return SVG_New_polyline();
		case TAG_SVG_prefetch: return SVG_New_prefetch();
		case TAG_SVG_radialGradient: return SVG_New_radialGradient();
		case TAG_SVG_rect: return SVG_New_rect();
		case TAG_SVG_script: return SVG_New_script();
		case TAG_SVG_set: return SVG_New_set();
		case TAG_SVG_solidColor: return SVG_New_solidColor();
		case TAG_SVG_stop: return SVG_New_stop();
		case TAG_SVG_svg: return SVG_New_svg();
		case TAG_SVG_switch: return SVG_New_switch();
		case TAG_SVG_tbreak: return SVG_New_tbreak();
		case TAG_SVG_text: return SVG_New_text();
		case TAG_SVG_textArea: return SVG_New_textArea();
		case TAG_SVG_title: return SVG_New_title();
		case TAG_SVG_tspan: return SVG_New_tspan();
		case TAG_SVG_use: return SVG_New_use();
		case TAG_SVG_video: return SVG_New_video();
		default: return NULL;
	}
}

void SVGElement_Del(SVGElement *elt)
{
	GF_Node *node = (GF_Node *)elt;
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: SVG_a_Del(node); return;
		case TAG_SVG_animate: SVG_animate_Del(node); return;
		case TAG_SVG_animateColor: SVG_animateColor_Del(node); return;
		case TAG_SVG_animateMotion: SVG_animateMotion_Del(node); return;
		case TAG_SVG_animateTransform: SVG_animateTransform_Del(node); return;
		case TAG_SVG_animation: SVG_animation_Del(node); return;
		case TAG_SVG_audio: SVG_audio_Del(node); return;
		case TAG_SVG_circle: SVG_circle_Del(node); return;
		case TAG_SVG_defs: SVG_defs_Del(node); return;
		case TAG_SVG_desc: SVG_desc_Del(node); return;
		case TAG_SVG_discard: SVG_discard_Del(node); return;
		case TAG_SVG_ellipse: SVG_ellipse_Del(node); return;
		case TAG_SVG_font: SVG_font_Del(node); return;
		case TAG_SVG_font_face: SVG_font_face_Del(node); return;
		case TAG_SVG_font_face_name: SVG_font_face_name_Del(node); return;
		case TAG_SVG_font_face_src: SVG_font_face_src_Del(node); return;
		case TAG_SVG_font_face_uri: SVG_font_face_uri_Del(node); return;
		case TAG_SVG_foreignObject: SVG_foreignObject_Del(node); return;
		case TAG_SVG_g: SVG_g_Del(node); return;
		case TAG_SVG_glyph: SVG_glyph_Del(node); return;
		case TAG_SVG_handler: SVG_handler_Del(node); return;
		case TAG_SVG_hkern: SVG_hkern_Del(node); return;
		case TAG_SVG_image: SVG_image_Del(node); return;
		case TAG_SVG_line: SVG_line_Del(node); return;
		case TAG_SVG_linearGradient: SVG_linearGradient_Del(node); return;
		case TAG_SVG_listener: SVG_listener_Del(node); return;
		case TAG_SVG_metadata: SVG_metadata_Del(node); return;
		case TAG_SVG_missing_glyph: SVG_missing_glyph_Del(node); return;
		case TAG_SVG_mpath: SVG_mpath_Del(node); return;
		case TAG_SVG_path: SVG_path_Del(node); return;
		case TAG_SVG_polygon: SVG_polygon_Del(node); return;
		case TAG_SVG_polyline: SVG_polyline_Del(node); return;
		case TAG_SVG_prefetch: SVG_prefetch_Del(node); return;
		case TAG_SVG_radialGradient: SVG_radialGradient_Del(node); return;
		case TAG_SVG_rect: SVG_rect_Del(node); return;
		case TAG_SVG_script: SVG_script_Del(node); return;
		case TAG_SVG_set: SVG_set_Del(node); return;
		case TAG_SVG_solidColor: SVG_solidColor_Del(node); return;
		case TAG_SVG_stop: SVG_stop_Del(node); return;
		case TAG_SVG_svg: SVG_svg_Del(node); return;
		case TAG_SVG_switch: SVG_switch_Del(node); return;
		case TAG_SVG_tbreak: SVG_tbreak_Del(node); return;
		case TAG_SVG_text: SVG_text_Del(node); return;
		case TAG_SVG_textArea: SVG_textArea_Del(node); return;
		case TAG_SVG_title: SVG_title_Del(node); return;
		case TAG_SVG_tspan: SVG_tspan_Del(node); return;
		case TAG_SVG_use: SVG_use_Del(node); return;
		case TAG_SVG_video: SVG_video_Del(node); return;
		default: return;
	}
}

u32 SVG_GetAttributeCount(GF_Node *node)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: return 67;
		case TAG_SVG_animate: return 34;
		case TAG_SVG_animateColor: return 34;
		case TAG_SVG_animateMotion: return 37;
		case TAG_SVG_animateTransform: return 35;
		case TAG_SVG_animation: return 48;
		case TAG_SVG_audio: return 31;
		case TAG_SVG_circle: return 61;
		case TAG_SVG_defs: return 41;
		case TAG_SVG_desc: return 7;
		case TAG_SVG_discard: return 15;
		case TAG_SVG_ellipse: return 62;
		case TAG_SVG_font: return 10;
		case TAG_SVG_font_face: return 37;
		case TAG_SVG_font_face_name: return 8;
		case TAG_SVG_font_face_src: return 7;
		case TAG_SVG_font_face_uri: return 14;
		case TAG_SVG_foreignObject: return 70;
		case TAG_SVG_g: return 59;
		case TAG_SVG_glyph: return 13;
		case TAG_SVG_handler: return 10;
		case TAG_SVG_hkern: return 12;
		case TAG_SVG_image: return 73;
		case TAG_SVG_line: return 62;
		case TAG_SVG_linearGradient: return 45;
		case TAG_SVG_listener: return 14;
		case TAG_SVG_metadata: return 7;
		case TAG_SVG_missing_glyph: return 9;
		case TAG_SVG_mpath: return 14;
		case TAG_SVG_path: return 60;
		case TAG_SVG_polygon: return 59;
		case TAG_SVG_polyline: return 59;
		case TAG_SVG_prefetch: return 19;
		case TAG_SVG_radialGradient: return 44;
		case TAG_SVG_rect: return 64;
		case TAG_SVG_script: return 9;
		case TAG_SVG_set: return 26;
		case TAG_SVG_solidColor: return 41;
		case TAG_SVG_stop: return 42;
		case TAG_SVG_svg: return 68;
		case TAG_SVG_switch: return 48;
		case TAG_SVG_tbreak: return 7;
		case TAG_SVG_text: return 62;
		case TAG_SVG_textArea: return 63;
		case TAG_SVG_title: return 7;
		case TAG_SVG_tspan: return 57;
		case TAG_SVG_use: return 68;
		case TAG_SVG_video: return 50;
		default: return 0;
	}
}

GF_Err SVG_GetAttributeInfo(GF_Node *node, GF_FieldInfo *info)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: return SVG_a_get_attribute(node, info);
		case TAG_SVG_animate: return SVG_animate_get_attribute(node, info);
		case TAG_SVG_animateColor: return SVG_animateColor_get_attribute(node, info);
		case TAG_SVG_animateMotion: return SVG_animateMotion_get_attribute(node, info);
		case TAG_SVG_animateTransform: return SVG_animateTransform_get_attribute(node, info);
		case TAG_SVG_animation: return SVG_animation_get_attribute(node, info);
		case TAG_SVG_audio: return SVG_audio_get_attribute(node, info);
		case TAG_SVG_circle: return SVG_circle_get_attribute(node, info);
		case TAG_SVG_defs: return SVG_defs_get_attribute(node, info);
		case TAG_SVG_desc: return SVG_desc_get_attribute(node, info);
		case TAG_SVG_discard: return SVG_discard_get_attribute(node, info);
		case TAG_SVG_ellipse: return SVG_ellipse_get_attribute(node, info);
		case TAG_SVG_font: return SVG_font_get_attribute(node, info);
		case TAG_SVG_font_face: return SVG_font_face_get_attribute(node, info);
		case TAG_SVG_font_face_name: return SVG_font_face_name_get_attribute(node, info);
		case TAG_SVG_font_face_src: return SVG_font_face_src_get_attribute(node, info);
		case TAG_SVG_font_face_uri: return SVG_font_face_uri_get_attribute(node, info);
		case TAG_SVG_foreignObject: return SVG_foreignObject_get_attribute(node, info);
		case TAG_SVG_g: return SVG_g_get_attribute(node, info);
		case TAG_SVG_glyph: return SVG_glyph_get_attribute(node, info);
		case TAG_SVG_handler: return SVG_handler_get_attribute(node, info);
		case TAG_SVG_hkern: return SVG_hkern_get_attribute(node, info);
		case TAG_SVG_image: return SVG_image_get_attribute(node, info);
		case TAG_SVG_line: return SVG_line_get_attribute(node, info);
		case TAG_SVG_linearGradient: return SVG_linearGradient_get_attribute(node, info);
		case TAG_SVG_listener: return SVG_listener_get_attribute(node, info);
		case TAG_SVG_metadata: return SVG_metadata_get_attribute(node, info);
		case TAG_SVG_missing_glyph: return SVG_missing_glyph_get_attribute(node, info);
		case TAG_SVG_mpath: return SVG_mpath_get_attribute(node, info);
		case TAG_SVG_path: return SVG_path_get_attribute(node, info);
		case TAG_SVG_polygon: return SVG_polygon_get_attribute(node, info);
		case TAG_SVG_polyline: return SVG_polyline_get_attribute(node, info);
		case TAG_SVG_prefetch: return SVG_prefetch_get_attribute(node, info);
		case TAG_SVG_radialGradient: return SVG_radialGradient_get_attribute(node, info);
		case TAG_SVG_rect: return SVG_rect_get_attribute(node, info);
		case TAG_SVG_script: return SVG_script_get_attribute(node, info);
		case TAG_SVG_set: return SVG_set_get_attribute(node, info);
		case TAG_SVG_solidColor: return SVG_solidColor_get_attribute(node, info);
		case TAG_SVG_stop: return SVG_stop_get_attribute(node, info);
		case TAG_SVG_svg: return SVG_svg_get_attribute(node, info);
		case TAG_SVG_switch: return SVG_switch_get_attribute(node, info);
		case TAG_SVG_tbreak: return SVG_tbreak_get_attribute(node, info);
		case TAG_SVG_text: return SVG_text_get_attribute(node, info);
		case TAG_SVG_textArea: return SVG_textArea_get_attribute(node, info);
		case TAG_SVG_title: return SVG_title_get_attribute(node, info);
		case TAG_SVG_tspan: return SVG_tspan_get_attribute(node, info);
		case TAG_SVG_use: return SVG_use_get_attribute(node, info);
		case TAG_SVG_video: return SVG_video_get_attribute(node, info);
		default: return GF_BAD_PARAM;
	}
}

u32 SVG_GetTagByName(const char *element_name)
{
	if (!element_name) return TAG_UndefinedNode;
	if (!stricmp(element_name, "a")) return TAG_SVG_a;
	if (!stricmp(element_name, "animate")) return TAG_SVG_animate;
	if (!stricmp(element_name, "animateColor")) return TAG_SVG_animateColor;
	if (!stricmp(element_name, "animateMotion")) return TAG_SVG_animateMotion;
	if (!stricmp(element_name, "animateTransform")) return TAG_SVG_animateTransform;
	if (!stricmp(element_name, "animation")) return TAG_SVG_animation;
	if (!stricmp(element_name, "audio")) return TAG_SVG_audio;
	if (!stricmp(element_name, "circle")) return TAG_SVG_circle;
	if (!stricmp(element_name, "defs")) return TAG_SVG_defs;
	if (!stricmp(element_name, "desc")) return TAG_SVG_desc;
	if (!stricmp(element_name, "discard")) return TAG_SVG_discard;
	if (!stricmp(element_name, "ellipse")) return TAG_SVG_ellipse;
	if (!stricmp(element_name, "font")) return TAG_SVG_font;
	if (!stricmp(element_name, "font-face")) return TAG_SVG_font_face;
	if (!stricmp(element_name, "font-face-name")) return TAG_SVG_font_face_name;
	if (!stricmp(element_name, "font-face-src")) return TAG_SVG_font_face_src;
	if (!stricmp(element_name, "font-face-uri")) return TAG_SVG_font_face_uri;
	if (!stricmp(element_name, "foreignObject")) return TAG_SVG_foreignObject;
	if (!stricmp(element_name, "g")) return TAG_SVG_g;
	if (!stricmp(element_name, "glyph")) return TAG_SVG_glyph;
	if (!stricmp(element_name, "handler")) return TAG_SVG_handler;
	if (!stricmp(element_name, "hkern")) return TAG_SVG_hkern;
	if (!stricmp(element_name, "image")) return TAG_SVG_image;
	if (!stricmp(element_name, "line")) return TAG_SVG_line;
	if (!stricmp(element_name, "linearGradient")) return TAG_SVG_linearGradient;
	if (!stricmp(element_name, "listener")) return TAG_SVG_listener;
	if (!stricmp(element_name, "metadata")) return TAG_SVG_metadata;
	if (!stricmp(element_name, "missing-glyph")) return TAG_SVG_missing_glyph;
	if (!stricmp(element_name, "mpath")) return TAG_SVG_mpath;
	if (!stricmp(element_name, "path")) return TAG_SVG_path;
	if (!stricmp(element_name, "polygon")) return TAG_SVG_polygon;
	if (!stricmp(element_name, "polyline")) return TAG_SVG_polyline;
	if (!stricmp(element_name, "prefetch")) return TAG_SVG_prefetch;
	if (!stricmp(element_name, "radialGradient")) return TAG_SVG_radialGradient;
	if (!stricmp(element_name, "rect")) return TAG_SVG_rect;
	if (!stricmp(element_name, "script")) return TAG_SVG_script;
	if (!stricmp(element_name, "set")) return TAG_SVG_set;
	if (!stricmp(element_name, "solidColor")) return TAG_SVG_solidColor;
	if (!stricmp(element_name, "stop")) return TAG_SVG_stop;
	if (!stricmp(element_name, "svg")) return TAG_SVG_svg;
	if (!stricmp(element_name, "switch")) return TAG_SVG_switch;
	if (!stricmp(element_name, "tbreak")) return TAG_SVG_tbreak;
	if (!stricmp(element_name, "text")) return TAG_SVG_text;
	if (!stricmp(element_name, "textArea")) return TAG_SVG_textArea;
	if (!stricmp(element_name, "title")) return TAG_SVG_title;
	if (!stricmp(element_name, "tspan")) return TAG_SVG_tspan;
	if (!stricmp(element_name, "use")) return TAG_SVG_use;
	if (!stricmp(element_name, "video")) return TAG_SVG_video;
	return TAG_UndefinedNode;
}

const char *SVG_GetElementName(u32 tag)
{
	switch(tag) {
	case TAG_SVG_a: return "a";
	case TAG_SVG_animate: return "animate";
	case TAG_SVG_animateColor: return "animateColor";
	case TAG_SVG_animateMotion: return "animateMotion";
	case TAG_SVG_animateTransform: return "animateTransform";
	case TAG_SVG_animation: return "animation";
	case TAG_SVG_audio: return "audio";
	case TAG_SVG_circle: return "circle";
	case TAG_SVG_defs: return "defs";
	case TAG_SVG_desc: return "desc";
	case TAG_SVG_discard: return "discard";
	case TAG_SVG_ellipse: return "ellipse";
	case TAG_SVG_font: return "font";
	case TAG_SVG_font_face: return "font-face";
	case TAG_SVG_font_face_name: return "font-face-name";
	case TAG_SVG_font_face_src: return "font-face-src";
	case TAG_SVG_font_face_uri: return "font-face-uri";
	case TAG_SVG_foreignObject: return "foreignObject";
	case TAG_SVG_g: return "g";
	case TAG_SVG_glyph: return "glyph";
	case TAG_SVG_handler: return "handler";
	case TAG_SVG_hkern: return "hkern";
	case TAG_SVG_image: return "image";
	case TAG_SVG_line: return "line";
	case TAG_SVG_linearGradient: return "linearGradient";
	case TAG_SVG_listener: return "listener";
	case TAG_SVG_metadata: return "metadata";
	case TAG_SVG_missing_glyph: return "missing-glyph";
	case TAG_SVG_mpath: return "mpath";
	case TAG_SVG_path: return "path";
	case TAG_SVG_polygon: return "polygon";
	case TAG_SVG_polyline: return "polyline";
	case TAG_SVG_prefetch: return "prefetch";
	case TAG_SVG_radialGradient: return "radialGradient";
	case TAG_SVG_rect: return "rect";
	case TAG_SVG_script: return "script";
	case TAG_SVG_set: return "set";
	case TAG_SVG_solidColor: return "solidColor";
	case TAG_SVG_stop: return "stop";
	case TAG_SVG_svg: return "svg";
	case TAG_SVG_switch: return "switch";
	case TAG_SVG_tbreak: return "tbreak";
	case TAG_SVG_text: return "text";
	case TAG_SVG_textArea: return "textArea";
	case TAG_SVG_title: return "title";
	case TAG_SVG_tspan: return "tspan";
	case TAG_SVG_use: return "use";
	case TAG_SVG_video: return "video";
	default: return "UndefinedNode";
	}
}

#endif /*GPAC_DISABLE_SVG*/

