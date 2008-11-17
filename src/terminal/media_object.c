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


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_x3d.h>
#include "media_memory.h"
#include "media_control.h"
#include <gpac/nodes_svg.h>


static GF_MediaObject *get_sync_reference(GF_InlineScene *is, XMLRI *iri, u32 o_type, GF_Node *orig_ref, Bool *post_pone)
{
	MFURL mfurl;
	SFURL sfurl;
	GF_MediaObject *res;
	GF_Node *ref = NULL;

	u32 stream_id = 0;
	if (iri->type==XMLRI_STREAMID) {
		stream_id = iri->lsr_stream_id;
	} else if (!iri->string) {
		return NULL;
	} else {
		if (iri->target) ref = iri->target;
		else if (iri->string[0]=='#') ref = gf_sg_find_node_by_name(is->graph, iri->string+1);
		else ref = gf_sg_find_node_by_name(is->graph, iri->string);

		if (ref) {
#ifndef GPAC_DISABLE_SVG
			GF_FieldInfo info;
#endif
			/*safety check, break cyclic references*/
			if (ref==orig_ref) return NULL;

			switch (ref->sgprivate->tag) {
#ifndef GPAC_DISABLE_SVG
			case TAG_SVG_audio:
				o_type = GF_MEDIA_OBJECT_AUDIO; 
				if (gf_node_get_attribute_by_tag(ref, TAG_XLINK_ATT_href, 0, 0, &info)==GF_OK) {
					return get_sync_reference(is, info.far_ptr, o_type, orig_ref ? orig_ref : ref, post_pone);
				}
				return NULL;
			case TAG_SVG_video:
				o_type = GF_MEDIA_OBJECT_VIDEO; 
				if (gf_node_get_attribute_by_tag(ref, TAG_XLINK_ATT_href, 0, 0, &info)==GF_OK) {
					return get_sync_reference(is, info.far_ptr, o_type, orig_ref ? orig_ref : ref, post_pone);
				}
				return NULL;
#endif
			default:
				return NULL;
			}
		}
	}
	*post_pone = 0;
	mfurl.count = 1;
	mfurl.vals = &sfurl;
	mfurl.vals[0].OD_ID = stream_id;
	mfurl.vals[0].url = iri->string;

	res = gf_inline_get_media_object(is, &mfurl, o_type, 0);
	if (!res) *post_pone = 1;
	return res;
}

GF_EXPORT
GF_MediaObject *gf_mo_register(GF_Node *node, MFURL *url, Bool lock_timelines)
{
	u32 obj_type;
#ifndef GPAC_DISABLE_SVG
	Bool post_pone;
	GF_FieldInfo info;
#endif
	GF_InlineScene *is;
	GF_MediaObject *res, *syncRef;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	is = (GF_InlineScene*)gf_sg_get_private(sg);
	if (!is) return NULL;

	syncRef = NULL;

	/*keep track of the kind of object expected if URL is not using OD scheme*/
	switch (gf_node_get_tag(node)) {
	/*MPEG4 only*/
	case TAG_MPEG4_AudioSource: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_MPEG4_AnimationStream: 
		obj_type = GF_MEDIA_OBJECT_UPDATES; 
		break;

	case TAG_MPEG4_InputSensor: obj_type = GF_MEDIA_OBJECT_INTERACT; break;

	/*MPEG4/X3D*/
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_Background2D: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_Background: case TAG_X3D_Background: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_MPEG4_Inline: case TAG_X3D_Inline: obj_type = GF_MEDIA_OBJECT_SCENE; break;
	
	/*SVG*/
#ifndef GPAC_DISABLE_SVG
	case TAG_SVG_audio: 
		obj_type = GF_MEDIA_OBJECT_AUDIO; 
		if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_syncReference, 0, 0, &info)==GF_OK) {
			syncRef = get_sync_reference(is, info.far_ptr, GF_MEDIA_OBJECT_UNDEF, node, &post_pone);
			/*syncRef is specified but doesn't exist yet, post-pone*/
			if (post_pone) return NULL;
		}
		break;
	case TAG_SVG_video: 
		obj_type = GF_MEDIA_OBJECT_VIDEO; 
		if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_syncReference, 0, 0, &info)==GF_OK) {
			syncRef = get_sync_reference(is, info.far_ptr, GF_MEDIA_OBJECT_UNDEF, node, &post_pone);
			/*syncRef is specified but doesn't exist yet, post-pone*/
			if (post_pone) return NULL;
		}
		break;
	case TAG_SVG_image: 
		obj_type = GF_MEDIA_OBJECT_VIDEO; 
		break;
#endif

	default: obj_type = GF_MEDIA_OBJECT_UNDEF; break;
	}

	/*move to primary resource handler*/
	while (is->secondary_resource && is->root_od->parentscene)
		is = is->root_od->parentscene;

	res = gf_inline_get_media_object_ex(is, url, obj_type, lock_timelines, syncRef, 0, node);

	if (res) {
	}
	return res;
}

void gf_mo_unregister(GF_Node *node, GF_MediaObject *mo)
{
	if (mo && node) {
		gf_list_del_item(mo->nodes, node);
	}
}

GF_MediaObject *gf_mo_new()
{
	GF_MediaObject *mo;
	mo = (GF_MediaObject *) malloc(sizeof(GF_MediaObject));
	memset(mo, 0, sizeof(GF_MediaObject));
	mo->speed = FIX_ONE;
	mo->URLs.count = 0;
	mo->URLs.vals = NULL;
	mo->nodes = gf_list_new();
	return mo;
}


GF_EXPORT
Bool gf_mo_get_visual_info(GF_MediaObject *mo, u32 *width, u32 *height, u32 *stride, u32 *pixel_ar, u32 *pixelFormat)
{
	GF_CodecCapability cap;
	if ((mo->type != GF_MEDIA_OBJECT_VIDEO) && (mo->type!=GF_MEDIA_OBJECT_TEXT)) return 0;

	if (width) {
		cap.CapCode = GF_CODEC_WIDTH;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*width = cap.cap.valueInt;
	}
	if (height) {
		cap.CapCode = GF_CODEC_HEIGHT;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*height = cap.cap.valueInt;
	}
	if (mo->type==GF_MEDIA_OBJECT_TEXT) return 1;

	if (stride) {
		cap.CapCode = GF_CODEC_STRIDE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*stride = cap.cap.valueInt;
	}
	if (pixelFormat) {
		cap.CapCode = GF_CODEC_PIXEL_FORMAT;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*pixelFormat = cap.cap.valueInt;
	}
	/*get PAR settings*/
	if (pixel_ar) {
		cap.CapCode = GF_CODEC_PAR;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*pixel_ar = cap.cap.valueInt;
		if (! (*pixel_ar & 0x0000FFFF)) *pixel_ar = 0;
		if (! (*pixel_ar & 0xFFFF0000)) *pixel_ar = 0;

		/**/
		if (! *pixel_ar) {
			GF_Channel *ch;
			GF_NetworkCommand com;
			com.base.command_type = GF_NET_CHAN_GET_PIXEL_AR;
			ch = gf_list_get(mo->odm->channels, 0);
			if (!ch) return 0;

			com.base.on_channel = ch;
			com.par.hSpacing = com.par.vSpacing = 0;
			if (gf_term_service_command(ch->service, &com) == GF_OK) {
				if ((com.par.hSpacing>65535) || (com.par.vSpacing>65535)) {
					com.par.hSpacing>>=16;
					com.par.vSpacing>>=16;
				}
				if (com.par.hSpacing|| com.par.vSpacing)
					*pixel_ar = (com.par.hSpacing<<16) | com.par.vSpacing;
			}
		}
	}
	return 1;
}

GF_EXPORT
Bool gf_mo_get_audio_info(GF_MediaObject *mo, u32 *sample_rate, u32 *bits_per_sample, u32 *num_channels, u32 *channel_config)
{
	GF_CodecCapability cap;
	if (!mo->odm || !mo->odm->codec || (mo->type != GF_MEDIA_OBJECT_AUDIO)) return 0;

	if (sample_rate) {
		cap.CapCode = GF_CODEC_SAMPLERATE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*sample_rate = cap.cap.valueInt;
	}
	if (num_channels) {
		cap.CapCode = GF_CODEC_NB_CHAN;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*num_channels = cap.cap.valueInt;
	}
	if (bits_per_sample) {
		cap.CapCode = GF_CODEC_BITS_PER_SAMPLE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*bits_per_sample = cap.cap.valueInt;
	}
	if (channel_config) {
		cap.CapCode = GF_CODEC_CHANNEL_CONFIG;
		gf_codec_get_capability(mo->odm->codec, &cap);
		*channel_config = cap.cap.valueInt;
	}
	return 1;
}

void gf_mo_update_caps(GF_MediaObject *mo)
{
	GF_CodecCapability cap;

	mo->flags &= ~GF_MO_IS_INIT;

	if (mo->type == GF_MEDIA_OBJECT_VIDEO) {
		cap.CapCode = GF_CODEC_FPS;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->odm->codec->fps = cap.cap.valueFloat;
	}
	else if (mo->type == GF_MEDIA_OBJECT_AUDIO) {
		u32 sr, nb_ch, bps;
		sr = nb_ch = bps = 0;
		gf_mo_get_audio_info(mo, &sr, &bps, &nb_ch, NULL);
		mo->odm->codec->bytes_per_sec = sr * nb_ch * bps / 8;
	}
}

GF_EXPORT
char *gf_mo_fetch_data(GF_MediaObject *mo, Bool resync, Bool *eos, u32 *timestamp, u32 *size)
{
	u32 obj_time;
	GF_CMUnit *CU;
	*eos = 0;

	if (!gf_odm_lock_mo(mo)) return NULL;

	if (!mo->odm->codec || !mo->odm->codec->CB) {
		gf_odm_lock(mo->odm, 0);
		return NULL;
	}

	/*if frame locked return it*/
	if (mo->nb_fetch) {
		*eos = 0;
		*timestamp = mo->timestamp;
		*size = mo->framesize;
		mo->nb_fetch ++;
		gf_odm_lock(mo->odm, 0);
		return mo->frame;
	}

	/*end of stream */
	*eos = gf_cm_is_eos(mo->odm->codec->CB);

	/*not running and no resync (ie audio)*/
	if (!resync && !gf_cm_is_running(mo->odm->codec->CB)) {
		gf_odm_lock(mo->odm, 0);
		return NULL;
	}

	/*new frame to fetch, lock*/
	CU = gf_cm_get_output(mo->odm->codec->CB);
	/*no output*/
	if (!CU || (CU->RenderedLength == CU->dataLength)) {
		gf_odm_lock(mo->odm, 0);
		return NULL;
	}

	/*note this assert is NOT true when recomputing DTS from CTS on the fly (MPEG1/2 RTP and H264/AVC RTP)*/
	//assert(CU->TS >= mo->odm->codec->CB->LastRenderedTS);

	if (mo->odm->codec->CB->UnitCount==1) resync = 0;

	/*resync*/
	if (resync) {
		u32 nb_droped = 0;
		obj_time = gf_clock_time(mo->odm->codec->ck);
		while (CU->TS < obj_time) {
			if (!CU->next->dataLength) break;
			/*figure out closest time*/
			if (CU->next->TS > obj_time) {
				*eos = 0;
				break;
			}
			nb_droped ++;
			if (nb_droped>1) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] At OTB %d dropped frame TS %d\n", mo->odm->OD->objectDescriptorID, obj_time, CU->TS));
				mo->odm->codec->nb_droped++;
			}
			/*discard*/
			CU->RenderedLength = CU->dataLength = 0;
			gf_cm_drop_output(mo->odm->codec->CB);
			/*get next*/
			CU = gf_cm_get_output(mo->odm->codec->CB);
			*eos = gf_cm_is_eos(mo->odm->codec->CB);
		}
	}	
	mo->framesize = CU->dataLength - CU->RenderedLength;
	mo->frame = CU->data + CU->RenderedLength;
	if (mo->timestamp != CU->TS) {
		MS_UpdateTiming(mo->odm, *eos);
		mo->timestamp = CU->TS;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] At OTB %d fetch frame TS %d size %d - %d unit in CB\n", mo->odm->OD->objectDescriptorID, gf_clock_time(mo->odm->codec->ck), mo->timestamp, mo->framesize, mo->odm->codec->CB->UnitCount));
		/*signal EOS after rendering last frame, not while rendering it*/
		*eos = 0;
	}

	/*also adjust CU time based on consummed bytes in input, since some codecs output very large audio chunks*/
	if (mo->odm->codec->bytes_per_sec) mo->timestamp += CU->RenderedLength * 1000 / mo->odm->codec->bytes_per_sec;

	mo->nb_fetch ++;
	*timestamp = mo->timestamp;
	*size = mo->framesize;

	gf_odm_lock(mo->odm, 0);
	return mo->frame;
}

GF_EXPORT
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, s32 forceDrop)
{
	u32 obj_time;
	if (!gf_odm_lock_mo(mo)) return;

	if (!mo->nb_fetch || !mo->odm->codec) {
		gf_odm_lock(mo->odm, 0);
		return;
	}
	mo->nb_fetch--;
	if (mo->nb_fetch) {
		gf_odm_lock(mo->odm, 0);
		return;
	}

	/*perform a sanity check on TS since the CB may have changed status - this may happen in 
	temporal scalability only*/
	if (mo->odm->codec->CB->output->dataLength ) {
		if (nb_bytes==0xFFFFFFFF) {
			mo->odm->codec->CB->output->RenderedLength = mo->odm->codec->CB->output->dataLength;
		} else {
			assert(mo->odm->codec->CB->output->RenderedLength + nb_bytes <= mo->odm->codec->CB->output->dataLength);
			mo->odm->codec->CB->output->RenderedLength += nb_bytes;
		}

		if (forceDrop<0) {
			/*only allow for explicit last frame keeping if only one node is using the resource
			otherwise this would block the composition memory*/
			if (mo->num_open>1) forceDrop=0;
			else {
				gf_odm_lock(mo->odm, 0);
				return;
			}
		}

		/*discard frame*/
		if (mo->odm->codec->CB->output->RenderedLength == mo->odm->codec->CB->output->dataLength) {
			if (forceDrop) {
				gf_cm_drop_output(mo->odm->codec->CB);
				forceDrop--;
//				if (forceDrop) mo->odm->codec->nb_droped++;
			} else {
				obj_time = gf_clock_time(mo->odm->codec->ck);
				if (mo->odm->codec->CB->output->next->dataLength) { 
					if (2*obj_time < mo->timestamp + mo->odm->codec->CB->output->next->TS ) {
						mo->odm->codec->CB->output->RenderedLength = 0;
					} else {
						gf_cm_drop_output(mo->odm->codec->CB);
					}
				} else {
					gf_cm_drop_output(mo->odm->codec->CB);
				}
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
void gf_mo_play(GF_MediaObject *mo, Double clipBegin, Double clipEnd, Bool can_loop)
{
	if (!mo) return;

	if (!mo->num_open && mo->odm) {
		Bool is_restart = 0;

		/*remove object from media queue*/
		gf_term_lock_net(mo->odm->term, 1);
		gf_list_del_item(mo->odm->term->media_queue, mo->odm);
		gf_term_lock_net(mo->odm->term, 0);

		if (mo->odm->media_start_time == (u64) -1) is_restart = 1;

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
				if (mo->odm->duration && (mo->odm->media_stop_time > mo->odm->duration)) {
					mo->odm->media_stop_time = 0;
				}
			} else {
				mo->odm->media_stop_time = 0;
			}
		}
		if (is_restart) {
			MC_Restart(mo->odm);
		} else {
			if (mo->odm->subscene && mo->odm->subscene->is_dynamic_scene)
				mo->odm->flags |= GF_ODM_REGENERATE_SCENE;

			gf_odm_start(mo->odm);
		}
	} else if (mo->odm) {
		if (mo->num_to_restart) mo->num_restart--;
		if (!mo->num_restart && (mo->num_to_restart==mo->num_open+1) ) {
			MC_Restart(mo->odm);
			mo->num_to_restart = mo->num_restart = 0;
		}
	}
	mo->num_open++;
}

GF_EXPORT
void gf_mo_stop(GF_MediaObject *mo)
{
	if (!mo || !mo->num_open) return;

	mo->num_open--;
	if (!mo->num_open && mo->odm) {
		/*do not stop directly, this can delete channel data currently being decoded (BIFS anim & co)*/
		gf_mx_p(mo->odm->term->net_mx);
		/*if object not in media queue, add it*/
		if (gf_list_find(mo->odm->term->media_queue, mo->odm)<0)
			gf_list_add(mo->odm->term->media_queue, mo->odm);
		
		/*signal STOP request*/
		mo->odm->media_start_time = (u64)-1;

		gf_mx_v(mo->odm->term->net_mx);
	} else {
		if (!mo->num_to_restart) {
			mo->num_restart = mo->num_to_restart = mo->num_open + 1;
		}
	}
}

GF_EXPORT
void gf_mo_restart(GF_MediaObject *mo)
{
	MediaControlStack *ctrl;
	if (!gf_odm_lock_mo(mo)) return;

	ctrl = ODM_GetMediaControl(mo->odm);
	/*if no control and not root of a scene, check timelines are unlocked*/
	if (!ctrl && !mo->odm->subscene) {
		/*don't restart if sharing parent scene clock*/
		if (gf_odm_shares_clock(mo->odm, gf_odm_get_media_clock(mo->odm->parentscene->root_od))) {
			gf_odm_lock(mo->odm, 0);
			return;
		}
	}
	/*all other cases, call restart to take into account clock references*/
	MC_Restart(mo->odm);
	gf_odm_lock(mo->odm, 0);
}

GF_EXPORT
Bool gf_mo_url_changed(GF_MediaObject *mo, MFURL *url)
{
	u32 od_id;
	Bool ret = 0;
	if (!mo) return (url ? 1 : 0);
	od_id = URL_GetODID(url);
	if ( (mo->OD_ID == GF_ESM_DYNAMIC_OD_ID) && (od_id == GF_ESM_DYNAMIC_OD_ID)) {
		ret = !gf_mo_is_same_url(mo, url);
	} else {
		ret = (mo->OD_ID == od_id) ? 0 : 1;
	}
	/*special case for 3GPP text: if not playing and user node changed, force removing it*/
	if (ret && mo->odm && !mo->num_open && (mo->type == GF_MEDIA_OBJECT_TEXT)) {
		mo->flags |= GF_MO_DISPLAY_REMOVE;
		gf_term_stop_codec(mo->odm->codec);
	}
	return ret;
}

GF_EXPORT
void gf_mo_pause(GF_MediaObject *mo)
{
	if (!mo || !mo->num_open || !mo->odm) return;

	MC_Pause(mo->odm);
}

GF_EXPORT
void gf_mo_resume(GF_MediaObject *mo)
{
	if (!mo || !mo->num_open || !mo->odm) return;

	MC_Resume(mo->odm);
}

GF_EXPORT
void gf_mo_set_speed(GF_MediaObject *mo, Fixed speed)
{
	MediaControlStack *ctrl;

	if (!mo) return;
	if (!mo->odm) {
		mo->speed = speed;
		return;
	}
	/*if media control forbidd that*/
	ctrl = ODM_GetMediaControl(mo->odm);
	if (ctrl) return;
	gf_odm_set_speed(mo->odm, speed);
}

GF_EXPORT
Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed)
{
	Fixed res;
	MediaControlStack *ctrl;
	if (!gf_odm_lock_mo(mo)) return in_speed;
	/*get control*/
	ctrl = ODM_GetMediaControl(mo->odm);
	res = ctrl ? ctrl->control->mediaSpeed : in_speed;
	gf_odm_lock(mo->odm, 0);
	return res;
}

GF_EXPORT
Bool gf_mo_get_loop(GF_MediaObject *mo, Bool in_loop)
{
	GF_Clock *ck;
	MediaControlStack *ctrl;
	if (!gf_odm_lock_mo(mo)) return in_loop;
	
	/*get control*/
	ctrl = ODM_GetMediaControl(mo->odm);
	if (ctrl) in_loop = ctrl->control->loop;

	/*otherwise looping is only accepted if not sharing parent scene clock*/
	ck = gf_odm_get_media_clock(mo->odm->parentscene->root_od);
	if (gf_odm_shares_clock(mo->odm, ck)) in_loop = 0;
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
	Bool res = 0;
	MediaControlStack *ctrl;

	if (!gf_odm_lock_mo(mo)) return 0;
	
	if (!mo->odm->state) {
		gf_odm_lock(mo->odm, 0);
		return 0;
	}

	/*get media control and see if object owning control is running*/
	ctrl = ODM_GetMediaControl(mo->odm);
	if (!ctrl) res = 1;
	/*if ctrl and ctrl not ruling this mediaObject, deny deactivation*/
	else if (ctrl->stream->odm != mo->odm) res = 0;
	/*this is currently under discussion in MPEG. for now we deny deactivation as soon as a mediaControl is here*/
	else if (ctrl->stream->odm->state) res = 0;
	/*otherwise allow*/	
	else res = 1;

	gf_odm_lock(mo->odm, 0);
	return res;
}

GF_EXPORT
Bool gf_mo_is_muted(GF_MediaObject *mo)
{
	Bool res = 0;
	if (!gf_odm_lock_mo(mo)) return 0;
	res = mo->odm->media_ctrl ? mo->odm->media_ctrl->control->mute : 0;
	gf_odm_lock(mo->odm, 0);
	return res;
}

GF_EXPORT
Bool gf_mo_is_done(GF_MediaObject *mo)
{
	Bool res = 0;
	GF_Codec *codec;
	u64 dur;
	if (!gf_odm_lock_mo(mo)) return 0;

	/*for natural media use composition buffer*/
	if (mo->odm->codec && mo->odm->codec->CB) 
		res = (mo->odm->codec->CB->Status==CB_STOP) ? 1 : 0;

	/*otherwise check EOS and time*/
	else {
		codec = mo->odm->codec;
		dur = mo->odm->duration;
		if (!mo->odm->codec) {
			if (!mo->odm->subscene) res = 0;
			else {
				codec = mo->odm->subscene->scene_codec;
				dur = mo->odm->subscene->duration;
			}
		}
		if (codec && (codec->Status==GF_ESM_CODEC_STOP)) {
			/*codec is done, check by duration*/
			GF_Clock *ck = gf_odm_get_media_clock(mo->odm);
			if (gf_clock_time(ck) > dur) res = 1;
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
Bool gf_mo_has_audio(GF_MediaObject *mo)
{
	char *sub_url, *ext;
	u32 i;
	GF_NetworkCommand com;
	GF_ClientService *ns;
	GF_InlineScene *is;
	if (!mo || !mo->odm) return 0;
	if (mo->type != GF_MEDIA_OBJECT_VIDEO) return 0;

	ns = mo->odm->net_service;
	is = mo->odm->parentscene;
	sub_url = strchr(ns->url, '#');
	for (i=0; i<gf_list_count(is->ODlist); i++) {
		GF_ObjectManager *odm = gf_list_get(is->ODlist, i);
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

