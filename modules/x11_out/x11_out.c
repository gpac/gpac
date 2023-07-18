/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2020
 *					All rights reserved
 *
 *  This file is part of GPAC /  X11 video output module
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

#include "x11_out.h"
#include <gpac/constants.h>
#include <gpac/utf.h>
#include <sys/time.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <errno.h>

enum
{
	GF_PIXEL_IYUV = GF_4CC('I', 'Y', 'U', 'V'),
	GF_PIXEL_I420 = GF_4CC('I', '4', '2', '0'),
	/*!YUV packed format*/
	GF_PIXEL_UYNV = GF_4CC('U', 'Y', 'N', 'V'),
	/*!YUV packed format*/
	GF_PIXEL_YUNV = GF_4CC('Y', 'U', 'N', 'V'),
	/*!YUV packed format*/
	GF_PIXEL_V422 = GF_4CC('V', '4', '2', '2'),

	GF_PIXEL_YV12 = GF_4CC('Y', 'V', '1', '2'),
	GF_PIXEL_Y422 = GF_4CC('Y', '4', '2', '2'),
	GF_PIXEL_YUY2 = GF_4CC('Y', 'U', 'Y', '2'),
};


void X11_SetupWindow (GF_VideoOutput * vout);

#ifdef GPAC_HAS_OPENGL
PFNGLXSWAPINTERVALEXTPROC my_glXSwapIntervalEXT;
PFNGLXSWAPINTERVALMESAPROC my_glXSwapIntervalMESA;
PFNGLXSWAPINTERVALSGIPROC my_glXSwapIntervalSGI;
#endif

#ifdef GPAC_HAS_X11_XV
static void X11_DestroyOverlay(XWindow *xwin)
{
	if (xwin->overlay) XFree(xwin->overlay);
	xwin->overlay = NULL;
	xwin->xv_pf_format = 0;
	if (xwin->display && (xwin->xvport>=0)) {
		XvUngrabPort(xwin->display, xwin->xvport, CurrentTime );
		xwin->xvport = -1;
	}
}

static Bool is_same_yuv(u32 pf, u32 a_pf)
{
	if (pf==a_pf) return 1;
#if 0
	switch (pf) {
	case GF_PIXEL_YV12:
	case GF_PIXEL_I420:
	case GF_PIXEL_IYUV:
		switch (a_pf) {
		case GF_PIXEL_YV12:
		case GF_PIXEL_I420:
		case GF_PIXEL_IYUV:
			return 1;
		default:
			return 0;
		}
	case GF_PIXEL_UYVY:
	case GF_PIXEL_UYNV:
	case GF_PIXEL_Y422:
		switch (a_pf) {
		case GF_PIXEL_UYVY:
		case GF_PIXEL_UYNV:
		case GF_PIXEL_Y422:
			return 1;
		default:
			return 0;
		}
	case GF_PIXEL_YUY2:
	case GF_PIXEL_YUNV:
		switch (a_pf) {
		case GF_PIXEL_YUY2:
		case GF_PIXEL_YUNV:
			return 1;
		default:
			return 0;
		}
	}
#endif
	return 0;
}
static u32 X11_GetPixelFormat(u32 pf)
{
	return GF_4CC((pf)&0xff, (pf>>8)&0xff, (pf>>16)&0xff, (pf>>24)&0xff);
}
static int X11_GetXVideoPort(GF_VideoOutput *vout, u32 pixel_format, Bool check_color)
{
	XWindow *xwin = (XWindow *)vout->opaque;
	Bool has_color_key = 0;
	XvAdaptorInfo *adaptors;
	unsigned int i;
	unsigned int nb_adaptors;
	int selected_port;

	if (XvQueryExtension(xwin->display, &i, &i, &i, &i, &i ) != Success) return -1;
	if (XvQueryAdaptors(xwin->display, DefaultRootWindow(xwin->display), &nb_adaptors, &adaptors) != Success) return -1;

	selected_port = -1;
	for (i=0; i < nb_adaptors; i++) {
		XvImageFormatValues *formats;
		int j, num_formats, port;

		if( !( adaptors[i].type & XvInputMask ) || !(adaptors[i].type & XvImageMask ) )
			continue;

		/* Check for our format... */
		formats = XvListImageFormats(xwin->display, adaptors[i].base_id, &num_formats);

		for (j=0; j<num_formats && (selected_port == -1 ); j++) {
			XvAttribute *attr=NULL;
			int k, nb_attributes;
			u32 pformat = X11_GetPixelFormat(formats[j].id);

			if( !is_same_yuv(pformat, pixel_format) ) continue;

			/* Grab first port  supporting this format */
			for (port=adaptors[i].base_id; (port < (int)(adaptors[i].base_id + adaptors[i].num_ports) ) && (selected_port == -1); port++) {
				if (port==xwin->xvport) continue;

				attr = XvQueryPortAttributes(xwin->display, port, &nb_attributes);
				for (k=0; k<nb_attributes; k++ ) {
					if (!strcmp(attr[k].name, "XV_COLORKEY")) {
						const Atom ckey = XInternAtom(xwin->display, "XV_COLORKEY", False);
						XvGetPortAttribute(xwin->display, port, ckey, &vout->overlay_color_key);
						has_color_key = 1;
						vout->overlay_color_key |= 0xFF000000;
					}
					/*			        else if (!strcmp(attr[k].name, "XV_AUTOPAINT_COLORKEY")) {
								            const Atom paint = XInternAtom(xwin->display, "XV_AUTOPAINT_COLORKEY", False);
								            XvSetPortAttribute(xwin->display, port, paint, 1);
									}
					*/
				}
				if (attr) {
					free(attr);
					attr=NULL;
				}

				if (check_color && !has_color_key) continue;

				if (XvGrabPort(xwin->display, port, CurrentTime) == Success) {
					selected_port = port;
					xwin->xv_pf_format = formats[j].id;
				}
			}
			if (selected_port == -1 )
				continue;

		}
		if (formats != NULL) XFree(formats);
	}
	if (nb_adaptors > 0)
		XvFreeAdaptorInfo(adaptors);

	return selected_port;
}
static GF_Err X11_InitOverlay(GF_VideoOutput *vout, u32 VideoWidth, u32 VideoHeight)
{
	XWindow *xwin = (XWindow *)vout->opaque;
	if (xwin->overlay && (VideoWidth<=xwin->overlay->width) && (VideoHeight<=xwin->overlay->height)) {
		return GF_OK;
	}

	X11_DestroyOverlay(xwin);

	xwin->xvport = X11_GetXVideoPort(vout, GF_PIXEL_YV12, 0);
	if (xwin->xvport<0)
		xwin->xvport = X11_GetXVideoPort(vout, GF_PIXEL_YUY2, 0);

	if (xwin->xvport<0) {
		return GF_NOT_SUPPORTED;
	}

	/* Create overlay image */
	xwin->overlay = XvCreateImage(xwin->display, xwin->xvport, xwin->xv_pf_format, NULL, VideoWidth, VideoHeight);
	if (!xwin->overlay) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Xv Overlay Creation Failure\n"));
		return GF_IO_ERR;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Overlay init %d x %d - pixel format %s - XV port %d\n",
	                                  VideoWidth, VideoHeight,
	                                  gf_4cc_to_str(vout->yuv_pixel_format), xwin->xvport ));

	return GF_OK;
}

GF_Err X11_Blit(struct _video_out *vout, GF_VideoSurface *video_src, GF_Window *src, GF_Window *dest, u32 overlay_type)
{
	XvImage *overlay;
	int xvport;
	Drawable dst_dr;
	GF_Err e;
	Window cur_wnd;
	XWindow *xwin = (XWindow *)vout->opaque;

	if (!video_src) {
		if (overlay_type && xwin->xvport) {
		}
		return GF_OK;
	}

	if (video_src->pixel_format != GF_PIXEL_YV12) return GF_NOT_SUPPORTED;
	cur_wnd = xwin->fullscreen ? xwin->full_wnd : xwin->wnd;
	/*init if needed*/
	if ((xwin->xvport<0) || !xwin->overlay) {
		e = X11_InitOverlay(vout, video_src->width, video_src->height);
		if (e) return e;
		if (!xwin->overlay) return GF_IO_ERR;
	}

	/*different size, recreate an image*/
	if (xwin->overlay && ((xwin->overlay->width != video_src->width) || (xwin->overlay->height != video_src->height))) {
		XFree(xwin->overlay);
		xwin->overlay = XvCreateImage(xwin->display, xwin->xvport, xwin->xv_pf_format, NULL, video_src->width, video_src->height);
		if (!xwin->overlay) return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[X11] Blit surface to dest %d x %d - overlay type %s\n", dest->w, dest->h,
	                                   (overlay_type==0)? "none" : ((overlay_type==1) ? "Top-Level" : "ColorKey")
	                                  ));

	overlay = xwin->overlay;
	xvport = xwin->xvport;

	overlay->data = video_src->video_buffer;

	overlay->num_planes = 3;
	overlay->pitches[0] = video_src->width;
	overlay->pitches[1] = xwin->overlay->pitches[2] = video_src->width/2;
	overlay->offsets[0] = 0;
	overlay->offsets[1] = video_src->width*video_src->height;
	overlay->offsets[2] = 5*video_src->width*video_src->height/4;

	dst_dr = cur_wnd;
	if (!overlay_type) {
		if (!xwin->pixmap) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Back buffer not configured for Blt\n"));
			return GF_BAD_PARAM;
		}
		dst_dr = xwin->pixmap;
	}
	XvPutImage(xwin->display, xvport, dst_dr, xwin->the_gc, overlay,
	           src->x, src->y, src->w, src->h,
	           dest->x, dest->y, dest->w, dest->h);

	return GF_OK;
}
#endif


/*Flush video */
GF_Err X11_Flush(struct _video_out *vout, GF_Window * dest)
{
	Window cur_wnd;
	/*
	 * write backbuffer to screen
	 */
	X11VID ();

	if (!xWindow->is_init) return GF_OK;
	cur_wnd = xWindow->fullscreen ? xWindow->full_wnd : xWindow->wnd;

	if (xWindow->output_3d) {
#ifdef GPAC_HAS_OPENGL
		XSync(xWindow->display, False);
		glFlush();
		glXSwapBuffers(xWindow->display, cur_wnd);
#endif
		return GF_OK;
	}

#ifdef GPAC_HAS_X11_SHM
	if (xWindow->pixmap) {
		XClearWindow(xWindow->display, cur_wnd);
		XSync(xWindow->display, False);
	} else if (xWindow->use_shared_memory) {
		XSync(xWindow->display, False);
		XShmPutImage (xWindow->display, cur_wnd, xWindow->the_gc, xWindow->surface,
		              0, 0, dest->x, dest->y, dest->w, dest->h, True);
	} else
#endif
	{
		XSync(xWindow->display, False);
		//XRaiseWindow(xWindow->display, xWindow->wnd);
		XPutImage (xWindow->display, cur_wnd, xWindow->the_gc, xWindow->surface,
		           0, 0, dest->x, dest->y, dest->w, dest->h);
	}
	return GF_OK;
}

typedef struct
{
	u32 x11_key;
	u32 gf_key;
	u32 gf_flags;
} X11KeyToGPAC;

static X11KeyToGPAC X11Keys[] =
{
	{XK_BackSpace, GF_KEY_BACKSPACE, 0},
	{XK_Tab, GF_KEY_TAB, 0},
//	{XK_Linefeed, GF_KEY_LINEFEED, 0},
	{XK_Clear, GF_KEY_CLEAR, 0},
	{XK_KP_Enter, GF_KEY_ENTER, GF_KEY_EXT_NUMPAD},
	{XK_Return, GF_KEY_ENTER, 0},
	{XK_Pause, GF_KEY_PAUSE, 0},
	{XK_Caps_Lock, GF_KEY_CAPSLOCK, 0},
	{XK_Scroll_Lock, GF_KEY_SCROLL, 0},
	{XK_Escape, GF_KEY_ESCAPE, 0},
	{XK_Delete, GF_KEY_DEL, 0},
	{XK_Kanji, GF_KEY_KANJIMODE, 0},
	{XK_Katakana, GF_KEY_JAPANESEKATAKANA, 0},
	{XK_Romaji, GF_KEY_JAPANESEROMAJI, 0},
	{XK_Hiragana, GF_KEY_JAPANESEHIRAGANA, 0},
	{XK_Kana_Lock, GF_KEY_KANAMODE, 0},
	{XK_Home, GF_KEY_HOME, 0},
	{XK_Left, GF_KEY_LEFT, 0},
	{XK_Up, GF_KEY_UP, 0},
	{XK_Right, GF_KEY_RIGHT, 0},
	{XK_Down, GF_KEY_DOWN, 0},
	{XK_Page_Up, GF_KEY_PAGEUP, 0},
	{XK_Page_Down, GF_KEY_PAGEDOWN, 0},
	{XK_End, GF_KEY_END, 0},
	//{XK_Begin, GF_KEY_BEGIN, 0},
	{XK_Select, GF_KEY_SELECT, 0},
	{XK_Print, GF_KEY_PRINTSCREEN, 0},
	{XK_Execute, GF_KEY_EXECUTE, 0},
	{XK_Insert, GF_KEY_INSERT, 0},
	{XK_Undo, GF_KEY_UNDO, 0},
	//{XK_Redo, GF_KEY_BEGIN, 0},
	//{XK_Menu, GF_KEY_BEGIN, 0},
	{XK_Find, GF_KEY_FIND, 0},
	{XK_Cancel, GF_KEY_CANCEL, 0},
	{XK_Help, GF_KEY_HELP, 0},
	//{XK_Break, GF_KEY_BREAK, 0},
	//{XK_Mode_switch, GF_KEY_BEGIN, 0},
	{XK_Num_Lock, GF_KEY_NUMLOCK, 0},
	{XK_F1, GF_KEY_F1, 0},
	{XK_F2, GF_KEY_F2, 0},
	{XK_F3, GF_KEY_F3, 0},
	{XK_F4, GF_KEY_F4, 0},
	{XK_F5, GF_KEY_F5, 0},
	{XK_F6, GF_KEY_F6, 0},
	{XK_F7, GF_KEY_F7, 0},
	{XK_F8, GF_KEY_F8, 0},
	{XK_F9, GF_KEY_F9, 0},
	{XK_F10, GF_KEY_F10, 0},
	{XK_F11, GF_KEY_F11, 0},
	{XK_F12, GF_KEY_F12, 0},
	{XK_F13, GF_KEY_F13, 0},
	{XK_F14, GF_KEY_F14, 0},
	{XK_F15, GF_KEY_F15, 0},
	{XK_F16, GF_KEY_F16, 0},
	{XK_F17, GF_KEY_F17, 0},
	{XK_F18, GF_KEY_F18, 0},
	{XK_F19, GF_KEY_F19, 0},
	{XK_F20, GF_KEY_F20, 0},
	{XK_F21, GF_KEY_F21, 0},
	{XK_F22, GF_KEY_F22, 0},
	{XK_F23, GF_KEY_F23, 0},
	{XK_F24, GF_KEY_F24, 0},
	{XK_KP_Delete, GF_KEY_COMMA, GF_KEY_EXT_NUMPAD},
	{XK_KP_Decimal, GF_KEY_COMMA, GF_KEY_EXT_NUMPAD},
	{XK_KP_Insert, GF_KEY_0, GF_KEY_EXT_NUMPAD},
	{XK_KP_0, GF_KEY_0, GF_KEY_EXT_NUMPAD},
	{XK_KP_End, GF_KEY_1, GF_KEY_EXT_NUMPAD},
	{XK_KP_1, GF_KEY_1, GF_KEY_EXT_NUMPAD},
	{XK_KP_Down, GF_KEY_2, GF_KEY_EXT_NUMPAD},
	{XK_KP_2, GF_KEY_2, GF_KEY_EXT_NUMPAD},
	{XK_KP_Page_Down, GF_KEY_3, GF_KEY_EXT_NUMPAD},
	{XK_KP_3, GF_KEY_3, GF_KEY_EXT_NUMPAD},
	{XK_KP_Left, GF_KEY_4, GF_KEY_EXT_NUMPAD},
	{XK_KP_4, GF_KEY_4, GF_KEY_EXT_NUMPAD},
	{XK_KP_Begin, GF_KEY_5, GF_KEY_EXT_NUMPAD},
	{XK_KP_5, GF_KEY_5, GF_KEY_EXT_NUMPAD},
	{XK_KP_Right, GF_KEY_6, GF_KEY_EXT_NUMPAD},
	{XK_KP_6, GF_KEY_6, GF_KEY_EXT_NUMPAD},
	{XK_KP_Home, GF_KEY_7, GF_KEY_EXT_NUMPAD},
	{XK_KP_7, GF_KEY_7, GF_KEY_EXT_NUMPAD},
	{XK_KP_Up, GF_KEY_8, GF_KEY_EXT_NUMPAD},
	{XK_KP_8, GF_KEY_8, GF_KEY_EXT_NUMPAD},
	{XK_KP_Page_Up, GF_KEY_9, GF_KEY_EXT_NUMPAD},
	{XK_KP_9, GF_KEY_9, GF_KEY_EXT_NUMPAD},
	{XK_KP_Add, GF_KEY_PLUS, GF_KEY_EXT_NUMPAD},
	{XK_KP_Subtract, GF_KEY_HYPHEN, GF_KEY_EXT_NUMPAD},
	{XK_KP_Multiply, GF_KEY_STAR, GF_KEY_EXT_NUMPAD},
	{XK_KP_Divide, GF_KEY_SLASH, GF_KEY_EXT_NUMPAD},
	{XK_Shift_R, GF_KEY_EXT_RIGHT, 0},
	{XK_Shift_L, GF_KEY_SHIFT, 0},
	{XK_Control_R, GF_KEY_EXT_RIGHT, 0},
	{XK_Control_L, GF_KEY_CONTROL, 0},
	{XK_Alt_R, GF_KEY_EXT_RIGHT, 0},
	{XK_Alt_L, GF_KEY_ALT, 0},
	{XK_Super_R, GF_KEY_EXT_RIGHT, 0},
	{XK_Super_L, GF_KEY_WIN, 0},
	{XK_Menu, GF_KEY_META, 0},
#ifdef XK_XKB_KEYS
	{XK_ISO_Level3_Shift, GF_KEY_ALTGRAPH, 0},
#endif
	{'!', GF_KEY_EXCLAMATION, 0},
	{'"', GF_KEY_QUOTATION, 0},
	{'#', GF_KEY_NUMBER, 0},
	{'$', GF_KEY_DOLLAR, 0},
	{'&', GF_KEY_AMPERSAND, 0},
	{'\'', GF_KEY_APOSTROPHE, 0},
	{'(', GF_KEY_LEFTPARENTHESIS, 0},
	{')', GF_KEY_RIGHTPARENTHESIS, 0},
	{',', GF_KEY_COMMA, 0},
	{':', GF_KEY_COLON, 0},
	{';', GF_KEY_SEMICOLON, 0},
	{'<', GF_KEY_LESSTHAN, 0},
	{'>', GF_KEY_GREATERTHAN, 0},
	{'?', GF_KEY_QUESTION, 0},
	{'@', GF_KEY_AT, 0},
	{'[', GF_KEY_LEFTSQUAREBRACKET, 0},
	{']', GF_KEY_RIGHTSQUAREBRACKET, 0},
	{'\\', GF_KEY_BACKSLASH, 0},
	{'_', GF_KEY_UNDERSCORE, 0},
	{'`', GF_KEY_GRAVEACCENT, 0},
	{' ', GF_KEY_SPACE, 0},
	{'/', GF_KEY_SLASH, 0},
	{'*', GF_KEY_STAR, 0},
	{'-', GF_KEY_HYPHEN, 0},
	{'+', GF_KEY_PLUS, 0},
	{'=', GF_KEY_EQUALS, 0},
	{'^', GF_KEY_CIRCUM, 0},
	{'{', GF_KEY_LEFTCURLYBRACKET, 0},
	{'}', GF_KEY_RIGHTCURLYBRACKET, 0},
	{'|', GF_KEY_PIPE, 0},
};

static u32 num_x11_keys = sizeof(X11Keys) / sizeof(X11KeyToGPAC);

/*
 * Translate X_Key to GF_Key
 */
//=====================================

static void x11_translate_key(u32 X11Key, GF_EventKey *evt)
{
	u32 i;

	evt->flags = 0;
	evt->hw_code = X11Key & 0xFF;
	for (i=0; i<num_x11_keys; i++) {
		if (X11Key == X11Keys[i].x11_key) {
			evt->key_code = X11Keys[i].gf_key;
			evt->flags = X11Keys[i].gf_flags;
			return;
		}
	}

	if ((X11Key>='0') && (X11Key<='9'))
		evt->key_code = GF_KEY_0 + X11Key - '0';
	else if ((X11Key>='A') && (X11Key<='Z'))
		evt->key_code = GF_KEY_A + X11Key - 'A';
	/*DOM3: translate to A -> Z, not a -> z*/
	else if ((X11Key>='a') && (X11Key<='z'))  {
		evt->key_code = GF_KEY_A + X11Key - 'a';
		evt->hw_code = X11Key - 'a' + 'A';
	} else {
		evt->key_code = GF_KEY_UNIDENTIFIED;
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[X11] Unrecognized key %X\n", X11Key));
	}
}



/* taken from SDL
 Ack!  XPending() actually performs a blocking read if no events available
*/
static int X11_Pending(Display *display)
{
	/* Flush the display connection and look to see if events are queued */
	XFlush(display);
	if ( XEventsQueued(display, QueuedAlready) ) return(1);

	/* More drastic measures are required -- see if X is ready to talk */
	{
		static struct timeval zero_time;	/* static == 0 */
		int x11_fd;
		fd_set fdset;
		x11_fd = ConnectionNumber(display);
		FD_ZERO(&fdset);
		FD_SET(x11_fd, &fdset);
		if ( select(x11_fd+1, &fdset, NULL, NULL, &zero_time) == 1 ) {
			return(XPending(display));
		}
	}
	/* Oh well, nothing is ready .. */
	return(0);
}

/*
 * handle X11 events
 * here we handle key, mouse, repaint and window sizing events
 */
static void X11_HandleEvents(GF_VideoOutput *vout)
{
	GF_Event evt;
	Window the_window;
	XComposeStatus state;
	X11VID();
	unsigned char keybuf[32];
	XEvent xevent;
	if (!xWindow->display) return;
	the_window = xWindow->fullscreen ? xWindow->full_wnd : xWindow->wnd;
	XSync(xWindow->display, False);

	while (X11_Pending(xWindow->display)) {
		XNextEvent(xWindow->display, &xevent);
		if (xevent.xany.window!=the_window) continue;
		switch (xevent.type) {
		/*
		 * X11 window resized event
		 * must inform GPAC
		 */
		case ConfigureNotify:
			if ((unsigned int) xevent.xconfigure.width != xWindow->w_width
			        || (unsigned int) xevent.xconfigure.height != xWindow->w_height)
			{
				evt.type = GF_EVENT_SIZE;
				xWindow->w_width = evt.size.width = xevent.xconfigure.width;
				xWindow->w_height = evt.size.height = xevent.xconfigure.height;
				vout->on_event(vout->evt_cbk_hdl, &evt);
			} else {
				evt.type = GF_EVENT_MOVE;
				evt.move.x = xevent.xconfigure.x;
				evt.move.y = xevent.xconfigure.y;
				vout->on_event(vout->evt_cbk_hdl, &evt);
			}
			break;
		/*
		 * Windows need repaint
		 */
		case Expose:
			if (xevent.xexpose.count > 0)	break;
			evt.type = GF_EVENT_REFRESH;
			vout->on_event (vout->evt_cbk_hdl, &evt);
			break;

		/* Have we been requested to quit (or another client message?) */
		case ClientMessage:
			if ( (xevent.xclient.format == 32) && (xevent.xclient.data.l[0] == xWindow->WM_DELETE_WINDOW) ) {
				memset(&evt, 0, sizeof(GF_Event));
				evt.type = GF_EVENT_QUIT;
				vout->on_event(vout->evt_cbk_hdl, &evt);
			}
			break;

		case KeyPress:
		case KeyRelease:
			x11_translate_key(XKeycodeToKeysym (xWindow->display, xevent.xkey.keycode, 0), &evt.key);
			evt.type = (xevent.type ==KeyPress) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			vout->on_event (vout->evt_cbk_hdl, &evt);

			if (xevent.type ==KeyPress) {
				s32 len;
				len = XLookupString (&xevent.xkey, (char *) keybuf, sizeof(keybuf), NULL, &state);
				if ((len>0) && (len<5)) {
					utf8_to_ucs4 (& evt.character.unicode_char, len, keybuf);
					evt.type = GF_EVENT_TEXTINPUT;
					vout->on_event (vout->evt_cbk_hdl, &evt);
				}
			}

			if (evt.key.key_code==GF_KEY_CONTROL) xWindow->ctrl_down = (xevent.type==KeyPress) ? 1 : 0;
			else if (evt.key.key_code==GF_KEY_ALT) xWindow->alt_down = (xevent.type==KeyPress) ? 1 : 0;
			else if (evt.key.key_code==GF_KEY_META) xWindow->meta_down = (xevent.type==KeyPress) ? 1 : 0;

			if ((evt.type==GF_EVENT_KEYUP) && (evt.key.key_code==GF_KEY_V) && xWindow->ctrl_down) {
				int sformat;
				unsigned long nb_bytes, overflow;
				unsigned char *data;
				Window owner;
				Atom selection, stype;
				Atom clipb_atom = XInternAtom(xWindow->display, "CLIPBOARD", 0);
				if (clipb_atom == None) break;
				owner = XGetSelectionOwner(xWindow->display, clipb_atom);
				if ((owner == None) || (owner == the_window)) {
					owner = DefaultRootWindow(xWindow->display);
					selection = XA_CUT_BUFFER0;
				} else {
					owner = the_window;
					selection = XInternAtom(xWindow->display, "GPAC_TEXT_SEL", False);
					XConvertSelection(xWindow->display, clipb_atom, XA_STRING, selection, owner, CurrentTime);
				}


				if (XGetWindowProperty(xWindow->display, owner, selection, 0, INT_MAX/4, False, AnyPropertyType, &stype, &sformat, &nb_bytes, &overflow, &data) == Success) {
					if (stype == XA_STRING) {
						char *text = gf_malloc(sizeof(char)*(nb_bytes+1));
						if (text) {
							memcpy(text, data, nb_bytes);
							text[nb_bytes] = 0;
							evt.type = GF_EVENT_PASTE_TEXT;
							evt.clipboard.text = text;
							vout->on_event(vout->evt_cbk_hdl, &evt);
							gf_free(text);
						}
					}
					XFree(data);
				}
			}
			else if ((evt.type==GF_EVENT_KEYUP) && (evt.key.key_code==GF_KEY_C) && xWindow->ctrl_down) {
				Atom clipb_atom = XInternAtom(xWindow->display, "CLIPBOARD", 0);
				evt.type = GF_EVENT_COPY_TEXT;
				if (vout->on_event(vout->evt_cbk_hdl, &evt)==GF_TRUE) {
					if (evt.clipboard.text) {
						XChangeProperty(xWindow->display, DefaultRootWindow(xWindow->display), XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace, (const unsigned char *)evt.clipboard.text, strlen(evt.clipboard.text));
						gf_free(evt.clipboard.text);
					}

					if ((clipb_atom != None) && XGetSelectionOwner(xWindow->display, clipb_atom) != the_window) {
						XSetSelectionOwner(xWindow->display, clipb_atom, the_window, CurrentTime);
					}
					if (XGetSelectionOwner(xWindow->display, XA_PRIMARY) != the_window) {
						XSetSelectionOwner(xWindow->display, XA_PRIMARY, the_window, CurrentTime);
					}
				}
			}
			break;

		case ButtonPress:
			if (!xWindow->fullscreen && !xWindow->has_focus) {
				xWindow->has_focus = 1;
				XSetInputFocus(xWindow->display, xWindow->wnd, RevertToParent, CurrentTime);
			}
		case ButtonRelease:
			//				last_mouse_move = xevent.xbutton.time;
			evt.mouse.x = xevent.xbutton.x;
			evt.mouse.y = xevent.xbutton.y;
			evt.type = (xevent.type == ButtonRelease) ? GF_EVENT_MOUSEUP : GF_EVENT_MOUSEDOWN;

			switch (xevent.xbutton.button) {
			case Button1:
				evt.mouse.button = GF_MOUSE_LEFT;
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
			case Button2:
				evt.mouse.button = GF_MOUSE_MIDDLE;
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
			case Button3:
				evt.mouse.button = GF_MOUSE_RIGHT;
				vout->on_event (vout->evt_cbk_hdl, &evt);
				break;
			case Button4:
				evt.type = GF_EVENT_MOUSEWHEEL;
				evt.mouse.wheel_pos = FIX_ONE;
				vout->on_event(vout->evt_cbk_hdl, &evt);
				break;
			case Button5:
				evt.type = GF_EVENT_MOUSEWHEEL;
				evt.mouse.wheel_pos = -FIX_ONE;
				vout->on_event(vout->evt_cbk_hdl, &evt);
				break;
			}
			if (!xWindow->fullscreen && (xevent.type==ButtonPress) )
				XSetInputFocus(xWindow->display, xWindow->wnd, RevertToNone, CurrentTime);
			break;

		case MotionNotify:
			evt.type = GF_EVENT_MOUSEMOVE;
			evt.mouse.x = xevent.xmotion.x;
			evt.mouse.y = xevent.xmotion.y;
			vout->on_event (vout->evt_cbk_hdl, &evt);
			break;
		case PropertyNotify:
			break;
		case MapNotify:
			break;
		case CirculateNotify:
			break;
		case UnmapNotify:
			break;
		case ReparentNotify:
			break;
		case FocusOut:
			if (!xWindow->fullscreen) xWindow->has_focus = 0;
			break;
		case FocusIn:
			if (!xWindow->fullscreen) xWindow->has_focus = 1;
			break;

		case DestroyNotify:
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			vout->on_event(vout->evt_cbk_hdl, &evt);
			break;

		default:
			break;
		}
	}

}



#ifdef GPAC_HAS_OPENGL
static GF_Err X11_SetupGL(GF_VideoOutput *vout)
{
	GF_Event evt;
	XWindow *xWin = (XWindow *)vout->opaque;

	if (!xWin->glx_visualinfo) return GF_IO_ERR;
	memset(&evt, 0, sizeof(GF_Event));

	if (!xWin->glx_context) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[X11] Setting up GL for display %d\n", xWin->display));
		XSync(xWin->display, False);
		xWin->glx_context = glXCreateContext(xWin->display,xWin->glx_visualinfo, NULL, True);
		XSync(xWin->display, False);
		if (!xWin->glx_context) return GF_IO_ERR;

		evt.setup.hw_reset = 1;
	}
	if ( ! glXMakeCurrent(xWin->display, xWin->fullscreen ? xWin->full_wnd : xWin->wnd, xWin->glx_context) ) return GF_IO_ERR;

#ifdef GPAC_HAS_OPENGL
	if (gf_module_get_bool((GF_BaseInterface*)vout, "no-vsync")) {
		my_glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalEXT");
		if (my_glXSwapIntervalEXT != NULL) {
			my_glXSwapIntervalEXT(xWin->display, xWin->wnd, 0);
		} else {
			my_glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalMESA");
			if (my_glXSwapIntervalMESA != NULL ) {
				my_glXSwapIntervalMESA(0);
			} else {
				my_glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalSGI");
				if (my_glXSwapIntervalSGI != NULL ) {
					my_glXSwapIntervalSGI(0);
				}
			}
		}
	}
#endif
	XSync(xWin->display, False);

	evt.type = GF_EVENT_VIDEO_SETUP;
	vout->on_event (vout->evt_cbk_hdl,&evt);
	xWin->is_init = 1;
	return GF_OK;
}

static void X11_ReleaseGL(XWindow *xWin)
{
	XSync(xWin->display, False);
	if (xWin->glx_context) {
		glXMakeCurrent(xWin->display, None, NULL);
		glXDestroyContext(xWin->display, xWin->glx_context);
		xWin->glx_context = NULL;
	}
	xWin->is_init = 0;
	XSync(xWin->display, False);
}

#endif


static void X11_ReleaseBackBuffer (GF_VideoOutput * vout)
{
	X11VID ();
	if (xWindow->x_data) {
		gf_free(xWindow->x_data);
		xWindow->x_data = NULL;
		if (xWindow->surface) xWindow->surface->data = NULL;
	}
#ifdef GPAC_HAS_X11_SHM
	if (xWindow->shmseginfo) XShmDetach (xWindow->display, xWindow->shmseginfo);
	if (xWindow->pixmap) {
		XFreePixmap(xWindow->display, xWindow->pixmap);
		xWindow->pixmap = 0L;
		xWindow->pwidth = xWindow->pheight = 0;
	} else {
		if (xWindow->surface) XDestroyImage(xWindow->surface);
		xWindow->surface = NULL;
	}
	if (xWindow->shmseginfo) {
		if (xWindow->shmseginfo->shmaddr) shmdt(xWindow->shmseginfo->shmaddr);
		if (xWindow->shmseginfo->shmid >= 0)
			shmctl (xWindow->shmseginfo->shmid, IPC_RMID, NULL);
		gf_free(xWindow->shmseginfo);
		xWindow->shmseginfo = NULL;
	}
#endif
	if (xWindow->surface) {
		XFree(xWindow->surface);
		xWindow->surface = NULL;
	}
	xWindow->is_init = 0;
#ifdef GPAC_HAS_X11_XV
	X11_DestroyOverlay(xWindow);
#endif

}

/*
 * init backbuffer
 */
GF_Err X11_InitBackBuffer (GF_VideoOutput * vout, u32 VideoWidth, u32 VideoHeight)
{
#ifdef GPAC_HAS_X11_SHM
	Window cur_wnd;
#endif
	u32 size;
	VideoWidth = VideoWidth > 32 ? VideoWidth : 32;
	VideoWidth = VideoWidth < 4096 ? VideoWidth : 4096;
	VideoHeight = VideoHeight > 32 ? VideoHeight : 32;
	VideoHeight = VideoHeight > 4096 ? 4096 : VideoHeight;

	X11VID ();
	if (!xWindow || !VideoWidth || !VideoHeight)
		return GF_BAD_PARAM;

	X11_ReleaseBackBuffer(vout);
	/*WATCHOUT seems we need even width when using shared memory extensions*/
	if (xWindow->use_shared_memory && (VideoWidth%2))
		VideoWidth++;

	size = VideoWidth * VideoHeight * xWindow->bpp;

#ifdef GPAC_HAS_X11_SHM
	cur_wnd = xWindow->fullscreen ? xWindow->full_wnd : xWindow->wnd;
	/*if we're using YUV blit to offscreen, we must use a pixmap*/
	if (vout->hw_caps & GF_VIDEO_HW_HAS_YUV) {
		GF_SAFEALLOC(xWindow->shmseginfo, XShmSegmentInfo);
		xWindow->shmseginfo->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0776);
		xWindow->shmseginfo->shmaddr = shmat(xWindow->shmseginfo->shmid, 0, 0);
		xWindow->shmseginfo->readOnly = False;
		if (!XShmAttach (xWindow->display, xWindow->shmseginfo)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Failed to attach shared memory!\n"));
		}
		xWindow->pixmap = XShmCreatePixmap(xWindow->display, cur_wnd,
		                                   (unsigned char *) xWindow->shmseginfo->shmaddr, xWindow->shmseginfo,
		                                   VideoWidth, VideoHeight, xWindow->depth);
		memset((unsigned char *) xWindow->shmseginfo->shmaddr, 0, sizeof(char)*size);
		XSetWindowBackgroundPixmap (xWindow->display, cur_wnd, xWindow->pixmap);
		xWindow->pwidth = VideoWidth;
		xWindow->pheight = VideoHeight;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[X11] Using X11 Pixmap %08x\n", (u32)xWindow->pixmap));
	} else if (xWindow->use_shared_memory) {
		GF_SAFEALLOC(xWindow->shmseginfo, XShmSegmentInfo);
		xWindow->surface = XShmCreateImage (xWindow->display, xWindow->visual,
		                                    xWindow->depth, ZPixmap, NULL, xWindow->shmseginfo,
		                                    VideoWidth, VideoHeight);
		xWindow->shmseginfo->shmid = shmget(IPC_PRIVATE, xWindow->surface->bytes_per_line * xWindow->surface->height,
		                                    IPC_CREAT | 0777);

		xWindow->surface->data = xWindow->shmseginfo->shmaddr = shmat(xWindow->shmseginfo->shmid, NULL, 0);
		xWindow->shmseginfo->readOnly = False;
		XShmAttach (xWindow->display, xWindow->shmseginfo);
	} else
#endif
	{
		xWindow->x_data = (char *) gf_malloc(sizeof(char)*size);
		xWindow->surface = XCreateImage (xWindow->display, xWindow->visual,
		                                 xWindow->depth, ZPixmap,
		                                 0,
		                                 xWindow->x_data,
		                                 VideoWidth, VideoHeight,
		                                 xWindow->bpp*8, xWindow->bpp*VideoWidth);
		if (!xWindow->surface) {
			return GF_IO_ERR;
		}

	}
	xWindow->is_init = 1;
	return GF_OK;
}

/*
 * resize buffer: note we don't resize window here
 */
GF_Err X11_ResizeBackBuffer (struct _video_out *vout, u32 newWidth, u32 newHeight)
{
	X11VID ();
	u32 w = xWindow->surface ? xWindow->surface->width : xWindow->pwidth;
	u32 h = xWindow->surface ? xWindow->surface->height : xWindow->pheight;
	if (!xWindow->is_init || (newWidth != w) || (newHeight != h)) {
		if ((newWidth >= 32) && (newHeight >= 32))
			return X11_InitBackBuffer (vout, newWidth, newHeight);
	}
	return GF_OK;
}

GF_Err X11_ProcessEvent (struct _video_out * vout, GF_Event * evt)
{
	X11VID ();

	X11_SetupWindow(vout);
	if (!xWindow->display) return GF_IO_ERR;

	if (evt) {
		switch (evt->type) {
		case GF_EVENT_SET_CURSOR:
			break;
		case GF_EVENT_SET_CAPTION:
			if (!xWindow->par_wnd && xWindow->wnd && evt->caption.caption) {
				XStoreName(xWindow->display, xWindow->wnd, "");
			}
			break;
		case GF_EVENT_SHOWHIDE:
			break;
		case GF_EVENT_MOVE:
			if (xWindow->fullscreen) return GF_OK;

			if (evt->move.relative == 2) {
			}
			else if (evt->move.relative) {
				s32 x, y;
				Window child;
				x = y = 0;
				XTranslateCoordinates(xWindow->display, xWindow->wnd, RootWindowOfScreen (xWindow->screenptr), 0, 0, &x, &y, &child );
				XMoveWindow(xWindow->display, xWindow->wnd, x + evt->move.x, y + evt->move.y);
			} else {
				XMoveWindow(xWindow->display, xWindow->wnd, evt->move.x, evt->move.y);
			}
			break;
		case GF_EVENT_SIZE:
			/*if owning the window and not in fullscreen, resize it (initial scene size)*/
			xWindow->w_width = evt->size.width;
			xWindow->w_height = evt->size.height;

			if (!xWindow->fullscreen) {
				if (xWindow->par_wnd) {
					XWindowAttributes pwa;
					XGetWindowAttributes(xWindow->display, xWindow->par_wnd, &pwa);
					XMoveResizeWindow(xWindow->display, xWindow->wnd, pwa.x, pwa.y, evt->size.width, evt->size.height);
					if (!xWindow->no_select_input) XSetInputFocus(xWindow->display, xWindow->wnd, RevertToNone, CurrentTime);
				} else {
					XResizeWindow (xWindow->display, xWindow->wnd, evt->size.width, evt->size.height);
				}
			}
			break;
		case GF_EVENT_VIDEO_SETUP:
			if (evt->setup.use_opengl) {
#ifdef GPAC_HAS_OPENGL
				xWindow->output_3d = GF_TRUE;
				return X11_SetupGL(vout);
#else
				return GF_NOT_SUPPORTED;
#endif
			} else {
				xWindow->output_3d = GF_FALSE;
				return X11_ResizeBackBuffer(vout, evt->setup.width, evt->setup.height);
			}
			break;
		case GF_EVENT_SET_GL:
			if (!xWindow->output_3d) return GF_OK;

#ifdef GPAC_HAS_OPENGL
			if ( ! glXMakeCurrent(xWindow->display, xWindow->fullscreen ? xWindow->full_wnd : xWindow->wnd, xWindow->glx_context) ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Cannot make context current\n"));
				return GF_IO_ERR;
			}
#else
			return GF_NOT_SUPPORTED;
#endif
			break;
		}
	} else {
		X11_HandleEvents(vout);
	}
	return GF_OK;

}

/* switch from/to full screen mode */
GF_Err X11_SetFullScreen (struct _video_out * vout, u32 bFullScreenOn, u32 * screen_width, u32 * screen_height)
{
	X11VID ();
	xWindow->fullscreen = bFullScreenOn;
#ifdef GPAC_HAS_OPENGL
//	if (xWindow->output_3d) X11_ReleaseGL(xWindow);
#endif

	if (bFullScreenOn) {
		xWindow->store_width = *screen_width;
		xWindow->store_height = *screen_height;

		xWindow->w_width = vout->max_screen_width;
		xWindow->w_height = vout->max_screen_height;

		XFreeGC (xWindow->display, xWindow->the_gc);
		xWindow->the_gc = XCreateGC (xWindow->display, xWindow->full_wnd, 0, NULL);

		XMoveResizeWindow (xWindow->display,
		                   (Window) xWindow->full_wnd, 0, 0,
		                   vout->max_screen_width,
		                   vout->max_screen_height);
		*screen_width = xWindow->w_width;
		*screen_height = xWindow->w_height;
		XUnmapWindow (xWindow->display, xWindow->wnd);
		XMapWindow (xWindow->display, xWindow->full_wnd);
		XSetInputFocus(xWindow->display, xWindow->full_wnd, RevertToNone, CurrentTime);
		XRaiseWindow(xWindow->display, xWindow->full_wnd);
		XGrabKeyboard(xWindow->display, xWindow->full_wnd, True, GrabModeAsync, GrabModeAsync, CurrentTime);

		GF_Event evt;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SIZE;
		evt.size.width = xWindow->w_width;
		evt.size.height = xWindow->w_height;
		vout->on_event(vout->evt_cbk_hdl, &evt);
	} else {
		*screen_width = xWindow->store_width;
		*screen_height = xWindow->store_height;
		XFreeGC (xWindow->display, xWindow->the_gc);
		xWindow->the_gc = XCreateGC (xWindow->display, xWindow->wnd, 0, NULL);
		XUnmapWindow (xWindow->display, xWindow->full_wnd);
		XMapWindow (xWindow->display, xWindow->wnd);
		XUngrabKeyboard(xWindow->display, CurrentTime);
		//if (xWindow->par_wnd) XSetInputFocus(xWindow->display, xWindow->wnd, RevertToNone, CurrentTime);

		GF_Event evt;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SIZE;
		evt.size.width = xWindow->w_width;
		evt.size.height = xWindow->w_height;
		vout->on_event(vout->evt_cbk_hdl, &evt);

		/*backbuffer resize will be done right after this is called */
	}
#ifdef GPAC_HAS_OPENGL
	if (xWindow->output_3d) X11_SetupGL(vout);
#endif
	return GF_OK;
}

/*
 * lock video mem
 */
GF_Err X11_LockBackBuffer(struct _video_out * vout, GF_VideoSurface * vi, u32 do_lock)
{
	X11VID ();

	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		memset(vi, 0, sizeof(GF_VideoSurface));
		if (xWindow->surface) {
			vi->width = xWindow->surface->width;
			vi->height = xWindow->surface->height;
			vi->pitch_x = xWindow->bpp;
			vi->pitch_y = xWindow->surface->width*xWindow->bpp;
			vi->pixel_format = xWindow->pixel_format;
			vi->video_buffer = xWindow->surface->data;
		} else {
#ifdef GPAC_HAS_X11_SHM
			vi->width = xWindow->pwidth;
			vi->height = xWindow->pheight;
			vi->pitch_x = xWindow->bpp;
			vi->pitch_y = xWindow->pwidth*xWindow->bpp;
			vi->pixel_format = xWindow->pixel_format;
			vi->video_buffer = (unsigned char *) xWindow->shmseginfo->shmaddr;
#endif
		}
		vi->is_hardware_memory = (xWindow->use_shared_memory) ? 1 : 0;
		return GF_OK;
	} else {
		return GF_OK;
	}
}


static XErrorHandler old_handler = NULL;
static int selectinput_err = 0;
static int X11_BadAccess_ByPass(Display * display,
                                XErrorEvent * event)
{
	char msg[60];
	if (!display || !event) return 0;

	if (event->error_code == BadAccess)
	{
		selectinput_err = 1;
		return 0;
	}
	if (old_handler != NULL)
		old_handler(display, event);
	else
	{
		XGetErrorText(display, event->error_code, (char *) &msg, 60);
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Error %s\n",msg));
	}
	return 0;
}


static void X11_XScreenSaverState(XWindow *xwin, Bool turn_on)
{
	if (turn_on) {
		if (!xwin->ss_t) return;
		XSetScreenSaver(xwin->display, xwin->ss_t, xwin->ss_i, xwin->ss_b, xwin->ss_e);
	} else {
		/* Save state information */
		XGetScreenSaver(xwin->display, &xwin->ss_t, &xwin->ss_i, &xwin->ss_b, &xwin->ss_e);
		if (xwin->ss_t)
			XSetScreenSaver(xwin->display, 0, xwin->ss_i, xwin->ss_b, xwin->ss_e);
	}
}

/*
 * Setup X11 wnd System
 */
void
X11_SetupWindow (GF_VideoOutput * vout)
{
	X11VID ();
#if defined(GPAC_HAS_X11_SHM) || defined(GPAC_HAS_X11_XV) || defined(GPAC_HAS_OPENGL)
	const char *sOpt;
#endif

	Bool autorepeat, supported;

	if (xWindow->setup_done) return;
	xWindow->setup_done = 1;

	XInitThreads();

	xWindow->display = XOpenDisplay (NULL);
	if (!xWindow->display) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Failed to open X11 display %d\n", errno));
		return;
	}
	xWindow->screennum = DefaultScreen (xWindow->display);
	xWindow->screenptr = DefaultScreenOfDisplay (xWindow->display);
	xWindow->visual = DefaultVisualOfScreen (xWindow->screenptr);
	xWindow->depth = DefaultDepth (xWindow->display, xWindow->screennum);

	{
		Float screenWidth = (Float)XWidthOfScreen(xWindow->screenptr);
		Float screenWidthIn = (Float)XWidthMMOfScreen(xWindow->screenptr) / 25.4f;
		Float screenHeight = (Float)XHeightOfScreen(xWindow->screenptr);
		Float screenHeightIn = (Float)XHeightMMOfScreen(xWindow->screenptr) / 25.4f;
		vout->dpi_x = (u32)(screenWidth / screenWidthIn);
		vout->dpi_y = (u32)(screenHeight / screenHeightIn);
	}

	switch (xWindow->depth) {
	case 8:
		xWindow->pixel_format = GF_PIXEL_GREYSCALE;
		break;
	case 16:
		xWindow->pixel_format = GF_PIXEL_RGB_565;
		break;
	case 24:
#ifdef GPAC_BIG_ENDIAN
		if (xWindow->visual->red_mask==0x00FF0000)
			xWindow->pixel_format = GF_PIXEL_RGB;
		else
			xWindow->pixel_format = GF_PIXEL_BGR;
#else
		if (xWindow->visual->red_mask==0x00FF0000)
			xWindow->pixel_format = GF_PIXEL_BGR;
		else
			xWindow->pixel_format = GF_PIXEL_RGB;
#endif
		break;
	default:
		xWindow->pixel_format = GF_PIXEL_GREYSCALE;
		break;
	}
	xWindow->bpp = xWindow->depth / 8;
	xWindow->bpp = xWindow->bpp == 3 ? 4 : xWindow->bpp;

	xWindow->screennum=0;
	vout->max_screen_width = DisplayWidth(xWindow->display, xWindow->screennum);
	vout->max_screen_height = DisplayHeight(xWindow->display, xWindow->screennum);
	vout->max_screen_bpp = 8;

	/*
	 * Full screen wnd
	 */
	xWindow->full_wnd = XCreateWindow (xWindow->display,
	                                   RootWindowOfScreen (xWindow->screenptr),
	                                   0, 0,
	                                   vout->max_screen_width,
	                                   vout->max_screen_height, 0,
	                                   xWindow->depth, InputOutput,
	                                   xWindow->visual, 0, NULL);

	XSelectInput(xWindow->display, xWindow->full_wnd,
	             FocusChangeMask | ExposureMask | PointerMotionMask | ButtonReleaseMask | ButtonPressMask | KeyPressMask | KeyReleaseMask);

	if (!xWindow->par_wnd) {
		xWindow->w_width = 320;
		xWindow->w_height = 240;
		xWindow->wnd = XCreateWindow (xWindow->display,
		                              RootWindowOfScreen(xWindow->screenptr), 0, 0,
		                              xWindow->w_width, xWindow->w_height, 0,
		                              xWindow->depth, InputOutput,
		                              xWindow->visual, 0, NULL);
	} else {
		XWindowAttributes pwa;
		XGetWindowAttributes(xWindow->display, xWindow->par_wnd, &pwa);
		xWindow->w_width = pwa.width;
		xWindow->w_height = pwa.height;
		xWindow->wnd = XCreateWindow (xWindow->display, xWindow->par_wnd, pwa.x, pwa.y,
		                              xWindow->w_width, xWindow->w_height, 0,
		                              xWindow->depth, InputOutput,
		                              xWindow->visual, 0, NULL);
	}

	if (!(xWindow->init_flags & GF_VOUT_INIT_HIDE)) {
		XMapWindow (xWindow->display, (Window) xWindow->wnd);
	}

	XSync(xWindow->display, False);
	XUnmapWindow (xWindow->display, (Window) xWindow->wnd);
	XSync(xWindow->display, False);
	old_handler = XSetErrorHandler(X11_BadAccess_ByPass);
	selectinput_err = 0;
	XSelectInput(xWindow->display, xWindow->wnd,
	             FocusChangeMask | StructureNotifyMask | PropertyChangeMask | ExposureMask |
	             PointerMotionMask | ButtonReleaseMask | ButtonPressMask |
	             KeyPressMask | KeyReleaseMask);
	XSync(xWindow->display, False);
	XSetErrorHandler(old_handler);
	if (selectinput_err) {
		XSelectInput(xWindow->display, xWindow->wnd,
		             StructureNotifyMask | PropertyChangeMask | ExposureMask |
		             KeyPressMask | KeyReleaseMask);

		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Cannot select input focus\n"));
	}
	XSync(xWindow->display, False);
	if (!(xWindow->init_flags & GF_VOUT_INIT_HIDE)) {
		XMapWindow (xWindow->display, (Window) xWindow->wnd);
	}
	XSizeHints *Hints = XAllocSizeHints ();
	Hints->flags = PSize | PMinSize;
	Hints->min_width = 64;
	Hints->min_height = 64;
	Hints->max_height = 4096;
	Hints->max_width = 4096;
	if (!xWindow->par_wnd) {
		XSetWMNormalHints (xWindow->display, xWindow->wnd, Hints);
		XStoreName (xWindow->display, xWindow->wnd, "GPAC X11 Output");
	}
	Hints->x = 0;
	Hints->y = 0;
	Hints->flags |= USPosition;
	XSetWMNormalHints (xWindow->display, xWindow->full_wnd, Hints);

	{
		XClassHint hint;
		hint.res_name = "gpac";
		hint.res_class = "gpac";
		XSetClassHint(xWindow->display, xWindow->wnd, &hint);
	}

	autorepeat = 1;
	XkbSetDetectableAutoRepeat(xWindow->display, autorepeat, &supported);


	if (xWindow->init_flags & GF_VOUT_WINDOW_NO_DECORATION) {
#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
#define MWM_HINTS_DECORATIONS (1L << 1)
		struct {
			unsigned long flags;
			unsigned long functions;
			unsigned long decorations;
			long inputMode;
			unsigned long status;
		} hints = {2, 0, 0, 0, 0};

		hints.flags = MWM_HINTS_DECORATIONS;
		hints.decorations = 0;

		XChangeProperty(xWindow->display, xWindow->wnd, XInternAtom(xWindow->display,"_MOTIF_WM_HINTS", False), XInternAtom(xWindow->display, "_MOTIF_WM_HINTS", False), 32, PropModeReplace, (unsigned char *)&hints, PROP_MOTIF_WM_HINTS_ELEMENTS);

	}

	xWindow->the_gc = XCreateGC (xWindow->display, xWindow->wnd, 0, NULL);
	xWindow->use_shared_memory = 0;

#ifdef GPAC_HAS_X11_SHM
	sOpt = gf_module_get_key((GF_BaseInterface*)vout, "hwvmem");
	if (!sOpt || strcmp(sOpt, "never")) {
		int XShmMajor, XShmMinor;
		Bool XShmPixmaps;
		if (XShmQueryVersion(xWindow->display, &XShmMajor, &XShmMinor, &XShmPixmaps)) {
			xWindow->use_shared_memory = 1;
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Using X11 Shared Memory\n"));
			if ((XShmPixmaps==True) && (XShmPixmapFormat(xWindow->display)==ZPixmap)) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] X11 Shared Memory Pixmaps available\n"));
			}
		}
	}

#endif

#ifdef GPAC_HAS_X11_XV
	if (!gf_module_get_bool((GF_BaseInterface *)vout, "colorkey")) {
		xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_YV12, 0);
	} else {
		xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_YV12, 1);
		if (xWindow->xvport<0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Hardware has no color keying\n"));
			vout->overlay_color_key = 0;
			xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_YV12, 0);
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Hardware uses color key %08x\n", vout->overlay_color_key));
		}
	}
	if (xWindow->xvport>=0) {
		XvUngrabPort(xWindow->display, xWindow->xvport, CurrentTime );
		xWindow->xvport = -1;
		vout->yuv_pixel_format = X11_GetPixelFormat(xWindow->xv_pf_format);
		vout->Blit = X11_Blit;
		vout->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY;
		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Using XV YUV Overlays\n"));

#ifdef GPAC_HAS_X11_SHM
		/*if user asked for YUV->RGB on offscreen, do it (it may crash the system)*/
		if (gf_module_get_bool((GF_BaseInterface *)vout, "offscreen-yuv")) {
			vout->hw_caps |= GF_VIDEO_HW_HAS_YUV;
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Using XV Offscreen YUV2RGB acceleration\n"));
		}
#endif
	}
#endif

	XSetWindowAttributes xsw;
	xsw.border_pixel = WhitePixel (xWindow->display, xWindow->screennum);
	xsw.background_pixel = BlackPixel (xWindow->display, xWindow->screennum);
	xsw.win_gravity = NorthWestGravity;
	XChangeWindowAttributes (xWindow->display, xWindow->wnd, CWBackPixel | CWWinGravity, &xsw);

	xsw.override_redirect = True;
	XChangeWindowAttributes(xWindow->display, xWindow->full_wnd,
	                        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWWinGravity, &xsw);

	if (!xWindow->par_wnd) {
		xWindow->WM_DELETE_WINDOW = XInternAtom (xWindow->display, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(xWindow->display, xWindow->wnd, &xWindow->WM_DELETE_WINDOW, 1);
	}


	{
		XEvent ev;
		long mask;

		memset (&ev, 0, sizeof (ev));
		ev.xclient.type = ClientMessage;
		ev.xclient.window = RootWindowOfScreen (xWindow->screenptr);
		ev.xclient.message_type = XInternAtom (xWindow->display, "KWM_KEEP_ON_TOP", False);
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = xWindow->full_wnd;
		ev.xclient.data.l[1] = CurrentTime;
		mask = SubstructureRedirectMask;
		XSendEvent (xWindow->display,RootWindowOfScreen (xWindow->screenptr), False,
		            mask, &ev);
	}

	/*openGL setup*/
#ifdef GPAC_HAS_OPENGL
	{
		int attribs[64];
		int i, nb_bits, nb_depth_bits;

		sOpt = gf_opts_get_key("core", "gl-bits-comp");
		/* Most outputs are 24/32 bits these days, use 8 bits per channel instead of 5, works better on MacOS X */
		nb_bits = sOpt ? atoi(sOpt) : 8;
		if (!sOpt) {
			gf_opts_set_key("core", "gl-bits-comp", "8");
		}
retry_8bpp:
		i=0;
		if (nb_bits>8) {
			attribs[i++] = GLX_DRAWABLE_TYPE;
			attribs[i++] =  GLX_WINDOW_BIT;
			attribs[i++] = GLX_RENDER_TYPE;
			attribs[i++] = GLX_RGBA_BIT;
		} else {
			attribs[i++] = GLX_RGBA;
		}
		attribs[i++] = GLX_RED_SIZE;
		attribs[i++] = nb_bits;
		attribs[i++] = GLX_GREEN_SIZE;
		attribs[i++] = nb_bits;
		attribs[i++] = GLX_BLUE_SIZE;
		attribs[i++] = nb_bits;
		if (nb_bits==10) {
			attribs[i++] = GLX_ALPHA_SIZE;
			attribs[i++] = 2;
		}

		sOpt = gf_opts_get_key("core", "gl-bits-depth");
		nb_depth_bits = sOpt ? atoi(sOpt) : 16;
		if (!sOpt) {
			gf_opts_set_key("core", "gl-bits-depth", "16");
		}
		if (nb_bits) {
			attribs[i++] = GLX_DEPTH_SIZE;
			attribs[i++] = nb_depth_bits;
		}
		sOpt = gf_opts_get_key("core", "gl-doublebuf");
		if (!sOpt) {
			gf_opts_set_key("core", "gl-doublebuf", "yes");
		}
		if (!sOpt || !strcmp(sOpt, "yes")) {
			attribs[i++] = GLX_DOUBLEBUFFER;
			attribs[i++] = True;
		}
		attribs[i++] = None;

		if (nb_bits>8) {
			int fbcount=0;
			GLXFBConfig *fb=NULL;
			typedef GLXFBConfig * (* FnGlXChooseFBConfigProc)( Display *, int, int const *,int * );
			typedef XVisualInfo * (* FnGlXGetVisualFromFBConfigProc)( Display *, GLXFBConfig );
			typedef int (* FnGlXGetFBConfigAttrib) (Display *  dpy,  GLXFBConfig  config,  int  attribute,  int *  value);


			FnGlXChooseFBConfigProc my_glXChooseFBConfig = (FnGlXChooseFBConfigProc) glXGetProcAddress((GLubyte*) "glXChooseFBConfig");
			FnGlXGetVisualFromFBConfigProc my_glXGetVisualFromFBConfig = (FnGlXGetVisualFromFBConfigProc)glXGetProcAddress((GLubyte*) "glXGetVisualFromFBConfig");
			FnGlXGetFBConfigAttrib my_glXGetFBConfigAttrib = (FnGlXGetFBConfigAttrib)glXGetProcAddress((GLubyte*) "glXGetFBConfigAttrib");

			if (my_glXChooseFBConfig && my_glXGetVisualFromFBConfig) {
				fb = my_glXChooseFBConfig(xWindow->display, xWindow->screennum, attribs, &fbcount);
			}

			if (fbcount==0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Failed to choose GLX config for 10 bit depth - retrying with 8 bit depth\n"));
				nb_bits = 8;
				goto retry_8bpp;
			}
			if (my_glXGetVisualFromFBConfig)
				xWindow->glx_visualinfo = my_glXGetVisualFromFBConfig(xWindow->display, fb[0]);

			if (my_glXGetFBConfigAttrib && fb) {
				int r, g, b;
				my_glXGetFBConfigAttrib(xWindow->display, fb[0], GLX_RED_SIZE, &r);
				my_glXGetFBConfigAttrib(xWindow->display, fb[0], GLX_GREEN_SIZE, &g);
				my_glXGetFBConfigAttrib(xWindow->display, fb[0], GLX_BLUE_SIZE, &b);
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[GLX] Configured display asked %d bits got r:%d g:%d b:%d bits\n", nb_bits, r, g, b));
			}
		} else {
			xWindow->glx_visualinfo = glXChooseVisual(xWindow->display, xWindow->screennum, attribs);
		}
		vout->max_screen_bpp = nb_bits;

		if (!xWindow->glx_visualinfo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[X11] Error selecting GL display\n"));
		}
	}

	xWindow->gl_wnd = XCreateWindow (xWindow->display, RootWindowOfScreen (xWindow->screenptr),
	                                 0,
	                                 0,
	                                 200,
	                                 200, 0,
	                                 xWindow->depth, InputOutput,
	                                 xWindow->visual, 0, NULL);

	XSync(xWindow->display, False);
	XUnmapWindow(xWindow->display, (Window) xWindow->gl_wnd);
	XSync(xWindow->display, False);

	sOpt = gf_opts_get_key("core", "gl-offscreen");
	if (!sOpt)
		gf_opts_set_key("core", "gl-offscreen", "Pixmap");
	if (sOpt && !strcmp(sOpt, "Window")) {
		xWindow->offscreen_type = 1;
	} else if (sOpt && !strcmp(sOpt, "VisibleWindow")) {
		xWindow->offscreen_type = 2;
		XSetWMNormalHints (xWindow->display, xWindow->gl_wnd, Hints);
	} else if (sOpt && !strcmp(sOpt, "Pixmap")) {
		xWindow->offscreen_type = 0;
	} else {
		xWindow->offscreen_type = 0;
	}
#endif


	/*turn off xscreensaver*/
	X11_XScreenSaverState(xWindow, 0);

	XFree (Hints);
}

GF_Err X11_Setup(struct _video_out *vout, void *os_handle, void *os_display, u32 flags)
{
	X11VID ();
	/*assign window if any, NEVER display*/
	xWindow->par_wnd = (Window) os_handle;
	xWindow->init_flags = flags;

	//window already setup and this restup asks for visible, show window
	if (xWindow->wnd) {
		if (!(xWindow->init_flags & GF_VOUT_INIT_HIDE)) {
			XMapWindow (xWindow->display, (Window) xWindow->wnd);
		}
	}

	if (os_display) xWindow->no_select_input = 1;

	/*the rest is done THROUGH THE MAIN RENDERER TRHEAD!!*/
	return GF_OK;
}

/* Shutdown X11 */
void X11_Shutdown (struct _video_out *vout)
{
	X11VID ();
	if (! xWindow->display) return;

	X11_ReleaseBackBuffer (vout);

#ifdef GPAC_HAS_OPENGL
	X11_ReleaseGL(xWindow);
	if (xWindow->glx_visualinfo)
		XFree(xWindow->glx_visualinfo);
	xWindow->glx_visualinfo = NULL;
#endif
	XFreeGC (xWindow->display, xWindow->the_gc);
	XUnmapWindow (xWindow->display, (Window) xWindow->wnd);
	XDestroyWindow (xWindow->display, (Window) xWindow->wnd);
	XDestroyWindow (xWindow->display, (Window) xWindow->full_wnd);
#ifdef GPAC_HAS_OPENGL
	if (xWindow->gl_offscreen) glXDestroyGLXPixmap(xWindow->display, xWindow->gl_offscreen);
	if (xWindow->gl_pixmap) XFreePixmap(xWindow->display, xWindow->gl_pixmap);
	XUnmapWindow (xWindow->display, (Window) xWindow->gl_wnd);
	XDestroyWindow (xWindow->display, (Window) xWindow->gl_wnd);
#endif

	/*restore xscreen saver*/
	X11_XScreenSaverState(xWindow, 1);

	XCloseDisplay (xWindow->display);
	gf_free(xWindow);
	vout->opaque = NULL;
}


static GF_GPACArg X11Args[] = {
	GF_DEF_ARG("offscreen-yuv", NULL, "enable offscreen yuv", "true", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("no-vsync", NULL, "disable vertical synchro", "false", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("hwvmem", NULL, "specify (2D rendering only) memory type of main video backbuffer. Depending on the scene type, this may drastically change the playback speed\n"
 "- always: always on hardware\n"
 "- never: always on system memory\n"
 "- auto: selected by GPAC based on content type (graphics or video)", "auto", "auto|always|never", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
	GF_DEF_ARG("colorkey", NULL, "enable colorkey for overlays", "true", NULL, GF_ARG_BOOL, 0),
	{0},
};


void *NewX11VideoOutput ()
{
	GF_VideoOutput *driv;
	XWindow *xWindow;
	GF_SAFEALLOC(driv, GF_VideoOutput);
	if (!driv) return NULL;
	GF_SAFEALLOC(xWindow, XWindow);
	if (!xWindow) {
		gf_free(driv);
		return NULL;
	}
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "x11", "gpac distribution")

	driv->opaque = xWindow;

	driv->Flush = X11_Flush;
	driv->SetFullScreen = X11_SetFullScreen;
	driv->Setup = X11_Setup;
	driv->Shutdown = X11_Shutdown;
	driv->LockBackBuffer = X11_LockBackBuffer;
	driv->ProcessEvent = X11_ProcessEvent;
	driv->hw_caps = GF_VIDEO_HW_OPENGL;
	/*fixme - needs a better detection scheme*/
	driv->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;

	if (gf_sys_is_test_mode()) {
		GF_Event evt;
		x11_translate_key(XK_BackSpace, &evt.key);
		X11_BadAccess_ByPass(NULL, NULL);
	}
	driv->args = X11Args;
	driv->description = "Video output using X11";

	return (void *) driv;

}


void
DeleteX11VideoOutput (GF_VideoOutput * vout)
{
	if (vout->opaque) gf_free(vout->opaque);
	gf_free(vout);
}

/*
 * interface query
 */
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		0
	};
	return si;
}


/*
 * interface create
 */
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface (u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE)
		return (GF_BaseInterface *) NewX11VideoOutput ();
	return NULL;
}


/*
 * interface destroy
 */
GPAC_MODULE_EXPORT
void ShutdownInterface (GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType)
	{
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteX11VideoOutput ((GF_VideoOutput *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( x11_out )
