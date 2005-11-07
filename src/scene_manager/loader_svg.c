/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/base_coding.h>

GF_Err svg_parse_attribute(SVGElement *elt, GF_FieldInfo *info, char *attribute_content, u8 anim_value_type, u8 transform_type);

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	Bool has_root;
	GF_List *nodes;
	GF_List *ided_nodes;
	GF_List *pending_hrefs;
	GF_List *defered_anims;
	GF_List *smil_times;
} GF_SVGParser;


typedef struct {
	u32 resolve_stage;
	/* Animation element being defered */
	SVGElement *animation_elt;
	/* target element*/
	SVGElement *target;
	/* id of the target element when unresolved*/
	char *target_id;

	/* attributes which cannot be parsed until the type of the target attribute is known */
	char *attributeName;
	char *type; /* only for animateTransform */
	char *to;
	char *from;
	char *by;
	char *values;
} DeferedAnimation;

static GF_Err svg_report(GF_SVGParser *parser, GF_Err e, char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (parser->load->OnMessage) {
		char szMsg[2048];
		char szMsgFull[2048];
		vsprintf(szMsg, format, args);
		sprintf(szMsgFull, "(line %d) %s", gf_xml_sax_get_line(parser->sax_parser), szMsg);
		parser->load->OnMessage(parser->load->cbk, szMsgFull, e);
	} else {
		fprintf(stdout, "(line %d) ", gf_xml_sax_get_line(parser->sax_parser));
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
	}
	va_end(args);
	if (e) parser->last_error = e;
	return e;
}
static void svg_progress(GF_SVGParser *parser, u32 done, u32 total)
{
	parser->load->OnProgress(parser->load->cbk, done, total);
}

static void svg_init_root_element(GF_SVGParser *parser, SVGsvgElement *root_svg)
{
	u32 svg_w, svg_h;
	svg_w = svg_h = 0;
	//svg_convert_length_unit_to_user_unit(parser, &(root_svg->width));
	//svg_convert_length_unit_to_user_unit(parser, &(root_svg->height));
	if (root_svg->width.type != SVG_LENGTH_PERCENTAGE) {
		svg_w = FIX2INT(root_svg->width.number);
		svg_h = FIX2INT(root_svg->height.number);
	}
	gf_sg_set_scene_size_info(parser->load->scene_graph, svg_w, svg_h, 1);
	gf_sg_set_root_node(parser->load->scene_graph, (GF_Node *)root_svg);
}

static void svg_delete_defered_anim(DeferedAnimation *anim, GF_SVGParser *parser)
{
	if (parser) gf_list_del_item(parser->defered_anims, anim);

	if (anim->target_id) free(anim->target_id);
	if (anim->attributeName) free(anim->attributeName);
	if (anim->to) free(anim->to);
	if (anim->from) free(anim->from);
	if (anim->by) free(anim->by);
	if (anim->values) free(anim->values);
	if (anim->type) free(anim->type);
	free(anim);
}

static Bool is_svg_text_element(SVGElement *elt)
{
	u32 tag = elt->sgprivate->tag;
	return ((tag==TAG_SVG_text)||(tag==TAG_SVG_textArea)||(tag==TAG_SVG_tspan));
}

static Bool is_svg_anchor_tag(u32 tag)
{
	return (tag == TAG_SVG_a);
}
static Bool is_svg_animation_tag(u32 tag)
{
	return (tag == TAG_SVG_set ||
			tag == TAG_SVG_animate ||
			tag == TAG_SVG_animateColor ||
			tag == TAG_SVG_animateTransform ||
			tag == TAG_SVG_animateMotion || 
			tag == TAG_SVG_discard)?1:0;
}

Bool svg_has_been_IDed(GF_SVGParser *parser, char *node_name)
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

u32 svg_get_node_id(GF_SVGParser *parser, char *nodename)
{
	GF_Node *n;
	u32 ID;
	if (sscanf(nodename, "N%d", &ID) == 1) {
		ID ++;
		n = gf_sg_find_node(parser->load->scene_graph, ID);
		if (n) {
			u32 nID = gf_sg_get_next_available_node_id(parser->load->scene_graph);
			const char *nname = gf_node_get_name(n);
			gf_node_set_id(n, nID, nname);
		}
	} else {
		ID = gf_sg_get_next_available_node_id(parser->load->scene_graph);
	}
	return ID;
}

static void svg_parse_element_id(GF_SVGParser *parser, SVGElement *elt, char *nodename)
{
	u32 id = 0;
	SVGElement *unided_elt;

	unided_elt = (SVGElement *)gf_sg_find_node_by_name(parser->load->scene_graph, nodename);
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

static Bool svg_resolve_smil_times(GF_SVGParser *parser, GF_List *smil_times, const char *node_name)
{
	u32 i, done, count = gf_list_count(smil_times);
	done = 0;
	for (i=0; i<count; i++) {
		SMIL_Time *t = gf_list_get(smil_times, i);
		/*not an event*/
		if (t->type != SMIL_TIME_EVENT) {
			done++;
			continue;
		}
		/*event target is anim target, already knwon*/
		if (!t->element_id) {
			done++;
			continue;
		}
		if (node_name && strcmp(node_name, t->element_id)) continue;
		t->element = gf_sg_find_node_by_name(parser->load->scene_graph, t->element_id);
		if (t->element) {
			free(t->element_id);
			t->element_id = NULL;
			done++;
		}
	}
	return (done==count) ? 1 : 0;
}

static Bool svg_parse_animation(GF_SVGParser *parser, DeferedAnimation *anim, const char *nodeID)
{
	SVG_IRI *iri;
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0, anim_transform_type = 0;

	if (!anim->target) anim->target = (SVGElement *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
	if (!anim->target) return 0;

	if (anim->resolve_stage==0) {

		/*setup IRI ptr*/
		if (gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "xlink:href", &info) != GF_OK) {
			svg_report(parser, GF_CORRUPTED_DATA, "No xlink:href for node %s", gf_node_get_class_name((GF_Node *)anim->animation_elt));
			return 1;
		}

		iri = info.far_ptr;
		iri->iri_owner = anim->animation_elt;
		iri->target = anim->target;
		iri->type = SVG_IRI_ELEMENTID;

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);

		if (anim->attributeName) {
			SMIL_AttributeName *attname_value;
			/* get the type of the target attribute to determine type of the from/to/by ... */
			if (gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "attributeName", &info) != GF_OK) {
				svg_report(parser, GF_CORRUPTED_DATA, "No attributeName for node %s", gf_node_get_class_name((GF_Node *)anim->animation_elt));
				return 1;
			}
			attname_value = (SMIL_AttributeName *)info.far_ptr;

			if (gf_node_get_field_by_name((GF_Node *)anim->target, anim->attributeName, &info) == GF_OK) {
				attname_value->type = info.fieldType;
				attname_value->field_ptr = info.far_ptr;
			} else {
				svg_report(parser, GF_CORRUPTED_DATA, "No field %s for node %s", anim->attributeName, gf_node_get_class_name((GF_Node *)anim->target));
				return 1;
			}
			anim_value_type = attname_value->type;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else {
				fprintf(stdout, "Error: no attributeName specified.\n");
			}
		}

		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			/* determine the transform_type in case of animateTransform attribute */
			SVG_TransformType *ttype; 
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "type", &info);
			ttype = (SVG_TransformType *) info.far_ptr;

			if (!strcmp(anim->type, "scale"))  anim_transform_type = SVG_TRANSFORM_SCALE;
			else if (!strcmp(anim->type, "rotate")) anim_transform_type = SVG_TRANSFORM_ROTATE;
			else if (!strcmp(anim->type, "translate")) anim_transform_type = SVG_TRANSFORM_TRANSLATE;
			else if (!strcmp(anim->type, "skewX")) anim_transform_type = SVG_TRANSFORM_SKEWX;
			else if (!strcmp(anim->type, "skewY")) anim_transform_type = SVG_TRANSFORM_SKEWY;
			else anim_transform_type = SVG_TRANSFORM_MATRIX;
			anim_value_type = SVG_TransformType_datatype;
			*ttype = anim_transform_type;
		} 

		/* Parsing of to / from / by / values */
		if (anim->to) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "to", &info);
			svg_parse_attribute(anim->animation_elt, &info, anim->to, anim_value_type, anim_transform_type);
		} 
		if (anim->from) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "from", &info);
			svg_parse_attribute(anim->animation_elt, &info, anim->from, anim_value_type, anim_transform_type);
		} 
		if (anim->by) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "by", &info);
			svg_parse_attribute(anim->animation_elt, &info, anim->by, anim_value_type, anim_transform_type);
		} 
		if (anim->values) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "values", &info);
			svg_parse_attribute(anim->animation_elt, &info, anim->values, anim_value_type, anim_transform_type);
		}
		anim->resolve_stage = 1;
	}

	if (anim->resolve_stage == 1) {
		gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "begin", &info);
		if (svg_resolve_smil_times(parser, * (GF_List **) info.far_ptr, nodeID)) {
			anim->resolve_stage = 2;
		} else {
			return 0;
		}
	}
	gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "end", &info);
	if (!svg_resolve_smil_times(parser, * (GF_List **) info.far_ptr, nodeID)) return 0;
	/*OK init the anim*/
	gf_node_init((GF_Node *)anim->animation_elt);
	return 1;
}

static void svg_resolved_refs(GF_SVGParser *parser, const char *nodeID)
{
	u32 count, i;

	/*check unresolved HREF*/
	count = gf_list_count(parser->pending_hrefs);
	for (i=0; i<count; i++) {
		SVG_IRI *iri = gf_list_get(parser->pending_hrefs, i);
		if (nodeID && !strcmp(iri->iri + 1, nodeID)) continue;
		iri->target = (SVGElement *)gf_sg_find_node_by_name(parser->load->scene_graph, iri->iri + 1);
		if (iri->target) {
			free(iri->iri);
			iri->iri = NULL;
			gf_list_rem(parser->pending_hrefs, i);
			i--;
			count--;
		}
	}

	/*check unresolved anims*/
	count = gf_list_count(parser->defered_anims);
	for (i=0; i<count; i++) {
		DeferedAnimation *anim = gf_list_get(parser->defered_anims, i);
		if (nodeID && anim->target_id && strcmp(anim->target_id + 1, nodeID)) continue;
		/*resolve it*/
		if (svg_parse_animation(parser, anim, nodeID)) {
			gf_node_init((GF_Node *)anim->animation_elt);
			svg_delete_defered_anim(anim, parser);
			i--;
			count--;
		}
	}
}

void svg_parse_style(SVGElement *elt, char *style);

static SVGElement *svg_parse_element(GF_SVGParser *parser, const char *name, const char *name_space, GF_List *attrs, SVGElement *parent)
{
	u32	tag, i, count;
	SVGElement *elt;
	Bool ided = 0;
	DeferedAnimation *anim = NULL;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = SVG_GetTagByName(name);
	if (tag == TAG_UndefinedNode) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = SVG_NewNode(parser->load->scene_graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (GF_Node *)parent);
	if (parent && elt) gf_list_add(parent->children, elt);

	if (is_svg_animation_tag(tag)) {
		GF_SAFEALLOC(anim, sizeof(DeferedAnimation));
		/*default anim target is parent node*/
		anim->animation_elt = elt;
	}


	/*parse all att*/
	count = gf_list_count(attrs);
	for (i=0; i<count; i++) {
		GF_SAXAttribute *att = gf_list_get(attrs, i);
		if (!att->value || !strlen(att->value)) continue;
		if (!stricmp(att->name, "style")) {
			svg_parse_style(elt, att->value);
		} else if (!stricmp(att->name, "id")) {
			svg_parse_element_id(parser, elt, att->value);
			ided = 1;
		} else if (anim && !stricmp(att->name, "attributeName")) {
			anim->attributeName = att->value;
			att->value = NULL;
		} else if (anim && !stricmp(att->name, "to")) {
			anim->to = att->value;
			att->value = NULL;
		} else if (anim && !stricmp(att->name, "from")) {
			anim->from = att->value;
			att->value = NULL;
		} else if (anim && !stricmp(att->name, "by")) {
			anim->by = att->value;
			att->value = NULL;
		} else if (anim && !stricmp(att->name, "values")) {
			anim->values = att->value;
			att->value = NULL;
		} else if (&anim && (tag == TAG_SVG_animateTransform) && !stricmp(att->name, "type")) {
			anim->type = att->value;
			att->value = NULL;
		} else if (!stricmp(att->name, "xlink:href")) {

			if (is_svg_animation_tag(tag)) {
				assert(anim);
				anim->target_id = att->value;
				att->value = NULL;
				/*may be NULL*/
				anim->target = (SVGElement *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
			} else {
				GF_FieldInfo info;
				if (!gf_node_get_field_by_name((GF_Node *)elt, "xlink:href", &info)) {
					SVG_IRI *iri = info.far_ptr;

					/*handle "data:" scheme when cache is specified*/
					if (parser->load->localPath && !strncmp(att->value, "data:", 5)) {
						char szFile[GF_MAX_PATH], *sep, buf[20], *data;
						u32 data_size;
						strcpy(szFile, parser->load->localPath);
						data_size = strlen(szFile);
						if (szFile[data_size-1] != GF_PATH_SEPARATOR) {
							szFile[data_size] = GF_PATH_SEPARATOR;
							szFile[data_size+1] = 0;
						}
						strcat(szFile, parser->load->fileName);
						sep = strrchr(szFile, '.');
						if (sep) sep[0] = 0;
						sprintf(buf, "_img_%08X", (u32) iri);
						strcat(szFile, buf);
						/*get mime type*/
						sep = att->value + 5;
						if (!strncmp(sep, "image/jpg", 9) || !strncmp(sep, "image/jpeg", 10)) strcat(szFile, ".jpg");
						else if (!strncmp(sep, "image/png", 9)) strcat(szFile, ".png");

						data = NULL;
						sep = strchr(att->value, ';');
						if (!strncmp(sep, ";base64,", 8)) {
							sep += 8;
							data_size = 2*strlen(sep);
							data = malloc(sizeof(char)*data_size);
							data_size = gf_base64_decode(sep, strlen(sep), data, data_size);
						}
						else if (!strncmp(sep, ";base16,", 8)) {
							data_size = 2*strlen(sep);
							data = malloc(sizeof(char)*data_size);
							sep += 8;
							data_size = gf_base16_decode(sep, strlen(sep), data, data_size);
						}
						iri->type = SVG_IRI_IRI;
						if (data) {
							FILE *f = fopen(szFile, "wb");
							fwrite(data, data_size, 1, f);
							fclose(f);
							iri->iri = strdup(szFile);
							free(data);
						} else {
							iri->iri = att->value;
							att->value = NULL;
						}
					} else {
						svg_parse_attribute(elt, &info, att->value, 0, 0);
						/*unresolved, queue it...*/
						if (is_svg_anchor_tag(tag) && (iri->type==SVG_IRI_ELEMENTID) && !iri->target && iri->iri) {
							gf_list_add(parser->pending_hrefs, iri);
						}
					}
				}
			}
		} else {
			GF_FieldInfo info;
			if (!gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)) {
				svg_parse_attribute(elt, &info, att->value, 0, 0);
				if (anim && (info.fieldType==SMIL_Times_datatype) ) {
					GF_List *l = * (GF_List**)info.far_ptr;
					u32 k, cnt = gf_list_count(l);
					for (k=0; k<cnt; k++) {
						SMIL_Time *t= gf_list_get(l, k);
						if (t->element_id && !t->element) gf_list_add(parser->smil_times, t);
					}
				}
			} else {
				/*SVG 1.1 events: create a listener and a handler on the fly, register them with current node
				and add listener struct*/
				u32 evtType = svg_dom_event_by_name(att->name + 2);
				if (evtType != SVG_DOM_EVT_UNKNOWN) {
					SVGhandlerElement *handler = gf_sg_dom_create_listener((GF_Node *) elt, evtType);
					handler->textContent = att->value;
					att->value = NULL;
					gf_node_init((GF_Node *)handler);
				} else {
					fprintf(stdout, "SVG Warning: Unknown attribute %s on element %s\n", (char *)att->name, gf_node_get_class_name((GF_Node *)elt));
				}
			}
		}
	}

	if (anim) {
		if (!anim->target && !anim->target_id) anim->target = parent;
		if (svg_parse_animation(parser, anim, NULL)) {
			svg_delete_defered_anim(anim, NULL);
		} else {
			gf_list_add(parser->defered_anims, anim);
		}
	}

	/* if the new element has an id, we try to resolve defered references */
	if (ided) {
		svg_resolved_refs(parser, gf_node_get_name((GF_Node *)elt));
	}

	/* We need to init the node at the end of the parsing, after parsing all attributes */
	/* text nodes must be initialized at the end of the <text> element */
	if (!anim && elt) gf_node_init((GF_Node *)elt);
	return elt;
}

static void svg_node_start(void *sax_cbck, const char *node_name, const char *name_space, GF_List *attributes)
{
	SVGElement *elt, *parent;
	GF_SVGParser *parser = sax_cbck;
	
	parent = gf_list_last(parser->nodes);
	if (parser->has_root) {
		assert(parent);
	}
	elt = svg_parse_element(parser, node_name, name_space, attributes, parent);
	if (!elt) return;
	if (!strcmp(node_name, "svg") && !parser->has_root) {
		parser->has_root = 1;
		svg_init_root_element(parser, (SVGsvgElement *)elt);
	}
	gf_list_add(parser->nodes, elt);
}

static void svg_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	GF_SVGParser *parser = sax_cbck;
	/*only remove created nodes ... */
	if (SVG_GetTagByName(node_name) != TAG_UndefinedNode) {
		GF_DOM_Event evt;
		const char *name;
		GF_Node *node = gf_list_last(parser->nodes);
		/*check node name...*/
		name = gf_node_get_class_name(node);
		if (strcmp(name, node_name)) fprintf(stdout, "\n\nERROR: svg depth mismatch \n\n");
		gf_list_rem_last(parser->nodes);

		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = SVG_DOM_EVT_LOAD;
		gf_sg_fire_dom_event(node, &evt);
	}
}

static void svg_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	const char *buf;
	u32 len;
	GF_SVGParser *parser = sax_cbck;
	SVGtitleElement *node = gf_list_last(parser->nodes);
	if (!node) return;

	buf = text_content;
	len = strlen(buf);

	if (!node->xml_space || (node->xml_space != XML_SPACE_PRESERVE)) {
		Bool go = 1;;
		while (go) {
			switch (buf[0]) {
			case '\n': case '\r': case ' ':
				buf ++;
				break;
			default:
				go = 0;
				break;
			}
		}
		len = strlen(buf);
		if (!len) return;
		go = 1;
		while (go) {
			if (!len) break;
			switch (buf[len-1]) {
			case '\n': case '\r': case ' ':
				len--;
				break;
			default:
				go = 0;
				break;
			}
		}
	}
	if (!len) return;

	switch (gf_node_get_tag((GF_Node *)node)) {
	case TAG_SVG_text:
	{
		SVGtextElement *text = (SVGtextElement *)node;
		u32 baselen = 0;
		if (text->textContent) baselen = strlen(text->textContent);
		text->textContent = (char *)realloc(text->textContent,baselen+len+1);
		strncpy(text->textContent + baselen, buf, len);
		text->textContent[baselen+len] = 0;
		gf_node_changed((GF_Node *)node, NULL);
		return;
	}
	case TAG_SVG_script:
	{
		SVGscriptElement *sc = (SVGscriptElement*)node;
		if (sc->textContent) free(sc->textContent);
		sc->textContent = (char *)malloc(sizeof(char)*(len+1));
		strncpy(sc->textContent, buf, len);
		sc->textContent[len] = 0;
		gf_node_init((GF_Node *)node);
		return;
	}

	}
}


static GF_SVGParser *svg_new_parser(GF_SceneLoader *load)
{
	GF_SVGParser *parser;
	GF_SAFEALLOC(parser, sizeof(GF_SVGParser));
	parser->nodes = gf_list_new();
	parser->ided_nodes = gf_list_new();
	parser->pending_hrefs = gf_list_new();
	parser->defered_anims = gf_list_new();
	parser->smil_times = gf_list_new();
	parser->sax_parser = gf_xml_sax_new(svg_node_start, svg_node_end, svg_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;
	if (load->ctx) load->ctx->is_pixel_metrics = 1;

	return parser;
}

GF_Err gf_sm_load_init_SVG(GF_SceneLoader *load)
{
	GF_Err e;
	GF_SVGParser *parser;

	if (!load->fileName) return GF_BAD_PARAM;
	parser = svg_new_parser(load);
	e = gf_xml_sax_parse_file(parser->sax_parser, load->fileName, parser->load->OnProgress ? svg_progress : NULL);
	if (e<0) svg_report(parser, e, "Unable to open file %s", load->fileName);
	return e;
}

GF_Err gf_sm_load_init_SVGString(GF_SceneLoader *load, char *str_data)
{
	GF_Err e;
	GF_SVGParser *parser = load->loader_priv;

	if (!parser) {
		char BOM[4];
		if (strlen(str_data)<4) return GF_BAD_PARAM;
		BOM[0] = str_data[0];
		BOM[1] = str_data[1];
		BOM[2] = str_data[2];
		BOM[3] = str_data[3];
		BOM[4] = 0;
		parser = svg_new_parser(load);
		e = gf_xml_sax_init(parser->sax_parser, BOM);
		if (e) {
			svg_report(parser, e, "Error initializing SAX parser");
			return e;
		}
		str_data += 4;
	}
	return gf_xml_sax_parse(parser->sax_parser, str_data);
}


GF_Err gf_sm_load_run_SVG(GF_SceneLoader *load)
{
	return GF_OK;
}

GF_Err gf_sm_load_done_SVG(GF_SceneLoader *load)
{
	GF_SVGParser *parser = (GF_SVGParser *)load->loader_priv;
	if (!parser) return GF_OK;

	
	gf_list_del(parser->nodes);
	gf_list_del(parser->ided_nodes);
	gf_list_del(parser->pending_hrefs);
	gf_list_del(parser->smil_times);
	
	/*delete all unresolved anims*/
	while (1) {
		DeferedAnimation *anim = gf_list_last(parser->defered_anims);
		if (!anim) break;
		svg_delete_defered_anim(anim, parser);
	}
	gf_list_del(parser->defered_anims);
	
	gf_xml_sax_del(parser->sax_parser);
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}


