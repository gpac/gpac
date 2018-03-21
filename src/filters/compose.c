/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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
#include <gpac/internal/compositor_dev.h>
#include "../filter_core/filter_session.h"


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
	if (!ctx) return GF_BAD_PARAM;

	count = gf_list_count(ctx->compositor->systems_pids);
	for (i=0; i<count; i++) {
		GF_FilterPacket *pck;
		GF_Err e = GF_EOS;
		GF_FilterPid *pid = gf_list_get(ctx->compositor->systems_pids, i);
		GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);

		assert (odm);

		e = GF_OK;
		pck = gf_filter_pid_get_packet(pid);
		if (!pck && gf_filter_pid_is_eos(pid)) {
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
	u32 mtype, codecid;
	u32 i, count;
	GF_Scene *scene = NULL;
	GF_Scene *top_scene = NULL;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);
	GF_FilterEvent evt;
	Bool in_iod = GF_FALSE;

	if (is_remove) {
		GF_Scene *scene;
		u32 ID=0;
		GF_ObjectManager *odm = gf_filter_pid_get_udta(pid);
		//already disconnected
		if (!odm) return GF_OK;
		ID = odm->ID;
		scene = odm->parentscene;
		if (scene && !scene->is_dynamic_scene)
			scene = NULL;
		//destroy the object
		gf_odm_disconnect(odm, 2);
		if (scene) {
			if (scene->visual_url.OD_ID == ID) {
				scene->visual_url.OD_ID = 0;
				gf_scene_regenerate(scene);
			} else if (scene->audio_url.OD_ID == ID) {
				scene->audio_url.OD_ID = 0;
				gf_scene_regenerate(scene);
			} else if (scene->text_url.OD_ID == ID) {
				scene->text_url.OD_ID = 0;
				gf_scene_regenerate(scene);
			} else if (scene->dims_url.OD_ID == ID) {
				scene->dims_url.OD_ID = 0;
				gf_scene_regenerate(scene);
			}
		}
		return GF_OK;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;
	mtype = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	codecid = prop->value.uint;

	odm = gf_filter_pid_get_udta(pid);
	if (odm) {
		if (mtype==GF_STREAM_SCENE) return GF_OK;
		if (mtype==GF_STREAM_OD) return GF_OK;
		//change of stream type for a given object, no use case yet
		if (odm->type != mtype)
			return GF_NOT_SUPPORTED;
		if (odm->mo) {
			odm->mo->config_changed = GF_TRUE;
			if ((odm->type == GF_STREAM_VISUAL) && odm->parentscene && odm->parentscene->is_dynamic_scene) {
				gf_scene_force_size_to_video(odm->parentscene, odm->mo);
				odm->mo->config_changed = GF_TRUE;
			}
		}
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

	//pure OCR streams are handled by dispatching OCR on the PID(s)
	if (codecid != GF_CODECID_RAW)
		return GF_NOT_SUPPORTED;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
	if (prop && prop->value.boolean) {
		scene->is_dynamic_scene = GF_FALSE;
		in_iod = GF_TRUE;
	}
	if ((mtype==GF_STREAM_OD) && !in_iod) return GF_NOT_SUPPORTED;

	//setup object (clock) and playback requests
	gf_scene_insert_pid(scene, scene->root_od->scene_ns, pid, in_iod);

	//attach scene to input filters - may be true for dynamic scene (text rendering) and regular scenes
	if ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE) ) {
		GF_FEVT_INIT(evt, GF_FEVT_ATTACH_SCENE, pid);
		evt.attach_scene.object_manager = gf_filter_pid_get_udta(pid);
		gf_filter_pid_send_event(pid, &evt);
	}
	//scene is dynamic
	if (scene->is_dynamic_scene) {
		gf_scene_regenerate(scene);
	}

	return GF_OK;
}

static GF_Err compose_reconfig_output(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *p;
	u32 sr, bps, nb_ch, cfg, afmt;
	Bool needs_reconfigure = GF_FALSE;
	GF_CompositorFilter *ctx = (GF_CompositorFilter *) gf_filter_get_udta(filter);

	if (ctx->compositor->vout == pid) return GF_NOT_SUPPORTED;
	if (ctx->compositor->audio_renderer->aout != pid) return GF_NOT_SUPPORTED;

	gf_mixer_get_config(ctx->compositor->audio_renderer->mixer, &sr, &nb_ch, &bps, &cfg);
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p && (p->value.uint != sr)) {
		sr = p->value.uint;
		needs_reconfigure = GF_TRUE;
	}
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p && (p->value.uint != nb_ch)) {
		nb_ch = p->value.uint;
		needs_reconfigure = GF_TRUE;
	}
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_AUDIO_FORMAT);
	afmt = GF_AUDIO_FMT_S16;
	if (bps==8) afmt = GF_AUDIO_FMT_U8;
	else if (bps==24) afmt = GF_AUDIO_FMT_S24;
	else if (bps==32) afmt = GF_AUDIO_FMT_S32;

	if (p && (p->value.uint != afmt)) {
		if (afmt==GF_AUDIO_FMT_S16) bps=16;
		else if (afmt==GF_AUDIO_FMT_U8) bps=8;
		else if (afmt==GF_AUDIO_FMT_S24) bps=24;
		else if (afmt==GF_AUDIO_FMT_S32) bps=32;
		//internal mixer doesn't support other audio formats
		else return GF_NOT_SUPPORTED;

		needs_reconfigure = GF_TRUE;
	}
	if (!needs_reconfigure) return GF_OK;

	GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Compositor] Audio output caps negotiated to %d Hz %d channels %d bps\n", sr, nb_ch, bps));
	gf_mixer_set_config(ctx->compositor->audio_renderer->mixer, sr, nb_ch, bps, 0);
	ctx->compositor->audio_renderer->need_reconfig = GF_TRUE;
	return GF_OK;
}

static Bool compose_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_ObjectManager *odm;

	switch (evt->base.type) {
	case GF_FEVT_INFO_UPDATE:
		odm = gf_filter_pid_get_udta(evt->base.on_pid);
		gf_odm_update_duration(odm, evt->base.on_pid);
		gf_odm_check_clock_mediatime(odm);
		break;
	//event(s) we trigger on ourselves to go up the filter chain
	case GF_FEVT_CAPS_CHANGE:
		return GF_FALSE;
	default:
		break;
	}
	//all events cancelled (play/stop/etc...)
	return GF_TRUE;
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
	GF_FilterPid *pid;
	GF_CompositorFilter *ctx = gf_filter_get_udta(filter);

	ctx->magic = GF_4CC('c','o','m','p');
	ctx->magic_ptr = ctx;

	ctx->compositor = gf_sc_new( gf_fs_get_user( gf_filter_get_session(filter) ) );
	if (!ctx->compositor)
		return GF_SERVICE_ERROR;
	ctx->compositor->no_regulation = GF_TRUE;
	ctx->compositor->filter = filter;
	ctx->compositor->fsess = gf_filter_get_session(filter);

	ctx->compositor->fsess->caps.max_screen_width = ctx->compositor->video_out->max_screen_width;
	ctx->compositor->fsess->caps.max_screen_height = ctx->compositor->video_out->max_screen_height;
	ctx->compositor->fsess->caps.max_screen_bpp = ctx->compositor->video_out->max_screen_bpp;

	//declare video output pid
	pid = ctx->compositor->vout = gf_filter_pid_new(filter);
	gf_filter_pid_set_name(pid, "vout");
	//compositor initiated for RT playback, vout pid may not be connected
	if (! (ctx->compositor->user->init_flags & GF_TERM_NO_DEF_AUDIO_OUT))
		gf_filter_pid_set_loose_connect(pid);

	gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_RGB) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->compositor->output_width) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->compositor->output_height) );

	gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT(ctx->compositor->frame_rate*1000, 1000) );

	//declare audio output pid
	if (! (ctx->compositor->user->init_flags & GF_TERM_NO_AUDIO) && ctx->compositor->audio_renderer) {
		GF_AudioRenderer *ar = ctx->compositor->audio_renderer;
		pid = ar->aout = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, ctx->compositor);
		gf_filter_pid_set_name(pid, "aout");
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(44100) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(44100) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2) );
		gf_filter_pid_set_max_buffer(ar->aout, 1000*ar->total_duration);
	}
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
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
	//we don't accept text streams for now, we only use scene streams for text
	CAP_EXC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE)
};

const GF_FilterRegister CompositorFilterRegister = {
	.name = "compositor",
	.description = "Compositor Filter running the GPAC interactive media compositor",
	.private_size = sizeof(GF_CompositorFilter),
	.requires_main_thread = GF_TRUE,
	.max_extra_pids = (u32) -1,
	INCAPS(CompositorFilterInputs),
	.args = CompositorFilterArgs,
	.initialize = compose_initialize,
	.finalize = compose_finalize,
	.process = compose_process,
	.process_event = compose_process_event,
	.configure_pid = compose_config_input,
	.reconfigure_output = compose_reconfig_output,
	.update_arg = compose_update_arg
};

const GF_FilterRegister *compose_filter_register(GF_FilterSession *session)
{
	return &CompositorFilterRegister;
}



