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
#include <gpac/constants.h>
#include <gpac/network.h>
#include "media_memory.h"
#include "media_control.h"

GF_DBUnit *gf_db_unit_new()
{
	GF_DBUnit *tmp;
	GF_SAFEALLOC(tmp, GF_DBUnit)
	if (tmp) memset(tmp, 0, sizeof(GF_DBUnit));
	return tmp;
}

void gf_db_unit_del(GF_DBUnit *db)
{
	while (db) {
		GF_DBUnit *next = db->next;
		if (db->data) gf_free(db->data);
		gf_free(db);
		db = next;
	}
}

static GF_CMUnit *gf_cm_unit_new()
{
	GF_CMUnit *tmp;
	GF_SAFEALLOC(tmp, GF_CMUnit)
	if (tmp) memset(tmp, 0, sizeof(GF_CMUnit));
	return tmp;
}

#ifdef _WIN32_WCE
#include <winbase.h>
static GFINLINE void *my_large_alloc(u32 size) {
	void *ptr;
	if (!size) return NULL;
	ptr = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!ptr) {
		DWORD res = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Cannot resize composition buffer to %d bytes - error %d", size, res));
		return NULL;
	}
	return ptr;
}
static GFINLINE void my_large_gf_free(void *ptr) {
	if (ptr) VirtualFree(ptr, 0, MEM_RELEASE);
}
#else
#define my_large_alloc(_size)	(_size ? gf_malloc(sizeof(char)*_size) : NULL)
#define my_large_gf_free(_ptr)	gf_free(_ptr)
#endif


static void gf_cm_unit_del(GF_CMUnit *cb, Bool no_data_allocation)
{
	if (!cb)
		return;
	if (cb->next) gf_cm_unit_del(cb->next, no_data_allocation);

	if (cb->data) {
		if (!no_data_allocation) {
			my_large_gf_free(cb->data);
		}
		cb->data = NULL;
	}
	if (cb->frame) {
		cb->frame->Release(cb->frame);
		cb->frame=NULL;
	}
	gf_free(cb);
}

GF_CompositionMemory *gf_cm_new(u32 UnitSize, u32 capacity, Bool no_allocation)
{
	GF_CompositionMemory *tmp;
	GF_CMUnit *cu, *prev;
	u32 i;
	if (!capacity) return NULL;

	GF_SAFEALLOC(tmp, GF_CompositionMemory)
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate composition memory\n"));
		return NULL;
	}

	tmp->Capacity = capacity;
	tmp->UnitSize = UnitSize;
	tmp->no_allocation = no_allocation;

	prev = NULL;
	i = 1;
	while (capacity) {
		cu = gf_cm_unit_new();
		if (!prev) {
			tmp->input = cu;
		} else {
			prev->next = cu;
			cu->prev = prev;
		}
		cu->dataLength = 0;
		if (no_allocation) {
			cu->data = NULL;
		} else {
			cu->data = UnitSize ? (char*)my_large_alloc(sizeof(char)*UnitSize) : NULL;
			if (cu->data) memset(cu->data, 0, sizeof(char)*UnitSize);
		}
		prev = cu;
		capacity --;
		i++;
	}
	cu->next = tmp->input;
	tmp->input->prev = cu;

	/*close the loop. The output is the input as the first item
	that will be ready for composition will be filled in the input*/
	tmp->output = tmp->input;

	tmp->Status = CB_STOP;
	return tmp;
}

void gf_cm_del(GF_CompositionMemory *cb)
{
	gf_odm_lock(cb->odm, 1);
	/*may happen when CB is destroyed right after creation */
	if (cb->Status == CB_BUFFER) {
		gf_clock_buffer_off(cb->odm->codec->ck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB destroy - ODM%d: buffering off at OTB %u (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck), gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));
	}
	if (cb->input) {
		/*break the loop and destroy*/
		cb->input->prev->next = NULL;
		gf_cm_unit_del(cb->input, cb->no_allocation);
		cb->input = NULL;
	}
	gf_odm_lock(cb->odm, 0);
	gf_free(cb);
}

void gf_cm_rewind_input(GF_CompositionMemory *cb)
{
	if (cb->UnitCount) {
		cb->UnitCount--;
		cb->input = cb->input->prev;
		cb->input->dataLength = 0;
	}
}

/*access to the input buffer - return NULL if no input is available (buffer full)*/
GF_CMUnit *gf_cm_lock_input(GF_CompositionMemory *cb, u32 TS, Bool codec_reordering)
{
	GF_CMUnit *cu;
	if (codec_reordering) {
		/*there is still something in the input buffer*/
		if (cb->input->dataLength) {
			if (cb->input->TS==TS)
				return cb->input;
			return NULL;
		}
		cb->input->TS = TS;
		return cb->input;
	}

	/*spatial scalable, go backward to fetch same TS*/
	cu = cb->input;
	while (1) {
		if (cu->TS == TS)
			return cu;
		cu = cu->prev;
		if (cu == cb->input) break;
	}
	/*temporal scalable (find empty slot)*/
	cu = cb->input;
	while (1) {
		if (!cu->dataLength) {
//			assert(!cu->TS || (cb->Capacity==1));
			cu->TS = TS;
			return cu;
		}
		cu = cu->next;
		if (cu == cb->input) return NULL;
	}
	return NULL;
}

#if 0
static void check_temporal(GF_CompositionMemory *cb)
{
	GF_CMUnit *cu;
	cu = cb->output;
	while (1) {
		if (cu->next==cb->output) break;
		assert(!cu->next->dataLength || (cu->TS < cu->next->TS));
		assert(!cu(>TS || (cu->TS >= cb->LastRenderedTS));
		       cu = cu->next;
	}
}
#endif

/*re-orders units in case of temporal scalability. Blocking call as it may change the output unit too*/
static GF_CMUnit *gf_cm_reorder_unit(GF_CompositionMemory *cb, GF_CMUnit *unit, Bool codec_reordering)
{
	GF_CMUnit *cu;
	if (codec_reordering) {
		cu = cb->input;
		cb->input = cb->input->next;
		return cu;
	}

	/*lock the buffer since we may move pointers*/
	gf_odm_lock(cb->odm, 1);

	/*1- unit is next on time axis (no temporal scalability)*/
	if (!cb->input->dataLength || cb->input->TS < unit->TS) {

		if (unit != cb->input) {
			/*remove unit from the list*/
			unit->prev->next = unit->next;
			unit->next->prev = unit->prev;

			/*insert unit after input*/
			unit->prev = cb->input;
			unit->next = cb->input->next;
			unit->next->prev = unit;
			unit->prev->next = unit;
		}
		/*set input to our unit*/
		cb->input = unit;
		goto exit;
	}
	/*2- unit is before our last delivered input - temporal scalability*/
	cu = cb->input;
	while (1) {

		if (cu->TS > unit->TS) {
			/*we have a unit after this one that has been consumed - discard our unit*/
			if (!cu->dataLength) {
				unit = NULL;
				goto exit;
			}
			/*previous unit is the active one - check one further*/
			if (cu->prev == unit) {
				if (!unit->prev->dataLength || (unit->prev->TS < unit->TS))
					break;
			}
			/*no previous unit or our unit is just after the previous unit*/
			else if (!cu->prev->dataLength || (cu->prev->TS < unit->TS)) {
				break;
			}
			/*otherwise need to go one further*/
		}

		/*go on*/
		cu = cu->prev;
		/*done (should never happen)*/
		if (cu == cb->input)
			goto exit;
	}

	/*remove unit from the list*/
	unit->prev->next = unit->next;
	unit->next->prev = unit->prev;

	/*insert active before cu*/
	unit->next = cu;
	unit->prev = cu->prev;
	unit->next->prev = unit;
	unit->prev->next = unit;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("Swapping CU buffer\n"));

exit:

	/*perform sanity check on output ordering*/
	if (unit) {
		/*if no more output reset to our min*/
		if (!cb->output->dataLength) {
			cb->output = cb->input;
			while (1) {
				if (!cb->output->prev->dataLength) break;
				cb->output = cb->output->prev;
			}
		}
		/*otherwise we just inserted a CU that should be the next one to be consumed*/
		else if (cb->output->TS > unit->TS) {
			cb->output = unit;
		}

	}

	//check_temporal(cb);
	/*unlock the buffer*/
	gf_odm_lock(cb->odm, 0);
	return unit;
}

static void cb_set_buffer_off(GF_CompositionMemory *cb)
{
	gf_clock_buffer_off(cb->odm->codec->ck);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB Buffering done ODM%d: buffering off at OTB %u (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck), gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));

	gf_term_service_media_event(cb->odm->parentscene->root_od, GF_EVENT_MEDIA_CANPLAY);
}

void gf_cm_unlock_input(GF_CompositionMemory *cb, GF_CMUnit *cu, u32 cu_size, Bool codec_reordering)
{
	/*nothing dispatched, ignore*/
	if (!cu_size || (!cu->data && !cu->frame && !cb->pY) ) {
		if (cu->frame) {
			cu->frame->Release(cu->frame);
			cu->frame = NULL;
		}
		cu->dataLength = 0;
		cu->TS = 0;
		return;
	}
	gf_odm_lock(cb->odm, 1);
//		assert(cu->frame);

	if (codec_reordering) {
		cb->input = cb->input->next;
	} else {
		cu = gf_cm_reorder_unit(cb, cu, codec_reordering);
		assert(cu);
	}

	if (cu) {
		/*FIXME - if the CU already has data, this is spatial scalability so same num buffers*/
		if (!cu->dataLength) cb->UnitCount += 1;
		cu->dataLength = cu_size;
		cu->RenderedLength = 0;

		/*turn off buffering for audio - this must be done now rather than when fetching first output frame since we're not
		sure output is fetched (Switch node, ...)*/
		if ( (cb->Status == CB_BUFFER) && (cb->UnitCount >= cb->Capacity) ) {
			/*done with buffering*/
			cb->Status = CB_BUFFER_DONE;
			
			//for audio, turn off buffering now. For video, we will wait for the first frame to be drawn
			if (cb->odm->codec->type == GF_STREAM_AUDIO)
				cb_set_buffer_off(cb);
		}

		//new FPS regulation doesn't need this signaling
#if 0
		/*since a new CU is here notify the compositor*/
		if ((cb->odm->codec->type==GF_STREAM_VISUAL) && cb->odm->mo && cb->odm->mo->num_open) {
			gf_term_invalidate_compositor(cb->odm->term);
		}
#endif
	}
	gf_odm_lock(cb->odm, 0);
}


/*Reset composition memory. Note we don't reset the content of each frame since it would lead to green frames
when using bitmap (visual), where data is not cached*/
void gf_cm_reset(GF_CompositionMemory *cb)
{
	GF_CMUnit *cu;

	gf_odm_lock(cb->odm, 1);
	cu = cb->input;
	cu->RenderedLength = 0;
	if (cu->dataLength && cb->odm->raw_frame_sema)  {
		cu->dataLength = 0;
		gf_sema_notify(cb->odm->raw_frame_sema, 1);
	}

	cu->dataLength = 0;
	if (cu->frame) {
		cu->frame->Release(cu->frame);
		cu->frame = NULL;
	}
	cu->TS = 0;
	cu = cu->next;
	while (cu != cb->input) {
		cu->RenderedLength = 0;
		cu->TS = 0;
		cu->dataLength = 0;
		if (cu->frame) {
			cu->frame->Release(cu->frame);
			cu->frame = NULL;
		}
		cu = cu->next;
	}
	cb->UnitCount = 0;
	cb->HasSeenEOS = 0;

	if (cb->odm->mo) cb->odm->mo->timestamp = 0;

	cb->output = cb->input;
	gf_odm_lock(cb->odm, 0);
}

void gf_cm_reset_timing(GF_CompositionMemory *cb)
{
	GF_CMUnit *cu = cb->input;

	gf_odm_lock(cb->odm, 1);
	cu->TS = 0;
	cu = cu->next;
	while (cu != cb->input) {
		cu->TS = 0;
		cu = cu->next;
	}
	if (cb->odm->mo) cb->odm->mo->timestamp = 0;
	gf_odm_lock(cb->odm, 0);
}

/*resize buffers (blocking)*/
void gf_cm_resize(GF_CompositionMemory *cb, u32 newCapacity)
{
	GF_CMUnit *cu;
	if (!newCapacity) return;

	/*lock buffer*/
	gf_odm_lock(cb->odm, 1);
	cu = cb->input;

	cb->UnitSize = newCapacity;
	
	while (1) {
		if (cu->frame) {
			cu->frame->Release(cu->frame);
			cu->frame = NULL;
		}
		if (!cb->no_allocation) {
			my_large_gf_free(cu->data);
			cu->data = (char*) my_large_alloc(newCapacity);
		} else {
			cu->data = NULL;
			if (cu->dataLength && cb->odm->raw_frame_sema) {
				gf_sema_notify(cb->odm->raw_frame_sema, 1);
			}
		}
		cu->dataLength = 0;
		cu->TS = 0;

		cu = cu->next;
		if (cu == cb->input) break;
	}
	
	cb->UnitCount = 0;
	cb->output = cb->input;
	gf_odm_lock(cb->odm, 0);
}



/*resize buffers (blocking)*/
void gf_cm_reinit(GF_CompositionMemory *cb, u32 UnitSize, u32 Capacity)
{
	GF_CMUnit *cu, *prev;
	u32 i;
	if (!Capacity || !UnitSize) return;

	gf_odm_lock(cb->odm, 1);
	if (cb->input) {
		/*break the loop and destroy*/
		cb->input->prev->next = NULL;
		gf_cm_unit_del(cb->input, cb->no_allocation);
		cb->input = NULL;
	}

	cu = NULL;
	cb->Capacity = Capacity;
	cb->UnitSize = UnitSize;

	prev = NULL;
	i = 1;
	while (Capacity) {
		cu = gf_cm_unit_new();
		if (!prev) {
			cb->input = cu;
		} else {
			prev->next = cu;
			cu->prev = prev;
		}
		cu->dataLength = 0;
		if (cb->no_allocation) {
			cu->data = NULL;
		} else {
			cu->data = (char*)my_large_alloc(UnitSize);
		}
		prev = cu;
		Capacity --;
		i++;
	}
	cu->next = cb->input;
	cb->input->prev = cu;
	cb->output = cb->input;
	gf_odm_lock(cb->odm, 0);
}

/*access to the first available CU for rendering
this is a blocking call since input may change the output (temporal scalability)*/
GF_CMUnit *gf_cm_get_output(GF_CompositionMemory *cb)
{
	/*if paused or stop or buffering, do nothing*/
	switch (cb->Status) {
	case CB_BUFFER:
	case CB_STOP:
		/*only visual buffers deliver data when buffering or stop*/
		if (cb->odm->codec->type != GF_STREAM_VISUAL) return NULL;
		break;
	case CB_BUFFER_DONE:
		//For non-visual output move to play state upon fetch
		if (cb->odm->codec->type != GF_STREAM_VISUAL)
			cb->Status = CB_PLAY;
		break;
	//we always deliver in pause, up to the caller to decide to consume or not the frame
	case CB_PAUSE:
		break;
	}

	/*no output*/
	if (!cb->UnitCount || !cb->output->dataLength) {
		if ((cb->Status != CB_STOP) && cb->HasSeenEOS && (cb->odm && cb->odm->codec)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Switching composition memory to stop state - time %d\n", cb->odm->OD->objectDescriptorID, (u32) cb->odm->media_stop_time));

			if ((cb->Status==CB_BUFFER_DONE) && (cb->odm->codec->type == GF_STREAM_VISUAL) ){
				gf_clock_buffer_off(cb->odm->codec->ck);
			}
			cb->Status = CB_STOP;
			cb->odm->media_current_time = (u32) cb->odm->media_stop_time;
#ifndef GPAC_DISABLE_VRML
			/*force update of media time*/
			mediasensor_update_timing(cb->odm, 1);
#endif
			gf_odm_signal_eos(cb->odm);
		}
		return NULL;
	}

	/*update the timing*/
	if ((cb->Status != CB_STOP) && cb->odm && cb->odm->codec) {
		if (cb->odm->codec->ck->has_media_time_shift) {
			cb->odm->media_current_time = cb->output->TS + cb->odm->codec->ck->media_time_at_init - cb->odm->codec->ck->init_time;
		} else {
			cb->odm->media_current_time = cb->output->TS;
		}

		/*handle visual object - EOS if no more data (we keep the last CU for rendering, so check next one)*/
		if (cb->HasSeenEOS && (cb->odm->codec->type == GF_STREAM_VISUAL) && (!cb->output->next->dataLength || (cb->Capacity==1))) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] Switching composition memory to stop state - time %d\n", cb->odm->OD->objectDescriptorID, (u32) cb->odm->media_stop_time));
			if (cb->Status==CB_BUFFER_DONE) {
				gf_clock_buffer_off(cb->odm->codec->ck);
			}
			cb->Status = CB_STOP;
			cb->odm->media_current_time = (u32) cb->odm->media_stop_time;
#ifndef GPAC_DISABLE_VRML
			/*force update of media time*/
			mediasensor_update_timing(cb->odm, 1);
#endif
			gf_odm_signal_eos(cb->odm);
		}
	}
	if (cb->output->sender_ntp) {
		cb->LastRenderedNTPDiff = gf_net_get_ntp_diff_ms(cb->output->sender_ntp);
		cb->LastRenderedNTP = cb->output->sender_ntp;
	}

	return cb->output;
}


/*mark the output as reusable and init clock*/
void gf_cm_output_kept(GF_CompositionMemory *cb)
{
	assert(cb->UnitCount);
	/*this allows reuse of the CU*/
	cb->output->RenderedLength = 0;
	cb->LastRenderedTS = cb->output->TS;

	//For visual output move to play state once first frame is drawn
	if ((cb->Status==CB_BUFFER_DONE) &&  (cb->odm->codec->type == GF_STREAM_VISUAL)) {
		cb_set_buffer_off(cb);
		cb->Status=CB_PLAY;
	}
}

/*drop the output CU*/
void gf_cm_drop_output(GF_CompositionMemory *cb)
{
	gf_cm_output_kept(cb);

	/*WARNING: in RAW mode, we (for the moment) only have one unit - setting output->dataLength to 0 means the input is available
	for the raw channel - we have to make sure the output is completely reseted before releasing the sema*/

	/*on visual streams (except raw oness), always keep the last AU*/
	if (cb->output->dataLength && (cb->odm->codec->type == GF_STREAM_VISUAL) ) {
		if ( !cb->output->next->dataLength || (cb->Capacity == 1) )  {
			Bool no_drop = 1;
			if (cb->no_allocation ) {
				if (cb->odm->term->bench_mode)
					no_drop = 0;
				else if (gf_clock_time(cb->odm->codec->ck) > cb->output->TS)
					no_drop = 0;
			}
			if (no_drop) {
				if (cb->odm->raw_frame_sema) {
					cb->output->dataLength = 0;
					gf_sema_notify(cb->odm->raw_frame_sema, 1);
				}
				return;
			}
		}
	}

	/*reset the output*/
	cb->output->dataLength = 0;
	if (cb->output->frame) {
		cb->output->frame->Release(cb->output->frame);
		cb->output->frame = NULL;
	}
	cb->output->TS = 0;
	cb->output = cb->output->next;
	cb->UnitCount -= 1;

	if (!cb->HasSeenEOS && cb->UnitCount <= cb->Min) {
		cb->odm->codec->PriorityBoost = 1;
	}

	if (cb->odm->raw_frame_sema) {
		gf_sema_notify(cb->odm->raw_frame_sema, 1);
	}
}

void gf_cm_set_status(GF_CompositionMemory *cb, u32 Status)
{
	if (cb->Status == Status)
		return;

	gf_odm_lock(cb->odm, 1);
	/*if we're asked for play, trigger on buffering*/
	if (Status == CB_PLAY) {
		switch (cb->Status) {
		case CB_STOP:
			cb->Status = CB_BUFFER;
			gf_clock_buffer_on(cb->odm->codec->ck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB status changed - ODM%d: buffering on at OTB %d (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck),gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));
			break;
		case CB_PAUSE:
			cb->Status = CB_PLAY;
			break;
		/*this should never happen (calling play while already buffering ...)*/
		case CB_BUFFER:
			cb->LastRenderedTS = 0;
			break;
		default:
			cb->Status = Status;
			break;
		}
	} else {
		cb->LastRenderedTS = 0;
		if (cb->Status == CB_BUFFER) {
			gf_clock_buffer_off(cb->odm->codec->ck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB status changed - ODM%d: buffering off at OTB %u (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck), gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));
		}
		if (Status == CB_STOP) {
			gf_cm_reset(cb);
			cb->LastRenderedTS = 0;
		}
		cb->Status = Status;
		if (Status==CB_BUFFER) {
			gf_clock_buffer_on(cb->odm->codec->ck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB status changed - ODM%d: buffering on at OTB %d (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck), gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));
		}
	}

	gf_odm_lock(cb->odm, 0);

}

void gf_cm_set_eos(GF_CompositionMemory *cb)
{
	gf_odm_lock(cb->odm, 1);
	/*we may have a pb if the stream is so short that the EOS is signaled
	while we're buffering. In this case we shall turn the clock on and
	keep a trace of the EOS notif*/
	if (cb->Status == CB_BUFFER) {
		cb->Status = CB_BUFFER_DONE;
		gf_clock_buffer_off(cb->odm->codec->ck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] CB EOS - ODM%d: buffering off at OTB %u (STB %d) (nb wait on clock: %d)\n", cb->odm->OD->objectDescriptorID, gf_clock_time(cb->odm->codec->ck), gf_term_get_time(cb->odm->term), cb->odm->codec->ck->Buffering));
	}
	cb->HasSeenEOS = 1;

	//in bench mode eos cannot be signaled through flush of composition memory since it is always empty - do it here
	if (cb->odm->term->bench_mode==2) {
		cb->Status = CB_STOP;
		gf_odm_signal_eos(cb->odm);
	}

	gf_term_invalidate_compositor(cb->odm->term);
	gf_odm_lock(cb->odm, 0);
}


Bool gf_cm_is_running(GF_CompositionMemory *cb)
{
	if (cb->Status == CB_PLAY)
		return !cb->odm->codec->ck->Paused;

	if ((cb->Status == CB_BUFFER_DONE) && (gf_clock_is_started(cb->odm->codec->ck) || cb->odm->term->play_state) ) {
		return 1;
	}

	if ((cb->odm->codec->type == GF_STREAM_VISUAL)
	        && (cb->Status == CB_STOP)
	        && cb->output->dataLength) return 1;

	return 0;
}


Bool gf_cm_is_eos(GF_CompositionMemory *cb)
{
	if (cb->HasSeenEOS && ((cb->Status == CB_STOP) || (cb->UnitCount==0)) )
		return 1;
	return 0;
}

void gf_cm_abort_buffering(GF_CompositionMemory *cb)
{
	if (cb->Status == CB_BUFFER) {
		cb->Status = CB_BUFFER_DONE;
		cb_set_buffer_off(cb);
	}
}
