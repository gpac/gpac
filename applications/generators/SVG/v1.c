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

void generateNode(FILE *output, SVGGenElement* svg_elt) 
{
	fprintf(output, "typedef struct _tagSVG_SA_%sElement\n{\n", svg_elt->implementation_name);

	if (svg_elt->has_transform) {
		fprintf(output, "\tTRANSFORMABLE_SVG_ELEMENT\n");
	} else {
		fprintf(output, "\tBASE_SVG_ELEMENT\n");
	}

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tSVGCommandBuffer updates;\n");
	} 

	generateAttributes(output, svg_elt->attributes, 0);

	/*special case for handler node*/
	if (!strcmp(svg_elt->implementation_name, "handler")) {
		fprintf(output, "\tvoid (*handle_event)(GF_Node *hdl, GF_DOM_Event *event);\n");
	}
	fprintf(output, "} SVG_SA_%sElement;\n\n\n", svg_elt->implementation_name);
}


void generateAttributeInfo(FILE *output, char * elt_imp_name, SVGGenAttribute *att, u32 i)
{
	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"%s\";\n", att->svg_name);
	fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", att->impl_type);
	fprintf(output, "\t\t\tinfo->far_ptr = & ((SVG_SA_%sElement *)node)->%s;\n", elt_imp_name, att->implementation_name);
	fprintf(output, "\t\t\treturn GF_OK;\n");
}

u32 generateCoreInfo(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"id\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_ID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = gf_node_get_name_address(node);\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:id\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_ID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = gf_node_get_name_address(node);\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"class\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_String_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SA_Element *)node)->core->_class;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:lang\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_LanguageID_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SA_Element *)node)->core->lang;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:base\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_String_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SA_Element *)node)->core->base;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"xml:space\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = XML_Space_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SA_Element *)node)->core->space;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"externalResourcesRequired\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Boolean_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVG_SA_Element *)node)->core->eRR;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	return i;
}

void generateAttributeInfoFlat(FILE *output, char *pointer, char *name, char *type, u32 i)
{
	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"%s\";\n", name);
	fprintf(output, "\t\t\tinfo->fieldType = %s_datatype;\n", type);
	fprintf(output, "\t\t\tinfo->far_ptr = &%s;\n", pointer);
	fprintf(output, "\t\t\treturn GF_OK;\n");
}

u32 generateTransformInfo(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"transform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Transform_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->transform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateMotionTransformInfo(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"motionTransform\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Motion_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = ((SVGTransformableElement *)node)->motionTransform;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateXYInfo(FILE *output, SVGGenElement *elt, u32 start)
{
	u32 i = start;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"x\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->xy.x;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;

	fprintf(output, "\t\tcase %d:\n", i);
	fprintf(output, "\t\t\tinfo->name = \"y\";\n");
	fprintf(output, "\t\t\tinfo->fieldType = SVG_Coordinate_datatype;\n");
	fprintf(output, "\t\t\tinfo->far_ptr = &((SVGTransformableElement *)node)->xy.y;\n");
	fprintf(output, "\t\t\treturn GF_OK;\n");
	i++;
	return i;
}

u32 generateGenericInfo(FILE *output, SVGGenElement *elt, u32 index, char *pointer_root, u32 start)
{
	u32 i = start;
	int k;
	for (k=0; k < generic_attributes[index].array_length; k++) {
		char *att_name = generic_attributes[index].array[k];
		SVGGenAttribute *a = findAttribute(elt, att_name);
		if (a) {
			char pointer[500];
			if (strstr(att_name, "xlink:")) {
				sprintf(pointer, "%s%s", pointer_root, att_name+6);
			} else if (strstr(att_name, "xml:")) {
				sprintf(pointer, "%s%s", pointer_root, att_name+4);
			} else {
				char imp_name[50];
				svgNameToImplementationName(att_name, imp_name);
				sprintf(pointer, "%s%s", pointer_root, imp_name);
			}
			generateAttributeInfoFlat(output, pointer, a->svg_name, a->impl_type, i);
			i++;
		}
	}
	return i;
}

u32 generateIndexInfo(FILE *output, SVGGenElement *elt, u32 index, u32 start)
{
	u32 i = start;
	int k;
	for (k=0; k < generic_attributes[index].array_length; k++) {
		char *att_name = generic_attributes[index].array[k];
		SVGGenAttribute *a = findAttribute(elt, att_name);
		if (a) {
			fprintf(output, "\tif(!strcmp(\"%s\", name)) return %d;\n", att_name, i); 
			i++;
		}
	}
	return i;
}

void generateNodeImpl(FILE *output, SVGGenElement* svg_elt) 
{
	u32 i;	

	/***************************************************/	
	/*				Constructor						   */
	/***************************************************/	
	fprintf(output, "void *gf_svg_new_%s()\n{\n\tSVG_SA_%sElement *p;\n", svg_elt->implementation_name,svg_elt->implementation_name);
	fprintf(output, "\tGF_SAFEALLOC(p, SVG_SA_%sElement);\n\tif (!p) return NULL;\n\tgf_node_setup((GF_Node *)p, TAG_SVG_%s);\n\tgf_sg_parent_setup((GF_Node *) p);\n",svg_elt->implementation_name,svg_elt->implementation_name);

	fprintf(output, "\tgf_svg_sa_init_core((SVG_SA_Element *)p);\n");		
	if (svg_elt->has_properties || 
		svg_elt->has_media_properties || 
		svg_elt->has_opacity_properties) {
		fprintf(output, "\tgf_svg_sa_init_properties((SVG_SA_Element *)p);\n");		
	} 
	if (svg_elt->has_focus) {
		fprintf(output, "\tgf_svg_sa_init_focus((SVG_SA_Element *)p);\n");		
	} 
	if (svg_elt->has_xlink) {
		fprintf(output, "\tgf_svg_sa_init_xlink((SVG_SA_Element *)p);\n");		
	} 
	if (svg_elt->has_timing) {
		fprintf(output, "\tgf_svg_sa_init_timing((SVG_SA_Element *)p);\n");		
	} 
	if (svg_elt->has_sync) {
		fprintf(output, "\tgf_svg_sa_init_sync((SVG_SA_Element *)p);\n");		
	}
	if (svg_elt->has_animation){
		fprintf(output, "\tgf_svg_sa_init_anim((SVG_SA_Element *)p);\n");		
	} 
	if (svg_elt->has_conditional) {
		fprintf(output, "\tgf_svg_sa_init_conditional((SVG_SA_Element *)p);\n");		
	} 

	if (svg_elt->has_transform) {
		fprintf(output, "\tgf_mx2d_init(p->transform.mat);\n");
	} 

	if (!strcmp(svg_elt->implementation_name, "conditional")) {
		fprintf(output, "\tgf_svg_sa_init_lsr_conditional(&p->updates);\n");
		fprintf(output, "\tgf_svg_sa_init_timing((SVG_SA_Element *)p);\n");		

	} 

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
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
	else if (!strcmp(svg_elt->svg_name, "solidColor")) {
		fprintf(output, "\tp->properties->solid_opacity.value = FIX_ONE;\n");
	}
	else if (!strcmp(svg_elt->svg_name, "stop")) {
		fprintf(output, "\tp->properties->stop_opacity.value = FIX_ONE;\n");
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

	/***************************************************/	
	/*                     Destructor                  */
	/***************************************************/	
	fprintf(output, "static void gf_svg_sa_%s_del(GF_Node *node)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tSVG_SA_%sElement *p = (SVG_SA_%sElement *)node;\n", svg_elt->implementation_name, svg_elt->implementation_name);

	fprintf(output, "\tgf_svg_sa_reset_base_element((SVG_SA_Element *)p);\n");

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
	
	/***************************************************/	
	/*				Attribute Access				   */
	/***************************************************/	
	fprintf(output, "static GF_Err gf_svg_sa_%s_get_attribute(GF_Node *node, GF_FieldInfo *info)\n{\n", svg_elt->implementation_name);
	fprintf(output, "\tswitch (info->fieldIndex) {\n");
	svg_elt->nb_atts = 0;
	svg_elt->nb_atts = generateCoreInfo(output, svg_elt, svg_elt->nb_atts);

	if (svg_elt->has_media_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 2, "((SVG_SA_Element *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 1, "((SVG_SA_Element *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_opacity_properties) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 3, "((SVG_SA_Element *)node)->properties->", svg_elt->nb_atts);
	if (svg_elt->has_focus) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 4, "((SVG_SA_Element *)node)->focus->", svg_elt->nb_atts);
	if (svg_elt->has_xlink) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 5, "((SVG_SA_Element *)node)->xlink->", svg_elt->nb_atts);
	if (svg_elt->has_timing) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 6, "((SVG_SA_Element *)node)->timing->", svg_elt->nb_atts);
	if (svg_elt->has_sync) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 7, "((SVG_SA_Element *)node)->sync->", svg_elt->nb_atts);
	if (svg_elt->has_animation) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 8, "((SVG_SA_Element *)node)->anim->", svg_elt->nb_atts);
	if (svg_elt->has_conditional) 
		svg_elt->nb_atts = generateGenericInfo(output, svg_elt, 9, "((SVG_SA_Element *)node)->conditional->", svg_elt->nb_atts);
	if (svg_elt->has_transform) {
		svg_elt->nb_atts = generateTransformInfo(output, svg_elt, svg_elt->nb_atts);
		svg_elt->nb_atts = generateMotionTransformInfo(output, svg_elt, svg_elt->nb_atts);
	}
	if (svg_elt->has_xy) 
		svg_elt->nb_atts = generateXYInfo(output, svg_elt, svg_elt->nb_atts);

	for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
		SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
		generateAttributeInfo(output, svg_elt->implementation_name, att, svg_elt->nb_atts++);
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

	/***************************************************/	
	/*	gf_svg_sa_%s_get_attribute_index_from_name		   */
	/***************************************************/	
	fprintf(output, "s32 gf_svg_sa_%s_get_attribute_index_from_name(char *name)\n{\n", svg_elt->implementation_name);
	{
		u32 att_index = 0;
		fprintf(output, "\tif(!strcmp(\"id\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"xml:id\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"class\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"xml:lang\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"xml:base\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"xml:space\", name)) return %d;\n", att_index); 
		att_index++;
		fprintf(output, "\tif(!strcmp(\"externalResourcesRequired\", name)) return %d;\n", att_index); 
		att_index++;
		if (svg_elt->has_media_properties) 
			att_index = generateIndexInfo(output, svg_elt, 2, att_index);
		if (svg_elt->has_properties) 
			att_index = generateIndexInfo(output, svg_elt, 1, att_index);
		if (svg_elt->has_opacity_properties) 
			att_index = generateIndexInfo(output, svg_elt, 3, att_index);
		if (svg_elt->has_focus) 
			att_index = generateIndexInfo(output, svg_elt, 4, att_index);
		if (svg_elt->has_xlink) 
			att_index = generateIndexInfo(output, svg_elt, 5, att_index);
		if (svg_elt->has_timing) 
			att_index = generateIndexInfo(output, svg_elt, 6, att_index);
		if (svg_elt->has_sync) 
			att_index = generateIndexInfo(output, svg_elt, 7, att_index);
		if (svg_elt->has_animation) 
			att_index = generateIndexInfo(output, svg_elt, 8, att_index);
		if (svg_elt->has_conditional) 
			att_index = generateIndexInfo(output, svg_elt, 9, att_index);
		if (svg_elt->has_transform) {
			fprintf(output, "\tif(!strcmp(\"transform\", name)) return %d;\n", att_index); 
			att_index++;
			/*motionTransform*/
			fprintf(output, "\tif(!strcmp(\"motionTransform\", name)) return %d;\n", att_index); 
			att_index++;
		}
		if (svg_elt->has_xy) {
			fprintf(output, "\tif(!strcmp(\"x\", name)) return %d;\n", att_index); 
			att_index++;
			fprintf(output, "\tif(!strcmp(\"y\", name)) return %d;\n", att_index); 
			att_index++;
		}

		for (i = 0; i < gf_list_count(svg_elt->attributes); i++) {
			SVGGenAttribute *att = gf_list_get(svg_elt->attributes, i);
			fprintf(output, "\tif(!strcmp(\"%s\", name)) return %d;\n", att->svg_name, att_index); 
			att_index++;
		}
	}
	fprintf(output, "\treturn -1;\n}\n\n");
}

void generateSVGCode_V1(GF_List *svg_elements) 
{
	FILE *output;
	u32 i;

	/***************************************************/	
	/***************************************************/	
	/*************** Creating .h file ******************/	
	/***************************************************/	
	/***************************************************/	
	output = BeginFile(0);
	fprintf(output, "#include <gpac/scenegraph_svg.h>\n\n\n");
	fprintf(output, "/* Definition of SVG element internal tags */\n");
	fprintf(output, "/* TAG names are made of \"TAG_SVG\" + SVG element name (with - replaced by _) */\n");

	/* write all tags */
	fprintf(output, "enum {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		if (i == 0) {
			fprintf(output, "\tTAG_SVG_%s = GF_NODE_RANGE_FIRST_SVG_SA", elt->implementation_name);
		} else {
			fprintf(output, ",\n\tTAG_SVG_%s", elt->implementation_name);
		}
	}
	
	fprintf(output, ",\n\t/*undefined elements (when parsing) use this tag*/\n\tTAG_SVG_UndefinedElement\n};\n\n");
	
	fprintf(output, "/******************************************\n");
 	fprintf(output, "*   SVG Elements structure definitions    *\n");
 	fprintf(output, "*******************************************/\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		generateNode(output, elt);
	}
	fprintf(output, "/******************************************\n");
 	fprintf(output, "*  End SVG Elements structure definitions *\n");
 	fprintf(output, "*******************************************/\n");
	EndFile(output, 0);

	/***************************************************/	
	/***************************************************/	
	/*************** Creating .c file ******************/	
	/***************************************************/	
	/***************************************************/	
	output = BeginFile(1);
	fprintf(output, "#include <gpac/nodes_svg_sa.h>\n\n");
	
	fprintf(output, "#ifndef GPAC_DISABLE_SVG\n\n");
	fprintf(output, "#include <gpac/internal/scenegraph_dev.h>\n\n");
	fprintf(output, "#ifdef GPAC_ENABLE_SVG_SA\n\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		generateNodeImpl(output, elt);
	}

	/***************************************************/	
	/* SVG_SA_Element *gf_svg_sa_create_node(u32 ElementTag)  */
	/***************************************************/	
	fprintf(output, "SVG_SA_Element *gf_svg_sa_create_node(u32 ElementTag)\n");
	fprintf(output, "{\n");
	fprintf(output, "\tswitch (ElementTag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_%s: return (SVG_SA_Element*) gf_svg_new_%s();\n",elt->implementation_name,elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return NULL;\n\t}\n}\n\n");
	
	/***************************************************/	
	/* void gf_svg_sa_element_del(SVG_SA_Element *elt)        */
	/***************************************************/	
	fprintf(output, "void gf_svg_sa_element_del(SVG_SA_Element *elt)\n{\n");
	fprintf(output, "\tGF_Node *node = (GF_Node *)elt;\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_%s: gf_svg_sa_%s_del(node); return;\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return;\n\t}\n}\n\n");

	/***************************************************/	
	/* u32 gf_svg_sa_get_attribute_count(SVG_SA_Element *elt) */
	/***************************************************/	
	fprintf(output, "u32 gf_svg_sa_get_attribute_count(GF_Node *node)\n{\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_%s: return %i;\n", elt->implementation_name, elt->nb_atts);
	}
	fprintf(output, "\t\tdefault: return 0;\n\t}\n}\n\n");
	
	/***********************************************************************/	
	/* GF_Err gf_svg_sa_get_attribute_info(GF_Node *node, GF_FieldInfo *info) */
	/***********************************************************************/	
	fprintf(output, "GF_Err gf_svg_sa_get_attribute_info(GF_Node *node, GF_FieldInfo *info)\n{\n");
	fprintf(output, "\tswitch (node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\t\tcase TAG_SVG_%s: return gf_svg_sa_%s_get_attribute(node, info);\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\t\tdefault: return GF_BAD_PARAM;\n\t}\n}\n\n");

	/****************************************************************/	
	/* u32 gf_svg_sa_node_type_by_class_name(const char *element_name) */
	/****************************************************************/	
	fprintf(output, "u32 gf_svg_sa_node_type_by_class_name(const char *element_name)\n{\n\tif (!element_name) return TAG_UndefinedNode;\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tif (!stricmp(element_name, \"%s\")) return TAG_SVG_%s;\n", elt->svg_name, elt->implementation_name);
	}
	fprintf(output, "\treturn TAG_UndefinedNode;\n}\n\n");


	/***************************************************/	
	/* const char *gf_svg_sa_get_element_name(u32 tag) */
	/***************************************************/	
	fprintf(output, "const char *gf_svg_sa_get_element_name(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s: return \"%s\";\n", elt->implementation_name, elt->svg_name);
	}
	fprintf(output, "\tdefault: return \"UndefinedNode\";\n\t}\n}\n\n");

	/***************************************************/	
	/* const char *gf_svg_sa_get_attribute_index_by_name(u32 tag) */
	/***************************************************/	
	fprintf(output, "s32 gf_svg_sa_get_attribute_index_by_name(GF_Node *node, char *name)\n{\n\tswitch(node->sgprivate->tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s: return gf_svg_sa_%s_get_attribute_index_from_name(name);\n", elt->implementation_name, elt->implementation_name);
	}
	fprintf(output, "\tdefault: return -1;\n\t}\n}\n\n");

	/***************************************************/	
	/* Bool gf_svg_is_element_transformable(u32 tag) */
	/***************************************************/	
	fprintf(output, "Bool gf_svg_is_element_transformable(u32 tag)\n{\n\tswitch(tag) {\n");
	for (i=0; i<gf_list_count(svg_elements); i++) {
		SVGGenElement *elt = (SVGGenElement *)gf_list_get(svg_elements, i);
		fprintf(output, "\tcase TAG_SVG_%s:", elt->implementation_name);
		if (elt->has_transform) fprintf(output, "return 1;\n");
		else fprintf(output, "return 0;\n");
	}
	fprintf(output, "\tdefault: return 0;\n\t}\n}\n");

	fprintf(output, "#endif /*GPAC_ENABLE_SVG_SA*/\n");
	fprintf(output, "#endif /*GPAC_DISABLE_SVG*/\n\n");
	EndFile(output, 1); 

	generate_laser_tables(svg_elements);
}

