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
#include <gpac/nodes_svg_sa.h>

#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_ENABLE_SVG_SA

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

} GF_SVG_SA_Parser;

typedef struct
{
	/*top of parsed sub-tree*/
	SVG_SA_Element *node;
	/*depth of unknown elements being skipped*/
	u32 unknown_depth;
	/*last child added, used to speed-up parsing*/
	GF_ChildNodeItem *last_child;
} SVG_SA_NodeStack;


typedef struct {
	/* Stage of the resolving:
	    0: resolving attributes which depends on the target: from, to, by, values, type
		1: resolving begin times
		2: resolving end times */
	u32 resolve_stage;
	/* Animation element being defered */
	SVG_SA_Element *animation_elt;
	/* anim parent*/
	SVG_SA_Element *anim_parent;
	/* target animated element*/
	SVG_SA_Element *target;
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

static GF_Err svg_sa_report(GF_SVG_SA_Parser *parser, GF_Err e, char *format, ...)
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

static void svg_sa_init_root_element(GF_SVG_SA_Parser *parser, SVG_SA_svgElement *root_svg)
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
	parser->has_root = 1;
}


static void svg_sa_delete_defered_anim(DeferedAnimation *anim, GF_List *defered_animations)
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

void svg_sa_reset_defered_animations(GF_List *l)
{
	/*delete all unresolved anims*/
	while (1) {
		DeferedAnimation *anim = (DeferedAnimation *)gf_list_last(l);
		if (!anim) break;
		svg_sa_delete_defered_anim(anim, l);
	}
}

static void svg_post_process_href(GF_SVG_SA_Parser *parser, XMLRI *iri)
{
	/*keep data when encoding*/
	if ( !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	/*unresolved, queue it...*/
	if ((iri->type==XMLRI_ELEMENTID) && !iri->target && iri->string) {
		gf_list_add(parser->defered_hrefs, iri);
	}
	if (iri->type != XMLRI_STRING) return;
	gf_svg_store_embedded_data(iri, parser->load->localPath, parser->load->fileName);
}

static Bool svg_sa_parse_animation(GF_SVG_SA_Parser *parser, GF_SceneGraph *sg, DeferedAnimation *anim, const char *nodeID)
{
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0;

	if (anim->resolve_stage==0) {
		if (!anim->target) anim->target = (SVG_SA_Element *) gf_sg_find_node_by_name(sg, anim->target_id + 1);
		if (!anim->target) return 0;

		/*setup IRI ptr*/
		anim->animation_elt->xlink->href.type = XMLRI_ELEMENTID;
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
			if (tag == TAG_SVG_SA_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else if (tag == TAG_SVG_SA_discard) {
				anim->resolve_stage = 1;
				return svg_sa_parse_animation(parser, sg, anim, nodeID);
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[SVG Parsing] missing attributeName attribute on %s\n", gf_node_get_name((GF_Node*)anim->animation_elt) ));
			}
		}

		if (anim->type && (tag== TAG_SVG_SA_animateTransform) ) {
			/* determine the transform_type in case of animateTransform attribute */
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "type", &info);
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] unknown datatype for animate transform.\n"));
				return 0;
			}
		}

		/* Parsing of to / from / by / values */
		if (anim->to) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "to", &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->to, anim_value_type);
			if (anim_value_type==XMLRI_datatype) svg_post_process_href(parser, (XMLRI*)anim->animation_elt->anim->to.value);
		} 
		if (anim->from) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "from", &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->from, anim_value_type);
			if (anim_value_type==XMLRI_datatype) svg_post_process_href(parser, (XMLRI*)anim->animation_elt->anim->from.value);
		} 
		if (anim->by) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "by", &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->by, anim_value_type);
			if (anim_value_type==XMLRI_datatype) svg_post_process_href(parser, (XMLRI*)anim->animation_elt->anim->by.value);
		} 
		if (anim->values) {
			gf_node_get_field_by_name((GF_Node *)anim->animation_elt, "values", &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->values, anim_value_type);
			if (anim_value_type==XMLRI_datatype) {
				u32 i, count = gf_list_count(anim->animation_elt->anim->values.values);
				for (i=0; i<count; i++) {
					XMLRI *iri = (XMLRI *)gf_list_get(anim->animation_elt->anim->values.values, i);
					svg_post_process_href(parser, iri);
				}
			}
		}
		anim->resolve_stage = 1;
	}

	if (anim->resolve_stage == 1) {
		if (gf_svg_resolve_smil_times(sg, anim->anim_parent, anim->animation_elt->timing->begin, 0, nodeID)) {
			anim->resolve_stage = 2;
		} else {
			return 0;
		}
	}
	if (!gf_svg_resolve_smil_times(sg, anim->anim_parent, anim->animation_elt->timing->end, 1, nodeID)) return 0;

	/*animateMotion needs its children to be parsed !! */
	if (!nodeID && gf_node_get_tag((GF_Node *)anim->animation_elt) == TAG_SVG_SA_animateMotion)
		return 1;

	/*OK init the anim*/
	gf_node_init((GF_Node *)anim->animation_elt);
	return 1;
}

static void svg_sa_resolved_refs(GF_SVG_SA_Parser *parser, GF_SceneGraph *sg, const char *nodeID)
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
			iri->target = (SVG_SA_Element *) targ;
			gf_svg_register_iri(sg, iri);
			free(iri->string);
			iri->string = NULL;
			gf_list_rem(parser->defered_hrefs, i);
			i--;
			count--;
		}
	}

	/*check unresolved listeners*/
	count = gf_list_count(parser->defered_listeners);
	for (i=0; i<count; i++) {
		GF_Node *par;
		SVG_SA_listenerElement *listener = (SVG_SA_listenerElement *)gf_list_get(parser->defered_listeners, i);

		par = NULL;
		if (listener->observer.type == XMLRI_ELEMENTID) {
			if (!listener->observer.target) continue;
			else par = (GF_Node*)listener->observer.target;
		}
		if (listener->target.type == XMLRI_ELEMENTID) {
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
		if (svg_sa_parse_animation(parser, sg, anim, nodeID)) {
			svg_sa_delete_defered_anim(anim, parser->defered_animations);
			i--;
			count--;
		}
	}
}

static SVG_SA_Element *svg_sa_parse_element(GF_SVG_SA_Parser *parser, const char *name, const char *name_space, 
									 const GF_XMLAttribute *attributes, u32 nb_attributes, 
									 SVG_SA_NodeStack *parent)
{
	u32	tag, i;
	SVG_SA_Element *elt;
	GF_FieldInfo info;
	const char *node_name = NULL;
	DeferedAnimation *anim = NULL;

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_sa_node_type_by_class_name(name);
	if (tag == TAG_UndefinedNode) {
		svg_sa_report(parser, GF_OK, "Unknown element %s - skipping", name);
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = (SVG_SA_Element*)gf_node_new(parser->load->scene_graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (parent ? (GF_Node *)parent->node : NULL));
	if (parent && elt) gf_node_list_add_child_last( & parent->node->children, (GF_Node*)elt, & parent->last_child);


	if (gf_svg_sa_is_animation_tag(tag)) {
		GF_SAFEALLOC(anim, DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		anim->target = parent->node;
		anim->anim_parent = parent->node;
	} else if (tag == TAG_SVG_SA_video || tag == TAG_SVG_SA_audio || tag == TAG_SVG_SA_animation || tag == TAG_SVG_SA_conditional) {
		/* warning: we use the DeferedAnimation structure for some timing nodes which are not 
		   animations, but we put the parse stage at 1 (timing) see svg_sa_parse_animation. */
		GF_SAFEALLOC(anim, DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		anim->anim_parent = parent->node;
		anim->resolve_stage = 1;
	}

	/*parse all att*/
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;

		if (!stricmp(att->name, "style")) {
			/* Special case: style if present will always be first in the list */
			gf_svg_parse_style((GF_Node *)elt, att->value);
		} else if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			gf_svg_parse_element_id((GF_Node *)elt, att->value, 0);
			node_name = att->value;
		} else if (anim && !stricmp(att->name, "attributeName")) {
			anim->attributeName = strdup(att->value);
		} else if (anim && !stricmp(att->name, "to")) {
			anim->to = strdup(att->value);
		} else if (anim && !stricmp(att->name, "from")) {
			anim->from = strdup(att->value);
		} else if (anim && !stricmp(att->name, "by")) {
			anim->by = strdup(att->value);
		} else if (anim && !stricmp(att->name, "values")) {
			anim->values = strdup(att->value);
		} else if (anim && (tag == TAG_SVG_SA_animateTransform) && !stricmp(att->name, "type")) {
			anim->type = strdup(att->value);
		} else if (!stricmp(att->name, "xlink:href") ) {
			if (!elt->xlink) {
				svg_sa_report(parser, GF_OK, "xlink:href on element %s ignored\n", name); 
			} else if (gf_svg_sa_is_animation_tag(tag)) {
				assert(anim);
				anim->target_id = strdup(att->value);
				/*may be NULL*/
				anim->target = (SVG_SA_Element *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
			} else {
				XMLRI *iri = & elt->xlink->href;
				memset(&info, 0, sizeof(GF_FieldInfo));
				info.far_ptr = & elt->xlink->href;
				info.fieldType = XMLRI_datatype;
				info.name = "xlink:href";
				gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);

				svg_post_process_href(parser, iri);
			}
		} else if (!strnicmp(att->name, "xmlns", 5)) {
		} else if (!stricmp(att->name, "ev:event") &&  tag == TAG_SVG_SA_handler) {
			/* When the handler element specifies the event attribute, an implicit listener is defined */
			GF_Node *node = (GF_Node *)elt;
			SVG_SA_listenerElement *listener;
			listener = (SVG_SA_listenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_SA_listener);
			gf_node_register((GF_Node *)listener, node);
			gf_node_list_add_child( & ((GF_ParentNode *)node)->children, (GF_Node*) listener);
			/* this listener listens to the given type of event */
			((SVG_SA_handlerElement *)node)->ev_event.type = listener->event.type = gf_dom_event_type_by_name(att->value);
			listener->handler.target = (SVG_SA_Element *)node;
			/* this listener listens with the parent of the handler as the event target */
			listener->target.target = parent->node;
			gf_dom_listener_add((GF_Node *) parent->node, (GF_Node *) listener);
		} else {
			u32 evtType = GF_EVENT_UNKNOWN;
			if (!strncmp(att->name, "on", 2)) evtType = gf_dom_event_type_by_name(att->name + 2);

			/*SVG 1.1 events: create a listener and a handler on the fly, register them with current node
			and add listener struct*/
			if (evtType != GF_EVENT_UNKNOWN) {
				XMLEV_Event evt;
				SVG_SA_handlerElement *handler;
				handler = gf_dom_listener_build((GF_Node *) elt, evtType, 0, NULL);
				handler->textContent = strdup(att->value);
				gf_node_init((GF_Node *)handler);
			} else if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
				gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
				if (info.fieldType== XMLRI_datatype) {
					XMLRI *iri = (XMLRI *)info.far_ptr;
					if ((iri->type==XMLRI_ELEMENTID) && !iri->target && iri->string)
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
				svg_sa_report(parser, GF_OK, "Unknown attribute %s on element %s - skipping\n", (char *)att->name, name);
			}
		}
	}

	if (anim) {
		if (svg_sa_parse_animation(parser, parser->load->scene_graph, anim, NULL)) {
			svg_sa_delete_defered_anim(anim, NULL);
		} else {
			gf_list_add(parser->defered_animations, anim);
		}
	}

	/* if the new element has an id, we try to resolve defered references */
	if (node_name) {
		svg_sa_resolved_refs(parser, parser->load->scene_graph, node_name);
	}

	/* We need to init the node at the end of the parsing, after parsing all attributes */
	/* text nodes must be initialized at the end of the <text> element */
	if (!anim && elt) gf_node_init((GF_Node *)elt);

	/*register listener element*/
	if (elt && (tag==TAG_SVG_SA_listener)) {
		Bool post_pone = 0;
		SVG_SA_Element *par = NULL;
		SVG_SA_listenerElement *listener = (SVG_SA_listenerElement *)elt;
		if (listener->observer.type == XMLRI_ELEMENTID) {
			if (!listener->observer.target) post_pone = 1;
			else par = listener->observer.target;
		}
		if (!par && (listener->target.type == XMLRI_ELEMENTID)) {
			if (!listener->target.target) post_pone = 1;
			else par = listener->target.target;
		}

		if (!listener->handler.target) listener->handler.target = parent->node;
		
		if (post_pone) {
			gf_list_add(parser->defered_listeners, listener);
		} else {
			if (!par) par = parent->node;
			gf_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
		}
	}
	return elt;
}

static void svg_sa_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	SVG_SA_NodeStack *stack, *parent;
	SVG_SA_Element *elt;
	GF_SVG_SA_Parser *parser = (GF_SVG_SA_Parser *)sax_cbck;

	parent = (SVG_SA_NodeStack *)gf_list_last(parser->node_stack);


	if (parser->has_root) {
		assert(parent);
	}
	elt = svg_sa_parse_element(parser, name, name_space, attributes, nb_attributes, parent);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		return;
	}
	GF_SAFEALLOC(stack, SVG_SA_NodeStack);
	stack->node = elt;
	gf_list_add(parser->node_stack, stack);

	if (gf_node_get_tag((GF_Node *)elt) == TAG_SVG_SA_svg && !parser->has_root) 
		svg_sa_init_root_element(parser, (SVG_SA_svgElement *)elt);

}

static void svg_sa_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	u32 end_tag;
	GF_SVG_SA_Parser *parser = (GF_SVG_SA_Parser *)sax_cbck;
	SVG_SA_NodeStack *top = (SVG_SA_NodeStack *)gf_list_last(parser->node_stack);

	/*only remove created nodes ... */
	end_tag = gf_svg_sa_node_type_by_class_name(name);
	if (end_tag != TAG_UndefinedNode) {
		GF_Node *node = (GF_Node *) top->node;
		if (node->sgprivate->tag != end_tag) {
			if (top->unknown_depth) {
				top->unknown_depth--;
				return;
			} else {
				svg_sa_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
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
			if (node->sgprivate->tag == TAG_SVG_SA_animateMotion) {
				gf_node_init(node);
			}
		}
	} else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
		} else {
			svg_sa_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
		}
	}
}

static void svg_sa_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	char *result;
	u32 len;
	Bool space_preserve = 0;
	GF_SVG_SA_Parser *parser = (GF_SVG_SA_Parser *)sax_cbck;
	SVG_SA_Element *node_core, *node;
	SVG_SA_NodeStack *top = (SVG_SA_NodeStack *)gf_list_last(parser->node_stack);
	node = top ? (SVG_SA_Element *) top->node : NULL;
	if (!node) return;
	if (top && top->unknown_depth) return;

	node_core = node;
	if (!node_core) return;
	if (!node_core->core) return;

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


	switch (gf_node_get_tag((GF_Node *)node)) {
	case TAG_SVG_SA_text:
	{
		SVG_SA_textElement *text = (SVG_SA_textElement *)node;
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
	default:
		if (node->textContent) free(node->textContent);
		node->textContent = result;
		break;
	}
}


static GF_SVG_SA_Parser *svg_sa_new_parser(GF_SceneLoader *load)
{
	GF_SVG_SA_Parser *parser;
	GF_SAFEALLOC(parser, GF_SVG_SA_Parser);
	parser->node_stack = gf_list_new();
	parser->defered_hrefs = gf_list_new();
	parser->defered_animations = gf_list_new();
	parser->defered_listeners = gf_list_new();

	parser->sax_parser = gf_xml_sax_new(svg_sa_node_start, svg_sa_node_end, svg_sa_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;
	if (load->ctx) load->ctx->is_pixel_metrics = 1;

	return parser;
}

extern size_t gpac_allocated_memory;

GF_Err gf_sm_load_init_svg_sa(GF_SceneLoader *load)
{
	GF_Err e;
	GF_SVG_SA_Parser *parser;

	if (!load->fileName) return GF_BAD_PARAM;
	parser = svg_sa_new_parser(load);

	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, svg_progress);
	if (e<0) return svg_sa_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );
#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage : %d\n", rti.gpac_memory);
	}
#endif

	return parser->last_error;
}

GF_Err gf_sm_load_init_svg_sa_string(GF_SceneLoader *load, char *str_data)
{
	GF_Err e;
	GF_SVG_SA_Parser *parser = (GF_SVG_SA_Parser *)load->loader_priv;

	if (!parser) {
		char BOM[6];
		BOM[0] = str_data[0];
		BOM[1] = str_data[1];
		BOM[2] = str_data[2];
		BOM[3] = str_data[3];
		BOM[4] = BOM[5] = 0;
		parser = svg_sa_new_parser(load);
		e = gf_xml_sax_init(parser->sax_parser, (unsigned char*)BOM);
		if (e) {
			svg_sa_report(parser, e, "Error initializing SAX parser: %s", gf_xml_sax_get_error(parser->sax_parser) );
			return e;
		}
		str_data += 4;
	}
	return gf_xml_sax_parse(parser->sax_parser, str_data);
}


GF_Err gf_sm_load_run_svg_sa(GF_SceneLoader *load)
{
	return GF_OK;
}

GF_Err gf_sm_load_done_svg_sa(GF_SceneLoader *load)
{
	GF_SVG_SA_Parser *parser = (GF_SVG_SA_Parser *)load->loader_priv;
	if (!parser) return GF_OK;
	while (gf_list_count(parser->node_stack)) {
		SVG_SA_NodeStack *st = (SVG_SA_NodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		free(st);
	}
	gf_list_del(parser->node_stack);
	gf_list_del(parser->defered_hrefs);
	gf_list_del(parser->defered_listeners);
	svg_sa_reset_defered_animations(parser->defered_animations);
	gf_list_del(parser->defered_animations);
	gf_xml_sax_del(parser->sax_parser);
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}

#endif /*GPAC_ENABLE_SVG_SA*/

#endif	/*GPAC_DISABLE_SVG*/

