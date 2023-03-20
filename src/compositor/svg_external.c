/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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
#include <gpac/mediaobject.h>

#if !defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/nodes_svg.h>
#include <gpac/compositor.h>
#include <gpac/network.h>

GF_EXPORT
GF_Err gf_sc_get_mfurl_from_xlink(GF_Node *node, MFURL *mfurl)
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
	if (!mfurl->vals) return GF_BAD_PARAM;
	sfurl = mfurl->vals;
	sfurl->OD_ID = stream_id;
	if (stream_id) return GF_OK;

	if (!strncmp(iri->string, "data:", 5)) {
		const char *cache_dir = gf_opts_get_key("core", "cache");
		e = gf_node_store_embedded_data(iri, cache_dir, "embedded_");
		if (e) return e;
		sfurl->url = gf_strdup(iri->string);
		return GF_OK;
	}
	sfurl->url = gf_scene_resolve_xlink(node, iri->string);
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

	gf_sc_get_mfurl_from_xlink(elt, &url);

	while (scene->secondary_resource && scene->root_od->parentscene)
		scene = scene->root_od->parentscene;

	mo = gf_scene_get_media_object_ex(scene, &url, GF_MEDIA_OBJECT_SCENE, lock_timelines, NULL, primary_resource, elt, NULL);
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
	if (!mo || !mo->odm || !mo->odm->subscene) {
		return;
	}

	if (mo->num_open) {
		mo->num_open--;
		if (!mo->num_open) {
			/*do we simply stop the associated document or unload it??? to check*/
//			gf_mo_stop(&mo);
			gf_odm_disconnect(mo->odm, 2);
			return;
		}
	}
}

#endif //!defined(GPAC_DISABLE_SVG) &&  !defined(GPAC_DISABLE_COMPOSITOR)
