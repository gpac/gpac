/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

struct __audiofilteritem
{
	GF_AudioInterface input;
	GF_AudioInterface *src;

	u32 out_chan, out_ch_cfg;

	u32 nb_used, nb_filled;

	GF_AudioFilterChain filter_chain;
};


GF_AudioFilterItem *gf_af_new(GF_Compositor *compositor, GF_AudioInterface *src, char *filter_name);
void gf_af_del(GF_AudioFilterItem *af);
void gf_af_reset(GF_AudioFilterItem *af);


static char *gf_audio_input_fetch_frame(void *callback, u32 *size, u32 audio_delay_ms)
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
	frame = gf_mo_fetch_data(ai->stream, ai->compositor->audio_renderer->step_mode ? GF_MO_FETCH_PAUSED : GF_MO_FETCH, &ai->stream_finished, &ts, size, NULL, NULL);
	/*invalidate scene on end of stream to refresh audio graph*/
	if (done != ai->stream_finished) {
		gf_sc_invalidate(ai->compositor, NULL);
	}

	/*no more data or not enough data, reset syncro drift*/
	if (!frame) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Audio Input] No data in audio object (eos %d)\n", ai->stream_finished));
		gf_mo_adjust_clock(ai->stream, 0);
		*size = 0;
		return NULL;
	}
	ai->need_release = 1;

	//step mode, return the frame without sync check
	if (ai->compositor->audio_renderer->step_mode) 
		return frame;

	speed = gf_mo_get_current_speed(ai->stream);

	gf_mo_get_object_time(ai->stream, &obj_time);
	obj_time += audio_delay_ms;
	if (ai->compositor->bench_mode) {
		drift = 0;
	} else {
		drift = (s32)obj_time;
		drift -= (s32)ts;
	}

#ifdef ENABLE_EARLY_FRAME_DETECTION
	/*too early (silence insertions), skip*/
	if (drift < 0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_AUDIO, ("[Audio Input] audio too early of %d (CTS %u at OTB %u with audio delay %d ms)\n", drift + audio_delay_ms, ts, obj_time, audio_delay_ms));
		ai->need_release = 0;
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
			ai->need_release = 0;
			return gf_audio_input_fetch_frame(callback, size, audio_delay_ms);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_AUDIO, ("[Audio Input] Audio clock: delay %d - obj time %d - CTS %d - adjust drift %d\n", audio_delay_ms, obj_time - audio_delay_ms, ts, drift));
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

void gf_sc_audio_predestroy(GF_AudioInput *ai)
{
	gf_sc_audio_stop(ai);
	gf_sc_audio_unregister(ai);

	if (ai->filter) gf_af_del(ai->filter);
}

GF_EXPORT
GF_Err gf_sc_audio_open(GF_AudioInput *ai, MFURL *url, Double clipBegin, Double clipEnd, Bool lock_timeline)
{
	u32 i;
	if (ai->is_open) return GF_BAD_PARAM;

	/*get media object*/
	ai->stream = gf_mo_register(ai->owner, url, lock_timeline, 0);
	/*bad URL*/
	if (!ai->stream) return GF_NOT_SUPPORTED;

	/*request play*/
	gf_mo_play(ai->stream, clipBegin, clipEnd, 0);

	ai->stream_finished = 0;
	ai->is_open = 1;
	gf_mo_set_flag(ai->stream, GF_MO_IS_INIT, 0);

	if (ai->filter) gf_af_del(ai->filter);
	ai->filter = NULL;

	for (i=0; i<url->count; i++) {
		if (url->vals[i].url && !strnicmp(url->vals[i].url, "#filter=", 8)) {
			ai->filter = gf_af_new(ai->compositor, &ai->input_ifce, url->vals[i].url+8);
			if (ai->filter)
				break;
		}
	}
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
	ai->is_open = 0;
	gf_mo_unregister(ai->owner, ai->stream);
	ai->stream = NULL;

	if (ai->filter) gf_af_del(ai->filter);
	ai->filter = NULL;

	gf_mixer_lock(ai->compositor->audio_renderer->mixer, 0);

}

GF_EXPORT
void gf_sc_audio_restart(GF_AudioInput *ai)
{
	if (!ai->is_open) return;
	if (ai->need_release) gf_mo_release_data(ai->stream, 0xFFFFFFFF, 2);
	ai->need_release = 0;
	ai->stream_finished = 0;
	if (ai->filter) gf_af_reset(ai->filter);
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
	if (ai->filter) aifce = &ai->filter->input;

	if (tr_state->audio_parent) {
		/*this assume only one parent may use an audio node*/
		if (ai->register_with_parent) return;
		if (ai->register_with_renderer) {
			gf_sc_ar_remove_src(ai->compositor->audio_renderer, aifce);
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

		gf_sc_ar_add_src(ai->compositor->audio_renderer, aifce);
		ai->register_with_renderer = 1;
		ai->snd = tr_state->sound_holder;
	}
}

GF_EXPORT
void gf_sc_audio_unregister(GF_AudioInput *ai)
{
	GF_AudioInterface *aifce = &ai->input_ifce;
	if (ai->filter) aifce = &ai->filter->input;

	if (ai->register_with_renderer) {
		ai->register_with_renderer = 0;
		gf_sc_ar_remove_src(ai->compositor->audio_renderer, aifce);
	} else {
		/*if used in a parent audio group, do a complete traverse to rebuild the group*/
		gf_sc_invalidate(ai->compositor, NULL);
	}
}


static char *gf_af_fetch_frame(void *callback, u32 *size, u32 audio_delay_ms)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)callback;

	*size = 0;
	if (!af->nb_used) {
		/*force filling the filter chain output until no data is available, otherwise we may end up
		with data in input and no data as output because of block framing of the filter chain*/
		while (!af->nb_filled) {
			u32 nb_bytes;
			char *data = af->src->FetchFrame(af->src->callback, &nb_bytes, audio_delay_ms + af->filter_chain.delay_ms);
			/*no input data*/
			if (!data || !nb_bytes)
				return NULL;

			if (nb_bytes > af->filter_chain.min_block_size) nb_bytes = af->filter_chain.min_block_size;
			memcpy(af->filter_chain.tmp_block1, data, nb_bytes);
			af->src->ReleaseFrame(af->src->callback, nb_bytes);

			af->nb_filled = gf_afc_process(&af->filter_chain, nb_bytes);
		}
	}

	*size = af->nb_filled - af->nb_used;
	return af->filter_chain.tmp_block1 + af->nb_used;
}
static void gf_af_release_frame(void *callback, u32 nb_bytes)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)callback;
	/*mark used bytes of filter output*/
	af->nb_used += nb_bytes;
	if (af->nb_used==af->nb_filled) {
		af->nb_used = 0;
		af->nb_filled = 0;
	}
}

static Fixed gf_af_get_speed(void *callback)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)callback;
	return af->src->GetSpeed(af->src->callback);
}
static Bool gf_af_get_channel_volume(void *callback, Fixed *vol)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)callback;
	return af->src->GetChannelVolume(af->src->callback, vol);
}

static Bool gf_af_is_muted(void *callback)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)callback;
	return af->src->IsMuted(af->src->callback);
}

static Bool gf_af_get_config(GF_AudioInterface *ai, Bool for_reconf)
{
	GF_AudioFilterItem *af = (GF_AudioFilterItem *)ai->callback;

	Bool res = af->src->GetConfig(af->src, for_reconf);
	if (!res) return 0;
	if (!for_reconf) return 1;


	af->input.bps = af->src->bps;
	af->input.samplerate = af->src->samplerate;

	af->input.ch_cfg = af->src->ch_cfg;
	af->input.chan = af->src->chan;

	if (gf_afc_setup(&af->filter_chain, af->input.bps, af->input.samplerate, af->src->chan, af->src->ch_cfg, &af->input.chan, &af->input.ch_cfg)!=GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUDIO, ("[Audio Input] Failed to configure audio filter chain\n"));

		return 0;
	}
	return 1;
}

GF_AudioFilterItem *gf_af_new(GF_Compositor *compositor, GF_AudioInterface *src, char *filter_name)
{
	GF_AudioFilterItem *filter;
	if (!src || !filter_name) return NULL;

	GF_SAFEALLOC(filter, GF_AudioFilterItem);

	filter->src = src;
	filter->input.FetchFrame = gf_af_fetch_frame;
	filter->input.ReleaseFrame = gf_af_release_frame;
	filter->input.GetSpeed = gf_af_get_speed;
	filter->input.GetChannelVolume = gf_af_get_channel_volume;
	filter->input.IsMuted = gf_af_is_muted;
	filter->input.GetConfig = gf_af_get_config;
	filter->input.callback = filter;

	gf_afc_load(&filter->filter_chain, compositor->user, filter_name);
	return filter;
}

void gf_af_del(GF_AudioFilterItem *af)
{
	gf_afc_unload(&af->filter_chain);
	gf_free(af);
}

void gf_af_reset(GF_AudioFilterItem *af)
{
	gf_afc_reset(&af->filter_chain);

	af->nb_filled = af->nb_used = 0;
}
