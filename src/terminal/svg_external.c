/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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
#include <gpac/nodes_svg.h>
#include <gpac/compositor.h>
#include <gpac/network.h>


char *gf_term_resolve_xlink(GF_Node *node, char *the_url)
{
	char *url;
	GF_Scene *scene = gf_sg_get_private(gf_node_get_graph(node));
	if (!scene) return NULL;

	url = gf_strdup(the_url);
	/*apply XML:base*/
	while (node) {
		GF_FieldInfo info;
		if (gf_node_get_attribute_by_tag(node, TAG_XML_ATT_base, 0, 0, &info)==GF_OK) {
			char *new_url = gf_url_concatenate( ((XMLRI*)info.far_ptr)->string, url);
			if (new_url) {
				gf_free(url);
				url = new_url;
			}
		}
		node = gf_node_get_parent(node, 0);
	}

	/*if this is a fragment and no XML:BASE was found, this is a fragment of the current document*/
	if (url[0]=='#') return url;

	if (scene) {
		char *the_url;
		if (scene->redirect_xml_base) {
			the_url = gf_url_concatenate(scene->redirect_xml_base, url);
		} else {
//			the_url = gf_url_concatenate(is->root_od->net_service->url, url);
			/*the root url of a document should be "." if not specified, so that the final URL resolve happens only once
			at the service level*/
			the_url = gf_strdup(url);
		}
		gf_free(url);
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
	GF_Scene *scene = gf_sg_get_private(gf_node_get_graph(node));
	if (!scene) return GF_BAD_PARAM;

	gf_sg_vrml_mf_reset(mfurl, GF_SG_VRML_MFURL);

	e = gf_node_get_attribute_by_tag(node, TAG_XLINK_ATT_href, 0, 0, &info);
	if (e) return e;

	iri = (XMLRI*)info.far_ptr;

	if (iri->type==XMLRI_STREAMID) {
		stream_id = iri->lsr_stream_id;
	} else if (!iri->string) return GF_OK;

	mfurl->count = 1;
	GF_SAFEALLOC(mfurl->vals, SFURL)
	sfurl = mfurl->vals;
	if (!sfurl) return GF_BAD_PARAM;
	sfurl->OD_ID = stream_id;
	if (stream_id) return GF_OK;

	if (!strncmp(iri->string, "data:", 5)) {
		const char *cache_dir = gf_cfg_get_key(scene->root_od->term->user->config, "General", "CacheDirectory");
		e = gf_node_store_embedded_data(iri, cache_dir, "embedded_");
		if (e) return e;
		sfurl->url = gf_strdup(iri->string);
		return GF_OK;
	}
	sfurl->url = gf_term_resolve_xlink(node, iri->string);
	return e;
}


/* Creates a subscene from the xlink:href */
static GF_Scene *gf_svg_get_subscene(GF_Node *elt, XLinkAttributesPointers *xlinkp, SMILSyncAttributesPointers *syncp, Bool use_sync, Bool primary_resource)
{
	MFURL url;
	Bool lock_timelines = 0;
	GF_MediaObject *mo;
	GF_SceneGraph *graph = gf_node_get_graph(elt);
	GF_Scene *scene = (GF_Scene *)gf_sg_get_private(graph);
	if (!scene) return NULL;

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

	while (scene->secondary_resource && scene->root_od->parentscene)
		scene = scene->root_od->parentscene;

	mo = gf_scene_get_media_object_ex(scene, &url, GF_MEDIA_OBJECT_SCENE, lock_timelines, NULL, primary_resource, elt);
	gf_sg_vrml_mf_reset(&url, GF_SG_VRML_MFURL);

	if (!mo || !mo->odm) return NULL;
	mo->odm->subscene->secondary_resource = primary_resource ? 0 : 1;
	return mo->odm->subscene;
}

GF_EXPORT
GF_MediaObject *gf_mo_load_xlink_resource(GF_Node *node, Bool primary_resource, Double clipBegin, Double clipEnd)
{
	GF_Scene *new_resource;
	SVGAllAttributes all_atts;
	XLinkAttributesPointers xlinkp;
	SMILSyncAttributesPointers syncp;
	GF_Scene *scene = gf_sg_get_private(gf_node_get_graph(node));
	if (!scene) return NULL;

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
//	else if (xlinkp.href->string && (xlinkp.href->string[0]=='#')) return NULL;

	new_resource = gf_svg_get_subscene(node, &xlinkp, &syncp, primary_resource ? 1 : 0, primary_resource);
	if (!new_resource) return NULL;

	/*play*/
	gf_mo_play(new_resource->root_od->mo, 0, -1, 0);

	return new_resource->root_od->mo;
}

GF_EXPORT
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
		if (!mo->num_open) {
			/*do we simply stop the associated document or unload it??? to check*/
//			gf_mo_stop(mo);
			gf_odm_disconnect(mo->odm, 2);
			return;
		}
	}

	/*ODM may be destroyed at this point !!*/
	if (mo->odm) gf_odm_lock(mo->odm, 0);
}

#endif //GPAC_DISABLE_SVG

