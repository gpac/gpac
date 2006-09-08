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



#ifndef _GF_USER_H_
#define _GF_USER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/math.h>
#include <gpac/module.h>

/*GPAC client terminal*/
typedef struct _tag_terminal GF_Terminal;


/*
		minimal event system handling both WM messages (from video driver) and terminal messages
			inspired from SDL nice event handling
*/

/*events*/
enum {
	/*Mouse move*/
	GF_EVT_MOUSEMOVE = 0,
	/*mouse buttons - note that VRML sensors only deal with events <= GF_EVT_LEFTUP*/
	GF_EVT_LEFTDOWN,
	GF_EVT_LEFTUP,
	GF_EVT_MIDDLEDOWN,
	GF_EVT_MIDDLEUP,
	GF_EVT_RIGHTDOWN,
	GF_EVT_RIGHTUP,
	GF_EVT_LDOUBLECLICK,
	/*wheel moves*/
	GF_EVT_MOUSEWHEEL,
	/*key actions*/
	GF_EVT_CHAR,	/*valid UTF-16 translated char*/
	GF_EVT_VKEYDOWN,
	GF_EVT_VKEYUP,
	GF_EVT_KEYDOWN,
	GF_EVT_KEYUP,
	/*window events*/
	/*size has changed - indicate new w & h in .x end .y fields of event. 
	When sent from gpac to a video plugin, indicates the output size should be changed. This is only sent when the plugin
	manages the output video himself
	When sent from a video plugin to gpac, indicates the output size has been changed. This is only sent when the plugin
	manages the output video himself
	*/
	GF_EVT_SIZE,		
	/*signals the scene size (if indicated in scene) upon connection (sent to the user event proc only)
		if scene size hasn't changed (seeking or other) this event is not sent
	*/
	GF_EVT_SCENE_SIZE,		
	GF_EVT_SHOWHIDE,	/*window show/hide (minimized or other). This is also sent to the user to signal focus switch in fullscreen*/
	GF_EVT_SET_CURSOR,	/*set mouse cursor*/
	GF_EVT_SET_CAPTION,	/*set window caption*/
	GF_EVT_MOVE,		/*move window*/
	GF_EVT_REFRESH, /*window needs repaint (whenever needed, eg restore, hide->show, background refresh, paint)*/
	GF_EVT_QUIT,	/*window is being closed*/
	/*video hw setup message:
		- when sent from gpac to plugin, indicates that the plugin should re-setup hardware context due to a window resize:
			* for 2D output, this means resizing the backbuffer if needed (depending on HW constraints)
			* for 3D output, this means re-setup of OpenGL context (depending on HW constraints). Depending on windowing systems 
			and implementations, it could be possible to resize a window without destroying the GL context.

		- when sent from plugin to gpac, indicates that hardware resources must be resetup before next render step (this is mainly
		due to discard all openGL textures and cached objects)
	*/
	GF_EVT_VIDEO_SETUP,

	/*terminal events*/
	GF_EVT_CONNECT,	/*signal URL is connected*/
	GF_EVT_DURATION,	/*signal duration of presentation*/
	GF_EVT_AUTHORIZATION,	/*indicates a user and pass is queried*/
	GF_EVT_NAVIGATE, /*indicates the user app should load or jump to the given URL.*/
	GF_EVT_NAVIGATE_INFO, /*indicates the link or its description under the mouse pointer*/
	GF_EVT_MESSAGE, /*message from the MPEG-4 terminal*/
	GF_EVT_PROGRESS, /*progress message from the MPEG-4 terminal*/
	GF_EVT_VIEWPOINTS,	/*indicates viewpoint list has changed - no struct associated*/
	GF_EVT_STREAMLIST,	/*indicates stream list has changed - no struct associated - only used when no scene info is present*/

	GF_EVT_SYS_COLORS,	/*queries the list of system colors*/
	GF_EVT_RESET_RTI,	/*forces a reset of the runtime info*/
	GF_EVT_UPDATE_RTI,	/*updates runtime info*/
};

/*virtual keys - codes respect MPEG4/VRML KeySensor ones*/
enum
{
	GF_VK_NONE = 0,
	/*key-sensor codes start*/
	GF_VK_F1,
	GF_VK_F2,
	GF_VK_F3,
	GF_VK_F4,
	GF_VK_F5,
	GF_VK_F6,
	GF_VK_F7,
	GF_VK_F8,
	GF_VK_F9,
	GF_VK_F10,
	GF_VK_F11,
	GF_VK_F12,
	GF_VK_HOME,
	GF_VK_END,
	GF_VK_PRIOR,
	GF_VK_NEXT,
	GF_VK_UP,
	GF_VK_DOWN,
	GF_VK_LEFT,
	GF_VK_RIGHT,
	/*key-sensor codes end*/
	GF_VK_RETURN,
	GF_VK_ESCAPE,
	GF_VK_SHIFT,
	GF_VK_CONTROL,
	GF_VK_MENU,
};

/*key modifiers state - set by terminal (not set by video driver)*/
enum
{
	GF_KM_SHIFT = (1),
	GF_KM_CTRL = (1<<2),
	GF_KM_ALT = (1<<3)
};

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_MOUSEMOVE, GF_EVT_MOUSEWHEEL, GF_EVT_LEFTDOWN, GF_EVT_LEFTUP, GF_EVT_MIDDLEDOWN, GF_EVT_MIDDLEUP, GF_EVT_RIGHTDOWN, GF_EVT_RIGHTUP*/
	u8 type;
	/*mouse location in output window, 2D-like:  top-left (0,0), increasing y towards bottom*/
	s32 x, y;
	/*wheel position (wheel current delta / wheel absolute delta) for GF_EVT_MouseWheel*/
	Fixed wheel_pos;
	/*key modifier*/
	u32 key_states;
} GF_EventMouse;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_VK* and GF_EVT_K**/
	u8 type;
	/*above virtual key code*/
	u32 vk_code;
	/* key virtual code (matching ASCI) */
	u32 virtual_code;
	/*key modifier*/
	u32 key_states;
} GF_EventKey;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_CHAR*/
	u8 type;
	/*above virtual key code*/
	u32 unicode_char;
} GF_EventChar;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_SIZE*/
	u8 type;
	/*width and height*/
	u16 width, height;
} GF_EventSize;

/*event proc return value: ignored
this event may be triggered by the renderer if owning window or if shortcut fullscreen is detected*/
typedef struct
{
	/*GF_EVT_SHOWHIDE*/
	u8 type;
	/*0: hidden - 1: visible - 2: fullscreen*/
	u32 show_type;
} GF_EventShow;


/*sensor signaling*/
enum
{
	GF_CURSOR_NORMAL = 0x00,
	GF_CURSOR_ANCHOR, 
	GF_CURSOR_TOUCH,
	/*discSensor, cylinderSensor, sphereSensor*/
	GF_CURSOR_ROTATE, 
	/*proximitySensor & proximitySensor2D*/
	GF_CURSOR_PROXIMITY, 
	/*planeSensor & planeSensor2D*/
	GF_CURSOR_PLANE,
	/*collision*/
	GF_CURSOR_COLLIDE, 
	GF_CURSOR_HIDE, 
};

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_SET_CURSOR*/
	u8 type;
	/*set if is visible*/
	u32 cursor_type;
} GF_EventCursor;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_SET_CAPTION*/
	u8 type;
	/*window style flags - NOT USED YET*/
	const char *caption;
} GF_EventCaption;

/*event proc: never posted*/
typedef struct
{
	/*GF_EVT_MOVE*/
	u8 type;
	s32 x, y;
	/*0: absolute positionning, 1: relative move, 2: use alignment constraints*/
	Bool relative;
	/*0: left/top, 1: middle, 2: right/bottom*/
	u8 align_x, align_y;
} GF_EventMove;

/*duration may be signaled several times: it may change when setting up streams
event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_DURATION*/
	u8 type;
	/*duration in seconds*/
	Double duration;
	/*is seeking supported for service*/
	Bool can_seek;
} GF_EventDuration;

/*event proc return value: 0 if URL not supported, 1 if accepted (it is the user responsability to load the url)
YOU SHALL NOT DIRECTLY OPEN THE NEW URL IN THE EVENT PROC, THIS WOULD DEADLOCK THE TERMINAL
*/
typedef struct
{
	/*GF_EVT_NAVIGATE and GF_EVT_NAVIGATE_INFO*/
	u8 type;
	/*new url to open / data to handle*/
	const char *to_url;
	/*parameters (cf vrml spec) - UNUSED for GF_EVT_NAVIGATE_INFO*/
	u32 param_count;
	const char **parameters;
} GF_EventNavigate;


/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_MESSAGE*/
	u8 type;
	/*name of service issuing the message*/
	const char *service;
	/*message*/
	const char *message;
	/*error if any*/
	GF_Err error;
} GF_EventMessage;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_PROGRESS*/
	u8 type;
	/*name of service issuing the progress notif*/
	const char *service;
	/*progress type: 0: buffering, 1: downloading, 2: importing (BT/VRML/...)*/
	u32 progress_type;
	/*amount done and total amount of operation.
		For buffer events, expresses current and total desired stream buffer in scene in milliseconds
		For download events, expresses current and total size of download in bytes
		For import events, no units defined (depends on importers)
	*/
	u32 done, total;
} GF_EventProgress;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_CONNECT*/
	u8 type;
	/*sent upon connection/deconnection completion. if any error, it is signaled through message event*/
	Bool is_connected;
} GF_EventConnect;

/*event proc return value: 1 if info has been completed, 0 otherwise (and operation this request was for
will then fail)*/
typedef struct
{
	/*GF_EVT_AUTHORIZATION*/
	u8 type;
	/*the URL the auth request is for*/
	const char *site_url;
	/*user name (provided buffer can hold 50 bytes). It may already be formated, or an empty ("") string*/
	char *user;
	/*password (provided buffer can hold 50 bytes)*/
	char *password;
} GF_EventAuthorize;


/*event proc return value: 1 if info has been completed, 0 otherwise */
typedef struct
{
	/*GF_EVT_SYS_COLORS*/
	u8 type;
	/*ARGB colors, in order:
	ActiveBorder, ActiveCaption, AppWorkspace, Background, ButtonFace, ButtonHighlight, ButtonShadow, 
	ButtonText, CaptionText, GrayText, Highlight, HighlightText, InactiveBorder, InactiveCaption, 
	InactiveCaptionText, InfoBackground, InfoText, Menu, MenuText, Scrollbar, ThreeDDarkShadow, 
	ThreeDFace, ThreeDHighlight, ThreeDLightShadow, ThreeDShadow, Window, WindowFrame, WindowText
	*/
	u32 sys_colors[28];
} GF_EventSysColors;


typedef union
{
	u8 type;
	GF_EventMouse mouse;
	GF_EventKey key;
	GF_EventChar character;
	GF_EventSize size;
	GF_EventShow show;
	GF_EventDuration duration;
	GF_EventNavigate navigate;
	GF_EventMessage message;
	GF_EventProgress progress;
	GF_EventConnect connect;
	GF_EventCaption caption;
	GF_EventCursor cursor;
	GF_EventAuthorize auth;
	GF_EventSysColors sys_cols;
	GF_EventMove move;
} GF_Event;
	

enum
{
	/*display should be hidden upon initialization*/
	GF_TERM_INIT_HIDE = 1,
	/*no audio renderer will be created*/
	GF_TERM_NO_AUDIO = 1<<1,
	/*terminal is used to extract content: 
		* audio render is disabled
		* media codecs are not threaded
		* all composition memories are filled before rendering
		* rendering is done after media decoding
		* frame-rate regulation is disabled (no sleep)
		* the user is responsible for updating the terminal
	*/
	GF_TERM_NOT_THREADED = 1<<2,
	/*forces 2D renderer, regardless of config file*/
	GF_TERM_FORCE_2D = 1<<3,
	/*forces 3D renderer, regardless of config file*/
	GF_TERM_FORCE_3D = 1<<4,
	/*works in windowless mode - experimental, only supported on Win32*/
	GF_TERM_WINDOWLESS = 1<<5
};

/*user object for all callbacks*/
typedef struct 
{
	/*user defined callback for all functions - cannot be NULL*/
	void *opaque;
	/*the event proc. Return value depend on the event type, usually 0
	cannot be NULL if os_window_handler is specified and dont_override_window_proc is set
	may be NULL otherwise*/
	Bool (*EventProc)(void *opaque, GF_Event *event);

	/*config file of client - cannot be NULL*/
	GF_Config *config;
	/*modules manager - cannot be NULL - owned by the user (to allow selection of module directory)*/
	GF_ModuleManager *modules;
	/*optional os window handler (HWND on win32/winCE, XWindow for X11) 
	if not set the video outut will create and manage the display window.*/
	void *os_window_handler;
	/*for now, only used by X11 (indicates display the window is on)*/
	void *os_display;

	/*init flags bypassing GPAC config file	*/
	u32 init_flags;
} GF_User;


/*macro for event forwarding*/
#define GF_USER_SENDEVENT(_user, _evt)	if (_user->EventProc) _user->EventProc(_user->opaque, _evt)

/*macro for message event format/send*/
#define GF_USER_MESSAGE(_user, _serv, _msg, _e)	\
	{	\
		GF_Event evt;	\
		if (_user->EventProc) {	\
			evt.type = GF_EVT_MESSAGE;	\
			evt.message.service = _serv;	\
			evt.message.message = _msg;	\
			evt.message.error = _e;	\
			_user->EventProc(_user->opaque, &evt);	\
		}	\
	}


#ifdef __cplusplus
}
#endif

#endif	/*_GF_USER_H_*/

