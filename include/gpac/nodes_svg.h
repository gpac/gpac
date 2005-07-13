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
	DO NOT MOFIFY - File generated on GMT Wed Jul 13 11:50:18 2005

	BY SVGGen for GPAC Version 0.4.0
*/

#ifndef _GF_SVG_NODES_H
#define _GF_SVG_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_svg.h>


/* Definition of SVG element internal tags */
/* TAG names are made of "TAG_SVG" + SVG element name (with - replaced by _) */
enum {
	TAG_SVG_a = GF_NODE_RANGE_FIRST_SVG,
	TAG_SVG_animate,
	TAG_SVG_animateColor,
	TAG_SVG_animateMotion,
	TAG_SVG_animateTransform,
	TAG_SVG_animation,
	TAG_SVG_audio,
	TAG_SVG_circle,
	TAG_SVG_defs,
	TAG_SVG_desc,
	TAG_SVG_discard,
	TAG_SVG_ellipse,
	TAG_SVG_font,
	TAG_SVG_font_face,
	TAG_SVG_font_face_name,
	TAG_SVG_font_face_src,
	TAG_SVG_font_face_uri,
	TAG_SVG_foreignObject,
	TAG_SVG_g,
	TAG_SVG_glyph,
	TAG_SVG_handler,
	TAG_SVG_hkern,
	TAG_SVG_image,
	TAG_SVG_line,
	TAG_SVG_linearGradient,
	TAG_SVG_metadata,
	TAG_SVG_missing_glyph,
	TAG_SVG_mpath,
	TAG_SVG_path,
	TAG_SVG_polygon,
	TAG_SVG_polyline,
	TAG_SVG_prefetch,
	TAG_SVG_radialGradient,
	TAG_SVG_rect,
	TAG_SVG_script,
	TAG_SVG_set,
	TAG_SVG_solidColor,
	TAG_SVG_stop,
	TAG_SVG_svg,
	TAG_SVG_switch,
	TAG_SVG_tBreak,
	TAG_SVG_text,
	TAG_SVG_textArea,
	TAG_SVG_title,
	TAG_SVG_tspan,
	TAG_SVG_use,
	TAG_SVG_video,
	/*undefined elements (when parsing) use this tag*/
	TAG_SVG_UndefinedElement
};

/******************************************
*   SVG Elements structure definitions    *
*******************************************/
typedef struct _tagSVGaElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_LinkTarget target; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGaElement;


typedef struct _tagSVGanimateElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SMIL_AttributeName attributeName; /* required, animatable: no, inheritable: false */
	SVG_String attributeType; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue min; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue max; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue to; /* optional, animatable: no, inheritable: false */
	SMIL_CalcModeValue calcMode; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValues values; /* optional, animatable: no, inheritable: false */
	SMIL_KeyTimesValues keyTimes; /* optional, animatable: no, inheritable: false */
	SMIL_KeySplinesValues keySplines; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue from; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue by; /* optional, animatable: no, inheritable: false */
	SMIL_AdditiveValue additive; /* optional, animatable: no, inheritable: false */
	SMIL_AccumulateValue accumulate; /* optional, animatable: no, inheritable: false */
} SVGanimateElement;


typedef struct _tagSVGanimateColorElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SMIL_AttributeName attributeName; /* required, animatable: no, inheritable: false */
	SVG_String attributeType; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue min; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue max; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue to; /* optional, animatable: no, inheritable: false */
	SMIL_CalcModeValue calcMode; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValues values; /* optional, animatable: no, inheritable: false */
	SMIL_KeyTimesValues keyTimes; /* optional, animatable: no, inheritable: false */
	SMIL_KeySplinesValues keySplines; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue from; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue by; /* optional, animatable: no, inheritable: false */
	SMIL_AdditiveValue additive; /* optional, animatable: no, inheritable: false */
	SMIL_AccumulateValue accumulate; /* optional, animatable: no, inheritable: false */
} SVGanimateColorElement;


typedef struct _tagSVGanimateMotionElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue min; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue max; /* optional, animatable: no, inheritable: false */
	SMIL_AdditiveValue additive; /* optional, animatable: no, inheritable: false */
	SMIL_AccumulateValue accumulate; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue to; /* optional, animatable: no, inheritable: false */
	SMIL_CalcModeValue calcMode; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValues values; /* optional, animatable: no, inheritable: false */
	SMIL_KeyTimesValues keyTimes; /* optional, animatable: no, inheritable: false */
	SMIL_KeySplinesValues keySplines; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue from; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue by; /* optional, animatable: no, inheritable: false */
	SVG_AnimateTransformTypeValue type; /* optional, animatable: no, inheritable: false */
	SVG_String path; /* optional, animatable: no, inheritable: false */
	SVG_String keyPoints; /* optional, animatable: no, inheritable: false */
	SVG_String rotate; /* optional, animatable: no, inheritable: false */
	SVG_String origin; /* optional, animatable: no, inheritable: false */
} SVGanimateMotionElement;


typedef struct _tagSVGanimateTransformElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SMIL_AttributeName attributeName; /* required, animatable: no, inheritable: false */
	SVG_String attributeType; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue min; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue max; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue to; /* optional, animatable: no, inheritable: false */
	SMIL_CalcModeValue calcMode; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValues values; /* optional, animatable: no, inheritable: false */
	SMIL_KeyTimesValues keyTimes; /* optional, animatable: no, inheritable: false */
	SMIL_KeySplinesValues keySplines; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue from; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue by; /* optional, animatable: no, inheritable: false */
	SMIL_AdditiveValue additive; /* optional, animatable: no, inheritable: false */
	SMIL_AccumulateValue accumulate; /* optional, animatable: no, inheritable: false */
	SVG_AnimateTransformTypeValue type; /* optional, animatable: no, inheritable: false */
} SVGanimateTransformElement;


typedef struct _tagSVGanimationElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehavior; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehaviorDefault; /* optional, animatable: no, inheritable: false */
	SVG_String syncTolerance; /* optional, animatable: no, inheritable: false */
	SVG_String syncToleranceDefault; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_PreserveAspectRatioSpec preserveAspectRatio; /* optional, animatable: no, inheritable: false */
	SVG_String overflow; /* optional, animatable: no, inheritable: false */
} SVGanimationElement;


typedef struct _tagSVGaudioElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehavior; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehaviorDefault; /* optional, animatable: no, inheritable: false */
	SVG_String syncTolerance; /* optional, animatable: no, inheritable: false */
	SVG_String syncToleranceDefault; /* optional, animatable: no, inheritable: false */
	SVG_ContentType type; /* optional, animatable: no, inheritable: false */
} SVGaudioElement;


typedef struct _tagSVGcircleElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cx; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cy; /* optional, animatable: no, inheritable: false */
	SVG_Length r; /* required, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGcircleElement;


typedef struct _tagSVGdefsElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGdefsElement;


typedef struct _tagSVGdescElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
} SVGdescElement;


typedef struct _tagSVGdiscardElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
} SVGdiscardElement;


typedef struct _tagSVGellipseElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Length rx; /* optional, animatable: no, inheritable: false */
	SVG_Length ry; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cx; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cy; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGellipseElement;


typedef struct _tagSVGfontElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_Number horiz_adv_x; /* optional, animatable: no, inheritable: false */
	SVG_Number horiz_origin_x; /* optional, animatable: no, inheritable: false */
} SVGfontElement;


typedef struct _tagSVGfont_faceElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_String font_family; /* optional, animatable: no, inheritable: false */
	SVG_FontStyleValue font_style; /* optional, animatable: no, inheritable: false */
	SVG_String font_weight; /* optional, animatable: no, inheritable: false */
	SVG_FontSizeValue font_size; /* optional, animatable: no, inheritable: false */
	SVG_String font_variant; /* optional, animatable: no, inheritable: false */
	SVG_String font_stretch; /* optional, animatable: no, inheritable: false */
	SVG_String unicode_range; /* optional, animatable: no, inheritable: false */
	SVG_String panose_1; /* optional, animatable: no, inheritable: false */
	SVG_String widths; /* optional, animatable: no, inheritable: false */
	SVG_String bbox; /* optional, animatable: no, inheritable: false */
	SVG_Number units_per_em; /* optional, animatable: no, inheritable: false */
	SVG_Number stemv; /* optional, animatable: no, inheritable: false */
	SVG_Number stemh; /* optional, animatable: no, inheritable: false */
	SVG_Number slope; /* optional, animatable: no, inheritable: false */
	SVG_Number cap_height; /* optional, animatable: no, inheritable: false */
	SVG_Number x_height; /* optional, animatable: no, inheritable: false */
	SVG_Number accent_height; /* optional, animatable: no, inheritable: false */
	SVG_Number ascent; /* optional, animatable: no, inheritable: false */
	SVG_Number descent; /* optional, animatable: no, inheritable: false */
	SVG_Number ideographic; /* optional, animatable: no, inheritable: false */
	SVG_Number alphabetic; /* optional, animatable: no, inheritable: false */
	SVG_Number mathematical; /* optional, animatable: no, inheritable: false */
	SVG_Number hanging; /* optional, animatable: no, inheritable: false */
	SVG_Number underline_position; /* optional, animatable: no, inheritable: false */
	SVG_Number underline_thickness; /* optional, animatable: no, inheritable: false */
	SVG_Number strikethrough_position; /* optional, animatable: no, inheritable: false */
	SVG_Number strikethrough_thickness; /* optional, animatable: no, inheritable: false */
	SVG_Number overline_position; /* optional, animatable: no, inheritable: false */
	SVG_Number overline_thickness; /* optional, animatable: no, inheritable: false */
} SVGfont_faceElement;


typedef struct _tagSVGfont_face_nameElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String name; /* optional, animatable: no, inheritable: false */
} SVGfont_face_nameElement;


typedef struct _tagSVGfont_face_srcElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
} SVGfont_face_srcElement;


typedef struct _tagSVGfont_face_uriElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
} SVGfont_face_uriElement;


typedef struct _tagSVGforeignObjectElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGforeignObjectElement;


typedef struct _tagSVGgElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGgElement;


typedef struct _tagSVGglyphElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Number horiz_adv_x; /* optional, animatable: no, inheritable: false */
	SVG_PathData d; /* optional, animatable: no, inheritable: false */
	SVG_String unicode; /* required, animatable: no, inheritable: false */
	SVG_String glyph_name; /* optional, animatable: no, inheritable: false */
	SVG_String arabic_form; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes lang; /* optional, animatable: no, inheritable: false */
} SVGglyphElement;


typedef struct _tagSVGhandlerElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_ContentType type; /* optional, animatable: no, inheritable: false */
	SVG_XSLT_QName ev_event; /* required, animatable: no, inheritable: false */
} SVGhandlerElement;


typedef struct _tagSVGhkernElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String u1; /* optional, animatable: no, inheritable: false */
	SVG_String g1; /* optional, animatable: no, inheritable: false */
	SVG_String u2; /* optional, animatable: no, inheritable: false */
	SVG_String g2; /* optional, animatable: no, inheritable: false */
	SVG_Number k; /* required, animatable: no, inheritable: false */
} SVGhkernElement;


typedef struct _tagSVGimageElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_OpacityValue opacity; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_PreserveAspectRatioSpec preserveAspectRatio; /* optional, animatable: no, inheritable: false */
	SVG_ContentType type; /* optional, animatable: no, inheritable: false */
} SVGimageElement;


typedef struct _tagSVGlineElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x1; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y1; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x2; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y2; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGlineElement;


typedef struct _tagSVGlinearGradientElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x1; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y1; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x2; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y2; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGlinearGradientElement;


typedef struct _tagSVGmetadataElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
} SVGmetadataElement;


typedef struct _tagSVGmissing_glyphElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Number horiz_adv_x; /* optional, animatable: no, inheritable: false */
	SVG_PathData d; /* optional, animatable: no, inheritable: false */
} SVGmissing_glyphElement;


typedef struct _tagSVGmpathElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
} SVGmpathElement;


typedef struct _tagSVGpathElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_PathData d; /* optional, animatable: no, inheritable: false */
	SVG_Number pathLength; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGpathElement;


typedef struct _tagSVGpolygonElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Points points; /* required, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGpolygonElement;


typedef struct _tagSVGpolylineElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Points points; /* required, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGpolylineElement;


typedef struct _tagSVGprefetchElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_NumberOrPercentage mediaSize; /* optional, animatable: no, inheritable: false */
	SVG_String mediaTime; /* optional, animatable: no, inheritable: false */
	SVG_String mediaCharacterEncoding; /* optional, animatable: no, inheritable: false */
	SVG_String mediaContentEncodings; /* optional, animatable: no, inheritable: false */
	SVG_NumberOrPercentage bandwidth; /* optional, animatable: no, inheritable: false */
} SVGprefetchElement;


typedef struct _tagSVGradialGradientElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cx; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate cy; /* optional, animatable: no, inheritable: false */
	SVG_Length r; /* required, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGradialGradientElement;


typedef struct _tagSVGrectElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_Length rx; /* optional, animatable: no, inheritable: false */
	SVG_Length ry; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGrectElement;


typedef struct _tagSVGscriptElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_ContentType type; /* optional, animatable: no, inheritable: false */
} SVGscriptElement;


typedef struct _tagSVGsetElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SMIL_AttributeName attributeName; /* required, animatable: no, inheritable: false */
	SVG_String attributeType; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SMIL_FillValue fill; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue min; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue max; /* optional, animatable: no, inheritable: false */
	SMIL_AnimateValue to; /* optional, animatable: no, inheritable: false */
} SVGsetElement;


typedef struct _tagSVGsolidColorElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGsolidColorElement;


typedef struct _tagSVGstopElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_GradientOffset offset; /* required, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGstopElement;


typedef struct _tagSVGsvgElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehavior; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehaviorDefault; /* optional, animatable: no, inheritable: false */
	SVG_String syncTolerance; /* optional, animatable: no, inheritable: false */
	SVG_String syncToleranceDefault; /* optional, animatable: no, inheritable: false */
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_PreserveAspectRatioSpec preserveAspectRatio; /* optional, animatable: no, inheritable: false */
	SVG_ViewBoxSpec viewBox; /* optional, animatable: no, inheritable: false */
	SVG_String zoomAndPan; /* optional, animatable: no, inheritable: false */
	SVG_String version; /* optional, animatable: no, inheritable: false */
	SVG_Text baseProfile; /* optional, animatable: no, inheritable: false */
	SVG_ContentType contentScriptType; /* optional, animatable: no, inheritable: false */
	SVG_String snapshotTime; /* optional, animatable: no, inheritable: false */
	SVG_String timelineBegin; /* optional, animatable: no, inheritable: false */
	SVG_String playbackOrder; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGsvgElement;


typedef struct _tagSVGswitchElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGswitchElement;


typedef struct _tagSVGtBreakElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
} SVGtBreakElement;


typedef struct _tagSVGtextElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean editable; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_Coordinates x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinates y; /* optional, animatable: no, inheritable: false */
	SVG_Numbers rotate; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGtextElement;


typedef struct _tagSVGtextAreaElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_Boolean editable; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGtextAreaElement;


typedef struct _tagSVGtitleElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
} SVGtitleElement;


typedef struct _tagSVGtspanElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGtspanElement;


typedef struct _tagSVGuseElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_String overflow; /* optional, animatable: no, inheritable: false */
	SVG_DisplayValue display; /* animatable: yes, inheritable: false */
	SVG_VisibilityValue visibility; /* animatable: yes, inheritable: true */
	SVG_String image_rendering; /* animatable: yes, inheritable: true */
	SVG_String pointer_events; /* animatable: yes, inheritable: true */
	SVG_String shape_rendering; /* animatable: yes, inheritable: true */
	SVG_String text_rendering; /* animatable: yes, inheritable: true */
	SVG_Number audio_level; /* animatable: yes, inheritable: false */
	SVG_OpacityValue fill_opacity; /* animatable: yes, inheritable: true */
	SVG_OpacityValue stroke_opacity; /* animatable: yes, inheritable: true */
	SVG_Paint fill; /* animatable: yes, inheritable: true */
	SVG_ClipFillRule fill_rule; /* animatable: yes, inheritable: true */
	SVG_Paint stroke; /* animatable: yes, inheritable: true */
	SVG_StrokeDashArrayValue stroke_dasharray; /* animatable: yes, inheritable: true */
	SVG_StrokeDashOffsetValue stroke_dashoffset; /* animatable: yes, inheritable: true */
	SVG_StrokeLineCapValue stroke_linecap; /* animatable: yes, inheritable: true */
	SVG_StrokeLineJoinValue stroke_linejoin; /* animatable: yes, inheritable: true */
	SVG_StrokeMiterLimitValue stroke_miterlimit; /* animatable: yes, inheritable: true */
	SVG_StrokeWidthValue stroke_width; /* animatable: yes, inheritable: true */
	SVG_Color color; /* animatable: yes, inheritable: true */
	SVG_String color_rendering; /* animatable: yes, inheritable: true */
	SVG_String vector_effect; /* animatable: yes, inheritable: false */
	SVG_Paint viewport_fill; /* animatable: yes, inheritable: false */
	SVG_OpacityValue viewport_fill_opacity; /* animatable: yes, inheritable: false */
	SVG_SVGColor solid_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue solid_opacity; /* animatable: yes, inheritable: false */
	SVG_String display_align; /* animatable: yes, inheritable: true */
	SVG_Number line_increment; /* animatable: yes, inheritable: true */
	SVG_SVGColor stop_color; /* animatable: yes, inheritable: false */
	SVG_OpacityValue stop_opacity; /* animatable: yes, inheritable: false */
	SVG_FontFamilyValue font_family; /* animatable: yes, inheritable: true */
	SVG_FontSizeValue font_size; /* animatable: yes, inheritable: true */
	SVG_FontStyleValue font_style; /* animatable: yes, inheritable: true */
	SVG_String font_weight; /* animatable: yes, inheritable: true */
	SVG_TextAnchorValue text_anchor; /* animatable: yes, inheritable: true */
} SVGuseElement;


typedef struct _tagSVGvideoElement
{
	BASE_NODE
	CHILDREN
	BASE_SVG_ELEMENT
	SVG_ID id; /* optional, animatable: no, inheritable: false */
	SVG_String class_attribute; /* optional, animatable: no, inheritable: false */
	SVG_ID xml_id; /* optional, animatable: no, inheritable: false */
	SVG_IRI xml_base; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCode xml_lang; /* optional, animatable: no, inheritable: false */
	SVG_TextContent textContent; /* optional, animatable: no, inheritable: false */
	SVG_String xml_space; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_actuate; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_type; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_role; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_arcrole; /* optional, animatable: no, inheritable: false */
	SVG_String xlink_title; /* optional, animatable: no, inheritable: false */
	SVG_IRI xlink_href; /* required, animatable: no, inheritable: false */
	SVG_String xlink_show; /* optional, animatable: no, inheritable: false */
	SVG_FeatureList requiredFeatures; /* optional, animatable: no, inheritable: false */
	SVG_ExtensionList requiredExtensions; /* optional, animatable: no, inheritable: false */
	SVG_FormatList requiredFormats; /* optional, animatable: no, inheritable: false */
	SVG_FontList requiredFonts; /* optional, animatable: no, inheritable: false */
	SVG_LanguageCodes systemLanguage; /* optional, animatable: no, inheritable: false */
	SVG_Boolean externalResourcesRequired; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues begin; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue dur; /* optional, animatable: no, inheritable: false */
	SMIL_BeginOrEndValues end; /* optional, animatable: no, inheritable: false */
	SMIL_RepeatCountValue repeatCount; /* optional, animatable: no, inheritable: false */
	SMIL_MinMaxDurRepeatDurValue repeatDur; /* optional, animatable: no, inheritable: false */
	SMIL_RestartValue restart; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehavior; /* optional, animatable: no, inheritable: false */
	SVG_String syncBehaviorDefault; /* optional, animatable: no, inheritable: false */
	SVG_String syncTolerance; /* optional, animatable: no, inheritable: false */
	SVG_String syncToleranceDefault; /* optional, animatable: no, inheritable: false */
	SVG_Boolean focusable; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNext; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusPrev; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthEast; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouth; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusSouthWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusWest; /* optional, animatable: no, inheritable: false */
	SVG_Focus focusNorthWest; /* optional, animatable: no, inheritable: false */
	SVG_TransformList transform; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate x; /* optional, animatable: no, inheritable: false */
	SVG_Coordinate y; /* optional, animatable: no, inheritable: false */
	SVG_Length width; /* optional, animatable: no, inheritable: false */
	SVG_Length height; /* optional, animatable: no, inheritable: false */
	SVG_PreserveAspectRatioSpec preserveAspectRatio; /* optional, animatable: no, inheritable: false */
	SVG_ContentType type; /* optional, animatable: no, inheritable: false */
	SVG_String transformBehavior; /* optional, animatable: no, inheritable: false */
	SVG_String overlay; /* optional, animatable: no, inheritable: false */
} SVGvideoElement;


/******************************************
*  End SVG Elements structure definitions *
*******************************************/
#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG_NODES_H*/

