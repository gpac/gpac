/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / GAPI WinCE-iPaq video render module
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


#include <windows.h>
#include <aygshell.h>
#include <wingdi.h>
#include <gx.h>

#include "gapi.h"

#define PRINT(__str) OutputDebugString(_T(__str))

#define GAPICTX(dr)	GAPIPriv *gctx = (GAPIPriv *) dr->opaque;

static GF_Err GAPI_InitSurface(GF_VideoOutput *dr, u32 VideoWidth, u32 VideoHeight);

static GF_VideoOutput *the_video_driver = NULL;

static void GAPI_GetCoordinates(DWORD lParam, GF_Event *evt)
{
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
	evt->mouse.x = LOWORD(lParam);
	evt->mouse.y = HIWORD(lParam);

	if (ctx->fullscreen) {
		POINT pt;
		pt.x = evt->mouse.x;
		pt.y = evt->mouse.y;
		ClientToScreen(ctx->hWnd, &pt);
		if (ctx->is_landscape) {
			evt->mouse.x = ctx->fs_w - pt.y;
			evt->mouse.y = pt.x;
		} else {
			evt->mouse.x = pt.x;
			evt->mouse.y = pt.y;
		}
	}
}

static u32 GAPI_TranslateActionKey(u32 VirtKey) 
{
	switch (VirtKey) {
	case VK_HOME: return GF_VK_HOME;
	case VK_END: return GF_VK_END;
	case VK_NEXT: return GF_VK_PRIOR;
	case VK_PRIOR: return GF_VK_NEXT;
	case VK_UP: return GF_VK_UP;
	case VK_DOWN: return GF_VK_DOWN;
	case VK_LEFT: return GF_VK_LEFT;
	case VK_RIGHT: return GF_VK_RIGHT;
	case VK_F1: return GF_VK_F1;
	case VK_F2: return GF_VK_F2;
	case VK_F3: return GF_VK_F3;
	case VK_F4: return GF_VK_F4;
	case VK_F5: return GF_VK_F5;
	case VK_F6: return GF_VK_F6;
	case VK_F7: return GF_VK_F7;
	case VK_F8: return GF_VK_F8;
	case VK_F9: return GF_VK_F9;
	case VK_F10: return GF_VK_F10;
	case VK_F11: return GF_VK_F11;
	case VK_F12: return GF_VK_F12;
	case VK_RETURN: return GF_VK_RETURN;
	case VK_ESCAPE: return GF_VK_ESCAPE;
	case VK_SHIFT: return GF_VK_SHIFT;
	case VK_CONTROL: return GF_VK_CONTROL;
	case VK_MENU: return GF_VK_MENU;
	default: return 0;
	}
}

u32 GX_TRANSLATE_KEY(u32 vk)
{
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
	short res = (LOWORD(vk) != 0x5b) ? LOWORD(vk) : vk;
	if (res==ctx->keys.vkLeft) return GF_VK_LEFT;
	else if (res==ctx->keys.vkRight) return GF_VK_RIGHT;
	else if (res==ctx->keys.vkDown) return GF_VK_DOWN;
	else if (res==ctx->keys.vkUp) return GF_VK_UP;
	else if (res==ctx->keys.vkStart) return GF_VK_RETURN;
	else if (res==ctx->keys.vkA) return GF_VK_CONTROL;
	else if (res==ctx->keys.vkB) return GF_VK_SHIFT;
	else if (res==ctx->keys.vkC) return GF_VK_MENU;
	else return 0;
}

LRESULT APIENTRY GAPI_WindowProc(HWND hWnd, UINT msg, UINT wParam, LONG lParam)
{
	GF_Event evt;
	switch (msg) {
	case WM_SIZE:
	if (0) {
		GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
		evt.type = GF_EVT_SIZE;
		evt.size.width = LOWORD(lParam);
		evt.size.height = HIWORD(lParam);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
	}
		break;
	case WM_CLOSE:
		evt.type = GF_EVT_QUIT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		return 1;
	case WM_DESTROY:
		PostQuitMessage (0);
		break;

	case WM_ERASEBKGND:
		evt.type = GF_EVT_REFRESH;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC dc, hdcBitmap, old;
		GAPIPriv *gctx = (GAPIPriv *)the_video_driver->opaque;
		if (gctx->gx_mode) break;
		dc = BeginPaint(gctx->hWnd, &ps);
		hdcBitmap = CreateCompatibleDC((HDC)dc);
		old = (HDC) SelectObject(hdcBitmap, gctx->bitmap);
		BitBlt((HDC)dc, gctx->dst_blt.x, gctx->dst_blt.y, gctx->bb_width, gctx->bb_height, hdcBitmap, 0, 0, SRCCOPY);
		SelectObject(hdcBitmap, old);
		EndPaint(gctx->hWnd, &ps);
	}
		break;


	case WM_MOUSEMOVE:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_MOUSEMOVE;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_LEFTDOWN;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONUP:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVT_LEFTUP;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;

	/*FIXME - there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		evt.key.vk_code = GAPI_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_SYSKEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		/*emulate button actions as vk codes*/
		if ((evt.key.vk_code = GX_TRANSLATE_KEY(wParam)) != 0) {
			evt.key.virtual_code = wParam;
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			break;
		}

		evt.key.vk_code = GAPI_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			/*also send a normal key for non-key-sensors*/
			if (evt.key.vk_code>GF_VK_RIGHT) goto send_key;
		} else {
send_key:
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_KEYDOWN : GF_EVT_KEYUP;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		}
		break;
	case WM_CHAR:
		evt.type = GF_EVT_CHAR;
		evt.character.unicode_char = wParam;
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

void GAPI_WindowThread(void *par)
{
	MSG msg;
	WNDCLASS wc;
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;

	memset(&wc, 0, sizeof(WNDCLASS));
	wc.hInstance = GetModuleHandle(_T("gapi.dll"));
	wc.lpfnWndProc = GAPI_WindowProc;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	wc.lpszClassName = _T("GPAC GAPI Output");
	RegisterClass (&wc);
	
	ctx->hWnd = CreateWindow(_T("GPAC GAPI Output"), NULL, WS_POPUP, 0, 0, 120, 100, NULL, NULL, wc.hInstance, NULL);
	if (ctx->hWnd == NULL) {
		ctx->ThreadID = 0;
		ExitThread(1);
	}
	ShowWindow(ctx->hWnd, SW_SHOWNORMAL);

	while (GetMessage (&(msg), NULL, 0, 0)) {
		TranslateMessage (&(msg));
		DispatchMessage (&(msg));
	}
	ctx->ThreadID = 0;
	ExitThread (0);
}


void GAPI_SetupWindow(GF_VideoOutput *dr)
{
	GAPIPriv *ctx = (GAPIPriv *)dr->opaque;
	if (the_video_driver) return;
	the_video_driver = dr;

	if (!ctx->hWnd) {
		ctx->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) GAPI_WindowThread, (LPVOID) dr, 0, &(ctx->ThreadID) );
		while (!ctx->hWnd && ctx->hThread) gf_sleep(10);
		if (!ctx->hThread) return;
		ctx->owns_hwnd = 1;
	} else {
		/*override window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, (DWORD) GAPI_WindowProc);
	}
}

void GAPI_ShutdownWindow(GF_VideoOutput *dr)
{
	GAPIPriv *ctx = (GAPIPriv *)dr->opaque;

	if (ctx->owns_hwnd) {
		PostMessage(ctx->hWnd, WM_DESTROY, 0, 0);
		while (ctx->ThreadID) gf_sleep(10);
		UnregisterClass(_T("GPAC GAPI Output"), GetModuleHandle(_T("gapi.dll")));
		CloseHandle(ctx->hThread);
		ctx->hThread = NULL;
	}
	ctx->hWnd = NULL;
	the_video_driver = NULL;
}


GF_Err GAPI_Clear(GF_VideoOutput *dr, u32 color)
{
	GAPICTX(dr);
	gctx->erase_dest = 1;
	return GF_OK;
}

static void createPixmap(GAPIPriv *ctx, HDC dc, Bool for_gl)
{
    const size_t    bmiSize = sizeof(BITMAPINFO) + 256U*sizeof(RGBQUAD);
    BITMAPINFO*     bmi;
    DWORD*          p;
    bmi = (BITMAPINFO*)malloc(bmiSize);
    memset(bmi, 0, bmiSize);

    bmi->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth          = ctx->bb_width;
    bmi->bmiHeader.biHeight         = -1 * (s32) ctx->bb_height;	/*top-down image*/
    bmi->bmiHeader.biPlanes         = (short)1;
    bmi->bmiHeader.biBitCount       = (unsigned int) ctx->bits_per_pixel;
    bmi->bmiHeader.biCompression    = BI_BITFIELDS;
    bmi->bmiHeader.biClrUsed        = 3;

    p = (DWORD*)bmi->bmiColors;
	switch (ctx->pixel_format) {
	case GF_PIXEL_RGB_555:
		p[0] = 0x00007c00; p[1] = 0x000003e0; p[2] = 0x0000001f;
		break;
	case GF_PIXEL_RGB_565:
		p[0] = 0x0000f800; p[1] = 0x000007e0; p[2] = 0x0000001f;
		break;
	case GF_PIXEL_RGB_24:
		p[0] = 0x00ff0000; p[1] = 0x0000ff00; p[2] = 0x000000ff;
		break;
	}
		p[0] = 0x0000f800; p[1] = 0x000007e0; p[2] = 0x0000001f;
	if (for_gl) {
		ctx->bitmap = CreateDIBSection(dc, bmi, DIB_RGB_COLORS, (void **) &ctx->bits, NULL, 0);
	} else {
		ctx->bitmap = CreateDIBSection(dc, bmi, DIB_RGB_COLORS, (void **) &ctx->backbuffer, NULL, 0);
	}
	free(bmi);
}


#ifdef GPAC_USE_OGL_ES

void GAPI_ReleaseOGL_ES(GAPIPriv *ctx)
{
	if (ctx->egldpy) {
		if (ctx->eglctx) eglDestroyContext(ctx->egldpy, ctx->eglctx);
		if (ctx->surface ) eglDestroySurface(ctx->egldpy, ctx->surface);
		if (ctx->egldpy) eglTerminate(ctx->egldpy);
		ctx->egldpy = 0;
	}
    if (ctx->bitmap) DeleteObject(ctx->bitmap);
	ctx->bitmap = NULL;
}

GF_Err GAPI_SetupOGL_ES(GF_VideoOutput *dr) 
{
	EGLint n, maj, min;
	GF_Event evt;
	HDC dc;
	GAPICTX(dr)
	static int atts[15];
	atts[0] = EGL_RED_SIZE; atts[1] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : 5;
	atts[2] = EGL_GREEN_SIZE; atts[3] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : (gctx->pixel_format==GF_PIXEL_RGB_565) ? 6 : 5;
	atts[4]  = EGL_BLUE_SIZE; atts[5] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : 5;
	atts[6] = EGL_ALPHA_SIZE; atts[7] = EGL_DONT_CARE;
	atts[8]  = EGL_DEPTH_SIZE; atts[9] = 32;
	atts[10] = EGL_STENCIL_SIZE; atts[11] = EGL_DONT_CARE;
	atts[12]  = EGL_SURFACE_TYPE; atts[13] = EGL_PIXMAP_BIT;
	atts[14]  = EGL_NONE;

	/*whenever window is resized we must reinit OGL-ES*/
	GAPI_ReleaseOGL_ES(gctx);

	if (!gctx->fullscreen) {
		RECT rc;
		::GetClientRect(gctx->hWnd, &rc);
		gctx->bb_width = rc.right-rc.left;
		gctx->bb_height = rc.bottom-rc.top;
		dc = GetDC(gctx->hWnd);
		createPixmap(gctx, dc, 1);
		ReleaseDC(gctx->hWnd, dc);
	}

	gctx->egldpy = eglGetDisplay(/*gctx->dpy*/EGL_DEFAULT_DISPLAY);
	if (!eglInitialize(gctx->egldpy, &maj, &min)) {
		gctx->egldpy = NULL;
		return GF_IO_ERR;
	}

	eglGetConfigs(gctx->egldpy, NULL, 0, &n);
	if (!eglChooseConfig(gctx->egldpy, atts, &gctx->eglconfig, 1, &n)) {
		return GF_IO_ERR;
	}
	if (gctx->fullscreen) {
		gctx->surface = eglCreateWindowSurface(gctx->egldpy, gctx->eglconfig, gctx->hWnd, 0);
	} else {
		gctx->surface = eglCreatePixmapSurface(gctx->egldpy, gctx->eglconfig, gctx->bitmap, 0);
	}

	if (!gctx->surface) {
		return GF_IO_ERR; 
	}
	gctx->eglctx = eglCreateContext(gctx->egldpy, gctx->eglconfig, NULL, NULL);
	if (!gctx->eglctx) {
		eglDestroySurface(gctx->egldpy, gctx->surface);
		gctx->surface = 0L;
		return GF_IO_ERR; 
	}
    if (!eglMakeCurrent(gctx->egldpy, gctx->surface, gctx->surface, gctx->eglctx)) {
		eglDestroyContext(gctx->egldpy, gctx->eglctx);
		gctx->eglctx = 0L;
		eglDestroySurface(gctx->egldpy, gctx->surface);
		gctx->surface = 0L;
		return GF_IO_ERR;
	}
	evt.type = GF_EVT_VIDEO_SETUP;
	dr->on_event(dr->evt_cbk_hdl, &evt);	
	return GF_OK;
}
#endif


void GAPI_ReleaseObjects(GAPIPriv *ctx)
{
#ifdef GPAC_USE_OGL_ES
	if (ctx->is_3D) GAPI_ReleaseOGL_ES(ctx);
	else
#endif
    if (ctx->bitmap) DeleteObject(ctx->bitmap);
	else if (ctx->backbuffer) free(ctx->backbuffer);
	ctx->backbuffer = NULL;
	ctx->bitmap = NULL;
}

GF_Err GAPI_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, Bool noover, GF_GLConfig *cfg)
{
	struct GXDisplayProperties gx = GXGetDisplayProperties();
	RECT rc;
	GAPICTX(dr);
	gctx->hWnd = (HWND) os_handle;
	
	/*get keys in both 2D and 3D modes*/
	gctx->keys = GXGetDefaultKeys(GX_NORMALKEYS);

#if 0
	/*FIXME - not supported in rasterizer*/
	 if (gx.ffFormat & kfDirect444) {
		gctx->pixel_format = GF_PIXEL_RGB_444;
		gctx->BPP = 2;
		gctx->bitsPP = 12;
	} 
	else 
#endif
	if (gx.ffFormat & kfDirect555) {
		gctx->pixel_format = GF_PIXEL_RGB_555;
		gctx->BPP = 2;
		gctx->bits_per_pixel = 15;
	} 
	else if (gx.ffFormat & kfDirect565) {
		gctx->pixel_format = GF_PIXEL_RGB_565;
		gctx->BPP = 2;
		gctx->bits_per_pixel = 16;
	}
	else if (gx.ffFormat & kfDirect888) {
		gctx->pixel_format = GF_PIXEL_RGB_24;
		gctx->BPP = 3;
		gctx->bits_per_pixel = 24;
	} else {
		return GF_NOT_SUPPORTED;
	}
	dr->max_screen_width = gctx->screen_w = gx.cxWidth;
	dr->max_screen_height = gctx->screen_h = gx.cyHeight;
	gctx->is_landscape = (gx.ffFormat & kfLandscape) ? 1 : 0;
	gctx->x_pitch = gx.cbxPitch;
	gctx->y_pitch = gx.cbyPitch;

	if (cfg) {
#ifdef GPAC_USE_OGL_ES
		gctx->gl_cfg = *cfg;
		gctx->is_3D = 1;
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	GAPI_SetupWindow(dr);
	if (!gctx->hWnd) return GF_IO_ERR;
#ifdef GPAC_USE_OGL_ES
	if (gctx->is_3D) return GF_OK;
#endif
	
	/*setup GX*/
	if (!GXOpenDisplay(gctx->hWnd, 0L)) {
		MessageBox(NULL, _T("Cannot open display"), _T("GAPI Error"), MB_OK);
		return GF_IO_ERR;
	}
    GetClientRect(gctx->hWnd, &rc);
	gctx->backup_w = rc.right - rc.left;
	gctx->backup_h = rc.bottom - rc.top;
	return GAPI_InitSurface(dr, gctx->backup_w, gctx->backup_h);
}

static void GAPI_Shutdown(GF_VideoOutput *dr)
{
	GAPICTX(dr);

	gf_mx_p(gctx->mx);
	GAPI_ReleaseObjects(gctx);

	GXCloseDisplay();
	GAPI_ShutdownWindow(dr);
	gf_mx_v(gctx->mx);
}

static GF_Err GAPI_SetFullScreen(GF_VideoOutput *dr, Bool bOn, u32 *outWidth, u32 *outHeight)
{
	GF_Err e;
	GAPICTX(dr);

	if (!gctx) return GF_BAD_PARAM;
	if (bOn == gctx->fullscreen) return GF_OK;

#ifdef GPAC_USE_OGL_ES
	if (gctx->is_3D) {
		gctx->fullscreen = bOn;
		return GAPI_SetupOGL_ES(dr);
	}
#endif

	gf_mx_p(gctx->mx);
	GAPI_ReleaseObjects(gctx);
	GXCloseDisplay();
	e = GF_OK;
	if (bOn) {
		if (!GXOpenDisplay(GetParent(gctx->hWnd), GX_FULLSCREEN)) {
			GXOpenDisplay(gctx->hWnd, 0L);
			gctx->fullscreen = 0;
			e = GF_IO_ERR;
		} else {
			gctx->fullscreen = 1;
		}
	} else {
		GXOpenDisplay(gctx->hWnd, 0L);
		gctx->fullscreen = 0;
	}

	if (!e) {
		gctx->is_landscape = gctx->fullscreen;
		if (gctx->fullscreen) {
			gctx->backup_w = *outWidth;
			gctx->backup_h = *outHeight;

			if (gctx->is_landscape) {
				gctx->fs_w = gctx->screen_h;
				gctx->fs_h = gctx->screen_w;
			} else {
				gctx->fs_w = gctx->screen_w;
				gctx->fs_h = gctx->screen_h;
			}
			*outWidth = gctx->fs_w;
			*outHeight = gctx->fs_h;
		} else {
			*outWidth = gctx->backup_w;
			*outHeight = gctx->backup_h;
		}
		e = GAPI_InitSurface(dr, *outWidth, *outHeight); 
	}
	gf_mx_v(gctx->mx);

	return e;
}

GF_Err GAPI_ClearFS(GAPIPriv *gctx, unsigned char *ptr, s32 x_pitch, s32 y_pitch)
{
	s32 i, j;
	gf_mx_p(gctx->mx);
	if (gctx->BPP==3) {
		for (i=0; i< (s32)gctx->fs_h; i++) {
			unsigned char *_ptr =  ptr + i*y_pitch;
			for (j=0; j<(s32)gctx->fs_w; j++) { 
				_ptr[0] = _ptr[1] = _ptr[2] = 0;
				_ptr += x_pitch;
			}
		}
	} else {
		for (i=0; i<(s32)gctx->fs_h; i++) {
			unsigned char *_ptr = ptr + i*y_pitch;
			for (j=0; j<(s32)gctx->fs_w; j++) {
				* ((unsigned short *)_ptr) = 0;
				_ptr += x_pitch;
			}
		}
	}
	gf_mx_v(gctx->mx);
	return GF_OK;
}


static GF_Err GAPI_FlipBackBuffer(GF_VideoOutput *dr)
{
	GF_VideoSurface src, dst;
	unsigned char *ptr;
	GAPICTX(dr);
	s32 pitch_y = gctx->y_pitch;
	s32 pitch_x = gctx->x_pitch;
	if (!gctx || !gctx->gx_mode) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);

	/*get a pointer to video memory*/
	ptr = (unsigned char *) GXBeginDraw();
	if (!ptr) {
		gf_mx_v(gctx->mx);
		return GF_IO_ERR;
	}
	
	src.video_buffer = gctx->backbuffer;
	src.width = gctx->bb_width;
	src.height = gctx->bb_height;
	src.pitch = gctx->bb_pitch;
	src.pixel_format = gctx->pixel_format;
	src.is_hardware_memory = 0;

	dst.width = gctx->dst_blt.w;
	dst.height = gctx->dst_blt.h;
	dst.pixel_format = gctx->pixel_format;
	dst.is_hardware_memory = 1;


	if (gctx->fullscreen) {
		if (gctx->is_landscape) {
			if (gctx->y_pitch>0) {
				pitch_x = -gctx->y_pitch;
				/*start of frame-buffer is top-left corner*/
				if (gctx->x_pitch>0) {
					ptr += gctx->screen_h * gctx->y_pitch;
					pitch_y = gctx->x_pitch;
				} 
				/*start of frame-buffer is top-right corner*/
				else {
					ptr += gctx->screen_h * gctx->y_pitch + gctx->screen_w * gctx->x_pitch;
					pitch_y = -gctx->x_pitch;
				}
			} else {
				pitch_x = gctx->y_pitch;
				/*start of frame-buffer is bottom-left corner*/
				if (gctx->x_pitch>0) {
					pitch_y = gctx->x_pitch;
				} 
				/*start of frame-buffer is bottom-right corner*/
				else {
					ptr += gctx->screen_w * gctx->x_pitch;
					pitch_y = gctx->x_pitch;
				}
			}
		}
		if (gctx->erase_dest) {
			gctx->erase_dest = 0;
			GAPI_ClearFS(gctx, ptr, pitch_x, pitch_y);
		}
	} else {
		gctx->dst_blt.x += gctx->off_x;
		gctx->dst_blt.y += gctx->off_y;
	}

	ptr += gctx->dst_blt.x * pitch_x + pitch_y * gctx->dst_blt.y;
	dst.video_buffer = ptr;
	dst.pitch = pitch_y;
		
	gf_stretch_bits(&dst, &src, NULL, NULL, pitch_x, 0xFF, 0, NULL, NULL);

	GXEndDraw();
	gf_mx_v(gctx->mx);
	return GF_OK;
}



static GF_Err GAPI_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	GF_Err e;
	GAPICTX(dr);

	if (!gctx) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);
	
#ifdef GPAC_USE_OGL_ES
	if (gctx->is_3D) {
		if (gctx->fullscreen && gctx->surface && gctx->egldpy) {
			if (gctx->erase_dest) {
				InvalidateRect(gctx->hWnd, NULL, TRUE);
				gctx->erase_dest = 0;
			}
			eglSwapBuffers(gctx->egldpy, gctx->surface);
		} else {
		    InvalidateRect(gctx->hWnd, NULL, gctx->erase_dest);
			gctx->erase_dest = 0;
		}
		gf_mx_v(gctx->mx);
		return GF_OK;
	}
#endif
	e = GF_OK;
	if (gctx->backbuffer) {
		if (dest) {
			gctx->dst_blt = *dest;
		} else {
			assert(0);
			gctx->dst_blt.x = gctx->dst_blt.y = 0;
			gctx->dst_blt.w = gctx->bb_width;
			gctx->dst_blt.h = gctx->bb_height;
		}
		if (gctx->gx_mode) {
			if (!gctx->fullscreen && gctx->erase_dest) {
				InvalidateRect(gctx->hWnd, NULL, TRUE);
				gctx->erase_dest = 0;
			}
			e = GAPI_FlipBackBuffer(dr);
		} else {
			InvalidateRect(gctx->hWnd, NULL, gctx->erase_dest);
			gctx->erase_dest = 0;
		}
	}
	gf_mx_v(gctx->mx);
	return e;
}


static GF_Err GAPI_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	GAPICTX(dr);
	if (!evt) return GF_OK;
	switch (evt->type) {
	case GF_EVT_SHOWHIDE:
		if (gctx->hWnd) ShowWindow(gctx->hWnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	case GF_EVT_SIZE:
	case GF_EVT_VIDEO_SETUP:
#ifdef GPAC_USE_OGL_ES
		if (gctx->is_3D) return GAPI_SetupOGL_ES(the_video_driver);
#endif
		return GAPI_InitSurface(dr, evt->size.width, evt->size.height);
	}
	return GF_OK;
}


static GF_Err GAPI_InitSurface(GF_VideoOutput *dr, u32 VideoWidth, u32 VideoHeight)
{
	GAPICTX(dr);

	if (!gctx || !VideoWidth || !VideoHeight) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);

	GAPI_ReleaseObjects(gctx);

	gctx->bb_size = VideoWidth * VideoHeight * gctx->BPP;
	gctx->bb_width = VideoWidth;
	gctx->bb_height = VideoHeight;
	gctx->bb_pitch = VideoWidth * gctx->BPP;

	if (gctx->force_gx || gctx->fullscreen) {
		gctx->backbuffer = (unsigned char *) malloc(sizeof(unsigned char) * gctx->bb_size);
		gctx->gx_mode = 1;
	} else {
		HDC dc = GetDC(gctx->hWnd);
		createPixmap(gctx, dc, 0);
		::ReleaseDC(gctx->hWnd, dc);
		gctx->gx_mode = 0;
		RECT rc;
		GetWindowRect(gctx->hWnd, &rc);
		gctx->off_x = rc.left;
		gctx->off_y = rc.top;
	}
	gctx->erase_dest = 1;


	gf_mx_v(gctx->mx);
	return GF_OK;
}
static GF_Err GAPI_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	GAPICTX(dr);

	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		vi->width = gctx->bb_width;
		vi->height = gctx->bb_height;
		vi->pitch = gctx->bb_pitch;
		vi->pixel_format = gctx->pixel_format;
		vi->video_buffer = gctx->backbuffer;
		vi->is_hardware_memory = 0;
	}
	return GF_OK;
}


static void *NewGAPIVideoOutput()
{
	GAPIPriv *priv;
	GF_VideoOutput *driv = (GF_VideoOutput *) malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "GAPI Video Output", "gpac distribution")

	priv = (GAPIPriv *) malloc(sizeof(GAPIPriv));
	memset(priv, 0, sizeof(GAPIPriv));
	priv->mx = gf_mx_new();
	driv->opaque = priv;
	priv->force_gx = 0;

	/*alpha and keying to do*/
	driv->hw_caps = GF_VIDEO_HW_CAN_ROTATE;
#ifdef GPAC_USE_OGL_ES
	driv->hw_caps = GF_VIDEO_HW_HAS_OPENGL;
#endif

	driv->Setup = GAPI_Setup;
	driv->Shutdown = GAPI_Shutdown;
	driv->Flush = GAPI_Flush;
	driv->ProcessEvent = GAPI_ProcessEvent;
	driv->Blit = NULL;
	driv->LockBackBuffer = GAPI_LockBackBuffer;
	driv->SetFullScreen = GAPI_SetFullScreen;
	return (void *)driv;
}

static void DeleteVideoOutput(void *ifce)
{
	GF_VideoOutput *driv = (GF_VideoOutput *) ifce;
	GAPICTX(driv);
	GAPI_Shutdown(driv);
	gf_mx_del(gctx->mx);
	free(gctx);
	free(driv);
}

/*interface query*/
Bool QueryInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 1;
	return 0;
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewGAPIVideoOutput();
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VideoOutput *dd = (GF_VideoOutput *)ifce;
	switch (dd->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput(dd);
		break;
	}
}

