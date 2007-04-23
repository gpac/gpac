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
	DO NOT MOFIFY - File generated on GMT Mon Apr 23 13:06:03 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/

#ifndef _GF_SVG3_NODES_H
#define _GF_SVG3_NODES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/scenegraph_svg.h>


/* Definition of SVG 3 Alternate element internal tags */
/* TAG names are made of "TAG_SVG3" + SVG element name (with - replaced by _) */
enum {
	TAG_SVG3_a = GF_NODE_RANGE_FIRST_SVG3,
	TAG_SVG3_animate,
	TAG_SVG3_animateColor,
	TAG_SVG3_animateMotion,
	TAG_SVG3_animateTransform,
	TAG_SVG3_animation,
	TAG_SVG3_audio,
	TAG_SVG3_circle,
	TAG_SVG3_conditional,
	TAG_SVG3_cursorManager,
	TAG_SVG3_defs,
	TAG_SVG3_desc,
	TAG_SVG3_discard,
	TAG_SVG3_ellipse,
	TAG_SVG3_font,
	TAG_SVG3_font_face,
	TAG_SVG3_font_face_src,
	TAG_SVG3_font_face_uri,
	TAG_SVG3_foreignObject,
	TAG_SVG3_g,
	TAG_SVG3_glyph,
	TAG_SVG3_handler,
	TAG_SVG3_hkern,
	TAG_SVG3_image,
	TAG_SVG3_line,
	TAG_SVG3_linearGradient,
	TAG_SVG3_listener,
	TAG_SVG3_metadata,
	TAG_SVG3_missing_glyph,
	TAG_SVG3_mpath,
	TAG_SVG3_path,
	TAG_SVG3_polygon,
	TAG_SVG3_polyline,
	TAG_SVG3_prefetch,
	TAG_SVG3_radialGradient,
	TAG_SVG3_rect,
	TAG_SVG3_rectClip,
	TAG_SVG3_script,
	TAG_SVG3_selector,
	TAG_SVG3_set,
	TAG_SVG3_simpleLayout,
	TAG_SVG3_solidColor,
	TAG_SVG3_stop,
	TAG_SVG3_svg,
	TAG_SVG3_switch,
	TAG_SVG3_tbreak,
	TAG_SVG3_text,
	TAG_SVG3_textArea,
	TAG_SVG3_title,
	TAG_SVG3_tspan,
	TAG_SVG3_use,
	TAG_SVG3_video,
	/*undefined elements (when parsing) use this tag*/
	TAG_SVG3_UndefinedElement
};

/* Definition of SVG 3 attribute internal tags */
/* TAG names are made of "TAG_SVG3_ATT_" + SVG attribute name (with - replaced by _) */
enum {
	TAG_SVG3_ATT_id,
	TAG_SVG3_ATT__class,
	TAG_SVG3_ATT_xml_id,
	TAG_SVG3_ATT_xml_base,
	TAG_SVG3_ATT_xml_lang,
	TAG_SVG3_ATT_xml_space,
	TAG_SVG3_ATT_requiredFeatures,
	TAG_SVG3_ATT_requiredExtensions,
	TAG_SVG3_ATT_requiredFormats,
	TAG_SVG3_ATT_requiredFonts,
	TAG_SVG3_ATT_systemLanguage,
	TAG_SVG3_ATT_display,
	TAG_SVG3_ATT_visibility,
	TAG_SVG3_ATT_image_rendering,
	TAG_SVG3_ATT_pointer_events,
	TAG_SVG3_ATT_shape_rendering,
	TAG_SVG3_ATT_text_rendering,
	TAG_SVG3_ATT_audio_level,
	TAG_SVG3_ATT_viewport_fill,
	TAG_SVG3_ATT_viewport_fill_opacity,
	TAG_SVG3_ATT_fill_opacity,
	TAG_SVG3_ATT_stroke_opacity,
	TAG_SVG3_ATT_fill,
	TAG_SVG3_ATT_fill_rule,
	TAG_SVG3_ATT_stroke,
	TAG_SVG3_ATT_stroke_dasharray,
	TAG_SVG3_ATT_stroke_dashoffset,
	TAG_SVG3_ATT_stroke_linecap,
	TAG_SVG3_ATT_stroke_linejoin,
	TAG_SVG3_ATT_stroke_miterlimit,
	TAG_SVG3_ATT_stroke_width,
	TAG_SVG3_ATT_color,
	TAG_SVG3_ATT_color_rendering,
	TAG_SVG3_ATT_vector_effect,
	TAG_SVG3_ATT_solid_color,
	TAG_SVG3_ATT_solid_opacity,
	TAG_SVG3_ATT_display_align,
	TAG_SVG3_ATT_line_increment,
	TAG_SVG3_ATT_stop_color,
	TAG_SVG3_ATT_stop_opacity,
	TAG_SVG3_ATT_font_family,
	TAG_SVG3_ATT_font_size,
	TAG_SVG3_ATT_font_style,
	TAG_SVG3_ATT_font_variant,
	TAG_SVG3_ATT_font_weight,
	TAG_SVG3_ATT_text_anchor,
	TAG_SVG3_ATT_text_align,
	TAG_SVG3_ATT_focusHighlight,
	TAG_SVG3_ATT_externalResourcesRequired,
	TAG_SVG3_ATT_focusable,
	TAG_SVG3_ATT_nav_next,
	TAG_SVG3_ATT_nav_prev,
	TAG_SVG3_ATT_nav_up,
	TAG_SVG3_ATT_nav_up_right,
	TAG_SVG3_ATT_nav_right,
	TAG_SVG3_ATT_nav_down_right,
	TAG_SVG3_ATT_nav_down,
	TAG_SVG3_ATT_nav_down_left,
	TAG_SVG3_ATT_nav_left,
	TAG_SVG3_ATT_nav_up_left,
	TAG_SVG3_ATT_transform,
	TAG_SVG3_ATT_xlink_type,
	TAG_SVG3_ATT_xlink_role,
	TAG_SVG3_ATT_xlink_arcrole,
	TAG_SVG3_ATT_xlink_title,
	TAG_SVG3_ATT_xlink_href,
	TAG_SVG3_ATT_xlink_show,
	TAG_SVG3_ATT_xlink_actuate,
	TAG_SVG3_ATT_target,
	TAG_SVG3_ATT_attributeName,
	TAG_SVG3_ATT_attributeType,
	TAG_SVG3_ATT_begin,
	TAG_SVG3_ATT_lsr_enabled,
	TAG_SVG3_ATT_dur,
	TAG_SVG3_ATT_end,
	TAG_SVG3_ATT_repeatCount,
	TAG_SVG3_ATT_repeatDur,
	TAG_SVG3_ATT_restart,
	TAG_SVG3_ATT_smil_fill,
	TAG_SVG3_ATT_min,
	TAG_SVG3_ATT_max,
	TAG_SVG3_ATT_to,
	TAG_SVG3_ATT_calcMode,
	TAG_SVG3_ATT_values,
	TAG_SVG3_ATT_keyTimes,
	TAG_SVG3_ATT_keySplines,
	TAG_SVG3_ATT_from,
	TAG_SVG3_ATT_by,
	TAG_SVG3_ATT_additive,
	TAG_SVG3_ATT_accumulate,
	TAG_SVG3_ATT_path,
	TAG_SVG3_ATT_keyPoints,
	TAG_SVG3_ATT_rotate,
	TAG_SVG3_ATT_origin,
	TAG_SVG3_ATT_type,
	TAG_SVG3_ATT_clipBegin,
	TAG_SVG3_ATT_clipEnd,
	TAG_SVG3_ATT_syncBehavior,
	TAG_SVG3_ATT_syncTolerance,
	TAG_SVG3_ATT_syncMaster,
	TAG_SVG3_ATT_syncReference,
	TAG_SVG3_ATT_x,
	TAG_SVG3_ATT_y,
	TAG_SVG3_ATT_width,
	TAG_SVG3_ATT_height,
	TAG_SVG3_ATT_preserveAspectRatio,
	TAG_SVG3_ATT_initialVisibility,
	TAG_SVG3_ATT_content_type,
	TAG_SVG3_ATT_cx,
	TAG_SVG3_ATT_cy,
	TAG_SVG3_ATT_r,
	TAG_SVG3_ATT_enabled,
	TAG_SVG3_ATT_cursorManager_x,
	TAG_SVG3_ATT_cursorManager_y,
	TAG_SVG3_ATT_rx,
	TAG_SVG3_ATT_ry,
	TAG_SVG3_ATT_horiz_adv_x,
	TAG_SVG3_ATT_horiz_origin_x,
	TAG_SVG3_ATT_font_stretch,
	TAG_SVG3_ATT_unicode_range,
	TAG_SVG3_ATT_panose_1,
	TAG_SVG3_ATT_widths,
	TAG_SVG3_ATT_bbox,
	TAG_SVG3_ATT_units_per_em,
	TAG_SVG3_ATT_stemv,
	TAG_SVG3_ATT_stemh,
	TAG_SVG3_ATT_slope,
	TAG_SVG3_ATT_cap_height,
	TAG_SVG3_ATT_x_height,
	TAG_SVG3_ATT_accent_height,
	TAG_SVG3_ATT_ascent,
	TAG_SVG3_ATT_descent,
	TAG_SVG3_ATT_ideographic,
	TAG_SVG3_ATT_alphabetic,
	TAG_SVG3_ATT_mathematical,
	TAG_SVG3_ATT_hanging,
	TAG_SVG3_ATT_underline_position,
	TAG_SVG3_ATT_underline_thickness,
	TAG_SVG3_ATT_strikethrough_position,
	TAG_SVG3_ATT_strikethrough_thickness,
	TAG_SVG3_ATT_overline_position,
	TAG_SVG3_ATT_overline_thickness,
	TAG_SVG3_ATT_d,
	TAG_SVG3_ATT_unicode,
	TAG_SVG3_ATT_glyph_name,
	TAG_SVG3_ATT_arabic_form,
	TAG_SVG3_ATT_lang,
	TAG_SVG3_ATT_ev_event,
	TAG_SVG3_ATT_u1,
	TAG_SVG3_ATT_g1,
	TAG_SVG3_ATT_u2,
	TAG_SVG3_ATT_g2,
	TAG_SVG3_ATT_k,
	TAG_SVG3_ATT_opacity,
	TAG_SVG3_ATT_x1,
	TAG_SVG3_ATT_y1,
	TAG_SVG3_ATT_x2,
	TAG_SVG3_ATT_y2,
	TAG_SVG3_ATT_gradientUnits,
	TAG_SVG3_ATT_spreadMethod,
	TAG_SVG3_ATT_gradientTransform,
	TAG_SVG3_ATT_event,
	TAG_SVG3_ATT_phase,
	TAG_SVG3_ATT_propagate,
	TAG_SVG3_ATT_defaultAction,
	TAG_SVG3_ATT_observer,
	TAG_SVG3_ATT_listener_target,
	TAG_SVG3_ATT_handler,
	TAG_SVG3_ATT_pathLength,
	TAG_SVG3_ATT_points,
	TAG_SVG3_ATT_mediaSize,
	TAG_SVG3_ATT_mediaTime,
	TAG_SVG3_ATT_mediaCharacterEncoding,
	TAG_SVG3_ATT_mediaContentEncodings,
	TAG_SVG3_ATT_bandwidth,
	TAG_SVG3_ATT_fx,
	TAG_SVG3_ATT_fy,
	TAG_SVG3_ATT_size,
	TAG_SVG3_ATT_choice,
	TAG_SVG3_ATT_delta,
	TAG_SVG3_ATT_offset,
	TAG_SVG3_ATT_syncBehaviorDefault,
	TAG_SVG3_ATT_syncToleranceDefault,
	TAG_SVG3_ATT_viewBox,
	TAG_SVG3_ATT_zoomAndPan,
	TAG_SVG3_ATT_version,
	TAG_SVG3_ATT_baseProfile,
	TAG_SVG3_ATT_snapshotTime,
	TAG_SVG3_ATT_timelineBegin,
	TAG_SVG3_ATT_playbackOrder,
	TAG_SVG3_ATT_editable,
	TAG_SVG3_ATT_text_x,
	TAG_SVG3_ATT_text_y,
	TAG_SVG3_ATT_text_rotate,
	TAG_SVG3_ATT_transformBehavior,
	TAG_SVG3_ATT_overlay,
	TAG_SVG3_ATT_motionTransform,
	/*undefined attributes (when parsing) use this tag*/
	TAG_SVG3_ATT_Unknown
};

struct _all_atts {
	SVG_ID *id;
	SVG_String *_class;
	SVG_ID *xml_id;
	SVG_IRI *xml_base;
	SVG_LanguageID *xml_lang;
	XML_Space *xml_space;
	SVG_ListOfIRI *requiredFeatures;
	SVG_ListOfIRI *requiredExtensions;
	SVG_FormatList *requiredFormats;
	SVG_FontList *requiredFonts;
	SVG_LanguageIDs *systemLanguage;
	SVG_Display *display;
	SVG_Visibility *visibility;
	SVG_RenderingHint *image_rendering;
	SVG_PointerEvents *pointer_events;
	SVG_RenderingHint *shape_rendering;
	SVG_RenderingHint *text_rendering;
	SVG_Number *audio_level;
	SVG_Paint *viewport_fill;
	SVG_Number *viewport_fill_opacity;
	SVG_Number *fill_opacity;
	SVG_Number *stroke_opacity;
	SVG_Paint *fill;
	SVG_FillRule *fill_rule;
	SVG_Paint *stroke;
	SVG_StrokeDashArray *stroke_dasharray;
	SVG_Length *stroke_dashoffset;
	SVG_StrokeLineCap *stroke_linecap;
	SVG_StrokeLineJoin *stroke_linejoin;
	SVG_Number *stroke_miterlimit;
	SVG_Length *stroke_width;
	SVG_Paint *color;
	SVG_RenderingHint *color_rendering;
	SVG_VectorEffect *vector_effect;
	SVG_SVGColor *solid_color;
	SVG_Number *solid_opacity;
	SVG_DisplayAlign *display_align;
	SVG_Number *line_increment;
	SVG_SVGColor *stop_color;
	SVG_Number *stop_opacity;
	SVG_FontFamily *font_family;
	SVG_FontSize *font_size;
	SVG_FontStyle *font_style;
	SVG_FontVariant *font_variant;
	SVG_FontWeight *font_weight;
	SVG_TextAnchor *text_anchor;
	SVG_TextAlign *text_align;
	SVG_FocusHighlight *focusHighlight;
	SVG_Boolean *externalResourcesRequired;
	SVG_Boolean *focusable;
	SVG_Focus *nav_next;
	SVG_Focus *nav_prev;
	SVG_Focus *nav_up;
	SVG_Focus *nav_up_right;
	SVG_Focus *nav_right;
	SVG_Focus *nav_down_right;
	SVG_Focus *nav_down;
	SVG_Focus *nav_down_left;
	SVG_Focus *nav_left;
	SVG_Focus *nav_up_left;
	SVG_Transform *transform;
	SVG_String *xlink_type;
	SVG_IRI *xlink_role;
	SVG_IRI *xlink_arcrole;
	SVG_String *xlink_title;
	SVG_IRI *xlink_href;
	SVG_String *xlink_show;
	SVG_String *xlink_actuate;
	SVG_ID *target;
	SMIL_AttributeName *attributeName;
	SMIL_AttributeType *attributeType;
	SMIL_Times *begin;
	SVG_Boolean *lsr_enabled;
	SMIL_Duration *dur;
	SMIL_Times *end;
	SMIL_RepeatCount *repeatCount;
	SMIL_Duration *repeatDur;
	SMIL_Restart *restart;
	SMIL_Fill *smil_fill;
	SMIL_Duration *min;
	SMIL_Duration *max;
	SMIL_AnimateValue *to;
	SMIL_CalcMode *calcMode;
	SMIL_AnimateValues *values;
	SMIL_KeyTimes *keyTimes;
	SMIL_KeySplines *keySplines;
	SMIL_AnimateValue *from;
	SMIL_AnimateValue *by;
	SMIL_Additive *additive;
	SMIL_Accumulate *accumulate;
	SVG_PathData *path;
	SMIL_KeyPoints *keyPoints;
	SVG_Rotate *rotate;
	SVG_String *origin;
	SVG_TransformType *type;
	SVG_Clock *clipBegin;
	SVG_Clock *clipEnd;
	SMIL_SyncBehavior *syncBehavior;
	SMIL_SyncTolerance *syncTolerance;
	SVG_Boolean *syncMaster;
	SVG_String *syncReference;
	SVG_Coordinate *x;
	SVG_Coordinate *y;
	SVG_Length *width;
	SVG_Length *height;
	SVG_PreserveAspectRatio *preserveAspectRatio;
	SVG_InitialVisibility *initialVisibility;
	SVG_ContentType *content_type;
	SVG_Coordinate *cx;
	SVG_Coordinate *cy;
	SVG_Length *r;
	SVG_Boolean *enabled;
	SVG_Length *cursorManager_x;
	SVG_Length *cursorManager_y;
	SVG_Length *rx;
	SVG_Length *ry;
	SVG_Number *horiz_adv_x;
	SVG_Number *horiz_origin_x;
	SVG_String *font_stretch;
	SVG_String *unicode_range;
	SVG_String *panose_1;
	SVG_String *widths;
	SVG_String *bbox;
	SVG_Number *units_per_em;
	SVG_Number *stemv;
	SVG_Number *stemh;
	SVG_Number *slope;
	SVG_Number *cap_height;
	SVG_Number *x_height;
	SVG_Number *accent_height;
	SVG_Number *ascent;
	SVG_Number *descent;
	SVG_Number *ideographic;
	SVG_Number *alphabetic;
	SVG_Number *mathematical;
	SVG_Number *hanging;
	SVG_Number *underline_position;
	SVG_Number *underline_thickness;
	SVG_Number *strikethrough_position;
	SVG_Number *strikethrough_thickness;
	SVG_Number *overline_position;
	SVG_Number *overline_thickness;
	SVG_PathData *d;
	SVG_String *unicode;
	SVG_String *glyph_name;
	SVG_String *arabic_form;
	SVG_LanguageIDs *lang;
	XMLEV_Event *ev_event;
	SVG_String *u1;
	SVG_String *g1;
	SVG_String *u2;
	SVG_String *g2;
	SVG_Number *k;
	SVG_Number *opacity;
	SVG_Coordinate *x1;
	SVG_Coordinate *y1;
	SVG_Coordinate *x2;
	SVG_Coordinate *y2;
	SVG_GradientUnit *gradientUnits;
	SVG_SpreadMethod *spreadMethod;
	SVG_Transform *gradientTransform;
	XMLEV_Event *event;
	XMLEV_Phase *phase;
	XMLEV_Propagate *propagate;
	XMLEV_DefaultAction *defaultAction;
	SVG_IRI *observer;
	SVG_IRI *listener_target;
	SVG_IRI *handler;
	SVG_Number *pathLength;
	SVG_Points *points;
	SVG_Number *mediaSize;
	SVG_String *mediaTime;
	SVG_String *mediaCharacterEncoding;
	SVG_String *mediaContentEncodings;
	SVG_Number *bandwidth;
	SVG_Coordinate *fx;
	SVG_Coordinate *fy;
	LASeR_Size *size;
	LASeR_Choice *choice;
	LASeR_Size *delta;
	SVG_Number *offset;
	SMIL_SyncBehavior *syncBehaviorDefault;
	SMIL_SyncTolerance *syncToleranceDefault;
	SVG_ViewBox *viewBox;
	SVG_ZoomAndPan *zoomAndPan;
	SVG_String *version;
	SVG_String *baseProfile;
	SVG_Clock *snapshotTime;
	SVG_TimelineBegin *timelineBegin;
	SVG_PlaybackOrder *playbackOrder;
	SVG_Boolean *editable;
	SVG_Coordinates *text_x;
	SVG_Coordinates *text_y;
	SVG_Numbers *text_rotate;
	SVG_TransformBehavior *transformBehavior;
	SVG_Overlay *overlay;
	GF_Matrix2D *motionTransform;
};

#ifdef __cplusplus
}
#endif



#endif		/*_GF_SVG3_NODES_H*/

