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

/*mouse hiding timeout in fullscreen, in milliseconds*/
#define MOUSE_HIDE_TIMEOUT	1000

static const GF_VideoOutput *the_video_output = NULL;

void DD_SetCursor(GF_VideoOutput *dr, u32 cursor_type);

static void DD_GetCoordinates(DWORD lParam, GF_Event *evt)
{
	POINTS pt = MAKEPOINTS(lParam);
	evt->mouse.x = pt.x;
	evt->mouse.y = pt.y;
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


static void mouse_move(DDContext *ctx, GF_VideoOutput *vout)
{
	if (ctx->fullscreen) {
		ctx->last_mouse_move = gf_sys_clock();
		if (ctx->cursor_type==GF_CURSOR_HIDE) DD_SetCursor(vout, ctx->cursor_type_backup);
	}
}

static void mouse_start_timer(DDContext *ctx, HWND hWnd, GF_VideoOutput *vout)
{
	if (ctx->fullscreen) {
		if (!ctx->timer) ctx->timer = SetTimer(hWnd, 10, 1000, NULL);
		mouse_move(ctx, vout);
	}
}

void grab_mouse(DDContext *ctx, GF_VideoOutput *vout)
{
	if (ctx->fullscreen) DD_SetCursor(vout, GF_CURSOR_NORMAL);
	SetCapture(ctx->cur_hwnd);
	mouse_move(ctx, vout);
}
void release_mouse(DDContext *ctx, HWND hWnd, GF_VideoOutput *vout)
{
	ReleaseCapture();
	mouse_start_timer(ctx, hWnd, vout);
}

LRESULT APIENTRY DD_WindowProc(HWND hWnd, UINT msg, UINT wParam, LONG lParam)
{
	GF_Event evt;
	DDContext *ctx;
	GF_VideoOutput *vout = (GF_VideoOutput *) GetWindowLong(hWnd, GWL_USERDATA);

	if (!vout) return DefWindowProc (hWnd, msg, wParam, lParam);
	ctx = (DDContext *)vout->opaque;

	switch (msg) {
	case WM_SIZE:
		/*always notify GPAC since we're not sure the owner of the window is listening to these events*/
		evt.type = GF_EVT_SIZE;
		evt.size.width = LOWORD(lParam);
		evt.size.height = HIWORD(lParam);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_CLOSE:
		if (hWnd==ctx->os_hwnd) {
			evt.type = GF_EVT_QUIT;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		return 1;
	case WM_DESTROY:
		if (ctx->owns_hwnd || (hWnd==ctx->fs_hwnd)) {
			PostQuitMessage (0);
		} else if (ctx->orig_wnd_proc) {
			/*restore window proc*/
			SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, ctx->orig_wnd_proc);
			ctx->orig_wnd_proc = 0L;
		}
		break;
	case WM_ACTIVATE:
#if 1
		if (ctx->fullscreen && (LOWORD(wParam)==WA_INACTIVE) && (hWnd==ctx->fs_hwnd)) {
			evt.type = GF_EVT_SHOWHIDE;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
#endif
		break;

	case WM_SETCURSOR:
		if (ctx->cur_hwnd==hWnd) DD_SetCursor(vout, ctx->cursor_type);
		return 1;
	case WM_ERASEBKGND:
		//InvalidateRect(ctx->cur_hwnd, NULL, TRUE);
		//break;
	case WM_PAINT:
		if (ctx->cur_hwnd==hWnd) {
			evt.type = GF_EVT_REFRESH;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		break;
	case WM_KILLFOCUS:
		if (hWnd==ctx->os_hwnd) ctx->has_focus = 0;
		break;

	case WM_MOUSEMOVE:
		if (ctx->cur_hwnd!=hWnd) break;
		if (ctx->last_mouse_pos != lParam) {
			ctx->last_mouse_pos = lParam;
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVT_MOUSEMOVE;
			DD_GetCoordinates(lParam, &evt);
			vout->on_event(vout->evt_cbk_hdl, &evt);
			mouse_start_timer(ctx, hWnd, vout);
		}
		break;

	case WM_TIMER:
		if (wParam==10) {
			if (ctx->fullscreen && (ctx->cursor_type!=GF_CURSOR_HIDE)) {
				if (gf_sys_clock() > MOUSE_HIDE_TIMEOUT + ctx->last_mouse_move) {
					ctx->cursor_type_backup = ctx->cursor_type;
					DD_SetCursor(vout, GF_CURSOR_HIDE);
					KillTimer(hWnd, ctx->timer);
					ctx->timer = 0;
				}
			}
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVT_LEFTDOWN;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_LBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVT_LEFTUP;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVT_RIGHTDOWN;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_RBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVT_RIGHTUP;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVT_MIDDLEDOWN;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_MBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVT_MIDDLEUP;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MOUSEWHEEL: 
		if (ctx->cur_hwnd==hWnd) {
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVT_MOUSEWHEEL;
			DD_GetCoordinates(lParam, &evt);
			evt.mouse.wheel_pos = FLT2FIX( ((Float) (s16) HIWORD(wParam)) / WHEEL_DELTA );
			vout->on_event(vout->evt_cbk_hdl, &evt);
			mouse_start_timer(ctx, hWnd, vout);
		}
		return 1;

	/*FIXME - there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		evt.key.vk_code = DD_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code) {
			evt.type = (msg==WM_SYSKEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code<=GF_VK_RIGHT) evt.key.virtual_code = 0;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		evt.key.vk_code = DD_TranslateActionKey(wParam);
		evt.key.virtual_code = wParam;
		if (evt.key.vk_code ) {
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_VKEYDOWN : GF_EVT_VKEYUP;
			if (evt.key.vk_code <=GF_VK_RIGHT) evt.key.virtual_code = 0;
			vout->on_event(vout->evt_cbk_hdl, &evt);
			/*also send a normal key for non-key-sensors*/
			if (evt.key.vk_code>GF_VK_RIGHT) goto send_key;
		} else {
send_key:
			evt.type = (msg==WM_KEYDOWN) ? GF_EVT_KEYDOWN : GF_EVT_KEYUP;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		break;
	case WM_CHAR:
		evt.type = GF_EVT_CHAR;
		evt.character.unicode_char = wParam;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x80000 
#define LWA_COLORKEY   0x00000001
#define LWA_ALPHA      0x00000002
#endif
typedef BOOL (WINAPI* typSetLayeredWindowAttributes)(HWND,COLORREF,BYTE,DWORD);


static void SetWindowless(GF_VideoOutput *vout, HWND hWnd)
{
	const char *opt;
	u32 a, r, g, b;
	COLORREF ckey;
	typSetLayeredWindowAttributes _SetLayeredWindowAttributes;
	HMODULE hUser32;
	u32 isWin2K;
	OSVERSIONINFO Version = {sizeof(OSVERSIONINFO)};
	GetVersionEx(&Version);
	isWin2K = (Version.dwPlatformId == VER_PLATFORM_WIN32_NT && Version.dwMajorVersion >= 5); 
	if (!isWin2K) return;
	
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Enabling windowless mode\n"));
	hUser32 = GetModuleHandle("USER32.DLL");
	if (hUser32 == NULL) return;

	_SetLayeredWindowAttributes = (typSetLayeredWindowAttributes) GetProcAddress(hUser32,"SetLayeredWindowAttributes");
	if (_SetLayeredWindowAttributes == NULL) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[DX Out] Win32 layered windows not supported\n"));
		return;
	}

	SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	/*get background ckey*/
	opt = gf_modules_get_option((GF_BaseInterface *)vout, "Rendering", "ColorKey");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)vout, "Rendering", "ColorKey", "FFFEFEFE");
		opt = "FFFEFEFE";
	}

	sscanf(opt, "%02X%02X%02X%02X", &a, &r, &g, &b);
	ckey = RGB(r, g, b);
	if (a<255)
		_SetLayeredWindowAttributes(hWnd, ckey, (u8) a, LWA_COLORKEY|LWA_ALPHA);
	else
		_SetLayeredWindowAttributes(hWnd, ckey, 0, LWA_COLORKEY);

	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[DX Out] Using color key %s\n", opt));
}

 

u32 DD_WindowThread(void *par)
{
	u32 flags;
	RECT rc;
	MSG msg;
	WNDCLASS wc;
	HINSTANCE hInst;
	GF_VideoOutput *vout = par;
	DDContext *ctx = (DDContext *)vout->opaque;

	hInst = GetModuleHandle("gm_dx_hw.dll");
	memset(&wc, 0, sizeof(WNDCLASS));
	wc.style = CS_BYTEALIGNWINDOW;
	wc.hInstance = hInst;
	wc.lpfnWndProc = DD_WindowProc;
	wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	wc.lpszClassName = "GPAC DirectDraw Output";
	RegisterClass (&wc);

	flags = ctx->switch_res;
	ctx->switch_res = 0;

	if (!ctx->os_hwnd) {
		if (flags & GF_TERM_WINDOWLESS) ctx->windowless = 1;

		ctx->os_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC DirectDraw Output", ctx->windowless ? WS_POPUP : WS_OVERLAPPEDWINDOW, 0, 0, 120, 100, NULL, NULL, hInst, NULL);

		if (ctx->os_hwnd == NULL) {
			ctx->th_state = 2;
			return 1;
		}
		if (flags & GF_TERM_INIT_HIDE) {
			ShowWindow(ctx->os_hwnd, SW_HIDE);
		} else {
			SetForegroundWindow(ctx->os_hwnd);
			ShowWindow(ctx->os_hwnd, SW_SHOWNORMAL);
		}

		/*get border & title bar sizes*/
		rc.left = rc.top = 0;
		rc.right = rc.bottom = 100;
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, 0);
		ctx->off_w = rc.right - rc.left - 100;
		ctx->off_h = rc.bottom - rc.top - 100;
		ctx->owns_hwnd = 1;

		if (ctx->windowless) SetWindowless(vout, ctx->os_hwnd);
	}

	ctx->fs_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC DirectDraw FS Output", WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
	if (!ctx->fs_hwnd) {
		ctx->th_state = 2;
		return 1;
	}
	ShowWindow(ctx->fs_hwnd, SW_HIDE);
	ctx->th_state = 1;

	/*if visible set focus*/
	if (!ctx->switch_res) SetFocus(ctx->os_hwnd);

	ctx->switch_res = 0;
	SetWindowLong(ctx->os_hwnd, GWL_USERDATA, (LONG) vout);
	SetWindowLong(ctx->fs_hwnd, GWL_USERDATA, (LONG) vout);

	/*load cursors*/
	ctx->curs_normal = LoadCursor(NULL, IDC_ARROW);
	assert(ctx->curs_normal);
	ctx->curs_hand = LoadCursor(hInst, MAKEINTRESOURCE(IDC_HAND_PTR));
	ctx->curs_collide = LoadCursor(hInst, MAKEINTRESOURCE(IDC_COLLIDE));
	ctx->cursor_type = GF_CURSOR_NORMAL;

	while (GetMessage (&(msg), NULL, 0, 0)) {
		TranslateMessage (&(msg));
		DispatchMessage (&(msg));
	}
	ctx->th_state = 2;
	return 0;
}


void DD_SetupWindow(GF_VideoOutput *dr, u32 flags)
{
	DDContext *ctx = (DDContext *)dr->opaque;

	if (ctx->os_hwnd) {
		ctx->orig_wnd_proc = GetWindowLong(ctx->os_hwnd, GWL_WNDPROC);
		/*override window proc*/
		SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, (DWORD) DD_WindowProc);
	}
	ctx->switch_res = flags;
	/*create our event thread - since we always have a dedicated window for fullscreen, we need that
	even when a window is passed to us*/
	ctx->th = gf_th_new();
	gf_th_run(ctx->th, DD_WindowThread, dr);
	while (!ctx->th_state) gf_sleep(2);

	if (!the_video_output) the_video_output = dr;
}

void DD_ShutdownWindow(GF_VideoOutput *dr)
{
	DDContext *ctx = (DDContext *)dr->opaque;

	if (ctx->owns_hwnd) {
		PostMessage(ctx->os_hwnd, WM_DESTROY, 0, 0);
	} else if (ctx->orig_wnd_proc) {
		/*restore window proc*/
		SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, ctx->orig_wnd_proc);
		ctx->orig_wnd_proc = 0L;
	}
	PostMessage(ctx->fs_hwnd, WM_DESTROY, 0, 0);
	while (ctx->th_state!=2) gf_sleep(10);
	UnregisterClass("GPAC DirectDraw Output", GetModuleHandle("gm_dx_hw.dll"));
	gf_th_del(ctx->th);
	ctx->th = NULL;
	ctx->os_hwnd = NULL;
	ctx->fs_hwnd = NULL;
	the_video_output = NULL;
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
	if (!the_video_output) return NULL;
	return ((DDContext*)the_video_output->opaque)->os_hwnd;
}



GF_Err DD_ProcessEvent(GF_VideoOutput*dr, GF_Event *evt)
{
	DDContext *ctx = (DDContext *)dr->opaque;
	if (!evt) return GF_OK;

	switch (evt->type) {
	case GF_EVT_SET_CURSOR:
		DD_SetCursor(dr, evt->cursor.cursor_type);
		break;
	case GF_EVT_SET_CAPTION:
		if (evt->caption.caption) SetWindowText(ctx->os_hwnd, evt->caption.caption);
		break;
	case GF_EVT_MOVE:
		if (evt->move.relative == 2) {
			u32 x, y, fsw, fsh;
			x = y = 0;
			fsw = GetSystemMetrics(SM_CXSCREEN);
			fsh = GetSystemMetrics(SM_CYSCREEN);

			if (evt->move.align_x==1) x = (fsw - ctx->width) / 2;
			else if (evt->move.align_x==2) x = fsw - ctx->width;

			if (evt->move.align_y==1) y = (fsh - ctx->height) / 2;
			else if (evt->move.align_y==2) y = fsh - ctx->height;

			SetWindowPos(ctx->os_hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
		else if (evt->move.relative) {
			POINT pt;
			pt.x = pt.y = 0;
			MapWindowPoints(ctx->os_hwnd, NULL, &pt, 1);
			SetWindowPos(ctx->os_hwnd, NULL, evt->move.x + pt.x, evt->move.y + pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		} else {
			SetWindowPos(ctx->os_hwnd, NULL, evt->move.x, evt->move.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
		break;
	case GF_EVT_SHOWHIDE:
		ShowWindow(ctx->os_hwnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	/*if scene resize resize window*/
	case GF_EVT_SIZE:
		if (ctx->owns_hwnd) {
			if (ctx->windowless)
				SetWindowPos(ctx->os_hwnd, NULL, 0, 0, evt->size.width, evt->size.height, SWP_NOZORDER | SWP_NOMOVE);
			else 
				SetWindowPos(ctx->os_hwnd, NULL, 0, 0, evt->size.width + ctx->off_w, evt->size.height + ctx->off_h, SWP_NOZORDER | SWP_NOMOVE);

			if (ctx->fullscreen) {
				ctx->store_width = evt->size.width;
				ctx->store_height = evt->size.height;
			}
		}
		break;
	/*HW setup*/
	case GF_EVT_VIDEO_SETUP:
		if (ctx->is_3D_out) {
			ctx->width = evt->size.width;
			ctx->height = evt->size.height;
			return DD_SetupOpenGL(dr);
		}
		return DD_SetBackBufferSize(dr, evt->size.width, evt->size.height);
	}
	return GF_OK;
}
