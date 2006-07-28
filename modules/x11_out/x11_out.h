/*
 *					GPAC Multimedia Framework
 *
 *			Authors: DINH Anh Min - Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
 *					All rights reserved
 *
 *  This file is part of GPAC / X11 video output module
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
#ifndef _X11_OUT_H
#define _X11_OUT_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <gpac/modules/video_out.h>
#include <gpac/thread.h>
#include <gpac/list.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef GPAC_HAS_OPENGL
#include <GL/glx.h>
#endif

#ifdef GPAC_HAS_X11_SHM
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
	Window par_wnd;	//main window handler passed to module, NULL otherwise
	Bool setup_done, no_select_input;	//setup is done
	Display *display;	//required by all X11 method, provide by XOpenDisplay, Mozilla wnd ...
	Window wnd;	//window handler created by module
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

#ifdef GPAC_HAS_X11_SHM
	Pixmap pixmap;		//main drawing image : local sharememory mode
	XShmSegmentInfo *shmseginfo;
#endif
	
	GF_List *surfaces;	//surfaces list

	Bool is_init, fullscreen, has_focus;

	/*backbuffer size before entering fullscreen mode (used for restore) */
	u32 store_width, store_height;

	u32 w_width, w_height;
	u32 depth, bpp, pixel_format;
	Bool is_3D_out;
#ifdef GPAC_HAS_OPENGL
	XVisualInfo *glx_visualinfo;
	GLXContext glx_context;
	GF_GLConfig gl_cfg;
#endif
} XWindow;

void StretchBits (void *dst, u32 dst_bpp, u32 dst_w, u32 dst_h, u32 dst_pitch,
	     void *src, u32 src_bpp, u32 src_w, u32 src_h, u32 src_pitch, Bool FlipIt);


#endif /* _X11_OUT_H */
