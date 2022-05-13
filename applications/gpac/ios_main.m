/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac ios application
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

#ifdef main
#undef main
#endif

#import <UIKit/UIKit.h>
//requires SDL 2.0.10 - if not available, remove ios_main.m from project
typedef int (*SDL_main_func)(int argc, char *argv[]);
extern int SDL_UIKitRunApp(int argc, char *argv[], SDL_main_func mainFunction);

int SDL_main(int argc, char **argv);

int main(int argc, char **argv)
{
	int i, needs_uikit=0;

	if (argc<=1) {
		needs_uikit=1;
	} else {
		for (i=1; i<argc; i++) {
			if (strstr(argv[i], "-mp4c")
				|| strstr(argv[i], "-gui")
				|| strstr(argv[i], "-req-gl")
				|| strstr(argv[i], "vout")
				|| strstr(argv[i], "player=base")
				|| strstr(argv[i], "player=gui")
			) {
				needs_uikit = 1;
				break;
			}
		}
	}

	if (needs_uikit) {
		return SDL_UIKitRunApp(argc, argv, SDL_main);
	}

	return SDL_main(argc, argv);
}

