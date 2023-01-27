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

#include "nodes_stacks.h"

#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

typedef struct
{
	GF_TimeNode time_handle;
	Bool store_info;
	Double start_time, cycle_interval;
	u32 num_cycles;
	GF_Compositor *compositor;
#ifndef GPAC_DISABLE_X3D
	Bool is_x3d;
#endif
} TimeSensorStack;

static void timesensor_destroy(GF_Node *ts, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		TimeSensorStack *st = (TimeSensorStack *)gf_node_get_private(ts);
		if (st->time_handle.is_registered) {
			gf_sc_unregister_time_node(st->compositor, &st->time_handle);
		}
		gf_free(st);
	}
}


static
void timesensor_deactivate(TimeSensorStack *stack, M_TimeSensor *ts)
{
	ts->isActive = 0;
	gf_node_event_out((GF_Node *) ts, 7);//"isActive"
	assert(stack->time_handle.is_registered);
	stack->time_handle.needs_unregister = 1;
	stack->num_cycles = 0;
}

static
void timesensor_update_time(GF_TimeNode *st)
{
	Double currentTime, cycleTime;
	Fixed newFraction;
	u32 inc;
	M_TimeSensor *TS = (M_TimeSensor *)st->udta;
	TimeSensorStack *stack = (TimeSensorStack *)gf_node_get_private(st->udta);

	if (! TS->enabled) {
		if (TS->isActive) {
			TS->cycleTime = gf_node_get_scene_time(st->udta);
			gf_node_event_out(st->udta, 5);//"cycleTime"
			timesensor_deactivate(stack, TS);
		}
		return;
	}

	if (stack->store_info) {
		stack->store_info = 0;
		stack->start_time = TS->startTime;
		stack->cycle_interval = TS->cycleInterval;
	}

	currentTime = gf_node_get_scene_time(st->udta);
	if (!TS->isActive) {
		if (currentTime < stack->start_time) return;
		/*special case: if we're greater than both start and stop time don't activate*/
		if (!TS->isActive && (TS->stopTime > stack->start_time) && (currentTime >= TS->stopTime)) {
			stack->time_handle.needs_unregister = 1;
			return;
		}
#ifndef GPAC_DISABLE_X3D
		if (stack->is_x3d && !TS->loop) {
			if (!stack->start_time && !gf_sys_old_arch_compat() ) return;
			if (currentTime >= TS->startTime+stack->cycle_interval) return;
		}
#endif
	}

	cycleTime = currentTime - stack->start_time - stack->num_cycles * stack->cycle_interval;
	newFraction = FLT2FIX ( fmod(cycleTime, stack->cycle_interval) / stack->cycle_interval );

	if (TS->isActive) {
		TS->time = currentTime;
		gf_node_event_out(st->udta, 8);//"time"

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
			gf_node_event_out(st->udta, 6);//"fraction_changed"
			timesensor_deactivate(stack, TS);
			return;
		}
		if (! TS->loop) {
			if (cycleTime >= stack->cycle_interval) {
				TS->fraction_changed = FIX_ONE;
				gf_node_event_out(st->udta, 6);//"fraction_changed"
				timesensor_deactivate(stack, TS);
				return;
			}
		}
		if (newFraction != TS->fraction_changed) {
			TS->fraction_changed = newFraction;
			gf_node_event_out(st->udta, 6);//"fraction_changed"
		}
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/

	if (!TS->isActive) {
		st->needs_unregister = 0;
		TS->isActive = 1;
		gf_node_event_out(st->udta, 7); //"isActive"
		TS->cycleTime = currentTime;
		gf_node_event_out(st->udta, 5);//"cycleTime"
		TS->fraction_changed = newFraction;
		gf_node_event_out(st->udta, 6);//"fraction_changed"
	}

	//compute cycle time
	if (TS->loop && (cycleTime >= stack->cycle_interval) ) {
		inc = 1 + (u32) ( (cycleTime - stack->cycle_interval ) / stack->cycle_interval );
		stack->num_cycles += inc;
		cycleTime -= inc*stack->cycle_interval;
		TS->cycleTime = currentTime - cycleTime;
		gf_node_event_out(st->udta, 5);//"cycleTime"
	}
}

void compositor_init_timesensor(GF_Compositor *compositor, GF_Node *node)
{
	TimeSensorStack *st;
	GF_SAFEALLOC(st, TimeSensorStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate time  sensor 2d stack\n"));
		return;
	}
	st->time_handle.UpdateTimeNode = timesensor_update_time;
	st->time_handle.udta = node;
	st->store_info = 1;
	st->compositor = compositor;
#ifndef GPAC_DISABLE_X3D
	st->is_x3d = (gf_node_get_tag(node)==TAG_X3D_TimeSensor) ? 1 : 0;
#endif
	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, timesensor_destroy);
	/*time sensor needs to be run only if def'ed, otherwise it doesn't impact scene*/
	//if (gf_node_get_id(node))
	gf_sc_register_time_node(compositor, &st->time_handle);
}


void compositor_timesensor_modified(GF_Node *t)
{
	M_TimeSensor *ts = (M_TimeSensor *)t;
	TimeSensorStack *stack = (TimeSensorStack *) gf_node_get_private(t);
	if (!stack) return;

	if (ts->isActive) timesensor_update_time(&stack->time_handle);

	if (!ts->isActive) stack->store_info = 1;

	if (ts->enabled) {
		stack->time_handle.needs_unregister = 0;
		if (!stack->time_handle.is_registered) {
			gf_sc_register_time_node(stack->compositor, &stack->time_handle);
		}
	}
}

#endif //!defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
