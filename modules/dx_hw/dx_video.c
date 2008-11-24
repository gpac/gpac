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


#include "dx_hw.h"

#include <gpac/user.h>

#ifdef _WIN32_WCE
#ifdef GPAC_USE_OGL_ES
#endif
#else
#include <GL/gl.h>
#endif

#define DDCONTEXT	DDContext *dd = (DDContext *)dr->opaque;


static void RestoreWindow(DDContext *dd) 
{
	if (!dd->NeedRestore) return;

	dd->NeedRestore = 0;
	if (dd->output_3d_type==1) {
#ifndef _WIN32_WCE
		ChangeDisplaySettings(NULL,0);
#endif
	} else {
#ifdef USE_DX_3
		IDirectDraw_SetCooperativeLevel(dd->pDD, dd->cur_hwnd, DDSCL_NORMAL);
#else
		IDirectDraw7_SetCooperativeLevel(dd->pDD, dd->cur_hwnd, DDSCL_NORMAL);
#endif
		dd->NeedRestore = 0;
	}

//	SetForegroundWindow(dd->cur_hwnd);
	SetFocus(dd->cur_hwnd);
}

void DestroyObjectsEx(DDContext *dd, Bool only_3d)
{
	if (!only_3d) {
		RestoreWindow(dd);

		SAFE_DD_RELEASE(dd->rgb_pool.pSurface);
		memset(&dd->rgb_pool, 0, sizeof(DDSurface));
		SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
		memset(&dd->yuv_pool, 0, sizeof(DDSurface));

		SAFE_DD_RELEASE(dd->pPrimary);
		SAFE_DD_RELEASE(dd->pBack);
		SAFE_DD_RELEASE(dd->pDD);
		dd->ddraw_init = 0;

		/*do not destroy associated GL context*/
		if (dd->output_3d_type==2) return;
	}

	/*delete openGL context*/
#ifdef GPAC_USE_OGL_ES
	if (dd->eglctx) eglDestroyContext(dd->egldpy, dd->eglctx);
	dd->eglctx = NULL;
	if (dd->surface) eglDestroySurface(dd->egldpy, dd->surface);
	dd->surface = NULL;
	if (dd->gl_HDC) {
		if (dd->egldpy) eglTerminate(dd->egldpy);
		ReleaseDC(dd->cur_hwnd, (HDC) dd->gl_HDC);
		dd->gl_HDC = 0L;
		dd->egldpy = NULL;
	}
#elif !defined(_WIN32_WCE) 
	if (dd->gl_HRC) {
		wglMakeCurrent(dd->gl_HDC, NULL);
		wglDeleteContext(dd->gl_HRC);
		dd->gl_HRC = NULL;
	}
	if (dd->gl_HDC) {
		ReleaseDC(dd->cur_hwnd, dd->gl_HDC);
		dd->gl_HDC = NULL;
	}
#endif
}

void DestroyObjects(DDContext *dd)
{
	DestroyObjectsEx(dd, 0);
}

GF_Err DD_SetupOpenGL(GF_VideoOutput *dr) 
{
	const char *sOpt;
	GF_Event evt;
	DDCONTEXT

#ifdef GPAC_USE_OGL_ES
	EGLint major, minor;
	EGLint n;
	EGLConfig configs[1];
	u32 nb_bits;
	u32 i=0;
	static int egl_atts[20];

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsPerComponent");
	nb_bits = sOpt ? atoi(sOpt) : 5;

	egl_atts[i++] = EGL_RED_SIZE; egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_GREEN_SIZE; egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_BLUE_SIZE; egl_atts[i++] = nb_bits;
	/*alpha for compositeTexture*/
	egl_atts[i++] = EGL_ALPHA_SIZE; egl_atts[i++] = 1;
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	nb_bits = sOpt ? atoi(sOpt) : 5;
	egl_atts[i++] = EGL_DEPTH_SIZE; egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_STENCIL_SIZE; egl_atts[i++] = EGL_DONT_CARE;
	egl_atts[i++] = EGL_NONE;

	/*already setup*/
	DestroyObjects(dd);
	dd->gl_HDC = (NativeDisplayType) GetDC(dd->cur_hwnd);
	dd->egldpy = eglGetDisplay(/*dd->gl_HDC*/ EGL_DEFAULT_DISPLAY);
	if (!eglInitialize(dd->egldpy, &major, &minor)) return GF_IO_ERR;
	if (!eglChooseConfig(dd->egldpy, egl_atts, configs, 1, &n)) return GF_IO_ERR;
	dd->eglconfig = configs[0];
	dd->surface = eglCreateWindowSurface(dd->egldpy, dd->eglconfig, dd->cur_hwnd, 0);
	if (!dd->surface) return GF_IO_ERR; 
	dd->eglctx = eglCreateContext(dd->egldpy, dd->eglconfig, NULL, NULL);
	if (!dd->eglctx) {
		eglDestroySurface(dd->egldpy, dd->surface);
		dd->surface = 0L;
		return GF_IO_ERR; 
	}
    if (!eglMakeCurrent(dd->egldpy, dd->surface, dd->surface, dd->eglctx)) {
		eglDestroyContext(dd->egldpy, dd->eglctx);
		dd->eglctx = 0L;
		eglDestroySurface(dd->egldpy, dd->surface);
		dd->surface = 0L;
		return GF_IO_ERR;
	}
#elif !defined(_WIN32_WCE)
    PIXELFORMATDESCRIPTOR pfd; 
    s32 pixelformat; 

	/*already setup*/
//	if (dd->gl_HRC) return GF_OK;
	DestroyObjectsEx(dd, (dd->output_3d_type==1) ? 0 : 1);

	if (dd->output_3d_type==2) {
		dd->gl_HDC = GetDC(dd->gl_hwnd);
	} else {
		dd->gl_HDC = GetDC(dd->cur_hwnd);
	}
	if (!dd->gl_HDC) return GF_IO_ERR;

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
	if (dd->gl_double_buffer) 
		pfd.dwFlags |= PFD_DOUBLEBUFFER;
	else {
		sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "UseGLDoubleBuffering");
		if (sOpt && !strcmp(sOpt, "yes")) pfd.dwFlags |= PFD_DOUBLEBUFFER;
	}

    pfd.dwLayerMask = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	pfd.cDepthBits = sOpt ? atoi(sOpt) : 16;
	/*we need alpha support for composite textures...*/
	pfd.cAlphaBits = 8;
    if ( (pixelformat = ChoosePixelFormat(dd->gl_HDC, &pfd)) == FALSE ) return GF_IO_ERR; 
    if (SetPixelFormat(dd->gl_HDC, pixelformat, &pfd) == FALSE) 
		return GF_IO_ERR; 
	dd->gl_HRC = wglCreateContext(dd->gl_HDC);
	if (!dd->gl_HRC) return GF_IO_ERR;
	if (!wglMakeCurrent(dd->gl_HDC, dd->gl_HRC)) return GF_IO_ERR;
#endif
	if (dd->output_3d_type==1) {
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.opengl_mode = 1;
		dr->on_event(dr->evt_cbk_hdl, &evt);	
	}
	return GF_OK;
}




GF_Err DD_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	RECT rc;
	DDCONTEXT
	dd->os_hwnd = (HWND) os_handle;
	
	//if (init_flags & (GF_TERM_NO_VISUAL_THREAD | GF_TERM_NO_REGULATION) ) dd->systems_memory = 2;
	DD_SetupWindow(dr, init_flags);
	/*fatal error*/
	if (!dd->os_hwnd) return GF_IO_ERR;
	dd->cur_hwnd = dd->os_hwnd;

	dd->output_3d_type = 0;
	GetWindowRect(dd->cur_hwnd, &rc);
	return InitDirectDraw(dr, rc.right - rc.left, rc.bottom - rc.top);
}

static void DD_Shutdown(GF_VideoOutput *dr)
{
	DDCONTEXT

	/*force destroy of opengl*/
	if (dd->output_3d_type) dd->output_3d_type = 1;
	DestroyObjects(dd);

	DD_ShutdownWindow(dr);
}

static GF_Err DD_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	GF_Err e;
	const char *sOpt;
	u32 MaxWidth, MaxHeight;
	DDCONTEXT;

	if (!dd->width ||!dd->height) return GF_BAD_PARAM;
	if (bOn == dd->fullscreen) return GF_OK;
	if (!dd->fs_hwnd) return GF_NOT_SUPPORTED;
	dd->fullscreen = bOn;
	
	/*whenever changing card display mode relocate fastest YUV format for blit (since it depends
	on the dest pixel format)*/
	dd->yuv_init = 0;
	if (dd->fullscreen) {
		const char *sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "SwitchResolution");
		if (sOpt && !stricmp(sOpt, "yes")) dd->switch_res = 1;
		/*get current or best fitting mode*/
		if (GetDisplayMode(dd) != GF_OK) return GF_IO_ERR;
	}

	MaxWidth = MaxHeight = 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "MaxResolution");
	if (sOpt) sscanf(sOpt, "%dx%d", &MaxWidth, &MaxHeight);

	/*destroy all objects*/
	DestroyObjects(dd);
	if (dd->timer) KillTimer(dd->cur_hwnd, dd->timer);
	dd->timer = 0;
	ShowWindow(dd->cur_hwnd, SW_HIDE);
	dd->cur_hwnd = dd->fullscreen ? dd->fs_hwnd : dd->os_hwnd;
	ShowWindow(dd->cur_hwnd, SW_SHOW);

	if (dd->output_3d_type==1) {
		DEVMODE settings;
		e = GF_OK;
		/*Setup FS*/
		if (dd->fullscreen) {
			/*change display mode*/
			if ((MaxWidth && (dd->fs_width >= MaxWidth)) || (MaxHeight && (dd->fs_height >= MaxHeight)) ) {
				dd->fs_width = MaxWidth;
				dd->fs_height = MaxHeight;
			}
			SetWindowPos(dd->cur_hwnd, NULL, 0, 0, dd->fs_width, dd->fs_height, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS);

#ifndef _WIN32_WCE
			memset(&settings, 0, sizeof(DEVMODE));
			settings.dmSize = sizeof(DEVMODE);
			settings.dmPelsWidth = dd->fs_width;
			settings.dmPelsHeight = dd->fs_height;
			settings.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
			if ( ChangeDisplaySettings(&settings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DirectDraw] cannot change display settings\n"));
				e = GF_IO_ERR;
			} 
			dd->NeedRestore = 1;
#endif

			dd->fs_store_width = dd->fs_width;
			dd->fs_store_height = dd->fs_height;
		}
		if (!e) e = DD_SetupOpenGL(dr);
		
	} else {
		e = InitDirectDraw(dr, dd->width, dd->height);
	}

	if (bOn) {
		dd->store_width = *outWidth;
		dd->store_height = *outHeight;
		*outWidth = dd->fs_width;
		*outHeight = dd->fs_height;
	} else {
		*outWidth = dd->store_width;
		*outHeight = dd->store_height;
	}
	SetForegroundWindow(dd->cur_hwnd);
	SetFocus(dd->cur_hwnd);
	return e;
}


GF_Err DD_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	RECT rc;
	HRESULT hr;
	DDCONTEXT;

	if (!dd) return GF_BAD_PARAM;
	if (dd->output_3d_type==1) {
#ifdef GPAC_USE_OGL_ES
		if (dd->surface) eglSwapBuffers(dd->egldpy, dd->surface);
#else
		SwapBuffers(dd->gl_HDC);
#endif
		return GF_OK;
	}

	if (!dd->ddraw_init) return GF_BAD_PARAM;

	if (!dd->fullscreen && dd->windowless) {
		HDC hdc;
		/*lock backbuffer HDC*/
		dr->LockOSContext(dr, 1);
		/*get window hdc and copy from backbuffer to window*/
		hdc = GetDC(dd->os_hwnd);
		BitBlt(hdc, 0, 0, dd->width, dd->height, dd->lock_hdc, 0, 0, SRCCOPY );
		ReleaseDC(dd->os_hwnd, hdc);
		/*unlock backbuffer HDC*/
		dr->LockOSContext(dr, 0);
		return GF_OK;
	}
	
	if (dest) {
		POINT pt;
		pt.x = dest->x;
		pt.y = dest->y;
		ClientToScreen(dd->cur_hwnd, &pt);
		dest->x = pt.x;
		dest->y = pt.y;
		MAKERECT(rc, dest);
		hr = IDirectDrawSurface_Blt(dd->pPrimary, &rc, dd->pBack, NULL, DDBLT_WAIT, NULL);
	} else {
		hr = IDirectDrawSurface_Blt(dd->pPrimary, NULL, dd->pBack, NULL, DDBLT_WAIT, NULL);
	}
	if (hr == DDERR_SURFACELOST) {
		IDirectDrawSurface_Restore(dd->pPrimary);
		IDirectDrawSurface_Restore(dd->pBack);
	}
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}



#ifdef USE_DX_3
HRESULT WINAPI EnumDisplayModes( LPDDSURFACEDESC lpDDDesc, LPVOID lpContext)
#else
HRESULT WINAPI EnumDisplayModes( LPDDSURFACEDESC2 lpDDDesc, LPVOID lpContext)
#endif
{
	DDContext *dd = (DDContext *) lpContext;
	
	//check W and H
	if (dd->width <= lpDDDesc->dwWidth  && dd->height <= lpDDDesc->dwHeight
		//check FSW and FSH
		&& dd->fs_width > lpDDDesc->dwWidth && dd->fs_height > lpDDDesc->dwHeight) {

		if (lpDDDesc->dwHeight == 200)
			return DDENUMRET_OK;
		
		dd->fs_width = lpDDDesc->dwWidth;
		dd->fs_height = lpDDDesc->dwHeight;

		return DDENUMRET_CANCEL;
	}
	return DDENUMRET_OK;
}

GF_Err GetDisplayMode(DDContext *dd)
{
	if (dd->switch_res) {
		HRESULT hr;
		Bool temp_dd = 0;;
		if (!dd->pDD) {
			LPDIRECTDRAW ddraw;
			DirectDrawCreate(NULL, &ddraw, NULL);
#ifdef USE_DX_3
			IDirectDraw_QueryInterface(ddraw, &IID_IDirectDraw, (LPVOID *)&dd->pDD);
#else
			IDirectDraw_QueryInterface(ddraw, &IID_IDirectDraw7, (LPVOID *)&dd->pDD);
#endif		
			temp_dd = 1;
		}
		//we start with a hugde res and downscale
		dd->fs_width = dd->fs_height = 50000;

#ifdef USE_DX_3
		hr = IDirectDraw_EnumDisplayModes(dd->pDD, 0L, NULL, dd,  (LPDDENUMMODESCALLBACK) EnumDisplayModes);
#else
		hr = IDirectDraw7_EnumDisplayModes(dd->pDD, 0L, NULL, dd,  (LPDDENUMMODESCALLBACK2) EnumDisplayModes);
#endif
		if (temp_dd) SAFE_DD_RELEASE(dd->pDD);
		if (FAILED(hr)) return GF_IO_ERR;
	} else {
		dd->fs_width = GetSystemMetrics(SM_CXSCREEN);
		dd->fs_height = GetSystemMetrics(SM_CYSCREEN);
	}
	return GF_OK;
}



static void *NewDXVideoOutput()
{
	DDContext *pCtx;
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "DirectX Video Output", "gpac distribution");

	pCtx = malloc(sizeof(DDContext));
	memset(pCtx, 0, sizeof(DDContext));
	driv->opaque = pCtx;
	driv->Flush = DD_Flush;
	driv->Setup  = DD_Setup;
	driv->Shutdown = DD_Shutdown;
	driv->SetFullScreen = DD_SetFullScreen;
	driv->ProcessEvent = DD_ProcessEvent;

    driv->max_screen_width = GetSystemMetrics(SM_CXSCREEN);
    driv->max_screen_height = GetSystemMetrics(SM_CYSCREEN);
	driv->hw_caps = GF_VIDEO_HW_OPENGL | GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;

	DD_SetupDDraw(driv);

	return (void *)driv;
}

static void DeleteVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	DDContext *dd = (DDContext *)driv->opaque;
	free(dd);
	free(driv);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return NewDXVideoOutput();
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return NewAudioOutput();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput((GF_VideoOutput *)ifce);
		break;
	case GF_AUDIO_OUTPUT_INTERFACE:
		DeleteAudioOutput(ifce);
		break;
	}
}
