/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/nodes_x3d.h>

#ifndef GPAC_DISABLE_BIFS


static GF_Err ParseConfig(GF_BitStream *bs, BIFSStreamInfo *info, u32 version)
{
	Bool hasSize, cmd_stream;

	if (info->config.elementaryMasks) gf_list_del(info->config.elementaryMasks);
	info->config.elementaryMasks = NULL	;

	if (version==2) {
		info->config.Use3DMeshCoding = (Bool)gf_bs_read_int(bs, 1);
		info->config.UsePredictiveMFField = (Bool)gf_bs_read_int(bs, 1);
	}
	info->config.NodeIDBits = gf_bs_read_int(bs, 5);
	info->config.RouteIDBits = gf_bs_read_int(bs, 5);
	if (version==2) {
		info->config.ProtoIDBits = gf_bs_read_int(bs, 5);
	}
	cmd_stream = (Bool)gf_bs_read_int(bs, 1);

	if (cmd_stream) {
		info->config.PixelMetrics = (Bool)gf_bs_read_int(bs, 1);
		hasSize = (Bool)gf_bs_read_int(bs, 1);
		if (hasSize) {
			info->config.Width = gf_bs_read_int(bs, 16);
			info->config.Height = gf_bs_read_int(bs, 16);
		}
		gf_bs_align(bs);

		if (gf_bs_get_size(bs) != gf_bs_get_position(bs)) return GF_ODF_INVALID_DESCRIPTOR;
		return GF_OK;
	} else {
		info->config.BAnimRAP = (Bool)gf_bs_read_int(bs, 1);
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
		BIFSElementaryMask *em = (BIFSElementaryMask *)gf_list_last(info->config.elementaryMasks);
		if (!em) break;
		gf_list_rem_last(info->config.elementaryMasks);
		gf_free(em);
	}
	gf_free(info);
}

GF_EXPORT
GF_BifsDecoder *gf_bifs_decoder_new(GF_SceneGraph *scenegraph, Bool command_dec)
{
	GF_BifsDecoder *tmp;
	GF_SAFEALLOC(tmp, GF_BifsDecoder);
	if (!tmp) return NULL;
	
	tmp->QPs = gf_list_new();
	tmp->streamInfo = gf_list_new();
	tmp->info = NULL;

	tmp->pCurrentProto = NULL;
	tmp->scenegraph = scenegraph;
	tmp->command_buffers = gf_list_new();
	if (command_dec) {
		tmp->dec_memory_mode = GF_TRUE;
		tmp->force_keep_qp = GF_TRUE;
	}
	tmp->current_graph = NULL;
	return tmp;
}


BIFSStreamInfo *gf_bifs_dec_get_stream(GF_BifsDecoder * codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	if (!codec || !codec->streamInfo)
		return NULL;
	while ((ptr = (BIFSStreamInfo *) gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_bifs_decoder_configure_stream(GF_BifsDecoder * codec, u16 ESID, u8 *DecoderSpecificInfo, u32 DecoderSpecificInfoLength, u32 objectTypeIndication)
{
	GF_BitStream *bs;
	BIFSStreamInfo *pInfo;
	Bool new_cfg = GF_FALSE;
	GF_Err e;

	if (!DecoderSpecificInfo) {
		/* Hack for T-DMB non compliant streams */
		GF_SAFEALLOC(pInfo, BIFSStreamInfo);
		if (!pInfo) return GF_OUT_OF_MEM;
		pInfo->ESID = ESID;
		pInfo->config.PixelMetrics = GF_TRUE;
		pInfo->config.version = (objectTypeIndication==2) ? 1 : 2;
		assert( codec );
		assert( codec->streamInfo );
		return gf_list_add(codec->streamInfo, pInfo);
	}

	assert( codec );
	pInfo = gf_bifs_dec_get_stream(codec, ESID);
	//we allow reconfigure of the BIFS stream
	if (pInfo == NULL) {
		GF_SAFEALLOC(pInfo, BIFSStreamInfo);
		if (!pInfo) return GF_OUT_OF_MEM;
		new_cfg = GF_TRUE;
	}
	bs = gf_bs_new(DecoderSpecificInfo, DecoderSpecificInfoLength, GF_BITSTREAM_READ);
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
		gf_free(pInfo);
		gf_bs_del(bs);
		return GF_BIFS_UNKNOWN_VERSION;
	}
	gf_bs_del(bs);

	assert( codec->streamInfo );
	//first stream, configure size
	if (!codec->ignore_size && !gf_list_count(codec->streamInfo)) {
		gf_sg_set_scene_size_info(codec->scenegraph, pInfo->config.Width, pInfo->config.Height, pInfo->config.PixelMetrics);
	}

	if (new_cfg)
		gf_list_add(codec->streamInfo, pInfo);
	return GF_OK;
}



#if 0	//deprecated
GF_EXPORT
void gf_bifs_decoder_ignore_size_info(GF_BifsDecoder *codec)
{
	if (codec) codec->ignore_size = GF_TRUE;
}

GF_EXPORT
GF_Err gf_bifs_decoder_remove_stream(GF_BifsDecoder *codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	while ((ptr = (BIFSStreamInfo*)gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) {
			gf_free(ptr);
			gf_list_rem(codec->streamInfo, i-1);
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}
#endif


GF_EXPORT
void gf_bifs_decoder_del(GF_BifsDecoder *codec)
{
	assert(gf_list_count(codec->QPs)==0);
	gf_list_del(codec->QPs);

	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		BIFSStreamInfo *p = (BIFSStreamInfo*)gf_list_get(codec->streamInfo, 0);
		bifs_info_del(p);
		gf_list_rem(codec->streamInfo, 0);
	}
	gf_list_del(codec->streamInfo);

	while (gf_list_count(codec->command_buffers)) {
		CommandBufferItem *cbi = (CommandBufferItem *)gf_list_get(codec->command_buffers, 0);
		gf_free(cbi);
		gf_list_rem(codec->command_buffers, 0);
	}
	gf_list_del(codec->command_buffers);
	gf_free(codec);
}


void BD_EndOfStream(void *co)
{
	((GF_BifsDecoder *) co)->LastError = GF_IO_ERR;
}

void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

GF_EXPORT
GF_Err gf_bifs_decode_au(GF_BifsDecoder *codec, u16 ESID, const u8 *data, u32 data_length, Double ts_offset)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!codec || !data || codec->dec_memory_mode) return GF_BAD_PARAM;
	if (!data_length) return GF_OK;

	codec->info = gf_bifs_dec_get_stream(codec, ESID);
	if (!codec->info) {
		return GF_BAD_PARAM;
	}
	/*setup current scene graph*/
	codec->current_graph = codec->scenegraph;
	codec->cts_offset = ts_offset;

	bs = gf_bs_new((u8 *)data, data_length, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;
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
	return e;
}


#ifndef GPAC_DISABLE_BIFS_ENC

GF_Node *gf_bifs_enc_find_node(GF_BifsEncoder *codec, u32 nodeID)
{
	if (codec->current_proto_graph) return gf_sg_find_node(codec->current_proto_graph, nodeID);
	assert(codec->scene_graph);
	return gf_sg_find_node(codec->scene_graph, nodeID);
}


GF_EXPORT
GF_BifsEncoder *gf_bifs_encoder_new(GF_SceneGraph *graph)
{
	GF_BifsEncoder * tmp;
	GF_SAFEALLOC(tmp, GF_BifsEncoder);
	if (!tmp) return NULL;
	tmp->QPs = gf_list_new();
	tmp->streamInfo = gf_list_new();
	tmp->info = NULL;
	tmp->encoded_nodes = gf_list_new();
	tmp->scene_graph = graph;
	return tmp;
}

static BIFSStreamInfo *BE_GetStream(GF_BifsEncoder * codec, u16 ESID)
{
	u32 i;
	BIFSStreamInfo *ptr;

	i=0;
	while ((ptr = (BIFSStreamInfo*)gf_list_enum(codec->streamInfo, &i))) {
		if(ptr->ESID==ESID) return ptr;
	}
	return NULL;
}

GF_EXPORT
void gf_bifs_encoder_del(GF_BifsEncoder *codec)
{
	assert(gf_list_count(codec->QPs)==0);
	gf_list_del(codec->QPs);
	/*destroy all config*/
	while (gf_list_count(codec->streamInfo)) {
		BIFSStreamInfo *p = (BIFSStreamInfo*)gf_list_get(codec->streamInfo, 0);
		bifs_info_del(p);
		gf_list_rem(codec->streamInfo, 0);
	}
	gf_list_del(codec->streamInfo);
	gf_list_del(codec->encoded_nodes);
	if (codec->src_url) gf_free(codec->src_url);
//	gf_mx_del(codec->mx);
	gf_free(codec);
}

GF_EXPORT
GF_Err gf_bifs_encoder_new_stream(GF_BifsEncoder *codec, u16 ESID, GF_BIFSConfig *cfg, Bool encodeNames, Bool has_predictive)
{
	u32 i, count;
	BIFSStreamInfo *pInfo;

//	gf_mx_p(codec->mx);
	if (BE_GetStream(codec, ESID) != NULL) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}

	GF_SAFEALLOC(pInfo, BIFSStreamInfo);
	if (!pInfo) return GF_OUT_OF_MEM;
	pInfo->ESID = ESID;
	codec->UseName = encodeNames;
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
			GF_ElementaryMask *em = (GF_ElementaryMask *)gf_list_get(cfg->elementaryMasks, i);
			GF_SAFEALLOC(bem, BIFSElementaryMask);
			if (!bem) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Fail to allocate elementary mask"));
				continue;
			}
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

GF_EXPORT
GF_Err gf_bifs_encode_au(GF_BifsEncoder *codec, u16 ESID, GF_List *command_list, u8 **out_data, u32 *out_data_length)
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
	gf_bs_get_content(bs, out_data, out_data_length);
	gf_bs_del(bs);
//	gf_mx_v(codec->mx);
	return e;
}

GF_EXPORT
GF_Err gf_bifs_encoder_get_config(GF_BifsEncoder *codec, u16 ESID, u8 **out_data, u32 *out_data_length)
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
			BIFSElementaryMask *em = (BIFSElementaryMask *)gf_list_get(codec->info->config.elementaryMasks, i);
			if (em->node) gf_bs_write_int(bs, gf_node_get_id((GF_Node*)em->node), codec->info->config.NodeIDBits);
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
	gf_bs_get_content(bs, out_data, out_data_length);
	gf_bs_del(bs);
//	gf_mx_v(codec->mx);
	return GF_OK;
}

GF_EXPORT
u8 gf_bifs_encoder_get_version(GF_BifsEncoder *codec, u16 ESID)
{
	u8 ret = 0;
//	gf_mx_p(codec->mx);
	codec->info = BE_GetStream(codec, ESID);
	if (codec->info) ret = codec->info->config.version;
//	gf_mx_v(codec->mx);
	return ret;
}

GF_EXPORT
GF_Err gf_bifs_encoder_set_source_url(GF_BifsEncoder *codec, const char *src_url)
{
	if (!codec) return GF_BAD_PARAM;
	if (codec->src_url) gf_free(codec->src_url);
	codec->src_url = gf_strdup(src_url);
	return GF_OK;
}


#endif /*GPAC_DISABLE_BIFS_ENC*/

GF_EXPORT
u32 gf_bifs_get_child_table(GF_Node *Node)
{
	assert(Node);
	return gf_sg_mpeg4_node_get_child_ndt(Node);
}


GF_Err gf_bifs_get_field_index(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField)
{
	assert(Node);
	switch (Node->sgprivate->tag) {
	case TAG_ProtoNode:
		return gf_sg_proto_get_field_ind_static(Node, inField, IndexMode, allField);
	case TAG_MPEG4_Script:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Script:
#endif
		return gf_sg_script_get_field_index(Node, inField, IndexMode, allField);
	default:
		return gf_sg_mpeg4_node_get_field_index(Node, inField, IndexMode, allField);
	}
}


/* QUANTIZATION AND BIFS_Anim Info */
GF_EXPORT
Bool gf_bifs_get_aq_info(GF_Node *Node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits)
{
	switch (Node->sgprivate->tag) {
	case TAG_ProtoNode:
		return gf_sg_proto_get_aq_info(Node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	default:
		return gf_sg_mpeg4_node_get_aq_info(Node, FieldIndex, QType, AType, b_min, b_max, QT13_bits);
	}
}

#endif /*GPAC_DISABLE_BIFS*/

