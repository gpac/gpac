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
	DO NOT MOFIFY - File generated on GMT Tue May 15 11:18:46 2007

	BY SVGGen for GPAC Version 0.4.3-DEV
*/


#include <gpac/internal/laser_dev.h>



s32 gf_lsr_anim_type_from_attribute(u32 tag) {
	switch(tag) {
	case TAG_SVG_ATT_target: return 0;
	case TAG_SVG_ATT_accumulate: return 1;
	case TAG_SVG_ATT_additive: return 2;
	case TAG_SVG_ATT_audio_level: return 3;
	case TAG_SVG_ATT_bandwidth: return 4;
	case TAG_SVG_ATT_begin: return 5;
	case TAG_SVG_ATT_calcMode: return 6;
	case TAG_LSR_ATT_children: return 7;
	case TAG_SVG_ATT_choice: return 8;
	case TAG_SVG_ATT_clipBegin: return 9;
	case TAG_SVG_ATT_clipEnd: return 10;
	case TAG_SVG_ATT_color: return 11;
	case TAG_SVG_ATT_color_rendering: return 12;
	case TAG_SVG_ATT_cx: return 13;
	case TAG_SVG_ATT_cy: return 14;
	case TAG_SVG_ATT_d: return 15;
	case TAG_SVG_ATT_delta: return 16;
	case TAG_SVG_ATT_display: return 17;
	case TAG_SVG_ATT_display_align: return 18;
	case TAG_SVG_ATT_dur: return 19;
	case TAG_SVG_ATT_editable: return 20;
	case TAG_LSR_ATT_enabled: return 21;
	case TAG_SVG_ATT_end: return 22;
	case TAG_XMLEV_ATT_event: return 23;
	case TAG_SVG_ATT_externalResourcesRequired: return 24;
	case TAG_SVG_ATT_fill: return 25;
	case TAG_SVG_ATT_fill_opacity: return 26;
	case TAG_SVG_ATT_fill_rule: return 27;
	case TAG_SVG_ATT_focusable: return 28;
	case TAG_SVG_ATT_font_family: return 29;
	case TAG_SVG_ATT_font_size: return 30;
	case TAG_SVG_ATT_font_style: return 31;
	case TAG_SVG_ATT_font_variant: return 32;
	case TAG_SVG_ATT_font_weight: return 33;
	case TAG_SVG_ATT_fullscreen: return 34;
	case TAG_SVG_ATT_gradientUnits: return 35;
	case TAG_XMLEV_ATT_handler: return 36;
	case TAG_SVG_ATT_height: return 37;
	case TAG_SVG_ATT_image_rendering: return 38;
	case TAG_SVG_ATT_keyPoints: return 39;
	case TAG_SVG_ATT_keySplines: return 40;
	case TAG_SVG_ATT_keyTimes: return 41;
	case TAG_SVG_ATT_line_increment: return 42;
	case TAG_XMLEV_ATT_target: return 43;
	case TAG_SVG_ATT_mediaCharacterEncoding: return 44;
	case TAG_SVG_ATT_mediaContentEncodings: return 45;
	case TAG_SVG_ATT_mediaSize: return 46;
	case TAG_SVG_ATT_mediaTime: return 47;
	case TAG_SVG_ATT_nav_down: return 48;
	case TAG_SVG_ATT_nav_down_left: return 49;
	case TAG_SVG_ATT_nav_down_right: return 50;
	case TAG_SVG_ATT_nav_left: return 51;
	case TAG_SVG_ATT_nav_next: return 52;
	case TAG_SVG_ATT_nav_prev: return 53;
	case TAG_SVG_ATT_nav_right: return 54;
	case TAG_SVG_ATT_nav_up: return 55;
	case TAG_SVG_ATT_nav_up_left: return 56;
	case TAG_SVG_ATT_nav_up_right: return 57;
	case TAG_XMLEV_ATT_observer: return 58;
	case TAG_SVG_ATT_offset: return 59;
	case TAG_SVG_ATT_opacity: return 60;
	case TAG_SVG_ATT_overflow: return 61;
	case TAG_SVG_ATT_overlay: return 62;
	case TAG_SVG_ATT_path: return 63;
	case TAG_SVG_ATT_pathLength: return 64;
	case TAG_SVG_ATT_pointer_events: return 65;
	case TAG_SVG_ATT_points: return 66;
	case TAG_SVG_ATT_preserveAspectRatio: return 67;
	case TAG_SVG_ATT_r: return 68;
	case TAG_SVG_ATT_repeatCount: return 69;
	case TAG_SVG_ATT_repeatDur: return 70;
	case TAG_SVG_ATT_requiredExtensions: return 71;
	case TAG_SVG_ATT_requiredFeatures: return 72;
	case TAG_SVG_ATT_requiredFormats: return 73;
	case TAG_SVG_ATT_restart: return 74;
	case TAG_SVG_ATT_rotate: return 75;
	case TAG_LSR_ATT_rotation: return 76;
	case TAG_SVG_ATT_rx: return 77;
	case TAG_SVG_ATT_ry: return 78;
	case TAG_LSR_ATT_scale: return 79;
	case TAG_SVG_ATT_shape_rendering: return 80;
	case TAG_SVG_ATT_size: return 81;
	case TAG_SVG_ATT_solid_color: return 82;
	case TAG_SVG_ATT_solid_opacity: return 83;
	case TAG_SVG_ATT_stop_color: return 84;
	case TAG_SVG_ATT_stop_opacity: return 85;
	case TAG_SVG_ATT_stroke: return 86;
	case TAG_SVG_ATT_stroke_dasharray: return 87;
	case TAG_SVG_ATT_stroke_dashoffset: return 88;
	case TAG_SVG_ATT_stroke_linecap: return 89;
	case TAG_SVG_ATT_stroke_linejoin: return 90;
	case TAG_SVG_ATT_stroke_miterlimit: return 91;
	case TAG_SVG_ATT_stroke_opacity: return 92;
	case TAG_SVG_ATT_stroke_width: return 93;
	case TAG_LSR_ATT_svg_height: return 94;
	case TAG_LSR_ATT_svg_width: return 95;
	case TAG_SVG_ATT_syncBehavior: return 96;
	case TAG_SVG_ATT_syncBehaviorDefault: return 97;
	case TAG_SVG_ATT_syncReference: return 98;
	case TAG_SVG_ATT_syncTolerance: return 99;
	case TAG_SVG_ATT_syncToleranceDefault: return 100;
	case TAG_SVG_ATT_systemLanguage: return 101;
	case TAG_SVG_ATT_text_align: return 102;
	case TAG_SVG_ATT_text_anchor: return 103;
	case TAG_SVG_ATT_text_decoration: return 104;
	case TAG_LSR_ATT_text_display: return 105;
	case TAG_SVG_ATT_text_rendering: return 106;
	case TAG_LSR_ATT_textContent: return 107;
	case TAG_SVG_ATT_transform: return 108;
	case TAG_SVG_ATT_transformBehavior: return 109;
	case TAG_LSR_ATT_translation: return 110;
	case TAG_SVG_ATT_vector_effect: return 111;
	case TAG_SVG_ATT_viewBox: return 112;
	case TAG_SVG_ATT_viewport_fill: return 113;
	case TAG_SVG_ATT_viewport_fill_opacity: return 114;
	case TAG_SVG_ATT_visibility: return 115;
	case TAG_SVG_ATT_width: return 116;
	case TAG_SVG_ATT_x: return 117;
	case TAG_SVG_ATT_x1: return 118;
	case TAG_SVG_ATT_x2: return 119;
	case TAG_XLINK_ATT_actuate: return 120;
	case TAG_XLINK_ATT_arcrole: return 121;
	case TAG_XLINK_ATT_href: return 122;
	case TAG_XLINK_ATT_role: return 123;
	case TAG_XLINK_ATT_show: return 124;
	case TAG_XLINK_ATT_title: return 125;
	case TAG_XLINK_ATT_type: return 126;
	case TAG_XML_ATT_base: return 127;
	case TAG_XML_ATT_lang: return 128;
	case TAG_SVG_ATT_y: return 129;
	case TAG_SVG_ATT_y1: return 130;
	case TAG_SVG_ATT_y2: return 131;
	case TAG_SVG_ATT_zoomAndPan: return 132;
	default: return -1;
	}
}



s32 gf_lsr_rare_type_from_attribute(u32 tag) {
	switch(tag) {
	case TAG_SVG_ATT__class: return 0;
	case TAG_SVG_ATT_audio_level: return 1;
	case TAG_SVG_ATT_color: return 2;
	case TAG_SVG_ATT_color_rendering: return 3;
	case TAG_SVG_ATT_display: return 4;
	case TAG_SVG_ATT_display_align: return 5;
	case TAG_SVG_ATT_fill_opacity: return 6;
	case TAG_SVG_ATT_fill_rule: return 7;
	case TAG_SVG_ATT_image_rendering: return 8;
	case TAG_SVG_ATT_line_increment: return 9;
	case TAG_SVG_ATT_pointer_events: return 10;
	case TAG_SVG_ATT_shape_rendering: return 11;
	case TAG_SVG_ATT_solid_color: return 12;
	case TAG_SVG_ATT_solid_opacity: return 13;
	case TAG_SVG_ATT_stop_color: return 14;
	case TAG_SVG_ATT_stop_opacity: return 15;
	case TAG_SVG_ATT_stroke_dasharray: return 16;
	case TAG_SVG_ATT_stroke_dashoffset: return 17;
	case TAG_SVG_ATT_stroke_linecap: return 18;
	case TAG_SVG_ATT_stroke_linejoin: return 19;
	case TAG_SVG_ATT_stroke_miterlimit: return 20;
	case TAG_SVG_ATT_stroke_opacity: return 21;
	case TAG_SVG_ATT_stroke_width: return 22;
	case TAG_SVG_ATT_text_anchor: return 23;
	case TAG_SVG_ATT_text_rendering: return 24;
	case TAG_SVG_ATT_viewport_fill: return 25;
	case TAG_SVG_ATT_viewport_fill_opacity: return 26;
	case TAG_SVG_ATT_vector_effect: return 27;
	case TAG_SVG_ATT_visibility: return 28;
	case TAG_SVG_ATT_requiredExtensions: return 29;
	case TAG_SVG_ATT_requiredFeatures: return 30;
	case TAG_SVG_ATT_requiredFormats: return 31;
	case TAG_SVG_ATT_systemLanguage: return 32;
	case TAG_XML_ATT_base: return 33;
	case TAG_XML_ATT_lang: return 34;
	case TAG_XML_ATT_space: return 35;
	case TAG_SVG_ATT_nav_next: return 36;
	case TAG_SVG_ATT_nav_up: return 37;
	case TAG_SVG_ATT_nav_up_left: return 38;
	case TAG_SVG_ATT_nav_up_right: return 39;
	case TAG_SVG_ATT_nav_prev: return 40;
	case TAG_SVG_ATT_nav_down: return 41;
	case TAG_SVG_ATT_nav_down_left: return 42;
	case TAG_SVG_ATT_nav_down_right: return 43;
	case TAG_SVG_ATT_nav_left: return 44;
	case TAG_SVG_ATT_focusable: return 45;
	case TAG_SVG_ATT_nav_right: return 46;
	case TAG_SVG_ATT_transform: return 47;
	case TAG_SVG_ATT_text_decoration: return 48;
	case TAG_SVG_ATT_syncMaster: return 49;
	case TAG_SVG_ATT_focusHighlight: return 49;
	case TAG_SVG_ATT_initialVisibility: return 49;
	case TAG_SVG_ATT_fullscreen: return 49;
	case TAG_SVG_ATT_requiredFonts: return 49;
	case TAG_SVG_ATT_font_variant: return 50;
	case TAG_SVG_ATT_font_family: return 51;
	case TAG_SVG_ATT_font_size: return 52;
	case TAG_SVG_ATT_font_style: return 53;
	case TAG_SVG_ATT_font_weight: return 54;
	case TAG_XLINK_ATT_title: return 55;
	case TAG_XLINK_ATT_type: return 56;
	case TAG_XLINK_ATT_role: return 57;
	case TAG_XLINK_ATT_arcrole: return 58;
	case TAG_XLINK_ATT_actuate: return 59;
	case TAG_XLINK_ATT_show: return 60;
	case TAG_SVG_ATT_end: return 61;
	case TAG_SVG_ATT_max: return 62;
	case TAG_SVG_ATT_min: return 63;
	default: return -1;
	}
}



s32 gf_lsr_anim_type_to_attribute(u32 tag) {
	switch(tag) {
	case 0: return TAG_SVG_ATT_target;
	case 1: return TAG_SVG_ATT_accumulate;
	case 2: return TAG_SVG_ATT_additive;
	case 3: return TAG_SVG_ATT_audio_level;
	case 4: return TAG_SVG_ATT_bandwidth;
	case 5: return TAG_SVG_ATT_begin;
	case 6: return TAG_SVG_ATT_calcMode;
	case 7: return TAG_LSR_ATT_children;
	case 8: return TAG_SVG_ATT_choice;
	case 9: return TAG_SVG_ATT_clipBegin;
	case 10: return TAG_SVG_ATT_clipEnd;
	case 11: return TAG_SVG_ATT_color;
	case 12: return TAG_SVG_ATT_color_rendering;
	case 13: return TAG_SVG_ATT_cx;
	case 14: return TAG_SVG_ATT_cy;
	case 15: return TAG_SVG_ATT_d;
	case 16: return TAG_SVG_ATT_delta;
	case 17: return TAG_SVG_ATT_display;
	case 18: return TAG_SVG_ATT_display_align;
	case 19: return TAG_SVG_ATT_dur;
	case 20: return TAG_SVG_ATT_editable;
	case 21: return TAG_LSR_ATT_enabled;
	case 22: return TAG_SVG_ATT_end;
	case 23: return TAG_XMLEV_ATT_event;
	case 24: return TAG_SVG_ATT_externalResourcesRequired;
	case 25: return TAG_SVG_ATT_fill;
	case 26: return TAG_SVG_ATT_fill_opacity;
	case 27: return TAG_SVG_ATT_fill_rule;
	case 28: return TAG_SVG_ATT_focusable;
	case 29: return TAG_SVG_ATT_font_family;
	case 30: return TAG_SVG_ATT_font_size;
	case 31: return TAG_SVG_ATT_font_style;
	case 32: return TAG_SVG_ATT_font_variant;
	case 33: return TAG_SVG_ATT_font_weight;
	case 34: return TAG_SVG_ATT_fullscreen;
	case 35: return TAG_SVG_ATT_gradientUnits;
	case 36: return TAG_XMLEV_ATT_handler;
	case 37: return TAG_SVG_ATT_height;
	case 38: return TAG_SVG_ATT_image_rendering;
	case 39: return TAG_SVG_ATT_keyPoints;
	case 40: return TAG_SVG_ATT_keySplines;
	case 41: return TAG_SVG_ATT_keyTimes;
	case 42: return TAG_SVG_ATT_line_increment;
	case 43: return TAG_XMLEV_ATT_target;
	case 44: return TAG_SVG_ATT_mediaCharacterEncoding;
	case 45: return TAG_SVG_ATT_mediaContentEncodings;
	case 46: return TAG_SVG_ATT_mediaSize;
	case 47: return TAG_SVG_ATT_mediaTime;
	case 48: return TAG_SVG_ATT_nav_down;
	case 49: return TAG_SVG_ATT_nav_down_left;
	case 50: return TAG_SVG_ATT_nav_down_right;
	case 51: return TAG_SVG_ATT_nav_left;
	case 52: return TAG_SVG_ATT_nav_next;
	case 53: return TAG_SVG_ATT_nav_prev;
	case 54: return TAG_SVG_ATT_nav_right;
	case 55: return TAG_SVG_ATT_nav_up;
	case 56: return TAG_SVG_ATT_nav_up_left;
	case 57: return TAG_SVG_ATT_nav_up_right;
	case 58: return TAG_XMLEV_ATT_observer;
	case 59: return TAG_SVG_ATT_offset;
	case 60: return TAG_SVG_ATT_opacity;
	case 61: return TAG_SVG_ATT_overflow;
	case 62: return TAG_SVG_ATT_overlay;
	case 63: return TAG_SVG_ATT_path;
	case 64: return TAG_SVG_ATT_pathLength;
	case 65: return TAG_SVG_ATT_pointer_events;
	case 66: return TAG_SVG_ATT_points;
	case 67: return TAG_SVG_ATT_preserveAspectRatio;
	case 68: return TAG_SVG_ATT_r;
	case 69: return TAG_SVG_ATT_repeatCount;
	case 70: return TAG_SVG_ATT_repeatDur;
	case 71: return TAG_SVG_ATT_requiredExtensions;
	case 72: return TAG_SVG_ATT_requiredFeatures;
	case 73: return TAG_SVG_ATT_requiredFormats;
	case 74: return TAG_SVG_ATT_restart;
	case 75: return TAG_SVG_ATT_rotate;
	case 76: return TAG_LSR_ATT_rotation;
	case 77: return TAG_SVG_ATT_rx;
	case 78: return TAG_SVG_ATT_ry;
	case 79: return TAG_LSR_ATT_scale;
	case 80: return TAG_SVG_ATT_shape_rendering;
	case 81: return TAG_SVG_ATT_size;
	case 82: return TAG_SVG_ATT_solid_color;
	case 83: return TAG_SVG_ATT_solid_opacity;
	case 84: return TAG_SVG_ATT_stop_color;
	case 85: return TAG_SVG_ATT_stop_opacity;
	case 86: return TAG_SVG_ATT_stroke;
	case 87: return TAG_SVG_ATT_stroke_dasharray;
	case 88: return TAG_SVG_ATT_stroke_dashoffset;
	case 89: return TAG_SVG_ATT_stroke_linecap;
	case 90: return TAG_SVG_ATT_stroke_linejoin;
	case 91: return TAG_SVG_ATT_stroke_miterlimit;
	case 92: return TAG_SVG_ATT_stroke_opacity;
	case 93: return TAG_SVG_ATT_stroke_width;
	case 94: return TAG_LSR_ATT_svg_height;
	case 95: return TAG_LSR_ATT_svg_width;
	case 96: return TAG_SVG_ATT_syncBehavior;
	case 97: return TAG_SVG_ATT_syncBehaviorDefault;
	case 98: return TAG_SVG_ATT_syncReference;
	case 99: return TAG_SVG_ATT_syncTolerance;
	case 100: return TAG_SVG_ATT_syncToleranceDefault;
	case 101: return TAG_SVG_ATT_systemLanguage;
	case 102: return TAG_SVG_ATT_text_align;
	case 103: return TAG_SVG_ATT_text_anchor;
	case 104: return TAG_SVG_ATT_text_decoration;
	case 105: return TAG_LSR_ATT_text_display;
	case 106: return TAG_SVG_ATT_text_rendering;
	case 107: return TAG_LSR_ATT_textContent;
	case 108: return TAG_SVG_ATT_transform;
	case 109: return TAG_SVG_ATT_transformBehavior;
	case 110: return TAG_LSR_ATT_translation;
	case 111: return TAG_SVG_ATT_vector_effect;
	case 112: return TAG_SVG_ATT_viewBox;
	case 113: return TAG_SVG_ATT_viewport_fill;
	case 114: return TAG_SVG_ATT_viewport_fill_opacity;
	case 115: return TAG_SVG_ATT_visibility;
	case 116: return TAG_SVG_ATT_width;
	case 117: return TAG_SVG_ATT_x;
	case 118: return TAG_SVG_ATT_x1;
	case 119: return TAG_SVG_ATT_x2;
	case 120: return TAG_XLINK_ATT_actuate;
	case 121: return TAG_XLINK_ATT_arcrole;
	case 122: return TAG_XLINK_ATT_href;
	case 123: return TAG_XLINK_ATT_role;
	case 124: return TAG_XLINK_ATT_show;
	case 125: return TAG_XLINK_ATT_title;
	case 126: return TAG_XLINK_ATT_type;
	case 127: return TAG_XML_ATT_base;
	case 128: return TAG_XML_ATT_lang;
	case 129: return TAG_SVG_ATT_y;
	case 130: return TAG_SVG_ATT_y1;
	case 131: return TAG_SVG_ATT_y2;
	case 132: return TAG_SVG_ATT_zoomAndPan;
	default: return -1;
	}
}



s32 gf_lsr_rare_type_to_attribute(u32 tag) {
	switch(tag) {
	case 0: return TAG_SVG_ATT__class;
	case 1: return TAG_SVG_ATT_audio_level;
	case 2: return TAG_SVG_ATT_color;
	case 3: return TAG_SVG_ATT_color_rendering;
	case 4: return TAG_SVG_ATT_display;
	case 5: return TAG_SVG_ATT_display_align;
	case 6: return TAG_SVG_ATT_fill_opacity;
	case 7: return TAG_SVG_ATT_fill_rule;
	case 8: return TAG_SVG_ATT_image_rendering;
	case 9: return TAG_SVG_ATT_line_increment;
	case 10: return TAG_SVG_ATT_pointer_events;
	case 11: return TAG_SVG_ATT_shape_rendering;
	case 12: return TAG_SVG_ATT_solid_color;
	case 13: return TAG_SVG_ATT_solid_opacity;
	case 14: return TAG_SVG_ATT_stop_color;
	case 15: return TAG_SVG_ATT_stop_opacity;
	case 16: return TAG_SVG_ATT_stroke_dasharray;
	case 17: return TAG_SVG_ATT_stroke_dashoffset;
	case 18: return TAG_SVG_ATT_stroke_linecap;
	case 19: return TAG_SVG_ATT_stroke_linejoin;
	case 20: return TAG_SVG_ATT_stroke_miterlimit;
	case 21: return TAG_SVG_ATT_stroke_opacity;
	case 22: return TAG_SVG_ATT_stroke_width;
	case 23: return TAG_SVG_ATT_text_anchor;
	case 24: return TAG_SVG_ATT_text_rendering;
	case 25: return TAG_SVG_ATT_viewport_fill;
	case 26: return TAG_SVG_ATT_viewport_fill_opacity;
	case 27: return TAG_SVG_ATT_vector_effect;
	case 28: return TAG_SVG_ATT_visibility;
	case 29: return TAG_SVG_ATT_requiredExtensions;
	case 30: return TAG_SVG_ATT_requiredFeatures;
	case 31: return TAG_SVG_ATT_requiredFormats;
	case 32: return TAG_SVG_ATT_systemLanguage;
	case 33: return TAG_XML_ATT_base;
	case 34: return TAG_XML_ATT_lang;
	case 35: return TAG_XML_ATT_space;
	case 36: return TAG_SVG_ATT_nav_next;
	case 37: return TAG_SVG_ATT_nav_up;
	case 38: return TAG_SVG_ATT_nav_up_left;
	case 39: return TAG_SVG_ATT_nav_up_right;
	case 40: return TAG_SVG_ATT_nav_prev;
	case 41: return TAG_SVG_ATT_nav_down;
	case 42: return TAG_SVG_ATT_nav_down_left;
	case 43: return TAG_SVG_ATT_nav_down_right;
	case 44: return TAG_SVG_ATT_nav_left;
	case 45: return TAG_SVG_ATT_focusable;
	case 46: return TAG_SVG_ATT_nav_right;
	case 47: return TAG_SVG_ATT_transform;
	case 48: return TAG_SVG_ATT_text_decoration;
	case 50: return TAG_SVG_ATT_font_variant;
	case 51: return TAG_SVG_ATT_font_family;
	case 52: return TAG_SVG_ATT_font_size;
	case 53: return TAG_SVG_ATT_font_style;
	case 54: return TAG_SVG_ATT_font_weight;
	case 55: return TAG_XLINK_ATT_title;
	case 56: return TAG_XLINK_ATT_type;
	case 57: return TAG_XLINK_ATT_role;
	case 58: return TAG_XLINK_ATT_arcrole;
	case 59: return TAG_XLINK_ATT_actuate;
	case 60: return TAG_XLINK_ATT_show;
	case 61: return TAG_SVG_ATT_end;
	case 62: return TAG_SVG_ATT_max;
	case 63: return TAG_SVG_ATT_min;
	default: return -1;
	}
}



u32 gf_lsr_same_rare(SVGAllAttributes *elt_atts, SVGAllAttributes *base_atts)
{
	GF_FieldInfo f_elt, f_base;
	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT__class;
	f_elt.far_ptr = elt_atts->_class;
	f_base.far_ptr = base_atts->_class;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_audio_level;
	f_elt.far_ptr = elt_atts->audio_level;
	f_base.far_ptr = base_atts->audio_level;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Paint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_color;
	f_elt.far_ptr = elt_atts->color;
	f_base.far_ptr = base_atts->color;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_RenderingHint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_color_rendering;
	f_elt.far_ptr = elt_atts->color_rendering;
	f_base.far_ptr = base_atts->color_rendering;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Display_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_display;
	f_elt.far_ptr = elt_atts->display;
	f_base.far_ptr = base_atts->display;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_DisplayAlign_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_display_align;
	f_elt.far_ptr = elt_atts->display_align;
	f_base.far_ptr = base_atts->display_align;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_fill_opacity;
	f_elt.far_ptr = elt_atts->fill_opacity;
	f_base.far_ptr = base_atts->fill_opacity;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FillRule_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_fill_rule;
	f_elt.far_ptr = elt_atts->fill_rule;
	f_base.far_ptr = base_atts->fill_rule;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_RenderingHint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_image_rendering;
	f_elt.far_ptr = elt_atts->image_rendering;
	f_base.far_ptr = base_atts->image_rendering;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_line_increment;
	f_elt.far_ptr = elt_atts->line_increment;
	f_base.far_ptr = base_atts->line_increment;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_PointerEvents_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_pointer_events;
	f_elt.far_ptr = elt_atts->pointer_events;
	f_base.far_ptr = base_atts->pointer_events;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_RenderingHint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_shape_rendering;
	f_elt.far_ptr = elt_atts->shape_rendering;
	f_base.far_ptr = base_atts->shape_rendering;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Paint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_solid_color;
	f_elt.far_ptr = elt_atts->solid_color;
	f_base.far_ptr = base_atts->solid_color;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_solid_opacity;
	f_elt.far_ptr = elt_atts->solid_opacity;
	f_base.far_ptr = base_atts->solid_opacity;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Paint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stop_color;
	f_elt.far_ptr = elt_atts->stop_color;
	f_base.far_ptr = base_atts->stop_color;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stop_opacity;
	f_elt.far_ptr = elt_atts->stop_opacity;
	f_base.far_ptr = base_atts->stop_opacity;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_StrokeDashArray_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_dasharray;
	f_elt.far_ptr = elt_atts->stroke_dasharray;
	f_base.far_ptr = base_atts->stroke_dasharray;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Length_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_dashoffset;
	f_elt.far_ptr = elt_atts->stroke_dashoffset;
	f_base.far_ptr = base_atts->stroke_dashoffset;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_StrokeLineCap_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_linecap;
	f_elt.far_ptr = elt_atts->stroke_linecap;
	f_base.far_ptr = base_atts->stroke_linecap;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_StrokeLineJoin_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_linejoin;
	f_elt.far_ptr = elt_atts->stroke_linejoin;
	f_base.far_ptr = base_atts->stroke_linejoin;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_miterlimit;
	f_elt.far_ptr = elt_atts->stroke_miterlimit;
	f_base.far_ptr = base_atts->stroke_miterlimit;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_opacity;
	f_elt.far_ptr = elt_atts->stroke_opacity;
	f_base.far_ptr = base_atts->stroke_opacity;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Length_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_stroke_width;
	f_elt.far_ptr = elt_atts->stroke_width;
	f_base.far_ptr = base_atts->stroke_width;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_TextAnchor_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_text_anchor;
	f_elt.far_ptr = elt_atts->text_anchor;
	f_base.far_ptr = base_atts->text_anchor;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_RenderingHint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_text_rendering;
	f_elt.far_ptr = elt_atts->text_rendering;
	f_base.far_ptr = base_atts->text_rendering;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Paint_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_viewport_fill;
	f_elt.far_ptr = elt_atts->viewport_fill;
	f_base.far_ptr = base_atts->viewport_fill;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Number_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_viewport_fill_opacity;
	f_elt.far_ptr = elt_atts->viewport_fill_opacity;
	f_base.far_ptr = base_atts->viewport_fill_opacity;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_VectorEffect_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_vector_effect;
	f_elt.far_ptr = elt_atts->vector_effect;
	f_base.far_ptr = base_atts->vector_effect;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Visibility_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_visibility;
	f_elt.far_ptr = elt_atts->visibility;
	f_base.far_ptr = base_atts->visibility;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XMLRI_List_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_requiredExtensions;
	f_elt.far_ptr = elt_atts->requiredExtensions;
	f_base.far_ptr = base_atts->requiredExtensions;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XMLRI_List_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_requiredFeatures;
	f_elt.far_ptr = elt_atts->requiredFeatures;
	f_base.far_ptr = base_atts->requiredFeatures;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_StringList_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_requiredFormats;
	f_elt.far_ptr = elt_atts->requiredFormats;
	f_base.far_ptr = base_atts->requiredFormats;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_StringList_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_systemLanguage;
	f_elt.far_ptr = elt_atts->systemLanguage;
	f_base.far_ptr = base_atts->systemLanguage;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XMLRI_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XML_ATT_base;
	f_elt.far_ptr = elt_atts->xml_base;
	f_base.far_ptr = base_atts->xml_base;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_LanguageID_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XML_ATT_lang;
	f_elt.far_ptr = elt_atts->xml_lang;
	f_base.far_ptr = base_atts->xml_lang;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XML_Space_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XML_ATT_space;
	f_elt.far_ptr = elt_atts->xml_space;
	f_base.far_ptr = base_atts->xml_space;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_next;
	f_elt.far_ptr = elt_atts->nav_next;
	f_base.far_ptr = base_atts->nav_next;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_up;
	f_elt.far_ptr = elt_atts->nav_up;
	f_base.far_ptr = base_atts->nav_up;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_up_left;
	f_elt.far_ptr = elt_atts->nav_up_left;
	f_base.far_ptr = base_atts->nav_up_left;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_up_right;
	f_elt.far_ptr = elt_atts->nav_up_right;
	f_base.far_ptr = base_atts->nav_up_right;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_prev;
	f_elt.far_ptr = elt_atts->nav_prev;
	f_base.far_ptr = base_atts->nav_prev;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_down;
	f_elt.far_ptr = elt_atts->nav_down;
	f_base.far_ptr = base_atts->nav_down;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_down_left;
	f_elt.far_ptr = elt_atts->nav_down_left;
	f_base.far_ptr = base_atts->nav_down_left;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_down_right;
	f_elt.far_ptr = elt_atts->nav_down_right;
	f_base.far_ptr = base_atts->nav_down_right;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_left;
	f_elt.far_ptr = elt_atts->nav_left;
	f_base.far_ptr = base_atts->nav_left;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focusable_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_focusable;
	f_elt.far_ptr = elt_atts->focusable;
	f_base.far_ptr = base_atts->focusable;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Focus_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_nav_right;
	f_elt.far_ptr = elt_atts->nav_right;
	f_base.far_ptr = base_atts->nav_right;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_Transform_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_transform;
	f_elt.far_ptr = elt_atts->transform;
	f_base.far_ptr = base_atts->transform;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_text_decoration;
	f_elt.far_ptr = elt_atts->text_decoration;
	f_base.far_ptr = base_atts->text_decoration;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FontVariant_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_font_variant;
	f_elt.far_ptr = elt_atts->font_variant;
	f_base.far_ptr = base_atts->font_variant;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FontFamily_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_font_family;
	f_elt.far_ptr = elt_atts->font_family;
	f_base.far_ptr = base_atts->font_family;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FontSize_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_font_size;
	f_elt.far_ptr = elt_atts->font_size;
	f_base.far_ptr = base_atts->font_size;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FontStyle_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_font_style;
	f_elt.far_ptr = elt_atts->font_style;
	f_base.far_ptr = base_atts->font_style;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SVG_FontWeight_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_font_weight;
	f_elt.far_ptr = elt_atts->font_weight;
	f_base.far_ptr = base_atts->font_weight;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_title;
	f_elt.far_ptr = elt_atts->xlink_title;
	f_base.far_ptr = base_atts->xlink_title;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_type;
	f_elt.far_ptr = elt_atts->xlink_type;
	f_base.far_ptr = base_atts->xlink_type;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XMLRI_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_role;
	f_elt.far_ptr = elt_atts->xlink_role;
	f_base.far_ptr = base_atts->xlink_role;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = XMLRI_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_arcrole;
	f_elt.far_ptr = elt_atts->xlink_arcrole;
	f_base.far_ptr = base_atts->xlink_arcrole;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_actuate;
	f_elt.far_ptr = elt_atts->xlink_actuate;
	f_base.far_ptr = base_atts->xlink_actuate;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = DOM_String_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_XLINK_ATT_show;
	f_elt.far_ptr = elt_atts->xlink_show;
	f_base.far_ptr = base_atts->xlink_show;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SMIL_Times_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_end;
	f_elt.far_ptr = elt_atts->end;
	f_base.far_ptr = base_atts->end;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SMIL_Duration_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_max;
	f_elt.far_ptr = elt_atts->max;
	f_base.far_ptr = base_atts->max;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	f_elt.fieldType = f_base.fieldType = SMIL_Duration_datatype;
	f_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_min;
	f_elt.far_ptr = elt_atts->min;
	f_base.far_ptr = base_atts->min;
	if (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;

	return 1;
}

