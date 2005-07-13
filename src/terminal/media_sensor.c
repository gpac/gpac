/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

static void DestroyMediaSensor(GF_Node *node)
{
	MediaSensorStack *st = gf_node_get_private(node);

	/*unlink from OD*/
	if (st->stream && st->stream->odm) 
		gf_list_del_item(st->stream->odm->ms_stack, st);

	gf_list_del(st->seg);
	free(st);
}

/*render : setup media sensor and update timing in case of inline scenes*/
void RenderMediaSensor(GF_Node *node, void *rs)
{
	GF_Clock *ck;
	MediaSensorStack *st = gf_node_get_private(node);

	if (!st->stream) st->stream = gf_mo_find(node, &st->sensor->url);
	if (!st->stream) return;
	if (!st->stream->odm) return;
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
		ck = st->stream->odm->subscene->scene_codec->ck;
		/*since audio may be used alone through an inline scene, we need to refresh the graph*/
		if (st->stream->odm->is_open) gf_term_invalidate_renderer(st->stream->term);
	}
	/*check anim streams*/
	else if (st->stream->odm->codec && (st->stream->odm->codec->type==GF_STREAM_SCENE)) ck = st->stream->odm->codec->ck;
	/*check OCR streams*/
	else if (st->stream->odm->ocr_codec) ck = st->stream->odm->ocr_codec->ck;

	if (ck && gf_clock_is_started(ck) ) {
		st->stream->odm->current_time = gf_clock_time(ck);
		MS_UpdateTiming(st->stream->odm);
	}
}

void InitMediaSensor(GF_InlineScene *is, GF_Node *node)
{
	MediaSensorStack *st = malloc(sizeof(MediaSensorStack));
	memset(st, 0, sizeof(MediaSensorStack));

	st->parent = is;
	st->sensor = (M_MediaSensor *)node;
	st->seg = gf_list_new();
	gf_node_set_render_function(node, RenderMediaSensor);
	gf_node_set_predestroy_function(node, DestroyMediaSensor);
	gf_node_set_private(node, st);

}

/*only URL can be changed, so reset and get new URL*/
void MS_Modified(GF_Node *node)
{
	MediaSensorStack *st = gf_node_get_private(node);
	if (!st) return;
	
	while (gf_list_count(st->seg)) gf_list_rem(st->seg, 0);

	/*unlink from OD*/
	if (st->stream && st->stream->odm) 
		gf_list_del_item(st->stream->odm->ms_stack, st);

	st->stream = gf_mo_find(node, &st->sensor->url);
	st->is_init = 0;
	gf_term_invalidate_renderer(st->parent->root_od->term);
}

void MS_UpdateTiming(GF_ObjectManager *odm)
{
	GF_Segment *desc;
	u32 i, count, j, ms_count;
	Double time;
	ms_count = gf_list_count(odm->ms_stack);
	if (!ms_count) return;
	
	time = odm->current_time / 1000.0;
	for (j=0; j<ms_count; j++) {
		MediaSensorStack *media_sens = (MediaSensorStack *)gf_list_get(odm->ms_stack, j);
		if (!media_sens->is_init) continue;
		count = gf_list_count(media_sens->seg);
		if (media_sens->active_seg==count) {
			/*full object controled*/
			if (!count) {
				if (!media_sens->sensor->isActive) {
					media_sens->sensor->isActive = 1;
					gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
					if (odm->subscene) {
						media_sens->sensor->mediaDuration = odm->subscene->duration;
					} else {
						media_sens->sensor->mediaDuration = odm->duration;
					}
					media_sens->sensor->mediaDuration /= 1000;
					gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaDuration");
				}
				if (media_sens->sensor->mediaCurrentTime != time) {
					media_sens->sensor->mediaCurrentTime = time;
					gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaCurrentTime");
				}
				/*check for end of scene (MediaSensor on inline)*/
				if (odm->subscene && odm->subscene->duration) {
					GF_Clock *ck = gf_odm_get_media_clock(odm);
					if (ck->has_seen_eos && media_sens->sensor->isActive && (1000*time>odm->subscene->duration)) {
						media_sens->sensor->isActive = 0;
						gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
					}
				}
				continue;
			}
		}


		/*locate segment*/
		for (i=media_sens->active_seg; i<count; i++) {
			desc = gf_list_get(media_sens->seg, i);
			/*not controled*/
			if (desc->startTime > time) {
				if (media_sens->sensor->isActive) {
					media_sens->sensor->isActive = 0;
					gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
				}
				break;
			}
			if (desc->startTime + desc->Duration < time) continue;
			/*segment switch, force activation (isActive TRUE send at each seg)*/
			if (media_sens->active_seg != i) {
				media_sens->active_seg = i;
				media_sens->sensor->isActive = 0;
			}

			if (!media_sens->sensor->isActive) {
				media_sens->sensor->isActive = 1;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
				/*set info*/
				gf_sg_vrml_mf_reset(& media_sens->sensor->info, GF_SG_VRML_MFSTRING);
				gf_sg_vrml_mf_alloc(& media_sens->sensor->info, GF_SG_VRML_MFSTRING, 1);
				media_sens->sensor->info.vals[0] = desc->SegmentName ? strdup(desc->SegmentName) : NULL;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "info");
				/*set duration*/
				media_sens->sensor->mediaDuration = desc->Duration;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaDuration");
				/*set seg start time*/
				media_sens->sensor->streamObjectStartTime = desc->startTime;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "streamObjectStartTime");
			}
			/*set media time - relative to segment start time*/
			time -= desc->startTime;
			if (media_sens->sensor->mediaCurrentTime != time) {
				media_sens->sensor->mediaCurrentTime = time;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaCurrentTime");
			}
			break;
		}
		if (i==count) {
			/*we're after last segment, deactivate*/
			if (media_sens->sensor->isActive) {
				media_sens->sensor->isActive = 0;
				gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
				media_sens->active_seg = count;
			}
		}
	}
}

void MS_Stop(MediaSensorStack *st)
{
	if (st->sensor->isActive) {
		st->sensor->isActive = 0;
		gf_node_event_out_str((GF_Node *) st->sensor, "isActive");
	}
	st->active_seg = 0;
}
