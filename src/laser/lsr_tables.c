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


#include <gpac/nodes_svg.h>

static const s32 a_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 70, 71, -1, 72, 100, 105, 0
};

static const s32 animate_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 3, -1, -1, -1, -1, -1, 7, 39, 40, 1, 2, -1
};

static const s32 animateColor_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 3, -1, -1, -1, -1, -1, 7, 39, 40, 1, 2, -1
};

static const s32 animateMotion_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, -1, -1, -1, -1, 7, 39, 40, 1, 2, -1, 62, 38, 74, -1
};

static const s32 animateTransform_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, 7, 39, 40, 1, 2, -1
};

static const s32 animation_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 25, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 95, 98, -1, 97, 70, 71, -1, 72, 100, 105, 114, 126, 113, 36, 66, -1
};

static const s32 audio_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 25, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 10, 11, 95, 98, -1, 97, 70, 71, -1, 72, 100
};

static const s32 circle_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 14, 15, 67
};

static const s32 conditional_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1, -1
};

static const s32 cursorManager_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 114, 126
};

static const s32 defs_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108
};

static const s32 desc_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 discard_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6
};

static const s32 ellipse_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 76, 77, 14, 15
};

static const s32 font_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1, -1
};

static const s32 font_face_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 29, 31, 33, 32, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_name_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1
};

static const s32 font_face_src_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 font_face_uri_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123
};

static const s32 foreignObject_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 70, 71, -1, 72, 100, 105, 114, 126, 113, 36
};

static const s32 g_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105
};

static const s32 glyph_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1, -1, -1, -1, -1, 16
};

static const s32 handler_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1
};

static const s32 hkern_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1, -1, -1, -1, -1
};

static const s32 image_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 59, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 70, 71, -1, 72, 100, 105, 114, 126, 113, 36, 66
};

static const s32 line_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 115, 127, 116, 128
};

static const s32 linearGradient_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, 119, 121, 122, 117, 120, 118, 123, 34, -1, -1, 115, 127, 116, 128
};

static const s32 listener_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 23, -1, -1, -1, 57, -1, 35, 21
};

static const s32 metadata_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 missing_glyph_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, -1, 16
};

static const s32 mpath_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123
};

static const s32 path_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 63, 16
};

static const s32 polygon_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 65
};

static const s32 polyline_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 65
};

static const s32 prefetch_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 45, 46, 43, 44, 5
};

static const s32 radialGradient_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, 119, 121, 122, 117, 120, 118, 123, -1, -1, 34, -1, -1, 14, 15, 67
};

static const s32 rect_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 114, 126, 113, 36, 76, 77
};

static const s32 rectClip_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 80
};

static const s32 script_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 selector_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 9
};

static const s32 set_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 3, -1, -1, -1
};

static const s32 simpleLayout_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, -1
};

static const s32 solidColor_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108
};

static const s32 stop_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, 58
};

static const s32 svg_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 96, 99, 109, 129, -1, -1, -1, -1, -1, -1, 114, 126, 95, 94, 66
};

static const s32 switch_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105
};

static const s32 tbreak_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 text_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 114, 126, 74, 20
};

static const s32 textArea_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100, 105, 113, 36, 114, 126, 20
};

static const s32 title_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24
};

static const s32 tspan_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 70, 71, -1, 72, 100
};

static const s32 use_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 12, 13, 18, 25, 26, 27, 29, 30, 31, 33, 41, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 101, 108, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 70, 71, -1, 72, 100, 105, 114, 126
};

static const s32 video_field_to_attrib_type[] = {
-1, -1, -1, 125, 124, -1, 24, 4, 17, 37, 64, 79, 103, 110, 111, 112, 25, -1, 28, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 119, 121, 122, 117, 120, 118, 123, 6, 22, 19, 68, 69, 73, -1, -1, -1, 10, 11, 95, 98, -1, 97, 70, 71, -1, 72, 100, 105, 106, 61, 114, 126, 113, 36, 66, -1
};

s32 gf_lsr_field_to_attrib_type(GF_Node *n, u32 fieldIndex)
{
	if(!n) return -2;
	switch (gf_node_get_tag(n)) {
	case TAG_SVG_a:
		return a_field_to_attrib_type[fieldIndex];
	case TAG_SVG_animate:
		return animate_field_to_attrib_type[fieldIndex];
	case TAG_SVG_animateColor:
		return animateColor_field_to_attrib_type[fieldIndex];
	case TAG_SVG_animateMotion:
		return animateMotion_field_to_attrib_type[fieldIndex];
	case TAG_SVG_animateTransform:
		return animateTransform_field_to_attrib_type[fieldIndex];
	case TAG_SVG_animation:
		return animation_field_to_attrib_type[fieldIndex];
	case TAG_SVG_audio:
		return audio_field_to_attrib_type[fieldIndex];
	case TAG_SVG_circle:
		return circle_field_to_attrib_type[fieldIndex];
	case TAG_SVG_conditional:
		return conditional_field_to_attrib_type[fieldIndex];
	case TAG_SVG_cursorManager:
		return cursorManager_field_to_attrib_type[fieldIndex];
	case TAG_SVG_defs:
		return defs_field_to_attrib_type[fieldIndex];
	case TAG_SVG_desc:
		return desc_field_to_attrib_type[fieldIndex];
	case TAG_SVG_discard:
		return discard_field_to_attrib_type[fieldIndex];
	case TAG_SVG_ellipse:
		return ellipse_field_to_attrib_type[fieldIndex];
	case TAG_SVG_font:
		return font_field_to_attrib_type[fieldIndex];
	case TAG_SVG_font_face:
		return font_face_field_to_attrib_type[fieldIndex];
	case TAG_SVG_font_face_name:
		return font_face_name_field_to_attrib_type[fieldIndex];
	case TAG_SVG_font_face_src:
		return font_face_src_field_to_attrib_type[fieldIndex];
	case TAG_SVG_font_face_uri:
		return font_face_uri_field_to_attrib_type[fieldIndex];
	case TAG_SVG_foreignObject:
		return foreignObject_field_to_attrib_type[fieldIndex];
	case TAG_SVG_g:
		return g_field_to_attrib_type[fieldIndex];
	case TAG_SVG_glyph:
		return glyph_field_to_attrib_type[fieldIndex];
	case TAG_SVG_handler:
		return handler_field_to_attrib_type[fieldIndex];
	case TAG_SVG_hkern:
		return hkern_field_to_attrib_type[fieldIndex];
	case TAG_SVG_image:
		return image_field_to_attrib_type[fieldIndex];
	case TAG_SVG_line:
		return line_field_to_attrib_type[fieldIndex];
	case TAG_SVG_linearGradient:
		return linearGradient_field_to_attrib_type[fieldIndex];
	case TAG_SVG_listener:
		return listener_field_to_attrib_type[fieldIndex];
	case TAG_SVG_metadata:
		return metadata_field_to_attrib_type[fieldIndex];
	case TAG_SVG_missing_glyph:
		return missing_glyph_field_to_attrib_type[fieldIndex];
	case TAG_SVG_mpath:
		return mpath_field_to_attrib_type[fieldIndex];
	case TAG_SVG_path:
		return path_field_to_attrib_type[fieldIndex];
	case TAG_SVG_polygon:
		return polygon_field_to_attrib_type[fieldIndex];
	case TAG_SVG_polyline:
		return polyline_field_to_attrib_type[fieldIndex];
	case TAG_SVG_prefetch:
		return prefetch_field_to_attrib_type[fieldIndex];
	case TAG_SVG_radialGradient:
		return radialGradient_field_to_attrib_type[fieldIndex];
	case TAG_SVG_rect:
		return rect_field_to_attrib_type[fieldIndex];
	case TAG_SVG_rectClip:
		return rectClip_field_to_attrib_type[fieldIndex];
	case TAG_SVG_script:
		return script_field_to_attrib_type[fieldIndex];
	case TAG_SVG_selector:
		return selector_field_to_attrib_type[fieldIndex];
	case TAG_SVG_set:
		return set_field_to_attrib_type[fieldIndex];
	case TAG_SVG_simpleLayout:
		return simpleLayout_field_to_attrib_type[fieldIndex];
	case TAG_SVG_solidColor:
		return solidColor_field_to_attrib_type[fieldIndex];
	case TAG_SVG_stop:
		return stop_field_to_attrib_type[fieldIndex];
	case TAG_SVG_svg:
		return svg_field_to_attrib_type[fieldIndex];
	case TAG_SVG_switch:
		return switch_field_to_attrib_type[fieldIndex];
	case TAG_SVG_tbreak:
		return tbreak_field_to_attrib_type[fieldIndex];
	case TAG_SVG_text:
		return text_field_to_attrib_type[fieldIndex];
	case TAG_SVG_textArea:
		return textArea_field_to_attrib_type[fieldIndex];
	case TAG_SVG_title:
		return title_field_to_attrib_type[fieldIndex];
	case TAG_SVG_tspan:
		return tspan_field_to_attrib_type[fieldIndex];
	case TAG_SVG_use:
		return use_field_to_attrib_type[fieldIndex];
	case TAG_SVG_video:
		return video_field_to_attrib_type[fieldIndex];
	default:
		return -2;
	}
}

