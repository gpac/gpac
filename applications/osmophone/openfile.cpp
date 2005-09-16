#include <windows.h>
#include <aygshell.h>

#include <gpac/tools.h>
#include "resource.h"

static HINSTANCE g_hInst = NULL;
static HWND g_hWndMenuBar;
static HWND hDirTxt;
static HWND hList;
static TCHAR w_current_dir[GF_MAX_PATH] = _T("\\");
static u8 current_dir[GF_MAX_PATH] = "\\";
static const char *extension_list = NULL;
static char *out_url = NULL;
Bool bViewUnknownTypes = 0;
Bool bPLMode = 0;


Bool enum_dirs(void *cbk, char *name, char *path)
{
	TCHAR w_name[GF_MAX_PATH], w_str_name[GF_MAX_PATH];

	CE_CharToWide(name, w_name);
	wcscpy(w_str_name, _T("+ "));
	wcscat(w_str_name, w_name);
    int iRes = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) w_str_name);
	SendMessage(hList, LB_SETITEMDATA, iRes, (LPARAM) 1);
	return 0;
}

Bool enum_files(void *cbk, char *name, char *path)
{
	TCHAR w_name[GF_MAX_PATH];

	if (!bViewUnknownTypes && extension_list) {
		char *ext = strrchr(name, '.');
		if (!ext || !strstr(extension_list, ext+1)) return 0;
	}
	CE_CharToWide(name, w_name);
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) w_name);
	return 0;
}


void set_directory(TCHAR *dir)
{
	SendMessage(hList, LB_RESETCONTENT, 0, 0);

	CE_WideToChar(dir, (char *) current_dir);
	wcscpy(w_current_dir, dir);
	SetWindowText(hDirTxt, w_current_dir);

	if (strcmp((const char *) current_dir, "\\")) {
		int iRes = SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)(LPCTSTR) _T("+ ..") );
		SendMessage(hList, LB_SETITEMDATA, iRes, (LPARAM) 1);
	}

	/*enum directories*/
	gf_enum_directory((const char *) current_dir, 1, enum_dirs, NULL);
	/*enum files*/
	gf_enum_directory((char *) current_dir, 0, enum_files, NULL);
    SendMessage(hList, LB_SETCURSEL, 0, 0);
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

	if (FALSE == SHInitDialog(&sid))
		return FALSE;
    
    ShowWindow(g_hWndMenuBar, SW_SHOW);

    hDirTxt = GetDlgItem(hWnd, IDC_DIRNAME);
    hList = GetDlgItem(hWnd, IDC_FILELIST);

	RECT rc;
	GetClientRect(hWnd, &rc);
	u32 caption_h = GetSystemMetrics(SM_CYCAPTION) - 3;
	MoveWindow(hDirTxt, 0, 0, rc.right - rc.left, caption_h, 1);
	MoveWindow(hList, 0, caption_h, rc.right - rc.left, rc.bottom - rc.top - caption_h, 1);
	set_directory(w_current_dir);
	return TRUE;
}

void process_list_change(HWND hWnd)
{
	TCHAR sTxt[GF_MAX_PATH];
    if (!SendMessage(hList, LB_GETSELCOUNT, 0, 0)) return;

    u32 i = SendMessage(hList, LB_GETCURSEL, 0, 0);
	SendMessage(hList, LB_GETTEXT, i, (LPARAM)(LPCTSTR) sTxt);

	DWORD param = SendMessage(hList, LB_GETITEMDATA, i, 0);
	if (param==1) {
		if (!wcscmp(sTxt, _T("+ ..") ) ) {
			current_dir[strlen((const char *) current_dir)-1] = 0;
			char *b = strrchr((const char *) current_dir, '\\');
			if (b) b[1] = 0;
			else b[0] = '\\';

			CE_CharToWide((char *) current_dir, w_current_dir);
		} else {
			wcscat(w_current_dir, sTxt+2);
			wcscat(w_current_dir, _T("\\"));
			CE_WideToChar(w_current_dir, (char *) current_dir);
		}
		set_directory(w_current_dir);
	} else {
		char szTxt[1024];
		CE_WideToChar(sTxt, (char *) szTxt);
		strcpy((char *) out_url, (const char *) current_dir);
		strcat(out_url, szTxt);
		EndDialog(hWnd, 1);
	}
}

static BOOL OnMenuPopup(const HWND hWnd, const WPARAM wParam)
{
	if (!extension_list) {
        EnableMenuItem((HMENU)wParam, IDM_OF_VIEW_ALL, MF_BYCOMMAND|MF_GRAYED);
	} else {
        EnableMenuItem((HMENU)wParam, IDM_OF_VIEW_ALL, MF_BYCOMMAND|MF_ENABLED);
		if (bViewUnknownTypes) {
			CheckMenuItem((HMENU)wParam, IDM_OF_VIEW_ALL, MF_BYCOMMAND|MF_CHECKED);
		} else {
			CheckMenuItem((HMENU)wParam, IDM_OF_VIEW_ALL, MF_BYCOMMAND|MF_UNCHECKED);
		}
	}
	CheckMenuItem((HMENU)wParam, IDM_OF_PLAYLIST, MF_BYCOMMAND| (bPLMode ? MF_CHECKED : MF_UNCHECKED) );
	
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

    case WM_INITMENUPOPUP:
        OnMenuPopup(hWnd, wParam);
        break;

    case WM_COMMAND:
        switch LOWORD(wParam) {
        case IDOK:
            process_list_change(hWnd);
            break;
        case IDCANCEL:
            EndDialog(hWnd, 0);
            break;
		case IDM_OF_VIEW_ALL:
			bViewUnknownTypes = !bViewUnknownTypes;
			set_directory(w_current_dir);
			break;
		case IDM_OF_PLAYLIST:
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

Bool gf_file_dialog(HINSTANCE inst, HWND parent, char *url, const char *ext_list)
{
	extension_list = ext_list;
	out_url = url;
	g_hInst = inst;
	int iResult = DialogBox(inst, MAKEINTRESOURCE(IDD_FILEDIALOG), parent,(DLGPROC)FileDialogProc);
	if (iResult>0) return 1;
	return 0;
}

