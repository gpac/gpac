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


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/modules/codec.h>
#include <gpac/nodes_x3d.h>
#include "media_memory.h"
#include "media_control.h"
#include <gpac/nodes_svg.h>
#include <gpac/network.h>


#ifndef GPAC_DISABLE_SVG
static GF_MediaObject *get_sync_reference(GF_Scene *scene, XMLRI *iri, u32 o_type, GF_Node *orig_ref, Bool *post_pone)
{
	MFURL mfurl;
	SFURL sfurl;
	GF_MediaObject *res;
	GF_Node *ref = NULL;

	u32 stream_id = 0;
	if (post_pone) *post_pone = GF_FALSE;
	
	if (iri->type==XMLRI_STREAMID) {
		stream_id = iri->lsr_stream_id;
	} else if (!iri->string) {
		return NULL;
	} else {
		if (iri->target) ref = (GF_Node *)iri->target;
		else if (iri->string[0]=='#') ref = gf_sg_find_node_by_name(scene->graph, iri->string+1);
		else ref = gf_sg_find_node_by_name(scene->graph, iri->string);

		if (ref) {
			GF_FieldInfo info;
			/*safety check, break cyclic references*/
			if (ref==orig_ref) return NULL;

			switch (ref->sgprivate->tag) {
			case TAG_SVG_audio:
				o_type = GF_MEDIA_OBJECT_AUDIO;
				if (gf_node_get_attribute_by_tag(ref, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &info)==GF_OK) {
					return get_sync_reference(scene, (XMLRI *)info.far_ptr, o_type, orig_ref ? orig_ref : ref, post_pone);
				}
				return NULL;
			case TAG_SVG_video:
				o_type = GF_MEDIA_OBJECT_VIDEO;
				if (gf_node_get_attribute_by_tag(ref, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &info)==GF_OK) {
					return get_sync_reference(scene, (XMLRI *)info.far_ptr, o_type, orig_ref ? orig_ref : ref, post_pone);
				}
				return NULL;
			default:
				return NULL;
			}
		}
	}
	*post_pone = GF_FALSE;
	mfurl.count = 1;
	mfurl.vals = &sfurl;
	mfurl.vals[0].OD_ID = stream_id;
	mfurl.vals[0].url = iri->string;

	res = gf_scene_get_media_object(scene, &mfurl, o_type, GF_FALSE);
	if (!res) *post_pone = GF_TRUE;
	return res;
}
#endif


GF_EXPORT
GF_MediaObject *gf_mo_register(GF_Node *node, MFURL *url, Bool lock_timelines, Bool force_new_res)
{
	u32 obj_type;
#ifndef GPAC_DISABLE_SVG
	Bool post_pone;
	GF_FieldInfo info;
#endif
	GF_Scene *scene;
	GF_MediaObject *res, *syncRef;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	scene = (GF_Scene*)gf_sg_get_private(sg);
	if (!scene) return NULL;

	syncRef = NULL;

	/*keep track of the kind of object expected if URL is not using OD scheme*/
	switch (gf_node_get_tag(node)) {
#ifndef GPAC_DISABLE_VRML
	/*MPEG-4 / VRML / X3D only*/
	case TAG_MPEG4_AudioClip:
	case TAG_MPEG4_AudioSource:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_AudioClip:
#endif
		obj_type = GF_MEDIA_OBJECT_AUDIO;
		break;
	case TAG_MPEG4_SBVCAnimation:
	case TAG_MPEG4_AnimationStream:
		obj_type = GF_MEDIA_OBJECT_UPDATES;
		break;
	case TAG_MPEG4_BitWrapper:
		obj_type = GF_MEDIA_OBJECT_SCENE;
		break;
	case TAG_MPEG4_InputSensor:
		obj_type = GF_MEDIA_OBJECT_INTERACT;
		break;
	case TAG_MPEG4_Background2D:
	case TAG_MPEG4_Background:
	case TAG_MPEG4_ImageTexture:
	case TAG_MPEG4_CacheTexture:
	case TAG_MPEG4_MovieTexture:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Background:
	case TAG_X3D_ImageTexture:
	case TAG_X3D_MovieTexture:
#endif
		obj_type = GF_MEDIA_OBJECT_VIDEO;
		break;
	case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
#endif
		obj_type = GF_MEDIA_OBJECT_SCENE;
		break;
#endif /*GPAC_DISABLE_VRML*/

		/*SVG*/
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_audio:
		obj_type = GF_MEDIA_OBJECT_AUDIO;
		if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_syncReference, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			syncRef = get_sync_reference(scene, (XMLRI *)info.far_ptr, GF_MEDIA_OBJECT_UNDEF, node, &post_pone);
			/*syncRef is specified but doesn't exist yet, post-pone*/
			if (post_pone) return NULL;
		}
		break;
	case TAG_SVG_video:
		obj_type = GF_MEDIA_OBJECT_VIDEO;
		if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_syncReference, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			syncRef = get_sync_reference(scene, (XMLRI *)info.far_ptr, GF_MEDIA_OBJECT_UNDEF, node, &post_pone);
			/*syncRef is specified but doesn't exist yet, post-pone*/
			if (post_pone) return NULL;
		}
		break;
	case TAG_SVG_image:
		obj_type = GF_MEDIA_OBJECT_VIDEO;
		break;
	case TAG_SVG_foreignObject:
	case TAG_SVG_animation:
		obj_type = GF_MEDIA_OBJECT_SCENE;
		break;
	case TAG_LSR_updates:
		obj_type = GF_MEDIA_OBJECT_UPDATES;
		break;
#endif

	default:
		obj_type = GF_MEDIA_OBJECT_UNDEF;
		break;
	}

	/*move to primary resource handler*/
	while (scene->secondary_resource && scene->root_od->parentscene)
		scene = scene->root_od->parentscene;

	res = gf_scene_get_media_object_ex(scene, url, obj_type, lock_timelines, syncRef, force_new_res, node);
	return res;
}


void gf_mo_unregister(GF_Node *node, GF_MediaObject *mo)
{
	if (mo && node) {
		gf_mo_event_target_remove_by_node(mo, node);
	}
}

GF_MediaObject *gf_mo_new()
{
	GF_MediaObject *mo;
	mo = (GF_MediaObject *) gf_malloc(sizeof(GF_MediaObject));
	memset(mo, 0, sizeof(GF_MediaObject));
	mo->speed = FIX_ONE;
	mo->URLs.count = 0;
	mo->URLs.vals = NULL;
	mo->evt_targets = gf_list_new();
	return mo;
}

GF_EXPORT
Bool gf_mo_get_visual_info(GF_MediaObject *mo, u32 *width, u32 *height, u32 *stride, u32 *pixel_ar, u32 *pixelFormat, Bool *is_flipped)
{
	if ((mo->type != GF_MEDIA_OBJECT_VIDEO) && (mo->type!=GF_MEDIA_OBJECT_TEXT)) return GF_FALSE;

	if (mo->config_changed) {
		gf_mo_update_caps(mo);
		mo->config_changed = GF_FALSE;
	}
	if (width) *width = mo->width;
	if (height) *height = mo->height;
	if (stride) *stride = mo->stride;
	if (pixel_ar) *pixel_ar = mo->pixel_ar;
	if (pixelFormat) *pixelFormat = mo->pixelformat;
	if (is_flipped) *is_flipped = mo->is_flipped;
	return GF_TRUE;
}

GF_EXPORT
void gf_mo_get_nb_views(GF_MediaObject *mo, int * nb_views)
{
	if (mo) *nb_views = mo->nb_views;
}

GF_EXPORT

void gf_mo_get_nb_layers(GF_MediaObject *mo, int * nb_layers)
{
	if (mo) *nb_layers = mo->nb_layers;
}

GF_EXPORT
Bool gf_mo_get_audio_info(GF_MediaObject *mo, u32 *sample_rate, u32 *bits_per_sample, u32 *num_channels, u32 *channel_config)
{
	if (!mo->odm || (mo->type != GF_MEDIA_OBJECT_AUDIO)) return GF_FALSE;

	if (mo->config_changed) {
		gf_mo_update_caps(mo);
		mo->config_changed = GF_FALSE;
	}

	if (sample_rate) *sample_rate = mo->sample_rate;
	if (bits_per_sample) *bits_per_sample = mo->bits_per_sample;
	if (num_channels) *num_channels = mo->num_channels;
	if (channel_config) *channel_config = mo->channel_config;
	return GF_TRUE;
}


void gf_mo_update_caps(GF_MediaObject *mo)
{
	const GF_PropertyValue *v;
	if (!mo->odm || !mo->odm->pid) return;

	if (mo->odm->type==GF_STREAM_VISUAL) {
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_WIDTH);
		if (v) mo->width = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_HEIGHT);
		if (v) mo->height = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_STRIDE);
		if (v) mo->stride = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_PIXFMT);
		if (v) mo->pixelformat = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_SAR);
		if (v) mo->pixel_ar = (v->value.frac.num) << 16 | (v->value.frac.den);

	} else if (mo->odm->type==GF_STREAM_AUDIO) {
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_SAMPLE_RATE);
		if (v) mo->sample_rate = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_NUM_CHANNELS);
		if (v) mo->num_channels = v->value.uint;
		v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (v) mo->channel_config = v->value.uint;

		mo->bits_per_sample = 16;

		mo->bytes_per_sec = mo->bits_per_sample * mo->num_channels * mo->sample_rate / 8;
	} else if (mo->odm->type==GF_STREAM_SCENE) {
		//nothing to do
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("NOT YET IMPLEMENTED IN FILTERS\n"));
	}
}

GF_EXPORT
char *gf_mo_fetch_data(GF_MediaObject *mo, GF_MOFetchMode resync, u32 upload_time_ms, Bool *eos, u32 *timestamp, u32 *size, s32 *ms_until_pres, s32 *ms_until_next, GF_MediaDecoderFrame **outFrame)
{

	u32 force_decode_mode = 0;
	u32 obj_time;
	s32 diff;
	Bool bench_mode, skip_resync;
	const char *data;
	const GF_PropertyValue *v;
	u32 timescale=0;
	u32 pck_ts=0, next_ts=0;
	GF_FilterPacket *next_pck=NULL;

	*eos = GF_FALSE;
	*timestamp = mo->timestamp;
	*size = mo->framesize;
	if (ms_until_pres) *ms_until_pres = mo->ms_until_pres;
	if (ms_until_next) *ms_until_next = mo->ms_until_next;
	if (outFrame) *outFrame = NULL;

	if (!mo->odm || !mo->odm->pid)
		return NULL;

	/*if frame locked return it*/
	if (mo->nb_fetch) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: CU already fetched, returning\n", mo->odm->ID));
		mo->nb_fetch ++;
		return mo->frame;
	}
	v = gf_filter_pid_get_property(mo->odm->pid, GF_PROP_PID_TIMESCALE);
	if (v) timescale = v->value.uint;
	if (!timescale) timescale=1;

	if ( gf_odm_check_buffering(mo->odm, NULL) ) {
		//if buffering, first frame fetched and still buffering return
		if (mo->pck && mo->odm->nb_buffering)
			return NULL;
	}

	if (!mo->pck) {
		mo->pck = gf_filter_pid_get_packet(mo->odm->pid);
		if (!mo->pck)
			return NULL;
		mo->pck = gf_filter_pck_ref(mo->pck);
		gf_filter_pid_drop_packet(mo->odm->pid);
	}
	data = gf_filter_pck_get_data(mo->pck, size);
	timescale = gf_filter_pck_get_timescale(mo->pck);

	mo->is_eos = GF_FALSE;
	if (!data) {
		Bool is_eos = gf_filter_pck_get_eos(mo->pck);
		if (!mo->is_eos && is_eos) {
			mo->is_eos = GF_TRUE;
			mediasensor_update_timing(mo->odm, GF_TRUE);
			gf_odm_signal_eos_reached(mo->odm);
			*eos = GF_TRUE;
		}
		gf_filter_pid_drop_packet(mo->odm->pid);
		mo->pck = NULL;
		return NULL;
	}

	pck_ts = (u32) (1000*gf_filter_pck_get_cts(mo->pck) / timescale);

	/*not running and no resync (ie audio)*/
	if (!resync && !gf_clock_is_started(mo->odm->ck)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: CB not running, returning\n", mo->odm->ID));
		return NULL;
	}

	if (resync==GF_MO_FETCH_PAUSED)
		resync=GF_MO_FETCH;

	next_pck = gf_filter_pid_get_packet(mo->odm->pid);
	next_ts = 0;
	if (next_pck) {
		next_ts = (u32) (1000*gf_filter_pck_get_cts(next_pck) / timescale);

		if (gf_filter_pck_get_eos(next_pck)) {
			if (!mo->is_eos) {
				mo->is_eos = GF_TRUE;
				mediasensor_update_timing(mo->odm, GF_TRUE);
				gf_odm_signal_eos_reached(mo->odm);
			}
			gf_filter_pid_drop_packet(mo->odm->pid);
			next_pck=NULL;
		}
	}
	*eos = mo->is_eos;

	/*resync*/
	obj_time = gf_clock_time(mo->odm->ck);

	skip_resync = GF_FALSE;
	//no drop mode, only for speed = 1: all frames are presented, we discard the current output only if already presented and next frame time is mature
	if ((mo->odm->ck->speed == FIX_ONE)
		&& (mo->type==GF_MEDIA_OBJECT_VIDEO)
		&& ! mo->odm->low_latency_mode
	) {
		assert(mo->odm->parentscene);

		if (! mo->odm->parentscene->compositor->drop_late_frames) {
			//if the next AU is at most 1 sec from the current clock use no drop mode
//			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At %d frame TS %u next frame TS %d next data length %d (%d in CB)\n", mo->odm->ID, obj_time, CU->TS, CU->next->TS, CU->next->dataLength, codec->CB->UnitCount));
			if (next_pck && (next_ts + 1000 >= obj_time)) {
				skip_resync = GF_TRUE;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At %u frame TS %u next frame TS %d too late in no-drop mode, enabling drop - resync mode %d\n", mo->odm->ID, obj_time, pck_ts, next_ts, resync));
			}
		}
	}

	if (skip_resync) {
		resync=GF_MO_FETCH; //prevent resync code below
		if (mo->odm->parentscene->compositor->use_step_mode) upload_time_ms=0;
		//we are in no resync mode, drop current frame once played and object time just matured
		//do it only if clock is started or if compositor step mode is set
		//the time threshold for fecthing is given by the caller
		if ( (gf_clock_is_started(mo->odm->ck) || mo->odm->parentscene->compositor->use_step_mode
		)

			&& (mo->timestamp==pck_ts) && next_pck && (next_ts <= obj_time + upload_time_ms) ) {

			//delete our packet
			gf_filter_pck_unref(mo->pck);
			mo->pck = gf_filter_pck_ref(next_pck);
			pck_ts = next_ts;
			//drop next packet from pid
			gf_filter_pid_drop_packet(mo->odm->pid);

			next_pck = gf_filter_pid_get_packet(mo->odm->pid);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Switching to next CU CTS %u now %u\n", mo->odm->ID, next_ts, obj_time));

			mo->RenderedLength = 0;
		}
	}

	next_ts = next_pck ? (u32) (1000*gf_filter_pck_get_cts(next_pck) / timescale) : 0;


	if (resync) {
		u32 nb_dropped = 0;
		while (next_pck) {
			u32 next_data_size;
			if (mo->odm->ck->speed > 0 ? pck_ts >= obj_time : pck_ts <= obj_time )
				break;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Try to drop frame TS %u next frame TS %u obj time %u\n", mo->odm->ID, pck_ts, next_ts, obj_time));

			gf_filter_pck_get_data(next_pck, &next_data_size);
			//nothing ready yet
			if (!next_data_size) {
				//TODO: force decode mode (pull mode of filter graph ?)

				break;
			}

			/*figure out closest time*/
			if (mo->odm->ck->speed > 0 ? next_ts > obj_time : next_ts < obj_time) {
				*eos = GF_FALSE;
				break;
			}

			nb_dropped ++;
			if (nb_dropped>=1) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At OTB %u dropped frame TS %u\n", mo->odm->ID, obj_time, pck_ts));

				mo->odm->nb_dropped++;
			}
			/*discard*/
			gf_filter_pck_unref(mo->pck);
			/*reassign current to next packet*/
			mo->pck = gf_filter_pck_ref(next_pck);
			pck_ts = next_ts;
			//drop next packet from pid
			gf_filter_pid_drop_packet(mo->odm->pid);
			next_pck = gf_filter_pid_get_packet(mo->odm->pid);
			next_ts = next_pck ? (u32) (1000*gf_filter_pck_get_cts(next_pck) / timescale) : 0;
			mo->RenderedLength = 0;
		}
	}

	mo->frame = gf_filter_pck_get_data(mo->pck, &mo->size);
	mo->framesize = mo->size - mo->RenderedLength;
	mo->frame += mo->RenderedLength;
//	mo->media_frame = CU->frame;

	if (next_ts) {
		diff = (s32) (next_ts) - (s32) obj_time;
	} else  {
		diff = 1000*gf_filter_pck_get_duration(mo->pck)  / timescale;
	}
	if (mo->speed)
		mo->ms_until_next = FIX2INT(diff * mo->speed);
	else
		mo->ms_until_next = diff;

	//do't allow too crazy refresh rates
	if (mo->ms_until_next>500)
		mo->ms_until_next=500;

	diff = (mo->speed >= 0) ? (s32) (pck_ts) - (s32) obj_time : (s32) obj_time - (s32) (pck_ts);
	mo->ms_until_pres = FIX2INT(diff * mo->speed);

	if (mo->timestamp != pck_ts) {
		mo->frame_dur = gf_filter_pck_get_duration(mo->pck);

		if (mo->odm->media_current_time <= mo->timestamp)
			mo->odm->media_current_time = mo->timestamp;

		if (mo->odm->parentscene->is_dynamic_scene) {
			GF_Scene *s = mo->odm->parentscene;
			while (s && s->root_od->addon) {
				s = s->root_od->parentscene;
			}
			if (s && (s->root_od->media_current_time < mo->odm->media_current_time) )
				s->root_od->media_current_time = mo->odm->media_current_time;
		}

#ifndef GPAC_DISABLE_VRML
		if (! *eos )
			mediasensor_update_timing(mo->odm, GF_FALSE);
#endif

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d (%s)] At OTB %u fetch frame TS %u size %d (previous TS %u) - %d unit in CB - UTC "LLU" ms - %d ms until CTS is due - %d ms until next frame\n", mo->odm->ID, mo->odm->scene_ns->url, gf_clock_time(mo->odm->ck), pck_ts, mo->framesize, mo->timestamp, gf_filter_pid_get_packet_count(mo->odm->pid), gf_net_get_utc(), mo->ms_until_pres, mo->ms_until_next ));

		v = gf_filter_pck_get_property(mo->pck, GF_PROP_PCK_SENDER_NTP);
		if (v) {
			s32 ntp_diff = gf_net_get_ntp_diff_ms(v->value.longuint);
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d (%s)] Frame TS %u NTP diff with sender %d ms\n", mo->odm->ID, mo->odm->scene_ns->url, pck_ts, ntp_diff));
		}

		mo->timestamp = pck_ts;
		/*signal EOS after rendering last frame, not while rendering it*/
		*eos = GF_FALSE;

	} else if (*eos) {
		//already rendered the last frame, consider we no longer have pending late frame on this stream
		mo->ms_until_pres = 0;
	} else {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d (%s)] At OTB %u same frame fetch TS %u\n", mo->odm->ID, mo->odm->net_service->url, obj_time, CU->TS ));
	}

	/*also adjust CU time based on consummed bytes in input, since some codecs output very large audio chunks*/
	if (mo->bytes_per_sec) mo->timestamp += mo->RenderedLength * 1000 / mo->bytes_per_sec;

	if (mo->odm->parentscene->compositor->bench_mode) {
		mo->ms_until_pres = -1;
		mo->ms_until_next = 1;
	}


	mo->nb_fetch ++;
	assert(mo->frame);
	*timestamp = mo->timestamp;
	*size = mo->framesize;
	if (ms_until_pres) *ms_until_pres = mo->ms_until_pres;
	if (ms_until_next) *ms_until_next = mo->ms_until_next;
	if (outFrame) *outFrame = mo->media_frame;

	gf_odm_service_media_event(mo->odm, GF_EVENT_MEDIA_TIME_UPDATE);

	if (mo->media_frame)
		return (char *) mo->media_frame;

	return mo->frame;
}

GF_EXPORT
GF_Err gf_mo_get_raw_image_planes(GF_MediaObject *mo, u8 **pY_or_RGB, u8 **pU, u8 **pV, u32 *stride_luma_rgb, u32 *stride_chroma)
{
	u32 stride;
	if (!mo || !mo->odm || !mo->odm) return GF_BAD_PARAM;
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("NOT YET IMPLEMENTED IN FILTERS\n"));
	return GF_NOT_SUPPORTED;

#if 0
	if (mo->odm->codec->direct_vout) {
		*pY_or_RGB = mo->odm->codec->CB->pY;
		*pU = mo->odm->codec->CB->pU;
		*pV = mo->odm->codec->CB->pV;
		return GF_OK;
	}
	if (!mo->odm->codec->direct_frame_output || !mo->media_frame)
		return GF_BAD_PARAM;

	mo->media_frame->GetPlane(mo->media_frame, 0, (const char **) pY_or_RGB, stride_luma_rgb);
	mo->media_frame->GetPlane(mo->media_frame, 1, (const char **) pU, &stride);
	mo->media_frame->GetPlane(mo->media_frame, 2, (const char **) pV, &stride);
	*stride_chroma = stride;
	return GF_OK;
#endif
}

GF_EXPORT
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, s32 drop_mode)
{
	GF_FilterPacket *pck;
	if (!mo || !mo->odm || !mo->odm->pid || !mo->nb_fetch) return;

	mo->nb_fetch--;
	if (mo->nb_fetch) {
		return;
	}

	if (nb_bytes==0xFFFFFFFF) {
		mo->RenderedLength = mo->size;
	} else {
		assert(mo->RenderedLength + nb_bytes <= mo->size);
		mo->RenderedLength += nb_bytes;
	}

	if (drop_mode<0) {
		/*only allow for explicit last frame keeping if only one node is using the resource
			otherwise this would block the composition memory*/
		if (mo->num_open>1) {
			drop_mode=0;
		} else {
			return;
		}
	}

	/*discard frame*/
	if (mo->RenderedLength >= mo->size) {
		mo->RenderedLength = 0;
		if (/*mo->is_eos ||*/ drop_mode) {
			gf_filter_pck_unref(mo->pck);
			mo->pck=NULL;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At OTB %u released frame TS %u\n", mo->odm->ID,gf_clock_time(mo->odm->ck), mo->timestamp));
		} else {
			/*we cannot drop since we don't know the speed of the playback (which can even be frame by frame)*/
		}
	}
}

GF_EXPORT
void gf_mo_get_object_time(GF_MediaObject *mo, u32 *obj_time)
{
	/*get absolute clock (without drift) for audio*/
	if (mo->odm->ck) {
		if (mo->odm->type==GF_STREAM_AUDIO)
			*obj_time = gf_clock_real_time(mo->odm->ck);
		else
			*obj_time = gf_clock_time(mo->odm->ck);
	}
	/*unknown / unsupported object*/
	else {
		*obj_time = 0;
	}
}

GF_EXPORT
s32 gf_mo_get_clock_drift(GF_MediaObject *mo)
{
	s32 res = 0;

	/*regular media codec...*/
	if (mo->odm->ck) {
		res = mo->odm->ck->drift;
	}
	return res;
}

GF_EXPORT
void gf_mo_play(GF_MediaObject *mo, Double clipBegin, Double clipEnd, Bool can_loop)
{
	if (!mo) return;

	if (!mo->num_open && mo->odm) {
		Bool is_restart = GF_FALSE;

		if (mo->odm->state == GF_ODM_STATE_PLAY) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("NOT YET IMPLEMENTED IN FILTERS\n"));
//			is_restart = GF_TRUE;
		}

		if (mo->odm->flags & GF_ODM_NO_TIME_CTRL) {
			mo->odm->media_start_time = 0;
		} else {
			mo->odm->media_start_time = (u64) (clipBegin*1000);
			if (mo->odm->duration && (mo->odm->media_start_time > mo->odm->duration)) {
				if (can_loop) {
					mo->odm->media_start_time %= mo->odm->duration;
				} else {
					mo->odm->media_start_time = mo->odm->duration;
				}
			}
			if (clipEnd>=clipBegin) {
				mo->odm->media_stop_time = (u64) (clipEnd*1000);
				if (mo->odm->duration && (mo->odm->media_stop_time >=0) && ((u64) mo->odm->media_stop_time > mo->odm->duration)) {
					mo->odm->media_stop_time = 0;
				}
			} else {
				mo->odm->media_stop_time = 0;
			}
		}
		/*done prefetching*/
		mo->odm->flags &= ~GF_ODM_PREFETCH;

		if (is_restart) {
			mediacontrol_restart(mo->odm);
		} else {
			gf_odm_start(mo->odm);
		}
	} else if (mo->odm) {
		if (mo->num_to_restart) mo->num_restart--;
		if (!mo->num_restart && (mo->num_to_restart==mo->num_open+1) ) {
			mediacontrol_restart(mo->odm);
			mo->num_to_restart = mo->num_restart = 0;
		}
	}
	mo->num_open++;
}

GF_EXPORT
Bool gf_mo_stop(GF_MediaObject *mo)
{
	Bool ret = GF_FALSE;
	if (!mo || !mo->num_open) return GF_FALSE;

	mo->num_open--;
	if (!mo->num_open && mo->odm) {
		if (mo->odm->flags & GF_ODM_DESTROYED) return GF_TRUE;

		/*signal STOP request*/
		if ((mo->OD_ID==GF_MEDIA_EXTERNAL_ID) || (mo->odm && mo->odm->ID && (mo->odm->ID==GF_MEDIA_EXTERNAL_ID))) {
			gf_odm_disconnect(mo->odm, 2);
			ret = GF_TRUE;
		} else {
			gf_odm_stop_or_destroy(mo->odm);
		}
	} else {
		if (!mo->num_to_restart) {
			mo->num_restart = mo->num_to_restart = mo->num_open + 1;
		}
	}
	return ret;
}

GF_EXPORT
void gf_mo_restart(GF_MediaObject *mo)
{
	void *mediactrl_stack = NULL;

#ifndef GPAC_DISABLE_VRML
	mediactrl_stack = gf_odm_get_mediacontrol(mo->odm);
#endif
	/*if no control and not root of a scene, check timelines are unlocked*/
	if (!mediactrl_stack && !mo->odm->subscene) {
		/*don't restart if sharing parent scene clock*/
		if (gf_odm_shares_clock(mo->odm, gf_odm_get_media_clock(mo->odm->parentscene->root_od))) {
			return;
		}
	}
	/*all other cases, call restart to take into account clock references*/
	mediacontrol_restart(mo->odm);
}

u32 gf_mo_get_od_id(MFURL *url)
{
	u32 i, j, tmpid;
	char *str, *s_url;
	u32 id = 0;

	if (!url) return 0;

	for (i=0; i<url->count; i++) {
		if (url->vals[i].OD_ID) {
			/*works because OD ID 0 is forbidden in MPEG4*/
			if (!id) {
				id = url->vals[i].OD_ID;
			}
			/*bad url, only one object can be described in MPEG4 urls*/
			else if (id != url->vals[i].OD_ID) return 0;
		} else if (url->vals[i].url && strlen(url->vals[i].url)) {
			/*format: od:ID or od:ID#segment - also check for "ID" in case...*/
			str = url->vals[i].url;
			if (!strnicmp(str, "od:", 3)) str += 3;
			/*remove segment info*/
			s_url = gf_strdup(str);
			j = 0;
			while (j<strlen(s_url)) {
				if (s_url[j]=='#') {
					s_url[j] = 0;
					break;
				}
				j++;
			}
			j = sscanf(s_url, "%u", &tmpid);
			/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
			if (j==1) {
				char szURL[20];
				sprintf(szURL, "%u", tmpid);
				if (stricmp(szURL, s_url)) j = 0;
			}
			gf_free(s_url);

			if (j!= 1) {
				/*dynamic OD if only one URL specified*/
				if (!i) return GF_MEDIA_EXTERNAL_ID;
				/*otherwise ignore*/
				continue;
			}
			if (!id) {
				id = tmpid;
				continue;
			}
			/*bad url, only one object can be described in MPEG4 urls*/
			else if (id != tmpid) return 0;
		}
	}
	return id;
}


Bool gf_mo_is_same_url(GF_MediaObject *obj, MFURL *an_url, Bool *keep_fragment, u32 obj_hint_type)
{
	Bool include_sub_url = GF_FALSE;
	u32 i;
	char szURL1[GF_MAX_PATH], szURL2[GF_MAX_PATH], *ext;

	if (!obj->URLs.count) {
		if (!obj->odm) return GF_FALSE;
		strcpy(szURL1, obj->odm->scene_ns->url);
	} else {
		strcpy(szURL1, obj->URLs.vals[0].url);
	}

	/*don't analyse audio/video to locate segments or viewports*/
	if ((obj->type==GF_MEDIA_OBJECT_AUDIO) || (obj->type==GF_MEDIA_OBJECT_VIDEO)) {
		if (keep_fragment) *keep_fragment = GF_FALSE;
		include_sub_url = GF_TRUE;
	} else if ((obj->type==GF_MEDIA_OBJECT_SCENE) && keep_fragment && obj->odm) {
		u32 j;
		/*for remoteODs/dynamic ODs, check if one of the running service cannot be used*/
		for (i=0; i<an_url->count; i++) {
			GF_Scene *scene;
			GF_SceneNamespace *sns;
			char *frag = strrchr(an_url->vals[i].url, '#');
			j=0;
			/*this is the same object (may need some refinement)*/
			if (!stricmp(szURL1, an_url->vals[i].url)) return GF_TRUE;

			/*fragment is a media segment, same URL*/
			if (frag ) {
				Bool same_res = GF_FALSE;
				frag[0] = 0;
				same_res = !strncmp(an_url->vals[i].url, szURL1, strlen(an_url->vals[i].url)) ? GF_TRUE : GF_FALSE;
				frag[0] = '#';

				/*if we're talking about the same resource, check if the fragment can be matched*/
				if (same_res) {
					/*if the fragment is a node which can be found, this is the same resource*/
					if (obj->odm->subscene && (gf_sg_find_node_by_name(obj->odm->subscene->graph, frag+1)!=NULL) )
						return GF_TRUE;

					/*if the expected type is an existing segment (undefined media type), this is the same resource*/
					if (!obj_hint_type && gf_odm_find_segment(obj->odm, frag+1))
						return GF_TRUE;
				}
			}

			scene = gf_scene_get_root_scene(obj->odm->parentscene ? obj->odm->parentscene : obj->odm->subscene);
			while ( (sns = (GF_SceneNamespace*) gf_list_enum(scene->namespaces, &j) ) ) {
				/*sub-service of an existing service - don't touch any fragment*/
#ifdef FILTER_FIXME
				if (gf_term_service_can_handle_url(sns, an_url->vals[i].url)) {
					*keep_fragment = GF_TRUE;
					return GF_FALSE;
				}
#endif
			}
		}
	}

	/*check on full URL without removing fragment IDs*/
	if (include_sub_url) {
		for (i=0; i<an_url->count; i++) {
			if (an_url->vals[i].url && !stricmp(szURL1, an_url->vals[i].url)) return GF_TRUE;
		}
		/*not same resource, we will have to check fragment as URL might point to a sub-service or single stream of a mux*/
		if (keep_fragment) *keep_fragment = GF_TRUE;
		return GF_FALSE;
	}
	ext = strrchr(szURL1, '#');
	if (ext) ext[0] = 0;
	for (i=0; i<an_url->count; i++) {
		if (!an_url->vals[i].url) return GF_FALSE;
		strcpy(szURL2, an_url->vals[i].url);
		ext = strrchr(szURL2, '#');
		if (ext) ext[0] = 0;
		if (!stricmp(szURL1, szURL2)) return GF_TRUE;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_mo_url_changed(GF_MediaObject *mo, MFURL *url)
{
	u32 od_id;
	Bool ret = GF_FALSE;
	if (!mo) return (url ? GF_TRUE : GF_FALSE);
	od_id = gf_mo_get_od_id(url);
	if ( (mo->OD_ID == GF_MEDIA_EXTERNAL_ID) && (od_id == GF_MEDIA_EXTERNAL_ID)) {
		ret = !gf_mo_is_same_url(mo, url, NULL, 0);
	} else {
		ret = (mo->OD_ID == od_id) ? GF_FALSE : GF_TRUE;
	}
	/*special case for 3GPP text: if not playing and user node changed, force removing it*/
	if (ret && mo->odm && !mo->num_open && (mo->type == GF_MEDIA_OBJECT_TEXT)) {
		mo->flags |= GF_MO_DISPLAY_REMOVE;
	}
	return ret;
}

GF_EXPORT
void gf_mo_pause(GF_MediaObject *mo)
{
#ifndef GPAC_DISABLE_VRML
	if (!mo || !mo->num_open || !mo->odm) return;
	mediacontrol_pause(mo->odm);
#endif
}

GF_EXPORT
void gf_mo_resume(GF_MediaObject *mo)
{
#ifndef GPAC_DISABLE_VRML
	if (!mo || !mo->num_open || !mo->odm) return;
	mediacontrol_resume(mo->odm, 0);
#endif
}

GF_EXPORT
void gf_mo_set_speed(GF_MediaObject *mo, Fixed speed)
{
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif

	if (!mo) return;
	if (!mo->odm) {
		mo->speed = speed;
		return;
	}
	//override startup speed if asked to
	if (mo->odm->set_speed) {
		speed = mo->odm->set_speed;
		mo->odm->set_speed = 0;
	}
#ifndef GPAC_DISABLE_VRML
	/*if media control forbidd that*/
	ctrl = gf_odm_get_mediacontrol(mo->odm);
	if (ctrl) return;
#endif

	if (mo->odm->scene_ns && mo->odm->scene_ns->owner && (mo->odm->scene_ns->owner->flags & GF_ODM_INHERIT_TIMELINE))
		return;

	gf_odm_set_speed(mo->odm, speed, GF_TRUE);
}

GF_EXPORT
Fixed gf_mo_get_current_speed(GF_MediaObject *mo)
{
	return (mo && mo->odm && mo->odm->ck) ? mo->odm->ck->speed : FIX_ONE;
}

GF_EXPORT
u32 gf_mo_get_min_frame_dur(GF_MediaObject *mo)
{
	return mo ? mo->frame_dur : 0;

}


GF_EXPORT
Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed)
{
	Fixed res = in_speed;
	if (!mo || !mo->odm) return in_speed;

#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;

	/*get control*/
	ctrl = gf_odm_get_mediacontrol(mo->odm);
	if (ctrl) res = ctrl->control->mediaSpeed;

#endif

	return res;
}

GF_EXPORT
Bool gf_mo_get_loop(GF_MediaObject *mo, Bool in_loop)
{
	GF_Clock *ck;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif
	if (!mo) return in_loop;

	/*get control*/
#ifndef GPAC_DISABLE_VRML
	ctrl = gf_odm_get_mediacontrol(mo->odm);
	if (ctrl) in_loop = ctrl->control->loop;
#endif

	/*otherwise looping is only accepted if not sharing parent scene clock*/
	ck = gf_odm_get_media_clock(mo->odm->parentscene->root_od);
	if (gf_odm_shares_clock(mo->odm, ck)) {
		in_loop = GF_FALSE;
#ifndef GPAC_DISABLE_VRML
		/*
			if (ctrl && ctrl->stream->odm && ctrl->stream->odm->subscene)
					gf_term_invalidate_compositor(mo->odm->term);
		*/
#endif
	}
	return in_loop;
}

GF_EXPORT
Double gf_mo_get_duration(GF_MediaObject *mo)
{
	Double dur;
	dur = ((Double) (s64)mo->odm->duration)/1000.0;
	return dur;
}

GF_EXPORT
Bool gf_mo_should_deactivate(GF_MediaObject *mo)
{
	Bool res = GF_FALSE;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif

	if (!mo->odm->state || (mo->odm->parentscene && mo->odm->parentscene->is_dynamic_scene)) {
		return GF_FALSE;
	}

#ifndef GPAC_DISABLE_VRML
	/*get media control and see if object owning control is running*/
	ctrl = gf_odm_get_mediacontrol(mo->odm);
	if (!ctrl) res = GF_TRUE;
	/*if ctrl and ctrl not ruling this mediaObject, deny deactivation*/
	else if (ctrl->stream->odm != mo->odm) res = GF_FALSE;
	/*this is currently under discussion in MPEG. for now we deny deactivation as soon as a mediaControl is here*/
	else if (ctrl->stream->odm->state) res = GF_FALSE;
	/*otherwise allow*/
	else
#endif
		res = GF_TRUE;

	return res;
}

GF_EXPORT
Bool gf_mo_is_muted(GF_MediaObject *mo)
{
	Bool res = GF_FALSE;
#ifndef GPAC_DISABLE_VRML
	res = mo->odm->media_ctrl ? mo->odm->media_ctrl->control->mute : GF_FALSE;
#endif
	return res;
}

GF_EXPORT
Bool gf_mo_is_done(GF_MediaObject *mo)
{
	GF_Clock *ck;
	u64 dur;
	if (!mo || !mo->odm) return GF_FALSE;

	if (! mo->odm->has_seen_eos) return GF_FALSE;

	if ((mo->odm->type==GF_STREAM_AUDIO) || (mo->odm->type==GF_STREAM_VISUAL)) {
		return GF_TRUE;
	}

	/*check time - technically this should also apply to video streams since we could extend the duration
	of the last frame - to further test*/
	dur = (mo->odm->subscene && mo->odm->subscene->duration) ? mo->odm->subscene->duration : mo->odm->duration;
	/*codec is done, check by duration*/
	ck = gf_odm_get_media_clock(mo->odm);
	if (gf_clock_time(ck) > dur)
		return GF_TRUE;

	return GF_FALSE;
}

/*resyncs clock - only audio objects are allowed to use this*/
GF_EXPORT
void gf_mo_adjust_clock(GF_MediaObject *mo, s32 ms_drift)
{
	if (!mo || !mo->odm) return;
	if (mo->odm->type != GF_STREAM_AUDIO) return;
	gf_clock_adjust_drift(mo->odm->ck, ms_drift);
}

GF_EXPORT
u32 gf_mo_get_flags(GF_MediaObject *mo)
{
	return mo ? mo->flags : 0;
}

GF_EXPORT
void gf_mo_set_flag(GF_MediaObject *mo, u32 flag, Bool set_on)
{
	if (mo) {
		if (set_on)
			mo->flags |= flag;
		else
			mo->flags &= ~flag;
	}
}

GF_EXPORT
u32 gf_mo_get_last_frame_time(GF_MediaObject *mo)
{
	return mo ? mo->timestamp : 0;
}

GF_EXPORT
u32 gf_mo_has_audio(GF_MediaObject *mo)
{
	char *sub_url, *ext;
	u32 i;
	GF_SceneNamespace *ns;
	GF_Scene *scene;
	if (!mo || !mo->odm) return 0;
	if (mo->type != GF_MEDIA_OBJECT_VIDEO) return 0;
	if (!mo->odm->scene_ns) return 2;

	ns = mo->odm->scene_ns;
	scene = mo->odm->parentscene;
	sub_url = strchr(ns->url, '#');
	for (i=0; i<gf_list_count(scene->resources); i++) {
		GF_ObjectManager *odm = (GF_ObjectManager *)gf_list_get(scene->resources, i);
		if (odm->scene_ns != ns) continue;
		//object already associated
		if (odm->mo) continue;

		if (sub_url) {
			ext = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
			if (ext) ext = strchr(ext, '#');
			if (!ext || strcmp(sub_url, ext)) continue;
		}
		/*we have one audio object not bound with the scene from the same service, let's use it*/
		if (odm->mo->type == GF_MEDIA_OBJECT_AUDIO) return 1;
	}
	return 0;
}

GF_EXPORT
GF_SceneGraph *gf_mo_get_scenegraph(GF_MediaObject *mo)
{
	if (!mo || !mo->odm || !mo->odm->subscene) return NULL;
	return mo->odm->subscene->graph;
}


GF_EXPORT
GF_DOMEventTarget *gf_mo_event_target_add_node(GF_MediaObject *mo, GF_Node *n)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOMEventTarget *target = NULL;
	if (!mo ||!n) return NULL;
	target = gf_dom_event_get_target_from_node(n);
	gf_list_add(mo->evt_targets, target);
	return target;
#else
	return NULL;
#endif
}

GF_Err gf_mo_event_target_remove(GF_MediaObject *mo, GF_DOMEventTarget *target)
{
	if (!mo || !target) return GF_BAD_PARAM;
	gf_list_del_item(mo->evt_targets, target);
	return GF_OK;
}

GF_Err gf_mo_event_target_remove_by_index(GF_MediaObject *mo, u32 i)
{
	if (!mo) return GF_BAD_PARAM;
	gf_list_rem(mo->evt_targets, i);
	return GF_OK;
}

GF_Node *gf_mo_event_target_enum_node(GF_MediaObject *mo, u32 *i)
{
	GF_DOMEventTarget *target;
	if (!mo || !i) return NULL;
	target = (GF_DOMEventTarget *)gf_list_enum(mo->evt_targets, i);
	if (!target) return NULL;
	//if (target->ptr_type != GF_DOM_EVENT_TARGET_NODE) return NULL;
	return (GF_Node *)target->ptr;
}

s32 gf_mo_event_target_find_by_node(GF_MediaObject *mo, GF_Node *node)
{
	u32 i, count;
	count = gf_list_count(mo->evt_targets);
	for (i = 0; i < count; i++) {
		GF_DOMEventTarget *target = (GF_DOMEventTarget *)gf_list_get(mo->evt_targets, i);
		if (target->ptr == node) {
			return i;
		}
	}
	return -1;
}

GF_EXPORT
GF_Err gf_mo_event_target_remove_by_node(GF_MediaObject *mo, GF_Node *node)
{
	u32 i, count;
	count = gf_list_count(mo->evt_targets);
	for (i = 0; i < count; i++) {
		GF_DOMEventTarget *target = (GF_DOMEventTarget *)gf_list_get(mo->evt_targets, i);
		if (target->ptr == node) {
			gf_list_del_item(mo->evt_targets, target);
			i--;
			count--;
			//return GF_OK;
		}
	}
	return GF_BAD_PARAM;
}

GF_EXPORT
GF_Node *gf_event_target_get_node(GF_DOMEventTarget *target)
{
	if (target && (target->ptr_type == GF_DOM_EVENT_TARGET_NODE)) {
		return (GF_Node *)target->ptr;
	}
	return NULL;
}

GF_EXPORT
GF_DOMEventTarget *gf_mo_event_target_get(GF_MediaObject *mo, u32 i)
{
	GF_DOMEventTarget *target = (GF_DOMEventTarget *)gf_list_get(mo->evt_targets, i);
	return target;
}

void gf_mo_event_target_reset(GF_MediaObject *mo)
{
	if (mo->evt_targets) gf_list_reset(mo->evt_targets);
}

u32 gf_mo_event_target_count(GF_MediaObject *mo)
{
	if (!mo) return 0;
	return gf_list_count(mo->evt_targets);
}

void gf_mo_del(GF_MediaObject *mo)
{
	assert(gf_list_count(mo->evt_targets) == 0);
	gf_list_del(mo->evt_targets);
	if (mo->pck) gf_filter_pck_unref(mo->pck);

	gf_sg_mfurl_del(mo->URLs);
	gf_free(mo);
}


Bool gf_mo_get_srd_info(GF_MediaObject *mo, GF_MediaObjectVRInfo *vr_info)
{
	GF_Scene *scene;
	if (!vr_info) return GF_FALSE;

	scene = mo->odm->subscene ? mo->odm->subscene : mo->odm->parentscene;
	memset(vr_info, 0, sizeof(GF_MediaObjectVRInfo));

	vr_info->srd_x = mo->srd_x;
	vr_info->srd_y = mo->srd_y;
	vr_info->srd_w = mo->srd_w;
	vr_info->srd_h = mo->srd_h;
	vr_info->srd_min_x = scene->srd_min_x;
	vr_info->srd_min_y = scene->srd_min_y;
	vr_info->srd_max_x = scene->srd_max_x;
	vr_info->srd_max_y = scene->srd_max_y;
	vr_info->is_tiled_srd = scene->is_tiled_srd;
	
	gf_sg_get_scene_size_info(scene->graph, &vr_info->scene_width, &vr_info->scene_height);

	return (!scene->vr_type && !scene->is_srd) ? GF_FALSE : GF_TRUE;
}

/*sets quality hint for this media object  - quality_rank is between 0 (min quality) and 100 (max quality)*/
void gf_mo_hint_quality_degradation(GF_MediaObject *mo, u32 quality_degradation)
{
	if (!mo->odm || !mo->odm || !mo->odm->pid) {
		return;
	}
	if (mo->quality_degradation_hint != quality_degradation) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_QUALITY_SWITCH, mo->odm->pid);
		evt.quality_switch.quality_degradation = quality_degradation;
		gf_filter_pid_send_event(mo->odm->pid, &evt);

		mo->quality_degradation_hint = quality_degradation;
	}
}

void gf_mo_hint_visible_rect(GF_MediaObject *mo, u32 min_x, u32 max_x, u32 min_y, u32 max_y)
{
	if (!mo->odm || !mo->odm || !mo->odm->pid) {
		return;
	}

	if ((mo->view_min_x!=min_x) || (mo->view_max_x!=max_x) || (mo->view_min_y!=min_y) || (mo->view_max_y!=max_y)) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_QUALITY_SWITCH, mo->odm->pid);
		mo->view_min_x = min_x;
		mo->view_max_x = max_x;
		mo->view_min_y = min_y;
		mo->view_max_y = max_y;
		
		evt.visibility_hint.min_x = min_x;
		evt.visibility_hint.max_x = max_x;
		evt.visibility_hint.min_y = min_y;
		evt.visibility_hint.max_y = max_y;

		gf_filter_pid_send_event(mo->odm->pid, &evt);
	}
}



