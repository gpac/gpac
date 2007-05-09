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
#include <gpac/nodes_x3d.h>
#include "media_memory.h"
#include "media_control.h"
#include <gpac/nodes_svg_sa.h>
#include <gpac/nodes_svg_sani.h>
#include <gpac/nodes_svg_da.h>


GF_EXPORT
GF_MediaObject *gf_mo_find(GF_Node *node, MFURL *url, Bool lock_timelines)
{
	u32 obj_type;
	GF_InlineScene *is;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	is = (GF_InlineScene*)gf_sg_get_private(sg);
	if (!is) return NULL;

	/*keep track of the kind of object expected if URL is not using OD scheme*/
	switch (gf_node_get_tag(node)) {
	/*MPEG4 only*/
	case TAG_MPEG4_AudioSource: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_MPEG4_AnimationStream: obj_type = GF_MEDIA_OBJECT_BIFS; break;
	case TAG_MPEG4_InputSensor: obj_type = GF_MEDIA_OBJECT_INTERACT; break;

	/*MPEG4/X3D*/
	case TAG_MPEG4_MovieTexture: case TAG_X3D_MovieTexture: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_Background2D: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_Background: case TAG_X3D_Background: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_ImageTexture: case TAG_X3D_ImageTexture: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_MPEG4_AudioClip: case TAG_X3D_AudioClip: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_MPEG4_Inline: case TAG_X3D_Inline: obj_type = GF_MEDIA_OBJECT_SCENE; break;
	
	/*SVG*/
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_audio: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_SVG_SA_image: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_SVG_SA_video: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_audio: obj_type = GF_MEDIA_OBJECT_AUDIO; break;
	case TAG_SVG_SANI_image: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_SVG_SANI_video: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
#endif
	case TAG_SVG_audio: 
		obj_type = GF_MEDIA_OBJECT_AUDIO; 
		break;
	case TAG_SVG_image: 
		obj_type = GF_MEDIA_OBJECT_VIDEO; 
		break;
	case TAG_SVG_video: 
		obj_type = GF_MEDIA_OBJECT_VIDEO; 
		break;

	default: obj_type = GF_MEDIA_OBJECT_UNDEF; break;
	}
	return gf_is_get_media_object(is, url, obj_type, lock_timelines);
}

GF_MediaObject *gf_mo_new()
{
	GF_MediaObject *mo;
	mo = (GF_MediaObject *) malloc(sizeof(GF_MediaObject));
	memset(mo, 0, sizeof(GF_MediaObject));
	mo->speed = FIX_ONE;
	mo->URLs.count = 0;
	mo->URLs.vals = NULL;
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

void MO_UpdateCaps(GF_MediaObject *mo)
{
	GF_CodecCapability cap;

	mo->flags &= ~GF_MO_IS_INIT;

	if (mo->type == GF_MEDIA_OBJECT_VIDEO) {
		cap.CapCode = GF_CODEC_FPS;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->odm->codec->fps = cap.cap.valueFloat;
	}
	else if (mo->type == GF_MEDIA_OBJECT_AUDIO) {
		void gf_sr_lock_audio(void *, Bool );
		u32 sr, nb_ch, bps;
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

	if (!mo) return NULL;

	if (!mo->odm || !mo->odm->codec || !mo->odm->codec->CB) return NULL;

	gf_odm_lock(mo->odm, 1);

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
				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] At OTB %d dropped frame TS %d\n", obj_time, mo->odm->OD->objectDescriptorID, CU->TS));
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
void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, u32 forceDrop)
{
	u32 obj_time;
	if (!mo || !mo->nb_fetch) return;
	assert(mo->odm);
	mo->nb_fetch--;
	if (mo->nb_fetch) return;

	gf_odm_lock(mo->odm, 1);

	/*perform a sanity check on TS since the CB may have changed status - this may happen in 
	temporal scalability only*/
	if (mo->odm->codec->CB->output->dataLength ) {
		if (nb_bytes==0xFFFFFFFF) {
			mo->odm->codec->CB->output->RenderedLength = mo->odm->codec->CB->output->dataLength;
		} else {
			assert(mo->odm->codec->CB->output->RenderedLength + nb_bytes <= mo->odm->codec->CB->output->dataLength);
			mo->odm->codec->CB->output->RenderedLength += nb_bytes;
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
	if (!mo || !mo->odm || !obj_time) return;

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
}


GF_EXPORT
void gf_mo_play(GF_MediaObject *mo, Double media_offset, Bool can_loop)
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
			mo->odm->media_start_time = (u64) (media_offset*1000);
			if (mo->odm->duration && (mo->odm->media_start_time > mo->odm->duration)) {
				if (can_loop) {
					mo->odm->media_start_time %= mo->odm->duration;
				} else {
					mo->odm->media_start_time = mo->odm->duration;
				}
			}
		}
		if (is_restart) {
			MC_Restart(mo->odm);
		} else {
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
		if (gf_list_find(mo->odm->term->media_queue, mo->odm)<0) {
			gf_list_add(mo->odm->term->media_queue, mo->odm);
			mo->odm->media_start_time = (u64)-1;
		}
		/*otherwise a play is pending, remove it*/
		else if (mo->odm->media_start_time >= 0) {
			gf_list_del_item(mo->odm->term->media_queue, mo->odm);
		}
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
	if (!mo) return;
	assert(mo->num_open);

	ctrl = ODM_GetMediaControl(mo->odm);
	/*if no control and not root of a scene, check timelines are unlocked*/
	if (!ctrl && !mo->odm->subscene) {
		/*don't restart if sharing parent scene clock*/
		if (gf_odm_shares_clock(mo->odm, gf_odm_get_media_clock(mo->odm->parentscene->root_od))) return;
	}
	/*all other cases, call restart to take into account clock references*/
	MC_Restart(mo->odm);
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
	if (!mo || !mo->odm) return in_speed;
	gf_odm_lock(mo->odm, 1);
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
	if (!mo || !mo->odm) return in_loop;
	
	gf_odm_lock(mo->odm, 1);
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
	if (!mo || !mo->odm) return -1.0;
	return ((Double) (s64)mo->odm->duration)/1000.0;
}

GF_EXPORT
Bool gf_mo_should_deactivate(GF_MediaObject *mo)
{
	Bool res = 0;
	MediaControlStack *ctrl;
	
	if (!mo || !mo->odm) return 0;
	/*not running*/
	if (!mo->odm->state) return 0;

	gf_odm_lock(mo->odm, 1);

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
	if (!mo->odm) return 1;
	gf_odm_lock(mo->odm, 1);
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
	if (!mo || !mo->odm) return 0;

	gf_odm_lock(mo->odm, 1);
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

u32 gf_mo_get_last_frame_time(GF_MediaObject *mo)
{
	return mo ? mo->timestamp : 0;
}

