/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2005-2012
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
#include <gpac/unicode.h>
#include <gpac/user.h>
#include <sys/time.h>
#include <X11/XKBlib.h>


void X11_SetupWindow (GF_VideoOutput * vout);


#ifdef GPAC_HAS_X11_XV
static void X11_DestroyOverlay(XWindow *xwin)
{
	if (xwin->overlay) XFree(xwin->overlay);
	xwin->overlay = NULL;
	xwin->xv_pf_format = 0;	
	if (xwin->xvport>=0) {
		XvUngrabPort(xwin->display, xwin->xvport, CurrentTime );
		xwin->xvport = -1;
	}
}

static Bool is_same_yuv(u32 pf, u32 a_pf)
{
	if (pf==a_pf) return 1;
	return 0;
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
		XvAttribute *attr;
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
*/			}
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

	xwin->xvport = X11_GetXVideoPort(vout, GF_PIXEL_I420, 0);
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
	}

	/*different size, recreate an image*/
	if ((xwin->overlay->width != video_src->width) || (xwin->overlay->height != video_src->height)) {
		if (xwin->overlay) XFree(xwin->overlay);
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

	if (xWindow->output_3d_mode==1) {
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

//=====================================
/*
 * Translate X_Key to GF_Key
 */
//=====================================
static void x11_translate_key(u32 X11Key, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = X11Key & 0xFF;
	switch (X11Key) {

	case XK_BackSpace: evt->key_code = GF_KEY_BACKSPACE; break;
	case XK_Tab: evt->key_code = GF_KEY_TAB; break;
	//case XK_Linefeed: evt->key_code = GF_KEY_LINEFEED; break;
	case XK_Clear: evt->key_code = GF_KEY_CLEAR; break;

	case XK_KP_Enter:
		evt->flags = GF_KEY_EXT_NUMPAD; 
	case XK_Return: 
		evt->key_code = GF_KEY_ENTER; 
		break;
	case XK_Pause: evt->key_code = GF_KEY_PAUSE; break;
	case XK_Caps_Lock: evt->key_code = GF_KEY_CAPSLOCK; break;
	case XK_Scroll_Lock: evt->key_code = GF_KEY_SCROLL; break;
	case XK_Escape: evt->key_code = GF_KEY_ESCAPE; break;
	case XK_Delete: 
		evt->key_code = GF_KEY_DEL; 
		break;

	case XK_Kanji: evt->key_code = GF_KEY_KANJIMODE; break;
	case XK_Katakana: evt->key_code = GF_KEY_JAPANESEKATAKANA; break;
	case XK_Romaji: evt->key_code = GF_KEY_JAPANESEROMAJI; break;
	case XK_Hiragana: evt->key_code = GF_KEY_JAPANESEHIRAGANA; break;
	case XK_Kana_Lock: evt->key_code = GF_KEY_KANAMODE; break;

	case XK_Home: evt->key_code = GF_KEY_HOME; break;
	case XK_Left: evt->key_code = GF_KEY_LEFT; break;
	case XK_Up: evt->key_code = GF_KEY_UP; break;
	case XK_Right: evt->key_code = GF_KEY_RIGHT; break;
	case XK_Down: evt->key_code = GF_KEY_DOWN; break;
	case XK_Page_Up: evt->key_code = GF_KEY_PAGEUP; break;
	case XK_Page_Down: evt->key_code = GF_KEY_PAGEDOWN; break;
	case XK_End: evt->key_code = GF_KEY_END; break;
	//case XK_Begin: evt->key_code = GF_KEY_BEGIN; break;


	case XK_Select: evt->key_code = GF_KEY_SELECT; break;
	case XK_Print: evt->key_code = GF_KEY_PRINTSCREEN; break;
	case XK_Execute: evt->key_code = GF_KEY_EXECUTE; break;
	case XK_Insert: evt->key_code = GF_KEY_INSERT; break;
	case XK_Undo: evt->key_code = GF_KEY_UNDO; break;
	//case XK_Redo: evt->key_code = GF_KEY_BEGIN; break;
	//case XK_Menu: evt->key_code = GF_KEY_BEGIN; break;
	case XK_Find: evt->key_code = GF_KEY_FIND; break;
	case XK_Cancel: evt->key_code = GF_KEY_CANCEL; break;
	case XK_Help: evt->key_code = GF_KEY_HELP; break;
	//case XK_Break: evt->key_code = GF_KEY_BREAK; break;
	//case XK_Mode_switch: evt->key_code = GF_KEY_BEGIN; break;
	case XK_Num_Lock: evt->key_code = GF_KEY_NUMLOCK; break;
	
	case XK_F1: evt->key_code = GF_KEY_F1; break;
	case XK_F2: evt->key_code = GF_KEY_F2; break;
	case XK_F3: evt->key_code = GF_KEY_F3; break;
	case XK_F4: evt->key_code = GF_KEY_F4; break;
	case XK_F5: evt->key_code = GF_KEY_F5; break;
	case XK_F6: evt->key_code = GF_KEY_F6; break;
	case XK_F7: evt->key_code = GF_KEY_F7; break;
	case XK_F8: evt->key_code = GF_KEY_F8; break;
	case XK_F9: evt->key_code = GF_KEY_F9; break;
	case XK_F10: evt->key_code = GF_KEY_F10; break;
	case XK_F11: evt->key_code = GF_KEY_F11; break;
	case XK_F12: evt->key_code = GF_KEY_F12; break;
	case XK_F13: evt->key_code = GF_KEY_F13; break;
	case XK_F14: evt->key_code = GF_KEY_F14; break;
	case XK_F15: evt->key_code = GF_KEY_F15; break;
	case XK_F16: evt->key_code = GF_KEY_F16; break;
	case XK_F17: evt->key_code = GF_KEY_F17; break;
	case XK_F18: evt->key_code = GF_KEY_F18; break;
	case XK_F19: evt->key_code = GF_KEY_F19; break;
	case XK_F20: evt->key_code = GF_KEY_F20; break;
	case XK_F21: evt->key_code = GF_KEY_F21; break;
	case XK_F22: evt->key_code = GF_KEY_F22; break;
	case XK_F23: evt->key_code = GF_KEY_F23; break;
	case XK_F24: evt->key_code = GF_KEY_F24; break;

	case XK_KP_Delete: 
	case XK_KP_Decimal: 
		evt->flags = GF_KEY_EXT_NUMPAD; 
		evt->key_code = GF_KEY_COMMA; 
		break;

	case XK_KP_Insert:
	case XK_KP_0: 
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_0; break;
	case XK_KP_End:
	case XK_KP_1: 
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_1; break;
	case XK_KP_Down: 
	case XK_KP_2: 
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_2; break;
	case XK_KP_Page_Down: 
	case XK_KP_3:
		 evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_3; break;
	case XK_KP_Left: 
	case XK_KP_4: 
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_4; break;
	case XK_KP_Begin: 
	case XK_KP_5:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_5; break;
	case XK_KP_Right: 
	case XK_KP_6:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_6; break;
	case XK_KP_Home: 
	case XK_KP_7:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_7; break;
	case XK_KP_Up: 
	case XK_KP_8:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_8; break;
	case XK_KP_Page_Up: 
	case XK_KP_9:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_9; break;
	case XK_KP_Add:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_PLUS; break;
	case XK_KP_Subtract:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_HYPHEN; break;
	case XK_KP_Multiply:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_STAR; break;
	case XK_KP_Divide:
		evt->flags = GF_KEY_EXT_NUMPAD; evt->key_code = GF_KEY_SLASH; break;


	case XK_Shift_R:
		evt->flags = GF_KEY_EXT_RIGHT;
	case XK_Shift_L:
		evt->key_code = GF_KEY_SHIFT; 
		break;
	case XK_Control_R:
		evt->flags = GF_KEY_EXT_RIGHT;
	case XK_Control_L:
		evt->key_code = GF_KEY_CONTROL; 
		break;
	case XK_Alt_R:
		evt->flags = GF_KEY_EXT_RIGHT;
	case XK_Alt_L:
		evt->key_code = GF_KEY_ALT; 
		break;
	case XK_Super_R:
		evt->flags = GF_KEY_EXT_RIGHT;
	case XK_Super_L:
		evt->key_code = GF_KEY_WIN; 
		break;

	case XK_Menu: evt->key_code = GF_KEY_META; break;
#ifdef XK_XKB_KEYS
	case XK_ISO_Level3_Shift: evt->key_code = GF_KEY_ALTGRAPH; break;

#endif
	case '!': evt->key_code = GF_KEY_EXCLAMATION; break;
	case '"': evt->key_code = GF_KEY_QUOTATION; break;
	case '#': evt->key_code = GF_KEY_NUMBER; break;
	case '$': evt->key_code = GF_KEY_DOLLAR; break;
	case '&': evt->key_code = GF_KEY_AMPERSAND; break;
	case '\'': evt->key_code = GF_KEY_APOSTROPHE; break;
	case '(': evt->key_code = GF_KEY_LEFTPARENTHESIS; break;
	case ')': evt->key_code = GF_KEY_RIGHTPARENTHESIS; break;
	case ',': evt->key_code = GF_KEY_COMMA; break;
	case ':': evt->key_code = GF_KEY_COLON; break;
	case ';': evt->key_code = GF_KEY_SEMICOLON; break;
	case '<': evt->key_code = GF_KEY_LESSTHAN; break;
	case '>': evt->key_code = GF_KEY_GREATERTHAN; break;
	case '?': evt->key_code = GF_KEY_QUESTION; break;
	case '@': evt->key_code = GF_KEY_AT; break;
	case '[': evt->key_code = GF_KEY_LEFTSQUAREBRACKET; break;
	case ']': evt->key_code = GF_KEY_RIGHTSQUAREBRACKET; break;
	case '\\': evt->key_code = GF_KEY_BACKSLASH; break;
	case '_': evt->key_code = GF_KEY_UNDERSCORE; break;
	case '`': evt->key_code = GF_KEY_GRAVEACCENT; break;
	case ' ': evt->key_code = GF_KEY_SPACE; break;
	case '/': evt->key_code = GF_KEY_SLASH; break;
	case '*': evt->key_code = GF_KEY_STAR; break;
	case '-': evt->key_code = GF_KEY_HYPHEN; break;
	case '+': evt->key_code = GF_KEY_PLUS; break;
	case '=': evt->key_code = GF_KEY_EQUALS; break;
	case '^': evt->key_code = GF_KEY_CIRCUM; break;
	case '{': evt->key_code = GF_KEY_LEFTCURLYBRACKET; break;
	case '}': evt->key_code = GF_KEY_RIGHTCURLYBRACKET; break;
	case '|': evt->key_code = GF_KEY_PIPE; break;
	default:
		if ((X11Key>='0') && (X11Key<='9'))  evt->key_code = GF_KEY_0 + X11Key - '0';
		else if ((X11Key>='A') && (X11Key<='Z'))  evt->key_code = GF_KEY_A + X11Key - 'A';
		/*DOM3: translate to A -> Z, not a -> z*/
		else if ((X11Key>='a') && (X11Key<='z'))  {
			evt->key_code = GF_KEY_A + X11Key - 'a';
			evt->hw_code = X11Key - 'a' + 'A';
		}
		else {
			evt->key_code = 0; 
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[X11] Unrecognized key %X\n", X11Key));
		}
		break;
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

  GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[X11] Setting up GL for display %d\n", xWin->display));
  XSync(xWin->display, False);
  xWin->glx_context = glXCreateContext(xWin->display,xWin->glx_visualinfo, NULL, True);
  XSync(xWin->display, False);
  if (!xWin->glx_context) return GF_IO_ERR;
  if (xWin->output_3d_mode==2)  return GF_IO_ERR;

  if ( ! glXMakeCurrent(xWin->display, xWin->fullscreen ? xWin->full_wnd : xWin->wnd, xWin->glx_context) ) return GF_IO_ERR;
  XSync(xWin->display, False);
  memset(&evt, 0, sizeof(GF_Event));
  evt.type = GF_EVENT_VIDEO_SETUP;
  vout->on_event (vout->evt_cbk_hdl,&evt);
  xWin->is_init = 1;
  return GF_OK;
}

static GF_Err X11_SetupGLPixmap(GF_VideoOutput *vout, u32 width, u32 height)
{
  XWindow *xWin = (XWindow *)vout->opaque;

  if (xWin->glx_context) {
    glXMakeCurrent(xWin->display, None, NULL);
    glXDestroyContext(xWin->display, xWin->glx_context);
    xWin->glx_context = NULL;
  }
  if (xWin->gl_offscreen) glXDestroyGLXPixmap(xWin->display, xWin->gl_offscreen);
  xWin->gl_offscreen = 0;
  if (xWin->gl_pixmap) XFreePixmap(xWin->display, xWin->gl_pixmap);
  xWin->gl_pixmap = 0;

  if (xWin->offscreen_type) {
	  GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Using offscreen GL context through XWindow\n"));
	  if (xWin->offscreen_type==2) {
		XSync(xWin->display, False);
		XMapWindow (xWin->display, (Window) xWin->gl_wnd);
		XSync(xWin->display, False);
//		XSetWMNormalHints (xWin->display, xWin->gl_wnd, Hints);
 		XStoreName (xWin->display, xWin->gl_wnd, "GPAC Offscreeen Window - debug mode");
	}

	  XSync(xWin->display, False);
	  xWin->glx_context = glXCreateContext(xWin->display,xWin->glx_visualinfo, NULL, True);
	  XSync(xWin->display, False);
	  if (!xWin->glx_context) return GF_IO_ERR;
	  if ( ! glXMakeCurrent(xWin->display, xWin->gl_wnd, xWin->glx_context) ) return GF_IO_ERR;
	  XSync(xWin->display, False);

  } else {
  GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Using offscreen GL context through glXPixmap\n"));
  xWin->gl_pixmap = XCreatePixmap(xWin->display, xWin->gl_wnd, width, height, xWin->depth);
  if (!xWin->gl_pixmap) return GF_IO_ERR;
 
  xWin->gl_offscreen = glXCreateGLXPixmap(xWin->display, xWin->glx_visualinfo, xWin->gl_pixmap);
  if (!xWin->gl_offscreen) return GF_IO_ERR;
 
  XSync(xWin->display, False);
  xWin->glx_context = glXCreateContext(xWin->display, xWin->glx_visualinfo, 0, GL_FALSE);
  XSync(xWin->display, False);
  if (!xWin->glx_context) return GF_IO_ERR;

 XSync(xWin->display, False);
 GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[X11] Activating GLContext on GLPixmap - this may crash !!\n"));
 glXMakeCurrent(xWin->display, xWin->gl_offscreen, xWin->glx_context);
  }

  GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Offscreen GL context setup\n"));
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
		if (xWindow->surface->data)
			gf_free(xWindow->surface->data);
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
	Window cur_wnd;
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
	cur_wnd = xWindow->fullscreen ? xWindow->full_wnd : xWindow->wnd;

#ifdef GPAC_HAS_X11_SHM
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
		char *data = (char *) gf_malloc(sizeof(char)*size);
		xWindow->surface = XCreateImage (xWindow->display, xWindow->visual,
				      xWindow->depth, ZPixmap,
				      0,
				      data,
				      VideoWidth, VideoHeight,
				      xWindow->bpp*8, xWindow->bpp*VideoWidth);
		if (!xWindow->surface) {
			gf_free(data);
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

	if (!xWindow->setup_done) X11_SetupWindow(vout);

	if (evt) {

	switch (evt->type) {
	case GF_EVENT_SET_CURSOR:
		break;
	case GF_EVENT_SET_CAPTION:
		if (!xWindow->par_wnd) XStoreName (xWindow->display, xWindow->wnd, evt->caption.caption);
		break;
	case GF_EVENT_SHOWHIDE:
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
		switch (evt->setup.opengl_mode){
#ifdef GPAC_HAS_OPENGL
		case 1:
			xWindow->output_3d_mode = 1;
			return X11_SetupGL(vout);
		case 2:
			xWindow->output_3d_mode = 2;
			return X11_SetupGLPixmap(vout, evt->setup.width, evt->setup.height);
#endif
		case 0:
			xWindow->output_3d_mode = 0;
			return X11_ResizeBackBuffer(vout, evt->setup.width, evt->setup.height);
		default:
			return GF_NOT_SUPPORTED;
		}
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
	if (xWindow->output_3d_mode==1) X11_ReleaseGL(xWindow);
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
	} else {
		*screen_width = xWindow->store_width;
		*screen_height = xWindow->store_height;
		XFreeGC (xWindow->display, xWindow->the_gc);
		xWindow->the_gc = XCreateGC (xWindow->display, xWindow->wnd, 0, NULL);
		XUnmapWindow (xWindow->display, xWindow->full_wnd);
		XMapWindow (xWindow->display, xWindow->wnd);
		XUngrabKeyboard(xWindow->display, CurrentTime);
		/*looks like this makes osmozilla crash*/
		//if (xWindow->par_wnd) XSetInputFocus(xWindow->display, xWindow->wnd, RevertToNone, CurrentTime);

		/*backbuffer resize will be done right after this is called */
	}
#ifdef GPAC_HAS_OPENGL
	if (xWindow->output_3d_mode==1) X11_SetupGL(vout);
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
	return;
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
	const char *sOpt;
        Bool autorepeat, supported;

	xWindow->display = XOpenDisplay (NULL);
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
		xWindow->pixel_format = GF_PIXEL_RGB_32;
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
		XMapWindow (xWindow->display, (Window) xWindow->wnd);
	} else {
		XWindowAttributes pwa;
		XGetWindowAttributes(xWindow->display, xWindow->par_wnd, &pwa);
		xWindow->w_width = pwa.width;
		xWindow->w_height = pwa.height;
		xWindow->wnd = XCreateWindow (xWindow->display, xWindow->par_wnd, pwa.x, pwa.y,
					xWindow->w_width, xWindow->w_height, 0,
					   xWindow->depth, InputOutput,
					   xWindow->visual, 0, NULL);
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
	XMapWindow (xWindow->display, (Window) xWindow->wnd);

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

	autorepeat = 1;
	XkbSetDetectableAutoRepeat(xWindow->display, autorepeat, &supported);


	if (xWindow->init_flags & GF_TERM_WINDOW_NO_DECORATION) {
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
	sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "UseHardwareMemory");
        if (sOpt && !strcmp(sOpt, "yes")) {
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
	sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "DisableColorKeying");
	if (sOpt && !strcmp(sOpt, "yes")) {
		xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_I420, 0);
	} else {
		xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_I420, 1);
		if (xWindow->xvport<0) {
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[X11] Hardware has no color keying\n"));
			vout->overlay_color_key = 0;
			xWindow->xvport = X11_GetXVideoPort(vout, GF_PIXEL_I420, 0);
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
		sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "EnableOffscreenYUV");
		if (sOpt && !strcmp(sOpt, "yes")) {
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
	  int i, nb_bits;

	  sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "GLNbBitsPerComponent");
	  /* Most outputs are 24/32 bits these days, use 8 bits per channel instead of 5, works better on MacOS X */
	  nb_bits = sOpt ? atoi(sOpt) : 8;
          if (!sOpt){
             gf_modules_set_option((GF_BaseInterface *)vout, "Video", "GLNbBitsPerComponent", "8");
          }
	  i=0;
	  attribs[i++] = GLX_RGBA;
	  attribs[i++] = GLX_RED_SIZE;
	  attribs[i++] = nb_bits;
	  attribs[i++] = GLX_GREEN_SIZE;
	  attribs[i++] = nb_bits;
	  attribs[i++] = GLX_BLUE_SIZE;
	  attribs[i++] = nb_bits;
	  sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "GLNbBitsDepth");
	  nb_bits = sOpt ? atoi(sOpt) : 16;
          if (!sOpt){
             gf_modules_set_option((GF_BaseInterface *)vout, "Video", "GLNbBitsDepth", "16");
          }
	  if (nb_bits) {
		  attribs[i++] = GLX_DEPTH_SIZE;
		  attribs[i++] = nb_bits;
	  }
	  sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "UseGLDoubleBuffering");
          if (!sOpt){
             gf_modules_set_option((GF_BaseInterface *)vout, "Video", "UseGLDoubleBuffering", "yes");
          }
	  if (!sOpt || !strcmp(sOpt, "yes")) attribs[i++] = GLX_DOUBLEBUFFER;
	  attribs[i++] = None;
	  xWindow->glx_visualinfo = glXChooseVisual(xWindow->display, xWindow->screennum, attribs);
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

	sOpt = gf_modules_get_option((GF_BaseInterface *)vout, "Video", "X113DOffscreenMode");
	if (!sOpt)
		gf_modules_set_option((GF_BaseInterface *)vout, "Video", "X113DOffscreenMode", "Pixmap");
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

	xWindow->setup_done = 1;
	XFree (Hints);
}

GF_Err X11_Setup(struct _video_out *vout, void *os_handle, void *os_display, u32 flags)
{
	X11VID ();
	/*assign window if any, NEVER display*/
	xWindow->par_wnd = (Window) os_handle;
	xWindow->init_flags = flags;

	/*OSMOZILLA HACK*/
	if (os_display) xWindow->no_select_input = 1;

	/*the rest is done THROUGH THE MAIN RENDERER TRHEAD!!*/
	return GF_OK;
}

/* Shutdown X11 */
void X11_Shutdown (struct _video_out *vout)
{
	X11VID ();

	if (xWindow->output_3d_mode==1) {
#ifdef GPAC_HAS_OPENGL
		X11_ReleaseGL(xWindow);
#endif
	} else {
		X11_ReleaseBackBuffer (vout);
	}
#ifdef GPAC_HAS_OPENGL
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
}




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
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "X11 Video Output", "gpac distribution")

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
	return (void *) driv;

}


void
DeleteX11VideoOutput (GF_VideoOutput * vout)
{
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

GPAC_MODULE_STATIC_DELARATION( x11_out )
