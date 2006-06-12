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
#include <gpac/nodes_svg.h>


GF_MediaObject *gf_mo_find(GF_Node *node, MFURL *url)
{
	u32 obj_type;
	GF_InlineScene *is;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	is = gf_sg_get_private(sg);
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
	case TAG_SVG_image: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_SVG_video: obj_type = GF_MEDIA_OBJECT_VIDEO; break;
	case TAG_SVG_audio: obj_type = GF_MEDIA_OBJECT_AUDIO; break;

	default: obj_type = GF_MEDIA_OBJECT_UNDEF; break;
	}
	return gf_is_get_media_object(is, url, obj_type);
}

GF_MediaObject *gf_mo_new(GF_Terminal *term)
{
	GF_MediaObject *mo;
	mo = (GF_MediaObject *) malloc(sizeof(GF_MediaObject));
	memset(mo, 0, sizeof(GF_MediaObject));
	mo->speed = FIX_ONE;
	mo->URLs.count = 0;
	mo->URLs.vals = NULL;
	mo->term = term;
	return mo;
}


void MO_UpdateCaps(GF_MediaObject *mo)
{
	GF_CodecCapability cap;

	if (mo->type == GF_MEDIA_OBJECT_VIDEO) {
		cap.CapCode = GF_CODEC_WIDTH;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->width = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_HEIGHT;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->height = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_STRIDE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->stride = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_PIXEL_FORMAT;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->pixelFormat = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_FPS;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->odm->codec->fps = cap.cap.valueFloat;
		/*get PAR settings*/
		cap.CapCode = GF_CODEC_PAR;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->pixel_ar = cap.cap.valueInt;
		if (! (mo->pixel_ar & 0x0000FFFF)) mo->pixel_ar = 0;
		if (! (mo->pixel_ar & 0xFFFF0000)) mo->pixel_ar = 0;
		mo->mo_flags &= ~GF_MO_IS_INIT;
	}
	else if (mo->type == GF_MEDIA_OBJECT_AUDIO) {
		void gf_sr_lock_audio(void *, Bool );

		cap.CapCode = GF_CODEC_SAMPLERATE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->sample_rate = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_NB_CHAN;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->num_channels = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_BITS_PER_SAMPLE;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->bits_per_sample = cap.cap.valueInt;
		mo->odm->codec->bytes_per_sec = mo->sample_rate * mo->num_channels * mo->bits_per_sample / 8;
		cap.CapCode = GF_CODEC_CHANNEL_CONFIG;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->channel_config = cap.cap.valueInt;

		gf_sr_lock_audio(mo->term->renderer, 1);
		mo->mo_flags &= ~GF_MO_IS_INIT;
		gf_sr_lock_audio(mo->term->renderer, 0);
	}
	else if (mo->type==GF_MEDIA_OBJECT_TEXT) {
		cap.CapCode = GF_CODEC_WIDTH;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->width = cap.cap.valueInt;
		cap.CapCode = GF_CODEC_HEIGHT;
		gf_codec_get_capability(mo->odm->codec, &cap);
		mo->height = cap.cap.valueInt;
		mo->mo_flags &= ~GF_MO_IS_INIT;
	}
}

Bool gf_mo_fetch_data(GF_MediaObject *mo, Bool resync, Bool *eos)
{
	Bool ret;
	u32 obj_time;
	GF_CMUnit *CU;
	*eos = 0;

	if (!mo) return 0;

	if (!mo->odm || !mo->odm->codec || !mo->odm->codec->CB) return 0;
	/*check if we need to play*/
	if (mo->num_open && !mo->odm->is_open) {
		gf_odm_start(mo->odm);
		return 0;
	}

	ret = 0;
	gf_cm_lock(mo->odm->codec->CB, 1);

	/*end of stream */
	*eos = gf_cm_is_eos(mo->odm->codec->CB);
	/*not running and no resync (ie audio)*/
	if (!resync && !gf_cm_is_running(mo->odm->codec->CB)) goto exit;
	/*if not running but resync (video, image), try to load the composition memory anyway*/

	/*if frame locked return it*/
	if (mo->num_fetched) {
		*eos = 0;
		ret = 1;
		mo->num_fetched++;
		goto exit;
	}

	/*new frame to fetch, lock*/
	CU = gf_cm_get_output(mo->odm->codec->CB);
	/*no output*/
	if (!CU || (CU->RenderedLength == CU->dataLength)) {
		goto exit;
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
			if (CU->next->TS > obj_time) break;
			nb_droped ++;
			if (nb_droped>1) {
				//fprintf(stdout, "droping frame TS %d time %d\n", CU->TS, obj_time);
				mo->odm->codec->nb_droped++;
			}
			/*discard*/
			CU->RenderedLength = CU->dataLength = 0;
			gf_cm_drop_output(mo->odm->codec->CB);
			/*get next*/
			CU = gf_cm_get_output(mo->odm->codec->CB);
		}
	}
	mo->current_size = CU->dataLength - CU->RenderedLength;
	mo->current_frame = CU->data + CU->RenderedLength;
	mo->current_ts = CU->TS;
	/*also adjust CU time based on consummed bytes in input, since some codecs output very large audio chunks*/
	if (mo->odm->codec->bytes_per_sec) mo->current_ts += CU->RenderedLength * 1000 / mo->odm->codec->bytes_per_sec;

	mo->num_fetched++;
	ret = 1;

/*
	obj_time = gf_clock_time(mo->odm->codec->ck);
	fprintf(stdout, "At OTB %d fetch frame TS %d size %d - %d unit in CB\n", obj_time, mo->current_ts, mo->current_size, mo->odm->codec->CB->UnitCount);
*/
	
exit:

	gf_cm_lock(mo->odm->codec->CB, 0);
	return ret;
}

void gf_mo_release_data(GF_MediaObject *mo, u32 nb_bytes, u32 forceDrop)
{
	u32 obj_time;
	if (!mo || !mo->num_fetched) return;
	assert(mo->odm);
	mo->num_fetched--;
	if (mo->num_fetched) return;

	gf_cm_lock(mo->odm->codec->CB, 1);

	/*perform a sanity check on TS since the CB may have changed status - this may happen in 
	temporal scalability only*/
	if (mo->odm->codec->CB->output->dataLength ) {
		assert(mo->odm->codec->CB->output->RenderedLength + nb_bytes <= mo->odm->codec->CB->output->dataLength);
		mo->odm->codec->CB->output->RenderedLength += nb_bytes;

		/*discard frame*/
		if (mo->odm->codec->CB->output->RenderedLength == mo->odm->codec->CB->output->dataLength) {
			if (forceDrop) {
				gf_cm_drop_output(mo->odm->codec->CB);
				forceDrop--;
//				if (forceDrop) mo->odm->codec->nb_droped++;
			} else {
				obj_time = gf_clock_time(mo->odm->codec->ck);
				if (mo->odm->codec->CB->output->next->dataLength) { 
					if (2*obj_time < mo->current_ts + mo->odm->codec->CB->output->next->TS ) {
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
	gf_cm_lock(mo->odm->codec->CB, 0);
}

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


void gf_mo_play(GF_MediaObject *mo, Double media_offset, Bool can_loop)
{
	if (!mo) return;

	gf_term_lock_net(mo->term, 1);
	if (!mo->num_open && mo->odm) {
		if (mo->odm->no_time_ctrl) {
			mo->odm->media_start_time = 0;
		} else {
			mo->odm->media_start_time = (u32) (media_offset*1000);
			if (mo->odm->duration && (mo->odm->media_start_time > mo->odm->duration)) {
				if (can_loop) {
					mo->odm->media_start_time %= (u32) mo->odm->duration;
				} else {
					mo->odm->media_start_time = (u32) mo->odm->duration;
				}
			}
		}
		gf_odm_start(mo->odm);
	} else {
		if (mo->num_to_restart) mo->num_restart--;
		if (!mo->num_restart && (mo->num_to_restart==mo->num_open+1) ) {
			MC_Restart(mo->odm);
			mo->num_to_restart = mo->num_restart = 0;
		}
	}

	mo->num_open++;
	gf_term_lock_net(mo->term, 0);
}

void gf_mo_stop(GF_MediaObject *mo)
{
	if (!mo) return;
	assert(mo->num_open);
	mo->num_open--;
	if (!mo->num_open && mo->odm) {
		/*do not stop directly, this can delete channel data currently being decoded (BIFS anim & co)*/
		if (gf_list_find(mo->odm->term->od_pending, mo->odm)<0) {
			mo->odm->media_start_time = -1;
			gf_list_add(mo->odm->term->od_pending, mo->odm);
		}
	} else {
		if (!mo->num_to_restart) {
			mo->num_restart = mo->num_to_restart = mo->num_open + 1;
		}
	}
}

void gf_mo_restart(GF_MediaObject *mo)
{
	MediaControlStack *ctrl;
	if (!mo) return;
	assert(mo->num_open);
	/*this should not happen (inline scene are restarted internally)*/
	assert(!mo->odm->subscene);

	ctrl = ODM_GetMediaControl(mo->odm);
	
	if (!ctrl) {
		GF_Clock *ck = gf_odm_get_media_clock(mo->odm->parentscene->root_od);
		/*don't restart if sharing parent scene clock*/
		if (gf_odm_shares_clock(mo->odm, ck)) return;
	}
	/*all other cases, call restart to take into account clock references*/
	MC_Restart(mo->odm);
}

Bool gf_mo_url_changed(GF_MediaObject *mo, MFURL *url)
{
	u32 od_id;
	Bool ret = 0;
	if (!mo) return (url ? 1 : 0);
	od_id = URL_GetODID(url);
	if ( (mo->OD_ID == GF_ESM_DYNAMIC_OD_ID) && (od_id == GF_ESM_DYNAMIC_OD_ID)) {
		ret = !gf_is_same_url(&mo->URLs, url);
	} else {
		ret = (mo->OD_ID == od_id) ? 0 : 1;
	}
	/*special case for 3GPP text: if not playing and user node changed, force removing it*/
	if (ret && mo->odm && !mo->num_open && (mo->type == GF_MEDIA_OBJECT_TEXT)) {
		mo->mo_flags |= GF_MO_DISPLAY_REMOVE;
		gf_mm_stop_codec(mo->odm->codec);
	}
	return ret;
}


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

Fixed gf_mo_get_speed(GF_MediaObject *mo, Fixed in_speed)
{
	MediaControlStack *ctrl;
	if (!mo || !mo->odm) return in_speed;
	/*get control*/
	ctrl = ODM_GetMediaControl(mo->odm);
	return ctrl ? ctrl->control->mediaSpeed : in_speed;
}

Bool gf_mo_get_loop(GF_MediaObject *mo, Bool in_loop)
{
	GF_Clock *ck;
	MediaControlStack *ctrl;
	if (!mo || !mo->odm) return in_loop;
	
	/*get control*/
	ctrl = ODM_GetMediaControl(mo->odm);
	if (ctrl) in_loop = ctrl->control->loop;

	/*otherwise looping is only accepted if not sharing parent scene clock*/
	ck = gf_odm_get_media_clock(mo->odm->parentscene->root_od);
	if (gf_odm_shares_clock(mo->odm, ck)) return 0;
	return in_loop;
}

Double gf_mo_get_duration(GF_MediaObject *mo)
{
	if (!mo || !mo->odm) return -1.0;
	return ((Double) (s64)mo->odm->duration)/1000.0;
}

Bool gf_mo_should_deactivate(GF_MediaObject *mo)
{
	MediaControlStack *ctrl;
	
	if (!mo || !mo->odm) return 0;
	/*not running*/
	if (!mo->odm->is_open) return 0;

	/*get media control and see if object owning control is running*/
	ctrl = ODM_GetMediaControl(mo->odm);
	if (!ctrl) return 1;
	/*if ctrl and ctrl not ruling this mediaObject, deny deactivation*/
	if (ctrl->stream->odm != mo->odm) return 0;
	/*this is currently under discussion in MPEG. for now we deny deactivation as soon as a mediaControl is here*/
	if (ctrl->stream->odm->is_open) return 0;
	/*otherwise allow*/	
	return 1;
}

Bool gf_mo_is_muted(GF_MediaObject *mo)
{
	if (!mo->odm) return 1;
	return mo->odm->media_ctrl ? mo->odm->media_ctrl->control->mute : 0;
}

Bool gf_mo_is_done(GF_MediaObject *mo)
{
	GF_Codec *codec;
	u64 dur;
	if (!mo || !mo->odm) return 0;

	/*for natural media use composition buffer*/
	if (mo->odm->codec && mo->odm->codec->CB) return (mo->odm->codec->CB->Status==CB_STOP) ? 1 : 0;

	/*otherwise check EOS and time*/
	codec = mo->odm->codec;
	dur = mo->odm->duration;
	if (!mo->odm->codec) {
		if (!mo->odm->subscene) return 0;
		codec = mo->odm->subscene->scene_codec;
		dur = mo->odm->subscene->duration;
	}
	if (codec->Status==GF_ESM_CODEC_STOP) {
		/*codec is done, check by duration*/
		GF_Clock *ck = gf_odm_get_media_clock(mo->odm);
		if (gf_clock_time(ck) > dur) return 1;
	}
	return 0;
}

/*resyncs clock - only audio objects are allowed to use this*/
void gf_mo_adjust_clock(GF_MediaObject *mo, s32 ms_drift)
{
	if (!mo || !mo->odm) return;
	if (!mo->odm->codec || (mo->odm->codec->type != GF_STREAM_AUDIO) ) return;
	gf_clock_adjust_drift(mo->odm->codec->ck, ms_drift);
}

