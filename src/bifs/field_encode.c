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
#include <gpac/internal/bifs_tables.h>
#include <gpac/network.h>
#include "quant.h"
#include "script.h"

#ifndef GPAC_DISABLE_BIFS_ENC

GF_Err gf_bifs_field_index_by_mode(GF_Node *node, u32 all_ind, u8 indexMode, u32 *outField)
{
	GF_Err e;
	u32 i, count, temp;
	count = gf_node_get_num_fields_in_mode(node, indexMode);
	for (i=0; i<count; i++) {
		e = gf_bifs_get_field_index(node, i, indexMode, &temp);
		if (e) return e;
		if (temp==all_ind) {
			*outField = i;
			return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}


void BE_WriteSFFloat(GF_BifsEncoder *codec, Fixed val, GF_BitStream *bs, char *com)
{
	if (codec->ActiveQP && codec->ActiveQP->useEfficientCoding) {
		gf_bifs_enc_mantissa_float(codec, val, bs);
	} else {
		gf_bs_write_float(bs, FIX2FLT(val));
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] SFFloat\t\t32\t\t%g\t\t%s\n", FIX2FLT(val), com ? com : "") );
	}
}


GF_Err gf_bifs_enc_sf_field(GF_BifsEncoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
{
	GF_Err e;

	if (node) {
		e = gf_bifs_enc_quant_field(codec, bs, node, field);
		if (e != GF_EOS) return e;
	}
	switch (field->fieldType) {
	case GF_SG_VRML_SFBOOL:
		GF_BIFS_WRITE_INT(codec, bs, * ((SFBool *)field->far_ptr), 1, "SFBool", NULL);
		break;
	case GF_SG_VRML_SFCOLOR:
		BE_WriteSFFloat(codec, ((SFColor *)field->far_ptr)->red, bs, "color.red");
		BE_WriteSFFloat(codec, ((SFColor *)field->far_ptr)->green, bs, "color.green");
		BE_WriteSFFloat(codec, ((SFColor *)field->far_ptr)->blue, bs, "color.blue");
		break;
	case GF_SG_VRML_SFFLOAT:
		BE_WriteSFFloat(codec, * ((SFFloat *)field->far_ptr), bs, NULL);
		break;
	case GF_SG_VRML_SFINT32:
		GF_BIFS_WRITE_INT(codec, bs, * ((SFInt32 *)field->far_ptr), 32, "SFInt32", NULL);
		break;
	case GF_SG_VRML_SFROTATION:
		BE_WriteSFFloat(codec, ((SFRotation  *)field->far_ptr)->x, bs, "rot.x");
		BE_WriteSFFloat(codec, ((SFRotation  *)field->far_ptr)->y, bs, "rot.y");
		BE_WriteSFFloat(codec, ((SFRotation  *)field->far_ptr)->z, bs, "rot.z");
		BE_WriteSFFloat(codec, ((SFRotation  *)field->far_ptr)->q, bs, "rot.theta");
		break;

	case GF_SG_VRML_SFSTRING:
		if (node && (node->sgprivate->tag==TAG_MPEG4_CacheTexture) && (field->fieldIndex<=2)) {
			u32 size, val;
			char buf[4096];
			char *res_src = NULL;
			const char *src = ((SFString*)field->far_ptr)->buffer;
			FILE *f;
			if (codec->src_url) res_src = gf_url_concatenate(codec->src_url, src);

			f = gf_fopen(res_src ? res_src : src, "rb");
			if (!f) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Cannot open source file %s for encoding CacheTexture\n", res_src ? res_src : src));
				return GF_URL_ERROR;
			}
			if (res_src) gf_free(res_src);
			gf_fseek(f, 0, SEEK_END);
			size = (u32) gf_ftell(f);
			val = gf_get_bit_size(size);
			GF_BIFS_WRITE_INT(codec, bs, val, 5, "nbBits", NULL);
			GF_BIFS_WRITE_INT(codec, bs, size, val, "length", NULL);
			gf_fseek(f, 0, SEEK_SET);
			while (size) {
				u32 read = (u32) fread(buf, 1, 4096, f);
				gf_bs_write_data(bs, buf, read);
				size -= read;
			}
			gf_fclose(f);
		} else {
			u32 i, val, len;
			char *str = (char *) ((SFString*)field->far_ptr)->buffer;
			if (node && (node->sgprivate->tag==TAG_MPEG4_BitWrapper) ) {
				len = ((M_BitWrapper*)node)->buffer_len;
			} else {
				len = str ? (u32) strlen(str) : 0;
			}
			val = gf_get_bit_size(len);
			GF_BIFS_WRITE_INT(codec, bs, val, 5, "nbBits", NULL);
			GF_BIFS_WRITE_INT(codec, bs, len, val, "length", NULL);
			for (i=0; i<len; i++) gf_bs_write_int(bs, str[i], 8);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] string\t\t%d\t\t%s\n", 8*len, str) );
		}
		break;

	case GF_SG_VRML_SFTIME:
		gf_bs_write_double(bs, *((SFTime *)field->far_ptr));
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] SFTime\t\t%d\t\t%g\n", 64, *((SFTime *)field->far_ptr)));
		break;

	case GF_SG_VRML_SFVEC2F:
		BE_WriteSFFloat(codec, ((SFVec2f *)field->far_ptr)->x, bs, "vec2f.x");
		BE_WriteSFFloat(codec, ((SFVec2f *)field->far_ptr)->y, bs, "vec2f.y");
		break;

	case GF_SG_VRML_SFVEC3F:
		BE_WriteSFFloat(codec, ((SFVec3f *)field->far_ptr)->x, bs, "vec3f.x");
		BE_WriteSFFloat(codec, ((SFVec3f *)field->far_ptr)->y, bs, "vec3f.y");
		BE_WriteSFFloat(codec, ((SFVec3f *)field->far_ptr)->z, bs, "vec3f.z");
		break;

	case GF_SG_VRML_SFURL:
	{
		SFURL *url = (SFURL *) field->far_ptr;
		GF_BIFS_WRITE_INT(codec, bs, (url->OD_ID>0) ? 1 : 0, 1, "hasODID", "SFURL");
		if (url->OD_ID>0) {
			GF_BIFS_WRITE_INT(codec, bs, url->OD_ID, 10, "ODID", "SFURL");
		} else {
			u32 i, len = url->url ? (u32) strlen(url->url) : 0;
			u32 val = gf_get_bit_size(len);
			GF_BIFS_WRITE_INT(codec, bs, val, 5, "nbBits", NULL);
			GF_BIFS_WRITE_INT(codec, bs, len, val, "length", NULL);
			for (i=0; i<len; i++) gf_bs_write_int(bs, url->url[i], 8);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] string\t\t%d\t\t%s\t\t//SFURL\n", 8*len, url->url));
		}
	}
	break;
	case GF_SG_VRML_SFIMAGE:
	{
		u32 size, i;
		SFImage *img = (SFImage *)field->far_ptr;
		GF_BIFS_WRITE_INT(codec, bs, img->width, 12, "width", "SFImage");
		GF_BIFS_WRITE_INT(codec, bs, img->height, 12, "height", "SFImage");
		GF_BIFS_WRITE_INT(codec, bs, img->numComponents - 1, 2, "nbComp", "SFImage");
		size = img->width * img->height * img->numComponents;
		for (i=0; i<size; i++) gf_bs_write_int(bs, img->pixels[i], 8);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] pixels\t\t%d\t\tnot dumped\t\t//SFImage\n", 8*size));
	}
	break;

	case GF_SG_VRML_SFCOMMANDBUFFER:
	{
		SFCommandBuffer *cb = (SFCommandBuffer *) field->far_ptr;
		if (cb->buffer) gf_free(cb->buffer);
		cb->buffer = NULL;
		cb->bufferSize = 0;
		if (gf_list_count(cb->commandList)) {
			u32 i, nbBits;
			GF_BitStream *bs_cond = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] /*SFCommandBuffer*/\n" ));
			e = gf_bifs_enc_commands(codec, cb->commandList, bs_cond);
			if (!e) gf_bs_get_content(bs_cond, &cb->buffer, &cb->bufferSize);
			gf_bs_del(bs_cond);
			if (e) return e;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODING, ("[BIFS] /*End SFCommandBuffer*/\n"));
			nbBits = gf_get_bit_size(cb->bufferSize);
			GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "NbBits", NULL);
			GF_BIFS_WRITE_INT(codec, bs, cb->bufferSize, nbBits, "BufferSize", NULL);
			for (i=0; i<cb->bufferSize; i++) GF_BIFS_WRITE_INT(codec, bs, cb->buffer[i], 8, "buffer byte", NULL);
		}
		/*empty command buffer*/
		else {
			GF_BIFS_WRITE_INT(codec, bs, 0, 5, "NbBits", NULL);
		}
	}
	break;

	case GF_SG_VRML_SFNODE:
		return gf_bifs_enc_node(codec, *((GF_Node **)field->far_ptr), field->NDTtype, bs, node);

	case GF_SG_VRML_SFSCRIPT:
#ifdef GPAC_HAS_SPIDERMONKEY
		codec->LastError = SFScript_Encode(codec, (SFScript *)field->far_ptr, bs, node);
#else
		return GF_NOT_SUPPORTED;
#endif
		break;
	case GF_SG_VRML_SFATTRREF:
	{
		u32 idx=0;
		SFAttrRef *ar = (SFAttrRef *)field->far_ptr;
		u32 nbBitsDEF = gf_get_bit_size(gf_node_get_num_fields_in_mode(ar->node, GF_SG_FIELD_CODING_DEF) - 1);
		GF_BIFS_WRITE_INT(codec, bs, gf_node_get_id(ar->node) - 1, codec->info->config.NodeIDBits, "NodeID", NULL);

		gf_bifs_field_index_by_mode(ar->node, ar->fieldIndex, GF_SG_FIELD_CODING_DEF, &idx);
		GF_BIFS_WRITE_INT(codec, bs, idx, nbBitsDEF, "field", NULL);
	}
	break;
	default:
		return GF_NOT_SUPPORTED;
	}
	return codec->LastError;
}


GF_Err gf_bifs_enc_mf_field(GF_BifsEncoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
{
	GF_ChildNodeItem *list = NULL;
	GF_Err e;
	u32 nbBits, qp_local;
	Bool use_list, qp_on, initial_qp;
	u32 nbF, i;
	GF_FieldInfo sffield;

	nbF = 0;
	if (field->fieldType != GF_SG_VRML_MFNODE) {
		nbF = field->far_ptr ? ((GenMFField *)field->far_ptr)->count : 0;
		if (!nbF && (field->fieldType == GF_SG_VRML_MFSCRIPT))
			nbF = 1;
	} else if (field->far_ptr) {
		list = *((GF_ChildNodeItem **)field->far_ptr);
		nbF = gf_node_list_get_count(list);
	}
	/*reserved*/
	GF_BIFS_WRITE_INT(codec, bs, 0, 1, "reserved", NULL);
	if (!nbF) {
		/*is list*/
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "isList", NULL);
		/*end flag*/
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
		return GF_OK;
	}

	/*do we work in list or vector*/
	use_list = GF_FALSE;
	nbBits = gf_get_bit_size(nbF);
	if (nbBits + 5 > nbF + 1) use_list = GF_TRUE;

	GF_BIFS_WRITE_INT(codec, bs, use_list, 1, "isList", NULL);
	if (!use_list) {
		GF_BIFS_WRITE_INT(codec, bs, nbBits, 5, "nbBits", NULL);
		GF_BIFS_WRITE_INT(codec, bs, nbF, nbBits, "length", NULL);
	}

	memset(&sffield, 0, sizeof(GF_FieldInfo));
	sffield.fieldIndex = field->fieldIndex;
	sffield.fieldType = gf_sg_vrml_get_sf_type(field->fieldType);
	sffield.NDTtype = field->NDTtype;

	qp_on = GF_FALSE;
	qp_local = 0;
	initial_qp = codec->ActiveQP ? GF_TRUE : GF_FALSE;
	for (i=0; i<nbF; i++) {

		if (use_list) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "end", NULL);

		if (field->fieldType != GF_SG_VRML_MFNODE) {
			gf_sg_vrml_mf_get_item(field->far_ptr, field->fieldType, &sffield.far_ptr, i);
			e = gf_bifs_enc_sf_field(codec, bs, node, &sffield);
		} else {
			assert(list);
			e = gf_bifs_enc_node(codec, list->node, field->NDTtype, bs, node);

			/*activate QP*/
			if (list->node->sgprivate->tag == TAG_MPEG4_QuantizationParameter) {
				qp_local = ((M_QuantizationParameter *)list->node)->isLocal;
				if (qp_on) gf_bifs_enc_qp_remove(codec, GF_FALSE);
				e = gf_bifs_enc_qp_set(codec, list->node);
				if (e) return e;
				qp_on = GF_TRUE;
				if (qp_local) qp_local = 2;
			}
			list = list->next;
		}

		if (e) return e;

		if (qp_on && qp_local) {
			if (qp_local == 2) qp_local -= 1;
			else {
				gf_bifs_enc_qp_remove(codec, initial_qp);
				qp_local = qp_on = GF_FALSE;
			}
		}
	}

	if (use_list) GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
	if (qp_on) gf_bifs_enc_qp_remove(codec, initial_qp);
	/*for QP14*/
	gf_bifs_enc_qp14_set_length(codec, nbF);
	return GF_OK;
}


GF_Err gf_bifs_enc_field(GF_BifsEncoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field)
{
	assert(node);
	if (field->fieldType == GF_SG_VRML_UNKNOWN)
		return GF_NON_COMPLIANT_BITSTREAM;

	if (gf_sg_vrml_is_sf_field(field->fieldType)) {
		return gf_bifs_enc_sf_field(codec, bs, node, field);
	}

	/*TO DO : PMF support*/

	if (codec->info->config.UsePredictiveMFField) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "usePredictive", NULL);
	}
	return gf_bifs_enc_mf_field(codec, bs, node, field);
}

/*we assume a node field is not ISed several times (that's stated as "undefined behaviour" in VRML*/
GF_Route *gf_bifs_enc_is_field_ised(GF_BifsEncoder *codec, GF_Node *node, u32 fieldIndex)
{
	GF_Route *r;
	u32 i;
	if (!codec->encoding_proto) return NULL;

	if (node->sgprivate->interact && node->sgprivate->interact->routes) {
		i=0;
		while ((r = (GF_Route*)gf_list_enum(node->sgprivate->interact->routes, &i))) {
			if (!r->IS_route) continue;
			if ((r->ToNode == node) && (r->ToField.fieldIndex==fieldIndex)) return r;
			else if ((r->FromNode == node) && (r->FromField.fieldIndex==fieldIndex)) return r;
		}
	}

	i=0;
	while ((r = (GF_Route*)gf_list_enum(codec->encoding_proto->sub_graph->Routes, &i))) {
		if (!r->IS_route) continue;
		if ((r->ToNode == node) && (r->ToField.fieldIndex==fieldIndex)) return r;
		else if ((r->FromNode == node) && (r->FromField.fieldIndex==fieldIndex)) return r;
	}
	return NULL;
}

/**/
GF_Err EncNodeFields(GF_BifsEncoder * codec, GF_BitStream *bs, GF_Node *node)
{
	u8 mode;
	GF_Route *isedField;
	GF_Node *clone;
	GF_Err e;
	s32 *enc_fields;
	u32 numBitsALL, numBitsDEF, allInd, count, i, nbBitsProto, nbFinal;
	Bool use_list, nodeIsFDP = GF_FALSE;
	GF_FieldInfo field, clone_field;

	e = GF_OK;

	if (codec->encoding_proto) {
		mode = GF_SG_FIELD_CODING_ALL;
		nbBitsProto = gf_get_bit_size(gf_sg_proto_get_field_count(codec->encoding_proto) - 1);
		numBitsALL = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_ALL) - 1);
	} else {
		mode = GF_SG_FIELD_CODING_DEF;
		nbBitsProto = 0;
		numBitsALL = 0;
	}
	count = gf_node_get_num_fields_in_mode(node, mode);
	if (node->sgprivate->tag==TAG_MPEG4_Script) count = 3;

	if (!count) {
		GF_BIFS_WRITE_INT(codec, bs, 0, 1, "isMask", NULL);
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
		return GF_OK;
	}

	if (node->sgprivate->tag == TAG_ProtoNode) {
		clone = gf_sg_proto_create_instance(node->sgprivate->scenegraph, ((GF_ProtoInstance *)node)->proto_interface);
	} else {
		clone = gf_node_new(node->sgprivate->scenegraph, node->sgprivate->tag);
	}
	if (clone) gf_node_register(clone, NULL);

	numBitsDEF = gf_get_bit_size(gf_node_get_num_fields_in_mode(node, GF_SG_FIELD_CODING_DEF) - 1);

	enc_fields = (s32*)gf_malloc(sizeof(s32) * count);
	nbFinal = 0;
	for (i=0; i<count; i++) {
		enc_fields[i] = -1;
		/*get field in ALL mode*/
		if (mode == GF_SG_FIELD_CODING_ALL) {
			allInd = i;
		} else {
			gf_bifs_get_field_index(node, i, GF_SG_FIELD_CODING_DEF, &allInd);
		}

		/*encode proto code*/
		if (codec->encoding_proto) {
			isedField = gf_bifs_enc_is_field_ised(codec, node, allInd);
			if (isedField) {
				enc_fields[i] = allInd;
				nbFinal ++;
				continue;
			}
		}
		/*common case*/
		gf_node_get_field(node, allInd, &field);
		/*if event don't encode (happens when encoding protos)*/
		if ((field.eventType == GF_SG_EVENT_IN) || (field.eventType == GF_SG_EVENT_OUT)) continue;
		/*if field is default skip*/
		switch (field.fieldType) {
		case GF_SG_VRML_SFNODE:
			if (* (GF_Node **) field.far_ptr) {
				enc_fields[i] = allInd;
				nbFinal++;
			}
			break;
		case GF_SG_VRML_MFNODE:
			if (* (GF_ChildNodeItem **) field.far_ptr) {
				enc_fields[i] = allInd;
				nbFinal++;
			}
			break;
		case GF_SG_VRML_SFCOMMANDBUFFER:
		{
			SFCommandBuffer *cb = (SFCommandBuffer *)field.far_ptr;
			if (gf_list_count(cb->commandList)) {
				enc_fields[i] = allInd;
				nbFinal++;
			}
		}
		break;
		case GF_SG_VRML_MFSCRIPT:
			enc_fields[i] = allInd;
			nbFinal++;
			break;
		default:
			gf_node_get_field(clone, allInd, &clone_field);
			if (!gf_sg_vrml_field_equal(clone_field.far_ptr, field.far_ptr, field.fieldType)) {
				enc_fields[i] = allInd;
				nbFinal++;
			}
			break;
		}
	}
	if (clone) gf_node_unregister(clone, NULL);

	use_list = GF_TRUE;
	/* patch for FDP node : */
	/* cannot use default field sorting due to spec "mistake", so use list to imply inversion between field 2 and field 3 of FDP*/
	if (node->sgprivate->tag == TAG_MPEG4_FDP) {
		s32 s4SwapValue = enc_fields[2];
		enc_fields[2] = enc_fields[3];
		enc_fields[3] = s4SwapValue;
		nodeIsFDP = GF_TRUE;
		use_list = GF_TRUE;
	}
	/*number of bits in mask node is count*1, in list node is 1+nbFinal*(1+numBitsDEF) */
	else if (count < 1+nbFinal*(1+numBitsDEF))
		use_list = GF_FALSE;

	GF_BIFS_WRITE_INT(codec, bs, use_list ? 0 : 1, 1, "isMask", NULL);

	for (i=0; i<count; i++) {
		if (enc_fields[i] == -1) {
			if (!use_list) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "Mask", NULL);
			continue;
		}
		allInd = (u32) enc_fields[i];

		/*encode proto code*/
		if (codec->encoding_proto) {
			isedField = gf_bifs_enc_is_field_ised(codec, node, allInd);
			if (isedField) {
				if (use_list) {
					GF_BIFS_WRITE_INT(codec, bs, 0, 1, "end", NULL);
				} else {
					GF_BIFS_WRITE_INT(codec, bs, 1, 1, "Mask", NULL);
				}
				GF_BIFS_WRITE_INT(codec, bs, 1, 1, "isedField", NULL);
				if (use_list) GF_BIFS_WRITE_INT(codec, bs, allInd, numBitsALL, "nodeField", NULL);

				if (isedField->ToNode == node) {
					GF_BIFS_WRITE_INT(codec, bs, isedField->FromField.fieldIndex, nbBitsProto, "protoField", NULL);
				} else {
					GF_BIFS_WRITE_INT(codec, bs, isedField->ToField.fieldIndex, nbBitsProto, "protoField", NULL);
				}
				continue;
			}
		}
		/*common case*/
		gf_node_get_field(node, allInd, &field);
		if (use_list) {
			/*not end flag*/
			GF_BIFS_WRITE_INT(codec, bs, 0, 1, "end", NULL);
		} else {
			/*mask flag*/
			GF_BIFS_WRITE_INT(codec, bs, 1, 1, "Mask", NULL);
		}
		/*not ISed field*/
		if (codec->encoding_proto) GF_BIFS_WRITE_INT(codec, bs, 0, 1, "isedField", NULL);
		if (use_list) {
			if (codec->encoding_proto || nodeIsFDP) {
				u32 ind=0;
				/*for proto, we're in ALL mode and we need DEF mode*/
				/*for FDP, encoding requires to get def id from all id as fields 2 and 3 are reversed*/
				gf_bifs_field_index_by_mode(node, allInd, GF_SG_FIELD_CODING_DEF, &ind);
				GF_BIFS_WRITE_INT(codec, bs, ind, numBitsDEF, "field", (char*)field.name);
			} else {
				GF_BIFS_WRITE_INT(codec, bs, i, numBitsDEF, "field", (char*)field.name);
			}
		}
		e = gf_bifs_enc_field(codec, bs, node, &field);
		if (e) goto exit;
	}
	/*end flag*/
	if (use_list) GF_BIFS_WRITE_INT(codec, bs, 1, 1, "end", NULL);
exit:
	gf_free(enc_fields);
	return e;
}

Bool BE_NodeIsUSE(GF_BifsEncoder * codec, GF_Node *node)
{
	u32 i, count;
	if (!node || !gf_node_get_id(node) ) return GF_FALSE;
	count = gf_list_count(codec->encoded_nodes);
	for (i=0; i<count; i++) {
		if (gf_list_get(codec->encoded_nodes, i) == node) return GF_TRUE;
	}
	gf_list_add(codec->encoded_nodes, node);
	return GF_FALSE;
}

GF_Err gf_bifs_enc_node(GF_BifsEncoder * codec, GF_Node *node, u32 NDT_Tag, GF_BitStream *bs, GF_Node *parent_node)
{
	u32 NDTBits, node_type, node_tag, BVersion, node_id;
	const char *node_name;
	Bool flag, reset_qp14;
	GF_Node *new_node;
	GF_Err e;

	assert(codec->info);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[BIFS] Encode node %s\n", gf_node_get_class_name(node) ));

	/*NULL node is a USE of maxID*/
	if (!node) {
		GF_BIFS_WRITE_INT(codec, bs, 1, 1, "USE", NULL);
		GF_BIFS_WRITE_INT(codec, bs, (1<<codec->info->config.NodeIDBits) - 1 , codec->info->config.NodeIDBits, "NodeID", "NULL");
		return GF_OK;
	}

	flag = BE_NodeIsUSE(codec, node);
	GF_BIFS_WRITE_INT(codec, bs, flag ? 1 : 0, 1, "USE", (char*)gf_node_get_class_name(node));

	if (flag) {
		gf_bs_write_int(bs, gf_node_get_id(node) - 1, codec->info->config.NodeIDBits);
		new_node = gf_bifs_enc_find_node(codec, gf_node_get_id(node) );
		if (!new_node)
			return codec->LastError = GF_SG_UNKNOWN_NODE;

		/*restore QP14 length*/
		switch (gf_node_get_tag(new_node)) {
		case TAG_MPEG4_Coordinate:
		{
			u32 nbCoord = ((M_Coordinate *)new_node)->point.count;
			gf_bifs_enc_qp14_enter(codec, GF_TRUE);
			gf_bifs_enc_qp14_set_length(codec, nbCoord);
			gf_bifs_enc_qp14_enter(codec, GF_FALSE);
		}
		break;
		case TAG_MPEG4_Coordinate2D:
		{
			u32 nbCoord = ((M_Coordinate2D *)new_node)->point.count;
			gf_bifs_enc_qp14_enter(codec, GF_TRUE);
			gf_bifs_enc_qp14_set_length(codec, nbCoord);
			gf_bifs_enc_qp14_enter(codec, GF_FALSE);
		}
		break;
		}
		return GF_OK;
	}

	BVersion = GF_BIFS_V1;
	node_tag = node->sgprivate->tag;
	while (1) {
		node_type = gf_bifs_get_node_type(NDT_Tag, node_tag, BVersion);
		NDTBits = gf_bifs_get_ndt_bits(NDT_Tag, BVersion);
		if (BVersion==2 && (node_tag==TAG_ProtoNode)) node_type = 1;
		GF_BIFS_WRITE_INT(codec, bs, node_type, NDTBits, "ndt", NULL);
		if (node_type) break;

		BVersion += 1;
		if (BVersion > GF_BIFS_NUM_VERSION) {
			if (parent_node) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Cannot encode node %s as a child of %s\n", gf_node_get_class_name(node), gf_node_get_class_name(parent_node) ));
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[BIFS] Cannot encode node %s in the SFWorldNode context\n", gf_node_get_class_name(node) ));
			}
			return codec->LastError = GF_BIFS_UNKNOWN_VERSION;
		}
	}
	if (BVersion==2 && node_type==1) {
		GF_Proto *proto = ((GF_ProtoInstance *)node)->proto_interface;
		GF_BIFS_WRITE_INT(codec, bs, proto->ID, codec->info->config.ProtoIDBits, "protoID", NULL);
	}

	/*special handling of 3D mesh*/

	/*DEF'd node*/
	node_name = gf_node_get_name_and_id(node, &node_id);
	GF_BIFS_WRITE_INT(codec, bs, node_id ? 1 : 0, 1, "DEF", NULL);
	if (node_id) {
		GF_BIFS_WRITE_INT(codec, bs, node_id - 1, codec->info->config.NodeIDBits, "NodeID", NULL);
		if (codec->UseName) gf_bifs_enc_name(codec, bs, (char*) node_name );
	}

	/*no updates of time fields for now - NEEDED FOR A LIVE ENCODER*/

	/*if coords were not stored for QP14 before coding this node, reset QP14 it when leaving*/
	reset_qp14 = !codec->coord_stored;

	/*QP14 case*/
	switch (node_tag) {
	case TAG_MPEG4_Coordinate:
	case TAG_MPEG4_Coordinate2D:
		gf_bifs_enc_qp14_enter(codec, GF_TRUE);
	}

	e = EncNodeFields(codec, bs, node);
	if (e) return e;

	if (codec->coord_stored && reset_qp14)
		gf_bifs_enc_qp14_reset(codec);

	switch (node_tag) {
	case TAG_MPEG4_Coordinate:
	case TAG_MPEG4_Coordinate2D:
		gf_bifs_enc_qp14_enter(codec, GF_FALSE);
		break;
	}
	return GF_OK;
}


#endif /*GPAC_DISABLE_BIFS_ENC*/
