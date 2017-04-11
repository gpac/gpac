/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
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
#include <gpac/user.h>


GF_Err compose_bifs_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_bifs_dec_process(GF_Scene *scene, GF_FilterPid *pid);

GF_Err compose_odf_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_odf_dec_process(GF_Scene *scene, GF_FilterPid *pid);

typedef struct
{
	//FIXME, we need to get rid of this one!
	GF_User user;

	//compositor
	GF_Compositor *compositor;
	GF_Scene *scene;
} GF_CompositorFilter;


static GF_Err compose_process(GF_Filter *filter)
{
	u32 i, count;
	u32 ms_sys_wait = 0;
	s32 ms_until_next=0;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);

	ctx->compositor->ms_until_next_frame = 0;

	count = gf_list_count(ctx->compositor->systems_pids);
	for (i=0; i<count; i++) {
		GF_Err e = GF_EOS;
		u32 oti=0, mtype=0;
		const GF_PropertyValue *prop;
		GF_FilterPid *pid = gf_list_get(ctx->compositor->systems_pids, i);
		GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);
		GF_Scene *scene;
		assert (odm);
		
		scene = odm->subscene ? odm->subscene : odm->parentscene;

		//TODO clean that by getting rid of OTI
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
		assert(prop);
		mtype = prop->value.uint;

		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
		assert(prop);
		oti = prop->value.uint;

		//that's a main PID
		if (mtype==GF_STREAM_SCENE) {
			if ((oti==GPAC_OTI_SCENE_BIFS) || (oti==GPAC_OTI_SCENE_BIFS_V2)) {

				e = compose_bifs_dec_process(scene, pid);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("Unknwon OTI %d for BIFS stream type\n", oti));
			}
		} else if (mtype==GF_STREAM_OD) {
			e = compose_odf_dec_process(scene, pid);
		}

		if (e==GF_EOS) {
			gf_list_rem(ctx->compositor->systems_pids, i);
			i--;
			count--;
			gf_odm_on_eos(odm, pid);
		}
	}
	ms_sys_wait = ctx->compositor->ms_until_next_frame;
	ctx->compositor->ms_until_next_frame = 0;

	gf_sc_draw_frame(ctx->compositor, GF_FALSE, &ms_until_next);
	if (ms_sys_wait && (ms_until_next>ms_sys_wait))
		ms_until_next = ms_sys_wait;

	//ask for real-time reschedule
	gf_filter_ask_rt_reschedule(filter, ms_until_next*1000);

	return GF_OK;
}

static GF_Err compose_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ObjectManager *odm;
	const GF_PropertyValue *prop;
	u32 mtype, oti;
	GF_Err e;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);

	if (is_remove)
		return GF_NOT_SUPPORTED;

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

	if ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE) ) {
		GF_Scene *scene = NULL;
		//create a default scene
		if (!ctx->scene) {
			ctx->scene = gf_scene_new(ctx->compositor, NULL);
			ctx->scene->is_dynamic_scene = GF_FALSE;
			ctx->scene->root_od = gf_odm_new();
			ctx->scene->root_od->scene_ns = gf_scene_ns_new(ctx->scene, ctx->scene->root_od, "test", NULL);
			ctx->scene->root_od->subscene = ctx->scene;
		}
		//todo for inline
		scene = ctx->scene;

		if  (mtype==GF_STREAM_SCENE) {
			if ((oti==GPAC_OTI_SCENE_BIFS) || (oti==GPAC_OTI_SCENE_BIFS_V2)) {
				e = compose_bifs_dec_config_input(scene, pid, oti, GF_FALSE);
				if (e) return e;
			} else {
				return GF_NOT_SUPPORTED;
			}
		} else if (mtype==GF_STREAM_OD) {
			e = compose_odf_dec_config_input(scene, pid, oti, GF_FALSE);
			if (e) return e;
		} else {
			return GF_NOT_SUPPORTED;
		}
		//TODO
		return GF_OK;
	} else {
		if (oti != GPAC_OTI_RAW_MEDIA_STREAM)
			return GF_NOT_SUPPORTED;
		//create a default scene
		if (!ctx->scene) {
			ctx->scene = gf_scene_new(ctx->compositor, NULL);
			ctx->scene->is_dynamic_scene = GF_TRUE;
			ctx->scene->root_od = gf_odm_new();
			ctx->scene->root_od->scene_ns = gf_scene_ns_new(ctx->scene, ctx->scene->root_od, "test", NULL);
			ctx->scene->root_od->subscene = ctx->scene;
		}
		gf_scene_insert_pid(ctx->scene, ctx->scene->root_od->scene_ns, pid);
		gf_scene_regenerate(ctx->scene);

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
		if (ctx->scene) {
			gf_odm_disconnect(ctx->scene->root_od, GF_TRUE);
		}
		gf_sc_del(ctx->compositor);
	}
	if (ctx->user.modules) gf_modules_del(ctx->user.modules);
	if (ctx->user.config) gf_cfg_del(ctx->user.config);
}

Bool compose_EventProc(void *ptr, GF_Event *evt)
{
	if (evt->type==GF_EVENT_QUIT) {
		gf_filter_session_abort(gf_filter_get_session( (GF_Filter *)ptr ), GF_EOS);
	}
	return 0;
}

GF_Err compose_initialize(GF_Filter *filter)
{
	u32 count=0;
	GF_PropertyValue p;
	GF_CompositorFilter *ctx = gf_filter_get_udta(filter);


	ctx->user.config = gf_cfg_init(NULL, NULL);
	if (!ctx->user.config) return GF_SERVICE_ERROR;
	ctx->user.modules = gf_modules_new(NULL, ctx->user.config);
	if (ctx->user.modules) count = gf_modules_get_count(ctx->user.modules);
	if (!count || !ctx->user.modules) {
		fprintf(stderr, "Error: no modules found - cannot load compositor\n");
		return GF_SERVICE_ERROR;
	}
	ctx->user.EventProc = compose_EventProc;
	ctx->user.opaque = filter;
	ctx->user.init_flags = GF_TERM_NO_REGULATION;
	
	ctx->compositor = gf_sc_new(&ctx->user);
	if (!ctx->compositor) return GF_SERVICE_ERROR;
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
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_SCENE_BIFS ), GF_FALSE},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_SCENE), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_SCENE_BIFS_V2 ), GF_FALSE},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_OD), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_OD_V1 ), GF_FALSE},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_OD), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_OD_V2 ), GF_FALSE},

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
	.input_caps = CompositorFilterInputs,
	.args = CompositorFilterArgs,
	.initialize = compose_initialize,
	.finalize = compose_finalize,
	.process = compose_process,
	.configure_pid = compose_config_input,
	.update_arg = compose_update_arg
};

const GF_FilterRegister *compose_filter_register(GF_FilterSession *session, Bool load_meta_filters)
{
	return &CompositorFilterRegister;
}



