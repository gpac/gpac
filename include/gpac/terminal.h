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
void gf_term_play_from_time(GF_Terminal *term, u32 from_time);
/*connect URL and seek right away - only needed when reloading the complete player (avoids waiting
for connection and post a seek..)*/
void gf_term_connect_from_time(GF_Terminal *term, const char *URL, u32 time_in_ms);

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


/*
		OD browsing and misc exports
*/

#include <gpac/mpeg4_odf.h>

/*
	OD Browsing API - ALL ITEMS ARE READ-ONLY AND SHALL NOT BE MODIFIED
*/
typedef struct _od_manager GF_ObjectManager;

/*returns top-level OD of the presentation*/
GF_ObjectManager *gf_term_get_root_object(GF_Terminal *term);
/*returns number of sub-ODs in the current root. scene_od must be an inline OD*/
u32 gf_term_get_object_count(GF_Terminal *term, GF_ObjectManager *scene_od);
/*returns indexed (0-based) OD manager in the scene*/
GF_ObjectManager *gf_term_get_object(GF_Terminal *term, GF_ObjectManager *scene_od, u32 index);
/*returns remote GF_ObjectManager of this OD if any, NULL otherwise*/
GF_ObjectManager *gf_term_get_remote_object(GF_Terminal *term, GF_ObjectManager *odm);
/*returns 0 if not inline, 1 if inline, 2 if externProto scene*/
u32 gf_term_object_subscene_type(GF_Terminal *term, GF_ObjectManager *odm);

/*select given object when stream selection is available*/
void gf_term_select_object(GF_Terminal *term, GF_ObjectManager *odm);

typedef struct
{
	GF_ObjectDescriptor *od;
	Double duration;
	Double current_time;
	/*0: stoped, 1: playing, 2: paused, 3: not setup, 4; setup failed.*/
	u32 status;
	/*if set, the PL flags are valid*/
	Bool has_profiles;
	Bool inline_pl;
	u8 OD_pl; 
	u8 scene_pl;
	u8 audio_pl;
	u8 visual_pl;
	u8 graphics_pl;

	/*name of module handling the service service */
	const char *service_handler;
	/*name of service*/
	const char *service_url;
	/*set if the service is owned by this object*/
	Bool owns_service;

	/*stream buffer:
		-2: stream is not playing
		-1: stream has no buffering
		>=0: amount of media data present in buffer, in ms
	*/
	s32 buffer;
	/*number of AUs in DB (cumulated on all input channels)*/
	u32 db_unit_count;
	/*number of CUs in composition memory (if any) and CM capacity*/
	u16 cb_unit_count, cb_max_count;
	/*clock drift in ms of object clock: this is the delay set by the audio renderer to keep AV in sync*/
	s32 clock_drift;
	/*codec name*/
	const char *codec_name;
	/*object type - match streamType (cf constants.h)*/
	u32 od_type;
	/*audio properties*/
	u32 sample_rate, bits_per_sample, num_channels;
	/*video properties (w & h also used for scene codecs)*/
	u32 width, height, pixelFormat, par;

	/*average birate over last second and max bitrate over one second at decoder input - expressed in bits per sec*/
	u32 avg_bitrate, max_bitrate;
	u32 total_dec_time, max_dec_time, nb_dec_frames;

	/*set if ISMACryp present on the object - will need refinement for IPMPX...
	0: not protected - 1: protected and OK - 2: protected and DRM failed*/
	u32 protection;
} ODInfo;

/*fills the ODInfo structure describing the OD manager*/
GF_Err gf_term_get_object_info(GF_Terminal *term, GF_ObjectManager *odm, ODInfo *info);
/*gets current downloads info for the service - only use if ODM owns thesrevice, returns 0 otherwise.
	@d_enum: in/out current enum - shall start to 0, incremented at each call. fct returns 0 if no more 
	downloads
	@server: server name
	@path: file/data location on server
	@bytes_done, @total_bytes: file info. total_bytes may be 0 (eg http streaming)
	@bytes_per_sec: guess what
*/
Bool gf_term_get_download_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, const char **server, const char **path, u32 *bytes_done, u32 *total_bytes, u32 *bytes_per_sec);

/*same principles as above , struct __netcom is defined in net_api.h*/
typedef struct __netcom NetStatCommand;
Bool gf_term_get_channel_net_info(GF_Terminal *term, GF_ObjectManager *odm, u32 *d_enum, u32 *chid, NetStatCommand *netcom, GF_Err *ret_code);

/*retrieves world info of the scene @od belongs to. 
If @odm is or points to an inlined OD the world info of the inlined content is retrieved
If @odm is NULL the world info of the main scene is retrieved
returns NULL if no WorldInfo available
returns world title if available 
@descriptions: any textual descriptions is stored here
  all strings are allocated by term and shall be freed by user
*/
char *gf_term_get_world_info(GF_Terminal *term, GF_ObjectManager *scene_od, GF_List *descriptions);

/*dumps scene graph in specified file, in BT or XMT format
@rad_name: file radical (NULL for stdout) - if not NULL MUST BE GF_MAX_PATH length
if @skip_proto is set proto declarations are not dumped
If @odm is or points to an inlined OD the inlined scene is dumped
If @odm is NULL the main scene is dumped
*/
GF_Err gf_term_dump_scene(GF_Terminal *term, char *rad_name, Bool xml_dump, Bool skip_proto, GF_ObjectManager *odm);


/*refresh window info when window moved (redraws offscrenn to screen without rendering) */
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_refresh(GF_Terminal *term);

/*request window size change (window resized)*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
GF_Err gf_term_set_size(GF_Terminal *term, u32 NewWidth, u32 NewHeight);

/*signal window size change (window resized)*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
GF_Err gf_term_size_changed(GF_Terminal *term, u32 NewWidth, u32 NewHeight);

/*post user interaction to terminal*/
/*NOT NEEDED WHEN THE TERMINAL IS HANDLING THE DISPLAY WINDOW (cf user.h)*/
void gf_term_user_input(GF_Terminal *term, GF_Event *event);

/*post extended user mouse interaction to terminal 
	X and Y are point coordinates in the display expressed in BIFS-like fashion (0,0) at center of 
	display and Y increasing from bottom to top
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


#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_H_*/
