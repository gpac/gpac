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


#include <gpac/constants.h>
#include <gpac/internal/compositor_dev.h>

#if !defined(GPAC_DISABLE_COMPOSITOR)


void mediacontrol_restart(GF_ObjectManager *odm)
{
	GF_List *to_restart;
	GF_ObjectManager *ctrl_od;
	GF_Clock *ck, *scene_ck;
	u32 i;
	u32 current_seg;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif

	if (!odm || (odm->flags & GF_ODM_NO_TIME_CTRL) ) return;

#ifndef GPAC_DISABLE_VRML
	ctrl = gf_odm_get_mediacontrol(odm);
	if (ctrl) {
		/*we have a control - filter calls to only handle objects owning media control*/
		ctrl_od = ctrl->stream->odm;
		/*if media control owns the scene this OD refers to the scene is always restarted - TODO make that an option*/
		if (!ctrl_od->subscene) {
			if (ctrl->stream->odm != odm) return;
		}
		odm = ctrl->stream->odm;

		/*this is inline restart - only possible through media control*/
		if (odm->subscene && odm->subscene->root_od==ctrl->stream->odm) {
			gf_inline_restart(odm->subscene);
			return;
		}
	}
#endif

	/*if clock is main scene clock do nothing*/
	scene_ck = gf_odm_get_media_clock(odm->parentscene->root_od);
	if (gf_odm_shares_clock(odm, scene_ck)) {
		if (odm->parentscene->is_dynamic_scene)
			gf_scene_restart_dynamic(odm->parentscene, 0, 0, 0);
		return;
	}

	/*otherwise locate all objects sharing the clock*/
	ck = gf_odm_get_media_clock(odm);
	if (!ck) return;

	current_seg = 0;
#ifndef GPAC_DISABLE_VRML
	/*store current segment idx*/
	if (ctrl) {
		current_seg = ctrl->current_seg;
		/*if last segment is passed restart*/
		if (gf_list_count(ctrl->seg) == current_seg) current_seg = 0;
	}
#endif

	to_restart = gf_list_new();
	/*do stop/start in 2 pass, it's much cleaner for servers*/
	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(odm->parentscene->resources, &i))) {
		if (!gf_odm_shares_clock(ctrl_od, ck)) continue;
		/*if running, stop and collect for restart*/
		if (ctrl_od->state) {
			gf_odm_stop(ctrl_od, 1);
			gf_list_add(to_restart, ctrl_od);
		}
	}
	/*force clock reset since we don't know how OD ordering is done*/
	gf_clock_reset(ck);
#ifndef GPAC_DISABLE_VRML
	if (ctrl) ctrl->current_seg = current_seg;
#endif

	/*play on all ODs collected for restart*/
	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(to_restart, &i))) {
		//we want to make sure restart is clled right away with the current media control settings
		gf_odm_start(ctrl_od);
	}
	gf_list_del(to_restart);
}


Bool MC_URLChanged(MFURL *old_url, MFURL *new_url)
{
	u32 i;
	if (gf_mo_get_od_id(old_url) != gf_mo_get_od_id(new_url)) return 1;
	
	if ((new_url->count==1) && new_url->vals[0].url && !strlen(new_url->vals[0].url) ) new_url->count = 0;
	
	if (old_url->count != new_url->count) return 1;

	for (i=0; i<old_url->count; i++) {
		if (old_url->vals[i].url || new_url->vals[i].url) {
			if (!old_url->vals[i].url || !new_url->vals[i].url) return 1;
			if (strcmp(old_url->vals[i].url, new_url->vals[i].url)) return 1;
		}
	}
	return 0;
}




/*resume all objects*/
void mediacontrol_resume(GF_ObjectManager *odm, Bool resume_to_live)
{
	u32 i;
	GF_ObjectManager *ctrl_od;
	GF_Scene *in_scene;
	GF_Clock *ck;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	/*otherwise locate all objects sharing the clock*/
	ck = gf_odm_get_media_clock(odm);
	if (!ck) return;

	in_scene = odm->parentscene;
	if (odm->subscene) {
		gf_assert(odm->subscene->root_od==odm);
		gf_assert(odm->subscene->is_dynamic_scene || gf_odm_shares_clock(odm, ck) );
		/*resume root*/
		gf_odm_resume(odm);
		in_scene = odm->subscene;
	}

	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(in_scene->resources, &i))) {
		if (!odm->subscene && !gf_odm_shares_clock(ctrl_od, ck))
			continue;
		if (!ctrl_od->ck || !ctrl_od->pid)
			continue;

		if (ctrl_od->addon && (ctrl_od->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
			gf_clock_resume(ck);
			if (resume_to_live)
				gf_scene_select_main_addon(in_scene, ctrl_od, GF_FALSE, 0);
		}

		if (ctrl_od->subscene) {
			mediacontrol_resume(ctrl_od, resume_to_live);
		} else {
			gf_odm_resume(ctrl_od);
		}
	}
}


/*pause all objects*/
void mediacontrol_pause(GF_ObjectManager *odm)
{
	u32 i;
	GF_ObjectManager *ctrl_od;
	GF_Scene *in_scene;
	GF_Clock *ck;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	/*otherwise locate all objects sharing the clock*/
	ck = gf_odm_get_media_clock(odm);
	if (!ck) {
		odm->flags |= GF_ODM_PAUSE_QUEUED;
		return;
	}

	in_scene = odm->parentscene;
	if (odm->subscene) {
		gf_assert(odm->subscene->root_od==odm);
		gf_assert(odm->subscene->is_dynamic_scene || gf_odm_shares_clock(odm, ck) );
		/*pause root*/
		gf_odm_pause(odm);
		in_scene = odm->subscene;
	}

	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(in_scene->resources, &i))) {
		if (!odm->subscene && !gf_odm_shares_clock(ctrl_od, ck))
			continue;
		if (!ctrl_od->ck || !ctrl_od->pid)
			continue;

		if (ctrl_od->addon && (ctrl_od->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
			gf_clock_pause(ck);
			gf_scene_select_main_addon(in_scene, ctrl_od, GF_TRUE, gf_clock_time_absolute(ck) );
		}

		if (ctrl_od->subscene) {
			mediacontrol_pause(ctrl_od);
		} else {
			gf_odm_pause(ctrl_od);
		}
	}
}


/*pause all objects*/
void mediacontrol_set_speed(GF_ObjectManager *odm, Fixed speed)
{
	u32 i;
	GF_ObjectManager *ctrl_od;
	GF_Scene *in_scene;
	GF_Clock *ck;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	/*locate all objects sharing the clock*/
	ck = gf_odm_get_media_clock(odm);
	if (!ck) return;

	in_scene = odm->parentscene;
	if (odm->subscene) {
		gf_assert(odm->subscene->root_od==odm);
		in_scene = odm->subscene;

		//dynamic scene with speed direction, we need to re-start everything to issue new PLAY requests
		if (in_scene->is_dynamic_scene && (gf_mulfix(ck->speed, speed) < 0)) {
			u64 time = gf_clock_time_absolute(ck);
			gf_clock_set_speed(ck, speed);

			//enable main addon
			if (speed<0) {
				i=0;
				while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(in_scene->resources, &i))) {
					if (ctrl_od->addon && (ctrl_od->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
						gf_scene_select_main_addon(in_scene, ctrl_od, GF_TRUE, time );
						break;
					}
				}
			}
			gf_scene_restart_dynamic(in_scene, time, 0, 1);
			return;
		}
		gf_clock_set_speed(ck, speed);
		gf_odm_set_speed(odm, speed, GF_TRUE);
	}

	i=0;
	while ((ctrl_od = (GF_ObjectManager*)gf_list_enum(in_scene->resources, &i))) {
		if (!gf_odm_shares_clock(ctrl_od, ck)) continue;

		if (ctrl_od->subscene) {
			mediacontrol_set_speed(ctrl_od, speed);
		} else {
			gf_odm_set_speed(ctrl_od, speed, GF_TRUE);
		}
	}
}

#ifndef GPAC_DISABLE_VRML

void MC_GetRange(MediaControlStack *ctrl, Double *start_range, Double *end_range)
{
	u32 i;
	Double duration;

	if (gf_list_count(ctrl->seg)) {
		GF_Segment *last_seg, *prev_seg;
		GF_Segment *desc = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);
		if (!desc) {
			*start_range = 0;
			*end_range = 0;
			return;
		}
		/*get last segment in consecutive range so that we never issue stop/play between consecutive segments*/
		prev_seg = desc;
		last_seg = NULL;
		duration = desc->Duration;
		i=1+ctrl->current_seg;
		while ((last_seg = (GF_Segment *)gf_list_enum(ctrl->seg, &i))) {
			if (prev_seg->startTime + prev_seg->Duration != last_seg->startTime) {
				last_seg = NULL;
				break;
			}
			prev_seg = last_seg;
			duration += last_seg->Duration;
		}
//		if (!last_seg) last_seg = desc;

		*start_range = desc->startTime;
		if (ctrl->control->mediaStartTime>=0) *start_range += ctrl->control->mediaStartTime;

		*end_range = desc->startTime;
		if ((ctrl->control->mediaStopTime>=0) && ctrl->control->mediaStopTime<duration) {
			*end_range += ctrl->control->mediaStopTime;
		} else {
			*end_range += duration;
		}
	} else {
		if (ctrl->control->mediaStartTime>=0) *start_range = ctrl->control->mediaStartTime;
		if (ctrl->control->mediaStopTime>=0) *end_range = ctrl->control->mediaStopTime;
	}
}


void RenderMediaControl(GF_Node *node, void *rs, Bool is_destroy)
{
	Bool shall_restart, need_restart;
	GF_ObjectManager *odm;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	MediaControlStack *stack =(MediaControlStack *) gf_node_get_private(node);

	if (is_destroy) {
		/*reset ODM using this control*/
		if (stack->stream) {
			if (stack->stream->odm) {
				odm = stack->stream->odm;
				gf_odm_remove_mediacontrol(odm, stack);
			}
			/*also removes the association ck<->MC if the object has been destroyed before the node*/
			if (stack->ck) stack->ck->mc = NULL;
		}
		gf_list_del(stack->seg);
		gf_sg_vrml_mf_reset(&stack->url, GF_SG_VRML_MFURL);
		gf_free(stack);
		return;
	}
	//we need to disable culling otherwise we may never be called back again ...
	tr_state->disable_cull = 1;

	/*not changed nothing to do - note we need to register with stream yet for control switching...*/
	if (stack->stream && (!stack->changed || !stack->control->enabled)) return;

	need_restart = (stack->changed==2) ? 1 : 0;
	shall_restart = (stack->control->mediaStartTime>=0) ? 1 : 0;

	/*check url target*/
	if (stack->stream) {
		if (MC_URLChanged(&stack->url, &stack->control->url)) {
			GF_MediaObject *prev;
			gf_sg_vrml_mf_reset(&stack->url, GF_SG_VRML_MFURL);

			prev = stack->stream;
			if (gf_list_find(stack->parent->scene_objects, prev)<0)
				prev = NULL;

			stack->stream = gf_scene_get_media_object(stack->parent, &stack->control->url, GF_MEDIA_OBJECT_UNDEF, 0);
			if (stack->stream) {
				if (!stack->stream->odm) return;
				/*MediaControl on inline: if dynamic scene, make sure it is connected before attaching...*/
				if (stack->stream->odm->subscene) {
					if (stack->stream->odm->subscene->is_dynamic_scene && !stack->stream->odm->ck) return;
				}
				gf_sg_vrml_field_copy(&stack->url, &stack->control->url, GF_SG_VRML_MFURL);

				/*remove from prev*/
				if (prev && prev->odm && (prev != stack->stream)) gf_odm_remove_mediacontrol(prev->odm, stack);
				/*register with new*/
				/*if we assigned the media control to an exiting object - force the state of the object*/
				gf_odm_set_mediacontrol((GF_ObjectManager *) stack->stream->odm, stack);

				while (gf_list_count(stack->seg)) gf_list_rem(stack->seg, 0);
				gf_odm_init_segments((GF_ObjectManager *) stack->stream->odm, stack->seg, &stack->control->url);

				stack->current_seg = 0;
				//do not restart if no mediaStartTime and speed is 1
				if ((stack->control->mediaStartTime>0) || gf_list_count(stack->seg) || (stack->control->mediaSpeed != FIX_ONE) ) {
					shall_restart = need_restart = 1;
				} else {
					shall_restart = need_restart = 0;
					//URL changed, we are by default in PLAY mode.
					stack->media_speed = 1;
				}

				stack->ck = gf_odm_get_media_clock(stack->stream->odm);
			}
			/*control has been removed and we were paused, resume*/
			else if (stack->paused) {
				if (prev)
					mediacontrol_resume((GF_ObjectManager *) prev->odm, 0);
				
				stack->paused = 0;
			}
			/*MediaControl has been detached*/
			else {
				if (prev)
					gf_odm_remove_mediacontrol(prev->odm, stack);
				return;
			}
		}
	} else {
		stack->stream = gf_scene_get_media_object(stack->parent, &stack->control->url, GF_MEDIA_OBJECT_UNDEF, 0);
		if (!stack->stream || !stack->stream->odm) {
			if (stack->control->url.count) gf_sc_invalidate(stack->parent->compositor, NULL);
			stack->stream = NULL;
			stack->changed = 0;
			return;
		}
		if (stack->stream->connect_state > MO_CONNECT_BUFFERING) {
			gf_sg_vrml_mf_reset(&stack->control->url, GF_SG_VRML_MFURL);
			stack->enabled = GF_FALSE;
			stack->stream = NULL;
			return;
		}
		stack->ck = gf_odm_get_media_clock(stack->stream->odm);
		/*OD not ready yet*/
		if (!stack->ck) {
			stack->stream = NULL;
			if (stack->control->url.count) {
				stack->is_init = 0;
				gf_sc_invalidate(stack->parent->compositor, NULL);
			}
			return;
		}
		gf_sg_vrml_field_copy(&stack->url, &stack->control->url, GF_SG_VRML_MFURL);
		gf_odm_set_mediacontrol((GF_ObjectManager *) stack->stream->odm, stack);

		while (gf_list_count(stack->seg)) gf_list_rem(stack->seg, 0);
		gf_odm_init_segments((GF_ObjectManager *) stack->stream->odm, stack->seg, &stack->control->url);
		stack->current_seg = 0;

		/*we shouldn't have to restart unless start/stop times have been changed, which is tested below*/
		need_restart = (stack->stream->odm->state==GF_ODM_STATE_PLAY) ? 1 : 0;
	}

	if ((stack->is_init && !stack->changed) || !stack->control->enabled || !stack->stream) return;


	/*if not previously enabled and now enabled, switch all other controls off and reactivate*/
	if (!stack->enabled) {
		Bool restart;
		stack->enabled = 1;
		restart = gf_odm_switch_mediacontrol(stack->stream->odm, stack);
		if (restart) need_restart = 1;
	}

	stack->changed = 0;

	if (!stack->control->mediaSpeed) shall_restart = 0;

	odm = (GF_ObjectManager *)stack->stream->odm;

	/*check for changes*/
	if (!stack->is_init) {
		need_restart = 0;
		/*not linked yet*/
		if (!odm) return;
		stack->media_speed = stack->control->mediaSpeed;
		stack->enabled = stack->control->enabled;
		stack->media_start = stack->control->mediaStartTime;
		if (stack->media_stop != stack->control->mediaStopTime) {
			if (stack->control->mediaStopTime < 1000000000) need_restart  = 1;
			stack->media_stop = stack->control->mediaStopTime;
		}
		stack->is_init = 1;
		stack->paused = 0;
		/*the object has already been started, and media start time is not 0, restart*/
		if (stack->stream->num_open) {
			if (need_restart  || (stack->media_start > 0) || (gf_list_count(stack->seg)>0 )  || (stack->media_speed!=FIX_ONE ) ) {
				mediacontrol_restart(odm);
			} else if (stack->media_speed == 0) {
				mediacontrol_pause(odm);
				stack->paused = 1;
			}
		}
		return;
	}

	if (stack->media_speed != stack->control->mediaSpeed) {
		/*if no speed pause*/
		if (!stack->control->mediaSpeed && !stack->paused) {
			mediacontrol_pause(odm);
			stack->paused = 1;
		}
		/*else resume if paused*/
		else if (stack->control->mediaSpeed && stack->paused) {
			mediacontrol_resume(odm, 0);
			stack->paused = 0;
			need_restart += shall_restart;
		}
		/*else set speed*/
		else if (stack->media_speed && stack->control->mediaSpeed) {
			/*don't set speed if we have to restart the media ...*/
			if (!shall_restart) mediacontrol_set_speed(odm, stack->control->mediaSpeed);
			need_restart += shall_restart;
		}
		/*init state was paused*/
		else if (!stack->media_speed) {
			need_restart ++;
		}
		stack->media_speed = stack->control->mediaSpeed;
	}
	/*check start/stop changes*/
	if (stack->media_start != stack->control->mediaStartTime) {
		stack->media_start = stack->control->mediaStartTime;
		need_restart += shall_restart;
	}
	/*stop change triggers restart no matter what (new range) if playing*/
	if (stack->media_stop != stack->control->mediaStopTime) {
		stack->media_stop = stack->control->mediaStopTime;
		if (stack->control->mediaSpeed) need_restart = 1;
	}

	if (need_restart) {
		mediacontrol_restart(odm);
	}

	/*handle preroll*/

}

void InitMediaControl(GF_Scene *scene, GF_Node *node)
{
	MediaControlStack *stack;
	GF_SAFEALLOC(stack, MediaControlStack);
	if (!stack) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_INTERACT, ("[Compositor] Failed to allocate media control stack\n"));
		return;
	}
	stack->changed = 1;
	stack->parent = scene;
	stack->control = (M_MediaControl *)node;
	stack->seg = gf_list_new();

	/*default values are stored on first render*/
	gf_node_set_callback_function(node, RenderMediaControl);
	gf_node_set_private(node, stack);
}


void MC_Modified(GF_Node *node)
{
	MediaControlStack *stack =(MediaControlStack *) gf_node_get_private(node);
	if (!stack) return;
	if (stack->changed!=2) {
		/*check URL*/
		if (MC_URLChanged(&stack->url, &stack->control->url))
			stack->changed = 2;
		/*check speed (play/pause)*/
		else if (stack->media_speed != stack->control->mediaSpeed)
			stack->changed = 1;
		/*check mediaStartTime (seek)*/
		else if (stack->media_start != stack->control->mediaStartTime) {
			/*do not reevaluate if mediaStartTime is reset to -1 (current time)*/
			if (stack->control->mediaStartTime!=-1.0)
				stack->changed = 2;
			/*check mediaStopTime <0 (timeshift buffer control)*/
		} else if (stack->media_stop != stack->control->mediaStopTime) {
			if (stack->control->mediaStopTime<=0)
				stack->changed = 2;
		}
//		else stack->changed = 1;
	}

	gf_node_dirty_set( gf_sg_get_root_node(gf_node_get_graph(node)), 0, 1);
	/*invalidate scene, we recompute MC state in render*/
	gf_sc_invalidate(stack->parent->compositor, NULL);
}


void gf_odm_set_mediacontrol(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	/*keep track of it*/
	if (ctrl && (gf_list_find(odm->mc_stack, ctrl) < 0)) gf_list_add(odm->mc_stack, ctrl);
	if (ctrl && !ctrl->control->enabled) return;

	if (odm->subscene && odm->subscene->is_dynamic_scene) {
		if (odm->ck) {
			/*deactivate current control*/
			if (ctrl && odm->ck->mc) {
				odm->ck->mc->control->enabled = 0;
				gf_node_event_out((GF_Node *)odm->ck->mc->control, 7/*"enabled"*/);
			}
			odm->ck->mc = ctrl;
		}
	} else {
		/*for each clock in the controled OD*/
		if (odm->ck && (odm->ck->mc != ctrl)) {
			/*deactivate current control*/
			if (ctrl && odm->ck->mc) {
				odm->ck->mc->control->enabled = 0;
				gf_node_event_out((GF_Node *)odm->ck->mc->control, 7/*"enabled"*/);
			}
			/*and attach this control to the clock*/
			odm->ck->mc = ctrl;
		}
	}
	/*store active control on media*/
	odm->media_ctrl = gf_odm_get_mediacontrol(odm);
}



MediaControlStack *gf_odm_get_mediacontrol(GF_ObjectManager *odm)
{
	GF_Clock *ck;
	ck = gf_odm_get_media_clock(odm);
	if (!ck) return NULL;
	return ck->mc;
}


void gf_odm_remove_mediacontrol(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	gf_list_del_item(odm->mc_stack, ctrl);
	/*removed. Note the spec doesn't say what to do in this case...*/
	if (odm->media_ctrl == ctrl) {
		/*we're about to release the media control from this object - if paused, force a resume (as if no MC was set)*/
		if (ctrl->paused)
			mediacontrol_resume(odm, 0);
		gf_odm_set_mediacontrol(odm, NULL);
	}
}

Bool gf_odm_switch_mediacontrol(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	u32 i;
	MediaControlStack *st2;
	if (!ctrl->control->enabled) return 0;

	/*for all media controls other than this one force enable to false*/
	i=0;
	while ((st2 = (MediaControlStack *)gf_list_enum(odm->mc_stack, &i))) {
		if (st2 == ctrl) continue;
		if (st2->control->enabled) {
			st2->control->enabled = 0;
			gf_node_event_out((GF_Node *) st2->control, 7/*"enabled"*/);
		}
		st2->enabled = 0;
	}
	if (ctrl == odm->media_ctrl) return 0;
	gf_odm_set_mediacontrol(odm, ctrl);
	return 1;
}

Bool gf_odm_check_segment_switch(GF_ObjectManager *odm)
{
	u32 count, i;
	GF_Segment *cur, *next;
	MediaControlStack *ctrl = gf_odm_get_mediacontrol(odm);

	/*if no control or control not on this object ignore segment switch*/
	if (!ctrl || (ctrl->stream->odm != odm)) return 0;

	count = gf_list_count(ctrl->seg);
	/*reached end of controled stream (no more segments)*/
	if (ctrl->current_seg>=count) return 0;

	/*synth media, trigger if end of segment run-time*/
	if (!odm->type || ((odm->type!=GF_STREAM_VISUAL) && (odm->type!=GF_STREAM_AUDIO))) {
		GF_Clock *ck = gf_odm_get_media_clock(odm);
		u64 now = gf_clock_time_absolute(ck);
		u64 dur = odm->subscene ? odm->subscene->duration : odm->duration;
		cur = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);
		if (odm->subscene && odm->subscene->needs_restart) return 0;
		if (cur) dur = (u32) ((cur->Duration+cur->startTime)*1000);
		//if next frame is after current segment trigger switch now
		if (now + odm->parentscene->compositor->frame_duration < dur)
			return 0;
	} else {
		/*FIXME - for natural media with scalability, we should only process when all streams of the object are done*/
	}

	/*get current segment and move to next one*/
	cur = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);
	ctrl->current_seg ++;

	/*resync in case we have been issuing a play range over several segments*/
	for (i=ctrl->current_seg; i<count; i++) {
		next = (GF_Segment *)gf_list_get(ctrl->seg, i);
		if (
		    /*if next seg start is after cur seg start*/
		    (cur->startTime < next->startTime)
		    /*if next seg start is before cur seg end*/
		    && (cur->startTime + cur->Duration > next->startTime)
		    /*if next seg start is already passed*/
		    && (1000*next->startTime < odm->media_current_time)
		    /*then next segment was taken into account when requesting play*/
		) {
			cur = next;
			ctrl->current_seg ++;
		}
	}
	/*if last segment in ctrl is done, end of stream*/
	if (ctrl->current_seg >= count) return 0;
	next = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);

	/*if next seg start is not in current seg, media needs restart*/
	if ((next->startTime < cur->startTime) || (cur->startTime + cur->Duration < next->startTime))
		mediacontrol_restart(odm);

	return 1;
}


#endif /*GPAC_DISABLE_VRML*/

#endif //!defined(GPAC_DISABLE_COMPOSITOR)
