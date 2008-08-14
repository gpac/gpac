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


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
/*includes MPEG4 nodes + input sensor stack*/
#include "input_sensor.h"
/*includes X3D nodes for WorldInfo, Inline and Key/String sensors*/
#include <gpac/nodes_x3d.h>
#include <gpac/nodes_svg.h>

void InitMediaControl(GF_InlineScene *is, GF_Node *node);
void MC_Modified(GF_Node *node);
void InitMediaSensor(GF_InlineScene *is, GF_Node *node);
void MS_Modified(GF_Node *node);
void InitInline(GF_InlineScene *is, GF_Node *node);


void TraverseWorldInfo(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_InlineScene *is = (GF_InlineScene *)gf_node_get_private(node);
	is->world_info = is_destroy ? NULL : (M_WorldInfo *) node;
}

void svg_traverse_title(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_InlineScene *is = (GF_InlineScene *)gf_node_get_private(node);
	is->world_info = is_destroy ? NULL : (M_WorldInfo *) node;
}

void evaluate_term_cap(GF_Node *node)
{
	GF_SystemRTInfo rti;
	Double fps;
	u32 height;
	Bool b_on;
	u32 b_charge, b_level;

	M_TermCap *tc = (M_TermCap *)node;
	GF_InlineScene *is = gf_node_get_private(node);
	tc->value = 0;
	switch (tc->capability) {
	case 0:	/*framerate*/
		fps = gf_sc_get_fps(is->root_od->term->compositor, 1);
		if (fps<=5.0) tc->value = 1;
		else if (fps<=10.0) tc->value = 2;
		else if (fps<=20.0) tc->value = 3;
		else if (fps<=40.0) tc->value = 4;
		else tc->value = 5;
		break;
	case 1:	/*colordepth*/
		return;
	case 2:	/*screensize*/
		height = is->root_od->term->compositor->display_height;
		if (height<200) tc->value = 1;
		else if (height<400) tc->value = 2;
		else if (height<800) tc->value = 3;
		else if (height<1600) tc->value = 4;
		else tc->value = 4;
		break;
	case 3:	/*graphics hardware*/
		return;
	case 32:/*audio out format*/
		return;
	case 33:/*audio out format*/
		return;
	case 34:/*spatial audio cap*/
		return;
	case 64:/*CPU load*/
		if (!gf_sys_get_rti(200, &rti, 0) ) return;
		if (rti.total_cpu_usage<20) tc->value = 1;
		else if (rti.total_cpu_usage<40) tc->value = 2;
		else if (rti.total_cpu_usage<60) tc->value = 3;
		else if (rti.total_cpu_usage<80) tc->value = 4;
		else tc->value = 5;
		break;
	case 65:/*mem load*/
		if (!gf_sys_get_rti(200, &rti, GF_RTI_SYSTEM_MEMORY_ONLY) ) return;
		rti.physical_memory_avail /= 1024;
		if (rti.physical_memory_avail < 100) tc->value = 1;
		else if (rti.physical_memory_avail < 512) tc->value = 2;
		else if (rti.physical_memory_avail < 2048) tc->value = 3;
		else if (rti.physical_memory_avail < 8192) tc->value = 4;
		else if (rti.physical_memory_avail < 32768) tc->value = 5;
		else if (rti.physical_memory_avail < 204800) tc->value = 6;
		else tc->value = 7;
		break;

	/*GPAC extensions*/
	case 100: /*display width*/
		tc->value = is->root_od->term->compositor->display_width;
		break;
	case 101: /*display height*/
		tc->value = is->root_od->term->compositor->display_height;
		break;
	case 102: /*frame rate*/
		tc->value = (u32) gf_sc_get_fps(is->root_od->term->compositor, 1);
		break;
	case 103: /*total CPU*/
		if (!gf_sys_get_rti(200, &rti, 0) ) return;
		tc->value = rti.total_cpu_usage;
		break;
	case 104: /*process CPU*/
		if (!gf_sys_get_rti(200, &rti, 0) ) return;
		tc->value = rti.process_cpu_usage;
		break;
	case 106: /*total memory in kB*/
		if (!gf_sys_get_rti(200, &rti, GF_RTI_SYSTEM_MEMORY_ONLY) ) return;
		tc->value = (u32) (rti.physical_memory/1024);
		break;
	case 107: /*total memory available in kB*/
		if (!gf_sys_get_rti(200, &rti, GF_RTI_SYSTEM_MEMORY_ONLY) ) return;
		tc->value = (u32) (rti.physical_memory_avail/1024);
		break;
	case 108: /*process memory in kB*/
		if (!gf_sys_get_rti(200, &rti, 0) ) return;
		tc->value = (u32) (rti.process_memory/1024);
		break;
	case 109: /*battery on/off*/
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level);
		tc->value = b_on;
		break;
	case 110: /*battery charging*/
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level);
		tc->value = b_charge;
		break;
	case 111: /*battery level*/
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level);
		tc->value = b_level;
		break;

	case 112: /*audio vol*/
		tc->value = gf_sys_get_battery_state(&b_on, &b_charge, &b_level);
		break;
	case 113: /*audio pan*/
		tc->value = gf_sys_get_battery_state(&b_on, &b_charge, &b_level);
		break;
	default:
		return;
	}
	gf_node_event_out(node, 2);
}

static void InitTermCap(GF_InlineScene *is, GF_Node *node)
{
	M_TermCap *tc = (M_TermCap *)node;
	tc->on_evaluate = evaluate_term_cap;
	gf_node_set_private(node, is);
	/*evaluate upon init (cf BIFS spec)*/
	evaluate_term_cap(node);
}

void gf_term_on_node_init(void *_is, GF_Node *node)
{
	GF_InlineScene *is = (GF_InlineScene *)_is;
	if (!node || !is) return;
	
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: 
	case TAG_X3D_Inline: 
		InitInline(is, node); break;
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
		gf_node_set_callback_function(node, TraverseWorldInfo);
		gf_node_set_private(node, is);
		break;

	case TAG_X3D_KeySensor: InitKeySensor(is, node); break;
	case TAG_X3D_StringSensor: InitStringSensor(is, node); break;

	case TAG_MPEG4_TermCap: 
		InitTermCap(is, node); break;

#ifndef GPAC_DISABLE_SVG

	case TAG_SVG_title: 
		gf_node_set_callback_function(node, svg_traverse_title);
		gf_node_set_private(node, is);
		break;
#endif

	default: gf_sc_on_node_init(is->root_od->term->compositor, node); break;
	}
}

void gf_term_on_node_modified(void *_is, GF_Node *node)
{
	GF_InlineScene *is = (GF_InlineScene *)_is;
	if (!is) return;
	if (!node) {
		gf_sc_invalidate(is->root_od->term->compositor, NULL); 
		return;
	}
	
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: 
	case TAG_X3D_Inline: 
		gf_inline_on_modified(node); break;
	case TAG_MPEG4_MediaBuffer: break;
	case TAG_MPEG4_MediaControl: MC_Modified(node); break;
	case TAG_MPEG4_MediaSensor: MS_Modified(node); break;
	case TAG_MPEG4_InputSensor: InputSensorModified(node); break;
	case TAG_MPEG4_Conditional: break;
	default: gf_sc_invalidate(is->root_od->term->compositor, node); break;
	}
}

GF_EXPORT
void gf_term_node_callback(void *_is, u32 type, GF_Node *n, void *param)
{
	if (type==GF_SG_CALLBACK_MODIFIED) gf_term_on_node_modified(_is, n);
	else if (type==GF_SG_CALLBACK_INIT) gf_term_on_node_init(_is, n);
	/*get all inline nodes using this subscene and bubble up...*/
	else if (type==GF_SG_CALLBACK_GRAPH_DIRTY) {
		u32 i=0;
		GF_Node *root;
		GF_InlineScene *is = (GF_InlineScene *)_is;

		while ((root=(GF_Node*)gf_list_enum(is->inline_nodes, &i))) {
			gf_node_dirty_set(root, GF_SG_CHILD_DIRTY, 1);
		}
	}
}
