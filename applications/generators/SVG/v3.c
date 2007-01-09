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

void generateNodeImpl3(FILE *output, SVGGenElement* svg_elt) 
{
	u32 i;	

	fprintf(output, "SVG3Attribute *gf_svg3_%s_create_attribute(u32 tag)\n",svg_elt->implementation_name);
	fprintf(output, "{\n");
	fprintf(output, "\tswitch(tag) {\n");
	for (i = 0; i < gf_list_count(svg_elt->generic_attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->generic_attributes, i);
		fprintf(output, "\t\tcase TAG_SVG3_ATT_%s: return gf_svg3_create_attribute_from_datatype(%s_datatype, tag);\n", att->implementation_name, att->impl_type);
	}

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
		fprintf(output, "\t\tcase TAG_SVG3_ATT_%s: return gf_svg3_create_attribute_from_datatype(%s_datatype, tag);\n", att->implementation_name, att->impl_type);
	}

	fprintf(output, "\t\tdefault: return NULL;\n");		
	fprintf(output, "\t}\n");
	fprintf(output, "}\n");		
	fprintf(output, "\n");		
}

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
			} else if (!strcmp(elt->svg_name, "cursorManager")) {
				if (!strcmp(att->implementation_name, "x")) {
					strcpy(att->implementation_name, "cursorManager_x");
				} else if (!strcmp(att->implementation_name, "y")) {
					strcpy(att->implementation_name, "cursorManager_y");
				}
			} else if (!strcmp(att->impl_type, "SVG_ContentType")) {
				strcpy(att->implementation_name, "content_type");
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
}

void generateSVGCode_V3(GF_List *svg_elements)
{
	FILE *output;
	u32 i;
	GF_List *all_atts = gf_list_new();

	output = BeginFile(0);
	fprintf(output, "#include <gpac/scenegraph_svg3.h>\n\n\n");

	/* ELEMENT tags */
	fprintf(output, "/* Definition of SVG 3 Alternate element internal tags */\n");
	fprintf(output, "/* TAG names are made of \"TAG_SVG3\" + SVG element name (with - replaced by _) */\n");
	fprintf(output, "enum {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		if (i == 0) {
			fprintf(output, "\tTAG_SVG3_%s = GF_NODE_RANGE_FIRST_SVG3", elt->implementation_name);
		} else {
			fprintf(output, ",\n\tTAG_SVG3_%s", elt->implementation_name);
		}
	}
	fprintf(output, ",\n\t/*undefined elements (when parsing) use this tag*/\n\tTAG_SVG3_UndefinedElement\n};\n\n");



	buildGlobalAttributeList(svg_elements, all_atts);

	/* ATTRIBUTE tags */
	fprintf(output, "/* Definition of SVG 3 attribute internal tags */\n");
	fprintf(output, "/* TAG names are made of \"TAG_SVG3_ATT_\" + SVG attribute name (with - replaced by _) */\n");
	fprintf(output, "enum {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\tTAG_SVG3_ATT_%s,\n", att->implementation_name);
	}
	fprintf(output, "\t/*undefined attributes (when parsing) use this tag*/\n\tTAG_SVG3_ATT_Unknown\n};\n\n");

	/* Structure pointing to all attributes tags */
	fprintf(output, "typedef struct _all_atts {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t%s *%s;\n", att->impl_type, att->implementation_name);
	}
	fprintf(output, "} SVG3AllAttributes;\n");

	EndFile(output, 0);

	/***************************************************/	
	/***************************************************/	
	/*************** Creating .c file ******************/	
	/***************************************************/	
	/***************************************************/	
	output = BeginFile(1);	
	fprintf(output, "#ifndef GPAC_DISABLE_SVG\n\n");
	fprintf(output, "#include <gpac/internal/scenegraph_dev.h>\n\n");
	fprintf(output, "#include <gpac/nodes_svg3.h>\n\n");

	
	fprintf(output, "u32 gf_svg3_get_attribute_tag(u32 element_tag, const char *attribute_name)\n{\n\tif (!attribute_name) return TAG_SVG3_ATT_Unknown;\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		if (!strcmp(att->impl_type, "SMIL_Fill")) continue; 
		if (!strcmp(att->impl_type, "SVG_ContentType")) continue; 
		if (!strcmp(att->svg_name, "x") || !strcmp(att->svg_name, "y")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_%s;\n", att->implementation_name);
			fprintf(output, "\t\telse if (element_tag == TAG_SVG3_cursorManager) return TAG_SVG3_ATT_cursorManager_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG3_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "rotate")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG3_text) return TAG_SVG3_ATT_text_%s;\n", att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG3_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "type")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG3_handler || element_tag == TAG_SVG3_audio || element_tag == TAG_SVG3_video || element_tag == TAG_SVG3_image || element_tag == TAG_SVG3_script) return TAG_SVG3_ATT_content_type;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG3_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else if (!strcmp(att->svg_name, "fill")) {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) {\n", att->svg_name);
			fprintf(output, "\t\tif (element_tag == TAG_SVG3_animate || element_tag == TAG_SVG3_animateColor || element_tag == TAG_SVG3_animateMotion || element_tag == TAG_SVG3_animateTransform || element_tag == TAG_SVG3_animation || element_tag == TAG_SVG3_audio || element_tag == TAG_SVG3_video || element_tag == TAG_SVG3_set) return TAG_SVG3_ATT_smil_fill;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t\telse return TAG_SVG3_ATT_%s;\n", att->svg_name, att->implementation_name);
			fprintf(output, "\t}\n");
		} else {
			fprintf(output, "\tif (!stricmp(attribute_name, \"%s\")) return TAG_SVG3_ATT_%s;\n", att->svg_name, att->implementation_name);
		}
	}
	fprintf(output, "\treturn TAG_SVG3_ATT_Unknown;\n}\n\n");

	fprintf(output, "u32 gf_svg3_get_attribute_type_from_tag(u32 tag)\n{\n");
	fprintf(output, "\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t\tcase TAG_SVG3_ATT_%s: return %s_datatype;\n", att->implementation_name, att->impl_type);
	}
	fprintf(output, "\t\tdefault: return SVG_Unknown_datatype;\n");
	fprintf(output, "\t}\n");
	fprintf(output, "\treturn TAG_SVG3_ATT_Unknown;\n}\n\n");

	fprintf(output, "void gf_svg3_fill_all_attributes(SVG3AllAttributes *all_atts, SVG3Element *e)\n");
	fprintf(output, "{\n");
	fprintf(output, "\tu32 i, count;\n");
	fprintf(output, "\tcount = gf_list_count(e->attributes);\n");
	fprintf(output, "\tfor (i=0; i<count; i++) {\n");
	fprintf(output, "\t\tSVG3Attribute *att = gf_list_get(e->attributes, i);\n");
	fprintf(output, "\t\tswitch(att->tag) {\n");
	for (i=0; i<gf_list_count(all_atts); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(all_atts, i);
		fprintf(output, "\t\tcase TAG_SVG3_ATT_%s: all_atts->%s = (%s *)att->data; break;\n", att->implementation_name, att->implementation_name, att->impl_type);
	}
	fprintf(output, "\t\t}\n");
	fprintf(output, "\t}\n");
	fprintf(output, "}\n");
	fprintf(output, "\n");

	gf_list_del(all_atts);

	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		generateNodeImpl3(output, elt);
	}

	/****************************************************************/	
	/* u32 gf_node_svg3_type_by_class_name(const char *element_name) */
	/****************************************************************/	
	fprintf(output, "u32 gf_node_svg3_type_by_class_name(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG3_%s;\n", elt->svg_name, elt->implementation_name);
	}
	fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");

	/***************************************************/	
	/* const char *gf_svg3_get_element_name(u32 tag) */
	/***************************************************/	
	fprintf(output, "const char *gf_svg3_get_element_name(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG3_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
	}
	fprintf(output, "\tdefault: return \"TAG_SVG3_UndefinedNode\";\n\t}\n}\n\n");

	fprintf(output, "SVG3Attribute *gf_svg3_create_attribute(GF_Node *node, u32 tag)\n{\n\tswitch(node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG3_%s: return gf_svg3_%s_create_attribute(tag);\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\tdefault: return NULL;\n\t}\n}\n\n");

	fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
	EndFile(output, 1); 

}

