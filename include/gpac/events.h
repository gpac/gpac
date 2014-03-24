/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Events management
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



#ifndef _GF_EVENTS_H_
#define _GF_EVENTS_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/math.h>
#include <gpac/tools.h>
#include <gpac/events_constants.h>


/*mouse button modifiers*/
enum
{
	GF_MOUSE_LEFT = 0,
	GF_MOUSE_MIDDLE,
	GF_MOUSE_RIGHT
};

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_MOUSEMOVE, GF_EVENT_MOUSEWHEEL, GF_EVENT_MOUSEDOWN, GF_EVENT_MOUSEUP*/
	u8 type;
	/*mouse location in output window, 2D-like:  top-left (0,0), increasing y towards bottom*/
	s32 x, y;
	/*wheel position (wheel current delta / wheel absolute delta) for GF_EVENT_MouseWheel*/
	Fixed wheel_pos;
	/*0: left - 1: middle, 2- right*/
	u32 button;
	/*key modifier*/
	u32 key_states;
} GF_EventMouse;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_KEYDOWN and GF_EVENT_KEYUP*/
	u8 type;
	/*above GPAC/DOM key code*/
	u32 key_code;
	/* hadrware key value (matching ASCI) */
	u32 hw_code;
	/*key modifier*/
	u32 flags;
} GF_EventKey;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_TEXTINPUT*/
	u8 type;
	/*above virtual key code*/
	u32 unicode_char;
} GF_EventChar;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_SIZE*/
	u8 type;
	/*width and height*/
	u16 width, height;
} GF_EventSize;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_VIDEO_SETUP*/
	u8 type;
	/*width and height of visual surface to allocate*/
	u16 width, height;
	/*indicates whether double buffering is desired - when sent from plugin to player, indicates the backbuffer has been destroyed*/
	Bool back_buffer;
	/*indicates whether system memory for the backbuffer is desired (no video blitting)*/
	Bool system_memory;
	/*indicates whether opengl context shall be created. Values are:
		0: no opengl context shall be created
		1: opengl context shall be created for the main window and set as the current one
		2: an extra opengl context shall be created for offscreen rendering and set as the current one
			if not supported, mix of 2D (raster) and 3D (openGL) will be disabled
	*/
	u32 opengl_mode;

	// resources must be resetup before next render step (this is mainly due to discard all openGL textures and cached objects) - inly used when sent from plugin to term
	Bool hw_reset;
} GF_EventVideoSetup;

/*event proc return value: ignored
this event may be triggered by the compositor if owning window or if shortcut fullscreen is detected*/
typedef struct
{
	/*GF_EVENT_SHOWHIDE*/
	u8 type;
	/*0: hidden - 1: visible - 2: fullscreen*/
	u32 show_type;
} GF_EventShow;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_SET_CURSOR*/
	u8 type;
	/*set if is visible*/
	u32 cursor_type;
} GF_EventCursor;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_SET_CAPTION*/
	u8 type;
	/*window style flags - NOT USED YET*/
	const char *caption;
} GF_EventCaption;

/*event proc: never posted*/
typedef struct
{
	/*GF_EVENT_MOVE*/
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
	/*GF_EVENT_DURATION*/
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
	/*GF_EVENT_NAVIGATE and GF_EVENT_NAVIGATE_INFO*/
	u8 type;
	/*new url to open / data to handle*/
	const char *to_url;
	/*parameters (cf vrml spec) - UNUSED for GF_EVENT_NAVIGATE_INFO*/
	u32 param_count;
	const char **parameters;
} GF_EventNavigate;


/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_MESSAGE*/
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
	/*GF_EVENT_PROGRESS*/
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
	/* for download events */
	u32 bytes_per_seconds;
} GF_EventProgress;

/*event proc return value: ignored*/
typedef struct
{
	/*GF_EVENT_CONNECT*/
	u8 type;
	/*sent upon connection/deconnection completion. if any error, it is signaled through message event*/
	Bool is_connected;
} GF_EventConnect;

/*event proc return value: 1 to indicate the terminal should attempt a default layout for this addon, 0: nothing will be done*/
typedef struct
{
	/*GF_EVENT_ADDON_DETECTED*/
	u8 type;
	const char *addon_url;
	const char *mime_type;
} GF_EventAddonConnect;

/*event proc return value: 1 if info has been completed, 0 otherwise (and operation this request was for
will then fail)*/
typedef struct
{
	/*GF_EVENT_AUTHORIZATION*/
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
	/*GF_EVENT_SYS_COLORS*/
	u8 type;
	/*ARGB colors, in order:
	ActiveBorder, ActiveCaption, AppWorkspace, Background, ButtonFace, ButtonHighlight, ButtonShadow,
	ButtonText, CaptionText, GrayText, Highlight, HighlightText, InactiveBorder, InactiveCaption,
	InactiveCaptionText, InfoBackground, InfoText, Menu, MenuText, Scrollbar, ThreeDDarkShadow,
	ThreeDFace, ThreeDHighlight, ThreeDLightShadow, ThreeDShadow, Window, WindowFrame, WindowText
	*/
	u32 sys_colors[28];
} GF_EventSysColors;


typedef struct {
	/* GF_EVENT_TREE_MODIFIED, GF_EVENT_NODE_INSERTED, GF_EVENT_NODE_REMOVED, GF_EVENT_NODE_INSERTED_DOC, GF_EVENT_NODE_REMOVED_DOC, GF_EVENT_ATTR_MODIFIED, GF_EVENT_CHAR_DATA_MODIFIED */
	u8 type;
	void *relatedNode;
	void *prevValue;
	void *newValue;
	void *attrName;
	u8 attrChange;
} GF_EventMutation;


typedef struct {
	/* GF_EVENT_OPENFILE*/
	u8 type;
	u32 nb_files;
	char **files;
} GF_EventOpenFile;

typedef struct {
	/* GF_EVENT_FORWARDED*/
	u8 type;
	/*one of te above event*/
	u8 forward_type;
	/*original type of event as forwarded by the service*/
	u32 service_event_type;
	/*parameter of event as forwarded by the service - creation/deletion is handled by the service*/
	void *param;
} GF_EventForwarded;

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
	GF_EventVideoSetup setup;
	GF_EventMutation mutation;
	GF_EventForwarded forwarded_event;
	GF_EventOpenFile open_file;
	GF_EventAddonConnect addon_connect;
} GF_Event;


#ifdef __cplusplus
}
#endif

#endif	/*_GF_EVENTS_H_*/

