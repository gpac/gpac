/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

#define ENABLE_EARLY_FRAME_DETECTION

/*diff time in ms to consider an audio frame too late and drop it - we should try to dynamically figure this out
since the drift may be high on TS for example, where PTS-PCR>500ms is quite common*/
#define MAX_RESYNC_TIME		1000
//if drift between audio object time and clock varies more is than this value (in ms) between two drift computation, clock is adjusted. We don't adjust for lower values otherwise we would
//introduce oscillations in the clock and non-smooth playback
#define MIN_DRIFT_ADJUST	75


static u8 *gf_audio_input_fetch_frame(void *callback, u32 *size, u32 *planar_size, u32 audio_delay_ms)
{
	char *frame;
	u32 obj_time, ts;
	s32 drift;
	Fixed speed;
	Bool done;
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	/*even if the stream is signaled as finished we must check it, because it may have been restarted by a mediaControl*/
	if (!ai->stream) return NULL;

	done = ai->stream_finished;
	ai->input_ifce.is_buffering = GF_FALSE;

	frame = gf_mo_fetch_data(ai->stream, ai->compositor->audio_renderer->non_rt_output ? GF_MO_FETCH_PAUSED : GF_MO_FETCH, 0, &ai->stream_finished, &ts, size, NULL, NULL, NULL, planar_size);
	/*invalidate scene on end of stream to refresh audio graph*/
	if (done != ai->stream_finished) {
		gf_sc_invalidate(ai->compositor, NULL);
	}

	/*no more data or not enough data, reset syncro drift*/
	if (!frame) {
		if (!ai->stream_finished && gf_mo_is_started(ai->stream)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_AUDIO, ("[Audio Input] No data in audio object\n"));
		}
		gf_mo_adjust_clock(ai->stream, 0);
		ai->input_ifce.is_buffering = gf_mo_is_buffering(ai->stream);
		*size = 0;
		return NULL;
	}
	ai->need_release = GF_TRUE;

	//step mode, return the frame without sync check
	if (ai->compositor->audio_renderer->non_rt_output) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Audio Input] audio frame CTS %u %d bytes fetched\n", ts, *size));
		return frame;
	}

	speed = gf_mo_get_current_speed(ai->stream);

	gf_mo_get_object_time(ai->stream, &obj_time);
	obj_time += audio_delay_ms;
	if (ai->compositor->bench_mode) {
		drift = 0;
	} else {
		drift = (s32)obj_time;
		drift -= (s32)ts;
	}
	if (ai->stream->odm->prev_clock_at_discontinuity_plus_one) {
		s32 drift_old = drift;
		s32 diff;
		drift_old -= (s32) ai->stream->odm->ck->init_timestamp;
		drift_old += (s32) ai->stream->odm->prev_clock_at_discontinuity_plus_one - 1;
		diff = ABS(drift_old);
		diff -= ABS(drift);
		if (diff < 0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[Audio Input] in clock discontinuity: drift old clock %d new clock %d - disabling clock adjustment\n", drift_old, drift));
			drift = 0;
			audio_delay_ms = 0;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[Audio Input] end of clock discontinuity: drift old clock %d new clock %d\n", drift_old, drift));
			ai->stream->odm->prev_clock_at_discontinuity_plus_one = 0;
			if (drift<0) {
				drift = 0;
			}
		}
	}

#ifdef ENABLE_EARLY_FRAME_DETECTION
	/*too early (silence insertions), skip*/
	if (drift < 0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Audio Input] audio too early of %d (CTS %u at OTB %u with audio delay %d ms)\n", drift + audio_delay_ms, ts, obj_time, audio_delay_ms));
		ai->need_release = GF_FALSE;
		gf_mo_release_data(ai->stream, 0, -1);
		*size = 0;
		return NULL;
	}
#endif
	/*adjust drift*/
	if (audio_delay_ms) {
		s32 resync_delay = speed > 0 ? FIX2INT(speed * MAX_RESYNC_TIME) : FIX2INT(-speed * MAX_RESYNC_TIME);
		/*CU is way too late, discard and fetch a new one - this usually happen when media speed is more than 1*/
		if (drift>resync_delay) {
			GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Audio Input] Audio data too late obj time %d - CTS %d - drift %d ms - resync forced\n", obj_time - audio_delay_ms, ts, drift));
			gf_mo_release_data(ai->stream, *size, 2);
			ai->need_release = GF_FALSE;
			return gf_audio_input_fetch_frame(callback, size, planar_size, audio_delay_ms);
		}
		if (ai->stream->odm && ai->stream->odm->ck)
			resync_delay = ai->stream->odm->ck->drift - drift;
		else
			resync_delay = -drift;
			
		if (resync_delay < 0) resync_delay = -resync_delay;

		if (resync_delay > MIN_DRIFT_ADJUST) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Audio Input] Audio clock: delay %d - obj time %d - audio delay %d - CTS %d - adjust drift %d\n", audio_delay_ms, obj_time, audio_delay_ms, ts, drift));
			gf_mo_adjust_clock(ai->stream, drift);
		}
	}
	return frame;
}

static void gf_audio_input_release_frame(void *callback, u32 nb_bytes)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return;
	gf_mo_release_data(ai->stream, nb_bytes, 1);
	ai->need_release = GF_FALSE;
}

static Fixed gf_audio_input_get_speed(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	return gf_mo_get_current_speed(ai->stream);
}

static Bool gf_audio_input_get_volume(void *callback, Fixed *vol)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (ai->snd && ai->snd->GetChannelVolume) {
		return ai->snd->GetChannelVolume(ai->snd->owner, vol);
	} else {
		u32 i;
		for (i=0; i<GF_AUDIO_MIXER_MAX_CHANNELS; i++)
			vol[i] = ai->intensity;
			
		return (ai->intensity==FIX_ONE) ? GF_FALSE : GF_TRUE;
	}
}

static Bool gf_audio_input_is_muted(void *callback)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (!ai->stream) return GF_TRUE;

	if (ai->stream->odm->nb_buffering)
		gf_odm_check_buffering(ai->stream->odm, NULL);
	if (ai->is_muted)
		return GF_TRUE;
	return gf_mo_is_muted(ai->stream);
}

static Bool gf_audio_input_get_config(GF_AudioInterface *aifc, Bool for_recf)
{
	GF_AudioInput *ai = (GF_AudioInput *) aifc->callback;
	if (!ai->stream) return GF_FALSE;
	/*watchout for object reuse*/
	if (aifc->samplerate &&  !ai->stream->config_changed) return GF_TRUE;

	gf_mo_get_audio_info(ai->stream, &aifc->samplerate, &aifc->afmt , &aifc->chan, &aifc->ch_cfg, &aifc->forced_layout);

	if (!for_recf)
		return aifc->samplerate ? GF_TRUE : GF_FALSE;

	if (aifc->samplerate && aifc->chan && aifc->afmt && ((aifc->chan<=2) || aifc->ch_cfg))  {
		ai->stream->config_changed = GF_FALSE;
		return GF_TRUE;
	}
	//still not ready !
	ai->stream->config_changed=GF_TRUE;
	return GF_FALSE;
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

void gf_sc_audio_predestroy(GF_AudioInput *ai)
{
	gf_sc_audio_stop(ai);
	gf_sc_audio_unregister(ai);
}

GF_EXPORT
GF_Err gf_sc_audio_open(GF_AudioInput *ai, MFURL *url, Double clipBegin, Double clipEnd, Bool lock_timeline)
{
	if (ai->is_open) return GF_BAD_PARAM;

	/*get media object*/
	ai->stream = gf_mo_register(ai->owner, url, lock_timeline, GF_FALSE);
	/*bad URL*/
	if (!ai->stream) return GF_NOT_SUPPORTED;

	/*request play*/
	gf_mo_play(ai->stream, clipBegin, clipEnd, GF_FALSE);

	ai->stream_finished = GF_FALSE;
	ai->is_open = 1;
	//force reload of audio props
	ai->stream->config_changed = GF_TRUE;

	return GF_OK;
}

GF_EXPORT
void gf_sc_audio_stop(GF_AudioInput *ai)
{
	if (!ai->is_open) return;

	/*we must make sure audio mixer is not using the stream otherwise we may leave it dirty (with unrelease frame)*/
	gf_mixer_lock(ai->compositor->audio_renderer->mixer, GF_TRUE);

	assert(!ai->need_release);

	gf_mo_stop(&ai->stream);
	ai->is_open = 0;
	gf_mo_unregister(ai->owner, ai->stream);
	ai->stream = NULL;

	gf_mixer_lock(ai->compositor->audio_renderer->mixer, GF_FALSE);

}

GF_EXPORT
void gf_sc_audio_restart(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	if (ai->need_release) gf_mo_release_data(ai->stream, 0xFFFFFFFF, 2);
	ai->need_release = GF_FALSE;
	ai->stream_finished = GF_FALSE;

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
	GF_AudioInterface *aifce;
	/*check interface is valid*/
	if (!ai->input_ifce.FetchFrame
	        || !ai->input_ifce.GetChannelVolume
	        || !ai->input_ifce.GetConfig
	        || !ai->input_ifce.GetSpeed
	        || !ai->input_ifce.IsMuted
	        || !ai->input_ifce.ReleaseFrame
	   ) return;

	aifce = &ai->input_ifce;

	if (tr_state->audio_parent) {
		/*this assume only one parent may use an audio node*/
		if (ai->register_with_parent) return;
		if (ai->register_with_renderer) {
			gf_sc_ar_remove_src(ai->compositor->audio_renderer, aifce);
			ai->register_with_renderer = GF_FALSE;
		}
		tr_state->audio_parent->add_source(tr_state->audio_parent, ai);
		ai->register_with_parent = GF_TRUE;
		ai->snd = tr_state->sound_holder;
	} else if (!ai->register_with_renderer) {

		if (ai->register_with_parent) {
			ai->register_with_parent = GF_FALSE;
			/*if used in a parent audio group, do a complete traverse to rebuild the group*/
			gf_sc_invalidate(ai->compositor, NULL);
		}

		gf_sc_ar_add_src(ai->compositor->audio_renderer, aifce);
		ai->register_with_renderer = GF_TRUE;
		ai->snd = tr_state->sound_holder;
	}
}

GF_EXPORT
void gf_sc_audio_unregister(GF_AudioInput *ai)
{
	GF_AudioInterface *aifce = &ai->input_ifce;

	if (ai->register_with_renderer) {
		ai->register_with_renderer = GF_FALSE;
		gf_sc_ar_remove_src(ai->compositor->audio_renderer, aifce);
	} else {
		/*if used in a parent audio group, do a complete traverse to rebuild the group*/
		gf_sc_invalidate(ai->compositor, NULL);
	}
}

