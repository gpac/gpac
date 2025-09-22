/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Ivica Arsov, Jean Le Feuvre
 *			Copyright (c) Mines-Telecom 2009-
 *					All rights reserved
 *
 *  This file is part of GPAC / Wrapper
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

#include <gpac/setup.h>

#include <android/bitmap.h>

typedef struct
{
	JNIEnv * env;
	jobject * bitmap;
	u32 width, height;
	void * locked_data;
} AndroidContext;


#define RAW_OUT_PIXEL_FORMAT		GF_PIXEL_RGB_32
#define NBPP						4

#define RAWCTX	AndroidContext *rc = (AndroidContext *)dr->opaque

static GF_Err raw_resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout raw_resize\n"));
	return GF_OK;
}

GF_Err RAW_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	AndroidBitmapInfo  info;
	RAWCTX;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout RAW_Setup\n"));

	if (!rc->width)
	{
		rc->env = (JNIEnv *)os_handle;
		rc->bitmap = (jobject *)os_display;

		AndroidBitmap_getInfo(rc->env, *(rc->bitmap), &info);
		rc->width = info.width;
		rc->height = info.height;
		rc->locked_data = NULL;
	}
	else
	{
		rc->env = (JNIEnv *)os_handle;
		rc->bitmap = (jobject *)os_display;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout rc dims: %d:%d\n", rc->height, rc->width));

	return GF_OK;
}


static void RAW_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout RAW_Shutdown\n"));
	rc->bitmap = NULL;
}


static GF_Err RAW_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout RAW_Flush\n"));
	return GF_OK;
}

static GF_Err RAW_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	RAWCTX;
	void * pixels;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout RAW_LockBackBuffer: %d\n", do_lock));
	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout locked_data: %d\n", rc->locked_data));
		if (!rc->locked_data)
		{
			int ret;
			if ((ret = AndroidBitmap_lockPixels(rc->env, *(rc->bitmap), &pixels)) < 0) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout lock failed (%d)\n", ret));
			}
			rc->locked_data = pixels;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout rc dims: %d:%d\n", rc->height, rc->width));
		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->height = rc->height;
		vi->width = rc->width;
		vi->video_buffer = rc->locked_data;
		vi->is_hardware_memory = 0;
		vi->pitch_x = NBPP;
		vi->pitch_y = NBPP * vi->width;
		vi->pixel_format = RAW_OUT_PIXEL_FORMAT;
	}
	else
	{
		if (rc->locked_data)
		{
			AndroidBitmap_unlockPixels(rc->env, *(rc->bitmap));
			rc->locked_data = NULL;
		}
	}
	return GF_OK;
}

static GF_Err RAW_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout RAW_ProcessEvent\n"));
	if (evt) {
		switch (evt->type) {
		case GF_EVENT_SIZE:
			//if (evt->setup.opengl_mode) return GF_OK;
			return raw_resize(dr, evt->setup.width, evt->setup.height);
		case GF_EVENT_VIDEO_SETUP:
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_VideoOutput *NewRawVideoOutput()
{
	AndroidContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "androidbmp", "gpac distribution")

	pCtx = gf_malloc(sizeof(AndroidContext));
	memset(pCtx, 0, sizeof(AndroidContext));

	driv->opaque = pCtx;

	driv->Flush = RAW_Flush;
	driv->LockBackBuffer = RAW_LockBackBuffer;
	driv->Setup = RAW_Setup;
	driv->Shutdown = RAW_Shutdown;
	driv->ProcessEvent = RAW_ProcessEvent;

	driv->hw_caps = GF_VIDEO_HW_OPENGL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout init\n"));
	return (void *)driv;
}

void DeleteVideoOutput(void *ifce)
{
	AndroidContext *rc;
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;

	RAW_Shutdown(driv);
	rc = (AndroidContext *)driv->opaque;
	gf_free(rc);
	gf_free(driv);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Android vout deinit\n"));
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
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewRawVideoOutput();
	return NULL;
}
/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput((GF_VideoOutput *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( droid_vidbmp )
