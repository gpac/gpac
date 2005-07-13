// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Osmo4.h"

#include <gpac/options.h>

#include "MainFrm.h"
#include <gx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CChildView::CChildView()
{
}

CChildView::~CChildView()
{
	/*since the wndproc is overwritten by the terminal, we detach the handle otherwise we get a nice assertion
	failure from windows*/
	HWND hWnd = Detach();
	::PostMessage(hWnd, WM_QUIT, 0, 0);
}


BEGIN_MESSAGE_MAP(CChildView,CWnd )
	//{{AFX_MSG_MAP(CChildView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		NULL, HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_APP_EXIT, OnAppExit)
	ON_MESSAGE(WM_OPENURL, Open)
	ON_MESSAGE(WM_SETTINGCHANGE, OnSIPChange)
	ON_MESSAGE(WM_SETSIZE,OnSetSize)
	ON_MESSAGE(WM_NAVIGATE,OnNavigate)
	ON_WM_SIZE()
	ON_COMMAND(ID_FILE_STEP, OnFileStep)
	ON_UPDATE_COMMAND_UI(ID_FILE_STEP, OnUpdateFileStep)
	ON_COMMAND(ID_FILE_PAUSE, OnFilePause)
	ON_UPDATE_COMMAND_UI(ID_FILE_PAUSE, OnUpdateFilePause)
	ON_COMMAND(ID_FILE_STOP, OnFileStop)
	ON_UPDATE_COMMAND_UI(ID_FILE_STOP, OnUpdateFileStop)
	ON_COMMAND(ID_VIEW_FULLSCREEN, OnViewFullscreen)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREEN, OnUpdateViewFullscreen)
	ON_WM_CLOSE()
	ON_COMMAND(ID_VIEW_FIT, OnViewFit)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIT, OnUpdateViewFit)
	ON_COMMAND(ID_VIEW_AR_ORIG, OnViewArOrig)
	ON_COMMAND(ID_VIEW_AR_FILL, OnViewArFill)
	ON_COMMAND(ID_VIEW_AR_43, OnViewAr43)
	ON_COMMAND(ID_VIEW_AR_169, OnViewAr169)
	ON_COMMAND(ID_NAV_NONE, OnNavNone)
	ON_COMMAND(ID_NAV_SLIDE, OnNavSlide)
	ON_COMMAND(ID_NAVE_RESET, OnNaveReset)
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_COMMAND(ID_VIEW_TIMING, OnViewTiming)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TIMING, OnUpdateViewTiming)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	GXOpenInput();
	m_is_stoped = 0;
	m_view_timing = 0;
}

CMainFrame::~CMainFrame()
{
	GXCloseInput();
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	COsmo4 *app = GetApp();

	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create a view to occupy the client area of the frame
	if (!m_wndView.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW | WS_BORDER,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create view window\n");
		return -1;
	}
	m_wndView.ShowWindow(SW_HIDE);

	if (!m_wndDumb.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW | WS_BORDER,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create dumb window\n");
		return -1;
	}
	m_wndDumb.SetWindowPos(this, 0, 0, app->m_screen_width, app->m_screen_height-app->m_menu_height, 0L);
	m_wndDumb.ShowWindow(SW_SHOW);

	if (!m_progBar.Create(IDD_CONTROL , this) ) {
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	m_progBar.UpdateWindow();
	m_progBar.SetWindowPos(this, 0, 0, app->m_screen_width, app->m_menu_height, 0L);
	m_progBar.ShowWindow(SW_SHOWNORMAL);
	

	m_wndCommandBar.m_bShowSharedNewButton = FALSE;

	if (!m_wndCommandBar.Create(this) 
			|| !m_wndCommandBar.InsertMenuBar(IDR_MENU)
			|| !m_wndCommandBar.AddAdornments()
			|| !m_wndCommandBar.LoadBitmap(IDR_MAINFRAME) 
		)
	{
		TRACE0("Failed to create CommandBar\n");
		return -1;      // fail to create
	}

	CToolBarCtrl & toolBar = m_wndCommandBar.GetToolBarCtrl();
	TBBUTTON tb;
	memset(&tb, 0, sizeof(tb));
    tb.idCommand = ID_OPEN_FILE; tb.iBitmap = 0; tb.fsStyle = TBSTYLE_BUTTON; toolBar.AddButtons(1, &tb);
    tb.idCommand = 0; tb.iBitmap = 0; tb.fsStyle = TBSTYLE_SEP; toolBar.AddButtons(1, &tb);
    tb.idCommand = ID_FILE_PAUSE; tb.iBitmap = 1; tb.fsStyle = TBSTYLE_BUTTON; toolBar.AddButtons(1, &tb);
    tb.idCommand = ID_FILE_STEP; tb.iBitmap = 2; tb.fsStyle = TBSTYLE_BUTTON; toolBar.AddButtons(1, &tb);
    tb.idCommand = ID_FILE_STOP; tb.iBitmap = 3; tb.fsStyle = TBSTYLE_BUTTON; toolBar.AddButtons(1, &tb);
    tb.idCommand = 0; tb.iBitmap = 0; tb.fsStyle = TBSTYLE_SEP; toolBar.AddButtons(1, &tb);
	
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
	SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);
	SetWindowPos(NULL, 0, 0, app->m_screen_width, app->m_screen_height, 0L);

	SetWindowText(_T("Osmo4"));	
	return 0;
}

void CMainFrame::SetPauseButton(Bool force_play_button)
{
	CToolBarCtrl & toolBar = m_wndCommandBar.GetToolBarCtrl();
	TBBUTTON tb;
	memset(&tb, 0, sizeof(tb));
    tb.idCommand = ID_FILE_PAUSE; tb.fsStyle = TBSTYLE_BUTTON;
	
	if (force_play_button || m_is_stoped || gf_term_get_option(GetApp()->m_term, GF_OPT_PLAY_STATE)==GF_STATE_PAUSED) {
		tb.iBitmap = 4;
	} else {
		tb.iBitmap = 1;
	}
	toolBar.DeleteButton(5);
	toolBar.InsertButton(5, &tb);
}


BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs


	cs.lpszClass = AfxRegisterWndClass(0);
	return TRUE;
}

 
/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
	// let the view have first crack at the command
	if (m_wndView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
		return TRUE;

	// otherwise, do default handling
	return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

#define PROGRESS_TIMER	20
#define PROGRESS_REFRESH_MS		500

void CALLBACK EXPORT ProgressTimer(HWND , UINT , UINT nID , DWORD )
{
	if (nID != PROGRESS_TIMER) return;
	((CMainFrame *) GetApp()->m_pMainWnd)->UpdateTime();
}

void CMainFrame::UpdateTime()
{
	u32 now;

	COsmo4 *app = GetApp();
	if (!app->m_open || m_is_stoped) return;
	now = gf_term_get_time_in_ms(app->m_term);
	if (!now) return;

	if (app->m_can_seek && (now>=app->max_duration + 100)) {
		if (gf_term_get_option(app->m_term, GF_OPT_IS_FINISHED)) {
			if (app->m_Loop && m_full_screen) {
				gf_term_play_from_time(app->m_term, 0);
			} else {
				OnFileStop();
				if (app->m_Loop) OnFilePause();
			}
			return;
		}
	} 

	if (!m_full_screen) m_progBar.SetPosition(now);
}

void CMainFrame::CloseURL()
{
	COsmo4 *app = GetApp();
	if (!app->m_open) return;
	if (m_full_screen) OnViewFullscreen();
	if (m_view_timing) KillTimer(PROGRESS_TIMER);
	gf_term_disconnect(app->m_term);
	app->m_open = 0;
	app->m_can_seek = 0;
	app->max_duration = (u32) -1;
	m_progBar.m_prev_time = 0;
	m_progBar.SetPosition(0);
}

void CMainFrame::OnAppExit() 
{
	CloseURL();
	PostMessage(WM_QUIT);
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	COsmo4 *app = GetApp();
	u32 disp_w, disp_h, c_w, c_h, x, y;

	if (m_full_screen) return;

	disp_w = app->m_screen_width;
	disp_h = app->m_screen_height;
	CFrameWnd::OnSize(nType, disp_w, disp_h);

	x = y = 0;
	disp_h -= app->m_menu_height;

	if (m_view_timing) {
		disp_h -= app->m_menu_height;
		y = app->m_menu_height;
		m_progBar.SetWindowPos(this, 0, 0, app->m_screen_width, app->m_menu_height, 0L);
		m_progBar.ShowWindow(SW_SHOWNORMAL);
		m_wndDumb.SetWindowPos(this, 0, app->m_menu_height, disp_w, disp_h, 0L);
	} else {
		m_wndDumb.SetWindowPos(this, 0, 0, disp_w, disp_h, 0L);
		m_progBar.ShowWindow(SW_HIDE);
	}

	if (m_view_timing)
		SetTimer(PROGRESS_TIMER, PROGRESS_REFRESH_MS, ProgressTimer);

	if (!app->m_scene_width || !app->m_scene_height) {
		if (m_view_timing) {
			m_wndView.SetWindowPos(this, 0, app->m_menu_height, disp_w, disp_h, SWP_SHOWWINDOW);
		} else {
			m_wndView.SetWindowPos(this, 0, 0, disp_w, disp_h, SWP_SHOWWINDOW);
		}
		return;
	}

	if (!app->m_fit_screen && (app->m_scene_width < disp_w) && (app->m_scene_height < disp_h)) {
		c_w = app->m_scene_width;
		c_h = app->m_scene_height;
		x = (disp_w - c_w) / 2;
		y = (disp_h - c_h) / 2;
	} else {
		c_w = disp_w;
		c_h = disp_h;
	}

	m_wndView.SetWindowPos(this, x, y, c_w, c_h, SWP_SHOWWINDOW);
	gf_term_set_size(GetApp()->m_term, c_w, c_h);
}

void CMainFrame::UpdateFullScreenDisplay()
{
	COsmo4 *app = GetApp();
	u32 disp_w, disp_h, y;
	disp_w = app->m_screen_width;
	disp_h = app->m_screen_height;

	if (m_full_screen) {
		m_progBar.ShowWindow(SW_HIDE);
		m_wndCommandBar.ShowWindow(SW_HIDE);
		m_wndView.SetForegroundWindow();
		SHFullScreen(m_wndView /*::GetForegroundWindow()*/, SHFS_HIDESTARTICON | SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON);
		//m_wndDumb.SetWindowPos(this, 0, 0, disp_w, app->m_menu_height+disp_h, 0L);
		//m_wndDumb.UpdateWindow();
	} else {
		disp_h -= app->m_menu_height;
		y = 0;
		if (m_view_timing) {
			disp_h -= app->m_menu_height;
			y = app->m_menu_height;
		}
		m_progBar.ShowWindow(SW_SHOWNORMAL);
		m_wndCommandBar.ShowWindow(SW_SHOWNORMAL);
		m_wndView.SetForegroundWindow();
		SHFullScreen(m_wndView /*::GetForegroundWindow()*/, SHFS_HIDESTARTICON | SHFS_HIDETASKBAR | SHFS_SHOWSIPBUTTON);
		//m_wndDumb.SetWindowPos(this, 0, y, disp_w, disp_h, 0L);
		//m_wndDumb.UpdateWindow();
		//gf_term_refresh(app->m_term);
	}
}

void CMainFrame::OnViewFullscreen() 
{
	COsmo4 *gpac = GetApp();
	if (!gpac->m_open) return;
	
	m_full_screen = !m_full_screen;
	UpdateFullScreenDisplay();
	gf_term_set_option(gpac->m_term, GF_OPT_FULLSCREEN, m_full_screen);
}

void CMainFrame::OnUpdateViewFullscreen(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetApp()->m_open ? TRUE : FALSE);
}

LONG CMainFrame::OnSetSize(WPARAM wParam, LPARAM lParam)
{
	RECT rc;
	GetWindowRect(&rc);
	SetWindowPos(NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER | SWP_NOMOVE);
	//GetApp()->m_url_changed = 0;
	return 1;	
}

LONG CMainFrame::Open(WPARAM wParam, LPARAM lParam)
{
	COsmo4 *app = GetApp();
	CloseURL();
	char filename[5000];
	CE_WideToChar((LPTSTR) (LPCTSTR) app->m_filename, filename);
	m_is_stoped = 0;
	gf_term_connect(app->m_term, filename);
	app->SetBacklightState(1);
	return 1;	
}

LONG CMainFrame::OnNavigate(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	COsmo4 *app = GetApp();
	char to_url[MAX_PATH];
	CE_WideToChar((LPTSTR) (LPCTSTR) app->m_navigate_url, to_url);

	if (strstr(to_url, ".mp4")) {
		char _str[MAX_PATH];
		if (!strstr(to_url, "://") && !strstr(to_url, "/") && !strstr(to_url, "\\") ) {
			CString _last = app->m_filename;
			int res = _last.ReverseFind('\\');
			_last.SetAt(res+1, 0);

			CE_WideToChar((LPTSTR) (LPCTSTR) _last, _str);
			strcat(_str, to_url);
		} else {
			strcpy(_str, to_url);
		}
		console_message = (LPCSTR) _str;
		console_err = GF_OK;
		PostMessage(WM_CONSOLEMSG);
		app->m_filename = _str;
		Open(0, 0);
	} else {
		SHELLEXECUTEINFO info;
		console_message = app->m_navigate_url;
		console_err = GF_OK;
		PostMessage(WM_CONSOLEMSG);
	
		memset(&info, 0, sizeof(SHELLEXECUTEINFO));
		info.cbSize = sizeof(SHELLEXECUTEINFO);
		info.lpVerb = L"open";
		info.fMask = SEE_MASK_NOCLOSEPROCESS;
		info.lpFile = L"iexplore";
		info.lpParameters = (LPCTSTR) app->m_navigate_url;
		info.nShow = SW_SHOWNORMAL;
		ShellExecuteEx(&info);
	}
	return 1;	
}

void CMainFrame::OnFilePause()
{
	COsmo4 *app = GetApp();
	if (m_is_stoped) {
		char filename[5000];
		CE_WideToChar((LPTSTR) (LPCTSTR) app->m_filename, filename);
		m_is_stoped = 0;
		gf_term_connect(app->m_term, filename);
		app->SetBacklightState(1);

		if (m_view_timing)
			SetTimer(PROGRESS_TIMER, PROGRESS_REFRESH_MS, ProgressTimer);

		SetPauseButton();
	} else {
		app->Pause();
	}
}
void CMainFrame::OnUpdateFilePause(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetApp()->m_open ? TRUE : FALSE);
}
void CMainFrame::OnFileStop()
{
	COsmo4 *app = GetApp();
	if (!app->m_open) return;
	if (m_full_screen) OnViewFullscreen();
	m_is_stoped = 1;
	if (m_view_timing) KillTimer(PROGRESS_TIMER);
	gf_term_disconnect(app->m_term);
	m_progBar.SetPosition(0);
	app->SetBacklightState(0);
	SetPauseButton();
}

void CMainFrame::OnUpdateFileStop(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetApp()->m_open ? TRUE : FALSE);
}

void CMainFrame::OnFileStep()
{
	COsmo4 *app = GetApp();
	gf_term_set_option(app->m_term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	app->SetBacklightState(0);
	SetPauseButton(1);
}
void CMainFrame::OnUpdateFileStep(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(GetApp()->m_open ? TRUE : FALSE);
}

void CMainFrame::OnClose() 
{
	PostMessage(WM_DESTROY);
}

LONG CMainFrame::OnSIPChange(WPARAM wParam, LPARAM lParam)
{
	if (wParam == SPI_SETSIPINFO) GetApp()->ShowTaskBar(0);
	return 1;
}

void CMainFrame::OnViewFit() 
{
	COsmo4 *app = GetApp();
	app->m_fit_screen = !app->m_fit_screen;
	if (app->m_open) OnSetSize(0, 0);
}

void CMainFrame::OnUpdateViewFit(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(GetApp()->m_fit_screen ? TRUE : FALSE);
}

void CMainFrame::OnViewArOrig() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_KEEP);	
}
void CMainFrame::OnViewArFill() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_FILL_SCREEN);	
}
void CMainFrame::OnViewAr43() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_4_3);	
}
void CMainFrame::OnViewAr169() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO, GF_ASPECT_RATIO_16_9);	
}

void CMainFrame::OnNavNone() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_NONE);
}

void CMainFrame::OnNavSlide() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_NAVIGATION, GF_NAVIGATE_SLIDE);
}

void CMainFrame::OnNaveReset() 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_NAVIGATION_TYPE, 0);
}

void CMainFrame::ForwardMessage()
{
	const MSG *msg = GetCurrentMessage();
	m_wndView.SendMessage(msg->message, msg->wParam, msg->lParam);
}
void CMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	ForwardMessage(); 
}
void CMainFrame::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	ForwardMessage(); 
}


void CMainFrame::OnViewTiming() 
{
	if (m_full_screen) return;
	if (m_view_timing) KillTimer(PROGRESS_TIMER);
	m_view_timing = !m_view_timing;
	OnSetSize(0, 0);
}

void CMainFrame::OnUpdateViewTiming(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_view_timing ? TRUE : FALSE);
}
