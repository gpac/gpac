// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Osmo4.h"

#include <gpac/options.h>
#include <gpac/network.h>

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
	ON_WM_SETFOCUS()
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
	ON_COMMAND(ID_NAV_RESET, OnNaveReset)
	ON_COMMAND_RANGE(ID_NAV_NONE, ID_NAV_EXAMINE, OnSetNavigation)
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_COMMAND(ID_VIEW_TIMING, OnViewTiming)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TIMING, OnUpdateViewTiming)
	ON_WM_INITMENUPOPUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	GXOpenInput();
	m_view_timing = 0;
	m_restore_fs = 0;
}

CMainFrame::~CMainFrame()
{
	GXCloseInput();
}

void CMainFrame::OnSetFocus(CWnd* pOldWnd)
{
	if (m_restore_fs) {
		m_restore_fs = 0;
		GetApp()->ShowTaskBar(0);
		OnViewFullscreen();
	}
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

	
	if (!m_dumbWnd.Create(NULL, NULL, AFX_WS_DEFAULT_VIEW | WS_BORDER,
		CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST, NULL))
	{
		TRACE0("Failed to create dumb window\n");
		return -1;
	}
	m_dumbWnd.SetWindowPos(this, 0, 0, app->m_screen_width, app->m_screen_height-app->m_menu_height, 0L);
	m_dumbWnd.ShowWindow(SW_HIDE);

	if (!m_progBar.Create(IDD_CONTROL , this) ) {
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	m_progBar.UpdateWindow();
	m_progBar.SetWindowPos(this, 0, 0, app->m_screen_width, app->m_menu_height, 0L);
	m_progBar.ShowWindow(SW_SHOWNORMAL);
	

//	m_wndCommandBar.m_bShowSharedNewButton = FALSE;

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
	
	if (force_play_button || GetApp()->m_stoped || gf_term_get_option(GetApp()->m_term, GF_OPT_PLAY_STATE)==GF_STATE_PAUSED) {
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

/*
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
*/

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
	if (!app->m_open || app->m_stoped) return;
	now = gf_term_get_time_in_ms(app->m_term);
	if (!now) return;

	if (app->m_can_seek && (now>=app->m_duration + 100)) {
		if (gf_term_get_option(app->m_term, GF_OPT_IS_FINISHED)) {
			if (app->m_Loop && m_full_screen) {
				gf_term_play_from_time(app->m_term, 0, 0);
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
	if (m_view_timing) KillTimer(PROGRESS_TIMER);
	gf_term_disconnect(app->m_term);
	app->m_open = 0;
	app->m_can_seek = 0;
	app->m_duration = (u32) -1;
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
	} else {
		m_progBar.ShowWindow(SW_HIDE);
	}
	m_dumbWnd.SetWindowPos(this, 0, y, disp_w, disp_h, 0L);
	m_dumbWnd.ShowWindow(SW_SHOW);

	if (m_view_timing)
		SetTimer(PROGRESS_TIMER, PROGRESS_REFRESH_MS, ProgressTimer);

	if (!app->m_scene_width || !app->m_scene_height) {
		m_wndView.SetWindowPos(this, 0, y, disp_w, disp_h, SWP_SHOWWINDOW);
		gf_term_set_size(app->m_term, disp_w, disp_h);
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
	m_wndView.SetWindowPos(this, x, y, c_w, c_h, SWP_SHOWWINDOW | SWP_NOZORDER);
	gf_term_set_size(app->m_term, c_w, c_h);
}


void CMainFrame::OnViewFullscreen() 
{
	COsmo4 *app = GetApp();
	if (!app->m_open) return;
	u32 disp_w = app->m_screen_width;
	u32 disp_h = app->m_screen_height;
	
	Bool is_full_screen = !m_full_screen;

	/*prevent resize messages*/
	m_full_screen = 1;

	HWND hWnd = GetSafeHwnd();
	::SetForegroundWindow(hWnd);
	::CommandBar_Show(m_wndCommandBar.GetSafeHwnd(), is_full_screen ? FALSE : TRUE);
	SHFullScreen(hWnd, SHFS_HIDESTARTICON | SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON);

	if (is_full_screen) {
		m_dumbWnd.ShowWindow(SW_HIDE);

		::MoveWindow(m_hWnd, 0, 0, disp_w, disp_h, 0);
		m_wndView.GetWindowRect(&m_view_rc);
		m_wndView.SetWindowPos(this, 0, 0, disp_w, disp_h, SWP_NOZORDER);
		gf_term_set_option(app->m_term, GF_OPT_FULLSCREEN, is_full_screen);
		m_full_screen = 1;
	} else {
		gf_term_set_option(app->m_term, GF_OPT_FULLSCREEN, is_full_screen);
		m_full_screen = 0;
		OnSetSize(0,0);
		m_dumbWnd.ShowWindow(SW_SHOW);
		gf_term_set_option(app->m_term, GF_OPT_REFRESH, 0);
	}
}


void CMainFrame::OnUpdateViewFullscreen(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetApp()->m_open ? TRUE : FALSE);
}

LONG CMainFrame::OnSetSize(WPARAM wParam, LPARAM lParam)
{
	RECT rc;
	if (m_full_screen) return 0;
	GetWindowRect(&rc);
	SetWindowPos(NULL, 0, 0, rc.right-rc.left, rc.bottom-rc.top, SWP_NOZORDER | SWP_NOMOVE);
	return 1;	
}

LONG CMainFrame::Open(WPARAM wParam, LPARAM lParam)
{
	COsmo4 *app = GetApp();
	CloseURL();
	char filename[5000];
	CE_WideToChar((u16 *) (LPCTSTR) app->m_filename, filename);
	app->m_stoped = 0;
	
	if (app->m_reconnect_time) {
		gf_term_connect_from_time(app->m_term, filename, app->m_reconnect_time, 0);
		app->m_reconnect_time = 0;
	} else {
		gf_term_connect(app->m_term, filename);
	}
	app->SetBacklightState(1);
	return 1;	
}




LONG CMainFrame::OnNavigate(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	COsmo4 *app = GetApp();
	char to_url[MAX_PATH];
	CE_WideToChar((u16 *) (LPCTSTR) app->m_navigate_url, to_url);

	if (gf_term_is_supported_url(app->m_term, to_url, 1, app->m_no_mime_fetch)) {
		char fileName[MAX_PATH];
		TCHAR w_to_url[MAX_PATH];
		CE_WideToChar((u16 *) (LPCTSTR) app->m_filename, fileName);
		char *str = gf_url_concatenate(fileName, to_url);
		if (!str) str = strdup(to_url);
		CE_CharToWide(str, (u16 *)w_to_url);
		free(str);
		app->m_filename = w_to_url;
		Open(0, 0);
	} else {
		SHELLEXECUTEINFO info;
		console_message = app->m_navigate_url;
		console_err = GF_OK;
		PostMessage(WM_CONSOLEMSG);

		
		if (m_full_screen) {
			OnViewFullscreen();
			app->ShowTaskBar(1);
			m_restore_fs = 1;
		}
	
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
	if (app->m_stoped) {
		char filename[5000];
		CE_WideToChar((u16 *) (LPCTSTR) app->m_filename, filename);
		app->m_stoped = 0;
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
	COsmo4 *app = GetApp();
	pCmdUI->Enable((app->m_open || app->m_stoped) ? TRUE : FALSE);
}
void CMainFrame::OnFileStop()
{
	COsmo4 *app = GetApp();
	if (!app->m_open) return;
	if (m_full_screen) OnViewFullscreen();
	app->m_stoped = 1;
	if (m_view_timing) KillTimer(PROGRESS_TIMER);
	gf_term_disconnect(app->m_term);
	m_progBar.SetPosition(0);
	app->SetBacklightState(0);
	SetPauseButton();
}

void CMainFrame::OnUpdateFileStop(CCmdUI* pCmdUI)
{
	pCmdUI->Enable( GetApp()->m_open  ? TRUE : FALSE);
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

void CMainFrame::OnSetNavigation(UINT nID) 
{
	gf_term_set_option(GetApp()->m_term, GF_OPT_NAVIGATION, nID - ID_NAV_NONE);
}


void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) 
{
	COsmo4 *app = GetApp();
	CFrameWnd::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	u32 opt = gf_term_get_option(GetApp()->m_term, GF_OPT_ASPECT_RATIO);	
	CheckMenuItem(pPopupMenu->m_hMenu, ID_VIEW_AR_ORIG, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_KEEP) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(pPopupMenu->m_hMenu, ID_VIEW_AR_FILL, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_FILL_SCREEN) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(pPopupMenu->m_hMenu, ID_VIEW_AR_43, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_4_3) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(pPopupMenu->m_hMenu, ID_VIEW_AR_169, MF_BYCOMMAND| (opt==GF_ASPECT_RATIO_16_9) ? MF_CHECKED : MF_UNCHECKED);
	
	CheckMenuItem(pPopupMenu->m_hMenu, ID_VIEW_FIT, MF_BYCOMMAND| app->m_fit_screen ? MF_CHECKED : MF_UNCHECKED);

	u32 type;
	if (!app->m_open) type = GF_NAVIGATE_TYPE_NONE;
	else type = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION_TYPE);

	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_NONE, MF_BYCOMMAND | ((type==GF_NAVIGATE_TYPE_NONE) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_SLIDE, MF_BYCOMMAND | ((type==GF_NAVIGATE_TYPE_NONE) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_RESET, MF_BYCOMMAND | ((type==GF_NAVIGATE_TYPE_NONE) ? MF_GRAYED : MF_ENABLED) );

	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_WALK, MF_BYCOMMAND | ( (type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_FLY, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_EXAMINE, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_COLLIDE_OFF, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_COLLIDE_REG, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_COLLIDE_DISP, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );
	EnableMenuItem(pPopupMenu->m_hMenu, ID_NAV_GRAVITY, MF_BYCOMMAND | ((type!=GF_NAVIGATE_TYPE_3D) ? MF_GRAYED : MF_ENABLED) );

	if (type==GF_NAVIGATE_TYPE_NONE) {
		u32 mode = gf_term_get_option(app->m_term, GF_OPT_NAVIGATION);
		CheckMenuItem(pPopupMenu->m_hMenu, ID_NAV_NONE, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_NONE) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem(pPopupMenu->m_hMenu, ID_NAV_SLIDE, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_SLIDE) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem(pPopupMenu->m_hMenu, ID_NAV_WALK, MF_BYCOMMAND | ( (mode==GF_NAVIGATE_WALK) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem(pPopupMenu->m_hMenu, ID_NAV_FLY, MF_BYCOMMAND | ((mode==GF_NAVIGATE_FLY) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem(pPopupMenu->m_hMenu, ID_NAV_EXAMINE, MF_BYCOMMAND | ((mode==GF_NAVIGATE_EXAMINE) ? MF_CHECKED : MF_UNCHECKED) );
	}
}
