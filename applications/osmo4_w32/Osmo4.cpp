// GPAC.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Osmo4.h"
#include <gpac/network.h>

#include "MainFrm.h"
#include "OpenUrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Osmo4

BEGIN_MESSAGE_MAP(Osmo4, CWinApp)
	//{{AFX_MSG_MAP(Osmo4)
	ON_COMMAND(ID_FILEOPEN, OnOpenFile)
	ON_COMMAND(ID_FILE_STEP, OnFileStep)
	ON_COMMAND(ID_OPEN_URL, OnOpenUrl)
	ON_COMMAND(ID_FILE_RELOAD, OnFileReload)
	ON_COMMAND(ID_CONFIG_RELOAD, OnConfigReload)
	ON_COMMAND(ID_FILE_PLAY, OnFilePlay)
	ON_UPDATE_COMMAND_UI(ID_FILE_PLAY, OnUpdateFilePlay)
	ON_UPDATE_COMMAND_UI(ID_FILE_STEP, OnUpdateFileStep)
	ON_COMMAND(ID_FILE_STOP, OnFileStop)
	ON_UPDATE_COMMAND_UI(ID_FILE_STOP, OnUpdateFileStop)
	ON_COMMAND(ID_SWITCH_RENDER, OnSwitchRender)
	ON_UPDATE_COMMAND_UI(ID_FILE_RELOAD, OnUpdateFileStop)
	ON_COMMAND(ID_H_ABOUT, OnAbout)
	ON_COMMAND(ID_FILE_MIGRATE, OnFileMigrate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Osmo4 construction

Osmo4::Osmo4()
{
}

/////////////////////////////////////////////////////////////////////////////
// The one and only Osmo4 object

Osmo4 theApp;



class UserPassDialog : public CDialog
{
// Construction
public:
	UserPassDialog(CWnd* pParent = NULL);   // standard constructor

	Bool GetPassword(const char *site_url, char *user, char *password);

// Dialog Data
	//{{AFX_DATA(UserPassDialog)
	enum { IDD = IDD_PASSWD };
	CStatic	m_SiteURL;
	CEdit m_User;
	CEdit m_Pass;
	//}}AFX_DATA

	void SetSelection(u32 sel);
	char cur_ext[200], cur_mime[200];

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(UserPassDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	const char *m_site_url;
	char *m_user, *m_password;

	// Generated message map functions
	//{{AFX_MSG(UserPassDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
	//}}AFX_MSG
};

UserPassDialog::UserPassDialog(CWnd* pParent /*=NULL*/)
	: CDialog(UserPassDialog::IDD, pParent)
{
	//{{AFX_DATA_INIT(COptStream)
	//}}AFX_DATA_INIT
}

void UserPassDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(UserPassDialog)
	DDX_Control(pDX, IDC_TXT_SITE, m_SiteURL);
	DDX_Control(pDX, IDC_EDIT_USER, m_User);
	DDX_Control(pDX, IDC_EDIT_PASSWORD, m_Pass);
	//}}AFX_DATA_MAP
}

BOOL UserPassDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_SiteURL.SetWindowText(m_site_url);
	m_User.SetWindowText(m_user);
	m_Pass.SetWindowText("");
	return TRUE;
}

void UserPassDialog::OnClose()
{
	m_User.GetWindowText(m_user, 50);
	m_Pass.GetWindowText(m_password, 50);
}

Bool UserPassDialog::GetPassword(const char *site_url, char *user, char *password)
{
	m_site_url = site_url;
	m_user = user;
	if (DoModal() != IDOK) return 0;
	return 1;
}


u32 get_sys_col(int idx)
{
	u32 res;
	DWORD val = GetSysColor(idx);
	res = (val)&0xFF; res<<=8;
	res |= (val>>8)&0xFF; res<<=8;
	res |= (val>>16)&0xFF;
	return res;
}

static void Osmo4_progress_cbk(void *usr, char *title, u32 done, u32 total)
{
	if (!total) return;
	CMainFrame *pFrame = (CMainFrame *) ((Osmo4 *) usr)->m_pMainWnd;
	s32 prog = (s32) ( (100 * (u64)done) / total);
	if (pFrame->m_last_prog < prog) {
		pFrame->console_err = GF_OK;
		pFrame->m_last_prog = prog;
		pFrame->console_message.Format("%s %02d %%", title, prog);
		pFrame->PostMessage(WM_CONSOLEMSG, 0, 0);
		if (done==total) pFrame->m_last_prog = -1;
	}
}

#define W32_MIN_WIDTH 120

static void log_msg(char *msg)
{
	::MessageBox(NULL, msg, "GPAC", MB_OK);
}
Bool Osmo4_EventProc(void *priv, GF_Event *evt)
{
	u32 dur;
	Osmo4 *gpac = (Osmo4 *) priv;
	CMainFrame *pFrame = (CMainFrame *) gpac->m_pMainWnd;
	/*shutdown*/
	if (!pFrame) return 0;

	switch (evt->type) {
	case GF_EVENT_DURATION:
		dur = (u32) (1000 * evt->duration.duration);
		//if (dur<1100) dur = 0;
		pFrame->m_pPlayList->SetDuration((u32) evt->duration.duration );
		gpac->max_duration = dur;
		gpac->can_seek = evt->duration.can_seek;
		if (!gpac->can_seek) {
			pFrame->m_Sliders.m_PosSlider.EnableWindow(FALSE);
		} else {
			pFrame->m_Sliders.m_PosSlider.EnableWindow(TRUE);
			pFrame->m_Sliders.m_PosSlider.SetRangeMin(0);
			pFrame->m_Sliders.m_PosSlider.SetRangeMax(dur);
		}
		break;

	case GF_EVENT_MESSAGE:
		if (!evt->message.service || !strcmp(evt->message.service, (LPCSTR) pFrame->m_pPlayList->GetURL() )) {
			pFrame->console_service = "main service";
		} else {
			pFrame->console_service = evt->message.service;
		}
		if (evt->message.error!=GF_OK) {
			if (evt->message.error<GF_OK || !gpac->m_NoConsole) {
				pFrame->console_err = evt->message.error;
				pFrame->console_message = evt->message.message;
				gpac->m_pMainWnd->PostMessage(WM_CONSOLEMSG, 0, 0);

				/*any error before connection confirm is a service connection error*/
				if (!gpac->m_isopen) pFrame->m_pPlayList->SetDead();
			}
			return 0;
		}
		if (gpac->m_NoConsole) return 0;

		/*process user message*/
		pFrame->console_err = GF_OK;
		pFrame->console_message = evt->message.message;
		gpac->m_pMainWnd->PostMessage(WM_CONSOLEMSG, 0, 0);
		break;
	case GF_EVENT_PROGRESS:
		char *szType;
		if (evt->progress.progress_type==0) szType = "Buffer ";
		else if (evt->progress.progress_type==1) szType = "Download ";
		else if (evt->progress.progress_type==2) szType = "Import ";
		gf_set_progress(szType, evt->progress.done, evt->progress.total);
		break;
	case GF_EVENT_NAVIGATE_INFO:
		pFrame->console_message = evt->navigate.to_url;
		gpac->m_pMainWnd->PostMessage(WM_CONSOLEMSG, 1000, 0);
		break;

	case GF_EVENT_SCENE_SIZE:
		if (evt->size.width && evt->size.height) {
			gpac->orig_width = evt->size.width;
			gpac->orig_height = evt->size.height;
			if (gpac->m_term && !pFrame->m_bFullScreen) 
				pFrame->PostMessage(WM_SETSIZE, evt->size.width, evt->size.height);
		}
		break;
	/*don't resize on win32 msg notif*/
	case GF_EVENT_SIZE:
		if (gpac->m_term && !pFrame->m_bFullScreen && gpac->orig_width && (evt->size.width < W32_MIN_WIDTH) ) 
			pFrame->PostMessage(WM_SETSIZE, W32_MIN_WIDTH, (W32_MIN_WIDTH*gpac->orig_height) / gpac->orig_width);
		break;

	case GF_EVENT_CONNECT:
		if (pFrame->m_bStartupFile) return 0;

		pFrame->BuildStreamList(1);
		if (evt->connect.is_connected) {
			pFrame->BuildChapterList(0);
			gpac->m_isopen = 1;
		} else {
			gpac->max_duration = 0;
			gpac->m_isopen = 0;
			pFrame->BuildChapterList(1);
		}
		pFrame->m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, gpac->m_isopen ? 4 : 3);
		pFrame->m_Sliders.m_PosSlider.SetPos(0);
		pFrame->SetProgTimer(1);
		if (!pFrame->m_bFullScreen) {
			pFrame->SetFocus();
			pFrame->SetForegroundWindow();
		}
		break;

	case GF_EVENT_QUIT:
		pFrame->PostMessage(WM_CLOSE, 0L, 0L);
		break;
	case GF_EVENT_MIGRATE:
	{
		const char *str = gf_cfg_get_key(gpac->m_user.config, "Network", "SessionMigration");
		if (!str || !strcmp(str, "no"))
			gf_cfg_set_key(gpac->m_user.config, "Network", "SessionMigration", "yes");

		gpac->m_navigate_url = "";
		pFrame->PostMessage(WM_NAVIGATE, NULL, NULL);
	}
		break;
	case GF_EVENT_KEYDOWN:
		if (gpac->can_seek && evt->key.flags & GF_KEY_MOD_ALT) {
			s32 res;
			switch (evt->key.key_code) {
			case GF_KEY_LEFT:
				res = gf_term_get_time_in_ms(gpac->m_term) - 5*gpac->max_duration/100;
				if (res<0) res=0;
				gpac->PlayFromTime(res);
				break;
			case GF_KEY_RIGHT:
				res = gf_term_get_time_in_ms(gpac->m_term) + 5*gpac->max_duration/100;
				if ((u32) res>=gpac->max_duration) res = 0;
				gpac->PlayFromTime(res);
				break;
			case GF_KEY_DOWN:
				res = gf_term_get_time_in_ms(gpac->m_term) - 60000;
				if (res<0) res=0;
				gpac->PlayFromTime(res);
				break;
			case GF_KEY_UP:
				res = gf_term_get_time_in_ms(gpac->m_term) + 60000;
				if ((u32) res>=gpac->max_duration) res = 0;
				gpac->PlayFromTime(res);
				break;
			}
		} else if (evt->key.flags & GF_KEY_MOD_CTRL) {
			switch (evt->key.key_code) {
			case GF_KEY_LEFT:
				pFrame->m_pPlayList->PlayPrev();
				break;
			case GF_KEY_RIGHT:
				pFrame->m_pPlayList->PlayNext();
				break;
			}
		} else {
			switch (evt->key.key_code) {
			case GF_KEY_HOME:
				gf_term_set_option(gpac->m_term, GF_OPT_NAVIGATION_TYPE, 1);
				break;
			case GF_KEY_ESCAPE:
				pFrame->PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
				break;
			}
		}
		break;
	case GF_EVENT_NAVIGATE:
		/*fixme - a proper browser would require checking mime type & co*/
		/*store URL since it may be destroyed, and post message*/
		gpac->m_navigate_url = evt->navigate.to_url;
		pFrame->PostMessage(WM_NAVIGATE, NULL, NULL);
		return 1;
	case GF_EVENT_VIEWPOINTS:
		pFrame->BuildViewList();
		return 0;
	case GF_EVENT_STREAMLIST:
		pFrame->BuildStreamList(0);
		return 0;
	case GF_EVENT_SET_CAPTION:
		pFrame->SetWindowText(evt->caption.caption);
		break;
	case GF_EVENT_DBLCLICK:
		pFrame->PostMessage(WM_COMMAND, ID_VIEW_FULLSCREEN);
		return 0;
	case GF_EVENT_AUTHORIZATION:
	{
		UserPassDialog passdlg;
		return passdlg.GetPassword(evt->auth.site_url, evt->auth.user, evt->auth.password);
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
		return 1;
	}
	return 0;
}


/*here's the trick: use a storage section shared among all processes for the wnd handle and for the command line
NOTE: this has to be static memory of course, don't try to alloc anything there...*/
#pragma comment(linker, "/SECTION:.shr,RWS") 
#pragma data_seg(".shr") 
HWND static_gpac_hwnd = NULL; 
char static_szCmdLine[MAX_PATH] = "";
#pragma data_seg() 

const char *static_gpac_get_url()
{
	return (const char *) static_szCmdLine;
}

static void osmo4_do_log(void *cbk, u32 level, u32 tool, const char *fmt, va_list list)
{
	FILE *logs = (FILE *) cbk;
    vfprintf(logs, fmt, list);
	fflush(logs);
}

BOOL Osmo4::InitInstance()
{
	CCommandLineInfo cmdInfo;

	m_logs = NULL;

	m_term = NULL;

	memset(&m_user, 0, sizeof(GF_User));

	strcpy((char *) szAppPath, AfxGetApp()->m_pszHelpFilePath);
	while (szAppPath[strlen((char *) szAppPath)-1] != '\\') szAppPath[strlen((char *) szAppPath)-1] = 0;
	if (szAppPath[strlen((char *) szAppPath)-1] != '\\') strcat(szAppPath, "\\");

	/*setup user*/
	memset(&m_user, 0, sizeof(GF_User));

	Bool first_launch = 0;
	/*init config and modules*/
	m_user.config = gf_cfg_new((const char *) szAppPath, "GPAC.cfg");
	if (!m_user.config) {
		first_launch = 1;
		/*create blank config file in the exe dir*/
		unsigned char config_file[MAX_PATH];
		strcpy((char *) config_file, (const char *) szAppPath);
		strcat((char *) config_file, "GPAC.cfg");
		FILE *ft = fopen((const char *) config_file, "wt");
		fclose(ft);
		m_user.config = gf_cfg_new((const char *) szAppPath, "GPAC.cfg");
		if (!m_user.config) {
			MessageBox(NULL, "GPAC Configuration file not found", "Fatal Error", MB_OK);
			m_pMainWnd->PostMessage(WM_CLOSE);
		}
	}
	const char *opt = gf_cfg_get_key(m_user.config, "General", "SingleInstance");
	m_SingleInstance = (opt && !stricmp(opt, "yes")) ? 1 : 0;

	m_hMutex = NULL;
	if (m_SingleInstance) {
		m_hMutex = CreateMutex(NULL, FALSE, "Osmo4_GPAC_INSTANCE");
		if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
			char szDIR[1024];
			if (m_hMutex) CloseHandle(m_hMutex);
			m_hMutex = NULL;

			if (!static_gpac_hwnd || !IsWindow(static_gpac_hwnd) ) {
				::MessageBox(NULL, "Osmo4 ghost process detected", "Error at last shutdown" , MB_OK);
			} else {
				::SetForegroundWindow(static_gpac_hwnd);

				if (m_lpCmdLine && strlen(m_lpCmdLine)) {
					DWORD res;
					u32 len;
					char *the_url, *cmd;
					GetCurrentDirectory(1024, szDIR);
					if (szDIR[strlen(szDIR)-1] != '\\') strcat(szDIR, "\\");
					cmd = (char *)(const char *) m_lpCmdLine;
					strcpy(static_szCmdLine, "");
					if (cmd[0]=='"') cmd+=1;

					if (!strnicmp(cmd, "-queue ", 7)) {
						strcat(static_szCmdLine, "-queue ");
						cmd += 7;
					}
					the_url = gf_url_concatenate(szDIR, cmd);
					if (!the_url) {
						strcat(static_szCmdLine, cmd);
					} else {
						strcat(static_szCmdLine, the_url);
						free(the_url);
					}
					while ( (len = strlen(static_szCmdLine)) ) {
						char s = static_szCmdLine[len-1];
						if ((s==' ') || (s=='"')) static_szCmdLine[len-1]=0;
						else break;
					}
					::SendMessageTimeout(static_gpac_hwnd, WM_NEWINSTANCE, 0, 0, 0, 1000, &res);
				}
			}
			
			return FALSE;
		}
	}

#if 0
	// Standard initialization
#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

#endif

	SetRegistryKey(_T("GPAC"));
	CMainFrame* pFrame = new CMainFrame;
	m_pMainWnd = pFrame;
	pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, NULL, NULL);
	m_pMainWnd->DragAcceptFiles();

	if (m_SingleInstance) static_gpac_hwnd = m_pMainWnd->m_hWnd;

	const char *str = gf_cfg_get_key(m_user.config, "General", "ModulesDirectory");
	m_user.modules = gf_modules_new(str, m_user.config);
	if (!m_user.modules) {
		const char *sOpt;
		/*inital launch*/
		m_user.modules = gf_modules_new(szAppPath, m_user.config);
		if (m_user.modules) {
			unsigned char str_path[MAX_PATH];
			gf_cfg_set_key(m_user.config, "General", "ModulesDirectory", (const char *) szAppPath);

			sOpt = gf_cfg_get_key(m_user.config, "Compositor", "Raster2D");
			if (!sOpt) gf_cfg_set_key(m_user.config, "Compositor", "Raster2D", "GPAC 2D Raster");

			sOpt = gf_cfg_get_key(m_user.config, "General", "CacheDirectory");
			if (!sOpt) {
				sprintf((char *) str_path, "%scache", szAppPath);
				gf_cfg_set_key(m_user.config, "General", "CacheDirectory", (const char *) str_path);
			}
			/*setup UDP traffic autodetect*/
			gf_cfg_set_key(m_user.config, "Network", "AutoReconfigUDP", "yes");
			gf_cfg_set_key(m_user.config, "Network", "UDPTimeout", "10000");
			gf_cfg_set_key(m_user.config, "Network", "BufferLength", "3000");

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

			sprintf((char *) str_path, "%sgpac.mp4", szAppPath);
			gf_cfg_set_key(m_user.config, "General", "StartupFile", (const char *) str_path);
		}

		/*check audio config on windows, force config*/
		sOpt = gf_cfg_get_key(m_user.config, "Audio", "ForceConfig");
		if (!sOpt) {
			gf_cfg_set_key(m_user.config, "Audio", "ForceConfig", "yes");
			gf_cfg_set_key(m_user.config, "Audio", "NumBuffers", "2");
			gf_cfg_set_key(m_user.config, "Audio", "TotalDuration", "120");
		}
		/*check video config */
		sOpt = gf_cfg_get_key(m_user.config, "Video", "UseHardwareMemory");
		if (!sOpt) gf_cfg_set_key(m_user.config, "Video", "UseHardwareMemory", "yes");

		/*by default use GDIplus, much faster than freetype on font loading*/
		gf_cfg_set_key(m_user.config, "FontEngine", "FontReader", "gdip_rend");

	}	
	if (! gf_modules_get_count(m_user.modules) ) {
		MessageBox(NULL, "No modules available - system cannot work", "Fatal Error", MB_OK);
		m_pMainWnd->PostMessage(WM_CLOSE);
	}

	/*setup font dir*/
	str = gf_cfg_get_key(m_user.config, "FontEngine", "FontDirectory");
	if (!str) {
		char szFtPath[MAX_PATH];
		::GetWindowsDirectory((char*)szFtPath, MAX_PATH);
		if (szFtPath[strlen((char*)szFtPath)-1] != '\\') strcat((char*)szFtPath, "\\");
		strcat((char *)szFtPath, "Fonts");
		gf_cfg_set_key(m_user.config, "FontEngine", "FontDirectory", (const char *) szFtPath);
	}

	/*check video driver, if none or raw_out use dx_hw by default*/
	str = gf_cfg_get_key(m_user.config, "Video", "DriverName");
	if (!str || !stricmp(str, "raw_out")) {
		gf_cfg_set_key(m_user.config, "Video", "DriverName", "dx_hw");
	}

	/*reset session migration*/
	str = gf_cfg_get_key(m_user.config, "Network", "SessionMigration");
	if (str && !strcmp(str, "yes"))
		gf_cfg_set_key(m_user.config, "Network", "SessionMigration", "no");

	/*check log file*/
	str = gf_cfg_get_key(m_user.config, "General", "LogFile");
	if (str) {
		m_logs = fopen(str, "wt");
		gf_log_set_callback(m_logs, osmo4_do_log);
	}
	else m_logs = NULL;

	/*set log level*/
	m_log_level = 0;
	str = gf_cfg_get_key(m_user.config, "General", "LogLevel");
	if (str) {
		if (!stricmp(str, "debug")) m_log_level = GF_LOG_DEBUG;
		else if (!stricmp(str, "info")) m_log_level = GF_LOG_INFO;
		else if (!stricmp(str, "warning")) m_log_level = GF_LOG_WARNING;
		else if (!stricmp(str, "error")) m_log_level = GF_LOG_ERROR;
		gf_log_set_level(m_log_level);
	}

	/*set log tools*/
	m_log_tools = 0;
	str = gf_cfg_get_key(m_user.config, "General", "LogTools");
	if (str) {
		char *sep;
		char *val = (char *) str;
		while (val) {
			sep = strchr(val, ':');
			if (sep) sep[0] = 0;
			if (!stricmp(val, "core")) m_log_tools |= GF_LOG_CODING;
			else if (!stricmp(val, "coding")) m_log_tools |= GF_LOG_CODING;
			else if (!stricmp(val, "container")) m_log_tools |= GF_LOG_CONTAINER;
			else if (!stricmp(val, "network")) m_log_tools |= GF_LOG_NETWORK;
			else if (!stricmp(val, "rtp")) m_log_tools |= GF_LOG_RTP;
			else if (!stricmp(val, "author")) m_log_tools |= GF_LOG_AUTHOR;
			else if (!stricmp(val, "sync")) m_log_tools |= GF_LOG_SYNC;
			else if (!stricmp(val, "codec")) m_log_tools |= GF_LOG_CODEC;
			else if (!stricmp(val, "parser")) m_log_tools |= GF_LOG_PARSER;
			else if (!stricmp(val, "media")) m_log_tools |= GF_LOG_MEDIA;
			else if (!stricmp(val, "scene")) m_log_tools |= GF_LOG_SCENE;
			else if (!stricmp(val, "script")) m_log_tools |= GF_LOG_SCRIPT;
			else if (!stricmp(val, "interact")) m_log_tools |= GF_LOG_INTERACT;
			else if (!stricmp(val, "compose")) m_log_tools |= GF_LOG_COMPOSE;
			else if (!stricmp(val, "mmio")) m_log_tools |= GF_LOG_MMIO;
			else if (!stricmp(val, "none")) m_log_tools = 0;
			else if (!stricmp(val, "all")) m_log_tools = 0xFFFFFFFF;
			if (!sep) break;
			sep[0] = ':';
			val = sep+1;
		}
		gf_log_set_tools(m_log_tools);
	}

	gf_sys_init();

	m_user.opaque = this;
	m_user.os_window_handler = pFrame->m_pWndView->m_hWnd;
	m_user.EventProc = Osmo4_EventProc;

	m_reset = 0;
	orig_width = 320;
	orig_height = 240;

	gf_set_progress_callback(this, Osmo4_progress_cbk);

	m_term = gf_term_new(&m_user);
	if (! m_term) {
		MessageBox(NULL, "Cannot load GPAC Terminal", "Fatal Error", MB_OK);
		m_pMainWnd->PostMessage(WM_CLOSE);
		return TRUE;
	}
	SetOptions();
	UpdateRenderSwitch();

	pFrame->SendMessage(WM_SETSIZE, orig_width, orig_height);
	pFrame->m_Address.ReloadURLs();

	pFrame->m_Sliders.SetVolume();

	m_reconnect_time = 0;


	ParseCommandLine(cmdInfo);

	start_mode = 0;

	if (! cmdInfo.m_strFileName.IsEmpty()) {
		pFrame->m_pPlayList->QueueURL(cmdInfo.m_strFileName);
		pFrame->m_pPlayList->RefreshList();
		pFrame->m_pPlayList->PlayNext();
	} else {
		char sPL[MAX_PATH];
		strcpy((char *) sPL, szAppPath);
		strcat(sPL, "gpac_pl.m3u");
		pFrame->m_pPlayList->OpenPlayList(sPL);
		const char *sOpt = gf_cfg_get_key(GetApp()->m_user.config, "General", "PLEntry");
		if (sOpt) {
			s32 count = (s32)gf_list_count(pFrame->m_pPlayList->m_entries);
			pFrame->m_pPlayList->m_cur_entry = atoi(sOpt);
			if (pFrame->m_pPlayList->m_cur_entry>=count)
				pFrame->m_pPlayList->m_cur_entry = count-1;
		} else {
			pFrame->m_pPlayList->m_cur_entry = -1;
		}
#if 0
		if (pFrame->m_pPlayList->m_cur_entry>=0) {
			start_mode = 1;
			pFrame->m_pPlayList->Play();
		}
#endif

		sOpt = gf_cfg_get_key(GetApp()->m_user.config, "General", "StartupFile");
		if (sOpt) gf_term_connect(m_term, sOpt);
	}
	pFrame->SetFocus();
	pFrame->SetForegroundWindow();
	return TRUE;
}

int Osmo4::ExitInstance() 
{
	if (m_term) gf_term_del(m_term);
	if (m_user.modules) gf_modules_del(m_user.modules);
	if (m_user.config) gf_cfg_del(m_user.config);
	gf_sys_close();
	/*last instance*/
	if (m_hMutex) {
		CloseHandle(m_hMutex);
		static_gpac_hwnd = NULL;
	}
	if (m_logs) fclose(m_logs);
	return CWinApp::ExitInstance();
}



/////////////////////////////////////////////////////////////////////////////
// Osmo4 message handlers





/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnGogpac();
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
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
	ON_BN_CLICKED(IDC_GOGPAC, OnGogpac)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void Osmo4::OnAbout() 
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

BOOL CAboutDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	CString str = "GPAC/Osmo4 - version " GPAC_FULL_VERSION;
	SetWindowText(str);
	return TRUE;  
}

void CAboutDlg::OnGogpac() 
{
	ShellExecute(NULL, "open", "http://gpac.sourceforge.net", NULL, NULL, SW_SHOWNORMAL);
}

/////////////////////////////////////////////////////////////////////////////
// Osmo4 message handlers


void Osmo4::SetOptions()
{
	const char *sOpt = gf_cfg_get_key(m_user.config, "General", "Loop");
	m_Loop = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "LookForSubtitles");
	m_LookForSubtitles = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "ConsoleOff");
	m_NoConsole = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "ViewXMT");
	m_ViewXMTA  = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	sOpt = gf_cfg_get_key(m_user.config, "General", "NoMIMETypeFetch");
	m_NoMimeFetch = (!sOpt || !stricmp(sOpt, "yes")) ? 1 : 0;
}


void Osmo4::OnOpenUrl() 
{
	COpenUrl url;
	if (url.DoModal() != IDOK) return;

	CMainFrame *pFrame = (CMainFrame *) m_pMainWnd;
	pFrame->m_pPlayList->Truncate();
	pFrame->m_pPlayList->QueueURL(url.m_url);
	pFrame->m_pPlayList->RefreshList();
	pFrame->m_pPlayList->PlayNext();
}


CString Osmo4::GetFileFilter()
{
	u32 keyCount, i;
	CString sFiles;
	CString sExts;
	CString supportedFiles;

	/*force MP4 and 3GP files at beginning to make sure they are selected (Win32 bug with too large filters)*/
	supportedFiles = "All Known Files|*.m3u;*.pls;*.mp4;*.3gp;*.3g2";

	sExts = "";
	sFiles = "";
	keyCount = gf_cfg_get_key_count(m_user.config, "MimeTypes");
	for (i=0; i<keyCount; i++) {
		const char *sMime;
		Bool first;
		char *sKey;
		const char *opt;
		char szKeyList[1000], sDesc[1000];
		sMime = gf_cfg_get_key_name(m_user.config, "MimeTypes", i);
		if (!sMime) continue;
		CString sOpt;
		opt = gf_cfg_get_key(m_user.config, "MimeTypes", sMime);
		/*remove module name*/
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

		/*if same description for # mime types skip (means an old mime syntax)*/
		if (sFiles.Find(sDesc)>=0) continue;
		/*if same extensions for # mime types skip (don't polluate the file list)*/
		if (sExts.Find(szKeyList)>=0) continue;

		sExts += szKeyList;
		sExts += " ";
		sFiles += sDesc;
		sFiles += "|";

		first = 1;

		sOpt = CString(szKeyList);
		while (1) {
			
			int pos = sOpt.Find(' ');
			CString ext = (pos==-1) ? sOpt : sOpt.Left(pos);
			/*WATCHOUT: we do have some "double" ext , eg .wrl.gz - these are NOT supported by windows*/
			if (ext.Find(".")<0) {
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
			rem.Format("%s ", (LPCTSTR) ext);
			sOpt.Replace((LPCTSTR) rem, "");
		}
		sFiles += "|";
	}
	supportedFiles += "|";
	supportedFiles += sFiles;
	supportedFiles += "M3U Playlists|*.m3u|ShoutCast Playlists|*.pls|All Files |*.*|";
	return supportedFiles;
}

void Osmo4::OnOpenFile() 
{
	CString sFiles = GetFileFilter();
	u32 nb_items;
	
	/*looks like there's a bug here, main filter isn't used correctly while the others are*/
	CFileDialog fd(TRUE,NULL,NULL, OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST , sFiles);
	fd.m_ofn.nMaxFile = 25000;
	fd.m_ofn.lpstrFile = (char *) malloc(sizeof(char) * fd.m_ofn.nMaxFile);
	fd.m_ofn.lpstrFile[0] = 0;

	if (fd.DoModal()!=IDOK) {
		free(fd.m_ofn.lpstrFile);
		return;
	}

	CMainFrame *pFrame = (CMainFrame *) m_pMainWnd;

	nb_items = 0;
	POSITION pos = fd.GetStartPosition();
	while (pos) {
		CString file = fd.GetNextPathName(pos);
		nb_items++;
	}
	/*if several items, act as playlist (replace playlist), otherwise as browser (lost all "next" context)*/
	if (nb_items==1) 
		pFrame->m_pPlayList->Truncate();
	else
		pFrame->m_pPlayList->Clear();

	pos = fd.GetStartPosition();
	while (pos) {
		CString file = fd.GetNextPathName(pos);
		pFrame->m_pPlayList->QueueURL(file);
	}
	free(fd.m_ofn.lpstrFile);
	pFrame->m_pPlayList->RefreshList();
	pFrame->m_pPlayList->PlayNext();
}


void Osmo4::Pause()
{
	if (!m_isopen) return;
	gf_term_set_option(m_term, GF_OPT_PLAY_STATE, (gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) ? GF_STATE_PAUSED : GF_STATE_PLAYING);
}

void Osmo4::OnMainPause() 
{
	Pause();	
}

void Osmo4::OnFileStep() 
{
	gf_term_set_option(m_term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	((CMainFrame *) m_pMainWnd)->m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, 3);
}
void Osmo4::OnUpdateFileStep(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_isopen && !m_reset);	
}

void Osmo4::PlayFromTime(u32 time)
{
	Bool do_pause;
	if (start_mode==1) do_pause = 1;
	else if (start_mode==2) do_pause = 0;
	else do_pause = /*!m_AutoPlay*/0;
	gf_term_play_from_time(m_term, time, do_pause);
	m_reset = 0;
}


void Osmo4::OnFileReload() 
{
	gf_term_disconnect(m_term);
	m_pMainWnd->PostMessage(WM_OPENURL);
}

void Osmo4::OnFileMigrate() 
{
	const char *str = gf_cfg_get_key(m_user.config, "Network", "SessionMigration");
	if (!str || !strcmp(str, "no"))
		gf_cfg_set_key(m_user.config, "Network", "SessionMigration", "yes");

	m_navigate_url = "";
	m_pMainWnd->PostMessage(WM_NAVIGATE, NULL, NULL);
}

void Osmo4::OnConfigReload() 
{
	gf_term_set_option(m_term, GF_OPT_RELOAD_CONFIG, 1); 
}

void Osmo4::UpdatePlayButton(Bool force_play)
{
	if (!force_play && gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
		((CMainFrame *) m_pMainWnd)->m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, 4);
	} else {
		((CMainFrame *) m_pMainWnd)->m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, 3);
	}
}

void Osmo4::OnFilePlay() 
{
	if (m_isopen) {
		if (m_reset) {
			m_reset = 0;
			PlayFromTime(0);
			((CMainFrame *)m_pMainWnd)->SetProgTimer(1);
		} else {
			Pause();
		}
		UpdatePlayButton();
	} else {
		((CMainFrame *) m_pMainWnd)->m_pPlayList->Play();
	}
}

void Osmo4::OnUpdateFilePlay(CCmdUI* pCmdUI) 
{
	if (m_isopen) {
		pCmdUI->Enable(TRUE);	
		if (pCmdUI->m_nID==ID_FILE_PLAY) {
			if (!m_isopen) {
				pCmdUI->SetText("Play/Pause\tCtrl+P");
			} else if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) {
				pCmdUI->SetText("Pause\tCtrl+P");
			} else {
				pCmdUI->SetText("Resume\tCtrl+P");
			}
		}
	} else {
		pCmdUI->Enable(((CMainFrame *)m_pMainWnd)->m_pPlayList->HasValidEntries() );	
		pCmdUI->SetText("Play\tCtrl+P");
	}
}

void Osmo4::OnFileStop() 
{
	CMainFrame *pFrame = (CMainFrame *) m_pMainWnd;
	if (m_reset) return;
	if (gf_term_get_option(m_term, GF_OPT_PLAY_STATE)==GF_STATE_PLAYING) Pause();
	m_reset = 1;
	pFrame->m_Sliders.m_PosSlider.SetPos(0);
	pFrame->SetProgTimer(0);
	pFrame->m_wndToolBar.SetButtonInfo(5, ID_FILE_PLAY, TBBS_BUTTON, 3);
	start_mode = 2;
}

void Osmo4::OnUpdateFileStop(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_isopen);	
}

void Osmo4::OnSwitchRender() 
{
	const char *opt = gf_cfg_get_key(m_user.config, "Compositor", "ForceOpenGL");
	Bool use_gl = (opt && !stricmp(opt, "yes")) ? 1 : 0;
	gf_cfg_set_key(m_user.config, "Compositor", "ForceOpenGL", use_gl ? "no" : "yes");

	gf_term_set_option(m_term, GF_OPT_USE_OPENGL, !use_gl);

	UpdateRenderSwitch();
}

void Osmo4::UpdateRenderSwitch()
{
	const char *opt = gf_cfg_get_key(m_user.config, "Compositor", "ForceOpenGL");
	if (opt && !stricmp(opt, "no"))
		((CMainFrame *) m_pMainWnd)->m_wndToolBar.SetButtonInfo(12, ID_SWITCH_RENDER, TBBS_BUTTON, 10);
	else
		((CMainFrame *) m_pMainWnd)->m_wndToolBar.SetButtonInfo(12, ID_SWITCH_RENDER, TBBS_BUTTON, 9);
}
