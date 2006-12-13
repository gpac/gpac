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
#include "media_memory.h"
/*in case the media manager is also responsible for visual rendering*/
#include <gpac/renderer.h>


u32 MM_Loop(void *par);


enum
{
	GF_MM_CE_RUNNING= 1,
	GF_MM_CE_HAS_ERROR = 1<<1,
	GF_MM_CE_THREADED = 1<<2,
	GF_MM_CE_REQ_THREAD = 1<<3,
	/*only used by threaded decs to signal end of thread*/
	GF_MM_CE_DEAD = 1<<4
};

typedef struct
{
	u32 flags;
	GF_Codec *dec;
	/*for threaded decoders*/
	GF_Thread *thread;
	GF_Mutex *mx;
} CodecEntry;

GF_Err gf_term_init_scheduler(GF_Terminal *term, u32 threading_mode)
{
	term->mm_mx = gf_mx_new();
	term->codecs = gf_list_new();

	term->frame_duration = 33;
	switch (threading_mode) {
	case GF_TERM_THREAD_SINGLE: term->flags |= GF_TERM_SINGLE_THREAD;
		break;
	case GF_TERM_THREAD_MULTI: term->flags |= GF_TERM_MULTI_THREAD;
		break;
	default:
		break;
	}

	if (term->user->init_flags & GF_TERM_NO_VISUAL_THREAD) return GF_OK;

	term->mm_thread = gf_th_new();
	term->flags |= GF_TERM_RUNNING;
	term->priority = GF_THREAD_PRIORITY_NORMAL;
	gf_th_run(term->mm_thread, MM_Loop, term);
	return GF_OK;
}

void gf_term_stop_scheduler(GF_Terminal *term)
{
	if (term->mm_thread) {
		term->flags &= ~GF_TERM_RUNNING;
		while (!(term->flags & GF_TERM_DEAD) ) 
			gf_sleep(0);

		assert(! gf_list_count(term->codecs));
		gf_th_del(term->mm_thread);
	}
	gf_list_del(term->codecs);
	gf_mx_del(term->mm_mx);
}

static CodecEntry *mm_get_codec(GF_List *list, GF_Codec *codec)
{
	CodecEntry *ce;
	u32 i = 0;
	while ((ce = (CodecEntry*)gf_list_enum(list, &i))) {
		if (ce->dec==codec) return ce;
	}
	return NULL;
}


void gf_term_add_codec(GF_Terminal *term, GF_Codec *codec)
{
	u32 i, count;
	Bool threaded;
	CodecEntry *cd;
	CodecEntry *ptr, *next;
	GF_CodecCapability cap;
	assert(codec);

	/*we need REAL exclusive access when adding a dec*/
	gf_mx_p(term->mm_mx);

	cd = mm_get_codec(term->codecs, codec);
	if (cd) goto exit;

	GF_SAFEALLOC(cd, CodecEntry);
	cd->dec = codec;

	cap.CapCode = GF_CODEC_WANTS_THREAD;
	cap.cap.valueInt = 0;
	gf_codec_get_capability(codec, &cap);
	threaded = cap.cap.valueInt;
	if (threaded) cd->flags |= GF_MM_CE_REQ_THREAD;

	if (term->flags & GF_TERM_MULTI_THREAD) {
		if ((codec->type==0x04) || (codec->type==0x05)) threaded = 1;
	} else if (term->flags & GF_TERM_SINGLE_THREAD) {
		threaded = 0;
	}
	
	if (threaded) {
		cd->thread = gf_th_new();
		cd->mx = gf_mx_new();
		cd->flags |= GF_MM_CE_THREADED;
		gf_list_add(term->codecs, cd);
		goto exit;
	}

	//add codec 1- per priority 2- per type, audio being first
	//priorities inherits from Systems (5bits) so range from 0 to 31
	//we sort from MAX to MIN
	count = gf_list_count(term->codecs);
	for (i=0; i<count; i++) {
		ptr = (CodecEntry*)gf_list_get(term->codecs, i);
		if (ptr->flags & GF_MM_CE_THREADED) continue;

		//higher priority, continue
		if (ptr->dec->Priority > codec->Priority) continue;

		//same priority, put audio first
		if (ptr->dec->Priority == codec->Priority) {
			//we insert audio (0x05) before video (0x04)
			if (ptr->dec->type < codec->type) {
				gf_list_insert(term->codecs, cd, i);
				goto exit;
			}
			//same prior, same type: insert after
			if (ptr->dec->type == codec->type) {
				if (i+1==count) {
					gf_list_add(term->codecs, cd);
				} else {
					gf_list_insert(term->codecs, cd, i+1);
				}
				goto exit;
			}
			//we insert video (0x04) after audio (0x05) if next is not audio
			//last one
			if (i+1 == count) {
				gf_list_add(term->codecs, cd);
				goto exit;
			}
			next = (CodecEntry*)gf_list_get(term->codecs, i+1);
			//# priority level, insert
			if ((next->flags & GF_MM_CE_THREADED) || (next->dec->Priority != codec->Priority)) {
				gf_list_insert(term->codecs, cd, i+1);
				goto exit;
			}
			//same priority level and at least one after : continue
			continue;
		}
		gf_list_insert(term->codecs, cd, i);
		goto exit;
	}
	//if we got here, first in list
	gf_list_add(term->codecs, cd);

exit:
	gf_mx_v(term->mm_mx);
	return;
}

void gf_term_remove_codec(GF_Terminal *term, GF_Codec *codec)
{
	u32 i;
	CodecEntry *ce;

	/*we need REAL exclusive access when removing a dec*/
	gf_mx_p(term->mm_mx);

	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
		if (ce->dec != codec) continue;

		if (ce->thread) {
			if (ce->flags & GF_MM_CE_RUNNING) {
				ce->flags &= ~GF_MM_CE_RUNNING;
				while (! (ce->flags & GF_MM_CE_DEAD)) gf_sleep(10);
				ce->flags &= ~GF_MM_CE_DEAD;
			}
			gf_th_del(ce->thread);
			gf_mx_del(ce->mx);
		}
		free(ce);
		gf_list_rem(term->codecs, i-1);
		break;
	}
	gf_mx_v(term->mm_mx);
	return;
}

static u32 MM_SimulationStep(GF_Terminal *term, u32 *last_dec)
{
	CodecEntry *ce;
	GF_Err e;
	u32 count, current_dec, remain;
	u32 time_taken, time_slice, time_left;

	current_dec = last_dec ? *last_dec : 0;
	
	gf_term_handle_services(term);

	gf_mx_p(term->mm_mx);

	count = gf_list_count(term->codecs);
	time_left = term->frame_duration;

	if (current_dec >= count) current_dec = 0;
	remain = count;

	/*this is ultra basic a nice scheduling system would be much better*/
	while (remain) {
		ce = (CodecEntry*)gf_list_get(term->codecs, current_dec);
		if (!ce) break;

		if (!(ce->flags & GF_MM_CE_RUNNING) || (ce->flags & GF_MM_CE_THREADED) ) {
			remain--;
			if (!remain) break;
			current_dec = (current_dec + 1) % count;
			continue;
		}
		time_slice = ce->dec->Priority * time_left / term->cumulated_priority;
		if (ce->dec->PriorityBoost) time_slice *= 2;
		time_taken = gf_sys_clock();

		e = gf_codec_process(ce->dec, time_slice);
		gf_mx_v(ce->mx);

		/*avoid signaling errors too often...*/
#ifndef GPAC_DISABLE_LOG
		if (e) GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[ODM%d] Decoding Error %s\n", ce->dec->odm->OD->objectDescriptorID, gf_error_to_string(e) ));
#endif

		time_taken = gf_sys_clock() - time_taken;

		if (ce->dec->CB && (ce->dec->CB->UnitCount >= ce->dec->CB->Min)) ce->dec->PriorityBoost = 0;

		remain -= 1;
		if (!remain) break;

		current_dec = (current_dec + 1) % count;

		if (time_left > time_taken) {
			time_left -= time_taken;
		} else {
			break;
		}
	}
	gf_mx_v(term->mm_mx);

	if (term->flags & GF_TERM_RENDER_FRAME) {
		time_taken = gf_sys_clock();
		gf_sr_render_frame(term->renderer);
		time_taken = gf_sys_clock() - time_taken;

		if (time_left>time_taken) 
			time_left -= time_taken;
		else
			time_left = 0;
	}
	if (!(term->user->init_flags & GF_TERM_NO_REGULATION)) gf_sleep(time_left);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Decode and render in %d / %d ms\n", term->frame_duration-time_left, term->frame_duration));
	return time_left;
}

u32 MM_Loop(void *par)
{
	u32 current_dec;
	GF_Terminal *term = (GF_Terminal *) par;

	gf_th_set_priority(term->mm_thread, term->priority);

	current_dec = 0;
	while (term->flags & GF_TERM_RUNNING) {
		MM_SimulationStep(term, &current_dec);
	}
	term->flags |= GF_TERM_DEAD;
	return 0;
}

u32 RunSingleDec(void *ptr)
{
	GF_Err e;
	u32 time_left;
	CodecEntry *ce = (CodecEntry *) ptr;

	while (ce->flags & GF_MM_CE_RUNNING) {
		time_left = gf_sys_clock();
		gf_mx_p(ce->mx);
		e = gf_codec_process(ce->dec, ce->dec->odm->term->frame_duration);
		if (e) gf_term_message(ce->dec->odm->term, ce->dec->odm->net_service->url, "Decoding Error", e);
		gf_mx_v(ce->mx);
		time_left = gf_sys_clock() - time_left;


		/*no priority boost this way for systems codecs, priority is dynamically set by not releasing the 
		graph when late and moving on*/
		if (!ce->dec->CB || (ce->dec->CB->UnitCount == ce->dec->CB->Capacity)) 
			ce->dec->PriorityBoost = 0;

		/*while on don't sleep*/
		if (ce->dec->PriorityBoost) continue;

		if (time_left) {
			while (time_left > ce->dec->odm->term->frame_duration) time_left -= ce->dec->odm->term->frame_duration;
			gf_sleep(time_left);
		} else {
			gf_sleep(ce->dec->odm->term->frame_duration);
		}
	}
	ce->flags |= GF_MM_CE_DEAD;
	return 0;
}

/*NOTE: when starting/stoping a decoder we only lock the decoder mutex, NOT the media manager. This
avoids deadlocking in case a system codec waits for the scene graph and the renderer requests 
a stop/start on a media*/
void gf_term_start_codec(GF_Codec *codec)
{
	GF_CodecCapability cap;
	CodecEntry *ce;
	GF_Terminal *term = codec->odm->term;
	ce = mm_get_codec(term->codecs, codec);
	if (!ce) return;

	/*lock dec*/
	if (ce->mx) gf_mx_p(ce->mx);

	/*clean decoder memory and wait for RAP*/
	if (codec->CB) gf_cm_reset(codec->CB);

	cap.CapCode = GF_CODEC_WAIT_RAP;
	gf_codec_set_capability(codec, cap);

	if (codec->decio && (codec->decio->InterfaceType == GF_SCENE_DECODER_INTERFACE)) {
		cap.CapCode = GF_CODEC_SHOW_SCENE;
		cap.cap.valueInt = 1;
		gf_codec_set_capability(codec, cap);
	}

	gf_codec_set_status(codec, GF_ESM_CODEC_PLAY);

	if (!(ce->flags & GF_MM_CE_RUNNING)) {
		ce->flags |= GF_MM_CE_RUNNING;
		if (ce->thread) {
			gf_th_run(ce->thread, RunSingleDec, ce);
			gf_th_set_priority(ce->thread, term->priority);
		} else {
			term->cumulated_priority += ce->dec->Priority+1;
		}
	}

	/*unlock dec*/
	if (ce->mx)
		gf_mx_v(ce->mx);
}

void gf_term_stop_codec(GF_Codec *codec)
{
	CodecEntry *ce;
	GF_Terminal *term = codec->odm->term;
	ce = mm_get_codec(term->codecs, codec);
	if (!ce) return;

	if (ce->mx) gf_mx_p(ce->mx);
	else gf_mx_p(term->mm_mx);
	
	if (codec->decio && codec->odm->mo && (codec->odm->mo->flags & GF_MO_DISPLAY_REMOVE) ) {
		GF_CodecCapability cap;
		cap.CapCode = GF_CODEC_SHOW_SCENE;
		cap.cap.valueInt = 0;
		gf_codec_set_capability(codec, cap);
		codec->odm->mo->flags &= ~GF_MO_DISPLAY_REMOVE;
	}

	/*set status directly and don't touch CB state*/
	codec->Status = GF_ESM_CODEC_STOP;
	/*don't wait for end of thread since this can be triggered within the decoding thread*/
	if (ce->flags & GF_MM_CE_RUNNING) {
		ce->flags &= ~GF_MM_CE_RUNNING;
		if (!ce->thread) 
			term->cumulated_priority -= codec->Priority+1;
	}

	if (ce->mx) gf_mx_v(ce->mx);
	else gf_mx_v(term->mm_mx);
}

void gf_term_set_threading(GF_Terminal *term, u32 mode)
{
	u32 i;
	Bool thread_it, restart_it;
	CodecEntry *ce;

	switch (mode) {
	case GF_TERM_THREAD_SINGLE: 
		if (term->flags & GF_TERM_SINGLE_THREAD) return;
		term->flags &= ~GF_TERM_MULTI_THREAD;
		term->flags |= GF_TERM_SINGLE_THREAD;
		break;
	case GF_TERM_THREAD_MULTI: 
		if (term->flags & GF_TERM_MULTI_THREAD) return;
		term->flags &= ~GF_TERM_SINGLE_THREAD;
		term->flags |= GF_TERM_MULTI_THREAD;
		break;
	default:
		if (!(term->flags & (GF_TERM_MULTI_THREAD | GF_TERM_SINGLE_THREAD) ) ) return;
		term->flags &= ~GF_TERM_SINGLE_THREAD;
		term->flags &= ~GF_TERM_MULTI_THREAD;
		break;
	}

	gf_mx_p(term->mm_mx);


	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
		thread_it = 0;
		/*free mode, decoder wants threading - do */
		if ((mode == GF_TERM_THREAD_FREE) && (ce->flags & GF_MM_CE_REQ_THREAD)) thread_it = 1;
		else if (mode == GF_TERM_THREAD_MULTI) thread_it = 1;

		if (thread_it && (ce->flags & GF_MM_CE_THREADED)) continue;
		if (!thread_it && !(ce->flags & GF_MM_CE_THREADED)) continue;

		restart_it = 0;
		if (ce->flags & GF_MM_CE_RUNNING) {
			restart_it = 1;
			ce->flags &= ~GF_MM_CE_RUNNING;
		}

		if (ce->flags & GF_MM_CE_THREADED) {
			/*wait for thread to die*/
			while (!(ce->flags & GF_MM_CE_DEAD)) gf_sleep(0);
			ce->flags &= ~GF_MM_CE_DEAD;
			gf_th_del(ce->thread);
			ce->thread = NULL;
			gf_mx_del(ce->mx);
			ce->mx = NULL;
			ce->flags &= ~GF_MM_CE_THREADED;
		} else {
			term->cumulated_priority -= ce->dec->Priority+1;
		}

		if (thread_it) {
			ce->flags |= GF_MM_CE_THREADED;
			ce->thread = gf_th_new();
			ce->mx = gf_mx_new();
		}

		if (restart_it) {
			ce->flags |= GF_MM_CE_RUNNING;
			if (ce->thread) {
				gf_th_run(ce->thread, RunSingleDec, ce);
				gf_th_set_priority(ce->thread, term->priority);
			} else {
				term->cumulated_priority += ce->dec->Priority+1;
			}
		}
	}
	gf_mx_v(term->mm_mx);
}

void gf_term_set_priority(GF_Terminal *term, s32 Priority)
{
	u32 i;
	CodecEntry *ce;
	gf_mx_p(term->mm_mx);

	gf_th_set_priority(term->mm_thread, Priority);

	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
		if (ce->flags & GF_MM_CE_THREADED)
			gf_th_set_priority(ce->thread, Priority);
	}
	term->priority = Priority;
	gf_mx_v(term->mm_mx);
}

GF_EXPORT
GF_Err gf_term_process_step(GF_Terminal *term)
{
	if (!(term->flags & GF_TERM_RENDER_FRAME)) return GF_BAD_PARAM;

	MM_SimulationStep(term, NULL);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_term_process_flush(GF_Terminal *term)
{
	u32 i;
	CodecEntry *ce;
	if (!(term->flags & GF_TERM_RENDER_FRAME) ) return GF_BAD_PARAM;

	/*update till frame mature*/
	while (1) {
		gf_term_handle_services(term);
		gf_mx_p(term->mm_mx);

		i=0;
		while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
			gf_codec_process(ce->dec, 10000);
		}
		gf_mx_v(term->mm_mx);

		if (!gf_sr_render_frame(term->renderer))
			break;

		if (! (term->user->init_flags & GF_TERM_NO_REGULATION))
			break;
	}
	return GF_OK;
}

