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


#ifndef _GF_BIFS_DEV_H_
#define _GF_BIFS_DEV_H_


#include <gpac/nodes_mpeg4.h>
#include <gpac/bitstream.h>
#include <gpac/bifs.h>
#include <gpac/thread.h>

typedef struct
{
	/*v1 or v2*/
	u8 version;
	/*if false this is a BIFS-ANIM stream*/
	Bool IsCommandStream;
	/*BIFS config - common fields*/
	u16 NodeIDBits;
	u16 RouteIDBits;
	Bool PixelMetrics;
	/*set to 0, 0 if no size is specified*/
	u16 Width, Height;

	/*BIFS-Anim - not supported */
	/*if 1 the BIFS_Anim codec is reset at each intra frame*/
	Bool BAnimRAP;
	/*will be specified once we have BIFS anim*/
	void *AnimMask;

	/*BIFS v2 add-on*/
	Bool Use3DMeshCoding;
	Bool UsePredictiveMFField;
	u16 ProtoIDBits;
} BIFSConfig;



/*per_stream config support*/
typedef struct 
{
	BIFSConfig config;
	Bool UseName;
	u16 ESID;
} BIFSStreamInfo;


struct __tag_bifs_dec
{
	GF_Err LastError;
	/*all attached streams*/
	GF_List *streamInfo;
	/*active stream*/
	BIFSStreamInfo *info;

	GF_SceneGraph *scenegraph;
	/*modified during conditional execution / proto parsing*/
	GF_SceneGraph *current_graph;

	/*Quantization*/
	/*QP stack*/
	GF_List *QPs;
	/*active QP*/
	M_QuantizationParameter *ActiveQP;

	/*QP 14 stuff: we need to store the last numb of fields in the last recieved Coord //field (!!!)*/
	
	/*number of iten in the Coord field*/
	u32 NumCoord;
	Bool coord_stored, storing_coord;

	/*active QP*/
	M_QuantizationParameter *GlobalQP;


	/*only set at SceneReplace during proto parsing, NULL otherwise*/
	GF_Proto *pCurrentProto;

	Double (*GetSceneTime)(void *st_cbk);
	void *st_cbk;

	/*when set the decoder works with commands rather than modifying the scene graph directly*/
	Bool dec_memory_mode;
	Bool force_keep_qp;
	/*only set in mem mode. Conditionals/InputSensors are stacked while decoding, then decoded once the AU is decoded
	to make sure all nodes potentially used by the conditional command buffer are created*/
	GF_List *conditionals;

	Bool ignore_size;
	
//	GF_Mutex *mx;
};


/*decodes an GF_Node*/
GF_Node *gf_bifs_dec_node(GF_BifsDecoder * codec, GF_BitStream *bs, u32 NDT_Tag);
/*decodes an SFField (to get a ptr to the field, use gf_node_get_field )
the FieldIndex is used for Quantzation*/
GF_Err gf_bifs_dec_sf_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
/*decodes a Field (either SF or MF). The field MUST BE EMPTY*/
GF_Err gf_bifs_dec_field(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
/*decodes a route*/
GF_Err gf_bifs_dec_route(GF_BifsDecoder * codec, GF_BitStream *bs, Bool is_insert);
/*get name*/
void gf_bifs_dec_name(GF_BitStream *bs, char *name);

BIFSStreamInfo *gf_bifs_dec_get_stream(GF_BifsDecoder * codec, u16 ESID);
/*decodes a BIFS command frame*/
GF_Err gf_bifs_dec_command(GF_BifsDecoder * codec, GF_BitStream *bs);
/*decodes proto list - if proto_list is not NULL, protos parsed are not registered with the parent graph
and added to the list*/
GF_Err gf_bifs_dec_proto_list(GF_BifsDecoder * codec, GF_BitStream *bs, GF_List *proto_list);
/*decodes field(s) of a node - exported for MultipleReplace*/
GF_Err gf_bifs_dec_node_list(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node);
GF_Err gf_bifs_dec_node_mask(GF_BifsDecoder * codec, GF_BitStream *bs, GF_Node *node);


GF_Node *gf_bifs_dec_find_node(GF_BifsDecoder * codec, u32 NodeID);

/*called once a field has been modified through a command, send eventOut or propagate eventIn if needed*/
void gf_bifs_check_field_change(GF_Node *node, GF_FieldInfo *field);


struct __tag_bifs_enc
{
	GF_Err LastError;
	/*all attached streams*/
	GF_List *streamInfo;
	/*active stream*/
	BIFSStreamInfo *info;

	/*the scene graph the codec is encoding (set htrough ReplaceScene or manually)*/
	GF_SceneGraph *scene_graph;
	/*current proto graph for DEF/USE*/
	GF_SceneGraph *current_proto_graph;

	/*Quantization*/
	/*QP stack*/
	GF_List *QPs;
	/*active QP*/
	M_QuantizationParameter *ActiveQP;

	/*active QP*/
	M_QuantizationParameter *GlobalQP;

	u32 NumCoord;
	Bool coord_stored, storing_coord;

	GF_Proto *encoding_proto;

	GF_Mutex *mx;

	/*keep track of DEF/USE*/
	GF_List *encoded_nodes;

	FILE *trace;
};

GF_Err gf_bifs_enc_commands(GF_BifsEncoder *codec, GF_List *comList, GF_BitStream *bs);

GF_Err gf_bifs_enc_node(GF_BifsEncoder * codec, GF_Node *node, u32 NDT_Tag, GF_BitStream *bs);
GF_Err gf_bifs_enc_sf_field(GF_BifsEncoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
GF_Err gf_bifs_enc_field(GF_BifsEncoder * codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
GF_Err gf_bifs_enc_mf_field(GF_BifsEncoder *codec, GF_BitStream *bs, GF_Node *node, GF_FieldInfo *field);
GF_Err gf_bifs_enc_route(GF_BifsEncoder *codec, GF_Route *r, GF_BitStream *bs);
void gf_bifs_enc_name(GF_BifsEncoder *codec, GF_BitStream *bs, char *name);
GF_Node *gf_bifs_enc_find_node(GF_BifsEncoder *codec, u32 nodeID);

void gf_bifs_enc_log_bits(GF_BifsEncoder *codec, s32 val, u32 nbBits, char *str, char *com);

#define GF_BE_WRITE_INT(codec, bs, val, nbBits, str, com)	{\
	gf_bs_write_int(bs, val, nbBits);	\
	gf_bifs_enc_log_bits(codec, val, nbBits, str, com);	}\


GF_Route *gf_bifs_enc_is_field_ised(GF_BifsEncoder *codec, GF_Node *node, u32 fieldIndex);


/*get field QP and anim info*/
Bool gf_bifs_get_aq_info(GF_Node *Node, u32 FieldIndex, u8 *QType, u8 *AType, Fixed *b_min, Fixed *b_max, u32 *QT13_bits);

/*get the absolute field 0_based index (or ALL mode) given the field index in IndexMode*/
GF_Err gf_bifs_get_field_index(GF_Node *Node, u32 inField, u8 IndexMode, u32 *allField);

/*returns the opaque NodeDataType of the node "children" field if any, or 0*/
u32 gf_bifs_get_child_table(GF_Node *Node);

/*returns binary type of node in the given version of the desired NDT*/
u32 gf_bifs_get_node_type(u32 NDT_Tag, u32 NodeTag, u32 Version);

/*converts field index from all_mode to given mode*/
GF_Err gf_bifs_field_index_by_mode(GF_Node *node, u32 all_ind, u8 indexMode, u32 *outField);

/*return number of bits needed to code all nodes present in the specified NDT*/
u32 gf_bifs_get_ndt_bits(u32 NDT_Tag, u32 Version);
/*return absolute node tag given its type in the NDT and the NDT version number*/
u32 gf_bifs_ndt_get_node_type(u32 NDT_Tag, u32 NodeType, u32 Version);

/*set QP and anim info for a proto field (BIFS allows for that in proto coding)*/
GF_Err gf_bifs_proto_field_set_aq_info(GF_ProtoFieldInterface *field, u32 QP_Type, u32 hasMinMax, u32 QPSFType, void *qp_min_value, void *qp_max_value, u32 QP13_NumBits);


GF_Err gf_bifs_insert_sf_node(void *mfnode_far_ptr, GF_Node *new_child, s32 Position);

#endif	//_GF_BIFS_DEV_H_

