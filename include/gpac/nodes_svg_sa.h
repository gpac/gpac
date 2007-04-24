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
	DO NOT MOFIFY - File generated on GMT Mon Apr 23 13:13:53 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/

#ifndef _GF_SVG_SA_NODES_H
#define _GF_SVG_SA_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_svg.h>

#ifdef GPAC_ENABLE_SVG_SA

/* Definition of SVG element internal tags */
/* TAG names are made of "TAG_SVG" + SVG element name (with - replaced by _) */
enum {
	TAG_SVG_SA_a = GF_NODE_RANGE_FIRST_SVG_SA,
	TAG_SVG_SA_animate,
	TAG_SVG_SA_animateColor,
	TAG_SVG_SA_animateMotion,
	TAG_SVG_SA_animateTransform,
	TAG_SVG_SA_animation,
	TAG_SVG_SA_audio,
	TAG_SVG_SA_circle,
	TAG_SVG_SA_conditional,
	TAG_SVG_SA_cursorManager,
	TAG_SVG_SA_defs,
	TAG_SVG_SA_desc,
	TAG_SVG_SA_discard,
	TAG_SVG_SA_ellipse,
	TAG_SVG_SA_font,
	TAG_SVG_SA_font_face,
	TAG_SVG_SA_font_face_src,
	TAG_SVG_SA_font_face_uri,
	TAG_SVG_SA_foreignObject,
	TAG_SVG_SA_g,
	TAG_SVG_SA_glyph,
	TAG_SVG_SA_handler,
	TAG_SVG_SA_hkern,
	TAG_SVG_SA_image,
	TAG_SVG_SA_line,
	TAG_SVG_SA_linearGradient,
	TAG_SVG_SA_listener,
	TAG_SVG_SA_metadata,
	TAG_SVG_SA_missing_glyph,
	TAG_SVG_SA_mpath,
	TAG_SVG_SA_path,
	TAG_SVG_SA_polygon,
	TAG_SVG_SA_polyline,
	TAG_SVG_SA_prefetch,
	TAG_SVG_SA_radialGradient,
	TAG_SVG_SA_rect,
	TAG_SVG_SA_rectClip,
	TAG_SVG_SA_script,
	TAG_SVG_SA_selector,
	TAG_SVG_SA_set,
	TAG_SVG_SA_simpleLayout,
	TAG_SVG_SA_solidColor,
	TAG_SVG_SA_stop,
	TAG_SVG_SA_svg,
	TAG_SVG_SA_switch,
	TAG_SVG_SA_tbreak,
	TAG_SVG_SA_text,
	TAG_SVG_SA_textArea,
	TAG_SVG_SA_title,
	TAG_SVG_SA_tspan,
	TAG_SVG_SA_use,
	TAG_SVG_SA_video,
	/*undefined elements (when parsing) use this tag*/
	TAG_SVG_SA_UndefinedElement
};

/******************************************
*   SVG Elements structure definitions    *
*******************************************/
typedef struct _tagSVG_SA_aElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_ID target;
} SVG_SA_aElement;


typedef struct _tagSVG_SA_animateElement
{
	BASE_SVG_ELEMENT
} SVG_SA_animateElement;


typedef struct _tagSVG_SA_animateColorElement
{
	BASE_SVG_ELEMENT
} SVG_SA_animateColorElement;


typedef struct _tagSVG_SA_animateMotionElement
{
	BASE_SVG_ELEMENT
	SVG_PathData path;
	SMIL_KeyPoints keyPoints;
	SVG_Rotate rotate;
	SVG_String origin;
} SVG_SA_animateMotionElement;


typedef struct _tagSVG_SA_animateTransformElement
{
	BASE_SVG_ELEMENT
} SVG_SA_animateTransformElement;


typedef struct _tagSVG_SA_animationElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_InitialVisibility initialVisibility;
} SVG_SA_animationElement;


typedef struct _tagSVG_SA_audioElement
{
	BASE_SVG_ELEMENT
	SVG_ContentType type;
} SVG_SA_audioElement;


typedef struct _tagSVG_SA_circleElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
} SVG_SA_circleElement;


typedef struct _tagSVG_SA_conditionalElement
{
	BASE_SVG_ELEMENT
	SVGCommandBuffer updates;
	SVG_Boolean enabled;
} SVG_SA_conditionalElement;


typedef struct _tagSVG_SA_cursorManagerElement
{
	BASE_SVG_ELEMENT
	SVG_Length x;
	SVG_Length y;
} SVG_SA_cursorManagerElement;


typedef struct _tagSVG_SA_defsElement
{
	BASE_SVG_ELEMENT
} SVG_SA_defsElement;


typedef struct _tagSVG_SA_descElement
{
	BASE_SVG_ELEMENT
} SVG_SA_descElement;


typedef struct _tagSVG_SA_discardElement
{
	BASE_SVG_ELEMENT
} SVG_SA_discardElement;


typedef struct _tagSVG_SA_ellipseElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Length rx;
	SVG_Length ry;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
} SVG_SA_ellipseElement;


typedef struct _tagSVG_SA_fontElement
{
	BASE_SVG_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_Number horiz_origin_x;
} SVG_SA_fontElement;


typedef struct _tagSVG_SA_font_faceElement
{
	BASE_SVG_ELEMENT
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
} SVG_SA_font_faceElement;


typedef struct _tagSVG_SA_font_face_srcElement
{
	BASE_SVG_ELEMENT
} SVG_SA_font_face_srcElement;


typedef struct _tagSVG_SA_font_face_uriElement
{
	BASE_SVG_ELEMENT
} SVG_SA_font_face_uriElement;


typedef struct _tagSVG_SA_foreignObjectElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
} SVG_SA_foreignObjectElement;


typedef struct _tagSVG_SA_gElement
{
	TRANSFORMABLE_SVG_ELEMENT
} SVG_SA_gElement;


typedef struct _tagSVG_SA_glyphElement
{
	BASE_SVG_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
	SVG_String unicode;
	SVG_String glyph_name;
	SVG_String arabic_form;
	SVG_LanguageIDs lang;
} SVG_SA_glyphElement;


typedef struct _tagSVG_SA_handlerElement
{
	BASE_SVG_ELEMENT
	SVG_ContentType type;
	XMLEV_Event ev_event;
	void (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);
} SVG_SA_handlerElement;


typedef struct _tagSVG_SA_hkernElement
{
	BASE_SVG_ELEMENT
	SVG_String u1;
	SVG_String g1;
	SVG_String u2;
	SVG_String g2;
	SVG_Number k;
} SVG_SA_hkernElement;


typedef struct _tagSVG_SA_imageElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_ContentType type;
} SVG_SA_imageElement;


typedef struct _tagSVG_SA_lineElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVG_SA_lineElement;


typedef struct _tagSVG_SA_linearGradientElement
{
	BASE_SVG_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVG_SA_linearGradientElement;


typedef struct _tagSVG_SA_listenerElement
{
	BASE_SVG_ELEMENT
	XMLEV_Event event;
	XMLEV_Phase phase;
	XMLEV_Propagate propagate;
	XMLEV_DefaultAction defaultAction;
	SVG_IRI observer;
	SVG_IRI target;
	SVG_IRI handler;
	SVG_Boolean enabled;
} SVG_SA_listenerElement;


typedef struct _tagSVG_SA_metadataElement
{
	BASE_SVG_ELEMENT
} SVG_SA_metadataElement;


typedef struct _tagSVG_SA_missing_glyphElement
{
	BASE_SVG_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
} SVG_SA_missing_glyphElement;


typedef struct _tagSVG_SA_mpathElement
{
	BASE_SVG_ELEMENT
} SVG_SA_mpathElement;


typedef struct _tagSVG_SA_pathElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_PathData d;
	SVG_Number pathLength;
} SVG_SA_pathElement;


typedef struct _tagSVG_SA_polygonElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Points points;
} SVG_SA_polygonElement;


typedef struct _tagSVG_SA_polylineElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Points points;
} SVG_SA_polylineElement;


typedef struct _tagSVG_SA_prefetchElement
{
	BASE_SVG_ELEMENT
	SVG_Number mediaSize;
	SVG_String mediaTime;
	SVG_String mediaCharacterEncoding;
	SVG_String mediaContentEncodings;
	SVG_Number bandwidth;
} SVG_SA_prefetchElement;


typedef struct _tagSVG_SA_radialGradientElement
{
	BASE_SVG_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Transform gradientTransform;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
	SVG_Coordinate fx;
	SVG_Coordinate fy;
} SVG_SA_radialGradientElement;


typedef struct _tagSVG_SA_rectElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_Length rx;
	SVG_Length ry;
} SVG_SA_rectElement;


typedef struct _tagSVG_SA_rectClipElement
{
	TRANSFORMABLE_SVG_ELEMENT
	LASeR_Size size;
} SVG_SA_rectClipElement;


typedef struct _tagSVG_SA_scriptElement
{
	BASE_SVG_ELEMENT
	SVG_ContentType type;
} SVG_SA_scriptElement;


typedef struct _tagSVG_SA_selectorElement
{
	TRANSFORMABLE_SVG_ELEMENT
	LASeR_Choice choice;
} SVG_SA_selectorElement;


typedef struct _tagSVG_SA_setElement
{
	BASE_SVG_ELEMENT
} SVG_SA_setElement;


typedef struct _tagSVG_SA_simpleLayoutElement
{
	TRANSFORMABLE_SVG_ELEMENT
	LASeR_Size delta;
} SVG_SA_simpleLayoutElement;


typedef struct _tagSVG_SA_solidColorElement
{
	BASE_SVG_ELEMENT
} SVG_SA_solidColorElement;


typedef struct _tagSVG_SA_stopElement
{
	BASE_SVG_ELEMENT
	SVG_Number offset;
} SVG_SA_stopElement;


typedef struct _tagSVG_SA_svgElement
{
	BASE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_ViewBox viewBox;
	SVG_ZoomAndPan zoomAndPan;
	SVG_String version;
	SVG_String baseProfile;
	SVG_ContentType contentScriptType;
	SVG_Clock snapshotTime;
	SVG_TimelineBegin timelineBegin;
	SVG_PlaybackOrder playbackOrder;
} SVG_SA_svgElement;


typedef struct _tagSVG_SA_switchElement
{
	TRANSFORMABLE_SVG_ELEMENT
} SVG_SA_switchElement;


typedef struct _tagSVG_SA_tbreakElement
{
	BASE_SVG_ELEMENT
} SVG_SA_tbreakElement;


typedef struct _tagSVG_SA_textElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Boolean editable;
	SVG_Coordinates x;
	SVG_Coordinates y;
	SVG_Numbers rotate;
} SVG_SA_textElement;


typedef struct _tagSVG_SA_textAreaElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Boolean editable;
	SVG_Length width;
	SVG_Length height;
} SVG_SA_textAreaElement;


typedef struct _tagSVG_SA_titleElement
{
	BASE_SVG_ELEMENT
} SVG_SA_titleElement;


typedef struct _tagSVG_SA_tspanElement
{
	BASE_SVG_ELEMENT
} SVG_SA_tspanElement;


typedef struct _tagSVG_SA_useElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
} SVG_SA_useElement;


typedef struct _tagSVG_SA_videoElement
{
	TRANSFORMABLE_SVG_ELEMENT
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_ContentType type;
	SVG_InitialVisibility initialVisibility;
	SVG_TransformBehavior transformBehavior;
	SVG_Overlay overlay;
} SVG_SA_videoElement;


/******************************************
*  End SVG Elements structure definitions *
*******************************************/

#endif	/*GPAC_ENABLE_SVG_SA*/

#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG_SA_NODES_H*/

