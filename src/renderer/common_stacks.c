/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#include "common_stacks.h"

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

void gf_sr_on_node_init(GF_Renderer *sr, GF_Node *node)
{
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_AnimationStream: InitAnimationStream(sr, node); break;
	case TAG_MPEG4_AudioBuffer: InitAudioBuffer(sr, node); break;
	case TAG_MPEG4_AudioSource: InitAudioSource(sr, node); break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: InitAudioClip(sr, node); break;
	case TAG_MPEG4_TimeSensor: case TAG_X3D_TimeSensor: InitTimeSensor(sr, node); break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: InitImageTexture(sr, node); break;
	case TAG_MPEG4_PixelTexture: case TAG_X3D_PixelTexture: InitPixelTexture(sr, node); break;
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: InitMovieTexture(sr, node); break;
	default:
		sr->visual_renderer->NodeInit(sr->visual_renderer, node);
		break;
	}
}

void gf_sr_invalidate(GF_Renderer *sr, GF_Node *byObj)
{

	if (!byObj) {
		sr->draw_next_frame = 1;
		return;
	}
	switch (gf_node_get_tag(byObj)) {
	case TAG_MPEG4_AnimationStream: AnimationStreamModified(byObj); break;
	case TAG_MPEG4_AudioBuffer: AudioBufferModified(byObj); break;
	case TAG_MPEG4_AudioSource: AudioSourceModified(byObj); break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: AudioClipModified(byObj); break;
	case TAG_MPEG4_TimeSensor: case TAG_X3D_TimeSensor: TimeSensorModified(byObj); break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: ImageTextureModified(byObj); break;
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: MovieTextureModified(byObj); break;
	default:
		/*for all nodes, invalidate parent graph - note we do that for sensors as well to force recomputing
		sensor list cached at grouping node level*/
		if (!sr->visual_renderer->NodeChanged(sr->visual_renderer, byObj)) {
			gf_node_dirty_set(byObj, 0, 1);
			sr->draw_next_frame = 1;
		}
		break;
	}
}

typedef struct
{
	GF_Node *owner;
	GF_Renderer *compositor;
	GF_TimeNode time_handle;
	Double start_time;
	GF_MediaObject *stream;
	MFURL current_url;
} AnimationStreamStack;

static void DestroyAnimationStream(GF_Node *node)
{
	AnimationStreamStack *st = (AnimationStreamStack *) gf_node_get_private(node);

	if (st->time_handle.is_registered) {
		gf_sr_unregister_time_node(st->compositor, &st->time_handle);
	}
	if (st->stream && st->stream->num_open) {
		st->stream->mo_flags |= GF_MO_DISPLAY_REMOVE;
		gf_mo_stop(st->stream);
	}
	gf_sg_vrml_mf_reset(&st->current_url, GF_SG_VRML_MFURL);
	free(st);
}


static void AS_CheckURL(AnimationStreamStack *stack, M_AnimationStream *as)
{
	if (!stack->stream) {
		gf_sg_vrml_mf_reset(&stack->current_url, GF_SG_VRML_MFURL);
		gf_sg_vrml_field_copy(&stack->current_url, &as->url, GF_SG_VRML_MFURL);
		stack->stream = gf_mo_find((GF_Node *)as, &as->url);
		gf_sr_invalidate(stack->compositor, NULL);

		/*if changed while playing trigger*/
		if (as->isActive) {
			gf_mo_play(stack->stream);
			gf_mo_set_speed(stack->stream, as->speed);
		}
		return;
	}
	/*check change*/
	if (gf_mo_url_changed(stack->stream, &as->url)) {
		gf_sg_vrml_mf_reset(&stack->current_url, GF_SG_VRML_MFURL);
		gf_sg_vrml_field_copy(&stack->current_url, &as->url, GF_SG_VRML_MFURL);
		/*if changed while playing stop old source*/
		if (as->isActive) {
			stack->stream->mo_flags |= GF_MO_DISPLAY_REMOVE;
			gf_mo_stop(stack->stream);
		}
		stack->stream = gf_mo_find((GF_Node *)as, &as->url);
		/*if changed while playing play new source*/
		if (as->isActive) {
			gf_mo_play(stack->stream);
			gf_mo_set_speed(stack->stream, as->speed);
		}
		gf_sr_invalidate(stack->compositor, NULL);
	}
}

static Fixed AS_GetSpeed(AnimationStreamStack *stack, M_AnimationStream *as)
{
	return gf_mo_get_speed(stack->stream, as->speed);
}
static Bool AS_GetLoop(AnimationStreamStack *stack, M_AnimationStream *as)
{
	return gf_mo_get_loop(stack->stream, as->loop);
}

static void AS_Activate(AnimationStreamStack *stack, M_AnimationStream *as)
{
	AS_CheckURL(stack, as);
	as->isActive = 1;
	gf_node_event_out_str((GF_Node*)as, "isActive");

	gf_mo_play(stack->stream);
	gf_mo_set_speed(stack->stream, as->speed);
}

static void AS_Deactivate(AnimationStreamStack *stack, M_AnimationStream *as)
{
	if (as->isActive) {
		as->isActive = 0;
		gf_node_event_out_str((GF_Node*)as, "isActive");
	}
	if (stack->stream) {
		if (gf_mo_url_changed(stack->stream, &as->url)) 
			stack->stream->mo_flags |= GF_MO_DISPLAY_REMOVE;
		gf_mo_stop(stack->stream);
	}
	stack->time_handle.needs_unregister = 1;
	gf_sr_invalidate(stack->compositor, NULL);
}

static void AS_UpdateTime(GF_TimeNode *st)
{
	Double time;
	M_AnimationStream *as = (M_AnimationStream *)st->obj;
	AnimationStreamStack *stack = gf_node_get_private(st->obj);
	
	/*not active, store start time and speed*/
	if ( ! as->isActive) {
		stack->start_time = as->startTime;
	}
	time = gf_node_get_scene_time(st->obj);

	if ((time < stack->start_time) || (stack->start_time < 0)) return;

	if (AS_GetSpeed(stack, as) && as->isActive) {
		//if stoptime is reached (>startTime) deactivate	
		if ((as->stopTime > stack->start_time) && (time >= as->stopTime) ) {
			AS_Deactivate(stack, as);
			return;
		}
		if (gf_mo_is_done(stack->stream)) {
			if (AS_GetLoop(stack, as)) {
				gf_mo_restart(stack->stream);
			} else if (gf_mo_should_deactivate(stack->stream)) {
				AS_Deactivate(stack, as);
			}
		}
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or 
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/
	if (!as->isActive && !st->needs_unregister) AS_Activate(stack, as);
}


void InitAnimationStream(GF_Renderer *sr, GF_Node *node)
{
	AnimationStreamStack *st = malloc(sizeof(AnimationStreamStack));
	memset(st, 0, sizeof(AnimationStreamStack));
	gf_sr_traversable_setup(st, node, sr);
	st->time_handle.UpdateTimeNode = AS_UpdateTime;
	st->time_handle.obj = node;
	
	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyAnimationStream);
	
	gf_sr_register_time_node(sr, &st->time_handle);
}



void AnimationStreamModified(GF_Node *node)
{
	M_AnimationStream *as = (M_AnimationStream *)node;
	AnimationStreamStack *st = (AnimationStreamStack *) gf_node_get_private(node);
	if (!st) return;

	/*update state if we're active*/
	if (as->isActive) 
		AS_UpdateTime(&st->time_handle);

	/*check URL change*/
	AS_CheckURL(st, as);

	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister) 
		gf_sr_register_time_node(st->compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = 0;
}

typedef struct
{
	GF_TimeNode time_handle;
	Bool store_info;
	Double start_time, cycle_interval;
	u32 num_cycles;
	GF_Renderer *compositor;
} TimeSensorStack;

static
void DestroyTimeSensor(GF_Node *ts)
{
	TimeSensorStack *st = gf_node_get_private(ts);
	if (st->time_handle.is_registered) {
		gf_sr_unregister_time_node(st->compositor, &st->time_handle);
	}
	free(st);
}


static
void ts_deactivate(TimeSensorStack *stack, M_TimeSensor *ts)
{
	ts->isActive = 0;
	gf_node_event_out_str((GF_Node *) ts, "isActive");
	assert(stack->time_handle.is_registered);
	stack->time_handle.needs_unregister = 1;
	stack->num_cycles = 0;
}

static
void UpdateTimeSensor(GF_TimeNode *st)
{
	Double currentTime, cycleTime;
	Fixed newFraction;
	u32 inc;
	M_TimeSensor *TS = (M_TimeSensor *)st->obj;
	TimeSensorStack *stack = gf_node_get_private(st->obj);

	if (! TS->enabled) {
		if (TS->isActive) {
			TS->cycleTime = gf_node_get_scene_time(st->obj);
			gf_node_event_out_str(st->obj, "cycleTime");
			ts_deactivate(stack, TS);
		}
		return;
	}

	if (stack->store_info) {
		stack->store_info = 0;
		stack->start_time = TS->startTime;
		stack->cycle_interval = TS->cycleInterval;
	}
	
	currentTime = gf_node_get_scene_time(st->obj);
	if (currentTime < stack->start_time) return;
	/*special case: if we're greater than both start and stop time don't activate*/
	else if (!TS->isActive && (TS->stopTime > stack->start_time) && (currentTime >= TS->stopTime)) {
		stack->time_handle.needs_unregister = 1;
		return;
	}

	cycleTime = currentTime - stack->start_time - stack->num_cycles * stack->cycle_interval;
	newFraction = FLT2FIX ( fmod(cycleTime, stack->cycle_interval) / stack->cycle_interval );

	if (TS->isActive) {
		TS->time = currentTime;
		gf_node_event_out_str(st->obj, "time");

		/*VRML:
			"f = fmod( (now - startTime) , cycleInterval) / cycleInterval
			if (f == 0.0 && now > startTime) fraction_changed = 1.0
			else fraction_changed = f"
		*/
		if (!newFraction && (currentTime > stack->start_time ) ) newFraction = FIX_ONE;

		/*check for deactivation - if so generate a fraction_changed AS IF NOW WAS EXACTLY STOPTIME*/
		if ((TS->stopTime > stack->start_time) && (currentTime >= TS->stopTime) ) {
			cycleTime = TS->stopTime - stack->start_time - stack->num_cycles * stack->cycle_interval;
			TS->fraction_changed = FLT2FIX( fmod(cycleTime, stack->cycle_interval) / stack->cycle_interval );
			/*cf above*/
			if (TS->fraction_changed < FIX_EPSILON) TS->fraction_changed = FIX_ONE;
			gf_node_event_out_str(st->obj, "fraction_changed");
			ts_deactivate(stack, TS);
			return;
		}
		if (! TS->loop) {
			if (cycleTime >= stack->cycle_interval) {
				TS->fraction_changed = FIX_ONE;
				gf_node_event_out_str(st->obj, "fraction_changed");
				ts_deactivate(stack, TS);
				return;
			}
		}
		TS->fraction_changed = newFraction;
		gf_node_event_out_str(st->obj, "fraction_changed");
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or 
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/

	if (!TS->isActive) {
		st->needs_unregister = 0;
		TS->isActive = 1;
		gf_node_event_out_str(st->obj, "isActive");
		TS->cycleTime = currentTime;
		gf_node_event_out_str(st->obj, "cycleTime");
		TS->fraction_changed = newFraction;
		gf_node_event_out_str(st->obj, "fraction_changed");
	}

	//compute cycle time
	if (TS->loop && (cycleTime >= stack->cycle_interval) ) {
		inc = 1 + (u32) ( (cycleTime - stack->cycle_interval ) / stack->cycle_interval );
		stack->num_cycles += inc;
		cycleTime -= inc*stack->cycle_interval;
		TS->cycleTime = currentTime - cycleTime;
		gf_node_event_out_str(st->obj, "cycleTime");
	}
}

void InitTimeSensor(GF_Renderer *sr, GF_Node *node)
{
	TimeSensorStack *st = malloc(sizeof(TimeSensorStack));
	memset(st, 0, sizeof(TimeSensorStack));
	st->time_handle.UpdateTimeNode = UpdateTimeSensor;
	st->time_handle.obj = node;
	st->store_info = 1;
	st->compositor = sr;

	gf_node_set_private(node, st);
	gf_node_set_predestroy_function(node, DestroyTimeSensor);
	/*time sensor needs to be run only if def'ed, otherwise it doesn't impact scene*/
	if (gf_node_get_id(node)) gf_sr_register_time_node(sr, &st->time_handle);
}


void TimeSensorModified(GF_Node *t)
{
	M_TimeSensor *ts = (M_TimeSensor *)t;
	TimeSensorStack *stack = (TimeSensorStack *) gf_node_get_private(t);
	if (!stack) return;

	if (ts->isActive) UpdateTimeSensor(&stack->time_handle);

	if (!ts->isActive) stack->store_info = 1;

	if (ts->enabled) {
		stack->time_handle.needs_unregister = 0;
		if (!stack->time_handle.is_registered) {
			gf_sr_register_time_node(stack->compositor, &stack->time_handle);
		}
	}
}
