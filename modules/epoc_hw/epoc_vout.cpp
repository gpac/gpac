/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / EPOC video output module
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

/*driver interface*/
#include <gpac/modules/video_out.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/codec.h>

#include <w32std.h>

#ifdef GPAC_USE_OGL_ES
#include <GLES/egl.h>
#endif

typedef struct
{
	RWindow *window;
	RWsSession *session;

	u32 pixel_format, bpp, width, height;
	CWsScreenDevice *screen;
	CFbsBitmap *surface;
	CWindowGc *gc;

	char *locked_data;
	u32 output_3d_type;

#ifdef GPAC_USE_OGL_ES
	EGLDisplay egl_display;
	EGLSurface egl_surface;
	EGLContext egl_context;
#endif

} EPOCVideo;


static void EVID_ResetSurface(GF_VideoOutput *dr, Bool gl_only)
{
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;

#ifdef GPAC_USE_OGL_ES
	if (ctx->egl_display) {
		eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (ctx->egl_context) eglDestroyContext(ctx->egl_display, ctx->egl_context);
		ctx->egl_context = NULL;
		if (ctx->egl_surface) eglDestroySurface(ctx->egl_display, ctx->egl_surface); 
		ctx->egl_surface = NULL; 
		eglTerminate(ctx->egl_display);
		ctx->egl_display = NULL;
	}
#endif
	if (gl_only) return;

	if (ctx->locked_data) ctx->surface->UnlockHeap();
	ctx->locked_data = NULL;
	if (ctx->surface) delete ctx->surface;
	ctx->surface = NULL;
	if (ctx->gc) delete ctx->gc;
	ctx->gc = NULL;
	if (ctx->screen) delete ctx->screen;
	ctx->screen = NULL;
}

static GF_Err EVID_InitSurface(GF_VideoOutput *dr)
{
	TInt gl_buffer_size;
	TInt e;
	TDisplayMode disp_mode;
	TSize s;
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Reseting video\n"));
	EVID_ResetSurface(dr, 0);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Video reset OK\n"));

	ctx->screen = new CWsScreenDevice(*ctx->session);
	if (!ctx->screen) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create screen device for session\n"));
		return GF_IO_ERR;
	}
 	e = ctx->screen->Construct();
	if (e != KErrNone) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot construct screen device for session - error %d\n", e));
		return GF_IO_ERR;
	}
 	e = ctx->screen->CreateContext(ctx->gc);
	if (e != KErrNone) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create graphical context - error %d\n", e));
		return GF_IO_ERR;
	}

	ctx->surface = new CFbsBitmap();
	if (!ctx->surface) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot allocate backbuffer surface\n"));
		return GF_IO_ERR;
	}

	s = ctx->window->Size();
	disp_mode = ctx->screen->DisplayMode();
	e = ctx->surface->Create(s, disp_mode);
	if (e != KErrNone) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create backbuffer surface - error %d\n", e));
		return GF_IO_ERR;
	}

	gl_buffer_size = 0;
	switch (disp_mode) {
	case EGray256: 
		ctx->pixel_format = GF_PIXEL_GREYSCALE; 
		ctx->bpp = 1;
		break;
	case EColor64K: 
		ctx->pixel_format = GF_PIXEL_RGB_565; 
		ctx->bpp = 2;
		gl_buffer_size = 16;
		break;
	case EColor16M: 
		ctx->pixel_format = GF_PIXEL_RGB_24; 
		ctx->bpp = 3;
		gl_buffer_size = 24;
		break;
	/** 4096 colour display (12 bpp). */
	case EColor4K: 
		ctx->pixel_format = GF_PIXEL_RGB_444; 
		ctx->bpp = 2;
		gl_buffer_size = 12;
		break;
	/** True colour display mode (32 bpp, but top byte is unused and unspecified) */
	case EColor16MU: 
		ctx->pixel_format = GF_PIXEL_RGB_32; 
		ctx->bpp = 4;
		gl_buffer_size = 32;
		break;
#if defined(__SERIES60_3X__)
	/** Display mode with alpha (24bpp colour plus 8bpp alpha) */
	case EColor16MA: 
		ctx->pixel_format = GF_PIXEL_ARGB; 
		ctx->bpp = 4;
		gl_buffer_size = 32;
		break;
#endif
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Unsupported display type %d\n", disp_mode));
		return GF_NOT_SUPPORTED;
	}
	ctx->width = s.iWidth;
	ctx->height = s.iHeight;

#ifdef GPAC_USE_OGL_ES
	if (ctx->output_3d_type==1) {
		if (!gl_buffer_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Display mode not supported by OpenGL\n"));
			return GF_IO_ERR;
		}
		ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (ctx->egl_display == NULL) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot open OpenGL display\n"));
			return GF_IO_ERR;
		}

		if (eglInitialize(ctx->egl_display, NULL, NULL) == EGL_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot initialize OpenGL display\n"));
			return GF_IO_ERR;
		}
		EGLConfig *configList = NULL;
		EGLint numOfConfigs = 0; 
		EGLint configSize   = 0;
		if (eglGetConfigs(ctx->egl_display, configList, configSize, &numOfConfigs) == EGL_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot retrieve OpenGL configurations\n"));
			return GF_IO_ERR;
		}
	    configSize = numOfConfigs;
	    configList = (EGLConfig*) malloc(sizeof(EGLConfig)*configSize);

		// Define properties for the wanted EGLSurface 
		EGLint atts[7];
		const char *opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");

		atts[0] = EGL_BUFFER_SIZE; atts[1] = gl_buffer_size; 
		atts[2] = EGL_DEPTH_SIZE; atts[3] = opt ? atoi(opt) : 16;
		atts[4] = EGL_SURFACE_TYPE; atts[5] = EGL_PIXMAP_BIT;
		atts[6] = EGL_NONE;

		if (eglChooseConfig(ctx->egl_display, atts, configList, configSize, &numOfConfigs) == EGL_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot choose OpenGL configuration\n"));
			return GF_IO_ERR;
		}
		EGLConfig gl_cfg = configList[0];
		free(configList);

		ctx->egl_surface = eglCreatePixmapSurface(ctx->egl_display, gl_cfg, ctx->surface, NULL);
		if (ctx->egl_surface == NULL) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create OpenGL surface\n"));
			return GF_IO_ERR;
		}
		ctx->egl_context = eglCreateContext(ctx->egl_display, gl_cfg, EGL_NO_CONTEXT, NULL);
		if (ctx->egl_context == NULL) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create OpenGL context\n"));
			return GF_IO_ERR;
		}
		if (eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface, ctx->egl_context) == EGL_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot bind OpenGL context to current thread\n"));
			return GF_IO_ERR;
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Video OpenGL setup\n"));
	}

#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Video setup OK - %d x %d @ PixelFormat %s\n", ctx->width, ctx->height, gf_4cc_to_str(ctx->pixel_format) ));
	return GF_OK;
}

#ifdef GPAC_USE_OGL_ES

static GF_Err EVID_SetupOGL_ES_Offscreen(GF_VideoOutput *dr, u32 width, u32 height)
{
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;
	EVID_ResetSurface(dr, 1);
	if (!ctx->screen) return GF_NOT_SUPPORTED;

	TDisplayMode disp_mode = ctx->screen->DisplayMode();
	TInt gl_buffer_size = 0;
	switch (disp_mode) {
	case EColor64K: gl_buffer_size = 16; break;
	case EColor16M: gl_buffer_size = 24; break;
	/** 4096 colour display (12 bpp). */
	case EColor4K: gl_buffer_size = 12; break;
	/** True colour display mode (32 bpp, but top byte is unused and unspecified) */
	case EColor16MU: gl_buffer_size = 32; break;
#if defined(__SERIES60_3X__)
	/** Display mode with alpha (24bpp colour plus 8bpp alpha) */
	case EColor16MA: gl_buffer_size = 32; break;
#endif
	case EGray256:
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Unsupported display type %d for OpenGL\n", disp_mode));
		return GF_NOT_SUPPORTED;
	}

	ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (ctx->egl_display == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot open OpenGL display\n"));
		return GF_IO_ERR;
	}

	if (eglInitialize(ctx->egl_display, NULL, NULL) == EGL_FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot initialize OpenGL display\n"));
		return GF_IO_ERR;
	}

	EGLConfig *configList = NULL;
	EGLint numOfConfigs = 0; 
	EGLint configSize   = 0;
	if (eglGetConfigs(ctx->egl_display, configList, configSize, &numOfConfigs) == EGL_FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot retrieve OpenGL configurations\n"));
		return GF_IO_ERR;
	}
	configSize = numOfConfigs;
	configList = (EGLConfig*) malloc(sizeof(EGLConfig)*configSize);

	// Define properties for the wanted EGLSurface 
	EGLint atts[13];
	const char *opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	atts[0] = EGL_RED_SIZE;		atts[1] = 8;
	atts[2] = EGL_GREEN_SIZE;	atts[3] = 8;
	atts[4] = EGL_BLUE_SIZE;	atts[5] = 8;
	atts[6] = EGL_ALPHA_SIZE;	atts[7] = (dr->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA) ? 8 : EGL_DONT_CARE;
	atts[8] = EGL_SURFACE_TYPE; atts[9] = EGL_PBUFFER_BIT;
	atts[10] = EGL_DEPTH_SIZE; atts[11] = opt ? atoi(opt) : 16;
	atts[12] = EGL_NONE;

	if (eglChooseConfig(ctx->egl_display, atts, configList, configSize, &numOfConfigs) == EGL_FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot choose Offscreen OpenGL configuration\n"));
		return GF_IO_ERR;
	}
	EGLConfig gl_cfg = configList[0];
	free(configList);

	atts[0] = EGL_WIDTH; atts[1] = width;
	atts[2] = EGL_HEIGHT; atts[3] = height;
	atts[4] = EGL_NONE;

	ctx->egl_surface = eglCreatePbufferSurface(ctx->egl_display, gl_cfg, atts);
	if (ctx->egl_surface == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create OpenGL Pbuffer\n"));
		return GF_IO_ERR;
	}
	ctx->egl_context = eglCreateContext(ctx->egl_display, gl_cfg, EGL_NO_CONTEXT, NULL);
	if (ctx->egl_context == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot create Offscreen OpenGL context\n"));
		return GF_IO_ERR;
	}
	if (eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface, ctx->egl_context) == EGL_FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[EPOC Video] Cannot bind Offscreen OpenGL context to current thread\n"));
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Offscreen OpenGL setup - size %d x %d\n", width, height));
	return GF_OK;
}
#endif


static GF_Err EVID_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	GF_Err res;
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;

	ctx->window = (RWindow *)os_handle;
	ctx->session = (RWsSession *)os_display;

	res = EVID_InitSurface(dr);

	/*setup opengl offscreen*/
#ifdef GPAC_USE_OGL_ES
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Querying Offscreen OpenGL Capabilities\n"));
	dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
	GF_Err e = EVID_SetupOGL_ES_Offscreen(dr, 20, 20);
	if (e!=GF_OK) {
		dr->hw_caps &= ~GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
		e = EVID_SetupOGL_ES_Offscreen(dr, 20, 20);
	}
	if (!e) {
		dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Offscreen OpenGL available - alpha support: %s\n", (dr->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA) ? "yes" : "no"));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Offscreen OpenGL not available\n"));
	}
#endif

	return res;
}

static void EVID_Shutdown(GF_VideoOutput *dr)
{
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;
	EVID_ResetSurface(dr, 0);
	ctx->session = NULL;
	ctx->window = NULL;
}

static GF_Err EVID_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	//EPOCVideo *ctx = (EPOCVideo *)dr->opaque;
	return GF_NOT_SUPPORTED;
}

static GF_Err EVID_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;

	if (ctx->gc && ctx->surface) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Activating window\n"));
		ctx->gc->Activate(*ctx->window);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Bliting backbuffer\n"));
		ctx->gc->BitBlt(TPoint(0,0), ctx->surface);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Deactivating window\n"));
		ctx->gc->Deactivate();
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Flush success\n"));
	}
	return GF_OK;
}

static GF_Err EVID_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	//EPOCVideo *ctx = (EPOCVideo *)dr->opaque;
	if (!evt) return GF_OK;

	switch (evt->type) {
	case GF_EVENT_SHOWHIDE:
		break;
	case GF_EVENT_SIZE:
		/*nothing to do since we don't own the window*/
		break;
	case GF_EVENT_VIDEO_SETUP:
		((EPOCVideo *)dr->opaque)->output_3d_type = evt->setup.opengl_mode;
		if (evt->setup.opengl_mode==2) {
#ifdef GPAC_USE_OGL_ES
			return EVID_SetupOGL_ES_Offscreen(dr, evt->setup.width, evt->setup.height);
#else
			return GF_NOT_SUPPORTED;
#endif
		}
		return EVID_InitSurface(dr/*, evt->size.width, evt->size.height*/);
	}
	return GF_OK;
}

static GF_Err EVID_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	EPOCVideo *ctx = (EPOCVideo *)dr->opaque;
	if (!ctx->surface) return GF_BAD_PARAM;

	if (do_lock) {
		if (!ctx->locked_data) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Locking backbuffer memory\n"));
			ctx->surface->LockHeap();
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Retrieving backbuffer memory address\n"));
			ctx->locked_data = (char *)ctx->surface->DataAddress();
		}
		vi->height = ctx->height;
		vi->width = ctx->width;
		vi->is_hardware_memory = 0;
		vi->pitch = ctx->width * ctx->bpp;
		vi->pixel_format = ctx->pixel_format;
		vi->video_buffer = ctx->locked_data;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Backbuffer locked OK - address 0x%08x\n", ctx->locked_data));
	} else {
		if (ctx->locked_data) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Unlocking backbuffer memory\n"));
			ctx->surface->UnlockHeap();
			ctx->locked_data = NULL;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[EPOC Video] Backbuffer unlocked OK\n"));
		}
	}
	return GF_OK;
}

static void *EPOC_vout_new()
{
	EPOCVideo *priv;
	GF_VideoOutput *driv;
	GF_SAFEALLOC(driv, GF_VideoOutput);
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "EPOC Video Output", "gpac distribution")

	GF_SAFEALLOC(priv, EPOCVideo);
	driv->opaque = priv;

	/*alpha and keying to do*/
	driv->hw_caps = 0;
#ifdef GPAC_USE_OGL_ES
	/*no offscreen opengl with epoc at the moment*/
	driv->hw_caps |= GF_VIDEO_HW_OPENGL;
#endif

	driv->Setup = EVID_Setup;
	driv->Shutdown = EVID_Shutdown;
	driv->Flush = EVID_Flush;
	driv->ProcessEvent = EVID_ProcessEvent;
	driv->Blit = NULL;
	driv->LockBackBuffer = EVID_LockBackBuffer;
	driv->SetFullScreen = EVID_SetFullScreen;
	return (void *)driv;
}
static void EPOC_vout_del(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	EPOCVideo *priv = (EPOCVideo *)driv->opaque;
	free(priv);
	free(driv);
}

#ifdef __cplusplus
extern "C" {
#endif


void *EPOC_aout_new();
void EPOC_aout_del(void *ifce);

void EPOC_codec_del(void *ifcg);
void *EPOC_codec_new();
	
/*interface query*/
GF_EXPORT
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return 1;
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) EPOC_vout_new();
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return (GF_BaseInterface *) EPOC_aout_new();
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *) EPOC_codec_new();
	return NULL;
}
/*interface destroy*/
GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VideoOutput *dd = (GF_VideoOutput *)ifce;
	switch (dd->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		EPOC_vout_del(dd);
		break;
	case GF_AUDIO_OUTPUT_INTERFACE:
		EPOC_aout_del(ifce);
		break;
	case GF_MEDIA_DECODER_INTERFACE:
		EPOC_codec_del(ifce);
		break;
	}
}

#ifdef __cplusplus
}
#endif

