/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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
#include <gpac/internal/scenegraph_dev.h>



static Bool gf_node_in_table_by_tag(u32 tag, u32 NDTType)
{
	if (!tag) return 0;
	if (tag==TAG_ProtoNode) return 1;
	else if (tag<=GF_NODE_RANGE_LAST_MPEG4) {
		u32 i;
		u32 gf_bifs_get_node_type(u32 NDT_Tag, u32 NodeTag, u32 Version);

		for (i=0;i<GF_BIFS_LAST_VERSION; i++) {
			if (gf_bifs_get_node_type(NDTType, tag, i+1)) return 1;
		}
		return 0;
	} else if (tag<=GF_NODE_RANGE_LAST_X3D) {
		Bool X3D_IsNodeInTable(u32 NDT_Tag, u32 NodeTag);
		return X3D_IsNodeInTable(NDTType, tag);
	}
	return 0;
}

Bool gf_node_in_table(GF_Node *node, u32 NDTType)
{
	u32 tag = node ? node->sgprivate->tag : 0;
	if (tag==TAG_ProtoNode) {
		tag = gf_sg_proto_get_render_tag(((GF_ProtoInstance *)node)->proto_interface);
		if (tag==TAG_UndefinedNode) return 1;
	}
	return gf_node_in_table_by_tag(tag, NDTType);
}


GF_SceneManager *gf_sm_new(GF_SceneGraph *graph)
{
	GF_SceneManager *tmp;
	
	if (!graph) return NULL;
	GF_SAFEALLOC(tmp, sizeof(GF_SceneManager));
	tmp->streams = gf_list_new();
	tmp->scene_graph = graph;
	return tmp;
}

GF_StreamContext *gf_sm_stream_new(GF_SceneManager *ctx, u16 ES_ID, u8 streamType, u8 objectType)
{
	u32 i;
	GF_StreamContext *tmp;

	i=0;
	while ((tmp = gf_list_enum(ctx->streams, &i))) {
		/*we MUST use the same ST*/
		if (tmp->streamType!=streamType) continue;
		/*if no ESID/OTI specified this is a base layer (default stream created by parsers)
		if ESID/OTI specified this is a stream already setup
		*/
		if ( tmp->ESID==ES_ID ){
			//tmp->objectType = objectType;
			return tmp;
		}
	}
	
	tmp = malloc(sizeof(GF_StreamContext));
	memset(tmp , 0, sizeof(GF_StreamContext));
	tmp->AUs = gf_list_new();
	tmp->ESID = ES_ID;
	tmp->streamType = streamType;
	tmp->objectType = objectType;
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
		GF_StreamContext *tmp = gf_list_get(ctx->streams, i);
		if (tmp->ESID==ES_ID) return tmp;
	}
	return NULL;
}
static void gf_sm_delete_stream(GF_StreamContext *sc)
{
	while (gf_list_count(sc->AUs)) {
		GF_AUContext *au = gf_list_last(sc->AUs);
		gf_list_rem_last(sc->AUs);

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
		free(au);
	}
	gf_list_del(sc->AUs);
	free(sc);
}

void gf_sm_stream_del(GF_SceneManager *ctx, GF_StreamContext *sc)
{
	if (gf_list_del_item(ctx->streams, sc)>=0) {
		gf_sm_delete_stream(sc);
	}
}

void gf_sm_del(GF_SceneManager *ctx)
{
	u32 count;
	while ( (count = gf_list_count(ctx->streams)) ) {
		GF_StreamContext *sc = gf_list_get(ctx->streams, count-1);
		gf_list_rem(ctx->streams, count-1);
		gf_sm_delete_stream(sc);
	}
	gf_list_del(ctx->streams);
	if (ctx->root_od) gf_odf_desc_del((GF_Descriptor *) ctx->root_od);
	free(ctx);
}


GF_AUContext *gf_sm_stream_au_new(GF_StreamContext *stream, u64 timing, Double time_sec, Bool isRap)
{
	u32 i;
	GF_AUContext *tmp;

	/*look for existing AU*/
	i=0;
	while ((tmp = gf_list_enum(stream->AUs, &i))) {
		if (timing && (tmp->timing==timing)) return tmp;
		else if (time_sec && (tmp->timing_sec == time_sec)) return tmp;
		else if (!time_sec && !timing && !tmp->timing && !tmp->timing_sec) return tmp;
		/*insert AU*/
		else if ((time_sec && time_sec<tmp->timing_sec) || (timing && timing<tmp->timing)) {
			tmp = malloc(sizeof(GF_AUContext));
			tmp->commands = gf_list_new();
			tmp->is_rap = isRap;
			tmp->timing = timing;
			tmp->timing_sec = time_sec;
			tmp->owner = stream;
			gf_list_insert(stream->AUs, tmp, i);
			return tmp;
		}
	}
	tmp = malloc(sizeof(GF_AUContext));
	tmp->commands = gf_list_new();
	tmp->is_rap = isRap;
	tmp->timing = timing;
	tmp->timing_sec = time_sec;
	tmp->owner = stream;
	gf_list_add(stream->AUs, tmp);
	return tmp;
}

GF_Err gf_sm_make_random_access(GF_SceneManager *ctx)
{
	GF_Err e;
	u32 i, j, stream_count, au_count, com_count;
	GF_AUContext *au;
	GF_Command *com;

	e = GF_OK;
	stream_count = gf_list_count(ctx->streams);
	for (i=0; i<stream_count; i++) {
		GF_StreamContext *sc = gf_list_get(ctx->streams, i);
		/*FIXME - do this as well for ODs*/
		if (sc->streamType == GF_STREAM_SCENE) {
			/*apply all commands - this will also apply the SceneReplace*/
			j=0;
			while ((au = gf_list_enum(sc->AUs, &j))) {
				e = gf_sg_command_apply_list(ctx->scene_graph, au->commands, 0);
				if (e) return e;
			}

			/* Delete all the commands in the stream */
			while ( (au_count = gf_list_count(sc->AUs)) ) {
				au = gf_list_get(sc->AUs, au_count-1);
				gf_list_rem(sc->AUs, au_count-1);
				while ( (com_count = gf_list_count(au->commands)) ) {
					com = gf_list_get(au->commands, com_count - 1);
					gf_list_rem(au->commands, com_count - 1);
					gf_sg_command_del(com);
				}
				gf_list_del(au->commands);
				free(au);
			}
			/*and recreate scene replace*/
			au = gf_sm_stream_au_new(sc, 0, 0, 1);
			com = gf_sg_command_new(ctx->scene_graph, GF_SG_SCENE_REPLACE);
			com->node = ctx->scene_graph->RootNode;
			ctx->scene_graph->RootNode = NULL;
			gf_list_del(com->new_proto_list);
			com->new_proto_list = ctx->scene_graph->protos;
			ctx->scene_graph->protos = NULL;
			/*FIXME - check routes & protos*/
			gf_list_add(au->commands, com);
		}
	}
	return e;
}


GF_Err gf_sm_load_init_BT(GF_SceneLoader *load);
void gf_sm_load_done_BT(GF_SceneLoader *load);
GF_Err gf_sm_load_run_BT(GF_SceneLoader *load);
GF_Err gf_sm_load_init_BTString(GF_SceneLoader *load, char *str);
GF_Err gf_sm_load_done_BTString(GF_SceneLoader *load);

GF_Err gf_sm_load_init_xmt(GF_SceneLoader *load);
void gf_sm_load_done_xmt(GF_SceneLoader *load);
GF_Err gf_sm_load_run_xmt(GF_SceneLoader *load);
GF_Err gf_sm_load_init_xmt_string(GF_SceneLoader *load, char *str);
GF_Err gf_sm_load_done_xmt_string(GF_SceneLoader *load);


GF_Err gf_sm_load_init_MP4(GF_SceneLoader *load);
void gf_sm_load_done_MP4(GF_SceneLoader *load);
GF_Err gf_sm_load_run_MP4(GF_SceneLoader *load);

#ifndef GPAC_DISABLE_SVG
GF_Err gf_sm_load_init_SVG(GF_SceneLoader *load);
GF_Err gf_sm_load_done_SVG(GF_SceneLoader *load);
GF_Err gf_sm_load_run_SVG(GF_SceneLoader *load);
GF_Err gf_sm_load_init_SVGString(GF_SceneLoader *load, char *str);
#endif


#ifndef GPAC_READ_ONLY

GF_Err gf_sm_load_init_SWF(GF_SceneLoader *load);
void gf_sm_load_done_SWF(GF_SceneLoader *load);
GF_Err gf_sm_load_run_SWF(GF_SceneLoader *load);

GF_Err gf_sm_load_init_QT(GF_SceneLoader *load);
void gf_sm_load_done_QT(GF_SceneLoader *load);
GF_Err gf_sm_load_run_QT(GF_SceneLoader *load);
#endif


static GF_Err gf_sm_load_init_from_string(GF_SceneLoader *load, char *str)
{

	/*we need at least a scene graph*/
	if (!load || (!load->ctx && !load->scene_graph)) return GF_BAD_PARAM;

	if (!load->type) return GF_NOT_SUPPORTED;

	if (!load->scene_graph) load->scene_graph = load->ctx->scene_graph;

	switch (load->type) {
	case GF_SM_LOAD_BT: 
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		return gf_sm_load_init_BTString(load, str);
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		return gf_sm_load_init_xmt_string(load, str);
#ifndef GPAC_DISABLE_SVG
	case GF_SM_LOAD_SVG: 
	case GF_SM_LOAD_XSR: 
		return gf_sm_load_init_SVGString(load, str);
#endif
	case GF_SM_LOAD_SWF: 
		return GF_NOT_SUPPORTED;
#ifndef GPAC_READ_ONLY
	case GF_SM_LOAD_MP4:
		return GF_NOT_SUPPORTED;
#endif
	}
	return GF_NOT_SUPPORTED;
}

static void gf_sm_load_done_string(GF_SceneLoader *load)
{
	switch (load->type) {
	case GF_SM_LOAD_BT:
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		gf_sm_load_done_BTString(load); 
		break;
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		/*we do not reset it here to enable SAX parsing*/
		break;
#ifndef GPAC_DISABLE_SVG
	/*we do not reset it here to enable SAX parsing*/
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_XSR:
		break;
#endif
	default: 
		break;
	}
}

GF_Err gf_sm_load_string(GF_SceneLoader *load, char *str)
{
	GF_Err e = gf_sm_load_init_from_string(load, str);
	if (e) return e;
	e = gf_sm_load_run(load);
	gf_sm_load_done_string(load);
	return e;
}


/*initializes the context loader*/
GF_Err gf_sm_load_init(GF_SceneLoader *load)
{
	char *ext, szExt[50];
	/*we need at least a scene graph*/
	if (!load || (!load->ctx && !load->scene_graph) || (!load->fileName && !load->isom)) return GF_BAD_PARAM;

	if (!load->type) {
		if (load->isom) {
			load->type = GF_SM_LOAD_MP4;
		} else {
			ext = strrchr(load->fileName, '.');
			if (!ext) return GF_NOT_SUPPORTED;
			if (!stricmp(ext, ".gz")) {
				char *anext;
				ext[0] = 0;
				anext = strrchr(load->fileName, '.');
				ext[0] = '.';
				ext = anext;
			}
			strcpy(szExt, &ext[1]);
			strlwr(szExt);
			if (strstr(szExt, "bt")) load->type = GF_SM_LOAD_BT;
			else if (strstr(szExt, "wrl")) load->type = GF_SM_LOAD_VRML;
			else if (strstr(szExt, "x3dv")) load->type = GF_SM_LOAD_X3DV;
			else if (strstr(szExt, "xmt") || strstr(szExt, "xmta")) load->type = GF_SM_LOAD_XMTA;
			else if (strstr(szExt, "x3d")) load->type = GF_SM_LOAD_X3D;
			else if (strstr(szExt, "swf")) load->type = GF_SM_LOAD_SWF;
			else if (strstr(szExt, "mov")) load->type = GF_SM_LOAD_QT;
			else if (strstr(szExt, "svg")) load->type = GF_SM_LOAD_SVG;
			else if (strstr(szExt, "xsr")) load->type = GF_SM_LOAD_XSR;
		}
	}
	if (!load->type) return GF_NOT_SUPPORTED;

	if (!load->scene_graph) load->scene_graph = load->ctx->scene_graph;

	switch (load->type) {
	case GF_SM_LOAD_BT: 
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		return gf_sm_load_init_BT(load);
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		return gf_sm_load_init_xmt(load);
#ifndef GPAC_DISABLE_SVG
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_XSR:
		return gf_sm_load_init_SVG(load);
#endif
#ifndef GPAC_READ_ONLY
	case GF_SM_LOAD_SWF: 
		return gf_sm_load_init_SWF(load);
	case GF_SM_LOAD_QT: 
		return gf_sm_load_init_QT(load);
	case GF_SM_LOAD_MP4:
		return gf_sm_load_init_MP4(load);
#endif
	}
	return GF_NOT_SUPPORTED;
}

void gf_sm_load_done(GF_SceneLoader *load)
{
	switch (load->type) {
	case GF_SM_LOAD_BT:
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		gf_sm_load_done_BT(load); 
		break;
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		gf_sm_load_done_xmt(load); 
		break;
#ifndef GPAC_DISABLE_SVG
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_XSR:
		gf_sm_load_done_SVG(load);
		break;
#endif
#ifndef GPAC_READ_ONLY
	case GF_SM_LOAD_SWF: 
		gf_sm_load_done_SWF(load); 
		break;
	case GF_SM_LOAD_QT: 
		gf_sm_load_done_QT(load); 
		break;
	case GF_SM_LOAD_MP4: 
		gf_sm_load_done_MP4(load); 
		break;
#endif
	}
}

GF_Err gf_sm_load_run(GF_SceneLoader *load)
{
	switch (load->type) {
	case GF_SM_LOAD_BT:
	case GF_SM_LOAD_VRML:
	case GF_SM_LOAD_X3DV:
		return gf_sm_load_run_BT(load);
	case GF_SM_LOAD_XMTA:
	case GF_SM_LOAD_X3D:
		return gf_sm_load_run_xmt(load);
#ifndef GPAC_DISABLE_SVG
	case GF_SM_LOAD_SVG:
	case GF_SM_LOAD_XSR:
		return gf_sm_load_run_SVG(load);
#endif

#ifndef GPAC_READ_ONLY
	case GF_SM_LOAD_SWF:
		return gf_sm_load_run_SWF(load);
	case GF_SM_LOAD_QT: 
		return gf_sm_load_run_QT(load);
	case GF_SM_LOAD_MP4: 
		return gf_sm_load_run_MP4(load);
#endif
	default:
		return GF_BAD_PARAM;
	}
}

