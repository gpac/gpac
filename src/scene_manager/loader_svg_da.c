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
#include <gpac/nodes_svg_da.h>

#ifndef GPAC_DISABLE_SVG

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

} GF_SVG_Parser;

typedef struct
{
	/*top of parsed sub-tree*/
	SVG_Element *node;
	/*depth of unknown elements being skipped*/
	u32 unknown_depth;
	/*last child added, used to speed-up parsing*/
	GF_ChildNodeItem *last_child;
} SVG_NodeStack;


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

static void svg_post_process_href(GF_SVG_Parser *parser, SVG_IRI *iri)
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

static Bool svg_parse_animation(GF_SVG_Parser *parser, GF_SceneGraph *sg, SVG_DeferedAnimation *anim, const char *nodeID)
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
			SVG_IRI *iri;
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_xlink_href, 1, 0, &info);
			iri = (SVG_IRI *)info.far_ptr;
			iri->type = SVG_IRI_ELEMENTID;
			iri->target = anim->target;
			gf_svg_register_iri(sg, iri);
		}

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);
		/* get the attribute name attribute if specified */
		if (gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_attributeName, 0, 0, &info) == GF_OK) {
			gf_svg_get_attribute_by_name((GF_Node *)anim->target, ((SMIL_AttributeName *)info.far_ptr)->name, 1, 1, &info);
			anim_value_type = info.fieldType;
		} else {
			if (tag == TAG_SVG_animateMotion) {
				anim_value_type = SVG_Motion_datatype;
			} else if (tag == TAG_SVG_discard) {
				/* there is no value to parse in discard, we can jump to the next stage */
				anim->resolve_stage = 1;
				return svg_parse_animation(parser, sg, anim, nodeID);
			} else {
				svg_report(parser, GF_OK, "Missing attributeName attribute on %s", gf_node_get_name((GF_Node *)anim->animation_elt));
				return 0;
			}
		}

		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_type, 1, 0, &info);
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

		if (anim->to) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_to, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->to, anim_value_type);
			if (anim_value_type==SVG_IRI_datatype) 
				svg_post_process_href(parser, (SVG_IRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->from) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_from, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->from, anim_value_type);
			if (anim_value_type==SVG_IRI_datatype) 
				svg_post_process_href(parser, (SVG_IRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->by) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_by, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->by, anim_value_type);
			if (anim_value_type==SVG_IRI_datatype) 
				svg_post_process_href(parser, (SVG_IRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		} 
		if (anim->values) {
			gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_values, 1, 0, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->values, anim_value_type);
			if (anim_value_type==SVG_IRI_datatype) {
				u32 i, count;
				SMIL_AnimateValues *anim_values;
				anim_values = (SMIL_AnimateValues *)info.far_ptr;
				count = gf_list_count(anim_values->values);
				for (i=0; i<count; i++) {
					SVG_IRI *iri = (SVG_IRI *)gf_list_get(anim_values->values, i);
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
		if (gf_svg_resolve_smil_times(sg, anim->anim_parent, *(GF_List **)info.far_ptr, 0, nodeID)) {
			anim->resolve_stage = 2;
		} else {
			return 0;
		}
	}

	gf_svg_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_end, 1, 0, &info);
	if (!gf_svg_resolve_smil_times(sg, anim->anim_parent, *(GF_List **)info.far_ptr, 1, nodeID)) return 0;

	/*animateMotion needs its children to be parsed before it can be initialized !! */
	if (gf_node_get_tag((GF_Node *)anim->animation_elt) != TAG_SVG_animateMotion)
		gf_node_init((GF_Node *)anim->animation_elt);
	
	return 1;
}

static void svg_resolved_refs(GF_SVG_Parser *parser, GF_SceneGraph *sg, const char *nodeID)
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
			iri->target = targ;
			gf_svg_register_iri(sg, iri);
			free(iri->iri);
			iri->iri = NULL;
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
			SVG_IRI *observer = info.far_ptr;
			if (observer->type == SVG_IRI_ELEMENTID) {
				if (!observer->target) continue;
				else par = observer->target;
			}
		}
		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 0, 0, &info) == GF_OK) {
			SVG_IRI *target = info.far_ptr;
			if (target->type == SVG_IRI_ELEMENTID) {
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
		if (svg_parse_animation(parser, sg, anim, nodeID)) {
			svg_delete_defered_anim(anim, parser->defered_animations);
			i--;
			count--;
		}
	}
}

static SVG_Element *svg_parse_element(GF_SVG_Parser *parser, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes, SVG_NodeStack *parent)
{
	u32	tag, i;
	Bool needs_init;
	SVG_Element *elt;
	const char *node_name = NULL;
	SVG_DeferedAnimation *anim = NULL;
	
	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = gf_svg_get_element_tag(name);
	if (tag == TAG_UndefinedNode) {
		svg_report(parser, GF_OK, "Unknown element %s - skipping", name);
		return NULL;
	}

	/* Creates a node in the current scene graph */
	elt = (SVG_Element*)gf_node_new(parser->load->scene_graph, tag);
	if (!elt) {
		parser->last_error = GF_SG_UNKNOWN_NODE;
		return NULL;
	}
	gf_node_register((GF_Node *)elt, (parent ? (GF_Node *)parent->node : NULL));
	if (parent && elt) gf_node_list_add_child_last( & parent->node->children, (GF_Node*)elt, & parent->last_child);

	if (gf_svg_is_animation_tag(tag)) {
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		anim->target = parent->node;
		anim->anim_parent = parent->node;
	} else if (tag == TAG_SVG_video || 
			   tag == TAG_SVG_audio || 
			   tag == TAG_SVG_animation) {
		/* warning: we use the SVG_DeferedAnimation structure for some timing nodes which are not 
		   animations, but we put the parse stage at 1 (timing) see svg_parse_animation. */
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
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
			gf_svg_parse_style((GF_Node *)elt, att->value);
		} else if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			gf_svg_parse_element_id((GF_Node *)elt, att->value, 0);
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
				SVG_IRI *iri = NULL;
				if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
					gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
					iri = info.far_ptr;
				} else {
					svg_report(parser, GF_OK, "Skipping %s", att->name);
				}
				svg_post_process_href(parser, iri);
			}
		} else if (!stricmp(att->name, "ev:event") &&  tag == TAG_SVG_handler) {
			/* When the handler element specifies the event attribute, an implicit listener is defined */
			GF_Node *node = (GF_Node *)elt;
			SVG_Element *listener;
			u32 type;
			GF_FieldInfo info;
			listener = (SVG_Element *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
			gf_node_register((GF_Node *)listener, node);
			gf_node_list_add_child( & ((GF_ParentNode *)node)->children, (GF_Node*) listener);
			/* this listener listens to the given type of event */
			type = gf_dom_event_type_by_name(att->value);
			gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_ev_event, 1, 0, &info);
			((XMLEV_Event *)info.far_ptr)->type = type;
			gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_event, 1, 0, &info);
			((XMLEV_Event *)info.far_ptr)->type = type;
			gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_handler, 1, 0, &info);
			((SVG_IRI *)info.far_ptr)->target = node;
			/* this listener listens with the parent of the handler as the event target */
			gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 1, 0, &info);
			((SVG_IRI *)info.far_ptr)->target = parent->node;
			gf_dom_listener_add((GF_Node *) parent->node, (GF_Node *) listener);
		} else {
			GF_FieldInfo info;
			if (gf_node_get_field_by_name((GF_Node *)elt, att->name, &info)==GF_OK) {
				gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
			} else {
				svg_report(parser, GF_OK, "Skipping %s", att->name);
			}
		}
	}

	needs_init = 1;
	if (anim) {
		if (svg_parse_animation(parser, parser->load->scene_graph, anim, NULL)) {
			svg_delete_defered_anim(anim, NULL);
			needs_init = 0;
		} else {
			gf_list_add(parser->defered_animations, anim);
		}
	}

	/* if the new element has an id, we try to resolve defered references */
	if (node_name) {
		svg_resolved_refs(parser, parser->load->scene_graph, node_name);
	}

	if (needs_init) gf_node_init((GF_Node *)elt);

	/*register listener element*/
	if (elt && (tag==TAG_SVG_listener)) {
		GF_FieldInfo info;
		Bool post_pone = 0;
		SVG_Element *par = NULL;
		SVG_Element *listener = (SVG_Element *)elt;

		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_observer, 0, 0, &info) == GF_OK) {
			SVG_IRI *observer = info.far_ptr;
			if (observer->type == SVG_IRI_ELEMENTID) {
				if (!observer->target) post_pone = 1;
				else par = observer->target;
			}
		}

		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_listener_target, 0, 0, &info) == GF_OK) {
			SVG_IRI *target = info.far_ptr;
			if (!par && (target->type == SVG_IRI_ELEMENTID)) {
				if (!target->target) post_pone = 1;
				else par = target->target;
			}
		}

		if (gf_svg_get_attribute_by_tag((GF_Node *)listener, TAG_SVG_ATT_handler, 1, 0, &info) == GF_OK) {
			SVG_IRI *handler = info.far_ptr;
			if (!handler->target) handler->target = parent->node;
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
	gf_sg_set_root_node(parser->load->scene_graph, (GF_Node *)root_svg);
	parser->has_root = 1;
}

static void svg_node_start(void *sax_cbck, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *stack, *parent;
	SVG_Element *elt;

	parent = (SVG_NodeStack *)gf_list_last(parser->node_stack);

	if (parser->has_root) {
		assert(parent);
	}

	elt = svg_parse_element(parser, name, name_space, attributes, nb_attributes, parent);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		return;
	}
	GF_SAFEALLOC(stack, SVG_NodeStack);
	stack->node = elt;
	gf_list_add(parser->node_stack, stack);

	if (gf_node_get_tag((GF_Node *)elt) == TAG_SVG_svg && !parser->has_root) 
		svg_init_root_element(parser, elt);
}

static void svg_node_end(void *sax_cbck, const char *name, const char *name_space)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *top = (SVG_NodeStack *)gf_list_last(parser->node_stack);

	if (!top) return;

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

			/*init animateMotion once all children have been parsed tomake sure we get the mpath child if any*/
			if (node->sgprivate->tag == TAG_SVG_animateMotion) {
				gf_node_init((GF_Node *)node);
			}
		}
	} else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
		} else {
			svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
		}
	}
}

static void svg_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	char *result;
	u32 len;

	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *top = (SVG_NodeStack *)gf_list_last(parser->node_stack);
	SVG_Element *elt = (top ? top->node : NULL);
	GF_FieldInfo info;
	u8 xml_space;
	u32 tag;

	if (!elt) return;

	result = strdup(text_content);
	len = strlen(text_content);

	if (is_cdata) goto skip_xml_space;

	if (gf_svg_get_attribute_by_tag((GF_Node *)elt, TAG_SVG_ATT_xml_space, 0, 0, &info) == GF_OK)
		xml_space = *((XML_Space *)info.far_ptr);
	else 
		xml_space = XML_SPACE_DEFAULT;

	if (xml_space != XML_SPACE_PRESERVE) {
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

	tag = gf_node_get_tag((GF_Node *)elt);
	switch (tag) {
		case TAG_SVG_text:
		{
			if (is_cdata) {
			} else {
				GF_DOMText *text = gf_dom_add_text_node((GF_Node *)elt, result);
				gf_node_changed((GF_Node *)text, NULL);
			}
			break;
		}
		default:
			free(result);
	}

}

static GF_SVG_Parser *svg_new_parser(GF_SceneLoader *load)
{
	GF_SVG_Parser *parser;
	if (load->type!=GF_SM_LOAD_SVG_DA) return NULL;

	GF_SAFEALLOC(parser, GF_SVG_Parser);
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

GF_Err gf_sm_load_init_svg(GF_SceneLoader *load)
{
	GF_Err e;
	GF_SVG_Parser *parser;

	if (!load->fileName) return GF_BAD_PARAM;
	parser = svg_new_parser(load);


	svg_report(parser, GF_OK, "Parsing ...");

	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, svg_progress);
	if (e<0) return svg_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );

	svg_report(parser, GF_OK, "... done");
#if 0
	{
		GF_SystemRTInfo rti;
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage before delete gz: %d\n", rti.gpac_memory);
		if (parser->sax_parser) {
			gf_xml_sax_del(parser->sax_parser);
			parser->sax_parser = NULL;
		}
		gf_sys_get_rti(0, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		fprintf(stdout, "Memory usage after delete gz: %d\n", rti.gpac_memory);
	}
#endif

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
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;
	if (!parser) return GF_OK;
	while (gf_list_count(parser->node_stack)) {
		SVG_NodeStack *st = (SVG_NodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		free(st);
	}
	gf_list_del(parser->node_stack);
	gf_list_del(parser->defered_hrefs);
	gf_list_del(parser->defered_listeners);
	/* reset animations */
	gf_list_del(parser->defered_animations);
	if (parser->sax_parser) {
		gf_xml_sax_del(parser->sax_parser);
	}
	free(parser);
	load->loader_priv = NULL;
	return GF_OK;
}

#endif

