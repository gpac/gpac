/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
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

#if defined(__GNUC__) && !defined(DIRECTSOUND_VERSION)
#define DIRECTSOUND_VERSION 0x0500
#endif

/*driver interfaces*/
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>
#include <gpac/list.h>
#include <gpac/constants.h>

//define before thread.h which includes windows.h and winbase.h
#define INITGUID

#include <gpac/thread.h>



/*
#include <windows.h>
*/

#define HAS_DDRAW_H

#ifdef HAS_DDRAW_H
#include <ddraw.h>
#include <mmsystem.h>
#include <dsound.h>

#ifndef _WIN32_WCE
#include <vfw.h>
#endif

/*DirectDraw video output*/
#if (DIRECTDRAW_VERSION < 0x0700)
#define USE_DX_3
#endif


#ifdef USE_DX_3
typedef LPDIRECTDRAWSURFACE LPDDRAWSURFACE;
typedef DDSURFACEDESC DDSURFDESC;
#else
typedef LPDIRECTDRAWSURFACE7 LPDDRAWSURFACE;
typedef DDSURFACEDESC2 DDSURFDESC;
#endif
typedef DDSURFDESC *LPDDSURFDESC;

typedef HRESULT(WINAPI * DIRECTDRAWCREATEPROC) (GUID *, LPDIRECTDRAW *, IUnknown *);

#endif



#ifdef _WIN32_WCE
# ifndef SWP_ASYNCWINDOWPOS
#  define SWP_ASYNCWINDOWPOS 0
# endif
#endif



#ifdef GPAC_USE_GLES1X
#include "GLES/egl.h"
#elif defined(GPAC_USE_GLES2)
#include "EGL/egl.h"
#endif

#define EGL_CHECK_ERR	{s32 res = eglGetError(); if (res!=12288) GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("EGL Error %d file %s line %d\n", res, __FILE__, __LINE__)); }

typedef struct
{
	LPDDRAWSURFACE pSurface;
	u32 width, height, format, pitch;
	Bool is_yuv;
} DDSurface;


#if defined(GPAC_USE_TINYGL)
# ifndef GPAC_DISABLE_3D
#  define GPAC_DISABLE_3D
# endif
#endif

#ifndef WM_UNICHAR
#define WM_UNICHAR	0x0109
#endif //WM_UNICHAR

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
	Bool force_alpha;
	Bool offscreen_yuv_to_rgb;

	u32 width, height;
	u32 fs_width, fs_height;
	u32 store_width, store_height;
	LONG backup_styles;
	Bool alt_down, ctrl_down;
	Bool on_secondary_screen;
	u32 pixelFormat;
	u32 video_bpp;

	HDC lock_hdc;

	/*HW surfaces for blitting+stretch*/
	DDSurface rgb_pool, yuv_pool;

	/*if we run in threaded mode*/
	GF_Thread *th;
	u32 th_state;


	Bool owns_hwnd;
	u32 off_w, off_h, prev_styles;
	LONG_PTR last_mouse_pos;
	/*cursors*/
	HCURSOR curs_normal, curs_hand, curs_collide;
	u32 cursor_type;
	Bool is_setup, disable_vsync;
	char *caption;
	/*gl*/
#ifndef GPAC_DISABLE_3D

#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)
	NativeDisplayType gl_HDC;
	EGLDisplay egldpy;
	EGLSurface surface;
	EGLConfig eglconfig;
	EGLContext eglctx;
#else
	HDC gl_HDC, pb_HDC;
	HGLRC gl_HRC, pb_HRC;
	Bool glext_init;
#endif
	Bool output_3d;
	HWND bound_hwnd;
	Bool gl_double_buffer;
	/*0: not init, 1: used, 2: not used*/
	u32 mode_high_bpp;
	u8 bpp;
#endif

	Bool has_focus;
	LONG_PTR orig_wnd_proc;

	UINT_PTR timer;
	u32 last_mouse_move, cursor_type_backup;
	Bool windowless, hidden;

	Bool dd_lost;
	Bool force_video_mem_for_yuv;

	HMODULE hDDrawLib;
	DIRECTDRAWCREATEPROC DirectDrawCreate;
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

#define MAKERECT(rc, dest)	{ rc.left = dest->x; rc.top = dest->y; rc.right = rc.left + dest->w; rc.bottom = rc.top + dest->h;	}

/*this is REALLY ugly, to pass the HWND to DSound when we create the window in this module*/
HWND DD_GetGlobalHWND();

#ifndef GPAC_DISABLE_3D
GF_Err DD_SetupOpenGL(GF_VideoOutput *dr, u32 offscreen_width, u32 offscreen_height);
#endif


#define SAFE_DD_RELEASE(p) { if(p) { (p)->lpVtbl->Release( (p) ); (p)=NULL; } }


void *NewAudioOutput();
void DeleteDxAudioOutput(void *);


#define SAFE_DS_RELEASE(p) { if(p) { p->lpVtbl->Release(p); (p)=NULL; } }

LRESULT APIENTRY DD_WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


#endif
