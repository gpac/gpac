/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *    Copyright (c)2004-200X ENST - All rights reserved
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
	DO NOT MOFIFY - File generated on GMT Sat Feb 25 10:56:16 2006

	BY SVGGen for GPAC Version 0.4.1-DEV
*/

#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

void *gf_svg_new_a()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_a_del(GF_Node *node)
{
	SVGaElement *p = (SVGaElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_a_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 54:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 55:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 56:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 57:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 58:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 59:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 60:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 61:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 62:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 63:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 64:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 65:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 66:
			info->name = "target";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = & ((SVGaElement *)node)->target;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_animate()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_anim((SVGElement *)p);
	return p;
}

static void gf_svg_animate_del(GF_Node *node)
{
	SVGanimateElement *p = (SVGanimateElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_animate_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_animateColor()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_anim((SVGElement *)p);
	return p;
}

static void gf_svg_animateColor_del(GF_Node *node)
{
	SVGanimateColorElement *p = (SVGanimateColorElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_animateColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->calcMode;
			return GF_OK;
		case 30:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keySplines;
			return GF_OK;
		case 31:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keyTimes;
			return GF_OK;
		case 32:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->accumulate;
			return GF_OK;
		case 33:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->additive;
			return GF_OK;
		case 34:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_animateMotion()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_anim((SVGElement *)p);
	p->path.commands = gf_list_new();
	p->path.points = gf_list_new();
	p->keyPoints = gf_list_new();
	return p;
}

static void gf_svg_animateMotion_del(GF_Node *node)
{
	SVGanimateMotionElement *p = (SVGanimateMotionElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_reset_path(p->path);
	gf_smil_delete_key_types(p->keyPoints);
	free(p->origin);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_animateMotion_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->to;
			return GF_OK;
		case 24:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->from;
			return GF_OK;
		case 25:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->by;
			return GF_OK;
		case 26:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->values;
			return GF_OK;
		case 27:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->calcMode;
			return GF_OK;
		case 28:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keySplines;
			return GF_OK;
		case 29:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keyTimes;
			return GF_OK;
		case 30:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->accumulate;
			return GF_OK;
		case 31:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->additive;
			return GF_OK;
		case 32:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->lsr_enabled;
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
			info->fieldType = SVG_Rotate_datatype;
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

void *gf_svg_new_animateTransform()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_anim((SVGElement *)p);
	return p;
}

static void gf_svg_animateTransform_del(GF_Node *node)
{
	SVGanimateTransformElement *p = (SVGanimateTransformElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_animateTransform_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "from";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->from;
			return GF_OK;
		case 27:
			info->name = "by";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->by;
			return GF_OK;
		case 28:
			info->name = "values";
			info->fieldType = SMIL_AnimateValues_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->values;
			return GF_OK;
		case 29:
			info->name = "type";
			info->fieldType = SVG_TransformType_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->type;
			return GF_OK;
		case 30:
			info->name = "calcMode";
			info->fieldType = SMIL_CalcMode_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->calcMode;
			return GF_OK;
		case 31:
			info->name = "keySplines";
			info->fieldType = SMIL_KeySplines_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keySplines;
			return GF_OK;
		case 32:
			info->name = "keyTimes";
			info->fieldType = SMIL_KeyTimes_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->keyTimes;
			return GF_OK;
		case 33:
			info->name = "accumulate";
			info->fieldType = SMIL_Accumulate_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->accumulate;
			return GF_OK;
		case 34:
			info->name = "additive";
			info->fieldType = SMIL_Additive_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->additive;
			return GF_OK;
		case 35:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_animation()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_sync((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_animation_del(GF_Node *node)
{
	SVGanimationElement *p = (SVGanimationElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_animation_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 17:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 18:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 19:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 20:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 21:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 22:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 23:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 24:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 25:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 26:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 27:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 28:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 29:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 30:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 31:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 32:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 33:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 34:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 35:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 36:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 37:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 38:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 39:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 40:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 41:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 42:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 43:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 44:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 45:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncBehavior;
			return GF_OK;
		case 46:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncTolerance;
			return GF_OK;
		case 47:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncMaster;
			return GF_OK;
		case 48:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncReference;
			return GF_OK;
		case 49:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 50:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 51:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 52:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 53:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 54:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 55:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->x;
			return GF_OK;
		case 56:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->y;
			return GF_OK;
		case 57:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->width;
			return GF_OK;
		case 58:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->height;
			return GF_OK;
		case 59:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 60:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVGanimationElement *)node)->initialVisibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_audio()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_sync((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	return p;
}

static void gf_svg_audio_del(GF_Node *node)
{
	SVGaudioElement *p = (SVGaudioElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_audio_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 17:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 18:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 19:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 20:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 21:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 22:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 23:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 24:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 25:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 26:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 27:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 28:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 29:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 30:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 31:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 32:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 33:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->clipBegin;
			return GF_OK;
		case 34:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->clipEnd;
			return GF_OK;
		case 35:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncBehavior;
			return GF_OK;
		case 36:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncTolerance;
			return GF_OK;
		case 37:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncMaster;
			return GF_OK;
		case 38:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncReference;
			return GF_OK;
		case 39:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 40:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 41:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 42:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 43:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_circle()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_circle_del(GF_Node *node)
{
	SVGcircleElement *p = (SVGcircleElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_circle_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->cx;
			return GF_OK;
		case 60:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->cy;
			return GF_OK;
		case 61:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGcircleElement *)node)->r;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_defs()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	return p;
}

static void gf_svg_defs_del(GF_Node *node)
{
	SVGdefsElement *p = (SVGdefsElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_defs_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_desc()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_desc_del(GF_Node *node)
{
	SVGdescElement *p = (SVGdescElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_desc_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_discard()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	return p;
}

static void gf_svg_discard_del(GF_Node *node)
{
	SVGdiscardElement *p = (SVGdiscardElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_discard_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_ellipse()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_ellipse_del(GF_Node *node)
{
	SVGellipseElement *p = (SVGellipseElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_ellipse_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->rx;
			return GF_OK;
		case 60:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->ry;
			return GF_OK;
		case 61:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->cx;
			return GF_OK;
		case 62:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGellipseElement *)node)->cy;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_font()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_font_del(GF_Node *node)
{
	SVGfontElement *p = (SVGfontElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_font_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "horiz-origin-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->horiz_origin_x;
			return GF_OK;
		case 8:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfontElement *)node)->horiz_adv_x;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_font_face()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_font_face_del(GF_Node *node)
{
	SVGfont_faceElement *p = (SVGfont_faceElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	if (p->font_family.value) free(p->font_family.value);
	free(p->font_stretch);
	free(p->unicode_range);
	free(p->panose_1);
	free(p->widths);
	free(p->bbox);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_font_face_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_family;
			return GF_OK;
		case 8:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_style;
			return GF_OK;
		case 9:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_weight;
			return GF_OK;
		case 10:
			info->name = "font-variant";
			info->fieldType = SVG_FontVariant_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_variant;
			return GF_OK;
		case 11:
			info->name = "font-stretch";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->font_stretch;
			return GF_OK;
		case 12:
			info->name = "unicode-range";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->unicode_range;
			return GF_OK;
		case 13:
			info->name = "panose-1";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->panose_1;
			return GF_OK;
		case 14:
			info->name = "widths";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->widths;
			return GF_OK;
		case 15:
			info->name = "bbox";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->bbox;
			return GF_OK;
		case 16:
			info->name = "units-per-em";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->units_per_em;
			return GF_OK;
		case 17:
			info->name = "stemv";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->stemv;
			return GF_OK;
		case 18:
			info->name = "stemh";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->stemh;
			return GF_OK;
		case 19:
			info->name = "slope";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->slope;
			return GF_OK;
		case 20:
			info->name = "cap-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->cap_height;
			return GF_OK;
		case 21:
			info->name = "x-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->x_height;
			return GF_OK;
		case 22:
			info->name = "accent-height";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->accent_height;
			return GF_OK;
		case 23:
			info->name = "ascent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->ascent;
			return GF_OK;
		case 24:
			info->name = "descent";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->descent;
			return GF_OK;
		case 25:
			info->name = "ideographic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->ideographic;
			return GF_OK;
		case 26:
			info->name = "alphabetic";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->alphabetic;
			return GF_OK;
		case 27:
			info->name = "mathematical";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->mathematical;
			return GF_OK;
		case 28:
			info->name = "hanging";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->hanging;
			return GF_OK;
		case 29:
			info->name = "underline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->underline_position;
			return GF_OK;
		case 30:
			info->name = "underline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->underline_thickness;
			return GF_OK;
		case 31:
			info->name = "strikethrough-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->strikethrough_position;
			return GF_OK;
		case 32:
			info->name = "strikethrough-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->strikethrough_thickness;
			return GF_OK;
		case 33:
			info->name = "overline-position";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->overline_position;
			return GF_OK;
		case 34:
			info->name = "overline-thickness";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGfont_faceElement *)node)->overline_thickness;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_font_face_name()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_font_face_name_del(GF_Node *node)
{
	SVGfont_face_nameElement *p = (SVGfont_face_nameElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	free(p->name);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_font_face_name_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGfont_face_nameElement *)node)->name;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_font_face_src()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_font_face_src_del(GF_Node *node)
{
	SVGfont_face_srcElement *p = (SVGfont_face_srcElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_font_face_src_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_font_face_uri()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	return p;
}

static void gf_svg_font_face_uri_del(GF_Node *node)
{
	SVGfont_face_uriElement *p = (SVGfont_face_uriElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_font_face_uri_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_foreignObject()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_foreignObject_del(GF_Node *node)
{
	SVGforeignObjectElement *p = (SVGforeignObjectElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_foreignObject_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 54:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 55:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 56:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 57:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 58:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 59:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 60:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 61:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 62:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 63:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 64:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 65:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 66:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->x;
			return GF_OK;
		case 67:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->y;
			return GF_OK;
		case 68:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->width;
			return GF_OK;
		case 69:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGforeignObjectElement *)node)->height;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_g()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_g_del(GF_Node *node)
{
	SVGgElement *p = (SVGgElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_g_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "lsr:choice";
			info->fieldType = LASeR_Choice_datatype;
			info->far_ptr = & ((SVGgElement *)node)->lsr_choice;
			return GF_OK;
		case 60:
			info->name = "lsr:size";
			info->fieldType = LASeR_Size_datatype;
			info->far_ptr = & ((SVGgElement *)node)->lsr_size;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_glyph()
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
	gf_svg_init_core((SVGElement *)p);
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	return p;
}

static void gf_svg_glyph_del(GF_Node *node)
{
	SVGglyphElement *p = (SVGglyphElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	free(p->unicode);
	free(p->glyph_name);
	free(p->arabic_form);
	gf_svg_reset_path(p->d);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "unicode";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->unicode;
			return GF_OK;
		case 8:
			info->name = "glyph-name";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->glyph_name;
			return GF_OK;
		case 9:
			info->name = "arabic-form";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->arabic_form;
			return GF_OK;
		case 10:
			info->name = "lang";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->lang;
			return GF_OK;
		case 11:
			info->name = "horiz-adv-x";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->horiz_adv_x;
			return GF_OK;
		case 12:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGglyphElement *)node)->d;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_handler()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	return p;
}

static void gf_svg_handler_del(GF_Node *node)
{
	SVGhandlerElement *p = (SVGhandlerElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_handler_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "ev:event";
			info->fieldType = XMLEV_Event_datatype;
			info->far_ptr = & ((SVGhandlerElement *)node)->ev_event;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_hkern()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_hkern_del(GF_Node *node)
{
	SVGhkernElement *p = (SVGhkernElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	free(p->u1);
	free(p->g1);
	free(p->u2);
	free(p->g2);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_hkern_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
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

void *gf_svg_new_image()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_image_del(GF_Node *node)
{
	SVGimageElement *p = (SVGimageElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_image_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->opacity;
			return GF_OK;
		case 17:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 18:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 19:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 20:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 21:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 22:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 23:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 24:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 25:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 26:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 27:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 28:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 29:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 30:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 31:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 32:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 33:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 34:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 35:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 36:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 37:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 38:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 39:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 40:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 41:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 42:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->x;
			return GF_OK;
		case 43:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->y;
			return GF_OK;
		case 44:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->width;
			return GF_OK;
		case 45:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->height;
			return GF_OK;
		case 46:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGimageElement *)node)->preserveAspectRatio;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_line()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_line_del(GF_Node *node)
{
	SVGlineElement *p = (SVGlineElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_line_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->x1;
			return GF_OK;
		case 60:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->y1;
			return GF_OK;
		case 61:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->x2;
			return GF_OK;
		case 62:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlineElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_linearGradient()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	p->x2.value = FIX_ONE;
	p->y2.value = FIX_ONE;
	return p;
}

static void gf_svg_linearGradient_del(GF_Node *node)
{
	SVGlinearGradientElement *p = (SVGlinearGradientElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_linearGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 42:
			info->name = "x1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->x1;
			return GF_OK;
		case 43:
			info->name = "y1";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->y1;
			return GF_OK;
		case 44:
			info->name = "x2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->x2;
			return GF_OK;
		case 45:
			info->name = "y2";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGlinearGradientElement *)node)->y2;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_listener()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_listener_del(GF_Node *node)
{
	SVGlistenerElement *p = (SVGlistenerElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->observer);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->target);
	gf_svg_reset_iri(node->sgprivate->scenegraph, &p->handler);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_listener_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "event";
			info->fieldType = XMLEV_Event_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->event;
			return GF_OK;
		case 8:
			info->name = "phase";
			info->fieldType = XMLEV_Phase_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->phase;
			return GF_OK;
		case 9:
			info->name = "propagate";
			info->fieldType = XMLEV_Propagate_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->propagate;
			return GF_OK;
		case 10:
			info->name = "defaultAction";
			info->fieldType = XMLEV_DefaultAction_datatype;
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
		case 14:
			info->name = "lsr:timeAttribute";
			info->fieldType = LASeR_TimeAttribute_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->lsr_timeAttribute;
			return GF_OK;
		case 15:
			info->name = "lsr:delay";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGlistenerElement *)node)->lsr_delay;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_metadata()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_metadata_del(GF_Node *node)
{
	SVGmetadataElement *p = (SVGmetadataElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_metadata_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_missing_glyph()
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
	gf_svg_init_core((SVGElement *)p);
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	return p;
}

static void gf_svg_missing_glyph_del(GF_Node *node)
{
	SVGmissing_glyphElement *p = (SVGmissing_glyphElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_reset_path(p->d);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_missing_glyph_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
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

void *gf_svg_new_mpath()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	return p;
}

static void gf_svg_mpath_del(GF_Node *node)
{
	SVGmpathElement *p = (SVGmpathElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_mpath_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_path()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	p->d.commands = gf_list_new();
	p->d.points = gf_list_new();
	return p;
}

static void gf_svg_path_del(GF_Node *node)
{
	SVGpathElement *p = (SVGpathElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_reset_path(p->d);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_path_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "pathLength";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->pathLength;
			return GF_OK;
		case 60:
			info->name = "d";
			info->fieldType = SVG_PathData_datatype;
			info->far_ptr = & ((SVGpathElement *)node)->d;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_polygon()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	p->points = gf_list_new();
	return p;
}

static void gf_svg_polygon_del(GF_Node *node)
{
	SVGpolygonElement *p = (SVGpolygonElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_delete_points(p->points);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_polygon_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVGpolygonElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_polyline()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	p->points = gf_list_new();
	return p;
}

static void gf_svg_polyline_del(GF_Node *node)
{
	SVGpolylineElement *p = (SVGpolylineElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_delete_points(p->points);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_polyline_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "points";
			info->fieldType = SVG_Points_datatype;
			info->far_ptr = & ((SVGpolylineElement *)node)->points;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_prefetch()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	return p;
}

static void gf_svg_prefetch_del(GF_Node *node)
{
	SVGprefetchElement *p = (SVGprefetchElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	free(p->mediaTime);
	free(p->mediaCharacterEncoding);
	free(p->mediaContentEncodings);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_prefetch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
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

void *gf_svg_new_radialGradient()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	p->cx.value = FIX_ONE/2;
	p->cy.value = FIX_ONE/2;
	p->r.value = FIX_ONE/2;
	return p;
}

static void gf_svg_radialGradient_del(GF_Node *node)
{
	SVGradialGradientElement *p = (SVGradialGradientElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_radialGradient_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "gradientUnits";
			info->fieldType = SVG_GradientUnit_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->gradientUnits;
			return GF_OK;
		case 42:
			info->name = "cx";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->cx;
			return GF_OK;
		case 43:
			info->name = "cy";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->cy;
			return GF_OK;
		case 44:
			info->name = "r";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGradialGradientElement *)node)->r;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_rect()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_rect_del(GF_Node *node)
{
	SVGrectElement *p = (SVGrectElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_rect_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->x;
			return GF_OK;
		case 60:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->y;
			return GF_OK;
		case 61:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->width;
			return GF_OK;
		case 62:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->height;
			return GF_OK;
		case 63:
			info->name = "rx";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->rx;
			return GF_OK;
		case 64:
			info->name = "ry";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGrectElement *)node)->ry;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_script()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	svg_init_lsr_script(&p->lsr_script);
	return p;
}

static void gf_svg_script_del(GF_Node *node)
{
	SVGscriptElement *p = (SVGscriptElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	svg_reset_lsr_script(&p->lsr_script);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_script_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "lsr:begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->lsr_begin;
			return GF_OK;
		case 8:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGscriptElement *)node)->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_set()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_anim((SVGElement *)p);
	return p;
}

static void gf_svg_set_del(GF_Node *node)
{
	SVGsetElement *p = (SVGsetElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_set_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 8:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 9:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 10:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 11:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 12:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 13:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 14:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 15:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 16:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 17:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 18:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 19:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 20:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 21:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 22:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 23:
			info->name = "attributeName";
			info->fieldType = SMIL_AttributeName_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeName;
			return GF_OK;
		case 24:
			info->name = "attributeType";
			info->fieldType = SMIL_AttributeType_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->attributeType;
			return GF_OK;
		case 25:
			info->name = "to";
			info->fieldType = SMIL_AnimateValue_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->to;
			return GF_OK;
		case 26:
			info->name = "lsr:enabled";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->anim->lsr_enabled;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_solidColor()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	return p;
}

static void gf_svg_solidColor_del(GF_Node *node)
{
	SVGsolidColorElement *p = (SVGsolidColorElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_solidColor_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_stop()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	return p;
}

static void gf_svg_stop_del(GF_Node *node)
{
	SVGstopElement *p = (SVGstopElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_stop_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "offset";
			info->fieldType = SVG_Number_datatype;
			info->far_ptr = & ((SVGstopElement *)node)->offset;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_svg()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_sync((SVGElement *)p);
	p->width.value = INT2FIX(100);
	p->height.value = INT2FIX(100);
	return p;
}

static void gf_svg_svg_del(GF_Node *node)
{
	SVGsvgElement *p = (SVGsvgElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	free(p->version);
	free(p->baseProfile);
	free(p->contentScriptType);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_svg_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "syncBehaviorDefault";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncBehaviorDefault;
			return GF_OK;
		case 54:
			info->name = "syncToleranceDefault";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncToleranceDefault;
			return GF_OK;
		case 55:
			info->name = "viewBox";
			info->fieldType = SVG_ViewBox_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->viewBox;
			return GF_OK;
		case 56:
			info->name = "zoomAndPan";
			info->fieldType = SVG_ZoomAndPan_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->zoomAndPan;
			return GF_OK;
		case 57:
			info->name = "version";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->version;
			return GF_OK;
		case 58:
			info->name = "baseProfile";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->baseProfile;
			return GF_OK;
		case 59:
			info->name = "contentScriptType";
			info->fieldType = SVG_ContentType_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->contentScriptType;
			return GF_OK;
		case 60:
			info->name = "snapshotTime";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->snapshotTime;
			return GF_OK;
		case 61:
			info->name = "timelineBegin";
			info->fieldType = SVG_TimelineBegin_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->timelineBegin;
			return GF_OK;
		case 62:
			info->name = "playbackOrder";
			info->fieldType = SVG_PlaybackOrder_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->playbackOrder;
			return GF_OK;
		case 63:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->width;
			return GF_OK;
		case 64:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->height;
			return GF_OK;
		case 65:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGsvgElement *)node)->preserveAspectRatio;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_switch()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_switch_del(GF_Node *node)
{
	SVGswitchElement *p = (SVGswitchElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_switch_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_tbreak()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_tbreak_del(GF_Node *node)
{
	SVGtbreakElement *p = (SVGtbreakElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_tbreak_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_text()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	p->x = gf_list_new();
	p->y = gf_list_new();
	return p;
}

static void gf_svg_text_del(GF_Node *node)
{
	SVGtextElement *p = (SVGtextElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_svg_delete_coordinates(p->x);
	gf_svg_delete_coordinates(p->y);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_text_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "x";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->x;
			return GF_OK;
		case 60:
			info->name = "y";
			info->fieldType = SVG_Coordinates_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->y;
			return GF_OK;
		case 61:
			info->name = "rotate";
			info->fieldType = SVG_Numbers_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->rotate;
			return GF_OK;
		case 62:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextElement *)node)->editable;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_textArea()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_textArea_del(GF_Node *node)
{
	SVGtextAreaElement *p = (SVGtextAreaElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_textArea_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 58:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 59:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->width;
			return GF_OK;
		case 60:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->height;
			return GF_OK;
		case 61:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->x;
			return GF_OK;
		case 62:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->y;
			return GF_OK;
		case 63:
			info->name = "editable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = & ((SVGtextAreaElement *)node)->editable;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_title()
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
	gf_svg_init_core((SVGElement *)p);
	return p;
}

static void gf_svg_title_del(GF_Node *node)
{
	SVGtitleElement *p = (SVGtitleElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_title_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_tspan()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	return p;
}

static void gf_svg_tspan_del(GF_Node *node)
{
	SVGtspanElement *p = (SVGtspanElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_tspan_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 54:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 55:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 56:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 57:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_use()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_use_del(GF_Node *node)
{
	SVGuseElement *p = (SVGuseElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_use_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "color";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color;
			return GF_OK;
		case 17:
			info->name = "color-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->color_rendering;
			return GF_OK;
		case 18:
			info->name = "display-align";
			info->fieldType = SVG_DisplayAlign_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display_align;
			return GF_OK;
		case 19:
			info->name = "fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 20:
			info->name = "fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_opacity;
			return GF_OK;
		case 21:
			info->name = "fill-rule";
			info->fieldType = SVG_FillRule_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill_rule;
			return GF_OK;
		case 22:
			info->name = "font-family";
			info->fieldType = SVG_FontFamily_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_family;
			return GF_OK;
		case 23:
			info->name = "font-size";
			info->fieldType = SVG_FontSize_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_size;
			return GF_OK;
		case 24:
			info->name = "font-style";
			info->fieldType = SVG_FontStyle_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_style;
			return GF_OK;
		case 25:
			info->name = "font-weight";
			info->fieldType = SVG_FontWeight_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->font_weight;
			return GF_OK;
		case 26:
			info->name = "line-increment";
			info->fieldType = SVG_LineIncrement_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->line_increment;
			return GF_OK;
		case 27:
			info->name = "solid-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_color;
			return GF_OK;
		case 28:
			info->name = "solid-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->solid_opacity;
			return GF_OK;
		case 29:
			info->name = "stop-color";
			info->fieldType = SVG_SVGColor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_color;
			return GF_OK;
		case 30:
			info->name = "stop-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stop_opacity;
			return GF_OK;
		case 31:
			info->name = "stroke";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke;
			return GF_OK;
		case 32:
			info->name = "stroke-dasharray";
			info->fieldType = SVG_StrokeDashArray_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dasharray;
			return GF_OK;
		case 33:
			info->name = "stroke-dashoffset";
			info->fieldType = SVG_StrokeDashOffset_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_dashoffset;
			return GF_OK;
		case 34:
			info->name = "stroke-linecap";
			info->fieldType = SVG_StrokeLineCap_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linecap;
			return GF_OK;
		case 35:
			info->name = "stroke-linejoin";
			info->fieldType = SVG_StrokeLineJoin_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_linejoin;
			return GF_OK;
		case 36:
			info->name = "stroke-miterlimit";
			info->fieldType = SVG_StrokeMiterLimit_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_miterlimit;
			return GF_OK;
		case 37:
			info->name = "stroke-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_opacity;
			return GF_OK;
		case 38:
			info->name = "stroke-width";
			info->fieldType = SVG_StrokeWidth_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->stroke_width;
			return GF_OK;
		case 39:
			info->name = "text-anchor";
			info->fieldType = SVG_TextAnchor_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_anchor;
			return GF_OK;
		case 40:
			info->name = "vector-effect";
			info->fieldType = SVG_VectorEffect_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->vector_effect;
			return GF_OK;
		case 41:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 42:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 43:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 44:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 45:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 46:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 47:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 48:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 49:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 50:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 51:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 52:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 53:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 54:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 55:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 56:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 57:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 58:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 59:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 60:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 61:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 62:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 63:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 64:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 65:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 66:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->x;
			return GF_OK;
		case 67:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGuseElement *)node)->y;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

void *gf_svg_new_video()
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
	gf_svg_init_core((SVGElement *)p);
	gf_svg_init_properties((SVGElement *)p);
	gf_svg_init_focus((SVGElement *)p);
	gf_svg_init_xlink((SVGElement *)p);
	gf_svg_init_timing((SVGElement *)p);
	gf_svg_init_sync((SVGElement *)p);
	gf_svg_init_conditional((SVGElement *)p);
	gf_mx2d_init(p->transform);
	return p;
}

static void gf_svg_video_del(GF_Node *node)
{
	SVGvideoElement *p = (SVGvideoElement *)node;
	gf_svg_reset_base_element((SVGElement *)p);
	gf_sg_parent_reset((GF_Node *) p);
	gf_node_free((GF_Node *)p);
}

static GF_Err gf_svg_video_get_attribute(GF_Node *node, GF_FieldInfo *info)
{
	switch (info->fieldIndex) {
		case 0:
			info->name = "id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 1:
			info->name = "xml:id";
			info->fieldType = SVG_ID_datatype;
			info->far_ptr = &node->sgprivate->NodeName;
			return GF_OK;
		case 2:
			info->name = "class";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->_class;
			return GF_OK;
		case 3:
			info->name = "xml:lang";
			info->fieldType = SVG_LanguageID_datatype;
			info->far_ptr = &((SVGElement *)node)->core->lang;
			return GF_OK;
		case 4:
			info->name = "xml:base";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->core->base;
			return GF_OK;
		case 5:
			info->name = "xml:space";
			info->fieldType = XML_Space_datatype;
			info->far_ptr = &((SVGElement *)node)->core->space;
			return GF_OK;
		case 6:
			info->name = "externalResourcesRequired";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->core->eRR;
			return GF_OK;
		case 7:
			info->name = "audio-level";
			info->fieldType = SVG_AudioLevel_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->audio_level;
			return GF_OK;
		case 8:
			info->name = "display";
			info->fieldType = SVG_Display_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->display;
			return GF_OK;
		case 9:
			info->name = "image-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->image_rendering;
			return GF_OK;
		case 10:
			info->name = "pointer-events";
			info->fieldType = SVG_PointerEvents_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->pointer_events;
			return GF_OK;
		case 11:
			info->name = "shape-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->shape_rendering;
			return GF_OK;
		case 12:
			info->name = "text-rendering";
			info->fieldType = SVG_RenderingHint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->text_rendering;
			return GF_OK;
		case 13:
			info->name = "viewport-fill";
			info->fieldType = SVG_Paint_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill;
			return GF_OK;
		case 14:
			info->name = "viewport-fill-opacity";
			info->fieldType = SVG_Opacity_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->viewport_fill_opacity;
			return GF_OK;
		case 15:
			info->name = "visibility";
			info->fieldType = SVG_Visibility_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->visibility;
			return GF_OK;
		case 16:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->properties->fill;
			return GF_OK;
		case 17:
			info->name = "focusHighlight";
			info->fieldType = SVG_FocusHighlight_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusHighlight;
			return GF_OK;
		case 18:
			info->name = "focusable";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->focusable;
			return GF_OK;
		case 19:
			info->name = "nav-down";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down;
			return GF_OK;
		case 20:
			info->name = "nav-down-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_left;
			return GF_OK;
		case 21:
			info->name = "nav-down-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_down_right;
			return GF_OK;
		case 22:
			info->name = "nav-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_left;
			return GF_OK;
		case 23:
			info->name = "nav-next";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_next;
			return GF_OK;
		case 24:
			info->name = "nav-prev";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_prev;
			return GF_OK;
		case 25:
			info->name = "nav-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_right;
			return GF_OK;
		case 26:
			info->name = "nav-up";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up;
			return GF_OK;
		case 27:
			info->name = "nav-up-left";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_left;
			return GF_OK;
		case 28:
			info->name = "nav-up-right";
			info->fieldType = SVG_Focus_datatype;
			info->far_ptr = &((SVGElement *)node)->focus->nav_up_right;
			return GF_OK;
		case 29:
			info->name = "xlink:href";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->href;
			return GF_OK;
		case 30:
			info->name = "xlink:show";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->show;
			return GF_OK;
		case 31:
			info->name = "xlink:title";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->title;
			return GF_OK;
		case 32:
			info->name = "xlink:actuate";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->actuate;
			return GF_OK;
		case 33:
			info->name = "xlink:role";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->role;
			return GF_OK;
		case 34:
			info->name = "xlink:arcrole";
			info->fieldType = SVG_IRI_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->arcrole;
			return GF_OK;
		case 35:
			info->name = "xlink:type";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->xlink->type;
			return GF_OK;
		case 36:
			info->name = "begin";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->begin;
			return GF_OK;
		case 37:
			info->name = "end";
			info->fieldType = SMIL_Times_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->end;
			return GF_OK;
		case 38:
			info->name = "dur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->dur;
			return GF_OK;
		case 39:
			info->name = "repeatCount";
			info->fieldType = SMIL_RepeatCount_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatCount;
			return GF_OK;
		case 40:
			info->name = "repeatDur";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->repeatDur;
			return GF_OK;
		case 41:
			info->name = "restart";
			info->fieldType = SMIL_Restart_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->restart;
			return GF_OK;
		case 42:
			info->name = "min";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->min;
			return GF_OK;
		case 43:
			info->name = "max";
			info->fieldType = SMIL_Duration_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->max;
			return GF_OK;
		case 44:
			info->name = "fill";
			info->fieldType = SMIL_Fill_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->fill;
			return GF_OK;
		case 45:
			info->name = "clipBegin";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->clipBegin;
			return GF_OK;
		case 46:
			info->name = "clipEnd";
			info->fieldType = SVG_Clock_datatype;
			info->far_ptr = &((SVGElement *)node)->timing->clipEnd;
			return GF_OK;
		case 47:
			info->name = "syncBehavior";
			info->fieldType = SMIL_SyncBehavior_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncBehavior;
			return GF_OK;
		case 48:
			info->name = "syncTolerance";
			info->fieldType = SMIL_SyncTolerance_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncTolerance;
			return GF_OK;
		case 49:
			info->name = "syncMaster";
			info->fieldType = SVG_Boolean_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncMaster;
			return GF_OK;
		case 50:
			info->name = "syncReference";
			info->fieldType = SVG_String_datatype;
			info->far_ptr = &((SVGElement *)node)->sync->syncReference;
			return GF_OK;
		case 51:
			info->name = "requiredExtensions";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredExtensions;
			return GF_OK;
		case 52:
			info->name = "requiredFeatures";
			info->fieldType = SVG_ListOfIRI_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFeatures;
			return GF_OK;
		case 53:
			info->name = "requiredFonts";
			info->fieldType = SVG_FontList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFonts;
			return GF_OK;
		case 54:
			info->name = "requiredFormats";
			info->fieldType = SVG_FormatList_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->requiredFormats;
			return GF_OK;
		case 55:
			info->name = "systemLanguage";
			info->fieldType = SVG_LanguageIDs_datatype;
			info->far_ptr = &((SVGElement *)node)->conditional->systemLanguage;
			return GF_OK;
		case 56:
			info->name = "transform";
			info->fieldType = SVG_Matrix_datatype;
			info->far_ptr = &((SVGTransformableElement *)node)->transform;
			return GF_OK;
		case 57:
			info->name = "transformBehavior";
			info->fieldType = SVG_TransformBehavior_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->transformBehavior;
			return GF_OK;
		case 58:
			info->name = "overlay";
			info->fieldType = SVG_Overlay_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->overlay;
			return GF_OK;
		case 59:
			info->name = "x";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->x;
			return GF_OK;
		case 60:
			info->name = "y";
			info->fieldType = SVG_Coordinate_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->y;
			return GF_OK;
		case 61:
			info->name = "width";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->width;
			return GF_OK;
		case 62:
			info->name = "height";
			info->fieldType = SVG_Length_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->height;
			return GF_OK;
		case 63:
			info->name = "preserveAspectRatio";
			info->fieldType = SVG_PreserveAspectRatio_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->preserveAspectRatio;
			return GF_OK;
		case 64:
			info->name = "initialVisibility";
			info->fieldType = SVG_InitialVisibility_datatype;
			info->far_ptr = & ((SVGvideoElement *)node)->initialVisibility;
			return GF_OK;
		default: return GF_BAD_PARAM;
	}
}

SVGElement *gf_svg_create_node(u32 ElementTag)
{
	switch (ElementTag) {
		case TAG_SVG_a: return gf_svg_new_a();
		case TAG_SVG_animate: return gf_svg_new_animate();
		case TAG_SVG_animateColor: return gf_svg_new_animateColor();
		case TAG_SVG_animateMotion: return gf_svg_new_animateMotion();
		case TAG_SVG_animateTransform: return gf_svg_new_animateTransform();
		case TAG_SVG_animation: return gf_svg_new_animation();
		case TAG_SVG_audio: return gf_svg_new_audio();
		case TAG_SVG_circle: return gf_svg_new_circle();
		case TAG_SVG_defs: return gf_svg_new_defs();
		case TAG_SVG_desc: return gf_svg_new_desc();
		case TAG_SVG_discard: return gf_svg_new_discard();
		case TAG_SVG_ellipse: return gf_svg_new_ellipse();
		case TAG_SVG_font: return gf_svg_new_font();
		case TAG_SVG_font_face: return gf_svg_new_font_face();
		case TAG_SVG_font_face_name: return gf_svg_new_font_face_name();
		case TAG_SVG_font_face_src: return gf_svg_new_font_face_src();
		case TAG_SVG_font_face_uri: return gf_svg_new_font_face_uri();
		case TAG_SVG_foreignObject: return gf_svg_new_foreignObject();
		case TAG_SVG_g: return gf_svg_new_g();
		case TAG_SVG_glyph: return gf_svg_new_glyph();
		case TAG_SVG_handler: return gf_svg_new_handler();
		case TAG_SVG_hkern: return gf_svg_new_hkern();
		case TAG_SVG_image: return gf_svg_new_image();
		case TAG_SVG_line: return gf_svg_new_line();
		case TAG_SVG_linearGradient: return gf_svg_new_linearGradient();
		case TAG_SVG_listener: return gf_svg_new_listener();
		case TAG_SVG_metadata: return gf_svg_new_metadata();
		case TAG_SVG_missing_glyph: return gf_svg_new_missing_glyph();
		case TAG_SVG_mpath: return gf_svg_new_mpath();
		case TAG_SVG_path: return gf_svg_new_path();
		case TAG_SVG_polygon: return gf_svg_new_polygon();
		case TAG_SVG_polyline: return gf_svg_new_polyline();
		case TAG_SVG_prefetch: return gf_svg_new_prefetch();
		case TAG_SVG_radialGradient: return gf_svg_new_radialGradient();
		case TAG_SVG_rect: return gf_svg_new_rect();
		case TAG_SVG_script: return gf_svg_new_script();
		case TAG_SVG_set: return gf_svg_new_set();
		case TAG_SVG_solidColor: return gf_svg_new_solidColor();
		case TAG_SVG_stop: return gf_svg_new_stop();
		case TAG_SVG_svg: return gf_svg_new_svg();
		case TAG_SVG_switch: return gf_svg_new_switch();
		case TAG_SVG_tbreak: return gf_svg_new_tbreak();
		case TAG_SVG_text: return gf_svg_new_text();
		case TAG_SVG_textArea: return gf_svg_new_textArea();
		case TAG_SVG_title: return gf_svg_new_title();
		case TAG_SVG_tspan: return gf_svg_new_tspan();
		case TAG_SVG_use: return gf_svg_new_use();
		case TAG_SVG_video: return gf_svg_new_video();
		default: return NULL;
	}
}

void gf_svg_element_del(SVGElement *elt)
{
	GF_Node *node = (GF_Node *)elt;
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: gf_svg_a_del(node); return;
		case TAG_SVG_animate: gf_svg_animate_del(node); return;
		case TAG_SVG_animateColor: gf_svg_animateColor_del(node); return;
		case TAG_SVG_animateMotion: gf_svg_animateMotion_del(node); return;
		case TAG_SVG_animateTransform: gf_svg_animateTransform_del(node); return;
		case TAG_SVG_animation: gf_svg_animation_del(node); return;
		case TAG_SVG_audio: gf_svg_audio_del(node); return;
		case TAG_SVG_circle: gf_svg_circle_del(node); return;
		case TAG_SVG_defs: gf_svg_defs_del(node); return;
		case TAG_SVG_desc: gf_svg_desc_del(node); return;
		case TAG_SVG_discard: gf_svg_discard_del(node); return;
		case TAG_SVG_ellipse: gf_svg_ellipse_del(node); return;
		case TAG_SVG_font: gf_svg_font_del(node); return;
		case TAG_SVG_font_face: gf_svg_font_face_del(node); return;
		case TAG_SVG_font_face_name: gf_svg_font_face_name_del(node); return;
		case TAG_SVG_font_face_src: gf_svg_font_face_src_del(node); return;
		case TAG_SVG_font_face_uri: gf_svg_font_face_uri_del(node); return;
		case TAG_SVG_foreignObject: gf_svg_foreignObject_del(node); return;
		case TAG_SVG_g: gf_svg_g_del(node); return;
		case TAG_SVG_glyph: gf_svg_glyph_del(node); return;
		case TAG_SVG_handler: gf_svg_handler_del(node); return;
		case TAG_SVG_hkern: gf_svg_hkern_del(node); return;
		case TAG_SVG_image: gf_svg_image_del(node); return;
		case TAG_SVG_line: gf_svg_line_del(node); return;
		case TAG_SVG_linearGradient: gf_svg_linearGradient_del(node); return;
		case TAG_SVG_listener: gf_svg_listener_del(node); return;
		case TAG_SVG_metadata: gf_svg_metadata_del(node); return;
		case TAG_SVG_missing_glyph: gf_svg_missing_glyph_del(node); return;
		case TAG_SVG_mpath: gf_svg_mpath_del(node); return;
		case TAG_SVG_path: gf_svg_path_del(node); return;
		case TAG_SVG_polygon: gf_svg_polygon_del(node); return;
		case TAG_SVG_polyline: gf_svg_polyline_del(node); return;
		case TAG_SVG_prefetch: gf_svg_prefetch_del(node); return;
		case TAG_SVG_radialGradient: gf_svg_radialGradient_del(node); return;
		case TAG_SVG_rect: gf_svg_rect_del(node); return;
		case TAG_SVG_script: gf_svg_script_del(node); return;
		case TAG_SVG_set: gf_svg_set_del(node); return;
		case TAG_SVG_solidColor: gf_svg_solidColor_del(node); return;
		case TAG_SVG_stop: gf_svg_stop_del(node); return;
		case TAG_SVG_svg: gf_svg_svg_del(node); return;
		case TAG_SVG_switch: gf_svg_switch_del(node); return;
		case TAG_SVG_tbreak: gf_svg_tbreak_del(node); return;
		case TAG_SVG_text: gf_svg_text_del(node); return;
		case TAG_SVG_textArea: gf_svg_textArea_del(node); return;
		case TAG_SVG_title: gf_svg_title_del(node); return;
		case TAG_SVG_tspan: gf_svg_tspan_del(node); return;
		case TAG_SVG_use: gf_svg_use_del(node); return;
		case TAG_SVG_video: gf_svg_video_del(node); return;
		default: return;
	}
}

u32 gf_svg_get_attribute_count(GF_Node *node)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: return 67;
		case TAG_SVG_animate: return 35;
		case TAG_SVG_animateColor: return 35;
		case TAG_SVG_animateMotion: return 37;
		case TAG_SVG_animateTransform: return 36;
		case TAG_SVG_animation: return 61;
		case TAG_SVG_audio: return 44;
		case TAG_SVG_circle: return 62;
		case TAG_SVG_defs: return 41;
		case TAG_SVG_desc: return 7;
		case TAG_SVG_discard: return 15;
		case TAG_SVG_ellipse: return 63;
		case TAG_SVG_font: return 9;
		case TAG_SVG_font_face: return 35;
		case TAG_SVG_font_face_name: return 8;
		case TAG_SVG_font_face_src: return 7;
		case TAG_SVG_font_face_uri: return 14;
		case TAG_SVG_foreignObject: return 70;
		case TAG_SVG_g: return 61;
		case TAG_SVG_glyph: return 13;
		case TAG_SVG_handler: return 8;
		case TAG_SVG_hkern: return 12;
		case TAG_SVG_image: return 47;
		case TAG_SVG_line: return 63;
		case TAG_SVG_linearGradient: return 46;
		case TAG_SVG_listener: return 16;
		case TAG_SVG_metadata: return 7;
		case TAG_SVG_missing_glyph: return 9;
		case TAG_SVG_mpath: return 14;
		case TAG_SVG_path: return 61;
		case TAG_SVG_polygon: return 60;
		case TAG_SVG_polyline: return 60;
		case TAG_SVG_prefetch: return 19;
		case TAG_SVG_radialGradient: return 45;
		case TAG_SVG_rect: return 65;
		case TAG_SVG_script: return 9;
		case TAG_SVG_set: return 27;
		case TAG_SVG_solidColor: return 41;
		case TAG_SVG_stop: return 42;
		case TAG_SVG_svg: return 66;
		case TAG_SVG_switch: return 59;
		case TAG_SVG_tbreak: return 7;
		case TAG_SVG_text: return 63;
		case TAG_SVG_textArea: return 64;
		case TAG_SVG_title: return 7;
		case TAG_SVG_tspan: return 58;
		case TAG_SVG_use: return 68;
		case TAG_SVG_video: return 65;
		default: return 0;
	}
}

GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	switch (node->sgprivate->tag) {
		case TAG_SVG_a: return gf_svg_a_get_attribute(node, info);
		case TAG_SVG_animate: return gf_svg_animate_get_attribute(node, info);
		case TAG_SVG_animateColor: return gf_svg_animateColor_get_attribute(node, info);
		case TAG_SVG_animateMotion: return gf_svg_animateMotion_get_attribute(node, info);
		case TAG_SVG_animateTransform: return gf_svg_animateTransform_get_attribute(node, info);
		case TAG_SVG_animation: return gf_svg_animation_get_attribute(node, info);
		case TAG_SVG_audio: return gf_svg_audio_get_attribute(node, info);
		case TAG_SVG_circle: return gf_svg_circle_get_attribute(node, info);
		case TAG_SVG_defs: return gf_svg_defs_get_attribute(node, info);
		case TAG_SVG_desc: return gf_svg_desc_get_attribute(node, info);
		case TAG_SVG_discard: return gf_svg_discard_get_attribute(node, info);
		case TAG_SVG_ellipse: return gf_svg_ellipse_get_attribute(node, info);
		case TAG_SVG_font: return gf_svg_font_get_attribute(node, info);
		case TAG_SVG_font_face: return gf_svg_font_face_get_attribute(node, info);
		case TAG_SVG_font_face_name: return gf_svg_font_face_name_get_attribute(node, info);
		case TAG_SVG_font_face_src: return gf_svg_font_face_src_get_attribute(node, info);
		case TAG_SVG_font_face_uri: return gf_svg_font_face_uri_get_attribute(node, info);
		case TAG_SVG_foreignObject: return gf_svg_foreignObject_get_attribute(node, info);
		case TAG_SVG_g: return gf_svg_g_get_attribute(node, info);
		case TAG_SVG_glyph: return gf_svg_glyph_get_attribute(node, info);
		case TAG_SVG_handler: return gf_svg_handler_get_attribute(node, info);
		case TAG_SVG_hkern: return gf_svg_hkern_get_attribute(node, info);
		case TAG_SVG_image: return gf_svg_image_get_attribute(node, info);
		case TAG_SVG_line: return gf_svg_line_get_attribute(node, info);
		case TAG_SVG_linearGradient: return gf_svg_linearGradient_get_attribute(node, info);
		case TAG_SVG_listener: return gf_svg_listener_get_attribute(node, info);
		case TAG_SVG_metadata: return gf_svg_metadata_get_attribute(node, info);
		case TAG_SVG_missing_glyph: return gf_svg_missing_glyph_get_attribute(node, info);
		case TAG_SVG_mpath: return gf_svg_mpath_get_attribute(node, info);
		case TAG_SVG_path: return gf_svg_path_get_attribute(node, info);
		case TAG_SVG_polygon: return gf_svg_polygon_get_attribute(node, info);
		case TAG_SVG_polyline: return gf_svg_polyline_get_attribute(node, info);
		case TAG_SVG_prefetch: return gf_svg_prefetch_get_attribute(node, info);
		case TAG_SVG_radialGradient: return gf_svg_radialGradient_get_attribute(node, info);
		case TAG_SVG_rect: return gf_svg_rect_get_attribute(node, info);
		case TAG_SVG_script: return gf_svg_script_get_attribute(node, info);
		case TAG_SVG_set: return gf_svg_set_get_attribute(node, info);
		case TAG_SVG_solidColor: return gf_svg_solidColor_get_attribute(node, info);
		case TAG_SVG_stop: return gf_svg_stop_get_attribute(node, info);
		case TAG_SVG_svg: return gf_svg_svg_get_attribute(node, info);
		case TAG_SVG_switch: return gf_svg_switch_get_attribute(node, info);
		case TAG_SVG_tbreak: return gf_svg_tbreak_get_attribute(node, info);
		case TAG_SVG_text: return gf_svg_text_get_attribute(node, info);
		case TAG_SVG_textArea: return gf_svg_textArea_get_attribute(node, info);
		case TAG_SVG_title: return gf_svg_title_get_attribute(node, info);
		case TAG_SVG_tspan: return gf_svg_tspan_get_attribute(node, info);
		case TAG_SVG_use: return gf_svg_use_get_attribute(node, info);
		case TAG_SVG_video: return gf_svg_video_get_attribute(node, info);
		default: return GF_BAD_PARAM;
	}
}

u32 gf_node_svg_type_by_class_name(const char *element_name)
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

const char *gf_svg_get_element_name(u32 tag)
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


