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

void generateAttributes2(FILE *output, GF_List *attributes) 
{
	u32 i;
	for (i = 0; i<gf_list_count(attributes); i++) {
		SVGGenAttribute *att = (SVGGenAttribute *)gf_list_get(attributes, i);
		if (!strcmp(att->implementation_name, "transform")) continue;
		fprintf(output, "\t%s %s;\n", att->impl_type, att->implementation_name);
	}
}

void generateNode2(FILE *output, SVGGenElement* svg_elt) 
{
	fprintf(output, "typedef struct _tagSVG_SANI_%sElement\n{\n", svg_elt->implementation_name);

	if (svg_elt->has_transform) {
		fprintf(output, "\tTRANSFORMABLE_SVG_SANI_ELEMENT\n");
	} else {
		fprintf(output, "\tBASE_SVG_SANI_ELEMENT\n");
	}

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tSVGCommandBuffer updates;\n");
	} 

	generateAttributes2(output, svg_elt->attributes);

	/*special case for handler node*/
	if (!strcmp(svg_elt->implementation_name, "handler")) {
		fprintf(output, "\tvoid (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);\n");
	}
	fprintf(output, "} SVG_SANI_%sElement;\n\n\n", svg_elt->implementation_name);
}

void generateAttributeInfo2(FILE *output, char * elt_imp_name, SVGGenAttribute *att, u32 i)
{
	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"%s\";\n", att->svg_name);
	fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", att->impl_type);
	fprintf(output, "\t\t\tinfo->far_ptr = & ((SVG_SANI_%sElement *)node)->%s;\n", elt_imp_name, att->implementation_name);
	fprintf(output, "\t\t\treturn GF_OK;\n");
}

u32 generateTransformInfo2(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"transform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Transform_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SANI_TransformableElement *)node)->transform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateMotionTransformInfo2(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"motionTransform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Transform_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = ((SVG_SANI_TransformableElement *)node)->motionTransform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateXYInfo2(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"x\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SANI_TransformableElement *)node)->xy.x;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"y\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SANI_TransformableElement *)node)->xy.y;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

void generateNodeImpl2(FILE *output, SVGGenElement* svg_elt) 
{
	u32 i;	

	/* Constructor */
	fprintf(output, "void *gf_svg_sani_new_%s()\n{\n\tSVG_SANI_%sElement *p;\n", svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "\tGF_SAFEALLOC(p, SVG_SANI_%sElement);\n\tif (!p) return NULL;\n\tgf_node_setup((GF_Node *)p, TAG_SVG_SANI_%s);\n\tgf_sg_parent_setup((GF_Node *) p);\n",svg_elt->implementation_name,svg_elt->implementation_name);

	fprintf(output, "\tgf_svg_sani_init_core((SVG_SANI_Element *)p);\n");		
	if (svg_elt->has_focus) {
		fprintf(output, "\tgf_svg_sani_init_focus((SVG_SANI_Element *)p);\n");		
	} 
	if (svg_elt->has_xlink) {
		fprintf(output, "\tgf_svg_sani_init_xlink((SVG_SANI_Element *)p);\n");		
	} 
	if (svg_elt->has_timing) {
		fprintf(output, "\tgf_svg_sani_init_timing((SVG_SANI_Element *)p);\n");		
	} 
	if (svg_elt->has_sync) {
		fprintf(output, "\tgf_svg_sani_init_sync((SVG_SANI_Element *)p);\n");		
	}
	if (svg_elt->has_animation){
		fprintf(output, "\tgf_svg_sani_init_anim((SVG_SANI_Element *)p);\n");		
	} 
	if (svg_elt->has_conditional) {
		fprintf(output, "\tgf_svg_sani_init_conditional((SVG_SANI_Element *)p);\n");		
	} 

	if (svg_elt->has_transform) {
		fprintf(output, "\tgf_mx2d_init(p->transform.mat);\n");
	} 

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tgf_svg_sa_init_lsr_conditional(&p->updates);\n");
		fprintf(output, "\tgf_svg_sani_init_timing((SVG_SANI_Element *)p);\n");		

	} 

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
		
		/* forcing initialization of old-properties */
		if (!strcmp("audio-level", att->svg_name)) {
			fprintf(output, "\tp->audio_level.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->audio_level.value = FIX_ONE;\n");
		} else if (!strcmp("display", att->svg_name)) {
			fprintf(output, "\tp->display = SVG_DISPLAY_INLINE;\n");
		} else if (!strcmp("display-align", att->svg_name)) {
			fprintf(output, "\tp->display_align = SVG_DISPLAYALIGN_AUTO;\n");
		} else if (!strcmp("fill", att->svg_name)) {
			fprintf(output, "\tp->fill.type = SVG_PAINT_COLOR;\n");
			fprintf(output, "\tp->fill.color.type = SVG_COLOR_RGBCOLOR;\n");
		} else if (!strcmp("fill-opacity", att->svg_name)) {
			fprintf(output, "\tp->fill_opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->fill_opacity.value = FIX_ONE;\n");
		} else if (!strcmp("fill-rule", att->svg_name)) {
			fprintf(output, "\tp->fill_rule = SVG_FILLRULE_NONZERO;\n");
		} else if (!strcmp("font-family", att->svg_name)) {
			fprintf(output, "\tp->font_family.type = SVG_FONTFAMILY_VALUE;\n");
			fprintf(output, "\tp->font_family.value = strdup(\"Arial\");\n");
		} else if (!strcmp("font-size", att->svg_name)) {
			fprintf(output, "\tp->font_size.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->font_size.value = 12*FIX_ONE;\n");
		} else if (!strcmp("font-style", att->svg_name)) {
			fprintf(output, "\tp->font_style = SVG_FONTSTYLE_NORMAL;\n");
		} else if (!strcmp("font-variant", att->svg_name)) {
			fprintf(output, "\tp->font_variant = SVG_FONTVARIANT_NORMAL;\n");
		} else if (!strcmp("font-weight", att->svg_name)) {
			fprintf(output, "\tp->font_weight = SVG_FONTWEIGHT_NORMAL;\n");
		} else if (!strcmp("line-increment", att->svg_name)) {
			fprintf(output, "\tp->line_increment.type = SVG_NUMBER_AUTO;\n");
			fprintf(output, "\tp->line_increment.value = FIX_ONE;\n");
		} else if (!strcmp("opacity", att->svg_name)) {
			fprintf(output, "\tp->opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->opacity.value = FIX_ONE;\n");
		} else if (!strcmp("solid-color", att->svg_name)) {
			fprintf(output, "\tp->solid_color.type = SVG_PAINT_COLOR;\n");
			fprintf(output, "\tp->solid_color.color.type = SVG_COLOR_RGBCOLOR;\n");
		} else if (!strcmp("solid-opacity", att->svg_name)) {
			fprintf(output, "\tp->solid_opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->solid_opacity.value = FIX_ONE;\n");
		} else if (!strcmp("solid-color", att->svg_name)) {
			fprintf(output, "\tp->stop_color.type = SVG_PAINT_COLOR;\n");
			fprintf(output, "\tp->stop_color.color.type = SVG_COLOR_RGBCOLOR;\n");
		} else if (!strcmp("stop-opacity", att->svg_name)) {
			fprintf(output, "\tp->stop_opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->stop_opacity.value = FIX_ONE;\n");
		} else if (!strcmp("stroke", att->svg_name)) {
			fprintf(output, "\tp->stroke.type = SVG_PAINT_NONE;\n");
			fprintf(output, "\tp->stroke.color.type = SVG_COLOR_RGBCOLOR;\n");
		} else if (!strcmp("stroke-dasharray", att->svg_name)) {
			fprintf(output, "\tp->stroke_dasharray.type = SVG_STROKEDASHARRAY_NONE;\n");
		} else if (!strcmp("stroke-dashoffset", att->svg_name)) {
			fprintf(output, "\tp->stroke_dashoffset.type = SVG_NUMBER_VALUE;\n");
		} else if (!strcmp("stroke-linecap", att->svg_name)) {
			fprintf(output, "\tp->stroke_linecap = SVG_STROKELINECAP_BUTT;\n");
		} else if (!strcmp("stroke-linejoin", att->svg_name)) {
			fprintf(output, "\tp->stroke_linejoin = SVG_STROKELINEJOIN_MITER;\n");
		} else if (!strcmp("stroke-miterlimit", att->svg_name)) {
			fprintf(output, "\tp->stroke_miterlimit.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->stroke_miterlimit.value = 4*FIX_ONE;\n");
		} else if (!strcmp("stroke-opacity", att->svg_name)) {
			fprintf(output, "\tp->stroke_opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->stroke_opacity.value = FIX_ONE;\n");
		} else if (!strcmp("stroke-width", att->svg_name)) {
			fprintf(output, "\tp->stroke_width.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->stroke_width.value = FIX_ONE;\n");
		} else if (!strcmp("text-align", att->svg_name)) {
			fprintf(output, "\tp->text_align = SVG_TEXTALIGN_START;\n");
		} else if (!strcmp("text-anchor", att->svg_name)) {
			fprintf(output, "\tp->text_anchor = SVG_TEXTANCHOR_START;\n");
		} else if (!strcmp("vector-effect", att->svg_name)) {
			fprintf(output, "\tp->vector_effect = SVG_VECTOREFFECT_NONE;\n");
		} else if (!strcmp("viewport-fill", att->svg_name)) {
			fprintf(output, "\tp->viewport_fill.type = SVG_PAINT_NONE;\n");
		} else if (!strcmp("viewport-fill-opacity", att->svg_name)) {
			fprintf(output, "\tp->viewport_fill_opacity.type = SVG_NUMBER_VALUE;\n");
			fprintf(output, "\tp->viewport_fill_opacity.value = FIX_ONE;\n");
		} else if (!strcmp("visibility", att->svg_name)) {
			fprintf(output, "\tp->visibility = SVG_VISIBILITY_VISIBLE;\n");
		}

		/* Initialization of complex types */
		if ( !strcmp("SVG_Points", att->impl_type) || 
			 !strcmp("SVG_Coordinates", att->impl_type) ||
			 !strcmp("SMIL_KeyPoints", att->impl_type)) {
			fprintf(output, "\tp->%s = gf_list_new();\n", att->implementation_name);
		} else if (!strcmp("SVG_PathData", att->impl_type) && !strcmp(svg_elt->svg_name, "animateMotion")) {
			fprintf(output, "#ifdef USE_GF_PATH\n");
			fprintf(output, "\tgf_path_reset(&p->path);\n");
			fprintf(output, "#else\n");
			fprintf(output, "\tp->path.commands = gf_list_new();\n");
			fprintf(output, "\tp->path.points = gf_list_new();\n");
			fprintf(output, "#endif\n");
		} else if (!strcmp("SVG_PathData", att->impl_type)) {
			fprintf(output, "#ifdef USE_GF_PATH\n");
			fprintf(output, "\tgf_path_reset(&p->d);\n");
			fprintf(output, "#else\n");
			fprintf(output, "\tp->d.commands = gf_list_new();\n");
			fprintf(output, "\tp->d.points = gf_list_new();\n");
			fprintf(output, "#endif\n");
		} else if (!strcmp(att->svg_name, "lsr:enabled")) {
			fprintf(output, "\tp->lsr_enabled = 1;\n");
		} 
	}
	/*some default values*/
	if (!strcmp(svg_elt->svg_name, "svg")) {
		fprintf(output, "\tp->width.type = SVG_NUMBER_PERCENTAGE;\n");
		fprintf(output, "\tp->width.value = INT2FIX(100);\n");
		fprintf(output, "\tp->height.type = SVG_NUMBER_PERCENTAGE;\n");
		fprintf(output, "\tp->height.value = INT2FIX(100);\n");
	}
	else if (!strcmp(svg_elt->svg_name, "linearGradient")) {
		fprintf(output, "\tp->x2.value = FIX_ONE;\n");
		fprintf(output, "\tgf_mx2d_init(p->gradientTransform.mat);\n");
	}
	else if (!strcmp(svg_elt->svg_name, "radialGradient")) {
		fprintf(output, "\tp->cx.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->cy.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->r.value = FIX_ONE/2;\n");
		fprintf(output, "\tgf_mx2d_init(p->gradientTransform.mat);\n");
		fprintf(output, "\tp->fx.value = FIX_ONE/2;\n");
		fprintf(output, "\tp->fy.value = FIX_ONE/2;\n");
	}
	else if (!strcmp(svg_elt->svg_name, "video") || !strcmp(svg_elt->svg_name, "audio") || !strcmp(svg_elt->svg_name, "animation")) {
		fprintf(output, "\tp->timing->dur.type = SMIL_DURATION_MEDIA;\n");
	}
	fprintf(output, "\treturn p;\n}\n\n");

	/* Destructor */
	fprintf(output, "static void gf_svg_sani_%s_del(GF_Node *node)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tSVG_SANI_%sElement *p = (SVG_SANI_%sElement *)node;\n", svg_elt->implementation_name, svg_elt->implementation_name);
	fprintf(output, "\tgf_svg_sani_reset_base_element((SVG_SANI_Element *)p);\n");

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tgf_svg_sa_reset_lsr_conditional(&p->updates);\n");
	} 
	else if (!strcmp(svg_elt->implementation_name, "a")) {
		fprintf(output, "\tif (p->target) free(p->target);\n");
	} 

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
		if (!strcmp("SMIL_KeyPoints", att->impl_type)) {
			fprintf(output, "\tgf_smil_delete_key_types(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_Coordinates", att->impl_type)) {
			fprintf(output, "\tgf_svg_delete_coordinates(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_Points", att->impl_type)) {
			fprintf(output, "\tgf_svg_delete_points(p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_PathData", att->impl_type)) {
			if (!strcmp(svg_elt->svg_name, "animateMotion")) {
				fprintf(output, "\tgf_svg_reset_path(p->path);\n");
			} else {
				fprintf(output, "\tgf_svg_reset_path(p->d);\n");
			}
		} else if (!strcmp("XMLRI", att->impl_type)) {
			fprintf(output, "\tgf_svg_reset_iri(node->sgprivate->scenegraph, &p->%s);\n", att->implementation_name);
		} else if (!strcmp("SVG_FontFamily", att->impl_type)) {
			fprintf(output, "\tif (p->%s.value) free(p->%s.value);\n", att->implementation_name, att->implementation_name);
		} else if (!strcmp("SVG_String", att->impl_type) || !strcmp("SVG_ContentType", att->impl_type)) {
			fprintf(output, "\tfree(p->%s);\n", att->implementation_name);
		}
	}
	if (svg_elt->has_transform) {
		fprintf(output, "\tif (p->motionTransform) free(p->motionTransform);\n");
	} 

	fprintf(output, "\tgf_sg_parent_reset((GF_Node *) p);\n");
	fprintf(output, "\tgf_node_free((GF_Node *)p);\n");
	fprintf(output, "}\n\n");

	/* Attribute Access */
	fprintf(output, "static GF_Err gf_svg_sani_%s_get_attribute(GF_Node *node, GF_FieldInfo *info)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tswitch (info->fieldIndex) {\n");
	svg_elt->nb_atts = 0;
	svg_elt->nb_atts = generateCoreInfo(output, svg_elt, svg_elt->nb_atts);

	if (svg_elt->has_focus) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 4, "((SVG_SANI_Element *)node)->focus->", svg_elt->nb_atts);
	if (svg_elt->has_xlink) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 5, "((SVG_SANI_Element *)node)->xlink->", svg_elt->nb_atts);
	if (svg_elt->has_timing) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 6, "((SVG_SANI_Element *)node)->timing->", svg_elt->nb_atts);
	if (svg_elt->has_sync) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 7, "((SVG_SANI_Element *)node)->sync->", svg_elt->nb_atts);
	if (svg_elt->has_animation) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 8, "((SVG_SANI_Element *)node)->anim->", svg_elt->nb_atts);
	if (svg_elt->has_conditional) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 9, "((SVG_SANI_Element *)node)->conditional->", svg_elt->nb_atts);
	if (svg_elt->has_transform) {
		svg_elt->nb_atts = generateTransformInfo2(output, svg_elt, svg_elt->nb_atts);
		svg_elt->nb_atts = generateMotionTransformInfo2(output, svg_elt, svg_elt->nb_atts);
	}
	if (svg_elt->has_xy) 
		svg_elt->nb_atts = generateXYInfo2(output, svg_elt, svg_elt->nb_atts);

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
		generateAttributeInfo2(output, svg_elt->implementation_name, att, svg_elt->nb_atts++);
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

}

void generateSVGCode_V2(GF_List *svg_elements) 
{
	FILE *output;
	u32 i;

	output = BeginFile(0);
	fprintf(output, "#include <gpac/scenegraph_svg.h>\n\n\n");
	fprintf(output, "/* Definition of SVG 2 Alternate element internal tags */\n");
	fprintf(output, "/* TAG names are made of \"TAG_SVG_SANI_\" + SVG element name (with - replaced by _) */\n");

	/* write all tags */
	fprintf(output, "enum {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		if (i == 0) {
			fprintf(output, "\tTAG_SVG_SANI_%s = GF_NODE_RANGE_FIRST_SVG_SANI", elt->implementation_name);
		} else {
			fprintf(output, ",\n\tTAG_SVG_SANI_%s", elt->implementation_name);
		}
	}
	
	fprintf(output, ",\n\t/*undefined elements (when parsing) use this tag*/\n\tTAG_SVG_SANI_UndefinedElement\n};\n\n");

	fprintf(output, "/******************************************\n");
 	fprintf(output, "*   SVG_SANI_ Elements structure definitions    *\n");
 	fprintf(output, "*******************************************/\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		generateNode2(output, elt);
	}
	EndFile(output, 0);

	output = BeginFile(1);
	fprintf(output, "#include <gpac/nodes_svg_sani.h>\n\n");

	fprintf(output, "#ifndef GPAC_DISABLE_SVG\n\n");
	fprintf(output, "#include <gpac/internal/scenegraph_dev.h>\n\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		generateNodeImpl2(output, elt);
	}

	fprintf(output, "SVG_SANI_Element *gf_svg_sani_create_node(u32 ElementTag)\n");
	fprintf(output, "{\n");
	fprintf(output, "\tswitch (ElementTag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_SANI_%s: return (SVG_SANI_Element*) gf_svg_sani_new_%s();\n",elt->implementation_name,elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return NULL;\n\t}\n}\n\n");
	
	fprintf(output, "void gf_svg_sani_element_del(SVG_SANI_Element *elt)\n{\n");
	fprintf(output, "\tGF_Node *node = (GF_Node *)elt;\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_SANI_%s: gf_svg_sani_%s_del(node); return;\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return;\n\t}\n}\n\n");

	fprintf(output, "u32 gf_svg_sani_get_attribute_count(GF_Node *node)\n{\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_SANI_%s: return %i;\n", elt->implementation_name, elt->nb_atts);
	}
	fprintf(output, "\t\tdefault: return 0;\n\t}\n}\n\n");
	
	fprintf(output, "GF_Err gf_svg_sani_get_attribute_info(GF_Node *node, GF_FieldInfo *info)\n{\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_SANI_%s: return gf_svg_sani_%s_get_attribute(node, info);\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

	fprintf(output, "u32 gf_svg_sani_type_by_class_name(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG_SANI_%s;\n", elt->svg_name, elt->implementation_name);
	}
	fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");

	fprintf(output, "const char *gf_svg_sani_get_element_name(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_SANI_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
	}
	fprintf(output, "\tdefault: return \"UndefinedNode\";\n\t}\n}\n\n");

	fprintf(output, "Bool gf_svg_sani_is_element_transformable(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_SANI_%s:", elt->implementation_name);
		if (elt->has_transform) fprintf(output, "return 1;\n");
		else fprintf(output, "return 0;\n");
	}
	fprintf(output, "\tdefault: return 0;\n\t}\n}\n");

	fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
	EndFile(output, 1); 

	generate_laser_tables(svg_elements);
}
