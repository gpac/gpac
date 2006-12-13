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

#include <gpac/internal/renderer_dev.h>

#define MIN_RESYNC_TIME		1000

static char *AI_FetchFrame(void *callback, u32 *size, u32 audio_delay_ms)
{
	char *frame;
	u32 obj_time, ts;
	s32 drift;
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	/*even if the stream is signaled as finished we must check it, because it may have been restarted by a mediaControl*/
	if (!ai->stream) return NULL;
	
	frame = gf_mo_fetch_data(ai->stream, 0, &ai->stream_finished, &ts, size);
	/*invalidate scene on end of stream to refresh audio graph*/
	if (ai->stream_finished) gf_sr_invalidate(ai->compositor, NULL);

	/*no more data or not enough data, reset syncro drift*/
	if (!frame) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Audio Render] No data in audio aobject (eos %d)\n", ai->stream_finished));
		gf_mo_adjust_clock(ai->stream, 0);
		return NULL;
	}
	ai->need_release = 1;
	
	gf_mo_get_object_time(ai->stream, &obj_time);
	obj_time += audio_delay_ms;
	drift = obj_time - ts;

	/*too early (silence insertions), don't render*/
	if (drift + (s32) audio_delay_ms + MIN_RESYNC_TIME < 0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Audio Render] audio too early %d (CTS %d)\n", drift + audio_delay_ms + MIN_RESYNC_TIME, ts));
		ai->need_release = 0;
		gf_mo_release_data(ai->stream, 0, 0);
		return NULL;
	}
	/*adjust drift*/
	if (audio_delay_ms) {
		/*CU is way too late, discard and fetch a new one - this usually happen when media speed is more than 1*/
		if (drift>500) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Audio Render] Audio data too late (drift %d ms) - resync forced\n", drift));
			gf_mo_release_data(ai->stream, *size, 2);
			ai->need_release = 0;
			return AI_FetchFrame(callback, size, audio_delay_ms);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RENDER, ("[Audio Render] Audio clock: delay %d - obj time %d - CTS %d - adjust drift %d\n", audio_delay_ms, obj_time - audio_delay_ms, ts, drift));
		gf_mo_adjust_clock(ai->stream, drift);
	}
	return frame;
}

static void AI_ReleaseFrame(void *callback, u32 nb_bytes)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return;
	gf_mo_release_data(ai->stream, nb_bytes, 1);
	ai->need_release = 0;
}

static Fixed AI_GetSpeed(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	return gf_mo_get_speed(ai->stream, ai->speed);
}

static Bool AI_GetChannelVolume(void *callback, Fixed *vol)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (ai->snd && ai->snd->GetChannelVolume) {
		return ai->snd->GetChannelVolume(ai->snd->owner, vol);
	} else {
		vol[0] = vol[1] = vol[2] = vol[3] = vol[4] = vol[5] = FIX_ONE;
		return 0;
	}
}

static Bool AI_IsMuted(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return 1;
	if (ai->is_muted) return 1;
	return gf_mo_is_muted(ai->stream);
}

static Bool AI_GetConfig(GF_AudioInterface *aifc, Bool for_recf)
{
	GF_AudioInput *ai = (GF_AudioInput *) aifc->callback;
	if (!ai->stream) return 0;
	/*watchout for object reuse*/
	if (aifc->sr && (gf_mo_get_flags(ai->stream) & GF_MO_IS_INIT)) return 1;
	if (!for_recf) 
		return 0;

	gf_mo_get_audio_info(ai->stream, &aifc->sr, &aifc->bps , &aifc->chan, &aifc->ch_cfg);

	if (aifc->sr * aifc->chan * aifc->bps && ((aifc->chan<=2) || aifc->ch_cfg))  {
		gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 1);
		return 1;
	}
	gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 0);
	return 0;
}

GF_EXPORT
void gf_sr_audio_setup(GF_AudioInput *ai, GF_Renderer *sr, GF_Node *node)
{
	memset(ai, 0, sizeof(GF_AudioInput));
	ai->owner = node;
	ai->compositor = sr;
	ai->stream = NULL;
	/*setup io interface*/
	ai->input_ifce.FetchFrame = AI_FetchFrame;
	ai->input_ifce.ReleaseFrame = AI_ReleaseFrame;
	ai->input_ifce.GetConfig = AI_GetConfig;
	ai->input_ifce.GetChannelVolume = AI_GetChannelVolume;
	ai->input_ifce.GetSpeed = AI_GetSpeed;
	ai->input_ifce.IsMuted = AI_IsMuted;
	ai->input_ifce.callback = ai;

	ai->speed = FIX_ONE;
}



GF_EXPORT
GF_Err gf_sr_audio_open(GF_AudioInput *ai, MFURL *url)
{
	if (ai->is_open) return GF_BAD_PARAM;

	/*get media object*/
	ai->stream = gf_mo_find(ai->owner, url, 0);
	/*bad URL*/
	if (!ai->stream) return GF_NOT_SUPPORTED;

	/*store url*/
	gf_sg_vrml_field_copy(&ai->url, url, GF_SG_VRML_MFURL);

	/*request play*/
	gf_mo_play(ai->stream, 0, 0);

	ai->stream_finished = 0;
	ai->is_open = 1;
	gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 0);
	return GF_OK;
}

GF_EXPORT
void gf_sr_audio_stop(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	
	/*we must make sure audio mixer is not using the stream otherwise we may leave it dirty (with unrelease frame)*/
	gf_mixer_lock(ai->compositor->audio_renderer->mixer, 1);

	assert(!ai->need_release);

	gf_mo_stop(ai->stream);
	gf_sg_vrml_mf_reset(&ai->url, GF_SG_VRML_MFURL);
	ai->is_open = 0;
	ai->stream = NULL;

	gf_mixer_lock(ai->compositor->audio_renderer->mixer, 0);

}

GF_EXPORT
void gf_sr_audio_restart(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	if (ai->need_release) gf_mo_release_data(ai->stream, 0xFFFFFFFF, 2);
	ai->need_release = 0;
	ai->stream_finished = 0;
	gf_mo_restart(ai->stream);
}

GF_EXPORT
Bool gf_sr_audio_check_url(GF_AudioInput *ai, MFURL *url)
{
	if (!ai->stream) return url->count;
	return gf_mo_url_changed(ai->stream, url);
}

GF_EXPORT
void gf_sr_audio_register(GF_AudioInput *ai, GF_BaseEffect *eff)
{
	/*check interface is valid*/
	if (!ai->input_ifce.FetchFrame
		|| !ai->input_ifce.GetChannelVolume
		|| !ai->input_ifce.GetConfig
		|| !ai->input_ifce.GetSpeed
		|| !ai->input_ifce.IsMuted
		|| !ai->input_ifce.ReleaseFrame
		) return;

	if (eff->audio_parent) {
		/*this assume only one parent may use an audio node*/
		if (ai->register_with_parent) return;
		if (ai->register_with_renderer) {
			gf_sr_ar_remove_src(ai->compositor->audio_renderer, &ai->input_ifce);
			ai->register_with_renderer = 0;
		}
		eff->audio_parent->add_source(eff->audio_parent, ai);
		ai->register_with_parent = 1;
		ai->snd = eff->sound_holder;
	} else if (!ai->register_with_renderer) {
		
		if (ai->register_with_parent) {
			ai->register_with_parent = 0;
			/*if used in a parent audio group, do a complete traverse to rebuild the group*/
			gf_sr_invalidate(ai->compositor, NULL);
		}

		gf_sr_ar_add_src(ai->compositor->audio_renderer, &ai->input_ifce);
		ai->register_with_renderer = 1;
		ai->snd = eff->sound_holder;
	}
}

GF_EXPORT
void gf_sr_audio_unregister(GF_AudioInput *ai)
{
	if (ai->register_with_renderer) {
		ai->register_with_renderer = 0;
		gf_sr_ar_remove_src(ai->compositor->audio_renderer, &ai->input_ifce);
	} else {
		/*if used in a parent audio group, do a complete traverse to rebuild the group*/
		gf_sr_invalidate(ai->compositor, NULL);
	}
}


