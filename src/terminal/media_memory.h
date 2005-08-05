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

#ifndef _MEDIA_MEMORY_H_
#define _MEDIA_MEMORY_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/thread.h>

/*compressed media unit*/
typedef struct _decoding_buffer 
{
	struct _decoding_buffer *next;

	/*decoding time stamp in ms*/
	u32 DTS;
	/*composition time stamp in ms*/
	u32 CTS;
	/*random access point flag*/
	Bool RAP;
	/*amount of padding bits*/
	u8 PaddingBits;

	u32 dataLength;
	unsigned char *data;
} AUBUFFER, *LPAUBUFFER;

LPAUBUFFER DB_New();
void DB_Delete(LPAUBUFFER db);


/*composition memory (composition buffer) status*/
enum
{
	/*CB stoped (contains invalid data)*/
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
} CUBUFFER, *LPCUBUFFER;


/*composition buffer (circular buffer of CUs)*/
typedef struct _composition_memory
{
	/*input is used by the decoder to deliver CUs. 
	if temporal scalability is enabled, this is the LAST DELIVERED CU 
	otherwise this is the next available CU slot*/
	LPCUBUFFER input;
	/*output is the next available frme for rendering*/
	LPCUBUFFER output;
	/*capacity is the number of allocated buffers*/
	u32 Capacity;
	/*Min is the triggering of the media manager*/
	u32 Min;
	/*Unit size is the size of each buffer*/
	u32 UnitSize;

	/*Status of the buffer*/
	u32 Status;
	/*Number of active units*/
	u32 UnitCount;

	/*OD manager ruling this CB*/
	struct _od_manager *odm;

	/*set when EOS is recieved by decoder*/
	Bool HasSeenEOS;

	/*trick for temporal scalability: this is the last rendered CTS*/
	u32 LastRenderedTS;

	/*exclusive access is required since rendering and decoding don't take place in the same thread*/
	GF_Mutex *mx;

} COMPOBUF, *LPCOMPOBUF;

/*a composition buffer only has fixed-size unit*/
LPCOMPOBUF CB_New(u32 UnitSize, u32 capacity);
void CB_Delete(LPCOMPOBUF cb);
/*re-inits complete cb*/
void CB_Reinit(LPCOMPOBUF cb, u32 UnitSize, u32 Capacity);

/*get exclusive access to CB*/
void CB_Lock(LPCOMPOBUF cb, u32 LockIt);

/*locks available input for desired TS (needed for scalability) - return NULL if no 
input is available (buffer full)*/
LPCUBUFFER CB_LockInput(LPCOMPOBUF cb, u32 TS);
/*dispatch data in input. If NbBytes is 0, no data is dispatched. TS is needed for re-ordering
of buffers*/
void CB_UnlockInput(LPCOMPOBUF cb, u32 TS, u32 NbBytes);

/*fetch output buffer, NULL if output is empty*/
LPCUBUFFER CB_GetOutput(LPCOMPOBUF cb);
/*release the output buffer once rendered - if renderedLength is not equal to dataLength the
output is NOT droped*/
void CB_DropOutput(LPCOMPOBUF cb);

/*reset the entire memory*/
void CB_Reset(LPCOMPOBUF cb);

/*resize the buffers*/
void CB_ResizeBuffers(LPCOMPOBUF cb, u32 newCapacity);

/*set the status of the buffer. Buffering is done automatically*/
void CB_SetStatus(LPCOMPOBUF cb, u32 Status);

/*return 1 if object is running (clock started), 0 otherwise*/
Bool CB_IsRunning(LPCOMPOBUF cb);
/*signals end of stream*/
void CB_SetEndOfStream(LPCOMPOBUF cb);
/*returns 1 if end of stream has been reached (eg, signaled and no more data)*/
Bool CB_IsEndOfStream(LPCOMPOBUF cb);
/*aborts buffering if any*/
void CB_AbortBuffering(LPCOMPOBUF cb);

#ifdef __cplusplus
}
#endif


#endif	/*_MEDIA_MEMORY_H_*/

