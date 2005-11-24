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
	DO NOT MOFIFY - File generated on GMT Thu Nov 24 14:29:53 2005

	BY SVGGen for GPAC Version 0.4.1-DEV
*/


#include <gpac/nodes_svg.h>

static const s32 a_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 53
};

static const s32 animate_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 animateColor_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 animateMotion_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1
};

static const s32 animateTransform_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, 56, -1, -1, -1, -1, -1
};

static const s32 animation_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 10, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, 55, 63, 67, 62, 27, 34, -1
};

static const s32 audio_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 10, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 circle_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60, 4, 5, 35
};

static const s32 defs_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57
};

static const s32 desc_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 discard_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1
};

static const s32 ellipse_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60, 37, 38, 4, 5
};

static const s32 font_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 22, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_name_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_src_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 font_face_uri_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1
};

static const s32 foreignObject_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 63, 67, 62, 27
};

static const s32 g_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55
};

static const s32 glyph_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 6
};

static const s32 handler_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 hkern_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 image_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 30, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 63, 67, 62, 27, 34
};

static const s32 line_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60, 64, 68, 65, 69
};

static const s32 linearGradient_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, 7, 61, 28, 32, 39, -1, 0, 59, 60, -1, -1, -1, -1, -1, 9, 64, 68, 65, 69
};

static const s32 listener_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 53, -1
};

static const s32 metadata_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 missing_glyph_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, -1, 6
};

static const s32 mpath_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1
};

static const s32 path_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 31, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60, 6
};

static const s32 polygon_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 33, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60
};

static const s32 polyline_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 33, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60
};

static const s32 prefetch_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 radialGradient_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, 7, 61, 28, 32, 39, -1, 0, 59, 60, -1, -1, -1, -1, -1, 9, 4, 5, 35
};

static const s32 rect_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, -1, -1, -1, 7, 61, 28, 32, 39, -1, 0, 59, 60, 63, 67, 62, 27, 37, 38
};

static const s32 script_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 set_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1
};

static const s32 solidColor_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57
};

static const s32 stop_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1
};

static const s32 svg_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 58, -1, -1, -1, -1, -1, -1, -1, 62, 27, 34
};

static const s32 switch_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55
};

static const s32 tbreak_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 text_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 63, 67, 36, 26
};

static const s32 textArea_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 62, 27, 63, 67, 26
};

static const s32 title_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1
};

static const s32 tspan_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const s32 use_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 2, 3, 8, 10, 11, 12, 22, 23, 24, 25, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 54, 57, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, 63, 67
};

static const s32 video_field_to_anim_type[] = {
-1, -1, -1, -1, -1, -1, -1, 0, 7, 28, 32, 39, -1, 59, 60, 61, 10, -1, 21, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 66, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1, 63, 67, 62, 27, 34, -1
};

s32 gf_lsr_field_to_anim_type(GF_Node *n, u32 fieldIndex)
{
	if(!n) return -2;
	switch (gf_node_get_tag(n)) {
	case TAG_SVG_a:
		return a_field_to_anim_type[fieldIndex];
	case TAG_SVG_animate:
		return animate_field_to_anim_type[fieldIndex];
	case TAG_SVG_animateColor:
		return animateColor_field_to_anim_type[fieldIndex];
	case TAG_SVG_animateMotion:
		return animateMotion_field_to_anim_type[fieldIndex];
	case TAG_SVG_animateTransform:
		return animateTransform_field_to_anim_type[fieldIndex];
	case TAG_SVG_animation:
		return animation_field_to_anim_type[fieldIndex];
	case TAG_SVG_audio:
		return audio_field_to_anim_type[fieldIndex];
	case TAG_SVG_circle:
		return circle_field_to_anim_type[fieldIndex];
	case TAG_SVG_defs:
		return defs_field_to_anim_type[fieldIndex];
	case TAG_SVG_desc:
		return desc_field_to_anim_type[fieldIndex];
	case TAG_SVG_discard:
		return discard_field_to_anim_type[fieldIndex];
	case TAG_SVG_ellipse:
		return ellipse_field_to_anim_type[fieldIndex];
	case TAG_SVG_font:
		return font_field_to_anim_type[fieldIndex];
	case TAG_SVG_font_face:
		return font_face_field_to_anim_type[fieldIndex];
	case TAG_SVG_font_face_name:
		return font_face_name_field_to_anim_type[fieldIndex];
	case TAG_SVG_font_face_src:
		return font_face_src_field_to_anim_type[fieldIndex];
	case TAG_SVG_font_face_uri:
		return font_face_uri_field_to_anim_type[fieldIndex];
	case TAG_SVG_foreignObject:
		return foreignObject_field_to_anim_type[fieldIndex];
	case TAG_SVG_g:
		return g_field_to_anim_type[fieldIndex];
	case TAG_SVG_glyph:
		return glyph_field_to_anim_type[fieldIndex];
	case TAG_SVG_handler:
		return handler_field_to_anim_type[fieldIndex];
	case TAG_SVG_hkern:
		return hkern_field_to_anim_type[fieldIndex];
	case TAG_SVG_image:
		return image_field_to_anim_type[fieldIndex];
	case TAG_SVG_line:
		return line_field_to_anim_type[fieldIndex];
	case TAG_SVG_linearGradient:
		return linearGradient_field_to_anim_type[fieldIndex];
	case TAG_SVG_listener:
		return listener_field_to_anim_type[fieldIndex];
	case TAG_SVG_metadata:
		return metadata_field_to_anim_type[fieldIndex];
	case TAG_SVG_missing_glyph:
		return missing_glyph_field_to_anim_type[fieldIndex];
	case TAG_SVG_mpath:
		return mpath_field_to_anim_type[fieldIndex];
	case TAG_SVG_path:
		return path_field_to_anim_type[fieldIndex];
	case TAG_SVG_polygon:
		return polygon_field_to_anim_type[fieldIndex];
	case TAG_SVG_polyline:
		return polyline_field_to_anim_type[fieldIndex];
	case TAG_SVG_prefetch:
		return prefetch_field_to_anim_type[fieldIndex];
	case TAG_SVG_radialGradient:
		return radialGradient_field_to_anim_type[fieldIndex];
	case TAG_SVG_rect:
		return rect_field_to_anim_type[fieldIndex];
	case TAG_SVG_script:
		return script_field_to_anim_type[fieldIndex];
	case TAG_SVG_set:
		return set_field_to_anim_type[fieldIndex];
	case TAG_SVG_solidColor:
		return solidColor_field_to_anim_type[fieldIndex];
	case TAG_SVG_stop:
		return stop_field_to_anim_type[fieldIndex];
	case TAG_SVG_svg:
		return svg_field_to_anim_type[fieldIndex];
	case TAG_SVG_switch:
		return switch_field_to_anim_type[fieldIndex];
	case TAG_SVG_tbreak:
		return tbreak_field_to_anim_type[fieldIndex];
	case TAG_SVG_text:
		return text_field_to_anim_type[fieldIndex];
	case TAG_SVG_textArea:
		return textArea_field_to_anim_type[fieldIndex];
	case TAG_SVG_title:
		return title_field_to_anim_type[fieldIndex];
	case TAG_SVG_tspan:
		return tspan_field_to_anim_type[fieldIndex];
	case TAG_SVG_use:
		return use_field_to_anim_type[fieldIndex];
	case TAG_SVG_video:
		return video_field_to_anim_type[fieldIndex];
	default:
		return -2;
	}
}

