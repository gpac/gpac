#include <windows.h>

#ifdef _WIN32_WCE

#include <aygshell.h>

#include <gpac/config_file.h>
#include "resource.h"

static HINSTANCE g_hInst = NULL;
static HMENU g_hMenuView;
static HWND g_hWndMenuBar;
static HWND hDirTxt;
static HWND the_wnd;
static HWND hList;
static TCHAR w_current_dir[GF_MAX_PATH] = _T("\\");
static u8 current_dir[GF_MAX_PATH] = "\\";
static const char *extension_list = NULL;
static char *out_url = NULL;
Bool bViewUnknownTypes = GF_FALSE;
Bool playlist_mode = GF_FALSE;
GF_Config *cfg;


static void refresh_menu_states()
{
	if (playlist_mode) {
		EnableMenuItem(g_hMenuView, IDM_OF_PL_UP, MF_BYCOMMAND|MF_ENABLED);
		EnableMenuItem(g_hMenuView, IDM_OF_PL_DOWN, MF_BYCOMMAND|MF_ENABLED );
	} else {
		EnableMenuItem(g_hMenuView, IDM_OF_VIEW_ALL, MF_BYCOMMAND| (extension_list ? MF_ENABLED : MF_GRAYED) );
		CheckMenuItem(g_hMenuView, IDM_OF_VIEW_ALL, MF_BYCOMMAND| (bViewUnknownTypes ? MF_CHECKED : MF_UNCHECKED) );
	}
	CheckMenuItem(g_hMenuView, IDM_OF_PLAYLIST, MF_BYCOMMAND| (playlist_mode ? MF_CHECKED : MF_UNCHECKED) );
}

static void switch_menu_pl()
{
	DeleteMenu(g_hMenuView, IDM_OF_VIEW_ALL, MF_BYCOMMAND);
	DeleteMenu(g_hMenuView, IDM_OF_PL_UP, MF_BYCOMMAND);
	DeleteMenu(g_hMenuView, IDM_OF_PL_DOWN, MF_BYCOMMAND);
	DeleteMenu(g_hMenuView, IDM_OF_PL_CLEAR, MF_BYCOMMAND);

	if (playlist_mode) {
		InsertMenu(g_hMenuView, 0, MF_BYPOSITION, IDM_OF_PL_CLEAR, _T("Clear"));
		InsertMenu(g_hMenuView, 0, MF_BYPOSITION, IDM_OF_PL_DOWN, _T("Move Down") );
		InsertMenu(g_hMenuView, 0, MF_BYPOSITION, IDM_OF_PL_UP, _T("Move Up") );
	} else {
		InsertMenu(g_hMenuView, 0, MF_BYPOSITION, IDM_OF_VIEW_ALL, _T("All Unknown Files") );
	}
	TBBUTTONINFO tbbi;
	tbbi.cbSize = sizeof(tbbi);
	tbbi.dwMask = TBIF_TEXT;
	tbbi.pszText = playlist_mode ? _T("Remove") : _T("Add");
	SendMessage(g_hWndMenuBar, TB_SETBUTTONINFO, IDM_OF_PL_ACT, (LPARAM)&tbbi);
	refresh_menu_states();
}


Bool enum_dirs(void *cbk, char *name, char *path)
{
	TCHAR w_name[GF_MAX_PATH], w_str_name[GF_MAX_PATH];

	CE_CharToWide(name, (u16 *) w_name);
	wcscpy(w_str_name, _T("+ "));
	wcscat(w_str_name, w_name);
	int iRes = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) w_str_name);
	SendMessage(hList, LB_SETITEMDATA, iRes, (LPARAM) 1);
	return GF_FALSE;
}

Bool enum_files(void *cbk, char *name, char *path)
{
	TCHAR w_name[GF_MAX_PATH];

	if (!bViewUnknownTypes && extension_list) {
		char *ext = strrchr(name, '.');
		if (!ext || !strstr(extension_list, ext+1)) return GF_FALSE;
	}
	CE_CharToWide(name, (u16 *) w_name);
	SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) w_name);
	return GF_FALSE;
}


void set_directory(TCHAR *dir)
{
	SendMessage(hList, LB_RESETCONTENT, 0, 0);

	CE_WideToChar((u16 *) dir, (char *) current_dir);
	wcscpy(w_current_dir, dir);
	SetWindowText(hDirTxt, w_current_dir);

	if (strcmp((const char *) current_dir, "\\")) {
		int iRes = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) _T("+ ..") );
		SendMessage(hList, LB_SETITEMDATA, iRes, (LPARAM) 1);
	}

	/*enum directories*/
	gf_enum_directory((const char *) current_dir, GF_TRUE, enum_dirs, NULL, NULL);
	/*enum files*/
	gf_enum_directory((char *) current_dir, GF_FALSE, enum_files, NULL, NULL);
	SendMessage(hList, LB_SETCURSEL, 0, 0);
	SetFocus(hList);
}



void refresh_playlist()
{
	TCHAR w_name[GF_MAX_PATH];
	u32 count, i;

	SetWindowText(hDirTxt, _T("Playlist"));
	SendMessage(hList, LB_RESETCONTENT, 0, 0);

	count = gf_cfg_get_key_count(cfg, "Playlist");
	for (i=0; i<count; i++) {
		const char *file = gf_cfg_get_key_name(cfg, "Playlist", i);
		char *name = strrchr(file, '\\');
		if (!name) name = strrchr(file, '/');
		CE_CharToWide(name ? name+1 : (char *) file, (u16 *) w_name);
		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) w_name);
	}
	i = 0;
	const char *ple = gf_cfg_get_key(cfg, "General", "PLEntry");
	i = ple ? atoi(ple) : 0;
	if (i>=count) i=0;
	SendMessage(hList, LB_SETCURSEL, i, 0);
	SetFocus(hList);
}

void playlist_act(u32 act_type)
{
	u32 idx, count;
	char entry[MAX_PATH];
	const char *url;

	/*reset all*/
	if (act_type == 3) {
		count = gf_cfg_get_key_count(cfg, "Playlist");
		while (count) {
			url = gf_cfg_get_key_name(cfg, "Playlist", 0);
			gf_cfg_set_key(cfg, "Playlist", url, NULL);
			count--;
		}
		refresh_playlist();
		return;
	}
	count = SendMessage(hList, LB_GETSELCOUNT, 0, 0);
	if (!count) return;
	idx = SendMessage(hList, LB_GETCURSEL, 0, 0);

	if ((act_type==1) && !idx) return;
	else if ((act_type==2) && (idx+1==count)) return;

	url = gf_cfg_get_key_name(cfg, "Playlist", idx);
	if (!url) return;
	strcpy(entry, url);
	/*remove from playlist*/
	gf_cfg_set_key(cfg, "Playlist", url, NULL);
	switch (act_type) {
	/*remove*/
	case 0:
		if (idx+1==count) idx--;
		break;
	/*up*/
	case 1:
		gf_cfg_insert_key(cfg, "Playlist", entry, "", idx-1);
		idx--;
		break;
	/*down*/
	case 2:
		gf_cfg_insert_key(cfg, "Playlist", entry, "", idx+1);
		idx++;
		break;
	}
	refresh_playlist();
	SendMessage(hList, LB_SETCURSEL, idx, 0);
	SetFocus(hList);
}

Bool add_files(void *cbk, char *name, char *path)
{
	if (!bViewUnknownTypes && extension_list) {
		char *ext = strrchr(name, '.');
		if (!ext || !strstr(extension_list, ext+1)) return GF_FALSE;
	}
	gf_cfg_set_key(cfg, "Playlist", path, "");
	return GF_FALSE;
}

void process_list_change(HWND hWnd, Bool add_to_pl)
{
	TCHAR sTxt[GF_MAX_PATH];
	if (!SendMessage(hList, LB_GETSELCOUNT, 0, 0)) return;

	u32 idx = SendMessage(hList, LB_GETCURSEL, 0, 0);
	SendMessage(hList, LB_GETTEXT, idx, (LPARAM)(LPCTSTR) sTxt);

	DWORD param = SendMessage(hList, LB_GETITEMDATA, idx, 0);
	if (param==1) {
		if (!wcscmp(sTxt, _T("+ ..") ) ) {
			if (add_to_pl) return;
			current_dir[strlen((const char *) current_dir)-1] = 0;
			char *b = strrchr((const char *) current_dir, '\\');
			if (b) b[1] = 0;
			else b[0] = '\\';
			CE_CharToWide((char *) current_dir, (u16 *) w_current_dir);
			set_directory(w_current_dir);
		} else {
			if (add_to_pl) {
				char dir[MAX_PATH];
				TCHAR wdir[MAX_PATH];
				wcscpy(wdir, w_current_dir);
				wcscat(wdir, sTxt+2);
				wcscat(wdir, _T("\\"));
				CE_WideToChar((u16 *) wdir, (char *) dir);
				gf_enum_directory(dir, GF_FALSE, add_files, NULL, NULL);
			} else {
				wcscat(w_current_dir, sTxt+2);
				wcscat(w_current_dir, _T("\\"));
				CE_WideToChar((u16 *) w_current_dir, (char *) current_dir);
				set_directory(w_current_dir);
			}
		}
	} else {
		char szTxt[1024];
		CE_WideToChar((u16 *) sTxt, (char *) szTxt);
		strcpy((char *) out_url, (const char *) current_dir);
		strcat(out_url, szTxt);
		if (add_to_pl) {
			gf_cfg_set_key(cfg, "Playlist", out_url, "");
			strcpy(out_url, "");
		} else {
			if (playlist_mode) {
				const char *file;
				char szPLE[20];
				sprintf(szPLE, "%d", idx);
				gf_cfg_set_key(cfg, "General", "PLEntry", szPLE);
				file = gf_cfg_get_key_name(cfg, "Playlist", idx);
				strcpy(out_url, file);
			}
			gf_cfg_set_key(cfg, "General", "LastWorkingDir", (const char *) current_dir);
			EndDialog(hWnd, 1);
		}
	}
}

BOOL InitFileDialog(const HWND hWnd)
{
	TCHAR           psz[80];
	ZeroMemory(psz, sizeof(psz));
	SHINITDLGINFO sid;
	ZeroMemory(&sid, sizeof(sid));
	sid.dwMask  = SHIDIM_FLAGS;
	sid.dwFlags = SHIDIF_SIZEDLGFULLSCREEN;
	sid.hDlg    = hWnd;

	if (FALSE == SHInitDialog(&sid))
		return FALSE;

	SHMENUBARINFO mbi;
	ZeroMemory(&mbi, sizeof(SHMENUBARINFO));
	mbi.cbSize      = sizeof(SHMENUBARINFO);
	mbi.hwndParent  = hWnd;
	mbi.nToolBarId  = IDR_MENU_OPEN;
	mbi.hInstRes    = g_hInst;

	if (FALSE == SHCreateMenuBar(&mbi))
	{
		return FALSE;
	}
	g_hWndMenuBar = mbi.hwndMB;

	ShowWindow(g_hWndMenuBar, SW_SHOW);

	the_wnd = hWnd;

	hDirTxt = GetDlgItem(hWnd, IDC_DIRNAME);
	hList = GetDlgItem(hWnd, IDC_FILELIST);
	g_hMenuView = (HMENU)SendMessage(g_hWndMenuBar, SHCMBM_GETSUBMENU, 0, ID_OF_VIEW);

	RECT rc;
	GetClientRect(hWnd, &rc);
	u32 caption_h = GetSystemMetrics(SM_CYCAPTION) - 3;
	MoveWindow(hDirTxt, 0, 0, rc.right - rc.left, caption_h, 1);
	MoveWindow(hList, 0, caption_h, rc.right - rc.left, rc.bottom - rc.top - caption_h, 1);

	if (playlist_mode) {
		refresh_playlist();
	} else {
		if (!strcmp((const char *) current_dir, "\\")) {
			char *opt = (char *) gf_cfg_get_key(cfg, "General", "LastWorkingDir");
			if (opt) CE_CharToWide(opt, (u16 *) w_current_dir);
		}
		set_directory(w_current_dir);
	}
	switch_menu_pl();
	return TRUE;
}

BOOL CALLBACK FileDialogProc(const HWND hWnd, const UINT Msg, const WPARAM wParam, const LPARAM lParam)
{
	BOOL bProcessedMsg = TRUE;

	switch (Msg) {
	case WM_INITDIALOG:
		if (FALSE == InitFileDialog(hWnd))
			EndDialog(hWnd, -1);
		break;

	case WM_ACTIVATE:
		if (WA_INACTIVE != LOWORD(wParam)) SetFocus(hWnd);
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_FILELIST) {
			if (HIWORD(wParam) == LBN_DBLCLK) {
				process_list_change(hWnd, GF_FALSE);
			} else {
				bProcessedMsg = FALSE;
			}
		} else {
			switch (LOWORD(wParam)) {
			case IDOK:
				process_list_change(hWnd, GF_FALSE);
				break;
			case IDCANCEL:
				EndDialog(hWnd, 0);
				break;
			case IDM_OF_VIEW_ALL:
				bViewUnknownTypes = (Bool) !bViewUnknownTypes;
				refresh_menu_states();
				set_directory(w_current_dir);
				break;
			case IDM_OF_PLAYLIST:
				playlist_mode = (Bool) !playlist_mode;
				if (playlist_mode) refresh_playlist();
				else set_directory(w_current_dir);
				switch_menu_pl();
				break;
			case IDM_OF_PL_ACT:
				if (playlist_mode) {
					playlist_act(0);
				} else {
					process_list_change(hWnd, GF_TRUE);
				}
				break;
			case IDM_OF_PL_UP:
				playlist_act(1);
				break;
			case IDM_OF_PL_DOWN:
				playlist_act(2);
				break;
			case IDM_OF_PL_CLEAR:
				playlist_act(3);
				break;
			default:
				bProcessedMsg = FALSE;
				break;
			}
		}
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_LEFT:
		case '1':
			playlist_act(1);
			break;
		case VK_RIGHT:
		case '2':
			playlist_act(2);
			break;
		default:
			bProcessedMsg = FALSE;
			break;
		}
		break;

	default:
		bProcessedMsg = FALSE;
	}

	return bProcessedMsg;
}

Bool gf_file_dialog(HINSTANCE inst, HWND parent, char *url, const char *ext_list, GF_Config *gpac_cfg)
{
	extension_list = ext_list;
	out_url = url;
	g_hInst = inst;
	cfg = gpac_cfg;
	int iResult = DialogBox(inst, MAKEINTRESOURCE(IDD_FILEDIALOG), parent,(DLGPROC)FileDialogProc);
	if (iResult>0) return GF_TRUE;
	return GF_FALSE;
}


#endif

