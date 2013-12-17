/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/internal/compositor_dev.h>


u32 MM_Loop(void *par);


enum
{
	GF_MM_CE_RUNNING= 1,
	GF_MM_CE_HAS_ERROR = 1<<1,
	GF_MM_CE_THREADED = 1<<2,
	GF_MM_CE_REQ_THREAD = 1<<3,
	/*only used by threaded decs to signal end of thread*/
	GF_MM_CE_DEAD = 1<<4,
	GF_MM_CE_DISCARDED = 1<<5,
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
	term->mm_mx = gf_mx_new("MediaManager");
	term->codecs = gf_list_new();

	term->frame_duration = 33;
	switch (threading_mode) {
	case GF_TERM_THREAD_SINGLE: term->flags |= GF_TERM_SINGLE_THREAD;
		break;
	case GF_TERM_THREAD_MULTI: 
		term->flags |= GF_TERM_MULTI_THREAD;
		break;
	default:
		break;
	}

	if (term->user->init_flags & GF_TERM_NO_DECODER_THREAD) 
		return GF_OK;

	term->mm_thread = gf_th_new("MediaManager");
	term->flags |= GF_TERM_RUNNING;
	term->priority = GF_THREAD_PRIORITY_NORMAL;
	gf_th_run(term->mm_thread, MM_Loop, term);
	return GF_OK;
}

void gf_term_stop_scheduler(GF_Terminal *term)
{
	if (term->mm_thread) {
		u32 count, i;

		term->flags &= ~GF_TERM_RUNNING;
		while (!(term->flags & GF_TERM_DEAD) ) 
			gf_sleep(2);

		count = gf_list_count(term->codecs);
		for (i=0; i<count; i++) {
			CodecEntry *ce = gf_list_get(term->codecs, i);
			if (ce->flags & GF_MM_CE_DISCARDED) {
				gf_free(ce);
				gf_list_rem(term->codecs, i);
				count--;
				i--;
			}
		}

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
	Bool locked;
	Bool threaded;
	CodecEntry *cd;
	CodecEntry *ptr, *next;
	GF_CodecCapability cap;
	assert(codec);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Registering codec %s\n", codec->decio ? codec->decio->module_name : "RAW"));

	/*caution: the mutex can be grabbed by a decoder waiting for a mutex owned by the calling thread
	this happens when several scene codecs are running concurently and triggering play/pause on media*/
	locked = gf_mx_try_lock(term->mm_mx);

	cd = mm_get_codec(term->codecs, codec);
	if (cd) goto exit;

	GF_SAFEALLOC(cd, CodecEntry);
	cd->dec = codec;
	if (!cd->dec->Priority) 
		cd->dec->Priority = 1;
	
	/*we force audio codecs to be threaded in free mode, so that we avoid waiting in the audio renderer if another decoder is locking the main mutex
	this can happen when the audio decoder is running late*/
	if (codec->type==GF_STREAM_AUDIO) {
		threaded = 1;
	} else {
		cap.CapCode = GF_CODEC_WANTS_THREAD;
		cap.cap.valueInt = 0;
		gf_codec_get_capability(codec, &cap);
		threaded = cap.cap.valueInt;
	}
	
	if (threaded) cd->flags |= GF_MM_CE_REQ_THREAD;


	if (term->flags & GF_TERM_MULTI_THREAD) {
		if ((codec->type==GF_STREAM_AUDIO) || (codec->type==GF_STREAM_VISUAL)) threaded = 1;
	} else if (term->flags & GF_TERM_SINGLE_THREAD) {
		threaded = 0;
	}
	if (codec->flags & GF_ESM_CODEC_IS_RAW_MEDIA) 
		threaded = 0;
	
	if (threaded) {
		cd->thread = gf_th_new(cd->dec->decio->module_name);
		cd->mx = gf_mx_new(cd->dec->decio->module_name);
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
	if (locked) gf_mx_v(term->mm_mx);
	return;
}

void gf_term_remove_codec(GF_Terminal *term, GF_Codec *codec)
{
	u32 i;
	Bool locked;
	CodecEntry *ce;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Unregistering codec %s\n", codec->decio ? codec->decio->module_name : "RAW"));

	/*cf note above*/
	locked = gf_mx_try_lock(term->mm_mx);

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
		if (locked) {
			gf_free(ce);
			gf_list_rem(term->codecs, i-1);
		} else {
			ce->flags |= GF_MM_CE_DISCARDED;
		}
		break;
	}
	if (locked) gf_mx_v(term->mm_mx);
	return;
}

Bool gf_term_find_codec(GF_Terminal *term, GF_Codec *codec)
{
	CodecEntry *ce;
	u32 i=0;
	while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
		if (ce->dec == codec) return 1;
	}
	return 0;
}

static u32 MM_SimulationStep_Decoder(GF_Terminal *term)
{
	CodecEntry *ce;
	GF_Err e;
	u32 count, remain;
	u32 time_taken, time_slice, time_left;
	
#ifndef GF_DISABLE_LOG
	term->compositor->networks_time = gf_sys_clock();
#endif

//	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Media Manager] Entering simultion step\n"));
	gf_term_handle_services(term);

#ifndef GF_DISABLE_LOG
	term->compositor->networks_time = gf_sys_clock() - term->compositor->networks_time;
#endif

#ifndef GF_DISABLE_LOG
	term->compositor->decoders_time = gf_sys_clock();
#endif
	gf_mx_p(term->mm_mx);

	count = gf_list_count(term->codecs);
	time_left = term->frame_duration;

	if (term->last_codec >= count) term->last_codec = 0;
	remain = count;
	time_taken = 0;
	/*this is ultra basic a nice scheduling system would be much better*/
	while (remain) {
		ce = (CodecEntry*)gf_list_get(term->codecs, term->last_codec);
		if (!ce) break;

		if (!(ce->flags & GF_MM_CE_RUNNING) || (ce->flags & GF_MM_CE_THREADED) || ce->dec->force_cb_resize) {
			remain--;
			if (!remain) break;
			term->last_codec = (term->last_codec + 1) % count;
			continue;
		}
		time_slice = ce->dec->Priority * time_left / term->cumulated_priority;
		if (ce->dec->PriorityBoost) time_slice *= 2;
		time_taken = gf_sys_clock();

		e = gf_codec_process(ce->dec, time_slice);
		time_taken = gf_sys_clock() - time_taken;
		/*avoid signaling errors too often...*/
#ifndef GPAC_DISABLE_LOG
		if (e) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[ODM%d] Decoding Error %s\n", ce->dec->odm->OD->objectDescriptorID, gf_error_to_string(e) ));
		} else {
			//GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[%s] Decode time slice %d ms out of %d ms\n", ce->dec->decio ? ce->dec->decio->module_name : "RAW", time_taken, time_left ));
		}
#endif
		if (ce->flags & GF_MM_CE_DISCARDED) {
			gf_free(ce);
			gf_list_rem(term->codecs, term->last_codec);
			count--;
			if (!count)
				break;
		} else {
			if (ce->dec->CB && (ce->dec->CB->UnitCount >= ce->dec->CB->Min)) ce->dec->PriorityBoost = 0;
		}
		term->last_codec = (term->last_codec + 1) % count;

		remain -= 1;
		if (time_left > time_taken) {
			time_left -= time_taken;
			if (!remain) break;
		} else {
			time_left = 0;
			break;
		}
	}
	gf_mx_v(term->mm_mx);
#ifndef GF_DISABLE_LOG
	term->compositor->decoders_time = gf_sys_clock() - term->compositor->decoders_time;
#endif

	return time_left;
}

u32 MM_Loop(void *par)
{
	GF_Terminal *term = (GF_Terminal *) par;
	Bool do_scene = (term->flags & GF_TERM_NO_VISUAL_THREAD) ? 1 : 0;
	Bool do_codec = (term->flags & GF_TERM_NO_DECODER_THREAD) ? 0 : 1;
	Bool do_regulate = (term->user->init_flags & GF_TERM_NO_REGULATION) ? 0 : 1;

	gf_th_set_priority(term->mm_thread, term->priority);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[MediaManager] Entering thread ID %d\n", gf_th_id() ));
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("(RTI] Terminal Cycle Log\tServices\tDecoders\tCompositor\tSleep\n"));

	while (term->flags & GF_TERM_RUNNING) {
		u32 left = 0;
		if (do_codec) left = MM_SimulationStep_Decoder(term);
		else left = term->frame_duration;
		
		if (do_scene) {
			u32 time_taken = gf_sys_clock();
			gf_sc_draw_frame(term->compositor);
			time_taken = gf_sys_clock() - time_taken;
			if (left>time_taken) 
				left -= time_taken;
			else
				left = 0;
		}

		if (do_regulate) {
			if (left==term->frame_duration) {
				gf_sleep(term->frame_duration);
			}
		}
	}
	term->flags |= GF_TERM_DEAD;
	return 0;
}

u32 RunSingleDec(void *ptr)
{
	GF_Err e;
	u32 time_left;
	CodecEntry *ce = (CodecEntry *) ptr;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[MediaDecoder %d] Entering thread ID %d\n", ce->dec->odm->OD->objectDescriptorID, gf_th_id() ));

	while (ce->flags & GF_MM_CE_RUNNING) {
		time_left = gf_sys_clock();
		if (!ce->dec->force_cb_resize) {
			gf_mx_p(ce->mx);
			e = gf_codec_process(ce->dec, ce->dec->odm->term->frame_duration);
			if (e) gf_term_message(ce->dec->odm->term, ce->dec->odm->net_service->url, "Decoding Error", e);
			gf_mx_v(ce->mx);
		}
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
avoids deadlocking in case a system codec waits for the scene graph and the compositor requests 
a stop/start on a media*/
void gf_term_start_codec(GF_Codec *codec, Bool is_resume)
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

	if (!is_resume) {
		cap.CapCode = GF_CODEC_WAIT_RAP;
		gf_codec_set_capability(codec, cap);

		if (codec->decio && (codec->decio->InterfaceType == GF_SCENE_DECODER_INTERFACE)) {
			cap.CapCode = GF_CODEC_SHOW_SCENE;
			cap.cap.valueInt = 1;
			gf_codec_set_capability(codec, cap);
		}
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

void gf_term_stop_codec(GF_Codec *codec, Bool is_pause)
{
	GF_CodecCapability cap;
	Bool locked = 0;
	CodecEntry *ce;
	GF_Terminal *term = codec->odm->term;
	ce = mm_get_codec(term->codecs, codec);
	if (!ce) return;

	if (ce->mx) gf_mx_p(ce->mx);
	/*We must make sure:
		1- media codecs are synchrounously stop otherwise we could destroy the composition memory while 
	the codec writes to it
		2- prevent deadlock for other codecs waiting for the scene graph
	*/
	else if (codec->CB) {
		locked = 1;
		gf_mx_p(term->mm_mx);
	} else {
		locked = gf_mx_try_lock(term->mm_mx);
	}

	if (!is_pause) {
		cap.CapCode = GF_CODEC_ABORT;
		cap.cap.valueInt = 0;
		gf_codec_set_capability(codec, cap);
		
		if (codec->decio && codec->odm->mo && (codec->odm->mo->flags & GF_MO_DISPLAY_REMOVE) ) {
			cap.CapCode = GF_CODEC_SHOW_SCENE;
			cap.cap.valueInt = 0;
			gf_codec_set_capability(codec, cap);
			codec->odm->mo->flags &= ~GF_MO_DISPLAY_REMOVE;
		}
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
	/*cf note above*/
	else if (locked) gf_mx_v(term->mm_mx);
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
			while (!(ce->flags & GF_MM_CE_DEAD)) gf_sleep(1);
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
			ce->thread = gf_th_new(ce->dec->decio->module_name);
			ce->mx = gf_mx_new(ce->dec->decio->module_name);
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

Bool gf_term_lock_codec(GF_Codec *codec, Bool lock, Bool trylock)
{
	Bool res = 1;
	CodecEntry *ce;
	GF_Terminal *term = codec->odm->term;

	ce = mm_get_codec(term->codecs, codec);
	if (!ce) return 0;

	if (ce->mx) {
		if (lock) {
			if (trylock) {
				res = gf_mx_try_lock(ce->mx);
			} else {
				res = gf_mx_p(ce->mx);
			}
		}
		else gf_mx_v(ce->mx);
	}
	else {
		if (lock) {
			if (trylock) {
				res = gf_mx_try_lock(term->mm_mx);
			} else {
				res = gf_mx_p(term->mm_mx);
			}
		}
		else gf_mx_v(term->mm_mx);
	}
	return res;
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
u32 gf_term_process_step(GF_Terminal *term)
{
	u32 time_taken = gf_sys_clock();

	if (term->flags & GF_TERM_NO_DECODER_THREAD) {
		MM_SimulationStep_Decoder(term);
	} 

	if (term->flags & GF_TERM_NO_COMPOSITOR_THREAD) {
		gf_sc_draw_frame(term->compositor);
	}
	time_taken = gf_sys_clock() - time_taken;
	if (time_taken > term->compositor->frame_duration) {
		time_taken = 0;
	} else {
		time_taken = term->compositor->frame_duration - time_taken;
	}
	if (term->user->init_flags & GF_TERM_NO_REGULATION) return time_taken;

	if (2*time_taken >= term->compositor->frame_duration) {
		gf_sleep(time_taken);
	}
	return time_taken;
}

GF_EXPORT
GF_Err gf_term_process_flush(GF_Terminal *term)
{
	u32 i;
	CodecEntry *ce;
	if (!(term->flags & GF_TERM_NO_COMPOSITOR_THREAD) ) return GF_BAD_PARAM;

	/*update till frame mature*/
	while (1) {
		
		if (term->flags & GF_TERM_NO_DECODER_THREAD) {
			gf_term_handle_services(term);
			gf_mx_p(term->mm_mx);
			i=0;
			while ((ce = (CodecEntry*)gf_list_enum(term->codecs, &i))) {
				gf_codec_process(ce->dec, 10000);
			}
			gf_mx_v(term->mm_mx);
		}

		if (!gf_sc_draw_frame(term->compositor))
			break;

		if (! (term->user->init_flags & GF_TERM_NO_REGULATION))
			break;
	}
	return GF_OK;
}

