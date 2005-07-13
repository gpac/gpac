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

/*all surfaces are plain RGB 24*/
typedef struct
{
    char *pixels;
	u32 width, height;
	u32 id;
} RawSurface;

typedef struct
{
	GF_List *surfaces;
    
	char *pixels;
	u32 width, height;
} RawContext;




#define RAW_OUT_PIXEL_FORMAT		GF_PIXEL_ARGB
#define NBPP						4

#define RAWCTX	RawContext *rc = (RawContext *)dr->opaque

static void raw_resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	RAWCTX;

	if (rc->pixels) free(rc->pixels);
	rc->width = w;
	rc->height = h;
	rc->pixels = malloc(sizeof(char) * NBPP * w * h);
}

static RawSurface *raw_get_surface(GF_VideoOutput *dr, u32 id)
{
	u32 i;
	RawSurface *s;
	RAWCTX;
	for (i=0; i<gf_list_count(rc->surfaces); i++) {
		s = gf_list_get(rc->surfaces, i);
		if (s->id == id) return s;
	}
	return NULL;
}

GF_Err RAW_SetupHardware(GF_VideoOutput *dr, void *os_handle, void *os_display, Bool no_proc_override, GF_GLConfig *cfg)
{
	if (cfg) return GF_NOT_SUPPORTED;
	raw_resize(dr, 100, 100);
	return GF_OK;
}


static void RAW_Shutdown(GF_VideoOutput *dr)
{
	RAWCTX;

	while (gf_list_count(rc->surfaces)) {
		RawSurface *s = gf_list_get(rc->surfaces, 0);
		gf_list_rem(rc->surfaces, 0);
		if (s->pixels) free(s->pixels);
		free(s);
	}
	if (rc->pixels) free(rc->pixels);
	rc->pixels = NULL;
}


static GF_Err RAW_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	return GF_NOT_SUPPORTED;
}

static GF_Err RAW_FlushVideo(GF_VideoOutput *dr, GF_Window *dest)
{
	return GF_OK;
}

static GF_Err RAW_LockSurface(GF_VideoOutput *dr, u32 surface_id, GF_VideoSurface *vi)
{
	RawSurface *s;
	RAWCTX;

	if (!surface_id) {
		vi->height = rc->height;
		vi->width = rc->width;
		vi->video_buffer = rc->pixels;
	} else {
		s = raw_get_surface(dr, surface_id);
		if (!s) return GF_BAD_PARAM;
		vi->height = s->height;
		vi->width = s->width;
		vi->video_buffer = s->pixels;
	}
	vi->pitch = NBPP * vi->width;
	vi->os_handle = NULL;
	vi->pixel_format = RAW_OUT_PIXEL_FORMAT;
	return GF_OK;
}

static GF_Err RAW_UnlockSurface(GF_VideoOutput *dr, u32 surface_id)
{
	return GF_OK;
}

static GF_Err RAW_ClearSurface(GF_VideoOutput *dr, u32 surface_id, GF_Window *src, u32 color)
{
	GF_Err e;
	u32 i, j, endx, endy;
	u8 r, g, b;
	GF_VideoSurface s;

	r = (color>>16) & 0xFF;
	g = (color>>8) & 0xFF;
	b = (color) & 0xFF;

	e = RAW_LockSurface(dr, surface_id, &s);
	if (e) return e;

	i = j = 0;
	endx = s.width;
	endy = s.height;
	if (src) {
		j = src->y;
		i = src->x;
		endx = i + src->w;
		endy = j + src->h;
	}
	if (endx > s.width) endx = s.width;
	if (endy > s.height) endy = s.height;

	for (; j<endy; j++) {
		for (; i<endy; i+=NBPP) {
			s.video_buffer[j * s.pitch + i] = r;
			s.video_buffer[j * s.pitch + i + 1] = g;
			s.video_buffer[j * s.pitch + i + 2] = b;
		}
	}
	return GF_OK;
}

static GF_Err RAW_Clear(GF_VideoOutput *dr, u32 color)
{
	return RAW_ClearSurface(dr, 0, NULL, color);
}

static void *RAW_GetContext(GF_VideoOutput *dr, u32 surface_id)
{
	return NULL;
}

static void RAW_ReleaseContext(GF_VideoOutput *dr, u32 surface_id, void *context)
{
}


static GF_Err RAW_GetPixelFormat(GF_VideoOutput *dr, u32 surfaceID, u32 *pixel_format)
{
	*pixel_format = RAW_OUT_PIXEL_FORMAT;
	return GF_OK;
}

static GF_Err RAW_Blit(GF_VideoOutput *dr, u32 src_id, u32 dst_id, GF_Window *src, GF_Window *dst)
{
	return GF_NOT_SUPPORTED;
}

static GF_Err RAW_CreateSurface(GF_VideoOutput *dr, u32 width, u32 height, u32 pixel_format, u32 *surfaceID)
{
	RawSurface *s;
	RAWCTX;

	s = malloc(sizeof(RawSurface));
	s->height = height;
	s->width = width;
	s->id = (u32) s;
	s->pixels = malloc(sizeof(char) * NBPP * width * height);
	gf_list_add(rc->surfaces, s);
	*surfaceID = s->id;
	return GF_OK;
}


/*deletes video surface by id*/
static GF_Err RAW_DeleteSurface(GF_VideoOutput *dr, u32 surface_id)
{
	u32 i;
	RawSurface *s;
	RAWCTX;

	if (!surface_id) return GF_BAD_PARAM;

	for (i=0; i<gf_list_count(rc->surfaces); i++) {
		s = gf_list_get(rc->surfaces, i);
		if (s->id == surface_id) {
			gf_list_rem(rc->surfaces, i);
			if (s->pixels) free(s->pixels);
			free(s);
			return GF_OK;
		}
	}
	return GF_OK;
}

Bool RAW_IsSurfaceValid(GF_VideoOutput *dr, u32 surface_id)
{
	if (surface_id) return (raw_get_surface(dr, surface_id)==NULL) ? 0 : 1;
	return 1;
}


static GF_Err RAW_ResizeSurface(GF_VideoOutput *dr, u32 surface_id, u32 width, u32 height)
{
	RawSurface *s;

	if (!surface_id) {
		raw_resize(dr, width, height);
		return GF_OK;
	}
	s = raw_get_surface(dr, surface_id);
	if (!s) return GF_BAD_PARAM;

	if (s->pixels) free(s->pixels);
	s->width = width;
	s->height = height;
	s->pixels = malloc(sizeof(char) * NBPP * width * height);
	return GF_OK;
}


static GF_Err RAW_Resize(GF_VideoOutput *dr, u32 width, u32 height)
{
	return RAW_ResizeSurface(dr, 0, width, height);
}

static GF_Err RAW_PushEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	if (evt->type == GF_EVT_WINDOWSIZE) {
		return RAW_ResizeSurface(dr, 0, evt->size.width, evt->size.height);
	}
	return GF_OK;
}

void *NewVideoOutput()
{
	RawContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Raw Video Output", "gpac distribution", 0)

	pCtx = malloc(sizeof(RawContext));
	memset(pCtx, 0, sizeof(RawContext));
	pCtx->surfaces = gf_list_new();
	
	driv->opaque = pCtx;
	/*alpha and keying to do*/
	driv->bHasAlpha = 0;
	driv->bHasKeying = 0;
	driv->bHasYUV = 0;

	driv->Blit = RAW_Blit;
	driv->Clear = RAW_Clear;
	driv->CreateSurface = RAW_CreateSurface;
	driv->DeleteSurface = RAW_DeleteSurface;
	driv->FlushVideo = RAW_FlushVideo;
	driv->GetContext = RAW_GetContext;
	driv->GetPixelFormat = RAW_GetPixelFormat;
	driv->LockSurface = RAW_LockSurface;
	driv->ReleaseContext = RAW_ReleaseContext;
	driv->IsSurfaceValid = RAW_IsSurfaceValid;
	driv->Resize = RAW_Resize;
	driv->SetFullScreen = RAW_SetFullScreen;
	driv->SetupHardware = RAW_SetupHardware;
	driv->Shutdown = RAW_Shutdown;
	driv->UnlockSurface = RAW_UnlockSurface;
	driv->ResizeSurface	= RAW_ResizeSurface;

	driv->PushEvent = RAW_PushEvent;
	return (void *)driv;
}

static void DeleteVideoOutput(void *ifce)
{
	RawContext *rc;
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;

	RAW_Shutdown(driv);
	rc = (RawContext *)driv->opaque;
	gf_list_del(rc->surfaces);
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
void *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return NewVideoOutput();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(void *ifce)
{
	GF_VideoOutput *dd = (GF_VideoOutput *)ifce;
	switch (dd->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput(dd);
		break;
	}
}

#endif
