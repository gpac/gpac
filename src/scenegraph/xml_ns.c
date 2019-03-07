/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
 *
 *  GPAC is free software, TYPE, 0 }, you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, TYPE, 0 }, either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY, TYPE, 0 }, without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library, TYPE, 0 }, see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <gpac/internal/scenegraph_dev.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/nodes_svg.h>

enum
{
	/*no specific criteria*/
	GF_SVG_ATTOPT_NONE = 0,
	/*attribute only valid for SMIL anims*/
	GF_SVG_ATTOPT_SMIL = 1,
	/*attribute only valid for TEXT*/
	GF_SVG_ATTOPT_TEXT = 2,
	/*attribute only valid for cursor*/
	GF_SVG_ATTOPT_CURSOR = 3,
	/*attribute only valid for listener*/
	GF_SVG_ATTOPT_LISTENER = 4,
	/*attribute only valid for filters*/
	GF_SVG_ATTOPT_FILTER = 5,
} GF_SVGAttOption;

static const struct xml_att_def {
	const char *name;
	u32 tag;
	u32 type;
	u32 opts;
	u32 xmlns;
} xml_attributes [] =
{
	/*XML base*/
	{ "id", TAG_XML_ATT_id, SVG_ID_datatype, 0, GF_XMLNS_XML } ,
	{ "base", TAG_XML_ATT_base, XMLRI_datatype, 0, GF_XMLNS_XML } ,
	{ "lang", TAG_XML_ATT_lang, SVG_LanguageID_datatype, 0, GF_XMLNS_XML } ,
	{ "space", TAG_XML_ATT_space, XML_Space_datatype, 0, GF_XMLNS_XML } ,

	/*XLINK*/
	{ "type", TAG_XLINK_ATT_type, DOM_String_datatype, 0, GF_XMLNS_XLINK },
	{ "role", TAG_XLINK_ATT_role, XMLRI_datatype, 0, GF_XMLNS_XLINK },
	{ "arcrole", TAG_XLINK_ATT_arcrole, XMLRI_datatype, 0, GF_XMLNS_XLINK },
	{ "title", TAG_XLINK_ATT_title, DOM_String_datatype, 0, GF_XMLNS_XLINK },
	{ "href", TAG_XLINK_ATT_href, XMLRI_datatype, 0, GF_XMLNS_XLINK },
	{ "show", TAG_XLINK_ATT_show, DOM_String_datatype, 0, GF_XMLNS_XLINK },
	{ "actuate", TAG_XLINK_ATT_actuate, DOM_String_datatype, 0, GF_XMLNS_XLINK },

	/*XML Events*/
	{ "event", TAG_XMLEV_ATT_event, XMLEV_Event_datatype, 0, GF_XMLNS_XMLEV },
	{ "phase", TAG_XMLEV_ATT_phase, XMLEV_Phase_datatype, 0, GF_XMLNS_XMLEV },
	{ "propagate", TAG_XMLEV_ATT_propagate, XMLEV_Propagate_datatype, 0, GF_XMLNS_XMLEV },
	{ "defaultAction", TAG_XMLEV_ATT_defaultAction, XMLEV_DefaultAction_datatype, 0, GF_XMLNS_XMLEV },
	{ "observer", TAG_XMLEV_ATT_observer, XML_IDREF_datatype, 0, GF_XMLNS_XMLEV },
	{ "target", TAG_XMLEV_ATT_target, XML_IDREF_datatype, 0, GF_XMLNS_XMLEV },
	{ "handler", TAG_XMLEV_ATT_handler, XMLRI_datatype, 0, GF_XMLNS_XMLEV },


	{ "id", TAG_SVG_ATT_id, SVG_ID_datatype, 0, GF_XMLNS_SVG } ,
	{ "class", TAG_SVG_ATT__class, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "requiredFeatures", TAG_SVG_ATT_requiredFeatures, XMLRI_List_datatype, 0, GF_XMLNS_SVG },
	{ "requiredExtensions", TAG_SVG_ATT_requiredExtensions, XMLRI_List_datatype, 0, GF_XMLNS_SVG },
	{ "requiredFormats", TAG_SVG_ATT_requiredFormats, DOM_StringList_datatype, 0, GF_XMLNS_SVG },
	{ "requiredFonts", TAG_SVG_ATT_requiredFonts, DOM_StringList_datatype, 0, GF_XMLNS_SVG },
	{ "systemLanguage", TAG_SVG_ATT_systemLanguage, DOM_StringList_datatype, 0, GF_XMLNS_SVG },
	{ "display", TAG_SVG_ATT_display, SVG_Display_datatype, 0, GF_XMLNS_SVG },
	{ "visibility", TAG_SVG_ATT_visibility, SVG_Visibility_datatype, 0, GF_XMLNS_SVG },
	{ "image-rendering", TAG_SVG_ATT_image_rendering, SVG_RenderingHint_datatype, 0, GF_XMLNS_SVG },
	{ "pointer-events", TAG_SVG_ATT_pointer_events, SVG_PointerEvents_datatype, 0, GF_XMLNS_SVG },
	{ "shape-rendering", TAG_SVG_ATT_shape_rendering, SVG_RenderingHint_datatype, 0, GF_XMLNS_SVG },
	{ "text-rendering", TAG_SVG_ATT_text_rendering, SVG_RenderingHint_datatype, 0, GF_XMLNS_SVG },
	{ "audio-level", TAG_SVG_ATT_audio_level, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "viewport-fill", TAG_SVG_ATT_viewport_fill, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	{ "viewport-fill-opacity", TAG_SVG_ATT_viewport_fill_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "overflow", TAG_SVG_ATT_overflow, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "fill-opacity", TAG_SVG_ATT_fill_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-opacity", TAG_SVG_ATT_stroke_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "fill-rule", TAG_SVG_ATT_fill_rule, SVG_FillRule_datatype, 0, GF_XMLNS_SVG },
	{ "stroke", TAG_SVG_ATT_stroke, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-dasharray", TAG_SVG_ATT_stroke_dasharray, SVG_StrokeDashArray_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-dashoffset", TAG_SVG_ATT_stroke_dashoffset, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-linecap", TAG_SVG_ATT_stroke_linecap, SVG_StrokeLineCap_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-linejoin", TAG_SVG_ATT_stroke_linejoin, SVG_StrokeLineJoin_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-miterlimit", TAG_SVG_ATT_stroke_miterlimit, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "stroke-width", TAG_SVG_ATT_stroke_width, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "color", TAG_SVG_ATT_color, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	{ "color-rendering", TAG_SVG_ATT_color_rendering, SVG_RenderingHint_datatype, 0, GF_XMLNS_SVG },
	{ "vector-effect", TAG_SVG_ATT_vector_effect, SVG_VectorEffect_datatype, 0, GF_XMLNS_SVG },
	{ "solid-color", TAG_SVG_ATT_solid_color, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	{ "solid-opacity", TAG_SVG_ATT_solid_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "display-align", TAG_SVG_ATT_display_align, SVG_DisplayAlign_datatype, 0, GF_XMLNS_SVG },
	{ "line-increment", TAG_SVG_ATT_line_increment, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "stop-color", TAG_SVG_ATT_stop_color, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	{ "stop-opacity", TAG_SVG_ATT_stop_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "font-family", TAG_SVG_ATT_font_family, SVG_FontFamily_datatype, 0, GF_XMLNS_SVG },
	{ "font-size", TAG_SVG_ATT_font_size, SVG_FontSize_datatype, 0, GF_XMLNS_SVG },
	{ "font-style", TAG_SVG_ATT_font_style, SVG_FontStyle_datatype, 0, GF_XMLNS_SVG },
	{ "font-variant", TAG_SVG_ATT_font_variant, SVG_FontVariant_datatype, 0, GF_XMLNS_SVG },
	{ "font-weight", TAG_SVG_ATT_font_weight, SVG_FontWeight_datatype, 0, GF_XMLNS_SVG },
	{ "text-anchor", TAG_SVG_ATT_text_anchor, SVG_TextAnchor_datatype, 0, GF_XMLNS_SVG },
	{ "text-align", TAG_SVG_ATT_text_align, SVG_TextAlign_datatype, 0, GF_XMLNS_SVG },
	{ "text-decoration", TAG_SVG_ATT_text_decoration, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "focusHighlight", TAG_SVG_ATT_focusHighlight, SVG_FocusHighlight_datatype, 0, GF_XMLNS_SVG },
	{ "externalResourcesRequired", TAG_SVG_ATT_externalResourcesRequired, SVG_Boolean_datatype, 0, GF_XMLNS_SVG },
	{ "focusable", TAG_SVG_ATT_focusable, SVG_Focusable_datatype, 0, GF_XMLNS_SVG },
	{ "nav-next", TAG_SVG_ATT_nav_next, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-prev", TAG_SVG_ATT_nav_prev, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-up", TAG_SVG_ATT_nav_up, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-up-right", TAG_SVG_ATT_nav_up_right, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-right", TAG_SVG_ATT_nav_right, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-down-right", TAG_SVG_ATT_nav_down_right, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-down", TAG_SVG_ATT_nav_down, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-down-left", TAG_SVG_ATT_nav_down_left, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-left", TAG_SVG_ATT_nav_left, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "nav-up-left", TAG_SVG_ATT_nav_up_left, SVG_Focus_datatype, 0, GF_XMLNS_SVG },
	{ "transform", TAG_SVG_ATT_transform, SVG_Transform_datatype, 0, GF_XMLNS_SVG },
	{ "target", TAG_SVG_ATT_target, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "attributeName", TAG_SVG_ATT_attributeName, SMIL_AttributeName_datatype, 0, GF_XMLNS_SVG },
	{ "attributeType", TAG_SVG_ATT_attributeType, SMIL_AttributeType_datatype, 0, GF_XMLNS_SVG },
	{ "begin", TAG_SVG_ATT_begin, SMIL_Times_datatype, 0, GF_XMLNS_SVG },
	{ "dur", TAG_SVG_ATT_dur, SMIL_Duration_datatype, 0, GF_XMLNS_SVG },
	{ "end", TAG_SVG_ATT_end, SMIL_Times_datatype, 0, GF_XMLNS_SVG },
	{ "repeatCount", TAG_SVG_ATT_repeatCount, SMIL_RepeatCount_datatype, 0, GF_XMLNS_SVG },
	{ "repeatDur", TAG_SVG_ATT_repeatDur, SMIL_Duration_datatype, 0, GF_XMLNS_SVG },
	{ "restart", TAG_SVG_ATT_restart, SMIL_Restart_datatype, 0, GF_XMLNS_SVG },
	{ "min", TAG_SVG_ATT_min, SMIL_Duration_datatype, 0, GF_XMLNS_SVG },
	{ "max", TAG_SVG_ATT_max, SMIL_Duration_datatype, 0, GF_XMLNS_SVG },
	{ "to", TAG_SVG_ATT_to, SMIL_AnimateValue_datatype, 0, GF_XMLNS_SVG },
	{ "calcMode", TAG_SVG_ATT_calcMode, SMIL_CalcMode_datatype, 0, GF_XMLNS_SVG },
	{ "values", TAG_SVG_ATT_values, SMIL_AnimateValues_datatype, 0, GF_XMLNS_SVG },
	{ "keyTimes", TAG_SVG_ATT_keyTimes, SMIL_KeyTimes_datatype, 0, GF_XMLNS_SVG },
	{ "keySplines", TAG_SVG_ATT_keySplines, SMIL_KeySplines_datatype, 0, GF_XMLNS_SVG },
	{ "from", TAG_SVG_ATT_from, SMIL_AnimateValue_datatype, 0, GF_XMLNS_SVG },
	{ "by", TAG_SVG_ATT_by, SMIL_AnimateValue_datatype, 0, GF_XMLNS_SVG },
	{ "additive", TAG_SVG_ATT_additive, SMIL_Additive_datatype, 0, GF_XMLNS_SVG },
	{ "accumulate", TAG_SVG_ATT_accumulate, SMIL_Accumulate_datatype, 0, GF_XMLNS_SVG },
	{ "path", TAG_SVG_ATT_path, SVG_PathData_datatype, 0, GF_XMLNS_SVG },
	{ "keyPoints", TAG_SVG_ATT_keyPoints, SMIL_KeyPoints_datatype, 0, GF_XMLNS_SVG },
	{ "origin", TAG_SVG_ATT_origin, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "clipBegin", TAG_SVG_ATT_clipBegin, SVG_Clock_datatype, 0, GF_XMLNS_SVG },
	{ "clipEnd", TAG_SVG_ATT_clipEnd, SVG_Clock_datatype, 0, GF_XMLNS_SVG },
	{ "syncBehavior", TAG_SVG_ATT_syncBehavior, SMIL_SyncBehavior_datatype, 0, GF_XMLNS_SVG },
	{ "syncTolerance", TAG_SVG_ATT_syncTolerance, SMIL_SyncTolerance_datatype, 0, GF_XMLNS_SVG },
	{ "syncMaster", TAG_SVG_ATT_syncMaster, SVG_Boolean_datatype, 0, GF_XMLNS_SVG },
	{ "syncReference", TAG_SVG_ATT_syncReference, XMLRI_datatype, 0, GF_XMLNS_SVG },
	{ "width", TAG_SVG_ATT_width, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "height", TAG_SVG_ATT_height, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "preserveAspectRatio", TAG_SVG_ATT_preserveAspectRatio, SVG_PreserveAspectRatio_datatype, 0, GF_XMLNS_SVG },
	{ "initialVisibility", TAG_SVG_ATT_initialVisibility, SVG_InitialVisibility_datatype, 0, GF_XMLNS_SVG },
	{ "cx", TAG_SVG_ATT_cx, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "cy", TAG_SVG_ATT_cy, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "r", TAG_SVG_ATT_r, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "rx", TAG_SVG_ATT_rx, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "ry", TAG_SVG_ATT_ry, SVG_Length_datatype, 0, GF_XMLNS_SVG },
	{ "horiz-adv-x", TAG_SVG_ATT_horiz_adv_x, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "horiz-origin-x", TAG_SVG_ATT_horiz_origin_x, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "font-stretch", TAG_SVG_ATT_font_stretch, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "unicode-range", TAG_SVG_ATT_unicode_range, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "panose-1", TAG_SVG_ATT_panose_1, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "widths", TAG_SVG_ATT_widths, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "bbox", TAG_SVG_ATT_bbox, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "units-per-em", TAG_SVG_ATT_units_per_em, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "stemv", TAG_SVG_ATT_stemv, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "stemh", TAG_SVG_ATT_stemh, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "slope", TAG_SVG_ATT_slope, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "cap-height", TAG_SVG_ATT_cap_height, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "x-height", TAG_SVG_ATT_x_height, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "accent-height", TAG_SVG_ATT_accent_height, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "ascent", TAG_SVG_ATT_ascent, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "descent", TAG_SVG_ATT_descent, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "ideographic", TAG_SVG_ATT_ideographic, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "alphabetic", TAG_SVG_ATT_alphabetic, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "mathematical", TAG_SVG_ATT_mathematical, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "hanging", TAG_SVG_ATT_hanging, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "underline-position", TAG_SVG_ATT_underline_position, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "underline-thickness", TAG_SVG_ATT_underline_thickness, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "strikethrough-position", TAG_SVG_ATT_strikethrough_position, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "strikethrough-thickness", TAG_SVG_ATT_strikethrough_thickness, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "overline-position", TAG_SVG_ATT_overline_position, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "overline-thickness", TAG_SVG_ATT_overline_thickness, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "d", TAG_SVG_ATT_d, SVG_PathData_datatype, 0, GF_XMLNS_SVG },
	{ "unicode", TAG_SVG_ATT_unicode, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "glyph-name", TAG_SVG_ATT_glyph_name, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "arabic-form", TAG_SVG_ATT_arabic_form, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "lang", TAG_SVG_ATT_lang, DOM_StringList_datatype, 0, GF_XMLNS_SVG },
	{ "u1", TAG_SVG_ATT_u1, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "g1", TAG_SVG_ATT_g1, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "u2", TAG_SVG_ATT_u2, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "g2", TAG_SVG_ATT_g2, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "k", TAG_SVG_ATT_k, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "opacity", TAG_SVG_ATT_opacity, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "x1", TAG_SVG_ATT_x1, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "y1", TAG_SVG_ATT_y1, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "x2", TAG_SVG_ATT_x2, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "y2", TAG_SVG_ATT_y2, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "gradientUnits", TAG_SVG_ATT_gradientUnits, SVG_GradientUnit_datatype, 0, GF_XMLNS_SVG },
	{ "filterUnits", TAG_SVG_ATT_filterUnits, SVG_GradientUnit_datatype, 0, GF_XMLNS_SVG },
	{ "spreadMethod", TAG_SVG_ATT_spreadMethod, SVG_SpreadMethod_datatype, 0, GF_XMLNS_SVG },
	{ "gradientTransform", TAG_SVG_ATT_gradientTransform, SVG_Transform_datatype, 0, GF_XMLNS_SVG },
	{ "pathLength", TAG_SVG_ATT_pathLength, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "points", TAG_SVG_ATT_points, SVG_Points_datatype, 0, GF_XMLNS_SVG },
	{ "mediaSize", TAG_SVG_ATT_mediaSize, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "mediaTime", TAG_SVG_ATT_mediaTime, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "mediaCharacterEncoding", TAG_SVG_ATT_mediaCharacterEncoding, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "mediaContentEncodings", TAG_SVG_ATT_mediaContentEncodings, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "bandwidth", TAG_SVG_ATT_bandwidth, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "fx", TAG_SVG_ATT_fx, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "fy", TAG_SVG_ATT_fy, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	{ "size", TAG_SVG_ATT_size, LASeR_Size_datatype, 0, GF_XMLNS_SVG },
	{ "choice", TAG_SVG_ATT_choice, LASeR_Choice_datatype, 0, GF_XMLNS_SVG },
	{ "delta", TAG_SVG_ATT_delta, LASeR_Size_datatype, 0, GF_XMLNS_SVG },
	{ "offset", TAG_SVG_ATT_offset, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "syncBehaviorDefault", TAG_SVG_ATT_syncBehaviorDefault, SMIL_SyncBehavior_datatype, 0, GF_XMLNS_SVG },
	{ "syncToleranceDefault", TAG_SVG_ATT_syncToleranceDefault, SMIL_SyncTolerance_datatype, 0, GF_XMLNS_SVG },
	{ "viewBox", TAG_SVG_ATT_viewBox, SVG_ViewBox_datatype, 0, GF_XMLNS_SVG },
	{ "zoomAndPan", TAG_SVG_ATT_zoomAndPan, SVG_ZoomAndPan_datatype, 0, GF_XMLNS_SVG },
	{ "version", TAG_SVG_ATT_version, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "baseProfile", TAG_SVG_ATT_baseProfile, DOM_String_datatype, 0, GF_XMLNS_SVG },
	{ "snapshotTime", TAG_SVG_ATT_snapshotTime, SVG_Clock_datatype, 0, GF_XMLNS_SVG },
	{ "timelineBegin", TAG_SVG_ATT_timelineBegin, SVG_TimelineBegin_datatype, 0, GF_XMLNS_SVG },
	{ "playbackOrder", TAG_SVG_ATT_playbackOrder, SVG_PlaybackOrder_datatype, 0, GF_XMLNS_SVG },
	{ "editable", TAG_SVG_ATT_editable, SVG_Boolean_datatype, 0, GF_XMLNS_SVG },
	{ "transformBehavior", TAG_SVG_ATT_transformBehavior, SVG_TransformBehavior_datatype, 0, GF_XMLNS_SVG },
	{ "overlay", TAG_SVG_ATT_overlay, SVG_Overlay_datatype, 0, GF_XMLNS_SVG },
	{ "fullscreen", TAG_SVG_ATT_fullscreen, SVG_Boolean_datatype, 0, GF_XMLNS_SVG },
	{ "motionTransform", TAG_SVG_ATT_motionTransform, SVG_Motion_datatype, 0, GF_XMLNS_SVG },
	/*SMIL anim fill*/
	{ "fill", TAG_SVG_ATT_smil_fill, SMIL_Fill_datatype, GF_SVG_ATTOPT_SMIL, GF_XMLNS_SVG },
	/*regular paint fill*/
	{ "fill", TAG_SVG_ATT_fill, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	/*filter*/
	{ "filter", TAG_SVG_ATT_filter, SVG_Paint_datatype, 0, GF_XMLNS_SVG },
	/*text rotate*/
	{ "rotate", TAG_SVG_ATT_text_rotate, SVG_Numbers_datatype, GF_SVG_ATTOPT_TEXT, GF_XMLNS_SVG },
	/*regular matrix rotate*/
	{ "rotate", TAG_SVG_ATT_rotate, SVG_Rotate_datatype, 0, GF_XMLNS_SVG },
	/*SMIL anim type*/
	{ "type", TAG_SVG_ATT_transform_type, SVG_TransformType_datatype, GF_SVG_ATTOPT_SMIL, GF_XMLNS_SVG },
	/*Filter componentTransfer type*/
	{ "type", TAG_SVG_ATT_filter_transfer_type, SVG_Filter_TransferType_datatype, GF_SVG_ATTOPT_FILTER, GF_XMLNS_SVG },
	/*regular content type*/
	{ "type", TAG_SVG_ATT_type, SVG_ContentType_datatype, 0, GF_XMLNS_SVG },
	/*text x*/
	{ "x", TAG_SVG_ATT_text_x, SVG_Coordinates_datatype, GF_SVG_ATTOPT_TEXT, GF_XMLNS_SVG },
	/*regular x position*/
	{ "x", TAG_SVG_ATT_x, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },
	/*text y*/
	{ "y", TAG_SVG_ATT_text_y, SVG_Coordinates_datatype, GF_SVG_ATTOPT_TEXT, GF_XMLNS_SVG },
	/*regular y position*/
	{ "y", TAG_SVG_ATT_y, SVG_Coordinate_datatype, 0, GF_XMLNS_SVG },

	/*filters*/
	{ "tableValues", TAG_SVG_ATT_filter_table_values, SVG_Numbers_datatype, 0, GF_XMLNS_SVG },
	{ "intercept", TAG_SVG_ATT_filter_intercept, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "amplitude", TAG_SVG_ATT_filter_amplitude, SVG_Number_datatype, 0, GF_XMLNS_SVG },
	{ "exponent", TAG_SVG_ATT_filter_exponent, SVG_Number_datatype, 0, GF_XMLNS_SVG },

	/*LASeR*/
	{ "enabled", TAG_LSR_ATT_enabled, SVG_Boolean_datatype, 0, GF_XMLNS_LASER },
	/*cursor x*/
	{ "x", TAG_SVG_ATT_cursorManager_x, SVG_Length_datatype, GF_SVG_ATTOPT_CURSOR, GF_XMLNS_LASER },
	/*cursor y*/
	{ "y", TAG_SVG_ATT_cursorManager_y, SVG_Length_datatype, GF_SVG_ATTOPT_CURSOR, GF_XMLNS_LASER },

	/*XBL*/
	{ "id", TAG_XBL_ATT_id, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "extends", TAG_XBL_ATT_extends, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "display", TAG_XBL_ATT_display, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "inheritstyle", TAG_XBL_ATT_inheritstyle, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "includes", TAG_XBL_ATT_includes, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "name", TAG_XBL_ATT_name, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "implements", TAG_XBL_ATT_implements, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "type", TAG_XBL_ATT_type, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "readonly", TAG_XBL_ATT_readonly, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "onget", TAG_XBL_ATT_onget, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "onset", TAG_XBL_ATT_onset, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "event", TAG_XBL_ATT_event, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "action", TAG_XBL_ATT_action, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "phase", TAG_XBL_ATT_phase, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "button", TAG_XBL_ATT_button, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "modifiers", TAG_XBL_ATT_modifiers, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "keycode", TAG_XBL_ATT_keycode, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "key", TAG_XBL_ATT_key, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "charcode", TAG_XBL_ATT_charcode, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "clickcount", TAG_XBL_ATT_clickcount, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "command", TAG_XBL_ATT_command, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "preventdefault", TAG_XBL_ATT_preventdefault, DOM_String_datatype, 0, GF_XMLNS_XBL },
	{ "src", TAG_XBL_ATT_src, DOM_String_datatype, 0, GF_XMLNS_XBL },

	/*GPAC SVG Extensions*/
	{ "use-as-primary", TAG_GSVG_ATT_useAsPrimary, SVG_Boolean_datatype, 0, GF_XMLNS_SVG_GPAC_EXTENSION},
	{ "depthOffset", TAG_GSVG_ATT_depthOffset, SVG_Number_datatype, 0, GF_XMLNS_SVG_GPAC_EXTENSION},
	{ "depthGain", TAG_GSVG_ATT_depthGain, SVG_Number_datatype, 0, GF_XMLNS_SVG_GPAC_EXTENSION},

};

void gf_xml_push_namespaces(GF_DOMNode *elt)
{
	GF_DOMAttribute *att = elt->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5)) {
				char *qname = datt->name+5;
				gf_sg_add_namespace(elt->sgprivate->scenegraph, *(DOM_String *) datt->data, qname[0] ? qname+1 : NULL);
			}
		}
		att = att->next;
	}
}

void gf_xml_pop_namespaces(GF_DOMNode *elt)
{
	GF_DOMAttribute *att = elt->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5)) {
				char *qname = datt->name+5;
				gf_sg_remove_namespace(elt->sgprivate->scenegraph, *(DOM_String *) datt->data, qname[0] ? qname+1 : NULL);
			}
		}
		att = att->next;
	}
}


static u32 gf_xml_get_namespace(GF_DOMNode *elt, const char *attribute_name)
{
	GF_DOMAttribute *att = elt->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5) && !strcmp(datt->name+6, attribute_name)) {
				return gf_xml_get_namespace_id(*(DOM_String *)  datt->data);
			}
		}
		att = att->next;
	}
	if (!elt->sgprivate->parents) return 0;
	return gf_xml_get_namespace((GF_DOMNode*)elt->sgprivate->parents->node, attribute_name);
}

static char *gf_xml_get_namespace_qname(GF_DOMNode *elt, u32 ns)
{
	GF_DOMAttribute *att = elt->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5) && (gf_xml_get_namespace_id(*(DOM_String *)  datt->data)==ns)) {
				if (datt->name[5]) return datt->name+6;
				return NULL;
			}
		}
		att = att->next;
	}
	if (!elt->sgprivate->parents) return NULL;
	return gf_xml_get_namespace_qname((GF_DOMNode*)elt->sgprivate->parents->node, ns);
}

u32 gf_xml_get_attribute_tag(GF_Node *elt, char *attribute_name, u32 ns)
{
	char *ns_sep;
	u32 i, count;
	count = sizeof(xml_attributes) / sizeof(struct xml_att_def);

	if (!ns) {
		ns_sep = strchr(attribute_name, ':');
		if (ns_sep) {
			ns_sep[0] = 0;
			ns = gf_sg_get_namespace_code(elt->sgprivate->scenegraph, attribute_name);
			if (ns==GF_XMLNS_UNDEFINED) ns = gf_xml_get_namespace((GF_DOMNode*)elt, attribute_name);
			ns_sep[0] = ':';
			attribute_name = ++ns_sep;
		} else {
			ns = gf_xml_get_element_namespace(elt);
			if (!ns) ns = gf_sg_get_namespace_code(elt->sgprivate->scenegraph, NULL);
		}
	}

	for (i=0; i<count; i++) {
		if (strcmp(xml_attributes[i].name, attribute_name)) continue;
		if (xml_attributes[i].xmlns != ns) continue;

		switch (xml_attributes[i].opts) {
		case GF_SVG_ATTOPT_SMIL:
			switch (elt->sgprivate->tag) {
			case TAG_SVG_animate:
			case TAG_SVG_animateColor:
			case TAG_SVG_animateMotion:
			case TAG_SVG_animateTransform:
			case TAG_SVG_animation:
			case TAG_SVG_audio:
			case TAG_SVG_video:
			case TAG_SVG_set:
				return xml_attributes[i].tag;
			default:
				break;
			}
			break;
		case GF_SVG_ATTOPT_TEXT:
			if (elt->sgprivate->tag == TAG_SVG_text)
				return xml_attributes[i].tag;
			break;
		case GF_SVG_ATTOPT_CURSOR:
			if (elt->sgprivate->tag == TAG_LSR_cursorManager)
				return xml_attributes[i].tag;
			break;
		case GF_SVG_ATTOPT_LISTENER:
			if (elt->sgprivate->tag == TAG_SVG_listener)
				return xml_attributes[i].tag;
			break;
		case GF_SVG_ATTOPT_FILTER:
			switch (elt->sgprivate->tag) {
			case TAG_SVG_filter:
			case TAG_SVG_feDistantLight:
			case TAG_SVG_fePointLight:
			case TAG_SVG_feSpotLight:
			case TAG_SVG_feBlend:
			case TAG_SVG_feColorMatrix:
			case TAG_SVG_feComponentTransfer:
			case TAG_SVG_feFuncR:
			case TAG_SVG_feFuncG:
			case TAG_SVG_feFuncB:
			case TAG_SVG_feFuncA:
			case TAG_SVG_feComposite:
			case TAG_SVG_feConvolveMatrix:
			case TAG_SVG_feDiffuseLighting:
			case TAG_SVG_feDisplacementMap:
			case TAG_SVG_feFlood:
			case TAG_SVG_feGaussianBlur:
			case TAG_SVG_feImage:
			case TAG_SVG_feMerge:
			case TAG_SVG_feMorphology:
			case TAG_SVG_feOffset:
			case TAG_SVG_feSpecularLighting:
			case TAG_SVG_feTile:
			case TAG_SVG_feTurbulence:
				return xml_attributes[i].tag;
			default:
				break;
			}
			break;
		default:
			return xml_attributes[i].tag;
		}
	}
	return TAG_DOM_ATT_any;
}

u32 gf_xml_get_attribute_type(u32 tag)
{
	u32 i, count;
	count = sizeof(xml_attributes) / sizeof(struct xml_att_def);
	for (i=0; i<count; i++) {
		if (xml_attributes[i].tag==tag) return xml_attributes[i].type;
	}
	return DOM_String_datatype;
}

const char*gf_svg_get_attribute_name(GF_Node *node, u32 tag)
{
	u32 i, count, ns;

	//ns = gf_sg_get_namespace_code(node->sgprivate->scenegraph, NULL);
	ns = gf_xml_get_element_namespace(node);
	count = sizeof(xml_attributes) / sizeof(struct xml_att_def);
	for (i=0; i<count; i++) {
		if (xml_attributes[i].tag==tag) {
			char *xmlns;
			if (ns == xml_attributes[i].xmlns)
				return xml_attributes[i].name;

			xmlns = (char *) gf_xml_get_namespace_qname((GF_DOMNode*)node, xml_attributes[i].xmlns);
			if (xmlns) {
				sprintf(node->sgprivate->scenegraph->szNameBuffer, "%s:%s", xmlns, xml_attributes[i].name);
				return node->sgprivate->scenegraph->szNameBuffer;
			}
			return xml_attributes[i].name;
		}
	}
	return 0;
}
GF_DOMAttribute *gf_xml_create_attribute(GF_Node *node, u32 tag)
{
	u32 type = gf_xml_get_attribute_type(tag);
	return gf_node_create_attribute_from_datatype(type, tag);
}

static const struct xml_elt_def {
	const char *name;
	u32 tag;
	u32 xmlns;
} xml_elements [] =
{
	{ "listener", TAG_SVG_listener, GF_XMLNS_XMLEV},
	/*SVG*/
	{ "a", TAG_SVG_a, GF_XMLNS_SVG },
	{ "animate", TAG_SVG_animate, GF_XMLNS_SVG },
	{ "animateColor", TAG_SVG_animateColor, GF_XMLNS_SVG },
	{ "animateMotion", TAG_SVG_animateMotion, GF_XMLNS_SVG },
	{ "animateTransform", TAG_SVG_animateTransform, GF_XMLNS_SVG },
	{ "animation", TAG_SVG_animation, GF_XMLNS_SVG },
	{ "audio", TAG_SVG_audio, GF_XMLNS_SVG },
	{ "circle", TAG_SVG_circle, GF_XMLNS_SVG },
	{ "defs", TAG_SVG_defs, GF_XMLNS_SVG },
	{ "desc", TAG_SVG_desc, GF_XMLNS_SVG },
	{ "discard", TAG_SVG_discard, GF_XMLNS_SVG },
	{ "ellipse", TAG_SVG_ellipse, GF_XMLNS_SVG },
	{ "font", TAG_SVG_font, GF_XMLNS_SVG },
	{ "font-face", TAG_SVG_font_face, GF_XMLNS_SVG },
	{ "font-face-src", TAG_SVG_font_face_src, GF_XMLNS_SVG },
	{ "font-face-uri", TAG_SVG_font_face_uri, GF_XMLNS_SVG },
	{ "foreignObject", TAG_SVG_foreignObject, GF_XMLNS_SVG },
	{ "g", TAG_SVG_g, GF_XMLNS_SVG },
	{ "glyph", TAG_SVG_glyph, GF_XMLNS_SVG },
	{ "handler", TAG_SVG_handler, GF_XMLNS_SVG },
	{ "hkern", TAG_SVG_hkern, GF_XMLNS_SVG },
	{ "image", TAG_SVG_image, GF_XMLNS_SVG },
	{ "line", TAG_SVG_line, GF_XMLNS_SVG },
	{ "linearGradient", TAG_SVG_linearGradient, GF_XMLNS_SVG },
	{ "metadata", TAG_SVG_metadata, GF_XMLNS_SVG },
	{ "missing-glyph", TAG_SVG_missing_glyph, GF_XMLNS_SVG },
	{ "mpath", TAG_SVG_mpath, GF_XMLNS_SVG },
	{ "path", TAG_SVG_path, GF_XMLNS_SVG },
	{ "polygon", TAG_SVG_polygon, GF_XMLNS_SVG },
	{ "polyline", TAG_SVG_polyline, GF_XMLNS_SVG },
	{ "prefetch", TAG_SVG_prefetch, GF_XMLNS_SVG },
	{ "radialGradient", TAG_SVG_radialGradient, GF_XMLNS_SVG },
	{ "rect", TAG_SVG_rect, GF_XMLNS_SVG },
	{ "script", TAG_SVG_script, GF_XMLNS_SVG },
	{ "set", TAG_SVG_set, GF_XMLNS_SVG },
	{ "solidColor", TAG_SVG_solidColor, GF_XMLNS_SVG },
	{ "stop", TAG_SVG_stop, GF_XMLNS_SVG },
	{ "svg", TAG_SVG_svg, GF_XMLNS_SVG },
	{ "switch", TAG_SVG_switch, GF_XMLNS_SVG },
	{ "tbreak", TAG_SVG_tbreak, GF_XMLNS_SVG },
	{ "text", TAG_SVG_text, GF_XMLNS_SVG },
	{ "textArea", TAG_SVG_textArea, GF_XMLNS_SVG },
	{ "title", TAG_SVG_title, GF_XMLNS_SVG },
	{ "tspan", TAG_SVG_tspan, GF_XMLNS_SVG },
	{ "use", TAG_SVG_use, GF_XMLNS_SVG },
	{ "video", TAG_SVG_video, GF_XMLNS_SVG },
	{ "filter", TAG_SVG_filter, GF_XMLNS_SVG },
	{ "feDistantLight", TAG_SVG_feDistantLight, GF_XMLNS_SVG },
	{ "fePointLight", TAG_SVG_fePointLight, GF_XMLNS_SVG },
	{ "feSpotLight", TAG_SVG_feSpotLight, GF_XMLNS_SVG },
	{ "feBlend", TAG_SVG_feBlend, GF_XMLNS_SVG },
	{ "feColorMatrix", TAG_SVG_feColorMatrix, GF_XMLNS_SVG },
	{ "feComponentTransfer", TAG_SVG_feComponentTransfer, GF_XMLNS_SVG },
	{ "feFuncR", TAG_SVG_feFuncR, GF_XMLNS_SVG },
	{ "feFuncG", TAG_SVG_feFuncG, GF_XMLNS_SVG },
	{ "feFuncB", TAG_SVG_feFuncB, GF_XMLNS_SVG },
	{ "feFuncA", TAG_SVG_feFuncA, GF_XMLNS_SVG },
	{ "feComposite", TAG_SVG_feComposite, GF_XMLNS_SVG },
	{ "feConvolveMatrix", TAG_SVG_feConvolveMatrix, GF_XMLNS_SVG },
	{ "feDiffuseLighting", TAG_SVG_feDiffuseLighting, GF_XMLNS_SVG },
	{ "feDisplacementMap", TAG_SVG_feDisplacementMap, GF_XMLNS_SVG },
	{ "feFlood", TAG_SVG_feFlood, GF_XMLNS_SVG },
	{ "feGaussianBlur", TAG_SVG_feGaussianBlur, GF_XMLNS_SVG },
	{ "feImage", TAG_SVG_feImage, GF_XMLNS_SVG },
	{ "feMerge", TAG_SVG_feMerge, GF_XMLNS_SVG },
	{ "feMorphology", TAG_SVG_feMorphology, GF_XMLNS_SVG },
	{ "feOffset", TAG_SVG_feOffset, GF_XMLNS_SVG },
	{ "feSpecularLighting", TAG_SVG_feSpecularLighting, GF_XMLNS_SVG },
	{ "feTile", TAG_SVG_feTile, GF_XMLNS_SVG },
	{ "feTurbulence", TAG_SVG_feTurbulence, GF_XMLNS_SVG },

	/*LASeR*/
	{ "conditional", TAG_LSR_conditional, GF_XMLNS_LASER },
	{ "rectClip", TAG_LSR_rectClip, GF_XMLNS_LASER },
	{ "cursorManager", TAG_LSR_cursorManager, GF_XMLNS_LASER },
	{ "selector", TAG_LSR_selector, GF_XMLNS_LASER },
	{ "simpleLayout", TAG_LSR_simpleLayout, GF_XMLNS_LASER },
	{ "updates", TAG_LSR_updates, GF_XMLNS_LASER },

	/*XBL*/
	{ "bindings", TAG_XBL_bindings, GF_XMLNS_XBL },
	{ "binding", TAG_XBL_binding, GF_XMLNS_XBL },
	{ "content", TAG_XBL_content, GF_XMLNS_XBL },
	{ "children", TAG_XBL_children, GF_XMLNS_XBL },
	{ "implementation", TAG_XBL_implementation, GF_XMLNS_XBL },
	{ "constructor", TAG_XBL_constructor, GF_XMLNS_XBL },
	{ "destructor", TAG_XBL_destructor, GF_XMLNS_XBL },
	{ "field", TAG_XBL_field, GF_XMLNS_XBL },
	{ "property", TAG_XBL_property, GF_XMLNS_XBL },
	{ "getter", TAG_XBL_getter, GF_XMLNS_XBL },
	{ "setter", TAG_XBL_setter, GF_XMLNS_XBL },
	{ "method", TAG_XBL_method, GF_XMLNS_XBL },
	{ "parameter", TAG_XBL_parameter, GF_XMLNS_XBL },
	{ "body", TAG_XBL_body, GF_XMLNS_XBL },
	{ "handlers", TAG_XBL_handlers, GF_XMLNS_XBL },
	{ "handler", TAG_XBL_handler, GF_XMLNS_XBL },
	{ "resources", TAG_XBL_resources, GF_XMLNS_XBL },
	{ "stylesheet", TAG_XBL_stylesheet, GF_XMLNS_XBL },
	{ "image", TAG_XBL_image, GF_XMLNS_XBL },
};

GF_EXPORT
u32 gf_xml_get_element_tag(const char *element_name, u32 ns)
{
	u32 i, count;
	if (!element_name) return TAG_UndefinedNode;
	count = sizeof(xml_elements) / sizeof(struct xml_elt_def);
	for (i=0; i<count; i++) {
		if (!strcmp(xml_elements[i].name, element_name)) {
			if (!ns || (xml_elements[i].xmlns==ns) )
				return xml_elements[i].tag;
		}
	}
	return TAG_UndefinedNode;
}

const char *gf_xml_get_element_name(GF_Node *n)
{
	u32 i, count, ns;

	ns = n ? gf_sg_get_namespace_code(n->sgprivate->scenegraph, NULL) : 0;
	count = sizeof(xml_elements) / sizeof(struct xml_elt_def);
	for (i=0; i<count; i++) {
		if (n && n->sgprivate && (n->sgprivate->tag==xml_elements[i].tag)) {
			char *xmlns;
			if (!n || (ns == xml_elements[i].xmlns))
				return xml_elements[i].name;

			xmlns = (char *) gf_sg_get_namespace_qname(n->sgprivate->scenegraph, xml_elements[i].xmlns);
			if (xmlns) {
				sprintf(n->sgprivate->scenegraph->szNameBuffer, "%s:%s", xmlns, xml_elements[i].name);
				return n->sgprivate->scenegraph->szNameBuffer;
			}
			return xml_elements[i].name;
		}
	}
	return "UndefinedNode";
}

GF_NamespaceType gf_xml_get_element_namespace(GF_Node *n)
{
	u32 i, count;
	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *elt = (GF_DOMFullNode *)n;
		return elt->ns;
	}

	count = sizeof(xml_elements) / sizeof(struct xml_elt_def);
	for (i=0; i<count; i++) {
		if (n->sgprivate->tag==xml_elements[i].tag) return xml_elements[i].xmlns;
	}
	return GF_XMLNS_UNDEFINED;
}

u32 gf_node_get_attribute_count(GF_Node *node)
{
	u32 count = 0;
	GF_DOMNode *dom = (GF_DOMNode *)node;
	GF_DOMAttribute *atts = dom->attributes;
	while (atts) {
		count++;
		atts = atts->next;
	}
	return count;
}

GF_Err gf_node_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	GF_DOMNode *dom = (GF_DOMNode *)node;
	GF_DOMAttribute *atts = dom->attributes;
	while (atts) {
		if (atts->tag == info->fieldIndex) {
			info->fieldType = atts->data_type;
			info->far_ptr  = atts->data;
			return GF_OK;
		}
		atts = atts->next;
	}
	info->fieldType = 0;
	info->far_ptr  = NULL;
	return GF_NOT_SUPPORTED;
}

void gf_node_delete_attributes(GF_Node *node)
{
	GF_DOMAttribute *tmp;
	GF_DOMAttribute *att = ((GF_DOMNode*)node)->attributes;
	while(att) {
		gf_svg_delete_attribute_value(att->data_type, att->data, node->sgprivate->scenegraph);
		tmp = att;
		att = att->next;
		if (tmp->tag==TAG_DOM_ATT_any) {
			gf_free( ((GF_DOMFullAttribute*)tmp)->name);
		}
		gf_free(tmp);
	}
}

SVGAttribute *gf_node_create_attribute_from_datatype(u32 data_type, u32 attribute_tag)
{
	SVGAttribute *att;
	if (!data_type) return NULL;

	GF_SAFEALLOC(att, SVGAttribute);
	if (!att) return NULL;
	att->data_type = (u16) data_type;
	att->tag = (u16) attribute_tag;
	att->data = gf_svg_create_attribute_value(att->data_type);
	return att;
}

GF_EXPORT
GF_Err gf_node_get_attribute_by_name(GF_Node *node, char *name, u32 xmlns_code, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	u32 attribute_tag = gf_xml_get_attribute_tag(node, name, xmlns_code);
	if (attribute_tag == TAG_DOM_ATT_any) {
		u32 len = 0;
		const char *ns = NULL;
		SVGAttribute *last_att = NULL;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute *) ((SVG_Element*)node)->attributes;
		if (xmlns_code) ns = gf_sg_get_namespace_qname(node->sgprivate->scenegraph, xmlns_code);
		if (ns) len = (u32) strlen(ns);

		while (att) {
			if (((u32) att->tag == TAG_DOM_ATT_any) &&
			        ((!ns && !strcmp(name, att->name)) || (ns && !strncmp(att->name, ns, len) && !strcmp(att->name+len+1, name)))
			   ) {
				field->fieldIndex = att->tag;
				field->fieldType = att->data_type;
				field->far_ptr = att->data;
				return GF_OK;
			}
			last_att = (SVGAttribute *) att;
			att = (GF_DOMFullAttribute *) att->next;
		}
		if (create_if_not_found) {
			GF_SAFEALLOC(att, GF_DOMFullAttribute);
			if (!att) return GF_OUT_OF_MEM;
			att->data_type = (u16) DOM_String_datatype;
			att->tag = (u16) TAG_DOM_ATT_any;
			att->data = gf_svg_create_attribute_value(att->data_type);

			att->name = gf_strdup(name);
			if (!xmlns_code)
				att->xmlns = gf_xml_get_element_namespace(node);
			else
				att->xmlns = xmlns_code;

			if (last_att) last_att->next = (SVGAttribute *)att;
			else ((SVG_Element*)node)->attributes = (SVGAttribute *)att;

			field->far_ptr = att->data;
			field->fieldType = att->data_type;
			field->fieldIndex = att->tag;
			return GF_OK;
		}
		return GF_NOT_SUPPORTED;
	}
	return gf_node_get_attribute_by_tag(node, attribute_tag, create_if_not_found, set_default, field);
}


static void attributes_set_default_value(GF_Node *node, SVGAttribute *att)
{
	u32 node_tag = node->sgprivate->tag;
	switch (att->tag) {
	case TAG_SVG_ATT_width:
	case TAG_SVG_ATT_height:
		if (node_tag == TAG_SVG_svg) {
			SVG_Length *len = att->data;
			len->type = SVG_NUMBER_PERCENTAGE;
			len->value = INT2FIX(100);
		}
		break;
	case TAG_SVG_ATT_x2:
		if (node_tag == TAG_SVG_linearGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE;
		}
		break;
	case TAG_SVG_ATT_cx:
	case TAG_SVG_ATT_cy:
	case TAG_SVG_ATT_fx:
	case TAG_SVG_ATT_fy:
	case TAG_SVG_ATT_r:
		if (node_tag == TAG_SVG_radialGradient) {
			((SVG_Number *)att->data)->value = FIX_ONE/2;
		}
		break;
	case TAG_SVG_ATT_dur:
		if (node_tag == TAG_SVG_video ||
		        node_tag == TAG_SVG_audio ||
		        node_tag == TAG_SVG_animation)
		{
			((SMIL_Duration *)att->data)->type = SMIL_DURATION_MEDIA;
		} else {
			/*is this correct?*/
			((SMIL_Duration *)att->data)->type = SMIL_DURATION_INDEFINITE;
		}
		break;
	case TAG_SVG_ATT_min:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_DEFINED;
		break;
	case TAG_SVG_ATT_repeatDur:
		((SMIL_Duration *)att->data)->type = SMIL_DURATION_INDEFINITE;
		break;
	case TAG_SVG_ATT_calcMode:
		if (node_tag == TAG_SVG_animateMotion)
			*((SMIL_CalcMode *)att->data) = SMIL_CALCMODE_PACED;
		else
			*((SMIL_CalcMode *)att->data) = SMIL_CALCMODE_LINEAR;
		break;
	case TAG_SVG_ATT_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_INHERIT;
		break;
	case TAG_SVG_ATT_solid_color:
	case TAG_SVG_ATT_stop_color:
		((SVG_Paint *)att->data)->type = SVG_PAINT_COLOR;
		((SVG_Paint *)att->data)->color.type = SVG_COLOR_RGBCOLOR;
		break;
	case TAG_SVG_ATT_opacity:
	case TAG_SVG_ATT_solid_opacity:
	case TAG_SVG_ATT_stop_opacity:
	case TAG_SVG_ATT_audio_level:
	case TAG_SVG_ATT_viewport_fill_opacity:
		((SVG_Number *)att->data)->value = FIX_ONE;
		break;
	case TAG_SVG_ATT_display:
		*((SVG_Display *)att->data) = SVG_DISPLAY_INLINE;
		break;
	case TAG_SVG_ATT_fill:
	case TAG_SVG_ATT_stroke:
		((SVG_Paint *)att->data)->type = SVG_PAINT_INHERIT;
		break;
	case TAG_SVG_ATT_stroke_dasharray:
		((SVG_StrokeDashArray *)att->data)->type = SVG_STROKEDASHARRAY_INHERIT;
		break;
	case TAG_SVG_ATT_fill_opacity:
	case TAG_SVG_ATT_stroke_opacity:
	case TAG_SVG_ATT_stroke_width:
	case TAG_SVG_ATT_font_size:
	case TAG_SVG_ATT_line_increment:
	case TAG_SVG_ATT_stroke_dashoffset:
	case TAG_SVG_ATT_stroke_miterlimit:
		((SVG_Number *)att->data)->type = SVG_NUMBER_INHERIT;
		break;
	case TAG_SVG_ATT_vector_effect:
		*((SVG_VectorEffect *)att->data) = SVG_VECTOREFFECT_NONE;
		break;
	case TAG_SVG_ATT_fill_rule:
		*((SVG_FillRule *)att->data) = SVG_FILLRULE_INHERIT;
		break;
	case TAG_SVG_ATT_font_weight:
		*((SVG_FontWeight *)att->data) = SVG_FONTWEIGHT_INHERIT;
		break;
	case TAG_SVG_ATT_visibility:
		*((SVG_Visibility *)att->data) = SVG_VISIBILITY_INHERIT;
		break;
	case TAG_SVG_ATT_smil_fill:
		*((SMIL_Fill *)att->data) = SMIL_FILL_REMOVE;
		break;
	case TAG_XMLEV_ATT_defaultAction:
		*((XMLEV_DefaultAction *)att->data) = XMLEVENT_DEFAULTACTION_PERFORM;
		break;
	case TAG_SVG_ATT_zoomAndPan:
		*((SVG_ZoomAndPan *)att->data) = SVG_ZOOMANDPAN_MAGNIFY;
		break;
	case TAG_SVG_ATT_stroke_linecap:
		*(SVG_StrokeLineCap*)att->data = SVG_STROKELINECAP_INHERIT;
		break;
	case TAG_SVG_ATT_stroke_linejoin:
		*(SVG_StrokeLineJoin*)att->data = SVG_STROKELINEJOIN_INHERIT;
		break;

	case TAG_SVG_ATT_transform:
		gf_mx2d_init(((SVG_Transform*)att->data)->mat);
		break;


	/*all default=0 values (don't need init)*/
	case TAG_SVG_ATT_font_family:
	case TAG_SVG_ATT_font_style:
	case TAG_SVG_ATT_text_anchor:
	case TAG_SVG_ATT_x:
	case TAG_SVG_ATT_y:
		break;

	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCENE, ("[Scene] Cannot create default value for SVG attribute %s\n", gf_svg_get_attribute_name(node, att->tag)));
	}
}

GF_EXPORT
GF_Err gf_node_get_attribute_by_tag(GF_Node *node, u32 attribute_tag, Bool create_if_not_found, Bool set_default, GF_FieldInfo *field)
{
	SVG_Element *elt = (SVG_Element *)node;
	SVGAttribute *att = elt->attributes;
	SVGAttribute *last_att = NULL;

	while (att) {
		if ((u32) att->tag == attribute_tag) {
			field->fieldIndex = att->tag;
			field->fieldType = att->data_type;
			field->far_ptr  = att->data;
			return GF_OK;
		}
		last_att = att;
		att = att->next;
	}

	if (create_if_not_found) {
		/* field not found create one */
		att = gf_xml_create_attribute(node, attribute_tag);
		if (att) {
			if (!elt->attributes) elt->attributes = att;
			else last_att->next = att;
			field->far_ptr = att->data;
			field->fieldType = att->data_type;
			field->fieldIndex = att->tag;
			/* attribute name should not be called, if needed use gf_svg_get_attribute_name(att->tag);*/
			field->name = NULL;
			if (set_default) attributes_set_default_value(node, att);
			return GF_OK;
		}
	}

	return GF_NOT_SUPPORTED;
}

GF_EXPORT
void gf_node_register_iri(GF_SceneGraph *sg, XMLRI *target)
{
#ifndef GPAC_DISABLE_SVG
	if (gf_list_find(sg->xlink_hrefs, target)<0) {
		gf_list_add(sg->xlink_hrefs, target);
	}
#endif
}

GF_EXPORT
void gf_node_unregister_iri(GF_SceneGraph *sg, XMLRI *target)
{
#ifndef GPAC_DISABLE_SVG
	gf_list_del_item(sg->xlink_hrefs, target);
#endif
}

GF_Node *gf_xml_node_clone(GF_SceneGraph *inScene, GF_Node *orig, GF_Node *cloned_parent, char *inst_id, Bool deep)
{
	GF_DOMAttribute *att;
	GF_Node *clone = gf_node_new(inScene, orig->sgprivate->tag);
	if (!clone) return NULL;

	if (orig->sgprivate->tag == TAG_DOMText) {
		GF_DOMText *n_src,*n_dst;
		n_src = (GF_DOMText *)orig;
		n_dst = (GF_DOMText *)clone;
		n_dst->type = n_src->type;
		n_dst->textContent = gf_strdup(n_src->textContent);
	} else {
		if (orig->sgprivate->tag == TAG_DOMFullNode) {
			GF_DOMFullNode *n_src,*n_dst;
			n_src = (GF_DOMFullNode *)orig;
			n_dst = (GF_DOMFullNode *)clone;
			n_dst->ns = n_src->ns;
			n_dst->name = gf_strdup(n_dst->name);
		}

		att = ((GF_DOMNode *)orig)->attributes;
		while (att) {
			GF_FieldInfo dst, src;
			/*create by name*/
			if (att->tag==TAG_DOM_ATT_any) {
				gf_node_get_attribute_by_name(clone, ((GF_DOMFullAttribute*)att)->name, 0, 1, 0, &dst);
			} else {
				gf_node_get_attribute_by_tag(clone, att->tag, 1, 0, &dst);
			}
			src.far_ptr = att->data;
			src.fieldType = att->data_type;
			src.fieldIndex = att->tag;
			gf_svg_attributes_copy(&dst, &src, 0);
			if (att->tag==TAG_XLINK_ATT_href) {
				XMLRI *iri = (XMLRI *)att->data;
				if (iri->target == gf_node_get_parent(orig, 0)) {
					((XMLRI *)dst.far_ptr)->target = cloned_parent;
				} else {
					((XMLRI *)dst.far_ptr)->target = NULL;
				}
			}
			att = att->next;
		}
	}
	if (cloned_parent) {
		gf_node_list_add_child( & ((GF_ParentNode*)cloned_parent)->children, clone);
		gf_node_register(clone, cloned_parent);
		/*TO CLARIFY: can we init the node right now or should we wait for insertion in the scene tree ?*/
		gf_node_init(clone);
	}
	if (deep) {
		GF_ChildNodeItem *child = ((GF_ParentNode *)orig)->children;
		while (child) {
			gf_node_clone(inScene, child->node, clone, inst_id, 1);
			child = child->next;
		}
	}
	return clone;
}

/*TODO FIXME, this is ugly, add proper cache system*/
#include <gpac/base_coding.h>


static u32 check_existing_file(char *base_file, char *ext, char *data, u32 data_size, u32 idx)
{
	char szFile[GF_MAX_PATH];
	u64 fsize;
	FILE *f;

	sprintf(szFile, "%s%04X%s", base_file, idx, ext);

	f = gf_fopen(szFile, "rb");
	if (!f) return 0;

	gf_fseek(f, 0, SEEK_END);
	fsize = gf_ftell(f);
	if (fsize==data_size) {
		u32 offset=0;
		char cache[1024];
		gf_fseek(f, 0, SEEK_SET);
		while (fsize) {
			u32 read = (u32) fread(cache, 1, 1024, f);
			if ((s32) read < 0) return 0;
			fsize -= read;
			if (memcmp(cache, data+offset, sizeof(char)*read)) break;
			offset+=read;
		}
		gf_fclose(f);
		f = NULL;
		/*same file*/
		if (!fsize) return 2;
	}
	if (f)
		gf_fclose(f);
	return 1;
}


GF_EXPORT
GF_Err gf_node_store_embedded_data(XMLRI *iri, const char *cache_dir, const char *base_filename)
{
	char szFile[GF_MAX_PATH], buf[20], *sep, *data, *ext;
	u32 data_size, idx;
	Bool existing;
	FILE *f;

	if (!cache_dir || !base_filename || !iri || !iri->string || strncmp(iri->string, "data:", 5)) return GF_OK;

	/*handle "data:" scheme when cache is specified*/
	strcpy(szFile, cache_dir);
	data_size = (u32) strlen(szFile);
	if (szFile[data_size-1] != GF_PATH_SEPARATOR) {
		szFile[data_size] = GF_PATH_SEPARATOR;
		szFile[data_size+1] = 0;
	}
	if (base_filename) {
		sep = strrchr(base_filename, GF_PATH_SEPARATOR);
#ifdef WIN32
		if (!sep) sep = strrchr(base_filename, '/');
#endif
		if (!sep) sep = (char *) base_filename;
		else sep += 1;
		strcat(szFile, sep);
	}
	sep = strrchr(szFile, '.');
	if (sep) sep[0] = 0;
	strcat(szFile, "_img_");

	/*get mime type*/
	sep = (char *)iri->string + 5;
	if (!strncmp(sep, "image/jpg", 9) || !strncmp(sep, "image/jpeg", 10)) ext = ".jpg";
	else if (!strncmp(sep, "image/png", 9)) ext = ".png";
	else if (!strncmp(sep, "image/svg+xml", 13)) ext = ".svg";
	else return GF_BAD_PARAM;


	data = NULL;
	sep = strchr(iri->string, ';');
	if (!strncmp(sep, ";base64,", 8)) {
		sep += 8;
		data_size = 2 * (u32) strlen(sep);
		data = (char*)gf_malloc(sizeof(char)*data_size);
		if (!data) return GF_OUT_OF_MEM;
		data_size = gf_base64_decode(sep, (u32) strlen(sep), data, data_size);
	}
	else if (!strncmp(sep, ";base16,", 8)) {
		data_size = 2 * (u32) strlen(sep);
		data = (char*)gf_malloc(sizeof(char)*data_size);
		if (!data) return GF_OUT_OF_MEM;
		sep += 8;
		data_size = gf_base16_decode(sep, (u32) strlen(sep), data, data_size);
	}
	if (!data_size) return GF_OK;

	iri->type = XMLRI_STRING;

	existing = 0;
	idx = 0;
	while (1) {
		u32 res = check_existing_file(szFile, ext, data, data_size, idx);
		if (!res) break;
		if (res==2) {
			existing = 1;
			break;
		}
		idx++;
	}
	sprintf(buf, "%04X", idx);
	strcat(szFile, buf);
	strcat(szFile, ext);

	if (!existing) {
		f = gf_fopen(szFile, "wb");
		if (!f) {
			gf_free(data);
			gf_free(iri->string);
			iri->string = NULL;
			return GF_IO_ERR;
		}
		gf_fwrite(data, data_size, 1, f);
		gf_fclose(f);
	}
	gf_free(data);
	gf_free(iri->string);
	iri->string = gf_strdup(szFile);
	return GF_OK;
}


#endif /*GPAC_DISABLE_SVG*/

