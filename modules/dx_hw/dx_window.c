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


#ifdef _WIN32_WCE
static void DD_GetCoordinates(DWORD lParam, GF_Event *evt)
{
	evt->mouse.x = LOWORD(lParam);
	evt->mouse.y = HIWORD(lParam);
}
#else

static void DD_GetCoordinates(LPARAM lParam, GF_Event *evt)
{
	POINTS pt = MAKEPOINTS(lParam);
	evt->mouse.x = pt.x;
	evt->mouse.y = pt.y;
}
#endif

static void w32_translate_key(WPARAM wParam, LPARAM lParam, GF_EventKey *evt)
{
	evt->flags = 0;
	evt->hw_code = (u32) wParam;
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

#if !defined(_WIN32_WCE) && !defined(__GNUC__)
	case VK_OEM_PLUS:
		evt->key_code = GF_KEY_PLUS;
		break;
	case VK_OEM_MINUS:
		evt->key_code = GF_KEY_PLUS;
		break;
#endif

#ifndef _WIN32_WCE
	case VK_NONCONVERT:
		evt->key_code = GF_KEY_NONCONVERT;
		break;
	case VK_ACCEPT:
		evt->key_code = GF_KEY_ACCEPT;
		break;
	case VK_MODECHANGE:
		evt->key_code = GF_KEY_MODECHANGE;
		break;
#endif

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
	case VK_F6:
		evt->key_code = GF_KEY_F6;
		break;
	case VK_F7:
		evt->key_code = GF_KEY_F7;
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
		if ((wParam>=0x30) && (wParam<=0x39))  evt->key_code = GF_KEY_0 + (u32) (wParam-0x30);
		else if ((wParam>=0x41) && (wParam<=0x5A))  evt->key_code = GF_KEY_A + (u32) (wParam-0x41);
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

LRESULT APIENTRY DD_WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Bool ret = 1;
	GF_Event evt;
	DDContext *ctx;
#ifdef _WIN64
	GF_VideoOutput *vout = (GF_VideoOutput *) GetWindowLongPtr(hWnd, GWLP_USERDATA);
#else
	GF_VideoOutput *vout = (GF_VideoOutput *) GetWindowLong(hWnd, GWL_USERDATA);
#endif
	if (!vout) return DefWindowProc(hWnd, msg, wParam, lParam);
	ctx = (DDContext *)vout->opaque;

	switch (msg) {
	case WM_SIZE:
		/*always notify GPAC since we're not sure the owner of the window is listening to these events*/
		if (wParam==SIZE_MINIMIZED) {
			evt.type = GF_EVENT_SHOWHIDE;
			evt.show.show_type = 0;
			ctx->hidden = 1;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		} else {
			if (ctx->hidden && wParam==SIZE_RESTORED) {
				ctx->hidden = 0;
				evt.type = GF_EVENT_SHOWHIDE;
				evt.show.show_type = 1;
				vout->on_event(vout->evt_cbk_hdl, &evt);
			}
			evt.type = GF_EVENT_SIZE;
			evt.size.width = LOWORD(lParam);
			evt.size.height = HIWORD(lParam);
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		break;
	case WM_MOVE:
		evt.type = GF_EVENT_MOVE;
		evt.move.x = LOWORD(lParam);
		evt.move.y = HIWORD(lParam);
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_CLOSE:
		if (hWnd==ctx->os_hwnd) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		break;
	case WM_DESTROY:
		if (ctx->owns_hwnd || (hWnd==ctx->fs_hwnd)) {
			PostQuitMessage (0);
		} else if (ctx->orig_wnd_proc) {
			/*restore window proc*/
#ifdef _WIN64
			SetWindowLongPtr(ctx->os_hwnd, GWLP_WNDPROC, ctx->orig_wnd_proc);
#else
			SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, ctx->orig_wnd_proc);
#endif
			ctx->orig_wnd_proc = 0L;
		}
		break;
	case WM_DISPLAYCHANGE:
		ctx->dd_lost = 1;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.back_buffer = 1;
		vout->on_event(vout->evt_cbk_hdl, &evt);
		break;

	case WM_ACTIVATE:
		if (!ctx->on_secondary_screen && ctx->fullscreen && (LOWORD(wParam)==WA_INACTIVE)
		        && (hWnd==ctx->fs_hwnd)
#ifndef GPAC_DISABLE_3D
		        && (ctx->output_3d_type!=2)
#endif
		   ) {
			evt.type = GF_EVENT_SHOWHIDE;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		/*fallthrough*/
#ifndef _WIN32_WCE
	case WM_ACTIVATEAPP:
#endif
		if (hWnd==ctx->os_hwnd) {
			ctx->has_focus = 1;
			SetFocus(hWnd);
		}
		break;
	case WM_KILLFOCUS:
		if (hWnd==ctx->os_hwnd) ctx->has_focus = 0;
		break;
	case WM_SETFOCUS:
		if (hWnd==ctx->os_hwnd) ctx->has_focus = 1;
		break;
	case WM_IME_SETCONTEXT:
		if ((hWnd==ctx->os_hwnd) && (wParam!=0) && !ctx->fullscreen) SetFocus(hWnd);
		break;

	case WM_ERASEBKGND:
		/*the erasebkgnd message causes flickering when resizing the window, we discard it*/
		if (ctx->is_setup) {
			break;
		}
	case WM_PAINT:
		if (ctx->cur_hwnd==hWnd) {
			evt.type = GF_EVENT_REFRESH;
			vout->on_event(vout->evt_cbk_hdl, &evt);
		}
		/*this avoids 100% cpu usage in Firefox*/
		return DefWindowProc (hWnd, msg, wParam, lParam);

	case WM_SETCURSOR:
		if (ctx->cur_hwnd==hWnd) DD_SetCursor(vout, ctx->cursor_type);
		break;

	case WM_DROPFILES:
	{
		char szFile[GF_MAX_PATH];
		GF_Event evt;
		u32 i;

		HDROP hDrop = (HDROP) wParam;
		evt.type = GF_EVENT_OPENFILE;
		evt.open_file.nb_files = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		evt.open_file.files = gf_malloc(sizeof(char *)*evt.open_file.nb_files);
		for (i=0; i<evt.open_file.nb_files; i++) {
			u32 res = DragQueryFile(hDrop, i, szFile, GF_MAX_PATH);
			evt.open_file.files[i] = res ? gf_strdup(szFile) : NULL;
		}
		DragFinish(hDrop);
		/*send message*/
		vout->on_event(vout->evt_cbk_hdl, &evt);
		for (i=0; i<evt.open_file.nb_files; i++) {
			if (evt.open_file.files[i]) gf_free(evt.open_file.files[i]);
		}
		gf_free(evt.open_file.files);
	}
	break;

	case WM_MOUSEMOVE:
		if (ctx->cur_hwnd!=hWnd) break;

		if (ctx->last_mouse_pos != lParam) {
			ctx->last_mouse_pos = lParam;
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVENT_MOUSEMOVE;
			DD_GetCoordinates(lParam, &evt);
			ret = vout->on_event(vout->evt_cbk_hdl, &evt);
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
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
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
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVENT_MOUSEDOWN;
		DD_GetCoordinates(lParam, &evt);
		evt.mouse.button = GF_MOUSE_RIGHT;
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
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
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		grab_mouse(ctx, vout);
		evt.type = GF_EVENT_MOUSEDOWN;
		evt.mouse.button = GF_MOUSE_MIDDLE;
		DD_GetCoordinates(lParam, &evt);
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
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
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
		mouse_start_timer(ctx, hWnd, vout);
		break;
	case WM_MOUSEWHEEL:
		if (ctx->cur_hwnd==hWnd) {
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVENT_MOUSEWHEEL;
			DD_GetCoordinates(lParam, &evt);
			evt.mouse.wheel_pos = FLT2FIX( ((Float) (s16) HIWORD(wParam)) / WHEEL_DELTA );
			ret = vout->on_event(vout->evt_cbk_hdl, &evt);
			mouse_start_timer(ctx, hWnd, vout);
		}
		break;
	case WM_HSCROLL:
		if (ctx->cur_hwnd==hWnd) {
			DD_SetCursor(vout, (ctx->cursor_type==GF_CURSOR_HIDE) ? ctx->cursor_type_backup : ctx->cursor_type);
			evt.type = GF_EVENT_MOUSEWHEEL;
			evt.mouse.button = 1;
			switch (LOWORD(wParam)) {
			case SB_LEFT:
				evt.mouse.wheel_pos = -1;
				break;
			case SB_RIGHT:
				evt.mouse.wheel_pos = +1;
				break;
			case SB_LINELEFT:
				evt.mouse.wheel_pos = -5;
				break;
			case SB_LINERIGHT:
				evt.mouse.wheel_pos = +5;
				break;
			case SB_PAGELEFT:
				evt.mouse.wheel_pos = -10;
				break;
			case SB_PAGERIGHT:
				evt.mouse.wheel_pos = +10;
				break;
			default:
				break;
			}
			ret = vout->on_event(vout->evt_cbk_hdl, &evt);
			mouse_start_timer(ctx, hWnd, vout);
		}
		break;

	/*there's a bug on alt state (we miss one event)*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYUP:
	case WM_KEYDOWN:
		w32_translate_key(wParam, lParam, &evt.key);
		evt.type = ((msg==WM_SYSKEYDOWN) || (msg==WM_KEYDOWN)) ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
		if (evt.key.key_code==GF_KEY_ALT) ctx->alt_down = (evt.type==GF_EVENT_KEYDOWN) ? 1 : 0;
		if (evt.key.key_code==GF_KEY_CONTROL) ctx->ctrl_down = (evt.type==GF_EVENT_KEYDOWN) ? 1 : 0;
		if ((ctx->os_hwnd==ctx->fs_hwnd) && ctx->alt_down && (evt.key.key_code==GF_KEY_F4)) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_QUIT;
		}
		else if (ctx->ctrl_down && (evt.type==GF_EVENT_KEYUP) && (evt.key.key_code==GF_KEY_V)) {
			HGLOBAL hglbCopy;
			if (!IsClipboardFormatAvailable(CF_TEXT)) break;
			if (!OpenClipboard(ctx->cur_hwnd)) break;

			hglbCopy = GetClipboardData(CF_TEXT);
			if (hglbCopy) {
				LPTSTR lptstrCopy = (char *) GlobalLock(hglbCopy);
				evt.type = GF_EVENT_PASTE_TEXT;
				evt.message.message = lptstrCopy;
				ret = vout->on_event(vout->evt_cbk_hdl, &evt);
				GlobalUnlock(hglbCopy);
			}
			CloseClipboard();
			break;
		}
		else if (ctx->ctrl_down && (evt.type==GF_EVENT_KEYUP) && (evt.key.key_code==GF_KEY_C)) {
			evt.type = GF_EVENT_COPY_TEXT;
			if ((vout->on_event(vout->evt_cbk_hdl, &evt)==GF_TRUE) && evt.message.message) {
				size_t len;
				HGLOBAL hglbCopy;
				LPTSTR lptstrCopy;
				if (!IsClipboardFormatAvailable(CF_TEXT)) break;
				if (!OpenClipboard(ctx->cur_hwnd)) break;
				EmptyClipboard();

				len = strlen(evt.message.message);
				if (!len) break;

				hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(char));
				lptstrCopy = (char *) GlobalLock(hglbCopy);
				memcpy(lptstrCopy, evt.message.message, len * sizeof(char));
				lptstrCopy[len] = 0;
				GlobalUnlock(hglbCopy);
				SetClipboardData(CF_TEXT, hglbCopy);
				CloseClipboard();
				break;
			}
		}
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);

		if ( !ctx->ctrl_down && !ctx->alt_down
		        && evt.key.key_code != GF_KEY_CONTROL && evt.key.key_code != GF_KEY_ALT )
			ret = 1;
		break;

	case WM_UNICHAR:
	case WM_CHAR:
		/*no reason to filter out things*/
//		if (wParam>=32)
	{
		evt.type = GF_EVENT_TEXTINPUT;
		evt.character.unicode_char = (u32) wParam;
		ret = vout->on_event(vout->evt_cbk_hdl, &evt);
	}
	break;
	/*
		case WM_CANCELMODE:
		case WM_CAPTURECHANGED:
		case WM_NCHITTEST:
			return 0;
	*/
	default:
		return DefWindowProc (hWnd, msg, wParam, lParam);
	}

	if (!ret &&(ctx->os_hwnd==hWnd) && ctx->orig_wnd_proc)
		return CallWindowProc((WNDPROC) ctx->orig_wnd_proc, hWnd, msg, wParam, lParam);
	return 0;
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
#ifdef _WIN32_WCE
	GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[DX Out] Windowloess mode not supported on Window CE\n"));
	return;
#else
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
#endif
}


Bool DD_InitWindows(GF_VideoOutput *vout, DDContext *ctx)
{
	u32 flags;
	Bool use_fs_wnd = 1;
#ifndef _WIN32_WCE
	RECT rc;
#endif
	WNDCLASS wc;
	HINSTANCE hInst;

#ifndef _WIN32_WCE
	hInst = GetModuleHandle("gm_dx_hw.dll");
#else
	hInst = GetModuleHandle(_T("gm_dx_hw.dll"));
#endif

	memset(&wc, 0, sizeof(WNDCLASS));
#ifndef _WIN32_WCE
	wc.style = CS_BYTEALIGNWINDOW;
	wc.hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_OSMO_ICON) );
	wc.lpszClassName = "GPAC DirectDraw Output";
#else
	wc.lpszClassName = _T("GPAC DirectDraw Output");
#endif
	wc.hInstance = hInst;
	wc.lpfnWndProc = DD_WindowProc;
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject (BLACK_BRUSH);
	RegisterClass (&wc);

	flags = ctx->switch_res;
	ctx->switch_res = 0;
	ctx->force_alpha = (flags & GF_TERM_WINDOW_TRANSPARENT) ? 1 : 0;

	if (!ctx->os_hwnd) {
		u32 styles;
		if (flags & GF_TERM_WINDOWLESS) ctx->windowless = 1;


#ifdef _WIN32_WCE
		ctx->os_hwnd = CreateWindow(_T("GPAC DirectDraw Output"), _T("GPAC DirectDraw Output"), WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
#else

		if (flags & GF_TERM_WINDOW_NO_DECORATION) {
			styles = WS_POPUP | WS_THICKFRAME;
		} else if (ctx->windowless) {
			styles = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP;
		} else {
			styles = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_OVERLAPPEDWINDOW;
		}
		use_fs_wnd=0;
		ctx->os_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC DirectDraw Output", styles, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
		ctx->backup_styles = styles;
#endif
		if (ctx->os_hwnd == NULL) {
			return 0;
		}
		DragAcceptFiles(ctx->os_hwnd, TRUE);

		if (flags & GF_TERM_INIT_HIDE) {
			ShowWindow(ctx->os_hwnd, SW_HIDE);
		} else {
			SetForegroundWindow(ctx->os_hwnd);
			ShowWindow(ctx->os_hwnd, SW_SHOW);
		}

		/*get border & title bar sizes*/
#ifdef _WIN32_WCE
		ctx->off_w = 0;
		ctx->off_h = 0;
#else
		rc.left = rc.top = 0;
		rc.right = rc.bottom = 100;
		AdjustWindowRect(&rc, styles, 0);
		ctx->off_w = rc.right - rc.left - 100;
		ctx->off_h = rc.bottom - rc.top - 100;
#endif
		ctx->owns_hwnd = 1;

		if (ctx->windowless) SetWindowless(vout, ctx->os_hwnd);
	}

	if (use_fs_wnd) {
#ifdef _WIN32_WCE
		ctx->fs_hwnd = CreateWindow(_T("GPAC DirectDraw Output"), _T("GPAC DirectDraw FS Output"), WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
#else
		ctx->fs_hwnd = CreateWindow("GPAC DirectDraw Output", "GPAC DirectDraw FS Output", WS_POPUP, 0, 0, 120, 100, NULL, NULL, hInst, NULL);
#endif
		if (!ctx->fs_hwnd) {
			return 0;
		}
		ShowWindow(ctx->fs_hwnd, SW_HIDE);
#ifdef _WIN64
		SetWindowLongPtr(ctx->fs_hwnd, GWLP_USERDATA, (LONG) vout);
#else
		SetWindowLong(ctx->fs_hwnd, GWL_USERDATA, (LONG) vout);
#endif
	} else {
		ctx->fs_hwnd = ctx->os_hwnd;
	}

	/*if visible set focus*/
	if (!ctx->switch_res) SetFocus(ctx->os_hwnd);

	ctx->switch_res = 0;
#ifdef _WIN64
	SetWindowLongPtr(ctx->os_hwnd, GWLP_USERDATA, (LONG) vout);
#else
	SetWindowLong(ctx->os_hwnd, GWL_USERDATA, (LONG) vout);
#endif

	/*load cursors*/
	ctx->curs_normal = LoadCursor(NULL, IDC_ARROW);
	ctx->curs_hand = LoadCursor(hInst, MAKEINTRESOURCE(IDC_HAND_PTR));
	ctx->curs_collide = LoadCursor(hInst, MAKEINTRESOURCE(IDC_COLLIDE));
	ctx->cursor_type = GF_CURSOR_NORMAL;
	return 1;
}

u32 DD_WindowThread(void *par)
{
	MSG msg;

	GF_VideoOutput *vout = par;
	DDContext *ctx = (DDContext *)vout->opaque;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[DirectXOutput] Entering thread ID %d\n", gf_th_id() ));

	if (DD_InitWindows(vout, ctx)) {
		s32 msg_ok=1;
		ctx->th_state = 1;
		while (msg_ok) {
			msg_ok = GetMessage (&(msg), NULL, 0, 0);
			if (msg_ok == -1) msg_ok = 0;
			if (msg.message == WM_DESTROY) PostQuitMessage(0);	//WM_DESTROY: exit
			TranslateMessage (&(msg));
			DispatchMessage (&(msg));
		}
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
#ifdef _WIN64
			ctx->orig_wnd_proc = GetWindowLongPtr(ctx->os_hwnd, GWLP_WNDPROC);
			SetWindowLongPtr(ctx->os_hwnd, GWLP_WNDPROC, (DWORD) DD_WindowProc);
#else
			ctx->orig_wnd_proc = GetWindowLong(ctx->os_hwnd, GWL_WNDPROC);
			SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, (DWORD) DD_WindowProc);
#endif
		}
		ctx->parent_wnd = GetParent(ctx->os_hwnd);
	}
	ctx->switch_res = flags;

	if (flags & GF_TERM_WINDOW_NO_THREAD) {
		DD_InitWindows(dr, ctx);
	} else {
		/*create event thread*/
		ctx->th = gf_th_new("DirectX Video");
		gf_th_run(ctx->th, DD_WindowThread, dr);
		while (!ctx->th_state)
			gf_sleep(1);
	}
	if (!the_video_output) the_video_output = dr;
}

static void dd_closewindow(HWND hWnd)
{
	PostMessage(hWnd, WM_DESTROY, 0, 0);
}

void DD_ShutdownWindow(GF_VideoOutput *dr)
{
	DDContext *ctx = (DDContext *)dr->opaque;

	if (ctx->owns_hwnd) {
		dd_closewindow(ctx->os_hwnd);
	} else if (ctx->orig_wnd_proc) {
		/*restore window proc*/
#ifdef _WIN64
		SetWindowLongPtr(ctx->os_hwnd, GWLP_WNDPROC, ctx->orig_wnd_proc);
#else
		SetWindowLong(ctx->os_hwnd, GWL_WNDPROC, ctx->orig_wnd_proc);
#endif
		ctx->orig_wnd_proc = 0L;
	}

	if (ctx->fs_hwnd != ctx->os_hwnd) {
		dd_closewindow(ctx->fs_hwnd);
#ifdef _WIN64
		SetWindowLongPtr(ctx->fs_hwnd, GWLP_USERDATA, (LONG) NULL);
		SetWindowLongPtr(ctx->fs_hwnd, GWLP_WNDPROC, (DWORD) DefWindowProc);
#else
		SetWindowLong(ctx->fs_hwnd, GWL_USERDATA, (LONG) NULL);
		SetWindowLong(ctx->fs_hwnd, GWL_WNDPROC, (DWORD) DefWindowProc);
#endif
	}

#ifndef GPAC_DISABLE_3D
	if (ctx->gl_hwnd) dd_closewindow(ctx->gl_hwnd);
#endif

	if (ctx->th) {
		while (ctx->th_state!=2)
			gf_sleep(10);

		gf_th_del(ctx->th);
		ctx->th = NULL;
	}

	/*special care for Firefox: the windows created by our NP plugin may still be called
	after the shutdown of the plugin !!*/
#ifdef _WIN64
	SetWindowLongPtr(ctx->os_hwnd, GWLP_USERDATA, (LONG) NULL);
#else
	SetWindowLong(ctx->os_hwnd, GWL_USERDATA, (LONG) NULL);
#endif

#ifdef _WIN32_WCE
	UnregisterClass(_T("GPAC DirectDraw Output"), GetModuleHandle(_T("gm_dx_hw.dll")) );
#else
	UnregisterClass("GPAC DirectDraw Output", GetModuleHandle("gm_dx_hw.dll"));
#endif
	ctx->os_hwnd = NULL;
	ctx->fs_hwnd = NULL;
#ifndef GPAC_DISABLE_3D
	ctx->gl_hwnd = NULL;
#endif
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

static u32 get_sys_col(int idx)
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


/*Note: all calls to SetWindowPos are made in a non-blocking way using SWP_ASYNCWINDOWPOS. This avoids deadlocks
when the compositor request a size change and the DX window thread has grabbed the main compositor mutex.
This typically happens when switching playlist items as fast as possible*/
GF_Err DD_ProcessEvent(GF_VideoOutput*dr, GF_Event *evt)
{
	DDContext *ctx = (DDContext *)dr->opaque;

	if (!evt) {
		if (!ctx->th) {
			MSG msg;
			while (PeekMessage (&(msg), NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage (&(msg));
				DispatchMessage (&(msg));
			}
		}
		return GF_OK;
	}

	switch (evt->type) {
	case GF_EVENT_SET_CURSOR:
		DD_SetCursor(dr, evt->cursor.cursor_type);
		break;
	case GF_EVENT_SET_CAPTION:
#ifndef _WIN32_WCE
		if (evt->caption.caption) SetWindowText(ctx->os_hwnd, evt->caption.caption);
#endif
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

			SetWindowPos(ctx->os_hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
		}
		else if (evt->move.relative) {
			POINT pt;
			pt.x = pt.y = 0;
			MapWindowPoints(ctx->os_hwnd, NULL, &pt, 1);
			SetWindowPos(ctx->os_hwnd, NULL, evt->move.x + pt.x, evt->move.y + pt.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
		} else {
			SetWindowPos(ctx->os_hwnd, NULL, evt->move.x, evt->move.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
		}
		break;
	case GF_EVENT_SHOWHIDE:
		ShowWindow(ctx->os_hwnd, evt->show.show_type ? SW_SHOW : SW_HIDE);
		break;
	/*if scene resize resize window*/
	case GF_EVENT_SIZE:
		if (ctx->owns_hwnd) {
			if (ctx->fullscreen) {
				ctx->store_width = evt->size.width;
				ctx->store_height = evt->size.height;
			} else {
				if (ctx->windowless)
					SetWindowPos(ctx->os_hwnd, NULL, 0, 0, evt->size.width, evt->size.height, SWP_NOZORDER | SWP_NOMOVE | SWP_ASYNCWINDOWPOS);
				else
					SetWindowPos(ctx->os_hwnd, NULL, 0, 0, evt->size.width + ctx->off_w, evt->size.height + ctx->off_h, SWP_NOZORDER | SWP_NOMOVE | SWP_ASYNCWINDOWPOS);
			}
		}
		break;
	/*HW setup*/
	case GF_EVENT_VIDEO_SETUP:
		if (ctx->dd_lost) {
			ctx->dd_lost = 0;
			DestroyObjects(ctx);
		}
		ctx->is_setup=1;
		switch (evt->setup.opengl_mode) {
		case 0:
#ifndef GPAC_DISABLE_3D
			ctx->output_3d_type = 0;
#endif
			return DD_SetBackBufferSize(dr, evt->setup.width, evt->setup.height, evt->setup.system_memory);
#ifndef GPAC_DISABLE_3D
		case 1:
			ctx->output_3d_type = 1;
			ctx->width = evt->setup.width;
			ctx->height = evt->setup.height;
			ctx->gl_double_buffer = evt->setup.back_buffer;
			return DD_SetupOpenGL(dr, 0, 0);
		case 2:
			ctx->output_3d_type = 2;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DX Out] Attempting to resize Offscreen OpenGL window to %d x %d\n", evt->size.width, evt->size.height));
			if (ctx->gl_hwnd)
				SetWindowPos(ctx->gl_hwnd, NULL, 0, 0, evt->size.width, evt->size.height, SWP_NOZORDER | SWP_NOMOVE | SWP_ASYNCWINDOWPOS);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[DX Out] Resizing Offscreen OpenGL window to %d x %d\n", evt->size.width, evt->size.height));
			SetForegroundWindow(ctx->cur_hwnd);
			ctx->gl_double_buffer = evt->setup.back_buffer;
			return DD_SetupOpenGL(dr, evt->size.width, evt->size.height);
#else
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
