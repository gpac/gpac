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

#ifndef GPAC_DISABLE_BIFS_ENC

GF_Err BE_EncProtoList(GF_BifsEncoder *codec, GF_List *protoList, GF_BitStream *bs);


void gf_bifs_enc_name(GF_BifsEncoder *codec, GF_BitStream *bs, char *name)
{
	u32 i = 0;
	if (!name) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BIFS] Coding IDs using names but no name is specified\n"));
		i = 1;
	} else {
		while (name[i]) {
			gf_bs_write_int(bs, name[i], 8);
			i++;
		}
	}
	gf_bs_write_int(bs, 0, 8);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] DEF\t\t%d\t\t%s\n", 8*i, name));
}

static GF_Err BE_XReplace(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	u32 i,nbBits;
	GF_FieldInfo field;
	GF_Err e;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_BAD_PARAM;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);


	gf_bs_write_int(bs, gf_node_get_id(com->node)-1, codec->info->config.NodeIDBits);
	nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN)-1);
	gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &i);
	GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "field", NULL);

	gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (!gf_sg_vrml_is_sf_field(field.fieldType)) {
		if ((inf->pos != -2) || com->toNodeID) {
			GF_BIFS_WRITE_INT(codec, bs, 1, 1, "indexedReplacement", NULL);
			if (com->toNodeID) {
				GF_Node *n = gf_bifs_enc_find_node(codec, com->toNodeID);
				GF_BIFS_WRITE_INT(codec, bs, 1, 1, "dynamicIndex", NULL);
				GF_BIFS_WRITE_INT(codec, bs, com->toNodeID-1, codec->info->config.NodeIDBits, "idxNodeID", NULL);
				nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_DEF)-1);
				gf_bifs_field_index_by_mode(n, com->toFieldIndex, GF_SG_FIELD_CODING_DEF, &i);
				GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "idxField", NULL);
			} else {
				GF_BIFS_WRITE_INT(codec, bs, 0, 1, "dynamicIndex", NULL);
				if (inf->pos==-1) {
					GF_BIFS_WRITE_INT(codec, bs, 3, 2, "replacementPosition", NULL);
				} else if (inf->pos==0) {
					GF_BIFS_WRITE_INT(codec, bs, 2, 2, "replacementPosition", NULL);
				} else {
					GF_BIFS_WRITE_INT(codec, bs, 0, 2, "replacementPosition", NULL);
					GF_BIFS_WRITE_INT(codec, bs, inf->pos, 16, "position", NULL);
				}
			}
		} else {
			GF_BIFS_WRITE_INT(codec, bs, 0, 1, "indexedReplacement", NULL);
		}
	}
	if (field.fieldType==GF_SG_VRML_MFNODE) {
		if (com->ChildNodeTag) {
			GF_Node *n;
			if (com->ChildNodeTag>0) {
				n = gf_node_new(codec->scene_graph, com->ChildNodeTag);
			} else {
				GF_Proto *proto = gf_sg_find_proto(codec->scene_graph, -com->ChildNodeTag , NULL);
				if (!proto) return GF_SG_UNKNOWN_NODE;
				n = gf_sg_proto_create_instance(codec->scene_graph, proto);
			}
			if (!n) return GF_SG_UNKNOWN_NODE;
			gf_node_register(n, NULL);

			GF_BIFS_WRITE_INT(codec, bs, 1, 1, "childField", NULL);

			nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_IN)-1);
			gf_bifs_field_index_by_mode(n, com->child_field, GF_SG_FIELD_CODING_IN, &i);
			GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "childField", NULL);
			gf_node_unregister(n, NULL);
		} else {
			GF_BIFS_WRITE_INT(codec, bs, 0, 1, "childField", NULL);
		}
	}
	if (com->fromNodeID) {
		GF_Node *n = gf_bifs_enc_find_node(codec, com->fromNodeID);
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "valueFromNode", NULL);

		GF_BIFS_WRITE_INT(codec, bs, com->fromNodeID-1, codec->info->config.NodeIDBits, "sourceNodeID", NULL);
		nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_DEF)-1);
		gf_bifs_field_index_by_mode(n, com->fromFieldIndex, GF_SG_FIELD_CODING_DEF, &i);
		GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "sourceField", NULL);

		return GF_OK;
	}

	GF_BIFS_WRITE_INT(codec, bs, 0, 1, "valueFromNode", NULL);

	field.far_ptr = inf->field_ptr;
	field.fieldType = inf->fieldType;
	e = gf_bifs_enc_field(codec, bs, com->node, &field);
	return e;
}

static GF_Err BE_MultipleIndexedReplace(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	u32 i,nbBits, count, maxPos, nbBitsPos;
	GF_FieldInfo field;
	GF_Err e;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);


	gf_bs_write_int(bs, gf_node_get_id(com->node)-1, codec->info->config.NodeIDBits);
	nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN)-1);
	gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &i);
	GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "field", NULL);

	gf_node_get_field(com->node, inf->fieldIndex, &field);
	field.fieldType = inf->fieldType;

	count = gf_list_count(com->command_fields);
	maxPos = 0;
	for (i=0; i<count; i++) {
		inf = (GF_CommandField *)gf_list_get(com->command_fields, i);
		if (maxPos < (u32) inf->pos) maxPos = inf->pos;
	}
	nbBitsPos = gf_get_bit_size(maxPos);
	GF_BIFS_WRITE_INT(codec, bs, nbBitsPos, 5, "nbBitsPos", NULL);

	nbBits = gf_get_bit_size(count);
	GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "nbBits", NULL);
	GF_BIFS_WRITE_INT(codec, bs, count, nbBits, "count", NULL);

	for (i=0; i<count; i++) {
		inf = (GF_CommandField *)gf_list_get(com->command_fields, i);
		GF_BIFS_WRITE_INT(codec, bs, inf->pos, nbBitsPos, "idx", NULL);
		field.far_ptr = inf->field_ptr;
		e = gf_bifs_enc_field(codec, bs, com->node, &field);
		if (e) return e;
	}
	return GF_OK;
}

static GF_Err BE_MultipleReplace(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	u32 i, j, nbBits, count, numFields, allField;
	Bool use_list;
	GF_FieldInfo field;
	GF_Err e;

	gf_bs_write_int(bs, gf_node_get_id(com->node)-1, codec->info->config.NodeIDBits);

	count = gf_list_count(com->command_fields);
	use_list = GF_TRUE;
	numFields = gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_DEF);
	nbBits = gf_get_bit_size(numFields - 1);
	if (count < 1+count*(1+nbBits)) use_list = GF_FALSE;
	GF_BIFS_WRITE_INT(codec, bs, use_list ? 0 : 1, 1, "isMask", NULL);

	for (i=0; i<numFields; i++) {
		GF_CommandField *inf = NULL;
		gf_bifs_get_field_index(com->node, i, GF_SG_FIELD_CODING_DEF, &allField);
		for (j=0; j<count; j++) {
			inf = (GF_CommandField *)gf_list_get(com->command_fields, j);
			if (inf->fieldIndex==allField) break;
			inf = NULL;
		}
		if (!inf) {
			if (!use_list) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "Mask", NULL);
			continue;
		}
		/*common case*/
		gf_node_get_field(com->node, inf->fieldIndex, &field);
		if (use_list) {
			/*not end flag*/
			GF_BIFS_WRITE_INT(codec, bs, 0, 1, "end", NULL);
		} else {
			/*mask flag*/
			GF_BIFS_WRITE_INT(codec, bs, 1, 1, "Mask", NULL);
		}
		if (use_list) GF_BIFS_WRITE_INT(codec, bs, i, nbBits, "field", (char*)field.name);
		field.far_ptr = inf->field_ptr;
		e = gf_bifs_enc_field(codec, bs, com->node, &field);
		if (e) return e;
	}
	/*end flag*/
	if (use_list) GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
	return GF_OK;
}

static GF_Err BE_GlobalQuantizer(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	GF_Err e;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);
	if (inf->new_node) ((M_QuantizationParameter *)inf->new_node)->isLocal = 0;
	e = gf_bifs_enc_node(codec, inf->new_node, NDT_SFWorldNode, bs, NULL);
	if (e) return e;

	/*reset global QP*/
	if (codec->scene_graph->global_qp) {
		gf_node_unregister(codec->scene_graph->global_qp, NULL);
		codec->scene_graph->global_qp = NULL;
	}
	codec->ActiveQP = NULL;

	/*no QP*/
	if (!inf->new_node) return GF_OK;

	/*register global QP*/
	codec->scene_graph->global_qp = inf->new_node;
	gf_node_register(inf->new_node, NULL);
	codec->ActiveQP = (M_QuantizationParameter *) inf->new_node;
	codec->ActiveQP->isLocal = 0;
	return GF_OK;
}

static GF_Err BE_EncProtoDelete(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	u32 nbBits, i;
	Bool use_list = GF_FALSE;
	nbBits = gf_get_bit_size(com->del_proto_list_size);
	if (nbBits+5>com->del_proto_list_size) use_list = GF_TRUE;
	GF_BIFS_WRITE_INT(codec, bs, use_list, 1, "isList", NULL);
	if (!use_list) {
		GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "len", NULL);
		GF_BIFS_WRITE_INT(codec, bs, com->del_proto_list_size, nbBits, "len", NULL);
	}
	for (i=0; i<com->del_proto_list_size; i++) {
		if (use_list) GF_BIFS_WRITE_INT(codec, bs, 1, 1, "moreProto", NULL);
		GF_BIFS_WRITE_INT(codec, bs, com->del_proto_list[i], codec->info->config.ProtoIDBits, "protoID", NULL);
	}
	if (use_list) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreProto", NULL);
	return GF_OK;
}

static GF_Err BE_ExtendedUpdate(GF_BifsEncoder * codec, GF_Command *com, GF_BitStream *bs)
{
	GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Insert", NULL);
	GF_BIFS_WRITE_INT(codec, bs, 1, 2, "ExtendedUpdate", NULL);
	switch (com->tag) {
	case GF_SG_PROTO_INSERT:
		GF_BIFS_WRITE_INT(codec, bs, 0, 8, "MultipleReplace", NULL);
		return BE_EncProtoList(codec, com->new_proto_list, bs);
	case GF_SG_PROTO_DELETE:
		GF_BIFS_WRITE_INT(codec, bs, 1, 8, "ProtoDelete", NULL);
		return BE_EncProtoDelete(codec, com, bs);
	case GF_SG_PROTO_DELETE_ALL:
		GF_BIFS_WRITE_INT(codec, bs, 2, 8, "DeleteAllProtos", NULL);
		return GF_OK;
	case GF_SG_MULTIPLE_INDEXED_REPLACE:
		GF_BIFS_WRITE_INT(codec, bs, 3, 8, "MultipleReplace", NULL);
		return BE_MultipleIndexedReplace(codec, com, bs);
	case GF_SG_MULTIPLE_REPLACE:
		GF_BIFS_WRITE_INT(codec, bs, 4, 8, "MultipleReplace", NULL);
		return BE_MultipleReplace(codec, com, bs);
	case GF_SG_GLOBAL_QUANTIZER:
		GF_BIFS_WRITE_INT(codec, bs, 5, 8, "GlobalQuantizer", NULL);
		return BE_GlobalQuantizer(codec, com, bs);
	case GF_SG_NODE_DELETE_EX:
		GF_BIFS_WRITE_INT(codec, bs, 6, 8, "MultipleReplace", NULL);
		GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);
		return GF_OK;
	case GF_SG_XREPLACE:
		GF_BIFS_WRITE_INT(codec, bs, 7, 8, "XReplace", NULL);
		return BE_XReplace(codec, com, bs);
	default:
		return GF_BAD_PARAM;
	}
}

/*inserts a node in a container (node.children)*/
GF_Err BE_NodeInsert(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	u32 NDT;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);

	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);

	NDT = gf_bifs_get_child_table(com->node);

	switch (inf->pos) {
	case 0:
		GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FIRST", "idx");
		break;
	case -1:
		GF_BIFS_WRITE_INT(codec, bs, 3, 2, "LAST", "idx");
		break;
	default:
		GF_BIFS_WRITE_INT(codec, bs, 0, 2, "pos", "idx");
		GF_BIFS_WRITE_INT(codec, bs, inf->pos, 8, "pos", NULL);
		break;
	}
	return gf_bifs_enc_node(codec, inf->new_node, NDT, bs, NULL);
}

GF_Err BE_IndexInsert(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	GF_Err e;
	u32 NumBits, ind;
	GF_FieldInfo field, sffield;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);

	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);

	/*index insertion uses IN mode for field index*/
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN)-1);
	gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	GF_BIFS_WRITE_INT(codec, bs, ind, NumBits, "field", NULL);

	switch (inf->pos) {
	case 0:
		GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FIRST", "idx");
		break;
	case -1:
		GF_BIFS_WRITE_INT(codec, bs, 3, 2, "LAST", "idx");
		break;
	default:
		GF_BIFS_WRITE_INT(codec, bs, 0, 2, "pos", "idx");
		GF_BIFS_WRITE_INT(codec, bs, inf->pos, 16, "pos", NULL);
		break;
	}
	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType))
		return GF_NON_COMPLIANT_BITSTREAM;

	memcpy(&sffield, &field, sizeof(GF_FieldInfo));
	sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
	sffield.far_ptr = inf->field_ptr;

	/*rescale the MFField and parse the SFField*/
	if (field.fieldType==GF_SG_VRML_MFNODE) {
		return gf_bifs_enc_node(codec, inf->new_node, field.NDTtype, bs, com->node);
	} else {
		return gf_bifs_enc_sf_field(codec, bs, com->node, &sffield);
	}
}


GF_Err BE_IndexDelete(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	u32 NumBits, ind;
	GF_Err e;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);

	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);

	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN) - 1);
	e = gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	if (e) return e;
	GF_BIFS_WRITE_INT(codec, bs, ind, NumBits, "field", NULL);

	switch (inf->pos) {
	case 0:
		GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FIRST", "idw");
		break;
	case -1:
		GF_BIFS_WRITE_INT(codec, bs, 3, 2, "LAST", "idx");
		break;
	default:
		GF_BIFS_WRITE_INT(codec, bs, 0, 2, "pos", "idx");
		GF_BIFS_WRITE_INT(codec, bs, inf->pos, 16, "pos", NULL);
		break;
	}
	return GF_OK;
}


GF_Err BE_NodeReplace(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	GF_Node *new_node = NULL;
	if (gf_list_count(com->command_fields)) {
		GF_CommandField *inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);
		if (inf) new_node = inf->new_node;
	}
	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);
	return gf_bifs_enc_node(codec, new_node, NDT_SFWorldNode, bs, NULL);
}

GF_Err BE_FieldReplace(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	GF_Err e;
	u32 ind, NumBits;
	GF_FieldInfo field;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);

	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);

	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN)-1);
	gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	GF_BIFS_WRITE_INT(codec, bs, ind, NumBits, "field", NULL);

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;
	field.far_ptr = inf->field_ptr;

	/* Warning: To be changed when proper solution is found */
	if (gf_sg_vrml_get_sf_type(field.fieldType) == GF_SG_VRML_SFSCRIPT) codec->is_encoding_command = GF_TRUE;

	e = gf_bifs_enc_field(codec, bs, com->node, &field);

	codec->is_encoding_command = GF_FALSE;
	return e;
}

GF_Err BE_IndexFieldReplace(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs)
{
	u32 ind, NumBits;
	GF_Err e;
	GF_FieldInfo field, sffield;
	GF_CommandField *inf;
	if (!gf_list_count(com->command_fields)) return GF_OK;
	inf = (GF_CommandField *)gf_list_get(com->command_fields, 0);

	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(com->node, GF_SG_FIELD_CODING_IN)-1);
	gf_bifs_field_index_by_mode(com->node, inf->fieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	GF_BIFS_WRITE_INT(codec, bs, ind, NumBits, "field", NULL);

	e = gf_node_get_field(com->node, inf->fieldIndex, &field);
	if (e) return e;
	if (gf_sg_vrml_is_sf_field(field.fieldType))
		return GF_NON_COMPLIANT_BITSTREAM;

	switch (inf->pos) {
	case 0:
		GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FIRST", "idx");
		break;
	case -1:
		GF_BIFS_WRITE_INT(codec, bs, 3, 2, "LAST", "idx");
		break;
	default:
		GF_BIFS_WRITE_INT(codec, bs, 0, 2, "pos", "idx");
		GF_BIFS_WRITE_INT(codec, bs, inf->pos, 16, "pos", NULL);
		break;
	}

	if (field.fieldType == GF_SG_VRML_MFNODE) {
		e = gf_bifs_enc_node(codec, inf->new_node, field.NDTtype, bs, com->node);
	} else {
		memcpy(&sffield, &field, sizeof(GF_FieldInfo));
		sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
		sffield.far_ptr = inf->field_ptr;
		e = gf_bifs_enc_sf_field(codec, bs, com->node, &sffield);
	}
	return e;
}

GF_Err BE_RouteReplace(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs, Bool isInsert)
{
	GF_Err e;
	GF_Node *n;
	u32 numBits, ind;

	if (isInsert) {
		GF_BIFS_WRITE_INT(codec, bs, com->RouteID ? 1 : 0, 1, "isDEF", NULL);
		if (com->RouteID) {
			GF_BIFS_WRITE_INT(codec, bs, com->RouteID-1, codec->info->config.RouteIDBits, "RouteID", NULL);
			if (codec->UseName) gf_bifs_enc_name(codec, bs, com->def_name);
		}
	} else {
		GF_BIFS_WRITE_INT(codec, bs, com->RouteID - 1, codec->info->config.RouteIDBits, "RouteID", NULL);
	}

	/*origin*/
	GF_BIFS_WRITE_INT(codec, bs, com->fromNodeID - 1, codec->info->config.NodeIDBits, "outNodeID", NULL);
	n = gf_bifs_enc_find_node(codec, com->fromNodeID);
	numBits = gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_OUT) - 1;
	numBits = gf_get_bit_size(numBits);
	e = gf_bifs_field_index_by_mode(n, com->fromFieldIndex, GF_SG_FIELD_CODING_OUT, &ind);
	if (e) return e;
	GF_BIFS_WRITE_INT(codec, bs, ind, numBits, "outField", NULL);

	/*target*/
	GF_BIFS_WRITE_INT(codec, bs, com->toNodeID - 1, codec->info->config.NodeIDBits, "inNodeID", NULL);
	n = gf_bifs_enc_find_node(codec, com->toNodeID);
	numBits = gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_IN) - 1;
	numBits = gf_get_bit_size(numBits);
	e = gf_bifs_field_index_by_mode(n, com->toFieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	GF_BIFS_WRITE_INT(codec, bs, ind, numBits, "inField", NULL);
	return e;
}


GF_Err BE_EncProtoList(GF_BifsEncoder *codec, GF_List *protoList, GF_BitStream *bs)
{
	u8 useQuant, useAnim;
	u32 i, j, nbRoutes, nbBits, numProtos, numFields, count;
	GF_Node *node;
	GF_ProtoFieldInterface *proto_field;
	GF_Proto *proto, *prev_proto;
	GF_Route *r;
	GF_Err e;
	GF_SceneGraph *rootSG;
	GF_FieldInfo field;

	e = GF_OK;
	if (!protoList || !gf_list_count(protoList)) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreProto", NULL);
		return GF_OK;
	}
	if (!codec->info->config.ProtoIDBits)
		return GF_NON_COMPLIANT_BITSTREAM;

	/*store state*/
	rootSG = codec->current_proto_graph;
	prev_proto = codec->encoding_proto;

	numProtos = gf_list_count(protoList);
	for (i=0; i<numProtos; i++) {
		proto = (GF_Proto*)gf_list_get(protoList, i);
		useQuant = useAnim = 0;
		/*set current proto state*/
		codec->encoding_proto = proto;
		codec->current_proto_graph = proto->sub_graph;

		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "moreProto", NULL);

		/*1- proto interface declaration*/
		GF_BIFS_WRITE_INT(codec, bs, proto->ID, codec->info->config.ProtoIDBits, "protoID", NULL);

		if (codec->UseName) gf_bifs_enc_name(codec, bs, proto->Name);

		numFields = gf_list_count(proto->proto_fields);
		for (j=0; j<numFields; j++) {
			proto_field = (GF_ProtoFieldInterface*)gf_list_get(proto->proto_fields, j);

			GF_BIFS_WRITE_INT(codec, bs, 1, 1, "moreField", NULL);
			GF_BIFS_WRITE_INT(codec, bs, proto_field->EventType, 2, "eventType", NULL);
			GF_BIFS_WRITE_INT(codec, bs, proto_field->FieldType, 6, "fieldType", NULL);

			if (codec->UseName) gf_bifs_enc_name(codec, bs, proto_field->FieldName);
			switch (proto_field->EventType) {
			case GF_SG_EVENT_EXPOSED_FIELD:
			case GF_SG_EVENT_FIELD:
				gf_sg_proto_field_get_field(proto_field, &field);
				if (gf_sg_vrml_is_sf_field(field.fieldType)) {
					e = gf_bifs_enc_sf_field(codec, bs, NULL, &field);
				} else {
					if (codec->info->config.UsePredictiveMFField) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "usePredictive", NULL);
					e = gf_bifs_enc_mf_field(codec, bs, NULL, &field);
				}
				if (e) goto exit;
				break;
			}
			if (proto_field->QP_Type) useQuant = 1;
			if (proto_field->Anim_Type) useAnim = 1;
		}
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreField", NULL);

		GF_BIFS_WRITE_INT(codec, bs, proto->ExternProto.count ? 1 : 0, 1, "externProto", NULL);
		/*externProto*/
		if (proto->ExternProto.count) {
			memset(&field, 0, sizeof(GF_FieldInfo));
			field.far_ptr = &proto->ExternProto;
			field.fieldType = GF_SG_VRML_MFURL;
			field.name = "ExternProto";

			if (codec->info->config.UsePredictiveMFField) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "usePredictive", NULL);
			e = gf_bifs_enc_mf_field(codec, bs, NULL, &field);
			if (e) goto exit;
		} else {
			/*encode sub-proto list*/
			e = BE_EncProtoList(codec, proto->sub_graph->protos, bs);
			if (e) goto exit;

			count = gf_list_count(proto->node_code);
			/*BIFS cannot encode empty protos ! We therefore encode a NULL node instead*/
			if (!count) {
				gf_bifs_enc_node(codec, NULL, NDT_SFWorldNode, bs, NULL);
				GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreNodes", NULL);
			} else {
				for (j=0; j<count; j++) {
					/*parse all nodes in SFWorldNode table*/
					node = (GF_Node*)gf_list_get(proto->node_code, j);
					e = gf_bifs_enc_node(codec, node, NDT_SFWorldNode, bs, NULL);
					if (e) goto exit;
					GF_BIFS_WRITE_INT(codec, bs, (j+1==count) ? 0 : 1, 1, "moreNodes", NULL);
				}
			}

			/*encode routes routes*/
			nbRoutes = count = gf_list_count(proto->sub_graph->Routes);
			for (j=0; j<count; j++) {
				r = (GF_Route*)gf_list_get(proto->sub_graph->Routes, j);
				if (r->IS_route) nbRoutes--;
			}

			GF_BIFS_WRITE_INT(codec, bs, nbRoutes ? 1 : 0, 1, "hasRoute", NULL);
			if (nbRoutes) {
				nbBits = gf_get_bit_size(nbRoutes);
				if (nbBits + 5 > nbRoutes) {
					GF_BIFS_WRITE_INT(codec, bs, 1, 1, "isList", NULL);
					/*list*/
					for (j=0; j<count; j++) {
						r = (GF_Route*)gf_list_get(proto->sub_graph->Routes, j);
						if (r->IS_route) continue;
						e = gf_bifs_enc_route(codec, r, bs);
						if (e) goto exit;
						nbRoutes--;
						GF_BIFS_WRITE_INT(codec, bs, nbRoutes ? 1 : 0, 1, "moreRoute", NULL);
					}
				} else {
					GF_BIFS_WRITE_INT(codec, bs, 0, 1, "isList", NULL);
					GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "nbBits", NULL);
					GF_BIFS_WRITE_INT(codec, bs, nbRoutes, nbBits, "length", NULL);
					for (j=0; j<count; j++) {
						r = (GF_Route*)gf_list_get(proto->sub_graph->Routes, j);
						if (r->IS_route) continue;
						e = gf_bifs_enc_route(codec, r, bs);
						if (e) goto exit;
					}
				}
			}
		}

		/*anim and Quantization stuff*/
		GF_BIFS_WRITE_INT(codec, bs, useQuant, 1, "useQuant", NULL);
		GF_BIFS_WRITE_INT(codec, bs, useAnim, 1, "useAnim", NULL);

		if (!useAnim && !useQuant) continue;

		count = gf_sg_proto_get_field_count(proto);
		for (j=0; j<count; j++) {
			proto_field = gf_sg_proto_field_find(proto, j);
			gf_sg_proto_field_get_field(proto_field, &field);

			/*quant*/
			if (useQuant && ( (field.eventType == GF_SG_EVENT_FIELD) || (field.eventType == GF_SG_EVENT_EXPOSED_FIELD) )) {
				GF_BIFS_WRITE_INT(codec, bs, proto_field->QP_Type, 4, "QPType", NULL);
				if (proto_field->QP_Type==QC_LINEAR_SCALAR) GF_BIFS_WRITE_INT(codec, bs, proto_field->NumBits, 5, "nbBits", NULL);
				GF_BIFS_WRITE_INT(codec, bs, proto_field->hasMinMax, 1, "hasMinMax", NULL);
				if (proto_field->hasMinMax) {
					field.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);
					switch (field.fieldType) {
					case GF_SG_VRML_SFINT32:
					case GF_SG_VRML_SFTIME:
						break;
					default:
						field.fieldType = GF_SG_VRML_SFFLOAT;
						break;
					}
					field.name = "QPMinValue";
					field.far_ptr = proto_field->qp_min_value;
					gf_bifs_enc_sf_field(codec, bs, NULL, &field);

					field.name = "QPMaxValue";
					field.far_ptr = proto_field->qp_max_value;
					gf_bifs_enc_sf_field(codec, bs, NULL, &field);
				}
			}

			/*anim - not supported yet*/
			if (useAnim && ( (field.eventType == GF_SG_EVENT_IN) || (field.eventType == GF_SG_EVENT_EXPOSED_FIELD) )) {
				e = GF_NOT_SUPPORTED;
				goto exit;
			}
		}
	}
	GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreProto", NULL);

exit:
	/*restore scene graph state*/
	codec->encoding_proto = prev_proto;
	codec->current_proto_graph = rootSG;
	return e;
}



GF_Err gf_bifs_enc_route(GF_BifsEncoder *codec, GF_Route *r, GF_BitStream *bs)
{
	GF_Err e;
	u32 numBits, ind;

	if (!r) return GF_BAD_PARAM;

	GF_BIFS_WRITE_INT(codec, bs, r->ID ? 1: 0, 1, "isDEF", NULL);
	/*def'ed route*/
	if (r->ID) {
		GF_BIFS_WRITE_INT(codec, bs, r->ID-1, codec->info->config.RouteIDBits, "routeID", NULL);
		if (codec->UseName) gf_bifs_enc_name(codec, bs, r->name);
	}
	/*origin*/
	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(r->FromNode) - 1, codec->info->config.NodeIDBits, "outNodeID", NULL);
	numBits = gf_node_get_num_fields_in_mode(r->FromNode, GF_SG_FIELD_CODING_OUT) - 1;
	numBits = gf_get_bit_size(numBits);
	e = gf_bifs_field_index_by_mode(r->FromNode, r->FromField.fieldIndex, GF_SG_FIELD_CODING_OUT, &ind);
	if (e) return e;
	GF_BIFS_WRITE_INT(codec, bs, ind, numBits, "outField", NULL);

	/*target*/
	GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(r->ToNode) - 1, codec->info->config.NodeIDBits, "inNodeID", NULL);
	numBits = gf_node_get_num_fields_in_mode(r->ToNode, GF_SG_FIELD_CODING_IN) - 1;
	numBits = gf_get_bit_size(numBits);
	e = gf_bifs_field_index_by_mode(r->ToNode, r->ToField.fieldIndex, GF_SG_FIELD_CODING_IN, &ind);
	GF_BIFS_WRITE_INT(codec, bs, ind, numBits, "inField", NULL);
	return e;
}

GF_Err BE_SceneReplaceEx(GF_BifsEncoder *codec, GF_Command *com, GF_BitStream *bs, GF_List *routes)
{
	u32 i, nbR, nbBits;
	GF_Err e;

	/*reserved*/
	GF_BIFS_WRITE_INT(codec, bs, 0, 6, "reserved", NULL);
	GF_BIFS_WRITE_INT(codec, bs, codec->UseName ? 1 : 0, 1, "useName", NULL);

	if (gf_list_count(com->new_proto_list)) {
		e = BE_EncProtoList(codec, com->new_proto_list, bs);
		if (e) goto exit;
	} else {
		e = BE_EncProtoList(codec, com->in_scene->protos, bs);
		if (e) goto exit;
	}

	/*NULL root is valid for ProtoLibraries*/
	e = gf_bifs_enc_node(codec, com->node, NDT_SFTopNode, bs, NULL);
	if (e || !gf_list_count(routes) ) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "hasRoute", NULL);
		return codec->LastError = e;
	}
	GF_BIFS_WRITE_INT(codec, bs, 1, 1, "hasRoute", NULL);
	nbR = gf_list_count(routes);
	nbBits = gf_get_bit_size(nbR);
	if (nbBits + 5 > nbR) {
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "isList", NULL);
		/*list*/
		for (i=0; i<nbR; i++) {
			e = gf_bifs_enc_route(codec, (GF_Route*)gf_list_get(routes, i), bs);
			if (e) goto exit;
			GF_BIFS_WRITE_INT(codec, bs, (i+1==nbR) ? 0 : 1, 1, "moreRoute", NULL);
		}
	} else {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "isList", NULL);
		GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "nbBits", NULL);
		GF_BIFS_WRITE_INT(codec, bs, nbR, nbBits, "nbRoutes", NULL);
		for (i=0; i<nbR; i++) {
			e = gf_bifs_enc_route(codec, (GF_Route*)gf_list_get(routes, i), bs);
			if (e) goto exit;
		}
	}

exit:
	return codec->LastError = e;
}


GF_Err BE_SceneReplace(GF_BifsEncoder *codec, GF_SceneGraph *graph, GF_BitStream *bs)
{
	u32 i, nbR, nbBits;
	GF_Err e;

	/*reserved*/
	GF_BIFS_WRITE_INT(codec, bs, 0, 6, "reserved", NULL);
	GF_BIFS_WRITE_INT(codec, bs, codec->UseName ? 1 : 0, 1, "useName", NULL);

	/*assign current graph*/
	codec->scene_graph = graph;

	e = BE_EncProtoList(codec, codec->scene_graph ? codec->scene_graph->protos : NULL, bs);
	if (e) goto exit;

	/*NULL root is valid for ProtoLibraries*/
	e = gf_bifs_enc_node(codec, graph ? graph->RootNode : NULL, NDT_SFTopNode, bs, NULL);
	if (e || !graph || !gf_list_count(graph->Routes) ) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "hasRoute", NULL);
		return codec->LastError = e;
	}
	GF_BIFS_WRITE_INT(codec, bs, 1, 1, "hasRoute", NULL);
	nbR = gf_list_count(graph->Routes);
	nbBits = gf_get_bit_size(nbR);
	if (nbBits + 5 > nbR) {
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "isList", NULL);
		/*list*/
		for (i=0; i<nbR; i++) {
			e = gf_bifs_enc_route(codec, (GF_Route*)gf_list_get(graph->Routes, i), bs);
			if (e) goto exit;
			GF_BIFS_WRITE_INT(codec, bs, (i+1==nbR) ? 0 : 1, 1, "moreRoute", NULL);
		}
	} else {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "isList", NULL);
		GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "nbBits", NULL);
		GF_BIFS_WRITE_INT(codec, bs, nbR, nbBits, "nbRoutes", NULL);
		for (i=0; i<nbR; i++) {
			e = gf_bifs_enc_route(codec, (GF_Route*)gf_list_get(graph->Routes, i), bs);
			if (e) goto exit;
		}
	}

exit:
	return codec->LastError = e;
}


GF_Err gf_bifs_enc_commands(GF_BifsEncoder *codec, GF_List *comList, GF_BitStream *bs)
{
	u32 i;
	u32 count;
	GF_List *routes;
	GF_Err e = GF_OK;

	routes = NULL;

	codec->LastError = GF_OK;
	count = gf_list_count(comList);

	for (i=0; i<count; i++) {
		GF_Command *com = (GF_Command*)gf_list_get(comList, i);
		switch (com->tag) {
		case GF_SG_SCENE_REPLACE:
		{
			/*reset node context*/
			while (gf_list_count(codec->encoded_nodes)) gf_list_rem(codec->encoded_nodes, 0);
			GF_BIFS_WRITE_INT(codec, bs, 3, 2, "SceneReplace", NULL);

			if (!com->aggregated) {
				routes = gf_list_new();
				/*now the trick: get all following InsertRoutes and convert as routes*/
				for (; i<count-1; i++) {
					GF_Route *r;
					GF_Command *rcom = (GF_Command*)gf_list_get(comList, i+1);
					if (rcom->tag!=GF_SG_ROUTE_INSERT) break;
					GF_SAFEALLOC(r, GF_Route);
					if (!r) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Cannot allocate route\n"));
						continue;
					}
					r->FromField.fieldIndex = rcom->fromFieldIndex;
					r->FromNode = gf_sg_find_node(codec->scene_graph, rcom->fromNodeID);
					r->ToField.fieldIndex = rcom->toFieldIndex;
					r->ToNode = gf_sg_find_node(codec->scene_graph, rcom->toNodeID);
					r->ID = rcom->RouteID;
					r->name = rcom->def_name;
					gf_list_add(routes, r);
				}
				e = BE_SceneReplaceEx(codec, com, bs, routes);

				while (gf_list_count(routes)) {
					GF_Route *r = (GF_Route*)gf_list_get(routes, 0);
					gf_list_rem(routes, 0);
					gf_free(r);
				}
				gf_list_del(routes);
			} else {
				e = BE_SceneReplaceEx(codec, com, bs, codec->scene_graph->Routes);
			}
		}
		break;
		/*replace commands*/
		case GF_SG_NODE_REPLACE:
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "Replace", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Node", NULL);
			e = BE_NodeReplace(codec, com, bs);
			break;
		case GF_SG_FIELD_REPLACE:
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "Replace", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 1, 2, "Field", NULL);
			e = BE_FieldReplace(codec, com, bs);
			break;
		case GF_SG_INDEXED_REPLACE:
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "Replace", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FieldIndex", NULL);
			e = BE_IndexFieldReplace(codec, com, bs);
			break;
		case GF_SG_ROUTE_REPLACE:
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "Replace", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 3, 2, "Route", NULL);
			e = BE_RouteReplace(codec, com, bs, GF_FALSE);
			break;
		case GF_SG_NODE_INSERT:
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Insert", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Node", NULL);
			e = BE_NodeInsert(codec, com, bs);
			break;
		case GF_SG_INDEXED_INSERT:
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Insert", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FieldIndex", NULL);
			e = BE_IndexInsert(codec, com, bs);
			break;
		case GF_SG_ROUTE_INSERT:
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Insert", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 3, 2, "Route", NULL);
			e = BE_RouteReplace(codec, com, bs, GF_TRUE);
			break;
		case GF_SG_NODE_DELETE:
			GF_BIFS_WRITE_INT(codec, bs, 1, 2, "Delete", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 0, 2, "Node", NULL);
			GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(com->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);
			break;
		case GF_SG_INDEXED_DELETE:
			GF_BIFS_WRITE_INT(codec, bs, 1, 2, "Delete", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 2, 2, "FieldIndex", NULL);
			e = BE_IndexDelete(codec, com, bs);
			break;
		case GF_SG_ROUTE_DELETE:
			GF_BIFS_WRITE_INT(codec, bs, 1, 2, "Delete", NULL);
			GF_BIFS_WRITE_INT(codec, bs, 3, 2, "Route", NULL);
			GF_BIFS_WRITE_INT(codec, bs, com->RouteID - 1, codec->info->config.RouteIDBits, "RouteID", NULL);
			break;

		default:
			e = BE_ExtendedUpdate(codec, com, bs);
			break;
		}
		if (e) break;

		GF_BIFS_WRITE_INT(codec, bs, (i+1==count) ? 0 : 1, 1, "moreCommands", NULL);
	}

	while (gf_list_count(codec->QPs)) gf_bifs_enc_qp_remove(codec, GF_TRUE);
	return e;
}


GF_EXPORT
GF_Err gf_bifs_encoder_get_rap(GF_BifsEncoder *codec, u8 **out_data, u32 *out_data_length)
{
	GF_BitStream *bs;
	GF_Err e;
	GF_List *ctx_bck;

	/*reset context for RAP encoding*/
	ctx_bck = codec->encoded_nodes;
	codec->encoded_nodes = gf_list_new();

	if (!codec->info) codec->info = (BIFSStreamInfo*)gf_list_get(codec->streamInfo, 0);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	GF_BIFS_WRITE_INT(codec, bs, 3, 2, "SceneReplace", NULL);
	e = BE_SceneReplace(codec, codec->scene_graph, bs);
	if (e == GF_OK) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "moreCommands", NULL);
		gf_bs_get_content(bs, out_data, out_data_length);
	}
	gf_bs_del(bs);

	/*restore context*/
	gf_list_del(codec->encoded_nodes);
	codec->encoded_nodes = ctx_bck;

	return e;
}

#endif	/*GPAC_DISABLE_BIFS_ENC*/
