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


#ifndef _DXHW_H
#define _DXHW_H


/*driver interfaces*/
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>
#include <gpac/list.h>
#include <gpac/constants.h>
#include <gpac/thread.h>

#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#ifndef _WIN32_WCE
#include <vfw.h>
#endif

#include <ddraw.h>

#ifdef GPAC_USE_OGL_ES
#include "GLES/egl.h"
#endif

/*
		DirectDraw video output
*/

#if (DIRECTDRAW_VERSION < 0x0700)
#define USE_DX_3
#endif

typedef struct
{
#ifdef USE_DX_3
    LPDIRECTDRAWSURFACE pSurface;
#else
    LPDIRECTDRAWSURFACE7 pSurface;
#endif
	u32 width, height, format, pitch;
} DDSurface;

typedef struct
{
	HWND os_hwnd, fs_hwnd, cur_hwnd, parent_wnd;
	Bool NeedRestore;
	Bool switch_res;

#ifdef USE_DX_3
    LPDIRECTDRAW pDD;
    LPDIRECTDRAWSURFACE pPrimary;
    LPDIRECTDRAWSURFACE pBack;
#else
    LPDIRECTDRAW7 pDD;
    LPDIRECTDRAWSURFACE7 pPrimary;
    LPDIRECTDRAWSURFACE7 pBack;
#endif
	Bool ddraw_init;
	Bool yuv_init;
	Bool fullscreen;
	Bool systems_memory;

	u32 width, height;
	u32 fs_width, fs_height;
	u32 fs_store_width, fs_store_height;
	u32 store_width, store_height;

	u32 pixelFormat;
	u32 video_bpp;

	HDC lock_hdc;

	/*HW surfaces for blitting+stretch*/
	DDSurface rgb_pool, yuv_pool;
	
	/*if we own the window*/
	GF_Thread *th;
	u32 th_state;


	Bool owns_hwnd;
	u32 off_w, off_h, prev_styles;
	LONG last_mouse_pos;
	/*cursors*/
	HCURSOR curs_normal, curs_hand, curs_collide;
	u32 cursor_type;

	/*gl*/
#ifdef GPAC_USE_OGL_ES
	NativeDisplayType gl_HDC;
    EGLDisplay egldpy;
    EGLSurface surface;
    EGLConfig eglconfig;
    EGLContext eglctx;
#else
	HDC gl_HDC;
	HGLRC gl_HRC;
#endif
	u32 output_3d_type;
	HWND gl_hwnd;
	Bool has_focus;
	Bool gl_double_buffer;

	DWORD orig_wnd_proc;

	u32 last_mouse_move, timer, cursor_type_backup;
	Bool windowless;
} DDContext;

void DD_SetupWindow(GF_VideoOutput *dr, Bool hide);
void DD_ShutdownWindow(GF_VideoOutput*dr);
GF_Err DD_ProcessEvent(GF_VideoOutput*dr, GF_Event *evt);

void DestroyObjects(DDContext *dd);
GF_Err GetDisplayMode(DDContext *dd);
/*2D-only callbacks*/
void DD_SetupDDraw(GF_VideoOutput *driv);
GF_Err InitDirectDraw(GF_VideoOutput *dr, u32 Width, u32 Height);
void DD_InitYUV(GF_VideoOutput *dr);

GF_Err DD_SetBackBufferSize(GF_VideoOutput *dr, u32 width, u32 height, Bool use_system_memory);

GF_Err DD_FlushEx(GF_VideoOutput *dr, GF_Window *dest, Bool wait_for_sync);

void dx_copy_pixels(GF_VideoSurface *dst_s, const GF_VideoSurface *src_s, const GF_Window *src_wnd);

#define MAKERECT(rc, dest)	{ rc.left = dest->x; rc.top = dest->y; rc.right = rc.left + dest->w; rc.bottom = rc.top + dest->h;	}

/*this is REALLY ugly, to pass the HWND to DSound when we create the window in this module*/
HWND DD_GetGlobalHWND();

GF_Err DD_SetupOpenGL(GF_VideoOutput *dr);

#ifdef USE_DX_3
#define SAFE_DD_RELEASE(p) { if(p) { IDirectDraw_Release(p); (p)=NULL; } }
#else
#define SAFE_DD_RELEASE(p) { if(p) { IDirectDraw7_Release(p); (p)=NULL; } }
#endif


void *NewAudioOutput();
void DeleteAudioOutput(void *);


#define SAFE_DS_RELEASE(p) { if(p) { IDirectSound_Release(p); (p)=NULL; } }

#endif 
