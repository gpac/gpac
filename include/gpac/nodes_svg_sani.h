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
	DO NOT MOFIFY - File generated on GMT Mon Apr 23 13:10:06 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/

#ifndef _GF_SVG_SANI_NODES_H
#define _GF_SVG_SANI_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_svg.h>

#ifdef GPAC_ENABLE_SVG_SANI

/* Definition of SVG 2 Alternate element internal tags */
/* TAG names are made of "TAG_SVG_SANI_" + SVG element name (with - replaced by _) */
enum {
	TAG_SVG_SANI_a = GF_NODE_RANGE_FIRST_SVG_SANI,
	TAG_SVG_SANI_animate,
	TAG_SVG_SANI_animateColor,
	TAG_SVG_SANI_animateMotion,
	TAG_SVG_SANI_animateTransform,
	TAG_SVG_SANI_animation,
	TAG_SVG_SANI_audio,
	TAG_SVG_SANI_circle,
	TAG_SVG_SANI_conditional,
	TAG_SVG_SANI_cursorManager,
	TAG_SVG_SANI_defs,
	TAG_SVG_SANI_desc,
	TAG_SVG_SANI_discard,
	TAG_SVG_SANI_ellipse,
	TAG_SVG_SANI_font,
	TAG_SVG_SANI_font_face,
	TAG_SVG_SANI_font_face_src,
	TAG_SVG_SANI_font_face_uri,
	TAG_SVG_SANI_foreignObject,
	TAG_SVG_SANI_g,
	TAG_SVG_SANI_glyph,
	TAG_SVG_SANI_handler,
	TAG_SVG_SANI_hkern,
	TAG_SVG_SANI_image,
	TAG_SVG_SANI_line,
	TAG_SVG_SANI_linearGradient,
	TAG_SVG_SANI_listener,
	TAG_SVG_SANI_metadata,
	TAG_SVG_SANI_missing_glyph,
	TAG_SVG_SANI_mpath,
	TAG_SVG_SANI_path,
	TAG_SVG_SANI_polygon,
	TAG_SVG_SANI_polyline,
	TAG_SVG_SANI_prefetch,
	TAG_SVG_SANI_radialGradient,
	TAG_SVG_SANI_rect,
	TAG_SVG_SANI_rectClip,
	TAG_SVG_SANI_script,
	TAG_SVG_SANI_selector,
	TAG_SVG_SANI_set,
	TAG_SVG_SANI_simpleLayout,
	TAG_SVG_SANI_solidColor,
	TAG_SVG_SANI_stop,
	TAG_SVG_SANI_svg,
	TAG_SVG_SANI_switch,
	TAG_SVG_SANI_tbreak,
	TAG_SVG_SANI_text,
	TAG_SVG_SANI_textArea,
	TAG_SVG_SANI_title,
	TAG_SVG_SANI_tspan,
	TAG_SVG_SANI_use,
	TAG_SVG_SANI_video,
	/*undefined elements (when parsing) use this tag*/
	TAG_SVG_SANI_UndefinedElement
};

/******************************************
*   SVG_SANI_ Elements structure definitions    *
*******************************************/
typedef struct _tagSVG_SANI_aElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_ID target;
} SVG_SANI_aElement;


typedef struct _tagSVG_SANI_animateElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_animateElement;


typedef struct _tagSVG_SANI_animateColorElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_animateColorElement;


typedef struct _tagSVG_SANI_animateMotionElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_PathData path;
	SMIL_KeyPoints keyPoints;
	SVG_Rotate rotate;
	SVG_String origin;
} SVG_SANI_animateMotionElement;


typedef struct _tagSVG_SANI_animateTransformElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_animateTransformElement;


typedef struct _tagSVG_SANI_animationElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_InitialVisibility initialVisibility;
	SVG_Number audio_level;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_Paint viewport_fill;
	SVG_Number viewport_fill_opacity;
} SVG_SANI_animationElement;


typedef struct _tagSVG_SANI_audioElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_ContentType type;
	SVG_Number audio_level;
	SVG_Display display;
} SVG_SANI_audioElement;


typedef struct _tagSVG_SANI_circleElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
} SVG_SANI_circleElement;


typedef struct _tagSVG_SANI_conditionalElement
{
	BASE_SVG_SANI_ELEMENT
	SVGCommandBuffer updates;
	SVG_Boolean enabled;
} SVG_SANI_conditionalElement;


typedef struct _tagSVG_SANI_cursorManagerElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Length x;
	SVG_Length y;
} SVG_SANI_cursorManagerElement;


typedef struct _tagSVG_SANI_defsElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_defsElement;


typedef struct _tagSVG_SANI_descElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_descElement;


typedef struct _tagSVG_SANI_discardElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_discardElement;


typedef struct _tagSVG_SANI_ellipseElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Length rx;
	SVG_Length ry;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
} SVG_SANI_ellipseElement;


typedef struct _tagSVG_SANI_fontElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_Number horiz_origin_x;
} SVG_SANI_fontElement;


typedef struct _tagSVG_SANI_font_faceElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_FontFamily font_family;
	SVG_FontStyle font_style;
	SVG_FontWeight font_weight;
	SVG_FontVariant font_variant;
	SVG_String font_stretch;
	SVG_String unicode_range;
	SVG_String panose_1;
	SVG_String widths;
	SVG_String bbox;
	SVG_Number units_per_em;
	SVG_Number stemv;
	SVG_Number stemh;
	SVG_Number slope;
	SVG_Number cap_height;
	SVG_Number x_height;
	SVG_Number accent_height;
	SVG_Number ascent;
	SVG_Number descent;
	SVG_Number ideographic;
	SVG_Number alphabetic;
	SVG_Number mathematical;
	SVG_Number hanging;
	SVG_Number underline_position;
	SVG_Number underline_thickness;
	SVG_Number strikethrough_position;
	SVG_Number strikethrough_thickness;
	SVG_Number overline_position;
	SVG_Number overline_thickness;
} SVG_SANI_font_faceElement;


typedef struct _tagSVG_SANI_font_face_srcElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_font_face_srcElement;


typedef struct _tagSVG_SANI_font_face_uriElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_font_face_uriElement;


typedef struct _tagSVG_SANI_foreignObjectElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_Display display;
	SVG_Visibility visibility;
} SVG_SANI_foreignObjectElement;


typedef struct _tagSVG_SANI_gElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
} SVG_SANI_gElement;


typedef struct _tagSVG_SANI_glyphElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
	SVG_String unicode;
	SVG_String glyph_name;
	SVG_String arabic_form;
	SVG_LanguageIDs lang;
} SVG_SANI_glyphElement;


typedef struct _tagSVG_SANI_handlerElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_ContentType type;
	XMLEV_Event ev_event;
	void (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);
} SVG_SANI_handlerElement;


typedef struct _tagSVG_SANI_hkernElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_String u1;
	SVG_String g1;
	SVG_String u2;
	SVG_String g2;
	SVG_Number k;
} SVG_SANI_hkernElement;


typedef struct _tagSVG_SANI_imageElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_ContentType type;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_RenderingHint image_rendering;
	SVG_PointerEvents pointer_events;
} SVG_SANI_imageElement;


typedef struct _tagSVG_SANI_lineElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVG_SANI_lineElement;


typedef struct _tagSVG_SANI_linearGradientElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVG_SANI_linearGradientElement;


typedef struct _tagSVG_SANI_listenerElement
{
	BASE_SVG_SANI_ELEMENT
	XMLEV_Event event;
	XMLEV_Phase phase;
	XMLEV_Propagate propagate;
	XMLEV_DefaultAction defaultAction;
	SVG_IRI observer;
	SVG_IRI target;
	SVG_IRI handler;
	SVG_Boolean enabled;
} SVG_SANI_listenerElement;


typedef struct _tagSVG_SANI_metadataElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_metadataElement;


typedef struct _tagSVG_SANI_missing_glyphElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
} SVG_SANI_missing_glyphElement;


typedef struct _tagSVG_SANI_mpathElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_mpathElement;


typedef struct _tagSVG_SANI_pathElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_PathData d;
	SVG_Number pathLength;
} SVG_SANI_pathElement;


typedef struct _tagSVG_SANI_polygonElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Points points;
} SVG_SANI_polygonElement;


typedef struct _tagSVG_SANI_polylineElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Points points;
} SVG_SANI_polylineElement;


typedef struct _tagSVG_SANI_prefetchElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Number mediaSize;
	SVG_String mediaTime;
	SVG_String mediaCharacterEncoding;
	SVG_String mediaContentEncodings;
	SVG_Number bandwidth;
} SVG_SANI_prefetchElement;


typedef struct _tagSVG_SANI_radialGradientElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
	SVG_Coordinate fx;
	SVG_Coordinate fy;
} SVG_SANI_radialGradientElement;


typedef struct _tagSVG_SANI_rectElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_VectorEffect vector_effect;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_Length rx;
	SVG_Length ry;
} SVG_SANI_rectElement;


typedef struct _tagSVG_SANI_rectClipElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	LASeR_Size size;
} SVG_SANI_rectClipElement;


typedef struct _tagSVG_SANI_scriptElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_ContentType type;
} SVG_SANI_scriptElement;


typedef struct _tagSVG_SANI_selectorElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	LASeR_Choice choice;
} SVG_SANI_selectorElement;


typedef struct _tagSVG_SANI_setElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_setElement;


typedef struct _tagSVG_SANI_simpleLayoutElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	LASeR_Size delta;
} SVG_SANI_simpleLayoutElement;


typedef struct _tagSVG_SANI_solidColorElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_SVGColor solid_color;
	SVG_Number solid_opacity;
} SVG_SANI_solidColorElement;


typedef struct _tagSVG_SANI_stopElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_RenderingHint color_rendering;
	SVG_SVGColor stop_color;
	SVG_Number stop_opacity;
	SVG_Number offset;
} SVG_SANI_stopElement;


typedef struct _tagSVG_SANI_svgElement
{
	BASE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_Paint viewport_fill;
	SVG_Number viewport_fill_opacity;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_ViewBox viewBox;
	SVG_ZoomAndPan zoomAndPan;
	SVG_String version;
	SVG_String baseProfile;
	SVG_ContentType contentScriptType;
	SVG_Clock snapshotTime;
	SVG_TimelineBegin timelineBegin;
	SVG_PlaybackOrder playbackOrder;
} SVG_SANI_svgElement;


typedef struct _tagSVG_SANI_switchElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
} SVG_SANI_switchElement;


typedef struct _tagSVG_SANI_tbreakElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_tbreakElement;


typedef struct _tagSVG_SANI_textElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Boolean editable;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_FontFamily font_family;
	SVG_FontSize font_size;
	SVG_FontStyle font_style;
	SVG_FontVariant font_variant;
	SVG_FontWeight font_weight;
	SVG_TextAnchor text_anchor;
	SVG_TextAlign text_align;
	SVG_VectorEffect vector_effect;
	SVG_PointerEvents pointer_events;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_Coordinates x;
	SVG_Coordinates y;
	SVG_Numbers rotate;
} SVG_SANI_textElement;


typedef struct _tagSVG_SANI_textAreaElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Boolean editable;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_RenderingHint text_rendering;
	SVG_Number fill_opacity;
	SVG_Number stroke_opacity;
	SVG_Paint fill;
	SVG_FillRule fill_rule;
	SVG_Paint stroke;
	SVG_StrokeDashArray stroke_dasharray;
	SVG_Length stroke_dashoffset;
	SVG_StrokeLineCap stroke_linecap;
	SVG_StrokeLineJoin stroke_linejoin;
	SVG_Number stroke_miterlimit;
	SVG_Length stroke_width;
	SVG_RenderingHint color_rendering;
	SVG_FontFamily font_family;
	SVG_FontSize font_size;
	SVG_FontStyle font_style;
	SVG_FontVariant font_variant;
	SVG_FontWeight font_weight;
	SVG_TextAnchor text_anchor;
	SVG_TextAlign text_align;
	SVG_DisplayAlign display_align;
	SVG_Number line_increment;
	SVG_VectorEffect vector_effect;
	SVG_Length width;
	SVG_Length height;
} SVG_SANI_textAreaElement;


typedef struct _tagSVG_SANI_titleElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_titleElement;


typedef struct _tagSVG_SANI_tspanElement
{
	BASE_SVG_SANI_ELEMENT
} SVG_SANI_tspanElement;


typedef struct _tagSVG_SANI_useElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Display display;
	SVG_Visibility visibility;
} SVG_SANI_useElement;


typedef struct _tagSVG_SANI_videoElement
{
	TRANSFORMABLE_SVG_SANI_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_ContentType type;
	SVG_InitialVisibility initialVisibility;
	SVG_Number audio_level;
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_TransformBehavior transformBehavior;
	SVG_Overlay overlay;
} SVG_SANI_videoElement;


#endif	/*GPAC_ENABLE_SVG_SANI*/

#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG_SANI_NODES_H*/

