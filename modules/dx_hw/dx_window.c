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

static void w32_translate_key(u32 wParam, u32 lParam, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = wParam;
	switch (wParam) {
	case VK_BACK: evt->key_code = GF_KEY_BACKSPACE; break;
	case VK_TAB: evt->key_code = GF_KEY_TAB; break;
	case VK_CLEAR: evt->key_code = GF_KEY_CLEAR; break;
	case VK_RETURN: evt->key_code = GF_KEY_ENTER; break;
	case VK_SHIFT: evt->key_code = GF_KEY_SHIFT; break;
	case VK_CONTROL: evt->key_code = GF_KEY_CONTROL; break;
	case VK_MENU: evt->key_code = GF_KEY_ALT; break;
	case VK_PAUSE: evt->key_code = GF_KEY_PAUSE; break;
	case VK_CAPITAL: evt->key_code = GF_KEY_CAPSLOCK; break;
	case VK_KANA: evt->key_code = GF_KEY_KANAMODE; break;
	case VK_JUNJA: evt->key_code = GF_KEY_JUNJAMODE; break;
	case VK_FINAL: evt->key_code = GF_KEY_FINALMODE; break;
	case VK_KANJI: evt->key_code = GF_KEY_KANJIMODE; break;
	case VK_ESCAPE: evt->key_code = GF_KEY_ESCAPE; break;
	case VK_CONVERT: evt->key_code = GF_KEY_CONVERT; break;
	case VK_NONCONVERT: evt->key_code = GF_KEY_NONCONVERT; break;
	case VK_ACCEPT: evt->key_code = GF_KEY_ACCEPT; break;
	case VK_MODECHANGE: evt->key_code = GF_KEY_MODECHANGE; break;
	case VK_SPACE: evt->key_code = GF_KEY_SPACE; break;
	case VK_PRIOR: evt->key_code = GF_KEY_PAGEUP; break;
	case VK_NEXT: evt->key_code = GF_KEY_PAGEDOWN; break;
	case VK_END: evt->key_code = GF_KEY_END; break;
	case VK_HOME: evt->key_code = GF_KEY_HOME; break;
	case VK_LEFT: evt->key_code = GF_KEY_LEFT; break;
	case VK_UP: evt->key_code = GF_KEY_UP; break;
	case VK_RIGHT: evt->key_code = GF_KEY_RIGHT; break;
	case VK_DOWN: evt->key_code = GF_KEY_DOWN; break;
	case VK_SELECT: evt->key_code = GF_KEY_SELECT; break;
	case VK_PRINT: 
	case VK_SNAPSHOT:
		evt->key_code = GF_KEY_PRINTSCREEN; break;
	case VK_EXECUTE: evt->key_code = GF_KEY_EXECUTE; break;
	case VK_INSERT: evt->key_code = GF_KEY_INSERT; break;
	case VK_DELETE: evt->key_code = GF_KEY_DEL; break;
	case VK_HELP: evt->key_code = GF_KEY_HELP; break;

/*	case '!': evt->key_code = GF_KEY_EXCLAMATION; break;
	case '"': evt->key_code = GF_KEY_QUOTATION; break;
	case '#': evt->key_code = GF_KEY_NUMBER; break;
	case '$': evt->key_code = GF_KEY_DOLLAR; break;
	case '&': evt->key_code = GF_KEY_AMPERSAND; break;
	case '\'': evt->key_code = GF_KEY_APOSTROPHE; break;
	case '(': evt->key_code = GF_KEY_LEFTPARENTHESIS; break;
	case ')': evt->key_code = GF_KEY_RIGHTPARENTHESIS; break;
	case ',': evt->key_code = GF_KEY_COMMA; break;
	case ':': evt->key_code = GF_KEY_COLON; break;
	case ';': evt->key_code = GF_KEY_SEMICOLON; break;
	case '<': evt->key_code = GF_KEY_LESSTHAN; break;
	case '>': evt->key_code = GF_KEY_GREATERTHAN; break;
	case '?': evt->key_code = GF_KEY_QUESTION; break;
	case '@': evt->key_code = GF_KEY_AT; break;
	case '[': evt->key_code = GF_KEY_LEFTSQUAREBRACKET; break;
	case ']': evt->key_code = GF_KEY_RIGHTSQUAREBRACKET; break;
	case '\\': evt->key_code = GF_KEY_BACKSLASH; break;
	case '_': evt->key_code = GF_KEY_UNDERSCORE; break;
	case '`': evt->key_code = GF_KEY_GRAVEACCENT; break;
	case ' ': evt->key_code = GF_KEY_SPACE; break;
	case '/': evt->key_code = GF_KEY_SLASH; break;
	case '*': evt->key_code = GF_KEY_STAR; break;
	case '-': evt->key_code = GF_KEY_HIPHEN; break;
	case '+': evt->key_code = GF_KEY_PLUS; break;
	case '=': evt->key_code = GF_KEY_EQUALS; break;
	case '^': evt->key_code = GF_KEY_CIRCUM; break;
	case '{': evt->key_code = GF_KEY_LEFTCURLYBRACKET; break;
	case '}': evt->key_code = GF_KEY_RIGHTCURLYBRACKET; break;
	case '|': evt->key_code = GF_KEY_PIPE; break;
*/		


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
	case VK_F1: evt->key_code = GF_KEY_F1; break;
	case VK_F2: evt->key_code = GF_KEY_F2; break;
	case VK_F3: evt->key_code = GF_KEY_F3; break;
	case VK_F4: evt->key_code = GF_KEY_F4; break;
	case VK_F5: evt->key_code = GF_KEY_F5; break;
	case VK_F6: evt->key_code = GF_KEY_F6; break;
	case VK_F7: evt->key_code = GF_KEY_F7; break;
	case VK_F8: evt->key_code = GF_KEY_F8; break;
	case VK_F9: evt->key_code = GF_KEY_F9; break;
	case VK_F10: evt->key_code = GF_KEY_F10; break;
	case VK_F11: evt->key_code = GF_KEY_F11; break;
	case VK_F12: evt->key_code = GF_KEY_F12; break;
	case VK_F13: evt->key_code = GF_KEY_F13; break;
	case VK_F14: evt->key_code = GF_KEY_F14; break;
	case VK_F15: evt->key_code = GF_KEY_F15; break;
	case VK_F16: evt->key_code = GF_KEY_F16; break;
	case VK_F17: evt->key_code = GF_KEY_F17; break;
	case VK_F18: evt->key_code = GF_KEY_F18; break;
	case VK_F19: evt->key_code = GF_KEY_F19; break;
	case VK_F20: evt->key_code = GF_KEY_F20; break;
	case VK_F21: evt->key_code = GF_KEY_F21; break;
	case VK_F22: evt->key_code = GF_KEY_F22; break;
	case VK_F23: evt->key_code = GF_KEY_F23; break;
	case VK_F24: evt->key_code = GF_KEY_F24; break;

	case VK_NUMLOCK: evt->key_code = GF_KEY_NUMLOCK; break;
	case VK_SCROLL: evt->key_code = GF_KEY_SCROLL; break;

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
	case VK_PROCESSKEY: evt->key_code = GF_KEY_PROCESS; break;
#endif /* WINVER >= 0x0400 */

	case VK_ATTN: evt->key_code = GF_KEY_ATTN; break;
	case VK_CRSEL: evt->key_code = GF_KEY_CRSEL; break;
	case VK_EXSEL: evt->key_code = GF_KEY_EXSEL; break;
	case VK_EREOF: evt->key_code = GF_KEY_ERASEEOF; break;
	case VK_PLAY: evt->key_code = GF_KEY_PLAY; break;
	case VK_ZOOM: evt->key_code = GF_KEY_ZOOM; break;
	//case VK_NONAME: evt->key_code = GF_KEY_NONAME; break;
	//case VK_PA1: evt->key_code = GF_KEY_PA1; break;
	case VK_OEM_CLEAR: evt->key_code = GF_KEY_CLEAR; break;

	/*thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) */
	/* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) */
	default: 
		if ((wParam>=0x30) && (wParam<=0x39))  evt->key_code = GF_KEY_0 + wParam-0x30;
		else if ((wParam>=0x41) && (wParam<=0x5A))  evt->key_code = GF_KEY_A + wParam-0x41;
		/*DOM 3 Events: Implementations that are unable to identify a key must use the key identifier "Unidentified".*/
		else
			evt->key_code = GF_KEY_UNIDENTIFIED;
		break;
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
		evt.type = GF_EVENT_SIZE;
		evt.size.width = LOWORD(lParam);
		evt.size.height = HIWORD(lParam);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_CLOSE:
		if (hWnd==ctx->os_hwnd) {
			evt.type = GF_EVENT_QUIT;
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
			evt.type = GF_EVENT_SHOWHIDE;
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
			evt.type = GF_EVENT_REFRESH;
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
			evt.type = GF_EVENT_MOUSEMOVE;
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
		evt.type = GF_EVENT_MOUSEDOWN;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_LEFT;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_LBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVENT_MOUSEUP;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_LEFT;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVENT_MOUSEDOWN;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_RIGHT;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_RBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVENT_MOUSEUP;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_RIGHT;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVENT_MOUSEDOWN;
		evt.mouse.button = GF_MOUSE_MIDDLE;
		DD_GetCoordinates(lParam, &evt);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		if (!ctx->has_focus && (hWnd==ctx->os_hwnd)) {
			ctx->has_focus = 1;
			SetFocus(ctx->os_hwnd);
		}
		break;
	case WM_MBUTTONUP:
		release_mouse(ctx, hWnd, vout);
		evt.type = GF_EVENT_MOUSEUP;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_MIDDLE;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MOUSEWHEEL: 
		if (ctx->cur_hwnd==hWnd) {
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVENT_MOUSEWHEEL;
			DD_GetCoordinates(lParam, &evt);
			evt.mouse.wheel_pos = FLT2FIX( ((Float) (s16) HIWORD(wParam)) / WHEEL_DELTA );
			vout->on_event(vout->evt_cbk_hdl, &evt);
			mouse_start_timer(ctx, hWnd, vout);
		}
		return 1;

	/*there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYUP:
	case WM_KEYDOWN:
		w32_translate_key(wParam, lParam, &evt.key);
		evt.type = ((msg==WM_SYSKEYDOWN) || (msg==WM_KEYDOWN)) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;

	case WM_CHAR:
		evt.type = GF_EVENT_TEXTINPUT;
		evt.character.unicode_char = wParam;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	}
	return DefWindowProc (hWnd, msg, wParam, lParam);
}

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x80000 
#endif
#ifndef LWA_COLORKEY
#define LWA_COLORKEY   0x00000001
#endif
#ifndef LWA_ALPHA
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
	opt = gf_modules_get_option((GF_BaseInterface *)vout, "Compositor", "ColorKey");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)vout, "Compositor", "ColorKey", "FFFEFEFE");
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[DirectXOutput] Entering thread ID %d\n", gf_th_id() ));

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

	ctx->gl_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC OpenGL Offscreen", WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
	if (!ctx->gl_hwnd) {
		ctx->th_state = 2;
		return 1;
	}
	ShowWindow(ctx->gl_hwnd, SW_HIDE);

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
		/*override window proc*/
		if (!(flags & GF_TERM_NO_WINDOWPROC_OVERRIDE) ) {
			ctx->orig_wnd_proc = GetWindowLong(ctx->os_hwnd, GWL_WNDPROC);
			SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, (DWORD) DD_WindowProc);
		}
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
	PostMessage(ctx->gl_hwnd, WM_DESTROY, 0, 0);
	while (ctx->th_state!=2) gf_sleep(10);
	UnregisterClass("GPAC DirectDraw Output", GetModuleHandle("gm_dx_hw.dll"));
	gf_th_del(ctx->th);
	ctx->th = NULL;
	ctx->os_hwnd = NULL;
	ctx->fs_hwnd = NULL;
	ctx->gl_hwnd = NULL;
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
	case GF_EVENT_SET_CURSOR:
		DD_SetCursor(dr, evt->cursor.cursor_type);
		break;
	case GF_EVENT_SET_CAPTION:
		if (evt->caption.caption) SetWindowText(ctx->os_hwnd, evt->caption.caption);
		break;
	case GF_EVENT_MOVE:
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
	case GF_EVENT_SHOWHIDE:
		ShowWindow(ctx->os_hwnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	/*if scene resize resize window*/
	case GF_EVENT_SIZE:
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
	case GF_EVENT_VIDEO_SETUP:
		switch (evt->setup.opengl_mode) {
		case 0:
			ctx->output_3d_type = 0;
			return DD_SetBackBufferSize(dr, evt->size.width, evt->size.height);
		case 1:
			ctx->output_3d_type = 1;
			ctx->width = evt->setup.width;
			ctx->height = evt->setup.height;
			return DD_SetupOpenGL(dr);
		case 2:
			ctx->output_3d_type = 2;
			SetWindowPos(ctx->gl_hwnd, NULL, 0, 0, evt->size.width, evt->size.height, SWP_NOZORDER | SWP_NOMOVE);
			return DD_SetupOpenGL(dr);
		}
	}
	return GF_OK;
}
