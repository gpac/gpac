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

#include <gpac/maths.h>
#include <gpac/internal/bifs_dev.h>
#include "quant.h"

#ifndef GPAC_DISABLE_BIFS


GF_Err BD_DecMFFieldList(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com);
GF_Err BD_DecMFFieldVec(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com);



void gf_bifs_dec_name(GF_BitStream *bs, char *name, u32 size)
{
	Bool error = GF_FALSE;
	u32 i = 0;
	while (1) {
		char c = gf_bs_read_int(bs, 8);
		if (i<size)
			name[i] = c;
		else {
			error = GF_TRUE;
		}
		if (!c) break;
		i++;
	}
	if (error) {
		name[size-1] = 0;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BIFS] name too long %d bytes but max size %d, truncating\n", i, size));
	}
}


static GF_Err BD_XReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_FieldInfo targetField, fromField;
	GF_Node *target, *n, *fromNode;
	s32 pos = -2;
	void *slot_ptr;
	u32 id, nbBits, ind, aind;
	GF_Err e;


	id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	target = gf_sg_find_node(codec->current_graph, id);
	if (!target) return GF_SG_UNKNOWN_NODE;

	nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(target, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, nbBits);
	e = gf_bifs_get_field_index(target, ind, GF_SG_FIELD_CODING_IN, &aind);
	if (e) return e;
	e = gf_node_get_field(target, aind, &targetField);
	if (e) return e;

	if (!gf_sg_vrml_is_sf_field(targetField.fieldType)) {
		/*this is indexed replacement*/
		if (gf_bs_read_int(bs, 1)) {
			/*index is dynamic*/
			if (gf_bs_read_int(bs, 1)) {
				id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
				n = gf_sg_find_node(codec->current_graph, id);
				if (!n) return GF_SG_UNKNOWN_NODE;

				nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(n, GF_SG_FIELD_CODING_DEF)-1);
				ind = gf_bs_read_int(bs, nbBits);
				e = gf_bifs_get_field_index(n, ind, GF_SG_FIELD_CODING_DEF, &aind);
				if (e) return e;
				e = gf_node_get_field(n, aind, &fromField);
				if (e) return e;

				pos = 0;
				switch (fromField.fieldType) {
				case GF_SG_VRML_SFBOOL:
					if (*(SFBool*)fromField.far_ptr) pos = 1;
					break;
				case GF_SG_VRML_SFINT32:
					if (*(SFInt32*)fromField.far_ptr >=0) pos = *(SFInt32*)fromField.far_ptr;
					break;
				case GF_SG_VRML_SFFLOAT:
					if ( (*(SFFloat *)fromField.far_ptr) >=0) pos = (s32) floor( FIX2FLT(*(SFFloat*)fromField.far_ptr) );
					break;
				case GF_SG_VRML_SFTIME:
					if ( (*(SFTime *)fromField.far_ptr) >=0) pos = (s32) floor( (*(SFTime *)fromField.far_ptr) );
					break;
				}
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
			}
		}
	}

	fromNode = NULL;
	if (gf_bs_read_int(bs, 1)) {
		id = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		fromNode = gf_sg_find_node(codec->current_graph, id);
		if (!fromNode) return GF_SG_UNKNOWN_NODE;

		nbBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(fromNode, GF_SG_FIELD_CODING_DEF)-1);
		ind = gf_bs_read_int(bs, nbBits);
		e = gf_bifs_get_field_index(fromNode, ind, GF_SG_FIELD_CODING_DEF, &aind);
		if (e) return e;
		e = gf_node_get_field(fromNode, aind, &fromField);
		if (e) return e;
	}

	/*indexed replacement*/
	if (pos>=-1) {
		/*if MFNode remove the child and set new node*/
		if (targetField.fieldType == GF_SG_VRML_MFNODE) {
			GF_Node *newnode;
			if (fromNode) {
				newnode = *(GF_Node**)fromField.far_ptr;
			} else {
				newnode = gf_bifs_dec_node(codec, bs, targetField.NDTtype);
			}
			gf_node_replace_child(target, (GF_ChildNodeItem**) targetField.far_ptr, pos, newnode);
			if (newnode) gf_node_register(newnode, NULL);
		}
		/*erase the field item*/
		else {
			u32 sftype;
			if ((pos < 0) || ((u32) pos >= ((GenMFField *) targetField.far_ptr)->count) ) {
				pos = ((GenMFField *)targetField.far_ptr)->count - 1;
				/*may happen with text and default value*/
				if (pos < 0) {
					pos = 0;
					gf_sg_vrml_mf_alloc(targetField.far_ptr, targetField.fieldType, 1);
				}
			}
			e = gf_sg_vrml_mf_get_item(targetField.far_ptr, targetField.fieldType, & slot_ptr, pos);
			if (e) return e;
			sftype = gf_sg_vrml_get_sf_type(targetField.fieldType);

			if (fromNode) {
				if (sftype==fromField.fieldType)
					gf_sg_vrml_field_copy(slot_ptr, fromField.far_ptr, sftype);
			} else {
				GF_FieldInfo sffield;
				memcpy(&sffield, &targetField, sizeof(GF_FieldInfo));
				sffield.fieldType = sftype;
				sffield.far_ptr = slot_ptr;
				gf_bifs_dec_sf_field(codec, bs, target, &sffield, GF_FALSE);
			}
		}
		gf_bifs_check_field_change(target, &targetField);
		return GF_OK;
	}
	switch (targetField.fieldType) {
	case GF_SG_VRML_SFNODE:
	{
		GF_Node *newnode;
		if (fromNode) {
			newnode = *(GF_Node**)fromField.far_ptr;
		} else {
			newnode = gf_bifs_dec_node(codec, bs, targetField.NDTtype);
		}

		n = *((GF_Node **) targetField.far_ptr);

		*((GF_Node **) targetField.far_ptr) = newnode;
		if (newnode) gf_node_register(newnode, target);

		if (n) gf_node_unregister(n, target);

		break;
	}
	case GF_SG_VRML_MFNODE:
	{
		GF_ChildNodeItem *previous = * (GF_ChildNodeItem **) targetField.far_ptr;
		* ((GF_ChildNodeItem **) targetField.far_ptr) = NULL;

		if (fromNode) {
			GF_ChildNodeItem *list, *prev, *cur;
			list = * ((GF_ChildNodeItem **) targetField.far_ptr);
			prev=NULL;
			while (list) {
				cur = (GF_ChildNodeItem*)gf_malloc(sizeof(GF_ChildNodeItem));
				cur->next = NULL;
				cur->node = list->node;
				if (prev) {
					prev->next = cur;
				} else {
					* ((GF_ChildNodeItem **) targetField.far_ptr) = cur;
				}
				gf_node_register(list->node, target);
				prev = cur;
				list = list->next;
			}
		} else {
			e = gf_bifs_dec_field(codec, bs, target, &targetField, GF_FALSE);
			if (e) return e;
		}
		if (previous)
			gf_node_unregister_children(target, previous);

		break;
	}
	default:
		if (!gf_sg_vrml_is_sf_field(targetField.fieldType)) {
			e = gf_sg_vrml_mf_reset(targetField.far_ptr, targetField.fieldType);
		}
		if (e) return e;

		if (fromNode) {
			if (fromField.fieldType == targetField.fieldType)
				gf_sg_vrml_field_clone(targetField.far_ptr, fromField.far_ptr, targetField.fieldType, codec->current_graph);
		} else {
			e = gf_bifs_dec_field(codec, bs, target, &targetField, GF_FALSE);
		}
		break;
	}
	gf_bifs_check_field_change(target, &targetField);
	return e;
}

static GF_Err BD_DecProtoDelete(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 ID, flag, count;
	GF_Proto *proto;

	flag = gf_bs_read_int(bs, 1);
	if (flag) {
		flag = gf_bs_read_int(bs, 1);
		while (flag) {
			ID = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			proto = gf_sg_find_proto(codec->current_graph, ID, NULL);
			if (proto) gf_sg_proto_del(proto);
			flag = gf_bs_read_int(bs, 1);
		}
	} else {
		flag = gf_bs_read_int(bs, 5);
		count = gf_bs_read_int(bs, flag);
		while (count) {
			ID = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
			proto = gf_sg_find_proto(codec->current_graph, ID, NULL);
			if (proto) gf_sg_proto_del(proto);
			count--;
		}
	}
	return GF_OK;
}


static GF_Err BD_DecMultipleIndexReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 ID, ind, field_ind, NumBits, lenpos, lennum, count, pos;
	GF_Node *node;
	GF_Err e;
	GF_FieldInfo field, sffield;

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


	/*cf index value replace */
	if (field.fieldType == GF_SG_VRML_MFNODE) {
		while (count) {
			GF_Node *new_node;
			pos = gf_bs_read_int(bs, lenpos);
			/*first decode*/
			new_node = gf_bifs_dec_node(codec, bs, field.NDTtype);
			if (!new_node) return codec->LastError;
			e = gf_node_register(new_node, node);
			if (e) return e;
			/*then replace*/
			e = gf_node_replace_child(node, (GF_ChildNodeItem**) field.far_ptr, pos, new_node);
			count--;
		}
		if (!e) gf_bifs_check_field_change(node, &field);
		return e;
	}
	/*Not a node list*/
	memcpy(&sffield, &field, sizeof(GF_FieldInfo));
	sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);

	while (count) {
		pos = gf_bs_read_int(bs, lenpos);

		if (pos && pos >= ((GenMFField *)field.far_ptr)->count) {
			pos = ((GenMFField *)field.far_ptr)->count - 1;
		}

		e = gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, & sffield.far_ptr, pos);
		if (e) return e;
		e = gf_bifs_dec_sf_field(codec, bs, node, &sffield, GF_FALSE);
		if (e) break;
		count--;
	}
	if (!e) gf_bifs_check_field_change(node, &field);
	return e;
}

static GF_Err BD_DecMultipleReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Err e;
	u32 NodeID, flag;
	GF_Node *node;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	flag = gf_bs_read_int(bs, 1);
	if (flag) {
		e = gf_bifs_dec_node_mask(codec, bs, node, GF_FALSE);
	} else {
		e = gf_bifs_dec_node_list(codec, bs, node, GF_FALSE);
	}
	return e;
}


static GF_Err BD_DecGlobalQuantizer(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Node *node;
	node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);

	/*reset global QP*/
	if (codec->scenegraph->global_qp) {
		gf_node_unregister(codec->scenegraph->global_qp, NULL);
		codec->scenegraph->global_qp = NULL;
	}
	codec->ActiveQP = NULL;

	if (!node || (gf_node_get_tag(node) != TAG_MPEG4_QuantizationParameter)) {
		if (node) gf_node_unregister(node, NULL);
		return codec->LastError;
	}

	/*register global QP*/
	codec->scenegraph->global_qp = node;
	gf_node_register(node, NULL);
	codec->ActiveQP = (M_QuantizationParameter *) node;
	codec->ActiveQP->isLocal = 0;
	return GF_OK;
}

static GF_Err BD_DecNodeDeleteEx(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 NodeID;
	GF_Node *node;
	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	return gf_node_replace(node, NULL, GF_TRUE);
}

#ifdef GPAC_UNUSED_FUNC
static GF_Err BD_DecOperandReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	s32 pos;
	GF_FieldInfo field;
	GF_FieldInfo src_field;
	GF_Err e;
	u32 NodeID, NumBits, ind, field_ind, type, src_type, dst_type;
	GF_Node *node, *src;
	void *src_ptr, *dst_ptr;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;
	e = gf_node_get_field(node, field_ind, &field);
	if (e) return e;

	dst_type = field.fieldType;
	dst_ptr = field.far_ptr;
	pos = -2;
	if (! gf_sg_vrml_is_sf_field(field.fieldType)) {
		type = gf_bs_read_int(bs, 2);
		switch (type) {
		/*no index*/
		case 0:
			break;
		/*specified*/
		case 1:
			pos = gf_bs_read_int(bs, 8);
			break;
		/*first*/
		case 2:
			pos = 0;
			break;
		/*last*/
		case 3:
			/*-1 means append*/
			pos = ((GenMFField *)field.far_ptr)->count;
			if (!pos)
				return GF_NON_COMPLIANT_BITSTREAM;
			pos -= 1;
			break;
		default:
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (pos>-2) {
			dst_type = gf_sg_vrml_get_sf_type(field.fieldType);
			e = gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &dst_ptr, pos);
			if (e) return e;
		}
	}

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	src = gf_sg_find_node(codec->current_graph, NodeID);
	if (!src) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(src, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(src, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;
	e = gf_node_get_field(src, field_ind, &src_field);
	if (e) return e;

	src_type = src_field.fieldType;
	src_ptr = src_field.far_ptr;
	pos = -2;
	if (! gf_sg_vrml_is_sf_field(src_field.fieldType)) {
		type = gf_bs_read_int(bs, 2);
		switch (type) {
		/*no index*/
		case 0:
			break;
		/*specified*/
		case 1:
			pos = gf_bs_read_int(bs, 8);
			break;
		/*first*/
		case 2:
			pos = 0;
			break;
		/*last*/
		case 3:
			/*-1 means append*/
			pos = ((GenMFField *)src_field.far_ptr)->count;
			if (!pos)
				return GF_NON_COMPLIANT_BITSTREAM;
			pos -= 1;
			break;
		default:
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (pos>-2) {
			src_type = gf_sg_vrml_get_sf_type(src_field.fieldType);
			e = gf_sg_vrml_mf_get_item(src_field.far_ptr, src_field.fieldType, &src_ptr, pos);
			if (e) return e;
		}
	}
	if (src_type!=dst_type) return GF_NON_COMPLIANT_BITSTREAM;
	gf_sg_vrml_field_copy(dst_ptr, src_ptr, src_type);
	return GF_OK;
}
#endif /*GPAC_UNUSED_FUNC*/

static GF_Err BD_DecExtendedUpdate(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 type;

	type = gf_bs_read_int(bs, 8);
	switch (type) {
	case 0:
		return gf_bifs_dec_proto_list(codec, bs, NULL);
	case 1:
		return BD_DecProtoDelete(codec, bs);
	case 2:
		return gf_sg_delete_all_protos(codec->current_graph);
	case 3:
		return BD_DecMultipleIndexReplace(codec, bs);
	case 4:
		return BD_DecMultipleReplace(codec, bs);
	case 5:
		return BD_DecGlobalQuantizer(codec, bs);
	case 6:
		return BD_DecNodeDeleteEx(codec, bs);
	case 7:
		return BD_XReplace(codec, bs);
	default:
		return GF_BIFS_UNKNOWN_VERSION;
	}
}

/*inserts a node in a container (node.children)*/
static GF_Err BD_DecNodeInsert(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 NodeID, NDT;
	s32 type, pos;
	GF_Node *node, *def;
	GF_Err e;

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
	if (!node) return codec->LastError;

	e = gf_node_register(node, def);
	if (e) return e;
	e = gf_node_insert_child(def, node, pos);
	if (!e) {
		GF_FieldInfo field;
		/*get it by name in case no add/removeChildren*/
		e = gf_node_get_field_by_name(def, "children", &field);
		if (e) return e;
		gf_bifs_check_field_change(def, &field);
	}
	return e;
}

/*NB This can insert a node as well (but usually not in the .children field)*/
static GF_Err BD_DecIndexInsert(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Err e;
	u32 NodeID;
	u32 NumBits, ind, field_ind;
	u8 type;
	s32 pos;
	GF_Node *def;
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
		GF_Node *node = gf_bifs_dec_node(codec, bs, field.NDTtype);
		if (!node) return codec->LastError;

		e = gf_node_register(node, def);
		if (e) return e;
		/*this is generic MFNode container*/
		if (pos== -1) {
			e = gf_node_list_add_child( (GF_ChildNodeItem **) field.far_ptr, node);
		} else {
			e = gf_node_list_insert_child((GF_ChildNodeItem **) field.far_ptr, node, pos);
		}

		if (!e) gf_bifs_check_field_change(def, &field);
	} else {
		if (pos == -1) {
			e = gf_sg_vrml_mf_append(field.far_ptr, field.fieldType, & sffield.far_ptr);
		} else {
			/*insert is 0-based*/
			e = gf_sg_vrml_mf_insert(field.far_ptr, field.fieldType, & sffield.far_ptr, pos);
		}
		if (e) return e;
		e = gf_bifs_dec_sf_field(codec, bs, def, &sffield, GF_FALSE);
		if (!e) gf_bifs_check_field_change(def, &field);
	}
	return e;
}


static GF_Err BD_DecInsert(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u8 type;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		return BD_DecNodeInsert(codec, bs);
	/*Extended BIFS updates*/
	case 1:
		return BD_DecExtendedUpdate(codec, bs);
	case 2:
		return BD_DecIndexInsert(codec, bs);
	case 3:
		return gf_bifs_dec_route(codec, bs, GF_TRUE);
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
}


static GF_Err BD_DecIndexDelete(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 NodeID, NumBits, SF_type, ind, field_ind;
	s32 pos;
	u8 type;
	GF_Node *node;
	GF_Err e;
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

	SF_type = gf_sg_vrml_get_sf_type(field.fieldType);

	/*special handling in case of a node*/
	if (SF_type == GF_SG_VRML_SFNODE) {
		GF_ChildNodeItem** nlist_ptr = (GF_ChildNodeItem**) field.far_ptr;
		if (*nlist_ptr) {
			e = gf_node_replace_child(node, nlist_ptr, pos, NULL);
		}
	} else {
		e = gf_sg_vrml_mf_remove(field.far_ptr, field.fieldType, pos);
	}
	/*deletion -> node has changed*/
	if (!e) gf_bifs_check_field_change(node, &field);

	return e;
}

static GF_Err BD_DecDelete(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u8 type;
	u32 ID;
	GF_Node *n;

	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		ID = 1+gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		n = gf_sg_find_node(codec->current_graph, ID);
#ifdef MPEG4_STRICT
		if (!n) return GF_NON_COMPLIANT_BITSTREAM;
#else
		if (!n) return GF_OK;
#endif
		/*this is a delete of a DEF node, remove ALL INSTANCES*/
		return gf_node_replace(n, NULL, GF_FALSE);
	case 2:
		return BD_DecIndexDelete(codec, bs);
	case 3:
		ID = 1+gf_bs_read_int(bs, codec->info->config.RouteIDBits);
		/*don't complain if route can't be deleted (not present)*/
		gf_sg_route_del_by_id(codec->current_graph, ID);
		return GF_OK;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return GF_OK;
}


static GF_Err BD_DecNodeReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u32 NodeID;
	GF_Node *node, *new_node;
	GF_Err e;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	/*this is delete / new on a DEF node: replace ALL instances*/
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;

	/*and just parse a new GF_Node - it is encoded in SFWorldNode table */
	new_node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);
	if (!new_node && codec->LastError) return codec->LastError;

	e = gf_node_replace(node, new_node, GF_FALSE);
	return e;
}

static GF_Err BD_DecFieldReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Err e;
	u32 NodeID, ind, field_ind, NumBits;
	GF_Node *node;
	GF_ChildNodeItem *prev_child;
	GF_FieldInfo field;

	NodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
	node = gf_sg_find_node(codec->current_graph, NodeID);
	if (!node) return GF_NON_COMPLIANT_BITSTREAM;
	NumBits = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_IN)-1);
	ind = gf_bs_read_int(bs, NumBits);
	e = gf_bifs_get_field_index(node, ind, GF_SG_FIELD_CODING_IN, &field_ind);
	if (e) return e;

	e = gf_node_get_field(node, field_ind, &field);
	if (e) return e;

	prev_child = NULL;
	/*store prev MFNode content*/
	if (field.fieldType == GF_SG_VRML_MFNODE) {
		prev_child = * ((GF_ChildNodeItem **) field.far_ptr);
		* ((GF_ChildNodeItem **) field.far_ptr) = NULL;
	}
	/*regular field*/
	else if (!gf_sg_vrml_is_sf_field(field.fieldType)) {
		e = gf_sg_vrml_mf_reset(field.far_ptr, field.fieldType);
		if (e) return e;
	}

	/*parse the field*/
	codec->is_com_dec = GF_TRUE;
	e = gf_bifs_dec_field(codec, bs, node, &field, GF_FALSE);
	codec->is_com_dec = GF_FALSE;
	/*remove prev nodes*/
	if (field.fieldType == GF_SG_VRML_MFNODE) {
		gf_node_unregister_children(node, prev_child);
	}
	if (!e) gf_bifs_check_field_change(node, &field);
	return e;
}

static GF_Err BD_DecIndexValueReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Node *new_node;
	u32 NodeID, ind, field_ind, NumBits, pos;
	u8 type;
	GF_Node *node;
	GF_Err e;
	GF_FieldInfo field, sffield;

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
		if (!field.far_ptr) return GF_NON_COMPLIANT_BITSTREAM;
		pos = ((GenMFField *) field.far_ptr)->count - 1;
		break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*if MFNode remove the child and parse new node*/
	if (field.fieldType == GF_SG_VRML_MFNODE) {
		/*get the new node*/
		new_node = gf_bifs_dec_node(codec, bs, field.NDTtype);
		if (codec->LastError) {
			e = codec->LastError;
			goto exit;
		}
		if (new_node) {
			e = gf_node_register(new_node, node);
			if (e) return e;
		}
		/*replace prev node*/
		e = gf_node_replace_child(node, (GF_ChildNodeItem**) field.far_ptr, pos, new_node);
		if (!e) gf_bifs_check_field_change(node, &field);
	}
	/*erase the field item*/
	else {
		memcpy(&sffield, &field, sizeof(GF_FieldInfo));
		sffield.fieldType = gf_sg_vrml_get_sf_type(field.fieldType);

		/*make sure this is consistent*/
		if (pos && pos >= ((GenMFField *)field.far_ptr)->count) {
			pos = ((GenMFField *)field.far_ptr)->count - 1;
		}

		e = gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, & sffield.far_ptr, pos);
		if (e) return e;
		e = gf_bifs_dec_sf_field(codec, bs, node, &sffield, GF_FALSE);
		if (!e) gf_bifs_check_field_change(node, &field);
	}

exit:
	return e;
}

static GF_Err BD_DecRouteReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Err e;
	u32 RouteID, numBits, ind, node_id, fromID, toID;
	char name[1000], *ptr;
	GF_Route *r;
	GF_Node *OutNode, *InNode;

	RouteID = 1+gf_bs_read_int(bs, codec->info->config.RouteIDBits);

	r = gf_sg_route_find(codec->current_graph, RouteID);
#ifdef MPEG4_STRICT
	if (!r) return GF_NON_COMPLIANT_BITSTREAM;
	ptr = gf_sg_route_get_name(r);
	gf_sg_route_del(r);
#else
	ptr = NULL;
	if (r) {
		ptr = gf_sg_route_get_name(r);
//		gf_sg_route_del(r);
	}
#endif

	if (ptr) strcpy(name, ptr);

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

	if (r) {
		if (r->FromNode->sgprivate->interact)
			gf_list_del_item(r->FromNode->sgprivate->interact->routes, r);

		r->is_setup = 0;
		r->lastActivateTime = 0;

		r->FromNode = OutNode;
		r->FromField.fieldIndex = fromID;
		r->ToNode = InNode;
		r->ToField.fieldIndex = toID;

		if (!r->FromNode->sgprivate->interact) {
			GF_SAFEALLOC(r->FromNode->sgprivate->interact, struct _node_interactive_ext);
			if (!r->FromNode->sgprivate->interact) return GF_OUT_OF_MEM;
		}
		if (!r->FromNode->sgprivate->interact->routes) {
			r->FromNode->sgprivate->interact->routes = gf_list_new();
			if (!r->FromNode->sgprivate->interact->routes) return GF_OUT_OF_MEM;
		}
		gf_list_add(r->FromNode->sgprivate->interact->routes, r);
	} else {
		r = gf_sg_route_new(codec->current_graph, OutNode, fromID, InNode, toID);
		if (!r) return GF_OUT_OF_MEM;
		gf_sg_route_set_id(r, RouteID);
		if (ptr) e = gf_sg_route_set_name(r, name);
	}
	return e;
}


static GF_Err BD_DecReplace(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	u8 type;
	type = gf_bs_read_int(bs, 2);
	switch (type) {
	case 0:
		return BD_DecNodeReplace(codec, bs);
	case 1:
		return BD_DecFieldReplace(codec, bs);
	case 2:
		return BD_DecIndexValueReplace(codec, bs);
	case 3:
		return BD_DecRouteReplace(codec, bs);
	}
	return GF_OK;
}

void command_buffers_del(GF_List *command_buffers);

/*if parent is non-NULL, we are in a proto code parsing, otherwise this is a top-level proto*/
GF_Err gf_bifs_dec_proto_list(GF_BifsDecoder * codec, GF_BitStream *bs, GF_List *proto_list)
{
	u8 flag, field_type, event_type, useQuant, useAnim, f;
	u32 i, NbRoutes, ID, /*numProtos, */ numFields, count, qpsftype, QP_Type, NumBits, nb_cmd_bufs;
	GF_Node *node;
	char name[1000];
	GF_ProtoFieldInterface *proto_field;
	GF_Proto *proto, *ParentProto;
	GF_Err e;
	u32 hasMinMax;
	void *qp_min_value, *qp_max_value;
	GF_SceneGraph *rootSG;
	GF_FieldInfo field;

	NumBits = qpsftype = 0;
	//store proto at codec level
	rootSG = codec->current_graph;
	ParentProto = codec->pCurrentProto;
	e = GF_OK;

	nb_cmd_bufs = gf_list_count(codec->command_buffers);
	//numProtos = 0;
	proto = NULL;
	flag = gf_bs_read_int(bs, 1);
	while (flag) {

		if (!codec->info->config.ProtoIDBits) return GF_NON_COMPLIANT_BITSTREAM;

		/*1- proto interface declaration*/
		ID = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);

		if (codec->UseName) {
			gf_bifs_dec_name(bs, name, 1000);
		} else {
			sprintf(name, "Proto%d", gf_list_count(codec->current_graph->protos) );
		}
		/*create a proto in the current graph*/
		proto = gf_sg_proto_new(codec->current_graph, ID, name, proto_list ? GF_TRUE : GF_FALSE);
		if (!proto) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		if (proto_list) gf_list_add(proto_list, proto);

		/*during parsing, this proto is the current active one - all nodes/proto defined/declared
		below it will belong to its namespace*/
		codec->current_graph = gf_sg_proto_get_graph(proto);
		codec->pCurrentProto = proto;

		numFields = 0;
		flag = gf_bs_read_int(bs, 1);
		while (flag) {
			event_type = gf_bs_read_int(bs, 2);
			field_type = gf_bs_read_int(bs, 6);

			if (codec->UseName) {
				gf_bifs_dec_name(bs, name, 1000);
			} else {
				sprintf(name, "_field%d", numFields);
			}

			/*create field interface*/
			proto_field = gf_sg_proto_field_new(proto, field_type, event_type, name);

			/*get field info */
			gf_sg_proto_field_get_field(proto_field, &field);

			switch (event_type) {
			case GF_SG_EVENT_EXPOSED_FIELD:
			case GF_SG_EVENT_FIELD:
				/*parse default value except nodes ...*/
				if (gf_sg_vrml_is_sf_field(field_type)) {
					e = gf_bifs_dec_sf_field(codec, bs, NULL, &field, GF_FALSE);
				} else {
					if (codec->info->config.UsePredictiveMFField) {
						f = gf_bs_read_int(bs, 1);
						/*predictive encoding of proto field is not possible since QP info is not present yet*/
						assert(!f);
					}
					/*reserved*/
					f = gf_bs_read_int(bs, 1);
					if (!f) {
						if (gf_bs_read_int(bs, 1)) {
							e = BD_DecMFFieldList(codec, bs, NULL, &field, GF_FALSE);
						} else {
							e = BD_DecMFFieldVec(codec, bs, NULL, &field, GF_FALSE);
						}
					}
				}
				if (e) goto exit;

				break;
			}

			flag = gf_bs_read_int(bs, 1);
			numFields++;
		}

		/*2- parse proto code*/
		flag = gf_bs_read_int(bs, 1);

		/*externProto*/
		if (flag) {
			memset(&field, 0, sizeof(GF_FieldInfo));
			field.far_ptr = gf_sg_proto_get_extern_url(proto);
			field.fieldType = GF_SG_VRML_MFURL;
			field.name = "ExternProto";

			if (codec->info->config.UsePredictiveMFField) {
				flag = gf_bs_read_int(bs, 1);
				assert(!flag);
			}
			/*reserved*/
			gf_bs_read_int(bs, 1);

			/*list or vector*/
			flag = gf_bs_read_int(bs, 1);
			if (flag) {
				e = BD_DecMFFieldList(codec, bs, NULL, &field, GF_FALSE);
			} else {
				e = BD_DecMFFieldVec(codec, bs, NULL, &field, GF_FALSE);
			}
			if (e) goto exit;
		}
		/*get proto code*/
		else {
			//scope command buffer solving by proto nodes - cf poc in #2040
			//this means that any pending command buffer not solved at the end of a proto declaration is
			//a falty one
			GF_List *old_cb = codec->command_buffers;
			codec->command_buffers = gf_list_new();
			
			/*parse sub-proto list - subprotos are ALWAYS registered with parent proto graph*/
			e = gf_bifs_dec_proto_list(codec, bs, NULL);
			if (e) {
				command_buffers_del(codec->command_buffers);
				codec->command_buffers = old_cb;
				goto exit;
			}

			flag = 1;

			while (flag) {
				/*parse all nodes in SFWorldNode table*/
				node = gf_bifs_dec_node(codec, bs, NDT_SFWorldNode);
				if (!node) {
					if (codec->LastError) {
						e = codec->LastError;
						command_buffers_del(codec->command_buffers);
						codec->command_buffers = old_cb;
						goto exit;
					} else {
						flag = gf_bs_read_int(bs, 1);
						continue;
					}
				}
				e = gf_node_register(node, NULL);
				if (e) {
					command_buffers_del(codec->command_buffers);
					codec->command_buffers = old_cb;
					goto exit;
				}
				//Ivica patch - Flush immediately because of proto instantiation
				gf_bifs_flush_command_list(codec);

				gf_sg_proto_add_node_code(proto, node);
				flag = gf_bs_read_int(bs, 1);
			}

			command_buffers_del(codec->command_buffers);
			codec->command_buffers = old_cb;

			/*routes*/
			flag = gf_bs_read_int(bs, 1);
			if (flag) {
				flag = gf_bs_read_int(bs, 1);
				if (flag) {
					/*list route*/
					while (flag) {
						e = gf_bifs_dec_route(codec, bs, GF_FALSE);
						if (e) goto exit;
						flag = gf_bs_read_int(bs, 1);
					}
				} else {
					/*vector*/
					i = gf_bs_read_int(bs, 5);
					NbRoutes = gf_bs_read_int(bs, i);
					for (i=0; i<NbRoutes; i++) {
						e = gf_bifs_dec_route(codec, bs, GF_FALSE);
						if (e) goto exit;
					}
				}
			}
		}

		/*restore the namespace*/
		codec->current_graph = rootSG;

		/*3- parse anim and Quantization stuff*/
		useQuant = gf_bs_read_int(bs, 1);
		useAnim = gf_bs_read_int(bs, 1);

		count = gf_sg_proto_get_field_count(proto);
		for (i=0; i<count; i++) {
			proto_field = gf_sg_proto_field_find(proto, i);
			gf_sg_proto_field_get_field(proto_field, &field);

			/*quant*/
			if (useQuant && ( (field.eventType == GF_SG_EVENT_FIELD) || (field.eventType == GF_SG_EVENT_EXPOSED_FIELD) )) {
				QP_Type = gf_bs_read_int(bs, 4);

				if (QP_Type==QC_LINEAR_SCALAR) {
					NumBits = gf_bs_read_int(bs, 5);
				}
				hasMinMax = gf_bs_read_int(bs, 1);
				qp_min_value = qp_max_value = NULL;
				if (hasMinMax) {
					/*parse min and max*/
					qpsftype = gf_sg_vrml_get_sf_type(field.fieldType);
					switch (qpsftype) {
					case GF_SG_VRML_SFINT32:
					case GF_SG_VRML_SFTIME:
						break;
					/*other fields are of elementary type SFFloat or shouldn't have min/max*/
					default:
						qpsftype = GF_SG_VRML_SFFLOAT;
						break;
					}
					field.fieldType = qpsftype;

					qp_min_value = gf_sg_vrml_field_pointer_new(qpsftype);
					field.name = "QPMinValue";
					field.far_ptr = qp_min_value;
					gf_bifs_dec_sf_field(codec, bs, NULL, &field, GF_FALSE);

					qp_max_value = gf_sg_vrml_field_pointer_new(qpsftype);
					field.name = "QPMaxValue";
					field.far_ptr = qp_max_value;
					gf_bifs_dec_sf_field(codec, bs, NULL, &field, GF_FALSE);
				}

				/*and store*/
				if (QP_Type) {
					e = gf_bifs_proto_field_set_aq_info(proto_field, QP_Type, hasMinMax, qpsftype, qp_min_value, qp_max_value, NumBits);
					gf_sg_vrml_field_pointer_del(qp_min_value, qpsftype);
					gf_sg_vrml_field_pointer_del(qp_max_value, qpsftype);
				}
			}

			/*anim - not supported yet*/
			if (useAnim && ( (field.eventType == GF_SG_EVENT_IN) || (field.eventType == GF_SG_EVENT_EXPOSED_FIELD) )) {
				flag = gf_bs_read_int(bs, 1);
				if (flag) {
					/*Anim_Type = */gf_bs_read_int(bs, 4);
				}
			}
		}

		//numProtos ++;

		/*4- get next proto*/
		flag = gf_bs_read_int(bs, 1);
	}

exit:
	if (e) {
		if (proto) {
			if (proto_list) gf_list_del_item(proto_list, proto);

			//remove all command buffers inserted while decoding the proto list
			u32 new_cmd_bufs = gf_list_count(codec->command_buffers);
			while (nb_cmd_bufs < new_cmd_bufs) {
				new_cmd_bufs--;
				CommandBufferItem *cbi = gf_list_pop_back(codec->command_buffers);
				gf_node_unregister(cbi->node, NULL);
				gf_free(cbi);
			}
			gf_sg_proto_del(proto);
		}
		codec->current_graph = rootSG;
	}
	/*restore original parent proto at codec level*/
	codec->pCurrentProto = ParentProto;
	return e;
}



GF_Err gf_bifs_dec_route(GF_BifsDecoder * codec, GF_BitStream *bs, Bool is_insert)
{
	GF_Err e;
	u8 flag;
	GF_Route *r;
	GF_Node *InNode, *OutNode;
	u32 RouteID, outField, inField, numBits, ind, node_id;
	char name[1000];

	RouteID = 0;

	flag = gf_bs_read_int(bs, 1);
	/*def'ed route*/
	if (flag) {
		RouteID = 1 + gf_bs_read_int(bs, codec->info->config.RouteIDBits);
		if (codec->UseName) gf_bifs_dec_name(bs, name, 1000);
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

	r = gf_sg_route_new(codec->current_graph, OutNode, outField, InNode, inField);
	if (!r) return GF_OUT_OF_MEM;
	if (RouteID) {
		e = gf_sg_route_set_id(r, RouteID);
		if (!e && codec->UseName) e = gf_sg_route_set_name(r, name);
	}
	return e;
}


GF_Err BD_DecSceneReplace(GF_BifsDecoder * codec, GF_BitStream *bs, GF_List *proto_list)
{
	u8 flag;
	u32 i, nbR;
	GF_Err e;
	GF_Node *root;

	/*Reset the existing scene / scene graph, protos and route lists*/
	if (!proto_list) gf_sg_reset(codec->current_graph);

	/*reserved*/
	gf_bs_read_int(bs, 6);

	codec->UseName = (Bool)gf_bs_read_int(bs, 1);
	/*parse PROTOS*/
	e = gf_bifs_dec_proto_list(codec, bs, proto_list);
	if (e) goto exit;

	//cannot have replace scene in a proto
	if (codec->pCurrentProto) return GF_NON_COMPLIANT_BITSTREAM;
	/*Parse the top node - always of type SFTopNode*/
	root = gf_bifs_dec_node(codec, bs, NDT_SFTopNode);
	if (!root && codec->LastError) {
		e = codec->LastError;
		goto exit;
	}

	if (root) {
		e = gf_node_register(root, NULL);
		if (e) goto exit;
	}
	gf_sg_set_root_node(codec->current_graph, root);

	/*Parse the routes*/
	flag = gf_bs_read_int(bs, 1);


	if (flag) {
		flag = gf_bs_read_int(bs, 1);
		if (flag) {
			/*list*/
			while (flag) {
				e = gf_bifs_dec_route(codec, bs, GF_FALSE);
				if (e) goto exit;
				flag = gf_bs_read_int(bs, 1);
			}
		} else {
			/*vector*/
			i = gf_bs_read_int(bs, 5);
			nbR = gf_bs_read_int(bs, i);
			for (i=0; i<nbR; i++) {
				e = gf_bifs_dec_route(codec, bs, GF_FALSE);
				if (e) goto exit;
			}
		}
	}

exit:
	return e;
}

GF_Err gf_bifs_dec_command(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	GF_Err e;
	e = codec->LastError = GF_OK;

	codec->ActiveQP = (M_QuantizationParameter*)codec->scenegraph->global_qp;

	while (1) {
		u8 type = gf_bs_read_int(bs, 2);
		switch (type) {
		case 0:
			e = BD_DecInsert(codec, bs);
			break;
		case 1:
			e = BD_DecDelete(codec, bs);
			break;
		case 2:
			e = BD_DecReplace(codec, bs);
			break;
		case 3:
			e = BD_DecSceneReplace(codec, bs, NULL);
			break;
		}
		if (e) return e;
		if (! gf_bs_read_int(bs, 1)) break;
	}

	while (gf_list_count(codec->QPs)) {
		gf_bifs_dec_qp_remove(codec, GF_TRUE);
	}
	gf_bifs_flush_command_list(codec);

	return GF_OK;
}

#endif /*GPAC_DISABLE_BIFS*/
