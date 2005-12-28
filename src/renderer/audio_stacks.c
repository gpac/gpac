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


#include "common_stacks.h"
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>

typedef struct
{
	GF_AudioInput input;
	GF_TimeNode time_handle;
	Double start_time;
	Bool set_duration;
} AudioClipStack;

static void DestroyAudioClip(GF_Node *node)
{
	AudioClipStack *st = (AudioClipStack *)gf_node_get_private(node);
	gf_sr_audio_stop(&st->input);
	gf_sr_audio_unregister(&st->input);
	if (st->time_handle.is_registered) {
		gf_sr_unregister_time_node(st->input.compositor, &st->time_handle);
	}
	free(st);
}


static void AC_Activate(AudioClipStack *st, M_AudioClip *ac)
{
	gf_sr_audio_open(&st->input, &ac->url);
	ac->isActive = 1;
	gf_node_event_out_str((GF_Node *)ac, "isActive");

	gf_mo_set_speed(st->input.stream, st->input.speed);
	/*rerender all graph to get parent audio group*/
	gf_sr_invalidate(st->input.compositor, NULL);
}

static void AC_Deactivate(AudioClipStack *st, M_AudioClip *ac)
{
	gf_sr_audio_stop(&st->input);
	ac->isActive = 0;
	gf_node_event_out_str((GF_Node *)ac, "isActive");

	st->time_handle.needs_unregister = 1;
}

static void RenderAudioClip(GF_Node *node, void *rs)
{
	GF_BaseEffect *eff = (GF_BaseEffect *)rs;
	M_AudioClip *ac = (M_AudioClip *)node;
	AudioClipStack *st = (AudioClipStack *)gf_node_get_private(node);
	/*check end of stream*/
	if (st->input.stream && st->input.stream_finished) {
		if (gf_mo_get_loop(st->input.stream, ac->loop)) {
			gf_sr_audio_restart(&st->input);
		} else if (ac->isActive && gf_mo_should_deactivate(st->input.stream)) {
			/*deactivate*/
			AC_Deactivate(st, ac);
		}
	}
	if (ac->isActive) {
		gf_sr_audio_register(&st->input, (GF_BaseEffect*)rs);
	}
	if (st->set_duration && st->input.stream) {
		ac->duration_changed = gf_mo_get_duration(st->input.stream);
		gf_node_event_out_str(node, "duration_changed");
		st->set_duration = 0;
	}

	/*store mute flag*/
	st->input.is_muted = (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF);
}

static void AC_UpdateTime(GF_TimeNode *tn)
{
	Double time;
	M_AudioClip *ac = (M_AudioClip *)tn->obj;
	AudioClipStack *st = (AudioClipStack *)gf_node_get_private(tn->obj);

	if (! ac->isActive) {
		st->start_time = ac->startTime;
		st->input.speed = ac->pitch;
	}
	time = gf_node_get_scene_time(tn->obj);
	if ((time<st->start_time) || (st->start_time<0)) return;
	
	if (ac->isActive) {
		if ( (ac->stopTime > st->start_time) && (time>=ac->stopTime)) {
			AC_Deactivate(st, ac);
			return;
		}
	}
	if (!ac->isActive) AC_Activate(st, ac);
}


void InitAudioClip(GF_Renderer *sr, GF_Node *node)
{
	AudioClipStack *st = malloc(sizeof(AudioClipStack));
	memset(st, 0, sizeof(AudioClipStack));
	gf_sr_audio_setup(&st->input, sr, node);

	st->time_handle.UpdateTimeNode = AC_UpdateTime;
	st->time_handle.obj = node;
	st->set_duration = 1;

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderAudioClip);
	gf_node_set_predestroy_function(node, DestroyAudioClip);

	gf_sr_register_time_node(sr, &st->time_handle);
}


void AudioClipModified(GF_Node *node)
{
	M_AudioClip *ac = (M_AudioClip *)node;
	AudioClipStack *st = (AudioClipStack *) gf_node_get_private(node);
	if (!st) return;

	/*MPEG4 spec is not clear about that , so this is not forbidden*/
	if (st->input.is_open && st->input.is_open) {
		if (gf_sr_audio_check_url(&st->input, &ac->url)) {
			gf_sr_audio_stop(&st->input);
			gf_sr_audio_open(&st->input, &ac->url);
			/*force unregister to resetup audio cfg*/
			gf_sr_audio_unregister(&st->input);
			gf_sr_invalidate(st->input.compositor, NULL);
		}
	}

	//update state if we're active
	if (ac->isActive) {
		AC_UpdateTime(&st->time_handle);
		/*we're no longer active fon't check for reactivation*/
		if (!ac->isActive) return;
	}

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister) 
		gf_sr_register_time_node(st->input.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = 0;
}


typedef struct
{
	GF_AudioInput input;
	GF_TimeNode time_handle;
	Bool is_active;
	Double start_time;
} AudioSourceStack;

static void DestroyAudioSource(GF_Node *node)
{
	AudioSourceStack *st = (AudioSourceStack *)gf_node_get_private(node);
	gf_sr_audio_stop(&st->input);
	gf_sr_audio_unregister(&st->input);
	if (st->time_handle.is_registered) {
		gf_sr_unregister_time_node(st->input.compositor, &st->time_handle);
	}
	free(st);
}


static void AS_Activate(AudioSourceStack *st, M_AudioSource *as)
{
	gf_sr_audio_open(&st->input, &as->url);
	st->is_active = 1;
	gf_mo_set_speed(st->input.stream, st->input.speed);
	/*rerender all graph to get parent audio group*/
	gf_sr_invalidate(st->input.compositor, NULL);
}

static void AS_Deactivate(AudioSourceStack *st, M_AudioSource *as)
{
	gf_sr_audio_stop(&st->input);
	st->is_active = 0;
	st->time_handle.needs_unregister = 1;
}

static void RenderAudioSource(GF_Node *node, void *rs)
{
	GF_BaseEffect*eff = (GF_BaseEffect*)rs;
	M_AudioSource *as = (M_AudioSource *)node;
	AudioSourceStack *st = (AudioSourceStack *)gf_node_get_private(node);
	/*check end of stream*/
	if (st->input.stream && st->input.stream_finished) {
		if (gf_mo_get_loop(st->input.stream, 0)) {
			gf_sr_audio_restart(&st->input);
		} else if (st->is_active && gf_mo_should_deactivate(st->input.stream)) {
			/*deactivate*/
			AS_Deactivate(st, as);
		}
	}
	if (st->is_active) {
		gf_sr_audio_register(&st->input, (GF_BaseEffect*)rs);
	}

	/*store mute flag*/
	st->input.is_muted = (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF);
}

static void AS_UpdateTime(GF_TimeNode *tn)
{
	Double time;
	M_AudioSource *as = (M_AudioSource *)tn->obj;
	AudioSourceStack *st = (AudioSourceStack *)gf_node_get_private(tn->obj);

	if (! st->is_active) {
		st->start_time = as->startTime;
		st->input.speed = as->speed;
	}
	time = gf_node_get_scene_time(tn->obj);
	if ((time<st->start_time) || (st->start_time<0)) return;
	
	if (st->input.input_ifce.GetSpeed(st->input.input_ifce.callback) && st->is_active) {
		if ( (as->stopTime > st->start_time) && (time>=as->stopTime)) {
			AS_Deactivate(st, as);
			return;
		}
	}
	if (!st->is_active) AS_Activate(st, as);
}


void InitAudioSource(GF_Renderer *sr, GF_Node *node)
{
	AudioSourceStack *st = malloc(sizeof(AudioSourceStack));
	memset(st, 0, sizeof(AudioSourceStack));
	gf_sr_audio_setup(&st->input, sr, node);

	st->time_handle.UpdateTimeNode = AS_UpdateTime;
	st->time_handle.obj = node;

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderAudioSource);
	gf_node_set_predestroy_function(node, DestroyAudioSource);

	gf_sr_register_time_node(sr, &st->time_handle);
}


void AudioSourceModified(GF_Node *node)
{
	M_AudioSource *as = (M_AudioSource *)node;
	AudioSourceStack *st = (AudioSourceStack *) gf_node_get_private(node);
	if (!st) return;

	/*MPEG4 spec is not clear about that , so this is not forbidden*/
	if (st->input.is_open&& st->input.is_open) {
		if (gf_sr_audio_check_url(&st->input, &as->url)) {
			gf_sr_audio_stop(&st->input);
			gf_sr_audio_open(&st->input, &as->url);
			/*force unregister to resetup audio cfg*/
			gf_sr_audio_unregister(&st->input);
			gf_sr_invalidate(st->input.compositor, NULL);
		}
	}

	//update state if we're active
	if (st->is_active) {
		AS_UpdateTime(&st->time_handle);
		if (!st->is_active) return;
	}

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister) 
		gf_sr_register_time_node(st->input.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = 0;
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

static void DestroyAudioBuffer(GF_Node *node)
{
	AudioBufferStack *st = (AudioBufferStack *)gf_node_get_private(node);

	gf_sr_audio_unregister(&st->output);
	if (st->time_handle.is_registered) 
		gf_sr_unregister_time_node(st->output.compositor, &st->time_handle);

	gf_mixer_del(st->am);
	if (st->buffer) free(st->buffer);
	gf_list_del(st->new_inputs);
	free(st);
}


/*we have no choice but always browsing the children, since a src can be replaced by a new one
without the parent being modified. We just collect the src and check against the current mixer inputs
to reset the mixer or not - the spec is not clear about that btw, shall rebuffering happen if a source is modified or not ...*/
static void RenderAudioBuffer(GF_Node *node, void *rs)
{
	u32 count, i, j;
	Bool update_mixer;
	GF_AudioGroup *parent;
	AudioBufferStack *st = (AudioBufferStack *)gf_node_get_private(node);
	M_AudioBuffer *ab = (M_AudioBuffer *)node;
	GF_BaseEffect*eff = (GF_BaseEffect*) rs;

	parent = eff->audio_parent;
	eff->audio_parent = (GF_AudioGroup *) st;
	count = gf_list_count(ab->children);
	for (i=0; i<count; i++) {
		GF_Node *ptr = gf_list_get(ab->children, i);
		gf_node_render(ptr, eff);
	}

	gf_mixer_lock(st->am, 1);

	/*if no new inputs don't change mixer config*/
	update_mixer = gf_list_count(st->new_inputs) ? 1 : 0;
	
	if (gf_mixer_get_src_count(st->am) == gf_list_count(st->new_inputs)) {
		u32 count = gf_list_count(st->new_inputs);
		update_mixer = 0;
		for (j=0; j<count; j++) {
			GF_AudioInput *cur = gf_list_get(st->new_inputs, j);
			if (!gf_mixer_is_src_present(st->am, &cur->input_ifce)) {
				update_mixer = 1;
				break;
			}
		}
	}

	if (update_mixer) {
		gf_mixer_remove_all(st->am);
		gf_mixer_force_chanel_out(st->am, ab->numChan);
	}

	while (gf_list_count(st->new_inputs)) {
		GF_AudioInput *src = gf_list_get(st->new_inputs, 0);
		gf_list_rem(st->new_inputs, 0);
		if (update_mixer) gf_mixer_add_input(st->am, &src->input_ifce);
	}

	gf_mixer_lock(st->am, 0);
	eff->audio_parent = parent;

	/*Note the audio buffer is ALWAYS registered untill destroyed since buffer filling shall happen even when inactive*/
	if (!st->output.register_with_parent || !st->output.register_with_renderer) 
		gf_sr_audio_register(&st->output, eff);

	/*store mute flag*/
	st->is_muted = (eff->trav_flags & GF_SR_TRAV_SWITCHED_OFF);
}



static void AB_Activate(AudioBufferStack *st, M_AudioBuffer *ab)
{
	ab->isActive = 1;
	gf_node_event_out_str((GF_Node *)ab, "isActive");
	/*rerender all graph to get parent audio group*/
	gf_sr_invalidate(st->output.compositor, NULL);
	st->done = 0;
	st->read_pos = 0;
}

static void AB_Deactivate(AudioBufferStack *st, M_AudioBuffer *ab)
{
	ab->isActive = 0;
	gf_node_event_out_str((GF_Node *)ab, "isActive");
	st->time_handle.needs_unregister = 1;
}

static void AB_UpdateTime(GF_TimeNode *tn)
{
	Double time;
	M_AudioBuffer *ab = (M_AudioBuffer *)tn->obj;
	AudioBufferStack *st = (AudioBufferStack *)gf_node_get_private(tn->obj);

	if (! ab->isActive) {
		st->start_time = ab->startTime;
	}
	time = gf_node_get_scene_time(tn->obj);
	if ((time<st->start_time) || (st->start_time<0)) return;
	
	if (ab->isActive) {
		if ( (ab->stopTime > st->start_time) && (time>=ab->stopTime)) {
			AB_Deactivate(st, ab);
			return;
		}
		/*THIS IS NOT NORMATIVE*/
		if ( !ab->loop && st->done) {
			AB_Deactivate(st, ab);
			return;
		}
	}
	if (!ab->isActive) AB_Activate(st, ab);
}




static char *AB_FetchFrame(void *callback, u32 *size, u32 audio_delay_ms)
{
	u32 blockAlign;
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	M_AudioBuffer *ab = (M_AudioBuffer*)st->output.owner;

	if (!st->is_init) return NULL;
	if (!st->buffer) {
		st->done = 0;
		st->buffer_size = (u32) ceil(FIX2FLT(ab->length) * st->output.input_ifce.bps*st->output.input_ifce.sr*st->output.input_ifce.chan/8);
		blockAlign = gf_mixer_get_block_align(st->am);
		/*BLOCK ALIGN*/
		while (st->buffer_size%blockAlign) st->buffer_size++;
		st->buffer = malloc(sizeof(char) * st->buffer_size);
		memset(st->buffer, 0, sizeof(char) * st->buffer_size);
		st->read_pos = st->write_pos = 0;
	}
	if (st->done) return NULL;

	/*even if not active, fill the buffer*/
	if (st->write_pos < st->buffer_size) {
		u32 written;
		while (1) {
			/*just try to completely fill it*/
			written = gf_mixer_get_output(st->am, st->buffer + st->write_pos, st->buffer_size - st->write_pos);
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

static void AB_ReleaseFrame(void *callback, u32 nb_bytes)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	st->read_pos += nb_bytes;
	assert(st->read_pos<=st->write_pos);
	if (st->read_pos==st->write_pos) {
		if (st->write_pos<st->buffer_size) {
			/*reading faster than buffering - let's still attempt to fill the buffer*/
#if 0
			st->write_pos = st->buffer_size;
			fprintf(stdout, "Warning: AudioBuffer done playing before buffer filling done\n");
#endif
		} else if ( ((M_AudioBuffer*)st->output.owner)->loop) {
			st->read_pos = 0;
		} else {
			st->done = 1;
		}
	}
}


static Fixed AB_GetSpeed(void *callback)
{
	M_AudioBuffer *ab = (M_AudioBuffer *) ((GF_AudioInput *) callback)->owner;
	return ab->pitch;
}

static Bool AB_GetChannelVolume(void *callback, Fixed *vol)
{
	GF_AudioInput *ai = (GF_AudioInput *) callback;
	if (ai->snd->GetChannelVolume) {
		return ai->snd->GetChannelVolume(ai->snd->owner, vol);
	} else {
		vol[0] = vol[1] = vol[2] = vol[3] = vol[4] = vol[5] = FIX_ONE;
		return 0;
	}
}

static Bool AB_IsMuted(void *callback)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) callback)->owner);
	return st->is_muted;
}

static Bool AB_GetConfig(GF_AudioInterface *aifc, Bool for_reconf)
{
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private( ((GF_AudioInput *) aifc->callback)->owner);

	if (gf_mixer_must_reconfig(st->am)) {
		if (gf_mixer_reconfig(st->am)) {
			if (st->buffer) free(st->buffer);
			st->buffer = NULL;
			st->buffer_size = 0;
		}

		gf_mixer_get_config(st->am, &aifc->sr, &aifc->chan, &aifc->bps, &aifc->ch_cfg);
		st->is_init = (aifc->sr && aifc->chan && aifc->bps) ? 1 : 0;
		assert(st->is_init);
		if (!st->is_init) aifc->sr = aifc->chan = aifc->bps = aifc->ch_cfg = 0;
		/*this will force invalidation*/
		return (for_reconf && st->is_init) ? 1 : 0;
	}
	return st->is_init;
}

void AB_AddSource(GF_AudioGroup *_this, GF_AudioInput *src)
{
	AudioBufferStack *st = (AudioBufferStack *)_this;
	if (!src) return;
	/*just collect the input, reconfig is done once all children are rendered*/
	gf_list_add(st->new_inputs, src);
}


void setup_audiobufer(GF_AudioInput *ai, GF_Renderer *sr, GF_Node *node)
{
	memset(ai, 0, sizeof(GF_AudioInput));
	ai->owner = node;
	ai->compositor = sr;
	/*NEVER used for audio buffer*/
	ai->stream = NULL;
	/*setup io interface*/
	ai->input_ifce.FetchFrame = AB_FetchFrame;
	ai->input_ifce.ReleaseFrame = AB_ReleaseFrame;
	ai->input_ifce.GetConfig = AB_GetConfig;
	ai->input_ifce.GetChannelVolume = AB_GetChannelVolume;
	ai->input_ifce.GetSpeed = AB_GetSpeed;
	ai->input_ifce.IsMuted = AB_IsMuted;
	ai->input_ifce.callback = ai;
	ai->speed = FIX_ONE;
}

void InitAudioBuffer(GF_Renderer *sr, GF_Node *node)
{
	AudioBufferStack *st = malloc(sizeof(AudioBufferStack));
	memset(st, 0, sizeof(AudioBufferStack));

	/*use our private input*/
	setup_audiobufer(&st->output, sr, node);
	st->add_source = AB_AddSource;

	st->time_handle.UpdateTimeNode = AB_UpdateTime;
	st->time_handle.obj = node;
	st->set_duration = 1;

	st->am = gf_mixer_new(NULL);
	st->new_inputs = gf_list_new();

	gf_node_set_private(node, st);
	gf_node_set_render_function(node, RenderAudioBuffer);
	gf_node_set_predestroy_function(node, DestroyAudioBuffer);
	gf_sr_register_time_node(sr, &st->time_handle);
}


void AudioBufferModified(GF_Node *node)
{
	M_AudioBuffer *ab = (M_AudioBuffer *)node;
	AudioBufferStack *st = (AudioBufferStack *) gf_node_get_private(node);
	if (!st) return;

	//update state if we're active
	if (ab->isActive) 
		AB_UpdateTime(&st->time_handle);

	/*make sure we are still registered*/
	if (!st->time_handle.is_registered && !st->time_handle.needs_unregister) 
		gf_sr_register_time_node(st->output.compositor, &st->time_handle);
	else
		st->time_handle.needs_unregister = 0;
}

