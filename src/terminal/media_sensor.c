/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#include "media_control.h"
#include <gpac/constants.h>
#include <gpac/internal/compositor_dev.h>

#ifndef GPAC_DISABLE_VRML

/*render : setup media sensor and update timing in case of inline scenes*/
void RenderMediaSensor(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	GF_Clock *ck;
	MediaSensorStack *st = (MediaSensorStack *)gf_node_get_private(node);

	if (is_destroy) {
		/*unlink from OD*/
		if (st->stream && st->stream->odm)
			gf_list_del_item(st->stream->odm->ms_stack, st);

		gf_list_del(st->seg);
		gf_free(st);
		return;
	}
	//we need to disable culling otherwise we may never be called back again ...
	tr_state->disable_cull = 1;

	if (!st->stream) st->stream = gf_mo_register(node, &st->sensor->url, 0, 0);
	if (!st->stream || !st->stream->odm) return;

	if (!st->is_init) {
		gf_list_add(st->stream->odm->ms_stack, st);
		gf_odm_init_segments(st->stream->odm, st->seg, &st->sensor->url);
		st->is_init = 1;
		st->active_seg = 0;

	}
	/*media sensor bound to natural media (audio, video) is updated when fetching the stream
	data for rendering.*/

	ck = NULL;
	/*check inline scenes - if the scene is set to restart DON'T MODIFY SENSOR: since we need a 2 render
	passes to restart inline, scene is considered as not running*/
	if (st->stream->odm->subscene && !st->stream->odm->subscene->needs_restart) {
		if (st->stream->odm->subscene->scene_codec) ck = st->stream->odm->subscene->scene_codec->ck;
		/*dynamic scene*/
		else ck = st->stream->odm->subscene->dyn_ck;
		/*since audio may be used alone through an inline scene, we need to refresh the graph*/
		if (ck && !ck->has_seen_eos && st->stream->odm->state) gf_term_invalidate_compositor(st->stream->odm->term);
	}
	/*check anim streams*/
	else if (st->stream->odm->codec && (st->stream->odm->codec->type==GF_STREAM_SCENE)) ck = st->stream->odm->codec->ck;
	/*check OCR streams*/
	else if (st->stream->odm->ocr_codec) ck = st->stream->odm->ocr_codec->ck;

	if (ck && gf_clock_is_started(ck) ) {
		st->stream->odm->current_time = gf_clock_time(ck);
		mediasensor_update_timing(st->stream->odm, 0);
	}
}

void InitMediaSensor(GF_Scene *scene, GF_Node *node)
{
	MediaSensorStack *st;
	GF_SAFEALLOC(st, MediaSensorStack);

	st->parent = scene;
	st->sensor = (M_MediaSensor *)node;
	st->seg = gf_list_new();
	gf_node_set_callback_function(node, RenderMediaSensor);
	gf_node_set_private(node, st);

}

/*only URL can be changed, so reset and get new URL*/
void MS_Modified(GF_Node *node)
{
	MediaSensorStack *st = (MediaSensorStack *)gf_node_get_private(node);
	if (!st) return;

	while (gf_list_count(st->seg)) gf_list_rem(st->seg, 0);

	if (st->stream) {
		/*unlink from OD*/
		if (st->stream->odm && st->stream->odm->ms_stack)
			gf_list_del_item(st->stream->odm->ms_stack, st);

		gf_mo_unregister(node, st->stream);
		if (st->sensor->isActive) {
			st->sensor->isActive = 0;
			gf_node_event_out((GF_Node *) st->sensor, 4/*"isActive"*/);
		}
	}
	st->stream = NULL;
	st->is_init = 0;
	gf_term_invalidate_compositor(st->parent->root_od->term);
}


static void media_sensor_activate(MediaSensorStack *media_sens, GF_Segment *desc)
{
	media_sens->sensor->isActive = 1;
	gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
	/*set info*/
	gf_sg_vrml_mf_reset(& media_sens->sensor->info, GF_SG_VRML_MFSTRING);
	gf_sg_vrml_mf_alloc(& media_sens->sensor->info, GF_SG_VRML_MFSTRING, 1);
	media_sens->sensor->info.vals[0] = desc->SegmentName ? gf_strdup(desc->SegmentName) : NULL;
	gf_node_event_out((GF_Node *) media_sens->sensor, 5/*"info"*/);
	/*set duration*/
	media_sens->sensor->mediaDuration = desc->Duration;
	gf_node_event_out((GF_Node *) media_sens->sensor, 3/*"mediaDuration"*/);
	/*set seg start time*/
	media_sens->sensor->streamObjectStartTime = desc->startTime;
	gf_node_event_out((GF_Node *) media_sens->sensor, 2/*"streamObjectStartTime"*/);
}

void mediasensor_update_timing(GF_ObjectManager *odm, Bool is_eos)
{
	GF_Segment *desc;
	u32 i, count, j, ms_count;
	Double time;
	ms_count = gf_list_count(odm->ms_stack);
	if (!ms_count) return;

	time = odm->current_time / 1000.0;
	//dirty hack to get timing of frame when very late (openhevc debug)
	if (odm->subscene && odm->subscene->dyn_ck && odm->subscene->dyn_ck->last_TS_rendered)
		time = odm->subscene->dyn_ck->last_TS_rendered / 1000.0;

	for (j=0; j<ms_count; j++) {
		MediaSensorStack *media_sens = (MediaSensorStack *)gf_list_get(odm->ms_stack, j);
		if (!media_sens->is_init) continue;
		count = gf_list_count(media_sens->seg);

		/*full object controled*/
		if (!media_sens->active_seg && !count) {
			/*check for end of scene (MediaSensor on inline)*/
			if (odm->subscene && odm->subscene->duration) {
				GF_Clock *ck = gf_odm_get_media_clock(odm);
				if (ck->has_seen_eos && (1000*time>=(Double) (s64)odm->subscene->duration)) {
					if (media_sens->sensor->isActive) {
						/*force notification of time (ntify the scene duration rather than the current clock*/
						media_sens->sensor->mediaCurrentTime = (Double) odm->subscene->duration;
						media_sens->sensor->mediaCurrentTime /= 1000;
						gf_node_event_out((GF_Node *) media_sens->sensor, 1/*"mediaCurrentTime"*/);
						media_sens->sensor->isActive = 0;
						gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);

						GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[ODM%d] Deactivating media sensor\n", odm->OD->objectDescriptorID));
					}
					continue;
				}
			}

			if (!is_eos && !media_sens->sensor->isActive) {
				media_sens->sensor->isActive = 1;
				gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);

				gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
				if (odm->subscene) {
					media_sens->sensor->mediaDuration = (Double) (s64)odm->subscene->duration;
				} else {
					media_sens->sensor->mediaDuration = (Double) (s64)odm->duration;
				}
				if (media_sens->sensor->mediaDuration)
					media_sens->sensor->mediaDuration /= 1000;
				else
					media_sens->sensor->mediaDuration = -FIX_ONE;

				gf_node_event_out((GF_Node *) media_sens->sensor, 3/*"mediaDuration"*/);
			}

			if (is_eos && media_sens->sensor->isActive) {
				if (media_sens->sensor->mediaDuration>=0) {
					media_sens->sensor->mediaCurrentTime = media_sens->sensor->mediaDuration;
				} else {
					media_sens->sensor->mediaCurrentTime = time;
				}
				gf_node_event_out((GF_Node *) media_sens->sensor, 1/*"mediaCurrentTime"*/);
				media_sens->sensor->isActive = 0;
				gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
			} else {
				if (media_sens->sensor->isActive && (media_sens->sensor->mediaCurrentTime != time)) {
					media_sens->sensor->mediaCurrentTime = time;
					gf_node_event_out((GF_Node *) media_sens->sensor, 1/*"mediaCurrentTime"*/);
				}
			}
			continue;
		}

		/*locate segment*/
		for (i=media_sens->active_seg; i<count; i++) {
			desc = (GF_Segment*)gf_list_get(media_sens->seg, i);
			/*not controled*/
			if (desc->startTime > time) {
				if (media_sens->sensor->isActive) {
					media_sens->sensor->isActive = 0;
					gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);

					GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[ODM%d] Deactivating media sensor at time %g - segment %s\n", odm->OD->objectDescriptorID, time, desc->SegmentName));
				}
				continue;
			}
			if (desc->startTime + desc->Duration < time) continue;
			if (desc->startTime + desc->Duration == time) {
				continue;
			}
			/*segment switch, force activation (isActive TRUE send at each seg)*/
			if (media_sens->active_seg != i) {
				media_sens->active_seg = i;
				media_sens->sensor->isActive = 0;
			}

			if (!media_sens->sensor->isActive) {
				media_sensor_activate(media_sens, desc);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[ODM%d] Activating media sensor time %g - segment %s\n", odm->OD->objectDescriptorID, time, desc->SegmentName));
			}

			/*set media time - relative to segment start time*/
			time -= desc->startTime;
			if (media_sens->sensor->mediaCurrentTime != time) {
				media_sens->sensor->mediaCurrentTime = time;
				gf_node_event_out((GF_Node *) media_sens->sensor, 1/*"mediaCurrentTime"*/);
			}
			break;
		}
		if (i==count) {
			/*we're after last segment, deactivate*/
			if (media_sens->sensor->isActive) {
				media_sens->sensor->isActive = 0;
				gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
				media_sens->active_seg = count;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[ODM%d] Deactivating media sensor at time %g: no more segments\n", odm->OD->objectDescriptorID, time));
			}
		}
	}
}

void MS_Stop(MediaSensorStack *st)
{
	if (st->sensor->isActive) {
		st->sensor->isActive = 0;
		gf_node_event_out((GF_Node *) st->sensor, 4/*"isActive"*/);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[ODM%d] Deactivating media sensor\n", st->stream->odm->OD->objectDescriptorID));
	}
	st->active_seg = 0;
}

#endif /*GPAC_DISABLE_VRML*/
