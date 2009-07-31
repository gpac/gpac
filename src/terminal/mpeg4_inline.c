/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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



/*for OD service types*/
#include <gpac/constants.h>
/*for URL concatenation*/
#include <gpac/network.h>
#include <gpac/internal/terminal_dev.h>
#include "media_control.h"
#include <gpac/compositor.h>
#include <gpac/nodes_x3d.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif



#ifndef GPAC_DISABLE_VRML

void gf_inline_restart(GF_Scene *scene)
{
	scene->needs_restart = 1;
	gf_term_invalidate_compositor(scene->root_od->term);
}


static Bool gf_inline_set_scene(M_Inline *root)
{
	GF_MediaObject *mo;
	GF_Scene *parent;
	GF_SceneGraph *graph = gf_node_get_graph((GF_Node *) root);
	parent = (GF_Scene *)gf_sg_get_private(graph);
	if (!parent) return 0;

	mo = gf_scene_get_media_object_ex(parent, &root->url, GF_MEDIA_OBJECT_SCENE, 0, NULL, 0, (GF_Node*)root);
	if (!mo || !mo->odm) return 0;

	if (!mo->odm->subscene) {
		gf_term_invalidate_compositor(parent->root_od->term);
		return 0;
	}
	/*assign inline scene as private stack of inline node, and remember inline node for event propagation*/
	gf_node_set_private((GF_Node *)root, mo->odm->subscene);
	/*play*/
	gf_mo_play(mo, 0, -1, 0);
	return 1;
}

void gf_inline_on_modified(GF_Node *node)
{
	u32 ODID;
	GF_MediaObject *mo;
	M_Inline *pInline = (M_Inline *) node;
	GF_Scene *scene = (GF_Scene *)gf_node_get_private(node);

	ODID = gf_mo_get_od_id(&pInline->url);
	if (scene) {
		mo = (scene->root_od) ? scene->root_od->mo : NULL;

		/*disconnect current inline if we're the last one using it (same as regular OD session leave/join)*/
		if (mo) {
			Bool changed = 1;
			if (ODID != GF_MEDIA_EXTERNAL_ID) {
				if (ODID && (ODID==scene->root_od->OD->objectDescriptorID)) changed = 0;
			} else {
				if (gf_mo_is_same_url(mo, &pInline->url, NULL, 0) ) changed = 0;
			}
			if (mo->num_open) {
				if (!changed) return;

				gf_scene_notify_event(scene, GF_EVENT_UNLOAD, node);
				gf_node_dirty_parents(node);
				gf_list_del_item(mo->nodes, node);

				switch (gf_node_get_tag(node)) {
				case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
				case TAG_X3D_Inline:
#endif
					gf_node_set_private(node, NULL);
					break;
				}
				
				mo->num_open --;
				if (!mo->num_open) {
					if (ODID == GF_MEDIA_EXTERNAL_ID) {
						GF_Scene *parent = scene->root_od->parentscene;
						/*!!! THIS WILL DESTROY THE INLINE SCENE OBJECT !!!*/
						gf_odm_disconnect(scene->root_od, 1);
						/*and force removal of the media object*/
						if (parent) {
							if (gf_list_del_item(parent->scene_objects, mo)>=0) {
								gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
								gf_list_del(mo->nodes);
								free(mo);
							}
						}
					} else {
						gf_term_lock_net(scene->root_od->term, 1);
						
						/*external media are completely unloaded*/
						if (scene->root_od->OD->objectDescriptorID==GF_MEDIA_EXTERNAL_ID) {
							scene->root_od->action_type = GF_ODM_ACTION_DELETE;
						} else {
							scene->root_od->action_type = GF_ODM_ACTION_STOP;
						}
						if (gf_list_find(scene->root_od->term->media_queue, scene->root_od)<0)
							gf_list_add(scene->root_od->term->media_queue, scene->root_od);

						gf_term_lock_net(scene->root_od->term, 0);
					}
				}
			}
		}
	}	
	if (ODID) gf_inline_set_scene(pInline);
}

static void gf_inline_check_restart(GF_Scene *scene)
{
	/*no ctrl if no duration*/
	if (!scene->duration) return;
	if (!scene->needs_restart) gf_odm_check_segment_switch(scene->root_od);
	if (scene->needs_restart) return;

	if (scene->root_od->media_ctrl && scene->root_od->media_ctrl->control->loop) {
		GF_Clock *ck = gf_odm_get_media_clock(scene->root_od);
		if (ck->has_seen_eos) {
			u32 now = gf_clock_time(ck);
			u64 dur = scene->duration;
			if (scene->root_od->media_ctrl->current_seg) {
				/*only process when all segments are played*/
				if (gf_list_count(scene->root_od->media_ctrl->seg) <= scene->root_od->media_ctrl->current_seg) {
					scene->needs_restart = 1;
					scene->root_od->media_ctrl->current_seg = 0;
				}
			}
			else {
				Double s, e;
				s = now; s/=1000;
				e = -1;
				MC_GetRange(scene->root_od->media_ctrl, &s, &e);
				if ((e>=0) && (e<GF_MAX_FLOAT)) dur = (u32) (e*1000);
				if (dur<now) {
					scene->needs_restart = 1;
					scene->root_od->media_ctrl->current_seg = 0;
				}
			}
		} else {
			/*trigger render until to watch for restart...*/
			gf_term_invalidate_compositor(scene->root_od->term);
		}
	}
}

static void gf_inline_traverse(GF_Node *n, void *rs, Bool is_destroy)
{
	MFURL *current_url;
	GF_Scene *scene = (GF_Scene *)gf_node_get_private(n);

	if (is_destroy) {
		GF_MediaObject *mo;
		if (!scene) return;
		mo = scene->root_od ? scene->root_od->mo : NULL;

		gf_scene_notify_event(scene, GF_EVENT_UNLOAD, n);
		if (!mo) return;
		gf_list_del_item(mo->nodes, n);

		/*disconnect current inline if we're the last one using it (same as regular OD session leave/join)*/
		if (mo->num_open) {
			mo->num_open --;
			if (!mo->num_open) {
				/*this is unspecified in the spec: whenever an inline not using the 
				OD framework is destroyed, destroy the associated resource*/
				if (mo->OD_ID == GF_MEDIA_EXTERNAL_ID) {
					gf_odm_disconnect(scene->root_od, 1);

					/*get parent scene and remove MediaObject in case the ressource
					gets re-requested later on*/
					scene = (GF_Scene *)gf_sg_get_private(gf_node_get_graph((GF_Node *) n) );
					if (gf_list_del_item(scene->scene_objects, mo)>=0) {
						gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
						gf_list_del(mo->nodes);
						free(mo);
					}
				} else {
					gf_odm_stop(scene->root_od, 1);
					gf_scene_disconnect(scene, 1);
					assert(gf_list_count(scene->resources) == 0);
				}
			}
		}
		return;
	}


	//if no private scene is associated	get the node parent graph, retrieve the IS and find the OD
	if (!scene) {
		M_Inline *inl = (M_Inline *)n;
		gf_inline_set_scene(inl);
		scene = (GF_Scene *)gf_node_get_private(n);
		if (!scene) {
			/*just like protos, we must invalidate parent graph until attached*/
			if (inl->url.count)
				gf_node_dirty_set(n, 0, 1);
			return;
		}
	}

	gf_inline_check_restart(scene);

	/*if we need to restart, shutdown graph and do it*/
	if (scene->needs_restart) {
		u32 current_seg = 0;
		/*special case: scene change*/
		if (scene->needs_restart==2) {
			scene->needs_restart = 0;
			gf_inline_on_modified(n);
			return;
		}

		if (scene->root_od->media_ctrl) current_seg = scene->root_od->media_ctrl->current_seg;
		scene->needs_restart = 0;

		if (scene->is_dynamic_scene) {
			if (scene->root_od->media_ctrl) scene->root_od->media_ctrl->current_seg = current_seg;
			gf_scene_restart_dynamic(scene, 0);
		} else {
			/*we cannot use gf_mo_restart since it only sets the needs_restart for inline scenes. 
			The rational is that gf_mo_restart can be called from the parent scene (OK) or from the scene itself, in 
			which case shutting down the graph would crash the compositor. We therefore need two render passes to 
			safely restart an inline scene*/

			/*1- stop main object from playing but don't disconnect channels*/
			gf_odm_stop(scene->root_od, 1);
			/*2- close all ODs inside the scene and reset the graph*/
			gf_scene_disconnect(scene, 0);
			if (scene->root_od->media_ctrl) scene->root_od->media_ctrl->current_seg = current_seg;
			/*3- restart the scene*/
			gf_odm_start(scene->root_od);
		}
		gf_node_dirty_set(n, 0, 1);
		return;
	} 
	
	/*if not attached return (attaching the graph cannot be done in render since render is not called while unattached :) */
	if (!scene->graph_attached) {
		/*just like protos, we must invalidate parent graph until attached*/
		gf_node_dirty_set(n, 0, 1);
		return;
	}
	/*clear dirty flags for any sub-inlines, bitmaps or protos*/
	gf_node_dirty_clear(n, 0);

	current_url = scene->current_url;
	scene->current_url = & ((M_Inline*)n)->url;
	gf_sc_traverse_subscene(scene->root_od->term->compositor, n, scene->graph, rs);
	scene->current_url = current_url;
}


static Bool gf_inline_is_hardcoded_proto(MFURL *url, GF_Config *cfg)
{
	u32 i;
	const char *sOpt = gf_cfg_get_key(cfg, "Systems", "hardcoded_protos");
	for (i=0; i<url->count; i++) {
		if (!url->vals[i].url) continue;
		if (strstr(url->vals[i].url, "urn:inet:gpac:builtin")) return 1;
		if (sOpt && strstr(sOpt, url->vals[i].url)) return 1;
	}
	return 0;
}

GF_SceneGraph *gf_inline_get_proto_lib(void *_is, MFURL *lib_url)
{
	GF_ProtoLink *pl;
	u32 i;
	GF_Scene *scene = (GF_Scene *) _is;
	if (!scene || !lib_url->count) return NULL;

	if (gf_inline_is_hardcoded_proto(lib_url, scene->root_od->term->user->config)) return GF_SG_INTERNAL_PROTO;

	i=0;
	while ((pl = (GF_ProtoLink*)gf_list_enum(scene->extern_protos, &i))) {
		if (!pl->mo) continue;
		if (gf_mo_get_od_id(pl->url) != GF_MEDIA_EXTERNAL_ID) {
			if (gf_mo_get_od_id(pl->url) == gf_mo_get_od_id(lib_url)) {
				if (!pl->mo->odm || !pl->mo->odm->subscene) return NULL;
				return pl->mo->odm->subscene->graph;
			}
		}
	}

	/*for string URL based protos, recursively check until top if the proto lib is not already present*/
	if (lib_url->vals[0].url) {
		GF_Scene *check_scene = scene;
		while (check_scene) {
			i=0;
			while ((pl = (GF_ProtoLink*)gf_list_enum(check_scene->extern_protos, &i))) {
				char *url1, *url2;
				Bool ok;
				if (!pl->mo) continue;
				if (gf_mo_get_od_id(pl->url) != GF_MEDIA_EXTERNAL_ID) continue;
				/*not the same url*/
				if (!gf_mo_is_same_url(pl->mo, lib_url, NULL, 0)) continue;
				/*check the url path is the same*/
				url1 = gf_url_concatenate(pl->mo->odm->net_service->url, lib_url->vals[0].url);
				url2 = gf_url_concatenate(scene->root_od->net_service->url, lib_url->vals[0].url);
				ok = 0;
				if (url1 && url2 && !strcmp(url1, url2)) ok=1;
				if (url1) free(url1);
				if (url2) free(url2);
				if (!ok) continue;
				if (!pl->mo->odm || !pl->mo->odm->subscene) return NULL;
				return pl->mo->odm->subscene->graph;
			}
			check_scene = check_scene->root_od->parentscene;
		}
	}

	/*not found, let's try to load it*/

	if (!lib_url || !lib_url->count) return NULL;

	/*internal, don't waste ressources*/
	if (gf_inline_is_hardcoded_proto(lib_url, scene->root_od->term->user->config)) return NULL;
	
	i=0;
	while ((pl = (GF_ProtoLink*)gf_list_enum(scene->extern_protos, &i)) ) {
		if (pl->url == lib_url) return NULL;
		if (pl->url->vals[0].OD_ID && (pl->url->vals[0].OD_ID == lib_url->vals[0].OD_ID)) return NULL;
		if (pl->url->vals[0].url && lib_url->vals[0].url && !stricmp(pl->url->vals[0].url, lib_url->vals[0].url) ) return NULL;
	}
	pl = (GF_ProtoLink*)malloc(sizeof(GF_ProtoLink));
	pl->url = lib_url;
	gf_list_add(scene->extern_protos, pl);
	pl->mo = gf_scene_get_media_object(scene, lib_url, GF_MEDIA_OBJECT_SCENE, 0);
	/*this may already be destroyed*/
	if (pl->mo) gf_mo_play(pl->mo, 0, -1, 0);

	/*and return NULL*/
	return NULL;
}

Bool gf_inline_is_protolib_object(GF_Scene *scene, GF_ObjectManager *odm)
{
	u32 i;
	GF_ProtoLink *pl;
	i=0;
	while ((pl = (GF_ProtoLink*)gf_list_enum(scene->extern_protos, &i))) {
		if (pl->mo->odm == odm) return 1;
	}
	return 0;
}

GF_EXPORT
Bool gf_inline_is_default_viewpoint(GF_Node *node)
{
	const char *nname, *seg_name;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_Scene *scene = sg ? (GF_Scene *) gf_sg_get_private(sg) : NULL;
	if (!scene) return 0;

	nname = gf_node_get_name(node);
	if (!nname) return 0;

	/*check any viewpoint*/
	seg_name = strrchr(scene->root_od->net_service->url, '#');
	
	/*check the URL of the parent*/
	if (!seg_name && scene->current_url) {
		if (scene->current_url->count && scene->current_url->vals[0].url) 
			seg_name = strrchr(scene->current_url->vals[0].url, '#');
	} else if (!seg_name && scene->root_od->mo && scene->root_od->mo->URLs.count && scene->root_od->mo->URLs.vals[0].url) {
		seg_name = strrchr(scene->root_od->mo->URLs.vals[0].url, '#');
	}
	if (!seg_name) return 0;
	seg_name += 1;
	/*look for a media segment with this name - if none found, this is a viewpoint name*/
	if (gf_odm_find_segment(scene->root_od, (char *) seg_name) != NULL) return 0;

	return (!strcmp(nname, seg_name));
}


void gf_init_inline(GF_Scene *scene, GF_Node *node)
{
	gf_node_set_callback_function(node, gf_inline_traverse);
}
#endif


