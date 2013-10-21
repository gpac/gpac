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

#ifndef _MEDIA_MEMORY_H_
#define _MEDIA_MEMORY_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/thread.h>

enum
{
	/*AU is RAP*/
	GF_DB_AU_RAP = 1,
	/*special flag for systems streams: we may receive a CTS in a carousel, to indicate the 
	SFTime init time*/
	GF_DB_AU_CTS_IN_PAST = 1<<1,
	/*hack for some DMB streams not signaling TS for BIFS*/
	GF_DB_AU_NO_TIMESTAMPS = 1<<2
};

/*compressed media unit*/
typedef struct _decoding_buffer 
{
	struct _decoding_buffer *next;

	/*decoding time stamp in ms*/
	u32 DTS;
	/*composition time stamp in ms*/
	u32 CTS;
	/*AU flags*/
	u8 flags;
	/*amount of padding bits*/
	u8 PaddingBits;

	u32 dataLength;
	char *data;
} GF_DBUnit;

GF_DBUnit *gf_db_unit_new();
void gf_db_unit_del(GF_DBUnit *db);


/*composition memory (composition buffer) status*/
enum
{
	/*CB stopped (contains invalid data)*/
	CB_STOP = 0,
	/*CB playing (contains valid data)*/
	CB_PLAY,
	/*CB is not playing but contains VALID data (prevents output droping)*/
	CB_PAUSE,
	/*CB is in buffering stage (internal only)*/
	CB_BUFFER,
	/*CB is done buffering and waits for data (internal only)*/
	CB_BUFFER_DONE,
};

/*Composition Unit structure*/
typedef struct _composition_unit {
	/*next and previous buffers. previous is used for temporal scalability*/
	struct _composition_unit *next, *prev;
	/*TS: composition time stamp in ms*/
	u32 TS;

	/*this is for audio CU in case the whole CU is not consumed in one shot*/
	u32 RenderedLength;

	u32 dataLength;
	char* data;
} GF_CMUnit;


/*composition buffer (circular buffer of CUs)*/
struct _composition_memory
{
	/*input is used by the decoder to deliver CUs. 
	if temporal scalability is enabled, this is the LAST DELIVERED CU 
	otherwise this is the next available CU slot*/
	GF_CMUnit *input;
	/*output is the next available frme for rendering*/
	GF_CMUnit *output;
	/*capacity is the number of allocated buffers*/
	u32 Capacity;
	/*Min is the triggering of the media manager*/
	u32 Min;
	/*Unit size is the size of each buffer*/
	u32 UnitSize;
	Bool no_allocation;

	/*Status of the buffer*/
	u32 Status;
	/*Number of active units*/
	u32 UnitCount;

	/*OD manager ruling this CB*/
	struct _od_manager *odm;

	/*set when EOS is received by decoder*/
	Bool HasSeenEOS;

	/*trick for temporal scalability: this is the last rendered CTS*/
	u32 LastRenderedTS;

	u8 *pY, *pU, *pV;
};

/*a composition buffer only has fixed-size unit*/
GF_CompositionMemory *gf_cm_new(u32 UnitSize, u32 capacity, Bool no_allocation);
void gf_cm_del(GF_CompositionMemory *cb);
/*re-inits complete cb*/
void gf_cm_reinit(GF_CompositionMemory *cb, u32 UnitSize, u32 Capacity);

/*locks available input for desired TS (needed for scalability) - return NULL if no 
input is available (buffer full)*/
GF_CMUnit *gf_cm_lock_input(GF_CompositionMemory *cb, u32 TS, Bool codec_reordering);
/*dispatch data in input. If NbBytes is 0, no data is dispatched. TS is needed for re-ordering
of buffers*/
void gf_cm_unlock_input(GF_CompositionMemory *cb, GF_CMUnit *cu, u32 cu_size, Bool codec_reordering);
/*rewind input of 1 unit - used when doing precise seeking*/
void gf_cm_rewind_input(GF_CompositionMemory *cb);

/*fetch output buffer, NULL if output is empty*/
GF_CMUnit *gf_cm_get_output(GF_CompositionMemory *cb);
/*release the output buffer once rendered - if renderedLength is not equal to dataLength the
output is NOT droped*/
void gf_cm_drop_output(GF_CompositionMemory *cb);

/*reset the entire memory*/
void gf_cm_reset(GF_CompositionMemory *cb);

/*resize the buffers*/
void gf_cm_resize(GF_CompositionMemory *cb, u32 newCapacity);

/*set the status of the buffer. Buffering is done automatically*/
void gf_cm_set_status(GF_CompositionMemory *cb, u32 Status);

/*return 1 if object is running (clock started), 0 otherwise*/
Bool gf_cm_is_running(GF_CompositionMemory *cb);
/*signals end of stream*/
void gf_cm_set_eos(GF_CompositionMemory *cb);
/*returns 1 if end of stream has been reached (eg, signaled and no more data)*/
Bool gf_cm_is_eos(GF_CompositionMemory *cb);
/*aborts buffering if any*/
void gf_cm_abort_buffering(GF_CompositionMemory *cb);

#ifdef __cplusplus
}
#endif


#endif	/*_MEDIA_MEMORY_H_*/

