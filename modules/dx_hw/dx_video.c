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




#ifndef GPAC_DISABLE_3D

#define WGL_RED_BITS_ARB                    0x2015
#define WGL_GREEN_BITS_ARB                  0x2017
#define WGL_BLUE_BITS_ARB                   0x2019

#define WGL_TEXTURE_FORMAT_ARB         0x2072
#define WGL_TEXTURE_TARGET_ARB         0x2073
#define WGL_TEXTURE_RGB_ARB            0x2075

#define WGL_TEXTURE_RGBA_ARB           0x2076
#define WGL_NO_TEXTURE_ARB             0x2077
#define WGL_TEXTURE_2D_ARB             0x207A
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DRAW_TO_PBUFFER_ARB        0x202D
#define WGL_BIND_TO_TEXTURE_RGBA_ARB   0x2071
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_GENERIC_ACCELERATION_ARB	0x2026
#define WGL_FULL_ACCELERATION_ARB      0x2027

typedef Bool (APIENTRY *CHOOSEPFFORMATARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
static CHOOSEPFFORMATARB wglChoosePixelFormatARB = NULL;

typedef void *(APIENTRY *CREATEPBUFFERARB)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList);
static CREATEPBUFFERARB wglCreatePbufferARB = NULL;

typedef void (APIENTRY *DESTROYBUFFERARB)(void *pb);
static DESTROYBUFFERARB wglDestroyPbufferARB = NULL;

typedef HDC (APIENTRY *GETPBUFFERDCARB)(void *pb);
static GETPBUFFERDCARB wglGetPbufferDCARB = NULL;

typedef HDC (APIENTRY *RELEASEPBUFFERDCARB)(void *pb, HDC dc);
static RELEASEPBUFFERDCARB wglReleasePbufferDCARB = NULL;

static void dd_init_gl_offscreen(GF_VideoOutput *driv)
{
	const char *opt;
	DDContext *dd = driv->opaque;

	opt = gf_modules_get_option((GF_BaseInterface *)driv, "Video", "GLOffscreenMode");

#ifndef _WIN32_WCE

	wglCreatePbufferARB = (CREATEPBUFFERARB) wglGetProcAddress("wglCreatePbufferARB");
	if (opt && strcmp(opt, "PBuffer")) wglCreatePbufferARB = NULL;

	if (wglCreatePbufferARB) {
		wglChoosePixelFormatARB = (CHOOSEPFFORMATARB) wglGetProcAddress("wglChoosePixelFormatARB");
		wglDestroyPbufferARB = (DESTROYBUFFERARB) wglGetProcAddress("wglDestroyPbufferARB");
		wglGetPbufferDCARB = (GETPBUFFERDCARB ) wglGetProcAddress("wglGetPbufferDCARB");
		wglReleasePbufferDCARB = (RELEASEPBUFFERDCARB ) wglGetProcAddress("wglReleasePbufferDCARB");

		GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX] Using PBuffer for OpenGL Offscreen Rendering\n"));
		driv->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;

		if (!opt) gf_modules_set_option((GF_BaseInterface *)driv, "Video", "GLOffscreenMode", "PBuffer");
	} else 
#endif
	{
		u32 gl_type = 1;
		HINSTANCE hInst;

#ifndef _WIN32_WCE
		hInst = GetModuleHandle("gm_dx_hw.dll");
#else
		hInst = GetModuleHandle(_T("gm_dx_hw.dll"));
#endif
		opt = gf_modules_get_option((GF_BaseInterface *)driv, "Video", "GLOffscreenMode");
		if (opt) {
			if (!strcmp(opt, "Window")) gl_type = 1;
			else if (!strcmp(opt, "VisibleWindow")) gl_type = 2;
			else gl_type = 0;
		} else {
			gf_modules_set_option((GF_BaseInterface *)driv, "Video", "GLOffscreenMode", "Window");
		}

		if (gl_type) {
#ifdef _WIN32_WCE
			dd->gl_hwnd = CreateWindow(_T("GPAC DirectDraw Output"), _T("GPAC OpenGL Offscreen"), WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
#else
			dd->gl_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC OpenGL Offscreen", WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
#endif
			if (!dd->gl_hwnd)
				return;

			ShowWindow(dd->gl_hwnd, (gl_type == 2) ? SW_SHOW : SW_HIDE);
			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX] Using %s window for OpenGL Offscreen Rendering\n", (gl_type == 2) ? "Visible" : "Hidden"));
			driv->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
		}
	}

}
#endif


static void RestoreWindow(DDContext *dd) 
{
	if (!dd->NeedRestore) return;
	dd->NeedRestore = 0;

#ifndef GPAC_DISABLE_3D
	if (dd->output_3d_type==1) {
#ifndef _WIN32_WCE
		ChangeDisplaySettings(NULL,0);
#endif
	} else 
#endif

	{
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
//		RestoreWindow(dd);

		SAFE_DD_RELEASE(dd->rgb_pool.pSurface);
		memset(&dd->rgb_pool, 0, sizeof(DDSurface));
		SAFE_DD_RELEASE(dd->yuv_pool.pSurface);
		memset(&dd->yuv_pool, 0, sizeof(DDSurface));

		SAFE_DD_RELEASE(dd->pPrimary);
		SAFE_DD_RELEASE(dd->pBack);
		SAFE_DD_RELEASE(dd->pDD);
		dd->ddraw_init = 0;

#ifdef GPAC_DISABLE_3D
	}
#else

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

	if (dd->pb_HRC) {
		wglMakeCurrent(dd->pb_HDC, NULL);
		wglDeleteContext(dd->pb_HRC);
		dd->pb_HRC = NULL;
	}
	if (dd->pb_HDC) {
//		wglReleasePbufferDCARB(dd->pbuffer, dd->pb_HDC);
		ReleaseDC(dd->bound_hwnd, dd->pb_HDC);
		dd->pb_HDC = NULL;
	}
	if (dd->pbuffer) {
		wglDestroyPbufferARB(dd->pbuffer);
		dd->pbuffer = NULL;
	}
	
	if (dd->gl_HRC) {
		//wglMakeCurrent(NULL, NULL);
		wglDeleteContext(dd->gl_HRC);
		dd->gl_HRC = NULL;
	}
	if (dd->gl_HDC) {
		ReleaseDC(dd->bound_hwnd, dd->gl_HDC);
		dd->gl_HDC = NULL;
	}
#endif


#endif
}

void DestroyObjects(DDContext *dd)
{
	DestroyObjectsEx(dd, 0);
}

#ifndef GPAC_DISABLE_3D

GF_Err DD_SetupOpenGL(GF_VideoOutput *dr, u32 offscreen_width, u32 offscreen_height) 
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
	u32 i;

	/*already setup*/
	DestroyObjectsEx(dd, (dd->output_3d_type==1) ? 0 : 1);

	dd->bound_hwnd = (dd->gl_hwnd && (dd->output_3d_type==2)) ? dd->gl_hwnd : dd->cur_hwnd;
	dd->gl_HDC = GetDC(dd->bound_hwnd );

	if (!dd->gl_HDC) return GF_IO_ERR;

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_SUPPORT_OPENGL;
	if (dd->bound_hwnd != dd->fs_hwnd) pfd.dwFlags |= PFD_DRAW_TO_WINDOW;

	if (dd->gl_double_buffer ) {
		pfd.dwFlags |= PFD_DOUBLEBUFFER;
	} else {
		sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "UseGLDoubleBuffering");
		if (!sOpt || !strcmp(sOpt, "yes")) pfd.dwFlags |= PFD_DOUBLEBUFFER;
	}

    pfd.dwLayerMask = PFD_MAIN_PLANE;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	pfd.cDepthBits = sOpt ? atoi(sOpt) : 16;
	/*we need alpha support for composite textures...*/
	pfd.cAlphaBits = 8;
    if ( (pixelformat = ChoosePixelFormat(dd->gl_HDC, &pfd)) == FALSE ) return GF_IO_ERR; 
	
    if (SetPixelFormat(dd->gl_HDC, pixelformat, &pfd) == FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX GL] Cannot select pixel format: error %d- disabling GL\n", GetLastError() ));
		return GF_IO_ERR; 
	}
	
	dd->gl_HRC = wglCreateContext(dd->gl_HDC);
	if (!dd->gl_HRC) return GF_IO_ERR;

	if (!dd->glext_init) {
		dd->glext_init=1;
		wglMakeCurrent(dd->gl_HDC, dd->gl_HRC);
		dd_init_gl_offscreen(dr);
		return DD_SetupOpenGL(dr, offscreen_width, offscreen_height);
	}


	if ((dd->output_3d_type!=2) || dd->gl_hwnd) {
		if (!wglMakeCurrent(dd->gl_HDC, dd->gl_HRC)) return GF_IO_ERR;
	} else if (wglCreatePbufferARB) {
		const int pbufferAttributes [50] = {
			WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
			WGL_TEXTURE_TARGET_ARB,	WGL_TEXTURE_2D_ARB,
			0
		};
		u32 pformats[20];
		u32 nbformats=0;
		int hdcAttributes[] = {
			WGL_SUPPORT_OPENGL_ARB, TRUE,
			WGL_DRAW_TO_PBUFFER_ARB, TRUE,
			WGL_RED_BITS_ARB, 8,
			WGL_GREEN_BITS_ARB, 8,
			WGL_BLUE_BITS_ARB, 8, 
/*
			WGL_DEPTH_BITS_ARB, 16,
			WGL_BIND_TO_TEXTURE_RGBA_ARB, TRUE,
			WGL_COLOR_BITS_ARB,24,
			WGL_DEPTH_BITS_ARB, 0,
			WGL_STENCIL_BITS_ARB,0,
*/			0
		}; 
		wglChoosePixelFormatARB(dd->gl_HDC, hdcAttributes, NULL, 20, pformats, &nbformats); 
		// Create the PBuffer
		for (i=0; i<nbformats; i++) {
			dd->pbuffer = wglCreatePbufferARB(dd->gl_HDC, pformats[i], offscreen_width, offscreen_height, pbufferAttributes); 
			if (dd->pbuffer) break;
		}
		if (!dd->pbuffer) return GF_IO_ERR;

		dd->pb_HDC = wglGetPbufferDCARB(dd->pbuffer);
		dd->pb_HRC = wglCreateContext(dd->pb_HDC);
		if (!wglMakeCurrent(dd->pb_HDC, dd->pb_HRC)) return GF_IO_ERR;

	
	}
#endif

	if (dd->output_3d_type==1) {
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.opengl_mode = 1;
		dr->on_event(dr->evt_cbk_hdl, &evt);	
	}
	return GF_OK;
}

#endif



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

	{
		HDC hdc;
		hdc = GetDC(dd->os_hwnd);
		dr->dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
		dr->dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(dd->os_hwnd, hdc);
	}

#ifndef GPAC_DISABLE_3D
	dd->output_3d_type = 0;
#endif
	GetWindowRect(dd->cur_hwnd, &rc);
	return InitDirectDraw(dr, rc.right - rc.left, rc.bottom - rc.top);
}

static void DD_Shutdown(GF_VideoOutput *dr)
{
	DDCONTEXT

	/*force destroy of opengl*/
#ifndef GPAC_DISABLE_3D
	if (dd->output_3d_type) dd->output_3d_type = 1;
#endif
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

	if (dd->NeedRestore) RestoreWindow(dd);
	/*destroy all objects*/
	DestroyObjects(dd);

	if (dd->timer) KillTimer(dd->cur_hwnd, dd->timer);
	dd->timer = 0;
	ShowWindow(dd->cur_hwnd, SW_HIDE);
	dd->cur_hwnd = dd->fullscreen ? dd->fs_hwnd : dd->os_hwnd;
	ShowWindow(dd->cur_hwnd, SW_SHOW);

#ifndef GPAC_DISABLE_3D
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
		if (!e) e = DD_SetupOpenGL(dr, 0, 0);
		
	} else 
#endif
	{
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

#ifndef GPAC_DISABLE_3D

	if (dd->output_3d_type==1) {
#ifdef GPAC_USE_OGL_ES
		if (dd->surface) eglSwapBuffers(dd->egldpy, dd->surface);
#else
		SwapBuffers(dd->gl_HDC);
#endif
		return GF_OK;
	}
#endif


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
GF_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si;
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
