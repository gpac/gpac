/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph Generator sub-project
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

#include "svggen.h"

static char *laser_attribute_name_type_list[] = {
	"target", "accumulate", "additive", "audio_level", "bandwidth", "begin", "calcMode", "children", "choice", "clipBegin", "clipEnd", "color", "color_rendering", "cx", "cy", "d", "delta", "display", "display_align", "dur", "editable", "lsr_enabled", "end", "event", "externalResourcesRequired", "fill", "fill_opacity", "fill_rule", "focusable", "font_family", "font_size", "font_style", "font_variant", "font_weight", "fullscreen", "gradientUnits", "handler", "height", "image_rendering", "keyPoints", "keySplines", "keyTimes", "line_increment", "listener_target", "mediaCharacterEncoding", "mediaContentEncodings", "mediaSize", "mediaTime", "nav_down", "nav_down_left", "nav_down_right", "nav_left", "nav_next", "nav_prev", "nav_right", "nav_up", "nav_up_left", "nav_up_right", "observer", "offset", "opacity", "overflow", "overlay", "path", "pathLength", "pointer_events", "points", "preserveAspectRatio", "r", "repeatCount", "repeatDur", "requiredExtensions", "requiredFeatures", "requiredFormats", "restart", "rotate", "rotation", "rx", "ry", "scale", "shape_rendering", "size", "solid_color", "solid_opacity", "stop_color", "stop_opacity", "stroke", "stroke_dasharray", "stroke_dashoffset", "stroke_linecap", "stroke_linejoin", "stroke_miterlimit", "stroke_opacity", "stroke_width", "svg_height", "svg_width", "syncBehavior", "syncBehaviorDefault", "syncReference", "syncTolerance", "syncToleranceDefault", "systemLanguage", "text_align", "text_anchor", "text_decoration", "text_display", "text_rendering", "textContent", "transform", "transformBehavior", "translation", "vector_effect", "viewBox", "viewport_fill", "viewport_fill_opacity", "visibility", "width", "x", "x1", "x2", "xlink_actuate", "xlink_arcrole", "xlink_href", "xlink_role", "xlink_show", "xlink_title", "xlink_type", "xml_base", "xml_lang", "y", "y1", "y2", "zoomAndPan", NULL
};



static char *laser_attribute_rare_type_list[] = {
	"_class", "audio_level", "color", "color_rendering", "display", "display_align", "fill_opacity",
	"fill_rule",  "image_rendering", "line_increment", "pointer_events", "shape_rendering", "solid_color",
	"solid_opacity", "stop_color", "stop_opacity", "stroke_dasharray", "stroke_dashoffset", "stroke_linecap",
	"stroke_linejoin", "stroke_miterlimit",	"stroke_opacity", "stroke_width", "text_anchor", "text_rendering",
	"viewport_fill", "viewport_fill_opacity", "vector_effect", "visibility", "requiredExtensions",
	"requiredFeatures", "requiredFormats", "systemLanguage", "xml_base", "xml_lang", "xml_space",
	"nav_next", "nav_up", "nav_up_left", "nav_up_right", "nav_prev", "nav_down", "nav_down_left",
	"nav_down_right", "nav_left", "focusable", "nav_right", "transform","text_decoration",
	"extension", /*LASER EXTENSIONS SVG*/

	"font_variant", "font_family", "font_size", "font_style", "font_weight", "xlink_title", "xlink_type",
	"xlink_role", "xlink_arcrole", "xlink_actuate", "xlink_show", "end", "max", "min",
	NULL
};


s32 get_lsr_att_name_type(const char *name)
{
	u32 i = 0;
	while (laser_attribute_name_type_list[i]) {
		if (!strcmp(name, laser_attribute_name_type_list[i])) return i;
		i++;
	}
	return -1;
}

void generateGenericAttrib(FILE *output, SVGGenElement *elt, u32 index)
{
	int k;
	for (k=0; k < generic_attributes[index].array_length; k++) {
		char *att_name = generic_attributes[index].array[k];
		SVGGenAttribute *a = findAttribute(elt, att_name);
		if (a) {
			s32 type = get_lsr_att_name_type(att_name);
			/*SMIL anim fill not updatable*/
			if ((index==6) && !strcmp(att_name, "fill")) {
				type = -1;
			}
			fprintf(output, ", %d", type);
		}
	}
}

void generate_laser_tables(GF_List *svg_elements)
{
	FILE *output;
	u32 i;
	u32 special_cases;

	output = BeginFile(2);
	if (generation_mode	== 1) fprintf(output, "\n#include <gpac/nodes_svg_sa.h>\n\n");
	else if (generation_mode == 2) fprintf(output, "\n#include <gpac/nodes_svg_sani.h>\n\n");
	else if (generation_mode == 3) fprintf(output, "\n#include <gpac/nodes_svg_da.h>\n\n");

	for (i=0; i<gf_list_count(svg_elements); i++) {
		u32 j, fcount;
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);

		fcount = gf_list_count(elt->attributes);

		fprintf(output, "static const s32 %s_field_to_attrib_type[] = {\n", elt->implementation_name);

		/*core info: id, xml:id, class, xml:lang, xml:base, xml:space, externalResourcesRequired*/
		fprintf(output, "-1, -1, -1, 125, 124, -1, 24");
		if (elt->has_media_properties) generateGenericAttrib(output, elt, 2);
		if (elt->has_properties) generateGenericAttrib(output, elt, 1);
		if (elt->has_opacity_properties) generateGenericAttrib(output, elt, 3);
		if (elt->has_focus) generateGenericAttrib(output, elt, 4);
		if (elt->has_xlink) generateGenericAttrib(output, elt, 5);
		if (elt->has_timing) generateGenericAttrib(output, elt, 6);
		if (elt->has_sync) generateGenericAttrib(output, elt, 7);
		if (elt->has_animation) generateGenericAttrib(output, elt, 8);
		if (elt->has_conditional) generateGenericAttrib(output, elt, 9);
		/*WATCHOUT - HARDCODED VALUES*/
		if (elt->has_transform) fprintf(output, ", 105");
		if (elt->has_xy) fprintf(output, ", 116, 129");


		/*svg.width and svg.height escapes*/
		special_cases = 0;
		if (!strcmp(elt->svg_name, "svg")) special_cases = 1;
		else if (!strcmp(elt->svg_name, "a")) special_cases = 2;

		for (j=0; j<fcount; j++) {
			SVGGenAttribute *att = gf_list_get(elt->attributes, j);
			s32 type = get_lsr_att_name_type(att->svg_name);
			if (special_cases==1) {
				if (!strcmp(att->svg_name, "width"))
					type = 95;
				else if (!strcmp(att->svg_name, "height"))
					type = 94;
			}
			if ((special_cases==2) && !strcmp(att->svg_name, "target"))
				type = 0;
			fprintf(output, ", %d", type);
		}
		fprintf(output, "\n};\n\n");

	}
	fprintf(output, "s32 gf_lsr_field_to_attrib_type(GF_Node *n, u32 fieldIndex)\n{\n\tif(!n) return -2;\n\tswitch (gf_node_get_tag(n)) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		u32 fcount;
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s:\n", elt->implementation_name);
		fcount = gf_list_count(elt->attributes);
		fprintf(output, "\t\treturn %s_field_to_attrib_type[fieldIndex];\n", elt->implementation_name);
	}
	fprintf(output, "\tdefault:\n\t\treturn -2;\n\t}\n}\n\n");
}


void generate_laser_tables_da(GF_List *atts)
{
	FILE *output;
	u32 i, count, j;

	output = BeginFile(2);

	fprintf(output, "\n#include <gpac/internal/laser_dev.h>\n\n");
	fprintf(output, "\n\ns32 gf_lsr_anim_type_from_attribute(u32 tag) {\n\tswitch(tag) {\n");

	count = gf_list_count(atts);
	j=0;
	while (laser_attribute_name_type_list[j]) {
		for (i=0; i<count; i++) {
			SVGGenAttribute *att = gf_list_get(atts, i);

			if (!strcmp(att->implementation_name, laser_attribute_name_type_list[j])) {
				fprintf(output, "\tcase TAG_SVG_ATT_%s: return %d;\n", att->implementation_name, j);
				break;
			}
		}
		if (i==count) {
			//fprintf(stdout, "Warning: Ignoring %s\n", laser_attribute_name_type_list[j]);
			fprintf(output, "\tcase TAG_LSR_ATT_%s: return %d;\n", laser_attribute_name_type_list[j], j);
		}
		j++;
	}
	fprintf(output, "\tdefault: return -1;\n\t}\n}\n\n");

	fprintf(output, "\n\ns32 gf_lsr_rare_type_from_attribute(u32 tag) {\n\tswitch(tag) {\n");
	count = gf_list_count(atts);
	j=0;
	while (laser_attribute_rare_type_list[j]) {
		for (i=0; i<count; i++) {
			SVGGenAttribute *att = gf_list_get(atts, i);

			if (!strcmp(att->implementation_name, laser_attribute_rare_type_list[j])) {
				fprintf(output, "\tcase TAG_SVG_ATT_%s: return %d;\n", att->implementation_name, j);
				break;
			}
		}
		if (i==count) {
			if (!strcmp(laser_attribute_rare_type_list[j], "extension")) {
				fprintf(output, "\tcase TAG_SVG_ATT_syncMaster: return %d;\n", j);
				fprintf(output, "\tcase TAG_SVG_ATT_focusHighlight: return %d;\n", j);
				fprintf(output, "\tcase TAG_SVG_ATT_initialVisibility: return %d;\n", j);
				fprintf(output, "\tcase TAG_SVG_ATT_fullscreen: return %d;\n", j);
				fprintf(output, "\tcase TAG_SVG_ATT_requiredFonts: return %d;\n", j);
			} else {
				fprintf(stdout, "Warning: Ignoring %s\n", laser_attribute_rare_type_list[j]);
			}
		}
		j++;
	}
	fprintf(output, "\tdefault: return -1;\n\t}\n}\n\n");



	fprintf(output, "\n\ns32 gf_lsr_anim_type_to_attribute(u32 tag) {\n\tswitch(tag) {\n");
	j=0;
	while (laser_attribute_name_type_list[j]) {
		for (i=0; i<count; i++) {
			SVGGenAttribute *att = gf_list_get(atts, i);

			if (!strcmp(att->implementation_name, laser_attribute_name_type_list[j])) {
				fprintf(output, "\tcase %d: return TAG_SVG_ATT_%s;\n", j, att->implementation_name);
				break;
			}
		}
		if (i==count) {
			fprintf(output, "\tcase %d: return TAG_LSR_ATT_%s;\n", j, laser_attribute_name_type_list[j]);
		}
		j++;
	}
	fprintf(output, "\tdefault: return -1;\n\t}\n}\n\n");

	fprintf(output, "\n\ns32 gf_lsr_rare_type_to_attribute(u32 tag) {\n\tswitch(tag) {\n");
	j=0;
	while (laser_attribute_rare_type_list[j]) {
		for (i=0; i<count; i++) {
			SVGGenAttribute *att = gf_list_get(atts, i);

			if (!strcmp(att->implementation_name, laser_attribute_rare_type_list[j])) {
				fprintf(output, "\tcase %d: return TAG_SVG_ATT_%s;\n", j, att->implementation_name);
				break;
			}
		}
		j++;
	}
	fprintf(output, "\tdefault: return -1;\n\t}\n}\n\n");


	fprintf(output, "\n\nu32 gf_lsr_same_rare(SVGAllAttributes *elt_atts, SVGAllAttributes *base_atts)\n{\n");
	fprintf(output, "\tGF_FieldInfo f_elt, f_base;\n");

	j=0;
	while (laser_attribute_rare_type_list[j]) {
		SVGGenAttribute *att = NULL;
		if (!strcmp(laser_attribute_rare_type_list[j], "extension")) {
			j++;
			continue;
		}
		for (i=0; i<count; i++) {
			att = gf_list_get(atts, i);
			if (!strcmp(att->implementation_name, laser_attribute_rare_type_list[j]))
				break;
			att = NULL;
		}
		assert(att);

		fprintf(output, "\tf_elt.fieldType = f_base.fieldType = %s_datatype;\n", att->impl_type);
		fprintf(output, "\tf_elt.fieldIndex = f_base.fieldIndex = TAG_SVG_ATT_%s;\n", laser_attribute_rare_type_list[j]);
		fprintf(output, "\tf_elt.far_ptr = elt_atts->%s;\n", laser_attribute_rare_type_list[j]);
		fprintf(output, "\tf_base.far_ptr = base_atts->%s;\n", laser_attribute_rare_type_list[j]);
		fprintf(output, "\tif (!gf_svg_attributes_equal(&f_elt, &f_base)) return 0;\n\n");

		j++;
	}
	fprintf(output, "\treturn 1;\n}\n\n");

	gf_fclose(output);
}
