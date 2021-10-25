/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
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



/*for OD service types*/
#include <gpac/constants.h>
/*for URL concatenation*/
#include <gpac/network.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/compositor.h>
#include <gpac/nodes_x3d.h>
#include <gpac/crypt.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif



#ifndef GPAC_DISABLE_VRML

void gf_inline_restart(GF_Scene *scene)
{
	scene->needs_restart = 1;
	gf_sc_invalidate(scene->compositor, NULL);
}


static Bool gf_inline_set_scene(M_Inline *root)
{
	GF_MediaObject *mo;
	GF_Scene *parent;
	GF_SceneGraph *graph = gf_node_get_graph((GF_Node *) root);
	parent = (GF_Scene *)gf_sg_get_private(graph);
	if (!parent) return GF_FALSE;

	mo = gf_scene_get_media_object_ex(parent, &root->url, GF_MEDIA_OBJECT_SCENE, GF_FALSE, NULL, GF_FALSE, (GF_Node*)root, NULL);
	if (!mo) return GF_FALSE;
	if (mo->connect_state > MO_CONNECT_BUFFERING) {
		gf_sg_vrml_mf_reset(&root->url, GF_SG_VRML_MFURL);
		return GF_FALSE;
	}
	//invalidate as soon as we have an mo (eg something is attached to the scene)
	gf_sc_invalidate(parent->compositor, NULL);

	if (!mo->odm) return GF_FALSE;

	if (!mo->odm->subscene) {
		gf_sc_invalidate(parent->compositor, NULL);
		return GF_FALSE;
	}
	/*assign inline scene as private stack of inline node, and remember inline node for event propagation*/
	gf_node_set_private((GF_Node *)root, mo->odm->subscene);
	mo->odm->subscene->object_attached = GF_TRUE;
	if (gf_list_find(mo->odm->subscene->attached_inlines, root)<0)
		gf_list_add(mo->odm->subscene->attached_inlines, root);

	/*play*/
	gf_mo_play(mo, 0, -1, GF_FALSE);

	return GF_TRUE;
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
			Bool changed = GF_TRUE;
			if (ODID != GF_MEDIA_EXTERNAL_ID) {
				if (ODID && (ODID==scene->root_od->ID)) changed = GF_FALSE;
			} else {
				if (gf_mo_is_same_url(mo, &pInline->url, NULL, 0) ) changed = GF_FALSE;
			}
			if (mo->num_open) {
				if (!changed) return;

				gf_scene_notify_event(scene, GF_EVENT_UNLOAD, node, NULL, GF_OK, GF_TRUE);
				gf_node_dirty_parents(node);
				gf_mo_event_target_remove_by_node(mo, node);

				/*reset the scene pointer as it may get destroyed*/
				switch (gf_node_get_tag(node)) {
				case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
				case TAG_X3D_Inline:
#endif
					gf_node_set_private(node, NULL);
					break;
				}

				scene->object_attached = GF_FALSE;
				mo->num_open --;
				if (!mo->num_open) {
					if (ODID == GF_MEDIA_EXTERNAL_ID) {
						GF_Scene *parent = scene->root_od->parentscene;
						/*!!! THIS WILL DESTROY THE INLINE SCENE OBJECT !!!*/
						gf_odm_disconnect(scene->root_od, GF_TRUE);
						/*and force removal of the media object*/
						if (parent) {
							if (gf_list_del_item(parent->scene_objects, mo)>=0) {
								gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
								gf_mo_del(mo);
							}
						}
					} else {

						/*external media are completely unloaded, except addons which are only declared once */
						if (!scene->root_od->addon && (scene->root_od->ID==GF_MEDIA_EXTERNAL_ID)) {
							gf_odm_disconnect(scene->root_od, 2);
						} else {
							gf_odm_stop_or_destroy(scene->root_od);
						}
					}
				}
			}
		}
	}
	/*force a redraw and load scene at next pass - we cannot load the scene now because
		- we can be in a JS call (eg JS mutex blocked)
		- locating scene objects matching the new url needs exclusive access to the MediaObject list, achieved with the term net mutex
		- another service may already be setting up objects (eg exclusive access to the net mutex grabbed), which can trigger event forwarding
		- some event forwarders may request JS context (eg access to JS mutex)

		In such a case we would end up in a deadlock - this needs urgent fixing ...
	*/
	if (ODID) {
		/*if no parent we must process the url change as we may not be traversed later on (not in the scene tree)*/
		if (gf_node_get_parent(node, 0)==NULL) {
			gf_inline_set_scene(pInline);
		} else {
			gf_node_dirty_parents(node);
		}
	}
}

static void gf_inline_check_restart(GF_Scene *scene)
{
	/*no ctrl if no duration*/
	if (!scene->duration) return;
	if (!scene->needs_restart) gf_odm_check_segment_switch(scene->root_od);
	if (scene->needs_restart) return;

	if (scene->root_od->media_ctrl && scene->root_od->media_ctrl->control->loop) {
		GF_Clock *ck = gf_odm_get_media_clock(scene->root_od);
		if (ck->has_seen_eos && !ck->nb_paused) {
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
				s = now;
				s/=1000;
				e = -1;
				MC_GetRange(scene->root_od->media_ctrl, &s, &e);
				if ((e>=0) && (e<GF_MAX_FLOAT)) dur = (u32) (e*1000);
				if (dur<=now) {
					scene->needs_restart = 1;
					scene->root_od->media_ctrl->current_seg = 0;
				} else {
					/*trigger render until to watch for restart...*/
					gf_sc_invalidate(scene->compositor, NULL);
				}
			}
		}
	}
}

void gf_scene_mpeg4_inline_check_restart(GF_Scene *scene)
{
	gf_inline_check_restart(scene);
	
	if (scene->needs_restart) {
		gf_sc_invalidate(scene->compositor, NULL);
		return;
	}
}

void gf_scene_mpeg4_inline_restart(GF_Scene *scene)
{
	u32 current_seg = 0;

	if (scene->root_od->media_ctrl) current_seg = scene->root_od->media_ctrl->current_seg;

	if (scene->is_dynamic_scene) {
		s64 from = 0;
		if (scene->root_od->media_ctrl) {
			if (scene->root_od->media_ctrl->media_stop<=0) {
				from = (s64) (scene->root_od->media_ctrl->media_stop * 1000) - 1;
			}
			else if (scene->root_od->media_ctrl->media_start>=0) {
				scene->root_od->media_ctrl->current_seg = current_seg;
				from = (s64) (scene->root_od->media_ctrl->media_start * 1000);
			}
		}
		gf_scene_restart_dynamic(scene, from, 0, 0);
	} else {
		/*we cannot use gf_mo_restart since it only sets the needs_restart for inline scenes.
		The rational is that gf_mo_restart can be called from the parent scene (OK) or from the scene itself, in
		which case shutting down the graph would crash the compositor. We therefore need two render passes to
		safely restart an inline scene*/

		/*1- stop main object from playing but don't disconnect channels*/
		gf_odm_stop(scene->root_od, GF_TRUE);
		/*2- close all ODs inside the scene and reset the graph*/
		gf_scene_disconnect(scene, GF_FALSE);
		if (scene->root_od->media_ctrl) scene->root_od->media_ctrl->current_seg = current_seg;
		/*3- restart the scene*/
		gf_odm_start(scene->root_od);
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

		gf_list_del_item(scene->attached_inlines, n);
		
		gf_scene_notify_event(scene, GF_EVENT_UNLOAD, n, NULL, GF_OK, GF_TRUE);
		if (!mo) return;
		gf_mo_event_target_remove_by_node(mo, n);

		/*disconnect current inline if we're the last one using it (same as regular OD session leave/join)*/
		if (mo->num_open) {
			mo->num_open --;
			if (!mo->num_open) {
				/*this is unspecified in the spec: whenever an inline not using the
				OD framework is destroyed, destroy the associated resource*/
				if (mo->OD_ID == GF_MEDIA_EXTERNAL_ID) {
					/*get parent scene and remove MediaObject in case the resource
					gets re-requested later on*/
					GF_Scene *parent_scene = (GF_Scene *)gf_sg_get_private(gf_node_get_graph((GF_Node *) n) );
					if (gf_list_del_item(parent_scene->scene_objects, mo)>=0) {
						gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
						if (mo->odm) {
							gf_odm_reset_media_control(mo->odm, 1);
							mo->odm->mo = NULL;
						}
						gf_mo_del(mo);
					}
					gf_odm_disconnect(scene->root_od, 2);
				} else {
					gf_odm_stop(scene->root_od, 1);
					gf_scene_disconnect(scene->root_od->subscene, GF_TRUE);
				}
			}
		}
		return;
	}


	//if no private scene is associated	get the node parent graph, retrieve the IS and find the OD
	if (!scene) {
		M_Inline *inl = (M_Inline *)n;
		if (!inl->url.count)
			return;

		gf_inline_set_scene(inl);
		scene = (GF_Scene *)gf_node_get_private(n);
		if (!scene) {
			/*just like protos, we must invalidate parent graph until attached*/
			if (inl->url.count) {
				if (!inl->url.vals[0].OD_ID && (!inl->url.vals[0].url || !strlen(inl->url.vals[0].url) ) ) {
					gf_sg_vrml_mf_reset(&inl->url, GF_SG_VRML_MFURL);
				} else {
					gf_node_dirty_set(n, 0, GF_TRUE);
				}
			}
			return;
		}
	}

	/*if not attached return (attaching the graph cannot be done in render since render is not called while unattached :) */
	if (!scene->graph_attached) {
		M_Inline *inl = (M_Inline *)n;
		if (inl->url.count) {
			/*just like protos, we must invalidate parent graph until attached*/
			gf_node_dirty_set(n, 0, GF_TRUE);
			//and request bew anim frame until attached
			if (scene->object_attached)
				gf_sc_invalidate(scene->compositor, NULL);
		}
		return;
	}
	/*clear dirty flags for any sub-inlines, bitmaps or protos*/
	gf_node_dirty_clear(n, 0);

	current_url = scene->current_url;
	scene->current_url = & ((M_Inline*)n)->url;
	gf_sc_traverse_subscene(scene->compositor, n, scene->graph, rs);
	scene->current_url = current_url;

	//do we have to restart for next frame ? If so let's do it
	gf_inline_check_restart(scene);

	/*if we need to restart, shutdown graph and do it*/
	if (scene->needs_restart) {
		/*special case: scene change*/
		if (scene->needs_restart==2) {
			scene->needs_restart = 0;
			gf_inline_on_modified(n);
			return;
		}

		scene->needs_restart = 0;
		gf_scene_mpeg4_inline_restart(scene);
		gf_node_dirty_set(n, 0, GF_TRUE);
		return;
	}

}


static Bool gf_inline_is_hardcoded_proto(GF_Compositor *compositor, MFURL *url)
{
	u32 i;
	for (i=0; i<url->count; i++) {
		if (!url->vals[i].url) continue;
		if (strstr(url->vals[i].url, "urn:inet:gpac:builtin")) return GF_TRUE;

		if (gf_sc_uri_is_hardcoded_proto(compositor, url->vals[i].url))
			return GF_TRUE;
	}
	return GF_FALSE;
}

GF_SceneGraph *gf_inline_get_proto_lib(void *_is, MFURL *lib_url)
{
	GF_ProtoLink *pl;
	u32 i;
	GF_Scene *scene = (GF_Scene *) _is;
	if (!scene) return NULL;

	//this is a scene reset, destroy all proto links
	if (!lib_url) {
		while (gf_list_count(scene->extern_protos)) {
			pl = gf_list_pop_back(scene->extern_protos);
			if (pl->mo) {
				//proto link was not attached, manual discard
				if (!pl->mo->odm) {
					gf_sg_vrml_mf_reset(&pl->mo->URLs, GF_SG_VRML_MFURL);
					gf_list_del_item(scene->scene_objects, pl->mo);
					gf_mo_del(pl->mo);
				}
			}
			gf_free(pl);
		}
		return NULL;
	}
	if (!lib_url->count)
		return NULL;

	if (gf_inline_is_hardcoded_proto(scene->compositor, lib_url)) return (void *) GF_SG_INTERNAL_PROTO;

	i=0;
	while ((pl = (GF_ProtoLink*)gf_list_enum(scene->extern_protos, &i))) {
		/*not ready yet*/
		if (!pl->mo || !pl->mo->odm) continue;

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
				if (!pl->mo || !pl->mo->odm) continue;

				if (gf_mo_get_od_id(pl->url) != GF_MEDIA_EXTERNAL_ID) continue;
				/*not the same url*/
				if (!gf_mo_is_same_url(pl->mo, lib_url, NULL, 0)) continue;
				ok = GF_FALSE;

				/*check the url path is the same*/
				url1 = gf_url_concatenate(pl->mo->odm->scene_ns->url, lib_url->vals[0].url);
				url2 = gf_url_concatenate(scene->root_od->scene_ns->url, lib_url->vals[0].url);
				if (url1 && url2 && !strcmp(url1, url2)) ok=GF_TRUE;
				if (url1) gf_free(url1);
				if (url2) gf_free(url2);

				if (!ok) continue;
				if (!pl->mo->odm || !pl->mo->odm->subscene) return NULL;
				return pl->mo->odm->subscene->graph;
			}
			check_scene = check_scene->root_od->parentscene;
		}
	}

	/*not found, let's try to load it*/

	if (!lib_url || !lib_url->count) return NULL;

	/*internal, don't waste resources*/
	if (gf_inline_is_hardcoded_proto(scene->compositor, lib_url)) return NULL;

	i=0;
	while ((pl = (GF_ProtoLink*)gf_list_enum(scene->extern_protos, &i)) ) {
		if (pl->url == lib_url) return NULL;
		if (pl->url->vals[0].OD_ID && (pl->url->vals[0].OD_ID == lib_url->vals[0].OD_ID)) return NULL;
		if (pl->url->vals[0].url && lib_url->vals[0].url && !stricmp(pl->url->vals[0].url, lib_url->vals[0].url) ) return NULL;
	}
	pl = (GF_ProtoLink*)gf_malloc(sizeof(GF_ProtoLink));
	pl->url = lib_url;
	gf_list_add(scene->extern_protos, pl);
	pl->mo = gf_scene_get_media_object(scene, lib_url, GF_MEDIA_OBJECT_SCENE, GF_FALSE);
	/*this may already be destroyed*/
	if (pl->mo) gf_mo_play(pl->mo, 0, -1, GF_FALSE);

	/*and return NULL*/
	return NULL;
}


Bool gf_inline_is_default_viewpoint(GF_Node *node)
{
	const char *nname, *seg_name;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_Scene *scene = sg ? (GF_Scene *) gf_sg_get_private(sg) : NULL;
	if (!scene) return GF_FALSE;

	nname = gf_node_get_name(node);
	if (!nname) return GF_FALSE;

	/*check any viewpoint*/
	seg_name = strrchr(scene->root_od->scene_ns->url, '#');

	/*check the URL of the parent*/
	if (!seg_name && scene->current_url) {
		if (scene->current_url->count && scene->current_url->vals[0].url)
			seg_name = strrchr(scene->current_url->vals[0].url, '#');
	} else if (!seg_name && scene->root_od->mo && scene->root_od->mo->URLs.count && scene->root_od->mo->URLs.vals[0].url) {
		seg_name = strrchr(scene->root_od->mo->URLs.vals[0].url, '#');
	}
	if (!seg_name) return GF_FALSE;
	seg_name += 1;
	/*look for a media segment with this name - if none found, this is a viewpoint name*/
	if (gf_odm_find_segment(scene->root_od, (char *) seg_name) != NULL) return GF_FALSE;

	return (!strcmp(nname, seg_name) ? GF_TRUE : GF_FALSE);
}


void gf_init_inline(GF_Scene *scene, GF_Node *node)
{
	gf_node_set_callback_function(node, gf_inline_traverse);
}

static char *storage_get_section(M_Storage *storage)
{
	GF_Scene *scene;
	char *szPath;
	u8 hash[20];
	char name[50];
	u32 i;
	size_t len;

	scene = (GF_Scene *)gf_node_get_private((GF_Node*)storage);

	len = strlen(scene->root_od->scene_ns->url)+strlen(storage->name.buffer)+2;
	szPath = (char *)gf_malloc(sizeof(char)* len);
	strcpy(szPath, scene->root_od->scene_ns->url);
	strcat(szPath, "@");
	strcat(szPath, storage->name.buffer);
	gf_sha1_csum((u8*)szPath, (u32) strlen(szPath), hash);
	gf_free(szPath);

	strcpy(name, "@cache=");
	for (i=0; i<20; i++) {
		char t[3];
		t[2] = 0;
		sprintf(t, "%02X", hash[i]);
		strcat(name, t);
	}
	return gf_strdup(name);
}

static void storage_parse_sf(void *ptr, u32 fieldType, char *opt)
{
	Float v1, v2, v3;
	switch (fieldType) {
	case GF_SG_VRML_SFBOOL:
		sscanf(opt, "%d", ((SFBool *)ptr));
		break;
	case GF_SG_VRML_SFINT32:
		sscanf(opt, "%d",  ((SFInt32 *)ptr) );
		break;
	case GF_SG_VRML_SFTIME:
		sscanf(opt, "%lf", ((SFTime *)ptr) );
		break;
	case GF_SG_VRML_SFFLOAT:
		sscanf(opt, "%g", &v1);
		* (SFFloat *)ptr = FLT2FIX(v1);
		break;
	case GF_SG_VRML_SFVEC2F:
		sscanf(opt, "%g %g", &v1, &v2);
		((SFVec2f*)ptr)->x = FLT2FIX(v1);
		((SFVec2f*)ptr)->y = FLT2FIX(v2);
		break;
	case GF_SG_VRML_SFVEC3F:
		sscanf(opt, "%g %g %g", &v1, &v2, &v3);
		((SFVec3f*)ptr)->x = FLT2FIX(v1);
		((SFVec3f*)ptr)->y = FLT2FIX(v2);
		((SFVec3f*)ptr)->z = FLT2FIX(v3);
		break;
	case GF_SG_VRML_SFSTRING:
		if ( ((SFString *)ptr)->buffer) gf_free(((SFString *)ptr)->buffer);
		((SFString *)ptr)->buffer = gf_strdup(opt);
		break;
	default:
		break;
	}
}

static void gf_storage_load(M_Storage *storage)
{
	const char *opt;
	char szID[20];
	u32 i, count;
	u32 sec, exp, frac;
	char *section = storage_get_section(storage);
	if (!section) return;

	if (!gf_opts_get_key_count(section)) {
		gf_free(section);
		return;
	}
	opt = gf_opts_get_key(section, "expireAfterNTP");
	gf_net_get_ntp(&sec, &frac);
	sscanf(opt, "%u", &exp);
	if (exp && (exp<=sec)) {
		gf_opts_del_section(section);
		gf_free(section);
		return;
	}

	count = gf_opts_get_key_count(section)-1;
	if (!count || (count!=storage->storageList.count)) {
		gf_opts_del_section(section);
		gf_free(section);
		return;
	}

	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		sprintf(szID, "%d", i);
		opt = gf_opts_get_key(section, szID);
		if (!opt) break;
		if (!storage->storageList.vals[i].node) break;
		if (gf_node_get_field(storage->storageList.vals[i].node, storage->storageList.vals[i].fieldIndex, &info) != GF_OK) break;

		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			storage_parse_sf(info.far_ptr, info.fieldType, (char *) opt);
		} else {
			u32 sftype = gf_sg_vrml_get_sf_type(info.fieldType);
			char *sep, *val;
			void *slot;
			gf_sg_vrml_mf_reset(info.far_ptr, info.fieldType);
			while (1) {
				val = (char *)strchr(opt, '\'');
				sep = val ? strchr(val+1, '\'') : NULL;
				if (!val || !sep) break;

				sep[0] = 0;
				gf_sg_vrml_mf_append(info.far_ptr, info.fieldType, &slot);
				storage_parse_sf(slot, sftype, val+1);
				sep[0] = '\'';
				opt = sep+1;
			}
		}
		gf_node_changed(storage->storageList.vals[i].node, &info);
	}
	gf_free(section);
}

char *storage_serialize_sf(void *ptr, u32 fieldType)
{
	char szVal[50];
	switch (fieldType) {
	case GF_SG_VRML_SFBOOL:
		sprintf(szVal, "%d", *((SFBool *)ptr) ? 1 : 0);
		return gf_strdup(szVal);
	case GF_SG_VRML_SFINT32:
		sprintf(szVal, "%d",  *((SFInt32 *)ptr) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFTIME:
		sprintf(szVal, "%g", *((SFTime *)ptr) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFFLOAT:
		sprintf(szVal, "%g", FIX2FLT( *((SFFloat *)ptr) ) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC2F:
		sprintf(szVal, "%g %g", FIX2FLT( ((SFVec2f *)ptr)->x), FIX2FLT( ((SFVec2f *)ptr)->y) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFVEC3F:
		sprintf(szVal, "%g %g %g", FIX2FLT( ((SFVec3f *)ptr)->x), FIX2FLT( ((SFVec3f *)ptr)->y) , FIX2FLT( ((SFVec3f *)ptr)->z) );
		return gf_strdup(szVal);
	case GF_SG_VRML_SFSTRING:
		return gf_strdup( ((SFString *)ptr)->buffer ? ((SFString *)ptr)->buffer : "");

	default:
		break;
	}
	return NULL;
}

void gf_storage_save(M_Storage *storage)
{
	char szID[20];
	u32 i, j;
	char *section = storage_get_section(storage);
	if (!section) return;

	gf_opts_del_section(section);

	if (storage->expireAfter) {
		u32 sec, frac;
		char szNTP[100];
		gf_net_get_ntp(&sec, &frac);
		sec += storage->expireAfter;
		sprintf(szNTP, "%u", sec);
		gf_opts_set_key(section, "expireAfterNTP", szNTP);
	} else {
		gf_opts_set_key(section, "expireAfterNTP", "0");
	}

	for (i=0; i<storage->storageList.count; i++) {
		char *val;
		GF_FieldInfo info;
		sprintf(szID, "%d", i);

		if (!storage->storageList.vals[i].node) break;
		if (gf_node_get_field(storage->storageList.vals[i].node, storage->storageList.vals[i].fieldIndex, &info) != GF_OK) break;

		if (gf_sg_vrml_is_sf_field(info.fieldType)) {
			val = storage_serialize_sf(info.far_ptr, info.fieldType);
		} else {
			//u32 sftype = gf_sg_vrml_get_sf_type(info.fieldType);
			val = NULL;
			for (j=0; j<((GenMFField *)info.far_ptr)->count; j++) {
				char *slotval;
				void *slot;
				if (gf_sg_vrml_mf_get_item(info.far_ptr, info.fieldType, &slot, j) != GF_OK) break;
				slotval = storage_serialize_sf(info.far_ptr, info.fieldType);
				if (!slotval) break;
				if (val) {
					val = (char *)gf_realloc(val, strlen(val) + 3 + strlen((const char *)slot));
				} else {
					val = (char *)gf_malloc(3 + strlen((const char *)slot));
					val[0] = 0;
				}
				strcat(val, "'");
				strcat(val, slotval);
				strcat(val, "'");
				gf_free(slot);
			}
		}
		if (val) {
			gf_opts_set_key(section, szID, val);
			gf_free(val);
		}
	}
	gf_free(section);
}


static void gf_storage_traverse(GF_Node *n, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Scene *scene = (GF_Scene *)gf_node_get_private(n);
		GF_SceneNamespace *scene_ns = scene->root_od->scene_ns;
		while (scene->root_od->parentscene) {
			if (scene->root_od->parentscene->root_od->scene_ns != scene_ns)
				break;
			scene = scene->root_od->parentscene;
		}
		gf_list_del_item(scene->storages, n);
	}
}

static void on_force_restore(GF_Node *n, struct _route *_route)
{
	if (n) gf_storage_load((M_Storage *)n);
}
static void on_force_save(GF_Node *n, struct _route *_route)
{
	if (n) gf_storage_save((M_Storage *)n);
}

void gf_scene_init_storage(GF_Scene *scene, GF_Node *node)
{
	GF_SceneNamespace *scene_ns;
	M_Storage *storage = (M_Storage *) node;

	if (!storage->name.buffer || !strlen(storage->name.buffer) ) return;
	if (!storage->storageList.count) return;

	storage->on_forceSave = on_force_save;
	storage->on_forceRestore = on_force_restore;
	gf_node_set_callback_function(node, gf_storage_traverse);
	gf_node_set_private(node, scene);

	scene_ns = scene->root_od->scene_ns;
	while (scene->root_od->parentscene) {
		if (scene->root_od->parentscene->root_od->scene_ns != scene_ns)
			break;
		scene = scene->root_od->parentscene;
	}

	gf_list_add(scene->storages, node);
	if (storage->_auto) gf_storage_load(storage);

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		Bool aptr;
		on_force_save(NULL, NULL);
		on_force_restore(NULL, NULL);
		storage_parse_sf(&aptr, GF_SG_VRML_SFBOOL, "true");

	}
#endif
}

#endif // GPAC_DISABLE_VRML

GF_Node *gf_scene_get_keynav(GF_SceneGraph *sg, GF_Node *sensor)
{
#ifndef GPAC_DISABLE_VRML
	u32 i, count;
	GF_Scene *scene = (GF_Scene *)gf_sg_get_private(sg);
	if (!scene) return NULL;
	if (!sensor) return (GF_Node *)gf_list_get(scene->keynavigators, 0);

	count = gf_list_count(scene->keynavigators);
	for (i=0; i<count; i++) {
		M_KeyNavigator *kn = (M_KeyNavigator *)gf_list_get(scene->keynavigators, i);
		if (kn->sensor==sensor) return (GF_Node *) kn;
	}
#endif
	return NULL;
}


