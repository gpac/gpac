/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Jean Le Feuvre - Romain Bouqueau
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

#include "sdl_out.h"
#include <gpac/user.h>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef GPAC_CONFIG_IOS
#include <OpenGLES/ES1/gl.h>
#include <OpenGLES/ES1/glext.h>
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



#if SDL_VERSION_ATLEAST(2,0,0)
void SDLVid_SetCaption(SDL_Window* window)
{
	if (SDL_GetCurrentVideoDriver()) {
		char szCap[1024];
		sprintf(szCap, "SDL Video Output (%s)", SDL_GetCurrentVideoDriver());
		SDL_SetWindowTitle(window, szCap);
	} else {
		SDL_SetWindowTitle(window, "SDL Video Output");
	}
}
#else

#define SDLK_KP_0	SDLK_KP0
#define SDLK_KP_1	SDLK_KP1
#define SDLK_KP_2	SDLK_KP2
#define SDLK_KP_3	SDLK_KP3
#define SDLK_KP_4	SDLK_KP4
#define SDLK_KP_5	SDLK_KP5
#define SDLK_KP_6	SDLK_KP6
#define SDLK_KP_7	SDLK_KP7
#define SDLK_KP_8	SDLK_KP8
#define SDLK_KP_9	SDLK_KP9
#define SDLK_NUMLOCKCLEAR SDLK_NUMLOCK
#define SDLK_SCROLLLOCK SDLK_SCROLLOCK
#define SDLK_APPLICATION SDLK_COMPOSE
#define SDLK_PRINTSCREEN SDLK_PRINT
#define SDL_WINDOW_RESIZABLE SDL_RESIZABLE

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
static const u32 nb_video_modes = 8;

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
#endif



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


static void sdl_translate_key(u32 SDLkey, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = SDLkey;
	switch (SDLkey) {
	case SDLK_BACKSPACE:
		evt->key_code = GF_KEY_BACKSPACE;
		break;
	case SDLK_TAB:
		evt->key_code = GF_KEY_TAB;
		break;
	case SDLK_CLEAR:
		evt->key_code = GF_KEY_CLEAR;
		break;
	case SDLK_PAUSE:
		evt->key_code = GF_KEY_PAUSE;
		break;
	case SDLK_ESCAPE:
		evt->key_code = GF_KEY_ESCAPE;
		break;
	case SDLK_SPACE:
		evt->key_code = GF_KEY_SPACE;
		break;

	case SDLK_KP_ENTER:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_RETURN:
		evt->key_code = GF_KEY_ENTER;
		break;

	case SDLK_KP_MULTIPLY:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_ASTERISK:
		evt->key_code = GF_KEY_STAR;
		break;
	case SDLK_KP_PLUS:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_PLUS:
		evt->key_code = GF_KEY_PLUS;
		break;
	case SDLK_KP_MINUS:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_MINUS:
		evt->key_code = GF_KEY_HYPHEN;
		break;
	case SDLK_KP_DIVIDE:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_SLASH:
		evt->key_code = GF_KEY_SLASH;
		break;

	case SDLK_KP_0:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_0:
		evt->key_code = GF_KEY_0;
		break;
	case SDLK_KP_1:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_1:
		evt->key_code = GF_KEY_1;
		break;
	case SDLK_KP_2:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_2:
		evt->key_code = GF_KEY_2;
		break;
	case SDLK_KP_3:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_3:
		evt->key_code = GF_KEY_3;
		break;
	case SDLK_KP_4:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_4:
		evt->key_code = GF_KEY_4;
		break;
	case SDLK_KP_5:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_5:
		evt->key_code = GF_KEY_5;
		break;
	case SDLK_KP_6:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_6:
		evt->key_code = GF_KEY_6;
		break;
	case SDLK_KP_7:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_7:
		evt->key_code = GF_KEY_7;
		break;
	case SDLK_KP_8:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_8:
		evt->key_code = GF_KEY_8;
		break;
	case SDLK_KP_9:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_9:
		evt->key_code = GF_KEY_9;
		break;
	case SDLK_KP_PERIOD:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_PERIOD:
		evt->key_code = GF_KEY_FULLSTOP;
		break;
	case SDLK_KP_EQUALS:
		evt->flags = GF_KEY_EXT_NUMPAD;
	case SDLK_EQUALS:
		evt->key_code = GF_KEY_EQUALS;
		break;

	case SDLK_EXCLAIM:
		evt->key_code = GF_KEY_EXCLAMATION;
		break;
	case SDLK_QUOTEDBL:
		evt->key_code = GF_KEY_QUOTATION;
		break;
	case SDLK_HASH:
		evt->key_code = GF_KEY_NUMBER;
		break;
	case SDLK_DOLLAR:
		evt->key_code = GF_KEY_DOLLAR;
		break;
	case SDLK_AMPERSAND:
		evt->key_code = GF_KEY_AMPERSAND;
		break;
	case SDLK_QUOTE:
		evt->key_code = GF_KEY_APOSTROPHE;
		break;
	case SDLK_LEFTPAREN:
		evt->key_code = GF_KEY_LEFTPARENTHESIS;
		break;
	case SDLK_RIGHTPAREN:
		evt->key_code = GF_KEY_RIGHTPARENTHESIS;
		break;
	case SDLK_COMMA:
		evt->key_code = GF_KEY_COMMA;
		break;
	case SDLK_COLON:
		evt->key_code = GF_KEY_COLON;
		break;
	case SDLK_SEMICOLON:
		evt->key_code = GF_KEY_SEMICOLON;
		break;
	case SDLK_LESS:
		evt->key_code = GF_KEY_LESSTHAN;
		break;
	case SDLK_GREATER:
		evt->key_code = GF_KEY_GREATERTHAN;
		break;
	case SDLK_QUESTION:
		evt->key_code = GF_KEY_QUESTION;
		break;
	case SDLK_AT:
		evt->key_code = GF_KEY_AT;
		break;
	case SDLK_LEFTBRACKET:
		evt->key_code = GF_KEY_LEFTSQUAREBRACKET;
		break;
	case SDLK_RIGHTBRACKET:
		evt->key_code = GF_KEY_RIGHTSQUAREBRACKET;
		break;
	case SDLK_BACKSLASH:
		evt->key_code = GF_KEY_BACKSLASH;
		break;
	case SDLK_UNDERSCORE:
		evt->key_code = GF_KEY_UNDERSCORE;
		break;
	case SDLK_BACKQUOTE:
		evt->key_code = GF_KEY_GRAVEACCENT;
		break;
	case SDLK_DELETE:
		evt->key_code = GF_KEY_DEL;
		break;
	case SDLK_UNDO:
		evt->key_code = GF_KEY_UNDO;
		break;

	case SDLK_UP:
		evt->key_code = GF_KEY_UP;
		break;
	case SDLK_DOWN:
		evt->key_code = GF_KEY_DOWN;
		break;
	case SDLK_RIGHT:
		evt->key_code = GF_KEY_RIGHT;
		break;
	case SDLK_LEFT:
		evt->key_code = GF_KEY_LEFT;
		break;
	case SDLK_INSERT:
		evt->key_code = GF_KEY_INSERT;
		break;
	case SDLK_HOME:
		evt->key_code = GF_KEY_HOME;
		break;
	case SDLK_END:
		evt->key_code = GF_KEY_END;
		break;
	case SDLK_PAGEUP:
		evt->key_code = GF_KEY_PAGEUP;
		break;
	case SDLK_PAGEDOWN:
		evt->key_code = GF_KEY_PAGEDOWN;
		break;
	case SDLK_F1:
		evt->key_code = GF_KEY_F1;
		break;
	case SDLK_F2:
		evt->key_code = GF_KEY_F2;
		break;
	case SDLK_F3:
		evt->key_code = GF_KEY_F3;
		break;
	case SDLK_F4:
		evt->key_code = GF_KEY_F4;
		break;
	case SDLK_F5:
		evt->key_code = GF_KEY_F5;
		break;
	case SDLK_F6:
		evt->key_code = GF_KEY_F6;
		break;
	case SDLK_F7:
		evt->key_code = GF_KEY_F7;
		break;
	case SDLK_F8:
		evt->key_code = GF_KEY_F8;
		break;
	case SDLK_F9:
		evt->key_code = GF_KEY_F9;
		break;
	case SDLK_F10:
		evt->key_code = GF_KEY_F10;
		break;
	case SDLK_F11:
		evt->key_code = GF_KEY_F11;
		break;
	case SDLK_F12:
		evt->key_code = GF_KEY_F12;
		break;
	case SDLK_F13:
		evt->key_code = GF_KEY_F13;
		break;
	case SDLK_F14:
		evt->key_code = GF_KEY_F14;
		break;
	case SDLK_F15:
		evt->key_code = GF_KEY_F15;
		break;
	case SDLK_NUMLOCKCLEAR:
		evt->key_code = GF_KEY_NUMLOCK;
		break;
	case SDLK_CAPSLOCK:
		evt->key_code = GF_KEY_CAPSLOCK;
		break;
	case SDLK_SCROLLLOCK:
		evt->key_code = GF_KEY_SCROLL;
		break;

	case SDLK_RSHIFT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case SDLK_LSHIFT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case SDLK_LCTRL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case SDLK_RCTRL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case SDLK_LALT:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case SDLK_RALT:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
#if (SDL_MAJOR_VERSION<=1) && (SDL_MINOR_VERSION<3)
	case SDLK_LMETA:
	case SDLK_LSUPER:
#else
	case SDLK_LGUI:
#endif
		evt->key_code = GF_KEY_META;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
#if (SDL_MAJOR_VERSION<=1) && (SDL_MINOR_VERSION<3)
	case SDLK_RMETA:
	case SDLK_RSUPER:
#else
	case SDLK_RGUI:
#endif
		evt->key_code = GF_KEY_META;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case SDLK_MODE:
		evt->key_code = GF_KEY_MODECHANGE;
		break;
	case SDLK_APPLICATION:
		evt->key_code = GF_KEY_COMPOSE;
		break;
	case SDLK_HELP:
		evt->key_code = GF_KEY_HELP;
		break;
	case SDLK_PRINTSCREEN:
		evt->key_code = GF_KEY_PRINTSCREEN;
		break;

#if (SDL_MAJOR_VERSION>=1) && (SDL_MINOR_VERSION>=3)
	/*
		SDLK_CARET		= 94,
	 */
	case SDLK_a:
	case SDLK_b:
	case SDLK_c:
	case SDLK_d:
	case SDLK_e:
	case SDLK_f:
	case SDLK_g:
	case SDLK_h:
	case SDLK_i:
	case SDLK_j:
	case SDLK_k:
	case SDLK_l:
	case SDLK_m:
	case SDLK_n:
	case SDLK_o:
	case SDLK_p:
	case SDLK_q:
	case SDLK_r:
	case SDLK_s:
	case SDLK_t:
	case SDLK_u:
	case SDLK_v:
	case SDLK_w:
	case SDLK_x:
	case SDLK_y:
	case SDLK_z:
		evt->key_code = GF_KEY_A + SDLkey - SDLK_a;
		break;
		/*
		SDLK_DELETE		= 127,

		SDLK_SYSREQ		= 317,
		SDLK_POWER		= 320,

		*/
#endif

	default:
		if ((SDLkey>=0x30) && (SDLkey<=0x39))  evt->key_code = GF_KEY_0 + SDLkey-0x30;
		else if ((SDLkey>=0x41) && (SDLkey<=0x5A))  evt->key_code = GF_KEY_A + SDLkey-0x41;
		else if ((SDLkey>=0x61) && (SDLkey<=0x7A))  evt->key_code = GF_KEY_A + SDLkey-0x61;
		else
		{
			evt->key_code = GF_KEY_UNIDENTIFIED;
		}
		break;
	}
}

#if 0
void SDLVid_SetHack(void *os_handle, Bool set_on)
{
	char buf[50];
	if (set_on && os_handle) {
		sprintf(buf, "SDL_WINDOWID=%u", (u32) os_handle);
	} else {
		strcpy(buf, "SDL_WINDOWID=");
	}
#ifdef WIN32
	putenv(buf);
#else
	if (set_on) unsetenv("SDL_WINDOWID=");
	else putenv(buf);
#endif
}
#endif

static void SDLVid_DestroyObjects(SDLVidCtx *ctx)
{
#if SDL_VERSION_ATLEAST(2,0,0)

	if (ctx->pool_rgb) SDL_DestroyTexture(ctx->pool_rgb);
	ctx->pool_rgb = NULL;
	if (ctx->pool_rgba) SDL_DestroyTexture(ctx->pool_rgba);
	ctx->pool_rgba = NULL;
	if (ctx->pool_yuv) SDL_DestroyTexture(ctx->pool_yuv);
	ctx->pool_yuv = NULL;

	if (ctx->tx_back_buffer) SDL_DestroyTexture(ctx->tx_back_buffer);
	ctx->tx_back_buffer = NULL;
	if (ctx->back_buffer_pixels) gf_free(ctx->back_buffer_pixels);
	ctx->back_buffer_pixels = NULL;

#else
	if (ctx->back_buffer) SDL_FreeSurface(ctx->back_buffer);
	ctx->back_buffer = NULL;
	if (ctx->pool_rgb) SDL_FreeSurface(ctx->pool_rgb);
	ctx->pool_rgb = NULL;
	if (ctx->pool_rgba) SDL_FreeSurface(ctx->pool_rgba);
	ctx->pool_rgba = NULL;
	SDL_FreeYUVOverlay(ctx->yuv_overlay);
	ctx->yuv_overlay=NULL;
#endif
}

#if SDL_VERSION_ATLEAST(2,0,0)
#ifdef GPAC_CONFIG_IOS
#define SDL_WINDOW_FLAGS			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE
#define SDL_FULLSCREEN_FLAGS		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE
#define SDL_GL_WINDOW_FLAGS			SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE
#define SDL_GL_FULLSCREEN_FLAGS		SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE
#else
#define SDL_WINDOW_FLAGS			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
#define SDL_FULLSCREEN_FLAGS		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
#define SDL_GL_WINDOW_FLAGS			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
#define SDL_GL_FULLSCREEN_FLAGS		SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
#endif
#else
#ifdef GPAC_CONFIG_IOS
#define SDL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE | SDL_NOFRAME
#define SDL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_FULLSCREEN | SDL_NOFRAME
#define SDL_GL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_RESIZABLE | SDL_NOFRAME
#define SDL_GL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_FULLSCREEN | SDL_NOFRAME
#else
#define SDL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE
#define SDL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_FULLSCREEN
#define SDL_GL_WINDOW_FLAGS			SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_RESIZABLE
#define SDL_GL_FULLSCREEN_FLAGS		SDL_HWSURFACE | SDL_OPENGL | SDL_HWACCEL | SDL_FULLSCREEN
#endif
#endif


GF_Err SDLVid_ResizeWindow(GF_VideoOutput *dr, u32 width, u32 height)
{
	Bool hw_reset = GF_FALSE;
	SDLVID();
	GF_Event evt;

	/*lock X mutex to make sure the event queue is not being processed*/
	gf_mx_p(ctx->evt_mx);

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[SDL] Resizing window %dx%d\n", width, height));

	if (ctx->output_3d_type) {
		u32 flags, nb_bits;
		const char *opt;

		if (ctx->screen && (ctx->width==width) && (ctx->height==height) ) {
			gf_mx_v(ctx->evt_mx);
			return GF_OK;
		}

#if SDL_VERSION_ATLEAST(2,0,0)
		flags = SDL_GL_WINDOW_FLAGS;
		if (ctx->os_handle) flags &= ~SDL_WINDOW_RESIZABLE;
		if (ctx->fullscreen) flags |= SDL_FULLSCREEN_FLAGS;
#else
		flags = SDL_GL_WINDOW_FLAGS;
		if (ctx->os_handle) flags &= ~SDL_RESIZABLE;
		if (ctx->fullscreen) flags |= SDL_FULLSCREEN_FLAGS;
		if (!ctx->screen) ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
#endif
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		opt = gf_opts_get_key("core", "gl-bits-depth");
		nb_bits = opt ? atoi(opt) : 16;
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, nb_bits);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
		opt = gf_opts_get_key("core", "gl-bits-comp");
		nb_bits = opt ? atoi(opt) : 8;
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, nb_bits);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, nb_bits);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, nb_bits);

#if SDL_VERSION_ATLEAST(2,0,0)
		if (ctx->hidden)
			flags |= SDL_WINDOW_HIDDEN;
#endif
		
		assert(width);
		assert(height);
#if SDL_VERSION_ATLEAST(2,0,0)

#ifdef GPAC_USE_GLES2
		/* Set the correct attributes for MASK and MAJOR version */
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#endif

		if (!ctx->screen) {
			if (!(ctx->screen = SDL_CreateWindow("", 0, 0, width, height, flags))) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot create window: %s\n", SDL_GetError()));
				gf_mx_v(ctx->evt_mx);
				return GF_IO_ERR;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[SDL] Window created\n"));
		}

		if ( !ctx->gl_context ) {
			if (!(ctx->gl_context = SDL_GL_CreateContext(ctx->screen))) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot initialize gl context: %s\n", SDL_GetError()));
				gf_mx_v(ctx->evt_mx);
				return GF_IO_ERR;
			}
			if (SDL_GL_MakeCurrent(ctx->screen, ctx->gl_context)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot make context current: %s\n", SDL_GetError()));
				gf_mx_v(ctx->evt_mx);
				return GF_IO_ERR;
			}
			hw_reset = GF_TRUE;
		}

		if (!ctx->disable_vsync)
			ctx->disable_vsync = gf_opts_get_bool("core", "disable-vsync");

		if (ctx->disable_vsync) {
#if defined(__APPLE__) && !defined(GPAC_CONFIG_IOS)
#else
			SDL_GL_SetSwapInterval(0);
#endif
		}

		SDL_SetWindowSize(ctx->screen, width, height);

#else
		hw_reset = GF_TRUE;
		ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
		if (!ctx->screen) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot create window: %s\n", SDL_GetError()));
			gf_mx_v(ctx->evt_mx);
			return GF_IO_ERR;
		}
#endif
		ctx->width = width;
		ctx->height = height;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.hw_reset = hw_reset;
		dr->on_event(dr->evt_cbk_hdl, &evt);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[SDL] 3D Setup done\n"));
	} else {
		u32 flags;

#ifdef GPAC_CONFIG_IOS
		flags = SDL_FULLSCREEN_FLAGS;
		//SDL readme says it would make us faster
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
#else
		flags = SDL_WINDOW_FLAGS | SDL_WINDOW_RESIZABLE;

		if (ctx->os_handle) flags &= ~SDL_WINDOW_RESIZABLE;

#endif
		if (ctx->os_handle) flags &= ~SDL_WINDOW_RESIZABLE;

#if SDL_VERSION_ATLEAST(2,0,0)

#ifdef GPAC_CONFIG_IOS
		//still some issues with render to tager and landscape orientation, we need to reset everything ...
		if (ctx->enable_defer_mode) {
			if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
			ctx->renderer=NULL;
			if (ctx->screen) SDL_DestroyWindow(ctx->screen);
			ctx->screen=NULL;
		}
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
		if (ctx->hidden)
			flags |= SDL_WINDOW_HIDDEN;
#endif

		if (!ctx->screen) {
			ctx->screen = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);

			if (!ctx->screen) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot create window: %s\n", SDL_GetError()));
				gf_mx_v(ctx->evt_mx);
				return GF_IO_ERR;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[SDL] Window created\n"));
			SDL_RaiseWindow(ctx->screen);
		}
		if ( !ctx->renderer ) {
			u32 flags = SDL_RENDERER_ACCELERATED;
			if (! gf_opts_get_bool("core", "disable-vsync")) {
				flags |= SDL_RENDERER_PRESENTVSYNC;
			}


			if (!(ctx->renderer = SDL_CreateRenderer(ctx->screen, -1, flags))) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot create renderer: %s\n", SDL_GetError()));
				gf_mx_v(ctx->evt_mx);
				return GF_IO_ERR;
			}
		}
#ifndef GPAC_CONFIG_IOS
		SDL_SetWindowSize(ctx->screen, width, height);
#endif
		SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
		SDL_RenderClear(ctx->renderer);

#else
		ctx->screen = SDL_SetVideoMode(width, height, 0, flags);
#endif
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[SDL] 2D Setup done\n"));
	}
	gf_mx_v(ctx->evt_mx);
	return ctx->screen ? GF_OK : GF_IO_ERR;
}

static void SDLVid_SetCursor(GF_VideoOutput *dr, u32 cursor_type)
{
#ifndef GPAC_CONFIG_IOS
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
#endif
}

static Bool SDLVid_InitializeWindow(SDLVidCtx *ctx, GF_VideoOutput *dr)
{
	u32 flags;
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_DisplayMode vinf;
#else
	const SDL_VideoInfo *vinf;
#endif

#ifdef WIN32
	putenv("directx");
#endif

	flags = SDL_WasInit(SDL_INIT_VIDEO);
	if (!(flags & SDL_INIT_VIDEO)) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
			return GF_FALSE;
		}
	}

	ctx->curs_def = SDL_GetCursor();
	ctx->curs_hand = SDLVid_LoadCursor(hand_data);
	ctx->curs_collide = SDLVid_LoadCursor(collide_data);
#if SDL_VERSION_ATLEAST(2,0,0)

#else
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#endif

	ctx->last_mouse_move = SDL_GetTicks();
	ctx->cursor_on = GF_TRUE;

	/*save display resolution - SDL seems to get the screen resolution if asked for video info before
	changing the video mode - to check on other platforms*/
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_GetDesktopDisplayMode(0,&vinf);
	dr->max_screen_width = vinf.w;
	dr->max_screen_height = vinf.h;
	dr->max_screen_bpp = 8;
#else
	vinf = SDL_GetVideoInfo();
#if SDL_VERSION_ATLEAST(1, 2, 10)
	dr->max_screen_width = vinf->current_w;
	dr->max_screen_height = vinf->current_h;
	dr->max_screen_bpp = 8;
#else
	{
		SDL_Rect** modes;
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
		assert( (modes != (SDL_Rect**)0));
		if ( modes == (SDL_Rect**)-1 ) {
			fprintf(stderr, "SDL : DONT KNOW WHICH MODE TO USE, using 640x480\n");
			dr->max_screen_width = 640;
			dr->max_screen_height = 480;
		} else {
			int i;
			dr->max_screen_width = 0;
			for (i=0; modes[i]; ++i) {
				int w = modes[i]->w;
				if (w > dr->max_screen_width) {
					dr->max_screen_width = w;
					dr->max_screen_height = modes[i]->h;
				}
			}
		}
	}
	dr->max_screen_bpp = 8;
#endif /* versions prior to 1.2.10 do not have the size of screen */
#endif

	if (!ctx->os_handle)
#if SDL_VERSION_ATLEAST(2,0,0)
		SDLVid_SetCaption(ctx->screen);
#else
		SDLVid_SetCaption();
#endif
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[SDL] Video output initialized - screen resolution %d %d\n", dr->max_screen_width, dr->max_screen_height));

	return GF_TRUE;
}

static void SDLVid_ResetWindow(SDLVidCtx *ctx)
{
	SDLVid_DestroyObjects(ctx);
#if SDL_VERSION_ATLEAST(2,0,0)
	if ( ctx->gl_context ) {
		SDL_GL_DeleteContext(ctx->gl_context);
		ctx->gl_context = NULL;
	}
	if ( ctx->renderer ) {
		SDL_DestroyRenderer(ctx->renderer);
		ctx->renderer = NULL;
	}

	/*iOS SDL2 has a nasty bug that breaks switching between 2D and GL context if we don't re-init the video subsystem*/
#ifdef GPAC_CONFIG_IOS
	if ( ctx->screen ) {
		SDL_DestroyWindow(ctx->screen);
		ctx->screen=NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	SDL_InitSubSystem(SDL_INIT_VIDEO);
#endif

#endif
}

static void SDLVid_ShutdownWindow(SDLVidCtx *ctx)
{
	SDLVid_DestroyObjects(ctx);
	SDLVid_ResetWindow(ctx);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

#if defined SDL_TEXTINPUTEVENT_TEXT_SIZE /*&& !defined GPAC_CONFIG_IOS*/
#include <gpac/unicode.h>
#endif


Bool SDLVid_ProcessMessageQueue(SDLVidCtx *ctx, GF_VideoOutput *dr)
{
	SDL_Event sdl_evt;
	GF_Event gpac_evt;

#ifdef GPAC_CONFIG_IOS
	while (SDL_WaitEventTimeout(&sdl_evt, 0)) {
#else
	while (SDL_PollEvent(&sdl_evt)) {
#endif

		switch (sdl_evt.type) {
#if SDL_VERSION_ATLEAST(2,0,0)
		case SDL_WINDOWEVENT:
			switch (sdl_evt.window.event) {
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				gpac_evt.type = GF_EVENT_SIZE;
				gpac_evt.size.width = sdl_evt.window.data1;
				gpac_evt.size.height = sdl_evt.window.data2;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			case SDL_WINDOWEVENT_EXPOSED:
			case SDL_WINDOWEVENT_SHOWN:
			case SDL_WINDOWEVENT_MOVED:
				gpac_evt.type = GF_EVENT_REFRESH;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			}
			break;
#else
		case SDL_VIDEORESIZE:
			gpac_evt.type = GF_EVENT_SIZE;
			gpac_evt.size.width = sdl_evt.resize.w;
			gpac_evt.size.height = sdl_evt.resize.h;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			break;
		case SDL_VIDEOEXPOSE:
			gpac_evt.type = GF_EVENT_REFRESH;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			break;
#endif

		case SDL_QUIT:
			memset(&gpac_evt, 0, sizeof(GF_Event));
			gpac_evt.type = GF_EVENT_QUIT;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			return GF_FALSE;

#ifdef SDL_TEXTINPUTEVENT_TEXT_SIZE
		/*keyboard*/
		case SDL_TEXTINPUT: /* Since SDL 1.3, text-input is handled in a specific event */
		{
			u32 len = (u32) strlen( sdl_evt.text.text);
			u32 ucs4_len;
			assert( len < 5 );
			ucs4_len = utf8_to_ucs4 (&(gpac_evt.character.unicode_char), len, (unsigned char*)(sdl_evt.text.text));
			if (ucs4_len) {
				gpac_evt.type = GF_EVENT_TEXTINPUT;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			}
			break;
		}
#endif /* SDL_TEXTINPUTEVENT_TEXT_SIZE */
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			sdl_translate_key(sdl_evt.key.keysym.sym, &gpac_evt.key);
			gpac_evt.type = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			if (gpac_evt.key.key_code==GF_KEY_CONTROL) ctx->ctrl_down = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_TRUE : GF_FALSE;
			else if (gpac_evt.key.key_code==GF_KEY_ALT) ctx->alt_down = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_TRUE : GF_FALSE;
			else if (gpac_evt.key.key_code==GF_KEY_META) ctx->meta_down = (sdl_evt.key.type==SDL_KEYDOWN) ? GF_TRUE : GF_FALSE;

#ifdef SDL_TEXTINPUTEVENT_TEXT_SIZE
			if (sdl_evt.type==SDL_KEYDOWN) {
				if ((gpac_evt.key.key_code==GF_KEY_ENTER) || (gpac_evt.key.key_code==GF_KEY_BACKSPACE)) {
					gpac_evt.type = GF_EVENT_TEXTINPUT;
					gpac_evt.character.unicode_char = (gpac_evt.key.key_code==GF_KEY_ENTER) ? '\r' : '\b';
					dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				}
			}
#endif


#if (SDL_MAJOR_VERSION>=1) && (SDL_MINOR_VERSION>=3)

			if ((gpac_evt.type==GF_EVENT_KEYUP) && (gpac_evt.key.key_code==GF_KEY_V)
#if defined(__DARWIN__) || defined(__APPLE__)
			        && ctx->meta_down
#else
			        && ctx->ctrl_down
#endif
			   ) {
#if defined(__DARWIN__) || defined(__APPLE__)
#else
				gpac_evt.type = GF_EVENT_PASTE_TEXT;
				gpac_evt.message.message = (const char *) SDL_GetClipboardText();
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				SDL_free((char *) gpac_evt.message.message);
#endif
			}
			else if ((gpac_evt.type==GF_EVENT_KEYUP) && (gpac_evt.key.key_code==GF_KEY_C)
#if defined(__DARWIN__) || defined(__APPLE__)
			         && ctx->meta_down
#else
			         && ctx->ctrl_down
#endif
			        ) {
				gpac_evt.type = GF_EVENT_COPY_TEXT;
#if defined(__DARWIN__) || defined(__APPLE__)
#else
				if (dr->on_event(dr->evt_cbk_hdl, &gpac_evt)==GF_TRUE)
					SDL_SetClipboardText((char *)gpac_evt.message.message );
#endif
			}
#endif

#ifndef SDL_TEXTINPUTEVENT_TEXT_SIZE
			if ((sdl_evt.key.type==SDL_KEYDOWN)
			        && sdl_evt.key.keysym.unicode
			        && ((sdl_evt.key.keysym.unicode=='\r') || (sdl_evt.key.keysym.unicode=='\n')  || (sdl_evt.key.keysym.unicode=='\b') || (sdl_evt.key.keysym.unicode=='\t') )
			   ) {
				gpac_evt.character.unicode_char = sdl_evt.key.keysym.unicode;
				gpac_evt.type = GF_EVENT_TEXTINPUT;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			}
#else
#endif
			break;

		/*mouse*/
		case SDL_MOUSEMOTION:
			ctx->last_mouse_move = SDL_GetTicks();
			gpac_evt.type = GF_EVENT_MOUSEMOVE;
			gpac_evt.mouse.x = sdl_evt.motion.x;
			gpac_evt.mouse.y = sdl_evt.motion.y;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			ctx->last_mouse_move = SDL_GetTicks();
			gpac_evt.mouse.x = sdl_evt.motion.x;
			gpac_evt.mouse.y = sdl_evt.motion.y;
			gpac_evt.type = (sdl_evt.type==SDL_MOUSEBUTTONUP) ? GF_EVENT_MOUSEUP : GF_EVENT_MOUSEDOWN;
			switch (sdl_evt.button.button) {
			case SDL_BUTTON_LEFT:
				gpac_evt.mouse.button = GF_MOUSE_LEFT;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			case SDL_BUTTON_MIDDLE:
				gpac_evt.mouse.button = GF_MOUSE_MIDDLE;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
			case SDL_BUTTON_RIGHT:
				gpac_evt.mouse.button = GF_MOUSE_RIGHT;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
#ifdef SDL_BUTTON_WHEELUP
			case SDL_BUTTON_WHEELUP:
			case SDL_BUTTON_WHEELDOWN:
				/*SDL handling is not perfect there, it just says up/down but no info on how much
				the wheel was rotated...*/
				gpac_evt.mouse.wheel_pos = (sdl_evt.button.button==SDL_BUTTON_WHEELUP) ? FIX_ONE : -FIX_ONE;
				gpac_evt.type = GF_EVENT_MOUSEWHEEL;
				dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
				break;
#endif
			}
			break;
#if SDL_VERSION_ATLEAST(2,0,0)
		case SDL_MOUSEWHEEL:
			/*SDL handling is not perfect there, it just says up/down but no info on how much
			the wheel was rotated...*/
			gpac_evt.mouse.wheel_pos = INT2FIX(sdl_evt.wheel.y);
			gpac_evt.type = GF_EVENT_MOUSEWHEEL;
			dr->on_event(dr->evt_cbk_hdl, &gpac_evt);
			break;
#endif

		}
	}
	return GF_TRUE;
}

#ifdef	SDL_WINDOW_THREAD
u32 SDLVid_EventProc(void *par)
{
#if 0
	u32 last_mouse_move;
#endif
	Bool ret;
	GF_VideoOutput *dr = (GF_VideoOutput *)par;
	SDLVID();

	if (!SDLVid_InitializeWindow(ctx, dr)) {
		ctx->sdl_th_state = SDL_STATE_STOP_REQ;
	}

	ctx->sdl_th_state = SDL_STATE_RUNNING;
	while (ctx->sdl_th_state==SDL_STATE_RUNNING) {
		/*after much testing: we must ensure nothing is using the event queue when resizing window.
		-- under X, it throws Xlib "unexpected async reply" under linux, therefore we don't wait events,
		we check for events and execute them if any
		-- under Win32, the SDL_SetVideoMode deadlocks, so we don't force exclusive access to events
		*/
#ifndef WIN32
		gf_mx_p(ctx->evt_mx);
#endif

		ret = SDLVid_ProcessMessageQueue(ctx, dr);

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

		/*QUIT message has been processed*/
		if (!ret) {
			ctx->sdl_th_state = SDL_STATE_STOP_REQ;
			break;
		}

		gf_sleep(2);
	}

	while (ctx->sdl_th_state == SDL_STATE_STOP_REQ)
		gf_sleep(10);

	SDLVid_ShutdownWindow(ctx);
	ctx->sdl_th_state = SDL_STATE_STOPPED;

	return 0;
}
#endif /*SDL_WINDOW_THREAD*/


GF_Err SDLVid_Setup(struct _video_out *dr, void *os_handle, void *os_display, u32 init_flags)
{
	SDLVID();
	/*we don't allow SDL hack, not stable enough*/
	//if (os_handle) SDLVid_SetHack(os_handle, 1);

	ctx->os_handle = os_handle;
	ctx->is_init = GF_FALSE;
	ctx->output_3d_type = 0;
	ctx->force_alpha = (init_flags & GF_TERM_WINDOW_TRANSPARENT) ? GF_TRUE : GF_FALSE;
	ctx->hidden = (init_flags & GF_TERM_INIT_HIDE) ? GF_TRUE : GF_FALSE;

	if (!SDLOUT_InitSDL())
		return GF_IO_ERR;

#ifdef	SDL_WINDOW_THREAD
	ctx->sdl_th_state = SDL_STATE_STOPPED;
	gf_th_run(ctx->sdl_th, SDLVid_EventProc, dr);

	while (!ctx->sdl_th_state)
		gf_sleep(10);

	if (ctx->sdl_th_state==SDL_STATE_STOP_REQ) {

		SDLOUT_CloseSDL();
		ctx->sdl_th_state = SDL_STATE_STOPPED;
		return GF_IO_ERR;
	}
#else
	if (!SDLVid_InitializeWindow(ctx, dr)) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDLOUT_CloseSDL();
		return GF_IO_ERR;
	}
#endif

	//coverage
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_test_mode()) {
		GF_Event evt;
		sdl_translate_key(SDLK_BACKSPACE, &evt.key);
		SDLVid_SetCursor(dr, GF_CURSOR_NORMAL);
	}
#endif

	ctx->is_init = GF_TRUE;
	return GF_OK;
}

static void SDLVid_Shutdown(GF_VideoOutput *dr)
{
	SDLVID();
	/*remove all surfaces*/

	if (!ctx->is_init) return;
#ifdef	SDL_WINDOW_THREAD
	if (ctx->sdl_th_state==SDL_STATE_RUNNING) {
		SDL_Event evt;
		evt.type = SDL_QUIT;
		SDL_PushEvent(&evt);
	}
	/*wait until thread say it is stopped*/
	ctx->sdl_th_state = SDL_STATE_WAIT_FOR_THREAD_TERMINATION;
	while (ctx->sdl_th_state != SDL_STATE_STOPPED)
		gf_sleep(10);

#else
	SDLVid_ShutdownWindow(ctx);
#endif

	SDLOUT_CloseSDL();
	ctx->is_init = GF_FALSE;
}


GF_Err SDLVid_SetFullScreen(GF_VideoOutput *dr, Bool bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
	int bpp;
	u32 pref_bpp;
	SDLVID();
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_DisplayMode goodMode;
	u32 numDisplayModes;
	u32 mask;
#endif

	if (ctx->fullscreen==bFullScreenOn) return GF_OK;

	/*lock to get sure the event queue is not processed under X*/
	gf_mx_p(ctx->evt_mx);
	ctx->fullscreen = bFullScreenOn;

#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_GetCurrentDisplayMode(0, &goodMode);
	SDL_PixelFormatEnumToMasks(goodMode.format, &bpp, &mask, &mask, &mask, &mask);
	pref_bpp = bpp;
#else
	pref_bpp = bpp = ctx->screen->format->BitsPerPixel;
#endif

	if (ctx->fullscreen) {
#if ! ( SDL_VERSION_ATLEAST(2,0,0) )
		u32 flags = (ctx->output_3d_type==1) ? SDL_GL_FULLSCREEN_FLAGS : SDL_FULLSCREEN_FLAGS;
#endif
		Bool switch_res = GF_FALSE;
		switch_res = gf_opts_get_bool("core", "switch-vres");
		if (!dr->max_screen_width || !dr->max_screen_height) switch_res = GF_TRUE;

		ctx->store_width = *screen_width;
		ctx->store_height = *screen_height;
		if (switch_res) {
			u32 i;
			ctx->fs_width = *screen_width;
			ctx->fs_height = *screen_height;

#if SDL_VERSION_ATLEAST(2,0,0)
			numDisplayModes = SDL_GetNumDisplayModes(0);
			for(i=0; i<numDisplayModes; i++) {
				SDL_GetDisplayMode(0, i, &goodMode);
				if ((ctx->fs_width <= (u32) goodMode.w) && (ctx->fs_height <= (u32) goodMode.h)) {
					s32 bppDisp;
					ctx->fs_width = goodMode.w;
					ctx->fs_height = goodMode.h;
					SDL_PixelFormatEnumToMasks(goodMode.format, &bppDisp, &mask, &mask, &mask, &mask);
					pref_bpp = bppDisp;
					break;
				}
			}
#else
			for(i=0; i<nb_video_modes; i++) {
				if (ctx->fs_width<=video_modes[2*i] && ctx->fs_height<=video_modes[2*i + 1]) {
					if ((pref_bpp = SDL_VideoModeOK(video_modes[2*i], video_modes[2*i+1], bpp, flags))) {
						ctx->fs_width = video_modes[2*i];
						ctx->fs_height = video_modes[2*i + 1];
						break;
					}
				}
			}
#endif
		} else {
#if SDL_VERSION_ATLEAST(2,0,0)
			SDL_GetCurrentDisplayMode(0, &goodMode);
#endif
			ctx->fs_width = dr->max_screen_width;
			ctx->fs_height = dr->max_screen_height;
		}
#if SDL_VERSION_ATLEAST(2,0,0)
		SDL_SetWindowDisplayMode(ctx->screen, &goodMode);
		SDL_SetWindowFullscreen(ctx->screen, SDL_WINDOW_FULLSCREEN_DESKTOP);
#else
		ctx->screen = SDL_SetVideoMode(ctx->fs_width, ctx->fs_height, pref_bpp, flags);
#endif
		/*we switched bpp, clean all objects*/
		if (bpp != pref_bpp) SDLVid_DestroyObjects(ctx);
		*screen_width = ctx->fs_width;
		*screen_height = ctx->fs_height;
		/*GL has changed*/
		if (ctx->output_3d_type==1) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_VIDEO_SETUP;
			evt.setup.opengl_mode = 3;
			dr->on_event(dr->evt_cbk_hdl, &evt);
		}
	} else {
#if SDL_VERSION_ATLEAST(2,0,0)
		SDL_SetWindowFullscreen(ctx->screen, 0);
#endif
		SDLVid_ResizeWindow(dr, ctx->store_width, ctx->store_height);
		*screen_width = ctx->store_width;
		*screen_height = ctx->store_height;
	}
	gf_mx_v(ctx->evt_mx);
	if (!ctx->screen) return GF_IO_ERR;
	return GF_OK;
}

GF_Err SDLVid_SetBackbufferSize(GF_VideoOutput *dr, u32 newWidth, u32 newHeight, Bool system_mem)
{
	const char *opt;
	SDLVID();
#if SDL_VERSION_ATLEAST(2,0,0)

#else
	u32 col;
#endif

	if (ctx->output_3d_type==1) return GF_BAD_PARAM;

	opt = gf_opts_get_key("core", "hwvmem");
	if (system_mem) {
		if (opt && !strcmp(opt, "always")) system_mem = GF_FALSE;
	} else {
		if (opt && !strcmp(opt, "never")) system_mem = GF_TRUE;
	}
	ctx->use_systems_memory = system_mem;


	/*clear screen*/
#if SDL_VERSION_ATLEAST(2,0,0)


	if (ctx->tx_back_buffer) SDL_DestroyTexture(ctx->tx_back_buffer);
	if (ctx->back_buffer_pixels) gf_free(ctx->back_buffer_pixels);

	ctx->tx_back_buffer = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, newWidth, newHeight);
	ctx->back_buffer_pixels = gf_malloc(sizeof(char)*3*newWidth*newHeight);


	SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
	SDL_RenderClear(ctx->renderer);
	SDL_RenderPresent(ctx->renderer);

#else
	if(ctx->screen) {
	col = SDL_MapRGB(ctx->screen->format, 0, 0, 0);
	SDL_FillRect(ctx->screen, NULL, col);
	SDL_Flip(ctx->screen);
	}
	if (ctx->back_buffer && ((u32) ctx->back_buffer->w==newWidth) && ((u32) ctx->back_buffer->h==newHeight)) {
		return GF_OK;
	}
	if (ctx->back_buffer) SDL_FreeSurface(ctx->back_buffer);

	if (ctx->screen) ctx->back_buffer = SDL_CreateRGBSurface(ctx->use_systems_memory ? SDL_SWSURFACE : SDL_HWSURFACE, newWidth, newHeight, ctx->screen->format->BitsPerPixel, ctx->screen->format->Rmask, ctx->screen->format->Gmask, ctx->screen->format->Bmask, 0);

	if (!ctx->back_buffer) return GF_IO_ERR;
#endif
	ctx->width = newWidth;
	ctx->height = newHeight;

	return GF_OK;
}

u32 SDLVid_MapPixelFormat(SDL_PixelFormat *format, Bool force_alpha)
{
	if (format->palette) return 0;
	switch (format->BitsPerPixel) {
	case 16:
		if ((format->Rmask==0x7c00) && (format->Gmask==0x03e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_555;
		if ((format->Rmask==0xf800) && (format->Gmask==0x07e0) && (format->Bmask==0x001f) ) return GF_PIXEL_RGB_565;
		return 0;
	case 24:
		if (format->Rmask==0x00FF0000) return GF_PIXEL_RGB;
		if (format->Rmask==0x000000FF) return GF_PIXEL_BGR;
		return 0;
	case 32:
		if (format->Amask==0xFF000000) return GF_PIXEL_ARGB;
		if (format->Rmask==0x00FF0000) return force_alpha ? GF_PIXEL_ARGB : GF_PIXEL_RGBX;
		if (format->Rmask==0x000000FF) return force_alpha ? GF_PIXEL_RGBA : GF_PIXEL_BGRX;
		return 0;
	default:
		return 0;
	}
}

#if SDL_VERSION_ATLEAST(2,0,0)
static GF_Err SDLVid_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *video_info, Bool do_lock)
{
	SDLVID();

	if (do_lock) {
		memset(video_info, 0, sizeof(GF_VideoSurface));
		video_info->width = ctx->width;
		video_info->height = ctx->height;
		video_info->pitch_x = 0;
		video_info->pitch_y = ctx->width*3;
		video_info->video_buffer = ctx->back_buffer_pixels;
		video_info->pixel_format = GF_PIXEL_RGB;
		video_info->is_hardware_memory = 0;
		if (ctx->needs_bb_grab) {
			SDL_RenderReadPixels(ctx->renderer, NULL, SDL_PIXELFORMAT_RGB24, video_info->video_buffer, video_info->pitch_y);
			ctx->needs_bb_grab = 0;
		}
	} else {
		SDL_UpdateTexture(ctx->tx_back_buffer, NULL, video_info->video_buffer, video_info->pitch_y);
		SDL_RenderCopy(ctx->renderer, ctx->tx_back_buffer, NULL, NULL);
	}
	return GF_OK;
}

#else
static GF_Err SDLVid_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *video_info, Bool do_lock)
{
	SDLVID();

	if (!ctx->back_buffer) return GF_BAD_PARAM;
	if (do_lock) {
		if (!video_info) return GF_BAD_PARAM;
		if (SDL_LockSurface(ctx->back_buffer)<0) return GF_IO_ERR;
		memset(video_info, 0, sizeof(GF_VideoSurface));
		video_info->width = ctx->back_buffer->w;
		video_info->height = ctx->back_buffer->h;
		video_info->pitch_x = 0;
		video_info->pitch_y = ctx->back_buffer->pitch;
		video_info->video_buffer = (char*)ctx->back_buffer->pixels;
		video_info->pixel_format = SDLVid_MapPixelFormat(ctx->back_buffer->format, ctx->force_alpha);
		video_info->is_hardware_memory = !ctx->use_systems_memory;
	} else {
		SDL_UnlockSurface(ctx->back_buffer);
	}
	return GF_OK;
}
#endif


#if SDL_VERSION_ATLEAST(2,0,0)

//for CGLSetParameter
#if defined(__APPLE__) && !defined(GPAC_CONFIG_IOS)
#include <OpenGL/OpenGL.h>
#endif

static GF_Err SDLVid_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	SDLVID();
	/*if resizing don't process otherwise we may deadlock*/
	if (!ctx->screen) return GF_OK;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[SDL] swapping video buffers\n"));

	if (ctx->output_3d_type==1) {
		//with SDL2 we have to disable vsync by overriding swap interval
#if defined(__APPLE__) && !defined(GPAC_CONFIG_IOS)
		if (ctx->disable_vsync) {
			GLint sync = 0;
			CGLContextObj gl_ctx = CGLGetCurrentContext();
			CGLSetParameter(gl_ctx, kCGLCPSwapInterval, &sync);
		}
#endif
		SDL_GL_SwapWindow(ctx->screen);
		return GF_OK;
	}

	if (ctx->enable_defer_mode) {
		if (ctx->needs_bb_flush) {
			SDL_UpdateTexture(ctx->tx_back_buffer, NULL, ctx->back_buffer_pixels, 3*ctx->width);
			SDL_RenderCopy(ctx->renderer, ctx->tx_back_buffer, NULL, NULL);
		}
		SDL_RenderReadPixels(ctx->renderer, NULL, SDL_PIXELFORMAT_RGB24, ctx->back_buffer_pixels, 3*ctx->width);
		ctx->needs_bb_grab = 0;
		ctx->needs_bb_flush = 0;
		SDL_RenderPresent(ctx->renderer);
		//push back texture after SDL flip
		SDL_RenderCopy(ctx->renderer, ctx->tx_back_buffer, NULL, NULL);
	} else {
		ctx->needs_clear = 1;
		SDL_RenderPresent(ctx->renderer);
	}


	return GF_OK;
}

#else

static GF_Err SDLVid_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	SDL_Rect rc;
	SDLVID();
	/*if resizing don't process otherwise we may deadlock*/
	if (!ctx->screen) return GF_OK;

	//GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Flush\n"));

	if (ctx->output_3d_type==1) {
		SDL_GL_SwapBuffers();
		return GF_OK;
	}
	if (!ctx->back_buffer) return GF_BAD_PARAM;

	if ((dest->w != (u32) ctx->back_buffer->w) || (dest->h != (u32) ctx->back_buffer->h)) {
		GF_VideoSurface src, dst;

		SDL_LockSurface(ctx->back_buffer);
		memset(&src, 0, sizeof(GF_VideoSurface));
		src.height = ctx->back_buffer->h;
		src.width = ctx->back_buffer->w;
		src.pitch_x = 0;
		src.pitch_y = ctx->back_buffer->pitch;
		src.pixel_format = SDLVid_MapPixelFormat(ctx->back_buffer->format, ctx->force_alpha);
		src.video_buffer = (char*)ctx->back_buffer->pixels;

		SDL_LockSurface(ctx->screen);
		dst.height = ctx->screen->h;
		dst.width = ctx->screen->w;
		dst.pitch_x = 0;
		dst.pitch_y = ctx->screen->pitch;
		dst.pixel_format = SDLVid_MapPixelFormat(ctx->screen->format, GF_FALSE);
		dst.video_buffer = (char*)ctx->screen->pixels;

		gf_stretch_bits(&dst, &src, dest, NULL, 0xFF, GF_FALSE, NULL, NULL);
		SDL_UnlockSurface(ctx->screen);
		SDL_UnlockSurface(ctx->back_buffer);

	} else {
		rc.x = dest->x;
		rc.y = dest->y;
		rc.w = dest->w;
		rc.h = dest->h;
		SDL_BlitSurface(ctx->back_buffer, NULL, ctx->screen, &rc);
	}
	SDL_Flip(ctx->screen);
	return GF_OK;
}
#endif


#ifdef WIN32
static u32 get_sys_col(int idx)
{
	u32 res;
	DWORD val = GetSysColor(idx);
	res = (val)&0xFF;
	res<<=8;
	res |= (val>>8)&0xFF;
	res<<=8;
	res |= (val>>16)&0xFF;
	return res;
}
#endif

static GF_Err SDLVid_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	if (!evt) {
#ifndef	SDL_WINDOW_THREAD
		SDLVID();
		SDLVid_ProcessMessageQueue(ctx, dr);
#endif
		return GF_OK;
	}
	switch (evt->type) {
	case GF_EVENT_SET_CURSOR:
#ifndef GPAC_CONFIG_IOS
		SDLVid_SetCursor(dr, evt->cursor.cursor_type);
#endif
		break;
	case GF_EVENT_SET_CAPTION:
#ifdef SDL_WINDOW_THREAD
	{
		SDLVID();
		if (ctx->sdl_th_state != SDL_STATE_RUNNING)
			break;
	}
#endif
#if SDL_VERSION_ATLEAST(2,0,0)
	{
#if !defined(GPAC_CONFIG_IOS)
		SDLVID();
		SDL_SetWindowTitle(ctx->screen, evt->caption.caption);
		SDLVid_ProcessEvent(dr, NULL);
#endif
	}
#else
	SDL_WM_SetCaption(evt->caption.caption, NULL);
#endif
	break;
	case GF_EVENT_SHOWHIDE:
		/*the only way to have proper show/hide with SDL is to shutdown the video system and reset it up
		which we don't want to do since the setup MUST occur in the rendering thread for some configs (openGL)*/
		return GF_NOT_SUPPORTED;
	case GF_EVENT_SIZE:
	{
		SDLVID();
#ifdef GPAC_CONFIG_IOS
		if (ctx->fullscreen) {
		} else {
			SDLVid_ResizeWindow(dr, evt->size.width, evt->size.height);
		}
#else
		if (ctx->fullscreen) {
			//ctx->store_width = evt->size.width;
			//ctx->store_height = evt->size.height;
		} else {
			SDLVid_ResizeWindow(dr, evt->size.width, evt->size.height);
		}
#endif
	}
	break;
	case GF_EVENT_MOVE:

#if !defined(GPAC_CONFIG_IOS) && SDL_VERSION_ATLEAST(2,0,0)
	{
		SDLVID();

		if (ctx->fullscreen) return GF_OK;

		if (evt->move.relative == 2) {
		}
		else if (evt->move.relative) {
			s32 x, y;
			x = y = 0;
			SDL_GetWindowPosition(ctx->screen, &x, &y);
			SDL_SetWindowPosition(ctx->screen, x + evt->move.x, y + evt->move.y);
		} else {
			SDL_SetWindowPosition(ctx->screen, evt->move.x, evt->move.y);
		}
	}
#endif
	break;
	case GF_EVENT_VIDEO_SETUP:
	{
		SDLVID();
		ctx->disable_vsync=evt->setup.disable_vsync;
		switch (evt->setup.opengl_mode) {
		case 0:
			/*force a resetup of the window*/
			if (ctx->output_3d_type) {
				ctx->width = ctx->height = 0;
				ctx->output_3d_type = 0;
				SDLVid_ResetWindow(ctx);
				SDLVid_ResizeWindow(dr, evt->setup.width, evt->setup.height);
			} else {
#if SDL_VERSION_ATLEAST(2,0,0)
				SDLVid_ResizeWindow(dr, evt->setup.width, evt->setup.height);
#endif
			}
			ctx->output_3d_type = 0;
			return SDLVid_SetBackbufferSize(dr, evt->setup.width, evt->setup.height, evt->setup.system_memory);
		case 1:
			/*force a resetup of the window*/
			if (!ctx->output_3d_type) {
				ctx->width = ctx->height = 0;
				SDLVid_ResetWindow(ctx);
			}
			ctx->output_3d_type = 1;
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[SDL] Setting up 3D in SDL.\n"));
#ifdef GPAC_CONFIG_IOS
//            return SDLVid_ResizeWindow(dr, dr->max_screen_width, dr->max_screen_height);
			return SDLVid_ResizeWindow(dr, evt->setup.width, evt->setup.height);
#else
			return SDLVid_ResizeWindow(dr, evt->setup.width, evt->setup.height);
#endif
		case 2:
			/*find a way to do that in SDL*/
			ctx->output_3d_type = 2;
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] 3D not supported with SDL.\n"));
			return GF_NOT_SUPPORTED;
		}
		default:
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Trying to set an Unknown Mode %d !\n", evt->setup.opengl_mode));
			return GF_NOT_SUPPORTED;
		}
		break;
	case GF_EVENT_SYS_COLORS:
#ifdef WIN32
		evt->sys_cols.sys_colors[0] = get_sys_col(COLOR_ACTIVEBORDER);
		evt->sys_cols.sys_colors[1] = get_sys_col(COLOR_ACTIVECAPTION);
		evt->sys_cols.sys_colors[2] = get_sys_col(COLOR_APPWORKSPACE);
		evt->sys_cols.sys_colors[3] = get_sys_col(COLOR_BACKGROUND);
		evt->sys_cols.sys_colors[4] = get_sys_col(COLOR_BTNFACE);
		evt->sys_cols.sys_colors[5] = get_sys_col(COLOR_BTNHIGHLIGHT);
		evt->sys_cols.sys_colors[6] = get_sys_col(COLOR_BTNSHADOW);
		evt->sys_cols.sys_colors[7] = get_sys_col(COLOR_BTNTEXT);
		evt->sys_cols.sys_colors[8] = get_sys_col(COLOR_CAPTIONTEXT);
		evt->sys_cols.sys_colors[9] = get_sys_col(COLOR_GRAYTEXT);
		evt->sys_cols.sys_colors[10] = get_sys_col(COLOR_HIGHLIGHT);
		evt->sys_cols.sys_colors[11] = get_sys_col(COLOR_HIGHLIGHTTEXT);
		evt->sys_cols.sys_colors[12] = get_sys_col(COLOR_INACTIVEBORDER);
		evt->sys_cols.sys_colors[13] = get_sys_col(COLOR_INACTIVECAPTION);
		evt->sys_cols.sys_colors[14] = get_sys_col(COLOR_INACTIVECAPTIONTEXT);
		evt->sys_cols.sys_colors[15] = get_sys_col(COLOR_INFOBK);
		evt->sys_cols.sys_colors[16] = get_sys_col(COLOR_INFOTEXT);
		evt->sys_cols.sys_colors[17] = get_sys_col(COLOR_MENU);
		evt->sys_cols.sys_colors[18] = get_sys_col(COLOR_MENUTEXT);
		evt->sys_cols.sys_colors[19] = get_sys_col(COLOR_SCROLLBAR);
		evt->sys_cols.sys_colors[20] = get_sys_col(COLOR_3DDKSHADOW);
		evt->sys_cols.sys_colors[21] = get_sys_col(COLOR_3DFACE);
		evt->sys_cols.sys_colors[22] = get_sys_col(COLOR_3DHIGHLIGHT);
		evt->sys_cols.sys_colors[23] = get_sys_col(COLOR_3DLIGHT);
		evt->sys_cols.sys_colors[24] = get_sys_col(COLOR_3DSHADOW);
		evt->sys_cols.sys_colors[25] = get_sys_col(COLOR_WINDOW);
		evt->sys_cols.sys_colors[26] = get_sys_col(COLOR_WINDOWFRAME);
		evt->sys_cols.sys_colors[27] = get_sys_col(COLOR_WINDOWTEXT);
		return GF_OK;
#else
		return GF_NOT_SUPPORTED;
#endif

	case GF_EVENT_TEXT_EDITING_START:
	case GF_EVENT_TEXT_EDITING_END:
#if defined(GPAC_CONFIG_IOS) && SDL_VERSION_ATLEAST(2,0,0)
		if (evt->type==GF_EVENT_TEXT_EDITING_START) {
			SDL_StartTextInput();
		} else {
			SDL_StopTextInput();
		}
		return GF_OK;
#else
		return GF_NOT_SUPPORTED;
#endif

	case GF_EVENT_SET_GL:
	{
#if SDL_VERSION_ATLEAST(2,0,0)
		SDLVID();
		if (SDL_GL_MakeCurrent(ctx->screen, ctx->gl_context)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Cannot make context current: %s\n", SDL_GetError()));
			return GF_IO_ERR;
		}
#endif
	}
		return GF_OK;
	}
	return GF_OK;
}

#if SDL_VERSION_ATLEAST(2,0,0)
static GF_Err SDL_Blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	SDLVID();
	Bool need_copy=0;
	u32 format;
	s32 acc, w, h;
	int res;
	Bool set_blend=0;
	SDL_Rect dstrc;
	SDL_Texture **pool;
	SDL_Rect srcrc, *src_ptr=NULL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[SDL] Bliting surface (overlay type %d)\n", overlay_type));

	if (ctx->needs_bb_flush) {
		SDL_UpdateTexture(ctx->tx_back_buffer, NULL, ctx->back_buffer_pixels, 3*ctx->width);
		SDL_RenderCopy(ctx->renderer, ctx->tx_back_buffer, NULL, NULL);
		ctx->needs_bb_grab = 1;
	}

	ctx->needs_bb_grab = 1;
	if (ctx->needs_clear) {
		SDL_RenderClear(ctx->renderer);
		ctx->needs_clear = 0;
	}

	dstrc.w = dst_wnd->w;
	dstrc.h = dst_wnd->h;
	dstrc.x = dst_wnd->x;
	dstrc.y = dst_wnd->y;

	if (src_wnd) {
		srcrc.x = src_wnd->x;
		srcrc.y = src_wnd->y;
		srcrc.w = src_wnd->w;
		srcrc.h = src_wnd->h;
		src_ptr = &srcrc;
	}

	//this is a clear (not very elegant ...)
	if ((video_src->width<=2) && (video_src->height<=2)) {
		u8 *pix =(u8 *) video_src->video_buffer;
		if (video_src->pixel_format == GF_PIXEL_RGB) {
			SDL_SetRenderDrawColor(ctx->renderer, pix[0], pix[1], pix[2], 0xFF);
		} else {
			SDL_SetRenderDrawColor(ctx->renderer, pix[0], pix[1], pix[2], pix[3]);
		}
		res = SDL_RenderFillRect(ctx->renderer, &dstrc);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL2] Clear error: %s\n", SDL_GetError()));
			return GF_IO_ERR;
		}
		return GF_OK;
	}

	switch (video_src->pixel_format) {
	case GF_PIXEL_RGB:
		pool = &ctx->pool_rgb;
		format=SDL_PIXELFORMAT_RGB24;
		break;
	case GF_PIXEL_XRGB:
		pool = &ctx->pool_rgb;
		format=SDL_PIXELFORMAT_BGRX8888;
		break;
	case GF_PIXEL_RGBX:
		pool = &ctx->pool_rgb;
		format=SDL_PIXELFORMAT_BGR888;
		break;
	case GF_PIXEL_XBGR:
		pool = &ctx->pool_rgb;
		format=SDL_PIXELFORMAT_RGBX8888;
		break;
	case GF_PIXEL_BGRX:
		pool = &ctx->pool_rgb;
		format=SDL_PIXELFORMAT_RGB888;
		break;
	case GF_PIXEL_RGBA:
		pool = &ctx->pool_rgba;
		format=SDL_PIXELFORMAT_ABGR8888;
		set_blend=1;
		break;
	case GF_PIXEL_YUV:
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_YV12;
		format=SDL_PIXELFORMAT_IYUV;
		break;
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV_10:
		need_copy=1;
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_YV12;
		break;
	/*FIXME we need to upgrade our SDL build*/
#if !defined(GPAC_CONFIG_IOS) && SDL_VERSION_ATLEAST(2,0,0)
	case GF_PIXEL_NV12:
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_NV12;
		break;
	case GF_PIXEL_NV21:
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_NV21;
		break;
#endif
	case GF_PIXEL_UYVY:
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_UYVY;
		break;
	case GF_PIXEL_YUYV:
		pool = &ctx->pool_yuv;
		format=SDL_PIXELFORMAT_YUY2;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}


	if (*pool ) {
		SDL_QueryTexture(*pool, &format, &acc, &w, &h);
		if ((w != video_src->width) || (h != video_src->height) ) {
			SDL_DestroyTexture(*pool);
			*pool = NULL;
		}
	}
	if (!(*pool)) {
		(*pool) = SDL_CreateTexture(ctx->renderer, format, SDL_TEXTUREACCESS_STREAMING, video_src->width, video_src->height);
		if (!(*pool)) return GF_NOT_SUPPORTED;
	}

	SDL_QueryTexture((*pool), &format, &acc, &w, &h);

	if (need_copy) {
		GF_VideoSurface dst_v;
		u8 *pixels;
		int pitch;
		GF_Window swnd;
		/*copy pixels*/
		if (SDL_LockTexture(*pool, NULL, (void**)&pixels, &pitch) < 0) {
			return GF_NOT_SUPPORTED;
		}
		if (!src_wnd) {
			swnd.x = swnd.y=0;
			swnd.w = video_src->width;
			swnd.h = video_src->height;
			src_wnd = &swnd;
		}

		memset(&dst_v, 0, sizeof(GF_VideoSurface));
		dst_v.video_buffer = pixels;
		dst_v.u_ptr = pixels + h*pitch;
		dst_v.v_ptr = pixels + 5*h*pitch/4;
		dst_v.pitch_y = pitch;
		dst_v.pixel_format = GF_PIXEL_YUV;
		dst_v.width = video_src->width;
		dst_v.height = video_src->height;

		gf_stretch_bits(&dst_v, video_src, NULL, src_wnd, 0xFF, GF_FALSE, NULL, NULL);

		SDL_UnlockTexture(*pool);
	} else {
		SDL_UpdateTexture(*pool, NULL, video_src->video_buffer, video_src->pitch_y);
	}

	if (set_blend || (video_src->global_alpha!=0xFF)) {
		res = SDL_SetTextureBlendMode(*pool, SDL_BLENDMODE_BLEND);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL2] Cannot change texture blend mode: %s\n", SDL_GetError()));
			return GF_IO_ERR;
		}
		res = SDL_SetTextureAlphaMod(*pool, video_src->global_alpha);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL2] Cannot change global alpha of texture: %s\n", SDL_GetError()));
			return GF_IO_ERR;
		}
	} else {
		res = SDL_SetTextureBlendMode(*pool, SDL_BLENDMODE_NONE);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL2] Cannot change texture blend mode: %s\n", SDL_GetError()));
			return GF_IO_ERR;
		}
	}

	res = SDL_RenderCopy(ctx->renderer, *pool, src_ptr, &dstrc);
	if (res<0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL2] Blit error: %s\n", SDL_GetError()));
		return GF_IO_ERR;
	}
	return GF_OK;
}

#else
static GF_Err SDL_Blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	SDLVID();
	u32 amask = 0;
	u32 bpp;
	GF_Err e = GF_OK;
	u32 i;
	u8 *dst, *src;
	SDL_Rect dstrc;
	SDL_Surface **pool;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[SDL] Bliting surface (overlay type %d)\n", overlay_type));

	if (overlay_type) {
		GF_VideoSurface dst_v;
		if (!video_src) {
			if (ctx->yuv_overlay) {
				SDL_FreeYUVOverlay(ctx->yuv_overlay);
				ctx->yuv_overlay=NULL;
			}
			return GF_OK;
		}
		if (!ctx->yuv_overlay || (ctx->yuv_overlay->w != src_wnd->w) || (ctx->yuv_overlay->h != src_wnd->h) ) {
			if (ctx->yuv_overlay) SDL_FreeYUVOverlay(ctx->yuv_overlay);

			ctx->yuv_overlay = SDL_CreateYUVOverlay(src_wnd->w, src_wnd->h, SDL_YV12_OVERLAY, ctx->screen);
			if (!ctx->yuv_overlay) return GF_NOT_SUPPORTED;
		}
		/*copy pixels*/
		SDL_LockYUVOverlay(ctx->yuv_overlay);


		memset(&dst_v, 0, sizeof(GF_VideoSurface));
		dst_v.video_buffer = (char *) ctx->yuv_overlay->pixels[0];
		dst_v.u_ptr = (char *) ctx->yuv_overlay->pixels[1];
		dst_v.v_ptr = (char *) ctx->yuv_overlay->pixels[2];
		dst_v.pitch_y = ctx->yuv_overlay->pitches[0];
		dst_v.pixel_format = GF_PIXEL_YUV;
		dst_v.width = video_src->width;
		dst_v.height = video_src->height;

		gf_stretch_bits(&dst_v, video_src, NULL, src_wnd, 0xFF, GF_FALSE, NULL, NULL);

		SDL_UnlockYUVOverlay(ctx->yuv_overlay);

		dstrc.w = dst_wnd->w;
		dstrc.h = dst_wnd->h;
		dstrc.x = dst_wnd->x;
		dstrc.y = dst_wnd->y;
		SDL_DisplayYUVOverlay(ctx->yuv_overlay, &dstrc);
		return GF_OK;
	}

	/*SDL doesn't support stretching ...*/
	if ((src_wnd->w != dst_wnd->w) || (src_wnd->h!=dst_wnd->h))
		return GF_NOT_SUPPORTED;

	switch (video_src->pixel_format) {
	case GF_PIXEL_RGB:
		pool = &ctx->pool_rgb;
		bpp = 3;
		break;
	case GF_PIXEL_RGBA:
		pool = &ctx->pool_rgba;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		amask = 0x000000FF;
#else
		amask = 0xFF000000;
#endif
		bpp = 4;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	if (! *pool || ((*pool)->w < (int) src_wnd->w) || ((*pool)->h < (int) src_wnd->h) ) {
		if ((*pool)) SDL_FreeSurface((*pool));

		(*pool) = SDL_CreateRGBSurface(ctx->use_systems_memory ? SDL_SWSURFACE : SDL_HWSURFACE,
		                               src_wnd->w, src_wnd->h, 8*bpp,
		                               0x000000FF, 0x0000FF00, 0x00FF0000, amask);

		if (! (*pool) ) return GF_IO_ERR;
	}

	SDL_LockSurface(*pool);

	dst = (u8 *) ( (*pool)->pixels);
	src = (u8*)video_src->video_buffer + video_src->pitch_y*src_wnd->y + src_wnd->x*bpp;
	for (i=0; i<src_wnd->h; i++) {
		memcpy(dst, src, bpp * src_wnd->w);
		src += video_src->pitch_y;
		dst += (*pool)->pitch;
	}
	SDL_UnlockSurface(*pool);

	dstrc.w = dst_wnd->w;
	dstrc.h = dst_wnd->h;
	dstrc.x = dst_wnd->x;
	dstrc.y = dst_wnd->y;

	if (SDL_BlitSurface(*pool, NULL, ctx->back_buffer, &dstrc))
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[SDL] Blit error: %s\n", SDL_GetError()));

	return e;
}

#endif

void *SDL_NewVideo()
{
#if SDL_VERSION_ATLEAST(2,0,0)
	const char *opt;
#endif
	SDLVidCtx *ctx;
	GF_VideoOutput *driv;

	driv = (GF_VideoOutput*)gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "SDL Video Output", "gpac distribution");

	ctx = (SDLVidCtx*)gf_malloc(sizeof(SDLVidCtx));
	memset(ctx, 0, sizeof(SDLVidCtx));
#ifdef	SDL_WINDOW_THREAD
	ctx->sdl_th = gf_th_new("SDLVideo");
#endif
	ctx->evt_mx = gf_mx_new("SDLEvents");


	driv->opaque = ctx;
	driv->Setup = SDLVid_Setup;
	driv->Shutdown = SDLVid_Shutdown;
	driv->SetFullScreen = SDLVid_SetFullScreen;
	driv->Flush = SDLVid_Flush;
	driv->ProcessEvent = SDLVid_ProcessEvent;
	/*no offscreen opengl with SDL*/
	driv->hw_caps |= GF_VIDEO_HW_OPENGL;

	/*no YUV hardware blitting in SDL (only overlays)*/
	driv->hw_caps |= GF_VIDEO_HW_HAS_RGB ;

#if SDL_VERSION_ATLEAST(2,0,0)

	driv->hw_caps |= GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_STRETCH | GF_VIDEO_HW_HAS_RGBA;


	opt = gf_opts_get_key("core", "sdl-defer");
	ctx->enable_defer_mode = 0;
	if (opt && !strcmp(opt, "yes"))
		ctx->enable_defer_mode = 1;

	if (! ctx->enable_defer_mode)
		driv->hw_caps |= GF_VIDEO_HW_DIRECT_ONLY;
#else
	driv->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY;
#endif



	driv->Blit = SDL_Blit;
	driv->LockBackBuffer = SDLVid_LockBackBuffer;
	driv->LockOSContext = NULL;
#ifndef SDL_TEXTINPUTEVENT_TEXT_SIZE
	SDL_EnableUNICODE(1);
#else
	SDL_StartTextInput();
#endif /* SDL_TEXTINPUTEVENT_TEXT_SIZE */

	return driv;
}

void SDL_DeleteVideo(void *ifce)
{
	GF_VideoOutput *dr = (GF_VideoOutput *)ifce;
	SDLVID();
#ifdef	SDL_WINDOW_THREAD
	gf_th_del(ctx->sdl_th);
#endif
	gf_mx_del(ctx->evt_mx);
	gf_free(ctx);
	gf_free(dr);
}

