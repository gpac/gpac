/*
 *  SDL_Hack.c
 *  mp4client
 *
 *  Created by bouqueau on 19/04/10.
 *  Copyright 2010 Telecom ParisTech. All rights reserved.
 *
 */

#include <gpac/tools.h>
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

