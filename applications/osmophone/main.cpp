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
/*for initial setup*/
#include <gpac/modules/service.h>

#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <aygshell.h>
#include "resource.h"

#include <gx.h>

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
static HWND g_hwnd_menu1 = NULL;
static HWND g_hwnd_menu2 = NULL;
static HWND g_hwnd_status = NULL;
static HINSTANCE g_hinst = NULL;
static Bool is_ppc = 0;

static Bool is_connected = 0;
static Bool navigation_on = 0;
static Bool playlist_navigation_on = 1;

static u32 Duration;
static Bool CanSeek = 0;
static u32 Volume=100;
static char the_url[GF_MAX_PATH] = "";
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
static Bool force_2d_gl = 0;
static Bool ctrl_mod_down = 0;
static Bool view_cpu = 0;
static Bool menu_switched = 0;
static Bool use_low_fps = 0;
static Bool use_svg_prog = 0;

static Bool log_rti = 0;
static FILE *rti_file = NULL;
static u32 rti_update_time_ms = 200;

static u32 playlist_act = 0;

void set_status(char *state)
{
	if (show_status && g_hwnd_status) {
		TCHAR wstate[1024];
		CE_CharToWide(state, (u16 *) wstate);
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
	if (!is_connected && reset_status) {
		SendMessage(g_hwnd_status, WM_SETTEXT, 0, (LPARAM) TEXT("Ready") );
		reset_status = 0;
		return;
	}

	FPS = gf_term_get_framerate(term, 0);
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


static u32 prev_pos = 0;
void cbk_on_progress(void *_title, u32 done, u32 total)
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
	full_screen = !full_screen;
	if (full_screen) {
		do_layout(0);
		gf_term_set_option(term, GF_OPT_FULLSCREEN, full_screen);
	} else {
		gf_term_set_option(term, GF_OPT_FULLSCREEN, full_screen);
		do_layout(1);
	}
}


static void on_gpac_rti_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	GF_SystemRTInfo rti;

	if (lm != GF_LOG_RTI) return;

	gf_sys_get_rti(rti_update_time_ms, &rti, 0);
	
	fprintf(rti_file, "% 8d\t% 8d\t% 8d\t% 4d\t% 8d\t\t", 
		gf_sys_clock(),
		gf_term_get_time_in_ms(term),
		rti.total_cpu_usage,
		(u32) gf_term_get_framerate(term, 0),
		(u32) ( rti.gpac_memory / 1024)
	);
	if (fmt) vfprintf(rti_file, fmt+6 /*[RTI] "*/, list);
}

static void on_gpac_log(void *cbk, u32 ll, u32 lm, const char *fmt, va_list list)
{
	if (rti_file && fmt) vfprintf(rti_file, fmt, list);	
}

static void setup_logs()
{
	if (rti_file) fclose(rti_file);
	rti_file = NULL;

	gf_log_set_level(GF_LOG_ERROR);
	gf_log_set_tools(0);
	gf_log_set_callback(NULL, NULL);

	if (log_rti) {
		rti_file = fopen("\\gpac_rti.txt", "a+t");

		fprintf(rti_file, "!! GPAC RunTime Info for file %s !!\n", the_url);
		fprintf(rti_file, "SysTime(ms)\tSceneTime(ms)\tCPU\tFPS\tMemory(kB)\tObservation\n");

		gf_log_set_level(GF_LOG_DEBUG);
		gf_log_set_tools(GF_LOG_RTI);
		gf_log_set_callback(rti_file, on_gpac_rti_log);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI] System state when enabling log\n"));
	} else {
		u32 lt, ll;
		const char *str = gf_cfg_get_key(user.config, "General", "LogLevel");
		ll=0;
		if (str && stricmp(str, "none")) {
			if (!stricmp(str, "debug")) ll = GF_LOG_DEBUG;
			else if (!stricmp(str, "info")) ll = GF_LOG_INFO;
			else if (!stricmp(str, "warning")) ll = GF_LOG_WARNING;
			else if (!stricmp(str, "error")) ll = GF_LOG_ERROR;
			gf_log_set_level(ll);
		}
		lt = 0;
		str = gf_cfg_get_key(user.config, "General", "LogTools");
		if (str) {
			char *sep;
			char *val = (char *) str;
			while (val) {
				sep = strchr(val, ':');
				if (sep) sep[0] = 0;
				if (!stricmp(val, "core")) lt |= GF_LOG_CODING;
				else if (!stricmp(val, "coding")) lt |= GF_LOG_CODING;
				else if (!stricmp(val, "container")) lt |= GF_LOG_CONTAINER;
				else if (!stricmp(val, "network")) lt |= GF_LOG_NETWORK;
				else if (!stricmp(val, "rtp")) lt |= GF_LOG_RTP;
				else if (!stricmp(val, "author")) lt |= GF_LOG_AUTHOR;
				else if (!stricmp(val, "sync")) lt |= GF_LOG_SYNC;
				else if (!stricmp(val, "codec")) lt |= GF_LOG_CODEC;
				else if (!stricmp(val, "parser")) lt |= GF_LOG_PARSER;
				else if (!stricmp(val, "media")) lt |= GF_LOG_MEDIA;
				else if (!stricmp(val, "scene")) lt |= GF_LOG_SCENE;
				else if (!stricmp(val, "script")) lt |= GF_LOG_SCRIPT;
				else if (!stricmp(val, "interact")) lt |= GF_LOG_INTERACT;
				else if (!stricmp(val, "compose")) lt |= GF_LOG_COMPOSE;
				else if (!stricmp(val, "mmio")) lt |= GF_LOG_MMIO;
				else if (!stricmp(val, "none")) ll = 0;
				else if (!stricmp(val, "all")) lt = 0xFFFFFFFF;
				if (!sep) break;
				sep[0] = ':';
				val = sep+1;
			}
			gf_log_set_tools(lt);
		}
		if (ll && (rti_file = fopen("\\gpac_logs.txt", "wt"))) {
			gf_log_set_callback(rti_file, on_gpac_log);
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
		if (!evt->message.message) return 0;
		set_status((char *) evt->message.message);
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
	case GF_EVENT_SCENE_SIZE:
		do_layout(1);
		break;
	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			is_connected = 1;
			if (!backlight_off) set_backlight_state(1);
			refresh_recent_files();
			navigation_on = (gf_term_get_option(term, GF_OPT_NAVIGATION)==GF_NAVIGATE_NONE) ? 0 : 1;
		} else {
			navigation_on = 0;
			is_connected = 0;
			Duration = 0;
		}
		break;
	case GF_EVENT_QUIT:
		break;
	case GF_EVENT_KEYDOWN:
		switch (evt->key.key_code) {
		case GF_KEY_ENTER:
			if (full_screen) set_full_screen();
			break;
		case GF_KEY_1:
			ctrl_mod_down = !ctrl_mod_down;
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
		if (gf_term_is_supported_url(term, evt->navigate.to_url, 1, 1)) {
			gf_term_navigate_to(term, evt->navigate.to_url);
			return 1;
		} else {
			u16 dst[1024];
			SHELLEXECUTEINFO info;

/*
			if (full_screen) gf_term_set_option(term, GF_OPT_FULLSCREEN, 0);
			full_screen = 0;
*/
			memset(&info, 0, sizeof(SHELLEXECUTEINFO));
			info.cbSize = sizeof(SHELLEXECUTEINFO);
			info.lpVerb = L"open";
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpFile = L"iexplore";
			CE_CharToWide((char *) evt->navigate.to_url, dst);
			info.lpParameters = (LPCTSTR) dst;
			info.nShow = SW_SHOWNORMAL;
			ShellExecuteEx(&info);
		}
		return 1;
	}
	return 0;
}

#define TERM_NOT_THREADED

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
	user.init_flags = GF_TERM_NO_VISUAL_THREAD | GF_TERM_NO_REGULATION;
#endif

	term = gf_term_new(&user);
	if (!term) {
		gf_modules_del(user.modules);
		gf_cfg_del(user.config);
		memset(&user, 0, sizeof(GF_User));
		return 0;
	}

#ifdef TERM_NOT_THREADED
	::SetTimer(g_hwnd, GPAC_TIMER_ID, GPAC_TIMER_DUR, NULL);
#endif

	const char *str = gf_cfg_get_key(user.config, "General", "StartupFile");
	if (str) {
		do_layout(1);
		strcpy(the_url, str);
		gf_term_connect(term, str);
	}
	return 1;
}

void do_layout(Bool notif_size)
{
	u32 w, h;
	if (full_screen) {
		w = screen_w;
		h = screen_h;
		::ShowWindow(g_hwnd_status, SW_HIDE);
		::ShowWindow(g_hwnd_menu1, SW_HIDE);
		::ShowWindow(g_hwnd_menu2, SW_HIDE);

		SHFullScreen(g_hwnd, SHFS_HIDESIPBUTTON);
		::MoveWindow(g_hwnd, 0, 0, screen_w, screen_h, 1);
		::MoveWindow(g_hwnd_disp, 0, 0, screen_w, screen_h, 1);
		SetForegroundWindow(g_hwnd_disp);
	} else {
		::ShowWindow(g_hwnd_menu1, menu_switched ? SW_HIDE : SW_SHOW);
		::ShowWindow(g_hwnd_menu2, menu_switched ? SW_SHOW : SW_HIDE);
		if (show_status) {
			::MoveWindow(g_hwnd, 0, 0, disp_w, disp_h, 1);
			::ShowWindow(g_hwnd_status, SW_SHOW);
			::MoveWindow(g_hwnd_status, 0, 0, disp_w, caption_h, 1);
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
	if (notif_size && term) gf_term_set_size(term, w, h);
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

static void show_taskbar(Bool show_it)
{
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
}

void refresh_recent_files()
{
	u32 count = gf_cfg_get_key_count(user.config, "RecentFiles");

    HMENU hMenu = (HMENU)SendMessage(g_hwnd_menu1, SHCMBM_GETSUBMENU, 0, ID_MENU_FILE);
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
	res = gf_file_dialog(g_hinst, hwnd, the_url, ext_list, user.config);

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
//	::ShowWindow(g_hwnd_mb, SW_HIDE);
	int iResult = DialogBox(g_hinst, MAKEINTRESOURCE(IDD_APPABOUT), hwnd,(DLGPROC)AboutDialogProc);
//	::ShowWindow(g_hwnd_mb, SW_SHOW);
	freeze_display(0);
}

void pause_file()
{
	if (!is_connected) return;
	TBBUTTONINFO tbbi; 
	tbbi.cbSize = sizeof(tbbi); 
	tbbi.dwMask = TBIF_TEXT; 

	if (gf_term_get_option(term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		tbbi.pszText = _T("Resume"); 
	} else {
		gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
		tbbi.pszText = _T("Pause"); 
	}
	SendMessage(g_hwnd_menu1, TB_SETBUTTONINFO, IDM_FILE_PAUSE, (LPARAM)&tbbi); 

}

void set_svg_progressive()
{
	use_svg_prog = !use_svg_prog;
	if (use_svg_prog) {
		gf_cfg_set_key(user.config, "SAXLoader", "Progressive", "yes");
		gf_cfg_set_key(user.config, "SAXLoader", "MaxDuration", "30");
	} else {
		gf_cfg_set_key(user.config, "SAXLoader", "MaxDuration", "0");
		gf_cfg_set_key(user.config, "SAXLoader", "Progressive", "no");
	}
}

void do_copy_paste()
{
	if (!OpenClipboard(g_hwnd)) return;

	/*or we are editing text and clipboard is not empty*/
	if (IsClipboardFormatAvailable(CF_TEXT) && (gf_term_paste_text(term, NULL, 1)==GF_OK)) {
		HGLOBAL hglbCopy = GetClipboardData(CF_TEXT);
		if (hglbCopy) {
			char szString[1024];
			LPCTSTR paste_string = (LPCTSTR) GlobalLock(hglbCopy);
			CE_WideToChar((u16 *) paste_string, szString);
			gf_term_paste_text(term, szString, 0);
			GlobalUnlock(hglbCopy); 
		}
	}
	/*we have something to copy*/
	else if (gf_term_get_text_selection(term, 1)!=NULL) {
		u32 len;
		const char *text = gf_term_get_text_selection(term, 0);
		if (text && strlen(text)) {
			EmptyClipboard();
			len = strlen(text);

			HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(u16)); 
			LPCTSTR new_string = (LPCTSTR) GlobalLock(hglbCopy);
			CE_CharToWide((char*)text, (u16*)new_string); 
			GlobalUnlock(hglbCopy); 
			SetClipboardData(CF_TEXT, hglbCopy);
		}
	}
	CloseClipboard(); 
}

BOOL HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam)) {
	case IDM_FILE_OPEN:
		open_file(hwnd);
		break;
	case IDM_FILE_LOG_RTI:
		log_rti = !log_rti;
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
		show_status = !show_status;
		do_layout(1);
		break;
	case IDM_VIEW_CPU:
		view_cpu = !view_cpu;
		break;
	case IDM_VIEW_FORCEGL:
		force_2d_gl = !force_2d_gl;
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
		use_low_fps = !use_low_fps;
		gf_term_set_simulation_frame_rate(term, use_low_fps ? 15.0 : 30.0);
		break;
	case IDM_VIEW_DIRECT:
	{
		Bool drend = gf_term_get_option(term, GF_OPT_DIRECT_DRAW) ? 0 : 1;
		gf_term_set_option(term, GF_OPT_DIRECT_DRAW, drend);
		gf_cfg_set_key(user.config, "Compositor", "DirectDraw", drend ? "yes" : "no");
	}
		break;


	case IDM_MENU_SWITCH:
		menu_switched = !menu_switched;
		do_layout(0);
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
		playlist_navigation_on = !playlist_navigation_on;
		break;
	case IDM_VIEW_SVG_LOAD:
		set_svg_progressive();
		break;
	case IDM_ITEM_QUIT:
		DestroyWindow(hwnd);
		return FALSE;
	}
	return TRUE;
}

static BOOL OnMenuPopup(const HWND hWnd, const WPARAM wParam)
{
	if (menu_switched) {
		CheckMenuItem((HMENU)wParam, IDM_VIEW_STATUS, MF_BYCOMMAND| (show_status ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_VIEW_FORCEGL, MF_BYCOMMAND| (force_2d_gl ? MF_CHECKED : MF_UNCHECKED) );
		EnableMenuItem((HMENU)wParam, IDM_VIEW_CPU, MF_BYCOMMAND| (show_status ? MF_ENABLED : MF_GRAYED) );
		if (show_status) CheckMenuItem((HMENU)wParam, IDM_VIEW_CPU, MF_BYCOMMAND| (view_cpu ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem((HMENU)wParam, IDM_VIEW_LOW_RATE, MF_BYCOMMAND| (use_low_fps ? MF_CHECKED : MF_UNCHECKED) );

		EnableMenuItem((HMENU)wParam, IDM_VIEW_DIRECT, MF_BYCOMMAND| (!force_2d_gl ? MF_ENABLED : MF_GRAYED) );
		CheckMenuItem((HMENU)wParam, IDM_VIEW_DIRECT, MF_BYCOMMAND| (!force_2d_gl && gf_term_get_option(term, GF_OPT_DIRECT_DRAW) ? MF_CHECKED : MF_UNCHECKED) );

		CheckMenuItem((HMENU)wParam, IDM_VIEW_SVG_LOAD, MF_BYCOMMAND| (use_svg_prog ? MF_CHECKED : MF_UNCHECKED) );
		return TRUE;
	}

	u32 opt = gf_term_get_option(term, GF_OPT_ASPECT_RATIO);	
	CheckMenuItem((HMENU)wParam, IDS_CAP_DISABLE_PLAYLIST, MF_BYCOMMAND| (!playlist_navigation_on ? MF_CHECKED : MF_UNCHECKED) );
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_NONE, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_KEEP) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_FILL, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_FILL_SCREEN) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_4_3, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_4_3) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem((HMENU)wParam, IDM_VIEW_AR_16_9, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_16_9) ? MF_CHECKED : MF_UNCHECKED);

	EnableMenuItem((HMENU)wParam, IDM_FILE_PAUSE, MF_BYCOMMAND| (is_connected ? MF_ENABLED : MF_GRAYED) );

	if (/*we have something to copy*/
		(gf_term_get_text_selection(term, 1)!=NULL) 
		/*or we are editing text and clipboard is not empty*/
		|| (IsClipboardFormatAvailable(CF_TEXT) && (gf_term_paste_text(term, NULL, 1)==GF_OK))
	) {
		EnableMenuItem((HMENU)wParam, ID_FILE_CUT_PASTE, MF_BYCOMMAND| MF_ENABLED);
	} else {
		EnableMenuItem((HMENU)wParam, ID_FILE_CUT_PASTE, MF_BYCOMMAND| MF_GRAYED);
	}
	
	CheckMenuItem((HMENU)wParam, IDM_FILE_LOG_RTI, MF_BYCOMMAND| (log_rti) ? MF_CHECKED : MF_UNCHECKED);
	
	u32 type;
	if (!is_connected) type = 0;
	else type = gf_term_get_option(term, GF_OPT_NAVIGATION_TYPE);
	navigation_on = type ? 1 : 0;

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
		navigation_on = (mode==GF_NAVIGATE_NONE) ? 0 : 1;
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

    return TRUE;
}


BOOL CALLBACK MainWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
	{
		SHMENUBARINFO mbi;
		memset(&mbi, 0, sizeof(SHMENUBARINFO));
		mbi.cbSize = sizeof(SHMENUBARINFO);
		mbi.hwndParent = hwnd;
		mbi.hInstRes = g_hinst;
		mbi.nBmpId = 0;
		mbi.cBmpImages = 0;	
		
		mbi.nToolBarId = IDM_MAIN_MENU1;
		SHCreateMenuBar(&mbi);
		g_hwnd_menu1 = mbi.hwndMB;

		mbi.nToolBarId = IDM_MAIN_MENU2;
		SHCreateMenuBar(&mbi);
		g_hwnd_menu2 = mbi.hwndMB;

		g_hwnd_status = CreateWindow(TEXT("STATIC"), TEXT("Status"), WS_CHILD|WS_VISIBLE|SS_LEFT, 0, 0, disp_w, caption_h-1, hwnd, NULL, g_hinst, NULL);
		do_layout(1);
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
			if (wParam==VK_LEFT) { switch_playlist(1); break; }
			else if (wParam==VK_RIGHT) { switch_playlist(0); break; }
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
		freeze_display(0);
		break;
    case WM_KILLFOCUS:
		freeze_display(1);
		break;
    case WM_ACTIVATE:
        if (WA_INACTIVE != LOWORD(wParam)) {
			freeze_display(0);
			SetFocus(hwnd);
		} else {
			freeze_display(1);
		}
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

	ShowWindow(g_hwnd, CmdShow);

	refresh_recent_files();

	::SetTimer(g_hwnd, STATE_TIMER_ID, STATE_TIMER_DUR, NULL);
	do_layout(1);

	set_status("Loading Terminal");
	PostMessage(g_hwnd, WM_LOADTERM, 0, 0);
	set_status("Ready");
	return TRUE;
}

Bool initial_setup(const char *szExePath)
{
	char szPath[GF_MAX_PATH];
	TCHAR wzPath[GF_MAX_PATH];
	strcpy(szPath, szExePath);
	strcat(szPath, "GPAC.cfg");
	FILE *f = fopen(szPath, "wt");
	if (!f) return 0;
	fclose(f);

	user.config = gf_cfg_new(szExePath, "GPAC.cfg");
	
	gf_cfg_set_key(user.config, "General", "ModulesDirectory", szExePath);

	strcpy(szPath, szExePath);
	strcat(szPath, "cache");
	CE_CharToWide(szPath, (u16 *) wzPath);
	CreateDirectory(wzPath, NULL);
	gf_cfg_set_key(user.config, "General", "CacheDirectory", szPath);

	/*base rendering options*/
	gf_cfg_set_key(user.config, "Compositor", "Raster2D", "GPAC 2D Raster");
	/*enable UDP traffic autodetect*/
	gf_cfg_set_key(user.config, "Network", "AutoReconfigUDP", "yes");
	gf_cfg_set_key(user.config, "Network", "UDPTimeout", "10000");
	gf_cfg_set_key(user.config, "Network", "BufferLength", "3000");


	gf_cfg_set_key(user.config, "Audio", "ForceConfig", "yes");
	gf_cfg_set_key(user.config, "Audio", "NumBuffers", "2");
	gf_cfg_set_key(user.config, "Audio", "TotalDuration", "200");

	gf_cfg_set_key(user.config, "Video", "DriverName", "GAPI Video Output");

	/*FIXME - is this true on all WinCE systems??*/
	gf_cfg_set_key(user.config, "FontEngine", "FontDirectory", "\\Windows");

	sprintf((char *) szPath, "%sgpac.mp4", szExePath);
	gf_cfg_set_key(user.config, "General", "StartupFile", (const char *) szPath);


	/*save*/
	gf_cfg_del(user.config);
	user.config = gf_cfg_new(szExePath, "GPAC.cfg");
	if (!user.config) return 0;
	MessageBox(NULL, _T("Thank you for installing GPAC"), _T("Initial setup done"), MB_OK);
	return 1;
}



int WINAPI WinMain(HINSTANCE hInstance, 
				   HINSTANCE hPrevInstance, 
				   LPWSTR lpCmdLine, 
				   int nShowCmd 
) {
	MSG 	msg;
	TCHAR wzExePath[GF_MAX_PATH];
	char szExePath[GF_MAX_PATH];

	HWND 	hwndOld = NULL;	
	const char *str;
	Bool initial_launch = 0;

	if (hwndOld = FindWindow(_T("Osmophone"), NULL))
	{
		SetForegroundWindow(hwndOld);    
		return 0;
	}
	
	memset(&user, 0, sizeof(GF_User));
	term = NULL;

	GetModuleFileName(NULL, wzExePath, GF_MAX_PATH);
	CE_WideToChar((u16 *) wzExePath, szExePath);
	char *sep = strrchr(szExePath, '\\');
	sep[1] = 0;

	g_hinst = hInstance;
	
	menu_h = GetSystemMetrics(SM_CYMENU) - 6;
	caption_h = GetSystemMetrics(SM_CYCAPTION) - 3;
	screen_w = GetSystemMetrics(SM_CXSCREEN);
	screen_h = GetSystemMetrics(SM_CYSCREEN);
	disp_w = screen_w;
	disp_h = screen_h - menu_h /*- caption_h*/;

	
	TCHAR      szBuf[MAX_PATH];
	SystemParametersInfo(SPI_GETPLATFORMTYPE, MAX_PATH, szBuf, 0);

	if (! lstrcmp(szBuf, __TEXT("PocketPC"))) is_ppc = 1;
	else if (! lstrcmp(szBuf, __TEXT("Palm PC2"))) is_ppc = 1;


	user.config = gf_cfg_new(szExePath, "GPAC.cfg");
	if (!user.config) {
		initial_setup(szExePath);
		initial_launch = 1;
	}
	if (!user.config) {
		MessageBox(NULL, _T("Couldn't locate GPAC config file"), _T("Fatal Error"), MB_OK);
		return 0;
	}
	str = gf_cfg_get_key(user.config, "General", "ModulesDirectory");
	if (!str) {
		gf_cfg_del(user.config);
		MessageBox(NULL, _T("Couldn't locate GPAC plugins"), _T("Fatal Error"), MB_OK);
		return 0;
	}

	gf_sys_init();

	user.modules = gf_modules_new(str, user.config);
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
		u32 i;
		for (i=0; i<gf_modules_get_count(user.modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(user.modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			if (ifce) {
				ifce->CanHandleURL(ifce, "test.test");
				gf_modules_close_interface((GF_BaseInterface *)ifce);
			}
		}
	}

	str = gf_cfg_get_key(user.config, "General", "Loop");
	loop = (str && !stricmp(str, "yes")) ? 1 : 0;

	str = gf_cfg_get_key(user.config, "SAXLoader", "Progressive");
	use_svg_prog = (str && !strcmp(str, "yes")) ? 1 : 0;

	str = gf_cfg_get_key(user.config, "General", "RTIRefreshPeriod");
	if (str) {
		rti_update_time_ms = atoi(str);
	} else {
		gf_cfg_set_key(user.config, "General", "RTIRefreshPeriod", "200");
	}


	if (is_ppc) GXOpenInput();

	if (InitInstance(nShowCmd)) {
		SetForegroundWindow(g_hwnd);
		show_taskbar(0);

		force_2d_gl = gf_term_get_option(term, GF_OPT_USE_OPENGL);

		while (GetMessage(&msg, NULL, 0,0) == TRUE) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);

			if (playlist_act) {
				switch_playlist(playlist_act-1);
				playlist_act = 0;
			}
		}
		show_taskbar(1);
	}
	if (is_ppc) GXCloseInput();

	/*and destroy*/
	if (term) gf_term_del(term);
	if (user.modules) gf_modules_del(user.modules);
	if (user.config) gf_cfg_del(user.config);

	if (backlight_off) set_backlight_state(0);

	gf_sys_close();
	if (rti_file) fclose(rti_file);
	return 0;
}
