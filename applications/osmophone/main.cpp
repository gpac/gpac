/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line client
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

/*includes both terminal and od browser*/
#include <gpac/terminal.h>
#include <gpac/options.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <aygshell.h>
#include "resource.h"

#include <gx.h>

#define WM_LOADTERM	WM_USER + 1
#define STATE_TIMER_ID		20
#define STATE_TIMER_DUR		1000


Bool gf_file_dialog(HINSTANCE inst, HWND parent, char *url, const char *ext_list);
void set_backlight_state(Bool disable);
void refresh_recent_files();

static HWND g_hwnd = NULL;
static HWND g_hwnd_disp = NULL;
static HWND g_hwnd_mb = NULL;
static HWND g_hwnd_status = NULL;
static HINSTANCE g_hinst = NULL;

static Bool is_connected = 0;

static u32 Duration;
static Bool CanSeek = 0;
static u32 Volume=100;
static char the_url[GF_MAX_PATH];
static Bool NavigateTo = 0;
static char the_next_url[GF_MAX_PATH];
static GF_Terminal *term;
static GF_User user;
static u32 disp_w = 0;
static u32 disp_h = 0;
static u32 screen_w = 0;
static u32 screen_h = 0;
static u32 menu_h = 0;
static u32 caption_h = 0;
static Bool backlight_off = 0;
static u32 prev_batt_bl, prev_ac_bl;
static Bool show_status = 1;
static Bool reset_status = 1;
static u32 last_state_time = 0;
static Bool loop = 0;
static Bool full_screen = 0;

void set_status(char *state)
{
	if (show_status && g_hwnd_status) {
		TCHAR wstate[1024];
		CE_CharToWide(state, wstate);
		SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) wstate);
		last_state_time = GetTickCount();
	}
}

void update_state_info()
{
	TCHAR wstate[1024];
	Double FPS;
	u32 time, m, s;
	if (!show_status) return;
	if (last_state_time) {
		if (GetTickCount() > last_state_time + 1000) {
			last_state_time = 0;
			reset_status = 1;
		}
		else return;
	}
	if (!term) return;
	if (!is_connected) {
		if (reset_status) {
			SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) TEXT("Ready") );
			reset_status = 0;
		}
		return;
	}

	FPS = gf_term_get_framerate(term, 0);
	time = gf_term_get_time_in_ms(term) / 1000;
	m = time/60;
	s = time - m*60;
	wsprintf(wstate, TEXT("Time: %02d:%02d - FPS %02.2f"), m, s, FPS);
	SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) wstate);
}

Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	switch (evt->type) {
	case GF_EVT_DURATION:
		Duration = (u32) (evt->duration.duration*1000);
		CanSeek = evt->duration.can_seek;
		break;
	case GF_EVT_MESSAGE:
	{
		const char *servName;
		if (!evt->message.service || !strcmp(evt->message.service, the_url)) {
			servName = "main service";
		} else {
			servName = evt->message.service;
		}
		if (!evt->message.message) return 0;
		set_status((char *) evt->message.message);
	}
		break;
	case GF_EVT_PROGRESS:
	{
		char *szTitle = "";
		if (evt->progress.progress_type==0) szTitle = "Buffer ";
		else if (evt->progress.progress_type==1) szTitle = "Download ";
		else if (evt->progress.progress_type==2) szTitle = "Import ";
		gf_cbk_on_progress(szTitle, evt->progress.done, evt->progress.total);
	}
		break;
	
	case GF_EVT_SIZE:
		MoveWindow(g_hwnd, 0, caption_h, disp_w, disp_h, FALSE);
		if (term) gf_term_set_size(term, disp_w, disp_h);
		break;
	case GF_EVT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			if (!backlight_off) set_backlight_state(1);
			refresh_recent_files();

		} else {
			is_connected = 0;
			Duration = 0;
		}
		break;
	case GF_EVT_QUIT:
		break;
	}
	return 0;
}


Bool LoadTerminal()
{
	/*by default use current dir*/
	strcpy(the_url, ".");

	g_hwnd_disp = CreateWindow(TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE , 0, 0, disp_w, disp_h, g_hwnd, NULL, g_hinst, NULL);

	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	user.os_window_handler = g_hwnd_disp;

	term = gf_term_new(&user);
	if (!term) {
		gf_modules_del(user.modules);
		gf_cfg_del(user.config);
		memset(&user, 0, sizeof(GF_User));
		return 0;
	}
	return 1;
}

void do_layout()
{
	u32 w, h;
	if (full_screen) {
		w = screen_w;
		h = screen_h;
		::ShowWindow(g_hwnd_status, SW_HIDE);
		::ShowWindow(g_hwnd_mb, SW_HIDE);
		::MoveWindow(g_hwnd, 0, 0, screen_w, screen_h, 1);
		::MoveWindow(g_hwnd_disp, 0, 0, screen_w, screen_h, 1);
	} else {
		::ShowWindow(g_hwnd_mb, SW_SHOW);
		if (show_status) {
			::MoveWindow(g_hwnd, 0, caption_h, disp_w, disp_h, 1);
			::ShowWindow(g_hwnd_status, SW_SHOW);
			::MoveWindow(g_hwnd_status, 0, 0, disp_w, caption_h-2, 1);
			::MoveWindow(g_hwnd_disp, 0, caption_h, disp_w, disp_h - caption_h, 1);
			w = disp_w;
			h = disp_h - caption_h;
		} else {
			::ShowWindow(g_hwnd_status, SW_HIDE);
			::MoveWindow(g_hwnd, 0, caption_h, disp_w, disp_h, 1);
			::MoveWindow(g_hwnd_disp, 0, 0, disp_w, disp_h, 1);
			w = disp_w;
			h = disp_h;
		}
	}
	if (term) gf_term_set_size(term, w, h);
}

void HandleInputMessages(UINT msg, WPARAM wParam, LPARAM lParam)
{
	
}


void set_backlight_state(Bool disable) 
{
	HKEY hKey = 0;
	DWORD dwSize;
	DWORD dwValue;
	HANDLE hBL;


	if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, 0, &hKey ) != ERROR_SUCCESS) return;

	if (disable) {
		dwSize = 4;
		RegQueryValueEx(hKey, _T("BatteryTimeout"), NULL, NULL,(unsigned char*) &prev_batt_bl, &dwSize);
		dwSize = 4;
		RegQueryValueEx(hKey, _T("ACTimeout"), NULL, NULL, (unsigned char*) &prev_ac_bl,&dwSize);
		dwSize = 4;
		dwValue = 0xefff ;
		RegSetValueEx(hKey, _T("BatteryTimeout"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		dwSize = 4;
		dwValue = 0xefff ;
		RegSetValueEx( hKey, _T("ACTimeout"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		backlight_off = 1;
	} else {
		if (prev_batt_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("BatteryTimeout"), NULL, REG_DWORD, (unsigned char *)&prev_batt_bl, dwSize);
		}
		if (prev_ac_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("ACTimeout"), NULL, REG_DWORD,(unsigned char *)&prev_ac_bl, dwSize);
		}
		backlight_off = 0;
	}
	RegCloseKey(hKey);
	hBL = CreateEvent(NULL, FALSE, FALSE, _T("BackLightChangeEvent"));
	if (hBL) {
		SetEvent(hBL);
		CloseHandle(hBL);
	}
}

static Bool do_resume = 0;
static Bool prev_backlight_state;
void freeze_display(Bool do_freeze)
{
	if (do_freeze) {
		prev_backlight_state = backlight_off;
		do_resume = 0;
		if (0 && is_connected && gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
			do_resume= 1;
			gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
		/*freeze display*/
		gf_term_set_option(term, GF_OPT_FREEZE_DISPLAY, 1);
		set_backlight_state(0);
		gf_sleep(100);
	} else {
		if (prev_backlight_state) set_backlight_state(1);
		gf_term_set_option(term, GF_OPT_FREEZE_DISPLAY, 0);
		if (do_resume) {
			gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
			set_backlight_state(1);
		}	
	}
}

void do_open_file()
{
	gf_cfg_set_key(user.config, "RecentFiles", the_url, NULL);
	gf_cfg_insert_key(user.config, "RecentFiles", the_url, "");
	u32 count = gf_cfg_get_key_count(user.config, "RecentFiles");
	if (count > 10) gf_cfg_set_key(user.config, "RecentFiles", gf_cfg_get_key_name(user.config, "RecentFiles", count-1), NULL);

	gf_term_connect(term, the_url);
}

void refresh_recent_files()
{
	u32 count = gf_cfg_get_key_count(user.config, "RecentFiles");

    HMENU hMenu = (HMENU)SendMessage(g_hwnd_mb, SHCMBM_GETSUBMENU, 0, ID_MENU_FILE);
	/*pos is hardcoded*/
	hMenu = GetSubMenu(hMenu, 2);

	while (RemoveMenu(hMenu, 0, MF_BYPOSITION)) {}

	for (u32 i=0; i<count; i++) {
		TCHAR txt[100];
		char *name;
		const char *sOpt = gf_cfg_get_key_name(user.config, "RecentFiles", i);
		name = strrchr(sOpt, '\\');
		if (!name) name = strrchr(sOpt, '/');
		if (!name) name = (char *) sOpt;
		else name += 1;

		CE_CharToWide(name, txt);
		AppendMenu(hMenu, MF_STRING, IDM_OPEN_FILE1+i, txt);
	}
}

void open_file(HWND hwnd)
{
	Bool res;
	char ext_list[4096];
	strcpy(ext_list, "");

	freeze_display(1);

	u32 count = gf_cfg_get_key_count(user.config, "MimeTypes");
	for (u32 i=0; i<count; i++) {
		char szKeyList[1000], *sKey;
		const char *sMime = gf_cfg_get_key_name(user.config, "MimeTypes", i);
		const char *opt = gf_cfg_get_key(user.config, "MimeTypes", sMime);
		strcpy(szKeyList, opt+1);
		sKey = strrchr(szKeyList, '\"');
		if (!sKey) continue;
		sKey[0] = 0;
		strcat(ext_list, szKeyList);
		strcat(ext_list, " ");
	}
	res = gf_file_dialog(g_hinst, hwnd, the_url, ext_list);

	freeze_display(0);

	/*let's go*/
	if (res) do_open_file();
}

void load_recent_file(u32 idx)
{
	const char *sOpt = gf_cfg_get_key_name(user.config, "RecentFiles", idx);
	if (sOpt) {
		strcpy(the_url, sOpt);
		do_open_file();
	}
}

BOOL CALLBACK AboutDialogProc(const HWND hWnd, const UINT Msg, const WPARAM wParam, const LPARAM lParam) 
{
	BOOL ret = TRUE;
    switch (Msg) {
    case WM_INITDIALOG:
	{
	    TCHAR           psz[80];
		ZeroMemory(psz, sizeof(psz));
		SHINITDLGINFO sid;
		ZeroMemory(&sid, sizeof(sid));
		sid.dwMask  = SHIDIM_FLAGS;
		sid.dwFlags = SHIDIF_SIZEDLGFULLSCREEN;
		sid.hDlg    = hWnd;
		SHInitDialog(&sid);

		SHMENUBARINFO mbi;
		memset(&mbi, 0, sizeof(SHMENUBARINFO));
		mbi.cbSize = sizeof(SHMENUBARINFO);
		mbi.hwndParent = hWnd;
		mbi.nToolBarId = IDR_ABOUT_MENU;
		mbi.hInstRes = g_hinst;
		mbi.nBmpId = 0;
		mbi.cBmpImages = 0;	
		SHCreateMenuBar(&mbi);
	}
        break;
    case WM_COMMAND:
    case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;
	default:
		ret = FALSE;
	}
	return ret;
}
void view_about(HWND hwnd)
{
	freeze_display(1);
	::ShowWindow(g_hwnd_mb, SW_HIDE);
	int iResult = DialogBox(g_hinst, MAKEINTRESOURCE(IDD_APPABOUT), hwnd,(DLGPROC)AboutDialogProc);
	::ShowWindow(g_hwnd_mb, SW_SHOW);
	freeze_display(0);
}

BOOL HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) {
	case IDM_FILE_OPEN:
		open_file(hwnd);
		break;
	case IDM_VIEW_ABOUT:
		view_about(hwnd);
		break;
	case IDM_VIEW_FS:
		full_screen = !full_screen;
		do_layout();
		break;
	case IDM_VIEW_STATUS:
		show_status = !show_status;
		do_layout();
		break;

	case IDM_OPEN_FILE1: 
	case IDM_OPEN_FILE2: 
	case IDM_OPEN_FILE3: 
	case IDM_OPEN_FILE4: 
	case IDM_OPEN_FILE5: 
	case IDM_OPEN_FILE6: 
	case IDM_OPEN_FILE7: 
	case IDM_OPEN_FILE8: 
	case IDM_OPEN_FILE9: 
	case IDM_OPEN_FILE10: 
		load_recent_file(LOWORD(wParam) - IDM_OPEN_FILE1); 
		break;

	case IDM_ITEM_QUIT:
		DestroyWindow(hwnd);
		return FALSE;
	}
	return TRUE;
}

static BOOL OnMenuPopup(const HWND hWnd, const WPARAM wParam)
{
	if (show_status) {
		CheckMenuItem((HMENU)wParam, IDM_VIEW_STATUS, MF_BYCOMMAND|MF_CHECKED);
	} else {
		CheckMenuItem((HMENU)wParam, IDM_VIEW_STATUS, MF_BYCOMMAND|MF_UNCHECKED);
	}
    return TRUE;
}


BOOL CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HandleInputMessages(msg, wParam, lParam);

	switch (msg) {
	case WM_CREATE:
	{
		SHMENUBARINFO mbi;
		memset(&mbi, 0, sizeof(SHMENUBARINFO));
		mbi.cbSize = sizeof(SHMENUBARINFO);
		mbi.hwndParent = hwnd;
		mbi.nToolBarId = IDM_MAIN_MENU;
		mbi.hInstRes = g_hinst;
		mbi.nBmpId = 0;
		mbi.cBmpImages = 0;	
		SHCreateMenuBar(&mbi);
		g_hwnd_mb = mbi.hwndMB;

		g_hwnd_status = CreateWindow(TEXT("STATIC"), TEXT("Status"), WS_CHILD|WS_VISIBLE|SS_LEFT, 0, 0, disp_w, caption_h-1, hwnd, NULL, g_hinst, NULL);
		do_layout();
	}
		break;
		
	case WM_COMMAND:
		if (HandleCommand(hwnd, wParam, lParam))
			return DefWindowProc(hwnd, msg, wParam, lParam);
		break;

	case WM_TIMER:
		update_state_info();
		break;
		
	case WM_HOTKEY:
		break;
		
    case WM_INITMENUPOPUP:
        OnMenuPopup(hwnd, wParam);
        break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
    case WM_SETFOCUS:
		freeze_display(0);
		set_status("WM_SETFOCUS");
		break;
    case WM_KILLFOCUS:
		freeze_display(1);
		set_status("WM_KILLFOCUS");
		break;
    case WM_ACTIVATE:
        if (WA_INACTIVE != LOWORD(wParam)) {
			freeze_display(0);
			SetFocus(hwnd);
		} else {
			freeze_display(1);
		}
		set_status("WM_ACTIVATE");
        break;

	case WM_LOADTERM:
		if (!LoadTerminal()) {
			MessageBox(hwnd, L"Cannot load GPAC", L"Error", MB_OK);
			PostQuitMessage(0);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
		
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	
	return 0;
}

BOOL InitInstance (int CmdShow)
{
	WNDCLASS wc;

    wc.style			= 0;
    wc.lpfnWndProc		= (WNDPROC) MainWndProc;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= g_hinst;
    wc.hIcon			= LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_ICON));
    wc.hCursor			= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName		= 0;
    wc.lpszClassName	= _T("Osmophone");
	RegisterClass(&wc);

	g_hwnd = CreateWindow(_T("Osmophone"), _T("Osmophone"), 
						  WS_VISIBLE,
						  0, caption_h, disp_w, disp_h, 
						  NULL, NULL, g_hinst, NULL);
	
	if (!g_hwnd) return FALSE;

	::SetTimer(g_hwnd, STATE_TIMER_ID, STATE_TIMER_DUR, NULL);

	refresh_recent_files();
	set_status("Loading Terminal");
	do_layout();

	PostMessage(g_hwnd, WM_LOADTERM, 0, 0);
	set_status("Ready");
	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, 
				   HINSTANCE hPrevInstance, 
				   LPWSTR lpCmdLine, 
				   int nShowCmd 
) {
	MSG 	msg;
	HWND 	hwndOld = NULL;	
	const char *str;
	
	memset(&user, 0, sizeof(GF_User));
	term = NULL;

	g_hinst = hInstance;
	
	menu_h = GetSystemMetrics(SM_CYMENU) - 6;
	caption_h = GetSystemMetrics(SM_CYCAPTION) - 3;
	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);
	disp_w = screen_w;
	disp_h = screen_h - menu_h - caption_h;

	
	if (hwndOld = FindWindow(_T("Osmophone"), NULL))
	{
		SetForegroundWindow(hwndOld);    
		return 0;
	}

	user.config = gf_cfg_new("\\Storage\\Program Files\\Osmo4\\", "GPAC.cfg");
	str = gf_cfg_get_key(user.config, "General", "ModulesDirectory");

	user.modules = gf_modules_new((const unsigned char *) str, user.config);
	if (!gf_modules_get_count(user.modules)) {
		MessageBox(GetForegroundWindow(), _T("No modules found"), _T("GPAC Init Error"), MB_OK);
		gf_modules_del(user.modules);
		gf_cfg_del(user.config);
		memset(&user, 0, sizeof(GF_User));
		return 0;
	}
	str = gf_cfg_get_key(user.config, "General", "Loop");
	loop = (str && !stricmp(str, "yes")) ? 1 : 0;

	if (InitInstance(nShowCmd)) {
		while (GetMessage(&msg, NULL, 0,0) == TRUE) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}

	/*and destroy*/
	if (term) gf_term_del(term);
	if (user.modules) gf_modules_del(user.modules);
	if (user.config) gf_cfg_del(user.config);

	if (backlight_off) set_backlight_state(0);
	return 0;
}
