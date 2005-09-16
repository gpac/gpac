/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


#include "sdl_out.h"


#ifdef WIN32
#include <windows.h>
#endif


/*cursors data*/
static char hand_data[] = 
{
	0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
	0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,2,2,1,2,2,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,1,2,2,1,2,2,1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,0,1,2,2,1,2,2,1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,2,2,1,1,2,2,2,2,2,2,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,2,2,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,2,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,2,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,2,2,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


static char collide_data[] =
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
	0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,1,0,0,1,1,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,1,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,1,1,0,0,1,1,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,1,1,1,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};




#define SDLVID()	SDLVidCtx *ctx = (SDLVidCtx *)dr->opaque

#if defined(__linux__)
#define HAVE_X11
#endif
#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

static u32 video_modes[] = 
{
	320, 200,
	320, 240,
	400, 300,
	600, 400,
	800, 600,
	1024, 768,
	1152, 864,
	1280, 1024
};
static u32 nb_video_modes = 8;

void SDLVid_SetCaption()
{
	char szName[100];
	if (SDL_VideoDriverName(szName, 100)) {
		char szCap[1024];
		sprintf(szCap, "SDL Video Output (%s)", szName);
		SDL_WM_SetCaption(szCap, NULL);
	} else {
		SDL_WM_SetCaption("SDL Video Output", NULL);
	}
}

SDL_Cursor *SDLVid_LoadCursor(char *maskdata)
{
	s32 ind, i, j;
	u8 data[4*32];
	u8 mask[4*32];

	ind = -1;
	for (i=0; i<32; i++) {
		for (j=0; j<32; j++) {
			if (j%8) {
				data[ind] <<= 1;
				mask[ind] <<= 1;
			} else {
				ind++;
				data[ind] = mask[ind] = 0;
			}
			switch (maskdata[j+32*i]) {
			/*black*/
			case 1:
				data[ind] |= 0x01;
			/*white*/
			case 2:
				mask[ind] |= 0x01;
				break;
			}
		}
	}
	return SDL_CreateCursor(data, mask, 32, 32, 0, 0);
}


static u32 SDLVid_TranslateActionKey(u32 VirtKey) 
{
	switch (VirtKey) {
	case SDLK_HOME: return GF_VK_HOME;
	case SDLK_END: return GF_VK_END;
	case SDLK_PAGEUP: return GF_VK_PRIOR;
	case SDLK_PAGEDOWN: return GF_VK_NEXT;
	case SDLK_UP: return GF_VK_UP;
	case SDLK_DOWN: return GF_VK_DOWN;
	case SDLK_LEFT: return GF_VK_LEFT;
	case SDLK_RIGHT: return GF_VK_RIGHT;
	case SDLK_F1: return GF_VK_F1;
	case SDLK_F2: return GF_VK_F2;
	case SDLK_F3: return GF_VK_F3;
	case SDLK_F4: return GF_VK_F4;
	case SDLK_F5: return GF_VK_F5;
	case SDLK_F6: return GF_VK_F6;
	case SDLK_F7: return GF_VK_F7;
	case SDLK_F8: return GF_VK_F8;
	case SDLK_F9: return GF_VK_F9;
	case SDLK_F10: return GF_VK_F10;
	case SDLK_F11: return GF_VK_F11;
	case SDLK_F12: return GF_VK_F12;
	case SDLK_RETURN: return GF_VK_RETURN;
	case SDLK_ESCAPE: return GF_VK_ESCAPE;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		return GF_VK_SHIFT;
	case SDLK_LCTRL: 
	case SDLK_RCTRL: 
		return GF_VK_CONTROL;
	case SDLK_LALT:
	case SDLK_RALT:
		return GF_VK_MENU;
	default: return 0;
	}
}

#if 0
void SDLVid_SetHack(void *os_handle, Bool set_on)
{
  unsetenv("SDL_WINDOWID=");
	if (!os_handle) return;
	if (set_on) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", (u32) os_handle);
		setenv("SDL_WINDOWID", buf, 1);
		sprintf(buf, "SDL_WINDOWID=%u", (u32) os_handle);
		putenv(buf);
	}
}
#endif

static void SDLVid_DestroyObjects(SDLVidCtx *ctx)
{
	if (ctx->back_buffer) SDL_FreeSurface(ctx->back_buffer);
	ctx->back_buffer = NULL;
}


#define SDL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE
#define SDL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_FULLSCREEN
#define SDL_GL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_RESIZABLE
#define SDL_GL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_FULLSCREEN

void SDLVid_ResizeWindow(GF_VideoOutput *dr, u32 width, u32 height) 
{
	SDLVID();
	GF_Event evt;

	/*lock X mutex to make sure the event queue is not being processed*/
	gf_mx_p(ctx->evt_mx);

	if (ctx->is_3D_out) {
		u32 flags;
		if ((ctx->width==width) && (ctx->height==height) ) {
			gf_mx_v(ctx->evt_mx);
			return;
		}
		flags = SDL_GL_WINDOW_FLAGS;
		if (ctx->os_handle) flags &= ~SDL_RESIZABLE;
		if (!ctx->screen) ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, ctx->screen->format->BitsPerPixel);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, 0);

		assert(width);
		assert(height);
		ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
		assert(ctx->screen);
		ctx->width = width;
		ctx->height = height;
		evt.type = GF_EVT_VIDEO_SETUP;
		dr->on_event(dr->evt_cbk_hdl, &evt);		
	} else {
		u32 flags = SDL_WINDOW_FLAGS;
		if (ctx->os_handle) flags &= ~SDL_RESIZABLE;
		ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
		assert(ctx->screen);
		if (!ctx->os_handle) SDLVid_SetCaption();
	}
	gf_mx_v(ctx->evt_mx);
}


u32 SDLVid_EventProc(void *par)
{
	u32 flags, last_mouse_move;
	Bool cursor_on;
	SDL_Event sdl_evt;
	GF_Event gpac_evt;
	GF_VideoOutput *dr = (GF_VideoOutput *)par;
	SDLVID();

	flags = SDL_WasInit(SDL_INIT_VIDEO);
	if (!(flags & SDL_INIT_VIDEO)) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO)<0) {
			ctx->sdl_th_state = 3;
			return 0;
		}
	}

	/*note we create the window on the first resize. This is the only way to keep synchronous with X and cope with GL threading*/
	
	ctx->sdl_th_state = 1;
	ctx->curs_def = SDL_GetCursor();
	ctx->curs_hand = SDLVid_LoadCursor(hand_data);
	ctx->curs_collide = SDLVid_LoadCursor(collide_data);
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	last_mouse_move = SDL_GetTicks();
	cursor_on = 1;

	/*save display resolution (SDL doesn't give acees to that)*/
	dr->max_screen_width = dr->max_screen_height = 0;
#ifdef HAVE_X11
    {
    Display *dpy = XOpenDisplay(NULL);
    if (dpy) {
        dr->max_screen_width = DisplayWidth(dpy, DefaultScreen(dpy));
        dr->max_screen_height = DisplayHeight(dpy, DefaultScreen(dpy));
		XCloseDisplay(dpy);
    }
    }
#endif
#ifdef WIN32
    dr->max_screen_width = GetSystemMetrics(SM_CXSCREEN);
    dr->max_screen_height = GetSystemMetrics(SM_CYSCREEN);
#endif
    
	while (ctx->sdl_th_state==1) {
		/*after much testing: we must ensure nothing is using the event queue when resizing window.
		-- under X, it throws Xlib "unexpected async reply" under linux, therefore we don't wait events,
		we check for events and execute them if any
		-- under Win32, the SDL_SetVideoMode deadlocks, so we don't force exclusive access to events
		*/
#ifndef WIN32
		gf_mx_p(ctx->evt_mx);
#endif
		while (SDL_PollEvent(&sdl_evt)) {
			switch (sdl_evt.type) {
			case SDL_VIDEORESIZE:
			  	gpac_evt.type = GF_EVT_SIZE;
				gpac_evt.size.width = sdl_evt.resize.w;
				gpac_evt.size.height = sdl_evt.resize.h;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			case SDL_QUIT:
				if (ctx->sdl_th_state==1) {
					gpac_evt.type = GF_EVT_QUIT;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				} else {
					goto exit;
				}
				break;
			case SDL_VIDEOEXPOSE:
				gpac_evt.type = GF_EVT_REFRESH;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;

			/*keyboard*/
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				gpac_evt.key.vk_code = SDLVid_TranslateActionKey(sdl_evt.key.keysym.sym);
				gpac_evt.key.virtual_code = sdl_evt.key.keysym.sym;
				if (gpac_evt.key.vk_code) {
					gpac_evt.type = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
					if (gpac_evt.key.vk_code<=GF_VK_RIGHT) gpac_evt.key.virtual_code = 0;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					/*also send a normal key for non-key-sensors*/
					if (gpac_evt.key.vk_code>GF_VK_RIGHT) goto send_key;
				} else {
send_key:
					gpac_evt.type = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_EVT_KEYDOWN : GF_EVT_KEYUP;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					if ((sdl_evt.key.type==SDL_KEYDOWN) && sdl_evt.key.keysym.unicode) {
						gpac_evt.character.unicode_char = sdl_evt.key.keysym.unicode;
						gpac_evt.type = GF_EVT_CHAR;
						dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					}
				}
				break;

			/*mouse*/
			case SDL_MOUSEMOTION:
				last_mouse_move = SDL_GetTicks();
				gpac_evt.type = GF_EVT_MOUSEMOVE;
				gpac_evt.mouse.x = sdl_evt.motion.x;
				gpac_evt.mouse.y = sdl_evt.motion.y;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				last_mouse_move = SDL_GetTicks();
				gpac_evt.mouse.x = sdl_evt.motion.x;
				gpac_evt.mouse.y = sdl_evt.motion.y;
				switch (sdl_evt.button.button) {
				case SDL_BUTTON_LEFT:
					gpac_evt.type = (sdl_evt.type==SDL_MOUSEBUTTONUP) ? GF_EVT_LEFTUP : GF_EVT_LEFTDOWN;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					break;
				case SDL_BUTTON_MIDDLE:
					gpac_evt.type = (sdl_evt.type==SDL_MOUSEBUTTONUP) ? GF_EVT_MIDDLEUP : GF_EVT_MIDDLEDOWN;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					break;
				case SDL_BUTTON_RIGHT:
					gpac_evt.type = (sdl_evt.type==SDL_MOUSEBUTTONUP) ? GF_EVT_RIGHTUP : GF_EVT_RIGHTDOWN;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					break;
#ifdef SDL_BUTTON_WHEELUP
				case SDL_BUTTON_WHEELUP:
				case SDL_BUTTON_WHEELDOWN:
					/*SDL handling is not perfect there, it just says up/down but no info on how much
					the wheel was rotated...*/
					gpac_evt.mouse.wheel_pos = (sdl_evt.button.button==SDL_BUTTON_WHEELUP) ? FIX_ONE : -FIX_ONE;
					gpac_evt.type = GF_EVT_MOUSEWHEEL;
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
					break;
#endif
				}
				break;
			}
		}

#ifndef WIN32
		gf_mx_v(ctx->evt_mx);
#endif

		/*looks like this hides the cursor for ever when switching back from FS*/
#if 0
		if (ctx->fullscreen && (last_mouse_move + 2000 < SDL_GetTicks()) ) {
			if (cursor_on) SDL_ShowCursor(0);
			cursor_on = 0;
		} else if (!cursor_on) {
			SDL_ShowCursor(1);
			cursor_on = 1;
		}
#endif
		
		gf_sleep(5);
	}

exit:
	SDLVid_DestroyObjects(ctx);
	SDL_FreeCursor(ctx->curs_hand);
	SDL_FreeCursor(ctx->curs_collide);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	ctx->sdl_th_state = 3;
	return 0;
}


GF_Err SDLVid_Setup(struct _video_out *dr, void *os_handle, void *os_display, u32 no_proc_override, GF_GLConfig *cfg)
{
	SDLVID();
	/*we don't allow SDL hack, not stable enough*/
	//if (os_handle) return GF_NOT_SUPPORTED;
	ctx->os_handle = os_handle;
	ctx->is_init = 0;
	ctx->is_3D_out = cfg ? 1 : 0;
	if (!SDLOUT_InitSDL()) return GF_IO_ERR;
	ctx->sdl_th_state = 0;
	gf_th_run(ctx->sdl_th, SDLVid_EventProc, dr);
	while (!ctx->sdl_th_state) gf_sleep(10);
	if (ctx->sdl_th_state==3) {
		SDLOUT_CloseSDL();
		ctx->sdl_th_state = 0;
		return GF_IO_ERR;
	}
	ctx->is_init = 1;
	return GF_OK;
}

static void SDLVid_Shutdown(GF_VideoOutput *dr)
{
	SDLVID();
	/*remove all surfaces*/

	if (!ctx->is_init) return;
	if (ctx->sdl_th_state==1) {
		SDL_Event evt;
		ctx->sdl_th_state = 2;
		evt.type = SDL_QUIT;
		SDL_PushEvent(&evt);
		while (ctx->sdl_th_state != 3) gf_sleep(100);
	}
	SDLOUT_CloseSDL();
	ctx->is_init = 0;
}


GF_Err SDLVid_SetFullScreen(GF_VideoOutput *dr, u32 bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
	u32 bpp, pref_bpp;
	SDLVID();

	if (ctx->fullscreen==bFullScreenOn) return GF_OK;

	/*lock to get sure the event queue is not processed under X*/
	gf_mx_p(ctx->evt_mx);
	ctx->fullscreen = bFullScreenOn;

	pref_bpp = bpp = ctx->screen->format->BitsPerPixel;

	if (ctx->fullscreen) {
		u32 flags;
		Bool switch_res = 0;
		const char *sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "SwitchResolution");
		if (sOpt && !stricmp(sOpt, "yes")) switch_res = 1;
		if (!dr->max_screen_width || !dr->max_screen_height) switch_res = 1;

		flags = ctx->is_3D_out ? SDL_GL_FULLSCREEN_FLAGS : SDL_FULLSCREEN_FLAGS;
		ctx->store_width = *screen_width;
		ctx->store_height = *screen_height;
		if (switch_res) {
			u32 i;
			ctx->fs_width = *screen_width;
			ctx->fs_height = *screen_height;
			for(i=0; i<nb_video_modes; i++) {
				if (ctx->fs_width<=video_modes[2*i] && ctx->fs_height<=video_modes[2*i + 1]) {
					if ((pref_bpp = SDL_VideoModeOK(video_modes[2*i], video_modes[2*i+1], bpp, flags))) {
						ctx->fs_width = video_modes[2*i];
						ctx->fs_height = video_modes[2*i + 1];
						break;
					}
				}
			}
		} else {
			ctx->fs_width = dr->max_screen_width;
			ctx->fs_height = dr->max_screen_height;
		}
		ctx->screen = SDL_SetVideoMode(ctx->fs_width, ctx->fs_height, pref_bpp, flags);
		/*we switched bpp, clean all objects*/
		if (bpp != pref_bpp) SDLVid_DestroyObjects(ctx);
		*screen_width = ctx->fs_width;
		*screen_height = ctx->fs_height;
		/*GL has changed*/
		if (ctx->is_3D_out) {
			GF_Event evt;
			evt.type = GF_EVT_VIDEO_SETUP;
			dr->on_event(dr->evt_cbk_hdl, &evt);
		}
	} else {
		SDLVid_ResizeWindow(dr, ctx->store_width, ctx->store_height);
		*screen_width = ctx->store_width;
		*screen_height = ctx->store_height;
	}
	gf_mx_v(ctx->evt_mx);
	if (!ctx->screen) return GF_IO_ERR;
	return GF_OK;
}

GF_Err SDLVid_SetBackbufferSize(GF_VideoOutput *dr, u32 newWidth, u32 newHeight)
{
	u32 col;
	const char *opt;
	SDLVID();
	
	if (ctx->is_3D_out) return GF_BAD_PARAM;

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "UseHardwareMemory");
	ctx->systems_memory = (opt && !strcmp(opt, "yes")) ? 0 : 1;

	/*clear screen*/
	col = SDL_MapRGB(ctx->screen->format, 0, 0, 0);
	SDL_FillRect(ctx->screen, NULL, col);
	SDL_Flip(ctx->screen);

	if (ctx->back_buffer && ((u32) ctx->back_buffer->w==newWidth) && ((u32) ctx->back_buffer->h==newHeight)) {
		return GF_OK;
	}
	if (ctx->back_buffer) SDL_FreeSurface(ctx->back_buffer);
	ctx->back_buffer = SDL_CreateRGBSurface(ctx->systems_memory ? SDL_SWSURFACE : SDL_HWSURFACE, newWidth, newHeight, ctx->screen->format->BitsPerPixel, ctx->screen->format->Rmask, ctx->screen->format->Gmask, ctx->screen->format->Bmask, 0);
	ctx->width = newWidth;
	ctx->height = newHeight;
	if (!ctx->back_buffer) return GF_IO_ERR;

	return GF_OK;
}

u32 SDLVid_MapPixelFormat(SDL_PixelFormat *format)
{
	if (format->palette) return 0;
	switch (format->BitsPerPixel) {
	case 16:
		if ((format->Rmask==0x7c00) && (format->Gmask==0x03e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_555;
		if ((format->Rmask==0xf800) && (format->Gmask==0x07e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_565;
		return 0;
	case 24:
		if (format->Rmask==0x00FF0000) return GF_PIXEL_RGB_24;
		if (format->Rmask==0x000000FF) return GF_PIXEL_BGR_24;
		return 0;
	case 32:
		if (format->Amask==0xFF000000) return GF_PIXEL_ARGB;
		if (format->Rmask==0x00FF0000) return GF_PIXEL_RGB_32;
		if (format->Rmask==0x000000FF) return GF_PIXEL_BGR_32;
		return 0;
	default:
		return 0;
	}
}

static GF_Err SDLVid_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *video_info, Bool do_lock)
{
	SDLVID();
	if (!ctx->back_buffer) return GF_BAD_PARAM;
	if (do_lock) {
		if (!video_info) return GF_BAD_PARAM;
		if (SDL_LockSurface(ctx->back_buffer)<0) return GF_IO_ERR;
		video_info->width = ctx->back_buffer->w;
		video_info->height = ctx->back_buffer->h;
		video_info->pitch = ctx->back_buffer->pitch;
		video_info->video_buffer = ctx->back_buffer->pixels;
		video_info->pixel_format = SDLVid_MapPixelFormat(ctx->back_buffer->format);
		video_info->is_hardware_memory = !ctx->systems_memory;
	} else {
		SDL_UnlockSurface(ctx->back_buffer);
	}
	return GF_OK;
}

static GF_Err SDLVid_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	SDL_Rect rc;
	SDLVID();
	/*if resizing don't process otherwise we may deadlock*/
	if (!ctx->screen) return GF_OK;

	if (ctx->is_3D_out) {
		SDL_GL_SwapBuffers();
		return GF_OK;
	}
	if (!ctx->back_buffer) return GF_BAD_PARAM;

	if ((dest->w != (u32) ctx->back_buffer->w) || (dest->h != (u32) ctx->back_buffer->h)) {
		GF_VideoSurface src, dst;

		SDL_LockSurface(ctx->back_buffer);
		SDL_LockSurface(ctx->screen);

		src.height = ctx->back_buffer->h;
		src.width = ctx->back_buffer->w;
		src.pitch = ctx->back_buffer->pitch;
		src.pixel_format = SDLVid_MapPixelFormat(ctx->back_buffer->format);
		src.video_buffer = ctx->back_buffer->pixels;

		dst.height = ctx->screen->h;
		dst.width = ctx->screen->w;
		dst.pitch = ctx->screen->pitch;
		dst.pixel_format = SDLVid_MapPixelFormat(ctx->screen->format);
		dst.video_buffer = ctx->screen->pixels;

		gf_stretch_bits(&dst, &src, dest, NULL, 0, 0xFF, 0, NULL, NULL);

		SDL_UnlockSurface(ctx->back_buffer);
		SDL_UnlockSurface(ctx->screen);

	} else {
		rc.x = dest->x; rc.y = dest->y; rc.w = dest->w; rc.h = dest->h;
		SDL_BlitSurface(ctx->back_buffer, NULL, ctx->screen, &rc);
	}
	SDL_Flip(ctx->screen);
	return GF_OK;
}

static void SDLVid_SetCursor(GF_VideoOutput *dr, u32 cursor_type)
{
	SDLVID();
	switch (cursor_type) {
	case GF_CURSOR_ANCHOR:
	case GF_CURSOR_TOUCH:
	case GF_CURSOR_ROTATE:
	case GF_CURSOR_PROXIMITY:
	case GF_CURSOR_PLANE:
		SDL_SetCursor(ctx->curs_hand);
		break;
	case GF_CURSOR_COLLIDE:
		SDL_SetCursor(ctx->curs_collide);
		break;
	default:
		SDL_SetCursor(ctx->curs_def);
		break;
	}
}

static GF_Err SDLVid_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	if (!evt) return GF_OK;
	switch (evt->type) {
	case GF_EVT_SET_CURSOR:
		SDLVid_SetCursor(dr, evt->cursor.cursor_type);
		break;
	case GF_EVT_SET_CAPTION:
		SDL_WM_SetCaption(evt->caption.caption, NULL);
		break;
	case GF_EVT_SHOWHIDE:
		/*the only way to have proper show/hide with SDL is to shutdown the video system and reset it up
		which we don't want to do since the setup MUST occur in the rendering thread for some configs (openGL)*/
		return GF_NOT_SUPPORTED;
	case GF_EVT_SIZE:
		SDLVid_ResizeWindow(dr, evt->size.width, evt->size.height);
		break;
	case GF_EVT_VIDEO_SETUP:
	{
		SDLVID();
		if (ctx->is_3D_out) SDLVid_ResizeWindow(dr, evt->size.width, evt->size.height);
		else SDLVid_SetBackbufferSize(dr, evt->size.width, evt->size.height);
	}
		break;
	}
	return GF_OK;
}




void *SDL_NewVideo()
{
	SDLVidCtx *ctx;
	GF_VideoOutput *driv;
	
	driv = malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "SDL Video Output", "gpac distribution");

	ctx = malloc(sizeof(SDLVidCtx));
	memset(ctx, 0, sizeof(SDLVidCtx));
	ctx->sdl_th = gf_th_new();
	ctx->evt_mx = gf_mx_new();
	
	driv->opaque = ctx;
	driv->Setup = SDLVid_Setup;
	driv->Shutdown = SDLVid_Shutdown;
	driv->SetFullScreen = SDLVid_SetFullScreen;
	driv->Flush = SDLVid_Flush;
	driv->ProcessEvent = SDLVid_ProcessEvent;
	driv->hw_caps |= GF_VIDEO_HW_HAS_OPENGL;

	/*no YUV hardware blitting in SDL (only overlays)*/

	/*NO BLIT in SDL (we rely on GPAC to do it by soft)*/
	driv->Blit = NULL;
	driv->LockBackBuffer = SDLVid_LockBackBuffer;
	driv->LockOSContext = NULL;

	return driv;
}

void SDL_DeleteVideo(void *ifce)
{
	GF_VideoOutput *dr = (GF_VideoOutput *)ifce;
	SDLVID();
	gf_th_del(ctx->sdl_th);
	gf_mx_del(ctx->evt_mx);
	free(ctx);
	free(dr);
}

