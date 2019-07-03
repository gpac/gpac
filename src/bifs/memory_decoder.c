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
#include "quant.h"


#ifndef GPAC_DISABLE_BIFS

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
			gf_node_register(inf->new_node, NULL);
		} else {
			field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			e = gf_bifs_dec_sf_field(codec, bs, node, &field, GF_TRUE);
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
	node = gf_sg_find_node(codec->current_graph, NodeID);
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
				field.far_ptr = inf->field_ptr = &inf->node_list;
			} else {
				field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			}
			e = gf_bifs_dec_field(codec, bs, node, &field, GF_TRUE);
			if (e) goto exit;
		}
	} else {
		flag = gf_bs_read_int(bs, 1);
		nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF)-1);
		while (!flag && (codec->LastError>=0)) {
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
				field.far_ptr = inf->field_ptr = &inf->node_list;
			} else {
				field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
			}
			e = gf_bifs_dec_field(codec, bs, node, &field, GF_TRUE);
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
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	/*reset global QP*/
	if (codec->scenegraph->global_qp) {
		gf_node_unregister(codec->scenegraph->global_qp, NULL);
	}
	codec->ActiveQP = NULL;
	codec->scenegraph->global_qp = NULL;

	if (node && (gf_node_get_tag(node) != TAG_MPEG4_QuantizationParameter)) {
		gf_node_unregister(node, NULL);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*register global QP*/
	codec->ActiveQP = (M_QuantizationParameter *) node;
	codec->ActiveQP->isLocal = 0;
	codec->scenegraph->global_qp = node;
	if (node) {
		/*register TWICE: once for the command, and for the scenegraph globalQP*/
		node->sgprivate->num_instances = 2;
	}
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
			com->del_proto_list = (u32*)gf_realloc(com->del_proto_list, sizeof(u32) * (com->del_proto_list_size+1));
			com->del_proto_list[count] = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			com->del_proto_list_size++;
			flag = gf_bs_read_int(bs, 1);
		}
	} else {
		flag = gf_bs_read_int(bs, 5);
		com->del_proto_list_size = gf_bs_read_int(bs, flag);
		com->del_proto_list = (u32*)gf_realloc(com->del_proto_list, sizeof(u32) * (com->del_proto_list_size));
		flag = 0;
		while (flag<com->del_proto_list_size) {
			com->del_proto_list[flag] = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			flag++;
		}
	}
	gf_list_add(com_list, com);
	return GF_OK;
}

static GF_Err BM_XReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_FieldInfo targetField, fromField, decfield;
	GF_Node *target, *n, *fromNode;
	s32 pos = -2;
	u32 id, nbBits, ind, aind;
	GF_Err e;
	GF_Command *com;
	GF_CommandField *inf;

	id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	target = gf_sg_find_node(codec->current_graph, id);
	if (!target) return GF_SG_UNKNOWN_NODE;

	com = gf_sg_command_new(codec->current_graph, GF_SG_XREPLACE);
	BM_SetCommandNode(com, target);
	gf_list_add(com_list, com);

	nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(target, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, nbBits);
	e = gf_bifs_get_field_index(target, ind, GF_SG_FIELD_CODING_IN, &aind);
	if (e) return e;
	e = gf_node_get_field(target, aind, &targetField);
	if (e) return e;

	inf = gf_sg_command_field_new(com);
	inf->fieldIndex = aind;

	if (!gf_sg_vrml_is_sf_field(targetField.fieldType)) {
		/*this is indexed replacement*/
		if (gf_bs_read_int(bs, 1)) {
			/*index is dynamic*/
			if (gf_bs_read_int(bs, 1)) {
				id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
				n = gf_sg_find_node(codec->current_graph, id);
				if (!n) return GF_SG_UNKNOWN_NODE;
				com->toNodeID = id;

				nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_DEF)-1);
				ind = gf_bs_read_int(bs, nbBits);
				e = gf_bifs_get_field_index(n, ind, GF_SG_FIELD_CODING_DEF, &aind);
				if (e) return e;
				e = gf_node_get_field(n, aind, &fromField);
				if (e) return e;
				com->toFieldIndex = aind;
			} else {
				u32 type = gf_bs_read_int(bs, 2);
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
				}
			}
		}
		if (targetField.fieldType==GF_SG_VRML_MFNODE) {
			if (gf_bs_read_int(bs, 1)) {
				target = gf_node_list_get_child(*(GF_ChildNodeItem **)targetField.far_ptr, pos);
				if (!target) return GF_SG_UNKNOWN_NODE;

				nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(target, GF_SG_FIELD_CODING_IN)-1);
				ind = gf_bs_read_int(bs, nbBits);
				e = gf_bifs_get_field_index(target, ind, GF_SG_FIELD_CODING_IN, &aind);
				if (e) return e;
				e = gf_node_get_field(target, aind, &targetField);
				if (e) return e;
				pos = -2;
				com->child_field = aind;
				com->ChildNodeTag = gf_node_get_tag(target);
				if (com->ChildNodeTag == TAG_ProtoNode) {
					s32 p_id = gf_sg_proto_get_id(gf_node_get_proto(target));
					com->ChildNodeTag = -p_id;
				}
			}
		}
		inf->pos = pos;
	}

	fromNode = NULL;
	if (gf_bs_read_int(bs, 1)) {
		id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		fromNode = gf_sg_find_node(codec->current_graph, id);
		if (!fromNode) return GF_SG_UNKNOWN_NODE;
		com->fromNodeID = id;

		nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(fromNode, GF_SG_FIELD_CODING_DEF)-1);
		ind = gf_bs_read_int(bs, nbBits);
		e = gf_bifs_get_field_index(fromNode, ind, GF_SG_FIELD_CODING_DEF, &aind);
		if (e) return e;
		e = gf_node_get_field(fromNode, aind, &fromField);
		if (e) return e;
		com->fromFieldIndex = aind;

		return GF_OK;
	}


	if (pos>= -1) {
		inf->fieldType = gf_sg_vrml_get_sf_type(targetField.fieldType);
	} else {
		inf->fieldType = targetField.fieldType;
	}
	decfield.fieldIndex = inf->fieldIndex;
	decfield.fieldType = inf->fieldType;

	if (inf->fieldType==GF_SG_VRML_SFNODE) {
		decfield.far_ptr = inf->field_ptr = &inf->new_node;
	} else if (inf->fieldType==GF_SG_VRML_MFNODE) {
		decfield.far_ptr = inf->field_ptr = &inf->node_list;
	} else {
		decfield.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(inf->fieldType);
	}
	e = gf_bifs_dec_sf_field(codec, bs, target, &decfield, GF_TRUE);
	return e;
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
		GF_Node *n = gf_sg_find_node(codec->current_graph, ID);
		if (!n) return GF_OK;
		com = gf_sg_command_new(codec->current_graph, GF_SG_NODE_DELETE_EX);
		BM_SetCommandNode(com, n);
		gf_list_add(com_list, com);
	}
	return GF_OK;
	case 7:
		return BM_XReplace(codec, bs, com_list);

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
	def = gf_sg_find_node(codec->current_graph, NodeID);
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
		gf_node_register(node, NULL);
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
	def = gf_sg_find_node(codec->current_graph, NodeID);
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
			gf_node_register(node, NULL);
		}
	} else {
		com = gf_sg_command_new(codec->current_graph, GF_SG_INDEXED_INSERT);
		BM_SetCommandNode(com, def);
		inf = gf_sg_command_field_new(com);
		inf->pos = pos;
		inf->fieldIndex = field_ind;
		inf->fieldType = sffield.fieldType;
		sffield.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(sffield.fieldType);
		codec->LastError = gf_bifs_dec_sf_field(codec, bs, def, &sffield, GF_TRUE);
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
		if (codec->UseName) gf_bifs_dec_name(bs, name);
	}
	/*origin*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	OutNode = gf_sg_find_node(codec->current_graph, node_id);
	if (!OutNode) return GF_SG_UNKNOWN_NODE;

	numBits = gf_node_get_num_fields_in_mode(OutNode, GF_SG_FIELD_CODING_OUT) - 1;
	numBits = gf_get_bit_size(numBits);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(OutNode, ind, GF_SG_FIELD_CODING_OUT, &outField);
	if (e) return e;

	/*target*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	InNode = gf_sg_find_node(codec->current_graph, node_id);
	if (!InNode) return GF_SG_UNKNOWN_NODE;

	numBits = gf_node_get_num_fields_in_mode(InNode, GF_SG_FIELD_CODING_IN) - 1;
	numBits = gf_get_bit_size(numBits);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(InNode, ind, GF_SG_FIELD_CODING_IN, &inField);
	if (e) return e;

	com = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_INSERT);
	com->RouteID = RouteID;
	if (codec->UseName) com->def_name = gf_strdup( name);
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
	node = gf_sg_find_node(codec->current_graph, NodeID);
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
		n = gf_sg_find_node(codec->current_graph, ID);
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
	node = gf_sg_find_node(codec->current_graph, NodeID);
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
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	e = gf_node_get_field(node, field_ind, &field);
	if (e) return e;

	com = gf_sg_command_new(codec->current_graph, GF_SG_FIELD_REPLACE);
	BM_SetCommandNode(com, node);
	inf = gf_sg_command_field_new(com);
	inf->fieldIndex = field_ind;
	inf->fieldType = field.fieldType;
	if (inf->fieldType == GF_SG_VRML_SFNODE) {
		field.far_ptr = inf->field_ptr = &inf->new_node;
	} else if (inf->fieldType == GF_SG_VRML_MFNODE) {
		field.far_ptr = inf->field_ptr = &inf->node_list;
	} else {
		field.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(field.fieldType);
	}
	/*parse the field*/
	codec->LastError = gf_bifs_dec_field(codec, bs, node, &field, GF_TRUE);

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

	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	e = gf_node_get_field(node, field_ind, &field);
	if (e) return e;
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
		if (inf->new_node) gf_node_register(inf->new_node, NULL);
	} else {
		memcpy(&sffield, &field, sizeof(GF_FieldInfo));
		sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		inf->fieldType = sffield.fieldType;
		sffield.far_ptr = inf->field_ptr = gf_sg_vrml_field_pointer_new(sffield.fieldType);
		codec->LastError = gf_bifs_dec_sf_field(codec, bs, node, &sffield, GF_TRUE);
	}
	gf_list_add(com_list, com);
	return codec->LastError;
}

GF_Err BM_ParseRouteReplace(GF_BifsDecoder *codec, GF_BitStream *bs, GF_List *com_list)
{
	GF_Err e;
	GF_Command *com;
	u32 RouteID, numBits, ind, node_id, fromID, toID;
	GF_Node *OutNode, *InNode;

	RouteID = 1+gf_bs_read_int(bs, codec->info->config.RouteIDBits);

	/*origin*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	OutNode = gf_sg_find_node(codec->current_graph, node_id);
	if (!OutNode) return GF_NON_COMPLIANT_BITSTREAM;
	numBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(OutNode, GF_SG_FIELD_CODING_OUT) - 1);
	ind = gf_bs_read_int(bs, numBits);
	e = gf_bifs_get_field_index(OutNode, ind, GF_SG_FIELD_CODING_OUT, &fromID);
	if (e) return e;

	/*target*/
	node_id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	InNode = gf_sg_find_node(codec->current_graph, node_id);
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
	com->use_names = codec->UseName;

	/*restore*/
	com->node = codec->scenegraph->RootNode;
	codec->scenegraph->RootNode = backup_root;
	gf_list_add(com_list, com);
	/*insert routes*/
	while (gf_list_count(codec->scenegraph->Routes)) {
		GF_Route *r = (GF_Route*)gf_list_get(codec->scenegraph->Routes, 0);
		GF_Command *ri = gf_sg_command_new(codec->current_graph, GF_SG_ROUTE_INSERT);
		gf_list_rem(codec->scenegraph->Routes, 0);
		ri->fromFieldIndex = r->FromField.fieldIndex;
		ri->fromNodeID = gf_node_get_id(r->FromNode);
		ri->toFieldIndex = r->ToField.fieldIndex;
		ri->toNodeID = gf_node_get_id(r->ToNode);
		if (r->ID) ri->RouteID = r->ID;
		ri->def_name = r->name ? gf_strdup(r->name) : NULL;
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
	GF_Err e;
	go = 1;
	e = GF_OK;

	codec->LastError = GF_OK;
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
	}
	while (gf_list_count(codec->QPs)) {
		gf_bifs_dec_qp_remove(codec, GF_TRUE);
	}
	return GF_OK;
}

void BM_EndOfStream(void *co)
{
	((GF_BifsDecoder *) co)->LastError = GF_IO_ERR;
}

void gf_bs_set_eos_callback(GF_BitStream *bs, void (*EndOfStream)(void *par), void *par);


GF_Err gf_bifs_flush_command_list(GF_BifsDecoder *codec)
{
	GF_BitStream *bs;
	GF_Err e;
	CommandBufferItem *cbi;
	u32 NbPass = gf_list_count(codec->command_buffers);
	GF_List *nextPass = gf_list_new();
	while (NbPass) {
		while (gf_list_count(codec->command_buffers)) {
			cbi = (CommandBufferItem *)gf_list_get(codec->command_buffers, 0);
			gf_list_rem(codec->command_buffers, 0);
			codec->current_graph = gf_node_get_graph(cbi->node);
			e = GF_OK;
			if (cbi->cb->bufferSize) {
				bs = gf_bs_new((char*)cbi->cb->buffer, cbi->cb->bufferSize, GF_BITSTREAM_READ);
				gf_bs_set_eos_callback(bs, BM_EndOfStream, codec);
				e = BM_ParseCommand(codec, bs, cbi->cb->commandList);
				gf_bs_del(bs);
			}
			if (!e) {
				gf_free(cbi);
				continue;
			}
			/*this may be an error or a dependency pb - reset coimmand list and move to next pass*/
			while (gf_list_count(cbi->cb->commandList)) {
				u32 i;
				GF_CommandField *cf;
				GF_Command *com = (GF_Command *)gf_list_get(cbi->cb->commandList, 0);
				gf_list_rem(cbi->cb->commandList, 0);
				cf = (GF_CommandField *) gf_list_get(com->command_fields, 0);
				if (cf && cf->fieldType==GF_SG_VRML_SFCOMMANDBUFFER) {
					for (i=0; i<gf_list_count(codec->command_buffers); i++) {
						CommandBufferItem *cbi2 = (CommandBufferItem *)gf_list_get(codec->command_buffers, i);
						if (cbi2->cb == cf->field_ptr) {
							gf_free(cbi2);
							gf_list_rem(codec->command_buffers, i);
							i--;
						}
					}
				}
				gf_sg_command_del(com);
			}
			gf_list_add(nextPass, cbi);
		}
		if (!gf_list_count(nextPass)) break;
		/*prepare next pass*/
		while (gf_list_count(nextPass)) {
			cbi = (CommandBufferItem *)gf_list_get(nextPass, 0);
			gf_list_rem(nextPass, 0);
			gf_list_add(codec->command_buffers, cbi);
		}
		NbPass --;
		if (NbPass > gf_list_count(codec->command_buffers)) NbPass = gf_list_count(codec->command_buffers);
		codec->LastError = GF_OK;
	}
	gf_list_del(nextPass);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_bifs_decode_command_list(GF_BifsDecoder *codec, u16 ESID, u8 *data, u32 data_length, GF_List *com_list)
{
	GF_BitStream *bs;
	GF_Err e;

	if (!codec || !data || !codec->dec_memory_mode || !com_list) return GF_BAD_PARAM;

	codec->info = gf_bifs_dec_get_stream(codec, ESID);
	if (!codec->info) return GF_BAD_PARAM;
	if (codec->info->config.elementaryMasks ) return GF_NOT_SUPPORTED;

	/*root parse (not conditionals)*/
	assert(codec->scenegraph);
	/*setup current scene graph*/
	codec->current_graph = codec->scenegraph;

	codec->ActiveQP = (M_QuantizationParameter*) codec->scenegraph->global_qp;

	bs = gf_bs_new(data, data_length, GF_BITSTREAM_READ);
	gf_bs_set_eos_callback(bs, BM_EndOfStream, codec);

	e = BM_ParseCommand(codec, bs, com_list);
	gf_bs_del(bs);

	/*decode conditionals / input sensors*/
	if (!e) {
		gf_bifs_flush_command_list(codec);
	}
	/*if err or not reset conditionals*/
	while (gf_list_count(codec->command_buffers)) {
		CommandBufferItem *cbi = (CommandBufferItem *)gf_list_get(codec->command_buffers, 0);
		gf_free(cbi);
		gf_list_rem(codec->command_buffers, 0);
	}

	/*reset current config*/
	codec->info = NULL;
	codec->current_graph = NULL;



//	gf_mx_v(codec->mx);
	return e;
}

#endif /*GPAC_DISABLE_BIFS*/

