/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

/*!
\file <gpac/events.h>
\brief Event system used by GPAC playback.
 */
	
/*!
\addtogroup evt_grp Event System
\ingroup playback_grp
\brief Event system used by GPAC playback.

This section documents the event structures used by thethe compositor, input modules and output rendering modules for communication.
@{
 */

#include <gpac/maths.h>
#include <gpac/tools.h>
#include <gpac/events_constants.h>


/*! mouse button modifiers*/
enum
{
	GF_MOUSE_LEFT = 0,
	GF_MOUSE_MIDDLE,
	GF_MOUSE_RIGHT
};

/*! Mouse event structure
	event proc return value: ignored
*/
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
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventMouse;

/*! Mouse event structure
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_MULTITOUCH*/
	u8 type;
	/*normalized center of multitouch event*/
	Fixed x, y;
	/*finger rotation*/
	Fixed rotation;
	/*finger pinch*/
	Fixed pinch;
	/*number of fingers detected*/
	u32 num_fingers;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventMultiTouch;

/*! Keyboard key event
	event proc return value: ignored
*/
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
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventKey;

/*! Keyboard character event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_TEXTINPUT*/
	u8 type;
	/*above virtual key code*/
	u32 unicode_char;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventChar;

/*! Display orientation */
typedef enum
{
	/*! Unknown display orientation */
    GF_DISPLAY_MODE_UNKNOWN=0,
    /*! portrait display orientation*/
    GF_DISPLAY_MODE_PORTRAIT,
    /*! landscape display orientation (+90deg anti-clockwise from portrait)*/
    GF_DISPLAY_MODE_LANDSCAPE,
    /*! inverted landscape display orientation (-90deg anti-clockwise from portrait)*/
    GF_DISPLAY_MODE_LANDSCAPE_INV,
    /*! upside down portrait display orientation (+180deg anti-clockwise from portrait)*/
    GF_DISPLAY_MODE_PORTRAIT_INV
} GF_DisplayOrientationType;

/*! Window resize event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SIZE, GF_EVENT_SCENE_SIZE*/
	u8 type;
	/*width and height*/
	u32 width, height;
	/*display orientation */
	GF_DisplayOrientationType orientation;
	/*ID of window the event occured in (driver to app only)
	For GF_EVENT_SCENE_SIZE, a value different from 0 indicates the root node is connected (scene is parsed)*/
	u32 window_id;
} GF_EventSize;

/*! Video setup (2D or 3D) event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_VIDEO_SETUP*/
	u8 type;
	/*width and height of visual surface to allocate*/
	u32 width, height;
	/*indicates whether double buffering is desired - when sent from plugin to player, indicates the backbuffer has been destroyed*/
	Bool back_buffer;
	/*indicates whether system memory for the backbuffer is desired (no video blitting)*/
	Bool system_memory;
	/*indicates whether opengl context shall be created. Values are:
		GF_FALSE: no opengl context shall be created
		GF_TRUE: opengl context shall be created for the main window and set as the current one
	*/
	Bool use_opengl;

	Bool disable_vsync;

	// resources must be resetup before next render step (this is mainly due to discard all OpenGL textures and cached objects) - inly used when sent from plugin to term
	Bool hw_reset;
} GF_EventVideoSetup;

/*! Windows show event
	event proc return value: ignored
	this event may be triggered by the compositor if owning window or if shortcut fullscreen is detected
*/
typedef struct
{
	/*GF_EVENT_SHOWHIDE*/
	u8 type;
	/*0: hidden - 1: visible - 2: fullscreen*/
	u32 show_type;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventShow;

/*! Mouse cursor event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SET_CURSOR*/
	u8 type;
	/*set if is visible*/
	u32 cursor_type;
} GF_EventCursor;

/*! Window caption event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SET_CAPTION*/
	u8 type;
	const char *caption;
} GF_EventCaption;

/*! Window move event
	event proc: never posted
*/
typedef struct
{
	/*GF_EVENT_MOVE*/
	u8 type;
	s32 x, y;
	/*0: absolute positionning, 1: relative move, 2: use alignment constraints*/
	u32 relative;
	/*0: left/top, 1: middle, 2: right/bottom*/
	u8 align_x, align_y;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventMove;

/*! Media duration event
	duration may be signaled several times: it may change when setting up streams
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_DURATION*/
	u8 type;
	/*duration in seconds*/
	Double duration;
	/*is seeking supported for service*/
	Bool can_seek;
} GF_EventDuration;

/*! Hyperlink navigation event
	event proc return value: 0 if URL not supported, 1 if accepted (it is the user responsibility to load the url)
	YOU SHALL NOT DIRECTLY OPEN THE NEW URL IN THE EVENT PROC, THIS WOULD DEADLOCK THE COMPOSITOR
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


/*! Service message event
	event proc return value: ignored
*/
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

/*! Progress event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_PROGRESS*/
	u8 type;
	/*name of service issuing the progress notif*/
	const char *service;
	/*progress type: 0: buffering, 1: downloading, 2: importing (BT/VRML/...), 3: filter status update*/
	u32 progress_type;
	/*amount done and total amount of operation.
		For buffer events, expresses current and total desired stream buffer in scene in milliseconds
		For download events, expresses current and total size of download in bytes
		For import events, no units defined (depends on importers)
	*/
	u32 done, total;
	union {
		/* for download events */
		u32 bytes_per_seconds;
		/* for filter status event, index of filter in session */
		u32 filter_idx;
	};
	
} GF_EventProgress;

/*! Service connection event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_CONNECT*/
	u8 type;
	/*sent upon connection/deconnection completion. if any error, it is signaled through message event*/
	Bool is_connected;
} GF_EventConnect;

/*! Addon connection notification event
	event proc return value: 1 to indicate the compositor should attempt a default layout for this addon, 0: nothing will be done
*/
typedef struct
{
	/*GF_EVENT_ADDON_DETECTED*/
	u8 type;
	const char *addon_url;
	const char *mime_type;
} GF_EventAddonConnect;

/*! Authentication event
	event proc return value: 1 if info has been completed, 0 otherwise (and operation this request was for will then fail)
*/
typedef struct
{
	/*! GF_EVENT_AUTHORIZATION*/
	u8 type;
	/*! set to GF_TRUE if TLS is used*/
	Bool secure;
	/*! the URL the auth request is for*/
	const char *site_url;
	/*! user name (provided buffer can hold 50 bytes). It may already be formatted, or an empty ("") string*/
	char *user;
	/*! password (provided buffer can hold 50 bytes)*/
	char *password;
	/*! async function to call back once pass is entered. If NULL, user/password must be set in the event*/
	void (*on_usr_pass)(void *usr_cbk, const char *usr_name, const char *password, Bool store_info);
	/*! user data for async function*/
	void *async_usr_data;
} GF_EventAuthorize;


/*! System desktop colors and window decoration event
	event proc return value: 1 if info has been completed, 0 otherwise
*/
typedef struct
{
	/*! GF_EVENT_SYS_COLORS*/
	u8 type;
	/*! ARGB colors, in order:
	ActiveBorder, ActiveCaption, AppWorkspace, Background, ButtonFace, ButtonHighlight, ButtonShadow,
	ButtonText, CaptionText, GrayText, Highlight, HighlightText, InactiveBorder, InactiveCaption,
	InactiveCaptionText, InfoBackground, InfoText, Menu, MenuText, Scrollbar, ThreeDDarkShadow,
	ThreeDFace, ThreeDHighlight, ThreeDLightShadow, ThreeDShadow, Window, WindowFrame, WindowText
	*/
	u32 sys_colors[28];
} GF_EventSysColors;

/*! DOM mutation event*/
typedef struct {
	/* GF_EVENT_TREE_MODIFIED, GF_EVENT_NODE_INSERTED, GF_EVENT_NODE_REMOVED, GF_EVENT_NODE_INSERTED_DOC, GF_EVENT_NODE_REMOVED_DOC, GF_EVENT_ATTR_MODIFIED, GF_EVENT_CHAR_DATA_MODIFIED */
	u8 type;
	void *relatedNode;
	void *prevValue;
	void *newValue;
	void *attrName;
	u8 attrChange;
} GF_EventMutation;

/*! Open file notification event*/
typedef struct {
	/* GF_EVENT_DROPFILE*/
	u8 type;
	u32 nb_files;
	char **files;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventOpenFile;

/*! Orientation sensor change event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SENSOR_ORIENTATION*/
	u8 type;
	/* device orientation as quaternion if w is not 0, or as radians otherwise*/
	Float x, y, z, w;
} GF_EventOrientationSensor;

/*! GPS sensor change event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SENSOR_GPS*/
	u8 type;
	/* position in degrees*/
	Double longitude, latitude;
	/* altitude in meters above the WGS84 reference ellipsoid*/
	Double altitude;
	Float accuracy, bearing;
	//ms since unix EPOCH
	u64 timestamp;
} GF_EventGPS;


/*! Orientation sensor activation event
	event proc return value: ignored
*/
typedef struct
{
	/*GF_EVENT_SENSOR_REQUEST*/
	u8 type;
	/*device evt type to activate (eg GF_EVENT_SENSOR_ORIENTATION)*/
	u32 sensor_type;
	Bool activate;
} GF_EventSensorRequest;


/*! Clipboard
	event proc return value: true if text has been set for COPY, ignored otherwise
*/
typedef struct
{
	/*GF_EVENT_MESSAGE*/
	u8 type;
	/*
	- const char * for PASTE_TEXT
	- char * for COPY_TEXT, must be freed by caller
	*/
	char *text;
	/*ID of window the event occured in (driver to app only)*/
	u32 window_id;
} GF_EventClipboard;


/*! Event object*/
typedef union
{
	u8 type;
	GF_EventMouse mouse;
	GF_EventMultiTouch mtouch;
	GF_EventKey key;
	GF_EventChar character;
	GF_EventOrientationSensor sensor;
	GF_EventGPS gps;
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
	GF_EventOpenFile open_file;
	GF_EventAddonConnect addon_connect;
	GF_EventSensorRequest activate_sensor;
	GF_EventClipboard clipboard;
} GF_Event;

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_EVENTS_H_*/

