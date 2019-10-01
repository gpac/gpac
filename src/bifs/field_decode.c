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
#include "script.h"

#ifndef GPAC_DISABLE_BIFS

void SFCommandBufferChanged(GF_BifsDecoder * codec, GF_Node *node)
{
	void Conditional_BufferReplaced(GF_BifsDecoder * codec, GF_Node *node);

	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Conditional:
		Conditional_BufferReplaced(codec, node);
		break;
	}
}


//startTimes, stopTimes and co are coded as relative to their AU timestamp when received
//on the wire. If from scripts or within proto the offset doesn't apply
void BD_OffsetSFTime(GF_BifsDecoder * codec, Double *time)
{
	if ((!codec->is_com_dec && codec->pCurrentProto) || codec->dec_memory_mode) return;
	*time += codec->cts_offset;
}

void BD_CheckSFTimeOffset(GF_BifsDecoder *codec, GF_Node *node, GF_FieldInfo *inf)
{
	if (gf_node_get_tag(node) != TAG_ProtoNode) {
		if (!stricmp(inf->name, "startTime") || !stricmp(inf->name, "stopTime"))
			BD_OffsetSFTime(codec,  (Double *)inf->far_ptr);
	} else if (gf_sg_proto_field_is_sftime_offset(node, inf)) {
		BD_OffsetSFTime(codec,  (Double *)inf->far_ptr);
	}
}


Fixed BD_ReadSFFloat(GF_BifsDecoder * codec, GF_BitStream *bs)
{
	if (codec->ActiveQP && codec->ActiveQP->useEfficientCoding)
		return gf_bifs_dec_mantissa_float(codec, bs);

	return FLT2FIX(gf_bs_read_float(bs));
}


GF_Err gf_bifs_dec_sf_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com)
{
	GF_Err e;
	GF_Node *new_node;
	u32 size, length, w, h, i;
	char *buffer;

	//blindly call unquantize. return is OK, error or GF_EOS
	if (codec->ActiveQP && node) {
		e = gf_bifs_dec_unquant_field(codec, bs, node, field);
		if (e != GF_EOS) return e;
	}
	//not quantized, use normal scheme
	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
		* ((SFBool *) field->far_ptr) = (SFBool) gf_bs_read_int(bs, 1);
		break;
	case GF_SG_VRML_SFCOLOR:
		((SFColor *)field->far_ptr)->red = BD_ReadSFFloat(codec, bs);
		((SFColor *)field->far_ptr)->green = BD_ReadSFFloat(codec, bs);
		((SFColor *)field->far_ptr)->blue = BD_ReadSFFloat(codec, bs);
		break;
	case GF_SG_VRML_SFFLOAT:
		*((SFFloat *)field->far_ptr) = BD_ReadSFFloat(codec, bs);
		break;
	case GF_SG_VRML_SFINT32:
		*((SFInt32 *)field->far_ptr) = (s32) gf_bs_read_int(bs, 32);
		break;
	case GF_SG_VRML_SFTIME:
		*((SFTime *)field->far_ptr) = gf_bs_read_double(bs);
		if (node) BD_CheckSFTimeOffset(codec, node, field);
		break;
	case GF_SG_VRML_SFVEC2F:
		((SFVec2f *)field->far_ptr)->x = BD_ReadSFFloat(codec, bs);
		((SFVec2f *)field->far_ptr)->y = BD_ReadSFFloat(codec, bs);
		break;
	case GF_SG_VRML_SFVEC3F:
		((SFVec3f *)field->far_ptr)->x = BD_ReadSFFloat(codec, bs);
		((SFVec3f *)field->far_ptr)->y = BD_ReadSFFloat(codec, bs);
		((SFVec3f *)field->far_ptr)->z = BD_ReadSFFloat(codec, bs);
		break;
	case GF_SG_VRML_SFROTATION:
		((SFRotation *)field->far_ptr)->x = BD_ReadSFFloat(codec, bs);
		((SFRotation *)field->far_ptr)->y = BD_ReadSFFloat(codec, bs);
		((SFRotation *)field->far_ptr)->z = BD_ReadSFFloat(codec, bs);
		((SFRotation *)field->far_ptr)->q = BD_ReadSFFloat(codec, bs);
		break;
	case GF_SG_VRML_SFSTRING:
		size = gf_bs_read_int(bs, 5);
		length = gf_bs_read_int(bs, size);
		if (gf_bs_available(bs) < length) return GF_NON_COMPLIANT_BITSTREAM;

		if (node && (node->sgprivate->tag==TAG_MPEG4_CacheTexture) && (field->fieldIndex<=2)) {
			M_CacheTexture *ct = (M_CacheTexture *) node;
			ct->data_len = length;
			if (ct->data) gf_free(ct->data);
			ct->data = (u8*)gf_malloc(sizeof(char)*length);
			gf_bs_read_data(bs, (char*)ct->data, length);
		} else if (node && (node->sgprivate->tag==TAG_MPEG4_BitWrapper) ) {
			M_BitWrapper *bw = (M_BitWrapper*) node;
			if (bw->buffer.buffer) gf_free(bw->buffer.buffer);
			bw->buffer_len = length;
			bw->buffer.buffer = (char*)gf_malloc(sizeof(char)*length);
			gf_bs_read_data(bs, (char*)bw->buffer.buffer, length);
		} else {
			if ( ((SFString *)field->far_ptr)->buffer ) gf_free( ((SFString *)field->far_ptr)->buffer);
			((SFString *)field->far_ptr)->buffer = (char *)gf_malloc(sizeof(char)*(length+1));
			memset(((SFString *)field->far_ptr)->buffer , 0, length+1);
			for (i=0; i<length; i++) {
				((SFString *)field->far_ptr)->buffer[i] = gf_bs_read_int(bs, 8);
			}
		}
		break;
	case GF_SG_VRML_SFURL:
	{
		SFURL *url = (SFURL *) field->far_ptr;
		size = gf_bs_read_int(bs, 1);
		if (size) {
			if (url->url) gf_free(url->url );
			url->url = NULL;
			length = gf_bs_read_int(bs, 10);
			url->OD_ID = length;
		} else {
			if ( url->OD_ID ) url->OD_ID = (u32) -1;
			size = gf_bs_read_int(bs, 5);
			length = gf_bs_read_int(bs, size);
			if (gf_bs_available(bs) < length) return GF_NON_COMPLIANT_BITSTREAM;
			buffer = NULL;
			if (length) {
				buffer = (char *)gf_malloc(sizeof(char)*(length+1));
				memset(buffer, 0, length+1);
				for (i=0; i<length; i++) buffer[i] = gf_bs_read_int(bs, 8);
			}
			if (url->url) gf_free( url->url);
			/*if URL is empty set it to NULL*/
			if (buffer && strlen(buffer)) {
				url->url = buffer;
			} else {
				gf_free(buffer);
				url->url = NULL;
			}
		}
	}
	break;
	case GF_SG_VRML_SFIMAGE:
		if (((SFImage *)field->far_ptr)->pixels) gf_free(((SFImage *)field->far_ptr)->pixels);
		w = gf_bs_read_int(bs, 12);
		h = gf_bs_read_int(bs, 12);
		length = gf_bs_read_int(bs, 2);

		if (length > 3) length = 3;
		length += 1;
		size = w * h * length;
		if (gf_bs_available(bs) < size) return GF_NON_COMPLIANT_BITSTREAM;
		((SFImage *)field->far_ptr)->width = w;
		((SFImage *)field->far_ptr)->height = h;
		((SFImage *)field->far_ptr)->numComponents = length;
		((SFImage *)field->far_ptr)->pixels = (unsigned char *)gf_malloc(sizeof(char)*size);
		//WARNING: Buffers are NOT ALIGNED IN THE BITSTREAM
		for (i=0; i<size; i++) {
			((SFImage *)field->far_ptr)->pixels[i] = gf_bs_read_int(bs, 8);
		}
		break;
	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *sfcb = (SFCommandBuffer *)field->far_ptr;
		if (!node) return GF_BAD_PARAM;
		if (sfcb->buffer) {
			gf_free(sfcb->buffer);
			sfcb->buffer = NULL;
		}
		while (gf_list_count(sfcb->commandList)) {
			GF_Command *com = (GF_Command*)gf_list_get(sfcb->commandList, 0);
			gf_list_rem(sfcb->commandList, 0);
			gf_sg_command_del(com);
		}

		size = gf_bs_read_int(bs, 5);
		length = gf_bs_read_int(bs, size);
		if (gf_bs_available(bs) < length) return GF_NON_COMPLIANT_BITSTREAM;

		sfcb->bufferSize = length;
		if (length) {
			sfcb->buffer = (unsigned char *)gf_malloc(sizeof(char)*(length));
			//WARNING Buffers are NOT ALIGNED IN THE BITSTREAM
			for (i=0; i<length; i++) {
				sfcb->buffer[i] = gf_bs_read_int(bs, 8);
			}
		}
		//notify the node - this is needed in case an enhencement layer replaces the buffer, in which case
		//the # ID Bits may change
		SFCommandBufferChanged(codec, node);

		/*
		 1 - memory mode, register command buffer for later parsing
		 2 - InputSensor only works on decompressed commands
		*/
		if (codec->dec_memory_mode || (node->sgprivate->tag==TAG_MPEG4_InputSensor)) {
			CommandBufferItem *cbi = (CommandBufferItem *)gf_malloc(sizeof(CommandBufferItem));
			cbi->node = node;
			cbi->cb = sfcb;
			gf_list_add(codec->command_buffers, cbi);
		}
	}
	break;
	case GF_SG_VRML_SFNODE:
		//for nodes the field ptr is a ptr to the field, which is a node ptr ;)
		new_node = gf_bifs_dec_node(codec, bs, field->NDTtype);
		if (new_node) {
			e = gf_node_register(new_node, is_mem_com ? NULL : node);
			if (e) return e;
		}
		//it may happen that new_node is NULL (this is valid for a proto declaration)
		*((GF_Node **) field->far_ptr) = new_node;
		break;
	case GF_SG_VRML_SFSCRIPT:
#ifdef GPAC_HAS_QJS
		codec->LastError = SFScript_Parse(codec, (SFScript*)field->far_ptr, bs, node);
#else
		return GF_NOT_SUPPORTED;
#endif
		break;
	case GF_SG_VRML_SFATTRREF:
	{
		SFAttrRef *ar = (SFAttrRef *)field->far_ptr;
		u32 nodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		ar->node = gf_sg_find_node(codec->current_graph, nodeID);
		if (!ar->node) {

		} else {
			u32 nbBitsDEF = gf_get_bit_size(gf_node_get_num_fields_in_mode(ar->node, GF_SG_FIELD_CODING_DEF) - 1);
			u32 field_ref = gf_bs_read_int(bs, nbBitsDEF);
			codec->LastError = gf_bifs_get_field_index(ar->node, field_ref, GF_SG_FIELD_CODING_DEF, &ar->fieldIndex);
		}
	}
	break;
	default:
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	return codec->LastError;
}

GF_Err BD_DecMFFieldList(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com)
{
	GF_Node *new_node;
	GF_Err e;
	u8 endFlag, qp_local, qp_on, initial_qp;
	GF_ChildNodeItem *last = NULL;
	u32 nbF;

	GF_FieldInfo sffield;

	memset(&sffield, 0, sizeof(GF_FieldInfo));
	sffield.fieldIndex = field->fieldIndex;
	sffield.fieldType = gf_sg_vrml_get_sf_type(field->fieldType);
	sffield.NDTtype = field->NDTtype;

	nbF = 0;
	qp_on = qp_local = 0;
	initial_qp = codec->ActiveQP ? 1 : 0;

	endFlag = gf_bs_read_int(bs, 1);
	while (!endFlag  && (codec->LastError>=0)) {
		if (field->fieldType != GF_SG_VRML_MFNODE) {
			e = gf_sg_vrml_mf_append(field->far_ptr, field->fieldType, & sffield.far_ptr);
			if (e) return e;
			e = gf_bifs_dec_sf_field(codec, bs, node, &sffield, GF_FALSE);
			if (e) return e;
		} else {
			new_node = gf_bifs_dec_node(codec, bs, field->NDTtype);
			//append
			if (new_node) {
				e = gf_node_register(new_node, is_mem_com ? NULL : node);
				if (e) return e;

				//regular coding
				if (node) {
					//special case for QP, register as the current QP
					if (gf_node_get_tag(new_node) == TAG_MPEG4_QuantizationParameter) {
						qp_local = ((M_QuantizationParameter *)new_node)->isLocal;
						//we have a QP in the same scope, remove previous
						if (qp_on) gf_bifs_dec_qp_remove(codec, GF_FALSE);
						e = gf_bifs_dec_qp_set(codec, new_node);
						if (e) return e;
						qp_on = 1;
						if (qp_local) qp_local = 2;
						if (codec->force_keep_qp) {
							e = gf_node_list_add_child_last(field->far_ptr, new_node, &last);
						} else {
							gf_node_register(new_node, NULL);
							gf_node_unregister(new_node, node);
						}
					} else
						//this is generic MFNode container
						e = gf_node_list_add_child_last(field->far_ptr, new_node, &last);

				}
				//proto coding: directly add the child
				else if (codec->pCurrentProto) {
					//TO DO: what happens if this is a QP node on the interface ?
					e = gf_node_list_add_child_last( (GF_ChildNodeItem **)field->far_ptr, new_node, &last);
				}
			} else {
				return codec->LastError;
			}
		}
		if (e) return e;

		endFlag = gf_bs_read_int(bs, 1);

		//according to the spec, the QP applies to the current node itself,
		//not just children. If IsLocal is TRUE remove the node
		if (qp_on && qp_local) {
			if (qp_local == 2) {
				qp_local = 1;
			} else {
				//ask to get rid of QP and reactivate if we had a QP when entering
				gf_bifs_dec_qp_remove(codec, initial_qp);
				qp_local = 0;
				qp_on = 0;
			}
		}
		nbF += 1;
	}
	/*finally delete the QP if any (local or not) as we get out of this node
	and reactivate previous one*/
	if (qp_on) gf_bifs_dec_qp_remove(codec, initial_qp);
	/*this is for QP 14*/
	gf_bifs_dec_qp14_set_length(codec, nbF);
	return GF_OK;
}

GF_Err BD_DecMFFieldVec(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com)
{
	GF_Err e;
	u32 NbBits, nbFields;
	u32 i;
	GF_ChildNodeItem *last;
	u8 qp_local, qp_on, initial_qp;
	GF_FieldInfo sffield;

	memset(&sffield, 0, sizeof(GF_FieldInfo));
	sffield.fieldIndex = field->fieldIndex;
	sffield.fieldType = gf_sg_vrml_get_sf_type(field->fieldType);
	sffield.NDTtype = field->NDTtype;

	initial_qp = qp_local = qp_on = 0;

	//vector description - alloc the MF size before
	NbBits = gf_bs_read_int(bs, 5);
	nbFields = gf_bs_read_int(bs, NbBits);

	if (codec->ActiveQP) {
		initial_qp = 1;
		/*this is for QP 14*/
		gf_bifs_dec_qp14_set_length(codec, nbFields);
	}

	if (field->fieldType != GF_SG_VRML_MFNODE) {
		e = gf_sg_vrml_mf_alloc(field->far_ptr, field->fieldType, nbFields);
		if (e) return e;

		for (i=0; i<nbFields; i++) {
			e = gf_sg_vrml_mf_get_item(field->far_ptr, field->fieldType, & sffield.far_ptr, i);
			if (e) return e;
			e = gf_bifs_dec_sf_field(codec, bs, node, &sffield, GF_FALSE);
			if (e) return e;
		}
	} else {
		last = NULL;
		for (i=0; i<nbFields; i++) {
			GF_Node *new_node = gf_bifs_dec_node(codec, bs, field->NDTtype);
			if (new_node) {
				e = gf_node_register(new_node, is_mem_com ? NULL : node);
				if (e) return e;

				if (node) {
					/*special case for QP, register as the current QP*/
					if (gf_node_get_tag(new_node) == TAG_MPEG4_QuantizationParameter) {
						qp_local = ((M_QuantizationParameter *)new_node)->isLocal;
						/*we have a QP in the same scope, remove previous
						NB: we assume this is the right behaviour, the spec doesn't say
						whether QP is cumulative or not*/
						if (qp_on) gf_bifs_dec_qp_remove(codec, GF_FALSE);

						e = gf_bifs_dec_qp_set(codec, new_node);
						if (e) return e;
						qp_on = 1;
						if (qp_local) qp_local = 2;
						if (codec->force_keep_qp) {
							e = gf_node_list_add_child_last(field->far_ptr, new_node, &last);
							if (e) return e;
						} else {
							gf_node_register(new_node, NULL);
							gf_node_unregister(new_node, node);
						}
					} else {
						e = gf_node_list_add_child_last(field->far_ptr, new_node, &last);
						if (e) return e;
					}
				}
				/*proto coding*/
				else if (codec->pCurrentProto) {
					/*TO DO: what happens if this is a QP node on the interface ?*/
					e = gf_node_list_add_child_last( (GF_ChildNodeItem **)field->far_ptr, new_node, &last);
					if (e) return e;
				}
			} else {
				return codec->LastError ? codec->LastError : GF_NON_COMPLIANT_BITSTREAM;
			}
		}
		/*according to the spec, the QP applies to the current node itself, not just children.
		If IsLocal is TRUE remove the node*/
		if (qp_on && qp_local) {
			if (qp_local == 2) {
//				qp_local = 1;
			} else {
				//ask to get rid of QP and reactivate if we had a QP when entering the node
				gf_bifs_dec_qp_remove(codec, initial_qp);
//				qp_local = 0;
			}
		}
	}
	/*finally delete the QP if any (local or not) as we get out of this node*/
	if (qp_on) gf_bifs_dec_qp_remove(codec, GF_TRUE);
	return GF_OK;
}


void gf_bifs_check_field_change(GF_Node *node, GF_FieldInfo *field)
{
	if (field->fieldType==GF_SG_VRML_MFNODE) node->sgprivate->flags |= GF_SG_CHILD_DIRTY;
	/*signal node modif*/
	gf_node_changed(node, field);
	/*Notify eventOut in all cases to handle protos*/
	gf_node_event_out(node, field->fieldIndex);
	/*and propagate eventIn if any*/
	if (field->on_event_in) {
		field->on_event_in(node, NULL);
	} else if ((gf_node_get_tag(node) == TAG_MPEG4_Script) && (field->eventType==GF_SG_EVENT_IN)) {
		gf_sg_script_event_in(node, field);
	}

}

GF_Err gf_bifs_dec_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field, Bool is_mem_com)
{
	GF_Err e;
	u8 flag;

//	if (codec->LastError) return codec->LastError;

	assert(node);
//	if (field->fieldType == GF_SG_VRML_UNKNOWN) return GF_NON_COMPLIANT_BITSTREAM;

	if (gf_sg_vrml_is_sf_field(field->fieldType)) {
		e = gf_bifs_dec_sf_field(codec, bs, node, field, is_mem_com);
		if (e) return e;
	} else {
		/*clean up the eventIn field if not done*/
		if (field->eventType == GF_SG_EVENT_IN) {
			if (field->fieldType == GF_SG_VRML_MFNODE) {
				gf_node_unregister_children(node, * (GF_ChildNodeItem **)field->far_ptr);
				* (GF_ChildNodeItem **)field->far_ptr = NULL;
			} else {
				//remove all items of the MFField
				gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
			}
		}

		/*predictiveMFField*/
		if (codec->info->config.UsePredictiveMFField) {
			flag = gf_bs_read_int(bs, 1);
			if (flag) {
#ifdef GPAC_ENABLE_BIFS_PMF
				return gf_bifs_dec_pred_mf_field(codec, bs, node, field);
#else
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[BIFS] Stream uses Predictive Field Coding, disabled in this build!\n"));
				return GF_NOT_SUPPORTED;
#endif
			}
		}

		/*reserved*/
		flag = gf_bs_read_int(bs, 1);
		if (!flag) {
			/*destroy the field content...*/
			if (field->fieldType != GF_SG_VRML_MFNODE) {
				e = gf_sg_vrml_mf_reset(field->far_ptr, field->fieldType);
				if (e) return e;
			}
			/*List description - alloc is dynamic*/
			flag = gf_bs_read_int(bs, 1);
			if (flag) {
				e = BD_DecMFFieldList(codec, bs, node, field, is_mem_com);
			} else {
				e = BD_DecMFFieldVec(codec, bs, node, field, is_mem_com);
			}
			if (e) return e;
		}
	}
	return GF_OK;
}


GF_Err BD_SetProtoISed(GF_BifsDecoder * codec, u32 protofield, GF_Node *n, u32 nodefield)
{
	/*take care of conditional execution in proto*/
	if (codec->current_graph->pOwningProto) {
		return gf_sg_proto_instance_set_ised((GF_Node *) codec->current_graph->pOwningProto, protofield, n, nodefield);
	}
	/*regular ISed fields*/
	else {
		return gf_sg_proto_field_set_ised(codec->pCurrentProto, protofield, n, nodefield);
	}
}

GF_Err gf_bifs_dec_node_list(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, Bool is_proto)
{
	u8 flag;
	GF_Err e;
	u32 numBitsALL, numBitsDEF, field_all, field_ref, numProtoBits;
	GF_FieldInfo field;

	numProtoBits = numBitsALL = 0;
	if (codec->pCurrentProto) {
		numProtoBits = gf_get_bit_size(gf_sg_proto_get_field_count(codec->pCurrentProto) - 1);
		numBitsALL = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL)-1);
	}
	numBitsDEF = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF)-1);

	flag = gf_bs_read_int(bs, 1);
	while (!flag && (codec->LastError>=0)) {
		if (codec->pCurrentProto) {
			//IS'ed flag
			flag = gf_bs_read_int(bs, 1);
			if (flag) {
				//get field index in ALL mode for node
				field_ref = gf_bs_read_int(bs, numBitsALL);
				//get field index in ALL mode for proto
				field_all = gf_bs_read_int(bs, numProtoBits);
				e = gf_node_get_field(node, field_ref, &field);
				if (e) return e;
				e = BD_SetProtoISed(codec, field_all, node, field_ref);
				if (e) return e;
				flag = gf_bs_read_int(bs, 1);
				continue;
			}
		}

		//fields are coded in DEF mode
		field_ref = gf_bs_read_int(bs, numBitsDEF);
		e = gf_bifs_get_field_index(node, field_ref, GF_SG_FIELD_CODING_DEF, &field_all);
		if (e) return e;
		e = gf_node_get_field(node, field_all, &field);
		if (e) return e;
		e = gf_bifs_dec_field(codec, bs, node, &field, GF_FALSE);
		if (e) return e;
		flag = gf_bs_read_int(bs, 1);

		if (is_proto) gf_sg_proto_mark_field_loaded(node, &field);
	}
	return codec->LastError;
}

GF_Err gf_bifs_dec_node_mask(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, Bool is_proto)
{
	u32 i, numFields, numProtoFields, index, flag, nbBits;
	GF_Err e;
	GF_FieldInfo field;

	//proto coding
	if (codec->pCurrentProto) {
		numFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
		numProtoFields = gf_sg_proto_get_field_count(codec->pCurrentProto);
		nbBits = gf_get_bit_size(numProtoFields-1);

		for (i=0; i<numFields; i++) {
			flag = gf_bs_read_int(bs, 1);
			if (!flag) continue;
			flag = gf_bs_read_int(bs, 1);
			//IS'ed field, create route for binding to Proto declaration
			if (flag) {
				//reference index of our IS'ed proto field
				flag = gf_bs_read_int(bs, nbBits);
				e = gf_node_get_field(node, i, &field);
				if (e) return e;
				e = BD_SetProtoISed(codec, flag, node, i);
			}
			//regular field, parse it (nb: no contextual coding for protos in maskNode,
			//all node fields are coded
			else {
				e = gf_node_get_field(node, i, &field);
				if (e) return e;
				e = gf_bifs_dec_field(codec, bs, node, &field, GF_FALSE);
			}
			if (e) return e;
		}
	}
	//regular coding
	else {
		numFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF);
		for (i=0; i<numFields; i++) {
			flag = gf_bs_read_int(bs, 1);
			if (!flag) continue;
			gf_bifs_get_field_index(node, i, GF_SG_FIELD_CODING_DEF, &index);
			e = gf_node_get_field(node, index, &field);
			if (e) return e;
			e = gf_bifs_dec_field(codec, bs, node, &field, GF_FALSE);
			if (e) return e;

			if (is_proto) gf_sg_proto_mark_field_loaded(node, &field);
		}
	}
	return GF_OK;
}


static void UpdateTimeNode(GF_BifsDecoder * codec, GF_Node *node)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_AnimationStream:
		BD_OffsetSFTime(codec, & ((M_AnimationStream*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_AnimationStream*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioBuffer:
		BD_OffsetSFTime(codec, & ((M_AudioBuffer*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_AudioBuffer*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioClip:
		BD_OffsetSFTime(codec, & ((M_AudioClip*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_AudioClip*)node)->stopTime);
		break;
	case TAG_MPEG4_AudioSource:
		BD_OffsetSFTime(codec, & ((M_AudioSource*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_AudioSource*)node)->stopTime);
		break;
	case TAG_MPEG4_MovieTexture:
		BD_OffsetSFTime(codec, & ((M_MovieTexture*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_MovieTexture*)node)->stopTime);
		break;
	case TAG_MPEG4_TimeSensor:
		BD_OffsetSFTime(codec, & ((M_TimeSensor*)node)->startTime);
		BD_OffsetSFTime(codec, & ((M_TimeSensor*)node)->stopTime);
		break;
	case TAG_ProtoNode:
	{
		u32 i, nbFields;
		GF_FieldInfo inf;
		nbFields = gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL);
		for (i=0; i<nbFields; i++) {
			gf_node_get_field(node, i, &inf);
			if (inf.fieldType != GF_SG_VRML_SFTIME) continue;
			BD_CheckSFTimeOffset(codec, node, &inf);
		}
	}
	break;
	}
}

GF_Node *gf_bifs_dec_node(GF_BifsDecoder * codec, GF_BitStream *bs, u32 NDT_Tag)
{
	u32 nodeID, NDTBits, node_type, node_tag, ProtoID, BVersion;
	Bool skip_init, reset_qp14;
	GF_Node *new_node;
	GF_Err e;
	GF_Proto *proto;
	void SetupConditional(GF_BifsDecoder *codec, GF_Node *node);

	//to store the UseName
	char name[1000];

#if 0
	/*should only happen with inputSensor, in which case this is BAAAAD*/
	if (!codec->info) {
		codec->LastError = GF_BAD_PARAM;
		return NULL;
	}
#endif


	BVersion = GF_BIFS_V1;

	/*this is a USE statement*/
	if (gf_bs_read_int(bs, 1)) {
		nodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		/*NULL node is encoded as USE with ID = all bits to 1*/
		if (nodeID == (u32) (1<<codec->info->config.NodeIDBits))
			return NULL;
		//find node and return it
		new_node = gf_sg_find_node(codec->current_graph, nodeID);

		if (!new_node) {
			codec->LastError = GF_SG_UNKNOWN_NODE;
		} else {
			/*restore QP14 length*/
			switch (gf_node_get_tag(new_node)) {
			case TAG_MPEG4_Coordinate:
			{
				u32 nbCoord = ((M_Coordinate *)new_node)->point.count;
				gf_bifs_dec_qp14_enter(codec, GF_TRUE);
				gf_bifs_dec_qp14_set_length(codec, nbCoord);
				gf_bifs_dec_qp14_enter(codec, GF_FALSE);
			}
			break;
			case TAG_MPEG4_Coordinate2D:
			{
				u32 nbCoord = ((M_Coordinate2D *)new_node)->point.count;
				gf_bifs_dec_qp14_enter(codec, GF_TRUE);
				gf_bifs_dec_qp14_set_length(codec, nbCoord);
				gf_bifs_dec_qp14_enter(codec, GF_FALSE);
			}
			break;
			}
		}
		return new_node;
	}

	//this is a new node
	nodeID = 0;
	name[0] = 0;
	node_tag = 0;
	proto = NULL;

	//browse all node groups
	while (1) {
		NDTBits = gf_bifs_get_ndt_bits(NDT_Tag, BVersion);
		/*this happens in replacescene where no top-level node is present (externProto)*/
		if ((BVersion==1) && (NDTBits > 8 * gf_bs_available(bs)) ) {
			codec->LastError = GF_OK;
			return NULL;
		}

		node_type = gf_bs_read_int(bs, NDTBits);
		if (node_type) break;

		//increment BIFS version
		BVersion += 1;
		//not supported
		if (BVersion > GF_BIFS_NUM_VERSION) {
			codec->LastError = GF_BIFS_UNKNOWN_VERSION;
			return NULL;
		}
	}
	if (BVersion==2 && node_type==1) {
		ProtoID = gf_bs_read_int(bs, codec->info->config.ProtoIDBits);
		/*look in current graph for the proto - this may be a proto graph*/
		proto = gf_sg_find_proto(codec->current_graph, ProtoID, NULL);
		/*this was in proto so look in main scene*/
		if (!proto && codec->current_graph != codec->scenegraph)
			proto = gf_sg_find_proto(codec->scenegraph, ProtoID, NULL);

		if (!proto) {
			codec->LastError = GF_SG_UNKNOWN_NODE;
			return NULL;
		}
	} else {
		node_tag = gf_bifs_ndt_get_node_type(NDT_Tag, node_type, BVersion);
	}

	/*special handling of 3D mesh*/
	if ((node_tag == TAG_MPEG4_IndexedFaceSet) && codec->info->config.Use3DMeshCoding) {
		if (gf_bs_read_int(bs, 1)) {
			/*nodeID = 1 + */gf_bs_read_int(bs, codec->info->config.NodeIDBits);
			if (codec->UseName) gf_bifs_dec_name(bs, name);
		}
		/*parse the 3DMesh node*/
		return NULL;
	}
	/*unknown node*/
	if (!node_tag && !proto) {
		codec->LastError = GF_SG_UNKNOWN_NODE;
		return NULL;
	}


	/*DEF'd flag*/
	if (gf_bs_read_int(bs, 1)) {
		if (!codec->info->config.NodeIDBits) {
			codec->LastError = GF_NON_COMPLIANT_BITSTREAM;
			return NULL;
		}
		nodeID = 1 + gf_bs_read_int(bs, codec->info->config.NodeIDBits);
		if (codec->UseName) gf_bifs_dec_name(bs, name);
	}

	new_node = NULL;
	skip_init = GF_FALSE;

	/*don't check node IDs duplicate since VRML may use them...*/
#if 0
	/*if a node with same DEF is already in the scene, use it
	we don't do that in memory mode because commands may force replacement
	of a node with a new node with same ID, and we want to be able to dump it (otherwise we would
	dump a USE)*/
	if (nodeID && !codec->dec_memory_mode) {
		new_node = gf_sg_find_node(codec->current_graph, nodeID);
		if (new_node) {
			if (proto) {
				if ((gf_node_get_tag(new_node) != TAG_ProtoNode) || (gf_node_get_proto(new_node) != proto)) {
					codec->LastError = GF_NON_COMPLIANT_BITSTREAM;
					return NULL;
				}
				skip_init = 1;
			} else {
				if (gf_node_get_tag(new_node) != node_tag) {
					codec->LastError = GF_NON_COMPLIANT_BITSTREAM;
					return NULL;
				}
				skip_init = 1;
			}
		}
	}
	if (!new_node)
#endif

	{
		if (proto) {
			/*create proto interface*/
			new_node = gf_sg_proto_create_instance(codec->current_graph, proto);
            //don't init protos unless externProto (in which case we want init for hardcoded protos)
            if (! proto->ExternProto.count) skip_init = GF_TRUE;
		} else {
			new_node = gf_node_new(codec->current_graph, node_tag);
		}
	}
	if (!new_node) {
		codec->LastError = GF_NOT_SUPPORTED;
		return NULL;
	}

	/*VRML: "The transformation hierarchy shall be a directed acyclic graph; results are undefined if a node
	in the transformation hierarchy is its own ancestor"
	that's good, because the scene graph can't handle cyclic graphs (destroy will never be called).
	We therefore only register the node once parsed*/
	if (nodeID) {
		if (strlen(name)) {
			gf_node_set_id(new_node, nodeID, name);
		} else {
			gf_node_set_id(new_node, nodeID, NULL);
		}
	}


	/*update default time fields except in proto parsing*/
	if (!codec->pCurrentProto) UpdateTimeNode(codec, new_node);
	/*nodes are only init outside protos, nodes internal to protos are never intialized */
	else skip_init = GF_TRUE;

	/*if coords were not stored for QP14 before coding this node, reset QP14 it when leaving*/
	reset_qp14 = !codec->coord_stored;

	/*QP 14 is a special quant mode for IndexFace/Line(2D)Set to quantize the
	coordonate(2D) child, based on the first field parsed
	we must check the type of the node and notfy the QP*/
	switch (node_tag) {
	case TAG_MPEG4_Coordinate:
	case TAG_MPEG4_Coordinate2D:
		gf_bifs_dec_qp14_enter(codec, GF_TRUE);
	}

	if (gf_bs_read_int(bs, 1)) {
		e = gf_bifs_dec_node_mask(codec, bs, new_node, proto ? GF_TRUE : GF_FALSE);
	} else {
		e = gf_bifs_dec_node_list(codec, bs, new_node, proto ? GF_TRUE : GF_FALSE);
	}
	if (codec->coord_stored && reset_qp14)
		gf_bifs_dec_qp14_reset(codec);

	if (e) {
		codec->LastError = e;
		/*register*/
		gf_node_register(new_node, NULL);
		/*unregister (deletes)*/
		gf_node_unregister(new_node, NULL);
		return NULL;
	}

	if (!skip_init)
		gf_node_init(new_node);

	switch (node_tag) {
	case TAG_MPEG4_Coordinate:
	case TAG_MPEG4_Coordinate2D:
		gf_bifs_dec_qp14_enter(codec, GF_FALSE);
		break;
	case TAG_MPEG4_Script:
		/*load script if in main graph (useless to load in proto declaration)*/
		if (codec->scenegraph == codec->current_graph) {
			gf_sg_script_load(new_node);
		}
		break;
	/*conditionals must be init*/
	case TAG_MPEG4_Conditional:
		SetupConditional(codec, new_node);
		break;
	}

	/*proto is initialized upon the first traversal to have the same behavior as wth BT/XMT loading*/
#if 0
	/*if new node is a proto and we're in the top scene, load proto code*/
	if (proto && (codec->scenegraph == codec->current_graph)) {
		codec->LastError = gf_sg_proto_load_code(new_node);
	}
#endif

	return new_node;
}

#endif /*GPAC_DISABLE_BIFS*/
