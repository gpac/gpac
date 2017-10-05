/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/term_info.h>

/*WorldInfo node*/
#include <gpac/nodes_mpeg4.h>
/*title node*/
#include <gpac/nodes_svg.h>


static Bool check_in_scene(GF_Scene *scene, GF_ObjectManager *odm)
{
	u32 i;
	GF_ObjectManager *ptr, *root;
	if (!scene) return 0;
	root = scene->root_od;
	if (odm == root) return 1;
	scene = root->subscene;

	i=0;
	while ((ptr = (GF_ObjectManager *)gf_list_enum(scene->resources, &i))) {
		if (ptr == odm) return 1;
		if (check_in_scene(ptr->subscene, odm)) return 1;
	}
	return 0;
}

static Bool gf_term_check_odm(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term->compositor->root_scene) return 0;
	return check_in_scene(term->compositor->root_scene, odm);
}


/*returns top-level OD of the presentation*/
GF_EXPORT
GF_ObjectManager *gf_term_get_root_object(GF_Terminal *term)
{
	if (!term) return NULL;
	if (!term->compositor->root_scene) return NULL;
	return term->compositor->root_scene->root_od;
}

/*returns number of sub-ODs in the current root. scene_od must be an inline OD*/
GF_EXPORT
u32 gf_term_get_object_count(GF_Terminal *term, GF_ObjectManager *scene_od)
{
	if (!term || !scene_od) return 0;
	if (!gf_term_check_odm(term, scene_od)) return 0;
	if (!scene_od->subscene) return 0;
	return gf_list_count(scene_od->subscene->resources);
}

/*returns indexed (0-based) OD manager in the scene*/
GF_EXPORT
GF_ObjectManager *gf_term_get_object(GF_Terminal *term, GF_ObjectManager *scene_od, u32 index)
{
	if (!term || !scene_od) return NULL;
	if (!gf_term_check_odm(term, scene_od)) return NULL;
	if (!scene_od->subscene) return NULL;
	return (GF_ObjectManager *) gf_list_get(scene_od->subscene->resources, index);
}

GF_EXPORT
u32 gf_term_object_subscene_type(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term || !odm) return 0;
	if (!gf_term_check_odm(term, odm)) return 0;

	if (!odm->subscene) return 0;
#ifndef GPAC_DISABLE_VRML
	if (odm->parentscene) return gf_inline_is_protolib_object(odm->parentscene, odm) ? 3 : 2;
#endif
	return 1;
}

/*select given object when stream selection is available*/
GF_EXPORT
void gf_term_select_object(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term || !odm) return;
	if (!gf_term_check_odm(term, odm)) return;

	gf_scene_select_object(term->compositor->root_scene, odm);
}


/*select given object when stream selection is available*/
GF_EXPORT
void gf_term_select_service(GF_Terminal *term, GF_ObjectManager *odm, u32 service_id)
{
	if (!term || !odm || !odm->subscene) return;
	if (!gf_term_check_odm(term, odm)) return;

#ifndef GPAC_DISABLE_VRML
	gf_scene_set_service_id(odm->subscene, service_id);
#endif
}


/*select given object when stream selection is available*/
GF_EXPORT
void gf_term_toggle_addons(GF_Terminal *term, Bool show_addons)
{
	if (!term || !term->compositor->root_scene || !term->compositor->root_scene->is_dynamic_scene) return;
#ifndef GPAC_DISABLE_VRML
	gf_scene_toggle_addons(term->compositor->root_scene, show_addons);
#endif
}

GF_EXPORT
u32 gf_term_get_current_service_id(GF_Terminal *term)
{
	SFURL *the_url;
	GF_MediaObject *mo;
	GF_Compositor *compositor = term ? term->compositor : NULL;
	if (!term || !term->compositor->root_scene) return 0;
	if (! term->compositor->root_scene->is_dynamic_scene) return term->compositor->root_scene->root_od->ServiceID;

	if (compositor->root_scene->visual_url.OD_ID || compositor->root_scene->visual_url.url)
		the_url = &compositor->root_scene->visual_url;
	else
		the_url = &compositor->root_scene->audio_url;

	mo = gf_scene_find_object(compositor->root_scene, the_url->OD_ID, the_url->url);
	if (mo && mo->odm) return mo->odm->ServiceID;
	return 0;
}




GF_EXPORT
GF_Err gf_term_get_object_info(GF_Terminal *term, GF_ObjectManager *odm, GF_MediaInfo *info)
{
	if (!term || !odm || !info) return GF_BAD_PARAM;
	if (!gf_term_check_odm(term, odm)) return GF_BAD_PARAM;
	return gf_odm_get_object_info(odm, info);
}


GF_EXPORT
Bool gf_term_get_download_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, const char **server, const char **path, u32 *bytes_done, u32 *total_bytes, u32 *bytes_per_sec)
{
#ifdef FILTER_FIXME
	GF_DownloadSession * sess;
	if (!term || !odm || !gf_term_check_odm(term, odm)) return 0;
	if (odm->net_service->owner != odm) return 0;

	if (*d_enum >= gf_list_count(odm->net_service->dnloads)) return 0;
	sess = (GF_DownloadSession*)gf_list_get(odm->net_service->dnloads, *d_enum);
	if (!sess) return 0;
	(*d_enum) ++;
	gf_dm_sess_get_stats(sess, server, path, bytes_done, total_bytes, bytes_per_sec, NULL);
	return 1;
#endif
	return 0;
}

GF_EXPORT
Bool gf_term_get_channel_net_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, u32 *chid, NetStatCommand *netcom, GF_Err *ret_code)
{
#ifdef FILTER_FIXME
	GF_Channel *ch;
	GF_NetworkCommand com;
	if (!term || !odm || !gf_term_check_odm(term, odm)) return 0;
	if (*d_enum >= gf_list_count(odm->channels)) return 0;
	ch = (GF_Channel*)gf_list_get(odm->channels, *d_enum);
	if (!ch) return 0;
	(*d_enum) ++;
	if (ch->is_pulling) {
		(*ret_code) = GF_NOT_SUPPORTED;
		return 1;
	}
	(*chid) = ch->esd->ESID;
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.on_channel = ch;
	com.command_type = GF_NET_GET_STATS;
	(*ret_code) = gf_term_service_command(ch->service, &com);
	memcpy(netcom, &com.net_stats, sizeof(NetStatCommand));
#endif
	return 1;
}

GF_EXPORT
GF_Err gf_term_get_service_info(GF_Terminal *term, GF_ObjectManager *odm, NetInfoCommand *netinfo)
{
#ifdef FILTER_FIXME
	GF_Err e;
	GF_NetworkCommand com;
	if (!term || !odm || !netinfo || !gf_term_check_odm(term, odm)) return GF_BAD_PARAM;
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_SERVICE_INFO;
	e = gf_term_service_command(odm->net_service, &com);
	memcpy(netinfo, &com.info, sizeof(NetInfoCommand));
	return e;
#endif
	return GF_NOT_SUPPORTED;
}

GF_EXPORT
const char *gf_term_get_world_info(GF_Terminal *term, GF_ObjectManager *scene_od, GF_List *descriptions)
{
	GF_Node *info;
	if (!term) return NULL;
	info = NULL;
	if (!scene_od) {
		if (!term->compositor->root_scene) return NULL;
		info = (GF_Node*)term->compositor->root_scene->world_info;
	} else {
		if (!gf_term_check_odm(term, scene_od)) return NULL;
		info = (GF_Node*) (scene_od->subscene ? scene_od->subscene->world_info : scene_od->parentscene->world_info);
	}
	if (!info) return NULL;

	if (gf_node_get_tag(info) == TAG_SVG_title) {
		/*FIXME*/
		//return ((SVG_titleElement *) info)->textContent;
		return "TO FIX IN GPAC!!";
	} else {
#ifndef GPAC_DISABLE_VRML
		M_WorldInfo *wi = (M_WorldInfo *) info;
		if (descriptions) {
			u32 i;
			for (i=0; i<wi->info.count; i++) {
				gf_list_add(descriptions, wi->info.vals[i]);
			}
		}
		return wi->title.buffer;
#endif
	}
	return "GPAC";
}

GF_EXPORT
GF_Err gf_term_dump_scene(GF_Terminal *term, char *rad_name, char **filename, Bool xml_dump, Bool skip_protos, GF_ObjectManager *scene_od)
{
#ifndef GPAC_DISABLE_SCENE_DUMP
	GF_SceneGraph *sg;
	GF_ObjectManager *odm;
	GF_SceneDumper *dumper;
	GF_List *extra_graphs;
	u32 mode;
	u32 i;
	char szExt[20], *ext;
	GF_Err e;

	if (!term || !term->compositor->root_scene) return GF_BAD_PARAM;
	if (!scene_od) {
		if (!term->compositor->root_scene) return GF_BAD_PARAM;
		odm = term->compositor->root_scene->root_od;
	} else {
		odm = scene_od;
		if (!gf_term_check_odm(term, scene_od))
			odm = term->compositor->root_scene->root_od;
	}

	if (odm->subscene) {
		if (!odm->subscene->graph) return GF_IO_ERR;
		sg = odm->subscene->graph;
		extra_graphs = odm->subscene->extra_scenes;
	} else {
		if (!odm->parentscene->graph) return GF_IO_ERR;
		sg = odm->parentscene->graph;
		extra_graphs = odm->parentscene->extra_scenes;
	}

	mode = xml_dump ? GF_SM_DUMP_AUTO_XML : GF_SM_DUMP_AUTO_TXT;
	/*figure out best dump format based on extension*/
	ext = odm->scene_ns ? strrchr(odm->scene_ns->url, '.') : NULL;
	if (ext) {
		strcpy(szExt, ext);
		strlwr(szExt);
		if (!strcmp(szExt, ".wrl")) mode = xml_dump ? GF_SM_DUMP_X3D_XML : GF_SM_DUMP_VRML;
		else if(!strncmp(szExt, ".x3d", 4) || !strncmp(szExt, ".x3dv", 5) ) mode = xml_dump ? GF_SM_DUMP_X3D_XML : GF_SM_DUMP_X3D_VRML;
		else if(!strncmp(szExt, ".bt", 3) || !strncmp(szExt, ".xmt", 4) || !strncmp(szExt, ".mp4", 4) ) mode = xml_dump ? GF_SM_DUMP_XMTA : GF_SM_DUMP_BT;
	}

	dumper = gf_sm_dumper_new(sg, rad_name, GF_FALSE, ' ', mode);

	if (!dumper) return GF_IO_ERR;
	e = gf_sm_dump_graph(dumper, skip_protos, 0);
	for (i = 0; i < gf_list_count(extra_graphs); i++) {
		GF_SceneGraph *extra = (GF_SceneGraph *)gf_list_get(extra_graphs, i);
		gf_sm_dumper_set_extra_graph(dumper, extra);
		e = gf_sm_dump_graph(dumper, skip_protos, 0);
	}

	if (filename) *filename = gf_strdup(gf_sm_dump_get_name(dumper));
	gf_sm_dumper_del(dumper);
	return e;
#else
	return GF_NOT_SUPPORTED;
#endif
}


