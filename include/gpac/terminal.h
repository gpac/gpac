/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#ifndef _GF_TERMINAL_H_
#define _GF_TERMINAL_H_


#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/user.h>

/*creates a new terminal for a userApp callback*/
GF_Terminal *gf_term_new(GF_User *user);

/*delete the app - stop is done automatically, you don't have to do it before deleting the app
returns GF_IO_ERR if client couldn't be shutdown normally*/
GF_Err gf_term_del(GF_Terminal *term);

/*connects to a URL*/
void gf_term_connect(GF_Terminal *term, const char *URL);
/*disconnects the url*/
void gf_term_disconnect(GF_Terminal *term);
/*reloads (shutdown/restart) the current url if any. This is the only safe way of restarting a 
presentation from inside the EventProc where doing a disconnect/connect could deadlock*/
void gf_term_reload(GF_Terminal *term);
/*restarts url from given time (in ms)*/
void gf_term_play_from_time(GF_Terminal *term, u32 from_time, Bool pause_at_first_frame);
/*connect URL and seek right away - only needed when reloading the complete player (avoids waiting
for connection and post a seek..)*/
void gf_term_connect_from_time(GF_Terminal *term, const char *URL, u32 time_in_ms, Bool pause_at_first_frame);

/*returns current framerate
	if @absoluteFPS is set, the return value is the absolute framerate, eg NbFrameCount/NbTimeSpent regardless of
whether a frame has been drawn or not, which means the FPS returned can be much greater than the target rendering 
framerate
	if @absoluteFPS is not set, the return value is the FPS taking into account not drawn frames (eg, less than 
	or equal to renderer FPS)
*/
Double gf_term_get_framerate(GF_Terminal *term, Bool absoluteFPS);
/*get main scene current time in milliseconds*/
u32 gf_term_get_time_in_ms(GF_Terminal *term);

/*get viewpoints/viewports for main scene - idx is 1-based, and if greater than number of viewpoints return GF_EOS*/
GF_Err gf_term_get_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char **outName, Bool *is_bound);
/*set active viewpoints/viewports for main scene given its name - idx is 1-based, or 0 to set by viewpoint name
if only one viewpoint is present in the scene, this will bind/unbind it*/
GF_Err gf_term_set_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char *viewpoint_name);

/*adds an object to the scene - only possible if scene has selectable streams (cf GF_OPT_CAN_SELECT_STREAMS option)*/
GF_Err gf_term_add_object(GF_Terminal *term, const char *url, Bool auto_play);


/*set/set option - most of the terminal cfg is done through options, please refer to user.h for details*/
GF_Err gf_term_set_option(GF_Terminal *term, u32 opt_type, u32 opt_value);
u32 gf_term_get_option(GF_Terminal *term, u32 opt_type);

/*checks if given URL is understood by client.
if use_parent_url is set, relative URLs are solved against the current presentation URL*/
Bool gf_term_is_supported_url(GF_Terminal *term, const char *fileName, Bool use_parent_url, Bool no_mime_check);

/*sets simulation frame rate*/
GF_Err gf_term_set_simulation_frame_rate(GF_Terminal * term, Double frame_rate);
/*gets simulation frame rate*/
Double gf_term_get_simulation_frame_rate(GF_Terminal *term);


/*refresh window info when window moved (redraws offscrenn to screen without rendering) */
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_refresh(GF_Terminal *term);

/*request visual output size change:
	* NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)
	* if the user app manages the output window it shall resize it before calling this
*/
GF_Err gf_term_set_size(GF_Terminal *term, u32 NewWidth, u32 NewHeight);

/*post user interaction to terminal*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_user_event(GF_Terminal *term, GF_Event *event);

/*post extended user mouse interaction to terminal 
	X and Y are point coordinates in the display expressed in 2D coord system top-left (0,0), Y increasing towards bottom
	@xxx_but_down: specifiy whether the mouse button is down(2) or up (1), 0 if unchanged
	@wheel: specifiy current wheel inc (0: unchanged , +1 for one wheel delta forward, -1 for one wheel delta backward)
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_mouse_input(GF_Terminal *term, GF_EventMouse *event);

/*post extended user key interaction to terminal 
	@keyPressed, keyReleased: UTF-8 char code of regular keys
	@actionKeyPressed, actionKeyReleased: UTF-8 char code of action keys (Fxx, up, down, left, right, home, end, page up, page down) - cf spec for values
	@xxxKeyDown: specifiy whether given key is down (2) or up (1), 0 if unchanged
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_keyboard_input(GF_Terminal *term, s32 keyPressed, s32 keyReleased, s32 actionKeyPressed, s32 actionKeyReleased, u32 shiftKeyDown, u32 controlKeyDown, u32 altKeyDown);

/*post extended user character interaction to terminal 
	@character: unicode character input
*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_string_input(GF_Terminal *term, u32 character);



/*ObjectManager used by both terminal and object browser (term_info.h)*/
typedef struct _od_manager GF_ObjectManager;

#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_H_*/
