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
#include <gpac/nodes_svg.h>
#include <gpac/renderer.h>


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


/* Creates a subscene from the xlink:href */
static GF_InlineScene *gf_svg_get_subscene(SVGElement *elt, Bool use_sync)
{
	MFURL url;
	Bool lock_timelines = 0;
	GF_MediaObject *mo;
	GF_SceneGraph *graph = gf_node_get_graph((GF_Node *) elt);
	GF_InlineScene *is = gf_sg_get_private(graph);
	if (!is) return NULL;

	if (use_sync && elt->sync) {
		switch (elt->sync->syncBehavior) {
		case SMIL_SYNCBEHAVIOR_LOCKED:
		case SMIL_SYNCBEHAVIOR_CANSLIP:
			lock_timelines = 1;
			break;
		case SMIL_SYNCBEHAVIOR_DEFAULT:
		{
			SVGsvgElement *svg = (SVGsvgElement *) gf_sg_get_root_node(gf_node_get_graph((GF_Node*)elt));
			if (svg && svg->sync) {
				switch (svg->sync->syncBehaviorDefault) {
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
	gf_term_set_mfurl_from_uri(is->root_od->term, &url, &elt->xlink->href);

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
	while ((ctrl_od = gf_list_enum(is->ODlist, &i))) {
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
	GF_InlineScene *is;
	GF_Node *n = (GF_Node *)rti->timed_elt;
	is = gf_node_get_private(n);

	if (!is) {
		is = gf_svg_get_subscene((SVGElement *)n, 1);
		if (!is) return;

		/*assign animation scene as private stack of inline node, and remember inline node for event propagation*/
		gf_node_set_private((GF_Node *)n, is);
		gf_list_add(is->inline_nodes, n);
	}

	/*play*/
	if (!is->root_od->mo->num_open) {	
		if (rti->timed_elt->timing->clipEnd>0) is->root_od->media_stop_time = (u64) (1000*rti->timed_elt->timing->clipEnd);
		gf_mo_play(is->root_od->mo, rti->timed_elt->timing->clipBegin, 0);
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
		is = gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) gf_mo_stop(is->root_od->mo);
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		is = gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) gf_mo_stop(is->root_od->mo);
		break;
	case SMIL_TIMING_EVAL_RESTART:
		is = gf_node_get_private((GF_Node *)rti->timed_elt);
		if (is && is->root_od->mo) is->needs_restart = 1;
		break;
	}
}

void SVG_Render_animation(GF_Node *n, void *rs)
{
	GF_Node *sub_root;
	GF_InlineScene *is = gf_node_get_private(n);
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
	gf_smil_timing_init_runtime_info((SVGElement *)node);
	if (((SVGElement *)node)->timing->runtime) {
		SMIL_Timing_RTI *rti = ((SVGElement *)node)->timing->runtime;
		rti->evaluate = svg_animation_smil_evaluate;
	}
	gf_node_set_render_function(node, SVG_Render_animation);
}



static void SVG_Render_use(GF_Node *node, void *rs)
{
	SVGuseElement *use = (SVGuseElement *)node;

	if (use->xlink->href.type == SVG_IRI_ELEMENTID) {
		GF_InlineScene *is = gf_sg_get_private(gf_node_get_graph((GF_Node *) node));
		gf_sr_render_inline(is->root_od->term->renderer, node, (GF_Node *)use->xlink->href.target, rs);
	} else {
		char *fragment;
		GF_Node *shadow_root;
		GF_InlineScene *is = gf_node_get_private(node);

		if (!is) {
			is = gf_svg_get_subscene((SVGElement *)node, 0);
			if (!is) return;

			/*assign animation scene as private stack of inline node, and remember inline node for event propagation*/
			gf_node_set_private((GF_Node *)node, is);
			gf_list_add(is->inline_nodes, node);

			/*play*/
			if (!is->root_od->mo->num_open) 
				gf_mo_play(is->root_od->mo, 0, 0);
		}
		
		shadow_root = gf_sg_get_root_node(is->graph);

		if (fragment = strchr(use->xlink->href.iri, '#')) {
			shadow_root = gf_sg_find_node_by_name(is->graph, fragment+1);
		}
		if (shadow_root) {
			gf_sr_render_inline(is->root_od->term->renderer, node, shadow_root, rs);
		}
	}
}

void SVG_Init_use(GF_InlineScene *is, GF_Node *node)
{
	gf_node_set_render_function(node, SVG_Render_use);
}

#endif //GPAC_DISABLE_SVG

