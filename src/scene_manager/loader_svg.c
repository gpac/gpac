/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/nodes_svg.h>
#include <gpac/base_coding.h>

#include <gpac/internal/terminal_dev.h>


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
	u32 has_root;

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

	/*the namespace of the parent element. This is a shortcut to avoid querying the namespace list stored
	in the scene graph to find out the namespace of the current element.
	For example in <foo:bar><foo:test/></foo:bar>, it avoids looking for the namespace when parsing <foo:test>
	*/
	u32 current_ns;

	/*if parser is used to parse a fragment, the root of the fragment is stored here*/
	GF_Node *fragment_root;
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

	/*the namespace of the parent element (used for restoring the context after the current node has been parsed)*/
	u32 current_ns;
	Bool has_ns;
} SVG_NodeStack;

static GF_Err svg_report(GF_SVG_Parser *parser, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 2048, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[SVG Parsing] line %d - %s\n", gf_xml_sax_get_line(parser->sax_parser), szMsg));
	}
#endif
	if (e) {
		parser->last_error = e;
		gf_xml_sax_suspend(parser->sax_parser, GF_TRUE);
	}
	return e;
}

static void svg_progress(void *cbk, u64 done, u64 total)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)cbk;

	/*notify MediaEvent*/
	if (parser->load && parser->load->is) {
		parser->load->is->on_media_event(parser->load->is, GF_EVENT_MEDIA_PROGRESS);
		if (done == total) {
			parser->load->is->on_media_event(parser->load->is, GF_EVENT_MEDIA_LOAD_DONE);
		}
	}
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
	if (!st) return NULL;

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
			GF_ESD *esd = (GF_ESD *)gf_list_get(parser->load->ctx->root_od->ESDescriptors, i);
			if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
				GF_LASERConfig *cfg = (GF_LASERConfig *)esd->decoderConfig->decoderSpecificInfo;
				if (cfg && (cfg->tag==GF_ODF_LASER_CFG_TAG)) {
					if (!cfg->extensionIDBits) cfg->extensionIDBits = 2;
				}
			}
		}
	}
}


static void svg_process_media_href(GF_SVG_Parser *parser, GF_Node *elt, XMLRI *iri)
{
	u32 tag = gf_node_get_tag(elt);

	if ((tag==TAG_SVG_image) || (tag==TAG_SVG_video) || (tag==TAG_SVG_audio)) {
		SVG_SAFExternalStream *st = svg_saf_get_stream(parser, 0, iri->string+1);
		if (!st && !strnicmp(iri->string, "stream:", 7))
			st = svg_saf_get_stream(parser, 0, iri->string+7);
		if (st) {
			gf_free(iri->string);
			iri->string = NULL;
			iri->lsr_stream_id = st->id;
			iri->type = XMLRI_STREAMID;
			return;
		}
	}
	if ((parser->load->flags & GF_SM_LOAD_EMBEDS_RES) && (iri->type==XMLRI_STRING) ) {
		u64 size;
		char *buffer;
		FILE *f;
		f = gf_fopen(iri->string, "rb");
		if (!f) {
			return;
		}
		gf_fseek(f, 0, SEEK_END);
		size = gf_ftell(f);
		gf_fseek(f, 0, SEEK_SET);

		buffer = gf_malloc(sizeof(char) * (size_t)(size+1));
		size = fread(buffer, sizeof(char), (size_t)size, f);
		gf_fclose(f);


		if (tag==TAG_SVG_script) {
			GF_DOMText *dtext;
			GF_DOMAttribute *att, *prev;
			buffer[size]=0;
			dtext = gf_dom_add_text_node(elt, buffer);
			dtext->type = GF_DOM_TEXT_CDATA;

			gf_free(iri->string);
			iri->string=NULL;

			/*delete attribute*/
			att = ((GF_DOMNode*)elt)->attributes;
			prev = NULL;
			while(att) {
				if (att->tag!=TAG_XLINK_ATT_href) {
					prev = att;
					att = att->next;
					continue;
				}
				gf_svg_delete_attribute_value(att->data_type, att->data, elt->sgprivate->scenegraph);
				if (prev) prev->next = att->next;
				else ((GF_DOMNode*)elt)->attributes = att->next;
				gf_free(att);
				break;
			}
		} else {
			char *mtype;
			char *buf64;
			u64 size64;
			char *ext;
			buf64 = (char *)gf_malloc((size_t)size*2);
			size64 = gf_base64_encode(buffer, (u32)size, buf64, (u32)size*2);
			buf64[size64] = 0;
			mtype = "application/data";
			ext = strchr(iri->string, '.');
			if (ext) {
				if (!stricmp(ext, ".png")) mtype = "image/png";
				if (!stricmp(ext, ".jpg") || !stricmp(ext, ".jpeg")) mtype = "image/jpg";
			}
			gf_free(iri->string);
			iri->string = (char *)gf_malloc(sizeof(char)*(40+(size_t)size64));
			sprintf(iri->string, "data:%s;base64,%s", mtype, buf64);
			gf_free(buf64);
			gf_free(buffer);
		}
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

	tag = gf_xml_get_element_tag(node_class, parser->current_ns);
	n = gf_node_new(parser->load->scene_graph, tag);

	gf_free(node_class);

	if (n) {
		gf_svg_parse_element_id(n, ID, GF_FALSE);
		gf_list_add(parser->peeked_nodes, n);
	}
	return n;
}

static void svg_post_process_href(GF_SVG_Parser *parser, GF_Node *elt, XMLRI *iri)
{
	GF_Err e;

	/* Embed script if needed or clean data URL with proper mime type */
	svg_process_media_href(parser, elt, iri);

	/*keep data when encoding*/
	if ( !(parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK)) return;

	/*unresolved, queue it...*/
	if ((iri->type==XMLRI_ELEMENTID) && !iri->target && iri->string) {
		gf_list_add(parser->defered_hrefs, iri);
	}
	if (iri->type != XMLRI_STRING) return;
	e = gf_node_store_embedded_data(iri, parser->load->localPath, parser->load->fileName);
	if (e) svg_report(parser, e, "Error storing embedded IRI data");
}

static void svg_delete_defered_anim(SVG_DeferedAnimation *anim, GF_List *defered_animations)
{
	if (defered_animations) gf_list_del_item(defered_animations, anim);

	if (anim->target_id) gf_free(anim->target_id);
	if (anim->to) gf_free(anim->to);
	if (anim->from) gf_free(anim->from);
	if (anim->by) gf_free(anim->by);
	if (anim->values) gf_free(anim->values);
	if (anim->type) gf_free(anim->type);
	gf_free(anim);
}

static Bool svg_parse_animation(GF_SVG_Parser *parser, GF_SceneGraph *sg, SVG_DeferedAnimation *anim, const char *nodeID, u32 force_type)
{
	GF_FieldInfo info;
	u32 tag;
	u8 anim_value_type = 0;

	if (anim->resolve_stage==0) {
		/* Stage 0: parsing the animation attribute values
					for that we need to resolve the target first */

		/* if we don't have a target, try to get it */
		if (!anim->target)
			anim->target = (SVG_Element *) gf_sg_find_node_by_name(sg, anim->target_id + 1);

		/* if now we have a target, create the xlink:href attribute on the animation element and set it to the found target */
		if (anim->target) {
			XMLRI *iri;
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info);
			iri = (XMLRI *)info.far_ptr;
			iri->type = XMLRI_ELEMENTID;
			iri->target = anim->target;
			gf_node_register_iri(sg, iri);
		}

		tag = gf_node_get_tag((GF_Node *)anim->animation_elt);
		/* get the attribute name attribute if specified */
		if (anim->type && (tag== TAG_SVG_animateTransform) ) {
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_transform_type, GF_TRUE, GF_FALSE, &info);
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
				return GF_FALSE;
			}
		}
		else if (gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_attributeName, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			SMIL_AttributeName *attname = (SMIL_AttributeName *)info.far_ptr;

			/*parse the attribute name even if the target is not found, because a namespace could be specified and
			only valid for the current node*/
			if (!attname->type) {
				char *sep;
				char *name = attname->name;
				sep = strchr(name, ':');
				if (sep) {
					sep[0] = 0;
					attname->type = gf_sg_get_namespace_code(anim->animation_elt->sgprivate->scenegraph, name);
					sep[0] = ':';
					name = gf_strdup(sep+1);
					gf_free(attname->name);
					attname->name = name;
				} else {
					attname->type = parser->current_ns;
				}
			}

			/* the target is still not known stay in stage 0 */
			if (!anim->target) return GF_FALSE;

			gf_node_get_attribute_by_name((GF_Node *)anim->target, attname->name, attname->type, GF_TRUE, GF_TRUE, &info);
			/*set the tag value to avoid parsing the name in the anim node_init phase*/
			attname->tag = info.fieldIndex;
			attname->type = 0;
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
				return GF_FALSE;
			}
		}

		/* the target is still not known stay in stage 0 */
		if (!anim->target) return GF_FALSE;

		if (anim->to) {
			/* now that we have a target, if there is a to value to parse, create the attribute and parse it */
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_to, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->to, anim_value_type);
			if (anim_value_type==XMLRI_datatype) {
				svg_post_process_href(parser, (GF_Node *) anim->target, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
			}
		}
		if (anim->from) {
			/* now that we have a target, if there is a from value to parse, create the attribute and parse it */
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_from, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->from, anim_value_type);
			if (anim_value_type==XMLRI_datatype)
				svg_post_process_href(parser,  (GF_Node *) anim->target, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		}
		if (anim->by) {
			/* now that we have a target, if there is a by value to parse, create the attribute and parse it */
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_by, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->by, anim_value_type);
			if (anim_value_type==XMLRI_datatype)
				svg_post_process_href(parser,  (GF_Node *) anim->target, (XMLRI*)((SMIL_AnimateValue *)info.far_ptr)->value);
		}
		if (anim->values) {
			/* now that we have a target, if there is a 'values' value to parse, create the attribute and parse it */
			gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_values, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute((GF_Node *)anim->animation_elt, &info, anim->values, anim_value_type);
			if (anim_value_type==XMLRI_datatype) {
				u32 i, count;
				SMIL_AnimateValues *anim_values;
				anim_values = (SMIL_AnimateValues *)info.far_ptr;
				count = gf_list_count(anim_values->values);
				for (i=0; i<count; i++) {
					XMLRI *iri = (XMLRI *)gf_list_get(anim_values->values, i);
					svg_post_process_href(parser,  (GF_Node *) anim->target, iri);
				}
			}
		}
		anim->resolve_stage = 1;
	}

	if (anim->resolve_stage == 1) {
		/* Stage 1: parsing the begin values
					we go into the next stage only if at least one begin value is resolved */
		gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_begin, GF_TRUE, GF_FALSE, &info);
		if (gf_svg_resolve_smil_times((GF_Node *)anim->animation_elt, anim->target, *(GF_List **)info.far_ptr, GF_FALSE, nodeID)) {
			anim->resolve_stage = 2;
		} else if (force_type!=2) {
			return GF_FALSE;
		}
	}

	gf_node_get_attribute_by_tag((GF_Node *)anim->animation_elt, TAG_SVG_ATT_end, GF_TRUE, GF_FALSE, &info);
	if (!gf_svg_resolve_smil_times((GF_Node *)anim->animation_elt, anim->target, *(GF_List **)info.far_ptr, GF_TRUE, nodeID)) {
		if (force_type!=2) return GF_FALSE;
	}

	/*animateMotion needs its children to be parsed before it can be initialized !! */
	if (force_type || gf_node_get_tag((GF_Node *)anim->animation_elt) != TAG_SVG_animateMotion) {
		gf_node_init((GF_Node *)anim->animation_elt);
		return GF_TRUE;
	} else {
		return GF_FALSE;
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
			gf_node_register_iri(sg, iri);
			gf_free(iri->string);
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
		if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_observer, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			XMLRI *observer = (XMLRI *)info.far_ptr;
			if (observer->type == XMLRI_ELEMENTID) {
				if (!observer->target && observer->string && !strcmp(observer->string, nodeID) ) {
					observer->target = gf_sg_find_node_by_name(sg, (char*) nodeID);
				}
			}
			if (observer->type == XMLRI_ELEMENTID) par = (GF_Node *)observer->target;
			if (!par) continue;
		}
		if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_target, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			XMLRI *target = (XMLRI *)info.far_ptr;
			if (target->type == XMLRI_ELEMENTID) {
				if (!target->target) continue;
				else {
					if (!par) par = (GF_Node*)target->target;
				}
			}
		}
		assert(par);
		gf_node_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
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
	if (!gf_node_get_attribute_by_tag((GF_Node *)root_svg, TAG_SVG_ATT_width, GF_FALSE, GF_FALSE, &width_info)
	        && !gf_node_get_attribute_by_tag((GF_Node *)root_svg, TAG_SVG_ATT_height, GF_FALSE, GF_FALSE, &height_info)) {
		SVG_Length * w = (SVG_Length *)width_info.far_ptr;
		SVG_Length * h = (SVG_Length *)height_info.far_ptr;
		if (w->type == SVG_NUMBER_VALUE) svg_w = FIX2INT(w->value);
		if (h->type == SVG_NUMBER_VALUE) svg_h = FIX2INT(h->value);
		gf_sg_set_scene_size_info(parser->load->scene_graph, svg_w, svg_h, GF_TRUE);
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

static void svg_check_namespace(GF_SVG_Parser *parser, const GF_XMLAttribute *attributes, u32 nb_attributes, Bool *has_ns)
{
	u32 i;
	/*parse all att to check for namespaces, to:
	   - add the prefixed namespaces (xmlns:xxxx='...')
	   - change the namespace for this element if no prefix (xmlns='...')*/
 	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		if (!strncmp(att->name, "xmlns", 5)) {
			/* check if we have a prefix for this namespace */
			char *qname = strchr(att->name, ':');
			if (qname) {
				qname++;
			}
			/* Adds the namespace to the scene graph, either with a prefix or with NULL */
			gf_sg_add_namespace(parser->load->scene_graph, att->value, qname);

			if (!qname) {
				/* Only if there was no prefix, we change the namespace for the current element */
				parser->current_ns = gf_sg_get_namespace_code_from_name(parser->load->scene_graph, att->value);
			}
			/* Signal that when ending this element, namespaces will have to be removed */
			*has_ns = GF_TRUE;
		}
	}
}

//#define SKIP_ALL
//#define SKIP_ATTS
//#define SKIP_ATTS_PARSING
//#define SKIP_INIT

//#define SKIP_UNKNOWN_NODES

static SVG_Element *svg_parse_element(GF_SVG_Parser *parser, const char *name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes, SVG_NodeStack *parent, Bool *has_ns)
{
	GF_FieldInfo info;
	u32	tag, i, count, ns, xmlns;
	Bool needs_init, has_id;
	SVG_Element *elt = NULL;
	const char *node_name = NULL;
	const char *ev_event, *ev_observer;
	SVG_DeferedAnimation *anim = NULL;
	char *ID = NULL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[SVG Parsing] Parsing node %s\n", name));

	*has_ns = GF_FALSE;

	svg_check_namespace(parser, attributes, nb_attributes, has_ns);

	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		if (!att->value || !strlen(att->value)) continue;
		/* FIXME: This should be changed to reflect that xml:id has precedence over id if both are specified with different values */
		if (!stricmp(att->name, "id") || !stricmp(att->name, "xml:id")) {
			if (!ID) ID = att->value;
		}
	}

	/* CHECK: overriding the element namespace with the parent one, if given ???
	   This is wrong ??*/
	xmlns = parser->current_ns;
	if (name_space) {
		xmlns = gf_sg_get_namespace_code(parser->load->scene_graph, (char *) name_space);
		if (!xmlns) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[SVG Parsing] line %d - XMLNS prefix %s not defined - skipping\n", gf_xml_sax_get_line(parser->sax_parser), name_space));
			return NULL;
		}
	}

	/* Translates the node type (called name) from a String into a unique numeric identifier in GPAC */
	tag = xmlns ? gf_xml_get_element_tag(name, xmlns) : TAG_UndefinedNode;
	if (tag == TAG_UndefinedNode) {
#ifdef SKIP_UNKNOWN_NODES
		GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[SVG Parsing] line %d - Unknown element %s - skipping\n", gf_xml_sax_get_line(parser->sax_parser), name));
		return NULL;
#else
		tag = TAG_DOMFullNode;
#endif
	}

	/* If this element has an ID, we look in the list of elements already created in advance (in case of reference) to see if it is there,
	   in which case we will reuse it*/
	has_id = GF_FALSE;
	count = gf_list_count(parser->peeked_nodes);
	if (count && ID) {
		for (i=0; i<count; i++) {
			GF_Node *n = (GF_Node *)gf_list_get(parser->peeked_nodes, i);
			const char *n_id = gf_node_get_name(n);
			if (n_id && !strcmp(n_id, ID)) {
				gf_list_rem(parser->peeked_nodes, i);
				has_id = GF_TRUE;
				elt = (SVG_Element*)n;
				break;
			}
		}
	}

	/* If the element was found in the list of elements already created, we do not need to create it, we reuse it.
	   Otherwise, we create it based on the tag */
	if (!has_id) {
		/* Creates a node in the current scene graph */
		elt = (SVG_Element*)gf_node_new(parser->load->scene_graph, tag);
		if (!elt) {
			parser->last_error = GF_SG_UNKNOWN_NODE;
			return NULL;
		}
		/* CHECK: Why isn't this code in the gf_node_new call ?? */
		if (tag == TAG_DOMFullNode) {
			GF_DOMFullNode *d = (GF_DOMFullNode *)elt;
			d->name = gf_strdup(name);
			d->ns = xmlns;
		}
	}

	/* We indicate that the element is used by its parent (reference counting for safe deleting) */
	gf_node_register((GF_Node *)elt, (parent ? (GF_Node *)parent->node : NULL));
	/* We attach this element as the last child of its parent */
	if (parent && elt) gf_node_list_add_child_last( & parent->node->children, (GF_Node*)elt, & parent->last_child);

	/* By default, all elements will need initialization for rendering, except some that will explicitly set it to 0 */
	needs_init = GF_TRUE;

	if (gf_svg_is_animation_tag(tag)) {
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
		if (!anim) {
			parser->last_error = GF_OUT_OF_MEM;
			return NULL;
		}
		/*default anim target is parent node*/
		anim->animation_elt = elt;
		if (!parent) {
			if (parser->command) {
				anim->target = anim->anim_parent = (SVG_Element*) parser->command->node;
			}
		} else {
			anim->target = anim->anim_parent = parent->node;
		}
	} else if (gf_svg_is_timing_tag(tag)) {
		/* warning: we use the SVG_DeferedAnimation structure for some timing nodes which are not
		   animations, but we put the parse stage at 1 (timing) see svg_parse_animation. */
		GF_SAFEALLOC(anim, SVG_DeferedAnimation);
		if (!anim) {
			parser->last_error = GF_OUT_OF_MEM;
			return NULL;
		}
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
		/* Scripts and handlers don't render and have no initialization phase */
		needs_init = GF_FALSE;
	}

	ev_event = ev_observer = NULL;

#ifdef SKIP_ATTS
	nb_attributes = 0;
#endif

	/*set the root of the SVG tree BEFORE processing events in order to have it setup for script init (e.g. load events, including in root svg)*/
	if ((tag == TAG_SVG_svg) && !parser->has_root) {
		svg_init_root_element(parser, elt);
	}

	/*parse all att*/
	for (i=0; i<nb_attributes; i++) {
		GF_XMLAttribute *att = (GF_XMLAttribute *)&attributes[i];
		char *att_name = NULL;
		if (!att->value || !strlen(att->value)) continue;

		/* first determine in which namespace is the attribute and store the result in ns,
		then shift the char buffer to point to the local name of the attribute*/
		ns = xmlns;
		att_name = strchr(att->name, ':');
		if (att_name) {
			if (!strncmp(att->name, "xmlns", 5)) {
				ns = gf_sg_get_namespace_code(parser->load->scene_graph, att_name+1);
				att_name = att->name;
			} else {
				att_name[0] = 0;
				ns = gf_sg_get_namespace_code(parser->load->scene_graph, att->name);
				att_name[0] = ':';
				att_name++;
			}
		} else {
			att_name = att->name;
		}

		/* Begin of special cases of attributes */

		/* CHECK: Shouldn't namespaces be checked here ? */
		if (!stricmp(att_name, "style")) {
			gf_svg_parse_style((GF_Node *)elt, att->value);
			continue;
		}

		/* Some attributes of the animation elements cannot be parsed (into typed values) until the type of value is known,
		   we defer the parsing and store them temporarily as strings */
		if (anim) {
			if (!stricmp(att_name, "to")) {
				anim->to = gf_strdup(att->value);
				continue;
			}
			if (!stricmp(att_name, "from")) {
				anim->from = gf_strdup(att->value);
				continue;
			}
			if (!stricmp(att_name, "by")) {
				anim->by = gf_strdup(att->value);
				continue;
			}
			if (!stricmp(att_name, "values")) {
				anim->values = gf_strdup(att->value);
				continue;
			}
			if ((tag == TAG_SVG_animateTransform) && !stricmp(att_name, "type")) {
				anim->type = gf_strdup(att->value);
				continue;
			}
		}

		/* Special case for xlink:href attributes */
		if ((ns == GF_XMLNS_XLINK) && !stricmp(att_name, "href") ) {

			if (gf_svg_is_animation_tag(tag)) {
				/* For xlink:href in animation elements,
				we try to locate the target of the xlink:href to determine the type of values to be animated */
				assert(anim);
				anim->target_id = gf_strdup(att->value);
				/*The target may be NULL, if it has not yet been parsed, we will try to resolve it later on */
				anim->target = (SVG_Element *) gf_sg_find_node_by_name(parser->load->scene_graph, anim->target_id + 1);
				continue;
			} else {
				/* For xlink:href attribute on elements other than animation elements,
				   we create the attribute, parse it and try to do some special process it */
				GF_FieldInfo info;
				XMLRI *iri = NULL;
				if (gf_node_get_attribute_by_tag((GF_Node *)elt, TAG_XLINK_ATT_href, GF_TRUE, GF_FALSE, &info)==GF_OK) {
					gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
					iri = (XMLRI *)info.far_ptr;

					/* extract streamID ref or data URL and store as file */
					svg_post_process_href(parser, (GF_Node *)elt, iri);
					continue;
				}
			}
		}

		/* For the XML Event handler element, we need to defer the parsing of some attributes */
		if ((tag == TAG_SVG_handler) && (ns == GF_XMLNS_XMLEV)) {
			if (!stricmp(att_name, "event") ) {
				ev_event = att->value;
				continue;
			}
			if (!stricmp(att_name, "observer") ) {
				ev_observer = att->value;
				continue;
			}
		}

		/*laser specific stuff*/
		if (ns == GF_XMLNS_LASER) {
			/* CHECK: we should probably check the namespace of the attribute here */
			if (!stricmp(att_name, "scale") ) {
				if (gf_node_get_attribute_by_tag((GF_Node *)elt, TAG_SVG_ATT_transform, GF_TRUE, GF_TRUE, &info)==GF_OK) {
					SVG_Point pt;
					SVG_Transform *mat = (SVG_Transform *)info.far_ptr;
					svg_parse_point(&pt, att->value);
					gf_mx2d_add_scale(&mat->mat, pt.x, pt.y);
					continue;
				}
			}
			if (!stricmp(att_name, "translation") ) {
				if (gf_node_get_attribute_by_tag((GF_Node *)elt, TAG_SVG_ATT_transform, GF_TRUE, GF_TRUE, &info)==GF_OK) {
					SVG_Point pt;
					SVG_Transform *mat = (SVG_Transform *)info.far_ptr;
					svg_parse_point(&pt, att->value);
					gf_mx2d_add_translation(&mat->mat, pt.x, pt.y);
					continue;
				}
			}
		}

		/* For all attributes of the form 'on...', like 'onclick' we create a listener for the event on the current element,
		   we connect the listener to a handler that contains the code in the 'on...' attribute. */
		/* CHECK: we should probably check the namespace of the attribute and of the element here */
		if (!strncmp(att_name, "on", 2)) {
			u32 evtType = gf_dom_event_type_by_name(att_name + 2);
			if (evtType != GF_EVENT_UNKNOWN) {
				SVG_handlerElement *handler = gf_dom_listener_build((GF_Node *) elt, evtType, 0);
				gf_dom_add_text_node((GF_Node *)handler, gf_strdup(att->value) );
				gf_node_init((GF_Node *)handler);
				continue;
			}
			svg_report(parser, GF_OK, "Skipping unknown event handler %s on node %s", att->name, name);
		}

		/* end of special cases of attributes */

		/* General attribute creation and parsing */
		if (gf_node_get_attribute_by_name((GF_Node *)elt, att_name, ns, GF_TRUE, GF_FALSE, &info)==GF_OK) {
#ifndef SKIP_ATTS_PARSING
			GF_Err e = gf_svg_parse_attribute((GF_Node *)elt, &info, att->value, 0);
			if (e) {
				svg_report(parser, e, "Error parsing attribute %s on node %s", att->name, name);
				continue;
			}
			if (info.fieldType == SVG_ID_datatype) {
				/*"when both 'id' and 'xml:id'  are specified on the same element but with different values,
				the SVGElement::id field must return either of the values but should give precedence to
				the 'xml:id'  attribute."*/
				if (!node_name || (info.fieldIndex == TAG_XML_ATT_id)) {
					node_name = *(SVG_ID *)info.far_ptr;
					/* Check if ID start with a digit, which is not a valid ID for a node according to XML (see http://www.w3.org/TR/xml/#id) */
					if (isdigit(node_name[0])) {
						svg_report(parser, GF_BAD_PARAM, "Invalid value %s for node %s %s", node_name, name, att->name);
						node_name = NULL;
					}
				}
			} else {
				switch (info.fieldIndex) {
				case TAG_SVG_ATT_syncMaster:
				case TAG_SVG_ATT_focusHighlight:
				case TAG_SVG_ATT_initialVisibility:
				case TAG_SVG_ATT_fullscreen:
				case TAG_SVG_ATT_requiredFonts:
					/*switch LASeR Configuration to v2 because these attributes are not part of v1*/
					svg_lsr_set_v2(parser);
					break;
				}
			}
#endif
			continue;
		}

		/* all other attributes (??? failed to be created) should fall in this category */
		svg_report(parser, GF_OK, "Skipping attribute %s on node %s", att->name, name);
	}

	/* When a handler element specifies the event attribute, an implicit listener is defined */
	if (ev_event) {
		GF_Node *node = (GF_Node *)elt;
		SVG_Element *listener;
		u32 type;
		GF_FieldInfo info;
		listener = (SVG_Element *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
		/*We don't want to insert the implicit listener in the DOM. However remember
		the listener at the handler level in case the handler gets destroyed*/
		gf_node_set_private(node, (GF_Node*)listener );
		gf_node_register((GF_Node*)listener, NULL);

		/* this listener listens to the given type of event */
		type = gf_dom_event_type_by_name(ev_event);
		gf_node_get_attribute_by_tag(node, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
		((XMLEV_Event *)info.far_ptr)->type = type;
		gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
		((XMLEV_Event *)info.far_ptr)->type = type;
		gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_handler, GF_TRUE, GF_FALSE, &info);
		((XMLRI *)info.far_ptr)->target = node;

		if (ev_observer) {
			/* An observer was specified, so it needs to be used */
			gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_observer, GF_TRUE, GF_FALSE, &info);
			gf_svg_parse_attribute((GF_Node *)elt, &info, (char*)ev_observer, 0);
		} else {
			/* No observer specified, this listener listens with the parent of the handler as the event target */
			gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_target, GF_TRUE, GF_FALSE, &info);
			((XMLRI *)info.far_ptr)->target = parent->node;
		}
		/* if the target was found (already parsed), we are fine, otherwise we need to try to find it again,
		   we place the listener in the defered listener list */
		if ( ((XMLRI *)info.far_ptr)->target)
			gf_node_dom_listener_add(((XMLRI *)info.far_ptr)->target, (GF_Node *) listener);
		else
			gf_list_add(parser->defered_listeners, listener);
	}

	/* if the new element has an id, we try to resolve defered references (including animations, href and listeners (just above)*/
	if (node_name) {
		if (!has_id) {
			/* if the element was already created before this call, we don't need to get a numerical id, we have it already */
			gf_svg_parse_element_id((GF_Node *)elt, node_name, parser->command_depth ? GF_TRUE : GF_FALSE);
		}
		svg_resolved_refs(parser, parser->load->scene_graph, node_name);
	}

	/* if the new element is an animation, now that all specified attributes have been found,
	   we can start parsing them */
	if (anim) {
		/*FIXME - we need to parse from/to/values but not initialize the stack*/
//		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
		needs_init = GF_FALSE;
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
	if (needs_init) {
		/* For elements that need it, we initialize the rendering stack */
		gf_node_init((GF_Node *)elt);
	}
#endif

	if (parent && elt) {
		/*mark parent element as dirty (new child added) and invalidate parent graph for progressive rendering*/
		gf_node_dirty_set((GF_Node *)parent->node, GF_SG_CHILD_DIRTY, GF_TRUE);
		/*request scene redraw*/
		if (parser->load->scene_graph->NodeCallback) {
			parser->load->scene_graph->NodeCallback(parser->load->scene_graph->userpriv, GF_SG_CALLBACK_MODIFIED, NULL, NULL);
		}
	}

	/*If we are in playback mode, we register (reference counting for safe deleting) the listener element with the element that uses it */
	if ((parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) && elt && (tag==TAG_SVG_listener)) {
		GF_FieldInfo info;
		Bool post_pone = GF_FALSE;
		SVG_Element *par = NULL;
		SVG_Element *listener = (SVG_Element *)elt;

		if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_observer, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			XMLRI *observer = (XMLRI *)info.far_ptr;
			if (observer->type == XMLRI_ELEMENTID) {
				if (!observer->target) post_pone = GF_TRUE;
				else par = (SVG_Element *)observer->target;
			}
		}

		if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_target, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			XMLRI *target = (XMLRI *)info.far_ptr;
			if (!par && (target->type == XMLRI_ELEMENTID)) {
				if (!target->target) post_pone = GF_TRUE;
				else par = (SVG_Element *)target->target;
			}
		}
		/*check handler, create it if not specified*/
		if (parent && (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_handler, GF_TRUE, GF_FALSE, &info) == GF_OK)) {
			XMLRI *handler = (XMLRI *)info.far_ptr;
			if (!handler->target) {
				if (!handler->string) handler->target = parent->node;
			}
		}
		/*if event is a key event, register it with root*/
		if (!par && gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			XMLEV_Event *ev = (XMLEV_Event *)info.far_ptr;
			if ((ev->type>=GF_EVENT_KEYUP) && (ev->type<=GF_EVENT_TEXTINPUT)) par = (SVG_Element*) listener->sgprivate->scenegraph->RootNode;
		}

		if (post_pone) {
			gf_list_add(parser->defered_listeners, listener);
		} else {
			if (!par && parent) par = parent->node;
			gf_node_dom_listener_add((GF_Node *)par, (GF_Node *) listener);
		}
	}
	return elt;
}


static GF_Err lsr_parse_command(GF_SVG_Parser *parser, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_FieldInfo info;
	GF_Node *opNode;
	Bool is_replace = GF_FALSE;
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
				if (gf_node_get_attribute_by_name(parser->command->node, atAtt, parser->current_ns, GF_FALSE, GF_FALSE, &info)==GF_OK) {
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
		is_replace = GF_TRUE;
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
			field->fieldType = DOM_String_datatype;
			gf_node_register(parser->command->node, NULL);
			if (atValue) {
				field->field_ptr = gf_svg_create_attribute_value(field->fieldType);
				*(SVG_String *)field->field_ptr = gf_strdup(atValue);
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
			else /*if (!strcmp(atAtt, "rotation")) */ {
				info.fieldType = SVG_Transform_Rotate_datatype;
				info.fieldIndex = TAG_LSR_ATT_rotation;
			}
		} else {
			/*FIXME - handle namespace properly here !!*/
			info.fieldIndex = gf_xml_get_attribute_tag(parser->command->node, atAtt, (strchr(atAtt, ':')==NULL) ? parser->current_ns : 0);
			info.fieldType = gf_xml_get_attribute_type(info.fieldIndex);

#ifndef GPAC_DISABLE_LASER
			/*
						if (gf_lsr_anim_type_from_attribute(info.fieldIndex)<0) {
							return svg_report(parser, GF_BAD_PARAM, "Attribute %s of element %s is not updatable\n", atAtt, gf_node_get_class_name(parser->command->node));
						}
			*/
#endif /*GPAC_DISABLE_LASER*/
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
			if (field->field_ptr) {
				gf_svg_parse_attribute(parser->command->node, &nf, atValue, (u8) info.fieldType);
				if (info.fieldType==XMLRI_datatype)
					svg_process_media_href(parser, parser->command->node, (XMLRI*)field->field_ptr);
			}
		} else if (opNode) {
			parser->command->fromNodeID = gf_node_get_id(opNode);
			/*FIXME - handle namespace properly here !!*/
			parser->command->fromFieldIndex = gf_xml_get_attribute_tag(opNode, atOperandAtt, parser->current_ns);
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
				parser->command->send_event_string = gf_strdup(atString);
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
	else if (!strcmp(name, "Replace")) return GF_SG_LSR_REPLACE;
	else if (!strcmp(name, "Delete")) return GF_SG_LSR_DELETE;
	else if (!strcmp(name, "Insert")) return GF_SG_LSR_INSERT;
	else if (!strcmp(name, "Restore")) return GF_SG_LSR_RESTORE;
	else if (!strcmp(name, "Save")) return GF_SG_LSR_SAVE;
	else if (!strcmp(name, "Clean")) return GF_SG_LSR_CLEAN;
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
		for (i=0; i<nb_attributes; i++) {
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
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_SCENE_LASER;
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
	u32 xmlns;
	Bool has_ns;

#ifdef SKIP_ALL
	return;
#endif

	parent = (SVG_NodeStack *)gf_list_last(parser->node_stack);

	/*switch to conditional*/
	if (parent && parent->node->sgprivate->tag==TAG_LSR_conditional) {
		cond = parent->node;
		parent = NULL;
	}

	/* If the loader was created with the DIMS type and this is the root element, restore the stream and AU
	context - in DIMS? we only have one stream an dcommands are stacked in the last AU of the stream*/
	if (!parent && (parser->load->type == GF_SM_LOAD_DIMS) && parser->load->ctx) {

		/*if not created, do it now*/
		if (!gf_list_count(parser->load->ctx->streams)) {
			parser->laser_es = gf_sm_stream_new(parser->load->ctx, 1, GF_STREAM_SCENE, GPAC_OTI_SCENE_DIMS);
			parser->laser_es->timeScale = 1000;
			/* Create a default AU to behave as other streams (LASeR, BIFS)
			   but it is left empty, there is no notion of REPLACE Scene or NEw Scene,
			   the RAP is the graph */
			parser->laser_au = gf_sm_stream_au_new(parser->laser_es, 0, 0, GF_TRUE);
		} else {
			parser->laser_es = gf_list_get(parser->load->ctx->streams, 0);
			parser->laser_au = gf_list_last(parser->laser_es->AUs);
		}
	}

	/*saf setup*/
	if ((!parent && (parser->load->type!=GF_SM_LOAD_SVG)) || cond) {
		u32 com_type;
		Bool has_ns=GF_FALSE;
		svg_check_namespace(parser, attributes, nb_attributes, &has_ns);

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
			u32 time, i;
			Bool rap;
			time = 0;
			rap =  GF_FALSE;
			if (!gf_list_count(parser->laser_es->AUs)) rap = GF_TRUE;
			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) rap = !strcmp(att->value, "yes") ? GF_TRUE : GF_FALSE;
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
			SVG_SAFExternalStream*st;
			/*create a SAF stream*/
			if (!parser->saf_es) {
				GF_ESD *esd = (GF_ESD*)gf_odf_desc_esd_new(2);
				esd->ESID = 0xFFFE;
				esd->decoderConfig->streamType = GF_STREAM_OD;
				esd->decoderConfig->objectTypeIndication = 1;
				parser->saf_es = gf_sm_stream_new(parser->load->ctx, esd->ESID, GF_STREAM_OD, GPAC_OTI_OD_V1);
				if (!parser->load->ctx->root_od) parser->load->ctx->root_od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_IOD_TAG);
				parser->saf_es->timeScale = 1000;
				gf_list_add(parser->load->ctx->root_od->ESDescriptors, esd);
			}
			time = 0;
			ts_res = 1000;
			OTI = ST = 0;
			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "time")) time = atoi(att->value);
				else if (!strcmp(att->name, "rap")) ;//rap = !strcmp(att->value, "yes") ? 1 : 0;
				else if (!strcmp(att->name, "url")) url = gf_strdup(att->value);
				else if (!strcmp(att->name, "streamID")) ID = att->value;
				else if (!strcmp(att->name, "objectTypeIndication")) OTI = atoi(att->value);
				else if (!strcmp(att->name, "streamType")) ST = atoi(att->value);
				else if (!strcmp(att->name, "timeStampResolution")) ts_res = atoi(att->value);
				else if (!strcmp(att->name, "source")) src = att->value;

			}
			if (!strcmp(name, "imageHeader")) ST = 4;

			/*create new SAF stream*/
			st = svg_saf_get_next_available_stream(parser);
			st->stream_name = gf_strdup(ID);

			/*create new SAF unit*/
			parser->saf_au = gf_sm_stream_au_new(parser->saf_es, time, 0, GF_FALSE);
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
					mux->file_name = gf_strdup(src);
					st->nhml_info = NULL;
				} else {
					if (parser->load->localPath) {
						strcpy(szName, parser->load->localPath);
						strcat(szName, "/");
						strcat(szName, ID ? ID : "");
					} else {
						strcpy(szName, ID ? ID : "");
					}
					strcat(szName, "_temp.nhml");
					mux->file_name = gf_strdup(szName);
					st->nhml_info = mux->file_name;
					nhml = gf_fopen(st->nhml_info, "wt");
					if (nhml) {
						fprintf(nhml, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
						fprintf(nhml, "<NHNTStream version=\"1.0\" timeScale=\"%d\" streamType=\"%d\" objectTypeIndication=\"%d\" inRootOD=\"no\" trackID=\"%d\">\n", ts_res, ST, OTI, st->id);
						gf_fclose(nhml);
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[LASeR Parser] Error opening nhml file %s while preparing import\n", st->nhml_info));
					}
					mux->delete_file = GF_TRUE;
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
			for (i=0; i<nb_attributes; i++) {
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
			nhml = gf_fopen(st->nhml_info, "a+t");
			fprintf(nhml, "<NHNTSample ");
			if (time) fprintf(nhml, "DTS=\"%d\" ", time);
			if (length) fprintf(nhml, "dataLength=\"%d\" ", length);
			if (offset) fprintf(nhml, "mediaOffset=\"%d\" ", offset);
			if (rap) fprintf(nhml, "isRAP=\"yes\" ");
			if (src) fprintf(nhml, "mediaFile=\"%s\" ", src);
			fprintf(nhml, "/>\n");
			gf_fclose(nhml);
			return;
		}
		if (!strcmp(name, "endOfStream") ) {
			FILE *nhml;
			char *id = NULL;
			u32 i;
			SVG_SAFExternalStream*st;
			for (i=0; i<nb_attributes; i++) {
				GF_XMLAttribute *att = (GF_XMLAttribute *) &attributes[i];
				if (!strcmp(att->name, "ref")) id = att->value;
			}
			st = svg_saf_get_stream(parser, 0, id);
			if (!st || !st->nhml_info) {
				return;
			}
			nhml = gf_fopen(st->nhml_info, "a+t");
			fprintf(nhml, "</NHNTStream>\n");
			gf_fclose(nhml);
			return;
		}

		if (!strcmp(name, "endOfSAFSession") ) {
			return;
		}

		if ((parser->load->type==GF_SM_LOAD_XSR) && !parser->laser_au && !cond) {
			if (parser->load->flags & GF_SM_LOAD_CONTEXT_READY) {
				assert(parser->laser_es);
				parser->laser_au = gf_sm_stream_au_new(parser->laser_es, 0, 0, GF_FALSE);
			} else {
				svg_report(parser, GF_BAD_PARAM, "LASeR sceneUnit not defined for command %s", name);
				return;
			}
		}
		/*command parsing*/
		com_type = lsr_get_command_by_name(name);
		if (com_type != GF_SG_UNDEFINED) {
			SVG_NodeStack *top;
			GF_Err e;
			parser->command = gf_sg_command_new(parser->load->scene_graph, com_type);

			/*this is likely a conditional start - update unknown depth level*/
			top = (SVG_NodeStack*)gf_list_last(parser->node_stack);
			if (top) {
				top->unknown_depth ++;
				parser->command_depth++;
			}

			e = lsr_parse_command(parser, attributes, nb_attributes);
			if (e!= GF_OK) {
				parser->command->node = NULL;
				gf_sg_command_del(parser->command);
				parser->command = NULL;
				GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[LASeR Parser] Error parsing %s command - skipping\n", (parser->load->type==GF_SM_LOAD_XSR) ? "LASeR" : "DIMS"));
				return;
			}

			if (cond) {
				GF_DOMUpdates *up = cond->children ? (GF_DOMUpdates *)cond->children->node : NULL;
				if (!up) {
					up = gf_dom_add_updates_node((GF_Node*)cond);

					if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
						gf_node_set_callback_function((GF_Node*)up, xsr_exec_command_list);
					}
				}
				gf_list_add(up->updates, parser->command);
			} else if (parser->laser_au) {
				gf_list_add(parser->laser_au->commands, parser->command);
			}
			switch (com_type) {
			case GF_SG_LSR_NEW_SCENE:
			case GF_SG_LSR_REFRESH_SCENE:
				if (parser->laser_au) parser->laser_au->flags |= GF_SM_AU_RAP;
				break;
			}

			return;
		}
	}

	if (!parent && !parser->command && (parser->load->flags & GF_SM_LOAD_CONTEXT_STREAMING)) {
		gf_sg_reset(parser->load->scene_graph);
		parser->has_root = 0;
	}

	/*something not supported happened (bad command name, bad root, ...) */
	if ((parser->has_root==1) && !parent && !parser->command)
		return;

	xmlns = parser->current_ns;
	has_ns = GF_FALSE;

	elt = svg_parse_element(parser, name, name_space, attributes, nb_attributes, parent, &has_ns);
	if (!elt) {
		if (parent) parent->unknown_depth++;
		else if (cond) parser->command_depth++;
		return;
	}
	GF_SAFEALLOC(stack, SVG_NodeStack);
	if (!stack) {
		parser->last_error = GF_OUT_OF_MEM;
		return;
	}
	stack->node = elt;
	stack->current_ns = xmlns;
	stack->has_ns = has_ns;
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
	} else if (!parser->has_root ) {
		gf_list_del_item(parser->node_stack, stack);
		gf_free(stack);
		gf_node_unregister((GF_Node *)elt, NULL);
	} else if ((parser->has_root==2) && !parser->fragment_root) {
		parser->fragment_root = (GF_Node *)elt;
	}
}

static void svg_node_end(void *sax_cbck, const char *name, const char *name_space)
{
#ifdef SKIP_UNKNOWN_NODES
	u32 ns;
#endif
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
				if (parser->load->type==GF_SM_LOAD_DIMS && parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
					gf_sg_command_apply(parser->load->scene_graph, parser->command, 0);
					gf_sg_command_del(parser->command);
				}
				parser->command = NULL;
				return;
			}
		}
		/*error*/
		return;
	}

#ifdef SKIP_UNKNOWN_NODES
	ns = parser->current_ns;
	if (name_space)
		ns = gf_sg_get_namespace_code(parser->load->scene_graph, (char *) name_space);

	/*only remove created nodes ... */
	if (gf_xml_get_element_tag(name, ns) != TAG_UndefinedNode)
#endif
	{
		const char *the_name;
		Bool mismatch = GF_FALSE;
		SVG_Element *node = top->node;
		/*check node name...*/
		the_name = gf_node_get_class_name((GF_Node *)node);
		if (name_space && strstr(the_name, name_space) && strstr(the_name, name) ) {}
		else if (!strcmp(the_name, name) ) {}
		else mismatch = GF_TRUE;

		if (mismatch) {
			if (top->unknown_depth) {
				top->unknown_depth--;
				return;
			} else {
				svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch: expecting </%s> got </%s>", the_name, name);
				return;
			}
		}
		parser->current_ns = top->current_ns;
		if (top->has_ns) gf_xml_pop_namespaces(top->node);
		gf_free(top);
		gf_list_rem_last(parser->node_stack);

		if (parser->load->flags & GF_SM_LOAD_FOR_PLAYBACK) {
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
			/*if we have associated event listeners, trigger the onLoad, only in playback mode */
			if (node->sgprivate->interact && node->sgprivate->interact->dom_evt) {
				GF_DOM_Event evt;
				memset(&evt, 0, sizeof(GF_DOM_Event));
				evt.type = GF_EVENT_LOAD;
				gf_dom_event_fire((GF_Node*)node, &evt);
			}

		}
	}
#ifdef SKIP_UNKNOWN_NODES
	else if (top) {
		if (top->unknown_depth) {
			top->unknown_depth--;
			if (!top->unknown_depth) {
				/*this is not 100% correct, we should track which unsupported node changed the namespace*/
				parser->current_ns = top->current_ns;
				if (parser->command_depth) parser->command_depth --;
			}
		} else if (parser->command_depth) {
			parser->command_depth--;
		} else {
			svg_report(parser, GF_BAD_PARAM, "SVG depth mismatch");
		}
	}
#endif
}

static void svg_text_content(void *sax_cbck, const char *text_content, Bool is_cdata)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)sax_cbck;
	SVG_NodeStack *top = (SVG_NodeStack *)gf_list_last(parser->node_stack);
	SVG_Element *elt = (top ? top->node : NULL);
	GF_DOMText *text;
	Bool skip_text;
	u32 tag;

#ifdef SKIP_ALL
	return;
#endif

	if (top && top->unknown_depth && !parser->command_depth) return;
	if (!elt && !parser->command) return;

	tag = (elt ? gf_node_get_tag((GF_Node *)elt) : 0);

	/*if top is a conditional, create a new text node*/
	if (!elt || tag == TAG_LSR_conditional) {
		GF_CommandField *field;
		if (!parser->command) return;

		field = (GF_CommandField *)gf_list_get(parser->command->command_fields, 0);
		if (parser->command->tag == GF_SG_LSR_NEW_SCENE || parser->command->tag == GF_SG_LSR_ADD) return;

		if (!field || field->field_ptr) return;

		if (field->new_node) {
			svg_report(parser, GF_OK, "Warning: LASeR cannot replace children with a mix of text nodes and elements - ignoring text\n");
			return;
		}

		/*create a text node but don't register it with any node*/
		text = gf_dom_new_text_node(parser->load->scene_graph);
		gf_node_register((GF_Node *)text, NULL);
		text->textContent = gf_strdup(text_content);

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
	skip_text = GF_TRUE;
	switch (tag) {
	case TAG_DOMFullNode:
	case TAG_SVG_a:
	case TAG_SVG_title:
	case TAG_SVG_desc:
	case TAG_SVG_metadata:
	case TAG_SVG_text:
	case TAG_SVG_tspan:
	case TAG_SVG_textArea:
		skip_text = GF_FALSE;
		break;
	/*for script and handlers only add text if not empty*/
	case TAG_SVG_handler:
	case TAG_SVG_script:
	{
		u32 i, len = (u32) strlen(text_content);
		for (i=0; i<len; i++) {
			if (!strchr(" \n\r\t", text_content[i])) {
				skip_text = GF_FALSE;
				break;
			}
		}
	}
	break;
	}

	if (!skip_text) {
		text = gf_dom_add_text_node((GF_Node *)elt, gf_strdup(text_content));
		text->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
		gf_node_changed((GF_Node *)text, NULL);
	}
}

static GF_SVG_Parser *svg_new_parser(GF_SceneLoader *load)
{
	GF_SVG_Parser *parser;
	switch (load->type) {
	case GF_SM_LOAD_XSR:
		if (!load->ctx) return NULL;
		break;
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_DIMS:
		break;
	default:
		return NULL;
	}

	GF_SAFEALLOC(parser, GF_SVG_Parser);
	if (!parser) {
		return NULL;
	}
	parser->node_stack = gf_list_new();
	parser->defered_hrefs = gf_list_new();
	parser->defered_animations = gf_list_new();
	parser->defered_listeners = gf_list_new();
	parser->peeked_nodes = gf_list_new();

	parser->sax_parser = gf_xml_sax_new(svg_node_start, svg_node_end, svg_text_content, parser);
	parser->load = load;
	load->loader_priv = parser;
	if (load->ctx) load->ctx->is_pixel_metrics = GF_TRUE;

	/*to cope with old files not signaling XMLNS, add the SVG NS by default*/
	gf_sg_add_namespace(parser->load->scene_graph, "http://www.w3.org/2000/svg", NULL);
	parser->current_ns = GF_XMLNS_SVG;
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

static void gf_sm_svg_flush_state(GF_SVG_Parser *parser)
{
	while (gf_list_count(parser->node_stack)) {
		SVG_NodeStack *st = (SVG_NodeStack *)gf_list_last(parser->node_stack);
		gf_list_rem_last(parser->node_stack);
		gf_free(st);
	}
	/*FIXME - if there still are som defered listeners, we should pass them to the scene graph
	and wait for the parent to be defined*/
	while (gf_list_count(parser->defered_listeners)) {
		GF_Node *l = (GF_Node *)gf_list_last(parser->defered_listeners);
		gf_list_rem_last(parser->defered_listeners);
		/*listeners not resolved are not inserted in the tree - destroy them*/
		gf_node_register(l, NULL);
		gf_node_unregister(l, NULL);
	}
}

static GF_Err gf_sm_load_initialize_svg(GF_SceneLoader *load, const char *str_data, Bool is_fragment)
{
	GF_Err e;
	GF_SVG_Parser *parser;

	if (str_data) {
		char BOM[6];
		BOM[0] = str_data[0];
		BOM[1] = str_data[1];
		BOM[2] = str_data[2];
		BOM[3] = str_data[3];
		BOM[4] = BOM[5] = 0;
		parser = svg_new_parser(load);
		if (!parser) return GF_BAD_PARAM;

		if (is_fragment) parser->has_root = 2;
		e = gf_xml_sax_init(parser->sax_parser, (unsigned char*)BOM);
		if (e) {
			svg_report(parser, e, "Error initializing SAX parser: %s", gf_xml_sax_get_error(parser->sax_parser) );
			return e;
		}
		str_data += 4;

	} else if (load->fileName) {
		parser = svg_new_parser(load);
		if (!parser) return GF_BAD_PARAM;
	} else {
		return GF_BAD_PARAM;
	}

	/*chunk parsing*/
	if (load->flags & GF_SM_LOAD_CONTEXT_READY) {
		u32 i;
		GF_StreamContext *sc;
		if (!load->ctx) return GF_BAD_PARAM;

		/*restore context - note that base layer are ALWAYS declared BEFORE enhancement layers with gpac parsers*/
		i=0;
		while ((sc = (GF_StreamContext*)gf_list_enum(load->ctx->streams, &i))) {
			switch (sc->streamType) {
			case GF_STREAM_SCENE:
				if (!parser->laser_es) parser->laser_es = sc;
				break;
			default:
				break;
			}
		}
		/*need at least one scene stream - FIXME - accept SVG as root? */
		if (!parser->laser_es) return GF_BAD_PARAM;
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("SVG: MPEG-4 LASeR / DIMS Scene Chunk Parsing"));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[Parser] %s Scene Parsing: %s\n", ((load->type==GF_SM_LOAD_SVG) ? "SVG" : ((load->type==GF_SM_LOAD_XSR) ? "LASeR" : "DIMS")), load->fileName));
	}

	if (str_data)
		return gf_xml_sax_parse(parser->sax_parser, str_data);

	return GF_OK;
}


GF_Err load_svg_run(GF_SceneLoader *load)
{
	u32 in_time;
	GF_Err e;
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;

	if (!parser) {
		e = gf_sm_load_initialize_svg(load, NULL, GF_FALSE);
		if (e) return e;
		parser = (GF_SVG_Parser *)load->loader_priv;
	}

	in_time = gf_sys_clock();
	e = gf_xml_sax_parse_file(parser->sax_parser, (const char *)load->fileName, svg_progress);
	if (e<0) return svg_report(parser, e, "Unable to parse file %s: %s", load->fileName, gf_xml_sax_get_error(parser->sax_parser) );
	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("[Parser] Scene parsed and Scene Graph built in %d ms\n", gf_sys_clock() - in_time));

	svg_flush_animations(parser);
	gf_sm_svg_flush_state(parser);
	return e;

}

static void load_svg_done(GF_SceneLoader *load)
{
	SVG_SAFExternalStream *st;
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;
	if (!parser) return;

	gf_sm_svg_flush_state(parser);

	gf_list_del(parser->node_stack);
	gf_list_del(parser->defered_hrefs);
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
		gf_free(st->stream_name);
		gf_free(st);
		st = next;
	}
	gf_free(parser);
	load->loader_priv = NULL;
}

GF_Err load_svg_parse_string(GF_SceneLoader *load, const char *str)
{
	GF_Err e;
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;

	if (!parser) {
		e = gf_sm_load_initialize_svg(load, str, GF_FALSE);
		parser = (GF_SVG_Parser *)load->loader_priv;
	} else {
		e = gf_xml_sax_parse(parser->sax_parser, str);
	}
	if (e<0) svg_report(parser, e, "Unable to parse chunk: %s", gf_xml_sax_get_error(parser->sax_parser) );
	else e = parser->last_error;

	svg_flush_animations(parser);

	/*if error move to done state and wait for next XML chunk*/
	if (e) load_svg_done(load);

	return e;
}

static GF_Err load_svg_suspend(GF_SceneLoader *load, Bool suspend)
{
	GF_SVG_Parser *parser = (GF_SVG_Parser *)load->loader_priv;
	if (parser) gf_xml_sax_suspend(parser->sax_parser, suspend);
	return GF_OK;
}


GF_Err gf_sm_load_init_svg(GF_SceneLoader *load)
{
	load->process = load_svg_run;
	load->done = load_svg_done;
	load->parse_string = load_svg_parse_string;
	load->suspend = load_svg_suspend;
	return GF_OK;

}

GF_Node *gf_sm_load_svg_from_string(GF_SceneGraph *in_scene, char *node_str)
{
	GF_Err e;
	GF_SVG_Parser *parser;
	GF_Node *node;
	GF_SceneLoader ctx;
	memset(&ctx, 0, sizeof(GF_SceneLoader));
	ctx.scene_graph = in_scene;
	ctx.type = GF_SM_LOAD_SVG;

	e = gf_sm_load_initialize_svg(&ctx, node_str, GF_TRUE);

	parser = (GF_SVG_Parser *)ctx.loader_priv;
	node = parser->fragment_root;

	if (e != GF_OK) {
		if (parser->fragment_root) gf_node_unregister(parser->fragment_root, NULL);
		parser->fragment_root=NULL;
		load_svg_done(&ctx);
		return NULL;
	}

	/*don't register*/
	if (node) node->sgprivate->num_instances--;
	load_svg_done(&ctx);
	return node;
}

#endif

