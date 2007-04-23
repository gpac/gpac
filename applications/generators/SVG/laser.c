/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004-2005
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
	"a.target", "accumulate", "additive", "attributeName", "audio-level", "bandwidth", "begin", "calcMode", "children", "choice", "clipBegin", "clipEnd", "color", "color-rendering", "cx", "cy", "d", "display", "display-align", "dur", "editable", "enabled", "end", "event", "externalResourcesRequired", "fill", "fill-opacity", "fill-rule", "focusable", "font-family", "font-size", "font-style", "font-variant", "font-weight", "gradientUnits", "handler", "height", "image-rendering", "keyPoints", "keySplines", "keyTimes", "line-increment", "listener.target", "mediaCharacterEncoding", "mediaContentEncodings", "mediaSize", "mediaTime", "nav-down", "nav-down-left", "nav-down-right", "nav-left", "nav-next", "nav-prev", "nav-right", "nav-up", "nav-up-left", "nav-up-right", "observer", "offset", "opacity", "overflow", "overlay", "path", "pathLength", "pointer-events", "points", "preserveAspectRatio", "r", "repeatCount", "repeatDur", "requiredExtensions", "requiredFeatures", "requiredFormats", "restart", "rotate", "rotation", "rx", "ry", "scale", "shape-rendering", "size", "solid-color", "solid-opacity", "stop-color", "stop-opacity", "stroke", "stroke-dasharray", "stroke-dashoffset", "stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-opacity", "stroke-width", "svg.height", "svg.width", "syncBehavior", "syncBehaviorDefault", "syncReference", "syncTolerance", "syncToleranceDefault", "systemLanguage", "text-anchor", "text-decoration", "text-rendering", "textContent", "transform", "transformBehavior", "translation", "vector-effect", "viewBox", "viewport-fill", "viewport-fill-opacity", "visibility", "width", "x", "x1", "x2", "xlink:actuate", "xlink:arcrole", "xlink:href", "xlink:role", "xlink:show", "xlink:title", "xlink:type", "xml:base", "xml:lang", "y", "y1", "y2", "zoomAndPan", NULL
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

