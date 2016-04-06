/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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
#include <gpac/media_tools.h>
#include <gpac/bifs.h>
#include <gpac/xml.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/network.h>


GF_EXPORT
GF_SceneManager *gf_sm_new(GF_SceneGraph *graph)
{
	GF_SceneManager *tmp;

	if (!graph) return NULL;
	GF_SAFEALLOC(tmp, GF_SceneManager);
	tmp->streams = gf_list_new();
	tmp->scene_graph = graph;
	return tmp;
}

GF_EXPORT
GF_StreamContext *gf_sm_stream_new(GF_SceneManager *ctx, u16 ES_ID, u8 streamType, u8 objectType)
{
	u32 i;
	GF_StreamContext *tmp;

	i=0;
	while ((tmp = (GF_StreamContext*)gf_list_enum(ctx->streams, &i))) {
		/*we MUST use the same ST*/
		if (tmp->streamType!=streamType) continue;
		/*if no ESID/OTI specified this is a base layer (default stream created by parsers)
		if ESID/OTI specified this is a stream already setup
		*/
		if ( tmp->ESID==ES_ID ) {
			//tmp->objectType = objectType;
			return tmp;
		}
	}

	GF_SAFEALLOC(tmp, GF_StreamContext);
	tmp->AUs = gf_list_new();
	tmp->ESID = ES_ID;
	tmp->streamType = streamType;
	tmp->objectType = objectType ? objectType : 1;
	tmp->timeScale = 1000;
	gf_list_add(ctx->streams, tmp);
	return tmp;
}

GF_StreamContext *gf_sm_stream_find(GF_SceneManager *ctx, u16 ES_ID)
{
	u32 i, count;
	if (!ES_ID) return NULL;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GF_StreamContext *tmp = (GF_StreamContext *)gf_list_get(ctx->streams, i);
		if (tmp->ESID==ES_ID) return tmp;
	}
	return NULL;
}

GF_EXPORT
GF_MuxInfo *gf_sm_get_mux_info(GF_ESD *src)
{
	u32 i;
	GF_MuxInfo *mux;
	i=0;
	while ((mux = (GF_MuxInfo *)gf_list_enum(src->extensionDescriptors, &i))) {
		if (mux->tag == GF_ODF_MUXINFO_TAG) return mux;
	}
	return NULL;
}


static void gf_sm_au_del(GF_StreamContext *sc, GF_AUContext *au)
{
	while (gf_list_count(au->commands)) {
		void *comptr = gf_list_last(au->commands);
		gf_list_rem_last(au->commands);
		switch (sc->streamType) {
		case GF_STREAM_OD:
			gf_odf_com_del((GF_ODCom**) & comptr);
			break;
		case GF_STREAM_SCENE:
			gf_sg_command_del((GF_Command *)comptr);
			break;
		}
	}
	gf_list_del(au->commands);
	gf_free(au);
}

static void gf_sm_reset_stream(GF_StreamContext *sc)
{
	while (gf_list_count(sc->AUs)) {
		GF_AUContext *au = (GF_AUContext *)gf_list_last(sc->AUs);
		gf_list_rem_last(sc->AUs);
		gf_sm_au_del(sc, au);

	}
}

static void gf_sm_delete_stream(GF_StreamContext *sc)
{
	gf_sm_reset_stream(sc);
	gf_list_del(sc->AUs);
	if (sc->name) gf_free(sc->name);
	if (sc->dec_cfg) gf_free(sc->dec_cfg);
	gf_free(sc);
}

GF_EXPORT
void gf_sm_stream_del(GF_SceneManager *ctx, GF_StreamContext *sc)
{
	if (gf_list_del_item(ctx->streams, sc)>=0) {
		gf_sm_delete_stream(sc);
	}
}

GF_EXPORT
void gf_sm_del(GF_SceneManager *ctx)
{
	u32 count;
	while ( (count = gf_list_count(ctx->streams)) ) {
		GF_StreamContext *sc = (GF_StreamContext *)gf_list_get(ctx->streams, count-1);
		gf_list_rem(ctx->streams, count-1);
		gf_sm_delete_stream(sc);
	}
	gf_list_del(ctx->streams);
	if (ctx->root_od) gf_odf_desc_del((GF_Descriptor *) ctx->root_od);
	gf_free(ctx);
}

GF_EXPORT
void gf_sm_reset(GF_SceneManager *ctx)
{
	GF_StreamContext *sc;
	u32 i=0;
	while ( (sc = gf_list_enum(ctx->streams, &i)) ) {
		gf_sm_reset_stream(sc);
	}
	if (ctx->root_od) gf_odf_desc_del((GF_Descriptor *) ctx->root_od);
	ctx->root_od = NULL;
}

GF_EXPORT
GF_AUContext *gf_sm_stream_au_new(GF_StreamContext *stream, u64 timing, Double time_sec, Bool isRap)
{
	u32 i;
	GF_AUContext *tmp;
	u64 tmp_timing;

	tmp_timing = timing ? timing : (u64) (time_sec*1000);
	if (stream->imp_exp_time >= tmp_timing) {
		/*look for existing AU*/
		i=0;
		while ((tmp = (GF_AUContext *)gf_list_enum(stream->AUs, &i))) {
			if (timing && (tmp->timing==timing)) return tmp;
			else if (time_sec && (tmp->timing_sec == time_sec)) return tmp;
			else if (!time_sec && !timing && !tmp->timing && !tmp->timing_sec) return tmp;
			/*insert AU*/
			else if ((time_sec && time_sec<tmp->timing_sec) || (timing && timing<tmp->timing)) {
				GF_SAFEALLOC(tmp, GF_AUContext);
				tmp->commands = gf_list_new();
				if (isRap) tmp->flags = GF_SM_AU_RAP;
				tmp->timing = timing;
				tmp->timing_sec = time_sec;
				tmp->owner = stream;
				gf_list_insert(stream->AUs, tmp, i-1);
				return tmp;
			}
		}
	}
	GF_SAFEALLOC(tmp, GF_AUContext);
	tmp->commands = gf_list_new();
	if (isRap) tmp->flags = GF_SM_AU_RAP;
	tmp->timing = timing;
	tmp->timing_sec = time_sec;
	tmp->owner = stream;
	if (stream->disable_aggregation) tmp->flags |= GF_SM_AU_NOT_AGGREGATED;
	gf_list_add(stream->AUs, tmp);
	stream->imp_exp_time = tmp_timing;
	return tmp;
}

static Bool node_in_commands_subtree(GF_Node *node, GF_List *commands)
{
#ifndef GPAC_DISABLE_VRML
	u32 i, j, count, nb_fields;

	count = gf_list_count(commands);
	for (i=0; i<count; i++) {
		GF_Command *com = gf_list_get(commands, i);
		if (com->tag>=GF_SG_LAST_BIFS_COMMAND) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scene Manager] Command check for LASeR/DIMS not supported\n"));
			return 0;
		}
		if (com->tag==GF_SG_SCENE_REPLACE) {
			if (gf_node_parent_of(com->node, node)) return 1;
			continue;
		}
		nb_fields = gf_list_count(com->command_fields);
		for (j=0; j<nb_fields; j++) {
			GF_CommandField *field = gf_list_get(com->command_fields, j);
			switch (field->fieldType) {
			case GF_SG_VRML_SFNODE:
				if (field->new_node) {
					if (gf_node_parent_of(field->new_node, node)) return 1;
				}
				break;
			case GF_SG_VRML_MFNODE:
				if (field->field_ptr) {
					GF_ChildNodeItem *child;
					child = field->node_list;
					while (child) {
						if (gf_node_parent_of(child->node, node)) return 1;
						child = child->next;
					}
				}
				break;
			}
		}
	}
#endif
	return 0;
}

static u32 store_or_aggregate(GF_StreamContext *sc, GF_Command *com, GF_List *commands, Bool *has_modif)
{
#ifndef GPAC_DISABLE_VRML
	u32 i, count, j, nb_fields;
	GF_CommandField *field, *check_field;

	/*if our command deals with a node inserted in the commands list, apply command list*/
	if (node_in_commands_subtree(com->node, commands)) return 0;

	/*otherwise, check if we can substitute a previous command with this one*/
	count = gf_list_count(commands);
	for (i=0; i<count; i++) {
		GF_Command *check = gf_list_get(commands, i);

		if (sc->streamType == GF_STREAM_SCENE) {
			Bool check_index=0;
			Bool original_is_index = 0;
			Bool apply;
			switch (com->tag) {
			case GF_SG_INDEXED_REPLACE:
				check_index=1;
			case GF_SG_MULTIPLE_INDEXED_REPLACE:
			case GF_SG_FIELD_REPLACE:
			case GF_SG_MULTIPLE_REPLACE:
				if (check->node != com->node) break;
				/*we may aggregate an indexed insertion and a replace one*/
				if (check_index) {
					if (check->tag == GF_SG_INDEXED_REPLACE) {}
					else if (check->tag == GF_SG_INDEXED_INSERT) {
						original_is_index = 1;
					}
					else {
						break;
					}
				} else {
					if (check->tag != com->tag) break;
				}
				nb_fields = gf_list_count(com->command_fields);
				if (gf_list_count(check->command_fields) != nb_fields) break;
				apply=1;
				for (j=0; j<nb_fields; j++) {
					field = gf_list_get(com->command_fields, j);
					check_field = gf_list_get(check->command_fields, j);
					if ((field->pos != check_field->pos) || (field->fieldIndex != check_field->fieldIndex)) {
						apply=0;
						break;
					}
				}
				/*same target node+fields, destroy first command and store new one*/
				if (apply) {
					/*if indexed, change command tag*/
					if (original_is_index) com->tag = GF_SG_INDEXED_INSERT;

					gf_sg_command_del((GF_Command *)check);
					gf_list_rem(commands, i);
					if (has_modif) *has_modif = 1;
					return 1;
				}
				break;

			case GF_SG_NODE_REPLACE:
				if (check->tag != GF_SG_NODE_REPLACE) {
					break;
				}
				/*TODO - THIS IS NOT SUPPORTED IN GPAC SINCE WE NEVER ALLOW FOR DUPLICATE NODE IDs IN THE SCENE !!!*/
				if (gf_node_get_id(check->node) != gf_node_get_id(com->node) ) {
					break;
				}
				/*same node ID, destroy first command and store new one*/
				gf_sg_command_del((GF_Command *)check);
				gf_list_rem(commands, i);
				if (has_modif) *has_modif = 1;
				return 1;

			case GF_SG_INDEXED_DELETE:
				/*look for an indexed insert before the indexed delete with same target pos and node. If found, discard both commands!*/
				if (check->tag != GF_SG_INDEXED_INSERT) break;
				if (com->node != check->node) break;
				field = gf_list_get(com->command_fields, 0);
				check_field = gf_list_get(check->command_fields, 0);
				if (!field || !check_field) break;
				if (field->pos != check_field->pos) break;
				if (field->fieldIndex != check_field->fieldIndex) break;

				gf_sg_command_del((GF_Command *)check);
				gf_list_rem(commands, i);
				if (has_modif) *has_modif = 1;
				return 2;

			default:
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scene Manager] Stream Aggregation not implemented for command - aggregating on main scene\n"));
				break;
			}
		}
	}
	/*the command modifies another stream than associated current carousel stream, we have to store it.*/
	if (has_modif) *has_modif=1;
#endif
	return 1;
}

static GF_StreamContext *gf_sm_get_stream(GF_SceneManager *ctx, u16 ESID)
{
	u32 i, count;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);
		if (sc->ESID==ESID) return sc;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_sm_aggregate(GF_SceneManager *ctx, u16 ESID)
{
	GF_Err e;
	u32 i, stream_count;
#ifndef GPAC_DISABLE_VRML
	u32 j;
	GF_AUContext *au;
	GF_Command *com;
#endif

	e = GF_OK;

#if DEBUG_RAP
	com_count = 0;
	stream_count = gf_list_count(ctx->streams);
	for (i=0; i<stream_count; i++) {
		GF_StreamContext *sc = (GF_StreamContext *)gf_list_get(ctx->streams, i);
		if (sc->streamType == GF_STREAM_SCENE) {
			au_count = gf_list_count(sc->AUs);
			for (j=0; j<au_count; j++) {
				au = (GF_AUContext *)gf_list_get(sc->AUs, j);
				com_count += gf_list_count(au->commands);
			}
		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_SCENE, ("[SceneManager] Making RAP with %d commands\n", com_count));
#endif

	stream_count = gf_list_count(ctx->streams);
	for (i=0; i<stream_count; i++) {
		GF_AUContext *carousel_au;
		GF_List *carousel_commands;
		GF_StreamContext *aggregate_on_stream;
		GF_StreamContext *sc = (GF_StreamContext *)gf_list_get(ctx->streams, i);
		if (ESID && (sc->ESID!=ESID)) continue;

		/*locate the AU in which our commands will be aggregated*/
		carousel_au = NULL;
		carousel_commands = NULL;
		aggregate_on_stream = sc->aggregate_on_esid ? gf_sm_get_stream(ctx, sc->aggregate_on_esid) : NULL;
		if (aggregate_on_stream==sc) {
			carousel_commands = gf_list_new();
		} else if (aggregate_on_stream) {
			if (!gf_list_count(aggregate_on_stream->AUs)) {
				carousel_au = gf_sm_stream_au_new(aggregate_on_stream, 0, 0, 1);
			} else {
				/* assert we already performed aggregation */
				assert(gf_list_count(aggregate_on_stream->AUs)==1);
				carousel_au = gf_list_get(aggregate_on_stream->AUs, 0);
			}
			carousel_commands = carousel_au->commands;
		}
		/*TODO - do this as well for ODs*/
#ifndef GPAC_DISABLE_VRML
		if (sc->streamType == GF_STREAM_SCENE) {
			Bool has_modif = 0;
			/*we check for each stream if it is a base stream (SceneReplace ...) - several streams may carry RAPs if inline nodes are used*/
			Bool base_stream_found = 0;

			/*in DIMS we use an empty initial AU with no commands to signal the RAP*/
			if (sc->objectType == GPAC_OTI_SCENE_DIMS) base_stream_found = 1;

			/*apply all commands - this will also apply the SceneReplace*/
			while (gf_list_count(sc->AUs)) {
				u32 count;
				au = (GF_AUContext *) gf_list_get(sc->AUs, 0);
				gf_list_rem(sc->AUs, 0);

				/*AU not aggregated*/
				if (au->flags & GF_SM_AU_NOT_AGGREGATED) {
					gf_sm_au_del(sc, au);
					continue;
				}

				count = gf_list_count(au->commands);

				for (j=0; j<count; j++) {
					u32 store=0;
					com = gf_list_get(au->commands, j);
					if (!base_stream_found) {
						switch (com->tag) {
						case GF_SG_SCENE_REPLACE:
						case GF_SG_LSR_NEW_SCENE:
						case GF_SG_LSR_REFRESH_SCENE:
							base_stream_found = 1;
							break;
						}
					}

					/*aggregate the command*/

					/*if stream doesn't carry a carousel or carries the base carousel (scene replace), always apply the command*/
					if (base_stream_found || !sc->aggregate_on_esid) {
						store = 0;
					}
					/*otherwise, check wether the command should be kept in this stream as is, or can be aggregated on this stream*/
					else {
						switch (com->tag) {
						/*the following commands do not impact a sub-tree (eg do not deal with nodes), we cannot
						aggregate them... */
						case GF_SG_ROUTE_REPLACE:
						case GF_SG_ROUTE_DELETE:
						case GF_SG_ROUTE_INSERT:
						case GF_SG_PROTO_INSERT:
						case GF_SG_PROTO_DELETE:
						case GF_SG_PROTO_DELETE_ALL:
						case GF_SG_GLOBAL_QUANTIZER:
						case GF_SG_LSR_RESTORE:
						case GF_SG_LSR_SAVE:
						case GF_SG_LSR_SEND_EVENT:
						case GF_SG_LSR_CLEAN:
							/*todo check in which category to put these commands*/
//						case GF_SG_LSR_ACTIVATE:
//						case GF_SG_LSR_DEACTIVATE:
							store = 1;
							break;
						/*other commands:
							!!! we need to know if the target node of the command has been inserted in this stream !!!

						This is a tedious task, for now we will consider the following cases:
							- locate a similar command in the stored list: remove the similar one and aggregate on stream
							- by default all AUs are stored if the stream is in aggregate mode - we should fix that by checking insertion points:
							 if a command apllies on a node that has been inserted in this stream, we can aggregate, otherwise store
						*/
						default:
							/*check if we can directly store the command*/
							assert(carousel_commands);
							store = store_or_aggregate(sc, com, carousel_commands, &has_modif);
							break;
						}
					}

					switch (store) {
					/*command has been merged with a previous command in carousel and needs to be destroyed*/
					case 2:
						gf_list_rem(au->commands, j);
						j--;
						count--;
						gf_sg_command_del((GF_Command *)com);
						break;
					/*command shall be moved to carousel without being applied*/
					case 1:
						gf_list_insert(carousel_commands, com, 0);
						gf_list_rem(au->commands, j);
						j--;
						count--;
						break;
					/*command can be applied*/
					default:
						e = gf_sg_command_apply(ctx->scene_graph, com, 0);
						break;
					}
				}
				gf_sm_au_del(sc, au);
			}

			/*and recreate scene replace*/
			if (base_stream_found) {
				au = gf_sm_stream_au_new(sc, 0, 0, 1);

				switch (sc->objectType) {
				case GPAC_OTI_SCENE_BIFS:
				case GPAC_OTI_SCENE_BIFS_V2:
					com = gf_sg_command_new(ctx->scene_graph, GF_SG_SCENE_REPLACE);
					break;
				case GPAC_OTI_SCENE_LASER:
					com = gf_sg_command_new(ctx->scene_graph, GF_SG_LSR_NEW_SCENE);
					break;
				case GPAC_OTI_SCENE_DIMS:
				/* We do not create a new command, empty AU is enough in DIMS*/
				default:
					com = NULL;
					break;
				}

				if (com) {
					com->node = ctx->scene_graph->RootNode;
					ctx->scene_graph->RootNode = NULL;
					gf_list_del(com->new_proto_list);
					com->new_proto_list = ctx->scene_graph->protos;
					ctx->scene_graph->protos = NULL;
					/*indicate the command is the aggregated scene graph, so that PROTOs and ROUTEs
					are taken from the scenegraph when encoding*/
					com->aggregated = 1;
					gf_list_add(au->commands, com);
				}
			}
			/*update carousel flags of the AU*/
			else if (carousel_commands) {
				/*if current stream caries its own carousel*/
				if (!carousel_au) {
					carousel_au = gf_sm_stream_au_new(sc, 0, 0, 1);
					gf_list_del(carousel_au->commands);
					carousel_au->commands = carousel_commands;
				}
				carousel_au->flags |= GF_SM_AU_RAP | GF_SM_AU_CAROUSEL;
				if (has_modif) carousel_au->flags |= GF_SM_AU_MODIFIED;
			}
		}
#endif
	}
	return e;
}

#ifndef GPAC_DISABLE_LOADER_BT
GF_Err gf_sm_load_init_bt(GF_SceneLoader *load);
#endif

#ifndef GPAC_DISABLE_LOADER_XMT
GF_Err gf_sm_load_init_xmt(GF_SceneLoader *load);
#endif

#ifndef GPAC_DISABLE_LOADER_ISOM
GF_Err gf_sm_load_init_isom(GF_SceneLoader *load);
#endif

#ifndef GPAC_DISABLE_SVG

GF_Err gf_sm_load_init_svg(GF_SceneLoader *load);

GF_Err gf_sm_load_init_xbl(GF_SceneLoader *load);
GF_Err gf_sm_load_run_xbl(GF_SceneLoader *load);
void gf_sm_load_done_xbl(GF_SceneLoader *load);
#endif

#ifndef GPAC_DISABLE_SWF_IMPORT
GF_Err gf_sm_load_init_swf(GF_SceneLoader *load);
#endif


#ifndef GPAC_DISABLE_QTVR

GF_Err gf_sm_load_init_qt(GF_SceneLoader *load);
#endif



GF_EXPORT
GF_Err gf_sm_load_string(GF_SceneLoader *load, const char *str, Bool do_clean)
{
	GF_Err e;
	if (!load->type) e = GF_BAD_PARAM;
	else if (load->parse_string) e = load->parse_string(load, str);
	else e = GF_NOT_SUPPORTED;

	return e;
}


/*initializes the context loader*/
GF_EXPORT
GF_Err gf_sm_load_init(GF_SceneLoader *load)
{
	GF_Err e = GF_NOT_SUPPORTED;
	char *ext, szExt[50];
	/*we need at least a scene graph*/
	if (!load || (!load->ctx && !load->scene_graph)
#ifndef GPAC_DISABLE_ISOM
	        || (!load->fileName && !load->isom && !(load->flags & GF_SM_LOAD_FOR_PLAYBACK) )
#endif
	   ) return GF_BAD_PARAM;

	if (!load->type) {
#ifndef GPAC_DISABLE_ISOM
		if (load->isom) {
			load->type = GF_SM_LOAD_MP4;
		} else
#endif
		{
			ext = (char *)strrchr(load->fileName, '.');
			if (!ext) return GF_NOT_SUPPORTED;
			if (!stricmp(ext, ".gz")) {
				char *anext;
				ext[0] = 0;
				anext = (char *)strrchr(load->fileName, '.');
				ext[0] = '.';
				ext = anext;
			}
			strcpy(szExt, &ext[1]);
			strlwr(szExt);
			if (strstr(szExt, "bt")) load->type = GF_SM_LOAD_BT;
			else if (strstr(szExt, "wrl")) load->type = GF_SM_LOAD_VRML;
			else if (strstr(szExt, "x3dv")) load->type = GF_SM_LOAD_X3DV;
#ifndef GPAC_DISABLE_LOADER_XMT
			else if (strstr(szExt, "xmt") || strstr(szExt, "xmta")) load->type = GF_SM_LOAD_XMTA;
			else if (strstr(szExt, "x3d")) load->type = GF_SM_LOAD_X3D;
#endif
			else if (strstr(szExt, "swf")) load->type = GF_SM_LOAD_SWF;
			else if (strstr(szExt, "mov")) load->type = GF_SM_LOAD_QT;
			else if (strstr(szExt, "svg")) load->type = GF_SM_LOAD_SVG;
			else if (strstr(szExt, "xsr")) load->type = GF_SM_LOAD_XSR;
			else if (strstr(szExt, "xbl")) load->type = GF_SM_LOAD_XBL;
			else if (strstr(szExt, "xml")) {
				char *rtype = gf_xml_get_root_type(load->fileName, &e);
				if (rtype) {
					if (!strcmp(rtype, "SAFSession")) load->type = GF_SM_LOAD_XSR;
					else if (!strcmp(rtype, "XMT-A")) load->type = GF_SM_LOAD_XMTA;
					else if (!strcmp(rtype, "X3D")) load->type = GF_SM_LOAD_X3D;
					else if (!strcmp(rtype, "bindings")) load->type = GF_SM_LOAD_XBL;

					gf_free(rtype);
				}
			}
		}
	}
	if (!load->type) return e;

	if (!load->scene_graph) load->scene_graph = load->ctx->scene_graph;

	switch (load->type) {
#ifndef GPAC_DISABLE_LOADER_BT
	case GF_SM_LOAD_BT:
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		return gf_sm_load_init_bt(load);
#endif

#ifndef GPAC_DISABLE_LOADER_XMT
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		return gf_sm_load_init_xmt(load);
#endif

#ifndef GPAC_DISABLE_SVG
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_XSR:
	case GF_SM_LOAD_DIMS:
		return gf_sm_load_init_svg(load);

	case GF_SM_LOAD_XBL:
		e = gf_sm_load_init_xbl(load);

		load->process = gf_sm_load_run_xbl;;
		load->done = gf_sm_load_done_xbl;
		return e;
#endif

#ifndef GPAC_DISABLE_SWF_IMPORT
	case GF_SM_LOAD_SWF:
		return gf_sm_load_init_swf(load);
#endif

#ifndef GPAC_DISABLE_LOADER_ISOM
	case GF_SM_LOAD_MP4:
		return gf_sm_load_init_isom(load);
#endif

#ifndef GPAC_DISABLE_QTVR
	case GF_SM_LOAD_QT:
		return gf_sm_load_init_qt(load);
#endif
	default:
		return GF_NOT_SUPPORTED;
	}
	return GF_NOT_SUPPORTED;
}

GF_EXPORT
void gf_sm_load_done(GF_SceneLoader *load)
{
	if (load->done) load->done(load);
}

GF_EXPORT
GF_Err gf_sm_load_run(GF_SceneLoader *load)
{
	if (load->process) return load->process(load);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sm_load_suspend(GF_SceneLoader *load, Bool suspend)
{
	if (load->suspend) return load->suspend(load, suspend);
	return GF_OK;
}

#if !defined(GPAC_DISABLE_LOADER_BT) || !defined(GPAC_DISABLE_LOADER_XMT)
#include <gpac/base_coding.h>
void gf_sm_update_bitwrapper_buffer(GF_Node *node, const char *fileName)
{
	u32 data_size = 0;
	char *data = NULL;
	char *buffer;
	M_BitWrapper *bw = (M_BitWrapper *)node;

	if (!bw->buffer.buffer) return;
	buffer = bw->buffer.buffer;
	if (!strnicmp(buffer, "file://", 7)) {
		char *url = gf_url_concatenate(fileName, buffer+7);
		if (url) {
			FILE *f = gf_fopen(url, "rb");
			if (f) {
				fseek(f, 0, SEEK_END);
				data_size = (u32) ftell(f);
				fseek(f, 0, SEEK_SET);
				data = gf_malloc(sizeof(char)*data_size);
				if (data) {
					if (fread(data, 1, data_size, f) != data_size) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_SCENE, ("[Scene Manager] error reading bitwrapper file %s\n", url));
					}
				}
				gf_fclose(f);
			}
			gf_free(url);
		}
	} else {
		Bool base_64 = 0;
		if (!strnicmp(buffer, "data:application/octet-string", 29)) {
			char *sep = strchr(bw->buffer.buffer, ',');
			base_64 = strstr(bw->buffer.buffer, ";base64") ? 1 : 0;
			if (sep) buffer = sep+1;
		}

		if (base_64) {
			data_size = 2 * (u32) strlen(buffer);
			data = (char*)gf_malloc(sizeof(char)*data_size);
			if (data)
				data_size = gf_base64_decode(buffer, (u32) strlen(buffer), data, data_size);
		} else {
			u32 i, c;
			char s[3];
			data_size = (u32) strlen(buffer) / 3;
			data = (char*)gf_malloc(sizeof(char) * data_size);
			if (data) {
				s[2] = 0;
				for (i=0; i<data_size; i++) {
					s[0] = buffer[3*i+1];
					s[1] = buffer[3*i+2];
					sscanf(s, "%02X", &c);
					data[i] = (unsigned char) c;
				}
			}
		}
	}
	gf_free(bw->buffer.buffer);
	bw->buffer.buffer = NULL;
	bw->buffer_len = 0;
	if (data) {
		bw->buffer.buffer = data;
		bw->buffer_len = data_size;
	}

}
#endif //!defined(GPAC_DISABLE_LOADER_BT) || !defined(GPAC_DISABLE_LOADER_XMT)


