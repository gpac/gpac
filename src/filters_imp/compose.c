/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / compositor filter
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

#include <gpac/filters.h>
#include <gpac/config_file.h>
#include <gpac/compositor.h>
#include <gpac/internal/terminal_dev.h>


GF_Err compose_bifs_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_bifs_dec_process(GF_Scene *scene, GF_FilterPid *pid);

GF_Err compose_odf_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_odf_dec_process(GF_Scene *scene, GF_FilterPid *pid);

typedef struct
{
	u32 magic;	//must be "comp"
	void *magic_ptr; //must point to this structure

	//FIXME, we need to get rid of this one!
	GF_User user;

	//compositor
	GF_Compositor *compositor;
} GF_CompositorFilter;

//a bit ugly, used by terminal (old APIs)
GF_Compositor *gf_sc_from_filter(GF_Filter *filter)
{
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);
	if (ctx->magic != GF_4CC('c','o','m','p')) return NULL;
	if (ctx->magic_ptr != ctx) return NULL;

	return ctx->compositor;
}

static GF_Err compose_process(GF_Filter *filter)
{
	u32 i, count;
	s32 ms_until_next = 0;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);

	count = gf_list_count(ctx->compositor->systems_pids);
	for (i=0; i<count; i++) {
		GF_FilterPacket *pck;
		GF_Err e = GF_EOS;
		GF_FilterPid *pid = gf_list_get(ctx->compositor->systems_pids, i);
		GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);

		assert (odm);

		e = GF_OK;
		pck = gf_filter_pid_get_packet(pid);
		if (pck && gf_filter_pck_get_eos(pck)) {
			e = GF_EOS;
		}
		if (pck)
			gf_filter_pid_drop_packet(pid);


		if (e==GF_EOS) {
			gf_list_rem(ctx->compositor->systems_pids, i);
			i--;
			count--;
			gf_odm_on_eos(odm, pid);
		}
	}

	gf_sc_draw_frame(ctx->compositor, GF_FALSE, &ms_until_next);

	//to clean up,depending on whether we use a thread to poll user inputs, etc...
	if (ms_until_next > 100)
		ms_until_next = 100;

	//ask for real-time reschedule
	gf_filter_ask_rt_reschedule(filter, ms_until_next ? ms_until_next*1000 : 1);

	return GF_OK;
}

static GF_Err compose_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ObjectManager *odm;
	const GF_PropertyValue *prop;
	u32 mtype, oti;
	u32 i, count;
	GF_Scene *scene = NULL;
	GF_Scene *top_scene = NULL;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);
	GF_FilterEvent evt;
	Bool in_iod = GF_FALSE;

	if (is_remove) {
		GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);
		//already disconnected
		if (!odm) return GF_OK;
		//destroy the object
		gf_odm_disconnect(odm, 2);
		return GF_OK;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;
	mtype = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (!prop) return GF_NOT_SUPPORTED;
	oti = prop->value.uint;

	odm = gf_filter_pid_get_udta(pid);
	if (odm) {
		if (mtype==GF_STREAM_SCENE) return GF_OK;
		if (mtype==GF_STREAM_OD) return GF_OK;
		//change of stream type for a given object, no use case yet
		if (odm->type != mtype)
			return GF_NOT_SUPPORTED;
		if (odm->mo) odm->mo->config_changed = GF_TRUE;
		return GF_OK;
	}

	//create a default scene
	if (!ctx->compositor->root_scene) {
		ctx->compositor->root_scene = gf_scene_new(ctx->compositor, NULL);
		ctx->compositor->root_scene->is_dynamic_scene = GF_TRUE;
		ctx->compositor->root_scene->root_od = gf_odm_new();
		ctx->compositor->root_scene->root_od->scene_ns = gf_scene_ns_new(ctx->compositor->root_scene, ctx->compositor->root_scene->root_od, "test", NULL);
		ctx->compositor->root_scene->root_od->subscene = ctx->compositor->root_scene;
	}

	//default scene is root one
	scene = ctx->compositor->root_scene;
	top_scene = ctx->compositor->root_scene;

	//browse all scene namespaces and figure out our parent scene
	count = gf_list_count(top_scene->namespaces);
	for (i=0; i<count; i++) {
		GF_SceneNamespace *sns = gf_list_get(top_scene->namespaces, i);
		if (!sns->source_filter) continue;
		assert(sns->owner);
		if (gf_filter_pid_is_filter_in_parents(pid, sns->source_filter)) {
			//we are attaching an inline, create the subscene if not done already
			if (!sns->owner->subscene) {
				assert(sns->owner->parentscene);
				sns->owner->subscene = gf_scene_new(ctx->compositor, sns->owner->parentscene);
				sns->owner->subscene->root_od = sns->owner;
			}
			scene = sns->owner->subscene;
			break;
		}
	}
	assert(scene);

	//we have an MPEG-4 ESID defined for the PID, this is MPEG-4 systems
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (prop && scene->is_dynamic_scene) {
		scene->is_dynamic_scene = GF_FALSE;
	}

	//TODO: pure OCR streams
	if (oti != GPAC_OTI_RAW_MEDIA_STREAM)
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	if (prop && prop->value.boolean) in_iod = GF_TRUE;

	if ((mtype==GF_STREAM_OD) && !in_iod) return GF_NOT_SUPPORTED;

	//setup object (clock) and playback requests
	gf_scene_insert_pid(scene, scene->root_od->scene_ns, pid, in_iod);

	//attach scene to input filters
	if ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE) ) {
		GF_FEVT_INIT(evt, GF_FEVT_ATTACH_SCENE, pid);
		evt.attach_scene.object_manager = gf_filter_pid_get_udta(pid);
		gf_filter_pid_send_event(pid, &evt);
	} else if (scene->is_dynamic_scene) {
		gf_scene_regenerate(scene);
	}

	return GF_OK;
}

static GF_Err compose_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	return GF_OK;
}

static void compose_finalize(GF_Filter *filter)
{
	GF_CompositorFilter *ctx = gf_filter_get_udta(filter);

	if (ctx->compositor) {
		gf_sc_set_scene(ctx->compositor, NULL);
		if (ctx->compositor->root_scene) {
			gf_odm_disconnect(ctx->compositor->root_scene->root_od, GF_TRUE);
		}
		gf_sc_del(ctx->compositor);
	}
	if (ctx->user.modules) gf_modules_del(ctx->user.modules);
	if (ctx->user.config) gf_cfg_del(ctx->user.config);
}

GF_Err compose_initialize(GF_Filter *filter)
{
	GF_CompositorFilter *ctx = gf_filter_get_udta(filter);

	ctx->magic = GF_4CC('c','o','m','p');
	ctx->magic_ptr = ctx;

	ctx->compositor = gf_sc_new( gf_fs_get_user( gf_filter_get_session(filter) ) );
	if (!ctx->compositor)
		return GF_SERVICE_ERROR;
	ctx->compositor->no_regulation = GF_TRUE;
	ctx->compositor->filter = filter;
	ctx->compositor->fsess = gf_filter_get_session(filter);
	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(GF_CompositorFilter, _n)
static const GF_FilterArgs CompositorFilterArgs[] =
{
	//example uint option using enum, result parsed ranges from 0(=v1) to 2(=v3)
	{ NULL }
};

static const GF_FilterCapability CompositorFilterInputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_SCENE)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM ), GF_FALSE},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_OD), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM ), GF_FALSE},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM ) },

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM ) },

	{ NULL }
};

const GF_FilterRegister CompositorFilterRegister = {
	.name = "compositor",
	.description = "Compositor Filter running the GPAC interactive media compositor. Sink filter for now",
	.private_size = sizeof(GF_CompositorFilter),
	.requires_main_thread = GF_TRUE,
	.max_extra_pids = (u32) -1,
	.input_caps = CompositorFilterInputs,
	.args = CompositorFilterArgs,
	.initialize = compose_initialize,
	.finalize = compose_finalize,
	.process = compose_process,
	.configure_pid = compose_config_input,
	.update_arg = compose_update_arg
};

const GF_FilterRegister *compose_filter_register(GF_FilterSession *session)
{
	return &CompositorFilterRegister;
}



