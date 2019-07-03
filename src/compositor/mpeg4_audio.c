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


#include "nodes_stacks.h"

#ifndef GPAC_DISABLE_VRML


static void audioclip_activate(AudioClipStack *st, M_AudioClip *ac)
{
	if (gf_sc_audio_open(&st->input, &ac->url, 0, -1, GF_FALSE) != GF_OK) {
		st->failure = GF_TRUE;
		return;
	}
	ac->isActive = 1;
	gf_node_event_out((GF_Node *)ac, 7/*"isActive"*/);

	gf_mo_set_speed(st->input.stream, st->input.speed);
	/*traverse all graph to get parent audio group*/
	gf_sc_invalidate(st->input.compositor, NULL);
}

static void audioclip_deactivate(AudioClipStack *st, M_AudioClip *ac)
{
	gf_sc_audio_stop(&st->input);
	ac->isActive = 0;
	gf_node_event_out((GF_Node *)ac, 7/*"isActive"*/);

	st->time_handle.needs_unregister = GF_TRUE;
}

static void audioclip_traverse(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	M_AudioClip *ac = (M_AudioClip *)node;
	AudioClipStack *st = (AudioClipStack *)gf_node_get_private(node);

	if (is_destroy) {
		gf_sc_audio_predestroy(&st->input);
		if (st->time_handle.is_registered) {
			gf_sc_unregister_time_node(st->input.compositor, &st->time_handle);
		}
		gf_free(st);
		return;
	}
	if (st->failure) return;

	/*check end of stream*/
	if (st->input.stream && st->input.stream_finished) {
		if (gf_mo_get_loop(st->input.stream, ac->loop)) {
			gf_sc_audio_restart(&st->input);
		} else if (ac->isActive && gf_mo_should_deactivate(st->input.stream)) {
			/*deactivate*/
			audioclip_deactivate(st, ac);
		}
	}
	if (ac->isActive) {
		gf_sc_audio_register(&st->input, (GF_TraverseState*)rs);
	}
	if (st->set_duration && st->input.stream && st->input.stream->odm) {
		ac->duration_changed = gf_mo_get_duration(st->input.stream);
		gf_node_event_out(node, 6/*"duration_changed"*/);
		st->set_duration = GF_FALSE;
	}

	/*store mute flag*/
	st->input.is_muted = tr_state->switched_off;
}

static void audioclip_update_time(GF_TimeNode *tn)
{
	Double time;
	M_AudioClip *ac = (M_AudioClip *)tn->udta;
	AudioClipStack *st = (AudioClipStack *)gf_node_get_private((GF_Node*)tn->udta);

	if (st->failure) return;
	if (! ac->isActive) {
		st->start_time = ac->startTime;
		st->input.speed = ac->pitch;
	}
	time = gf_node_get_scene_time((GF_Node*)tn->udta);
	if ((time<st->start_time) || (st->start_time<0)) return;

	if (ac->isActive) {
		if ( (ac->stopTime > st->start_time) && (time>=ac->stopTime)) {
			audioclip_deactivate(st, ac);
			return;
		}
	}
	if (!ac->isActive) audioclip_activate(st, ac);
}


void compositor_init_audioclip(GF_Compositor *compositor, GF_Node *node)
{
	AudioClipStack *st;
	GF_SAFEALLOC(st, AudioClipStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate style group stack\n"));
		return;
	}
	gf_sc_audio_setup(&st->input, compositor, node);

	st->time_handle.UpdateTimeNode = audioclip_update_time;
	st->time_handle.udta = node;
	st->set_duration = GF_TRUE;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, audioclip_traverse);
	gf_sc_register_time_node(compositor, &st->time_handle);
}


void compositor_audioclip_modified(GF_Node *node)
{
	M_AudioClip *ac = (M_AudioClip *)node;
	AudioClipStack *st = (AudioClipStack *) gf_node_get_private(node);
	if (!st) return;

	st->failure = GF_FALSE;

	/*MPEG4 spec is not clear about that , so this is not forbidden*/
	if (st->input.is_open) {
		if (gf_sc_audio_check_url(&st->input, &ac->url)) {
			gf_sc_audio_stop(&st->input);
			gf_sc_audio_open(&st->input, &ac->url, 0, -1, GF_FALSE);
			/*force unregister to resetup audio cfg*/
			gf_sc_audio_unregister(&st->input);
			gf_sc_invalidate(st->input.compositor, NULL);
		}
	}

	//update state if we're active
	if (ac->isActive) {
		audioclip_update_time(&st->time_handle);
		/*we're no longer active fon't check for reactivation*/
		if (!ac->isActive) return;
	}

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister)
		gf_sc_register_time_node(st->input.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = GF_FALSE;
}


static void audiosource_activate(AudioSourceStack *st, M_AudioSource *as)
{
	if (gf_sc_audio_open(&st->input, &as->url, 0, -1, GF_FALSE) != GF_OK)
		return;
	st->is_active = GF_TRUE;
	gf_mo_set_speed(st->input.stream, st->input.speed);
	/*traverse all graph to get parent audio group*/
	gf_sc_invalidate(st->input.compositor, NULL);
}

static void audiosource_deactivate(AudioSourceStack *st, M_AudioSource *as)
{
	gf_sc_audio_stop(&st->input);
	st->is_active = GF_FALSE;
	st->time_handle.needs_unregister = GF_TRUE;
}

static void audiosource_traverse(GF_Node *node, void *rs, Bool is_destroy)
{
	GF_TraverseState*tr_state = (GF_TraverseState*)rs;
	M_AudioSource *as = (M_AudioSource *)node;
	AudioSourceStack *st = (AudioSourceStack *)gf_node_get_private(node);


	if (is_destroy) {
		gf_sc_audio_predestroy(&st->input);
		if (st->time_handle.is_registered) {
			gf_sc_unregister_time_node(st->input.compositor, &st->time_handle);
		}
		gf_free(st);
		return;
	}


	/*check end of stream*/
	if (st->input.stream && st->input.stream_finished) {
		if (gf_mo_get_loop(st->input.stream, GF_FALSE)) {
			gf_sc_audio_restart(&st->input);
		} else if (st->is_active && gf_mo_should_deactivate(st->input.stream)) {
			/*deactivate*/
			audiosource_deactivate(st, as);
		}
	}
	if (st->is_active) {
		gf_sc_audio_register(&st->input, (GF_TraverseState*)rs);
	}

	/*store mute flag*/
	st->input.is_muted = tr_state->switched_off;
}

static void audiosource_update_time(GF_TimeNode *tn)
{
	Double time;
	M_AudioSource *as = (M_AudioSource *)tn->udta;
	AudioSourceStack *st = (AudioSourceStack *)gf_node_get_private((GF_Node*)tn->udta);

	if (! st->is_active) {
		st->start_time = as->startTime;
		st->input.speed = as->speed;
	}
	time = gf_node_get_scene_time((GF_Node*)tn->udta);
	if ((time<st->start_time) || (st->start_time<0)) return;

	if (st->input.input_ifce.GetSpeed(st->input.input_ifce.callback) && st->is_active) {
		if ( (as->stopTime > st->start_time) && (time>=as->stopTime)) {
			audiosource_deactivate(st, as);
			return;
		}
	}
	if (!st->is_active) audiosource_activate(st, as);
}


void compositor_init_audiosource(GF_Compositor *compositor, GF_Node *node)
{
	AudioSourceStack *st;
	GF_SAFEALLOC(st, AudioSourceStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate style group stack\n"));
		return;
	}
	gf_sc_audio_setup(&st->input, compositor, node);

	st->time_handle.UpdateTimeNode = audiosource_update_time;
	st->time_handle.udta = node;

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, audiosource_traverse);
	gf_sc_register_time_node(compositor, &st->time_handle);
}


void compositor_audiosource_modified(GF_Node *node)
{
	M_AudioSource *as = (M_AudioSource *)node;
	AudioSourceStack *st = (AudioSourceStack *) gf_node_get_private(node);
	if (!st) return;

	/*MPEG4 spec is not clear about that , so this is not forbidden*/
	if (gf_sc_audio_check_url(&st->input, &as->url)) {
		if (st->input.is_open) gf_sc_audio_stop(&st->input);
		/*force unregister to resetup audio cfg*/
		gf_sc_audio_unregister(&st->input);
		gf_sc_invalidate(st->input.compositor, NULL);

		if (st->is_active) gf_sc_audio_open(&st->input, &as->url, 0, -1, GF_FALSE);
	}

	//update state if we're active
	if (st->is_active) {
		audiosource_update_time(&st->time_handle);
		if (!st->is_active) return;
	}

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister)
		gf_sc_register_time_node(st->input.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = GF_FALSE;
}


typedef struct
{
	AUDIO_GROUP_NODE

	GF_TimeNode time_handle;
	Double start_time;
	Bool set_duration;
	/*AudioBuffer mixes its children*/
	GF_AudioMixer *am;
	Bool is_init, is_muted;
	/*buffer audio data*/
	char *buffer;
	u32 buffer_size;

	Bool done;
	/*read/write position in buffer and associated read time (CTS)*/
	u32 read_pos, write_pos, cur_cts;
	/*list of audio children after a traverse*/
	GF_List *new_inputs;
} AudioBufferStack;



/*we have no choice but always browsing the children, since a src can be replaced by a new one
without the parent being modified. We just collect the src and check against the current mixer inputs
to reset the mixer or not - the spec is not clear about that btw, shall rebuffering happen if a source is modified or not ...*/
static void audiobuffer_traverse(GF_Node *node, void *rs, Bool is_destroy)
{
	u32 j;
	Bool update_mixer;
	GF_ChildNodeItem *l;
	GF_AudioGroup *parent;
	AudioBufferStack *st = (AudioBufferStack *)gf_node_get_private(node);
	M_AudioBuffer *ab = (M_AudioBuffer *)node;
	GF_TraverseState*tr_state = (GF_TraverseState*) rs;

	if (is_destroy) {
		gf_sc_audio_unregister(&st->output);
		if (st->time_handle.is_registered)
			gf_sc_unregister_time_node(st->output.compositor, &st->time_handle);

		gf_mixer_del(st->am);
		if (st->buffer) gf_free(st->buffer);
		gf_list_del(st->new_inputs);
		gf_free(st);
		return;
	}
	parent = tr_state->audio_parent;
	tr_state->audio_parent = (GF_AudioGroup *) st;
	l = ab->children;
	while (l) {
		gf_node_traverse(l->node, tr_state);
		l = l->next;
	}

	gf_mixer_lock(st->am, GF_TRUE);

	/*if no new inputs don't change mixer config*/
	update_mixer = gf_list_count(st->new_inputs) ? GF_TRUE : GF_FALSE;

	if (gf_mixer_get_src_count(st->am) == gf_list_count(st->new_inputs)) {
		u32 count = gf_list_count(st->new_inputs);
		update_mixer = GF_FALSE;
		for (j=0; j<count; j++) {
			GF_AudioInput *cur = (GF_AudioInput *)gf_list_get(st->new_inputs, j);
			if (!gf_mixer_is_src_present(st->am, &cur->input_ifce)) {
				update_mixer = GF_TRUE;
				break;
			}
		}
	}

	if (update_mixer) {
		gf_mixer_remove_all(st->am);
		gf_mixer_force_chanel_out(st->am, ab->numChan);
	}

	while (gf_list_count(st->new_inputs)) {
		GF_AudioInput *src = (GF_AudioInput *)gf_list_get(st->new_inputs, 0);
		gf_list_rem(st->new_inputs, 0);
		if (update_mixer) gf_mixer_add_input(st->am, &src->input_ifce);
	}

	gf_mixer_lock(st->am, GF_FALSE);
	tr_state->audio_parent = parent;

	/*Note the audio buffer is ALWAYS registered untill destroyed since buffer filling shall happen even when inactive*/
	if (!st->output.register_with_parent || !st->output.register_with_renderer)
		gf_sc_audio_register(&st->output, tr_state);

	/*store mute flag*/
	st->is_muted = tr_state->switched_off;
}



static void audiobuffer_activate(AudioBufferStack *st, M_AudioBuffer *ab)
{
	ab->isActive = 1;
	gf_node_event_out((GF_Node *)ab, 17/*"isActive"*/);
	/*rerender all graph to get parent audio group*/
	gf_sc_invalidate(st->output.compositor, NULL);
	st->done = GF_FALSE;
	st->read_pos = 0;
}

static void audiobuffer_deactivate(AudioBufferStack *st, M_AudioBuffer *ab)
{
	ab->isActive = 0;
	gf_node_event_out((GF_Node *)ab, 17/*"isActive"*/);
	st->time_handle.needs_unregister = GF_TRUE;
}

static void audiobuffer_update_time(GF_TimeNode *tn)
{
	Double time;
	M_AudioBuffer *ab = (M_AudioBuffer *)tn->udta;
	AudioBufferStack *st = (AudioBufferStack *)gf_node_get_private((GF_Node*)tn->udta);

	if (! ab->isActive) {
		st->start_time = ab->startTime;
	}
	time = gf_node_get_scene_time((GF_Node*)tn->udta);
	if ((time<st->start_time) || (st->start_time<0)) return;

	if (ab->isActive) {
		if ( (ab->stopTime > st->start_time) && (time>=ab->stopTime)) {
			audiobuffer_deactivate(st, ab);
			return;
		}
		/*THIS IS NOT NORMATIVE*/
		if ( !ab->loop && st->done) {
			audiobuffer_deactivate(st, ab);
			return;
		}
	}
	if (!ab->isActive) audiobuffer_activate(st, ab);
}




static u8 *audiobuffer_fetch_frame(void *callback, u32 *size, u32 *planar_stride, u32 audio_delay_ms)
{
	u32 blockAlign;
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	M_AudioBuffer *ab = (M_AudioBuffer*)st->output.owner;

	if (!st->is_init) return NULL;
	if (!st->buffer) {
		st->done = GF_FALSE;
		st->buffer_size = (u32) ceil(FIX2FLT(ab->length) * gf_audio_fmt_bit_depth(st->output.input_ifce.afmt) * st->output.input_ifce.samplerate*st->output.input_ifce.chan/8);
		blockAlign = gf_mixer_get_block_align(st->am);
		/*BLOCK ALIGN*/
		while (st->buffer_size%blockAlign) st->buffer_size++;
		st->buffer = (char*)gf_malloc(sizeof(char) * st->buffer_size);
		memset(st->buffer, 0, sizeof(char) * st->buffer_size);
		st->read_pos = st->write_pos = 0;
	}
	if (st->done) return NULL;

	/*even if not active, fill the buffer*/
	if (st->write_pos < st->buffer_size) {
		u32 written;
		while (1) {
			/*just try to completely fill it*/
			written = gf_mixer_get_output(st->am, st->buffer + st->write_pos, st->buffer_size - st->write_pos, 0);
			if (!written) break;
			st->write_pos += written;
			assert(st->write_pos<=st->buffer_size);
		}
	}
	/*not playing*/
	if (! ab->isActive) return NULL;
	*size = st->write_pos - st->read_pos;
	return st->buffer + st->read_pos;
}

static void audiobuffer_release_frame(void *callback, u32 nb_bytes)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	st->read_pos += nb_bytes;
	assert(st->read_pos<=st->write_pos);
	if (st->read_pos==st->write_pos) {
		if (st->write_pos<st->buffer_size) {
			/*reading faster than buffering - let's still attempt to fill the buffer*/
#if 0
			st->write_pos = st->buffer_size;
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[AudioBuffer] done playing before buffer filling done\n"));
#endif
		} else if ( ((M_AudioBuffer*)st->output.owner)->loop) {
			st->read_pos = 0;
		} else {
			st->done = GF_TRUE;
		}
	}
}


static Fixed audiobuffer_get_speed(void *callback)
{
	M_AudioBuffer *ab = (M_AudioBuffer *) ((GF_AudioInput *) callback)->owner;
	return ab->pitch;
}

static Bool audiobuffer_get_volume(void *callback, Fixed *vol)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (ai->snd->GetChannelVolume) {
		return ai->snd->GetChannelVolume(ai->snd->owner, vol);
	} else {
//		u32 i;
//		for (i=0; i<GF_AUDIO_MIXER_MAX_CHANNELS; i++) vol[i] = FIX_ONE;
		return GF_FALSE;
	}
}

static Bool audiobuffer_is_muted(void *callback)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	return st->is_muted;
}

static Bool audiobuffer_get_config(GF_AudioInterface *aifc, Bool for_reconf)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) aifc->callback)->owner);

	if (gf_mixer_must_reconfig(st->am)) {
		Bool force_config = GF_TRUE;
		if (gf_mixer_reconfig(st->am)) {
			if (st->buffer) gf_free(st->buffer);
			st->buffer = NULL;
			st->buffer_size = 0;
		}

		gf_mixer_get_config(st->am, &aifc->samplerate, &aifc->chan, &aifc->afmt, &aifc->ch_cfg);
		//we only work with packed formats
		switch (aifc->afmt) {
		case GF_AUDIO_FMT_U8P:
			aifc->afmt = GF_AUDIO_FMT_U8;
			break;
		case GF_AUDIO_FMT_S16P:
			aifc->afmt = GF_AUDIO_FMT_S16;
			break;
		case GF_AUDIO_FMT_S24P:
			aifc->afmt = GF_AUDIO_FMT_S24;
			break;
		case GF_AUDIO_FMT_S32P:
			aifc->afmt = GF_AUDIO_FMT_S32;
			break;
		case GF_AUDIO_FMT_FLTP:
			aifc->afmt = GF_AUDIO_FMT_FLT;
			break;
		case GF_AUDIO_FMT_DBLP:
			aifc->afmt = GF_AUDIO_FMT_DBL;
			break;
		default:
			force_config = GF_FALSE;
			break;
		}
		if (force_config) {
			gf_mixer_set_config(st->am, aifc->samplerate, aifc->chan, aifc->afmt, aifc->ch_cfg);
		}
		st->is_init = (aifc->samplerate && aifc->chan && aifc->afmt) ? GF_TRUE : GF_FALSE;
		assert(st->is_init);
		if (!st->is_init) aifc->samplerate = aifc->chan = aifc->afmt = aifc->ch_cfg = 0;
		/*this will force invalidation*/
		return (for_reconf && st->is_init) ? GF_TRUE : GF_FALSE;
	}
	return st->is_init;
}

void audiobuffer_add_source(GF_AudioGroup *_this, GF_AudioInput *src)
{
	AudioBufferStack *st = (AudioBufferStack *)_this;
	if (!src) return;
	/*just collect the input, reconfig is done once all children are rendered*/
	gf_list_add(st->new_inputs, src);
}


void setup_audiobuffer(GF_AudioInput *ai, GF_Compositor *compositor, GF_Node *node)
{
	memset(ai, 0, sizeof(GF_AudioInput));
	ai->owner = node;
	ai->compositor = compositor;
	/*NEVER used for audio buffer*/
	ai->stream = NULL;
	/*setup io interface*/
	ai->input_ifce.FetchFrame = audiobuffer_fetch_frame;
	ai->input_ifce.ReleaseFrame = audiobuffer_release_frame;
	ai->input_ifce.GetConfig = audiobuffer_get_config;
	ai->input_ifce.GetChannelVolume = audiobuffer_get_volume;
	ai->input_ifce.GetSpeed = audiobuffer_get_speed;
	ai->input_ifce.IsMuted = audiobuffer_is_muted;
	ai->input_ifce.callback = ai;
	ai->speed = FIX_ONE;
}

void compositor_init_audiobuffer(GF_Compositor *compositor, GF_Node *node)
{
	AudioBufferStack *st;
	GF_SAFEALLOC(st, AudioBufferStack);
	if (!st) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate audiobuffer stack\n"));
		return;
	}

	/*use our private input*/
	setup_audiobuffer(&st->output, compositor, node);
	st->add_source = audiobuffer_add_source;

	st->time_handle.UpdateTimeNode = audiobuffer_update_time;
	st->time_handle.udta = node;
	st->set_duration = GF_TRUE;

	st->am = gf_mixer_new(NULL);
	st->new_inputs = gf_list_new();

	gf_node_set_private(node, st);
	gf_node_set_callback_function(node, audiobuffer_traverse);
	gf_sc_register_time_node(compositor, &st->time_handle);
}


void compositor_audiobuffer_modified(GF_Node *node)
{
	M_AudioBuffer *ab = (M_AudioBuffer *)node;
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private(node);
	if (!st) return;

	//update state if we're active
	if (ab->isActive)
		audiobuffer_update_time(&st->time_handle);

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister)
		gf_sc_register_time_node(st->output.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = GF_FALSE;
}

#endif /*GPAC_DISABLE_VRML*/
