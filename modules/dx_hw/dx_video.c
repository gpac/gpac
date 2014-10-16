/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#define WGL_DRAW_TO_WINDOW_ARB   0x2001
#define WGL_PIXEL_TYPE_ARB   0x2013
#define WGL_RED_BITS_ARB                0x2015
#define WGL_GREEN_BITS_ARB              0x2017
#define WGL_BLUE_BITS_ARB               0x2019
#define WGL_ALPHA_BITS_ARB				0x201B
#define WGL_TEXTURE_FORMAT_ARB         0x2072
#define WGL_TEXTURE_TARGET_ARB         0x2073
#define WGL_TEXTURE_RGB_ARB            0x2075

#define WGL_TEXTURE_RGBA_ARB           0x2076
#define WGL_NO_TEXTURE_ARB             0x2077
#define WGL_TEXTURE_2D_ARB             0x207A
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_DRAW_TO_PBUFFER_ARB        0x202D
#define WGL_BIND_TO_TEXTURE_RGBA_ARB   0x2071
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_GENERIC_ACCELERATION_ARB	0x2026
#define WGL_FULL_ACCELERATION_ARB      0x2027
#define WGL_TYPE_RGBA_ARB   0x202B

typedef Bool (APIENTRY *CHOOSEPFFORMATARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
static CHOOSEPFFORMATARB wglChoosePixelFormatARB = NULL;

typedef Bool (APIENTRY *GETPIXELFORMATATTRIBIV)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int* piAttributes, int *piValues);
static GETPIXELFORMATATTRIBIV wglGetPixelFormatAttribivARB = NULL;


typedef void *(APIENTRY *CREATEPBUFFERARB)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList);
static CREATEPBUFFERARB wglCreatePbufferARB = NULL;

typedef void (APIENTRY *DESTROYBUFFERARB)(void *pb);
static DESTROYBUFFERARB wglDestroyPbufferARB = NULL;

typedef HDC (APIENTRY *GETPBUFFERDCARB)(void *pb);
static GETPBUFFERDCARB wglGetPbufferDCARB = NULL;

typedef HDC (APIENTRY *RELEASEPBUFFERDCARB)(void *pb, HDC dc);
static RELEASEPBUFFERDCARB wglReleasePbufferDCARB = NULL;

typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );
PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = NULL;

static void dd_init_gl_offscreen(GF_VideoOutput *driv)
{
	const char *opt;
	DDContext *dd = driv->opaque;

	opt = gf_modules_get_option((GF_BaseInterface *)driv, "Video", "GLOffscreenMode");

#ifndef _WIN32_WCE
	wglChoosePixelFormatARB = (CHOOSEPFFORMATARB) wglGetProcAddress("wglChoosePixelFormatARB");

	wglCreatePbufferARB = (CREATEPBUFFERARB) wglGetProcAddress("wglCreatePbufferARB");
	if (opt && strcmp(opt, "PBuffer")) wglCreatePbufferARB = NULL;

	if (wglCreatePbufferARB) {
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
			dd->gl_hwnd = CreateWindow(_T("GPAC DirectDraw Output"), _T("GPAC OpenGL Offscreen"), WS_POPUP, 0, 0, 120, 100, NULL, NULL, GetModuleHandle(_T("gm_dx_hw.dll")), NULL);
#else
			dd->gl_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC OpenGL Offscreen", WS_POPUP, 0, 0, 120, 100, NULL, NULL, GetModuleHandle("gm_dx_hw.dll"), NULL);
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

		dd->pDD->lpVtbl->SetCooperativeLevel(dd->pDD, dd->cur_hwnd, DDSCL_NORMAL);
	dd->NeedRestore = 0;

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
		dd->yuv_pool.is_yuv = 1;

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
	Bool hw_reset = GF_FALSE;
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

	egl_atts[i++] = EGL_RED_SIZE;
	egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_GREEN_SIZE;
	egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_BLUE_SIZE;
	egl_atts[i++] = nb_bits;
	/*alpha for compositeTexture*/
	egl_atts[i++] = EGL_ALPHA_SIZE;
	egl_atts[i++] = 1;
	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	nb_bits = sOpt ? atoi(sOpt) : 5;
	egl_atts[i++] = EGL_DEPTH_SIZE;
	egl_atts[i++] = nb_bits;
	egl_atts[i++] = EGL_STENCIL_SIZE;
	egl_atts[i++] = EGL_DONT_CARE;
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
	HWND highbpp_hwnd = NULL;
	HWND target_hwnd = NULL;
	int bits_depth = 16;
	u32 i;
	Bool use_double_buffer;

	/*already setup*/
	target_hwnd = (dd->gl_hwnd && (dd->output_3d_type==2)) ? dd->gl_hwnd : dd->cur_hwnd;
	if ((dd->bound_hwnd == target_hwnd) && dd->gl_HRC)
		goto exit;

	hw_reset = GF_TRUE;
	dd->bound_hwnd = target_hwnd;

	/*cleanup*/
	DestroyObjectsEx(dd, (dd->output_3d_type==1) ? 0 : 1);

	//first time we init GL: create a dummy window to select pixel format for high bpp - we must do this because
	//- we must get a valid GL context to query the extensions for bpp > 8 (regular choosePixelFormat does not work for them)
	//- we must call SetPixelFormat to create the GL context
	//- it is not possible to call several time SetPixelFormat on the same window with different PF properties ...
	if (!dd->mode_high_bpp) {
		sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsPerComponent");
		if (!sOpt) {
			gf_modules_set_option((GF_BaseInterface *)dr, "Video", "GLNbBitsPerComponent", "8");
			dd->bpp = 8;
		} else {
			dd->bpp = atoi(sOpt);
		}
		if (dd->bpp > 8) {
			highbpp_hwnd = CreateWindow("GPAC DirectDraw Output", "dummy", WS_POPUP, 0, 0, 128, 128, NULL, NULL, NULL /* GetModuleHandle("gm_dx_hw.dll")*/, NULL);
			dd->gl_HDC = GetDC(highbpp_hwnd);

			memset(&pfd, 0, sizeof(pfd));
			pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_SUPPORT_OPENGL;
			if ( (pixelformat = ChoosePixelFormat(dd->gl_HDC, &pfd)) == FALSE ) return GF_IO_ERR;

			if (SetPixelFormat(dd->gl_HDC, pixelformat, &pfd) == FALSE) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX GL] Cannot select pixel format: error %d- disabling GL\n", GetLastError() ));
				return GF_IO_ERR;
			}
			dd->gl_HRC = wglCreateContext(dd->gl_HDC);
			if (!dd->gl_HRC) return GF_IO_ERR;

			wglMakeCurrent(dd->gl_HDC, dd->gl_HRC);
			wglChoosePixelFormatARB = (CHOOSEPFFORMATARB) wglGetProcAddress("wglChoosePixelFormatARB");
			wglGetPixelFormatAttribivARB = (GETPIXELFORMATATTRIBIV) wglGetProcAddress("wglGetPixelFormatAttribivARB");

			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(dd->gl_HRC);
			dd->gl_HRC = NULL;
			ReleaseDC(highbpp_hwnd, dd->gl_HDC);
			DestroyWindow(highbpp_hwnd);


			dd->mode_high_bpp = wglChoosePixelFormatARB ? 1 : 2;
		} else {
			dd->mode_high_bpp = 2;
		}
	}

	dd->gl_HDC = GetDC(dd->bound_hwnd);
	if (!dd->gl_HDC) return GF_IO_ERR;

	use_double_buffer = 0;
	if (dd->gl_double_buffer ) {
		use_double_buffer = dd->gl_double_buffer;
	} else {
		sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "UseGLDoubleBuffering");
		if (!sOpt || !strcmp(sOpt, "yes")) use_double_buffer = 1;
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	if (sOpt) bits_depth = atoi(sOpt);

	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;

	if (dd->mode_high_bpp == 1) {
		int pformats[200];
		u32 nbformats=0;
		Bool found = GF_FALSE;
		float fattribs[1] = { 0.0f };

		int hdcAttributes[] = {
			WGL_SUPPORT_OPENGL_ARB, TRUE,
			WGL_DRAW_TO_WINDOW_ARB, (dd->bound_hwnd != dd->fs_hwnd) ? TRUE : FALSE,
			WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
			WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
			WGL_RED_BITS_ARB, dd->bpp,
			WGL_GREEN_BITS_ARB, dd->bpp,
			WGL_BLUE_BITS_ARB, dd->bpp,
			WGL_ALPHA_BITS_ARB, (dd->bpp==10) ? 2 : 0,
			WGL_DEPTH_BITS_ARB, bits_depth,
			WGL_DOUBLE_BUFFER_ARB, use_double_buffer ? TRUE : FALSE,
			0,0
		};

		wglChoosePixelFormatARB(dd->gl_HDC, hdcAttributes, NULL, 200, pformats, &nbformats);

		for (i=0; i<nbformats; i++) {
			if (SetPixelFormat(dd->gl_HDC, pformats[i], &pfd) != FALSE) {
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX GL] Cannot select pixel format: error %d - disabling high color res GL and retrying\n", GetLastError() ));
			dd->mode_high_bpp = 2;
			return DD_SetupOpenGL(dr, offscreen_width, offscreen_height);
		}

		dr->max_screen_bpp = dd->bpp;

		if (wglGetPixelFormatAttribivARB) {
			int rb, gb, bb, att;
			rb = gb = bb = 0;
			att = WGL_RED_BITS_ARB;
			wglGetPixelFormatAttribivARB(dd->gl_HDC, pformats[0], 0, 1, &att, &rb);
			att = WGL_GREEN_BITS_ARB;
			wglGetPixelFormatAttribivARB(dd->gl_HDC, pformats[0], 0, 1, &att, &gb);
			att = WGL_BLUE_BITS_ARB;
			wglGetPixelFormatAttribivARB(dd->gl_HDC, pformats[0], 0, 1, &att, &bb);

			if ((rb != dd->bpp) || (gb != dd->bpp) || (bb != dd->bpp)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DX GL] Asked for %d bits per pixel but got %d red %d green %d blue\n", dd->bpp, rb, gb, bb ));
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DX GL] Setup OpenGL bpp: %d red %d green %d blue\n", rb, gb, bb ));
			}
		}

	} else {
		pfd.dwFlags = PFD_SUPPORT_OPENGL;
		if (dd->bound_hwnd != dd->fs_hwnd) pfd.dwFlags |= PFD_DRAW_TO_WINDOW;

		if (use_double_buffer) pfd.dwFlags |= PFD_DOUBLEBUFFER;
		pfd.dwLayerMask = PFD_MAIN_PLANE;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = bits_depth;
		/*we need alpha support for composite textures...*/
		pfd.cAlphaBits = 8;

		if ( (pixelformat = ChoosePixelFormat(dd->gl_HDC, &pfd)) == FALSE ) return GF_IO_ERR;

		if (SetPixelFormat(dd->gl_HDC, pixelformat, &pfd) == FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX GL] Cannot select pixel format: error %d- disabling GL\n", GetLastError() ));
			return GF_IO_ERR;
		}
	}

	dd->gl_HRC = wglCreateContext(dd->gl_HDC);
	if (!dd->gl_HRC) return GF_IO_ERR;

	if (!dd->glext_init) {
		dd->glext_init=1;
		wglMakeCurrent(dd->gl_HDC, dd->gl_HRC);
		dd_init_gl_offscreen(dr);
	}

	if (dd->disable_vsync) {
		if (!wglSwapIntervalEXT) {
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress( "wglSwapIntervalEXT" );
		}
		if (wglSwapIntervalEXT) {
			wglSwapIntervalEXT(0);
		}
	}

	if ((dd->output_3d_type!=2) || dd->gl_hwnd) {
		if (!wglMakeCurrent(dd->gl_HDC, dd->gl_HRC)) return GF_IO_ERR;
	} else if (wglCreatePbufferARB) {
		const int pbufferAttributes [50] = {
			WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
			WGL_TEXTURE_TARGET_ARB,	WGL_TEXTURE_2D_ARB,
			0
		};
		int pformats[20];
		u32 nbformats=0;
		int hdcAttributes[] = {
			WGL_SUPPORT_OPENGL_ARB, TRUE,
			WGL_DRAW_TO_PBUFFER_ARB, TRUE,
			WGL_RED_BITS_ARB, 8,
			WGL_GREEN_BITS_ARB, 8,
			WGL_BLUE_BITS_ARB, 8,
			WGL_DEPTH_BITS_ARB, bits_depth,
			0
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

	/*special care for Firefox: XUL and OpenGL do not go well together, there is a stack overflow in WM_PAINT
	for our plugin window - avoid this by overriding the WindowProc once OpenGL is setup!!*/
	if ((dd->bound_hwnd==dd->os_hwnd) && dd->orig_wnd_proc)
#ifdef _WIN64
		SetWindowLongPtr(dd->os_hwnd, GWLP_WNDPROC, (DWORD) DD_WindowProc);
#else
		SetWindowLong(dd->os_hwnd, GWL_WNDPROC, (DWORD) DD_WindowProc);
#endif

exit:
	if (dd->output_3d_type==1) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.hw_reset = hw_reset;
		dr->on_event(dr->evt_cbk_hdl, &evt);
	}
	return GF_OK;
}

#endif



GF_Err DD_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 init_flags)
{
	RECT rc;
	DDCONTEXT
	const char *opt;
	dd->os_hwnd = (HWND) os_handle;

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

	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "DisableVSync");
	if (opt && !strcmp(opt, "yes")) dd->disable_vsync = GF_TRUE;

	return GF_OK;
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

	if (bOn == dd->fullscreen) return GF_OK;
	if (!dd->fs_hwnd) return GF_NOT_SUPPORTED;

	dd->fullscreen = bOn;

	if (!dd->width ||!dd->height) return GF_OK;

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
	if (dd->os_hwnd!=dd->fs_hwnd) DestroyObjects(dd);

	if (dd->timer) KillTimer(dd->cur_hwnd, dd->timer);
	dd->timer = 0;
	if (dd->os_hwnd != dd->fs_hwnd) {
		ShowWindow(dd->cur_hwnd, SW_HIDE);
		dd->cur_hwnd = dd->fullscreen ? dd->fs_hwnd : dd->os_hwnd;
		ShowWindow(dd->cur_hwnd, SW_SHOW);
	} else {
		ShowWindow(dd->cur_hwnd, SW_HIDE);
		SetWindowLong(dd->os_hwnd, GWL_STYLE, dd->fullscreen ? WS_POPUP : dd->backup_styles);
		ShowWindow(dd->cur_hwnd, SW_SHOW);
	}

#ifndef GPAC_DISABLE_3D
	if (dd->output_3d_type==1) {
		e = GF_OK;
		dd->on_secondary_screen = 0;
		/*Setup FS*/
		if (dd->fullscreen) {
			int X = 0;
			int Y = 0;
#if(WINVER >= 0x0500)
			HMONITOR hMonitor;
			MONITORINFOEX minfo;
			/*get monitor our windows is on*/
			hMonitor = MonitorFromWindow(dd->os_hwnd, MONITOR_DEFAULTTONEAREST);
			if (hMonitor) {
				memset(&minfo, 0, sizeof(MONITORINFOEX));
				minfo.cbSize = sizeof(MONITORINFOEX);
				/*get monitor top-left for fullscreen switch, and adjust width and height*/
				GetMonitorInfo(hMonitor, (LPMONITORINFO) &minfo);
				dd->fs_width = minfo.rcWork.right - minfo.rcWork.left;
				dd->fs_height = minfo.rcWork.bottom - minfo.rcWork.top;
				X = minfo.rcWork.left;
				Y = minfo.rcWork.top;
				if (!(minfo.dwFlags & MONITORINFOF_PRIMARY)) dd->on_secondary_screen = 1;
			}
#endif

			/*change display mode*/
			if ((MaxWidth && (dd->fs_width >= MaxWidth)) || (MaxHeight && (dd->fs_height >= MaxHeight)) ) {
				dd->fs_width = MaxWidth;
				dd->fs_height = MaxHeight;
			}
			SetWindowPos(dd->cur_hwnd, NULL, X, Y, dd->fs_width, dd->fs_height, SWP_SHOWWINDOW | SWP_NOZORDER /*| SWP_ASYNCWINDOWPOS*/);

			dd->fs_store_width = dd->fs_width;
			dd->fs_store_height = dd->fs_height;
		} else if (dd->os_hwnd==dd->fs_hwnd) {
			SetWindowPos(dd->os_hwnd, NULL, 0, 0, dd->store_width+dd->off_w, dd->store_height+dd->off_h, SWP_NOMOVE | SWP_NOZORDER /*| SWP_ASYNCWINDOWPOS*/);
		}

		if (!e) e = DD_SetupOpenGL(dr, 0, 0);

	} else
#endif
	{

		if (!dd->fullscreen && (dd->os_hwnd==dd->fs_hwnd)) {
			SetWindowPos(dd->os_hwnd, NULL, 0, 0, dd->store_width+dd->off_w, dd->store_height+dd->off_h, SWP_NOZORDER | SWP_NOMOVE | SWP_ASYNCWINDOWPOS);
		}
		/*first time FS, store*/
		if (!dd->store_width) {
			dd->store_width = dd->width;
			dd->store_height = dd->height;
		}
		e = InitDirectDraw(dr, dd->store_width, dd->store_height);
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DX] FLushing video output\n"));

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

	if (!dd->disable_vsync)
		hr = dd->pDD->lpVtbl->WaitForVerticalBlank(dd->pDD, DDWAITVB_BLOCKBEGIN, NULL);

	if (dest) {
		POINT pt;
		pt.x = dest->x;
		pt.y = dest->y;
		ClientToScreen(dd->cur_hwnd, &pt);
		dest->x = pt.x;
		dest->y = pt.y;
		MAKERECT(rc, dest);
		hr = dd->pPrimary->lpVtbl->Blt(dd->pPrimary, &rc, dd->pBack, NULL, DDBLT_WAIT, NULL);
	} else {
		hr = dd->pPrimary->lpVtbl->Blt(dd->pPrimary, NULL, dd->pBack, NULL, DDBLT_WAIT, NULL);
	}
	if (hr == DDERR_SURFACELOST) {
		dd->pPrimary->lpVtbl->Restore(dd->pPrimary);
		dd->pBack->lpVtbl->Restore(dd->pBack);
	}
	return FAILED(hr) ? GF_IO_ERR : GF_OK;
}



HRESULT WINAPI EnumDisplayModes( LPDDSURFDESC lpDDDesc, LPVOID lpContext)
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
	if (dd->switch_res && dd->DirectDrawCreate) {
		HRESULT hr;
		Bool temp_dd = 0;;
		if (!dd->pDD) {
			LPDIRECTDRAW ddraw;
			dd->DirectDrawCreate(NULL, &ddraw, NULL);
			ddraw->lpVtbl->QueryInterface(ddraw, &IID_IDirectDraw7, (LPVOID *)&dd->pDD);
			temp_dd = 1;
		}
		//we start with a hugde res and downscale
		dd->fs_width = dd->fs_height = 50000;

		hr = dd->pDD->lpVtbl->EnumDisplayModes(dd->pDD, 0L, NULL, dd,  EnumDisplayModes);

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
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "DirectX Video Output", "gpac distribution");

	pCtx = gf_malloc(sizeof(DDContext));
	memset(pCtx, 0, sizeof(DDContext));
	driv->opaque = pCtx;
	driv->Flush = DD_Flush;
	driv->Setup  = DD_Setup;
	driv->Shutdown = DD_Shutdown;
	driv->SetFullScreen = DD_SetFullScreen;
	driv->ProcessEvent = DD_ProcessEvent;

	pCtx->hDDrawLib = LoadLibrary("ddraw.dll");
	if (pCtx->hDDrawLib) {
		pCtx->DirectDrawCreate = (DIRECTDRAWCREATEPROC) GetProcAddress(pCtx->hDDrawLib, "DirectDrawCreate");
	}

	driv->max_screen_width = GetSystemMetrics(SM_CXSCREEN);
	driv->max_screen_height = GetSystemMetrics(SM_CYSCREEN);
	driv->max_screen_bpp = 8;
	driv->hw_caps = GF_VIDEO_HW_OPENGL | GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA | GF_VIDEO_HW_HAS_HWND_HDC;

	DD_SetupDDraw(driv);

	return (void *)driv;
}

static void DeleteVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	DDContext *dd = (DDContext *)driv->opaque;

	if (dd->hDDrawLib) {
		FreeLibrary(dd->hDDrawLib);
	}

	gf_free(dd);
	gf_free(driv);
}

/*interface query*/
GPAC_MODULE_EXPORT
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
GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return NewDXVideoOutput();
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return NewAudioOutput();
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
	case GF_AUDIO_OUTPUT_INTERFACE:
		DeleteAudioOutput(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( dx_out )
