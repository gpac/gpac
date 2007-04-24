/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / LASeR+SVG XML Loader module
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


#include "svg_parser.h"

#ifndef GPAC_DISABLE_SVG

extern Bool xmllib_is_init;


Bool SVG_CheckDownload(SVGParser *parser)
{
	u32 size;
	FILE *f = fopen(parser->file_name, "rt");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fclose(f);
	if (size==parser->file_size) return 1;
	return 0;
}

static xmlNodePtr lsr_toElement(xmlNodePtr ptr)
{
	while (ptr) {
		if (ptr->type == XML_ELEMENT_NODE) return ptr;
		ptr = ptr->next;
	}
	return NULL;
}

GF_Err lsr_parse_command(SVGParser *parser, xmlNodePtr com)
{
	xmlAttrPtr attributes;
	GF_FieldInfo info;
	if (!com) return GF_OK;

	if (!strcmp(com->name, "NewScene")) {
		SVG_SA_Element *n = svg_parse_dom_element(parser, lsr_toElement(com->children), NULL);
		if (n) {
			SVG_SA_svgElement *root_svg = (SVG_SA_svgElement *)n;
			if (root_svg->width.type == SVG_NUMBER_VALUE) {
				parser->svg_w = FIX2INT(root_svg->width.value);
				parser->svg_h = FIX2INT(root_svg->height.value);
			}
			gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
			gf_sg_set_root_node(parser->graph, (GF_Node *)n);
			parser->needs_attachement = 1;
		} else {
			gf_sg_reset(parser->graph);
		}
		return GF_OK;
	}
	if (!strcmp(com->name, "Insert")) {
		s32 pos = -1;
		char *at_att = "children";
		SVG_SA_Element *at_node = NULL;
		attributes = com->properties;
		while (attributes) {
			if (attributes->type == XML_ATTRIBUTE_NODE) {
				if (!stricmp(attributes->name, "index")) pos = atoi(attributes->children->content);
				else if (!stricmp(attributes->name, "attributeName")) at_att = attributes->children->content;
				else if (!stricmp(attributes->name, "ref")) at_node = (SVG_SA_Element *) gf_sg_find_node_by_name(parser->graph, attributes->children->content);
				else fprintf(stdout, "WARNING: LASeR.Insert %s attribute not handled\n", attributes->name);
			}
			attributes = attributes->next;
		}
		if (!at_node) return GF_SG_UNKNOWN_NODE;

		if (!strcmp(at_att, "children")) {
			SVG_SA_Element *n = svg_parse_dom_element(parser, lsr_toElement(com->children), at_node);
			if (n) {
				if (pos == -1) gf_node_list_add_child( &at_node->children, (GF_Node*)n);
				else gf_node_list_insert_child( &at_node->children, (GF_Node*)n, (u32) pos);
			}
			gf_node_dirty_set((GF_Node *) at_node, GF_SG_CHILD_DIRTY, 0);
		} else {
			gf_node_get_field_by_name((GF_Node *)at_node, at_att, &info);
			fprintf(stdout, "WARNING: point insert not supported\n");
			gf_node_dirty_set((GF_Node *) at_node, GF_SG_SVG_GEOMETRY_DIRTY, 0);
		}

		return GF_OK;
	}

	if (!strcmp(com->name, "Replace")) {
		s32 pos = -1;
		char *at_att = NULL;
		char *value = NULL;
		SVG_SA_Element *at_node = NULL;
		attributes = com->properties;
		while (attributes) {
			if (attributes->type == XML_ATTRIBUTE_NODE) {
				if (!stricmp(attributes->name, "index")) pos = atoi(attributes->children->content);
				else if (!stricmp(attributes->name, "attributeName")) at_att = attributes->children->content;
				else if (!stricmp(attributes->name, "value")) value = attributes->children->content;
				else if (!stricmp(attributes->name, "ref")) at_node = (SVG_SA_Element *) gf_sg_find_node_by_name(parser->graph, attributes->children->content);
				else fprintf(stdout, "WARNING: LASeR.Replace %s attribute not handled\n", attributes->name);
			}
			attributes = attributes->next;
		}
		if (!at_node) return GF_SG_UNKNOWN_NODE;

		if (!at_att) {
			GF_Node *old;
			SVG_SA_Element *n = svg_parse_dom_element(parser, lsr_toElement(com->children), at_node);
			if (pos<0) pos = gf_node_list_get_count(at_node->children) - 1;
			old = gf_node_list_get_child(at_node->children, pos);
			if (old) {
				gf_node_list_del_child(&at_node->children, old);
				gf_node_unregister(old, (GF_Node *) at_node);
			}
			if (n) gf_node_list_insert_child(&at_node->children, (GF_Node*)n, pos);
			gf_node_dirty_set((GF_Node *) at_node, GF_SG_CHILD_DIRTY, 0);
		} else {
			if (!gf_node_get_field_by_name((GF_Node *)at_node, at_att, &info)) {
				if (pos>=0) 
					fprintf(stdout, "WARNING: point replace not supported\n");
				else
					gf_svg_parse_attribute((GF_Node *)at_node, &info, value, 0, 0);

				gf_node_dirty_set((GF_Node *) at_node, GF_SG_SVG_GEOMETRY_DIRTY, 0);
			}
		}
		return GF_OK;
	}
	if (!strcmp(com->name, "Delete")) {
		s32 pos = -1;
		char *at_att = "children";
		SVG_SA_Element *at_node = NULL;
		attributes = com->properties;
		while (attributes) {
			if (attributes->type == XML_ATTRIBUTE_NODE) {
				if (!stricmp(attributes->name, "index")) pos = atoi(attributes->children->content);
				else if (!stricmp(attributes->name, "attributeName")) at_att = attributes->children->content;
				else if (!stricmp(attributes->name, "ref")) at_node = (SVG_SA_Element *) gf_sg_find_node_by_name(parser->graph, attributes->children->content);
				else fprintf(stdout, "WARNING: LASeR.Delete %s attribute not handled\n", attributes->name);
			}
			attributes = attributes->next;
		}
		if (!at_node) return GF_SG_UNKNOWN_NODE;

		if (!strcmp(at_att, "children")) {
			GF_Node *old;
			if (pos<0) pos = gf_node_list_get_count(at_node->children) - 1;
			old = gf_node_list_get_child(at_node->children, pos);
			if (old) {
				gf_node_list_del_child(&at_node->children, (GF_Node*)old);
				gf_node_unregister(old, (GF_Node *) at_node);
			}
			gf_node_dirty_set((GF_Node *) at_node, GF_SG_CHILD_DIRTY, 0);
		} else {
			fprintf(stdout, "WARNING: point delete not supported\n");
		}

		return GF_OK;
	}
	fprintf(stdout, "WARNING: command %s not supported\n", com->name);
	return GF_OK;
}


GF_Err SVGParser_ParseLASeR(SVGParser *parser)
{
	GF_Err e = GF_OK;
	u32 scene_time;
	xmlNodePtr com;
	xmlAttrPtr attributes;

	/* XML Related code */
	if (!xmllib_is_init) {
		xmlInitParser();
		LIBXML_TEST_VERSION
		xmllib_is_init=1;
	}

	if (!parser->loader_status) {
		xmlDocPtr doc = NULL;
		xmlNodePtr root = NULL;
		/*need full doc*/
		if (!SVG_CheckDownload(parser)) return GF_OK;

		parser->loader_status = 2;
		doc = xmlParseFile(parser->file_name);
		if (doc == NULL) return GF_BAD_PARAM;
		root = xmlDocGetRootElement(doc);
		if (strcmp(root->name, "SAFSession")) return GF_BAD_PARAM;

		if (!root->children) return GF_BAD_PARAM;
		parser->sU = lsr_toElement(root->children);
		if (!parser->sU || strcmp(parser->sU->name, "sceneHeader")) return GF_BAD_PARAM;
		parser->sU = lsr_toElement(parser->sU->next);

		/*OK*/
		parser->loader_status = 1;

		/* Scene Graph related code */
		parser->ided_nodes = gf_list_new();
	} else if (parser->loader_status == 2) return GF_EOS;

	if (!parser->sU || strcmp(parser->sU->name, "sceneUnit")) return GF_EOS;

	scene_time = 0;
	attributes = parser->sU->properties;
	while (attributes) {
		if ((attributes->type == XML_ATTRIBUTE_NODE) && !stricmp(attributes->name, "time")) {
			scene_time = atoi(attributes->children->content);
		}
		attributes = attributes->next;
	}
	if (scene_time > parser->stream_time) return GF_OK;

	/*parse AU*/
	com = lsr_toElement(parser->sU->children);
	while (com) {
		e = lsr_parse_command(parser, com);
		if (e) return e;
		com = lsr_toElement(com->next);
	}

	/*go to next AU*/
	parser->sU = lsr_toElement(parser->sU->next);
	return e;
}

#endif
