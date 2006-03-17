/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / modules interfaces
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


/*

		Note on video driver: this is not a graphics driver, the only thing requested from this driver
	is accessing video memory and performing stretch of YUV and RGB on the backbuffer (bitmap node)
	the graphics driver is a different entity that performs 2D rasterization

*/

#ifndef _GF_MODULE_VIDEO_OUT_H_
#define _GF_MODULE_VIDEO_OUT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*include event system*/
#include <gpac/user.h>
/*include framebuffer definition*/
#include <gpac/color.h>

/*
		Video hardware output module
*/

/*opengl configuration*/
typedef struct
{
	Bool double_buffered;
} GF_GLConfig;

enum
{
	/*HW supports YUV->backbuffer blitting*/
	GF_VIDEO_HW_HAS_YUV = (1<<1),
	/*HW supports keying*/
	GF_VIDEO_HW_HAS_COLOR_KEY = (1<<2),
	/*HW supports OpenGL rendering. Whether this is OpenGL or OpenGL-ES depends on compilation settings and cannot be 
	changed at runtime*/
	GF_VIDEO_HW_HAS_OPENGL = (1<<4),
	/*HW supports 90 degres rotation of display (Mobile Phones & PDAs)*/
	GF_VIDEO_HW_CAN_ROTATE = (1<<5),
};

/*interface name and version for video output*/
#define GF_VIDEO_OUTPUT_INTERFACE	GF_4CC('G','V','O',0x03) 

/*
			video output interface

	the video output may run in 2 modes: 2D and 3D.

	** the 2D video output works by accessing a backbuffer surface on the video mem board - 
	the app accesses to the surface through the GF_VideoSurface handler. 
	The module may support HW blitting of RGB or YUV data to backbuffer.

	** the 3D video output only handles window management and openGL contexts setup.
	The context shall be setup in Resize and SetFullScreen calls which are always happening in the main 
	rendering thread. This will take care of openGL context issues with multithreading

	Except Setup and Shutdown functions, all interface functions are called through the main renderer thread
	or its user to avoid multithreading issues. Care must still be taken when handling events
*/
typedef struct _video_out
{
	/* interface declaration*/
	GF_DECL_MODULE_INTERFACE

	/*setup system - if os_handle is NULL the driver shall create the output display (common case)
	the other case is currently only used by child windows on win32 and winCE
	@no_proc_override: when set and a os_handle is passed, the module shall not try to
	override the window proc
	if cfg is specified, the output is 3D, otherwise 2D*/
	GF_Err (*Setup)(struct _video_out *vout, void *os_handle, void *os_display, Bool no_proc_override, GF_GLConfig *cfg);
	/*shutdown system */
	void (*Shutdown) (struct _video_out *vout);

	/*flush video: the video shall be presented to screen 
	the destination area to update is in client display coordinates (0,0) being top-left, (w,h) bottom-right
	Note: dest is always NULL in 3D mode (buffer flip only)*/
	GF_Err (*Flush) (struct _video_out *vout, GF_Window *dest);

	GF_Err (*SetFullScreen) (struct _video_out *vout, Bool fs_on, u32 *new_disp_width, u32 *new_disp_height);

	/*window events sent to output:
	GF_EVT_SET_CURSOR: sets cursor
	GF_EVT_SET_CAPTION: sets caption
	GF_EVT_SHOWHIDE: show/hide output window for self-managed output
	GF_EVT_SIZE:  inital window resize upon scene load
	GF_EVT_VIDEO_SETUP: all HW related setup:
		* for 2D output, this means resizing the backbuffer if needed (depending on HW constraints)
		* for 3D output, this means re-setup of OpenGL context (depending on HW constraints). Depending on windowing systems 
			and implementations, it could be possible to resize a window without destroying the GL context.
	
	This function is also called with a NULL event at the begining of each rendering cycle, in order to allow event 
	handling for modules uncapable of safe multithreading (eg X11)
	*/
	GF_Err (*ProcessEvent)(struct _video_out *vout, GF_Event *event);

	/*pass events to user (assigned before setup)*/
	void *evt_cbk_hdl;
	void (*on_event)(void *hdl, GF_Event *event);

	/*
			All the following are 2D specific and are NEVER called in 3D mode
	*/
	/*locks backbuffer video memory
	do_lock: specifies whether backbuffer shall be locked or released
	*/
	GF_Err (*LockBackBuffer)(struct _video_out *vout, GF_VideoSurface *video_info, Bool do_lock);

	/*lock video mem through OS context (only HDC for Win32 at the moment)
	do_lock: specifies whether OS context shall be locked or released*/
	void *(*LockOSContext)(struct _video_out *vout, Bool do_lock);

	/*blit surface src to backbuffer - if a window is not specified, the full surface is used
	the blitter MUST support stretching and RGB24 sources. Support for YUV is indicated in the hw caps
	of the driver. If none is supported, just set this function to NULL and let gpac performs software blitting.
	Whenever this function fails, the blit will be performed in software mode*/
	GF_Err (*Blit)(struct _video_out *vout, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 *src_rgb_key);

	/*set of above HW flags*/
	u32 hw_caps;
	/*main pixel format of video board (informative only)*/
	u32 pixel_format;
	/*yuv pixel format if HW YUV blitting is supported (informative only) */
	u32 yuv_pixel_format;
	/*maximum resolution of the screen*/
	u32 max_screen_width, max_screen_height;

	/*driver private*/
	void *opaque;
} GF_VideoOutput;



#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_VIDEO_OUT_H_*/

