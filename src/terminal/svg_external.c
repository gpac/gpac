/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Rendering sub-project
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

#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/mediaobject.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg_da.h>
#include <gpac/compositor.h>


char *gf_term_resolve_xlink(GF_Node *node, char *the_url)
{
	char *url;
	GF_InlineScene *is = gf_sg_get_private(gf_node_get_graph(node));
	if (!is) return NULL;

	url = strdup(the_url);
	/*apply XML:base*/
	while (node) {
		GF_FieldInfo info;
		if (gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xml_base, 0, 0, &info)==GF_OK) {
			char *new_url = gf_url_concatenate( ((XMLRI*)info.far_ptr)->string, url);
			if (new_url) {
				free(url);
				url = new_url;
			}
		}
		node = gf_node_get_parent(node, 0);
	}

	/*if this is a fragment and no XML:BASE was found, this is a fragment of the current document*/
	if (url[0]=='#') return url;

	if (is) {
		char *the_url = gf_url_concatenate(is->root_od->net_service->url, url);
		free(url);
		return the_url;
	} 
	return url;
}

GF_EXPORT
GF_Err gf_term_get_mfurl_from_xlink(GF_Node *node, MFURL *mfurl)
{
	u32 stream_id = 0;
	GF_Err e = GF_OK;
	SFURL *sfurl = NULL;
	GF_FieldInfo info;
	XMLRI *iri;
	GF_InlineScene *is = gf_sg_get_private(gf_node_get_graph(node));
	if (!is) return GF_BAD_PARAM;

	gf_sg_vrml_mf_reset(mfurl, GF_SG_VRML_MFURL);

	e = gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_xlink_href, 0, 0, &info);
	if (e) return e;

	iri = (XMLRI*)info.far_ptr;

	if (iri->type==XMLRI_STREAMID) {
		stream_id = iri->lsr_stream_id;
	} else if (!iri->string) return GF_OK;

	mfurl->count = 1;
	GF_SAFEALLOC(mfurl->vals, SFURL)
	sfurl = mfurl->vals;
	sfurl->OD_ID = stream_id;
	if (stream_id) return GF_OK;

	if (!strncmp(iri->string, "data:", 5)) {
		const char *cache_dir = gf_cfg_get_key(is->root_od->term->user->config, "General", "CacheDirectory");
		return gf_svg_store_embedded_data(iri, cache_dir, "embedded_");
	}
	sfurl->url = gf_term_resolve_xlink(node, iri->string);
	return e;
}


/* Creates a subscene from the xlink:href */
static GF_InlineScene *gf_svg_get_subscene(GF_Node *elt, XLinkAttributesPointers *xlinkp, SMILSyncAttributesPointers *syncp, Bool use_sync, Bool primary_resource)
{
	MFURL url;
	Bool lock_timelines = 0;
	GF_MediaObject *mo;
	GF_SceneGraph *graph = gf_node_get_graph(elt);
	GF_InlineScene *is = (GF_InlineScene *)gf_sg_get_private(graph);
	if (!is) return NULL;

	if (use_sync && syncp) {
		switch ((syncp->syncBehavior?*syncp->syncBehavior:SMIL_SYNCBEHAVIOR_DEFAULT)) {
		case SMIL_SYNCBEHAVIOR_LOCKED:
		case SMIL_SYNCBEHAVIOR_CANSLIP:
			lock_timelines = 1;
			break;
		case SMIL_SYNCBEHAVIOR_DEFAULT:
		{
#if 0
			if (svg && syncp) {
				switch ((syncp->syncBehaviorDefault ? *syncp->syncBehaviorDefault : SMIL_SYNCBEHAVIOR_LOCKED)) {
				case SMIL_SYNCBEHAVIOR_LOCKED:
				case SMIL_SYNCBEHAVIOR_CANSLIP:
					lock_timelines = 1;
					break;
				default:
					break;
				}
			}
#endif
		}
		default:
			break;
		}
	}
	memset(&url, 0, sizeof(MFURL));
	if (!xlinkp->href) return NULL;

	gf_term_get_mfurl_from_xlink(elt, &url);

	while (is->secondary_resource && is->root_od->parentscene)
		is = is->root_od->parentscene;

	/*
		creates the media object if not already created at the InlineScene level
		TODO FIX ME: do it at the terminal level
	*/
	mo = gf_inline_get_media_object_ex(is, &url, GF_MEDIA_OBJECT_SCENE, lock_timelines, NULL, primary_resource);
	gf_sg_vrml_mf_reset(&url, GF_SG_VRML_MFURL);

	if (!mo || !mo->odm) return NULL;
	mo->odm->subscene->secondary_resource = primary_resource ? 0 : 1;
	return mo->odm->subscene;
}


GF_MediaObject *gf_mo_load_xlink_resource(GF_Node *node, Bool primary_resource, Double clipBegin, Double clipEnd)
{
	GF_InlineScene *new_resource;
	SVGAllAttributes all_atts;
	XLinkAttributesPointers xlinkp;
	SMILSyncAttributesPointers syncp;
	GF_InlineScene *is = gf_sg_get_private(gf_node_get_graph(node)); 
	if (!is) return NULL;

	gf_svg_flatten_attributes((SVG_Element *)node, &all_atts);
	xlinkp.actuate = all_atts.xlink_actuate;
	xlinkp.arcrole = all_atts.xlink_arcrole;
	xlinkp.href = all_atts.xlink_href;
	xlinkp.role = all_atts.xlink_role;
	xlinkp.show = all_atts.xlink_show;
	xlinkp.title = all_atts.xlink_title;
	xlinkp.type = all_atts.xlink_type;
	syncp.syncBehavior = all_atts.syncBehavior;
	syncp.syncBehaviorDefault = all_atts.syncBehaviorDefault;
	syncp.syncMaster = all_atts.syncMaster;
	syncp.syncReference = all_atts.syncReference;
	syncp.syncTolerance = all_atts.syncTolerance;
	syncp.syncToleranceDefault = all_atts.syncToleranceDefault;

	if (!xlinkp.href) return NULL;

	if (xlinkp.href->type == XMLRI_ELEMENTID) return NULL;

	new_resource = gf_svg_get_subscene(node, &xlinkp, &syncp, primary_resource ? 1 : 0, primary_resource);
	if (!new_resource) return NULL;

	/*remember parent node for event propagation*/
	gf_list_add(new_resource->inline_nodes, node);

	/*play*/
	gf_mo_play(new_resource->root_od->mo, 0, -1, 0);

	return new_resource->root_od->mo;	
}

void gf_mo_unload_xlink_resource(GF_Node *node, GF_MediaObject *mo)
{
	if (!mo) return;
	if (!gf_odm_lock_mo(mo)) return;
	if (!mo->odm->subscene) {
		gf_odm_lock(mo->odm, 0);
		return;
	}
	if (mo->num_open) {
		mo->num_open--;
		gf_list_del_item(mo->odm->subscene->inline_nodes, node);
		if (!mo->num_open) {

			/*do we simply stop the associated document or unload it??? to check*/
//			gf_mo_stop(mo);
			gf_odm_disconnect(mo->odm, 1);
		}
	}
	/*ODM may be destroyed at this point !!*/
	if (mo->odm) gf_odm_lock(mo->odm, 0);
}

#endif //GPAC_DISABLE_SVG

