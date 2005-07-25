/***************************************************************************
*            X11_out.h
*
*  Mon Jun 13 12:13:02 2005
*  Copyright  2005  DINH Anh Minh
*  Email anhminh.dinh@gmail.com
****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _X11_OUT_H
#define _X11_OUT_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <gpac/modules/video_out.h>
#include <gpac/thread.h>
#include <gpac/list.h>


//#define __USE_X_SHAREDMEMORY__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <GL/glx.h>

#include <stdio.h>
#include <stdlib.h>		/* getenv(), etc. */
#include <unistd.h>		/* sleep(), etc.  */


#ifdef __USE_X_SHAREDMEMORY__
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#define X11VID()	XWindow *xWindow = (XWindow *)vout->opaque;

#define VIDEO_XI_STANDARD     0x00	/* Use standard Xlib calls */
#define VIDEO_XI_SHMSTD       0x01	/* Use Xlib shared memory extension */
#define VIDEO_XI_SHMPIXMAP    0x02	/* Use shared memory pixmap */

#define RGB555(r,g,b) (((r&248)<<7) + ((g&248)<<2)  + (b>>3))
#define RGB565(r,g,b) (((r&248)<<8) + ((g&252)<<3)  + (b>>3))

typedef struct
{
	unsigned char *buffer;	//surface buffer
	u32 pitch, pixel_format, width, height, BPP;	//
	u32 id;		//
}
X11WrapSurface;

typedef struct
{
	Display *display;	//required by all X11 method, provide by XOpenDisplay, Mozilla wnd ...
	Window wnd;	//main window handler
	Bool owns_wnd;
	Window full_wnd;	//full screen
	Screen *screenptr;	//X11 stuff
	int screennum;		//...
	Visual *visual;		//...
	GC the_gc;			//graphics context
	XImage *surface;	//main drawing image: software mode
	Atom WM_DELETE_WINDOW;	//window deletion

	X11WrapSurface *back_buffer;	//back buffer
	Colormap colormap;	//not for now
	int videoaccesstype;	//

#ifdef __USE_X_SHAREDMEMORY__
	Pixmap pixmap;		//main drawing image : local sharememory mode
	XShmSegmentInfo *shmseginfo;
#endif
	
	GF_Thread *th;		//handle event thread
	GF_Mutex *mx;		//mutex blocker
	GF_List *surfaces;	//surfaces list

	Bool is_init, fullscreen;

	/*backbuffer size before entering fullscreen mode (used for restore) */
	u32 store_width, store_height;
	/*display size */
	u32 display_width, display_height;

	u32 w_width, w_height;
	u32 x11_th_state;
	u32 depth, bpp, pixel_format;
	Bool is_3D_out;
	XVisualInfo *glx_visualinfo;
	GLXContext glx_context;
} XWindow;

void StretchBits (void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, u32 dst_pitch,
	     void *src, u32 src_bpp, u32 src_w, u32 src_h, u32 src_pitch, Bool FlipIt);


#endif /* _X11_OUT_H */
