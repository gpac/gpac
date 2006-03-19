/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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


#include <gpac/internal/bifs_dev.h>
#include <gpac/mpeg4_odf.h>


static GF_Err ParseConfig(GF_BitStream *bs, BIFSStreamInfo *info, u32 version)
{
	Bool hasSize, cmd_stream;

	if (version==2) {
		info->config.Use3DMeshCoding = gf_bs_read_int(bs, 1);
		info->config.UsePredictiveMFField = gf_bs_read_int(bs, 1);
	}
	info->config.NodeIDBits = gf_bs_read_int(bs, 5);
	info->config.RouteIDBits = gf_bs_read_int(bs, 5);
	if (version==2) {
		info->config.ProtoIDBits = gf_bs_read_int(bs, 5);
	}
	cmd_stream = gf_bs_read_int(bs, 1);

	if (cmd_stream) {
		info->config.PixelMetrics = gf_bs_read_int(bs, 1);
		hasSize = gf_bs_read_int(bs, 1);
		if (hasSize) {
			info->config.Width = gf_bs_read_int(bs, 16);
			info->config.Height = gf_bs_read_int(bs, 16);
		}
		gf_bs_align(bs);

		if (gf_bs_get_size(bs) != gf_bs_get_position(bs)) return GF_ODF_INVALID_DESCRIPTOR;
		return GF_OK;
	} else {
		info->config.BAnimRAP = gf_bs_read_int(bs, 1);
		info->config.elementaryMasks = gf_list_new();
		while (1) {
		  /*u32 node_id = */gf_bs_read_int(bs, info->config.NodeIDBits);
			/*this assumes only FDP, BDP and IFS2D (no elem mask)*/
			if (gf_bs_read_int(bs, 1) == 0) break;
		}
		gf_bs_align(bs);
		if (gf_bs_get_size(bs) != gf_bs_get_position(bs))  return GF_NOT_SUPPORTED;
		return GF_OK;
	}
}

static void bifs_info_del(BIFSStreamInfo *info)
{
	while (1) {
		BIFSElementaryMask *em = gf_list_last(info->config.elementaryMasks);
		if (!em) break;
		gf_list_rem_last(info->config.elementaryMasks);
		free(em);
	}
	free(info);
}

GF_BifsDecoder *gf_bifs_decoder_new(GF_SceneGraph *scenegraph, Bool command_dec)
{
	GF_BifsDecoder * tmp = malloc(sizeof(GF_BifsDecoder));
	memset(tmp, 0, sizeof(GF_BifsDecoder));

	tmp->QPs = gf_list_new();
	tmp->streamInfo = gf_list_new();
	tmp->info = NULL;	

	tmp->pCurrentProto = NULL;
	tmp->scenegraph = scenegraph;
	if (command_dec) {
		tmp->dec_memory_mode = 1;
		tmp->force_keep_qp = 1;
		tmp->conditionals = gf_list_new();
	}
	tmp->current_graph = NULL;
//	tmp->mx = gf_mx_new();
	return tmp;
}


BIFSStreamInfo *gf_bifs_dec_get_stream(GF_BifsDecoder * codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	while ((ptr = gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}

GF_Err gf_bifs_decoder_configure_stream(GF_BifsDecoder * codec, u16 ESID, char *DecoderSpecificInfo, u32 DecoderSpecificInfoLength, u32 objectTypeIndication)
{
	GF_BitStream *bs;
	BIFSStreamInfo *pInfo;
	GF_Err e;
	
	if (!DecoderSpecificInfo) return GF_BAD_PARAM;
//	gf_mx_p(codec->mx);
	if (gf_bifs_dec_get_stream(codec, ESID) != NULL) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}

	
	bs = gf_bs_new(DecoderSpecificInfo, DecoderSpecificInfoLength, GF_BITSTREAM_READ);
	pInfo = malloc(sizeof(BIFSStreamInfo));
	memset(pInfo, 0, sizeof(BIFSStreamInfo));
	pInfo->ESID = ESID;

	pInfo->config.version = objectTypeIndication;
	/*parse config with indicated oti*/
	e = ParseConfig(bs, pInfo, (u32) objectTypeIndication);
	if (e) {
		pInfo->ESID = ESID;
		/*some content indicates a wrong OTI, so try to parse with v1 or v2*/
		gf_bs_seek(bs, 0);
		/*try with reverse config*/
		e = ParseConfig(bs, pInfo, (objectTypeIndication==2) ? 1 : 2);
		pInfo->config.version = (objectTypeIndication==2) ? 1 : 2;
	}

	if (e && (e != GF_ODF_INVALID_DESCRIPTOR)) {
		free(pInfo);
		gf_bs_del(bs);
		return GF_BIFS_UNKNOWN_VERSION;
	}
	gf_bs_del(bs);

	//first stream, configure size
	if (!codec->ignore_size && !gf_list_count(codec->streamInfo)) {
		gf_sg_set_scene_size_info(codec->scenegraph, pInfo->config.Width, pInfo->config.Height, pInfo->config.PixelMetrics);
	}

	gf_list_add(codec->streamInfo, pInfo);
//	gf_mx_v(codec->mx);
	return GF_OK;
}

void gf_bifs_decoder_ignore_size_info(GF_BifsDecoder *codec)
{
	if (codec) codec->ignore_size = 1;
}


GF_Err gf_bifs_decoder_remove_stream(GF_BifsDecoder *codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	while ((ptr = gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) {
			free(ptr);
			gf_list_rem(codec->streamInfo, i-1);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

void gf_bifs_decoder_del(GF_BifsDecoder *codec)
{	
	if (codec->GlobalQP) gf_node_unregister((GF_Node *) codec->GlobalQP, NULL);
	
	assert(gf_list_count(codec->QPs)==0);
	gf_list_del(codec->QPs);

	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		BIFSStreamInfo *p = gf_list_get(codec->streamInfo, 0);
		bifs_info_del(p);
		gf_list_rem(codec->streamInfo, 0);
	}
	gf_list_del(codec->streamInfo);
	if (codec->dec_memory_mode) {
		assert(gf_list_count(codec->conditionals) == 0);
		gf_list_del(codec->conditionals);
	}
//	gf_mx_del(codec->mx);
	free(codec);
}


void BD_EndOfStream(void *co)
{
	((GF_BifsDecoder *) co)->LastError = GF_IO_ERR;
}

void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

GF_Err gf_bifs_decode_au(GF_BifsDecoder *codec, u16 ESID, char *data, u32 data_length, Double ts_offset)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!codec || !data || codec->dec_memory_mode) return GF_BAD_PARAM;

//	gf_mx_p(codec->mx);
	codec->info = gf_bifs_dec_get_stream(codec, ESID);
	if (!codec->info) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}
	/*setup current scene graph*/
	codec->current_graph = codec->scenegraph;
	codec->cts_offset = ts_offset;

	bs = gf_bs_new(data, data_length, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(bs, BD_EndOfStream, codec);

	if (codec->info->config.elementaryMasks) {
		e = GF_NOT_SUPPORTED;
	} else {
		e = gf_bifs_dec_command(codec, bs);
	}
	gf_bs_del(bs);
	/*reset current config*/
	codec->info = NULL;
	codec->current_graph = NULL;

//	gf_mx_v(codec->mx);
	return e;
}
	

GF_Node *gf_bifs_dec_find_node(GF_BifsDecoder * codec, u32 NodeID)
{	
	assert(codec->current_graph);
	return gf_sg_find_node(codec->current_graph, NodeID);
}



void gf_bifs_decoder_set_time_offset(GF_BifsDecoder *codec, Double ts)
{
	codec->cts_offset = ts;
}

GF_Node *gf_bifs_enc_find_node(GF_BifsEncoder *codec, u32 nodeID)
{
	if (codec->current_proto_graph) return gf_sg_find_node(codec->current_proto_graph, nodeID);
	assert(codec->scene_graph);
	return gf_sg_find_node(codec->scene_graph, nodeID);
}


GF_BifsEncoder *gf_bifs_encoder_new(GF_SceneGraph *graph)
{
	GF_BifsEncoder * tmp;
	tmp = malloc(sizeof(GF_BifsEncoder));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_BifsEncoder));
	tmp->QPs = gf_list_new();
	tmp->streamInfo = gf_list_new();
	tmp->info = NULL;	
//	tmp->mx = gf_mx_new();
	tmp->encoded_nodes = gf_list_new();
	tmp->scene_graph = graph;
	return tmp;
}

static BIFSStreamInfo *BE_GetStream(GF_BifsEncoder * codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	while ((ptr = gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}

void gf_bifs_encoder_del(GF_BifsEncoder *codec)
{	
	if (codec->GlobalQP) gf_node_unregister((GF_Node *) codec->GlobalQP, NULL);
	assert(gf_list_count(codec->QPs)==0);
	gf_list_del(codec->QPs);
	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		BIFSStreamInfo *p = gf_list_get(codec->streamInfo, 0);
		bifs_info_del(p);
		gf_list_rem(codec->streamInfo, 0);
	}
	gf_list_del(codec->streamInfo);
	gf_list_del(codec->encoded_nodes);
//	gf_mx_del(codec->mx);
	free(codec);
}

GF_Err gf_bifs_encoder_new_stream(GF_BifsEncoder *codec, u16 ESID, GF_BIFSConfig *cfg, Bool encodeNames, Bool has_predictive)
{
	u32 i, count; 
	BIFSStreamInfo *pInfo;
	
//	gf_mx_p(codec->mx);
	if (BE_GetStream(codec, ESID) != NULL) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}
	
	pInfo = malloc(sizeof(BIFSStreamInfo));
	memset(pInfo, 0, sizeof(BIFSStreamInfo));
	pInfo->ESID = ESID;
	pInfo->UseName = encodeNames;
	pInfo->config.Height = cfg->pixelHeight;
	pInfo->config.Width = cfg->pixelWidth;
	pInfo->config.NodeIDBits = cfg->nodeIDbits;
	pInfo->config.RouteIDBits = cfg->routeIDbits;
	pInfo->config.ProtoIDBits = cfg->protoIDbits;
	pInfo->config.PixelMetrics = cfg->pixelMetrics;
	pInfo->config.version = (has_predictive || cfg->protoIDbits) ? 2 : 1;
	pInfo->config.UsePredictiveMFField = has_predictive;

	if (cfg->elementaryMasks) {
		pInfo->config.elementaryMasks = gf_list_new();
		count = gf_list_count(cfg->elementaryMasks);
		for (i=0; i<count; i++) {
			BIFSElementaryMask *bem;
			GF_ElementaryMask *em = gf_list_get(cfg->elementaryMasks, i);
			GF_SAFEALLOC(bem, sizeof(BIFSElementaryMask));
			if (em->node_id) bem->node = gf_sg_find_node(codec->scene_graph, em->node_id);
			else if (em->node_name) bem->node = gf_sg_find_node_by_name(codec->scene_graph, em->node_name);
			bem->node_id = em->node_id;
			gf_list_add(pInfo->config.elementaryMasks, bem);
		}
	}
	
	gf_list_add(codec->streamInfo, pInfo);
//	gf_mx_v(codec->mx);
	return GF_OK;
}

GF_Err gf_bifs_encode_au(GF_BifsEncoder *codec, u16 ESID, GF_List *command_list, char **out_data, u32 *out_data_length)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!codec || !command_list || !out_data || !out_data_length) return GF_BAD_PARAM;

//	gf_mx_p(codec->mx);
	codec->info = BE_GetStream(codec, ESID);
	if (!codec->info) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (codec->info->config.elementaryMasks) {
		e = GF_NOT_SUPPORTED;
	} else {
		e = gf_bifs_enc_commands(codec, command_list, bs);
	}
	gf_bs_align(bs);
	gf_bs_get_content(bs, (unsigned char **) out_data, out_data_length);
	gf_bs_del(bs);
//	gf_mx_v(codec->mx);
	return e;
}

GF_Err gf_bifs_encoder_get_config(GF_BifsEncoder *codec, u16 ESID, char **out_data, u32 *out_data_length)
{
	GF_BitStream *bs;

	if (!codec || !out_data || !out_data_length) return GF_BAD_PARAM;

//	gf_mx_p(codec->mx);
	codec->info = BE_GetStream(codec, ESID);
	if (!codec->info) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (codec->info->config.version==2) {
		gf_bs_write_int(bs, codec->info->config.Use3DMeshCoding ? 1 : 0, 1);
		gf_bs_write_int(bs, codec->info->config.UsePredictiveMFField ? 1 : 0, 1);
	}
	gf_bs_write_int(bs, codec->info->config.NodeIDBits, 5);
	gf_bs_write_int(bs, codec->info->config.RouteIDBits, 5);
	if (codec->info->config.version==2) {
		gf_bs_write_int(bs, codec->info->config.ProtoIDBits, 5);
	}
	if (codec->info->config.elementaryMasks) {
		u32 i, count;
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, codec->info->config.BAnimRAP, 1);
		count = gf_list_count(codec->info->config.elementaryMasks);
		for (i=0; i<count; i++) {
			BIFSElementaryMask *em = gf_list_get(codec->info->config.elementaryMasks, i);
			if (em->node) gf_bs_write_int(bs, gf_node_get_id(em->node), codec->info->config.NodeIDBits);
			else  gf_bs_write_int(bs, em->node_id, codec->info->config.NodeIDBits);
			gf_bs_write_int(bs, (i+1==count) ? 0 : 1, 1);
		}
	} else {
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, codec->info->config.PixelMetrics ? 1 : 0, 1);
		if (codec->info->config.Width || codec->info->config.Height) {
			gf_bs_write_int(bs, 1, 1);
			gf_bs_write_int(bs, codec->info->config.Width, 16);
			gf_bs_write_int(bs, codec->info->config.Height, 16);
		} else {
			gf_bs_write_int(bs, 0, 1);
		}
	}
	
	gf_bs_align(bs);
	gf_bs_get_content(bs, (unsigned char **) out_data, out_data_length);
	gf_bs_del(bs);
//	gf_mx_v(codec->mx);
	return GF_OK;
}

u8 gf_bifs_encoder_get_version(GF_BifsEncoder *codec, u16 ESID)
{
	u8 ret = 0;
//	gf_mx_p(codec->mx);
	codec->info = BE_GetStream(codec, ESID);
	if (codec->info) ret = codec->info->config.version;
//	gf_mx_v(codec->mx);
	return ret;
}


void gf_bifs_enc_log_bits(GF_BifsEncoder *codec, s32 val, u32 nbBits, char *str, char *com)
{
	if (!codec->trace) return;
	fprintf(codec->trace, "%s\t\t%d\t\t%d", str, nbBits, val);
	if (com) fprintf(codec->trace, "\t\t//%s", com);
	fprintf(codec->trace, "\n");
}

void BE_LogFloat(GF_BifsEncoder *codec, Fixed val, u32 nbBits, char *str, char *com)
{
	if (!codec->trace) return;
	fprintf(codec->trace, "%s\t\t%d\t\t%g", str, nbBits, FIX2FLT(val));
	if (com) fprintf(codec->trace, "\t\t//%s", com);
	fprintf(codec->trace, "\n");
}

void gf_bifs_encoder_set_trace(GF_BifsEncoder *codec, FILE *trace)
{
	codec->trace = trace;
	if (trace) fprintf(codec->trace, "Name\t\tNbBits\t\tValue\t\t//comment\n\n");
}
