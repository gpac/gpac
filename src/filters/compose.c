/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
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
//to set caps in filter session, to cleanup!
#include "../filter_core/filter_session.h"

#ifndef GPAC_DISABLE_PLAYER

GF_Err compose_bifs_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_bifs_dec_process(GF_Scene *scene, GF_FilterPid *pid);

GF_Err compose_odf_dec_config_input(GF_Scene *scene, GF_FilterPid *pid, u32 oti, Bool is_remove);
GF_Err compose_odf_dec_process(GF_Scene *scene, GF_FilterPid *pid);


static GF_Err compose_process(GF_Filter *filter)
{
	u32 i, nb_sys_streams_active;
	s32 ms_until_next = 0;
	Bool ret;
	GF_Compositor *ctx = (GF_Compositor *) gf_filter_get_udta(filter);
	if (!ctx) return GF_BAD_PARAM;
	if (!ctx->vout) return GF_OK;

	if (ctx->check_eos_state == 2)
		return GF_EOS;

	/*need to reload*/
	if (ctx->reload_state == 1) {
		ctx->reload_state = 0;
		gf_sc_disconnect(ctx);
		ctx->reload_state = 2;
	}
	if (ctx->reload_state == 2) {
		if (!ctx->root_scene) {
			ctx->reload_state = 0;
			if (ctx->reload_url) {
				gf_sc_connect_from_time(ctx, ctx->reload_url, 0, 0, 0, NULL);
				gf_free(ctx->reload_url);
				ctx->reload_url = NULL;
			}
		}
	}

	ctx->last_error = GF_OK;
	if (ctx->reload_config) {
		ctx->reload_config = GF_FALSE;
		gf_sc_reload_config(ctx);
	}

	nb_sys_streams_active = gf_list_count(ctx->systems_pids);
	for (i=0; i<nb_sys_streams_active; i++) {
		GF_FilterPacket *pck;
		GF_Err e;
		GF_FilterPid *pid = gf_list_get(ctx->systems_pids, i);
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
			gf_list_rem(ctx->systems_pids, i);
			i--;
			nb_sys_streams_active--;
			gf_odm_on_eos(odm, pid);
		}
		if (ctx->reload_scene_size) {
			u32 w, h;
			gf_sg_get_scene_size_info(ctx->root_scene->graph, &w, &h);
			if ((ctx->scene_width!=w) || (ctx->scene_height!=h)) {
				gf_sc_set_scene_size(ctx, w, h, GF_TRUE);
			}
		}
	}

	ret = gf_sc_draw_frame(ctx, GF_FALSE, &ms_until_next);

	if (!ctx->player) {
		Bool forced_eos = GF_FALSE;
		Bool was_over = GF_FALSE;
		/*remember to check for eos*/
		if (ctx->dur<0) {
			if (ctx->frame_number >= (u32) -ctx->dur)
				ctx->check_eos_state = 2;
		} else if (ctx->dur>0) {
			Double n = ctx->scene_sampled_clock;
			n /= 1000;
			if (n>=ctx->dur)
				ctx->check_eos_state = 2;
			else if (!ret && ctx->vfr && !ctx->check_eos_state && !nb_sys_streams_active && ctx->scene_sampled_clock && !ctx->validator_mode) {
				ctx->check_eos_state = 1;
				if (!ctx->validator_mode)
					ctx->force_next_frame_redraw = GF_TRUE;
			}
		} else if (!ret && !ctx->frame_was_produced && !ctx->audio_frames_sent && !ctx->check_eos_state && !nb_sys_streams_active) {
			ctx->check_eos_state = 1;
			was_over = GF_TRUE;
		} else if (ctx->sys_frames_pending) {
			ctx->check_eos_state = 0;
		}

		if (ctx->timeout && (ctx->check_eos_state == 1) && !gf_filter_connections_pending(filter)) {
			u32 now = gf_sys_clock();
			if (!ctx->last_check_pass)
				ctx->last_check_pass = now;

			if (now - ctx->last_check_pass > ctx->timeout) {
				ctx->check_eos_state = 2;
				if (!gf_filter_end_of_session(filter)) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Could not detect end of stream(s) in the %d ms, aborting\n", ctx->timeout));
					forced_eos = GF_TRUE;
				}
				gf_filter_abort(filter);
			}
		} else {
			ctx->last_check_pass = 0;
		}

		if ((ctx->check_eos_state==2) || (ctx->check_eos_state && gf_sc_check_end_of_scene(ctx, GF_TRUE))) {
			u32 count;
			ctx->force_next_frame_redraw = GF_FALSE;
			count = gf_filter_get_ipid_count(ctx->filter);
			if (ctx->root_scene) {
				gf_filter_pid_set_eos(ctx->vout);
				if (ctx->audio_renderer && ctx->audio_renderer->aout)
					gf_filter_pid_set_eos(ctx->audio_renderer->aout);
			}
			//send stop
			if (ctx->dur) {
				for (i=0; i<count; i++) {
					GF_FilterPid *pid = gf_filter_get_ipid(ctx->filter, i);
					if (!gf_filter_pid_is_eos(pid)) {
						GF_FilterEvent evt;
						GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
						gf_filter_pid_send_event(pid, &evt);
						GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
						gf_filter_pid_send_event(pid, &evt);
						//and discard every incoming packet
						gf_filter_pid_set_discard(pid, GF_TRUE);
					}
				}
			}
			return forced_eos ? GF_SERVICE_ERROR : GF_EOS;
		}
		ctx->check_eos_state = was_over ? 1 : 0;
		//always repost a process task since we maye have things to draw even though no new input
		gf_filter_post_process_task(filter);
		return ctx->last_error;
	}

	//player mode
	
	//quit event seen, do not flush, just abort and return last error
	if (ctx->check_eos_state) {
		gf_filter_abort(filter);
		return ctx->last_error ? ctx->last_error : GF_EOS;
	}


	//to clean up,depending on whether we use a thread to poll user inputs, etc...
	if ((u32) ms_until_next > 100)
		ms_until_next = 100;

	//ask for real-time reschedule
	gf_filter_ask_rt_reschedule(filter, ms_until_next ? ms_until_next*1000 : 1);

	return ctx->last_error;
}

static void merge_properties(GF_Compositor *ctx, GF_FilterPid *pid, u32 mtype, GF_Scene *parent_scene)
{
	const GF_PropertyValue *p;
	if (!ctx->vout) return;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
	if (!p) return;

	if (mtype==GF_STREAM_SCENE) {
		if (!parent_scene || !parent_scene->is_dynamic_scene) {
			gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_URL, p);
		}
	} else if (parent_scene && parent_scene->is_dynamic_scene) {
		if (mtype==GF_STREAM_VISUAL)
			gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_URL, p);
	}
}

static void compositor_setup_vout(GF_Compositor *ctx)
{
	//declare video output pid
	GF_FilterPid *pid;
	pid = ctx->vout = gf_filter_pid_new(ctx->filter);
	gf_filter_pid_set_name(pid, "vout");
	//compositor initiated for RT playback, vout pid may not be connected
	gf_filter_pid_set_loose_connect(pid);

	gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL) );
	if (ctx->timescale)
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale) );
	else
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->fps.num) );

	gf_filter_pid_set_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->opfmt ? ctx->opfmt : GF_PIXEL_RGB) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->output_width) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->output_height) );

	gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC(ctx->fps) );
	gf_filter_pid_set_property(pid, GF_PROP_PID_DELAY, NULL);
}

static GF_Err compose_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_ObjectManager *odm;
	const GF_PropertyValue *prop;
	u32 mtype, codecid;
	u32 i, count;
	GF_Scene *def_scene = NULL;
	GF_Scene *scene = NULL;
	GF_Scene *top_scene = NULL;
	GF_Compositor *ctx = (GF_Compositor *) gf_filter_get_udta(filter);
	GF_FilterEvent evt;
	Bool in_iod = GF_FALSE;
	Bool was_dyn_scene = GF_FALSE;
	if (is_remove) {
		u32 ID=0;
		odm = gf_filter_pid_get_udta(pid);
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
			} else if (scene->subs_url.OD_ID == ID) {
				scene->subs_url.OD_ID = 0;
				gf_scene_regenerate(scene);
			}
		}
		return GF_OK;
	}

	if (!ctx->vout) {
		compositor_setup_vout(ctx);
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!prop) return GF_NOT_SUPPORTED;
	mtype = prop->value.uint;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!prop) return GF_NOT_SUPPORTED;
	codecid = prop->value.uint;

	odm = gf_filter_pid_get_udta(pid);

	//in filter mode, check we can handle creating a canvas from input video format. If not, negociate a supported format
	if (!ctx->player) {
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
		if (prop && (!odm || (odm->mo && (odm->mo->pixelformat != prop->value.uint)))) {
			GF_EVGSurface *test_c = gf_evg_surface_new(GF_FALSE);
			GF_Err e = gf_evg_surface_attach_to_buffer(test_c, (u8 *) prop, 48, 48, 0, 0, prop->value.uint);
			gf_evg_surface_delete(test_c);
			if (e) {
				u32 new_fmt;
				Bool transparent = gf_pixel_fmt_is_transparent(prop->value.uint);
				if (gf_pixel_fmt_is_yuv(prop->value.uint)) {
					if (!transparent && gf_pixel_is_wide_depth(prop->value.uint)>8) {
						new_fmt = GF_PIXEL_YUV444_10;
					} else {
						new_fmt = transparent ? GF_PIXEL_YUVA444_PACK : GF_PIXEL_YUV444_PACK;
					}
				} else {
					new_fmt = transparent ? GF_PIXEL_RGBA : GF_PIXEL_RGB;
				}
				gf_filter_pid_negociate_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(new_fmt) );
				return GF_OK;
			}
		}
	}

	if (odm) {
		Bool notify_quality = GF_FALSE;

		if (gf_filter_pid_is_sparse(pid)) {
			odm->flags |= GF_ODM_IS_SPARSE;
		} else {
			odm->flags &= ~GF_ODM_IS_SPARSE;
		}

		if (mtype==GF_STREAM_SCENE) { }
		else if (mtype==GF_STREAM_OD) { }
		//change of stream type for a given object, no use case yet
		else {
			if (odm->type != mtype)
				return GF_NOT_SUPPORTED;
			if (odm->mo) {
				odm->mo->config_changed = GF_TRUE;
				gf_mo_update_caps_ex(odm->mo, GF_TRUE);
				if (odm->mo->config_changed && (odm->type == GF_STREAM_VISUAL) && odm->parentscene && odm->parentscene->is_dynamic_scene) {
					gf_scene_force_size_to_video(odm->parentscene, odm->mo);
				}
			}
			gf_odm_update_duration(odm, pid);
			//we can safely call this here since we are in reconfigure
			gf_odm_check_clock_mediatime(odm);
			notify_quality = GF_TRUE;
		}
		merge_properties(ctx, pid, mtype, odm->parentscene);

		if (notify_quality) {
			GF_Event gevt;
			memset(&gevt, 0, sizeof(GF_Event));
			gevt.type = GF_EVENT_QUALITY_SWITCHED;
			gf_filter_forward_gf_event(filter, &gevt, GF_FALSE, GF_FALSE);
		}

		return GF_OK;
	}

	//create a default scene
	if (!ctx->root_scene) {
		const char *service_url = "unknown";
		const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (p) service_url = p->value.string;
		
		ctx->root_scene = gf_scene_new(ctx, NULL);
		ctx->root_scene->root_od = gf_odm_new();
		ctx->root_scene->root_od->scene_ns = gf_scene_ns_new(ctx->root_scene, ctx->root_scene->root_od, service_url, NULL);
		ctx->root_scene->root_od->subscene = ctx->root_scene;
		ctx->root_scene->root_od->scene_ns->nb_odm_users++;
		switch (mtype) {
		case GF_STREAM_SCENE:
		case GF_STREAM_PRIVATE_SCENE:
		case GF_STREAM_OD:
			ctx->root_scene->is_dynamic_scene = GF_FALSE;
			break;
		default:
			ctx->root_scene->is_dynamic_scene = GF_TRUE;
			break;
		}

		if (!ctx->root_scene->root_od->scene_ns->url_frag) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_FRAG_URL);
			if (p && p->value.string)
				ctx->root_scene->root_od->scene_ns->url_frag = gf_strdup(p->value.string);
		}

		if (!ctx->player)
			gf_filter_post_process_task(filter);
	}

	//default scene is root one
	scene = ctx->root_scene;
	top_scene = ctx->root_scene;

	switch (mtype) {
	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
		prop = gf_filter_pid_get_property(pid, GF_PROP_PID_IN_IOD);
		if (prop && prop->value.boolean) {
			in_iod = GF_TRUE;
		}
		break;
	}


	//browse all scene namespaces and figure out our parent scene
	count = gf_list_count(top_scene->namespaces);
	for (i=0; i<count; i++) {
		GF_SceneNamespace *sns = gf_list_get(top_scene->namespaces, i);
		if (!sns->source_filter) {
			if (sns->connect_ack && sns->owner && !def_scene) {
				def_scene = sns->owner->subscene ? sns->owner->subscene : sns->owner->parentscene;
			}
			continue;
		}
		assert(sns->owner);
		if (gf_filter_pid_is_filter_in_parents(pid, sns->source_filter)) {
			Bool scene_setup = GF_FALSE;
			if (!sns->owner->subscene && sns->owner->parentscene && (mtype!=GF_STREAM_OD) && (mtype!=GF_STREAM_SCENE)) {
				u32 j;
				for (j=0; j<gf_list_count(sns->owner->parentscene->scene_objects); j++) {
					GF_MediaObject *mo = gf_list_get(sns->owner->parentscene->scene_objects, j);
					if (mo->OD_ID == GF_MEDIA_EXTERNAL_ID) continue;
					if (mo->OD_ID != sns->owner->ID) continue;

					if (mo->type != GF_MEDIA_OBJECT_SCENE) continue;
					//this is a pid from a subservice (inline) inserted through OD commands, create the subscene
					sns->owner->subscene = gf_scene_new(NULL, sns->owner->parentscene);
					sns->owner->subscene->root_od = sns->owner;
					//scenes are by default dynamic
					sns->owner->subscene->is_dynamic_scene = GF_TRUE;
					sns->owner->mo = mo;
					mo->odm = sns->owner;
					break;
				}
			}
			//this is an animation stream
			if (!in_iod && ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE)) ) {
				scene_setup = GF_TRUE;
			}
			//otherwise if parent scene is setup and root object type is scene or OD, do not create an inline
			//inline nodes using od:// must trigger subscene creation by using gf_scene_get_media_object with object type GF_MEDIA_OBJECT_SCENE
			else if (sns->owner->parentscene
				&& sns->owner->parentscene->root_od
				&& sns->owner->parentscene->root_od->pid
				&& ((sns->owner->parentscene->root_od->type==GF_STREAM_SCENE) || (sns->owner->parentscene->root_od->type==GF_STREAM_OD))
			) {
				scene_setup = GF_TRUE;
			}

			//we are attaching an inline, create the subscene if not done already
			if (!scene_setup && !sns->owner->subscene && ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE))  ) {
				//ignore system PIDs from subservice - this is typically the case when playing a bt/xmt file
				//created from a container (mp4) and still referring to that container for the media streams
				if (sns->owner->ignore_sys) {
					GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
					gf_filter_pid_send_event(pid, &evt);
					GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
					gf_filter_pid_send_event(pid, &evt);
					return GF_OK;
				}

				assert(sns->owner->parentscene);
				sns->owner->subscene = gf_scene_new(ctx, sns->owner->parentscene);
				sns->owner->subscene->root_od = sns->owner;
			}
			scene = sns->owner->subscene ? sns->owner->subscene : sns->owner->parentscene;
			break;
		}
	}
	if (!scene) scene = def_scene;
	assert(scene);

	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Configuring PID %s\n", gf_stream_type_name(mtype)));

	was_dyn_scene = scene->is_dynamic_scene;

	//pure OCR streams are handled by dispatching OCR on the PID(s)
	if (codecid != GF_CODECID_RAW)
		return GF_NOT_SUPPORTED;

	switch (mtype) {
	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
		if (in_iod) {
			scene->is_dynamic_scene = GF_FALSE;
		} else {
			//we have an MPEG-4 ESID defined for the PID, this is MPEG-4 systems
			prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
			if (prop && scene->is_dynamic_scene) {
				scene->is_dynamic_scene = GF_FALSE;
			}
		}
		break;
	}

	if ((mtype==GF_STREAM_OD) && !in_iod) return GF_NOT_SUPPORTED;

	//we inserted a root scene (bt/svg/...) after a pid (passthrough mode), we need to create a new namespace for
	//the scene and reassign the old namespace to the previously created ODM
	if (scene->root_od && !scene->root_od->parentscene && was_dyn_scene && (was_dyn_scene != scene->is_dynamic_scene)) {
		GF_SceneNamespace *new_sns=NULL;
		const char *service_url = "unknown";
		const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_URL);
		if (p) service_url = p->value.string;
		new_sns = gf_scene_ns_new(ctx->root_scene, ctx->root_scene->root_od, service_url, NULL);

		for (i=0; i<gf_list_count(scene->resources); i++) {
			GF_ObjectManager *anodm = gf_list_get(scene->resources, i);

			if (new_sns && (anodm->scene_ns == scene->root_od->scene_ns) && (scene->root_od->scene_ns->owner==scene->root_od)) {
				scene->root_od->scene_ns->owner = anodm;
				break;
			}
		}
		scene->root_od->scene_ns = new_sns;
		gf_sc_set_scene(ctx, NULL);
		gf_sg_reset(scene->graph);
		gf_sc_set_scene(ctx, scene->graph);
		//do not reload scene size in GUI mode, let the gui decide
		if (ctx->player<2)
			ctx->reload_scene_size = GF_TRUE;
		//force clock to NULL, will resetup based on OCR_ES_IDs
		scene->root_od->ck = NULL;
	}

	//setup object (clock) and playback requests
	gf_scene_insert_pid(scene, scene->root_od->scene_ns, pid, in_iod);

	if (was_dyn_scene != scene->is_dynamic_scene) {
		for (i=0; i<gf_list_count(scene->resources); i++) {
			GF_ObjectManager *anodm = gf_list_get(scene->resources, i);
			if (anodm->mo)
				anodm->flags |= GF_ODM_PASSTHROUGH;
		}
	}


	//attach scene to input filters - may be true for dynamic scene (text rendering) and regular scenes
	if ((mtype==GF_STREAM_OD) || (mtype==GF_STREAM_SCENE) || (mtype==GF_STREAM_TEXT) ) {
		void gf_filter_pid_exec_event(GF_FilterPid *pid, GF_FilterEvent *evt);

		GF_FEVT_INIT(evt, GF_FEVT_ATTACH_SCENE, pid);
		evt.attach_scene.object_manager = gf_filter_pid_get_udta(pid);
		gf_filter_pid_exec_event(pid, &evt);
	}
	//scene is dynamic
	if (scene->is_dynamic_scene) {
		Bool reset = GF_FALSE;
		u32 scene_vr_type = 0;
		char *sep = scene->root_od->scene_ns->url_frag;
		if (sep && ( !strnicmp(sep, "LIVE360", 7) || !strnicmp(sep, "360", 3) || !strnicmp(sep, "VR", 2) ) ) {
			scene_vr_type = 1;
		}
		if (!sep) {
			prop = gf_filter_pid_get_property(pid, GF_PROP_PID_PROJECTION_TYPE);
			if (prop && (prop->value.uint==GF_PROJ360_EQR)) scene_vr_type = 1;
		}
		if (scene_vr_type) {
			if (scene->vr_type != scene_vr_type) reset = GF_TRUE;
			scene->vr_type = scene_vr_type;
		}
		if (reset)
			gf_sg_reset(scene->graph);

		gf_scene_regenerate(scene);

		if (!ctx->player)
			gf_filter_pid_set_property_str(ctx->vout, "InteractiveScene", scene_vr_type ? &PROP_UINT(2) : NULL);
	}
	else if (!ctx->player)
		gf_filter_pid_set_property_str(ctx->vout, "InteractiveScene", &PROP_UINT(1));

	merge_properties(ctx, pid, mtype, scene);
	return GF_OK;
}

#include "../compositor/visual_manager.h"

static GF_Err compose_reconfig_output(GF_Filter *filter, GF_FilterPid *pid)
{
	const GF_PropertyValue *p;
	u32 sr, o_fmt, nb_ch, afmt;
	u64 cfg;
	Bool needs_reconfigure = GF_FALSE;
	GF_Compositor *ctx = (GF_Compositor *) gf_filter_get_udta(filter);

	if (ctx->vout == pid) {
		u32 w, h;
		p = gf_filter_pid_caps_query(pid, GF_PROP_PID_PIXFMT);
		if (p) {
			u32 stride;
#ifndef GPAC_DISABLE_3D
			if (ctx->scene && (ctx->hybrid_opengl || ctx->visual->type_3d)) {
				switch (p->value.uint) {
				case GF_PIXEL_RGBA:
				case GF_PIXEL_RGB:
					break;
				default:
					return GF_NOT_SUPPORTED;
				}
			}
#endif
			if (ctx->opfmt != p->value.uint) {
				ctx->opfmt = p->value.uint;
				gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->opfmt) );
				gf_pixel_get_size_info(ctx->opfmt, ctx->display_width, ctx->display_height, NULL, &stride, NULL, NULL, NULL);
				gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_STRIDE, &PROP_UINT(stride) );
				if (!ctx->player) {
					ctx->new_width = ctx->display_width;
					ctx->new_height = ctx->display_height;
					ctx->msg_type |= GF_SR_CFG_INITIAL_RESIZE;
				}
			}
		}
		
		w = h = 0;
		p = gf_filter_pid_caps_query(pid, GF_PROP_PID_WIDTH);
		if (p) w = p->value.uint;
		p = gf_filter_pid_caps_query(pid, GF_PROP_PID_HEIGHT);
		if (p) h = p->value.uint;

		if (w && h) {
			ctx->osize.x = w;
			ctx->osize.y = h;
/*			gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_WIDTH, &PROP_UINT(w) );
			gf_filter_pid_set_property(ctx->vout, GF_PROP_PID_HEIGHT, &PROP_UINT(h) );
*/		}
		return GF_OK;
	}

	if (ctx->audio_renderer->aout == pid) {

		gf_mixer_get_config(ctx->audio_renderer->mixer, &sr, &nb_ch, &o_fmt, &cfg);
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
		if (p) afmt = p->value.uint;
		else afmt = GF_AUDIO_FMT_S16;

		if (o_fmt != afmt) {
			needs_reconfigure = GF_TRUE;
		}
		if (!needs_reconfigure) return GF_OK;

		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Compositor] Audio output caps negotiated to %d Hz %d channels %s \n", sr, nb_ch, gf_audio_fmt_name(afmt) ));
		gf_mixer_set_config(ctx->audio_renderer->mixer, sr, nb_ch, afmt, 0);
		ctx->audio_renderer->need_reconfig = GF_TRUE;
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static Bool compose_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	switch (evt->base.type) {
	//event(s) we trigger on ourselves to go up the filter chain
	case GF_FEVT_CAPS_CHANGE:
		return GF_FALSE;
	case GF_FEVT_CONNECT_FAIL:
	{
		GF_Compositor *ctx = (GF_Compositor *) gf_filter_get_udta(filter);
		if (ctx->audio_renderer && (evt->base.on_pid == ctx->audio_renderer->aout))
			ctx->audio_renderer->non_rt_output = 0;
	}
		return GF_FALSE;
	case GF_FEVT_BUFFER_REQ:
		return GF_TRUE;
		
	case GF_FEVT_INFO_UPDATE:
	{
		u32 bps=0;
		u64 tot_size=0, down_size=0;
		GF_ObjectManager *odm = gf_filter_pid_get_udta(evt->base.on_pid);
		GF_PropertyEntry *pe=NULL;
		GF_PropertyValue *p = (GF_PropertyValue *) gf_filter_pid_get_info(evt->base.on_pid, GF_PROP_PID_TIMESHIFT_STATE, &pe);
		if (p && p->value.uint) {
			GF_Event an_evt;
			memset(&an_evt, 0, sizeof(GF_Event));
			GF_Compositor *ctx = (GF_Compositor *) gf_filter_get_udta(filter);
			if (p->value.uint==1) {
				an_evt.type = GF_EVENT_TIMESHIFT_UNDERRUN;
				gf_sc_send_event(ctx, &an_evt);
			} else if (p->value.uint==2) {
				an_evt.type = GF_EVENT_TIMESHIFT_OVERFLOW;
				gf_sc_send_event(ctx, &an_evt);
			}
			p->value.uint = 0;
		}

		p = (GF_PropertyValue *) gf_filter_pid_get_info(evt->base.on_pid, GF_PROP_PID_DOWN_RATE, &pe);
		if (p) bps = p->value.uint;
		p = (GF_PropertyValue *) gf_filter_pid_get_info(evt->base.on_pid, GF_PROP_PID_DOWN_SIZE, &pe);
		if (p) tot_size = p->value.longuint;

		p = (GF_PropertyValue *) gf_filter_pid_get_info(evt->base.on_pid, GF_PROP_PID_DOWN_BYTES, &pe);
		if (p) down_size = p->value.longuint;

		if (bps && down_size && tot_size)  {
			odm = gf_filter_pid_get_udta(evt->base.on_pid);
			if ((down_size!=odm->last_filesize_signaled) || (down_size != tot_size)) {
				odm->last_filesize_signaled = down_size;
				gf_odm_service_media_event_with_download(odm, GF_EVENT_MEDIA_PROGRESS, down_size, tot_size, bps/8, 0, 0);
			}
		}
		gf_filter_release_property(pe);
	}
		return GF_TRUE;

	case GF_FEVT_USER:
		return gf_sc_user_event(gf_filter_get_udta(filter), (GF_Event *) &evt->user_event.event);

	//handle play for non-player mode, dynamic scenes only
	case GF_FEVT_PLAY:
	{
		GF_Compositor *compositor = gf_filter_get_udta(filter);
		s32 diff = (s32) (evt->play.start_range*1000);
		diff -= (s32) gf_sc_get_time_in_ms(compositor);
		if (!compositor->player && compositor->root_scene->is_dynamic_scene && !evt->play.initial_broadcast_play
			&& (abs(diff)>=1000)
		) {
			gf_sc_play_from_time(compositor, evt->play.start_range*1000, GF_FALSE);
		}
	}
		break;
	//handle stop for non-player mode, dynamic scenes only
	case GF_FEVT_STOP:
	{
		GF_Compositor *compositor = gf_filter_get_udta(filter);
		if (!compositor->player && compositor->root_scene->is_dynamic_scene && !evt->play.initial_broadcast_play) {
			if (compositor->root_scene->is_dynamic_scene) {
				u32 i, count = gf_list_count(compositor->root_scene->resources);
				for (i=0; i<count; i++) {
					GF_ObjectManager *odm = gf_list_get(compositor->root_scene->resources, i);
					gf_odm_stop(odm, GF_TRUE);
				}
			} else {
				gf_odm_stop(compositor->root_scene->root_od, 1);
			}
		}
	}
		break;

	default:
		break;
	}
	//all events cancelled (play/stop/etc...)
	return GF_TRUE;
}

static GF_Err compose_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	GF_Compositor *compositor = gf_filter_get_udta(filter);
	compositor->reload_config = GF_TRUE;
	return GF_OK;
}

static void compose_finalize(GF_Filter *filter)
{
	GF_Compositor *ctx = gf_filter_get_udta(filter);

	if (ctx) {
		gf_sc_set_scene(ctx, NULL);
		if (ctx->root_scene) {
			gf_odm_disconnect(ctx->root_scene->root_od, GF_TRUE);
		}
		gf_sc_unload(ctx);
	}
}
void compositor_setup_aout(GF_Compositor *ctx)
{
	if (!ctx->noaudio && ctx->audio_renderer && !ctx->audio_renderer->aout) {
		GF_FilterPid *pid = ctx->audio_renderer->aout = gf_filter_pid_new(ctx->filter);
		gf_filter_pid_set_udta(pid, ctx);
		gf_filter_pid_set_name(pid, "aout");
		gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(44100) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(44100) );
		gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(2) );
		gf_filter_pid_set_max_buffer(ctx->audio_renderer->aout, 1000*ctx->abuf);
		gf_filter_pid_set_property(pid, GF_PROP_PID_DELAY, NULL);
		gf_filter_pid_set_loose_connect(pid);
	}
}

static GF_Err compose_initialize(GF_Filter *filter)
{
	GF_Err e;
	GF_FilterSessionCaps sess_caps;
	GF_Compositor *ctx = gf_filter_get_udta(filter);

	ctx->filter = filter;

	if (gf_filter_is_dynamic(filter)) {
		ctx->dyn_filter_mode = GF_TRUE;
		ctx->vfr = GF_TRUE;
	}

	//playout buffer not greater than max buffer
	if (ctx->buffer > ctx->mbuffer)
		ctx->buffer = ctx->mbuffer;

	//rebuffer level not greater than playout buffer
	if (ctx->rbuffer >= ctx->buffer)
		ctx->rbuffer = 0;


    if (ctx->player) {
		//explicit disable of OpenGL
		if (ctx->drv==GF_SC_DRV_OFF)
			ctx->ogl = GF_SC_GLMODE_OFF;

		if (ctx->ogl == GF_SC_GLMODE_AUTO)
			ctx->ogl = GF_SC_GLMODE_HYBRID;

		//we operate video output directly and dispatch audio output, we need to disable blocking mode
		//otherwise we will only get called when audio output is not blocking, and we will likely missed video frames
		gf_filter_prevent_blocking(filter, GF_TRUE);
	}

    e = gf_sc_load(ctx);
	if (e) return e;

	gf_filter_get_session_caps(filter, &sess_caps);

	sess_caps.max_screen_width = ctx->video_out->max_screen_width;
	sess_caps.max_screen_height = ctx->video_out->max_screen_height;
	sess_caps.max_screen_bpp = ctx->video_out->max_screen_bpp;

	gf_filter_set_session_caps(filter, &sess_caps);

	//make filter sticky (no shutdown if all inputs removed)
	gf_filter_make_sticky(filter);

	if (ctx->player) {

		//load audio filter chain, declaring audio output pid first
		if (!ctx->noaudio) {
			GF_Filter *audio_out = gf_filter_load_filter(filter, "aout", &e);
			ctx->audio_renderer->non_rt_output = 0;
			if (!audio_out) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to load audio output filter (%s) - audio disabled\n", gf_error_to_string(e) ));
			}
//			else {
//				gf_filter_reconnect_output(filter);
//			}
		}
		compositor_setup_aout(ctx);

		//create vout right away
		compositor_setup_vout(ctx);

		//always request a process task since we don't depend on input packets arrival (animations, pure scene presentations)
		gf_filter_post_process_task(filter);
	}
	//if not player mode, wait for pid connection to create vout, otherwise pid linking could fail

	//for coverage
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		compose_update_arg(filter, NULL, NULL);
	}
#endif

	gf_filter_set_event_target(filter, GF_TRUE);
	if (ctx->player==2) {
		const char *gui_path = gf_opts_get_key("core", "startup-file");
		if (gui_path) {
			gf_sc_connect_from_time(ctx, gui_path, 0, 0, 0, NULL);
			if (ctx->src)
				gf_opts_set_key("temp", "gui_load_urls", ctx->src);
		}
	}
	//src set, connect it (whether player mode or not)
	else if (ctx->src) {
		gf_sc_connect_from_time(ctx, ctx->src, 0, 0, 0, NULL);
	}
	return GF_OK;
}

GF_FilterProbeScore compose_probe_url(const char *url, const char *mime)
{
	//check all our builtin URL schemes
	if (!strnicmp(url, "mosaic://", 9)) {
		return GF_FPROBE_FORCE;
	}
	else if (!strnicmp(url, "views://", 8)) {
		return GF_FPROBE_FORCE;
	}
	return GF_FPROBE_NOT_SUPPORTED;
}


#define OFFS(_n)	#_n, offsetof(GF_Compositor, _n)
static GF_FilterArgs CompositorArgs[] =
{
	{ OFFS(aa), "set anti-aliasing mode for raster graphics; whether the setting is applied or not depends on the graphics module or graphic card\n"
		"- none: no anti-aliasing\n"
		"- text: anti-aliasing for text only\n"
		"- all: complete anti-aliasing", GF_PROP_UINT, "all", "none|text|all", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hlfill), "set highlight fill color (ARGB)", GF_PROP_UINT, "0x0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hlline), "set highlight stroke color (ARGB)", GF_PROP_UINT, "0xFF000000", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hllinew), "set highlight stroke width", GF_PROP_FLOAT, "1.0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sz), "enable scalable zoom. When scalable zoom is enabled, resizing the output window will also recompute all vectorial objects. Otherwise only the final buffer is stretched", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(bc), "default background color to use when displaying transparent images or video with no scene composition instructions", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(yuvhw), "enable YUV hardware for 2D blit", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(blitp), "partial hardware blit. If not set, will force more redraw", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(softblt), "enable software blit/stretch in 2D. If disabled, vector graphics rasterizer will always be used", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_EXPERT},

	{ OFFS(stress), "enable stress mode of compositor (rebuild all vector graphics and texture states at each frame)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fast), "enable speed optimization - whether the setting is applied or not depends on the graphics module / graphic card", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(bvol), "draw bounding volume of objects\n"
			"- no: disable bounding box\n"
			"- box: draws a rectangle (2D) or box (3D)\n"
			"- aabb: draws axis-aligned bounding-box tree (3D) or rectangle (2D)", GF_PROP_UINT, "no", "no|box|aabb", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(textxt), "specify whether text shall be drawn to a texture and then rendered or directly rendered. Using textured text can improve text rendering in 3D and also improve text-on-video like content\n"
		"- default: use texturing for OpenGL rendering, no texture for 2D rasterizer\n"
		"- never: never uses text textures\n"
		"- always: always render text to texture before drawing"
		"", GF_PROP_UINT, "default", "default|never|always", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(out8b), "convert 10-bit video to 8 bit texture before GPU upload", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(drop), "drop late frame when drawing. If not set, frames are not dropped until a desynchronization of 1 second or more is observed", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(sclock), "force synchronizing all streams on a single clock", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(sgaze), "simulate gaze events through mouse", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ckey), "color key to use in windowless mode (0xFFRRGGBB). GPAC currently does not support true alpha blitting to desktop due to limitations in most windowing toolkit, it therefore uses color keying mechanism. The alpha part of the key is used for global transparency of the output, if supported", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(timeout), "timeout in ms after which a source is considered dead (0 disable timeout)", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(fps), "simulation frame rate when animation-only sources are played (ignored when video is present)", GF_PROP_FRACTION, "30/1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(timescale), "timescale used for output packets when no input video PID. A value of 0 means fps numerator", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(autofps), "use video input fps for output, ignored in player mode. If no video or not set, uses [-fps]()", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(vfr), "only emit frames when changes are detected. (always true in player mode and when filter is dynamically loaded)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},

	{ OFFS(dur), "duration of generation. Mostly used when no video input is present. Negative values mean number of frames, positive values duration in second, 0 stops as soon as all streams are done", GF_PROP_DOUBLE, "0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(fsize), "force the scene to resize to the biggest bitmap available if no size info is given in the BIFS configuration", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mode2d), "specify whether immediate drawing should be used or not\n"
	"- immediate: the screen is completely redrawn at each frame (always on if pass-through mode is detected)\n"
	"- defer: object positioning is tracked from frame to frame and dirty rectangles info is collected in order to redraw the minimal amount of the screen buffer\n"
	"- debug: only renders changed areas, resetting other areas\n"
	 "Whether the setting is applied or not depends on the graphics module and player mode", GF_PROP_UINT, "defer", "defer|immediate|debug", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(amc), "audio multichannel support; if disabled always down-mix to stereo. Useful if the multichannel output does not work properly", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(asr), "force output sample rate (0 for auto)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ach), "force output channels (0 for auto)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(alayout), "force output channel layout (0 for auto)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(afmt), "force output channel format (0 for auto)", GF_PROP_PCMFMT, "s16", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(asize), "audio output packet size in samples", GF_PROP_UINT, "1024", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(abuf), "audio output buffer duration in ms - the audio renderer fills the output PID up to this value. A too low value will lower latency but can have real-time playback issues", GF_PROP_UINT,
#ifdef GPAC_CONFIG_ANDROID
		"200"
#else
		"100"
#endif
		, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(avol), "audio volume in percent", GF_PROP_UINT, "100", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(apan), "audio pan in percent, 50 is no pan", GF_PROP_UINT, "50", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(async), "audio resynchronization; if disabled, audio data is never dropped but may get out of sync", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(max_aspeed), "silence audio if playback speed is greater than specified value", GF_PROP_DOUBLE, "2.0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(max_vspeed), "move to i-frame only decoding if playback speed is greater than specified value", GF_PROP_DOUBLE, "4.0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},

	{ OFFS(buffer), "playout buffer in ms (overridden by `BufferLength` property of input PID)", GF_PROP_UINT, "3000", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(rbuffer), "rebuffer trigger in ms (overridden by `RebufferLength` property of input PID)", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(mbuffer), "max buffer in ms, must be greater than playout buffer (overridden by `BufferMaxOccupancy` property of input PID)", GF_PROP_UINT, "3000", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(ntpsync), "ntp resync threshold in ms (drops frame if their NTP is more than the given threshold above local ntp), 0 disables ntp drop", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},

	{ OFFS(nojs), "disable javascript", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(noback), "ignore background nodes and viewport fill (useful when dumping to PNG)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},

#ifndef GPAC_DISABLE_3D
	{ OFFS(ogl), "specify 2D rendering mode\n"
				"- auto: automatically decides between on, off and hybrid based on content\n"
				"- off: disables OpenGL; 3D will not be rendered\n"
				"- on: uses OpenGL for all graphics; this will involve polygon tesselation and 2D graphics will not look as nice as 2D mode\n"
				"- hybrid: the compositor performs software drawing of 2D graphics with no textures (better quality) and uses OpenGL for all 2D objects with textures and 3D objects"
				, GF_PROP_UINT, "auto", "auto|off|hybrid|on", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pbo), "enable PixelBufferObjects to push YUV textures to GPU in OpenGL Mode. This may slightly increase the performances of the playback", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nav), "override the default navigation mode of MPEG-4/VRML (Walk) and X3D (Examine)\n"
	"- none: disables navigation\n"
	"- walk: 3D world walk\n"
	"- fly: 3D world fly (no ground detection)\n"
	"- pan: 2D/3D world zoom/pan\n"
	"- game: 3D world game (mouse gives walk direction)\n"
	"- slide: 2D/3D world slide\n"
	"- exam: 2D/3D object examine\n"
	"- orbit: 3D object orbit\n"
	"- vr: 3D world VR (yaw/pitch/roll)"
	"", GF_PROP_UINT, "none", "none|walk|fly|pan|game|slide|exam|orbit|vr", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(linegl), "indicate that outlining shall be done through OpenGL pen width rather than vectorial outlining", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(epow2), "emulate power-of-2 textures for OpenGL (old hardware). Ignored if OpenGL rectangular texture extension is enabled\n"
	"- yes: video texture is not resized but emulated with padding. This usually speeds up video mapping on shapes but disables texture transformations\n"
	"- no: video is resized to a power of 2 texture when mapping to a shape", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(paa), "indicate whether polygon antialiasing should be used in full antialiasing mode. If not set, only lines and points antialiasing are used", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(bcull), "indicate whether backface culling shall be disable or not\n"
				"- on: enables backface culling\n"
				"- off: disables backface culling\n"
				"- alpha: only enables backface culling for transparent meshes"
		"", GF_PROP_UINT, "on", "off|on|alpha", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(wire), "wireframe mode\n"
	"- none: objects are drawn as solid\n"
    "- only: objects are drawn as wireframe only\n"
    "- solid: objects are drawn as solid and wireframe is then drawn", GF_PROP_UINT, "none", "none|only|solid", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(norms), "normal vector drawing for debug\n"
	"- none: no normals drawn\n"
	"- face: one normal per face drawn\n"
	"- vertex: one normal per vertex drawn"
	"", GF_PROP_UINT, "none", "none|face|vertex", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(rext), "use non power of two (rectangular) texture GL extension", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(cull), "use aabb culling: large objects are rendered in multiple calls when not fully in viewport", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(depth_gl_scale), "set depth scaler", GF_PROP_FLOAT, "100", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(depth_gl_type), "set geometry type used to draw depth video\n"
	"- none: no geometric conversion\n"
	"- point: compute point cloud from pixel+depth\n"
	"- strip: same as point but thins point set"
	"", GF_PROP_UINT, "none", "none|point|strip", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(nbviews), "number of views to use in stereo mode", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(stereo), "stereo output type. If your graphic card does not support OpenGL shaders, only `top` and `side` modes will be available\n"
		"- none: no stereo\n"
		"- side: images are displayed side by side from left to right\n"
		"- top: images are displayed from top (laft view) to bottom (right view)\n"
		"- hmd: same as side except that view aspect ratio is not changed\n"
		"- ana: standard color anaglyph (red for left view, green and blue for right view) is used (forces views=2)\n"
		"- cols: images are interleaved by columns, left view on even columns and left view on odd columns (forces views=2)\n"
		"- rows: images are interleaved by columns, left view on even rows and left view on odd rows (forces views=2)\n"
		"- spv5: images are interleaved by for SpatialView 5 views display, fullscreen mode (forces views=5)\n"
		"- alio8: images are interleaved by for Alioscopy 8 views displays, fullscreen mode (forces views=8)\n"
		"- custom: images are interleaved according to the shader file indicated in [-mvshader](). The shader is exposed each view as uniform sampler2D gfViewX, where X is the view number starting from the left", GF_PROP_UINT, "none", "none|top|side|hmd|custom|cols|rows|ana|spv5|alio8", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mvshader), "file path to the custom multiview interleaving shader", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fpack), "default frame packing of input video\n"
		"- none: no frame packing\n"
		"- top: top bottom frame packing\n"
		"- side: side by side packing"
	"", GF_PROP_UINT, "none", "none|top|side", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(camlay), "camera layout in multiview modes\n"
		"- straight: camera is moved along a straight line, no rotation\n"
		"- offaxis: off-axis projection is used\n"
		"- linear: camera is moved along a straight line with rotation\n"
		"- circular: camera is moved along a circle with rotation"
	"", GF_PROP_UINT, "offaxis", "straight|offaxis|linear|circular", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(iod), "inter-ocular distance (eye separation) in cm (distance between the cameras). ", GF_PROP_FLOAT, "6.4", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(rview), "reverse view order", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dbgpack), "view packed stereo video as single image (show all)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},


	{ OFFS(tvtn), "number of point sampling for tile visibility algorithm", GF_PROP_UINT, "30", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tvtt), "number of points above which the tile is considered visible", GF_PROP_UINT, "8", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tvtd), "debug tiles and full coverage SRD\n"
		"- off: regular draw\n"
		"- partial: only displaying partial tiles, not the full sphere video\n"
		"- full: only display the full sphere video", GF_PROP_UINT, "off", "off|partial|full", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(tvtf), "force all tiles to be considered visible, regardless of viewpoint", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fov), "default field of view for VR", GF_PROP_FLOAT, "1.570796326794897", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(vertshader), "path to vertex shader file", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT },
	{ OFFS(fragshader), "path to fragment shader file", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT },
#endif

#ifdef GF_SR_USE_DEPTH
	{ OFFS(autocal), "auto calibration of znear/zfar in depth rendering mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dispdepth), "display depth, negative value uses default screen height", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dispdist), "distance in cm between the camera and the zero-disparity plane. There is currently no automatic calibration of depth in GPAC", GF_PROP_FLOAT, "50", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
#ifndef GPAC_DISABLE_3D
	{ OFFS(focdist), "distance of focus point", GF_PROP_FLOAT, "0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
#endif
#endif

#ifdef GF_SR_USE_VIDEO_CACHE
	{ OFFS(vcsize), "visual cache size when storing raster graphics to memory", GF_PROP_UINT, "0", "0,+I", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(vcscale), "visual cache scale factor in percent when storing raster graphics to memory", GF_PROP_UINT, "100", "0,100", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(vctol), "visual cache tolerance when storing raster graphics to memory. If the difference between the stored version scale and the target display scale is less than tolerance, the cache will be used, otherwise it will be recomputed", GF_PROP_UINT, "30", "0,100", GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
#endif
	{ OFFS(osize), "force output size. If not set, size is derived from inputs", GF_PROP_VEC2I, "0x0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dpi), "default dpi if not indicated by video output", GF_PROP_VEC2I, "96x96", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(dbgpvr), "debug scene used by PVR addon", GF_PROP_FLOAT, "0", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_EXPERT},
	{ OFFS(player), "set compositor in player mode\n"
	"- no: regular mode\n"
	"- base: player mode\n"
	"- gui: player mode with GUI auto-start", GF_PROP_UINT, "no", "no|base|gui", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(noaudio), "disable audio output", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(opfmt), "pixel format to use for output. Ignored in [-player]() mode", GF_PROP_PIXFMT, "none", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(drv), "indicate if graphics driver should be used\n"
				"- no: never loads a graphics driver, software blit is used, no 3D possible (in player mode, disables OpenGL)\n"
				"- yes: always loads a graphics driver, output pixel format will be RGB (in player mode, same as `auto`)\n"
				"- auto: decides based on the loaded content"
			, GF_PROP_UINT, "auto", "no|yes|auto", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(src), "URL of source content", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},

	{ OFFS(gaze_x), "horizontal gaze coordinate (0=left, width=right)", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(gaze_y), "vertical gaze coordinate (0=top, height=bottom)", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(gazer_enabled), "enable gaze event dispatch", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},

	{ OFFS(subtx), "horizontal translation in pixels towards right for subtitles renderers", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(subty), "vertical translation in pixels towards right for subtitles renderers", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(subfs), "font size for subtitles renderers (0 means automatic)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(subd), "subtitle delay in milliseconds for subtitles renderers", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(audd), "audio delay in milliseconds", GF_PROP_SINT, "0", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{0}
};

static const GF_FilterCapability CompositorCaps[] =
{
	/*first cap bundle for explicitly loaded compositor: accepts audio and video as well as scene/od*/
	CAP_UINT(GF_CAPS_INPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT|GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	/*second cap bundle for dynmac loaded compositor: only accepts text/scene/od*/
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


const GF_FilterRegister CompositorFilterRegister = {
	.name = "compositor",
	GF_FS_SET_DESCRIPTION("Compositor")
	GF_FS_SET_HELP("The GPAC compositor allows mixing audio, video, text and graphics in a timed fashion.\n"
	"The compositor operates either in media-client or filter-only mode.\n"
	"\n"
	"# Media-client mode\n"
	"In this mode, the compositor acts as a pseudo-sink for the video side and creates its own output window.\n"
	"The video frames are dispatched to the output video PID in the form of frame pointers requiring later GPU read if used.\n"
	"The audio part acts as a regular filter, potentially mixing and resampling the audio inputs to generate its output.\n"
	"User events are directly processed by the filter in this mode.\n"
	"\n"
	"# Filter mode\n"
	"In this mode, the compositor acts as a regular filter generating frames based on the loaded scene.\n"
	"It will generate its outputs based on the input video frames, and will process user event sent by consuming filter(s).\n"
	"If no input video frames (e.g. pure BIFS / SVG / VRML), the filter will generate frames based on the [-fps](), at constant or variable frame rate.\n"
	"It will stop generating frames as soon as all input streams are done, unless extended/reduced by [-dur]().\n"
	"If audio streams are loaded, an audio output PID is created.\n"
	"\n"
	"The default output pixel format in filter mode is:\n"
	"- `rgb` when the filter is explicitly loaded by the application\n"
	"- `rgba` when the filter is loaded during a link resolution\n"
	"This can be changed by assigning the [-opfmt]() option.\n"
	"\n"
	"In filter-only mode, the special URL `gpid://` is used to locate PIDs in the scene description, in order to design scenes independently from source media.\n"
	"When such a PID is associated to a `Background2D` node in BIFS (no SVG mapping yet), the compositor operates in pass-through mode.\n"
	"In this mode, only new input frames on the pass-through PID will generate new frames, and the scene clock matches the input packet time.\n"
	"The output size and pixel format will be set to the input size and pixel format, unless specified otherwise in the filter options.\n"
	"\n"
	"If only 2D graphics are used and display driver is not forced, 2D rasterizer will happen in the output pixel format (including YUV pixel formats).\n"
	"In this case, in-place processing (rasterizing over the input frame data) will be used whenever allowed by input data.\n"
	"\n"
	"If 3D graphics are used or display driver is forced, OpenGL will be used on offscreen surface and the output packet will be an OpenGL texture.\n"
	"\n"
	"# Specific URL syntaxes\n"
	"The compositor accepts any URL type supported by GPAC. It also accepts the following schemes for URLs:\n"
	"- views:// : creates an auto-stereo scene of N views from `views://v1::.::vN`\n"
	"- mosaic:// : creates a mosaic of N views from `mosaic://v1::.::vN`\n"
	"\n"
	"For both syntaxes, `vN` can be any type of URL supported by GPAC.\n"
	"For `views://` syntax, the number of rendered views is set by [-nbviews]():\n"
	"- If the URL gives less views than rendered, the views will be repeated\n"
	"- If the URL gives more views than rendered, the extra views will be ignored\n"
	"\n"
	"The compositor can act as a source filter when the [-src]() option is explicitly set, independently from the operating mode:\n"
	"EX gpac compositor:src=source.mp4 vout\n"
	"\n"
	"The compositor can act as a source filter when the source url uses one of the compositor built-in protocol schemes:\n"
	"EX gpac -i mosaic://URL1:URL2 vout\n"
	"\n"
	)
	.private_size = sizeof(GF_Compositor),
	.flags = GF_FS_REG_MAIN_THREAD,
	.max_extra_pids = (u32) -1,
	SETCAPS(CompositorCaps),
	.args = CompositorArgs,
	.initialize = compose_initialize,
	.finalize = compose_finalize,
	.process = compose_process,
	.process_event = compose_process_event,
	.configure_pid = compose_configure_pid,
	.reconfigure_output = compose_reconfig_output,
	.update_arg = compose_update_arg,
	.probe_url = compose_probe_url,
};

const GF_FilterRegister *compose_filter_register(GF_FilterSession *session)
{
	u32 i=0;
	u32 nb_args = sizeof(CompositorArgs) / sizeof(GF_FilterArgs) - 1;

	for (i=0; i<nb_args; i++) {
		if (!strcmp(CompositorArgs[i].arg_name, "afmt")) {
			CompositorArgs[i].min_max_enum = gf_audio_fmt_all_names();
		}
		else if (!strcmp(CompositorArgs[i].arg_name, "opfmt")) {
			CompositorArgs[i].min_max_enum =  gf_pixel_fmt_all_names();
		}
	}
	return &CompositorFilterRegister;
}
#else
const GF_FilterRegister *compose_filter_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_PLAYER

