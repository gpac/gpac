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
#include <gpac/nodes_svg_sa.h>
#include <gpac/nodes_svg_sani.h>
#include <gpac/nodes_svg_da.h>
#include <gpac/renderer.h>


GF_EXPORT
Bool gf_term_set_mfurl_from_uri(GF_Terminal *term, MFURL *mfurl, SVG_IRI *iri)
{
	Bool ret = 1;
	SFURL *sfurl = NULL;
	if (!iri->iri) return 0;

	gf_sg_vrml_mf_reset(mfurl, GF_SG_VRML_MFURL);
	mfurl->count = 1;
	GF_SAFEALLOC(mfurl->vals, SFURL)
	sfurl = mfurl->vals;
	sfurl->OD_ID = 0;
	if (term && !strncmp(iri->iri, "data:", 5)) {
		const char *cache_dir = gf_cfg_get_key(term->user->config, "General", "CacheDirectory");
		ret = gf_svg_store_embedded_data(iri, cache_dir, "embedded_");
	}
	sfurl->url = strdup(iri->iri);
	return ret;
}

GF_EXPORT
Bool gf_term_check_iri_change(GF_Terminal *term, MFURL *url, SVG_IRI *iri)
{
	if (url->count && !iri->iri) return 1;
	if (!url->count && iri->iri) return 1;
	if (!url->count) return 0;
	if (!strcmp(url->vals[0].url, iri->iri)) return 0;
	return 1;
}


/* Creates a subscene from the xlink:href */
static GF_InlineScene *gf_svg_get_subscene(GF_Node *elt, XLinkAttributesPointers *xlinkp, SMILSyncAttributesPointers *syncp, Bool use_sync)
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
			SVGsvgElement *svg = (SVGsvgElement *) gf_sg_get_root_node(gf_node_get_graph(elt));
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
		}
		default:
			break;
		}
	}
	memset(&url, 0, sizeof(MFURL));
	gf_term_set_mfurl_from_uri(is->root_od->term, &url, xlinkp->href);

	/*
		creates the media object if not already created at the InlineScene level
		TODO FIX ME: do it at the terminal level
	*/
	mo = gf_is_get_media_object(is, &url, GF_MEDIA_OBJECT_SCENE, lock_timelines);
	gf_sg_vrml_mf_reset(&url, GF_SG_VRML_MFURL);

	if (!mo || !mo->odm) return NULL;
	return mo->odm->subscene;
}

void gf_svg_subscene_start(GF_InlineScene *is)
{
	GF_MediaObject *mo = is->root_od->mo;
	if (!mo) return;
	gf_mo_play(mo, 0, 0);
}

void gf_svg_subscene_stop(GF_InlineScene *is, Bool reset_ck)
{
	u32 i;
	GF_ObjectManager *ctrl_od;
	GF_Clock *ck;

	if (!is->root_od->mo->num_open) return;
	
	/* Can we control the timeline of the subscene (e.g. non broadcast ...) */
	if (is->root_od->flags & GF_ODM_NO_TIME_CTRL) return;

	assert(is->root_od->parentscene);
	
	/* Is it the same timeline as the parent scene ? */
	ck = gf_odm_get_media_clock(is->root_od);
	if (!ck) return;
    /* Achtung: unspecified ! do we have the right to stop the parent timeline as well ??? */
	if (gf_odm_shares_clock(is->root_od->parentscene->root_od, ck)) return;
	
	/* stop main subod and all other sub od */
	gf_mo_stop(is->root_od->mo);
	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (ctrl_od->mo->num_open) gf_mo_stop(ctrl_od->mo);
	}
	gf_mo_stop(is->root_od->mo);
	if (reset_ck) 
		gf_clock_reset(ck);
	else
		ck->clock_init = 0;
}


/***********************************
 *  'animation' specific functions *
 ***********************************/

static void svg_animation_smil_update(SMIL_Timing_RTI *rti, Fixed normalized_scene_time)
{
	XLinkAttributesPointers xlinkp;
	SMILSyncAttributesPointers syncp;
	SVG_Clock *clipBegin, *clipEnd;

	GF_InlineScene *is;
	GF_Node *n = (GF_Node *)rti->timed_elt;
	u32 tag = gf_node_get_tag(n);
	is = (GF_InlineScene *)gf_node_get_private(n);


	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		xlinkp.actuate = &(((SVGanimationElement *)n)->xlink->actuate);
		xlinkp.arcrole = &((SVGanimationElement *)n)->xlink->arcrole;
		xlinkp.href = &((SVGanimationElement *)n)->xlink->href;
		xlinkp.role = &((SVGanimationElement *)n)->xlink->role;
		xlinkp.show = &((SVGanimationElement *)n)->xlink->show;
		xlinkp.title = &((SVGanimationElement *)n)->xlink->title;
		xlinkp.type = &((SVGanimationElement *)n)->xlink->type;
		syncp.syncBehavior = &((SVGanimationElement *)n)->sync->syncBehavior;
		syncp.syncBehaviorDefault = &((SVGanimationElement *)n)->sync->syncBehaviorDefault;
		syncp.syncMaster = &((SVGanimationElement *)n)->sync->syncMaster;
		syncp.syncReference = &((SVGanimationElement *)n)->sync->syncReference;
		syncp.syncTolerance = &((SVGanimationElement *)n)->sync->syncTolerance;
		syncp.syncToleranceDefault = &((SVGanimationElement *)n)->sync->syncToleranceDefault;
		clipBegin = &((SVGanimationElement *)n)->timing->clipBegin;
		clipEnd = &((SVGanimationElement *)n)->timing->clipEnd;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG2) && (tag<=GF_NODE_RANGE_LAST_SVG2)) {
		xlinkp.actuate = &((SVG2animationElement *)n)->xlink->actuate;
		xlinkp.arcrole = &((SVG2animationElement *)n)->xlink->arcrole;
		xlinkp.href = &((SVG2animationElement *)n)->xlink->href;
		xlinkp.role = &((SVG2animationElement *)n)->xlink->role;
		xlinkp.show = &((SVG2animationElement *)n)->xlink->show;
		xlinkp.title = &((SVG2animationElement *)n)->xlink->title;
		xlinkp.type = &((SVG2animationElement *)n)->xlink->type;
		syncp.syncBehavior = &((SVG2animationElement *)n)->sync->syncBehavior;
		syncp.syncBehaviorDefault = &((SVG2animationElement *)n)->sync->syncBehaviorDefault;
		syncp.syncMaster = &((SVG2animationElement *)n)->sync->syncMaster;
		syncp.syncReference = &((SVG2animationElement *)n)->sync->syncReference;
		syncp.syncTolerance = &((SVG2animationElement *)n)->sync->syncTolerance;
		syncp.syncToleranceDefault = &((SVG2animationElement *)n)->sync->syncToleranceDefault;
		clipBegin = &((SVG2animationElement *)n)->timing->clipBegin;
		clipEnd = &((SVG2animationElement *)n)->timing->clipEnd;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG3) && (tag<=GF_NODE_RANGE_LAST_SVG3)) {
		SVG3AllAttributes all_atts;
		gf_svg3_fill_all_attributes(&all_atts, (SVG3Element *)n);
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
		clipBegin = all_atts.clipBegin;
		clipEnd = all_atts.clipEnd;
	}

	if (!is) {
		is = gf_svg_get_subscene(n, &xlinkp, &syncp, 1);
		if (!is) return;

		/*assign animation scene as private stack of inline node, and remember inline node for event propagation*/
		gf_node_set_private((GF_Node *)n, is);
		gf_list_add(is->inline_nodes, n);
	}

	/*play*/
	if (!is->root_od->mo->num_open) {	
		if (clipEnd && clipEnd>0) is->root_od->media_stop_time = (u64) (1000*(*clipEnd));
		gf_mo_play(is->root_od->mo, (clipBegin?*clipBegin:0), 0);
	}
}

static void svg_animation_smil_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 status)
{
	GF_InlineScene *is;
	switch (status) {
	case SMIL_TIMING_EVAL_UPDATE:
		svg_animation_smil_update(rti, normalized_scene_time);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		is = (GF_InlineScene *)gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) gf_mo_stop(is->root_od->mo);
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		is = (GF_InlineScene *)gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) gf_mo_stop(is->root_od->mo);
		break;
	case SMIL_TIMING_EVAL_REPEAT:
		is = (GF_InlineScene *)gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) is->needs_restart = 1;
		break;
	}
}

void SVG_Render_animation(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_Node *sub_root;
	GF_InlineScene *is;
	
	if (is_destroy) return;


	is = (GF_InlineScene *)gf_node_get_private(n);
	if (!is || !is->graph_attached) return;

	if (is->needs_restart) {
		is->needs_restart = 0;
		if (is->is_dynamic_scene) {
			gf_is_restart_dynamic(is, 0);
		} else {
			/*we cannot use gf_mo_restart since it only sets the needs_restart for inline scenes. 
			The rational is that gf_mo_restart can be called from the parent scene (OK) or from the scene itself, in 
			which case shutting down the graph would crash the renderer. We therefore need two render passes to 
			safely restart an inline scene*/

			/*1- stop main object from playing but don't disconnect channels*/
			gf_odm_stop(is->root_od, 1);
			/*2- close all ODs inside the scene and reset the graph*/
			gf_is_disconnect(is, 0);
			/*3- restart the scene*/
			gf_odm_start(is->root_od);
		}
	}

	sub_root = gf_sg_get_root_node(is->graph);
	if (sub_root) {
//		if (is->root_od->mo->num_open && (is->root_od->media_stop_time < (s64) (gf_node_get_scene_time(sub_root)*1000))) {
//			gf_mo_stop(is->root_od->mo);
//		}
		gf_sr_render_inline(is->root_od->term->renderer, n, sub_root, rs);
	}
}


void SVG_Init_animation(GF_InlineScene *is, GF_Node *node)
{
	gf_smil_timing_init_runtime_info(node);
	if (((SVGElement *)node)->timing->runtime) {
		SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
		rti->evaluate = svg_animation_smil_evaluate;
	}
	gf_node_set_callback_function(node, SVG_Render_animation);
}



static void SVG_Render_use(GF_Node *node, void *rs, Bool is_destroy)
{
	XLinkAttributesPointers xlinkp;
	SMILSyncAttributesPointers syncp;
	u32 tag = gf_node_get_tag(node);

	if (is_destroy) return;

	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		xlinkp.actuate = &(((SVGuseElement *)node)->xlink->actuate);
		xlinkp.arcrole = &((SVGuseElement *)node)->xlink->arcrole;
		xlinkp.href = &((SVGuseElement *)node)->xlink->href;
		xlinkp.role = &((SVGuseElement *)node)->xlink->role;
		xlinkp.show = &((SVGuseElement *)node)->xlink->show;
		xlinkp.title = &((SVGuseElement *)node)->xlink->title;
		xlinkp.type = &((SVGuseElement *)node)->xlink->type;
		syncp.syncBehavior = &((SVGuseElement *)node)->sync->syncBehavior;
		syncp.syncBehaviorDefault = &((SVGuseElement *)node)->sync->syncBehaviorDefault;
		syncp.syncMaster = &((SVGuseElement *)node)->sync->syncMaster;
		syncp.syncReference = &((SVGuseElement *)node)->sync->syncReference;
		syncp.syncTolerance = &((SVGuseElement *)node)->sync->syncTolerance;
		syncp.syncToleranceDefault = &((SVGuseElement *)node)->sync->syncToleranceDefault;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG2) && (tag<=GF_NODE_RANGE_LAST_SVG2)) {
		xlinkp.actuate = &((SVG2useElement *)node)->xlink->actuate;
		xlinkp.arcrole = &((SVG2useElement *)node)->xlink->arcrole;
		xlinkp.href = &((SVG2useElement *)node)->xlink->href;
		xlinkp.role = &((SVG2useElement *)node)->xlink->role;
		xlinkp.show = &((SVG2useElement *)node)->xlink->show;
		xlinkp.title = &((SVG2useElement *)node)->xlink->title;
		xlinkp.type = &((SVG2useElement *)node)->xlink->type;
		syncp.syncBehavior = &((SVG2useElement *)node)->sync->syncBehavior;
		syncp.syncBehaviorDefault = &((SVG2useElement *)node)->sync->syncBehaviorDefault;
		syncp.syncMaster = &((SVG2useElement *)node)->sync->syncMaster;
		syncp.syncReference = &((SVG2useElement *)node)->sync->syncReference;
		syncp.syncTolerance = &((SVG2useElement *)node)->sync->syncTolerance;
		syncp.syncToleranceDefault = &((SVG2useElement *)node)->sync->syncToleranceDefault;
	} else if ((tag>=GF_NODE_RANGE_FIRST_SVG3) && (tag<=GF_NODE_RANGE_LAST_SVG3)) {
		SVG3AllAttributes all_atts;
		gf_svg3_fill_all_attributes(&all_atts, (SVG3Element *)node);
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
	}

	if (!xlinkp.href) return;

	if (xlinkp.href->type == SVG_IRI_ELEMENTID) {
		GF_InlineScene *is = (GF_InlineScene *)gf_sg_get_private(gf_node_get_graph((GF_Node *) node));
		gf_sr_render_inline(is->root_od->term->renderer, node, xlinkp.href->target, rs);
	} else {
		char *fragment;
		GF_Node *shadow_root;
		GF_InlineScene *is = (GF_InlineScene *)gf_node_get_private(node);

		if (!is) {
			is = gf_svg_get_subscene(node, &xlinkp, &syncp, 0);
			if (!is) return;

			/*assign animation scene as private stack of inline node, and remember inline node for event propagation*/
			gf_node_set_private(node, is);
			gf_list_add(is->inline_nodes, node);

			/*play*/
			if (!is->root_od->mo->num_open) 
				gf_mo_play(is->root_od->mo, 0, 0);
		}
		
		shadow_root = gf_sg_get_root_node(is->graph);

		if ((fragment = strchr(xlinkp.href->iri, '#')) ) {
			shadow_root = gf_sg_find_node_by_name(is->graph, fragment+1);
		}
		if (shadow_root) {
			gf_sr_render_inline(is->root_od->term->renderer, node, shadow_root, rs);
		}
	}
}

void SVG_Init_use(GF_InlineScene *is, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_use);
}

void SVG2_Init_use(GF_InlineScene *is, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_use);
}

void SVG3_Init_use(GF_InlineScene *is, GF_Node *node)
{
	gf_node_set_callback_function(node, SVG_Render_use);
}

#endif //GPAC_DISABLE_SVG

