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
#include <gpac/internal/bifs_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

typedef struct
{
	GF_SceneLoader *load;
	GF_Err last_error;
	GF_SAXParser *sax_parser;
	Bool has_root;
	u32 command_depth;
	
	/* stack of SVG nodes*/
	GF_List *node_stack;

	GF_List *defered_hrefs;
	GF_List *defered_animations;
	GF_List *defered_listeners;

	/*LASeR parsing*/
	GF_StreamContext *laser_es;
	GF_AUContext *laser_au;
	GF_Command *command;
	/*SAF AU maps to OD AU and is used for each new media declaration*/
	GF_AUContext *saf_au;
	GF_StreamContext *saf_es;
} GF_SVGParser;

typedef struct
{
	/*top of parsed sub-tree*/
	GF_Node *node;
	/*depth of unknown elements being skipped*/
	u32 unknown_depth;
} SVGNodeStack;


typedef struct {
	/* Stage of the resolving:
	    0: resolving attributes which depends on the target: from, to, by, values, type
		1: resolving begin times
		2: resolving end times */
	u32 resolve_stage;
	/* Animation element being defered */
	SVGElement *animation_elt;
	/* anim parent*/
	SVGElement *anim_parent;
	/* target animated element*/
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
#ifndef GPAC_DISABLE_LOG
	if (gf_log_get_level() && (gf_log_get_tools() & GF_LOG_PARSER)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsprintf(szMsg, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[SVG Parsing] %s (line %d)\n", szMsg, gf_xml_sax_get_line(parser->sax_parser)) );
	}
#endif
	if (e) parser->last_error = e;
	return e;
}
static void svg_progress(void *cbk, u32 done, u32 total)
{
	gf_set_progress("SVG Parsing", done, total);
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
		DeferedAnimation *anim = (DeferedAnimation *)gf_list_last(l);
		if (!anim) break;
		svg_delete_defered_anim(anim, l);
	}
}

static void svg_post_process_href(GF_SVGParser *parser, SVG_IRI *iri)
{
	/*keep data when encoding*/
	if ( !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	/*unresolved, queue it...*/
	if ((iri->type==SVG_IRI_ELEMENTID) && !iri->target && iri->iri) {
		gf_list_add(parser->defered_hrefs, iri);
	}
	if (iri->type != SVG_IRI_IRI) return;
	gf_svg_store_embedded_data(iri, parser->load->localPath, parser->load->fileName);
}

static Bool svg_resolve_smil_times(GF_SVGParser *parser, GF_SceneGraph *sg, SVGElement *anim_parent, SVGElement *anim, GF_List *smil_times, Bool is_end, const char *node_name)
{
	u32 i, done, count = gf_list_count(smil_times);
	done = 0;
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time *)gf_list_get(smil_times, i);
		/*not an event*/
		if (t->type != GF_SMIL_TIME_EVENT) {
			done++;
			continue;
		}
		if (!t->element_id) {
			/*event source is anim parent*/
			if (!t->element) t->element = (GF_Node *)anim_parent;
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
	if (done!=count) return 0;
	return 1;
}

static Bool svg_parse_animation(GF_SVGParser *parser, GF_SceneGraph *sg, DeferedAnimation *anim, const char *nodeID)
{
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0, anim_transform_type = 0;

	if (anim->resolve_stage==0) {
		if (!anim->target) anim->target = (SVGElement *) gf_sg_find_node_by_name(sg, anim->target_id + 1);
		if (!anim->target) return 0;

		/*setup IRI ptr*/
		anim->animation_elt->xlink->href.type = SVG_IRI_ELEMENTID;
		anim->animation_elt->xlink->href.target = anim->target;
		gf_svg_register_iri(sg, &anim->animation_elt->xlink->href);

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);

		if (anim->attributeName) {
			SMIL_AttributeName *attname_value;
			/* get the type of the target attribute to determine type of the from/to/by ... */
			if (gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "attributeName", &info) != GF_OK) return 1;
			attname_value = (SMIL_AttributeName *)info.far_ptr;

			if (gf_node_get_field_by_name((GF_Node *)anim->target, anim->attributeName, &info) == GF_OK) {
				attname_value->type = info.fieldType;
				attname_value->field_ptr = info.far_ptr;
				attname_value->name = anim->attributeName;
			} else {
				return 1;
			}
			anim_value_type = attname_value->type;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else if (tag == TAG_SVG_discard) {
				anim->resolve_stage = 1;
				return svg_parse_animation(parser, sg, anim, nodeID);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] missing attributeName attribute on %s\n", anim->animation_elt->sgprivate->NodeName));
			}
		}

		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			/* determine the transform_type in case of animateTransform attribute */
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "type", &info);
			gf_svg_parse_attribute(anim->animation_elt, &info, anim->type, 0, 0);
			anim_transform_type = *(SVG_TransformType *) info.far_ptr;
			anim_value_type = SVG_Matrix_datatype;
		}

		/* Parsing of to / from / by / values */
		if (anim->to) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "to", &info);
			gf_svg_parse_attribute(anim->animation_elt, &info, anim->to, anim_value_type, anim_transform_type);
			if (anim_value_type==SVG_IRI_datatype) svg_post_process_href(parser, (SVG_IRI*)anim->animation_elt->anim->to.value);
		} 
		if (anim->from) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "from", &info);
			gf_svg_parse_attribute(anim->animation_elt, &info, anim->from, anim_value_type, anim_transform_type);
			if (anim_value_type==SVG_IRI_datatype) svg_post_process_href(parser, (SVG_IRI*)anim->animation_elt->anim->from.value);
		} 
		if (anim->by) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "by", &info);
			gf_svg_parse_attribute(anim->animation_elt, &info, anim->by, anim_value_type, anim_transform_type);
			if (anim_value_type==SVG_IRI_datatype) svg_post_process_href(parser, (SVG_IRI*)anim->animation_elt->anim->by.value);
		} 
		if (anim->values) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "values", &info);
			gf_svg_parse_attribute(anim->animation_elt, &info, anim->values, anim_value_type, anim_transform_type);
			if (anim_value_type==SVG_IRI_datatype) {
				u32 i, count = gf_list_count(anim->animation_elt->anim->values.values);
				for (i=0; i<count; i++) {
					SVG_IRI *iri = (SVG_IRI *)gf_list_get(anim->animation_elt->anim->values.values, i);
					svg_post_process_href(parser, iri);
				}
			}
		}
		anim->resolve_stage = 1;
	}

	if (anim->resolve_stage == 1) {
		if (svg_resolve_smil_times(parser, sg, anim->anim_parent, anim->animation_elt, anim->animation_elt->timing->begin, 0, nodeID)) {
			anim->resolve_stage = 2;
		} else {
			return 0;
		}
	}
	if (!svg_resolve_smil_times(parser, sg, anim->anim_parent, anim->animation_elt, anim->animation_elt->timing->end, 1, nodeID)) return 0;

	/*animateMotion needs its children to be parsed !! */
	if (!nodeID && gf_node_get_tag((GF_Node *)anim->animation_elt) == TAG_SVG_animateMotion)
		return 1;

	/*OK init the anim*/
	gf_node_init((GF_Node *)anim->animation_elt);
	return 1;
}

static void svg_resolved_refs(GF_SVGParser *parser, GF_SceneGraph *sg, const char *nodeID)
{
	u32 count, i;

	/*check unresolved HREF*/
	count = gf_list_count(parser->defered_hrefs);
	for (i=0; i<count; i++) {
		GF_Node *targ;
		SVG_IRI *iri = (SVG_IRI *)gf_list_get(parser->defered_hrefs, i);
		if (nodeID && strcmp(iri->iri + 1, nodeID)) continue;
		targ = gf_sg_find_node_by_name(sg, iri->iri + 1);
		if (targ) {
			iri->type = SVG_IRI_ELEMENTID;
			iri->target = (SVGElement *) targ;
			gf_svg_register_iri(sg, iri);
			free(iri->iri);
			iri->iri = NULL;
			gf_list_rem(parser->defered_hrefs, i);
			i--;
			count--;
		}
	}

	/*check unresolved listeners*/
	count = gf_list_count(parser->defered_listeners);
	for (i=0; i<count; i++) {
		GF_Node *par;
		SVGlistenerElement *listener = (SVGlistenerElement *)gf_list_get(parser->defered_listeners, i);

		par = NULL;
		if (listener->observer.type == SVG_IRI_ELEMENTID) {
			if (!listener->observer.target) continue;
			else par = (GF_Node*)listener->observer.target;
		}
		if (listener->target.type == SVG_IRI_ELEMENTID) {
			if (!listener->target.target) continue;
			else {
				if (!par) par = (GF_Node*)listener->target.target;
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
		DeferedAnimation *anim = (DeferedAnimation *)gf_list_get(parser->defered_animations, i);
		/*resolve it - we don't check the name since it may be used in SMIL times, which we don't track at anim level*/
		if (svg_parse_animation(parser, sg, anim, nodeID)) {
			svg_delete_defered_anim(anim, parser->defered_animations);
			i--;
			count--;
		}
	}
}

static SVGElement *svg_parse_element(GF_SVGParser *parser, const char *name, const char *name_space, GF_List *attrs, SVGElement *parent)
{
	u32	tag, i, count;
	SVGElement *elt;
	GF_FieldInfo info;
	Bool ided = 0;
	DeferedAnimation *anim = NULL;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_node_svg_type_by_class_name(name);
	if (tag == TAG_UndefinedNode) {
		svg_report(parser, GF_OK, "Unknown element %s - skipping", name);
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = (SVGElement*)gf_node_new(parser->load->scene_graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (GF_Node *)parent);
	if (parent && elt) gf_list_add(parent->children, elt);

	if (is_svg_animation_tag(tag)) {
		GF_SAFEALLOC(anim, DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		anim->target = parent;
		anim->anim_parent = parent;
	} else if (tag == TAG_SVG_video || tag == TAG_SVG_audio || tag == TAG_SVG_animation || tag == TAG_SVG_conditional) {
		/* warning: we use the DeferedAnimation structure for some timing nodes which are not 
		   animations, but we put the parse stage at 1 (timing) see svg_parse_animation. */
		GF_SAFEALLOC(anim, DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		anim->anim_parent = parent;
		anim->resolve_stage = 1;
	}

	/*parse all att*/
	count = gf_list_count(attrs);
	for (i=0; i<count; i++) {

		GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attrs, i);
		if (!att->value || !strlen(att->value)) continue;

		if (!stricmp(att->name, "style")) {
			/* Special case: style if present will always be first in the list */
			gf_svg_parse_style(elt, att->value);
		} else if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			gf_svg_parse_element_id(elt, att->value, parser->command_depth ? 1 : 0);
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
		} else if (!stricmp(att->name, "xlink:href") ) {
			if (!elt->xlink) {
				svg_report(parser, GF_OK, "Warning: xlink:href on element %s ignored\n", name); 
			} else if (is_svg_animation_tag(tag)) {
				assert(anim);
				anim->target_id = att->value;
				att->value = NULL;
				/*may be NULL*/
				anim->target = (SVGElement *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
			} else {
				SVG_IRI *iri = & elt->xlink->href;
				memset(&info, 0, sizeof(GF_FieldInfo));
				info.far_ptr = & elt->xlink->href;
				info.fieldType = SVG_IRI_datatype;
				info.name = "xlink:href";
				gf_svg_parse_attribute(elt, &info, att->value, 0, 0);

				svg_post_process_href(parser, iri);
			}
		} else if (!strnicmp(att->name, "xmlns", 5)) {
		} else if (!stricmp(att->name, "ev:event") &&  tag == TAG_SVG_handler) {
			/* When the handler element specifies the event attribute, an implicit listener is defined */
			GF_Node *node = (GF_Node *)elt;
			SVGlistenerElement *listener;
			listener = (SVGlistenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
			gf_node_register((GF_Node *)listener, node);
			gf_list_add( ((GF_ParentNode *)node)->children, listener);
			/* this listener listens to the given type of event */
			((SVGhandlerElement *)node)->ev_event.type = listener->event.type = gf_dom_event_type_by_name(att->value);
			listener->handler.target = (SVGElement *)node;
			/* this listener listens with the parent of the handler as the event target */
			listener->target.target = parent;
			gf_dom_listener_add((GF_Node *) parent, (GF_Node *) listener);
		} else {
			u32 evtType = GF_EVENT_UNKNOWN;
			if (!strncmp(att->name, "on", 2)) evtType = gf_dom_event_type_by_name(att->name + 2);

			/*SVG 1.1 events: create a listener and a handler on the fly, register them with current node
			and add listener struct*/
			if (evtType != GF_EVENT_UNKNOWN) {
				XMLEV_Event evt;
				SVGhandlerElement *handler;
				evt.type = evtType;
				evt.parameter = 0;
				handler = gf_dom_listener_build((GF_Node *) elt, evt);
				handler->textContent = att->value;
				att->value = NULL;
				gf_node_init((GF_Node *)handler);
			} else if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
				gf_svg_parse_attribute(elt, &info, att->value, 0, 0);
				if (info.fieldType== SVG_IRI_datatype) {
					SVG_IRI *iri = (SVG_IRI *)info.far_ptr;
					if ((iri->type==SVG_IRI_ELEMENTID) && !iri->target && iri->iri)
						gf_list_add(parser->defered_hrefs, iri);
				}
			}
			/*LASeR HACKS*/
			else if (!strcmp(att->name, "lsr:translation")) {
				if (gf_node_get_field_by_name((GF_Node *)elt, "transform", &info)==GF_OK) {
					Float tx, ty;
					sscanf(att->value, "%f %f", &tx, &ty);
					gf_mx2d_add_translation((GF_Matrix2D*)info.far_ptr, FLT2FIX(tx), FLT2FIX(ty));
				}
			}
			else {
				svg_report(parser, GF_OK, "Unknown attribute %s on element %s - skipping\n", (char *)att->name, gf_node_get_class_name((GF_Node *)elt));
			}
		}
	}

	if (anim) {
		if (svg_parse_animation(parser, parser->load->scene_graph, anim, NULL)) {
			svg_delete_defered_anim(anim, NULL);
		} else {
			gf_list_add(parser->defered_animations, anim);
		}
	}

	/* if the new element has an id, we try to resolve defered references */
	if (ided) {
		svg_resolved_refs(parser, parser->load->scene_graph, gf_node_get_name((GF_Node *)elt));
	}

	/* We need to init the node at the end of the parsing, after parsing all attributes */
	/* text nodes must be initialized at the end of the <text> element */
	if (!anim && elt) gf_node_init((GF_Node *)elt);

	/*register listener element*/
	if (elt && (tag==TAG_SVG_listener)) {
		Bool post_pone = 0;
		SVGElement *par = NULL;
		SVGlistenerElement *listener = (SVGlistenerElement *)elt;
		if (listener->observer.type == SVG_IRI_ELEMENTID) {
			if (!listener->observer.target) post_pone = 1;
			else par = listener->observer.target;
		}
		if (!par && (listener->target.type == SVG_IRI_ELEMENTID)) {
			if (!listener->target.target) post_pone = 1;
			else par = listener->target.target;
		}

		if (!listener->handler.target) listener->handler.target = parent;
		
		if (post_pone) {
			gf_list_add(parser->defered_listeners, listener);
		} else {
			if (!par) par = parent;
			gf_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
		}
	}
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
			GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attrs, i);
			if (!strcmp(att->name, "profile")) lsrc->profile = !strcmp(att->value, "full") ? 1 : 0;
			else if (!strcmp(att->name, "level")) lsrc->level = atoi(att->value);
			else if (!strcmp(att->name, "resolution")) lsrc->resolution = atoi(att->value);
			else if (!strcmp(att->name, "timeResolution")) lsrc->time_resolution = atoi(att->value);
			else if (!strcmp(att->name, "coordBits")) lsrc->coord_bits = atoi(att->value);
			else if (!strcmp(att->name, "scaleBits_minus_coordBits")) lsrc->scale_bits_minus_coord_bits = atoi(att->value);
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

static GF_Err lsr_parse_command(GF_SVGParser *parser, GF_List *attr)
{
	GF_FieldInfo info;
	GF_Node *opNode;
	GF_Err e;
	Bool is_replace = 0;
	char *atNode = NULL;
	char *atAtt = NULL;
	char *atOperandNode = NULL;
	char *atOperandAtt = NULL;
	char *atValue = NULL;
	GF_CommandField *field;
	s32 index = -1;
	u32 i, count = gf_list_count(attr);
	switch (parser->command->tag) {
	case GF_SG_LSR_NEW_SCENE:
		return GF_OK;
	case GF_SG_LSR_DELETE:
		for (i=0; i<count; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attr, i);
			if (!strcmp(att->name, "ref")) atNode = att->value;
			else if (!strcmp(att->name, "attributeName")) atAtt = att->value;
			else if (!strcmp(att->name, "index")) index = atoi(att->value);
		}
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		parser->command->node = gf_sg_find_node_by_name(parser->load->scene_graph, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		if (atAtt || (index>=0) ) {
			if (!strcmp(atAtt, "children")) atAtt = NULL;
			if (atAtt && gf_node_get_field_by_name(parser->command->node, atAtt, &info) != GF_OK)
				return svg_report(parser, GF_BAD_PARAM, "Attribute %s does not belong to node %s", atAtt, atNode);

			field = gf_sg_command_field_new(parser->command);
			field->pos = index;
			field->fieldIndex = atAtt ? info.fieldIndex : 0;
			field->fieldType = atAtt ? info.fieldType : 0;
		}
		gf_node_register(parser->command->node, NULL);
		return GF_OK;
	case GF_SG_LSR_REPLACE:
		is_replace = 1;
	case GF_SG_LSR_ADD:
	case GF_SG_LSR_INSERT:
		for (i=0; i<count; i++) {
			GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attr, i);
			if (!strcmp(att->name, "ref")) atNode = att->value;
			else if (!strcmp(att->name, "operandElementId")) atOperandNode = att->value;
			else if (!strcmp(att->name, "operandAttributeName")) atOperandAtt = att->value;
			else if (!strcmp(att->name, "value")) atValue = att->value;
			else if (!strcmp(att->name, "attributeName")) atAtt = att->value;
			/*replace only*/
			else if (!strcmp(att->name, "index")) index = atoi(att->value);
		}
		if (!atNode) return svg_report(parser, GF_BAD_PARAM, "Missing node ref for command");
		parser->command->node = gf_sg_find_node_by_name(parser->load->scene_graph, atNode);
		if (!parser->command->node) return svg_report(parser, GF_BAD_PARAM, "Cannot find node node ref %s for command", atNode);
		/*child or node replacement*/
		if ( (is_replace || (parser->command->tag==GF_SG_LSR_INSERT)) && (!atAtt || !strcmp(atAtt, "children")) ) {
			field = gf_sg_command_field_new(parser->command);
			field->pos = index;
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
				field->field_ptr = gf_svg_create_attribute_value(field->fieldType, 0);
				*(SVG_String *)field->field_ptr = strdup(atValue);
			}
			return GF_OK;
		}
		e = gf_node_get_field_by_name(parser->command->node, atAtt, &info);
		if (e != GF_OK) {
			if (!strcmp(atAtt, "scale") || !strcmp(atAtt, "translation") || !strcmp(atAtt, "rotation")) {
				e = gf_node_get_field_by_name(parser->command->node, "transform", &info);
				if (!e) {
					if (!strcmp(atAtt, "scale")) info.eventType = SVG_TRANSFORM_SCALE;
					else if (!strcmp(atAtt, "translation")) info.eventType = SVG_TRANSFORM_TRANSLATE;
					else if (!strcmp(atAtt, "rotation")) info.eventType = SVG_TRANSFORM_ROTATE;
				}
			}
			if (e) return svg_report(parser, GF_BAD_PARAM, "Attribute %s does not belong to node %s", atAtt, atNode);
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

			field->field_ptr = nf.far_ptr = gf_svg_create_attribute_value(info.fieldType, (u8) info.eventType);
			gf_svg_parse_attribute((SVGElement *)parser->command->node, &nf, atValue, (u8) info.fieldType, (u8) info.eventType);
			if (info.eventType) {
				field->fieldIndex = (u32) -2;
				field->fieldType = info.eventType;
			}
		} else if (opNode) {
			GF_FieldInfo op_field;
			if (gf_node_get_field_by_name(opNode, atOperandAtt, &op_field) != GF_OK)
				return svg_report(parser, GF_BAD_PARAM, "Attribute %s does not belong to node %s", atOperandAtt, atOperandNode);
			parser->command->fromNodeID = opNode->sgprivate->NodeID;
			parser->command->fromFieldIndex = op_field.fieldIndex;
		}
		gf_node_register(parser->command->node, NULL);
		return GF_OK;
	default:
		return GF_OK;
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
	return GF_SG_UNDEFINED;
}
static void svg_node_start(void *sax_cbck, const char *name, const char *name_space, GF_List *attributes)
{
	SVGNodeStack *stack, *parent;
	SVGElement *elt;
	SVGconditionalElement *cond = NULL;
	GF_SVGParser *parser = (GF_SVGParser *)sax_cbck;

	parent = (SVGNodeStack *)gf_list_last(parser->node_stack);
	if (parent && parent->node->sgprivate->tag==TAG_SVG_conditional) {
		cond = (SVGconditionalElement *)parent->node;
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
				GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attributes, i);
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "startOffset")) startOffset = atoi(att->value);
				else if (!strcmp(att->name, "length")) length = atoi(att->value);
				else if (!strcmp(att->name, "source")) { 
					source = att->value; 
					att->value = NULL; 
				}
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
				GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attributes, i);
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
			}
			/*create new laser unit*/
			parser->laser_au = gf_sm_stream_au_new(parser->laser_es, time, 0, rap);
			return;			
		}
		if (!strcmp(name, "StreamHeader") || !strcmp(name, "RemoteStreamHeader")) {
			char *url = NULL;
			u32 time, ID, OTI, ST, count, i;
			GF_ODUpdate *odU;
			GF_ObjectDescriptor *od;
			Bool rap;
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
			count = gf_list_count(attributes);
			for (i=0; i<count;i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *)gf_list_get(attributes, i);
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "url")) { url = att->value; att->value = NULL; } 
				else if (!strcmp(att->name, "streamID")) ID = atoi(att->value);
				else if (!strcmp(att->name, "objectTypeIndication")) OTI = atoi(att->value);
				else if (!strcmp(att->name, "streamType")) ST = atoi(att->value);
			}
			/*create new SAF unit*/
			parser->saf_au = gf_sm_stream_au_new(parser->saf_es, time, 0, 0);
			odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);
			gf_list_add(parser->saf_au->commands, odU);
			od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
			gf_list_add(odU->objectDescriptors, od);
			if (url) od->URLString = url;
			else {
				GF_ESD *esd = (GF_ESD*)gf_odf_desc_esd_new(2);
				gf_list_add(od->ESDescriptors, esd);
			}
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
			SVGNodeStack *top;
			GF_Err e;
			parser->command = gf_sg_command_new(parser->load->scene_graph, com_type);
			if (cond) {
				gf_list_add(cond->updates.com_list, parser->command);
			} else {
				gf_list_add(parser->laser_au->commands, parser->command);
			}
			/*this is likely a conditional start - update unknown depth level*/	
			top = (SVGNodeStack*)gf_list_last(parser->node_stack);
			if (top) {
				top->unknown_depth ++;
				parser->command_depth++;
			}

			e = lsr_parse_command(parser, attributes);
			if (e!= GF_OK) parser->command->node = NULL;

			return;
		}
	}

	if (parser->has_root) {
		assert(parent || parser->command);
	}
	elt = svg_parse_element(parser, name, name_space, attributes, parent ? (SVGElement *) parent->node : NULL);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		return;
	}
	GF_SAFEALLOC(stack, SVGNodeStack);
	stack->node = (GF_Node *) elt;
	gf_list_add(parser->node_stack, stack);

	if (!strcmp(name, "svg") && !parser->has_root) 
		svg_init_root_element(parser, (SVGsvgElement *)elt);
	else if (!parent && parser->has_root) {
		GF_CommandField *field = (GF_CommandField *)gf_list_get(parser->command->command_fields, 0);
		assert(field);
		/*either not assigned or textContent*/
		assert(!field->fieldType || (field->fieldType==SVG_String_datatype));
		if (field->new_node) {
			field->node_list = gf_list_new();
			field->field_ptr = &field->node_list;
			gf_list_add(field->node_list, field->new_node);
			field->new_node = NULL;
			gf_list_add(field->node_list, elt);
		} else if (field->node_list) {
			gf_list_add(field->node_list, elt);
		} else if (!field->new_node) {
			field->new_node = (GF_Node*)elt;
			field->field_ptr = &field->new_node;
		}
	}
}

static void svg_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	GF_SVGParser *parser = (GF_SVGParser *)sax_cbck;
	SVGNodeStack *top = (SVGNodeStack *)gf_list_last(parser->node_stack);

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
	}
	/*only remove created nodes ... */
	if (gf_node_svg_type_by_class_name(name) != TAG_UndefinedNode) {
		const char *the_name;
		GF_Node *node = top->node;
		/*check node name...*/
		the_name = gf_node_get_class_name(node);
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
			gf_dom_event_fire(node, NULL, &evt);

			/*init animateMotion once all children have been parsed tomake sure we get the mpath child if any*/
			if (node->sgprivate->tag == TAG_SVG_animateMotion) {
				gf_node_init(node);
			}
		}
	} else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
			if (!top->unknown_depth) parser->command_depth --;
		} else {
			svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
		}
	}
}

static void svg_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	char *result;
	u32 len;
	Bool space_preserve = 0;
	GF_SVGParser *parser = (GF_SVGParser *)sax_cbck;
	SVGElement *node_core, *node;
	SVGNodeStack *top = (SVGNodeStack *)gf_list_last(parser->node_stack);
	node = top ? (SVGElement *) top->node : NULL;
	if (!node && !parser->command) return;
	if (top && top->unknown_depth) return;

	node_core = node;
	if (!node) node_core = (SVGElement *)parser->command->node;
	if (!node_core) return;

	result = strdup(text_content);
	len = strlen(text_content);

	if (is_cdata) goto skip_xml_space;

	if (node_core->core->space != XML_SPACE_PRESERVE) {
		u32 j, i, state;
		i = j = 0;
		state = 0;
		for (i=0; i<len; i++) {
			switch (text_content[i]) {
			case '\n': 
			case '\r': 
				break;
			case '\t': 
			case ' ':
				if (j && !state) {
					state = 1;
					result[j] = ' ';
					j++;
				}
				break;
			default:
				result[j] = text_content[i];
				j++;
				state = 0;
				break;
			}
		}
		result[j] = 0;
		len = j;
	}
	else {
		u32 j, i;
		i = j = 0;
		space_preserve = 1;
		for (i=0; i<len; i++) {
			switch (text_content[i]) {
			case '\r': 
				break;
			case '\n': 
			case '\t': 
				result[j] = ' ';
				j++;
				break;
			default:
				result[j] = text_content[i];
				j++;
				break;
			}
		}
		result[j] = 0;
		len = j;
	}

skip_xml_space:
	if (!len) {
		free(result);
		return;
	}

	if (!node) {
		GF_CommandField *field = (GF_CommandField *)gf_list_get(parser->command->command_fields, 0);

		free(result);
		return;
		
		
		if (!field) {
			field = gf_sg_command_field_new(parser->command);
			field->pos = -1;
			field->fieldIndex = (u32) -1;
			field->fieldType = SVG_String_datatype;
		} 
		if (field->fieldType != SVG_String_datatype) return;
		if (!field->field_ptr) 
			field->field_ptr = gf_svg_create_attribute_value(field->fieldType, 0);
		if (*(SVG_String *)field->field_ptr) free(*(SVG_String *)field->field_ptr);

		*(SVG_String *)field->field_ptr = result;
		return;
	}

	switch (gf_node_get_tag((GF_Node *)node)) {
	case TAG_SVG_text:
	{
		SVGtextElement *text = (SVGtextElement *)node;
		u32 baselen = 0;
		if (text->textContent) baselen = strlen(text->textContent);
		if (baselen && space_preserve) {
			text->textContent = (char *)realloc(text->textContent,baselen+len+2);
			strcat(text->textContent, " ");
			strncpy(text->textContent + baselen + 1, result, len);
			text->textContent[baselen+len+1] = 0;
		} else {
			text->textContent = (char *)realloc(text->textContent,baselen+len+1);
			strncpy(text->textContent + baselen, result, len);
			text->textContent[baselen+len] = 0;
		}
		gf_node_changed((GF_Node *)node, NULL);
		free(result);
		return;
	}
	case TAG_SVG_script:
	{
		SVGscriptElement *sc = (SVGscriptElement*)node;
		if (sc->textContent) free(sc->textContent);
		sc->textContent = result;
		gf_node_init((GF_Node *)node);
		return;
	}
	case TAG_SVG_handler:
	{
		SVGhandlerElement *h = (SVGhandlerElement*)node;
		if (h->textContent) free(h->textContent);
		h->textContent = result;
		gf_node_init((GF_Node *)node);
		return;
	}
	default:
		if (node->textContent) free(node->textContent);
		node->textContent = result;
		break;
	}
}


static GF_SVGParser *svg_new_parser(GF_SceneLoader *load)
{
	GF_SVGParser *parser;
	if ((load->type==GF_SM_LOAD_XSR) && !load->ctx) return NULL;
	GF_SAFEALLOC(parser, GF_SVGParser);
	parser->node_stack = gf_list_new();
	parser->defered_hrefs = gf_list_new();
	parser->defered_animations = gf_list_new();
	parser->defered_listeners = gf_list_new();

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


	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ( (load->type==GF_SM_LOAD_XSR) ? "SVG: MPEG-4 (LASER) Scene Parsing\n" : "SVG: SVG Scene Parsing\n"));

	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, svg_progress);
	if (e<0) return svg_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );
	return parser->last_error;
}

GF_Err gf_sm_load_init_SVGString(GF_SceneLoader *load, char *str_data)
{
	GF_Err e;
	GF_SVGParser *parser = (GF_SVGParser *)load->loader_priv;

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


GF_Err gf_sm_load_run_SVG(GF_SceneLoader *load)
{
	return GF_OK;
}

GF_Err gf_sm_load_done_SVG(GF_SceneLoader *load)
{
	GF_SVGParser *parser = (GF_SVGParser *)load->loader_priv;
	if (!parser) return GF_OK;
	while (gf_list_count(parser->node_stack)) {
		SVGNodeStack *st = (SVGNodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		free(st);
	}
	gf_list_del(parser->node_stack);
	gf_list_del(parser->defered_hrefs);
	gf_list_del(parser->defered_listeners);
	svg_reset_defered_animations(parser->defered_animations);
	gf_list_del(parser->defered_animations);
	gf_xml_sax_del(parser->sax_parser);
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}

#endif

