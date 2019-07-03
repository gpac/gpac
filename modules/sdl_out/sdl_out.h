/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SDL audio and video module
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

#ifndef _GF_SDL_OUT_H_
#define _GF_SDL_OUT_H_

/*driver interfaces*/
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>
#include <gpac/thread.h>

/*SDL*/
#include <SDL.h>

/*SDL init routines*/
Bool SDLOUT_InitSDL();
void SDLOUT_CloseSDL();

/*when not threaded on win32, locks happen randomly when setting the window's caption*/
#ifdef WIN32
//#define SDL_WINDOW_THREAD
#endif

typedef enum {
	SDL_STATE_STOPPED = 0,
	SDL_STATE_RUNNING,
	SDL_STATE_STOP_REQ,
	SDL_STATE_WAIT_FOR_THREAD_TERMINATION
} GF_SDL_STATE;

typedef struct
{
#ifdef	SDL_WINDOW_THREAD
	GF_Thread *sdl_th;
	GF_SDL_STATE sdl_th_state;
#endif
	GF_Mutex *evt_mx;
	Bool is_init, fullscreen;
	/*fullscreen display size*/
	u32 fs_width, fs_height;
	/*backbuffer size before entering fullscreen mode (used for restore)*/
	u32 store_width, store_height;
	/*cursors*/
	SDL_Cursor *curs_def, *curs_hand, *curs_collide;
	Bool use_systems_memory;


	Bool disable_vsync;

#if SDL_VERSION_ATLEAST(2,0,0)
	char szCaption[100];
	Bool enable_defer_mode;
	Bool needs_clear;
	Bool needs_bb_flush;
	Bool needs_bb_grab;

	SDL_GLContext gl_context;
	SDL_Renderer *renderer;
	SDL_Window *screen;

	SDL_Texture *tx_back_buffer;
	u8 *back_buffer_pixels;

	SDL_Texture *pool_rgb, *pool_rgba, *pool_yuv;

#else
	SDL_Surface *screen;
	SDL_Surface *back_buffer;

	SDL_Surface *pool_rgb, *pool_rgba;
	SDL_Overlay *yuv_overlay;
#endif

	u32 width, height;

	SDL_Surface *offscreen_gl;

	u32 output_3d_type;
	void *os_handle;

	Bool force_alpha, hidden;

	u32 last_mouse_move;
	Bool cursor_on;

	Bool ctrl_down, alt_down, meta_down;
} SDLVidCtx;

void SDL_DeleteVideo(void *ifce);
void *SDL_NewVideo();


/*
			SDL audio
*/
typedef struct
{
	u32 num_buffers, total_duration, delay_ms, total_size, volume, alloc_size;
	Bool is_init, is_running;
	u8 * audioBuff;
} SDLAudCtx;

void SDL_DeleteAudio(void *ifce);
void *SDL_NewAudio();

#endif
