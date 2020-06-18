/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
/*for initial setup*/
#include <gpac/modules/service.h>

#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#ifdef _WIN32_WCE
#include <aygshell.h>
#include <gx.h>

#else

#ifndef _T
#define _T(__a) (LPCSTR) __a
#endif

#endif

#include "resource.h"

#define WM_LOADTERM	WM_USER + 1
#define STATE_TIMER_ID		20
#define STATE_TIMER_DUR		1000
#define GPAC_TIMER_ID		21
#define GPAC_TIMER_DUR		33


Bool gf_file_dialog(HINSTANCE inst, HWND parent, char *url, const char *ext_list, GF_Config *cfg);
void set_backlight_state(Bool disable);
void refresh_recent_files();
void do_layout(Bool notif_size);

static HWND g_hwnd = NULL;
static HWND g_hwnd_disp = NULL;
static HWND g_hwnd_menu = NULL;
static HWND g_hwnd_status = NULL;
static HINSTANCE g_hinst = NULL;
static Bool is_ppc = GF_FALSE;

static Bool is_connected = GF_FALSE;
static Bool navigation_on = GF_FALSE;
static Bool playlist_navigation_on = GF_TRUE;

static u32 log_level = GF_LOG_ERROR;

static u32 Duration;
static Bool CanSeek = GF_FALSE;
static u32 Volume=100;
static char the_url[GF_MAX_PATH] = "";
static Bool NavigateTo = GF_FALSE;
static char the_next_url[GF_MAX_PATH];
static GF_Terminal *term;
static GF_User user;
static u32 disp_w = 0;
static u32 disp_h = 0;
static u32 screen_w = 0;
static u32 screen_h = 0;
static u32 menu_h = 0;
static u32 caption_h = 0;
static u32 ratio_h = 1;
static Bool backlight_off = GF_FALSE;
static u32 prev_batt_bl, prev_ac_bl;
static Bool show_status = GF_TRUE;
static Bool reset_status = GF_TRUE;
static u32 last_state_time = 0;
static Bool loop = GF_FALSE;
static Bool full_screen = GF_FALSE;
static Bool force_2d_gl = GF_FALSE;
static Bool ctrl_mod_down = GF_FALSE;
static Bool view_cpu = GF_TRUE;
static Bool use_low_fps = GF_FALSE;
static Bool use_svg_prog = GF_FALSE;

static Bool log_rti = GF_FALSE;
static FILE *log_file = NULL;
static u32 rti_update_time_ms = 200;

static u32 playlist_act = 0;


void recompute_res(u32 sw, u32 sh )
{
	caption_h = GetSystemMetrics(SM_CYCAPTION);
	menu_h = GetSystemMetrics(SM_CYMENU)+2;
	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);

	ratio_h = sh / screen_h;
	screen_w = sw;
	screen_h = sh;
	menu_h *= ratio_h;
//	caption_h *= ratio_h;
	disp_w = screen_w;
	disp_h = screen_h - menu_h /*- caption_h*/;
}


void set_status(char *state)
{
	if (show_status && g_hwnd_status) {
#ifdef _WIN32_WCE
		TCHAR wstate[1024];
		CE_CharToWide(state, (u16 *) wstate);
		SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) wstate);
#else
		SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) state);
#endif
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
		if (GetTickCount() > last_state_time + 200) {
			last_state_time = 0;
			reset_status = GF_TRUE;
		}
		else return;
	}
	if (!term) return;
	if (!is_connected && reset_status) {
		SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) TEXT("Ready") );
		reset_status = GF_FALSE;
		return;
	}

	FPS = gf_term_get_framerate(term, GF_FALSE);
	time = gf_term_get_time_in_ms(term) / 1000;
	m = time/60;
	s = time - m*60;
	if (view_cpu) {
		GF_SystemRTInfo rti;
		if (!gf_sys_get_rti(STATE_TIMER_DUR, &rti, 0)) return;
		wsprintf(wstate, TEXT("T %02d:%02d : FPS %02.2f : CPU %02d"), m, s, FPS, rti.total_cpu_usage);
	} else {
		wsprintf(wstate, TEXT("T %02d:%02d : FPS %02.2f"), m, s, FPS);
	}
	SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) wstate);
}


static u64 prev_pos = 0;
void cbk_on_progress(void *_title, u64 done, u64 total)
{
#if 0
	char szMsg[1024];
	u32 pos = (u32) ((u64) done * 100)/total;
	if (pos<prev_pos) prev_pos = 0;
	if (pos!=prev_pos) {
		prev_pos = pos;
		sprintf(szMsg, "%s: (%02d/100)", _title ? (char *)_title : "", pos);
		set_status(szMsg);
	}
#endif
}

void set_full_screen()
{
	full_screen = (Bool)!full_screen;
	if (full_screen) {
		show_status = GF_FALSE;
		do_layout(GF_FALSE);
		gf_term_set_option(term, GF_OPT_FULLSCREEN, full_screen);
	} else {
		const char *str = gf_cfg_get_key(user.config, "General", "ShowStatusBar");
		show_status = (str && !strcmp(str, "yes")) ? GF_TRUE : GF_FALSE;
		gf_term_set_option(term, GF_OPT_FULLSCREEN, full_screen);
		do_layout(GF_TRUE);
	}
}


static void on_gpac_rti_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	if (!log_file) return;

	if (lm & GF_LOG_RTI) {
		GF_SystemRTInfo rti;
		if (fmt) vfprintf(log_file, fmt, list);

		gf_sys_get_rti(rti_update_time_ms, &rti, 0);

		fprintf(log_file, "% 8d\t% 8d\t% 8d\t% 4d\t% 8d\t\t",
		        gf_sys_clock(),
		        gf_term_get_time_in_ms(term),
		        rti.total_cpu_usage,
		        (u32) gf_term_get_framerate(term, GF_FALSE),
		        (u32) ( rti.gpac_memory / 1024)
		       );
	} else if (fmt && (ll>=GF_LOG_INFO)) {
		vfprintf(log_file, fmt, list);
	}
}
static void on_gpac_log(void *cbk, GF_LOG_Level ll, GF_LOG_Tool lm, const char *fmt, va_list list)
{
	if (fmt && log_file) vfprintf(log_file, fmt, list);
}


static void setup_logs()
{
	if (log_file) gf_fclose(log_file);
	log_file = NULL;

	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
	gf_log_set_callback(NULL, NULL);

	if (log_rti) {
		const char *filename = gf_cfg_get_key(user.config, "General", "LogFile");
		if (!filename) {
			gf_cfg_set_key(user.config, "General", "LogFile", "\\gpac_logs.txt");
			filename = "\\gpac_logs.txt";
		}
		log_file = gf_fopen(filename, "a+t");

		fprintf(log_file, "!! GPAC RunTime Info for file %s !!\n", the_url);
		fprintf(log_file, "SysTime(ms)\tSceneTime(ms)\tCPU\tFPS\tMemory(kB)\tObservation\n");

		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
		gf_log_set_tool_level(GF_LOG_RTI, GF_LOG_DEBUG);
		gf_log_set_callback(log_file, on_gpac_rti_log);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
	} else {
		const char *filename = gf_cfg_get_key(user.config, "General", "LogFile");
		if (!filename) {
			gf_cfg_set_key(user.config, "General", "LogFile", "\\gpac_logs.txt");
			filename = "\\gpac_logs.txt";
		}
		const char *logs = gf_cfg_get_key(user.config, "General", "Logs");
		if (logs) {
			if (gf_log_set_tools_levels( logs ) != GF_OK) {
			} else {
				if (log_file = gf_fopen(filename, "a+t")) {
					gf_log_set_callback(log_file, on_gpac_log);
				}
			}
		}
	}
}

void do_open_file()
{
	gf_cfg_set_key(user.config, "RecentFiles", the_url, NULL);
	gf_cfg_insert_key(user.config, "RecentFiles", the_url, "", 0);
	u32 count = gf_cfg_get_key_count(user.config, "RecentFiles");
	if (count > 10) gf_cfg_set_key(user.config, "RecentFiles", gf_cfg_get_key_name(user.config, "RecentFiles", count-1), NULL);

	setup_logs();

	gf_term_connect(term, the_url);
}

void switch_playlist(Bool play_prev)
{
	char szPLE[20];
	u32 idx = 0;
	u32 count;
	const char *ple = gf_cfg_get_key(user.config, "General", "PLEntry");
	if (ple) idx = atoi(ple);

	count = gf_cfg_get_key_count(user.config, "Playlist");
	if (!count) return;
	/*not the first launch*/
	if (strlen(the_url)) {
		if (!idx && play_prev) return;
		if (play_prev) idx--;
		else idx++;
		if (idx>=count) return;
	} else {
		if (idx>=count) idx=0;
	}

	ple = gf_cfg_get_key_name(user.config, "Playlist", idx);
	if (!ple) return;

	sprintf(szPLE, "%d", idx);
	gf_cfg_set_key(user.config, "General", "PLEntry", szPLE);

	strcpy(the_url, ple);
	do_open_file();
}


Bool GPAC_EventProc(void *ptr, GF_Event *evt)
{
	switch (evt->type) {
	case GF_EVENT_DURATION:
		Duration = (u32) (evt->duration.duration*1000);
		CanSeek = evt->duration.can_seek;
		break;
	case GF_EVENT_MESSAGE:
	{
		if (!evt->message.message) return GF_FALSE;
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("%s: %s\n", evt->message.message, gf_error_to_string(evt->message.error)));
		//set_status((char *) evt->message.message);
	}
	break;
	case GF_EVENT_PROGRESS:
	{
		char *szTitle = "";
		if (evt->progress.progress_type==0) szTitle = "Buffer ";
		else if (evt->progress.progress_type==1) szTitle = "Download ";
		else if (evt->progress.progress_type==2) szTitle = "Import ";
		cbk_on_progress(szTitle, evt->progress.done, evt->progress.total);
	}
	break;

	case GF_EVENT_SIZE:
		break;
	case GF_EVENT_RESOLUTION:
		recompute_res(evt->size.width, evt->size.height);
		do_layout(GF_TRUE);
		break;

	case GF_EVENT_SCENE_SIZE:
		do_layout(GF_TRUE);
		break;
	case GF_EVENT_DBLCLICK:
		set_full_screen();
		return GF_FALSE;
	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = GF_TRUE;
			if (!backlight_off) set_backlight_state(GF_TRUE);
			refresh_recent_files();
			navigation_on = (gf_term_get_option(term, GF_OPT_NAVIGATION)==GF_NAVIGATE_NONE) ? GF_FALSE : GF_TRUE;
		} else {
			navigation_on = GF_FALSE;
			is_connected = GF_FALSE;
			Duration = 0;
		}
		break;
	case GF_EVENT_EOS:
		if (Duration>2000)
			gf_term_play_from_time(term, 0, 0);
		break;
	case GF_EVENT_QUIT:
		PostMessage(g_hwnd, WM_DESTROY, 0, 0);
		break;
	case GF_EVENT_KEYDOWN:
		switch (evt->key.key_code) {
		case GF_KEY_ENTER:
			if (full_screen) set_full_screen();
			break;
		case GF_KEY_1:
			ctrl_mod_down = (Bool)!ctrl_mod_down;
			evt->key.key_code = GF_KEY_CONTROL;
			evt->type = ctrl_mod_down ? GF_EVENT_KEYDOWN : GF_EVENT_KEYUP;
			gf_term_user_event(term, evt);
			break;
		case GF_KEY_MEDIAPREVIOUSTRACK:
			playlist_act = 2;
			break;
		case GF_KEY_MEDIANEXTTRACK:
			playlist_act = 1;
			break;
		}
		break;
	case GF_EVENT_NAVIGATE:
		if (gf_term_is_supported_url(term, evt->navigate.to_url, GF_TRUE, GF_TRUE)) {
			gf_term_navigate_to(term, evt->navigate.to_url);
			return GF_TRUE;
		} else {
#ifdef _WIN32_WCE
			u16 dst[1024];
#endif
			SHELLEXECUTEINFO info;

			/*
						if (full_screen) gf_term_set_option(term, GF_OPT_FULLSCREEN, 0);
						full_screen = 0;
			*/
			memset(&info, 0, sizeof(SHELLEXECUTEINFO));
			info.cbSize = sizeof(SHELLEXECUTEINFO);
			info.lpVerb = _T("open");
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpFile = _T("iexplore");
#ifdef _WIN32_WCE
			CE_CharToWide((char *) evt->navigate.to_url, dst);
			info.lpParameters = (LPCTSTR) dst;
#else
			info.lpParameters = evt->navigate.to_url;
#endif
			info.nShow = SW_SHOWNORMAL;
			ShellExecuteEx(&info);
		}
		return GF_TRUE;
	}
	return GF_FALSE;
}

//#define TERM_NOT_THREADED

Bool LoadTerminal()
{
	/*by default use current dir*/
	strcpy(the_url, ".");

	setup_logs();

	g_hwnd_disp = CreateWindow(TEXT("STATIC"), NULL, WS_CHILD | WS_VISIBLE , 0, 0, disp_w, disp_h, g_hwnd, NULL, g_hinst, NULL);

	user.EventProc = GPAC_EventProc;
	/*dummy in this case (global vars) but MUST be non-NULL*/
	user.opaque = user.modules;
	user.os_window_handler = g_hwnd_disp;
#ifdef TERM_NOT_THREADED
	user.init_flags = GF_TERM_NO_REGULATION;
	user.threads = 0;
#endif

	term = gf_term_new(&user);
	if (!term) {
		gf_modules_del(user.modules);
		gf_cfg_del(user.config);
		memset(&user, 0, sizeof(GF_User));
		return GF_FALSE;
	}

#ifdef _WIN32_WCE
	screen_w = term->compositor->video_out->max_screen_width;
	screen_h = term->compositor->video_out->max_screen_height;
	disp_w = screen_w;
	disp_h = screen_h - menu_h /*- caption_h*/;
#endif

#ifdef TERM_NOT_THREADED
	::SetTimer(g_hwnd, GPAC_TIMER_ID, GPAC_TIMER_DUR, NULL);
#endif

	const char *str = gf_cfg_get_key(user.config, "General", "StartupFile");
	if (str) {
		do_layout(GF_TRUE);
		strcpy(the_url, str);
		gf_term_connect(term, str);
	}
	return GF_TRUE;
}

void do_layout(Bool notif_size)
{
	u32 w, h;
	if (full_screen) {
		w = screen_w;
		h = screen_h;
		::ShowWindow(g_hwnd_status, SW_HIDE);
#ifdef _WIN32_WCE
		::ShowWindow(g_hwnd_menu, SW_HIDE);
#endif

#ifdef _WIN32_WCE
		SHFullScreen(g_hwnd, SHFS_HIDESIPBUTTON);
#endif
		::MoveWindow(g_hwnd, 0, 0, screen_w, screen_h, 1);
		::MoveWindow(g_hwnd_disp, 0, 0, screen_w, screen_h, 1);
		SetForegroundWindow(g_hwnd_disp);
	} else {
#ifdef _WIN32_WCE
		::ShowWindow(g_hwnd_menu, SW_SHOW);
#endif
		if (show_status) {
			::MoveWindow(g_hwnd, 0, 0, disp_w, disp_h, 1);
			::ShowWindow(g_hwnd_status, SW_SHOW);
			::MoveWindow(g_hwnd_status, 0, 0, disp_w, caption_h, 1);
			::MoveWindow(g_hwnd_disp, 0, caption_h, disp_w, disp_h - caption_h, 1);
			w = disp_w;
			h = disp_h - caption_h*ratio_h;
		} else {
			::ShowWindow(g_hwnd_status, SW_HIDE);
//			::MoveWindow(g_hwnd, 0, caption_h, disp_w, disp_h, 1);
			::MoveWindow(g_hwnd, 0, 0, disp_w, disp_h, 1);
			::MoveWindow(g_hwnd_disp, 0, 0, disp_w, disp_h, 1);
			w = disp_w;
			h = disp_h;
		}
	}
	if (notif_size && term) gf_term_set_size(term, w, h);
}


void set_backlight_state(Bool disable)
{
#ifdef _WIN32_WCE
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
		backlight_off = GF_TRUE;
	} else {
		if (prev_batt_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("BatteryTimeout"), NULL, REG_DWORD, (unsigned char *)&prev_batt_bl, dwSize);
		}
		if (prev_ac_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("ACTimeout"), NULL, REG_DWORD,(unsigned char *)&prev_ac_bl, dwSize);
		}
		backlight_off = GF_FALSE;
	}
	RegCloseKey(hKey);
	hBL = CreateEvent(NULL, FALSE, FALSE, _T("BackLightChangeEvent"));
	if (hBL) {
		SetEvent(hBL);
		CloseHandle(hBL);
	}
#endif
}

static Bool do_resume = GF_FALSE;
static Bool prev_backlight_state;
void gf_freeze_display(Bool do_gf_freeze)
{
	if (do_gf_freeze) {
		prev_backlight_state = backlight_off;
		do_resume = GF_FALSE;
		if (0 && is_connected && gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
			do_resume= GF_TRUE;
			gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
		/*gf_freeze display*/
		gf_term_set_option(term, GF_OPT_FREEZE_DISPLAY, 1);

		set_backlight_state(GF_FALSE);
		gf_sleep(100);
	} else {
		if (prev_backlight_state) set_backlight_state(GF_TRUE);
		gf_term_set_option(term, GF_OPT_FREEZE_DISPLAY, 0);

		if (do_resume) {
			gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
			set_backlight_state(GF_TRUE);
		}
	}
}

static void show_taskbar(Bool show_it)
{
#ifdef _WIN32_WCE
	HWND wnd;
	if (!is_ppc) return;

	wnd = GetForegroundWindow();
	wnd = g_hwnd;
	if (show_it) {
		SHFullScreen(wnd, SHFS_SHOWSTARTICON | SHFS_SHOWTASKBAR| SHFS_SHOWSIPBUTTON);
		::ShowWindow(::FindWindow(_T("HHTaskbar"),NULL), SW_SHOWNA);
	} else {
		::ShowWindow(::FindWindow(_T("HHTaskbar"),NULL), SW_HIDE);
		SHFullScreen(wnd, SHFS_HIDESTARTICON | SHFS_HIDETASKBAR| SHFS_HIDESIPBUTTON);
	}
#endif
}

void refresh_recent_files()
{
#ifdef _WIN32_WCE
	u32 count = gf_cfg_get_key_count(user.config, "RecentFiles");

	HMENU hMenu = (HMENU)SendMessage(g_hwnd_menu, SHCMBM_GETSUBMENU, 0, ID_MENU_FILE);
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

		CE_CharToWide(name, (u16 *) txt);
		AppendMenu(hMenu, MF_STRING, IDM_OPEN_FILE1+i, txt);
	}
#endif
}


void open_file(HWND hwnd)
{
	Bool res;
	char ext_list[4096];
	strcpy(ext_list, "");

	gf_freeze_display(GF_TRUE);

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
#ifdef _WIN32_WCE
	res = gf_file_dialog(g_hinst, hwnd, the_url, ext_list, user.config);
#else
	res = GF_FALSE;
#endif
	gf_freeze_display(GF_FALSE);

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
		char szText[4096];
		HWND hctl = GetDlgItem(hWnd, IDC_NAMECTRL);
		sprintf(szText, "GPAC/Osmo4\nversion %s", GPAC_FULL_VERSION);
#ifdef _WIN32_WCE
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

		u16 swText[4096];
		CE_CharToWide(szText, swText);

		SetWindowText(hctl, (LPCWSTR) swText);
#else
		SetWindowText(hctl, szText);
#endif
		MoveWindow(hctl, 0, 0, screen_w, 40, 1);
		MoveWindow(hWnd, 0, 0, screen_w, disp_w, 1);

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
	gf_freeze_display(GF_TRUE);
//	::ShowWindow(g_hwnd_mb, SW_HIDE);
	int iResult = DialogBox(g_hinst, MAKEINTRESOURCE(IDD_APPABOUT), hwnd,(DLGPROC)AboutDialogProc);
//	::ShowWindow(g_hwnd_mb, SW_SHOW);
	gf_freeze_display(GF_FALSE);
}

void pause_file()
{
	if (!is_connected) return;
#ifdef _WIN32_WCE
	TBBUTTONINFO tbbi;
	tbbi.cbSize = sizeof(tbbi);
	tbbi.dwMask = TBIF_TEXT;
#endif

	if (gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
#ifdef _WIN32_WCE
		tbbi.pszText = _T("Resume");
#endif
	} else {
		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
#ifdef _WIN32_WCE
		tbbi.pszText = _T("Pause");
#endif
	}
#ifdef _WIN32_WCE
	SendMessage(g_hwnd_menu, TB_SETBUTTONINFO, IDM_FILE_PAUSE, (LPARAM)&tbbi);
#endif
}

void set_svg_progressive()
{
	use_svg_prog = (Bool)!use_svg_prog;
	if (use_svg_prog) {
		gf_cfg_set_key(user.config, "SAXLoader", "Progressive", "yes");
		gf_cfg_set_key(user.config, "SAXLoader", "MaxDuration", "30");
	} else {
		gf_cfg_set_key(user.config, "SAXLoader", "MaxDuration", "0");
		gf_cfg_set_key(user.config, "SAXLoader", "Progressive", "no");
	}
}

void set_gx_mode(u32 mode)
{
	char *opt;
	switch (mode) {
	case 0:
		opt = "raw";
		break;
	case 1:
		opt = "gx";
		break;
	case 2:
		opt = "gdi";
		break;
	}
	gf_cfg_set_key(user.config, "GAPI", "FBAccess", opt);

//	gf_term_del(term);
//	term = gf_term_new(&user);
//	gf_term_disconnect(term);
//	gf_term_connect(term, the_url);
	do_layout(GF_TRUE);
}

void do_copy_paste()
{
	if (!OpenClipboard(g_hwnd)) return;

	/*or we are editing text and clipboard is not empty*/
	if (IsClipboardFormatAvailable(CF_TEXT) && (gf_term_paste_text(term, NULL, GF_TRUE)==GF_OK)) {
		HGLOBAL hglbCopy = GetClipboardData(CF_TEXT);
		if (hglbCopy) {
#ifdef _WIN32_WCE
			char szString[1024];
			LPCTSTR paste_string = (LPCTSTR) GlobalLock(hglbCopy);
			CE_WideToChar((u16 *) paste_string, szString);
			gf_term_paste_text(term, szString, GF_FALSE);
#else
			char *szString = (char *)GlobalLock(hglbCopy);
			gf_term_paste_text(term, szString, GF_FALSE);
#endif
			GlobalUnlock(hglbCopy);
		}
	}
	/*we have something to copy*/
	else if (gf_term_get_text_selection(term, GF_TRUE)!=NULL) {
		u32 len;
		const char *text = gf_term_get_text_selection(term, GF_FALSE);
		if (text && strlen(text)) {
			EmptyClipboard();
			len = strlen(text);

#ifdef _WIN32_WCE
			HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(u16));
			LPCTSTR new_string = (LPCTSTR) GlobalLock(hglbCopy);
			CE_CharToWide((char*)text, (u16*)new_string);
#else
			HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(u8));
			char *new_string = (char*) GlobalLock(hglbCopy);
			strcpy(new_string, text);
#endif
			GlobalUnlock(hglbCopy);
			SetClipboardData(CF_TEXT, hglbCopy);
		}
	}
	CloseClipboard();
}

static void rewrite_log_tools(u32 tool)
{
	char *logs;
	if (tool == GF_LOG_ALL) {
		gf_log_set_tool_level(GF_LOG_ALL, log_level);
	} else {
		if (gf_log_tool_level_on(tool, log_level)) {
			gf_log_set_tool_level(tool, GF_LOG_QUIET);
		} else {
			gf_log_set_tool_level(tool, log_level);
		}
	}
	logs = gf_log_get_tools_levels();
	if (logs) {
		gf_cfg_set_key(user.config, "General", "Logs", logs);
		gf_free(logs);
	}
	setup_logs();
}

BOOL HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) {
	case IDM_FILE_OPEN:
		open_file(hwnd);
		break;
	case IDM_FILE_LOG_RTI:
		log_rti = (Bool)!log_rti;
		setup_logs();
		break;
	case IDM_FILE_PAUSE:
		pause_file();
		break;
	case IDM_VIEW_ABOUT:
		view_about(hwnd);
		break;
	case IDM_VIEW_FS:
		set_full_screen();
		break;
	case IDM_VIEW_STATUS:
		show_status = (Bool)!show_status;
		gf_cfg_set_key(user.config, "General", "ShowStatusBar", show_status ? "yes" : "no");
		do_layout(GF_TRUE);
		break;
	case IDM_VIEW_CPU:
		view_cpu = (Bool)!view_cpu;
		break;
	case IDM_VIEW_FORCEGL:
		force_2d_gl = (Bool)!force_2d_gl;
		gf_cfg_set_key(user.config, "Compositor", "ForceOpenGL", force_2d_gl ? "yes" : "no");
		gf_term_set_option(term, GF_OPT_USE_OPENGL, force_2d_gl);
		break;
	case IDM_NAV_RESET:
		gf_term_set_option(term, GF_OPT_NAVIGATION_TYPE, 0);
		break;
	case IDM_NAV_NONE:
		gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
		break;
	case IDM_NAV_SLIDE:
		gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
		break;
	case IDM_NAV_WALK:
		gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_WALK);
		break;
	case IDM_NAV_FLY:
		gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_FLY);
		break;
	case IDM_NAV_EXAMINE:
		gf_term_set_option(term, GF_OPT_NAVIGATION, GF_NAVIGATE_EXAMINE);
		break;
	case IDM_NAV_HEADLIGHT:
		gf_term_set_option(term, GF_OPT_HEADLIGHT, !gf_term_get_option(term, GF_OPT_HEADLIGHT) );
		break;
	case IDM_NAV_COL_NONE:
		gf_term_set_option(term, GF_OPT_COLLISION, GF_COLLISION_NONE);
		break;
	case IDM_NAV_COL_REG:
		gf_term_set_option(term, GF_OPT_COLLISION, GF_COLLISION_NORMAL);
		break;
	case IDM_NAV_COL_DISP:
		gf_term_set_option(term, GF_OPT_COLLISION, GF_COLLISION_DISPLACEMENT);
		break;
	case IDM_NAV_GRAVITY:
		gf_term_set_option(term, GF_OPT_GRAVITY, !gf_term_get_option(term, GF_OPT_GRAVITY));
		break;

	case IDM_VIEW_AR_NONE:
		gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
		break;
	case IDM_VIEW_AR_FILL:
		gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);
		break;
	case IDM_VIEW_AR_4_3:
		gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);
		break;
	case IDM_VIEW_AR_16_9:
		gf_term_set_option(term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);
		break;

	case IDM_VIEW_LOW_RATE:
		use_low_fps = (Bool)!use_low_fps;
		gf_term_set_simulation_frame_rate(term, use_low_fps ? 15.0 : 30.0);
		break;
	case ID_VIDEO_DIRECTDRAW:
	{
		u32 drend = (gf_term_get_option(term, GF_OPT_DRAW_MODE)==GF_DRAW_MODE_IMMEDIATE) ? GF_DRAW_MODE_DEFER : GF_DRAW_MODE_IMMEDIATE;
		gf_term_set_option(term, GF_OPT_DRAW_MODE, drend);
		gf_cfg_set_key(user.config, "Compositor", "DrawMode", drend ? "immediate" : "defer");
	}
	break;

	case ID_FILE_CUT_PASTE:
		do_copy_paste();
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

	case IDS_CAP_DISABLE_PLAYLIST:
		playlist_navigation_on = (Bool)!playlist_navigation_on;
		break;
	case IDM_VIEW_SVG_LOAD:
		set_svg_progressive();
		break;
	case ID_VIDEO_DIRECTFB:
		set_gx_mode(0);
		break;
	case ID_VIDEO_GAPI:
		set_gx_mode(1);
		break;
	case ID_VIDEO_GDI:
		set_gx_mode(2);
		break;

	case ID_LOGLEVEL_NONE:
		log_level = GF_LOG_QUIET;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_LOGLEVEL_ERROR:
		log_level = GF_LOG_ERROR;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_LOGLEVEL_WARNING:
		log_level = GF_LOG_WARNING;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_LOGLEVEL_INFO:
		log_level = GF_LOG_INFO;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_LOGLEVEL_DEBUG:
		log_level = GF_LOG_DEBUG;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_TOOLS_CORE:
		rewrite_log_tools(GF_LOG_CORE);
		break;
	case ID_TOOLS_CODING:
		rewrite_log_tools(GF_LOG_CODING);
		break;
	case ID_TOOLS_CONTAINER:
		rewrite_log_tools(GF_LOG_CONTAINER);
		break;
	case ID_TOOLS_NETWORK:
		rewrite_log_tools(GF_LOG_NETWORK);
		break;
	case ID_TOOLS_RTP:
		rewrite_log_tools(GF_LOG_RTP);
		break;
	case ID_TOOLS_SCRIPT:
		rewrite_log_tools(GF_LOG_SCRIPT);
		break;
	case ID_TOOLS_CODEC:
		rewrite_log_tools(GF_LOG_CODEC);
		break;
	case ID_TOOLS_PARSER:
		rewrite_log_tools(GF_LOG_PARSER);
		break;
	case ID_TOOLS_MEDIA:
		rewrite_log_tools(GF_LOG_MEDIA);
		break;
	case ID_TOOLS_SCENE:
		rewrite_log_tools(GF_LOG_SCENE);
		break;
	case ID_TOOLS_INTERACT:
		rewrite_log_tools(GF_LOG_INTERACT);
		break;
	case ID_TOOLS_COMPOSE:
		rewrite_log_tools(GF_LOG_COMPOSE);
		break;
	case ID_TOOLS_MMIO:
		rewrite_log_tools(GF_LOG_MMIO);
		break;
	case ID_TOOLS_RTI:
		rewrite_log_tools(GF_LOG_RTI);
		break;
	case ID_TOOLS_ALL:
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_TOOLS_NONE:
		log_level = GF_LOG_QUIET;
		rewrite_log_tools(GF_LOG_ALL);
		break;
	case ID_LOGS_RESET:
		if (log_file) {
			gf_fclose(log_file);
			log_file = NULL;
		}
		{
			const char *filename = gf_cfg_get_key(user.config, "General", "LogFile");
			if (filename) gf_delete_file(filename);
		}
		setup_logs();
		break;

	case IDM_ITEM_QUIT:
		DestroyWindow(hwnd);
		return FALSE;
	}
	return TRUE;
}

static BOOL OnMenuPopup(const HWND hWnd, const WPARAM wParam)
{
	char *opt;
	u32 type;
	CheckMenuItem((HMENU)wParam, IDM_VIEW_STATUS, MF_BYCOMMAND| (show_status ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, IDM_VIEW_FORCEGL, MF_BYCOMMAND| (force_2d_gl ? MF_CHECKED : MF_UNCHECKED) );
	EnableMenuItem((HMENU)wParam, IDM_VIEW_CPU, MF_BYCOMMAND| (show_status ? MF_ENABLED : MF_GRAYED) );
	if (show_status) CheckMenuItem((HMENU)wParam, IDM_VIEW_CPU, MF_BYCOMMAND| (view_cpu ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, IDM_VIEW_LOW_RATE, MF_BYCOMMAND| (use_low_fps ? MF_CHECKED : MF_UNCHECKED) );

	EnableMenuItem((HMENU)wParam, ID_VIDEO_DIRECTDRAW, MF_BYCOMMAND| (!force_2d_gl ? MF_ENABLED : MF_GRAYED) );
	CheckMenuItem((HMENU)wParam, ID_VIDEO_DIRECTDRAW, MF_BYCOMMAND| (!force_2d_gl && gf_term_get_option(term, GF_OPT_DRAW_MODE) ? MF_CHECKED : MF_UNCHECKED) );

	CheckMenuItem((HMENU)wParam, IDM_VIEW_SVG_LOAD, MF_BYCOMMAND| (use_svg_prog ? MF_CHECKED : MF_UNCHECKED) );

	opt = (char *)gf_cfg_get_key(user.config, "GAPI", "FBAccess");
	if (!opt) opt = "raw";
	CheckMenuItem((HMENU)wParam, ID_VIDEO_DIRECTFB, MF_BYCOMMAND| ( (!strcmp(opt,"raw")) ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_VIDEO_GAPI, MF_BYCOMMAND| ( (!strcmp(opt,"gx")) ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_VIDEO_GDI, MF_BYCOMMAND| ( (!strcmp(opt,"gdi")) ? MF_CHECKED : MF_UNCHECKED) );


	type = gf_term_get_option(term, GF_OPT_ASPECT_RATIO);
	CheckMenuItem((HMENU)wParam, IDS_CAP_DISABLE_PLAYLIST, MF_BYCOMMAND| (!playlist_navigation_on ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_NONE, MF_BYCOMMAND| (type==GF_ASPECT_RATIO_KEEP) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_FILL, MF_BYCOMMAND| (type==GF_ASPECT_RATIO_FILL_SCREEN) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_4_3, MF_BYCOMMAND| (type==GF_ASPECT_RATIO_4_3) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_16_9, MF_BYCOMMAND| (type==GF_ASPECT_RATIO_16_9) ? MF_CHECKED : MF_UNCHECKED);

	EnableMenuItem((HMENU)wParam, IDM_FILE_PAUSE, MF_BYCOMMAND| (is_connected ? MF_ENABLED : MF_GRAYED) );

	if (/*we have something to copy*/
	    (gf_term_get_text_selection(term, GF_TRUE)!=NULL)
	    /*or we are editing text and clipboard is not empty*/
	    || (IsClipboardFormatAvailable(CF_TEXT) && (gf_term_paste_text(term, NULL, GF_TRUE)==GF_OK))
	) {
		EnableMenuItem((HMENU)wParam, ID_FILE_CUT_PASTE, MF_BYCOMMAND| MF_ENABLED);
	} else {
		EnableMenuItem((HMENU)wParam, ID_FILE_CUT_PASTE, MF_BYCOMMAND| MF_GRAYED);
	}

	CheckMenuItem((HMENU)wParam, IDM_FILE_LOG_RTI, MF_BYCOMMAND| (log_rti) ? MF_CHECKED : MF_UNCHECKED);

	if (!is_connected) type = 0;
	else type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	navigation_on = type ? GF_TRUE : GF_FALSE;

	EnableMenuItem((HMENU)wParam, IDM_NAV_NONE, MF_BYCOMMAND | (!type ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_SLIDE, MF_BYCOMMAND | (!type ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_RESET, MF_BYCOMMAND | (!type ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_WALK, MF_BYCOMMAND | ( (type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_FLY, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_EXAMINE, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_COL_NONE, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_COL_REG, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_COL_DISP, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_GRAVITY, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem((HMENU)wParam, IDM_NAV_HEADLIGHT, MF_BYCOMMAND | ( (type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );

	if (type) {
		u32 mode = gf_term_get_option(term, GF_OPT_NAVIGATION);
		navigation_on = (mode==GF_NAVIGATE_NONE) ? GF_FALSE : GF_TRUE;
		CheckMenuItem((HMENU)wParam, IDM_NAV_NONE, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_NONE) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_NAV_SLIDE, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_SLIDE) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_NAV_WALK, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_WALK) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_NAV_FLY, MF_BYCOMMAND | ((mode==GF_NAVIGATE_FLY) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_NAV_EXAMINE, MF_BYCOMMAND | ((mode==GF_NAVIGATE_EXAMINE) ? MF_CHECKED : MF_UNCHECKED) );

		if (type==GF_NAVIGATE_TYPE_3D) {
			CheckMenuItem((HMENU)wParam, IDM_NAV_HEADLIGHT, MF_BYCOMMAND | ( gf_term_get_option(term, GF_OPT_HEADLIGHT) ? MF_CHECKED : MF_UNCHECKED) );
			CheckMenuItem((HMENU)wParam, IDM_NAV_GRAVITY, MF_BYCOMMAND | ( gf_term_get_option(term, GF_OPT_GRAVITY) ? MF_CHECKED : MF_UNCHECKED) );
			type = gf_term_get_option(term, GF_OPT_COLLISION);
			CheckMenuItem((HMENU)wParam, IDM_NAV_COL_NONE, MF_BYCOMMAND | ( (type==GF_COLLISION_NONE) ? MF_CHECKED : MF_UNCHECKED) );
			CheckMenuItem((HMENU)wParam, IDM_NAV_COL_REG, MF_BYCOMMAND | ( (type==GF_COLLISION_NORMAL) ? MF_CHECKED : MF_UNCHECKED) );
			CheckMenuItem((HMENU)wParam, IDM_NAV_COL_DISP, MF_BYCOMMAND | ( (type==GF_COLLISION_DISPLACEMENT) ? MF_CHECKED : MF_UNCHECKED) );
		}
	}

	opt = (char *) gf_cfg_get_key(user.config, "General", "Logs");
	CheckMenuItem((HMENU)wParam, ID_LOGLEVEL_NONE, MF_BYCOMMAND | ( strstr(opt, "quiet") ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_LOGLEVEL_ERROR, MF_BYCOMMAND | ( strstr(opt, "error") ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_LOGLEVEL_WARNING, MF_BYCOMMAND | ( strstr(opt, "warning") ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_LOGLEVEL_INFO, MF_BYCOMMAND | ( strstr(opt, "info") ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, ID_LOGLEVEL_DEBUG, MF_BYCOMMAND | ( strstr(opt, "debug") ? MF_CHECKED : MF_UNCHECKED) );

#define CHECK_TOOL(_ID, _name) CheckMenuItem((HMENU)wParam, _ID, MF_BYCOMMAND | ( ((strstr(opt, _name) != NULL) || !strcmp(opt, "all")) ? MF_CHECKED : MF_UNCHECKED) );
	opt = (char *) gf_cfg_get_key(user.config, "General", "Logs");
	CHECK_TOOL(ID_TOOLS_CORE, "core");
	CHECK_TOOL(ID_TOOLS_CODING, "coding");
	CHECK_TOOL(ID_TOOLS_CONTAINER, "container");
	CHECK_TOOL(ID_TOOLS_NETWORK, "network");
	CHECK_TOOL(ID_TOOLS_RTP, "rtp");
	CHECK_TOOL(ID_TOOLS_SYNC, "sync");
	CHECK_TOOL(ID_TOOLS_SCRIPT, "script");
	CHECK_TOOL(ID_TOOLS_CODEC, "codec");
	CHECK_TOOL(ID_TOOLS_PARSER, "parser");
	CHECK_TOOL(ID_TOOLS_MEDIA, "media");
	CHECK_TOOL(ID_TOOLS_SCENE, "scene");
	CHECK_TOOL(ID_TOOLS_INTERACT, "interact");
	CHECK_TOOL(ID_TOOLS_COMPOSE, "compose");
	CHECK_TOOL(ID_TOOLS_MMIO, "mmio");
	CHECK_TOOL(ID_TOOLS_RTI, "rti");


	return TRUE;
}


BOOL CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
	{
#ifdef _WIN32_WCE
		SHMENUBARINFO mbi;
		memset(&mbi, 0, sizeof(SHMENUBARINFO));
		mbi.cbSize = sizeof(SHMENUBARINFO);
		mbi.hwndParent = hwnd;
		mbi.hInstRes = g_hinst;
		mbi.nBmpId = 0;
		mbi.cBmpImages = 0;

		mbi.nToolBarId = IDM_MAIN_MENU;
		SHCreateMenuBar(&mbi);
		g_hwnd_menu = mbi.hwndMB;
#else
		g_hwnd_menu = NULL;
#endif

		g_hwnd_status = CreateWindow(TEXT("STATIC"), TEXT("Status"), WS_CHILD|WS_VISIBLE|SS_LEFT, 0, 0, disp_w, caption_h, hwnd, NULL, g_hinst, NULL);

		do_layout(GF_TRUE);
	}
	break;

	case WM_COMMAND:
		if (HandleCommand(hwnd, wParam, lParam))
			return DefWindowProc(hwnd, msg, wParam, lParam);
		break;
	case WM_TIMER:
		if (wParam==STATE_TIMER_ID) update_state_info();
#ifdef TERM_NOT_THREADED
		else gf_term_process_step(term);
#endif
		break;
	case WM_HOTKEY:
		break;
	case WM_KEYDOWN:
		if (playlist_navigation_on && !navigation_on) {
			if (wParam==VK_LEFT) {
				switch_playlist(GF_TRUE);
				break;
			}
			else if (wParam==VK_RIGHT) {
				switch_playlist(GF_FALSE);
				break;
			}
		}
	/*fall through*/
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONUP:
		::SendMessage(g_hwnd_disp, msg, wParam, lParam);
		return 0;
	case WM_INITMENUPOPUP:
		OnMenuPopup(hwnd, wParam);
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_SETFOCUS:
		gf_freeze_display(GF_FALSE);
		break;
	case WM_KILLFOCUS:
		if ((HWND) wParam==g_hwnd) {
			gf_freeze_display(GF_TRUE);
		}
		break;
	case WM_ACTIVATE:
		if (WA_INACTIVE != LOWORD(wParam)) {
			if ((HWND) lParam == g_hwnd) {
				gf_freeze_display(GF_FALSE);
				SetFocus(hwnd);
			}
		} else {
			if ((HWND) lParam == g_hwnd_disp) {
				gf_freeze_display(GF_TRUE);
			}
		}
		break;
	case WM_LOADTERM:
		if (!LoadTerminal()) {
			MessageBox(hwnd, _T("Cannot load GPAC"), _T("Error"), MB_OK);
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

#ifdef _WIN32_WCE
	int style = WS_VISIBLE;
#else
	int style = WS_POPUP;
#endif

	g_hwnd = CreateWindow(_T("Osmophone"), _T("Osmophone"),
	                      style,
	                      0, caption_h, disp_w, disp_h,
	                      NULL, NULL, g_hinst, NULL);

	if (!g_hwnd) return FALSE;

	ShowWindow(g_hwnd, CmdShow);

	refresh_recent_files();

	::SetTimer(g_hwnd, STATE_TIMER_ID, STATE_TIMER_DUR, NULL);
	do_layout(GF_TRUE);

	set_status("Loading Terminal");
	PostMessage(g_hwnd, WM_LOADTERM, 0, 0);
	set_status("Ready");
	return TRUE;
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

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
#ifdef _WIN32_WCE
                   LPWSTR lpCmdLine,
#else
                   LPSTR lpCmdLine,
#endif
                   int nShowCmd
                  ) {
	MSG 	msg;

	HWND 	hwndOld = NULL;
	const char *str;
	Bool initial_launch = GF_FALSE;

	if (hwndOld = FindWindow(_T("Osmophone"), NULL))
	{
		SetForegroundWindow(hwndOld);
		return 0;
	}

	memset(&user, 0, sizeof(GF_User));
	term = NULL;

	g_hinst = hInstance;

	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);

	/*are we in low res or high res*/
	RawFrameBufferInfo Info;
	HDC DC = GetDC(NULL);
	ExtEscape(DC, GETRAWFRAMEBUFFER, 0, NULL, sizeof(RawFrameBufferInfo), (char*)&Info);
	ReleaseDC(NULL,DC);
	if (Info.pFramePointer) {
		recompute_res(Info.cxPixels, Info.cyPixels);
	} else {
		recompute_res(screen_w, screen_h);
	}



#ifdef _WIN32_WCE
	TCHAR      szBuf[MAX_PATH];
	SystemParametersInfo(SPI_GETPLATFORMTYPE, MAX_PATH, szBuf, 0);
	if (! lstrcmp(szBuf, __TEXT("PocketPC"))) is_ppc = GF_TRUE;
	else if (! lstrcmp(szBuf, __TEXT("Palm PC2"))) is_ppc = GF_TRUE;
#endif


	user.config = gf_cfg_init(NULL, &initial_launch);
	if (!user.config) {
		MessageBox(NULL, _T("Couldn't locate GPAC config file"), _T("Fatal Error"), MB_OK);
		return 0;
	}

	str = gf_cfg_get_key(user.config, "Compositor", "ScreenWidth");
	if (str) screen_w = atoi(str);
	str = gf_cfg_get_key(user.config, "Compositor", "ScreenHeight");
	if (str) screen_h = atoi(str);
	disp_w = screen_w;
#ifdef _WIN32_WCE
	disp_h = screen_h - menu_h /*- caption_h*/;
#else
	disp_h = screen_h;
#endif

	gf_sys_init(GF_MemTrackerNone);
	user.modules = gf_modules_new(NULL, user.config);
	if (!gf_modules_get_count(user.modules)) {
		MessageBox(GetForegroundWindow(), _T("No modules found"), _T("GPAC Init Error"), MB_OK);
		gf_modules_del(user.modules);
		gf_cfg_del(user.config);
		memset(&user, 0, sizeof(GF_User));
		gf_sys_close();
		return 0;
	}

	/*first launch, register all files ext*/
	if (initial_launch) {
		/*FFMPEG registration - FFMPEG DLLs compiled with CEGCC cannot be loaded directly under WM 6.1
		cf http://cegcc.sourceforge.net/docs/faq.html#DllDoesNotWorkWithWindowsMobile6.1
		*/
		HKEY hKey = NULL;
		DWORD dwSize;
		DWORD dwValue;
		RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("System\\Loader\\LoadModuleLow"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, &dwSize);
		dwSize = 4;
		dwValue = 1;
		LONG res = RegSetValueEx(hKey, _T("avcodec-52.dll"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		res = RegSetValueEx(hKey, _T("avformat-52.dll"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		res = RegSetValueEx(hKey, _T("avutil-50.dll"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		res = RegSetValueEx(hKey, _T("swscale-0.dll"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		RegCloseKey(hKey);

		u32 i;
		for (i=0; i<gf_modules_get_count(user.modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(user.modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			if (ifce) {
				ifce->CanHandleURL(ifce, "test.test");
				gf_modules_close_interface((GF_BaseInterface *)ifce);
			}
		}
		MessageBox(NULL, _T("Thank you for installing GPAC"), _T("Initial setup done"), MB_OK);
	}

	str = gf_cfg_get_key(user.config, "General", "Loop");
	loop = (!str || !stricmp(str, "yes")) ? GF_TRUE : GF_FALSE;

	str = gf_cfg_get_key(user.config, "SAXLoader", "Progressive");
	use_svg_prog = (str && !strcmp(str, "yes")) ? GF_TRUE : GF_FALSE;

	str = gf_cfg_get_key(user.config, "General", "RTIRefreshPeriod");
	if (str) {
		rti_update_time_ms = atoi(str);
	} else {
		gf_cfg_set_key(user.config, "General", "RTIRefreshPeriod", "200");
	}

	str = gf_cfg_get_key(user.config, "General", "ShowStatusBar");
	show_status = (str && !strcmp(str, "yes")) ? GF_TRUE : GF_FALSE;


#ifdef _WIN32_WCE
	if (is_ppc) GXOpenInput();
#endif

	if (InitInstance(nShowCmd)) {
		SetForegroundWindow(g_hwnd);
		show_taskbar(GF_FALSE);

		force_2d_gl = (Bool)gf_term_get_option(term, GF_OPT_USE_OPENGL);

		while (GetMessage(&msg, NULL, 0,0) == TRUE) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);

			if (playlist_act) {
				switch_playlist((Bool)(playlist_act-1));
				playlist_act = 0;
			}
		}
		show_taskbar(GF_TRUE);
	}
#ifdef _WIN32_WCE
	if (is_ppc) GXCloseInput();
#endif

	/*and destroy*/
	if (term) gf_term_del(term);
	if (user.modules) gf_modules_del(user.modules);
	if (user.config) gf_cfg_del(user.config);

	if (backlight_off) set_backlight_state(GF_FALSE);

	gf_sys_close();
	if (log_file) gf_fclose(log_file);
	return 0;
}
