/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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


#ifndef GPAC_DISABLE_BIFS

/*private stack for conditional*/
typedef struct
{
	/*BIFS decoder*/
	GF_BifsDecoder *codec;
	/*BIFS config of original stream carrying the conditional*/
	BIFSStreamInfo *info;
} ConditionalStack;

void Conditional_PreDestroy(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		ConditionalStack *priv = (ConditionalStack*)gf_node_get_private(n);
		if (priv) gf_free(priv);
	}
}

void Conditional_BufferReplaced(GF_BifsDecoder *codec, GF_Node *n)
{
	ConditionalStack *priv = (ConditionalStack*)gf_node_get_private(n);
	if (!priv || (gf_node_get_tag(n) != TAG_MPEG4_Conditional)) return;
	priv->info = codec->info;
}

static void Conditional_execute(M_Conditional *node)
{
	char *buffer;
	u32 len;
	GF_BitStream *bs;
	GF_BifsDecoder *codec;
	GF_Proto *prevproto;
	GF_SceneGraph *prev_graph;
	ConditionalStack *priv = (ConditionalStack*)gf_node_get_private((GF_Node*)node);
	if (!priv) return;

	/*set the codec working graph to the node one (to handle conditional in protos)*/
	prev_graph = priv->codec->current_graph;
	priv->codec->current_graph = gf_node_get_graph((GF_Node*)node);
	if (!priv->codec->current_graph) return;

	priv->codec->info = priv->info;
	prevproto = priv->codec->pCurrentProto;
	priv->codec->pCurrentProto = NULL;
	if (priv->codec->current_graph->pOwningProto) priv->codec->pCurrentProto = priv->codec->current_graph->pOwningProto->proto_interface;

	/*set isActive - to clarify in the specs*/
	node->isActive = 1;
	gf_node_event_out((GF_Node *)node, 3/*"isActive"*/);
	if (!node->buffer.bufferSize) return;

	/*we may replace ourselves*/
	buffer = (char*)node->buffer.buffer;
	len = node->buffer.bufferSize;
	node->buffer.buffer = NULL;
	node->buffer.bufferSize = 0;
	bs = gf_bs_new(buffer, len, GF_BITSTREAM_READ);
	codec = priv->codec;
	codec->cts_offset = gf_node_get_scene_time((GF_Node*)node);
	/*a conditional may destroy/replace itself - to prevent that, protect node by a register/unregister ...*/
	gf_node_register((GF_Node*)node, NULL);
#ifdef GF_SELF_REPLACE_ENABLE
	/*and a conditional may destroy the entire scene!*/
	cur_graph->graph_has_been_reset = 0;
#endif
	gf_bifs_dec_command(codec, bs);
	gf_bs_del(bs);
#ifdef GF_SELF_REPLACE_ENABLE
	if (cur_graph->graph_has_been_reset) {
		return;
	}
#endif
	if (node->buffer.buffer) {
		gf_free(buffer);
	} else {
		node->buffer.buffer = (u8*)buffer;
		node->buffer.bufferSize = len;
	}
	//set isActive - to clarify in the specs
//	node->isActive = 0;
	gf_node_unregister((GF_Node*)node, NULL);
	codec->cts_offset = 0;
	codec->pCurrentProto = prevproto;
	codec->current_graph = prev_graph;
}

void Conditional_OnActivate(GF_Node *n, GF_Route *route)
{
	M_Conditional *node = (M_Conditional *)n;
	if (! node->activate) return;
	Conditional_execute(node);
}

void Conditional_OnReverseActivate(GF_Node *n, GF_Route *route)
{
	M_Conditional *node = (M_Conditional *)n;
	if (node->reverseActivate) return;
	Conditional_execute(node);
}

void SetupConditional(GF_BifsDecoder *codec, GF_Node *node)
{
	ConditionalStack *priv;
	if (gf_node_get_tag(node) != TAG_MPEG4_Conditional) return;
	priv = (ConditionalStack*)gf_malloc(sizeof(ConditionalStack));

	/*needed when initializing extern protos*/
	if (!codec->info) codec->info = (BIFSStreamInfo*)gf_list_get(codec->streamInfo, 0);
	if (!codec->info) return;

	priv->info = codec->info;
	priv->codec = codec;
	codec->has_conditionnals = GF_TRUE;
	gf_node_set_callback_function(node, Conditional_PreDestroy);
	gf_node_set_private(node, priv);
	((M_Conditional *)node)->on_activate = Conditional_OnActivate;
	((M_Conditional *)node)->on_reverseActivate = Conditional_OnReverseActivate;
}

#endif	/*GPAC_DISABLE_BIFS*/


/*this is ugly but we have no choice, we need to clone the conditional stack because of externProto*/
void BIFS_SetupConditionalClone(GF_Node *node, GF_Node *orig)
{
#ifndef GPAC_DISABLE_BIFS
	M_Conditional *ptr;
	ConditionalStack *priv_orig, *priv;
	priv_orig = (ConditionalStack*)gf_node_get_private(orig);
	/*looks we're not in BIFS*/
	if (!priv_orig) {
#else
	{
#endif

#ifndef GPAC_DISABLE_VRML
		u32 i;
		GF_Command *ori_com;
		M_Conditional *c_orig, *c_dest;
		c_orig = (M_Conditional *)orig;
		c_dest = (M_Conditional *)node;
		gf_node_init(node);
		/*and clone all commands*/
		i=0;
		while ((ori_com = (GF_Command*)gf_list_enum(c_orig->buffer.commandList, &i))) {
			GF_Command *dest_com = gf_sg_vrml_command_clone(ori_com, gf_node_get_graph(node), GF_TRUE);
			if (dest_com) gf_list_add(c_dest->buffer.commandList, dest_com);
		}
#endif
		return;
	}

#ifndef GPAC_DISABLE_BIFS
	priv = (ConditionalStack*)gf_malloc(sizeof(ConditionalStack));
	priv->codec = priv_orig->codec;
	priv->info = priv_orig->info;
	gf_node_set_callback_function(node, Conditional_PreDestroy);
	gf_node_set_private(node, priv);
	ptr = (M_Conditional *)node;
	ptr->on_activate = Conditional_OnActivate;
	ptr->on_reverseActivate = Conditional_OnReverseActivate;
#endif

}
