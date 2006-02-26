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
	DO NOT MOFIFY - File generated on GMT Sat Feb 25 10:56:16 2006

	BY SVGGen for GPAC Version 0.4.1-DEV
*/


#include <gpac/nodes_svg.h>

static const s32 a_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 71, 72, -1, 73, 101, 105, -1
};

static const s32 animate_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 3, -1, 106, 44, 7, 111, 8, 51, 52, 0, 1, -1
};

static const s32 animateColor_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 3, -1, 106, 44, 7, 111, 8, 51, 52, 0, 1, -1
};

static const s32 animateMotion_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 106, 44, 7, 111, 8, 51, 52, 0, 1, -1, 63, 50, 75, -1
};

static const s32 animateTransform_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 3, -1, 106, 44, 7, 111, 110, 8, 51, 52, 0, 1, -1
};

static const s32 animation_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 25, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 96, 99, -1, 98, 71, 72, -1, 73, 101, 105, 118, 131, 117, 47, 67, -1
};

static const s32 audio_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 25, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, -1, -1, 96, 99, -1, 98, 71, 72, -1, 73, 101
};

static const s32 circle_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 13, 14, 68
};

static const s32 defs_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112
};

static const s32 desc_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 discard_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6
};

static const s32 ellipse_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 77, 78, 13, 14
};

static const s32 font_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1
};

static const s32 font_face_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 39, 41, 43, 42, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_name_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1
};

static const s32 font_face_src_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 font_face_uri_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127
};

static const s32 foreignObject_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 71, 72, -1, 73, 101, 105, 118, 131, 117, 47
};

static const s32 g_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, -1, -1
};

static const s32 glyph_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1, -1, -1, -1, 15
};

static const s32 handler_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1
};

static const s32 hkern_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1, -1, -1, -1
};

static const s32 image_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 60, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 71, 72, -1, 73, 101, 105, 118, 131, 117, 47, 67
};

static const s32 line_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 119, 132, 120, 133
};

static const s32 linearGradient_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, 45, 119, 132, 120, 133
};

static const s32 listener_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 23, -1, -1, -1, 58, -1, 46, -1, -1
};

static const s32 metadata_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 missing_glyph_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, 15
};

static const s32 mpath_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127
};

static const s32 path_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 64, 15
};

static const s32 polygon_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 66
};

static const s32 polyline_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 66
};

static const s32 prefetch_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 56, 57, 54, 55, 5
};

static const s32 radialGradient_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, 45, 13, 14, 68
};

static const s32 rect_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 118, 131, 117, 47, 77, 78
};

static const s32 script_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, -1, -1
};

static const s32 set_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, 3, -1, 106, -1
};

static const s32 solidColor_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112
};

static const s32 stop_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, 59
};

static const s32 svg_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 97, 100, 113, 134, -1, -1, -1, -1, -1, -1, 95, 94, 67
};

static const s32 switch_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105
};

static const s32 tbreak_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 text_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 118, 131, 75, 20
};

static const s32 textArea_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101, 105, 117, 47, 118, 131, 20
};

static const s32 title_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24
};

static const s32 tspan_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 71, 72, -1, 73, 101
};

static const s32 use_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 11, 12, 18, 25, 26, 27, 39, 40, 41, 43, 53, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 102, 112, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 71, 72, -1, 73, 101, 105, 118, 131
};

static const s32 video_field_to_attrib_type[] = {
-1, -1, -1, 127, 126, 128, 24, 4, 17, 48, 65, 80, 103, 114, 115, 116, 25, -1, 38, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 123, 125, 126, 121, 124, 122, 127, 6, 22, 19, 69, 70, 74, -1, -1, -1, -1, -1, 96, 99, -1, 98, 71, 72, -1, 73, 101, 105, 108, 62, 118, 131, 117, 47, 67, -1
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

