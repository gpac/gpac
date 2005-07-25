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

/*
		Video hardware output module
*/

/*opengl configuration*/
typedef struct
{
	Bool double_buffered;
} GF_GLConfig;

/*interface name and version for video output*/
#define GF_VIDEO_OUTPUT_INTERFACE	GF_FOUR_CHAR_INT('G','V','O',0x02) 

/*
			video output interface

	the video output may run in 2 modes: 2D and 3D.

	** the 2D video output works by creating surfaces on the video mem board - the app refers to the surfaces 
	by their IDs, and access them through the GF_VideoSurface handler. 
	Surface index 0 is reserved for the main video memory (if double (or n) buffering is performed by the driver 
	the surface 0 is the backbuffer (or current flip buffer) )
	Overlay surfaces are not supported yet due to lack of time and also no profile for simple AV in MPEG4

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

	/*set full screen - screen resolution shall be reported in screen_width and screen_height when turning 
	on FS, otherwise retored window size shall be reported - the screen mode to select shall be the smallest 
	one bigger than current output size - the driver may destroy all extra surfaces created when 
	switching to fullscreen*/
	GF_Err (*SetFullScreen) (struct _video_out *vout, Bool bFullScreenOn, u32 *screen_width, u32 *screen_height);

	/*flush video: the video shall be presented to screen 
	the destination area to update is in client display coordinates (0,0) being top-left, (w,h) bottom-right
	it shall be ugnored when using 3D output (buffer flip only)*/
	GF_Err (*FlushVideo) (struct _video_out *vout, GF_Window *dest);

	/*window events sent to output:
	GF_EVT_SET_CURSOR, GF_EVT_SET_STYLE, GF_EVT_SET_CAPTION, GF_EVT_SHOWHIDE, GF_EVT_SIZE for inital window resize
	and GF_EVT_VIDEO_SETUP for all HW related setup
	*/
	GF_Err (*ProcessEvent)(struct _video_out *vout, GF_Event *event);

	/*pass events to user (assigned before setup)*/
	void *evt_cbk_hdl;
	void (*on_event)(void *hdl, GF_Event *event);

	/*driver private*/
	void *opaque;

	Bool bHas3DSupport;
	/*
			All the following are 2D specific and are NEVER called in 3D mode
	*/
	/*clears screen with specified color*/
	GF_Err (*Clear) (struct _video_out *vout, u32 color);
	/*creates a offscreen video surface and setup surface id - pixel format MUST be respected except for YUV
	formats, where the hardware is free to choose the fastest format for blit*/
	GF_Err (*CreateSurface) (struct _video_out *vout, u32 width, u32 height, u32 pixel_format, u32 *surfaceID);
	/*deletes video surface by id*/
	GF_Err (*DeleteSurface) (struct _video_out *vout, u32 surface_id);
	/*lock video mem*/
	GF_Err (*LockSurface)(struct _video_out *vout, u32 surface_id, GF_VideoSurface *video_info);
	/*unlock video mem*/
	GF_Err (*UnlockSurface)(struct _video_out *vout, u32 surface_id);
	/*checks if the surface is valid - this is used to discard surfaces when changing video mode (fullscreen)*/
	Bool (*IsSurfaceValid) (struct _video_out *vout, u32 surface_id);
	/*resize surface - this may also be called on surfaceID=0 (eg, backbuffer)*/
	GF_Err (*ResizeSurface) (struct _video_out *vout, u32 surface_id, u32 width, u32 height);

	/*lock video mem through OS context (HDC, ...)*/
	void *(*GetContext)(struct _video_out *vout, u32 surface_id);
	/*unlock video mem through OS context (HDC, ...)*/
	void (*ReleaseContext)(struct _video_out *vout, u32 surface_id, void *context);

	/*blit operations - windows are provided in surface coordinate*/

	/*blit surface src to surface dest - if a window is not specified, the full surface is used _ can be NULL*/
	GF_Err (*Blit)(struct _video_out *vout, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst);
	/*blit surface src to surface dest with keying for src surface - if a window is not specified, the full surface is used - can be NULL*/
	GF_Err (*BlitKey)(struct _video_out *vout, u32 color_key, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst);
	/*blit surface src to surface dest with alpha - if a window is not specified, the full surface is used - can be NULL*/
	GF_Err (*BlitAlpha)(struct _video_out *vout, u32 alpha, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst);

	/*returns pixel format of the surface - if surfaceID is 0, the main video memory format is requested*/
	GF_Err (*GetPixelFormat) (struct _video_out *vout, u32 surfaceID, u32 *pixel_format);

	/*set to true if hardware supports color keying*/
	Bool bHasKeying;
	/*set to true if hardware supports color keying with stretching*/
	Bool bHasKeyingStretch;
	/*set to true if hardware supports texture blending*/
	Bool bHasAlpha;
	/*set to true if hardware supports texture blending with stretching*/
	Bool bHasAlphaStretch;
	/*set to true if YV12 input can be blited - this may be changed dynamically whenever a YUV surface is 
	resized or card main format changes*/
	Bool bHasYUV;
} GF_VideoOutput;



#ifdef __cplusplus
}
#endif


#endif	/*_GF_MODULE_VIDEO_OUT_H_*/

