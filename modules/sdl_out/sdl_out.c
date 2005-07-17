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

static Bool is_init = 0;
static u32 num_users = 0;

Bool SDLOUT_InitSDL()
{
	if (is_init) {
		num_users++;
		return 1;
	}
	if (SDL_Init(0) < 0) return 0;
	is_init = 1;
	num_users++;
	return 1;
}

void SDLOUT_CloseSDL()
{
	if (!is_init) return;
	assert(num_users);
	num_users--;
	if (!num_users) SDL_Quit();
	return;
}


/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return SDL_NewVideo();
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return SDL_NewAudio();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		SDL_DeleteVideo(ifce);
		break;
	case GF_AUDIO_OUTPUT_INTERFACE:
		SDL_DeleteAudio(ifce);
		break;
	}
}
