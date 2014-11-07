/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Romain Bouqueau @ GPAC Licensing
 *					All rights reserved
 *
 *  This file is part of GPAC / Dektec SDI video render module
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

#define _DTAPI_DISABLE_AUTO_LINK
#include <DTAPI.h>

extern "C" {

/*driver interfaces*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/constants.h>
#include <gpac/setup.h>


//#define DEKTEC_DUMP_UYVY

//TODO: use gf_stretch_bits and get values from config file
#define DWIDTH 1920
#define DHEIGHT 1080
#define DFORMAT DTAPI_IOCONFIG_1080I50
#define DNUMFIELDS 2


typedef struct
{
	char *pixels;
	unsigned char *pixels_UYVY;
	u32 width, height, pixel_format, bpp;

	DtDevice *dvc;
	DtFrameBuffer *dtf;
	bool isSending;
	s64 frameNum;
} DtContext;

//FIXME: referenced from dx
void dx_copy_pixels(GF_VideoSurface *dst_s, const GF_VideoSurface *src_s, const GF_Window *src_wnd);

static GF_Err Dektec_Blit(GF_VideoOutput *dr, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	DtContext *dtc = (DtContext*)dr->opaque;
	GF_VideoSurface temp_surf;
	dx_copy_pixels(&temp_surf, video_src, src_wnd);
	memset(&temp_surf, 0, sizeof(GF_VideoSurface));
	temp_surf.pixel_format = GF_PIXEL_UYVY;
	temp_surf.video_buffer = (char*)dtc->pixels_UYVY;
	temp_surf.pitch_y = dtc->width*2;
	dx_copy_pixels(&temp_surf, video_src, src_wnd);
	return GF_OK;
}

static GF_Err Dektec_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	DtContext *dtc = (DtContext*)dr->opaque;
	DtFrameBuffer *dtf = dtc->dtf;
	DTAPI_RESULT res;
	if (!dtc->isSending) {
		// Start transmission
		res = dtf->Start();
		if (res != DTAPI_OK) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MODULE, ("[Dektec Out] Can't start transmission: %s.\n", DtapiResult2Str(res)));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Dektec Out] Transmission started.\n"));
			dtc->isSending = true;
		}
	}
	s64 firstSafeFrame=-1, lastSafeFrame=-1;
	res = dtf->WaitFrame(firstSafeFrame, lastSafeFrame);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't get the next frame: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}
	
	if (dtc->frameNum == -1)
		dtc->frameNum = firstSafeFrame;

	int numLines=-1, numWritten=DWIDTH*DHEIGHT*2/DNUMFIELDS;
	res = dtf->WriteVideo(dtc->frameNum, dtc->pixels_UYVY, numWritten, DTAPI_SDI_FIELD1, DTAPI_SDI_8B, 1, numLines);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't write frame "LLD": %s\n", dtc->frameNum, DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}
	numLines=-1, numWritten=DWIDTH*DHEIGHT*2/DNUMFIELDS;
	res = dtf->WriteVideo(dtc->frameNum, dtc->pixels_UYVY+numWritten, numWritten, DTAPI_SDI_FIELD2, DTAPI_SDI_8B, 1, numLines);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't write frame "LLD": %s\n", dtc->frameNum, DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Dektec Out] Written frame "LLD" [(0x%X,0x%X,0x%X,0x%X)(0x%X,0x%X,0x%X,0x%X)(0x%X,0x%X,0x%X,0x%X)(0x%X,0x%X,0x%X,0x%X)]\n", dtc->frameNum, DWIDTH, DHEIGHT,
		dtc->pixels_UYVY[0], dtc->pixels_UYVY[1], dtc->pixels_UYVY[2], dtc->pixels_UYVY[3], dtc->pixels_UYVY[4], dtc->pixels_UYVY[5], dtc->pixels_UYVY[6], dtc->pixels_UYVY[7],
		dtc->pixels_UYVY[8], dtc->pixels_UYVY[9], dtc->pixels_UYVY[10], dtc->pixels_UYVY[11], dtc->pixels_UYVY[12], dtc->pixels_UYVY[13], dtc->pixels_UYVY[14], dtc->pixels_UYVY[15]));
	
	dtc->frameNum++;

#ifdef DEKTEC_DUMP_UYVY
	char szName[1024];
	sprintf(szName, "test.uyvy");
	FILE *f = fopen(szName, "ab");
	fwrite(dtc->pixels_UYVY, dtc->width*dtc->height*2, 1, f);
	fclose(f);
#endif
	return GF_OK;
}

static GF_Err Dektec_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	DtContext *dtc = (DtContext*)dr->opaque;

	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->height = dtc->height;
		vi->width = dtc->width;
		vi->video_buffer = dtc->pixels;
		vi->pitch_x = dtc->bpp;
		vi->pitch_y = dtc->bpp * vi->width;/*the correct value here is dtc->pixel_format, but the soft raster only supports RGB*/
		/*TODO: allocate a real RGB buffer, otherwise we will crash when displaying composed objects*/
		vi->pixel_format = GF_PIXEL_RGB_24;
	}
	return GF_OK;
}

static void Dektec_Shutdown(GF_VideoOutput *dr)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Dektec Out] Dektec_Shutdown\n"));
	DtContext *dtc = (DtContext*)dr->opaque;
	DtFrameBuffer *dtf = dtc->dtf;
	DTAPI_RESULT res = dtf->Start(false);
	if (res != DTAPI_OK)
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't stop transmission: %s\n", DtapiResult2Str(res)));
  dtf->Detach();
  dtc->dvc->Detach();
	dtc->isSending = false;
}

static GF_Err Dektec_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	DtFrameBuffer *dtf = (DtFrameBuffer*)(((DtContext*)dr->opaque)->dtf);

	if (evt) {
		switch (evt->type) {
		case GF_EVENT_VIDEO_SETUP:
			//FIXME: we do not know how to resize
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Dektec Out] resize (%ux%u) received.\n", evt->size.width, evt->size.height));
			//Dektec_Shutdown(dr);
			//Dektec_attach_start(dr);
			if (evt->size.width != DWIDTH || evt->size.height != DHEIGHT) {
				GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Dektec Out] Bad resize (%ux%u) received. Expected %dx%d.\n", evt->size.width, evt->size.height, DWIDTH, DHEIGHT));
				//return GF_BAD_PARAM;
				if (evt->size.width*evt->size.height > DWIDTH*DHEIGHT)
					return GF_BAD_PARAM;
			}
			return GF_OK;
		}
	}
	return GF_OK;
}

static GF_Err Dektec_resize(GF_VideoOutput *dr, u32 w, u32 h)
{
	DtContext *dtc = (DtContext*)dr->opaque;
	if (dtc->pixels) gf_free(dtc->pixels);
	if (dtc->pixels_UYVY) gf_free(dtc->pixels_UYVY);
	dtc->width = w;
	dtc->height = h;
	dtc->pixels = (char*)gf_malloc(dtc->bpp * w * h);
	if (!dtc->pixels) return GF_OUT_OF_MEM;
	memset(dtc->pixels, 0, dtc->bpp * w * h);
	dtc->pixels_UYVY = (unsigned char*)gf_malloc(2 * w * h);
	if (!dtc->pixels_UYVY) return GF_OUT_OF_MEM;
	memset(dtc->pixels_UYVY, 0, 2 * w * h);
	return GF_OK;
}

GF_Err Dektec_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MODULE, ("[Dektec Out] Dektec_Setup\n"));
	DtContext *dtc = (DtContext*)dr->opaque;
	dtc->bpp = 3; //needed because lock_buffer expects RGB
	dtc->pixel_format = GF_PIXEL_YV12;
	dtc->isSending = false;
	Dektec_resize(dr, DWIDTH, DHEIGHT);
	
	int port = 1;
	const char *opt;
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "DektecVideo", "SDIOutput");
	if (opt) {
		port = atoi(opt);
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MODULE, ("[Dektec Out] No port specified, using default port: %d\n", port));
	}

	// Attach device and output channel objects to hardware
	DtDevice *dvc = dtc->dvc;
	DtFrameBuffer *dtf = dtc->dtf;
	DTAPI_RESULT res;
	res = dvc->AttachToType(2154);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] No DTA-2154 in system: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}	
	
  res = dvc->SetIoConfig(port, DTAPI_IOCONFIG_IODIR, DTAPI_IOCONFIG_OUTPUT, DTAPI_IOCONFIG_OUTPUT);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't set I/O config for the device: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}
	
  res = dtf->AttachToOutput(dvc, port, 0);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't attach to port %d: %s\n", port, DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	int IoStdValue=-1, IoStdSubValue=-1;
	res = DtapiVidStd2IoStd(DFORMAT, IoStdValue, IoStdSubValue);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Unknown VidStd: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	res = dtf->SetIoConfig(DTAPI_IOCONFIG_IOSTD, IoStdValue, IoStdSubValue);
	if (res != DTAPI_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] Can't set I/O config: %s\n", DtapiResult2Str(res)));
		return GF_BAD_PARAM;
	}

	dtc->frameNum = -1;

	return GF_OK;
}

GF_VideoOutput *NewDektecVideoOutput()
{
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "Dektec Video Output", "gpac distribution")
		
	DtDevice *dvc = new DtDevice;
	DtFrameBuffer *dtf = new DtFrameBuffer;
	if (!dtf || !dvc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Dektec Out] DTA API couldn't be initialized.\n"));
		delete dvc;
		delete dtf;
		gf_free(driv);
		return NULL;
	}

	DtContext *dtc = new DtContext;
	memset(dtc, 0, sizeof(DtContext));
	dtc->dvc = dvc;
	dtc->dtf = dtf;
	driv->opaque = (void*)dtc;

	driv->Flush = Dektec_Flush;
	driv->LockBackBuffer = Dektec_LockBackBuffer;
	driv->Setup = Dektec_Setup;
	driv->Shutdown = Dektec_Shutdown;
	driv->ProcessEvent = Dektec_ProcessEvent;
	driv->Blit = Dektec_Blit;

	driv->hw_caps |= GF_VIDEO_HW_HAS_YUV_OVERLAY | GF_VIDEO_HW_HAS_YUV;

	return driv;
}

void DeleteDektecVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	DtContext *dtc = (DtContext*)driv->opaque;
	gf_free(dtc->pixels);
	gf_free(dtc->pixels_UYVY);
	delete(dtc->dtf);
	delete(dtc->dvc);
	delete(dtc);
	gf_free(driv);
}


#ifndef GPAC_STANDALONE_RENDER_2D

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
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewDektecVideoOutput();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteDektecVideoOutput((GF_VideoOutput *)ifce);
		break;
	}
}


GPAC_MODULE_STATIC_DECLARATION( dektec_out )

#endif

} /* extern "C" */
