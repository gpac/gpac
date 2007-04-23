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

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>

#include <gpac/nodes_svg_da.h>

u32 gf_svg3_get_attribute_tag(u32 element_tag, const char *attribute_name)
{
	if (!attribute_name) return TAG_SVG3_ATT_Unknown;
	if (!stricmp(attribute_name, "id")) return TAG_SVG3_ATT_id;
	if (!stricmp(attribute_name, "class")) return TAG_SVG3_ATT__class;
	if (!stricmp(attribute_name, "xml:id")) return TAG_SVG3_ATT_xml_id;
	if (!stricmp(attribute_name, "xml:base")) return TAG_SVG3_ATT_xml_base;
	if (!stricmp(attribute_name, "xml:lang")) return TAG_SVG3_ATT_xml_lang;
	if (!stricmp(attribute_name, "xml:space")) return TAG_SVG3_ATT_xml_space;
	if (!stricmp(attribute_name, "requiredFeatures")) return TAG_SVG3_ATT_requiredFeatures;
	if (!stricmp(attribute_name, "requiredExtensions")) return TAG_SVG3_ATT_requiredExtensions;
	if (!stricmp(attribute_name, "requiredFormats")) return TAG_SVG3_ATT_requiredFormats;
	if (!stricmp(attribute_name, "requiredFonts")) return TAG_SVG3_ATT_requiredFonts;
	if (!stricmp(attribute_name, "systemLanguage")) return TAG_SVG3_ATT_systemLanguage;
	if (!stricmp(attribute_name, "display")) return TAG_SVG3_ATT_display;
	if (!stricmp(attribute_name, "visibility")) return TAG_SVG3_ATT_visibility;
	if (!stricmp(attribute_name, "image-rendering")) return TAG_SVG3_ATT_image_rendering;
	if (!stricmp(attribute_name, "pointer-events")) return TAG_SVG3_ATT_pointer_events;
	if (!stricmp(attribute_name, "shape-rendering")) return TAG_SVG3_ATT_shape_rendering;
	if (!stricmp(attribute_name, "text-rendering")) return TAG_SVG3_ATT_text_rendering;
	if (!stricmp(attribute_name, "audio-level")) return TAG_SVG3_ATT_audio_level;
	if (!stricmp(attribute_name, "viewport-fill")) return TAG_SVG3_ATT_viewport_fill;
	if (!stricmp(attribute_name, "viewport-fill-opacity")) return TAG_SVG3_ATT_viewport_fill_opacity;
	if (!stricmp(attribute_name, "fill-opacity")) return TAG_SVG3_ATT_fill_opacity;
	if (!stricmp(attribute_name, "stroke-opacity")) return TAG_SVG3_ATT_stroke_opacity;
	if (!stricmp(attribute_name, "fill")) {
		if (element_tag == TAG_SVG3_animate || element_tag == TAG_SVG3_animateColor || element_tag == TAG_SVG3_animateMotion || element_tag == TAG_SVG3_animateTransform || element_tag == TAG_SVG3_animation || element_tag == TAG_SVG3_audio || element_tag == TAG_SVG3_video || element_tag == TAG_SVG3_set) return TAG_SVG3_ATT_smil_fill;
		else return TAG_SVG3_ATT_fill;
	}
	if (!stricmp(attribute_name, "fill-rule")) return TAG_SVG3_ATT_fill_rule;
	if (!stricmp(attribute_name, "stroke")) return TAG_SVG3_ATT_stroke;
	if (!stricmp(attribute_name, "stroke-dasharray")) return TAG_SVG3_ATT_stroke_dasharray;
	if (!stricmp(attribute_name, "stroke-dashoffset")) return TAG_SVG3_ATT_stroke_dashoffset;
	if (!stricmp(attribute_name, "stroke-linecap")) return TAG_SVG3_ATT_stroke_linecap;
	if (!stricmp(attribute_name, "stroke-linejoin")) return TAG_SVG3_ATT_stroke_linejoin;
	if (!stricmp(attribute_name, "stroke-miterlimit")) return TAG_SVG3_ATT_stroke_miterlimit;
	if (!stricmp(attribute_name, "stroke-width")) return TAG_SVG3_ATT_stroke_width;
	if (!stricmp(attribute_name, "color")) return TAG_SVG3_ATT_color;
	if (!stricmp(attribute_name, "color-rendering")) return TAG_SVG3_ATT_color_rendering;
	if (!stricmp(attribute_name, "vector-effect")) return TAG_SVG3_ATT_vector_effect;
	if (!stricmp(attribute_name, "solid-color")) return TAG_SVG3_ATT_solid_color;
	if (!stricmp(attribute_name, "solid-opacity")) return TAG_SVG3_ATT_solid_opacity;
	if (!stricmp(attribute_name, "display-align")) return TAG_SVG3_ATT_display_align;
	if (!stricmp(attribute_name, "line-increment")) return TAG_SVG3_ATT_line_increment;
	if (!stricmp(attribute_name, "stop-color")) return TAG_SVG3_ATT_stop_color;
	if (!stricmp(attribute_name, "stop-opacity")) return TAG_SVG3_ATT_stop_opacity;
	if (!stricmp(attribute_name, "font-family")) return TAG_SVG3_ATT_font_family;
	if (!stricmp(attribute_name, "font-size")) return TAG_SVG3_ATT_font_size;
	if (!stricmp(attribute_name, "font-style")) return TAG_SVG3_ATT_font_style;
	if (!stricmp(attribute_name, "font-variant")) return TAG_SVG3_ATT_font_variant;
	if (!stricmp(attribute_name, "font-weight")) return TAG_SVG3_ATT_font_weight;
	if (!stricmp(attribute_name, "text-anchor")) return TAG_SVG3_ATT_text_anchor;
	if (!stricmp(attribute_name, "text-align")) return TAG_SVG3_ATT_text_align;
	if (!stricmp(attribute_name, "focusHighlight")) return TAG_SVG3_ATT_focusHighlight;
	if (!stricmp(attribute_name, "externalResourcesRequired")) return TAG_SVG3_ATT_externalResourcesRequired;
	if (!stricmp(attribute_name, "focusable")) return TAG_SVG3_ATT_focusable;
	if (!stricmp(attribute_name, "nav-next")) return TAG_SVG3_ATT_nav_next;
	if (!stricmp(attribute_name, "nav-prev")) return TAG_SVG3_ATT_nav_prev;
	if (!stricmp(attribute_name, "nav-up")) return TAG_SVG3_ATT_nav_up;
	if (!stricmp(attribute_name, "nav-up-right")) return TAG_SVG3_ATT_nav_up_right;
	if (!stricmp(attribute_name, "nav-right")) return TAG_SVG3_ATT_nav_right;
	if (!stricmp(attribute_name, "nav-down-right")) return TAG_SVG3_ATT_nav_down_right;
	if (!stricmp(attribute_name, "nav-down")) return TAG_SVG3_ATT_nav_down;
	if (!stricmp(attribute_name, "nav-down-left")) return TAG_SVG3_ATT_nav_down_left;
	if (!stricmp(attribute_name, "nav-left")) return TAG_SVG3_ATT_nav_left;
	if (!stricmp(attribute_name, "nav-up-left")) return TAG_SVG3_ATT_nav_up_left;
	if (!stricmp(attribute_name, "transform")) return TAG_SVG3_ATT_transform;
	if (!stricmp(attribute_name, "xlink:type")) return TAG_SVG3_ATT_xlink_type;
	if (!stricmp(attribute_name, "xlink:role")) return TAG_SVG3_ATT_xlink_role;
	if (!stricmp(attribute_name, "xlink:arcrole")) return TAG_SVG3_ATT_xlink_arcrole;
	if (!stricmp(attribute_name, "xlink:title")) return TAG_SVG3_ATT_xlink_title;
	if (!stricmp(attribute_name, "xlink:href")) return TAG_SVG3_ATT_xlink_href;
	if (!stricmp(attribute_name, "xlink:show")) return TAG_SVG3_ATT_xlink_show;
	if (!stricmp(attribute_name, "xlink:actuate")) return TAG_SVG3_ATT_xlink_actuate;
	if (!stricmp(attribute_name, "target")) return TAG_SVG3_ATT_target;
	if (!stricmp(attribute_name, "attributeName")) return TAG_SVG3_ATT_attributeName;
	if (!stricmp(attribute_name, "attributeType")) return TAG_SVG3_ATT_attributeType;
	if (!stricmp(attribute_name, "begin")) return TAG_SVG3_ATT_begin;
	if (!stricmp(attribute_name, "lsr:enabled")) return TAG_SVG3_ATT_lsr_enabled;
	if (!stricmp(attribute_name, "dur")) return TAG_SVG3_ATT_dur;
	if (!stricmp(attribute_name, "end")) return TAG_SVG3_ATT_end;
	if (!stricmp(attribute_name, "repeatCount")) return TAG_SVG3_ATT_repeatCount;
	if (!stricmp(attribute_name, "repeatDur")) return TAG_SVG3_ATT_repeatDur;
	if (!stricmp(attribute_name, "restart")) return TAG_SVG3_ATT_restart;
	if (!stricmp(attribute_name, "min")) return TAG_SVG3_ATT_min;
	if (!stricmp(attribute_name, "max")) return TAG_SVG3_ATT_max;
	if (!stricmp(attribute_name, "to")) return TAG_SVG3_ATT_to;
	if (!stricmp(attribute_name, "calcMode")) return TAG_SVG3_ATT_calcMode;
	if (!stricmp(attribute_name, "values")) return TAG_SVG3_ATT_values;
	if (!stricmp(attribute_name, "keyTimes")) return TAG_SVG3_ATT_keyTimes;
	if (!stricmp(attribute_name, "keySplines")) return TAG_SVG3_ATT_keySplines;
	if (!stricmp(attribute_name, "from")) return TAG_SVG3_ATT_from;
	if (!stricmp(attribute_name, "by")) return TAG_SVG3_ATT_by;
	if (!stricmp(attribute_name, "additive")) return TAG_SVG3_ATT_additive;
	if (!stricmp(attribute_name, "accumulate")) return TAG_SVG3_ATT_accumulate;
	if (!stricmp(attribute_name, "path")) return TAG_SVG3_ATT_path;
	if (!stricmp(attribute_name, "keyPoints")) return TAG_SVG3_ATT_keyPoints;
	if (!stricmp(attribute_name, "rotate")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_rotate;
		else return TAG_SVG3_ATT_rotate;
	}
	if (!stricmp(attribute_name, "origin")) return TAG_SVG3_ATT_origin;
	if (!stricmp(attribute_name, "type")) {
		if (element_tag == TAG_SVG3_handler || element_tag == TAG_SVG3_audio || element_tag == TAG_SVG3_video || element_tag == TAG_SVG3_image || element_tag == TAG_SVG3_script) return TAG_SVG3_ATT_content_type;
		else return TAG_SVG3_ATT_type;
	}
	if (!stricmp(attribute_name, "clipBegin")) return TAG_SVG3_ATT_clipBegin;
	if (!stricmp(attribute_name, "clipEnd")) return TAG_SVG3_ATT_clipEnd;
	if (!stricmp(attribute_name, "syncBehavior")) return TAG_SVG3_ATT_syncBehavior;
	if (!stricmp(attribute_name, "syncTolerance")) return TAG_SVG3_ATT_syncTolerance;
	if (!stricmp(attribute_name, "syncMaster")) return TAG_SVG3_ATT_syncMaster;
	if (!stricmp(attribute_name, "syncReference")) return TAG_SVG3_ATT_syncReference;
	if (!stricmp(attribute_name, "x")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_x;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_x;
		else return TAG_SVG3_ATT_x;
	}
	if (!stricmp(attribute_name, "y")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_y;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_y;
		else return TAG_SVG3_ATT_y;
	}
	if (!stricmp(attribute_name, "width")) return TAG_SVG3_ATT_width;
	if (!stricmp(attribute_name, "height")) return TAG_SVG3_ATT_height;
	if (!stricmp(attribute_name, "preserveAspectRatio")) return TAG_SVG3_ATT_preserveAspectRatio;
	if (!stricmp(attribute_name, "initialVisibility")) return TAG_SVG3_ATT_initialVisibility;
	if (!stricmp(attribute_name, "cx")) return TAG_SVG3_ATT_cx;
	if (!stricmp(attribute_name, "cy")) return TAG_SVG3_ATT_cy;
	if (!stricmp(attribute_name, "r")) return TAG_SVG3_ATT_r;
	if (!stricmp(attribute_name, "enabled")) return TAG_SVG3_ATT_enabled;
	if (!stricmp(attribute_name, "x")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_cursorManager_x;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_x;
		else return TAG_SVG3_ATT_x;
	}
	if (!stricmp(attribute_name, "y")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_cursorManager_y;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_y;
		else return TAG_SVG3_ATT_y;
	}
	if (!stricmp(attribute_name, "rx")) return TAG_SVG3_ATT_rx;
	if (!stricmp(attribute_name, "ry")) return TAG_SVG3_ATT_ry;
	if (!stricmp(attribute_name, "horiz-adv-x")) return TAG_SVG3_ATT_horiz_adv_x;
	if (!stricmp(attribute_name, "horiz-origin-x")) return TAG_SVG3_ATT_horiz_origin_x;
	if (!stricmp(attribute_name, "font-stretch")) return TAG_SVG3_ATT_font_stretch;
	if (!stricmp(attribute_name, "unicode-range")) return TAG_SVG3_ATT_unicode_range;
	if (!stricmp(attribute_name, "panose-1")) return TAG_SVG3_ATT_panose_1;
	if (!stricmp(attribute_name, "widths")) return TAG_SVG3_ATT_widths;
	if (!stricmp(attribute_name, "bbox")) return TAG_SVG3_ATT_bbox;
	if (!stricmp(attribute_name, "units-per-em")) return TAG_SVG3_ATT_units_per_em;
	if (!stricmp(attribute_name, "stemv")) return TAG_SVG3_ATT_stemv;
	if (!stricmp(attribute_name, "stemh")) return TAG_SVG3_ATT_stemh;
	if (!stricmp(attribute_name, "slope")) return TAG_SVG3_ATT_slope;
	if (!stricmp(attribute_name, "cap-height")) return TAG_SVG3_ATT_cap_height;
	if (!stricmp(attribute_name, "x-height")) return TAG_SVG3_ATT_x_height;
	if (!stricmp(attribute_name, "accent-height")) return TAG_SVG3_ATT_accent_height;
	if (!stricmp(attribute_name, "ascent")) return TAG_SVG3_ATT_ascent;
	if (!stricmp(attribute_name, "descent")) return TAG_SVG3_ATT_descent;
	if (!stricmp(attribute_name, "ideographic")) return TAG_SVG3_ATT_ideographic;
	if (!stricmp(attribute_name, "alphabetic")) return TAG_SVG3_ATT_alphabetic;
	if (!stricmp(attribute_name, "mathematical")) return TAG_SVG3_ATT_mathematical;
	if (!stricmp(attribute_name, "hanging")) return TAG_SVG3_ATT_hanging;
	if (!stricmp(attribute_name, "underline-position")) return TAG_SVG3_ATT_underline_position;
	if (!stricmp(attribute_name, "underline-thickness")) return TAG_SVG3_ATT_underline_thickness;
	if (!stricmp(attribute_name, "strikethrough-position")) return TAG_SVG3_ATT_strikethrough_position;
	if (!stricmp(attribute_name, "strikethrough-thickness")) return TAG_SVG3_ATT_strikethrough_thickness;
	if (!stricmp(attribute_name, "overline-position")) return TAG_SVG3_ATT_overline_position;
	if (!stricmp(attribute_name, "overline-thickness")) return TAG_SVG3_ATT_overline_thickness;
	if (!stricmp(attribute_name, "d")) return TAG_SVG3_ATT_d;
	if (!stricmp(attribute_name, "unicode")) return TAG_SVG3_ATT_unicode;
	if (!stricmp(attribute_name, "glyph-name")) return TAG_SVG3_ATT_glyph_name;
	if (!stricmp(attribute_name, "arabic-form")) return TAG_SVG3_ATT_arabic_form;
	if (!stricmp(attribute_name, "lang")) return TAG_SVG3_ATT_lang;
	if (!stricmp(attribute_name, "ev:event")) return TAG_SVG3_ATT_ev_event;
	if (!stricmp(attribute_name, "u1")) return TAG_SVG3_ATT_u1;
	if (!stricmp(attribute_name, "g1")) return TAG_SVG3_ATT_g1;
	if (!stricmp(attribute_name, "u2")) return TAG_SVG3_ATT_u2;
	if (!stricmp(attribute_name, "g2")) return TAG_SVG3_ATT_g2;
	if (!stricmp(attribute_name, "k")) return TAG_SVG3_ATT_k;
	if (!stricmp(attribute_name, "opacity")) return TAG_SVG3_ATT_opacity;
	if (!stricmp(attribute_name, "x1")) return TAG_SVG3_ATT_x1;
	if (!stricmp(attribute_name, "y1")) return TAG_SVG3_ATT_y1;
	if (!stricmp(attribute_name, "x2")) return TAG_SVG3_ATT_x2;
	if (!stricmp(attribute_name, "y2")) return TAG_SVG3_ATT_y2;
	if (!stricmp(attribute_name, "gradientUnits")) return TAG_SVG3_ATT_gradientUnits;
	if (!stricmp(attribute_name, "spreadMethod")) return TAG_SVG3_ATT_spreadMethod;
	if (!stricmp(attribute_name, "gradientTransform")) return TAG_SVG3_ATT_gradientTransform;
	if (!stricmp(attribute_name, "event")) return TAG_SVG3_ATT_event;
	if (!stricmp(attribute_name, "phase")) return TAG_SVG3_ATT_phase;
	if (!stricmp(attribute_name, "propagate")) return TAG_SVG3_ATT_propagate;
	if (!stricmp(attribute_name, "defaultAction")) return TAG_SVG3_ATT_defaultAction;
	if (!stricmp(attribute_name, "observer")) return TAG_SVG3_ATT_observer;
	if (!stricmp(attribute_name, "target")) return TAG_SVG3_ATT_listener_target;
	if (!stricmp(attribute_name, "handler")) return TAG_SVG3_ATT_handler;
	if (!stricmp(attribute_name, "pathLength")) return TAG_SVG3_ATT_pathLength;
	if (!stricmp(attribute_name, "points")) return TAG_SVG3_ATT_points;
	if (!stricmp(attribute_name, "mediaSize")) return TAG_SVG3_ATT_mediaSize;
	if (!stricmp(attribute_name, "mediaTime")) return TAG_SVG3_ATT_mediaTime;
	if (!stricmp(attribute_name, "mediaCharacterEncoding")) return TAG_SVG3_ATT_mediaCharacterEncoding;
	if (!stricmp(attribute_name, "mediaContentEncodings")) return TAG_SVG3_ATT_mediaContentEncodings;
	if (!stricmp(attribute_name, "bandwidth")) return TAG_SVG3_ATT_bandwidth;
	if (!stricmp(attribute_name, "fx")) return TAG_SVG3_ATT_fx;
	if (!stricmp(attribute_name, "fy")) return TAG_SVG3_ATT_fy;
	if (!stricmp(attribute_name, "size")) return TAG_SVG3_ATT_size;
	if (!stricmp(attribute_name, "choice")) return TAG_SVG3_ATT_choice;
	if (!stricmp(attribute_name, "delta")) return TAG_SVG3_ATT_delta;
	if (!stricmp(attribute_name, "offset")) return TAG_SVG3_ATT_offset;
	if (!stricmp(attribute_name, "syncBehaviorDefault")) return TAG_SVG3_ATT_syncBehaviorDefault;
	if (!stricmp(attribute_name, "syncToleranceDefault")) return TAG_SVG3_ATT_syncToleranceDefault;
	if (!stricmp(attribute_name, "viewBox")) return TAG_SVG3_ATT_viewBox;
	if (!stricmp(attribute_name, "zoomAndPan")) return TAG_SVG3_ATT_zoomAndPan;
	if (!stricmp(attribute_name, "version")) return TAG_SVG3_ATT_version;
	if (!stricmp(attribute_name, "baseProfile")) return TAG_SVG3_ATT_baseProfile;
	if (!stricmp(attribute_name, "snapshotTime")) return TAG_SVG3_ATT_snapshotTime;
	if (!stricmp(attribute_name, "timelineBegin")) return TAG_SVG3_ATT_timelineBegin;
	if (!stricmp(attribute_name, "playbackOrder")) return TAG_SVG3_ATT_playbackOrder;
	if (!stricmp(attribute_name, "editable")) return TAG_SVG3_ATT_editable;
	if (!stricmp(attribute_name, "x")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_text_x;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_x;
		else return TAG_SVG3_ATT_x;
	}
	if (!stricmp(attribute_name, "y")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_text_y;
		else if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_y;
		else return TAG_SVG3_ATT_y;
	}
	if (!stricmp(attribute_name, "rotate")) {
		if (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_text_rotate;
		else return TAG_SVG3_ATT_rotate;
	}
	if (!stricmp(attribute_name, "transformBehavior")) return TAG_SVG3_ATT_transformBehavior;
	if (!stricmp(attribute_name, "overlay")) return TAG_SVG3_ATT_overlay;
	return TAG_SVG3_ATT_Unknown;
}

u32 gf_svg3_get_attribute_type_from_tag(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return SVG_ID_datatype;
		case TAG_SVG3_ATT__class: return SVG_String_datatype;
		case TAG_SVG3_ATT_xml_id: return SVG_ID_datatype;
		case TAG_SVG3_ATT_xml_base: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_xml_lang: return SVG_LanguageID_datatype;
		case TAG_SVG3_ATT_xml_space: return XML_Space_datatype;
		case TAG_SVG3_ATT_requiredFeatures: return SVG_ListOfIRI_datatype;
		case TAG_SVG3_ATT_requiredExtensions: return SVG_ListOfIRI_datatype;
		case TAG_SVG3_ATT_requiredFormats: return SVG_FormatList_datatype;
		case TAG_SVG3_ATT_requiredFonts: return SVG_FontList_datatype;
		case TAG_SVG3_ATT_systemLanguage: return SVG_LanguageIDs_datatype;
		case TAG_SVG3_ATT_display: return SVG_Display_datatype;
		case TAG_SVG3_ATT_visibility: return SVG_Visibility_datatype;
		case TAG_SVG3_ATT_image_rendering: return SVG_RenderingHint_datatype;
		case TAG_SVG3_ATT_pointer_events: return SVG_PointerEvents_datatype;
		case TAG_SVG3_ATT_shape_rendering: return SVG_RenderingHint_datatype;
		case TAG_SVG3_ATT_text_rendering: return SVG_RenderingHint_datatype;
		case TAG_SVG3_ATT_audio_level: return SVG_Number_datatype;
		case TAG_SVG3_ATT_viewport_fill: return SVG_Paint_datatype;
		case TAG_SVG3_ATT_viewport_fill_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_fill_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_stroke_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_fill: return SVG_Paint_datatype;
		case TAG_SVG3_ATT_fill_rule: return SVG_FillRule_datatype;
		case TAG_SVG3_ATT_stroke: return SVG_Paint_datatype;
		case TAG_SVG3_ATT_stroke_dasharray: return SVG_StrokeDashArray_datatype;
		case TAG_SVG3_ATT_stroke_dashoffset: return SVG_Length_datatype;
		case TAG_SVG3_ATT_stroke_linecap: return SVG_StrokeLineCap_datatype;
		case TAG_SVG3_ATT_stroke_linejoin: return SVG_StrokeLineJoin_datatype;
		case TAG_SVG3_ATT_stroke_miterlimit: return SVG_Number_datatype;
		case TAG_SVG3_ATT_stroke_width: return SVG_Length_datatype;
		case TAG_SVG3_ATT_color: return SVG_Paint_datatype;
		case TAG_SVG3_ATT_color_rendering: return SVG_RenderingHint_datatype;
		case TAG_SVG3_ATT_vector_effect: return SVG_VectorEffect_datatype;
		case TAG_SVG3_ATT_solid_color: return SVG_SVGColor_datatype;
		case TAG_SVG3_ATT_solid_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_display_align: return SVG_DisplayAlign_datatype;
		case TAG_SVG3_ATT_line_increment: return SVG_Number_datatype;
		case TAG_SVG3_ATT_stop_color: return SVG_SVGColor_datatype;
		case TAG_SVG3_ATT_stop_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_font_family: return SVG_FontFamily_datatype;
		case TAG_SVG3_ATT_font_size: return SVG_FontSize_datatype;
		case TAG_SVG3_ATT_font_style: return SVG_FontStyle_datatype;
		case TAG_SVG3_ATT_font_variant: return SVG_FontVariant_datatype;
		case TAG_SVG3_ATT_font_weight: return SVG_FontWeight_datatype;
		case TAG_SVG3_ATT_text_anchor: return SVG_TextAnchor_datatype;
		case TAG_SVG3_ATT_text_align: return SVG_TextAlign_datatype;
		case TAG_SVG3_ATT_focusHighlight: return SVG_FocusHighlight_datatype;
		case TAG_SVG3_ATT_externalResourcesRequired: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_focusable: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_nav_next: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_prev: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_up: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_up_right: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_right: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_down_right: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_down: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_down_left: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_left: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_nav_up_left: return SVG_Focus_datatype;
		case TAG_SVG3_ATT_transform: return SVG_Transform_datatype;
		case TAG_SVG3_ATT_xlink_type: return SVG_String_datatype;
		case TAG_SVG3_ATT_xlink_role: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_xlink_arcrole: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_xlink_title: return SVG_String_datatype;
		case TAG_SVG3_ATT_xlink_href: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_xlink_show: return SVG_String_datatype;
		case TAG_SVG3_ATT_xlink_actuate: return SVG_String_datatype;
		case TAG_SVG3_ATT_target: return SVG_ID_datatype;
		case TAG_SVG3_ATT_attributeName: return SMIL_AttributeName_datatype;
		case TAG_SVG3_ATT_attributeType: return SMIL_AttributeType_datatype;
		case TAG_SVG3_ATT_begin: return SMIL_Times_datatype;
		case TAG_SVG3_ATT_lsr_enabled: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_dur: return SMIL_Duration_datatype;
		case TAG_SVG3_ATT_end: return SMIL_Times_datatype;
		case TAG_SVG3_ATT_repeatCount: return SMIL_RepeatCount_datatype;
		case TAG_SVG3_ATT_repeatDur: return SMIL_Duration_datatype;
		case TAG_SVG3_ATT_restart: return SMIL_Restart_datatype;
		case TAG_SVG3_ATT_smil_fill: return SMIL_Fill_datatype;
		case TAG_SVG3_ATT_min: return SMIL_Duration_datatype;
		case TAG_SVG3_ATT_max: return SMIL_Duration_datatype;
		case TAG_SVG3_ATT_to: return SMIL_AnimateValue_datatype;
		case TAG_SVG3_ATT_calcMode: return SMIL_CalcMode_datatype;
		case TAG_SVG3_ATT_values: return SMIL_AnimateValues_datatype;
		case TAG_SVG3_ATT_keyTimes: return SMIL_KeyTimes_datatype;
		case TAG_SVG3_ATT_keySplines: return SMIL_KeySplines_datatype;
		case TAG_SVG3_ATT_from: return SMIL_AnimateValue_datatype;
		case TAG_SVG3_ATT_by: return SMIL_AnimateValue_datatype;
		case TAG_SVG3_ATT_additive: return SMIL_Additive_datatype;
		case TAG_SVG3_ATT_accumulate: return SMIL_Accumulate_datatype;
		case TAG_SVG3_ATT_path: return SVG_PathData_datatype;
		case TAG_SVG3_ATT_keyPoints: return SMIL_KeyPoints_datatype;
		case TAG_SVG3_ATT_rotate: return SVG_Rotate_datatype;
		case TAG_SVG3_ATT_origin: return SVG_String_datatype;
		case TAG_SVG3_ATT_type: return SVG_TransformType_datatype;
		case TAG_SVG3_ATT_clipBegin: return SVG_Clock_datatype;
		case TAG_SVG3_ATT_clipEnd: return SVG_Clock_datatype;
		case TAG_SVG3_ATT_syncBehavior: return SMIL_SyncBehavior_datatype;
		case TAG_SVG3_ATT_syncTolerance: return SMIL_SyncTolerance_datatype;
		case TAG_SVG3_ATT_syncMaster: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_syncReference: return SVG_String_datatype;
		case TAG_SVG3_ATT_x: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_y: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_width: return SVG_Length_datatype;
		case TAG_SVG3_ATT_height: return SVG_Length_datatype;
		case TAG_SVG3_ATT_preserveAspectRatio: return SVG_PreserveAspectRatio_datatype;
		case TAG_SVG3_ATT_initialVisibility: return SVG_InitialVisibility_datatype;
		case TAG_SVG3_ATT_content_type: return SVG_ContentType_datatype;
		case TAG_SVG3_ATT_cx: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_cy: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_r: return SVG_Length_datatype;
		case TAG_SVG3_ATT_enabled: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_cursorManager_x: return SVG_Length_datatype;
		case TAG_SVG3_ATT_cursorManager_y: return SVG_Length_datatype;
		case TAG_SVG3_ATT_rx: return SVG_Length_datatype;
		case TAG_SVG3_ATT_ry: return SVG_Length_datatype;
		case TAG_SVG3_ATT_horiz_adv_x: return SVG_Number_datatype;
		case TAG_SVG3_ATT_horiz_origin_x: return SVG_Number_datatype;
		case TAG_SVG3_ATT_font_stretch: return SVG_String_datatype;
		case TAG_SVG3_ATT_unicode_range: return SVG_String_datatype;
		case TAG_SVG3_ATT_panose_1: return SVG_String_datatype;
		case TAG_SVG3_ATT_widths: return SVG_String_datatype;
		case TAG_SVG3_ATT_bbox: return SVG_String_datatype;
		case TAG_SVG3_ATT_units_per_em: return SVG_Number_datatype;
		case TAG_SVG3_ATT_stemv: return SVG_Number_datatype;
		case TAG_SVG3_ATT_stemh: return SVG_Number_datatype;
		case TAG_SVG3_ATT_slope: return SVG_Number_datatype;
		case TAG_SVG3_ATT_cap_height: return SVG_Number_datatype;
		case TAG_SVG3_ATT_x_height: return SVG_Number_datatype;
		case TAG_SVG3_ATT_accent_height: return SVG_Number_datatype;
		case TAG_SVG3_ATT_ascent: return SVG_Number_datatype;
		case TAG_SVG3_ATT_descent: return SVG_Number_datatype;
		case TAG_SVG3_ATT_ideographic: return SVG_Number_datatype;
		case TAG_SVG3_ATT_alphabetic: return SVG_Number_datatype;
		case TAG_SVG3_ATT_mathematical: return SVG_Number_datatype;
		case TAG_SVG3_ATT_hanging: return SVG_Number_datatype;
		case TAG_SVG3_ATT_underline_position: return SVG_Number_datatype;
		case TAG_SVG3_ATT_underline_thickness: return SVG_Number_datatype;
		case TAG_SVG3_ATT_strikethrough_position: return SVG_Number_datatype;
		case TAG_SVG3_ATT_strikethrough_thickness: return SVG_Number_datatype;
		case TAG_SVG3_ATT_overline_position: return SVG_Number_datatype;
		case TAG_SVG3_ATT_overline_thickness: return SVG_Number_datatype;
		case TAG_SVG3_ATT_d: return SVG_PathData_datatype;
		case TAG_SVG3_ATT_unicode: return SVG_String_datatype;
		case TAG_SVG3_ATT_glyph_name: return SVG_String_datatype;
		case TAG_SVG3_ATT_arabic_form: return SVG_String_datatype;
		case TAG_SVG3_ATT_lang: return SVG_LanguageIDs_datatype;
		case TAG_SVG3_ATT_ev_event: return XMLEV_Event_datatype;
		case TAG_SVG3_ATT_u1: return SVG_String_datatype;
		case TAG_SVG3_ATT_g1: return SVG_String_datatype;
		case TAG_SVG3_ATT_u2: return SVG_String_datatype;
		case TAG_SVG3_ATT_g2: return SVG_String_datatype;
		case TAG_SVG3_ATT_k: return SVG_Number_datatype;
		case TAG_SVG3_ATT_opacity: return SVG_Number_datatype;
		case TAG_SVG3_ATT_x1: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_y1: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_x2: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_y2: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_gradientUnits: return SVG_GradientUnit_datatype;
		case TAG_SVG3_ATT_spreadMethod: return SVG_SpreadMethod_datatype;
		case TAG_SVG3_ATT_gradientTransform: return SVG_Transform_datatype;
		case TAG_SVG3_ATT_event: return XMLEV_Event_datatype;
		case TAG_SVG3_ATT_phase: return XMLEV_Phase_datatype;
		case TAG_SVG3_ATT_propagate: return XMLEV_Propagate_datatype;
		case TAG_SVG3_ATT_defaultAction: return XMLEV_DefaultAction_datatype;
		case TAG_SVG3_ATT_observer: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_listener_target: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_handler: return SVG_IRI_datatype;
		case TAG_SVG3_ATT_pathLength: return SVG_Number_datatype;
		case TAG_SVG3_ATT_points: return SVG_Points_datatype;
		case TAG_SVG3_ATT_mediaSize: return SVG_Number_datatype;
		case TAG_SVG3_ATT_mediaTime: return SVG_String_datatype;
		case TAG_SVG3_ATT_mediaCharacterEncoding: return SVG_String_datatype;
		case TAG_SVG3_ATT_mediaContentEncodings: return SVG_String_datatype;
		case TAG_SVG3_ATT_bandwidth: return SVG_Number_datatype;
		case TAG_SVG3_ATT_fx: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_fy: return SVG_Coordinate_datatype;
		case TAG_SVG3_ATT_size: return LASeR_Size_datatype;
		case TAG_SVG3_ATT_choice: return LASeR_Choice_datatype;
		case TAG_SVG3_ATT_delta: return LASeR_Size_datatype;
		case TAG_SVG3_ATT_offset: return SVG_Number_datatype;
		case TAG_SVG3_ATT_syncBehaviorDefault: return SMIL_SyncBehavior_datatype;
		case TAG_SVG3_ATT_syncToleranceDefault: return SMIL_SyncTolerance_datatype;
		case TAG_SVG3_ATT_viewBox: return SVG_ViewBox_datatype;
		case TAG_SVG3_ATT_zoomAndPan: return SVG_ZoomAndPan_datatype;
		case TAG_SVG3_ATT_version: return SVG_String_datatype;
		case TAG_SVG3_ATT_baseProfile: return SVG_String_datatype;
		case TAG_SVG3_ATT_snapshotTime: return SVG_Clock_datatype;
		case TAG_SVG3_ATT_timelineBegin: return SVG_TimelineBegin_datatype;
		case TAG_SVG3_ATT_playbackOrder: return SVG_PlaybackOrder_datatype;
		case TAG_SVG3_ATT_editable: return SVG_Boolean_datatype;
		case TAG_SVG3_ATT_text_x: return SVG_Coordinates_datatype;
		case TAG_SVG3_ATT_text_y: return SVG_Coordinates_datatype;
		case TAG_SVG3_ATT_text_rotate: return SVG_Numbers_datatype;
		case TAG_SVG3_ATT_transformBehavior: return SVG_TransformBehavior_datatype;
		case TAG_SVG3_ATT_overlay: return SVG_Overlay_datatype;
		case TAG_SVG3_ATT_motionTransform: return SVG_Motion_datatype;
		default: return SVG_Unknown_datatype;
	}
	return TAG_SVG3_ATT_Unknown;
}

void gf_svg3_fill_all_attributes(SVG3AllAttributes *all_atts, SVG3Element *e)
{
	SVG3Attribute *att = e->attributes;
	memset(all_atts, 0, sizeof(SVG3AllAttributes));
	while (att) {
		switch(att->tag) {
			case TAG_SVG3_ATT_id: all_atts->id = (SVG_ID *)att->data; break;
			case TAG_SVG3_ATT__class: all_atts->_class = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_xml_id: all_atts->xml_id = (SVG_ID *)att->data; break;
			case TAG_SVG3_ATT_xml_base: all_atts->xml_base = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_xml_lang: all_atts->xml_lang = (SVG_LanguageID *)att->data; break;
			case TAG_SVG3_ATT_xml_space: all_atts->xml_space = (XML_Space *)att->data; break;
			case TAG_SVG3_ATT_requiredFeatures: all_atts->requiredFeatures = (SVG_ListOfIRI *)att->data; break;
			case TAG_SVG3_ATT_requiredExtensions: all_atts->requiredExtensions = (SVG_ListOfIRI *)att->data; break;
			case TAG_SVG3_ATT_requiredFormats: all_atts->requiredFormats = (SVG_FormatList *)att->data; break;
			case TAG_SVG3_ATT_requiredFonts: all_atts->requiredFonts = (SVG_FontList *)att->data; break;
			case TAG_SVG3_ATT_systemLanguage: all_atts->systemLanguage = (SVG_LanguageIDs *)att->data; break;
			case TAG_SVG3_ATT_display: all_atts->display = (SVG_Display *)att->data; break;
			case TAG_SVG3_ATT_visibility: all_atts->visibility = (SVG_Visibility *)att->data; break;
			case TAG_SVG3_ATT_image_rendering: all_atts->image_rendering = (SVG_RenderingHint *)att->data; break;
			case TAG_SVG3_ATT_pointer_events: all_atts->pointer_events = (SVG_PointerEvents *)att->data; break;
			case TAG_SVG3_ATT_shape_rendering: all_atts->shape_rendering = (SVG_RenderingHint *)att->data; break;
			case TAG_SVG3_ATT_text_rendering: all_atts->text_rendering = (SVG_RenderingHint *)att->data; break;
			case TAG_SVG3_ATT_audio_level: all_atts->audio_level = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_viewport_fill: all_atts->viewport_fill = (SVG_Paint *)att->data; break;
			case TAG_SVG3_ATT_viewport_fill_opacity: all_atts->viewport_fill_opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_fill_opacity: all_atts->fill_opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_stroke_opacity: all_atts->stroke_opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_fill: all_atts->fill = (SVG_Paint *)att->data; break;
			case TAG_SVG3_ATT_fill_rule: all_atts->fill_rule = (SVG_FillRule *)att->data; break;
			case TAG_SVG3_ATT_stroke: all_atts->stroke = (SVG_Paint *)att->data; break;
			case TAG_SVG3_ATT_stroke_dasharray: all_atts->stroke_dasharray = (SVG_StrokeDashArray *)att->data; break;
			case TAG_SVG3_ATT_stroke_dashoffset: all_atts->stroke_dashoffset = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_stroke_linecap: all_atts->stroke_linecap = (SVG_StrokeLineCap *)att->data; break;
			case TAG_SVG3_ATT_stroke_linejoin: all_atts->stroke_linejoin = (SVG_StrokeLineJoin *)att->data; break;
			case TAG_SVG3_ATT_stroke_miterlimit: all_atts->stroke_miterlimit = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_stroke_width: all_atts->stroke_width = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_color: all_atts->color = (SVG_Paint *)att->data; break;
			case TAG_SVG3_ATT_color_rendering: all_atts->color_rendering = (SVG_RenderingHint *)att->data; break;
			case TAG_SVG3_ATT_vector_effect: all_atts->vector_effect = (SVG_VectorEffect *)att->data; break;
			case TAG_SVG3_ATT_solid_color: all_atts->solid_color = (SVG_SVGColor *)att->data; break;
			case TAG_SVG3_ATT_solid_opacity: all_atts->solid_opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_display_align: all_atts->display_align = (SVG_DisplayAlign *)att->data; break;
			case TAG_SVG3_ATT_line_increment: all_atts->line_increment = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_stop_color: all_atts->stop_color = (SVG_SVGColor *)att->data; break;
			case TAG_SVG3_ATT_stop_opacity: all_atts->stop_opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_font_family: all_atts->font_family = (SVG_FontFamily *)att->data; break;
			case TAG_SVG3_ATT_font_size: all_atts->font_size = (SVG_FontSize *)att->data; break;
			case TAG_SVG3_ATT_font_style: all_atts->font_style = (SVG_FontStyle *)att->data; break;
			case TAG_SVG3_ATT_font_variant: all_atts->font_variant = (SVG_FontVariant *)att->data; break;
			case TAG_SVG3_ATT_font_weight: all_atts->font_weight = (SVG_FontWeight *)att->data; break;
			case TAG_SVG3_ATT_text_anchor: all_atts->text_anchor = (SVG_TextAnchor *)att->data; break;
			case TAG_SVG3_ATT_text_align: all_atts->text_align = (SVG_TextAlign *)att->data; break;
			case TAG_SVG3_ATT_focusHighlight: all_atts->focusHighlight = (SVG_FocusHighlight *)att->data; break;
			case TAG_SVG3_ATT_externalResourcesRequired: all_atts->externalResourcesRequired = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_focusable: all_atts->focusable = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_nav_next: all_atts->nav_next = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_prev: all_atts->nav_prev = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_up: all_atts->nav_up = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_up_right: all_atts->nav_up_right = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_right: all_atts->nav_right = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_down_right: all_atts->nav_down_right = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_down: all_atts->nav_down = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_down_left: all_atts->nav_down_left = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_left: all_atts->nav_left = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_nav_up_left: all_atts->nav_up_left = (SVG_Focus *)att->data; break;
			case TAG_SVG3_ATT_transform: all_atts->transform = (SVG_Transform *)att->data; break;
			case TAG_SVG3_ATT_xlink_type: all_atts->xlink_type = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_xlink_role: all_atts->xlink_role = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_xlink_arcrole: all_atts->xlink_arcrole = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_xlink_title: all_atts->xlink_title = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_xlink_href: all_atts->xlink_href = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_xlink_show: all_atts->xlink_show = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_xlink_actuate: all_atts->xlink_actuate = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_target: all_atts->target = (SVG_ID *)att->data; break;
			case TAG_SVG3_ATT_attributeName: all_atts->attributeName = (SMIL_AttributeName *)att->data; break;
			case TAG_SVG3_ATT_attributeType: all_atts->attributeType = (SMIL_AttributeType *)att->data; break;
			case TAG_SVG3_ATT_begin: all_atts->begin = (SMIL_Times *)att->data; break;
			case TAG_SVG3_ATT_lsr_enabled: all_atts->lsr_enabled = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_dur: all_atts->dur = (SMIL_Duration *)att->data; break;
			case TAG_SVG3_ATT_end: all_atts->end = (SMIL_Times *)att->data; break;
			case TAG_SVG3_ATT_repeatCount: all_atts->repeatCount = (SMIL_RepeatCount *)att->data; break;
			case TAG_SVG3_ATT_repeatDur: all_atts->repeatDur = (SMIL_Duration *)att->data; break;
			case TAG_SVG3_ATT_restart: all_atts->restart = (SMIL_Restart *)att->data; break;
			case TAG_SVG3_ATT_smil_fill: all_atts->smil_fill = (SMIL_Fill *)att->data; break;
			case TAG_SVG3_ATT_min: all_atts->min = (SMIL_Duration *)att->data; break;
			case TAG_SVG3_ATT_max: all_atts->max = (SMIL_Duration *)att->data; break;
			case TAG_SVG3_ATT_to: all_atts->to = (SMIL_AnimateValue *)att->data; break;
			case TAG_SVG3_ATT_calcMode: all_atts->calcMode = (SMIL_CalcMode *)att->data; break;
			case TAG_SVG3_ATT_values: all_atts->values = (SMIL_AnimateValues *)att->data; break;
			case TAG_SVG3_ATT_keyTimes: all_atts->keyTimes = (SMIL_KeyTimes *)att->data; break;
			case TAG_SVG3_ATT_keySplines: all_atts->keySplines = (SMIL_KeySplines *)att->data; break;
			case TAG_SVG3_ATT_from: all_atts->from = (SMIL_AnimateValue *)att->data; break;
			case TAG_SVG3_ATT_by: all_atts->by = (SMIL_AnimateValue *)att->data; break;
			case TAG_SVG3_ATT_additive: all_atts->additive = (SMIL_Additive *)att->data; break;
			case TAG_SVG3_ATT_accumulate: all_atts->accumulate = (SMIL_Accumulate *)att->data; break;
			case TAG_SVG3_ATT_path: all_atts->path = (SVG_PathData *)att->data; break;
			case TAG_SVG3_ATT_keyPoints: all_atts->keyPoints = (SMIL_KeyPoints *)att->data; break;
			case TAG_SVG3_ATT_rotate: all_atts->rotate = (SVG_Rotate *)att->data; break;
			case TAG_SVG3_ATT_origin: all_atts->origin = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_type: all_atts->type = (SVG_TransformType *)att->data; break;
			case TAG_SVG3_ATT_clipBegin: all_atts->clipBegin = (SVG_Clock *)att->data; break;
			case TAG_SVG3_ATT_clipEnd: all_atts->clipEnd = (SVG_Clock *)att->data; break;
			case TAG_SVG3_ATT_syncBehavior: all_atts->syncBehavior = (SMIL_SyncBehavior *)att->data; break;
			case TAG_SVG3_ATT_syncTolerance: all_atts->syncTolerance = (SMIL_SyncTolerance *)att->data; break;
			case TAG_SVG3_ATT_syncMaster: all_atts->syncMaster = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_syncReference: all_atts->syncReference = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_x: all_atts->x = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_y: all_atts->y = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_width: all_atts->width = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_height: all_atts->height = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_preserveAspectRatio: all_atts->preserveAspectRatio = (SVG_PreserveAspectRatio *)att->data; break;
			case TAG_SVG3_ATT_initialVisibility: all_atts->initialVisibility = (SVG_InitialVisibility *)att->data; break;
			case TAG_SVG3_ATT_content_type: all_atts->content_type = (SVG_ContentType *)att->data; break;
			case TAG_SVG3_ATT_cx: all_atts->cx = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_cy: all_atts->cy = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_r: all_atts->r = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_enabled: all_atts->enabled = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_cursorManager_x: all_atts->cursorManager_x = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_cursorManager_y: all_atts->cursorManager_y = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_rx: all_atts->rx = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_ry: all_atts->ry = (SVG_Length *)att->data; break;
			case TAG_SVG3_ATT_horiz_adv_x: all_atts->horiz_adv_x = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_horiz_origin_x: all_atts->horiz_origin_x = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_font_stretch: all_atts->font_stretch = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_unicode_range: all_atts->unicode_range = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_panose_1: all_atts->panose_1 = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_widths: all_atts->widths = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_bbox: all_atts->bbox = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_units_per_em: all_atts->units_per_em = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_stemv: all_atts->stemv = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_stemh: all_atts->stemh = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_slope: all_atts->slope = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_cap_height: all_atts->cap_height = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_x_height: all_atts->x_height = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_accent_height: all_atts->accent_height = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_ascent: all_atts->ascent = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_descent: all_atts->descent = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_ideographic: all_atts->ideographic = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_alphabetic: all_atts->alphabetic = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_mathematical: all_atts->mathematical = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_hanging: all_atts->hanging = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_underline_position: all_atts->underline_position = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_underline_thickness: all_atts->underline_thickness = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_strikethrough_position: all_atts->strikethrough_position = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_strikethrough_thickness: all_atts->strikethrough_thickness = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_overline_position: all_atts->overline_position = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_overline_thickness: all_atts->overline_thickness = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_d: all_atts->d = (SVG_PathData *)att->data; break;
			case TAG_SVG3_ATT_unicode: all_atts->unicode = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_glyph_name: all_atts->glyph_name = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_arabic_form: all_atts->arabic_form = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_lang: all_atts->lang = (SVG_LanguageIDs *)att->data; break;
			case TAG_SVG3_ATT_ev_event: all_atts->ev_event = (XMLEV_Event *)att->data; break;
			case TAG_SVG3_ATT_u1: all_atts->u1 = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_g1: all_atts->g1 = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_u2: all_atts->u2 = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_g2: all_atts->g2 = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_k: all_atts->k = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_opacity: all_atts->opacity = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_x1: all_atts->x1 = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_y1: all_atts->y1 = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_x2: all_atts->x2 = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_y2: all_atts->y2 = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_gradientUnits: all_atts->gradientUnits = (SVG_GradientUnit *)att->data; break;
			case TAG_SVG3_ATT_spreadMethod: all_atts->spreadMethod = (SVG_SpreadMethod *)att->data; break;
			case TAG_SVG3_ATT_gradientTransform: all_atts->gradientTransform = (SVG_Transform *)att->data; break;
			case TAG_SVG3_ATT_event: all_atts->event = (XMLEV_Event *)att->data; break;
			case TAG_SVG3_ATT_phase: all_atts->phase = (XMLEV_Phase *)att->data; break;
			case TAG_SVG3_ATT_propagate: all_atts->propagate = (XMLEV_Propagate *)att->data; break;
			case TAG_SVG3_ATT_defaultAction: all_atts->defaultAction = (XMLEV_DefaultAction *)att->data; break;
			case TAG_SVG3_ATT_observer: all_atts->observer = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_listener_target: all_atts->listener_target = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_handler: all_atts->handler = (SVG_IRI *)att->data; break;
			case TAG_SVG3_ATT_pathLength: all_atts->pathLength = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_points: all_atts->points = (SVG_Points *)att->data; break;
			case TAG_SVG3_ATT_mediaSize: all_atts->mediaSize = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_mediaTime: all_atts->mediaTime = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_mediaCharacterEncoding: all_atts->mediaCharacterEncoding = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_mediaContentEncodings: all_atts->mediaContentEncodings = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_bandwidth: all_atts->bandwidth = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_fx: all_atts->fx = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_fy: all_atts->fy = (SVG_Coordinate *)att->data; break;
			case TAG_SVG3_ATT_size: all_atts->size = (LASeR_Size *)att->data; break;
			case TAG_SVG3_ATT_choice: all_atts->choice = (LASeR_Choice *)att->data; break;
			case TAG_SVG3_ATT_delta: all_atts->delta = (LASeR_Size *)att->data; break;
			case TAG_SVG3_ATT_offset: all_atts->offset = (SVG_Number *)att->data; break;
			case TAG_SVG3_ATT_syncBehaviorDefault: all_atts->syncBehaviorDefault = (SMIL_SyncBehavior *)att->data; break;
			case TAG_SVG3_ATT_syncToleranceDefault: all_atts->syncToleranceDefault = (SMIL_SyncTolerance *)att->data; break;
			case TAG_SVG3_ATT_viewBox: all_atts->viewBox = (SVG_ViewBox *)att->data; break;
			case TAG_SVG3_ATT_zoomAndPan: all_atts->zoomAndPan = (SVG_ZoomAndPan *)att->data; break;
			case TAG_SVG3_ATT_version: all_atts->version = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_baseProfile: all_atts->baseProfile = (SVG_String *)att->data; break;
			case TAG_SVG3_ATT_snapshotTime: all_atts->snapshotTime = (SVG_Clock *)att->data; break;
			case TAG_SVG3_ATT_timelineBegin: all_atts->timelineBegin = (SVG_TimelineBegin *)att->data; break;
			case TAG_SVG3_ATT_playbackOrder: all_atts->playbackOrder = (SVG_PlaybackOrder *)att->data; break;
			case TAG_SVG3_ATT_editable: all_atts->editable = (SVG_Boolean *)att->data; break;
			case TAG_SVG3_ATT_text_x: all_atts->text_x = (SVG_Coordinates *)att->data; break;
			case TAG_SVG3_ATT_text_y: all_atts->text_y = (SVG_Coordinates *)att->data; break;
			case TAG_SVG3_ATT_text_rotate: all_atts->text_rotate = (SVG_Numbers *)att->data; break;
			case TAG_SVG3_ATT_transformBehavior: all_atts->transformBehavior = (SVG_TransformBehavior *)att->data; break;
			case TAG_SVG3_ATT_overlay: all_atts->overlay = (SVG_Overlay *)att->data; break;
		case TAG_SVG3_ATT_motionTransform: all_atts->motionTransform = (GF_Matrix2D *)att->data; break;
		}
		att = att->next;
	}
}

SVG3Attribute *gf_svg3_a_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_target: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_animate_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_attributeName: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeName_datatype, tag);
		case TAG_SVG3_ATT_attributeType: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeType_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_to: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_calcMode: return gf_svg3_create_attribute_from_datatype(SMIL_CalcMode_datatype, tag);
		case TAG_SVG3_ATT_values: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValues_datatype, tag);
		case TAG_SVG3_ATT_keyTimes: return gf_svg3_create_attribute_from_datatype(SMIL_KeyTimes_datatype, tag);
		case TAG_SVG3_ATT_keySplines: return gf_svg3_create_attribute_from_datatype(SMIL_KeySplines_datatype, tag);
		case TAG_SVG3_ATT_from: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_by: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_additive: return gf_svg3_create_attribute_from_datatype(SMIL_Additive_datatype, tag);
		case TAG_SVG3_ATT_accumulate: return gf_svg3_create_attribute_from_datatype(SMIL_Accumulate_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_animateColor_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_attributeName: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeName_datatype, tag);
		case TAG_SVG3_ATT_attributeType: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeType_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_to: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_calcMode: return gf_svg3_create_attribute_from_datatype(SMIL_CalcMode_datatype, tag);
		case TAG_SVG3_ATT_values: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValues_datatype, tag);
		case TAG_SVG3_ATT_keyTimes: return gf_svg3_create_attribute_from_datatype(SMIL_KeyTimes_datatype, tag);
		case TAG_SVG3_ATT_keySplines: return gf_svg3_create_attribute_from_datatype(SMIL_KeySplines_datatype, tag);
		case TAG_SVG3_ATT_from: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_by: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_additive: return gf_svg3_create_attribute_from_datatype(SMIL_Additive_datatype, tag);
		case TAG_SVG3_ATT_accumulate: return gf_svg3_create_attribute_from_datatype(SMIL_Accumulate_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_animateMotion_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_additive: return gf_svg3_create_attribute_from_datatype(SMIL_Additive_datatype, tag);
		case TAG_SVG3_ATT_accumulate: return gf_svg3_create_attribute_from_datatype(SMIL_Accumulate_datatype, tag);
		case TAG_SVG3_ATT_to: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_calcMode: return gf_svg3_create_attribute_from_datatype(SMIL_CalcMode_datatype, tag);
		case TAG_SVG3_ATT_values: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValues_datatype, tag);
		case TAG_SVG3_ATT_keyTimes: return gf_svg3_create_attribute_from_datatype(SMIL_KeyTimes_datatype, tag);
		case TAG_SVG3_ATT_keySplines: return gf_svg3_create_attribute_from_datatype(SMIL_KeySplines_datatype, tag);
		case TAG_SVG3_ATT_from: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_by: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_path: return gf_svg3_create_attribute_from_datatype(SVG_PathData_datatype, tag);
		case TAG_SVG3_ATT_keyPoints: return gf_svg3_create_attribute_from_datatype(SMIL_KeyPoints_datatype, tag);
		case TAG_SVG3_ATT_rotate: return gf_svg3_create_attribute_from_datatype(SVG_Rotate_datatype, tag);
		case TAG_SVG3_ATT_origin: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_animateTransform_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_attributeName: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeName_datatype, tag);
		case TAG_SVG3_ATT_attributeType: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeType_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_to: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_calcMode: return gf_svg3_create_attribute_from_datatype(SMIL_CalcMode_datatype, tag);
		case TAG_SVG3_ATT_values: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValues_datatype, tag);
		case TAG_SVG3_ATT_keyTimes: return gf_svg3_create_attribute_from_datatype(SMIL_KeyTimes_datatype, tag);
		case TAG_SVG3_ATT_keySplines: return gf_svg3_create_attribute_from_datatype(SMIL_KeySplines_datatype, tag);
		case TAG_SVG3_ATT_from: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_by: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		case TAG_SVG3_ATT_additive: return gf_svg3_create_attribute_from_datatype(SMIL_Additive_datatype, tag);
		case TAG_SVG3_ATT_accumulate: return gf_svg3_create_attribute_from_datatype(SMIL_Accumulate_datatype, tag);
		case TAG_SVG3_ATT_type: return gf_svg3_create_attribute_from_datatype(SVG_TransformType_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_animation_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_clipBegin: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_clipEnd: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_syncBehavior: return gf_svg3_create_attribute_from_datatype(SMIL_SyncBehavior_datatype, tag);
		case TAG_SVG3_ATT_syncTolerance: return gf_svg3_create_attribute_from_datatype(SMIL_SyncTolerance_datatype, tag);
		case TAG_SVG3_ATT_syncMaster: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_syncReference: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_preserveAspectRatio: return gf_svg3_create_attribute_from_datatype(SVG_PreserveAspectRatio_datatype, tag);
		case TAG_SVG3_ATT_initialVisibility: return gf_svg3_create_attribute_from_datatype(SVG_InitialVisibility_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_audio_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_syncBehavior: return gf_svg3_create_attribute_from_datatype(SMIL_SyncBehavior_datatype, tag);
		case TAG_SVG3_ATT_syncTolerance: return gf_svg3_create_attribute_from_datatype(SMIL_SyncTolerance_datatype, tag);
		case TAG_SVG3_ATT_syncMaster: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_syncReference: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_clipBegin: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_clipEnd: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_circle_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_cx: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_cy: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_r: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_conditional_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_cursorManager_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_cursorManager_x: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_cursorManager_y: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_defs_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_desc_create_attribute(u32 tag)
{
	switch(tag) {
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_discard_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_ellipse_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_rx: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_ry: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_cx: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_cy: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_font_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_horiz_adv_x: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_horiz_origin_x: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_font_face_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_stretch: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_unicode_range: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_panose_1: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_widths: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_bbox: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_units_per_em: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stemv: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stemh: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_slope: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_cap_height: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_x_height: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_accent_height: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_ascent: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_descent: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_ideographic: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_alphabetic: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_mathematical: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_hanging: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_underline_position: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_underline_thickness: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_strikethrough_position: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_strikethrough_thickness: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_overline_position: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_overline_thickness: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_font_face_src_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_font_face_uri_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_foreignObject_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_g_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_glyph_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_horiz_adv_x: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_d: return gf_svg3_create_attribute_from_datatype(SVG_PathData_datatype, tag);
		case TAG_SVG3_ATT_unicode: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_glyph_name: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_arabic_form: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_handler_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		case TAG_SVG3_ATT_ev_event: return gf_svg3_create_attribute_from_datatype(XMLEV_Event_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_hkern_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_u1: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_g1: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_u2: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_g2: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_k: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_image_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_preserveAspectRatio: return gf_svg3_create_attribute_from_datatype(SVG_PreserveAspectRatio_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_line_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_x1: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y1: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_x2: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y2: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_linearGradient_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_gradientUnits: return gf_svg3_create_attribute_from_datatype(SVG_GradientUnit_datatype, tag);
		case TAG_SVG3_ATT_spreadMethod: return gf_svg3_create_attribute_from_datatype(SVG_SpreadMethod_datatype, tag);
		case TAG_SVG3_ATT_gradientTransform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_x1: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y1: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_x2: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y2: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_listener_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_event: return gf_svg3_create_attribute_from_datatype(XMLEV_Event_datatype, tag);
		case TAG_SVG3_ATT_phase: return gf_svg3_create_attribute_from_datatype(XMLEV_Phase_datatype, tag);
		case TAG_SVG3_ATT_propagate: return gf_svg3_create_attribute_from_datatype(XMLEV_Propagate_datatype, tag);
		case TAG_SVG3_ATT_defaultAction: return gf_svg3_create_attribute_from_datatype(XMLEV_DefaultAction_datatype, tag);
		case TAG_SVG3_ATT_observer: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_listener_target: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_handler: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_metadata_create_attribute(u32 tag)
{
	switch(tag) {
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_missing_glyph_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_horiz_adv_x: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_d: return gf_svg3_create_attribute_from_datatype(SVG_PathData_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_mpath_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_path_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_d: return gf_svg3_create_attribute_from_datatype(SVG_PathData_datatype, tag);
		case TAG_SVG3_ATT_pathLength: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_polygon_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_points: return gf_svg3_create_attribute_from_datatype(SVG_Points_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_polyline_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_points: return gf_svg3_create_attribute_from_datatype(SVG_Points_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_prefetch_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_mediaSize: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_mediaTime: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_mediaCharacterEncoding: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_mediaContentEncodings: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_bandwidth: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_radialGradient_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_gradientUnits: return gf_svg3_create_attribute_from_datatype(SVG_GradientUnit_datatype, tag);
		case TAG_SVG3_ATT_spreadMethod: return gf_svg3_create_attribute_from_datatype(SVG_SpreadMethod_datatype, tag);
		case TAG_SVG3_ATT_gradientTransform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_cx: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_cy: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_r: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_fx: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_fy: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_rect_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_rx: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_ry: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_rectClip_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_size: return gf_svg3_create_attribute_from_datatype(LASeR_Size_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_script_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_selector_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_choice: return gf_svg3_create_attribute_from_datatype(LASeR_Choice_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_set_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_attributeName: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeName_datatype, tag);
		case TAG_SVG3_ATT_attributeType: return gf_svg3_create_attribute_from_datatype(SMIL_AttributeType_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_to: return gf_svg3_create_attribute_from_datatype(SMIL_AnimateValue_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_simpleLayout_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_delta: return gf_svg3_create_attribute_from_datatype(LASeR_Size_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_solidColor_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_stop_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_offset: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_svg_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_syncBehaviorDefault: return gf_svg3_create_attribute_from_datatype(SMIL_SyncBehavior_datatype, tag);
		case TAG_SVG3_ATT_syncToleranceDefault: return gf_svg3_create_attribute_from_datatype(SMIL_SyncTolerance_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_preserveAspectRatio: return gf_svg3_create_attribute_from_datatype(SVG_PreserveAspectRatio_datatype, tag);
		case TAG_SVG3_ATT_viewBox: return gf_svg3_create_attribute_from_datatype(SVG_ViewBox_datatype, tag);
		case TAG_SVG3_ATT_zoomAndPan: return gf_svg3_create_attribute_from_datatype(SVG_ZoomAndPan_datatype, tag);
		case TAG_SVG3_ATT_version: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_baseProfile: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		case TAG_SVG3_ATT_snapshotTime: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_timelineBegin: return gf_svg3_create_attribute_from_datatype(SVG_TimelineBegin_datatype, tag);
		case TAG_SVG3_ATT_playbackOrder: return gf_svg3_create_attribute_from_datatype(SVG_PlaybackOrder_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_switch_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_tbreak_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_text_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_editable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_text_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinates_datatype, tag);
		case TAG_SVG3_ATT_text_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinates_datatype, tag);
		case TAG_SVG3_ATT_text_rotate: return gf_svg3_create_attribute_from_datatype(SVG_Numbers_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_textArea_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_editable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_title_create_attribute(u32 tag)
{
	switch(tag) {
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_tspan_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_use_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_fill_rule: return gf_svg3_create_attribute_from_datatype(SVG_FillRule_datatype, tag);
		case TAG_SVG3_ATT_stroke: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_stroke_dasharray: return gf_svg3_create_attribute_from_datatype(SVG_StrokeDashArray_datatype, tag);
		case TAG_SVG3_ATT_stroke_dashoffset: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_stroke_linecap: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineCap_datatype, tag);
		case TAG_SVG3_ATT_stroke_linejoin: return gf_svg3_create_attribute_from_datatype(SVG_StrokeLineJoin_datatype, tag);
		case TAG_SVG3_ATT_stroke_miterlimit: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stroke_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_color: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_color_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_vector_effect: return gf_svg3_create_attribute_from_datatype(SVG_VectorEffect_datatype, tag);
		case TAG_SVG3_ATT_solid_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_solid_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_display_align: return gf_svg3_create_attribute_from_datatype(SVG_DisplayAlign_datatype, tag);
		case TAG_SVG3_ATT_line_increment: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_stop_color: return gf_svg3_create_attribute_from_datatype(SVG_SVGColor_datatype, tag);
		case TAG_SVG3_ATT_stop_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_font_family: return gf_svg3_create_attribute_from_datatype(SVG_FontFamily_datatype, tag);
		case TAG_SVG3_ATT_font_size: return gf_svg3_create_attribute_from_datatype(SVG_FontSize_datatype, tag);
		case TAG_SVG3_ATT_font_style: return gf_svg3_create_attribute_from_datatype(SVG_FontStyle_datatype, tag);
		case TAG_SVG3_ATT_font_variant: return gf_svg3_create_attribute_from_datatype(SVG_FontVariant_datatype, tag);
		case TAG_SVG3_ATT_font_weight: return gf_svg3_create_attribute_from_datatype(SVG_FontWeight_datatype, tag);
		case TAG_SVG3_ATT_text_anchor: return gf_svg3_create_attribute_from_datatype(SVG_TextAnchor_datatype, tag);
		case TAG_SVG3_ATT_text_align: return gf_svg3_create_attribute_from_datatype(SVG_TextAlign_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

SVG3Attribute *gf_svg3_video_create_attribute(u32 tag)
{
	switch(tag) {
		case TAG_SVG3_ATT_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT__class: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xml_id: return gf_svg3_create_attribute_from_datatype(SVG_ID_datatype, tag);
		case TAG_SVG3_ATT_xml_base: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xml_lang: return gf_svg3_create_attribute_from_datatype(SVG_LanguageID_datatype, tag);
		case TAG_SVG3_ATT_xml_space: return gf_svg3_create_attribute_from_datatype(XML_Space_datatype, tag);
		case TAG_SVG3_ATT_focusHighlight: return gf_svg3_create_attribute_from_datatype(SVG_FocusHighlight_datatype, tag);
		case TAG_SVG3_ATT_display: return gf_svg3_create_attribute_from_datatype(SVG_Display_datatype, tag);
		case TAG_SVG3_ATT_visibility: return gf_svg3_create_attribute_from_datatype(SVG_Visibility_datatype, tag);
		case TAG_SVG3_ATT_image_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_pointer_events: return gf_svg3_create_attribute_from_datatype(SVG_PointerEvents_datatype, tag);
		case TAG_SVG3_ATT_shape_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_text_rendering: return gf_svg3_create_attribute_from_datatype(SVG_RenderingHint_datatype, tag);
		case TAG_SVG3_ATT_audio_level: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill: return gf_svg3_create_attribute_from_datatype(SVG_Paint_datatype, tag);
		case TAG_SVG3_ATT_viewport_fill_opacity: return gf_svg3_create_attribute_from_datatype(SVG_Number_datatype, tag);
		case TAG_SVG3_ATT_clipBegin: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_clipEnd: return gf_svg3_create_attribute_from_datatype(SVG_Clock_datatype, tag);
		case TAG_SVG3_ATT_xlink_actuate: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_type: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_role: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_arcrole: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_title: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_xlink_href: return gf_svg3_create_attribute_from_datatype(SVG_IRI_datatype, tag);
		case TAG_SVG3_ATT_xlink_show: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_requiredFeatures: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredExtensions: return gf_svg3_create_attribute_from_datatype(SVG_ListOfIRI_datatype, tag);
		case TAG_SVG3_ATT_requiredFormats: return gf_svg3_create_attribute_from_datatype(SVG_FormatList_datatype, tag);
		case TAG_SVG3_ATT_requiredFonts: return gf_svg3_create_attribute_from_datatype(SVG_FontList_datatype, tag);
		case TAG_SVG3_ATT_systemLanguage: return gf_svg3_create_attribute_from_datatype(SVG_LanguageIDs_datatype, tag);
		case TAG_SVG3_ATT_externalResourcesRequired: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_begin: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_lsr_enabled: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_dur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_end: return gf_svg3_create_attribute_from_datatype(SMIL_Times_datatype, tag);
		case TAG_SVG3_ATT_repeatCount: return gf_svg3_create_attribute_from_datatype(SMIL_RepeatCount_datatype, tag);
		case TAG_SVG3_ATT_repeatDur: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_restart: return gf_svg3_create_attribute_from_datatype(SMIL_Restart_datatype, tag);
		case TAG_SVG3_ATT_smil_fill: return gf_svg3_create_attribute_from_datatype(SMIL_Fill_datatype, tag);
		case TAG_SVG3_ATT_min: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_max: return gf_svg3_create_attribute_from_datatype(SMIL_Duration_datatype, tag);
		case TAG_SVG3_ATT_syncBehavior: return gf_svg3_create_attribute_from_datatype(SMIL_SyncBehavior_datatype, tag);
		case TAG_SVG3_ATT_syncTolerance: return gf_svg3_create_attribute_from_datatype(SMIL_SyncTolerance_datatype, tag);
		case TAG_SVG3_ATT_syncMaster: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_syncReference: return gf_svg3_create_attribute_from_datatype(SVG_String_datatype, tag);
		case TAG_SVG3_ATT_focusable: return gf_svg3_create_attribute_from_datatype(SVG_Boolean_datatype, tag);
		case TAG_SVG3_ATT_nav_next: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_prev: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_right: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_down_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_nav_up_left: return gf_svg3_create_attribute_from_datatype(SVG_Focus_datatype, tag);
		case TAG_SVG3_ATT_transform: return gf_svg3_create_attribute_from_datatype(SVG_Transform_datatype, tag);
		case TAG_SVG3_ATT_x: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_y: return gf_svg3_create_attribute_from_datatype(SVG_Coordinate_datatype, tag);
		case TAG_SVG3_ATT_width: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_height: return gf_svg3_create_attribute_from_datatype(SVG_Length_datatype, tag);
		case TAG_SVG3_ATT_preserveAspectRatio: return gf_svg3_create_attribute_from_datatype(SVG_PreserveAspectRatio_datatype, tag);
		case TAG_SVG3_ATT_content_type: return gf_svg3_create_attribute_from_datatype(SVG_ContentType_datatype, tag);
		case TAG_SVG3_ATT_initialVisibility: return gf_svg3_create_attribute_from_datatype(SVG_InitialVisibility_datatype, tag);
		case TAG_SVG3_ATT_transformBehavior: return gf_svg3_create_attribute_from_datatype(SVG_TransformBehavior_datatype, tag);
		case TAG_SVG3_ATT_overlay: return gf_svg3_create_attribute_from_datatype(SVG_Overlay_datatype, tag);
		case TAG_SVG3_ATT_motionTransform: return gf_svg3_create_attribute_from_datatype(SVG_Motion_datatype, tag);
		default: return NULL;
	}
}

u32 gf_node_svg3_type_by_class_name(const char *element_name)
{
	if (!element_name) return TAG_UndefinedNode;
	if (!stricmp(element_name, "a")) return TAG_SVG3_a;
	if (!stricmp(element_name, "animate")) return TAG_SVG3_animate;
	if (!stricmp(element_name, "animateColor")) return TAG_SVG3_animateColor;
	if (!stricmp(element_name, "animateMotion")) return TAG_SVG3_animateMotion;
	if (!stricmp(element_name, "animateTransform")) return TAG_SVG3_animateTransform;
	if (!stricmp(element_name, "animation")) return TAG_SVG3_animation;
	if (!stricmp(element_name, "audio")) return TAG_SVG3_audio;
	if (!stricmp(element_name, "circle")) return TAG_SVG3_circle;
	if (!stricmp(element_name, "conditional")) return TAG_SVG3_conditional;
	if (!stricmp(element_name, "cursorManager")) return TAG_SVG3_cursorManager;
	if (!stricmp(element_name, "defs")) return TAG_SVG3_defs;
	if (!stricmp(element_name, "desc")) return TAG_SVG3_desc;
	if (!stricmp(element_name, "discard")) return TAG_SVG3_discard;
	if (!stricmp(element_name, "ellipse")) return TAG_SVG3_ellipse;
	if (!stricmp(element_name, "font")) return TAG_SVG3_font;
	if (!stricmp(element_name, "font-face")) return TAG_SVG3_font_face;
	if (!stricmp(element_name, "font-face-src")) return TAG_SVG3_font_face_src;
	if (!stricmp(element_name, "font-face-uri")) return TAG_SVG3_font_face_uri;
	if (!stricmp(element_name, "foreignObject")) return TAG_SVG3_foreignObject;
	if (!stricmp(element_name, "g")) return TAG_SVG3_g;
	if (!stricmp(element_name, "glyph")) return TAG_SVG3_glyph;
	if (!stricmp(element_name, "handler")) return TAG_SVG3_handler;
	if (!stricmp(element_name, "hkern")) return TAG_SVG3_hkern;
	if (!stricmp(element_name, "image")) return TAG_SVG3_image;
	if (!stricmp(element_name, "line")) return TAG_SVG3_line;
	if (!stricmp(element_name, "linearGradient")) return TAG_SVG3_linearGradient;
	if (!stricmp(element_name, "listener")) return TAG_SVG3_listener;
	if (!stricmp(element_name, "metadata")) return TAG_SVG3_metadata;
	if (!stricmp(element_name, "missing-glyph")) return TAG_SVG3_missing_glyph;
	if (!stricmp(element_name, "mpath")) return TAG_SVG3_mpath;
	if (!stricmp(element_name, "path")) return TAG_SVG3_path;
	if (!stricmp(element_name, "polygon")) return TAG_SVG3_polygon;
	if (!stricmp(element_name, "polyline")) return TAG_SVG3_polyline;
	if (!stricmp(element_name, "prefetch")) return TAG_SVG3_prefetch;
	if (!stricmp(element_name, "radialGradient")) return TAG_SVG3_radialGradient;
	if (!stricmp(element_name, "rect")) return TAG_SVG3_rect;
	if (!stricmp(element_name, "rectClip")) return TAG_SVG3_rectClip;
	if (!stricmp(element_name, "script")) return TAG_SVG3_script;
	if (!stricmp(element_name, "selector")) return TAG_SVG3_selector;
	if (!stricmp(element_name, "set")) return TAG_SVG3_set;
	if (!stricmp(element_name, "simpleLayout")) return TAG_SVG3_simpleLayout;
	if (!stricmp(element_name, "solidColor")) return TAG_SVG3_solidColor;
	if (!stricmp(element_name, "stop")) return TAG_SVG3_stop;
	if (!stricmp(element_name, "svg")) return TAG_SVG3_svg;
	if (!stricmp(element_name, "switch")) return TAG_SVG3_switch;
	if (!stricmp(element_name, "tbreak")) return TAG_SVG3_tbreak;
	if (!stricmp(element_name, "text")) return TAG_SVG3_text;
	if (!stricmp(element_name, "textArea")) return TAG_SVG3_textArea;
	if (!stricmp(element_name, "title")) return TAG_SVG3_title;
	if (!stricmp(element_name, "tspan")) return TAG_SVG3_tspan;
	if (!stricmp(element_name, "use")) return TAG_SVG3_use;
	if (!stricmp(element_name, "video")) return TAG_SVG3_video;
	return TAG_UndefinedNode;
}

const char *gf_svg3_get_element_name(u32 tag)
{
	switch(tag) {
	case TAG_SVG3_a: return "a";
	case TAG_SVG3_animate: return "animate";
	case TAG_SVG3_animateColor: return "animateColor";
	case TAG_SVG3_animateMotion: return "animateMotion";
	case TAG_SVG3_animateTransform: return "animateTransform";
	case TAG_SVG3_animation: return "animation";
	case TAG_SVG3_audio: return "audio";
	case TAG_SVG3_circle: return "circle";
	case TAG_SVG3_conditional: return "conditional";
	case TAG_SVG3_cursorManager: return "cursorManager";
	case TAG_SVG3_defs: return "defs";
	case TAG_SVG3_desc: return "desc";
	case TAG_SVG3_discard: return "discard";
	case TAG_SVG3_ellipse: return "ellipse";
	case TAG_SVG3_font: return "font";
	case TAG_SVG3_font_face: return "font-face";
	case TAG_SVG3_font_face_src: return "font-face-src";
	case TAG_SVG3_font_face_uri: return "font-face-uri";
	case TAG_SVG3_foreignObject: return "foreignObject";
	case TAG_SVG3_g: return "g";
	case TAG_SVG3_glyph: return "glyph";
	case TAG_SVG3_handler: return "handler";
	case TAG_SVG3_hkern: return "hkern";
	case TAG_SVG3_image: return "image";
	case TAG_SVG3_line: return "line";
	case TAG_SVG3_linearGradient: return "linearGradient";
	case TAG_SVG3_listener: return "listener";
	case TAG_SVG3_metadata: return "metadata";
	case TAG_SVG3_missing_glyph: return "missing-glyph";
	case TAG_SVG3_mpath: return "mpath";
	case TAG_SVG3_path: return "path";
	case TAG_SVG3_polygon: return "polygon";
	case TAG_SVG3_polyline: return "polyline";
	case TAG_SVG3_prefetch: return "prefetch";
	case TAG_SVG3_radialGradient: return "radialGradient";
	case TAG_SVG3_rect: return "rect";
	case TAG_SVG3_rectClip: return "rectClip";
	case TAG_SVG3_script: return "script";
	case TAG_SVG3_selector: return "selector";
	case TAG_SVG3_set: return "set";
	case TAG_SVG3_simpleLayout: return "simpleLayout";
	case TAG_SVG3_solidColor: return "solidColor";
	case TAG_SVG3_stop: return "stop";
	case TAG_SVG3_svg: return "svg";
	case TAG_SVG3_switch: return "switch";
	case TAG_SVG3_tbreak: return "tbreak";
	case TAG_SVG3_text: return "text";
	case TAG_SVG3_textArea: return "textArea";
	case TAG_SVG3_title: return "title";
	case TAG_SVG3_tspan: return "tspan";
	case TAG_SVG3_use: return "use";
	case TAG_SVG3_video: return "video";
	default: return "TAG_SVG3_UndefinedNode";
	}
}

SVG3Attribute *gf_svg3_create_attribute(GF_Node *node, u32 tag)
{
	switch(node->sgprivate->tag) {
	case TAG_SVG3_a: return gf_svg3_a_create_attribute(tag);
	case TAG_SVG3_animate: return gf_svg3_animate_create_attribute(tag);
	case TAG_SVG3_animateColor: return gf_svg3_animateColor_create_attribute(tag);
	case TAG_SVG3_animateMotion: return gf_svg3_animateMotion_create_attribute(tag);
	case TAG_SVG3_animateTransform: return gf_svg3_animateTransform_create_attribute(tag);
	case TAG_SVG3_animation: return gf_svg3_animation_create_attribute(tag);
	case TAG_SVG3_audio: return gf_svg3_audio_create_attribute(tag);
	case TAG_SVG3_circle: return gf_svg3_circle_create_attribute(tag);
	case TAG_SVG3_conditional: return gf_svg3_conditional_create_attribute(tag);
	case TAG_SVG3_cursorManager: return gf_svg3_cursorManager_create_attribute(tag);
	case TAG_SVG3_defs: return gf_svg3_defs_create_attribute(tag);
	case TAG_SVG3_desc: return gf_svg3_desc_create_attribute(tag);
	case TAG_SVG3_discard: return gf_svg3_discard_create_attribute(tag);
	case TAG_SVG3_ellipse: return gf_svg3_ellipse_create_attribute(tag);
	case TAG_SVG3_font: return gf_svg3_font_create_attribute(tag);
	case TAG_SVG3_font_face: return gf_svg3_font_face_create_attribute(tag);
	case TAG_SVG3_font_face_src: return gf_svg3_font_face_src_create_attribute(tag);
	case TAG_SVG3_font_face_uri: return gf_svg3_font_face_uri_create_attribute(tag);
	case TAG_SVG3_foreignObject: return gf_svg3_foreignObject_create_attribute(tag);
	case TAG_SVG3_g: return gf_svg3_g_create_attribute(tag);
	case TAG_SVG3_glyph: return gf_svg3_glyph_create_attribute(tag);
	case TAG_SVG3_handler: return gf_svg3_handler_create_attribute(tag);
	case TAG_SVG3_hkern: return gf_svg3_hkern_create_attribute(tag);
	case TAG_SVG3_image: return gf_svg3_image_create_attribute(tag);
	case TAG_SVG3_line: return gf_svg3_line_create_attribute(tag);
	case TAG_SVG3_linearGradient: return gf_svg3_linearGradient_create_attribute(tag);
	case TAG_SVG3_listener: return gf_svg3_listener_create_attribute(tag);
	case TAG_SVG3_metadata: return gf_svg3_metadata_create_attribute(tag);
	case TAG_SVG3_missing_glyph: return gf_svg3_missing_glyph_create_attribute(tag);
	case TAG_SVG3_mpath: return gf_svg3_mpath_create_attribute(tag);
	case TAG_SVG3_path: return gf_svg3_path_create_attribute(tag);
	case TAG_SVG3_polygon: return gf_svg3_polygon_create_attribute(tag);
	case TAG_SVG3_polyline: return gf_svg3_polyline_create_attribute(tag);
	case TAG_SVG3_prefetch: return gf_svg3_prefetch_create_attribute(tag);
	case TAG_SVG3_radialGradient: return gf_svg3_radialGradient_create_attribute(tag);
	case TAG_SVG3_rect: return gf_svg3_rect_create_attribute(tag);
	case TAG_SVG3_rectClip: return gf_svg3_rectClip_create_attribute(tag);
	case TAG_SVG3_script: return gf_svg3_script_create_attribute(tag);
	case TAG_SVG3_selector: return gf_svg3_selector_create_attribute(tag);
	case TAG_SVG3_set: return gf_svg3_set_create_attribute(tag);
	case TAG_SVG3_simpleLayout: return gf_svg3_simpleLayout_create_attribute(tag);
	case TAG_SVG3_solidColor: return gf_svg3_solidColor_create_attribute(tag);
	case TAG_SVG3_stop: return gf_svg3_stop_create_attribute(tag);
	case TAG_SVG3_svg: return gf_svg3_svg_create_attribute(tag);
	case TAG_SVG3_switch: return gf_svg3_switch_create_attribute(tag);
	case TAG_SVG3_tbreak: return gf_svg3_tbreak_create_attribute(tag);
	case TAG_SVG3_text: return gf_svg3_text_create_attribute(tag);
	case TAG_SVG3_textArea: return gf_svg3_textArea_create_attribute(tag);
	case TAG_SVG3_title: return gf_svg3_title_create_attribute(tag);
	case TAG_SVG3_tspan: return gf_svg3_tspan_create_attribute(tag);
	case TAG_SVG3_use: return gf_svg3_use_create_attribute(tag);
	case TAG_SVG3_video: return gf_svg3_video_create_attribute(tag);
	default: return NULL;
	}
}

#endif /*GPAC_DISABLE_SVG*/


