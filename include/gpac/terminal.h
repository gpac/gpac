/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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


/*!
\file <gpac/terminal.h>
\brief GPAC media player API.
 */
	
/*!
\addtogroup playback_grp Media Player
\brief GPAC media player (streaming engine and compositor).
*/
	
/*!
\addtogroup terminal_grp Terminal
\ingroup playback_grp
\brief GPAC media player APIs.

This section documents the user-level API of the GPAC media player.

@{
*/
	


#include <gpac/user.h>

/*! creates a new terminal for a userApp callback
\param user a user description
\return a new terminal*/
GF_Terminal *gf_term_new(GF_User *user);

/*! deletes a terminal - stop is done automatically
\param term the target terminal
\return error if any (GF_IO_ERR if client couldn't be shutdown normally)*/
GF_Err gf_term_del(GF_Terminal *term);

/*! connects to a URL - connection OK or error is acknowledged via the user callback
\param term the target terminal
\param URL the target URL
*/
void gf_term_connect(GF_Terminal *term, const char *URL);
/*! disconnects the current url
\param term the target terminal
*/
void gf_term_disconnect(GF_Terminal *term);

/*! navigates to a given destination or shutdown/restart the current one if any.
This is the only safe way of restarting/jumping a presentation from inside the EventProc
where doing a disconnect/connect could deadlock if toURL is NULL, uses the current URL
\param term the target terminal
\param toURL the new target URL
*/
void gf_term_navigate_to(GF_Terminal *term, const char *toURL);

/*! restarts url from given time (in ms).
\param term the target terminal
\param from_time restart time in milliseconds
\param pause_at_first_frame if 1, pauses at the first frame. If 2, pauses at the first frame only if the terminal is in paused state.
\return	0: service is not connected yet, 1: service has no seeking capabilities, 2: service has been seeked
*/
u32 gf_term_play_from_time(GF_Terminal *term, u64 from_time, u32 pause_at_first_frame);

/*! connects URL and seek right away - only needed when reloading the complete player (avoids waiting
for connection and post a seek..)

\param term the target terminal
\param URL the target URL
\param time_in_ms connect time in milliseconds
\param pause_at_first_frame if 1, pauses rendering and streaming when starting, if 2 pauses only rendering
*/
void gf_term_connect_from_time(GF_Terminal *term, const char *URL, u64 time_in_ms, u32 pause_at_first_frame);

/*! connects a URL but specify a parent path for this URL
\param term the target terminal
\param URL the target URL
\param parent_URL the parent URL of the connected service
*/
void gf_term_connect_with_path(GF_Terminal *term, const char *URL, const char *parent_URL);

/*! gets framerate
\param term the target terminal
\param absoluteFPS if GF_TRUE, the return value is the absolute framerate, eg NbFrameCount/NbTimeSpent regardless of whether a frame has been drawn or not, which means the FPS returned can be much greater than the target rendering framerate. If GF_FALSE, the return value is the FPS taking into account not drawn frames (eg, less than	or equal to compositor FPS)
\return current framerate
*/
Double gf_term_get_framerate(GF_Terminal *term, Bool absoluteFPS);
/*! gets main scene current time
\param term the target terminal
\return time in milliseconds
*/
u32 gf_term_get_time_in_ms(GF_Terminal *term);

/*! gets elapsed time since loading of the scene - may be different from scene time when seeking or live content
\param term the target terminal
\return time elapsed in milliseconds
*/
u32 gf_term_get_elapsed_time_in_ms(GF_Terminal *term);

/*! gets current URL
\param term the target terminal
\return the current URL
*/
const char *gf_term_get_url(GF_Terminal *term);

/*! gets viewpoints/viewports for main scene
\param term the target terminal
\param viewpoint_idx 1-based index of the viewport to query
\param outName set to the name of the viewport
\param is_bound set to GF_TRUE of that viewport is bound
\return error if any, GF_EOS if index is greater than number of viewpoints
*/
GF_Err gf_term_get_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char **outName, Bool *is_bound);

/*! sets active viewpoints/viewports for main scene given its name
\note if only one viewpoint is present in the scene
\param term the target terminal
\param viewpoint_idx 1-based index of the viewport to set or 0 to set by viewpoint name
\param viewpoint_name name of the viewport
\return error if any, GF_EOS if index is greater than number of viewpoints
*/
GF_Err gf_term_set_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char *viewpoint_name);

/*! adds an object to the scene - only possible if scene has selectable streams (cf GF_OPT_CAN_SELECT_STREAMS option)
\param term the target terminal
\param url the URL of the object to inject
\param auto_play selcts the object for playback when inserting it
\return error if any
*/
GF_Err gf_term_add_object(GF_Terminal *term, const char *url, Bool auto_play);

/*! sets option - most of the terminal cfg is done through options, please refer to options.h for details
\param term the target terminal
\param opt_type option to set
\param opt_value option value
\return error if any
*/
GF_Err gf_term_set_option(GF_Terminal *term, u32 opt_type, u32 opt_value);

/*! gets option - most of the terminal cfg is done through options, please refer to options.h for details
\param term the target terminal
\param opt_type option to set
\return option value
*/
u32 gf_term_get_option(GF_Terminal *term, u32 opt_type);

/*! checks if given URL is understood by client.
\param term the target terminal
\param URL the URL to check
\param use_parent_url if GF_TRUE, relative URLs are solved against the current presentation URL
\param no_mime_check if GF_TRUE, checks by file extension only
\return GF_TRUE if client should be able to handle the URL*/
Bool gf_term_is_supported_url(GF_Terminal *term, const char *URL, Bool use_parent_url, Bool no_mime_check);

/*! get the current service ID for MPEG-2 TS mux
\param term the target terminal
\return service ID or 0 if no service ID is associated or not loaded yet
*/
u32 gf_term_get_current_service_id(GF_Terminal *term);

/*! gets simulation frame rate
\param term the target terminal
\param nb_frames_drawn set to the number of frames drawn
\return simulation frames per seconds
*/
Double gf_term_get_simulation_frame_rate(GF_Terminal *term, u32 *nb_frames_drawn);

/*! gets visual output size (current window/display size)
\param term the target terminal
\param width set to the display width
\param height set to the display height
\return error if any
*/
GF_Err gf_term_get_visual_output_size(GF_Terminal *term, u32 *width, u32 *height);
/*! sets playback speed
\param term the target terminal
\param speed the requested speed
\return error if any
*/
GF_Err gf_term_set_speed(GF_Terminal *term, Fixed speed);

/*! sends a set of scene commands (BT, XMT, X3D, LASeR+XML) to the scene
\param term the target terminal
\param type indicates the language used - accepted values are
	"model/x3d+xml" or "x3d": commands is an X3D+XML scene
	"model/x3d+vrml" or  "xrdv": commands is an X3D+VRML scene
	"model/vrml" or "vrml": commands is an VRML scene
	"application/x-xmt" or "xmt": commands is an XMT-A scene or a list of XMT-A updates
	"application/x-bt" or "bt": commands is a BT scene or a list of BT updates
	"image/svg+xml" or "svg": commands is an SVG scene
	"application/x-laser+xml" or "laser": commands is an SVG/LASeR+XML  scene or a list of LASeR+XML updates
	if not specified, the type will be guessed from the current root node if any
\param com the textual update
\return error if any
*/
GF_Err gf_term_scene_update(GF_Terminal *term, char *type, char *com);

/*! sets visual output size change:
	* NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)
	* if the user app manages the output window it shall resize it before calling this

\param term the target terminal
\param NewWidth the output width in pixels
\param NewHeight the output height in pixels
\return error if any
*/
GF_Err gf_term_set_size(GF_Terminal *term, u32 NewWidth, u32 NewHeight);

/*! returns current text selection.
\param term the target terminal
\param probe_only if GF_TRUE, simply returns a non-NULL string ("") if some text is selected
\return text selection if any, or NULL otherwise. */
const char *gf_term_get_text_selection(GF_Terminal *term, Bool probe_only);

/*! pastes text into current selection if any.
\param term the target terminal
\param txt the text to set
\param probe_only if GF_TRUE, only check if text is currently edited
\return error if any
*/
GF_Err gf_term_paste_text(GF_Terminal *term, const char *txt, Bool probe_only);

/*! processes pending tasks in the media session
\note If filter session regulation is not disabled, the function will sleep for until next frame should be drawn before returning.
\param term the target terminal
\return GF_TRUE if a new frame was drawn, GF_FALSE otherwise
*/
Bool gf_term_process_step(GF_Terminal *term);

/*! post user interaction to terminal
\param term the target terminal
\param event the event to post
\return GF_TRUE if event was directly consumed
*/
Bool gf_term_user_event(GF_Terminal *term, GF_Event *event);

/*! post event to terminal
\warning NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)
\param term the target terminal
\param evt the event to post
\return GF_TRUE if event was directly consumed
*/
Bool gf_term_send_event(GF_Terminal *term, GF_Event *evt);




/*framebuffer access*/
#include <gpac/color.h>

/*! gets screen buffer
\note the screen buffer is released by calling gf_term_release_screen_buffer
\param term the target terminal
\param framebuffer set to the framebuffer information
\return error if any
*/
GF_Err gf_term_get_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer);

/*! gets view buffer - this locks the scene graph too until released is called
\note the screen buffer is released by calling gf_term_release_screen_buffer
\param term the target terminal
\param framebuffer set to the framebuffer information
\param view_idx indicates the view index, and ranges from 0 to GF_OPT_NUM_STEREO_VIEWS value
\param depth_buffer_type indicates the depth buffer mode
\return error if any
*/
GF_Err gf_term_get_offscreen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer, u32 view_idx, GF_CompositorGrabMode depth_buffer_type);

/*! releases screen buffer and unlocks graph
\param term the target terminal
\param framebuffer the pointer passed to \ref gf_term_get_screen_buffer
\return error if any
*/
GF_Err gf_term_release_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer);

/*! switches quality up or down - can be called several time in the same direction
this will call all decoders to adjust their quality levels

\param term the target terminal
\param up if GF_TRUE, switches quality up,otherwise down
*/
void gf_term_switch_quality(GF_Terminal *term, Bool up);

/*! checks if a mime type is supported
\param term the target terminal
\param mime the mime type to check
\return GF_TRUE if supported
*/
Bool gf_term_is_type_supported(GF_Terminal *term, const char* mime);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_H_*/
