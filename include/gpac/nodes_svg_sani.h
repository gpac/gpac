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

#ifndef _GF_SVG2_NODES_H
#define _GF_SVG2_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_svg.h>


/* Definition of SVG 2 Alternate element internal tags */
/* TAG names are made of "TAG_SVG2" + SVG element name (with - replaced by _) */
enum {
	TAG_SVG2_a = GF_NODE_RANGE_FIRST_SVG2,
	TAG_SVG2_animate,
	TAG_SVG2_animateColor,
	TAG_SVG2_animateMotion,
	TAG_SVG2_animateTransform,
	TAG_SVG2_animation,
	TAG_SVG2_audio,
	TAG_SVG2_circle,
	TAG_SVG2_conditional,
	TAG_SVG2_cursorManager,
	TAG_SVG2_defs,
	TAG_SVG2_desc,
	TAG_SVG2_discard,
	TAG_SVG2_ellipse,
	TAG_SVG2_font,
	TAG_SVG2_font_face,
	TAG_SVG2_font_face_src,
	TAG_SVG2_font_face_uri,
	TAG_SVG2_foreignObject,
	TAG_SVG2_g,
	TAG_SVG2_glyph,
	TAG_SVG2_handler,
	TAG_SVG2_hkern,
	TAG_SVG2_image,
	TAG_SVG2_line,
	TAG_SVG2_linearGradient,
	TAG_SVG2_listener,
	TAG_SVG2_metadata,
	TAG_SVG2_missing_glyph,
	TAG_SVG2_mpath,
	TAG_SVG2_path,
	TAG_SVG2_polygon,
	TAG_SVG2_polyline,
	TAG_SVG2_prefetch,
	TAG_SVG2_radialGradient,
	TAG_SVG2_rect,
	TAG_SVG2_rectClip,
	TAG_SVG2_script,
	TAG_SVG2_selector,
	TAG_SVG2_set,
	TAG_SVG2_simpleLayout,
	TAG_SVG2_solidColor,
	TAG_SVG2_stop,
	TAG_SVG2_svg,
	TAG_SVG2_switch,
	TAG_SVG2_tbreak,
	TAG_SVG2_text,
	TAG_SVG2_textArea,
	TAG_SVG2_title,
	TAG_SVG2_tspan,
	TAG_SVG2_use,
	TAG_SVG2_video,
	/*undefined elements (when parsing) use this tag*/
	TAG_SVG2_UndefinedElement
};

/******************************************
*   SVG2 Elements structure definitions    *
*******************************************/
typedef struct _tagSVG2aElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	SVG_PointerEvents pointer_events;
	SVG_ID target;
} SVG2aElement;


typedef struct _tagSVG2animateElement
{
	BASE_SVG2_ELEMENT
} SVG2animateElement;


typedef struct _tagSVG2animateColorElement
{
	BASE_SVG2_ELEMENT
} SVG2animateColorElement;


typedef struct _tagSVG2animateMotionElement
{
	BASE_SVG2_ELEMENT
	SVG_PathData path;
	SMIL_KeyPoints keyPoints;
	SVG_Rotate rotate;
	SVG_String origin;
} SVG2animateMotionElement;


typedef struct _tagSVG2animateTransformElement
{
	BASE_SVG2_ELEMENT
} SVG2animateTransformElement;


typedef struct _tagSVG2animationElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2animationElement;


typedef struct _tagSVG2audioElement
{
	BASE_SVG2_ELEMENT
	SVG_ContentType type;
	SVG_Number audio_level;
	SVG_Display display;
} SVG2audioElement;


typedef struct _tagSVG2circleElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2circleElement;


typedef struct _tagSVG2conditionalElement
{
	BASE_SVG2_ELEMENT
	SVGCommandBuffer updates;
	SVG_Boolean enabled;
} SVG2conditionalElement;


typedef struct _tagSVG2cursorManagerElement
{
	BASE_SVG2_ELEMENT
	SVG_Length x;
	SVG_Length y;
} SVG2cursorManagerElement;


typedef struct _tagSVG2defsElement
{
	BASE_SVG2_ELEMENT
} SVG2defsElement;


typedef struct _tagSVG2descElement
{
	BASE_SVG2_ELEMENT
} SVG2descElement;


typedef struct _tagSVG2discardElement
{
	BASE_SVG2_ELEMENT
} SVG2discardElement;


typedef struct _tagSVG2ellipseElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2ellipseElement;


typedef struct _tagSVG2fontElement
{
	BASE_SVG2_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_Number horiz_origin_x;
} SVG2fontElement;


typedef struct _tagSVG2font_faceElement
{
	BASE_SVG2_ELEMENT
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
} SVG2font_faceElement;


typedef struct _tagSVG2font_face_srcElement
{
	BASE_SVG2_ELEMENT
} SVG2font_face_srcElement;


typedef struct _tagSVG2font_face_uriElement
{
	BASE_SVG2_ELEMENT
} SVG2font_face_uriElement;


typedef struct _tagSVG2foreignObjectElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_Display display;
	SVG_Visibility visibility;
} SVG2foreignObjectElement;


typedef struct _tagSVG2gElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
} SVG2gElement;


typedef struct _tagSVG2glyphElement
{
	BASE_SVG2_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
	SVG_String unicode;
	SVG_String glyph_name;
	SVG_String arabic_form;
	SVG_LanguageIDs lang;
} SVG2glyphElement;


typedef struct _tagSVG2handlerElement
{
	BASE_SVG2_ELEMENT
	SVG_ContentType type;
	XMLEV_Event ev_event;
	void (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);
} SVG2handlerElement;


typedef struct _tagSVG2hkernElement
{
	BASE_SVG2_ELEMENT
	SVG_String u1;
	SVG_String g1;
	SVG_String u2;
	SVG_String g2;
	SVG_Number k;
} SVG2hkernElement;


typedef struct _tagSVG2imageElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2imageElement;


typedef struct _tagSVG2lineElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2lineElement;


typedef struct _tagSVG2linearGradientElement
{
	BASE_SVG2_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVG2linearGradientElement;


typedef struct _tagSVG2listenerElement
{
	BASE_SVG2_ELEMENT
	XMLEV_Event event;
	XMLEV_Phase phase;
	XMLEV_Propagate propagate;
	XMLEV_DefaultAction defaultAction;
	SVG_IRI observer;
	SVG_IRI target;
	SVG_IRI handler;
	SVG_Boolean enabled;
} SVG2listenerElement;


typedef struct _tagSVG2metadataElement
{
	BASE_SVG2_ELEMENT
} SVG2metadataElement;


typedef struct _tagSVG2missing_glyphElement
{
	BASE_SVG2_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
} SVG2missing_glyphElement;


typedef struct _tagSVG2mpathElement
{
	BASE_SVG2_ELEMENT
} SVG2mpathElement;


typedef struct _tagSVG2pathElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2pathElement;


typedef struct _tagSVG2polygonElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2polygonElement;


typedef struct _tagSVG2polylineElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2polylineElement;


typedef struct _tagSVG2prefetchElement
{
	BASE_SVG2_ELEMENT
	SVG_Number mediaSize;
	SVG_String mediaTime;
	SVG_String mediaCharacterEncoding;
	SVG_String mediaContentEncodings;
	SVG_Number bandwidth;
} SVG2prefetchElement;


typedef struct _tagSVG2radialGradientElement
{
	BASE_SVG2_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
	SVG_Coordinate fx;
	SVG_Coordinate fy;
} SVG2radialGradientElement;


typedef struct _tagSVG2rectElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2rectElement;


typedef struct _tagSVG2rectClipElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	LASeR_Size size;
} SVG2rectClipElement;


typedef struct _tagSVG2scriptElement
{
	BASE_SVG2_ELEMENT
	SVG_ContentType type;
} SVG2scriptElement;


typedef struct _tagSVG2selectorElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	LASeR_Choice choice;
} SVG2selectorElement;


typedef struct _tagSVG2setElement
{
	BASE_SVG2_ELEMENT
} SVG2setElement;


typedef struct _tagSVG2simpleLayoutElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
	LASeR_Size delta;
} SVG2simpleLayoutElement;


typedef struct _tagSVG2solidColorElement
{
	BASE_SVG2_ELEMENT
	SVG_SVGColor solid_color;
	SVG_Number solid_opacity;
} SVG2solidColorElement;


typedef struct _tagSVG2stopElement
{
	BASE_SVG2_ELEMENT
	SVG_RenderingHint color_rendering;
	SVG_SVGColor stop_color;
	SVG_Number stop_opacity;
	SVG_Number offset;
} SVG2stopElement;


typedef struct _tagSVG2svgElement
{
	BASE_SVG2_ELEMENT
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
} SVG2svgElement;


typedef struct _tagSVG2switchElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Display display;
	SVG_Visibility visibility;
} SVG2switchElement;


typedef struct _tagSVG2tbreakElement
{
	BASE_SVG2_ELEMENT
} SVG2tbreakElement;


typedef struct _tagSVG2textElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2textElement;


typedef struct _tagSVG2textAreaElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2textAreaElement;


typedef struct _tagSVG2titleElement
{
	BASE_SVG2_ELEMENT
} SVG2titleElement;


typedef struct _tagSVG2tspanElement
{
	BASE_SVG2_ELEMENT
} SVG2tspanElement;


typedef struct _tagSVG2useElement
{
	TRANSFORMABLE_SVG2_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Display display;
	SVG_Visibility visibility;
} SVG2useElement;


typedef struct _tagSVG2videoElement
{
	TRANSFORMABLE_SVG2_ELEMENT
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
} SVG2videoElement;


#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG2_NODES_H*/

