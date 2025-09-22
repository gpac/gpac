/*
 *					GPAC Multimedia Framework
 *
 *			Authors: Romain Bouqueau - Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectFB video output module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.0
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <gpac/modules/video_out.h>
#include <gpac/scenegraph_svg.h>

#include "directfb_out.h"


#define DirectFBVID() DirectFBVidCtx *ctx = (DirectFBVidCtx *)driv->opaque

/**
 *	function DirectFBVid_Setup
 * 	- DirectFB setup
 **/
GF_Err DirectFBVid_Setup(GF_VideoOutput *driv, void *os_handle, void *os_display, u32 init_flags)
{
	const char* opt;

	DirectFBVID();
	DirectFBVid_CtxSetIsInit(ctx, 0);

	// initialisation and surface creation
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization\n"));
	// check window mode used - SDL or X11
	opt = gf_module_get_key((GF_BaseInterface *)driv, "devsys");
	DirectFBVid_InitAndCreateSurface(ctx, opt);

	// set flip mode
	FLIP_MODE flip_mode = 0;
	opt = gf_module_get_key((GF_BaseInterface *)driv, "flipmode");
	if (!opt || !strcmp(opt, "waitsync")) flip_mode = FLIP_WAITFORSYNC;
	else if (!strcmp(opt, "wait")) flip_mode = FLIP_WAIT;
	else if (!strcmp(opt, "sync")) flip_mode = FLIP_ONSYNC;
	else if (!strcmp(opt, "swap")) flip_mode = FLIP_SWAP;

	DirectFBVid_CtxSetFlipMode(ctx, flip_mode);

	// end of initialization
	DirectFBVid_CtxSetIsInit(ctx, 1);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Initialization success - HW caps %08x\n", driv->hw_caps));

//	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Pixel format %s\n", gf_4cc_to_str(ctx->pixel_format)));
	return GF_OK;
}


/**
 *	function DirectFBVid_Shutdown
 * 	- shutdown DirectFB module
 **/
static void DirectFBVid_Shutdown(GF_VideoOutput *driv)
{
	u32 ret;
	DirectFBVID();
	ret = DirectFBVid_ShutdownWrapper(ctx);
	if (ret) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Failed to shutdown properly\n"));
	}
}


/**
 *	function DirectFBVid_Flush
 * 	- flushing buffer
 **/
static GF_Err DirectFBVid_Flush(GF_VideoOutput *driv, GF_Window *dest)
{
	DirectFBVID();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] Flipping backbuffer\n"));
	return DirectFBVid_CtxPrimaryFlip(ctx);
}


/**
 *	function DirectFBVid_SetFullScreen
 * 	- set fullscreen mode
 **/
GF_Err DirectFBVid_SetFullScreen(GF_VideoOutput *driv, u32 bFullScreenOn, u32 *screen_width, u32 *screen_height)
{
	DirectFBVID();

	*screen_width = DirectFBVid_CtxGetWidth(ctx);
	*screen_height = DirectFBVid_CtxGetHeight(ctx);

	return GF_OK;
}


/**
 *	function DirectFBVid_ProcessMessageQueue
 * 	- handle DirectFB events
 **/
Bool DirectFBVid_ProcessMessageQueue(DirectFBVidCtx *ctx, GF_VideoOutput *driv)
{
	GF_Event gpac_evt;
	memset(&gpac_evt, 0, sizeof(gpac_evt));
	while (DirectFBVid_ProcessMessageQueueWrapper(ctx, &gpac_evt.type, &gpac_evt.key.flags, &gpac_evt.key.key_code, &gpac_evt.mouse.x, &gpac_evt.mouse.y, &gpac_evt.mouse.button) == GF_OK)
	{
		driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
		gpac_evt.key.flags = 0;
	}

	return GF_OK;
}


/**
 *	function DirectFBVid_ProcessEvent
 * 	- process events
 **/
static GF_Err DirectFBVid_ProcessEvent(GF_VideoOutput *driv, GF_Event *evt)
{
	DirectFBVID();

	if (!evt) {
		DirectFBVid_ProcessMessageQueue(ctx, driv);
		return GF_OK;
	}

	switch (evt->type) {
	case GF_EVENT_SIZE:
		if ((DirectFBVid_CtxGetWidth(ctx) !=evt->size.width) || (DirectFBVid_CtxGetHeight(ctx) != evt->size.height)) {
			GF_Event gpac_evt;
			gpac_evt.type = GF_EVENT_SIZE;
			gpac_evt.size.width = DirectFBVid_CtxGetWidth(ctx);
			gpac_evt.size.height = DirectFBVid_CtxGetHeight(ctx);
			driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
		}
		return GF_OK;

	case GF_EVENT_VIDEO_SETUP:
		if (evt->setup.use_opengl) return GF_NOT_SUPPORTED;

		if ((DirectFBVid_CtxGetWidth(ctx) !=evt->setup.width) || (DirectFBVid_CtxGetHeight(ctx) != evt->setup.height)) {
			GF_Event gpac_evt;
			gpac_evt.type = GF_EVENT_SIZE;
			gpac_evt.size.width = DirectFBVid_CtxGetWidth(ctx);
			gpac_evt.size.height = DirectFBVid_CtxGetHeight(ctx);
			driv->on_event(driv->evt_cbk_hdl, &gpac_evt);
		}
		return GF_OK;
	default:
		return GF_OK;
	}
}


/**
 *	function DirectFBVid_LockBackBuffer
 * 	- lock the surface to access certain data
 **/
static GF_Err DirectFBVid_LockBackBuffer(GF_VideoOutput *driv, GF_VideoSurface *video_info, u32 do_lock)
{
	void *buf;
	u32 pitch, ret;

	DirectFBVID();
	if (!DirectFBVid_CtxGetPrimary(ctx)) return GF_BAD_PARAM;
	if (do_lock)
	{
		if (!video_info) return GF_BAD_PARAM;
		// lock surface first in order to access data below
		ret = DirectFBVid_CtxPrimaryLock(ctx, &buf, &pitch);
		if (ret != 0) return GF_IO_ERR;

		// fetch data
		memset(video_info, 0, sizeof(GF_VideoSurface));
		video_info->width = DirectFBVid_CtxGetWidth(ctx);
		video_info->height = DirectFBVid_CtxGetHeight(ctx);
		video_info->pitch_x = 0;
		video_info->pitch_y = pitch;
		video_info->video_buffer = buf;
		video_info->pixel_format = DirectFBVid_CtxGetPixelFormat(ctx);
		video_info->is_hardware_memory = !DirectFBVid_CtxIsHwMemory(ctx);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] backbuffer locked\n"));
	} else {
		// unlock the surface after direct access
		DirectFBVid_CtxPrimaryUnlock(ctx);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] backbuffer unlocked\n"));
	}

	return GF_OK;
}


/**
 *	function DirectFBVid_Blit
 * 	- blit a surface
 **/
static GF_Err DirectFBVid_Blit(GF_VideoOutput *driv, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	u32 ret;
	DirectFBVID();

	ret = DirectFBVid_BlitWrapper(ctx, video_src->width, video_src->height, video_src->pixel_format, video_src->video_buffer, video_src->pitch_y, src_wnd->x, src_wnd->y, src_wnd->w, src_wnd->h, dst_wnd->x, dst_wnd->y, dst_wnd->w, dst_wnd->h, overlay_type);
	if (ret) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectFB] cannot create blit source surface for pixel format %s\n", gf_4cc_to_str(video_src->pixel_format)));
		return GF_NOT_SUPPORTED;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DirectFB] blit successful\n"));

	return GF_OK;
}

static GF_GPACArg DirectFBArgs[] = {
	GF_DEF_ARG("devsys", NULL, "device for DFB", "false", NULL, GF_ARG_BOOL, 0),
	GF_DEF_ARG("flipmode", NULL, "fliping mode for directFB\n" flip
	"- waitsync: wait for vsync\n"
	"- wait: wait only\n"
	"- sync: sync only\n"
	"- swap: swap only", "waitsync", NULL, GF_ARG_BOOL, 0),
	{0},
};


/**
 *	function DirectFBNewVideo
 * 	- creates a DirectFb module
 **/
void *DirectFBNewVideo()
{
	DirectFBVidCtx *ctx;
	GF_VideoOutput *driv;

	driv = gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "directfb", "gpac distribution");

	ctx = gf_malloc(DirectFBVid_GetCtxSizeOf());
	memset(ctx, 0, DirectFBVid_GetCtxSizeOf());

	/* GF_VideoOutput */
	driv->opaque = ctx;
	driv->Setup = DirectFBVid_Setup;
	driv->Shutdown = DirectFBVid_Shutdown;
	driv->Flush = DirectFBVid_Flush;
	driv->SetFullScreen = DirectFBVid_SetFullScreen;
	driv->ProcessEvent = DirectFBVid_ProcessEvent;
	driv->LockBackBuffer = DirectFBVid_LockBackBuffer;
	driv->LockOSContext = NULL;
	driv->Blit = DirectFBVid_Blit;
	driv->hw_caps |= GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_STRETCH;
	driv->args = DirectFBArgs;
	driv->description = "Video output using DirectFB";
	return driv;
}


/**
 *	function DirectFBDeleteVideo
 * 	- delete video
 **/
void DirectFBDeleteVideo(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *)ifce;
	DirectFBVID();
	gf_free(ctx);
	gf_free(driv);
}


/*interface query*/
GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		0
	};
	return si;
}


/*interface create*/
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return DirectFBNewVideo();
	return NULL;
}


/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DirectFBDeleteVideo(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( directfb_out )
