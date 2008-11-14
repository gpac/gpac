/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#include <gpac/internal/compositor_dev.h>

#define MIN_RESYNC_TIME		500

static char *gf_audio_input_fetch_frame(void *callback, u32 *size, u32 audio_delay_ms)
{
	char *frame;
	u32 obj_time, ts;
	s32 drift;
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	/*even if the stream is signaled as finished we must check it, because it may have been restarted by a mediaControl*/
	if (!ai->stream) return NULL;
	
	frame = gf_mo_fetch_data(ai->stream, 0, &ai->stream_finished, &ts, size);
	/*invalidate scene on end of stream to refresh audio graph*/
	if (ai->stream_finished) gf_sc_invalidate(ai->compositor, NULL);

	/*no more data or not enough data, reset syncro drift*/
	if (!frame) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Audio Input] No data in audio object (eos %d)\n", ai->stream_finished));
		gf_mo_adjust_clock(ai->stream, 0);
		return NULL;
	}
	ai->need_release = 1;
	
	
	gf_mo_get_object_time(ai->stream, &obj_time);
	obj_time += audio_delay_ms;
	drift = (s32)obj_time;
	drift -= (s32)ts;

	/*too early (silence insertions), skip*/
	if (drift + (s32) (audio_delay_ms + ai->is_open) < 0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Audio Input] audio too early %d (CTS %d)\n", drift + audio_delay_ms + MIN_RESYNC_TIME, ts));
		ai->need_release = 0;
		gf_mo_release_data(ai->stream, 0, 0);
		return NULL;
	}
	/*adjust drift*/
	if (audio_delay_ms) {
		/*CU is way too late, discard and fetch a new one - this usually happen when media speed is more than 1*/
		if (drift>500) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Audio Input] Audio data too late (drift %d ms) - resync forced\n", drift));
			gf_mo_release_data(ai->stream, *size, 2);
			ai->need_release = 0;
			return gf_audio_input_fetch_frame(callback, size, audio_delay_ms);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Audio Input] Audio clock: delay %d - obj time %d - CTS %d - adjust drift %d\n", audio_delay_ms, obj_time - audio_delay_ms, ts, drift));
		gf_mo_adjust_clock(ai->stream, drift);
	}
	return frame;
}

static void gf_audio_input_release_frame(void *callback, u32 nb_bytes)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return;
	gf_mo_release_data(ai->stream, nb_bytes, 1);
	ai->need_release = 0;
	/*as soon as we have released a frame for this audio stream, update resynbc tolerance*/
	ai->is_open = MIN_RESYNC_TIME;
}

static Fixed gf_audio_input_get_speed(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	return gf_mo_get_speed(ai->stream, ai->speed);
}

static Bool gf_audio_input_get_volume(void *callback, Fixed *vol)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (ai->snd && ai->snd->GetChannelVolume) {
		return ai->snd->GetChannelVolume(ai->snd->owner, vol);
	} else {
		vol[0] = vol[1] = vol[2] = vol[3] = vol[4] = vol[5] = ai->intensity;
		return (ai->intensity==FIX_ONE) ? 0 : 1;
	}
}

static Bool gf_audio_input_is_muted(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return 1;
	if (ai->is_muted) 
		return 1;
	return gf_mo_is_muted(ai->stream);
}

static Bool gf_audio_input_get_config(GF_AudioInterface *aifc, Bool for_recf)
{
	GF_AudioInput *ai = (GF_AudioInput *) aifc->callback;
	if (!ai->stream) return 0;
	/*watchout for object reuse*/
	if (aifc->samplerate && (gf_mo_get_flags(ai->stream) & GF_MO_IS_INIT)) return 1;
	if (!for_recf) 
		return 0;

	gf_mo_get_audio_info(ai->stream, &aifc->samplerate, &aifc->bps , &aifc->chan, &aifc->ch_cfg);

	if (aifc->samplerate * aifc->chan * aifc->bps && ((aifc->chan<=2) || aifc->ch_cfg))  {
		gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 1);
		return 1;
	}
	gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 0);
	return 0;
}

GF_EXPORT
void gf_sc_audio_setup(GF_AudioInput *ai, GF_Compositor *compositor, GF_Node *node)
{
	memset(ai, 0, sizeof(GF_AudioInput));
	ai->owner = node;
	ai->compositor = compositor;
	ai->stream = NULL;
	/*setup io interface*/
	ai->input_ifce.FetchFrame = gf_audio_input_fetch_frame;
	ai->input_ifce.ReleaseFrame = gf_audio_input_release_frame;
	ai->input_ifce.GetConfig = gf_audio_input_get_config;
	ai->input_ifce.GetChannelVolume = gf_audio_input_get_volume;
	ai->input_ifce.GetSpeed = gf_audio_input_get_speed;
	ai->input_ifce.IsMuted = gf_audio_input_is_muted;
	ai->input_ifce.callback = ai;
	ai->intensity = FIX_ONE;

	ai->speed = FIX_ONE;
}



GF_EXPORT
GF_Err gf_sc_audio_open(GF_AudioInput *ai, MFURL *url, Double clipBegin, Double clipEnd)
{
	if (ai->is_open) return GF_BAD_PARAM;

	/*get media object*/
	ai->stream = gf_mo_register(ai->owner, url, 0);
	/*bad URL*/
	if (!ai->stream) return GF_NOT_SUPPORTED;

	/*store url*/
	gf_sg_vrml_field_copy(&ai->url, url, GF_SG_VRML_MFURL);

	/*request play*/
	gf_mo_play(ai->stream, clipBegin, clipEnd, 0);

	ai->stream_finished = 0;
	ai->is_open = 1;
	gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 0);
	return GF_OK;
}

GF_EXPORT
void gf_sc_audio_stop(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	
	/*we must make sure audio mixer is not using the stream otherwise we may leave it dirty (with unrelease frame)*/
	gf_mixer_lock(ai->compositor->audio_renderer->mixer, 1);

	assert(!ai->need_release);

	gf_mo_stop(ai->stream);
	gf_sg_vrml_mf_reset(&ai->url, GF_SG_VRML_MFURL);
	ai->is_open = 0;
	gf_mo_unregister(ai->owner, ai->stream);
	ai->stream = NULL;

	gf_mixer_lock(ai->compositor->audio_renderer->mixer, 0);

}

GF_EXPORT
void gf_sc_audio_restart(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	if (ai->need_release) gf_mo_release_data(ai->stream, 0xFFFFFFFF, 2);
	ai->need_release = 0;
	ai->stream_finished = 0;
	gf_mo_restart(ai->stream);
}

GF_EXPORT
Bool gf_sc_audio_check_url(GF_AudioInput *ai, MFURL *url)
{
	if (!ai->stream) return url->count;
	return gf_mo_url_changed(ai->stream, url);
}

GF_EXPORT
void gf_sc_audio_register(GF_AudioInput *ai, GF_TraverseState *tr_state)
{
	/*check interface is valid*/
	if (!ai->input_ifce.FetchFrame
		|| !ai->input_ifce.GetChannelVolume
		|| !ai->input_ifce.GetConfig
		|| !ai->input_ifce.GetSpeed
		|| !ai->input_ifce.IsMuted
		|| !ai->input_ifce.ReleaseFrame
		) return;

	if (tr_state->audio_parent) {
		/*this assume only one parent may use an audio node*/
		if (ai->register_with_parent) return;
		if (ai->register_with_renderer) {
			gf_sc_ar_remove_src(ai->compositor->audio_renderer, &ai->input_ifce);
			ai->register_with_renderer = 0;
		}
		tr_state->audio_parent->add_source(tr_state->audio_parent, ai);
		ai->register_with_parent = 1;
		ai->snd = tr_state->sound_holder;
	} else if (!ai->register_with_renderer) {
		
		if (ai->register_with_parent) {
			ai->register_with_parent = 0;
			/*if used in a parent audio group, do a complete traverse to rebuild the group*/
			gf_sc_invalidate(ai->compositor, NULL);
		}

		gf_sc_ar_add_src(ai->compositor->audio_renderer, &ai->input_ifce);
		ai->register_with_renderer = 1;
		ai->snd = tr_state->sound_holder;
	}
}

GF_EXPORT
void gf_sc_audio_unregister(GF_AudioInput *ai)
{
	if (ai->register_with_renderer) {
		ai->register_with_renderer = 0;
		gf_sc_ar_remove_src(ai->compositor->audio_renderer, &ai->input_ifce);
	} else {
		/*if used in a parent audio group, do a complete traverse to rebuild the group*/
		gf_sc_invalidate(ai->compositor, NULL);
	}
}


