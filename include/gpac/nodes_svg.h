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
	DO NOT MOFIFY - File generated on GMT Thu Jun 22 15:29:41 2006

	BY SVGGen for GPAC Version 0.4.1-DEV
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
	TAG_SVG_conditional,
	TAG_SVG_cursorManager,
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
	TAG_SVG_listener,
	TAG_SVG_metadata,
	TAG_SVG_missing_glyph,
	TAG_SVG_mpath,
	TAG_SVG_path,
	TAG_SVG_polygon,
	TAG_SVG_polyline,
	TAG_SVG_prefetch,
	TAG_SVG_radialGradient,
	TAG_SVG_rect,
	TAG_SVG_rectClip,
	TAG_SVG_script,
	TAG_SVG_selector,
	TAG_SVG_set,
	TAG_SVG_simpleLayout,
	TAG_SVG_solidColor,
	TAG_SVG_stop,
	TAG_SVG_svg,
	TAG_SVG_switch,
	TAG_SVG_tbreak,
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
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_ID target;
} SVGaElement;


typedef struct _tagSVGanimateElement
{
	BASE_SVG_ELEMENT
} SVGanimateElement;


typedef struct _tagSVGanimateColorElement
{
	BASE_SVG_ELEMENT
} SVGanimateColorElement;


typedef struct _tagSVGanimateMotionElement
{
	BASE_SVG_ELEMENT
	SVG_PathData path;
	SMIL_KeyPoints keyPoints;
	SVG_Rotate rotate;
	SVG_String origin;
} SVGanimateMotionElement;


typedef struct _tagSVGanimateTransformElement
{
	BASE_SVG_ELEMENT
} SVGanimateTransformElement;


typedef struct _tagSVGanimationElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_InitialVisibility initialVisibility;
} SVGanimationElement;


typedef struct _tagSVGaudioElement
{
	BASE_SVG_ELEMENT
} SVGaudioElement;


typedef struct _tagSVGcircleElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
} SVGcircleElement;


typedef struct _tagSVGconditionalElement
{
	BASE_SVG_ELEMENT
	SVGCommandBuffer updates;
	SMIL_Times lsr_begin;
	SVG_Boolean lsr_enabled;
} SVGconditionalElement;


typedef struct _tagSVGcursorManagerElement
{
	BASE_SVG_ELEMENT
	SVG_Length x;
	SVG_Length y;
} SVGcursorManagerElement;


typedef struct _tagSVGdefsElement
{
	BASE_SVG_ELEMENT
} SVGdefsElement;


typedef struct _tagSVGdescElement
{
	BASE_SVG_ELEMENT
} SVGdescElement;


typedef struct _tagSVGdiscardElement
{
	BASE_SVG_ELEMENT
} SVGdiscardElement;


typedef struct _tagSVGellipseElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Length rx;
	SVG_Length ry;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
} SVGellipseElement;


typedef struct _tagSVGfontElement
{
	BASE_SVG_ELEMENT
	SVG_Number horiz_origin_x;
	SVG_Number horiz_adv_x;
} SVGfontElement;


typedef struct _tagSVGfont_faceElement
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
} SVGfont_faceElement;


typedef struct _tagSVGfont_face_nameElement
{
	BASE_SVG_ELEMENT
	SVG_String name;
} SVGfont_face_nameElement;


typedef struct _tagSVGfont_face_srcElement
{
	BASE_SVG_ELEMENT
} SVGfont_face_srcElement;


typedef struct _tagSVGfont_face_uriElement
{
	BASE_SVG_ELEMENT
} SVGfont_face_uriElement;


typedef struct _tagSVGforeignObjectElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
} SVGforeignObjectElement;


typedef struct _tagSVGgElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
} SVGgElement;


typedef struct _tagSVGglyphElement
{
	BASE_SVG_ELEMENT
	SVG_String unicode;
	SVG_String glyph_name;
	SVG_String arabic_form;
	SVG_LanguageIDs lang;
	SVG_Number horiz_adv_x;
	SVG_PathData d;
} SVGglyphElement;


typedef struct _tagSVGhandlerElement
{
	BASE_SVG_ELEMENT
	XMLEV_Event ev_event;
	void (*handle_event)(struct _tagSVGhandlerElement *hdl, GF_DOM_Event *event);
} SVGhandlerElement;


typedef struct _tagSVGhkernElement
{
	BASE_SVG_ELEMENT
	SVG_String u1;
	SVG_String g1;
	SVG_String u2;
	SVG_String g2;
	SVG_Number k;
} SVGhkernElement;


typedef struct _tagSVGimageElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
} SVGimageElement;


typedef struct _tagSVGlineElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVGlineElement;


typedef struct _tagSVGlinearGradientElement
{
	BASE_SVG_ELEMENT
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Matrix gradientTransform;
	SVG_Coordinate x1;
	SVG_Coordinate y1;
	SVG_Coordinate x2;
	SVG_Coordinate y2;
} SVGlinearGradientElement;


typedef struct _tagSVGlistenerElement
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
} SVGlistenerElement;


typedef struct _tagSVGmetadataElement
{
	BASE_SVG_ELEMENT
} SVGmetadataElement;


typedef struct _tagSVGmissing_glyphElement
{
	BASE_SVG_ELEMENT
	SVG_Number horiz_adv_x;
	SVG_PathData d;
} SVGmissing_glyphElement;


typedef struct _tagSVGmpathElement
{
	BASE_SVG_ELEMENT
} SVGmpathElement;


typedef struct _tagSVGpathElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Number pathLength;
	SVG_PathData d;
} SVGpathElement;


typedef struct _tagSVGpolygonElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Points points;
} SVGpolygonElement;


typedef struct _tagSVGpolylineElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Points points;
} SVGpolylineElement;


typedef struct _tagSVGprefetchElement
{
	BASE_SVG_ELEMENT
	SVG_Number mediaSize;
	SVG_String mediaTime;
	SVG_String mediaCharacterEncoding;
	SVG_String mediaContentEncodings;
	SVG_Number bandwidth;
} SVGprefetchElement;


typedef struct _tagSVGradialGradientElement
{
	BASE_SVG_ELEMENT
	SVG_Coordinate fx;
	SVG_Coordinate fy;
	SVG_GradientUnit gradientUnits;
	SVG_SpreadMethod spreadMethod;
	SVG_Matrix gradientTransform;
	SVG_Coordinate cx;
	SVG_Coordinate cy;
	SVG_Length r;
} SVGradialGradientElement;


typedef struct _tagSVGrectElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_Length rx;
	SVG_Length ry;
} SVGrectElement;


typedef struct _tagSVGrectClipElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	LASeR_Size size;
} SVGrectClipElement;


typedef struct _tagSVGscriptElement
{
	BASE_SVG_ELEMENT
} SVGscriptElement;


typedef struct _tagSVGselectorElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	LASeR_Choice choice;
} SVGselectorElement;


typedef struct _tagSVGsetElement
{
	BASE_SVG_ELEMENT
} SVGsetElement;


typedef struct _tagSVGsimpleLayoutElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	LASeR_Size delta;
} SVGsimpleLayoutElement;


typedef struct _tagSVGsolidColorElement
{
	BASE_SVG_ELEMENT
} SVGsolidColorElement;


typedef struct _tagSVGstopElement
{
	BASE_SVG_ELEMENT
	SVG_Number offset;
} SVGstopElement;


typedef struct _tagSVGsvgElement
{
	BASE_SVG_ELEMENT
	SVG_ViewBox viewBox;
	SVG_ZoomAndPan zoomAndPan;
	SVG_String version;
	SVG_String baseProfile;
	SVG_ContentType contentScriptType;
	SVG_Clock snapshotTime;
	SVG_TimelineBegin timelineBegin;
	SVG_PlaybackOrder playbackOrder;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
} SVGsvgElement;


typedef struct _tagSVGswitchElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
} SVGswitchElement;


typedef struct _tagSVGtbreakElement
{
	BASE_SVG_ELEMENT
} SVGtbreakElement;


typedef struct _tagSVGtextElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinates x;
	SVG_Coordinates y;
	SVG_Numbers rotate;
	SVG_Boolean editable;
} SVGtextElement;


typedef struct _tagSVGtextAreaElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Length width;
	SVG_Length height;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Boolean editable;
} SVGtextAreaElement;


typedef struct _tagSVGtitleElement
{
	BASE_SVG_ELEMENT
} SVGtitleElement;


typedef struct _tagSVGtspanElement
{
	BASE_SVG_ELEMENT
} SVGtspanElement;


typedef struct _tagSVGuseElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_Coordinate x;
	SVG_Coordinate y;
} SVGuseElement;


typedef struct _tagSVGvideoElement
{
	BASE_SVG_ELEMENT
	SVG_Matrix transform;
	SVG_TransformBehavior transformBehavior;
	SVG_Overlay overlay;
	SVG_Coordinate x;
	SVG_Coordinate y;
	SVG_Length width;
	SVG_Length height;
	SVG_PreserveAspectRatio preserveAspectRatio;
	SVG_InitialVisibility initialVisibility;
} SVGvideoElement;


/******************************************
*  End SVG Elements structure definitions *
*******************************************/
#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG_NODES_H*/

