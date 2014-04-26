/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifdef GPAC_USE_OGL_ES
#include "GLES/egl.h"
#endif

/*driver interface*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	HWND hWnd;
	DWORD orig_wnd_proc;
	GF_Mutex *mx;

	unsigned char *raw_ptr;
	Bool contiguous_mem;

	GXKeyList keys;

	u32 screen_w, screen_h;
	u32 fs_w, fs_h;
	/*store w and h for fullscreen*/
	u32 backup_w, backup_h;
	s32 x_pitch, y_pitch;
	Bool fullscreen;
	u32 gx_mode;

	u32 sys_w, sys_h;
	Bool scale_coords;

	/*main surface info*/
	char *backbuffer;
	u32 bb_size, bb_width, bb_height, bb_pitch;
	u32 pixel_format;
	u32 BPP, bits_per_pixel;

	GF_Window dst_blt;
	DWORD ThreadID;
	HANDLE hThread;
	Bool owns_hwnd;

	Bool erase_dest;
	u32 off_x, off_y;

	HBITMAP bitmap, old_bitmap;
	DWORD * bits;
	HDC hdcBitmap, hdc;
	BITMAPINFO*     bmi;

#ifdef GPAC_USE_OGL_ES
	u32 output_3d_type;
	EGLDisplay egldpy;
	EGLSurface surface;
	EGLConfig eglconfig;
	EGLContext eglctx;

	HBITMAP gl_bitmap;
	DWORD *gl_bits;
	HWND gl_hwnd;
	Bool use_pbuffer;
#endif

} GAPIPriv;

GF_Err GAPI_SetupOGL_ES_Offscreen(GF_VideoOutput *dr, u32 width, u32 height) ;


#ifdef __cplusplus
}
#endif

#endif
