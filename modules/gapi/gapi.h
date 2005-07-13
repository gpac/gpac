/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectX audio and video render module
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


#ifndef _GAPI_H
#define _GAPI_H

#include <gpac/list.h>
#include <gpac/thread.h>
/*driver interface*/
#include <gpac/modules/video_out.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	u32 width, height, pitch, pixel_format;
	unsigned char *buffer;
	u32 ID;
} GAPISurface;

typedef struct
{
	HWND hWnd;
	GF_Mutex *mx;
	struct GXDisplayProperties gx;
	Bool gapi_open;
	GF_List *surfaces;
	
	GXKeyList keys;

	u32 screen_w, screen_h;
	u32 fs_w, fs_h;
	/*store w and h for fullscreen*/
	u32 disp_w, disp_h;

	Bool fullscreen;
	Bool is_init;

	/*main surface info*/
	unsigned char *backbuffer;
	u32 bb_size, bb_width, bb_height, bb_pitch;
	u32 pixel_format;
	u32 BPP, bitsPP;

	GF_Window dst_blt;

	Bool is_resizing;
	DWORD ThreadID;
	HANDLE hThread;
	Bool owns_hwnd;
} GAPIPriv;


#ifdef __cplusplus
}
#endif

#endif 
