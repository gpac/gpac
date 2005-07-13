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
#include "resource.h"

/*crude redef of winuser.h due to windows/winsock2 conflicts*/
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL                   0x020A
#define WHEEL_DELTA                     120
#endif

#ifndef WM_MOUSEHOVER
#define WM_MOUSEHOVER                   0x02A1
#endif


void DD_SetCursor(GF_VideoOutput *dr, u32 cursor_type);

static GF_VideoOutput *the_video_driver = NULL;

static void DD_MapBIFSCorrdinate(DWORD lParam, GF_Event *evt)
{
	DDContext *ctx = (DDContext *)the_video_driver->opaque;
	POINTS pt = MAKEPOINTS(lParam);
	if (ctx->fullscreen) {
		evt->mouse.x = pt.x - ctx->fs_store_width / 2;
		evt->mouse.y = ctx->fs_store_height / 2 - pt.y;
	} else {
		evt->mouse.x = pt.x - ctx->width / 2;
		evt->mouse.y = ctx->height / 2 - pt.y;
	}
}

static u32 DD_TranslateActionKey(u32 VirtKey) 
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
	case VK_MENU: 
		return GF_VK_MENU;
	default: return 0;
	}
}

void grab_mouse(DDContext *ctx)
{
	if (ctx->fullscreen) DD_SetCursor(the_video_driver, GF_CURSOR_NORMAL);
	SetCapture(ctx->hWnd);
}

#define USE_MOUSE_HOVER	0

void mouse_hover(DDContext *ctx)
{
#if USE_MOUSE_HOVER
	if (ctx->fullscreen) {
		TRACKMOUSEEVENT hover;
		hover.cbSize = sizeof(TRACKMOUSEEVENT);
		hover.dwFlags = TME_HOVER;
		hover.hwndTrack = ctx->hWnd;
		hover.dwHoverTime = 2000;
		TrackMouseEvent(&hover);
	}
#endif
}
void release_mouse(DDContext *ctx)
{
	ReleaseCapture();
#if USE_MOUSE_HOVER
	mouse_hover(ctx);
#endif
}

LRESULT APIENTRY DD_WindowProc(HWND hWnd, UINT msg, UINT wParam, LONG lParam)
{
	GF_Event evt;
	DDContext *ctx;

	if (!the_video_driver) return DefWindowProc (hWnd, msg, wParam, lParam);

	ctx = (DDContext *)the_video_driver->opaque;
	switch (msg) {
	case WM_SIZE:
		if (!ctx->is_resizing) {
			ctx->is_resizing = 1;
			evt.type = GF_EVT_WINDOWSIZE;
			ctx->width = evt.size.width = LOWORD(lParam);
			ctx->height = evt.size.height = HIWORD(lParam);
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
			ctx->is_resizing = 0;
		}
		break;
	case WM_CLOSE:
		evt.type = GF_EVT_QUIT;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		return 1;
	case WM_DESTROY:
		PostQuitMessage (0);
		break;
	case WM_SETCURSOR:
		DD_SetCursor(the_video_driver, ctx->cursor_type);
		return 1;
	case WM_ERASEBKGND:
	case WM_PAINT:
		evt.type = GF_EVT_REFRESH;
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;

	case WM_MOUSEMOVE:
		if (ctx->last_mouse_pos != lParam) {
			ctx->last_mouse_pos = lParam;
			DD_SetCursor(the_video_driver, (ctx->cursor_type==GF_CURSOR_HIDE) ? GF_CURSOR_NORMAL : ctx->cursor_type);
			evt.type = GF_EVT_MOUSEMOVE;
			DD_MapBIFSCorrdinate(lParam, &evt);
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);

			mouse_hover(ctx);
		}
		break;
	case WM_MOUSEHOVER:
		DD_SetCursor(the_video_driver, GF_CURSOR_HIDE);
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		grab_mouse(ctx);
		evt.type = GF_EVT_LEFTDOWN;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_LBUTTONUP:
		release_mouse(ctx);
		evt.type = GF_EVT_LEFTUP;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		grab_mouse(ctx);
		evt.type = GF_EVT_RIGHTDOWN;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_RBUTTONUP:
		release_mouse(ctx);
		evt.type = GF_EVT_RIGHTUP;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		grab_mouse(ctx);
		evt.type = GF_EVT_MIDDLEDOWN;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_MBUTTONUP:
		release_mouse(ctx);
		evt.type = GF_EVT_MIDDLEUP;
		DD_MapBIFSCorrdinate(lParam, &evt);
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	case WM_MOUSEWHEEL: 
		DD_SetCursor(the_video_driver, GF_CURSOR_NORMAL);
		evt.type = GF_EVT_MOUSEWHEEL;
		DD_MapBIFSCorrdinate(lParam, &evt);
		evt.mouse.wheel_pos = FLT2FIX( ((Float) (s16) HIWORD(wParam)) / WHEEL_DELTA );
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);

		mouse_hover(ctx);
		break;

	/*FIXME - there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		evt.key.vk_code = DD_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_SYSKEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		evt.key.vk_code = DD_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code ) {
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code <=GF_VK_RIGHT) evt.key.virtual_code = 0;
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
		the_video_driver->on_event(the_video_driver->evt_cbk_hdl, &evt);
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

void DD_WindowThread(void *par)
{
	RECT rc;
	MSG msg;
	WNDCLASS wc;
	DDContext *ctx = (DDContext *)the_video_driver->opaque;

	memset(&wc, 0, sizeof(WNDCLASS));
	wc.style = CS_BYTEALIGNWINDOW;
	wc.hInstance = GetModuleHandle("dx_hw.dll");
	wc.lpfnWndProc = DD_WindowProc;
	wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	wc.lpszClassName = "GPAC DirectDraw Output";
	RegisterClass (&wc);
	
	ctx->hWnd = CreateWindow("GPAC DirectDraw Output", "GPAC DirectDraw Output", WS_OVERLAPPEDWINDOW, 0, 0, 120, 100, NULL, NULL, wc.hInstance, NULL);
	if (ctx->hWnd == NULL) {
		ctx->ThreadID = 0;
		ExitThread(1);
	}
	ShowWindow(ctx->hWnd, SW_SHOWNORMAL);

	/*get border & title bar sizes*/
	rc.left = rc.top = 0;
	rc.right = rc.bottom = 100;
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);
	ctx->off_w = rc.right - rc.left - 100;
	ctx->off_h = rc.bottom - rc.top - 100;

	while (GetMessage (&(msg), NULL, 0, 0)) {
		TranslateMessage (&(msg));
		DispatchMessage (&(msg));
	}
	ctx->ThreadID = 0;
	ExitThread (0);
}


void DD_SetupWindow(GF_VideoOutput *dr)
{
	HINSTANCE hInst;
	DDContext *ctx = (DDContext *)dr->opaque;
	if (the_video_driver) return;
	the_video_driver = dr;

	if (!ctx->hWnd) {
		ctx->hThread = CreateThread (NULL, 0, (LPTHREAD_START_ROUTINE) DD_WindowThread, (LPVOID) dr, 0, &(ctx->ThreadID) );
		while (!ctx->hWnd && ctx->hThread) gf_sleep(10);
		if (!ctx->hThread) return;
		ctx->owns_hwnd = 1;
	} else {
		ctx->orig_wnd_proc = GetWindowLong(ctx->hWnd, GWL_WNDPROC);
		/*override window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, (DWORD) DD_WindowProc);
	}

	/*load cursors*/
	ctx->curs_normal = LoadCursor(NULL, IDC_ARROW);
	hInst = GetModuleHandle("dx_hw.dll");
	ctx->curs_hand = LoadCursor(hInst, MAKEINTRESOURCE(IDC_HAND_PTR));
	ctx->curs_collide = LoadCursor(hInst, MAKEINTRESOURCE(IDC_COLLIDE));

}

void DD_ShutdownWindow(GF_VideoOutput *dr)
{
	DDContext *ctx = (DDContext *)dr->opaque;

	if (ctx->owns_hwnd) {
		PostMessage(ctx->hWnd, WM_DESTROY, 0, 0);
		while (ctx->ThreadID) gf_sleep(10);
		UnregisterClass("GPAC DirectDraw Output", GetModuleHandle("dx_hw.dll"));
		CloseHandle(ctx->hThread);
		ctx->hThread = NULL;
	} else {
		/*restore window proc*/
		SetWindowLong(ctx->hWnd, GWL_WNDPROC, ctx->orig_wnd_proc);
		ctx->orig_wnd_proc = 0L;
	}
	ctx->hWnd = NULL;
	the_video_driver = NULL;
}

void DD_SetCursor(GF_VideoOutput *dr, u32 cursor_type)
{
	DDContext *ctx = (DDContext *)dr->opaque;
	if (cursor_type==GF_CURSOR_HIDE) {
		if (ctx->cursor_type!=GF_CURSOR_HIDE) {
			ShowCursor(FALSE);
			ctx->cursor_type = cursor_type;
		}
		return;
	}
	if (ctx->cursor_type==GF_CURSOR_HIDE) ShowCursor(TRUE);
	ctx->cursor_type = cursor_type;

	switch (cursor_type) {
	case GF_CURSOR_ANCHOR:
	case GF_CURSOR_TOUCH:
	case GF_CURSOR_ROTATE:
	case GF_CURSOR_PROXIMITY:
	case GF_CURSOR_PLANE:
		SetCursor(ctx->curs_hand);
		break;
	case GF_CURSOR_COLLIDE:
		SetCursor(ctx->curs_collide);
		break;
	default:
		SetCursor(ctx->curs_normal);
		break;
	}
}

HWND DD_GetGlobalHWND()
{
	if (!the_video_driver) return NULL;
	return ((DDContext*)the_video_driver->opaque)->hWnd;
}



GF_Err DD_PushEvent(GF_VideoOutput*dr, GF_Event *evt)
{
	switch (evt->type) {
	case GF_EVT_SET_CURSOR:
		DD_SetCursor(dr, evt->cursor.cursor_type);
		break;
	case GF_EVT_SET_STYLE:
		break;
	case GF_EVT_SET_CAPTION:
	{
		DDContext *ctx = (DDContext *)dr->opaque;
		if (ctx->hWnd && evt->caption.caption) SetWindowText(ctx->hWnd, evt->caption.caption);
	}
		break;
	case GF_EVT_SHOWHIDE:
	{
		DDContext *ctx = (DDContext *)dr->opaque;
		if (ctx->hWnd) ShowWindow(ctx->hWnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
	}
		break;
	case GF_EVT_NEEDRESIZE:
	{
		DDContext *ctx = (DDContext *)dr->opaque;
		ctx->is_resizing = 1;
		SetWindowPos(ctx->hWnd, NULL, 0, 0, evt->size.width + ctx->off_w, evt->size.height + ctx->off_h, SWP_NOZORDER | SWP_NOMOVE);
		ctx->width = evt->size.width;
		ctx->height = evt->size.height;
		ctx->is_resizing = 0;
		if (ctx->is_3D_out) DD_SetupOpenGL(the_video_driver);
	}
		break;
	}


	return GF_OK;
}
