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

#if !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)

typedef struct
{
	GF_Compositor *compositor;
	GF_TimeNode time_handle;
	Double start_time;
	GF_MediaObject *stream;
	MFURL current_url;
} AnimationStreamStack;

static void animationstream_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		M_AnimationStream *as = (M_AnimationStream *)node;
		AnimationStreamStack *st = (AnimationStreamStack *) gf_node_get_private(node);

		if (st->time_handle.is_registered) {
			gf_sc_unregister_time_node(st->compositor, &st->time_handle);
		}
		if (st->stream && as->isActive) {
			gf_mo_set_flag(st->stream, GF_MO_DISPLAY_REMOVE, 1);
			gf_mo_stop(&st->stream);
		}
		gf_sg_vrml_mf_reset(&st->current_url, GF_SG_VRML_MFURL);
		gf_free(st);
	}
}


static void animationstream_check_url(AnimationStreamStack *stack, M_AnimationStream *as)
{
	if (!stack->stream) {
		gf_sg_vrml_mf_reset(&stack->current_url, GF_SG_VRML_MFURL);
		gf_sg_vrml_field_copy(&stack->current_url, &as->url, GF_SG_VRML_MFURL);
		stack->stream = gf_mo_register((GF_Node *)as, &as->url, 0, 0);
		gf_sc_invalidate(stack->compositor, NULL);

		/*if changed while playing trigger*/
		if (as->isActive) {
			gf_mo_play(stack->stream, 0, -1, 0);
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
			gf_mo_set_flag(stack->stream, GF_MO_DISPLAY_REMOVE, 1);
			gf_mo_stop(&stack->stream);
		}
		gf_mo_unregister((GF_Node *)as, stack->stream);

		stack->stream = gf_mo_register((GF_Node *)as, &as->url, 0, 0);
		/*if changed while playing play new source*/
		if (as->isActive) {
			gf_mo_play(stack->stream, 0, -1, 0);
			gf_mo_set_speed(stack->stream, as->speed);
		}
		gf_sc_invalidate(stack->compositor, NULL);
	}
}

static Fixed animationstream_get_speed(AnimationStreamStack *stack, M_AnimationStream *as)
{
	return gf_mo_get_speed(stack->stream, as->speed);
}
static Bool animationstream_get_loop(AnimationStreamStack *stack, M_AnimationStream *as)
{
	return gf_mo_get_loop(stack->stream, as->loop);
}

static void animationstream_activate(AnimationStreamStack *stack, M_AnimationStream *as)
{
	animationstream_check_url(stack, as);
	as->isActive = 1;
	gf_node_event_out((GF_Node*)as, 6/*"isActive"*/);

	gf_mo_play(stack->stream, 0, -1, 0);
	gf_mo_set_speed(stack->stream, as->speed);
}

static void animationstream_deactivate(AnimationStreamStack *stack, M_AnimationStream *as)
{
	Bool no_redraw = GF_FALSE;
	//if dynamic root scene and this no more active streams, do not force a redraw
	if (stack->stream && stack->stream->odm && stack->compositor->root_scene->is_dynamic_scene) {
		if (!gf_list_count(stack->compositor->systems_pids))
			no_redraw = GF_TRUE;
	}
	
	if (as->isActive) {
		as->isActive = 0;
		gf_node_event_out((GF_Node*)as, 6/*"isActive"*/);
	}
	if (stack->stream) {
		if (gf_mo_url_changed(stack->stream, &as->url))
			gf_mo_set_flag(stack->stream, GF_MO_DISPLAY_REMOVE, 1);
		gf_mo_stop(&stack->stream);
	}
	stack->time_handle.needs_unregister = 1;
	if (!no_redraw)
		gf_sc_invalidate(stack->compositor, NULL);
}

static void animationstream_update_time(GF_TimeNode *st)
{
	Double time;
	M_AnimationStream *as = (M_AnimationStream *)st->udta;
	AnimationStreamStack *stack = (AnimationStreamStack *)gf_node_get_private(st->udta);

	/*not active, store start time and speed*/
	if ( ! as->isActive) {
		stack->start_time = as->startTime;
	}
	time = gf_node_get_scene_time(st->udta);

	if ((time < stack->start_time) || (stack->start_time < 0)) return;

	if (animationstream_get_speed(stack, as) && as->isActive) {
		//if stoptime is reached (>startTime) deactivate
		if ((as->stopTime > stack->start_time) && (time >= as->stopTime) ) {
			animationstream_deactivate(stack, as);
			return;
		}
		if (gf_mo_is_done(stack->stream)) {
			if (animationstream_get_loop(stack, as)) {
				gf_mo_restart(stack->stream);
			} else if (gf_mo_should_deactivate(stack->stream)) {
				animationstream_deactivate(stack, as);
			}
		}
	}

	/*we're (about to be) active: VRML:
	"A time-dependent node is inactive until its startTime is reached. When time now becomes greater than or
	equal to startTime, an isActive TRUE event is generated and the time-dependent node becomes active 	*/
	if (!as->isActive && !st->needs_unregister) animationstream_activate(stack, as);
}


void compositor_init_animationstream(GF_Compositor *compositor, GF_Node *node)
{
	AnimationStreamStack *st;
	GF_SAFEALLOC(st, AnimationStreamStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate AnimationStream stack\n"));
		return;
	}
	st->compositor = compositor;
	st->time_handle.UpdateTimeNode = animationstream_update_time;
	st->time_handle.udta = node;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, animationstream_destroy);

	gf_sc_register_time_node(compositor, &st->time_handle);
}



void compositor_animationstream_modified(GF_Node *node)
{
	M_AnimationStream *as = (M_AnimationStream *)node;
	AnimationStreamStack *st = (AnimationStreamStack *) gf_node_get_private(node);
	if (!st) return;

	/*update state if we're active*/
	if (as->isActive)
		animationstream_update_time(&st->time_handle);

	/*check URL change*/
	animationstream_check_url(st, as);

	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister)
		gf_sc_register_time_node(st->compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = 0;
}

#endif // !defined(GPAC_DISABLE_VRML) && !defined(GPAC_DISABLE_COMPOSITOR)
