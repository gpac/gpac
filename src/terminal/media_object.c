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

GF_EXPORT
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
Bool gf_mo_get_audio_info(GF_MediaObject *mo, u32 *sample_rate, u32 *bits_per_sample, u32 *num_channels, u32 *channel_config, Bool *forced_layout)
{
	if (!mo->odm || !mo->odm->codec || (mo->type != GF_MEDIA_OBJECT_AUDIO)) return GF_FALSE;

	if (sample_rate) *sample_rate = mo->sample_rate;
	if (bits_per_sample) *bits_per_sample = mo->bits_per_sample;
	if (num_channels) *num_channels = mo->num_channels;
	if (channel_config) *channel_config = mo->channel_config;
	if (forced_layout) *forced_layout = GF_FALSE;

	if (mo->odm->ambi_ch_id) {
		if (mo->num_channels>1) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d]: tagged as ambisonic channel %d but has %d channels, ignoring ambisonic tag\n",  mo->odm->OD->objectDescriptorID, mo->odm->ambi_ch_id, mo->num_channels ));
		} else {
			if (num_channels) *num_channels = 1;
			if (channel_config) *channel_config = 1 << (mo->odm->ambi_ch_id - 1);
			if (forced_layout) *forced_layout = GF_TRUE;

		}
	}

	return GF_TRUE;
}

static void gf_mo_update_visual_info(GF_MediaObject *mo)
{
	GF_Channel *ch;
	GF_NetworkCommand com;

	GF_CodecCapability cap;
	if ((mo->type != GF_MEDIA_OBJECT_VIDEO) && (mo->type!=GF_MEDIA_OBJECT_TEXT)) return;

	cap.CapCode = GF_CODEC_WIDTH;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->width = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_HEIGHT;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->height = cap.cap.valueInt;

	if (mo->type==GF_MEDIA_OBJECT_TEXT) return;

	cap.CapCode = GF_CODEC_FLIP;
	cap.cap.valueInt = 0;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->is_flipped = cap.cap.valueInt ? GF_TRUE : GF_FALSE;

	cap.CapCode = GF_CODEC_STRIDE;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->stride = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_PIXEL_FORMAT;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->pixelformat = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_FPS;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->odm->codec->fps = cap.cap.valueFloat;

	cap.CapCode = GF_CODEC_NBVIEWS;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->nb_views = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_NBLAYERS;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->nb_layers = cap.cap.valueInt;

	if (mo->odm && mo->odm->parentscene->is_dynamic_scene) {
#ifndef GPAC_DISABLE_VRML
		const char *name = gf_node_get_name(gf_event_target_get_node(gf_mo_event_target_get(mo, 0)));
		if (name && !strcmp(name, "DYN_VIDEO1")) {
			const char *opt;
			u32 r, g, b, a;
			M_Background2D *back = (M_Background2D *) gf_sg_find_node_by_name(mo->odm->parentscene->graph, "DYN_BACK");
			if (back) {
				switch (cap.cap.valueInt) {
				case GF_PIXEL_ARGB:
				case GF_PIXEL_RGBA:
				case GF_PIXEL_YUVA:
					opt = gf_cfg_get_key(mo->odm->term->user->config, "Compositor", "BackColor");
					if (!opt) {
						gf_cfg_set_key(mo->odm->term->user->config, "Compositor", "BackColor", "FF999999");
						opt = "FF999999";
					}
					sscanf(opt, "%02X%02X%02X%02X", &a, &r, &g, &b);
					back->backColor.red = INT2FIX(r)/255;
					back->backColor.green = INT2FIX(g)/255;
					back->backColor.blue = INT2FIX(b)/255;
					break;
				default:
					back->backColor.red = back->backColor.green = back->backColor.blue = 0;
					break;
				}
				gf_node_dirty_set((GF_Node *)back, 0, GF_TRUE);
			}
		}
#endif
	}
	/*get PAR settings*/
	cap.CapCode = GF_CODEC_PAR;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->pixel_ar = cap.cap.valueInt;
	if (! (mo->pixel_ar & 0x0000FFFF)) mo->pixel_ar = 0;
	if (! (mo->pixel_ar & 0xFFFF0000)) mo->pixel_ar = 0;

	memset(&com, 0, sizeof(GF_NetworkCommand));
	ch = (GF_Channel *)gf_list_get(mo->odm->channels, 0);
	if (!ch) return;
	com.base.on_channel = ch;

	/**/
	if (! mo->pixel_ar) {
		com.base.command_type = GF_NET_CHAN_GET_PIXEL_AR;
		if (gf_term_service_command(ch->service, &com) == GF_OK) {
			if ((com.par.hSpacing>65535) || (com.par.vSpacing>65535)) {
				com.par.hSpacing>>=16;
				com.par.vSpacing>>=16;
			}
			if (com.par.hSpacing|| com.par.vSpacing)
				mo->pixel_ar = (com.par.hSpacing<<16) | com.par.vSpacing;
		}
	}

	com.base.command_type = GF_NET_CHAN_GET_SRD;
	if ((gf_term_service_command(ch->service, &com) == GF_OK)) {
		//regular SRD object
		if (com.srd.w && com.srd.h) {
			mo->srd_x = com.srd.x;
			mo->srd_y = com.srd.y;
			mo->srd_w = com.srd.w;
			mo->srd_h = com.srd.h;
			mo->srd_full_w = com.srd.width;
			mo->srd_full_h = com.srd.height;

			if (mo->odm->parentscene->is_dynamic_scene) {
				if ((mo->srd_w == mo->srd_full_w) && (mo->srd_h == mo->srd_full_h)) {
					mo->odm->parentscene->srd_type = 2;
				} else if (!mo->odm->parentscene->srd_type) {
					mo->odm->parentscene->srd_type = 1;
				}
			}
		}
		// SRD object with no size but global scene size: HEVC tiled bas object
		else if (com.srd.width && com.srd.height) {
			if (mo->odm->parentscene->is_dynamic_scene && !mo->odm->parentscene->srd_type) {
				mo->odm->parentscene->is_tiled_srd = GF_TRUE;
			}
		}
	}
}

static void gf_mo_update_audio_info(GF_MediaObject *mo)
{
	GF_CodecCapability cap;
	if (!mo->odm || !mo->odm->codec || (mo->type != GF_MEDIA_OBJECT_AUDIO)) return;

	if (mo->odm->term->bench_mode==2) {
		mo->sample_rate = 44100;
		mo->bits_per_sample = 16;
		mo->num_channels = 2;
		mo->channel_config = 0;
		return;
	}
	cap.CapCode = GF_CODEC_SAMPLERATE;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->sample_rate = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_NB_CHAN;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->num_channels = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_BITS_PER_SAMPLE;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->bits_per_sample = cap.cap.valueInt;

	cap.CapCode = GF_CODEC_CHANNEL_CONFIG;
	gf_codec_get_capability(mo->odm->codec, &cap);
	mo->channel_config = cap.cap.valueInt;

	mo->odm->codec->bytes_per_sec = mo->sample_rate * mo->num_channels * mo->bits_per_sample / 8;
}

void gf_mo_update_caps(GF_MediaObject *mo)
{
	mo->flags &= ~GF_MO_IS_INIT;

	if (mo->type == GF_MEDIA_OBJECT_VIDEO) {
		gf_mo_update_visual_info(mo);
	} else if (mo->type == GF_MEDIA_OBJECT_AUDIO) {
		gf_mo_update_audio_info(mo);
	}
}

static Bool gf_odm_check_cb_resize(GF_MediaObject *mo, GF_Codec *codec)
{
	/*resize requested - if last frame destroy CB and force a decode*/
	if (codec->force_cb_resize && (codec->CB->UnitCount<=1)) {
		GF_CMUnit *CU = gf_cm_get_output(codec->CB);
		if (!CU || (CU->TS==mo->timestamp)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[%s] ODM%d: Resizing output buffer %d -> %d\n", codec->decio->module_name, codec->odm->OD->objectDescriptorID, codec->CB->UnitSize, codec->force_cb_resize));
			gf_codec_resize_composition_buffer(codec, codec->force_cb_resize);
			codec->force_cb_resize=0;
			//decode and resize CB
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

GF_EXPORT
char *gf_mo_fetch_data(GF_MediaObject *mo, GF_MOFetchMode resync, u32 upload_time_ms, Bool *eos, u32 *timestamp, u32 *size, s32 *ms_until_pres, s32 *ms_until_next, GF_MediaDecoderFrame **outFrame)
{
	GF_Codec *codec;
	u32 force_decode_mode = 0;
	u32 obj_time;
	GF_CMUnit *CU;
	s32 diff;
	Bool bench_mode, skip_resync;

	*eos = GF_FALSE;
	*timestamp = mo->timestamp;
	*size = mo->framesize;
	if (ms_until_pres) *ms_until_pres = mo->ms_until_pres;
	if (ms_until_next) *ms_until_next = mo->ms_until_next;
	if (outFrame) *outFrame = NULL;

	if (!gf_odm_lock_mo(mo)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: Failed to lock MO, returning\n", mo->odm ? mo->odm->OD->objectDescriptorID : 0));
		return NULL;
	}

	if (!mo->odm->codec || !mo->odm->codec->CB) {
		gf_odm_lock(mo->odm, 0);
		return NULL;
	}

	/*if frame locked return it*/
	if (mo->nb_fetch) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: CU already fetched, returning\n", mo->odm->OD->objectDescriptorID));
		mo->nb_fetch ++;
		gf_odm_lock(mo->odm, 0);
		return mo->frame;
	}
	codec = mo->odm->codec;

	/*end of stream */
	*eos = gf_cm_is_eos(codec->CB);

	/*not running and no resync (ie audio)*/
	if (!resync && !gf_cm_is_running(codec->CB)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: CB not running, returning\n", mo->odm->OD->objectDescriptorID));
		gf_odm_lock(mo->odm, 0);
		return NULL;
	}
	if (resync==GF_MO_FETCH_PAUSED)
		resync=GF_MO_FETCH;

	bench_mode = mo->odm->term->bench_mode;

	/*resize requested - if last frame destroy CB and force a decode*/
	if (gf_odm_check_cb_resize(mo, codec)) {
		force_decode_mode = 1;
	}

	/*fast forward, bench mode with composition memory: force one decode if no data is available*/
	if (! *eos && ((codec->ck->speed > FIX_ONE) || (codec->odm->term->bench_mode && !codec->CB->no_allocation) || (codec->type==GF_STREAM_AUDIO) ) ) {
		force_decode_mode = 2;
	}

	if (force_decode_mode) {
		u32 retry = 100;
		gf_odm_lock(mo->odm, 0);
		while (retry) {
			gf_odm_check_cb_resize(mo, codec);
			
			if (gf_term_lock_codec(codec, GF_TRUE, GF_TRUE)) {
				gf_codec_process(codec, 1);
				gf_term_lock_codec(codec, GF_FALSE, GF_TRUE);
			}
//            if (force_decode_mode==2) break;

			CU = gf_cm_get_output(codec->CB);
			if (CU)
				break;
			
			retry--;
			//we will wait max 100 ms for the CB to be re-fill
			gf_sleep(1);
		}
		if (!retry && (force_decode_mode==1)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] At %d could not resize, decode and fetch next frame in 100 ms - blank frame after TS %u\n", mo->odm->OD->objectDescriptorID, gf_clock_time(codec->ck), mo->timestamp));
		}
		if (!gf_odm_lock_mo(mo)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] ODM %d: Failed to lock ODM\n", mo->odm->OD->objectDescriptorID));
			return NULL;
		}
	}

	/*new frame to fetch, lock*/
	CU = gf_cm_get_output(codec->CB);
	/*no output*/
	if (!CU || (CU->RenderedLength == CU->dataLength)) {
		gf_odm_lock(mo->odm, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] No CU available\n", mo->odm->OD->objectDescriptorID));
		return NULL;
	}

	/*note this assert is NOT true when recomputing DTS from CTS on the fly (MPEG1/2 RTP and H264/AVC RTP)*/
	//assert(CU->TS >= codec->CB->LastRenderedTS);

	if (codec->CB->UnitCount<=1) resync = GF_MO_FETCH;

	if (bench_mode && resync) {
		resync = GF_MO_FETCH;
		if (mo->timestamp == CU->TS) {
			if (CU->next->dataLength) {
				gf_cm_drop_output(codec->CB);
				CU = gf_cm_get_output(codec->CB);
			}
		}
	}


	/*resync*/
	obj_time = gf_clock_time(codec->ck);

	skip_resync = GF_FALSE;
	//no drop mode, only for speed = 1: all frames are presented, we discard the current output only if already presented and next frame time is mature
	if ((codec->ck->speed == FIX_ONE) && (mo->type==GF_MEDIA_OBJECT_VIDEO)  && !(codec->flags & GF_ESM_CODEC_IS_LOW_LATENCY) ) {


		if (!(mo->odm->term->flags & GF_TERM_DROP_LATE_FRAMES)) {
			if (mo->odm->term->compositor->force_late_frame_draw) {
				mo->flags |= GF_MO_IN_RESYNC;
			}
			else if (mo->flags & GF_MO_IN_RESYNC) {
				if (CU->next->TS >= obj_time) {
					skip_resync = GF_TRUE;
					mo->flags &= ~GF_MO_IN_RESYNC;
				}
			}
			//if the next AU is at most 200 ms from the current clock use no drop mode
			else if (CU->next->dataLength && (CU->next->TS + 200 >= obj_time)) {
				skip_resync = GF_TRUE;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At %u frame TS %u next frame TS %d too late in no-drop mode, enabling drop - resync mode %d\n", mo->odm->OD->objectDescriptorID, obj_time, CU->TS, CU->next->TS, resync));
				mo->flags |= GF_MO_IN_RESYNC;
			}
		}
	}

	if (skip_resync) {
		resync=GF_MO_FETCH;
		if (mo->odm->term->use_step_mode) upload_time_ms=0;
		//we are in no resync mode, drop current frame once played and object time just matured
		//do it only if clock is started or if compositor step mode is set
		//the time threshold for fecthing is given by the caller
		if ( (gf_clock_is_started(codec->ck) || mo->odm->term->use_step_mode)

			&& (mo->timestamp==CU->TS) && CU->next->dataLength && (CU->next->TS <= obj_time + upload_time_ms) ) {
			
			gf_cm_drop_output(codec->CB);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Switching to next CU CTS %u now %u\n", mo->odm->OD->objectDescriptorID, CU->next->TS, obj_time));
			CU = gf_cm_get_output(codec->CB);
			if (!CU) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] No next CU available, skipping\n", mo->odm->OD->objectDescriptorID));
				gf_odm_lock(mo->odm, 0);
				return NULL;
			}
		}
	}

	if (resync) {
		while (1) {
			if (codec->ck->speed > 0 ? CU->TS >= obj_time : CU->TS <= obj_time )
				break;

			if (mo->timestamp != CU->TS) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Try to drop frame TS %u next frame TS %u length %d obj time %u\n", mo->odm->OD->objectDescriptorID, CU->TS, CU->next->TS, CU->next->dataLength, obj_time));
			}

			if (!CU->next->dataLength) {
				if (force_decode_mode) {
					obj_time = gf_clock_time(codec->ck);
					gf_odm_lock(mo->odm, 0);
					if (gf_term_lock_codec(codec, GF_TRUE, GF_TRUE)) {
						gf_codec_process(codec, 1);
						gf_term_lock_codec(codec, GF_FALSE, GF_TRUE);
					}
					gf_odm_lock(mo->odm, 1);
					if (!CU->next->dataLength) {
						break;
					}
				} else {
					break;
				}
			}

			/*figure out closest time*/
			if (codec->ck->speed > 0 ? CU->next->TS > obj_time : CU->next->TS < obj_time) {
				*eos = GF_FALSE;
				break;
			}

			if (mo->timestamp != CU->TS) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At OTB %u dropped frame TS %u\n", mo->odm->OD->objectDescriptorID, obj_time, CU->TS));
				codec->nb_dropped++;
			}

			/*discard*/
			CU->RenderedLength = CU->dataLength = 0;
			gf_cm_drop_output(codec->CB);

			/*get next*/
			CU = gf_cm_get_output(codec->CB);
			*eos = gf_cm_is_eos(codec->CB);
		}
	}

	mo->framesize = CU->dataLength - CU->RenderedLength;
	mo->frame = CU->data + CU->RenderedLength;
	mo->media_frame = CU->frame;

	if (CU->next->dataLength) {
		diff = (s32) (CU->next->TS) - (s32) obj_time;
	} else  {
		diff = mo->odm->codec->min_frame_dur;
	}
//	fprintf(stderr, "diff is %d ms\n", diff);
	mo->ms_until_next = FIX2INT(diff * mo->speed);

	diff = (mo->speed >= 0) ? (s32) (CU->TS) - (s32) obj_time : (s32) obj_time - (s32) (CU->TS);
	mo->ms_until_pres = FIX2INT(diff * mo->speed);


	if (mo->timestamp != CU->TS) {
#ifndef GPAC_DISABLE_VRML
		if (! gf_cm_is_eos(codec->CB))
			mediasensor_update_timing(mo->odm, 0);
#endif

		if (mo->odm->parentscene->is_dynamic_scene) {
			GF_Scene *s = mo->odm->parentscene;
			while (s && s->root_od->addon) {
				s = s->root_od->parentscene;
			}
			if (s && s->root_od)
				s->root_od->media_current_time = mo->odm->media_current_time;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d (%s)] At OTB %u fetch frame TS %u size %d (previous TS %u) - %d unit in CB - UTC "LLU" ms - %d ms until CTS is due - %d ms until next frame\n", mo->odm->OD->objectDescriptorID, mo->odm->net_service->url, gf_clock_time(codec->ck), CU->TS, mo->framesize, mo->timestamp, codec->CB->UnitCount, gf_net_get_utc(), mo->ms_until_pres, mo->ms_until_next ));

		if (CU->sender_ntp) {
			s32 ntp_diff = gf_net_get_ntp_diff_ms(CU->sender_ntp);
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d (%s)] Frame TS %u NTP diff with sender %d ms\n", mo->odm->OD->objectDescriptorID, mo->odm->net_service->url, CU->TS, ntp_diff));
		}

		mo->timestamp = CU->TS;
		/*signal EOS after rendering last frame, not while rendering it*/
		*eos = GF_FALSE;

	} else if (*eos) {
		//already rendered the last frame, consider we no longer have pending late frame on this stream
		mo->ms_until_pres = 0;
	} else {
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d (%s)] At OTB %u same frame fetch TS %u\n", mo->odm->OD->objectDescriptorID, mo->odm->net_service->url, obj_time, CU->TS ));
	}

	/*also adjust CU time based on consummed bytes in input, since some codecs output very large audio chunks*/
	if (codec->bytes_per_sec) mo->timestamp += CU->RenderedLength * 1000 / codec->bytes_per_sec;

	if (bench_mode) {
		mo->ms_until_pres = -1;
		mo->ms_until_next = 1;
	}


	mo->nb_fetch ++;
	*timestamp = mo->timestamp;
	*size = mo->framesize;
	if (ms_until_pres) *ms_until_pres = mo->ms_until_pres;
	if (ms_until_next) *ms_until_next = mo->ms_until_next;
	if (outFrame) *outFrame = mo->media_frame;

	gf_term_service_media_event(mo->odm, GF_EVENT_MEDIA_TIME_UPDATE);

	gf_odm_lock(mo->odm, 0);
	if (mo->media_frame)
		return (char *) mo->media_frame;

	if (codec->direct_vout)
		return (char *) codec->CB->pY;
	return mo->frame;
}

GF_EXPORT
GF_Err gf_mo_get_raw_image_planes(GF_MediaObject *mo, u8 **pY_or_RGB, u8 **pU, u8 **pV, u32 *stride_luma_rgb, u32 *stride_chroma)
{
	u32 stride;
	if (!mo || !mo->odm || !mo->odm->codec) return GF_BAD_PARAM;
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
}

GF_EXPORT
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, s32 drop_mode)
{
#if 0
	u32 obj_time;
#endif
	if (!gf_odm_lock_mo(mo))
		return;

	if (!mo->nb_fetch || !mo->odm->codec) {
		gf_odm_lock(mo->odm, 0);
		return;
	}
	mo->nb_fetch--;
	if (mo->nb_fetch) {
		gf_odm_lock(mo->odm, 0);
		return;
	}

	/*	if ((drop_mode==0) && !(mo->odm->term->flags & GF_TERM_DROP_LATE_FRAMES) && (mo->type==GF_MEDIA_OBJECT_VIDEO))
			drop_mode=1;
		else
	*/
	if (mo->odm->codec->CB->no_allocation && (mo->odm->codec->CB->Capacity==1) )
		drop_mode = 1;


	/*perform a sanity check on TS since the CB may have changed status - this may happen in
	temporal scalability only*/
	if (mo->odm->codec->CB->output->dataLength ) {
		if (nb_bytes==0xFFFFFFFF) {
			mo->odm->codec->CB->output->RenderedLength = mo->odm->codec->CB->output->dataLength;
		} else {
			assert(mo->odm->codec->CB->output->RenderedLength + nb_bytes <= mo->odm->codec->CB->output->dataLength);
			mo->odm->codec->CB->output->RenderedLength += nb_bytes;
		}

		if (drop_mode<0) {
			/*only allow for explicit last frame keeping if only one node is using the resource
			otherwise this would block the composition memory*/
			if (mo->num_open>1) drop_mode=0;
			else {
				gf_odm_lock(mo->odm, 0);
				return;
			}
		}

		/*discard frame*/
		if (mo->odm->codec->CB->output->RenderedLength == mo->odm->codec->CB->output->dataLength) {
			if (drop_mode) {
				gf_cm_drop_output(mo->odm->codec->CB);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At OTB %u released frame TS %u\n", mo->odm->OD->objectDescriptorID, gf_clock_time(mo->odm->codec->ck), mo->timestamp));
			} else {
				/*we cannot drop since we don't know the speed of the playback (which can even be frame by frame)*/

				//notif CB we kept the output
				gf_cm_output_kept(mo->odm->codec->CB);
			}
		}
	}
	gf_odm_lock(mo->odm, 0);
}

GF_EXPORT
void gf_mo_get_object_time(GF_MediaObject *mo, u32 *obj_time)
{
	if (!gf_odm_lock_mo(mo)) return;

	/*regular media codec...*/
	if (mo->odm->codec) {
		/*get absolute clock (without drift) for audio*/
		if (mo->odm->codec->type==GF_STREAM_AUDIO)
			*obj_time = gf_clock_real_time(mo->odm->codec->ck);
		else
			*obj_time = gf_clock_time(mo->odm->codec->ck);
	}
	/*BIFS object */
	else if (mo->odm->subscene && mo->odm->subscene->scene_codec) {
		*obj_time = gf_clock_time(mo->odm->subscene->scene_codec->ck);
	}
	/*unknown / unsupported object*/
	else {
		*obj_time = 0;
	}
	gf_odm_lock(mo->odm, 0);
}

GF_EXPORT
s32 gf_mo_get_clock_drift(GF_MediaObject *mo)
{
	s32 res = 0;
	if (!gf_odm_lock_mo(mo)) return 0;

	/*regular media codec...*/
	if (mo->odm->codec) {
		res = mo->odm->codec->ck->drift;
	}
	/*BIFS object */
	else if (mo->odm->subscene && mo->odm->subscene->scene_codec) {
		res = mo->odm->subscene->scene_codec->ck->drift;
	}
	gf_odm_lock(mo->odm, 0);
	return res;
}

GF_EXPORT
void gf_mo_play(GF_MediaObject *mo, Double clipBegin, Double clipEnd, Bool can_loop)
{
	if (!mo) return;

	if (!mo->num_open && mo->odm) {
		s32 res;
		Bool is_restart = GF_FALSE;

		/*remove object from media queue*/
		gf_term_lock_media_queue(mo->odm->term, GF_TRUE);
		res = gf_list_del_item(mo->odm->term->media_queue, mo->odm);
		gf_term_lock_media_queue(mo->odm->term, GF_FALSE);

		if (mo->odm->action_type!=GF_ODM_ACTION_PLAY) {
			mo->odm->action_type = GF_ODM_ACTION_PLAY;
			is_restart = GF_FALSE;
			res = -1;
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
			gf_odm_start(mo->odm, (res>=0) ? 1 : 0);
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

		/*do not stop directly, this can delete channel data currently being decoded (BIFS anim & co)*/
		gf_term_lock_media_queue(mo->odm->term, GF_TRUE);
		/*if object not in media queue, add it*/
		if (gf_list_find(mo->odm->term->media_queue, mo->odm)<0) {
			gf_list_add(mo->odm->term->media_queue, mo->odm);
		} else {
			//ODM was queued for PLAY and we stop it, keep in queue for stoping buffers but don't send net commands
			if (mo->odm->action_type == GF_ODM_ACTION_PLAY) {
				//force a stop without network commands
				mo->odm->state = GF_ODM_STATE_STOP_NO_NET;
			}
		}

		/*signal STOP request*/
		if ((mo->OD_ID==GF_MEDIA_EXTERNAL_ID) || (mo->odm && mo->odm->OD && (mo->odm->OD->objectDescriptorID==GF_MEDIA_EXTERNAL_ID))) {
			mo->odm->action_type = GF_ODM_ACTION_DELETE;
			ret = GF_TRUE;
		}
		else if (mo->odm->action_type!=GF_ODM_ACTION_DELETE) {
			mo->odm->action_type = GF_ODM_ACTION_STOP;
		}

		gf_term_lock_media_queue(mo->odm->term, GF_FALSE);
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
	if (!gf_odm_lock_mo(mo)) return;

#ifndef GPAC_DISABLE_VRML
	mediactrl_stack = gf_odm_get_mediacontrol(mo->odm);
#endif
	/*if no control and not root of a scene, check timelines are unlocked*/
	if (!mediactrl_stack && !mo->odm->subscene) {
		/*don't restart if sharing parent scene clock*/
		if (gf_odm_shares_clock(mo->odm, gf_odm_get_media_clock(mo->odm->parentscene->root_od))) {
			gf_odm_lock(mo->odm, 0);
			return;
		}
	}
	/*all other cases, call restart to take into account clock references*/
	mediacontrol_restart(mo->odm);
	gf_odm_lock(mo->odm, 0);
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
		strcpy(szURL1, obj->odm->net_service->url);
	} else {
		strcpy(szURL1, obj->URLs.vals[0].url);
	}

	/*don't analyse audio/video to locate segments or viewports*/
	if ((obj->type==GF_MEDIA_OBJECT_AUDIO) || (obj->type==GF_MEDIA_OBJECT_VIDEO)) {
		if (keep_fragment) *keep_fragment = GF_FALSE;
		include_sub_url = GF_TRUE;
	} else if ((obj->type==GF_MEDIA_OBJECT_SCENE) && keep_fragment && obj->odm) {
		GF_ClientService *ns;
		u32 j;
		/*for remoteODs/dynamic ODs, check if one of the running service cannot be used*/
		for (i=0; i<an_url->count; i++) {
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

			gf_term_lock_media_queue(obj->odm->term, 1);
			while ( (ns = (GF_ClientService*)gf_list_enum(obj->odm->term->net_services, &j)) ) {
				/*sub-service of an existing service - don't touch any fragment*/
				if (gf_term_service_can_handle_url(ns, an_url->vals[i].url)) {
					*keep_fragment = GF_TRUE;
					gf_term_lock_media_queue(obj->odm->term, 0);
					return GF_FALSE;
				}
			}
			gf_term_lock_media_queue(obj->odm->term, 0);
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
		gf_term_stop_codec(mo->odm->codec, GF_FALSE);
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
	if (mo->odm->net_service && mo->odm->net_service->owner && (mo->odm->net_service->owner->flags & GF_ODM_INHERIT_TIMELINE))
		return;
	gf_odm_set_speed(mo->odm, speed, GF_TRUE);
}

GF_EXPORT
Fixed gf_mo_get_current_speed(GF_MediaObject *mo)
{
	return (mo && mo->odm && mo->odm->codec && mo->odm->codec->ck) ? mo->odm->codec->ck->speed : FIX_ONE;
}

GF_EXPORT
u32 gf_mo_get_min_frame_dur(GF_MediaObject *mo)
{
	return (mo && mo->odm && mo->odm->codec) ? mo->odm->codec->min_frame_dur : 0;
}


GF_EXPORT
Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed)
{
	Fixed res = in_speed;

#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;

	if (!gf_odm_lock_mo(mo)) return in_speed;

	/*get control*/
	ctrl = gf_odm_get_mediacontrol(mo->odm);
	if (ctrl) res = ctrl->control->mediaSpeed;

	gf_odm_lock(mo->odm, 0);
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
	if (!gf_odm_lock_mo(mo)) return in_loop;

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
	gf_odm_lock(mo->odm, 0);
	return in_loop;
}

GF_EXPORT
Double gf_mo_get_duration(GF_MediaObject *mo)
{
	Double dur;
	if (!gf_odm_lock_mo(mo)) return -1.0;
	dur = ((Double) (s64)mo->odm->duration)/1000.0;
	gf_odm_lock(mo->odm, 0);
	return dur;
}

GF_EXPORT
Bool gf_mo_should_deactivate(GF_MediaObject *mo)
{
	Bool res = GF_FALSE;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif

	if (!gf_odm_lock_mo(mo)) return GF_FALSE;


	if (!mo->odm->state || (mo->odm->parentscene && mo->odm->parentscene->is_dynamic_scene)) {
		gf_odm_lock(mo->odm, 0);
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

	gf_odm_lock(mo->odm, 0);
	return res;
}

GF_EXPORT
Bool gf_mo_is_muted(GF_MediaObject *mo)
{
	Bool res = GF_FALSE;
#ifndef GPAC_DISABLE_VRML
	if (!gf_odm_lock_mo(mo)) return GF_FALSE;
	res = mo->odm->media_ctrl ? mo->odm->media_ctrl->control->mute : GF_FALSE;
	gf_odm_lock(mo->odm, 0);
#endif
	return res;
}

GF_EXPORT
Bool gf_mo_is_done(GF_MediaObject *mo)
{
	Bool res = GF_FALSE;
	GF_Codec *codec;
	u64 dur;
	if (!gf_odm_lock_mo(mo)) return GF_FALSE;

	if (mo->odm->codec && mo->odm->codec->CB) {
		/*for natural media use composition buffer*/
		res = (mo->odm->codec->CB->Status==CB_STOP) ? GF_TRUE : GF_FALSE;
	} else {
		/*otherwise check EOS and time*/
		codec = mo->odm->codec;
		dur = mo->odm->duration;
		if (!mo->odm->codec) {
			if (!mo->odm->subscene) res = GF_FALSE;
			else {
				codec = mo->odm->subscene->scene_codec;
				dur = mo->odm->subscene->duration;
			}
		}
		if (codec && (codec->Status==GF_ESM_CODEC_STOP)) {
			/*codec is done, check by duration*/
			GF_Clock *ck = gf_odm_get_media_clock(mo->odm);
			if (gf_clock_time(ck) > dur) res = GF_TRUE;
		}
	}
	gf_odm_lock(mo->odm, 0);
	return res;
}

/*resyncs clock - only audio objects are allowed to use this*/
GF_EXPORT
void gf_mo_adjust_clock(GF_MediaObject *mo, s32 ms_drift)
{
	if (!mo || !mo->odm) return;
	if (!mo->odm->codec || (mo->odm->codec->type != GF_STREAM_AUDIO) ) return;
	gf_clock_adjust_drift(mo->odm->codec->ck, ms_drift);
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
Bool gf_mo_is_private_media(GF_MediaObject *mo)
{
	if (mo && mo->odm && mo->odm->codec && mo->odm->codec->decio && (mo->odm->codec->decio->InterfaceType==GF_PRIVATE_MEDIA_DECODER_INTERFACE)) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
Bool gf_mo_set_position(GF_MediaObject *mo, GF_Window *src, GF_Window *dst)
{
	GF_Err e;
	GF_PrivateMediaDecoder *dec;
	if (!mo->odm || !mo->odm->codec || !mo->odm->codec->decio || (mo->odm->codec->decio->InterfaceType!=GF_PRIVATE_MEDIA_DECODER_INTERFACE)) return GF_FALSE;

	dec = (GF_PrivateMediaDecoder*)mo->odm->codec->decio;
	e = dec->Control(dec, GF_FALSE, src, dst);
	if (e==GF_BUFFER_TOO_SMALL) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
Bool gf_mo_is_raw_memory(GF_MediaObject *mo)
{
	if (!mo->odm || !mo->odm->codec) return GF_FALSE;
	return mo->odm->codec->direct_frame_output || mo->odm->codec->direct_vout ;
}

GF_EXPORT
u32 gf_mo_has_audio(GF_MediaObject *mo)
{
	char *sub_url, *ext;
	u32 i;
	GF_NetworkCommand com;
	GF_ClientService *ns;
	GF_Scene *scene;
	if (!mo || !mo->odm) return 0;
	if (mo->type != GF_MEDIA_OBJECT_VIDEO) return 0;
	if (!mo->odm->net_service) return 2;

	ns = mo->odm->net_service;
	scene = mo->odm->parentscene;
	sub_url = strchr(ns->url, '#');
	for (i=0; i<gf_list_count(scene->resources); i++) {
		GF_ObjectManager *odm = (GF_ObjectManager *)gf_list_get(scene->resources, i);
		if (odm->net_service != ns) continue;
		if (!odm->mo) continue;

		if (sub_url) {
			ext = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
			if (ext) ext = strchr(ext, '#');
			if (!ext || strcmp(sub_url, ext)) continue;
		}
		/*there is already an audio object in this service, do not recreate one*/
		if (odm->mo->type == GF_MEDIA_OBJECT_AUDIO) return 0;
	}
	memset(&com, 0, sizeof(GF_NetworkCommand) );
	com.command_type = GF_NET_SERVICE_HAS_AUDIO;
	com.audio.base_url = mo->URLs.count ? mo->URLs.vals[0].url : NULL;
	if (!com.audio.base_url) com.audio.base_url = ns->url;
	if (gf_term_service_command(ns, &com) == GF_OK) return 1;
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
	gf_free(mo);
}


Bool gf_mo_get_srd_info(GF_MediaObject *mo, GF_MediaObjectVRInfo *vr_info)
{
	GF_Scene *scene;
	if (!vr_info) return GF_FALSE;
	if (!gf_odm_lock_mo(mo)) return GF_FALSE;
	
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
	vr_info->has_full_coverage = (scene->srd_type==2) ? GF_TRUE : GF_FALSE;
	
	gf_sg_get_scene_size_info(scene->graph, &vr_info->scene_width, &vr_info->scene_height);

	gf_odm_lock(mo->odm, 0);
	return (!scene->vr_type && !scene->srd_type) ? GF_FALSE : GF_TRUE;
}

/*sets quality hint for this media object  - quality_rank is between 0 (min quality) and 100 (max quality)*/
void gf_mo_hint_quality_degradation(GF_MediaObject *mo, u32 quality_degradation)
{
	if (!gf_odm_lock_mo(mo)) return;
	if (!mo->odm || !mo->odm->codec) {
		gf_odm_lock(mo->odm, 0);
		return;
	}
	if (mo->quality_degradation_hint != quality_degradation) {
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_QUALITY_SWITCH;
		com.base.on_channel = gf_list_get(mo->odm->codec->inChannels, 0);
		com.switch_quality.quality_degradation = quality_degradation;
		gf_term_service_command(mo->odm->net_service, &com);
		
		mo->quality_degradation_hint = quality_degradation;
	}

	gf_odm_lock(mo->odm, 0);
}

void gf_mo_hint_visible_rect(GF_MediaObject *mo, u32 min_x, u32 max_x, u32 min_y, u32 max_y)
{
	
	if (!gf_odm_lock_mo(mo)) return;
	if (!mo->odm || !mo->odm->codec) {
		gf_odm_lock(mo->odm, 0);
		return;
	}
	if ((mo->view_min_x!=min_x) || (mo->view_max_x!=max_x) || (mo->view_min_y!=min_y) || (mo->view_max_y!=max_y)) {
		GF_NetworkCommand com;
		mo->view_min_x = min_x;
		mo->view_max_x = max_x;
		mo->view_min_y = min_y;
		mo->view_max_y = max_y;
		
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_CHAN_VISIBILITY_HINT;
		com.base.on_channel = gf_list_get(mo->odm->codec->inChannels, 0);
		com.visibility_hint.min_x = min_x;
		com.visibility_hint.max_x = max_x;
		com.visibility_hint.min_y = min_y;
		com.visibility_hint.max_y = max_y;
		gf_term_service_command(mo->odm->net_service, &com);
	}
	
	gf_odm_lock(mo->odm, 0);
}



