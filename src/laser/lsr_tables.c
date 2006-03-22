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
	DO NOT MOFIFY - File generated on GMT Wed Mar 22 15:27:25 2006

	BY SVGGen for GPAC Version 0.4.1-DEV
*/


#include <gpac/nodes_svg.h>

static const s32 a_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 72, 73, -1, 74, 102, 105, 0
};

static const s32 animate_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 4, -1, 108, 35, 8, 113, 9, 42, 43, 1, 2, -1
};

static const s32 animateColor_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 4, -1, 108, 35, 8, 113, 9, 42, 43, 1, 2, -1
};

static const s32 animateMotion_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 108, 35, 8, 113, 9, 42, 43, 1, 2, -1, 64, 41, 76, -1
};

static const s32 animateTransform_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 4, -1, 108, 35, 8, 113, 112, 9, 42, 43, 1, 2, -1
};

static const s32 animation_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 26, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 97, 100, -1, 99, 72, 73, -1, 74, 102, 105, 120, 133, 119, 38, 68, -1
};

static const s32 audio_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 26, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, -1, -1, 97, 100, -1, 99, 72, 73, -1, 74, 102
};

static const s32 circle_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 14, 15, 69
};

static const s32 defs_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114
};

static const s32 desc_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 discard_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7
};

static const s32 ellipse_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 78, 79, 14, 15
};

static const s32 font_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1
};

static const s32 font_face_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 30, 32, 34, 33, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_name_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1
};

static const s32 font_face_src_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 font_face_uri_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129
};

static const s32 foreignObject_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 72, 73, -1, 74, 102, 105, 120, 133, 119, 38
};

static const s32 g_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, -1, -1
};

static const s32 glyph_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1, -1, -1, -1, 16
};

static const s32 handler_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1
};

static const s32 hkern_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1, -1, -1, -1
};

static const s32 image_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 61, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 72, 73, -1, 74, 102, 105, 120, 133, 119, 38, 68
};

static const s32 line_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 121, 134, 122, 135
};

static const s32 linearGradient_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, 125, 127, 128, 123, 126, 124, 129, 36, -1, -1, 121, 134, 122, 135
};

static const s32 listener_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 24, -1, -1, -1, 59, 103, 37, -1, -1
};

static const s32 metadata_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 missing_glyph_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, 16
};

static const s32 mpath_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129
};

static const s32 path_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 65, 16
};

static const s32 polygon_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 67
};

static const s32 polyline_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 67
};

static const s32 prefetch_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 47, 48, 45, 46, 6
};

static const s32 radialGradient_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, 125, 127, 128, 123, 126, 124, 129, -1, -1, 36, -1, -1, 14, 15, 69
};

static const s32 rect_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 120, 133, 119, 38, 78, 79
};

static const s32 script_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1
};

static const s32 set_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, 4, -1, 108, -1
};

static const s32 solidColor_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114
};

static const s32 stop_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, 60
};

static const s32 svg_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 98, 101, 115, 136, -1, -1, -1, -1, -1, -1, 120, 133, 95, 94, 68
};

static const s32 switch_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105
};

static const s32 tbreak_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 text_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 120, 133, 76, 21
};

static const s32 textArea_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102, 105, 119, 38, 120, 133, 21
};

static const s32 title_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 tspan_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 72, 73, -1, 74, 102
};

static const s32 use_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 12, 13, 19, 26, 27, 28, 30, 31, 32, 34, 44, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 104, 114, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 72, 73, -1, 74, 102, 105, 120, 133
};

static const s32 video_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 5, 18, 39, 66, 81, 105, 116, 117, 118, 26, -1, 29, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 125, 127, 128, 123, 126, 124, 129, 7, 23, 20, 70, 71, 75, -1, -1, -1, -1, -1, 97, 100, -1, 99, 72, 73, -1, 74, 102, 105, 110, 63, 120, 133, 119, 38, 68, -1
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
	case TAG_SVG_script:
		return script_field_to_attrib_type[fieldIndex];
	case TAG_SVG_set:
		return set_field_to_attrib_type[fieldIndex];
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

