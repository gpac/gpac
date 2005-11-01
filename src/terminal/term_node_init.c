/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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


#include <gpac/renderer.h>
#include <gpac/internal/terminal_dev.h>
/*includes MPEG4 nodes + input sensor stack*/
#include "input_sensor.h"
/*includes X3D nodes for WorldInfo, Inline and Key/String sensors*/
#include <gpac/nodes_x3d.h>

void InitMediaControl(GF_InlineScene *is, GF_Node *node);
void MC_Modified(GF_Node *node);
void InitMediaSensor(GF_InlineScene *is, GF_Node *node);
void MS_Modified(GF_Node *node);


void Destroy_WorldInfo(GF_Node *node)
{
	GF_InlineScene *is = gf_node_get_private(node);
	is->world_info = NULL;
}
void Render_WorldInfo(GF_Node *node, void *rs)
{
	GF_InlineScene *is = gf_node_get_private(node);
	is->world_info = (M_WorldInfo *) node;
}

void gf_term_on_node_init(void *_is, GF_Node *node)
{
	GF_InlineScene *is = (GF_InlineScene *)_is;
	if (!node || !is) return;
	
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: 
	case TAG_X3D_Inline: 
		gf_node_set_render_function(node, gf_is_render); break;
	case TAG_MPEG4_MediaBuffer: break;
	case TAG_MPEG4_MediaControl: InitMediaControl(is, node); break;
	case TAG_MPEG4_MediaSensor: InitMediaSensor(is, node); break;
	case TAG_MPEG4_InputSensor: InitInputSensor(is, node); break;

	/*BIFS nodes, get back to codec, but filter externProtos*/
	case TAG_MPEG4_Conditional: break;
	case TAG_MPEG4_QuantizationParameter: break;
	/*world info is stored at the inline scene level*/
	case TAG_MPEG4_WorldInfo:
	case TAG_X3D_WorldInfo:
		gf_node_set_predestroy_function(node, Destroy_WorldInfo);
		gf_node_set_render_function(node, Render_WorldInfo);
		gf_node_set_private(node, is);
		break;

	case TAG_X3D_KeySensor: InitKeySensor(is, node); break;
	case TAG_X3D_StringSensor: InitStringSensor(is, node); break;

	default: gf_sr_on_node_init(is->root_od->term->renderer, node); break;
	}
}

void gf_term_on_node_modified(void *_is, GF_Node *node)
{
	GF_InlineScene *is = (GF_InlineScene *)_is;
	if (!is) return;
	if (!node) {
		gf_sr_invalidate(is->root_od->term->renderer, NULL); 
		return;
	}
	
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: 
	case TAG_X3D_Inline: 
		gf_is_on_modified(node); break;
	case TAG_MPEG4_MediaBuffer: break;
	case TAG_MPEG4_MediaControl: MC_Modified(node); break;
	case TAG_MPEG4_MediaSensor: MS_Modified(node); break;
	case TAG_MPEG4_InputSensor: InputSensorModified(node); break;
	case TAG_MPEG4_Conditional: break;
	default: gf_sr_invalidate(is->root_od->term->renderer, node); break;
	}
}
