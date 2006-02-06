/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Cyril Concolato - Jean le Feuvre - Jean-Claude Moissinac
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Loader module
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
#include <gpac/options.h>
#include <gpac/internal/terminal_dev.h>

#ifndef GPAC_DISABLE_SVG


Bool xmllib_is_init = 0;

static void svg_init_root_element(SVGParser *parser, SVGsvgElement *root_svg)
{
	if (root_svg->width.type == SVG_NUMBER_VALUE) {
		parser->svg_w = FIX2INT(root_svg->width.value);
		parser->svg_h = FIX2INT(root_svg->height.value);
	} else {
		/* if unit for width & height is in percentage, then the only meaningful value for 
		width and height is 100 %. In this case, we don't specify a value. It will be assigned by the renderer */
		parser->svg_w = parser->svg_h = 0;
	}
	gf_sg_set_scene_size_info(parser->graph, parser->svg_w, parser->svg_h, 1);
	gf_sg_set_root_node(parser->graph, (GF_Node *)root_svg);
}

defered_element *list_dichotomic_search(GF_List *ptr, const char *search_id, s32 *index)
{
	s32 tst_value;
	s32 first = 0;
	s32 last = gf_list_count(ptr)-1;
	s32 current = -1;
	defered_element *de = NULL;
	Bool	found = 0;

	/* dichotomic search in the sorted list of defered elements */
	while (first<=last)
	{
		current = (first+last)/2;
		de = (defered_element *)gf_list_get(ptr, current);
		tst_value = strcmp(search_id, &(de->target_id[1]));
		if (tst_value==0) {
			found = 1;
			break;
		}
		else if (tst_value>0)
			first = current +1;
		else
			last = current -1;
	}
	if (index)
		*index=current; /* index where the item could be inserted if necessary */
	return (found?de:NULL);
}

void svg_entity_decl(void *user_data,
				const xmlChar *name,
				int type,
				const xmlChar *publicId,
				const xmlChar *systemId,
				xmlChar *content)
{
	SVGParser	*parser = (SVGParser *)user_data;
	xmlEntityPtr ent = (xmlEntityPtr)malloc(sizeof(xmlEntity));
	if (ent)
	{
		ent->name = strdup(name);
		ent->type = type;
		ent->ExternalID = strdup(publicId);
		ent->SystemID = strdup(systemId);
		ent->content = strdup(content);
		gf_list_add(parser->entities, ent);	
	}
}

void svg_start_document(void *user_data)
{
	SVGParser	*parser = (SVGParser *)user_data;
	parser->sax_state = STARTSVG;
	parser->unknown_depth = 0;
	parser->entities  = gf_list_new();
	if (!parser->entities)
	{
		parser->sax_state = ERROR;
		// TODO implement a safe cleaning if errorif (!elt) {
		return;
	}
}

void svg_end_document(void *user_data)
{
	SVGParser	*parser = (SVGParser *)user_data;
	parser->sax_state = FINISHSVG;
}

xmlEntityPtr svg_get_entity(void * user_data, const xmlChar *name)
{
	SVGParser	*parser = (SVGParser *)user_data;
	u32 i, count;
	count = gf_list_count(parser->entities);
	for (i=0; i<count; i++) {
		xmlEntityPtr ent = gf_list_get(parser->entities, i);
		if (!strcmp(ent->name, name))
		{
			return ent;// and replace entity by ent->content
		}
	}
	return xmlGetPredefinedEntity(name);
}

Bool is_svg_text_element(SVGElement *elt)
{
	u32 tag = elt->sgprivate->tag;
	return ((tag==TAG_SVG_text)||(tag==TAG_SVG_textArea)||(tag==TAG_SVG_tspan));
}

Bool is_svg_anchor_tag(u32 tag)
{
	return (tag == TAG_SVG_a);
}

Bool is_svg_animation_tag(u32 tag)
{
	return (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard)?1:0;
}

void svg_characters(void *user_data, const xmlChar *ch, s32 len)
{
	SVGParser	*parser = (SVGParser *)user_data;
	SVGElement *elt = (SVGElement *)gf_list_get(parser->svg_node_stack,gf_list_count(parser->svg_node_stack)-1);
	s32		baselen = 0;

	if (is_svg_text_element(elt))
	{
		SVGtextElement *text = (SVGtextElement *)elt;
		char *tmp = (char *)ch;
		/* suppress begining spaces */
		if ((!text->core->space)||(text->core->space != XML_SPACE_PRESERVE)) {
			while ((*tmp == ' ' || *tmp=='\n')&& (len>0)) { tmp++; len--; }
		}
		if (text->textContent)
		{
			baselen = strlen(text->textContent);
			text->textContent = (char *)realloc(text->textContent,baselen+len+1);
		}
		else
			text->textContent = (char *)malloc(len+1);
		strncpy(&text->textContent[baselen], tmp, len); // TODO verify how to manage text coding type
		text->textContent[baselen+len]=0;

		/* suppress ending spaces */
		if ((!text->core->space)||(text->core->space != XML_SPACE_PRESERVE)) {
			tmp = &(text->textContent[baselen+len - 1]);
			while (*tmp == ' ' || *tmp=='\n') tmp--;
			tmp++;
			*tmp = 0;
		}

		gf_node_changed((GF_Node *)elt, NULL);
	}
}
// TODO verifiy good practices to replace entities
xmlChar *svg_expand_entities(SVGParser	*parser, xmlChar *originalStyle)
{
	xmlChar *style = strdup(originalStyle);
	xmlChar *newStyle;
	u32 len = strlen(style);
	Bool	expanded=0;
	xmlEntityPtr ent;
	u32		newlen;
	u32		i, j;

	do 
	{
		char *str = style;
		len = strlen(style);
		expanded=0;
		for (i = 0; i < len+1; i++) {
			if (str[i] == '&') 
			{
				for (j=0; j<len+1; j++)
				{
					if (str[j] == ';' || str[j] == 0) 
					{
						char *value_string;
						u32 entity_len = 0;
						entity_len = j - i -1;
						if (entity_len)
						{
							GF_SAFEALLOC(value_string, entity_len+1);
							memcpy(value_string, str+i+1, entity_len);
							value_string[entity_len] = 0;
							ent = svg_get_entity(parser, value_string);
							if (ent)
							{
								newlen = strlen(ent->content);
								GF_SAFEALLOC(newStyle, len+newlen-entity_len);
								memcpy(newStyle, style, i);
								memcpy(newStyle+i, ent->content, newlen);
								memcpy(newStyle+i+newlen, style+i+entity_len+1, len-i-entity_len-1);
								newStyle[len+newlen-entity_len-2] = 0;
								free(style);
								style = newStyle;
								expanded = 1;
							}
						}
					}
				}
			}
		}
	}
	while (expanded==1);
	return style;
}

void svg_start_element(void *user_data, const xmlChar *name, const xmlChar **attrs)
{
	SVGParser	*parser = (SVGParser *)user_data;
	SVGElement *elt;

	switch(parser->sax_state) {
	case STARTSVG:
		if (!stricmp((char *)name, "svg")) {
			elt = svg_parse_sax_element(parser, name, attrs, NULL);
			if (!elt) {
				parser->last_error = GF_SG_UNKNOWN_NODE;
				parser->sax_state = ERROR;
				return;
			} else {
				svg_init_root_element(parser, (SVGsvgElement *)elt);
			}
			parser->svg_node_stack = gf_list_new();
			if (!parser->svg_node_stack) {
				parser->sax_state = ERROR;
				return;
			}
			gf_list_add(parser->svg_node_stack, elt);
			parser->sax_state = SVGCONTENT;
			parser->loader_status = 2;
		} else {
			parser->prev_sax_state = STARTSVG;
			parser->unknown_depth++;
			parser->sax_state = UNKNOWN;
		}
		break;
	case SVGCONTENT:
		elt = svg_parse_sax_element(parser, name, attrs, 
							  (SVGElement *)gf_list_get(parser->svg_node_stack,gf_list_count(parser->svg_node_stack)-1));
		if (elt) 
			gf_list_add(parser->svg_node_stack, elt);
		else {
			parser->prev_sax_state = SVGCONTENT;
			parser->unknown_depth++;
			parser->sax_state = UNKNOWN;
		}
		break;
	case UNKNOWN:
		parser->unknown_depth++;
		break;
	}
}

void svg_end_element(void *user_data, const xmlChar *name)
{
	SVGParser	*parser = (SVGParser *)user_data;
	switch(parser->sax_state) {
	case STARTSVG:
		break;
	case SVGCONTENT:
		gf_list_rem_last(parser->svg_node_stack);
		break;
	case UNKNOWN:
		parser->unknown_depth--;
		if (parser->unknown_depth==0) parser->sax_state = parser->prev_sax_state;
		break;
	}
	/*JLF: there's a pb with endDocument, I force it here*/
	if (!stricmp(name, "svg")) parser->sax_state = FINISHSVG;
}
/* end of SAX related functions */

/* Generic Scene Graph handling functions for ID */
void svg_parse_element_id(SVGParser *parser, SVGElement *elt, char *nodename)
{
	u32 id = 0;
	SVGElement *unided_elt;

	unided_elt = (SVGElement *)gf_sg_find_node_by_name(parser->graph, nodename);
	if (unided_elt) {
		id = gf_node_get_id((GF_Node *)unided_elt);
		if (svg_has_been_IDed(parser, nodename)) unided_elt = NULL;
	} else {
		id = svg_get_node_id(parser, nodename);
	}
	gf_node_set_id((GF_Node *)elt, id, nodename);
	if (unided_elt) gf_node_replace((GF_Node *)unided_elt, (GF_Node *)elt, 0);

	gf_list_add(parser->ided_nodes, elt);
}
				
Bool svg_has_been_IDed(SVGParser *parser, xmlChar *node_name)
{
	u32 i, count;
	count = gf_list_count(parser->ided_nodes);
	for (i=0; i<count; i++) {
		GF_Node *n = gf_list_get(parser->ided_nodes, i);
		const char *nname = gf_node_get_name(n);
		if (!strcmp(nname, node_name)) return 1;
	}
	return 0;
}

u32 svg_get_next_node_id(SVGParser *parser)
{
	u32 ID;
	ID = gf_sg_get_next_available_node_id(parser->graph);
	if (ID>parser->max_node_id) parser->max_node_id = ID;
	return ID;
}

u32 svg_get_node_id(SVGParser *parser, xmlChar *nodename)
{
	GF_Node *n;
	u32 ID;
	if (sscanf(nodename, "N%d", &ID) == 1) {
		ID ++;
		n = gf_sg_find_node(parser->graph, ID);
		if (n && 0) {
			u32 nID = svg_get_next_node_id(parser);
			const char *nname = gf_node_get_name(n);
			gf_node_set_id(n, nID, nname);
		}
		if (parser->max_node_id<ID) parser->max_node_id=ID;
	} else {
		ID = svg_get_next_node_id(parser);
	}
	return ID;
}
/* end of Generic Scene Graph handling functions for ID */


/* DOM Related functions */
/* Parses all the attributes of an element except id */		  
void svg_parse_dom_attributes(SVGParser *parser, 
									xmlNodePtr node,
									SVGElement *elt,
									u8 anim_value_type,
									u8 anim_transform_type)
{
	xmlAttrPtr attributes;
	char *style;
	u32 tag;

	tag = gf_node_get_tag((GF_Node*)elt);

	/* Parsing the style attribute */
	if ((style = xmlGetProp(node, "style"))) svg_parse_style(elt, style);

	/* Parsing all the other attributes, with a special case for the id attribute */
	attributes = node->properties;
	while (attributes) {
		if (attributes->type == XML_ATTRIBUTE_NODE) {
			if (!stricmp(attributes->name, "id")) {
				/* should have been done in svg_parse_element_id */
			} else if (!stricmp(attributes->name, "attributeName")) {
				/* we don't parse the animation element attributes here */
			} else if (!stricmp(attributes->name, "type")) {
				if (tag == TAG_SVG_animateTransform) {
				/* we don't parse the animation element attributes here */
				} else {
					GF_FieldInfo info;
					if (!gf_node_get_field_by_name((GF_Node *)elt, "type", &info)) {
						svg_parse_attribute(elt, &info, attributes->children->content, 0, 0);
					}
				}
			} else if (!stricmp(attributes->name, "href")) {
				if (tag == TAG_SVG_set ||
						tag == TAG_SVG_animate ||
						tag == TAG_SVG_animateColor ||
						tag == TAG_SVG_animateTransform ||
						tag == TAG_SVG_animateMotion || 
						tag == TAG_SVG_discard) {
					/* we don't parse the animation element attributes here */
				} else {
					GF_FieldInfo info;
					if (!gf_node_get_field_by_name((GF_Node *)elt, "xlink:href", &info)) {
						if (!svg_store_embedded_data(info.far_ptr, attributes->children->content, parser->temp_dir, parser->file_name)) {
							svg_parse_attribute(elt, &info, attributes->children->content, 0, 0);
						}
					}
				}
			} else if (strcmp(attributes->name, "style")) {
				GF_FieldInfo info;
				u32 evtType = svg_dom_event_by_name((char *) attributes->name + 2);
				if (evtType != SVG_DOM_EVT_UNKNOWN) {
					XMLEV_Event evt;
					SVGhandlerElement *handler;
					evt.parameter = 0;
					evt.type = evtType;
					handler = gf_sg_dom_create_listener((GF_Node *) elt, evt);
					handler->textContent = strdup(attributes->children->content);
					gf_node_init((GF_Node *)handler);
				} else if (!gf_node_get_field_by_name((GF_Node *)elt, (char *)attributes->name, &info)) {
					svg_parse_attribute(elt, &info, attributes->children->content, anim_value_type, anim_transform_type);
				} else {
					fprintf(stdout, "SVG Warning: Unknown attribute %s on element %s\n", attributes->name, gf_node_get_class_name((GF_Node *)elt));
				}
			}
		} 
		attributes = attributes->next;
	}
}

void svg_parse_dom_children(SVGParser *parser, xmlNodePtr node, SVGElement *elt)
{
	xmlNodePtr children;
	u32 tag;

	tag = gf_node_get_tag((GF_Node*)elt);

	/* Parsing the children of the current element, with a special case for text nodes.*/
	children = node->xmlChildrenNode;
	while(children) {
		SVGElement *child;
		if (children->type == XML_ELEMENT_NODE) {
			child = svg_parse_dom_element(parser, children, elt);
			if (child) gf_list_add(elt->children, child);
		} else if ((children->type == XML_TEXT_NODE) && (tag == TAG_SVG_text)) {
			SVGtextElement *text = (SVGtextElement *)elt;
			if (text->core->space && text->core->space == XML_SPACE_PRESERVE) {
				text->textContent = strdup(children->content);
			} else {
				char *tmp = children->content;
				u32 len;
				while (*tmp == ' ' || *tmp=='\n') tmp++;
				text->textContent = strdup(tmp);
				len = strlen(tmp);
				tmp = &(text->textContent[len - 1]);
				while (*tmp == ' ' || *tmp=='\n') tmp--;
				tmp++;
				*tmp = 0;
			}
		} else if ((children->type == XML_CDATA_SECTION_NODE) && (tag == TAG_SVG_script)) {
			SVGscriptElement *script = (SVGscriptElement *)elt;
			script->textContent = strdup(children->content);
		}
		children = children->next;
	}
}

void svg_parse_dom_defered_animations(SVGParser *parser, xmlNodePtr node, SVGElement *elt, SVGElement *parent)
{
	GF_FieldInfo xlink_href;
	u8 anim_value_type = 0;
	u8 anim_transform_type = 0;
	u32 tag;

	tag = gf_node_get_tag((GF_Node*)elt);

	/* Resolve the target element*/
	if (!gf_node_get_field_by_name((GF_Node *)elt, "xlink:href", &xlink_href)) {
		char *href;
		if ((href = xmlGetProp(node, "href"))) {
			svg_parse_attribute(elt,&xlink_href,  href, 0, 0);
		} else {
			/* default is the parent element */
			elt->xlink->href.type = SVG_IRI_ELEMENTID;
			elt->xlink->href.target = parent;
			gf_svg_register_iri(parser->graph, &elt->xlink->href);
		}
	}

	/* Resolve the target field */
	if (tag == TAG_SVG_animateMotion) {
		/* use of a special type for animation motion values (x,y) */
		anim_value_type = SVG_Motion_datatype;
	} else {
		char *attributeName;
		if (tag == TAG_SVG_animateTransform) {
			char *type;
			if ((type = xmlGetProp(node, "type"))) {
				GF_FieldInfo type_info;
				if (!gf_node_get_field_by_name((GF_Node *)elt, "type", &type_info)) {
					/* parsing the type attribute of the animateTransform node,
					   so we set the value of anim_transform_type to be able to parse 
					   the animation values appropriately */
					svg_parse_attribute(elt, &type_info, type, 0, 0);
					anim_value_type		= SVG_Matrix_datatype;
					anim_transform_type = *(SVG_TransformType*)type_info.far_ptr;
				} else {
					fprintf(stdout, "Warning: type attribute not found.\n");
				}
			} else {
				fprintf(stdout, "Warning: type attribute not specified in animateTransform.\n");
			} 
		} else if ((attributeName = xmlGetProp(node, "attributeName"))) {
			GF_FieldInfo attributeName_info;
			if (!gf_node_get_field_by_name((GF_Node *)elt, "attributeName", &attributeName_info)) {
				smil_parse_attributename(elt, attributeName);
				anim_value_type = ((SMIL_AttributeName *)attributeName_info.far_ptr)->type;
			} else {
				fprintf(stdout, "Warning: attributeName attribute not found.\n");
			}
		} else {
			if (tag != TAG_SVG_discard)
				fprintf(stdout, "Warning: target attribute not specified.\n");
		}
	}

	svg_parse_dom_attributes(parser, node, elt, anim_value_type, anim_transform_type);

	svg_parse_dom_children(parser, node, elt);
	/* We need to init the node at the end of the parsing, after parsing all attributes */
	if (elt) {
		GF_DOM_Event evt;
		gf_node_init((GF_Node *)elt);
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = SVG_DOM_EVT_LOAD;
		gf_sg_fire_dom_event((GF_Node*)elt, NULL, &evt);
	}
}


/* Parse and Create and SVG element 
	TODO: handle xml:id attribute 
	TODO: find a way to handle mixed content
*/
SVGElement *svg_parse_dom_element(SVGParser *parser, xmlNodePtr node, SVGElement *parent)
{
	u32 tag;
	SVGElement *elt;
	char *id = NULL;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_get_tag_by_name(node->name);
	if (tag == TAG_UndefinedNode) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = gf_svg_new_node(parser->graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (GF_Node *)parent);

	if ( (id = xmlGetProp(node, "id")) ) svg_parse_element_id(parser, elt, id);

	/* For animation elements, we defer parsing until the all the node are parsed,
	   then we first need to resolve the target element, 
	   then to determine the target field using 'attributeName' attribute,
	   and based on this attribute we can know how to parse the animation values 
	   (anim_value_type, anim_transform_type) */
	if (tag == TAG_SVG_set ||
		tag == TAG_SVG_animate ||
		tag == TAG_SVG_animateColor ||
		tag == TAG_SVG_animateTransform ||
		tag == TAG_SVG_animateMotion || 
		tag == TAG_SVG_discard) {
		defered_element *de = malloc(sizeof(defered_element));
		de->node = node;
		de->animation_elt = elt;
		de->parent = parent;
		gf_list_add(parser->defered_animation_elements, de);
		return elt;
	}

	svg_parse_dom_attributes(parser, node, elt, 0, 0);

	svg_parse_dom_children(parser, node, elt);
	/* We need to init the node at the end of the parsing, after parsing all attributes */
	if (elt) {
		GF_DOM_Event evt;
		gf_node_init((GF_Node *)elt);
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = SVG_DOM_EVT_LOAD;
		gf_sg_fire_dom_event((GF_Node *)elt, NULL, &evt);
	}
	return elt;
}
/* DOM end */

/* SAX functions */
void svg_parse_sax_defered_anchor(SVGParser *parser, SVGElement *anchor_elt, defered_element local_de)
{
	GF_FieldInfo xlink_href_info;
	gf_node_get_field_by_name((GF_Node *)anchor_elt, "xlink:href", &xlink_href_info);
	if (local_de.target_id) 
		svg_parse_attribute(anchor_elt, &xlink_href_info, local_de.target_id, 0, 0);
	else {
		/* default is the parent element */
		local_de.animation_elt->xlink->href.type = SVG_IRI_ELEMENTID;
		local_de.animation_elt->xlink->href.target = local_de.parent;
		gf_svg_register_iri(parser->graph, &local_de.animation_elt->xlink->href);
	}
}

void svg_parse_sax_defered_animation(SVGParser *parser, SVGElement *animation_elt, defered_element local_de)
{
	GF_FieldInfo info;
	u8 anim_value_type = 0, anim_transform_type = 0;

	GF_FieldInfo xlink_href_info;
	gf_node_get_field_by_name((GF_Node *)animation_elt, "xlink:href", &xlink_href_info);
	if (local_de.target_id) {
		svg_parse_attribute(animation_elt, &xlink_href_info, local_de.target_id, 0, 0);
		free(local_de.target_id);
	} else {
		/* default is the parent element */
		local_de.animation_elt->xlink->href.type = SVG_IRI_ELEMENTID;
		local_de.animation_elt->xlink->href.target = local_de.parent;
		gf_svg_register_iri(parser->graph, &local_de.animation_elt->xlink->href);
	}

	if (local_de.attributeName) {
		/* get the type of the target attribute to determine type of the from/to/by ... */
		smil_parse_attributename(animation_elt, local_de.attributeName);
		gf_node_get_field_by_name((GF_Node *)animation_elt, "attributeName", &info);
		anim_value_type = ((SMIL_AttributeName *)info.far_ptr)->type;
	} else {
		switch (gf_node_get_tag((GF_Node *)animation_elt) ) {
		case TAG_SVG_animateMotion:
			anim_value_type = SVG_Motion_datatype;
			break;
		case TAG_SVG_discard:
			break;
		default:
			fprintf(stdout, "Error: no attributeName specified.\n");
			break;
		}
	}

	if (gf_node_get_tag((GF_Node *)animation_elt) == TAG_SVG_animateTransform && local_de.type) {
		/* determine the transform_type in case of animateTransform attribute */
		GF_FieldInfo type_info;
		gf_node_get_field_by_name((GF_Node *)animation_elt, "type", &type_info);
		svg_parse_attribute(animation_elt, &type_info, local_de.type, 0, 0);
		anim_value_type = SVG_Matrix_datatype;
		anim_transform_type = *(SVG_TransformType*)type_info.far_ptr;
	} 

	/* Parsing of to / from / by / values */
	if (local_de.to) {
		gf_node_get_field_by_name((GF_Node *)animation_elt, "to", &info);
		svg_parse_attribute(animation_elt, &info, local_de.to, anim_value_type, anim_transform_type);
		free(local_de.to);
	} 
	if (local_de.from) {
		gf_node_get_field_by_name((GF_Node *)animation_elt, "from", &info);
		svg_parse_attribute(animation_elt, &info, local_de.from, anim_value_type, anim_transform_type);
		free(local_de.from);
	} 
	if (local_de.by) {
		gf_node_get_field_by_name((GF_Node *)animation_elt, "by", &info);
		svg_parse_attribute(animation_elt, &info, local_de.by, anim_value_type, anim_transform_type);
		free(local_de.by);
	} 
	if (local_de.values) {
		gf_node_get_field_by_name((GF_Node *)animation_elt, "values", &info);
		svg_parse_attribute(animation_elt, &info, local_de.values, anim_value_type, anim_transform_type);
		free(local_de.values);
	} 
	/*OK init the anim*/
	gf_node_init((GF_Node *)animation_elt);

	/*free attributeName after init since it is used in the SMIL anim setup*/
	if (local_de.attributeName) free(local_de.attributeName);

}

SVGElement *svg_parse_sax_element(SVGParser *parser, const xmlChar *name, const xmlChar **attrs, SVGElement *parent)
{
	u32			tag;
	SVGElement *elt;
	s32			attribute_index = 0;
	Bool		ided = 0;

	/* animation element to be defered after the target element is resolved */
	defered_element *de = NULL;

	/* variable required for parsing of animation attributes in a certain order 
	   if the target is already resolved */
	defered_element local_de;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_get_tag_by_name(name);
	if (tag == TAG_UndefinedNode) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = gf_svg_new_node(parser->graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (GF_Node *)parent);
	if (parent && elt) gf_list_add(parent->children, elt);

	/* Parsing the style attribute */
	attribute_index=0;
	if (attrs)
	while (attrs[attribute_index]!=NULL)
	{
		if (stricmp(attrs[attribute_index],"style")==0) {
			xmlChar *style= svg_expand_entities(parser, (xmlChar *)attrs[attribute_index+1]);
			if (style)
			{
				svg_parse_style(elt, style);
				free(style);
			}
			break;
		}
		attribute_index+=2;
	}

	/* For animation elements, we try to resolve the target element;
	if we are able to determine the target,
	then we have to determine the target field using 'attributeName' attribute,
	and based on this attribute we can know how to parse the animation values 
	(anim_value_type, anim_transform_type) 
	if are not able to determine the target, we put the element in a list for future processing
	(each time a new id is defined, we try to resolve the defered elements)
	*/
	if (is_svg_animation_tag(tag)) {
		/* Parsing the xlink:href attribute */
		Bool xlink_href_found = 0;
		attribute_index=0;
		memset(&local_de, 0, sizeof(defered_element));
		local_de.parent = parent;
		if (attrs)
		while (attrs[attribute_index]!=NULL)
		{
			if (stricmp(attrs[attribute_index],"xlink:href")==0) {
				GF_Node *node;
				xlink_href_found = 1;
				/* check if the target is known */
				node = gf_sg_find_node_by_name(parser->graph, (char *)&(attrs[attribute_index+1][1]));
				if (node) { 
					/* target found; we can resolve all the attributes */
					local_de.target = (SVGElement *)node;
					local_de.target_id = strdup(attrs[attribute_index+1]);
				} else /* target unresolved */
				{
					s32	position;

					GF_SAFEALLOC(de,sizeof(defered_element))

					// de->node = node; we need to save some attributes for future parsing
					// and not parse these attributes below
					de->target_id = strdup(attrs[attribute_index+1]);
					de->animation_elt = elt;
					de->parent = parent;
					/* this list must be sorted on target_id key */
					list_dichotomic_search(parser->defered_animation_elements, &(de->target_id[1]), &position);
					if (position <= 0) gf_list_add(parser->defered_animation_elements, de);
					else gf_list_insert(parser->defered_animation_elements, de, position);
				}
				break;
			} 
			attribute_index+=2;
		}
		if (!xlink_href_found) {
			local_de.target = parent;			
			local_de.animation_elt = elt;
		}
	}
	/* For anchor elements, we try to resolve the target element;
	if are not able to determine the target, we put the element in a list for future processing
	(each time a new id is defined, we try to resolve the defered elements)
	*/
	if (is_svg_anchor_tag(tag)) {
		/* Parsing the xlink:href attribute */
		Bool xlink_href_found = 0;
		attribute_index=0;
		memset(&local_de, 0, sizeof(defered_element));
		local_de.parent = parent;
		if (attrs)
		while (attrs[attribute_index]!=NULL)
		{
			if (stricmp(attrs[attribute_index],"xlink:href")==0) {
				GF_FieldInfo xlink_href_info;
				gf_node_get_field_by_name((GF_Node *)elt, "xlink:href", &xlink_href_info);
				svg_parse_attribute(elt, &xlink_href_info, (char *)attrs[attribute_index+1], 0, 0);
				break;
			} 
			attribute_index+=2;
		}
		if (!xlink_href_found) {
			local_de.target = parent;			
		}
	}
	
	/* Parsing all the other attributes, with a special case of id */
	attribute_index=0;
	if (attrs)
	while (attrs[attribute_index]) {
		if (!stricmp(attrs[attribute_index], "id")) {
			svg_parse_element_id(parser, elt, (xmlChar *)attrs[attribute_index+1]);
			ided = 1;
		} else if (!stricmp(attrs[attribute_index], "attributeName")) {
			if (de) de->attributeName = strdup(attrs[attribute_index+1]);
			else local_de.attributeName = strdup(attrs[attribute_index+1]);
		} else if (!stricmp(attrs[attribute_index], "to")) {
			if (de) de->to = strdup(attrs[attribute_index+1]);
			else local_de.to = strdup(attrs[attribute_index+1]);
		} else if (!stricmp(attrs[attribute_index], "from")) {
			if (de) de->from = strdup(attrs[attribute_index+1]);
			else local_de.from = strdup(attrs[attribute_index+1]);
		} else if (!stricmp(attrs[attribute_index], "by")) {
			if (de) de->by = strdup(attrs[attribute_index+1]);
			else local_de.by = strdup(attrs[attribute_index+1]);
		} else if (!stricmp(attrs[attribute_index], "values")) {
			if (de) de->values = strdup(attrs[attribute_index+1]);
			else local_de.values = strdup(attrs[attribute_index+1]);
		} else if (!stricmp(attrs[attribute_index], "type")) {
			if (tag == TAG_SVG_animateTransform) {
				if (de) de->type = strdup(attrs[attribute_index+1]);
				else local_de.type = strdup(attrs[attribute_index+1]);
			} else {
				GF_FieldInfo info;
				if (!gf_node_get_field_by_name((GF_Node *)elt, "type", &info)) {
					svg_parse_attribute(elt, &info, (xmlChar *)attrs[attribute_index+1], 0, 0);
				}
			}
		} else if (!stricmp(attrs[attribute_index], "xlink:href")) {
			if (is_svg_animation_tag(tag)) {
				/* already dealt with above */
			} else {
				GF_FieldInfo info;
				if (!gf_node_get_field_by_name((GF_Node *)elt, "xlink:href", &info)) {
					svg_parse_attribute(elt, &info, (xmlChar *)attrs[attribute_index+1], 0, 0);
				}
			}
		} else {
			GF_FieldInfo info;
			u32 evtType = svg_dom_event_by_name((char *) (char *)attrs[attribute_index] + 2);
			if (evtType != SVG_DOM_EVT_UNKNOWN) {
				XMLEV_Event evt;
				SVGhandlerElement *handler;
				evt.parameter = 0;
				evt.type = evtType;
				handler = gf_sg_dom_create_listener((GF_Node *) elt, evt);
				handler->textContent = strdup((char *)attrs[attribute_index+1]);
				gf_node_init((GF_Node *)handler);
			} else if (!gf_node_get_field_by_name((GF_Node *)elt, (char *)attrs[attribute_index], &info)) {
				svg_parse_attribute(elt, &info, (xmlChar *)attrs[attribute_index+1], 0, 0);
			} else {
				fprintf(stdout, "SVG Warning: Unknown attribute %s on element %s\n", (char *)attrs[attribute_index], gf_node_get_class_name((GF_Node *)elt));
			}
		}
		attribute_index+=2;
	}

	if (!de && is_svg_animation_tag(tag)) svg_parse_sax_defered_animation(parser, elt, local_de);

	/* if the new element has an id, we try to resolve defered references */
	if (ided) {
		defered_element *previous_de;
		const char *new_id = gf_node_get_name((GF_Node *)elt);
		/* dichotomic search in the sorted list of defered animation elements */
		previous_de = list_dichotomic_search(parser->defered_animation_elements, new_id, NULL);
		if (previous_de) { /* defered element 'previous_de' can be resolved by the new elt */
			svg_parse_sax_defered_animation(parser, previous_de->animation_elt, *previous_de);
		}
		
		/* defered references for anchor */
		{
			u32 i;
			u32 count = gf_list_count(parser->unresolved_hrefs);
			for (i=0; i<count; i++)
			{
				href_instance *hi = gf_list_get(parser->unresolved_hrefs, i);
				if (hi) {
					SVG_IRI *iri = hi->iri;
					GF_Node *targ = gf_sg_find_node_by_name(parser->graph, &(iri->iri[1]));
					if (iri->target) {
						hi->elt->xlink->href.type = SVG_IRI_ELEMENTID;
						hi->elt->xlink->href.target = (SVGElement *)targ;
						gf_svg_register_iri(parser->graph, &hi->elt->xlink->href);
						gf_node_init((GF_Node *)hi->elt);
						gf_list_rem(parser->unresolved_hrefs, i);
					}
				}
			}
		}
		
	}

	/* We need to init the node at the end of the parsing, after parsing all attributes */
	if (!is_svg_animation_tag(tag) && elt) {
		GF_DOM_Event evt;
		/*init node*/
		gf_node_init((GF_Node *)elt);
		/*fire initialization event*/
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = SVG_DOM_EVT_LOAD;
		gf_sg_fire_dom_event((GF_Node *) elt, NULL, &evt);
	}
	return elt;
}
/* SAX end */

/* Constructors and Desctructos of the SVG Parser */
SVGParser *NewSVGParser()
{
	SVGParser *tmp;
	GF_SAFEALLOC(tmp, sizeof(SVGParser));
	return tmp;
}

void SVGParser_Terminate(SVGParser *parser)
{
    /* there is no more input, indicate the parsing is finished.
	   Is this needed ?
		xmlParseChunk(ctxt, inputbuffer, 0, 1);
     */

    /* destroy the SAX parser context and file. */
    if (parser->sax_handler) free(parser->sax_handler);
	if (parser->sax_ctx) xmlFreeParserCtxt(parser->sax_ctx);
	if (parser->sax_file) fclose(parser->sax_file);

	if (xmllib_is_init) xmlCleanupParser();
	xmllib_is_init = 0;

	gf_list_del(parser->ided_nodes);
	gf_list_del(parser->unresolved_timing_elements);
	gf_list_del(parser->unresolved_hrefs);
	gf_list_del(parser->defered_animation_elements);
	if (parser->entities) gf_list_del(parser->entities);
	if (parser->svg_node_stack) gf_list_del(parser->svg_node_stack);
	if (parser->file_name) free(parser->file_name);
	free(parser);
}

static void SVGParser_InitAllParsers(SVGParser *parser)
{
	/* Scene Graph related code */
	parser->ided_nodes = gf_list_new();

	/* List of elements to be resolved after parsing but before rendering */
	parser->unresolved_timing_elements = gf_list_new();
	
	/* xlink:href attributes */
	parser->unresolved_hrefs = gf_list_new();

	/* defered animation elements */
	parser->defered_animation_elements = gf_list_new();
}

/* Full DOM Parsing and Progressive Parsing functions */
GF_Err SVGParser_ParseFullDoc(SVGParser *parser)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	SVGElement *n;
	//u32 d;
	/* XML Related code */
	if (!xmllib_is_init) {
		xmlInitParser();
		LIBXML_TEST_VERSION
		xmllib_is_init=1;
	}

	doc = xmlParseFile(parser->file_name);
	if (doc == NULL) return GF_BAD_PARAM;
	root = xmlDocGetRootElement(doc);

	SVGParser_InitAllParsers(parser);

	n = svg_parse_dom_element(parser, root, NULL);
	if (n) svg_init_root_element(parser, (SVGsvgElement *)n);

	/* Resolve time elements */
	while (gf_list_count(parser->unresolved_timing_elements) > 0) {
		SMIL_Time *v = gf_list_get(parser->unresolved_timing_elements, 0);
		gf_list_rem(parser->unresolved_timing_elements, 0);
		v->element = gf_sg_find_node_by_name(parser->graph, v->element_id);
		if (v->element) {
			free(v->element_id);
			v->element_id = NULL;
		}
	}

	/* Resolve hrefs */
	while (gf_list_count(parser->unresolved_hrefs) > 0) {
		href_instance *hi = gf_list_get(parser->unresolved_hrefs, 0);
		SVG_IRI *iri = hi->iri;
		GF_Node *targ =  gf_sg_find_node_by_name(parser->graph, &(iri->iri[1]));
		gf_list_rem(parser->unresolved_hrefs, 0);
		if (targ) {
			hi->elt->xlink->href.type = SVG_IRI_ELEMENTID;
			hi->elt->xlink->href.target = (SVGElement *)targ;
			gf_svg_register_iri(parser->graph, &hi->elt->xlink->href);
			if (iri->iri) free(iri->iri);
			iri->iri = NULL;
		}
	}

	while (gf_list_count(parser->defered_animation_elements)) {
		defered_element *de = gf_list_get(parser->defered_animation_elements, 0);
		gf_list_rem(parser->defered_animation_elements, 0);
		svg_parse_dom_defered_animations(parser, de->node, de->animation_elt, de->parent);
		free(de);
	}

	//scanf("%d", &d);
	xmlFreeDoc(doc);
	//scanf("%d", &d);
	return GF_OK;
}

static void SVGParser_InitSaxHandler(SVGParser *parser)
{
	GF_SAFEALLOC(parser->sax_handler, sizeof(xmlSAXHandler))
	parser->sax_handler->startDocument	= svg_start_document;
	parser->sax_handler->endDocument	= svg_end_document;
	parser->sax_handler->characters		= svg_characters;
	parser->sax_handler->endElement		= svg_end_element;
	parser->sax_handler->startElement	= svg_start_element;
	parser->sax_handler->getEntity		= svg_get_entity;
	parser->sax_handler->entityDecl 	= svg_entity_decl;
}

GF_Err SVGParser_InitProgressiveFileChunk(SVGParser *parser)
{
    char inputbuffer[SAX_MAX_CHARS];
    s32	len;

	/* XML Related code */
	if (!xmllib_is_init) {
		xmlInitParser();
		LIBXML_TEST_VERSION
		xmllib_is_init=1;
	}

    parser->sax_file = fopen(parser->file_name, "rb");
    if (parser->sax_file == NULL) return GF_IO_ERR;

	parser->sax_state	= UNDEF;

	SVGParser_InitAllParsers(parser);
	SVGParser_InitSaxHandler(parser);

    /* Read a few first byte to check the input used for the
     * encoding detection at the parser level. */
    len = fread(inputbuffer, 1, 4, parser->sax_file);
    if (len != 4)  return GF_OK; 
	parser->nb_bytes_read = len;

	/* Create a progressive parsing context, the 2 first arguments
     * are not used since we want to build a tree and not use a SAX
     * parsing interface. We also pass the first bytes of the document
     * to allow encoding detection when creating the parser but this
     * is optional. */
    parser->sax_ctx = xmlCreatePushParserCtxt(parser->sax_handler, parser, inputbuffer, len, NULL);
    
	/* TODO setup a better error value and verify cleanup: fclose... */
	if (parser->sax_ctx == NULL)  return GF_IO_ERR; 

	return GF_OK;
}

GF_Err SVGParser_ParseProgressiveFileChunk(SVGParser *parser)
{
	u32 read_bytes, entry_time, diff;
    char inputbuffer[SAX_MAX_CHARS];

	if (!parser->sax_ctx) return GF_OK;

	entry_time = gf_sys_clock();

	fseek(parser->sax_file, parser->nb_bytes_read, SEEK_SET);
	while (1) {
		read_bytes = fread(inputbuffer, 1, SAX_MAX_CHARS, parser->sax_file);

		if (read_bytes > 0) {
			xmlParseChunk(parser->sax_ctx, inputbuffer, read_bytes, 0);
			parser->nb_bytes_read += read_bytes;
		}
		
		if (parser->sax_state == FINISHSVG) return GF_EOS;
		if (parser->sax_state == ERROR) return GF_IO_ERR;

		if (parser->load_type == SVG_LOAD_SAX_PROGRESSIVE) {
			diff = gf_sys_clock() - entry_time;
			if (diff > parser->sax_max_duration) return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err SVGParser_ParseMemoryFirstChunk(SVGParser *parser, unsigned char *inBuffer, u32 inBufferLength)
{
	/* XML Related code */
	if (!xmllib_is_init) {
		xmlInitParser();
		LIBXML_TEST_VERSION
		xmllib_is_init=1;
	}

	parser->sax_state	= UNDEF;

	SVGParser_InitAllParsers(parser);
	SVGParser_InitSaxHandler(parser);

	/* Create a progressive parsing context, the 2 first arguments
     * are not used since we want to build a tree and not use a SAX
     * parsing interface. We also pass the first bytes of the document
     * to allow encoding detection when creating the parser but this
     * is optional. */
    parser->sax_ctx = xmlCreatePushParserCtxt(parser->sax_handler, parser, inBuffer, 4, NULL);
    
	/* TODO setup a better error value and verify cleanup: fclose... */
	if (parser->sax_ctx == NULL)  return GF_IO_ERR; 

	if (inBufferLength > 4) 
		return SVGParser_ParseMemoryNextChunk(parser, inBuffer+4, inBufferLength-4);

	return GF_OK;
}

GF_Err SVGParser_ParseMemoryNextChunk(SVGParser *parser, unsigned char *inBuffer, u32 inBufferLength)
{
	if (!parser->sax_ctx) return GF_OK;
	
	xmlParseChunk(parser->sax_ctx, inBuffer, inBufferLength, 0);
	
	if (parser->sax_state == FINISHSVG) return GF_EOS;
	if (parser->sax_state == ERROR) return GF_IO_ERR;
	return GF_OK;
}

/* The rest of the file is required for DANAE but not used in GPAC */
struct danae_parser {
	u32 type; // 0 = SVG, 1 = LSR
	void *parser;
};
void *DANAE_NewSVGParser(char *filename, void *scene_graph)
{
	struct danae_parser *dp;
	char *ext;
	if (!filename || !scene_graph) return NULL;
	if ((ext = strrchr(filename, '.')) == NULL) return NULL;

	dp = malloc(sizeof(struct danae_parser));
	if (!strcmp(ext, ".svg")) {
		dp->type = 0;
		dp->parser = NewSVGParser();
		((SVGParser *)dp->parser)->oti = SVGLOADER_OTI_SVG;
		((SVGParser *)dp->parser)->file_name = strdup(filename);
		((SVGParser *)dp->parser)->graph = scene_graph;
	} else if (!strcmp(ext, ".xsr")) {
		dp->type = 0;
		dp->parser = NewSVGParser();
		((SVGParser *)dp->parser)->oti = SVGLOADER_OTI_LASERML;
		((SVGParser *)dp->parser)->file_name = strdup(filename);
		((SVGParser *)dp->parser)->graph = scene_graph;
	} else if (!strcmp(ext, ".lsr")) {
		dp->type = 1;
	}

	return dp;
}

void DANAE_SVGParser_Parse(void *p)
{
	struct danae_parser *dp = (struct danae_parser *)p;
	if (!dp->type) {
		if (((SVGParser *)dp->parser)->oti == SVGLOADER_OTI_SVG) {
			SVGParser_ParseFullDoc(dp->parser);
		} else if (((SVGParser *)dp->parser)->oti == SVGLOADER_OTI_LASERML) {
			SVGParser_ParseLASeR(dp->parser);
		}
	} 
}

void DANAE_SVGParser_Terminate(void *p)
{

}

#endif
