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


void buildGlobalAttributeList(GF_List *svg_elements, GF_List *all_atts)
{
	u32 i, j, k;
	Bool added = 0;

	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		for (j = 0; j < gf_list_count(elt->generic_attributes); j++) {
			SVGGenAttribute *att = gf_list_get(elt->generic_attributes, j);
			added = 0;
			if (!strcmp(att->impl_type, "SMIL_Fill")) {
				strcpy(att->implementation_name, "smil_fill");
			} else if (!strcmp(att->impl_type, "SVG_TransformType")) {
				strcpy(att->implementation_name, "transform_type");
			}
			for (k = 0; k < gf_list_count(all_atts); k++) {
				SVGGenAttribute *a = gf_list_get(all_atts, k);
				if (!strcmp(a->implementation_name, att->implementation_name)
				        && !strcmp(a->impl_type, att->impl_type)) {
					added = 1;
					break;
				}
			}
			if (!added) {
				gf_list_add(all_atts, att);
			}
		}
		for (j = 0; j < gf_list_count(elt->attributes); j++) {
			SVGGenAttribute *att = gf_list_get(elt->attributes, j);
			added = 0;
			if (!strcmp(elt->svg_name, "text")) {
				if (!strcmp(att->implementation_name, "x")) {
					strcpy(att->implementation_name, "text_x");
				} else if (!strcmp(att->implementation_name, "y")) {
					strcpy(att->implementation_name, "text_y");
				} else if (!strcmp(att->implementation_name, "rotate")) {
					strcpy(att->implementation_name, "text_rotate");
				}
			} else if (!strcmp(elt->svg_name, "listener") && !strcmp(att->implementation_name, "target")) {
				strcpy(att->implementation_name, "listener_target");
			} else if (!strcmp(elt->svg_name, "a") && !strcmp(att->implementation_name, "target")) {
				strcpy(att->impl_type, "SVG_String");
			} else if (!strcmp(elt->svg_name, "cursorManager")) {
				if (!strcmp(att->implementation_name, "x")) {
					strcpy(att->implementation_name, "cursorManager_x");
				} else if (!strcmp(att->implementation_name, "y")) {
					strcpy(att->implementation_name, "cursorManager_y");
				}
			}
			for (k = 0; k < gf_list_count(all_atts); k++) {
				SVGGenAttribute *a = gf_list_get(all_atts, k);
				if (!strcmp(a->implementation_name, att->implementation_name)
				        && !strcmp(a->impl_type, att->impl_type)) {
					added = 1;
					break;
				}
			}
			if (!added) {
				gf_list_add(all_atts, att);
			}
		}
	}
	/*motionTransform is not parsed in rng*/
	{
		SVGGenAttribute *att = NewSVGGenAttribute();
		strcpy(att->implementation_name, "motionTransform");
		strcpy(att->impl_type, "SVG_Motion");
		att->svg_name = "motionTransform";
		att->svg_type = "SVG_Motion";
		gf_list_add(all_atts, att);
	}
}

void generateSVGCode_V3(GF_List *svg_elements)
{
	FILE *output;
	u32 i;
	GF_List *all_atts = gf_list_new();

	buildGlobalAttributeList(svg_elements, all_atts);

	/***************************************************/
	/***************************************************/
	/*************** Creating .h file ******************/
	/***************************************************/
	/***************************************************/
	output = BeginFile(0);
	fprintf(output, "#include <gpac/scenegraph_svg.h>\n\n\n");

	/* Generation of ELEMENT tags */
	fprintf(output, "/* Definition of SVG 3 Alternate element internal tags */\n");
	fprintf(output, "/* TAG names are made of \"TAG_SVG\" + SVG element name (with - replaced by _) */\n");
	fprintf(output, "enum {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		if (i == 0) {
			fprintf(output, "\tTAG_SVG_%s = GF_NODE_RANGE_FIRST_SVG", elt->implementation_name);
		} else {
			fprintf(output, ",\n\tTAG_SVG_%s", elt->implementation_name);
		}
	}
	fprintf(output, ",\n\t/*undefined elements (when parsing) use this tag*/\n\tTAG_SVG_UndefinedElement\n};\n\n");

	/* Generation of ATTRIBUTE tags */
	fprintf(output, "/* Definition of SVG 3 attribute internal tags - %d defined */\n", gf_list_count(all_atts));
	fprintf(output, "/* TAG names are made of \"TAG_SVG_ATT_\" + SVG attribute name (with - replaced by _) */\n");
	fprintf(output, "enum {\n");

	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		if (i) fprintf(output, "\tTAG_SVG_ATT_%s,\n", att->implementation_name);
		else fprintf(output, "\tTAG_SVG_ATT_%s = TAG_SVG_ATT_RANGE_FIRST,\n", att->implementation_name);
	}
	fprintf(output, "\t/*undefined attributes (when parsing) use this tag*/\n\tTAG_SVG_ATT_Unknown\n};\n\n");

	/* Generation of the flatten structure pointing to all possible attributes in SVG */
	fprintf(output, "struct _all_atts {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t%s *%s;\n", att->impl_type, att->implementation_name);
	}
	fprintf(output, "};\n");

	EndFile(output, 0);

	/***************************************************/
	/***************************************************/
	/*************** Creating .c file ******************/
	/***************************************************/
	/***************************************************/
	output = BeginFile(1);
	fprintf(output, "#ifndef GPAC_DISABLE_SVG\n\n");
	fprintf(output, "#include <gpac/internal/scenegraph_dev.h>\n\n");
	fprintf(output, "#include <gpac/nodes_svg_da.h>\n\n");


	/****************************************************************/
	/* u32 gf_svg_get_attribute_tag(u32 element_tag, const char *attribute_name) */
	/****************************************************************/
	fprintf(output, "u32 gf_svg_get_attribute_tag(u32 element_tag, const char *attribute_name)\n{\n\tif (!attribute_name) return TAG_SVG_ATT_Unknown;\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);

		/* get rid of duplicates */
		if (!strcmp(att->impl_type, "SMIL_Fill")) continue;
		if (!strcmp(att->impl_type, "SVG_ContentType")) continue;
		if (!strcmp(att->implementation_name, "text_x")) continue;
		if (!strcmp(att->implementation_name, "text_y")) continue;
		if (!strcmp(att->implementation_name, "text_rotate")) continue;
		if (!strcmp(att->implementation_name, "cursorManager_x")) continue;
		if (!strcmp(att->implementation_name, "cursorManager_y")) continue;

		if (!strcmp(att->svg_name, "x") || !strcmp(att->svg_name, "y")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG_text) return TAG_SVG_ATT_text_%s;\n", att->implementation_name);
			fprintf(output, "\t\telse if (element_tag == TAG_SVG_cursorManager) return TAG_SVG_ATT_cursorManager_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "rotate")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG_text) return TAG_SVG_ATT_text_%s;\n", att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "type")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG_animateTransform) return TAG_SVG_ATT_transform_type;\n");
			fprintf(output, "\t\telse return TAG_SVG_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "fill")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG_animate || element_tag == TAG_SVG_animateColor || element_tag == TAG_SVG_animateMotion || element_tag == TAG_SVG_animateTransform || element_tag == TAG_SVG_animation || element_tag == TAG_SVG_audio || element_tag == TAG_SVG_video || element_tag == TAG_SVG_set) return TAG_SVG_ATT_smil_fill;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) return TAG_SVG_ATT_%s;\n", att->svg_name, att->implementation_name);
		}
	}
	fprintf(output, "\treturn TAG_SVG_ATT_Unknown;\n}\n\n");

	/****************************************************************/
	/* u32 gf_svg_get_attribute_type(u32 tag) */
	/****************************************************************/
	fprintf(output, "u32 gf_svg_get_attribute_type(u32 tag)\n{\n");
	fprintf(output, "\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t\tcase TAG_SVG_ATT_%s: return %s_datatype;\n", att->implementation_name, att->impl_type);
	}
	fprintf(output, "\t\tdefault: return SVG_Unknown_datatype;\n");
	fprintf(output, "\t}\n");
	fprintf(output, "\treturn TAG_SVG_ATT_Unknown;\n}\n\n");

	/****************************************************************/
	/* const char* gf_svg_get_attribute_name(u32 tag) */
	/****************************************************************/
	fprintf(output, "const char*gf_svg_get_attribute_name(u32 tag)\n{\n");
	fprintf(output, "\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t\tcase TAG_SVG_ATT_%s: return \"%s\";\n", att->implementation_name, att->svg_name);
	}
	fprintf(output, "\t\tdefault: return \"unknown\";\n");
	fprintf(output, "\t}\n");
	fprintf(output, "}\n\n");

	/***************************************************/
	/* SVGAttribute *gf_svg_create_attribute(GF_Node *node, u32 tag) */
	/***************************************************/
	fprintf(output, "SVGAttribute *gf_svg_create_attribute(GF_Node *node, u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\tcase TAG_SVG_ATT_%s: return gf_svg_create_attribute_from_datatype(%s_datatype, tag);\n", att->implementation_name, att->impl_type);
	}
	fprintf(output, "\tdefault: return NULL;\n\t}\n}\n\n");

	/****************************************************************/
	/* void gf_svg_flatten_attributes(SVG_Element *e, SVGAllAttributes *all_atts) */
	/****************************************************************/
	fprintf(output, "void gf_svg_flatten_attributes(SVG_Element *e, SVGAllAttributes *all_atts)\n");

	fprintf(output, "{\n");
	fprintf(output, "\tSVGAttribute *att;\n");
	fprintf(output, "\tmemset(all_atts, 0, sizeof(SVGAllAttributes));\n");
	fprintf(output, "\tif (e->sgprivate->tag <= GF_NODE_FIRST_DOM_NODE_TAG) return;\n");
	fprintf(output, "\tatt = e->attributes;\n");
	fprintf(output, "\twhile (att) {\n");
	fprintf(output, "\t\tswitch(att->tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t\tcase TAG_SVG_ATT_%s: all_atts->%s = (%s *)att->data; break;\n", att->implementation_name, att->implementation_name, att->impl_type);
	}
	fprintf(output, "\t\t}\n");
	fprintf(output, "\tatt = att->next;\n");
	fprintf(output, "\t}\n");
	fprintf(output, "}\n");
	fprintf(output, "\n");

	/****************************************************************/
	/* u32 gf_svg_get_element_tag(const char *element_name) */
	/****************************************************************/
	fprintf(output, "u32 gf_svg_get_element_tag(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG_%s;\n", elt->svg_name, elt->implementation_name);
	}
	fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");

	/***************************************************/
	/* const char *gf_svg_get_element_name(u32 tag) */
	/***************************************************/
	fprintf(output, "const char *gf_svg_get_element_name(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
	}
	fprintf(output, "\tdefault: return \"TAG_SVG_UndefinedNode\";\n\t}\n}\n\n");

	fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
	EndFile(output, 1);


	generate_laser_tables_da(all_atts);

	gf_list_del(all_atts);

}

