/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifdef GPAC_USE_OGL_ES

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)

#if 0
# pragma message("Using OpenGL-ES Common Lite Profile")
# pragma comment(lib, "libGLES_CL")

#define GLES_NO_PBUFFER
#define GLES_NO_PIXMAP

#else
# pragma message("Using OpenGL-ES Common Profile")
# pragma comment(lib, "libGLES_CM")

//#define GLES_NO_PIXMAP

#endif

#pragma comment(lib, "gx.lib")
#endif

#endif

static Bool landscape = GF_FALSE;


#define PRINT(__str) OutputDebugString(_T(__str))

#define GAPICTX(dr)	GAPIPriv *gctx = (GAPIPriv *) dr->opaque;

static GF_Err GAPI_InitBackBuffer(GF_VideoOutput *dr, u32 VideoWidth, u32 VideoHeight);

static GF_VideoOutput *the_video_driver = NULL;

static void GAPI_GetCoordinates(DWORD lParam, GF_Event *evt)
{
	GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
	evt->mouse.x = LOWORD(lParam);
	evt->mouse.y = HIWORD(lParam);

	if (ctx->scale_coords) {
		evt->mouse.x = evt->mouse.x * the_video_driver->max_screen_width / ctx->sys_w;
		evt->mouse.y = evt->mouse.y * the_video_driver->max_screen_height / ctx->sys_h;
	}


	if (ctx->fullscreen) {
		POINT pt;
		pt.x = evt->mouse.x;
		pt.y = evt->mouse.y;
		ClientToScreen(ctx->hWnd, &pt);
		if (landscape) {
			evt->mouse.x = ctx->fs_w - pt.y;
			evt->mouse.y = pt.x;
		} else {
			evt->mouse.x = pt.x;
			evt->mouse.y = pt.y;
		}
	}
}

static void w32_translate_key(u32 wParam, u32 lParam, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = wParam;
	switch (wParam) {
	case VK_BACK:
		evt->key_code = GF_KEY_BACKSPACE;
		break;
	case VK_TAB:
		evt->key_code = GF_KEY_TAB;
		break;
	case VK_CLEAR:
		evt->key_code = GF_KEY_CLEAR;
		break;
	case VK_RETURN:
		evt->key_code = GF_KEY_ENTER;
		break;
	case VK_SHIFT:
		evt->key_code = GF_KEY_SHIFT;
		break;
	case VK_CONTROL:
		evt->key_code = GF_KEY_CONTROL;
		break;
	case VK_MENU:
		evt->key_code = GF_KEY_ALT;
		break;
	case VK_PAUSE:
		evt->key_code = GF_KEY_PAUSE;
		break;
	case VK_CAPITAL:
		evt->key_code = GF_KEY_CAPSLOCK;
		break;
	case VK_KANA:
		evt->key_code = GF_KEY_KANAMODE;
		break;
	case VK_JUNJA:
		evt->key_code = GF_KEY_JUNJAMODE;
		break;
	case VK_FINAL:
		evt->key_code = GF_KEY_FINALMODE;
		break;
	case VK_KANJI:
		evt->key_code = GF_KEY_KANJIMODE;
		break;
	case VK_ESCAPE:
		evt->key_code = GF_KEY_ESCAPE;
		break;
	case VK_CONVERT:
		evt->key_code = GF_KEY_CONVERT;
		break;
	case VK_SPACE:
		evt->key_code = GF_KEY_SPACE;
		break;
	case VK_PRIOR:
		evt->key_code = GF_KEY_PAGEUP;
		break;
	case VK_NEXT:
		evt->key_code = GF_KEY_PAGEDOWN;
		break;
	case VK_END:
		evt->key_code = GF_KEY_END;
		break;
	case VK_HOME:
		evt->key_code = GF_KEY_HOME;
		break;
	case VK_LEFT:
		evt->key_code = GF_KEY_LEFT;
		break;
	case VK_UP:
		evt->key_code = GF_KEY_UP;
		break;
	case VK_RIGHT:
		evt->key_code = GF_KEY_RIGHT;
		break;
	case VK_DOWN:
		evt->key_code = GF_KEY_DOWN;
		break;
	case VK_SELECT:
		evt->key_code = GF_KEY_SELECT;
		break;
	case VK_PRINT:
	case VK_SNAPSHOT:
		evt->key_code = GF_KEY_PRINTSCREEN;
		break;
	case VK_EXECUTE:
		evt->key_code = GF_KEY_EXECUTE;
		break;
	case VK_INSERT:
		evt->key_code = GF_KEY_INSERT;
		break;
	case VK_DELETE:
		evt->key_code = GF_KEY_DEL;
		break;
	case VK_HELP:
		evt->key_code = GF_KEY_HELP;
		break;

	/*	case VK_LWIN: return ;
		case VK_RWIN: return ;
		case VK_APPS: return ;
	*/
	case VK_NUMPAD0:
		evt->key_code = GF_KEY_0;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD1:
		evt->key_code = GF_KEY_1;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD2:
		evt->key_code = GF_KEY_2;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD3:
		evt->key_code = GF_KEY_3;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD4:
		evt->key_code = GF_KEY_4;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD5:
		evt->key_code = GF_KEY_5;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD6:
		evt->key_code = GF_KEY_6;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD7:
		evt->key_code = GF_KEY_7;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD8:
		evt->key_code = GF_KEY_8;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_NUMPAD9:
		evt->key_code = GF_KEY_9;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_MULTIPLY:
		evt->key_code = GF_KEY_STAR;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_ADD:
		evt->key_code = GF_KEY_PLUS;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_SEPARATOR:
		evt->key_code = GF_KEY_FULLSTOP;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_SUBTRACT:
		evt->key_code = GF_KEY_HYPHEN;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_DECIMAL:
		evt->key_code = GF_KEY_COMMA;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_DIVIDE:
		evt->key_code = GF_KEY_SLASH;
		evt->flags = GF_KEY_EXT_NUMPAD;
		break;
	case VK_F1:
		evt->key_code = GF_KEY_F1;
		break;
	case VK_F2:
		evt->key_code = GF_KEY_F2;
		break;
	case VK_F3:
		evt->key_code = GF_KEY_F3;
		break;
	case VK_F4:
		evt->key_code = GF_KEY_F4;
		break;
	case VK_F5:
		evt->key_code = GF_KEY_F5;
		break;
//	case VK_F6: evt->key_code = GF_KEY_F6; break;
//	case VK_F7: evt->key_code = GF_KEY_F7; break;
	case VK_F6:
		evt->key_code = GF_KEY_VOLUMEUP;
		break;
	case VK_F7:
		evt->key_code = GF_KEY_VOLUMEDOWN;
		break;
	case VK_F8:
		evt->key_code = GF_KEY_F8;
		break;

	case VK_F9:
		evt->key_code = GF_KEY_F9;
		break;
	case VK_F10:
		evt->key_code = GF_KEY_F10;
		break;
	case VK_F11:
		evt->key_code = GF_KEY_F11;
		break;
	case VK_F12:
		evt->key_code = GF_KEY_F12;
		break;
	case VK_F13:
		evt->key_code = GF_KEY_F13;
		break;
	case VK_F14:
		evt->key_code = GF_KEY_F14;
		break;
	case VK_F15:
		evt->key_code = GF_KEY_F15;
		break;
	case VK_F16:
		evt->key_code = GF_KEY_F16;
		break;
	case VK_F17:
		evt->key_code = GF_KEY_F17;
		break;
	case VK_F18:
		evt->key_code = GF_KEY_F18;
		break;
	case VK_F19:
		evt->key_code = GF_KEY_F19;
		break;
	case VK_F20:
		evt->key_code = GF_KEY_F20;
		break;
	case VK_F21:
		evt->key_code = GF_KEY_F21;
		break;
	case VK_F22:
		evt->key_code = GF_KEY_F22;
		break;
	case VK_F23:
		evt->key_code = GF_KEY_F23;
		break;
	case VK_F24:
		evt->key_code = GF_KEY_F24;
		break;

	case VK_NUMLOCK:
		evt->key_code = GF_KEY_NUMLOCK;
		break;
	case VK_SCROLL:
		evt->key_code = GF_KEY_SCROLL;
		break;

	/*
	 * VK_L* & VK_R* - left and right Alt, Ctrl and Shift virtual keys.
	 * Used only as parameters to GetAsyncKeyState() and GetKeyState().
	 * No other API or message will distinguish left and right keys in this way.
	 */
	case VK_LSHIFT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case VK_RSHIFT:
		evt->key_code = GF_KEY_SHIFT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case VK_LCONTROL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case VK_RCONTROL:
		evt->key_code = GF_KEY_CONTROL;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;
	case VK_LMENU:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_LEFT;
		break;
	case VK_RMENU:
		evt->key_code = GF_KEY_ALT;
		evt->flags = GF_KEY_EXT_RIGHT;
		break;

#if(WINVER >= 0x0400)
	case VK_PROCESSKEY:
		evt->key_code = GF_KEY_PROCESS;
		break;
#endif /* WINVER >= 0x0400 */

	case VK_ATTN:
		evt->key_code = GF_KEY_ATTN;
		break;
	case VK_CRSEL:
		evt->key_code = GF_KEY_CRSEL;
		break;
	case VK_EXSEL:
		evt->key_code = GF_KEY_EXSEL;
		break;
	case VK_EREOF:
		evt->key_code = GF_KEY_ERASEEOF;
		break;
	case VK_PLAY:
		evt->key_code = GF_KEY_PLAY;
		break;
	case VK_ZOOM:
		evt->key_code = GF_KEY_ZOOM;
		break;
	//case VK_NONAME: evt->key_code = GF_KEY_NONAME; break;
	//case VK_PA1: evt->key_code = GF_KEY_PA1; break;
	case VK_OEM_CLEAR:
		evt->key_code = GF_KEY_CLEAR;
		break;

	/*thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
	/* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */
	default:
		if ((wParam>=0x30) && (wParam<=0x39))  evt->key_code = GF_KEY_0 + wParam-0x30;
		else if ((wParam>=0x41) && (wParam<=0x5A))  evt->key_code = GF_KEY_A + wParam-0x41;
		else {
			GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
			short res = (LOWORD(wParam) != 0x5b) ? LOWORD(wParam) : wParam;

			if (res==ctx->keys.vkLeft) evt->key_code = landscape ? GF_KEY_UP : GF_KEY_LEFT;
			else if (res==ctx->keys.vkRight) evt->key_code = landscape ? GF_KEY_DOWN : GF_KEY_RIGHT;
			else if (res==ctx->keys.vkDown) evt->key_code = landscape ? GF_KEY_LEFT : GF_KEY_DOWN;
			else if (res==ctx->keys.vkUp) evt->key_code = landscape ? GF_KEY_RIGHT : GF_KEY_UP;
			else if (res==ctx->keys.vkStart) evt->key_code = GF_KEY_ENTER;
			else if (res==ctx->keys.vkA)
				evt->key_code = GF_KEY_MEDIAPREVIOUSTRACK;
			else if (res==ctx->keys.vkB)
				evt->key_code = GF_KEY_MEDIANEXTTRACK;
			else if (res==ctx->keys.vkC)
				evt->key_code = GF_KEY_SHIFT;
			else if (res==0xc1)
				evt->key_code = GF_KEY_ALT;
			else if (res==0xc2)
				evt->key_code = GF_KEY_CONTROL;
			else if (res==0xc5)
				evt->key_code = GF_KEY_VOLUMEDOWN;
			else {
				evt->key_code = GF_KEY_UNIDENTIFIED;
			}
		}
		break;
	}
}

//#define DIRECT_BITBLT

LRESULT APIENTRY GAPI_WindowProc(HWND hWnd, UINT msg, UINT wParam, LONG lParam)
{
	GF_Event evt;
	switch (msg) {
	case WM_SIZE:
	{
		GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
		evt.type = GF_EVENT_SIZE;
		evt.size.width = LOWORD(lParam);
		evt.size.height = HIWORD(lParam);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
	}
	break;
	case WM_CLOSE:
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_QUIT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		return 1;
	case WM_DESTROY:
	{
		GAPIPriv *ctx = (GAPIPriv *)the_video_driver->opaque;
		if (ctx->owns_hwnd ) {
			PostQuitMessage (0);
		} else if (ctx->orig_wnd_proc) {
			/*restore window proc*/
			SetWindowLong(ctx->hWnd, GWL_WNDPROC, ctx->orig_wnd_proc);
			ctx->orig_wnd_proc = 0L;
		}
	}
	break;

	case WM_ERASEBKGND:
		evt.type = GF_EVENT_REFRESH;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_PAINT:
#ifndef DIRECT_BITBLT
	{
		GAPIPriv *gctx = (GAPIPriv *)the_video_driver->opaque;
		if (gctx->gx_mode || !gctx->bitmap) break;
		HDC dc = GetDC(gctx->hWnd);
		BitBlt(dc, gctx->dst_blt.x, gctx->dst_blt.y, gctx->bb_width, gctx->bb_height, gctx->hdcBitmap, 0, 0, SRCCOPY);
		ReleaseDC(gctx->hWnd, dc);
	}
#endif
	break;

	case WM_MOUSEMOVE:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVENT_MOUSEMOVE;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVENT_MOUSEDOWN;
		evt.mouse.button = GF_MOUSE_LEFT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONUP:
		GAPI_GetCoordinates(lParam, &evt);
		evt.type = GF_EVENT_MOUSEUP;
		evt.mouse.button = GF_MOUSE_LEFT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;

	/*FIXME - there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
		w32_translate_key(wParam, lParam, &evt.key);
		evt.type = ((msg==WM_SYSKEYDOWN) || (msg==WM_KEYDOWN))  ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_CHAR:
		evt.type = GF_EVENT_TEXTINPUT;
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
	wc.hInstance = GetModuleHandle(_T("gm_gapi.dll"));
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
#ifdef GPAC_USE_OGL_ES
	GF_Err e;
#endif
	GAPIPriv *ctx = (GAPIPriv *)dr->opaque;
	if (the_video_driver) return;
	the_video_driver = dr;

	if (!ctx->hWnd) {
		ctx->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) GAPI_WindowThread, (LPVOID) dr, 0, &(ctx->ThreadID) );
		while (!ctx->hWnd && ctx->hThread) gf_sleep(10);
		if (!ctx->hThread) return;
		ctx->owns_hwnd = GF_TRUE;
	} else {
		ctx->orig_wnd_proc = GetWindowLong(ctx->hWnd, GWL_WNDPROC);
		/*override window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, (DWORD) GAPI_WindowProc);
	}

	{
		HDC hdc;
		hdc = GetDC(ctx->hWnd);
		dr->dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
		dr->dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(ctx->hWnd, hdc);
	}


#ifdef GPAC_USE_OGL_ES
	ctx->use_pbuffer = GF_TRUE;
	dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
	e = GAPI_SetupOGL_ES_Offscreen(dr, 20, 20);
	if (e!=GF_OK) {
		dr->hw_caps &= ~GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
#ifndef GLES_NO_PIXMAP
		e = GAPI_SetupOGL_ES_Offscreen(dr, 20, 20);
#endif
	}
	if (!e) {
		dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN;
		return;
	}
	ctx->use_pbuffer = GF_FALSE;

	WNDCLASS wc;
	HINSTANCE hInst;
	hInst = GetModuleHandle(_T("gm_gapi.dll") );
	memset(&wc, 0, sizeof(WNDCLASS));
	wc.hInstance = hInst;
	wc.lpfnWndProc = GAPI_WindowProc;
	wc.lpszClassName = _T("GPAC GAPI Offscreen");
	RegisterClass (&wc);

	ctx->gl_hwnd = CreateWindow(_T("GPAC GAPI Offscreen"), _T("GPAC GAPI Offscreen"), WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
	if (!ctx->gl_hwnd) return;
	ShowWindow(ctx->gl_hwnd, SW_HIDE);

	dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
	e = GAPI_SetupOGL_ES_Offscreen(dr, 20, 20);

	if (e!=GF_OK) {
		dr->hw_caps &= ~GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
		e = GAPI_SetupOGL_ES_Offscreen(dr, 20, 20);
	}
	if (e==GF_OK) dr->hw_caps |= GF_VIDEO_HW_OPENGL_OFFSCREEN;
#endif

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
	} else if (ctx->orig_wnd_proc) {
		/*restore window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, ctx->orig_wnd_proc);
		ctx->orig_wnd_proc = 0L;
	}
	ctx->hWnd = NULL;
#ifdef GPAC_USE_OGL_ES
	PostMessage(ctx->gl_hwnd, WM_DESTROY, 0, 0);
	ctx->gl_hwnd = NULL;
	UnregisterClass(_T("GPAC GAPI Offscreen"), GetModuleHandle(_T("gm_gapi.dll") ));
#endif

	the_video_driver = NULL;
}


GF_Err GAPI_Clear(GF_VideoOutput *dr, u32 color)
{
	GAPICTX(dr);
	gctx->erase_dest = GF_TRUE;
	return GF_OK;
}


static void createPixmap(GAPIPriv *ctx, u32 pix_type)
{
	const size_t    bmiSize = sizeof(BITMAPINFO) + 256U*sizeof(RGBQUAD);
	BITMAPINFO*     bmi;
	DWORD*          p;
	u32 bpel = 0;

	if (ctx->bmi) gf_free(ctx->bmi);

	bmi = (BITMAPINFO*)gf_malloc(bmiSize);
	memset(bmi, 0, bmiSize);

	bmi->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth          = ctx->bb_width;
	bmi->bmiHeader.biHeight         = -1 *  (s32) ctx->bb_height;	/*top-down image*/
	bmi->bmiHeader.biPlanes         = (short)1;
	bmi->bmiHeader.biBitCount       = (unsigned int) ctx->bits_per_pixel;
	bmi->bmiHeader.biCompression    = BI_BITFIELDS;
	bmi->bmiHeader.biClrUsed        = 3;

	p = (DWORD*)bmi->bmiColors;
	switch (ctx->pixel_format) {
	case GF_PIXEL_RGB_555:
		p[0] = 0x00007c00;
		p[1] = 0x000003e0;
		p[2] = 0x0000001f;
		bpel = 16;
		break;
	case GF_PIXEL_RGB_565:
		p[0] = 0x0000f800;
		p[1] = 0x000007e0;
		p[2] = 0x0000001f;
		bpel = 16;
		break;
	case GF_PIXEL_RGB_24:
		p[0] = 0x00ff0000;
		p[1] = 0x0000ff00;
		p[2] = 0x000000ff;
		bpel = 24;
		break;
	}
	ctx->hdc = GetDC(NULL/*ctx->hWnd*/);

	if (pix_type==2) {
#ifdef GPAC_USE_OGL_ES
		ctx->gl_bitmap = CreateDIBSection(ctx->hdc, bmi, DIB_RGB_COLORS, (void **) &ctx->gl_bits, NULL, 0);
#endif
	} else if (pix_type==1) {
		ctx->hdcBitmap = CreateCompatibleDC(ctx->hdc);
		ctx->bitmap = CreateDIBSection(ctx->hdc, bmi, DIB_RGB_COLORS, (void **) &ctx->bits, NULL, 0);
		ctx->old_bitmap = (HBITMAP) SelectObject(ctx->hdcBitmap, ctx->bitmap);
	} else {
		ctx->hdcBitmap = CreateCompatibleDC(ctx->hdc);
		ctx->bitmap = CreateDIBSection(ctx->hdc, bmi, DIB_RGB_COLORS, (void **) &ctx->backbuffer, NULL, 0);
		ctx->old_bitmap = (HBITMAP) SelectObject(ctx->hdcBitmap, ctx->bitmap);

		/*watchout - win32 always create DWORD align memory, so align our pitch*/
		while ((ctx->bb_pitch % 4) != 0) ctx->bb_pitch ++;
	}
	ReleaseDC(NULL/*ctx->hWnd*/, ctx->hdc);

	ctx->bmi = bmi;
//	gf_free(bmi);
}


#ifdef GPAC_USE_OGL_ES

void GAPI_ReleaseOGL_ES(GAPIPriv *ctx, Bool offscreen_only)
{
	if (ctx->egldpy) {
		eglMakeCurrent(ctx->egldpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

		if (ctx->eglctx) eglDestroyContext(ctx->egldpy, ctx->eglctx);
		ctx->eglctx = 0;
		if (ctx->surface) eglDestroySurface(ctx->egldpy, ctx->surface);
		ctx->surface = 0;
		if (ctx->egldpy) eglTerminate(ctx->egldpy);
		ctx->egldpy = 0;
	}
	if (ctx->gl_bitmap) DeleteObject(ctx->gl_bitmap);
	ctx->gl_bitmap = NULL;

	if (offscreen_only) return;

	if (ctx->bitmap) DeleteObject(ctx->bitmap);
	ctx->bitmap = NULL;
}

GF_Err GAPI_SetupOGL_ES(GF_VideoOutput *dr)
{
	EGLint n, maj, min;
	u32 i;
	GF_Event evt;
	GAPICTX(dr)
	static int atts[32];
	const char *opt;

	i=0;
	atts[i++] = EGL_RED_SIZE;
	atts[i++] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : 5;
	atts[i++] = EGL_GREEN_SIZE;
	atts[i++] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : (gctx->pixel_format==GF_PIXEL_RGB_565) ? 6 : 5;
	atts[i++] = EGL_BLUE_SIZE;
	atts[i++] = (gctx->pixel_format==GF_PIXEL_RGB_24) ? 8 : 5;
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	atts[i++] = EGL_DEPTH_SIZE;
	atts[i++] = opt ? atoi(opt) : 16;
	atts[i++] = EGL_SURFACE_TYPE;

#ifdef GLES_NO_PIXMAP
	atts[i++] = EGL_WINDOW_BIT;
#else
	atts[i++] = EGL_PIXMAP_BIT;
//	atts[i++] = gctx->fullscreen ? EGL_WINDOW_BIT : EGL_PIXMAP_BIT;
#endif
	atts[i++] = EGL_ALPHA_SIZE;
	atts[i++] = EGL_DONT_CARE;
	atts[i++] = EGL_STENCIL_SIZE;
	atts[i++] = EGL_DONT_CARE;
	atts[i++] = EGL_NONE;

	/*whenever window is resized we must reinit OGL-ES*/
	GAPI_ReleaseOGL_ES(gctx, GF_FALSE);

	if (!gctx->fullscreen) {
		RECT rc;
		::GetClientRect(gctx->hWnd, &rc);
		gctx->bb_width = rc.right-rc.left;
		gctx->bb_height = rc.bottom-rc.top;

#ifndef GLES_NO_PIXMAP
		createPixmap(gctx, 1);
#endif
	}

	gctx->egldpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!gctx->egldpy) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot get OpenGL display\n"));
		return GF_IO_ERR;
	}
	if (!eglInitialize(gctx->egldpy, &maj, &min)) {
		gctx->egldpy = NULL;
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot initialize OpenGL layer\n"));
		return GF_IO_ERR;
	}

	if (!eglChooseConfig(gctx->egldpy, atts, &gctx->eglconfig, 1, &n) || (eglGetError() != EGL_SUCCESS)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot choose OpenGL config\n"));
		return GF_IO_ERR;
	}

	if (gctx->fullscreen
#ifdef GLES_NO_PIXMAP
	        || 1
#endif
	   ) {
		gctx->surface = eglCreateWindowSurface(gctx->egldpy, gctx->eglconfig, gctx->hWnd, 0);
	} else {
		gctx->surface = eglCreatePixmapSurface(gctx->egldpy, gctx->eglconfig, gctx->bitmap, 0);
	}

	if (!gctx->surface) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot create OpenGL surface - error %d\n", eglGetError()));
		return GF_IO_ERR;
	}
	gctx->eglctx = eglCreateContext(gctx->egldpy, gctx->eglconfig, NULL, NULL);
	if (!gctx->eglctx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot create OpenGL context\n"));
		eglDestroySurface(gctx->egldpy, gctx->surface);
		gctx->surface = 0L;
		return GF_IO_ERR;
	}
	if (!eglMakeCurrent(gctx->egldpy, gctx->surface, gctx->surface, gctx->eglctx)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GAPI] Cannot bind OpenGL context\n"));
		eglDestroyContext(gctx->egldpy, gctx->eglctx);
		gctx->eglctx = 0L;
		eglDestroySurface(gctx->egldpy, gctx->surface);
		gctx->surface = 0L;
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[GAPI] OpenGL initialize - %d x %d \n", gctx->bb_width, gctx->bb_height));
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_VIDEO_SETUP;
	evt.hw_reset = 1;
	dr->on_event(dr->evt_cbk_hdl, &evt);
	return GF_OK;
}



GF_Err GAPI_SetupOGL_ES_Offscreen(GF_VideoOutput *dr, u32 width, u32 height)
{
	int atts[15];
	const char *opt;
	EGLint n, maj, min;

	GAPICTX(dr)

	GAPI_ReleaseOGL_ES(gctx, GF_TRUE);

	if (!gctx->use_pbuffer) {
		SetWindowPos(gctx->gl_hwnd, NULL, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);
		createPixmap(gctx, 2);
	}

	gctx->egldpy = eglGetDisplay(/*gctx->dpy*/EGL_DEFAULT_DISPLAY);
	if (!eglInitialize(gctx->egldpy, &maj, &min)) {
		gctx->egldpy = NULL;
		return GF_IO_ERR;
	}
	atts[0] = EGL_RED_SIZE;
	atts[1] = 8;
	atts[2] = EGL_GREEN_SIZE;
	atts[3] = 8;
	atts[4] = EGL_BLUE_SIZE;
	atts[5] = 8;
	atts[6] = EGL_ALPHA_SIZE;
	atts[7] = (dr->hw_caps & GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA) ? 8 : EGL_DONT_CARE;
	opt = gf_modules_get_option((GF_BaseInterface *)dr, "Video", "GLNbBitsDepth");
	atts[8] = EGL_DEPTH_SIZE;
	atts[9] = opt ? atoi(opt) : 16;

	atts[10] = EGL_STENCIL_SIZE;
	atts[11] = EGL_DONT_CARE;
	atts[12] = EGL_SURFACE_TYPE;
	atts[13] = gctx->use_pbuffer ? EGL_PBUFFER_BIT : EGL_PIXMAP_BIT;
	atts[14] = EGL_NONE;

	eglGetConfigs(gctx->egldpy, NULL, 0, &n);
	if (!eglChooseConfig(gctx->egldpy, atts, &gctx->eglconfig, 1, &n)) {
		return GF_IO_ERR;
	}

	if (!gctx->use_pbuffer) {
		gctx->surface = eglCreatePixmapSurface(gctx->egldpy, gctx->eglconfig, gctx->gl_bitmap, 0);
	} else {
		atts[0] = EGL_WIDTH;
		atts[1] = width;
		atts[2] = EGL_HEIGHT;
		atts[3] = height;
		atts[4] = EGL_NONE;

		gctx->surface = eglCreatePbufferSurface(gctx->egldpy, gctx->eglconfig, atts);
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
	return GF_OK;
}

#endif


void GAPI_ReleaseObjects(GAPIPriv *ctx)
{
	ctx->raw_ptr = NULL;

#ifdef GPAC_USE_OGL_ES
	if (ctx->output_3d_type) GAPI_ReleaseOGL_ES(ctx, GF_FALSE);
	else
#endif
		if (ctx->bitmap) DeleteObject(ctx->bitmap);
		else if (ctx->backbuffer) gf_free(ctx->backbuffer);
	ctx->backbuffer = NULL;
	ctx->bitmap = NULL;

	if (ctx->hdcBitmap) {
		if (ctx->old_bitmap) SelectObject(ctx->hdcBitmap, ctx->old_bitmap);
		ctx->old_bitmap = NULL;
		DeleteDC(ctx->hdcBitmap);
		ctx->hdcBitmap = NULL;
	}
	if (ctx->hdc) ReleaseDC(NULL/*ctx->hWnd*/, ctx->hdc);
	ctx->hdc = NULL;
}

GF_Err GAPI_Setup(GF_VideoOutput *dr, void *os_handle, void *os_display, u32 noover)
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

	GAPI_SetupWindow(dr);
	if (!gctx->hWnd) return GF_IO_ERR;

	/*setup GX*/
	if (!GXOpenDisplay(gctx->hWnd, 0L)) {
		MessageBox(NULL, _T("Cannot open display"), _T("GAPI Error"), MB_OK);
		return GF_IO_ERR;
	}
	GetClientRect(gctx->hWnd, &rc);
	gctx->backup_w = rc.right - rc.left;
	gctx->backup_h = rc.bottom - rc.top;

	return GAPI_InitBackBuffer(dr, gctx->backup_w, gctx->backup_h);
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
	Bool is_wide_scene = (bOn==2) ? GF_TRUE : GF_FALSE;
	GF_Err e;
	GAPICTX(dr);

	if (!gctx) return GF_BAD_PARAM;
	if (is_wide_scene) bOn = GF_TRUE;
	if (bOn == gctx->fullscreen) return GF_OK;

#ifdef GPAC_USE_OGL_ES
	if (gctx->output_3d_type==1) {
		gctx->fullscreen = bOn;
		return GAPI_SetupOGL_ES(dr);
	}
#endif

	gf_mx_p(gctx->mx);
	GAPI_ReleaseObjects(gctx);
	GXCloseDisplay();
	e = GF_OK;
	if (bOn) {
		if (!GXOpenDisplay(GetParent(gctx->hWnd), 0L/*GX_FULLSCREEN*/)) {
			GXOpenDisplay(gctx->hWnd, 0L);
			gctx->fullscreen = GF_FALSE;
			e = GF_IO_ERR;
		} else {
			gctx->fullscreen = GF_TRUE;
		}
	} else {
		GXOpenDisplay(gctx->hWnd, 0L);
		gctx->fullscreen = GF_FALSE;
	}

	landscape = GF_FALSE;
	if (!e) {
		if (gctx->fullscreen) {
			gctx->backup_w = *outWidth;
			gctx->backup_h = *outHeight;

			if (is_wide_scene && (gctx->screen_w < gctx->screen_h)) landscape = GF_TRUE;
			else if (!is_wide_scene && (gctx->screen_w > gctx->screen_h)) landscape = GF_TRUE;
			else landscape = GF_FALSE;

			if (landscape) {
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
	}
	gf_mx_v(gctx->mx);

	return e;
}

GF_Err GAPI_ClearFS(GAPIPriv *gctx, unsigned char *ptr)
{
	gf_mx_p(gctx->mx);
	memset(ptr, 0, sizeof(char) * gctx->screen_w*gctx->screen_h*gctx->BPP);
	gf_mx_v(gctx->mx);
	return GF_OK;
}


static GF_Err GAPI_FlipBackBuffer(GF_VideoOutput *dr)
{
	GF_VideoSurface src, dst;
	unsigned char *ptr;
	GAPICTX(dr);
	if (!gctx || !gctx->gx_mode) return GF_BAD_PARAM;

	/*get a pointer to video memory*/
	if (gctx->gx_mode==1) {
		ptr = (unsigned char *) GXBeginDraw();
	} else {
		ptr = (unsigned char *) gctx->raw_ptr;
	}
	if (!ptr) return GF_IO_ERR;

	src.video_buffer = gctx->backbuffer;
	src.width = gctx->bb_width;
	src.height = gctx->bb_height;
	src.pitch_x = gctx->BPP;
	src.pitch_y = gctx->y_pitch;
	src.pixel_format = gctx->pixel_format;
	src.is_hardware_memory = GF_FALSE;


	dst.width = gctx->dst_blt.w;
	dst.height = gctx->dst_blt.h;
	dst.pixel_format = gctx->pixel_format;
	dst.is_hardware_memory = GF_TRUE;
	dst.video_buffer = (char*)ptr;
	dst.pitch_x = gctx->x_pitch;
	dst.pitch_y = gctx->y_pitch;

	if (gctx->fullscreen) {
		if (gctx->erase_dest) {
			gctx->erase_dest = GF_FALSE;
			GAPI_ClearFS(gctx, ptr);
		}
	} else {
		gctx->dst_blt.x += gctx->off_x;
		gctx->dst_blt.y += gctx->off_y;
	}

	/*apply x/y offset*/
	if (!gctx->fullscreen)
		dst.video_buffer += gctx->dst_blt.x * gctx->x_pitch + gctx->y_pitch * gctx->dst_blt.y;

	if (gctx->contiguous_mem) {
		memcpy(dst.video_buffer, src.video_buffer, src.width*gctx->BPP*src.height );
	} else if (landscape) {
		u32 y, lsize = dst.height*gctx->x_pitch;
		for (y=0; y<dst.width; y++) {
			char *s = src.video_buffer + y*gctx->y_pitch;
			char *d = dst.video_buffer + y*gctx->y_pitch;
			memcpy(d, s, lsize);
		}
	} else {
		u32 y, lsize = dst.width*gctx->x_pitch;
		for (y=0; y<dst.height; y++) {
			char *s = src.video_buffer + y*gctx->y_pitch;
			char *d = dst.video_buffer + y*gctx->y_pitch;
			memcpy(d, s, lsize);
		}
	}

	if (gctx->gx_mode==1) GXEndDraw();
	return GF_OK;
}



static GF_Err GAPI_Flush(GF_VideoOutput *dr, GF_Window *dest)
{
	GF_Err e;
	GAPICTX(dr);

	if (!gctx) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);

#ifdef GPAC_USE_OGL_ES
	if (gctx->output_3d_type==1) {
#ifndef GLES_NO_PIXMAP
		if (gctx->fullscreen && gctx->surface && gctx->egldpy) {
#endif
			if (gctx->erase_dest) {
				InvalidateRect(gctx->hWnd, NULL, TRUE);
				gctx->erase_dest = GF_FALSE;
			}
			eglSwapBuffers(gctx->egldpy, gctx->surface);
#ifndef GLES_NO_PIXMAP
		} else {
			InvalidateRect(gctx->hWnd, NULL, gctx->erase_dest);
			gctx->erase_dest = GF_FALSE;
		}
#endif
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
				gctx->erase_dest = GF_FALSE;
			}
			e = GAPI_FlipBackBuffer(dr);
		} else {
#ifndef DIRECT_BITBLT
			InvalidateRect(gctx->hWnd, NULL, gctx->erase_dest);
#else
//			BitBlt(gctx->hdc, gctx->dst_blt.x, gctx->dst_blt.y, gctx->bb_width, gctx->bb_height, gctx->hdcBitmap, 0, 0, SRCCOPY);
			HDC dc = GetDC(NULL);
			BitBlt(dc, gctx->dst_blt.x, gctx->dst_blt.y, gctx->bb_width, gctx->bb_height, gctx->hdcBitmap, 0, 0, SRCCOPY);
			ReleaseDC(NULL, dc);
#endif
			gctx->erase_dest = GF_FALSE;
		}
	}
	gf_mx_v(gctx->mx);
	return e;
}

u32 get_sys_col(int idx)
{
	u32 res;
	DWORD val = GetSysColor(idx);
	res = (val)&0xFF;
	res<<=8;
	res |= (val>>8)&0xFF;
	res<<=8;
	res |= (val>>16)&0xFF;
	return res;
}

static GF_Err GAPI_ProcessEvent(GF_VideoOutput *dr, GF_Event *evt)
{
	GAPICTX(dr);
	if (!evt) return GF_OK;
	switch (evt->type) {
	case GF_EVENT_SHOWHIDE:
		if (gctx->hWnd) ShowWindow(gctx->hWnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	case GF_EVENT_SIZE:
		/*nothing to do since we don't own the window*/
		break;
	case GF_EVENT_VIDEO_SETUP:
		switch (evt->setup.opengl_mode) {
		case 0:
#ifdef GPAC_USE_OGL_ES
			gctx->output_3d_type = 0;
#endif
			return GAPI_InitBackBuffer(dr, evt->setup.width, evt->setup.height);
#ifdef GPAC_USE_OGL_ES
		case 1:
			gctx->output_3d_type = 1;
			return GAPI_SetupOGL_ES(the_video_driver);
		case 2:
			gctx->output_3d_type = 2;
			return GAPI_SetupOGL_ES_Offscreen(the_video_driver, evt->setup.width, evt->setup.height);
#else
		default:
			return GF_NOT_SUPPORTED;
#endif
		}
	case GF_EVENT_SYS_COLORS:
		evt->sys_cols.sys_colors[0] = get_sys_col(COLOR_ACTIVEBORDER);
		evt->sys_cols.sys_colors[1] = get_sys_col(COLOR_ACTIVECAPTION);
		evt->sys_cols.sys_colors[2] = get_sys_col(COLOR_APPWORKSPACE);
		evt->sys_cols.sys_colors[3] = get_sys_col(COLOR_BACKGROUND);
		evt->sys_cols.sys_colors[4] = get_sys_col(COLOR_BTNFACE);
		evt->sys_cols.sys_colors[5] = get_sys_col(COLOR_BTNHIGHLIGHT);
		evt->sys_cols.sys_colors[6] = get_sys_col(COLOR_BTNSHADOW);
		evt->sys_cols.sys_colors[7] = get_sys_col(COLOR_BTNTEXT);
		evt->sys_cols.sys_colors[8] = get_sys_col(COLOR_CAPTIONTEXT);
		evt->sys_cols.sys_colors[9] = get_sys_col(COLOR_GRAYTEXT);
		evt->sys_cols.sys_colors[10] = get_sys_col(COLOR_HIGHLIGHT);
		evt->sys_cols.sys_colors[11] = get_sys_col(COLOR_HIGHLIGHTTEXT);
		evt->sys_cols.sys_colors[12] = get_sys_col(COLOR_INACTIVEBORDER);
		evt->sys_cols.sys_colors[13] = get_sys_col(COLOR_INACTIVECAPTION);
		evt->sys_cols.sys_colors[14] = get_sys_col(COLOR_INACTIVECAPTIONTEXT);
		evt->sys_cols.sys_colors[15] = get_sys_col(COLOR_INFOBK);
		evt->sys_cols.sys_colors[16] = get_sys_col(COLOR_INFOTEXT);
		evt->sys_cols.sys_colors[17] = get_sys_col(COLOR_MENU);
		evt->sys_cols.sys_colors[18] = get_sys_col(COLOR_MENUTEXT);
		evt->sys_cols.sys_colors[19] = get_sys_col(COLOR_SCROLLBAR);
		evt->sys_cols.sys_colors[20] = get_sys_col(COLOR_3DDKSHADOW);
		evt->sys_cols.sys_colors[21] = get_sys_col(COLOR_3DFACE);
		evt->sys_cols.sys_colors[22] = get_sys_col(COLOR_3DHIGHLIGHT);
		evt->sys_cols.sys_colors[23] = get_sys_col(COLOR_3DLIGHT);
		evt->sys_cols.sys_colors[24] = get_sys_col(COLOR_3DSHADOW);
		evt->sys_cols.sys_colors[25] = get_sys_col(COLOR_WINDOW);
		evt->sys_cols.sys_colors[26] = get_sys_col(COLOR_WINDOWFRAME);
		evt->sys_cols.sys_colors[27] = get_sys_col(COLOR_WINDOWTEXT);
		return GF_OK;
	}
	return GF_OK;
}

#if 1



#define ESC_QUERYESCSUPPORT 8
#define GETGXINFO			0x00020000

typedef struct GXDeviceInfo
{
	long Version;           //00
	void * pvFrameBuffer;   //04
	unsigned long cbStride; //08
	unsigned long cxWidth;  //0c
	unsigned long cyHeight; //10
	unsigned long cBPP;     //14
	unsigned long ffFormat; //18
	char Unused[0x84-7*4];
} GXDeviceInfo;


static Bool check_resolution_switch(GF_VideoOutput *dr, u32 width, u32 height)
{
	GAPICTX(dr);

	gctx->sys_w = GetSystemMetrics(SM_CXSCREEN);
	gctx->sys_h = GetSystemMetrics(SM_CYSCREEN);
	gctx->scale_coords = GF_FALSE;
	if (gctx->sys_w != width) gctx->scale_coords = GF_TRUE;
	else if (gctx->sys_h != height) gctx->scale_coords = GF_TRUE;

	if (gctx->scale_coords) {
		gctx->off_x = gctx->off_x * width / gctx->sys_w;
		gctx->off_y = gctx->off_y * height / gctx->sys_h;
	}

	HDC hdc = GetDC(gctx->hWnd);
	dr->dpi_x = (u32) (width * 25.4 / GetDeviceCaps(hdc, HORZSIZE) );
	dr->dpi_y = (u32) (height * 25.4 / GetDeviceCaps(hdc, VERTSIZE) );
	ReleaseDC(gctx->hWnd, hdc);

	if ((gctx->screen_w==width) && (gctx->screen_h==height)) return GF_FALSE;

	GF_Event evt;
	dr->max_screen_width = gctx->screen_w = width;
	dr->max_screen_height = gctx->screen_h = height;
	dr->max_screen_bpp = 8;//we don't filter for bpp less than 8

	evt.type = GF_EVENT_RESOLUTION;
	evt.size.width = dr->max_screen_width;
	evt.size.height = dr->max_screen_height;
	dr->on_event(the_video_driver->evt_cbk_hdl, &evt);

	return GF_TRUE;
}

#ifndef GETRAWFRAMEBUFFER
#define GETRAWFRAMEBUFFER   0x00020001
typedef struct _RawFrameBufferInfo
{
	WORD wFormat;
	WORD wBPP;
	VOID *pFramePointer;
	int	cxStride;
	int	cyStride;
	int cxPixels;
	int cyPixels;
} RawFrameBufferInfo;

#define FORMAT_565 1
#define FORMAT_555 2
#define FORMAT_OTHER 3
#endif

static GF_Err gapi_get_raw_fb(GF_VideoOutput *dr)
{
	long tmp;
	RawFrameBufferInfo Info;
	GAPICTX(dr);
	HDC DC = GetDC(NULL);
	memset(&Info,0,sizeof(RawFrameBufferInfo));

	ExtEscape(DC, GETRAWFRAMEBUFFER, 0, NULL, sizeof(RawFrameBufferInfo), (char*)&Info);
	if (!Info.pFramePointer /* && QueryPlatform(PLATFORM_VER) >= 421*/ )
	{
		//try gxinfo
		DWORD Code = GETGXINFO;
		if (ExtEscape(DC, ESC_QUERYESCSUPPORT, sizeof(DWORD), (char*)&Code, 0, NULL) > 0)
		{
			DWORD DCWidth = GetDeviceCaps(DC,HORZRES);
			DWORD DCHeight = GetDeviceCaps(DC,VERTRES);
			GXDeviceInfo GXInfo;
			memset(&GXInfo,0,sizeof(GXInfo));
			GXInfo.Version = 100;
			ExtEscape(DC, GETGXINFO, 0, NULL, sizeof(GXInfo), (char*)&GXInfo);

			// avoid VGA devices (or QVGA smartphones emulating 176x220)
			if (GXInfo.cbStride>0 && !(GXInfo.ffFormat & kfLandscape) &&
			        ((DCWidth == GXInfo.cxWidth && DCHeight == GXInfo.cyHeight) ||
			         (DCWidth == GXInfo.cyHeight && DCHeight == GXInfo.cxWidth)))
			{
				Bool Detect = GF_FALSE;
				int* p = (int*)GXInfo.pvFrameBuffer;
				COLORREF Old = GetPixel(DC,0,0);
				*p ^= -1;
				Detect = (Bool) ( GetPixel(DC,0,0) != Old );
				*p ^= -1;

				if (Detect)
				{
					Info.pFramePointer = GXInfo.pvFrameBuffer;
					Info.cxPixels = GXInfo.cxWidth;
					Info.cyPixels = GXInfo.cyHeight;
					Info.cxStride = GXInfo.cBPP/8;
					Info.cyStride = GXInfo.cbStride;
					Info.wBPP = (WORD)GXInfo.cBPP;
					Info.wFormat = (WORD)GXInfo.ffFormat;
				}
			}
		}
	}

	ReleaseDC(NULL,DC);

	if (!Info.pFramePointer) return GF_NOT_SUPPORTED;

	gctx->x_pitch = Info.cxStride;
	gctx->y_pitch = Info.cyStride;

	if (abs(Info.cyStride) < abs(Info.cxStride))
	{
		if (abs(Info.cxStride)*8 < Info.cyPixels*Info.wBPP &&
		        abs(Info.cxStride)*8 >= Info.cxPixels*Info.wBPP) { //swapped resolution
			tmp = Info.cxPixels;
			Info.cxPixels = Info.cyPixels;
			Info.cyPixels = tmp;
		}
	}
	else
	{
		if (abs(Info.cyStride)*8 < Info.cxPixels*Info.wBPP &&
		        abs(Info.cyStride)*8 >= Info.cyPixels*Info.wBPP) {//swapped resolution
			tmp = Info.cxPixels;
			Info.cxPixels = Info.cyPixels;
			Info.cyPixels = tmp;
		}
	}

	gctx->raw_ptr = (unsigned char *)Info.pFramePointer;

	if (Info.cxStride<0)
		gctx->raw_ptr += (Info.cxStride * (Info.cxPixels-1));
	if (Info.cyStride<0)
		gctx->raw_ptr += (Info.cyStride * (Info.cyPixels-1));

	if (check_resolution_switch(dr, Info.cxPixels, Info.cyPixels))
		return GF_EOS;

	return GF_OK;
}

#endif


static GF_Err GAPI_InitBackBuffer(GF_VideoOutput *dr, u32 VideoWidth, u32 VideoHeight)
{
	u32 gx_mode;
	GAPICTX(dr);

	if (!gctx || !VideoWidth || !VideoHeight) return GF_BAD_PARAM;

	gf_mx_p(gctx->mx);

	GAPI_ReleaseObjects(gctx);

	/*flip W&H in landscape mode*/
	if (landscape) {
		u32 t = VideoWidth;
		VideoWidth = VideoHeight;
		VideoHeight = t;
	}

	RECT rc;
	GetWindowRect(gctx->hWnd, &rc);
	gctx->off_x = rc.left;
	gctx->off_y = rc.top;
	gctx->erase_dest = GF_TRUE;

	const char *opt = gf_modules_get_option((GF_BaseInterface *)dr, "GAPI", "FBAccess");
	if (!opt || !strcmp(opt, "raw")) gx_mode = 2;
	else if (opt && !strcmp(opt, "gx")) gx_mode = 1;
	else gx_mode = 0;

	if ((gx_mode != gctx->gx_mode) || !gctx->screen_w) {
		struct GXDisplayProperties gx = GXGetDisplayProperties();

		gctx->x_pitch = gx.cbxPitch;
		gctx->y_pitch = gx.cbyPitch;

		gctx->gx_mode = gx_mode;
		if (gctx->gx_mode==2) {
			if (gapi_get_raw_fb(dr) == GF_EOS) {
				gf_mx_v(gctx->mx);
				return GF_OK;
			}
		} else if (check_resolution_switch(dr, gx.cxWidth, gx.cyHeight)) {
			gf_mx_v(gctx->mx);
			return GF_OK;
		}
	}
	if (gctx->gx_mode==2) {
		GF_Err e = gapi_get_raw_fb(dr);
		if (e) {
			gf_mx_v(gctx->mx);
			if (e==GF_EOS) return GF_OK;
			else return e;
		}
	}


	gctx->bb_size = VideoWidth * VideoHeight * gctx->BPP;
	gctx->bb_width = VideoWidth;
	gctx->bb_height = VideoHeight;
	gctx->bb_pitch = VideoWidth * gctx->BPP;


	if (gctx->gx_mode) {
		gctx->backbuffer = (char *) gf_malloc(sizeof(unsigned char) * gctx->bb_size);

		gctx->contiguous_mem = ((gctx->x_pitch==gctx->BPP) && (gctx->y_pitch==gctx->screen_w*gctx->BPP)) ? GF_TRUE : GF_FALSE;
	} else {
		createPixmap(gctx, 0);
	}

	gf_mx_v(gctx->mx);
	return GF_OK;
}


static void GAPI_AdjustLandscape(GAPIPriv *gctx, GF_VideoSurface *dst, s32 x_pitch, s32 y_pitch)
{
	if (y_pitch>0) {
#if 1
		dst->pitch_x = -y_pitch;
		/*start of frame-buffer is top-left corner*/
		if (x_pitch>0) {
			dst->video_buffer += dst->height * y_pitch;
			dst->pitch_y = x_pitch;
		}
		/*start of frame-buffer is top-right corner*/
		else {
			dst->video_buffer += dst->height * y_pitch + dst->width * x_pitch;
			dst->pitch_y = -x_pitch;
		}
#else
		dst->pitch_x = y_pitch;
		/*start of frame-buffer is top-left corner*/
		if (x_pitch>0) {
			dst->video_buffer += y_pitch - x_pitch;
			dst->pitch_y = -x_pitch;
		}
		/*start of frame-buffer is top-right corner*/
		else {
			dst->video_buffer += dst->height * y_pitch + dst->width * x_pitch;
			dst->pitch_y = -x_pitch;
		}
#endif
	} else {
		dst->pitch_x = y_pitch;
		/*start of frame-buffer is bottom-left corner*/
		if (x_pitch>0) {
			dst->pitch_y = x_pitch;
		}
		/*start of frame-buffer is bottom-right corner*/
		else {
			dst->video_buffer += dst->width * x_pitch;
			dst->pitch_y = x_pitch;
		}
	}
	u32 t = dst->width;
	dst->width = dst->height;
	dst->height = t;
}

static GF_Err GAPI_LockBackBuffer(GF_VideoOutput *dr, GF_VideoSurface *vi, Bool do_lock)
{
	GAPICTX(dr);

	if (do_lock) {
		if (!vi) return GF_BAD_PARAM;
		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->width = gctx->bb_width;
		vi->height = gctx->bb_height;
		vi->video_buffer = gctx->backbuffer;
		vi->pixel_format = gctx->pixel_format;
		vi->is_hardware_memory = GF_FALSE;
		vi->pitch_x = gctx->x_pitch;
		vi->pitch_y = gctx->y_pitch;

		if (landscape)
			GAPI_AdjustLandscape(gctx, vi, gctx->x_pitch, gctx->y_pitch);
	}
	return GF_OK;
}


static void *NewGAPIVideoOutput()
{
	GAPIPriv *priv;
	GF_VideoOutput *driv = (GF_VideoOutput *) gf_malloc(sizeof(GF_VideoOutput));
	memset(driv, 0, sizeof(GF_VideoOutput));
	GF_REGISTER_MODULE_INTERFACE(driv, GF_VIDEO_OUTPUT_INTERFACE, "GAPI Video Output", "gpac distribution")

	priv = (GAPIPriv *) gf_malloc(sizeof(GAPIPriv));
	memset(priv, 0, sizeof(GAPIPriv));
	priv->mx = gf_mx_new("GAPI");
	driv->opaque = priv;

#ifdef GPAC_USE_OGL_ES
	driv->hw_caps = GF_VIDEO_HW_OPENGL | GF_VIDEO_HW_OPENGL_OFFSCREEN | GF_VIDEO_HW_OPENGL_OFFSCREEN_ALPHA;
#endif
	/*rgb, yuv to do*/

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
	gf_free(gctx);
	gf_free(driv);
}

#ifdef __cplusplus
extern "C" {
#endif


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
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return (GF_BaseInterface *) NewGAPIVideoOutput();
	return NULL;
}

/*interface destroy*/
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	GF_VideoOutput *dd = (GF_VideoOutput *)ifce;
	switch (dd->InterfaceType) {
	case GF_VIDEO_OUTPUT_INTERFACE:
		DeleteVideoOutput(dd);
		break;
	}
}

GPAC_MODULE_STATIC_DELARATION( gapi )

#ifdef __cplusplus
}
#endif

