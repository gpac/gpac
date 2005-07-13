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



#include "media_memory.h"
#include <gpac/internal/terminal_dev.h>
#include <gpac/constants.h>
#include "media_control.h"

#define NO_TEMPORAL_SCALABLE	1


LPAUBUFFER DB_New()
{
	LPAUBUFFER tmp = malloc(sizeof(AUBUFFER));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(AUBUFFER));
	return tmp;
}

void DB_Delete(LPAUBUFFER db)
{
	if (!db) return;
	if (db->next) DB_Delete(db->next);
	if (db->data) free(db->data);
	free(db);
}

LPCUBUFFER CU_New()
{
	LPCUBUFFER tmp = malloc(sizeof(CUBUFFER));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(CUBUFFER));
	return tmp;
}

void CU_Delete(LPCUBUFFER cb)
{
	if (cb->next) CU_Delete(cb->next);
	cb->next = NULL;
	if (cb->data) {
		free(cb->data);
		cb->data = NULL;
	}
	free(cb);
}


LPCOMPOBUF CB_New(u32 UnitSize, u32 capacity)
{
	LPCOMPOBUF tmp;
	LPCUBUFFER cu, prev;
	u32 i;
	if (!capacity) return NULL;
	
	tmp = malloc(sizeof(COMPOBUF));
	memset(tmp, 0, sizeof(COMPOBUF));
	tmp->Capacity = capacity;
	tmp->UnitSize = UnitSize;
	tmp->mx = gf_mx_new();

	prev = NULL;
	i = 1;
	while (capacity) {
		cu = CU_New();
		if (!prev) {
			tmp->input = cu;
		} else {
			prev->next = cu;
			cu->prev = prev;
		}
		cu->dataLength = 0;
		cu->data = UnitSize ? malloc(sizeof(char)*UnitSize) : NULL;
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

void CB_Delete(LPCOMPOBUF cb)
{
	CB_Lock(cb, 1);
	/*may happen when CB is destroyed right after creation in case*/
	if (cb->Status == CB_BUFFER) {
		gf_clock_buffer_off(cb->odm->codec->ck);
	}
	
	/*break the loop and destroy*/
	cb->input->prev->next = NULL;
	CU_Delete(cb->input);
	CB_Lock(cb, 0);
	gf_mx_del(cb->mx);
	free(cb);
}

void CB_Lock(LPCOMPOBUF cb, u32 LockIt)
{
	if (LockIt) 
		gf_mx_p(cb->mx);
	else
		gf_mx_v(cb->mx);
}

/*access to the input buffer - return NULL if no input is available (buffer full)*/
LPCUBUFFER CB_LockInput(LPCOMPOBUF cb, u32 TS)
{
#if NO_TEMPORAL_SCALABLE
	/*there is still something in the input buffer*/
	if (cb->input->dataLength) {
		if (cb->input->TS==TS) return cb->input;
		return NULL;
	}
	return cb->input;

#else	
	LPCUBUFFER cu;

	/*spatial scalable, go backward to fetch same TS*/
	cu = cb->input;
	while (1) {
		if (cu->TS == TS) return cu;
		cu = cu->prev;
		if (cu == cb->input) break;
	}
	/*temporal scalable (find empty slot)*/
	cu = cb->input;
	while (1) {
		if (!cu->dataLength) return cu;
		cu = cu->next;
		if (cu == cb->input) return NULL;
	}
	return NULL;
#endif

}

/*re-orders units in case of temporal scalability. Blocking call as it may change the output unit too*/
LPCUBUFFER LocateAndOrderUnit(LPCOMPOBUF cb, u32 TS)
{
	LPCUBUFFER unit;
#if NO_TEMPORAL_SCALABLE
	unit = cb->input;
	cb->input = cb->input->next;
	return unit;

#else
	LPCUBUFFER cu;

	/*lock the buffer since we may move pointers*/
	CB_Lock(cb, 1);
	unit = NULL;

	/*1- locate cu*/
	cu = cb->input;
	while (1) {
		if (cu->TS == TS) break;
		cu = cu->next;
		/*done (should never happen)*/
		if (cu == cb->input) goto exit;
	}
	unit = cu;

	/*2- spatial scalability or input is our unit, no reordering*/
	if (cu->dataLength || (cb->input->TS == TS) ) goto exit;

	/*3- unit is next on time axis (no temporal scalability)*/
	if (!cb->input->dataLength || cb->input->TS < TS) {

		/*remove unit from the list*/
		unit->prev->next = unit->next;
		unit->next->prev = unit->prev;

		/*insert unit after input*/
		unit->prev = cb->input;
		unit->next = cb->input->next;
		unit->next->prev = unit;
		unit->prev->next = unit;
		/*set input to our unit*/
		cb->input = unit;
		goto exit;
	}
	/*4- unit is before our last delivered input - temporal scalability*/
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
		if (cu == cb->input) goto exit;
	}
	
	/*remove unit from the list*/
	unit->prev->next = unit->next;
	unit->next->prev = unit->prev;

	/*insert active before cu*/
	unit->next = cu;
	unit->prev = cu->prev;
	unit->next->prev = unit;
	unit->prev->next = unit;
	
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

	/*unlock the buffer*/
	CB_Lock(cb, 0);
	return unit;

#endif
}

void CB_UnlockInput(LPCOMPOBUF cb, u32 TS, u32 NbBytes)
{
	LPCUBUFFER cu;

	/*nothing dispatched, ignore*/
	if (!NbBytes) return;
	CB_Lock(cb, 1);

	/*insert/swap this CU*/
	cu = LocateAndOrderUnit(cb, TS);

	if (cu) {
		/*if the CU already has data, this is spatial scalability so same num buffers*/
		if (!cu->dataLength) cb->UnitCount += 1;
		cu->dataLength = NbBytes;
		cu->RenderedLength = 0;

		/*turn off buffering - this must be done now rather than when fetching first output frame since we're not 
		sure output is fetched (Switch node, ...)*/
		if ( (cb->Status == CB_BUFFER) && (cb->UnitCount >= cb->Capacity) ) {
			/*done with buffering, signal to the clock (ONLY ONCE !)*/
			cb->Status = CB_BUFFER_DONE;
			gf_clock_buffer_off(cb->odm->codec->ck);
		} 

		/*since a new CU is here notify the renderer*/
		if ((cb->odm->codec->type==GF_STREAM_VISUAL) && cb->odm->mo && cb->odm->mo->num_open) {
			gf_term_invalidate_renderer(cb->odm->term);
		}
	}
	CB_Lock(cb, 0);
}


/*Reset composition memory. Note we don't reset the content of each frame since it would lead to green frames 
when using bitmap (visual), where data is not cached*/
void CB_Reset(LPCOMPOBUF cb)
{
	LPCUBUFFER cu;

	CB_Lock(cb, 1);

	cu = cb->input;
	cu->RenderedLength = 0;
	cu->dataLength = 0;
	cu->TS = 0;
	cu = cu->next;
	while (cu != cb->input) {
		cu->RenderedLength = 0;
		cu->TS = 0;
		cu->dataLength = 0;
		cu = cu->next;
	}
	cb->output = cb->input;
	cb->UnitCount = 0;
	cb->HasSeenEOS = 0;

	if (cb->odm->mo) cb->odm->mo->current_ts = 0;
	CB_Lock(cb, 0);
}

/*resize buffers (blocking)*/
void CB_ResizeBuffers(LPCOMPOBUF cb, u32 newCapacity)
{
	LPCUBUFFER cu;
	if (!newCapacity) return;

	/*lock buffer*/
	CB_Lock(cb, 1);
	cu = cb->input;

	cb->UnitSize = newCapacity;
	/*don't touch any existing memory (it may happen on the fly with audio)*/
	cu->data = realloc(cu->data, sizeof(char)*newCapacity);
	cu = cu->next;
	while (cu != cb->input) {
		cu->data = realloc(cu->data, sizeof(char)*newCapacity);
		cu = cu->next;
	}

	CB_Lock(cb, 0);
}



/*resize buffers (blocking)*/
void CB_Reinit(LPCOMPOBUF cb, u32 UnitSize, u32 Capacity)
{
	LPCUBUFFER cu, prev;
	u32 i;
	if (!Capacity || !UnitSize) return;

	CB_Lock(cb, 1);
	/*break the loop and destroy*/
	cb->input->prev->next = NULL;
	CU_Delete(cb->input);


	cb->Capacity = Capacity;
	cb->UnitSize = UnitSize;

	prev = NULL;
	i = 1;
	while (Capacity) {
		cu = CU_New();
		if (!prev) {
			cb->input = cu;
		} else {
			prev->next = cu;
			cu->prev = prev;
		}
		cu->dataLength = 0;
		cu->data = malloc(sizeof(char)*UnitSize);
		prev = cu;
		Capacity --;
		i++;
	}
	cu->next = cb->input;
	cb->input->prev = cu;
	cb->output = cb->input;
	CB_Lock(cb, 0);
}

/*access to the first available CU for rendering
this is a blocking call since input may change the output (temporal scalability)*/
LPCUBUFFER CB_GetOutput(LPCOMPOBUF cb)
{
	LPCUBUFFER out = NULL;

	/*if paused or stop or buffering, do nothing*/
	switch (cb->Status) {
	case CB_BUFFER:
		return NULL;
	case CB_STOP:
	case CB_PAUSE:
		/*only visual buffers deliver data when paused*/
		if (cb->odm->codec->type != GF_STREAM_VISUAL) goto exit;
		break;
	}

	/*no output*/
	if (!cb->output->dataLength) {
		if ((cb->Status != CB_STOP) && cb->HasSeenEOS && (cb->odm && cb->odm->codec)) {
			cb->Status = CB_STOP;
			cb->odm->current_time = cb->odm->range_end;
			MS_UpdateTiming(cb->odm);
		}
		goto exit;
	}

	/*update the timing*/
	if ((cb->Status != CB_STOP) && cb->odm && cb->odm->codec) {
		cb->odm->current_time = cb->output->TS;

		/*handle visual object - EOS if no more data (we keep the last CU for rendering, so check next one)*/
		if (cb->HasSeenEOS && (!cb->output->next->dataLength || (cb->Capacity==1))) {
			cb->Status = CB_STOP;
			cb->odm->current_time = cb->odm->range_end;
		}
		MS_UpdateTiming(cb->odm);
	}
	out = cb->output;

exit:
	return out;
}



/*drop the output CU*/
void CB_DropOutput(LPCOMPOBUF cb)
{
	assert(cb->UnitCount);
	/*this allows reuse of the CU*/
	cb->output->RenderedLength = 0;
	cb->LastRenderedTS = cb->output->TS;


	/*on visual streams, always keep the last AU*/
	if (cb->output->dataLength && (cb->odm->codec->type == GF_STREAM_VISUAL) ) {
		if ( !cb->output->next->dataLength || (cb->Capacity == 1) )  {
			return;
		}
	}
	
	/*reset the output*/
	cb->output->dataLength = 0;
	cb->output = cb->output->next;
	cb->UnitCount -= 1;

	if (!cb->HasSeenEOS && cb->UnitCount <= cb->Min) {
		cb->odm->codec->PriorityBoost = 1;
	}
}

void CB_SetStatus(LPCOMPOBUF cb, u32 Status)
{

	CB_Lock(cb, 1);
	/*if we're asked for play, trigger on buffering*/
	if (Status == CB_PLAY) {
		switch (cb->Status) {
		case CB_STOP:
			cb->Status = CB_BUFFER;
			gf_clock_buffer_on(cb->odm->codec->ck);
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
		}
		if (Status == CB_STOP) {
			CB_Reset(cb);
		}
		cb->Status = Status;
	}

	CB_Lock(cb, 0);

}

void CB_SetEndOfStream(LPCOMPOBUF cb)
{
	CB_Lock(cb, 1);
	/*we may have a pb if the stream is so short that the EOS is signaled 
	while we're buffering. In this case we shall turn the clock on and 
	keep a trace of the EOS notif*/
	if (cb->Status == CB_BUFFER) {
		cb->Status = CB_BUFFER_DONE;
		gf_clock_buffer_off(cb->odm->codec->ck);
	}
	cb->HasSeenEOS = 1;
	gf_term_invalidate_renderer(cb->odm->term);
	CB_Lock(cb, 0);
}


Bool CB_IsRunning(LPCOMPOBUF cb)
{
	if (cb->Status == CB_PLAY)
		return !cb->odm->codec->ck->Paused;

	if ((cb->Status == CB_BUFFER_DONE) && gf_clock_is_started(cb->odm->codec->ck)) {
		cb->Status = CB_PLAY;
		return 1;
	}

	if ((cb->odm->codec->type == GF_STREAM_VISUAL)
		&& (cb->Status == CB_STOP)
		&& cb->output->dataLength) return 1;

	return 0;
}


Bool CB_IsEndOfStream(LPCOMPOBUF cb)
{
	return ( (cb->Status == CB_STOP) && cb->HasSeenEOS);
}

void CB_AbortBuffering(LPCOMPOBUF cb)
{
	if (cb->Status == CB_BUFFER) {
		cb->Status = CB_BUFFER_DONE;
		gf_clock_buffer_off(cb->odm->codec->ck);
	}
}
