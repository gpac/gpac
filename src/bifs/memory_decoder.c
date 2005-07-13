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
/*for scene replace tricks*/
#include <gpac/internal/scenegraph_dev.h>
#include "quant.h"

GF_Err ParseMFFieldList(GF_BifsDecoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
GF_Err ParseMFFieldVec(GF_BifsDecoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);


static void BM_SetCommandNode(GF_Command *com, GF_Node *node)
{
	com->node = node;
	gf_node_register(node, NULL);
}

static GF_Err BM_ParseMultipleIndexedReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 ID, ind, field_ind, NumBits, lenpos, lennum, count;
	GF_Node *node;
	GF_Err e;
	GF_Command *com;
	GF_CommandField *inf;
	GF_FieldInfo field;
	
	ID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_sg_find_node(codec->current_graph, ID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;
	e = gf_node_get_field(node, field_ind, &field);
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

	lenpos = gf_bs_read_int(bs, 5);
	lennum = gf_bs_read_int(bs, 5);
	count = gf_bs_read_int(bs, lennum);
	
	com = gf_sg_command_new(codec->current_graph, GF_SG_MULTIPLE_INDEXED_REPLACE);
	BM_SetCommandNode(com, node);
	field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);

	while (count) {
		inf = gf_sg_command_field_new(com);
		inf->pos = gf_bs_read_int(bs, lenpos);
		inf->fieldIndex = field.fieldIndex;
		inf->fieldType = field.fieldType;
		
		if (field.fieldType==GF_SG_VRML_SFNODE) {
			inf->new_node = gf_bifs_dec_node(codec, bs, field.NDTtype);
			if (codec->LastError) goto err;
			inf->field_ptr = &inf->new_node;
			gf_node_register(inf->new_node, node);
		} else {
			field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			e = gf_bifs_dec_sf_field(codec, bs, node, &field);
			if (e) goto err;
		}
		count--;
	}
err:
	if (e) gf_sg_command_del(com);
	else gf_list_add(com_list, com);
	return e;
}

static GF_Err BM_ParseMultipleReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 i, numFields, index, flag, nbBits, field_ref, fieldind;
	GF_Err e;
	GF_FieldInfo field;
	u32 NodeID;
	GF_Node *node;
	GF_Command *com;
	GF_CommandField *inf;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_bifs_dec_find_node(codec, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	
	e = GF_OK;
	com = gf_sg_command_new(codec->current_graph, GF_SG_MULTIPLE_REPLACE);
	BM_SetCommandNode(com, node);
	flag = gf_bs_read_int(bs, 1);
	if (flag) {
		numFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF);
		for (i=0; i<numFields; i++) {
			flag = gf_bs_read_int(bs, 1);
			if (!flag) continue;
			gf_bifs_get_field_index(node, i, GF_SG_FIELD_CODING_DEF, &index);
			e = gf_node_get_field(node, index, &field);
			if (e) goto exit;
			inf = gf_sg_command_field_new(com);
			inf->fieldType = field.fieldType;
			inf->fieldIndex = field.fieldIndex;
			if (inf->fieldType==GF_SG_VRML_SFNODE) {
				field.far_ptr = inf->field_ptr = &inf->new_node;
			} else if (inf->fieldType==GF_SG_VRML_MFNODE) {
				inf->node_list = gf_list_new();
				field.far_ptr = inf->field_ptr = &inf->node_list;
			} else {
				field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			}
			e = gf_bifs_dec_field(codec, bs, node, &field);
			if (e) goto exit;
		}
	} else {
		flag = gf_bs_read_int(bs, 1);
		nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF)-1);
		while (!flag) {
			field_ref = gf_bs_read_int(bs, nbBits);
			e = gf_bifs_get_field_index(node, field_ref, GF_SG_FIELD_CODING_DEF, &fieldind);
			if (e) goto exit;
			e = gf_node_get_field(node, fieldind, &field);
			if (e) goto exit;
			inf = gf_sg_command_field_new(com);
			inf->fieldType = field.fieldType;
			inf->fieldIndex = field.fieldIndex;
			if (inf->fieldType==GF_SG_VRML_SFNODE) {
				field.far_ptr = inf->field_ptr = &inf->new_node;
			} else if (inf->fieldType==GF_SG_VRML_MFNODE) {
				inf->node_list = gf_list_new();
				field.far_ptr = inf->field_ptr = &inf->node_list;
			} else {
				field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			}
			e = gf_bifs_dec_field(codec, bs, node, &field);
			if (e) goto exit;
			flag = gf_bs_read_int(bs, 1);
		}
	}

	
exit:
	if (e) gf_sg_command_del(com);
	else gf_list_add(com_list, com);
	return e;
}

static GF_Err BM_ParseGlobalQuantizer(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Node *node;
	GF_Command *com;
	GF_CommandField *inf;
	node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);

	/*reset global QP*/
	if (codec->GlobalQP) gf_node_unregister((GF_Node *) codec->GlobalQP, NULL);
	codec->GlobalQP = codec->ActiveQP = NULL;
	
	if (node && (gf_node_get_tag(node) != TAG_MPEG4_QuantizationParameter)) {
		gf_node_unregister(node, NULL);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*register global QP*/
	codec->GlobalQP = codec->ActiveQP = (M_QuantizationParameter *) node;
	codec->GlobalQP->isLocal = 0;
	if (node) gf_node_register(node, NULL);
	com = gf_sg_command_new(codec->current_graph, GF_SG_GLOBAL_QUANTIZER);
	inf = gf_sg_command_field_new(com);
	inf->new_node = node;
	inf->field_ptr = &inf->new_node;
	inf->fieldType = GF_SG_VRML_SFNODE;
	gf_list_add(com_list, com);
	return GF_OK;
}

static GF_Err BM_ParseProtoDelete(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 flag, count;
	GF_Command *com = gf_sg_command_new(codec->current_graph, GF_SG_PROTO_DELETE);
	flag = gf_bs_read_int(bs, 1);
	if (flag) {
		count = 0;
		flag = gf_bs_read_int(bs, 1);
		while (flag) {
			com->del_proto_list = realloc(com->del_proto_list, sizeof(u32) * (com->del_proto_list_size+1));
			com->del_proto_list[count] = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			com->del_proto_list_size++;
			flag = gf_bs_read_int(bs, 1);
		}
	} else {
		flag = gf_bs_read_int(bs, 5);
		com->del_proto_list_size = gf_bs_read_int(bs, flag);
		com->del_proto_list = realloc(com->del_proto_list, sizeof(u32) * (com->del_proto_list_size));
		flag = 0;
		while (flag<com->del_proto_list_size) {
			com->del_proto_list[flag] = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			flag++;
		}
	}
	gf_list_add(com_list, com);
	return GF_OK;
}

static GF_Err BM_ParseExtendedUpdates(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 type = gf_bs_read_int(bs, 8);
	GF_Err e;

	switch (type) {
	case 0:
	{
		GF_Command *com = gf_sg_command_new(codec->current_graph, GF_SG_PROTO_INSERT);
		e = gf_bifs_dec_proto_list(codec, bs, com->new_proto_list);
		if (e) gf_sg_command_del(com);
		else gf_list_add(com_list, com);
	}
		return e;
	case 1:
		return BM_ParseProtoDelete(codec, bs, com_list);
	case 2:
	{
		GF_Command *com = gf_sg_command_new(codec->current_graph, GF_SG_PROTO_DELETE_ALL);
		return gf_list_add(com_list, com);
	}
	case 3:
		return BM_ParseMultipleIndexedReplace(codec, bs, com_list);
	case 4:
		return BM_ParseMultipleReplace(codec, bs, com_list);
	case 5:
		return BM_ParseGlobalQuantizer(codec, bs, com_list);
	case 6:
	{
		GF_Command *com;
		u32 ID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		GF_Node *n = gf_bifs_dec_find_node(codec, ID);
		if (!n) return GF_OK;
		com = gf_sg_command_new(codec->current_graph, GF_SG_NODE_DELETE_EX);
		BM_SetCommandNode(com, n);
		gf_list_add(com_list, com);
	}
		return GF_OK;
	default:
		return GF_BIFS_UNKNOWN_VERSION;
	}
}

/*inserts a node in a container (node.children)*/
GF_Err BM_ParseNodeInsert(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 NodeID, NDT;
	GF_Command *com;
	GF_CommandField *inf;
	s32 type, pos;
	GF_Node *node, *def;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	def = gf_bifs_dec_find_node(codec, NodeID);
	if (!def) return GF_NON_COMPLIANT_BITSTREAM;
	NDT = gf_bifs_get_child_table(def);
	if (!NDT) return GF_NON_COMPLIANT_BITSTREAM;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		pos = gf_bs_read_int(bs, 8);
		break;
	case 2:
		pos = 0;
		break;
	case 3:
		/*-1 means append*/
		pos = -1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	node = gf_bifs_dec_node(codec, bs, NDT);
	if (!codec->LastError) {
		com = gf_sg_command_new(codec->current_graph, GF_SG_NODE_INSERT);
		BM_SetCommandNode(com, def);
		inf = gf_sg_command_field_new(com);
		inf->pos = pos;
		inf->new_node = node;
		inf->field_ptr = &inf->new_node;
		inf->fieldType = GF_SG_VRML_SFNODE;
		gf_list_add(com_list, com);
		/*register*/
		gf_node_register(node, def);
	}
	return codec->LastError;
}

/*NB This can insert a node as well (but usually not in the .children field)*/
GF_Err BM_ParseIndexInsert(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Err e;
	u32 NodeID;
	u32 NumBits, ind, field_ind;
	u8 type;
	GF_Command *com;
	GF_CommandField *inf;
	s32 pos;
	GF_Node *def, *node;
	GF_FieldInfo field, sffield;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	def = gf_bifs_dec_find_node(codec, NodeID);
	if (!def) return GF_NON_COMPLIANT_BITSTREAM;
	/*index insertion uses IN mode for field index*/
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(def, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);

	e = gf_bifs_get_field_index(def, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		pos = gf_bs_read_int(bs, 16);
		break;
	case 2:
		pos = 0;
		break;
	case 3:
		pos = -1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	e = gf_node_get_field(def, field_ind, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

	memcpy(&sffield, &field, sizeof(GF_FieldInfo));
	sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);

	/*rescale the MFField and parse the SFField*/
	if (field.fieldType==GF_SG_VRML_MFNODE) {
		node = gf_bifs_dec_node(codec, bs, field.NDTtype);
		if (!codec->LastError) {
			com = gf_sg_command_new(codec->current_graph, GF_SG_INDEXED_INSERT);
			BM_SetCommandNode(com, def);
			inf = gf_sg_command_field_new(com);
			inf->pos = pos;
			inf->fieldIndex = field_ind;
			inf->fieldType = sffield.fieldType;
			inf->new_node = node;
			inf->field_ptr = &inf->new_node;
			gf_list_add(com_list, com);
			/*register*/
			gf_node_register(node, def);
		}
	} else {
		com = gf_sg_command_new(codec->current_graph, GF_SG_INDEXED_INSERT);
		BM_SetCommandNode(com, def);
		inf = gf_sg_command_field_new(com);
		inf->pos = pos;
		inf->fieldIndex = field_ind;
		inf->fieldType = sffield.fieldType;
		sffield.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(sffield.fieldType);
		codec->LastError = gf_bifs_dec_sf_field(codec, bs, def, &sffield);
		gf_list_add(com_list, com);
	}
	return codec->LastError;
}


GF_Err BM_ParseRouteInsert(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Err e;
	u8 flag;
	GF_Command *com;
	GF_Node *InNode, *OutNode;
	u32 RouteID, outField, inField, numBits, ind, node_id;
	char name[1000];

	RouteID = 0;

	flag = gf_bs_read_int(bs, 1);
	/*def'ed route*/
	if (flag) {
		RouteID = 1 + gf_bs_read_int(bs, codec->info->config.RouteIDBits);
		if (codec->info->UseName) gf_bifs_dec_name(bs, name);
	}
	/*origin*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	OutNode = gf_bifs_dec_find_node(codec, node_id);
	if (!OutNode) return GF_SG_UNKNOWN_NODE;

	numBits = gf_node_get_num_fields_in_mode(OutNode, GF_SG_FIELD_CODING_OUT) - 1;
	numBits = gf_get_bit_size(numBits);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(OutNode, ind, GF_SG_FIELD_CODING_OUT, &outField);

	/*target*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	InNode = gf_bifs_dec_find_node(codec, node_id);
	if (!InNode) return GF_SG_UNKNOWN_NODE;

	numBits = gf_node_get_num_fields_in_mode(InNode, GF_SG_FIELD_CODING_IN) - 1;
	numBits = gf_get_bit_size(numBits);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(InNode, ind, GF_SG_FIELD_CODING_IN, &inField);
	if (e) return e;

	com = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_INSERT);
	com->RouteID = RouteID;
	if (codec->info->UseName) com->def_name = strdup( name);
	com->fromNodeID = gf_node_get_id(OutNode);
	com->fromFieldIndex = outField;
	com->toNodeID = gf_node_get_id(InNode);
	com->toFieldIndex = inField;
	gf_list_add(com_list, com);
	return codec->LastError;
}


GF_Err BM_ParseInsert(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u8 type;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		return BM_ParseNodeInsert(codec, bs, com_list);
	case 1:
		return BM_ParseExtendedUpdates(codec, bs, com_list);
	case 2:
		return BM_ParseIndexInsert(codec, bs, com_list);
	case 3:
		return BM_ParseRouteInsert(codec, bs, com_list);
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
}


GF_Err BM_ParseIndexDelete(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 NodeID, NumBits, ind, field_ind;
	s32 pos;
	GF_Command *com;
	u8 type;
	GF_Node *node;
	GF_Err e;
	GF_CommandField *inf;
	GF_FieldInfo field;
	
	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_bifs_dec_find_node(codec, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN) - 1);
	ind = gf_bs_read_int(bs, NumBits);

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		pos = (u32) gf_bs_read_int(bs, 16);
		break;
	case 2:
		pos = 0;
		break;
	case 3:
		pos = -1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;
	e = gf_node_get_field(node, field_ind, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;
	com = gf_sg_command_new(codec->current_graph, GF_SG_INDEXED_DELETE);
	BM_SetCommandNode(com, node);
	inf = gf_sg_command_field_new(com);
	inf->pos = pos;
	inf->fieldIndex = field.fieldIndex;
	inf->fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
	gf_list_add(com_list, com);
	return codec->LastError;
}



GF_Err BM_ParseDelete(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u8 type;
	u32 ID;
	GF_Command *com;
	GF_Node *n;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		ID = 1+gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		n = gf_bifs_dec_find_node(codec, ID);
		if (!n) return GF_OK;
		com = gf_sg_command_new(codec->current_graph, GF_SG_NODE_DELETE);
		BM_SetCommandNode(com, n);
		gf_list_add(com_list, com);
		return GF_OK;
	case 2:
		return BM_ParseIndexDelete(codec, bs, com_list);
	case 3:
		com = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_DELETE);
		com->RouteID = 1+gf_bs_read_int(bs, codec->info->config.RouteIDBits);
		gf_list_add(com_list, com);
		return GF_OK;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}


GF_Err BM_ParseNodeReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 NodeID;
	GF_Command *com;
	GF_Node *node;
	GF_CommandField *inf;
	
	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	/*this is delete / new on a DEF node: replace ALL instances*/
	node = gf_bifs_dec_find_node(codec, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	com = gf_sg_command_new(codec->current_graph, GF_SG_NODE_REPLACE);
	BM_SetCommandNode(com, node);
	inf = gf_sg_command_field_new(com);
	inf->new_node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);
	inf->fieldType = GF_SG_VRML_SFNODE;
	inf->field_ptr = &inf->new_node;
	gf_list_add(com_list, com);
	gf_node_register(inf->new_node, NULL);
	return codec->LastError;
}

GF_Err BM_ParseFieldReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Err e;
	GF_Command *com;
	u32 NodeID, ind, field_ind, NumBits;
	GF_Node *node;
	GF_FieldInfo field;
	GF_CommandField *inf;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_bifs_dec_find_node(codec, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	e = gf_node_get_field(node, field_ind, &field);

	com = gf_sg_command_new(codec->current_graph, GF_SG_FIELD_REPLACE);
	BM_SetCommandNode(com, node);
	inf = gf_sg_command_field_new(com);
	inf->fieldIndex = field_ind;
	inf->fieldType = field.fieldType;
	if (inf->fieldType == GF_SG_VRML_SFNODE) {
		field.far_ptr = inf->field_ptr = &inf->new_node;
	} else if (inf->fieldType == GF_SG_VRML_MFNODE) {
		inf->node_list = gf_list_new();
		field.far_ptr = inf->field_ptr = &inf->node_list;
	} else {
		field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(field.fieldType);
	}
	/*parse the field*/
	codec->LastError = gf_bifs_dec_field(codec, bs, node, &field);

	/*register nodes*/
	if (inf->fieldType == GF_SG_VRML_SFNODE) {
		gf_node_register(inf->new_node, com->node);
	} else if (inf->fieldType == GF_SG_VRML_MFNODE) {
		u32 i;
		for (i=0; i<gf_list_count(inf->node_list); i++) {
			GF_Node *p = gf_list_get(inf->node_list, i);
			gf_node_register(p, com->node);
		}
	}

	gf_list_add(com_list, com);
	return codec->LastError;
}

GF_Err BM_ParseIndexValueReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u32 NodeID, ind, field_ind, NumBits;
	s32 type, pos;
	GF_Command *com;
	GF_Node *node;
	GF_Err e;
	GF_FieldInfo field, sffield;
	GF_CommandField *inf;

	/*get the node*/
	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);

	node = gf_bifs_dec_find_node(codec, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	e = gf_node_get_field(node, field_ind, &field);
	if (gf_sg_vrml_is_sf_field(field.fieldType)) return GF_NON_COMPLIANT_BITSTREAM;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		pos = gf_bs_read_int(bs, 16);
		break;
	case 2:
		pos = 0;
		break;
	case 3:
		pos = ((GenMFField *) field.far_ptr)->count - 1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	
	com = gf_sg_command_new(codec->current_graph, GF_SG_INDEXED_REPLACE);
	BM_SetCommandNode(com, node);
	inf = gf_sg_command_field_new(com);
	inf->fieldIndex = field.fieldIndex;
	inf->pos = pos;

	if (field.fieldType == GF_SG_VRML_MFNODE) {
		inf->fieldType = GF_SG_VRML_SFNODE;
		inf->new_node = gf_bifs_dec_node(codec, bs, field.NDTtype);
		inf->field_ptr = &inf->new_node;
		gf_node_register(inf->new_node, com->node);
	} else {
		memcpy(&sffield, &field, sizeof(GF_FieldInfo));
		sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		inf->fieldType = sffield.fieldType;
		sffield.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(sffield.fieldType);
		codec->LastError = gf_bifs_dec_sf_field(codec, bs, node, &sffield);
	}
	gf_list_add(com_list, com);
	return codec->LastError;
}

u32 BM_ParseRouteReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Err e;
	GF_Command *com;
	u32 RouteID, numBits, ind, node_id, fromID, toID;
	GF_Route *r;
	GF_Node *OutNode, *InNode;

	RouteID = 1+gf_bs_read_int(bs, codec->info->config.RouteIDBits);
	
	r = gf_sg_route_find(codec->current_graph, RouteID);

	/*origin*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	OutNode = gf_bifs_dec_find_node(codec, node_id);
	if (!OutNode) return GF_NON_COMPLIANT_BITSTREAM;
	numBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(OutNode, GF_SG_FIELD_CODING_OUT) - 1);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(OutNode, ind, GF_SG_FIELD_CODING_OUT, &fromID);
	if (e) return e;

	/*target*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	InNode = gf_bifs_dec_find_node(codec, node_id);
	if (!InNode) return GF_NON_COMPLIANT_BITSTREAM;
	numBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(InNode, GF_SG_FIELD_CODING_IN) - 1);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(InNode, ind, GF_SG_FIELD_CODING_IN, &toID);
	if (e) return e;

	com = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_REPLACE);
	com->RouteID = RouteID;
	com->fromNodeID = gf_node_get_id(OutNode);
	com->fromFieldIndex = fromID;
	com->toNodeID = gf_node_get_id(InNode);
	com->toFieldIndex = toID;
	gf_list_add(com_list, com);
	return codec->LastError;
}


GF_Err BM_ParseReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u8 type;
	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		return BM_ParseNodeReplace(codec, bs, com_list);
	case 1:
		return BM_ParseFieldReplace(codec, bs, com_list);
	case 2:
		return BM_ParseIndexValueReplace(codec, bs, com_list);
	case 3:
		return BM_ParseRouteReplace(codec, bs, com_list);
	}
	return GF_OK;
}

GF_Err BM_SceneReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Command *com;
	GF_Node *backup_root;
	GF_List *backup_routes;
	GF_Err BD_DecSceneReplace(GF_BifsDecoder * codec, GF_BitStream *bs, GF_List *proto_list);

	backup_routes = codec->scenegraph->Routes;
	backup_root = codec->scenegraph->RootNode;
	com = gf_sg_command_new(codec->current_graph, GF_SG_SCENE_REPLACE);
	codec->scenegraph->Routes = gf_list_new();
	codec->current_graph = codec->scenegraph;
	codec->LastError = BD_DecSceneReplace(codec, bs, com->new_proto_list);

	/*restore*/
	com->node = codec->scenegraph->RootNode;
	codec->scenegraph->RootNode = backup_root;
	gf_list_add(com_list, com);
	/*insert routes*/
	while (gf_list_count(codec->scenegraph->Routes)) {
		GF_Route *r = gf_list_get(codec->scenegraph->Routes, 0);
		GF_Command *ri = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_INSERT);
		gf_list_rem(codec->scenegraph->Routes, 0);
		ri->fromFieldIndex = r->FromFieldIndex;
		ri->fromNodeID = r->FromNode->sgprivate->NodeID;
		ri->toFieldIndex = r->ToFieldIndex;
		ri->toNodeID = r->ToNode->sgprivate->NodeID;
		if (r->ID) ri->RouteID = r->ID;
		ri->def_name = r->name ? strdup(r->name) : NULL;
		gf_list_add(com_list, ri);
		gf_sg_route_del(r);
	}
	gf_list_del(codec->scenegraph->Routes);
	codec->scenegraph->Routes = backup_routes;
	return codec->LastError;
}


GF_Err BM_ParseCommand(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	u8 go, type;
	u32 count;
	GF_Err e;
	go = 1;
	e = GF_OK;

	codec->LastError = 0;
	count = 0;

	while (go) {
		type = gf_bs_read_int(bs, 2);
		switch (type) {
		case 0:
			e = BM_ParseInsert(codec, bs, com_list);
			break;
		case 1:
			e = BM_ParseDelete(codec, bs, com_list);
			break;
		case 2:
			e = BM_ParseReplace(codec, bs, com_list);
			break;
		case 3:
			e = BM_SceneReplace(codec, bs, com_list);
			break;
		}
		if (e) return e;

		go = gf_bs_read_int(bs, 1);
		count++;

	}
	while (gf_list_count(codec->QPs)) {
		gf_bifs_dec_qp_remove(codec, 1);
	}
	return GF_OK;
}

void BM_EndOfStream(void *co)
{
	((GF_BifsDecoder *) co)->LastError = GF_IO_ERR;
}

void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);

GF_Err gf_bifs_decode_command_list(GF_BifsDecoder *codec, u16 ESID, char *data, u32 data_length, GF_List *com_list)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!codec || !data || !codec->dec_memory_mode || !com_list) return GF_BAD_PARAM;

//	gf_mx_p(codec->mx);
	codec->info = gf_bifs_dec_get_stream(codec, ESID);
	if (!codec->info || !codec->info->config.IsCommandStream) {
//		gf_mx_v(codec->mx);
		return GF_BAD_PARAM;
	}
	/*root parse (not conditionals)*/
	assert(codec->scenegraph);
	/*setup current scene graph*/
	codec->current_graph = codec->scenegraph;

	bs = gf_bs_new(data, data_length, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(bs, BM_EndOfStream, codec);

	e = BM_ParseCommand(codec, bs, com_list);
	gf_bs_del(bs);

	/*decode conditionals / input sensors*/
	if (!e) {
		GF_Node *n;
		SFCommandBuffer *c_bfr;
		u32 NbPass = gf_list_count(codec->conditionals);
		GF_List *nextPass = gf_list_new();
		while (NbPass) {
			while (gf_list_count(codec->conditionals)) {
				n = gf_list_get(codec->conditionals, 0);
				gf_list_rem(codec->conditionals, 0);
				codec->current_graph = gf_node_get_graph((GF_Node *)n);
				c_bfr = NULL;
				switch (gf_node_get_tag(n)) {
				case TAG_MPEG4_Conditional: c_bfr = & ((M_Conditional *)n)->buffer; break;
				case TAG_MPEG4_InputSensor: c_bfr = & ((M_InputSensor *)n)->buffer; break;
				}
				assert(c_bfr);
				bs = gf_bs_new(c_bfr->buffer, c_bfr->bufferSize, GF_BITSTREAM_READ);
				gf_bs_set_eos_callback(bs, BM_EndOfStream, codec);

				e = BM_ParseCommand(codec, bs, c_bfr->commandList);
				gf_bs_del(bs);
				if (!e) continue;
				/*this may be an error or a dependency pb - reset coimmand list and move to next pass*/
				while (gf_list_count(c_bfr->commandList)) {
					GF_Command *com = gf_list_get(c_bfr->commandList, 0);
					gf_list_rem(c_bfr->commandList, 0);
					gf_sg_command_del(com);
				}
				gf_list_add(nextPass, n);
			}
			if (!gf_list_count(nextPass)) break;
			/*prepare next pass*/
			while (gf_list_count(nextPass)) {
				n = gf_list_get(nextPass, 0);
				gf_list_rem(nextPass, 0);
				gf_list_add(codec->conditionals, n);
			}
			NbPass --;
			if (NbPass > gf_list_count(codec->conditionals)) NbPass = gf_list_count(codec->conditionals);
		}
		gf_list_del(nextPass);
	}
	/*if err or not reset conditionals*/
	while (gf_list_count(codec->conditionals)) gf_list_rem(codec->conditionals, 0);

	/*reset current config*/
	codec->info = NULL;
	codec->current_graph = NULL;



//	gf_mx_v(codec->mx);
	return e;
}
