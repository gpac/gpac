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

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include "media_memory.h"


static Bool check_in_scene(GF_InlineScene *scene, GF_ObjectManager *odm)
{
	u32 i;
	GF_ObjectManager *root;
	if (!scene) return 0;
	root = scene->root_od;
	while (1) {
		if (odm == root) return 1;
		if (!root->remote_OD) break;
		root = root->remote_OD;
	}
	scene = root->subscene;

	for (i=0; i<gf_list_count(scene->ODlist); i++) {
		GF_ObjectManager *ptr = gf_list_get(scene->ODlist, i);
		while (1) {
			if (ptr == odm) return 1;
			if (!ptr->remote_OD) break;
			ptr = ptr->remote_OD;
		}
		if (check_in_scene(ptr->subscene, odm)) return 1;
	}
	return 0;
}

static Bool gf_term_check_odm(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term->root_scene) return 0;
	return check_in_scene(term->root_scene, odm);
}


/*returns top-level OD of the presentation*/
GF_ObjectManager *gf_term_get_root_object(GF_Terminal *term)
{
	if (!term) return NULL;
	if (!term->root_scene) return NULL;
	return term->root_scene->root_od;
}

/*returns number of sub-ODs in the current root. scene_od must be an inline OD*/
u32 gf_term_get_object_count(GF_Terminal *term, GF_ObjectManager *scene_od)
{
	if (!term || !scene_od) return 0;
	if (!gf_term_check_odm(term, scene_od)) return 0;
	if (!scene_od->subscene) return 0;
	return gf_list_count(scene_od->subscene->ODlist);
}

/*returns indexed (0-based) OD manager in the scene*/
GF_ObjectManager *gf_term_get_object(GF_Terminal *term, GF_ObjectManager *scene_od, u32 index)
{
	if (!term || !scene_od) return NULL;
	if (!gf_term_check_odm(term, scene_od)) return NULL;
	if (!scene_od->subscene) return NULL;
	return (GF_ObjectManager *) gf_list_get(scene_od->subscene->ODlist, index);
}

/*returns remote GF_ObjectManager of this OD if any, NULL otherwise*/
GF_ObjectManager *gf_term_get_remote_object(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term || !odm) return NULL;
	if (!gf_term_check_odm(term, odm)) return NULL;
	return odm->remote_OD;
}


u32 gf_term_object_subscene_type(GF_Terminal *term, GF_ObjectManager *odm)
{
	Bool IS_IsProtoLibObject(GF_InlineScene *is, GF_ObjectManager *odm);

	if (!term || !odm) return 0;
	if (!gf_term_check_odm(term, odm)) return 0;

	if (!odm->subscene) return 0;
	if (odm->parentscene) return IS_IsProtoLibObject(odm->parentscene, odm) ? 2 : 1;
	return 1;
}

/*select given object when stream selection is available*/
void gf_term_select_object(GF_Terminal *term, GF_ObjectManager *odm)
{
	if (!term || !odm) return;
	if (!gf_term_check_odm(term, odm)) return;

	gf_is_select_object(term->root_scene, odm);
}



static void get_codec_stats(GF_Codec *dec, ODInfo *info)
{
	info->avg_bitrate = dec->avg_bit_rate;
	info->max_bitrate = dec->max_bit_rate;
	info->nb_dec_frames = dec->nb_dec_frames;
	info->max_dec_time = dec->max_dec_time;
	info->total_dec_time = dec->total_dec_time;
}

GF_Err gf_term_get_object_info(GF_Terminal *term, GF_ObjectManager *odm, ODInfo *info)
{
	if (!term || !odm || !info) return GF_BAD_PARAM;
	if (!gf_term_check_odm(term, odm)) return GF_BAD_PARAM;

	memset(info, 0, sizeof(ODInfo));
	info->od = odm->OD;

	info->duration = odm->duration;
	info->duration /= 1000;
	if (odm->codec) {
		/*since we don't remove ODs that failed setup, check for clock*/
		if (odm->codec->ck) info->current_time = odm->codec->CB ? odm->current_time : gf_clock_time(odm->codec->ck);
		info->current_time /= 1000;
	} else if (odm->subscene && odm->subscene->scene_codec) {
		info->current_time = gf_clock_time(odm->subscene->scene_codec->ck);
		info->current_time /= 1000;
		info->duration = odm->subscene->duration;
		info->duration /= 1000;
	}

	info->buffer = -2;
	info->db_unit_count = 0;

	/*Warning: is_open==2 means object setup, don't check then*/
	if (odm->is_open==2) {
		info->status = 3;
	} else if (odm->is_open) {
		u32 i, buf;
		GF_Clock *ck;
		
		ck = gf_odm_get_media_clock(odm);
		/*no clock means setup failed*/
		if (!ck) {
			info->status = 4;
		} else {
			info->status = gf_clock_is_started(ck) ? 1 : 2;
			info->clock_drift = ck->drift;

			info->buffer = -1;
			buf = 0;
			for (i=0; i<gf_list_count(odm->channels); i++) {
				GF_Channel *ch = gf_list_get(odm->channels, i);
				info->db_unit_count += ch->AU_Count;
				if (!ch->is_pulling) {
					if (ch->MaxBuffer) info->buffer = 0;
					buf += ch->BufferTime;
				}
				if (ch->is_protected) info->protection = ch->crypt ? 1 : 2;
			}
			if (buf) info->buffer = (s32) buf;
		}
	}

	info->has_profiles = (odm->Audio_PL<0) ? 0 : 1;
	if (info->has_profiles) {
		info->inline_pl = odm->ProfileInlining;
		info->OD_pl = odm->OD_PL; 
		info->scene_pl = odm->Scene_PL;
		info->audio_pl = odm->Audio_PL;
		info->visual_pl = odm->Visual_PL;
		info->graphics_pl = odm->Graphics_PL;
	}	

	info->service_handler = odm->net_service->ifce->module_name;
	info->service_url = odm->net_service->url;
	if (odm->net_service->owner == odm) info->owns_service = 1;

	if (odm->codec && odm->codec->decio) {
		if (!odm->codec->decio->GetName) {
			info->codec_name = odm->codec->decio->module_name;
		} else {
			info->codec_name = odm->codec->decio->GetName(odm->codec->decio);
		}
		info->od_type = odm->codec->type;
		if (odm->codec->CB) {
			info->cb_max_count = odm->codec->CB->Capacity;
			info->cb_unit_count = odm->codec->CB->UnitCount;
		}
	} 
	
	if (odm->subscene && (odm->subscene->root_od==odm) && odm->subscene->scene_codec) {
		GF_BaseDecoder *dec = odm->subscene->scene_codec->decio;
		info->od_type = odm->subscene->scene_codec->type;
		if (!dec->GetName) {
			info->codec_name = dec->module_name;
		} else {
			info->codec_name = dec->GetName(dec);
		}
		gf_sg_get_scene_size_info(odm->subscene->graph, &info->width, &info->height);
	} else if (odm->mo) {
		switch (info->od_type) {
		case GF_STREAM_VISUAL:
			info->width = odm->mo->width;
			info->height = odm->mo->height;
			info->pixelFormat = odm->mo->pixelFormat;
			break;
		case GF_STREAM_AUDIO:
			info->sample_rate = odm->mo->sample_rate;
			info->bits_per_sample = odm->mo->bits_per_sample;
			info->num_channels = odm->mo->num_channels;
			info->clock_drift = 0;
			break;
		case GF_STREAM_TEXT:
			info->width = odm->mo->width;
			info->height = odm->mo->height;
			break;
		}
	}
	if (odm->subscene && odm->subscene->scene_codec) get_codec_stats(odm->subscene->scene_codec, info);
	else if (odm->codec) get_codec_stats(odm->codec, info);

	return GF_OK;
}


Bool gf_term_get_download_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, const char **server, const char **path, u32 *bytes_done, u32 *total_bytes, u32 *bytes_per_sec)
{
	GF_DownloadSession * sess;
	if (!term || !odm || !gf_term_check_odm(term, odm)) return 0;
	if (odm->net_service->owner != odm) return 0;

	if (*d_enum >= gf_list_count(odm->net_service->dnloads)) return 0;
	sess = gf_list_get(odm->net_service->dnloads, *d_enum);
	if (!sess) return 0;
	(*d_enum) ++;
	gf_dm_get_stats(sess, server, path, bytes_done, total_bytes, bytes_per_sec, NULL);
	return 1;
}

Bool gf_term_get_channel_net_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, u32 *chid, NetStatCommand *netcom, GF_Err *ret_code)
{
	GF_Channel *ch;
	GF_NetworkCommand com;
	if (!term || !odm || !gf_term_check_odm(term, odm)) return 0;
	if (*d_enum >= gf_list_count(odm->channels)) return 0;
	ch = gf_list_get(odm->channels, *d_enum);
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
	return 1;
}

char *gf_term_get_world_info(GF_Terminal *term, GF_ObjectManager *scene_od, GF_List *descriptions)
{
	M_WorldInfo *wi;
	u32 i;
	GF_ObjectManager *odm;
	if (!term) return NULL;
	wi = NULL;
	if (!scene_od) {
		if (!term->root_scene) return NULL;
		wi = term->root_scene->world_info;
	} else {
		if (!gf_term_check_odm(term, scene_od)) return NULL;
		odm = scene_od;
		while (odm->remote_OD) odm = odm->remote_OD;
		wi = odm->subscene ? odm->subscene->world_info : odm->parentscene->world_info;
	}
	if (!wi) return NULL;

	for (i=0; i<wi->info.count; i++) {
		gf_list_add(descriptions, strdup(wi->info.vals[i]));
	}
	return strdup(wi->title.buffer);
}


GF_Err gf_term_dump_scene(GF_Terminal *term, char *rad_name, Bool xml_dump, Bool skip_protos, GF_ObjectManager *scene_od)
{
#ifndef GPAC_READ_ONLY
	GF_SceneGraph *sg;
	GF_ObjectManager *odm;
	GF_SceneDumper *dumper;
	GF_Err e;

	if (!term || !term->root_scene) return GF_BAD_PARAM;
	if (!scene_od) {
		if (!term->root_scene) return GF_BAD_PARAM;
		odm = term->root_scene->root_od;
	} else {
		odm = scene_od;
		if (!gf_term_check_odm(term, scene_od)) 
			odm = term->root_scene->root_od;
	}

	while (odm->remote_OD) odm = odm->remote_OD;
	if (odm->subscene) {
		if (!odm->subscene->graph) return GF_IO_ERR;
		sg = odm->subscene->graph;
	} else {
		if (!odm->parentscene->graph) return GF_IO_ERR;
		sg = odm->parentscene->graph;
	}

	dumper = gf_sm_dumper_new(sg, rad_name, ' ', xml_dump ? GF_SM_DUMP_AUTO_XML : GF_SM_DUMP_AUTO_TXT);
	if (!dumper) return GF_IO_ERR;
	e = gf_sm_dump_graph(dumper, skip_protos, 0);
	gf_sm_dumper_del(dumper);
	return e;
#else
	return GF_NOT_SUPPORTED;
#endif
}

