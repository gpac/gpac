/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#include <gpac/internal/compositor_dev.h>

#if !defined(GPAC_DISABLE_COMPOSITOR)

/*includes X3D nodes for WorldInfo, Inline and Key/String sensors*/
#include <gpac/nodes_x3d.h>
#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_VRML

void InitMediaControl(GF_Scene *scene, GF_Node *node);
void MC_Modified(GF_Node *node);

void InitMediaSensor(GF_Scene *scene, GF_Node *node);
void MS_Modified(GF_Node *node);

void gf_init_inline(GF_Scene *scene, GF_Node *node);
void gf_inline_on_modified(GF_Node *node);

void gf_scene_init_storage(GF_Scene *scene, GF_Node *node);


void TraverseWorldInfo(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Scene *scene = (GF_Scene *)gf_node_get_private(node);
	scene->world_info = is_destroy ? NULL : (M_WorldInfo *) node;
}

void TraverseKeyNavigator(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		GF_Scene *scene = (GF_Scene *)gf_node_get_private(node);
		gf_list_del_item(scene->keynavigators, node);
		gf_sc_key_navigator_del(scene->compositor, node);
	}
}

void on_kn_set_focus(GF_Node*node, GF_Route *_route)
{
	if (node) {
		GF_Scene *scene = (GF_Scene *)gf_node_get_private(node);
		gf_sc_change_key_navigator(scene->compositor, node);
	}
}

void evaluate_scene_cap(GF_Node *node, GF_Route *route)
{
	GF_SystemRTInfo rti;
	Double fps;
	u32 height;
	Bool b_on;
	u32 b_charge, b_level;

	M_TermCap *tc = (M_TermCap *)node;
	GF_Scene *scene = gf_node_get_private(node);
	tc->value = 0;
	switch (tc->capability) {
	case 0:	/*framerate*/
		fps = gf_sc_get_fps(scene->compositor, 1);
		if (fps<=5.0) tc->value = 1;
		else if (fps<=10.0) tc->value = 2;
		else if (fps<=20.0) tc->value = 3;
		else if (fps<=40.0) tc->value = 4;
		else tc->value = 5;
		break;
	case 1:	/*colordepth*/
		return;
	case 2:	/*screensize*/
		height = scene->compositor->display_height;
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
		tc->value = scene->compositor->display_width;
		break;
	case 101: /*display height*/
		tc->value = scene->compositor->display_height;
		break;
	case 102: /*frame rate*/
		tc->value = (u32) gf_sc_get_fps(scene->compositor, 1);
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
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level, NULL, NULL);
		tc->value = b_on;
		break;
	case 110: /*battery charging*/
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level, NULL, NULL);
		tc->value = b_charge;
		break;
	case 111: /*battery level*/
		gf_sys_get_battery_state(&b_on, &b_charge, &b_level, NULL, NULL);
		tc->value = b_level;
		break;

	case 112: /*audio vol*/
		tc->value = gf_sc_get_option(scene->compositor, GF_OPT_AUDIO_VOLUME);
		break;
	case 113: /*audio pan*/
		tc->value = gf_sc_get_option(scene->compositor, GF_OPT_AUDIO_PAN);
		break;
	default:
		return;
	}
	gf_node_event_out(node, 2);
}

static void InitTermCap(GF_Scene *scene, GF_Node *node)
{
	M_TermCap *tc = (M_TermCap *)node;
	tc->on_evaluate = evaluate_scene_cap;
	gf_node_set_private(node, scene);
	/*evaluate upon init (cf BIFS spec)*/
	evaluate_scene_cap(node, NULL);
}

#endif /*GPAC_DISABLE_VRML*/


#ifndef GPAC_DISABLE_SVG
static void svg_traverse_title(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_Scene *scene = (GF_Scene *)gf_node_get_private(node);
	scene->world_info = is_destroy ? NULL : node;
}
#endif

void gf_sc_on_node_init(GF_Compositor *sc, GF_Node *node);

void gf_scene_on_node_init(void *_scene, GF_Node *node)
{
	GF_Scene *scene = (GF_Scene *)_scene;
	if (!node || !scene) return;

	switch (gf_node_get_tag(node)) {
#ifndef GPAC_DISABLE_VRML

	case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
#endif
		gf_init_inline(scene, node);
		break;
	case TAG_MPEG4_MediaBuffer:
		break;
	case TAG_MPEG4_MediaControl:
		InitMediaControl(scene, node);
		break;
	case TAG_MPEG4_MediaSensor:
		InitMediaSensor(scene, node);
		break;
	case TAG_MPEG4_InputSensor:
		InitInputSensor(scene, node);
		break;

	/*BIFS nodes, get back to codec, but filter externProtos*/
	case TAG_MPEG4_Conditional:
		break;
	case TAG_MPEG4_QuantizationParameter:
		break;
	/*world info is stored at the inline scene level*/
	case TAG_MPEG4_WorldInfo:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_WorldInfo:
#endif
		gf_node_set_callback_function(node, TraverseWorldInfo);
		gf_node_set_private(node, scene);
		break;

#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_KeySensor:
		InitKeySensor(scene, node);
		break;
	case TAG_X3D_StringSensor:
		InitStringSensor(scene, node);
		break;
#endif

	case TAG_MPEG4_TermCap:
		InitTermCap(scene, node);
		break;

	case TAG_MPEG4_Storage:
		gf_scene_init_storage(scene, node);
		break;

	case TAG_MPEG4_KeyNavigator:
		gf_node_set_callback_function(node, TraverseKeyNavigator);
		gf_node_set_private(node, scene);
		gf_list_add(scene->keynavigators, node);
		((M_KeyNavigator*)node)->on_setFocus = on_kn_set_focus;
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			on_kn_set_focus(NULL, NULL);
		}
#endif
		break;

#endif


#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_title:
		gf_node_set_callback_function(node, svg_traverse_title);
		gf_node_set_private(node, scene);
		break;
#endif

	default:
		gf_sc_on_node_init(scene->compositor, node);
		break;
	}
}

void gf_scene_on_node_modified(void *_is, GF_Node *node)
{
	GF_Scene *scene = (GF_Scene *)_is;
	if (!scene) return;
	if (!node) {
		gf_sc_invalidate(scene->compositor, NULL);
		return;
	}

	switch (gf_node_get_tag(node)) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
#endif
		gf_inline_on_modified(node);
		break;
	case TAG_MPEG4_MediaBuffer:
		break;
	case TAG_MPEG4_MediaControl:
		MC_Modified(node);
		break;
	case TAG_MPEG4_MediaSensor:
		MS_Modified(node);
		break;
	case TAG_MPEG4_InputSensor:
		InputSensorModified(node);
		break;
	case TAG_MPEG4_Conditional:
		break;
	case TAG_MPEG4_Storage:
		break;
#endif
	default:
		gf_sc_invalidate(scene->compositor, node);
		break;
	}
}

static void gf_scene_on_node_destroyed(void *_is, GF_Node *node)
{
	GF_Scene *scene = (GF_Scene *)_is;
	if (!scene) return;
	gf_sc_node_destroy(scene->compositor, node, NULL);
}

GF_EXPORT
void gf_scene_node_callback(void *_is, GF_SGNodeCbkType type, GF_Node *n, void *param)
{
	switch (type) {
	case GF_SG_CALLBACK_MODIFIED:
		gf_scene_on_node_modified(_is, n);
		break;
	case GF_SG_CALLBACK_NODE_DESTROY:
		gf_scene_on_node_destroyed(_is, n);
		break;
	case GF_SG_CALLBACK_INIT:
		gf_scene_on_node_init(_is, n);
		break;
	/*get all inline nodes using this subscene and bubble up...*/
	case GF_SG_CALLBACK_GRAPH_DIRTY:
	{
		u32 i=0;
		GF_Scene *scene = (GF_Scene *)_is;
		if (scene->root_od->mo) {
			GF_Node *root;
			while ((root=(GF_Node*)gf_mo_event_target_enum_node(scene->root_od->mo, &i))) {
				gf_node_dirty_set(root, GF_SG_CHILD_DIRTY, GF_TRUE);
			}
		}
	}
	break;
	}
}

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
