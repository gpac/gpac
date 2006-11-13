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
	MM_CE_IDLE = 0,
	MM_CE_ACTIVE,
	MM_CE_DEAD,
};

typedef struct
{
	GF_Codec *dec;
	u32 state;
	GF_MediaManager *mm;
	Bool has_error, req_thread;

	/*for threaded decoders*/
	GF_Thread *thread;
	GF_Mutex *mx;
} CodecEntry;

GF_MediaManager *gf_mm_new(GF_Terminal *term, u32 threading_mode)
{
	GF_MediaManager *tmp;
	GF_SAFEALLOC(tmp, GF_MediaManager);
	tmp->mx = gf_mx_new();
	tmp->threaded_codecs = gf_list_new();
	tmp->unthreaded_codecs = gf_list_new();
	tmp->term = term;

	tmp->interrupt_cycle_ms = 33;
	tmp->threading_mode = threading_mode;
	if (term->user->init_flags & GF_TERM_NOT_THREADED) return tmp;

	tmp->th = gf_th_new();
	tmp->state = 1;
	tmp->priority = GF_THREAD_PRIORITY_NORMAL;
	gf_th_run(tmp->th, MM_Loop, tmp);
	return tmp;
}

void gf_mm_del(GF_MediaManager *mgr)
{
	if (mgr->th) {
		mgr->state = 0;
		while (mgr->state!=2) 
			gf_sleep(0);

		assert(! gf_list_count(mgr->threaded_codecs));
		assert(! gf_list_count(mgr->unthreaded_codecs));
		gf_th_del(mgr->th);
	}

	gf_list_del(mgr->threaded_codecs);
	gf_list_del(mgr->unthreaded_codecs);
	gf_mx_del(mgr->mx);
	free(mgr);
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


void gf_mm_add_codec(GF_MediaManager *mgr, GF_Codec *codec)
{
	u32 i, count;
	Bool threaded;
	CodecEntry *cd;
	CodecEntry *ptr, *next;
	GF_CodecCapability cap;
	assert(codec);

	/*we need REAL exclusive access when adding a dec*/
	gf_mx_p(mgr->mx);

	cd = mm_get_codec(mgr->unthreaded_codecs, codec);
	if (cd) goto exit;

	GF_SAFEALLOC(cd, CodecEntry);
	cd->dec = codec;
	cd->mm = mgr;

	cap.CapCode = GF_CODEC_WANTS_THREAD;
	cap.cap.valueInt = 0;
	gf_codec_get_capability(codec, &cap);
	cd->req_thread = threaded = cap.cap.valueInt;

	switch (mgr->threading_mode) {
	case GF_TERM_THREAD_MULTI:
		if ((codec->type==0x04) || (codec->type==0x05)) threaded = 1;
		break;
	case GF_TERM_THREAD_FREE:
	default:
		break;
	/*all codecs in MM thread*/
	case GF_TERM_THREAD_SINGLE: 
		threaded = 0;
		break;
	}
	
	if (threaded) {
		cd->thread = gf_th_new();
		cd->mx = gf_mx_new();
		cd->state = MM_CE_IDLE;
		gf_list_add(mgr->threaded_codecs, cd);
		goto exit;
	}

	//add codec 1- per priority 2- per type, audio being first
	//priorities inherits from Systems (5bits) so range from 0 to 31
	//we sort from MAX to MIN
	count = gf_list_count(mgr->unthreaded_codecs);
	for (i=0; i<count; i++) {
		ptr = (CodecEntry*)gf_list_get(mgr->unthreaded_codecs, i);
		//higher priority, continue
		if (ptr->dec->Priority > codec->Priority) continue;

		//same priority, put audio first
		if (ptr->dec->Priority == codec->Priority) {
			//we insert audio (0x05) before video (0x04)
			if (ptr->dec->type < codec->type) {
				gf_list_insert(mgr->unthreaded_codecs, cd, i);
				goto exit;
			}
			//same prior, same type: insert after
			if (ptr->dec->type == codec->type) {
				if (i+1==count) {
					gf_list_add(mgr->unthreaded_codecs, cd);
				} else {
					gf_list_insert(mgr->unthreaded_codecs, cd, i+1);
				}
				goto exit;
			}
			//we insert video (0x04) after audio (0x05) if next is not audio
			//last one
			if (i+1 == count) {
				gf_list_add(mgr->unthreaded_codecs, cd);
				goto exit;
			}
			next = (CodecEntry*)gf_list_get(mgr->unthreaded_codecs, i+1);
			//# priority level, insert
			if (next->dec->Priority != codec->Priority) {
				gf_list_insert(mgr->unthreaded_codecs, cd, i+1);
				goto exit;
			}
			//same priority level and at least one after : continue
			continue;
		}
		gf_list_insert(mgr->unthreaded_codecs, cd, i);
		goto exit;
	}
	//if we got here, nothing in the chain -> add the codec
	gf_list_add(mgr->unthreaded_codecs, cd);

exit:
	gf_mx_v(mgr->mx);
	return;
}

void gf_mm_remove_codec(GF_MediaManager *mgr, GF_Codec *codec)
{
	u32 i;
	CodecEntry *ce;

	/*we need REAL exclusive access when removing a dec*/
	gf_mx_p(mgr->mx);

	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(mgr->threaded_codecs, &i))) {
		if (ce->dec == codec) {
			assert(ce->thread);
			if (ce->state == MM_CE_ACTIVE) {
				ce->state = MM_CE_IDLE;
				while (ce->state!=MM_CE_DEAD) gf_sleep(10);
			}
			gf_th_del(ce->thread);
			gf_mx_del(ce->mx);
			free(ce);
			gf_list_rem(mgr->threaded_codecs, i-1);
			goto exit;
		}
	}
	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(mgr->unthreaded_codecs, &i))) {
		if (ce->dec == codec) {
			assert(!ce->thread);
			free(ce);
			gf_list_rem(mgr->unthreaded_codecs, i-1);
			break;
		}
	}
exit:
	gf_mx_v(mgr->mx);
	return;
}


u32 MM_Loop(void *par)
{
	CodecEntry *ce;
	GF_Err e;
	u32 count, current_dec, remain;
	u32 time_taken, time_slice, time_left;

	GF_MediaManager *mgr = (GF_MediaManager *) par;

	current_dec = 0;


	gf_th_set_priority(mgr->th, mgr->priority);

	while (mgr->state) {
		gf_term_handle_services(mgr->term);
		gf_mx_p(mgr->mx);

		count = gf_list_count(mgr->unthreaded_codecs);
		time_left = mgr->interrupt_cycle_ms;

		if (!count) {
			gf_mx_v(mgr->mx);
			gf_sleep(mgr->interrupt_cycle_ms);
			continue;
		}

		if (current_dec >= count) current_dec = 0;
		remain = count;

		/*this is ultra basic a nice scheduling system would be much better*/
		while (remain) {
			ce = (CodecEntry*)gf_list_get(mgr->unthreaded_codecs, current_dec);
			if (!ce) break;

			if (!ce->state) {
				remain--;
				if (!remain) break;
				current_dec = (current_dec + 1) % count;
				continue;
			}
			time_slice = ce->dec->Priority * time_left / mgr->cumulated_priority;
			if (ce->dec->PriorityBoost) time_slice *= 2;
			time_taken = gf_sys_clock();

			e = gf_codec_process(ce->dec, time_slice);
			gf_mx_v(ce->mx);

			/*avoid signaling errors too often...*/
			if (e && !ce->has_error) {
				gf_term_message(ce->dec->odm->term, ce->dec->odm->net_service->url, "Decoding Error", e);
				ce->has_error = 1;
			}

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
		gf_mx_v(mgr->mx);

		if (mgr->term->render_frames) {
			time_taken = gf_sys_clock();
			gf_sr_render_frame(mgr->term->renderer);
			time_taken = gf_sys_clock() - time_taken;

			if (time_left>time_taken) 
				time_left -= time_taken;
			else
				time_left = 0;
		}

		if (!count) {
			gf_sleep(mgr->interrupt_cycle_ms);
		} else {
//			while (time_left > mgr->interrupt_cycle_ms) time_left -= mgr->interrupt_cycle_ms;
			gf_sleep(time_left);
		}
	}
	mgr->state = 2;
	return 0;
}

u32 RunSingleDec(void *ptr)
{
	GF_Err e;
	u32 time_left;
	CodecEntry *ce = (CodecEntry *) ptr;

	while (ce->state) {
		time_left = gf_sys_clock();
		gf_mx_p(ce->mx);
		e = gf_codec_process(ce->dec, ce->mm->interrupt_cycle_ms);
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
			while (time_left > ce->mm->interrupt_cycle_ms) time_left -= ce->mm->interrupt_cycle_ms;
			gf_sleep(time_left);
		} else {
			gf_sleep(ce->mm->interrupt_cycle_ms);
		}
	}
	ce->state = MM_CE_DEAD;
	return 0;
}

/*NOTE: when starting/stoping a decoder we only lock the decoder mutex, NOT the media manager. This
avoids deadlocking in case a system codec waits for the scene graph and the renderer requests 
a stop/start on a media*/
void gf_mm_start_codec(GF_Codec *codec)
{
	GF_CodecCapability cap;
	CodecEntry *ce;
	GF_MediaManager *mgr = codec->odm->term->mediaman;
	ce = mm_get_codec(mgr->unthreaded_codecs, codec);
	if (!ce) ce = mm_get_codec(mgr->threaded_codecs, codec);
	if (!ce) return;

	/*lock dec*/
	if (ce->mx) gf_mx_p(ce->mx);
	ce->has_error = 0;

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

	if (ce->state!=MM_CE_ACTIVE) {
		if (ce->thread) {
			ce->state = MM_CE_ACTIVE;
			gf_th_run(ce->thread, RunSingleDec, ce);
			gf_th_set_priority(ce->thread, mgr->priority);
		} else {
			ce->state = MM_CE_ACTIVE;
			mgr->cumulated_priority += ce->dec->Priority+1;
		}
	}

	/*unlock dec*/
	if (ce->mx)
		gf_mx_v(ce->mx);
}

void gf_mm_stop_codec(GF_Codec *codec)
{
	CodecEntry *ce;
	GF_MediaManager *mgr = codec->odm->term->mediaman;
	ce = mm_get_codec(mgr->unthreaded_codecs, codec);
	if (!ce) ce = mm_get_codec(mgr->threaded_codecs, codec);
	if (!ce) return;

	if (ce->mx) gf_mx_p(ce->mx);
	else gf_mx_p(mgr->mx);
	
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
	if (ce->state == MM_CE_ACTIVE) {
		ce->state = MM_CE_IDLE;
		if (!ce->thread) 
			mgr->cumulated_priority -= codec->Priority+1;
	}

	if (ce->mx) gf_mx_v(ce->mx);
	else gf_mx_v(mgr->mx);
}

void gf_mm_set_threading(GF_MediaManager *mgr, u32 mode)
{
	u32 i, prev_state;
	CodecEntry *ce;

	if (mgr->threading_mode == mode) return;

	/*note we lock global mutex but don't lock any codecs*/
	gf_mx_p(mgr->mx);

	switch (mode) {
	/*moving to no threads*/
	case GF_TERM_THREAD_SINGLE:
		while (gf_list_count(mgr->threaded_codecs)) {
			CodecEntry *ce = (CodecEntry*)gf_list_get(mgr->threaded_codecs, 0);
			gf_list_rem(mgr->threaded_codecs, 0);

			prev_state = ce->state;
			/*stop thread*/
			if (ce->state==MM_CE_ACTIVE) {
				ce->state = 0;
				while (ce->state != MM_CE_DEAD) gf_sleep(0);
			}
			if (prev_state==MM_CE_ACTIVE) {
				mgr->cumulated_priority += ce->dec->Priority+1;
				ce->state = MM_CE_ACTIVE;
			} else {
				ce->state = MM_CE_IDLE;
			}
			gf_th_del(ce->thread);
			ce->thread = NULL;
			gf_mx_del(ce->mx);
			ce->mx = NULL;
			gf_list_add(mgr->unthreaded_codecs, ce);
		}
		break;
	/*moving to all threads*/
	case GF_TERM_THREAD_MULTI:
		while (gf_list_count(mgr->unthreaded_codecs)) {
			CodecEntry *ce = (CodecEntry*)gf_list_get(mgr->unthreaded_codecs, 0);
			gf_list_rem(mgr->unthreaded_codecs, 0);
			if (ce->state==MM_CE_ACTIVE) mgr->cumulated_priority -= ce->dec->Priority+1;

			ce->thread = gf_th_new();
			ce->mx = gf_mx_new();
			gf_list_add(mgr->threaded_codecs, ce);
			if (ce->state) {
				gf_th_run(ce->thread, RunSingleDec, ce);
				gf_th_set_priority(ce->thread, mgr->priority);
			}
		}
		break;
	/*moving to free threading*/
	case GF_TERM_THREAD_FREE:
	default:
		/*remove all forced-threaded dec*/
		i=0;
		while ((ce = (CodecEntry*)gf_list_enum(mgr->threaded_codecs, &i))) {
			if (ce->req_thread) continue;

			gf_list_rem(mgr->threaded_codecs, i-1);
			i--;

			/*stop it*/
			prev_state = ce->state;
			/*stop thread*/
			if (ce->state==MM_CE_ACTIVE) {
				ce->state = 0;
				while (ce->state != MM_CE_DEAD) gf_sleep(0);
			}
			if (prev_state==MM_CE_ACTIVE) {
				mgr->cumulated_priority += ce->dec->Priority+1;
				ce->state = MM_CE_ACTIVE;
			} else {
				ce->state = MM_CE_IDLE;
			}
			gf_th_del(ce->thread);
			ce->thread = NULL;
			gf_mx_del(ce->mx);
			ce->mx = NULL;
			gf_list_add(mgr->unthreaded_codecs, ce);
		}
		/*remove all forced unthreaded dec*/
		i=0;
		while ((ce = (CodecEntry*)gf_list_enum(mgr->unthreaded_codecs, &i)) ) {
			if (! ce->req_thread) continue;
			gf_list_rem(mgr->unthreaded_codecs, i-1);
			i--;
			if (ce->state==MM_CE_ACTIVE) mgr->cumulated_priority -= ce->dec->Priority+1;

			/*add to unthreaded list*/
			gf_list_add(mgr->threaded_codecs, ce);

			ce->thread = gf_th_new();
			ce->mx = gf_mx_new();
			gf_list_add(mgr->threaded_codecs, ce);
			if (ce->state) {
				gf_th_run(ce->thread, RunSingleDec, ce);
				gf_th_set_priority(ce->thread, mgr->priority);
			}
		}
	}
	mgr->threading_mode = mode;
	gf_mx_v(mgr->mx);
}

void gf_mm_set_priority(GF_MediaManager *mgr, s32 Priority)
{
	u32 i;
	CodecEntry *ce;
	gf_mx_p(mgr->mx);

	gf_th_set_priority(mgr->th, Priority);

	i=0;
	while ((ce = (CodecEntry*)gf_list_enum(mgr->threaded_codecs, &i))) {
		gf_th_set_priority(ce->thread, Priority);
	}
	mgr->priority = Priority;

	gf_mx_v(mgr->mx);
}


GF_Err gf_term_process(GF_Terminal *term)
{
	GF_Err e;
	u32 i;
	CodecEntry *ce;
	if(term->render_frames!=2) return GF_BAD_PARAM;

	/*update till frame mature*/
	while (1) {
		gf_term_handle_services(term);
		gf_mx_p(term->mediaman->mx);

		i=0;
		while ((ce = (CodecEntry*)gf_list_enum(term->mediaman->unthreaded_codecs, &i))) {
			e = gf_codec_process(ce->dec, 10000);
			/*avoid signaling errors too often...*/
			if (e && !ce->has_error) {
				gf_term_message(ce->dec->odm->term, ce->dec->odm->net_service->url, "Decoding Error", e);
				ce->has_error = 1;
			}

		}
		gf_mx_v(term->mediaman->mx);

		if (!gf_sr_render_frame(term->renderer))
			break;
	}
	return GF_OK;
}

