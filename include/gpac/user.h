/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / Stream Management sub-project
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



#ifndef _GF_USER_H_
#define _GF_USER_H_

#ifdef __cplusplus
extern "C" {
#endif


/*!
 *	\file <gpac/user.h>
 *	\brief GPAC terminal <-> user API.
 */

/*!
 *\addtogroup termuser_grp Terminal User
 *\ingroup playback_grp
 *\brief GPAC terminal <-> user API.
 *
 *This section documents the user-level API of the GPAC media player.
 *	@{
 */




#include <gpac/events.h>
#include <gpac/module.h>

/*GPAC client terminal*/
typedef struct _tag_terminal GF_Terminal;
typedef struct _tag_user GF_User;


enum
{
	/*disables frame-rate regulation (used when dumping content)*/
	GF_TERM_NO_REGULATION = 1,
	/*display should be hidden upon initialization*/
	GF_TERM_INIT_HIDE = 1<<1,
	/*no audio renderer will be created*/
	GF_TERM_NO_AUDIO = 1<<2,
	/*initializes client without a default audio out - used for dump modes where audio playback is not needed*/
	GF_TERM_NO_DEF_AUDIO_OUT = 1<<3,
	/*disables video output module - used for bench mode without video*/
	GF_TERM_NO_VIDEO = 1<<4,
	/*works without window thread*/
	GF_TERM_WINDOW_NO_THREAD = 1<<5,
	/*lets the main user handle window events (needed for browser plugins)*/
	GF_TERM_NO_WINDOWPROC_OVERRIDE = 1<<6,
	/*works without title bar*/
	GF_TERM_WINDOW_NO_DECORATION = 1<<7,

	/*framebuffer works in 32 bit alpha mode - experimental, only supported on Win32*/
	GF_TERM_WINDOW_TRANSPARENT = 1<<8,
	/*works in windowless mode - experimental, only supported on Win32*/
	GF_TERM_WINDOWLESS = 1<<9,
};

/*user object for all callbacks*/
struct _tag_user
{
	/*user defined callback for all functions - cannot be NULL*/
	void *opaque;
	/*the event proc. Return value depend on the event type, usually 0
	cannot be NULL if os_window_handler is specified and dont_override_window_proc is set
	may be NULL otherwise*/
	Bool (*EventProc)(void *opaque, GF_Event *event);

	/*optional os window handler (HWND on win32/winCE, XWindow for X11)
	if not set the video outut will create and manage the display window.*/
	void *os_window_handler;
	/*for now, only used by X11 (indicates display the window is on)*/
	void *os_display;

	/*init flags bypassing GPAC config file	*/
	u32 init_flags;
	/*number of threads to use*/
	u32 threads;
	/*filter blacklist - temporary, should be moved to config ?*/
	const char *blacklist;
};

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_USER_H_*/

