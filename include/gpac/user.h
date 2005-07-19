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
	GF_EVT_WINDOWSIZE,	/*size has changed - indicate new w & h in .x end .y fields of event*/
	GF_EVT_SHOWHIDE,	/*window show/hide (minimized or other)*/
	GF_EVT_SET_CURSOR,	/*set mouse cursor*/
	GF_EVT_SET_STYLE,	/*set window style*/
	GF_EVT_SET_CAPTION,	/*set window caption*/
	GF_EVT_REFRESH, /*window needs repaint (whenever needed, eg restore, hide->show, background refresh, paint)*/
	GF_EVT_QUIT,	/*window is being closed*/
	GF_EVT_GL_CHANGED,	/*GL context has been changed*/
	/*terminal events*/
	GF_EVT_CONNECT,	/*signal URL is connected*/
	GF_EVT_DURATION,	/*signal duration of presentation*/
	/*signal size of the scene client display (if indicated in scene) upon connection
	if scene size hasn't changed (seeking or other) this event is not sent
	this event is also sent to video output for intial scene resize*/
	GF_EVT_SCENESIZE,	
	GF_EVT_AUTHORIZATION,	/*indicates a user and pass is queried*/
	GF_EVT_NAVIGATE, /*indicates the user app should load or jump to the given URL.*/
	GF_EVT_MESSAGE, /*message from the MPEG-4 terminal*/
	GF_EVT_VIEWPOINTS,	/*indicates viewpoint list has changed - no struct associated*/
	GF_EVT_STREAMLIST,	/*indicates stream list has changed - no struct associated - only used when no scene info is present*/
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
	/*mouse location in BIFS-like coordinates (window center is 0,0, increasing Y from bottom to top) */
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
	/*GF_EVT_WINDOWSIZE, GF_EVT_SCENESIZE*/
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
	/*GF_EVT_SET_STYLE*/
	u8 type;
	/*window style flags - NOT USED YET*/
	u32 window_style;
} GF_EventStyle;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVT_SET_CAPTION*/
	u8 type;
	/*window style flags - NOT USED YET*/
	const char *caption;
} GF_EventCaption;

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
	/*GF_EVT_NAVIGATE*/
	u8 type;
	/*new url to open / data to handle*/
	const char *to_url;
	/*parameters (cf vrml spec:) )*/
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
	GF_EventConnect connect;
	GF_EventCaption caption;
	GF_EventStyle style;
	GF_EventCursor cursor;
	GF_EventAuthorize auth;
} GF_Event;
	
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

	/*only used with os_window_handler (win32/CE). 
	If not set the window proc will be overriden and the video driver will take care of window messages
	If set the window proc will be untouched and the app will have to process the window messages
	*/
	Bool dont_override_window_proc;
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



typedef struct
{
	u32 width, height, pitch;
	u32 pixel_format;
	/*pointer to video memory (top, left)*/
	unsigned char *video_buffer;
	/*os specific handler if needed*/
	void *os_handle;
} GF_VideoSurface;


/*window for blitting - all coords are x, y at top-left, x+w, y+h at bottom-right */
typedef struct
{
	u32 x, y;
	u32 w, h;
} GF_Window;


#ifdef __cplusplus
}
#endif

#endif	/*_GF_USER_H_*/

