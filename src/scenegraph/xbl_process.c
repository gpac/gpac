/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2008-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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
#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/utf.h>
#include <gpac/xml.h>
#include <gpac/scenegraph_svg.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/nodes_xbl.h>

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	XBL_Element *root;

	/* stack of XBL nodes*/
	GF_List *node_stack;
} GF_XBL_Parser;


typedef struct
{
	/*top of parsed sub-tree*/
	XBL_Element *node;
	/*depth of unknown elements being skipped*/
	u32 unknown_depth;
	/*last child added, used to speed-up parsing*/
	GF_ChildNodeItem *last_child;
} XBL_NodeStack;

static GF_Err xbl_parse_report(GF_XBL_Parser *parser, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 2048, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[XBL Parsing] line %d - %s\n", gf_xml_sax_get_line(parser->sax_parser), szMsg));
	}
#endif
	if (e) parser->last_error = e;
	return e;
}

static void xbl_parse_progress(void *cbk, u64 done, u64 total)
{
	gf_set_progress("XBL Parsing", done, total);
}

XBL_Element *gf_xbl_create_node(u32 ElementTag)
{
	XBL_Element *p;
	GF_SAFEALLOC(p, SVG_Element);
	gf_node_setup((GF_Node *)p, ElementTag);
	gf_sg_parent_setup((GF_Node *) p);
	return p;
}

static XBL_Element *xbl_parse_element(GF_XBL_Parser *parser, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes, XBL_NodeStack *parent)
{
	u32	tag, attribute_tag, i;
	XBL_Element *elt = NULL;

//	if (name_space && strlen(name_space)) return NULL;

	tag = gf_sg_node_get_tag_by_class_name(name, 0);
	if (tag != TAG_UndefinedNode) {
		elt = (XBL_Element *)gf_node_new(parser->load->scene_graph, tag);
	} else {
		elt = (XBL_Element *)gf_node_new(parser->load->scene_graph, TAG_DOMFullNode);
	}
	gf_node_register((GF_Node *)elt, (parent ? (GF_Node *)parent->node : NULL));
	if (parent && elt) gf_node_list_add_child_last( & parent->node->children, (GF_Node*)elt, & parent->last_child);

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		attribute_tag = gf_xml_get_attribute_tag((GF_Node *)elt, att->name, 0);
		if (attribute_tag!=TAG_DOM_ATT_any) {
			/*FIXME do we need to check if the attribute is specified several times*/
			GF_DOMAttribute *dom_att = gf_xml_create_attribute((GF_Node*)elt, attribute_tag);
			dom_att->data = gf_strdup(att->value);
		} else {
			xbl_parse_report(parser, GF_OK, "Skipping attribute %s on node %s", att->name, name);
		}
	}

	return elt;
}

static void xbl_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_XBL_Parser *parser = (GF_XBL_Parser *)sax_cbck;
	XBL_NodeStack *stack, *parent;
	XBL_Element *elt;

	parent = (XBL_NodeStack *)gf_list_last(parser->node_stack);

	elt = xbl_parse_element(parser, name, name_space, attributes, nb_attributes, parent);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		xbl_parse_report(parser, GF_OK, "Ignoring unknown element %s", name);
		return;
	}
	if (!parser->root) parser->root = elt;

	GF_SAFEALLOC(stack, XBL_NodeStack);
	if (!stack) {
		return;
	}
	stack->node = elt;
	gf_list_add(parser->node_stack, stack);

}

static void xbl_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	/* FIXME : this function does nothing
	GF_XBL_Parser *parser = (GF_XBL_Parser *)sax_cbck;
	XBL_NodeStack *top = (XBL_NodeStack *)gf_list_last(parser->node_stack);

	*/
}

static void xbl_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	GF_XBL_Parser *parser = (GF_XBL_Parser *)sax_cbck;
	XBL_NodeStack *top = (XBL_NodeStack *)gf_list_last(parser->node_stack);

	if (!top) return;
	if (/*!name_space && */gf_sg_node_get_tag_by_class_name(name, 0) != TAG_UndefinedNode) {
		const char *the_name;
		XBL_Element *node = top->node;
		/*check node name...*/
		the_name = gf_node_get_class_name((GF_Node *) node);
		if (strcmp(the_name, name)) {
			if (top->unknown_depth) {
				top->unknown_depth--;
				return;
			} else {
				xbl_parse_report(parser, GF_BAD_PARAM, "depth mismatch");
				return;
			}
		}
		gf_free(top);
		gf_list_rem_last(parser->node_stack);
	} else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
		} else {
			xbl_parse_report(parser, GF_BAD_PARAM, "depth mismatch");
		}
	}

}

static GF_XBL_Parser *xbl_new_parser(GF_SceneLoader *load)
{
	GF_XBL_Parser *parser;
	if (load->type==GF_SM_LOAD_XBL) {
		if (!load->ctx) return NULL;
	} else if (load->type!=GF_SM_LOAD_XBL) return NULL;

	GF_SAFEALLOC(parser, GF_XBL_Parser);
	if (!parser) return NULL;
	parser->node_stack = gf_list_new();
	parser->sax_parser = gf_xml_sax_new(xbl_node_start, xbl_node_end, xbl_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;

	return parser;
}


GF_Err gf_sm_load_init_xbl(GF_SceneLoader *load)
{
	GF_Err e;
	GF_XBL_Parser *parser;

	if (!load->fileName) return GF_BAD_PARAM;
	parser = xbl_new_parser(load);
	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ( "[Parser] XBL Parsing\n") );
	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, xbl_parse_progress);
	if (e<0) return xbl_parse_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );
	return parser->last_error;
}

GF_Err gf_sm_load_run_xbl(GF_SceneLoader *load)
{
	return GF_OK;
}

void gf_sm_load_done_xbl(GF_SceneLoader *load)
{
	GF_XBL_Parser *parser = (GF_XBL_Parser *)load->loader_priv;
	if (!parser) return;
	while (gf_list_count(parser->node_stack)) {
		XBL_NodeStack *st = (XBL_NodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		gf_free(st);
	}
	gf_list_del(parser->node_stack);
	if (parser->sax_parser) gf_xml_sax_del(parser->sax_parser);
	gf_free(parser);
	load->loader_priv = NULL;
	return;
}

void apply(GF_Node *bound_doc, GF_Node *binding_doc)
{

}

#endif
