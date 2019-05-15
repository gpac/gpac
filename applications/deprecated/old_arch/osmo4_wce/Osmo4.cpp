// Osmo4.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Osmo4.h"

#include <gpac/options.h>
#include <gpac/modules/service.h>
#include "MainFrm.h"
#include "OpenDlg.h"
#include "Options.h"
#include <gx.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// COsmo4

BEGIN_MESSAGE_MAP(COsmo4, CWinApp)
	//{{AFX_MSG_MAP(COsmo4)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(IDD_CONFIGURE, OnConfigure)
	ON_COMMAND(ID_OPEN_FILE, OnOpenFile)
	ON_COMMAND(ID_OPEN_URL, OnOpenUrl)
	ON_COMMAND(ID_SHORTCUTS, OnShortcuts)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()






Bool Osmo4CE_EventProc(void *priv, GF_Event *event)
{
	u32 dur;
	COsmo4 *app = (COsmo4 *) priv;
	CMainFrame *pFrame = (CMainFrame *) app->m_pMainWnd;
	/*shutdown*/
	if (!pFrame) return 0;

	switch (event->type) {
	case GF_EVENT_MESSAGE:
		if (event->message.error!=GF_OK) {
			if (event->message.error<GF_OK) {
				pFrame->console_err = event->message.error;
				pFrame->console_message = event->message.message;
				pFrame->PostMessage(WM_CONSOLEMSG, 0, 0);
			}
			return 0;
		}
		if (1) return 0;
		/*process user message*/
		pFrame->console_err = GF_OK;
		pFrame->console_message = event->message.message;
		pFrame->PostMessage(WM_CONSOLEMSG, 0, 0);
		break;
	case GF_EVENT_SIZE:
		break;
	case GF_EVENT_SCENE_SIZE:
		app->m_scene_width = event->size.width;
		app->m_scene_height = event->size.height;
		if (!pFrame->m_full_screen)
			pFrame->PostMessage(WM_SETSIZE, event->size.width, event->size.height);
		break;
	case GF_EVENT_CONNECT:
		app->m_open = event->connect.is_connected;
		break;
	case GF_EVENT_DURATION:
		dur = (u32) (1000 * event->duration.duration);
		if (dur<2000) dur = 0;
		app->m_duration = dur;
		app->m_can_seek = event->duration.can_seek && dur;
		pFrame->m_progBar.m_range_invalidated = 1;
		/*by default, don't display timing if not seekable and vice-versa*/
		if (app->m_can_seek != pFrame->m_view_timing) {
			pFrame->m_view_timing = app->m_can_seek;
			if (!pFrame->m_full_screen)
				pFrame->PostMessage(WM_SETSIZE, 0, 0);
		}
		break;
	case GF_EVENT_NAVIGATE:
		/*store URL since it may be destroyed, and post message*/
		app->m_navigate_url = event->navigate.to_url;
		pFrame->PostMessage(WM_NAVIGATE, NULL, NULL);
		return 1;
	case GF_EVENT_QUIT:
		pFrame->PostMessage(WM_CLOSE, 0L, 0L);
		break;
	/*ipaq keys*/
	case GF_EVENT_KEYDOWN:
		switch (event->key.key_code) {
		case GF_KEY_F1:
			pFrame->PostMessage(WM_COMMAND, ID_FILE_OPEN);
			break;
		case GF_KEY_F2:
			pFrame->PostMessage(WM_QUIT);
			break;
		case GF_KEY_F3:
			pFrame->PostMessage(WM_COMMAND, ID_FILE_RESTART);
			break;
		case GF_KEY_F5:
			pFrame->PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
			break;
		case GF_KEY_ENTER:
			pFrame->PostMessage(WM_COMMAND, ID_FILE_PAUSE);
			break;
		case GF_KEY_LEFT:
			if (app->m_duration>=2000) {
				s32 res = gf_term_get_time_in_ms(app->m_term) - 5*app->m_duration/100;
				if (res<0) res=0;
				gf_term_play_from_time(app->m_term, res, 0);
			}
			break;
		case GF_KEY_RIGHT:
			if (app->m_duration>=2000) {
				u32 res = gf_term_get_time_in_ms(app->m_term) + 5*app->m_duration/100;
				if (res>=app->m_duration) res = 0;
				gf_term_play_from_time(app->m_term, res, 0);
			}
			break;
		case GF_KEY_UP:
			if (app->m_duration>=2000) pFrame->PostMessage(WM_COMMAND, ID_FILE_STEP);
			break;
		case GF_KEY_DOWN:
			gf_term_set_option(app->m_term, GF_OPT_REFRESH, 0);
			break;
		}
		break;
	case GF_EVENT_DBLCLICK:
		pFrame->PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
		return 0;
	}

	return 0;
}

COsmo4::COsmo4()
	: CWinApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
	m_logs = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only COsmo4 object

COsmo4 theApp;

static void osmo4_do_log(void *cbk, GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list list)
{
	FILE *logs = (FILE *) cbk;
	if (logs) {
		vfprintf(logs, fmt, list);
		fflush(logs);
	}
}

void COsmo4::EnableLogs(Bool turn_on)
{
	if (turn_on) {
		const char *filename = gf_cfg_get_key(m_user.config, "General", "LogFile");
		if (!filename) {
			gf_cfg_set_key(m_user.config, "General", "LogFile", "\\gpac_logs.txt");
			filename = "\\gpac_logs.txt";
		}
		m_logs = gf_fopen(filename, "wt");
		if (!m_logs) {
			MessageBox(NULL, _T("Couldn't open log file on file system"), _T("Disabling logs"), MB_OK);
			turn_on = 0;
		} else {
			gf_log_set_tools_levels("network:rtp:sync:codec:media@debug");
			gf_log_set_callback(m_logs, osmo4_do_log);
			gf_cfg_set_key(m_user.config, "General", "Logs", "network:rtp:sync:codec:media@debug");
		}
	}
	if (!turn_on) {
		if (m_logs) {
			gf_fclose(m_logs);
			m_logs = 0;
		}
		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_NONE);
		gf_log_set_callback(NULL, NULL);
		gf_cfg_set_key(m_user.config, "General", "Logs", "all@quiet");
	}
}

/////////////////////////////////////////////////////////////////////////////
// COsmo4 initialization

BOOL COsmo4::InitInstance()
{
	Bool first_load = 0;
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	gf_sys_init(GF_MemTrackerNone);

	SetRegistryKey(_T("GPAC"));

	m_prev_batt_bl = m_prev_ac_bl = 0;

	m_screen_width = GetSystemMetrics(SM_CXSCREEN);
	m_screen_height = GetSystemMetrics(SM_CYSCREEN);
	m_menu_height = GetSystemMetrics(SM_CYMENU);
	m_scene_width = m_scene_height = 0;

	CMainFrame* pFrame = new CMainFrame;
	m_pMainWnd = pFrame;

	pFrame->LoadFrame(IDR_MAINFRAME, WS_VISIBLE, NULL, NULL);

	pFrame->ShowWindow(m_nCmdShow);
	pFrame->UpdateWindow();

	TCHAR w_config_path[MAX_PATH];
	char config_path[MAX_PATH];
	GetModuleFileName(NULL, w_config_path, MAX_PATH);
	CE_WideToChar((u16 *) w_config_path, (char *) config_path);

	while (config_path[strlen((char *) config_path)-1] != '\\') config_path[strlen((char *) config_path)-1] = 0;

	/*setup user*/
	memset(&m_user, 0, sizeof(GF_User));

	/*init config and plugins*/
	m_user.config = gf_cfg_init(NULL, &first_load);
	if (!m_user.config) {
		MessageBox(NULL, _T("GPAC Configuration file not found"), _T("Fatal Error"), MB_OK);
		m_pMainWnd->PostMessage(WM_CLOSE);
	}

	const char *str = gf_cfg_get_key(m_user.config, "General", "Logs");
	EnableLogs((str && strcmp(str, "all@quiet") ) ? 1 : 0);

	if (first_load) {
		/*first launch, register all files ext*/
		u32 i;
		for (i=0; i<gf_modules_get_count(m_user.modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(m_user.modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			if (ifce) {
				ifce->CanHandleURL(ifce, "test.test");
				gf_modules_close_interface((GF_BaseInterface *)ifce);
			}
		}
		::MessageBox(NULL, _T("Osmo4/GPAC Setup complete"), _T("Initial launch"), MB_OK);
	}


	str = gf_cfg_get_key(m_user.config, "Core", "ModulesDirectory");
	m_user.modules = gf_modules_new(str, m_user.config);
	if (!m_user.modules || ! gf_modules_get_count(m_user.modules) ) {
		MessageBox(NULL, _T("No plugins available - system cannot work"), _T("Fatal Error"), MB_OK);
		m_pMainWnd->PostMessage(WM_QUIT);
		return FALSE;
	}

	m_user.config = m_user.config;
	m_user.modules = m_user.modules;
	m_user.EventProc = Osmo4CE_EventProc;
	m_user.opaque = this;
	m_user.os_window_handler = pFrame->m_wndView.m_hWnd;


	m_term = gf_term_new(&m_user);
	if (! m_term) {
		MessageBox(NULL, _T("Cannot load MPEG-4 Terminal"), _T("Fatal Error"), MB_OK);
		m_pMainWnd->PostMessage(WM_QUIT);
	}

	m_stopped = 0;
	m_open = 0;
	m_can_seek = 0;
	m_DoResume = 0;
	SetOptions();
	pFrame->SendMessage(WM_SETSIZE, 0, 0);
	ShowTaskBar(0);

	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	if (! cmdInfo.m_strFileName.IsEmpty()) {
		m_filename = cmdInfo.m_strFileName;
		m_pMainWnd->PostMessage(WM_OPENURL);
	} else {
		str = gf_cfg_get_key(m_user.config, "General", "StartupFile");
		if (str) gf_term_connect(m_term, str);
	}
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// COsmo4 message handlers





/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	CStatic m_AbtTxt;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	virtual BOOL OnInitDialog();		// Added for WCE apps
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	DDX_Control(pDX, IDC_ABT_TEXT, m_AbtTxt);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COsmo4 commands
// Added for WCE apps

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString str = _T("Osmo4 Player\nGPAC V");
	str += _T(GPAC_VERSION);
	str += _T(" (build");
	str += _T(GPAC_BUILD_NUMBER);
	str += _T(")");
	m_AbtTxt.SetWindowText(str);
	return TRUE;
}



void COsmo4::OnAppAbout()
{
	CAboutDlg aboutDlg;
	ShowTaskBar(1);
	aboutDlg.DoModal();
	ShowTaskBar(0);
}


int COsmo4::ExitInstance()
{
	gf_term_del(m_term);
	gf_modules_del(m_user.modules);
	gf_cfg_del(m_user.config);
	ShowTaskBar(1);
	gf_sys_close();
	if (m_logs) gf_fclose(m_logs);
	return CWinApp::ExitInstance();
}

void COsmo4::SetOptions()
{
	const char *sOpt = gf_cfg_get_key(m_user.config, "General", "Loop");
	m_Loop = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "FillScreen");
	m_fit_screen = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "DisableBackLight");
	m_disable_backlight = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	gf_term_set_option(m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);
	sOpt = gf_cfg_get_key(m_user.config, "General", "NoMIMETypeFetch");
	m_no_mime_fetch = (!sOpt || !stricmp(sOpt, "yes")) ? 1 : 0;

	//gf_term_set_option(m_term, GF_OPT_AUDIO_VOLUME, 100);
}

void COsmo4::Pause()
{
	if (!m_open) return;

	if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
		gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		SetBacklightState(0);
	} else {
		gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
		SetBacklightState(1);
	}
	((CMainFrame*)m_pMainWnd)->SetPauseButton();
}


void COsmo4::OnConfigure()
{
	COptions dlg;

	ShowTaskBar(1);
	dlg.DoModal();
	ShowTaskBar(0);
}

void COsmo4::ShowTaskBar(Bool showIt, Bool pause_only)
{
	if (showIt) {
		m_DoResume = 0;
		if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
			m_DoResume = 1;
			gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		}
		gf_term_set_option(m_term, GF_OPT_FREEZE_DISPLAY, 1);
		if (!pause_only) {
			SHFullScreen(GetForegroundWindow(), SHFS_HIDESTARTICON | SHFS_SHOWTASKBAR| SHFS_SHOWSIPBUTTON);
			::ShowWindow(::FindWindow(_T("HHTaskbar"),NULL), SW_SHOWNA);
		}
		SetBacklightState(0);
	} else {
		if (!pause_only) {
			SHFullScreen(GetForegroundWindow(), SHFS_HIDESTARTICON | SHFS_HIDETASKBAR| SHFS_HIDESIPBUTTON);
			::ShowWindow(::FindWindow(_T("HHTaskbar"),NULL), SW_HIDE);
		}
		gf_term_set_option(m_term, GF_OPT_FREEZE_DISPLAY, 0);
		gf_term_set_option(m_term, GF_OPT_REFRESH, 0);
		if (m_DoResume) {
			gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
			SetBacklightState(1);
			m_DoResume = 0;
		}
		m_pMainWnd->SetFocus();
	}
}


CString COsmo4::GetFileFilter()
{
	u32 keyCount, i;
	CString sFiles;
	CString sExts;
	CString supportedFiles;

	return CString("All Files |*.*|");

	supportedFiles = "All Known Files|*.m3u;*.pls";

	sExts = "";
	sFiles = "";
	keyCount = gf_cfg_get_key_count(m_user.config, "MimeTypes");
	for (i=0; i<keyCount; i++) {
		const char *sMime;
		Bool first;
		char *sKey;
		char szKeyList[1000], sDesc[1000];
		short swKeyList[1000], swDesc[1000];
		sMime = gf_cfg_get_key_name(m_user.config, "MimeTypes", i);
		if (!sMime) continue;
		CString sOpt;
		const char *opt = gf_cfg_get_key(m_user.config, "MimeTypes", sMime);
		/*remove #include <gpac/options.h>
		name*/
		strcpy(szKeyList, opt+1);
		sKey = strrchr(szKeyList, '\"');
		if (!sKey) continue;
		sKey[0] = 0;
		/*get description*/
		sKey = strrchr(szKeyList, '\"');
		if (!sKey) continue;
		strcpy(sDesc, sKey+1);
		sKey[0] = 0;
		sKey = strrchr(szKeyList, '\"');
		if (!sKey) continue;
		sKey[0] = 0;

		CE_CharToWide(sDesc, (unsigned short *)swDesc);
		CE_CharToWide(szKeyList, (unsigned short *)swKeyList);

		/*if same description for # mime types skip (means an old mime syntax)*/
		if (sFiles.Find((LPCTSTR) swDesc)>=0) continue;
		/*if same extensions for # mime types skip (don't polluate the file list)*/
		if (sExts.Find((LPCTSTR) swKeyList)>=0) continue;

		sExts += (LPCTSTR) swKeyList;
		sExts += " ";
		sFiles += (LPCTSTR) swDesc;
		sFiles += "|";

		first = 1;

		sOpt = CString(szKeyList);
		while (1) {

			int pos = sOpt.Find(' ');
			CString ext = (pos==-1) ? sOpt : sOpt.Left(pos);
			/*WATCHOUT: we do have some "double" ext , eg .wrl.gz - these are NOT supported by windows*/
			if (ext.Find(_T("."))<0) {
				if (!first) {
					sFiles += ";";
				} else {
					first = 0;
				}
				sFiles += "*.";
				sFiles += ext;

				CString sext = ext;
				sext += ";";
				if (supportedFiles.Find(sext)<0) {
					supportedFiles += ";*.";
					supportedFiles += ext;
				}
			}

			if (sOpt==ext) break;
			CString rem;
			rem.Format(_T("%s "), (LPCTSTR) ext);
			sOpt.Replace((LPCTSTR) rem, _T(""));
		}
		sFiles += "|";
	}
	supportedFiles += "|";
	supportedFiles += sFiles;
	//supportedFiles += "M3U Playlists|*.m3u|ShoutCast Playlists|*.pls|All Files |*.*|";
	supportedFiles += "All Files |*.*|";
	return supportedFiles;
}

void COsmo4::OnOpenFile()
{
	Bool res;
	CFileDialog fd(TRUE,NULL,_T("\\"),OFN_HIDEREADONLY, GetFileFilter());

	ShowTaskBar(1);
	res = 0;
	if (fd.DoModal()==IDOK) {
		res = 1;
		m_filename = fd.GetPathName();
		m_DoResume = 0;/*done by term*/
	}
	ShowTaskBar(0);
	if (res) m_pMainWnd->PostMessage(WM_OPENURL);
}

void COsmo4::OnOpenUrl()
{
	OpenDlg dlg;
	Bool res;
	ShowTaskBar(1, 1);
	res = 0;
	if (dlg.DoModal() == IDOK) {
		res = 1;
		m_DoResume = 0;/*done by term*/
	}
	ShowTaskBar(0, 1);
	if (res) m_pMainWnd->PostMessage(WM_OPENURL);
}

void COsmo4::SetBacklightState(Bool disable)
{
	HKEY hKey = 0;
	DWORD dwSize;
	DWORD dwValue;
	HANDLE hBL;

	if (!m_disable_backlight) return;

	if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, 0, &hKey ) != ERROR_SUCCESS) return;

	if (disable) {
		dwSize = 4;
		RegQueryValueEx(hKey, _T("BatteryTimeout"), NULL, NULL,(unsigned char*) &m_prev_batt_bl, &dwSize);
		dwSize = 4;
		RegQueryValueEx(hKey, _T("ACTimeout"), NULL, NULL, (unsigned char*) &m_prev_ac_bl,&dwSize);
		dwSize = 4;
		dwValue = 0xefff ;
		RegSetValueEx(hKey, _T("BatteryTimeout"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
		dwSize = 4;
		dwValue = 0xefff ;
		RegSetValueEx( hKey, _T("ACTimeout"), NULL, REG_DWORD, (unsigned char *)&dwValue, dwSize);
	} else {
		if (m_prev_batt_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("BatteryTimeout"), NULL, REG_DWORD, (unsigned char *)&m_prev_batt_bl, dwSize);
		}
		if (m_prev_ac_bl) {
			dwSize = 4;
			RegSetValueEx(hKey, _T("ACTimeout"), NULL, REG_DWORD,(unsigned char *)&m_prev_ac_bl, dwSize);
		}
	}
	RegCloseKey(hKey);
	hBL = CreateEvent(NULL, FALSE, FALSE, _T("BackLightChangeEvent"));
	if (hBL) {
		SetEvent(hBL);
		CloseHandle(hBL);
	}
}

void COsmo4::OnShortcuts()
{
	ShowTaskBar(1);

	MessageBox(NULL,
	           _T("Double Click: Fullscreen on/off\n"),

	           _T("Osmo4 Shortcuts"),
	           MB_OK);

	ShowTaskBar(0);
}

