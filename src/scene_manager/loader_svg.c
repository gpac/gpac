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

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	Bool has_root;
	
	/* stack of SVG nodes*/
	GF_List *nodes;

	GF_List *defered_hrefs;
	GF_List *defered_animations;

	/*LASeR parsing*/
	GF_StreamContext *laser_es;
	GF_AUContext *laser_au;
	GF_Command *command;
	/*SAF AU maps to OD AU and is used for each new media declaration*/
	GF_AUContext *saf_au;
} GF_SVGParser;


typedef struct {
	/* Stage of the resolving:
	    0: resolving attributes which depends on the target: from, to, by, values, type
		1: resolving begin times
		2: resolving end times */
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
static void svg_progress(void *cbk, u32 done, u32 total)
{
	GF_SVGParser *parser = cbk;
	parser->load->OnProgress(parser->load->cbk, done, total);
}

static void svg_init_root_element(GF_SVGParser *parser, SVGsvgElement *root_svg)
{
	u32 svg_w, svg_h;
	svg_w = svg_h = 0;
	if (root_svg->width.type == SVG_NUMBER_VALUE) {
		svg_w = FIX2INT(root_svg->width.value);
		svg_h = FIX2INT(root_svg->height.value);
	}
	gf_sg_set_scene_size_info(parser->load->scene_graph, svg_w, svg_h, 1);
	if (parser->load->ctx) {
		parser->load->ctx->scene_width = svg_w;
		parser->load->ctx->scene_height = svg_h;
	}
	if (parser->load->type == GF_SM_LOAD_XSR) {
		assert(parser->command);
		assert(parser->command->tag == GF_SG_LSR_NEW_SCENE);
		parser->command->node = (GF_Node *)root_svg;
	} else {
		gf_sg_set_root_node(parser->load->scene_graph, (GF_Node *)root_svg);
	}
	parser->has_root = 1;
}


static void svg_delete_defered_anim(DeferedAnimation *anim, GF_List *defered_animations)
{
	if (defered_animations) gf_list_del_item(defered_animations, anim);

	if (anim->target_id) free(anim->target_id);
	if (anim->attributeName) free(anim->attributeName);
	if (anim->to) free(anim->to);
	if (anim->from) free(anim->from);
	if (anim->by) free(anim->by);
	if (anim->values) free(anim->values);
	if (anim->type) free(anim->type);
	free(anim);
}

void svg_reset_defered_animations(GF_List *l)
{
	/*delete all unresolved anims*/
	while (1) {
		DeferedAnimation *anim = gf_list_last(l);
		if (!anim) break;
		svg_delete_defered_anim(anim, l);
	}
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
		svg_report(parser, GF_BAD_PARAM, "Node %s already defined in document.", nodename);
	} else {
		id = svg_get_node_id(parser, nodename);
		gf_node_set_id((GF_Node *)elt, id, nodename);
	}
}

static Bool svg_resolve_smil_times(GF_SceneGraph *sg, SVGElement *anim_target, GF_List *smil_times, const char *node_name)
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
		if (!t->element_id) {
			/*event target is anim target*/
			if (!t->element) t->element = (GF_Node *)anim_target;
			done++;
			continue;
		}
		if (node_name && strcmp(node_name, t->element_id)) continue;
		t->element = gf_sg_find_node_by_name(sg, t->element_id);
		if (t->element) {
			free(t->element_id);
			t->element_id = NULL;
			done++;
		}
	}
	return (done==count) ? 1 : 0;
}

static Bool svg_parse_animation(GF_SceneGraph *sg, DeferedAnimation *anim, const char *nodeID)
{
	SVG_IRI *iri;
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0, anim_transform_type = 0;

	if (!anim->target) anim->target = (SVGElement *) gf_sg_find_node_by_name(sg, anim->target_id + 1);
	if (!anim->target) return 0;

	if (anim->resolve_stage==0) {

		/*setup IRI ptr*/
		if (gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "xlink:href", &info) != GF_OK)  return 1;

		iri = info.far_ptr;
		iri->iri_owner = anim->animation_elt;
		iri->target = anim->target;
		iri->type = SVG_IRI_ELEMENTID;

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);

		if (anim->attributeName) {
			SMIL_AttributeName *attname_value;
			/* get the type of the target attribute to determine type of the from/to/by ... */
			if (gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "attributeName", &info) != GF_OK) return 1;
			attname_value = (SMIL_AttributeName *)info.far_ptr;

			if (gf_node_get_field_by_name((GF_Node *)anim->target, anim->attributeName, &info) == GF_OK) {
				attname_value->type = info.fieldType;
				attname_value->field_ptr = info.far_ptr;
				/*TODO find a way to get rid of this*/
				attname_value->name = anim->attributeName;
				anim->attributeName = NULL;
			} else {
				return 1;
			}
			anim_value_type = attname_value->type;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else {
				return 1;
			}
		}

		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			/* determine the transform_type in case of animateTransform attribute */
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "type", &info);
			svg_parse_attribute(anim->animation_elt, &info, anim->type, 0, 0);
			anim_transform_type = *(SVG_TransformType *) info.far_ptr;
			anim_value_type = SVG_Matrix_datatype;
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
		if (svg_resolve_smil_times(sg, anim->target, * (GF_List **) info.far_ptr, nodeID)) {
			anim->resolve_stage = 2;
		} else {
			return 0;
		}
	}
	gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "end", &info);
	if (!svg_resolve_smil_times(sg, anim->target, * (GF_List **) info.far_ptr, nodeID)) return 0;
	/*OK init the anim*/
	gf_node_init((GF_Node *)anim->animation_elt);
	return 1;
}

static void svg_resolved_refs(GF_SceneGraph *sg, GF_List *defered_animations, GF_List *defered_hrefs, const char *nodeID)
{
	u32 count, i;

	/*check unresolved HREF*/
	if (defered_hrefs) {
		count = gf_list_count(defered_hrefs);
		for (i=0; i<count; i++) {
			SVG_IRI *iri = gf_list_get(defered_hrefs, i);
			if (nodeID && !strcmp(iri->iri + 1, nodeID)) continue;
			iri->target = (SVGElement *)gf_sg_find_node_by_name(sg, iri->iri + 1);
			if (iri->target) {
				free(iri->iri);
				iri->iri = NULL;
				gf_list_rem(defered_hrefs, i);
				i--;
				count--;
			}
		}
	}

	/*check unresolved anims*/
	if (defered_animations) {
		count = gf_list_count(defered_animations);
		for (i=0; i<count; i++) {
			DeferedAnimation *anim = gf_list_get(defered_animations, i);
			if (nodeID && anim->target_id && strcmp(anim->target_id + 1, nodeID)) continue;
			/*resolve it*/
			if (svg_parse_animation(sg, anim, nodeID)) {
				gf_node_init((GF_Node *)anim->animation_elt);
				svg_delete_defered_anim(anim, defered_animations);
				i--;
				count--;
			}
		}
	}
}

static SVGElement *svg_parse_element(GF_SVGParser *parser, const char *name, const char *name_space, GF_List *attrs, SVGElement *parent)
{
	u32	tag, i, count;
	SVGElement *elt;
	Bool ided = 0;
	DeferedAnimation *anim = NULL;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_get_tag_by_name(name);
	if (tag == TAG_UndefinedNode) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = gf_svg_new_node(parser->load->scene_graph, tag);
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
		anim->target = parent;
	}

	/*parse all att*/
	count = gf_list_count(attrs);
	for (i=0; i<count; i++) {
		GF_SAXAttribute *att = gf_list_get(attrs, i);
		if (!att->value || !strlen(att->value)) continue;
		if (!stricmp(att->name, "style")) {
			/* Special case: style if present will always be first in the list */
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
					if (!svg_store_embedded_data(iri, att->value, parser->load->localPath, parser->load->fileName)){
						svg_parse_attribute(elt, &info, att->value, 0, 0);
						/*unresolved, queue it...*/
						if (/*is_svg_anchor_tag(tag) && */(iri->type==SVG_IRI_ELEMENTID) && !iri->target && iri->iri) {
							gf_list_add(parser->defered_hrefs, iri);
						}
					}
				}
			}
		} else if (!strnicmp(att->name, "xmlns", 5)) {
		} else {
			GF_FieldInfo info;
			u32 evtType = svg_dom_event_by_name(att->name + 2);
			/*SVG 1.1 events: create a listener and a handler on the fly, register them with current node
			and add listener struct*/
			if (evtType != SVG_DOM_EVT_UNKNOWN) {
				SVGhandlerElement *handler = gf_sg_dom_create_listener((GF_Node *) elt, evtType);
				handler->textContent = att->value;
				att->value = NULL;
				gf_node_init((GF_Node *)handler);
			} else if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
				svg_parse_attribute(elt, &info, att->value, 0, 0);
			} else {
				svg_report(parser, GF_OK, "Unknown attribute %s on element %s", (char *)att->name, gf_node_get_class_name((GF_Node *)elt));
			}
		}
	}

	if (anim) {
		if (svg_parse_animation(parser->load->scene_graph, anim, NULL)) {
			svg_delete_defered_anim(anim, NULL);
		} else {
			gf_list_add(parser->defered_animations, anim);
		}
	}

	/* if the new element has an id, we try to resolve defered references */
	if (ided) {
		svg_resolved_refs(parser->load->scene_graph, parser->defered_animations, parser->defered_hrefs, gf_node_get_name((GF_Node *)elt));
	}

	/* We need to init the node at the end of the parsing, after parsing all attributes */
	/* text nodes must be initialized at the end of the <text> element */
	if (!anim && elt) gf_node_init((GF_Node *)elt);
	return elt;
}

static GF_ESD *lsr_parse_header(GF_SVGParser *parser, const char *name, const char *name_space, GF_List *attrs)
{
	GF_ESD *esd;
	u32 i, count;
	if (!strcmp(name, "LASeRHeader")) {
		GF_LASERConfig *lsrc = (GF_LASERConfig *) gf_odf_desc_new(GF_ODF_LASER_CFG_TAG);
		count = gf_list_count(attrs);
		for (i=0; i<count;i++) {
			GF_SAXAttribute *att = gf_list_get(attrs, i);
			if (!strcmp(att->name, "profile")) lsrc->profile = !strcmp(att->value, "full") ? 1 : 0;
			else if (!strcmp(att->name, "level")) lsrc->level = atoi(att->value);
			else if (!strcmp(att->name, "resolution")) lsrc->resolution = atoi(att->value);
			else if (!strcmp(att->name, "timeResolution")) lsrc->time_resolution = atoi(att->value);
			else if (!strcmp(att->name, "coordBits")) lsrc->coord_bits = atoi(att->value);
			else if (!strcmp(att->name, "scaleBits_minus_coordBits")) lsrc->scale_bits = atoi(att->value);
			else if (!strcmp(att->name, "colorComponentBits")) lsrc->colorComponentBits = atoi(att->value);
			else if (!strcmp(att->name, "append")) lsrc->append = !strcmp(att->value, "yes") ? 1 : 0;
			else if (!strcmp(att->name, "useFullRequestHost")) lsrc->fullRequestHost = !strcmp(att->value, "yes") ? 1 : 0;
			else if (!strcmp(att->name, "pathComponents")) lsrc->fullRequestHost = atoi(att->value);
			else if (!strcmp(att->name, "hasStringsIDs")) lsrc->has_string_ids = !strcmp(att->value, "yes") ? 1 : 0;
			/*others are ignored in GPAC atm*/
		}
		esd = gf_odf_desc_esd_new(2);
		gf_odf_desc_del((GF_Descriptor *)esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) lsrc;
		esd->decoderConfig->streamType = GF_STREAM_SCENE;
		esd->decoderConfig->objectTypeIndication = 0x09;
		esd->slConfig->timestampResolution = lsrc->time_resolution ? lsrc->time_resolution : 1000;
		return esd;
	}
	return NULL;
}

static u32 lsr_get_command_by_name(const char *name)
{
	if (!strcmp(name, "NewScene")) return GF_SG_LSR_NEW_SCENE;
	else if (!strcmp(name, "Add")) return GF_SG_LSR_ADD;
	else if (!strcmp(name, "Clean")) return GF_SG_LSR_CLEAN;
	else if (!strcmp(name, "Replace")) return GF_SG_LSR_REPLACE;
	else if (!strcmp(name, "Delete")) return GF_SG_LSR_DELETE;
	else if (!strcmp(name, "Insert")) return GF_SG_LSR_INSERT;
	else if (!strcmp(name, "Restore")) return GF_SG_LSR_RESTORE;
	else if (!strcmp(name, "Save")) return GF_SG_LSR_SAVE;
	else if (!strcmp(name, "SendEvent")) return GF_SG_LSR_SEND_EVENT;
	return GF_SG_UNDEFINED;
}
static void svg_node_start(void *sax_cbck, const char *name, const char *name_space, GF_List *attributes)
{
	SVGElement *elt, *parent;
	GF_SVGParser *parser = sax_cbck;

	parent = gf_list_last(parser->nodes);
	/*saf setup*/
	if (!parent && (parser->load->type==GF_SM_LOAD_XSR)) {
		u32 com_type;
		/*nothing to do, the context is already created*/
		if (!strcmp(name, "SAFSession")) return;
		/*nothing to do, wait for the laser (or other) header before creating stream)*/
		if (!strcmp(name, "sceneHeader")) return;
		/*nothing to do, wait for the laser (or other) header before creating stream)*/
		if (!strcmp(name, "LASeRHeader")) {
			GF_ESD *esd = lsr_parse_header(parser, name, name_space, attributes);
			if (!esd) svg_report(parser, GF_BAD_PARAM, "Invalid LASER Header");
			/*TODO find a better way of assigning an ID to the laser stream...*/
			esd->ESID = 1;
			parser->laser_es = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
			if (!parser->load->ctx->root_od) parser->load->ctx->root_od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
			gf_list_add(parser->load->ctx->root_od->ESDescriptors, esd);
			parser->laser_es->timeScale = esd->slConfig->timestampResolution;
			return;
		}
		if (!strcmp(name, "mediaUnit") || !strcmp(name, "imageUnit")) {
			u32 time, startOffset, length, rap, i, count;
			char *source = NULL;
			time = startOffset = length = rap = 0;
			count = gf_list_count(attributes);
			for (i=0; i<count;i++) {
				GF_SAXAttribute *att = gf_list_get(attributes, i);
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "startOffset")) startOffset = atoi(att->value);
				else if (!strcmp(att->name, "length")) length = atoi(att->value);
				else if (!strcmp(att->name, "source")) { source = att->value; att->value = NULL; }
			}
			/*TODO build NHML on the fly for later import*/
			if (source) free(source);
			return;			
		}
		if (!strcmp(name, "sceneUnit") ) {
			u32 time, rap, i, count;
			time = rap = 0;
			if (!gf_list_count(parser->laser_es->AUs)) rap = 1;
			count = gf_list_count(attributes);
			for (i=0; i<count;i++) {
				GF_SAXAttribute *att = gf_list_get(attributes, i);
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
			}
			/*create new laser unit*/
			parser->laser_au = gf_sm_stream_au_new(parser->laser_es, time, 0, rap);
			return;			
		}
		/*command parsing*/
		com_type = lsr_get_command_by_name(name);
		if (com_type != GF_SG_UNDEFINED) {
			parser->command = gf_sg_command_new(parser->load->scene_graph, GF_SG_LSR_NEW_SCENE);
			gf_list_add(parser->laser_au->commands, parser->command);
			return;
		}
	}

	if (parser->has_root) {
		assert(parent);
	}
	elt = svg_parse_element(parser, name, name_space, attributes, parent);
	if (!elt) return;
	if (!strcmp(name, "svg") && !parser->has_root) 
		svg_init_root_element(parser, (SVGsvgElement *)elt);
	
	gf_list_add(parser->nodes, elt);
}

static void svg_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	GF_SVGParser *parser = sax_cbck;
	GF_Node *node = gf_list_last(parser->nodes);

	if (!node) {
		if (parser->laser_au && !strcmp(name, "sceneUnit")) {
			parser->laser_au = NULL;
			return;
		}
		if (parser->command) {
			u32 com_type = lsr_get_command_by_name(name);
			if (com_type == parser->command->tag) {
				parser->command = NULL;
				return;
			}
		}
	}
	/*only remove created nodes ... */
	if (gf_svg_get_tag_by_name(name) != TAG_UndefinedNode) {
		const char *name;
		/*check node name...*/
		name = gf_node_get_class_name(node);
		if (strcmp(name, name)) fprintf(stdout, "\n\nERROR: svg depth mismatch \n\n");
		gf_list_rem_last(parser->nodes);

		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = SVG_DOM_EVT_LOAD;
			gf_sg_fire_dom_event(node, &evt);
		}
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

	if (!node->core->space || (node->core->space != XML_SPACE_PRESERVE)) {
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
	if ((load->type==GF_SM_LOAD_XSR) && !load->ctx) return NULL;
	GF_SAFEALLOC(parser, sizeof(GF_SVGParser));
	parser->nodes = gf_list_new();
	parser->defered_hrefs = gf_list_new();
	parser->defered_animations = gf_list_new();
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
	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, parser->load->OnProgress ? svg_progress : NULL);
	if (e<0) svg_report(parser, e, "Unable to open file %s", load->fileName);
	return e;
}

GF_Err gf_sm_load_init_SVGString(GF_SceneLoader *load, char *str_data)
{
	GF_Err e;
	GF_SVGParser *parser = load->loader_priv;

	if (!parser) {
		char BOM[5];
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
	gf_list_del(parser->defered_hrefs);
	svg_reset_defered_animations(parser->defered_animations);
	gf_list_del(parser->defered_animations);
	gf_xml_sax_del(parser->sax_parser);
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}


