/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Context loader filter
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

#include <gpac/internal/compositor_dev.h>
#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/network.h>
#include <gpac/nodes_mpeg4.h>

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_SCENEGRAPH)

typedef struct
{
	//opts
	Bool progressive;
	u32 sax_dur;

	//internal
	GF_FilterPid *in_pid, *out_pid;

	GF_Scene *scene;
	const char *file_name;


	GF_SceneManager *ctx;
	GF_SceneLoader load;
	u64 file_size;
	u32 load_flags;
	u32 nb_streams;
	u32 base_stream_id;
	u32 last_check_time;
	u64 last_check_size;
	/*mp3 import from flash*/
	GF_List *files_to_delete;
	/*progressive loading support for XMT X3D*/
	FILE *src;
	u32 file_pos;

	u64 pck_time;
	const char *service_url;
	Bool is_playing;
} CTXLoadPriv;


void ODS_SetupOD(GF_Scene *scene, GF_ObjectDescriptor *od);


static void CTXLoad_ExecuteConditional(M_Conditional *c, GF_Scene *scene)
{
	GF_List *clist = c->buffer.commandList;
	c->buffer.commandList = NULL;

	gf_sg_command_apply_list(gf_node_get_graph((GF_Node*)c), clist, gf_scene_get_time(scene));

	if (c->buffer.commandList != NULL) {
		while (gf_list_count(clist)) {
			GF_Command *sub_com = (GF_Command *)gf_list_get(clist, 0);
			gf_sg_command_del(sub_com);
			gf_list_rem(clist, 0);
		}
		gf_list_del(clist);
	} else {
		c->buffer.commandList = clist;
	}
}

static void CTXLoad_OnActivate(GF_Node *node, GF_Route *route)
{
	GF_Scene *scene = (GF_Scene *) gf_node_get_private(node);
	M_Conditional*c = (M_Conditional*)node;
	/*always apply in parent graph to handle protos correctly*/
	if (c->activate) CTXLoad_ExecuteConditional(c, scene);
}

static void CTXLoad_OnReverseActivate(GF_Node *node, GF_Route *route)
{
	GF_Scene *scene = (GF_Scene *) gf_node_get_private(node);
	M_Conditional*c = (M_Conditional*)node;
	/*always apply in parent graph to handle protos correctly*/
	if (!c->reverseActivate)
		CTXLoad_ExecuteConditional(c, scene);
}

void CTXLoad_NodeCallback(void *cbk, GF_SGNodeCbkType type, GF_Node *node, void *param)
{
	if ((type==GF_SG_CALLBACK_INIT) && (gf_node_get_tag(node) == TAG_MPEG4_Conditional) ) {
		M_Conditional*c = (M_Conditional*)node;
		c->on_activate = CTXLoad_OnActivate;
		c->on_reverseActivate = CTXLoad_OnReverseActivate;
		gf_node_set_private(node, cbk);
	} else {
		gf_scene_node_callback(cbk, type, node, param);
	}
}

static GF_Err CTXLoad_Setup(GF_Filter *filter, CTXLoadPriv *priv)
{
	const GF_PropertyValue *prop;
	if (!priv->file_name) return GF_BAD_PARAM;

	priv->ctx = gf_sm_new(priv->scene->graph);
	memset(&priv->load, 0, sizeof(GF_SceneLoader));
	priv->load.ctx = priv->ctx;
	priv->load.is = priv->scene;
	priv->load.scene_graph = priv->scene->graph;
	priv->load.fileName = priv->file_name;
	priv->load.src_url = priv->service_url;
	priv->load.flags = GF_SM_LOAD_FOR_PLAYBACK;
	priv->load.localPath = gf_get_default_cache_directory();

	priv->load.swf_import_flags = GF_SM_SWF_STATIC_DICT | GF_SM_SWF_QUAD_CURVE | GF_SM_SWF_SCALABLE_LINE | GF_SM_SWF_SPLIT_TIMELINE;

	if (!priv->files_to_delete)
		priv->files_to_delete = gf_list_new();

	prop = gf_filter_pid_get_property(priv->in_pid, GF_PROP_PID_MIME);
	if (prop && prop->value.string) {
		if (!strcmp(prop->value.string, "application/x-bt")) priv->load.type = GF_SM_LOAD_BT;
		else if (!strcmp(prop->value.string, "application/x-xmt")) priv->load.type = GF_SM_LOAD_XMTA;
		else if (!strcmp(prop->value.string, "model/vrml")) priv->load.type = GF_SM_LOAD_VRML;
		else if (!strcmp(prop->value.string, "x-model/x-vrml")) priv->load.type = GF_SM_LOAD_VRML;
		else if (!strcmp(prop->value.string, "model/x3d+vrml")) priv->load.type = GF_SM_LOAD_X3DV;
		else if (!strcmp(prop->value.string, "model/x3d+xml")) priv->load.type = GF_SM_LOAD_X3D;
		else if (!strcmp(prop->value.string, "application/x-shockwave-flash")) priv->load.type = GF_SM_LOAD_SWF;
		else if (!strcmp(prop->value.string, "application/x-LASeR+xml")) priv->load.type = GF_SM_LOAD_XSR;
	}

	return GF_OK;
}

GF_Err ctxload_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	CTXLoadPriv *priv = gf_filter_get_udta(filter);
	const GF_PropertyValue *prop;

	if (is_remove) {
		priv->in_pid = NULL;
		gf_filter_pid_remove(priv->out_pid);
		priv->out_pid = NULL;
		return GF_OK;
	}

	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	//we must have a file path
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_FILEPATH);
	if (!prop || ! prop->value.string) {
		return GF_NOT_SUPPORTED;
	}

	if (!priv->in_pid) {
		GF_FilterEvent fevt;
		priv->in_pid = pid;

		//we work with full file only, send a play event on source to indicate that
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		fevt.play.start_range = 0;
		fevt.base.on_pid = priv->in_pid;
		fevt.play.full_file_only = GF_TRUE;
		gf_filter_pid_send_event(priv->in_pid, &fevt);

	} else {
		if (pid != priv->in_pid) {
			return GF_REQUIRES_NEW_INSTANCE;
		}
		//update of PID filename
		if (!prop->value.string || !priv->file_name || strcmp(prop->value.string, priv->file_name))
			return GF_NOT_SUPPORTED;
		return GF_OK;
	}

#ifdef FILTER_FIXME
	/*animation stream like*/
	if (priv->ctx) {
		GF_StreamContext *sc;
		u32 i = 0;
		while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
			if (esd->ESID == sc->ESID) {
				priv->nb_streams++;
				return GF_OK;
			}
		}
		return GF_NON_COMPLIANT_BITSTREAM;
	}
#endif

	priv->file_name = prop->value.string;
	priv->nb_streams = 1;

	//declare a new output PID of type scene, codecid RAW
	priv->out_pid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(priv->out_pid, pid);
	gf_filter_pid_set_property(priv->out_pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_SCENE) );
	gf_filter_pid_set_property(priv->out_pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(priv->out_pid, GF_PROP_PID_IN_IOD, &PROP_BOOL(GF_TRUE) );

	gf_filter_pid_set_udta(pid, priv->out_pid);


	priv->file_size = 0;
	priv->load_flags = 0;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	priv->base_stream_id = prop ? prop->value.uint : -1;

	priv->pck_time = -1;


	switch (priv->load.type) {
	case GF_SM_LOAD_BT:
		gf_filter_set_name(filter, "Load:BT");
		break;
	case GF_SM_LOAD_VRML:
		gf_filter_set_name(filter, "Load:VRML97");
		break;
	case GF_SM_LOAD_X3DV:
		gf_filter_set_name(filter, "Load:X3D+vrml");
		break;
	case GF_SM_LOAD_XMTA:
		gf_filter_set_name(filter, "Load:XMTA");
		break;
	case GF_SM_LOAD_X3D:
		gf_filter_set_name(filter, "Load:X3D+XML Syntax");
		break;
	case GF_SM_LOAD_SWF:
		gf_filter_set_name(filter, "Load:SWF");
		break;
	case GF_SM_LOAD_XSR:
		gf_filter_set_name(filter, "Load:LASeRML");
		break;
	case GF_SM_LOAD_MP4:
		gf_filter_set_name(filter, "Load:MP4BIFSMemory");
		break;
	default:
		break;
	}

	return GF_OK;
}

static Bool ctxload_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	u32 count, i;
	CTXLoadPriv *priv = gf_filter_get_udta(filter);
	//check for scene attach
	switch (com->base.type) {
	case GF_FEVT_PLAY:
		//cancel play event, we work with full file
		//TODO: animation stream in BT
		priv->is_playing = GF_TRUE;
		return GF_TRUE;
	case GF_FEVT_ATTACH_SCENE:
		break;
	case GF_FEVT_RESET_SCENE:
		gf_sm_load_done(&priv->load);
		if (priv->ctx) gf_sm_del(priv->ctx);
		priv->ctx = NULL;
		priv->load_flags = 3;
		return GF_FALSE;
	default:
		return GF_FALSE;
	}
	if (!com->attach_scene.on_pid) return GF_TRUE;

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
		//we found our pid, set it up
		if (opid == com->attach_scene.on_pid) {
			if (!priv->scene) {
				GF_ObjectManager *odm = com->attach_scene.object_manager;
				priv->scene = odm->subscene ? odm->subscene : odm->parentscene;
				gf_sg_set_node_callback(priv->scene->graph, CTXLoad_NodeCallback);

				priv->service_url = odm->scene_ns->url;

				if (!priv->ctx)	CTXLoad_Setup(filter, priv);

			}
			return GF_TRUE;
		}
	}

	return GF_FALSE;
}

static Bool CTXLoad_StreamInRootOD(GF_ObjectDescriptor *od, u32 ESID)
{
	u32 i, count;
	/*no root, only one stream possible*/
	if (!od) return GF_TRUE;
	count = gf_list_count(od->ESDescriptors);
	/*idem*/
	if (!count) return GF_TRUE;
	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *)gf_list_get(od->ESDescriptors, i);
		if (esd->ESID==ESID) return GF_TRUE;
	}
	return GF_FALSE;
}


Double CTXLoad_GetVRMLTime(void *cbk)
{
	u32 secs, msecs;
	Double res;
	gf_utc_time_since_1970(&secs, &msecs);
	res = msecs;
	res /= 1000;
	res += secs;
	return res;
}

static void CTXLoad_CheckStreams(CTXLoadPriv *priv )
{
	u32 i, j, max_dur;
	GF_StreamContext *sc;
	u32 nb_aus=0;
	max_dur = 0;
	i=0;
	while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
		GF_AUContext *au;
		/*all streams in root OD are handled with ESID 0 to differentiate with any animation streams*/
		if (CTXLoad_StreamInRootOD(priv->ctx->root_od, sc->ESID)) sc->in_root_od = GF_TRUE;
		if (!sc->timeScale) sc->timeScale = 1000;

		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			if (!au->timing) au->timing = (u64) (sc->timeScale*au->timing_sec);
			if (gf_list_count(au->commands))
				nb_aus++;
		}
		if (au && sc->in_root_od && (au->timing>max_dur)) max_dur = (u32) (au->timing * 1000 / sc->timeScale);
	}
	if (max_dur) {
		priv->scene->root_od->duration = max_dur;
		gf_scene_set_duration(priv->scene);
	}
	if ((priv->load_flags==1) && priv->ctx->root_od && priv->ctx->root_od->URLString) {
		gf_filter_pid_set_property(priv->out_pid, GF_PROP_PID_REMOTE_URL, &PROP_STRING(priv->ctx->root_od->URLString) );
	}
	if ((priv->load_flags==2) && !nb_aus) {
		gf_filter_pid_set_eos(priv->out_pid);
	}
}

static GF_Err ctxload_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_FilterPacket *pck;
	u32 i, j, k, updates_pending, last_rap=0;
	GF_AUContext *au;
	Bool can_delete_com;
	GF_StreamContext *sc;
	Bool is_seek = GF_FALSE;
	Bool is_start, is_end;
	u32 min_next_time_ms = 0;
	CTXLoadPriv *priv = gf_filter_get_udta(filter);

	//not yet ready
	if (!priv->scene) {
		if (priv->is_playing) {
			gf_filter_pid_set_eos(priv->out_pid);
			return GF_EOS;
		}
		return GF_OK;
	}

	/*something failed*/
	if (priv->load_flags==3) return GF_EOS;

	/*this signals main scene deconnection, destroy the context if needed*/
	if (!priv->ctx) {
		e = CTXLoad_Setup(filter, priv);
		if (e) return e;
	}

	is_start = is_end = GF_FALSE;
	pck = gf_filter_pid_get_packet(priv->in_pid);
	if (pck) {
		//source is FILE, untimed - consider we init from 0
		u64 cts = 0;
		if ((s64)priv->pck_time<0) priv->pck_time = cts;
		else if (priv->pck_time!=cts) {
			priv->pck_time = cts;
			is_seek = GF_TRUE;
		}
		//init clocks
		gf_odm_check_buffering(priv->scene->root_od, priv->in_pid);
		gf_filter_pck_get_framing(pck, &is_start, &is_end);
		gf_filter_pid_drop_packet(priv->in_pid);
	}

	if (is_seek) {
		/*seek on root stream: destroy the context manager and reload it. We cannot seek on the main stream
		because commands may have changed node attributes/children and we don't track the initial value*/
		if (priv->load_flags) {
			if (priv->src) gf_fclose(priv->src);
			priv->src = NULL;
			gf_sm_load_done(&priv->load);
			priv->file_pos = 0;

			return CTXLoad_Setup(filter, priv);
		}
		i=0;
		while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
#ifdef FILTER_FIXME
			/*not our stream*/
			if (!sc->in_root_od && (sc->ESID != ES_ID)) continue;
			/*not the base stream*/
			if (sc->in_root_od && (priv->base_stream_id != ES_ID)) continue;
#endif
			/*handle SWF media extraction*/
			if ((sc->streamType == GF_STREAM_OD) && (priv->load_flags==1)) continue;
			sc->last_au_time = 0;
		}
		return GF_OK;
	}

	if (priv->load_flags != 2) {

		if (priv->progressive) {
			u32 entry_time;
			char file_buf[4096+1];
			if (!priv->src) {
				priv->src = gf_fopen(priv->file_name, "rb");
				if (!priv->src) return GF_URL_ERROR;
				priv->file_pos = 0;
			}
			priv->load.type = GF_SM_LOAD_XMTA;
			e = GF_OK;
			entry_time = gf_sys_clock();
			gf_fseek(priv->src, priv->file_pos, SEEK_SET);
			while (1) {
				u32 diff;
				s32 nb_read = (s32) gf_fread(file_buf, 4096, priv->src);
				if (nb_read<0) {
					return GF_IO_ERR;
				}
				file_buf[nb_read] = 0;
				if (!nb_read) {
					if (priv->file_pos==priv->file_size) {
						gf_fclose(priv->src);
						priv->src = NULL;
						priv->load_flags = 2;
						gf_sm_load_done(&priv->load);
						break;
					}
					break;
				}

				e = gf_sm_load_string(&priv->load, file_buf, GF_FALSE);
				priv->file_pos += nb_read;
				if (e) break;
				diff = gf_sys_clock() - entry_time;
				if (diff > priv->sax_dur) break;
			}
			if (!priv->scene->graph_attached) {
				gf_sg_set_scene_size_info(priv->scene->graph, priv->ctx->scene_width, priv->ctx->scene_height, priv->ctx->is_pixel_metrics);
				gf_scene_attach_to_compositor(priv->scene);

				CTXLoad_CheckStreams(priv);
			}
		}
		/*load first frame only*/
		else if (!priv->load_flags) {
			/*we need the whole file - this could be further optimized by loading from memory the entire buffer*/
			if (!is_end) return GF_OK;

			priv->load_flags = 1;
			e = gf_sm_load_init(&priv->load);
			if (!e) {
				CTXLoad_CheckStreams(priv);
				gf_sg_set_scene_size_info(priv->scene->graph, priv->ctx->scene_width, priv->ctx->scene_height, priv->ctx->is_pixel_metrics);
				/*VRML, override base clock*/
				if ((priv->load.type==GF_SM_LOAD_VRML) || (priv->load.type==GF_SM_LOAD_X3DV) || (priv->load.type==GF_SM_LOAD_X3D)) {
					/*override clock callback*/
					gf_sg_set_scene_time_callback(priv->scene->graph, CTXLoad_GetVRMLTime);
				}
			}
		}
		/*load the rest*/
		else {
			priv->load_flags = 2;
			e = gf_sm_load_run(&priv->load);
			gf_sm_load_done(&priv->load);
			/*in case this was not set in the first pass (XMT)*/
			gf_sg_set_scene_size_info(priv->scene->graph, priv->ctx->scene_width, priv->ctx->scene_height, priv->ctx->is_pixel_metrics);
		}

		if (e<0) {
			gf_sm_load_done(&priv->load);
			gf_sm_del(priv->ctx);
			priv->ctx = NULL;
			priv->load_flags = 3;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[CtxLoad] Failed to load context for file %s: %s\n", priv->file_name, gf_error_to_string(e) ));
			if (priv->out_pid)
				gf_filter_pid_set_eos(priv->out_pid);
			return e;
		}

		/*and figure out duration of root scene, and take care of XMT timing*/
		if (priv->load_flags==2) {
			CTXLoad_CheckStreams(priv);
			if (!gf_list_count(priv->ctx->streams)) {
				gf_scene_attach_to_compositor(priv->scene);
			}
		}
	}

	updates_pending = 0;

	i=0;
	while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
		u32 stream_time = gf_clock_time(priv->scene->root_od->ck);

		//compositor is in end of stream mode, flush all commands
		if (priv->scene->compositor->check_eos_state==2)
			stream_time=0xFFFFFFFF;

#ifdef FILTER_FIXME
		/*not our stream*/
		if (!sc->in_root_od && (sc->ESID != ES_ID)) continue;
		/*not the base stream*/
		if (sc->in_root_od && (priv->base_stream_id != ES_ID)) continue;
#endif
		/*handle SWF media extraction*/
		if ((sc->streamType == GF_STREAM_OD) && (priv->load_flags==1)) continue;

		/*check for seek*/
		if (sc->last_au_time > 1 + stream_time) {
			sc->last_au_time = 0;
		}

		can_delete_com = GF_FALSE;
		if (sc->in_root_od && (priv->load_flags==2)) can_delete_com = GF_TRUE;

		/*we're in the right stream, apply update*/
		j=0;

		/*seek*/
		if (!sc->last_au_time) {
			while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
				u32 au_time = (u32) (au->timing*1000/sc->timeScale);

				if (au_time > stream_time)
					break;
				if (au->flags & GF_SM_AU_RAP) last_rap = j-1;
			}
			j = last_rap;
		}

		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			u32 au_time = (u32) (au->timing*1000/sc->timeScale);

			if (au_time + 1 <= sc->last_au_time) {
				/*remove first replace command*/
				if (can_delete_com && (sc->streamType==GF_STREAM_SCENE)) {
					while (gf_list_count(au->commands)) {
						GF_Command *com = (GF_Command *)gf_list_get(au->commands, 0);
						gf_list_rem(au->commands, 0);
						gf_sg_command_del(com);
					}
					j--;
					gf_list_rem(sc->AUs, j);
					gf_list_del(au->commands);
					gf_free(au);
				}
				continue;
			}

			if (au_time > stream_time) {
				Double ts_offset;
				u32 t = au_time - stream_time;
				if (!min_next_time_ms || (min_next_time_ms>t))
					min_next_time_ms = t;

				updates_pending++;

				ts_offset = (Double) au->timing;
				ts_offset /= sc->timeScale;
				gf_sc_sys_frame_pending(priv->scene->compositor, ts_offset, stream_time, filter);
				break;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[CtxLoad] %s applying AU time %d\n", priv->file_name, au_time ));

			if (sc->streamType == GF_STREAM_SCENE) {
				GF_Command *com;
				/*apply the commands*/
				k=0;
				while ((com = (GF_Command *)gf_list_enum(au->commands, &k))) {
					e = gf_sg_command_apply(priv->scene->graph, com, 0);
					if (e) break;
					/*remove commands on base layer*/
					if (can_delete_com) {
						k--;
						gf_list_rem(au->commands, k);
						gf_sg_command_del(com);
					}
				}
			}
			else if (sc->streamType == GF_STREAM_OD) {
				/*apply the commands*/
				while (gf_list_count(au->commands)) {
					Bool keep_com = GF_FALSE;
					GF_ODCom *com = (GF_ODCom *)gf_list_get(au->commands, 0);
					gf_list_rem(au->commands, 0);
					switch (com->tag) {
					case GF_ODF_OD_UPDATE_TAG:
					{
						GF_ODUpdate *odU = (GF_ODUpdate *)com;
						while (gf_list_count(odU->objectDescriptors)) {
							GF_ESD *esd;
							char *remote;
							GF_MuxInfo *mux = NULL;
							GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)gf_list_get(odU->objectDescriptors, 0);
							gf_list_rem(odU->objectDescriptors, 0);
							/*we can only work with single-stream ods*/
							esd = (GF_ESD*)gf_list_get(od->ESDescriptors, 0);
							if (!esd) {
								if (od->URLString) {
									ODS_SetupOD(priv->scene, od);
								}
								gf_odf_desc_del((GF_Descriptor *) od);
								continue;
							}
							/*fix OCR dependencies*/
							if (CTXLoad_StreamInRootOD(priv->ctx->root_od, esd->OCRESID)) esd->OCRESID = priv->base_stream_id;

							/*forbidden if ESD*/
							if (od->URLString) {
								gf_odf_desc_del((GF_Descriptor *) od);
								continue;
							}
							/*look for MUX info*/
							k=0;
							while ((mux = (GF_MuxInfo*)gf_list_enum(esd->extensionDescriptors, &k))) {
								if (mux->tag == GF_ODF_MUXINFO_TAG) break;
								mux = NULL;
							}
							/*we need a mux if not animation stream*/
							if (!mux || !mux->file_name) {
								/*only animation streams are handled*/
								if (!esd->decoderConfig) {
								} else if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
									/*set ST to private scene to get sure the stream will be redirected to us*/
									esd->decoderConfig->streamType = GF_STREAM_PRIVATE_SCENE;
									esd->dependsOnESID = priv->base_stream_id;
									ODS_SetupOD(priv->scene, od);
								} else if (esd->decoderConfig->streamType==GF_STREAM_INTERACT) {
									GF_UIConfig *cfg = (GF_UIConfig *) esd->decoderConfig->decoderSpecificInfo;
									gf_odf_encode_ui_config(cfg, &esd->decoderConfig->decoderSpecificInfo);
									gf_odf_desc_del((GF_Descriptor *) cfg);
									ODS_SetupOD(priv->scene, od);
								} else if (esd->decoderConfig->streamType==GF_STREAM_OCR) {
									ODS_SetupOD(priv->scene, od);
								}
								gf_odf_desc_del((GF_Descriptor *) od);
								continue;
							}
							//solve url before import
							if (mux->src_url) {
								char *res_url = gf_url_concatenate(mux->src_url, mux->file_name);
								if (res_url) {
									gf_free(mux->file_name);
									mux->file_name = res_url;
								}
								gf_free(mux->src_url);
								mux->src_url = NULL;
							}
							/*text import*/
							if (mux->textNode) {
#ifdef GPAC_DISABLE_MEDIA_IMPORT
								gf_odf_desc_del((GF_Descriptor *) od);
								continue;
#else
								e = gf_sm_import_bifs_subtitle(priv->ctx, esd, mux);
								if (e) {
									e = GF_OK;
									gf_odf_desc_del((GF_Descriptor *) od);
									continue;
								}
								/*set ST to private scene and dependency to base to get sure the stream will be redirected to us*/
								esd->decoderConfig->streamType = GF_STREAM_PRIVATE_SCENE;
								esd->dependsOnESID = priv->base_stream_id;
								ODS_SetupOD(priv->scene, od);
								gf_odf_desc_del((GF_Descriptor *) od);
								continue;
#endif
							}

							/*soundstreams are a bit of a pain, they may be declared before any data gets written*/
							if (mux->delete_file) {
								FILE *t = gf_fopen(mux->file_name, "rb");
								if (!t) {
									keep_com = GF_TRUE;
									gf_list_insert(odU->objectDescriptors, od, 0);
									break;
								}
								gf_fclose(t);
							}
							/*remap to remote URL - warning, the URL has already been resolved according to the parent path*/
							remote = (char*)gf_malloc(sizeof(char) * (strlen("gpac://")+strlen(mux->file_name)+1) );
							strcpy(remote, "gpac://");
							strcat(remote, mux->file_name);
							k = od->objectDescriptorID;
							/*if files were created we'll have to clean up (swf import)*/
							if (mux->delete_file) gf_list_add(priv->files_to_delete, gf_strdup(remote));

							gf_odf_desc_del((GF_Descriptor *) od);
							od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
							od->URLString = remote;
							od->fake_remote = GF_TRUE;
							od->objectDescriptorID = k;
							ODS_SetupOD(priv->scene, od);
							gf_odf_desc_del((GF_Descriptor*)od);
						}
						if (keep_com) break;
					}
					break;
					case GF_ODF_OD_REMOVE_TAG:
					{
						GF_ODRemove *odR = (GF_ODRemove*)com;
						for (k=0; k<odR->NbODs; k++) {
							GF_ObjectManager *odm = gf_scene_find_odm(priv->scene, odR->OD_ID[k]);
							if (odm) gf_odm_disconnect(odm, 1);
						}
					}
					break;
					default:
						break;
					}
					if (keep_com) {
						gf_list_insert(au->commands, com, 0);
						break;
					} else {
						gf_odf_com_del(&com);
					}
					if (e) break;
				}

			}
			sc->last_au_time = au_time + 1;
			/*attach graph to renderer*/
			if (!priv->scene->graph_attached)
				gf_scene_attach_to_compositor(priv->scene);
			if (e) return e;

			/*for root streams remove completed AUs (no longer needed)*/
			if (sc->in_root_od && !gf_list_count(au->commands) ) {
				j--;
				gf_list_rem(sc->AUs, j);
				gf_list_del(au->commands);
				gf_free(au);
			}
		}
	}
	if (e) return e;

	if ((priv->load_flags==2) && !updates_pending) {
		gf_filter_pid_set_eos(priv->out_pid);
		return GF_EOS;
	}
	if (!min_next_time_ms) min_next_time_ms = 1;
	min_next_time_ms *= 1000;
	if (min_next_time_ms>2000)
		min_next_time_ms = 2000;

	gf_filter_ask_rt_reschedule(filter, min_next_time_ms);
	return GF_OK;
}


static void ctxload_finalize(GF_Filter *filter)
{
	CTXLoadPriv *priv = gf_filter_get_udta(filter);

	if (priv->ctx) gf_sm_del(priv->ctx);
	if (priv->files_to_delete) gf_list_del(priv->files_to_delete);
}

#include <gpac/utf.h>
static const char *ctxload_probe_data(const u8 *probe_data, u32 size, GF_FilterProbeScore *score)
{
	const char *mime_type = NULL;
	char *dst = NULL;
	u8 *res;

	/* check gzip magic header */
	if ((size>2) && (probe_data[0] == 0x1f) && (probe_data[1] == 0x8b)) {
		*score = GF_FPROBE_EXT_MATCH;
		return "btz|bt.gz|xmt.gz|xmtz|wrl.gz|x3dv.gz|x3dvz|x3d.gz|x3dz";
	}

	res = gf_utf_get_utf8_string_from_bom((char *)probe_data, size, &dst);
	if (res) probe_data = res;

	if (strstr(probe_data, "<XMT-A") || strstr(probe_data, ":mpeg4:xmta:")) {
		mime_type = "application/x-xmt";
	} else if (strstr(probe_data, "InitialObjectDescriptor")
		|| (strstr(probe_data, "EXTERNPROTO") && strstr(probe_data, "gpac:"))
	) {
		mime_type = "application/x-bt";
	} else if ( strstr(probe_data, "children") &&
				(strstr(probe_data, "Group") || strstr(probe_data, "OrderedGroup") || strstr(probe_data, "Layer2D") || strstr(probe_data, "Layer3D"))
	) {
		mime_type = "application/x-bt";
	} else if (strstr(probe_data, "#VRML V2.0 utf8")) {
		mime_type = "model/vrml";
	} else if ( strstr(probe_data, "#X3D V3.0")) {
		mime_type = "model/x3d+vrml";
	} else if (strstr(probe_data, "<X3D") || strstr(probe_data, "/x3d-3.0.dtd")) {
		mime_type = "model/x3d+xml";
	} else if (strstr(probe_data, "<saf") || strstr(probe_data, "mpeg4:SAF:2005")
		|| strstr(probe_data, "mpeg4:LASeR:2005")
	) {
		mime_type = "application/x-LASeR+xml";
	} else if (strstr(probe_data, "DIMSStream") ) {
		mime_type = "application/dims";
	} else if (strstr(probe_data, "<svg") || strstr(probe_data, "w3.org/2000/svg") ) {
		mime_type = "image/svg+xml";
	} else if (strstr(probe_data, "<widget")  ) {
		mime_type = "application/widget";
	} else if (strstr(probe_data, "<NHNTStream")) {
		mime_type = "application/x-nhml";
	} else if (strstr(probe_data, "TextStream") ) {
		mime_type = "text/ttxt";
	} else if (strstr(probe_data, "text3GTrack") ) {
		mime_type = "quicktime/text";
	}
	if (dst) gf_free(dst);
	if (mime_type) {
		*score = GF_FPROBE_MAYBE_SUPPORTED;
		return mime_type;
	}

	*score = GF_FPROBE_NOT_SUPPORTED;
	return NULL;
}

static const GF_FilterCapability CTXLoadCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "bt|btz|bt.gz|xmt|xmt.gz|xmtz|wrl|wrl.gz|x3dv|x3dv.gz|x3dvz|x3d|x3d.gz|x3dz|swf|xsr"),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "application/x-bt|application/x-xmt|model/vrml|x-model/x-vrml|model/x3d+vrml|model/x3d+xml|application/x-shockwave-flash|application/x-LASeR+xml"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


#define OFFS(_n)	#_n, offsetof(CTXLoadPriv, _n)

static const GF_FilterArgs CTXLoadArgs[] =
{
	{ OFFS(progressive), "enable progressive loading", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sax_dur), "loading duration for SAX parsing (XMT), 0 disables SAX parsing", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister CTXLoadRegister = {
	.name = "btplay",
	GF_FS_SET_DESCRIPTION("BT/XMT/X3D loader")
	GF_FS_SET_HELP("This filter parses MPEG-4 BIFS (BT and XMT), VRML97 and X3D (wrl and XML) files directly into the scene graph of the compositor. It cannot be used to dump content.")
	.private_size = sizeof(CTXLoadPriv),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = CTXLoadArgs,
	SETCAPS(CTXLoadCaps),
	.finalize = ctxload_finalize,
	.process = ctxload_process,
	.configure_pid = ctxload_configure_pid,
	.process_event = ctxload_process_event,
	.probe_data = ctxload_probe_data,
};

#endif //defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_SCENEGRAPH)


const GF_FilterRegister *ctxload_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_SCENEGRAPH)
	return &CTXLoadRegister;
#else
	return NULL;
#endif
}


