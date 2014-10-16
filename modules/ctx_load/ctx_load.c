/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / GPAC Scene Context loader module
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

#include <gpac/internal/terminal_dev.h>
#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/network.h>
#include <gpac/nodes_mpeg4.h>

#ifndef GPAC_DISABLE_VRML

typedef struct
{
	GF_Scene *scene;
	GF_Terminal *app;
	GF_SceneManager *ctx;
	GF_SceneLoader load;
	char *file_name;
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
	u32 file_pos, sax_max_duration;
	Bool progressive_support;
} CTXLoadPriv;


static GF_Err CTXLoad_GetCapabilities(GF_BaseDecoder *plug, GF_CodecCapability *cap)
{
	cap->cap.valueInt = 0;
	return GF_NOT_SUPPORTED;
}

static GF_Err CTXLoad_SetCapabilities(GF_BaseDecoder *plug, const GF_CodecCapability capability)
{
	return GF_OK;
}

static void ODS_SetupOD(GF_Scene *scene, GF_ObjectDescriptor *od)
{
	GF_ObjectManager *odm;
	odm = gf_scene_find_odm(scene, od->objectDescriptorID);
	/*remove the old OD*/
	if (odm) gf_odm_disconnect(odm, 1);
	odm = gf_odm_new();
	odm->OD = od;
	odm->term = scene->root_od->term;
	odm->parentscene = scene;
	gf_mx_p(scene->mx_resources);
	gf_list_add(scene->resources, odm);
	gf_mx_v(scene->mx_resources);

	/*locate service owner*/
	gf_odm_setup_object(odm, scene->root_od->net_service);
}


static void CTXLoad_Reset(CTXLoadPriv *priv)
{
	if (priv->ctx) gf_sm_del(priv->ctx);
	priv->ctx = NULL;
	gf_sg_reset(priv->scene->graph);
	if (priv->load_flags != 3) priv->load_flags = 0;
	while (gf_list_count(priv->files_to_delete)) {
		char *fileName = (char*)gf_list_get(priv->files_to_delete, 0);
		gf_list_rem(priv->files_to_delete, 0);
		gf_delete_file(fileName);
		gf_free(fileName);
	}
}

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

void CTXLoad_NodeCallback(void *cbk, u32 type, GF_Node *node, void *param)
{
	if ((type==GF_SG_CALLBACK_INIT) && (gf_node_get_tag(node) == TAG_MPEG4_Conditional) ) {
		M_Conditional*c = (M_Conditional*)node;
		c->on_activate = CTXLoad_OnActivate;
		c->on_reverseActivate = CTXLoad_OnReverseActivate;
		gf_node_set_private(node, cbk);
	} else {
		gf_term_node_callback(cbk, type, node, param);
	}
}

static Bool CTXLoad_CheckDownload(CTXLoadPriv *priv)
{
	u64 size;
	FILE *f;
	u32 now = gf_sys_clock();

	if (!priv->file_size && (now - priv->last_check_time < 1000) ) return 0;

	f = gf_f64_open(priv->file_name, "rt");
	if (!f) return 0;
	gf_f64_seek(f, 0, SEEK_END);
	size = gf_f64_tell(f);
	fclose(f);

	/*we MUST have a complete file for now ...*/
	if (!priv->file_size) {
		if (priv->last_check_size == size) return 1;
		priv->last_check_size = size;
		priv->last_check_time = now;
	} else {
		if (size==priv->file_size) return 1;
	}
	return 0;
}


static GF_Err CTXLoad_Setup(GF_BaseDecoder *plug)
{
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;
	if (!priv->file_name) return GF_BAD_PARAM;

	priv->ctx = gf_sm_new(priv->scene->graph);
	memset(&priv->load, 0, sizeof(GF_SceneLoader));
	priv->load.ctx = priv->ctx;
	priv->load.is = priv->scene;
	priv->load.scene_graph = priv->scene->graph;
	priv->load.fileName = priv->file_name;
	priv->load.flags = GF_SM_LOAD_FOR_PLAYBACK;
	priv->load.localPath = gf_modules_get_option((GF_BaseInterface *)plug, "General", "CacheDirectory");
	priv->load.swf_import_flags = GF_SM_SWF_STATIC_DICT | GF_SM_SWF_QUAD_CURVE | GF_SM_SWF_SCALABLE_LINE | GF_SM_SWF_SPLIT_TIMELINE;
	return GF_OK;
}

static GF_Err CTXLoad_AttachStream(GF_BaseDecoder *plug, GF_ESD *esd)
{
	const char *ext;
	GF_BitStream *bs;
	u32 size;
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;

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
	/*main dummy stream we need a dsi*/
	if (!esd->decoderConfig->decoderSpecificInfo)
		return GF_NON_COMPLIANT_BITSTREAM;
	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	priv->file_size = gf_bs_read_u32(bs);
	gf_bs_del(bs);
	size = esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32);
	priv->file_name = (char *) gf_malloc(sizeof(char)*(1 + size) );
	memcpy(priv->file_name, esd->decoderConfig->decoderSpecificInfo->data + sizeof(u32),  sizeof(char)*(esd->decoderConfig->decoderSpecificInfo->dataLength - sizeof(u32)) );
	priv->file_name[size] = 0;
	priv->nb_streams = 1;
	priv->load_flags = 0;
	priv->base_stream_id = esd->ESID;


	CTXLoad_Setup(plug);

	priv->progressive_support = 0;
	priv->sax_max_duration = 0;

	ext = strrchr(priv->file_name, '.');
	if (!ext) return GF_OK;

	ext++;
	if (!stricmp(ext, "xmt") || !stricmp(ext, "xmtz") || !stricmp(ext, "xmta")
	        || !stricmp(ext, "x3d") || !stricmp(ext, "x3dz")
	   ) {
		ext = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "Progressive");
		priv->progressive_support = (ext && !stricmp(ext, "yes")) ? 1 : 0;
	}
	if (priv->progressive_support) {
		ext = gf_modules_get_option((GF_BaseInterface *)plug, "SAXLoader", "MaxDuration");
		if (ext) priv->sax_max_duration = atoi(ext);
	}
	return GF_OK;
}

static GF_Err CTXLoad_DetachStream(GF_BaseDecoder *plug, u16 ES_ID)
{
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;
	priv->nb_streams --;
	return GF_OK;
}

static GF_Err CTXLoad_AttachScene(GF_SceneDecoder *plug, GF_Scene *scene, Bool is_scene_decoder)
{
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;
	if (priv->ctx) return GF_BAD_PARAM;

	priv->scene = scene;
	priv->app = scene->root_od->term;
	gf_sg_set_node_callback(scene->graph, CTXLoad_NodeCallback);

	return GF_OK;
}

static GF_Err CTXLoad_ReleaseScene(GF_SceneDecoder *plug)
{
	CTXLoad_Reset((CTXLoadPriv *) plug->privateStack);
	return GF_OK;
}

static Bool CTXLoad_StreamInRootOD(GF_ObjectDescriptor *od, u32 ESID)
{
	u32 i, count;
	/*no root, only one stream possible*/
	if (!od) return 1;
	count = gf_list_count(od->ESDescriptors);
	/*idem*/
	if (!count) return 1;
	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *)gf_list_get(od->ESDescriptors, i);
		if (esd->ESID==ESID) return 1;
	}
	return 0;
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
	GF_AUContext *au;
	GF_StreamContext *sc;
	max_dur = 0;
	i=0;
	while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
		/*all streams in root OD are handled with ESID 0 to differentiate with any animation streams*/
		if (CTXLoad_StreamInRootOD(priv->ctx->root_od, sc->ESID)) sc->in_root_od = 1;
		if (!sc->timeScale) sc->timeScale = 1000;

		j=0;
		while ((au = (GF_AUContext *)gf_list_enum(sc->AUs, &j))) {
			if (!au->timing) au->timing = (u64) (sc->timeScale*au->timing_sec);
		}
		if (au && sc->in_root_od && (au->timing>max_dur)) max_dur = (u32) (au->timing * 1000 / sc->timeScale);
	}
	if (max_dur) {
		priv->scene->root_od->duration = max_dur;
		gf_scene_set_duration(priv->scene);
	}
}

static GF_Err CTXLoad_ProcessData(GF_SceneDecoder *plug, const char *inBuffer, u32 inBufferLength,
                                  u16 ES_ID, u32 stream_time, u32 mmlevel)
{
	GF_Err e = GF_OK;
	u32 i, j, k, nb_updates, last_rap=0;
	GF_AUContext *au;
	Bool can_delete_com;
	GF_StreamContext *sc;
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;

	/*something failed*/
	if (priv->load_flags==3) return GF_EOS;

	/*this signals main scene deconnection, destroy the context if needed*/
	assert(ES_ID);
	if (!priv->ctx) {
		e = CTXLoad_Setup((GF_BaseDecoder *)plug);
		if (e) return e;
	}


	if (stream_time==(u32)-1) {
		/*seek on root stream: destroy the context manager and reload it. We cannot seek on the main stream
		because commands may have changed node attributes/children and we d'ont track the initial value*/
		if (priv->load_flags && (priv->base_stream_id == ES_ID)) {
			if (priv->src) fclose(priv->src);
			priv->src = NULL;
			gf_sm_load_done(&priv->load);
			priv->file_pos = 0;
			/*queue scene for detach*/
			gf_term_lock_media_queue(priv->scene->root_od->term, 1);
			priv->scene->root_od->action_type = GF_ODM_ACTION_SCENE_RECONNECT;
			gf_list_add(priv->scene->root_od->term->media_queue, priv->scene->root_od);
			gf_term_lock_media_queue(priv->scene->root_od->term, 0);

			return CTXLoad_Setup((GF_BaseDecoder *)plug);
		}
		i=0;
		while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
			/*not our stream*/
			if (!sc->in_root_od && (sc->ESID != ES_ID)) continue;
			/*not the base stream*/
			if (sc->in_root_od && (priv->base_stream_id != ES_ID)) continue;
			/*handle SWF media extraction*/
			if ((sc->streamType == GF_STREAM_OD) && (priv->load_flags==1)) continue;
			sc->last_au_time = 0;
		}
		return GF_OK;
	}

	if (priv->load_flags != 2) {

		if (priv->progressive_support) {
			u32 entry_time;
			char file_buf[4096+1];
			if (!priv->src) {
				priv->src = gf_f64_open(priv->file_name, "rb");
				if (!priv->src) return GF_URL_ERROR;
				priv->file_pos = 0;
			}
			priv->load.type = GF_SM_LOAD_XMTA;
			e = GF_OK;
			entry_time = gf_sys_clock();
			gf_f64_seek(priv->src, priv->file_pos, SEEK_SET);
			while (1) {
				u32 diff, nb_read;
				nb_read = (u32) fread(file_buf, 1, 4096, priv->src);
				file_buf[nb_read] = 0;
				if (!nb_read) {
					if (priv->file_pos==priv->file_size) {
						fclose(priv->src);
						priv->src = NULL;
						priv->load_flags = 2;
						gf_sm_load_done(&priv->load);
						break;
					}
					break;
				}

				e = gf_sm_load_string(&priv->load, file_buf, 0);
				priv->file_pos += nb_read;
				if (e) break;
				diff = gf_sys_clock() - entry_time;
				if (diff > priv->sax_max_duration) break;
			}
			if (!priv->scene->graph_attached) {
				gf_sg_set_scene_size_info(priv->scene->graph, priv->ctx->scene_width, priv->ctx->scene_height, priv->ctx->is_pixel_metrics);
				gf_scene_attach_to_compositor(priv->scene);

				CTXLoad_CheckStreams(priv);
			}
		}
		/*load first frame only*/
		else if (!priv->load_flags) {
			/*we need the whole file*/
			if (!CTXLoad_CheckDownload(priv)) return GF_OK;

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

	nb_updates = 0;

	i=0;
	while ((sc = (GF_StreamContext *)gf_list_enum(priv->ctx->streams, &i))) {
		/*not our stream*/
		if (!sc->in_root_od && (sc->ESID != ES_ID)) continue;
		/*not the base stream*/
		if (sc->in_root_od && (priv->base_stream_id != ES_ID)) continue;
		/*handle SWF media extraction*/
		if ((sc->streamType == GF_STREAM_OD) && (priv->load_flags==1)) continue;

		/*check for seek*/
		if (sc->last_au_time > 1 + stream_time) {
			sc->last_au_time = 0;
		}

		can_delete_com = 0;
		if (sc->in_root_od && (priv->load_flags==2)) can_delete_com = 1;

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
				nb_updates++;
				break;
			}

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
					Bool keep_com = 0;
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
								} else {
									gf_odf_desc_del((GF_Descriptor *) od);
								}
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
									gf_odf_desc_del((GF_Descriptor *) od);
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
								} else {
									gf_odf_desc_del((GF_Descriptor *) od);
								}
								continue;
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
								continue;
#endif
							}

							/*soundstreams are a bit of a pain, they may be declared before any data gets written*/
							if (mux->delete_file) {
								FILE *t = gf_f64_open(mux->file_name, "rb");
								if (!t) {
									keep_com = 1;
									gf_list_insert(odU->objectDescriptors, od, 0);
									break;
								}
								fclose(t);
							}
							/*remap to remote URL - warning, the URL has already been resolved according to the parent path*/
							remote = gf_malloc(sizeof(char) * (strlen("gpac://")+strlen(mux->file_name)+1) );
							strcpy(remote, "gpac://");
							strcat(remote, mux->file_name);
							k = od->objectDescriptorID;
							/*if files were created we'll have to clean up (swf import)*/
							if (mux->delete_file) gf_list_add(priv->files_to_delete, gf_strdup(remote));

							gf_odf_desc_del((GF_Descriptor *) od);
							od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
							od->URLString = remote;
							od->objectDescriptorID = k;
							ODS_SetupOD(priv->scene, od);
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
	if ((priv->load_flags==2) && !nb_updates) return GF_EOS;
	return GF_OK;
}

const char *CTXLoad_GetName(struct _basedecoder *plug)
{
	CTXLoadPriv *priv = (CTXLoadPriv *)plug->privateStack;

	switch (priv->load.type) {
	case GF_SM_LOAD_BT:
		return "MPEG-4 BT Parser";
	case GF_SM_LOAD_VRML:
		return "VRML 97 Parser";
	case GF_SM_LOAD_X3DV:
		return "X3D (VRML Syntax) Parser";
	case GF_SM_LOAD_XMTA:
		return "XMT-A Parser";
	case GF_SM_LOAD_X3D:
		return "X3D (XML Syntax) Parser";
	case GF_SM_LOAD_SWF:
		return "Flash (SWF) Emulator";
	case GF_SM_LOAD_XSR:
		return "LASeRML Loader";
	case GF_SM_LOAD_MP4:
		return "MP4 Memory Loader";
	case GF_SM_LOAD_XBL:
		return "XBL Parser";

	default:
		return "Undetermined";
	}
}

static u32 CTXLoad_CanHandleStream(GF_BaseDecoder *ifce, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType==GF_STREAM_PRIVATE_SCENE) {
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		switch (esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_PRIVATE_SCENE_GENERIC:
			return GF_CODEC_SUPPORTED;
		/*LASeR ML: we use this plugin since it has command handling*/
		case GPAC_OTI_PRIVATE_SCENE_LASER:
			return GF_CODEC_SUPPORTED;
		/* XBL */
		case GPAC_OTI_PRIVATE_SCENE_XBL:
			return GF_CODEC_SUPPORTED;
		/*SVG*/
		case GPAC_OTI_PRIVATE_SCENE_SVG:
			return GF_CODEC_MAYBE_SUPPORTED;
		}
	} else if (StreamType==GF_STREAM_SCENE) {
		/*media type query*/
		if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
		switch (esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_SCENE_SVG:
		case GPAC_OTI_SCENE_SVG_GZ:
		case GPAC_OTI_SCENE_DIMS:
			return GF_CODEC_MAYBE_SUPPORTED;
		default:
			break;
		}
	}
	return GF_CODEC_NOT_SUPPORTED;
}

void DeleteContextLoader(GF_BaseDecoder *plug)
{
	CTXLoadPriv *priv;
	if (!plug)
		return;
	priv = (CTXLoadPriv *)plug->privateStack;
	if (priv) {
		if (priv->file_name)
			gf_free(priv->file_name);
		priv->file_name = NULL;
		assert(!priv->ctx);
		if (priv->files_to_delete)
			gf_list_del(priv->files_to_delete);
		priv->files_to_delete = NULL;
		gf_free(priv);
		plug->privateStack = NULL;
	}
	gf_free(plug);
}

GF_BaseDecoder *NewContextLoader()
{
	CTXLoadPriv *priv;
	GF_SceneDecoder *tmp;

	GF_SAFEALLOC(tmp, GF_SceneDecoder);
	GF_SAFEALLOC(priv, CTXLoadPriv);
	priv->files_to_delete = gf_list_new();

	tmp->privateStack = priv;
	tmp->AttachStream = CTXLoad_AttachStream;
	tmp->DetachStream = CTXLoad_DetachStream;
	tmp->GetCapabilities = CTXLoad_GetCapabilities;
	tmp->SetCapabilities = CTXLoad_SetCapabilities;
	tmp->ProcessData = CTXLoad_ProcessData;
	tmp->AttachScene = CTXLoad_AttachScene;
	tmp->ReleaseScene = CTXLoad_ReleaseScene;
	tmp->GetName = CTXLoad_GetName;
	tmp->CanHandleStream = CTXLoad_CanHandleStream;
	GF_REGISTER_MODULE_INTERFACE(tmp, GF_SCENE_DECODER_INTERFACE, "GPAC Context Loader", "gpac distribution")
	return (GF_BaseDecoder*)tmp;
}

#endif


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_VRML
		GF_SCENE_DECODER_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
#ifndef GPAC_DISABLE_VRML
	case GF_SCENE_DECODER_INTERFACE:
		return (GF_BaseInterface *)NewContextLoader();
#endif
	default:
		return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_VRML
	case GF_SCENE_DECODER_INTERFACE:
		DeleteContextLoader((GF_BaseDecoder *)ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DECLARATION( ctx_load )
