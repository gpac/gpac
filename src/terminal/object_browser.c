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
#include "media_memory.h"


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


static void get_codec_stats(GF_FilterPid *pid, GF_MediaInfo *info)
{
	GF_FilterPidStatistics stats;
	gf_filter_pid_get_statistics(pid, &stats);

	info->avg_bitrate = stats.avgerage_bitrate;
	info->max_bitrate = stats.max_bitrate;
	info->nb_dec_frames = stats.nb_processed;
	info->max_dec_time = stats.max_process_time;
	info->total_dec_time = stats.total_process_time;
	info->first_frame_time = stats.first_process_time;
	info->last_frame_time = stats.last_process_time;
	info->au_duration = stats.min_frame_dur;
	info->nb_iraps = stats.nb_saps;
	info->irap_max_dec_time = stats.max_sap_process_time;
	info->irap_total_dec_time = stats.total_sap_process_time;
	info->avg_process_bitrate = stats.average_process_rate;
	info->max_process_bitrate = stats.max_process_rate;
}

GF_EXPORT
GF_Err gf_odm_get_object_info(GF_ObjectManager *odm, GF_MediaInfo *info)
{
	const GF_PropertyValue *prop;
	GF_ObjectManager *an_odm;
	GF_FilterPid *pid;

	if (!odm || !info) return GF_BAD_PARAM;
	memset(info, 0, sizeof(GF_MediaInfo));

	info->ODID = odm->ID;
	info->ServiceID = odm->ServiceID;
	info->pid_id = odm->pid_id;
	info->ocr_id = odm->ck ? odm->ck->clockID : 0;
	info->od_type = odm->type;

	info->duration = (Double) (s64)odm->duration;
	info->duration /= 1000;

	pid = odm->pid;
	an_odm = odm;
	while (an_odm->lower_layer_odm) {
		an_odm = an_odm->lower_layer_odm;
		pid = an_odm->pid;
	}

	if (pid) {
		/*since we don't remove ODs that failed setup, check for clock*/
		if (odm->ck) {
			info->current_time = odm->media_current_time;
			info->ntp_diff = odm->last_drawn_frame_ntp_diff;
			info->current_time = gf_clock_media_time(odm->ck);

		}
		info->current_time /= 1000;
		info->nb_dropped = odm->nb_dropped;
	} else if (odm->subscene) {
		if (odm->subscene->root_od->ck) {
			info->current_time = gf_clock_media_time(odm->subscene->root_od->ck);
			info->current_time /= 1000;
		}
		info->duration = (Double) (s64)odm->subscene->duration;
		info->duration /= 1000;
		info->nb_dropped = odm->subscene->root_od->nb_dropped;
		info->generated_scene = odm->subscene->is_dynamic_scene;
	}
	if (info->duration && info->current_time>info->duration)
		info->current_time = info->duration;

	info->buffer = -2;
	info->db_unit_count = 0;

	if (odm->state) {
		GF_Clock *ck;

		ck = gf_odm_get_media_clock(odm);
		/*no clock means setup failed*/
		if (!ck) {
			info->status = 4;
		} else {
			info->status = gf_clock_is_started(ck) ? 1 : 2;
			info->clock_drift = ck->drift;

			info->buffer = -1;
			info->min_buffer = -1;
			info->max_buffer = 0;

			info->buffer = gf_filter_pid_query_buffer_duration(pid) / 1000;
			info->max_buffer = odm->buffer_max_us / 1000;
			info->min_buffer = odm->buffer_min_us / 1000;

#ifdef FILTER_FIXME
			info->protection = ch->ipmp_tool ? 1 : 2;
#endif
		}
	}

	if (odm->scene_ns) {
		info->service_handler = gf_filter_get_name(odm->scene_ns->source_filter);

		info->service_url = odm->scene_ns->url;
		if (odm->scene_ns->owner == odm) info->owns_service = 1;
	} else if ((odm->subscene && odm->subscene->graph_attached) || (odm->ID)) {
		info->service_url = "No associated network Service";
	} else {
		info->service_url = "Service not found or error";
	}

	if (pid) {
		info->codec_name = gf_filter_pid_get_filter_name(pid);
		info->od_type = odm->type;

		gf_filter_pid_get_buffer_occupancy(pid, &info->cb_max_count, &info->cb_unit_count, NULL, NULL);

		get_codec_stats(pid, info);

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
		if (prop) info->lang_code = prop->value.string;
	}

	if (odm->subscene) {
		gf_sg_get_scene_size_info(odm->subscene->graph, &info->width, &info->height);
	} else if (odm->mo) {
		switch (info->od_type) {
		case GF_STREAM_VISUAL:
			gf_mo_get_visual_info(odm->mo, &info->width, &info->height, NULL, &info->par, &info->pixelFormat, NULL);
			break;
		case GF_STREAM_AUDIO:
			gf_mo_get_audio_info(odm->mo, &info->sample_rate, &info->bits_per_sample, &info->num_channels, NULL);
			info->clock_drift = 0;
			break;
		case GF_STREAM_TEXT:
			gf_mo_get_visual_info(odm->mo, &info->width, &info->height, NULL, NULL, NULL, NULL);
			break;
		}
	}

	if (odm->mo && odm->mo->URLs.count)
		info->media_url = odm->mo->URLs.vals[0].url;
	return GF_OK;
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


