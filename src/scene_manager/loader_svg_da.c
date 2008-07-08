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
#include <gpac/events.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/laser_dev.h>
#include <gpac/nodes_svg_da.h>

#ifndef GPAC_DISABLE_SVG

typedef struct _st_entry
{
	struct _st_entry *next;
	/*as refered to by xlink-href*/
	char *stream_name;
	/*stream id*/
	u32 id;
	const char *nhml_info;
} SVG_SAFExternalStream;

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	Bool has_root;
	
	/* stack of SVG nodes*/
	GF_List *node_stack;

	GF_List *defered_hrefs;
	GF_List *defered_animations;
	GF_List *defered_listeners;
	/*non-linear parsing*/
	GF_List *peeked_nodes;

	/*LASeR parsing*/
	u32 command_depth;
	GF_StreamContext *laser_es;
	GF_AUContext *laser_au;
	GF_Command *command;
	/*SAF AU maps to OD AU and is used for each new media declaration*/
	GF_AUContext *saf_au;
	GF_StreamContext *saf_es;

	SVG_SAFExternalStream *streams;
} GF_SVG_Parser;

typedef struct {
	/* Stage of the resolving:
	    0: resolving attributes which depends on the target: from, to, by, values, type
		1: resolving begin times
		2: resolving end times */
	u32 resolve_stage;
	/* Animation element being defered */
	SVG_Element *animation_elt;
	/* anim parent*/
	SVG_Element *anim_parent;
	/* target animated element*/
	SVG_Element *target;
	/* id of the target element when unresolved*/
	char *target_id;

	/* attributes which cannot be parsed until the type of the target attribute is known */
	char *type; /* only for animateTransform */
	char *to;
	char *from;
	char *by;
	char *values;
} SVG_DeferedAnimation;

typedef struct
{
	/*top of parsed sub-tree*/
	SVG_Element *node;
	/*the current element is animation that cannot be parsed completely 
	  upon reception of start tag of element but for which we may be able 
	  to parse at the end tag of the element (animateMotion)*/
	SVG_DeferedAnimation *anim;
	/*depth of unknown elements being skipped*/
	u32 unknown_depth;
	/*last child added, used to speed-up parsing*/
	GF_ChildNodeItem *last_child;
} SVG_NodeStack;

static GF_Err svg_report(GF_SVG_Parser *parser, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_get_level() && (gf_log_get_tools() & GF_LOG_PARSER)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsprintf(szMsg, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[SVG Parsing] line %d - %s\n", gf_xml_sax_get_line(parser->sax_parser), szMsg));
	}
#endif
	if (e) parser->last_error = e;
	return e;
}

static void svg_progress(void *cbk, u32 done, u32 total)
{
	gf_set_progress("SVG (Dynamic Attribute List) Parsing", done, total);
}

static SVG_SAFExternalStream *svg_saf_get_stream(GF_SVG_Parser *parser, u32 id, const char *name)
{
	SVG_SAFExternalStream *st;
	st = parser->streams;
	while (st) {
		if (id == st->id) return st;
		if (name && !strcmp(name, st->stream_name) ) return st;
		st = st->next;
	}
	return NULL;
}

static SVG_SAFExternalStream *svg_saf_get_next_available_stream(GF_SVG_Parser *parser)
{
	/*1 reserved for base laser stream*/
	u32 id = 1;
	SVG_SAFExternalStream *st = parser->streams;
	SVG_SAFExternalStream *prev = NULL;

	while (st) {
		prev = st;
		id = st->id;
		st = st->next;
	}
	GF_SAFEALLOC(st, SVG_SAFExternalStream);
	if (prev) prev->next = st;
	else parser->streams = st;
	st->id = id+1;
	return st;
}

static void svg_lsr_set_v2(GF_SVG_Parser *parser)
{
	u32 i;
	if (parser->load->ctx && parser->load->ctx->root_od) {
		for (i=0; i<gf_list_count(parser->load->ctx->root_od->ESDescriptors); i++) {
			GF_ESD *esd = gf_list_get(parser->load->ctx->root_od->ESDescriptors, i);
			if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
				GF_LASERConfig *cfg = (GF_LASERConfig *)esd->decoderConfig->decoderSpecificInfo;
				if (cfg && (cfg->tag==GF_ODF_LASER_CFG_TAG)) {
					if (!cfg->extensionIDBits) cfg->extensionIDBits = 2;
				}
			}
		}
	}
}


static void svg_process_media_href(GF_SVG_Parser *parser, XMLRI *iri)
{
	SVG_SAFExternalStream *st = svg_saf_get_stream(parser, 0, iri->string+1);
	if (st) {
		free(iri->string);
		iri->string = NULL;
		iri->lsr_stream_id = st->id;
		iri->type = XMLRI_STREAMID;
	}
}

static void xsr_exec_command_list(GF_Node *node, void *par, Bool is_destroy)
{
	GF_DOMUpdates *up = (GF_DOMUpdates *)node;
	if (is_destroy || !up || (up->sgprivate->tag!=TAG_DOMUpdates)) return;
	gf_sg_command_apply_list(up->sgprivate->scenegraph, up->updates, 0);
}

static GF_Node *svg_find_node(GF_SVG_Parser *parser, char *ID)
{
	u32 i, count, tag;
	char *node_class;
	GF_Node *n = gf_sg_find_node_by_name(parser->load->scene_graph, ID);
	if (n) return n;

	count = gf_list_count(parser->peeked_nodes);
	for (i=0; i<count; i++) {
		n = (GF_Node*)gf_list_get(parser->peeked_nodes, i);
		if (!strcmp(gf_node_get_name(n), ID)) return n;
	}
	node_class = gf_xml_sax_peek_node(parser->sax_parser, "id", ID, NULL, NULL, NULL, NULL);
	if (!node_class) return NULL;

	tag = gf_svg_get_element_tag(node_class);
	n = gf_node_new(parser->load->scene_graph, tag);

	free(node_class);

	if (n) {
		gf_svg_parse_element_id(n, ID, 0);
		gf_list_add(parser->peeked_nodes, n);
	}
	return n;
}

static void svg_post_process_href(GF_SVG_Parser *parser, XMLRI *iri)
{
	GF_Err e;
	/*keep data when encoding*/
	if ( !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	/*unresolved, queue it...*/
	if ((iri->type==XMLRI_ELEMENTID) && !iri->target && iri->string) {
		gf_list_add(parser->defered_hrefs, iri);
	}
	if (iri->type != XMLRI_STRING) return;
	e = gf_svg_store_embedded_data(iri, parser->load->localPath, parser->load->fileName);
	if (e) svg_report(parser, e, "Error storing embedded IRI data");
}

static void svg_delete_defered_anim(SVG_DeferedAnimation *anim, GF_List *defered_animations)
{
	if (defered_animations) gf_list_del_item(defered_animations, anim);

	if (anim->target_id) free(anim->target_id);
	if (anim->to) free(anim->to);
	if (anim->from) free(anim->from);
	if (anim->by) free(anim->by);
	if (anim->values) free(anim->values);
	if (anim->type) free(anim->type);
	free(anim);
}

static Bool svg_parse_animation(GF_SVG_Parser *parser, GF_SceneGraph *sg, SVG_DeferedAnimation *anim, const char *nodeID, u32 force_type)
{
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0;

	if (anim->resolve_stage==0) {
		/* Stage 0: parsing the animation attribute values 
					for that we need to resolve the target first */
		if (!anim->target) 
			anim->target = (SVG_Element *) gf_sg_find_node_by_name(sg, anim->target_id + 1);

		if (!anim->target) {
			/* the target is still not known stay in stage 0 */
			return 0;
		} else { 
			XMLRI *iri;
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_xlink_href, 1, 0, &info);
			iri = (XMLRI *)info.far_ptr;
			iri->type = XMLRI_ELEMENTID;
			iri->target = anim->target;
			gf_svg_register_iri(sg, iri);
		}

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);
		/* get the attribute name attribute if specified */
		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_transform_type, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->type, 0);
			switch(*(SVG_TransformType *) info.far_ptr) {
			case SVG_TRANSFORM_TRANSLATE:
				anim_value_type = SVG_Transform_Translate_datatype;
				break;
			case SVG_TRANSFORM_SCALE:
				anim_value_type = SVG_Transform_Scale_datatype;
				break;
			case SVG_TRANSFORM_ROTATE:
				anim_value_type = SVG_Transform_Rotate_datatype;
				break;
			case SVG_TRANSFORM_SKEWX:
				anim_value_type = SVG_Transform_SkewX_datatype;
				break;
			case SVG_TRANSFORM_SKEWY:
				anim_value_type = SVG_Transform_SkewY_datatype;
				break;
			case SVG_TRANSFORM_MATRIX:
				anim_value_type = SVG_Transform_datatype;
				break;
			default:
				svg_report(parser, GF_OK, "unknown datatype for animate transform");
				return 0;
			}
		}
		else if (gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_attributeName, 0, 0, &info) == GF_OK) {
			gf_svg_get_attribute_by_name((GF_Node *)anim->target, ((SMIL_AttributeName *)info.far_ptr)->name, 1, 1, &info);
			anim_value_type = info.fieldType;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else if (tag == TAG_SVG_discard) {
				/* there is no value to parse in discard, we can jump to the next stage */
				anim->resolve_stage = 1;
				return svg_parse_animation(parser, sg, anim, nodeID, 0);
			} else {
				svg_report(parser, GF_OK, "Missing attributeName attribute on %s", gf_node_get_name((GF_Node *)anim->animation_elt));
				return 0;
			}
		}

		if (anim->to) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_to, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->to, anim_value_type);
			if (anim_value_type==XMLRI_datatype) 
				svg_post_process_href(parser, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->from) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_from, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->from, anim_value_type);
			if (anim_value_type==XMLRI_datatype) 
				svg_post_process_href(parser, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->by) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_by, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->by, anim_value_type);
			if (anim_value_type==XMLRI_datatype) 
				svg_post_process_href(parser, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->values) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_values, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->values, anim_value_type);
			if (anim_value_type==XMLRI_datatype) {
				u32 i, count;
				SMIL_AnimateValues *anim_values;
				anim_values = (SMIL_AnimateValues *)info.far_ptr;
				count = gf_list_count(anim_values->values);
				for (i=0; i<count; i++) {
					XMLRI *iri = (XMLRI *)gf_list_get(anim_values->values, i);
					svg_post_process_href(parser, iri);
				}
			}
		}
		anim->resolve_stage = 1;
	}

	if (anim->resolve_stage == 1) {
		/* Stage 1: parsing the begin values 
					we go into the next stage only if at least one begin value is resolved */
		gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_begin, 1, 0, &info);
		if (gf_svg_resolve_smil_times(sg, anim->target, *(GF_List **)info.far_ptr, 0, nodeID)) {
			anim->resolve_stage = 2;
		} else if (force_type!=2) {
			return 0;
		}
	}

	gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_end, 1, 0, &info);
	if (!gf_svg_resolve_smil_times(sg, anim->target, *(GF_List **)info.far_ptr, 1, nodeID)) {
		if (force_type!=2) return 0;
	}

	/*animateMotion needs its children to be parsed before it can be initialized !! */
	if (force_type || gf_node_get_tag((GF_Node *)anim->animation_elt) != TAG_SVG_animateMotion) {
		gf_node_init((GF_Node *)anim->animation_elt);
		return 1;
	} else {
		return 0;
	}
	
}

static void svg_resolved_refs(GF_SVG_Parser *parser, GF_SceneGraph *sg, const char *nodeID)
{
	u32 count, i;

	/*check unresolved HREF*/
	count = gf_list_count(parser->defered_hrefs);
	for (i=0; i<count; i++) {
		GF_Node *targ;
		XMLRI *iri = (XMLRI *)gf_list_get(parser->defered_hrefs, i);
		if (nodeID && strcmp(iri->string + 1, nodeID)) continue;
		targ = gf_sg_find_node_by_name(sg, iri->string + 1);
		if (targ) {
			iri->type = XMLRI_ELEMENTID;
			iri->target = targ;
			gf_svg_register_iri(sg, iri);
			free(iri->string);
			iri->string = NULL;
			gf_list_rem(parser->defered_hrefs, i);
			i--;
			count--;
		}
	}

	/*check unresolved listeners */
	count = gf_list_count(parser->defered_listeners);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_Node *par;
		SVG_Element *listener = (SVG_Element *)gf_list_get(parser->defered_listeners, i);

		par = NULL;
		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_observer, 0, 0, &info) == GF_OK) {
			XMLRI *observer = info.far_ptr;
			if (observer->type == XMLRI_ELEMENTID) {
				if (!observer->target && observer->string && !strcmp(observer->string, nodeID) ) {
					observer->target = gf_sg_find_node_by_name(sg, (char*) nodeID);
				}
			}
			if (observer->type == XMLRI_ELEMENTID) par = observer->target;
			if (!par) continue;
		}
		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 0, 0, &info) == GF_OK) {
			XMLRI *target = info.far_ptr;
			if (target->type == XMLRI_ELEMENTID) {
				if (!target->target) continue;
				else {
					if (!par) par = (GF_Node*)target->target;
				}
			}
		}
		assert(par);
		gf_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
		gf_list_rem(parser->defered_listeners, i);
		i--;
		count--;
	}

	/*check unresolved anims*/
	count = gf_list_count(parser->defered_animations);
	for (i=0; i<count; i++) {
		SVG_DeferedAnimation *anim = (SVG_DeferedAnimation *)gf_list_get(parser->defered_animations, i);
		/*resolve it - we don't check the name since it may be used in SMIL times, which we don't track at anim level*/
		if (svg_parse_animation(parser, sg, anim, nodeID, 1)) {
			svg_delete_defered_anim(anim, parser->defered_animations);
			i--;
			count--;
		}
	}
}

static void svg_init_root_element(GF_SVG_Parser *parser, SVG_Element *root_svg)
{
	GF_FieldInfo width_info, height_info;
	u32 svg_w, svg_h;
	svg_w = svg_h = 0;
	if (!gf_svg_get_attribute_by_tag((GF_Node *)root_svg, TAG_SVG_ATT_width, 0, 0, &width_info)
		&& !gf_svg_get_attribute_by_tag((GF_Node *)root_svg, TAG_SVG_ATT_height, 0, 0, &height_info)) {
		SVG_Length * w = width_info.far_ptr;
		SVG_Length * h = height_info.far_ptr;
		if (w->type == SVG_NUMBER_VALUE) svg_w = FIX2INT(w->value);
		if (h->type == SVG_NUMBER_VALUE) svg_h = FIX2INT(h->value);
		gf_sg_set_scene_size_info(parser->load->scene_graph, svg_w, svg_h, 1);
		if (parser->load->ctx) {
			parser->load->ctx->scene_width = svg_w;
			parser->load->ctx->scene_height = svg_h;
		}
	}
	if (parser->load->type == GF_SM_LOAD_XSR) {
		assert(parser->command);
		assert(parser->command->tag == GF_SG_LSR_NEW_SCENE);
		parser->command->node = (GF_Node *)root_svg;
	}
	gf_sg_set_root_node(parser->load->scene_graph, (GF_Node *)root_svg);
	parser->has_root = 1;
}

//#define SKIP_ALL
//#define SKIP_ATTS
//#define SKIP_INIT

static SVG_Element *svg_parse_element(GF_SVG_Parser *parser, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes, SVG_NodeStack *parent)
{
	GF_FieldInfo info;
	u32	tag, i, count;
	Bool needs_init, has_id;
	SVG_Element *elt = NULL;
	const char *node_name = NULL;
	const char *ev_event, *ev_observer;
	SVG_DeferedAnimation *anim = NULL;
	
	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_get_element_tag(name);
	if (tag == TAG_UndefinedNode) {
		//svg_report(parser, GF_OK, "Unknown element %s - skipping\n", name);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[SVG Parsing] line %d - Unknown element %s - skipping\n", gf_xml_sax_get_line(parser->sax_parser), name));
		return NULL;
	}

	has_id = 0;
	count = gf_list_count(parser->peeked_nodes);
	if (count) {
		char *ID = NULL;
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
			if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
				ID = att->value;
				break;
			}
		}
		if (ID) {
			for (i=0; i<count;i++) {
				GF_Node *n = gf_list_get(parser->peeked_nodes, i);
				const char *n_id = gf_node_get_name(n);
				if (n_id && !strcmp(n_id, ID)) {
					gf_list_rem(parser->peeked_nodes, i);
					has_id = 1;
					elt = (SVG_Element*)n;
					break;
				}
			}
		}
	}

	if (!has_id) {
		/* Creates a node in the current scene graph */
		elt = (SVG_Element*)gf_node_new(parser->load->scene_graph, tag);
		if (!elt) {
			parser->last_error = GF_SG_UNKNOWN_NODE;
			return NULL;
		}
		/*set root BEFORE processing attributes in order to have it setup for script init*/
		if ((tag == TAG_SVG_svg) && !parser->has_root)
			svg_init_root_element(parser, elt);
	}

	gf_node_register((GF_Node *)elt, (parent ? (GF_Node *)parent->node : NULL));
	if (parent && elt) gf_node_list_add_child_last( & parent->node->children, (GF_Node*)elt, & parent->last_child);


	needs_init = 1;

	if (gf_svg_is_animation_tag(tag)) {
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		if (!parent) {
			if (parser->command) {
				anim->target = anim->anim_parent = (SVG_Element*) parser->command->node;
			}
		} else {
			anim->target = anim->anim_parent = parent->node;
		}
	} else if (tag == TAG_SVG_video || 
			   tag == TAG_SVG_audio || 
			   tag == TAG_SVG_animation) {
		/* warning: we use the SVG_DeferedAnimation structure for some timing nodes which are not 
		   animations, but we put the parse stage at 1 (timing) see svg_parse_animation. */
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		if (!parent) {
			if (parser->command) {
				anim->target = anim->anim_parent = (SVG_Element*) parser->command->node;
			}
		} else {
			anim->target = anim->anim_parent = parent->node;
		}
		anim->resolve_stage = 1;
	} else if ((tag == TAG_SVG_script) || (tag==TAG_SVG_handler)) {
		needs_init = 0;
	}

	ev_event = ev_observer = NULL;

#ifdef SKIP_ATTS
	nb_attributes = 0;
#endif

	/*parse all att*/
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;

		if (!stricmp(att->name, "style")) {
			gf_svg_parse_style((GF_Node *)elt, att->value);
		} else if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			if (!has_id) gf_svg_parse_element_id((GF_Node *)elt, att->value, parser->command_depth ? 1 : 0);
			node_name = att->value;
		} else if (anim && !stricmp(att->name, "to")) {
			anim->to = strdup(att->value);
		} else if (anim && !stricmp(att->name, "from")) {
			anim->from = strdup(att->value);
		} else if (anim && !stricmp(att->name, "by")) {
			anim->by = strdup(att->value);
		} else if (anim && !stricmp(att->name, "values")) {
			anim->values = strdup(att->value);
		} else if (anim && (tag == TAG_SVG_animateTransform) && !stricmp(att->name, "type")) {
			anim->type = strdup(att->value);
		} else if (!stricmp(att->name, "xlink:href") ) {
			if (gf_svg_is_animation_tag(tag)) {
				assert(anim);
				anim->target_id = strdup(att->value);
				/*may be NULL*/
				anim->target = (SVG_Element *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
			} else {
				GF_FieldInfo info;
				XMLRI *iri = NULL;
				if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
					gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
					iri = info.far_ptr;
				} else {
					svg_report(parser, GF_OK, "Skipping %s", att->name);
				}
				if ((tag==TAG_SVG_image) || (tag==TAG_SVG_video) || (tag==TAG_SVG_audio)) {
					svg_process_media_href(parser, iri);
				}
				svg_post_process_href(parser, iri);
			}
		} else if (!stricmp(att->name, "ev:event") &&  tag == TAG_SVG_handler) {
			ev_event = att->value;
		} else if (!stricmp(att->name, "ev:observer") &&  tag == TAG_SVG_handler) {
			ev_observer = att->value;
		}
		/*laser specific stuff*/
		else if (!stricmp(att->name, "lsr:scale") ) {
			if (gf_svg_get_attribute_by_tag((GF_Node *)elt, TAG_SVG_ATT_transform, 1, 1, &info)==GF_OK) {
				SVG_Point pt;
				SVG_Transform *mat = info.far_ptr;
				svg_parse_point(&pt, att->value);
				gf_mx2d_add_scale(&mat->mat, pt.x, pt.y);
			}
		} else if (!stricmp(att->name, "lsr:translation") ) {
			if (gf_svg_get_attribute_by_tag((GF_Node *)elt, TAG_SVG_ATT_transform, 1, 1, &info)==GF_OK) {
				SVG_Point pt;
				SVG_Transform *mat = info.far_ptr;
				svg_parse_point(&pt, att->value);
				gf_mx2d_add_translation(&mat->mat, pt.x, pt.y);
			}
		} else if (!strncmp(att->name, "xmlns", 5)) {

		} else if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
			gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);

			switch (info.fieldIndex) {
			case TAG_SVG_ATT_syncMaster:
			case TAG_SVG_ATT_focusHighlight: 
			case TAG_SVG_ATT_initialVisibility: 
			case TAG_SVG_ATT_fullscreen: 
			case TAG_SVG_ATT_requiredFonts: 
				/*switch to v2*/
				svg_lsr_set_v2(parser);
				break;
			}
		} else if (!strncmp(att->name, "on", 2)) {
			u32 evtType = gf_dom_event_type_by_name(att->name + 2);
			if (evtType != GF_EVENT_UNKNOWN) {
				SVG_handlerElement *handler = gf_dom_listener_build((GF_Node *) elt, evtType, 0, NULL);
				gf_dom_add_text_node((GF_Node *)handler, strdup(att->value) );
				gf_node_init((GF_Node *)handler);
			} else {
				svg_report(parser, GF_OK, "Skipping unknown event handler %s on node %s", att->name, name);
			}
		} else {
			svg_report(parser, GF_OK, "Skipping attribute %s on node %s", att->name, name);
		}
	}

	if (ev_event) {
		/* When the handler element specifies the event attribute, an implicit listener is defined */
		GF_Node *node = (GF_Node *)elt;
		SVG_Element *listener;
		u32 type;
		GF_FieldInfo info;
		listener = (SVG_Element *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
		gf_node_register((GF_Node *)listener, node);
		gf_node_list_add_child( & ((GF_ParentNode *)node)->children, (GF_Node*) listener);
		/* this listener listens to the given type of event */
		type = gf_dom_event_type_by_name(ev_event);
		gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_ev_event, 1, 0, &info);
		((XMLEV_Event *)info.far_ptr)->type = type;
		gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_event, 1, 0, &info);
		((XMLEV_Event *)info.far_ptr)->type = type;
		gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_handler, 1, 0, &info);
		((XMLRI *)info.far_ptr)->target = node;

		if (ev_observer) {
			gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_observer, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)elt, &info, (char*)ev_observer, 0);
		} else {
			/* this listener listens with the parent of the handler as the event target */
			gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 1, 0, &info);
			((XMLRI *)info.far_ptr)->target = parent->node;
		}
		if ( ((XMLRI *)info.far_ptr)->target) 
			gf_dom_listener_add(((XMLRI *)info.far_ptr)->target, (GF_Node *) listener);
		else
			gf_list_add(parser->defered_listeners, listener);
	}

	/* if the new element has an id, we try to resolve defered references */
	if (node_name) {
		svg_resolved_refs(parser, parser->load->scene_graph, node_name);
	}

	/* if the new element is an animation, now that all specified attributes have been found,
	   we can start parsing them */
	if (anim) {
		/*FIXME - we need to parse from/to/values but not initialize the stack*/
//		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
			needs_init = 0;
			if (svg_parse_animation(parser, parser->load->scene_graph, anim, NULL, 0)) {
				svg_delete_defered_anim(anim, NULL);
			} else {
				gf_list_add(parser->defered_animations, anim);
			}
//		} else {
//			svg_delete_defered_anim(anim, NULL);
//		}
	}

#ifndef SKIP_INIT
	if (needs_init) gf_node_init((GF_Node *)elt);
#endif

	if (parent && elt) {
		/*mark parent element as dirty (new child added) and invalidate parent graph for progressive rendering*/
		gf_node_dirty_set((GF_Node *)parent->node, GF_SG_CHILD_DIRTY, 1);
		/*request scene redraw*/
		if (parser->load->scene_graph->NodeCallback) 
			parser->load->scene_graph->NodeCallback(parser->load->scene_graph->userpriv, GF_SG_CALLBACK_MODIFIED, NULL, NULL);
	}

	/*register listener element*/
	if ((parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) && elt && (tag==TAG_SVG_listener)) {
		GF_FieldInfo info;
		Bool post_pone = 0;
		SVG_Element *par = NULL;
		SVG_Element *listener = (SVG_Element *)elt;

		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_observer, 0, 0, &info) == GF_OK) {
			XMLRI *observer = info.far_ptr;
			if (observer->type == XMLRI_ELEMENTID) {
				if (!observer->target) post_pone = 1;
				else par = observer->target;
			}
		}

		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 0, 0, &info) == GF_OK) {
			XMLRI *target = info.far_ptr;
			if (!par && (target->type == XMLRI_ELEMENTID)) {
				if (!target->target) post_pone = 1;
				else par = target->target;
			}
		}
		/*check handler, create it if not specified*/
		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_handler, 1, 0, &info) == GF_OK) {
			XMLRI *handler = info.far_ptr;
			if (!handler->target) {
				if (!handler->string) handler->target = parent->node;
			}
		}
		/*if event is a key event, register it with root*/
		if (!par && gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_event, 0, 0, &info) == GF_OK) {
			XMLEV_Event *ev = info.far_ptr;
			if (ev->type>GF_EVENT_MOUSEWHEEL) par = (SVG_Element*) listener->sgprivate->scenegraph->RootNode;
		}

		if (post_pone) {
			gf_list_add(parser->defered_listeners, listener);
		} else {
			if (!par) par = parent->node;
			gf_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
		}
	}
	return elt;
}


static GF_Err lsr_parse_command(GF_SVG_Parser *parser, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_FieldInfo info;
	GF_Node *opNode;
	Bool is_replace = 0;
	char *atNode = NULL;
	char *atAtt = NULL;
	char *atOperandNode = NULL;
	char *atOperandAtt = NULL;
	char *atValue = NULL;
	char *atPoint = NULL;
	char *atInteger = NULL;
	char *atEvent = NULL;
	char *atString = NULL;
	GF_CommandField *field;
	s32 index = -1;
	u32 i;
	switch (parser->command->tag) {
	case GF_SG_LSR_NEW_SCENE:
		return GF_OK;
	case GF_SG_LSR_DELETE:
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!strcmp(att->name, "ref")) atNode = att->value;
			else if (!strcmp(att->name, "attributeName")) atAtt = att->value;
			else if (!strcmp(att->name, "index")) index = atoi(att->value);
		}
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		/*should be a XML IDREF, not an XML IRI*/
		if (atNode[0]=='#') atNode++;

		parser->command->node = svg_find_node(parser, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		if (atAtt) {
			if (!strcmp(atAtt, "children")) {
				field = gf_sg_command_field_new(parser->command);
				field->pos = index;
				field->fieldIndex = 0;
				field->fieldType = 0;
			} else {
				if (gf_svg_get_attribute_by_name(parser->command->node, atAtt, 0, 0, &info)==GF_OK) {
					field = gf_sg_command_field_new(parser->command);
					field->pos = index;
					field->fieldIndex = info.fieldIndex;
					field->fieldType = info.fieldType;
				}
			}
		} else if (index>=0) {
			field = gf_sg_command_field_new(parser->command);
			field->pos = index;
		}
		gf_node_register(parser->command->node, NULL);
		return GF_OK;
	case GF_SG_LSR_REPLACE:
		is_replace = 1;
	case GF_SG_LSR_ADD:
	case GF_SG_LSR_INSERT:
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
			if (!strcmp(att->name, "ref")) atNode = att->value;
			else if (!strcmp(att->name, "operandElementId")) atOperandNode = att->value;
			else if (!strcmp(att->name, "operandAttributeName")) atOperandAtt = att->value;
			else if (!strcmp(att->name, "value")) atValue = att->value;
			else if (!strcmp(att->name, "attributeName")) atAtt = att->value;
			/*replace only*/
			else if (!strcmp(att->name, "index")) index = atoi(att->value);
		}
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		parser->command->node = svg_find_node(parser, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		/*child or node replacement*/
		if ( (is_replace || (parser->command->tag==GF_SG_LSR_INSERT)) && (!atAtt || !strcmp(atAtt, "children")) ) {
			field = gf_sg_command_field_new(parser->command);
			field->pos = index;
			if (atAtt) field->fieldIndex = TAG_LSR_ATT_children;
			gf_node_register(parser->command->node, NULL);
			return GF_OK;
		}
		if (!atAtt) return svg_report(parser, GF_BAD_PARAM, "Missing attribute name for command");
		if (!strcmp(atAtt, "textContent")) {
			field = gf_sg_command_field_new(parser->command);
			field->pos = -1;
			field->fieldIndex = (u32) -1;
			field->fieldType = SVG_String_datatype;
			gf_node_register(parser->command->node, NULL);
			if (atValue) {
				field->field_ptr = gf_svg_create_attribute_value(field->fieldType);
				*(SVG_String *)field->field_ptr = strdup(atValue);
			}
			return GF_OK;
		}
		if (!strcmp(atAtt, "scale") || !strcmp(atAtt, "translation") || !strcmp(atAtt, "rotation")) {
			if (!strcmp(atAtt, "scale")) {
				info.fieldType = SVG_Transform_Scale_datatype;
				info.fieldIndex = TAG_LSR_ATT_scale;
			}
			else if (!strcmp(atAtt, "translation")) {
				info.fieldType = SVG_Transform_Translate_datatype;
				info.fieldIndex = TAG_LSR_ATT_translation;
			}
			else if (!strcmp(atAtt, "rotation")) {
				info.fieldType = SVG_Transform_Rotate_datatype;
				info.fieldIndex = TAG_LSR_ATT_rotation;
			}
		} else {
			info.fieldIndex = gf_svg_get_attribute_tag(parser->command->node->sgprivate->tag, atAtt);
			info.fieldType = gf_svg_get_attribute_type(info.fieldIndex);

			if (gf_lsr_anim_type_from_attribute(info.fieldIndex)<0) {
				return svg_report(parser, GF_BAD_PARAM, "Attribute %s of element %s is not updatable\n", atAtt, gf_node_get_class_name(parser->command->node));
			}
		}

		opNode = NULL;
		if (atOperandNode) {
			opNode = gf_sg_find_node_by_name(parser->load->scene_graph, atOperandNode);
			if (!opNode) return svg_report(parser, GF_BAD_PARAM, "Cannot find operand element %s for command", atOperandNode);
		}
		if (!atValue && (!atOperandNode || !atOperandAtt) ) return svg_report(parser, GF_BAD_PARAM, "Missing attribute value for command");
		field = gf_sg_command_field_new(parser->command);
		field->pos = index;
		field->fieldIndex = info.fieldIndex;
		field->fieldType = info.fieldType;
		if (atValue) {
			GF_FieldInfo nf;
			nf.fieldType = info.fieldType;
			field->field_ptr = nf.far_ptr = gf_svg_create_attribute_value(info.fieldType);
			if (field->field_ptr) gf_svg_parse_attribute(parser->command->node, &nf, atValue, (u8) info.fieldType);
		} else if (opNode) {
			parser->command->fromNodeID = gf_node_get_id(opNode);
			parser->command->fromFieldIndex = gf_svg_get_attribute_tag(opNode->sgprivate->tag, atOperandAtt);
		}
		gf_node_register(parser->command->node, NULL);
		return GF_OK;
	case GF_SG_LSR_ACTIVATE:
	case GF_SG_LSR_DEACTIVATE:
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!strcmp(att->name, "ref")) atNode = att->value;
		}
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		/*should be a XML IDREF, not an XML IRI*/
		if (atNode[0]=='#') atNode++;
		parser->command->node = svg_find_node(parser, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		gf_node_register(parser->command->node, NULL);

		/*switch to V2*/
		svg_lsr_set_v2(parser);

		return GF_OK;
	case GF_SG_LSR_SEND_EVENT:
		for (i=0; i<nb_attributes; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!strcmp(att->name, "ref")) atNode = att->value;
			else if (!strcmp(att->name, "event")) atEvent = att->value;
			else if (!strcmp(att->name, "pointvalue")) atPoint = att->value;
			else if (!strcmp(att->name, "intvalue")) atInteger = att->value;
			else if (!strcmp(att->name, "stringvalue")) atString = att->value;
		}

		if (!atEvent) return svg_report(parser, GF_BAD_PARAM, "Missing event name for command");
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		/*should be a XML IDREF, not an XML IRI*/
		if (atNode[0]=='#') atNode++;
		parser->command->node = svg_find_node(parser, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		gf_node_register(parser->command->node, NULL);

		parser->command->send_event_name = gf_dom_event_type_by_name(atEvent);

		parser->command->send_event_integer = 0xEFFFFFFF;
		if (atString) {
			u32 key = gf_dom_get_key_type(atString);
			if (key == GF_KEY_UNIDENTIFIED) {
				parser->command->send_event_string = strdup(atString);
			} else {
				parser->command->send_event_integer = key;
			}
		}
		if (atInteger) parser->command->send_event_integer = atoi(atInteger);
		if (atPoint) {
			SVG_Point pt;
			svg_parse_point(&pt, atPoint);
			parser->command->send_event_x = FIX2INT(pt.x);
			parser->command->send_event_y = FIX2INT(pt.y);
		}
		return GF_OK;
	default:
		return GF_NOT_SUPPORTED;
	}
}


static u32 lsr_get_command_by_name(const char *name)
{
	if (!strcmp(name, "NewScene")) return GF_SG_LSR_NEW_SCENE;
	else if (!strcmp(name, "RefreshScene")) return GF_SG_LSR_REFRESH_SCENE;
	else if (!strcmp(name, "Add")) return GF_SG_LSR_ADD;
	else if (!strcmp(name, "Clean")) return GF_SG_LSR_CLEAN;
	else if (!strcmp(name, "Replace")) return GF_SG_LSR_REPLACE;
	else if (!strcmp(name, "Delete")) return GF_SG_LSR_DELETE;
	else if (!strcmp(name, "Insert")) return GF_SG_LSR_INSERT;
	else if (!strcmp(name, "Restore")) return GF_SG_LSR_RESTORE;
	else if (!strcmp(name, "Save")) return GF_SG_LSR_SAVE;
	else if (!strcmp(name, "SendEvent")) return GF_SG_LSR_SEND_EVENT;
	else if (!strcmp(name, "Activate")) return GF_SG_LSR_ACTIVATE;
	else if (!strcmp(name, "Deactivate")) return GF_SG_LSR_DEACTIVATE;
	return GF_SG_UNDEFINED;
}

static GF_ESD *lsr_parse_header(GF_SVG_Parser *parser, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_ESD *esd;
	u32 i;
	if (!strcmp(name, "LASeRHeader")) {
		GF_LASERConfig *lsrc = (GF_LASERConfig *) gf_odf_desc_new(GF_ODF_LASER_CFG_TAG);
		for (i=0; i<nb_attributes;i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
			if (!strcmp(att->name, "profile")) lsrc->profile = !strcmp(att->value, "full") ? 1 : 0;
			else if (!strcmp(att->name, "level")) lsrc->level = atoi(att->value);
			else if (!strcmp(att->name, "resolution")) lsrc->resolution = atoi(att->value);
			else if (!strcmp(att->name, "timeResolution")) lsrc->time_resolution = atoi(att->value);
			else if (!strcmp(att->name, "coordBits")) lsrc->coord_bits = atoi(att->value);
			else if (!strcmp(att->name, "scaleBits_minus_coordBits")) lsrc->scale_bits_minus_coord_bits = atoi(att->value);
			else if (!strcmp(att->name, "colorComponentBits")) lsrc->colorComponentBits = atoi(att->value);
			else if (!strcmp(att->name, "newSceneIndicator")) lsrc->newSceneIndicator = (!strcmp(att->value, "yes") || !strcmp(att->value, "true")) ? 1 : 0;
			else if (!strcmp(att->name, "useFullRequestHost")) lsrc->fullRequestHost = (!strcmp(att->value, "yes") || !strcmp(att->value, "true")) ? 1 : 0;
			else if (!strcmp(att->name, "pathComponents")) lsrc->fullRequestHost = atoi(att->value);
			else if (!strcmp(att->name, "extensionIDBits")) lsrc->extensionIDBits = atoi(att->value);
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

static void svg_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *stack, *parent;
	SVG_Element *elt;
	SVG_Element *cond = NULL;

#ifdef SKIP_ALL
	return;
#endif

	parent = (SVG_NodeStack *)gf_list_last(parser->node_stack);

	/*switch to conditional*/
	if (parent && parent->node->sgprivate->tag==TAG_SVG_conditional) {
		cond = parent->node;
		parent = NULL;
	}

	/*saf setup*/
	if ((!parent && (parser->load->type==GF_SM_LOAD_XSR)) || cond) {
		u32 com_type;
		/*nothing to do, the context is already created*/
		if (!strcmp(name, "SAFSession")) return;
		/*nothing to do, wait for the laser (or other) header before creating stream)*/
		if (!strcmp(name, "sceneHeader")) return;
		/*nothing to do, wait for the laser (or other) header before creating stream)*/
		if (!strcmp(name, "LASeRHeader")) {
			GF_ESD *esd = lsr_parse_header(parser, name, name_space, attributes, nb_attributes);
			if (!esd) svg_report(parser, GF_BAD_PARAM, "Invalid LASER Header");
			/*TODO find a better way of assigning an ID to the laser stream...*/
			esd->ESID = 1;
			parser->laser_es = gf_sm_stream_new(parser->load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
			if (!parser->load->ctx->root_od) parser->load->ctx->root_od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
			gf_list_add(parser->load->ctx->root_od->ESDescriptors, esd);
			parser->laser_es->timeScale = esd->slConfig->timestampResolution;
			return;
		}
		if (!strcmp(name, "sceneUnit") ) {
			u32 time, rap, i;
			time = rap = 0;
			if (!gf_list_count(parser->laser_es->AUs)) rap = 1;
			for (i=0; i<nb_attributes;i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
			}
			/*create new laser unit*/
			parser->laser_au = gf_sm_stream_au_new(parser->laser_es, time, 0, rap);
			return;			
		}

		if (!strcmp(name, "StreamHeader") || !strcmp(name, "RemoteStreamHeader")
			/*SAF & SAFML are just a pain ...*/
			|| !strcmp(name, "mediaHeader") || !strcmp(name, "imageHeader")
		) {
			char *url = NULL;
			char *src = NULL;
			const char *ID = NULL;
			u32 time, OTI, ST, i, ts_res;
			GF_ODUpdate *odU;
			GF_ObjectDescriptor *od;
			Bool rap;
			SVG_SAFExternalStream*st;
			/*create a SAF stream*/
			if (!parser->saf_es) {
				GF_ESD *esd = (GF_ESD*)gf_odf_desc_esd_new(2);
				esd->ESID = 0xFFFE;
				esd->decoderConfig->streamType = GF_STREAM_OD;
				esd->decoderConfig->objectTypeIndication = 1;
				parser->saf_es = gf_sm_stream_new(parser->load->ctx, esd->ESID, GF_STREAM_OD, 1);
				if (!parser->load->ctx->root_od) parser->load->ctx->root_od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
				parser->saf_es->timeScale = 1000;
				gf_list_add(parser->load->ctx->root_od->ESDescriptors, esd);
			}
			time = 0;
			rap = 0;
			ts_res = 1000;
			OTI = ST = 0;
			for (i=0; i<nb_attributes;i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "url")) url = strdup(att->value); 
				else if (!strcmp(att->name, "streamID")) ID = att->value;
				else if (!strcmp(att->name, "objectTypeIndication")) OTI = atoi(att->value);
				else if (!strcmp(att->name, "streamType")) ST = atoi(att->value);
				else if (!strcmp(att->name, "timeStampResolution")) ts_res = atoi(att->value);
				else if (!strcmp(att->name, "source")) src = att->value;

			}
			if (!strcmp(name, "imageHeader")) ST = 4;

			/*create new SAF stream*/
			st = svg_saf_get_next_available_stream(parser);
			st->stream_name = strdup(ID);

			/*create new SAF unit*/
			parser->saf_au = gf_sm_stream_au_new(parser->saf_es, time, 0, 0);
			odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			gf_list_add(parser->saf_au->commands, odU);
			od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			gf_list_add(odU->objectDescriptors, od);
			od->objectDescriptorID = st->id;

			if (url) {
				od->URLString = url;
			} else {
				char szName[1024];
				GF_MuxInfo *mux;
				FILE *nhml;
				GF_ESD *esd = (GF_ESD*)gf_odf_desc_esd_new(2);
				gf_list_add(od->ESDescriptors, esd);
				esd->decoderConfig->objectTypeIndication = OTI;
				esd->decoderConfig->streamType = ST;
				esd->ESID = st->id;

				mux = (GF_MuxInfo *)gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
				gf_list_add(esd->extensionDescriptors, mux);

				/*global source for stream, don't use nhml dumping*/
				if (src) {
					mux->file_name = strdup(src);
					st->nhml_info = NULL;
				} else {
					if (parser->load->localPath) {
						strcpy(szName, parser->load->localPath);
						strcat(szName, ID);
					} else {
						strcpy(szName, ID);
					}
					strcat(szName, "_temp.nhml");
					mux->file_name = strdup(szName);
					st->nhml_info = mux->file_name;
					nhml = fopen(st->nhml_info, "wt");
					fprintf(nhml, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
					fprintf(nhml, "<NHNTStream version=\"1.0\" timeScale=\"%d\" streamType=\"%d\" objectTypeIndication=\"%d\" inRootOD=\"no\" trackID=\"%d\">\n", ts_res, ST, OTI, st->id);
					fclose(nhml);
					mux->delete_file = 1;
				}
			}
			return;
		}
		if (!strcmp(name, "mediaUnit") || !strcmp(name, "imageUnit") ) {
			FILE *nhml;
			char *id = NULL;
			char *src = NULL;
			SVG_SAFExternalStream*st;
			u32 i, rap, time, offset, length;
			rap = time = offset = length = 0;
			for (i=0; i<nb_attributes;i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "source")) src = att->value;
				else if (!strcmp(att->name, "ref")) id = att->value;
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "startOffset")) offset = atoi(att->value);
				else if (!strcmp(att->name, "length")) length = atoi(att->value);
			}
			st = svg_saf_get_stream(parser, 0, id);
			if (!st || !st->nhml_info) {
				return;
			}
			nhml = fopen(st->nhml_info, "a+t");
			fprintf(nhml, "<NHNTSample ");
			if (time) fprintf(nhml, "DTS=\"%d\" ", time);
			if (length) fprintf(nhml, "dataLength=\"%d\" ", length);
			if (offset) fprintf(nhml, "mediaOffset=\"%d\" ", offset);
			if (rap) fprintf(nhml, "isRAP=\"yes\" ");
			if (src) fprintf(nhml, "mediaFile=\"%s\" ", src);
			fprintf(nhml, "/>\n");
			fclose(nhml);
			return;
		}
		if (!strcmp(name, "endOfStream") ) {
			FILE *nhml;
			char *id = NULL;
			u32 i;
			SVG_SAFExternalStream*st;
			for (i=0; i<nb_attributes;i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "ref")) id = att->value;
			}
			st = svg_saf_get_stream(parser, 0, id);
			if (!st || !st->nhml_info) {
				return;
			}
			nhml = fopen(st->nhml_info, "a+t");
			fprintf(nhml, "</NHNTStream>\n");
			fclose(nhml);
			return;
		}

		if (!strcmp(name, "endOfSAFSession") ) {
			return;
		}
		if (!parser->laser_au && !cond) {
			svg_report(parser, GF_BAD_PARAM, "LASeR Scene unit not defined for command %s", name);
			return;
		}
		/*command parsing*/
		com_type = lsr_get_command_by_name(name);
		if (com_type != GF_SG_UNDEFINED) {
			SVG_NodeStack *top;
			GF_Err e;
			parser->command = gf_sg_command_new(parser->load->scene_graph, com_type);
			if (cond) {
				GF_DOMUpdates *up = cond->children ? (GF_DOMUpdates *)cond->children->node : NULL;
				if (!up) {
					up = gf_dom_add_updates_node((GF_Node*)cond);

					if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
						gf_node_set_callback_function((GF_Node*)up, xsr_exec_command_list);
					}
				}
				gf_list_add(up->updates, parser->command);
			} else {
				gf_list_add(parser->laser_au->commands, parser->command);
			}
			/*this is likely a conditional start - update unknown depth level*/	
			top = (SVG_NodeStack*)gf_list_last(parser->node_stack);
			if (top) {
				top->unknown_depth ++;
				parser->command_depth++;
			}

			e = lsr_parse_command(parser, attributes, nb_attributes);
			if (e!= GF_OK) parser->command->node = NULL;

			return;
		}
	}

	if (!parent && !parser->command && (parser->load->flags & GF_SM_LOAD_CONTEXT_DIMS)) {
		gf_sg_reset(parser->load->scene_graph);
		parser->has_root = 0;
	} 

	if (parser->has_root) {
		assert(parent || parser->command);
	}

	elt = svg_parse_element(parser, name, name_space, attributes, nb_attributes, parent);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		else if (cond) parser->command_depth++;
		svg_report(parser, GF_OK, "Ignoring unknown element %s", name);
		return;
	}
	GF_SAFEALLOC(stack, SVG_NodeStack);
	stack->node = elt;
	gf_list_add(parser->node_stack, stack);

	if ( (gf_node_get_tag((GF_Node *)elt) == TAG_SVG_svg) && 
		(!parser->has_root || (parser->command && !parser->command->node) )
	) {
		
		if (!parser->has_root) svg_init_root_element(parser, elt);
		if (parser->command) parser->command->node = (GF_Node*)elt;
	} else if (!parent && parser->has_root && parser->command) {
		GF_CommandField *field = (GF_CommandField *)gf_list_get(parser->command->command_fields, 0);
		if (field) {
			/*either not assigned or textContent*/
			if (field->new_node && (field->new_node->sgprivate->tag==TAG_DOMText)) {
				svg_report(parser, GF_OK, "Warning: LASeR cannot replace children with a mix of text nodes and elements - ignoring text\n");
				gf_node_unregister(field->new_node, NULL);
				field->new_node = NULL;
			}
			if (field->new_node) {
				field->field_ptr = &field->node_list;
				gf_node_list_add_child(& field->node_list, field->new_node);
				field->new_node = NULL;
				gf_node_list_add_child( & field->node_list, (GF_Node*) elt);
			} else if (field->node_list) {
				gf_node_list_add_child(& field->node_list, (GF_Node*) elt);
			} else if (!field->new_node) {
				field->new_node = (GF_Node*)elt;
				field->field_ptr = &field->new_node;
			}
		} else {
			assert(parser->command->tag==GF_SG_LSR_NEW_SCENE);
			assert(gf_node_get_tag((GF_Node *)elt) == TAG_SVG_svg);
			if(!parser->command->node) 
				parser->command->node = (GF_Node *)elt;
		}
	} else if (!parser->has_root) {
		gf_list_del_item(parser->node_stack, stack);
		free(stack);
		gf_node_unregister((GF_Node *)elt, NULL);
	}
}

static void svg_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *top = (SVG_NodeStack *)gf_list_last(parser->node_stack);

#ifdef SKIP_ALL
	return;
#endif

	if (!top) {
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
		/*error*/
		return;
	}

	/*only remove created nodes ... */
	if (gf_svg_get_element_tag(name) != TAG_UndefinedNode) {
		const char *the_name;
		SVG_Element *node = top->node;
		/*check node name...*/
		the_name = gf_node_get_class_name((GF_Node *)node);
		if (strcmp(the_name, name)) {
			if (top->unknown_depth) {
				top->unknown_depth--;
				return;
			} else {
				svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
				return;
			}
		}
		free(top);
		gf_list_rem_last(parser->node_stack);

		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = GF_EVENT_LOAD;
			gf_dom_event_fire((GF_Node*)node, NULL, &evt);

			switch (node->sgprivate->tag) {
			case TAG_SVG_animateMotion:
				/* 
				try to init animateMotion once all children have been parsed 
				to make sure we get the mpath child if any, however, we are still not sure 
				if the target is known. We can force initialisation 
				because mpath children (if any have been parsed) 
				*/
				{
				   u32 i, count;
				   SVG_DeferedAnimation *anim = NULL;
				   count = gf_list_count(parser->defered_animations);
				   for (i = 0; i < count; i++) {
					   anim = gf_list_get(parser->defered_animations, i);
					   if (anim->animation_elt == node) break;
					   else anim = NULL;
				   }
				   if (anim) {
					   if (svg_parse_animation(parser, gf_node_get_graph((GF_Node *)node), anim, NULL, 1)) {
						   svg_delete_defered_anim(anim, parser->defered_animations);
					   }
				   }
				}
				break;
			case TAG_SVG_script:
			case TAG_SVG_handler:
			/*init script once text script is loaded*/
				gf_node_init((GF_Node *)node);
				break;
			}
		}
	} else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
			if (!top->unknown_depth) parser->command_depth --;
		} else if (parser->command_depth) {
			parser->command_depth--;
		} else {
			svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
		}
	}
}

static void svg_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *top = (SVG_NodeStack *)gf_list_last(parser->node_stack);
	SVG_Element *elt = (top ? top->node : NULL);
	GF_DOMText *text;
	u32 skip_text;
	u32 tag;

#ifdef SKIP_ALL
	return;
#endif

	if (top && top->unknown_depth && !parser->command_depth) return;
	if (!elt && !parser->command) return;

	tag = (elt ? gf_node_get_tag((GF_Node *)elt) : 0);

	/*if top is a conditional, create a new text node*/
	if (!elt || tag == TAG_SVG_conditional) {
		GF_CommandField *field = (GF_CommandField *)gf_list_get(parser->command->command_fields, 0);
		if (parser->command->tag == GF_SG_LSR_NEW_SCENE || parser->command->tag == GF_SG_LSR_ADD) return;
		
		assert(field);

		if (field->new_node) {
			svg_report(parser, GF_OK, "Warning: LASeR cannot replace children with a mix of text nodes and elements - ignoring text\n");
			return;
		}

		/*create a text node but don't register it with any node*/
		text = gf_dom_new_text_node(parser->load->scene_graph);
		gf_node_register((GF_Node *)text, NULL);
		text->textContent = strdup(text_content);

		if (field->new_node) {
			field->field_ptr = &field->node_list;
			gf_node_list_add_child(& field->node_list, field->new_node);
			field->new_node = NULL;
			gf_node_list_add_child( & field->node_list, (GF_Node*) text);
		} else if (field->node_list) {
			gf_node_list_add_child(& field->node_list, (GF_Node*) text);
		} else if (!field->new_node) {
			field->new_node = (GF_Node*)text;
			field->field_ptr = &field->new_node;
		}
		return;
	}
	skip_text = 1;
	switch (tag) {
	case TAG_SVG_a:
	case TAG_SVG_text:
	case TAG_SVG_tspan:
	case TAG_SVG_textArea:
		skip_text = 0;
		break;
	/*for script and handlers only add text if not empty*/
	case TAG_SVG_handler:
	case TAG_SVG_script:
	{
		u32 i, len = strlen(text_content);
		for (i=0; i<len; i++) {
			if (!strchr(" \n\r\t", text_content[i])) {
				skip_text = 0;
				break;
			}
		}
	}
		break;
	}

	if (!skip_text) {
		text = gf_dom_add_text_node((GF_Node *)elt, strdup(text_content));
		text->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
		gf_node_changed((GF_Node *)text, NULL);
	}
}

static GF_SVG_Parser *svg_new_parser(GF_SceneLoader *load)
{
	GF_SVG_Parser *parser;
	if (load->type==GF_SM_LOAD_XSR) {
		if (!load->ctx) return NULL;
	} else if (load->type!=GF_SM_LOAD_SVG_DA) return NULL;

	GF_SAFEALLOC(parser, GF_SVG_Parser);
	parser->node_stack = gf_list_new();
	parser->defered_hrefs = gf_list_new();
	parser->defered_animations = gf_list_new();
	parser->defered_listeners = gf_list_new();
	parser->peeked_nodes = gf_list_new();

	parser->sax_parser = gf_xml_sax_new(svg_node_start, svg_node_end, svg_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;
	if (load->ctx) load->ctx->is_pixel_metrics = 1;

	return parser;
}

static void svg_flush_animations(GF_SVG_Parser *parser)
{
	while (gf_list_count(parser->defered_animations)) {
		SVG_DeferedAnimation *anim = (SVG_DeferedAnimation *)gf_list_get(parser->defered_animations, 0);
		svg_parse_animation(parser, parser->load->scene_graph, anim, NULL, 2);
		svg_delete_defered_anim(anim, parser->defered_animations);
	}
}

GF_Err gf_sm_load_init_svg(GF_SceneLoader *load)
{
	GF_Err e;
	GF_SVG_Parser *parser;
	u32 in_time;

	if (!load->fileName) return GF_BAD_PARAM;
	parser = svg_new_parser(load);


	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ( (load->type==GF_SM_LOAD_XSR) ? "[Parser] MPEG-4 (LASER) Scene Parsing\n" : "[Parser] SVG Scene Parsing\n"));
	
	in_time = gf_sys_clock();
	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, svg_progress);
	if (e<0) return svg_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );
	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[SVG Parser] Scene parsed and Scene Graph built in %d ms\n", gf_sys_clock() - in_time));

	svg_flush_animations(parser);

	return parser->last_error;
}

GF_Err gf_sm_load_init_svg_string(GF_SceneLoader *load, char *str_data)
{
	GF_Err e;
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;

	if (!parser) {
		char BOM[6];
		BOM[0] = str_data[0];
		BOM[1] = str_data[1];
		BOM[2] = str_data[2];
		BOM[3] = str_data[3];
		BOM[4] = BOM[5] = 0;
		parser = svg_new_parser(load);
		e = gf_xml_sax_init(parser->sax_parser, (unsigned char*)BOM);
		if (e) {
			svg_report(parser, e, "Error initializing SAX parser: %s", gf_xml_sax_get_error(parser->sax_parser) );
			return e;
		}
		str_data += 4;
	}
	return gf_xml_sax_parse(parser->sax_parser, str_data);
}


GF_Err gf_sm_load_run_svg(GF_SceneLoader *load)
{
	return GF_OK;
}

GF_Err gf_sm_load_done_svg(GF_SceneLoader *load)
{
	SVG_SAFExternalStream *st;
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;
	if (!parser) return GF_OK;
	while (gf_list_count(parser->node_stack)) {
		SVG_NodeStack *st = (SVG_NodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		free(st);
	}
	gf_list_del(parser->node_stack);
	gf_list_del(parser->defered_hrefs);
	/*FIXME - if there still are som defered listeners, we should pass them to the scene graph
	and wait for the parent to be defined*/
	gf_list_del(parser->defered_listeners);
	gf_list_del(parser->peeked_nodes);
	/* reset animations */
	gf_list_del(parser->defered_animations);
	if (parser->sax_parser) {
		gf_xml_sax_del(parser->sax_parser);
	}
	st = parser->streams;
	while (st) {
		SVG_SAFExternalStream *next = st->next;
		free(st->stream_name);
		free(st);
		st = next;
	}
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}

#endif

