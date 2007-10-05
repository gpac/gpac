/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / DirectX audio and video render module
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



/*driver interfaces*/
#include <gpac/modules/video_out.h>
#include <gpac/list.h>
#include <gpac/constants.h>

typedef struct
{
	char *pixels;
	u32 width, height;
} RawContext;


#define RAW_OUT_PIXEL_FORMAT		GF_PIXEL_RGB_24
#define NBPP						3

#define RAWCTX	RawContext *rc = (RawContext *)dr->opaque

static GF_Err raw_resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	RAWCTX;

	if (rc->pixels) free(rc->pixels);
	rc->width = w;
	rc->height = h;
	rc->pixels = malloc(sizeof(char) * NBPP * w * h);
	if (!rc->pixels) return GF_OUT_OF_MEM;
	return GF_OK;
}

GF_Err RAW_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	raw_resize(dr, 100, 100);
	return GF_OK;
}


static void RAW_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;
	if (rc->pixels) free(rc->pixels);
	rc->pixels = NULL;
}


static GF_Err RAW_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	return GF_OK;
}

static GF_Err RAW_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	RAWCTX;
	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		vi->height = rc->height;
		vi->width = rc->width;
		vi->video_buffer = rc->pixels;
		vi->pitch = NBPP * vi->width;
		vi->pixel_format = RAW_OUT_PIXEL_FORMAT;
	}
	return GF_OK;
}

static GF_Err RAW_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	if (evt) {
		switch (evt->type) {
		case GF_EVENT_VIDEO_SETUP:
			if (evt->setup.opengl_mode) return GF_NOT_SUPPORTED;
			return raw_resize(dr, evt->setup.width, evt->setup.height);
		}
	}
	return GF_OK;
}

GF_VideoOutput *NewRawVideoOutput()
{
	RawContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Raw Video Output", "gpac distribution")

	pCtx = malloc(sizeof(RawContext));
	memset(pCtx, 0, sizeof(RawContext));

	driv->opaque = pCtx;

	driv->Flush = RAW_Flush;
	driv->LockBackBuffer = RAW_LockBackBuffer;
	driv->Setup = RAW_Setup;
	driv->Shutdown = RAW_Shutdown;
	driv->ProcessEvent = RAW_ProcessEvent;
	return (void *)driv;
}

void DeleteVideoOutput(void *ifce)
{
	RawContext *rc;
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;

	RAW_Shutdown(driv);
	rc = (RawContext *)driv->opaque;
	free(rc);
	free(driv);
}

#ifndef GPAC_STANDALONE_RENDER_2D

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewRawVideoOutput();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput((GF_VideoOutput *)ifce);
		break;
	}
}

#endif
